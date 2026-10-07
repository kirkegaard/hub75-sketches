#include "hub75.h"

#include <math.h>
#include <stdbool.h>

/* Port of a canvas demo: a cloud of regular polygons orbits the center.
   Each polygon's center rides a slowly detuning sine pair, and its
   radius and side count ride the same wave, so the field breathes
   between a tight knot of triangles and a loose ring of octagons. The
   canvas is scaled to the panel and kept grayscale: larger polygons
   burn brighter, smaller ones sink toward the background. */

#define COUNT_MAX 24

/* The canvas stepped its clock 0.1 per frame and read it as
   time * 0.05, so the phase advances 0.005 per frame. */
#define PHASE_STEP 0.05f

static uint16_t bg;
static int count;
static float radius_min;
static float radius_max;
static bool freeze = true;

static float mapf(float v, float l1, float h1, float l2, float h2) {
  return l2 + (h2 - l2) * (v - l1) / (h1 - l1);
}

static int clamp_int(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

void setup(void) {
  int short_side = width < height ? width : height;

  bg = color(0, 0, 0);

  count = clamp_int(width * height / 220, 12, COUNT_MAX);

  radius_max = (float)short_side * 0.09f;
  radius_min = radius_max * 0.50f;
}

void draw(void) {
  float t = (float)frameCount * PHASE_STEP;
  float cx0 = (float)width * 0.5f;
  float cy0 = (float)height * 0.5f;
  float sx = (float)width * 0.34f;
  float sy = (float)height * 0.34f;
  int i;

  background(bg);
  noFill();
  strokeWeight(1);

  for (i = 0; i < count; i++) {
    float fi = (float)i;
    float yo = sinf(t + fi + 0.1f * fi * fi);
    float awo = fabsf(yo);

    float xo = cosf(t + fi + 0.0046875f * fi * yo);
    if (freeze) {
      xo = cosf(fi * 2.399996f);
    }

    float radius = mapf(awo, 0.f, 1.f, radius_min, radius_max);
    int sides = (int)mapf(awo, 0.f, 1.f, 3.f, 8.f);

    int cx = (int)(cx0 + xo * sx);
    int cy = (int)(cy0 + yo * sy);

    uint8_t gray = (uint8_t)(90.f + awo * 165.f);
    stroke(color(gray, gray, gray));

    polygon(cx, cy, radius, sides, 0.f);
  }
}
