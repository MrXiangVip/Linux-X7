#ifndef HX_REG_H
#define HX_REG_H

#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
#endif

int hx_init(const char *cfg_file);
void hx_deinit(void);
void hx_cfg_input(int input_width, int input_height);
void hx_cfg_upscale(int upscale_width, int upscale_height);
int hx_pp(uint16_t *in, uint16_t *out);

#if defined (__cplusplus)
}
#endif

#endif /* HX_REG_H */
