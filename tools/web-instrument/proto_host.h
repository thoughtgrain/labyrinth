/* forced include for every unit of the device prototype: the host hooks the plugin screens draw through */
#include <stdint.h>
#include "op1_plugin.h"
void op1_raster_line(void *r, const op1_pt *a, const op1_pt *b, int32_t w, const uint8_t *c);
void host_mark(int readout);
