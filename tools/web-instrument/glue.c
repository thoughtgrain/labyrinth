/* Device prototype glue: one synth slot that plays toro or theseus, meikyu's synth bank clocking it, and the three
 * plugin screens, all compiled from the shipped sources to WebAssembly. The page sends encoder turns and presses,
 * keys, Shift and the mode buttons; it gets the audio a block at a time and every screen line through js_line.
 * Stand-ins for what the OP-1 does itself: six voices round robin, a simple envelope per engine, a soft clip. */
#include "proto_host.h"
#include "meikyu_core.h"
__attribute__((import_name("js_line"))) void js_line(float x1, float y1, float x2, float y2, float w, int rgb);
__attribute__((import_name("js_node"))) void js_node(int id, float a, float b, float c, float d, float e, float f);
__attribute__((import_name("js_sin"))) double js_sin(double);
double sin(double x) { return js_sin(x); }
long lround(double x) { return (long)(x < 0 ? x - 0.5 : x + 0.5); }
void *memcpy(void *d, const void *s, unsigned long n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, unsigned long n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }
void host_mark(int readout) { (void)readout; }
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c)
{ (void)r; js_line(a->x / 1024.0f, a->y / 1024.0f, b->x / 1024.0f, b->y / 1024.0f, w / 1024.0f, (c[0] << 16) | (c[1] << 8) | c[2]); }
void op1_node_set_transform(void *n, const int32_t *m)    /* toro's rig, rod and shadow nodes: 1, 2, 3 */
{ js_node((int)(uintptr_t)n, m[0] / 1024.0f, m[1] / 1024.0f, m[3] / 1024.0f, m[4] / 1024.0f, m[6] / 1024.0f, m[7] / 1024.0f); }

int32_t host_pitch_inc[128];
int16_t host_voice_vel[6];
uint8_t host_voices[6 * 0x24];

void toro_tu_init(void), theseus_tu_init(void), meikyu_tu_init(void);
void toro_tu_draw(const int16_t *p, int page, int dt), theseus_tu_draw(const int16_t *p, int page, int dt);
const op1_engine_t *toro_tu_engine(void), *theseus_tu_engine(void);
void mz_screen(void *r, int pos, int shift);

#define SR 44100.0f
enum { E_TORO, E_THESEUS };
static int16_t prm[2][8] = { { 2000, 6000, 18000, 8000, 0, 0, 0, 6000 }, { 9000, 14000, 16000, 8000, 0, 4000, 0, 16000 } };
static int eng = E_TORO;
static float bpm = 112.0f;
static const op1_engine_t *E(void) { return eng == E_TORO ? toro_tu_engine() : theseus_tu_engine(); }

/* voices with a stand-in envelope: toro gets a soft ADSR, theseus instant attack and a long release (its own decay
 * shapes each hit) */
typedef struct { int on, id, stage, note; float lvl; uint32_t inc; } voice_t;
static voice_t vs[6];
static int next_v, next_id;
static uint32_t note_inc(int note)
{
    static const float semi[12] = { 1.0f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f, 1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f };
    float hz = 440.0f; int d = note - 69;
    while (d < 0) { d += 12; hz *= 0.5f; } while (d >= 12) { d -= 12; hz *= 2.0f; }
    return (uint32_t)(hz * semi[d] / SR * 4294967296.0f);
}
static int voice_on(int note, int vel)
{
    int v = next_v; next_v = (next_v + 1) % 6;
    vs[v].on = 1; vs[v].id = next_id; vs[v].stage = 1; vs[v].lvl = 0; vs[v].inc = note_inc(note); vs[v].note = note;
    host_voice_vel[v] = (int16_t)(vel << 8);
    E()->note_on(host_voices + v * 0x24, prm[eng]);
    return next_id++ & 0x7fff;
}
static void voice_off_id(int id) { for (int v = 0; v < 6; v++) if (vs[v].on && vs[v].id == id && vs[v].stage < 3) vs[v].stage = 3; }

/* meikyu's hooks */
int mz_out_on(int pos, int note, int key, int vel) { (void)pos; (void)key; return voice_on(note, vel); }
void mz_out_off(int id) { voice_off_id(id); }
int mz_out_param(int knob, int value) { if (knob < 0 || knob > 3) return -1; prm[eng][knob] = (int16_t)(value * 258 > 32767 ? 32767 : value * 258); return 0; }
int mz_music(int *root, uint16_t *mask) { (void)root; (void)mask; return 0; }
void mz_changed(void) {}
int mz_voice(void) { return eng == E_TORO ? MZ_VOICE_TORO : MZ_VOICE_THESEUS; }

