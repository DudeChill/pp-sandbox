# Build a 2D physics sandbox (People Playground lineage) in C++ with a simulation-grade trauma, ballistics, and materials model

You are the sole engineer on this project. Deliver the game, not a design document. Work continuously: do not stop to ask which library, which number, or which feature comes first — decide, write the decision down, and verify it. The only acceptable reasons to stop are the stop conditions in §20.

---

## 0. Mission and invariants

The game is a 2D ragdoll sandbox with pause-time construction, contraptions, and violence. Reference: *People Playground*. Differentiator: **the simulation is authoritative; every pixel is a readout of state.** If a player cannot predict the result of an action from the model, the model is wrong.

Non-negotiable invariants, each enforceable in review:

1. **Nothing is scripted.** Damage, dismemberment, death, fracture, ricochet, and fire spread are computed from state and geometry. `if (hit_head) die()` is a defect.
2. **Conservation.** Momentum, energy (with declared losses), blood volume, and tissue mass are conserved. Tests assert numerical bounds.
3. **SI internally.** m, kg, s, J, N, Pa, K. Pixels live only in `render/` (default 100 px/m). Every formula is dimensionally checked.
4. **Determinism.** Same scene + input log ⇒ identical state hash for a given build, independent of thread count. Replay divergence is a P0 bug.
5. **No fake weight.** Cosmetic knockback, health bars, hit-scan bullets, and canned gore that contradicts the model are defects.
6. **Clinical presentation.** Anatomical, desaturated, cross-sectional. The gore setting changes palette/decals/audio only, never simulation.
7. **No placeholders.** If a mechanism is not implemented it is absent from the UI. No `TODO`, stub, mock, no-op, or "v1".
8. **Fictional subjects only.** Humanoids are abstract; no real-person likeness, no real-world targeting content.

---

## 1. Definition of done

A player builds a bridge, wires a motor, spawns a humanoid 150 m away, and fires a rifle. They observe: the projectile drops and slows per drag, yaws after impact, fragments above the construction-dependent velocity threshold, severs the correct vessel, produces a wound channel whose cavity matches the deposited energy, and drives the victim through tachycardia → narrowed pulse pressure → unconsciousness → death on a literature-plausible timescale. A tourniquet applied early changes the outcome. Structural steel bends and stays bent; glass shatters with a plausible shard distribution. The same scene replayed from its input log reproduces a bit-identical state hash. Every gate in §18 passes and the demo scenario runs at the performance budget.

---

## 2. Operating protocol (long-horizon work)

You will work across many sessions; assume context loss.

- Maintain `STATE.md` at repo root: current milestone, next five tasks, known failures, gate status, last passing commit. Update it **before** every commit and **after** every failed gate.
- Maintain `DECISIONS.md` (stack and scope choices, with one-line reasons) and `PHYSICS.md` (every constant: value, units, source or derivation, calibration test). A constant without provenance fails review.
- Commit at least once per work block; never commit with a failing gate; never leave the tree unbuildable. Conventional commits, one concern each.
- Run the gate command before claiming a milestone. Do not claim a result you did not execute; label anything unrun `[unverified]`.
- Prefer deleting a wrong subsystem over patching around it. No compatibility shims, no dead code, no parallel conventions.
- Work test-first for every numeric claim: write the failing assertion, then make it pass.
- Keep a hidden-ish `dev` console (spawn, set, measure, profile, dump-state) from M0; it multiplies later velocity.

---

## 3. Technical baseline

- **C++20**, CMake ≥ 3.25, GCC/Clang. Linux-first; platform code isolated in `platform/`.
- **SDL3** (window/input/audio), **OpenGL 4.6** batched instanced-quad renderer, custom sequential-impulse physics (Box2D v3 allowed only behind `sim::PhysicsWorld`, swappable, never leaking into anatomy/ballistics/physiology).
- **Deps:** `stb_image`, `doctest` (tests), `toml++` (data). Nothing else without a `DECISIONS.md` entry.
- **Build discipline on 4 cores / ~3 GB free:** `-j2`, `ld.lld` or `mold`, ThinLTO only in Release, never `-march=native`, never `-ffast-math`. Presets: `debug` (ASan+UBSan), `release`, `relwithdebinfo` (Tracy optional).
- **Layout:**

