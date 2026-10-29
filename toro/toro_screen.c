/* toro screen: RIDER (GUI thread). A robot bull on a hydraulic ram, head-on, with a cowboy aboard. toro.svg holds
 * the floor, the hose and the bull's body and faceplate (node "rig", moved here every frame with "rod" and
 * "shadow"); this file draws the horns, eyes, nose ring, steam, sparks, the cowboy (outline only) and the readouts.
 * Motion follows the notes the engine plays (toro_notes / toro_blocks from toro.c): nothing moves without a note.
 * Each control's colour goes on parts that only it moves, in shapes no other control uses:
 *   SPRD  blue   horn spread, and the cowboy's hat brim          SHAP (page 2) horn tips and brim ends curl
 *   (page-2 words always act on the drawing, as they always act on the sound; a press only swaps the readout)
 *   FOLD  green  how hard each note rocks the bull and throws the cowboy; the hose pulse; his boots and spurs kick
 *                NOIS (page 2) green sparks off the turntable while a note sounds
 *   CUT   white  the bull's eye slits and the cowboy's eyes      DRIV / APHX (page 2, one knob split in half):
 *                DRIV (0..50 %) steam from the vents on each note; APHX (50..100 %) white glitch slices tear across
 *                the rig on each note, laid out by the key played (same key, same tear) and wilder as APHX rises
 *   RES   orange the nose ring's swing on each note, and the cowboy's lasso: the arm rests at 0 and rises with a
 *                lasso that grows with RES; both keep swinging longer as RES goes up
 * Readouts: each in the corner nearest the part it drives (SPRD top-left by the hat and horn, RES top-right where
 * the lasso flies, CUT bottom-left under the face, FOLD bottom-right where the hose ends), with an icon that is a
 * miniature of that part. They never move. Text and icons stay at or above y 140; the drawing stays inside
 * 8..312 x 8..144 and at least 2 units from any readout (checked in the host preview, host/screen_host.c).
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "op1_plugin.h"
#include "seq/seq_text.h"
#include "knscr.h"
#ifdef OP1_HOST
void host_mark(int readout);
#else
#define host_mark(x) ((void)0)
#endif

extern volatile uint32_t toro_notes, toro_blocks;       /* toro.c */
extern volatile int16_t toro_key;

static const uint8_t C_BLUE[4] = { 105, 142, 255, 255 }, C_GREEN[4] = { 0, 237, 149, 255 }, C_WHITE[4] = { 255, 255, 255, 255 },
                     C_ORANGE[4] = { 255, 66, 63, 255 }, C_SEC[4] = { 185, 185, 201, 255 }, C_DEC[4] = { 98, 93, 119, 255 },
                     C_HSEG[4] = { 69, 96, 204, 255 }, C_HTIP[4] = { 196, 210, 255, 255 };
static const uint8_t *const C_ENC[4] = { C_BLUE, C_GREEN, C_WHITE, C_ORANGE };
#define W1  OP1_Q10(1)
#define W15 OP1_Q10(1.5)
#define W2  OP1_Q10(2)
#define Q   OP1_Q10                                     /* constants only */

/* ---- rig geometry (must match gen_svg.py) ---- */
#define TR_CX0  Q(150)
#define TR_BASE Q(132)
#define TR_S    2867                                    /* rig scale 0.7, Q12 */
#define TR_HK   4792                                    /* faceplate scale 1.17, Q12 */
#define TR_RS   6349                                    /* cowboy 1.55 */
#define TR_HS   6963                                    /* his hat 1.7 */
#define TR_BS   7168                                    /* his boots 1.75 */

/* phase of an oscillation at rate rad/s x 100, after t ms -> angle units */
static int32_t ph(int32_t t_ms, int32_t rate100) { return (int32_t)(((int64_t)t_ms * rate100 * 13038) / 1000000); }
static int32_t lerp10(int32_t a, int32_t b, int32_t f) { return a + (((b - a) * f) >> 10); }   /* f Q10 */

typedef struct {
    int32_t sprd, fold, cut, res, shape, noise, drive, aphx;   /* Q10 0..1 */
    int page2, gate, n;                                 /* page2: bit i = knob i shows its page-2 word */
    int16_t raw[7];                                     /* the knob words, for the readouts */
    int32_t t_ms, tq;                                   /* since the last note-on: ms, and seconds in Q10 */
} kn_t;

