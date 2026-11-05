/* theseus for the OP-XY: the TOWER screen. The kit stacked into a tower, kick at the base, floor tom, snare, rack
 * tom, and the hi-hat on its rod at the top, drawn in line: black shapes with grey outlines. Only the drum that's
 * hit moves, only upward, and only its head fills with light, so the eye lands on what just played.
 *   Decay   rings spread across the struck head: more of them, lasting longer
 *   Punch   how high the struck drum jumps, and how far a hat hit rocks the cymbals
 *   Cutoff  how brightly the struck head fills
 *   FM      inharmonic ghost hoops thrown out from the struck head (none up to 30), and shimmer arcs off the hats
 *   Noise   (page 2) sparks spray off the struck head, more on the high drums
 *   LFO     (page 2) some hits jump higher than others, following the hit LFO
 *   Rate    (page 2) the floor is the LFO's clock: one dash per hit of its cycle (32 .. 2), the current one lit
 *   Tune    (page 2) shifts which tier the hits land on, as it shifts the drum in the sound
 * The OP-XY gives the screen note starts, not pitches, so the drum each hit lands on comes from a stand-in: a
 * 16-step groove walked by the note serial, its zones shifted by Tune exactly as the engine shifts a pitch's zone.
 *
 * The art is retained (panel.svg.fragment from gen_panel.py; indices and geometry in nodes.h). Every tier's nodes
 * share its head's centre, so a hit is one translation per node; rings and hoops scale about that centre; the hat's
 * cymbals turn about their own centres. The floor slice is the one mutable outline. Copyright (c) 2026
 * briannhinton. MIT. */
#include "display.h"
#include "nodes.h"
#include <opxy/q31.hpp>

namespace theseus {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

inline i32 q10(i32 q31) { return q31 < 0 ? 0 : q31 >> 21; }
inline i32 iabs(i32 v) { return v < 0 ? -v : v; }
inline i32 expneg(i32 x)                                                          /* e^-x, x Q10 >= 0, -> Q10 */
{
    if (x <= 0) return unit;
    const i32 y = static_cast<i32>((static_cast<u32>(x) * 1477u) >> 10), n = y >> 10;
    if (n >= 10) return 0;
    const i32 t = (y & 1023), t2 = (t * t) >> 10, t3 = (t2 * t) >> 10;
    return (unit - ((t * 709) >> 10) + ((t2 * 246) >> 10) - ((t3 * 57) >> 10)) >> n;
}
inline u32 hash(u32 s) { s ^= s >> 16; s *= 0x7FEB352Du; s ^= s >> 15; s *= 0x846CA68Bu; s ^= s >> 16; return s; }
inline i32 rnd(u32& s) { s = hash(s + 0x9E3779B9u); return static_cast<i32>(s >> 22); }      /* 0..1023 */
inline void copy(char* out, u32 cap, const char* s) { u32 i = 0; for (; s[i] && i + 1 < cap; ++i) out[i] = s[i]; out[i] = 0; }
inline void hide_run(Frame& fr, u32 first, u32 count) { for (u32 i = 0; i < count; ++i) hide(fr, first + i); }

/* the stand-in groove, as zones (Q10): kick, hat, floor, hat, snare, hat, kick, rack tom, ... like the design's beat */
const short GROOVE[16] = { 20, 922, 256, 922, 461, 922, 20, 666, 20, 922, 256, 922, 461, 922, 666, 461 };
inline i32 tier_of(i32 zone)
{
    i32 best = 0;
    for (i32 k = 1; k < TIER_N; ++k) if (iabs(TIER_ZONE[k] - zone) < iabs(TIER_ZONE[best] - zone)) best = k;
    return best;
}
/* a hit's light: e^(-t / tau), tau = (30 ms + 1.47 s decay^2)(1 - 0.6 zone), the engine's decay */
inline i32 env(i32 t_ms, i32 decay, i32 zone)
{
    const i32 tau = ((30 * unit + 1470 * ((decay * decay) >> 10)) >> 10) * (unit - ((614 * zone) >> 10)) >> 10;   /* ms */
    return expneg(t_ms * unit / (tau < 8 ? 8 : tau));
}
/* a hit only ever lifts: up in ~18 ms, settling over ~140 ms, never below rest. Q10 px */
inline i32 rise(i32 t_ms, i32 amp)
{
    if (t_ms >= 2000) return 0;
    const i32 up = unit - expneg(t_ms * 57), down = expneg((t_ms * 7490) >> 10);
    return (((amp * up) >> 10) * down) >> 10;
}
}

