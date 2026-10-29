/* Host preview of the THREAD screen (not part of the plugin file). Runs the real engine and the real screen code,
 * with the raster redirected to SVG <line> elements over meikyu.svg, and writes frames for the preview / checks:
 *   cc -std=c99 -O2 -DOP1_HOST -I.. -o screen_host screen_host.c ../meikyu_core.c -lm && ./screen_host OUTDIR
 * Each frame is OUTDIR/<name>.svg plus a line of JSON on stdout: the readout boxes, for check_screen.py.
 * Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../../op1_plugin.h"
static FILE *svg_out;
static int mark;
void host_mark(int readout) { mark = readout; }
void op1_node_set_transform(void *n, const int32_t *m) { (void)n; (void)m; }
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c)
{
    (void)r;
    fprintf(svg_out, "<line class=\"%s\" x1=\"%.3f\" y1=\"%.3f\" x2=\"%.3f\" y2=\"%.3f\" stroke=\"#%02x%02x%02x\" stroke-width=\"%.3f\" stroke-linecap=\"round\"/>\n", mark == 1 ? "ro" : mark == 2 ? "ro2" : "art",
            a->x / 1024.0, a->y / 1024.0, b->x / 1024.0, b->y / 1024.0, c[0], c[1], c[2], w / 1024.0);
}
#include "../meikyu_screen.c"

int  mz_out_on(int pos, int note, int key, int vel) { (void)pos; (void)note; (void)key; (void)vel; static int id; return id++ & 1023; }
void mz_out_off(int id) { (void)id; }
int  mz_out_param(int knob, int value) { (void)knob; (void)value; return 0; }
int  mz_music(int *root, uint16_t *mask) { (void)root; (void)mask; return 0; }
void mz_changed(void) {}
static int drum_voice;
int mz_voice(void) { return drum_voice ? MZ_VOICE_THESEUS : MZ_VOICE_TORO; }

static char static_art[8192];
static void frame(const char *dir, const char *name, int pos, int shift)
{
    char path[512]; snprintf(path, sizeof path, "%s/%s.svg", dir, name);
    svg_out = fopen(path, "w");
    if (!svg_out) { perror(path); exit(1); }
    fprintf(svg_out, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 160\" width=\"640\" height=\"320\">"
                     "<rect width=\"320\" height=\"160\" fill=\"#000\"/>%s<g id=\"live\">\n", static_art);
    mz_screen(0, pos, shift);
    fprintf(svg_out, "</g></svg>\n");
    fclose(svg_out);
}
static void tick_steps(int pos, int frames)            /* about 60 frames/s at 120 BPM: 2 beats/s, 96 ticks/beat */
{
    for (int f = 0; f < frames; f++) {
        for (int t = 0; t < 3; t++) mz_advance(pos, 1u << 16);   /* 3 x 24ths of a beat per frame ~ 1/16 at 1/16 rate in 2 frames */
        mz_screen(0, pos, 0);                                     /* advance the screen's own frame counter */
    }
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    ks_init_host();
    FILE *f = fopen("../meikyu.svg", "r");
    if (!f) { perror("meikyu.svg"); return 1; }
    char all[8192]; size_t n = fread(all, 1, sizeof all - 1, f); all[n] = 0; fclose(f);
    char *a = strstr(all, "<path"), *e = strstr(all, "</svg>");
    if (a && e) { memcpy(static_art, a, (size_t)(e - a)); static_art[e - a] = 0; }
    svg_out = fopen("/dev/null", "w");

    mz_bank_t *b = mz_bank(0);
    mz_pat_t *q = &b->pats[b->pat];
    q->corrupt = 60; q->length = 14; q->density = 70; q->range = 14;
    mz_key_down(0, 60, 0, 0);
    tick_steps(0, 200);
    fclose(svg_out);
    frame(dir, "m_main", 0, 0);
    frame(dir, "m_shift", 0, 1);
    b->page = 1; b->mod_target = 2; frame(dir, "m_mod", 0, 0); b->page = 0; b->mod_target = 0;   /* toro loaded: FOLD */
    svg_out = fopen("/dev/null", "w");
    b->latched = 1; tick_steps(0, 7); fclose(svg_out); frame(dir, "m_hold", 0, 0);
    q->lock = 1; q->range = 0; q->length = 5; frame(dir, "m_lock", 0, 0);
    /* sweep for the checks: every length, corrupt and range extremes, many frames */
    int k = 0;
    for (int len = 2; len <= 16; len += 2) for (int rg = 0; rg <= 15; rg += 15) for (int sh = 0; sh < 2; sh++) {
        q->lock = 0; q->length = (uint8_t)len; q->range = (uint8_t)rg; q->corrupt = 100; q->density = 100;
        svg_out = fopen("/dev/null", "w"); tick_steps(0, 9 + len); fclose(svg_out);
        char nm[32]; snprintf(nm, sizeof nm, "sweep_%03d", k++);
        frame(dir, nm, 0, sh);
    }
    /* theseus loaded: DRM, square pads, SCL reads DRM */
    drum_voice = 1;
    q->corrupt = 50; q->length = 16; q->density = 75; q->range = 15; q->lock = 0;
    svg_out = fopen("/dev/null", "w"); tick_steps(0, 40); fclose(svg_out);
    frame(dir, "m_drm", 0, 0); frame(dir, "m_drm_shift", 0, 1); k += 2;
    drum_voice = 0;
    mz_key_up(0, 0, 0);
    printf("%d frames\n", k + 5);
    return 0;
}
