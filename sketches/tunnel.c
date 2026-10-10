#include "hub75.h"

#include <math.h>

/* One rule: the classic demo tunnel. Every pixel maps to a polar
   coordinate on the tunnel wall: the angle becomes the horizontal
   texture axis and the inverse distance from the centre becomes the
   depth axis. A grayscale checker rides that texture, spinning slowly
   while the camera flies forward. */

static uint16_t bg;

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  float t = (float)frameCount;
  /* The vanishing point drifts a little, so the tunnel sways instead of
     staring straight ahead. */
  float cx = (float)width * 0.5f + sinf(t * 0.008f) * (float)width * 0.06f;
  float cy = (float)height * 0.5f + cosf(t * 0.011f) * (float)height * 0.08f;
  float fwd = t * 0.07f;
  float spin = t * 0.003f;
  int x;
  int y;

  background(bg);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float dx = (float)x - cx;
      float dy = (float)y - cy;
      float dist = sqrtf(dx * dx + dy * dy);
      float z;
      float ang;
      float v;
      float fog;
      float tex;
      float level;
      uint8_t g;
      int checker;

      if (dist < 1.0f) {
        dist = 1.0f;
      }
      z = 48.0f / dist;
      ang = atan2f(dy, dx) + spin;
      v = z + fwd;

      checker = ((int)floorf(ang * 2.0f + 8.0f) + (int)floorf(v)) & 1;
      fog = 1.0f - z * 0.015f;
      if (fog < 0.15f) {
        fog = 0.15f;
      } else if (fog > 1.0f) {
        fog = 1.0f;
      }

      tex = checker ? 1.0f : 0.42f;
      level = tex * fog;
      g = (uint8_t)(level * 255.0f);
      stroke(color(g, g, g));
      point(x, y);
    }
  }
}
