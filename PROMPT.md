# Implementation prompt — deterministic 2D trauma/ballistics sandbox (C++20; Windows 10/11 + Linux)

**This document is the complete specification.** Hand it to an engineer or a coding agent and the result should be a working, tested, distributable game. Do not replace it with a design document, a prototype, or a vertical slice of stubs.

| Item | Value |
|---|---|
| Product | 2D ragdoll sandbox in the lineage of *People Playground* |
| Differentiator | Simulation is authoritative; every pixel is a readout of state |
| Language | C++20 |
| Platforms | **Windows 10/11 x64 (primary)** and Linux x86_64 |
| Stack | SDL2 ≥ 2.28, OpenGL 3.3 core, CMake ≥ 3.25, vcpkg (Windows) / distro SDL2 (Linux) |
| Deliverable | Game binary, source, tests, CI, Windows runtime bundle, README, decision/physics logs |
| Non-goal | An engine, a scripting layer, or a networked game |

---

## 0. Mission and invariants

The game is a 2D ragdoll sandbox with pause-time construction, contraptions, and violence. It must feel like *People Playground* to play and like a physics/physiology instrument under the hood. If a player cannot predict the outcome of an action from first principles, the model is wrong.

Non-negotiable invariants — each is reviewable and, where possible, tested:

1. **Nothing is scripted.** Damage, dismemberment, death, fracture, fire spread, and signal logic are computed from state and geometry. `if (hit_head) die()` is a defect.
2. **Conservation.** Blood is conserved (`blood + emitted == initial`), momentum is conserved by the solver (damping apart), and tissue damage is monotonic. Tests assert bounds.
3. **SI internally.** m, kg, s, J, N, Pa, °C. Pixels exist only in rendering (default 100 px/m). Every constant has units in `docs/PHYSICS.md`.
4. **Determinism.** Same build + same scene + same input log ⇒ identical state hash, independent of thread count. Replay divergence is a P0 bug.
5. **No fake weight.** No cosmetic knockback, no health-bar damage, no hit-scan bullets, no canned gore that contradicts the model.
6. **Clinical presentation.** Anatomical and desaturated; gore/accent settings may change palette, decals, and audio only — never simulation.
7. **No placeholders.** If a mechanism is absent, it is absent from the UI. No `TODO`, stub, mock, no-op, or “v1” shipped as done.
8. **Fictional subjects only.** Humanoids are abstract; no real-person likeness, no real-world targeting content.
9. **Portable by construction.** No POSIX-only API in `sim/`; no hardcoded paths; Windows and Linux build from the same CMake project and CI matrix.

---

## 1. Definition of done (observable)

A player on Windows or Linux:

1. Builds a structure, drops a ragdoll, and fires a rifle at it.
2. Sees the projectile travel (not hitscan), slow under quadratic drag, and drop under gravity.
3. Sees it stop in steel, perforate flesh, shatter glass, and fragment above a velocity threshold.
4. Sees the hit damage the correct tissue compartments, sever an artery when one is crossed, produce a blood particle spray, and start a hemorrhage whose rate follows the damaged tissue.
5. Applies a tourniquet and observes bleeding stop and blood volume stabilize.
6. Watches a close explosion damage a ragdoll and shove loose bodies; a distant ragdoll is unaffected.
7. Ignites wood, watches fire spread to a neighbouring wooden body but not to steel, and watches burns damage tissue.
8. Wires a button to a thruster, toggles the button, and watches the thruster accelerate the body.
9. Saves the scene, reloads it, and gets the same state hash; records 10+ seconds of input, replays it, and reads `replay OK`.
10. Runs `ctest` on Windows and Linux with **48/48 cases passing**, and the benchmark reports **p99 < 8 ms** for 1,500 bodies with **zero heap allocations during `step()`**.
11. Downloads the CI artifact `pp-windows-x64`, unpacks it, and runs `pp.exe` with no missing DLLs.

---

## 2. Operating protocol (long-horizon work)

Assume context loss and hostile review.

