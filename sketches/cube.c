#include "hub75.h"

#include <math.h>

static uint16_t bg;

static const float STEP = 6.28318530718f / 256.f;
static const int CUBE = 256;
static const int VERTS[8][3] = {
    {-256, -256, -256}, {256, -256, -256}, {-256, 256, -256}, {256, 256, -256},
    {-256, -256, 256},  {256, -256, 256},  {-256, 256, 256},  {256, 256, 256},
};
static const int EDGES[12][2] = {
    {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
    {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

static void rotate(float *x, float *y, float *z, float yaw, float pitch) {
  float s = sinf(yaw);
  float c = cosf(yaw);
  float nx = *x * c - *z * s;
  float nz = *x * s + *z * c;
  float ny;

  *x = nx;
  *z = nz;
  s = sinf(pitch);
  c = cosf(pitch);
  ny = *y * c - *z * s;
  nz = *y * s + *z * c;
  *y = ny;
  *z = nz;
}

static void project(float x, float y, float z, int *sx, int *sy) {
  int fit = width < height ? width : height;
  float focal = (float)(fit * 15 / 16);
  float denom = z + (float)(CUBE * 4);

  if (denom < 128.f) {
    denom = 128.f;
  }
  *sx = width / 2 + (int)(x * focal / denom);
  *sy = height / 2 + (int)(y * focal / denom);
}

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  float yaw = (float)(frameCount & 255u) * STEP;
  float pitch = (float)((frameCount * 3u) & 255u) * STEP;
  float pts[8][3];
  int depth[12];
  int order[12];
  int i;

  background(bg);

  for (i = 0; i < 8; i++) {
    float x = (float)VERTS[i][0];
    float y = (float)VERTS[i][1];
    float z = (float)VERTS[i][2];

    rotate(&x, &y, &z, yaw, pitch);

    pts[i][0] = x;
    pts[i][1] = y;
    pts[i][2] = z;
  }

  for (i = 0; i < 12; i++) {
    int a = EDGES[i][0];
    int b = EDGES[i][1];
    depth[i] = (int)((pts[a][2] + pts[b][2]) / 2.f);
    order[i] = i;
  }

  for (i = 1; i < 12; i++) {
    int key = order[i];
    int j = i;
    while (j > 0 && depth[order[j - 1]] < depth[key]) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = key;
  }

  for (i = 0; i < 12; i++) {
    int e = order[i];
    int a = EDGES[e][0];
    int b = EDGES[e][1];
    int shade = (420 - depth[e]) * 255 / 840;
    int x0, y0, x1, y1;

    if (shade < 48) {
      shade = 48;
    }

    if (shade > 255) {
      shade = 255;
    }

    project(pts[a][0], pts[a][1], pts[a][2], &x0, &y0);
    project(pts[b][0], pts[b][1], pts[b][2], &x1, &y1);
    stroke(color((uint8_t)shade, (uint8_t)shade, (uint8_t)shade));
    line(x0, y0, x1, y1);
  }
}
