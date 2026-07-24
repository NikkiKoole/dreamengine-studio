# Box2D cart ideas — the backlog

> **Status: EXPLORING / backlog (2026-07-14).** `tumble` (the first Box2D v3 cart) covers the
> "stuff colliding and getting dragged" half of the engine. These are candidate carts for the
> *other* half — mostly the joint + query features — so the shelf ends up mapping all of Box2D.
> First build: the **hill-climb buggy** — SHIPPED 2026-07-14 (`buggy`). Backend + integration:
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
| Native **`b2World_Explode`** (radial impulse) | ✓ **`boxjelly`** (cursor detonation, E) — tumble hand-rolls fragments; this uses the built-in |
| **Weld joints** that break | destructible welded structures |

## Now shown in `boxjelly` — the playground bay (2026-07-24)

`boxjelly` grew a small toy shelf that fills three of the rows above without needing a dedicated
cart, because they're *droppable sandbox toys*, not games (the game-shaped features — wheel joints,
motored flippers, sensors, ray casts, conveyors — stay their own carts):

- **Circle shapes + restitution A/B** — `b2CreateCircleShape` balls, one springy (restitution 0.82),
  one dead (0.04); `V` drops more. The first circle shapes in the Box2D carts (all others are boxes/hulls).
- **See-saw** — a plank on a **revolute joint** with `enableSpring` (self-levels empty) + `enableLimit`
  (ends stay off the floor). Torque/mass demo; blobs and crates weigh it down through the coupling.
- **`b2World_Explode`** — `E` detonates at the cursor: radial impulse to every dynamic body, a
  hand-rolled velocity kick to the verlet blobs (they live outside Box2D), `shake()` + a shockwave ring.
- **Prismatic launcher** — a pad on a **`b2PrismaticJoint`** (vertical rail, `enableLimit` stroke +
  `enableMotor`); auto-fires on a timer (or `F`): motor slams it up, retracts once it tops out, flinging
  whatever rests on it. The one joint type not shown in any other Box2D cart. Lives in the right-hand
  bay the canvas was widened to 400px for (layout pinned to `WORLD_CX` so nothing else shifted).

## The pitches

1. **Hill-climb buggy** ⭐ *(shipped — `buggy`)*. Chassis + two wheels on **wheel joints** — Box2D's built-in
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
