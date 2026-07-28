# Box2D puppets & the physics playground

> **Status: EXPLORING (2026-07-23).** A run of six carts + one shelf header that bring the
> "puppetmaker" idea (from the LÖVE2D **playtime** editor) into dreamengine: characters built from
> sprite parts, textures that deform, and rigid/soft/kinematic bodies intermingling. Built as a
> spike sequence; the open question is *what this is for here* (see "The open question").

Related: [box2d-integration.md](box2d-integration.md) (the vendored lib + `boxrig.h` contract) ·
[physics-notes.md](physics-notes.md) (the rigid-vs-particles fork).

## Where it came from

The maker's **playtime** project (a separate LÖVE2D Box2D character editor: shapes + joints +
fixtures + scripts + DQS mesh skinning + a Mipo character-DNA system, the toolchain for a
Sago-Mini-style small-apps pivot) is the mature home for this. These carts are the dreamengine-native
echo of that idea, useful because dreamengine is cheap to iterate in (write C, hit run, headless
dumps, deterministic input scripts). The spikes prototype the *ingestion* half (sprite → collision
geometry, auto-mesh) that playtime flagged as a gap; playtime owns the *skinning* half these mostly
don't have.

## What was built

The **puppetmaker representation**: a body part = one convex polygon of ≤8 `(x,y)` points (Box2D's
`B2_MAX_POLYGON_VERTICES`) + a texture, derived automatically from a sprite's alpha.

| Cart | What it proves |
|---|---|
| `carfit` | sprite alpha → traced outline (collision hull) **and** a grid-triangulated soft-deform mesh (verlet + `tritex`). The auto ingestion. |
| `puppet` | the rigid representation on real Box2D: each part auto-hulled to ≤8 verts (`b2MakePolygon`) + textured from that same polygon. Data-driven Rig table (parts + joints w/ limits + springs); marionette + lamp rigs; springy string; click-to-grab. |
| `boxlab` | a puppet that **tries to stay upright** — joint springs toward a standing pose + a torso PD controller. Recovers from moderate knocks, topples on big ones. |
| `boxskin` | one texture **skinned across a joint** (LBS over two bones), A/B vs rigid two-part rendering. The playtime `meshusert` idea, minimal. |
| `boxhuman` | the whole FIGURE: 15 bones, 6 sprite skins, the sprite↔body 1:1 finally broken. Segment-distance weights + DQS; one leg strip bends at hip, knee AND ankle; left/right share a texture by mirroring geometry and not UVs. |
| `boxjelly` | the culmination: verlet blob characters + Box2D crates + the self-righting puppet + one each of **static / kinematic / dynamic** bodies, two-way coupled, in one world. |

**`runtime/boxrig.h`** is the promoted shelf (cart-land library header, ADR-0006 — not core
`studio.h`, since Box2D is opt-in): `boxrig_hull` (sprite → ≤8-vert `b2Hull`), `boxrig_draw`
(texture a polygon from its own verts so paint == collision shape), `boxrig_point_in_body`
(click-to-grab), and `boxrig_resolve_box`/`resolve_poly` (the verlet↔Box2D coupling, `#ifdef PHYSICS_H`).

## The coupling (the hard part in `boxjelly`)

Two independent solvers — `physics.h` verlet (pixels) and Box2D (metres) — share one world:
- **Box2D → verlet** (hard): push every verlet point out of each rigid shape (oriented boxes *and*
  convex polygons via their own normals) so blobs rest and drape and get swept by the spinner.
- **verlet → Box2D** (capped impulse): those contacts apply a clamped impulse so a blob can shove a
  crate, stall the pinwheel, or press on the puppet, without the two solvers fighting into an explosion.
The cart owns the reaction-impulse policy (accumulate+cap for crates, immediate for the puppet);
`boxrig.h` owns the geometry + push-out and returns the contact.

## The open question — what is this *for* here?

Deliberately unresolved; kept live so we don't over-invest before it's answered. Plausible destinations:
1. **A cart-land capability shelf** — `boxrig.h` as the Box2D-character analogue of `physics.h`, so
   future carts reach for it. The carts are proofs the shelf works.
2. **A fast prototyping loop feeding playtime** — feel a mechanic here in an afternoon; polished
   version ships from playtime.
3. **A shippable small toy** — dreamengine has an `apps/` + iOS pipeline; a "knock the puppet over /
   pose it" cart could become a Sago-style one-verb app without touching playtime.
4. **Play and learn** — legitimate on its own.

Whichever it is, all six carts already clear dreamengine's two-part bar (ADR-0022: verifiable *and*
legible-and-delightful to a stranger).

## Next steps (from the carts' `todo`)

- ~~**DQS** (dual-quaternion skinning)~~ — done in `boxhuman` (SPACE A/Bs DQS / LBS / RIGID).
  `boxskin` still ships LBS on purpose, as the minimal two-bone teaching case.
- ~~**Skin the puppet's limbs**, not just one elbow~~ — done in `boxhuman`. A **tentacle chain**
  is still open, and is the case where playtime's newer spine-bind `(t, s)` beats weighted bones:
  no weight tuning, but it can't branch, so it does nothing for a torso.
