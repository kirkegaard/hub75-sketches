#ifndef HUB75_UTIL_VECTOR_H
#define HUB75_UTIL_VECTOR_H

#include <math.h>

/* Small value-type vectors. Vec2 lives in the panel plane, Vec3 in
   space. Value-returning helpers are the default, so calls can chain
   without pointers or hidden state. If a mutating helper earns its
   keep, name it with an explicit _inplace (or _to) suffix. */

typedef struct {
  float x;
  float y;
} Vec2;

typedef struct {
  float x;
  float y;
  float z;
} Vec3;

static inline Vec2 vec2(float x, float y) {
  Vec2 v;

  v.x = x;
  v.y = y;
  return v;
}

static inline Vec3 vec3(float x, float y, float z) {
  Vec3 v;

  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
  return vec2(a.x + b.x, a.y + b.y);
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
  return vec2(a.x - b.x, a.y - b.y);
}

static inline Vec2 vec2_scale(Vec2 v, float s) {
  return vec2(v.x * s, v.y * s);
}

static inline float vec2_dot(Vec2 a, Vec2 b) {
  return a.x * b.x + a.y * b.y;
}

static inline float vec2_len_sq(Vec2 v) { return vec2_dot(v, v); }

static inline float vec2_len(Vec2 v) { return sqrtf(vec2_len_sq(v)); }

static inline Vec2 vec2_normalized(Vec2 v) {
  float len = vec2_len(v);

  if (len <= 0.0f) {
    return vec2(0.0f, 0.0f);
  }
  return vec2_scale(v, 1.0f / len);
}

static inline Vec2 vec2_perp(Vec2 v) { return vec2(-v.y, v.x); }

static inline Vec2 vec2_rotate(Vec2 v, float radians) {
  float s = sinf(radians);
  float c = cosf(radians);

  return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static inline Vec2 vec2_from_angle(float radians, float length) {
  return vec2(cosf(radians) * length, sinf(radians) * length);
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
  return vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

static inline float vec2_distance(Vec2 a, Vec2 b) {
  return vec2_len(vec2_sub(a, b));
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
  return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline Vec3 vec3_scale(Vec3 v, float s) {
  return vec3(v.x * s, v.y * s, v.z * s);
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
  return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

static inline float vec3_len_sq(Vec3 v) { return vec3_dot(v, v); }

static inline float vec3_len(Vec3 v) { return sqrtf(vec3_len_sq(v)); }

static inline Vec3 vec3_normalized(Vec3 v) {
  float len = vec3_len(v);

  if (len <= 0.0f) {
    return vec3(0.0f, 0.0f, 0.0f);
  }
  return vec3_scale(v, 1.0f / len);
}

static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
  return vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t);
}

static inline float vec3_distance(Vec3 a, Vec3 b) {
  return vec3_len(vec3_sub(a, b));
}

#endif
