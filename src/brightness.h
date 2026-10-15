#ifndef HUB75_BRIGHTNESS_H
#define HUB75_BRIGHTNESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panel brightness, 0 (off) to 255 (full). The two board buttons step
   it; sketches cannot read those buttons. The simulator scales the lit
   LEDs and the firmware scales the RGB565 samples it hands to
   Protomatter, so both dim the same way. */
#define HUB75_BRIGHTNESS_MIN 0
#define HUB75_BRIGHTNESS_MAX 255
#define HUB75_BRIGHTNESS_STEP 32

int hub75_brightness(void);
void hub75_brightness_set(int value);
void hub75_brightness_up(void);
void hub75_brightness_down(void);

/* Scale one 8-bit channel, and one packed RGB565 sample, by brightness. */
uint8_t hub75_dim8(uint8_t channel, int brightness);
uint16_t hub75_dim565(uint16_t color, int brightness);

#ifdef __cplusplus
}
#endif

#endif
