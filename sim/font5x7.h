#ifndef HUB75_FONT5X7_H
#define HUB75_FONT5X7_H

#define HUB75_FONT_W 5
#define HUB75_FONT_H 7

/* One glyph as seven row bytes. In each byte bit 4 is the leftmost
   column and bit 0 the rightmost. Uppercase maps to lowercase. Always
   returns a valid glyph (space for anything unknown). */
const unsigned char *hub75_font_glyph(char c);

#endif
