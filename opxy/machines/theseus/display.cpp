/* theseus for the OP-XY: the PHALANX screen. A crested Corinthian helmet playing a five-piece kit. The art is
 * retained: panel.svg.fragment (from gen_panel.py) holds the kit and every moving part, and this code places,
 * turns, scales, repaints or hides each part per frame. Greyscale only, so each control keeps its own shape:
 *   Decay   ripple rings spread from the struck head; more rings that last longer as Decay rises
 *           Noise (page 2)  sizzle specks fly up off each hit
 *   Punch   the kick beater winds back further and swings a bigger ball; the kick flashes a diamond on impact
 *           LFO (page 2)    the head slides sideways with the hit LFO, with motion arcs trailing it
 *   Cutoff  the eyes: round embers that grow and burn brighter; strong hits throw short rays
 *           Rate (page 2)   how fast the slide swings
 *   FM      the crest: its bristles grow, go jagged and shake on every hit
 *           Tune (page 2)   shifts which drum the sticks land on, with the sound
 * The OP-XY gives the screen note starts and held counts, not pitches or the engine's state, so the drum each hit
 * lands on comes from a stand-in: a pattern seeded by the note serial that walks the kit the way a DRM pattern
 * does. The head swaps between five drawings (left, left 3/4, front, right 3/4, right), one step every two frames,
 * to look at the drum hit; it never rotates or scales. Copyright (c) 2026 briannhinton. MIT. */
#include "display.h"
#include "nodes.h"
#include <opxy/q31.hpp>

namespace theseus {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

const i32 SX = 1638, SY = 1331;                       // the kit's stretch: OP-1 units -> XY pixels, Q10 (1.6, 1.3), as gen_panel.py
const i32 S = SX;                                     // the uniform scale for rings and other round things
inline i32 PX(i32 x) { return (((x - 160) * SX) >> 10) + 240; }   // OP-1 x -> XY x (pixels)
inline i32 PY(i32 y) { return (((y - 8) * SY) >> 10) + 40; }
inline i32 PXq(i32 xq) { return (((xq - 160 * unit) * SX) >> 10) + 240 * unit; }   // the same for Q10 inputs
inline i32 PYq(i32 yq) { return (((yq - 8 * unit) * SY) >> 10) + 40 * unit; }
const i32 HELM = 1536;                                // head scale, Q10 (1.5)
const i32 NECK_X = PX(158), NECK_Y = PY(70);          // the neck pivot
enum { B_DIM = 1, B_DEC = 2, B_MID = 3, B_SEC = 5, B_EDGE = 6, B_LITE = 7, S_BLACK = 8, S_DARK = 9, S_DEC = 10, S_MID = 11, S_SEC = 12, S_EDGE = 13, S_LITE = 14, S_WHITE = 15 };
enum { D_KICK, D_FLOOR, D_RACK, D_SNARE, D_HAT, D_CYM, D_N };
struct Drum { i32 cx, cy, rx, ry, tipdx, hx, hy, side; };   // OP-1 units
const Drum DRUM[D_N] = {
    { 160, 114, 22, 22,  0,   0,   0, 2 }, { 207, 108, 13,  4, -3, -13,  -9, 1 }, { 190,  88, 11,  4, -2,  -9, -12, 1 },
    { 122,  98, 14,  4,  2,  11, -11, 0 }, {  94,  77, 13,  3,  5,  13,  -8, 0 }, { 230,  66, 17,  3, -7, -14,  -6, 1 } };
const i32 SHOULDER[2][2] = { { 146, 93 }, { 172, 93 } }, REST[2][4] = { { 134, 80, 124, 91 }, { 183, 78, 191, 87 } };
const i32 ARM = 21;                                   // upper arm and forearm, OP-1 units

inline i32 q10(i32 q31) { return q31 < 0 ? 0 : q31 >> 21; }
inline i32 expneg(i32 x)
{
    if (x <= 0) return unit;
    const i32 y = static_cast<i32>((static_cast<u32>(x) * 1477u) >> 10), n = y >> 10;
    if (n >= 10) return 0;
    const i32 t = (y & 1023), t2 = (t * t) >> 10, t3 = (t2 * t) >> 10;
    return (unit - ((t * 709) >> 10) + ((t2 * 246) >> 10) - ((t3 * 57) >> 10)) >> n;
}
inline i32 recoil(i32 t) { if (t <= 40) return 0; if (t >= 260) return unit; const i32 u = (t - 40) * unit / 220; return (u * (2 * unit - u)) >> 10; }
inline i32 lerp(i32 a, i32 b, i32 f) { return a + (((b - a) * f) >> 10); }
inline u32 hash(u32 s) { return s * 1664525u + 1013904223u; }
/* a group of parts sharing a pivot: the same transform for each, the artwork's own paint */
inline void group(Frame& fr, u32 first, u32 count, i32 x, i32 y, i32 scale, i32 tenths)
{
    const Turn t = rotation(tenths);
    for (u32 i = 0; i < count; ++i) { place(fr, first + i, x, y, scale, 0, keep); turn(fr, first + i, t); }
}
inline void hide_group(Frame& fr, u32 first, u32 count) { for (u32 i = 0; i < count; ++i) hide(fr, first + i); }
inline i32 isqrt(u32 v) { u32 r = 0, b = 1u << 30; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return static_cast<i32>(r); }
/* angle (tenths of a degree, clockwise from +x) of a vector, via the SDK's sine: a small iterative atan2 */
inline i32 angle_of(i32 dx, i32 dy)
{
    i32 lo = -1800, hi = 1800;                          /* bisect on cross product sign; 12 steps = 0.9 degree */
    for (int k = 0; k < 12; ++k) {
        const i32 mid = (lo + hi) / 2;
        const i32 cross = (sine(mid + 900) * dy - sine(mid) * dx);   /* cos*dy - sin*dx: > 0 when the vector is clockwise of mid */
        if (cross > 0) lo = mid; else hi = mid;
    }
    return (lo + hi) / 2;
}
/* the drum a hit lands on: a stand-in walk of the kit seeded by the note serial, kick-heavy like a DRM pattern */
inline i32 drum_for(u32 serial, i32 tune)
{
    static const opxy::u8 PAT[16] = { 0, 4, 2, 4, 0, 4, 3, 5, 0, 0, 2, 4, 1, 4, 2, 5 };
    const u32 step = serial & 15u, var = hash(serial >> 4) >> 29;   /* every 16 hits a small variation */
    i32 d = PAT[step];
    if (var == 1 && step == 7) d = D_CYM; else if (var == 2 && step == 13) d = D_FLOOR;
    d += tune > 614 ? 1 : (tune < 410 ? -1 : 0);          /* Tune shifts the kit up or down a drum */
    return d < 0 ? 0 : d >= D_N ? D_N - 1 : d;
}
}

