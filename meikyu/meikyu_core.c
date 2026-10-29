/* meikyu engine: 16 steps (A = 1..8, B = 9..16), each with a pitch value, a weight and a mod value. On every step the
 * sequencer plays the step if its weight is under the density (or the opposite when the step is flipped), then may
 * corrupt that step - rewrite its pitch, weight and mod value - so the next pass differs. Corrupt 0 or LOCK freezes
 * the pattern; corrupt 100 rewrites every step it plays.
 *
 * Threads: keys and encoders arrive on the KeyHandler / UI thread, mz_advance on the audio thread (player tick),
 * persistence text on the saver thread. Shared fields are bytes or aligned words, as in the stock kennel sequencers
 * (oracle, euclid). mz_changed is only called from the UI side; the audio side only raises save_due.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "meikyu_core.h"

mz_bank_t mz[2];

static const uint8_t mz_rate_ticks[MZ_RATES] = { 24, 12, 8, 6, 4, 3 };   /* 1/4 1/8 1/8T 1/16 1/16T 1/32 */
const char *const mz_rate_names[MZ_RATES] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
const char *const mz_scale_names[MZ_SCALES] = { "KEY", "MAJOR", "MINOR", "DORIAN", "PHRYG", "MIXO", "PENT+", "PENT-",
                                                "BLUES", "CHROM" };
/* bit n = n semitones above the root */
static const uint16_t mz_masks[MZ_SCALES] = { 0x5AD, 0xAB5, 0x5AD, 0x6AD, 0x5AB, 0x6B5, 0x295, 0x4A9, 0x4E9, 0xFFF };
const char *const mz_target_names[MZ_TARGETS] = { "OFF", "BLUE", "GREEN", "WHITE", "ORANGE" };
const char *const mz_source_names[MZ_SOURCES] = { "LANE", "PITCH", "ACCENT" };

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static uint32_t mz_rand(mz_bank_t *b)
{
    uint32_t x = b->rng ? b->rng : 0x6D617A65u;         /* xorshift32; never zero */
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    b->rng = x;
    return x;
}
static int mz_pct(mz_bank_t *b) { return (int)((mz_rand(b) >> 8) % 100u); }   /* 0..99 */

static void mz_roll_step(mz_bank_t *b, mz_pat_t *q, int i)
{
    q->val[i] = (uint8_t)(mz_rand(b) >> 12 & 15);
    q->wt[i]  = (uint8_t)mz_pct(b);
    q->mv[i]  = (uint8_t)(mz_rand(b) >> 20 & 15);
    q->flip  &= (uint16_t)~(1u << i);
}
static void mz_pat_cast(mz_bank_t *b, mz_pat_t *q)
{
    for (int i = 0; i < MZ_STEPS; i++) mz_roll_step(b, q, i);
}
static void mz_pat_default(mz_bank_t *b, mz_pat_t *q)
{
    q->corrupt = 20; q->length = 8; q->density = 60; q->range = 7; q->lock = 0; q->flip = 0;
    mz_pat_cast(b, q);
}
/* byte copy: no struct assignment, so the freestanding build never emits a memcpy call */
static void mz_pat_copy(mz_pat_t *d, const mz_pat_t *s)
{
    volatile uint8_t *dp = (volatile uint8_t *)d;
    const uint8_t *sp = (const uint8_t *)s;
    for (unsigned i = 0; i < sizeof *d; i++) dp[i] = sp[i];
}

mz_bank_t *mz_bank(int pos)
{
    mz_bank_t *b = &mz[pos & 1];
    if (b->init) return b;
    b->init = 1; b->pat = 0; b->rate = 3; b->gate = 50; b->scale = 2; b->page = 0; b->style = MZ_STYLE_SYN;
    b->mod_target = 0; b->mod_depth = 24; b->mod_base = 64; b->mod_src = MZ_SRC_LANE;
    b->root = 60; b->root_key = 7;                      /* C4: key C at octave 0 */
    b->step = -1; b->abs = -1; b->phase = 0; b->mod_sent = -1;
    b->rng = (pos & 1) ? 0x0B1E55EDu : 0x1AB71217u;     /* fixed: the first boot always starts from the same pattern */
    for (int p = 0; p < MZ_PATS; p++) mz_pat_default(b, &b->pats[p]);
    return b;
}
static mz_pat_t *mz_cur(mz_bank_t *b) { return &b->pats[b->pat]; }

