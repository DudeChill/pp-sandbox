#include "platform/sdl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "render/gl_api.h"
#include "sim/alloc.h"
#include "app/audio.h"
#include "render/renderer.h"
#include "sim/ragdoll.h"
#include "sim/rope.h"
#include "sim/ammo.h"
#include "sim/cloth.h"
#include "sim/weapon.h"
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

struct Stats {
    double avg = 0.0;
    double p50 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
    int n = 0;
};

Stats stats_of(const double* values, int count) {
    Stats s;
    if (count <= 0) return s;
    std::vector<double> sorted(values, values + count);
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : sorted) sum += v;
    s.n = count;
    s.avg = sum / static_cast<double>(count);
    s.p50 = sorted[static_cast<std::size_t>((count - 1) * 50 / 100)];
    s.p99 = sorted[static_cast<std::size_t>((count - 1) * 99 / 100)];
    s.max = sorted.back();
    return s;
}

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

int main(int argc, char** argv) {
    std::fprintf(stderr, "pp: main entered (argc=%d)\n", argc);
    std::fflush(stderr);
    int bench_frames = 0;
    int startup_threads = 1;
    bool self_test = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            bench_frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            startup_threads = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--self-test") == 0) {
            self_test = true;
        }
    }
    SDL_SetMainReady();
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    if (self_test) {
        std::printf("SELF-TEST starting\n");
        std::fflush(stdout);
        std::fprintf(stderr, "pp: self-test block\n");
        std::fflush(stderr);
        if (SDL_Init(SDL_INIT_TIMER) != 0) {
            std::fprintf(stderr, "self-test: SDL init failed: %s\n", SDL_GetError());
            return 2;
        }
        World w;
        build_scene(w);
        w.set_thread_count(1);
        std::fprintf(stderr, "pp: self-test world ready\n");
        std::fflush(stderr);
        InputFrame in;
        for (int i = 0; i < 240; ++i) w.step(in);
        std::fprintf(stderr, "pp: self-test steps done\n");
        std::fflush(stderr);
        bool finite = true;
        w.for_each([&](BodyId, const Body& b) {
            finite = finite && std::isfinite(b.pos.x) && std::isfinite(b.pos.y) &&
                     std::isfinite(b.vel.x) && std::isfinite(b.vel.y) &&
                     std::isfinite(b.angle);
        });
        const uint64_t hash = w.state_hash();
        const uint64_t periodic = w.last_periodic_hash();
        const bool ok = finite && w.body_count() > 0 && hash != 0 && periodic != 0;
        std::printf("SELF-TEST %s bodies=%u contacts=%u hash=%016llx periodic=%016llx\n",
                    ok ? "OK" : "FAILED", w.body_count(), w.contact_count(),
                    static_cast<unsigned long long>(hash),
                    static_cast<unsigned long long>(periodic));
        SDL_Quit();
        return ok ? 0 : 3;
    }
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
    SDL_GL_SetSwapInterval(bench_frames > 0 ? 0 : 1);
    SDL_StartTextInput();

    if (!glapi::load()) {
        std::fprintf(stderr, "failed to load GL entry points\n");
        return 1;
    }
    std::printf("GL_VERSION: %s\n", reinterpret_cast<const char*>(glapi::GetString(0x1F02)));
    std::printf("GL_RENDERER: %s\n", reinterpret_cast<const char*>(glapi::GetString(0x1F01)));

    app::Audio audio;
    audio.init();

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
    enum class Tool { None, Weld, Cut, Tourniquet, Blade, Motor, Wire };
    Tool tool = Tool::None;
    BodyId weld_first{};
    Vec2 weld_point{};
    int ammo_index = 0;
    world.set_thread_count(startup_threads);

    bool console_open = false;
    std::string console_text;
    std::string console_out = "console ready: help";
    bool panning = false;
    int pan_x = 0;
    int pan_y = 0;
    double sim_ms[4096]{};
    int sim_ms_count = 0;
    double render_ms[4096]{};
    int render_ms_count = 0;

    const auto now_s = [] {
        return static_cast<double>(SDL_GetPerformanceCounter()) /
               static_cast<double>(SDL_GetPerformanceFrequency());
    };

    auto stats_text = [](const char* name, const Stats& st) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s n=%d avg=%.3f p50=%.3f p99=%.3f max=%.3f ms",
                      name, st.n, st.avg, st.p50, st.p99, st.max);
        return std::string(buf);
    };

    auto execute_console = [&](const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        std::ostringstream out;
        if (cmd.empty()) {
            return;
        } else if (cmd == "help") {
            out << "spawn circle|box|ragdoll|rope|cloth|blob|glass|battery|button|thruster"
                   "; set gravity <y>|<x y>; set threads <n>; pause <0|1>; measure; profile; dump; hash; kill";
        } else if (cmd == "spawn") {
            std::string what;
            iss >> what;
            if (what == "circle") world.add_circle(mouse_world, 0.25f);
            else if (what == "box") world.add_box(mouse_world, Vec2{0.4f, 0.4f});
            else if (what == "ragdoll") spawn_ragdoll(world, mouse_world);
            else if (what == "rope") spawn_rope(world, mouse_world, 10, 0.22f, 0.045f, 0.0f);
            else if (what == "cloth") spawn_cloth(world, mouse_world, 6, 4, 0.12f, 0.03f);
            else if (what == "blob") spawn_soft_blob(world, mouse_world, 8, 0.35f);
            else if (what == "glass") {
                const BodyId pane = world.add_static_box(mouse_world, Vec2{0.5f, 0.5f});
                world.set_material(pane, MaterialId::Glass);
            } else if (what == "battery") {
                const BodyId b = world.add_static_box(mouse_world, Vec2{0.10f, 0.07f});
                world.set_battery(b, 220.0f);
            } else if (what == "button") {
                const BodyId b = world.add_static_box(mouse_world, Vec2{0.12f, 0.08f});
                world.set_component(b, Component::Button);
            } else if (what == "thruster") {
                const BodyId b = world.add_box(mouse_world, Vec2{0.20f, 0.08f});
                world.set_component(b, Component::Thruster, 3000.0f);
            } else {
                out << "unknown spawn target: " << what;
            }
            if (out.tellp() == 0) out << "spawned " << what << " at cursor";
        } else if (cmd == "set") {
            std::string key;
            iss >> key;
            if (key == "threads") {
                int n = 1;
                iss >> n;
                world.set_thread_count(n);
                out << "threads " << world.thread_count();
            } else if (key == "gravity") {
                float a = 0.0f;
                float b = 0.0f;
                if (iss >> a) {
                    if (iss >> b) {
                        world.set_gravity(Vec2{a, b});
                    } else {
                        world.set_gravity(Vec2{0.0f, -a});
                    }
                    out << "gravity " << world.gravity().x << " " << world.gravity().y;
                } else {
                    out << "usage: set gravity <y>|<x y>";
                }
            } else {
                out << "unknown set key: " << key;
            }
        } else if (cmd == "pause") {
            int p = 1;
            iss >> p;
            paused = p != 0;
            out << (paused ? "paused" : "running");
        } else if (cmd == "measure") {
            out << "bodies=" << world.body_count() << " contacts=" << world.contact_count()
                << " joints=" << world.joint_count() << " groups=" << world.group_count()
                << " wires=" << world.wires().size() << " wounds=" << world.wounds().size();
        } else if (cmd == "profile") {
            out << stats_text("sim", stats_of(sim_ms, sim_ms_count)) << " | "
                << stats_text("render", stats_of(render_ms, render_ms_count));
        } else if (cmd == "dump") {
            double blood_min = -1.0;
            int dead = 0;
            for (uint32_t g = 0; g < world.group_count(); ++g) {
                const Vitals& v = world.vitals(g);
                if (blood_min < 0.0 || v.blood_ml < blood_min) blood_min = v.blood_ml;
                dead += v.dead ? 1 : 0;
            }
            char buf[256];
            std::snprintf(buf, sizeof buf,
                          "hash=%016llx bodies=%u contacts=%u joints=%u groups=%u dead=%d blood=%.0f "
                          "wounds=%zu burning=%d wires=%zu",
                          static_cast<unsigned long long>(world.state_hash()),
                          world.body_count(), world.contact_count(), world.joint_count(),
                          world.group_count(), dead, blood_min, world.wounds().size(),
                          world.burning_count(), world.wires().size());
            out << buf;
        } else if (cmd == "hash") {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%016llx",
                          static_cast<unsigned long long>(world.state_hash()));
            out << buf;
        } else if (cmd == "kill") {
            const BodyId hit = world.pick(mouse_world);
            if (hit.valid()) {
                world.remove(hit);
                out << "removed body";
            } else {
                out << "nothing under cursor";
            }
        } else {
            out << "unknown command: " << cmd << " (try help)";
        }
        console_out = out.str();
        std::printf("console> %s\n", console_out.c_str());
    };

    struct ReplayFrame {
        BodyId grab{};
        Vec2 target{};
        bool active = false;
    };
    bool recording = false;
    std::vector<uint8_t> record_scene;
    std::vector<ReplayFrame> record_frames;
    bool playing = false;
    std::vector<uint8_t> play_scene;
    std::vector<ReplayFrame> play_frames;
    std::size_t play_index = 0;
    uint64_t play_expected = 0;

    auto write_u32 = [](std::vector<uint8_t>& b, uint32_t v) {
        b.push_back(static_cast<uint8_t>(v));
        b.push_back(static_cast<uint8_t>(v >> 8));
        b.push_back(static_cast<uint8_t>(v >> 16));
        b.push_back(static_cast<uint8_t>(v >> 24));
    };
    auto write_f32 = [&](std::vector<uint8_t>& b, float f) {
        uint32_t v = 0;
        std::memcpy(&v, &f, sizeof v);
        write_u32(b, v);
    };
    auto read_u32 = [](const std::vector<uint8_t>& b, std::size_t& off, bool& ok) {
        if (off + 4 > b.size()) { ok = false; return 0u; }
        const uint32_t v = static_cast<uint32_t>(b[off]) |
                           (static_cast<uint32_t>(b[off + 1]) << 8) |
                           (static_cast<uint32_t>(b[off + 2]) << 16) |
                           (static_cast<uint32_t>(b[off + 3]) << 24);
        off += 4;
        return v;
    };
    auto read_f32 = [&](const std::vector<uint8_t>& b, std::size_t& off, bool& ok) {
        const uint32_t v = read_u32(b, off, ok);
        float f = 0.0f;
        std::memcpy(&f, &v, sizeof f);
        return f;
    };
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
    double last_emitted_ml = -1.0;
    float blood_debt_ml = 0.0f;

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
            if (e.type == SDL_TEXTINPUT && console_open) {
                console_text += e.text.text;
                continue;
            }
            if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_BACKQUOTE) {
                console_open = !console_open;
                console_text.clear();
                continue;
            }
            if (console_open) {
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        console_open = false;
                    } else if (e.key.keysym.sym == SDLK_RETURN ||
                               e.key.keysym.sym == SDLK_KP_ENTER) {
                        execute_console(console_text);
                        console_text.clear();
                    } else if (e.key.keysym.sym == SDLK_BACKSPACE && !console_text.empty()) {
                        console_text.pop_back();
                    }
                }
                continue;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) {
                panning = true;
                pan_x = e.button.x;
                pan_y = e.button.y;
            }
            if (e.type == SDL_MOUSEMOTION) {
                mouse_world = cam.screen_to_world(e.motion.x, e.motion.y, width, height);
                if (panning) {
                    const float scale = cam.view_height / static_cast<float>(std::max(1, height));
                    cam.center.x -= static_cast<float>(e.motion.x - pan_x) * scale;
                    cam.center.y += static_cast<float>(e.motion.y - pan_y) * scale;
                    pan_x = e.motion.x;
                    pan_y = e.motion.y;
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) {
                panning = false;
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
                    } else if (tool == Tool::Wire) {
                        if (hit.valid()) {
                            if (!weld_first.valid()) {
                                weld_first = hit;
                            } else if (hit != weld_first) {
                                world.add_wire(weld_first, hit);
                                weld_first = BodyId{};
                            }
                        }
                    } else if (tool == Tool::Motor) {
                        if (hit.valid()) {
                            if (!weld_first.valid()) {
                                weld_first = hit;
                                weld_point = p;
                            } else if (hit != weld_first) {
                                world.add_motor_joint(weld_first, hit,
                                                      (weld_point + p) * 0.5f, 3.0f, 500.0f);
                                weld_first = BodyId{};
                            }
                        }
                    } else if (tool == Tool::Blade) {
                        if (hit.valid()) {
                            world.apply_blade(hit, p, Vec2{1.0f, 0.0f}, 350.0f);
                        }
                    } else if (tool == Tool::Tourniquet) {
                        if (hit.valid()) world.apply_tourniquet(hit);
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
                        if (hit.valid() &&
                            world.get(hit).component == Component::Button) {
                            world.set_button(hit, !world.get(hit).signal);
                        } else {
                            grab = world.pick(p);
                        }
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
                    case SDLK_e: world.explode(mouse_world, 4.0f, 8000.0f); break;
                    case SDLK_i: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.ignite(hit);
                        break;
                    }
                    case SDLK_k: {
                        const BodyId pane = world.add_static_box(mouse_world,
                                                                 Vec2{0.5f, 0.5f});
                        world.set_material(pane, MaterialId::Glass);
                        break;
                    }
                    case SDLK_1: ammo_index = 0; break;
                    case SDLK_2: ammo_index = 1; break;
                    case SDLK_3: ammo_index = 2; break;
                    case SDLK_4: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.apply_iv(hit, 500.0f);
                        break;
                    }
                    case SDLK_5: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.apply_blood_bag(hit);
                        break;
                    }
                    case SDLK_6: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.defibrillate(hit);
                        break;
                    }
                    case SDLK_7: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.apply_chest_seal(hit);
                        break;
                    }
                    case SDLK_8: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.apply_splint(hit);
                        break;
                    }
                    case SDLK_9: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.set_armour(hit, 0.06f, 2.0e8f, true);
                        break;
                    }
                    case SDLK_0: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.set_armour(hit, 0.08f, 1.0e9f, false);
                        break;
                    }
                    case SDLK_f: {
                        const Vec2 start = mouse_world - Vec2{12.0f, 0.0f};
                        const Vec2 dir = normalize(mouse_world - start);
                        WeaponSpec weapon;
                        weapon.ammo_index = ammo_index;
                        weapon.barrel_mm = default_barrel_mm(ammo_index);
                        weapon.zero_m = 100.0f;
                        fire_weapon(world, weapon, start, dir);
                        audio.gunshot(mouse_world, cam.center,
                                      muzzle_velocity(ammo_spec(ammo_index),
                                                      weapon.barrel_mm));
                        break;
                    }
                    case SDLK_o: spawn_rope(world, mouse_world, 10, 0.22f, 0.045f, 0.0f); break;
                    case SDLK_g: tool = tool == Tool::Weld ? Tool::None : Tool::Weld;
                                 weld_first = BodyId{}; break;
                    case SDLK_c: tool = tool == Tool::Cut ? Tool::None : Tool::Cut; break;
                    case SDLK_t: tool = tool == Tool::Tourniquet ? Tool::None : Tool::Tourniquet;
                                 break;
                    case SDLK_v: tool = tool == Tool::Blade ? Tool::None : Tool::Blade; break;
                    case SDLK_m: tool = tool == Tool::Motor ? Tool::None : Tool::Motor;
                                 weld_first = BodyId{}; break;
                    case SDLK_l: spawn_cloth(world, mouse_world, 6, 4, 0.12f, 0.03f); break;
                    case SDLK_SEMICOLON: spawn_soft_blob(world, mouse_world, 8, 0.35f); break;
                    case SDLK_p: {
                        const BodyId battery = world.add_static_box(mouse_world,
                                                                    Vec2{0.10f, 0.07f});
                        world.set_battery(battery, 220.0f);
                        break;
                    }
                    case SDLK_n: {
                        const BodyId button = world.add_static_box(mouse_world,
                                                                   Vec2{0.12f, 0.08f});
                        world.set_component(button, Component::Button);
                        break;
                    }
                    case SDLK_y: {
                        const BodyId thruster = world.add_box(mouse_world,
                                                              Vec2{0.20f, 0.08f});
                        world.set_component(thruster, Component::Thruster, 3000.0f);
                        break;
                    }
                    case SDLK_z: tool = tool == Tool::Wire ? Tool::None : Tool::Wire;
                                 weld_first = BodyId{}; break;
                    case SDLK_x: {
                        const BodyId hit = world.pick(mouse_world);
                        if (hit.valid()) world.remove(hit);
                        if (hit == grab) grab = BodyId{};
                        break;
                    }
                    case SDLK_r:
                        build_scene(world);
                        blood.clear();
                        wounds_seen = 0;
                        last_emitted_ml = -1.0;
                        blood_debt_ml = 0.0f;
                        break;
                    case SDLK_F5: {
                        std::vector<uint8_t> blob;
                        world.save_scene(blob);
                        std::ofstream out("scene.ppb", std::ios::binary);
                        out.write(reinterpret_cast<const char*>(blob.data()),
                                  static_cast<std::streamsize>(blob.size()));
                        std::printf("saved scene.ppb (%zu bytes)\n", blob.size());
                        break;
                    }
                    case SDLK_F6: {
                        if (!recording) {
                            record_scene.clear();
                            world.save_scene(record_scene);
                            record_frames.clear();
                            recording = true;
                            std::printf("recording started\n");
                        } else {
                            std::vector<uint8_t> blob;
                            write_u32(blob, 0x50525050u);  // PPRP
                            write_u32(blob, 1u);
                            write_u32(blob, static_cast<uint32_t>(record_scene.size()));
                            blob.insert(blob.end(), record_scene.begin(), record_scene.end());
                            write_u32(blob, static_cast<uint32_t>(record_frames.size()));
                            for (const ReplayFrame& f : record_frames) {
                                write_u32(blob, f.grab.index);
                                write_u32(blob, f.grab.generation);
                                write_f32(blob, f.target.x);
                                write_f32(blob, f.target.y);
                                blob.push_back(f.active ? 1u : 0u);
                            }
                            const uint64_t final_hash = world.state_hash();
                            write_u32(blob, static_cast<uint32_t>(final_hash));
                            write_u32(blob, static_cast<uint32_t>(final_hash >> 32));
                            std::ofstream out("replay.ppr", std::ios::binary);
                            out.write(reinterpret_cast<const char*>(blob.data()),
                                      static_cast<std::streamsize>(blob.size()));
                            recording = false;
                            std::printf("replay written (%zu frames)\n",
                                        record_frames.size());
                        }
                        break;
                    }
                    case SDLK_F7: {
                        std::ifstream in_file("replay.ppr", std::ios::binary);
                        std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in_file)),
                                                  std::istreambuf_iterator<char>());
                        bool ok = !blob.empty();
                        std::size_t off = 0;
                        if (ok) ok = read_u32(blob, off, ok) == 0x50525050u;
                        if (ok) ok = read_u32(blob, off, ok) == 1u;
                        const uint32_t scene_size = ok ? read_u32(blob, off, ok) : 0;
                        if (ok) ok = off + scene_size + 4 <= blob.size();
                        std::vector<uint8_t> scene;
                        if (ok) {
                            scene.assign(blob.begin() + static_cast<std::ptrdiff_t>(off),
                                         blob.begin() + static_cast<std::ptrdiff_t>(off + scene_size));
                            off += scene_size;
                        }
                        const uint32_t frame_count = ok ? read_u32(blob, off, ok) : 0;
                        std::vector<ReplayFrame> frames;
                        if (ok) frames.reserve(frame_count);
                        for (uint32_t i = 0; ok && i < frame_count; ++i) {
                            ReplayFrame f;
                            f.grab.index = read_u32(blob, off, ok);
                            f.grab.generation = read_u32(blob, off, ok);
                            f.target.x = read_f32(blob, off, ok);
                            f.target.y = read_f32(blob, off, ok);
                            if (ok && off < blob.size()) {
                                f.active = blob[off++] != 0;
                            } else {
                                ok = false;
                            }
                            frames.push_back(f);
                        }
                        uint64_t expected = 0;
                        if (ok) {
                            expected = read_u32(blob, off, ok);
                            expected |= static_cast<uint64_t>(read_u32(blob, off, ok)) << 32;
                        }
                        if (!ok || !world.load_scene(scene.data(), scene.size())) {
                            std::printf("replay rejected\n");
                        } else {
                            play_scene = std::move(scene);
                            play_frames = std::move(frames);
                            play_index = 0;
                            play_expected = expected;
                            playing = true;
                            blood.clear();
                            wounds_seen = 0;
                            last_emitted_ml = -1.0;
                            blood_debt_ml = 0.0f;
                            std::printf("replay started (%zu frames)\n", play_frames.size());
                        }
                        break;
                    }
                    case SDLK_F9: {
                        std::ifstream in_file("scene.ppb", std::ios::binary);
                        std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in_file)),
                                                  std::istreambuf_iterator<char>());
                        if (!in_file && blob.empty()) {
                            std::printf("no scene.ppb\n");
                        } else if (world.load_scene(blob.data(), blob.size())) {
                            blood.clear();
                            wounds_seen = 0;
                            last_emitted_ml = -1.0;
                            blood_debt_ml = 0.0f;
                            std::printf("loaded scene.ppb\n");
                        } else {
                            std::printf("scene.ppb rejected\n");
                        }
                        break;
                    }
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
                if (playing && play_index < play_frames.size()) {
                    const ReplayFrame& f = play_frames[play_index++];
                    in.grab = f.grab;
                    in.grab_target = f.target;
                    in.grab_active = f.active;
                } else {
                    in.grab = grab;
                    in.grab_target = mouse_world;
                    in.grab_active = grab.valid();
                }
                if (recording) {
                    record_frames.push_back(ReplayFrame{in.grab, in.grab_target,
                                                        in.grab_active});
                }
                const double step_start = now_s();
                world.step(in);
                if (sim_ms_count < 4096) {
                    sim_ms[sim_ms_count++] = (now_s() - step_start) * 1000.0;
                }
                if (playing && play_index >= play_frames.size()) {
                    const uint64_t actual = world.state_hash();
                    std::printf("replay %s (hash %016llx expected %016llx)\n",
                                actual == play_expected ? "OK" : "DIVERGED",
                                static_cast<unsigned long long>(actual),
                                static_cast<unsigned long long>(play_expected));
                    playing = false;
                }
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
                    if (wb.valid()) {
                        audio.impact(world.get(wb).material, we.energy);
                    }
                }
                wounds_seen = ws.size();

                // Presentation consumes the authoritative blood budget: one
                // droplet per 4 ml actually emitted, never more.
                double emitted = 0.0;
                for (uint32_t g = 0; g < world.group_count(); ++g) {
                    emitted += world.vitals(g).emitted_ml;
                }
                if (last_emitted_ml < 0.0) last_emitted_ml = emitted;
                const float delta_ml = static_cast<float>(emitted - last_emitted_ml);
                last_emitted_ml = emitted;
                if (delta_ml > 0.0f) {
                    blood_debt_ml = std::min(80.0f, blood_debt_ml + delta_ml);
                }
                if (!ws.empty()) {
                    const WoundEvent& src = ws.back();
                    int spawn_budget = 8;
                    while (blood_debt_ml >= 4.0f && spawn_budget-- > 0 &&
                           blood.size() < 4096) {
                        blood_debt_ml -= 4.0f;
                        const float spread = (next_rand() - 0.5f) * 2.4f;
                        Vec2 v = src.dir * (1.0f + next_rand() * 2.5f) +
                                 Vec2{spread, next_rand() * 1.5f};
                        blood.push_back(Blood{src.pos,
                                              v * (1.0f + next_rand() * 2.0f),
                                              1.0f, 0.03f + next_rand() * 0.05f});
                    }
                }
                accumulator -= static_cast<double>(World::kDt);
                ++steps;
            }
            if (steps == 8) accumulator = 0.0;
        }

        {
            const float bdt = static_cast<float>(std::min(frame_dt, 0.05));
            for (auto& b : blood) {
                b.vel.y -= 9.81f * bdt;
                b.pos += b.vel * bdt;
                if (b.pos.y < 0.01f) {
                    b.pos.y = 0.01f;
                    b.vel = {0.0f, 0.0f};
                    b.life -= bdt * 0.35f;
                } else {
                    b.life -= bdt * 0.08f;
                }
                if (b.life < 0.0f) b.life = 0.0f;
            }
            blood.erase(std::remove_if(blood.begin(), blood.end(),
                                       [](const Blood& b) { return b.life <= 0.0f; }),
                        blood.end());
        }

        const double render_start = now_s();
        renderer.begin(width, height, cam.center, cam.view_height);
        world.for_each([&](BodyId id, const Body& b) {
            render::Color color;
            if (b.component == Component::Button) {
                color = b.signal ? render::Color{0.95f, 0.80f, 0.25f, 1.0f}
                                 : render::Color{0.55f, 0.45f, 0.25f, 1.0f};
            } else if (b.component == Component::Thruster) {
                color = b.signal_in ? render::Color{0.30f, 0.95f, 0.95f, 1.0f}
                                    : render::Color{0.22f, 0.50f, 0.55f, 1.0f};
            } else if (!b.dynamic) {
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
        for (uint32_t g = 0; g < world.group_count(); ++g) {
            BodyId representative{};
            world.for_each([&](BodyId id, const Body& b) {
                if (!representative.valid() && b.group == g) representative = id;
            });
            if (!representative.valid()) continue;
            const Vitals& v = world.vitals(g);
            const Body& hb = world.get(representative);
            const Vec2 bar_center{hb.pos.x, hb.pos.y + 0.35f};
            const float frac = clampf(v.blood_ml / 5200.0f, 0.0f, 1.0f);
            renderer.box(bar_center, Vec2{0.30f, 0.025f}, 0.0f,
                         render::Color{0.10f, 0.10f, 0.12f, 0.85f});
            render::Color fill = v.dead ? render::Color{0.45f, 0.45f, 0.45f, 0.95f}
                                        : render::Color{0.75f - 0.45f * frac, 0.25f + 0.55f * frac,
                                                        0.20f, 0.95f};
            renderer.box(Vec2{bar_center.x - 0.30f * (1.0f - frac), bar_center.y},
                         Vec2{0.30f * frac, 0.018f}, 0.0f, fill);
        }
        {
            const float t = static_cast<float>(SDL_GetTicks()) * 0.01f;
            world.for_each([&](BodyId, const Body& b) {
                if (!b.ignited) return;
                for (int k = 0; k < 3; ++k) {
                    const float phase = t + static_cast<float>(k) * 2.1f;
                    const sim::Vec2 off{std::sin(phase) * 0.05f + (static_cast<float>(k) - 1.0f) * 0.05f,
                                        b.half.y * 0.6f + std::fabs(std::cos(phase)) * 0.10f};
                    renderer.circle(b.pos + off, 0.05f + 0.01f * static_cast<float>(k),
                                    render::Color{1.0f, 0.45f + 0.15f * static_cast<float>(k),
                                                  0.08f, 0.9f});
                }
            });
        }
        for (const Blood& b : blood) {
            renderer.circle(b.pos, b.radius, render::Color{0.62f, 0.05f, 0.08f, b.life});
        }
        for (const Wire& wire : world.wires()) {
            const BodyId a = world.id_at(wire.from);
            const BodyId b = world.id_at(wire.to);
            if (!a.valid() || !b.valid()) continue;
            const bool live = world.get(a).signal;
            renderer.line(world.get(a).pos, world.get(b).pos, 0.015f,
                          live ? render::Color{0.95f, 0.85f, 0.30f, 1.0f}
                               : render::Color{0.45f, 0.42f, 0.35f, 1.0f});
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
        {
            const float half_h = cam.view_height * 0.5f;
            const float half_w = half_h * static_cast<float>(width) / static_cast<float>(height);
            const sim::Vec2 tl{cam.center.x - half_w + 0.25f,
                               cam.center.y + half_h - 0.25f};
            const char* tool_name = "NONE";
            switch (tool) {
                case Tool::Weld: tool_name = "WELD"; break;
                case Tool::Cut: tool_name = "CUT"; break;
                case Tool::Tourniquet: tool_name = "TOURNIQUET"; break;
                case Tool::Blade: tool_name = "BLADE"; break;
                case Tool::Motor: tool_name = "MOTOR"; break;
                case Tool::Wire: tool_name = "WIRE"; break;
                case Tool::None: break;
            }
            char buf[256];
            std::snprintf(buf, sizeof buf, "PP SANDBOX  TOOL: %s%s",
                          tool_name, paused ? "  [PAUSED]" : "");
            renderer.text(tl, 0.035f, render::Color{0.85f, 0.88f, 0.95f, 1.0f}, buf);

            std::snprintf(buf, sizeof buf, "AMMO: %s  BODIES: %u  WOUNDS: %zu",
                          ammo_spec(ammo_index).name, world.body_count(),
                          world.wounds().size());
            renderer.text(sim::Vec2{tl.x, tl.y - 0.28f}, 0.035f,
                          render::Color{0.75f, 0.80f, 0.88f, 1.0f}, buf);

            double blood_min = -1.0;
            int dead = 0;
            for (uint32_t g = 0; g < world.group_count(); ++g) {
                const Vitals& v = world.vitals(g);
                if (blood_min < 0.0 || v.blood_ml < blood_min) blood_min = v.blood_ml;
                dead += v.dead ? 1 : 0;
            }
            if (world.group_count() > 0) {
                std::snprintf(buf, sizeof buf, "BLOOD: %.0f ML  DEAD: %d", blood_min, dead);
            } else {
                std::snprintf(buf, sizeof buf, "BLOOD: --  DEAD: 0");
            }
            renderer.text(sim::Vec2{tl.x, tl.y - 0.56f}, 0.035f,
                          render::Color{0.90f, 0.55f, 0.55f, 1.0f}, buf);

            renderer.text(sim::Vec2{tl.x, tl.y - 0.84f}, 0.03f,
                          render::Color{0.55f, 0.58f, 0.65f, 1.0f},
                          "LMB GRAB  F FIRE  1-3 AMMO  4-8 TREAT  9/0 ARMOUR  H RAG  L CLOTH  P BATTERY");
            renderer.text(sim::Vec2{tl.x, tl.y - 1.12f}, 0.03f,
                          render::Color{0.55f, 0.58f, 0.65f, 1.0f},
                          "` CONSOLE  MMB PAN  WHEEL ZOOM  F5/F9 SAVE/LOAD  F6/F7 REC/REPLAY");
            if (console_open) {
                char input_buf[192];
                std::snprintf(input_buf, sizeof input_buf, "> %s_",
                              console_text.c_str());
                renderer.text(sim::Vec2{tl.x, tl.y - 1.55f}, 0.032f,
                              render::Color{1.0f, 0.85f, 0.35f, 1.0f}, input_buf);
                char shown[112];
                std::snprintf(shown, sizeof shown, "%.96s", console_out.c_str());
                renderer.text(sim::Vec2{tl.x, tl.y - 1.86f}, 0.028f,
                              render::Color{0.75f, 0.78f, 0.85f, 1.0f}, shown);
            }
        }
        renderer.end();
        SDL_GL_SwapWindow(window);
        if (render_ms_count < 4096) {
            render_ms[render_ms_count++] = (now_s() - render_start) * 1000.0;
        }
        audio.update();

        ++frame;
        if (bench_frames > 0 && frame >= bench_frames) running = false;
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

    if (bench_frames > 0) {
        const Stats sim_stats = stats_of(sim_ms, sim_ms_count);
        const Stats render_stats = stats_of(render_ms, render_ms_count);
        std::printf("BENCH frames=%d bodies=%u contacts=%u sim(ms) avg=%.3f p50=%.3f p99=%.3f max=%.3f "
                    "render(ms) avg=%.3f p50=%.3f p99=%.3f max=%.3f\n",
                    frame, world.body_count(), world.contact_count(),
                    sim_stats.avg, sim_stats.p50, sim_stats.p99, sim_stats.max,
                    render_stats.avg, render_stats.p50, render_stats.p99, render_stats.max);
    }

    audio.shutdown();
    renderer.shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
