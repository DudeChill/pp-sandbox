# State

## Current milestone
**M9 — final integration. Complete.** M0–M8 complete.

## Evidence (2026-10-30)
- `pp_tests`: **48/48 cases pass** on Linux locally and on Windows CI.
- CI run `41009696522` and successors: linux + windows success; Windows artifact
  `pp-windows-x64` bundles `pp.exe`, `SDL2.dll`, VC++ runtime.
- Benchmark (`pp_bench`, 1,500 bodies): ~1.0–1.5 ms average, p99 ≤ 2.3 ms.
- Zero heap allocations inside `World::step()` after warm-up (asserted).
- Determinism: identical hashes across worlds over thousands of ticks; scene
  round-trip and 600-frame replay reproduce the stored hash.

## Implemented
M0 core: fixed-step solver, contacts, projection, sleeping, renderer, harness.
M1 construction: distance/revolute/weld joints, limits, motors, plastic hinges,
14-part ragdoll, rope, materials, weld/cut.
M2 trauma: tissue compartments, wound channels, hemorrhage, blood budget,
shock, death, tourniquet, HUD bar, blood particles.
M3 ballistics: projectiles, seven ammo classes, drag, CCD, penetration,
fragmentation, glass/concrete fracture, explosions.
M4 gore/melee: blade strokes, accumulated structural damage, joint severance,
burns and fluid loss.
M5 wiring/persistence: buttons, thrusters, wires, motors, binary scene,
input replay with hash verification.
M6 portability: Windows 10/11 (MSVC/vcpkg), Linux, CI matrix, runnable artifact.
M7 depth: barrel-length velocity, zeroing, yaw, temporary cavity, armour,
IV/blood/defib/chest seal/splint, arrest and hypoxia.
M8 systems: 220 V electricity via the wire graph, procedural audio with
distance-delayed crack, mass-spring cloth and soft blobs.
M9 integration: docs, prompt/count alignment, cross-platform packaging.

## Known limitations (explicit)
- Continuous FEM/mass-spring fracture is not implemented; welds use plastic
  hinges and bodies split into four deterministic shards.
- Wound channels use body-local AABBs, not polygon compartments; no bone
  blocking.
- Blood particles are cosmetic; the scalar budget is authoritative.
- No multithreading; determinism is per build and thread count.
- Renderer and audio are deliberately minimal (no assets, debug-grade).

## Performance log
| Date | Scene | Result |
|---|---|---|
| 2026-10-30 | 500 boxes | 0.77 ms/step |
| 2026-10-30 | 1,500 bodies (`pp_bench`) | avg 1.45 ms, p99 2.19 ms |
| 2026-10-30 | Windows CI bench, 1,500 bodies | avg 0.79–1.2 ms, p99 ≤ 1.7 ms |
| 2026-10-30 | demo scene, llvmpipe | 350–580 fps |