inline void display_reset(DisplayState& st)
{
    st.header.reset(); st.notes.reset();
    st.serial = 0; st.n = 0; st.initialized = 0; st.slice_key = 0;
    for (int k = 0; k < 5; ++k) { st.hit_t[k] = 60000; st.amp[k] = 0; st.zone[k] = 0; st.hit_n[k] = 0; }
    for (int k = 0; k < 2; ++k) { st.rock_t[k] = 60000; st.rock_side[k] = 1; }
}

inline void display(DisplayState& st, const Input& in, Frame& fr)
{
    /* header: the SDK's two pages; Rate as hits per LFO cycle, Tune in semitones */
    const char* const titles[8] = { "Decay", "Punch", "Cutoff", "FM", "Noise", "LFO", "Rate", "Tune" };
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, in.slots[i]);
    const i32 ri = clamp((q10(in.slots[6]) * 10) >> 10, 0, RATE_N - 1);                          /* as the engine reads Rate */
    { static const char* const R[RATE_N] = { "32", "24", "18", "14", "10", "8", "6", "4", "3", "2" }; copy(values[6], header_capacity, R[ri]); }
    { const i32 semis = ((q10(in.slots[7]) - 512) * 48 + 512) / 1024; char* o = values[7]; i32 a = semis < 0 ? -semis : semis;
      if (semis < 0) *o++ = '-'; else if (semis > 0) *o++ = '+'; if (a >= 10) *o++ = static_cast<char>('0' + a / 10); *o++ = static_cast<char>('0' + a % 10); *o = 0; }
    const opxy::u8 slots[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 } };
    page_header(st.header, in, fr, titles, values, N_HDR, slots);

    const i32 decay = q10(in.slots[0]), punch = q10(in.slots[1]), cut = q10(in.slots[2]), fm = q10(in.slots[3]);
    const i32 noise = q10(in.slots[4]), lfo = q10(in.slots[5]), tune = q10(in.slots[7]);
    const bool still = in.reduced_motion != 0;
    const i32 dt = clamp(in.elapsed * 1000 / unit, 1, 100);
    st.notes.update(in);
    for (int k = 0; k < 5; ++k) if (st.hit_t[k] < 60000) st.hit_t[k] += dt;
    for (int k = 0; k < 2; ++k) if (st.rock_t[k] < 60000) st.rock_t[k] += dt;

    /* a hit: the groove's next zone, shifted by Tune, picks the tier; the hit LFO sets how high it jumps */
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) {
        st.serial = in.note_serial; st.n = (st.n + 1) & 0xFFFFFFu;
        const i32 zone = clamp(GROOVE[(st.n - 1) & 15u] + tune - 512, 0, unit);
        const i32 k = tier_of(zone);
        const i32 per = RATE_HITS[ri];
        const i32 ph = static_cast<i32>(st.n % static_cast<u32>(per)) * unit / per;               /* the LFO's triangle at this hit */
        const i32 tri = unit - 4 * iabs(ph - 512);
        const i32 swing = unit + ((((lfo * 614) >> 10) * tri) >> 10);                              /* 1 + 0.6 lfo tri */
        st.hit_t[k] = 0; st.zone[k] = zone; st.hit_n[k] = st.n;
        st.amp[k] = (((3 * unit + 8 * punch) * swing) >> 10);                                      /* (3 + 8 punch) px */
        if (k == HAT) {                                                                            /* hats rock, alternating sides */
            st.rock_t[1] = st.rock_t[0]; st.rock_side[1] = st.rock_side[0];
            st.rock_t[0] = 0; st.rock_side[0] = ((st.n - 1) & 1u) ? 1 : -1;
        }
    }

    i32 e[5], lift[5];
    for (int k = 0; k < 5; ++k) {
        e[k] = st.hit_t[k] < 60000 ? env(st.hit_t[k], decay, st.zone[k]) : 0;
        lift[k] = still ? 0 : rise(st.hit_t[k], st.amp[k]);
    }

    /* the floor clock: the dash set for this Rate, the current hit's dash lit, its slice of the floor filled */
    const i32 per = RATE_HITS[ri];
    const i32 step = st.n ? static_cast<i32>((st.n - 1) % static_cast<u32>(per)) : -1;
    for (i32 r = 0; r < RATE_N; ++r) {
        const u32 first = static_cast<u32>(DASH_FIRST[r]);
        if (r != ri) { hide_run(fr, first, RATE_HITS[r]); continue; }
        for (i32 d = 0; d < per; ++d) place(fr, first + d, TOWER_X * unit, FLOOR_Y * unit, unit, d == step ? P_HOT : P_G4, fill_and_stroke);
    }
    {   /* the path always holds a wedge (a frame needs a nonzero key), parked until the first hit */
        const i32 ws = step < 0 ? 0 : step;
        const u32 key = 1u + static_cast<u32>(ri) * 64u + static_cast<u32>(ws);
        if (st.slice_key != key) {                     /* the wedge: centre, out along one edge, the arc, back along the other */
            st.slice_key = key;
            const i32 gap = per <= 8 ? 184 : 328;                                                  /* of a step, Q10 */
            const i32 a0 = 900 + ((3600 * (ws * unit + gap / 2)) / per >> 10);                   /* tenths, from the front */
            const i32 a1 = 900 + ((3600 * ((ws + 1) * unit - gap / 2)) / per >> 10);
            const i32 arc = clamp((a1 - a0) * 40 / 573, 2, 61);                                    /* points on the arc: ~0.6 px apart at the sides */
            const i32 edge = Frame::point_count - 1 - arc, e1 = edge / 2, e2 = edge - e1;
            Point* p = fr.paths[0].points;
            const i32 x0 = (FLOOR_RX * sine(a0 + 900)) >> 11, y0 = (FLOOR_RY * sine(a0)) >> 11;      /* half size: points stay within 64 px */
            const i32 x1 = (FLOOR_RX * sine(a1 + 900)) >> 11, y1 = (FLOOR_RY * sine(a1)) >> 11;
            u32 n = 0;
            p[n].x = 0; p[n].y = 0; ++n;
            for (i32 j = 1; j <= e1; ++j) { p[n].x = x0 * j / (e1 + 1); p[n].y = y0 * j / (e1 + 1); ++n; }
            for (i32 j = 0; j < arc; ++j) {
                const i32 a = a0 + (a1 - a0) * j / (arc - 1);
                p[n].x = (FLOOR_RX * sine(a + 900)) >> 11; p[n].y = (FLOOR_RY * sine(a)) >> 11; ++n;
            }
            for (i32 j = e2; j >= 1; --j) { p[n].x = x1 * j / (e2 + 1); p[n].y = y1 * j / (e2 + 1); ++n; }
            fr.paths[0].key = key;
        }
        if (step < 0) hide(fr, N_SLICE);
        else place(fr, N_SLICE, TOWER_X * unit, FLOOR_Y * unit, 2 * unit, P_SLICE, fill_and_stroke);   /* drawn at twice the points */
    }

    /* the drums: the tier lifts as one; the struck shell's outline brightens and its head fills; rings and hoops */
    const i32 rings = 1 + ((3 * decay + 512) >> 10);
    const i32 extra = fm > 307 ? (fm - 307) * unit / 717 : 0;                                      /* (fm - 0.3) / 0.7 */
    const i32 hoops = extra ? 1 + ((3 * extra + 512) >> 10) : 0;
    for (i32 k = 0; k < DRUM_N; ++k) {
        const i32 dy = -lift[k];
        const u32 first = static_cast<u32>(DRUM_FIRST[k]);
        for (u32 i = 0; i < static_cast<u32>(DRUM_COUNT[k]); ++i) {
            const u32 n = first + i;
            place(fr, n, TOWER_X * unit, NODE_OY[n - N_TIERS] * unit + dy, unit, NODE_SHADE[n - N_TIERS], fill_and_stroke);
        }
        const bool lit = e[k] >= 41;                                                               /* 0.04 */
        place(fr, DRUM_BODY[k], TOWER_X * unit, DRUM_TOP[k] * unit + dy, unit, lit ? P_SHELL_HOT : static_cast<u32>(DRUM_BODY_REST[k]), fill_and_stroke);
        const i32 level = clamp((unit + 4 * ((e[k] * (512 + cut / 2)) >> 10) + 512) >> 10, 1, 5);   /* 1 + 4 e (0.5 + 0.5 cutoff) */
        place(fr, DRUM_BATTER[k], TOWER_X * unit, DRUM_TOP[k] * unit + dy, unit, lit ? static_cast<u32>(P_LIT1 + level - 1) : P_HEAD, fill_and_stroke);
        /* rings: spreading out from the middle of the head as the hit fades */
        for (i32 r = 0; r < 4; ++r) {
            const u32 n = static_cast<u32>(DRUM_RING[k] + r);
            if (still || e[k] <= 31 || r >= rings) { hide(fr, n); continue; }
            const i32 u = ((((unit - e[k]) * 922) >> 10) + r * 225) & 1023;                          /* (0.9 (1 - e) + 0.22 r) mod 1 */
            const i32 s = u + 1829 / DRUM_R[k];                                                     /* + 1.5 px */
            if (s < 307) { hide(fr, n); continue; }
            place(fr, n, TOWER_X * unit, DRUM_TOP[k] * unit + dy + 410, s, r ? P_G6 : P_HOT, fill_and_stroke);
        }
    }
    /* fm's overtone hoops, thrown outward and up from the drum hit last (one shared set, authored at OVER_R) */
    {
        i32 k = -1;
        for (i32 j = 0; j < DRUM_N; ++j) if (st.hit_t[j] < 60000 && (k < 0 || st.hit_t[j] < st.hit_t[k])) k = j;
        for (i32 h = 0; h < 4; ++h) {
            const u32 n = static_cast<u32>(N_OVER + h);
            if (still || k < 0 || e[k] < 51 || h >= hoops) { hide(fr, n); continue; }
            const i32 g = unit + ((((82 + 102 * h) * (410 + fm)) >> 10) * (1331 - e[k]) >> 10);    /* 1 + (0.08 + 0.1 h)(0.4 + fm)(1.3 - e) */
            place(fr, n, TOWER_X * unit, DRUM_TOP[k] * unit - lift[k] - 1536 * (h + 1), clamp(g * DRUM_R[k] / OVER_R, 256, 2 * unit), h ? P_G5 : P_G6, fill_and_stroke);
        }
    }

    /* the hi-hat: a hit lifts the top cymbal open and lights it; recent hits rock both, one edge up, the other down */
    {
        const i32 open = (lift[HAT] * 1434) >> 10;                                                 /* 1.4 x */
        i32 top_deg = 0, low_deg = 0;                                                               /* tenths of a degree */
        if (!still) for (int h = 0; h < 2; ++h) {
            const i32 t = st.rock_t[h];
            if (t >= 3000) continue;
            const i32 amp = ((5 * unit + 7 * punch) * expneg((t * 2330) >> 10)) >> 10;            /* (5 + 7 punch) e^(-t/0.45), degrees Q10 */
            const i32 w = (t * 8847) >> 10;                                                        /* 2.4 Hz, tenths */
            top_deg += st.rock_side[h] * ((amp * 10 * sine(w + 900)) >> 20);
            low_deg += st.rock_side[h] * ((amp * 10 * 358 >> 10) * sine(w + 344 + 900) >> 20);     /* 0.35 of it, 0.6 rad behind */
        }
        place(fr, HAT_ROD, TOWER_X * unit, HAT_BASE * unit, unit, P_DIM, fill_and_stroke);
        const Turn tl = rotation(low_deg), tt = rotation(top_deg);
        const bool lit = e[HAT] >= 41;
        const i32 level = clamp((unit + 4 * ((e[HAT] * (512 + cut / 2)) >> 10) + 512) >> 10, 1, 5);
        for (u32 i = 0; i < HAT_PARTS; ++i) {
            place(fr, HAT_LOW + i, TOWER_X * unit, (HAT_BASE + 2) * unit, unit, NODE_SHADE[HAT_LOW + i - N_TIERS], fill_and_stroke); turn(fr, HAT_LOW + i, tl);
            const u32 shade = (i == 0 && lit) ? static_cast<u32>(P_LIT1 + level - 1) : NODE_SHADE[HAT_TOP + i - N_TIERS];
            place(fr, HAT_TOP + i, TOWER_X * unit, (HAT_BASE - 3) * unit - open, unit, shade, fill_and_stroke); turn(fr, HAT_TOP + i, tt);
        }
        const i32 arcs = fm > 51 && e[HAT] > 51 && !still ? (unit + 3 * fm + 512) >> 10 : 0;
        for (i32 a = 0; a < 4; ++a) {
            if (a >= arcs) { hide(fr, HAT_SHIM + a); continue; }
            place(fr, HAT_SHIM + a, TOWER_X * unit, (HAT_BASE - 3) * unit - open, unit, P_G6, fill_and_stroke);
        }
    }

    /* noise's sparks: specks thrown up off each sounding head, newest hit first, from one pool */
    u32 used = 0;
    if (!still && noise > 0) {
        bool done[5] = { false, false, false, false, false };
        for (int pass = 0; pass < 5; ++pass) {
            i32 k = -1;
            for (int j = 0; j < 5; ++j) if (!done[j] && (k < 0 || st.hit_t[j] < st.hit_t[k])) k = j;
            done[k] = true;
            if (st.hit_t[k] >= 60000 || e[k] < 51) continue;
            const i32 count = (((40 * noise) >> 10) * (512 + st.zone[k] / 2) + 512) >> 10;        /* 40 noise (0.5 + 0.5 zone) */
            const i32 r = k == HAT ? HAT_R : DRUM_R[k];
            const i32 top = k == HAT ? (HAT_BASE - 3) * unit - ((lift[HAT] * 1434) >> 10) : DRUM_TOP[k] * unit - lift[k];
            u32 seed = st.hit_n[k] * 2654435761u + 7u;
            for (i32 s = 0; s < count && used < SPARK_N; ++s) {
                const i32 a = 1890 + ((1620 * rnd(seed)) >> 10);                                   /* tenths: 1.05 pi .. 1.95 pi, the upper half */
                const i32 d = ((r * (717 + ((922 * rnd(seed)) >> 10))) * (1280 - e[k] / 2)) >> 10;  /* r (0.7 + 0.9 x)(1.25 - 0.5 e), Q10 px */
                const i32 size = 956 + ((887 * rnd(seed)) >> 10);                                  /* 1.4 .. 2.7 px half-size, over the authored 1.5 */
                const i32 x = TOWER_X * unit + ((sine(a + 900) * (d >> 4)) >> 6);
                const i32 y = top + ((((sine(a) * (d >> 4)) >> 6) * 563) >> 10) - 4 * unit;         /* 0.55 of it, 4 px up */
                place(fr, SPARK + used, x, y, size, P_G6, fill_and_stroke);
                ++used;
            }
        }
    }
    for (u32 s = used; s < SPARK_N; ++s) hide(fr, SPARK + s);
}
}
