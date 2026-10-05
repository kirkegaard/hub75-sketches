#include "hub75.h"

#include <math.h>

static const float TAU = 6.2831853f;

static uint16_t bg;

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  float radius = (float)(height < width ? height : width) * 0.5f - 1.5f;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float spin = (float)frameCount * 0.008f;
  float vx[3];
  float vy[3];
  int i;

  stroke(color(255, 255, 255));
  strokeWeight(1);

  if (buttonDown()) {
    noFill();
  } else if (buttonUp()) {
    fill(color(255, 255, 255));
  }

  for (i = 0; i < 3; i++) {
    float a = spin - TAU * (float)i / 3.f;
    vx[i] = cx + cosf(a) * radius;
    vy[i] = cy + sinf(a) * radius;
  }

  background(bg);
  triangle(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
}
