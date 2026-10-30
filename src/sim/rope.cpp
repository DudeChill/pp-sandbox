#include "rope.h"

namespace sim {

Rope spawn_rope(World& world, Vec2 anchor, int links, float segment_length,
                float radius, float break_force) {
    Rope rope;
    const int count = links < 0 ? 0 : (links > Rope::kMaxLinks ? Rope::kMaxLinks : links);
    const BodyId anchor_body = world.add_static_box(anchor, Vec2{0.05f, 0.05f});
    BodyId prev = anchor_body;
    Vec2 prev_anchor = anchor;

    for (int i = 0; i < count; ++i) {
        const Vec2 center = anchor + Vec2{0.0f, -segment_length * static_cast<float>(i + 1)};
        const BodyId link = world.add_circle(center, radius, 300.0f);
        world.get(link).friction = 0.5f;
        world.get(link).restitution = 0.0f;
        rope.joints[i] = world.add_distance_joint(prev, link, prev_anchor, center,
                                                  segment_length, break_force);
        rope.links[i] = link;
        prev = link;
        prev_anchor = center;
        rope.count = i + 1;
    }
    return rope;
}

} // namespace sim
