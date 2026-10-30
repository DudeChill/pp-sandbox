#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "render/gl_api.h"
#include "sim/alloc.h"
#include "render/renderer.h"
#include "sim/ragdoll.h"
#include "sim/rope.h"
#include "sim/world.h"

using namespace sim;

namespace {

struct Camera {
    Vec2 center{0.0f, 4.0f};
    float view_height = 12.0f;

    [[nodiscard]] Vec2 screen_to_world(int mx, int my, int w, int h) const {
        const float half_h = view_height * 0.5f;
        const float half_w = half_h * static_cast<float>(w) / static_cast<float>(h);
        const float nx = static_cast<float>(mx) / static_cast<float>(w);
        const float ny = static_cast<float>(my) / static_cast<float>(h);
        return {center.x - half_w + nx * 2.0f * half_w,
                center.y + half_h - ny * 2.0f * half_h};
    }
};

void build_scene(World& w) {
    w.clear();
    w.add_static_box(Vec2{0.0f, -0.5f}, Vec2{60.0f, 0.5f});
    w.add_static_box(Vec2{-24.0f, 8.0f}, Vec2{0.5f, 8.0f});
    w.add_static_box(Vec2{24.0f, 8.0f}, Vec2{0.5f, 8.0f});
    w.add_static_box(Vec2{-6.0f, 6.0f}, Vec2{3.0f, 0.15f}, 0.42f);
    w.add_static_box(Vec2{8.0f, 5.5f}, Vec2{2.0f, 0.15f}, -0.3f);

    for (int i = 0; i < 6; ++i) {
        w.add_box(Vec2{2.0f, 0.5f + static_cast<float>(i)}, Vec2{0.5f, 0.5f});
    }
    for (int i = 0; i < 18; ++i) {
        const float x = 12.0f + static_cast<float>(i % 6) * 0.55f;
        const float y = 0.4f + static_cast<float>(i / 6) * 0.55f;
        w.add_circle(Vec2{x, y}, 0.26f);
    }
}

} // namespace

