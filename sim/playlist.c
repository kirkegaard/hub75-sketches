#include "graphics.h"
#include "hub75.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each sketch holds for HOLD_FRAMES at the window's 30 fps (10 seconds),
   then lit dots morph for MORPH_FRAMES (2 seconds).
   PLAYLIST_HOLD_FRAMES and PLAYLIST_MORPH_FRAMES override those. */

#define HOLD_FRAMES 300
#define MORPH_FRAMES 60

typedef void (*sketch_fn)(void);

void drift_setup(void);
void drift_draw(void);
void lattice_setup(void);
void lattice_draw(void);
void steps_setup(void);
void steps_draw(void);
void triangle_setup(void);
void triangle_draw(void);
void cube_setup(void);
void cube_draw(void);

typedef struct Show {
  const char *name;
  sketch_fn setup;
  sketch_fn draw;
} Show;

static const Show shows[] = {
    {"drift", drift_setup, drift_draw},
    {"cube", cube_setup, cube_draw},
    {"lattice", lattice_setup, lattice_draw},
    {"steps", steps_setup, steps_draw},
    {"triangle", triangle_setup, triangle_draw},
};

static const int show_count = (int)(sizeof shows / sizeof shows[0]);

typedef struct Dot {
  int16_t x;
  int16_t y;
  uint16_t color;
} Dot;

static int hold_frames = HOLD_FRAMES;
static int morph_frames = MORPH_FRAMES;
static int show_index = 0;
static int next_show = 0;
static int morphing = 0;
static int hold_tick = 0;
static int morph_tick = 0;

static uint16_t *src_image = NULL;
static uint16_t *dst_image = NULL;
static Dot *src_dots = NULL;
static Dot *dst_dots = NULL;
static int src_count = 0;
static int dst_count = 0;
static int *src_join = NULL;
static int *dst_join = NULL;

static int frames_from_env(const char *name, int fallback) {
  const char *text = getenv(name);
  int value;

  if (!text || text[0] == '\0') {
    return fallback;
  }

  value = atoi(text);

  if (value < 1) {
    return fallback;
  }

  return value;
}

static int compare_row_then_column(const void *left, const void *right) {
  const Dot *a = left;
  const Dot *b = right;

  if (a->y != b->y) {
    return (int)a->y - (int)b->y;
  }

  return (int)a->x - (int)b->x;
}

static int collect_lit_dots(const uint16_t *image, Dot **dots_out) {
  int pixel_count = width * height;
  int lit = 0;
  int written = 0;
  int i;
  Dot *dots;

  for (i = 0; i < pixel_count; i++) {
    if (image[i] != 0) {
      lit++;
    }
  }

  if (lit == 0) {
    free(*dots_out);
    *dots_out = NULL;
    return 0;
  }

  dots = (Dot *)malloc((size_t)lit * sizeof(Dot));

  if (!dots) {
    return 0;
  }

  for (i = 0; i < pixel_count; i++) {
    if (image[i] == 0) {
      continue;
    }

    dots[written].x = (int16_t)(i % width);
    dots[written].y = (int16_t)(i / width);
    dots[written].color = image[i];
    written++;
  }

  qsort(dots, (size_t)written, sizeof(Dot), compare_row_then_column);
  free(*dots_out);
  *dots_out = dots;
  return written;
}

static void clear_pixels(void) {
  size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);

  memset(hub75_pixels(), 0, bytes);
}

static void plot(int x, int y, uint16_t color) {
  if ((unsigned)x >= (unsigned)width || (unsigned)y >= (unsigned)height) {
    return;
  }

  hub75_pixels()[(size_t)y * (size_t)width + (size_t)x] = color;
}

static uint16_t mix_color(uint16_t from, uint16_t to, uint64_t num,
                          uint64_t den) {
  int from_r = (from >> 11) & 31;
  int from_g = (from >> 5) & 63;
  int from_b = from & 31;
  int to_r = (to >> 11) & 31;
  int to_g = (to >> 5) & 63;
  int to_b = to & 31;
  int red =
      from_r + (int)((int64_t)(to_r - from_r) * (int64_t)num / (int64_t)den);
  int green =
      from_g + (int)((int64_t)(to_g - from_g) * (int64_t)num / (int64_t)den);
  int blue =
      from_b + (int)((int64_t)(to_b - from_b) * (int64_t)num / (int64_t)den);

  return (uint16_t)((red << 11) | (green << 5) | blue);
}

static void smoothstep(int step, uint64_t *numerator, uint64_t *denominator) {
  uint64_t t = (uint64_t)step;
  uint64_t total = (uint64_t)morph_frames;

  *numerator = t * t * (3 * total - 2 * t);
  *denominator = total * total * total;

  if (*denominator == 0) {
    *denominator = 1;
  }
}

static void plot_along(int from_x, int from_y, int to_x, int to_y, uint16_t ink,
                       uint64_t num, uint64_t den) {
  int64_t moved_x = (int64_t)(to_x - from_x) * (int64_t)num / (int64_t)den;
  int64_t moved_y = (int64_t)(to_y - from_y) * (int64_t)num / (int64_t)den;

  plot(from_x + (int)moved_x, from_y + (int)moved_y, ink);
}

static void plot_traveler(Dot from, Dot to, uint64_t num, uint64_t den) {
  uint16_t color = mix_color(from.color, to.color, num, den);

  plot_along(from.x, from.y, to.x, to.y, color, num, den);
}

