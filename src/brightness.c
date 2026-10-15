#include "brightness.h"

static int g_brightness = HUB75_BRIGHTNESS_MAX;

int hub75_brightness(void) { return g_brightness; }

void hub75_brightness_set(int value) {
  if (value < HUB75_BRIGHTNESS_MIN) {
    value = HUB75_BRIGHTNESS_MIN;
  } else if (value > HUB75_BRIGHTNESS_MAX) {
    value = HUB75_BRIGHTNESS_MAX;
  }

  g_brightness = value;
}

void hub75_brightness_up(void) {
  hub75_brightness_set(g_brightness + HUB75_BRIGHTNESS_STEP);
}

void hub75_brightness_down(void) {
  hub75_brightness_set(g_brightness - HUB75_BRIGHTNESS_STEP);
}

uint8_t hub75_dim8(uint8_t channel, int brightness) {
  if (brightness >= HUB75_BRIGHTNESS_MAX) {
    return channel;
  }

  if (brightness <= 0) {
    return 0;
  }

  return (uint8_t)(((int)channel * brightness + 127) / 255);
}

uint16_t hub75_dim565(uint16_t color, int brightness) {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  if (brightness >= HUB75_BRIGHTNESS_MAX) {
    return color;
  }

  r = (uint8_t)((color >> 11) & 0x1Fu);
  g = (uint8_t)((color >> 5) & 0x3Fu);
  b = (uint8_t)(color & 0x1Fu);

  r = (uint8_t)(((int)r * brightness + 127) / 255);
  g = (uint8_t)(((int)g * brightness + 127) / 255);
  b = (uint8_t)(((int)b * brightness + 127) / 255);

  return (uint16_t)((r << 11) | (g << 5) | b);
}
