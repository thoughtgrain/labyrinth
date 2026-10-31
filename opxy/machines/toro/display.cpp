/* toro for the OP-XY: the RIDER screen. A robot bull on a hydraulic ram, head-on, with a cowboy aboard. The art
 * is retained: panel.svg.fragment (from gen_panel.py) holds every part, and this code places, turns, scales,
 * repaints or hides each one per frame. Greyscale only, so each control keeps its own shape, as on the OP-1:
 *   Spread  the horns spread, and the hat brim with them       Shape (page 2)  the horn tips curl up
 *   Fold    each note lifts the ram and rocks the rig; the cowboy is thrown, his legs kick, his spurs spin;
 *           a pulse runs the hose                               Noise (page 2)  sparks off the turntable while a note is held
 *   Cutoff  the eye slits open, and the cowboy's eyes with them; a note lights the slits when it is up
 *           Drive (page 2)  steam from the vents on each note
 *   Res     each note strikes the nose ring; the lasso arm rises with a loop that grows and spins longer
 *           Aphx (page 2)  glitch slices tear across the rig on each note; the same note always tears the same
 * Motion follows the OP-XY's note activity (note_serial, held_notes): nothing moves without a note. The art is
 * authored in the OP-1's rig units (origin on the ram's top); RIG scales it to the XY's screen. Limbs are
 * chains of nodes: each joint's node is placed at the end of the one before it and turned, so the horns
 * telescope and the cowboy's legs and lasso arm bend as they did when the OP-1 drew them line by line.
 * The two-page header is the SDK's. Copyright (c) 2026 briannhinton. MIT. */
#include "display.h"
#include <opxy/q31.hpp>

namespace toro {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

const i32 CX = 240, BASE = 208, TOP0 = 186;          // the ram's foot and its resting top, pixels
const i32 RIG = 973;                                  // rig units -> pixels, Q10 (0.95)
const i32 CS = 1587;                                  // the cowboy's scale, Q10 (1.55)
const i32 HK = 1198;                                  // the head's scale, Q10 (1.17)
enum { N_LASSO = 0, N_ROPE = 1, N_HDR = 2, N_SHADOW = 10, N_ROD = 11, N_BODY = 12, N_HORN = 13, N_PLATE = 19, N_DECAL = 20, N_VENTS = 21,
       N_EYE = 22, N_HILITE = 24, N_RING = 26, N_STEAM = 27, N_SPARK = 31, N_PULSE = 37, N_TORSO = 38, N_BELT = 39, N_HEAD = 40, N_FACE = 41,
       N_BRIM = 42, N_CROWN = 43, N_UPPER = 44, N_FORE = 45, N_THIGH = 46, N_SHIN = 48, N_SPUR = 50, N_TEAR = 52, N_KNOT = 60, N_BOOT = 61, N_SPARE = 63 };
/* palette: 0..7 body entries (near-black fill, outline dim..white); 8..15 solid greys */
enum { B_DIM = 1, B_DEC = 2, B_MID = 3, B_SEC = 5, B_EDGE = 6, B_LITE = 7, S_BLACK = 8, S_DARK = 9, S_DEC = 10, S_MID = 11, S_SEC = 12, S_EDGE = 13, S_LITE = 14, S_WHITE = 15 };

inline i32 q10(i32 q31) { return q31 < 0 ? 0 : q31 >> 21; }                     // a control as 0..1023
inline i32 expneg(i32 x)                                                          // e^-x, x Q10 >= 0, -> Q10
{
    if (x <= 0) return unit;
    const i32 y = static_cast<i32>((static_cast<u32>(x) * 1477u) >> 10), n = y >> 10;
    if (n >= 10) return 0;
    const i32 t = (y & 1023), t2 = (t * t) >> 10, t3 = (t2 * t) >> 10;
    return (unit - ((t * 709) >> 10) + ((t2 * 246) >> 10) - ((t3 * 57) >> 10)) >> n;
}
inline i32 ph(i32 t_ms, i32 rate100) { return (t_ms * rate100 / 1000) * 36 / 628; }   // tenths of a degree at rate rad/s x100
inline i32 sn(i32 tenths) { return sine(tenths); }                                      // Q10
inline i32 cs(i32 tenths) { return sine(tenths + 900); }
inline i32 lerp(i32 a, i32 b, i32 f) { return a + (((b - a) * f) >> 10); }               // f Q10
/* the angle (tenths of a degree, clockwise from +x on screen) of a small vector: bisect on the cross product */
inline i32 angle_of(i32 dx, i32 dy)
{
    i32 lo = -1800, hi = 1800;
    for (int k = 0; k < 12; ++k) {
        const i32 mid = (lo + hi) / 2;
        const i32 cross = (sine(mid + 900) * dy - sine(mid) * dx);
        if (cross > 0) lo = mid; else hi = mid;
    }
    return (lo + hi) / 2;
}
/* a joint's position (Q10 px) a distance along a turned limb: from (x, y), len rig units at angle tenths */
inline void along(i32 x, i32 y, i32 len_q10, i32 tenths, i32& ox, i32& oy)
{
    ox = x + ((cs(tenths) * len_q10) >> 10);
    oy = y + ((sn(tenths) * len_q10) >> 10);
}
inline u32 hash(u32 s) { return s * 1664525u + 1013904223u; }
}

