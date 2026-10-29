/* Kit sampler: every DRM pad through theseus, one hit each (normal, then accent), at three settings:
 * the defaults, "laser" (PNCH and CUT up, short decay) and "metal" (FM and NOIS up, long decay). 0.45 s per hit.
 *   cc -std=c99 -O2 -DOP1_HOST -I<theseus>/host -I<synth_sdk> -I<theseus> kit_theseus.c -lm && ./a.out kit.wav */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "theseus.c"
static const int SEMIS[8] = { 0, 5, 7, 12, 17, 24, 31, 36 };
int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];
#define SR 44100
int main(int argc, char **argv)
{
    static const int16_t set[3][8] = { { 9000, 14000, 16000, 8000, 0, 4000, 0, 16000 },
                                       { 5000, 32767, 26000, 4000, 0, 0, 0, 16000 },
                                       { 22000, 8000, 22000, 30000, 0, 20000, 0, 16000 } };
    const int per = (int)(0.45 * SR / 128) * 128, n = 3 * 16 * per + SR / 2;
    int16_t *pcm = calloc((size_t)n, 2); long o = 0;
    for (int s = 0; s < 3; s++) for (int pad = 0; pad < 8; pad++) for (int acc = 0; acc < 2; acc++) {
        double hz = 65.41 * pow(2, SEMIS[pad] / 12.0);
        for (int i = 0; i < 128; i++) host_pitch_inc[i] = (int32_t)(uint32_t)(hz / SR * 4294967296.0);
        host_voice_vel[0] = (int16_t)((acc ? 118 : 92) << 8);
        th_note_on(host_voices, set[s]);
        int16_t out[128];
        for (int b = 0; b < per / 128; b++) {
            th_render(host_voices, out, set[s]);
            for (int i = 0; i < 128; i++) pcm[o++] = (int16_t)lrint(tanh(out[i] * 1.4 / 32768.0) * 30000.0);
        }
    }
    FILE *f = fopen(argc > 1 ? argv[1] : "kit.wav", "wb");
    uint32_t data = (uint32_t)n * 2, riff = 36 + data, sr = SR, br = SR * 2, fmt = 16; uint16_t pf = 1, ch = 1, ba = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&pf, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(pcm, 2, (size_t)n, f); fclose(f);
    printf("%d hits, %.1f s\n", 48, n / (double)SR);
    return 0;
}
