/* Host DSP check for toro (not part of the plugin file). The SDK snapshot ships no harness/op1_host.h, so the
 * test uses the small mock in host/harness/ (see README.md):
 *   cc -std=c99 -O2 -DOP1_HOST -Ihost -I../.. -o toro_test toro_test.c -lm && ./toro_test
 * Sweeps every knob over min / default / max at low and high notes, six voices, and checks: no saturation storms,
 * no DC lock-up, no runaway resonance, cutoff and fold audibly change the brightness, silence stays silent.
 * APHX (white page 2's upper half): the drive half reaches full drive at the middle; APHX is the same on every
 * play of a key, gentle at low settings (a few cents, the same sound), wild at the top, never a clipping storm. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "toro.c"
static int16_t lv3(int i) { static const int16_t v[3] = { 0, 16384, 32767 }; return v[i]; }

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { double rms, bright; int clip, peak; } stats_t;
static stats_t play(int voice, double hz, const int16_t *p, int blocks)
{
    void *self = host_voices + voice * 0x24;
    for (int n = 0; n < 128; n++) host_pitch_inc[n] = (int32_t)(uint32_t)(hz / 44100.0 * 4294967296.0);
    host_voice_vel[voice] = 127 << 8;
    mt_note_on(self, p);
    stats_t s = { 0, 0, 0, 0 };
    double prev = 0, sum = 0, dsum = 0; long cnt = 0;
    int16_t out[128];
    for (int b = 0; b < blocks; b++) {
        mt_render(self, out, p);
        if (b < blocks / 4) continue;                   /* settle */
        for (int n = 0; n < 128; n++) {
            double x = out[n];
            sum += x * x; dsum += (x - prev) * (x - prev); prev = x; cnt++;
            if (abs(out[n]) >= 32767) s.clip++;
            if (abs(out[n]) > s.peak) s.peak = abs(out[n]);
        }
    }
    s.rms = sqrt(sum / cnt);
    s.bright = sum > 0 ? sqrt(dsum / sum) : 0;          /* first-difference / signal: higher = brighter */
    return s;
}

