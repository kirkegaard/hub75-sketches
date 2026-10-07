#include "graphics.h"

#include "hub75.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

static uint16_t *fb = NULL;
static uint16_t fill_color = 0xFFFF;
static uint16_t stroke_color = 0xFFFF;
static int do_fill = 1;
static int do_stroke = 1;
static int stroke_w = 1;

int hub75_gfx_init(int w, int h) {
  size_t n;
  uint16_t *next;

  if (w <= 0 || h <= 0) {
    return -1;
  }
  n = (size_t)w * (size_t)h;
  if (h != 0 && n / (size_t)h != (size_t)w) {
    return -1;
  }
  next = (uint16_t *)calloc(n, sizeof(uint16_t));
  if (!next) {
    return -1;
  }
  free(fb);
  fb = next;
  width = w;
  height = h;
  return 0;
}

uint16_t *hub75_pixels(void) { return fb; }

void hub75_gfx_reset(void) {
  fill_color = 0xFFFF;
  stroke_color = 0xFFFF;
  do_fill = 1;
  do_stroke = 1;
  stroke_w = 1;
}

uint16_t color(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

void background(uint16_t c) {
  size_t n;
  size_t i;

  if (!fb) {
    return;
  }
  n = (size_t)width * (size_t)height;
  for (i = 0; i < n; i++) {
    fb[i] = c;
  }
}

void fill(uint16_t c) {
  fill_color = c;
  do_fill = 1;
}

void noFill(void) { do_fill = 0; }

void stroke(uint16_t c) {
  stroke_color = c;
  do_stroke = 1;
}

void noStroke(void) { do_stroke = 0; }

void strokeWeight(int weight) {
  if (weight < 1) {
    weight = 1;
  }
  stroke_w = weight;
}

static void put_px(int x, int y, uint16_t c) {
  if (!fb) {
    return;
  }
  if ((unsigned)x >= (unsigned)width || (unsigned)y >= (unsigned)height) {
    return;
  }
  fb[(size_t)y * (size_t)width + (size_t)x] = c;
}

static void stamp(int x, int y, uint16_t c) {
  int r = stroke_w / 2;
  int y0 = y - r;
  int x0 = x - r;
  int yy;
  int xx;

  for (yy = y0; yy < y0 + stroke_w; yy++) {
    for (xx = x0; xx < x0 + stroke_w; xx++) {
      put_px(xx, yy, c);
    }
  }
}

void point(int x, int y) {
  if (!do_stroke) {
    return;
  }
  stamp(x, y, stroke_color);
}

void line(int x0, int y0, int x1, int y1) {
  int dx;
  int sx;
  int dy;
  int sy;
  int err;

  if (!do_stroke) {
    return;
  }
  dx = x1 > x0 ? x1 - x0 : x0 - x1;
  sx = x0 < x1 ? 1 : -1;
  dy = y0 > y1 ? y1 - y0 : y0 - y1;
  sy = y0 < y1 ? 1 : -1;
  err = dx + dy;
  for (;;) {
    int e2;
    stamp(x0, y0, stroke_color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void rect(int x, int y, int w, int h) {
  int xx;
  int yy;
  int band;

  if (w <= 0 || h <= 0) {
    return;
  }
  if (do_fill) {
    for (yy = y; yy < y + h; yy++) {
      for (xx = x; xx < x + w; xx++) {
        put_px(xx, yy, fill_color);
      }
    }
  }
  if (!do_stroke) {
    return;
  }
  for (band = 0; band < stroke_w; band++) {
    int top = y + band;
    int bot = y + h - 1 - band;
    int left = x + band;
    int right = x + w - 1 - band;
    if (top > bot || left > right) {
      break;
    }
    for (xx = x; xx < x + w; xx++) {
      put_px(xx, top, stroke_color);
      put_px(xx, bot, stroke_color);
    }
    for (yy = y; yy < y + h; yy++) {
      put_px(left, yy, stroke_color);
      put_px(right, yy, stroke_color);
    }
  }
}

static void fill_tri(float x0, float y0, float x1, float y1, float x2,
                      float y2) {
  float area;
  int x;
  int y;

  area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area < 0.f) {
    float tx = x1;
    float ty = y1;
    x1 = x2;
    y1 = y2;
    x2 = tx;
    y2 = ty;
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

      if (b0 >= 0.f && b1 >= 0.f && b2 >= 0.f) {
        put_px(x, y, fill_color);
      }
    }
  }
}

/* Every pixel the segment crosses. One rounded sample per step of the
   longer axis skips LEDs on a shallow diagonal, and the fill (pixel
   centers only) leaves those same LEDs dark. */
static void stroke_edge(float x0, float y0, float x1, float y1) {
  double x0d = (double)x0;
  double y0d = (double)y0;
  double dx = (double)x1 - x0d;
  double dy = (double)y1 - y0d;
  int x = (int)floor(x0d);
  int y = (int)floor(y0d);
  int step_x = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
  int step_y = dy > 0.0 ? 1 : (dy < 0.0 ? -1 : 0);
  double t_dx = 0.0;
  double t_dy = 0.0;
  double t_max_x = 2.0;
  double t_max_y = 2.0;
  int guard;

  if (step_x > 0) {
    t_dx = 1.0 / dx;
    t_max_x = (floor(x0d) + 1.0 - x0d) * t_dx;
  } else if (step_x < 0) {
    t_dx = -1.0 / dx;
    t_max_x = (x0d - floor(x0d)) * t_dx;
  }
  if (step_y > 0) {
    t_dy = 1.0 / dy;
    t_max_y = (floor(y0d) + 1.0 - y0d) * t_dy;
  } else if (step_y < 0) {
    t_dy = -1.0 / dy;
    t_max_y = (y0d - floor(y0d)) * t_dy;
  }

  stamp(x, y, stroke_color);
  guard = width + height + 4;
  while (guard-- > 0) {
    if (t_max_x < t_max_y) {
      if (t_max_x > 1.0) {
        break;
      }
      x += step_x;
      t_max_x += t_dx;
    } else {
      if (t_max_y > 1.0) {
        break;
      }
      y += step_y;
      t_max_y += t_dy;
    }
    stamp(x, y, stroke_color);
  }
}

void triangle(float x0, float y0, float x1, float y1, float x2, float y2) {
  if (do_fill) {
    fill_tri(x0, y0, x1, y1, x2, y2);
  }
  if (!do_stroke) {
    return;
  }
  stroke_edge(x0, y0, x1, y1);
  stroke_edge(x1, y1, x2, y2);
  stroke_edge(x2, y2, x0, y0);
}

void polygon(int cx, int cy, float radius, int sides, float rotation) {
  int i;
  int prev_x = 0;
  int prev_y = 0;
  int first_x = 0;
  int first_y = 0;
  float step;

  if (sides < 3 || radius < 1.f) {
    return;
  }
  step = 6.2831853f / (float)sides;
  for (i = 0; i < sides; i++) {
    float a = rotation + step * (float)i;
    int x = (int)((float)cx + cosf(a) * radius);
    int y = (int)((float)cy + sinf(a) * radius);

    if (i == 0) {
      first_x = x;
      first_y = y;
    } else {
      if (do_fill) {
        fill_tri(cx, cy, prev_x, prev_y, x, y);
      }
      if (do_stroke) {
        line(prev_x, prev_y, x, y);
      }
    }
    prev_x = x;
    prev_y = y;
  }
  if (do_fill) {
    fill_tri(cx, cy, prev_x, prev_y, first_x, first_y);
  }
  if (do_stroke) {
    line(prev_x, prev_y, first_x, first_y);
  }
}

void circle(int cx, int cy, int d) {
  int r;
  int r2;
  int inner;
  int inner2;
  int y;
  int x;

  if (d <= 0) {
    return;
  }
  r = d / 2;
  r2 = r * r;
  inner = r - (stroke_w - 1);
  if (inner < 0) {
    inner = 0;
  }
  inner2 = inner * inner;
  for (y = cy - r; y <= cy + r; y++) {
    for (x = cx - r; x <= cx + r; x++) {
      int dx = x - cx;
      int dy = y - cy;
      int dist = dx * dx + dy * dy;
      if (dist > r2) {
        continue;
      }
      if (do_fill) {
        put_px(x, y, fill_color);
      }
      if (do_stroke && dist >= inner2) {
        put_px(x, y, stroke_color);
      }
    }
  }
}
