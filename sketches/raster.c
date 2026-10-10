#include "hub75.h"
#include "util.h"

#include <math.h>

/* Copper bars, the Amiga raster-line kind. Each bar is a stack of
   full-width lines, dim at the edge and white on the center line.
   The bar rides two sines, so the lines drift and cross. */

#define BARS 4

static const uint8_t kHue[BARS][3] = {
    {220, 36, 28},
    {230, 156, 24},
    {28, 196, 72},
    {36, 84, 230},
};

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
      int ramp = kRamp[dist];
      int r;
      int g;
      int bl;

      if (y < 0 || y >= height || dist > 4) {
        continue;
      }
      if (dist == 0) {
        r = 255;
        g = 255;
        bl = 255;
      } else {
        r = kHue[b][0] * ramp / 255;
        g = kHue[b][1] * ramp / 255;
        bl = kHue[b][2] * ramp / 255;
      }
      fill(color((uint8_t)clamp_byte(r), (uint8_t)clamp_byte(g),
                 (uint8_t)clamp_byte(bl)));
      rect(0, y, width, 1);
    }
  }
}
