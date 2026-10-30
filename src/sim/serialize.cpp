#include <cstring>
#include <limits>
#include <vector>

#include "world.h"

namespace sim {
namespace {

constexpr uint32_t kMagic = 0x43535050u;  // "PPSC"
constexpr uint32_t kVersion = 1u;

struct Writer {
    std::vector<uint8_t>& out;
    void u8(uint8_t v) { out.push_back(v); }
    void u32(uint32_t v) {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
        out.push_back(static_cast<uint8_t>(v >> 16));
        out.push_back(static_cast<uint8_t>(v >> 24));
    }
    void f32(float f) {
        uint32_t v = 0;
        std::memcpy(&v, &f, sizeof v);
        u32(v);
    }
    void vec(Vec2 v) { f32(v.x); f32(v.y); }
    void flag(bool b) { u8(b ? 1u : 0u); }
};

struct Reader {
    const uint8_t* data;
    size_t size;
    size_t offset = 0;
    bool ok = true;

    bool need(size_t n) {
        if (!ok || offset + n > size) {
            ok = false;
            return false;
        }
        return true;
    }
    uint8_t u8() {
        if (!need(1)) return 0;
        return data[offset++];
    }
    uint32_t u32() {
        if (!need(4)) return 0;
        const uint32_t v = static_cast<uint32_t>(data[offset]) |
                           (static_cast<uint32_t>(data[offset + 1]) << 8) |
                           (static_cast<uint32_t>(data[offset + 2]) << 16) |
                           (static_cast<uint32_t>(data[offset + 3]) << 24);
        offset += 4;
        return v;
    }
    float f32() {
        const uint32_t v = u32();
        float f = 0.0f;
        std::memcpy(&f, &v, sizeof f);
        return f;
    }
    Vec2 vec() { return {f32(), f32()}; }
    bool flag() { return u8() != 0; }
};

constexpr uint32_t kBodySlotsMax = 1u << 20;
constexpr uint32_t kJointSlotsMax = 1u << 20;

void write_body(Writer& w, const Body& b) {
    w.vec(b.pos);
    w.vec(b.vel);
    w.vec(b.force);
    w.f32(b.angle);
    w.f32(b.ang_vel);
    w.f32(b.torque);
    w.vec(b.half);
    w.f32(b.radius);
    w.f32(b.mass);
    w.f32(b.inv_mass);
    w.f32(b.inv_inertia);
    w.f32(b.friction);
    w.f32(b.restitution);
    w.u8(static_cast<uint8_t>(b.shape));
    w.u32(b.generation);
    w.flag(b.dynamic);
    w.flag(b.asleep);
    w.f32(b.sleep_timer);
    w.f32(b.health);
    w.f32(b.bleed_ml_s);
    w.f32(b.vascularity);
    w.u32(b.group);
    w.flag(b.vital);
    w.flag(b.tourniquet);
    w.u8(b.compartment_count);
    for (uint8_t i = 0; i < b.compartment_count; ++i) {
        const Compartment& c = b.compartments[i];
        w.vec(c.center);
        w.vec(c.half);
        w.u8(static_cast<uint8_t>(c.kind));
        w.f32(c.damage);
        w.flag(c.severed);
    }
}

bool read_body(Reader& r, Body& b) {
    b = Body{};
    b.pos = r.vec();
    b.vel = r.vec();
    b.force = r.vec();
    b.angle = r.f32();
    b.ang_vel = r.f32();
    b.torque = r.f32();
    b.half = r.vec();
    b.radius = r.f32();
    b.mass = r.f32();
    b.inv_mass = r.f32();
    b.inv_inertia = r.f32();
    b.friction = r.f32();
    b.restitution = r.f32();
    b.shape = static_cast<Shape>(r.u8());
    b.generation = r.u32();
    b.dynamic = r.flag();
    b.asleep = r.flag();
    b.sleep_timer = r.f32();
    b.health = r.f32();
    b.bleed_ml_s = r.f32();
    b.vascularity = r.f32();
    b.group = r.u32();
    b.vital = r.flag();
    b.tourniquet = r.flag();
    b.compartment_count = r.u8();
    if (b.compartment_count > Body::kMaxCompartments) return false;
    for (uint8_t i = 0; i < b.compartment_count; ++i) {
        Compartment& c = b.compartments[i];
        c.center = r.vec();
        c.half = r.vec();
        c.kind = static_cast<TissueKind>(r.u8());
        c.damage = r.f32();
        c.severed = r.flag();
    }
    return r.ok;
}

} // namespace

bool World::save_scene(std::vector<uint8_t>& out) const {
    out.clear();
    Writer w{out};
    w.u32(kMagic);
    w.u32(kVersion);
    w.u32(static_cast<uint32_t>(tick_));
    w.vec(gravity_);

    w.u32(static_cast<uint32_t>(bodies_.size()));
    for (size_t i = 0; i < bodies_.size(); ++i) {
        w.flag(alive_[i] != 0);
        write_body(w, bodies_[i]);
    }

    w.u32(static_cast<uint32_t>(joints_.size()));
    for (size_t i = 0; i < joints_.size(); ++i) {
        const Joint& j = joints_[i];
        w.flag(joint_alive_[i] != 0);
        w.u8(static_cast<uint8_t>(j.type));
        w.u32(j.a);
        w.u32(j.b);
        w.vec(j.local_a);
        w.vec(j.local_b);
        w.f32(j.rest_length);
        w.f32(j.ref_angle);
        w.f32(j.break_force);
        w.f32(j.break_torque);
        w.f32(j.yield_torque);
        w.flag(j.has_limit);
        w.f32(j.limit_min);
        w.f32(j.limit_max);
        w.f32(j.force_ema);
        w.f32(j.torque_ema);
        w.u32(j.generation);
    }

    w.u32(static_cast<uint32_t>(groups_.size()));
    for (const Vitals& v : groups_) {
        w.f32(v.blood_ml);
        w.f32(v.bleed_ml_s);
        w.flag(v.unconscious);
        w.flag(v.dead);
    }

    // Warm-start cache: pair keys and accumulated impulses.
    w.u32(static_cast<uint32_t>(prev_contacts_.size()));
    for (const Contact& c : prev_contacts_) {
        w.u32(c.a);
        w.u32(c.b);
        w.u8(c.count);
        w.f32(c.restitution_bias);
        w.f32(c.mu);
        for (uint8_t k = 0; k < c.count; ++k) {
            w.f32(c.pt[k].normal_impulse);
            w.f32(c.pt[k].tangent_impulse);
        }
    }

    w.u32(static_cast<uint32_t>(projectiles_.size()));
    for (size_t i = 0; i < projectiles_.size(); ++i) {
        const Projectile& p = projectiles_[i];
        w.flag(projectile_alive_[i] != 0);
        w.vec(p.pos);
        w.vec(p.prev);
        w.vec(p.vel);
        w.f32(p.mass);
        w.f32(p.radius);
        w.f32(p.drag_k);
        w.u32(p.generation);
        w.flag(p.alive);
    }
    return true;
}

bool World::load_scene(const void* data, std::size_t size) {
    if (data == nullptr || size < 16) return false;
    Reader r{static_cast<const uint8_t*>(data), size};
    if (r.u32() != kMagic) return false;
    if (r.u32() != kVersion) return false;
    const uint32_t tick = r.u32();
    const Vec2 gravity = r.vec();
    if (!r.ok) return false;

    const uint32_t body_slots = r.u32();
    if (body_slots > kBodySlotsMax) return false;

    std::vector<Body> bodies;
    std::vector<uint8_t> alive;
    std::vector<uint32_t> free_list;
    bodies.reserve(body_slots);
    alive.reserve(body_slots);
    uint32_t count = 0;
    for (uint32_t i = 0; i < body_slots; ++i) {
        const bool is_alive = r.flag();
        Body b;
        if (!read_body(r, b)) return false;
        bodies.push_back(b);
        alive.push_back(is_alive ? 1 : 0);
        if (is_alive) {
            ++count;
        } else {
            free_list.push_back(i);
        }
    }

    const uint32_t joint_slots = r.u32();
    if (joint_slots > kJointSlotsMax) return false;
    std::vector<Joint> joints;
    std::vector<uint8_t> joint_alive;
    std::vector<uint32_t> joint_free;
    uint32_t joint_count = 0;
    joints.reserve(joint_slots);
    joint_alive.reserve(joint_slots);
    for (uint32_t i = 0; i < joint_slots; ++i) {
        const bool is_alive = r.flag();
        Joint j;
        j.alive = is_alive;
        j.type = static_cast<JointType>(r.u8());
        j.a = r.u32();
        j.b = r.u32();
        j.local_a = r.vec();
        j.local_b = r.vec();
        j.rest_length = r.f32();
        j.ref_angle = r.f32();
        j.break_force = r.f32();
        j.break_torque = r.f32();
        j.yield_torque = r.f32();
        j.has_limit = r.flag();
        j.limit_min = r.f32();
        j.limit_max = r.f32();
        j.force_ema = r.f32();
        j.torque_ema = r.f32();
        j.generation = r.u32();
        if (!r.ok) return false;
        if (j.a >= bodies.size() || j.b >= bodies.size()) return false;
        joints.push_back(j);
        joint_alive.push_back(is_alive ? 1 : 0);
        if (is_alive) {
            ++joint_count;
        } else {
            joint_free.push_back(i);
        }
    }

    const uint32_t group_count = r.u32();
    if (group_count > (1u << 16)) return false;
    std::vector<Vitals> groups;
    groups.reserve(group_count);
    for (uint32_t i = 0; i < group_count; ++i) {
        Vitals v;
        v.blood_ml = r.f32();
        v.bleed_ml_s = r.f32();
        v.unconscious = r.flag();
        v.dead = r.flag();
        groups.push_back(v);
    }

    const uint32_t contact_slots = r.u32();
    if (contact_slots > (1u << 22)) return false;
    std::vector<Contact> prev_contacts;
    prev_contacts.reserve(contact_slots);
    for (uint32_t i = 0; i < contact_slots; ++i) {
        Contact c;
        c.a = r.u32();
        c.b = r.u32();
        c.count = r.u8();
        if (c.count > 2) return false;
        c.restitution_bias = r.f32();
        c.mu = r.f32();
        for (uint8_t k = 0; k < c.count; ++k) {
            c.pt[k].normal_impulse = r.f32();
            c.pt[k].tangent_impulse = r.f32();
        }
        if (!r.ok) return false;
        if (c.a >= bodies.size() || c.b >= bodies.size()) return false;
        prev_contacts.push_back(c);
    }

    const uint32_t projectile_slots = r.u32();
    if (projectile_slots > (1u << 20)) return false;
    std::vector<Projectile> projectiles;
    std::vector<uint8_t> projectile_alive;
    std::vector<uint32_t> projectile_free;
    uint32_t projectile_count = 0;
    projectiles.reserve(projectile_slots);
    projectile_alive.reserve(projectile_slots);
    for (uint32_t i = 0; i < projectile_slots; ++i) {
        const bool is_alive = r.flag();
        Projectile p;
        p.pos = r.vec();
        p.prev = r.vec();
        p.vel = r.vec();
        p.mass = r.f32();
        p.radius = r.f32();
        p.drag_k = r.f32();
        p.generation = r.u32();
        p.alive = r.flag();
        if (!r.ok) return false;
        projectiles.push_back(p);
        projectile_alive.push_back(is_alive ? 1 : 0);
        if (is_alive) {
            ++projectile_count;
        } else {
            projectile_free.push_back(i);
        }
    }
    if (!r.ok) return false;

    bodies_ = std::move(bodies);
    alive_ = std::move(alive);
    free_ = std::move(free_list);
    joints_ = std::move(joints);
    joint_alive_ = std::move(joint_alive);
    joint_free_ = std::move(joint_free);
    groups_ = std::move(groups);
    projectiles_ = std::move(projectiles);
    projectile_alive_ = std::move(projectile_alive);
    projectile_free_ = std::move(projectile_free);
    count_ = count;
    joint_count_ = joint_count;
    projectile_count_ = projectile_count;
    contact_count_ = 0;
    contacts_.clear();
    prev_contacts_ = std::move(prev_contacts);
    wounds_.clear();
    tick_ = tick;
    gravity_ = gravity;
    return true;
}

} // namespace sim
