/* meikyu screen: THREAD (GUI thread). Ariadne's thread runs the corridors of a round labyrinth toward the bull at
 * its centre. meikyu.svg holds the static walls and the bull; this file draws everything that moves, and the
 * readouts. Every control has its own colour and its own shape, so colour is never the only clue:
 *   CORR  blue   a diamond on each step CORRUPT rewrote, kept for one pass (LOCK freezes it: the value reads LOCK)
 *   LEN   green  a gate (a bar with end posts) across the corridor after the last step; the thread rewinds there
 *   DENS  white  beads: a bead is a step that plays; accents solid, the rest outlined
 *   RNG   orange a wedge from the thread to each bead: the pitch, as distance from the corridor's middle
 *   Shift RATE   blue   how fast the thread runs bead to bead      GATE  green  a ring round the bead while it sounds
 *         SCL    white  readout; DRM when the synth bank plays pads    HOLD  orange a square-headed pin through the ball
 *   With theseus loaded the synth bank plays drum pads (DRM): beads turn into small square pads, so the switch shows
 *   without colour, and SCL reads DRM.
 *   MOD page (orange press): TGT / DPTH / BASE / SRC readouts; the drawing stays.
 *   Flipped steps (blue press) carry a small blue X on the thread.
 * Layout: the labyrinth is centred; the four readouts sit in the corners in encoder order (blue top-left, green
 * top-right, white bottom-left, orange bottom-right), each with an icon that is a miniature of its shape on screen.
 * Four pattern pips sit under the labyrinth's lower left, the playing pattern solid.
 * Readouts never move with the drawing. Text and icons stay at or above y 140; the drawing stays inside 8..312 x 8..144
 * and at least 2 units from any readout (checked in the host preview, screen_host.c).
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "../../op1_plugin.h"
#include "../../seq/seq_text.h"
#include "knscr.h"
#include "meikyu_core.h"
#ifdef OP1_HOST
void host_mark(int readout);                            /* preview only: tags readout ink for the checks */
#else
#define host_mark(x) ((void)0)
#endif

static const uint8_t C_BLUE[4] = { 105, 142, 255, 255 }, C_GREEN[4] = { 0, 237, 149, 255 }, C_WHITE[4] = { 255, 255, 255, 255 },
                     C_ORANGE[4] = { 255, 66, 63, 255 }, C_SEC[4] = { 185, 185, 201, 255 }, C_DIM[4] = { 58, 56, 70, 255 },
                     C_DEC[4] = { 98, 93, 119, 255 };
static const uint8_t *const C_ENC[4] = { C_BLUE, C_GREEN, C_WHITE, C_ORANGE };
#define W15 OP1_Q10(1.5)
#define W2  OP1_Q10(2)

/* ---- labyrinth geometry (must match gen_svg.py) ---- */
#define TH_CX OP1_Q10(160)
#define TH_CY OP1_Q10(74)
static const int32_t TH_R[4] = { OP1_Q10(55), OP1_Q10(43), OP1_Q10(31), OP1_Q10(19) };
static const int32_t TH_W[5] = { OP1_Q10(61), OP1_Q10(49), OP1_Q10(37), OP1_Q10(25), OP1_Q10(13) };
#define TH_SP 1761                                      /* angle per step: a quarter turn x 0.86 */
static int32_t th_start(int r)                          /* angle where ring r starts (the drop from ring r - 1) */
{
    int32_t a = KS_TURN / 4;
    for (int k = 1; k <= r; k++) a += (k & 1 ? 4 : -4) * TH_SP;
    return a;
}
static int th_dir(int r) { return r & 1 ? -1 : 1; }
static int32_t th_ang(int32_t u)                        /* u: position along the path in steps, Q10 */
{
    int r = ks_clamp(u >> 10, 0, 15) >> 2;
    return th_start(r) + th_dir(r) * (((u - (r << 12)) * TH_SP) >> 10);
}
static ks_p th_pt(int32_t u, int32_t off)               /* point on the corridor of u, off from its middle (Q10) */
{
    int r = ks_clamp(u >> 10, 0, 15) >> 2;
    int32_t a = th_ang(u), rad = TH_R[r] + off;
    ks_p p = { TH_CX + ((rad >> 5) * ks_cos(a) >> 10), TH_CY + ((rad >> 5) * ks_sin(a) >> 10) };
    return p;
}

