#include <chrono>
#include <cmath>

#include "framework.h"
#include "sim/alloc.h"
#include "sim/ragdoll.h"
#include "sim/rope.h"
#include "sim/world.h"

using namespace sim;

namespace {

World make_ground() {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{200.0f, 0.5f});
    return w;
}

} // namespace

TEST_CASE("free fall matches analytic displacement") {
    World w;
    const BodyId id = w.add_circle(Vec2{0.0f, 10.0f}, 0.25f);
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in);
    const float y = w.get(id).pos.y;
    CHECK_NEAR(y, 10.0f - 0.5f * 9.81f, 0.05f);
    CHECK(w.get(id).vel.y < -9.5f);
}

TEST_CASE("circle rests on static floor") {
    World w = make_ground();
    const BodyId id = w.add_circle(Vec2{0.0f, 2.0f}, 0.5f);
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);
    const Body& b = w.get(id);
    CHECK_NEAR(b.pos.y, 0.5f, 0.02f);
    CHECK(std::fabs(b.vel.y) < 0.1f);
    CHECK(std::fabs(b.vel.x) < 0.1f);
}

TEST_CASE("five box stack stays aligned") {
    World w = make_ground();
    BodyId ids[5];
    for (int i = 0; i < 5; ++i) {
        ids[i] = w.add_box(Vec2{1.37f, 0.5f + static_cast<float>(i)}, Vec2{0.5f, 0.5f});
    }
    InputFrame in;
    for (int i = 0; i < 1200; ++i) w.step(in);
    for (int i = 0; i < 5; ++i) {
        const Body& b = w.get(ids[i]);
        CHECK_NEAR(b.pos.x, 1.37f, 0.03f);
        CHECK_NEAR(b.pos.y, 0.5f + static_cast<float>(i), 0.03f);
    }
}

TEST_CASE("six box tower survives thirty seconds") {
    World w = make_ground();
    BodyId ids[6];
    for (int i = 0; i < 6; ++i) {
        ids[i] = w.add_box(Vec2{1.37f, 0.5f + static_cast<float>(i)}, Vec2{0.5f, 0.5f});
    }
    InputFrame in;
    for (int i = 0; i < 7200; ++i) w.step(in);
    for (int i = 0; i < 6; ++i) {
        const Body& b = w.get(ids[i]);
        CHECK_NEAR(b.pos.x, 1.37f, 0.05f);
        CHECK_NEAR(b.pos.y, 0.5f + static_cast<float>(i), 0.05f);
    }
}

TEST_CASE("momentum is conserved in a collision") {
    World w;
    const BodyId a = w.add_circle(Vec2{-2.0f, 0.0f}, 0.4f, 1000.0f);
    const BodyId b = w.add_circle(Vec2{2.0f, 0.0f}, 0.6f, 1000.0f);
    w.get(a).vel = Vec2{3.0f, 0.0f};
    w.get(b).vel = Vec2{-1.0f, 0.0f};
    const float p0 = w.get(a).mass * w.get(a).vel.x + w.get(b).mass * w.get(b).vel.x;

    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in);

    const float p1 = w.get(a).mass * w.get(a).vel.x + w.get(b).mass * w.get(b).vel.x;
    CHECK_NEAR(p1, p0, 1e-3f);
}

TEST_CASE("deterministic across runs") {
    auto build = [] {
        World w = make_ground();
        for (int i = 0; i < 20; ++i) {
            w.add_box(Vec2{static_cast<float>(i % 5) * 1.02f - 2.0f,
                           0.6f + static_cast<float>(i / 5) * 1.02f},
                      Vec2{0.5f, 0.5f});
        }
        return w;
    };

    World w1 = build();
    World w2 = build();
    InputFrame in;
    for (int i = 0; i < 3000; ++i) {
        w1.step(in);
        w2.step(in);
        if ((i % 100) == 0) CHECK(w1.state_hash() == w2.state_hash());
    }
    CHECK(w1.state_hash() == w2.state_hash());
}

TEST_CASE("step performs no heap allocation after warmup") {
    World w = make_ground();
    for (int i = 0; i < 12; ++i) {
        w.add_box(Vec2{static_cast<float>(i % 4) * 1.05f, 0.6f + static_cast<float>(i / 4)},
                  Vec2{0.5f, 0.5f});
    }
    InputFrame in;
    for (int i = 0; i < 16; ++i) w.step(in);

    AllocationGuard guard;
    for (int i = 0; i < 8; ++i) w.step(in);
    CHECK(guard.allocations() == 0);
}

