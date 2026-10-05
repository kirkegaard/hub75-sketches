#include <Adafruit_Protomatter.h>
#include <Arduino.h>

#include <string.h>

#include "pins.h"
#include "portal.h"

extern "C" {
#include "graphics.h"
#include "hub75.h"
}

static Adafruit_Protomatter *matrix = NULL;
static int matrix_started = 0;

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

extern "C" int portal_begin(int w, int h) {
  int addrs;
  static const uint8_t depths[] = {4, 3};
  size_t i;
  int ok = 0;

  addrs = addr_count_for(h);
  if (addrs < 0 || w <= 0) {
    Serial.println("HUB75 rows must be 16, 32, or 64");
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
  if (hub75_gfx_init(w, h) != 0) {
    Serial.println("framebuffer alloc failed");
    return 0;
  }
  frameCount = 0;
  return 1;
}

extern "C" void portal_present(void) {
  uint16_t *dst;
  const uint16_t *src;
  size_t n;

  if (!matrix) {
    return;
  }
  dst = matrix->getBuffer();
  src = hub75_pixels();
  if (!dst || !src) {
    return;
  }
  n = (size_t)width * (size_t)height;
  memcpy(dst, src, n * sizeof(uint16_t));
  matrix->show();
}

extern "C" int buttonUp(void) { return digitalRead(HUB75_BUTTON_UP) == LOW; }

extern "C" int buttonDown(void) {
  return digitalRead(HUB75_BUTTON_DOWN) == LOW;
}
