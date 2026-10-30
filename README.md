# pp — a physics sandbox

![build](https://github.com/DudeChill/pp-sandbox/actions/workflows/build.yml/badge.svg)

A 2D ragdoll physics sandbox in the lineage of *People Playground*, built around a
deterministic simulation core. Every visible wound, fracture, fire, and signal is a
readout of state; nothing is scripted. `PROMPT.md` is the complete implementation
specification.

## Status — M0–M9 complete

- **Physics:** fixed 240 Hz, warm-started sequential-impulse solver, position
  projection, CCD, sleeping, distance/revolute/weld joints with limits, motors,
  plastic hinges, breakable ropes.
- **Humanoid:** 13-part ragdoll, tissue compartments (brain/heart/lungs/gut/
  muscle/bone/artery), arterial hemorrhage, blood budget with conservation,
  shock, arrest, death, tourniquet/IV/blood/defibrillator/chest seal/splint.
- **Ballistics:** seven ammunition classes, gravity and quadratic drag, CCD,
  material-dependent penetration, barrel-length muzzle velocity, zeroing, yaw,
  temporary cavity, deterministic fragmentation, armour with blunt trauma.
- **Materials:** density/modulus/strength/penetration resistance/fracture/ignition
  tables; glass shatters into shards; wood burns and spreads fire.
- **Sandbox:** weld/cut, ropes, cloth, soft blobs, motors, buttons, thrusters,
  wires, batteries (220 V shocks), 8 kJ explosions, blades with severance.
- **Persistence:** versioned binary scene save/load; input replay checked by
  state hash (`OK`/`DIVERGED`).
- **Presentation:** instanced OpenGL renderer, blood particles, flames, HUD with
  a built-in 3×5 font, procedural impact/gunshot audio with a distance-delayed
  supersonic crack.
- **Tooling:** in-game console for spawning, tuning, measuring, profiling and
  dumping state; camera pan/zoom; deterministic 1/2/4-thread broadphase.
- **Platforms:** Windows 10/11 x64 (MSVC + vcpkg) and Linux; shared CMake and
  GitHub Actions CI. Windows artifact bundles `SDL2.dll` and the VC++ runtime.

## Build

### Windows 10/11 (x64)

Requirements: Visual Studio 2022 Build Tools (Desktop C++), CMake 3.25+, and
[vcpkg](https://github.com/microsoft/vcpkg) with SDL2.

```bat
vcpkg install sdl2 --triplet x64-windows
cmake -S . -B build -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
build\Release\pp.exe
```

vcpkg copies `SDL2.dll` next to the executable. `pp.exe` is built as a console
application so logs stay visible. The simulation and tests build without SDL
using `-DPP_BUILD_APP=OFF`. Prebuilt bundles are the
`pp-windows-x64` artifact on green CI runs:
<https://github.com/DudeChill/pp-sandbox/actions/workflows/build.yml>.

### Linux

```sh
sudo apt-get install libsdl2-dev    # or: ./scripts/setup_deps.sh (user prefix)
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure
./build/release/pp                  # needs an X display, e.g. DISPLAY=:98
```

## Controls

| Input | Action |
|---|---|
| Left drag | Grab a body with a spring |
| Right click | Spawn a circle |
| `B` / `K` | Spawn a box / glass pane |
| `H` / `O` | Spawn a ragdoll / hanging rope |
| `L` / `;` | Spawn cloth / soft blob |
| `N` / `Y` / `P` | Spawn button / thruster / 220 V battery |
| `G` / `C` / `Z` | Weld / cut / wire tool (click two bodies) |
| `M` | Motor tool: click two bodies to add a motor joint |
| `T` / `V` | Tourniquet / blade tool |
| `1` / `2` / `3` | Select 9×19, 5.56, or 7.62 |
| `4` / `5` / `6` / `7` / `8` | IV / blood bag / defibrillator / chest seal / splint |
| `9` / `0` | Issue a soft vest / steel plate to the body under the cursor |
| `F` | Fire the selected round toward the cursor |
| `I` / `E` | Ignite the body under the cursor / 8 kJ explosion at cursor |
| `X` | Delete the body under the cursor |
| Middle drag | Pan the camera |
| Mouse wheel | Zoom |
| `` ` `` | Toggle the dev console (`spawn`, `set gravity`, `set threads`, `pause`, `measure`, `profile`, `dump`, `hash`, `kill`, `help`) |
| `Space` / `R` | Pause / reset scene |
| `F5` / `F9` | Save / load `scene.ppb` |
| `F6` / `F7` | Record / replay `replay.ppr` (hash-verified) |
| `Esc` | Quit |

## Verification

```sh
ctest --test-dir build/release --output-on-failure
./build/release/pp_tests        # 48 cases
cmake --build build/release -j2 --target pp_bench && ./build/release/pp_bench
DISPLAY=:98 ./build/release/pp --frames 300 --threads 4   # sim/render p99 report
./build/release/pp --self-test                             # headless runtime smoke test
:: Windows
build\Release\pp.exe --self-test
build\Release\pp.exe --frames 300 --threads 4
```

The suite covers rigid-body physics, determinism (including 1/2/4-thread
broadphase hashes), zero-allocation stepping,
joints and plastic hinges, ragdolls, ropes, cloth and soft bodies, penetration
calibration, fragmentation, yaw/cavity, armour, treatments and arrest,
electricity, fire, wiring, explosions, audio mapping, persistence, replay, and
CCD robustness. Benchmark: 1,500 bodies at ~1 ms average, p99 < 2.3 ms
(llvmpipe host; budget 8 ms).

## Not in scope (v1)

Continuous FEM fracture (welds use plastic hinges and bodies split into four
shards), networking, mod support, and an engine or scripting layer. Mass-spring
cloth/soft bodies, electricity, and procedural audio are in scope and shipped.

## Layout

```
src/sim/     deterministic core (no SDL/render/audio)
src/render/  instanced renderer, GL loader, bitmap font
src/app/     loop, camera, tools, HUD, audio, save/replay
src/platform SDL include shim
tests/       headless numeric suite
tools/       bench, scene builders
docs/        DECISIONS.md, PHYSICS.md
```
