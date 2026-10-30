#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "handle.h"
#include "material.h"
#include "math.h"

namespace sim {

enum class Shape : uint8_t { Circle = 0, Box = 1 };

enum class TissueKind : uint8_t {
    Skin = 0, Fat, Muscle, Bone, Artery, Vein, Nerve,
    Brain, Heart, Lung, Liver, Gut
};

struct Compartment {
    Vec2 center{};   // body-local
    Vec2 half{};     // body-local half extents
    TissueKind kind = TissueKind::Muscle;
    float damage = 0.0f;
    bool severed = false;
};

struct ProjectileTag {};
using ProjectileId = Handle<ProjectileTag>;

struct Projectile {
    Vec2 pos{};
    Vec2 prev{};
    Vec2 vel{};
    float mass = 0.008f;
    float radius = 0.0045f;
    float drag_k = 0.0f;   // 0.5 * rho * Cd * A / m
    uint32_t generation = 0;
    bool alive = true;
};

struct Vitals {
    float blood_ml = 5200.0f;
    float bleed_ml_s = 0.0f;
    bool unconscious = false;
    bool dead = false;
};

struct WoundEvent {
    Vec2 pos{};
    Vec2 dir{};
    float energy = 0.0f;
    float damage = 0.0f;
    uint32_t body = 0;
};

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
    MaterialId material = MaterialId::Default;
    uint32_t generation = 0;
    bool dynamic = true;
    bool asleep = false;
    float sleep_timer = 0.0f;

    float health = 1.0f;
    float bleed_ml_s = 0.0f;
    float vascularity = 0.0f;   // ml/s at full damage
    uint32_t group = UINT32_MAX;
    bool vital = false;
    bool tourniquet = false;

    static constexpr int kMaxCompartments = 6;
    Compartment compartments[kMaxCompartments]{};
    uint8_t compartment_count = 0;
};

enum class JointType : uint8_t { Distance = 0, Revolute = 1, Weld = 2 };

struct JointTag {};
using JointId = Handle<JointTag>;

struct Joint {
    JointType type = JointType::Revolute;
    uint32_t a = 0;
    uint32_t b = 0;
    Vec2 local_a{};
    Vec2 local_b{};
    float rest_length = 0.0f;
    float ref_angle = 0.0f;
    float break_force = 0.0f;   // N; 0 = unbreakable
    float break_torque = 0.0f;  // N·m; 0 = auto from materials
    float yield_torque = 0.0f;  // N·m; plastic hinge threshold
    bool has_limit = false;
    float limit_min = 0.0f;
    float limit_max = 0.0f;
    float accumulated = 0.0f;
    float accumulated_angular = 0.0f;
    float force_ema = 0.0f;
    float torque_ema = 0.0f;
    uint32_t generation = 0;
    bool alive = true;
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
    void set_gravity(Vec2 g) { gravity_ = g; }
    [[nodiscard]] Vec2 gravity() const { return gravity_; }
    void set_material(BodyId id, MaterialId m);
    void apply_tourniquet(BodyId id);
    void add_compartment(BodyId id, Vec2 local_center, Vec2 local_half, TissueKind kind);
    // Applies a blade stroke; returns true if it severed a joint (dismemberment).
    bool apply_blade(BodyId id, Vec2 world_point, Vec2 dir, float energy_j);

    [[nodiscard]] bool alive(BodyId id) const;
    [[nodiscard]] BodyId id_at(uint32_t index) const {
        return (index < bodies_.size() && alive_[index] != 0)
                   ? BodyId{index, bodies_[index].generation}
                   : BodyId{};
    }
    [[nodiscard]] Body& get(BodyId id);
    [[nodiscard]] const Body& get(BodyId id) const;

    [[nodiscard]] uint32_t body_count() const { return count_; }
    [[nodiscard]] uint32_t contact_count() const { return contact_count_; }
    [[nodiscard]] uint64_t state_hash() const;
    [[nodiscard]] BodyId pick(Vec2 point) const;
    [[nodiscard]] const std::vector<Contact>& contacts() const { return contacts_; }

