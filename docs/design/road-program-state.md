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
  M3 turn lanes + channelizing islands · M4 the street web (grid/organic/radial/cul-de-sac) · M5 sidewalks +
  crosswalks · M6 the mini-roundabout (traversable island, give-way entries, splitters — the at-grade ring) ·
  M7 the typed cross-section (median/bike/parking lane types; HW = sum of lanes, so the junction re-solves).
- **`spec()` harness** (new infrastructure, [`spec-harness.md`](spec-harness.md)) — the gameplay twin
  of `tune-check`; `streetlab` is the first/reference cart (50 assertions). **`roadlab` is now spec-locked
  too** (25 assertions: the `classify_turn` chirality, the `make_junction` generator counts, the splines
  landing on their ports) — a regression lock and a **golden reference for the Phase-2 port**. Both road
  sandboxes are pinned before the world step.

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
  signalized-vs-priority control, pedestrian refuge islands. M3/M5 did the headline ones (turn bays, medians,
  sidewalks, crosswalks).
- **Open research questions** remain — the per-pattern numeric metric table, a superblock/fused-grid
  algorithm, and state-of-the-art beyond the 2001/2008 pillars (learned generative models). See
  [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Backlog — loose ends scattered through the research (ranked, addable now)

Pulled from `road-hierarchy-notes` §2 / §5 / §8 — named in the research, in-scope for the sandbox, unbuilt.
Cheap + high-value at the top. (✓ = shipped since.)

**At-grade junction (Facet A — §2/§5), in `streetlab`'s junction view:**
- ✓ **Sidewalks + crosswalks** (M5) — the *pedestrian layer*. 'k' toggles an SW-wide kerbside sidewalk ring
      (the footprint inflated by SW, wrapping the corners) + zebra crosswalks at each mouth. Composes with skew/T.
- ✓ **Degree-1/3/4 share metric** (network view) — §8.2's real discriminator ("mean degree ALONE is not
      enough"). A "node mix" readout under the metrics line: `cul(1) % · T(3) % · X(4+) %` (Marshall's node
      taxonomy), spec-locked to actually SEPARATE the patterns (grid = 0 cul-de-sacs + a big X share; cul-de-sac
      = a real degree-1 share + far fewer Xs). Completes the SNDi readout next to degree/dead-ends/sinuosity.
- ✓ **Real cross-section / lane types** (M7) — §5 OpenDRIVE lane types: a lane-section from the centre out,
      `[median] · driving×N · [bike] · [parking]`, toggled per element (m/b/;). The median is the same island
      primitive as M3's splitter / M6's central island, run continuously. Key trick: HW = `cross_hw()` = the
      SUM of the lanes present, and every junction primitive keys off HW ⇒ widening the section re-solves the
      whole junction (curb returns, mouth, roundabout, sidewalks) for free. Spec'd. (Sidewalk = the M5 layer.)
- ✓ **Mini-roundabout** (M6, at-grade, §2) — a junction "treatment" (`r`, mutually exclusive with turn lanes):
      a circulatory disc + a MINI, TRAVERSABLE (mountable/domed) central island, flush teardrop SPLITTER
      islands, GIVE-WAY (yield) entry lines + CCW circulating arrows. Leg-model based ⇒ composes with skew/T/
      lanes/sidewalks. Distinct from roadlab's grade-separated ring. Spec'd: island ⊂ inscribed circle, stays mini.
- [ ] **Corner free-right slip + triangular channelizing island** (§2) — the corner treatment deferred in M3.
- [ ] **Curb extensions / bulb-outs + pedestrian refuge islands** (§2) — small ped-safety geometry.
- [ ] **TWLTL** (two-way centre left-turn lane) + **right-turn pockets** + **driveways as low-volume junctions** (§2).

**Network topology (Facet B — §8), in the network view:**
- [ ] **Fused-grid / superblock** pattern (§8.3) — perimeter arterial loop + calmed/discontinuous interior;
      the §8 open questions note *no algorithm exists in the 2001/2008 pillars*, so a small original bit.
- [ ] **Dendricity + circuity** metrics (§8.2) — the rest of the four-measure SNDi. Circuity needs shortest-path
      (heavier); defer until there's a reason.
- ✓ M4a–c: grid/organic/radial/cul-de-sac patterns + the curvature knob (winding, live sinuosity).

**Phase 2 — the world (§8.4), beyond a single screen:**
- [ ] **The two-tier major→minor generator** — major arterials first, then fill local streets per region
      (Parish-Müller / tensor fields). The bridge from "one pattern fills the screen" to a seed-driven world.
- [ ] **Integrate** `streetlab` × `roadnet2` × `roadlab` into one seed-driven map (the seams below).
- [ ] **Open research Qs** — per-pattern numeric metric table, Marshall route-structure mapping, learned
      generative models. See [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Known deferrals (pick up when convenient)

- **`streetlab`'s `index.json` gallery description is ~2 milestones behind** (still M4-era prose — doesn't
  mention M4b radial/cul-de-sac, M4c curve, or M5 sidewalks/crosswalks). Deferred because `index.json` is
  often mid-churn with other agents' carts (committing it risks sweeping their broken refs). Update it in a
  quiet moment via the splice dance (checkout HEAD index → edit only the streetlab entry → commit → restore).
  The cart's own header comment + HUD are current; only the registry blurb lags.

## Shared code & seams

**Duplication today is minimal — and that's correct.** roadlab and streetlab share only 6 function names, and
the only real overlap is trivial (`ux`/`uy` trig wrappers, `step_btn`, and the generic spec helpers). All the
*geometry* is deliberately distinct (interchange ramps vs at-grade curb-returns/network) — a shared geometry
file now would be over-abstraction. **No `roadkit.h` yet.**

- **Done:** the generic spec helpers (`spec_near`, `spec_close`, `spec_tap`) now live in
  [`runtime/spec.h`](../../runtime/spec.h), so every spec cart shares them instead of redefining them.
- **Specs on an includeable:** a shared header can't define `spec()` (one per cart), but it can expose a
  `void <lib>_selfcheck(void)` of `expect()`s that the cart's `spec()` calls — the pattern a future `roadkit.h`
  would use to carry its own tests. (Documented at the bottom of `spec.h`.)
- **The seams (marked `// SEAM (Phase 2)` in code):**
  - `roadlab.c` → `make_junction(legs, type)` is the interchange drawer a world calls per grade-separated crossing.
  - `streetlab.c` → `gen_network(pattern, seed)` is the per-region local-street generator a two-tier world calls;
    each network **node is a crossing the junction layer (M1–M3) can render in detail** (the network→junction zoom).
  - A `roadkit.h` becomes worthwhile **only if** Phase 2 makes both carts share primitives — extract then, not now.

## Pointers

- **Carts:** `tools/carts/{roadlab,streetlab,roadnet,roadnet2,interchange,rampkit}.c`
- **★ interchange map (start here for the ramp work):** [`road-geometry-handoff.md`](road-geometry-handoff.md)
- **Research + layered hierarchy:** [`road-hierarchy-notes.md`](road-hierarchy-notes.md)
- **The DSL / OpenDRIVE recon / geometry refs:** [`interchange-dsl.md`](interchange-dsl.md) ·
  [`junction-lanelink.md`](junction-lanelink.md) · [`road-geometry-refs.md`](road-geometry-refs.md)
- **The working method (build loop + verify gates):** [`roadlab-method.md`](roadlab-method.md)
- **The test harness:** [`spec-harness.md`](spec-harness.md)