```
/src
  sim/        math, bodies, joints, soft bodies, materials, fracture, fluids,
              projectiles, anatomy, physiology, damage, serialization  (pure, headless)
  render/     renderer, camera, decals, debug draw
  app/        loop, tools, build mode, UI, save/load, replay
  platform/   SDL/GL glue only
/tests        headless numeric harness + golden scenes
/tools        calibrate, replay-verify, scene-dump, fuzz-save
/data         ammo.toml materials.toml weapons.toml anatomy.toml items.toml
/docs         DECISIONS.md PHYSICS.md PROFILING.md
/scenes       demo + golden scenarios
```

- **Performance budget (Release, 4 cores):** 1,500 rigid bodies + 300 projectiles + 8 soft bodies ⇒ **sim step p99 < 8 ms**, render p99 < 8 ms, **zero heap allocations during `world.step()`**. Profile single-threaded first; threading hides cost, never removes it.

---

## 4. Determinism specification

Bitwise reproducibility **per build**, thread-count-invariant. Cross-machine/compiler reproducibility is not required.

- Fixed `dt = 1/240 s`, integer tick counter. No wall-clock in `sim/`.
- Iterate pools by `(index, generation)`; island ordering by minimum body handle; solve islands in parallel, **merge in fixed order** in `sim/`, and merge in fixed handle order; no atomics in persistent state.
- One PCG64 per scenario, seeded; never `rand()`. RNG draws consumed in deterministic order; no RNG in rendering-affecting branches.
- **FP control:** `-ffp-contract=off`, `-fno-fast-math`, no x87 (`-mfpmath=sse`), avoid libm variance by using our own `sim::math` (`sqrt` is IEEE-exact; implement `sincos` approximations with fixed code paths; no `std::pow` in hot loops).
- State hash: xxHash64 over a canonical binary dump (fields in declaration order, no padding). Hash every 60 ticks into the replay log; divergence reports the first differing field.
- Replay file: scene hash + physics-parameters hash + seed + timestamped input log. Refuse to load a replay on parameter mismatch.

---

## 5. Tick pipeline and event ordering

Strict order, single function, documented in `sim/step.cpp`:

```
1  sample input (from log)
2  apply forces: gravity, springs, motors, thrusters, external
3  broadphase
4  narrowphase + manifold build + warm start
5  velocity solve (8 it) + position solve (3 it)
6  integrate semi-implicit Euler
7  CCD sweep for projectiles and bodies > 20 m/s; generate impact events
8  impact/wound events, sorted by (tick, body_handle, projectile_id, seq)
9  structural fracture propagation
10 physiology integration (every 4th tick, fixed phase)
11 fluid/blood advection, temperature, electricity
12 emit render/decal/audio events (never feed state back)
13 sleep islands, update hash
```

Events are queued and processed in a deterministic key order; never rely on container iteration for causality. Damage events carry the full geometric path so downstream systems never re-query physics.

---

## 6. Interfaces (write these headers first)

