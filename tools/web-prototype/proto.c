/* Browser prototype glue: the real meikyu core (synth bank, which plays drum pads while theseus is loaded), the real
 * theseus voice and the real PHALANX screen code, compiled to WebAssembly. The page calls in to run the clock and
 * audio a block at a time and to draw a frame; every line the screen draws goes out through js_line. Stand-ins for
 * what the OP-1 does itself: six voices round robin, an instant-attack / long-release envelope, a soft clip. */
#include "op1_plugin.h"
__attribute__((import_name("js_line"))) void js_line(float x1, float y1, float x2, float y2, float w, int rgb);
__attribute__((import_name("js_sin"))) double js_sin(double);
double sin(double x) { return js_sin(x); }
long lround(double x) { return (long)(x < 0 ? x - 0.5 : x + 0.5); }
void *memcpy(void *d, const void *s, unsigned long n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, unsigned long n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }
void host_mark(int readout) { (void)readout; }
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c)
{
    (void)r;
    js_line(a->x / 1024.0f, a->y / 1024.0f, b->x / 1024.0f, b->y / 1024.0f, w / 1024.0f, (c[0] << 16) | (c[1] << 8) | c[2]);
}
#include "theseus.c"
#include "theseus_screen.c"
#include "meikyu_core.h"

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

#define SR 44100.0f
static int16_t params[8] = { 9000, 14000, 16000, 8000, 0, 4000, 0, 16000 };
static float bpm = 116.0f;
typedef struct { int on, id, stage; float lvl; uint32_t inc; } voice_t;
static voice_t vs[6];
static int next_v, next_id;
static uint32_t pad_inc(int note) { float hz = 440.0f; int d = note - 69;   /* 2^(d/12) without libm */
    static const float semi[12] = { 1.0f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f, 1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f };
    while (d < 0) { d += 12; hz *= 0.5f; } while (d >= 12) { d -= 12; hz *= 2.0f; }
    hz *= semi[d];
    return (uint32_t)(hz / SR * 4294967296.0f); }
static int voice_on(int note, int vel)
{
    int v = next_v; next_v = (next_v + 1) % 6;
    vs[v].on = 1; vs[v].id = next_id; vs[v].stage = 1; vs[v].lvl = 0; vs[v].inc = pad_inc(note);
    host_voice_vel[v] = (int16_t)(vel << 8);
    th_note_on(host_voices + v * 0x24, params);
    return next_id++ & 0x7fff;
}
int mz_out_on(int pos, int note, int key, int vel) { (void)pos; (void)key; return voice_on(note, vel); }
void mz_out_off(int id) { for (int v = 0; v < 6; v++) if (vs[v].on && vs[v].id == id && vs[v].stage < 3) vs[v].stage = 3; }
int mz_out_param(int knob, int value) { if (knob < 0 || knob > 3) return -1; params[knob] = (int16_t)(value * 258 > 32767 ? 32767 : value * 258); return 0; }
int mz_music(int *root, uint16_t *mask) { (void)root; (void)mask; return 0; }
void mz_changed(void) {}
int mz_voice(void) { return MZ_VOICE_THESEUS; }

/* ---- exports ---- */
static float out_buf[4096];
float *audio_buf(void) { return out_buf; }
void init(void) { ks_init_host(); th_screen_init(0); mz_bank(0); mz_bank(1); }
void set_param(int i, int v) { if (i >= 0 && i < 8 && i != 4) params[i] = (int16_t)(v < 0 ? 0 : v > 32767 ? 32767 : v); }
int get_param(int i) { return i >= 0 && i < 8 ? params[i] : 0; }
void set_bpm(int b) { bpm = (float)(b < 40 ? 40 : b > 240 ? 240 : b); }
/* run n blocks of 128 samples: clock meikyu, render the voices, mix into out_buf */
void run(int n)
{
    if (n > 32) n = 32;
    int16_t o[128];
    for (int bl = 0; bl < n; bl++) {
        mz_advance(0, (uint64_t)(bpm / 60.0f * 24.0f * 128.0f / SR * 65536.0f));
        float *mix = out_buf + bl * 128;
        for (int i = 0; i < 128; i++) mix[i] = 0;
        for (int v = 0; v < 6; v++) {
            if (!vs[v].on) continue;
            for (int i = 0; i < 128; i++) host_pitch_inc[i] = (int32_t)vs[v].inc;
            th_render(host_voices + v * 0x24, o, params);
            voice_t *x = &vs[v];
            for (int i = 0; i < 128; i++) {
                if (x->stage == 1) { x->lvl += 1.0f / 44.1f; if (x->lvl >= 1) { x->lvl = 1; x->stage = 2; } }
                else if (x->stage == 3) { x->lvl -= x->lvl / (1.2f * SR / 5); if (x->lvl < 1e-4f) { x->on = 0; x->lvl = 0; } }
                mix[i] += o[i] * x->lvl;
            }
        }
        for (int i = 0; i < 128; i++) {                 /* soft clip for the speakers, not part of the plugin */
            float s = mix[i] * (1.5f / 32768.0f), a = s < 0 ? -s : s;
            mix[i] = s / (1.0f + 0.28f * a * a) * 0.85f;
        }
    }
}
void frame(int dt, int page) { int16_t kn[4] = { params[0], params[1], params[2], params[3] }, w2[3] = { params[5], params[6], params[7] };
    th_screen_place(kn, w2, page, dt); th_screen_draw(0); }
/* transport and pattern (the synth bank) */
static mz_bank_t *B(void) { return mz_bank(0); }
static mz_pat_t *P(void) { return &B()->pats[B()->pat & 3]; }
void play(int root) { mz_bank_t *b = B(); if (b->running && !b->stop_pend) return; mz_key_down(0, root, 0, 1); mz_key_up(0, 0, 1); }   /* Shift + key: HOLD */
void stop(void) { mz_bank_t *b = B(); b->latched = 0; if (b->running) b->stop_pend = 1; }
int running(void) { return B()->running; }
void set_root(int note) { mz_bank_t *b = B(); b->root = (uint8_t)note; }
void set_pat(int field, int v)                          /* 0 corrupt, 1 length, 2 density, 3 range, 4 lock */
{
    mz_pat_t *q = P();
    if (field == 0) q->corrupt = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    else if (field == 1) q->length = (uint8_t)(v < 2 ? 2 : v > 16 ? 16 : v);
    else if (field == 2) q->density = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    else if (field == 3) q->range = (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v);
    else if (field == 4) q->lock = (uint8_t)(v != 0);
}
void recast(void) { mz_press(0, 1, 0); }
void flip(void) { mz_press(0, 0, 0); }
int step(void) { return B()->running ? B()->step : -1; }
int step_pad(int i) { return mz_step_pad(B(), i); }
int step_plays(int i) { return mz_step_plays(P(), i); }
int step_accent(int i) { return mz_step_accent(P(), i); }
int step_rewrote(int i) { return (B()->rewrote >> i) & 1; }
int pat_len(void) { return P()->length; }
void pad_hit(int pad, int vel) { voice_on(B()->root + mz_pad_semis[pad & 7], vel); }
