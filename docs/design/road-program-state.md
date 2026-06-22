# Road program — current state (2026-06-22)

> One-screen snapshot of where the whole road/streets effort stands, tying the tier-specific
> maps together. Detail lives in [`road-geometry-handoff.md`](road-geometry-handoff.md) (the
> ★ interchange map) and [`road-hierarchy-notes.md`](road-hierarchy-notes.md) (the research +
> the layered hierarchy). **Update the date + the table when a tier moves.**

## The thesis

Roads sit on a **mobility ↔ access** hierarchy. We build proof-of-grammar **sandboxes** bottom-up
— each one pins a tier *deterministically* (pure generators, spec-locked) — before a seed-driven
**world** composes them. The research ([`road-hierarchy-notes.md`](road-hierarchy-notes.md)) mapped
the whole hierarchy and flagged what was unmodeled; this is the build status against that map.

## Where each tier stands

| Tier | Cart | Status | What it proves |
|---|---|---|---|
| **Freeway / interchange** (grade-separated) | `roadlab` | ✅ done | the whole roads.org.uk catalogue (diamond/cloverleaf/stack/trumpet/fork/triangle/roundabout) generates from `(legs, type)` at any skew/lane-count — arc-spline ramps, clothoid joints, lane tapers, flyover elevation |
| **The seam** (access control / continuity tenet) | — | understood, not code | the line between the ramp world (above) and the at-grade world (below) — §1/§3 of the research |
| **At-grade junction** (Facet A) | `streetlab` M1–M3 | ✅ done | curb-return fillets (tangent arc), the leg layer (skew + the T), turn lanes + raised median channelizing islands — all angle-agnostic |
| **Local/collector network** (Facet B) | `streetlab` M4 | ✅ done | a seed-driven graph in 4 canonical patterns — grid / organic / radial / cul-de-sac — that the SNDi metrics (mean degree, dead-ends) actually separate (§8.2) |
| **(prior) spline road-world** | `roadnet` / `roadnet2` | partial | the deterministic highway-tier spline network; roadnet2 already dabbles in street patterns + cul-de-sacs |

**Headline:** every tier the research flagged as unmodeled now has a working, spec-locked sandbox.
The *grammar* of roads — interchange → at-grade junction → street web — is built end to end.

## Built this cycle (2026-06-22)

- **`streetlab`** (new cart) — the at-grade sibling of roadlab. M1 curb returns · M2 skew + the T ·
  M3 turn lanes + channelizing islands · M4 the street web (grid/organic/radial/cul-de-sac).
- **`spec()` harness** (new infrastructure, [`spec-harness.md`](spec-harness.md)) — the gameplay twin
  of `tune-check`; `streetlab` is the first and reference cart (27 assertions on its pure generators).

## The frontier — Phase 2 (not started)

The grammar is done; **composing a world** is the open work, and it has two halves the research named:

1. **The §8.4 two-tier generator** — major arterials first, then fill local streets *within each region*
   (Parish-Müller L-system / Chen tensor fields). Today one pattern fills the whole view; the real thing
   emits a *different* `(pattern, region)` per place and stitches them at the boundaries.
2. **Integration** — where the three sandboxes converge into one seed-driven map: `streetlab` (at-grade)
   × `roadnet2` (network/highway) × `roadlab` (interchanges). Today they're separate benches.

This is the step the interchange handoff has always pointed at ("a world that EMITS `(legs, type)` per
crossing from the seed").

## Honest caveats

- Sandboxes prove the **grammar**, not production fidelity: `streetlab`'s network is toy-scale (54 nodes,
  a jitter/spanning-tree morph) — **not** the actual L-system or tensor-field generators the papers describe.
- **Unbuilt Facet A primitives** the research named: staggered junctions, free-right slip lanes,
  signalized-vs-priority control, pedestrian refuge islands. M3 did the headline ones (turn bays, medians).
- **Open research questions** remain — the per-pattern numeric metric table, a superblock/fused-grid
  algorithm, and state-of-the-art beyond the 2001/2008 pillars (learned generative models). See
  [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Pointers

- **Carts:** `tools/carts/{roadlab,streetlab,roadnet,roadnet2,interchange,rampkit}.c`
- **★ interchange map (start here for the ramp work):** [`road-geometry-handoff.md`](road-geometry-handoff.md)
- **Research + layered hierarchy:** [`road-hierarchy-notes.md`](road-hierarchy-notes.md)
- **The DSL / OpenDRIVE recon / geometry refs:** [`interchange-dsl.md`](interchange-dsl.md) ·
  [`junction-lanelink.md`](junction-lanelink.md) · [`road-geometry-refs.md`](road-geometry-refs.md)
- **The working method (build loop + verify gates):** [`roadlab-method.md`](roadlab-method.md)
- **The test harness:** [`spec-harness.md`](spec-harness.md)