TEST_CASE("stale handles are rejected") {
    World w;
    const BodyId id = w.add_circle(Vec2{0.0f, 0.0f}, 0.5f);
    CHECK(w.alive(id));
    w.remove(id);
    CHECK(!w.alive(id));
    const BodyId again = w.add_circle(Vec2{1.0f, 0.0f}, 0.5f);
    CHECK(again.index == id.index);
    CHECK(again.generation != id.generation);
    CHECK(!w.alive(id));
}

TEST_CASE("pick finds the topmost body under a point") {
    World w;
    const BodyId a = w.add_circle(Vec2{0.0f, 0.0f}, 1.0f);
    const BodyId b = w.add_circle(Vec2{0.5f, 0.0f}, 0.5f);
    CHECK(w.pick(Vec2{0.5f, 0.0f}) == b);
    CHECK(w.pick(Vec2{-0.8f, 0.0f}) == a);
    CHECK(!w.pick(Vec2{50.0f, 0.0f}).valid());
}

TEST_CASE("500 bodies meet the 60 fps step budget") {
    World w = make_ground();
    for (int i = 0; i < 500; ++i) {
        w.add_box(Vec2{static_cast<float>(i % 25) * 0.9f - 10.0f,
                       0.6f + static_cast<float>(i / 25) * 1.05f},
                  Vec2{0.4f, 0.4f});
    }
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in); // settle

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 240; ++i) w.step(in);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / 240.0;
    std::printf("  avg step: %.3f ms (%u bodies, %u contacts)\n",
                ms, w.body_count(), w.contact_count());
    CHECK(ms < 16.6);
}

TEST_CASE("distance joint holds its length under gravity") {
    World w;
    const BodyId anchor = w.add_static_box(Vec2{0.0f, 3.0f}, Vec2{0.1f, 0.1f});
    const BodyId bob = w.add_box(Vec2{0.0f, 2.0f}, Vec2{0.1f, 0.1f});
    const JointId j = w.add_distance_joint(anchor, bob, Vec2{0.0f, 3.0f}, Vec2{0.0f, 2.0f}, 1.0f);
    InputFrame in;
    for (int i = 0; i < 720; ++i) w.step(in);
    const float d = length(w.get(bob).pos - w.get(anchor).pos);
    CHECK_NEAR(d, 1.0f, 0.02f);
    CHECK(w.joint_alive(j));
}

TEST_CASE("revolute joint keeps its anchor coincident") {
    World w;
    const BodyId anchor = w.add_static_box(Vec2{0.0f, 3.0f}, Vec2{0.1f, 0.1f});
    const BodyId arm = w.add_box(Vec2{0.0f, 2.5f}, Vec2{0.05f, 0.5f});
    const JointId j = w.add_revolute_joint(anchor, arm, Vec2{0.0f, 3.0f});
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);
    const Vec2 pa = w.get(anchor).pos + rotate(w.joint(j).local_a, w.get(anchor).angle);
    const Vec2 pb = w.get(arm).pos + rotate(w.joint(j).local_b, w.get(arm).angle);
    CHECK_NEAR(length(pb - pa), 0.0f, 0.01f);
}

TEST_CASE("weld joint preserves relative angle") {
    World w;
    const BodyId a = w.add_box(Vec2{0.0f, 2.0f}, Vec2{0.25f, 0.1f});
    const BodyId b = w.add_box(Vec2{0.5f, 2.0f}, Vec2{0.25f, 0.1f});
    const JointId j = w.add_weld_joint(a, b, Vec2{0.25f, 2.0f});
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);
    CHECK_NEAR(w.get(a).angle, w.get(b).angle, 0.01f);
    CHECK(w.joint_alive(j));
}

TEST_CASE("breakable joint fails above its rated force") {
    World w;
    const BodyId anchor = w.add_static_box(Vec2{0.0f, 5.0f}, Vec2{0.1f, 0.1f});
    const BodyId heavy = w.add_box(Vec2{0.0f, 4.0f}, Vec2{0.3f, 0.3f});
    const JointId j = w.add_distance_joint(anchor, heavy, Vec2{0.0f, 5.0f},
                                           Vec2{0.0f, 4.0f}, 1.0f, 100.0f);
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in);
    CHECK(!w.joint_alive(j));
    CHECK(w.get(heavy).pos.y < 3.0f);
}

