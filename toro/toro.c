/* toro (Spanish, "bull"): a two-oscillator voice with a wavefolder and a resonant low-pass, built to pair with the meikyu sequencer.
 * Signal path per voice: osc 1 + osc 2 (saw -> triangle morph) + noise -> wavefolder -> DC blocker -> 2-pole state-variable
 * low-pass -> soft-clip drive -> velocity. The stock T2 envelope is applied after render, as usual.
 *
 *   blue   : SPREAD  osc 2 ratio 1.0 .. 1.5 (slow beating at the low end, a fifth at the top)
 *   green  : FOLD    wavefolder gain 1x .. 8x
 *   white  : CUTOFF  low-pass, squared response
 *   orange : RES     low-pass resonance
 * Encoder press on blue / green / white opens page 2 (saved preset words 5 / 6 / 7):
 *   blue   : SHAPE   saw .. triangle (both oscillators)
 *   green  : NOISE   noise into the folder
 *   white  : DRIV / APHX, split down the middle:
 *              0 .. 50 %   DRIVE  output soft clip 1x .. 4x (full drive at the middle)
 *              50 .. 100 % APHX   full drive, plus key-tracked chaos: every key gets its own fixed, random-looking
 *                                 offsets to the two oscillators' tuning, the cutoff, fold, shape and resonance.
 *                                 Low amounts keep it one sound with a little per-key detune and colour, the way an
 *                                 old poly drifts voice to voice; the top end retunes keys by semitones, throws
 *                                 the filter octaves and crushes each key at its own sample-and-hold rate.
 *              The offsets come from a hash of the note's position on the keyboard (from the pitch, interpolated
 *              between neighbouring keys so a glide or bend slides smoothly): the same key always sounds the
 *              same, and it needs no voice memory.
 * The four page-1 knobs are the meikyu MOD lane's targets: FOLD and CUTOFF are the Labyrinth-style ones.
 *
 * Audio rules (op1_plugin.h): integers only, no division in the sample loop, coefficients once per block and
 * ramped across it, state clamped so a resonant filter cannot run away.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "op1_plugin.h"

typedef struct {              /* one per voice: 0x24 bytes, the first word is the firmware's */
    uint32_t vtable;          /* firmware-owned: never write */
    uint32_t ph1, ph2;        /* oscillator phases, 2^32 = one cycle */
    int32_t  low, band;       /* state-variable filter, Q15 */
    uint32_t rng;             /* noise */
    int32_t  f_last;          /* cutoff coefficient of the last block, 0 = none yet (no sweep on a new note) */
    int32_t  dc_x, dc_y;      /* DC blocker after the folder: an asymmetric wave folds to an offset */
} mt_voice_t;                 /* 4 + 32 bytes: the whole voice */

#define MT_F_MIN   300        /* cutoff coefficient range, Q15 (0.009 .. 0.9: about 60 Hz .. 6.5 kHz, lower with little resonance) */
#define MT_F_MAX   29491
#define MT_STATE   60000      /* filter state clamp: keeps every product below 2^31 */

/* for the screen (toro_screen.c, GUI thread): counts only, written by the audio thread, read without locks */
volatile uint32_t toro_notes;                            /* note-ons */
volatile uint32_t toro_blocks;                           /* rendered voice blocks: changes while anything sounds */
volatile int16_t  toro_key;                              /* keyboard position of the latest note (APHX's key), semitones */

static inline int32_t mt_clamp(int32_t v, int32_t lim) { return v > lim ? lim : (v < -lim ? -lim : v); }
static inline int32_t ks_clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int32_t mt_cutoff(int32_t p)                      /* knob 0..32767 -> coefficient, squared for feel */
{
    int32_t sq = (p * p) >> 15;
    return MT_F_MIN + ((sq * (MT_F_MAX - MT_F_MIN)) >> 15);
}

/* 2^(x / 4096) as Q12, x in Q12 octaves (-4 .. +4 octaves): whole octaves by shift, the fraction by a quadratic
 * (within 0.3 %). Once per block. */
static int32_t mt_exp2(int32_t x)
{
    if (x > 4 * 4096) x = 4 * 4096; else if (x < -4 * 4096) x = -4 * 4096;
    int32_t n = x >> 12, f = x & 4095;                   /* floor and fraction */
    int32_t m = 4096 + ((f * (2689 + ((f * 1407) >> 12))) >> 12);   /* 1 + f (0.6565 + 0.3435 f) */
    return n >= 0 ? m << n : m >> -n;
}
/* key position of a phase increment, Q8 semitones (log2 by the top bit, the mantissa with a parabola correction) */
static int32_t mt_key_q8(uint32_t inc)
{
    if (inc < 2u) return 0;
    int32_t msb = 31;
    while (!(inc & 0x80000000u)) { inc <<= 1; msb--; }
    int32_t man = (int32_t)((inc >> 15) & 0xFFFF);     /* Q16 mantissa above 1.0 */
    int32_t frac = man + ((((man * 22713) >> 16) * (65536 - man)) >> 16);   /* log2(1 + m), within 0.005 octave */
    return (int32_t)((((int64_t)msb << 16) + frac) * 12 >> 8);
}
/* six fixed, signed per-key values (Q15) for key k */
static void mt_key_rand(int32_t k, int32_t *r)
{
    uint32_t h = (uint32_t)k * 2654435761u ^ 0x5BD1E995u;
    for (int i = 0; i < 6; i++) {
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        r[i] = (int32_t)(h >> 16) - 32768;
    }
}