- Maintain `STATE.md`: current milestone, next five tasks, known failures, gate status, last passing commit. Update before every commit and after every failed gate.
- Maintain `docs/DECISIONS.md` (one-line reason per choice) and `docs/PHYSICS.md` (value, units, source or calibration test for every constant). A constant without provenance fails review.
- Commit at least once per work block; never commit a failing gate; never leave the tree unbuildable. One concern per commit.
- Run the gate command before claiming anything. Never claim a result you did not execute; label unrun results `[unverified]`.
- Work test-first for numeric claims: write the failing assertion, then make it pass.
- Prefer deleting a wrong subsystem over patching around it. No compatibility shims, dead code, or parallel conventions.
- Keep a dev console/mode that can spawn, set, measure, profile, and dump state.
- Do not ask which library, which number, or which feature comes first. Decide, document, verify.

---

## 3. Platform and toolchain law

Both platforms are first-class. The same sources build with the same commands modulo SDL acquisition.

### 3.1 Compilers and standards
- C++20 (`/std:c++20`, `-std=c++20`), CMake ≥ 3.25.
- Windows: MSVC 19.3x (Visual Studio 2022 Build Tools). MinGW-w64 is allowed only if CI covers it.
- Linux: GCC 13+ or Clang 16+.
- x64 only.

### 3.2 Dependencies
- SDL2 ≥ 2.28 (window, input, GL context). No SDL3 requirement; SDL3 may replace it only with a recorded decision.
- OpenGL 3.3 core (instanced quads). 4.x features are optional, never required.
- Windows: vcpkg manifest (`vcpkg.json`) with `sdl2`; `find_package(SDL2 CONFIG)`.
- Linux: distro `libsdl2-dev`, or a user-prefix extracted package for the fallback path.
- No other runtime dependency. Tests use a tiny in-repo harness (or doctest) and must run without SDL.
- No TOML/JSON parser is required in v1: material/ammo tables live in compiled C++ tables. A data-file loader is a stretch item.

### 3.3 Windows-specific requirements (these are defects if missed)
1. **Entry point:** `#define SDL_MAIN_HANDLED` before SDL headers, `SDL_SetMainReady()` before `SDL_Init`, and a plain `int main`. Do not link `SDL2main`. Console subsystem is acceptable (keeps logs); document it.
2. **GL ABI:** GL function pointers must use `__stdcall` on `_WIN32`; `GLsizeiptr`/`GLintptr` must be `std::ptrdiff_t` (never `long`, which is 32-bit).
3. **Allocator:** aligned `operator new` must use `_aligned_malloc` and aligned delete must use `_aligned_free` on Windows; `posix_memalign` on POSIX. The allocation counter must keep working.
4. **Headers:** include `<SDL2/SDL.h>` or `<SDL.h>` via a shim using `__has_include`. Define `NOMINMAX` and `WIN32_LEAN_AND_MEAN`.
5. **No POSIX-only calls** in portable code (`dlopen`, `unistd.h`, `pthreads`, `ssize_t`). Link `dl` only on `UNIX AND NOT APPLE`.
6. **CRT:** the CI artifact must include `SDL2.dll` and the VC++ runtime DLLs (`msvcp140`, `vcruntime140`, `vcruntime140_1`, `concrt140`) next to `pp.exe`.
7. **Paths:** save/replay files are relative or under the user profile; never absolute POSIX paths. Binary I/O uses `std::ios::binary`.

