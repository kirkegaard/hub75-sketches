#include "hub75.h"

#include <math.h>

/* One rule: every cell picks a character from a short pattern by its
   reading index, warped by a slow sine. Ported from the textmode sketch
   by ertdfgcvb ("Time: milliseconds"): the index is x + y plus a
   sinusoidal offset, so the pattern flows across the panel in wavy
   diagonals.

   The glyph bitmaps are the CC0 "3x5 Microfont" by Ella Jameson
   (github.com/nimaid/microfont). One u16 per ASCII code, bit 14 is the
   top-left pixel and bits run left to right, top to bottom. */

static uint16_t bg;

enum { CELL_W = 4, CELL_H = 6, PAD = 2 };

/* The original pattern uses U+2550 for the long line; '=' stands in. */
static const char pattern[] = "ABCxyz01=|+:. ";

enum { PATTERN_LEN = (int)(sizeof(pattern) - 1) };

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
  /* context.time is milliseconds; the window and firmware both step one
     frame every 33 ms, so frameCount * 33 stands in. */
  float t = (float)frameCount * 33.0f * 0.0001f;
  float st = sinf(t);
  int col;
  int row;

  if (cols < 1) {
    cols = 1;
  }
  if (rows < 1) {
    rows = 1;
  }
  origin_x = PAD + (width - 2 * PAD - (cols * CELL_W - 1)) / 2;
  origin_y = PAD + (height - 2 * PAD - (rows * CELL_H - 1)) / 2;

  background(bg);
  stroke(color(255, 255, 255));

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      float o = sinf((float)row * st * 0.2f + (float)col * 0.04f + t) * 20.0f;
      int i = (int)lroundf(fabsf((float)(col + row) + o)) % PATTERN_LEN;
      char ch = pattern[i];
      uint16_t bits;
      int gx;
      int gy;
      int r;
      int c;

      if (ch == ' ') {
        continue;
      }
      bits = font[(unsigned char)ch];
      gx = origin_x + col * CELL_W;
      gy = origin_y + row * CELL_H;
      for (r = 0; r < 5; r++) {
        for (c = 0; c < 3; c++) {
          if (bits & (1u << (14 - (r * 3 + c)))) {
            point(gx + c, gy + r);
          }
        }
      }
    }
  }
}
