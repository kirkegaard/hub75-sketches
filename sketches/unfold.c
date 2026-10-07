#include "hub75.h"

#include <math.h>

/* One rule: a centered horizontal line unfolds into a grid of dots,
   holds for a moment, then folds back into the line. Each dot only
   ever moves horizontally or vertically. The line slot of a dot and
   its grid cell are scrambled, and the dots are staggered, so the
   panel settles into the grid instead of snapping there in step. */

static uint16_t bg;
static uint16_t fg;

enum { EXPAND_FRAMES = 30, HOLD_FRAMES = 24, COLLAPSE_FRAMES = 30 };
enum { CYCLE_FRAMES = EXPAND_FRAMES + HOLD_FRAMES + COLLAPSE_FRAMES };

/* Fraction of the travel window a single dot spends moving. The rest
   is its wait before it starts, spread out by its hash. */
static const float motion = 0.55f;

static float ease(float t) { return t * t * (3.0f - 2.0f * t); }

static float clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static int gcd_i(int a, int b) {
  while (b) {
    int t = a % b;
    a = b;
    b = t;
  }
  return a;
}

/* A bijection on [0, count): dot i travels to cell (i * k) % count for
   some k coprime to count, so every cell is still used exactly once. */
static int scramble(int i, int count) {
  int k = count / 3 + 1;

  if (k < 1) {
    k = 1;
  }
  while (gcd_i(k, count) != 1) {
    k++;
  }
  return (i * k) % count;
}

/* Cheap integer hash to [0, 1) for a per-dot start offset. */
static float hashf(int i) {
  unsigned x = (unsigned)i * 2654435761u + 0x9E3779B9u;

  x ^= x >> 15;
  x *= 2246822519u;
  x ^= x >> 13;
  return (float)(x & 0xFFFFFFu) / (float)0x1000000u;
}

void setup(void) {
  bg = color(0, 0, 0);
  fg = color(255, 255, 255);
}

/* The dot's own 0..1 progress, delayed by its hash so the field does
   not move as one block. */
static float progress(int tick, int i) {
  float start = hashf(i);
  float span;
  float local;

  if (tick < EXPAND_FRAMES) {
    span = (float)EXPAND_FRAMES * motion;
    local = ((float)tick - start * ((float)EXPAND_FRAMES - span)) / span;
    return ease(clamp01(local));
  }
  if (tick < EXPAND_FRAMES + HOLD_FRAMES) {
    return 1.0f;
  }
  span = (float)COLLAPSE_FRAMES * motion;
  local = ((float)(tick - EXPAND_FRAMES - HOLD_FRAMES) -
           (1.0f - hashf(i + 7919)) * ((float)COLLAPSE_FRAMES - span)) /
          span;
  return 1.0f - ease(clamp01(local));
}

void draw(void) {
  int tick = (int)((frameCount - 1) % CYCLE_FRAMES);
  int cols = width / 8;
  int rows = height / 8;
  int pad = 4;
  int x0, x1, y0, y1;
  int count;
  int row;
  int col;

  if (cols < 1) {
    cols = 1;
  }

  if (rows < 1) {
    rows = 1;
  }

  if (pad * 2 >= width || pad * 2 >= height) {
    pad = 0;
  }

  x0 = pad;
  x1 = width - 1 - pad;
  y0 = pad;
  y1 = height - 1 - pad;
  count = cols * rows;

  background(bg);
  stroke(fg);

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      int i = row * cols + col;
      int cell = scramble(i, count);
      int cell_col = cell % cols;
      int cell_row = cell / cols;
      float p = progress(tick, i);
      float line_x = count > 1 ? (float)x0 + (float)i * (float)(x1 - x0) /
                                                 (float)(count - 1)
                               : (float)(x0 + x1) * 0.5f;
      float line_y = (float)(y0 + y1) * 0.5f;
      float grid_x = cols > 1 ? (float)x0 + (float)cell_col * (float)(x1 - x0) /
                                                (float)(cols - 1)
                              : (float)(x0 + x1) * 0.5f;
      float grid_y = rows > 1 ? (float)y0 + (float)cell_row * (float)(y1 - y0) /
                                                (float)(rows - 1)
                              : (float)(y0 + y1) * 0.5f;

      /* Two axis-aligned halves: drop into the row, then slide into
         the column. A dot is never on both axes at once. */
      float drop = p < 0.5f ? p * 2.0f : 1.0f;
      float slide = p < 0.5f ? 0.0f : (p - 0.5f) * 2.0f;
      int x = (int)lroundf(line_x + (grid_x - line_x) * slide);
      int y = (int)lroundf(line_y + (grid_y - line_y) * drop);

      point(x, y);
    }
  }
}
