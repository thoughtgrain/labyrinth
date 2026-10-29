/* knscr.h: small fixed-point drawing kit for the meikyu and toro screens (GUI thread only).
 * Everything is integer math: coordinates in Q10 screen units (OP1_Q10), angles in 1/8192 of a turn, sines in Q15,
 * 2D transforms in Q12. Drawing goes through sq_line / sq_text (core exports, seq/seq_text.h), so the same code runs
 * on the device and in the host preview (screen_host.c), where op1_raster_line is redirected to an SVG writer.
 * Fills are scanline runs of 1-unit lines: the raster only draws lines, and the shapes filled here are small.
 * The two plugins each carry an identical copy (a .knl file is self-contained).
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#ifndef KNSCR_H
#define KNSCR_H
#include <stdint.h>

#define KS_FN static __attribute__((unused))     /* each screen uses a subset */
#define KS_ONE    1024                              /* Q10 */
#define KS_TURN   8192                              /* angle units per turn */
#define KS_RAD(r) ((int32_t)((r) * 1303.797f))      /* constant radians -> angle units (compile time only) */

/* ---- sine: the firmware's 8192-entry Q15 table, or a host copy ---- */
#ifdef OP1_HOST
#include <math.h>
static int16_t ks_tab[KS_TURN];
KS_FN void ks_init_host(void) { for (int i = 0; i < KS_TURN; i++) ks_tab[i] = (int16_t)lround(32767.0 * sin(i * 6.283185307179586 / KS_TURN)); }
#define KS_SINE ks_tab
#else
#define ks_init_host() ((void)0)
#define KS_SINE OP1_SINE_TABLE
#endif
static inline int32_t ks_sin(int32_t a) { return KS_SINE[a & (KS_TURN - 1)]; }
static inline int32_t ks_cos(int32_t a) { return KS_SINE[(a + KS_TURN / 4) & (KS_TURN - 1)]; }

