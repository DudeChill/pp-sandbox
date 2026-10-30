# pp — a physics sandbox

A 2D ragdoll physics sandbox in the lineage of *People Playground*, built around a
deterministic simulation core. The long-term goal is simulation-grade trauma,
ballistics, and material behaviour: every visible wound or fracture must be
explainable by the state of the model. See `PROMPT.md` for the full specification.

## Status

Milestones **M0** (engine core) and **M1** (joints, ragdoll, materials,
construction) are complete:

- deterministic fixed-step rigid-body simulation with warm-started contacts
  and a position-projection pass;
- distance, revolute, and weld joints with limits, break thresholds, and
  plastic hinges driven by material section strength;
- 13-part ragdoll, breakable rope, weld/cut tools;
- material table (steel, aluminium, wood, glass, concrete, rubber, bone,
  flesh) and per-body material assignment;
- instanced 2D renderer, camera, grab/spawn/delete tools;
- 18 headless tests covering physics, joints, materials, determinism,
  allocation, and performance.

A first **trauma/ballistics slice** (M2/M3) is in: 9 mm-class projectiles with
gravity, drag, continuous collision, material-dependent penetration,
energy-based wound damage, per-body bleeding into a shared blood volume,
unconsciousness/death thresholds, a tourniquet treatment, a per-ragdoll blood
bar, and blood particle feedback. This is a
lumped model, not yet the per-compartment anatomy of the specification.

M4–M6 (full anatomy/physiology, melee/gore detail, wiring/save-replay,
secondary systems, content) are not implemented yet. See `STATE.md`.

## Build

The host lacks SDL2 development headers. Fetch them into the user prefix once:

```sh
./scripts/setup_deps.sh          # extracts libsdl2-dev into ~/.local/opt/ppdeps
```

Then:

```sh
cmake --preset release           # or: cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure
./build/release/pp               # needs an X display, e.g. DISPLAY=:98
```

Debug preset enables ASan/UBSan:

```sh
cmake --preset debug && cmake --build build/debug -j2
```

The build deliberately uses `-j2` and `-ffp-contract=off`; do not add
`-ffast-math` or `-march=native`, both break reproducibility.

## Controls

| Input | Action |
|---|---|
| Left drag | Grab a body with a spring |
| Right click | Spawn a circle |
| `B` | Spawn a box at the cursor |
| `H` | Spawn a ragdoll with its feet at the cursor |
| `O` | Spawn a hanging rope at the cursor |
| `G` | Weld tool: click two bodies to weld them |
| `C` | Cut tool: click near a joint to remove it |
| `F` | Fire a 9 mm round from the left of the cursor toward it |
| `T` | Tourniquet tool: click a limb to stop its bleeding |
| `X` | Delete the body under the cursor |
| Mouse wheel | Zoom |
| `Space` | Pause/unpause |
| `R` | Reset the demo scene |
| `Esc` | Quit |

## Layout

```
src/sim/     deterministic simulation core (no window/render/audio deps)
src/render/  instanced 2D renderer, GL loader
src/app/     main loop, tools, camera
tests/       headless numeric tests
scripts/     dependency setup
docs/        decisions and physics provenance
```

## Verification

`pp_tests` covers gravity, resting contact, stack stability at non-zero
coordinates, momentum conservation, determinism across runs, zero allocation
during `step()`, stale-handle rejection, picking, and the 60 fps budget for
500 bodies. The suite exits non-zero on any failure.
