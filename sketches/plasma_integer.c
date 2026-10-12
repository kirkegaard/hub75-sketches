#include "hub75.h"

/* One rule: a flowing plasma. Four integer sine waves -- two that scroll
   along x and y, one along the diagonal, and a zone-plate term from the
   radius -- beat against each other, and the sum walks a muted rainbow:
   indigo, blue, cyan, teal, yellow, orange, red, magenta, back through
   violet. No lookup table and no float, so it fits the CH552 matrix as
   well as the big panel. Its libm twin is plasma_float. */

/* A soft loop rather than a full-saturation hue wheel, in the spirit of
   sanctuary: the dark indigo gives the plasma depth, and the ends meet
   so there is no seam. */
#define PAL_N 9
static const uint8_t pal[PAL_N][3] = {
    {24, 20, 56},   {46, 70, 150},  {70, 170, 210}, {120, 210, 170},
    {230, 220, 120}, {240, 140, 60}, {220, 60, 70},  {200, 70, 180},
    {110, 50, 150},
};

/* Parabolic approximation of sine: a runs 0..255 as one full turn and
   the result walks -128..128. Cheap, smooth enough, integer only. */
static int isin(int a) {
  int x;
  int t;
  int y;

  a &= 255;
  x = a < 128 ? a : a - 256;
  t = x < 0 ? -x : x;
  y = (t * (128 - t)) >> 5;
  return x < 0 ? -y : y;
}

static uint8_t mix8(int a, int b, int f) {
  return (uint8_t)(a + (int)(((long)(b - a) * f) >> 8));
}

void setup(void) {}

void draw(void) {
  int t = (int)((frameCount * 2u) & 255u);
  int y;
  int x;

  background(color(0, 0, 0));
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int p = isin(x * 20 + t) +
              isin(y * 20 - t) +
              isin((x + y) * 14 + t * 2) +
              isin((x * x + y * y) * 2 - t * 3);
      int hue = (p + 512) >> 2;
      int pos;
      int idx;
      int frac;
      int nxt;

      if (hue > 255) {
        hue = 255;
      }
      pos = hue * PAL_N;
      idx = pos >> 8;
      frac = pos & 255;
      nxt = idx + 1 < PAL_N ? idx + 1 : 0;

      stroke(color(mix8(pal[idx][0], pal[nxt][0], frac),
                   mix8(pal[idx][1], pal[nxt][1], frac),
                   mix8(pal[idx][2], pal[nxt][2], frac)));
      point(x, y);
    }
  }
}