/* ---- exports ---- */
static float out_buf[4096];
float *audio_buf(void) { return out_buf; }
void init(void)
{
    toro_tu_init(); theseus_tu_init(); meikyu_tu_init();
    mz_bank(0); mz_bank(1);
    mz_pat_t *q = &mz_bank(0)->pats[0];
    q->density = 70; q->range = 9; q->corrupt = 15; q->length = 16;
}
int engine(void) { return eng; }
void set_engine(int e)                                  /* the SYNTH button: swap the slot's engine, voices cut */
{
    if (e == eng) return;
    for (int v = 0; v < 6; v++) vs[v].on = 0;
    eng = e ? E_THESEUS : E_TORO;
}
int get_param(int i) { return i >= 0 && i < 8 ? prm[eng][i] : 0; }
void set_param(int i, int v) { if (i >= 0 && i < 8 && i != 4) prm[eng][i] = (int16_t)(v < 0 ? 0 : v > 32767 ? 32767 : v); }
void set_bpm(int b) { bpm = (float)(b < 40 ? 40 : b > 240 ? 240 : b); }

/* keys: through meikyu while the sequencer is on (hold a key to play the pattern from it; Shift + key latches),
 * straight to the voice while it is off */
static int seq_on = 1, held_id[24];
void set_seq(int on) { seq_on = on; if (!on) { mz_bank_t *b = mz_bank(0); b->latched = 0; if (b->running) b->stop_pend = 1; } }
int seq(void) { return seq_on; }
void key_down(int note, int k, int shift)
{
    if (k < 0 || k > 23) return;
    if (seq_on) mz_key_down(0, note, k, shift);
    else held_id[k] = voice_on(note, 104) + 1;
}
void key_up(int k, int shift)
{
    if (k < 0 || k > 23) return;
    if (seq_on) mz_key_up(0, k, shift);
    else if (held_id[k]) { voice_off_id(held_id[k] - 1); held_id[k] = 0; }
}
void seq_turn(int knob, int delta, int shift) { mz_turn(0, knob, delta, shift); }
void seq_press(int knob, int shift) { mz_press(0, knob, shift); }
int seq_running(void) { return mz_bank(0)->running; }
void seq_stop(void) { mz_bank_t *b = mz_bank(0); b->latched = 0; if (b->running) b->stop_pend = 1; }

void run(int n)
{
    if (n > 32) n = 32;
    int16_t o[128];
    for (int bl = 0; bl < n; bl++) {
        mz_advance(0, (uint64_t)(bpm / 60.0f * 24.0f * 128.0f / SR * 65536.0f));
        float *mix = out_buf + bl * 128;
        for (int i = 0; i < 128; i++) mix[i] = 0;
        for (int v = 0; v < 6; v++) {
            voice_t *x = &vs[v];
            if (!x->on) continue;
            for (int i = 0; i < 128; i++) host_pitch_inc[i] = (int32_t)x->inc;
            E()->render(host_voices + v * 0x24, o, prm[eng]);
            float atk = eng == E_TORO ? 1.0f / (0.004f * SR) : 1.0f / 44.1f;
            float rel = eng == E_TORO ? 0.16f : 1.2f;
            for (int i = 0; i < 128; i++) {
                if (x->stage == 1) { x->lvl += atk; if (x->lvl >= 1) { x->lvl = 1; x->stage = 2; } }
                else if (x->stage == 2 && eng == E_TORO) x->lvl += (0.55f - x->lvl) / (0.22f * SR / 4);
                else if (x->stage == 3) { x->lvl -= x->lvl / (rel * SR / 5); if (x->lvl < 1e-4f) { x->on = 0; x->lvl = 0; } }
                mix[i] += o[i] * x->lvl;
            }
        }
        for (int i = 0; i < 128; i++) {                 /* soft clip for the speakers, not part of the plugins */
            float s = mix[i] * (1.3f / 32768.0f), a = s < 0 ? -s : s;
            mix[i] = s / (1.0f + 0.28f * a * a) * 0.85f;
        }
    }
}
/* view 0: the synth screen of the loaded engine; view 1: meikyu */
void frame(int view, int dt, int page, int shift)
{
    if (view == 1) mz_screen(0, 0, shift);
    else if (eng == E_TORO) toro_tu_draw(prm[eng], page, dt);
    else theseus_tu_draw(prm[eng], page, dt);
}
