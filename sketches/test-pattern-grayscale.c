#include "graphics.h"
#include "hub75.h"
#include "util.h"

#include <stdbool.h>

static uint16_t bg;

void setup(void) {
  bg = color(0, 0, 0);

  background(bg);
}

void draw(void) {
  uint16_t *fb = hub75_pixels();

  if (!fb) {
    return;
  }

  background(bg);

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      float t = 0.5f * ((float)x / (float)width +
                        (float)y / (float)height);
      int g = (int)((1.0f - t) * 255.0f + 0.5f);

      fb[y * width + x] = gray565(g, x, y);
    }
  }
}
