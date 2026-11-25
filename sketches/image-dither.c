#include "hub75.h"
#include "image.h"
#include "util/font3x5.h"

#include <stdlib.h>

/* Load a photo, fit it to the panel, then cycle through dither
   algorithms. The simulator reads HUB75_IMAGE for the path, HUB75_MODE
   to start on a specific mode, and HUB75_CYCLE=0 to stay there. The
   same source runs on the board, but the board has no filesystem, so
   image_load fails and the sketch shows a red X. Use image_load_memory
   with an embedded asset for firmware. */

#define IMAGE_PATH "tmp/picture-resized.jpg"
#define HOLD_MS 3500

typedef struct DemoMode {
  const char *name;
  DitherAlgorithm dither;
  int mono;
  int levels;
  int gray;
  int duotone;
} DemoMode;

static const DemoMode kModes[] = {
    {"bayer2 color", DITHER_BAYER_2, 0, 0, 0, 0},
    {"bayer4 color", DITHER_BAYER_4, 0, 0, 0, 0},
    {"bayer8 color", DITHER_BAYER_8, 0, 0, 0, 0},
    {"halftone", DITHER_HALFTONE_4, 0, 0, 0, 0},
    {"blue noise", DITHER_BLUE_NOISE_16, 0, 0, 0, 0},
    {"floyd color", DITHER_FLOYD_STEINBERG, 0, 0, 0, 0},
    {"atkinson", DITHER_ATKINSON, 0, 0, 0, 0},
    {"stucki", DITHER_STUCKI, 0, 0, 0, 0},
    {"burkes", DITHER_BURKES, 0, 0, 0, 0},
    {"jarvis", DITHER_JARVIS, 0, 0, 0, 0},
    {"sierra lite", DITHER_SIERRA_LITE, 0, 0, 0, 0},
    {"gray bayer", DITHER_BAYER_4, 0, 0, 1, 0},
    {"gray floyd", DITHER_FLOYD_STEINBERG, 0, 0, 1, 0},
    {"bayer4 mono", DITHER_BAYER_4, 1, 2, 0, 0},
    {"floyd mono", DITHER_FLOYD_STEINBERG, 1, 2, 0, 0},
    {"atkinson mono", DITHER_ATKINSON, 1, 2, 0, 0},
    {"blue noise 1b", DITHER_BLUE_NOISE_16, 1, 2, 0, 0},
    {"bayer4 gray", DITHER_BAYER_4, 1, 4, 0, 0},
    {"gameboy bayer", DITHER_BAYER_4, 1, 2, 0, 1},
    {"gameboy floyd", DITHER_FLOYD_STEINBERG, 1, 2, 0, 1},
};

enum { MODE_COUNT = (int)(sizeof(kModes) / sizeof(kModes[0])) };

static Image base;
static Image shown;
static int loaded;
static int mode;
static int cycle = 1;
static unsigned long next_mode;

static void draw_label(const char *text) {
  int x = 2;
  int y = height - 7;
  int width_px = 0;
  int i;
  int row;
  int col;

  if (height < 12 || width < 40) {
    return;
  }
  for (i = 0; text[i] != '\0'; i++) {
    width_px += 4;
  }

  noStroke();
  fill(color(0, 0, 0));
  rect(0, y - 1, width_px + 2, 7);

  stroke(color(255, 255, 255));
  strokeWeight(1);
  for (i = 0; text[i] != '\0'; i++) {
    uint16_t bits = font3x5[(unsigned char)text[i]];

    for (row = 0; row < 5; row++) {
      for (col = 0; col < 3; col++) {
        if (bits & (uint16_t)(1u << (14 - (row * 3 + col)))) {
          point(x + col, y + row);
        }
      }
    }
    x += 4;
  }
}

static void rebuild(void) {
  if (!loaded) {
    return;
  }
  if (image_copy(&shown, &base) != 0) {
    return;
  }

  if (kModes[mode].gray) {
    image_grayscale(&shown);
  }
  image_dither(&shown, kModes[mode].dither, kModes[mode].levels,
               kModes[mode].mono);

  if (kModes[mode].duotone) {
    image_duotone(&shown, color(15, 56, 15), color(155, 188, 15));
  }
}

void setup(void) {
  const char *path = IMAGE_PATH;

  image_init(&base);
  image_init(&shown);

  mode = 0;
#ifndef HUB75_FIRMWARE
  {
    const char *pick = getenv("HUB75_MODE");
    const char *hold = getenv("HUB75_CYCLE");
    const char *image = getenv("HUB75_IMAGE");

    if (pick && pick[0] != '\0') {
      mode = atoi(pick);
    }
    if (hold && hold[0] == '0') {
      cycle = 0;
    }
    if (image && image[0] != '\0') {
      path = image;
    }
  }
#endif

  loaded = image_load(&base, path) == 0;
  if (loaded) {
    image_fit(&base, width, height, IMAGE_COVER, IMAGE_BOX);
  }
  if (mode < 0 || mode >= MODE_COUNT) {
    mode = 0;
  }
  rebuild();
  next_mode = millis() + HOLD_MS;
}

void draw(void) {
  unsigned long now = millis();

  if (!loaded) {
    background(color(16, 0, 0));
    stroke(color(255, 80, 80));
    strokeWeight(1);
    line(0, 0, width - 1, height - 1);
    line(0, height - 1, width - 1, 0);
    return;
  }

  if (cycle && (long)(now - next_mode) >= 0) {
    mode = (mode + 1) % MODE_COUNT;
    next_mode += HOLD_MS;
    rebuild();
  }

  background(color(0, 0, 0));
  image_draw(&shown, 0, 0);
  draw_label(kModes[mode].name);
}
