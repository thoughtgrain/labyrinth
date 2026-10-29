/* meikyu (Japanese "maze"): firmware glue (sequencer type, player, persistence, screen). The engine is meikyu_core.c.
 * Built on the same contracts as the stock kennel sequencers oracle (seq/oracle.c) and c64arp (plugins/c64arp):
 *   - op1_seq_type_t registration through op1_feature_t.seq; the player vtable uses ok_player's layout
 *     (+0x14 key_down, +0x1c key_up, +0x24 reset, +0x2c tick at 96 per beat, +0x3c stop).
 *   - notes through op1_seq_note_on / _off; every note-off releases the exact id the note-on returned.
 *   - persistence through the core's sq_store (R246's background saver, own SQLite table "meikyuseq").
 *   - MOD lane through op1_param_set (op1_feature_midi.h), synth mode only.
 * The firmware addresses below are R246 3.0.x locations that the matching SDK's own sequencers read (mode, Shift,
 * shown screen, tape state, keyboard octave); they are not imports.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include "../../op1_feature_seq.h"
#include "../../op1_feature_midi.h"
#include "../../op1_feature_sound.h"
#include "../../op1_synth.h"
#include "../../seq/seq_text.h"                         /* sq_text / sq_line / sq_dot / sq_store: core exports */
#include "meikyu_core.h"

#define MZ_MODE()      (*(volatile int *)0x01DE4C40 == 1)          /* 0 synth, 1 drum */
#define MZ_SHIFT()     (*(volatile uint8_t *)0x01DD7F78)
#define MZ_SCREEN()    (*(volatile int *)0x018B65C8)
#define MZ_TAPE()      (*(volatile int *)0x018C17D4)               /* 3 / 6: REC held (armed) */
#define MZ_SYNTH_OCT() (((const int *)0x01DFE4E0)[*(volatile int *)0xFF8016D4 & 7])
#define MZ_DRUM_OCT()  (((const int *)0x01DFE500)[*(volatile int *)0xFF8016D8 & 7])

/* ---- engine hooks ---- */
int  mz_out_on(int pos, int note, int key, int vel) { return op1_seq_note_on(pos, note, key, vel); }
void mz_out_off(int id) { op1_seq_note_off(id); }
int  mz_out_param(int knob, int value) { return op1_param_set(OP1_PARAM_ENGINE, knob, value); }
/* Which engine is the current synth slot playing? Plugin engines run in the iter slot (engine type 14) with their
 * number in knob word 4; op1_sound_engine gives the engine object, and toro and theseus each carry an 8-byte tag
 * right after their op1_engine_t ("TORO", "THESEUS", see toro.c and theseus.c). Any other engine fails the compare
 * on the first bytes. Read once per step by the player and once per frame by the screen: a handful of reads. */
#define MZ_SYNTH_SLOT() (*(volatile int *)0xFF8016D4 & 7)
#define MZ_ENGINE(s)    (((const volatile int *)0xFF8016E4)[s])
static int mz_tag(const char *t, const char *k) { for (int i = 0; i < 8; i++) { if (t[i] != k[i]) return 0; if (!k[i]) return 1; } return 1; }
int mz_voice(void)
{
    int slot = MZ_SYNTH_SLOT();
    if (MZ_ENGINE(slot) != 14) return MZ_VOICE_OTHER;
    const op1_engine_t *e = op1_sound_engine(op1_synth_knobs(slot)[4] - OP1_ENGINE_MAGIC);
    if (!e) return MZ_VOICE_OTHER;
    const char *t = (const char *)(e + 1);
    if (mz_tag(t, "THESEUS")) return MZ_VOICE_THESEUS;
    if (mz_tag(t, "TORO")) return MZ_VOICE_TORO;
    return MZ_VOICE_OTHER;
}
int  mz_music(int *root, uint16_t *mask)
{
    if (!op1_music.valid) return 0;
    *root = op1_music.root; *mask = op1_music.scale_mask;
    return 1;
}

/* ---- persistence: table meikyuseq (is_synth, data), one row per mode ----
 * A row that fails validation is kept byte for byte and written back unchanged until the player edits that mode on
 * purpose, so a damaged or newer-format row is never replaced by defaults just because this build cannot read it. */
#define MZ_ROW_MAX 960
static char mz_sql[1100];
static char mz_kept[2][MZ_ROW_MAX];
static int16_t mz_kept_len[2];
static int mz_ser(int pos, char *out, int n)
{
    int m = pos & 1;
    if (mz_kept_len[m] > 0 && mz_kept_len[m] < n) {
        for (int i = 0; i < mz_kept_len[m]; i++) out[i] = mz_kept[m][i];
        out[mz_kept_len[m]] = 0;
        return mz_kept_len[m];
    }
    int o = mz_serialize(m, out, n);
    if (o < 0) { out[0] = 0; return 0; }                /* (cannot happen: 230 short numbers < the buffer) */
    return o;
}
static int mz_par(int pos, const char *s, int len)
{
    int m = pos & 1;
    if (mz_parse(m, s, len)) { mz_kept_len[m] = 0; return 1; }
    if (len > 0 && len < MZ_ROW_MAX) {
        for (int i = 0; i < len; i++) mz_kept[m][i] = s[i];
        mz_kept_len[m] = (int16_t)len;
    }
    return 0;
}
static sq_store_t mz_store = { .name = "meikyuseq", .ser = mz_ser, .parse = mz_par,
                               .sql = mz_sql, .cap = sizeof mz_sql, .maxlen = MZ_ROW_MAX };
