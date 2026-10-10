#include "hub75.h"

#define BAR_WIDTH 10
#define SCAN_SPEED 2
#define FADE_AMOUNT 16
#define MAX_PANEL_WIDTH 512

static uint8_t brightness[MAX_PANEL_WIDTH];

static int scanner_x(void) {
  int travel = width - BAR_WIDTH;
  unsigned long period;
  unsigned long phase;

  if (travel <= 0) {
    return 0;
  }

  period = (unsigned long)travel * 2;
  phase = (frameCount * SCAN_SPEED) % period;
  if (phase > (unsigned long)travel) {
    phase = period - phase;
  }
  return (int)phase;
}

void setup(void) {
  int x;

  for (x = 0; x < MAX_PANEL_WIDTH; x++) {
    brightness[x] = 0;
  }
}

void draw(void) {
  int panel_width = width < MAX_PANEL_WIDTH ? width : MAX_PANEL_WIDTH;
  int eye = scanner_x();
  int y = height / 2;
  int x;

  background(color(0, 0, 0));

  for (x = 0; x < panel_width; x++) {
    if (brightness[x] > FADE_AMOUNT) {
      brightness[x] -= FADE_AMOUNT;
    } else {
      brightness[x] = 0;
    }
  }

  for (x = eye; x < eye + BAR_WIDTH && x < panel_width; x++) {
    brightness[x] = 255;
  }

  strokeWeight(1);
  for (x = 0; x < panel_width; x++) {
    if (brightness[x] > 0) {
      stroke(color(brightness[x], 0, 0));
      point(x, y);
    }
  }
}
