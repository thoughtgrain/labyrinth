/* toro's voice and RIDER screen, as shipped */
#include "toro.c"
#include "toro_screen.c"
void toro_tu_init(void) { ks_init_host(); tr_screen_init(0); }
void toro_tu_draw(const int16_t *p, int page, int dt) { int16_t kn[4] = { p[0], p[1], p[2], p[3] }, w2[3] = { p[5], p[6], p[7] };
    tr_screen_place(kn, w2, page, dt); tr_screen_draw(0); }
const op1_engine_t *toro_tu_engine(void) { return &ENGINE_toro.engine; }
