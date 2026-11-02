#include "hub75.h"
#include "util.h"

#include <math.h>

#define HALF_PI 1.57079632679f
#define MAX_POINTS 256
#define MAX_TILT 0.85f
#define MAX_BRANCHES 24
#define MAX_BRANCH_POINTS 32

typedef struct {
  float x;
  float y;
} Point;

typedef struct {
  Point p[MAX_BRANCH_POINTS];
  int count;
  int depth;
  int side;
  unsigned char active;
  unsigned char split_done;
  float tip_x;
  float tip_y;
  float heading;
  float base_heading;
  float curvature;
  float arc_left;
  float length;
  float grown;
  float speed;
  float point_accum;
  float split_at;
} Branch;

static Point path[MAX_POINTS];
static Branch branches[MAX_BRANCHES];
static int path_head;
static int path_count;
static int branch_slot;
static int branch_side;
static float tip_x;
static float tip_y;
static float heading;
static float curvature;
static float arc_left;
static float point_accum;
static float main_travelled;
static float next_branch_at;
static float cam_x;
static float cam_y;
static float lead;
static float speed;
static float unit;
static uint32_t seed;

static void append_point(float x, float y) {
  path[path_head].x = x;
  path[path_head].y = y;
  path_head = (path_head + 1) % MAX_POINTS;
  if (path_count < MAX_POINTS) {
    path_count++;
  }
}

static void new_arc(void) {
  float off = heading + HALF_PI;

  arc_left = rng_float(&seed, 8.0f, 16.0f) * unit;
  curvature = (rng_float(&seed, -0.025f, 0.025f) - off * 0.015f) / unit;
}

static void advance_stem(float dist) {
  while (dist > 0.0f) {
    float piece = fminf(dist, arc_left);
    float mid;

    piece = fminf(piece, 1.5f);
    mid = heading + curvature * piece * 0.5f;
    tip_x += cosf(mid) * piece;
    tip_y += sinf(mid) * piece;
    heading += curvature * piece;
    dist -= piece;
    arc_left -= piece;
    point_accum += piece;

    if (heading < -HALF_PI - MAX_TILT) {
      heading = -HALF_PI - MAX_TILT;
      curvature = fabsf(curvature);
    } else if (heading > -HALF_PI + MAX_TILT) {
      heading = -HALF_PI + MAX_TILT;
      curvature = -fabsf(curvature);
    }

    if (arc_left <= 0.0001f) {
      new_arc();
    }
    if (point_accum >= 1.5f) {
      append_point(tip_x, tip_y);
      point_accum -= 1.5f;
    }
  }
}

static void append_branch_point(Branch *b, float x, float y) {
  if (b->count < MAX_BRANCH_POINTS) {
    b->p[b->count].x = x;
    b->p[b->count].y = y;
    b->count++;
  }
}

static void new_branch_arc(Branch *b) {
  float off = b->heading - b->base_heading;

  b->arc_left = rng_float(&seed, 6.0f, 12.0f) * unit;
  b->curvature =
      (rng_float(&seed, -0.025f, 0.025f) - off * 0.020f) / unit;
}

static void spawn_branch(float x, float y, float parent_heading, float length,
                         int depth, int side) {
  Branch *b = &branches[branch_slot];
  float rel_min = depth == 0 ? 0.65f : 0.55f;
  float rel_max = depth == 0 ? 1.15f : 1.00f;

  branch_slot = (branch_slot + 1) % MAX_BRANCHES;
  if (length < 3.0f) {
    length = 3.0f;
  }

  b->active = 1;
  b->split_done = 0;
  b->depth = depth;
  b->side = side;
  b->tip_x = x;
  b->tip_y = y;
  b->heading = parent_heading + (float)side * rng_float(&seed, rel_min, rel_max);
  b->base_heading = b->heading;
  b->length = length;
  b->grown = 0.0f;
  b->speed = 0.26f * unit * rng_float(&seed, 0.85f, 1.15f);
  b->point_accum = 0.0f;
  b->split_at = length * rng_float(&seed, 0.40f, 0.70f);
  if (b->split_at < 3.0f) {
    b->split_done = 1;
  }
  b->count = 0;
  append_branch_point(b, x, y);
  new_branch_arc(b);
}

static void advance_branch(Branch *b) {
  float dist;

  if (!b->active || b->grown >= b->length) {
    return;
  }

  dist = b->speed;
  if (dist > b->length - b->grown) {
    dist = b->length - b->grown;
  }
  b->grown += dist;

  while (dist > 0.0f) {
    float piece = fminf(dist, b->arc_left);
    float mid;

    piece = fminf(piece, 1.5f);
    mid = b->heading + b->curvature * piece * 0.5f;
    b->tip_x += cosf(mid) * piece;
    b->tip_y += sinf(mid) * piece;
    b->heading += b->curvature * piece;
    dist -= piece;
    b->arc_left -= piece;
    b->point_accum += piece;

    if (b->heading < b->base_heading - 0.70f) {
      b->heading = b->base_heading - 0.70f;
      b->curvature = fabsf(b->curvature);
    } else if (b->heading > b->base_heading + 0.70f) {
      b->heading = b->base_heading + 0.70f;
      b->curvature = -fabsf(b->curvature);
    }

    if (b->arc_left <= 0.0001f) {
      new_branch_arc(b);
    }
    if (b->point_accum >= 1.5f) {
      append_branch_point(b, b->tip_x, b->tip_y);
      b->point_accum -= 1.5f;
    }
  }

  if (!b->split_done && b->depth == 0 && b->grown >= b->split_at) {
    int side = (rng_next(&seed) & 1u) ? 1 : -1;

    b->split_done = 1;
    spawn_branch(b->tip_x, b->tip_y, b->heading,
                 b->length * rng_float(&seed, 0.55f, 0.80f), 1, side);
  }
}