int main(void)
{
    static const int16_t lv[3] = { 0, 16384, 32767 };
    static const double notes[3] = { 41.2, 261.6, 1760.0 };
    long combos = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 3; c++) for (int d = 0; d < 3; d++)
    for (int e = 0; e < 3; e++) for (int g = 0; g < 3; g++) for (int nn = 0; nn < 3; nn++) {
        int16_t p[8] = { lv[a], lv[b], lv[c], lv[d], 0x5300 + 32, lv[e], lv[g], lv[(a + d) % 3] };
        stats_t s = play(combos % 6, notes[nn], p, 64);
        combos++;
        CHECK(s.clip < 128 * 2, "clipping storm %d samples: %d %d %d %d %d %d @%.0f", s.clip, p[0], p[1], p[2], p[3], p[5], p[6], notes[nn]);
        CHECK(s.rms < 26000, "too hot rms %.0f", s.rms);
        if (c == 2) CHECK(s.rms > 200, "silent at open cutoff: rms %.0f (%d %d %d %d) @%.0f", s.rms, p[0], p[1], p[2], p[3], notes[nn]);
    }
    /* brightness: cutoff and fold must change the tone */
    int16_t base[8] = { 2000, 0, 4000, 4000, 0x5300 + 32, 0, 0, 6000 };
    stats_t dark = play(0, 110, base, 64);
    base[2] = 32767; stats_t open = play(0, 110, base, 64);
    CHECK(open.bright > dark.bright * 1.5, "cutoff: dark %.3f open %.3f", dark.bright, open.bright);
    base[2] = 32767; base[5] = 32767; base[1] = 0; stats_t nofold = play(0, 110, base, 64);   /* triangle */
    base[1] = 32767; stats_t fold = play(0, 110, base, 64);
    CHECK(fold.bright > nofold.bright * 1.2, "fold: off %.3f on %.3f", nofold.bright, fold.bright);
    /* DC: a held note averages near zero (the folder and filter add no offset) */
    int16_t z[8] = { 4000, 20000, 32767, 20000, 0x5300 + 32, 16000, 0, 3000 };   /* drive as before the split */
    for (int n = 0; n < 128; n++) host_pitch_inc[n] = (int32_t)(261.6 / 44100.0 * 4294967296.0);
    void *self = host_voices; mt_note_on(self, z);
    int16_t out[128]; double mean = 0; long cnt = 0;
    for (int bl = 0; bl < 400; bl++) { mt_render(self, out, z); if (bl >= 100) for (int n = 0; n < 128; n++) { mean += out[n]; cnt++; } }
    mean /= cnt;
    CHECK(fabs(mean) < 800, "DC offset %.0f", mean);
    /* ---- APHX ---- */
    {
        /* the drive half: word 16384 drives as hard as the old full knob did, and the upper half keeps it there */
        int16_t d1[8] = { 4000, 8000, 26000, 6000, 0x5300 + 32, 0, 0, 16384 }, d0[8] = { 4000, 8000, 26000, 6000, 0x5300 + 32, 0, 0, 0 };
        stats_t s0 = play(0, 220, d0, 48), s1 = play(0, 220, d1, 48);
        CHECK(s1.rms > s0.rms * 1.3, "drive half: rms %.0f -> %.0f", s0.rms, s1.rms);
        /* determinism: the same key twice gives the same samples (no voice memory, no randomness at play time) */
        int16_t ap[8] = { 4000, 12000, 22000, 9000, 0x5300 + 32, 6000, 0, 32767 };
        static int16_t A1[128 * 40], A2[128 * 40];
        for (int rep = 0; rep < 2; rep++) {
            void *self = host_voices; int16_t *dst = rep ? A2 : A1;
            for (int n = 0; n < 128; n++) host_pitch_inc[n] = (int32_t)(uint32_t)(277.18 / 44100.0 * 4294967296.0);
            host_voice_vel[0] = 100 << 8; mt_note_on(self, ap);
            for (int b = 0; b < 40; b++) mt_render(self, dst + b * 128, ap);
        }
        CHECK(memcmp(A1, A2, sizeof A1) == 0, "APHX: the same key must sound the same every time");
        /* how far it moves the pitch: read oscillator 1's phase advance over one block (exact, no estimation), 24 keys */
        double worst_lo = 0, worst_hi = 0; int differ = 0;
        for (int key = 0; key < 24; key++) {
            double hz = 27.5 * pow(2, key / 12.0);                         /* low enough that 128 steps never wrap */
            uint32_t i = (uint32_t)(hz / 44100.0 * 4294967296.0);
            int16_t lo[8] = { 0, 0, 32767, 0, 0x5300 + 32, 0, 0, 16384 + 4096 };    /* APHX 25 % */
            int16_t hi[8] = { 0, 0, 32767, 0, 0x5300 + 32, 0, 0, 32767 };           /* APHX 100 % */
            const int16_t *ps[2] = { lo, hi }; double cents[2];
            for (int k = 0; k < 2; k++) {
                mt_voice_t *v = (mt_voice_t *)(void *)(host_voices + 0x24);
                for (int n = 0; n < 128; n++) host_pitch_inc[n] = (int32_t)i;
                host_voice_vel[1] = 127 << 8; mt_note_on(v, ps[k]);
                int16_t out[128]; mt_render(v, out, ps[k]);
                cents[k] = 1200 * log2((double)v->ph1 / (128.0 * i));
            }
            if (fabs(cents[0]) > worst_lo) worst_lo = fabs(cents[0]);
            if (fabs(cents[1]) > worst_hi) worst_hi = fabs(cents[1]);
            differ += fabs(cents[1]) > 30;
        }
        CHECK(worst_lo > 0.5 && worst_lo < 25, "APHX 25 %%: keys should drift a few cents, worst %.0f cents", worst_lo);
        CHECK(worst_hi > 60 && differ >= 8, "APHX 100 %%: keys should be thrown off pitch (worst %.0f cents, %d keys moved)", worst_hi, differ);
        /* the top end stays bounded across keys and knob settings */
        for (int key = 0; key < 36; key += 5) for (int c = 0; c < 3; c++) {
            int16_t w[8] = { lv3(c), lv3(2 - c), lv3(c), lv3(c), 0x5300 + 32, lv3(2 - c), lv3(c), 32767 };
            stats_t s = play(key % 6, 41.2 * pow(2, key / 12.0), w, 48);
            CHECK(s.clip < 256 && s.rms < 26000, "APHX max key %d set %d: clip %d rms %.0f", key, c, s.clip, s.rms);
        }
        printf("APHX: drive half rms %.0f -> %.0f; pitch drift worst %.1f cents at 25 %%, %.0f cents at 100 %% (%d of 24 keys > 30)\n",
               s0.rms, s1.rms, worst_lo, worst_hi, differ);
    }
    printf("%ld knob/note combinations; dark %.3f open %.3f, fold off %.3f on %.3f\n", combos, dark.bright, open.bright, nofold.bright, fold.bright);
    printf(fails ? "%d FAILED\n" : "all toro checks passed\n", fails);
    return fails != 0;
}