/* ---- notes ---- */
static void mz_silence(mz_bank_t *b)
{
    if (b->sounding) { mz_out_off(b->sounding - 1); b->sounding = 0; }
}
static void mz_mod_rest(mz_bank_t *b, int pos)          /* back to BASE when the sequence stops */
{
    if (!(pos & 1) && b->mod_target && b->mod_sent >= 0 && b->mod_sent != b->mod_base)
        mz_out_param(b->mod_target - 1, b->mod_base);
    b->mod_sent = -1;
}

int mz_step_plays(const mz_pat_t *q, int i)
{
    int on = q->wt[i] < q->density;
    return ((q->flip >> i) & 1) ? !on : on;
}
int mz_step_accent(const mz_pat_t *q, int i) { return q->wt[i] < q->density / 4; }

/* DRM pads, spread so a DFAM-style voice (theseus) lands on a different drum for each: the held note is the kick,
 * a 4th and 5th up the low and high toms, the octave the snare, then rim, hat, open hat and metal */
const uint8_t mz_pad_semis[MZ_PADS] = { 0, 5, 7, 12, 17, 24, 31, 36 };
int mz_step_pad(const mz_bank_t *b, int i)
{
    const mz_pat_t *q = &b->pats[b->pat];
    int deg = q->val[i] * q->range / 15;                /* 0..15: RNG 0 keeps every hit on the kick */
    return deg >> 1;
}
/* the i-th scale note at or above b->root: snap up into the scale, then walk `deg` scale notes */
int mz_step_note(const mz_bank_t *b, int i)
{
    const mz_pat_t *q = &b->pats[b->pat];
    int deg = q->val[i] * q->range / 15;
    if (b == &mz[1]) return b->root + deg;              /* drums: chromatic, one drum key per degree */
    if (b->style == MZ_STYLE_DRM) return b->root + mz_pad_semis[mz_step_pad(b, i)];   /* pads: the scale is bypassed */
    int r = b->root % 12;
    uint16_t m = mz_masks[b->scale % MZ_SCALES];
    if (b->scale == MZ_SCALE_KEY) {
        int kr; uint16_t km;
        if (mz_music(&kr, &km) && (km & 0xFFF)) { r = kr % 12; m = km; }
    }
    int n = b->root;
    for (int k = 0; k < 12 && !((m >> ((n - r + 120) % 12)) & 1); k++) n++;
    while (deg > 0) { n++; if ((m >> ((n - r + 120) % 12)) & 1) deg--; }
    return n;
}

static int mz_mod_value(const mz_bank_t *b, const mz_pat_t *q, int i)
{
    int lane = 8;                                       /* 0..15, centre 7.5 */
    if (b->mod_src == MZ_SRC_LANE) lane = q->mv[i];
    else if (b->mod_src == MZ_SRC_PITCH) lane = q->val[i];
    else lane = mz_step_plays(q, i) ? (mz_step_accent(q, i) ? 15 : 8) : 0;
    int v = b->mod_base + ((lane * 2 - 15) * b->mod_depth) / 15;
    return clampi(v, 0, 127);
}

/* ---- transport ---- */
static void mz_start(mz_bank_t *b, int now)
{
    b->stop_pend = 0;
    if (b->running) return;
    uint32_t t = (uint32_t)(b->tick_q16 >> 16), r = mz_rate_ticks[b->rate];
    b->start = now ? t : (t / r + 1) * r;
    b->step = -1; b->abs = -1; b->phase = 0; b->reset_pend = 0; b->mod_sent = -1; b->running = 1;
}
static void mz_stop(int pos, mz_bank_t *b)
{
    b->latched = 0; b->stop_pend = 0;
    if (!b->running) return;
    b->running = 0; b->step = -1; b->rewrote = 0;
    mz_silence(b);
    mz_mod_rest(b, pos);
    if (b->mutated) b->save_due = 1;
}

