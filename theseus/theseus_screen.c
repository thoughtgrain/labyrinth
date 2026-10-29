/* theseus screen: PHALANX (GUI thread). A crested Corinthian helmet playing a five-piece kit. theseus.svg
 * holds the kit's shells, heads and stands and the drummer's cuirass; this file draws the helmet (it turns), the
 * cymbals, the crest, the eyes, the arms and sticks, the kick beater,
 * the ripples and the readouts. Motion follows the hits the engine plays (theseus_hits / _zone / _lfo from
 * theseus.c): the zone of each hit picks the drum the sticks land on, the same way it picks the sound, and nothing
 * moves without a hit.
 * Each control's colour goes on parts only it moves, in shapes no other control uses:
 *   DCY   blue   ripple rings spread from the struck head; more rings that last longer as DCY goes up
 *                NOIS (page 2) blue sizzle specks fly off each hit
 *   PNCH  green  the kick beater: the harder the punch, the further it winds back and the bigger the ball
 *                LFO (page 2) green sway: the head slides sideways with the hit LFO, with motion arcs trailing it
 *   CUT   white  the helmet's eyes: round embers that grow and burn brighter; strong hits throw short rays (round, so
 *                they never read like toro's slanted bull slits)
 *                RATE (page 2) how fast the sway swings (it follows the LFO itself); the readout gives hits per cycle
 *   FM    orange the crest: its bristles grow, go jagged and shake on every hit
 * (page-2 words always act on the drawing, as they always act on the sound; a press only swaps the readout)
 * The helmet turns to look at the drum being played: left (mirrored profile) for hats and snare, front for the kick,
 * right for the toms and cymbal. It never rotates or scales: it swaps between five drawn heads (left profile, left
 * 3/4, front, right 3/4, right profile), one step every two frames, so a turn passes through the in-between heads.
 * After 1.5 s without a hit it faces front.
 * Readouts sit in the corners nearest what they drive: DCY top-left over the hats, FM top-right by the crest's
 * plume, CUT bottom-left, PNCH bottom-right by the pedal, each with an icon that is a miniature of its part. They never
 * move. Text and icons stay at or above y 140; the drawing stays inside 8..312 x 8..144 and at least 2 units from any
 * readout (checked in the host preview, host/screen_host.c).
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "op1_plugin.h"
#include "seq/seq_text.h"
#include "knscr.h"
#ifdef OP1_HOST
void host_mark(int readout);
#else
#define host_mark(x) ((void)0)
#endif

extern volatile uint32_t theseus_hits;                  /* theseus.c */
extern volatile int16_t theseus_zone, theseus_vel, theseus_lfo;

static const uint8_t C_BLUE[4] = { 105, 142, 255, 255 }, C_GREEN[4] = { 0, 237, 149, 255 }, C_WHITE[4] = { 255, 255, 255, 255 },
                     C_ORANGE[4] = { 255, 66, 63, 255 }, C_SEC[4] = { 185, 185, 201, 255 }, C_DEC[4] = { 98, 93, 119, 255 },
                     C_EYE[4] = { 12, 11, 16, 255 }, C_HELM[4] = { 71, 67, 90, 255 };
static const uint8_t *const C_ENC[4] = { C_BLUE, C_GREEN, C_WHITE, C_ORANGE };
#define W1  OP1_Q10(1)
#define W15 OP1_Q10(1.5)
#define W2  OP1_Q10(2)
#define Q   OP1_Q10                                     /* constants only */

/* ---- geometry (must match gen_svg.py) ---- */
#define TH_PX   Q(158)                                  /* helmet neck pivot on screen */
#define TH_PY   Q(66)
#define TH_HS   4506                                    /* helmet scale 1.1, Q12 */
#define TH_ARM  Q(21)                                   /* upper arm and forearm */
enum { D_KICK, D_FLOOR, D_RACK, D_SNARE, D_HAT, D_CYM, D_N };
typedef struct { int16_t cx, cy, rx, ry, tipdx, hx, hy; int8_t side; } drum_t;   /* side: 0 left hand, 1 right, 2 foot */
static const drum_t DRUM[D_N] = {
    { 160, 114, 22, 22,  0,   0,   0, 2 },
    { 207, 108, 13,  4, -3, -13,  -9, 1 },
    { 190,  88, 11,  4, -2,  -9, -12, 1 },
    { 122,  98, 14,  4,  2,  11, -11, 0 },
    {  94,  77, 13,  3,  5,  13,  -8, 0 },
    { 230,  66, 17,  3, -7, -14,  -6, 1 },
};
static const int16_t SHOULDER[2][2] = { { 146, 93 }, { 172, 93 } }, REST[2][4] = { { 134, 80, 124, 91 }, { 183, 78, 191, 87 } };   /* hand x y, tip x y */

/* the drum a zone lands on: the same split the engine's sound makes (low = kick .. high = metal) */
static int zone_drum(int32_t z)
{
    return z < 4600 ? D_KICK : z < 6100 ? D_FLOOR : z < 11500 ? D_RACK : z < 16000 ? D_SNARE : z < 22000 ? D_HAT : D_CYM;
}