/* ---- readouts: corner slots, icon on the outer edge ---- */
typedef struct { int16_t nx, ny, vx, vy, ix; uint8_t right; } slot_t;
static const slot_t SLOT[4] = {                          /* blue TL, green TR, white BL, orange BR; baselines */
    { 23, 23, 9, 47, 9, 0 }, { 297, 23, 311, 47, 301, 1 }, { 23, 114, 9, 138, 9, 0 }, { 297, 114, 311, 138, 301, 1 } };
enum { IC_CORR, IC_LEN, IC_DENS, IC_RNG, IC_RATE, IC_GATE, IC_SCL, IC_HOLD, IC_TGT, IC_DPTH, IC_BASE, IC_SRC };
static void icon(void *r, int id, int32_t x, int32_t y, ks_c c)   /* 10 x 10 box, top-left (x, y), Q10 */
{
    ks_m m = { 4096, 0, 0, 4096, x, y };
#define P(px, py) ks_ap(&m, OP1_Q10(px), OP1_Q10(py))
    switch (id) {
    case IC_CORR: { ks_p p[4] = { P(5, 0.8), P(9.2, 5), P(5, 9.2), P(0.8, 5) }; ks_poly(r, p, 4, 1, W15, c); break; }
    case IC_LEN: ks_ln(r, P(1, 5), P(9, 5), W15, c); ks_ln(r, P(1, 1.5), P(1, 8.5), W15, c); ks_ln(r, P(9, 1.5), P(9, 8.5), W15, c); break;
    case IC_DENS: ks_disc(r, &m, OP1_Q10(2.8), OP1_Q10(5), OP1_Q10(2.3), c); ks_ring(r, &m, OP1_Q10(7.4), OP1_Q10(5), OP1_Q10(2), OP1_Q10(2), W15, c); break;
    case IC_RNG: { ks_p p[3] = { P(1, 1.2), P(9.4, 5), P(1, 8.8) }; ks_fill(r, p, 3, c); break; }
    case IC_RATE: { ks_p p[3] = { P(5.8, 2.2), P(9, 5), P(5.8, 7.8) }; ks_ln(r, P(0.8, 5), P(8.6, 5), W15, c); ks_poly(r, p, 3, 0, W15, c); break; }
    case IC_GATE: ks_ring(r, &m, OP1_Q10(5), OP1_Q10(5), OP1_Q10(4), OP1_Q10(4), W15, c); ks_disc(r, &m, OP1_Q10(5), OP1_Q10(5), OP1_Q10(1.5), c); break;
    case IC_SCL: { ks_p p[6] = { P(0.8, 9), P(3.6, 9), P(3.6, 5.5), P(6.4, 5.5), P(6.4, 2), P(9.2, 2) }; ks_poly(r, p, 6, 0, W15, c); break; }
    case IC_HOLD: { ks_p p[4] = { P(0.8, 0.8), P(5.2, 0.8), P(5.2, 5.2), P(0.8, 5.2) }; ks_poly(r, p, 4, 1, W15, c); ks_ln(r, P(3, 5.2), P(8.8, 9.2), W15, c); break; }
    case IC_TGT: ks_ring(r, &m, OP1_Q10(5), OP1_Q10(5), OP1_Q10(3.6), OP1_Q10(3.6), W15, c); ks_ln(r, P(5, 0.4), P(5, 9.6), W15, c); ks_ln(r, P(0.4, 5), P(9.6, 5), W15, c); break;
    case IC_DPTH: { ks_p a[3] = { P(2.4, 3.2), P(5, 0.8), P(7.6, 3.2) }, b[3] = { P(2.4, 6.8), P(5, 9.2), P(7.6, 6.8) };
        ks_ln(r, P(5, 0.8), P(5, 9.2), W15, c); ks_poly(r, a, 3, 0, W15, c); ks_poly(r, b, 3, 0, W15, c); break; }
    case IC_BASE: ks_ln(r, P(0.8, 8.6), P(9.2, 8.6), W15, c); ks_ln(r, P(5, 8.6), P(5, 2), W15, c); ks_ln(r, P(3, 4), P(7, 4), W15, c); break;
    case IC_SRC: { ks_p p[5] = { P(0.8, 7), P(3, 2.6), P(5, 7), P(7, 2.6), P(9.2, 7) }; ks_poly(r, p, 5, 0, W15, c); break; }
    }
#undef P
}
static void readout(void *r, int k, int ic, const char *name, const char *val, const char *unit)
{
    const slot_t *s = &SLOT[k];
    ks_c c = C_ENC[k];
    int32_t nw = ks_text_w(name, KS_XS), vw = ks_text_w(val, KS_S), uw = *unit ? ks_text_w(unit, KS_XS) + OP1_Q10(1.5) : 0;
    int32_t nx = OP1_Q10(s->nx) - (s->right ? nw : 0), vx = OP1_Q10(s->vx) - (s->right ? vw + uw : 0);
    sq_text(r, nx, OP1_Q10(s->ny) - 6 * KS_XS, KS_XS, name, KS_TW, c);
    sq_text(r, vx, OP1_Q10(s->vy) - 6 * KS_S, KS_S, val, KS_TW, c);
    if (*unit) sq_text(r, vx + vw + OP1_Q10(1.5), OP1_Q10(s->vy) - 6 * KS_XS, KS_XS, unit, KS_TW, c);
    icon(r, ic, OP1_Q10(s->ix), OP1_Q10(s->ny) - OP1_Q10(11.5), c);
}

