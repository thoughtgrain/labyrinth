/* toro for the OP-XY: the TAURUS screen. A star chart: the bull is the constellation, drawn the way a chart draws
 * it, with the controls on the stars and comets that streak across at random.
 *   Spread  β and ζ Tauri, the horn tips, move out from the head; their joins stretch with them
 *   Fold    each note turns the sky a little about Aldebaran and it eases back, further with Fold; the fold's
 *           character shows as how far the turn overshoots
 *   Cutoff  Aldebaran's magnitude: a point at 0, a spiked blaze with a halo at 100; a note flares it
 *   Res     the Pleiades: a note spins the cluster and it keeps turning for Res; high Res tightens it
 *   Shape   (page 2) bends the horns: the tip stars drift up at the ends
 *   Noise   (page 2) twelve field stars twinkle while a note is held
 *   Drive   (page 2) the Milky Way band brightens
 *   Aphx    (page 2) comets: each note has a chance of launching one, rising with Aphx; a rare comet also crosses
 *           the idle sky on its own (the one thing that moves without a note)
 * The art is retained (panel.svg.fragment from gen_panel.py, node names in nodes.h); this code places, turns,
 * scales and repaints. Stars are authored at their magnitude and scaled; joins are 100 px ribbons placed at one
 * star, turned toward the other and scaled to the distance. Copyright (c) 2026 briannhinton. MIT. */
#include "display.h"
#include "nodes.h"
#include <opxy/q31.hpp>

namespace toro {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
/* palette: solid greys 0 (black) .. 15 (white) */
enum { P_BLACK = 0, P_FAINT = 2, P_DIM = 4, P_MID = 7, P_LITE = 11, P_BRIGHT = 13, P_WHITE = 15 };

inline i32 q10(i32 q31) { return q31 < 0 ? 0 : q31 >> 21; }
inline i32 expneg(i32 x)                                                          // e^-x, x Q10 >= 0, -> Q10
{
    if (x <= 0) return unit;
    const i32 y = static_cast<i32>((static_cast<u32>(x) * 1477u) >> 10), n = y >> 10;
    if (n >= 10) return 0;
    const i32 t = (y & 1023), t2 = (t * t) >> 10, t3 = (t2 * t) >> 10;
    return (unit - ((t * 709) >> 10) + ((t2 * 246) >> 10) - ((t3 * 57) >> 10)) >> n;
}
inline i32 sn(i32 tenths) { return sine(tenths); }
inline i32 cs(i32 tenths) { return sine(tenths + 900); }
inline u32 hash(u32 s) { return s * 1664525u + 1013904223u; }
inline i32 angle_of(i32 dx, i32 dy)                                               // tenths of a degree, clockwise from +x
{
    i32 lo = -1800, hi = 1800;
    for (int k = 0; k < 12; ++k) {
        const i32 mid = (lo + hi) / 2;
        const i32 cross = (sine(mid + 900) * dy - sine(mid) * dx);
        if (cross > 0) lo = mid; else hi = mid;
    }
    return (lo + hi) / 2;
}
inline i32 isqrt(u32 v) { u32 r = 0, b = 1u << 30; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return static_cast<i32>(r); }
inline void group(Frame& fr, u32 first, u32 count, i32 x, i32 y, i32 scale, i32 tenths, u32 shade)
{
    const Turn t = rotation(tenths);
    for (u32 i = 0; i < count; ++i) { place(fr, first + i, x, y, scale, shade, fill_and_stroke); turn(fr, first + i, t); }
}
inline void hide_group(Frame& fr, u32 first, u32 count) { for (u32 i = 0; i < count; ++i) hide(fr, first + i); }
}

