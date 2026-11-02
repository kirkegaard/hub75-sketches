#include "hub75.h"
#include "util.h"

static Vec2 a;
static Vec2 b;

void setup(void) {
  a = vec2(1.0f, 2.0f);
  b = vec2(2.0f, 1.0f);
}

void draw(void) {
  Vec2 center = vec2(width / 2.0f, height / 2.0f);
  Vec2 sum = vec2_add(a, b);
  Vec2 dir = vec2_normalized(sum);
  Vec2 spin = vec2_rotate(dir, (float)frameCount * 0.02f);
  float reach = vec2_distance(a, b) * 10.0f;
  float facing = vec2_dot(dir, spin);

  background(color(0, 0, 0));
  strokeWeight(4);
  stroke(color(255, 255, 255));
  line((int)center.x, (int)center.y, (int)(center.x + spin.x * reach),
       (int)(center.y + spin.y * reach));

  noFill();
  circle((int)center.x, (int)center.y, (int)((facing + 1.0f) * 15.0f));
}