int mz_key_down(int pos, int note, int key, int shift)
{
    mz_bank_t *b = mz_bank(pos);
    if (key < 0 || key > 23) return 0;
    b->held |= 1u << key;
    b->root = (uint8_t)clampi(note, 0, 127); b->root_key = (uint8_t)key;   /* in phase: from the next step */
    b->latched = (uint8_t)(shift ? 1 : 0);              /* a key without Shift takes a latched pattern over */
    mz_start(b, 1);
    return 1;
}
void mz_key_up(int pos, int key, int shift)
{
    mz_bank_t *b = mz_bank(pos);
    if (key < 0 || key > 23 || !(b->held & (1u << key))) return;
    b->held &= ~(1u << key);
    if (b->held) return;
    if (shift) b->latched = 1;
    if (b->running && !b->latched) b->stop_pend = 1;    /* stops on the next step boundary */
}

int mz_toggle(int pos)
{
    mz_bank_t *b = mz_bank(pos);
    if (b->running) { mz_stop(pos, b); return 0; }
    mz_start(b, 1); b->latched = 1;
    return 1;
}
void mz_recall(int pos, int p)
{
    mz_bank_t *b = mz_bank(pos);
    if (p < 0 || p >= MZ_PATS || p == b->pat) return;
    mz_silence(b);
    b->pat = (uint8_t)p;
    mz_changed();
}

/* Shift + key, any screen: F G A B (0 2 4 6) pattern 1..4, C# (20) start / stop */
static const int8_t mz_shift_map[24] = { 10, -1, 11, -1, 12, -1, 13, -1, -1, -1, -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1, -1, 8, -1, -1, -1 };
int mz_shift_wants(int key) { return key >= 0 && key <= 23 && mz_shift_map[key] >= 0; }
int mz_shift_key(int pos, int key)
{
    if (!mz_shift_wants(key)) return 0;
    int a = mz_shift_map[key];
    if (a >= 10) { mz_recall(pos, a - 10); return 1; }
    return mz_toggle(pos) ? 2 : 1;
}

/* ---- encoders ---- */
static int mz_detents(mz_bank_t *b, int k, int delta)
{
    if ((b->acc[k] > 0 && delta < 0) || (b->acc[k] < 0 && delta > 0)) b->acc[k] = 0;   /* direction change */
    b->acc[k] += delta;
    int st = b->acc[k] / 1024;
    b->acc[k] -= st * 1024;
    return st;
}

void mz_turn(int pos, int knob, int delta, int shift)
{
    mz_bank_t *b = mz_bank(pos);
    if (knob < 0 || knob > 3) return;
    int st = mz_detents(b, knob, delta);
    if (!st) return;
    mz_pat_t *q = mz_cur(b);
    if (shift) {                                        /* rate, gate, scale, run */
        if (knob == 0) {
            int r = clampi(b->rate + st, 0, MZ_RATES - 1);
            if (r == b->rate) return;
            b->rate = (uint8_t)r;
            if (b->running) {                           /* keep the step grid aligned */
                uint32_t t = (uint32_t)(b->tick_q16 >> 16), rt = mz_rate_ticks[r];
                b->start = (t / rt + 1) * rt; b->step = -1; b->abs = -1; b->phase = 0;
            }
        } else if (knob == 1) b->gate = (uint8_t)clampi(b->gate + st * 5, 10, 100);
        else if (knob == 2) {                           /* SCL; with a drum voice the scale is bypassed, so the knob rests */
            if (b->style == MZ_STYLE_DRM) return;
            b->scale = (uint8_t)clampi(b->scale + st, 0, MZ_SCALES - 1);
        }
        else {                                          /* HOLD: right on, left off */
            if (st > 0) { b->latched = 1; b->stop_pend = 0; }   /* keeps a running pattern going without keys */
            else if (b->latched) {
                b->latched = 0;
                if (b->running && !b->held) b->stop_pend = 1;   /* no key down: stop on the next step boundary */
            }
            return;                                     /* playback state: not saved */
        }
    } else if (b->page) {                               /* MOD page */
        if (knob == 0) {
            int t = clampi(b->mod_target + st, 0, MZ_TARGETS - 1);
            if (t != b->mod_target) { mz_mod_rest(b, pos); b->mod_target = (uint8_t)t; }
        }
        else if (knob == 1) b->mod_depth = (uint8_t)clampi(b->mod_depth + st * 2, 0, 63);
        else if (knob == 2) b->mod_base = (uint8_t)clampi(b->mod_base + st * 2, 0, 127);
        else b->mod_src = (uint8_t)clampi(b->mod_src + st, 0, MZ_SOURCES - 1);
    } else {                                            /* CORRUPT, LENGTH, DENSITY, RANGE */
        if (knob == 0) q->corrupt = (uint8_t)clampi(q->corrupt + st * 5, 0, 100);
        else if (knob == 1) q->length = (uint8_t)clampi(q->length + st, 2, MZ_STEPS);
        else if (knob == 2) q->density = (uint8_t)clampi(q->density + st * 5, 0, 100);
        else q->range = (uint8_t)clampi(q->range + st, 0, 15);
    }
    mz_changed();
}

