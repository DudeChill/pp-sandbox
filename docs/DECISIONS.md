# Decisions

Each entry records a choice, the reason, and what would force a revisit.

## D001 — SDL2 instead of SDL3
**Choice.** SDL2 (2.32 headers, system runtime).
**Reason.** The host has no development headers and its package repository
offers `libsdl2-dev`; SDL3 is not packaged. Headers are extracted into
`~/.local/opt/ppdeps` by `scripts/setup_deps.sh`.
**Revisit.** Port to SDL3 when it is packaged or built from source.

## D002 — OpenGL 3.3 core, not 4.6
**Choice.** Request a 3.3 core context; use instanced quads.
**Reason.** The only available GL implementation is Mesa llvmpipe reporting
4.5 on Xvfb `:98`. 3.3 core provides everything the batch renderer needs and is
the widest-compatible subset.
**Revisit.** Raise the version only if a feature requires it.

## D003 — Custom test harness instead of doctest
**Choice.** ~60-line assertion harness in `tests/framework.h`.
**Reason.** Keeps the dependency list at zero and works offline. The harness
prints failures with file/line and exits non-zero.
**Revisit.** Switch to doctest if richer assertions or test filtering are needed.

## D004 — Custom GL entry-point loader
**Choice.** `src/render/gl_api.{h,cpp}` loads ~35 symbols via
`SDL_GL_GetProcAddress`.
**Reason.** GLEW/glad development packages are absent and the surface is small.
**Revisit.** Replace with glad if the renderer grows past a few hundred calls.

## D005 — Custom sequential-impulse solver
**Choice.** Hand-written solver: SAT + face clipping for boxes, circle contacts,
warm starting, 10 velocity iterations, post-integration position projection.
**Reason.** The prompt requires the physics wrapper to be ours and swappable.
Position projection replaced Baumgarte bias after a reproduced instability:
bias velocity injected energy into tall stacks and made them collapse at some
world coordinates (see D006).
**Revisit.** Evaluate a block solver for two-point contacts when ragdolls need
stiffer stacks.

## D006 — Contact clipping plane offset bug (fixed)
**Symptom.** A six-box tower was stable at x=0 and collapsed at x=1.
**Cause.** `clip_segment` takes absolute coordinates, but the side planes were
passed `±ref_half` without adding `dot(ref_center, n)`. At x≠0 the planes cut
the incident face down to one point, producing asymmetric impulses and torque.
**Fix.** Pass `dot(ref_center, n) + ref_half`; regression test stacks boxes at
x=1.37 for 30 s.
**Lesson.** Unit tests must not run at the coordinate origin only.

## D007 — Sleeping without islands
**Choice.** Bodies sleep individually; a dynamic body is woken when it contacts
an awake dynamic body.
**Reason.** Simple and correct enough for static scenes. A tower in continuous
contact therefore never sleeps, costing a little CPU but staying stable.
**Revisit.** Island-based sleeping in M1 when ragdolls and many contacts exist.

## D008 — Determinism scope
**Choice.** Bitwise identical state for a given build and thread count; a
replay stores the parameter hash and refuses mismatched loads.
**Reason.** Cross-compiler bitwise equality is not achievable without fixed
point. FP contraction is disabled, fast-math is forbidden, and hashing uses a
streaming xxHash64 over declaration-ordered fields.
**Revisit.** Add fixed-point or an FP-contract-controlled toolchain if
cross-machine replays are required.

## D009 — Plastic-hinge weld model
**Choice.** Welded interfaces carry a yield torque and an ultimate torque
derived from the weaker material's strength and the joint's 2D section modulus
(`depth × height² / 6`, depth fixed at 5 cm). Above yield the rest angle slips
at a rate proportional to the overload; above ultimate the joint breaks.
**Reason.** Delivers "steel bends and stays bent, then tears" without a
continuum FEM, and makes the beam-yield test falsifiable at ±10%.
**Revisit.** Replace with a mass-spring lattice when soft-body fracture lands.

## D010 — Wake propagation through joints
**Choice.** Each tick, awake dynamic bodies wake every joint-connected body.
**Reason.** A half-asleep island treats sleeping bodies as static, which
silently dropped loads applied to one end of a beam. Propagation fixed the
beam test and is deterministic.
**Revisit.** Fold into island-based sleeping.

## D011 — External forces survive into the tick
**Choice.** `World::step` clears accumulated forces at the end of the tick,
not the start. Callers set `Body::force` before stepping; any nonzero force
wakes the body.
**Reason.** Needed for static load tests and for motors/thrusters (M5).
**Revisit.** Add an explicit force accumulator API if multiple systems write.