TEST_CASE("ragdoll collapses plausibly and stays assembled") {
    World w = make_ground();
    Ragdoll r = spawn_ragdoll(w, Vec2{0.0f, 0.02f});
    w.get(r.torso()).vel = Vec2{1.2f, 0.0f}; // topple
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);

    for (int i = 0; i < Ragdoll::kParts; ++i) {
        const Body& b = w.get(r.parts[i]);
        CHECK(std::fabs(b.pos.x) < 3.0f);
        CHECK(b.pos.y > -0.2f);
        CHECK(b.pos.y < 2.0f);
    }
    for (int i = 0; i < Ragdoll::kJoints; ++i) {
        CHECK(w.joint_alive(r.joints[i]));
        const Joint& j = w.joint(r.joints[i]);
        const BodyId ba = w.id_at(j.a);
        const BodyId bb = w.id_at(j.b);
        CHECK(ba.valid());
        CHECK(bb.valid());
        const Vec2 pa = w.get(ba).pos + rotate(j.local_a, w.get(ba).angle);
        const Vec2 pb = w.get(bb).pos + rotate(j.local_b, w.get(bb).angle);
        CHECK(length(pb - pa) < 0.05f);
    }
    CHECK(w.get(r.head()).pos.y < 1.2f);
}

TEST_CASE("rope holds a load and keeps its segment lengths") {
    World w;
    Rope r = spawn_rope(w, Vec2{0.0f, 6.0f}, 6, 0.22f, 0.045f, 0.0f);
    const BodyId load = w.add_box(Vec2{0.0f, 6.0f - 6.0f * 0.22f - 0.2f}, Vec2{0.15f, 0.15f});
    w.add_distance_joint(r.links[5], load, w.get(r.links[5]).pos, w.get(load).pos, 0.2f, 0.0f);
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);
    for (int i = 0; i < r.count; ++i) CHECK(w.joint_alive(r.joints[i]));
    CHECK(w.get(r.links[5]).pos.y < 5.0f);
    CHECK(w.get(load).pos.y < 4.6f);
}

TEST_CASE("rope breaks above its rated tension") {
    World w;
    Rope r = spawn_rope(w, Vec2{0.0f, 6.0f}, 4, 0.2f, 0.04f, 200.0f);
    const BodyId load = w.add_box(Vec2{0.0f, 5.0f}, Vec2{0.3f, 0.3f});
    w.add_distance_joint(r.links[3], load, w.get(r.links[3]).pos, w.get(load).pos, 0.2f, 0.0f);
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in);
    int alive = 0;
    for (int i = 0; i < r.count; ++i) alive += w.joint_alive(r.joints[i]) ? 1 : 0;
    CHECK(alive < r.count);
    CHECK(w.get(load).pos.y < 4.0f);
}

TEST_CASE("weld plastic hinge yields within 10% of section prediction") {
    constexpr float kHalfHeight = 0.01f;
    constexpr float kDepth = 0.05f;
    constexpr float kSection = kDepth * (2.0f * kHalfHeight) * (2.0f * kHalfHeight) / 6.0f;
    const Material& steel = material(MaterialId::Steel);
    const float m_yield = steel.yield_stress * kSection;
    const float lever = 1.75f;
    const float f_yield = m_yield / lever;

    auto build = []() {
        World w;
        w.set_gravity(Vec2{0.0f, 0.0f});
        const BodyId wall = w.add_static_box(Vec2{0.0f, 0.0f}, Vec2{0.1f, 0.1f});
        w.set_material(wall, MaterialId::Steel);
        BodyId parts[4];
        for (int i = 0; i < 4; ++i) {
            parts[i] = w.add_box(Vec2{0.4f + 0.5f * static_cast<float>(i), 0.0f},
                                 Vec2{0.25f, kHalfHeight});
            w.set_material(parts[i], MaterialId::Steel);
        }
        w.add_weld_joint(wall, parts[0], Vec2{0.15f, 0.0f});
        for (int i = 0; i < 3; ++i) {
            w.add_weld_joint(parts[i], parts[i + 1],
                             Vec2{0.65f + 0.5f * static_cast<float>(i), 0.0f});
        }
        return std::tuple<World, BodyId, BodyId>(std::move(w), parts[3], wall);
    };

    {
        auto [w, tip, wall] = build();
        (void)wall;
        JointId root{};
        w.for_each_joint([&](JointId id, const Joint&) { if (!root.valid()) root = id; });
        const float before = w.joint(root).ref_angle;
        InputFrame in;
        for (int i = 0; i < 480; ++i) {
            const float ramp = std::min(1.0f, static_cast<float>(i + 1) * World::kDt);
            w.get(tip).force = Vec2{0.0f, -0.9f * f_yield * ramp};
            w.step(in);
        }
        CHECK_NEAR(w.joint(root).ref_angle, before, 0.003f);
        CHECK(w.joint_alive(root));
    }
    {
        auto [w, tip, wall] = build();
        (void)wall;
        JointId root{};
        w.for_each_joint([&](JointId id, const Joint&) { if (!root.valid()) root = id; });
        const float before = w.joint(root).ref_angle;
        InputFrame in;
        for (int i = 0; i < 480; ++i) {
            const float ramp = std::min(1.0f, static_cast<float>(i + 1) * World::kDt);
            w.get(tip).force = Vec2{0.0f, -1.1f * f_yield * ramp};
            w.step(in);
        }
        CHECK(std::fabs(w.joint(root).ref_angle - before) > 0.01f);
        CHECK(w.joint_alive(root));
    }
}

