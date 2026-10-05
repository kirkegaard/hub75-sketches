#include "hub75.h"

#include <math.h>

/* The hello-triangle color gradient. One vertex is red, one green,
   one blue, and each pixel inside mixes those three by its
   barycentric weights. triangle() can only fill a flat color, so
   this walks the same pixels and paints them with point().

   The triangle turns slowly. The colors stay stuck to the vertices. */

static const float TAU = 6.2831853f;

static const uint8_t VERTEX_COLOR[3][3] = {
    {0, 0, 255}, /* top, at rest */
    {255, 0, 0}, /* lower right */
    {0, 255, 0}, /* lower left */
};

static uint16_t bg;

static int channel(float v) {
  if (v < 0.f) {
    return 0;
  }
  if (v > 255.f) {
    return 255;
  }
  return (int)(v + 0.5f);
}

static void gradient_triangle(float x0, float y0, float x1, float y1, float x2,
                              float y2, const uint8_t c0[3],
                              const uint8_t c1[3], const uint8_t c2[3]) {
  float area;
  int x;
  int y;
  const uint8_t *k0 = c0;
  const uint8_t *k1 = c1;
  const uint8_t *k2 = c2;

  area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area < 0.f) {
    float tx = x1;
    float ty = y1;
    const uint8_t *tc = k1;
    x1 = x2;
    y1 = y2;
    k1 = k2;
    x2 = tx;
    y2 = ty;
    k2 = tc;
    area = -area;
  }
  if (area < 0.5f) {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float px = (float)x + 0.5f;
      float py = (float)y + 0.5f;
      float b0 = (x1 - px) * (y2 - py) - (x2 - px) * (y1 - py);
      float b1 = (x2 - px) * (y0 - py) - (x0 - px) * (y2 - py);
      float b2 = (x0 - px) * (y1 - py) - (x1 - px) * (y0 - py);
      float w0;
      float w1;
      float w2;

      if (b0 < 0.f || b1 < 0.f || b2 < 0.f) {
        continue;
      }
      w0 = b0 / area;
      w1 = b1 / area;
      w2 = b2 / area;
      stroke(color(
          channel(w0 * (float)k0[0] + w1 * (float)k1[0] + w2 * (float)k2[0]),
          channel(w0 * (float)k0[1] + w1 * (float)k1[1] + w2 * (float)k2[1]),
          channel(w0 * (float)k0[2] + w1 * (float)k1[2] + w2 * (float)k2[2])));
      point(x, y);
    }
  }
}

void setup(void) {
  bg = color(0, 0, 0);
  strokeWeight(1);
}

void draw(void) {
  float radius = (float)(height < width ? height : width) * 0.5f - 1.5f;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float spin = (float)frameCount * 0.008f;
  float vx[3];
  float vy[3];
  int i;

  for (i = 0; i < 3; i++) {
    float a = spin - TAU * (float)i / 3.f;
    vx[i] = cx + cosf(a) * radius;
    vy[i] = cy + sinf(a) * radius;
  }

  background(bg);
  gradient_triangle(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2], VERTEX_COLOR[0],
                    VERTEX_COLOR[1], VERTEX_COLOR[2]);
}
