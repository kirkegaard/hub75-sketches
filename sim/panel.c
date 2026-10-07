#include "panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same_word(const char *a, const char *b) {
  if (!a || !b) {
    return 0;
  }
  while (*a != '\0' && *b != '\0') {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (char)(ca - 'A' + 'a');
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (char)(cb - 'A' + 'a');
    }
    if (ca != cb) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

typedef struct Pitch {
  const char *name;
  const char *alias;
  int pitch_px;
} Pitch;

/* pitch_px = millimeters * HUB75_PX_PER_MM. The disk diameter comes
   from hub75_led_diameter so the same cell always makes the same circle. */
static const Pitch kPitches[] = {
    {"P1", "1", 8},
    {"P2.5", "2.5", 20},
    {"P4", "4", 32},
};

static const Pitch *find_pitch(const char *text) {
  size_t i;

  if (!text) {
    return NULL;
  }
  for (i = 0; i < sizeof kPitches / sizeof kPitches[0]; i++) {
    if (same_word(text, kPitches[i].name) ||
        same_word(text, kPitches[i].alias)) {
      return &kPitches[i];
    }
  }
  return NULL;
}

int hub75_panel_init(Hub75Panel *panel, int matrix_w, int matrix_h,
                     const char *pitch) {
  const Pitch *found;
  size_t pixels;
  uint8_t *rgb;

  if (!panel || matrix_w <= 0 || matrix_h <= 0) {
    return -1;
  }
  found = find_pitch(pitch);
  if (!found) {
    return -1;
  }
  pixels = (size_t)matrix_w * (size_t)found->pitch_px;
  pixels *= (size_t)matrix_h * (size_t)found->pitch_px;
  if (matrix_h != 0 && found->pitch_px != 0 &&
      pixels / ((size_t)matrix_h * (size_t)found->pitch_px) !=
          (size_t)matrix_w * (size_t)found->pitch_px) {
    return -1;
  }
  rgb = (uint8_t *)malloc(pixels * 3u);
  if (!rgb) {
    return -1;
  }
  free(panel->rgb);
  memset(panel, 0, sizeof *panel);
  panel->matrix_w = matrix_w;
  panel->matrix_h = matrix_h;
  panel->pitch_px = found->pitch_px;
  panel->led_px = hub75_led_diameter(found->pitch_px);
  panel->shape = HUB75_LED_CIRCLE;
  panel->img_w = matrix_w * found->pitch_px;
  panel->img_h = matrix_h * found->pitch_px;
  snprintf(panel->pitch_name, sizeof panel->pitch_name, "%s", found->name);
  panel->rgb = rgb;
  return 0;
}

static void expand565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
  uint8_t r5 = (uint8_t)((c >> 11) & 0x1Fu);
  uint8_t g6 = (uint8_t)((c >> 5) & 0x3Fu);
  uint8_t b5 = (uint8_t)(c & 0x1Fu);
  *r = (uint8_t)((r5 << 3) | (r5 >> 2));
  *g = (uint8_t)((g6 << 2) | (g6 >> 4));
  *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

int hub75_led_diameter(int cell) {
  int d;

  if (cell < 2) {
    return 1;
  }

  d = (cell * 7) / 10;

  return d;
}

static const char *kShapeNames[] = {"circle", "square"};

int hub75_led_shape_count(void) {
  return (int)(sizeof kShapeNames / sizeof kShapeNames[0]);
}

const char *hub75_led_shape_name(int shape) {
  if (shape < 0 || shape >= hub75_led_shape_count()) {
    shape = HUB75_LED_CIRCLE;
  }
  return kShapeNames[shape];
}

int hub75_led_shape_next(int shape) {
  return (shape + 1) % hub75_led_shape_count();
}

/* Is the point at (dx, dy) inside the lit shape of the given radius? */
static int led_lit(int shape, float dx, float dy, float radius, float r2) {
  if (shape == HUB75_LED_SQUARE) {
    return dx <= radius && dx >= -radius && dy <= radius && dy >= -radius;
  }
  return dx * dx + dy * dy <= r2;
}

void hub75_leds_render(uint8_t *rgb, int cell, int diameter, int shape,
                       const uint16_t *fb, int matrix_w, int matrix_h) {
  int img_w;
  int img_h;
  int y;
  float radius;
  float r2;
  float center;

  const uint8_t gap_r = 6;
  const uint8_t gap_g = 6;
  const uint8_t gap_b = 8;
  const uint8_t off_r = HUB75_LED_OFF_R;
  const uint8_t off_g = HUB75_LED_OFF_G;
  const uint8_t off_b = HUB75_LED_OFF_B;

  if (!rgb || !fb || cell < 1 || matrix_w < 1 || matrix_h < 1) {
    return;
  }

  if (diameter < 1) {
    diameter = 1;
  }

  img_w = matrix_w * cell;
  img_h = matrix_h * cell;
  radius = (float)diameter * 0.5f;
  r2 = radius * radius;
  center = (float)(cell - 1) * 0.5f;

  for (y = 0; y < img_h; y++) {
    int ly = y % cell;
    int led_y = y / cell;
    float dy = (float)ly - center;
    int x;
    uint8_t *row = rgb + (size_t)y * (size_t)img_w * 3u;

    for (x = 0; x < img_w; x++) {
      int lx = x % cell;
      int led_x = x / cell;
      float dx = (float)lx - center;
      uint8_t *px = row + (size_t)x * 3u;

      if (led_lit(shape, dx, dy, radius, r2)) {
        uint16_t sample = fb[(size_t)led_y * (size_t)matrix_w + (size_t)led_x];
        uint8_t r, g, b;

        expand565(sample, &r, &g, &b);

        if (r == 0 && g == 0 && b == 0) {
          px[0] = off_r;
          px[1] = off_g;
          px[2] = off_b;
        } else {
          px[0] = r;
          px[1] = g;
          px[2] = b;
        }
      } else {
        px[0] = gap_r;
        px[1] = gap_g;
        px[2] = gap_b;
      }
    }
  }
}

void hub75_panel_render(Hub75Panel *panel, const uint16_t *fb) {
  if (!panel || !panel->rgb) {
    return;
  }
  hub75_leds_render(panel->rgb, panel->pitch_px, panel->led_px, panel->shape, fb,
                    panel->matrix_w, panel->matrix_h);
}

void hub75_panel_free(Hub75Panel *panel) {
  if (!panel) {
    return;
  }
  free(panel->rgb);
  panel->rgb = NULL;
}
