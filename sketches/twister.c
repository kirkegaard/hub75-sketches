#include "graphics.h"
#include "hub75.h"

#include <math.h>
#include <stdlib.h>

/* Twelve square plates turned about the column, with the same spin and
   the same fly-in as the pharmacy-sign twister. They drop onto the
   column and bounce there before settling. The column runs across the
   wide axis.

   The settled column is centered on the panel. Nudge it with CENTER_X
   and CENTER_Y (pixels; positive is right and down). SPACING is the gap
   between plates. FOV is the horizontal field of view in degrees;
   a smaller number zooms in. PITCH is the downward look in radians:
   0 looks along the plates, about 0.8 looks onto the faces. */

#define SLICES 12
#define HALF_H 0.1f

#define CENTER_X 0.f
#define CENTER_Y 0.f
#define SPACING 1.f
#define FOV 120.f
#define PITCH 0.01f
#define PLATE 1.5f

/* Eye distance. The fly-in is tuned to this. Zoom with FOV. */
#define CAM_Z 5.5f

static const uint8_t kPastel[SLICES][3] = {
    {0xab, 0xde, 0xe6}, {0xcb, 0xaa, 0xcb}, {0xff, 0xff, 0xb5},
    {0xff, 0xcc, 0xb6}, {0xf3, 0xb0, 0xc3}, {0xc6, 0xdb, 0xda},
    {0xfe, 0xe1, 0xe8}, {0xfe, 0xd7, 0xc3}, {0xf6, 0xea, 0xc2},
    {0xec, 0xd5, 0xe3}, {0xff, 0x96, 0x8a}, {0x97, 0xc1, 0xa9},
};

static float *zbuf = NULL;
static int zcap = 0;

static int clamp_byte(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 255) {
    return 255;
  }
  return v;
}

/* Yaw, then tilt the column toward the eye. The tilt pivots on the
   column axis, so the stack stays centered. */
static void place(float c, float s, float pc, float ps, float x, float y,
                  float z, float ty, float *ox, float *oy, float *oz) {
  float wx = x * c - z * s;
  float wy = y + ty;
  float wz = x * s + z * c;

  *ox = wx;
  *oy = wy * pc - wz * ps;
  *oz = wy * ps + wz * pc + CAM_Z;
}

static void fill_tri(float x0, float y0, float z0, float x1, float y1, float z1,
                     float x2, float y2, float z2, uint16_t col) {
  float area;
  int x, y;
  uint16_t *fb;

  area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area < 0.f) {
    float tx = x1, ty = y1, tz = z1;
    x1 = x2;
    y1 = y2;
    z1 = z2;
    x2 = tx;
    y2 = ty;
    z2 = tz;
    area = -area;
  }
  if (area < 0.5f) {
    return;
  }

  fb = hub75_pixels();
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float px = (float)x + 0.5f;
      float py = (float)y + 0.5f;
      float b0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
      float b1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
      float b2 = ((x0 - px) * (y1 - py) - (x1 - px) * (y0 - py)) / area;
      float z;
      int i;

      if (b0 < 0.f || b1 < 0.f || b2 < 0.f) {
        continue;
      }
      z = b0 * z0 + b1 * z1 + b2 * z2;
      i = y * width + x;
      if (z < zbuf[i]) {
        zbuf[i] = z;
        fb[i] = col;
      }
    }
  }
}

/* One side of a plate. Winding matches the original mesh, so the
   back-face test is the same one (skip when the projected cross is positive).
 */
