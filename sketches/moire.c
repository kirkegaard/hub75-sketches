#include "graphics.h"
#include "hub75.h"

#include <math.h>

/* One rule: two sets of concentric rings, each a plain cosine of the
   distance from its own center, added together per pixel. The centers
   drift apart and back on slow sine paths, so the rings beat against
   each other into bands and whorls that no single ring contains. */

static const float TAU = 6.2831853f;

void setup(void) {}

void draw(void) {
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float t = (float)frameCount * 0.01f;
  float amp = 0.18f * (float)(width < height ? width : height);
  float c1x = cx + amp * sinf(t);
  float c1y = cy + amp * cosf(1.3f * t);
  float c2x = cx + amp * sinf(0.7f * t + 2.0f);
  float c2y = cy + amp * cosf(t);
  float k = TAU / 4.3f;
  uint16_t *fb = hub75_pixels();
  int x;
  int y;

  if (!fb) {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float d1x = (float)x - c1x;
      float d1y = (float)y - c1y;
      float d2x = (float)x - c2x;
      float d2y = (float)y - c2y;
      float v = cosf(sqrtf(d1x * d1x + d1y * d1y) * k) +
                cosf(sqrtf(d2x * d2x + d2y * d2y) * k);
      float nrm = (v + 2.0f) * 0.25f;
      uint8_t g = (uint8_t)(nrm * 255.0f);

      fb[y * width + x] = color(g, g, g);
    }
  }
}
