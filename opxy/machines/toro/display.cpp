/* toro for the OP-XY: the BLOCKS screen. A field of stacked isometric slabs whose heights are a small model of the
 * voice, so turning a knob reshapes the field the way it reshapes the sound.
 *   Spread  the second oscillator's ratio: a second, slower wave runs across the field at that ratio and the two
 *           beat into a moiré of ridges
 *   Fold    the wavefolder: heights fold back on themselves, and the field wraps round a drum, almost closed at 100;
 *           as it wraps, the voice's relief sinks into the drum's surface and notes push blocks out of it
 *   Cutoff  low leaves the two broad waves; high adds a fine ripple of harmonics on top
 *   Res     one row of tops lights up, moving front to back with the setting
 *   Shape   (page 2) saw to triangle: the main wave's ridges go from a sharp leading edge to symmetric
 *   Noise   (page 2) a fixed jitter in every column
 *   Drive   (page 2) soft clip: the tallest columns flatten to a plateau
 *   Aphx    (page 2) drift: each column wanders by its own amount, different for each note's landing spot
 *   a note  rolls the wave forward a step, pushes a patch of six slabs up with white tops, and sends a ripple out
 *           across the field; everything settles in under a second, and the idle field doesn't move
 * The drawing input carries no pitch, so notes land on eight spots in turn rather than by key.
 *
 * The art is retained (panel.svg.fragment from gen_panel.py; tables and indices in nodes.h). Every column is three
 * nodes authored at one origin, so a column only ever translates. Integer maths only: Q10 for the voice model, Q14
 * radians for the drum, where small angles need the precision. Copyright (c) 2026 briannhinton. MIT. */
#include "display.h"
#include "nodes.h"
#include <opxy/q31.hpp>

