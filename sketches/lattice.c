#include "hub75.h"

/* Parallel 45-degree lines crossing the panel from right to left.
   Adjacent positions crossfade for subpixel-smooth motion. */

static uint16_t bg;

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  const int frames_per_pixel = 2;
  const int line_spacing = 24;

  unsigned long tick = frameCount - 1;
  int position = width + height - 2 -
                 (int)((tick / frames_per_pixel) % (unsigned)line_spacing);
  int phase = (int)(tick % frames_per_pixel);
  int incoming_level = phase * 255 / frames_per_pixel;
  int outgoing_level = 255 - incoming_level;
  int x;

  background(bg);
  stroke(color((uint8_t)outgoing_level, (uint8_t)outgoing_level,
               (uint8_t)outgoing_level));

  for (x = position; x >= 0; x -= line_spacing) {
    line(x, 0, x - height + 1, height - 1);
  }

  stroke(color((uint8_t)incoming_level, (uint8_t)incoming_level,
               (uint8_t)incoming_level));

  for (x = position + line_spacing - 1; x >= 0; x -= line_spacing) {
    line(x, 0, x - height + 1, height - 1);
  }
}
