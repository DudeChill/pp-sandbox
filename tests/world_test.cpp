#include <chrono>
#include <cmath>

#include "framework.h"
#include "sim/alloc.h"
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
