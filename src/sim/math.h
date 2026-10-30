#pragma once

#include <cmath>

namespace sim {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline Vec2 operator*(float s, Vec2 a) { return {a.x * s, a.y * s}; }
inline Vec2 operator/(Vec2 a, float s) { return {a.x / s, a.y / s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }
inline Vec2& operator*=(Vec2& a, float s) { a.x *= s; a.y *= s; return a; }

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline Vec2 cross(float s, Vec2 v) { return {-s * v.y, s * v.x}; }
inline float length_sq(Vec2 a) { return dot(a, a); }
inline float length(Vec2 a) { return std::sqrt(length_sq(a)); }

inline Vec2 normalize(Vec2 a) {
    const float l = length(a);
    return l > 1e-12f ? a * (1.0f / l) : Vec2{1.0f, 0.0f};
}

inline Vec2 perp(Vec2 a) { return {-a.y, a.x}; }

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline Vec2 rotate(Vec2 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}

inline Vec2 inv_rotate(Vec2 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {c * v.x + s * v.y, -s * v.x + c * v.y};
}

} // namespace sim