### 3.4 Compiler flags
- MSVC: `/W4 /permissive- /fp:precise /Zc:__cplusplus`; no `/fast-math`.
- GCC/Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -ffp-contract=off -fno-fast-math`.
- Do not use `-march=native`; do not enable LTO by default on memory-constrained hosts.

### 3.5 CI and packaging
GitHub Actions, two jobs, green on every push:

- `ubuntu-latest`: apt `libsdl2-dev`; configure; build `-j2`; `ctest`; run the test binary for the full summary; run the benchmark.
- `windows-latest`: configure with the vcpkg toolchain (`-A x64`); build `--config Release`; `ctest -C Release`; run `pp_tests.exe`; run `pp_bench.exe`; stage `dist/` with `pp.exe`, all runtime DLLs, and the VC++ runtime; upload artifact `pp-windows-x64`.

Deliverable commands must be documented in `README.md` for both platforms.

---

## 4. Repository layout and deliverables

```
/CMakeLists.txt          portable build, MSVC + GCC/Clang
/CMakePresets.json       release/debug (sanitizers) presets
/vcpkg.json              sdl2 manifest
/.github/workflows/      linux + windows CI with artifact
/README.md               features, controls, build & run for both OSes
/STATE.md                milestone/status log
/docs/DECISIONS.md       choices with reasons
/docs/PHYSICS.md         constants, units, provenance
/src/sim/                pure deterministic core (no SDL/render/audio)
/src/render/             instanced 2D renderer, GL loader, 3x5 bitmap font
/src/app/                loop, camera, tools, HUD, save/replay
/src/platform/           SDL include shim; platform glue only
/tests/                  headless numeric suite (48 cases)
/tools/                  bench, scene builders
```

`sim/` must compile and test with `-DPP_BUILD_APP=OFF` and no SDL present.

Required artifacts: source tree, green CI, `pp-windows-x64` bundle, README, decision/physics docs, benchmark tool, scene/replay tooling.

---

## 5. Determinism, time, hashing

- Fixed `dt = 1/240 s`, integer tick counter. No wall-clock reads in `sim/`.
- State pooled by `(index, generation)` handles; iteration in index order; contacts sorted by `(body_a, body_b)`; wires by insertion order; events in deterministic key order.
- No `rand()`. Any randomness is a seeded, world-owned generator whose state is saved and hashed.
- FP: contraction off, fast-math forbidden, no `std::pow` in hot loops. Bitwise determinism is required per build and across 1/2/4 worker threads; cross-compiler bitwise equality is not.
- Hash: streaming xxHash64 over canonical fields in declaration order, every 60 ticks and on demand. Include bodies, joints, projectiles, compartments, vitals, heat, components, wires, remaining contact warm-start impulses.
- Scene and replay files embed a version, a payload, and a final state hash. Reject mismatched versions/magic; reject replays whose final hash differs (`DIVERGED`).

---

## 6. Simulation architecture and tick order

One `World::step(const InputFrame&)` with a strict, documented order:

```
1  sample input (live or replay)
2  evaluate wiring/signals; apply thruster forces
3  apply gravity and external/grab forces
4  wake joint-connected islands
5  broadphase (sweep-and-prune or uniform hash; deterministic pair order)
6  narrowphase: circle-circle, circle-box, box-box SAT + clipping; warm-start impulses
7  velocity solve: 10 iterations, sequential impulses, Baumgarte-free
8  integrate positions (semi-implicit Euler)
9  position projection: 3 iterations × 0.8, 5 mm slop (contacts and joints)
10 sweep projectiles (CCD) and resolve impacts, wounds, fragments, fracture
11 heat/fire update
12 physiology update every 4th tick (blood, shock, death)
13 clear forces; sleep bodies; contact cache swap; hash on demand
```

Rules: no allocation in `step()`; no virtual calls, RTTI, exceptions, or `std::function` in the tick; `sim/` has no rendering/audio/window includes; every subsystem is testable headlessly.

---

## 7. Data model (write these headers first)

```cpp
namespace sim {
template <class Tag> struct Handle { uint32_t index, generation; };

enum class Shape : uint8_t { Circle, Box };
enum class MaterialId : uint8_t { Default, Steel, Aluminium, Wood, Glass, Concrete, Rubber, Bone, Flesh };
enum class TissueKind : uint8_t { Skin, Fat, Muscle, Bone, Artery, Vein, Nerve, Brain, Heart, Lung, Liver, Gut };
enum class Component : uint8_t { None, Button, Thruster };
enum class JointType : uint8_t { Distance, Revolute, Weld };

struct Compartment {           // body-local axis-aligned region (v1)
  Vec2 center, half;
  TissueKind kind;
  float damage = 0;            // 0 intact … 1 destroyed
  bool severed = false;        // arteries
};

struct Body {
  Vec2 pos, vel, force; float angle, ang_vel, torque;
  Vec2 half; float radius, mass, inv_mass, inv_inertia;
  float friction, restitution;
  Shape shape; MaterialId material; uint32_t generation;
  bool dynamic, asleep; float sleep_timer;
  float health = 1, bleed_ml_s = 0, vascularity = 0;
  uint32_t group = UINT32_MAX; bool vital = false, tourniquet = false;
  Component component; bool signal = false, signal_in = false; float thrust_force = 0;
  float temperature_c = 20; bool ignited = false; float fuel_s = 0;
  Compartment compartments[6]; uint8_t compartment_count = 0;
};

struct Projectile { Vec2 pos, prev, vel; float mass, radius, drag_k;
                    float fragmentation_speed; uint8_t fragments;
                    uint32_t generation; bool alive; };

struct Vitals { float blood_ml = 5200, emitted_ml = 0, bleed_ml_s = 0;
                bool unconscious = false, dead = false; };

struct Joint { JointType type; uint32_t a, b; Vec2 local_a, local_b;
               float rest_length, ref_angle, break_force, break_torque, yield_torque;
               bool has_limit; float limit_min, limit_max;
               float motor_speed, motor_torque; float force_ema, torque_ema;
               uint32_t generation; bool alive; };

struct Wire { uint32_t from, to; };    // signal graph over body indices
}
```

Copy semantics are value types; serialization writes little-endian fields explicitly (no raw struct dumps).

---

## 8. Physics

### 8.1 Bodies
- Circle: `m = ρπr²`, `I = ½mr²`. Box: `m = 4ρhₓhᵧ`, `I = m(hₓ²+hᵧ²)/3`.
- Static bodies have zero inverse mass/inertia and are excluded from integration.
- Warm-started contact cache keyed by body pair; impulses persisted across frames and serialized.

### 8.2 Solver
- 10 velocity iterations, accumulated normal and Coulomb friction impulses, friction combined as `sqrt(μaμb)`, restitution from the larger body value with a 1 m/s threshold.
- No Baumgarte bias in the velocity solve; penetration is fixed by the position-projection pass. This is deliberate: bias velocity injects energy and destabilizes tall stacks.
- Contact points come from face clipping; store body-local anchors for projection. A zero-length chord is not a contact.
- Sleep: linear < 0.03 m/s, angular < 0.15 rad/s for 0.5 s. A nonzero external force wakes the body; joints propagate wake across connected islands.

### 8.3 Joints
- Distance, revolute (anchor coincidence), weld (anchor + relative angle with `ref_angle`).
- Optional angle limits; motors drive relative angular velocity with a torque cap (`impulse ≤ torque·dt`).
- Structural behaviour: weld joints carry a yield torque `σy·(d·h²/6)` and an ultimate torque `σu·(d·h²/6)` with a 5 cm 2D depth assumption. Above yield the rest angle slips plastically; above ultimate the joint is removed. This is the v1 beam model; document it.
- Break thresholds: distance joints break above a force; welds above ultimate torque.
- Ropes are distance-joint chains with a rated tension; cutting/breaking must let the load fall.

---

## 9. Humanoid anatomy (v1)

- 13 rigid parts: head, torso, pelvis, 2 upper arms, 2 forearms, 2 thighs, 2 shins, 2 feet. Revolute joints with limits at neck, shoulders, elbows, waist, hips, knees, ankles.
- Density 160 kg/m³ for the 2D parts gives roughly human masses; every part shares one vitals group.
- Compartments per part (body-local):
  - all: muscle core at 0.8× half extents;
  - head: brain at 0.55×, centred slightly forward;
  - torso: heart and two lungs;
  - pelvis: gut;
  - limbs: bone core plus a medial artery strip;
  - feet: bone.
- Vitality: the head is vital. Vascularity coefficients (ml/s at full damage): head 60, torso 50, pelvis 30, arms 12/10, thighs 15, shins 10, feet 5.
- Pulling cannot dismember. A blade must destroy structure; a projectile must damage compartments.

---

## 10. Physiology and blood

- Blood volume 5,200 ml per group. Bleeding is summed across member bodies each physiology tick (60 Hz).
- Per-tissue damage weights: brain 3.0, heart 2.5, liver 1.5, lung 1.2, artery 1.5, gut 0.8, bone 0.8, muscle 0.4, skin 0.2, vein 0.6, nerve 1.0, fat 0.1.
- Per-tissue hemorrhage (ml/s, scaled by damage; severed arteries use the full rate): artery 14, vein 4, liver 2, heart 2, lung 1.5, gut 1, muscle 0.5, skin 0.2, bone 0.05, brain 0.
- Vital death when a vital body’s health ≤ 0.2; a head hit that destroys the brain must kill within one update.
- Systemic: unconscious below 2,500 ml, dead below 900 ml. `blood + emitted` must equal 5,200 within 0.5 ml.
- Tourniquet: zeroes a body’s bleed and blocks new bleeding from that body; it does not heal.
- Treatment items: tourniquet, IV fluids, blood bag, defibrillator, chest seal, splint. A transfusion plus defibrillation must reverse hemorrhagic arrest; a splint halves limb bleeding; a chest seal halves hypoxia from lung damage.

---

## 11. Wounds, blades, burns

- **Projectile channel:** transform the entry/exit segment into body-local space, clip it against each compartment AABB, and take the inside length. Damage estimate:
  `d = clamp(inside / (2·max(half)) × energy_factor × 1.2, 0, 1)`,
  with `energy_factor = clamp(penetration_depth / 0.20, 0, 1)`.
  Accumulate `comp.damage += 0.7·d`, clamped to 1.
- Body health = `1 − max(comp.damage × tissue_weight)`. Arteries with damage > 0.2 become severed.
- **Blade stroke:** a 16 cm body-local segment at the cursor, `energy_factor = clamp(E/300, 0, 1)`. When bone or muscle damage exceeds 0.6, remove the nearest joint within 0.45 m → dismemberment. No scripted limb loss.
- **Burns:** burning bodies take `burn_damage_rate` health/s, accumulate skin/muscle damage, and add fluid loss `0.02 × vascularity` ml/s.
- **Gore presentation:** particles spawn from wound events in the app layer only; simulation blood is the scalar budget. No particle may imply blood that the budget did not remove.

---

## 12. Projectiles and ballistics

- Projectiles are integrated with `dv/dt = −k·v·|v| + g` and swept-segment CCD; no hitscan. A projectile must not tunnel a 1 mm plate at 1,000 m/s.
- Ammunition classes (mass g, radius mm, v₀ m/s, drag k 1/m, fragmentation threshold, fragment count):

| Class | m | r | v₀ | k | Frag |
|---|---|---|---|---|---|
| 9×19 FMJ | 8.0 | 4.5 | 350 | 0.0015 | — |
| 5.56 M855 | 4.0 | 2.85 | 940 | 0.0009 | ≥800 m/s, 4 |
| 7.62 M80 | 9.5 | 3.9 | 840 | 0.0008 | — |
| 12 ga slug | 28.0 | 9.0 | 400 | 0.0008 | — |
| 12 ga 00 | 3.5 | 4.1 | 400 | 0.0018 | — |
| Arrow | 25.0 | 4.0 | 70 | 0.0040 | — |
| Crossbow | 30.0 | 4.5 | 110 | 0.0030 | — |

- **Penetration:** `depth = E / (R_pen · A)` with `A = πr²`. Pass-through when `depth > path`; exit speed from the remaining energy after `R_pen·A·path` loss; otherwise stop and deposit all.
- **Fragments:** above the class threshold spawn up to four sub-projectiles at fixed angular offsets (±0.22, ±0.38 rad), 55% of impact speed, equal mass. Queue spawns and append after the iteration.
- **Impulse:** `m·Δv·fraction` along the path, applied at the hit point (angular effects included).
- **Calibration:** 9 mm at 350 m/s must penetrate 0.64 m of flesh-resistance material (12 MPa) within ±15%; a 0.6 m block is perforated, 0.7 m is not; steel (800 MPa) stops it.
- **Also required:** barrel-length muzzle-velocity curve, sight zeroing at a configured range, yaw growth in tissue with a temporary-cavity radius derived from deposited energy, armour that converts a stopped round into blunt trauma, and a distance-delayed supersonic crack (audio).

---

## 13. Materials, fracture, explosions

Material fields: density, friction, restitution, Young’s modulus, yield/ultimate stress, toughness, penetration resistance, fracture energy, ignition temperature, burn damage rate.

| Material | ρ | μ | e | R_pen (MPa) | Fracture (J) | Ignite °C | Burn /s |
|---|---|---|---|---|---|---|---|
| steel | 7850 | 0.60 | 0.05 | 800 | ∞ | — | — |
| aluminium | 2700 | 0.55 | 0.05 | 450 | ∞ | — | — |
| oak | 700 | 0.55 | 0.05 | 40 | ∞ | 300 | 0.02 |
| glass | 2500 | 0.40 | 0.02 | 12 | 25 | — | — |
| concrete | 2400 | 0.70 | 0.02 | 120 | 400 | — | — |
| rubber | 1100 | 0.85 | 0.60 | 15 | ∞ | 350 | 0.05 |
| bone | 1900 | 0.60 | 0.05 | 60 | ∞ | — | — |
| flesh | 1050 | 0.70 | 0.02 | 12 | ∞ | 350 | 0.10 |

- **Fracture:** when impact energy exceeds the threshold, remove the body’s joints, delete the body, and spawn four shards at 0.42 scale with inherited velocity, radial kick, and spin. Deterministic.
- **Explosions:** `blast = E / (4π(d + 0.3)²)`, impulse = blast scaled by inverse mass, wound segment through the body with `energy_factor = clamp(blast/500, 0, 1)`. An 8 kJ charge at 4 m must kill a close ragdoll, spare a distant one, and shove a loose crate.

---

## 14. Heat and fire

- Ambient 20 °C; non-burning bodies cool at 0.4/s.
- Ignition: wood 300 °C, rubber/flesh 350 °C; metals, glass, concrete, bone inert.
- Burning holds 600 °C for 12 s, applies the material burn rate, and raises flammable neighbours within 0.35 m by 250 °C/s.
- A burning head must kill its vitals group within 12 s; steel next to burning wood must never ignite.

---

## 15. Wiring, motors, buttons, thrusters

- Components are bodies: button (toggleable signal) and thruster (force along local +X when powered).
- Wires connect output → input as `(from, to)` body indices, evaluated before forces each tick: any powered source sets `signal_in` on its target.
- Thruster default force 3,000 N; powered thrusters wake their body.
- Motors are revolute joints with `motor_speed` and `motor_torque`; a 2 rad/s target with sufficient torque must reach the target within 1 s.
- Wires are serialized, hashed, and removed when either endpoint is deleted. No dangling handles.

---

## 16. Persistence and replay

- **Scene file:** magic `PPSC`, version, tick, gravity, body slots (alive flag, generation, full state, compartments), joints (all fields), vitals groups, wires, projectiles, and the contact warm-start cache. Rebuild free lists on load. Reject bad magic/version/bounds without mutating the world.
- **Replay file:** magic `PPRP`, version, scene blob, input frames `(grab handle, target, active)`, final hash. Replay applies frames from the saved scene and must reproduce the final hash.
- **Tests:** scene round-trip preserves the hash immediately and after 240 further ticks; a 600-frame input replay is bit-identical; corrupt/truncated files are rejected.

---

## 17. Rendering, HUD, input

- Batched instanced quads, one draw call per texture class; circles via a distance shader, boxes via quads; antialiased edges; no per-body draw calls.
- Debug-grade but clear: static grey, dynamic blue, sleeping dim, blood dark red, fire orange, wires yellow when live, buttons amber, thrusters cyan.
- 3×5 bitmap font rendered through the same instance batch; HUD shows tool, ammo, body/wound counts, blood, dead count. No per-frame allocations (use `string_view`).
- Blood particles and flames are presentation-only; they never feed state.
- Camera pan/zoom; fixed 240 Hz simulation with accumulator capped at 8 steps/frame; nearest/linear filter according to palette.

Controls (documented in README):

| Input | Action |
|---|---|
| LMB | grab / active tool |
| RMB | spawn circle |
| 1/2/3 | select 9×19 / 5.56 / 7.62 |
| F | fire toward cursor |
| H / O | spawn ragdoll / rope |
| B / K | spawn box / glass pane |
| G / C / T / V / M / Z | weld / cut / tourniquet / blade / motor / wire tools |
| N / Y | spawn button / thruster |
| MMB drag / wheel | pan / zoom the camera |
| `` ` `` | dev console: spawn, set gravity/threads, pause, measure, profile, dump, hash |
| I | ignite body under cursor |
| E | 8 kJ explosion at cursor |
| X | delete body |
| Space / R | pause / reset |
| F5 / F9 | save / load `scene.ppb` |
| F6 / F7 | record / replay `replay.ppr` (prints OK/DIVERGED) |
| Esc | quit |