```cpp
namespace sim {

template <class T> struct Handle { uint32_t index, generation; };  // 0 allocs, safe re-use

struct World;                       // owns pools + scratch arena; Arena reset per tick
void step(World&, const InputFrame&) noexcept;

struct Compartment {                // one per anatomical region, body-local
  TissueKind kind;                  // skin, fat, fascia, muscle, tendon, bone, artery, vein, nerve, organ
  MaterialId material;              // from materials.toml
  Vec2 poly[16]; uint8_t n;         // convex cross-section, body-local metres
  float thickness_m, area_m2, mass_kg, blood_share;
  float damage = 0;                 // 0 intact … 1 destroyed
  uint32_t supply_organ;            // for perfusion coupling
};

struct Projectile {
  BodyId body; float mass_kg, area_m2, drag_cd_mach[8];
  float v0, yaw_rate, frag_v_threshold, deformation_threshold;
  Vec3 orientation;                 // yaw matters after impact
};

struct ImpactEvent {                // sorted deterministic queue
  uint64_t tick; uint32_t seq; BodyId body; ProjectileId proj;
  float path[64][2]; uint8_t path_n; // body-local channel polyline
  float de_dx_jm[64];               // energy deposited per segment
};

struct Vitals {
  float hr_bpm, stroke_ml, co_lpm, svr, map_mmhg, cvp, icp;
  float spo2, etco2_mmhg, temp_c, blood_l, o2_debt_ml;
  float consciousness, shock_index;
  bool apnea, vf, brain_dead;
};

} // namespace sim
```

Rules: `sim/` has no window/render/audio includes; every subsystem takes a `const World&` or explicit inputs; testable without a renderer; no virtuals in the tick; handles validated in debug.

---

## 7. Data schemas (TOML, versioned)

- `ammo.toml`: `id, mass_g, diameter_mm, length_mm, bc_g7, construction, v0_mps, twist_mm, frag_v_threshold_mps, yaw_coeff, deformation_threshold_mps, tracer`.
- `weapons.toml`: `id, ammo, barrel_mm, twist_mm, pressure_curve[], cyclic_rpm, sight_height_mm, zero_m, dispersion_mrad, action, magazine, recoil_impulse_scale`.
- `materials.toml`: `id, density, E, sigma_tensile, sigma_compressive, sigma_shear, toughness, hardness, ductility, friction, restitution, conductivity, specific_heat, melting_point, flammability, anisotropy_x, anisotropy_y`.
- `anatomy.toml`: compartment polygons per body part, material links, blood share, organ/perfusion links.
- `items.toml`: tools, treatment items, construction parts, their costs and mechanisms.

All data files carry `schema_version`. Loader validates ranges; save files embed a data hash.

---

## 8. Anatomy

Humanoid = rigid-body chain (head, neck, thorax, abdomen, pelvis, upper/lower arm, hand, thigh, shin, foot) **plus** a 2D compartment model in body-local space, independent of the render mesh.

Compartment kinds: skin, subcutaneous fat, fascia, muscle, tendon, cortical/cancellous bone, marrow, artery, vein, nerve, and organs (brain, brainstem, heart, lungs + pleural space, liver, spleen, kidneys, gut, bladder, trachea, bronchi, esophagus, eyes, skull, ribs, spine).

Reference adult (75 kg), to be encoded in `anatomy.toml`:

| Quantity | Value |
|---|---|
| blood volume | 70 mL/kg ⇒ ~5.2 L |
| brain / heart / each lung | 1.4 kg / 300 g / 600 g |
| liver / spleen / each kidney | 1.5 kg / 150 g / 150 g |
| skeletal muscle / skin / cortical bone | ~30 kg / ~3.5 kg / ~10 kg |
| aorta / femoral artery / radial artery | r ≈ 12 / 4–5 / 2 mm |
| cortical bone E / σ_t / σ_c / σ_s | 17 GPa / 130 / 190 / 50 MPa |
| tendon σ_t | 50–100 MPa |
| skin σ_t / E | 5–20 MPa / 5–20 MPa |

**Dismemberment** requires cumulative failure of skin, fascia, muscle/tendon, and bone across the section. Pulling cannot separate a joint; a high-energy rifle round or sustained cutting can. Never a scripted limb threshold.

---

## 9. Physiology

Continuous state, integrated at 60 Hz, exposed in a vitals overlay.

