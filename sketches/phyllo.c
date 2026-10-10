#include "hub75.h"

#include <math.h>

/* One rule: dot i sits at angle i times the golden angle and radius
   proportional to the square root of i, the arrangement a sunflower
   uses to pack seeds. The dot count swells from nothing to a full head
   and back on one slow cosine, and the whole head turns underneath, so
   the florets appear to grow and shed. */

static const float GOLDEN = 2.39996323f;

void setup(void) {}

void draw(void) {
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float max_r = 0.47f * (float)(width < height ? width : height);
  float spacing = 2.6f;
  float max_n = (max_r * max_r) / (spacing * spacing);
  float phase = (float)frameCount * 0.012f;
  float turn = (float)frameCount * 0.002f;
  float u = 0.5f - 0.5f * cosf(phase);
  int n = (int)(u * max_n);
  int i;

  background(color(0, 0, 0));
  strokeWeight(1);

  for (i = 0; i < n; i++) {
    float fi = (float)i;
    float r = spacing * sqrtf(fi);
    float a = fi * GOLDEN + turn;
    float t = r / max_r;
    uint8_t g = (uint8_t)(80.0f + 175.0f * t);

    stroke(color(g, g, g));
    point((int)lroundf(cx + r * cosf(a)), (int)lroundf(cy + r * sinf(a)));
  }
}