static void update_branches(void) {
  int i;

  for (i = 0; i < MAX_BRANCHES; i++) {
    advance_branch(&branches[i]);
  }
}

static void maybe_spawn_branch(void) {
  if (main_travelled < next_branch_at) {
    return;
  }

  spawn_branch(tip_x, tip_y, heading,
               (float)height * rng_float(&seed, 0.18f, 0.32f), 0, branch_side);
  branch_side = -branch_side;
  next_branch_at = main_travelled + rng_float(&seed, 7.0f, 13.0f) * unit;
}

static void update_camera(void) {
  float target_x = tip_x;
  float target_y = tip_y + lead;

  cam_x += (target_x - cam_x) * 0.12f;
  cam_y += (target_y - cam_y) * 0.12f;
}

static void project(float x, float y, int *sx, int *sy) {
  *sx = (int)lroundf(x - cam_x + (float)width * 0.5f);
  *sy = (int)lroundf(y - cam_y + (float)height * 0.5f);
}

static int visible_segment(int x0, int y0, int x1, int y1) {
  if (x0 < -2 && x1 < -2) {
    return 0;
  }
  if (x0 > width + 2 && x1 > width + 2) {
    return 0;
  }
  if (y0 < -2 && y1 < -2) {
    return 0;
  }
  if (y0 > height + 2 && y1 > height + 2) {
    return 0;
  }
  return 1;
}

static void draw_branch(const Branch *b) {
  int prev_x;
  int prev_y;
  int i;
  int x;
  int y;

  if (!b->active || b->count < 1) {
    return;
  }

  project(b->p[0].x, b->p[0].y, &prev_x, &prev_y);
  for (i = 1; i < b->count; i++) {
    project(b->p[i].x, b->p[i].y, &x, &y);
    if (visible_segment(prev_x, prev_y, x, y)) {
      line(prev_x, prev_y, x, y);
    }
    prev_x = x;
    prev_y = y;
  }

  project(b->tip_x, b->tip_y, &x, &y);
  if (visible_segment(prev_x, prev_y, x, y)) {
    line(prev_x, prev_y, x, y);
  }
  point(x, y);
}

void setup(void) {
  int i;

  unit = (float)height / 64.0f;
  lead = (float)height * 0.24f;
  speed = 0.30f * unit;
  seed = 0x0F10E4A7u;
  path_head = 0;
  path_count = 0;
  branch_slot = 0;
  branch_side = (rng_next(&seed) & 1u) ? 1 : -1;
  tip_x = 0.0f;
  tip_y = 0.0f;
  heading = -HALF_PI;
  curvature = 0.0f;
  arc_left = 0.0f;
  point_accum = 0.0f;
  main_travelled = 0.0f;
  next_branch_at = rng_float(&seed, 8.0f, 14.0f) * unit;
  for (i = 0; i < MAX_BRANCHES; i++) {
    branches[i].active = 0;
  }
  append_point(tip_x, tip_y);
  new_arc();
  cam_x = tip_x;
  cam_y = tip_y + lead;
}

void draw(void) {
  int start;
  int prev_x;
  int prev_y;
  int i;
  int tx;
  int ty;
  int weight = height >= 48 ? 2 : 1;

  advance_stem(speed);
  main_travelled += speed;
  maybe_spawn_branch();
  update_branches();
  update_camera();

  background(color(0, 0, 0));
  stroke(color(225, 225, 225));
  strokeWeight(1);
  for (i = 0; i < MAX_BRANCHES; i++) {
    draw_branch(&branches[i]);
  }

  stroke(color(255, 255, 255));
  strokeWeight(weight);
  start = path_count < MAX_POINTS ? 0 : path_head;
  project(path[start].x, path[start].y, &prev_x, &prev_y);

  for (i = 1; i < path_count; i++) {
    int index = (start + i) % MAX_POINTS;
    int x;
    int y;

    project(path[index].x, path[index].y, &x, &y);
    if (visible_segment(prev_x, prev_y, x, y)) {
      line(prev_x, prev_y, x, y);
    }
    prev_x = x;
    prev_y = y;
  }

  project(tip_x, tip_y, &tx, &ty);
  if (visible_segment(prev_x, prev_y, tx, ty)) {
    line(prev_x, prev_y, tx, ty);
  }
  point(tx, ty);
}
