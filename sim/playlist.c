#include "graphics.h"
#include "hub75.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each show runs HOLD_FRAMES at the window's 30 fps (30 seconds), then
   the lit pixels travel into the next sketch over MORPH_FRAMES (2 seconds).
   PLAYLIST_HOLD_FRAMES and PLAYLIST_MORPH_FRAMES override those. */

#define HOLD_FRAMES 150
#define MORPH_FRAMES 60

typedef void (*sketch_fn)(void);

void drift_setup(void);
void drift_draw(void);
void lattice_setup(void);
void lattice_draw(void);
void steps_setup(void);
void steps_draw(void);
void cube2_setup(void);
void cube2_draw(void);
void twister_setup(void);
void twister_draw(void);
void raster_setup(void);
void raster_draw(void);
void gray_setup(void);
void gray_draw(void);
void squares_setup(void);
void squares_draw(void);
void hex_setup(void);
void hex_draw(void);
void pulse_setup(void);
void pulse_draw(void);
void bounce_setup(void);
void bounce_draw(void);
void gameboy_setup(void);
void gameboy_draw(void);
void gradient_setup(void);
void gradient_draw(void);

static const struct {
  const char *name;
  sketch_fn setup;
  sketch_fn draw;
} kShows[] = {
    {"drift", drift_setup, drift_draw},
    {"lattice", lattice_setup, lattice_draw},
    {"steps", steps_setup, steps_draw},
    {"cube2", cube2_setup, cube2_draw},
    {"twister", twister_setup, twister_draw},
    {"raster", raster_setup, raster_draw},
    {"gray", gray_setup, gray_draw},
    {"squares", squares_setup, squares_draw},
    {"hex", hex_setup, hex_draw},
    {"pulse", pulse_setup, pulse_draw},
    {"bounce", bounce_setup, bounce_draw},
    {"gameboy", gameboy_setup, gameboy_draw},
    {"gradient", gradient_setup, gradient_draw},
};

static const int kShowCount = (int)(sizeof kShows / sizeof kShows[0]);

typedef struct Dot {
  int16_t x;
  int16_t y;
  uint16_t color;
} Dot;

static int hold_frames = HOLD_FRAMES;
static int morph_frames = MORPH_FRAMES;
static int show_index = 0;
static int morph_next = 0;
static int showing = 1;
static int tick = 0;

static uint16_t *src_img = NULL;
static uint16_t *dst_img = NULL;
static Dot *src_dots = NULL;
static Dot *dst_dots = NULL;
static int src_n = 0;
static int dst_n = 0;

static int env_frames(const char *name, int fallback) {
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

static int cmp_dot(const void *va, const void *vb) {
  const Dot *a = va;
  const Dot *b = vb;
  if (a->y != b->y) {
    return (int)a->y - (int)b->y;
  }
  return (int)a->x - (int)b->x;
}

static int collect(const uint16_t *img, Dot **out) {
  int count = width * height;
  int n = 0;
  int i;
  int w = 0;
  Dot *dots;

  for (i = 0; i < count; i++) {
    if (img[i] != 0) {
      n++;
    }
  }
  if (n < 1) {
    n = 1;
  }
  dots = (Dot *)malloc((size_t)n * sizeof(Dot));
  if (!dots) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    if (img[i] == 0) {
      continue;
    }
    dots[w].x = (int16_t)(i % width);
    dots[w].y = (int16_t)(i / width);
    dots[w].color = img[i];
    w++;
  }
  if (w == 0) {
    dots[0].x = (int16_t)(width / 2);
    dots[0].y = (int16_t)(height / 2);
    dots[0].color = 0;
    w = 1;
  }
  qsort(dots, (size_t)w, sizeof(Dot), cmp_dot);
  free(*out);
  *out = dots;
  return w;
}

static void clear_pixels(void) {
  memset(hub75_pixels(), 0, (size_t)width * (size_t)height * sizeof(uint16_t));
}

