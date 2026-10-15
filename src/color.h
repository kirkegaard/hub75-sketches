#ifndef HUB75_COLOR_H
#define HUB75_COLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panel color correction. The framebuffer is display-referred (what an
   sRGB monitor shows), but a HUB75 panel drives linear-PWM LEDs with
   narrow-band emitters, so the same RGB565 sample reads as brighter and
   more saturated on the board. These two corrections narrow that gap;
   the simulator stays the reference, so it does not apply them.

   saturation_pct: 100 leaves colors alone, 0 is grayscale, up to 200
   boosts. gamma: 1.0 is off; 2.2 linearizes the sRGB-ish input for the
   LEDs. Both defaults leave samples untouched. */

void hub75_color_init(int saturation_pct, float gamma);

/* Apply saturation and gamma to one RGB565 sample. */
uint16_t hub75_correct565(uint16_t c);

#ifdef __cplusplus
}
#endif

#endif