---

## 18. Testing and acceptance matrix

Headless, deterministic, CI-gated; the suite must contain at least the 48 behaviours below (grouped). Tolerances are part of the contract.

**Core physics (9):** free-fall displacement; circle at rest on a floor; five-box stack; six-box tower for 30 s at a non-zero x (clipping-plane regression); momentum conservation without damping; determinism across worlds over 3,000 ticks at 1/2/4 broadphase workers; zero allocations in `step()` after warm-up; stale handles rejected; picking returns the topmost body.

**Performance (1):** 500 bodies sustain the 60 fps step budget.

**Joints and construction (6):** distance joint length; revolute anchor coincidence; weld relative angle; breakable joint fails above rated force; weld plastic hinge — no slip at 0.9× predicted moment, slip at 1.1×; motor reaches 2 rad/s in 1 s.

**Ragdoll and rope (3):** ragdoll collapses and stays assembled (anchor gaps < 5 cm); rope holds a load; rope breaks above rated tension.

**Ballistics (10):** gravity/drag; steel stops vs flesh perforates; penetration depth ±15% (0.6 m passes, 0.7 m stops); headshot kills and a thigh shot bleeds without immediate death; ammunition ordering; 5.56 fragments vs 9 mm does not; muzzle velocity rises with barrel length and saturates; zeroing raises the bore with range; 5.56 yaws and cavitates more than 9 mm; armour stops rounds and yields blunt trauma.