/* ---- animation state: frames since the playhead moved ---- */
static int32_t sc_abs = -2, sc_age = 99;
static const uint8_t EASE[6] = { 12, 8, 6, 5, 4, 3 };   /* frames the thread takes per step, by RATE */
static const char *const RATE_S[6] = { "1/4", "1/8", "8T", "1/16", "16T", "1/32" };
static const char *const SCL_S[10] = { "KEY", "MAJ", "MIN", "DOR", "PHR", "MIX", "PN+", "PN-", "BLU", "CHR" };
/* MOD targets by the engine meikyu finds in the synth slot: toro's and theseus's knob names, else the colours */
static const char *const TGT_S[3][5] = { { "OFF", "BLU", "GRN", "WHT", "ORG" }, { "OFF", "SPRD", "FOLD", "CUT", "RES" },
                                         { "OFF", "DCY", "PNCH", "CUT", "FM" } };
static const char *const SRC_S[3] = { "LANE", "PTCH", "ACC" };

void mz_screen(void *r, int pos, int shift)
{
    mz_bank_t *b = mz_bank(pos);
    const mz_pat_t *q = &b->pats[b->pat & (MZ_PATS - 1)];
    int len = ks_clamp(q->length, 2, MZ_STEPS), run = b->running && b->step >= 0, drm = !(pos & 1) && b->style == MZ_STYLE_DRM;
    if (b->abs != sc_abs) { sc_abs = b->abs; sc_age = 0; } else if (sc_age < 99) sc_age++;
    ks_m id = { 4096, 0, 0, 4096, 0, 0 };

    /* the thread: eases to the playhead at RATE; at the gate it rewinds to the start */
    int32_t u = 0;
    if (run) {
        int32_t e = EASE[b->rate % 6], f = ks_min(KS_ONE, (sc_age << 10) / e);
        u = b->step == 0 ? (((len << 10) - 512) * (KS_ONE - f) >> 10) + (f >> 1) : (b->step << 10) - 512 + f;
    }
    {
        ks_p pts[112]; int n = 0;
        pts[n++] = (ks_p){ TH_CX, OP1_Q10(138) };
        pts[n++] = (ks_p){ TH_CX, TH_CY + TH_R[0] };
        for (int ring = 0; ring < 4; ring++) {
            int32_t span = ks_min(4 << 10, u - (ring << 12));
            if (span <= 0) break;
            int segs = ks_max(2, (span * 6) >> 10);
            for (int j = 1; j <= segs && n < 110; j++) {
                int32_t uu = (ring << 12) + span * j / segs;
                if (span >= (4 << 10) && j == segs) uu -= 1;
                pts[n++] = th_pt(uu, 0);
            }
            if (span >= (4 << 10) && ring < 3 && n < 111) pts[n++] = th_pt((ring + 1) << 12, 0);
        }
        ks_poly(r, pts, n, 0, W15, C_SEC);
        if (run) ks_ring(r, &id, pts[n - 1].x, pts[n - 1].y, OP1_Q10(4), OP1_Q10(4), W15, C_SEC);   /* playhead */
    }

    /* steps: dims past LEN; beads (DENS), wedges (RNG), knots (CORR), flips, the sounding ring (GATE) */
    for (int i = 0; i < MZ_STEPS; i++) {
        int32_t um = (i << 10) + 512;
        ks_p c = th_pt(um, 0);
        if (i >= len) { ks_disc(r, &id, c.x, c.y, OP1_Q10(1.4), C_DIM); continue; }
        int32_t fr = (int32_t)q->val[i] * q->range;                                  /* 0..225 */
        int32_t off = ((fr * 1024 / 225 - 512) * 23) >> 1;                           /* +-5.75 units */
        ks_p bp = th_pt(um, off);
        if (mz_step_plays(q, i)) {
            if (ks_abs(off) > OP1_Q10(1.2)) {                                         /* wedge from the thread to the bead */
                int32_t dx = bp.x - c.x, dy = bp.y - c.y, L = (int32_t)ks_isqrt((uint32_t)((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) << 4;
                if (L > 0) {
                    int32_t ux = dx * 1024 / L, uy = dy * 1024 / L;                  /* Q10 unit vector */
                    ks_p t[3] = { { c.x - ((uy * 2458) >> 10), c.y + ((ux * 2458) >> 10) },
                                  { bp.x - ((ux * 2458) >> 10), bp.y - ((uy * 2458) >> 10) },
                                  { c.x + ((uy * 2458) >> 10), c.y - ((ux * 2458) >> 10) } };
                    ks_fill(r, t, 3, C_ORANGE);
                }
            }
            if (drm) {                                                                /* DRM: square pads */
                int32_t d = OP1_Q10(2.1);
                ks_p sq[4] = { { bp.x - d, bp.y - d }, { bp.x + d, bp.y - d }, { bp.x + d, bp.y + d }, { bp.x - d, bp.y + d } };
                if (mz_step_accent(q, i)) ks_fill(r, sq, 4, C_WHITE); else ks_poly(r, sq, 4, 1, W15, C_WHITE);
            }
            else if (mz_step_accent(q, i)) ks_disc(r, &id, bp.x, bp.y, OP1_Q10(2.1), C_WHITE);
            else ks_ring(r, &id, bp.x, bp.y, OP1_Q10(2.1), OP1_Q10(2.1), W15, C_WHITE);
            if (run && i == b->step && b->sounding) {
                int32_t rr = OP1_Q10(4.5) + ks_min(sc_age, 6) * OP1_Q10(0.4);
                ks_ring(r, &id, bp.x, bp.y, rr, rr, W15, C_GREEN);
            }
        }
        if ((b->rewrote >> i) & 1) {
            int fresh = run && i == b->step && sc_age < 8;
            int32_t d = fresh ? OP1_Q10(5.5) : OP1_Q10(4.2);
            ks_p p[4] = { { c.x, c.y - d }, { c.x + d, c.y }, { c.x, c.y + d }, { c.x - d, c.y } };
            ks_poly(r, p, 4, 1, fresh ? W2 : W15, C_BLUE);
        }
        if ((q->flip >> i) & 1) {
            int32_t d = OP1_Q10(2.2);
            sq_line(r, c.x - d, c.y - d, c.x + d, c.y + d, W15, C_BLUE);
            sq_line(r, c.x - d, c.y + d, c.x + d, c.y - d, W15, C_BLUE);
        }
    }

    /* LEN: the gate across the corridor half a step past the last step */
    {
        int ring = (len - 1) >> 2;
        int32_t a = th_ang(((len - 1) << 10) + 512) + th_dir(ring) * (TH_SP >> 1);
        int32_t ri = TH_W[ring + 1] + OP1_Q10(2), ro = TH_W[ring] - OP1_Q10(2), ca = ks_cos(a), sa = ks_sin(a);
        ks_p p0 = { TH_CX + ((ri >> 5) * ca >> 10), TH_CY + ((ri >> 5) * sa >> 10) };
        ks_p p1 = { TH_CX + ((ro >> 5) * ca >> 10), TH_CY + ((ro >> 5) * sa >> 10) };
        int32_t tx = -((OP1_Q10(2.6) >> 5) * sa >> 10), ty = (OP1_Q10(2.6) >> 5) * ca >> 10;
        ks_ln(r, p0, p1, W2, C_GREEN);
        sq_line(r, p0.x - tx, p0.y - ty, p0.x + tx, p0.y + ty, W2, C_GREEN);
        sq_line(r, p1.x - tx, p1.y - ty, p1.x + tx, p1.y + ty, W2, C_GREEN);
    }

    /* the ball of yarn shrinks as the thread runs out; HOLD pins it */
    {
        int32_t br = OP1_Q10(5.5) - ((u * 174) >> 10), by = OP1_Q10(138) - br;
        ks_ring(r, &id, TH_CX, by, br, br, W15, C_SEC);
        sq_line(r, TH_CX - (br * 6 / 10), by - (br / 2), TH_CX + (br * 6 / 10), by + (br / 2), W15, C_SEC);
        if (b->latched) {
            sq_line(r, TH_CX + OP1_Q10(9), OP1_Q10(140), TH_CX - OP1_Q10(1), OP1_Q10(130), W2, C_ORANGE);
            ks_p h[4] = { { TH_CX - OP1_Q10(4.5), OP1_Q10(125.5) }, { TH_CX, OP1_Q10(125.5) }, { TH_CX, OP1_Q10(130) }, { TH_CX - OP1_Q10(4.5), OP1_Q10(130) } };
            ks_poly(r, h, 4, 1, W2, C_ORANGE);
        }
    }
    host_mark(2);
    for (int i = 0; i < MZ_PATS; i++) {                 /* pattern pips under the labyrinth's lower left: the playing one solid */
        int32_t x = OP1_Q10(89 + i * 5), y = OP1_Q10(128);
        ks_p sq[4] = { { x, y }, { x + OP1_Q10(3), y }, { x + OP1_Q10(3), y + OP1_Q10(3) }, { x, y + OP1_Q10(3) } };
        if (i == (b->pat & 3)) ks_fill(r, sq, 4, C_SEC); else ks_poly(r, sq, 4, 1, OP1_Q10(1), C_DEC);
    }
    host_mark(1);

    /* readouts */
    char v[8], *p;
    if (shift) {
        readout(r, 0, IC_RATE, "RATE", RATE_S[b->rate % 6], "");
        p = sq_num(v, b->gate, 1); (void)p; readout(r, 1, IC_GATE, "GATE", v, "%");
        readout(r, 2, IC_SCL, "SCL", pos & 1 ? "DRUM" : (drm ? "DRM" : SCL_S[b->scale % 10]), "");
        readout(r, 3, IC_HOLD, "HOLD", b->latched ? "ON" : "OFF", "");
    } else if (b->page) {
        readout(r, 0, IC_TGT, "TGT", TGT_S[pos & 1 ? 0 : ks_clamp(mz_voice(), 0, 2)][b->mod_target % 5], "");
        sq_num(v, b->mod_depth, 1); readout(r, 1, IC_DPTH, "DPTH", v, "");
        sq_num(v, b->mod_base, 1); readout(r, 2, IC_BASE, "BASE", v, "");
        readout(r, 3, IC_SRC, "SRC", SRC_S[b->mod_src % 3], "");
    } else {
        if (q->lock) readout(r, 0, IC_CORR, "CORR", "LOCK", "");
        else { sq_num(v, q->corrupt, 1); readout(r, 0, IC_CORR, "CORR", v, "%"); }
        sq_num(v, (uint32_t)len, 1); readout(r, 1, IC_LEN, "LEN", v, "");
        sq_num(v, q->density, 1); readout(r, 2, IC_DENS, "DENS", v, "%");
        sq_num(v, q->range, 1); readout(r, 3, IC_RNG, "RNG", v, "");
    }
    host_mark(0);
}