static inline int32_t ks_abs(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t ks_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t ks_max(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t ks_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

KS_FN uint32_t ks_isqrt(uint32_t v)                /* floor(sqrt(v)) */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}
/* e^(-x), x in Q10 (>= 0) -> Q15. 2^-(x log2 e): whole part by shift, fraction by a cubic. */
KS_FN int32_t ks_expneg(int32_t x)
{
    if (x <= 0) return 32768;
    int32_t y = (int32_t)(((uint32_t)x * 1477u) >> 10);      /* x log2(e), Q10 */
    int32_t n = y >> 10;
    if (n >= 15) return 0;
    int32_t t = (y & 1023) << 5, t2 = (t * t) >> 15, t3 = (t2 * t) >> 15;   /* Q15 fraction */
    int32_t r = 32768 - ((t * 22713) >> 15) + ((t2 * 7871) >> 15) - ((t3 * 1819) >> 15);
    return r >> n;
}

/* ---- 2D transforms: x' = (x a + y c) / 4096 + tx, y' = (x b + y d) / 4096 + ty. Local coordinates stay within
 * about +-256 units (Q10 < 2^18) so the products fit in 32 bits with |a..d| < 2^13. ---- */
typedef struct { int32_t a, b, c, d, tx, ty; } ks_m;
typedef struct { int32_t x, y; } ks_p;              /* Q10 */
/* rotate by ang and scale by s (Q12) around the local origin, then move the origin to (tx, ty) */
KS_FN ks_m ks_rot(int32_t ang, int32_t s, int32_t tx, int32_t ty)
{
    int32_t c = (ks_cos(ang) * s) >> 15, n = (ks_sin(ang) * s) >> 15;
    ks_m m = { c, n, -n, c, tx, ty };
    return m;
}
static inline ks_p ks_ap(const ks_m *m, int32_t x, int32_t y)
{
    ks_p p = { ((x * m->a + y * m->c) >> 12) + m->tx, ((x * m->b + y * m->d) >> 12) + m->ty };
    return p;
}
KS_FN ks_m ks_mul(const ks_m *outer, const ks_m *inner)   /* outer after inner */
{
    ks_m m;
    m.a = (inner->a * outer->a + inner->b * outer->c) >> 12;
    m.b = (inner->a * outer->b + inner->b * outer->d) >> 12;
    m.c = (inner->c * outer->a + inner->d * outer->c) >> 12;
    m.d = (inner->c * outer->b + inner->d * outer->d) >> 12;
    ks_p t = ks_ap(outer, inner->tx, inner->ty);
    m.tx = t.x; m.ty = t.y;
    return m;
}
/* local transform about a pivot: rotate by ang, scale by s, around (px, py), then shift by (dx, dy) */
KS_FN ks_m ks_about(int32_t ang, int32_t s, int32_t px, int32_t py, int32_t dx, int32_t dy)
{
    ks_m m = ks_rot(ang, s, 0, 0);
    ks_p q = ks_ap(&m, -px, -py);
    m.tx = q.x + px + dx; m.ty = q.y + py + dy;
    return m;
}

/* ---- drawing (screen space, Q10) ---- */
typedef const uint8_t *ks_c;
static inline void ks_ln(void *r, ks_p a, ks_p b, int32_t w, ks_c c) { sq_line(r, a.x, a.y, b.x, b.y, w, c); }
KS_FN void ks_poly(void *r, const ks_p *p, int n, int closed, int32_t w, ks_c c)
{
    for (int i = 1; i < n; i++) ks_ln(r, p[i - 1], p[i], w, c);
    if (closed && n > 2) ks_ln(r, p[n - 1], p[0], w, c);
}
/* convex polygon fill: horizontal runs half a unit apart, each 0.7 units thick so they close up */
KS_FN void ks_fill(void *r, const ks_p *p, int n, ks_c c)
{
    int32_t y0 = p[0].y, y1 = p[0].y;
    for (int i = 1; i < n; i++) { y0 = ks_min(y0, p[i].y); y1 = ks_max(y1, p[i].y); }
    for (int32_t y = (y0 & ~(KS_ONE / 2 - 1)) + KS_ONE / 4; y <= y1; y += KS_ONE / 2) {
        int32_t xl = 0x7fffffff, xr = -0x7fffffff;
        for (int i = 0; i < n; i++) {
            ks_p a = p[i], b = p[(i + 1) % n];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                int32_t x = a.x + (int32_t)(((int64_t)(b.x - a.x) * (y - a.y)) / (b.y - a.y));
                xl = ks_min(xl, x); xr = ks_max(xr, x);
            }
        }
        if (xl <= xr) sq_line(r, xl, y, xr + 1, y, OP1_Q10(0.7), c);
    }
}
/* points on an ellipse (rx, ry in local units Q10) through a transform; n <= 32 */
KS_FN int ks_ellipse(ks_p *out, const ks_m *m, int32_t cx, int32_t cy, int32_t rx, int32_t ry, int n)
{
    for (int i = 0; i < n; i++) {
        int32_t a = i * KS_TURN / n;
        out[i] = ks_ap(m, cx + ((rx * ks_cos(a)) >> 15), cy + ((ry * ks_sin(a)) >> 15));
    }
    return n;
}
KS_FN void ks_ring(void *r, const ks_m *m, int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t w, ks_c c)
{
    ks_p p[24]; int n = ks_ellipse(p, m, cx, cy, rx, ry, rx > OP1_Q10(8) || ry > OP1_Q10(8) ? 24 : 14);
    ks_poly(r, p, n, 1, w, c);
}
KS_FN void ks_disc(void *r, const ks_m *m, int32_t cx, int32_t cy, int32_t rad, ks_c c)
{
    ks_p p[14]; int n = ks_ellipse(p, m, cx, cy, rad, rad, 14);
    ks_fill(r, p, n, c);
}
/* cubic Bezier from p0 through local control points into out (n segments, n + 1 points, n <= 24) */
KS_FN int ks_bez(ks_p *out, const ks_m *m, const int32_t *k, int n)   /* k: x0 y0 x1 y1 x2 y2 x3 y3, Q10 */
{
    for (int i = 0; i <= n; i++) {
        int32_t t = (i << 12) / n, u = 4096 - t;                       /* Q12 */
        int32_t b0 = (((u * u) >> 12) * u) >> 12, b1 = 3 * ((((u * u) >> 12) * t) >> 12),
                b2 = 3 * ((((u * t) >> 12) * t) >> 12), b3 = (((t * t) >> 12) * t) >> 12;
        int32_t x = (k[0] >> 4) * b0 + (k[2] >> 4) * b1 + (k[4] >> 4) * b2 + (k[6] >> 4) * b3;   /* Q6 * Q12 */
        int32_t y = (k[1] >> 4) * b0 + (k[3] >> 4) * b1 + (k[5] >> 4) * b2 + (k[7] >> 4) * b3;
        out[i] = ks_ap(m, x >> 8, y >> 8);
    }
    return n + 1;
}
KS_FN void ks_curve(void *r, const ks_m *m, const int32_t *k, int n, int32_t w, ks_c c)
{
    ks_p p[25]; int c2 = ks_bez(p, m, k, n); ks_poly(r, p, c2, 0, w, c);
}

/* ---- text: native stroke font (4 x 6 grid). Scale s (Q10) per grid unit: cap height 6 s. ---- */
#define KS_XS OP1_Q10(2.2222)                       /* 13.3 units tall: names, units */
#define KS_S  OP1_Q10(3.3333)                       /* 20 units tall: values */
#define KS_TW OP1_Q10(1.5)                          /* stroke for lettering */
KS_FN int32_t ks_text_w(const char *t, int32_t s)  /* ink width */
{
    int32_t w = 0;
    for (; *t; t++) w += (*t == '1' || *t == '.' ? 4 : 6) * s;
    return w ? w - 2 * s : 0;
}
#endif
