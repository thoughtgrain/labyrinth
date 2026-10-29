/* Minimal host mock for the preview builds (the SDK snapshot ships no harness/op1_host.h): only what the
 * meikyu and toro screens and engines touch. Not part of any plugin file. */
#include <stdint.h>
extern int32_t host_pitch_inc[128];
extern int16_t host_voice_vel[6];
extern uint8_t host_voices[6 * 0x24];
#define OP1_PITCH_INC   ((const int32_t *)host_pitch_inc)
#define OP1_VOICE_VEL   ((const volatile int16_t *)host_voice_vel)
#define OP1_VOICE_BASE  ((uintptr_t)host_voices)
void op1_node_set_transform(void *n, const int32_t *m);   /* the preview records node matrices */
