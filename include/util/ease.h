#ifndef HUB75_UTIL_EASE_H
#define HUB75_UTIL_EASE_H

#include <math.h>

/* Easing curves on x in [0, 1]. The set follows easings.net and its
   easingsFunctions reference (easeInQuad, easeOutCubic, ...), renamed
   to snake_case with an ease_ prefix. Each is its own function so a
   sketch reads the shape it wants by name. These are opinionated
   time-to-progress curves; the raw primitives they build on (clamp,
   lerp, mapf, smoothstep) live in interp.h. */

#define EASE_PI 3.14159265358979323846f
#define EASE_BACK_C1 1.70158f
#define EASE_BACK_C2 (EASE_BACK_C1 * 1.525f)
#define EASE_BACK_C3 (EASE_BACK_C1 + 1.0f)
#define EASE_ELASTIC_C4 (2.0f * EASE_PI / 3.0f)
#define EASE_ELASTIC_C5 (2.0f * EASE_PI / 4.5f)

static inline float ease_linear(float x) { return x; }

static inline float ease_in_quad(float x) { return x * x; }

static inline float ease_out_quad(float x) {
  return 1.0f - (1.0f - x) * (1.0f - x);
}

static inline float ease_in_out_quad(float x) {
  return x < 0.5f ? 2.0f * x * x : 1.0f - powf(-2.0f * x + 2.0f, 2.0f) * 0.5f;
}

static inline float ease_in_cubic(float x) { return x * x * x; }

static inline float ease_out_cubic(float x) {
  float t = 1.0f - x;
  return 1.0f - t * t * t;
}

static inline float ease_in_out_cubic(float x) {
  return x < 0.5f ? 4.0f * x * x * x
                  : 1.0f - powf(-2.0f * x + 2.0f, 3.0f) * 0.5f;
}

static inline float ease_in_quart(float x) { return x * x * x * x; }

static inline float ease_out_quart(float x) {
  float t = 1.0f - x;
  return 1.0f - t * t * t * t;
}

static inline float ease_in_out_quart(float x) {
  return x < 0.5f ? 8.0f * x * x * x * x
                  : 1.0f - powf(-2.0f * x + 2.0f, 4.0f) * 0.5f;
}

static inline float ease_in_quint(float x) { return x * x * x * x * x; }

static inline float ease_out_quint(float x) {
  float t = 1.0f - x;
  return 1.0f - t * t * t * t * t;
}

static inline float ease_in_out_quint(float x) {
  return x < 0.5f ? 16.0f * x * x * x * x * x
                  : 1.0f - powf(-2.0f * x + 2.0f, 5.0f) * 0.5f;
}

static inline float ease_in_sine(float x) {
  return 1.0f - cosf(x * EASE_PI * 0.5f);
}

static inline float ease_out_sine(float x) {
  return sinf(x * EASE_PI * 0.5f);
}

static inline float ease_in_out_sine(float x) {
  return -(cosf(EASE_PI * x) - 1.0f) * 0.5f;
}

static inline float ease_in_expo(float x) {
  return x == 0.0f ? 0.0f : powf(2.0f, 10.0f * x - 10.0f);
}

static inline float ease_out_expo(float x) {
  return x == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
}

static inline float ease_in_out_expo(float x) {
  if (x == 0.0f) {
    return 0.0f;
  }
  if (x == 1.0f) {
    return 1.0f;
  }
  return x < 0.5f ? powf(2.0f, 20.0f * x - 10.0f) * 0.5f
                  : (2.0f - powf(2.0f, -20.0f * x + 10.0f)) * 0.5f;
}

static inline float ease_in_circ(float x) {
  return 1.0f - sqrtf(1.0f - x * x);
}

static inline float ease_out_circ(float x) {
  float t = x - 1.0f;
  return sqrtf(1.0f - t * t);
}

static inline float ease_in_out_circ(float x) {
  return x < 0.5f ? (1.0f - sqrtf(1.0f - powf(2.0f * x, 2.0f))) * 0.5f
                  : (sqrtf(1.0f - powf(-2.0f * x + 2.0f, 2.0f)) + 1.0f) * 0.5f;
}

static inline float ease_in_back(float x) {
  return EASE_BACK_C3 * x * x * x - EASE_BACK_C1 * x * x;
}

static inline float ease_out_back(float x) {
  float t = x - 1.0f;
  return 1.0f + EASE_BACK_C3 * t * t * t + EASE_BACK_C1 * t * t;
}

static inline float ease_in_out_back(float x) {
  if (x < 0.5f) {
    return powf(2.0f * x, 2.0f) *
           ((EASE_BACK_C2 + 1.0f) * 2.0f * x - EASE_BACK_C2) * 0.5f;
  }
  return (powf(2.0f * x - 2.0f, 2.0f) *
              ((EASE_BACK_C2 + 1.0f) * (x * 2.0f - 2.0f) + EASE_BACK_C2) +
          2.0f) *
         0.5f;
}

static inline float ease_in_elastic(float x) {
  if (x == 0.0f) {
    return 0.0f;
  }
  if (x == 1.0f) {
    return 1.0f;
  }
  return -powf(2.0f, 10.0f * x - 10.0f) *
         sinf((x * 10.0f - 10.75f) * EASE_ELASTIC_C4);
}

static inline float ease_out_elastic(float x) {
  if (x == 0.0f) {
    return 0.0f;
  }
  if (x == 1.0f) {
    return 1.0f;
  }
  return powf(2.0f, -10.0f * x) * sinf((x * 10.0f - 0.75f) * EASE_ELASTIC_C4) +
         1.0f;
}

static inline float ease_in_out_elastic(float x) {
  if (x == 0.0f) {
    return 0.0f;
  }
  if (x == 1.0f) {
    return 1.0f;
  }
  if (x < 0.5f) {
    return -powf(2.0f, 20.0f * x - 10.0f) *
           sinf((20.0f * x - 11.125f) * EASE_ELASTIC_C5) * 0.5f;
  }
  return powf(2.0f, -20.0f * x + 10.0f) *
             sinf((20.0f * x - 11.125f) * EASE_ELASTIC_C5) * 0.5f +
         1.0f;
}

static inline float ease_out_bounce(float x) {
  const float n1 = 7.5625f;
  const float d1 = 2.75f;

  if (x < 1.0f / d1) {
    return n1 * x * x;
  }
  if (x < 2.0f / d1) {
    x -= 1.5f / d1;
    return n1 * x * x + 0.75f;
  }
  if (x < 2.5f / d1) {
    x -= 2.25f / d1;
    return n1 * x * x + 0.9375f;
  }
  x -= 2.625f / d1;
  return n1 * x * x + 0.984375f;
}

static inline float ease_in_bounce(float x) {
  return 1.0f - ease_out_bounce(1.0f - x);
}

static inline float ease_in_out_bounce(float x) {
  return x < 0.5f ? (1.0f - ease_out_bounce(1.0f - 2.0f * x)) * 0.5f
                  : (1.0f + ease_out_bounce(2.0f * x - 1.0f)) * 0.5f;
}

#undef EASE_PI
#undef EASE_BACK_C1
#undef EASE_BACK_C2
#undef EASE_BACK_C3
#undef EASE_ELASTIC_C4
#undef EASE_ELASTIC_C5

#endif
