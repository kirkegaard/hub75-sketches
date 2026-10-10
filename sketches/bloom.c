#include "hub75.h"
#include "util.h"

#include <math.h>

/* Port of a canvas demo: a cloud of soft discs ringed around the center.
   Each disc fades in as it shrinks, then respawns somewhere else in the
   ring with a fresh color from one palette. The canvas blends with
   alpha; here the color is dimmed toward the black background instead.
   The counts, radii, and ring are scaled to the panel. */

#define MAX_BALLS 512

typedef struct Ball {
  float x;
  float y;
  float a;
  float r;
  uint8_t cr;
  uint8_t cg;
  uint8_t cb;
} Ball;

static const uint8_t kPalettes[7][5][3] = {
    {{0x41, 0x14, 0x22},
     {0xbe, 0x3d, 0x64},
     {0xb7, 0x7e, 0x8d},
     {0xb3, 0x9e, 0xa2},
     {0xb0, 0xbf, 0xb7}},
    {{0xf1, 0xe6, 0xd4},
     {0xba, 0x3d, 0x49},
     {0x79, 0x1f, 0x33},
     {0x9f, 0x96, 0x94},
     {0xe3, 0xe1, 0xdc}},
    {{0x46, 0x7f, 0x71},
     {0xff, 0xe8, 0x7a},
     {0xff, 0xca, 0x53},
     {0xff, 0x89, 0x3b},
     {0xe5, 0x27, 0x38}},
    {{0x0f, 0x2d, 0x40},
     {0x19, 0x47, 0x59},
     {0x29, 0x6b, 0x73},
     {0x3e, 0x8c, 0x84},
     {0xd8, 0xf2, 0xf0}},
    {{0xcd, 0x43, 0x43},
     {0xbe, 0xb9, 0xb7},
     {0xce, 0xcb, 0xd0},
     {0xdb, 0xd5, 0xd1},
     {0x8f, 0xaa, 0xad}},
    {{0x11, 0x88, 0x88},
     {0x3f, 0xc1, 0xa6},
     {0x8e, 0xff, 0xec},
     {0xff, 0xff, 0xff},
     {0xf1, 0x0c, 0x5c}},
    {{0xfe, 0xa2, 0x35},
     {0xff, 0xd2, 0x4e},
     {0x33, 0xac, 0x93},
     {0x30, 0x97, 0x70},
     {0xcc, 0x4a, 0x6c}},
};

static Ball balls[MAX_BALLS];
static int ball_count;

static const uint8_t (*palette)[3];

static float half_x;
static float half_y;
static float xr_min;
static float xr_max;
static float yr_min;
static float yr_max;
static float bmin;
static float bmax;

static uint32_t seed = 1;

static void spawn(Ball *b, float angle) {
  float xr = rng_float(&seed, xr_min, xr_max);
  float yr = rng_float(&seed, yr_min, yr_max);
  int ci = (int)(rng_next(&seed) % 5u);

  b->x = half_x + sinf(angle) * xr;
  b->y = half_y + cosf(angle) * yr;
  b->a = 0.f;
  b->r = rng_float(&seed, bmin, bmax);
  b->cr = palette[ci][0];
  b->cg = palette[ci][1];
  b->cb = palette[ci][2];
}

void setup(void) {
  int area = width * height;
  int i;

  seed = 1;
  palette = kPalettes[rng_next(&seed) % 7u];

  half_x = (float)width * 0.5f;
  half_y = (float)height * 0.5f;

  bmax = (float)height / 8.0f;
  if (bmax < 2.0f) {
    bmax = 2.0f;
  }
  bmin = bmax / 5.0f;
  if (bmin < 1.0f) {
    bmin = 1.0f;
  }

  xr_max = half_x - bmax;
  if (xr_max < 2.0f) {
    xr_max = 2.0f;
  }
  yr_max = half_y - bmax;
  if (yr_max < 2.0f) {
    yr_max = 2.0f;
  }
  xr_min = xr_max * 0.25f;
  yr_min = yr_max * 0.25f;

  ball_count = clamp_int(area / 48, 24, MAX_BALLS);
  for (i = 0; i < ball_count; i++) {
    spawn(&balls[i], (float)i);
  }
}

void draw(void) {
  int i;

  background(color(0, 0, 0));
  noStroke();

  for (i = 0; i < ball_count; i++) {
    Ball *b = &balls[i];
    float a = b->a;
    int r;

    if (a > 1.f) {
      a = 1.f;
    }
    if (a < 0.f) {
      a = 0.f;
    }
    r = (int)lroundf(b->r);
    if (r >= 1) {
      fill(color((uint8_t)((float)b->cr * a), (uint8_t)((float)b->cg * a),
                 (uint8_t)((float)b->cb * a)));
      circle((int)lroundf(b->x), (int)lroundf(b->y), r * 2);
    }

    if (b->r < 0.1f) {
      spawn(b, rng_float(&seed, 0.f, 6.2831853f));
    }
    b->a += 0.01f;
    b->r -= 0.05f;
  }
}