- **Hemodynamics.** `CO = HR × SV`; `MAP ≈ CO × SVR` (SVR in mmHg·min/L, 1000–1400 at rest); `CPP = MAP − ICP` (ICP ≈ 10 mmHg). Baroreflex raises HR and SVR with volume loss, saturating; MAP falls once compensation is exhausted. Consciousness = f(CPP, cerebral O₂ delivery, duration).
- **Hemorrhage.** `Q = (π r⁴ ΔP)/(8 μ L) × tamponade × clot`, μ ≈ 3.5 mPa·s. Arterial flow systolic-gated, venous steady, capillary slow. Muscle/fascia tamponade; open arterial wounds do not. Severed vessels retract and constrict.
  - Targets: complete femoral artery transection ⇒ unconscious 30–120 s, death 2–5 min untreated; radial artery ⇒ tens of minutes; liver laceration ⇒ steady, often fatal.
- **Respiration.** RR 12–16, Vt 500 mL, FRC ~2.4 L, VO₂ 250 mL/min, VCO₂ 200 mL/min, Hb 15 g/dL. Hill-curve saturation. Pneumothorax/hemothorax reduce functional lung volume; tension pneumothorax raises intrathoracic pressure, collapses venous return, kills in 3–10 min untreated. Airway obstruction/tracheal transection ⇒ rapid hypoxia.
- **Perfusion/ischemia.** Organ flow ∝ demand × MAP factor; ischemia tolerance: brain 4–6 min, heart ~20 min, kidney 30–60 min, limb 4–6 h under tourniquet.
- **Shock/incapacitation.** Nociceptor drive from tissue damage and nerve hits; catecholamines mask early shock; staged degradation (fine motor → gross motor → collapse). Pain is state, not a health bar.
- **Neurology.** Brainstem destruction ⇒ apnea < 1 s. High spinal transection ⇒ paralysis + autonomic loss. Concussion from angular acceleration ⇒ transient unconsciousness scaled by dose.
- **Arrest/resuscitation.** VF from transthoracic current ≳100 mA (50–60 Hz, path-dependent) or profound hypoxia. CPR gives partial circulation; defibrillation converts VF only with oxygenated myocardium; brain death is terminal. Hypothermia slows metabolism and coagulation; burns destroy the skin barrier and cause fluid loss.
- **Conservation.** Emitted blood = lost blood ± 0.1%; O₂ consumed = demand + debt; no negative volumes. Assert per scenario.

**Treatment items** (mechanism, not a heal button): tourniquet, pressure dressing, chest seal, needle decompression, IV fluid, blood bag, defibrillator, splint, airway adjunct.

---

## 10. Damage and gore

- **Channel.** The projectile/blade path is sampled segment-by-segment against body-local compartment polygons; `dE/dx` depends on density, deformation, and yaw. Permanent cavity = compartments whose strain exceeds failure. Temporary cavity peak radial velocity `u_max ≈ sqrt(2 (dE/dx) / (ρ A))` (m/s), with a damped pulsation `r(t) = r_max e^{−t/τ} sin(2πt/T)`, τ ≈ 1–3 ms; tissue-specific response (lung: large temporary, little permanent; liver/spleen: dense, inelastic, hemorrhagic; bone: comminution + secondary fragments).
- **Vessels/nerves.** Lacerated within the permanent cavity; partial lacerations tamponade; severance ⇒ distal ischemia + permanent nerve deficit or rapid arterial exsanguination.
- **Secondary projectiles.** Bone and jacket/core fragments become real projectiles with mass and velocity.
- **Blood.** Volume-conserving particle/height-field hybrid: emitted at computed flow, gravity-driven, pools on surfaces, stains, clots with injury- and temperature-dependent τ. Fixed-size particle pool; on-screen blood bounded; **emitted volume must equal lost volume**.
- **Presentation.** Anatomical cross-sections, tissue colours from the material table, cavities drawn as cavities, no red particle spam. Gore setting (clinical/standard/minimal) affects visuals and audio only.

---

## 11. Ballistics