inline void display_reset(DisplayState& st)
{
    st.header.reset(); st.notes.reset();
    st.serial = 0; st.t_ms = 9000; st.n = 0; st.lasso_key = 0; st.clock = 0; st.seed = 0x5EED5EEDu; st.initialized = 0;
    for (u32 i = 0; i < 4; ++i) { st.comet_t[i] = -1; st.comet_x[i] = st.comet_y[i] = st.comet_a[i] = st.comet_l[i] = 0; }
    st.idle_ms = 0; st.spin = 0; st.sky = 0;
}

inline void display(DisplayState& st, const Input& in, Frame& fr)
{
    /* header: the SDK's two pages, values as 0..100 except Spread, a ratio */
    const char* const titles[8] = { "Spread", "Fold", "Cutoff", "Res", "Shape", "Noise", "Drive", "Aphx" };
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, in.slots[i]);
    { const i32 r = 100 + (q10(in.slots[0]) * 50 >> 10); values[0][0] = '1'; values[0][1] = '.'; values[0][2] = static_cast<char>('0' + (r / 10) % 10); values[0][3] = static_cast<char>('0' + r % 10); values[0][4] = 0; }
    const opxy::u8 slots[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 } };
    page_header(st.header, in, fr, titles, values, N_HDR, slots);

    if (!st.initialized) {                            /* the two reserved outlines: keyed once, parked */
        for (u32 p = 0; p < Frame::path_count; ++p) {
            fr.paths[p].key = 1;
            for (u32 i = 0; i < Frame::point_count; ++i) { fr.paths[p].points[i].x = (cs(static_cast<i32>(i) * 3600 / 64) * 4); fr.paths[p].points[i].y = (sn(static_cast<i32>(i) * 3600 / 64) * 4); }
        }
    }
    const i32 dt = clamp(in.elapsed * 1000 / unit, 1, 100);
    const NoteEvents ev = st.notes.update(in);
    (void)ev;
    bool note = false;
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) { st.serial = in.note_serial; st.t_ms = 0; st.n++; st.seed = hash(st.seed + st.n); note = true; }
    else if (st.t_ms < 60000) st.t_ms += dt;
    st.clock += static_cast<u32>(dt);
    const bool held = in.held_notes != 0 && in.held_notes <= 128;
    const bool still = in.reduced_motion;
    const i32 t = still ? 9000 : st.t_ms;
    const i32 tq = t * unit / 1000;

    const i32 sprd = q10(in.slots[0]), fold = q10(in.slots[1]), cut = q10(in.slots[2]), res = q10(in.slots[3]);
    const i32 shape = q10(in.slots[4]), noise = q10(in.slots[5]), drive = q10(in.slots[6]), aphx = q10(in.slots[7]);

    /* Fold: a note turns the sky about Aldebaran and it eases back; the turn is bigger with Fold and overshoots */
    if (note && !still) st.sky += ((st.n & 1) ? 1 : -1) * (40 + ((fold * 160) >> 10));            /* tenths of a degree */
    st.sky -= st.sky / 12;
    if (still) st.sky = 0;
    const i32 sky = st.sky;
    const i32 skc = cs(sky), sks = sn(sky);
    const i32 ax = CHART_CX + STAR_X[ST_ALD], ay = CHART_CY + STAR_Y[ST_ALD];                       /* Aldebaran, the pivot */
    /* star positions this frame, Q10 px: the horns move with Spread and bend with Shape, then everything turns */
    i32 sx[ST_N], sy[ST_N];
    for (i32 i = 0; i < ST_N; ++i) {
        i32 x = STAR_X[i] * unit, y = STAR_Y[i] * unit;
        if (i == ST_BETA || i == ST_ZETA) {
            const i32 out = (sprd * 44 * unit) >> 10;                                               /* up to 44 px further out */
            x += out;
            y += (i == ST_BETA ? -1 : 1) * ((sprd * 8 * unit) >> 10) - ((shape * 22 * unit) >> 10);    /* Shape lifts both tips */
        }
        const i32 rx = x - STAR_X[ST_ALD] * unit, ry = y - STAR_Y[ST_ALD] * unit;
        sx[i] = ax * unit + ((rx * skc - ry * sks) >> 10);
        sy[i] = ay * unit + ((rx * sks + ry * skc) >> 10);
    }
    /* Cutoff: Aldebaran's magnitude; a note flares it */
    {
        const i32 flare = still ? 0 : (expneg(tq * 6) * 3) >> 2;
        const i32 mag = 512 + ((cut * 900) >> 10) + flare;                                           /* 0.5 .. 1.9 */
        place(fr, N_ALD + 1, sx[ST_ALD], sy[ST_ALD], clamp(mag, 256, 2048), P_WHITE, fill_and_stroke);
        if (cut > 300) { place(fr, N_ALD, sx[ST_ALD], sy[ST_ALD], clamp((mag * (cut - 300)) >> 10, 256, 2048), cut > 700 ? P_LITE : P_MID, fill_and_stroke); turn(fr, N_ALD, rotation(sky + (still ? 0 : static_cast<i32>((st.clock / 40) % 3600u)))); }
        else hide(fr, N_ALD);
        if (cut > 600) place(fr, N_ALD + 2, sx[ST_ALD], sy[ST_ALD], clamp(mag, 256, 2048), flare > 200 ? P_MID : P_DIM, fill_and_stroke);
        else hide(fr, N_ALD + 2);
    }
    for (i32 i = 1; i < ST_N; ++i) place(fr, STAR_NODE[i], sx[i], sy[i], unit, P_LITE, fill_and_stroke);
    /* joins: placed at star A, turned toward B, scaled to the distance (authored 100 px) */
    for (i32 j = 0; j < JOIN_N; ++j) {
        const i32 a = JOIN_A[j], b = JOIN_B[j];
        const i32 dx = sx[b] - sx[a], dy = sy[b] - sy[a];
        const i32 len = isqrt(static_cast<u32>(((dx >> 4) * (dx >> 4)) + ((dy >> 4) * (dy >> 4)))) << 4;   /* Q10 px */
        place(fr, N_JOIN + j, sx[a], sy[a], clamp(len / 100, 128, 2048), P_DIM, fill_and_stroke);
        turn(fr, N_JOIN + j, rotation(angle_of(dx >> 6, dy >> 6)));
    }
    /* Res: the Pleiades spin on a note and keep turning for Res; higher Res pulls the cluster tighter */
    {
        if (note && !still) st.spin = 600 + ((res * 1400) >> 10);                                    /* tenths of a degree per second, decaying */
        const i32 tau = 150 + ((res * 2400) >> 10);                                                  /* ms */
        st.spin -= (st.spin * dt) / tau;
        if (st.spin < 0) st.spin = 0;
        st.lasso_key += (st.spin * dt) / 1000;                                                       /* the cluster's angle, tenths */
        const i32 ang = still ? 0 : (st.lasso_key % 3600);
        const i32 tight = unit - ((res * 400) >> 10);                                                /* 1.0 .. 0.6 */
        const i32 rx = PLE_X * unit, ry = PLE_Y * unit;
        const i32 px = ax * unit + (((rx - STAR_X[ST_ALD] * unit) * skc - (ry - STAR_Y[ST_ALD] * unit) * sks) >> 10);
        const i32 py = ay * unit + (((rx - STAR_X[ST_ALD] * unit) * sks + (ry - STAR_Y[ST_ALD] * unit) * skc) >> 10);
        const i32 c = cs(ang), s = sn(ang);
        for (u32 k = 0; k < 7; ++k) {
            const i32 ox = (PLE_DX[k] * tight), oy = (PLE_DY[k] * tight);                          /* Q10 px */
            place(fr, N_PLE + k, px + ((ox * c - oy * s) >> 10), py + ((ox * s + oy * c) >> 10), unit, st.spin > 100 ? P_WHITE : P_LITE, fill_and_stroke);
        }
        group(fr, N_PLE_RING, N_PLE_RING_COUNT, px, py, tight, ang, P_FAINT);
    }
    /* Noise: field stars twinkle while a note is held */
    for (u32 k = 0; k < 12; ++k) {
        if (still || noise < 20 || !held) { place(fr, N_TWINKLE + k, TW_X[k] * unit, TW_Y[k] * unit, unit, P_FAINT, fill_and_stroke); continue; }
        const u32 h = hash(st.seed + k * 7919u + (st.clock / 90));
        const bool lit = (h >> 16) % 1024u < static_cast<u32>(noise);
        place(fr, N_TWINKLE + k, TW_X[k] * unit, TW_Y[k] * unit, lit ? unit + 512 : unit, lit ? P_BRIGHT : P_FAINT, fill_and_stroke);
    }
    /* Drive: the Milky Way band brightens */
    if (drive > 600) place(fr, N_BAND, 0, 0, unit, P_BLACK + 1, fill_and_stroke); else hide(fr, N_BAND);   /* a faint wash under the band */
    /* comets: a note launches one with probability 25 % + Aphx; an idle comet crosses on its own every so often */
    {
        bool launch = false;
        if (note && !still) { st.idle_ms = 0; const u32 h = hash(st.seed ^ 0xC0FFEEu); launch = (h >> 16) % 1024u < static_cast<u32>(256 + ((aphx * 700) >> 10)); }
        else if (!still) { st.idle_ms += dt; if (st.idle_ms > 9000) { const u32 h = hash(st.seed + st.clock); if ((h >> 20) % 64u == 0) { launch = true; st.idle_ms = 0; st.seed = h; } } }
        if (launch) {
            for (u32 i = 0; i < 4; ++i) if (st.comet_t[i] < 0) {
                u32 h = hash(st.seed + i * 977u + st.clock);
                st.comet_x[i] = 20 + static_cast<i32>((h >> 8) % 440u); h = hash(h);
                st.comet_y[i] = 48 + static_cast<i32>((h >> 8) % 160u); h = hash(h);
                st.comet_a[i] = static_cast<i32>((h >> 8) % 3600u); h = hash(h);
                st.comet_l[i] = 700 + static_cast<i32>((h >> 8) % 900u) + ((aphx * 600) >> 10);    /* ms of flight */
                st.comet_t[i] = 0;
                break;
            }
        }
        for (u32 i = 0; i < 4; ++i) {
            if (st.comet_t[i] < 0 || still) { hide_group(fr, N_COMET0 + i * 2, 2); continue; }
            st.comet_t[i] += dt;
            const i32 u = st.comet_t[i] * unit / st.comet_l[i];                                      /* 0..1 along the flight */
            if (u >= unit) { st.comet_t[i] = -1; hide_group(fr, N_COMET0 + i * 2, 2); continue; }
            const i32 c = cs(st.comet_a[i]), s = sn(st.comet_a[i]);
            const i32 d = (u * 260) >> 10;                                                           /* px travelled */
            const i32 x = clamp(st.comet_x[i] * unit + c * d - 130 * c, -60 * unit, 600 * unit), y = clamp(st.comet_y[i] * unit + s * d - 130 * s, -60 * unit, 300 * unit);
            const i32 fade = expneg(u * 2);
            const i32 len = clamp(512 + ((aphx * 900) >> 10) + ((u < 512 ? u : unit - u) * 2), 256, 2048);
            place(fr, N_COMET0 + i * 2, x, y, len, fade > 500 ? P_MID : P_DIM, fill_and_stroke); turn(fr, N_COMET0 + i * 2, rotation(st.comet_a[i]));
            place(fr, N_COMET0 + i * 2 + 1, x, y, unit + (fade >> 1), fade > 300 ? P_WHITE : P_LITE, fill_and_stroke);
        }
    }
    hide(fr, N_PATH0); hide(fr, N_PATH1);
}
}
