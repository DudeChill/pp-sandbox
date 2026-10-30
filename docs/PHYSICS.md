# Physics constants and provenance

Convention: SI units. Every constant has a source or a calibration test; code
constants without provenance are review failures. Milestone M0 constants only.

| Constant | Value | Source / status |
|---|---|---|
| standard gravity | 9.81 m/s² | standard gravity, CODATA |
| simulation timestep | 1/240 s | engineering choice; 4 substeps per render frame at 60 Hz |
| velocity iterations | 10 | convergence test: six-box tower stable 30 s |
| position iterations | 3 | projection pass, correction factor 0.8 |
| contact slop | 5 mm | matches body scale (0.5 m boxes); avoids jitter |
| default friction | 0.6 | placeholder until the material table lands (M1) |
| friction combination | sqrt(μa·μb) | standard isotropic model |
| default restitution | 0.05 | placeholder; energy accounting test pending |
| sleep linear threshold | 0.03 m/s | 0.5 s below threshold ⇒ sleep |
| sleep angular threshold | 0.15 rad/s | same |
| grab spring | k = 400 N/m, c = 40 N·s/m | tuned for ~1 s settle on 100 kg bodies |
| grab force clamp | 20 kN | prevents teleporting heavy bodies |
| default density | 1000 kg/m³ | water-like placeholder; replaced by materials in M1 |

## Inverse mass / inertia

- Circle: `m = ρπr²`, `I = ½mr²`.
- Box: `m = 4ρhxhy`, `I = m(hx² + hy²)/3`.

## Solver notes

Velocity constraints: normal and Coulomb friction with accumulated impulses and
warm starting keyed by body pair. Penetration is corrected by a positional
projection pass after integration, so velocity solve adds no energy. Contact
points are recomputed each tick from SAT plus reference-face clipping; local
anchors are stored for the projection pass.

## Open items (M1+)

- Material table with density, modulus, strengths, toughness (prompt §13).
- Energy audit for restitution and friction losses.
- Seed-stable island ordering under multithreading.