/* press: blue FLIP the current step, green RE-CAST, white LOCK, orange MOD page. Shift + orange: RESET to step 1 */
void mz_press(int pos, int knob, int shift)
{
    mz_bank_t *b = mz_bank(pos);
    mz_pat_t *q = mz_cur(b);
    if (knob == 3) {
        if (shift) { if (b->running) b->reset_pend = 1; }
        else b->page ^= 1;
        return;
    }
    if (shift) return;
    if (knob == 0) {
        int i = b->step >= 0 ? b->step : 0;
        q->flip ^= (uint16_t)(1u << i);
    } else if (knob == 1) {
        b->rng ^= (uint32_t)(b->tick_q16 >> 4) | 1u;    /* the moment of the press seeds the cast */
        mz_pat_cast(b, q);
    } else if (knob == 2) q->lock ^= 1;
    else return;
    mz_changed();
}

/* lift / drop / scissors: copy / paste / clear the current pattern (clear = default settings, new cast) */
static mz_pat_t mz_clipboard[2];
static uint8_t  mz_clip_full[2];
void mz_clip(int pos, int op)
{
    mz_bank_t *b = mz_bank(pos);
    int m = pos & 1;
    mz_pat_t *q = mz_cur(b);
    if (op == 0) { mz_pat_copy(&mz_clipboard[m], q); mz_clip_full[m] = 1; return; }
    if (op == 1 && !mz_clip_full[m]) return;
    mz_silence(b);
    if (op == 1) mz_pat_copy(q, &mz_clipboard[m]);
    else mz_pat_default(b, q);
    mz_changed();
}

/* ---- clock: advance by inc ticks (Q16, 24 per beat). Audio thread. ---- */
void mz_advance(int pos, uint64_t inc)
{
    mz_bank_t *b = mz_bank(pos);
    uint64_t t1 = b->tick_q16;
    b->tick_q16 = t1 + inc;
    if (!b->running) return;
    uint64_t r = (uint64_t)mz_rate_ticks[b->rate] << 16, st0 = (uint64_t)b->start << 16;
    if (t1 < st0) return;
    int32_t a = (int32_t)((t1 - st0) / r);
    uint64_t in = (t1 - st0) % r;
    if (b->sounding && in >= r * b->gate / 100) mz_silence(b);
    if (a == b->abs) return;
    if (b->stop_pend && b->abs >= 0) { mz_stop(pos, b); return; }   /* released: stop on this boundary */
    b->abs = a;
    if (b->reset_pend) { b->phase = a; b->reset_pend = 0; }
    mz_pat_t *q = mz_cur(b);
    int len = clampi(q->length, 2, MZ_STEPS);
    int i = (int)((uint32_t)(a - b->phase) % (uint32_t)len);
    b->step = (int16_t)i;
    b->rewrote &= (uint16_t)~(1u << i);                 /* a new pass over step i: its old mark goes */
    mz_silence(b);
    b->style = (uint8_t)(!(pos & 1) && mz_voice() == MZ_VOICE_THESEUS ? MZ_STYLE_DRM : MZ_STYLE_SYN);   /* follows the loaded engine */
    if (mz_step_plays(q, i)) {
        int n = mz_step_note(b, i);
        int key = (pos & 1) ? clampi(b->root_key + (n - b->root), 0, 23) : b->root_key;
        if (pos & 1) n = b->root + (key - b->root_key);   /* drums: stay on the 24 keys */
        if (n >= 0 && n <= 127) {
            int id = mz_out_on(pos & 1, n, key, mz_step_accent(q, i) ? 118 : 92);
            if (id >= 0) b->sounding = (int16_t)(id + 1);
        }
    }
    if (!(pos & 1) && b->mod_target) {
        int v = mz_mod_value(b, q, i);
        if (v != b->mod_sent && mz_out_param(b->mod_target - 1, v) == 0) b->mod_sent = (int16_t)v;
    }
    if (!q->lock && q->corrupt && mz_pct(b) < q->corrupt) {   /* corrupt the step just played */
        mz_roll_step(b, q, i);
        b->rewrote |= (uint16_t)(1u << i);
        b->mutated = 1;
    }
    if (b->mutated && ++b->since_save >= 64) { b->since_save = 0; b->save_due = 1; }
}

