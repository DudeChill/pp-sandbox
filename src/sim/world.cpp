#include "world.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "hash.h"

namespace sim {
namespace {

constexpr float kSlop = 0.005f;
constexpr int kVelIters = 10;

constexpr float kSleepLin = 0.03f;
constexpr float kSleepAng = 0.15f;
constexpr float kSleepTime = 0.5f;
constexpr float kJointBeta = 0.2f;
constexpr float kJointMaxBias = 2.0f;
constexpr float kJointSlop = 0.002f;
constexpr float kJointCorrection = 0.8f;
constexpr float kLinDamp = 0.0f;
constexpr float kAngDamp = 0.0f;
constexpr float kGrabK = 400.0f;
constexpr float kGrabC = 40.0f;
constexpr float kMaxGrabForce = 20000.0f;
constexpr float kPi = 3.14159265358979323846f;

struct Axes {
    Vec2 ax[2];
};

inline Axes axes_of(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Axes r;
    r.ax[0] = {c, s};
    r.ax[1] = {-s, c};
    return r;
}

inline float half_of(const Body& b, int i) { return i == 0 ? b.half.x : b.half.y; }

inline float extent_of(const Body& b, const Axes& A, Vec2 axis) {
    if (b.shape == Shape::Circle) return b.radius;
    return b.half.x * std::fabs(dot(axis, A.ax[0])) +
           b.half.y * std::fabs(dot(axis, A.ax[1]));
}

inline AABB compute_aabb(const Body& b, const Axes& A) {
    if (b.shape == Shape::Circle) {
        return {b.pos.x - b.radius, b.pos.y - b.radius,
                b.pos.x + b.radius, b.pos.y + b.radius};
    }
    const float ex = b.half.x * std::fabs(A.ax[0].x) + b.half.y * std::fabs(A.ax[1].x);
    const float ey = b.half.x * std::fabs(A.ax[0].y) + b.half.y * std::fabs(A.ax[1].y);
    return {b.pos.x - ex, b.pos.y - ey, b.pos.x + ex, b.pos.y + ey};
}

inline bool overlaps(const AABB& a, const AABB& b) {
    return a.min_x <= b.max_x && b.min_x <= a.max_x &&
           a.min_y <= b.max_y && b.min_y <= a.max_y;
}

inline void apply_impulse(Body& b, Vec2 r, Vec2 impulse) {
    b.vel += impulse * b.inv_mass;
    b.ang_vel += cross(r, impulse) * b.inv_inertia;
}

inline float inv_mass_of(const Body& b) { return b.asleep ? 0.0f : b.inv_mass; }
inline float inv_inertia_of(const Body& b) { return b.asleep ? 0.0f : b.inv_inertia; }

bool clip_segment(Vec2& p0, Vec2& p1, Vec2 n, float offset) {
    const float d0 = dot(p0, n) - offset;
    const float d1 = dot(p1, n) - offset;
    if (d0 > 0.0f && d1 > 0.0f) return false;
    if (d0 > 0.0f) {
        const float t = d0 / (d0 - d1);
        p0 = p0 + (p1 - p0) * t;
    } else if (d1 > 0.0f) {
        const float t = d0 / (d0 - d1);
        p1 = p0 + (p1 - p0) * t;
    }
    return true;
}

bool collide_circle_circle(const Body& a, const Body& b, Contact& c) {
    const Vec2 d = b.pos - a.pos;
    const float dist = length(d);
    const float rsum = a.radius + b.radius;
    if (dist >= rsum) return false;
    const Vec2 n = dist > 1e-6f ? d * (1.0f / dist) : Vec2{1.0f, 0.0f};
    c.n = n;
    c.pt[0].p = a.pos + n * (a.radius - 0.5f * (rsum - dist));
    c.pt[0].depth = rsum - dist;
    c.count = 1;
    return true;
}

// Normal points from the circle to the box.
bool collide_circle_box(const Body& circle, const Body& box, Contact& c) {
    const Vec2 local = inv_rotate(circle.pos - box.pos, -box.angle);
    const Vec2 closest{clampf(local.x, -box.half.x, box.half.x),
                       clampf(local.y, -box.half.y, box.half.y)};
    const Vec2 delta = local - closest;
    const float dist_sq = length_sq(delta);
    Vec2 n_local;
    float depth = 0.0f;
    bool inside = false;

    if (dist_sq > 1e-12f) {
        const float dist = std::sqrt(dist_sq);
        if (dist >= circle.radius) return false;
        n_local = delta * (-1.0f / dist);
        depth = circle.radius - dist;
    } else {
        inside = true;
        const float dx = box.half.x - std::fabs(local.x);
        const float dy = box.half.y - std::fabs(local.y);
        if (dx < dy) {
            n_local = {local.x >= 0.0f ? -1.0f : 1.0f, 0.0f};
            depth = circle.radius + dx;
        } else {
            n_local = {0.0f, local.y >= 0.0f ? -1.0f : 1.0f};
            depth = circle.radius + dy;
        }
    }

    (void)inside;
    const Vec2 n = rotate(n_local, box.angle);
    c.n = n;
    c.pt[0].p = circle.pos + n * (circle.radius - 0.5f * depth);
    c.pt[0].depth = depth;
    c.count = 1;
    return true;
}

bool collide_box_box(const Body& a, const Body& b, Contact& out) {
    const Axes A = axes_of(a.angle);
    const Axes B = axes_of(b.angle);
    const Vec2 d = b.pos - a.pos;

    float best = -std::numeric_limits<float>::infinity();
    Vec2 axis{1.0f, 0.0f};
    bool owner_a = true;
    int idx = 0;

    for (int i = 0; i < 2; ++i) {
        const float s = std::fabs(dot(d, A.ax[i])) -
                        (extent_of(a, A, A.ax[i]) + extent_of(b, B, A.ax[i]));
        if (s > best) {
            best = s;
            axis = A.ax[i];
            owner_a = true;
            idx = i;
        }
    }
    for (int i = 0; i < 2; ++i) {
        const float s = std::fabs(dot(d, B.ax[i])) -
                        (extent_of(a, A, B.ax[i]) + extent_of(b, B, B.ax[i]));
        if (s > best) {
            best = s;
            axis = B.ax[i];
            owner_a = false;
            idx = i;
        }
    }
    if (best > 0.0f) return false;

    if (dot(d, axis) < 0.0f) axis = -axis;

    const Body& ref = owner_a ? a : b;
    const Body& inc = owner_a ? b : a;
    const Axes& R = owner_a ? A : B;
    const Axes& I = owner_a ? B : A;
    const Vec2 n_ref_inc = owner_a ? axis : -axis;

    const float sign = dot(n_ref_inc, R.ax[idx]) >= 0.0f ? 1.0f : -1.0f;
    const Vec2 ref_center = ref.pos + R.ax[idx] * (sign * half_of(ref, idx));
    const float ref_half = half_of(ref, 1 - idx);
    const Vec2 t = perp(n_ref_inc);

    const int j = std::fabs(dot(I.ax[0], n_ref_inc)) > std::fabs(dot(I.ax[1], n_ref_inc))
                      ? 0 : 1;
    const float sign2 = dot(I.ax[j], n_ref_inc) > 0.0f ? -1.0f : 1.0f;
    const Vec2 inc_center = inc.pos + I.ax[j] * (sign2 * half_of(inc, j));
    const float inc_half = half_of(inc, 1 - j);
    const Vec2 it = I.ax[1 - j];

    Vec2 p0 = inc_center - it * inc_half;
    Vec2 p1 = inc_center + it * inc_half;

    if (!clip_segment(p0, p1, t, dot(ref_center, t) + ref_half)) return false;
    if (!clip_segment(p0, p1, -t, dot(ref_center, -t) + ref_half)) return false;

    const Vec2 n = axis; // always points a -> b
    out.n = n;
    out.count = 0;
    const Vec2 points[2] = {p0, p1};
    for (const Vec2 p : points) {
        const float sep = dot(p - ref_center, n_ref_inc);
        if (sep <= kSlop) {
            out.pt[out.count].p = p;
            out.pt[out.count].depth = -sep;
            ++out.count;
            if (out.count == 2) break;
        }
    }
    return out.count > 0;
}

bool collide(const Body& a, const Body& b, Contact& c) {
    if (a.shape == Shape::Circle && b.shape == Shape::Circle) {
        return collide_circle_circle(a, b, c);
    }
    if (a.shape == Shape::Circle && b.shape == Shape::Box) {
        return collide_circle_box(a, b, c);
    }
    if (a.shape == Shape::Box && b.shape == Shape::Circle) {
        Contact tmp;
        if (!collide_circle_box(b, a, tmp)) return false;
        c = tmp;
        c.n = -c.n;
        return true;
    }
    return collide_box_box(a, b, c);
}

float rect_overlap_length(Vec2 origin, Vec2 delta, Vec2 center, Vec2 half) {
    float t0 = 0.0f;
    float t1 = 1.0f;
    const float p[4] = {-delta.x, delta.x, -delta.y, delta.y};
    const float q[4] = {origin.x - (center.x - half.x), (center.x + half.x) - origin.x,
                        origin.y - (center.y - half.y), (center.y + half.y) - origin.y};
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(p[i]) < 1e-9f) {
            if (q[i] < 0.0f) return 0.0f;
            continue;
        }
        const float r = q[i] / p[i];
        if (p[i] < 0.0f) {
            if (r > t1) return 0.0f;
            if (r > t0) t0 = r;
        } else {
            if (r < t0) return 0.0f;
            if (r < t1) t1 = r;
        }
    }
    return (t1 - t0) * length(delta);
}

