#include "hub75.h"

#include <math.h>

/* Pulsing hexes. Twelve probes ride a noise point, and any cell they
   touch grows by one step, up to the base size. Every other frame the
   field shrinks by one. Cells past a third of full size are orange;
   the smaller ones are cyan. Same rule as the 320x256 demo, with the
   grid fitted to this panel. */

#define PULSE_MAX 120
#define SINE_LEN 1024

typedef struct Cell {
  int x;
  int y;
  int radius;
} Cell;

static Cell cells[PULSE_MAX];
static int cell_count;
static int base_radius;

static int simple_noise(int x, int y, int z) {
  int hash = ((x * 73) + (y * 137) + (z * 241)) & (SINE_LEN - 1);
  float s = sinf((float)hash * (6.2831853f / (float)SINE_LEN));

  return (int)(s * 1024.f + (s >= 0.f ? 0.5f : -0.5f));
}

void setup(void) {
  int r = height / 10;
  int x_span;
  int margin;
  int y;
  int row;

  if (r < 4) {
    r = 4;
  }
  if (r > 7) {
    r = 7;
  }
  base_radius = r;
  x_span = (r * 173) / 100;
  if (x_span < 2) {
    x_span = 2;
  }
  margin = r;
  cell_count = 0;
  row = 0;
  for (y = margin; y <= height - margin && cell_count < PULSE_MAX;
       y += (r * 3) / 2, row++) {
    int x_start = margin + ((row & 1) ? x_span / 2 : 0);
    int x;

    for (x = x_start; x <= width - margin && cell_count < PULSE_MAX;
         x += x_span) {
      cells[cell_count].x = x - width / 2;
      cells[cell_count].y = y - height / 2;
      cells[cell_count].radius = 0;
      cell_count++;
    }
  }
}

void draw(void) {
  int f = (int)frameCount / 2;
  int amp = 100 * base_radius / 12;
  int ring = 20 * base_radius / 12;
  int reach = 30 * base_radius / 12;
  int reach_sq;
  int noise_x;
  int noise_y;
  int source;
  int i;
  uint16_t orange = color(255, 136, 0);
  uint16_t cyan = color(0, 255, 255);

  if (amp < 1) {
    amp = 1;
  }
  if (ring < 1) {
    ring = 1;
  }
  if (reach < 1) {
    reach = 1;
  }
  reach_sq = reach * reach;

  background(color(0, 0, 0));

  for (i = 0; i < cell_count; i++) {
    if (cells[i].radius > 0) {
      cells[i].radius--;
    }
  }

  noise_x = simple_noise(f / 32, f, 0) * amp / 1024;
  noise_y = simple_noise(0, f, f / 16) * amp / 1024;

  for (source = 0; source < 12; source++) {
    float a = (float)source * 30.f * 0.0174533f;
    int sx = noise_x + (int)(cosf(a) * (float)ring);
    int sy = noise_y + (int)(sinf(a) * (float)ring);

    for (i = 0; i < cell_count; i++) {
      int dx = cells[i].x - sx;
      int dy = cells[i].y - sy;

      if (dx * dx + dy * dy < reach_sq) {
        cells[i].radius++;
        if (cells[i].radius > base_radius) {
          cells[i].radius = base_radius;
        }
      }
    }
  }

  for (i = 0; i < cell_count; i++) {
    int radius = cells[i].radius;
    uint16_t col = (radius * 3 > base_radius) ? orange : cyan;

    noFill();
    stroke(col);
    strokeWeight(1);
    polygon(cells[i].x + width / 2, cells[i].y + height / 2, (float)radius, 6,
            0.5235988f);
  }
}
