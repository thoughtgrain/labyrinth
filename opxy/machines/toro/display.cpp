/* toro for the OP-XY: the RIDER screen. A robot bull on a hydraulic ram, head-on, with a cowboy aboard. The art
 * is retained: panel.svg.fragment (from gen_panel.py) holds every part, and this code places, turns, scales,
 * repaints or hides each one per frame. Greyscale only, so each control keeps its own shape, as on the OP-1:
 *   Spread  the horns spread, and the hat brim with them       Shape (page 2)  the horn tips curl up
 *   Fold    each note lifts the ram and rocks the rig; the cowboy bounces and his legs kick; a pulse runs the hose
 *           Noise (page 2)  sparks off the turntable while a note is held
 *   Cutoff  the eye slits open; a note flashes a glow round them when it is up
 *           Drive (page 2)  steam from the vents on each note
 *   Res     each note strikes the nose ring; the lasso arm rises with a loop that grows and spins longer
 *           Aphx (page 2)  glitch slices tear across the rig on each note; the same note always tears the same
 * Motion follows the OP-XY's note activity (note_serial, held_notes): nothing moves without a note. The two-page
 * header is the SDK's. Copyright (c) 2026 briannhinton. MIT. */
#include "display.h"
#include <opxy/q31.hpp>

namespace toro {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

const i32 CX = 240, BASE = 208, TOP0 = 186;          // the ram's foot and its resting top, pixels
const i32 RIG = 1382;                                 // rig scale, Q10 (1.35)
enum { N_LASSO = 0, N_RIPPLE = 1, N_HDR = 2, N_SHADOW = 10, N_ROD = 11, N_RIG = 12, N_HORN_L = 13, N_HORN_R = 14,
       N_EYE_L = 15, N_EYE_R = 16, N_RING = 17, N_GLOW_L = 18, N_GLOW_R = 19, N_STEAM = 20, N_SPARK = 24, N_PULSE = 30,
       N_TORSO = 31, N_HAT = 32, N_LEG_L = 33, N_LEG_R = 34, N_ARM = 35, N_TEAR = 36, N_FACETS = 44, N_FACE = 45, N_BODY = 46, N_SPARE = 47 };
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
// a point rotated by the rig's lean (tenths of a degree, clockwise) about the ram's top, in Q10 pixels
inline void rig_pt(i32 lx, i32 ly, i32 lean, i32 cx, i32 top, i32& x, i32& y)
{
    const i32 c = cs(lean), s = sn(lean);
    const i32 rx = (lx * RIG) >> 10, ry = (ly * RIG) >> 10;
    x = cx * unit + ((rx * c - ry * s));
    y = top * unit + ((rx * s + ry * c));
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
    const bool fresh = !st.initialized || st.serial != in.note_serial;
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) { st.serial = in.note_serial; st.t_ms = 0; st.n++; st.seed = hash(st.seed + st.n); }
    else if (st.t_ms < 60000) st.t_ms += dt;
    st.clock += static_cast<u32>(dt);
    const bool held = in.held_notes != 0 && in.held_notes <= 128;
    const bool still = in.reduced_motion;
    const i32 t = still ? 9000 : st.t_ms;
    const i32 tq = t * unit / 1000;
    (void)fresh;

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

