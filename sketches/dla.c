#include "graphics.h"
#include "hub75.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>

/* One rule: a slow crystal. A seed sits at the center, and walkers are
   released from a ring around it. A walker wanders at random until it
   touches the crystal, then it sticks and becomes part of it. Newly
   stuck cells glow and settle to white, so each arm is seen as it
   lands. When the crystal fills enough of the panel it restarts from a
   fresh seed. */

#define WALKERS 10
#define MAX_STEPS 300
#define TAU 6.2831853f

static uint8_t *occ = NULL;
static uint16_t *birth = NULL;
static int gw;
static int gh;
static int gn;
static int occ_count;

static uint32_t seed = 1;

static int occupied(int x, int y) {
  if ((unsigned)x >= (unsigned)gw || (unsigned)y >= (unsigned)gh) {
    return 0;
  }
  return occ[y * gw + x];
}

static int touching(int x, int y) {
  return occupied(x - 1, y) || occupied(x + 1, y) || occupied(x, y - 1) ||
         occupied(x, y + 1);
}

static void reset_cluster(void) {
  int i;

  for (i = 0; i < gn; i++) {
    occ[i] = 0;
    birth[i] = 0;
  }
  occ[(gh / 2) * gw + width / 2] = 1;
  birth[(gh / 2) * gw + width / 2] = (uint16_t)frameCount;
  occ_count = 1;
}

void setup(void) {
  gw = width;
  gh = height;
  gn = gw * gh;
  free(occ);
  free(birth);
  occ = (uint8_t *)calloc((size_t)gn, sizeof(uint8_t));
  birth = (uint16_t *)calloc((size_t)gn, sizeof(uint16_t));
  if (!occ || !birth) {
    free(occ);
    free(birth);
    occ = NULL;
    birth = NULL;
    return;
  }
  reset_cluster();
}

void draw(void) {
  uint16_t *fb;
  int w;
  int i;
  int x;
  int y;

  if (!occ || !birth) {
    return;
  }

  for (w = 0; w < WALKERS; w++) {
    float a = rng_float(&seed, 0.0f, TAU);
    float rad =
        3.0f + 0.8f * sqrtf((float)occ_count) + rng_float(&seed, 2.0f, 6.0f);
    int step;

    x = (int)lroundf((float)gw * 0.5f + cosf(a) * rad);
    y = (int)lroundf((float)gh * 0.5f + sinf(a) * rad);
    if (x < 0) {
      x = 0;
    }
    if (x >= gw) {
      x = gw - 1;
    }
    if (y < 0) {
      y = 0;
    }
    if (y >= gh) {
      y = gh - 1;
    }
    if (occupied(x, y)) {
      continue;
    }

    for (step = 0; step < MAX_STEPS; step++) {
      if (touching(x, y)) {
        if ((unsigned)x < (unsigned)gw && (unsigned)y < (unsigned)gh &&
            !occ[y * gw + x]) {
          occ[y * gw + x] = 1;
          birth[y * gw + x] = (uint16_t)frameCount;
          occ_count++;
        }
        break;
      }
      switch (rng_next(&seed) & 3u) {
      case 0:
        x++;
        break;
      case 1:
        x--;
        break;
      case 2:
        y++;
        break;
      default:
        y--;
        break;
      }
      if (x < 0) {
        x = 0;
      }
      if (x >= gw) {
        x = gw - 1;
      }
      if (y < 0) {
        y = 0;
      }
      if (y >= gh) {
        y = gh - 1;
      }
    }
  }

  if (occ_count > gn * 35 / 100) {
    reset_cluster();
  }

  fb = hub75_pixels();
  if (!fb) {
    return;
  }
  for (i = 0; i < gn; i++) {
    if (occ[i]) {
      unsigned age = (unsigned)frameCount - (unsigned)birth[i];
      uint8_t g;

      if (age < 90u) {
        g = (uint8_t)(60u + 195u * age / 90u);
      } else {
        g = 255;
      }
      fb[i] = color(g, g, g);
    } else {
      fb[i] = color(0, 0, 0);
    }
  }
}
