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

    // Tissue compartments in body-local space. Organs sit in the head, torso
    // and pelvis; limbs carry muscle, bone, and a medial artery.
    for (int i = 0; i < Ragdoll::kParts; ++i) {
        const Vec2 h = kParts[i].half;
        world.add_compartment(r.parts[i], Vec2{0.0f, 0.0f},
                              Vec2{h.x * 0.8f, h.y * 0.8f}, TissueKind::Muscle);
        if (i == 0) {
            world.add_compartment(r.parts[i], Vec2{h.x * 0.10f, h.y * 0.05f},
                                  Vec2{h.x * 0.55f, h.y * 0.55f}, TissueKind::Brain);
        } else if (i == 1) {
            world.add_compartment(r.parts[i], Vec2{-h.x * 0.25f, h.y * 0.15f},
                                  Vec2{h.x * 0.30f, h.y * 0.28f}, TissueKind::Heart);
            world.add_compartment(r.parts[i], Vec2{-h.x * 0.45f, h.y * 0.25f},
                                  Vec2{h.x * 0.25f, h.y * 0.45f}, TissueKind::Lung);
            world.add_compartment(r.parts[i], Vec2{h.x * 0.45f, h.y * 0.25f},
                                  Vec2{h.x * 0.25f, h.y * 0.45f}, TissueKind::Lung);
        } else if (i == 2) {
            world.add_compartment(r.parts[i], Vec2{0.0f, 0.0f},
                                  Vec2{h.x * 0.7f, h.y * 0.7f}, TissueKind::Gut);
        } else if (i >= 3 && i <= 10) {
            world.add_compartment(r.parts[i], Vec2{0.0f, 0.0f},
                                  Vec2{h.x * 0.30f, h.y * 0.85f}, TissueKind::Bone);
            const float side = (i == 3 || i == 5 || i == 7 || i == 9) ? 1.0f : -1.0f;
            world.add_compartment(r.parts[i], Vec2{side * h.x * 0.55f, 0.0f},
                                  Vec2{h.x * 0.10f, h.y * 0.70f}, TissueKind::Artery);
        } else {
            world.add_compartment(r.parts[i], Vec2{0.0f, 0.0f},
                                  Vec2{h.x * 0.4f, h.y * 0.5f}, TissueKind::Bone);
        }
    }

    // Vascular supply by part (ml/s at full tissue damage). The head and torso
    // are fatal; limbs bleed but are survivable without arterial involvement.
    constexpr float kVascularity[Ragdoll::kParts] = {
        60.0f, 50.0f, 30.0f, 12.0f, 12.0f, 10.0f, 10.0f,
        15.0f, 15.0f, 10.0f, 10.0f, 5.0f, 5.0f,
    };
    Vitals v;
    const uint32_t group = world.create_group(v);
    for (int i = 0; i < Ragdoll::kParts; ++i) {
        Body& b = world.get(r.parts[i]);
        b.group = group;
        b.material = MaterialId::Flesh;
        b.vascularity = kVascularity[i];
    }
    world.get(r.parts[0]).vital = true;
    world.get(r.parts[1]).vital = false;
    return r;
}

} // namespace sim
