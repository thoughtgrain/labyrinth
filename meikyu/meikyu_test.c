/* Host test for the meikyu engine (not part of the plugin file):
 *   cc -std=c99 -O2 -Wall -Wextra -o meikyu_test meikyu_test.c meikyu_core.c && ./meikyu_test
 * Checks the note ledger, scale quantization, corruption / LOCK, density, flips, latch and release, reset, the
 * MOD lane, MIDI start, the persistence text (round trip, rejected rows, v1 rows) and the DRM voice style. */
#include <stdio.h>
#include <string.h>
#include "meikyu_core.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- platform hooks: a voice ledger ---- */
#define IDS 4096
static int on_note[IDS], is_on[IDS], next_id, n_on, n_off, sounding_now, max_sounding, last_note, last_key;
static int param_knob = -1, param_val = -1, n_params;
static int music_valid, music_root; static uint16_t music_mask;
static int changed;
int mz_out_on(int pos, int note, int key, int vel)
{
    (void)pos; (void)vel;
    int id = next_id++ % IDS;
    CHECK(!is_on[id], "id %d reused while sounding", id);
    is_on[id] = 1; on_note[id] = note; n_on++; sounding_now++; last_note = note; last_key = key;
    if (sounding_now > max_sounding) max_sounding = sounding_now;
    return id;
}
void mz_out_off(int id)
{
    CHECK(id >= 0 && id < IDS && is_on[id], "note-off for id %d that is not sounding", id);
    if (id >= 0 && id < IDS && is_on[id]) { is_on[id] = 0; n_off++; sounding_now--; }
}
int mz_out_param(int knob, int value)
{
    CHECK(knob >= 0 && knob < 4 && value >= 0 && value <= 127, "param %d = %d out of range", knob, value);
    param_knob = knob; param_val = value; n_params++;
    return 0;
}
int mz_music(int *root, uint16_t *mask) { if (!music_valid) return 0; *root = music_root; *mask = music_mask; return 1; }
void mz_changed(void) { changed++; }
static int drum_voice;
int mz_voice(void) { return drum_voice ? MZ_VOICE_THESEUS : MZ_VOICE_OTHER; }

static void reset_all(void)
{
    memset(mz, 0, sizeof mz);
    memset(is_on, 0, sizeof is_on);
    next_id = n_on = n_off = sounding_now = max_sounding = 0;
    param_knob = param_val = -1; n_params = 0; music_valid = 0; changed = 0;
}
#define STEP_Q16 ((uint64_t)6 << 16)                    /* 1/16 = 6 ticks */
static void run_steps(int pos, int steps) { for (int i = 0; i < steps * 6 * 4; i++) mz_advance(pos, 1u << 14); }

static int in_scale(int note, int root, uint16_t mask) { return (mask >> ((note - root + 120) % 12)) & 1; }

