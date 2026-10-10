#include "hub75.h"
#include "util.h"

#include <math.h>

/* One rule: the panel is packed with tiny glyphs, and a left-right
   symmetric field colours them into a slow, monumental composition.
   The whole text field scrolls vertically, so the characters travel
   along with the colours instead of flickering in place. After the
   "ASCII sanctuary" experiments by Andreas Gysin.

   The glyph bitmaps are the CC0 "3x5 Microfont" by Ella Jameson
   (github.com/nimaid/microfont). One u16 per ASCII code, bit 14 is the
   top-left pixel and bits run left to right, top to bottom. */

static uint16_t bg;

enum { CELL_W = 4, CELL_H = 6, PAD = 2 };

/* Dim slate background up through white, orange, red to magenta. */
static const uint8_t palette[6][3] = {
    {38, 44, 68},  {78, 90, 132},  {222, 228, 236},
    {255, 150, 40}, {226, 46, 46}, {226, 62, 160},
};

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

static const char glyphs[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    ".,:;!?-+*/#%&@";

enum { GLYPH_COUNT = (int)(sizeof(glyphs) - 1) };

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  int cols = (width - 2 * PAD) / CELL_W;
  int rows = (height - 2 * PAD) / CELL_H;
  int origin_x;
  int origin_y;
  float cxc;
  float t = (float)frameCount;
  float scroll = t * 0.06f;
  int col;
  int row;

  if (cols < 1) {
    cols = 1;
  }
  if (rows < 1) {
    rows = 1;
  }
  cxc = (float)(cols - 1) * 0.5f;
  origin_x = PAD + (width - 2 * PAD - (cols * CELL_W - 1)) / 2;
  origin_y = PAD + (height - 2 * PAD - (rows * CELL_H - 1)) / 2;

  background(bg);

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      /* u is mirrored about the centre, so the field stays left-right
         symmetric. The text rides a warped vertical coordinate: a sine
         wave across the width offsets each column, so the glyphs and
         the colours flow together instead of scrolling flat. */
      float u = cxc > 0.0f ? fabsf((float)col - cxc) / cxc : 0.0f;
      float oy = sinf(u * 9.42f + t * 0.08f) * 2.5f;
      float fy = (float)row + scroll + oy;
      float v = fy * 0.35f;
      float f1 = sinf(6.5f * u) * cosf(v);
      float f2 = sinf(4.0f * u + 0.8f * v);
      float val = 0.5f + 0.34f * f1 + 0.16f * f2;
      int level;
      int trow = (int)floorf(fy);
      uint32_t seed = hash_mix(hash_combine2(col, trow));
      uint16_t bits = font[(unsigned char)glyphs[seed % (uint32_t)GLYPH_COUNT]];
      int gx = origin_x + col * CELL_W;
      int gy = origin_y + row * CELL_H;
      int r;
      int c;

      level = (int)(clamp01(val) * 5.999f);

      stroke(color(palette[level][0], palette[level][1], palette[level][2]));
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
