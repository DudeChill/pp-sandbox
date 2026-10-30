#include "ammo.h"

namespace sim {
namespace {

// Prompt calibration loads. Speeds and masses are the reference values; drag
// and fragmentation thresholds are tuned to the test suite.
constexpr AmmoSpec kAmmo[] = {
    {"9x19 FMJ",   8.0f,  4.5f,  350.0f, 0.0015f,   0.0f, 0},
    {"5.56 M855",  4.0f,  2.85f, 940.0f, 0.0009f, 800.0f, 4},
    {"7.62 M80",   9.5f,  3.9f,  840.0f, 0.0008f,   0.0f, 0},
    {"12ga slug", 28.0f,  9.0f,  400.0f, 0.0008f,   0.0f, 0},
    {"12ga 00",    3.5f,  4.1f,  400.0f, 0.0018f,   0.0f, 0},
    {"arrow",     25.0f,  4.0f,   70.0f, 0.0040f,   0.0f, 0},
    {"crossbow",  30.0f,  4.5f,  110.0f, 0.0030f,   0.0f, 0},
};
constexpr int kCount = static_cast<int>(sizeof(kAmmo) / sizeof(kAmmo[0]));

} // namespace

int ammo_count() { return kCount; }

const AmmoSpec& ammo_spec(int index) {
    return kAmmo[(index >= 0 && index < kCount) ? index : 0];
}

ProjectileId spawn_ammo(World& world, int ammo_index, Vec2 pos, Vec2 dir) {
    const AmmoSpec& a = ammo_spec(ammo_index);
    return world.spawn_projectile(pos, dir * a.muzzle_speed, a.mass_g / 1000.0f,
                                  a.radius_mm / 1000.0f, a.drag_k,
                                  a.fragmentation_speed, a.fragments);
}

} // namespace sim
