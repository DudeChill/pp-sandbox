#pragma once

#include <cstdint>
#include <vector>

#include "handle.h"
#include "math.h"

namespace sim {

enum class Shape : uint8_t { Circle = 0, Box = 1 };

struct Body {
    Vec2 pos{};
    Vec2 vel{};
    Vec2 force{};
    float angle = 0.0f;
    float ang_vel = 0.0f;
    float torque = 0.0f;

    Vec2 half{0.5f, 0.5f};
    float radius = 0.5f;
    float mass = 1.0f;
    float inv_mass = 0.0f;
    float inv_inertia = 0.0f;

    float friction = 0.6f;
    float restitution = 0.05f;

    Shape shape = Shape::Circle;
    uint32_t generation = 0;
    bool dynamic = true;
    bool asleep = false;
    float sleep_timer = 0.0f;
};

struct InputFrame {
    BodyId grab{};
    Vec2 grab_target{};
    bool grab_active = false;
};

struct AABB {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
};

struct ContactPoint {
    Vec2 p{};
    Vec2 local_a{};
    Vec2 local_b{};
    float depth = 0.0f;
    float normal_impulse = 0.0f;
    float tangent_impulse = 0.0f;
};

struct Contact {
    uint32_t a = 0;
    uint32_t b = 0;
    Vec2 n{};
    ContactPoint pt[2]{};
    uint8_t count = 0;
    float mu = 0.6f;
    float restitution_bias = 0.0f;
};

class World {
public:
    static constexpr float kDt = 1.0f / 240.0f;

    World();

    BodyId add_circle(Vec2 pos, float radius, float density = 1000.0f);
    BodyId add_box(Vec2 pos, Vec2 half_extents, float angle = 0.0f, float density = 1000.0f);
    BodyId add_static_box(Vec2 pos, Vec2 half_extents, float angle = 0.0f);

    void remove(BodyId id);
    void clear();

    [[nodiscard]] bool alive(BodyId id) const;
    [[nodiscard]] Body& get(BodyId id);
    [[nodiscard]] const Body& get(BodyId id) const;

    [[nodiscard]] uint32_t body_count() const { return count_; }
    [[nodiscard]] uint32_t contact_count() const { return contact_count_; }
    [[nodiscard]] uint64_t state_hash() const;
    [[nodiscard]] BodyId pick(Vec2 point) const;
    [[nodiscard]] const std::vector<Contact>& contacts() const { return contacts_; }

    void step(const InputFrame& in);

    template <class F>
    void for_each(F&& f) const {
        for (uint32_t i = 0; i < bodies_.size(); ++i) {
            if (!alive_[i]) continue;
            f(BodyId{i, bodies_[i].generation}, bodies_[i]);
        }
    }

private:
    BodyId create_body();
    void broadphase();
    void solve();
    void position_project();
    void integrate();

    std::vector<Body> bodies_;
    std::vector<uint8_t> alive_;
    std::vector<uint32_t> free_;
    std::vector<uint32_t> order_;
    std::vector<AABB> aabbs_;
    std::vector<Contact> contacts_;
    std::vector<Contact> prev_contacts_;
    uint32_t count_ = 0;
    uint32_t contact_count_ = 0;
};

} // namespace sim