float tissue_weight(TissueKind kind) {
    switch (kind) {
        case TissueKind::Brain: return 3.0f;
        case TissueKind::Heart: return 2.5f;
        case TissueKind::Liver: return 1.5f;
        case TissueKind::Lung: return 1.2f;
        case TissueKind::Gut: return 0.8f;
        case TissueKind::Artery: return 1.5f;
        case TissueKind::Vein: return 0.6f;
        case TissueKind::Nerve: return 1.0f;
        case TissueKind::Bone: return 0.8f;
        case TissueKind::Muscle: return 0.4f;
        case TissueKind::Skin: return 0.2f;
        case TissueKind::Fat: return 0.1f;
    }
    return 0.2f;
}

float tissue_bleed_rate(TissueKind kind) {
    switch (kind) {
        case TissueKind::Artery: return 14.0f;
        case TissueKind::Vein: return 4.0f;
        case TissueKind::Brain: return 0.0f;
        case TissueKind::Heart: return 2.0f;
        case TissueKind::Lung: return 1.5f;
        case TissueKind::Liver: return 2.0f;
        case TissueKind::Gut: return 1.0f;
        case TissueKind::Muscle: return 0.5f;
        case TissueKind::Skin: return 0.2f;
        case TissueKind::Bone: return 0.05f;
        case TissueKind::Fat: return 0.05f;
        case TissueKind::Nerve: return 0.05f;
    }
    return 0.0f;
}

struct WoundResult {
    float worst = 0.0f;
    float bleed = 0.0f;
};

WoundResult apply_local_wound(Body& body, Vec2 entry, Vec2 exit, float energy_factor) {
    WoundResult result;
    const Vec2 segment = exit - entry;
    for (uint8_t ci = 0; ci < body.compartment_count; ++ci) {
        Compartment& comp = body.compartments[ci];
        const float inside = rect_overlap_length(entry, segment, comp.center, comp.half);
        if (inside <= 0.0f) continue;
        const float extent = 2.0f * std::max(comp.half.x, comp.half.y);
        const float dmg = clampf(inside / std::max(extent, 1e-4f) * energy_factor * 1.2f,
                                 0.0f, 1.0f);
        comp.damage = clampf(comp.damage + dmg * 0.7f, 0.0f, 1.0f);
        if (comp.kind == TissueKind::Artery && dmg > 0.2f) comp.severed = true;
        result.worst = std::max(result.worst, comp.damage * tissue_weight(comp.kind));
        result.bleed += comp.severed ? tissue_bleed_rate(comp.kind)
                                     : comp.damage * tissue_bleed_rate(comp.kind);
    }
    result.worst = clampf(result.worst, 0.0f, 1.0f);
    return result;
}

bool ray_circle_hit(Vec2 origin, Vec2 delta, Vec2 center, float radius,
                    float& t_in, float& chord) {
    const Vec2 m = origin - center;
    const float a = dot(delta, delta);
    if (a < 1e-12f) return false;
    const float b = 2.0f * dot(m, delta);
    const float c = dot(m, m) - radius * radius;
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    const float root = std::sqrt(disc);
    float t0 = (-b - root) / (2.0f * a);
    float t1 = (-b + root) / (2.0f * a);
    if (t1 < 0.0f || t0 > 1.0f) return false;
    t0 = std::max(t0, 0.0f);
    t1 = std::min(t1, 1.0f);
    if (t1 <= t0) return false;
    t_in = t0;
    chord = (t1 - t0) * std::sqrt(a);
    return true;
}

bool ray_box_hit(Vec2 origin, Vec2 delta, const Body& body, float& t_in,
                 float& chord) {
    const Vec2 o = inv_rotate(origin - body.pos, -body.angle);
    const Vec2 d = inv_rotate(delta, -body.angle);
    float tmin = 0.0f;
    float tmax = 1.0f;
    const float axes[2] = {d.x, d.y};
    const float starts[2] = {o.x, o.y};
    const float halves[2] = {body.half.x, body.half.y};
    for (int i = 0; i < 2; ++i) {
        if (std::fabs(axes[i]) < 1e-9f) {
            if (std::fabs(starts[i]) > halves[i]) return false;
            continue;
        }
        float t1 = (-halves[i] - starts[i]) / axes[i];
        float t2 = (halves[i] - starts[i]) / axes[i];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (tmax <= tmin) return false;
    t_in = tmin;
    chord = (tmax - tmin) * length(delta);
    return true;
}

} // namespace

World::World() {
    bodies_.reserve(8192);
    alive_.reserve(8192);
    free_.reserve(1024);
    order_.reserve(8192);
    aabbs_.reserve(8192);
    contacts_.reserve(1u << 16);
    prev_contacts_.reserve(1u << 16);
    joints_.reserve(4096);
    joint_alive_.reserve(4096);
    joint_free_.reserve(1024);
    projectiles_.reserve(1024);
    projectile_alive_.reserve(1024);
    projectile_free_.reserve(256);
    wounds_.reserve(256);
    wires_.reserve(4096);
}

BodyId World::create_body() {
    uint32_t idx = 0;
    if (!free_.empty()) {
        idx = free_.back();
        free_.pop_back();
    } else {
        idx = static_cast<uint32_t>(bodies_.size());
        bodies_.emplace_back();
        alive_.push_back(0);
    }
    Body& b = bodies_[idx];
    const uint32_t gen = b.generation;
    b = Body{};
    b.generation = gen;
    alive_[idx] = 1;
    ++count_;
    return BodyId{idx, gen};
}

BodyId World::add_circle(Vec2 pos, float radius, float density) {
    const BodyId id = create_body();
    Body& b = bodies_[id.index];
    b.pos = pos;
    b.shape = Shape::Circle;
    b.radius = radius;
    b.half = {radius, radius};
    b.dynamic = density > 0.0f;
    if (b.dynamic) {
        b.mass = density * kPi * radius * radius;
        b.inv_mass = 1.0f / b.mass;
        const float inertia = 0.5f * b.mass * radius * radius;
        b.inv_inertia = 1.0f / inertia;
    }
    return id;
}

