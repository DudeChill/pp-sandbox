#include <cstdio>
#include <fstream>
#include <vector>

#include "sim/world.h"

using namespace sim;

int main() {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
    const BodyId button = w.add_static_box(Vec2{-1.5f, 1.4f}, Vec2{0.12f, 0.08f});
    w.set_component(button, Component::Button);
    const BodyId thruster = w.add_box(Vec2{-0.8f, 1.4f}, Vec2{0.20f, 0.08f});
    w.set_component(thruster, Component::Thruster, 3000.0f);
    w.add_wire(button, thruster);
    w.set_button(button, true);

    InputFrame in;
    for (int i = 0; i < 90; ++i) w.step(in);

    std::vector<uint8_t> blob;
    w.save_scene(blob);
    std::ofstream out("scene.ppb", std::ios::binary);
    out.write(reinterpret_cast<const char*>(blob.data()),
              static_cast<std::streamsize>(blob.size()));
    std::printf("thruster pos=(%.2f,%.2f) vel=(%.2f,%.2f) wires=%zu\n",
                w.get(thruster).pos.x, w.get(thruster).pos.y,
                w.get(thruster).vel.x, w.get(thruster).vel.y, w.wires().size());
    return 0;
}
