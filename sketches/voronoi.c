#include "graphics.h"
#include "hub75.h"

#include <math.h>

/* One rule: every pixel belongs to its nearest seed, and the gray is
   the gap between the nearest seed and the second nearest. On a cell
   boundary that gap is zero, so the walls come out dark while each cell
   interior shades by its own seed. The seeds drift on detuned sine
   pairs, so the cells slide and reshape instead of jumping. */

#define SEEDS_MAX 16

void setup(void) {}

void draw(void) {
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float t = (float)frameCount * 0.008f;
  float ax = (float)width * 0.34f;
  float ay = (float)height * 0.34f;
  float sx[SEEDS_MAX];
  float sy[SEEDS_MAX];
  uint8_t tone[SEEDS_MAX];
  int count = width * height / 700;
  int i;
  int x;
  int y;
  uint16_t *fb = hub75_pixels();

  if (!fb) {
    return;
  }

  if (count < 6) {
    count = 6;
  }

  if (count > SEEDS_MAX) {
    count = SEEDS_MAX;
  }

  for (i = 0; i < count; i++) {
    float fi = (float)i;
    float yo = sinf(t + fi + 0.1f * fi * fi);
    float xo = cosf(t + fi + 0.0046875f * fi * yo);
    int h = (i * 2654435761u) >> 24;

    sx[i] = cx + xo * ax;
    sy[i] = cy + yo * ay;
    tone[i] = (uint8_t)(120 + (h & 0x7F));
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float d1 = 1.0e9f;
      float d2 = 1.0e9f;
      int near = 0;

      for (i = 0; i < count; i++) {
        float dx = (float)x - sx[i];
        float dy = (float)y - sy[i];
        float d = dx * dx + dy * dy;

        if (d < d1) {
          d2 = d1;
          d1 = d;
          near = i;
        } else if (d < d2) {
          d2 = d;
        }
      }
      {
        float edge = sqrtf(d2) - sqrtf(d1);
        float ramp = edge < 2.0f ? edge * 0.5f : 1.0f;
        uint8_t g = (uint8_t)(ramp * (float)tone[near]);

        fb[y * width + x] = color(g, g, g);
      }
    }
  }
}
