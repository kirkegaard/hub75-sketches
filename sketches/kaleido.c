#include "hub75.h"

#include <math.h>

/* One rule: a single segment whose two ends each ride their own
   Lissajous curve, drawn once per mirror in a ring of rotational
   symmetry. Copies facing the same way are dimmer than the copies that
   cut across them, so the weave reads in gray instead of collapsing
   into one flat white star. */

static const float TAU = 6.2831853f;

void setup(void) {}

void draw(void) {
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float r = (float)(width < height ? width : height) * 0.46f;
  float t = (float)frameCount * 0.01f;
  float ax = cx + r * sinf(2.1f * t);
  float ay = cy + r * sinf(3.4f * t + 1.0f);
  float bx = cx + r * sinf(1.7f * t + 2.0f);
  float by = cy + r * cosf(2.3f * t);
  int mirrors = (width < height ? width : height) / 6;
  int i;

  if (mirrors < 4) {
    mirrors = 4;
  }
  if (mirrors > 16) {
    mirrors = 16;
  }

  background(color(0, 0, 0));
  strokeWeight(1);

  for (i = 0; i < mirrors; i++) {
    float a = TAU * (float)i / (float)mirrors;
    float c = cosf(a);
    float s = sinf(a);
    float rax = ax - cx;
    float ray = ay - cy;
    float rbx = bx - cx;
    float rby = by - cy;
    uint8_t g = (uint8_t)((i & 1) ? 90 : 200);

    stroke(color(g, g, g));
    line((int)lroundf(cx + rax * c - ray * s),
         (int)lroundf(cy + rax * s + ray * c),
         (int)lroundf(cx + rbx * c - rby * s),
         (int)lroundf(cy + rbx * s + rby * c));
  }
}
