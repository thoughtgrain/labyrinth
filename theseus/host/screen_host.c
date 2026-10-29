/* Host preview of the PHALANX screen (not part of the plugin file). Runs the real screen code with the raster
 * redirected to SVG <line> elements, theseus.svg underneath, and fake hits:
 *   cc -std=c99 -O2 -DOP1_HOST -I. -I.. -I../../.. -o screen_host screen_host.c -lm && ./screen_host OUTDIR
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "op1_plugin.h"
static FILE *svg_out;
static int mark;
void host_mark(int readout) { mark = readout; }
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c)
{
    (void)r;
    fprintf(svg_out, "<line class=\"%s\" x1=\"%.3f\" y1=\"%.3f\" x2=\"%.3f\" y2=\"%.3f\" stroke=\"#%02x%02x%02x\" stroke-width=\"%.3f\" stroke-linecap=\"round\"/>\n",
            mark ? "ro" : "art", a->x / 1024.0, a->y / 1024.0, b->x / 1024.0, b->y / 1024.0, c[0], c[1], c[2], w / 1024.0);
}
#include "../theseus.c"
#include "../theseus_screen.c"
int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

static char art[8192];
static void frame(const char *dir, const char *name)
{
    char path[512]; snprintf(path, sizeof path, "%s/%s.svg", dir, name);
    FILE *f = fopen(path, "w"); if (!f) { perror(path); exit(1); }
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 160\" width=\"640\" height=\"320\"><rect width=\"320\" height=\"160\" fill=\"#000\"/>%s\n", art);
    svg_out = f;
    th_screen_draw(0);
    fprintf(f, "</svg>\n");
    fclose(f);
    svg_out = fopen("/dev/null", "w");
}
static int16_t kn[4], w2[3];
/* a DRM-ish pattern: zones of the eight pads (root C2), one hit every `every` frames */
static const int16_t PADZ[8] = { 1966, 5400, 6750, 10158, 13600, 18350, 23265, 26542 };
static const int8_t PAT[16] = { 0, 5, 4, 5, 0, 5, 3, 6, 0, 0, 4, 5, 1, 5, 2, 7 };
static int step;
static void hit(int pad, int vel)
{
    theseus_zone = PADZ[pad & 7]; theseus_vel = (int16_t)(vel * 258);
    th_note_on(host_voices, (const int16_t[8]){ 0, 0, 0, 0, 0, w2[0], w2[1], w2[2] });   /* steps the hit LFO, counts the hit */
}
static void run(int frames, int every, int pg)
{
    for (int i = 0; i < frames; i++) {
        if (every && i % every == 0) { hit(PAT[step & 15], step % 4 == 0 ? 118 : 92); step++; }
        th_screen_place(kn, w2, pg, 16);
    }
}
static void shot_hit(const char *dir, const char *name, int pad, int after, int pg)
{
    hit(pad, 118);
    for (int i = 0; i <= after; i++) th_screen_place(kn, w2, pg, 16);
    frame(dir, name);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    ks_init_host();
    FILE *f = fopen("../theseus.svg", "r"); if (!f) { perror("theseus.svg"); return 1; }
    char all[8192]; size_t n = fread(all, 1, sizeof all - 1, f); all[n] = 0; fclose(f);
    char *a = strstr(all, "<path"), *e = strstr(all, "</svg>");
    memcpy(art, a, (size_t)(e - a)); art[e - a] = 0;
    svg_out = fopen("/dev/null", "w");
    th_screen_init(0);
    const struct { const char *name; int16_t k[4], p[3]; int pg, pad, after; } shots[] = {
        { "t_idle",   { 12000, 14000, 16000, 8000 },  { 4000, 0, 16000 },     0, -1, 0 },
        { "t_kick",   { 12000, 26000, 16000, 8000 },  { 4000, 0, 16000 },     0, 0, 1 },
        { "t_snare",  { 20000, 14000, 26000, 14000 }, { 4000, 0, 16000 },     0, 4, 2 },
        { "t_hat",    { 6000, 14000, 30000, 20000 },  { 26000, 0, 16000 },    1, 5, 3 },
        { "t_cym",    { 30000, 8000, 32767, 32767 },  { 32767, 32767, 32767 }, 0, 7, 4 },
        { "t_floor",  { 26000, 30000, 4000, 4000 },   { 0, 32767, 32767 },    6, 1, 6 },
        { "t_page2",  { 20000, 20000, 20000, 20000 }, { 20000, 26000, 30000 }, 7, 3, 2 },
    };
    for (unsigned s = 0; s < sizeof shots / sizeof shots[0]; s++) {
        memcpy(kn, shots[s].k, sizeof kn); memcpy(w2, shots[s].p, sizeof w2);
        run(40, 8, shots[s].pg);
        if (shots[s].pad < 0) { run(300, 0, 0); frame(dir, shots[s].name); }
        else shot_hit(dir, shots[s].name, shots[s].pad, shots[s].after, shots[s].pg);
    }
    /* the five heads, settled: left, left 3/4 (on the way to front), front, right 3/4, right */
    memcpy(kn, shots[3].k, sizeof kn); memcpy(w2, shots[3].p, sizeof w2);
    shot_hit(dir, "h_left", 5, 12, 0);
    hit(0, 118); for (int i = 0; i < 2; i++) th_screen_place(kn, w2, 0, 16); frame(dir, "h_left34");
    shot_hit(dir, "h_front", 0, 12, 0);
    hit(3, 118); for (int i = 0; i < 2; i++) th_screen_place(kn, w2, 0, 16); frame(dir, "h_right34");
    shot_hit(dir, "h_right", 3, 12, 0);
    /* sweep: every knob extreme, every pad, frames through each hit, both pages */
    int c = 0;
    for (int m = 0; m < 32; m++) {
        for (int i = 0; i < 4; i++) kn[i] = (m >> i) & 1 ? 32767 : 0;
        for (int i = 0; i < 3; i++) w2[i] = (m >> 4) & 1 ? 32767 : 0;
        for (int pad = 0; pad < 8; pad++) {
            hit(pad, (m + pad) & 1 ? 118 : 70);
            for (int fr = 0; fr < 24; fr += 4) {
                for (int z = 0; z < 4; z++) th_screen_place(kn, w2, (m >> 4) & 1 ? 7 : 0, 16);
                char nm[32]; snprintf(nm, sizeof nm, "sweep_%04d", c++);
                frame(dir, nm);
            }
        }
    }
    printf("%d frames\n", c + (int)(sizeof shots / sizeof shots[0]));
    return 0;
}
