#include "material.h"

namespace sim {
namespace {

// Values from docs/PHYSICS.md (engineering handbook ranges). 2D bodies assume
// an out-of-plane depth of kDepth2D when converting area to mass and strength.
constexpr Material kMaterials[] = {
    {"default",   1000.0f, 0.60f, 0.05f, 1.0e7f,   1.0e6f,   2.0e6f,   1.0f, 12.0e6f, 1.0e30f, 0.0f, 0.0f},
    {"steel",     7850.0f, 0.60f, 0.05f, 2.0e11f,  2.5e8f,   4.5e8f,   9.0f, 800.0e6f, 1.0e30f, 0.0f, 0.0f},
    {"aluminium", 2700.0f, 0.55f, 0.05f, 6.9e10f,  2.0e8f,   3.0e8f,   6.0f, 450.0e6f, 1.0e30f, 0.0f, 0.0f},
    {"oak",        700.0f, 0.55f, 0.05f, 1.0e10f,  5.0e7f,   9.0e7f,   3.0f, 40.0e6f, 1.0e30f, 300.0f, 0.02f},
    {"glass",     2500.0f, 0.40f, 0.02f, 7.0e10f,  5.0e7f,   5.0e7f,   0.5f, 12.0e6f, 25.0f, 0.0f, 0.0f},
    {"concrete",  2400.0f, 0.70f, 0.02f, 3.0e10f,  3.0e6f,   3.0e6f,   1.0f, 120.0e6f, 400.0f, 0.0f, 0.0f},
    {"rubber",    1100.0f, 0.85f, 0.60f, 1.0e7f,   2.0e7f,   3.0e7f,   8.0f, 15.0e6f, 1.0e30f, 350.0f, 0.05f},
    {"bone",      1900.0f, 0.60f, 0.05f, 1.7e10f,  1.3e8f,   1.5e8f,   1.5f, 60.0e6f, 1.0e30f, 0.0f, 0.0f},
    {"flesh",     1050.0f, 0.70f, 0.02f, 1.0e7f,   1.0e6f,   3.0e6f,   2.0f, 12.0e6f, 1.0e30f, 350.0f, 0.10f},
};
constexpr int kCount = static_cast<int>(MaterialId::Count);

} // namespace

const Material& material(MaterialId id) {
    const int i = static_cast<int>(id);
    return kMaterials[(i >= 0 && i < kCount) ? i : 0];
}

} // namespace sim
