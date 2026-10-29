/* theseus: a DFAM-style percussion voice for the OP-1, built to be played by the meikyu sequencer (VOICE: DRM).
 * Like the DFAM, there is one voice and the sequence makes the kit: every note is a hit, and the note's pitch picks
 * the drum. Low notes come out as kicks (a sine with a deep pitch drop), the middle as toms, and high notes lean on
 * noise and FM into snares, hats and metal. Velocity is the accent: louder, brighter, and a harder pitch drop.
 *
 * Signal path per hit: VCO 1 (sine-ish, swept by the pitch envelope) frequency-modulates VCO 2 (an inharmonic
 * square-ish partner) -> mix with noise -> 2-pole state-variable filter opened by the hit (low-pass, crossfading to
 * its high-pass over the upper half of the zones, so hats and metal hiss instead of thud) -> decay envelope -> velocity.
 *
 *   blue   : DCY    decay, about 30 ms .. 1.5 s (higher drums decay faster on their own)
 *   green  : PNCH   pitch-envelope depth: from a flat thud to a deep, laser-ish drop
 *   white  : CUT    low-pass cutoff (the hit opens it further)
 *   orange : FM     VCO 1 -> VCO 2 modulation and VCO 2 level: from clean skins to clangs
 * Encoder press on blue / green / white opens page 2 (saved preset words 5 / 6 / 7):
 *   blue   : NOIS   noise level (added to the noise the high drums bring on their own)
 *   green  : LFO    depth of the hit LFO on cutoff, FM and pitch
 *   white  : RATE   hit LFO speed: one cycle every 32 .. 2 hits
 * The hit LFO steps once per note rather than with the clock, so it is locked to the sequence: at RATE max it
 * alternates every other hit, at RATE min it drifts across two bars of 1/16s. meikyu's MOD lane can drive the four
 * page-1 knobs too (DCY and CUT are the ones that make a pattern breathe).
 *
 * Audio rules (op1_plugin.h): integers only, no division in the sample loop, envelopes computed once per block
 * and ramped across it, the filter coefficient capped so it cannot run away, the voice in the firmware's 32 bytes.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "op1_plugin.h"

typedef struct {              /* one per voice: 0x24 bytes, the first word is the firmware's */
    uint32_t vtable;          /* firmware-owned: never write */
    uint32_t ph1, ph2;        /* oscillator phases */
    uint32_t rng;             /* noise */
    int32_t  low, band;       /* state-variable filter, Q15 */
    uint32_t age;             /* samples since the hit */
    int16_t  lfo;             /* the hit LFO's value when this hit started, Q15 */
    int16_t  zone;            /* 0 = kick .. 32767 = hat, from the pitch (set on the first block) */
} th_voice_t;

/* for the screen (theseus_screen.c, GUI thread): written by the audio thread, read without locks */
volatile uint32_t theseus_hits;                          /* note-ons */
volatile int16_t  theseus_zone;                          /* zone of the latest hit */
volatile int16_t  theseus_vel;                           /* its velocity gain, Q15 */
volatile int16_t  theseus_lfo;                           /* the hit LFO now, Q15 (-32767..32767) */
static uint32_t th_lfo_pos;

#define TH_STATE 60000
static inline int32_t th_clamp(int32_t v, int32_t lim) { return v > lim ? lim : (v < -lim ? -lim : v); }

/* e^(-x), x Q10 >= 0 -> Q15: 2^-(x log2 e), whole part by shift, fraction by a cubic. Once per block. */
static int32_t th_expneg(int32_t x)
{
    if (x <= 0) return 32768;
    int32_t y = (int32_t)(((uint32_t)x * 1477u) >> 10);
    int32_t n = y >> 10;
    if (n >= 15) return 0;
    int32_t t = (y & 1023) << 5, t2 = (t * t) >> 15, t3 = (t2 * t) >> 15;
    return (32768 - ((t * 22713) >> 15) + ((t2 * 7871) >> 15) - ((t3 * 1819) >> 15)) >> n;
}
static inline int32_t th_tri(uint32_t ph) { int32_t s = (int32_t)ph >> 16; return ((s < 0 ? -s : s) << 1) - 32768; }
static inline int32_t th_sine(uint32_t ph)               /* parabolic sine from a triangle, Q15 */
{
    int32_t t = th_tri(ph + 0x40000000u);                /* -32768..32767, peak at ph 0.25 */
    int32_t a = t < 0 ? -t : t;
    return (t * (49152 - (a >> 1))) >> 15;               /* t (1.5 - |t|/2): close to sin */
}

