#pragma once

#include "world.h"

namespace sim {

struct Ragdoll {
    static constexpr int kParts = 13;
    static constexpr int kJoints = 12;
    BodyId parts[kParts]{};
    JointId joints[kJoints]{};
    BodyId head() const { return parts[0]; }
    BodyId torso() const { return parts[1]; }
};

// Spawns a 13-part humanoid standing with its feet at feet_pos.
Ragdoll spawn_ragdoll(World& world, Vec2 feet_pos);

} // namespace sim