- **A retracted finding, and why.** An earlier pass here claimed a *fold ceiling* for weighted-bone
  skinning (clean to ~-95°, inverted by -138°) and that playtime's spine-bind removed it. **Both
  numbers were measured on a broken rig** — see the ±180° `referenceAngle` branch cut below, which
  was snapping the whole figure apart on frame 1. The mesh inversions were the physics exploding,
  not the blend mode. Re-measured after the fix, with only the elbow folding and the knee held
  straight (inverted triangles out of 880, at frame 190):

  | elbow fold | DQS | SPINE |
  |---|---|---|
  | -70° | **0** | 10 |
  | -95° | **0** | 10 |
  | -120° | **0** | 10 |
  | -138° | **0** | 11 |

  So DQS has **no** fold ceiling on this rig, and the spine port carries a constant ~10 inverted
  triangles that does not vary with angle — a defect in the `(t, s)` implementation (most likely the
  chain-end overshoot or the hand bone), not a property of the technique. `boxhuman` therefore
  defaults to DQS, keeps SPINE on `SPACE` as an A/B, and the spine residual is the cart's open item.
  The **miter clamp** is default-off for the same reason: measured 6 rest / 16 fold with it against
  0 / 5 without. It assumes a smoothly swept ribbon width, and a dense Bezier bunches samples near a
  corner so `min(segA,segB)` collapses there and the clamp crushes the cross-section. Clamping
  relative to the bind-pose limit instead of absolutely was tried and did not help.

  The lesson worth keeping: **the inverted-triangle oracle was right all along and the eyeballing
  was wrong.** Bake each triangle's winding at bind, compare it every frame, and the number tells
  you what a crop of pixels cannot.

- **KEEP_ANGLE, and why playtime's "wrong" choice is right.** `boxhuman` now has a floor and
  ports playtime's per-body `KEEP_ANGLE` behaviour (`src/keep-angle.lua`): a PD controller that
  steers chosen bones back to the world angle they were authored at (pelvis and head upright, feet
  flat), suspended for whatever the user is dragging. playtime **writes the angular velocity**
  rather than applying a torque, which overrides the solver — so `boxhuman` implements both and
  measures. Head height above the floor (rest 98px), key `G` cycles:

  | keep mode | f60 | f200 | f390 | |
  |---|---|---|---|---|
  | off | 85.4 | 83.9 | 64.3 | slowly topples |
  | **omega-write (playtime)** | 84.9 | 84.0 | **84.0** | still standing |
  | torque (`boxlab`'s `balance()`) | 85.2 | 83.8 | 32.8 | collapsed |

  The physically-honest torque form loses: it asks politely and gravity out-votes it. **Judge this
  by head height, never by pelvis tilt** — the omega mode writes the pelvis angle directly, so it
  scores a perfect 0° tilt while lying flat.

- **The ±180° referenceAngle branch cut (this one bites any humanoid).** A revolute's limit is
  `b2RelativeAngle(qB,qA) - referenceAngle`, and `b2RelativeAngle` returns atan2 in `(-π, π]`. If
  you create bones rotated to point along their own segments, a humanoid's **pelvis→thigh** and
  **chest→arm** joints get a reference angle of exactly ±180° (spine up, limbs down) — dead on the
  branch cut, where numerical noise flips the measured angle by a full turn, the solver reads a
  colossal limit violation, and the figure snaps apart on frame 1 (measured: pelvis 16° off after
  ONE step). Fix: leave every bone body at **rotation identity** and bake the segment direction
  into the shape with `b2MakeOffsetBox`. Every `referenceAngle` becomes 0, no joint can sit on the
  cut, and `bone_angle()` cleanly means "rotation away from the authored pose". After the fix the
  rig sits at 0.0° error for 160 frames instead of exploding immediately.

- **Bone width is not cosmetic.** All bones were 2px-thin boxes, giving them almost no rotational
  inertia, so a KEEP_ANGLE controller on the root was simply absorbed by its heavier children.
  Half-width is per-bone data now (torso 0.30–0.32m, limbs 0.08–0.15m).

- **Three traps `boxhuman` hit**, worth knowing before the next rig cart:
  (1) bones created *rotated* need `j.referenceAngle` set to the pair's initial relative angle, or
  every limit is violated on frame 1 and the solver rips the pose apart;
  (2) a grid mesh must keep cells that *straddle* the alpha edge (test the neighbourhood, not the
  sample pixel) or the silhouette's concave pinches punch holes;
  (3) once the mesh overshoots the silhouette it samples index-0 texels, which are **opaque black**
  until the cart calls `colorkey(CLR_BLACK)` — a black halo on every limb.
- **A real get-up-from-prone controller** for `boxlab` (staged recovery, COM over support).
- **Two-way friction/rolling** in the coupling so a blob can *roll* a crate, not just shove it.
- **Slice 3 — light in-cart editing**: spawn shapes, pin joints, drag anchors (the box2d-editor seed).
- **Load a Rig from external data** so an imported sprite sheet becomes a puppet with no code edit.