BodyId World::add_box(Vec2 pos, Vec2 half_extents, float angle, float density) {
    const BodyId id = create_body();
    Body& b = bodies_[id.index];
    b.pos = pos;
    b.angle = angle;
    b.shape = Shape::Box;
    b.half = half_extents;
    b.dynamic = density > 0.0f;
    if (b.dynamic) {
        b.mass = density * 4.0f * half_extents.x * half_extents.y;
        b.inv_mass = 1.0f / b.mass;
        const float inertia = b.mass * (half_extents.x * half_extents.x +
                                        half_extents.y * half_extents.y) / 3.0f;
        b.inv_inertia = 1.0f / inertia;
    }
    return id;
}

BodyId World::add_static_box(Vec2 pos, Vec2 half_extents, float angle) {
    return add_box(pos, half_extents, angle, 0.0f);
}

bool World::alive(BodyId id) const {
    return id.index < bodies_.size() && alive_[id.index] != 0 &&
           bodies_[id.index].generation == id.generation;
}

Body& World::get(BodyId id) {
    assert(alive(id) && "invalid body handle");
    return bodies_[id.index];
}

const Body& World::get(BodyId id) const {
    assert(alive(id) && "invalid body handle");
    return bodies_[id.index];
}

void World::remove(BodyId id) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    alive_[id.index] = 0;
    ++b.generation;
    free_.push_back(id.index);
    --count_;

    size_t write = 0;
    for (size_t read = 0; read < wires_.size(); ++read) {
        if (wires_[read].from == id.index || wires_[read].to == id.index) continue;
        wires_[write++] = wires_[read];
    }
    wires_.resize(write);
}

void World::clear() {
    bodies_.clear();
    alive_.clear();
    free_.clear();
    contacts_.clear();
    prev_contacts_.clear();
    joints_.clear();
    joint_alive_.clear();
    joint_free_.clear();
    projectiles_.clear();
    projectile_alive_.clear();
    projectile_free_.clear();
    groups_.clear();
    wounds_.clear();
    wires_.clear();
    count_ = 0;
    contact_count_ = 0;
    joint_count_ = 0;
    projectile_count_ = 0;
    tick_ = 0;
}

BodyId World::pick(Vec2 point) const {
    BodyId best{};
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const Body& b = bodies_[i];
        bool inside = false;
        if (b.shape == Shape::Circle) {
            inside = length_sq(point - b.pos) <= b.radius * b.radius;
        } else {
            const Vec2 local = inv_rotate(point - b.pos, -b.angle);
            inside = std::fabs(local.x) <= b.half.x && std::fabs(local.y) <= b.half.y;
        }
        if (inside) best = BodyId{i, b.generation};
    }
    return best;
}

JointId World::add_distance_joint(BodyId a_id, BodyId b_id, Vec2 anchor_a, Vec2 anchor_b,
                                  float rest_length, float break_force) {
    assert(alive(a_id) && alive(b_id) && "invalid joint bodies");
    const Body& a = bodies_[a_id.index];
    const Body& b = bodies_[b_id.index];
    Joint j;
    j.type = JointType::Distance;
    j.a = a_id.index;
    j.b = b_id.index;
    j.local_a = inv_rotate(anchor_a - a.pos, -a.angle);
    j.local_b = inv_rotate(anchor_b - b.pos, -b.angle);
    j.rest_length = rest_length;
    j.break_force = break_force;
    return add_joint(j);
}

JointId World::add_revolute_joint(BodyId a_id, BodyId b_id, Vec2 world_anchor,
                                  bool has_limit, float limit_min, float limit_max) {
    assert(alive(a_id) && alive(b_id) && "invalid joint bodies");
    const Body& a = bodies_[a_id.index];
    const Body& b = bodies_[b_id.index];
    Joint j;
    j.type = JointType::Revolute;
    j.a = a_id.index;
    j.b = b_id.index;
    j.local_a = inv_rotate(world_anchor - a.pos, -a.angle);
    j.local_b = inv_rotate(world_anchor - b.pos, -b.angle);
    j.rest_length = 0.0f;
    j.ref_angle = b.angle - a.angle;
    j.has_limit = has_limit;
    j.limit_min = limit_min;
    j.limit_max = limit_max;
    return add_joint(j);
}

void World::apply_tourniquet(BodyId id) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    b.tourniquet = true;
    b.bleed_ml_s = 0.0f;
}

void World::ignite(BodyId id) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    const Material& m = material(b.material);
    if (m.ignition_celsius <= 0.0f || b.ignited) return;
    b.ignited = true;
    b.fuel_s = 12.0f;
    b.temperature_c = std::max(b.temperature_c, m.ignition_celsius);
}

bool World::is_burning(BodyId id) const {
    return alive(id) && bodies_[id.index].ignited;
}

int World::burning_count() const {
    int n = 0;
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (alive_[i] && bodies_[i].ignited) ++n;
    }
    return n;
}

void World::update_heat() {
    constexpr float kAmbient = 20.0f;
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& a = bodies_[i];
        if (!a.ignited) {
            a.temperature_c += (kAmbient - a.temperature_c) * 0.4f * kDt;
            if (a.temperature_c < 21.0f) a.temperature_c = kAmbient;
            continue;
        }
        a.fuel_s -= kDt;
        a.temperature_c = 600.0f;
        const Material& m = material(a.material);
        if (m.burn_damage_rate > 0.0f) {
            a.health = std::max(0.0f, a.health - m.burn_damage_rate * kDt);
            for (uint8_t ci = 0; ci < a.compartment_count; ++ci) {
                Compartment& c = a.compartments[ci];
                if (c.kind == TissueKind::Skin || c.kind == TissueKind::Muscle) {
                    c.damage = std::min(1.0f, c.damage + m.burn_damage_rate * kDt);
                }
            }
            if (a.vascularity > 0.0f && !a.tourniquet) {
                a.bleed_ml_s = std::max(a.bleed_ml_s, a.vascularity * 0.02f);
            }
            if (a.group < groups_.size() && a.vital && a.health <= 0.2f) {
                groups_[a.group].dead = true;
            }
        }
        if (a.fuel_s <= 0.0f) a.ignited = false;

        for (uint32_t j = 0; j < bodies_.size(); ++j) {
            if (j == i || !alive_[j]) continue;
            Body& b = bodies_[j];
            const Material& bm = material(b.material);
            if (b.ignited || bm.ignition_celsius <= 0.0f) continue;
            if (length(b.pos - a.pos) > 0.35f) continue;
            b.temperature_c += 250.0f * kDt;
            if (b.temperature_c >= bm.ignition_celsius) {
                b.ignited = true;
                b.fuel_s = 12.0f;
            }
        }
    }
}

void World::add_compartment(BodyId id, Vec2 local_center, Vec2 local_half,
                            TissueKind kind) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    if (b.compartment_count >= Body::kMaxCompartments) return;
    Compartment& c = b.compartments[b.compartment_count++];
    c.center = local_center;
    c.half = local_half;
    c.kind = kind;
    c.damage = 0.0f;
    c.severed = false;
}

void World::set_component(BodyId id, Component component, float thrust_force) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    b.component = component;
    b.thrust_force = thrust_force;
    b.signal = false;
    b.signal_in = false;
}

void World::set_button(BodyId id, bool on) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    if (b.component != Component::Button) return;
    b.signal = on;
}

void World::add_wire(BodyId from, BodyId to) {
    if (!alive(from) || !alive(to)) return;
    if (wires_.size() >= 4096) return;
    for (const Wire& w : wires_) {
        if (w.from == from.index && w.to == to.index) return;
    }
    wires_.push_back(Wire{from.index, to.index});
}

const std::vector<Wire>& World::wires() const { return wires_; }

