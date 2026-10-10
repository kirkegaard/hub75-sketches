#include "hub75.h"

#include <math.h>

/* One rule: a tunnel of doorways. The camera flies forward through
   them along Z. Each doorway is a tall rectangle at its own depth,
   tilted only around Z (a slight roll), and perspective-projects down
   to a vanishing point. The nearest one grows past the camera, wraps
   to the far end, and the trip never stops. After the reference by
   Andreas Gysin, "Infinite travel and reorientation tests for X-Y-Z". */

static uint16_t bg;

typedef struct {
  float x;
  float y;
  float z;
} V3;

static V3 rotate(V3 p, float ax, float ay, float az) {
  V3 r;
  float c;
  float s;

  c = cosf(ax);
  s = sinf(ax);
  r.x = p.x;
  r.y = p.y * c - p.z * s;
  r.z = p.y * s + p.z * c;
  p = r;

  c = cosf(ay);
  s = sinf(ay);
  r.x = p.x * c + p.z * s;
  r.y = p.y;
  r.z = -p.x * s + p.z * c;
  p = r;

  c = cosf(az);
  s = sinf(az);
  r.x = p.x * c - p.y * s;
  r.y = p.x * s + p.y * c;
  r.z = p.z;
  return r;
}

void setup(void) { bg = color(0, 0, 0); }

void draw(void) {
  const int n = 5;
  const float near_z = 6.0f;
  const float spacing = 60.0f;
  const float range = spacing * (float)n;
  const float focal = 95.0f;
  const float half_w = 20.0f;
  const float half_h = 32.0f;
  const float speed = 1.4f;
  float t = (float)frameCount;
  float cx = (float)width * 0.5f;
  float cy = (float)height * 0.5f;
  float z[5];
  int order[5];
  int i;
  int k;

  background(bg);

  for (i = 0; i < n; i++) {
    float phase = fmodf((float)i * spacing - t * speed, range);

    if (phase < 0.0f) {
      phase += range;
    }
    z[i] = near_z + phase;
    order[i] = i;
  }

  /* Far to near, so the closest square draws on top. */
  for (i = 0; i < n; i++) {
    for (k = i + 1; k < n; k++) {
      if (z[order[k]] > z[order[i]]) {
        int tmp = order[i];
        order[i] = order[k];
        order[k] = tmp;
      }
    }
  }

  for (k = 0; k < n; k++) {
    int idx = order[k];
    float s = focal / z[idx];
    float az = 0.35f * sinf(t * 0.010f + (float)idx * 1.7f);
    int px[4];
    int py[4];
    int c;
    uint16_t col;

    for (c = 0; c < 4; c++) {
      V3 p;
      p.x = (c == 1 || c == 2) ? half_w : -half_w;
      p.y = (c == 2 || c == 3) ? half_h : -half_h;
      p.z = 0.0f;
      p = rotate(p, 0.0f, 0.0f, az);
      px[c] = (int)lroundf(cx + p.x * s);
      py[c] = (int)lroundf(cy + p.y * s);
    }

    if (idx % 3 == 0) {
      col = color(0, 255, 255);
    } else if (idx % 3 == 1) {
      col = color(255, 0, 255);
    } else {
      col = color(255, 255, 0);
    }

    stroke(col);
    for (c = 0; c < 4; c++) {
      line(px[c], py[c], px[(c + 1) % 4], py[(c + 1) % 4]);
    }
  }
}
