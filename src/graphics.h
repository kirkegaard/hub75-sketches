#ifndef HUB75_GRAPHICS_H
#define HUB75_GRAPHICS_H

#include <stdint.h>

/* Allocate the RGB565 framebuffer and set width and height. */
int hub75_gfx_init(int w, int h);

/* Row-major RGB565, width * height samples. */
uint16_t *hub75_pixels(void);

#endif