- **Interior.** Chamber pressure curve → muzzle velocity vs barrel length (monotonic, saturating). Stability via Miller `Sg = 30 m / (t² d³ l (1 + l²))` (grains, inches/turn, inches, calibres); stable 1.5–2.0. Under-stabilised ⇒ yaw growth in flight.
- **Exterior.** `dv/dt = −(ρ Cd(M) A / 2m) v|v| + g`. `Cd(M)` tables per projectile class (interpolated), transonic drag rise, gravity, optional wind. No hit-scan. Supersonic crack is audio-only, distance-delayed. Drag work must equal the kinetic-energy integral (test).
- **Terminal.** Penetration via Lambert–Jonas `V_r = a (V_s^p − V_50^p)^{1/p}` (fit `a, p, V_50` per projectile/target and validate against the table), or THOR for armour. Yaw after impact grows with presented area; fragmentation above a construction/velocity threshold; each fragment becomes a projectile. Ricochet critical angle from velocity and relative hardness, with computed energy loss/deviation. Armour: fabric spreads load (backface blunt trauma), plates stop and spall.
- **Recoil.** Impulse = projectile momentum + effective propellant momentum (~1.2–1.5 × muzzle velocity × charge mass), applied through the wrist/shoulder chain. Muzzle rise from bore-axis height above COM. Sights zeroed at `zero_m`; drop at range is a real aiming problem. Automatic fire: cyclic rate + seeded dispersion cone + recoil coupling.

Calibration targets (10% ordnance gelatin, ±15%):

| Load | m | v₀ | Penetration | Behaviour |
|---|---|---|---|---|
| 9×19 FMJ | 8 g | 350 m/s | 60–70 cm | minimal yaw, small cavity |
| 5.56 M855 | 4 g | 940 m/s | 35–40 cm | yaws ~10 cm in, fragments ≳800 m/s |
| 7.62 M80 | 9.5 g | 840 m/s | 55–65 cm | through-and-through limbs |
| 12 ga slug | 28 g | 400 m/s | 35–45 cm | massive transfer, bone destruction |
| 12 ga 00 buck | 3.5 g ×9 | 400 m/s | 25–35 cm each | independent pellets |
| Arrow | 25 g | 70 m/s | 20–30 cm | cutting tip, weak temporary cavity |
| Crossbow bolt | 30 g | 110 m/s | 30–40 cm | high sectional density |

---

## 12. Melee

Cutting requires sliding velocity along the edge; force concentrates on the contact line. Penetration work = edge friction + separation energy + elastic deformation. Thrusting: `P = F/A` — point geometry decides. Blade angle, thickness behind the edge, and hardness set efficiency and edge damage against bone. Blunt: impulse × contact area ⇒ contusion, laceration, fracture. Mass, inertia tensor, and balance change swing dynamics; edge-to-edge contact chips or breaks blades by hardness vs toughness. Dismemberment only via cumulative structural failure.

---

## 13. Materials and fracture

All behaviour is data. Mass-spring/FEM-lite lattice: strain beyond yield ⇒ **plastic** deformation that persists; beyond ultimate ⇒ fracture propagated to neighbours; toughness decides whether a crack runs or arrests. Joints (weld/bolt/glue/knot) carry their own strength and fail first. Strain-rate dependence for muscle and skin; anisotropy for wood (≈10× weaker across grain).

