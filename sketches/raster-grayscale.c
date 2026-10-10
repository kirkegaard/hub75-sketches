#include "hub75.h"
#include "util.h"

#include <math.h>

/* Same copper bars as raster.c, in gray. Each bar keeps the luminance
   of its color twin, so the four bars stay different shades. The center
   line is white. */

#define BARS 4

static const uint8_t kGray[BARS] = {90, 163, 131, 86};

/* Center line outward. The last step is still lit, so the edge is a line. */
static const uint8_t kRamp[5] = {255, 220, 160, 110, 70};

void setup(void) {}

void draw(void) {
  int half = 4;
  int travel;
  int b;
  float t = (float)frameCount * 0.08f;

  if (height < 24) {
    half = 1;
  } else if (height < 48) {
    half = 2;
  }

  background(color(0, 0, 0));
  noStroke();

  travel = height / 2 - 1;
  if (travel < 1) {
    travel = 1;
  }

  for (b = 0; b < BARS; b++) {
    float wave = sinf(t + (float)b * 0.85f) * 0.78f +
                 sinf(t * 0.41f + (float)b * 1.6f) * 0.22f;
    int cy = height / 2 + (int)(wave * (float)travel);
    int dy;

    for (dy = -half; dy <= half; dy++) {
      int y = cy + dy;
      int dist = dy < 0 ? -dy : dy;
      int shade;

      if (y < 0 || y >= height || dist > 4) {
        continue;
      }
      if (dist == 0) {
        shade = 255;
      } else {
        shade = kGray[b] * kRamp[dist] / 255;
      }
      shade = clamp_byte(shade);
      fill(color((uint8_t)shade, (uint8_t)shade, (uint8_t)shade));
      rect(0, y, width, 1);
    }
  }
}