/* ---- small drawing helpers in a local frame ---- */
static void bz(void *r, const ks_m *m, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
               int n, int32_t w, ks_c c)
{
    const int32_t k[8] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    ks_curve(r, m, k, n, w, c);
}
static void ln(void *r, const ks_m *m, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t w, ks_c c)
{
    ks_ln(r, ks_ap(m, x0, y0), ks_ap(m, x1, y1), w, c);
}
/* an outlined limb through three joints, tapering w0 -> w1 (local units Q10), round ends */
static void limb(void *r, const ks_m *m, const int32_t *j, int32_t w0, int32_t w1, ks_c c)
{
    ks_p L[3], R[3];
    int32_t nx[2], ny[2];
    for (int s = 0; s < 2; s++) {
        int32_t dx = j[2 * s + 2] - j[2 * s], dy = j[2 * s + 3] - j[2 * s + 1];
        int32_t len = (int32_t)ks_isqrt((uint32_t)((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) << 4;
        if (len < 16) len = 16;
        nx[s] = -dy * 1024 / len; ny[s] = dx * 1024 / len;                     /* Q10 unit normal */
    }
    for (int i = 0; i < 3; i++) {
        int32_t w = i == 0 ? w0 : i == 2 ? w1 : (w0 + w1) / 2;
        int32_t ax = i == 0 ? nx[0] : i == 2 ? nx[1] : (nx[0] + nx[1]) / 2, ay = i == 0 ? ny[0] : i == 2 ? ny[1] : (ny[0] + ny[1]) / 2;
        L[i] = ks_ap(m, j[2 * i] + ((ax * w) >> 10), j[2 * i + 1] + ((ay * w) >> 10));
        R[i] = ks_ap(m, j[2 * i] - ((ax * w) >> 10), j[2 * i + 1] - ((ay * w) >> 10));
    }
    ks_poly(r, L, 3, 0, W15, c); ks_poly(r, R, 3, 0, W15, c);
    for (int e = 0; e < 2; e++) {                                               /* end caps: a bowed line */
        int i = e ? 2 : 0, s = e ? 1 : 0, sg = e ? 1 : -1;
        int32_t w = e ? w1 : w0, tx = ny[s] * sg, ty = -nx[s] * sg;              /* outward along the limb */
        ks_p a = L[i], b = ks_ap(m, j[2 * i] + ((tx * w) >> 10), j[2 * i + 1] + ((ty * w) >> 10)), d = R[i];
        ks_ln(r, a, b, W15, c); ks_ln(r, b, d, W15, c);
    }
}

/* ---- horns: a thick tube along a Bezier spine, drawn segment by segment ---- */
static void horn(void *r, const ks_m *H, int sd, const kn_t *k)
{
    int32_t sp = k->sprd, curl = k->shape;
    int32_t px[4] = { Q(18), Q(32) + sp * 10, Q(42) + sp * 20, Q(44) + sp * 25 - curl * 12 };
    int32_t py[4] = { Q(-18), Q(-21), Q(-21) - (Q(4) + sp * 3), Q(-18) - (Q(16) + sp * 4) + curl * 8 };
    int32_t kk[8];
    for (int i = 0; i < 4; i++) { kk[2 * i] = px[i] * sd; kk[2 * i + 1] = py[i]; }
    ks_p p[25];
    ks_bez(p, H, kk, 24);
    for (int j = 3; j < 24; j++) {                                              /* the root is inside the faceplate */
        int32_t u = (j * 1024 + 512) / 24, w = Q(5.4) - ((Q(3.4) * ((u * (717 + ((307 * u) >> 10))) >> 10)) >> 10);
        int32_t ws = (2 * w * ((TR_S * TR_HK) >> 12)) >> 12;                   /* screen width */
        ks_ln(r, p[j], p[j + 1], ws, u > 860 ? C_HTIP : C_BLUE);
        if (j == 8 || j == 13 || j == 18) {                                     /* telescoping joints */
            int32_t dx = p[j + 1].x - p[j].x, dy = p[j + 1].y - p[j].y;
            int32_t len = (int32_t)ks_isqrt((uint32_t)((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) << 4;
            if (len > 0) {
                int32_t h = ws * 9 / 20, nx = -dy * 1024 / len, ny = dx * 1024 / len;
                sq_line(r, p[j].x + ((nx * h) >> 10), p[j].y + ((ny * h) >> 10), p[j].x - ((nx * h) >> 10), p[j].y - ((ny * h) >> 10), Q(1.2), C_HSEG);
            }
        }
    }
}

/* ---- the cowboy (outline only) ---- */
static void cowboy(void *r, const ks_m *T, const kn_t *k, int32_t a, int32_t e)
{
    int dir = k->n & 1 ? 1 : -1;
    int32_t m = ks_min(1024, (((k->fold * (205 + ((922 * e) >> 15))) >> 10) * 1280) >> 10);
    int32_t wild = (k->fold * k->fold) >> 10;
    int32_t tt = k->t_ms + (k->n & 63) * 500;                                   /* his own clock: each note shifts it */
    int32_t rt = -((a * (1843 + 2 * wild)) >> 10) + dir * ((((m * (123 + ((410 * wild) >> 10))) >> 10) * 1304 >> 10) * ks_sin(ph(k->t_ms, 1100)) >> 15);
    int32_t up = (ks_abs(ks_sin(ph(k->t_ms, 900))) * ((m * (3072 + 4 * wild)) >> 10)) >> 15;
    int32_t slide = dir * ((((m * wild) >> 10) * 7 * ks_sin(ph(k->t_ms, 900) + 782)) >> 15);
    ks_m in = ks_about(rt, TR_RS, 0, Q(-52), slide, -up), W = ks_mul(T, &in);
    ks_m inl = ks_about(0, TR_RS, 0, Q(-52), slide * 2 / 5, -up * 3 / 10), WL = ks_mul(T, &inl);

    /* legs, boots and spurs: knees and boots swing on each throw (FOLD) */
    int32_t kick = (m * (4096 + ((6 * 1024 * (e >> 5)) >> 10) + ((9 * wild * (e >> 5)) >> 10))) >> 10;
    for (int sd = -1; sd <= 1; sd += 2) {
        int32_t p0 = ph(tt, 1300) + (sd < 0 ? 2216 : 0), p1 = p0 + 1043;
        int32_t j[6] = { Q(7) * sd, Q(-55),
                         (Q(24) + ((ks_sin(p0) * ((kick * 2) / 5)) >> 15)) * sd, Q(-50) - ((ks_cos(p0) * ((kick * 7) / 20)) >> 15) - 3 * m,
                         (Q(33) + ((ks_sin(p1) * ((kick * 3) / 4)) >> 15)) * sd, Q(-38) - ((ks_cos(p1) * ((kick * 7) / 10)) >> 15) - 4 * m };
        limb(r, &WL, j, Q(2.6), Q(2), C_SEC);
        int32_t bx = j[4], by = j[5], o = sd;
        ks_m bs = ks_about(0, TR_BS, bx, by, 0, 0), B = ks_mul(&WL, &bs);
        ks_p boot[16]; int nb = 0;
        boot[nb++] = ks_ap(&B, bx - Q(2.4) * o, by - Q(1));
        int32_t c1[8] = { bx - Q(2.6) * o, by + Q(6), bx - Q(2.6) * o, by + Q(8), bx + Q(1) * o, by + Q(8.5), bx + Q(6.5) * o, by + Q(8) };
        nb += ks_bez(boot + nb, &B, c1, 5);                                    /* starts at the shaft's foot */
        int32_t c2[8] = { bx + Q(6.5) * o, by + Q(8), bx + Q(7.5) * o, by + Q(7.8), bx + Q(7.5) * o, by + Q(6), bx + Q(5.5) * o, by + Q(5.6) };
        nb += ks_bez(boot + nb - 1, &B, c2, 4) - 1;                             /* (shares the toe point) */
        boot[nb++] = ks_ap(&B, bx + Q(2.4) * o, by + Q(4.5));
        boot[nb++] = ks_ap(&B, bx + Q(2.4) * o, by - Q(1));
        ks_poly(r, boot, nb, 1, W15, C_GREEN);
        ln(r, &B, bx - Q(2.6) * o, by + Q(2.4), bx + Q(2.4) * o, by + Q(2.4), W1, C_GREEN);
        int32_t hx = bx - Q(2.6) * o, hy = by + Q(6.5), sx = hx - Q(4) * o, spin = ph(tt, 2000);
        ln(r, &B, hx, hy, sx + Q(1.6) * o, hy, W1, C_GREEN);
        ks_ring(r, &B, sx, hy, Q(1.7), Q(1.7), W15, C_GREEN);
        for (int qq = 0; qq < 4; qq++) {
            int32_t aa = spin + qq * 1024, cx = (Q(2.8) * ks_cos(aa)) >> 15, cy = (Q(2.8) * ks_sin(aa)) >> 15;
            ln(r, &B, sx + cx, hy + cy, sx - cx, hy - cy, W1, C_GREEN);
        }
    }
    /* torso, belt, buckle, bandana */
    {
        ks_p t[16]; int n = 0;
        t[n++] = ks_ap(&W, Q(-6), Q(-54));
        int32_t c[8] = { Q(-9.5), Q(-73), Q(-6), Q(-75), Q(6), Q(-75), Q(9.5), Q(-73) };
        n += ks_bez(t + n, &W, c, 6);
        t[n++] = ks_ap(&W, Q(6), Q(-54));
        ks_poly(r, t, n, 1, W15, C_SEC);
        ln(r, &W, Q(-6.3), Q(-57.5), Q(6.3), Q(-57.5), W1, C_DEC);
        ks_p bk[4] = { ks_ap(&W, Q(-2.2), Q(-59.2)), ks_ap(&W, Q(2.2), Q(-59.2)), ks_ap(&W, Q(2.2), Q(-55.8)), ks_ap(&W, Q(-2.2), Q(-55.8)) };
        ks_poly(r, bk, 4, 1, W1, C_DEC);
        ks_p bd[3] = { ks_ap(&W, Q(-5), Q(-74)), ks_ap(&W, Q(5), Q(-74)), ks_ap(&W, Q(0), Q(-68.5)) };
        ks_poly(r, bd, 3, 1, W15, C_SEC);
        ln(r, &W, 0, Q(-68), 0, Q(-60), W1, C_DEC);
    }
    /* arms: the left grips the handle; the right rests at RES 0 and rises with a lasso as RES goes up */
    {
        int32_t jl[6] = { Q(-8.6), Q(-71), Q(-14), Q(-63), Q(-5), Q(-61) };
        limb(r, &W, jl, Q(1.9), Q(1.6), C_SEC);
        ks_ring(r, &W, Q(-4.2), Q(-61), Q(1.7), Q(1.7), W15, C_SEC);
        int32_t rs = k->res;
        int32_t amp = 102 + ((256 * rs) >> 10) + ((((m * (256 + ((307 * wild) >> 10))) >> 10) * rs) >> 10);   /* rad Q10 */
        int32_t wave = ((ks_sin(ph(tt, 1000 + 600 * wild / 1024)) * amp) >> 15) * 1304 >> 10;              /* units */
        int32_t elx = lerp10(Q(12.5), Q(18), rs), ely = lerp10(Q(-64), Q(-80), rs);
        int32_t hx = lerp10(Q(11), Q(25), rs) + ((ks_sin(wave) * ((Q(4) * rs) >> 10)) >> 15);
        int32_t hy = lerp10(Q(-57), Q(-88), rs) - ((ks_cos(wave) * ((Q(1.5) * rs) >> 10)) >> 15) + ((ks_sin(wave) * ((Q(1.2) * (1024 - rs)) >> 10)) >> 15);
        int32_t jr[6] = { Q(8.6), Q(-71), elx, ely, hx, hy };
        limb(r, &W, jr, Q(1.9), Q(1.6), C_SEC);
        ks_ring(r, &W, hx, hy, Q(1.8), Q(1.8), W15, C_SEC);
        if (rs > 31) {
            int32_t tau = 123 + ((1434 * rs) >> 10), live = ks_expneg(k->tq * 1024 / tau);
            int32_t R = ((Q(3) + rs * 29) * (19661 + ((13107 * (live >> 0)) >> 15))) >> 15;   /* (0.6 + 0.4 live) */
            int32_t cxl = hx + R * 6 / 10, cyl = hy - Q(2) - R * 12 / 100;
            int32_t spin = ph(k->t_ms, 1400) + (k->n & 7) * 1304;
            ks_p loop[24]; int nl = ks_ellipse(loop, &W, cxl, cyl, R, R * 42 / 100, 24);
            ks_p knot = ks_ap(&W, cxl + ((R * ks_cos(spin)) >> 15), cyl + ((R * 42 / 100 * ks_sin(spin)) >> 15));
            int32_t ymin = loop[0].y;
            for (int i = 1; i < nl; i++) ymin = ks_min(ymin, loop[i].y);
            int32_t dy = ks_max(0, Q(9.5) - ymin);                              /* a thrown loop dips rather than leave the screen */
            for (int i = 0; i < nl; i++) loop[i].y += dy;
            knot.y += dy;
            ks_poly(r, loop, nl, 1, W15, C_ORANGE);
            ks_p hs = ks_ap(&W, hx, hy), qc = { (hs.x + knot.x) / 2 + Q(2), ks_max(hs.y, knot.y) + Q(2) }, prev = hs;
            for (int i = 1; i <= 8; i++) {                                      /* the rope: a quadratic from hand to knot */
                int32_t s = i * 128, u = 1024 - s;
                ks_p p = { (u * u >> 10) * hs.x / 1024 + 2 * (u * s >> 10) * qc.x / 1024 + (s * s >> 10) * knot.x / 1024,
                           (u * u >> 10) * hs.y / 1024 + 2 * (u * s >> 10) * qc.y / 1024 + (s * s >> 10) * knot.y / 1024 };
                ks_ln(r, prev, p, W15, C_ORANGE); prev = p;
            }
            ks_m idm = { 4096, 0, 0, 4096, 0, 0 };
            ks_ring(r, &idm, knot.x, knot.y, Q(0.9), Q(0.9), W15, C_ORANGE);
        }
    }
    /* head and face: a smile while calm; an O with wide eyes as the bull throws him. CUT opens his eyes too */
    {
        ks_ring(r, &W, 0, Q(-81.5), Q(6.6), Q(6.6), W15, C_SEC);
        int32_t er = Q(0.5) + ((Q(0.45) * k->cut) >> 10) + ((Q(0.8) * m) >> 10);
        for (int sd = -1; sd <= 1; sd += 2) {
            ks_ring(r, &W, Q(2.5) * sd, Q(-82.6), er, er, W15, C_WHITE);
            ln(r, &W, Q(1.2) * sd, Q(-85) - ((Q(1.3) * m) >> 10), Q(3.8) * sd, Q(-84.2) - ((Q(1.6) * m) >> 10), W1, C_DEC);
        }
        if (m < 307) bz(r, &W, Q(-2.6), Q(-78.8), Q(-1.3), Q(-77.3), Q(1.3), Q(-77.3), Q(2.6), Q(-78.8), 6, W15, C_SEC);
        else ks_ring(r, &W, 0, Q(-78), Q(0.7) + ((Q(0.7) * m) >> 10), Q(0.9) + ((Q(1.1) * m) >> 10), W15, C_SEC);
    }
    /* the big hat (blue: SPRD widens the brim; SHAP curls its ends), lifting off on the hardest throws */
    {
        int32_t hl = ((((m * e) >> 15) * (2048 + ((1536 * wild) >> 10))) >> 10) + Q(0.6);
        int32_t bw = 819 + ((461 * k->sprd) >> 10), bc = k->shape * 3;
        ks_m hs = ks_about(0, TR_HS, 0, Q(-87), 0, 0), Hm = ks_mul(&W, &hs);
        int32_t x16 = (Q(16) * bw) >> 10, x12 = (Q(12) * bw) >> 10;
        bz(r, &Hm, -x16, Q(-86.5) - hl - bc, -x12, Q(-83.8) - hl, x12, Q(-83.8) - hl, x16, Q(-86.5) - hl - bc, 8, W15, C_BLUE);
        bz(r, &Hm, x16, Q(-86.5) - hl - bc, x12, Q(-86) - hl, -x12, Q(-86) - hl, -x16, Q(-86.5) - hl - bc, 8, W15, C_BLUE);
        bz(r, &Hm, Q(-7), Q(-85.6) - hl, Q(-8), Q(-92) - hl, Q(-6.5), Q(-96.5) - hl, Q(-3), Q(-96) - hl, 5, W15, C_BLUE);
        bz(r, &Hm, Q(-3), Q(-96) - hl, Q(-1), Q(-94.6) - hl, Q(1), Q(-94.6) - hl, Q(3), Q(-96) - hl, 3, W15, C_BLUE);
        bz(r, &Hm, Q(3), Q(-96) - hl, Q(6.5), Q(-96.5) - hl, Q(8), Q(-92) - hl, Q(7), Q(-85.6) - hl, 5, W15, C_BLUE);
        ln(r, &Hm, Q(-7.3), Q(-88) - hl, Q(7.3), Q(-88) - hl, W1, C_HSEG);
    }
}

/* ---- readouts ---- */
typedef struct { int16_t nx, ny, vx, vy, ix; uint8_t right; } slot_t;
static const slot_t SLOT[4] = {                          /* blue TL, green BR, white BL, orange TR; baselines */
    { 23, 23, 9, 47, 9, 0 }, { 297, 114, 311, 138, 301, 1 }, { 23, 114, 9, 138, 9, 0 }, { 297, 23, 311, 47, 301, 1 } };
enum { IC_SPRD, IC_FOLD, IC_CUT, IC_RES, IC_SHAP, IC_NOIS, IC_DRIV, IC_APHX };
static void icon(void *r, int id, int32_t x, int32_t y, ks_c c)
{
    ks_m m = { 4096, 0, 0, 4096, x, y };
#define P(px, py) ks_ap(&m, Q(px), Q(py))
    switch (id) {
    case IC_SPRD: ks_ring(r, &m, Q(5), Q(7), Q(2.2), Q(2.2), W15, c);
        bz(r, &m, Q(3), Q(6.2), Q(1), Q(6.2), Q(0.6), Q(4), Q(0.8), Q(1), 5, W15, c);
        bz(r, &m, Q(7), Q(6.2), Q(9), Q(6.2), Q(9.4), Q(4), Q(9.2), Q(1), 5, W15, c); break;
    case IC_FOLD: bz(r, &m, Q(0.8), Q(8.6), Q(3.4), Q(8.6), Q(4.6), Q(3.2), Q(9.2), Q(3.2), 6, W15, c); ks_disc(r, &m, Q(5.4), Q(5.2), Q(1.7), c); break;
    case IC_CUT: { ks_p p[4] = { P(0.6, 3.2), P(9.4, 4.6), P(9, 7.6), P(1, 6.2) }; ks_fill(r, p, 4, c); break; }
    case IC_RES: ks_ring(r, &m, Q(5.8), Q(3.6), Q(3.6), Q(2.4), W15, c); ks_ln(r, P(1.2, 9.4), P(3, 5.6), W15, c); break;
    case IC_SHAP: bz(r, &m, Q(0.8), Q(9), Q(5), Q(9), Q(9), Q(6.4), Q(8.4), Q(2.4), 6, W15, c);
        bz(r, &m, Q(8.4), Q(2.4), Q(8), Q(0.6), Q(5.2), Q(0.8), Q(5.6), Q(3), 4, W15, c); break;
    case IC_NOIS: ks_ln(r, P(5, 0.8), P(5, 9.2), W15, c); ks_ln(r, P(0.8, 5), P(9.2, 5), W15, c);
        ks_ln(r, P(2.2, 2.2), P(7.8, 7.8), W15, c); ks_ln(r, P(7.8, 2.2), P(2.2, 7.8), W15, c); break;
    case IC_APHX: ks_ln(r, P(0.6, 1.6), P(6.2, 1.6), W15, c); ks_ln(r, P(3.4, 5), P(9.4, 5), W15, c); ks_ln(r, P(1.4, 8.4), P(5.6, 8.4), W15, c); break;
    case IC_DRIV: bz(r, &m, Q(0.8), Q(8.8), Q(0.8), Q(5.4), Q(4.4), Q(5.4), Q(4.4), Q(8.8), 5, W15, c);
        bz(r, &m, Q(3.8), Q(6), Q(3.8), Q(1.6), Q(9.2), Q(1.6), Q(9.2), Q(6.4), 6, W15, c); break;
    }
#undef P
}
static void readout(void *r, int k, int ic, const char *name, int32_t q15)
{
    const slot_t *s = &SLOT[k];
    ks_c c = C_ENC[k];
    char v[6]; sq_num(v, (uint32_t)(ks_clamp(q15, 0, 32767) * 99 / 32767), 1);
    int32_t nw = ks_text_w(name, KS_XS), vw = ks_text_w(v, KS_S);
    sq_text(r, Q(s->nx) - (s->right ? nw : 0), Q(s->ny) - 6 * KS_XS, KS_XS, name, KS_TW, c);
    sq_text(r, Q(s->vx) - (s->right ? vw : 0), Q(s->vy) - 6 * KS_S, KS_S, v, KS_TW, c);
    icon(r, ic, Q(s->ix), Q(s->ny) - Q(11.5), c);
}

/* ---- per frame ---- */
static uint32_t sc_notes, sc_blocks, sc_frame;
static int32_t sc_t_ms = 9000;
static int sc_quiet = 99;
static void *n_rig, *n_rod, *n_shadow;

void tr_screen_init(void *scr)
{
#ifndef OP1_HOST
    n_rig = op1_find_node(scr, "rig"); n_rod = op1_find_node(scr, "rod"); n_shadow = op1_find_node(scr, "shadow");
#else
    (void)scr; n_rig = (void *)1; n_rod = (void *)2; n_shadow = (void *)3;    /* the preview records the matrices */
#endif
}
static void place(void *node, int32_t a, int32_t b, int32_t c, int32_t d, int32_t tx, int32_t ty)
{
    if (!node) return;
    const int32_t m9[9] = { a, b, 0, c, d, 0, tx, ty, KS_ONE };
    op1_node_set_transform(node, m9);
}

/* knobs: live page-1 values (Q15) and the page-2 words; page2: the engine screen shows page 2; dt: ms since the
 * last frame. Draws on top of the SVG (call after op1_screen_draw, with the rig nodes already placed: see
 * tr_screen_place, called before it). */
static kn_t sc_k;
static int32_t sc_a, sc_e, sc_lift, sc_cx;
void tr_screen_place(const int16_t *kn4, const int16_t *page2w, int page2, int dt)
{
    kn_t *k = &sc_k;
    k->sprd = (ks_max(kn4[0], 0) * 1025) >> 15; k->fold = (ks_max(kn4[1], 0) * 1025) >> 15;
    k->cut = (ks_max(kn4[2], 0) * 1025) >> 15; k->res = (ks_max(kn4[3], 0) * 1025) >> 15;
    k->shape = (ks_max(page2w[0], 0) * 1025) >> 15; k->noise = (ks_max(page2w[1], 0) * 1025) >> 15; k->drive = ks_min(1024, (ks_max(page2w[2], 0) * 2050) >> 15);   /* DRIV: white page 2's lower half */
    k->aphx = page2w[2] > 16384 ? ((page2w[2] - 16384) * 2050) >> 15 : 0;   /* APHX: its upper half */
    k->page2 = page2;
    for (int i = 0; i < 4; i++) k->raw[i] = kn4[i];
    for (int i = 0; i < 3; i++) k->raw[4 + i] = page2w[i];
    uint32_t notes = toro_notes, blocks = toro_blocks;
    if (dt < 1 || dt > 100) dt = 16;
    if (notes != sc_notes) { sc_notes = notes; sc_t_ms = 0; k->n++; } else if (sc_t_ms < 60000) sc_t_ms += dt;
    sc_quiet = blocks != sc_blocks ? 0 : ks_min(sc_quiet + 1, 99);
    sc_blocks = blocks;
    sc_frame++;
    k->gate = sc_quiet < 3 && sc_t_ms < 2000;
    k->t_ms = sc_t_ms; k->tq = sc_t_ms * 1024 / 1000;

    /* FOLD: each note lifts the ram and rocks the rig; it keeps rocking while notes play */
    int32_t e = ks_expneg(k->tq * 5), e2 = ks_expneg(k->tq * 3);
    int32_t lift = ((Q(2) + k->fold * 5 + (((k->fold * 7) * (e >> 5)) >> 10)) * 2) / 5;
    int dir = k->n & 1 ? 1 : -1, on = k->t_ms < 1000;
    int32_t amp = (k->fold * 104 + ((k->fold * k->fold) >> 10) * 261) >> 10;  /* angle units */
    int32_t env = 11469 + ((21299 * (e2 >> 0)) >> 15);
    int32_t a = on ? dir * ((((amp * env) >> 15) * ks_cos(ph(k->t_ms, 900))) >> 15) : 0;
    int32_t cx = TR_CX0 + ((a * 17695) >> 10);
    int32_t top = Q(116) - lift;
    sc_a = a; sc_e = e; sc_lift = lift; sc_cx = cx;

    ks_m T = ks_rot(a, TR_S, cx, top);
    place(n_rig, T.a >> 2, T.b >> 2, T.c >> 2, T.d >> 2, T.tx, T.ty);
    int32_t by = TR_BASE - ((Q(13) * TR_S) >> 12), rw2 = (Q(7) * TR_S) >> 12;
    place(n_rod, rw2, 0, cx - TR_CX0, top - by, TR_CX0, by);
    int32_t srx = ((Q(40) - (((TR_BASE - top) * 2) / 5)) * TR_S) >> 12, sry = (Q(5) * TR_S) >> 12;
    place(n_shadow, srx, 0, 0, sry, cx + a * 27, TR_BASE + ((Q(5) * TR_S) >> 12));
}

void tr_screen_draw(void *r)
{
    const kn_t *k = &sc_k;
    int32_t a = sc_a, e = sc_e, cx = sc_cx, top = Q(116) - sc_lift;
    ks_m T = ks_rot(a, TR_S, cx, top), D = { TR_HK, 0, 0, TR_HK, 0, Q(-30) }, H = ks_mul(&T, &D);

    horn(r, &H, -1, k); horn(r, &H, 1, k);

    /* CUT: the eye slits open; a note flashes a glow round them when CUT is up */
    int32_t eh = Q(1.4) + ((Q(3.4) * k->cut) >> 10);
    for (int sd = -1; sd <= 1; sd += 2) {
        ks_p q[4] = { ks_ap(&H, Q(19.5) * sd, Q(-7.8)), ks_ap(&H, Q(9.3) * sd, Q(-4.6)), ks_ap(&H, Q(9.3) * sd, Q(-4.6) + eh), ks_ap(&H, Q(18.8) * sd, Q(-7.8) + ((eh * 9) / 10)) };
        ks_fill(r, q, 4, C_WHITE);
        if (((e >> 5) * k->cut) >> 10 > 256) {
            ks_p g[4] = { ks_ap(&H, Q(22.8) * sd, Q(-10.26)), ks_ap(&H, Q(7.28) * sd, Q(-7)), ks_ap(&H, Q(7.28) * sd, Q(2.94)), ks_ap(&H, Q(21.8) * sd, Q(-0.72)) };
            ks_poly(r, g, 4, 1, W15, C_WHITE);
        }
    }
    /* DRIV (page 2): each note blasts steam from the vents */
    if (k->drive > 20 && k->t_ms < 600) {
        int puffs = 1 + ((k->drive * 3 + 512) >> 10);
        for (int sd = -1; sd <= 1; sd += 2) for (int i = 0; i < puffs; i++) {
            int32_t tt = ks_min(1024, k->t_ms * 1024 / 600 + i * 184);
            int32_t x = Q(10) + ((tt * (14 * 1024 + k->drive * 10)) >> 10), y = Q(22) + tt * 6, rr = Q(1.6) + tt * 3;
            bz(r, &H, (x - rr) * sd, y, (x - rr) * sd, y - rr * 13 / 10, (x + rr) * sd, y - rr * 13 / 10, (x + rr) * sd, y, 5, W15, C_SEC);
        }
    }
    /* APHX (white page 2, upper half): glitch slices tear across the rig on each note. Their layout comes from the
     * key the note was played on, as the sound's offsets do, so the same key tears the same way; more and wider
     * slices, longer, and jumpier as APHX rises */
    if (k->aphx > 10 && k->t_ms < 140 + ((k->aphx * 560) >> 10)) {
        uint32_t s = 2166136261u ^ ((uint32_t)(uint16_t)toro_key * 16777619u);
        int n = 2 + ((k->aphx * 9) >> 10);
        int32_t jit = Q(1) + ((((k->aphx * k->aphx) >> 10) * Q(9)) >> 10);
        uint32_t fl = sc_frame >> 1;                                            /* the tear jumps every other frame */
        for (int i = 0; i < n; i++) {
            s = s * 1664525u + 1013904223u; int32_t y = Q(40) + (int32_t)((s >> 16) % 78u) * KS_ONE;
            s = s * 1664525u + 1013904223u; int32_t x = cx - Q(50) + (int32_t)((s >> 16) % 80u) * KS_ONE;
            s = s * 1664525u + 1013904223u; int32_t len = Q(6) + (int32_t)((s >> 16) % 14u) * KS_ONE + ((k->aphx * Q(14)) >> 10);
            uint32_t q = (s >> 8) ^ (fl * 2654435761u);
            int32_t off = (int32_t)((q >> 20) % 2048u) - 1024;                  /* -1..1, Q10 */
            int32_t dx = (off * (jit >> 5)) >> 5;
            sq_line(r, x + dx, y, x + dx + len, y, W15, C_WHITE);
            if (k->aphx > 512) sq_line(r, x + dx + len + Q(3), y + Q(1.5), x + dx + len + Q(3) + len / 3, y + Q(1.5), W1, C_WHITE);
        }
    }
    /* RES: each note strikes the nose ring; resonance sets how long it keeps swinging */
    {
        int32_t tau = 123 + ((1434 * k->res) >> 10), dec = ks_expneg(k->tq * 1024 / tau);
        int32_t sw = (((717 * dec) >> 15) * ks_sin(ph(k->t_ms, 1400))) >> 15;   /* angle units */
        int32_t R = ((Q(4) + k->res * 5 / 2) * TR_HK) >> 12;                   /* rig units */
        int32_t pvy = Q(30) * 117 / 100 + Q(-30);
        int32_t rcx = (R * ks_sin(sw)) >> 15, rcy = pvy + ((R * ks_cos(sw)) >> 15);
        ks_ring(r, &T, rcx, rcy, R, R * 92 / 100, W2, C_ORANGE);
        if (ks_abs(sw) > 156 && k->res > 410) for (int sd = 0; sd < 2; sd++) {
            ks_p p[5]; int32_t base = sd ? KS_TURN / 2 : 0;
            for (int i = 0; i < 5; i++) { int32_t aa = base - 456 + i * 228; p[i] = ks_ap(&T, rcx + (((R + Q(3)) >> 5) * ks_cos(aa) >> 10), rcy + (((R + Q(3)) >> 5) * ks_sin(aa) >> 10)); }
            ks_poly(r, p, 5, 0, W15, C_ORANGE);
        }
    }
    /* FOLD: the hose pulse runs from the readout to the ram on each note */
    if (k->t_ms < 400) {
        static const int32_t hk[8] = { Q(170), Q(136), Q(200), Q(142), Q(236), Q(138), Q(256), Q(131) };
        ks_m idm = { 4096, 0, 0, 4096, 0, 0 };
        ks_p hp[21]; ks_bez(hp, &idm, hk, 20);
        int i = ks_clamp(((1024 - k->t_ms * 1024 / 400) * 19) >> 10, 0, 18);
        ks_disc(r, &idm, hp[i].x, hp[i].y, Q(2) + k->fold, C_GREEN);
    }
    /* NOIS (page 2): sparks off the turntable while a note sounds */
    if (k->noise > 20 && k->gate) {
        uint32_t s = 5u + (uint32_t)k->n * 7u + (uint32_t)(k->t_ms / 83);
        int n = (k->noise * 10 + 512) >> 10;
        for (int i = 0; i < n; i++) {
            s = s * 1664525u + 1013904223u; int32_t x = cx - Q(40) + (int32_t)((s >> 16) % 80) * KS_ONE;
            s = s * 1664525u + 1013904223u; int32_t y = TR_BASE - Q(14) + (int32_t)((s >> 16) % 16) * KS_ONE;
            sq_line(r, x - Q(1.6), y, x + Q(1.6), y, W15, C_GREEN);
            sq_line(r, x, y - Q(1.6), x, y + Q(1.6), W15, C_GREEN);
        }
    }
    cowboy(r, &T, k, a, e);

    host_mark(1);
    if (k->page2 & 1) readout(r, 0, IC_SHAP, "SHAP", k->raw[4]); else readout(r, 0, IC_SPRD, "SPRD", k->raw[0]);
    if (k->page2 & 2) readout(r, 1, IC_NOIS, "NOIS", k->raw[5]); else readout(r, 1, IC_FOLD, "FOLD", k->raw[1]);
    if (k->page2 & 4) {                                 /* one knob, two halves: the name says which half it is in */
        if (k->raw[6] > 16384) readout(r, 2, IC_APHX, "APHX", (k->raw[6] - 16384) * 2);
        else readout(r, 2, IC_DRIV, "DRIV", ks_max(k->raw[6], 0) * 2);
    } else readout(r, 2, IC_CUT, "CUT", k->raw[2]);
    readout(r, 3, IC_RES, "RES", k->raw[3]);
    host_mark(0);
}

/* ---- the engine screen ---- */
#ifndef OP1_HOST
static void tr_update(void *scr, int dt)
{
    int16_t kn[4];
    int slot = OP1_SCREEN_SLOT & 7;
    op1_synth_knobs_live(slot, kn);
    const int16_t *w = op1_synth_knobs(slot);
    int pg = (op1_eng_page[0] ? 1 : 0) | (op1_eng_page[1] ? 2 : 0) | (op1_eng_page[2] ? 4 : 0);
    tr_screen_place(kn, w + 5, pg, dt);                 /* moves the rig nodes before the SVG is drawn */
    op1_screen_draw(scr, dt);
    tr_screen_draw(op1_raster());
}
const op1_screen_t SCREEN_toro = { tr_screen_init, tr_update };
#endif
