#include "hub75.h"

#include <math.h>

/* The 16 corners of a 4D hypercube, spun in six coordinate planes and
   dropped to 2D by folding z into x and w into y, each by the square
   root of two. Three planes turn with time and the other three hold a
   fixed tilt so the cube is never edge-on. Two corners share an edge
   when they differ in exactly one axis. Ported from a canvas demo. */

#define VERTS 16
#define MAX_EDGES 32
#define SQRT2 1.41421356f

typedef struct Vec4 {
  float c[4];
} Vec4;

static int edges[MAX_EDGES][2];
static int edge_count;

/* Plane, the time multiplier, the fixed tilt, and the turn direction
   for each of the six rotations, in the order the demo applies them. */
static const int plane_a[6] = {0, 1, 0, 0, 1, 2};
static const int plane_b[6] = {1, 2, 3, 2, 3, 3};
static const float turn[6] = {1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f};
static const float tilt[6] = {0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 0.0f};
static const int counter[6] = {1, 1, 1, 0, 0, 0};

static Vec4 corner(int i) {
  Vec4 p;

  p.c[0] = (i & 1) ? 1.0f : -1.0f;
  p.c[1] = ((i >> 1) & 1) ? 1.0f : -1.0f;
  p.c[2] = ((i >> 2) & 1) ? 1.0f : -1.0f;
  p.c[3] = ((i >> 3) & 1) ? 1.0f : -1.0f;
  return p;
}

static void rotate(Vec4 *p, float value, int a, int b, int ccw) {
  float ca = cosf(value);
  float sa = sinf(value);
  float first = ca * p->c[a] + sa * p->c[b];
  float second = ccw ? (-sa * p->c[a] + ca * p->c[b])
                     : (sa * p->c[a] + ca * p->c[b]);

  p->c[a] = first;
  p->c[b] = second;
}

void setup(void) {
  int i;
  int j;

  edge_count = 0;
  for (i = 0; i < VERTS; i++) {
    for (j = i + 1; j < VERTS; j++) {
      int diff = i ^ j;

      if ((diff & (diff - 1)) == 0 && edge_count < MAX_EDGES) {
        edges[edge_count][0] = i;
        edges[edge_count][1] = j;
        edge_count++;
      }
    }
  }
}

void draw(void) {
  float t = (float)frameCount * 0.0033f;
  float scale = 0.30f * (float)height;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float px[VERTS];
  float py[VERTS];
  int i;

  background(color(0, 0, 0));
  stroke(color(255, 255, 255));
  strokeWeight(1);

  for (i = 0; i < VERTS; i++) {
    Vec4 p = corner(i);
    int r;

    for (r = 0; r < 6; r++) {
      rotate(&p, tilt[r] + t * turn[r], plane_a[r], plane_b[r], counter[r]);
    }
    px[i] = cx + (p.c[0] + SQRT2 * p.c[2]) * scale;
    py[i] = cy + (p.c[1] + SQRT2 * p.c[3]) * scale;
  }

  for (i = 0; i < edge_count; i++) {
    int a = edges[i][0];
    int b = edges[i][1];

    line((int)px[a], (int)py[a], (int)px[b], (int)py[b]);
  }

  for (i = 0; i < VERTS; i++) {
    point((int)px[i], (int)py[i]);
  }
}
