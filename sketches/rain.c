#include "hub75.h"
#include "util.h"

#define MAX_DROPS 64

static uint32_t seed = 1;
static int drop_count;

typedef struct Drop Drop;

struct Drop {
  int x;
  float y;
  float age;
  float life;

  void (*reset)(Drop *self);
  void (*step)(Drop *self);
};

static Drop drops[MAX_DROPS];

static void drop_reset(Drop *self) {
  self->x = (int)(rng_next(&seed) % (unsigned long)width);
  self->y = 0.0f;
  self->age = 0.0f;
  self->life = (float)height * rng_float(&seed, 0.6f, 1.1f);
}

static void drop_step(Drop *self) {
  self->age += 1.0f;

  if (self->age >= self->life) {
    self->reset(self);
    return;
  }

  self->y = ease_in_cubic(self->age / self->life) * (float)height;
}

static void drop_init(Drop *self) {
  self->reset = drop_reset;
  self->step = drop_step;
  self->reset(self);
}

void setup(void) {
  int i;

  drop_count = clamp_int(width / 4, 4, MAX_DROPS);

  for (i = 0; i < drop_count; i++) {
    drop_init(&drops[i]);
  }
}

void draw(void) {
  int i;

  background(color(0, 0, 0));
  stroke(color(255, 255, 255));

  for (i = 0; i < drop_count; i++) {
    Drop *drop = &drops[i];

    drop->step(drop);
    point(drop->x, (int)drop->y);
  }
}
