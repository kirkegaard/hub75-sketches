#include "hub75.h"

#include <math.h>

/* One rule: a terminal emulator. A left-right symmetric field is
   rendered as character density: the value at each cell picks a glyph
   from a light-to-heavy ramp, so the glyphs themselves build the
   composition. Most of the wall is gray; the densest parts pick up
   cyan and magenta accents. After the "Terminals" works by ertdfgcvb.

   The glyph bitmaps are the CC0 "3x5 Microfont" by Ella Jameson
   (github.com/nimaid/microfont). One u16 per ASCII code, bit 14 is the
   top-left pixel and bits run left to right, top to bottom. */

static uint16_t bg;

enum { CELL_W = 4, CELL_H = 6, PAD = 2 };

static const char ramp[] = ".:-=+*xX#%@";

enum { RAMP_N = (int)(sizeof(ramp) - 1) };

static const uint16_t font[128] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x2482, 0x5a00, 0x5f7d, 0x3cfa, 0x52a5, 0x3ceb, 0x2400,
    0x1491, 0x4494, 0x5540, 0x05d0, 0x0014, 0x01c0, 0x0002, 0x12a4,
    0x7b6f, 0x2c97, 0x62a7, 0x628e, 0x1779, 0x798e, 0x79ef, 0x72a4,
    0x7bef, 0x7bcf, 0x0410, 0x0414, 0x1511, 0x0e38, 0x4454, 0x72c2,
    0x2b63, 0x2bed, 0x6bae, 0x3923, 0x6b6e, 0x79a7, 0x79a4, 0x396b,
    0x5bed, 0x7497, 0x126a, 0x5bad, 0x4927, 0x5fed, 0x5ffd, 0x2b6a,
    0x6ba4, 0x3b73, 0x6bad, 0x388e, 0x7492, 0x5b6f, 0x5b6a, 0x5bfd,
    0x5aad, 0x5a92, 0x72a7, 0x3493, 0x4889, 0x6496, 0x2a00, 0x0007,
    0x4400, 0x076b, 0x4d6e, 0x0723, 0x176b, 0x0573, 0x15d2, 0x07ce,
    0x49ad, 0x2092, 0x2094, 0x4bad, 0x2491, 0x0bed, 0x0d6d, 0x056a,
    0x0d74, 0x0759, 0x0564, 0x070e, 0x2691, 0x0b6b, 0x0b6a, 0x0b7d,
    0x0a95, 0x0aca, 0x0e67, 0x3513, 0x2492, 0x6456, 0x00f0, 0x0000,
};

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  int cols = (width - 2 * PAD) / CELL_W;
  int rows = (height - 2 * PAD) / CELL_H;
  int origin_x;
  int origin_y;
  float t = (float)frameCount;
  float cxc;
  float cyc;
  int col;
  int row;

  if (cols < 2) {
    cols = 2;
  }
  if (rows < 2) {
    rows = 2;
  }
  cxc = (float)(cols - 1) * 0.5f;
  cyc = (float)(rows - 1) * 0.5f;
  origin_x = PAD + (width - 2 * PAD - (cols * CELL_W - 1)) / 2;
  origin_y = PAD + (height - 2 * PAD - (rows * CELL_H - 1)) / 2;

  background(bg);

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      /* Mirror x about the centre; dy is already symmetric. */
      float dx = cxc > 0.0f ? fabsf((float)col - cxc) / cxc : 0.0f;
      float dy = cyc > 0.0f ? fabsf((float)row - cyc) / cyc : 0.0f;
      float cheb = dx > dy ? dx : dy;
      float rings = 0.5f + 0.5f * cosf(cheb * 16.0f - t * 0.12f);
      float bars = 0.5f + 0.5f * cosf(dx * 26.0f + 0.6f * sinf(t * 0.04f));
      float band = 0.5f + 0.5f * sinf(dy * 13.0f + t * 0.05f);
      float d = rings > bars ? rings : bars;
      int idx;
      char ch;
      uint16_t c;
      int gx = origin_x + col * CELL_W;
      int gy = origin_y + row * CELL_H;
      int r;
      int k;

      d = 0.85f * d + 0.15f * band;
      /* Sharpen into plateaus so the shapes read as bold blocks. */
      d = (d - 0.35f) / 0.4f;
      if (d < 0.0f) {
        d = 0.0f;
      } else if (d > 1.0f) {
        d = 1.0f;
      }
      d = d * d * (3.0f - 2.0f * d);

      idx = (int)(d * (float)(RAMP_N - 1) + 0.5f);
      ch = ramp[idx];

      if (d > 0.82f) {
        float a = sinf(dx * 7.0f + t * 0.1f);
        if (a > 0.5f) {
          c = color(0, 200, 230);
        } else if (a < -0.5f) {
          c = color(230, 40, 200);
        } else {
          c = color(235, 235, 235);
        }
      } else {
        int g = (int)(45.0f + d * 190.0f);
        c = color((uint8_t)g, (uint8_t)g, (uint8_t)g);
      }

      stroke(c);
      for (r = 0; r < 5; r++) {
        for (k = 0; k < 3; k++) {
          if (font[(unsigned char)ch] & (1u << (14 - (r * 3 + k)))) {
            point(gx + k, gy + r);
          }
        }
      }
    }
  }
}
