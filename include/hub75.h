#ifndef HUB75_H
#define HUB75_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LED panel size. Valid from setup() onward. */
extern int width;
extern int height;

/* 1 on the first draw(), then 2, 3, ... The host steps this once per frame. */
extern unsigned long frameCount;

/* Milliseconds since the panel runtime started. */
unsigned long millis(void);

/* Matrix Portal S3 user buttons: 1 pressed, 0 released.
   The simulator maps the up and down arrow keys. A headless dump reads 0. */
int buttonUp(void);
int buttonDown(void);

/* Pack 8-bit channels into RGB565 (5/6/5). */
uint16_t color(uint8_t r, uint8_t g, uint8_t b);

/* Clear the framebuffer. Does not change fill or stroke. */
void background(uint16_t c);

/* Fill and stroke start enabled and white. */
void fill(uint16_t c);
void noFill(void);
void stroke(uint16_t c);
void noStroke(void);
void strokeWeight(int weight);

/* rect(x, y, w, h) is the top-left corner and the size.
   circle(x, y, d) is the center and the diameter.
   Stroke on rect and circle is drawn inside that footprint.
   point and line use stroke.

   triangle is three corners, in fractional pixels, so a slow turn
   does not snap. Its outline visits every pixel an edge crosses,
   so a diagonal does not leave a gap. polygon is a regular
   polygon: center, radius out to a vertex, side count, and
   rotation in radians. Rotation 0 puts a vertex on the right.
   Its stroke lies on the edges. */
void point(int x, int y);
void line(int x0, int y0, int x1, int y1);
void rect(int x, int y, int w, int h);
void circle(int cx, int cy, int d);
void triangle(float x0, float y0, float x1, float y1, float x2, float y2);
void polygon(int cx, int cy, float radius, int sides, float rotation);

void setup(void);
void draw(void);

#ifdef __cplusplus
}
#endif

#endif