| Material | ρ kg/m³ | E | σ_ult | Failure mode |
|---|---|---|---|---|
| steel | 7850 | 200 GPa | 400–550 MPa | yields/bends, then tears |
| aluminium | 2700 | 69 GPa | 200–300 MPa | dents, low stiffness |
| oak / pine | 700/500 | 10/8 GPa | 40–100 MPa | splinters along grain |
| glass | 2500 | 70 GPa | 50 MPa (tension) | flaw-driven crack propagation |
| concrete | 2400 | 30 GPa | 3 / 30 MPa | tension cracks, crushing, spall |
| rubber | 1100 | 10 MPa | 20 MPa | hyperelastic, large strain |
| kevlar fabric | 1440 | 70 GPa (fibre) | 3000 MPa (fibre) | energy spread, no plastic flow |
| bone | 1900 | 17 GPa | 130 MPa (tension) | comminution at point load |
| muscle | 1050 | 10 kPa–1 MPa | 0.1–1 MPa | viscoelastic, rate-dependent |
| skin | 1100 | 5–20 MPa | 5–20 MPa | stretches, then tears |
| tendon | 1120 | 1 GPa | 50–100 MPa | stiff, fails abruptly |

Required behaviours: steel stays bent; glass shards with a plausible size distribution; wood snaps at knots and splinters; concrete crumbles; cloth tears and drapes; rope snaps with recoil; pressurised vessels rupture and vent.

---

## 14. Secondary systems (M5)

- **Heat:** conduction/convection, ignition thresholds, fire spread on flammable materials, steel weakening with temperature, water → steam pressure.
- **Electricity:** node-graph resistance, current path through conductors and bodies, fibrillation threshold, arcing, fuses.
- **Gas/fluid pressure:** hydraulics, pneumatics, rupturing vessels.

Each gets a numeric test (e.g., 2 m of water heats 1 kg steel by the computed ΔT ±10%).

---

## 15. Construction, UI, tools

Pause-time build mode: spawn, weld/bolt/glue, rope/cable, hinge, slider, motor, piston, thruster, wheel, sensor, button, logic gate, timer. Typed wiring graph, topological evaluation, signal-flow debug overlay. Grab via spring constraint preserving momentum. Cut/sever tool. Searchable catalogue. Undo/redo for build actions. Camera pan/zoom/follow. Versioned binary scene save with forward-compatible reader (fuzz-tested).

---

## 16. Rendering and audio

Batched instanced quads; one draw call per material class; decal ring buffer; ragdolls drawn from anatomical geometry so wounds/dismemberment deform correctly. Audio parameterized by material pair, mass, and impact energy; distance-delayed supersonic crack; no canned gore SFX. Colourblind-safe palette; gore and blood settings in options.

---

## 17. Testing and calibration

Headless, deterministic, CI-gated. Required families:

1. **Golden physics scenes:** fixed scene hashes; tolerance 0 (exact) for determinism, documented tolerance for cross-flag builds.
2. **Ballistics table:** every row ±15%; drag-work integral; momentum conservation ±0.5%; ricochet angle; armour backface.
3. **Physiology timing:** femoral transection (unconscious 30–120 s, death 120–300 s); tourniquet within 60 s ⇒ survival > 30 min; tension pneumothorax death 3–10 min; brainstem < 1 s; VF fibrillation threshold.
4. **Tissue thresholds:** 9 mm through thigh vs femur; knife vs rib; blunt skull fracture impulse.
5. **Conservation:** blood ±0.1%, tissue mass, O₂, momentum, energy with declared losses.
6. **Structure:** beam yield stress ±10%; glass shard count/bounds; rope breaking tension; weld failure before bulk.
7. **Robustness:** no tunneling at 1,000 m/s through 1 mm; save-file fuzz target (libFuzzer) with zero crashes; NaN/inf guards.
8. **Performance:** p99 sim < 8 ms; allocation counter == 0 during `step()`; 4-thread hash identical to 1-thread.

**Calibration harness** (`tools/calibrate`): reads the target tables, sweeps parameters, reports residual error, writes the fitted constants to `PHYSICS.md` with the commit hash. CI fails if residuals exceed tolerance.

Example assertions:

```cpp
TEST_CASE("M855 gelatin") {
  auto r = fire(ammo::M855, 940.0f, gel10());
  CHECK(r.penetration_cm == doctest::Approx(37.5).epsilon(0.15));
  CHECK(r.fragmented);
  CHECK(r.yaw_start_cm > 5.0f);
}
TEST_CASE("femoral transection") {
  auto v = make_humanoid(); v.sever(vessel::femoral_L);
  simulate(v, 300_s);
  CHECK_FALSE(v.conscious_at(120_s));
  CHECK(v.dead_between(120_s, 300_s));
}
```