    JointId add_distance_joint(BodyId a, BodyId b, Vec2 anchor_a, Vec2 anchor_b,
                               float rest_length, float break_force = 0.0f);
    JointId add_revolute_joint(BodyId a, BodyId b, Vec2 world_anchor,
                               bool has_limit = false, float limit_min = 0.0f,
                               float limit_max = 0.0f);
    JointId add_weld_joint(BodyId a, BodyId b, Vec2 world_anchor,
                           float break_force = 0.0f, float break_torque = 0.0f);
    void remove_joint(JointId id);
    [[nodiscard]] bool joint_alive(JointId id) const;
    [[nodiscard]] Joint& joint(JointId id);
    [[nodiscard]] const Joint& joint(JointId id) const;
    [[nodiscard]] uint32_t joint_count() const { return joint_count_; }

    template <class F>
    void for_each_projectile(F&& f) const {
        for (uint32_t i = 0; i < projectiles_.size(); ++i) {
            if (projectile_alive_[i]) f(ProjectileId{i, projectiles_[i].generation},
                                        projectiles_[i]);
        }
    }

    template <class F>
    void for_each_joint(F&& f) const {
        for (uint32_t i = 0; i < joints_.size(); ++i) {
            if (joint_alive_[i]) f(JointId{i, joints_[i].generation}, joints_[i]);
        }
    }

    void step(const InputFrame& in);

    ProjectileId spawn_projectile(Vec2 pos, Vec2 vel, float mass, float radius,
                                  float drag_k);
    void remove_projectile(ProjectileId id);
    [[nodiscard]] bool projectile_alive(ProjectileId id) const;
    [[nodiscard]] Projectile& projectile(ProjectileId id);
    [[nodiscard]] const Projectile& projectile(ProjectileId id) const;
    [[nodiscard]] const std::vector<WoundEvent>& wounds() const { return wounds_; }

    uint32_t create_group(const Vitals& v);

    // Versioned binary scene format. Projectiles are transient but serialized
    // so a round-trip preserves the deterministic state hash.
    bool save_scene(std::vector<uint8_t>& out) const;
    bool load_scene(const void* data, std::size_t size);
    [[nodiscard]] Vitals& vitals(uint32_t group);
    [[nodiscard]] const Vitals& vitals(uint32_t group) const;
    [[nodiscard]] uint32_t group_count() const {
        return static_cast<uint32_t>(groups_.size());
    }

    template <class F>
    void for_each(F&& f) const {
        for (uint32_t i = 0; i < bodies_.size(); ++i) {
            if (!alive_[i]) continue;
            f(BodyId{i, bodies_[i].generation}, bodies_[i]);
        }
    }

private:
    BodyId create_body();
    JointId add_joint(const Joint& j);
    void broadphase();
    void solve();
    void step_projectiles();
    void update_vitals();
    void solve_joints();
    void project_joints();
    void position_project();
    void integrate();

    std::vector<Body> bodies_;
    std::vector<uint8_t> alive_;
    std::vector<uint32_t> free_;
    std::vector<uint32_t> order_;
    std::vector<AABB> aabbs_;
    std::vector<Contact> contacts_;
    std::vector<Contact> prev_contacts_;
    std::vector<Joint> joints_;
    std::vector<uint8_t> joint_alive_;
    std::vector<uint32_t> joint_free_;
    uint32_t joint_count_ = 0;
    Vec2 gravity_{0.0f, -9.81f};

    std::vector<Projectile> projectiles_;
    std::vector<uint8_t> projectile_alive_;
    std::vector<uint32_t> projectile_free_;
    std::vector<Vitals> groups_;
    std::vector<WoundEvent> wounds_;
    uint32_t projectile_count_ = 0;
    uint64_t tick_ = 0;
    uint32_t count_ = 0;
    uint32_t contact_count_ = 0;
};

} // namespace sim
