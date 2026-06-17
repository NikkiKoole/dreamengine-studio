# Road hierarchy — research notes (2026-06-17)

> Research-only notes to situate the road/junction work. **Nothing here is built.** Map of where roadlab
> sits in the whole functional road hierarchy and what's still unmodeled. Pairs with the interchange work:
> [`road-geometry-handoff.md`](road-geometry-handoff.md) (the ★ map), [`junction-lanelink.md`](junction-lanelink.md) §9.
>
> Source: a deep-research pass (28 sources, 120 claims, 25 adversarially verified — 24 confirmed, 1 killed).
> **Confidence is marked per section** — three requested topics came back THIN/unverified (flagged below);
> don't treat the thin parts as fact-checked.

## The 30-second version
Every road sits on a **mobility ↔ access continuum**; its position = its functional class. The boundary that
matters for us: **access control.** Freeways are *controlled, ramp-only* → grade-separated interchanges (the
loop/flyover family **roadlab already builds**). **Everything below the freeway tier crosses AT GRADE** → a
fundamentally different primitive set (curb returns, turn lanes, channelization, small radii) we have NOT
modeled. The two unmodeled frontiers are **(A) at-grade intersection primitives** and **(B) local/collector
network-topology patterns**; the **continuity tenet** is the seam between the modeled and unmodeled tiers.

## 1. The spine — functional classification ✓ (high confidence)
FHWA/AASHTO, decades-stable doctrine. Roads serve two opposing functions — **mobility** (few access points,
low friction) and **access** (many access points, high friction) — and the primary purpose defines the class.

| Tier | HPMS classes | Role | Access |
|---|---|---|---|
| **Arterial** | 1 Interstate · 2 Other Freeways & Expressways · 3 Other Principal · 4 Minor | highest mobility | Interstate = **controlled, ramp-only**; principal-and-below allow at-grade + driveways |
| **Collector** | 5 Major · 6 Minor | **balanced** — gathers local traffic, funnels to arterials | at-grade |
| **Local** | 7 Local | highest access, lowest mobility; deliberately discourages through traffic | direct driveway access |

**Key finding:** access control is the line between the grade-separated-interchange world (modeled) and the
at-grade world (unmodeled). FHWA (verbatim): Interstate access is *"controlled or limited to maximize mobility
by eliminating conflicts with driveways and at-grade intersections."*

## 2. Facet A — junctions per level ✓ (high confidence)
At-grade junctions are **a different grammar, not more interchange cells.** Primitives (FHWA Road Diet Guide
§4, NACTO corroborating):
- **TWLTL** (two-way left-turn lane) / raised medians
- **exclusive turn lanes / pockets**
- **single-lane & mini-roundabouts** (≠ the grade-separated roundabout *interchange* roadlab built)
- **curb extensions / bulb-outs**, **pedestrian refuge islands**
- **curb-return radii kept _as small as practical_** — explicitly to SLOW turning vehicles + shorten pedestrian
  crossings (opposite goal from a high-speed ramp)
- **driveways treated as low-volume intersections** (access management)

Verified *why*: at-grade design **slows and channelizes** movements that physically cross; interchanges
**eliminate** the crossing. The loop/flyover grammar does not transfer down — at-grade needs its own primitive
set (corner geometry, stop lines, crosswalks, channelizing islands).

## 3. Interconnection — the continuity tenet ✓ (high confidence)
Locals → collectors → arterials → freeways. FHWA (verbatim): *"a roadway of a higher classification should not
connect to a single roadway of a lower classification"*; *"Arterials should only connect to other Arterials."*
This is exactly why a driveway never attaches to a freeway — and it's the **rule that governs how the modeled
freeway tier attaches to the unmodeled arterial/collector tiers below it.** Critiques exist (fused-grid,
link-and-place, Marshall's movement-vs-place) — see §6, thin.

## 4. OSM taxonomy ✓ — but deliberately NOT 1:1
`motorway > trunk > primary > secondary > tertiary > unclassified > residential > service > living_street` is
an **importance/connectivity** hierarchy (from the British system), not a cross-section one. Approximate map:
motorway=freeway, trunk/primary=arterial, secondary/tertiary=collector, residential/service=local. **US OSM
guidance explicitly warns against a 1:1 mapping** to engineering classes (→ over-classification). `motorway` is
the only level tied to physical construction standards (controlled access, ramps only); `living_street` =
shared space, pedestrians equal/priority; `service` = minimal-mobility terminal access (bottom of the ladder).
**Killed claim (0-3, refuted):** that any controlled-access segment *must* be reclassified `motorway` in OSM.

## 5. Cross-section / lanes ⚠️ (partial)
Lane widths verified (FHWA Road Diet Guide §4): through/turn **10–12 ft** (aux turn lanes seldom <10), TWLTL
**10–16 ft**, bus **11–15 ft**, one-way bike **5 ft** (4 min), parallel parking **8 ft**. These map onto
OpenDRIVE lane types (`driving/shoulder/median/sidewalk/biking/parking/curb`) + `roadMark` — which roadlab
flagged as "worth taking" but never built. **Caveat:** the explicit OpenDRIVE lane-type/roadMark mapping
returned **no verified claim** (sources fetched, not in the verified set).

## 6. THIN / unverified — treat as leads, not facts
The verifier was strict and the key academic sources are books (not web-fetchable). These requested topics
returned **zero verified claims**:
- **Facet B — network topology patterns** (grid / cul-de-sac trees / loop-and-lollipop / fused-grid /
  radial-concentric / organic / superblock and their trade-offs). Flagged as *"the biggest unmodeled gap for a
  procedural generator."* → a focused **B-only deep dive** is the natural follow-up (roadnet2 already dabbles
  in street patterns, so this connects directly to the world step).