void mz_all_off(int pos)
{
    mz_bank_t *b = &mz[pos & 1];
    if (!b->init) return;
    mz_silence(b);
    mz_mod_rest(b, pos);
}
void mz_midi_start(void)                                /* MIDI START: restart running parts at step 1 */
{
    for (int pos = 0; pos < 2; pos++) {
        mz_bank_t *b = &mz[pos];
        b->tick_q16 = 0;
        if (!b->running) continue;
        mz_silence(b);
        b->start = 0; b->step = -1; b->abs = -1; b->phase = 0; b->reset_pend = 0;
    }
}
int mz_take_save_due(int pos)
{
    mz_bank_t *b = &mz[pos & 1];
    if (!b->save_due) return 0;
    b->save_due = 0; b->mutated = 0;
    return 1;
}

/* ---- persistence text ----
 * "1,pat,rate,gate,scale,root,mtarget,mdepth,mbase,msrc, (corrupt,length,density,range,lock,flip_lo,flip_hi,
 *  val x16, wt x16, mv x16) x 4" - 10 + 4 * 55 = 230 numbers. Field 0 is the format version. The voice style follows
 * the loaded engine, so it is not saved; version 2 rows (231 numbers, from the 0.3.0 test build that saved a manual
 * style) still load, with that field checked and ignored. */
#define MZ_HEAD1  10
#define MZ_HEAD   11
#define MZ_PF     (7 + 3 * MZ_STEPS)
#define MZ_TEXT_N (MZ_HEAD + MZ_PATS * MZ_PF)

int mz_join(char *out, int n, const int *v, int k)      /* numbers 0..9999 joined by ',' */
{
    int o = 0;
    for (int i = 0; i < k; i++) {
        char d[4]; int m = 0, x = v[i] < 0 ? 0 : v[i];
        do { d[m++] = (char)('0' + x % 10); x /= 10; } while (x && m < 4);
        if (o + m + 2 >= n) return -1;                  /* does not fit: refuse rather than truncate */
        if (i) out[o++] = ',';
        while (m) out[o++] = d[--m];
    }
    if (n > 0) out[o] = 0;
    return o;
}
int mz_split(const char *s, int len, int *v, int max)   /* count, or -1 on any malformed field */
{
    int k = 0, x = -1;
    for (int i = 0; i <= len; i++) {
        char ch = i < len ? s[i] : ',';
        if (ch >= '0' && ch <= '9') { x = (x < 0 ? 0 : x * 10) + (ch - '0'); if (x > 9999) return -1; }
        else if (ch == ',') { if (x < 0 || k >= max) return -1; v[k++] = x; x = -1; }
        else return -1;
    }
    return k;
}

