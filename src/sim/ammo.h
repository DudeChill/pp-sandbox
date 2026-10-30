#pragma once

#include <cstdint>

#include "world.h"

namespace sim {

struct AmmoSpec {
    const char* name;
    float mass_g;
    float radius_mm;
    float muzzle_speed;
    float drag_k;
    float fragmentation_speed;  // m/s; 0 = never fragments
    uint8_t fragments;
};

int ammo_count();
const AmmoSpec& ammo_spec(int index);

// Spawns a projectile from an ammunition class along `dir` at muzzle speed.
ProjectileId spawn_ammo(World& world, int ammo_index, Vec2 pos, Vec2 dir);

} // namespace sim
