/* theseus for the OP-XY: a port of the OP-1 kennel plugin, the same integer DSP. A DFAM-style percussion voice:
 * one voice and the sequence makes the kit. Every note is a hit, and the note's pitch picks the drum: low notes
 * are kicks (a sine with a deep pitch drop), the middle toms and snare, high notes lean on noise and FM into hats
 * and metal.
 *
 * Signal path per hit: VCO 1 (sine-ish, swept by the pitch envelope) frequency-modulates VCO 2 (an inharmonic
 * square-ish partner) -> mix with noise -> 2-pole state-variable filter opened by the hit (low-pass, crossfading
 * to its high-pass over the upper half of the zones) -> decay envelope. The OP-XY's own envelope comes after; set
 * it to a fast attack and full sustain and theseus's decay shapes each hit.
 *
 * Page 1                                        Page 2 (press any encoder)
 *   Decay   30 ms .. 1.5 s (high drums shorter)   Noise  noise on top of what the upper zones bring
 *   Punch   pitch-envelope depth, up to x7         LFO    depth of the hit LFO on cutoff, FM and (a little) pitch
 *   Cutoff  low-pass; the hit opens it further     Rate   hit LFO speed: one cycle every 32 .. 2 hits
 *   FM      VCO 1 -> VCO 2 depth and VCO 2 level   Tune   shifts which drum a note picks, -24 .. +24 semitones
 *
 * The hit LFO steps once per note rather than with a clock, so it is locked to the sequence: at Rate 2 it
 * alternates every other hit, at 32 it drifts across two bars of 1/16s. Its phase lives in Shared.
 * Differences from the OP-1 build: 64-sample blocks, Q31 controls and output, no velocity (the XY's own velocity
 * reaches the track level after the engine), Tune in the slot the OP-1 did not have, and the per-block envelope
 * division replaced by a reciprocal (the Blackfin+ has no divide).
 * Copyright (c) 2026 briannhinton. MIT. */
#include "core.h"
#include <opxy/q31.hpp>