int mz_serialize(int pos, char *out, int n)
{
    mz_bank_t *b = mz_bank(pos);
    static int v[MZ_TEXT_N];                            /* saver thread only: off the 4 KiB native stack */
    int c = 0;
    v[c++] = 1; v[c++] = b->pat; v[c++] = b->rate; v[c++] = b->gate; v[c++] = b->scale; v[c++] = b->root;
    v[c++] = b->mod_target; v[c++] = b->mod_depth; v[c++] = b->mod_base; v[c++] = b->mod_src;
    for (int p = 0; p < MZ_PATS; p++) {
        const mz_pat_t *q = &b->pats[p];
        v[c++] = q->corrupt; v[c++] = q->length; v[c++] = q->density; v[c++] = q->range; v[c++] = q->lock;
        v[c++] = q->flip & 0xFF; v[c++] = q->flip >> 8;
        for (int i = 0; i < MZ_STEPS; i++) v[c++] = q->val[i];
        for (int i = 0; i < MZ_STEPS; i++) v[c++] = q->wt[i];
        for (int i = 0; i < MZ_STEPS; i++) v[c++] = q->mv[i];
    }
    return mz_join(out, n, v, c);
}

static int in_range(const int *v, int k, int lo, int hi)
{
    for (int i = 0; i < k; i++) if (v[i] < lo || v[i] > hi) return 0;
    return 1;
}
/* validates the whole row before touching any state: a bad row loads nothing */
int mz_parse(int pos, const char *s, int len)
{
    static int v[MZ_TEXT_N];
    int k = mz_split(s, len, v, MZ_TEXT_N), head;
    if (k == MZ_TEXT_N && v[0] == 2) head = MZ_HEAD;
    else if (k == MZ_HEAD1 + MZ_PATS * MZ_PF && v[0] == 1) head = MZ_HEAD1;
    else return 0;
    if (head == MZ_HEAD && v[10] > MZ_STYLE_DRM) return 0;
    if (v[1] >= MZ_PATS || v[2] >= MZ_RATES || v[3] < 10 || v[3] > 100 || v[4] >= MZ_SCALES || v[5] > 127 ||
        v[6] >= MZ_TARGETS || v[7] > 63 || v[8] > 127 || v[9] >= MZ_SOURCES) return 0;
    for (int p = 0; p < MZ_PATS; p++) {
        const int *f = &v[head + p * MZ_PF];
        if (f[0] > 100 || f[1] < 2 || f[1] > MZ_STEPS || f[2] > 100 || f[3] > 15 || f[4] > 1 || f[5] > 255 || f[6] > 255)
            return 0;
        if (!in_range(f + 7, MZ_STEPS, 0, 15) || !in_range(f + 7 + MZ_STEPS, MZ_STEPS, 0, MZ_WT_MAX) ||
            !in_range(f + 7 + 2 * MZ_STEPS, MZ_STEPS, 0, 15)) return 0;
    }
    mz_bank_t *b = mz_bank(pos);
    mz_silence(b);
    b->pat = (uint8_t)v[1]; b->rate = (uint8_t)v[2]; b->gate = (uint8_t)v[3]; b->scale = (uint8_t)v[4];
    b->root = (uint8_t)v[5]; b->root_key = (uint8_t)((v[5] + 7) % 12);
    b->mod_target = (uint8_t)v[6]; b->mod_depth = (uint8_t)v[7]; b->mod_base = (uint8_t)v[8]; b->mod_src = (uint8_t)v[9];
    for (int p = 0; p < MZ_PATS; p++) {
        const int *f = &v[head + p * MZ_PF];
        mz_pat_t *q = &b->pats[p];
        q->corrupt = (uint8_t)f[0]; q->length = (uint8_t)f[1]; q->density = (uint8_t)f[2]; q->range = (uint8_t)f[3];
        q->lock = (uint8_t)f[4]; q->flip = (uint16_t)(f[5] | f[6] << 8);
        for (int i = 0; i < MZ_STEPS; i++) {
            q->val[i] = (uint8_t)f[7 + i]; q->wt[i] = (uint8_t)f[7 + MZ_STEPS + i]; q->mv[i] = (uint8_t)f[7 + 2 * MZ_STEPS + i];
        }
    }
    b->running = 0; b->latched = 0; b->stop_pend = 0; b->held = 0; b->step = -1; b->abs = -1; b->mod_sent = -1;
    b->rng ^= (uint32_t)len * 2654435761u;              /* a loaded pattern corrupts differently from the default */
    return 1;
}
