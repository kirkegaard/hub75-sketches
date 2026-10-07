#include "hub75.h"

/* Two particle streams. The border drifts one way and an inset box
   drifts the other, so the frame reads as a seam where the motion
   reverses, and a centered triangle stays clear. Adapted from a canvas
   particle demo; the padding, the counts, and the triangle are scaled
   to the panel. */

#define BG_MAX 1200
#define FG_MAX 600

typedef struct Particle {
  float x;
  float y;
  float v;
  float o;
} Particle;

static Particle back[BG_MAX];
static Particle front[FG_MAX];
static int back_count;
static int front_count;

static int pad;
static int tri_w;
static int tri_h;

static uint32_t seed = 1;

static unsigned long my_rand(void) {
  seed = seed * 1103515245u + 12345u;
  return (seed / 65536u) % 32768u;
}

static float rand_float(float min, float max) {
  return min + (max - min) * ((float)my_rand() / 32767.0f);
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

static void spawn_back(Particle *p) {
  p->x = rand_float(0.0f, (float)width);
  p->y = rand_float(0.0f, (float)height);
  p->v = rand_float(0.1f, 1.0f);
  p->o = rand_float(0.25f, 0.9f);
}

static void spawn_front(Particle *p) {
  p->x = rand_float((float)pad, (float)(width - pad));
  p->y = rand_float((float)pad, (float)(height - pad));
  p->v = rand_float(0.1f, 1.0f);
  p->o = rand_float(0.25f, 0.9f);
}

static int in_box(int x, int y) {
  return x >= pad && x < width - pad && y >= pad && y < height - pad;
}

/* Kept clear so the triangle reads as a silhouette over the interior. */
static int in_triangle(int x, int y) {
  int cx = width / 2;
  int cy = height / 2;
  int dy = y - (cy - tri_h);
  int half;

  if (dy < 0 || dy > 2 * tri_h) {
    return 0;
  }
  half = tri_h > 0 ? tri_w * dy / (2 * tri_h) : tri_w;
  return x >= cx - half && x <= cx + half;
}

void setup(void) {
  int area = width * height;
  int i;

  seed = 1;
  back_count = clamp_int(area / 24, 24, BG_MAX);
  front_count = clamp_int(area / 32, 12, FG_MAX);

  pad = (width < height ? width : height) / 6;
  if (pad < 2) {
    pad = 2;
  }
  tri_w = width / 6;
  tri_h = height / 3;

  for (i = 0; i < back_count; i++) {
    spawn_back(&back[i]);
  }
  for (i = 0; i < front_count; i++) {
    spawn_front(&front[i]);
  }
}

void draw(void) {
  int i;

  background(color(0, 0, 0));
  strokeWeight(1);

  for (i = 0; i < back_count; i++) {
    int x;
    int y;

    back[i].x -= back[i].v;
    if (back[i].x < 0.0f) {
      back[i].x = (float)width;
      back[i].y = rand_float(0.0f, (float)height);
      back[i].v = rand_float(0.1f, 1.0f);
    }
    x = (int)back[i].x;
    y = (int)back[i].y;
    if (in_box(x, y)) {
      continue;
    }
    stroke(color((uint8_t)(back[i].o * 255.0f), (uint8_t)(back[i].o * 255.0f),
                 (uint8_t)(back[i].o * 255.0f)));
    point(x, y);
  }

  for (i = 0; i < front_count; i++) {
    int x;
    int y;

    front[i].x += front[i].v;
    if (front[i].x > (float)(width - pad)) {
      front[i].x = (float)pad;
      front[i].y = rand_float((float)pad, (float)(height - pad));
      front[i].v = rand_float(0.1f, 1.0f);
    }
    x = (int)front[i].x;
    y = (int)front[i].y;
    if (in_triangle(x, y)) {
      continue;
    }
    stroke(color((uint8_t)(front[i].o * 255.0f), (uint8_t)(front[i].o * 255.0f),
                 (uint8_t)(front[i].o * 255.0f)));
    point(x, y);
  }
}
