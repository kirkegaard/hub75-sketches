#include "hub75.h"

/* Wireframe cube. Yaw steps once per frame, pitch three times as fast. */

static uint16_t bg;

/* Quarter-turn of sin, 0..256. A full turn is 256 steps. */
static const int SIN_Q[65] = {
    0,   6,   13,  19,  25,  31,  38,  44,  50,  56,  62,  68,  74,
    80,  86,  92,  98,  104, 109, 115, 121, 126, 132, 137, 142, 147,
    152, 157, 162, 167, 172, 177, 181, 185, 190, 194, 198, 202, 206,
    209, 213, 216, 220, 223, 226, 229, 231, 234, 237, 239, 241, 243,
    245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256, 256};

static const int CUBE = 256;
static const int VERTS[8][3] = {
    {-256, -256, -256}, {256, -256, -256}, {-256, 256, -256}, {256, 256, -256},
    {-256, -256, 256},  {256, -256, 256},  {-256, 256, 256},  {256, 256, 256},
};
static const int EDGES[12][2] = {
    {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
    {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

static int isin(int a) {
  int sign = 1;
  a &= 255;
  if (a >= 128) {
    sign = -1;
    a -= 128;
  }
  if (a > 64) {
    a = 128 - a;
  }
  return sign * SIN_Q[a];
}

static int icos(int a) { return isin(a + 64); }

static void rotate(int *x, int *y, int *z, int yaw, int pitch) {
  int s = isin(yaw);
  int c = icos(yaw);
  int nx = (*x * c - *z * s) / 256;
  int nz = (*x * s + *z * c) / 256;
  int ny;

  *x = nx;
  *z = nz;
  s = isin(pitch);
  c = icos(pitch);
  ny = (*y * c - *z * s) / 256;
  nz = (*y * s + *z * c) / 256;
  *y = ny;
  *z = nz;
}

static void project(int x, int y, int z, int *sx, int *sy) {
  int fit = width < height ? width : height;
  int focal = fit * 15 / 16;
  int denom = z + CUBE * 4;

  if (denom < 128) {
    denom = 128;
  }
  *sx = width / 2 + x * focal / denom;
  *sy = height / 2 + y * focal / denom;
}

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  int yaw = (int)(frameCount & 255u);
  int pitch = (int)((frameCount * 3u) & 255u);
  int pts[8][3];
  int depth[12];
  int order[12];
  int i;

  background(bg);
  for (i = 0; i < 8; i++) {
    int x = VERTS[i][0];
    int y = VERTS[i][1];
    int z = VERTS[i][2];
    rotate(&x, &y, &z, yaw, pitch);
    pts[i][0] = x;
    pts[i][1] = y;
    pts[i][2] = z;
  }
  for (i = 0; i < 12; i++) {
    int a = EDGES[i][0];
    int b = EDGES[i][1];
    depth[i] = (pts[a][2] + pts[b][2]) / 2;
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
