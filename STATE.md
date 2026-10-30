# State

## Current milestone
**M9 — final integration. Complete.** M0–M8 complete.

## Evidence (2026-10-30)
- `pp_tests`: **48/48 cases pass** on Linux and Windows.
- CI run `41013722082` (commit `baf84eb`): linux + windows success; Windows
  artifact `pp-windows-x64` (1.26 MB) bundles `pp.exe`, `SDL2.dll`, VC++ runtime
  and passes `pp.exe --self-test` from the packaged directory.
- Benchmark (`pp_bench`, 1,500 bodies): Linux avg 1.11 ms / p99 1.87 ms;
  Windows avg 1.20 ms / p99 2.20 ms (budget 8 ms).
- ThreadSanitizer: 48/48 clean.
- Render benchmark (`pp --frames 300 --threads 4`, llvmpipe, 1280×720):
  render avg 3.15 ms, p99 6.74 ms (budget 8 ms); self-test passes headless.
- Zero heap allocations inside `World::step()` after warm-up (asserted), also
  with two broadphase workers.
- Determinism: identical hashes across 1/2/4 broadphase workers over 3,000
  ticks; scene round-trip and 600-frame replay reproduce the stored hash;
  corrupt scene hashes are rejected.

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
M9 integration: docs, prompt/count alignment, cross-platform packaging,
deterministic multi-thread broadphase, in-game dev console, camera pan,
`--frames/--threads` timing runs, self-verifying scene format v3, runtime
smoke tests on both OSes.

## Known limitations (explicit)
- Continuous FEM/mass-spring fracture is not implemented; welds use plastic
  hinges and bodies split into four deterministic shards.
- Wound channels use body-local AABBs, not polygon compartments; no bone
  blocking.
- Blood particles are cosmetic; the scalar budget is authoritative.
- Parallelism is limited to broadphase collision tests; solver and physiology
  stay serial. Determinism is per build, not cross-compiler.
- Renderer and audio are deliberately minimal (no assets, debug-grade).

## Performance log
| Date | Scene | Result |
|---|---|---|
| 2026-10-30 | 500 boxes | 0.77 ms/step |
| 2026-10-30 | 1,500 bodies, local (`pp_bench`) | avg 2.13 ms, p99 5.82 ms |
| 2026-10-30 | 1,500 bodies, Linux CI | avg 1.11 ms, p99 1.87 ms |
| 2026-10-30 | 1,500 bodies, Windows CI | avg 1.20 ms, p99 2.20 ms |
| 2026-10-30 | render 300 frames, llvmpipe 1280×720 | avg 3.15 ms, p99 6.74 ms |
| 2026-10-30 | demo scene, llvmpipe | 250–360 fps |
