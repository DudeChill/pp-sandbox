#include "weapon.h"

#include <cmath>

namespace sim {
namespace {
constexpr float kGravity = 9.81f;
constexpr float kBoreMm = 20.0f;

float curve(float barrel_mm, float reference_mm) {
    const float k = std::max(reference_mm * 0.5f, 20.0f);
    const float l = std::max(barrel_mm, 0.0f) + kBoreMm;
    const float l_ref = reference_mm + kBoreMm;
    const float denom = std::sqrt(1.0f - std::exp(-l_ref / k));
    if (denom < 1e-6f) return 0.0f;
    return std::sqrt(1.0f - std::exp(-l / k)) / denom;
}
} // namespace

float muzzle_velocity(const AmmoSpec& ammo, float barrel_mm) {
    return ammo.muzzle_speed * curve(barrel_mm, ammo.reference_barrel_mm);
}

Vec2 launch_direction(Vec2 aim_dir, float muzzle_speed, float zero_m,
                      float sight_height_m) {
    const Vec2 dir = normalize(aim_dir);
    if (zero_m <= 0.01f || muzzle_speed <= 1.0f) return dir;
    const float time = zero_m / muzzle_speed;
    const float drop = 0.5f * kGravity * time * time;
    const float rise = drop + sight_height_m;
    const float angle = std::atan2(rise, zero_m);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {dir.x * c - dir.y * s, dir.x * s + dir.y * c};
}

ProjectileId fire_weapon(World& world, const WeaponSpec& weapon, Vec2 muzzle,
                         Vec2 aim_dir) {
    const AmmoSpec& ammo = ammo_spec(weapon.ammo_index);
    const float speed = muzzle_velocity(ammo, weapon.barrel_mm);
    const Vec2 dir = launch_direction(aim_dir, speed, weapon.zero_m,
                                      weapon.sight_height_m);
    return world.spawn_projectile(muzzle, dir * speed, ammo.mass_g / 1000.0f,
                                  ammo.radius_mm / 1000.0f, ammo.drag_k,
                                  ammo.fragmentation_speed, ammo.fragments,
                                  ammo.yaw_gain_per_m);
}

float default_barrel_mm(int ammo_index) {
    return ammo_spec(ammo_index).reference_barrel_mm;
}

} // namespace sim
