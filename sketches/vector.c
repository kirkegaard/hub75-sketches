#include "hub75.h"
#include "util.h"

static Vec2 vectors[3];

void setup(void) {
  vectors[0] = vec2(20.0f, 0.0f);
  vectors[1] = vec2(10.0f, 10.0f);
  vectors[2] = vec2(-10.0f, -10.0f);
}

void draw(void) {
  Vec2 c = vec2(width / 2.0f, height / 2.0f);

  background(color(0, 0, 0));
  noFill();
  stroke(color(255, 255, 255));

  strokeWeight(1);
  stroke(color(255, 255, 255));
  circle((int)c.x, (int)c.y, 25);

  for (int i = 0; i < 3; i++) {
    float radius = vec2_len(vectors[i]);
    float angle = atan2f(vectors[i].y, vectors[i].x);
    float speed = 0.02f * (float)(i + 1);
    float spin = (float)frameCount * speed + (float)i * 0.5f;
    Vec2 tip = vec2_add(c, vec2_from_angle(angle + spin, radius));

    noFill();

    strokeWeight(1);
    stroke(color(255, 255, 255));
    line((int)c.x, (int)c.y, (int)tip.x, (int)tip.y);

    strokeWeight(1);
    stroke(color(255, 255, 255));
    circle((int)tip.x, (int)tip.y, 20);
  }
}