void mz_changed(void)                                   /* UI thread: an edit on purpose */
{
    mz_kept_len[MZ_MODE()] = 0;
    sq_store_dirty(&mz_store);
}

/* ---- player ---- */
static int mz_nop(void *self) { (void)self; return 0; }
static int mz_pkey_down(void *self, int key)
{
    (void)self;
    if (key < 0 || key > 23) return 0;
    int m = MZ_MODE(), sh = MZ_SHIFT();
    if (sh) {                                           /* Shift + key shortcuts, any screen */
        int h = mz_shift_key(m, key);
        int tape = MZ_TAPE();
        if (h == 2 && (tape == 3 || tape == 6)) op1_seq_press_play();   /* start with REC held: also PLAY */
        if (h) return 1;
    }
    int note = key + 0x35 + (m ? MZ_DRUM_OCT() : MZ_SYNTH_OCT()) * 12;
    return mz_key_down(m, note, key, sh);               /* consumed: the maze plays from this key */
}
static void mz_pkey_up(void *self, int key)             /* consumed keys only; both modes (mode switch while held) */
{
    (void)self;
    mz_key_up(0, key, MZ_SHIFT()); mz_key_up(1, key, MZ_SHIFT());
}
static void mz_preset(void *self) { (void)self; mz_all_off(MZ_MODE()); }
static void mz_ptick(void *self, int counter) { (void)self; (void)counter; mz_advance(MZ_MODE(), 1u << 14); }  /* 1/96 beat */
static void mz_pstop(void *self) { (void)self; mz_all_off(0); mz_all_off(1); }
static const uint32_t mz_vt[16] = {
    0, 0,  0, (uint32_t)mz_nop,  0, (uint32_t)mz_pkey_down,  0, (uint32_t)mz_pkey_up,
    0, (uint32_t)mz_preset,  0, (uint32_t)mz_ptick,  0, (uint32_t)mz_nop,  0, (uint32_t)mz_pstop };
static struct { const uint32_t *vt; uint32_t pad[7]; } mz_player = { mz_vt, { 0 } };
static void *mz_player_get(void)
{
    if (!mz_store.ready) { mz_bank(0); mz_bank(1); sq_store_load(&mz_store); }
    return &mz_player;
}

/* ---- sequencer type callbacks (meikyu screen shown) ---- */
static int mz_key_ev(int mode, int code, int pressed, int shift)
{
    (void)shift;
    if (code < 0x0A || code > 0x0C) return 0;           /* lift / drop / scissors = copy / paste / clear */
    if (pressed) mz_clip(mode, code - 0x0A);
    return 1;
}
static void mz_press_ev(int mode, int knob, int shift) { mz_press(mode, knob, shift); }
static void mz_turn_ev(int mode, int knob, int delta, int shift) { mz_turn(mode, knob, delta, shift); }

/* ---- feature events: saves the corruption asked for, on the main loop ---- */
static void mz_event(int id, int32_t arg)
{
    (void)arg;
    if (id != OP1_EV_SERVICE || !mz_store.ready) return;
    int due = mz_take_save_due(0) | mz_take_save_due(1);
    if (due) sq_store_dirty(&mz_store);                 /* a kept (unreadable) row stays kept: no mz_changed */
}

/* ======================= screen (GUI thread) =======================
 * THREAD: meikyu.svg draws the labyrinth walls and the bull (static); meikyu_screen.c draws the thread, the steps and
 * the readouts on top, every frame. */
void mz_screen(void *r, int pos, int shift);
static void mz_scr_init(void *scr) { (void)scr; }
static void mz_scr_update(void *scr, int dt)
{
    op1_screen_draw(scr, dt);                           /* the static walls (meikyu.svg) */
    mz_screen(op1_raster(), MZ_MODE(), MZ_SHIFT() != 0);
}
const op1_screen_t SCREEN_meikyu = { mz_scr_init, mz_scr_update };

static const op1_seq_type_t mz_type = {
    .name = "meikyu", .player = mz_player_get, .svg = "meikyu.svg", .screen = &SCREEN_meikyu,
    .key = mz_key_ev, .press = mz_press_ev, .turn = mz_turn_ev, .shift_wants = mz_shift_wants,
    .midi_start = mz_midi_start };
static const char *const mz_claims[] = { 0 };
const op1_feature_t FEATURE_meikyu = { .api = OP1_FEATURE_API, .priority = 1000, .name = "meikyu", .seq = &mz_type,
                                     .event = mz_event, .claims = mz_claims };