    /* the ram and its shadow */
    place(fr, N_ROD, cx * unit, (BASE - 8) * unit, clamp(((BASE - 8) - top) * unit / 16, unit / 2, 2 * unit), B_SEC, fill_and_stroke);
    turn(fr, N_ROD, rotation(lean / 2));
    { const i32 srx = 40 - ((BASE - top) * 2) / 5; place(fr, N_SHADOW, (cx + ((a * 27) >> 10)) * unit, (BASE + 6) * unit, clamp(srx * unit / 30, unit / 2, 2 * unit), S_BLACK, fill_and_stroke); }
    /* the rig */
    place(fr, N_BODY, cx * unit, top * unit, RIG, B_SEC, fill_and_stroke); turn(fr, N_BODY, rotation(lean));
    place(fr, N_RIG, cx * unit, top * unit, RIG, B_SEC, fill_and_stroke); turn(fr, N_RIG, rotation(lean));
    place(fr, N_FACETS, cx * unit, top * unit, RIG, S_DEC, stroke); turn(fr, N_FACETS, rotation(lean));
    /* SPREAD: the horns spread wider; SHAPE curls the tips (the tips are drawn rising, so curl reads as more turn) */
    {
        const i32 spread_tenths = 150 + (sprd * 450 >> 10) + (shape * 250 >> 10);
        i32 x, y;
        rig_pt(-34, -66, lean, cx, top, x, y); place(fr, N_HORN_L, x, y, unit, B_LITE, fill_and_stroke); turn(fr, N_HORN_L, rotation(lean - spread_tenths));
        rig_pt(34, -66, lean, cx, top, x, y);  place(fr, N_HORN_R, x, y, unit, B_LITE, fill_and_stroke); turn(fr, N_HORN_R, rotation(lean + spread_tenths));
    }
    /* CUT: the eye slits open (scale), and a note flashes a glow round them while CUT is up */
    {
        const i32 open_ = 512 + (cut * 768 >> 10);
        const bool glow = !still && t < 160 && cut > 600;
        for (int sd = -1; sd <= 1; sd += 2) {
            i32 x, y; rig_pt(sd * 19, -60, lean, cx, top, x, y);
            place(fr, sd < 0 ? N_EYE_L : N_EYE_R, x, y, open_, S_LITE, fill_and_stroke); turn(fr, sd < 0 ? N_EYE_L : N_EYE_R, rotation(lean + sd * 80));
            if (glow) { place(fr, sd < 0 ? N_GLOW_L : N_GLOW_R, x, y, unit + (open_ >> 1), S_EDGE, stroke); turn(fr, sd < 0 ? N_GLOW_L : N_GLOW_R, rotation(lean + sd * 80)); }
            else hide(fr, sd < 0 ? N_GLOW_L : N_GLOW_R);
        }
    }
    /* DRIVE (page 2): each note blasts steam from the vents */
    for (u32 i = 0; i < 4; ++i) {
        const i32 sd = (i & 1) ? 1 : -1, k = static_cast<i32>(i >> 1);
        if (still || drive < 20 || t >= 600 || k >= 1 + ((drive * 3 + 512) >> 10)) { hide(fr, N_STEAM + i); continue; }
        const i32 tt = clamp(t * 1024 / 600 + k * 184, 0, 1024);
        i32 x, y; rig_pt(sd * (36 + ((tt * (20 + (drive * 10 >> 10))) >> 10)), -48 - (tt * 10 >> 10), lean, cx, top, x, y);
        place(fr, N_STEAM + i, x, y, unit + tt, S_SEC, stroke);
    }
    /* RES: each note strikes the nose ring; resonance sets how long it keeps swinging */
    {
        const i32 tau = 123 + ((1434 * res) >> 10), dec = expneg(tq * 1024 / tau);
        const i32 sw = (((717 * dec) >> 10) * sn(ph(t, 1400))) >> 10;                            // 1/8192 turn
        const i32 R = unit + ((res * 5) >> 3);                                                   // ring scale
        i32 x, y; rig_pt(0, -34, lean, cx, top, x, y);               /* hangs from the muzzle */
        place(fr, N_RING, x, y, R, S_LITE, stroke); turn(fr, N_RING, rotation(lean + sw * 3600 / 8192));
    }
    /* FOLD: the hose pulse runs from the hose end to the ram on each note */
    if (!still && t < 400) {
        const i32 u = unit - t * unit / 400;                                                     // 0 at the hose end, 1 at the ram
        const i32 hx = 400 - ((130 * u) >> 10), hy = (BASE - 2) + ((8 * sn(u * 1800 / unit)) >> 10);
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
    /* the cowboy: thrown by FOLD, his legs kick, the hat follows SPREAD, the lasso follows RES */
    {
        const i32 m = clamp(((fold * (205 + ((922 * e) >> 10))) >> 10) * 1280 >> 10, 0, 1024);
        const i32 wild = (fold * fold) >> 10;
        const i32 rt = -((a * (1843 + 2 * wild)) >> 10) + dir * ((((m * (123 + ((410 * wild) >> 10))) >> 10) * 1304 >> 10) * sn(ph(t, 1100)) >> 10);
        const i32 up = ((sn(ph(t, 900)) < 0 ? -sn(ph(t, 900)) : sn(ph(t, 900))) * ((m * (4608 + 6 * wild)) >> 10)) >> 10;   // Q10 px
        const i32 slide = dir * ((((m * wild) >> 10) * 7 * sn(ph(t, 900) + 344)) >> 10);
        const i32 tilt = lean + rt * 3600 / 8192;
        i32 sx, sy; rig_pt(0, -42, lean, cx, top, sx, sy);           /* astride the back, behind the faceplate: his torso and hat rise above it */
        sx += slide; sy -= up;
        place(fr, N_TORSO, sx, sy, RIG, B_SEC, fill_and_stroke); turn(fr, N_TORSO, rotation(tilt));
        place(fr, N_FACE, sx, sy, RIG, S_LITE, fill_and_stroke); turn(fr, N_FACE, rotation(tilt));
        /* hat: on the head, brim scaled by SPREAD, bigger than he is */
        { const i32 c = cs(tilt), s = sn(tilt); const i32 hx = sx + ((-s * 52 * RIG) >> 10), hy = sy + ((c * -52 * RIG) >> 10);
          place(fr, N_HAT, hx, hy, 1024 + (sprd * 600 >> 10), B_LITE, fill_and_stroke); turn(fr, N_HAT, rotation(tilt)); }
        /* legs: kick with each throw */
        const i32 kick = (m * (4096 + ((6 * 1024 * e) >> 10) + ((9 * wild * e) >> 10))) >> 10;
        for (int sd = -1; sd <= 1; sd += 2) {
            const i32 p0 = ph(t, 1300) + (sd < 0 ? 975 : 0);
            const i32 swing = (sn(p0) * ((kick * 2) / 5)) >> 10;                                 // Q10 angle-ish
            place(fr, sd < 0 ? N_LEG_L : N_LEG_R, sx + sd * ((9 * RIG) >> 10) * unit, sy, RIG, B_SEC, fill_and_stroke);
            turn(fr, sd < 0 ? N_LEG_L : N_LEG_R, rotation(tilt + sd * (swing * 400 >> 10)));
        }
        /* the lasso arm and loop: rest at RES 0; up with a loop that grows and spins as RES rises */
        {
            const i32 tau = 123 + ((1434 * res) >> 10), dec = expneg(tq * 1024 / tau);
            const i32 raise = res < 410 ? 0 : clamp((res - 410) * 1024 / 614, 0, 1024);   /* the arm rests below RES 40 */
            const i32 arm_tenths = tilt + 300 - (raise * 1200 >> 10);                           // down-right at rest, up and out at full
            const i32 c = cs(tilt), s = sn(tilt);
            const i32 ax = sx + (((c * 10 - s * -24) * RIG) >> 10), ay = sy + (((s * 10 + c * -24) * RIG) >> 10);
            place(fr, N_ARM, ax, ay, RIG, S_SEC, stroke); turn(fr, N_ARM, rotation(arm_tenths));
            if (raise > 0) {
                const i32 ac = cs(arm_tenths), as = sn(arm_tenths);
                const i32 lx = ax + (((ac * 26) * RIG) >> 10), ly = ay + (((as * 26) * RIG) >> 10);
                const i32 R = 8 + (raise * 44 >> 10) + ((dec * (raise * 12 >> 10)) >> 10);
                const i32 spin = still ? 0 : static_cast<i32>((st.clock * (3 + (res >> 7))) % 3600u);
                /* reshape the loop: an ellipse seen at an angle, a little wobble while the ring swings */
                const u32 key = 1u + static_cast<u32>(R) * 4096u + static_cast<u32>(spin >> 4);
                if (fr.paths[N_LASSO].key != key) {
                    for (u32 i = 0; i < 64; ++i) {
                        const i32 ang = static_cast<i32>(i) * 3600 / 64;
                        const i32 px = (cs(ang) * R) >> 0, py = (sn(ang) * R * 3 / 4) >> 0;
                        const i32 wob = (dec * (sn(ang * 3 + spin) * 2)) >> 10;
                        fr.paths[N_LASSO].points[i].x = clamp(px + ((wob * cs(ang)) >> 10), -63 * unit, 63 * unit);
                        fr.paths[N_LASSO].points[i].y = clamp(py + ((wob * sn(ang)) >> 10), -63 * unit, 63 * unit);
                    }
                    fr.paths[N_LASSO].key = key;
                }
                place(fr, N_LASSO, lx + (R * unit >> 1), ly - ((R * 3 / 4) * unit), unit, S_LITE, stroke); turn(fr, N_LASSO, rotation(spin / 6));
            } else hide(fr, N_LASSO);
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
    hide(fr, N_RIPPLE);
    for (u32 i = N_SPARE; i < Frame::node_count; ++i) hide(fr, i);
}
}