typedef struct {
    int32_t dcy, pnch, cut, fm, nois, lfo, rate;        /* Q10 0..1 */
    int page2;                                          /* bit i: knob i shows its page-2 word */
    int16_t raw[7];
} kn_t;
typedef struct { int8_t drum; int32_t t, vel; } hit_t;  /* ms since the hit, velocity Q10 */

static kn_t sc_k;
static hit_t sc_hit[4] = { { -1, 9000, 0 }, { -1, 9000, 0 }, { -1, 9000, 0 }, { -1, 9000, 0 } };   /* newest first */
static int32_t sc_side_t[3] = { 9000, 9000, 9000 };     /* ms since each hand / the foot last struck */
static int8_t  sc_side_d[3] = { D_SNARE, D_RACK, D_KICK };
static int32_t sc_any_t = 9000, sc_sway;              /* sway: sideways offset, Q10 units */
static int8_t  sc_view, sc_view_to, sc_view_hold;     /* head: -2 left, -1 left 3/4, 0 front, 1 right 3/4, 2 right */
static uint32_t sc_hits, sc_n;

/* ---- small helpers ---- */
static void bz(void *r, const ks_m *m, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
               int n, int32_t w, ks_c c)
{
    const int32_t k[8] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    ks_curve(r, m, k, n, w, c);
}
static int32_t len16(int32_t dx, int32_t dy) { return (int32_t)ks_isqrt((uint32_t)((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) << 4; }
/* 0 for the first 40 ms after a strike (the stick stays on the head), easing to 1 by 260 ms */
static int32_t recoil(int32_t t)
{
    if (t <= 40) return 0;
    if (t >= 260) return 1024;
    int32_t u = (t - 40) * 1024 / 220;
    return (u * (2048 - u)) >> 10;                      /* ease out */
}
static int32_t lerp(int32_t a, int32_t b, int32_t f) { return a + (((b - a) * f) >> 10); }

/* an outlined limb shoulder -> elbow -> hand (screen units), tapering */
static void arm(void *r, ks_p s, ks_p h, int side)
{
    int32_t dx = h.x - s.x, dy = h.y - s.y, d = len16(dx, dy);
    if (d < Q(2)) d = Q(2);
    if (d > 2 * TH_ARM - Q(1)) { h.x = s.x + (int32_t)(((int64_t)dx * (2 * TH_ARM - Q(1))) / d); h.y = s.y + (int32_t)(((int64_t)dy * (2 * TH_ARM - Q(1))) / d); dx = h.x - s.x; dy = h.y - s.y; d = 2 * TH_ARM - Q(1); }
    int32_t half = d >> 1, off = (int32_t)ks_isqrt((uint32_t)(((TH_ARM >> 4) * (TH_ARM >> 4)) - ((half >> 4) * (half >> 4)))) << 4;
    int32_t nx = -dy * 1024 / d, ny = dx * 1024 / d;                    /* Q10 unit normal */
    if (ny < 0 || (ny == 0 && side == 0)) { nx = -nx; ny = -ny; }      /* elbows hang down */
    if (side == 0 && nx > 0) nx = -nx / 2;                              /* and out to their own side */
    if (side == 1 && nx < 0) nx = -nx / 2;
    ks_p e = { s.x + dx / 2 + ((nx * off) >> 10), s.y + dy / 2 + ((ny * off) >> 10) };
    ks_p j[3] = { s, e, h };
    static const int32_t w[3] = { Q(2.6), Q(2.1), Q(1.6) };
    ks_p dir[3] = { { e.x - s.x, e.y - s.y }, { h.x - s.x, h.y - s.y }, { h.x - e.x, h.y - e.y } };   /* along the limb at each joint */
    for (int sd = -1; sd <= 1; sd += 2) {
        ks_p L[3];
        for (int i = 0; i < 3; i++) {
            int32_t l = len16(dir[i].x, dir[i].y);
            if (l < 16) l = 16;
            L[i].x = j[i].x + sd * (int32_t)(((int64_t)-dir[i].y * w[i]) / l);
            L[i].y = j[i].y + sd * (int32_t)(((int64_t)dir[i].x * w[i]) / l);
        }
        ks_poly(r, L, 3, 0, W15, C_SEC);
    }
    ks_m id = { 4096, 0, 0, 4096, 0, 0 };
    ks_ring(r, &id, s.x, s.y, Q(3), Q(3), W15, C_SEC);                  /* shoulder guard */
    ks_disc(r, &id, h.x, h.y, Q(2.2), C_SEC);                           /* fist */
}

/* ---- readouts ---- */
typedef struct { int16_t nx, ny, vx, vy, ix; uint8_t right; } slot_t;
static const slot_t SLOT[4] = {                          /* blue TL, green BR, white BL, orange TR; baselines */
    { 23, 23, 9, 47, 9, 0 }, { 297, 114, 311, 138, 301, 1 }, { 23, 114, 9, 138, 9, 0 }, { 297, 23, 311, 47, 301, 1 } };
enum { IC_DCY, IC_PNCH, IC_CUT, IC_FM, IC_NOIS, IC_LFO, IC_RATE };
static void icon(void *r, int id, int32_t x, int32_t y, ks_c c)
{
    ks_m m = { 4096, 0, 0, 4096, x, y };
#define P(px, py) ks_ap(&m, Q(px), Q(py))
    switch (id) {
    case IC_DCY: ks_ring(r, &m, Q(5), Q(5), Q(4.2), Q(4.2), W15, c); ks_ring(r, &m, Q(5), Q(5), Q(1.8), Q(1.8), W15, c); break;
    case IC_PNCH: ks_disc(r, &m, Q(5), Q(3), Q(2.6), c); ks_ln(r, P(5, 4), P(5, 9.4), W15, c); ks_ln(r, P(1.5, 9.4), P(8.5, 9.4), W15, c); break;
    case IC_CUT: ks_disc(r, &m, Q(5), Q(5), Q(2.2), c);
        ks_ln(r, P(5, 0.4), P(5, 1.6), W15, c); ks_ln(r, P(5, 8.4), P(5, 9.6), W15, c);
        ks_ln(r, P(0.4, 5), P(1.6, 5), W15, c); ks_ln(r, P(8.4, 5), P(9.6, 5), W15, c); break;
    case IC_FM: bz(r, &m, Q(1), Q(9), Q(1), Q(4), Q(5), Q(2.5), Q(9), Q(4), 6, W15, c);
        ks_ln(r, P(2.2, 5.4), P(0.6, 1.4), W15, c); ks_ln(r, P(4.6, 3.4), P(4.6, 0.6), W15, c); ks_ln(r, P(7.2, 3.4), P(9.4, 0.8), W15, c); break;
    case IC_NOIS: { static const int8_t d[5][2] = { { 2, 2 }, { 7, 1 }, { 5, 5 }, { 1, 8 }, { 8, 8 } };
        for (int i = 0; i < 5; i++) sq_line(r, x + Q(d[i][0]), y + Q(d[i][1]), x + Q(d[i][0]) + Q(0.4), y + Q(d[i][1]), W2, c);
        break; }
    case IC_LFO: bz(r, &m, Q(0.6), Q(5), Q(3), Q(-1), Q(7), Q(11), Q(9.4), Q(5), 8, W15, c); break;
    case IC_RATE: { ks_p p[3] = { P(2, 9.4), P(5, 0.8), P(8, 9.4) }; ks_poly(r, p, 3, 1, W15, c); ks_ln(r, P(5, 7.6), P(7.8, 2.6), W15, c); break; }
    }
#undef P
}
static void readout(void *r, int k, int ic, const char *name, const char *v)
{
    const slot_t *s = &SLOT[k];
    ks_c c = C_ENC[k];
    int32_t nw = ks_text_w(name, KS_XS), vw = ks_text_w(v, KS_S);
    sq_text(r, Q(s->nx) - (s->right ? nw : 0), Q(s->ny) - 6 * KS_XS, KS_XS, name, KS_TW, c);
    sq_text(r, Q(s->vx) - (s->right ? vw : 0), Q(s->vy) - 6 * KS_S, KS_S, v, KS_TW, c);
    icon(r, ic, Q(s->ix), Q(s->ny) - Q(11.5), c);
}
static void readout99(void *r, int k, int ic, const char *name, int32_t q15)
{
    char v[6]; sq_num(v, (uint32_t)(ks_clamp(q15, 0, 32767) * 99 / 32767), 1);
    readout(r, k, ic, name, v);
}

/* ---- per frame ---- */
void th_screen_init(void *scr) { (void)scr; }

/* The helmet's frame: no rotation, no scaling. It slides sideways with the LFO and bobs on each hit; mirror = -1
 * draws a right-facing head facing left. */
static ks_m helm_m(int mirror)
{
    int32_t bob = (Q(2.5) * ks_expneg(sc_any_t * 1024 / 90)) >> 15;
    ks_m H = { mirror * TH_HS, 0, 0, TH_HS, TH_PX + sc_sway, TH_PY + bob };
    return H;
}

/* knobs: live page-1 values and the page-2 words; page2: bits of knobs showing page 2; dt: ms since the last frame.
 * Advances the animation: call before op1_screen_draw, then th_screen_draw after it. */
void th_screen_place(const int16_t *kn4, const int16_t *page2w, int page2, int dt)
{
    kn_t *k = &sc_k;
    k->dcy = (ks_max(kn4[0], 0) * 1025) >> 15; k->pnch = (ks_max(kn4[1], 0) * 1025) >> 15;
    k->cut = (ks_max(kn4[2], 0) * 1025) >> 15; k->fm = (ks_max(kn4[3], 0) * 1025) >> 15;
    k->nois = (ks_max(page2w[0], 0) * 1025) >> 15; k->lfo = (ks_max(page2w[1], 0) * 1025) >> 15; k->rate = (ks_max(page2w[2], 0) * 1025) >> 15;
    k->page2 = page2;
    for (int i = 0; i < 4; i++) k->raw[i] = kn4[i];
    for (int i = 0; i < 3; i++) k->raw[4 + i] = page2w[i];
    if (dt < 1 || dt > 100) dt = 16;
    for (int i = 0; i < 4; i++) if (sc_hit[i].t < 60000) sc_hit[i].t += dt;
    for (int i = 0; i < 3; i++) if (sc_side_t[i] < 60000) sc_side_t[i] += dt;
    if (sc_any_t < 60000) sc_any_t += dt;

    uint32_t hits = theseus_hits;
    if (hits != sc_hits) {                              /* a new hit (or several since the last frame: show the latest) */
        sc_hits = hits; sc_n++;
        int d = zone_drum(theseus_zone);
        for (int i = 3; i > 0; i--) sc_hit[i] = sc_hit[i - 1];
        sc_hit[0].drum = (int8_t)d; sc_hit[0].t = 0; sc_hit[0].vel = ks_clamp((theseus_vel * 1025) >> 15, 256, 1024);
        int s = DRUM[d].side;
        sc_side_t[s] = 0; sc_side_d[s] = (int8_t)d; sc_any_t = 0;
        sc_view_to = (int8_t)(s == 0 ? -2 : s == 1 ? 2 : 0);   /* look at the drum: hats and snare left, kick front, toms and cymbal right */
    }
    if (sc_any_t > 1500) sc_view_to = 0;                /* idle: back to the audience */
    /* the head swaps one drawing at a time toward the drum, holding each for two frames, so a turn from left to
     * right shows left, left 3/4, front, right 3/4, right */
    if (sc_view != sc_view_to && ++sc_view_hold >= 2) { sc_view += sc_view_to > sc_view ? 1 : -1; sc_view_hold = 0; }
    /* LFO sway: the head slides sideways with the hit LFO, up to 4 units at full depth, eased so a step reads */
    int32_t target = (((int32_t)theseus_lfo * k->lfo) >> 10) * Q(4) >> 15;
    sc_sway += (target - sc_sway) / 4;
}

static void cymbal(void *r, int d, int32_t wob, int lift)
{
    const drum_t *g = &DRUM[d];
    ks_m T = ks_rot(wob, 4096, Q(g->cx), Q(g->cy));
    ks_ring(r, &T, 0, 0, Q(g->rx), Q(g->ry) - Q(0.6), W15, C_SEC);
    bz(r, &T, Q(-3.4), Q(-0.6), Q(-3), Q(-3.6), Q(3), Q(-3.6), Q(3.4), Q(-0.6), 5, W15, C_SEC);   /* bell */
    if (d == D_HAT) {                                   /* the bottom hat, fixed; the top one lifts a little on a hit */
        ks_m B = { 4096, 0, 0, 4096, Q(g->cx), Q(g->cy) + Q(3) + lift };
        ks_ring(r, &B, 0, 0, Q(g->rx), Q(g->ry) - Q(0.6), W15, C_DEC);
    }
}

/* ---- the helmet: a Corinthian helm, drawn in helmet units (x 1.1 on screen, origin at the neck pivot). It turns to
 * look at the drum being played by swapping between five drawings: profile, three-quarter and front (the left ones
 * are the right ones mirrored). ---- */
#define BZ(o, H, a, b, c, d, e, f, g, h, first) bezq(o, H, Q(a), Q(b), Q(c), Q(d), Q(e), Q(f), Q(g), Q(h), first)
static int bezq(ks_p *out, const ks_m *H, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int first)
{
    const int32_t k[8] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    ks_p p[7]; ks_bez(p, H, k, 6);
    int n = 0;
    for (int i = first ? 0 : 1; i < 7; i++) out[n++] = p[i];
    return n;
}
#define AP(H, x, y) ks_ap(H, Q(x), Q(y))
/* FM: the crest. A dense horsehair plume on a curved stalk; the bristles lengthen, alternate long and short (jagged)
 * and shake on every hit as FM goes up. Outward is away from the helmet's crown, so it works mirrored too. */
static void crest(void *r, const ks_m *H, const int32_t *ck, int sweep, const kn_t *k)
{
    ks_p p[25]; ks_bez(p, H, ck, 24);
    ks_p c = AP(H, 0, -12);
    int32_t shake = (k->fm * ks_expneg(sc_any_t * 1024 / 260)) >> 15;   /* Q10 */
    for (int j = 1; j < 24; j++) {
        int32_t dx = p[j + 1].x - p[j - 1].x, dy = p[j + 1].y - p[j - 1].y, l = len16(dx, dy);
        if (l < 16) continue;
        int32_t tx = dx * 1024 / l, ty = dy * 1024 / l, nx = ty, ny = -tx;
        if (((int64_t)nx * (p[j].x - c.x) + (int64_t)ny * (p[j].y - c.y)) < 0) { nx = -nx; ny = -ny; }
        int32_t taper = j < 4 ? 400 + j * 150 : j > 20 ? 400 + (24 - j) * 150 : 1024;
        int32_t len = Q(6) + ((Q(3) * k->fm) >> 10) + ((j & 1) ? ((Q(3.5) * k->fm) >> 10) : 0);
        len += (((Q(2.5) * shake) >> 10) * ks_sin(sc_any_t * 30 + j * 900)) >> 15;
        len = (len * taper) >> 10;
        int32_t sw = (sweep ? (len * 3) / 10 : 0) + ((((Q(2) * shake) >> 10) * ks_cos(sc_any_t * 26 + j * 700)) >> 15);
        ks_p e = { p[j].x + ((nx * len) >> 10) + ((tx * sw) >> 10), p[j].y + ((ny * len) >> 10) + ((ty * sw) >> 10) };
        ks_ln(r, p[j], e, W15, C_ORANGE);
    }
    ks_poly(r, p, 25, 0, W2, C_SEC);                                       /* the stalk over the bristle roots */
}
/* CUT: a round ember of an eye, wider and brighter as CUT rises; a strong hit throws short rays off it. (Round on
 * purpose: toro's bull has slanted slits, so the two never read alike.) */
static void eye(void *r, const ks_m *H, int32_t x, int32_t y, int32_t sz, const kn_t *k)   /* sz: Q10 size */
{
    if (k->cut <= 60) { ks_ring(r, H, x, y, Q(1.1), Q(1.1), W1, C_DEC); return; }
    int32_t rad = ((Q(0.9) + ((Q(1.5) * k->cut) >> 10)) * sz) >> 10;
    ks_disc(r, H, x, y, rad, C_WHITE);
    if (k->cut > 600 && sc_any_t < 160) {
        int32_t r0 = rad + ((Q(1.2) * sz) >> 10), r1 = rad + ((((Q(2.4) + ((Q(1.2) * k->cut) >> 10))) * sz) >> 10);
        for (int i = 0; i < 6; i++) {
            int32_t a = i * (KS_TURN / 6) + KS_TURN / 12, ca = ks_cos(a), sa = ks_sin(a);
            ks_ln(r, ks_ap(H, x + (((r0 >> 5) * ca) >> 10), y + (((r0 >> 5) * sa) >> 10)),
                     ks_ap(H, x + (((r1 >> 5) * ca) >> 10), y + (((r1 >> 5) * sa) >> 10)), W1, C_WHITE);
        }
    }
}
static void helmet_profile(void *r, const ks_m *H, const kn_t *k)
{
    ks_p p[32]; int n = 0;
    /* fill: the bowl and neck guard, the nose guard, the cheek guard (each one interval per row, as ks_fill wants) */
    p[n++] = AP(H, -16, 14);
    n += BZ(p + n, H, -16, 14, -20, 4, -21, -10, -14, -18, 0);
    n += BZ(p + n, H, -14, -18, -8, -24, 6, -25, 13, -18, 0);
    n += BZ(p + n, H, 13, -18, 17, -14, 19, -9, 19.5, -4, 0);
    p[n++] = AP(H, 14, 4.5); p[n++] = AP(H, -4, 16);
    n += BZ(p + n, H, -4, 16, -8, 18, -12, 18, -16, 14, 0);
    ks_fill(r, p, n, C_HELM);
    ks_p nose[4] = { AP(H, 19.5, -4), AP(H, 21, 9), AP(H, 17, 10.5), AP(H, 14, 4.5) };
    ks_fill(r, nose, 4, C_HELM);
    n = 0; p[n++] = AP(H, 14, 4.5); p[n++] = AP(H, 12.5, 8);
    n += BZ(p + n, H, 12.5, 8, 15, 12, 18, 16, 19, 20, 0);
    p[n++] = AP(H, 12, 22);
    n += BZ(p + n, H, 12, 22, 6, 22, 0, 18, -4, 16, 0);
    ks_fill(r, p, n, C_HELM);
    /* the face opening: the eye slot, and the gap between nose guard and cheek guard down to the mouth */
    ks_p slot[5] = { AP(H, 6.5, -7), AP(H, 11, -10), AP(H, 19.6, -9.6), AP(H, 19.6, -5), AP(H, 11, -4.4) };
    ks_p gap[4] = { AP(H, 14.6, -5), AP(H, 17, -5), AP(H, 15.6, 4.8), AP(H, 13.4, 4.8) };
    ks_fill(r, slot, 5, C_EYE); ks_fill(r, gap, 4, C_EYE);
    /* outline and brow band */
    n = 0; p[n++] = AP(H, -16, 14);
    n += BZ(p + n, H, -16, 14, -20, 4, -21, -10, -14, -18, 0);
    n += BZ(p + n, H, -14, -18, -8, -24, 6, -25, 13, -18, 0);
    n += BZ(p + n, H, 13, -18, 17, -14, 19, -9, 19.5, -4, 0);
    p[n++] = AP(H, 21, 9); p[n++] = AP(H, 17, 10.5); p[n++] = AP(H, 14, 4.5); p[n++] = AP(H, 12.5, 8);
    ks_poly(r, p, n, 0, W15, C_SEC);
    n = 0; p[n++] = AP(H, 12.5, 8);
    n += BZ(p + n, H, 12.5, 8, 15, 12, 18, 16, 19, 20, 0);
    p[n++] = AP(H, 12, 22);
    n += BZ(p + n, H, 12, 22, 6, 22, 0, 18, -4, 16, 0);
    n += BZ(p + n, H, -4, 16, -8, 18, -12, 18, -16, 14, 0);
    ks_poly(r, p, n, 0, W15, C_SEC);
    n = BZ(p, H, -17, 2, -8, -2, 4, -3, 11, -4.4, 1);
    ks_poly(r, p, n, 0, W1, C_DEC);
    static const int32_t ck[8] = { Q(12), Q(-22), Q(7), Q(-38), Q(-17), Q(-39), Q(-28), Q(-13) };
    crest(r, H, ck, 1, k);
    eye(r, H, Q(13.5), Q(-7.2), 1024, k);
}
static void helmet_front(void *r, const ks_m *H, const kn_t *k)
{
    ks_p p[24]; int n = 0;
    n += BZ(p + n, H, -19.5, -4, -19.5, -17, -11, -24, 0, -24, 1);           /* the dome */
    n += BZ(p + n, H, 0, -24, 11, -24, 19.5, -17, 19.5, -4, 0);
    ks_fill(r, p, n, C_HELM);
    for (int sd = -1; sd <= 1; sd += 2) {                                   /* the cheek guards, the mouth gap between */
#define APS(x, y) ks_ap(H, sd * Q(x), Q(y))
        n = 0;
        p[n++] = APS(19.5, -4); p[n++] = APS(0, -4); p[n++] = APS(0, 9); p[n++] = APS(3, 9);
        p[n++] = APS(3, 23); p[n++] = APS(7, 23);
        n += bezq(p + n, H, sd * Q(7), Q(23), sd * Q(14), Q(21), sd * Q(19), Q(16), sd * Q(20), Q(6), 0);
        ks_fill(r, p, n, C_HELM);
        ks_p sl[4] = { APS(15.5, -9), APS(3, -10), APS(3, -4.5), APS(14.5, -4) };   /* eye slit */
        ks_fill(r, sl, 4, C_EYE);
        n = bezq(p, H, sd * Q(20), Q(6), sd * Q(19), Q(16), sd * Q(14), Q(21), sd * Q(7), Q(23), 1);
        p[n++] = APS(3, 23); p[n++] = APS(3, 9); p[n++] = APS(0, 9);
        ks_poly(r, p, n, 0, W15, C_SEC);
        ks_ln(r, APS(19.5, -4), APS(20, 6), W15, C_SEC);
#undef APS
    }
    n = BZ(p, H, -19.5, -4, -19.5, -17, -11, -24, 0, -24, 1);
    n += BZ(p + n, H, 0, -24, 11, -24, 19.5, -17, 19.5, -4, 0);
    ks_poly(r, p, n, 0, W15, C_SEC);
    static const int32_t ck[8] = { Q(-11), Q(-20), Q(-6), Q(-29), Q(6), Q(-29), Q(11), Q(-20) };
    crest(r, H, ck, 0, k);
    eye(r, H, Q(-9), Q(-7), 1024, k); eye(r, H, Q(9), Q(-7), 1024, k);
}
/* three-quarter view, facing right: the T-slot and nose guard sit right of centre, the far eye slit and cheek
 * foreshortened on the left, the near cheek guard full */
static void helmet_three_quarter(void *r, const ks_m *H, const kn_t *k)
{
    ks_p p[24]; int n = 0;
    n += BZ(p + n, H, -19, -3, -19.5, -17, -10, -24.5, 1, -24, 1);           /* the dome */
    n += BZ(p + n, H, 1, -24, 12, -23.5, 18.5, -16, 18.5, -3, 0);
    ks_fill(r, p, n, C_HELM);
    n = 0;                                                                  /* far cheek and neck guard */
    p[n++] = AP(H, -19, -3); p[n++] = AP(H, 3, -3); p[n++] = AP(H, 3, 23); p[n++] = AP(H, -3, 22);
    n += BZ(p + n, H, -3, 22, -10, 21, -16, 18, -18, 10, 0);
    ks_fill(r, p, n, C_HELM);
    ks_p nose[4] = { AP(H, 3, -3), AP(H, 6, -3), AP(H, 6, 9), AP(H, 3, 9) };
    ks_fill(r, nose, 4, C_HELM);
    n = 0;                                                                  /* near cheek */
    p[n++] = AP(H, 6, -3); p[n++] = AP(H, 18.5, -3); p[n++] = AP(H, 19, 7);
    n += BZ(p + n, H, 19, 7, 18.5, 15, 15, 20, 9, 22.5, 0);
    p[n++] = AP(H, 6, 22.5); p[n++] = AP(H, 6, 9);
    ks_fill(r, p, n, C_HELM);
    ks_p mouth[4] = { AP(H, 3, 9), AP(H, 6, 9), AP(H, 6, 22.5), AP(H, 3, 23) };   /* the gap between the cheeks */
    ks_fill(r, mouth, 4, C_EYE);
    ks_p ne[4] = { AP(H, 6, -9.6), AP(H, 16.5, -9), AP(H, 17.5, -4.5), AP(H, 6, -4.2) };      /* near eye slit */
    ks_p fe[4] = { AP(H, -9, -8.5), AP(H, 3, -9.6), AP(H, 3, -4.2), AP(H, -8.5, -4.2) };      /* far eye slit */
    ks_fill(r, ne, 4, C_EYE); ks_fill(r, fe, 4, C_EYE);
    /* outline */
    n = 0; p[n++] = AP(H, -18, 10);
    p[n++] = AP(H, -19, -3);
    n += BZ(p + n, H, -19, -3, -19.5, -17, -10, -24.5, 1, -24, 0);
    n += BZ(p + n, H, 1, -24, 12, -23.5, 18.5, -16, 18.5, -3, 0);
    p[n++] = AP(H, 19, 7);
    n += BZ(p + n, H, 19, 7, 18.5, 15, 15, 20, 9, 22.5, 0);
    p[n++] = AP(H, 6, 22.5);
    ks_poly(r, p, n, 0, W15, C_SEC);
    n = 0; p[n++] = AP(H, 6, 22.5); p[n++] = AP(H, 6, 9); p[n++] = AP(H, 3, 9); p[n++] = AP(H, 3, 23); p[n++] = AP(H, -3, 22);
    n += BZ(p + n, H, -3, 22, -10, 21, -16, 18, -18, 10, 0);
    ks_poly(r, p, n, 0, W15, C_SEC);
    n = BZ(p, H, -19, -3, -12, -5, -6, -5.5, -9, -8.5, 1);                 /* brow line into the far slit */
    ks_poly(r, p, n, 0, W1, C_DEC);
    static const int32_t ck[8] = { Q(12), Q(-21), Q(6), Q(-35), Q(-14), Q(-36), Q(-23), Q(-14) };
    crest(r, H, ck, 1, k);
    eye(r, H, Q(11.5), Q(-6.9), 1024, k);
    eye(r, H, Q(-3), Q(-6.9), 680, k);                                      /* the far eye, foreshortened */
}
static void helmet(void *r, const kn_t *k)
{
    int v = sc_view, mirror = v < 0 ? -1 : 1;
    ks_m H = helm_m(mirror);
    if (v == 0) helmet_front(r, &H, k);
    else if (v == 1 || v == -1) helmet_three_quarter(r, &H, k);
    else helmet_profile(r, &H, k);
}

void th_screen_draw(void *r)
{
    const kn_t *k = &sc_k;
    ks_m id = { 4096, 0, 0, 4096, 0, 0 };

    /* cymbal and hats: they wobble after a hit (the hat less: it is clamped) */
    {
        int32_t tc = sc_side_d[1] == D_CYM ? sc_side_t[1] : 9000, th = sc_side_d[0] == D_HAT ? sc_side_t[0] : 9000;
        int32_t wc = -137 + ((((228 * ks_expneg(tc * 1024 / 450)) >> 15) * ks_cos(tc * 9)) >> 15);
        int32_t wh = (((68 * ks_expneg(th * 1024 / 180)) >> 15) * ks_cos(th * 12)) >> 15;
        cymbal(r, D_CYM, wc, 0);
        cymbal(r, D_HAT, wh, -((Q(1.2) * ks_expneg(th * 1024 / 120)) >> 15));
    }

    /* DCY: ripple rings from each recent hit */
    {
        int32_t life = 150 + ((1350 * k->dcy) >> 10);
        int rings = 1 + ((k->dcy * 3 + 512) >> 10);
        for (int h = 0; h < 4; h++) {
            const hit_t *ht = &sc_hit[h];
            if (ht->t >= life + 200 || ht->drum < 0) continue;
            const drum_t *g = &DRUM[ht->drum];
            int32_t l2 = ht->drum == D_KICK ? life + (life >> 1) : life;
            for (int i = 0; i < rings; i++) {
                int32_t ti = ht->t - i * 55;
                if (ti < 0 || ti >= l2) continue;
                int32_t pr = ti * 1024 / l2;
                if (ht->drum == D_KICK) {
                    int32_t rr = Q(4) + ((Q(14) * pr) >> 10);
                    ks_ring(r, &id, Q(g->cx), Q(g->cy), rr, rr, W15, C_BLUE);
                } else {
                    int32_t f = 256 + ((ht->drum == D_HAT || ht->drum == D_CYM ? 1100 : 990) * pr >> 10);
                    int32_t cy = Q(g->cy) - (ht->drum == D_HAT || ht->drum == D_CYM ? Q(0) : Q(0));
                    ks_ring(r, &id, Q(g->cx), cy, (Q(g->rx) * f) >> 10, ks_max(Q(1.2), (Q(g->ry) * f) >> 10), W15, C_BLUE);
                }
            }
        }
    }
    /* NOIS (page 2): sizzle specks fly up off each hit */
    if (k->nois > 20) {
        int n = (k->nois * 9 + 512) >> 10;
        for (int h = 0; h < 2; h++) {
            const hit_t *ht = &sc_hit[h];
            if (ht->drum < 0 || ht->t > 320) continue;
            const drum_t *g = &DRUM[ht->drum];
            uint32_t s = 11u + (sc_n - (uint32_t)h) * 7919u;
            int32_t up = Q(2) + ht->t * Q(10) / 320, top = ht->drum == D_KICK ? Q(g->cy - g->ry) : Q(g->cy);
            for (int i = 0; i < n; i++) {
                s = s * 1664525u + 1013904223u; int32_t x = Q(g->cx) + (int32_t)((int32_t)((s >> 16) % (2u * (uint32_t)g->rx + 1u)) - g->rx) * KS_ONE;
                s = s * 1664525u + 1013904223u; int32_t y = top - up - (int32_t)((s >> 16) % 6u) * KS_ONE;
                if (y < Q(52)) y = Q(52);
                sq_line(r, x, y, x + Q(0.3), y, Q(1.9), C_BLUE);
            }
        }
    }

    /* PNCH: the kick beater on its pedal; it winds back further and swings a bigger ball as PNCH rises */
    {
        int32_t g = recoil(sc_side_t[2]);
        int32_t rest = -(228 + ((1024 * k->pnch) >> 10));                  /* angle units: lean left 10 .. 55 degrees */
        int32_t a = (rest * g) >> 10;
        int32_t px = Q(166), py = Q(139), L = Q(17);
        int32_t bx = px + ((L >> 5) * ks_sin(a) >> 10), by = py - ((L >> 5) * ks_cos(a) >> 10);
        sq_line(r, px, py, bx, by, W15, C_GREEN);
        ks_disc(r, &id, bx, by, Q(2.2) + ((Q(1.6) * k->pnch) >> 10), C_GREEN);
        sq_line(r, Q(158), Q(141.5), Q(174), Q(139.5), W2, C_GREEN);                       /* footboard */
        if (sc_side_t[2] < 90 && sc_side_d[2] == D_KICK) {                               /* the thump */
            int32_t rr = Q(5) + ((Q(3) * k->pnch) >> 10);
            ks_p p[4] = { { bx - rr, by }, { bx, by - rr }, { bx + rr, by }, { bx, by + rr } };
            ks_poly(r, p, 4, 1, W1, C_GREEN);
        }
    }

    helmet(r, k);
    /* LFO (page 2): motion arcs trail the helmet's slide */
    if (k->lfo > 30) {
        int32_t lean = sc_sway / 20;                                         /* -> about +-205, as the arcs expect */
        if (ks_abs(lean) > 12) {
            int sd = lean > 0 ? -1 : 1;                                     /* trailing side */
            int32_t span = ks_min(800, 160 + ks_abs(lean) * 6);
            for (int i = 0; i < 2; i++) {
                int32_t R = Q(33) + i * Q(4), a0 = (sd < 0 ? KS_TURN / 2 : 0) - KS_TURN / 16;
                ks_p p[7];
                for (int j = 0; j < 7; j++) {
                    int32_t a = a0 - sd * (span * (j - 3)) / 6 - sd * 300;
                    p[j].x = TH_PX + ((R >> 5) * ks_cos(a) >> 10); p[j].y = TH_PY + ((R >> 5) * ks_sin(a) >> 10);
                }
                ks_poly(r, p, 7, 0, W15, C_GREEN);
            }
        }
    }

    /* arms and sticks: each hand strikes its drums and recoils to rest */
    for (int s = 0; s < 2; s++) {
        const drum_t *g = &DRUM[sc_side_d[s]];
        int32_t f = recoil(sc_side_t[s]);
        int32_t tipx = Q(g->cx + g->tipdx), tipy = Q(g->cy) - (sc_side_d[s] == D_CYM || sc_side_d[s] == D_HAT ? Q(1) : 0);
        int32_t hx = tipx + Q(g->hx), hy = tipy + Q(g->hy);
        int32_t bump = (ks_sin(f * 4) * Q(5)) >> 15;                       /* half a sine: up, then settle */
        ks_p h = { lerp(hx, Q(REST[s][0]), f), lerp(hy, Q(REST[s][1]), f) - bump };
        ks_p t = { lerp(tipx, Q(REST[s][2]), f), lerp(tipy, Q(REST[s][3]), f) - bump * 2 };
        if (sc_side_t[s] > 4000) {                                          /* idle: breathe */
            h.y += (ks_sin((int32_t)(sc_n * 300) + sc_any_t * 2 + s * 2000) * Q(0.6)) >> 15;
        }
        /* stick: from just behind the fist through the tip */
        int32_t dx = t.x - h.x, dy = t.y - h.y, l = len16(dx, dy);
        if (l < Q(4)) l = Q(4);
        ks_p back = { h.x - (int32_t)(((int64_t)dx * Q(3)) / l), h.y - (int32_t)(((int64_t)dy * Q(3)) / l) };
        ks_ln(r, back, t, W2, C_SEC);
        ks_disc(r, &id, t.x, t.y, Q(1.2), C_SEC);
        ks_p sh = { Q(SHOULDER[s][0]), Q(SHOULDER[s][1]) };
        arm(r, sh, h, s);
    }

    host_mark(1);
    char v[6];
    if (k->page2 & 1) readout99(r, 0, IC_NOIS, "NOIS", k->raw[4]); else readout99(r, 0, IC_DCY, "DCY", k->raw[0]);
    if (k->page2 & 2) readout99(r, 1, IC_LFO, "LFO", k->raw[5]); else readout99(r, 1, IC_PNCH, "PNCH", k->raw[1]);
    if (k->page2 & 4) {                                 /* RATE reads as hits per LFO cycle, as theseus.c steps it */
        int32_t rt = ks_max(k->raw[6], 0);
        sq_num(v, (uint32_t)(32 - ((rt * 30 + 16383) >> 15)), 1);
        readout(r, 2, IC_RATE, "RATE", v);
    } else readout99(r, 2, IC_CUT, "CUT", k->raw[2]);
    readout99(r, 3, IC_FM, "FM", k->raw[3]);
    host_mark(0);
}

/* ---- the engine screen ---- */
#ifndef OP1_HOST
static void th_update(void *scr, int dt)
{
    int16_t kn[4];
    int slot = OP1_SCREEN_SLOT & 7;
    op1_synth_knobs_live(slot, kn);
    const int16_t *w = op1_synth_knobs(slot);
    int pg = (op1_eng_page[0] ? 1 : 0) | (op1_eng_page[1] ? 2 : 0) | (op1_eng_page[2] ? 4 : 0);
    th_screen_place(kn, w + 5, pg, dt);                 /* moves the helmet node before the SVG is drawn */
    op1_screen_draw(scr, dt);
    th_screen_draw(op1_raster());
}
const op1_screen_t SCREEN_theseus = { th_screen_init, th_update };
#endif
