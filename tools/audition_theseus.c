/* Audition: the real meikyu engine (meikyu_core.c, which switches to drum pads because theseus is the loaded engine) clocking the real theseus voice (theseus.c) on a
 * laptop, with the MOD lane turning theseus's knobs, written to a WAV. The OP-1 parts I don't have are stand-ins: a
 * drum-friendly ADSR in place of the stock T2 envelope (instant attack, full sustain, long release, so theseus's own
 * decay shapes each hit), 6-voice round-robin allocation, no effects, no master.
 *   cc -std=c99 -O2 -DOP1_HOST -I<theseus>/host -I<synth_sdk> -I<theseus> -I<meikyu> audition_theseus.c <meikyu>/meikyu_core.c -lm
 *   ./a.out out.wav */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "theseus.c"
#include "meikyu_core.h"

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

#define SR 44100
#define BPM 116.0
static int16_t params[8] = { 9000, 14000, 16000, 8000, 0, 4000, 0, 16000 };   /* plugin.json defaults; page 2: NOIS, LFO, RATE */

typedef struct { int on, id, stage; double lvl, hz; } voice_t;   /* stage 0 off, 1 attack, 2 sustain, 3 release */
static voice_t vs[6];
static int next_v, next_id, pad_count[8];
static const double ATK = 0.001, REL = 1.2;
int mz_out_on(int pos, int note, int key, int vel)
{
    (void)pos; (void)key;
    int v = next_v; next_v = (next_v + 1) % 6;
    vs[v].on = 1; vs[v].id = next_id; vs[v].stage = 1; vs[v].lvl = 0;
    vs[v].hz = 440.0 * pow(2.0, (note - 69) / 12.0);
    host_voice_vel[v] = (int16_t)(vel << 8);
    for (int k = 0; k < 8; k++) if (note - 36 == mz_pad_semis[k]) pad_count[k]++;
    th_note_on(host_voices + v * 0x24, params);
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
int mz_voice(void) { return MZ_VOICE_THESEUS; }                /* theseus is the loaded engine: meikyu plays pads */

static void render(float *mix)
{
    int16_t out[128];
    memset(mix, 0, sizeof(float) * 128);
    for (int v = 0; v < 6; v++) {
        if (!vs[v].on) continue;
        for (int i = 0; i < 128; i++) host_pitch_inc[i] = (int32_t)(uint32_t)(vs[v].hz / SR * 4294967296.0);
        th_render(host_voices + v * 0x24, out, params);
        for (int i = 0; i < 128; i++) {
            voice_t *x = &vs[v];
            if (x->stage == 1) { x->lvl += 1.0 / (ATK * SR); if (x->lvl >= 1) { x->lvl = 1; x->stage = 2; } }
            else if (x->stage == 3) { x->lvl -= x->lvl / (REL * SR / 5); if (x->lvl < 1e-4) { x->on = 0; x->lvl = 0; } }
            mix[i] += out[i] * (float)x->lvl;
        }
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "meikyu_theseus.wav";
    const double len_s = 70.0;
    long frames = (long)(len_s * SR);
    int16_t *pcm = calloc((size_t)frames, sizeof *pcm);
    mz_bank_t *b = mz_bank(0);
    mz_pat_t *q = &b->pats[b->pat];
    b->rate = 3; b->gate = 90;                           /* 1/16 */
    q->corrupt = 0; q->length = 16; q->density = 70; q->range = 0; q->lock = 0;
    const double ticks_per_s = BPM / 60.0 * 24.0;
    float mix[128];
    int scene = -1;
    for (long f = 0; f < frames; f += 128) {
        double t = (double)f / SR;
        int sc = t < 8 ? 0 : t < 18 ? 1 : t < 30 ? 2 : t < 42 ? 3 : t < 54 ? 4 : t < 65 ? 5 : 6;
        if (sc != scene) {
            scene = sc;
            switch (sc) {
            case 0: mz_key_down(0, 36, 0, 0);
                    printf("%5.1fs  theseus loaded, so meikyu plays pads; key C2 held. RNG 0 = every hit on the kick pad; DENS 70, LEN 16, 1/16 at %.0f BPM; theseus at its defaults\n", t, BPM); break;
            case 1: q->range = 15; printf("%5.1fs  RNG 15: the steps spread over all eight pads (kick, toms, snare, rim, hats, metal)\n", t); break;
            case 2: q->corrupt = 30; params[6] = 22000; params[7] = 32767;
                    printf("%5.1fs  CORR 30: the beat rewrites itself; hit LFO depth 22000 at RATE max (flips every other hit)\n", t); break;
            case 3: b->mod_target = 1; b->mod_depth = 60; b->mod_base = 40; b->mod_src = MZ_SRC_ACCENT;
                    params[3] = 20000; params[5] = 14000;
                    printf("%5.1fs  MOD lane -> DCY (blue) from ACCENT: accents ring long, ghosts snap shut; FM 20000, NOIS 14000\n", t); break;
            case 4: b->mod_target = 3; b->mod_depth = 62; b->mod_base = 54; b->mod_src = MZ_SRC_LANE;
                    params[0] = 16000; params[1] = 30000; params[6] = 28000; params[7] = 3000; q->corrupt = 15;
                    printf("%5.1fs  MOD -> CUT (white) from LANE; PNCH 30000 (laser kicks); LFO depth 28000, RATE slow (a 30-hit drift)\n", t); break;
            case 5: b->latched = 1; mz_key_up(0, 0, 0); q->lock = 1; q->length = 12; params[3] = 30000; params[0] = 20000;
                    printf("%5.1fs  HOLD on, key released; LOCK; LEN 12 (the bar turns over early); FM 30000: metal\n", t); break;
            case 6: b->latched = 0; b->stop_pend = 1; printf("%5.1fs  HOLD off: stops on the next step, then the tail\n", t); break;
            }
        }
        mz_advance(0, (uint64_t)(ticks_per_s * 128.0 / SR * 65536.0));
        render(mix);
        for (int i = 0; i < 128 && f + i < frames; i++) {
            double x = mix[i] * 1.5;
            x = tanh(x / 32768.0 * 1.2) * 30000.0;          /* gentle limiter for the file, not part of the plugin */
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
    long peak = 0; double ss = 0; for (long i = 0; i < frames; i++) { long a = labs(pcm[i]); if (a > peak) peak = a; ss += (double)pcm[i] * pcm[i]; }
    printf("wrote %s: %.1f s, peak %ld, rms %.0f, hits %d; per pad:", path, len_s, peak, sqrt(ss / frames), next_id);
    for (int k = 0; k < 8; k++) printf(" %d", pad_count[k]);
    printf("\n");
    free(pcm);
    return 0;
}