static void th_note_on(void *self, const int16_t *params)
{
    th_voice_t *v = (th_voice_t *)self;
    v->ph1 = 0x40000000u; v->ph2 = 0;                    /* start VCO 1 at its peak: a click-free thump */
    v->low = 0; v->band = 0; v->age = 0; v->zone = -1;
    v->rng = 0x9E3779B9u ^ ((uint32_t)op1_voice_index(self) * 0x85EBCA6Bu) ^ (theseus_hits * 0x2545F491u);
    /* the hit LFO: one triangle step per hit; RATE sets the step (32 .. 2 hits per cycle) */
    int32_t rate = params[7] < 0 ? 0 : params[7];
    uint32_t hits = 32u - (uint32_t)((rate * 30 + 16383) >> 15);
    th_lfo_pos += 0xFFFFFFFFu / hits;
    int32_t tri = th_tri(th_lfo_pos);
    v->lfo = (int16_t)th_clamp(tri, 32767);
    theseus_lfo = v->lfo;
    theseus_hits++;
}

static void th_render(void *self, int16_t *out, const int16_t *params)
{
    th_voice_t *v = (th_voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    uint32_t i0 = (uint32_t)inc[0];
    if (v->zone < 0) {                                   /* the drum this pitch makes, on a log scale: 4 octaves from 55 Hz */
        int32_t msb = 0; uint32_t t = i0;
        while (t > 1u) { t >>= 1; msb++; }               /* once per hit, so a plain loop is fine */
        int32_t lg = msb * 256 + (int32_t)(((msb >= 8 ? i0 >> (msb - 8) : i0 << (8 - msb))) & 255);   /* log2, Q8 */
        int32_t k = (lg - 5702) * 32;                    /* 5702 = log2(55 Hz inc) in Q8; 1024 Q8 (4 oct) -> 32767 */
        v->zone = (int16_t)(k < 0 ? 0 : k > 32767 ? 32767 : k);
        theseus_zone = v->zone; theseus_vel = (int16_t)op1_voice_vel_gain(self);
    }
    int32_t z = v->zone, vel = op1_voice_vel_gain(self);
    int32_t dcy = params[0] < 0 ? 0 : params[0], pnch = params[1] < 0 ? 0 : params[1];
    int32_t cut = params[2] < 0 ? 0 : params[2], fm = params[3] < 0 ? 0 : params[3];
    int32_t nois = params[5] < 0 ? 0 : params[5], ldep = params[6] < 0 ? 0 : params[6];
    int32_t lfo = (v->lfo * (ldep >> 1)) >> 14;          /* -32767..32767 scaled by depth */

    /* envelopes at the block's start and end (Q15), ramped across it */
    int32_t ta = 1323 + (dcy >> 6) * (((dcy >> 6) * 130) >> 9);   /* amp decay in samples: ~30 ms .. ~1.5 s, squared feel */
    ta = ((ta >> 4) * ((32768 - ((z * 5) >> 3)) >> 4)) >> 7;       /* high drums decay faster (to ~40 %) */
    if (ta < 600) ta = 600;
    int32_t tp = 530 + (((32767 - z) * 1300) >> 15);     /* pitch drop time: ~12 .. ~42 ms */
    uint32_t a0 = v->age, a1 = v->age + OP1_BLOCK;
    int32_t ea0 = th_expneg((int32_t)((a0 << 6) / (uint32_t)(ta >> 4))), ea1 = th_expneg((int32_t)((a1 << 6) / (uint32_t)(ta >> 4)));
    int32_t ep0 = th_expneg((int32_t)((a0 << 6) / (uint32_t)(tp >> 4))), ep1 = th_expneg((int32_t)((a1 << 6) / (uint32_t)(tp >> 4)));
    if (a0 < 32) ea0 = (ea0 * (int32_t)a0) >> 5;         /* 0.7 ms attack: no click */
    /* pitch drop: up to x7 at the strike with PNCH max (accents dig deeper), lower drums drop further */
    int32_t pdepth = (pnch >> 3) * (6 + (vel >> 13));                         /* Q12 extra ratio, accents deeper */
    pdepth = (pdepth * (40960 - z)) >> 15;
    if (pdepth > 24576) pdepth = 24576;                                       /* at most x7 at the strike */
    /* filter: base cutoff + the hit's envelope + velocity + the hit LFO */
    int32_t fbase = 300 + (((cut >> 6) * (cut >> 6)) >> 4) + (z >> 2) + (lfo >> 3);   /* Q15 coefficient; tracks the zone so a closed CUT still lets hats through */
    int32_t fhit = (cut >> 1) + (vel >> 2);                                   /* the hit opens it more as CUT rises */
    int32_t fmk = (fm >> 9) + (((lfo >> 6) * (fm >> 9)) >> 9);                /* 0..96, FM index */
    fmk = fmk < 0 ? 0 : (fmk > 96 ? 96 : fmk);                                /* 96 x 16384 x 1024 < 2^31 */
    int32_t v2lvl = 8192 + (fm >> 1) + (z >> 2);                              /* VCO 2 level, Q15 */
    int32_t nmix = th_clamp(((nois * 3) >> 2) + ((z * 7) >> 3) - 6000, 32767);
    if (nmix < 0) nmix = 0;
    int32_t damp = 22000 - (fm >> 3);                                         /* Q14: a little ring, more with FM */
    int32_t hpz = z > 16384 ? (z - 16384) * 2 : 0;                            /* upper zones blend in the high-pass: hats, metal */
    int32_t ea = ea0, eas = (ea1 - ea0) >> 7, ep = ep0, eps = (ep1 - ep0) >> 7;

    uint32_t ph1 = v->ph1, ph2 = v->ph2, rng = v->rng;
    int32_t low = v->low, band = v->band;
    for (int n = 0; n < OP1_BLOCK; n++) {
        uint32_t i = (uint32_t)inc[n];
        uint32_t ip = i + (i >> 12) * (uint32_t)((pdepth * ep) >> 15);       /* the pitch drop */
        ip += (uint32_t)(((int32_t)(i >> 12) * (lfo >> 8)) >> 1);           /* the LFO bends pitch a little (~1.5 %) */
        ph1 += ip;
        int32_t s1 = th_sine(ph1);
        uint32_t i2 = ip + (ip >> 1) - (ip >> 5);                             /* x 1.47: inharmonic partner */
        int32_t ic = (int32_t)((i2 > 0x10000000u ? 0x10000000u : i2) >> 14);  /* capped so the product fits */
        ph2 += i2 + (uint32_t)((s1 >> 5) * ((ic * fmk) >> 0));
        int32_t s2 = (int32_t)ph2 >> 16;                                      /* saw */
        s2 = s2 > 12000 ? 24000 - s2 : (s2 < -12000 ? -24000 - s2 : s2);       /* folded: square-ish, hollow */
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int32_t ns = (int32_t)rng >> 16;
        int32_t x = ((s1 * (32767 - nmix)) >> 15) + ((s2 * v2lvl) >> 15) + ((ns * nmix) >> 15);
        x = th_clamp(x, 32767);
        int32_t f = fbase + ((fhit * ea) >> 15);
        int32_t fcap = 29491 - (((damp - 2768) * 5764) >> 15);
        f = f < 200 ? 200 : (f > fcap ? fcap : f);
        low  = th_clamp(low + ((f * band) >> 15), TH_STATE);
        int32_t high = th_clamp(x - low - ((damp * band) >> 14), 65535);
        band = th_clamp(band + ((f * (high >> 1)) >> 14), TH_STATE);
        int32_t lo = th_clamp(low, 65535), hi = th_clamp(high, 65535);
        int32_t o = th_clamp(lo + ((((hi - lo) >> 1) * hpz) >> 14), 65535);
        int32_t y = th_clamp((o * ea) >> 15, 32767);
        y -= (((((y * y) >> 15) * y) >> 15) * 10923) >> 15;                   /* soft clip */
        y = (y * 3) >> 2;
        out[n] = op1_sat16((y * vel) >> 15);
        ea += eas; ep += eps;
        if (ea < 0) ea = 0;
        if (ep < 0) ep = 0;
    }
    v->ph1 = ph1; v->ph2 = ph2; v->rng = rng; v->low = low; v->band = band;
    v->age = a1 > 0x400000u ? 0x400000u : a1;            /* ~95 s: long past silent, keeps a << 6 in range */
}

/* blue / green / white press: page 2 (words 5 / 6 / 7). Orange stays page 1: it shares word 7 with white.
 * The tag after the engine object lets meikyu recognise theseus as the loaded engine (op1_sound_engine + compare) and
 * switch its synth bank to drum pads on its own. plugin.json's symbol is this object: the firmware reads the
 * op1_engine_t at its start and never looks past it. */
typedef struct { op1_engine_t engine; char tag[8]; } th_tagged_t;
const th_tagged_t ENGINE_theseus = { { th_note_on, th_render, 0,
                                       { OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_NONE } }, "THESEUS" };
_Static_assert(__builtin_offsetof(th_tagged_t, tag) == sizeof(op1_engine_t), "tag right after the engine");
_Static_assert(sizeof(th_voice_t) <= 0x24, "voice too big");