namespace toro {
namespace {
using opxy::i32;
using opxy::i64;
using opxy::u32;
using namespace opxy::draw;

const i32 Q14 = 16384, PI_Q14 = 51472, HALF_PI_Q14 = 25736;
const i32 H_MIN = 8, H_SPAN = 32;                                                 /* column heights 8..40 px */
const i32 HOLLOW_Q10 = (H_MIN * 1024) + (H_SPAN * 1024 * 35) / 100;               /* below 35 % of the range: hollow */

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
inline i32 isqrt(u32 v) { u32 r = 0, b = 1u << 30; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return static_cast<i32>(r); }
/* the wavefolder: reflect at ±1 until inside, as a closed form with period 4 */
inline i32 fold_q10(i32 s)
{
    i32 t = (s + unit) % (4 * unit);
    if (t < 0) t += 4 * unit;
    return t < 2 * unit ? t - unit : 3 * unit - t;
}
/* sin(x)/x for |x| <= pi, x and result Q14. Taylor to x^8 within pi/2, and sin(pi - x) / x beyond */
inline i32 sinc_q14(i32 x)
{
    x = iabs(x);
    if (x <= HALF_PI_Q14) {
        const i32 x2 = (x * x) >> 14;
        i32 p = Q14 - x2 / 72;
        p = Q14 - ((x2 * p) >> 14) / 42;
        p = Q14 - ((x2 * p) >> 14) / 20;
        return Q14 - ((x2 * p) >> 14) / 6;
    }
    const i32 y = PI_Q14 - x;
    const i32 s = (y * sinc_q14(y)) >> 14;
    return (s << 14) / x;
}
}

inline void display_reset(DisplayState& st)
{
    st.header.reset(); st.notes.reset();
    st.serial = 0; st.t_ms = 60000; st.n = 0; st.initialized = 0;
}

inline void display(DisplayState& st, const Input& in, Frame& fr)
{
    /* header: the SDK's two pages, values as 0..100 except Spread, a ratio */
    const char* const titles[8] = { "Spread", "Fold", "Cutoff", "Res", "Shape", "Noise", "Drive", "Aphx" };
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, in.slots[i]);
    { const i32 r = 100 + ((q10(in.slots[0]) * 50 + 512) >> 10); values[0][0] = '1'; values[0][1] = '.'; values[0][2] = static_cast<char>('0' + (r / 10) % 10); values[0][3] = static_cast<char>('0' + r % 10); values[0][4] = 0; }
    const opxy::u8 slots[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 } };
    page_header(st.header, in, fr, titles, values, N_HDR, slots);

    /* notes: the serial changes once per note start */
    const i32 dt = clamp(in.elapsed * 1000 / unit, 1, 100);
    st.notes.update(in);
    if (!st.initialized) { st.serial = in.note_serial; st.initialized = 1; }
    else if (st.serial != in.note_serial) { st.serial = in.note_serial; st.t_ms = 0; st.n = (st.n + 1) & 0xFFFFFu; }
    else if (st.t_ms < 60000) st.t_ms += dt;
    const bool still = in.reduced_motion != 0;
    const bool played = st.n != 0;
    const i32 tq = st.t_ms * unit / 1000;                                         /* s since the note, Q10 */
    const u32 key = played ? (st.n - 1) % KEY_SPOT_N : 0;

    const i32 spread = q10(in.slots[0]), fold = q10(in.slots[1]), cut = q10(in.slots[2]), res = q10(in.slots[3]);
    const i32 shape = q10(in.slots[4]), noise = q10(in.slots[5]), drive = q10(in.slots[6]), aphx = q10(in.slots[7]);

    /* a note: the wave rolls forward 0.11 of a cycle, easing in over ~0.2 s; the patch lifts 22 px and settles; tops
     * flash for 120 ms; a ripple spreads 14 columns a second and fades */
    const i32 step = 113;                                                          /* 0.11 cycle, Q10 */
    const i32 ease = still ? unit : unit - expneg((tq * 4655) >> 10);             /* 1 - e^(-t/0.22) */
    const i32 phase = played ? static_cast<i32>(st.n - 1) * step + ((step * ease) >> 10) : 0;
    const bool live = played && !still && st.t_ms < 3000;
    const i32 lift = live ? 22 * expneg((tq * 5689) >> 10) : 0;                   /* Q10 px, 22 e^(-t/0.18) */
    const bool flash = live && st.t_ms < 120;
    const i32 ripple_fade = live ? expneg((tq * 1862) >> 10) : 0;                  /* e^(-t/0.55) */
    const i32 ci = KEY_I[key], cj = KEY_J[key];

    /* the voice */
    const i32 ratio = unit + spread / 2;                                           /* 1 .. 1.5 */
    const i32 gain = unit + 7 * fold;                                              /* 1 .. 8 */
    const i32 ceiling = unit - ((614 * drive) >> 10);                              /* Drive: clip at 1 - 0.6 drive */
    const i32 a3 = (((aphx * aphx) >> 10) * aphx) >> 10;
    const i32 drift = (((410 * aphx) >> 10) + ((614 * a3) >> 10)) * 461 >> 10;    /* 0.45 (0.4a + 0.6a^3) */
    const i32 key_turn = static_cast<i32>(key) * 974;                              /* 1.7 rad per landing spot, tenths */
    const i32 lit_row = res > 51 ? (6 * res + 512) >> 10 : -1;

    /* the drum: the field bends through 1.9 pi c about its middle row, c easing in from Fold 0.05. Per row i:
     * psi = (2i - 9) A / 20; R sin psi and R (1 - cos psi) are written as sinc products so they stay exact as A -> 0 */
    i32 c;                                                                         /* Q10 */
    {
        const i32 s = fold > 51 ? ((fold - 51) * 1078) >> 10 : 0;                  /* (fold - 0.05) / 0.95 */
        c = (unit - sine(((clamp(s, 0, unit) * 1800) >> 10) + 900)) / 2;          /* 0.5 - 0.5 cos(pi s) */
    }
    const i32 A = (c * 97797) >> 10;                                               /* 1.9 pi c, Q14 radians */
    const i64 L = 10 * (static_cast<i64>(Q14) + c * 8);                            /* rows, Q14: 10 (1 + 0.5 c) */
    const i32 sq4 = sinc_q14(A / 4);
    const i64 lift_rows = ((L / 2) * (A / 4) >> 14) * ((static_cast<i64>(sq4) * sq4) >> 14) >> 14;   /* R (1 - cos(A/2)) */
    i64 along_r[NX], up_r[NX];                                                      /* rows Q14, px Q14 */
    i32 sin_p[NX], cos_p[NX];                                                       /* Q14 */
    for (i32 i = 0; i < NX; ++i) {
        const i32 psi = (2 * i - 9) * A / 20;
        const i32 sp = sinc_q14(psi), sh = sinc_q14(psi / 2);
        const i64 lp = L * (2 * i - 9) / 20;                                       /* rows, Q14 */
        const i32 omc = static_cast<i32>(((static_cast<i64>(psi) * psi >> 15) * ((static_cast<i64>(sh) * sh) >> 14)) >> 14);   /* 1 - cos psi */
        along_r[i] = (lp * sp) >> 14;
        const i64 r1mc = (lp * (psi / 2) >> 14) * ((static_cast<i64>(sh) * sh) >> 14) >> 14;
        up_r[i] = (lift_rows - r1mc) * 168 / 10;                                   /* 16.8 px per row */
        sin_p[i] = static_cast<i32>((static_cast<i64>(psi) * sp) >> 14);
        cos_p[i] = Q14 - omc;
    }
    const i32 shrink = unit - ((942 * c) >> 10);                                   /* 1 - 0.92 c: the relief sinks */
    const i64 drop = static_cast<i64>(14) * c << 4;                                /* the drum settles 14 c px, Q14 */

    for (i32 k = 0; k < COL_N; ++k) {
        const i32 i = COL_I[k], j = COL_J[k];
        /* the voice at this column: saw/triangle plus the second wave, the harmonics, the fold, drive, noise, drift */
        const i32 ph = (COL_P1[k] + phase) & 1023;
        const i32 saw = 2 * ph - unit, tri = unit - 4 * iabs(ph - 512);
        const i32 osc1 = (saw * (unit - shape) + tri * shape) >> 10;
        const i32 cyc = static_cast<i32>((static_cast<i64>(ratio) * (COL_P2[k] + phase)) >> 10) & 1023;
        const i32 osc2 = (614 * sine(((cyc * 3600) >> 10) + 573)) >> 10;          /* 0.6 sin(2 pi cyc + 1) */
        const i32 raw = ((461 * (osc1 + osc2)) >> 10) + ((cut * COL_DETAIL[k]) >> 10);
        i32 hv = fold > 20 ? fold_q10((gain * raw) >> 10) : clamp(raw, -unit, unit);
        if (hv > ceiling) hv = ceiling;
        hv += (noise * COL_NOISE[k]) >> 10;
        hv += (drift * sine(COL_APHX[k] + key_turn)) >> 10;
        hv = clamp(hv, -unit, unit);
        const i32 hs = H_MIN * unit + (H_SPAN / 2) * (hv + unit);                  /* Q10 px */

        /* notes: the patch (the spot, its four neighbours and the one in front) and the ripple */
        const i32 di = i - ci, dj = j - cj;
        const bool patch = (iabs(di) + iabs(dj) <= 1) || (di == 1 && dj == 1);
        i32 h = H_MIN * unit + (((hs - H_MIN * unit) * shrink) >> 10);
        if (patch) h += (di == 0 && dj == 0) ? lift : (lift * 614) >> 10;
        if (ripple_fade) {
            const i32 d = isqrt(static_cast<u32>(di * di + dj * dj) << 20);       /* columns, Q10 */
            const i32 a = clamp(((d - 14 * tq) * 788) >> 10, -4 * unit, 4 * unit);   /* / 1.3 */
            h += (11 * ((expneg((a * a) >> 10) * ripple_fade) >> 10));
        }
        const i32 hp = (h + 512) >> 10;                                            /* whole pixels */

        /* where the top sits: flat at Fold 0, on the drum's surface as it wraps; height stands out along the normal */
        const i64 along = along_r[i] + static_cast<i64>(hp) * sin_p[i] * 10 / 168;  /* rows, Q14 */
        const i64 up = up_r[i] + static_cast<i64>(hp) * cos_p[i];                  /* px, Q14 */
        const i64 x14 = static_cast<i64>(ORIGIN_X2) * (Q14 / 2) + (static_cast<i64>(9 - 2 * j) * CUBE * Q14) / 2 + along * CUBE;
        const i64 y14 = static_cast<i64>(ORIGIN_Y2) * (Q14 / 2) + (static_cast<i64>(9 + 2 * j) * CUBE * Q14) / 4 + along * CUBE / 2 - up + drop;
        const i32 x = static_cast<i32>((x14 + Q14 / 2) >> 14) * unit;
        const i32 y = clamp(static_cast<i32>((y14 + Q14 / 2) >> 14), 38, 400) * unit;   /* never into the header band */

        /* shade: height bands, the Res row lit, the drum's underside dark, a struck patch white */
        u32 top = hp > 30 ? P_TOP_HI : hp > 18 ? P_TOP_MID : P_TOP_LO;
        if (j == lit_row) top = P_LIT;
        if (cos_p[i] < -(Q14 / 5)) top = P_TOP_LO;
        if (flash && patch) top = P_FLASH;
        /* the quiet part of the wave is hollow, decided on the voice's own height so it follows the sound */
        const bool wire = (hs < HOLLOW_Q10 || COL_WIRE[k]) && top != P_FLASH && top != P_LIT;
        const u32 n = N_COL + 3 * static_cast<u32>(k);
        place(fr, n, x, y, unit, wire ? P_WIRE : P_LEFT, fill_and_stroke);
        place(fr, n + 1, x, y, unit, wire ? P_WIRE : P_RIGHT, fill_and_stroke);
        place(fr, n + 2, x, y, unit, wire ? P_WIRE_TOP : top, fill_and_stroke);
    }
}
}