void World::set_material(BodyId id, MaterialId m) {
    if (!alive(id)) return;
    Body& b = bodies_[id.index];
    b.material = m;
    const Material& mat = material(m);
    b.friction = mat.friction;
    b.restitution = mat.restitution;
    if (b.dynamic) {
        const float area = b.shape == Shape::Circle
                               ? 3.14159265358979323846f * b.radius * b.radius
                               : 4.0f * b.half.x * b.half.y;
        b.mass = mat.density * area;
        b.inv_mass = 1.0f / b.mass;
        const float inertia = b.shape == Shape::Circle
                                  ? 0.5f * b.mass * b.radius * b.radius
                                  : b.mass * (b.half.x * b.half.x + b.half.y * b.half.y) / 3.0f;
        b.inv_inertia = 1.0f / inertia;
    }
}

JointId World::add_weld_joint(BodyId a_id, BodyId b_id, Vec2 world_anchor,
                              float break_force, float break_torque) {
    assert(alive(a_id) && alive(b_id) && "invalid joint bodies");
    const Body& a = bodies_[a_id.index];
    const Body& b = bodies_[b_id.index];
    Joint j;
    j.type = JointType::Weld;
    j.a = a_id.index;
    j.b = b_id.index;
    j.local_a = inv_rotate(world_anchor - a.pos, -a.angle);
    j.local_b = inv_rotate(world_anchor - b.pos, -b.angle);
    j.rest_length = 0.0f;
    j.ref_angle = b.angle - a.angle;
    j.break_force = break_force;
    j.break_torque = break_torque;

    const Material& ma = material(a.material);
    const Material& mb = material(b.material);
    const float sy = std::min(ma.yield_stress, mb.yield_stress);
    const float su = std::min(ma.ultimate_stress, mb.ultimate_stress);
    const float interface_h = std::min(2.0f * a.half.y, 2.0f * b.half.y);
    constexpr float kDepth2D = 0.05f;
    const float section = kDepth2D * interface_h * interface_h / 6.0f;
    j.yield_torque = sy * section;
    if (j.break_torque <= 0.0f) j.break_torque = su * section;
    return add_joint(j);
}

JointId World::add_motor_joint(BodyId a_id, BodyId b_id, Vec2 world_anchor,
                               float motor_speed, float motor_torque) {
    const JointId id = add_revolute_joint(a_id, b_id, world_anchor);
    if (id.valid()) {
        Joint& j = joints_[id.index];
        j.motor_speed = motor_speed;
        j.motor_torque = motor_torque;
    }
    return id;
}

JointId World::add_joint(const Joint& j) {
    uint32_t idx = 0;
    if (!joint_free_.empty()) {
        idx = joint_free_.back();
        joint_free_.pop_back();
    } else {
        idx = static_cast<uint32_t>(joints_.size());
        joints_.emplace_back();
        joint_alive_.push_back(0);
    }
    Joint& s = joints_[idx];
    const uint32_t gen = s.generation;
    s = j;
    s.generation = gen;
    joint_alive_[idx] = 1;
    ++joint_count_;
    return JointId{idx, gen};
}

void World::remove_joint(JointId id) {
    if (!joint_alive(id)) return;
    Joint& j = joints_[id.index];
    joint_alive_[id.index] = 0;
    j.alive = false;
    ++j.generation;
    joint_free_.push_back(id.index);
    --joint_count_;
}

bool World::joint_alive(JointId id) const {
    return id.index < joints_.size() && joint_alive_[id.index] != 0 &&
           joints_[id.index].generation == id.generation;
}

Joint& World::joint(JointId id) {
    assert(joint_alive(id) && "invalid joint handle");
    return joints_[id.index];
}

const Joint& World::joint(JointId id) const {
    assert(joint_alive(id) && "invalid joint handle");
    return joints_[id.index];
}

ProjectileId World::spawn_projectile(Vec2 pos, Vec2 vel, float mass, float radius,
                                     float drag_k, float fragmentation_speed,
                                     uint8_t fragments) {
    uint32_t idx = 0;
    if (!projectile_free_.empty()) {
        idx = projectile_free_.back();
        projectile_free_.pop_back();
    } else {
        idx = static_cast<uint32_t>(projectiles_.size());
        projectiles_.emplace_back();
        projectile_alive_.push_back(0);
    }
    Projectile& p = projectiles_[idx];
    const uint32_t gen = p.generation;
    p = Projectile{};
    p.generation = gen;
    p.pos = pos;
    p.prev = pos;
    p.vel = vel;
    p.mass = mass;
    p.radius = radius;
    p.drag_k = drag_k;
    p.fragmentation_speed = fragmentation_speed;
    p.fragments = fragments;
    p.alive = true;
    projectile_alive_[idx] = 1;
    ++projectile_count_;
    return ProjectileId{idx, gen};
}

void World::remove_projectile(ProjectileId id) {
    if (!projectile_alive(id)) return;
    projectile_alive_[id.index] = 0;
    projectiles_[id.index].alive = false;
    ++projectiles_[id.index].generation;
    projectile_free_.push_back(id.index);
    --projectile_count_;
}

bool World::projectile_alive(ProjectileId id) const {
    return id.index < projectiles_.size() && projectile_alive_[id.index] != 0 &&
           projectiles_[id.index].generation == id.generation;
}

Projectile& World::projectile(ProjectileId id) {
    assert(projectile_alive(id) && "invalid projectile handle");
    return projectiles_[id.index];
}

const Projectile& World::projectile(ProjectileId id) const {
    assert(projectile_alive(id) && "invalid projectile handle");
    return projectiles_[id.index];
}

uint32_t World::create_group(const Vitals& v) {
    groups_.push_back(v);
    return static_cast<uint32_t>(groups_.size() - 1);
}

Vitals& World::vitals(uint32_t group) {
    assert(group < groups_.size() && "invalid vitals group");
    return groups_[group];
}

const Vitals& World::vitals(uint32_t group) const {
    assert(group < groups_.size() && "invalid vitals group");
    return groups_[group];
}

bool World::apply_blade(BodyId id, Vec2 world_point, Vec2 dir, float energy_j) {
    if (!alive(id)) return false;
    Body& body = bodies_[id.index];
    if (body.compartment_count == 0) return false;

    const Vec2 d = normalize(dir);
    const float half_len = 0.08f;
    const Vec2 entry_world = world_point - d * half_len;
    const Vec2 exit_world = world_point + d * half_len;
    const Vec2 entry = inv_rotate(entry_world - body.pos, -body.angle);
    const Vec2 exit = inv_rotate(exit_world - body.pos, -body.angle);
    const float energy_factor = clampf(energy_j / 300.0f, 0.0f, 1.0f);
    const WoundResult wr = apply_local_wound(body, entry, exit, energy_factor);
    body.health = std::max(0.0f, 1.0f - wr.worst);
    if (!body.tourniquet) {
        body.bleed_ml_s = std::max(body.bleed_ml_s, wr.bleed);
    }
    if (body.group < groups_.size() && body.vital && body.health <= 0.2f) {
        groups_[body.group].dead = true;
    }
    body.asleep = false;
    body.sleep_timer = 0.0f;
    if (wounds_.size() >= 256) wounds_.erase(wounds_.begin());
    wounds_.push_back(WoundEvent{world_point, d, energy_j, wr.worst, id.index});

    bool severed_joint = false;
    float structural = 0.0f;
    for (uint8_t ci = 0; ci < body.compartment_count; ++ci) {
        const Compartment& comp = body.compartments[ci];
        if (comp.kind == TissueKind::Bone || comp.kind == TissueKind::Muscle) {
            structural = std::max(structural, comp.damage);
        }
    }
    if (structural > 0.6f) {
        float best_dist = 0.45f;
        JointId best{};
        for (uint32_t ji = 0; ji < joints_.size(); ++ji) {
            if (!joint_alive_[ji]) continue;
            Joint& j = joints_[ji];
            if (j.a != id.index && j.b != id.index) continue;
            const Body& other = bodies_[j.a == id.index ? j.b : j.a];
            const Vec2 anchor = other.pos + rotate(j.a == id.index ? j.local_b : j.local_a,
                                                   other.angle);
            const float dist = length(anchor - world_point);
            if (dist < best_dist) {
                best_dist = dist;
                best = JointId{ji, j.generation};
            }
        }
        if (best.valid()) {
            remove_joint(best);
            severed_joint = true;
        }
    }
    return severed_joint;
}

