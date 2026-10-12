/* graphics.c allocates the framebuffer once with calloc(n, sizeof(uint16_t))
   and frees it if a sketch changes the panel size. SDCC's malloc heap costs
   1 KB of external RAM, which is more than the CH552 has room for next to
   the WS2812 buffer, so back that single allocation with a static buffer.

   Only one framebuffer is ever live, so calloc always hands back the same
   block and free is a no-op. */

#include <stddef.h>
#include <stdint.h>

#include "config.h"

static __xdata uint16_t fb_storage[HUB75_PANEL_WIDTH * HUB75_PANEL_HEIGHT];

void *calloc(size_t n, size_t size) {
  if (n * size > sizeof(fb_storage)) {
    return 0;
  }
  return fb_storage;
}

void free(void *p) { (void)p; }
