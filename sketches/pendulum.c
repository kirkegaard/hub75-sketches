#include "hub75.h"

#include <math.h>

/* One rule: one dot per row, each swinging with a whole number of
   cycles that differs by one from its neighbour. They start together,
   fan out into a traveling wave as they fall out of step, then snap
   back into line when a common period comes around. The rows are shaded
   from dark to bright so the wave reads in depth. */

static const float TAU = 6.2831853f;

void setup(void) {}

void draw(void) {
  int rows = height / 4;
  float cx = (float)width * 0.5f;
  float amp = (float)width * 0.38f;
  float period = 260.0f;
  float w = TAU / period;
  int r;

  if (rows < 2) {
    rows = 2;
  }

  background(color(0, 0, 0));
  strokeWeight(2);

  for (r = 0; r < rows; r++) {
    float x = cx + amp * sinf(w * (10.0f + (float)r) * (float)frameCount);
    float y = ((float)r + 0.5f) * (float)height / (float)rows;
    float level = (float)r / (float)(rows - 1);
    uint8_t g = (uint8_t)(90.0f + 165.0f * level);

    stroke(color(g, g, g));
    point((int)lroundf(x), (int)lroundf(y));
  }
}
