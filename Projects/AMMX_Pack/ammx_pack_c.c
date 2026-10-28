#include "ammx_pack.h"

void pack_signed_words_c(const int16_t *in, int8_t *out, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        int16_t v = in[i];
        if (v < -128) {
            out[i] = -128;
        } else if (v > 127) {
            out[i] = 127;
        } else {
            out[i] = (int8_t)v;
        }
    }
}
