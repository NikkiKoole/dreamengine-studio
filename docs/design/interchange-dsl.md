# interchange DSL — describing junctions without coordinates (design sketch, 2026-06-16)

STATUS: SHIPPED (sketch 2026-06-16, realized by roadlab) — the two DSL layers landed via junction-lanelink §8–9; roadlab generates the roads.org.uk catalogue.

> Part of the road-geometry effort — map & how-we-got-here: [`road-geometry-handoff.md`](road-geometry-handoff.md).
> The research that grounds the geometry layer: [`road-geometry-refs.md`](road-geometry-refs.md).

A design exploration: **can every road interchange be described cleanly, declaratively — not as
hand-placed geometry?** Investigation (our `interchange.c` atoms + the roadnet2 junction matrix +
the [roads.org.uk](https://www.roads.org.uk/interchanges) taxonomy) says **yes**, in two layers:

1. **Topology layer** — *what connects to what, via which kind of ramp.* This is a genuinely clean DSL.
2. **Geometry layer** — *where the curves actually go.* A **relational** language (tangent / nest /
   clear / along) over a small **solver**, NOT hand-placed coordinates.

This is the formalization of the "reusable patterns" + `DRIVE` direction in
[`interchange-handoff.md`](interchange-handoff.md), and it *is* the roadnet2 drawer interface
([`roadnet2-plan.md`](roadnet2-plan.md) Part B: "given crossing roads + a type → draw the junction").

---

## Layer 1 — the topology DSL

> **A junction = a set of LEGS + a set of MOVEMENTS + a ramp-primitive assigned to each served movement.**

- **Leg** — a road meeting the hub: `(bearing, class, terminates?)`. (= our `Geo`: trunk dir,
  half-widths, angle.)
- **Movement** — an ordered pair of legs `O → D` (minus U-turns). The *set you serve* is the
  **completeness** axis: **full** = every movement; **partial** = a deliberate subset (half-diamond,
  parclo, half-trumpet — a partial is a real type built on purpose, not a broken full).
- **Ramp-primitive** — *how* a movement is physically connected. The primitive set is **tiny** — it's
  exactly the atoms we already have:

| primitive | what it is | our atom |
|---|---|---|
| `through` | the road continues (grade: at-grade / over / under) | the road strokers |
| `diverge` | peel off the driving-side lane, tangent | (extract from `near_pair`) |
| `merge` | reverse-curve to align tangent onto a lane | the loop exit / `near_pair` |
| `loop` | ~270° — the *hard* (left-equiv) turn, avoids crossing opposing traffic | `loop_ramp` / `loop_inner` |
| `flyover` | semi-direct: the hard turn done premium, crossing over | `flyover` |

**Every ramp = `diverge → [ loop | flyover | direct-curve ] → merge`.** That's the whole grammar.

### The payoff: every named interchange is one assignment

The **ramp-per-movement** family of the roads.org.uk catalog collapses to
**`legs × { movement → primitive }`**, with `DRIVE` deciding handedness. The named types differ *only*
in how they serve the **hard (left-equivalent)** turns:

| type | legs | hard turns served by |
|---|---|---|
| diamond | 2 (cross) | `diverge`+`merge` (at-grade / signal) |
| cloverleaf | 2 (cross) | 4× `loop` |
| partially-unrolled cloverleaf | 2 (cross) | mix: some `loop`, some `flyover` |
| (four-level) stack | 2 (cross) | 4× `flyover` |
| trumpet | T (one ends) | `loop` + (`loop` or `flyover`) + 2 direct |
| triangle | T/Y (3-way free-flow) | 3× `flyover` (trumpet's no-loop cousin) |
| wye / fork | Y (forks) | pure `diverge` |
| LILO | any | partial, all at-grade (no loop/flyover) |
| parclo / half-X | any | *same menu, subset of movements* |

So diamond / cloverleaf / stack are the **same legs and same movement set** — they differ only in the
primitive on the left turns (signal vs loop vs flyover). That is the "completeness × ramp-style menu"
axis already in the roadnet2 matrix.

### What this grammar does NOT cover — the ring family (verified against roads.org.uk, 2026-06-16)

Fetching the [roads.org.uk catalog](https://www.roads.org.uk/interchanges) (12 types, grouped by leg
count) confirms the table above covers **8 of 12** — the ramp-per-movement types — cleanly (the three
unnamed ones, *triangle / partially-unrolled cloverleaf / LILO*, are just unlisted assignments over the
same menu, so they're free). But **4 of 12 are a category this grammar can't express**: the
**roundabout / ring family** — *Roundabout Interchange* (the page calls it "the true British
interchange"), *Dumbbell*, *Three-Level Stacked Roundabout*, *Whirlpool* (turbine). These route
**multiple movements through one shared circulating carriageway**, not a dedicated ramp per movement —
so `{ movement → primitive }` doesn't decompose them. Covering them needs a new top-level primitive, a
**`ring` / `circulate`** (a closed loop the legs tap on/off, with the grade-separation living in how the
legs cross the ring), as a *peer* of the ramp grammar rather than an extension of it. **Parked** — the
ramp family is the priority; the ring family is a deliberately British shape we can add later. (Aside:
this is the over-claim correction — the earlier "*entire* catalog collapses to legs × {movement →
primitive}" was true only for the ramp family.)

### Sketch syntax
```
junction trumpet {
  leg HW = highway(bearing: E–W, lanes: 2)
  leg TR = road(bearing: S, lanes: 2, terminates)
  serve {
    TR → HW.west :  loop      // outer loop   (done)
    HW.west → TR :  loop      // inner loop   (nests with the above; WIP)
    TR → HW.east :  direct
    HW.east → TR :  direct
  }
}
```
`JUNCS[]` in `interchange.c` is already a proto-version of this (each row = `{topology, classA,
classB, ramp-style}`) — we just hardcoded geometry per style instead of deriving it from the
movement→primitive map.

---

## Layer 2 — the geometry language (the part that "isn't math")

**Key observation:** every geometry instruction in the session that built the loop was a *relation*,
never a coordinate — "tangent to the trunk", "fully on the far side", "merge *along* the lane",
"the trunk *becomes* the loop's straight side", "the two loops *nest*, kissing", "all lanes get *on*
it", "bend the *other way* to align with the road". **Coordinates were the output.** So the non-math
language already exists in how we talk; this layer formalizes it and puts a solver under it.

### The vocabulary
- **Port** — where a ramp attaches: `leg.direction.lane` → carries `(point, tangent direction, width)`.
  e.g. `HW.westbound.outer`, `TR.southbound`.
- **Continuity** — `tangent(a, b)` (smooth join, G1), `align(ramp, lane)` (run *along* it before merging).
- **Path-type** — qualitative curvature: `direct | loop | flyover` (not a radius).
- **Spatial relations** — `nest-inside(a, b)`, `clear(a, b, ≥ n·lane)`, `concentric(a, b)`,
  `parallel(a, b)`, `on(side)` where side ∈ {near, far, driving-side} (resolved by `DRIVE`).
- **Handedness** — never written; derived from `DRIVE` (+1 right, −1 left). Lets us read British
  diagrams and mirror in one place.

A ramp is then *declared*: `ramp(portA → portB, loop, on: far, clear: trunk ≥ 1 lane)` — and the
solver produces the curve. No coordinates authored.

### Two solver tiers (only one is hard)
1. **Per-ramp = closed-form, no iteration.** Two ports + a path-type pin the curve analytically
   (tangent continuity + the type). This is exactly the candy-cane (tangent-to-trunk) + reverse-curve
   (tangent-merge) constructions we derived by hand — encapsulate them behind `ramp(portA, portB,
   type)`. **Most of the session's work is this tier.**
2. **Between ramps = relaxation.** When ramps interact (two loops *nesting*, no-overlap, fit-in-frame)
   add relations (`nest`, `clear`, `concentric`) and a small iterative solver nudges the free
   parameters (radii, centre offsets) until satisfied. The *only* part needing iterative "solving";
   optional, only when primitives collide.

### Prior art (this is well-trodden, not vapor)
- **MetaPost / Hobby's algorithm** (Knuth & Hobby) — declare points + tangent *directions*, it solves
  the spline control points. Literally a non-math language for curves. **Roads ≈ MetaPost paths +
  width + road relations.** The closest precedent.
- **Penrose** (CMU) — declare objects + constraints (`tangent`, `contains`, `disjoint`); a solver lays
  them out. Diagrams from declarations.
- **Cassowary** (constraint solver, used in UI layout) and **parametric CAD sketchers** (SolveSpace,
  FreeCAD) — solvers for exactly `tangent` / `coincident` / `distance` constraints.

"Make solvers for the geometry" and "a language that isn't math" are **the same thing**: the non-math
language is the constraint declaration; the solver makes it concrete.

---

## What's clean vs. what needs the solver
- **Topology layer = a genuinely clean DSL.** ✅ Connectivity is fully declarative.
- **Geometry layer = clean *language*, but needs the solver under it.** Single ramps are closed-form
  (Tier 1); interacting ramps need relaxation (Tier 2). The DSL *feeds* the geometry (here are the
  ramps + relations); it does not replace the placement math — it hides it.

## Migration path from where we are
1. **`DRIVE` constant** + `right_of(dir)` — handedness from one place (already the agreed next pass).
2. **Extract the primitives** as real functions with **ports**: `diverge(port)`, `merge(port→lane)`,
   `loop(portA→portB)`, `flyover(portA→portB)` — generalising today's `loop_ramp`/`flyover`/`near_pair`.
3. **Tier-1 closed-form** inside each (the tangent constructions we already wrote).
4. **Re-express `JUNCS[]` as declarations** (`{movement → primitive}`) instead of per-style geometry.
   The concrete, standard-aligned schema for this — `Junction`/`Connection`/`LaneLink` C types reconciled
   against OpenDRIVE — is in [`junction-lanelink.md`](junction-lanelink.md).
5. **Add Tier-2 relations** only where needed first — the trumpet's two **nested loops** (`nest` +
   `clear`) is the natural first solver test.

## Open questions
- How far to push Tier 2 — a real relaxation solver, or curated per-type constant tables that *satisfy*
  the relations (cheaper, less general)? Lean: closed-form everywhere we can, relaxation only for nesting.
- Where the DSL is authored — inline C tables (like `JUNCS`), or a tiny parsed text format? (C tables
  first; text format only if worldgen needs to emit junctions.)
- Real-z vs fake-z grade separation interacts with `through`/`flyover` (currently fake-z drawn-over).
