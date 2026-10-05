#include <Arduino.h>

#include "config.h"
#include "portal.h"

extern "C" {
extern unsigned long frameCount;
void hub75_setup(void);
void hub75_draw(void);
}

/* Same clock as the simulator window: one sketch frame every 33 ms. */
static const uint32_t FRAME_MS = 33;

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!portal_begin(HUB75_PANEL_WIDTH, HUB75_PANEL_HEIGHT)) {
    Serial.println("HUB75 start failed");
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
  portal_present();
}