int main(int, char**) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("pp — physics sandbox", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        std::fprintf(stderr, "gl context failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    if (!glapi::load()) {
        std::fprintf(stderr, "failed to load GL entry points\n");
        return 1;
    }
    std::printf("GL_VERSION: %s\n", reinterpret_cast<const char*>(glapi::GetString(0x1F02)));
    std::printf("GL_RENDERER: %s\n", reinterpret_cast<const char*>(glapi::GetString(0x1F01)));

    render::Renderer renderer;
    if (!renderer.init()) {
        std::fprintf(stderr, "renderer init failed\n");
        return 1;
    }

    World world;
    build_scene(world);

    Camera cam;
    bool running = true;
    bool paused = false;
    BodyId grab{};
    Vec2 mouse_world{};
    enum class Tool { None, Weld, Cut };
    Tool tool = Tool::None;
    BodyId weld_first{};
    Vec2 weld_point{};
    struct Blood {
        Vec2 pos{};
        Vec2 vel{};
        float life = 1.0f;
        float radius = 0.02f;
    };
    std::vector<Blood> blood;
    blood.reserve(4096);
    std::size_t wounds_seen = 0;
    uint32_t blood_rng = 0x9E3779B9u;

    double last = static_cast<double>(SDL_GetPerformanceCounter()) /
                  static_cast<double>(SDL_GetPerformanceFrequency());
    double accumulator = 0.0;
    int width = 1280;
    int height = 720;
    int frame = 0;
    Uint64 fps_start = SDL_GetTicks64();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                width = e.window.data1;
                height = e.window.data2;
            }
            if (e.type == SDL_MOUSEMOTION) {
                mouse_world = cam.screen_to_world(e.motion.x, e.motion.y, width, height);
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                const Vec2 p = cam.screen_to_world(e.button.x, e.button.y, width, height);
                mouse_world = p;
                if (e.button.button == SDL_BUTTON_LEFT) {
                    const BodyId hit = world.pick(p);
                    if (tool == Tool::Weld) {
                        if (hit.valid()) {
                            if (!weld_first.valid()) {
                                weld_first = hit;
                                weld_point = p;
                            } else if (hit != weld_first) {
                                world.add_weld_joint(weld_first, hit, (weld_point + p) * 0.5f);
                                weld_first = BodyId{};
                            }
                        }
                    } else if (tool == Tool::Cut) {
                        JointId best{};
                        float best_d = 0.25f;
                        world.for_each_joint([&](JointId id, const Joint& j) {
                            const BodyId ba = world.id_at(j.a);
                            const BodyId bb = world.id_at(j.b);
                            if (!ba.valid() || !bb.valid()) return;
                            const Vec2 pa = world.get(ba).pos + rotate(j.local_a, world.get(ba).angle);
                            const Vec2 pb = world.get(bb).pos + rotate(j.local_b, world.get(bb).angle);
                            const Vec2 m = (pa + pb) * 0.5f;
                            const float d = length(m - p);
                            if (d < best_d) {
                                best_d = d;
                                best = id;
                            }
                        });
                        if (best.valid()) world.remove_joint(best);
                    } else {
                        grab = world.pick(p);
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    world.add_circle(p, 0.25f);
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                grab = BodyId{};
            }
            if (e.type == SDL_MOUSEWHEEL) {
                cam.view_height = clampf(cam.view_height * (e.wheel.y > 0 ? 0.9f : 1.1f),
                                         3.0f, 40.0f);
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_SPACE: paused = !paused; break;
                    case SDLK_b: world.add_box(mouse_world, Vec2{0.4f, 0.4f}); break;
                    case SDLK_h: spawn_ragdoll(world, mouse_world); break;
                    case SDLK_f: {
                        const Vec2 start = mouse_world - Vec2{12.0f, 0.0f};
                        const Vec2 dir = normalize(mouse_world - start);
                        world.spawn_projectile(start, dir * 350.0f, 0.008f, 0.0045f, 0.0015f);
                        break;
                    }
                    case SDLK_o: spawn_rope(world, mouse_world, 10, 0.22f, 0.045f, 0.0f); break;
                    case SDLK_g: tool = tool == Tool::Weld ? Tool::None : Tool::Weld;
                                 weld_first = BodyId{}; break;
                    case SDLK_c: tool = tool == Tool::Cut ? Tool::None : Tool::Cut; break;
                    case SDLK_x: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.remove(hit);
                        if (hit == grab) grab = BodyId{};
                        break;
                    }
                    case SDLK_r: build_scene(world); blood.clear(); wounds_seen = 0; break;
                    default: break;
                }
            }
        }

        int gx = 0;
        int gy = 0;
        int wx = 0;
        int wy = 0;
        const Uint32 buttons = SDL_GetGlobalMouseState(&gx, &gy);
        SDL_GetWindowPosition(window, &wx, &wy);
        mouse_world = cam.screen_to_world(gx - wx, gy - wy, width, height);
        if ((buttons & SDL_BUTTON_LMASK) == 0) grab = BodyId{};
        if (grab.valid() && !world.alive(grab)) grab = BodyId{};

        const double now = static_cast<double>(SDL_GetPerformanceCounter()) /
                           static_cast<double>(SDL_GetPerformanceFrequency());
        double frame_dt = now - last;
        last = now;
        frame_dt = std::min(frame_dt, 0.1);

        if (!paused) {
            accumulator += frame_dt;
            int steps = 0;
            while (accumulator >= static_cast<double>(World::kDt) && steps < 8) {
                InputFrame in;
                in.grab = grab;
                in.grab_target = mouse_world;
                in.grab_active = grab.valid();
                world.step(in);
                auto next_rand = [&]() {
                    blood_rng ^= blood_rng << 13;
                    blood_rng ^= blood_rng >> 17;
                    blood_rng ^= blood_rng << 5;
                    return static_cast<float>(blood_rng & 0xFFFFu) / 65535.0f;
                };
                const auto& ws = world.wounds();
                if (ws.size() < wounds_seen) wounds_seen = 0;
                for (std::size_t wi = wounds_seen; wi < ws.size(); ++wi) {
                    const WoundEvent& we = ws[wi];
                    if (we.damage <= 0.0f) continue;
                    const BodyId wb = world.id_at(we.body);
                    if (!wb.valid() || world.get(wb).vascularity <= 0.0f) continue;
                    const int n = 6 + static_cast<int>(we.damage * 14.0f);
                    for (int k = 0; k < n; ++k) {
                        if (blood.size() >= 4096) break;
                        const float spread = (next_rand() - 0.5f) * 2.4f;
                        Vec2 v = we.dir * (1.0f + next_rand() * 2.5f) +
                                 Vec2{spread, next_rand() * 1.5f};
                        blood.push_back(Blood{we.pos, v * (1.0f + next_rand() * 2.0f),
                                              1.0f, 0.03f + next_rand() * 0.05f});
                    }
                }
                wounds_seen = ws.size();
                accumulator -= static_cast<double>(World::kDt);
                ++steps;
            }
            if (steps == 8) accumulator = 0.0;
        }

        {
            const double bdt = std::min(frame_dt, 0.05);
            for (auto& b : blood) {
                b.vel.y -= 9.81 * bdt;
                b.pos += b.vel * bdt;
                if (b.pos.y < 0.01f) {
                    b.pos.y = 0.01f;
                    b.vel = {0.0f, 0.0f};
                    b.life -= static_cast<float>(bdt) * 0.35f;
                } else {
                    b.life -= static_cast<float>(bdt) * 0.08f;
                }
                if (b.life < 0.0f) b.life = 0.0f;
            }
            blood.erase(std::remove_if(blood.begin(), blood.end(),
                                       [](const Blood& b) { return b.life <= 0.0f; }),
                        blood.end());
        }

        renderer.begin(width, height, cam.center, cam.view_height);
        world.for_each([&](BodyId id, const Body& b) {
            render::Color color;
            if (!b.dynamic) {
                color = {0.22f, 0.24f, 0.29f, 1.0f};
            } else if (id == grab) {
                color = {0.30f, 0.85f, 0.52f, 1.0f};
            } else if (b.asleep) {
                color = b.shape == Shape::Circle ? render::Color{0.42f, 0.31f, 0.20f, 1.0f}
                                                 : render::Color{0.21f, 0.29f, 0.40f, 1.0f};
            } else {
                color = b.shape == Shape::Circle ? render::Color{0.88f, 0.56f, 0.24f, 1.0f}
                                                 : render::Color{0.38f, 0.56f, 0.82f, 1.0f};
            }
            if (b.shape == Shape::Circle) {
                renderer.circle(b.pos, b.radius, color);
            } else {
                renderer.box(b.pos, b.half, b.angle, color);
            }
        });
        for (const Blood& b : blood) {
            renderer.circle(b.pos, b.radius, render::Color{0.62f, 0.05f, 0.08f, b.life});
        }
        world.for_each_projectile([&](ProjectileId, const Projectile& p) {
            renderer.circle(p.pos, 0.02f, render::Color{1.0f, 0.92f, 0.45f, 1.0f});
        });
        world.for_each_joint([&](JointId, const Joint& j) {
            const BodyId ba = world.id_at(j.a);
            const BodyId bb = world.id_at(j.b);
            if (!ba.valid() || !bb.valid()) return;
            const Vec2 pa = world.get(ba).pos + rotate(j.local_a, world.get(ba).angle);
            const Vec2 pb = world.get(bb).pos + rotate(j.local_b, world.get(bb).angle);
            render::Color c{0.55f, 0.55f, 0.60f, 0.9f};
            if (j.type == JointType::Weld) c = {0.25f, 0.80f, 0.90f, 0.95f};
            if (j.type == JointType::Revolute) c = {0.85f, 0.75f, 0.30f, 0.95f};
            if (length(pb - pa) < 0.01f) {
                renderer.circle(pa, 0.02f, c);
            } else {
                renderer.line(pa, pb, 0.02f, c);
            }
        });
        renderer.end();
        SDL_GL_SwapWindow(window);

        ++frame;
        if (frame % 120 == 0) {
            const Uint64 now_ms = SDL_GetTicks64();
            const double fps = 120000.0 / static_cast<double>(now_ms - fps_start + 1);
            double blood_min = -1.0;
            int dead = 0;
            for (uint32_t g = 0; g < world.group_count(); ++g) {
                const Vitals& v = world.vitals(g);
                if (blood_min < 0.0 || v.blood_ml < blood_min) blood_min = v.blood_ml;
                dead += v.dead ? 1 : 0;
            }
            std::printf("fps %.1f  bodies %u  contacts %u  wounds %zu  groups %u  dead %d  blood %.0f  allocs %zu\n",
                        fps, world.body_count(), world.contact_count(),
                        world.wounds().size(), world.group_count(), dead, blood_min,
                        sim::allocation_count());
            fps_start = now_ms;
        }
    }

    renderer.shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
