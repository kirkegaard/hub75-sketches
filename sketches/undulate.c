#include "graphics.h"
#include "hub75.h"
#include "util.h"

/* One rule: a non-totalistic Generations cellular automaton in Hensel
   notation, 23-a4ai/3aiqy4n/3. A dead cell is born when its live
   neighbors form the 3a, 3i, 3q, 3y or 4n arrangement; a live cell
   survives on 2 neighbors, on 3 unless the arrangement is 3a, or on
   the 4a or 4i arrangement. Because the outcome depends on how the
   neighbors sit, not just how many, the field folds into slow waves
   instead of Life's blobs. A live cell that does not survive drops to
   a single dying state that burns amber before it clears, so every
   crest trails a warm edge the way the source clip does. The grid
   wraps at the edges and re-seeds from the hash when it empties or
   freezes. */

#define MAX_CELLS 16384

/* Golly's Generations states: 1 is alive, 2 is dying, 0 is dead. Only
   state 1 counts as a neighbor. */
#define DEAD 0
#define ALIVE 1
#define DYING 2

/* Isotropic neighborhoods, one bit per 3x3 cell. Bit 4 is the center
   and stays clear; the other bits are the eight neighbors:
     0 1 2
     3 4 5
     6 7 8  */
#define L3A 11
#define L3I 7
#define L3Q 70
#define L3Y 97
#define L4N 71
#define L4A 15
#define L4I 45

static uint8_t cur[MAX_CELLS];
static uint8_t next[MAX_CELLS];

static int cols;
static int rows;
static int cells;

static uint32_t seed = 1;
static int last_pop = -1;
static int still;

static int rotate90(int x) {
  return ((x & 0x4) << 6) | ((x & 0x20) << 2) | ((x & 0x100) >> 2) |
         ((x & 0x2) << 4) | (x & 0x10) | ((x & 0x80) >> 4) |
         ((x & 0x1) << 2) | ((x & 0x8) >> 2) | ((x & 0x40) >> 6);
}

static int flip_bits(int x) {
  return ((x & 0x07) << 6) | ((x & 0x1c0) >> 6) | (x & 0x38);
}

/* Hensel letters are isotropic: a configuration matches a letter when
   any rotation or reflection of it equals the letter's representative. */
static int matches(int cfg, int rep) {
  int y = cfg;
  int i;

  for (i = 0; i < 4; i++) {
    if (y == rep) {
      return 1;
    }
    y = rotate90(y);
  }
  y = flip_bits(y);
  for (i = 0; i < 4; i++) {
    if (y == rep) {
      return 1;
    }
    y = rotate90(y);
  }
  return 0;
}

static int popcount(int x) {
  int n = 0;

  while (x) {
    n++;
    x &= x - 1;
  }
  return n;
}

static int alive_at(int c, int r) {
  return cur[((r + rows) % rows) * cols + ((c + cols) % cols)] == ALIVE;
}

static int config(int c, int r) {
  int cfg = 0;

  if (alive_at(c - 1, r - 1)) {
    cfg |= 1 << 0;
  }
  if (alive_at(c, r - 1)) {
    cfg |= 1 << 1;
  }
  if (alive_at(c + 1, r - 1)) {
    cfg |= 1 << 2;
  }
  if (alive_at(c - 1, r)) {
    cfg |= 1 << 3;
  }
  if (alive_at(c + 1, r)) {
    cfg |= 1 << 5;
  }
  if (alive_at(c - 1, r + 1)) {
    cfg |= 1 << 6;
  }
  if (alive_at(c, r + 1)) {
    cfg |= 1 << 7;
  }
  if (alive_at(c + 1, r + 1)) {
    cfg |= 1 << 8;
  }
  return cfg;
}

static int step(int c, int r, int state) {
  int cfg = config(c, r);
  int n = popcount(cfg);

  if (state == DEAD) {
    if (n == 3 && (matches(cfg, L3A) || matches(cfg, L3I) ||
                   matches(cfg, L3Q) || matches(cfg, L3Y))) {
      return ALIVE;
    }
    if (n == 4 && matches(cfg, L4N)) {
      return ALIVE;
    }
    return DEAD;
  }
  if (state == ALIVE) {
    if (n == 2) {
      return ALIVE;
    }
    if (n == 3 && !matches(cfg, L3A)) {
      return ALIVE;
    }
    if (n == 4 && (matches(cfg, L4A) || matches(cfg, L4I))) {
      return ALIVE;
    }
    return DYING;
  }
  return DEAD;
}

static void seed_board(void) {
  int i;

  seed += 2654435761u;
  for (i = 0; i < cells; i++) {
    cur[i] = (rng_next(&seed) % 100u) < 38u ? ALIVE : DEAD;
  }
  last_pop = -1;
  still = 0;
}

void setup(void) {
  cols = width;
  rows = height;
  if (cols * rows > MAX_CELLS) {
    rows = MAX_CELLS / cols;
  }
  cells = cols * rows;
  seed_board();
}

void draw(void) {
  uint16_t *fb = hub75_pixels();
  int pop = 0;
  int i;
  int r;
  int c;

  if (!fb) {
    return;
  }

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
      int idx = r * cols + c;
      int s = step(c, r, cur[idx]);

      next[idx] = (uint8_t)s;
      if (s == ALIVE) {
        pop++;
        fb[idx] = color(255, 0, 0);
      } else if (s == DYING) {
        fb[idx] = color(255, 200, 0);
      } else {
        fb[idx] = color(0, 0, 0);
      }
    }
  }

  for (i = 0; i < cells; i++) {
    cur[i] = next[i];
  }

  if (pop == last_pop) {
    still++;
  } else {
    still = 0;
    last_pop = pop;
  }
  if (pop == 0 || still > 240) {
    seed_board();
  }
}
