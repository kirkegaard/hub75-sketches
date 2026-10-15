#include <Adafruit_Protomatter.h>
#include <Arduino.h>

#include "pins.h"
#include "portal.h"

extern "C" {
#include "brightness.h"
#include "color.h"
#include "graphics.h"
#include "hub75.h"
}

static Adafruit_Protomatter *matrix = NULL;
static int matrix_started = 0;

/* The physical panel width (Protomatter's canvas stride) and how the
   sketch framebuffer is rotated onto the panel. A rotated panel keeps
   its electrical canvas; ROTATE reorients the sketch's width x height
   view. */
static int phys_w = 0;
static int rotation = 0;

static uint8_t rgb_pins[] = {HUB75_R1, HUB75_G1, HUB75_B1,
                             HUB75_R2, HUB75_G2, HUB75_B2};
static uint8_t addr_pins[] = {HUB75_A, HUB75_B, HUB75_C, HUB75_D, HUB75_E};

static int addr_count_for(int h) {
  if (h == 16) {
    return 3;
  }

  if (h == 32) {
    return 4;
  }

  if (h == 64) {
    return 5;
  }

  return -1;
}

static void discard_matrix(void) {
  if (!matrix) {
    return;
  }

  if (matrix_started) {
    matrix->stop();
  }

  delete matrix;
  matrix = NULL;
  matrix_started = 0;
}

/* Protomatter's canvas height is (2 << addrCount) for one chain and one tile.
   begin() is what actually claims the pins; a mismatch here means the
   address count and the requested height disagree. */
static int try_begin(int w, int h, int addrs, uint8_t depth,
                     bool double_buffer) {
  ProtomatterStatus status;

  discard_matrix();

  matrix = new Adafruit_Protomatter((uint16_t)w, depth, 1, rgb_pins,
                                    (uint8_t)addrs, addr_pins, HUB75_CLK,
                                    HUB75_LAT, HUB75_OE, double_buffer);
  if (!matrix) {
    return 0;
  }

  status = matrix->begin();

  if (status != PROTOMATTER_OK) {
    Serial.printf("protomatter begin failed: depth %u %s status %d\n",
                  (unsigned)depth, double_buffer ? "double" : "single",
                  (int)status);
    delete matrix;
    matrix = NULL;
    return 0;
  }

  matrix_started = 1;

  if (matrix->width() != w || matrix->height() != h) {
    Serial.printf("protomatter reported %d x %d, wanted %d x %d\n",
                  matrix->width(), matrix->height(), w, h);
    discard_matrix();
    return 0;
  }

  Serial.printf("protomatter %d x %d, depth %u, %s buffer\n", w, h,
                (unsigned)depth, double_buffer ? "double" : "single");

  return 1;
}

extern "C" int portal_begin(int w, int h, int rotate) {
  int addrs;
  int lw;
  int lh;
  static const uint8_t depths[] = {4, 3};
  size_t i;
  int ok = 0;

  if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270) {
    Serial.printf("rotation must be 0, 90, 180, or 270, got %d\n", rotate);
    return 0;
  }

  addrs = addr_count_for(h);

  if (addrs < 0 || w <= 0) {
    Serial.printf("HUB75 rows must be 16, 32, or 64, got %d\n", h);
    Serial.println("a single panel cannot be taller than 64 rows; use ROTATE");
    return 0;
  }

  pinMode(HUB75_BUTTON_UP, INPUT_PULLUP);
  pinMode(HUB75_BUTTON_DOWN, INPUT_PULLUP);

  for (i = 0; i < sizeof depths / sizeof depths[0] && !ok; i++) {
    if (try_begin(w, h, addrs, depths[i], true) ||
        try_begin(w, h, addrs, depths[i], false)) {
      ok = 1;
    }
  }

  if (!ok) {
    Serial.println("HUB75 panel did not start");
    return 0;
  }

  /* 90 and 270 trade width for height: a 128x64 panel shows a 64x128
     sketch canvas, which is what a physically rotated panel needs. */
  lw = (rotate == 90 || rotate == 270) ? h : w;
  lh = (rotate == 90 || rotate == 270) ? w : h;

  if (hub75_gfx_init(lw, lh) != 0) {
    Serial.println("framebuffer alloc failed");
    return 0;
  }

  phys_w = w;
  rotation = rotate;
  frameCount = 0;

  return 1;
}

extern "C" void portal_present(void) {
  uint16_t *dst;
  const uint16_t *src;
  int brightness;
  int lx;
  int ly;

  if (!matrix) {
    return;
  }

  dst = matrix->getBuffer();
  src = hub75_pixels();

  if (!dst || !src) {
    return;
  }

  brightness = hub75_brightness();

  for (ly = 0; ly < height; ly++) {
    for (lx = 0; lx < width; lx++) {
      int ex;
      int ey;

      switch (rotation) {
      case 90:
        ex = height - 1 - ly;
        ey = lx;
        break;
      case 180:
        ex = width - 1 - lx;
        ey = height - 1 - ly;
        break;
      case 270:
        ex = ly;
        ey = width - 1 - lx;
        break;
      default:
        ex = lx;
        ey = ly;
        break;
      }

      dst[(size_t)ey * (size_t)phys_w + (size_t)ex] = hub75_dim565(
          hub75_correct565(src[(size_t)ly * (size_t)width + (size_t)lx]),
          brightness);
    }
  }

  matrix->show();
}

extern "C" int portal_button_up(void) {
  return digitalRead(HUB75_BUTTON_UP) == LOW;
}

extern "C" int portal_button_down(void) {
  return digitalRead(HUB75_BUTTON_DOWN) == LOW;
}

extern "C" void portal_brightness_up(void) { hub75_brightness_up(); }

extern "C" void portal_brightness_down(void) { hub75_brightness_down(); }
