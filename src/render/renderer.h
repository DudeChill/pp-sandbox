#pragma once

#include <vector>

#include "sim/math.h"

namespace render {

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

class Renderer {
public:
    bool init();
    void shutdown();

    void begin(int width, int height, sim::Vec2 center, float view_height);
    void box(sim::Vec2 center, sim::Vec2 half, float angle, Color color);
    void circle(sim::Vec2 center, float radius, Color color);
    void line(sim::Vec2 a, sim::Vec2 b, float thickness, Color color);
    void end();

private:
    struct Instance {
        float x, y;
        float sx, sy;
        float angle;
        float r, g, b, a;
        float shape;
    };

    std::vector<Instance> instances_;
    unsigned vao_ = 0;
    unsigned quad_vbo_ = 0;
    unsigned inst_vbo_ = 0;
    unsigned program_ = 0;
    int uniform_vp_ = -1;
    int width_ = 0;
    int height_ = 0;
    int instance_capacity_ = 0;
};

} // namespace render
