#ifndef HUB75_UTIL_DITHER_H
#define HUB75_UTIL_DITHER_H

#include <stdint.h>

/* Ordered dithering for the 5/6/5 framebuffer. The panel shows far fewer
   levels than 8 bits, so a smooth ramp quantized straight to RGB565
   bands. These helpers pick between the two nearest palette levels per
   pixel, spread over a 4x4 tile, so the eye averages the lost bits back. */

/* The 4x4 ordered (Bayer) thresholds, 0..15, arranged so neighbors never
   share a step. */
static inline int dither_threshold4(int x, int y) {
  static const uint8_t bayer[16] = {0, 8,  2,  10, 12, 4, 14, 6,
                                    3, 11, 1,  9,  15, 7, 13, 5};
  return bayer[((y & 3) << 2) | (x & 3)];
}

/* Dither one 8-bit channel to `levels` evenly spaced levels. threshold
   runs 0..thresholds-1, so a 4x4 Bayer tile passes thresholds=16 and an
   8x8 tile passes thresholds=64. Returns the chosen level. */
static inline int dither_levels_ex(int v, int levels, int threshold,
                                   int thresholds) {
  int max;
  int s;
  int base;
  int rem;

  if (levels < 2) {
    levels = 2;
  }
  if (thresholds < 1) {
    thresholds = 1;
  }
  if (v < 0) {
    v = 0;
  } else if (v > 255) {
    v = 255;
  }

  /* Position on the 0..levels-1 scale, scaled by 255 so the fraction is
     exact: the levels are not a whole-number step of 8 in 8-bit terms. */
  max = levels - 1;
  s = v * max;
  base = s / 255;
  rem = s % 255;

  /* Round up when the fraction beats this pixel's threshold. */
  if ((long)rem * thresholds > (long)threshold * 255 + 127) {
    base++;
  }
  if (base > max) {
    base = max;
  }
  return base;
}

/* The common 4x4 case. */
static inline int dither_levels(int v, int levels, int threshold) {
  return dither_levels_ex(v, levels, threshold, 16);
}

/* Dither one 8-bit channel to a palette of 2^bits levels. Returns the
   chosen level. bits is the panel's channel width (red/blue 5, green 6). */
static inline int dither_channel(int v, int bits, int threshold) {
  return dither_levels(v, 1 << bits, threshold);
}

/* 8-bit gray to RGB565 with the lost bits dithered into the 4x4 tile. */
static inline uint16_t gray565(int v, int x, int y) {
  int t = dither_threshold4(x, y);
  int r5 = dither_channel(v, 5, t);
  int g6 = dither_channel(v, 6, t);
  int b5 = r5;

  return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

#endif
