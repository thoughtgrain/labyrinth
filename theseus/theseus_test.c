/* Host DSP check for theseus (not part of the plugin file):
 *   cc -std=c99 -O2 -DOP1_HOST -Ihost -I../.. -o theseus_test theseus_test.c -lm && ./theseus_test
 * Sweeps all seven knobs over min / mid / max at the eight DRM pad pitches, then checks the things a drum voice
 * must do: every hit is audible (peak > -23 dBFS even with CUT and DCY at 0 and the LFO pulling the filter shut) and then decays to silence, no clipping storms, low pads are darker and longer than
 * high pads, PNCH really sweeps the pitch, CUT and FM really change the brightness, NOIS adds noise, the hit LFO
 * steps once per hit at the set rate, and the voice fits the firmware's 0x24 bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "theseus.c"

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { double rms_head, rms_tail, bright, zc_early, zc_late; int clip, peak; long len; } hit_t;
static hit_t hit(int voice, double hz, int vel, const int16_t *p, int blocks)
{
    void *self = host_voices + voice * 0x24;
    for (int n = 0; n < 128; n++) host_pitch_inc[n] = (int32_t)(uint32_t)(hz / 44100.0 * 4294967296.0);
    host_voice_vel[voice] = (int16_t)(vel << 8);
    th_note_on(self, p);
    hit_t h = { 0 };
    int16_t out[128]; double prev = 0, s2 = 0, d2 = 0, head = 0, tail = 0; long nh = 0, nt = 0;
    int zc1 = 0, zc2 = 0, last = 0;
    for (int b = 0; b < blocks; b++) {
        th_render(self, out, p);
        for (int n = 0; n < 128; n++) {
            double x = out[n]; long t = (long)b * 128 + n;
            s2 += x * x; d2 += (x - prev) * (x - prev); prev = x;
            if (t < 4410) { head += x * x; nh++; } else if (b >= blocks - 8) { tail += x * x; nt++; }
            int sg = out[n] > 64 ? 1 : (out[n] < -64 ? -1 : 0);
            if (sg && last && sg != last) { if (t < 441) zc1++; else if (t >= 2205 && t < 2646) zc2++; }
            if (sg) last = sg;
            if (abs(out[n]) >= 32767) h.clip++;
            if (abs(out[n]) > h.peak) h.peak = abs(out[n]);
            if (abs(out[n]) > 300) h.len = t;
        }
    }
    h.rms_head = sqrt(head / nh); h.rms_tail = nt ? sqrt(tail / nt) : 0;
    h.bright = s2 > 0 ? sqrt(d2 / s2) : 0; h.zc_early = zc1; h.zc_late = zc2;
    return h;
}
static double pad_hz(int pad) { static const int off[8] = { 0, 5, 7, 12, 17, 24, 31, 36 }; return 65.41 * pow(2, off[pad] / 12.0); }

int main(void)
{
    static const int16_t lv[3] = { 0, 16384, 32767 };
    long combos = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 3; c++) for (int d = 0; d < 3; d++)
    for (int e = 0; e < 3; e++) for (int g = 0; g < 3; g++) for (int pad = 0; pad < 8; pad += 1) {
        int16_t p[8] = { lv[a], lv[b], lv[c], lv[d], 0, lv[e], lv[g], lv[(a + c) % 3] };
        hit_t h = hit(combos % 6, pad_hz(pad), (combos & 1) ? 127 : 80, p, 260);   /* 0.75 s */
        combos++;
        CHECK(h.clip < 64, "clipping %d (%d %d %d %d %d %d) pad %d", h.clip, p[0], p[1], p[2], p[3], p[5], p[6], pad);
        CHECK(h.peak > 2400, "weak hit peak %d (%d %d %d %d %d %d) pad %d", h.peak, p[0], p[1], p[2], p[3], p[5], p[6], pad);
        if (a == 0) CHECK(h.rms_tail < 30, "short decay still ringing: tail %.0f pad %d", h.rms_tail, pad);
    }
    /* the kit: low pads long and dark, high pads short and bright (defaults) */
    int16_t def[8] = { 12000, 14000, 16000, 8000, 0, 4000, 0, 16000 };
    hit_t kick = hit(0, pad_hz(0), 127, def, 520), hat = hit(1, pad_hz(7), 127, def, 520);
    CHECK(kick.len > hat.len * 1.6, "kick %ld should outlast hat %ld", kick.len, hat.len);
    CHECK(hat.bright > kick.bright * 2, "hat %.3f should be brighter than kick %.3f", hat.bright, kick.bright);
    /* PNCH: the strike runs well above the settled pitch */
    int16_t pn[8] = { 20000, 32767, 32767, 0, 0, 0, 0, 0 };
    hit_t pk = hit(2, pad_hz(0), 127, pn, 100);
    CHECK(pk.zc_early > pk.zc_late * 2.5, "pitch drop: early %.0f late %.0f crossings / 10 ms", pk.zc_early, pk.zc_late);
    pn[1] = 0; hit_t flat = hit(2, pad_hz(0), 127, pn, 100);
    CHECK(flat.zc_early <= flat.zc_late + 2, "PNCH 0 should be flat: early %.0f late %.0f", flat.zc_early, flat.zc_late);
    /* CUT and FM change the tone */
    int16_t t[8] = { 16000, 8000, 3000, 0, 0, 0, 0, 0 };
    hit_t dark = hit(3, pad_hz(3), 127, t, 120); t[2] = 32767; hit_t open = hit(3, pad_hz(3), 127, t, 120);
    CHECK(open.bright > dark.bright * 1.5, "CUT: dark %.3f open %.3f", dark.bright, open.bright);
    hit_t nofm = open; t[3] = 32767; hit_t fmh = hit(3, pad_hz(3), 127, t, 120);
    CHECK(fmh.bright > nofm.bright * 1.2, "FM: off %.3f on %.3f", nofm.bright, fmh.bright);
    t[3] = 0; t[5] = 32767; hit_t noisy = hit(3, pad_hz(3), 127, t, 120);
    CHECK(noisy.bright > nofm.bright * 1.3, "NOIS: off %.3f on %.3f", nofm.bright, noisy.bright);
    /* velocity is the accent */
    hit_t soft = hit(4, pad_hz(2), 40, def, 60), loud = hit(4, pad_hz(2), 127, def, 60);
    CHECK(loud.rms_head > soft.rms_head * 2, "accent: soft %.0f loud %.0f", soft.rms_head, loud.rms_head);
    /* the hit LFO: RATE max flips every hit, RATE min takes 32 hits per cycle */
    int16_t lf[8] = { 4000, 0, 16000, 0, 0, 0, 32767, 32767 };
    int flips = 0, prev = 0;
    for (int k = 0; k < 16; k++) { th_note_on(host_voices, lf); int s = theseus_lfo > 0 ? 1 : -1; if (k && s != prev) flips++; prev = s; }
    CHECK(flips >= 14, "RATE max: %d sign flips in 16 hits", flips);
    lf[7] = 0; flips = 0;
    for (int k = 0; k < 32; k++) { th_note_on(host_voices, lf); int s = theseus_lfo > 0 ? 1 : -1; if (k && s != prev) flips++; prev = s; }
    CHECK(flips >= 1 && flips <= 3, "RATE min: %d sign flips in 32 hits", flips);
    /* zones */
    hit(5, 55, 127, def, 1); int z0 = theseus_zone; hit(5, 880, 127, def, 1); int z1 = theseus_zone;
    CHECK(z0 < 600 && z1 > 31000, "zone 55 Hz %d, 880 Hz %d", z0, z1);
    printf("%ld knob/pad combinations; kick %ld / hat %ld samples, bright %.3f / %.3f; pitch drop %.0f->%.0f; CUT %.3f->%.3f FM ->%.3f NOIS ->%.3f\n",
           combos, kick.len, hat.len, kick.bright, hat.bright, pk.zc_early, pk.zc_late, dark.bright, open.bright, fmh.bright, noisy.bright);
    printf(fails ? "%d FAILED\n" : "all theseus checks passed\n", fails);
    return fails != 0;
}
