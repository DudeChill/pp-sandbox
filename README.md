# pp — a physics sandbox

A 2D ragdoll physics sandbox in the lineage of *People Playground*, built around a
deterministic simulation core. The long-term goal is simulation-grade trauma,
ballistics, and material behaviour: every visible wound or fracture must be
explainable by the state of the model. See `PROMPT.md` for the full specification.

## Status

Milestone **M0** (engine core) is complete: deterministic fixed-step rigid-body
simulation, broadphase, contact solver with warm starting and position
projection, sleeping, instanced 2D renderer, camera, grab/delete/spawn tools,
headless test suite with numeric and determinism checks.

M1–M6 (joints/ragdolls/materials, anatomy/physiology, ballistics, melee/gore,
wiring/save-replay/secondary systems, content) are not implemented yet.
See `STATE.md` for the task list and `docs/DECISIONS.md` for recorded choices.

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
