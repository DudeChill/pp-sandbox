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

## Joints

| Constant | Value | Source / status |
|---|---|---|
| joint bias | 0.2 | same order as contact Baumgarte before split impulse |
| joint max bias velocity | 2 m/s | prevents explosive correction |
| joint velocity damping | 1.4 linear, 1.6 angular | structural damping, tuned for beam stability |
| joint projection iterations | 3, factor 0.8 | matches contact projection |
| torque EMA factor | 0.1 per tick | smooths solver spikes for yield/break |
| 2D depth for weld strength | 0.05 m | engineering assumption; documented in D009 |
| plastic slip rate | 1.5 rad/s at ultimate | calibration; yields permanent set |

## Materials

See `src/sim/material.cpp`: density, friction, restitution, Young's modulus,
yield and ultimate stress, toughness. Steel: 7850 kg/m³, 200 GPa, 250/450 MPa.
Bone: 1900 kg/m³, 17 GPa, 130/150 MPa. These feed weld strength now and the
penetration model in M3.

## Projectiles and trauma (slice)

| Quantity | Value | Notes |
|---|---|---|
| 9 mm projectile mass / radius | 8 g / 4.5 mm | reference load |
| muzzle speed | 350 m/s | 9×19 FMJ class |
| drag coefficient | 0.0015 1/m | `a = −k v²`; slows to ~206 m/s after 1 s |
| flesh penetration resistance | 12 MPa | calibrated so 9 mm penetrates 0.64 m |
| steel penetration resistance | 800 MPa | plate stops the same round |
| penetration depth | `E / (R·A)` | `A = πr²` |
| wound damage | `min(1, depth/0.20)·min(1, chord/0.05)` | lumped |
| bleed rate | `damage × vascularity` | head 60, torso 50, thigh 15 ml/s |
| unconscious threshold | 2.5 L blood | class III hemorrhage analogue |
| death threshold | 0.9 L blood | class IV analogue |
| vital death | head health ≤ 0.2 | brainstem analogue |

## Open items (M4+)

- Material table with density, modulus, strengths, toughness (prompt §13).
- Energy audit for restitution and friction losses.
- Seed-stable island ordering under multithreading.