static void side(float c, float s, float pc, float ps, float ty, float x0,
                 float y0, float z0, float x1, float y1, float z1, float x2,
                 float y2, float z2, float x3, float y3, float z3, float nx,
                 float ny, float nz, int pr, int pg, int pb, int slice) {
  float ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz;
  float lnx, lny, lnz;
  float lx, ly, lz, llen, diff;
  float ndc[4][3];
  float xpz;
  int k;
  uint16_t col;
  float sx, sy;
  float scale;

  place(c, s, pc, ps, x0, y0, z0, ty, &ax, &ay, &az);
  place(c, s, pc, ps, x1, y1, z1, ty, &bx, &by, &bz);
  place(c, s, pc, ps, x2, y2, z2, ty, &cx, &cy, &cz);
  place(c, s, pc, ps, x3, y3, z3, ty, &dx, &dy, &dz);
  if (az < 0.5f || bz < 0.5f || cz < 0.5f || dz < 0.5f) {
    return;
  }

  ndc[0][0] = ax / az;
  ndc[0][1] = ay / az;
  ndc[0][2] = az;
  ndc[1][0] = bx / bz;
  ndc[1][1] = by / bz;
  ndc[1][2] = bz;
  ndc[2][0] = cx / cz;
  ndc[2][1] = cy / cz;
  ndc[2][2] = cz;
  ndc[3][0] = dx / dz;
  ndc[3][1] = dy / dz;
  ndc[3][2] = dz;

  xpz = (ndc[1][0] - ndc[0][0]) * (ndc[2][1] - ndc[0][1]) -
        (ndc[1][1] - ndc[0][1]) * (ndc[2][0] - ndc[0][0]);
  if (xpz > 0.f) {
    return;
  }

  lnx = nx * c - nz * s;
  lny = ny;
  lnz = nx * s + nz * c;
  lx = 0.1f;
  ly = 8.f - ty;
  lz = -7.5f;
  llen = sqrtf(lx * lx + ly * ly + lz * lz);
  diff = 0.f;
  if (llen > 0.f) {
    diff = (lnx * lx + lny * ly + lnz * lz) / llen;
  }
  if (diff < 0.f) {
    diff = 0.f;
  }
  diff = 0.42f + 0.58f * diff;
  col = color((uint8_t)clamp_byte((int)(pr * diff)),
              (uint8_t)clamp_byte((int)(pg * diff)),
              (uint8_t)clamp_byte((int)(pb * diff)));

  (void)slice;
  sx = (float)width * 0.5f + CENTER_X;
  sy = (float)height * 0.5f + CENTER_Y;
  scale = ((float)width * 0.5f) / tanf(FOV * 0.5f * 0.0174533f);
  for (k = 0; k < 4; k++) {
    float x = ndc[k][0];
    float y = ndc[k][1];

    ndc[k][0] = sx + scale * y;
    ndc[k][1] = sy + scale * x;
  }
  fill_tri(ndc[0][0], ndc[0][1], ndc[0][2], ndc[1][0], ndc[1][1], ndc[1][2],
           ndc[2][0], ndc[2][1], ndc[2][2], col);
  fill_tri(ndc[0][0], ndc[0][1], ndc[0][2], ndc[2][0], ndc[2][1], ndc[2][2],
           ndc[3][0], ndc[3][1], ndc[3][2], col);
}

static void plate(float c, float s, float pc, float ps, float ty, float x0,
                  float x1, float z0, float z1, int pr, int pg, int pb,
                  int slice) {
  float y0 = -HALF_H;
  float y1 = HALF_H;

  side(c, s, pc, ps, ty, x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0, 0.f,
       1.f, 0.f, pr, pg, pb, slice);
  side(c, s, pc, ps, ty, x1, y0, z0, x1, y0, z1, x0, y0, z1, x0, y0, z0, 0.f,
       -1.f, 0.f, pr, pg, pb, slice);
  side(c, s, pc, ps, ty, x0, y1, z0, x1, y1, z0, x1, y0, z0, x0, y0, z0, 0.f,
       0.f, -1.f, pr, pg, pb, slice);
  side(c, s, pc, ps, ty, x0, y1, z1, x0, y1, z0, x0, y0, z0, x0, y0, z1, -1.f,
       0.f, 0.f, pr, pg, pb, slice);
  side(c, s, pc, ps, ty, x1, y1, z1, x0, y1, z1, x0, y0, z1, x1, y0, z1, 0.f,
       0.f, 1.f, pr, pg, pb, slice);
  side(c, s, pc, ps, ty, x1, y1, z0, x1, y1, z1, x1, y0, z1, x1, y0, z0, 1.f,
       0.f, 0.f, pr, pg, pb, slice);
}

void setup(void) {
  int n = width * height;
  if (n != zcap) {
    free(zbuf);
    zbuf = (float *)calloc((size_t)n, sizeof(float));
    zcap = zbuf ? n : 0;
  }
}

void draw(void) {
  float t;
  float pc;
  float ps;
  int i;
  int s;

  if (!zbuf) {
    return;
  }

  background(color(0, 0, 0));
  for (i = 0; i < zcap; i++) {
    zbuf[i] = 1.0e6f;
  }

  t = (float)frameCount * 33.f + 300.f;
  pc = cosf(PITCH);
  ps = sinf(PITCH);

  for (s = 0; s < SLICES; s++) {
    float y_home = ((float)s - 0.5f * (float)(SLICES - 1)) * SPACING;
    float tsy = t - (float)s * 50.f;

    float y = y_home + (0.1f + 22.f * powf(2.f, -0.004f * tsy)) *
                           (1.f + sinf(tsy / 200.f));

    float latent = 5.f * sinf(t / 10000.f + (float)s / 50.f);
    float main = (1.f - cosf(powf(t / 5000.f, 2.f))) * 4.f *
                 sinf(t / 1000.f - (float)s / 15.f);

    float angle = latent + main;
    float c = cosf(angle);
    float sn = sinf(angle);

    int pr = kPastel[s][0];
    int pg = kPastel[s][1];
    int pb = kPastel[s][2];

    plate(c, sn, pc, ps, y, -PLATE, PLATE, -PLATE, PLATE, pr, pg, pb, s);
  }
}