inline void display_reset(DisplayState& st)
{
    st.header.reset(); st.notes.reset();
    st.serial = 0; st.t_ms = 9000; st.n = 0; st.lasso_key = 0; st.clock = 0; st.seed = 0; st.initialized = 0;
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

    if (!st.initialized) {                            /* the first frame after a reset sets every outline */
        for (u32 p = 0; p < Frame::path_count; ++p) {
            fr.paths[p].key = 1;
            for (u32 i = 0; i < Frame::point_count; ++i) { fr.paths[p].points[i].x = (cs(static_cast<i32>(i) * 3600 / 64) * 20); fr.paths[p].points[i].y = (sn(static_cast<i32>(i) * 3600 / 64) * 20); }
        }
    }
    const i32 dt = clamp(in.elapsed * 1000 / unit, 1, 100);
    const NoteEvents ev = st.notes.update(in);
    (void)ev;
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) { st.serial = in.note_serial; st.t_ms = 0; st.n++; st.seed = hash(st.seed + st.n); }
    else if (st.t_ms < 60000) st.t_ms += dt;
    st.clock += static_cast<u32>(dt);
    const bool held = in.held_notes != 0 && in.held_notes <= 128;
    const bool still = in.reduced_motion;
    const i32 t = still ? 9000 : st.t_ms;
    const i32 tq = t * unit / 1000;
    const i32 tt = t + static_cast<i32>(st.n & 63) * 500;                                       /* the cowboy's own clock: each note shifts it */

    const i32 sprd = q10(in.slots[0]), fold = q10(in.slots[1]), cut = q10(in.slots[2]), res = q10(in.slots[3]);
    const i32 shape = q10(in.slots[4]), noise = q10(in.slots[5]), drive = q10(in.slots[6]), aphx = q10(in.slots[7]);

    /* FOLD: each note lifts the ram and rocks the rig; it keeps rocking while notes play */
    const i32 e = expneg(tq * 5), e2 = expneg(tq * 3);
    const i32 lift = ((2 * unit + fold * 5 + ((fold * 7 * e) >> 10)) * 2 / 5) * 3 / 2;          // Q10 px
    const i32 dir = (st.n & 1) ? 1 : -1;
    const bool on = t < 1000;
    const i32 amp = (fold * 104 + ((fold * fold) >> 10) * 261) >> 10;                             // angle units (1/8192 turn)
    const i32 env = 11469 + ((21299 * e2) >> 10);
    const i32 a = on ? dir * ((((amp * env) >> 15) * cs(ph(t, 900))) >> 10) : 0;               // 1/8192 turn
    const i32 lean = a * 3600 / 8192;                                                            // tenths of a degree
    const i32 cx = CX + ((a * 26) >> 10) * 3 / 2;
    const i32 top = TOP0 - (lift >> 10);
    const i32 lc = cs(lean), ls = sn(lean);
    /* a rig-unit point under the lean, Q10 px */
    struct Rig { i32 cx, top, c, s; i32 x, y;
        void at(i32 lx, i32 ly) { const i32 rx = (lx * RIG) >> 10, ry = (ly * RIG) >> 10; x = cx * unit + (rx * c - ry * s); y = top * unit + (rx * s + ry * c); }
        void atq(i32 lx, i32 ly) { const i32 rx = (lx * RIG) >> 20, ry = (ly * RIG) >> 20; x = cx * unit + (rx * c - ry * s); y = top * unit + (rx * s + ry * c); } } rg = { cx, top, lc, ls, 0, 0 };

    /* the ram and its shadow */
    place(fr, N_ROD, cx * unit, (BASE - 8) * unit, clamp(((BASE - 8) - top) * unit / 16, unit / 2, 2 * unit), S_SEC, fill_and_stroke);
    turn(fr, N_ROD, rotation(lean / 2));
    { const i32 srx = 40 - ((BASE - top) * 2) / 5; place(fr, N_SHADOW, (cx + ((a * 27) >> 10)) * unit, (BASE + 6) * unit, clamp(srx * unit / 30, unit / 2, 2 * unit), S_BLACK, fill_and_stroke); }
    /* the body, then the plate and its decals (the cowboy sits between them) */
    place(fr, N_BODY, cx * unit, top * unit, RIG, B_SEC, stroke); turn(fr, N_BODY, rotation(lean));
    place(fr, N_PLATE, cx * unit, top * unit, RIG, B_EDGE, stroke); turn(fr, N_PLATE, rotation(lean));
    place(fr, N_DECAL, cx * unit, top * unit, RIG, B_DEC, stroke); turn(fr, N_DECAL, rotation(lean));
    place(fr, N_VENTS, cx * unit, top * unit, RIG, B_DEC, stroke); turn(fr, N_VENTS, rotation(lean));

    /* SPREAD: the horns grow and swing outward; SHAPE curls the tips inward. One curved tube each, a tip on its end */
    for (int sd = -1; sd <= 1; sd += 2) {
        const u32 side = sd < 0 ? 0 : 1;
        const i32 base = sd < 0 ? 1800 : 0;                                                   /* the left horn is authored along -x */
        const i32 hs = (RIG * (unit + (sprd * 450 >> 10))) >> 10;                              /* horn scale */
        const i32 swing = (sprd * 160 >> 10) - 60;                                             /* down-and-out as SPREAD rises */
        const i32 t0 = lean + sd * swing + base;                                               /* the root's true direction */
        i32 x, y; rg.at(sd * 20 * HK >> 10, (-14 * HK >> 10) - 30);
        x = rg.x; y = rg.y;
        place(fr, N_HORN + 4 + side, x, y, hs, S_SEC, stroke); turn(fr, N_HORN + 4 + side, rotation(t0 + base));
        place(fr, N_HORN + side, x, y, hs, S_SEC, fill_and_stroke); turn(fr, N_HORN + side, rotation(t0 + base));
        /* the tube's end, (28, -17) head units, under the horn's turn and scale */
        const i32 ex = (28 * HK * hs) >> 10, ey = (-17 * HK * hs) >> 10;   /* Q10 px */
        const i32 c0 = cs(t0 + base), s0 = sn(t0 + base), lx = sd * ex;                        /* the authored end point under the node's turn */
        const i32 tx = x + ((lx * c0 - ey * s0) >> 10), ty = y + ((lx * s0 + ey * c0) >> 10);
        const i32 curl = (shape * 700 >> 10);
        place(fr, N_HORN + 2 + side, tx, ty, hs, S_LITE, fill_and_stroke); turn(fr, N_HORN + 2 + side, rotation(t0 + base - sd * curl));
    }
    /* CUT: the eye slits open (scale), and a note lights the highlight while CUT is up */
    {
        const i32 open_ = (RIG * (640 + (cut * 560 >> 10))) >> 10;
        const bool lit = !still && t < 160 && cut > 600;
        for (int sd = -1; sd <= 1; sd += 2) {
            const u32 side = sd < 0 ? 0 : 1;
            rg.atq(sd * 14250 * HK >> 10, (-3600 * HK >> 10) - 30 * 1024);
            place(fr, N_EYE + side, rg.x, rg.y, open_, S_LITE, stroke); turn(fr, N_EYE + side, rotation(lean));
            place(fr, N_HILITE + side, rg.x, rg.y, open_, lit ? S_WHITE : S_MID, fill_and_stroke); turn(fr, N_HILITE + side, rotation(lean));
        }
    }
    /* DRIVE (page 2): each note blasts steam from the vents */
    for (u32 i = 0; i < 4; ++i) {
        const i32 sd = (i & 1) ? 1 : -1, k = static_cast<i32>(i >> 1);
        if (still || drive < 20 || t >= 600 || k >= 1 + ((drive * 3 + 512) >> 10)) { hide(fr, N_STEAM + i); continue; }
        const i32 u = clamp(t * 1024 / 600 + k * 184, 0, 1024);
        rg.at(sd * (8 + ((u * (22 + (drive * 10 >> 10))) >> 10)), -6 - (u * 14 >> 10));
        place(fr, N_STEAM + i, rg.x, rg.y, unit + u, S_SEC, stroke);
    }
    /* RES: each note strikes the nose ring; resonance sets how long it keeps swinging */
    {
        const i32 tau = 123 + ((1434 * res) >> 10), dec = expneg(tq * 1024 / tau);
        const i32 sw = (((717 * dec) >> 10) * sn(ph(t, 1400))) >> 10;                            // 1/8192 turn
        const i32 R = (RIG * (unit + ((res * 5) >> 3))) >> 10;                                   // ring scale
        rg.at(0, 6);                                                                             /* hangs from the muzzle */
        place(fr, N_RING, rg.x, rg.y, R, S_LITE, stroke); turn(fr, N_RING, rotation(lean + sw * 3600 / 8192));
    }
    /* FOLD: the hose pulse runs from the hose end to the ram on each note */
    if (!still && t < 400) {
        const i32 u = unit - t * unit / 400;                                                     // 0 at the hose end, 1 at the ram
        const i32 hx = 400 - ((130 * u) >> 10), hy = (BASE + 2) + ((8 * sn(u * 1800 / unit)) >> 10);
        place(fr, N_PULSE, hx * unit, hy * unit, unit + fold, S_LITE, fill_and_stroke);
    } else hide(fr, N_PULSE);
    /* NOISE (page 2): sparks off the turntable while a note is held */
    for (u32 i = 0; i < 6; ++i) {
        if (still || noise < 20 || !held || i >= static_cast<u32>((noise * 6 + 512) >> 10)) { hide(fr, N_SPARK + i); continue; }
        u32 s = hash(st.seed + i * 7919u + (st.clock / 83));
        const i32 x = cx - 50 + static_cast<i32>((s >> 16) % 100u); s = hash(s);
        const i32 y = BASE - 20 + static_cast<i32>((s >> 16) % 16u);
        place(fr, N_SPARK + i, x * unit, y * unit, unit, S_LITE, fill_and_stroke);
    }

    /* the cowboy: thrown by FOLD, his legs kick and his spurs spin, the hat follows SPREAD, the lasso follows RES */
    {
        const i32 m = clamp(((fold * (205 + ((922 * e) >> 10))) >> 10) * 1280 >> 10, 0, 1024);
        const i32 wild = (fold * fold) >> 10;
        const i32 rt = -((a * (1843 + 2 * wild)) >> 10) + dir * ((((m * (123 + ((410 * wild) >> 10))) >> 10) * 1304 >> 10) * sn(ph(t, 1100)) >> 10);
        const i32 s9 = sn(ph(t, 900));
        const i32 up = clamp(((s9 < 0 ? -s9 : s9) * ((m * (3072 + 4 * wild)) >> 10)) >> 10, 0, 14 * unit);   // Q10 px
        const i32 slide = dir * ((((m * wild) >> 10) * 7 * sn(ph(t, 900) + 344)) >> 10);
        const i32 tilt = lean + rt * 3600 / 8192;
        const i32 tc = cs(tilt), ts = sn(tilt);
        rg.at(0, -58);                                                   /* his seat, on the back, his belt at the plate's top edge */
        const i32 sx = rg.x + slide, sy = rg.y - up;
        /* a point in his frame (cowboy units about the seat, Q10 px, under the tilt) */
        struct Cow { i32 sx, sy, c, s; i32 x, y;
            void at(i32 lx, i32 ly) { const i32 rx = (((lx * CS) >> 10) * RIG) >> 10, ry = ((((ly + 52) * CS) >> 10) * RIG) >> 10; x = sx + (rx * c - ry * s); y = sy + (rx * s + ry * c); } } cw = { sx, sy, tc, ts, 0, 0 };
        /* legs: the thigh from the hip to a knee that swings with each throw, the shin to an ankle that swings more */
        const i32 kick = (m * (4096 + ((6 * 1024 * e) >> 10) + ((9 * wild * e) >> 10))) >> 10;
        for (int sd = -1; sd <= 1; sd += 2) {
            const u32 side = sd < 0 ? 0 : 1;
            const i32 p0 = ph(tt, 1300) + (sd < 0 ? 975 : 0), p1 = p0 + 460;
            /* the OP-1's joints, cowboy units x 1024: hip (7, -55), knee (24 + .., -50 - ..), ankle (33 + .., -38 - ..) */
            const i32 kx = sd * (24 * unit + ((sn(p0) * ((kick * 2) / 5)) >> 10)), ky = -50 * unit - ((cs(p0) * ((kick * 7) / 20)) >> 10) - 3 * m;
            const i32 ax = sd * (33 * unit + ((sn(p1) * ((kick * 3) / 4)) >> 10)), ay = -38 * unit - ((cs(p1) * ((kick * 7) / 10)) >> 10) - 4 * m;
            const i32 base = sd < 0 ? 1800 : 0;                                                   /* the left leg is authored along -x */
            const i32 ta = tilt + angle_of((kx - sd * 7 * unit) >> 6, (ky + 55 * unit) >> 6);      /* true directions on screen */
            const i32 sa = tilt + angle_of((ax - kx) >> 6, (ay - ky) >> 6);
            cw.at(sd * 7, -55);
            const i32 hx = cw.x, hy = cw.y;
            place(fr, N_THIGH + side, hx, hy, RIG, B_SEC, stroke); turn(fr, N_THIGH + side, rotation(ta + base));
            i32 jx, jy; along(hx, hy, (((17700 * CS) >> 10) * RIG) >> 10, ta, jx, jy);
            place(fr, N_SHIN + side, jx, jy, RIG, B_SEC, stroke); turn(fr, N_SHIN + side, rotation(sa + base));
            /* the boot hangs upright from the ankle in his frame; the spur sits at its heel, spinning with his own clock */
            i32 ex, ey; along(jx, jy, (((15000 * CS) >> 10) * RIG) >> 10, sa, ex, ey);
            place(fr, N_BOOT + side, ex, ey, RIG, B_SEC, stroke); turn(fr, N_BOOT + side, rotation(tilt));
            const i32 spx = -sd * ((((6600 * 1792) >> 10) * CS >> 10) * RIG >> 10), spy = (((6500 * 1792) >> 10) * CS >> 10) * RIG >> 10;
            place(fr, N_SPUR + side, ex + ((spx * tc - spy * ts) >> 10), ey + ((spx * ts + spy * tc) >> 10), RIG, S_EDGE, stroke);
            turn(fr, N_SPUR + side, rotation(still ? 0 : ph(tt, 2000) % 3600));
        }
        place(fr, N_TORSO, sx, sy, RIG, B_SEC, stroke); turn(fr, N_TORSO, rotation(tilt));
        place(fr, N_BELT, sx, sy, RIG, B_DEC, stroke); turn(fr, N_BELT, rotation(tilt));
        place(fr, N_HEAD, sx, sy, RIG, B_SEC, stroke); turn(fr, N_HEAD, rotation(tilt));
        /* his face: eyes wide with CUT and wider as he is thrown (a scale about the head's centre) */
        cw.at(0, -81);
        place(fr, N_FACE, cw.x, cw.y, (RIG * (900 + (cut * 300 >> 10) + (m * 450 >> 10))) >> 10, S_LITE, stroke); turn(fr, N_FACE, rotation(tilt));
        /* hat: brim wider with SPREAD, lifting off on the hardest throws */
        {
            const i32 hl = ((((m * e) >> 10) * (2048 + ((1536 * wild) >> 10))) >> 10) + 600;    /* cowboy units x 1024 */
            cw.at(0, -87);
            const i32 hx = cw.x - ((ts * ((hl * CS >> 10) * RIG >> 10)) >> 10), hy = cw.y - ((tc * ((hl * CS >> 10) * RIG >> 10)) >> 10);
            place(fr, N_BRIM, hx, hy, (RIG * (820 + (sprd * 460 >> 10))) >> 10, B_LITE, stroke); turn(fr, N_BRIM, rotation(tilt - (shape * 60 >> 10) * dir));
            place(fr, N_CROWN, hx, hy, RIG, B_LITE, stroke); turn(fr, N_CROWN, rotation(tilt));
        }
        /* the lasso arm: rest at RES 0; up with a loop that grows and spins as RES rises */
        {
            const i32 tau = 123 + ((1434 * res) >> 10), dec = expneg(tq * 1024 / tau);
            const i32 ampl = 102 + ((256 * res) >> 10) + ((((m * (256 + ((307 * wild) >> 10))) >> 10) * res) >> 10);
            const i32 wave = ((sn(ph(tt, 1000 + 600 * wild / 1024)) * ampl) >> 10) * 1304 >> 10;       /* tenths-ish */
            /* the OP-1's elbow and hand, cowboy units x 1024 */
            const i32 elx = lerp(12500, 18000, res), ely = lerp(-64000, -80000, res);
            const i32 hx = lerp(11000, 25000, res) + ((sn(wave) * ((4000 * res) >> 10)) >> 10);
            const i32 hy = lerp(-57000, -88000, res) - ((cs(wave) * ((1500 * res) >> 10)) >> 10) + ((sn(wave) * ((1200 * (1024 - res)) >> 10)) >> 10);
            const i32 ua = tilt + angle_of((elx - 8600) >> 6, (ely + 71000) >> 6), fa = tilt + angle_of((hx - elx) >> 6, (hy - ely) >> 6);
            cw.at(8600 >> 10, -71);
            const i32 shx = cw.x, shy = cw.y;
            place(fr, N_UPPER, shx, shy, RIG, B_SEC, stroke); turn(fr, N_UPPER, rotation(ua));
            i32 ex, ey; along(shx, shy, (((9000 * CS) >> 10) * RIG) >> 10, ua, ex, ey);
            place(fr, N_FORE, ex, ey, RIG, B_SEC, stroke); turn(fr, N_FORE, rotation(fa));
            i32 px, py; along(ex, ey, (((10000 * CS) >> 10) * RIG) >> 10, fa, px, py);                   /* the hand */
            if (res > 31) {
                const i32 R = (((3000 + res * 29) * (614 + ((410 * dec) >> 10))) >> 10) * CS >> 10;    /* loop radius, cowboy units x 1024 -> rig x 1024 */
                const i32 Rp = (R * RIG) >> 10;                                                       /* Q10 px */
                const i32 spin = still ? 0 : static_cast<i32>((st.clock * (3 + (res >> 7))) % 3600u);
                const i32 lx = px + (Rp * 6 / 10), ly = py - ((2 * CS * RIG) >> 10) - Rp * 12 / 100;  /* the loop's centre */
                const i32 ry = Rp * 42 / 100;
                const u32 key = 1u + static_cast<u32>(Rp >> 8) * 64u + static_cast<u32>(spin >> 4);
                if (fr.paths[N_LASSO].key != key) {
                    for (u32 i = 0; i < 64; ++i) {
                        const i32 ang = static_cast<i32>(i) * 3600 / 64;
                        const i32 wob = (dec * (sn(ang * 3 + spin) * 2)) >> 10;
                        fr.paths[N_LASSO].points[i].x = clamp(((cs(ang) * Rp) >> 10) + ((wob * cs(ang)) >> 10), -63 * unit, 63 * unit);
                        fr.paths[N_LASSO].points[i].y = clamp(((sn(ang) * ry) >> 10) + ((wob * sn(ang)) >> 10), -63 * unit, 63 * unit);
                    }
                    fr.paths[N_LASSO].key = key;
                }
                place(fr, N_LASSO, lx, ly, unit, S_LITE, stroke); turn(fr, N_LASSO, rotation(0));
                /* the knot rides the loop; the rope is a quadratic from the hand to it, reshaped as a thin ribbon */
                const i32 kx = lx + ((cs(spin) * Rp) >> 10), ky = ly + ((sn(spin) * ry) >> 10);
                place(fr, N_KNOT, kx, ky, unit, S_LITE, stroke);
                const i32 mx = (px + kx) / 2, my = (py + ky) / 2;
                const i32 qx = mx + 2 * unit, qy = (py > ky ? py : ky) + 2 * unit;
                const u32 rkey = 1u + static_cast<u32>((kx - px) >> 9) * 4096u + static_cast<u32>((ky - py) >> 9);
                if (fr.paths[N_ROPE].key != rkey) {
                    for (u32 i = 0; i < 32; ++i) {
                        const i32 s = static_cast<i32>(i) * 1024 / 31, u = 1024 - s;
                        const i32 x = ((((u * u) >> 10) * (px - mx)) >> 10) + ((((2 * u * s) >> 10) * (qx - mx)) >> 10) + ((((s * s) >> 10) * (kx - mx)) >> 10);
                        const i32 y = ((((u * u) >> 10) * (py - my)) >> 10) + ((((2 * u * s) >> 10) * (qy - my)) >> 10) + ((((s * s) >> 10) * (ky - my)) >> 10);
                        fr.paths[N_ROPE].points[i].x = clamp(x, -63 * unit, 63 * unit); fr.paths[N_ROPE].points[i].y = clamp(y - 400, -63 * unit, 63 * unit);
                        fr.paths[N_ROPE].points[63 - i].x = clamp(x, -63 * unit, 63 * unit); fr.paths[N_ROPE].points[63 - i].y = clamp(y + 400, -63 * unit, 63 * unit);
                    }
                    fr.paths[N_ROPE].key = rkey;
                }
                place(fr, N_ROPE, mx, my, unit, S_LITE, stroke); turn(fr, N_ROPE, rotation(0));
            } else { hide(fr, N_LASSO); hide(fr, N_ROPE); hide(fr, N_KNOT); }
        }
    }
    /* APHX (page 2): glitch slices tear across the rig on each note; the layout follows the note's seed */
    {
        const i32 life = 140 + ((aphx * 560) >> 10);
        const i32 n = aphx > 10 ? 2 + ((aphx * 6) >> 10) : 0;
        const i32 jit = unit + ((((aphx * aphx) >> 10) * 9 * unit) >> 10);
        u32 s = 2166136261u ^ (st.seed * 16777619u);
        const u32 fl = st.clock / 33;
        for (i32 i = 0; i < 8; ++i) {
            if (still || i >= n || t >= life) { hide(fr, N_TEAR + i); continue; }
            s = hash(s); const i32 y = 60 + static_cast<i32>((s >> 16) % 118u);
            s = hash(s); const i32 x = cx - 75 + static_cast<i32>((s >> 16) % 120u);
            s = hash(s); const i32 len = clamp(512 + static_cast<i32>((s >> 16) % 700u) + (aphx * 820 >> 10), 512, 2048);
            const u32 qq = (s >> 8) ^ (fl * 2654435761u);
            const i32 off = static_cast<i32>((qq >> 20) % 2048u) - 1024;
            const i32 dx = (off * (jit >> 5)) >> 5;
            place(fr, N_TEAR + i, x * unit + dx, y * unit, len, S_WHITE, fill_and_stroke);
        }
    }
    for (u32 i = N_SPARE; i < Frame::node_count; ++i) hide(fr, i);
}
}