**Trauma and treatments (7):** severed artery bleeds ≥5 ml/s vs muscle-only <2; tourniquet stabilizes blood; repeated blade strokes sever a limb at a joint; `blood + emitted == 5200` within 0.5 ml; transfusion + defibrillation reverse hemorrhagic arrest, untreated arrest becomes brain death; chest seal halves hypoxia; splint halves limb bleeding and IV restores volume.

**Materials (1):** glass yields exactly 4 shards (body count +3), steel unchanged.

**Explosions (1):** close ragdoll dead, far ragdoll alive, crate impulse > 0.5 m/s at detonation.

**Heat (2):** fire spreads wood→wood but not to steel; burning kills within 12 s.

**Wiring (1):** unwired thruster idle, wired button accelerates it, stop removes acceleration, scene round-trip preserves the wire and hash.

**Electricity (1):** a wired 220 V battery fibrillates a humanoid; an unwired battery does not.

**Soft bodies (2):** cloth drapes from its pinned row and stays jointed under gravity; a soft blob deforms under a hit without losing constraints.

**Audio (1):** impact timbre ordering by material and distance-delayed crack maths are deterministic and sane.

**Persistence and replay (2):** scene round-trip deterministic over 240 ticks (corrupt/truncated blobs rejected); 600-frame replay bit-identical.