- **Games/sims modeling** (Cities: Skylines, SimCity, SUMO/OpenDRIVE as procedural precedent).
- **Staggered junctions & free-right slip lanes** — named in the question, not independently verified.

## 7. The layered map (road class → junction primitives → network pattern → coverage)
| Road class | Junction primitives | Network pattern | roadlab |
|---|---|---|---|
| **Freeway / motorway** (Interstate, Other Freeways & Expressways) | grade-separated interchanges (diamond/cloverleaf/stack/trumpet/fork/triangle/roundabout-interchange), ramp-only, no at-grade conflict | free-flow corridors | ✅ **modeled** |
| **Arterial** (principal / minor) | at-grade signalized crossroads & T, channelized, turn lanes/pockets, TWLTL/median, free-right slips, access-managed driveways | arterial grid backbone | ❌ unmodeled |
| **Collector** (major / minor) | at-grade priority/signal, single-lane & mini-roundabouts, staggered junctions, curb extensions, refuge islands, small curb-return radii | gathers locals → arterials | ❌ unmodeled |
| **Local / residential / service / living_street** | priority/yield/stop T & cross-junctions, driveways, shared space | grid · cul-de-sac trees · loop-and-lollipop · fused-grid · radial-concentric · organic · superblock | ❌ unmodeled (roadnet2 dabbles in patterns) |

**Bottom line:** roadlab built the *top* of the hierarchy precisely. The two unmodeled frontiers are
**(A) at-grade intersection primitives** (a different geometry grammar) and **(B) local/collector network
topology** (the street-web patterns). The continuity tenet is the seam.

## Open questions (carry into a follow-up)
- The network-topology patterns + their connectivity / permeability / cost / intersection-count trade-offs
  (Marshall *Streets & Patterns*, Jacobs *Great Streets*, suburban-vs-grid literature). **← the B deep dive.**
- Precise mapping of at-grade lane composition + junction primitives onto OpenDRIVE lane types / `roadMark`
  and SUMO's junction model.
- How Cities: Skylines / SimCity / OpenDRIVE+SUMO concretely instantiate the hierarchy (auto-generated vs
  authored intersections) — implementation precedent.
- Exact geometry of the named-but-unverified at-grade primitives: staggered junctions, free-right slips,
  splitter/turn channelizing islands, signalized-vs-priority control (AASHTO Green Book intersection chapter,
  NACTO directly).

## Key sources (primary unless noted)
- FHWA Functional Classification — [§2](https://www.fhwa.dot.gov/planning/processes/statewide/related/highway_functional_classifications/section02.cfm),
  [§3](https://www.fhwa.dot.gov/planning/processes/statewide/related/highway_functional_classifications/section03.cfm)
- FHWA Road Diet Informational Guide §4 — <https://highways.dot.gov/safety/other/road-diets/road-diet-informational-guide/4-designing-road-diet>
- OSM — [Key:highway](https://wiki.openstreetmap.org/wiki/Key:highway), [US 2021 Classification Guidance](https://wiki.openstreetmap.org/wiki/United_States/2021_Highway_Classification_Guidance), [US roads tagging](https://wiki.openstreetmap.org/wiki/United_States_roads_tagging)
- OpenDRIVE — [§11 Lanes](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/11_lanes/11_01_introduction.html), [§12 Junctions](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html)
- SUMO — [Road Networks](https://sumo.dlr.de/docs/Networks/SUMO_Road_Networks.html) · [Network Building](https://sumo.dlr.de/docs/Developer/Network_Building_Process.html)
- Network patterns (secondary) — [transportgeography.org street-network-types](https://transportgeography.org/contents/chapter8/transportation-urban-form/street-network-types/), [Fused grid](https://en.wikipedia.org/wiki/Fused_grid), [Road hierarchy](https://en.wikipedia.org/wiki/Road_hierarchy)
