#include "hub75.h"

#include <math.h>
#include <stdbool.h>

/* Pixels in a horizontal line, inset from the edges by a padding.
   Each column drifts slowly up and down around the center line, with
   its phase offset by its x position. Even columns ride a cosine and
   move up; odd columns ride a sine and move down.

   An envelope eases the whole line off the flat position, lets it
   animate, then settles it back on the line and holds for a moment
   before it starts again. */

static uint16_t bg;
static uint16_t fg_even;
static uint16_t fg_odd;

#define PAD 8
#define SPEED 0.04f
#define X_OFFSET 0.2f

#define PERIOD 320
#define HOLD 60
#define ACTIVE (PERIOD - HOLD)

void setup(void) {
  bg = color(0, 0, 0);
  fg_even = color(255, 255, 255);
  fg_odd = color(120, 120, 120);
}

void draw(void) {
  int cy = height / 2;
  int t = (int)(frameCount % PERIOD);
  float env;
  int x;

  if (t >= ACTIVE) {
    env = 0.f;
  } else {
    env = sinf(3.14159265f * (float)t / (float)ACTIVE);
  }

  background(bg);
  strokeWeight(1);

  for (x = PAD; x < width - PAD; x++) {
    bool odd = x % 2 == 1;
    float phase = (float)frameCount * SPEED + (float)x * X_OFFSET;
    float wave = odd ? sinf(phase) : cosf(phase);
    int dy = (int)lroundf(wave * env * (float)PAD);

    stroke(odd ? fg_odd : fg_even);
    point(x, odd ? cy + dy : cy - dy);
  }
}
