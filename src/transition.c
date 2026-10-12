#include "transition.h"

#include "util/interp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TRANSITION_FRAMES_PER_DOT 128
#define TRANSITION_MIN_FRAMES 24
#define TRANSITION_MAX_FRAMES 90

/* One lit pixel: where it is and what color, in RGB565. */
typedef struct Hub75Dot {
  int x;
  int y;
  uint16_t color;
} Hub75Dot;

static Hub75Dot *g_from = NULL;
static Hub75Dot *g_to = NULL;
static int g_from_count = 0;
static int g_to_count = 0;
static int g_shared = 0;
static int g_w = 0;
static int g_h = 0;
static int g_frame = 0;
static int g_frames = 0;
static int g_enabled = 1;

/* smoothstep matches the original playlist morph. Any ease curve can be
   swapped in with transition_set_ease(). */
static float ease_default(float x) { return smoothstep(x); }

static Hub75TransitionEase g_ease = ease_default;

static void release(void) {
  free(g_from);
  free(g_to);
  g_from = NULL;
  g_to = NULL;
  g_from_count = 0;
  g_to_count = 0;
  g_shared = 0;
  g_w = 0;
  g_h = 0;
  g_frame = 0;
  g_frames = 0;
}

void transition_cancel(void) { release(); }

void transition_set_enabled(int on) { g_enabled = on ? 1 : 0; }

int transition_enabled(void) { return g_enabled; }

void transition_set_ease(Hub75TransitionEase ease) {
  g_ease = ease ? ease : ease_default;
}

/* Reading order: top to bottom, then left to right. */
static int compare_row_then_column(const void *left, const void *right) {
  const Hub75Dot *a = left;
  const Hub75Dot *b = right;

  if (a->y != b->y) {
    return a->y - b->y;
  }

  return a->x - b->x;
}

/* Gather the lit pixels */
static int collect_dots(const uint16_t *image, Hub75Dot **dots_out) {
  int pixel_count = g_w * g_h;
  int lit = 0;
  int written = 0;
  int i;
  Hub75Dot *dots;

  for (i = 0; i < pixel_count; i++) {
    if (image[i] != 0) {
      lit++;
    }
  }

  free(*dots_out);
  *dots_out = NULL;

  if (lit == 0) {
    return 0;
  }

  dots = (Hub75Dot *)malloc((size_t)lit * sizeof(Hub75Dot));

  if (!dots) {
    return 0;
  }

  for (i = 0; i < pixel_count; i++) {
    if (image[i] == 0) {
      continue;
    }
    dots[written].x = i % g_w;
    dots[written].y = i / g_w;
    dots[written].color = image[i];
    written++;
  }

  qsort(dots, (size_t)written, sizeof(Hub75Dot), compare_row_then_column);
  *dots_out = dots;

  return written;
}

static uint16_t mix_color(uint16_t from, uint16_t to, float t) {
  int from_r = (from >> 11) & 0x1f;
  int from_g = (from >> 5) & 0x3f;
  int from_b = from & 0x1f;

  int to_r = (to >> 11) & 0x1f;
  int to_g = (to >> 5) & 0x3f;
  int to_b = to & 0x1f;

  int r = (int)lroundf((float)from_r + (float)(to_r - from_r) * t);
  int g = (int)lroundf((float)from_g + (float)(to_g - from_g) * t);
  int b = (int)lroundf((float)from_b + (float)(to_b - from_b) * t);

  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void plot(uint16_t *fb, int x, int y, uint16_t color) {
  if ((unsigned)x >= (unsigned)g_w || (unsigned)y >= (unsigned)g_h) {
    return;
  }

  fb[(size_t)y * (size_t)g_w + (size_t)x] = color;
}

int transition_begin(const uint16_t *src, const uint16_t *dst, int w, int h) {
  int in_flight;

  release();

  if (!g_enabled || !src || !dst || w <= 0 || h <= 0) {
    return 0;
  }

  g_w = w;
  g_h = h;

  g_from_count = collect_dots(src, &g_from);
  g_to_count = collect_dots(dst, &g_to);

  /* Nothing lit to land on */
  if (g_to_count == 0) {
    release();
    return 0;
  }

  g_shared = g_from_count < g_to_count ? g_from_count : g_to_count;

  g_frame = 0;
  in_flight = g_from_count > g_to_count ? g_from_count : g_to_count;
  g_frames = in_flight / TRANSITION_FRAMES_PER_DOT;

  if (g_frames < TRANSITION_MIN_FRAMES) {
    g_frames = TRANSITION_MIN_FRAMES;
  }

  if (g_frames > TRANSITION_MAX_FRAMES) {
    g_frames = TRANSITION_MAX_FRAMES;
  }

  return 1;
}

int transition_active(void) { return g_to != NULL && g_frame < g_frames; }

int transition_step(uint16_t *fb) {
  float t;
  int i;

  if (!transition_active() || !fb) {
    return 0;
  }

  /* t reaches 1.0 on the last frame, so the morph lands exactly on dst. */
  t = clamp01(g_ease((float)(g_frame + 1) / (float)g_frames));
  memset(fb, 0, (size_t)g_w * (size_t)g_h * sizeof(uint16_t));

  /* Paired dots slide one axis at a time along an L path, blending color.
     The longer leg goes first, so a dot never cuts a diagonal and a rigid
     shape keeps its form. */
  for (i = 0; i < g_shared; i++) {
    int fx = g_from[i].x;
    int fy = g_from[i].y;
    int tx = g_to[i].x;
    int ty = g_to[i].y;
    int dx = abs(tx - fx);
    int dy = abs(ty - fy);
    int total = dx + dy;
    int horiz_first = dx >= dy;
    float hsplit = total > 0 ? (float)dx / (float)total : 0.5f;
    float x;
    float y;
    uint16_t c = mix_color(g_from[i].color, g_to[i].color, t);

    if (horiz_first) {
      if (t < hsplit) {
        x = lerp((float)fx, (float)tx, hsplit > 0.0f ? t / hsplit : 1.0f);
        y = (float)fy;
      } else {
        x = (float)tx;
        y = lerp((float)fy, (float)ty,
                 hsplit < 1.0f ? (t - hsplit) / (1.0f - hsplit) : 1.0f);
      }
    } else {
      float vsplit = 1.0f - hsplit;
      if (t < vsplit) {
        x = (float)fx;
        y = lerp((float)fy, (float)ty, vsplit > 0.0f ? t / vsplit : 1.0f);
      } else {
        y = (float)ty;
        x = lerp((float)fx, (float)tx,
                 vsplit < 1.0f ? (t - vsplit) / (1.0f - vsplit) : 1.0f);
      }
    }

    plot(fb, (int)lroundf(x), (int)lroundf(y), c);
  }

  /* Extra source dots fade out where they are. */
  for (i = g_shared; i < g_from_count; i++) {
    uint16_t c = mix_color(g_from[i].color, 0, t);

    if (c != 0) {
      plot(fb, g_from[i].x, g_from[i].y, c);
    }
  }

  /* Extra destination dots fade in where they will live. */
  for (i = g_shared; i < g_to_count; i++) {
    uint16_t c = mix_color(0, g_to[i].color, t);

    if (c != 0) {
      plot(fb, g_to[i].x, g_to[i].y, c);
    }
  }

  g_frame++;

  return g_frame < g_frames;
}
