/* Audition: the real meikyu engine (meikyu_core.c) clocking the real toro voice (toro.c) on a laptop, with the
 * MOD lane turning toro's knobs, written to a WAV. The OP-1 parts I don't have are stand-ins: a plain ADSR for the
 * stock T2 envelope, 6-voice round-robin allocation, no effects, no master.
 *   cc -std=c99 -O2 -DOP1_HOST -I<toro>/host -I<synth_sdk> -I<meikyu> audition.c <meikyu>/meikyu_core.c -lm -o audition
 *   ./audition out.wav */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "toro.c"
#include "meikyu_core.h"

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

#define SR 44100
#define BPM 112.0
static int16_t params[8] = { 2000, 6000, 18000, 8000, 0, 0, 0, 6000 };   /* toro defaults (page 2: shape, noise, drive) */

/* ---- voices with a stand-in envelope ---- */
typedef struct { int on, id, stage; double lvl, hz; } voice_t;   /* stage 0 off, 1 attack, 2 decay/sustain, 3 release */
static voice_t vs[6];
static int next_v, next_id;
static const double ATK = 0.004, DEC = 0.22, SUS = 0.55, REL = 0.16;
int mz_out_on(int pos, int note, int key, int vel)
{
    (void)pos; (void)key;
    int v = next_v; next_v = (next_v + 1) % 6;
    vs[v].on = 1; vs[v].id = next_id; vs[v].stage = 1; vs[v].lvl = 0;
    vs[v].hz = 440.0 * pow(2.0, (note - 69) / 12.0);
    host_voice_vel[v] = (int16_t)(vel << 8);
    if (getenv("AUD_LOG")) printf("  note %d vel %d fold %d cut %d\n", note, vel, params[1], params[2]);
    mt_note_on(host_voices + v * 0x24, params);
    return next_id++ & 0x7fff;
}
void mz_out_off(int id) { for (int v = 0; v < 6; v++) if (vs[v].on && vs[v].id == id && vs[v].stage < 3) vs[v].stage = 3; }
int mz_out_param(int knob, int value)                    /* the MOD lane: same as turning the knob */
{
    if (knob < 0 || knob > 3) return -1;
    params[knob] = (int16_t)(value * 258 > 32767 ? 32767 : value * 258);
    return 0;
}
int mz_music(int *root, uint16_t *mask) { (void)root; (void)mask; return 0; }
void mz_changed(void) {}
int mz_voice(void) { return MZ_VOICE_TORO; }            /* toro is loaded: melodies, toro's MOD names */

static void render(float *mix, int n)                     /* n = 128 */
{
    int16_t out[128];
    memset(mix, 0, sizeof(float) * n);
    for (int v = 0; v < 6; v++) {
        if (!vs[v].on) continue;
        for (int i = 0; i < 128; i++) host_pitch_inc[i] = (int32_t)(uint32_t)(vs[v].hz / SR * 4294967296.0);
        mt_render(host_voices + v * 0x24, out, params);
        for (int i = 0; i < n; i++) {
            voice_t *x = &vs[v];
            if (x->stage == 1) { x->lvl += 1.0 / (ATK * SR); if (x->lvl >= 1) { x->lvl = 1; x->stage = 2; } }
            else if (x->stage == 2) x->lvl += (SUS - x->lvl) / (DEC * SR / 4);
            else if (x->stage == 3) { x->lvl -= x->lvl / (REL * SR / 5); if (x->lvl < 1e-4) { x->on = 0; x->lvl = 0; } }
            mix[i] += out[i] * (float)x->lvl;
        }
    }
}

int main(int argc, char **argv)
{
    /* APHX demo: one meikyu phrase on toro, locked, played four times with white page 2 at full drive (APHX 0),
     * then APHX 25 %, 50 % and 100 %. 8 seconds each. */
    const char *path = argc > 1 ? argv[1] : "toro_aphx.wav";
    const double len_s = 33.0;
    long frames = (long)(len_s * SR);
    int16_t *pcm = calloc((size_t)frames, sizeof *pcm);
    mz_bank_t *b = mz_bank(0);
    mz_pat_t *q = &b->pats[b->pat];
    b->rate = 3; b->gate = 70; b->scale = 2;
    q->corrupt = 0; q->length = 16; q->density = 80; q->range = 14; q->lock = 1;
    params[0] = 5000; params[1] = 9000; params[2] = 20000; params[3] = 12000; params[5] = 6000;
    const double ticks_per_s = BPM / 60.0 * 24.0;
    static const int16_t W7[4] = { 16384, 20480, 24576, 32767 };
    static const char *const WHAT[4] = { "DRIV 99, APHX 0: the plain sound", "APHX 25: a few cents per key, the same sound",
                                         "APHX 50: keys drift apart, the filter and fold start to move", "APHX 100: retuned keys, thrown filter, crushed" };
    float mix[128];
    int scene = -1;
    for (long f = 0; f < frames; f += 128) {
        double t = (double)f / SR;
        int sc = t < 8 ? 0 : t < 16 ? 1 : t < 24 ? 2 : 3;
        if (sc != scene) {
            scene = sc; params[7] = W7[sc];
            if (sc == 0) { mz_key_down(0, 48, 0, 1); mz_key_up(0, 0, 1); }
            printf("%5.1fs  %s\n", t, WHAT[sc]);
        }
        if (t >= 32.0 && b->running) { b->latched = 0; b->stop_pend = 1; }
        mz_advance(0, (uint64_t)(ticks_per_s * 128.0 / SR * 65536.0));
        render(mix, 128);
        for (int i = 0; i < 128 && f + i < frames; i++) {
            double x = mix[i] * 1.5;
            x = tanh(x / 32768.0 * 1.2) * 30000.0;
            pcm[f + i] = (int16_t)lrint(x);
        }
    }
    for (long i = 0; i < 1323; i++) pcm[frames - 1 - i] = (int16_t)(pcm[frames - 1 - i] * (i / 1323.0));
    FILE *o = fopen(path, "wb"); if (!o) { perror(path); return 1; }
    uint32_t data = (uint32_t)frames * 2, riff = 36 + data, sr = SR, br = SR * 2, fmt = 16;
    uint16_t pcmfmt = 1, ch = 1, ba = 2, bits = 16;
    fwrite("RIFF", 1, 4, o); fwrite(&riff, 4, 1, o); fwrite("WAVEfmt ", 1, 8, o); fwrite(&fmt, 4, 1, o);
    fwrite(&pcmfmt, 2, 1, o); fwrite(&ch, 2, 1, o); fwrite(&sr, 4, 1, o); fwrite(&br, 4, 1, o); fwrite(&ba, 2, 1, o); fwrite(&bits, 2, 1, o);
    fwrite("data", 1, 4, o); fwrite(&data, 4, 1, o); fwrite(pcm, 2, (size_t)frames, o); fclose(o);
    printf("wrote %s, notes %d\n", path, next_id);
    return 0;
}
