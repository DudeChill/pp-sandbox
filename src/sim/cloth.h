#pragma once

#include "world.h"

namespace sim {

struct Cloth {
    static constexpr int kMaxNodes = 64;
    BodyId nodes[kMaxNodes]{};
    int count = 0;
    int cols = 0;
    int rows = 0;
};

// Mass-spring cloth: a cols x rows grid of small bodies with structural and
// shear distance joints. The top row is pinned to static anchors.
Cloth spawn_cloth(World& world, Vec2 top_left, int cols, int rows, float spacing,
                  float radius, float break_force = 5000.0f);

// Soft body: a ring of bodies cross-braced to a centre body; deforms like a
// mass-spring blob and is tearable.
Cloth spawn_soft_blob(World& world, Vec2 center, int segments, float radius,
                      float break_force = 900.0f);

} // namespace sim
