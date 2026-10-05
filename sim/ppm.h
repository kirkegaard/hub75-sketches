#ifndef HUB75_PPM_H
#define HUB75_PPM_H

#include <stdint.h>

/* Binary PPM (P6). Returns 0 on success. */
int ppm_write(const char *path, const uint8_t *rgb, int w, int h);

#endif
