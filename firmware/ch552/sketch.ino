#include <Arduino.h>

#include "config.h"
#include "matrix.h"

extern unsigned long frameCount;
void hub75_setup(void);
void hub75_draw(void);

/* Same clock as the simulator window and the ESP32 firmware: one sketch
   frame every 33 ms. */
static const uint32_t FRAME_MS = 33;

void setup() {
  if (!matrix_begin(HUB75_PANEL_WIDTH, HUB75_PANEL_HEIGHT)) {
    for (;;) {
      delay(1000);
    }
  }
  hub75_setup();
}

void loop() {
  static uint32_t next_frame = 0;
  uint32_t now = millis();

  if (next_frame != 0 && (int32_t)(now - next_frame) < 0) {
    return;
  }
  next_frame = now + FRAME_MS;
  frameCount++;
  hub75_draw();
  matrix_present();
}