TEST_CASE("projectile falls under gravity and is slowed by drag") {
    World w;
    const ProjectileId free_fall = w.spawn_projectile(Vec2{0.0f, 10.0f}, Vec2{0.0f, 0.0f},
                                                      0.008f, 0.0045f, 0.0f);
    const ProjectileId dragged = w.spawn_projectile(Vec2{0.0f, 10.0f}, Vec2{350.0f, 0.0f},
                                                    0.008f, 0.0045f, 0.002f);
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in);
    CHECK_NEAR(w.projectile(free_fall).pos.y, 10.0f - 0.5f * 9.81f, 0.1f);
    CHECK(length(w.projectile(dragged).vel) < 250.0f);
    CHECK(length(w.projectile(dragged).vel) > 100.0f);
}

TEST_CASE("steel plate stops a pistol round, flesh lets it through") {
    World w;
    InputFrame in;

    const BodyId steel = w.add_static_box(Vec2{0.0f, 1.0f}, Vec2{0.05f, 0.5f});
    w.set_material(steel, MaterialId::Steel);
    const BodyId flesh = w.add_static_box(Vec2{4.0f, 1.0f}, Vec2{0.05f, 0.5f});
    w.set_material(flesh, MaterialId::Flesh);

    const ProjectileId a = w.spawn_projectile(Vec2{-1.0f, 1.0f}, Vec2{350.0f, 0.0f},
                                              0.008f, 0.0045f, 0.0f);
    const ProjectileId b = w.spawn_projectile(Vec2{3.0f, 1.0f}, Vec2{350.0f, 0.0f},
                                              0.008f, 0.0045f, 0.0f);
    for (int i = 0; i < 30; ++i) w.step(in);
    CHECK(!w.projectile_alive(a));
    CHECK(w.projectile_alive(b));
    CHECK(w.projectile(b).pos.x > 4.0f);
}

TEST_CASE("headshot kills, thigh shot bleeds without immediate death") {
    auto shoot = [](Vec2 target, Vec2 from) {
        World w;
        w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
        Ragdoll r = spawn_ragdoll(w, Vec2{0.0f, 0.02f});
        const ProjectileId p = w.spawn_projectile(from, normalize(target - from) * 350.0f,
                                                  0.008f, 0.0045f, 0.0f);
        (void)p;
        InputFrame in;
        for (int i = 0; i < 1200; ++i) w.step(in);
        const uint32_t group = w.get(r.head()).group;
        return std::tuple<World, Ragdoll, uint32_t>(std::move(w), r, group);
    };

    {
        auto [w, r, group] = shoot(Vec2{0.0f, 1.67f}, Vec2{-2.0f, 1.67f});
        (void)r;
        CHECK(w.vitals(group).dead);
    }
    {
        auto [w, r, group] = shoot(Vec2{-0.09f, 0.66f}, Vec2{-2.0f, 0.66f});
        (void)r;
        CHECK(!w.vitals(group).dead);
        CHECK(w.vitals(group).blood_ml < 5200.0f);
    }
}

TEST_CASE("pistol penetration depth lands within 15% of the calibration target") {
    auto shoot_block = [](float half_thickness) {
        World w;
        const BodyId block = w.add_static_box(Vec2{0.0f, 1.0f}, Vec2{half_thickness, 0.5f});
        w.set_material(block, MaterialId::Flesh);
        const ProjectileId p = w.spawn_projectile(Vec2{-2.0f, 1.0f}, Vec2{350.0f, 0.0f},
                                                  0.008f, 0.0045f, 0.0f);
        InputFrame in;
        for (int i = 0; i < 60; ++i) w.step(in);
        return w.projectile_alive(p);
    };
    // 9 mm FMJ at 350 m/s deposits 490 J; at 12 MPa and a 6.4e-5 m^2 cross-section
    // the predicted depth is 0.64 m, so a 0.60 m block is perforated and a
    // 0.70 m block is not.
    CHECK(shoot_block(0.30f));
    CHECK(!shoot_block(0.35f));
}

