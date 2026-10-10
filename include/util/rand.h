#ifndef HUB75_UTIL_RAND_H
#define HUB75_UTIL_RAND_H

#include <stdint.h>

/* A linear congruential generator. The caller owns the state so each
   sketch keeps its own sequence and can reseed it on demand. */

static inline unsigned long rng_next(uint32_t *state) {
  *state = *state * 1103515245u + 12345u;
  return (*state / 65536u) % 32768u;
}

static inline float rng_float(uint32_t *state, float min, float max) {
  return min + (max - min) * ((float)rng_next(state) / 32767.0f);
}

#endif
