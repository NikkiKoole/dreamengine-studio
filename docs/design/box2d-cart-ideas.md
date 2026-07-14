# Box2D cart ideas — the backlog

> **Status: EXPLORING / backlog (2026-07-14).** `tumble` (the first Box2D v3 cart) covers the
> "stuff colliding and getting dragged" half of the engine. These are candidate carts for the
> *other* half — mostly the joint + query features — so the shelf ends up mapping all of Box2D.
> First build: the **hill-climb buggy** (started 2026-07-14). Backend + integration:
> [box2d-integration.md](box2d-integration.md); the why-a-lib-here fork: [physics-notes.md](physics-notes.md).

## What `tumble` already shows

Dynamic boxes/circles, static wall segments, friction/restitution, contact **hit events** (impact
sound scaled by speed), the **mouse joint** (grab-drag), a **distance-joint** chain (the wrecking
ball), CCD **bullets**, and hand-rolled fragmentation. That's the collision + grab side.

## What's still unshown → a cart for each

| Unused Box2D feature | Candidate cart |
|---|---|
| **Wheel joints** (spring suspension + motor) + **`b2Chain`** smooth one-sided terrain | **hill-climb buggy** ⭐ |
| **Revolute joints** with **motors + angle limits** | **pinball** (snappy flippers) / spinner contraption |
| **Sensors** + overlap events (trigger zones) | scoring gates, goals, buttons |
| **Ray casts** (`b2World_CastRay`, line of sight) | a **turret / laser** defense toy |
| **Conveyor material** (`tangentSpeed`) + mixed joint types | a **Rube-Goldberg marble machine** |
| Native **`b2World_Explode`** (radial impulse) | (tumble hand-rolls fragments; could A/B the built-in) |
| **Weld joints** that break | destructible welded structures |

## The pitches

1. **Hill-climb buggy** ⭐ *(building)*. Chassis + two wheels on **wheel joints** — Box2D's built-in
   *spring suspension + motor torque*, the marquee vehicle feature — driving over procedural
   **`b2Chain`** terrain (smooth ground, no ghost bumps between segments). Gas/brake/lean, climb
   hills, don't land on your roof, reach the flag. Maximally different from `tumble` (locomotion vs
   destruction) and instantly legible. Verifiable: reached-flag / flipped-over. Delightful: driving
   feel + suspension bounce.

2. **Pinball table.** Two **revolute flippers** (motor + angle limit = a real snap), high-restitution
   bumpers, **sensor** score zones, tilt nudge. Best showcase of motored joints + sensors + events;
   more tuning to make it *feel* right than the car.

3. **Marble-machine sandbox.** Place spinners (motored revolutes), see-saws, **conveyor belts**
   (`tangentSpeed`), funnels; drop marbles and watch the cascade. Widest feature spread, most
   open-ended to land the "delightful to a stranger" half of the bar.

## Cross-pollination

The buggy could later drive *through* a `tumble`-style crate wall, tow a trailer (more distance
joints), or carry cargo that spills — marrying the two carts into one "physics world" once both feel
good on their own.
