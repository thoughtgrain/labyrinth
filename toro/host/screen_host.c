/* Host preview of the RIDER screen (not part of the plugin file). Runs the real screen code with the raster
 * redirected to SVG <line> elements, toro.svg underneath with the node matrices the screen set, and fake notes:
 *   cc -std=c99 -O2 -DOP1_HOST -I. -I.. -I../../.. -o screen_host screen_host.c -lm && ./screen_host OUTDIR
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "op1_plugin.h"
static FILE *svg_out;
static int mark;
void host_mark(int readout) { mark = readout; }
static int32_t mats[4][9];
void op1_node_set_transform(void *n, const int32_t *m) { memcpy(mats[(uintptr_t)n & 3], m, sizeof mats[0]); }
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c)
{
    (void)r;
    fprintf(svg_out, "<line class=\"%s\" x1=\"%.3f\" y1=\"%.3f\" x2=\"%.3f\" y2=\"%.3f\" stroke=\"#%02x%02x%02x\" stroke-width=\"%.3f\" stroke-linecap=\"round\"/>\n",
            mark ? "ro" : "art", a->x / 1024.0, a->y / 1024.0, b->x / 1024.0, b->y / 1024.0, c[0], c[1], c[2], w / 1024.0);
}
#include "../toro.c"
#include "../toro_screen.c"
int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

static char art[8192];
static char *mtx(int i, char *buf)
{
    const int32_t *m = mats[i];
    sprintf(buf, " transform=\"matrix(%.4f %.4f %.4f %.4f %.4f %.4f)\"", m[0] / 1024.0, m[1] / 1024.0, m[3] / 1024.0, m[4] / 1024.0, m[6] / 1024.0, m[7] / 1024.0);
    return buf;
}
static void frame(const char *dir, const char *name)
{
    char path[512]; snprintf(path, sizeof path, "%s/%s.svg", dir, name);
    FILE *f = fopen(path, "w"); if (!f) { perror(path); exit(1); }
    /* the SVG with the three nodes transformed */
    char out[16384], buf[160]; out[0] = 0;
    const char *p = art;
    struct { const char *tag; int node; } nodes[3] = { { "id=\"shadow\"", 3 }, { "id=\"rod\"", 2 }, { "id=\"rig\"", 1 } };
    while (*p) {
        const char *best = 0; int bn = 0;
        for (int i = 0; i < 3; i++) { const char *q = strstr(p, nodes[i].tag); if (q && (!best || q < best)) { best = q; bn = nodes[i].node; } }
        if (!best) { strcat(out, p); break; }
        size_t l = (size_t)(best - p) + strlen(bn == 3 ? "id=\"shadow\"" : bn == 2 ? "id=\"rod\"" : "id=\"rig\"");
        strncat(out, p, l); strcat(out, mtx(bn, buf)); p += l;
    }
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 160\" width=\"640\" height=\"320\"><rect width=\"320\" height=\"160\" fill=\"#000\"/>%s\n", out);
    svg_out = f;
    tr_screen_draw(0);
    fprintf(f, "</svg>\n");
    fclose(f);
    svg_out = fopen("/dev/null", "w");
}
static int16_t kn[4], w2[3];
static void run(int frames, int note_every, int pg)
{
    for (int i = 0; i < frames; i++) {
        if (note_every && i % note_every == 0) { toro_notes++; toro_key = (int16_t)(40 + (toro_notes * 7) % 24); }
        if (note_every && i % note_every < note_every * 6 / 10) toro_blocks++;
        tr_screen_place(kn, w2, pg, 16);
    }
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    ks_init_host();
    FILE *f = fopen("../toro.svg", "r"); if (!f) { perror("toro.svg"); return 1; }
    char all[8192]; size_t n = fread(all, 1, sizeof all - 1, f); all[n] = 0; fclose(f);
    char *a = strstr(all, "<path"), *e = strstr(all, "</svg>");
    memcpy(art, a, (size_t)(e - a)); art[e - a] = 0;
    svg_out = fopen("/dev/null", "w");
    tr_screen_init(0);
    const struct { const char *name; int16_t k[4], p[3]; int pg, frames; } shots[] = {
        { "t_calm",   { 18000, 0, 18000, 0 },         { 0, 0, 6000 },     0, 40 },
        { "t_mid",    { 18000, 16384, 20000, 16384 }, { 0, 0, 6000 },     0, 34 },
        { "t_wild",   { 30000, 32767, 30000, 32767 }, { 0, 0, 6000 },     0, 32 },
        { "t_page2",  { 20000, 26000, 22000, 26000 }, { 32767, 32767, 32767 }, 7, 33 },
        { "t_driv",   { 20000, 16384, 22000, 16384 }, { 0, 0, 12000 },   4, 33 },
        { "t_aphx_lo",{ 20000, 16384, 22000, 16384 }, { 0, 0, 21000 },   4, 32 },
        { "t_aphx_hi",{ 20000, 16384, 22000, 16384 }, { 0, 0, 32767 },   4, 33 },
    };
    for (unsigned s = 0; s < sizeof shots / sizeof shots[0]; s++) {
        memcpy(kn, shots[s].k, sizeof kn); memcpy(w2, shots[s].p, sizeof w2);
        run(shots[s].frames, 30, shots[s].pg);
        frame(dir, shots[s].name);
    }
    /* sweep: every knob extreme, many frames through notes, both pages */
    int c = 0;
    for (int m = 0; m < 32; m++) {
        for (int i = 0; i < 4; i++) kn[i] = (m >> i) & 1 ? 32767 : 0;
        for (int i = 0; i < 3; i++) w2[i] = (m >> 4) & 1 ? 32767 : 0;
        for (int fr = 0; fr < 30; fr += 3) {
            run(3, 30, (m >> 4) & 1 ? 7 : 0);
            char nm[32]; snprintf(nm, sizeof nm, "sweep_%04d", c++);
            frame(dir, nm);
        }
    }
    printf("%d frames\n", c + 4);
    return 0;
}