static void mt_note_on(void *self, const int16_t *params)
{
    mt_voice_t *v = (mt_voice_t *)self; (void)params;
    v->ph1 = 0; v->ph2 = 0x40000000u;                    /* a quarter cycle apart: no cancelling at unison */
    v->low = 0; v->band = 0; v->f_last = 0; v->dc_x = 0; v->dc_y = 0;
    v->rng = 0x9E3779B9u ^ ((uint32_t)op1_voice_index(self) * 0x85EBCA6Bu);
    toro_notes++;
}

static inline int32_t mt_osc(uint32_t ph, int32_t shape)  /* saw -> triangle, Q15 */
{
    int32_t saw = (int32_t)ph >> 16;                     /* -32768..32767 */
    int32_t tri = ((saw < 0 ? -saw : saw) << 1) - 32768;
    return saw + (((tri - saw) * (shape >> 1)) >> 14);   /* |tri - saw| <= 2^16: keep the product < 2^31 */
}

static void mt_render(void *self, int16_t *out, const int16_t *params)
{
    mt_voice_t *v = (mt_voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    int32_t w7     = params[7] < 0 ? 0 : params[7];
    int32_t drv    = w7 >= 16384 ? 32767 : w7 * 2;     /* DRIV: the lower half of white's page 2 */
    int32_t aphx   = w7 > 16384 ? (w7 - 16384) * 2 : 0;  /* APHX: the upper half, 0..32766 */
    int32_t pf1 = params[1], pf3 = params[3], ps5 = params[5] < 0 ? 0 : params[5];
    int32_t det1 = 0, det2 = 0, fmul = 4096, hold = 1;
    if (!v->f_last) toro_key = (int16_t)(mt_key_q8((uint32_t)inc[0]) >> 8);   /* first block of a note: for the screen */
    if (aphx) {
        /* the keyboard position, and this key's six offsets blended with the next key's */
        int32_t kq = mt_key_q8((uint32_t)inc[0]), k = kq >> 8, kf = kq & 255, ra[6], rb[6], r[6];
        mt_key_rand(k, ra); mt_key_rand(k + 1, rb);
        for (int i = 0; i < 6; i++) r[i] = ra[i] + (((rb[i] - ra[i]) * kf) >> 8);
        int32_t a = aphx, a2 = (a * a) >> 15, a3 = (a2 * a) >> 15;           /* Q15 curves: gentle, then wild */
        /* tuning: up to 0.15 semitone at a quarter, 3 semitones at the top; each oscillator its own way */
        int32_t semis = ((a * 614) >> 15) + ((a3 * 11674) >> 15);            /* Q12 semitones: 0.15 a + 2.85 a^3 */
        det1 = mt_exp2(((r[0] * semis) >> 15) / 12) - 4096;                  /* Q12 ratio - 1 */
        det2 = mt_exp2(((r[1] * semis) >> 15) / 12) - 4096;
        /* cutoff: up to half an octave early on, three octaves at the top */
        fmul = mt_exp2((r[2] * (((a * 2048) >> 15) + ((a3 * 10240) >> 15))) >> 15);
        /* fold, shape, resonance: pushed by the key, more as APHX rises */
        pf1 = ks_clampi(pf1 + ((r[3] * (a2 >> 1)) >> 15) + ((r[3] < 0 ? -r[3] : r[3]) * a3 >> 16), 0, 32767);
        ps5 = ks_clampi(ps5 + ((r[4] * a2) >> 15), 0, 32767);
        pf3 = ks_clampi(pf3 + ((((r[5] < 0 ? -r[5] : r[5]) * a3) >> 15) * 3 >> 2), 0, 32767);
        /* the top fifth: each key holds every 1..8th sample, its own rate */
        if (a > 26214) hold = 1 + (int32_t)((((uint32_t)(r[5] + 32768) >> 13) * (uint32_t)(a - 26214)) >> 13);
        if (hold > 8) hold = 8;
    }
    int32_t spread = params[0] >> 1;                     /* 0..16383 = ratio 1.0 .. 1.5, Q15 above 1 */
    int32_t fold   = 32768 + pf1 * 7;                    /* 1x .. 8x, Q15 */
    int32_t damp   = 32768 - (pf3 * 30000 >> 15);        /* 2.0 (none) .. 0.17 (ringing), Q14 */
    /* Chamberlin stability: f^2 + 2 f damp must stay well below 4. The linear cap runs from 0.9 at the lowest
     * damping to 0.739 at the highest, under the exact bound everywhere on that range. */
    int32_t fcap   = MT_F_MAX - (((damp - 2768) * 5764) >> 15);
    int32_t f1     = (mt_cutoff(params[2]) * fmul) >> 12;
    if (f1 > fcap) f1 = fcap;
    if (f1 < MT_F_MIN) f1 = MT_F_MIN;
    int32_t shape  = ps5;
    int32_t noise  = (params[6] < 0 ? 0 : params[6]) >> 2;          /* up to 25 % of the mix */
    int32_t drive  = 32768 + drv * 3;                    /* 1x .. 4x */
    int32_t gain   = op1_voice_vel_gain(self);

    int32_t f0 = v->f_last ? v->f_last : f1;
    int32_t fstep = (f1 - f0) >> 7;                      /* ramp the cutoff over the 128-sample block */
    int32_t f = f0;
    uint32_t ph1 = v->ph1, ph2 = v->ph2, rng = v->rng;
    int32_t low = v->low, band = v->band, dcx = v->dc_x, dcy = v->dc_y;

    int32_t held = 0, cnt = 0;
    for (int n = 0; n < OP1_BLOCK; n++) {
        uint32_t i = (uint32_t)inc[n];
        uint32_t i1 = i + (uint32_t)((int32_t)(i >> 12) * det1), i2 = i + (uint32_t)((int32_t)(i >> 12) * det2);   /* APHX detune */
        ph1 += i1;
        ph2 += i2 + (i2 >> 15) * (uint32_t)spread;       /* i * ratio in 32-bit math */
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int32_t s = (mt_osc(ph1, shape) + mt_osc(ph2, shape)) >> 1;
        s += (((int32_t)rng >> 16) * noise) >> 15;
        /* wavefolder: gain, then reflect at +-32767; 8x needs at most 5 reflections */
        int32_t x = (s * (fold >> 4)) >> 11;             /* 32-bit: |s| < 2^15, fold >> 4 <= 2^14 */
        for (int k = 0; k < 5; k++) {
            if (x > 32767) x = 65534 - x;
            else if (x < -32767) x = -65534 - x;
            else break;
        }
        /* DC blocker, pole 0.998 (about 15 Hz); |dcy| <= 65535 keeps the product below 2^31 */
        dcy = mt_clamp(x - dcx + ((dcy * 32700) >> 15), 65535); dcx = x; x = mt_clamp(dcy, 32767);
        /* Chamberlin state-variable low-pass */
        low  = mt_clamp(low + ((f * band) >> 15), MT_STATE);
        int32_t high = mt_clamp(x - low - ((damp * band) >> 14), 65535);
        band = mt_clamp(band + ((f * (high >> 1)) >> 14), MT_STATE);
        f += fstep;
        /* drive and soft clip x - x^3/3 */
        int32_t y = mt_clamp((mt_clamp(low, 65535) * (drive >> 3)) >> 12, 32767);
        y -= (((((y * y) >> 15) * y) >> 15) * 10923) >> 15;
        y = (y * 3) >> 2;                                /* headroom for six voices and the FX */
        if (hold > 1) { if (cnt == 0) { held = y; cnt = hold; } cnt--; y = held; }   /* APHX top end: per-key sample and hold */
        out[n] = op1_sat16((y * gain) >> 15);
    }
    toro_blocks++;
    v->ph1 = ph1; v->ph2 = ph2; v->rng = rng; v->low = low; v->band = band; v->f_last = f1; v->dc_x = dcx; v->dc_y = dcy;
}

/* blue / green / white press: page 2 (words 5 / 6 / 7). Orange stays page 1: it shares word 7 with white.
 * The tag after the engine object lets meikyu recognise toro as the loaded engine (op1_sound_engine + compare): it
 * then names its MOD targets after toro's knobs and keeps the synth bank melodic. plugin.json's symbol is this
 * object; the firmware reads the op1_engine_t at its start and never looks past it. */
typedef struct { op1_engine_t engine; char tag[8]; } mt_tagged_t;
const mt_tagged_t ENGINE_toro = { { mt_note_on, mt_render, 0,
                                    { OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_NONE } }, "TORO" };
_Static_assert(__builtin_offsetof(mt_tagged_t, tag) == sizeof(op1_engine_t), "tag right after the engine");
_Static_assert(sizeof(mt_voice_t) <= 0x24, "voice too big");
