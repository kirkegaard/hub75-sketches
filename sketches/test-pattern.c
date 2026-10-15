#include "graphics.h"
#include "hub75.h"

#include <math.h>
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
      fb[y * width + x] = color(
          (uint8_t)(sinf(3.14159265f * (float)x / (float)width) * 127.5f +
                    127.5f),
          (uint8_t)(cosf(3.14159265f * (float)y / (float)height) * 127.5f +
                    127.5f),
          (uint8_t)(sinf(3.14159265f * (float)y / (float)height) * 127.5f +
                    127.5f));
    }
  }
}
