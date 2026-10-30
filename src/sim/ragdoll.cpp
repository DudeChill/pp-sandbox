#include "ragdoll.h"

namespace sim {
namespace {

struct PartSpec {
    const char* name;
    Vec2 offset;   // body centre relative to the feet
    Vec2 half;
};

// Standing adult, roughly 1.75 m: head, torso, pelvis, arms, forearms,
// thighs, shins, feet. Density is scaled to give ~80 kg total.
constexpr PartSpec kParts[] = {
    {"head",       {0.00f, 1.65f}, {0.085f, 0.105f}},
    {"torso",      {0.00f, 1.29f}, {0.140f, 0.250f}},
    {"pelvis",     {0.00f, 0.95f}, {0.120f, 0.090f}},
    {"upper_arm_L", {-0.20f, 1.40f}, {0.045f, 0.160f}},
    {"upper_arm_R", { 0.20f, 1.40f}, {0.045f, 0.160f}},
    {"forearm_L",  {-0.20f, 1.08f}, {0.040f, 0.150f}},
    {"forearm_R",  { 0.20f, 1.08f}, {0.040f, 0.150f}},
    {"thigh_L",    {-0.09f, 0.64f}, {0.060f, 0.220f}},
    {"thigh_R",    { 0.09f, 0.64f}, {0.060f, 0.220f}},
    {"shin_L",     {-0.09f, 0.20f}, {0.050f, 0.210f}},
    {"shin_R",     { 0.09f, 0.20f}, {0.050f, 0.210f}},
    {"foot_L",     {-0.09f, 0.03f}, {0.050f, 0.030f}},
    {"foot_R",     { 0.09f, 0.03f}, {0.050f, 0.030f}},
};

constexpr float kDensity = 160.0f;

struct JointSpec {
    int a;
    int b;
    Vec2 anchor;   // feet-relative
    bool has_limit;
    float limit_min;
    float limit_max;
};

constexpr JointSpec kJoints[] = {
    {1, 0, {0.0f, 1.54f}, true, -0.6f, 0.6f},     // neck
    {1, 3, {-0.17f, 1.52f}, true, -2.6f, 2.6f},   // shoulder L
    {1, 4, {0.17f, 1.52f}, true, -2.6f, 2.6f},    // shoulder R
    {3, 5, {-0.20f, 1.24f}, true, 0.0f, 2.4f},    // elbow L
    {4, 6, {0.20f, 1.24f}, true, 0.0f, 2.4f},     // elbow R
    {1, 2, {0.0f, 1.04f}, true, -0.5f, 0.5f},     // waist
    {2, 7, {-0.09f, 0.88f}, true, -1.5f, 1.5f},   // hip L
    {2, 8, {0.09f, 0.88f}, true, -1.5f, 1.5f},    // hip R
    {7, 9, {-0.09f, 0.42f}, true, 0.0f, 2.3f},    // knee L
    {8, 10, {0.09f, 0.42f}, true, 0.0f, 2.3f},    // knee R
    {9, 11, {-0.09f, 0.015f}, true, -0.6f, 0.6f}, // ankle L
    {10, 12, {0.09f, 0.015f}, true, -0.6f, 0.6f}, // ankle R
};

} // namespace

Ragdoll spawn_ragdoll(World& world, Vec2 feet_pos) {
    Ragdoll r;
    for (int i = 0; i < Ragdoll::kParts; ++i) {
        const Vec2 center = feet_pos + kParts[i].offset;
        r.parts[i] = world.add_box(center, kParts[i].half, 0.0f, kDensity);
        world.get(r.parts[i]).friction = 0.8f;
        world.get(r.parts[i]).restitution = 0.0f;
    }
    for (int i = 0; i < Ragdoll::kJoints; ++i) {
        const JointSpec& s = kJoints[i];
        const Vec2 anchor = feet_pos + s.anchor;
        r.joints[i] = world.add_revolute_joint(r.parts[s.a], r.parts[s.b], anchor,
                                               s.has_limit, s.limit_min, s.limit_max);
    }
    return r;
}

} // namespace sim
