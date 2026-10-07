#ifndef HUB75_BROWSER_H
#define HUB75_BROWSER_H

/* The sketch browser is a simulator feature, not a sketch. The build
   links every file in sketches/ and generates a table of these
   entries. The browser picks one at a time and the simulator's
   setup()/draw() dispatch to it. */

typedef struct Hub75Sketch {
  const char *name;
  void (*setup)(void);
  void (*draw)(void);
} Hub75Sketch;

extern const Hub75Sketch hub75_sketches[];
extern const int hub75_sketch_count;

/* Choose the starting sketch by name. Falls back to the first entry. */
void browser_init(const char *default_name);

/* The sketch the simulator is running right now. */
const Hub75Sketch *browser_current(void);

int browser_is_open(void);
void browser_toggle(void);

/* Move the highlight without loading anything. */
void browser_move(int delta);

/* Load the highlighted sketch. */
void browser_commit(void);

/* Highlight and load the previous (-1) or next (+1) sketch. */
void browser_step(int delta);

int browser_index(void);
int browser_selection(void);
void browser_set_selection(int index);

/* Overlay geometry, in the renderer's logical pixels. */
typedef struct BrowserLayout {
  int visible;
  int x;
  int y;
  int w;
  int h;
  int title_h;
  int row_h;
  int scale;
  int pad;
  int first_row;
  int rows;
  int count;
} BrowserLayout;

/* Fill `out` for a view of view_w by view_h. Keeps the highlighted
   row on screen and clamps the scroll offset. */
void browser_layout(int view_w, int view_h, BrowserLayout *out);

/* Row index under a point, or -1 when the point misses a row. */
int browser_row_at(const BrowserLayout *layout, int px, int py);

#endif