static void plot(int x, int y, uint16_t color) {
  if ((unsigned)x >= (unsigned)width || (unsigned)y >= (unsigned)height) {
    return;
  }
  hub75_pixels()[(size_t)y * (size_t)width + (size_t)x] = color;
}

static uint16_t mix565(uint16_t a, uint16_t b, uint64_t num, uint64_t den) {
  int ar = (a >> 11) & 31;
  int ag = (a >> 5) & 63;
  int ab = a & 31;
  int br = (b >> 11) & 31;
  int bg = (b >> 5) & 63;
  int bb = b & 31;
  int r = ar + (int)((int64_t)(br - ar) * (int64_t)num / (int64_t)den);
  int g = ag + (int)((int64_t)(bg - ag) * (int64_t)num / (int64_t)den);
  int bl = ab + (int)((int64_t)(bb - ab) * (int64_t)num / (int64_t)den);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void paint_morph(int step) {
  uint64_t t = (uint64_t)step;
  uint64_t total = (uint64_t)morph_frames;
  uint64_t num = t * t * (3 * total - 2 * t);
  uint64_t den = total * total * total;
  int pairs;
  int i;

  if (den == 0) {
    den = 1;
  }
  pairs = src_n > dst_n ? src_n : dst_n;
  if (pairs < 1 || src_n < 1 || dst_n < 1) {
    return;
  }
  clear_pixels();
  for (i = 0; i < pairs; i++) {
    int si = (pairs == 1) ? 0 : i * (src_n - 1) / (pairs - 1);
    int di = (pairs == 1) ? 0 : i * (dst_n - 1) / (pairs - 1);
    Dot s = src_dots[si];
    Dot d = dst_dots[di];
    int x = s.x + (int)((int64_t)(d.x - s.x) * (int64_t)num / (int64_t)den);
    int y = s.y + (int)((int64_t)(d.y - s.y) * (int64_t)num / (int64_t)den);
    plot(x, y, mix565(s.color, d.color, num, den));
  }
}

static void begin_morph(void) {
  size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);

  morph_next = (show_index + 1) % kShowCount;
  memcpy(src_img, hub75_pixels(), bytes);
  frameCount = 1;
  kShows[morph_next].setup();
  kShows[morph_next].draw();
  memcpy(dst_img, hub75_pixels(), bytes);
  src_n = collect(src_img, &src_dots);
  dst_n = collect(dst_img, &dst_dots);
  memcpy(hub75_pixels(), src_img, bytes);
  showing = 0;
  tick = 0;
  fprintf(stderr, "playlist: %s -> %s\n", kShows[show_index].name,
          kShows[morph_next].name);
}

void setup(void) {
  size_t count = (size_t)width * (size_t)height;

  hold_frames = env_frames("PLAYLIST_HOLD_FRAMES", HOLD_FRAMES);
  morph_frames = env_frames("PLAYLIST_MORPH_FRAMES", MORPH_FRAMES);
  src_img = (uint16_t *)calloc(count, sizeof(uint16_t));
  dst_img = (uint16_t *)calloc(count, sizeof(uint16_t));
  if (!src_img || !dst_img) {
    fprintf(stderr, "playlist: out of memory\n");
    return;
  }
  show_index = 0;
  showing = 1;
  tick = 0;
  frameCount = 0;
  kShows[0].setup();
  fprintf(stderr, "playlist: %s for %d frames, morph %d frames\n",
          kShows[0].name, hold_frames, morph_frames);
}

void draw(void) {
  if (!src_img || !dst_img) {
    return;
  }
  if (!showing) {
    tick++;
    paint_morph(tick);
    if (tick >= morph_frames) {
      show_index = morph_next;
      showing = 1;
      tick = 1;
      frameCount = 1;
      fprintf(stderr, "playlist: %s\n", kShows[show_index].name);
    }
    return;
  }

  tick++;
  frameCount = (unsigned long)tick;
  kShows[show_index].draw();
  if (tick >= hold_frames) {
    begin_morph();
  }
}
