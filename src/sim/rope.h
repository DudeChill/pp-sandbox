#pragma once

#include "world.h"

namespace sim {

struct Rope {
    static constexpr int kMaxLinks = 48;
    BodyId links[kMaxLinks]{};
    JointId joints[kMaxLinks]{};
    int count = 0;
};

// Spawns a hanging rope anchored to a static point at `anchor`.
// Returns handles for every link and joint (joint[0] connects the anchor body).
Rope spawn_rope(World& world, Vec2 anchor, int links, float segment_length,
                float radius, float break_force);

} // namespace sim
