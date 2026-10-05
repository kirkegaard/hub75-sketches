#include "hub75.h"

/* One rule: rectangular frames every 8 pixels, the set
   stepped outward by one pixel each frame. */

static uint16_t bg;
static uint16_t fg;

void setup(void) {
  bg = color(0, 0, 0);
  fg = color(255, 255, 255);
}

void draw(void) {
  int phase = (int)(frameCount % 8);
  int limit = width < height ? width : height;
  int inset;

  background(bg);
  stroke(fg);
  noFill();
  for (inset = phase; inset < limit / 2; inset += 8) {
    rect(inset, inset, width - 2 * inset, height - 2 * inset);
  }
}
