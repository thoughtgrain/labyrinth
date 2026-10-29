/* meikyu (Japanese "maze"): a generative shift-register sequencer for the OP-1 (kennel sequencer type), engine part.
 * Platform independent: only <stdint.h>. The firmware glue (meikyu.c) and the host test (meikyu_test.c) provide the
 * four hooks at the bottom. See README.md for the controls.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#ifndef MEIKYU_CORE_H
#define MEIKYU_CORE_H
#include <stdint.h>

#define MZ_STEPS   16                   /* two 8-step halves, A and B; length > 8 chains them */
#define MZ_PATS    4
#define MZ_PPQN    24
#define MZ_RATES   6
#define MZ_SCALES  10
#define MZ_TARGETS 5                    /* OFF, engine knob blue / green / white / orange */
#define MZ_SOURCES 3                    /* LANE, PITCH, ACCENT */
#define MZ_WT_MAX  99                   /* step weight 0..99: the step plays when weight < density */

enum { MZ_SRC_LANE, MZ_SRC_PITCH, MZ_SRC_ACCENT };
enum { MZ_SCALE_KEY = 0 };              /* follows the published OP-1 key (op1_music), else minor */
enum { MZ_STYLE_SYN, MZ_STYLE_DRM };    /* synth bank voice style: scale-quantized melody, or 8 drum pads (theseus) */
#define MZ_PADS    8

typedef struct {
    uint8_t  corrupt;                   /* 0..100 % chance the step just played is rewritten */
    uint8_t  length;                    /* 2..16 */
    uint8_t  density;                   /* 0..100 */
    uint8_t  range;                     /* 0..15 scale degrees above the held note */
    uint8_t  lock;                      /* 1: corruption off, the pattern is frozen */
    uint16_t flip;                      /* bit i: step i's play decision inverted (blue press) */
    uint8_t  val[MZ_STEPS];             /* pitch value 0..15 (scaled by range) */
    uint8_t  wt[MZ_STEPS];              /* weight 0..99 */
    uint8_t  mv[MZ_STEPS];              /* mod lane value 0..15 */
} mz_pat_t;

typedef struct {
    uint8_t  init, pat, rate, gate, scale;
    uint8_t  mod_target, mod_depth, mod_base, mod_src;   /* target 0 = off; depth 0..63; base 0..127 */
    uint8_t  running, latched, stop_pend, page;           /* page 1: MOD page */
    uint8_t  root, root_key;            /* transposition: last held note and its key 0..23 */
    uint8_t  style;                     /* synth bank: MZ_STYLE_DRM while the loaded engine is theseus, else SYN (live, not saved) */
    uint32_t held;                      /* keys down that play the pattern (bit = key) */
    int16_t  step;                      /* playhead 0..15, -1 = not started */
    int32_t  abs;                       /* steps since start, -1 = none yet */
    int32_t  phase;                     /* abs value of step 0 (reset moves it) */
    uint8_t  reset_pend;
    uint32_t start;                     /* tick of the first step */
    uint64_t tick_q16;                  /* free-running clock, Q16 ticks, 24 per beat */
    int16_t  sounding;                  /* note id + 1, 0 = none (one voice) */
    int16_t  mod_sent;                  /* last value sent, -1 = none since start */
    uint32_t rng;
    volatile uint8_t save_due;          /* audio thread -> main loop: mutations want a save */
    uint8_t  mutated;
    uint8_t  since_save;
    uint16_t rewrote;                   /* bit i: step i was rewritten by CORRUPT on its last pass (screen) */
    int32_t  acc[4];
    mz_pat_t pats[MZ_PATS];
} mz_bank_t;

extern mz_bank_t mz[2];                 /* 0 synth, 1 drum */
extern const char *const mz_rate_names[MZ_RATES];
extern const char *const mz_scale_names[MZ_SCALES];
extern const char *const mz_target_names[MZ_TARGETS];
extern const char *const mz_source_names[MZ_SOURCES];
extern const uint8_t mz_pad_semis[MZ_PADS];             /* DRM: pad -> semitones above the held note */

/* hooks the platform provides */
int  mz_out_on(int pos, int note, int key, int vel);    /* -> note id (>= 0) or < 0 */
void mz_out_off(int id);
int  mz_out_param(int knob, int value);                 /* engine knob 0..3, value 0..127 */
int  mz_music(int *root, uint16_t *mask);               /* 1 if a key is published */
void mz_changed(void);                                  /* UI thread: the user changed something */
enum { MZ_VOICE_OTHER, MZ_VOICE_TORO, MZ_VOICE_THESEUS };
int  mz_voice(void);                                    /* which engine the current synth slot plays (MZ_VOICE_*) */

mz_bank_t *mz_bank(int pos);                            /* initialized bank */
/* keys (UI / KeyHandler thread) */
int  mz_key_down(int pos, int note, int key, int shift);
void mz_key_up(int pos, int key, int shift);
int  mz_shift_wants(int key);
int  mz_shift_key(int pos, int key);                    /* 0 not handled, 2 started, else 1 */
/* encoders */
void mz_turn(int pos, int knob, int delta, int shift);
void mz_press(int pos, int knob, int shift);
void mz_clip(int pos, int op);                          /* 0 copy, 1 paste, 2 clear */
void mz_recall(int pos, int p);
int  mz_toggle(int pos);
/* clock (audio thread) */
void mz_advance(int pos, uint64_t inc_q16);
void mz_all_off(int pos);
void mz_midi_start(void);
int  mz_take_save_due(int pos);                         /* main loop: 1 once per pending save */
/* queries */
int  mz_step_note(const mz_bank_t *b, int i);           /* note step i would play (synth: quantized or a pad) */
int  mz_step_pad(const mz_bank_t *b, int i);            /* DRM: pad 0..7 step i hits (val x range, halved) */
int  mz_step_plays(const mz_pat_t *q, int i);
int  mz_step_accent(const mz_pat_t *q, int i);
/* persistence text */
int  mz_serialize(int pos, char *out, int n);
int  mz_parse(int pos, const char *s, int len);         /* 1 loaded, 0 rejected (state unchanged) */
int  mz_join(char *out, int n, const int *v, int k);
int  mz_split(const char *s, int len, int *v, int max);

#endif