namespace theseus {
namespace {
using opxy::i32;
using opxy::u32;
const i32 STATE = 60000;
inline i32 clampv(i32 v, i32 lim) { return v > lim ? lim : (v < -lim ? -lim : v); }
inline i32 k15(i32 q31) { return q31 < 0 ? 0 : q31 >> 16; }
/* e^(-x), x Q10 >= 0 -> Q15 */
inline i32 expneg(i32 x)
{
    if (x <= 0) return 32768;
    const i32 y = static_cast<i32>((static_cast<u32>(x) * 1477u) >> 10), n = y >> 10;
    if (n >= 15) return 0;
    const i32 t = (y & 1023) << 5, t2 = (t * t) >> 15, t3 = (t2 * t) >> 15;
    return (32768 - ((t * 22713) >> 15) + ((t2 * 7871) >> 15) - ((t3 * 1819) >> 15)) >> n;
}
inline i32 tri(u32 ph) { const i32 s = static_cast<i32>(ph) >> 16; return ((s < 0 ? -s : s) << 1) - 32768; }
inline i32 sine(u32 ph) { const i32 t = tri(ph + 0x40000000u), a = t < 0 ? -t : t; return (t * (49152 - (a >> 1))) >> 15; }
/* 2^20 / d, once per block, for the two envelope rates. Two software divides per block (the Blackfin+ has no
 * divide) cost a few hundred cycles out of a block budget of tens of thousands; the sample loop has none. */
inline u32 recip20(u32 d) { return 1048576u / d; }
/* the LFO step for a Rate control: one cycle every 32 .. 2 hits, in ten choices like the definition's enum */
inline u32 lfo_step(i32 rate_q31)
{
    static const u32 hits[10] = { 32, 24, 18, 14, 10, 8, 6, 4, 3, 2 };
    const i32 k = rate_q31 < 0 ? 0 : static_cast<i32>((static_cast<u32>(rate_q31) >> 21) * 10 >> 10);
    return 0xFFFFFFFFu / hits[k > 9 ? 9 : k];
}
}

inline void initialize(Voice& v, Shared&, u32 owner)
{
    v.ph1 = 0x40000000u; v.ph2 = 0; v.low = 0; v.band = 0; v.age = 0; v.zone = -1; v.lfo = 0; v.serial = 0;
    v.rng = 0x9E3779B9u ^ (owner * 0x85EBCA6Bu);
}
inline void start(Voice& v, Shared& s, u32 owner)
{
    v.ph1 = 0x40000000u; v.ph2 = 0;                        /* VCO 1 at its peak: a click-free thump */
    v.low = 0; v.band = 0; v.age = 0; v.zone = -1;
    v.rng = 0x9E3779B9u ^ (owner * 0x85EBCA6Bu) ^ (s.hits * 0x2545F491u);
    s.hits++;
    v.serial = s.hits;
    v.lfo = 0;                                             /* the LFO steps on the first block, when Rate is known */
}
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared& s, u32, opxy::Output out, opxy::Parameters p, opxy::Pitch inc)
{
    const i32 dcy = k15(p[0]), pnch = k15(p[1]), cut = k15(p[2]), fm = k15(p[3]);
    const i32 nois = k15(p[4]), ldep = k15(p[5]);
    if (v.zone < 0) {                                      /* first block of a hit: the drum, and the LFO step */
        s.lfo_pos += lfo_step(p[6]);
        v.lfo = clampv(tri(s.lfo_pos), 32767);
        /* the drum this pitch makes, on a log scale: 4 octaves from 55 Hz, shifted by Tune */
        const u32 i0 = inc[0];
        i32 msb = 0; u32 t = i0;
        while (t > 1u) { t >>= 1; msb++; }
        const i32 lg = msb * 256 + static_cast<i32>((msb >= 8 ? i0 >> (msb - 8) : i0 << (8 - msb)) & 255);
        const i32 tune = (k15(p[7]) - 16384) / 32;                           /* Tune: -24 .. +24 semitones as Q8 octaves (512 = 2 octaves) */
        const i32 k = (lg - 5702 + tune) * 32;
        v.zone = k < 0 ? 0 : k > 32767 ? 32767 : k;
    }
    const i32 z = v.zone;
    const i32 lfo = (v.lfo * (ldep >> 1)) >> 14;
    const i32 vel = 32767;                                 /* the XY applies velocity after the engine */

    i32 ta = 1323 + (dcy >> 6) * (((dcy >> 6) * 130) >> 9);
    ta = ((ta >> 4) * ((32768 - ((z * 5) >> 3)) >> 4)) >> 7;
    if (ta < 600) ta = 600;
    const i32 tp = 530 + (((32767 - z) * 1300) >> 15);
    const u32 a0 = v.age, a1 = v.age + opxy::block_frames;
    const u32 ra = recip20(static_cast<u32>(ta >> 4)), rp = recip20(static_cast<u32>(tp >> 4));
    i32 ea0 = expneg(static_cast<i32>(((a0 >> 4) * ra) >> 10)), ea1 = expneg(static_cast<i32>(((a1 >> 4) * ra) >> 10));
    const i32 ep0 = expneg(static_cast<i32>(((a0 >> 4) * rp) >> 10)), ep1 = expneg(static_cast<i32>(((a1 >> 4) * rp) >> 10));
    if (a0 < 32) ea0 = (ea0 * static_cast<i32>(a0)) >> 5;
    i32 pdepth = (pnch >> 3) * (6 + (vel >> 13));
    pdepth = (pdepth * (40960 - z)) >> 15;
    if (pdepth > 24576) pdepth = 24576;
    const i32 fbase = 300 + (((cut >> 6) * (cut >> 6)) >> 4) + (z >> 2) + (lfo >> 3);
    const i32 fhit = (cut >> 1) + (vel >> 2);
    i32 fmk = (fm >> 9) + (((lfo >> 6) * (fm >> 9)) >> 9);
    fmk = fmk < 0 ? 0 : (fmk > 96 ? 96 : fmk);
    const i32 v2lvl = 8192 + (fm >> 1) + (z >> 2);
    i32 nmix = clampv(((nois * 3) >> 2) + ((z * 7) >> 3) - 6000, 32767);
    if (nmix < 0) nmix = 0;
    const i32 damp = 22000 - (fm >> 3);
    const i32 hpz = z > 16384 ? (z - 16384) * 2 : 0;
    const i32 fcap = 29491 - (((damp - 2768) * 5764) >> 15);
    i32 ea = ea0, ep = ep0;
    const i32 eas = (ea1 - ea0) >> 6, eps = (ep1 - ep0) >> 6;

    u32 ph1 = v.ph1, ph2 = v.ph2, rng = v.rng;
    i32 low = v.low, band = v.band;
    for (unsigned n = 0; n < opxy::block_frames; n++) {
        const u32 i = inc[n];
        u32 ip = i + (i >> 12) * static_cast<u32>((pdepth * ep) >> 15);
        ip += static_cast<u32>((static_cast<i32>(i >> 12) * (lfo >> 8)) >> 1);
        ph1 += ip;
        const i32 s1 = sine(ph1);
        const u32 i2 = ip + (ip >> 1) - (ip >> 5);
        const i32 ic = static_cast<i32>((i2 > 0x10000000u ? 0x10000000u : i2) >> 14);
        ph2 += i2 + static_cast<u32>((s1 >> 5) * (ic * fmk));
        i32 s2 = static_cast<i32>(ph2) >> 16;
        s2 = s2 > 12000 ? 24000 - s2 : (s2 < -12000 ? -24000 - s2 : s2);
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        const i32 ns = static_cast<i32>(rng) >> 16;
        i32 x = ((s1 * (32767 - nmix)) >> 15) + ((s2 * v2lvl) >> 15) + ((ns * nmix) >> 15);
        x = clampv(x, 32767);
        i32 f = fbase + ((fhit * ea) >> 15);
        f = f < 200 ? 200 : (f > fcap ? fcap : f);
        low = clampv(low + ((f * band) >> 15), STATE);
        const i32 high = clampv(x - low - ((damp * band) >> 14), 65535);
        band = clampv(band + ((f * (high >> 1)) >> 14), STATE);
        const i32 lo = clampv(low, 65535), hi = clampv(high, 65535);
        const i32 o = clampv(lo + ((((hi - lo) >> 1) * hpz) >> 14), 65535);
        i32 y = clampv((o * ea) >> 15, 32767);
        y -= (((((y * y) >> 15) * y) >> 15) * 10923) >> 15;
        y = (y * 3) >> 2;
        out[n] = y << 15;                                  /* Q15 -> Q31 at half scale */
        ea += eas; ep += eps;
        if (ea < 0) ea = 0;
        if (ep < 0) ep = 0;
    }
    v.ph1 = ph1; v.ph2 = ph2; v.rng = rng; v.low = low; v.band = band;
    v.age = a1 > 0x400000u ? 0x400000u : a1;
}
}
