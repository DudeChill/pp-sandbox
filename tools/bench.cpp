#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "sim/world.h"

using namespace sim;

int main() {
    World w;
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{200.0f, 0.5f});
    for (int i = 0; i < 1500; ++i) {
        w.add_box(Vec2{static_cast<float>(i % 50) * 0.85f - 21.0f,
                      0.6f + static_cast<float>(i / 50) * 0.9f},
                  Vec2{0.35f, 0.35f});
    }
    for (int i = 0; i < 300; ++i) {
        w.spawn_projectile(Vec2{-30.0f + static_cast<float>(i) * 0.05f, 6.0f},
                           Vec2{200.0f, 0.0f}, 0.008f, 0.0045f, 0.0015f);
    }
    InputFrame in;
    for (int i = 0; i < 120; ++i) w.step(in);  // settle/warm

    std::vector<double> times;
    times.reserve(240);
    for (int i = 0; i < 240; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        w.step(in);
        const auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(times.begin(), times.end());
    const double avg = [&] {
        double s = 0.0;
        for (double t : times) s += t;
        return s / static_cast<double>(times.size());
    }();
    std::printf("bodies=%u projectiles=%u contacts=%u  avg=%.2f ms  p50=%.2f  p99=%.2f  max=%.2f\n",
                w.body_count(), 0u, w.contact_count(), avg,
                times[times.size() / 2], times[(times.size() * 99) / 100],
                times.back());
    return 0;
}
