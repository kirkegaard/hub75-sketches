#include "hub75.h"
#include "util.h"

#define MAX_DROPS 64

static uint32_t seed = 1;

typedef struct Drop {
  int x;
  float y;
  float age;
  float life;
} Drop;

static Drop drops[MAX_DROPS];
static int drop_count;

static void reset_drop(Drop *drop) {
  drop->x = (int)(rng_next(&seed) % (unsigned long)width);
  drop->y = 0.0f;
  drop->age = 0.0f;
  drop->life = (float)height * rng_float(&seed, 0.6f, 1.1f);
}

void setup(void) {
  int i;

  drop_count = clamp_int(width / 4, 4, MAX_DROPS);
  for (i = 0; i < drop_count; i++) {
    reset_drop(&drops[i]);
  }
}

void draw(void) {
  int i;

  background(color(0, 0, 0));
  stroke(color(255, 255, 255));

  for (i = 0; i < drop_count; i++) {
    Drop *drop = &drops[i];

    drop->age += 1.0f;
    if (drop->age >= drop->life) {
      reset_drop(drop);
    } else {
      drop->y = ease_in_cubic(drop->age / drop->life) * (float)height;
    }
    point(drop->x, (int)drop->y);
  }
}
