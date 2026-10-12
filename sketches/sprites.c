#include "hub75.h"
#include "sprites.h"

/* One rule: a cast of characters wanders the panel. Each one drifts,
   flips to face the way it is going, and bobs as it walks; at a wall it
   turns around and becomes someone else. The panel sets the crowd -- one
   character on an 8x8 matrix, up to sixteen on the 128x64. Transparent
   pixels are skipped. Integer only, so it also fits the CH552 matrix. */

#define MAX_ACTORS 16

typedef struct {
  int x;
  int y;
  int vx;
  int vy;
  int who;
  unsigned phase;
} Actor;

static Actor actors[MAX_ACTORS];
static int actor_count;
static uint32_t seed = 0x1234u;

/* A tiny LCG, so the sketch stays integer-only (util.h's easings would
   drag the float library into the CH552 build). */
static unsigned next_rand(void) {
  seed = seed * 1103515245u + 12345u;
  return (unsigned)((seed / 65536u) % 32768u);
}

void setup(void) {
  int i;
  int maxx = width - HUB75_SPRITE_CELL;
  int maxy = height - HUB75_SPRITE_CELL;

  if (maxx < 0) {
    maxx = 0;
  }
  if (maxy < 0) {
    maxy = 0;
  }
  actor_count = (width / HUB75_SPRITE_CELL) * (height / HUB75_SPRITE_CELL);
  if (actor_count < 1) {
    actor_count = 1;
  }
  if (actor_count > MAX_ACTORS) {
    actor_count = MAX_ACTORS;
  }

  for (i = 0; i < actor_count; i++) {
    actors[i].x = maxx > 0 ? (int)(next_rand() % (unsigned)(maxx + 1)) : 0;
    actors[i].y = maxy > 0 ? (int)(next_rand() % (unsigned)(maxy + 1)) : 0;
    actors[i].vx = (next_rand() & 1u) ? 1 : -1;
    actors[i].vy = (next_rand() & 1u) ? 1 : -1;
    actors[i].who = (int)(next_rand() % HUB75_SPRITE_COUNT);
    actors[i].phase = (unsigned)next_rand();
  }
}

void draw(void) {
  int i;
  int maxx = width - HUB75_SPRITE_CELL;
  int maxy = height - HUB75_SPRITE_CELL;

  if (maxx < 0) {
    maxx = 0;
  }
  if (maxy < 0) {
    maxy = 0;
  }

  background(color(0, 0, 0));
  noStroke();
  for (i = 0; i < actor_count; i++) {
    Actor *a = &actors[i];
    int mirror;
    int bob;
    int ly;
    int lx;

    a->phase++;
    if ((a->phase & 1u) == 0u) {
      if (maxx > 0) {
        a->x += a->vx;
        if (a->x <= 0) {
          a->x = 0;
          a->vx = 1;
          a->who = (a->who + 1) % HUB75_SPRITE_COUNT;
        } else if (a->x >= maxx) {
          a->x = maxx;
          a->vx = -1;
          a->who = (a->who + 1) % HUB75_SPRITE_COUNT;
        }
      }
      if (maxy > 0) {
        a->y += a->vy;
        if (a->y <= 0) {
          a->y = 0;
          a->vy = 1;
          a->who = (a->who + 1) % HUB75_SPRITE_COUNT;
        } else if (a->y >= maxy) {
          a->y = maxy;
          a->vy = -1;
          a->who = (a->who + 1) % HUB75_SPRITE_COUNT;
        }
      }
    }
    if (maxx == 0 && maxy == 0 && (a->phase % 60u) == 0u) {
      a->who = (a->who + 1) % HUB75_SPRITE_COUNT;
    }

    mirror = (a->vx < 0) ^ (int)((a->phase >> 3) & 1u);
    /* Only bob where there is a row to give up, so a character that
       already fills the panel does not just flash a black border. */
    bob = a->y < maxy ? (int)((a->phase >> 2) & 1u) : 0;

    for (ly = 0; ly < HUB75_SPRITE_CELL; ly++) {
      for (lx = 0; lx < HUB75_SPRITE_CELL; lx++) {
        int sx = mirror ? HUB75_SPRITE_CELL - 1 - lx : lx;
        uint16_t v = hub75_sprites[a->who][ly * HUB75_SPRITE_CELL + sx];

        if (v) {
          fill(v);
          rect(a->x + lx, a->y + ly + bob, 1, 1);
        }
      }
    }
  }
}
