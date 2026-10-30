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
constexpr float kGravity = -9.81f;
constexpr float kSleepLin = 0.03f;
constexpr float kSleepAng = 0.15f;
constexpr float kSleepTime = 0.5f;
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

} // namespace

World::World() {
    bodies_.reserve(8192);
    alive_.reserve(8192);
    free_.reserve(1024);
    order_.reserve(8192);
    aabbs_.reserve(8192);
    contacts_.reserve(1u << 16);
    prev_contacts_.reserve(1u << 16);
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
}

void World::clear() {
    bodies_.clear();
    alive_.clear();
    free_.clear();
    contacts_.clear();
    prev_contacts_.clear();
    count_ = 0;
    contact_count_ = 0;
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

void World::step(const InputFrame& in) {
    // 1-2. Input and force accumulation.
    for (uint32_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        Body& b = bodies_[i];
        b.force = {};
        b.torque = 0.0f;
        if (!b.dynamic) continue;

        if (in.grab_active && in.grab == BodyId{i, b.generation}) {
            b.asleep = false;
            b.sleep_timer = 0.0f;
            Vec2 f = (in.grab_target - b.pos) * kGrabK - b.vel * kGrabC;
            const float mag = length(f);
            if (mag > kMaxGrabForce) f = f * (kMaxGrabForce / mag);
            b.force += f;
        }

        if (b.asleep) continue;
        b.vel += (b.force * b.inv_mass + Vec2{0.0f, kGravity}) * kDt;
        b.ang_vel += b.torque * b.inv_inertia * kDt;
    }

    broadphase();
    solve();
    integrate();
    position_project();

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
    }
    return h.digest();
}

} // namespace sim
