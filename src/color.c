#include "color.h"

#include <math.h>

/* Per-channel gamma tables, in the panel's native level counts. */
static uint8_t lut_r[32];
static uint8_t lut_g[64];
static uint8_t lut_b[32];

/* Saturation in 8.8 fixed point: 100% is 256. */
static int sat_q8 = 256;

/* Set once so the common identity case (100%, gamma 1.0) is a no-op. */
static int correcting = 0;

static uint8_t gamma_level(int level, int max, float gamma) {
  float v;

  if (gamma == 1.0f) {
    return (uint8_t)level;
  }
  v = powf((float)level / (float)max, gamma) * (float)max;
  return (uint8_t)(v + 0.5f);
}

static int clamp_byte(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 255) {
    return 255;
  }
  return v;
}

void hub75_color_init(int saturation_pct, float gamma) {
  int i;

  if (saturation_pct < 0) {
    saturation_pct = 0;
  } else if (saturation_pct > 200) {
    saturation_pct = 200;
  }
  if (!(gamma > 0.0f)) {
    gamma = 1.0f;
  }

  sat_q8 = (saturation_pct * 256 + 50) / 100;

  for (i = 0; i < 32; i++) {
    lut_r[i] = gamma_level(i, 31, gamma);
    lut_b[i] = gamma_level(i, 31, gamma);
  }
  for (i = 0; i < 64; i++) {
    lut_g[i] = gamma_level(i, 63, gamma);
  }

  correcting = !(saturation_pct == 100 && gamma == 1.0f);
}

uint16_t hub75_correct565(uint16_t c) {
  int r5;
  int g6;
  int b5;
  int r;
  int g;
  int b;
  int luma;

  if (!correcting) {
    return c;
  }

  r5 = (c >> 11) & 0x1F;
  g6 = (c >> 5) & 0x3F;
  b5 = c & 0x1F;

  /* Expand to 8-bit the same way the simulator does. */
  r = (r5 << 3) | (r5 >> 2);
  g = (g6 << 2) | (g6 >> 4);
  b = (b5 << 3) | (b5 >> 2);

  /* Pull each channel toward its luma (Rec. 601) by the saturation. */
  luma = (77 * r + 150 * g + 29 * b) / 256;
  r = clamp_byte(luma + (r - luma) * sat_q8 / 256);
  g = clamp_byte(luma + (g - luma) * sat_q8 / 256);
  b = clamp_byte(luma + (b - luma) * sat_q8 / 256);

  r5 = lut_r[r >> 3];
  g6 = lut_g[g >> 2];
  b5 = lut_b[b >> 3];

  return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}
