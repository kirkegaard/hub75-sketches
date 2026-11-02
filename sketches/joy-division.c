#include "hub75.h"
#include "util.h"

#include <math.h>
#include <stdint.h>

/* One rule: the Unknown Pleasures cover. Each trace is one rotation of
   pulsar CP 1919, read as intensity against phase: a flat baseline with
   a sharp main pulse, a close companion, and weaker echoes to the right.
   The pulse position and height wander from trace to trace, so the stack
   fills in the same jagged mountain. Successive pulses are plotted
   upward, and the whole stack crawls up a fraction of a row at a time,
   so the mountain slowly rearranges itself without leaving the cover. */

#define MAX_TRACES 32

static int plot_x;
static int plot_w;
static int trace_count;
static int trace_base[MAX_TRACES];
static int max_rise;

/* Value noise: one random value per integer, eased between them. The
   calls in profile() choose each trace's peak heights and positions. */
static float noise1(float x, uint32_t seed) {
  int i = (int)floorf(x);
  float f = x - (float)i;
  float a = hash_i01((int)hash_combine2(i, (int)seed));
  float b = hash_i01((int)hash_combine2(i + 1, (int)seed));

  return lerp(a, b, smoothstep(f));
}

/* A Gaussian whose two halves may have different widths, so the main
   pulse can rise steeply and fall away into a slower shoulder. */
static float pulse(float d, float left, float right) {
  float s = d < 0.f ? left : right;

  return expf(-0.5f * (d / s) * (d / s));
}

/* Normalized height at u in [0, 1] for a trace at (fractional) row r.
   The broad body keeps the peaks from floating apart, while the narrow
   components make the sharp fingers along the ridge. */
static float profile(float u, float r) {
  float cx = 0.43f + 0.035f * sinf(r * 0.43f + 0.4f) +
             0.014f * sinf(r * 1.1f + 2.1f) +
             (noise1(r * 1.7f + 0.5f, 21u) - 0.5f) * 0.040f;
  float w = 0.026f + 0.007f * noise1(r * 1.3f + 1.f, 10u);
  float left = w * (0.80f + 0.22f * noise1(r * 0.8f + 3.f, 11u));
  float right = w * (1.15f + 0.30f * noise1(r * 0.7f + 5.f, 12u));
  float c2 = cx + 0.028f + 0.015f * noise1(r * 0.6f + 7.f, 13u);
  float e1 = cx + 0.135f + 0.028f * noise1(r * 0.45f + 4.f, 15u);
  float e1b = cx + 0.170f + 0.020f * noise1(r * 0.7f + 2.f, 19u);
  float e2 = cx + 0.250f + 0.025f * noise1(r * 0.4f + 9.f, 17u);
  float h;

  h = 0.95f * pulse(u - cx, left, right);
  h += 0.50f * (0.30f + 0.70f * noise1(r * 0.9f + 2.f, 14u)) *
       pulse(u - c2, 0.014f, 0.014f);
  h += (0.22f + 0.25f * noise1(r * 0.55f + 8.f, 16u)) *
       pulse(u - e1, 0.032f, 0.032f);
  h += (0.12f + 0.20f * noise1(r * 0.8f + 4.f, 20u)) *
       pulse(u - e1b, 0.018f, 0.018f);
  h += (0.06f + 0.12f * noise1(r * 0.6f + 6.f, 18u)) *
       pulse(u - e2, 0.024f, 0.024f);
  h += 0.10f * pulse(u - cx - 0.04f, 0.110f, 0.110f);

  /* A trace is not a perfect ruled line: give the flats a little grain. */
  h += (noise1(u * (float)plot_w * 0.20f + r * 5.f, 77u) - 0.5f) * 0.022f;

  return h * (0.58f + 0.42f * noise1(r * 1.9f + 4.f, 0x1234u));
}

static int rise_at(int x, float row) {
  float u = (float)x / (float)(plot_w - 1);
  int rise = (int)lroundf(profile(u, row) * (float)max_rise);

  return clamp_int(rise, -1, max_rise);
}

void setup(void) {
  int spacing = 2;
  int bottom = height - 10;
  int top_base;
  int i;

  plot_w = width * 3 / 4;
  if (plot_w < 4) {
    plot_w = width;
  }
  plot_x = (width - plot_w) / 2;

  max_rise = clamp_int(height / 6, 3, 10);
  top_base = 1 + max_rise;
  trace_count = (bottom - top_base) / spacing + 1;
  trace_count = clamp_int(trace_count, 2, MAX_TRACES);

  for (i = 0; i < trace_count; i++) {
    trace_base[i] = bottom - i * spacing;
  }
}

void draw(void) {
  float t = (float)frameCount * 0.015f;
  int r;

  background(color(0, 0, 0));
  stroke(color(255, 255, 255));
  strokeWeight(1);

  for (r = 0; r < trace_count; r++) {
    float row = (float)r - t;
    int base = trace_base[r];
    int prev_y = base - rise_at(0, row);
    int x;

    for (x = 1; x < plot_w; x++) {
      int y = base - rise_at(x, row);

      line(plot_x + x - 1, prev_y, plot_x + x, y);
      prev_y = y;
    }
  }
}