---

## 18. Milestones and gates

One binary, one demo scenario, one gate command per milestone. Never start the next milestone early; never leave one half-done.

| # | Scope | Gate |
|---|---|---|
| M0 | CMake, presets, SDL3 window, fixed-step loop, debug renderer, rigid bodies, grab, harness, dev console | `ctest -L core` green; 500 bodies at 60 fps; allocation counter 0 |
| M1 | joints, ragdoll, materials + fracture lattice, rope, build mode (weld/cut/undo) | beam yield ±10%; rope rating; ragdoll collapses plausibly; goldens stable |
| M2 | anatomy polygons, channel, hemorrhage, perfusion, death, vitals overlay | physiology timing tests; blood conservation ±0.1% |
| M3 | ballistics: interior/exterior/terminal, ammo data, recoil, armour, CCD | gelatin table ±15%; momentum ±0.5%; no tunneling |
| M4 | melee, treatment items, clinical gore, audio | knife/rib and blunt-skull thresholds; tourniquet outcome test |
| M5 | wiring/logic, save/load, replay, heat/electricity/pressure | replay hash identical 1/2/4 threads; save round-trip; fuzz clean |
| M6 | content, tutorial, settings, polish | target scene 8 ms p99; no allocation; final demo passes §1 |

---

## 19. Review rubric and anti-patterns

Before each commit, answer in the message body:

1. Which state produces this visual? 2. What conserves and is asserted? 3. Which test would fail if I deleted this code? 4. Where do the constants come from? 5. Does it allocate/virtualise in the tick? 6. Is the sim still headless-testable? 7. Does it work with 1 thread? 8. Is any previous convention duplicated?

Rejected patterns:

```cpp
// REJECT — scripted outcome
if (hit.head && rng() < 0.9f) kill();
// ACCEPT — state-driven
brain.apply_strain(channel.strain(brain_region), dt);
physiology.integrate(dt);

// REJECT — hit-scan
raycast_and_damage(muzzle, dir);
// ACCEPT
projectile.integrate_substep(hit.dt);

// REJECT — cosmetic force
victim.apply_impulse(dir * 500.0f); // "feel"
// ACCEPT
recoil_impulse = bullet.momentum + gas.momentum;
```

Additional rejects: health integers; per-frame allocation; magic numbers in code; physics tuned to look good; `std::function` in step; dead code kept "for later"; tests that assert source text; tolerances widened to pass.

---

## 20. Risks, mitigations, stop conditions

| Risk | Mitigation |
|---|---|
| Determinism vs parallelism | fixed merge order, no atomics in state, 1/2/4-thread hash test; reduce parallelism if needed |
| Parameter sprawl | provenance rule in `PHYSICS.md`; calibration harness; no uncalibrated magic numbers |
| Scope explosion | depth-first per milestone; shotgun before flamethrower |
| Soft-body cost | fixed budget, sleeping, coarse LOD |
| "Accurate" vs "good-looking" | numbers validated by tests, looks validated by eye; never tune physics to look good |
| Build OOM | `-j2`, ThinLTO, `lld` |

**Stop and report** (with evidence) only if: a gate is unreachable for a structural reason you cannot resolve; a required constant cannot be sourced and no calibration target exists; two requirements are provably contradictory. Otherwise decide, document, continue.

---

## 21. First actions

1. Create `STATE.md`, `DECISIONS.md`, `PHYSICS.md` (constants table skeleton), data-file skeletons, CMake presets, and the failing test harness in one commit.
2. Implement M0 end-to-end.
3. Only then proceed to M1.

Do not ask for permission, library preferences, or content targets. Build, measure, verify.