**Robustness (1):** a 900 m/s projectile still registers a hit on a 1 mm plate (no tunneling); no NaN/inf after any test.

Commands:

```sh
ctest --test-dir build --output-on-failure
./build/pp_tests            # or build/Release/pp_tests.exe
```

Do not weaken tolerances to pass. If a target cannot be met, record the evidence and the smallest scope cut in `DECISIONS.md`.

---

## 19. Performance budget

- Release build, 4 cores, 1 thread: 1,500 bodies + projectiles ⇒ **p99 sim step < 8 ms**, average < 3 ms on a modern x86-64.
- **Zero heap allocations during `step()`** measured by a global allocation counter; reserve all pools and scratch up front.
- Render p99 < 8 ms with llvmpipe-class software GL at 1280×720.
- Benchmark tool prints bodies, contacts, avg, p50, p99, max.

Reference measurements (llvmpipe, 4 cores): 1,500 bodies ≈ 1.2–1.5 ms avg, p99 ≈ 2.2 ms.

---

## 20. Milestones and gates

| # | Scope | Gate |
|---|---|---|
| M0 | CMake (both OSes), presets, fixed-step loop, renderer, rigid bodies, grab, dev console, harness | 500 bodies ≥60 fps; allocation counter 0; CI green on Windows+Linux |
| M1 | joints, ragdoll, rope, materials, plastic welds, weld/cut | beam yield test; rope rating; ragdoll assembled; goldens stable |
| M2 | compartments, wound channel, hemorrhage, blood budget, vitals, tourniquet, HUD | headshot death; artery vs muscle; conservation; tourniquet |
| M3 | projectiles, ammo classes, penetration, fragmentation, steel/flesh, fracture | penetration ±15%; 5.56 fragments; glass shards |
| M4 | blade severance, explosions, heat/fire, blood/flame feedback | severance test; blast test; fire tests |
| M5 | wiring/motors/thrusters, scene save/load, replay recorder | wiring test; round-trip hashes; `replay OK` |
| M6 | packaging, README, Windows artifact, bench, polish | `pp-windows-x64` runs with bundled DLLs; p99 < 8 ms; CI green on both OSes |
| M7 | ballistics depth (interior curve, yaw/cavity, armour, zeroing) and treatment items | four ballistics tests and three treatment tests green |
| M8 | electricity, procedural audio, cloth and soft blobs | fibrillation, timbre, cloth and blob tests green |
| M9 | final integration, docs, cross-platform artifact | 48/48 on Windows and Linux; prompt/docs match the shipped feature set |

