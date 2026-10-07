#include "browser.h"

#include "font5x7.h"
#include "graphics.h"
#include "hub75.h"

#include <stddef.h>
#include <string.h>

/* The build generates hub75_sketches[] and hub75_sketch_count from the
   files in sketches/. These hold the running and highlighted entries. */
static int g_current = 0;
static int g_selection = 0;
static int g_open = 0;
static int g_scroll = 0;

void browser_init(const char *default_name) {
  int i;

  g_current = 0;
  g_selection = 0;
  g_open = 0;
  g_scroll = 0;

  if (default_name) {
    for (i = 0; i < hub75_sketch_count; i++) {
      if (strcmp(hub75_sketches[i].name, default_name) == 0) {
        g_current = i;
        g_selection = i;
        break;
      }
    }
  }
}

const Hub75Sketch *browser_current(void) {
  if (hub75_sketch_count < 1) {
    return NULL;
  }
  return &hub75_sketches[g_current];
}

int browser_is_open(void) { return g_open; }

void browser_toggle(void) { g_open = !g_open; }

int browser_index(void) { return g_current; }

int browser_selection(void) { return g_selection; }

static int wrap(int index) {
  int n = hub75_sketch_count;

  if (n < 1) {
    return 0;
  }
  index %= n;
  if (index < 0) {
    index += n;
  }
  return index;
}

void browser_set_selection(int index) {
  if (hub75_sketch_count < 1) {
    return;
  }
  g_selection = wrap(index);
}

/* Start the highlighted sketch. frameCount drops back to zero so the
   next draw() is frame 1, and the drawing state resets so a sketch
   that turned fill off does not leak into the next one. */
static void apply(void) {
  size_t count;

  if (hub75_sketch_count < 1) {
    return;
  }

  g_current = g_selection;
  frameCount = 0;
  hub75_gfx_reset();

  count = (size_t)width * (size_t)height;
  if (hub75_pixels() && count > 0) {
    memset(hub75_pixels(), 0, count * sizeof(uint16_t));
  }

  hub75_sketches[g_current].setup();
}

void browser_move(int delta) {
  browser_set_selection(g_selection + delta);
}

void browser_commit(void) { apply(); }

void browser_step(int delta) {
  browser_set_selection(g_selection + delta);
  apply();
}

void browser_layout(int view_w, int view_h, BrowserLayout *out) {
  const int scale = 2;
  const int pad = 8;
  const int margin = 12;
  int longest = 0;
  int text_w;
  int title_h = HUB75_FONT_H * scale + pad;
  int row_h = HUB75_FONT_H * scale + 6;
  int width;
  int rows;
  int available;
  int i;

  memset(out, 0, sizeof *out);
  out->count = hub75_sketch_count;

  if (!g_open || hub75_sketch_count < 1) {
    return;
  }

  for (i = 0; i < hub75_sketch_count; i++) {
    int len = (int)strlen(hub75_sketches[i].name);
    if (len > longest) {
      longest = len;
    }
  }

  text_w = longest * (HUB75_FONT_W + 1) * scale;
  width = text_w + pad * 2 + 10;
  if (width > view_w - margin * 2) {
    width = view_w - margin * 2;
  }
  if (width < 60) {
    return;
  }

  out->scale = scale;
  out->pad = pad;
  out->title_h = title_h;
  out->row_h = row_h;
  out->w = width;
  out->x = view_w - width - margin;
  out->y = margin;

  available = view_h - out->y - margin;
  rows = (available - title_h) / row_h;
  if (rows < 1) {
    rows = 1;
  }
  if (rows > hub75_sketch_count) {
    rows = hub75_sketch_count;
  }
  out->rows = rows;

  if (g_scroll > hub75_sketch_count - rows) {
    g_scroll = hub75_sketch_count - rows;
  }
  if (g_scroll < 0) {
    g_scroll = 0;
  }
  if (g_selection < g_scroll) {
    g_scroll = g_selection;
  }
  if (g_selection >= g_scroll + rows) {
    g_scroll = g_selection - rows + 1;
  }

  out->first_row = g_scroll;
  out->h = title_h + rows * row_h;
  out->visible = 1;
}

int browser_row_at(const BrowserLayout *layout, int px, int py) {
  int body_y;
  int row;

  if (!layout->visible) {
    return -1;
  }
  if (px < layout->x || px >= layout->x + layout->w) {
    return -1;
  }
  body_y = layout->y + layout->title_h;
  if (py < body_y || py >= layout->y + layout->h) {
    return -1;
  }

  row = layout->first_row + (py - body_y) / layout->row_h;
  if (row < 0 || row >= layout->count) {
    return -1;
  }
  return row;
}

/* The simulator's setup()/draw() land here. Sketches are compiled with
   renamed symbols, so this is the only definition in the link. */
void setup(void) {
  const Hub75Sketch *sketch = browser_current();

  if (sketch) {
    sketch->setup();
  }
}

void draw(void) {
  const Hub75Sketch *sketch = browser_current();

  if (sketch) {
    sketch->draw();
  }
}
