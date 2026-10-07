#include "hub75.h"

#include <math.h>

/* Port of the "cross" field: a grid of crosses whose size, spin, and
   color ride a plasma field that drifts over time. The panel scales the
   spacing and the arm length down; the color is the same HSL ramp as the
   canvas (hue by field value, half saturation, light). */

#define SEED 42
#define SPEED 0.01f
#define SAT 0.5f
#define LIGHT 0.8f

static uint16_t bg;
static int spacing;
static int max_size;
static int pad;

static float plasma(float x, float y, float t) {
  const float s = SEED * 0.1f;
  float v = 0.f;

  v += sinf(x * 0.3f + t + s);
  v += sinf(y * 0.3f + t * 0.7f + s * 1.3f);
  v += sinf((x + y) * 0.2f + t * 0.5f + s * 0.7f);
  v += sinf(sqrtf(x * x + y * y) * 0.3f + t * 0.8f + s * 1.1f);
  return v * 0.25f;
}

static uint16_t hsl_color(float h, float s, float l) {
  float c = (1.f - fabsf(2.f * l - 1.f)) * s;
  float hp = h / 60.f;
  float x = c * (1.f - fabsf(fmodf(hp, 2.f) - 1.f));
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float m = l - c * 0.5f;
  int seg = ((int)hp) % 6;

  if (seg < 0) {
    seg += 6;
  }
  if (seg == 0) {
    r = c;
    g = x;
  } else if (seg == 1) {
    r = x;
    g = c;
  } else if (seg == 2) {
    g = c;
    b = x;
  } else if (seg == 3) {
    g = x;
    b = c;
  } else if (seg == 4) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }

  return color((uint8_t)((r + m) * 255.f), (uint8_t)((g + m) * 255.f),
               (uint8_t)((b + m) * 255.f));
}

void setup(void) {
  int short_side = width < height ? width : height;

  bg = color(0, 0, 0);
  spacing = short_side / 8;
  if (spacing < 3) {
    spacing = 3;
  }
  max_size = spacing / 2 - 1;
  if (max_size < 1) {
    max_size = 1;
  }
  pad = spacing;
}

void draw(void) {
  float t = (float)frameCount * SPEED;
  int cols = (width - 2 * pad) / spacing;
  int rows = (height - 2 * pad) / spacing;
  int row;
  int col;

  if (cols < 1) {
    cols = 1;
  }
  if (rows < 1) {
    rows = 1;
  }

  background(bg);
  noFill();
  strokeWeight(1);

  for (row = 0; row <= rows; row++) {
    for (col = 0; col <= cols; col++) {
      float n = (plasma((float)col, (float)row, t) + 1.f) * 0.5f;
      int step = (int)lroundf(n * 8.f);
      float rot = (float)step * 0.7853981634f;
      float arm = (float)max_size * n;
      float cs = cosf(rot);
      float sn = sinf(rot);
      int cx = pad + col * spacing;
      int cy = pad + row * spacing;

      stroke(hsl_color(n * 360.f, SAT, LIGHT));
      line((int)lroundf(cx + arm * sn), (int)lroundf(cy - arm * cs),
           (int)lroundf(cx - arm * sn), (int)lroundf(cy + arm * cs));
      line((int)lroundf(cx - arm * cs), (int)lroundf(cy - arm * sn),
           (int)lroundf(cx + arm * cs), (int)lroundf(cy + arm * sn));
    }
  }
}