static int nearest_filled(int x, int y, const Dot *dots, int count) {
  int best = 0;
  int best_dist = -1;
  int i;

  for (i = 0; i < count; i++) {
    int dx = (int)dots[i].x - x;
    int dy = (int)dots[i].y - y;
    int dist = dx * dx + dy * dy;

    if (best_dist < 0 || dist < best_dist) {
      best = i;
      best_dist = dist;
    }
  }

  return best;
}

static int paired_count(void) {
  if (src_count < dst_count) {
    return src_count;
  }
  return dst_count;
}

/* Extra dots have no partner of their own. Each one joins the nearest
   dot that does, so it moves to a LED that is already filled. */
static void match_extras(void) {
  int shared = paired_count();
  int i;
  int *src_next = NULL;
  int *dst_next = NULL;

  if (src_count > 0) {
    src_next = (int *)malloc((size_t)src_count * sizeof(int));
  }
  if (dst_count > 0) {
    dst_next = (int *)malloc((size_t)dst_count * sizeof(int));
  }

  if ((src_count > 0 && !src_next) || (dst_count > 0 && !dst_next)) {
    free(src_next);
    free(dst_next);
    free(src_join);
    free(dst_join);
    src_join = NULL;
    dst_join = NULL;
    return;
  }

  free(src_join);
  free(dst_join);
  src_join = src_next;
  dst_join = dst_next;

  for (i = shared; i < src_count; i++) {
    if (shared < 1) {
      src_join[i] = -1;
      continue;
    }
    src_join[i] = nearest_filled(src_dots[i].x, src_dots[i].y, dst_dots, shared);
  }

  for (i = shared; i < dst_count; i++) {
    if (shared < 1) {
      dst_join[i] = -1;
      continue;
    }
    dst_join[i] = nearest_filled(dst_dots[i].x, dst_dots[i].y, src_dots, shared);
  }
}

static void plot_leave(int index, uint64_t num, uint64_t den) {
  Dot dot;
  Dot dest;
  int join;

  if (!src_join || src_join[index] < 0) {
    return;
  }

  dot = src_dots[index];
  join = src_join[index];
  dest = dst_dots[join];
  plot_along(dot.x, dot.y, dest.x, dest.y, dot.color, num, den);
}

static void plot_arrive(int index, uint64_t num, uint64_t den) {
  Dot dot;
  Dot origin;
  int join;

  dot = dst_dots[index];

  if (!dst_join || dst_join[index] < 0) {
    plot(dot.x, dot.y, dot.color);
    return;
  }

  join = dst_join[index];
  origin = src_dots[join];
  plot_along(origin.x, origin.y, dot.x, dot.y, dot.color, num, den);
}

static void paint_morph(int step) {
  uint64_t num;
  uint64_t den;
  int shared;
  int i;

  smoothstep(step, &num, &den);
  clear_pixels();
  shared = paired_count();

  for (i = shared; i < src_count; i++) {
    plot_leave(i, num, den);
  }

  for (i = shared; i < dst_count; i++) {
    plot_arrive(i, num, den);
  }

  for (i = 0; i < shared; i++) {
    plot_traveler(src_dots[i], dst_dots[i], num, den);
  }
}

static void copy_pixels(uint16_t *dest) {
  size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);

  memcpy(dest, hub75_pixels(), bytes);
}

static void begin_morph(void) {
  size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);

  next_show = (show_index + 1) % show_count;
  copy_pixels(src_image);

  frameCount = 1;
  shows[next_show].setup();
  shows[next_show].draw();
  copy_pixels(dst_image);

  src_count = collect_lit_dots(src_image, &src_dots);
  dst_count = collect_lit_dots(dst_image, &dst_dots);
  match_extras();

  memcpy(hub75_pixels(), src_image, bytes);
  morphing = 1;
  morph_tick = 0;
  fprintf(stderr, "playlist: %s -> %s\n", shows[show_index].name,
          shows[next_show].name);
}

void setup(void) {
  size_t count = (size_t)width * (size_t)height;

  hold_frames = frames_from_env("PLAYLIST_HOLD_FRAMES", HOLD_FRAMES);
  morph_frames = frames_from_env("PLAYLIST_MORPH_FRAMES", MORPH_FRAMES);
  src_image = (uint16_t *)calloc(count, sizeof(uint16_t));
  dst_image = (uint16_t *)calloc(count, sizeof(uint16_t));

  if (!src_image || !dst_image) {
    fprintf(stderr, "playlist: out of memory\n");
    return;
  }

  show_index = 0;
  morphing = 0;
  hold_tick = 0;
  morph_tick = 0;
  frameCount = 0;
  shows[0].setup();
  fprintf(stderr, "playlist: %s, hold %d frames, morph %d frames\n",
          shows[0].name, hold_frames, morph_frames);
}

void draw(void) {
  if (!src_image || !dst_image) {
    return;
  }

  if (morphing) {
    morph_tick++;
    paint_morph(morph_tick);

    if (morph_tick >= morph_frames) {
      show_index = next_show;
      morphing = 0;
      /* The last morph frame is the incoming sketch's first frame. */
      hold_tick = 1;
      frameCount = 1;
      fprintf(stderr, "playlist: %s\n", shows[show_index].name);
    }

    return;
  }

  hold_tick++;
  frameCount = (unsigned long)hold_tick;
  shows[show_index].draw();

  if (hold_tick >= hold_frames) {
    begin_morph();
  }
}
