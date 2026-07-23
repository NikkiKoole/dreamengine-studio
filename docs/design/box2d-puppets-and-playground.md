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

- **DQS** (dual-quaternion skinning) in `boxskin` — kill the LBS candy-wrapper collapse at sharp
  bends (playtime already has this).
- **Skin the puppet's limbs / a tentacle chain**, not just one elbow.
- **A real get-up-from-prone controller** for `boxlab` (staged recovery, COM over support).
- **Two-way friction/rolling** in the coupling so a blob can *roll* a crate, not just shove it.
- **Slice 3 — light in-cart editing**: spawn shapes, pin joints, drag anchors (the box2d-editor seed).
- **Load a Rig from external data** so an imported sprite sheet becomes a puppet with no code edit.
