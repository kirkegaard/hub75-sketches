#include "hub75.h"
#include "util.h"

#include <math.h>

/* One rule: a grid of square tiles, each carrying two quarter-circle
   arcs at opposite corners, in one of two rotations. Neighbouring arcs
   meet at the edge midpoints, so the tiles link into unbroken trails
   that wander the panel. Every tile re-rolls its rotation on its own
   slow clock, so the maze keeps rearranging itself. */

static void arc(int cx, int cy, float r, float a0, float a1) {
  int segments = 7;
  int k;
  float px = (float)cx + r * cosf(a0);
  float py = (float)cy + r * sinf(a0);

  for (k = 1; k <= segments; k++) {
    float a = a0 + (a1 - a0) * (float)k / (float)segments;
    float x = (float)cx + r * cosf(a);
    float y = (float)cy + r * sinf(a);

    line((int)lroundf(px), (int)lroundf(py), (int)lroundf(x), (int)lroundf(y));
    px = x;
    py = y;
  }
}

/* Stable hash of a tile and an epoch. */
static unsigned tile_hash(int tx, int ty, int epoch) {
  unsigned h = hash_combine3(tx, ty, epoch);

  h ^= h >> 13;
  h *= 2654435761u;
  h ^= h >> 15;
  return h;
}

void setup(void) {}

void draw(void) {
  int tile = (width < height ? width : height) / 8;
  int tx;
  int ty;

  if (tile < 6) {
    tile = 6;
  }

  background(color(0, 0, 0));
  strokeWeight(1);
  noFill();

  for (ty = 0; ty * tile <= height; ty++) {
    for (tx = 0; tx * tile <= width; tx++) {
      unsigned base = tile_hash(tx, ty, 0);
      int epoch = (int)(((unsigned long)frameCount + base % 60u) / 70u);
      unsigned h = tile_hash(tx, ty, epoch);
      int x0 = tx * tile;
      int y0 = ty * tile;
      float half = (float)tile * 0.5f;
      uint8_t g = (uint8_t)(120 + (h >> 8 & 0x7Fu));

      stroke(color(g, g, g));
      if (h & 1u) {
        arc(x0 + tile, y0, half, 1.5707963f, 3.1415927f);
        arc(x0, y0 + tile, half, 4.7123890f, 6.2831853f);
      } else {
        arc(x0, y0, half, 0.0f, 1.5707963f);
        arc(x0 + tile, y0 + tile, half, 3.1415927f, 4.7123890f);
      }
    }
  }
}