inline void display_reset(DisplayState& st)
{
    st.header.reset(); st.notes.reset();
    st.serial = 0; st.n = 0; st.seed = 0; st.clock = 0;
    for (int i = 0; i < 4; ++i) { st.hit[i].drum = -1; st.hit[i].t = 9000; }
    for (int i = 0; i < 3; ++i) st.side_t[i] = 9000;
    st.side_d[0] = D_SNARE; st.side_d[1] = D_RACK; st.side_d[2] = D_KICK;
    st.any_t = 9000; st.sway = 0; st.view = 2; st.view_to = 2; st.view_hold = 0; st.lfo_sim = 0; st.initialized = 0;
}

inline void display(DisplayState& st, const Input& in, Frame& fr)
{
    const char* const titles[8] = { "Decay", "Punch", "Cutoff", "FM", "Noise", "LFO", "Rate", "Tune" };
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, in.slots[i]);
    { static const char* const R[10] = { "32", "24", "18", "14", "10", "8", "6", "4", "3", "2" }; copy(values[6], header_capacity, R[clamp((q10(in.slots[6]) * 10) >> 10, 0, 9)]); }
    { const i32 semis = ((q10(in.slots[7]) - 512) * 48 + 512) / 1024; char* o = values[7]; i32 a = semis < 0 ? -semis : semis;
      if (semis < 0) *o++ = '-'; else if (semis > 0) *o++ = '+'; if (a >= 10) *o++ = static_cast<char>('0' + a / 10); *o++ = static_cast<char>('0' + a % 10); *o = 0; }
    const opxy::u8 slots[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 } };
    page_header(st.header, in, fr, titles, values, N_HDR, slots);

    if (!st.initialized) {
        for (u32 p = 0; p < Frame::path_count; ++p) {
            fr.paths[p].key = 1;
            for (u32 i = 0; i < Frame::point_count; ++i) { fr.paths[p].points[i].x = sine(static_cast<i32>(i) * 3600 / 64 + 900) * 20; fr.paths[p].points[i].y = sine(static_cast<i32>(i) * 3600 / 64) * 20; }
        }
    }
    const i32 dt = clamp(in.elapsed * 1000 / unit, 1, 100);
    st.notes.update(in);
    const i32 dcy = q10(in.slots[0]), pnch = q10(in.slots[1]), cut = q10(in.slots[2]), fm = q10(in.slots[3]);
    const i32 nois = q10(in.slots[4]), lfo = q10(in.slots[5]), rate = q10(in.slots[6]), tune = q10(in.slots[7]);
    const bool still = in.reduced_motion;
    for (int i = 0; i < 4; ++i) if (st.hit[i].t < 60000) st.hit[i].t += dt;
    for (int i = 0; i < 3; ++i) if (st.side_t[i] < 60000) st.side_t[i] += dt;
    if (st.any_t < 60000) st.any_t += dt;
    st.clock += static_cast<u32>(dt);
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) {                /* a hit (or several since the last frame: the latest) */
        st.serial = in.note_serial; st.n++; st.seed = hash(st.seed + st.n);
        const i32 d = drum_for(st.n, tune);
        for (int i = 3; i > 0; --i) st.hit[i] = st.hit[i - 1];
        st.hit[0].drum = d; st.hit[0].t = 0;
        const i32 s = DRUM[d].side;
        st.side_t[s] = 0; st.side_d[s] = d; st.any_t = 0;
        st.view_to = s == 0 ? 0 : s == 1 ? 4 : 2;
        /* the stand-in hit LFO: a triangle stepped per hit at the Rate, like the engine's */
        static const i32 HITS[10] = { 32, 24, 18, 14, 10, 8, 6, 4, 3, 2 };
        const i32 per = HITS[clamp((rate * 10) >> 10, 0, 9)];
        const i32 ph = static_cast<i32>((st.n % static_cast<u32>(per)) * 2048 / per);   /* 0..2047 */
        st.lfo_sim = ph < 1024 ? ph * 2 - 1024 : 3072 - ph * 2;                           /* -1024..1024 */
    }
    if (st.any_t > 1500) st.view_to = 2;
    if (st.view != st.view_to && ++st.view_hold >= 2) { st.view += st.view_to > st.view ? 1 : -1; st.view_hold = 0; }
    if (still) st.view = st.view_to;
    const i32 target = (st.lfo_sim * lfo * 6) >> 10;        /* up to 6 px, Q10 */
    st.sway += (target - st.sway) / 4;
    const i32 t = still ? 9000 : st.any_t;

    /* the head: one of five drawings, slid by the sway, bobbed by each hit; its face opening rides along */
    const i32 bob = (3 * expneg(t * 1024 / 90)) >> 0;      /* Q10 px */
    const i32 hx = NECK_X * unit + st.sway, hy = NECK_Y * unit + bob;
    for (i32 v = 0; v < 5; ++v) {
        if (v == st.view) group(fr, N_HEAD_0 + v * N_HEAD_0_COUNT, N_HEAD_0_COUNT, hx, hy, HELM, 0);
        else hide_group(fr, N_HEAD_0 + v * N_HEAD_0_COUNT, N_HEAD_0_COUNT);
    }
    /* FM: the crest, 12 bristles rooted along the stalk (the OP-1's Bezier, sampled), pointing outward from it;
       longer, jagged and shaking with FM */
    {
        const i32 shake = still ? 0 : (fm * expneg(t * 1024 / 260)) >> 10;
        const i32 mirror = st.view < 2 ? -1 : 1;
        /* the stalk's control points per view, head units x 8 (OP-1 curves: profile (12,-22)(7,-38)(-17,-39)(-28,-13),
           three-quarter (12,-21)(6,-35)(-14,-36)(-23,-14), front (-11,-20)(-6,-29)(6,-29)(11,-20)) */
        static const i32 K[3][8] = { { 96, -176, 56, -304, -136, -312, -224, -104 }, { 96, -168, 48, -280, -112, -288, -184, -112 }, { -88, -160, -48, -232, 48, -232, 88, -160 } };
        const i32* k = K[st.view == 0 || st.view == 4 ? 0 : st.view == 2 ? 2 : 1];
        for (i32 j = 0; j < 12; ++j) {
            const i32 u = 70 + j * 80, w = 1024 - u;                                                   /* Q10 along the curve, inset from the ends */
            const i32 b0 = (((w * w) >> 10) * w) >> 10, b1 = 3 * ((((w * w) >> 10) * u) >> 10), b2 = 3 * ((((w * u) >> 10) * u) >> 10), b3 = (((u * u) >> 10) * u) >> 10;
            const i32 bx = (k[0] * b0 + k[2] * b1 + k[4] * b2 + k[6] * b3) >> 3, by = (k[1] * b0 + k[3] * b1 + k[5] * b2 + k[7] * b3) >> 3;   /* Q10 head units */
            const i32 dx = (k[2] - k[0]) * (w * w >> 10) + 2 * (k[4] - k[2]) * ((w * u) >> 10) + (k[6] - k[4]) * (u * u >> 10);          /* tangent */
            const i32 dy = (k[3] - k[1]) * (w * w >> 10) + 2 * (k[5] - k[3]) * ((w * u) >> 10) + (k[7] - k[5]) * (u * u >> 10);
            i32 nx = dy, ny = -dx;                                                                     /* a normal */
            if ((((nx >> 8) * ((bx - (st.view == 2 ? 0 : -5 * unit)) >> 6)) + ((ny >> 8) * ((by + 8 * unit) >> 6))) < 0) { nx = -nx; ny = -ny; }   /* outward: away from the crown's centre */
            const i32 ang = angle_of((nx >> 8) * mirror, ny >> 8);                                                 /* tenths of a degree from +x */
            const i32 len = 1024 + ((1536 * fm) >> 10) + ((j & 1) ? ((1200 * fm) >> 10) : 0) + (((640 * shake) >> 10) * sine((t * 30 + j * 900) % 3600) >> 10);
            const i32 cx = hx + ((bx * mirror * HELM) >> 10), cy = hy + ((by * HELM) >> 10);
            place(fr, N_BRISTLE + j, cx, cy, clamp(len, 512, 2048), 0, keep);
            turn(fr, N_BRISTLE + j, rotation(ang + 900 + ((shake * sine((t * 26 + j * 700) % 3600) * 150) >> 20)));   /* the bristle points -y at rest: +90 turns it along the normal */
        }
    }
    /* Cutoff: the eyes, round embers; rays on strong hits */
    {
        const i32 sz = 512 + ((cut * 768) >> 10);
        i32 ex[2], ey[2], n = 2;
        if (st.view == 0 || st.view == 4) { n = 1; ex[0] = 13 * 1024 * (st.view == 0 ? -1 : 1); ey[0] = -7 * 1024; }
        else if (st.view == 2) { ex[0] = -9 * 1024; ex[1] = 9 * 1024; ey[0] = ey[1] = -7 * 1024; }
        else { const i32 m = st.view == 1 ? -1 : 1; ex[0] = 11 * 1024 * m; ex[1] = -3 * 1024 * m; ey[0] = ey[1] = -7 * 1024; }
        for (i32 i = 0; i < 2; ++i) {
            if (i >= n) { hide(fr, N_EYE_L + i); continue; }
            const i32 x = hx + ((ex[i] * HELM) >> 10), y = hy + ((ey[i] * HELM) >> 10);
            const i32 far = (st.view == 1 || st.view == 3) && i == 1 ? 680 : 1024;
            place(fr, N_EYE_L + i, x, y, (sz * far) >> 10, cut > 60 ? S_LITE : S_DEC, cut > 60 ? fill_and_stroke : stroke);
        }
        if (!still && cut > 600 && t < 160) group(fr, N_RAYS, N_RAYS_COUNT, hx + ((ex[0] * HELM) >> 10), hy + ((ey[0] * HELM) >> 10), sz, static_cast<i32>(st.clock % 3600u));
        else hide_group(fr, N_RAYS, N_RAYS_COUNT);
    }
    /* LFO (page 2): motion arcs trail the head's slide */
    {
        const i32 lean = st.sway / 6;                         /* about +-1024 at full */
        if (!still && lfo > 30 && (lean > 100 || lean < -100)) {
            const i32 sd = lean > 0 ? -1 : 1;
            for (i32 i = 0; i < 2; ++i) { place(fr, N_ARC + i, hx + sd * (36 + i * 6) * unit, hy, unit + i * 200, S_SEC, stroke); turn(fr, N_ARC + i, rotation(sd < 0 ? 1800 : 0)); }
        } else { hide(fr, N_ARC); hide(fr, N_ARC + 1); }
    }
    /* cymbal and hats wobble after a hit */
    {
        const i32 tc = st.side_d[1] == D_CYM ? st.side_t[1] : 9000, th = st.side_d[0] == D_HAT ? st.side_t[0] : 9000;
        const i32 wc = still ? -60 : -60 + ((((100 * expneg(tc * 1024 / 450)) >> 10) * sine((tc * 4) % 3600 + 900)) >> 10);
        const i32 wh = still ? 0 : (((30 * expneg(th * 1024 / 180)) >> 10) * sine((th * 5) % 3600 + 900)) >> 10;
        group(fr, N_CYM, N_CYM_COUNT, PX(230) * unit, PY(66) * unit, unit, wc);
        group(fr, N_HAT, N_HAT_COUNT, PX(94) * unit, PY(77) * unit - ((2 * expneg(th * 1024 / 120)) >> 0), unit, wh);
    }
    /* Decay: ripple rings from each recent hit */
    {
        const i32 life = 150 + ((1350 * dcy) >> 10), rings = 1 + ((dcy * 2 + 512) >> 10);
        i32 used = 0;
        for (int h = 0; h < 4 && used < 8; ++h) {
            const Hit& ht = st.hit[h];
            if (still || ht.drum < 0 || ht.t >= life + 200) continue;
            const Drum& g = DRUM[ht.drum];
            const i32 l2 = ht.drum == D_KICK ? life + (life >> 1) : life;
            for (i32 i = 0; i < rings && used < 8; ++i) {
                const i32 ti = ht.t - i * 55;
                if (ti < 0 || ti >= l2) continue;
                const i32 pr = ti * 1024 / l2;
                const i32 shade = pr < 512 ? S_LITE : (pr < 800 ? S_SEC : S_DEC);
                if (ht.drum == D_KICK) { place(fr, N_RING + used, PX(g.cx) * unit, PY(g.cy) * unit, clamp(((4 + ((18 * pr) >> 10)) * S) / 16, 256, 2048), shade, stroke); }
                else {
                    const i32 f = 256 + ((900 * pr) >> 10);
                    const i32 flat = (ht.drum == D_HAT || ht.drum == D_CYM) ? 3 : 5;   /* cymbals are seen edge-on: squash via a smaller ring */
                    place(fr, N_RING + used, PX(g.cx) * unit, PY(g.cy) * unit, clamp(((g.rx * f) >> 10) * S * flat / 80, 256, 2048), shade, stroke);
                }
                used++;
            }
        }
        for (; used < 8; ++used) hide(fr, N_RING + used);
    }
    /* Noise (page 2): sizzle specks fly up off each hit */
    {
        i32 used = 0;
        const i32 n = nois > 20 ? (nois * 4 + 512) >> 10 : 0;
        for (int h = 0; h < 2 && used < 8; ++h) {
            const Hit& ht = st.hit[h];
            if (still || ht.drum < 0 || ht.t > 320 || n == 0) continue;
            const Drum& g = DRUM[ht.drum];
            u32 s = 11u + (st.n - static_cast<u32>(h)) * 7919u;
            const i32 up = 2 + ht.t * 10 / 320, top = ht.drum == D_KICK ? g.cy - g.ry : g.cy;
            for (i32 i = 0; i < n && used < 8; ++i) {
                s = hash(s); const i32 x = g.cx + static_cast<i32>((s >> 16) % (2u * static_cast<u32>(g.rx) + 1u)) - g.rx;
                s = hash(s); const i32 y = top - up - static_cast<i32>((s >> 16) % 6u);
                place(fr, N_SPECK + used, PX(x) * unit, PY(y < 36 ? 36 : y) * unit, unit, S_LITE, fill_and_stroke);
                used++;
            }
        }
        for (; used < 8; ++used) hide(fr, N_SPECK + used);
    }
    /* Punch: the kick beater winds back, swings a bigger ball; the thump diamond on impact */
    {
        const i32 g = recoil(st.side_t[2]);
        const i32 rest = -(150 + ((450 * pnch) >> 10));       /* tenths of a degree: lean left 15 .. 60 */
        const i32 a = still ? rest : (rest * g) >> 10;
        group(fr, N_BEATER, N_BEATER_COUNT, PX(166) * unit, PY(139) * unit, 870 + ((pnch * 460) >> 10), a);
        if (!still && st.side_t[2] < 90 && st.side_d[2] == D_KICK) {
            const i32 bx = PX(166) * unit + ((sine(a) * -24 * (870 + ((pnch * 460) >> 10))) >> 10), by = PY(139) * unit - ((sine(a + 900) * 24 * (870 + ((pnch * 460) >> 10))) >> 10);
            place(fr, N_THUMP, bx, by, unit + ((pnch * 600) >> 10), S_LITE, stroke);
        } else hide(fr, N_THUMP);
    }
    /* arms and sticks: each hand strikes its drum and recoils to rest; upper arm + forearm from the elbow */
    for (int s = 0; s < 2; ++s) {
        const Drum& g = DRUM[st.side_d[s]];
        const i32 f = still ? unit : recoil(st.side_t[s]);
        const i32 tipx = g.cx + g.tipdx, tipy = g.cy - (st.side_d[s] == D_CYM || st.side_d[s] == D_HAT ? 1 : 0);
        const i32 handx = tipx + g.hx, handy = tipy + g.hy;
        const i32 bump = (sine(f * 1800 / unit) * 5) >> 10;
        const i32 hx_ = lerp(handx * unit, REST[s][0] * unit, f), hy_ = lerp(handy * unit, REST[s][1] * unit, f) - bump * unit;
        const i32 tx_ = lerp(tipx * unit, REST[s][2] * unit, f), ty_ = lerp(tipy * unit, REST[s][3] * unit, f) - 2 * bump * unit;
        /* elbow: the two ARM segments from the shoulder to the hand (OP-1 units, Q10) */
        const i32 sx = SHOULDER[s][0] * unit, sy = SHOULDER[s][1] * unit;
        i32 dx = hx_ - sx, dy = hy_ - sy;
        i32 d = isqrt(static_cast<u32>((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) << 4;
        if (d < 2 * unit) d = 2 * unit;
        const i32 dmax = 2 * ARM * unit - unit;
        if (d > dmax) { dx = static_cast<i32>((static_cast<opxy::i64>(dx) * dmax) / d); dy = static_cast<i32>((static_cast<opxy::i64>(dy) * dmax) / d); d = dmax; }
        const i32 half = d >> 1, off = isqrt(static_cast<u32>(((ARM * unit) >> 4) * ((ARM * unit) >> 4) - (half >> 4) * (half >> 4))) << 4;
        i32 nx = -dy * unit / d, ny = dx * unit / d;
        if (ny < 0 || (ny == 0 && s == 0)) { nx = -nx; ny = -ny; }
        if (s == 0 && nx > 0) nx = -nx / 2;
        if (s == 1 && nx < 0) nx = -nx / 2;
        const i32 ex = sx + dx / 2 + ((nx * off) >> 10), ey = sy + dy / 2 + ((ny * off) >> 10);
        /* place in XY pixels: the upper arm from the shoulder toward the elbow, the forearm from the elbow toward the hand,
           the stick continuing from the hand to the tip */
        const i32 SXp = PX(SHOULDER[s][0]) * unit, SYp = PY(SHOULDER[s][1]) * unit;
        const i32 EXp = PXq(ex), EYp = PYq(ey);
        const i32 HXp = PXq(hx_), HYp = PYq(hy_);
        const i32 TXp = PXq(tx_), TYp = PYq(ty_);
        const i32 ua = angle_of(EXp - SXp, EYp - SYp);
        const i32 ul = isqrt(static_cast<u32>(((EXp - SXp) >> 4) * ((EXp - SXp) >> 4) + ((EYp - SYp) >> 4) * ((EYp - SYp) >> 4))) << 4;
        group(fr, s ? N_UARM_R : N_UARM_L, N_UARM_L_COUNT, SXp, SYp, clamp(ul / 30, 256, 2048), ua);
        /* the forearm node is the forearm plus the stick in one piece, pointing +x from the elbow; scale it to the elbow-hand length and
           turn it toward the hand; the stick's tip then lands near the drum (the authored stick is 32 units past a 26-unit forearm) */
        const i32 fa = angle_of(HXp - EXp, HYp - EYp);
        const i32 fl = isqrt(static_cast<u32>(((HXp - EXp) >> 4) * ((HXp - EXp) >> 4) + ((HYp - EYp) >> 4) * ((HYp - EYp) >> 4))) << 4;
        group(fr, s ? N_FARM_R : N_FARM_L, N_FARM_L_COUNT, EXp, EYp, clamp(fl / 26, 256, 2048), fa);
        (void)TXp; (void)TYp;
    }
    hide(fr, N_RIPPLE); hide(fr, N_SWAY);
}
}
