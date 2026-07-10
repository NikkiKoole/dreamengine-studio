# Road hierarchy — research notes (2026-06-17)

STATUS: EXPLORING (research 2026-06-17) — the full functional hierarchy; its "unmodeled frontiers" have since been taken up (streetlab, [worldgen-plan.md](worldgen-plan.md)).

> Research-only notes to situate the road/junction work. **Nothing here was built _when written_** —
> but the build has since moved: `streetlab` now models **both** unmodeled frontiers (Facet A at-grade
> junctions, Facet B network topology). For the live build status see
> [`road-program-state.md`](road-program-state.md); this doc remains the research/map underneath it.
> Map of where roadlab sits in the whole functional road hierarchy. Pairs with the interchange work:
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
- **Facet B — network topology patterns** — the first pass returned zero verified claims here. **✅ now
  covered by a focused B-only deep dive → see §8 below** (22 verified claims, strong primary sources).
- **Games/sims modeling** (Cities: Skylines, SimCity) — still thin; the procedural-generation *papers*
  (Parish & Müller, Chen tensor fields, citygen) came through in §8, but the commercial game engines didn't.
- **Staggered junctions & free-right slip lanes** — named in the question, not independently verified.

## 7. The layered map (road class → junction primitives → network pattern → coverage)
| Road class | Junction primitives | Network pattern | roadlab |
|---|---|---|---|
| **Freeway / motorway** (Interstate, Other Freeways & Expressways) | grade-separated interchanges (diamond/cloverleaf/stack/trumpet/fork/triangle/roundabout-interchange), ramp-only, no at-grade conflict | free-flow corridors | ✅ **modeled** |
| **Arterial** (principal / minor) | at-grade signalized crossroads & T, channelized, turn lanes/pockets, TWLTL/median, free-right slips, access-managed driveways | arterial grid backbone | ❌ unmodeled |
| **Collector** (major / minor) | at-grade priority/signal, single-lane & mini-roundabouts, staggered junctions, curb extensions, refuge islands, small curb-return radii | gathers locals → arterials | ❌ unmodeled |
| **Local / residential / service / living_street** | priority/yield/stop T & cross-junctions, driveways, shared space | grid · cul-de-sac trees · loop-and-lollipop · fused-grid · radial-concentric · organic · superblock | ◑ modeled in `streetlab` (grid · organic · radial · cul-de-sac · superblock + SNDi metrics; build status → [`road-program-state.md`](road-program-state.md)) |

**Bottom line:** roadlab built the *top* of the hierarchy precisely. The two unmodeled frontiers are
**(A) at-grade intersection primitives** (a different geometry grammar) and **(B) local/collector network
topology** (the street-web patterns, detailed in §8). The continuity tenet is the seam.

## 8. Facet B — network topology of lesser streets (B deep-dive, 2026-06-17, verified)
A second focused deep-research pass on the street *web* (22 verified claims, 3 refuted, synthesis). Strong
primary sources. **This is the layer ABOVE the at-grade junctions (§2/facet A) and BELOW the interchange tier
roadlab already builds — and it's where roadnet2 already dabbles (β-skeleton highways + street-pattern palette
+ cul-de-sacs), so it connects straight to the seed-driven-world frontier.**

### 8.1 Canonical patterns → empirical types ✓
The named planning patterns (gridiron / dendritic cul-de-sac "loops-and-lollipops" / warped curvilinear /
radial-concentric / organic / fused-grid / superblock) are confirmed as **real, implementable archetypes** —
real networks empirically cluster into a *small* number of types:
- **SNDi 8-type global taxonomy** (Barrington-Leigh & Millard-Ball, PLOS ONE 2019): A = regular grids (most
  connected) → H = least connected; F = dead-end-heavy, G = dendritic (most network bridges).
- **3-type clustering** (Bay Area, arXiv 2025 preprint — corroboration only): Gridded / Orthogonal / Organic.
- These map onto the **generator's named rules**: Basic = organic/natural, New York = grid, Paris = radial-
  concentric, San Francisco = terrain-following.

