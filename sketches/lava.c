#include "hub75.h"
#include "util.h"

#include <math.h>

/* One rule: metaballs. Three soft charges wander the panel. Each holds
   a field that is one at its centre and fades to nothing at its rim;
   the shade is the sum of those fields, so a lonesome charge is a
   single soft orb and two that drift close swell into a molten bridge
   and pull apart again -- the lava lamp shape is just the sum.

   Each charge wanders on two sine waves per axis at unrelated rates,
   which keeps it on a slow path that curves and changes speed without
   stalling or closing into an orbit. */

#define BALLS 3

/* One sine: how far it swings, how fast, and where it starts. */
typedef struct Wave {
  float amp;
  float rate;
  float phase;
} Wave;

typedef struct Ball {
  float cx;
  float cy;
  float r;
  Wave x[2];
  Wave y[2];
} Ball;

static Ball balls[BALLS];

/* One axis's offset from its middle at frame t: the two waves summed. */
static float axis_offset(const Wave *axis, float t) {
  return axis[0].amp * sinf(t * axis[0].rate + axis[0].phase) +
         axis[1].amp * sinf(t * axis[1].rate + axis[1].phase);
}

/* Fill one axis's two waves. They split a travel budget, so the charge
   swings exactly that far and never needs turning back. */
static void axis_init(Wave *axis, uint32_t *s, float travel) {
  axis[0].amp = travel * rng_float(s, 0.50f, 0.70f);
  axis[1].amp = travel - axis[0].amp;
  axis[0].rate = rng_float(s, 0.005f, 0.010f); /* tens of seconds a turn */
  axis[1].rate = axis[0].rate * rng_float(s, 1.9f, 3.1f);
  axis[0].phase = rng_float(s, 0.f, 6.2831853f);
  axis[1].phase = rng_float(s, 0.f, 6.2831853f);
}

/* Build one charge: a size, a middle, and a pair of waves per axis.
   Every charge swings from the panel middle, as far as it can without
   its orb clipping an edge; they start apart because their waves are
   out of phase, not because their middles differ. */
static void ball_init(Ball *b, uint32_t *s, float w, float h) {
  float r = (w < h ? w : h) * rng_float(s, 0.22f, 0.34f);

  b->cx = w * 0.5f;
  b->cy = h * 0.5f;
  b->r = r;
  axis_init(b->x, s, (w * 0.5f - r) * rng_float(s, 0.62f, 0.95f));
  axis_init(b->y, s, (h * 0.5f - r) * rng_float(s, 0.62f, 0.95f));
}

void setup(void) {
  uint32_t seed = 0x9E3779B9u;
  int i;

  for (i = 0; i < BALLS; i++) {
    ball_init(&balls[i], &seed, (float)width, (float)height);
  }
}

void draw(void) {
  float t = (float)frameCount;
  float bx[BALLS];
  float by[BALLS];
  float br2[BALLS];
  int i;
  int x;
  int y;

  for (i = 0; i < BALLS; i++) {
    Ball *b = &balls[i];

    bx[i] = b->cx + axis_offset(b->x, t);
    by[i] = b->cy + axis_offset(b->y, t);
    br2[i] = b->r * b->r;
  }

  background(color(0, 0, 0));

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float m = 0.f;
      float shade;
      int v;

      /* Sum the fields. Each is 1 at its centre and reaches zero at the
         rim, so the total clears the threshold only near a charge or on
         the bridge where two of them overlap. */
      for (i = 0; i < BALLS; i++) {
        float dx = (float)x - bx[i];
        float dy = (float)y - by[i];
        float q = 1.f - (dx * dx + dy * dy) / br2[i];

        if (q > 0.f) {
          m += q * q;
        }
      }

      if (m < 0.2f) {
        continue;
      }

      shade = clamp01((m - 0.2f) * 1.3f);
      v = (int)(shade * 255.f);

      stroke(color((uint8_t)v, (uint8_t)v, (uint8_t)v));
      point(x, y);
    }
  }
}