Do not start a milestone early. Do not leave one half-done.

---

## 21. Review rubric and anti-patterns

Before each commit answer: (1) which state produces this visual; (2) what conserves and is asserted; (3) which test fails if I delete this; (4) where do the constants come from; (5) does the tick allocate/virtualize; (6) is the sim still headless; (7) does it work with one thread; (8) is any convention duplicated.

Rejected patterns:

```cpp
if (hit.head && rng() < 0.9f) kill();         // scripted outcome
raycast_and_damage(muzzle, dir);              // hitscan
victim.apply_impulse(dir * 500.0f);           // cosmetic force
health -= 10;                                 // health integers
if (penetration < 5) damage = 10;             // magic unitless numbers
```

Also rejected: per-frame allocation, `std::function` in the tick, physics tuned for looks, tests that assert source text, dead code “for later”, tolerances widened to pass, DLLs assumed present on the user’s machine.

---

## 22. Risks, stop conditions, out of scope

| Risk | Mitigation |
|---|---|
| Determinism vs parallelism | fixed merge order, no atomics in state, 1/2/4-thread hash test |
| Parameter sprawl | provenance in `PHYSICS.md`; calibration tests |
| Scope explosion | depth-first per milestone; no stretch item before a green gate |
| Soft-body cost | fixed budgets and LOD if soft bodies ever land |
| Windows linker/ABI traps | the §3.3 list; CI from day one, not at the end |
| Build OOM | `-j2`; no default LTO |

