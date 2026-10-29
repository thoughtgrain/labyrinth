/* theseus's voice and PHALANX screen, as shipped */
#include "theseus.c"
#include "theseus_screen.c"
void theseus_tu_init(void) { ks_init_host(); th_screen_init(0); }
void theseus_tu_draw(const int16_t *p, int page, int dt) { int16_t kn[4] = { p[0], p[1], p[2], p[3] }, w2[3] = { p[5], p[6], p[7] };
    th_screen_place(kn, w2, page, dt); th_screen_draw(0); }
const op1_engine_t *theseus_tu_engine(void) { return &ENGINE_theseus.engine; }
