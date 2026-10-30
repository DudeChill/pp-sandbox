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

## Next tasks (M2)
1. Per-body tissue compartments (skin, muscle, bone, organs) with damage state.
2. Wound channel from projectile/blade geometry, `dE/dx` deposition.
3. Blood volume, hemorrhage rates, perfusion, unconsciousness and death.
4. Vitals overlay and treatment items (tourniquet, chest seal, fluids).
5. Projectiles with drag and penetration feeding the trauma model.

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

## Performance log
| Date | Scene | Result |
|---|---|---|
| 2026-10-30 | 500 boxes on ground, Release, 1 thread | 0.77 ms/step |
| 2026-10-30 | demo scene, llvmpipe | 540–580 fps |
| 2026-10-30 | M1 tests, 500 bodies | 0.6 ms/step |