**Stop and report only if:** a gate is unreachable for a structural reason you cannot resolve; a required constant has no source and no calibration target; two requirements are provably contradictory. Otherwise decide, document, continue.

**Out of scope for v1 (must be listed as such in README):** continuous FEM fracture (the shipped model is plastic hinges plus quad shards); networking; mod support; an engine or scripting layer. Mass-spring cloth/soft blobs, electricity, and procedural audio are in scope as of M7–M9. Adding any out-of-scope item is a new milestone with its own tests.

---

## 23. First actions

1. In one commit: `CMakeLists.txt` (portable), `CMakePresets.json`, `vcpkg.json`, CI matrix, `STATE.md`, `docs/DECISIONS.md`, `docs/PHYSICS.md`, source skeleton, failing test harness.
2. Implement M0 end to end; prove it on both OSes in CI.
3. Then M1 → M6 in order, never skipping a gate.

Do not ask for permission, library preferences, or content targets. Build, measure, verify.

---

## Appendix A — Reference implementation (non-normative)

A working implementation of this prompt exists at <https://github.com/DudeChill/pp-sandbox> (C++20, SDL2, OpenGL 3.3, MSVC/vcpkg + Linux, 48 tests, Windows artifact in CI). Treat it as one acceptable implementation, not a constraint: any implementation that satisfies the gates above is valid. Its `STATE.md`, `docs/DECISIONS.md`, and `docs/PHYSICS.md` show the expected level of provenance.
