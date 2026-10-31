/* toro (Spanish, "bull") for the OP-XY: a port of the OP-1 kennel plugin of the same name, with the same integer
 * DSP so the two sound alike. Signal path per voice: osc 1 + osc 2 (saw -> triangle morph) + noise -> wavefolder
 * -> DC blocker -> 2-pole state-variable low-pass -> soft-clip drive -> output. The OP-XY's own envelope, filter
 * and effects come after.
 *
 * Page 1                                   Page 2 (press any encoder)
 *   Spread  osc 2 ratio 1.0 .. 1.5           Shape  saw .. triangle (both oscillators)
 *   Fold    wavefolder gain 1x .. 8x         Noise  noise into the folder
 *   Cutoff  low-pass, squared response       Drive  output soft clip 1x .. 4x
 *   Res     low-pass resonance               Aphx   key-tracked drift that turns to chaos (below)
 *
 * APHX gives every key its own fixed, random-looking offsets to the two oscillators' tuning, the cutoff, fold,
 * shape and resonance. Low amounts keep it one sound with a little per-key detune and colour, the way an old poly
 * drifts voice to voice; the top end retunes keys by semitones, throws the filter octaves and crushes each key at
 * its own sample-and-hold rate. The offsets come from a hash of the note's position on the keyboard (from the
 * pitch, interpolated between neighbouring keys so a glide slides smoothly), so the same key always sounds the
 * same, and it needs no voice memory. On the OP-1, Drive and Aphx shared one knob; here each has its own slot.
 *
 * Differences from the OP-1 build: 64-sample blocks, controls arrive as Q31 and the output is Q31 at half scale,
 * and the OP-XY gives engines no velocity, so velocity is gone (the XY's envelope and track volume stand in).
 * Integers only, no division in the sample loop, coefficients once per block and ramped across it.
 * Copyright (c) 2026 briannhinton. MIT. */
#include "core.h"
#include <opxy/q31.hpp>