bool World::fracture_body(uint32_t index, Vec2 dir, float energy_j) {
    if (index >= bodies_.size() || !alive_[index]) return false;
    const Body& body = bodies_[index];
    const Material& mat = material(body.material);
    if (energy_j < mat.fracture_energy) return false;

    const Vec2 center = body.pos;
    const float angle = body.angle;
    const Vec2 half = body.half;
    const Vec2 vel = body.vel;
    const float spin = body.ang_vel;
    const MaterialId material_id = body.material;
    const Shape shape = body.shape;

    if (shape != Shape::Box || half.x < 0.02f || half.y < 0.02f) return false;

    for (uint32_t ji = 0; ji < joints_.size(); ++ji) {
        if (!joint_alive_[ji]) continue;
        if (joints_[ji].a == index || joints_[ji].b == index) {
            remove_joint(JointId{ji, joints_[ji].generation});
        }
    }

    const BodyId removed = BodyId{index, bodies_[index].generation};
    remove(removed);

    const Vec2 offsets[4] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
    const Vec2 shard_half{half.x * 0.42f, half.y * 0.42f};
    for (const Vec2 off : offsets) {
        const Vec2 local{off.x * half.x * 0.5f, off.y * half.y * 0.5f};
        const Vec2 pos = center + rotate(local, angle);
        const BodyId shard = add_box(pos, shard_half, angle, material(material_id).density);
        (void)shard;
        const uint32_t sidx = shard.index;
        Body& sb = bodies_[sidx];
        sb.material = material_id;
        sb.vel = vel + rotate(Vec2{off.x * 1.4f, off.y * 1.4f}, angle) + dir * 1.2f;
        sb.ang_vel = spin + off.x * 2.0f;
        sb.friction = mat.friction;
        sb.restitution = mat.restitution;
        sb.asleep = false;
        sb.sleep_timer = 0.0f;
    }
    return true;
}

void World::explode(Vec2 center, float radius, float energy_j) {
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& body = bodies_[i];
        const Vec2 delta = body.pos - center;
        const float dist = length(delta);
        if (dist > radius) continue;
        const Vec2 dir = dist > 1e-4f ? delta * (1.0f / dist) : Vec2{0.0f, 1.0f};
        const float area = 4.0f * kPi * (dist + 0.3f) * (dist + 0.3f);
        const float impulse = energy_j / area;
        if (body.dynamic) {
            body.vel += dir * (impulse * body.inv_mass);
            body.asleep = false;
            body.sleep_timer = 0.0f;
        }

        if (body.compartment_count == 0 && body.vascularity <= 0.0f) continue;
        const float blast = energy_j / area;
        const float energy_factor = clampf(blast / 500.0f, 0.0f, 1.0f);
        const Vec2 entry_local =
            inv_rotate(body.pos - dir * 0.1f - body.pos, -body.angle);
        const Vec2 exit_local =
            inv_rotate(body.pos + dir * 0.1f - body.pos, -body.angle);
        const WoundResult wr = apply_local_wound(body, entry_local, exit_local,
                                                 energy_factor);
        body.health = std::max(0.0f, 1.0f - wr.worst);
        if (!body.tourniquet) {
            body.bleed_ml_s = std::max(body.bleed_ml_s, wr.bleed);
        }
        if (body.group < groups_.size() && body.vital && body.health <= 0.2f) {
            groups_[body.group].dead = true;
        }
        if (wounds_.size() >= 256) wounds_.erase(wounds_.begin());
        wounds_.push_back(WoundEvent{body.pos, dir, energy_j, wr.worst, i});
    }
}

void World::step_projectiles() {
    struct FragmentSpawn {
        Vec2 pos;
        Vec2 vel;
        float mass;
        float radius;
        float drag_k;
    };
    FragmentSpawn fragments[64];
    int fragment_count = 0;
    struct FractureRequest {
        uint32_t body;
        Vec2 dir;
        float energy;
    };
    FractureRequest fractures[16];
    int fracture_count = 0;
    for (uint32_t i = 0; i < projectiles_.size(); ++i) {
        if (!projectile_alive_[i]) continue;
        Projectile& p = projectiles_[i];
        if (!p.alive) continue;

        p.prev = p.pos;
        const float speed = length(p.vel);
        Vec2 accel = gravity_;
        if (p.drag_k > 0.0f) accel -= p.vel * (p.drag_k * speed);
        p.vel += accel * kDt;
        p.pos += p.vel * kDt;

        const Vec2 delta = p.pos - p.prev;
        if (length_sq(delta) < 1e-12f) continue;

        uint32_t best_body = UINT32_MAX;
        float best_t = 2.0f;
        float best_chord = 0.0f;
        for (uint32_t b = 0; b < bodies_.size(); ++b) {
            if (!alive_[b]) continue;
            const Body& body = bodies_[b];
            float t_in = 0.0f;
            float chord = 0.0f;
            bool hit = false;
            if (body.shape == Shape::Circle) {
                hit = ray_circle_hit(p.prev, delta, body.pos, body.radius + p.radius,
                                     t_in, chord);
            } else {
                Body expanded = body;
                expanded.half += Vec2{p.radius, p.radius};
                hit = ray_box_hit(p.prev, delta, expanded, t_in, chord);
            }
            if (hit && chord > 1e-3f && t_in < best_t) {
                best_t = t_in;
                best_body = b;
                best_chord = chord;
            }
        }
        if (best_body == UINT32_MAX) continue;

        Body& body = bodies_[best_body];
        const Vec2 hit_point = p.prev + delta * best_t;
        const Vec2 dir = normalize(delta);
        const float impact_speed = length(p.vel);
        const float energy = 0.5f * p.mass * impact_speed * impact_speed;
        const Material& mat = material(body.material);
        const float area = kPi * p.radius * p.radius;
        const float path = std::max(best_chord, 1e-4f);
        const float depth = energy / (mat.penetration_resistance * area);
        const float fraction = clampf(depth / path, 0.0f, 1.0f);
        const float damage = clampf(depth / 0.20f, 0.0f, 1.0f) *
                             clampf(path / 0.05f, 0.0f, 1.0f);

        float applied_damage = damage;
        if (body.compartment_count > 0) {
            const Vec2 entry_local = inv_rotate(hit_point - body.pos, -body.angle);
            const Vec2 exit_local =
                inv_rotate(hit_point + dir * path - body.pos, -body.angle);
            const float energy_factor = clampf(depth / 0.20f, 0.0f, 1.0f);

            const WoundResult wr =
                apply_local_wound(body, entry_local, exit_local, energy_factor);
            applied_damage = wr.worst;
            body.health = std::max(0.0f, 1.0f - applied_damage);
            if (!body.tourniquet) {
                body.bleed_ml_s = std::max(body.bleed_ml_s, wr.bleed);
            }
            if (body.group < groups_.size()) {
                Vitals& v = groups_[body.group];
                if (body.vital && body.health <= 0.2f) v.dead = true;
            }
        } else {
            body.health = std::max(0.0f, body.health - applied_damage);
            if (body.vascularity > 0.0f) {
                body.bleed_ml_s =
                    std::max(body.bleed_ml_s, applied_damage * body.vascularity);
            }
            if (body.group < groups_.size()) {
                Vitals& v = groups_[body.group];
                if (body.vital && body.health <= 0.2f) v.dead = true;
            }
        }

        const Vec2 impulse = dir * (p.mass * impact_speed * fraction);
        apply_impulse(body, hit_point - body.pos, impulse);
        if (body.dynamic) {
            body.asleep = false;
            body.sleep_timer = 0.0f;
        }

        if (wounds_.size() >= 256) wounds_.erase(wounds_.begin());
        wounds_.push_back(WoundEvent{hit_point, dir, energy, damage, best_body});

        if (p.fragmentation_speed > 0.0f && impact_speed > p.fragmentation_speed &&
            p.fragments > 0 && fragment_count + p.fragments < 64) {
            const float fragment_mass = p.mass / (static_cast<float>(p.fragments) + 1.0f);
            const float spread[4] = {0.22f, -0.22f, 0.38f, -0.38f};
            for (uint8_t f = 0; f < p.fragments && f < 4; ++f) {
                const float angle = spread[f];
                const Vec2 fdir{dir.x * std::cos(angle) - dir.y * std::sin(angle),
                                dir.x * std::sin(angle) + dir.y * std::cos(angle)};
                FragmentSpawn& fs = fragments[fragment_count++];
                fs.pos = hit_point;
                fs.vel = fdir * (impact_speed * 0.55f);
                fs.mass = fragment_mass;
                fs.radius = p.radius * 0.45f;
                fs.drag_k = p.drag_k;
            }
        }

        if (energy >= mat.fracture_energy && fracture_count < 16) {
            fractures[fracture_count++] = FractureRequest{best_body, dir, energy};
        }

        const float lost = mat.penetration_resistance * area * path;
        const float remaining = std::max(0.0f, energy - lost);
        if (depth > path) {
            p.vel = dir * std::sqrt(2.0f * remaining / p.mass);
            p.pos = hit_point + dir * (path + 2.0f * p.radius + 1e-3f);
            if (length(p.vel) < 30.0f) {
                p.alive = false;
                projectile_alive_[i] = 0;
                ++p.generation;
                projectile_free_.push_back(i);
                --projectile_count_;
            }
        } else {
            p.alive = false;
            projectile_alive_[i] = 0;
            ++p.generation;
            projectile_free_.push_back(i);
            --projectile_count_;
        }
    }

    for (int f = 0; f < fragment_count; ++f) {
        const FragmentSpawn& fs = fragments[f];
        spawn_projectile(fs.pos, fs.vel, fs.mass, fs.radius, fs.drag_k);
    }
    for (int f = 0; f < fracture_count; ++f) {
        fracture_body(fractures[f].body, fractures[f].dir, fractures[f].energy);
    }
}

