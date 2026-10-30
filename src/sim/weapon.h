#pragma once

#include "ammo.h"
#include "world.h"

namespace sim {

struct WeaponSpec {
    int ammo_index = 0;
    float barrel_mm = 120.0f;
    float zero_m = 100.0f;
    float sight_height_m = 0.0f;
};

// Smooth, saturating interior-ballistics curve normalised to the class's
// reference barrel: v(barrel) = v_ref * sqrt(1 - e^(-L/k)) / sqrt(1 - e^(-Lref/k)).
float muzzle_velocity(const AmmoSpec& ammo, float barrel_mm);

// Rotates the aim direction up by the elevation needed to hit `zero_m`,
// ignoring drag. Positive zero always raises the bore.
Vec2 launch_direction(Vec2 aim_dir, float muzzle_speed, float zero_m,
                      float sight_height_m);

ProjectileId fire_weapon(World& world, const WeaponSpec& weapon, Vec2 muzzle,
                         Vec2 aim_dir);

float default_barrel_mm(int ammo_index);

} // namespace sim
