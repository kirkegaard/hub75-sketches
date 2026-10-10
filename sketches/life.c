#include "hub75.h"
#include "util.h"

/* One rule: Conway's Game of Life on a coarse grid, wrapped at the
   edges so gliders circle the panel forever. A live cell burns white
   and a cell that just died keeps a dim afterglow that fades out, so
   the field reads as trails rather than a hard blink. When the board
   empties or freezes it is re-seeded from the hash. */

#define MAX_CELLS 4096

static uint8_t cur[MAX_CELLS];
static uint8_t next[MAX_CELLS];
static uint8_t fade[MAX_CELLS];

static int cols;
static int rows;
static int cells;
static int cell_size;
static int margin_x;
static int margin_y;

static int last_pop = -1;
static int still;

static uint32_t seed = 1;

static void seed_board(void) {
  int i;

  seed += 2654435761u;
  for (i = 0; i < cells; i++) {
    cur[i] = (rng_next(&seed) % 100u) < 32u ? 1 : 0;
    fade[i] = 0;
  }
  last_pop = -1;
  still = 0;
}

void setup(void) {
  int short_side = width < height ? width : height;

  cell_size = short_side / 16;
  if (cell_size < 2) {
    cell_size = 2;
  }
  if (cell_size > 6) {
    cell_size = 6;
  }
  cols = width / cell_size;
  rows = height / cell_size;
  if (cols < 3) {
    cols = 3;
  }
  if (rows < 3) {
    rows = 3;
  }
  cells = cols * rows;
  if (cells > MAX_CELLS) {
    cells = MAX_CELLS;
  }
  margin_x = (width - cols * cell_size) / 2;
  margin_y = (height - rows * cell_size) / 2;
  seed_board();
}

static int neighbors(int c, int r) {
  int n = 0;
  int dr;
  int dc;

  for (dr = -1; dr <= 1; dr++) {
    for (dc = -1; dc <= 1; dc++) {
      int rr;
      int cc;

      if (dr == 0 && dc == 0) {
        continue;
      }
      rr = (r + dr + rows) % rows;
      cc = (c + dc + cols) % cols;
      n += cur[rr * cols + cc] ? 1 : 0;
    }
  }
  return n;
}

void draw(void) {
  int pop = 0;
  int i;
  int r;
  int c;

  background(color(0, 0, 0));
  noStroke();

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
      int n = neighbors(c, r);
      int alive = cur[r * cols + c];
      int live = alive ? (n == 2 || n == 3) : (n == 3);
      int x = margin_x + c * cell_size;
      int y = margin_y + r * cell_size;

      next[r * cols + c] = (uint8_t)live;
      pop += live;

      if (live) {
        fade[r * cols + c] = 0;
        fill(color(255, 255, 255));
        rect(x, y, cell_size - 1, cell_size - 1);
      } else if (fade[r * cols + c] > 0) {
        uint8_t g = fade[r * cols + c] * 45u;

        fill(color(g, g, g));
        rect(x, y, cell_size - 1, cell_size - 1);
        fade[r * cols + c]--;
      } else if (alive) {
        fade[r * cols + c] = 5;
      }
    }
  }

  if (pop == last_pop) {
    still++;
  } else {
    still = 0;
    last_pop = pop;
  }
  if (pop == 0 || still > 180) {
    seed_board();
    return;
  }
  for (i = 0; i < cells; i++) {
    cur[i] = next[i];
  }
}