namespace toro {
namespace {
using opxy::i32;
using opxy::u32;

const i32 F_MIN = 300, F_MAX = 29491;   // cutoff coefficient range, Q15
const i32 STATE = 60000;                // filter state clamp: keeps every product below 2^31

inline i32 clampv(i32 v, i32 lim) { return v > lim ? lim : (v < -lim ? -lim : v); }
inline i32 clampi(i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline i32 k15(i32 q31) { return q31 < 0 ? 0 : q31 >> 16; }   // a control as the OP-1's 0..32767

inline i32 cutoff(i32 p)                  // knob 0..32767 -> coefficient, squared for feel
{
    const i32 sq = (p * p) >> 15;
    return F_MIN + ((sq * (F_MAX - F_MIN)) >> 15);
}
inline i32 osc(u32 ph, i32 shape)         // saw -> triangle, Q15
{
    const i32 saw = static_cast<i32>(ph) >> 16;
    const i32 tri = ((saw < 0 ? -saw : saw) << 1) - 32768;
    return saw + (((tri - saw) * (shape >> 1)) >> 14);
}
// 2^(x / 4096) as Q12, x in Q12 octaves (-4 .. +4): whole octaves by shift, the fraction by a quadratic
inline i32 exp2q(i32 x)
{
    if (x > 4 * 4096) x = 4 * 4096; else if (x < -4 * 4096) x = -4 * 4096;
    const i32 n = x >> 12, f = x & 4095;
    const i32 m = 4096 + ((f * (2689 + ((f * 1407) >> 12))) >> 12);
    return n >= 0 ? m << n : m >> -n;
}
// key position of a phase increment, Q8 semitones
inline i32 key_q8(u32 inc)
{
    if (inc < 2u) return 0;
    i32 msb = 31;
    while (!(inc & 0x80000000u)) { inc <<= 1; msb--; }
    const i32 man = static_cast<i32>((inc >> 15) & 0xFFFF);
    const i32 frac = man + ((((man * 22713) >> 16) * (65536 - man)) >> 16);
    return static_cast<i32>(((static_cast<opxy::i64>(msb) << 16) + frac) * 12 >> 8);
}
inline void key_rand(i32 k, i32 *r)       // six fixed, signed per-key values (Q15)
{
    u32 h = static_cast<u32>(k) * 2654435761u ^ 0x5BD1E995u;
    for (int i = 0; i < 6; i++) {
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        r[i] = static_cast<i32>(h >> 16) - 32768;
    }
}
}

inline void initialize(Voice& v, Shared&, u32 owner)
{
    v.ph1 = 0; v.ph2 = 0x40000000u; v.low = 0; v.band = 0; v.f_last = 0; v.dc_x = 0; v.dc_y = 0;
    v.rng = 0x9E3779B9u ^ (owner * 0x85EBCA6Bu);
}
inline void start(Voice& v, Shared& s, u32 owner)
{
    initialize(v, s, owner);             // a quarter cycle apart: no cancelling at unison
}
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& v, Shared&, u32, opxy::Output out, opxy::Parameters p, opxy::Pitch inc)
{
    const i32 drv = k15(p[6]), aphx = k15(p[7]);
    i32 pf1 = k15(p[1]), pf3 = k15(p[3]), ps5 = k15(p[4]);
    i32 det1 = 0, det2 = 0, fmul = 4096, hold = 1;
    if (aphx) {
        const i32 kq = key_q8(inc[0]), k = kq >> 8, kf = kq & 255;
        i32 ra[6], rb[6], r[6];
        key_rand(k, ra); key_rand(k + 1, rb);
        for (int i = 0; i < 6; i++) r[i] = ra[i] + (((rb[i] - ra[i]) * kf) >> 8);
        const i32 a = aphx, a2 = (a * a) >> 15, a3 = (a2 * a) >> 15;
        const i32 semis = ((a * 614) >> 15) + ((a3 * 11674) >> 15);        // Q12 semitones: 0.15 a + 2.85 a^3
        det1 = exp2q(((r[0] * semis) >> 15) / 12) - 4096;
        det2 = exp2q(((r[1] * semis) >> 15) / 12) - 4096;
        fmul = exp2q((r[2] * (((a * 2048) >> 15) + ((a3 * 10240) >> 15))) >> 15);
        pf1 = clampi(pf1 + ((r[3] * (a2 >> 1)) >> 15) + ((r[3] < 0 ? -r[3] : r[3]) * a3 >> 16), 0, 32767);
        ps5 = clampi(ps5 + ((r[4] * a2) >> 15), 0, 32767);
        pf3 = clampi(pf3 + ((((r[5] < 0 ? -r[5] : r[5]) * a3) >> 15) * 3 >> 2), 0, 32767);
        if (a > 26214) hold = 1 + static_cast<i32>(((static_cast<u32>(r[5] + 32768) >> 13) * static_cast<u32>(a - 26214)) >> 13);
        if (hold > 8) hold = 8;
    }
    const i32 spread = k15(p[0]) >> 1;                       // ratio 1.0 .. 1.5, Q15 above 1
    const i32 fold = 32768 + pf1 * 7;                         // 1x .. 8x
    const i32 damp = 32768 - (pf3 * 30000 >> 15);             // 2.0 (none) .. 0.17 (ringing), Q14
    const i32 fcap = F_MAX - (((damp - 2768) * 5764) >> 15);  // Chamberlin stability
    i32 f1 = (cutoff(k15(p[2])) * fmul) >> 12;
    if (f1 > fcap) f1 = fcap;
    if (f1 < F_MIN) f1 = F_MIN;
    const i32 shape = ps5, noise = k15(p[5]) >> 2, drive = 32768 + drv * 3;

    const i32 f0 = v.f_last ? v.f_last : f1;
    const i32 fstep = (f1 - f0) >> 6;                         // ramp the cutoff over the 64-sample block
    i32 f = f0;
    u32 ph1 = v.ph1, ph2 = v.ph2, rng = v.rng;
    i32 low = v.low, band = v.band, dcx = v.dc_x, dcy = v.dc_y, held = 0, cnt = 0;
    for (unsigned n = 0; n < opxy::block_frames; n++) {
        const u32 i = inc[n];
        const u32 i1 = i + static_cast<u32>(static_cast<i32>(i >> 12) * det1), i2 = i + static_cast<u32>(static_cast<i32>(i >> 12) * det2);
        ph1 += i1;
        ph2 += i2 + (i2 >> 15) * static_cast<u32>(spread);
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        i32 s = (osc(ph1, shape) + osc(ph2, shape)) >> 1;
        s += ((static_cast<i32>(rng) >> 16) * noise) >> 15;
        i32 x = (s * (fold >> 4)) >> 11;                      // wavefolder: gain, then reflect at +-32767
        for (int k = 0; k < 5; k++) {
            if (x > 32767) x = 65534 - x;
            else if (x < -32767) x = -65534 - x;
            else break;
        }
        dcy = clampv(x - dcx + ((dcy * 32700) >> 15), 65535); dcx = x; x = clampv(dcy, 32767);
        low = clampv(low + ((f * band) >> 15), STATE);
        const i32 high = clampv(x - low - ((damp * band) >> 14), 65535);
        band = clampv(band + ((f * (high >> 1)) >> 14), STATE);
        f += fstep;
        i32 y = clampv((clampv(low, 65535) * (drive >> 3)) >> 12, 32767);
        y -= (((((y * y) >> 15) * y) >> 15) * 10923) >> 15;  // soft clip x - x^3/3
        y = (y * 3) >> 2;
        if (hold > 1) { if (cnt == 0) { held = y; cnt = hold; } cnt--; y = held; }
        out[n] = y << 15;                                     // Q15 -> Q31 at half scale (|y| <= 24576)
    }
    v.ph1 = ph1; v.ph2 = ph2; v.rng = rng; v.low = low; v.band = band; v.f_last = f1; v.dc_x = dcx; v.dc_y = dcy;
}
}
