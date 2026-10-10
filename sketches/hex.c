#include "hub75.h"
#include "util.h"

#include <math.h>

/* A honeycomb. Cells are wireframe hexes. A ring of twelve probes,
   steered by noise, knocks nearby cells out. A cell stays dark while
   its life is high, then spins back up to full size. Same life rule as
   the openFrameworks field this comes from. The grid is sized for the
   panel, so the cells stay far enough apart to read. */

#define HEX_MAX 96

typedef struct Hex {
  float x;
  float y;
  int life;
  float noise;
} Hex;

static Hex hexes[HEX_MAX];
static int hex_count;
static float hex_radius;

static float hash2(int x, int y) {
  unsigned int n = (unsigned int)(x * 374761393 + y * 668265263);

  n = (n ^ (n >> 13)) * 1274126177u;
  return (float)(n & 65535u) / 65535.f;
}

static float noise2(float x, float y) {
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  float tx = smoothstep(x - (float)x0);
  float ty = smoothstep(y - (float)y0);
  float a = hash2(x0, y0);
  float b = hash2(x0 + 1, y0);
  float c = hash2(x0, y0 + 1);
  float d = hash2(x0 + 1, y0 + 1);

  return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
}

void setup(void) {
  float radius = (float)height / 8.f;
  float x_span;
  float y_step;
  int row;
  float y;

  if (radius < 4.f) {
    radius = 4.f;
  }
  if (radius > 9.f) {
    radius = 9.f;
  }
  hex_radius = radius;
  x_span = radius * 1.7320508f;
  y_step = radius * 1.5f;
  hex_count = 0;
  row = 0;
  for (y = radius; y <= (float)height - radius && hex_count < HEX_MAX;
       y += y_step, row++) {
    float x0 = (row & 1) ? x_span * 0.5f : 0.f;
    float x;

    for (x = x0; x <= (float)width - radius * 0.2f && hex_count < HEX_MAX;
         x += x_span) {
      Hex *h = &hexes[hex_count];

      h->x = x;
      h->y = y;
      h->life = 0;
      h->noise = hash2(hex_count, 39) * 1000.f;
      hex_count++;
    }
  }
}

void draw(void) {
  float t = (float)frameCount * 0.01f;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float dx = (noise2(1.7f, t) - 0.5f) * (float)width;
  float dy = (noise2(8.3f, t) - 0.5f) * (float)height;
  float reach = hex_radius * 1.35f;
  int i;
  int k;

  background(color(0, 0, 0));

  for (k = 0; k < hex_count; k++) {
    Hex *h = &hexes[k];

    h->life = h->life > 0 ? h->life - 1 : 0;
    if (h->life > 0) {
      h->noise += mapf((float)h->life, 0.f, 100.f, 0.05f, 0.1f);
    }
  }

  for (i = 0; i < 12; i++) {
    float a = (float)i * 30.f * 0.0174533f;
    float px = cx + dx * cosf(a) - dy * sinf(a);
    float py = cy + dx * sinf(a) + dy * cosf(a);

    for (k = 0; k < hex_count; k++) {
      float ox = hexes[k].x - px;
      float oy = hexes[k].y - py;

      if (ox * ox + oy * oy < reach * reach) {
        if (hexes[k].life < 50) {
          hexes[k].life += 4;
        } else {
          hexes[k].life = 50;
        }
      }
    }
  }

  for (k = 0; k < hex_count; k++) {
    Hex *h = &hexes[k];
    float radius;
    float spin;

    if (h->life > 10) {
      continue;
    }

    if (h->life > 0) {
      float n = noise2(h->x * 0.02f, h->noise);

      radius = mapf((float)h->life, 0.f, 10.f, hex_radius * 0.82f, 0.f);
      spin = (n * 2.f - 1.f) * 180.f;
    } else {
      radius = hex_radius * 0.82f;
      spin = 0.f;
    }

    noFill();
    stroke(color(255, 255, 255));
    strokeWeight(1);
    polygon((int)h->x, (int)h->y, radius, 6, (spin + 90.f) * 0.0174533f);
  }
}