int main(void)
{
    /* 1. ledger: hold a key, play long, release: every note-on gets one note-off, one voice at a time */
    reset_all();
    mz_bank(0)->pats[0].corrupt = 100;
    mz_key_down(0, 60, 7, 0);
    run_steps(0, 400);
    CHECK(n_on > 100, "expected notes, got %d", n_on);
    CHECK(max_sounding == 1, "max sounding %d", max_sounding);
    mz_key_up(0, 7, 0);
    run_steps(0, 2);
    CHECK(!mz[0].running, "still running after release");
    CHECK(sounding_now == 0 && n_on == n_off, "hanging notes: on %d off %d", n_on, n_off);
    CHECK(mz[0].save_due == 1, "mutations did not ask for a save");
    CHECK(mz_take_save_due(0) == 1 && mz_take_save_due(0) == 0, "save_due not taken once");

    /* 2. transposing while held: the old note is released by its own id (checked in mz_out_off) */
    reset_all();
    mz_key_down(0, 60, 7, 0);
    for (int k = 0; k < 50; k++) { run_steps(0, 1); mz_key_down(0, 60 + k % 12, 7 + k % 12, 0); }
    mz_all_off(0);
    CHECK(sounding_now == 0, "all_off left %d sounding", sounding_now);

    /* 3. quantization: every note in every fixed scale, from several roots */
    static const uint16_t masks[MZ_SCALES] = { 0x5AD, 0xAB5, 0x5AD, 0x6AD, 0x5AB, 0x6B5, 0x295, 0x4A9, 0x4E9, 0xFFF };
    for (int sc = 1; sc < MZ_SCALES; sc++)
        for (int root = 48; root < 72; root += 5) {
            reset_all();
            mz_bank_t *b = mz_bank(0);
            b->scale = (uint8_t)sc; b->root = (uint8_t)root;
            for (int range = 0; range <= 15; range++) {
                b->pats[0].range = (uint8_t)range;
                for (int i = 0; i < MZ_STEPS; i++) {
                    int n = mz_step_note(b, i);
                    CHECK(in_scale(n, root, masks[sc]), "scale %d root %d: %d not in scale", sc, root, n);
                    CHECK(n >= root && n <= root + 36, "scale %d: note %d outside range", sc, n);
                }
            }
            b->pats[0].range = 0;
            CHECK(mz_step_note(b, 3) == root, "range 0 must play the held note");
        }
    /* KEY follows the published key, root E (4), major */
    reset_all();
    mz_bank(0)->scale = MZ_SCALE_KEY; mz[0].root = 61;   /* C#: not in E major's... it is: E F# G# A B C# D# */
    music_valid = 1; music_root = 4; music_mask = 0xAB5;
    for (int i = 0; i < MZ_STEPS; i++) CHECK(in_scale(mz_step_note(&mz[0], i), 4, 0xAB5), "KEY scale");
    mz[0].root = 60;                                    /* C is outside E major: snaps up to C# */
    mz[0].pats[0].val[0] = 0;
    CHECK(mz_step_note(&mz[0], 0) == 61, "snap up from C in E major: %d", mz_step_note(&mz[0], 0));

    /* 4. corruption: 0 and LOCK keep the pattern, 100 changes it */
    for (int mode = 0; mode < 3; mode++) {
        reset_all();
        mz_pat_t *q = &mz_bank(0)->pats[0];
        q->corrupt = mode == 0 ? 0 : 100; q->lock = mode == 2;
        mz_pat_t before; memcpy(&before, q, sizeof before);
        mz_toggle(0);
        run_steps(0, 64);
        int same = memcmp(&before, q, sizeof before) == 0;
        CHECK(mode == 1 ? !same : same, "corruption mode %d: same=%d", mode, same);
        mz_toggle(0);
    }

    /* 5. density 0: silence; 100: every step; a flip inverts one step */
    reset_all();
    mz_bank(0)->pats[0].density = 0; mz[0].pats[0].corrupt = 0;
    mz_toggle(0); run_steps(0, 32);
    CHECK(n_on == 0, "density 0 played %d notes", n_on);
    mz[0].pats[0].density = 100; n_on = 0;
    run_steps(0, 32);
    CHECK(n_on >= 31 && n_on <= 33, "density 100 played %d of 32", n_on);
    mz[0].pats[0].flip = 1;                             /* step 1 rests now */
    CHECK(!mz_step_plays(&mz[0].pats[0], 0) && mz_step_plays(&mz[0].pats[0], 1), "flip");
    mz_toggle(0);
    CHECK(sounding_now == 0, "stop left notes");

    /* 6. latch with Shift: keeps running after release; Shift + C# stops */
    reset_all();
    mz_key_down(0, 60, 7, 1);
    mz_key_up(0, 7, 0);
    run_steps(0, 8);
    CHECK(mz[0].running && mz[0].latched, "latch");
    CHECK(mz_shift_key(0, 20) == 1 && !mz[0].running && sounding_now == 0, "Shift + C# stop");
    CHECK(mz_shift_key(0, 20) == 2 && mz[0].running, "Shift + C# start");
    mz_all_off(0);

    /* 6b. HOLD (Shift + orange): right keeps the pattern going after release, left lets it stop on the next step */
    reset_all();
    mz_key_down(0, 62, 9, 0);
    run_steps(0, 2);
    mz_turn(0, 3, 1024, 1);                             /* HOLD on while the key is down */
    mz_key_up(0, 9, 0);
    run_steps(0, 16);
    CHECK(mz[0].running && mz[0].latched, "HOLD on: stopped after release");
    mz_turn(0, 3, -1024, 1);                            /* HOLD off, no key down */
    run_steps(0, 2);
    CHECK(!mz[0].running && sounding_now == 0, "HOLD off: still running or hanging notes");
    mz_key_down(0, 62, 9, 0);                           /* HOLD off while a key is down: keeps playing */
    mz_turn(0, 3, 1024, 1); mz_turn(0, 3, -1024, 1);
    run_steps(0, 4);
    CHECK(mz[0].running, "HOLD off with a key down must not stop");
    mz_key_up(0, 9, 0); run_steps(0, 2);
    CHECK(!mz[0].running && sounding_now == 0, "release after HOLD off");

    /* 7. length and reset: steps stay inside the length; Shift + orange press restarts at step 1 */
    reset_all();
    mz_bank(0)->pats[0].length = 5; mz[0].pats[0].corrupt = 0;
    mz_toggle(0);
    for (int k = 0; k < 40; k++) { run_steps(0, 1); CHECK(mz[0].step >= 0 && mz[0].step < 5, "step %d", mz[0].step); }
    run_steps(0, 2);
    mz_press(0, 3, 1);
    run_steps(0, 1);
    CHECK(mz[0].step == 0, "reset: step %d", mz[0].step);
    mz_toggle(0);

    /* 8. MOD lane: values in range, back to BASE on stop; drums never send */
    reset_all();
    mz_bank(0)->mod_target = 2; mz[0].mod_depth = 63; mz[0].mod_base = 100; mz[0].mod_src = MZ_SRC_LANE;
    mz_toggle(0); run_steps(0, 64);
    CHECK(n_params > 4 && param_knob == 1, "mod sends: %d to knob %d", n_params, param_knob);
    mz_toggle(0);
    CHECK(param_val == 100, "stop did not restore BASE: %d", param_val);
    n_params = 0;
    mz_bank(1)->mod_target = 2;
    mz_toggle(1); run_steps(1, 32); mz_toggle(1);
    CHECK(n_params == 0, "drum mode sent %d params", n_params);
    CHECK(last_key >= 0 && last_key <= 23, "drum key %d", last_key);

    /* 9. MIDI start: running parts restart from step 1 */
    reset_all();
    mz_bank(0)->pats[0].corrupt = 0;
    mz_toggle(0); run_steps(0, 5);
    mz_midi_start(); run_steps(0, 1);
    CHECK(mz[0].step == 0 || mz[0].step == 1, "midi start step %d", mz[0].step);
    mz_all_off(0);

    /* 10. persistence: round trip, then rejected rows leave the state alone */
    reset_all();
    mz_bank(0)->pat = 2; mz[0].scale = 6; mz[0].mod_target = 4; mz[0].pats[3].flip = 0xA5C3; mz[0].pats[1].length = 13;
    static char text[2048], text2[2048];
    int len = mz_serialize(0, text, sizeof text);
    CHECK(len > 0 && len < 960, "row length %d", len);
    mz_bank_t saved; memcpy(&saved, &mz[0], sizeof saved);
    memset(&mz[0], 0, sizeof mz[0]);
    CHECK(mz_parse(0, text, len) == 1, "parse failed");
    int len2 = mz_serialize(0, text2, sizeof text2);
    CHECK(len == len2 && memcmp(text, text2, (size_t)len) == 0, "round trip differs");
    CHECK(memcmp(saved.pats, mz[0].pats, sizeof saved.pats) == 0 && mz[0].pat == 2 && mz[0].scale == 6, "state differs");
    CHECK(!mz[0].running && mz[0].step == -1, "loaded running");
    const char *bad[] = { "", "1,2", "2,0,3,50", "1,x" };
    for (unsigned k = 0; k < sizeof bad / sizeof bad[0]; k++)
        CHECK(mz_parse(0, bad[k], (int)strlen(bad[k])) == 0, "bad row %u accepted", k);
    text[len - 1] = '9'; text[len - 2] = '9';           /* last mod value 99: out of range */
    CHECK(mz_parse(0, text, len) == 0, "out-of-range row accepted");
    CHECK(memcmp(saved.pats, mz[0].pats, sizeof saved.pats) == 0, "rejected row changed state");
    CHECK(mz_serialize(0, text2, 40) == -1, "short buffer must refuse, not truncate");

    /* 11. encoders: detents and clamps */
    reset_all();
    for (int k = 0; k < 40; k++) mz_turn(0, 0, 1024, 0);
    CHECK(mz[0].pats[0].corrupt == 100, "corrupt clamp %d", mz[0].pats[0].corrupt);
    for (int k = 0; k < 40; k++) mz_turn(0, 1, -1024, 0);
    CHECK(mz[0].pats[0].length == 2, "length clamp %d", mz[0].pats[0].length);
    mz_turn(0, 2, 512, 0); mz_turn(0, 2, 511, 0);
    CHECK(mz[0].pats[0].density == 60, "half detents moved density");
    mz_press(0, 3, 0);
    CHECK(mz[0].page == 1, "orange press: MOD page");
    mz_turn(0, 0, 1024, 0);
    CHECK(mz[0].mod_target == 1, "MOD target");
    CHECK(changed > 0, "edits did not report changes");

    /* 13. rewrite marks for the screen: a rewritten step is marked until the playhead comes back to it */
    reset_all();
    { mz_bank_t *b = mz_bank(0); mz_pat_t *q = &b->pats[b->pat];
      q->corrupt = 100; q->lock = 0; q->length = 4;
      mz_key_down(0, 60, 0, 0); run_steps(0, 2);
      CHECK(b->step >= 0 && ((b->rewrote >> b->step) & 1), "corrupt 100 marks the step just played");
      q->corrupt = 0; run_steps(0, 4);
      CHECK(b->rewrote == 0, "a pass without corruption clears every mark (got %x)", b->rewrote);
      q->corrupt = 100; run_steps(0, 3); mz_key_up(0, 0, 0); run_steps(0, 2);
      CHECK(!b->running && b->rewrote == 0, "stopping clears the marks"); }

    /* 14. DRM follows the engine: while the loaded synth engine is a drum voice (theseus) the synth bank plays pads
     *     (the held note plus a pad offset; RNG 0 = kick only; SCL rests); otherwise melodies. No manual toggle, the
     *     drum bank never switches, the style is not saved, and 0.3.0's version 2 rows still load */
    reset_all();
    { mz_bank_t *b = mz_bank(0); mz_pat_t *q = &b->pats[b->pat];
      drum_voice = 1;
      mz_press(0, 2, 1); mz_press(1, 2, 1);
      CHECK(b->style == MZ_STYLE_SYN && mz_bank(1)->style == 0, "Shift + white press no longer toggles anything");
      q->corrupt = 0; q->lock = 1; q->density = 100; q->range = 15; q->length = 16;
      for (int i = 0; i < MZ_STEPS; i++) q->val[i] = (uint8_t)i;
      mz_key_down(0, 36, 0, 0); run_steps(0, 2);
      CHECK(b->style == MZ_STYLE_DRM, "theseus loaded: DRM on the next step");
      int sc = b->scale; mz_turn(0, 2, 2048, 1);
      CHECK(b->scale == sc, "SCL rests in DRM");
      int seen = 0;
      for (int i = 0; i < MZ_STEPS; i++) {
          int n = mz_step_note(b, i), pad = mz_step_pad(b, i);
          CHECK(pad == i / 2 && n == 36 + mz_pad_semis[pad], "step %d: note %d pad %d", i, n, pad);
          seen |= 1 << pad;
      }
      CHECK(seen == 0xFF, "range 15 reaches all eight pads (%x)", seen);
      run_steps(0, 4);
      CHECK(last_note - 36 == mz_pad_semis[mz_step_pad(b, b->step)], "played note %d is a pad", last_note);
      q->range = 0;
      for (int i = 0; i < MZ_STEPS; i++) CHECK(mz_step_note(b, i) == 36, "RNG 0: kick only (step %d)", i);
      q->range = 15;
      drum_voice = 0; run_steps(0, 2);
      CHECK(b->style == MZ_STYLE_SYN, "another engine: melodies again");
      CHECK(mz_step_note(b, 15) != 36 + mz_pad_semis[7] || b->scale == 9, "SYN quantizes to the scale again");
      drum_voice = 1;
      { mz_bank_t *d = mz_bank(1); d->pats[d->pat].corrupt = 0; mz_key_down(1, 36, 0, 0); run_steps(1, 3);
        CHECK(d->style == MZ_STYLE_SYN, "the drum bank never switches style"); mz_key_up(1, 0, 0); run_steps(1, 2); mz_all_off(1); }
      mz_key_up(0, 0, 0); run_steps(0, 2); mz_all_off(0);
      static char row[2048], row2[2048];
      int L = mz_serialize(0, row, sizeof row);
      CHECK(L > 0 && row[0] == '1', "version 1 rows again");
      int commas = 0; for (int i = 0; i < L; i++) commas += row[i] == ',';
      CHECK(commas == 229, "230 numbers (got %d)", commas + 1);
      /* a 0.3.0 version 2 row: insert a style field after field 9 */
      int f = 0, o = 0;
      for (int i = 0; i < L; i++) { row2[o++] = row[i]; if (row[i] == ',' && ++f == 10) { row2[o++] = '1'; row2[o++] = ','; } }
      row2[0] = '2'; row2[o] = 0;
      mz_bank_t keep; memcpy(&keep, b, sizeof keep);
      CHECK(mz_parse(0, row2, o) == 1, "v2 row loads");
      CHECK(memcmp(keep.pats, b->pats, sizeof keep.pats) == 0, "v2 row patterns");
      row2[0] = '1'; CHECK(mz_parse(0, row2, o) == 0, "v1 tag on a v2-length row refused"); }

    printf(fails ? "%d FAILED\n" : "all meikyu checks passed\n", fails);
    return fails != 0;
}
