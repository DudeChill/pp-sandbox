#include "cloth.h"

#include <cmath>

namespace sim {

Cloth spawn_cloth(World& world, Vec2 top_left, int cols, int rows, float spacing,
                  float radius, float break_force) {
    Cloth cloth;
    if (cols < 2) cols = 2;
    if (rows < 2) rows = 2;
    if (cols * rows > Cloth::kMaxNodes) return cloth;
    cloth.cols = cols;
    cloth.rows = rows;

    auto index = [cols](int x, int y) { return y * cols + x; };

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            // Spawn rows compressed so the pinned sheet has slack and drapes
            // when gravity pulls the structural constraints taut.
            const Vec2 pos = top_left + Vec2{static_cast<float>(x) * spacing,
                                             -static_cast<float>(y) * spacing * 0.35f};
            BodyId node = world.add_circle(pos, radius, y == 0 ? 0.0f : 250.0f);
            world.get(node).friction = 0.7f;
            world.get(node).restitution = 0.0f;
            cloth.nodes[index(x, y)] = node;
            ++cloth.count;
        }
    }
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const BodyId a = cloth.nodes[index(x, y)];
            if (x + 1 < cols) {
                const BodyId b = cloth.nodes[index(x + 1, y)];
                world.add_distance_joint(a, b, world.get(a).pos, world.get(b).pos,
                                         spacing, break_force);
            }
            if (y + 1 < rows) {
                const BodyId b = cloth.nodes[index(x, y + 1)];
                world.add_distance_joint(a, b, world.get(a).pos, world.get(b).pos,
                                         spacing, break_force);
            }
        }
    }
    // The top row is static, so it acts as the pinned edge without anchors
    // overlapping their nodes.
    return cloth;
}

Cloth spawn_soft_blob(World& world, Vec2 center, int segments, float radius,
                      float break_force) {
    Cloth blob;
    if (segments < 4) segments = 4;
    if (segments > 24) segments = 24;
    blob.cols = segments;
    blob.rows = 1;
    const BodyId core = world.add_circle(center, radius * 0.35f, 200.0f);
    world.get(core).friction = 0.6f;
    blob.nodes[blob.count++] = core;
    BodyId ring[24]{};
    for (int i = 0; i < segments; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) /
                            static_cast<float>(segments);
        const Vec2 pos = center + Vec2{std::cos(angle), std::sin(angle)} * radius;
        ring[i] = world.add_circle(pos, radius * 0.22f, 300.0f);
        world.get(ring[i]).friction = 0.6f;
        blob.nodes[blob.count++] = ring[i];
        world.add_distance_joint(core, ring[i], world.get(core).pos,
                                 world.get(ring[i]).pos, radius, break_force);
    }
    for (int i = 0; i < segments; ++i) {
        const int j = (i + 1) % segments;
        world.add_distance_joint(ring[i], ring[j], world.get(ring[i]).pos,
                                 world.get(ring[j]).pos,
                                 2.0f * radius * std::sin(3.14159265f / static_cast<float>(segments)),
                                 break_force);
    }
    return blob;
}

} // namespace sim
