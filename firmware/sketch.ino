#include <Arduino.h>

#include "config.h"
#include "portal.h"

#include "brightness.h"
#include "color.h"

extern "C" {
extern unsigned long frameCount;
void hub75_setup(void);
void hub75_draw(void);
}

/* Same clock as the simulator window: one sketch frame every 33 ms. */
static const uint32_t FRAME_MS = 33;

/* The user buttons dim and brighten the panel. Polled every loop
   iteration, so a press lands even while the sketch is between frames. */
static void poll_brightness(void) {
  static int prev_up = 0;
  static int prev_down = 0;
  int up = portal_button_up();
  int down = portal_button_down();

  if (up && !prev_up) {
    portal_brightness_up();
  }
  if (down && !prev_down) {
    portal_brightness_down();
  }
  prev_up = up;
  prev_down = down;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!portal_begin(HUB75_PANEL_WIDTH, HUB75_PANEL_HEIGHT, HUB75_ROTATE)) {
    Serial.println("HUB75 start failed");
    for (;;) {
      delay(1000);
    }
  }
  hub75_brightness_set(HUB75_BRIGHTNESS);
  hub75_color_init(HUB75_SATURATION, HUB75_GAMMA);
  hub75_setup();
}

void loop() {
  static uint32_t next_frame = 0;
  uint32_t now = millis();

  poll_brightness();

  if (next_frame != 0 && (int32_t)(now - next_frame) < 0) {
    return;
  }
  next_frame = now + FRAME_MS;
  frameCount++;
  hub75_draw();
  portal_present();
}
