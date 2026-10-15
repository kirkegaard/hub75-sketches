#include "netdebug.h"

#include "hub75.h"
#include "net.h"
#include "util/font3x5.h"
#include "ws.h"

#include <stdio.h>

#ifdef HUB75_FIRMWARE
#include "esp_system.h"
#endif

/* The overlay is the same on both platforms; only the reset reason and
   heap come from the board. Rows are 3x5 glyphs, one per 6px line. */

static void text(int row, const char *s, uint16_t c) {
  int x = 1;
  int gy = 1 + row * 6;
  int i;
  int r;
  int col;

  stroke(c);
  for (i = 0; s[i] && x + 3 <= width; i++) {
    unsigned char ch = (unsigned char)s[i];
    uint16_t glyph;

    if (ch >= 128) {
      ch = '?';
    }
    glyph = font3x5[ch];
    for (r = 0; r < 5; r++) {
      for (col = 0; col < 3; col++) {
        if (glyph & (1u << (14 - (r * 3 + col)))) {
          point(x + col, gy + r);
        }
      }
    }
    x += 4;
  }
}

void net_debug_draw(void) {
  char line[24];
  uint16_t c = color(255, 255, 255);
  int reason = 0;
  unsigned long heap = 0;

#ifdef HUB75_FIRMWARE
  reason = (int)esp_reset_reason();
  heap = (unsigned long)esp_get_free_heap_size();
#endif

  snprintf(line, sizeof line, "RST %d", reason);
  text(0, line, c);
  snprintf(line, sizeof line, "HEAP %lu", heap);
  text(1, line, c);
  text(2, net_status_text(), c);
  snprintf(line, sizeof line, "WS %s", ws_status_text());
  text(3, line, c);
  snprintf(line, sizeof line, "MSG %lu", ws_messages());
  text(4, line, c);
  snprintf(line, sizeof line, "CONN %lu", ws_connects());
  text(5, line, c);
}
