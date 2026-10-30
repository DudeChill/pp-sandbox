#pragma once

#include <algorithm>

#include "material.h"

namespace sim {

// Presentation mapping used by the procedural audio layer. Kept in sim/ so it
// is testable without a device; it never affects simulation state.
inline float impact_tone_hz(MaterialId material) {
    switch (material) {
        case MaterialId::Steel: return 2600.0f;
        case MaterialId::Aluminium: return 1800.0f;
        case MaterialId::Wood: return 520.0f;
        case MaterialId::Glass: return 3200.0f;
        case MaterialId::Concrete: return 260.0f;
        case MaterialId::Rubber: return 150.0f;
        case MaterialId::Bone: return 900.0f;
        case MaterialId::Flesh: return 220.0f;
        default: return 400.0f;
    }
}

inline float sound_delay_seconds(float metres) {
    constexpr float kSpeedOfSound = 343.0f;
    return std::max(0.0f, metres) / kSpeedOfSound;
}

inline float bang_duration_seconds(float energy_j) {
    return std::clamp(0.08f + energy_j / 40000.0f, 0.08f, 0.6f);
}

} // namespace sim