TEST_CASE("tourniquet stops bleeding from a limb wound") {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
    Ragdoll r = spawn_ragdoll(w, Vec2{0.0f, 0.02f});
    const BodyId thigh = r.parts[7];
    w.spawn_projectile(Vec2{-2.0f, 0.66f}, Vec2{350.0f, 0.0f}, 0.008f, 0.0045f, 0.0f);
    InputFrame in;
    for (int i = 0; i < 480; ++i) w.step(in);
    const uint32_t group = w.get(thigh).group;
    const float bleeding = w.vitals(group).blood_ml;
    CHECK(w.get(thigh).bleed_ml_s > 0.0f);

    for (int i = 0; i < Ragdoll::kParts; ++i) {
        if (w.get(r.parts[i]).bleed_ml_s > 0.0f) w.apply_tourniquet(r.parts[i]);
    }
    for (int i = 0; i < 480; ++i) w.step(in);
    const float treated = w.vitals(group).blood_ml;
    CHECK(treated >= bleeding - 1.0f);
    CHECK(!w.vitals(group).dead);
}

TEST_CASE("severed artery hemorrhages far more than muscle damage") {
    auto shoot = [](bool with_artery) {
        World w;
        const BodyId block = w.add_static_box(Vec2{0.0f, 0.0f}, Vec2{0.1f, 0.1f});
        w.set_material(block, MaterialId::Flesh);
        const uint32_t group = w.create_group(Vitals{});
        w.get(block).group = group;
        w.add_compartment(block, Vec2{0.0f, 0.0f}, Vec2{0.08f, 0.08f}, TissueKind::Muscle);
        if (with_artery) {
            w.add_compartment(block, Vec2{0.0f, 0.0f}, Vec2{0.02f, 0.02f}, TissueKind::Artery);
        }
        w.spawn_projectile(Vec2{-2.0f, 0.0f}, Vec2{350.0f, 0.0f}, 0.008f, 0.0045f, 0.0f);
        InputFrame in;
        for (int i = 0; i < 30; ++i) w.step(in);
        return w.get(block).bleed_ml_s;
    };
    CHECK(shoot(true) > 5.0f);
    CHECK(shoot(false) < 2.0f);
}

TEST_CASE("repeated blade strokes sever a limb at its joint") {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
    Ragdoll r = spawn_ragdoll(w, Vec2{0.0f, 0.02f});
    const BodyId arm = r.parts[3];
    const Vec2 point = w.get(arm).pos;
    const uint32_t before = w.joint_count();
    bool severed = false;
    for (int i = 0; i < 8 && !severed; ++i) {
        severed = w.apply_blade(arm, point, Vec2{1.0f, 0.0f}, 400.0f);
    }
    CHECK(severed);
    CHECK(w.joint_count() < before);
    CHECK(w.get(arm).bleed_ml_s > 0.0f);
}

TEST_CASE("scene binary round-trip preserves state and future evolution") {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
    Ragdoll r = spawn_ragdoll(w, Vec2{0.0f, 0.02f});
    w.spawn_projectile(Vec2{-2.0f, 0.66f}, Vec2{350.0f, 0.0f}, 0.008f, 0.0045f, 0.0f);
    w.add_box(Vec2{3.0f, 4.0f}, Vec2{0.3f, 0.3f});
    InputFrame in;
    for (int i = 0; i < 120; ++i) w.step(in);

    std::vector<uint8_t> blob;
    CHECK(w.save_scene(blob));
    CHECK(blob.size() > 64);

    World loaded;
    CHECK(loaded.load_scene(blob.data(), blob.size()));
    CHECK(loaded.state_hash() == w.state_hash());

    for (int i = 0; i < 240; ++i) {
        w.step(in);
        loaded.step(in);
        if ((i % 60) == 0) CHECK(loaded.state_hash() == w.state_hash());
    }
    CHECK(loaded.state_hash() == w.state_hash());

    std::vector<uint8_t> corrupt = blob;
    corrupt[3] ^= 0xFF;
    World bad;
    CHECK(!bad.load_scene(corrupt.data(), corrupt.size()));
    CHECK(!bad.load_scene(blob.data(), 8));
}
