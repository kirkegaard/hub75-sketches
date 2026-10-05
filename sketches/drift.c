#include "hub75.h"

/* One rule: vertical lines 16 columns apart, the whole comb
   steps one column to the right each frame. */

static uint16_t bg;
static uint16_t fg;

void setup(void) {
  bg = color(0, 0, 0);
  fg = color(255, 255, 255);
}

void draw(void) {
  int phase = (int)(frameCount % 16);
  int x;

  background(bg);
  stroke(fg);
  for (x = phase; x < width; x += 16) {
    line(x, 0, x, height - 1);
  }
}