### 8.2 Metrics — node degree ALONE is not enough ✓ (the key finding)
A pure grid is ~**100% four-way** intersections; **medieval-organic (10%) and cul-de-sac (9%) are nearly
identical on four-way share** despite being connectivity *opposites*. Mean node degree ranks them
(grid **4.0** / medieval **3.1** / cul-de-sac **2.2**) but can't capture tree-likeness. So measure a
**four-measure composite** (→ PCA = **SNDi**, Street-Network Disconnectedness Index; higher = more sprawl):
1. **nodal degree** (mean; plus the **degree-1 / 3 / 4 share** = dead-end / T / four-way distribution)
2. **dendricity** — tree-likeness (branches that don't reconnect)
3. **circuity** — network path vs straight-line distance
4. **sinuosity** — edge length vs end-to-end

Signature per type: **Gridded** = highest degree-4 + concentrated street bearings · **Orthogonal** = highest
degree-3 (T) · **Organic** = highest degree-1 (dead-ends) + dispersed bearings.

### 8.3 Trade-offs ⚠️ (medium confidence — partly contested)
- **cul-de-sac / loop (dendritic):** land/infrastructure efficiency (~16–25% less street land at comparable
  density), perceived safety/sociability.
- **grid:** connectivity, route choice, orientation, walkability.
- **fused grid / superblock:** designed to combine both (pedestrian-permeable, vehicle-discontinuous).

**Honest caveats (the verifier flagged these):** the cul-de-sac advantages rest heavily on **Grammenos/CMHC —
the fused-grid inventor's advocacy paper**, not independent validation; the 16–25% land figure is second-hand
within a wide range (others cite up to 50%); cul-de-sac safety/walkability is genuinely **contested** (Cozens &
Hillier 2008 mixed) — **two stronger safety claims were REFUTED** in verification. The fused-grid mode-shift
(~10% less auto, ~40% more walk/cycle) is a single-neighbourhood **Kelowna simulation**, *relative* not
percentage-points, *work-trips only*.

### 8.4 Procedural generation — the two pillars ✓ (both two-tier major→minor)
Both foundational methods converge on a **two-tier "major-then-minor"** sequence (arterials first, then fill
local streets) — exactly how a seed-driven world would layer onto roadlab's interchange tier.

**Parish & Müller — CityEngine (SIGGRAPH 2001)**, an extended **L-system**:
- decouples production rules from parameter-setting via **`globalGoals`** (objectives: street pattern +
  population density) + **`localConstraints`** (environment: water/parks, self-intersection, mark-failed-
  modules for deletion).
- **two tiers:** highways connect population-density *peaks* globally (each road-end shoots radial rays,
  samples the density map inverse-distance-weighted, grows toward the largest sum); streets fill locally per
  density + dominant pattern, **stop at zero population**.
- named pattern rules **blendable via greyscale control maps** — sum + weight active rules → one generator
  morphs grid ↔ radial across space.

**Chen et al. — tensor fields (SIGGRAPH 2008)**, streets as **hyperstreamlines of a 2D symmetric tensor field**:
- the field's two *perpendicular eigenvector families* = the two dominant street directions.
- user shapes the field (brush strokes, smoothing, constraints, noise, rotation fields) or edits the street
  graph directly.
- **two tiers:** trace the major graph first → its edges + topo boundaries **partition the domain into
  regions** → each region gets its own (possibly discontinuous) field → trace minor/residential streets (so
  minors needn't follow majors).

Other surfaced precedents: **citygen** (Kelly & McCabe, GDTW 2007), **agent-based** growth (Song, PCG Workshop
2019), and the open **probabletrain tensor-field city generator** (itch.io — a readable implementation of Chen).

### 8.5 Implementer synthesis — the morph knobs + how to validate ✓
A single generator can morph **grid ↔ radial ↔ organic ↔ cul-de-sac** by tuning a few knobs:
1. a **pattern-bias field** — greyscale control maps weighting grid/radial/terrain/natural (Parish & Müller)
   OR a **tensor field** (Chen);
2. a **density / threshold** driving where streets branch and where they stop;
3. a **dendricity / loop-vs-cul-de-sac knob** — whether dead-end branches reconnect;
4. a **curvature / noise knob** — warped vs straight.

**Validate** generated output by computing **SNDi's four measures** (node degree, dendricity, circuity,
sinuosity) **+ the degree-1/3/4 share** — these empirically separate every canonical pattern (e.g. grid 100%
four-way vs organic 10% vs cul-de-sac 9%, disambiguated by dendricity/circuity).

## 9. Facet C — the modal (active-travel) network as a PARALLEL layer ⚠️ (concept, 2026-06-23)
Bike paths and footways come in **two fundamentally different forms**, and we've only modelled one:

- **Road-bound (a cross-section attribute)** — a bike lane / sidewalk measured *outward from a road's centreline*
  (`bike_inner()`, the peds sidewalk ring in `streetlab`). It exists *because the road is there*. **Built.**
- **Independent (its own network edge)** — a path with **its own alignment**, that may run parallel to a road at
  an offset (separated by a grass verge), or **wander away from roads entirely** (through a park, along a canal,
  a block cut-through). It has no parent road centreline, so it is **NOT a wider cross-section** — it's a second
  *edge type in its own graph*. **Unmodelled.**

**Key insight: this is a SEPARATE network overlaid on the vehicle network — a different modal layer, NOT a lower
tier of the road hierarchy.** It has its own hierarchy (local path → cycle route → fast cycle route / NL
*snelfietsroute*) and its own junctions (path×road and path×path crossings — for which the M5/Pass-3 zebra +
elephant's-teeth marking primitives already exist).

**The hook is already in the research:** the fused-grid/superblock (§8.3) is defined as *"pedestrian-permeable,
vehicle-discontinuous"* — i.e. **the active-travel graph has MORE edges than the vehicle graph** (cut-throughs,
park links, paths where cars hit a dead-end). A solitary-path layer is the natural co-feature of the superblock,
not a separate effort. (Dutch reality: the parallel *fietsnetwerk* / *solitair fietspad* is arguably more
developed than the road network — this is a first-class layer, not a fringe.)

**Where it belongs:** Facet B / the network layer (`streetlab`'s `v` view), NOT the junction cross-section.
A thin "path edge" (own polyline + width, drawn as its own ribbon with grass verges) that (1) parallels a road
edge at an offset, (2) diverges to its own alignment, (3) crosses roads at its own crossings. The verge-separated
*parallel footway* is the borderline case — it could be a cross-section variant (`verge` lane type + setback)
OR an edge; the *goes-its-own-way* path must be an edge.

## Open questions (carry into a follow-up)
*Facet A (at-grade junctions):*
- Precise mapping of at-grade lane composition + junction primitives onto OpenDRIVE lane types / `roadMark`
  and SUMO's junction model.
- Exact geometry of the named-but-unverified at-grade primitives: staggered junctions, free-right slips,
  splitter/turn channelizing islands, signalized-vs-priority control (AASHTO Green Book intersection chapter,
  NACTO directly).
- **Build-gap ledger (audit 2026-06-24):** which of these primitives are designed-but-unbuilt vs built-but-thin
  vs deferred-by-plan is now tracked in one place → [`road-program-state.md`](road-program-state.md) →
  "Audit (2026-06-24) — the genuine misses". Top buildable miss: the **staggered junction (double-T)**;
  others: standalone **refuge island**, **loops-and-lollipops** pattern, the **dendricity** SNDi measure.

*Facet B (network topology — from the B-dive's own open questions):*
- The **per-pattern numeric metric table** (intersection density/km², link-node/β index, γ index, block size,
  circuity ratio) — the B-dive surfaced the *relative ordering* + the degree-share signatures, not a full
  numbers-per-pattern table.
- How Marshall's route-structure characteristic types + the transportgeography.org taxonomy **map onto** the
  SNDi 8-type / the 3-type clustering — and whether they add patterns (tributary vs distributor) the empirical
  clusterings miss.
- ✓ **A concrete superblock / fused-grid generation algorithm** (perimeter arterial loop + calmed/discontinuous
  interior) — neither Parish & Müller nor Chen model the deliberate permeable-but-discontinuous topology, so an
  original bit: BUILT in `streetlab` (2026-06-23) as a continuous arterial grid + a Kruskal spanning forest of
  cul-de-sac stems inside each cell, proven distinct by **circuity** (the heavier 3rd SNDi measure, also built).
  See [`road-program-state.md`](road-program-state.md). *Still open below:* the per-pattern numeric table + SOTA.
- **State of the art beyond the 2001/2008 pillars** — OSM/example-driven synthesis, learned generative models —
  and whether any give better control over the grid↔organic↔cul-de-sac morph than greyscale-blend / tensor fields.

## Seminal works — the "great papers/books"
The foundational academic references for the network-topology + procedural-generation frontiers.
**✓ = verified + linked by the B-dive; ◦ = named in the brief but not surfaced/verified (still canonical).**
- ✓ **Parish & Müller, "Procedural Modeling of Cities," SIGGRAPH 2001** — origin of CityEngine; the extended
  **L-system** with `globalGoals`/`localConstraints`, two-tier highway→street growth, blendable named pattern
  rules. *The* procedural-streets paper. [ACM DOI](https://dl.acm.org/doi/10.1145/383259.383292) ·
  [PDF (ResearchGate)](https://www.researchgate.net/publication/220720591_Procedural_Modeling_of_Cities)
- ✓ **Chen, Esch, Wonka, Müller, Zhang, "Interactive Procedural Street Modeling," ACM TOG / SIGGRAPH 2008** —
  **tensor-field** street generation (streets as hyperstreamlines; morph grid ↔ radial ↔ organic by editing the
  field). [ACM DOI](https://dl.acm.org/doi/10.1145/1360612.1360702) ·
  [project page](https://www.sci.utah.edu/~chengu/street_sig08/street_project.htm)
- ✓ **Barrington-Leigh & Millard-Ball, "A global assessment of street-network sprawl," PLOS ONE 2019** — the
  **SNDi** four-measure framework (nodal degree + dendricity + circuity + sinuosity) + the empirical 8-type
  taxonomy. The metric backbone for *validating* generated networks.
  [PLOS ONE](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0223078)
- ✓ **Grammenos et al. / CMHC — "Residential Street Pattern Design" (the Fused Grid)** — the pedestrian-
  permeable / vehicle-discontinuous hybrid + the land-efficiency argument (read as advocacy — see §8.3 caveat).
  [PDF](https://www.irbnet.de/daten/iconda/CIB4226.pdf)
- ✓ **Kelly & McCabe, "Citygen" (GDTW 2007)** — an open road-network generation system. [PDF](https://www.citygen.net/files/citygen_gdtw07.pdf)
- ◦ **Stephen Marshall, *Streets & Patterns* (Spon/Routledge, 2005)** — the route-structure typology +
  movement-vs-place framing. *(Named in the brief; not surfaced as a fetchable verified source — get directly.)*
- ◦ **Allan B. Jacobs, *Great Streets* (MIT Press, 1993)** — the one-square-mile figure-ground street maps. *(Not surfaced.)*
- ◦ **Hillier & Hanson, *The Social Logic of Space* (Cambridge, 1984)** — space syntax, basis for connectivity metrics. *(Not surfaced.)*
- ◦ **Southworth & Ben-Joseph, *Streets and the Shaping of Towns and Cities* (1997/2003)** — grid → cul-de-sac evolution. *(Not surfaced.)*

## Key sources — verified in the first deep-dive (primary unless noted)
The sources behind the verified findings above, grouped by topic. Quality tier per the research pass.

**Functional classification (the spine) — primary**
- FHWA Functional Classification — [§2 concepts](https://www.fhwa.dot.gov/planning/processes/statewide/related/highway_functional_classifications/section02.cfm)
  · [§3 the classes + access](https://www.fhwa.dot.gov/planning/processes/statewide/related/highway_functional_classifications/section03.cfm)
- [MnDOT — FHWA Functional Classification Guidelines (PDF)](https://www.dot.state.mn.us/planning/program/pdf/FHWA%20Guidelines.pdf)
  · [PennDOT — 2023 FHWA Functional Classification Guidelines (PDF)](https://gis.penndot.pa.gov/BPR_pdf_files/Documents/Traffic/Highway_Statistics/2023_FHWA_Functional_Classification_Guidelines.pdf)

**At-grade junction geometry (facet A) — primary** (the standout: MoDOT's stop/yield chapter)
- [FHWA Road Diet Informational Guide §4](https://highways.dot.gov/safety/other/road-diets/road-diet-informational-guide/4-designing-road-diet) — turn lanes, TWLTL, curb radii, access mgmt, lane widths
- [MoDOT EPG 233.2 — At-Grade Intersections with Stop & Yield Control](https://epg.modot.org/index.php/233.2_At-Grade_Intersections_with_Stop_and_Yield_Control) — the at-grade geometry reference
- [FHWA — Human Factors of intersection design (01103)](https://www.fhwa.dot.gov/publications/research/safety/humanfac/01103/ch1.cfm)
- *Attempted but didn't fetch cleanly (get directly next time):* NACTO Urban Street Design Guide (intersection elements), AASHTO Green Book intersection chapter, Louisiana DOTD Ch.6 At-Grade Intersections.

**OSM taxonomy — primary**
- [Key:highway](https://wiki.openstreetmap.org/wiki/Key:highway) · [Highway classes](https://wiki.openstreetmap.org/wiki/Highway_classes)
  · [US 2021 Classification Guidance](https://wiki.openstreetmap.org/wiki/United_States/2021_Highway_Classification_Guidance) · [US roads tagging](https://wiki.openstreetmap.org/wiki/United_States_roads_tagging)

**Cross-section / lanes + sim precedent — primary**
- OpenDRIVE — [§11 Lanes](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/11_lanes/11_01_introduction.html)
  · [§12 Junctions](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html)
- SUMO — [Road Networks](https://sumo.dlr.de/docs/Networks/SUMO_Road_Networks.html) · [Network Building Process](https://sumo.dlr.de/docs/Developer/Network_Building_Process.html)

**Network topology (facet B — verified by the B-dive; see §8 + Seminal works for the core papers)**
- *Core papers (links in Seminal works):* Parish & Müller 2001 · Chen et al. 2008 · Barrington-Leigh &
  Millard-Ball 2019 (SNDi) · Grammenos/CMHC fused grid · Citygen 2007.
- [arXiv 2511.06747 — Bay-Area intersection-pattern clustering (2025 preprint)](https://arxiv.org/pdf/2511.06747) — corroboration only, not peer-reviewed
- [Song — agent-based street generation (PCG Workshop 2019)](https://pcgworkshop.com/archive/song2019agentbased.pdf) · [probabletrain — tensor-field city generator (itch.io)](https://probabletrain.itch.io/city-generator)
- [transportgeography.org — street-network types](https://transportgeography.org/contents/chapter8/transportation-urban-form/street-network-types/) · [graph-theory measures/indices](https://transportgeography.org/contents/methods/graph-theory-measures-indices/) (secondary)
- [CNU — Street Networks 101](https://www.cnu.org/our-projects/street-networks/street-networks-101) · [Fused grid](https://en.wikipedia.org/wiki/Fused_grid) · [Road hierarchy](https://en.wikipedia.org/wiki/Road_hierarchy) · [Access management](https://en.wikipedia.org/wiki/Access_management) (secondary)
- [Rahman et al. — fused-grid mode-shift simulation (Travel Behaviour & Society 2019)](https://www.sciencedirect.com/science/article/abs/pii/S2214140519302269) (primary; simulation — see §8.3 caveat)