void World::update_vitals() {
    for (Vitals& v : groups_) v.bleed_ml_s = 0.0f;
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const Body& b = bodies_[i];
        if (b.group < groups_.size()) groups_[b.group].bleed_ml_s += b.bleed_ml_s;
    }
    for (Vitals& v : groups_) {
        if (v.dead) continue;
        const float requested = v.bleed_ml_s * kDt;
        const float loss = std::min(requested, v.blood_ml);
        v.blood_ml -= loss;
        v.emitted_ml += loss;
        if (v.blood_ml < 900.0f) {
            v.dead = true;
        } else if (v.blood_ml < 2500.0f) {
            v.unconscious = true;
        }
    }
}


void World::step(const InputFrame& in) {
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (alive_[i]) bodies_[i].signal_in = false;
    }
    for (const Wire& wire : wires_) {
        if (wire.from >= bodies_.size() || wire.to >= bodies_.size()) continue;
        if (alive_[wire.from] && alive_[wire.to] && bodies_[wire.from].signal) {
            bodies_[wire.to].signal_in = true;
        }
    }
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& b = bodies_[i];
        if (b.component == Component::Thruster && b.signal_in && b.dynamic) {
            b.force += rotate(Vec2{b.thrust_force, 0.0f}, b.angle);
            b.asleep = false;
            b.sleep_timer = 0.0f;
        }
    }


    // 1-2. Input and force accumulation.
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& b = bodies_[i];
        if (!b.dynamic) continue;

        if (b.asleep && (b.force.x != 0.0f || b.force.y != 0.0f || b.torque != 0.0f)) {
            b.asleep = false;
            b.sleep_timer = 0.0f;
        }

        if (in.grab_active && in.grab == BodyId{i, b.generation}) {
            b.asleep = false;
            b.sleep_timer = 0.0f;
            Vec2 f = (in.grab_target - b.pos) * kGrabK - b.vel * kGrabC;
            const float mag = length(f);
            if (mag > kMaxGrabForce) f = f * (kMaxGrabForce / mag);
            b.force += f;
        }

        if (b.asleep) continue;
        b.vel += (b.force * b.inv_mass + gravity_) * kDt;
        b.ang_vel += b.torque * b.inv_inertia * kDt;
    }

    for (Joint& j : joints_) {
        if (!j.alive) continue;
        j.accumulated = 0.0f;
        j.accumulated_angular = 0.0f;
    }

    // Wake propagation through joints: an awake body drags its connected
    // island awake, otherwise half-asleep islands produce wrong constraint
    // forces when an external load is applied.
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        for (uint32_t i = 0; i < joints_.size(); ++i) {
            if (!joint_alive_[i]) continue;
            Joint& j = joints_[i];
            Body& a = bodies_[j.a];
            Body& b = bodies_[j.b];
            if (a.dynamic && !a.asleep && b.dynamic && b.asleep) {
                b.asleep = false;
                b.sleep_timer = 0.0f;
                changed = true;
            }
            if (b.dynamic && !b.asleep && a.dynamic && a.asleep) {
                a.asleep = false;
                a.sleep_timer = 0.0f;
                changed = true;
            }
        }
        if (!changed) break;
    }

    broadphase();
    solve();
    integrate();
    position_project();
    project_joints();
    step_projectiles();
    update_heat();
    if ((tick_ % 4) == 0) update_vitals();
    ++tick_;

    contact_count_ = static_cast<uint32_t>(contacts_.size());
    prev_contacts_ = contacts_;
}

void World::broadphase() {
    order_.clear();
    aabbs_.clear();
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const Body& b = bodies_[i];
        aabbs_.push_back(compute_aabb(b, axes_of(b.angle)));
        order_.push_back(i);
    }

    const auto* aabb_ptr = aabbs_.data();
    std::sort(order_.begin(), order_.end(), [aabb_ptr](uint32_t lhs, uint32_t rhs) {
        const float lx = aabb_ptr[lhs].min_x;
        const float rx = aabb_ptr[rhs].min_x;
        return lx < rx || (lx == rx && lhs < rhs);
    });

    contacts_.clear();
    for (size_t i = 0; i < order_.size(); ++i) {
        const uint32_t ia = order_[i];
        const AABB& ba = aabbs_[ia];
        for (size_t j = i + 1; j < order_.size(); ++j) {
            const uint32_t ib = order_[j];
            const AABB& bb = aabbs_[ib];
            if (bb.min_x > ba.max_x) break;
            if (!overlaps(ba, bb)) continue;

            const Body& a = bodies_[ia];
            const Body& b = bodies_[ib];
            if (!a.dynamic && !b.dynamic) continue;
            if (a.asleep && b.asleep) continue;

            Contact c;
            if (!collide(a, b, c)) continue;
            c.a = ia;
            c.b = ib;

            if (a.dynamic && b.dynamic) {
                if (a.asleep) {
                    Body& wa = bodies_[ia];
                    wa.asleep = false;
                    wa.sleep_timer = 0.0f;
                }
                if (b.asleep) {
                    Body& wb = bodies_[ib];
                    wb.asleep = false;
                    wb.sleep_timer = 0.0f;
                }
            }

            c.mu = std::sqrt(a.friction * b.friction);
            const float e = std::max(a.restitution, b.restitution);
            float bias = 0.0f;
            if (e > 0.0f) {
                const Vec2 ra = c.pt[0].p - a.pos;
                const Vec2 rb = c.pt[0].p - b.pos;
                const Vec2 vrel = (b.vel + cross(b.ang_vel, rb)) -
                                  (a.vel + cross(a.ang_vel, ra));
                const float vn = dot(vrel, c.n);
                if (vn < -1.0f) bias = -e * vn;
            }
            c.restitution_bias = bias;
            for (uint8_t k = 0; k < c.count; ++k) {
                ContactPoint& cp = c.pt[k];
                cp.local_a = inv_rotate(cp.p - a.pos, -a.angle);
                cp.local_b = inv_rotate(cp.p - b.pos, -b.angle);
            }
            contacts_.push_back(c);
        }
    }

    std::sort(contacts_.begin(), contacts_.end(), [](const Contact& l, const Contact& r) {
        return l.a < r.a || (l.a == r.a && l.b < r.b);
    });

    size_t pi = 0;
    for (Contact& c : contacts_) {
        while (pi < prev_contacts_.size() &&
               (prev_contacts_[pi].a < c.a ||
                (prev_contacts_[pi].a == c.a && prev_contacts_[pi].b < c.b))) {
            ++pi;
        }
        if (pi < prev_contacts_.size() && prev_contacts_[pi].a == c.a &&
            prev_contacts_[pi].b == c.b) {
            const Contact& p = prev_contacts_[pi];
            const uint8_t n = std::min(c.count, p.count);
            for (uint8_t k = 0; k < n; ++k) {
                c.pt[k].normal_impulse = p.pt[k].normal_impulse;
                c.pt[k].tangent_impulse = p.pt[k].tangent_impulse;
            }
        }
    }
}

