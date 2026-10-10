#include "graphics.h"
#include "hub75.h"
#include "util.h"

#include <math.h>

/* One rule: fractal value noise used as opacity. A pixel samples a
   lattice of hashed random values, smooth-steps between them, and sums
   a few octaves at half the amplitude and double the frequency. That
   value is the alpha of a white veil over a dim backdrop, so the
   troughs go fully transparent and the peaks burn solid white. The
   field drifts, its time axis advances, and the veil's overall opacity
   breathes, so the layer is always changing. The backdrop is a second,
   much coarser noise so the transparency has something to reveal. */

#define OCTAVES 4
#define BACK_OCTAVES 2

/* Tuning knobs. LEVEL is the transparency cut: fbm values below it are
   clear, and it slides the whole veil darker or brighter. WHITE pushes
   the veil toward solid white after that: raise it for more white, 1.0
   lights only the very tallest peaks. VEIL_MIN is the dimmest the
   overall opacity breathes down to; 1.0 holds it steady. */
static const float LEVEL = 0.40f;
static const float WHITE = 1.25f;
static const float VEIL_MIN = 0.85f;

/* Hashed lattice value in [0, 1). */
static float hash3(int x, int y, int z) {
  unsigned h = hash_combine3(x, y, z);

  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return hash_unit((uint32_t)h);
}

static float hash2(int x, int y) {
  unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;

  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return hash_unit((uint32_t)h);
}

/* The backdrop only drifts, so it uses cheaper 2D noise. */
static float noise2(float x, float y) {
  int xi = (int)floorf(x);
  int yi = (int)floorf(y);
  float fx = x - (float)xi;
  float fy = y - (float)yi;
  float ux = smoothstep(fx);
  float uy = smoothstep(fy);
  float a = hash2(xi, yi) + (hash2(xi + 1, yi) - hash2(xi, yi)) * ux;
  float b =
      hash2(xi, yi + 1) + (hash2(xi + 1, yi + 1) - hash2(xi, yi + 1)) * ux;

  return lerp(a, b, uy);
}

static float fbm2(float x, float y, int octaves) {
  float sum = 0.0f;
  float amp = 0.5f;
  float norm = 0.0f;
  float freq = 1.0f;
  int o;

  for (o = 0; o < octaves; o++) {
    sum += amp * noise2(x * freq, y * freq);
    norm += amp;
    amp *= 0.5f;
    freq *= 2.0f;
  }
  return sum / norm;
}

static float noise3(float x, float y, float z) {
  int xi = (int)floorf(x);
  int yi = (int)floorf(y);
  int zi = (int)floorf(z);
  float fx = x - (float)xi;
  float fy = y - (float)yi;
  float fz = z - (float)zi;
  float ux = smoothstep(fx);
  float uy = smoothstep(fy);
  float uz = smoothstep(fz);
  float c000 = hash3(xi, yi, zi);
  float c100 = hash3(xi + 1, yi, zi);
  float c010 = hash3(xi, yi + 1, zi);
  float c110 = hash3(xi + 1, yi + 1, zi);
  float c001 = hash3(xi, yi, zi + 1);
  float c101 = hash3(xi + 1, yi, zi + 1);
  float c011 = hash3(xi, yi + 1, zi + 1);
  float c111 = hash3(xi + 1, yi + 1, zi + 1);
  float x00 = c000 + (c100 - c000) * ux;
  float x10 = c010 + (c110 - c010) * ux;
  float x01 = c001 + (c101 - c001) * ux;
  float x11 = c011 + (c111 - c011) * ux;
  float y0 = x00 + (x10 - x00) * uy;
  float y1 = x01 + (x11 - x01) * uy;

  return y0 + (y1 - y0) * uz;
}

static float fbm(float x, float y, float z, int octaves) {
  float sum = 0.0f;
  float amp = 0.5f;
  float norm = 0.0f;
  float freq = 1.0f;
  int o;

  for (o = 0; o < octaves; o++) {
    sum += amp * noise3(x * freq, y * freq, z * freq);
    norm += amp;
    amp *= 0.5f;
    freq *= 2.0f;
  }
  return sum / norm;
}

void setup(void) {}

void draw(void) {
  float short_side = (float)(width < height ? width : height);
  float over_scale = 6.0f / short_side;
  float back_scale = 2.2f / short_side;
  float t = (float)frameCount * 0.004f;
  float drift = (float)frameCount * 0.003f;
  float veil = VEIL_MIN + (1.0f - VEIL_MIN) *
                              (0.5f + 0.5f * sinf((float)frameCount * 0.005f));
  uint16_t *fb = hub75_pixels();
  int x;
  int y;

  if (!fb) {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float bx = (float)x * back_scale + drift * 0.4f;
      float by = (float)y * back_scale;
      float back = fbm2(bx, by, BACK_OCTAVES);
      float ox = (float)x * over_scale + drift;
      float oy = (float)y * over_scale;
      float a = fbm(ox, oy, t, OCTAVES);
      uint8_t bg;
      float g;

      back = smoothstep(back);
      bg = (uint8_t)(22.0f + 66.0f * back);

      a = smoothstep(a);
      a = (a - LEVEL) / (1.0f - LEVEL);
      a *= WHITE * veil;
      if (a < 0.0f) {
        a = 0.0f;
      }
      if (a > 1.0f) {
        a = 1.0f;
      }

      g = (float)bg + (255.0f - (float)bg) * a;
      {
        uint8_t gv = (uint8_t)g;

        fb[y * width + x] = color(gv, gv, gv);
      }
    }
  }
}
