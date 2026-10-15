#include "hub75.h"

#include <math.h>

/* A wireframe tunnel that snakes and twists. Rings march out of the
   vanishing point toward the camera and neighboring rings are wired
   together by rails, so the tube reads as a glowing mesh instead of a
   solid wall. The depth divide is the one from tunnel-1k: a ring's
   radius is R / z, and the ladder of depths is geometric, so the rings
   recycle seamlessly and appear evenly spaced on screen.

   Two extra waves ride on top. A bend slides the whole ring sideways by
   an amount that follows the tube's length, so the tunnel curves away
   and the vanishing point wanders; a roll spins each ring's cross
   section by depth, so the rails spiral. Both are keyed to the same
   log-depth axis as the rings, so the shape stays put in space while the
   camera flies through it. A five-lobed wobble flutes the radius, and
   the polar tunnel's drifting vanishing point adds a gentle sway.

   The mesh is grayscale. Near rings are bright white and the fog fades
   each ring toward black with distance; GAMMA bends that curve so the
   far end drops away fast and the tube reads deep. */

#define SIDES 20
#define RINGS 16
#define GROW 1.19f
#define TAU 6.2831853f

/* Wobble (and how fast it travels down the tube), the twist the rails
   spiral through per unit depth, and how far the fog dims. GAMMA bends
   the fog curve so the distance falls off faster. */
#define WOBBLE 0.035f
#define ZWOB 0.5f
#define TWIST 0.03f
#define FARFOG 0.88f
#define GAMMA 1.20f

/* How far the tube bends, as a fraction of the panel width, and the
   spatial and time frequencies of the bend. */
#define BEND 0.125f
#define BKF 0.32f
#define BW 0.64f

/* How far the cross section rolls, and the frequencies of the roll. */
#define ROLL 0.5f
#define RKF 0.30f
#define RW 0.60f

static uint16_t bg;

static uint8_t to8(float v) {
  if (v <= 0.0f) {
    return 0;
  }
  if (v >= 1.0f) {
    return 255;
  }
  return (uint8_t)(v * 255.0f + 0.5f);
}

/* A level (0..1) as a gray, packed to RGB565. */
static uint16_t shade(float level) {
  uint8_t g = to8(level);

  return color(g, g, g);
}

/* The wall point at angle a on the ring at depth z. Larger z is farther
   away, so the radius shrinks toward the center. The bend offsets the
   whole ring, the roll turns it, and the wobble flutes the radius. */
static void project(float a, float z, float t, float cx, float cy, float *ox,
                    float *oy) {
  float base = 0.60f * (float)width;
  float bend = BEND * (float)width;
  float u = logf(z) / logf(GROW);
  float offx = bend * sinf(u * BKF + t * BW);
  float offy = bend * sinf(u * BKF * 0.8f + t * BW * 0.7f + 1.7f);
  float roll = ROLL * sinf(u * RKF + t * RW);
  float wob = 1.0f + WOBBLE * sinf(a * 5.0f + z * ZWOB + t * 2.2f);
  float ang = a + t * 0.35f + z * TWIST + roll;
  float r = base * wob / z;

  *ox = cx + offx + cosf(ang) * r;
  *oy = cy + offy + sinf(ang) * r;
}

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  float t = (float)frameCount / 30.0f;
  /* The tube sways: the vanishing point drifts on slow sine paths. */
  float cx = (float)width * 0.5f + sinf(t * 0.4f) * (float)width * 0.05f;
  float cy = (float)height * 0.5f + cosf(t * 0.53f) * (float)height * 0.08f;
  /* Rings move in by one slot every turn of the phase; the wrap is
     seamless because the ladder of depths is geometric. */
  float phase = t * 2.0f;
  float lead = powf(GROW, -(phase - floorf(phase)));
  float fg[RINGS];
  float px[RINGS][SIDES];
  float py[RINGS][SIDES];
  int k;
  int i;

  background(bg);
  strokeWeight(1);

  for (k = 0; k < RINGS; k++) {
    float z = lead * powf(GROW, (float)k);

    fg[k] = powf(1.0f - FARFOG * (float)k / (float)(RINGS - 1), GAMMA);
    for (i = 0; i < SIDES; i++) {
      float a = TAU * (float)i / (float)SIDES;

      project(a, z, t, cx, cy, &px[k][i], &py[k][i]);
    }
  }

  /* Far to near, so the brighter near lines overdraw the dim ones. */
  for (k = RINGS - 1; k >= 0; k--) {
    for (i = 0; i < SIDES; i++) {
      int j = (i + 1) % SIDES;

      stroke(shade(fg[k]));
      line((int)px[k][i], (int)py[k][i], (int)px[k][j], (int)py[k][j]);
    }
    if (k > 0) {
      float rail = 0.5f * (fg[k] + fg[k - 1]);

      for (i = 0; i < SIDES; i++) {
        stroke(shade(rail));
        line((int)px[k][i], (int)py[k][i], (int)px[k - 1][i],
             (int)py[k - 1][i]);
      }
    }
  }
}