void World::solve() {
    for (Contact& c : contacts_) {
        Body& a = bodies_[c.a];
        Body& b = bodies_[c.b];
        if (inv_mass_of(a) + inv_mass_of(b) == 0.0f) continue;
        for (uint8_t k = 0; k < c.count; ++k) {
            const ContactPoint& cp = c.pt[k];
            if (cp.normal_impulse == 0.0f && cp.tangent_impulse == 0.0f) continue;
            const Vec2 ra = cp.p - a.pos;
            const Vec2 rb = cp.p - b.pos;
            const Vec2 impulse = c.n * cp.normal_impulse + perp(c.n) * cp.tangent_impulse;
            apply_impulse(b, rb, impulse);
            apply_impulse(a, ra, -impulse);
        }
    }

    for (int iteration = 0; iteration < kVelIters; ++iteration) {
        solve_joints();
        solve_joints();
        for (Contact& c : contacts_) {
            Body& a = bodies_[c.a];
            Body& b = bodies_[c.b];
            const float im_a = inv_mass_of(a);
            const float im_b = inv_mass_of(b);
            const float ii_a = inv_inertia_of(a);
            const float ii_b = inv_inertia_of(b);
            if (im_a + im_b == 0.0f) continue;

            for (uint8_t k = 0; k < c.count; ++k) {
                ContactPoint& cp = c.pt[k];
                const Vec2 ra = cp.p - a.pos;
                const Vec2 rb = cp.p - b.pos;

                const Vec2 vrel = (b.vel + cross(b.ang_vel, rb)) -
                                  (a.vel + cross(a.ang_vel, ra));
                const float vn = dot(vrel, c.n);

                const float kn = im_a + im_b +
                                 ii_a * cross(ra, c.n) * cross(ra, c.n) +
                                 ii_b * cross(rb, c.n) * cross(rb, c.n);
                if (kn <= 0.0f) continue;

                float j = (-(vn - c.restitution_bias)) / kn;

                const float old = cp.normal_impulse;
                cp.normal_impulse = std::max(0.0f, old + j);
                j = cp.normal_impulse - old;

                const Vec2 impulse = c.n * j;
                apply_impulse(b, rb, impulse);
                apply_impulse(a, ra, -impulse);

                const Vec2 t = perp(c.n);
                const Vec2 vrel2 = (b.vel + cross(b.ang_vel, rb)) -
                                   (a.vel + cross(a.ang_vel, ra));
                const float vt = dot(vrel2, t);
                const float kt = im_a + im_b +
                                 ii_a * cross(ra, t) * cross(ra, t) +
                                 ii_b * cross(rb, t) * cross(rb, t);
                if (kt <= 0.0f) continue;

                float jt = -vt / kt;
                const float max_f = c.mu * cp.normal_impulse;
                const float old_t = cp.tangent_impulse;
                cp.tangent_impulse = clampf(old_t + jt, -max_f, max_f);
                jt = cp.tangent_impulse - old_t;

                const Vec2 tangent_impulse = t * jt;
                apply_impulse(b, rb, tangent_impulse);
                apply_impulse(a, ra, -tangent_impulse);
            }
        }
    }

    constexpr float kEma = 0.1f;
    for (uint32_t i = 0; i < joints_.size(); ++i) {
        if (!joint_alive_[i]) continue;
        Joint& j = joints_[i];
        const float inst_force = std::fabs(j.accumulated) / kDt;
        const float inst_torque = std::fabs(j.accumulated_angular) / kDt;
        j.force_ema += (inst_force - j.force_ema) * kEma;
        j.torque_ema += (inst_torque - j.torque_ema) * kEma;

        if (j.type == JointType::Weld && j.yield_torque > 0.0f &&
            j.torque_ema > j.yield_torque) {
            const float span = std::max(j.break_torque - j.yield_torque, 1.0f);
            const float over = clampf((j.torque_ema - j.yield_torque) / span, 0.0f, 1.0f);
            const Body& a = bodies_[j.a];
            const Body& b = bodies_[j.b];
            const float rel = (b.angle - a.angle) - j.ref_angle;
            j.ref_angle += (rel >= 0.0f ? 1.0f : -1.0f) * over * 1.5f * kDt;
        }

        const bool force_break = j.break_force > 0.0f && j.force_ema > j.break_force;
        const bool torque_break = j.break_torque > 0.0f && j.torque_ema > j.break_torque;
        if (force_break || torque_break) {
            joint_alive_[i] = 0;
            j.alive = false;
            ++j.generation;
            joint_free_.push_back(i);
            --joint_count_;
        }
    }
}

