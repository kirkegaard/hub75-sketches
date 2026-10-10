#include "hub75.h"
#include "util.h"

#include <math.h>
#include <stdint.h>

/* Rising squares from a 2-bitplane Amiga screen. They drift upward,
   spin, and come back at the bottom. Ink is the original palette:
   white, orange, cyan. Half the squares are filled, half are outlines.
   A square is skipped while its spin would leave the sides, same as
   the screen-edge test on that demo. The count and the size are
   scaled down so a square still reads on 64 rows. */

#define SQUARE_MAX 24

static uint32_t seed = 1;

static int square_count = 16;
static int square_size = 8;

typedef struct Square {
  float x;
  float y;
  float velocity;
  float angle;
  uint8_t color;
  int filled;
} Square;

static Square squares[SQUARE_MAX];

static const uint8_t kInk[3][3] = {
    {255, 255, 255},
    {255, 136, 0},
    {0, 255, 255},
};

static int panel_count(void) {
  int n = (width * height) / 512;

  if (n < 8) {
    return 8;
  }
  if (n > SQUARE_MAX) {
    return SQUARE_MAX;
  }
  return n;
}

static int panel_size(void) {
  int s = height / 8;

  if (s < 4) {
    s = 4;
  }
  if (s > 12) {
    s = 12;
  }
  if (s & 1) {
    s--;
  }
  if (s < 4) {
    s = 4;
  }
  return s;
}

static void init_square(Square *square) {
  float span = (float)height / 256.0f;

  square->x = rng_float(&seed, (float)square_size * 0.5f,
                        (float)width - (float)square_size * 0.5f);
  square->y = (float)height + (float)square_size;
  square->velocity = rng_float(&seed, 0.5f, 3.0f) * span;
  square->angle = rng_float(&seed, 0.0f, 6.2831853f);
  square->color = (uint8_t)((rng_next(&seed) % 3u) + 1u);
  square->filled = (rng_next(&seed) % 2u) == 0u;
}

static void paint_square(int cx, int cy, int size, float angle, int ink,
                         int filled) {
  int half = size / 2;
  int reach = (int)((float)half * 1.5f);
  float c;
  float s;
  int x1, y1, x2, y2, x3, y3, x4, y4;
  uint16_t col;
  const uint8_t *rgb;

  if (cx - reach < 0 || cx + reach >= width) {
    return;
  }
  if (ink < 1 || ink > 3) {
    return;
  }

  c = cosf(angle);
  s = sinf(angle);

  x1 = cx + (int)(-half * c + half * s);
  y1 = cy + (int)(-half * s - half * c);

  x2 = cx + (int)(half * c + half * s);
  y2 = cy + (int)(half * s - half * c);

  x3 = cx + (int)(half * c - half * s);
  y3 = cy + (int)(half * s + half * c);

  x4 = cx + (int)(-half * c - half * s);
  y4 = cy + (int)(-half * s + half * c);

  rgb = kInk[ink - 1];
  col = color(rgb[0], rgb[1], rgb[2]);

  if (filled) {
    fill(col);
    noStroke();
    triangle(x1, y1, x2, y2, x3, y3);
    triangle(x1, y1, x3, y3, x4, y4);
    return;
  }

  noFill();
  stroke(col);
  strokeWeight(1);
  line(x1, y1, x2, y2);
  line(x2, y2, x3, y3);
  line(x3, y3, x4, y4);
  line(x4, y4, x1, y1);
}

void setup(void) {
  int i;

  seed = 1;
  square_count = panel_count();
  square_size = panel_size();
  for (i = 0; i < square_count; i++) {
    init_square(&squares[i]);
  }
}

void draw(void) {
  int i;

  background(color(0, 0, 0));
  for (i = 0; i < square_count; i++) {
    squares[i].y -= squares[i].velocity;
    squares[i].angle += 0.02f;
    if (squares[i].y < (float)-square_size) {
      init_square(&squares[i]);
    }
    paint_square((int)squares[i].x, (int)squares[i].y, square_size,
                 squares[i].angle, squares[i].color, squares[i].filled);
  }
}
