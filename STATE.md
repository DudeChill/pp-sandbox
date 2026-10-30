# State

## Current milestone
**M1 — joints, ragdolls, materials, construction. Complete.**
M0 (engine core) complete.

Gate evidence (2026-10-30):
- `ctest` / `pp_tests`: 10/10 cases pass.
- 500 boxes: 0.77 ms average step (budget 16.6 ms).
- Zero heap allocations across `World::step()` after warmup (asserted).
- Determinism: identical hashes across 3,000 ticks in two independent worlds.
- Visual: window on `:98`, instanced renderer, 6-box tower stable, grab
  highlights and moves a body, circles settle.

## M1 evidence (2026-10-30)
- Distance/revolute/weld joints, angle limits, break thresholds, plastic hinge.
- Beam plastic-yield test: no yield at 0.9× predicted moment, yield at 1.1×,
  no rupture below ultimate (uses a plastic-hinge weld model).
- 13-part ragdoll collapses and remains assembled (anchor gaps < 5 cm).
- Rope holds a load and breaks above rated tension.
- Noumenal checks: material table, weld/cut tools, joint rendering.
- `pp_tests`: 18/18 pass; 500-body step still ~0.6 ms.

## M2/M3 vertical slice (2026-10-30)
Implemented:
- Projectiles: gravity, quadratic drag, swept-segment CCD, material-resistance
  penetration, pass-through with energy loss, impulse transfer.
- Trauma: per-body health, wound events, bleeding scaled by vascularity,
  shared blood volume, unconsciousness (<2.5 L) and death (<0.9 L), vital
  (brain) destruction threshold; ragdoll vitals group.
- Tourniquet treatment (stops limb bleeding, asserted stable blood volume).
- Blood particle feedback and a per-ragdoll blood bar (presentation only,
  not yet volume-conserving).
- Tests: penetration depth calibration, steel-stop vs flesh pass-through,
  headshot death, limb bleed without immediate death, drag/gravity,
  tourniquet-stabilised blood volume.

## M4 progress (2026-10-30)
- Tissue compartments (muscle, bone, artery, brain, heart, lungs, gut) with
  path-length wound channels; artery severance drives hemorrhage.
- Blade strokes with accumulated structural damage and joint severance.
- Versioned binary scene save/load with warm-start cache; round-trip
  determinism test over 240 further ticks.
- Explosives: 8 kJ radial charge with impulse and blast-tissue damage.
- Glass/concrete fracture into four deterministic shards above a material
  energy threshold; joints on the fractured body are removed first.
- Input replay recorder (`F6`/`F7`) with scene snapshot + per-tick inputs and
  hash verification; tests prove byte-identical replay over 600 frames.
- Heat/fire: ignition thresholds, spread, burn tissue damage, fluid loss, and
  death of vital organs; flame rendering.
- Wiring: bodies can be buttons or thrusters, wires form a signal graph,
  thrusters apply force along +X when powered; save/load and hash included.
- Blood budget: `emitted_ml` tracks every millilitre removed, asserted against
  the initial volume.
- Bitmap HUD (tool, ammo, bodies, wounds, blood) with a 3x5 built-in font.
- Motor joints with target speed and torque cap; app motor tool (`M`).
- Seven ammunition classes (`ammo.toml` data in code) with per-class drag and
  fragmentation thresholds; deterministic fragment sub-projectiles; tests for
  velocity ordering and 5.56-vs-9 mm fragmentation.

## Next tasks (M6)
1. Wiring/logic graph, thrusters, sensors.
2. G7 ballistic coefficient tables and temporary cavity.
3. Heat, electricity, and pressure systems.
4. Blood particle volume tied to `emitted_ml`.
5. Content pass and tutorial.

## Known gaps / accepted debt
- Weld plasticity is a hinge model, not a continuum mass-spring lattice; beam
  strength uses a 2D depth assumption (`kDepth2D = 0.05 m`).
- Joint wake propagation wakes whole islands; proper island sleeping is still
  pending.
- Position projection corrects translation only, not rotation.
- Materials affect mass/friction/restitution only when assigned via
  `set_material`; ragdoll density is a scaled value for 2D depth.
- No multithreading yet, so the thread-count determinism gate is deferred to M5.
- Renderer is debug-grade: flat quads/discs, no text or UI widgets.
- Wound channels are rectangular-compartment approximations: no temporary
  cavity, no fragmentation, no bone blocking. 5.56-class rounds over-penetrate
  relative to the gelatin table.
- Only one bladed stroke geometry (horizontal, 16 cm); no edge angle or
  sliding velocity.
- Blood particles are cosmetic; emitted volume is not yet tied to blood loss.

## Performance log
| Date | Scene | Result |
|---|---|---|
| 2026-10-30 | 500 boxes on ground, Release, 1 thread | 0.77 ms/step |
| 2026-10-30 | demo scene, llvmpipe | 540–580 fps |
| 2026-10-30 | M1 tests, 500 bodies | 0.6 ms/step |
| 2026-10-30 | demo scene + 50 projectiles | 390–550 fps (llvmpipe) |
| 2026-10-30 | scene save/load round-trip | 3.3 KB, hashes equal |
| 2026-10-30 | 1,500 bodies (`pp_bench`) | avg 1.45 ms, p99 2.19 ms (budget 8 ms) |
