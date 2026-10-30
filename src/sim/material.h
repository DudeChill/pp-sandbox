#pragma once

#include <cstdint>

namespace sim {

enum class MaterialId : uint8_t {
    Default = 0,
    Steel,
    Aluminium,
    Wood,
    Glass,
    Concrete,
    Rubber,
    Bone,
    Flesh,
    Count
};

struct Material {
    const char* name;
    float density;         // kg/m^3
    float friction;
    float restitution;
    float youngs_modulus;  // Pa
    float yield_stress;    // Pa
    float ultimate_stress; // Pa
    float toughness;       // relative, drives crack arrest (M1+)
    float penetration_resistance; // Pa, calibrated against gelatin tests (M3)
    float fracture_energy;        // J, absolute threshold; 1e30 = never
};

const Material& material(MaterialId id);

} // namespace sim