void World::solve_joints() {
    for (uint32_t i = 0; i < joints_.size(); ++i) {
        if (!joint_alive_[i]) continue;
        Joint& j = joints_[i];
        Body& a = bodies_[j.a];
        Body& b = bodies_[j.b];
        const float im_a = inv_mass_of(a);
        const float im_b = inv_mass_of(b);
        const float ii_a = inv_inertia_of(a);
        const float ii_b = inv_inertia_of(b);
        if (im_a + im_b == 0.0f) continue;

        const Vec2 ra = rotate(j.local_a, a.angle);
        const Vec2 rb = rotate(j.local_b, b.angle);
        const Vec2 pa = a.pos + ra;
        const Vec2 pb = b.pos + rb;
        const Vec2 d = pb - pa;
        const float len = length(d);
        const Vec2 n = len > 1e-6f ? d * (1.0f / len) : Vec2{1.0f, 0.0f};
        const float c = len - j.rest_length;

        const Vec2 vrel = (b.vel + cross(b.ang_vel, rb)) -
                          (a.vel + cross(a.ang_vel, ra));
        const float vn = dot(vrel, n);
        const float ca = cross(ra, n);
        const float cb = cross(rb, n);
        const float k = im_a + im_b + ii_a * ca * ca + ii_b * cb * cb;
        if (k <= 0.0f) continue;

        const float bias = clampf(kJointBeta * c / kDt, -kJointMaxBias, kJointMaxBias);
        const float impulse = -(vn * 1.4f + bias) / k;
        j.accumulated += impulse;

        const Vec2 p = n * impulse;
        apply_impulse(b, rb, p);
        apply_impulse(a, ra, -p);

        const float k_ang = ii_a + ii_b;
        if (k_ang > 0.0f) {
            const float rel = (b.angle - a.angle) - j.ref_angle;
            float c_ang = 0.0f;
            bool active = false;
            if (j.type == JointType::Weld) {
                c_ang = rel;
                active = true;
            } else if (j.has_limit) {
                if (rel > j.limit_max) {
                    c_ang = rel - j.limit_max;
                    active = true;
                } else if (rel < j.limit_min) {
                    c_ang = rel - j.limit_min;
                    active = true;
                }
            }
            if (active) {
                const float w_rel = b.ang_vel - a.ang_vel;
                const float bias_ang = clampf(kJointBeta * c_ang / kDt,
                                              -kJointMaxBias, kJointMaxBias);
                const float impulse_ang = -(w_rel * 1.6f + bias_ang) / k_ang;
                j.accumulated_angular += impulse_ang;
                a.ang_vel -= impulse_ang * ii_a;
                b.ang_vel += impulse_ang * ii_b;
            }

            if (j.motor_torque > 0.0f && j.motor_speed != 0.0f) {
                const float w_rel = b.ang_vel - a.ang_vel;
                const float target = j.motor_speed;
                float motor_impulse = (target - w_rel) / k_ang;
                const float max_impulse = j.motor_torque * kDt;
                motor_impulse = clampf(motor_impulse, -max_impulse, max_impulse);
                j.accumulated_angular += motor_impulse;
                a.ang_vel -= motor_impulse * ii_a;
                b.ang_vel += motor_impulse * ii_b;
            }

        }
    }
}

void World::project_joints() {
    constexpr int kIters = 3;
    for (int iteration = 0; iteration < kIters; ++iteration) {
        for (uint32_t i = 0; i < joints_.size(); ++i) {
            if (!joint_alive_[i]) continue;
            const Joint& j = joints_[i];
            Body& a = bodies_[j.a];
            Body& b = bodies_[j.b];
            const float im_a = inv_mass_of(a);
            const float im_b = inv_mass_of(b);
            const float inv_sum = im_a + im_b;
            if (inv_sum == 0.0f) continue;

            const Vec2 pa = a.pos + rotate(j.local_a, a.angle);
            const Vec2 pb = b.pos + rotate(j.local_b, b.angle);
            const Vec2 d = pb - pa;
            const float len = length(d);
            const float error = len - j.rest_length;
            if (std::fabs(error) <= kJointSlop) continue;

            const Vec2 n = len > 1e-6f ? d * (1.0f / len) : Vec2{1.0f, 0.0f};
            const float correction = (error - (error > 0.0f ? kJointSlop : -kJointSlop)) *
                                     kJointCorrection / inv_sum;
            if (im_a > 0.0f) a.pos += n * (correction * im_a);
            if (im_b > 0.0f) b.pos -= n * (correction * im_b);
        }
    }
}

void World::integrate() {
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& b = bodies_[i];
        if (!b.dynamic || b.asleep) continue;

        b.vel *= 1.0f / (1.0f + kLinDamp * kDt);
        b.ang_vel *= 1.0f / (1.0f + kAngDamp * kDt);

        b.pos += b.vel * kDt;
        b.angle += b.ang_vel * kDt;

        if (length_sq(b.vel) < kSleepLin * kSleepLin &&
            std::fabs(b.ang_vel) < kSleepAng) {
            b.sleep_timer += kDt;
            if (b.sleep_timer >= kSleepTime) {
                b.asleep = true;
                b.vel = {};
                b.ang_vel = 0.0f;
            }
        } else {
            b.sleep_timer = 0.0f;
        }
    }

    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        bodies_[i].force = {};
        bodies_[i].torque = 0.0f;
    }
}

void World::position_project() {
    constexpr int kIters = 3;
    constexpr float kCorrection = 0.8f;
    for (int iteration = 0; iteration < kIters; ++iteration) {
        for (Contact& c : contacts_) {
            Body& a = bodies_[c.a];
            Body& b = bodies_[c.b];
            const float im_a = inv_mass_of(a);
            const float im_b = inv_mass_of(b);
            const float inv_sum = im_a + im_b;
            if (inv_sum == 0.0f) continue;
            for (uint8_t k = 0; k < c.count; ++k) {
                const ContactPoint& cp = c.pt[k];
                const Vec2 wa = a.pos + rotate(cp.local_a, a.angle);
                const Vec2 wb = b.pos + rotate(cp.local_b, b.angle);
                const float sep = dot(wb - wa, c.n);
                const float correction = -sep - kSlop;
                if (correction <= 0.0f) continue;
                const Vec2 offset = c.n * (correction * kCorrection / inv_sum);
                if (im_a > 0.0f) a.pos -= offset * im_a;
                if (im_b > 0.0f) b.pos += offset * im_b;
            }
        }
    }
}

uint64_t World::state_hash() const {
    Hasher h(0x9E3779B97F4A7C15ull);
    h.u32(count_);
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const Body& b = bodies_[i];
        h.u32(i);
        h.u32(b.generation);
        h.u32(static_cast<uint32_t>(b.shape));
        h.boolean(b.dynamic);
        h.boolean(b.asleep);
        h.f32(b.pos.x);
        h.f32(b.pos.y);
        h.f32(b.vel.x);
        h.f32(b.vel.y);
        h.f32(b.angle);
        h.f32(b.ang_vel);
        h.f32(b.force.x);
        h.f32(b.force.y);
        h.f32(b.torque);
        h.f32(b.half.x);
        h.f32(b.half.y);
        h.f32(b.radius);
        h.f32(b.inv_mass);
        h.f32(b.inv_inertia);
        h.f32(b.sleep_timer);
        h.f32(b.temperature_c);
        h.boolean(b.ignited);
        h.f32(b.fuel_s);
        h.u8(static_cast<uint8_t>(b.component));
        h.boolean(b.signal);
        h.boolean(b.signal_in);
        h.u32(b.compartment_count);
        for (uint8_t c = 0; c < b.compartment_count; ++c) {
            const Compartment& comp = b.compartments[c];
            h.f32(comp.damage);
            h.boolean(comp.severed);
        }
    }
    h.u32(static_cast<uint32_t>(wires_.size()));
    for (const Wire& w : wires_) {
        h.u32(w.from);
        h.u32(w.to);
    }
    h.u32(projectile_count_);
    for (uint32_t i = 0; i < projectiles_.size(); ++i) {
        if (!projectile_alive_[i]) continue;
        const Projectile& p = projectiles_[i];
        h.u32(i);
        h.u32(p.generation);
        h.f32(p.pos.x);
        h.f32(p.pos.y);
        h.f32(p.vel.x);
        h.f32(p.vel.y);
    }
    h.u32(static_cast<uint32_t>(groups_.size()));
    for (const Vitals& v : groups_) {
        h.f32(v.blood_ml);
        h.f32(v.emitted_ml);
        h.boolean(v.unconscious);
        h.boolean(v.dead);
    }
    h.u32(joint_count_);
    for (uint32_t i = 0; i < joints_.size(); ++i) {
        if (!joint_alive_[i]) continue;
        const Joint& j = joints_[i];
        h.u32(i);
        h.u32(j.generation);
        h.u32(static_cast<uint32_t>(j.type));
        h.u32(j.a);
        h.u32(j.b);
        h.f32(j.local_a.x);
        h.f32(j.local_a.y);
        h.f32(j.local_b.x);
        h.f32(j.local_b.y);
        h.f32(j.rest_length);
        h.f32(j.ref_angle);
        h.f32(j.break_force);
        h.f32(j.break_torque);
        h.f32(j.force_ema);
        h.f32(j.torque_ema);
    }
    return h.digest();
}

} // namespace sim
