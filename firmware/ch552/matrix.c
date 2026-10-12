/* WS2812 backend for the CH552 (ch55xduino). The sketch and the shared
   RGB565 graphics code are identical to the simulator and the ESP32
   firmware; only this file, which turns the framebuffer into the bytes a
   WS2812 matrix wants, is board specific.

   The matrix is a GRB part, one byte per channel, three bytes per LED.
   ch55xduino's neopixel_show_* passes the length in the low byte of a
   32-bit argument, so the whole panel must be 85 LEDs or fewer
   (8x8 = 64 LEDs = 192 bytes). */

#include "matrix.h"

#include <Arduino.h>
#include <WS2812.h>

#include <stdint.h>

#include "config.h"
#include "graphics.h"
#include "hub75.h"

/* WS2812 data lives in external RAM; that is where the CH552 keeps the
   buffers it bit-bangs. Sized by the build's panel. */
static __xdata uint8_t led_bytes[HUB75_PANEL_WIDTH * HUB75_PANEL_HEIGHT * 3];

int matrix_begin(int w, int h) {
  if (hub75_gfx_init(w, h) != 0) {
    return 0;
  }
  frameCount = 0;
  return 1;
}

void matrix_present(void) {
  const uint16_t *fb = hub75_pixels();
  unsigned n;
  unsigned i;

  if (!fb) {
    return;
  }
  n = (unsigned)width * (unsigned)height;
  for (i = 0; i < n; i++) {
    uint16_t px = fb[i];
    uint8_t r = (uint8_t)(((px >> 11) & 0x1Fu) << 3);
    uint8_t g = (uint8_t)(((px >> 5) & 0x3Fu) << 2);
    uint8_t b = (uint8_t)((px & 0x1Fu) << 3);

    led_bytes[3 * i + 0] = g;
    led_bytes[3 * i + 1] = r;
    led_bytes[3 * i + 2] = b;
  }
  neopixel_show_P1_4(led_bytes, n * 3u);
}
