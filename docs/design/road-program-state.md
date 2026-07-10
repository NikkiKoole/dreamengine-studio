# Road program — current state (2026-06-23)

STATUS: BUILDING (2026-06-23) — live per-tier build status for the roads effort (sandboxes → world); the table below tracks each tier, with ~14 open items across tiers still to land.

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
| **Local/collector network** (Facet B) | `streetlab` M4 + Stage 2 | ✅ done | a seed-driven graph in 5 canonical patterns — grid / organic / radial / cul-de-sac / **superblock** (fused-grid) — that the SNDi metrics separate (§8.2): mean degree, dead-ends, node-mix, sinuosity, and **circuity** (the measure that proves the superblock distinct) |
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

## Audit (2026-06-24) — the genuine misses, consolidated

A pass over `road-hierarchy-notes.md` (§2/§7/§8/§9) against what `streetlab` actually ships, to
surface gaps that were real but scattered/buried below (some framed as "already covered" or "add if
needed", so invisible). Split by kind — **a miss is designed-in-the-notes + in-scope + not built AND
not deliberately deferred.** Deferred-by-plan items are NOT misses; they're in the Roadmap below.

**Genuine misses (named in the research, in-scope, no milestone):**
- [ ] **Staggered junction (the "double-T")** — §2/§7 + §10 open-Q. The minor road is OFFSET so it meets
      the through road as TWO T-junctions a short distance apart (L-R or R-L stagger), breaking one
      high-conflict 4-way into two priority 3-leg junctions. **The first primitive that needs TWO hubs**
      offset along a shared through carriageway — streetlab's single-hub model can't express it (T just
      drops an arm). The curb-return grammar is angle-agnostic so each hub re-solves for free; the work
      is the two-hub layout. *Currently "parked" in the Roadmap (step, line ~162) — promoted here as the
      top buildable miss.*
- [ ] **Standalone pedestrian refuge island** — §2/§7 ("refuge islands"). Exists today ONLY implicitly:
      the turn median (M3) and roundabout splitter (M6) "double as" refuges. There is **no refuge island
      on a plain median crossing** (a nose of raised median at the crosswalk, gap for the ped). *Refines
      the Backlog's "refuge islands are already covered" note — the standalone primitive is the gap.*
- [ ] **`loops-and-lollipops` as a distinct network pattern** — §7/§8.1 name it separately. streetlab has
      cul-de-sac (dendritic tree + sparse loops); loops-and-lollipops (loop roads feeding cul-de-sac stems)
      is folded into it, not separately selectable. Likely a cheap 6th `gen_network` pattern.
- [x] **`dendricity` metric** — ✅ SHIPPED 2026-07-06 in **`tools/sndi-check.js`** (worldgen-plan
      rung 0): length-share of bridge edges (Tarjan) over the contracted graph, in the full SNDi
      composite, over a real `.rvb` OR a generated-graph JSON. (streetlab's *in-cart* panel still
      lacks it — add there only if the live bench A/B ever wants it.)

**Built-but-thin (shipped, but narrower than the notes intend):**
- [ ] **Bulb-out / curb extension** — `draw_bulb()` is locked to the one config `peds && parking && !bike`;
      it appears nowhere else. The notes treat it as a general corner treatment.
- [ ] **`roadMark` model** (§5, `road-geometry-refs.md` "worth taking next") — markings are hardcoded;
      no per-boundary type (solid/broken) · colour · width · lane-change rule.

**Possibly out-of-scope (logged so the decision is explicit, not an oversight):**
- [ ] **Signalized vs priority control** — §7 arterial row + §10 open-Q. Everything today is give-way/stop-bar.
      This is a signals/phasing/timing layer, **not geometry** — arguably belongs to a future control layer,
      not this geometry sandbox. Decide explicitly rather than leave it implied-missing.
- [ ] **Lane predecessor/successor links** (§5) — only needed once routing runs; deferred to the world tier.

**Deferred-by-plan (NOT misses — tracked in the Roadmap / Known deferrals):** modal active-travel layer
(Facet C, Stage 2.5) · crossings & priority markings pass · field-rendering cutover (blocked on
[`software-canvas.md`](software-canvas.md)) · two-tier major→minor generator (Phase 2) · one-way streets (Phase 2) · integration
streetlab × roadnet2 × roadlab (Phase 3).

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
- ✓ **Corner free-right slip + triangular channelizing island** (§2) — the corner treatment deferred in M3.
      `f` cycles it in (plain → turn lanes → free-right → roundabout). The slip = the kerb-side lane turned into a
      constant-width curve (concentric arcs about one fillet); the pork-chop island lives ONLY in the corner zone
      beyond the through roads' kerb lines — fill-then-reopen the gap-side carriageway, so it's a near-nothing
      sliver at 90° (a perpendicular free-right shouldn't block the through movement) and a clean triangle at skew,
      never crossing a travel lane. Spec'd (81): fr_fits window, constant-width band, exclusive toggle, knob clamp.
      ✓ **Composes with a bike lane** (owner-flagged, fixed 2026-06-23): the slip is anchored on the DRIVING-
      carriageway edge (`drive_outer()`), not the kerb, so a cycle track sits OUTBOARD and WRAPS the corner as its
      own continuous path (`corner_bike` at the free-right corner), the slip nesting inboard. Bike-outside-slip-
      inside ⇒ they're separated, not overlaid (the Dutch protected-corner principle) — no corner crossing conflict.
- ✓ **Curb extensions / bulb-outs** (§2) — `draw_bulb()` fills the parking clear-zone at each mouth, shortening
      the crossing (auto with pavement+parking). Pedestrian refuge islands are already covered (the turn median +
      the roundabout splitter both serve as refuges).
- ◑ **Minor markings pass** (§2): ✓ **TWLTL** (two-way centre left-turn lane — a painted CENTRE lane type, `m`
      cycles none→median→TWLTL; routes through `centre_hw()`/`drive_inner()` so it composes with every primitive)
      · ✓ **right-turn pockets** (bundled into the `turn lanes` treatment — peels the OUTER driving lane via
      `drive_outer()-LANEW` + a right arrow, mirror of the left bay) · ✓ **driveways** (`d` cycles a per-side
      bitmask off→+→−→both; a flared kerb-cut apron crosses the kerb + sidewalk into the grass = a low-volume
      access point, FHWA access management). **→ Facet A's marking vocabulary is complete.**
- [ ] **Crossings & priority markings pass** *(deferred — a focused session on just markings, owner request
      2026-06-23)*. The LANE markings are done; this pass is about CONFLICT/PRIORITY markings where modes meet:
      bike-vs-car give-way (elephant's-teeth where a slip rejoins the arm / "car yields to bike"); emphasising
      path×road crossings (raised tables, give-way triangles/sharks-teeth, priority arrows); making the existing
      straight-through bike crossing (Pass-3 #5b) compose cleanly with the free-right. Pairs naturally with the
      modal-network layer (Facet C, §9) — best done once paths-as-edges exist so there's a real thing to mark.

**Network topology (Facet B — §8), in the network view:**
- ✓ **Fused-grid / superblock** pattern (§8.3, shipped 2026-06-23) — a continuous arterial GRID (every SB-th
      lattice line) wrapping a vehicle-DISCONTINUOUS interior (local cul-de-sac stems off the perimeter via a
      Kruskal spanning forest + a rare loop fraction). The one original bit (no algorithm in the 2001/2008
      pillars). 5th pattern in `gen_network`; lattice bumped to 10×7 so `SB=3` cells divide cleanly. Spec'd.
      ✓ **Interior-only curve** (2026-06-23, owner request): the curve knob now winds ONLY the calmed interior
      local streets — the arterial frame stays dead straight (an `arterial` flag on the edge; `edge_bow` skips
      it). The real fused-grid look: engineered through-routes straight, residential interior curved. Other
      patterns mark no edge arterial, so they still bow uniformly. (Curve is visual/sinuosity only — topology,
      degree, circuity unchanged.)
- ◑ **Dendricity + circuity** metrics (§8.2) — ✓ **circuity** (shipped 2026-06-23): network shortest-path ÷
      straight-line, Floyd–Warshall over the graph (the heavier measure, fine at sandbox scale — O(n³) is a
      Stage-3 concern). It's the discriminator that PROVES the superblock: grid 1.21 < superblock 1.59 <
      cul-de-sac 2.18, even though superblock & cul-de-sac read near-identical on degree/node-mix — §8.2's
      "node degree alone is not enough" thesis, demonstrated. **Dendricity still deferred** (add if needed).
- ✓ M4a–c: grid/organic/radial/cul-de-sac patterns + the curvature knob (winding, live sinuosity).
- [ ] **Modal (active-travel) network layer** (Facet C, §9) — independent bike/foot paths as their OWN edges (not
      a road cross-section): parallel-at-offset / divergent / own crossings. The superblock's permeable-but-
      vehicle-discontinuous co-feature. See Stage 2.5.

**Phase 2 — the world (§8.4), beyond a single screen:**
- [ ] **The two-tier major→minor generator** — major arterials first, then fill local streets per region
      (Parish-Müller / tensor fields). The bridge from "one pattern fills the screen" to a seed-driven world.
- [ ] **One-way streets** — directionality is a *network* property: a one-way is a DIRECTED edge (the graph is
      undirected today), and a junction inherits "this arm is entry-/exit-only" from it. Phase-2 territory, not a
      cross-section shape — but it breaks the symmetric two-way mirror (no centreline, single flow), so the M7
      lane-section models the HALF-section as the unit (mirror at the draw site) ⇒ one-way is later a flag, not a rewrite.
- [ ] **Integrate** `streetlab` × `roadnet2` × `roadlab` into one seed-driven map (the seams below).
- [ ] **Open research Qs** — per-pattern numeric metric table, Marshall route-structure mapping, learned
      generative models. See [`road-hierarchy-notes.md`](road-hierarchy-notes.md) → "Open questions".

## Roadmap — the ordered execution plan (set 2026-06-22)

**Guiding principle:** finish each tier's grammar IN ISOLATION (spec-locked) before composing tiers — the
sandboxes pay off most when each is complete. Three stages, bottom-up. Do NOT jump to the Phase-2 "bridge"
early; exhaust what can be built in isolation first.

**Stage 1 — finish the at-grade JUNCTION (Facet A).** Close out `streetlab`'s junction view, increasing effort:
1. ✓ **Bulb-outs / curb extensions** (done 2026-06-22) — `draw_bulb()` fills the parking clear-zone (#8) with a
   sidewalk-grey kerb extension + inboard kerb, auto when pavement+parking **and no kerb-side bike lane**, and the
   zebra shortens to the driving lanes (sits between the bulbs). Per-side start datums (`side_clearance`) so it
   works under skew. NOT drawn with a bike lane on: a classic bulb (parking→sidewalk) and a protected bike-lane
   corner (bike-wrap + corner island) are different treatments — the bike-wrap is the corner treatment there, and
   stacking a straight bulb under the wrapping lane frays at skewed corners. (Protected-corner island = future.)
2. ✓ **Corner free-right slip + triangular channelizing island** *(done 2026-06-23)* — the corner treatment
   deferred in M3. `f` cycles it in. Constant-width slip (concentric arcs about one fillet); the pork-chop island
   is carved to the corner zone OUTSIDE the through kerb lines (fill-then-reopen-gap-side) ⇒ a near-nothing sliver
   at 90°, a clean triangle at skew, never over a lane (owner-approved). Spec'd (81). fr_fits window kept at 40–145°.
3. ◑ **Minor markings pass** *(low, batched)* — ✓ TWLTL (centre lane type, `m` cycle, owner-approved; routes
   through `centre_hw()`/`drive_inner()` ⇒ composes, no M7 regression) · ✓ right-turn pockets (bundled into the
   `turn lanes` treatment, off `drive_outer()`) · ✓ driveways (`d` per-side cycle, kerb-cut apron). **Step 3 DONE.**
4. *Parked:* staggered junctions + signalised-vs-priority CONTROL (the latter is a signals/phasing layer, not
   at-grade geometry — its own thing). Revisit only on request.
   → Facet A complete + spec-locked; the junction cart is done.

**Stage 2 — finish the NETWORK sandbox (Facet B).**
5. ✓ **Fused-grid / superblock pattern** *(done 2026-06-23)* — a continuous arterial grid wrapping a vehicle-
   discontinuous interior. The one original bit AND a single-region prototype of the two-tier world. Shipped
   WITH circuity (step 6) as one unit, since circuity is what proves it's distinct.
6. ✓ **Circuity metric** *(done 2026-06-23)* — Floyd–Warshall network/straight-line ratio; orders grid <
   superblock < cul-de-sac. *Dendricity still deferred* (add only if needed).
   → Network sandbox complete; the superblock teaches the region model. **Next: step 6b (the modal layer).**

**Stage 2.5 — the MODAL (active-travel) network layer (Facet C, §9 of the research).** *(new, scoped 2026-06-23)*
6b. **Independent bike/foot paths as their own network edges** — NOT a road cross-section (that's built), but a
    separate parallel graph overlaid on the vehicle network: a thin path edge (own polyline + width, drawn as its
    own ribbon with grass verges) that parallels a road at an offset, diverges to its own alignment (park, canal,
    block cut-through), and crosses roads at its own crossings (reusing the M5/Pass-3 zebra + elephant's-teeth
    markings). It has its own hierarchy (local → cycle route → fast route) and its own junctions. **Why here:** the
    superblock (step 5) is by definition *"pedestrian-permeable, vehicle-discontinuous"* — the active-travel graph
    has MORE edges than the vehicle one — so this is the superblock's natural co-feature, not a separate effort.
    Borderline sub-case: a verge-separated parallel footway *could* instead be a cross-section variant (`verge`
    lane type + setback sidewalk). Build the independent edge in the network view; revisit the verge variant then.
   → Pairs with / extends Stage 2; the two-tier world (Stage 3) then carries TWO overlaid networks, not one.

**► Suggested sequence for Stages 2–2.5 (set 2026-06-23)** — do them as ONE coherent "network + active-travel"
stretch, all still inside `streetlab`, before the Stage-3 world leap:
1. ✓ **Superblock + circuity together** *(done 2026-06-23)* — built the fused-grid pattern (step 5) and the
   circuity metric (step 6) in one pass. Circuity proves it: grid 1.21 < superblock 1.59 < cul-de-sac 2.18,
   where degree/node-mix can't tell the superblock from a cul-de-sac. Spec-locked (102 assertions), baked.
2. **Modal active-travel layer** (step 6b / Facet C) — paths-as-edges. The superblock's cut-throughs ARE these
   edges, so it lands naturally right after. **← NEXT.**
3. **Crossings & priority markings pass** (the deferred markings item) — now there are real path×road conflicts
   to mark, so it has something to bite on.
→ That completes the network sandbox *with* bikes/peds. THEN take the breath (end-of-Stage-2 stopping point)
   before Stage 3. Don't pull Stage-3 (two-tier world / integration) forward — exhaust the sandbox first.

**Stage 3 — Phase 2: compose the WORLD (the frontier; needs all sandboxes done).**
7. **Directed network → one-way streets** — directionality is a network property; the junction inherits
   entry/exit arms. The M7 half-section groundwork makes the one-way cross-section a flag.
8. **Two-tier major→minor generator** — major arterials first, fill local streets per region; the superblock
   (step 5) generalises straight into this.
9. **Network→junction zoom** — each network node renders in detail via the `streetlab` junction layer (the
   `gen_network` seam already marked in code).
10. **Integrate** `streetlab` × `roadnet2` × `roadlab` into one seed-driven map (grade-separated crossings call
    roadlab's `make_junction`).
11. *Ongoing/optional:* the open research questions (per-pattern metric table, learned generative models).
   → A seed-driven world that emits `(pattern, region)` per place and `(legs, type)` per crossing.

**Natural stopping points:** end of Stage 1 (junction done), end of Stage 2 (both sandboxes done — the clean
"stop and combine later" line), and each Stage-3 step is its own deliverable.

### ► Resume here (2026-06-23) — Stage 2 superblock + circuity COMPLETE; next is the MODAL layer (step 6b)

**State (latest):** the **superblock / fused-grid pattern + circuity metric** shipped (suggested-sequence step 1).
`PAT_SUPERBLOCK` is the 5th pattern in `gen_network` (cycle with `b` in the network view): a continuous arterial
grid (`SB=3`, every 3rd lattice line) wrapping a vehicle-discontinuous interior (Kruskal spanning forest of
cul-de-sac stems off the perimeter + a rare loop). Lattice bumped 9×6 → **10×7** so the cells divide cleanly.
`mean_circuity()` (Floyd–Warshall, in the node-mix readout line) orders **grid 1.21 < superblock 1.59 <
cul-de-sac 2.18** — and that's the only measure that separates the superblock from a cul-de-sac (they match on
degree/node-mix), the §8.2 thesis demonstrated. **The curve knob winds only the interior** (arterial frame stays
straight via an `arterial` edge flag) — the fused-grid look. Spec'd (**104 assertions**); build-all + ui-audit clean; baked.
**Next: step 6b — the modal active-travel layer** (paths-as-edges; the superblock's sealed interior is exactly
where the pedestrian/bike cut-throughs go). Then step 3 (crossings/priority markings), then the Stage-2 breather.

<details><summary>Earlier resume note — Stage 1 (Facet A, the free-right slip) COMPLETE</summary>

**State:** the free-right slip + pork-chop channelizing island is **done and owner-approved**. Toggle with `f`
/ the "junction:" toolbar cycle (plain → turn lanes → **free-right** → roundabout, mutually exclusive via
`set_treatment`/`cur_treatment`). `[`/`]` is its turn-radius knob `frR` ("slip R"). Spec'd (81 assertions);
build-all + ui-audit clean; baked. (Site publish is stale — a separate `publish-cart.sh` step, not done here.)

**The geometry that shipped (`draw_freeright`):**
- The slip is the **kerb-side LANE turned into a curve** that cuts the corner, anchored on that lane's
  **centreline** (`lc = HW − SLIPW/2`) via a `curb_return` fillet; the slip = centreline **± SLIPW/2** — two
  CONCENTRIC arcs about ONE fillet centre ⇒ **constant width** at any angle (`fill_ring`). Give-way = a radial
  dash bar at the **bA-arm** exit (drive-on-right). Gated to `fr_fits` (gap 40–145°).
- **The island rule (owner-specified):** the pork-chop lives ENTIRELY in the corner zone OUTSIDE both through
  roads' kerb lines — it must never sit in a travel lane (a straight-ahead driver would have to swerve round it).
  Recipe: `fill_corner` generously from the **kerb corner** to the slip's inner edge, THEN **re-lay each road's
  gap-side carriageway (centre→kerb) as asphalt**, carving the island back to just the wedge beyond the kerb
  lines. ⇒ a near-nothing sliver at 90° (correct — a perpendicular free-right barely needs one), a clean triangle
  at skew, verified by pixel-scan to never cross a kerb line. (Re-lay the *gap-side half only*, so a neighbouring
  corner's slip is untouched.)
- **Dead ends NOT to retry** (each baked-and-rejected): (a) two separate tangent fillets → slip **tapers/pinches**
  at the waist, mismatched at skew; (b) island as a concentric arc about the *outer* kerb centre → **sheared
  triangles** at skew; (c) island = the carriageway `fill_corner` → **paints over the through lanes**; (d) island
  apex anchored INSIDE the carriageway (lane-centreline or through-edge offset) → at acute/skewed corners the
  `fill_corner` cap fans a **long tail back over the lanes** (the bug the owner flagged). **The fix that worked:
  anchor at the kerb corner + reopen the gap-side carriageway** (island = only the bit beyond the kerb lines).

**Confirmed free-right parameters** (TxDOT Ch.13.9, NCHRP *Design Guidance for Channelized Right-Turn Lanes*
22238, AASHTO Green Book; closes the §6/§10 "named-but-unverified" gap): channelizing island min side ~15 ft
(12 ft constrained); island offset from the traveled way; exit defaults to YIELD/give-way; ped crosswalk across
the slip centre + island as refuge; small radii preferred; free-rights discouraged in ped-heavy contexts (NACTO).

**Stage 1 (Facet A) COMPLETE** — ✓ free-right slip, ✓ TWLTL, ✓ right-turn pockets, ✓ driveways; the at-grade
junction's geometry + marking vocabulary is built and spec-locked (94 assertions). The junction cart is done.
Also fixed: the **free-right × bike-lane** overlay (slip anchored on `drive_outer()` so the cycle track wraps the
corner outside it — separated, the protected-corner arrangement). **Deferred (owner): a focused crossings/priority
markings pass** (bike-vs-car give-way, path×road crossings) — best paired with the modal layer (Facet C).
**Next: Stage 2 — the NETWORK superblock** (the fused-grid: a perimeter arterial loop + a calmed/discontinuous
interior; the one original bit with no algorithm in the 2001/2008 pillars, and a single-region prototype of the
two-tier world, so it de-risks Stage 3). Don't jump to the world.
</details>

## Cross-section composition — known issues + the next-pass plan (2026-06-22)

M7 added typed lanes, but the EARLIER milestones (M3 turn lanes, M6 roundabout, M5 peds) still compute lane
offsets their own way — they predate the lane model. Collected issues:

1. **Turn-lane markings ignore the median offset** — the left-turn delineation + arrow are hardcoded at
   `LANEW` from the centre, not at the inner *driving* lane, so with a median they land on/in the median.
2. **Two medians stack** — the M3 turn splitter draws on top of the M7 continuous median. A turn bay should be
   the *gap in* the median, not a splitter laid over it.
3. **Stop bar spans the full HW** — including the bike + parking lanes (should stop at the driving lanes).
4. **Roundabout draws no lanes** — `cross_markings` runs only in the non-roundabout branch, so median/bike/
   parking widen the arms but paint nothing.
5. **Bike lane drops at the junction** — (5a) it should WRAP THE CORNER (a terracotta arc concentric with the
   curb return, one band inboard of the sidewalk ring — same construction as M5); (5b) the straight-through
   crossing (elephant's feet) is contextual → optional/deferred.
6. **Pavement bundled with crosswalks** under `k` — the sidewalk is a lane type; it wants its own toggle.
7/8. Cosmetic: markings start abruptly at the mouth; parking should end back from the junction (a clear zone) —
   the same cleanup the corner-wrap (5a) needs.

**The through-line: there's no single shared lane model.** The fix is one refactor + a follow-up:
- ✓ **Pass 1 — the lane-section as the single source of truth** (shipped 2026-06-22). `drive_inner()`/
  `drive_outer()` are the shared offsets; turn lanes (#1 delineation+arrow off the inner driving lane, #2 the
  splitter only when there's no median — the bay is the median's gap), the stop bar (#3 spans the driving lanes
  only), the roundabout approaches (#4 `cross_markings` now runs there) and pavement (#6 its own lane-type
  toggle on the cross-section row) all read from it. Half-section is the unit (one-way readiness). Spec'd (53).
- ✓ **Pass 2 — bike corner-wrap (#5a) + parking clear-zone (#8)** (shipped 2026-06-22). Reordered the section so
  the bike lane is OUTERMOST (kerb-side), parking inboard — the Dutch protected arrangement. The bike lane now
  WRAPS each corner: `corner_bike()` fills a terracotta annular band just inside the curb-return arc (radii
  R..R+BIKEW about the return centre), meeting each arm's kerb-side lane at the tangent points. Parking markings
  end `PCLEAR` back from the junction (a clear zone). Spec'd the lane order (58).
- ✓ **Pass 3 — the straight-through bike crossing, made OPTIONAL (#5b)** (shipped 2026-06-22). The bike toggle is
  now a 3-state cycle: off → on (lanes + the always-on corner-wrap) → +crossing. At level 2, `bike_thru()` draws
  elephant's-feet (a dashed row of terracotta squares) continuing each bike lane straight across the box — opt-in
  because it's contextual (signalised vs priority), exactly "some junctions want it, others don't". Spec'd the
  3-state cycle (60). (#7 transition-zone polish left as acceptable — the unmarked junction box is normal, and the
  bike crossing now optionally carries the lane through.)
- ✓ **Bike-lane composition follow-ups** (2026-06-22) — the bike lane now shows where the other treatments are:
  (a) with TURN LANES, `cross_markings` split into a kerb-side datum (bike/parking, at the corner clearance) and
  a centre datum (centreline/median/dividers, pushed past the turn bay) — a central turn bay no longer shoves the
  kerb-side bike lane off the arm; (b) at the ROUNDABOUT, the bike lane CIRCULATES — a terracotta ring just inside
  the kerb that meets the approach bike lanes at each mouth.

## Known deferrals (pick up when convenient)

- **Field-based road rendering refactor (streetlab)** *(queued 2026-06-24, proven in the `skewlab` cart)* —
  streetlab draws road edges PER-ARM (casing bands), which strays/misses-outline/double-lines at skew &
  curves. The fix: draw the road from ONE lateral distance field (`latDist`), with asphalt/kerb/sidewalk/
  lane-lines all thresholds on it — clean + uniform at any skew/curve, and it DELETES `mirror_blit` +
  the casing-fillet + `stroke_corner`. Proven in `skewlab`; deliberately deferred (core road-drawing
  rewrite, interacts with all the modes; may want to land with the software canvas). **Full method,
  boundary contract, and port plan:** [`field-based-road-rendering.md`](field-based-road-rendering.md).
- *(resolved 2026-06-22)* `streetlab`'s `index.json` gallery description is now current through M7 (committed in
  a quiet moment when the registry was clean). Left here as a note: when it lags again, update only the streetlab
  entry; if the file is dirty with foreign edits, prefer "just ship it" for a ref-safe description change.

## Shared code & seams

**Duplication between roadlab & streetlab is still minimal** (6 shared names, trivial `ux`/`uy` trig +
spec helpers). But the extraction trigger has now **fired for a different reason** than shared duplication:
`sloop` drives real OSM Delft (Rung B), and the OSM work surfaced that a **drivable world needs the
at-grade grammar as a callable, N-arm-native renderer** — so **`roadkit.h` is now GO.** Decision + the
full extraction plan (what moves in, the interface, phasing, keeping `spec` green): **[`roadkit.md`](roadkit.md)**.
*(This supersedes the "extract then, not now" note below.)*

- **Done:** the generic spec helpers (`spec_near`, `spec_close`, `spec_tap`) now live in
  [`runtime/spec.h`](../../runtime/spec.h), so every spec cart shares them instead of redefining them.
- **Specs on an includeable:** a shared header can't define `spec()` (one per cart), but it can expose a
  `void <lib>_selfcheck(void)` of `expect()`s that the cart's `spec()` calls — the pattern a future `roadkit.h`
  would use to carry its own tests. (Documented at the bottom of `spec.h`.)
- **The seams (marked `// SEAM (Phase 2)` in code):**
  - `roadlab.c` → `make_junction(legs, type)` is the interchange drawer a world calls per grade-separated crossing.
  - `streetlab.c` → `gen_network(pattern, seed)` is the per-region local-street generator a two-tier world calls;
    each network **node is a crossing the junction layer (M1–M3) can render in detail** (the network→junction zoom).
  - ~~A `roadkit.h` becomes worthwhile only if Phase 2 makes both carts share primitives — extract then, not now.~~
    **Superseded (2026-07-01): `roadkit.h` is GO** — the drivable world (`sloop`) is the consumer that needs
    both renderers callable + N-arm-native. Plan: [`roadkit.md`](roadkit.md).

## Pointers

- **Carts:** `tools/carts/{roadlab,streetlab,roadnet,roadnet2,interchange,rampkit}.c`
- **★ interchange map (start here for the ramp work):** [`road-geometry-handoff.md`](road-geometry-handoff.md)
- **Research + layered hierarchy:** [`road-hierarchy-notes.md`](road-hierarchy-notes.md)
- **The DSL / OpenDRIVE recon / geometry refs:** [`interchange-dsl.md`](interchange-dsl.md) ·
  [`junction-lanelink.md`](junction-lanelink.md) · [`road-geometry-refs.md`](road-geometry-refs.md)
- **The working method (build loop + verify gates):** [`roadlab-method.md`](roadlab-method.md)
- **The test harness:** [`spec-harness.md`](spec-harness.md)
