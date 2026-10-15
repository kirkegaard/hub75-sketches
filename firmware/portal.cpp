#include <Adafruit_Protomatter.h>
#include <Arduino.h>

#include "config.h"
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

/* The color depth begin() actually got. */
static int matrix_depth = 0;

/* Ordered 4x4 Bayer thresholds, 0..15. */
static const uint8_t kDither[16] = {0, 8,  2,  10, 12, 4, 14, 6,
                                    3, 11, 1,  9,  15, 7, 13, 5};

/* The panel keeps only the top panel_bits of a channel's field, which is
   what makes a gray ramp band. Dither the dropped low bits across a 4x4
   tile: a pixel rounds up when the Bayer threshold falls under its
   fraction, so the tile averages to the level in between. Spatial only,
   so a still image does not shimmer. field_bits is 5 for red and blue, 6
   for green. */
static uint8_t dither_field(uint8_t v8, int panel_bits, int field_bits,
                            int t) {
  int drop = field_bits - panel_bits;
  int vf;
  int level;
  int frac;
  int max;
  int thr;

  if (drop <= 0) {
    return (uint8_t)(v8 >> (8 - field_bits));
  }

  vf = v8 >> (8 - field_bits);
  level = vf >> drop;
  frac = vf & ((1 << drop) - 1);
  max = (1 << panel_bits) - 1;
  thr = (t * (1 << drop)) >> 4;
  if (thr < frac) {
    level++;
  }
  if (level > max) {
    level = max;
  }
  return (uint8_t)(level << drop);
}

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
  matrix_depth = depth;

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
  int depth;
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

  /* Start at the requested depth and step down until one starts. More
     bitplanes give more levels but refresh slower. */
  for (depth = HUB75_DEPTH > 6 ? 6 : HUB75_DEPTH; depth >= 3 && !ok;
       depth--) {
    if (try_begin(w, h, addrs, (uint8_t)depth, true) ||
        try_begin(w, h, addrs, (uint8_t)depth, false)) {
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
  int panel_r;
  int panel_g;
  int panel_b;
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

  /* Protomatter keeps only the top matrix_depth bits of each 5/6/5 field. */
  panel_r = matrix_depth < 5 ? matrix_depth : 5;
  panel_g = matrix_depth < 6 ? matrix_depth : 6;
  panel_b = matrix_depth < 5 ? matrix_depth : 5;

  for (ly = 0; ly < height; ly++) {
    for (lx = 0; lx < width; lx++) {
      int ex;
      int ey;
      uint16_t c;
      uint8_t r8;
      uint8_t g8;
      uint8_t b8;
      uint8_t r5;
      uint8_t g6;
      uint8_t b5;
      int t;

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

      c = hub75_dim565(
          hub75_correct565(src[(size_t)ly * (size_t)width + (size_t)lx]),
          brightness);

      /* Expand to 8-bit the way the simulator does, dither the bits the
         panel drops, and repack. */
      r8 = (uint8_t)((c >> 11) & 0x1Fu);
      g8 = (uint8_t)((c >> 5) & 0x3Fu);
      b8 = (uint8_t)(c & 0x1Fu);
      r8 = (uint8_t)((r8 << 3) | (r8 >> 2));
      g8 = (uint8_t)((g8 << 2) | (g8 >> 4));
      b8 = (uint8_t)((b8 << 3) | (b8 >> 2));

      t = kDither[((ey & 3) << 2) | (ex & 3)];

      r5 = dither_field(r8, panel_r, 5, t);
      g6 = dither_field(g8, panel_g, 6, t);
      b5 = dither_field(b8, panel_b, 5, t);

      dst[(size_t)ey * (size_t)phys_w + (size_t)ex] =
          (uint16_t)((r5 << 11) | (g6 << 5) | b5);
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
