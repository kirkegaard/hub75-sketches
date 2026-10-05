#ifndef HUB75_PANEL_H
#define HUB75_PANEL_H

#include <stdint.h>

/* Screen scale is 8 pixels per millimeter, so a P4 panel is four times
   a P1 panel of the same LED count. The LED disk is about 70% of the
   pitch; the rest of the cell is the gap. */
#define HUB75_PX_PER_MM 8

typedef struct Hub75Panel {
  int matrix_w;
  int matrix_h;
  int pitch_px;
  int led_px;
  int img_w;
  int img_h;
  char pitch_name[8];
  uint8_t *rgb;
} Hub75Panel;

/* pitch is "P1", "P2.5", or "P4" (a bare 1, 2.5, or 4 is accepted).
   Returns 0 on success. */
int hub75_panel_init(Hub75Panel *panel, int matrix_w, int matrix_h,
                     const char *pitch);

/* Odd-ish disk diameter for a cell of `cell` pixels. About 70% of the
   cell. An 8px cell uses 7, because 5 rasterizes as a square. */
int hub75_led_diameter(int cell);

/* Raster round LEDs into rgb. rgb is matrix_w * cell by matrix_h * cell.
   Off LEDs stay visible as dark disks. */
void hub75_leds_render(uint8_t *rgb, int cell, int diameter, const uint16_t *fb,
                       int matrix_w, int matrix_h);

/* Raster round LEDs into panel->rgb at the panel pitch. */
void hub75_panel_render(Hub75Panel *panel, const uint16_t *fb);

void hub75_panel_free(Hub75Panel *panel);

#endif
