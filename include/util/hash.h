#ifndef HUB75_UTIL_HASH_H
#define HUB75_UTIL_HASH_H

#include <stdint.h>

/* Integer hashes. The combine helpers fold coordinates together and
   the finalizers scatter the bits; hash_unit turns any of them into a
   fraction in [0, 1). */

/* Fold two or three integer coordinates into one value. */
static inline unsigned hash_combine2(int x, int y) {
  return (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
}

static inline unsigned hash_combine3(int x, int y, int z) {
  return (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^
         (unsigned)z * 83492791u;
}

/* Bit mixer for a single value (lowbias32). */
static inline uint32_t hash_mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

/* Map 24 bits of a hash to [0, 1). */
static inline float hash_unit(uint32_t x) {
  return (float)(x & 0xFFFFFFu) / (float)0x1000000u;
}

/* Hash one integer straight to [0, 1). */
static inline float hash_i01(int i) {
  unsigned x = (unsigned)i * 2654435761u + 0x9E3779B9u;

  x ^= x >> 15;
  x *= 2246822519u;
  x ^= x >> 13;
  return hash_unit((uint32_t)x);
}

#endif
