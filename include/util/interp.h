#ifndef HUB75_UTIL_INTERP_H
#define HUB75_UTIL_INTERP_H

/* Scalar primitives: keep a value in range, blend between two, and the
   Hermite smoothstep curve. The named curves in ease.h are built on
   these. */

/* --- clamp --- */

static inline float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

/* The common 0..1 case, used by eases and hashes. */
static inline float clamp01(float v) {
  return clampf(v, 0.0f, 1.0f);
}

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

/* For the 8-bit channels color() takes. */
static inline int clamp_byte(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 255) {
    return 255;
  }
  return v;
}

/* --- blend --- */

static inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

/* Rescale v from [a, b] to [c, d]. A zero-width input maps to c. */
static inline float mapf(float v, float a, float b, float c, float d) {
  if (a == b) {
    return c;
  }
  return c + (v - a) * (d - c) / (b - a);
}

/* Not one of the named curves: the Hermite smoothstep, 3x^2 - 2x^3.
   Callers pass x already normalized to [0, 1]. */
static inline float smoothstep(float x) { return x * x * (3.0f - 2.0f * x); }

/* --- integer --- */

static inline int gcd_i(int a, int b) {
  while (b) {
    int t = a % b;
    a = b;
    b = t;
  }
  return a;
}

#endif
