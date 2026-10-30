# State

## Current milestone
**M0 — engine core. Complete.**

Gate evidence (2026-10-30):
- `ctest` / `pp_tests`: 10/10 cases pass.
- 500 boxes: 0.77 ms average step (budget 16.6 ms).
- Zero heap allocations across `World::step()` after warmup (asserted).
- Determinism: identical hashes across 3,000 ticks in two independent worlds.
- Visual: window on `:98`, instanced renderer, 6-box tower stable, grab
  highlights and moves a body, circles settle.

## Next tasks (M1)
1. Joint types: revolute, weld, distance; ragdoll humanoid assembly.
2. Material table (`materials.toml`) with density/modulus/strength/toughness.
3. Mass-spring lattice for soft bodies and fracture; rope/cable constraints.
4. Build mode: weld, cut, undo/redo; save scene (versioned binary).
5. Island-based sleeping.

## Known gaps / accepted debt
- Sleeping is per-body, not island-based (tower stays awake; correctness fine).
- Position projection corrects translation only, not rotation.
- No material properties yet; density/friction/restitution are placeholders.
- No multithreading yet, so the thread-count determinism gate is deferred to M5.
- Renderer is debug-grade: flat quads/discs, no text or UI widgets.

## Performance log
| Date | Scene | Result |
|---|---|---|
| 2026-10-30 | 500 boxes on ground, Release, 1 thread | 0.77 ms/step |
| 2026-10-30 | demo scene, llvmpipe | 540–580 fps |
