#include "graphics.h"
#include "hub75.h"

#include <math.h>

/* One rule: three ripples, each a sine of the distance from a moving
   source and a little weaker with distance. Every pixel sums them, so
   where two crests meet the panel burns brighter and where a crest
   meets a trough it goes dark. The sources orbit the center at three
   different rates, which keeps the pattern from repeating. */

static const float TAU = 6.2831853f;
static const int SOURCES = 3;

void setup(void) {}

void draw(void) {
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float t = (float)frameCount * 0.012f;
  float amp = 0.28f * (float)(width < height ? width : height);
  float k = TAU / 5.0f;
  float sx[SOURCES];
  float sy[SOURCES];
  uint16_t *fb = hub75_pixels();
  int i;
  int x;
  int y;

  if (!fb) {
    return;
  }

  for (i = 0; i < SOURCES; i++) {
    float fi = (float)i;
    sx[i] = cx + amp * cosf(t * (1.0f + fi * 0.37f) + fi * 2.1f);
    sy[i] = cy + amp * sinf(t * (0.8f + fi * 0.29f) + fi * 1.7f);
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float sum = 0.0f;

      for (i = 0; i < SOURCES; i++) {
        float dx = (float)x - sx[i];
        float dy = (float)y - sy[i];
        float d = sqrtf(dx * dx + dy * dy);

        sum += sinf(d * k - t * 2.0f) / (1.0f + d * 0.08f);
      }
      sum *= 0.5f;
      if (sum > 1.0f) {
        sum = 1.0f;
      }
      if (sum < -1.0f) {
        sum = -1.0f;
      }
      {
        uint8_t g = (uint8_t)(128.0f + 120.0f * sum);

        fb[y * width + x] = color(g, g, g);
      }
    }
  }
}
