#include "hub75.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* One rule: the classic plasma. Four phase accumulators drift by tiny
   cosines of the grid coordinates, and a nest of cosines turns them into
   three channels. The accumulator updates happen per grid cell, so the
   phases vary across the frame; that spatial drift is what makes the
   plasma breathe. A direct port of the JS version -- fixed 48x48 field,
   mapped onto any panel -- except the six seeded rates are drawn at
   startup instead of baked in, so each run looks different. Uses libm
   and a few float tables, so it is the simulator / ESP32 take;
   plasma_integer is the CH552 one. */

#define RES 32
#define RAD 0.017453292f

/* The six seeded rates, each 1..4 like the JS seed(). */
static int A[6];

/* Frame-level phases: s1 = aa + ab, s2 = ac + ad. Only the sums feed the
   cosines, so these are all we carry between frames. */
static float s1f;
static float s2f;
static float inc_s1;
static float inc_s2;
static float per_s1;
static float per_s2;

/* Prefix sums of the per-cell increments, so a cell's phase can be had
   without walking the loop. */
static float pre0[RES + 1];
static float pre2[RES + 1];
static float pre1[RES + 1];
static float pre3[RES + 1];
static float total1;
static float total3;

static uint32_t rng;

static unsigned next_rng(void) {
  rng = rng * 1103515245u + 12345u;
  return (unsigned)(rng >> 16);
}

static uint8_t c8(float v) {
  int i = (int)v;

  if (i < 0) {
    i = 0;
  }
  if (i > 255) {
    i = 255;
  }
  return (uint8_t)i;
}

/* Rebuild the prefix tables and per-frame increments from A. */
static void rebuild(void) {
  int i;

  pre0[0] = pre2[0] = pre1[0] = pre3[0] = 0.0f;
  for (i = 0; i < RES; i++) {
    pre0[i + 1] = pre0[i] + cosf(RAD * (float)i * (float)A[0]);
    pre2[i + 1] = pre2[i] + sinf(RAD * (float)(RES - i) * (float)A[2]);
    pre1[i + 1] = pre1[i] + cosf(RAD * (float)i * (float)A[1]);
    pre3[i + 1] = pre3[i] + sinf(RAD * (float)(RES - i) * (float)A[3]);
  }
  total1 = pre1[RES];
  total3 = pre3[RES];
  inc_s1 = 0.0005f * pre0[RES] + 0.001f * (float)RES * total1;
  inc_s2 = 0.0010f * pre2[RES] + 0.001f * (float)RES * total3;
  per_s1 = 360.0f / (float)A[4];
  per_s2 = 360.0f / (float)A[5];
  s1f = 0.0f;
  s2f = 0.0f;
}

void setup(void) {
  int i;

  /* No clock in the sketch API, so borrow the address of a static as the
     entropy source: it shifts per run under ASLR. */
  rng = (uint32_t)(size_t)&rng ^ 0x9E3779B9u;
  for (i = 0; i < 6; i++) {
    A[i] = 1 + (int)(next_rng() % 4u);
  }
  rebuild();
}

void draw(void) {
  int px;
  int py;

  s1f = fmodf(s1f + inc_s1, per_s1);
  s2f = fmodf(s2f + inc_s2, per_s2);

  background(color(0, 0, 0));
  for (py = 0; py < height; py++) {
    int ix = py * RES / height;
    float fa = s1f + 0.0005f * pre0[ix + 1];
    float fc = s2f + 0.0010f * pre2[ix + 1];

    for (px = 0; px < width; px++) {
      int iy = px * RES / width;
      float s1 = fa + 0.001f * ((float)ix * total1 + pre1[iy + 1]);
      float s2 = fc + 0.001f * ((float)ix * total3 + pre3[iy + 1]);
      float h = (float)ix * 8.0f * sinf(RAD * s1 * (float)A[4]);
      float j = (float)iy * 8.0f * cosf(RAD * s2 * (float)A[5]);
      float k = ((float)ix * (float)A[0] + (float)iy * (float)A[1]) * 32.0f *
                sinf(RAD * ((float)(RES - ix) * h + (float)(iy - RES) * h) *
                     (float)A[2] / 720.0f);
      float l =
          ((float)RES * (float)A[3] - (float)ix * (float)A[3] +
           ((float)RES * (float)A[4] - (float)iy * (float)A[4])) *
          32.0f *
          sinf(RAD * ((float)ix * h + (float)iy * j) * (float)A[5] / 720.0f);
      float hh = 48.0f * cosf(RAD * h) + 42.0f * cosf(RAD * j);
      float cr = 128.0f + 42.0f * cosf(RAD * k) + hh;
      float cg = 128.0f + 42.0f * cosf(RAD * l) + hh;
      float cb = (cr + cg) * 0.5f - hh * 2.0f;

      stroke(color(c8(cr), c8(cg), c8(cb)));
      point(px, py);
    }
  }
}
