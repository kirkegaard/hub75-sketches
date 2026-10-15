#include "hub75.h"
#include "net.h"
#include "util/base64.h"
#include "ws.h"

#include <string.h>

/* The go-grid board (grid.lowpoly.dk): a WebSocket feed of a flat array
   of checkboxes. The server sends base64 of the bit array on connect,
   then "s:<cell>:<0|1>" on every toggle; "c:"/"r:"/"d:"/"p:" are
   presence noise. We show the first width*height cells, row-major, as
   lit pixels.

   The WebSocket itself is the general client in ws.h; this sketch only
   knows the grid protocol. The board talks wss:// to the live server;
   the simulator talks plain ws:// to a local go-grid. For a status
   readout on the panel, build with NETDEBUG=1. */

/* Override in secrets.h to point elsewhere, e.g. a local go-grid at
   "ws://localhost:6060/api/ws". */
#ifndef HUB75_GRID_WS
#define HUB75_GRID_WS "wss://grid.lowpoly.dk/ws"
#endif

enum { MAX_GRID_BYTES = 16384, RX_CAP = 24576 };
enum {
  BOOT_FRAMES = 75
}; /* ~2.5s before the net starts, so the debug
      overlay can show the reset reason first */

static uint16_t bg, fg;
static uint8_t bits[MAX_GRID_BYTES];
static char rx[RX_CAP];
static int cells = 0;
static int started = 0;

static int bit_at(int cell) {
  if (cell < 0 || cell >= cells) {
    return 0;
  }
  return (bits[cell >> 3] >> (7 - (cell & 7))) & 1;
}

static void set_bit(int cell, int on) {
  if (cell < 0 || cell >= cells) {
    return;
  }
  if (on) {
    bits[cell >> 3] |= (uint8_t)(1u << (7 - (cell & 7)));
  } else {
    bits[cell >> 3] &= (uint8_t)~(1u << (7 - (cell & 7)));
  }
}

/* The first message is base64 of the whole bit array; the rest are
   "s:<cell>:<0|1>". Anything with a known prefix is ignored. */
static void apply(const char *msg, int len) {
  int i;
  int cell;
  int bit;

  if (len >= 2 && msg[1] == ':') {
    char t = msg[0];

    if (t == 's') {
      cell = 0;
      bit = 0;
      i = 2;
      while (i < len && msg[i] >= '0' && msg[i] <= '9') {
        cell = cell * 10 + (msg[i] - '0');
        i++;
      }
      if (i < len && msg[i] == ':') {
        i++;
        if (i < len) {
          bit = (msg[i] == '1');
        }
      }
      set_bit(cell, bit);
      return;
    }

    if (t == 'c' || t == 'r' || t == 'd' || t == 'p') {
      return;
    }
  }

  {
    int n = base64_decode(msg, (size_t)len, bits, sizeof bits);
    if (n > 0) {
      cells = n * 8;
    }
  }
}

void setup(void) {
  bg = color(0, 0, 0);
  fg = color(255, 255, 255);
}

void draw(void) {
  int n;
  int i;
  int budget;

  if (!started) {
#ifdef HUB75_NET_DEBUG
    if (frameCount <= BOOT_FRAMES) {
      background(bg);
      return;
    }
#endif
    started = 1;
    net_begin();
    ws_open(HUB75_GRID_WS);
  }

  /* The live board floods presence/cursor messages. Drain a bounded
     batch per frame so a burst cannot spin draw() forever, and so the
     reader task keeps flowing (a stalled reader makes the server drop
     us). */
  for (budget = 0; budget < 256 && (n = ws_poll(rx, sizeof rx)) > 0; budget++) {
    apply(rx, n);
  }

  background(bg);

  if (cells <= 0) {
    return;
  }

  n = width * height;
  if (cells < n) {
    n = cells;
  }

  stroke(fg);
  for (i = 0; i < n; i++) {
    if (bit_at(i)) {
      point(i % width, i / width);
    }
  }
}
