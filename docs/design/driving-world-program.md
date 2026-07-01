# The Driving World — program map

STATUS: LIVING MAP (2026-07-01). **Milestone: P1 landed (OSM half) — `road_at()` drives sloop over real Delft (build a rig, drive real streets that drive grip, crash real buildings). the geometry-first RENDER arc is UNDERWAY in `citydrive` (the close pseudo-3D view where geometry is visible): ✅ connected asphalt surface (casing+fill, hierarchy by width), ✅ canals under roads (flat bridges), ✅ real OSM BRIDGES as raised decks + ✅ TUNNELS dashed (osm-roads now carries bridge/tunnel/layer) — next in this arc: markings → pavements → curb-return junctions via `roadkit.h`, see [`roadkit.md`](roadkit.md). Other open arc: (2) Rung B-proc — the same `road_at()` seam over roadnet2 (the infinite procedural world).** The umbrella over the whole "build a vehicle, drive a procedural world" research — sloop + the road/world/city/render carts as **one program, not a pile of carts.** Sits ABOVE [`road-program-state.md`](road-program-state.md) (which is only the road-geometry tier) and pulls the other layers — movement, the world spine, rendering, real-world data, city content — into one read. Use it to see how far each layer is and **what to finish first.** Update the status table + the "finish first" call when a layer moves.

> Companion to [`big-game-backlog.md`](big-game-backlog.md) (what's left *per cart* + the cross-cutting seams) and [`showreel-teaser.md`](showreel-teaser.md) (the trailer as a forcing function). This doc is the **horizontal map**: the layers, where they converge, and the recommended order. The field note [`field-notes/004-roads-as-convergence-layer.md`](../field-notes/004-roads-as-convergence-layer.md) is the discovery this formalizes.

## The thing being built

A **lo-fi GTA1-but-better** ([`showreel-teaser.md`](showreel-teaser.md), [`second-north-star.md`](second-north-star.md)): an *infinite, deterministic, procedural world you drive around*, assembled from many small spec-locked sandbox carts rather than one monolith. The honest core (per the north star): **author the rules, let the system judge you — nothing faked.** sloop's rig isn't a sprite with stats, it's a grid of parts whose handling is computed; the world isn't a backdrop, it's collidable geometry streamed from a seed.

The discovery in field note 004: **roads became the convergence layer** — the substrate where procedural generation, computational geometry, OSM import, rendering, and (future) traffic all meet. The deeper abstraction is probably **networks** (roads first; rivers/rails/pedestrians/utilities later). This program map is what that convergence looks like as a build plan.

## The stack — every cart + doc, by layer

Seven layers, bottom (what you drive *on*) to top (what you drive *as* / *see it through*). Status: ✅ done · ◑ partial · ○ sketch/seed · ⏸ parked.

### 1. Movement — the rig and the feel
The player and how driving feels. The most mature consumer layer.

| Cart | Status | Role |
|---|---|---|
| **`sloop`** | ◑ deep (rungs 1–2.7 + collidable world + **drives real OSM Delft**) | **The flagship.** Build-a-vehicle-from-parts → drive a chunk-streamed, collidable city. Rigid-body handling computed from the part grid; drift, gears, per-wheel springs; cones/houses/parked-cars resolved by one impulse. **✅ 2026-07-01: `road_at()` now drives it over real OSM data** — drag a `.rvb` and drive real Delft (real streets drive grip, real buildings are solid). Still its own internal world by default; the *shared procedural* spine (roadnet2) is Rung B-proc. Docs: [`sloop.md`](sloop.md), [`sloop-production.md`](sloop-production.md) (the money/process axis, cart `thecut`), [`sloop-handwork.md`](sloop-handwork.md) (the embodied axis, cart `handwork`). |
| **`drivemodes`** | ✅ bench | Driving-feel testbed (steering/grip models). |
| **`trackgen`** | ✅ + traffic | Kart racer; also the **host for `traffic-ai`** (NPC driving prototyped as a TRAFFIC mode). |
| **`outrun`** | ✅ (lineage) | Classic pseudo-3D arcade road — the ancestor of the render layer. |

### 2. The world spine — infinite deterministic generation
The substrate sloop is *meant* to drive. **This is the least-settled layer.** The honest read (2026-06-30): neither `roadnet` nor `roadnet2` is presumed to be the final spine — both have sat untouched for a while, and the answer may be one of them, a merge, or a fresh third path. **Don't discard either yet** — go cold on both before deciding (see §Decisions / P0).

| Cart | Status | Role |
|---|---|---|
| **`roadnet`** | ◑ rungs 1–3 + L2 (blocks/lots) | The spline arterial network over a heightmap — broke procgen-places' "arced roads" wall. Ranked hub/town hierarchy, terrain-aware bends/bridges, an L2 street level in the magnifier. Exposes the **`road_at()` query seam** sloop is meant to consume. Docs: [`roadnet.md`](roadnet.md), [`roadnet-handoff.md`](roadnet-handoff.md) (★ start here), [`roadnet-streetlevel.md`](roadnet-streetlevel.md). |
| **`roadnet2`** | ◑ foundation only | The **vector-native rewrite** — fixes v1's one mistake (two diverging street representations) by going one-graph/one-`road_at`. *This is the intended architecture; only the L0 spline core is ported so far.* Docs: [`roadnet2-plan.md`](roadnet2-plan.md), [`roadnet2-handoff.md`](roadnet2-handoff.md). |
| **`procplaces`** | ✅ v2 (tile-based) | The proven L2 reference generator (roads & cities on a tile grid); `road_at()` = collision==render seam. Frozen reference now. Doc: [`procgen-places.md`](procgen-places.md). |
| **`worldgen`, `gridcity`** | ✅ benches | Supporting world-gen testbeds (heightmap / grid-city scaffolds). |

### 3. Road geometry — the spec-locked sandboxes
**This layer is essentially done.** Full detail in [`road-program-state.md`](road-program-state.md) — don't duplicate; the summary:

| Cart | Status | Role |
|---|---|---|
| **`roadlab`** | ✅ done + spec-locked (25) | Grade-separated **interchanges** — the whole roads.org.uk catalogue from `(legs, type)` at any skew. Seam: `make_junction()`. |
| **`streetlab`** | ✅ junction + network done + spec-locked (104) | **At-grade junctions** (curb returns, turn lanes, roundabouts, free-rights, typed cross-sections) + the **local-street network** (grid/organic/radial/cul-de-sac/superblock, SNDi metrics). Seam: `gen_network()`. |

Ratified by [ADR-0021](../decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md): **geometry is developed in 2D top-down; the pseudo-3D view is a downstream adapter.**

### 4. City content — the fill language
What populates the land between and inside buildings. **Shipped; multi-storey is the open edge.**

| Cart | Status | Role |
|---|---|---|
| **`cityplan`** | ✅ stages A–D | **The forward cart** — the lotfill+interiors merge: a world you lift the roof off by zooming (city of roofs → BSP floor-plans → furniture). Multi-storey tenements = backlog. Doc: [`cityplan.md`](cityplan.md). |
| **`lotfill`** | ✅ (reference) | The fill language *between* buildings (terrain/forest/farm/blocks/lots). Doc: [`streetlevel-content.md`](streetlevel-content.md). |
| **`interiors`** | ✅ (reference) | The fill language *inside* one footprint (R/C/I floor plans). Doc: [`interiors-brief.md`](interiors-brief.md). |
| **`floorplan`** | ✅ | Floor-plan testbed. |

### 5. Rendering — the view onto the world
How the world is *shown*. ADR-0021's "adapter" layer.

| Cart | Status | Role |
|---|---|---|
| **`cityview`** | ✅ bench | The **pseudo-3D city look** — GTA1-meets-Zelda parallel-oblique projection (height pushes pixels up; ground stays plan). Seeded boxes. Doc: [`pseudo-3d-city.md`](pseudo-3d-city.md). |
| **`citydrive`** | ✅ (2026-06-30) · **★ the geometry-first render consumer — arc UNDERWAY** | cityview's **data-driven successor** — same projection/driving, but extrudes REAL OSM footprints (roadview's `.rvb`), drapes over real terrain (DEM), pitched-perspective camera. *Drive a real city in pseudo-3D.* **The road GRAMMAR lands here first** (close/pitched view where geometry is *visible*, not sub-pixel as in sloop's fast drive). **Shipped 2026-07-01:** roads render as one **connected asphalt surface** (casing+fill, hierarchy by width) instead of neon per-class quads; canals draw **under** roads (flat bridges); real OSM **bridges** raise onto decks (ramped, +shadow, `DECK_H` tunable — Delft 3 m) and **tunnels** dash — fed by osm-roads now carrying bridge/tunnel/layer. **Next:** markings → pavements → curb-return junctions via `roadkit.h`. See [`roadkit.md`](roadkit.md). |
| **field-based road rendering** | ✅ opt-in (`DE_FIELD_ROADS`) | One lateral distance field, every feature a threshold — the clean replacement for per-arm casing blits. **Cutover deferred on purpose** until the software canvas makes per-pixel fills cheap. Doc: [`field-based-road-rendering.md`](field-based-road-rendering.md). |

### 6. Real-world data — the world isn't only generated
The transition field note 004 called significant: consuming OSM instead of generating everything.

| Cart | Status | Role |
|---|---|---|
| **`roadview`** | ✅ shipped | Loads real OSM road geometry at RUNTIME (`.rvb` packed binary; Rotterdam 28 MB in ~0.66 s) — swap the file, see a different city. Pipeline: `data-tools/roadview/osm-roads.js`. Doc: [`external-data-carts.md`](external-data-carts.md). |
| **`citydrive`** | (see §5) | The pseudo-3D consumer of the same `.rvb` data. |

### 7. Agents — life on the network (the frontier)
| Cart | Status | Role |
|---|---|---|
| **`traffic-ai`** (in `trackgen`) | ◑ BUILDING | NPC driving — lane-keeping, IDM car-following, overtaking, traffic lights, phantom jams, K-turns. *Only the line they follow changes to wire into the real world* (`cl[]` → `road_at()`). Doc: [`traffic-ai.md`](traffic-ai.md). |
| pedestrians / crowds | ○ seed | `menigte.md` — 10k closed-form NPCs; the on-foot population. |

## Where the layers converge — the seams

The grammar (layer 3) and benches (4, 5, 6) are largely built **in isolation**. The program's real frontier is **composition** — three named seams, none of which any single cart owns yet:

1. **`road_at()` → sloop** *(layer 2 → layer 1)* — the moment the rig drives a world it doesn't itself generate. The backlog calls this the core: *"roadnet2's street tiers + `road_at`→sloop is its core. Everything waits on this."* **✅ Half-built 2026-07-01:** `road_at()` is real in `sloop.c` and drives render + grip + collision over an OSM producer (real Delft). The seam + spatial index + consumer are done; the remaining half is the *procedural* producer — give **roadnet2** a `road_at()` and swap it behind the same seam (Rung B-proc).
2. **The world composer** *(layer 3 → layer 2)* — the two-tier major→minor generator that emits a *different* `(pattern, region)` per place (streetlab's `gen_network`) and `(legs, type)` per crossing (roadlab's `make_junction`), stitched at boundaries. Today one pattern fills the whole view. This is Phase 2 of [`road-program-state.md`](road-program-state.md).
3. **The render adapter over real geometry** *(layer 5 over 2/3/6)* — `citydrive` already proves the pseudo-3D view works over real OSM data; the open question is whether sloop's *player* view becomes this adapter over the *generated* world.

## The convergence — real-in / synth-in / one render

We've been building the world from **two ends**, and they're not rivals — they're the two halves of one missing middle. The grammar end (`roadlab` + `streetlab`) and the data end (`roadview` + `citydrive`) each have exactly what the other lacks:

| | **roadlab + streetlab** (grammar end) | **roadview + citydrive** (data end) |
|---|---|---|
| Source | a seed + parameters | real OSM, loaded at runtime |
| **Knows** | *semantics* — "this is a T-junction, curb-return R, a turn bay, a give-way line, a 2+bike+parking section" | *almost nothing* — "a polyline tagged `residential`" |
| **Has** | toy scale (≈54 nodes), idealized, infinite/deterministic | real scale (Rotterdam ≈420k ways), messy, plausible, immediate |
| Geometry | derived from rules, **correct**, drivable (lanes, turns) | flat ribbons that overlap at junctions — **no junction model, no lanes** |
| Render | 2D top-down ([ADR-0021](../decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md)); pseudo-3D is an adapter | `citydrive` **is** that adapter, proven over arbitrary polygons + DEM |

Bluntly: **`streetlab` understands roads but has no real ones; `roadview` has real roads but understands nothing about them.** Semantics without scale, vs scale without semantics. Neither is a world alone — and that's *why* the spine (roadnet/roadnet2) felt stuck: it's the hard middle that joins them. The grounding is already there — the `.rvb` classifies every way into a real road ladder (highway/arterial/secondary/tertiary/road/service/cycleway/footway + bridge/tunnel/ref), and `streetlab`'s `gen_network()` produces the *same* node/edge graph shape OSM is.

### The four bridges (ranked by leverage)

1. **OSM intersections → `streetlab`'s junction renderer** *(the killer one).* At each real OSM node, read the incident ways' bearings + classes → that's `(legs)` and roughly `(type)`, exactly streetlab's leg-model input. Now a real city renders with real **curb returns, turn lanes, crosswalks, roundabouts** instead of overlapping ribbons. This is the **"network→junction zoom"** already named as Phase-3 step 9 in [`road-program-state.md`](road-program-state.md) — *except the network comes from reality, not a synthetic pattern.*

   **✅ Rung 1 landed (2026-06-30).** Proven in `streetlab` itself: the junction renderer reads bearings only through `leg_bearing()`/`leg_present()`, so an `'o'` "OSM mode" that overrides those with an *independent* per-arm bearing table (skewed 4-way / Y 3-way / acute fork) routes the whole render path unchanged — no geometry surgery, `spec` still 104/0 (default-off). **Key finding:** the per-arm casing path is lumpy at foreign skew angles, but the **field path (`g` / `-DDE_FIELD_ROADS`) renders them clean** — so rung 2+ should render OSM through the field path (this is where bridge #1 meets bridge #4). **Limit hit (as predicted):** `NLEG=4` caps it at 4 arms; a 5/6-way node needs `NLEG` raised + a present-count.

   **OSM pre-labels the roadlab↔streetlab split for you.** roadlab and streetlab aren't merged — they're two specialists under one per-crossing dispatcher (grade-separated vs at-grade; class only *predicts* which). OSM hands you that dispatch decision free: three independent signals say "grade-separated (→ `roadlab`'s `make_junction`)" vs "at-grade (→ streetlab)":
   - **The ramps are tagged.** `highway=*_link` (motorway_link / trunk_link / …) *are* the on/off-ramps and slip roads → those `_link` ways are roadlab's ramp legs.
   - **Grade separation is tagged.** `bridge` / `tunnel` / `layer=±N` mark which segment rides over/under.
   - **Topology is the cleanest tell.** A real intersection is a **shared graph node**; a flyover is two ways that **cross in 2D but share no node** (different `layer`). Crossing-with-a-node → at-grade; crossing-without-a-node → grade-separated. No tags needed.

   **✅ Rung 2 landed (2026-06-30).** Real OSM now flows through: `data-tools/roadview/osm-junction.js` parses a `.rvb` (RVB1/2/3), builds the node graph (quantized shared-coord clustering), and prints each intersection's incident-way bearings + classes. Fed `data/delft-centre.rvb`'s real junctions into streetlab's `'o'` mode — a real skewed 4-way (26/126/198/302°) and a real Y (125/237/342°) render with clean curb returns on the field path. **5-way landed (2026-06-30):** raised `NLEG` 4→6 (normal model capped at `NLEG_PAIR=4` so the 104 specs are untouched) and added Delft's real 5-arm node (`9,85,123,228,304`). It renders **clean on the field path with no rewrite** — `fr_render`/`fr_arm` were already N-arm-native; only the leg-count cap held it back. *This de-risks a big chunk of rung 3:* the field renderer already handles N independent arms; the parts that DON'T are the per-arm path and the bike corner-wrap (see the collinear-pair finding below). Still hardcoded *into* the cart (extractor → paste); the in-cart loader (rung 2b) is deferred per the build-order note below.

   **★ Headline rung-3 finding — the collinear-pair assumption.** streetlab's at-grade machinery assumes a junction is **collinear arm-PAIRS** (two straight streets crossing; the leg model literally pairs `b` with `b+180`). Real OSM nodes are **N independent arms**, and that mismatch surfaces in three places, all the same bug: (a) the **per-arm render path** has no concept of independent arms — it only covers the centre as a side-effect of crossing collinear bands, so toggling the field path off on an OSM node renders garbage (hence OSM mode force-locks the field path on); (b) the **bike corner-wrap** (`corner_bike`) draws one arc per gap keyed to pair geometry → adjacent wraps don't meet at foreign angles, leaving tiny green seam-notches at the wrap apices; (c) the wedge between widely-diverging arms is legitimately grass, not a hole. **The plain junction renders clean on the field path** (no treatments) — the artifacts only appear with bike/turn-lane treatments the OSM extractor doesn't even set. **Rung 3 is therefore not "add OSM to streetlab" but "build an N-arm-native renderer"** (the bike-wrap, the per-arm path, the leg model all want it) — which is exactly when `roadkit.h` should be extracted. *Don't pixel-patch the corner seams on the spike; they go away with the proper renderer.*

   **⚠ Step 0 (for the in-cart loader / grade routing): the current `.rvb` throws three signals away.** `data-tools/roadview/osm-roads.js` collapses `motorway_link` into plain `highway` (`/^(motorway|trunk)(_link)?$/ → 'highway'`) and keeps **no** `bridge`/`tunnel`/`layer`. The raw OSM has it; our importer flattens it. So the spike begins by teaching `osm-roads.js` to *preserve* the `_link` class + `bridge`/`tunnel`/`layer` + the shared-node-vs-crossing topology — then the cart routes each crossing mechanically, because the data already knows the answer. **Two known limits:** dual carriageways arrive as two parallel one-way ways (a "single" motorway = two lines to reconcile), and `_link` ways give ramp *centerlines* but not the interchange *type* (diamond/cloverleaf/stack) — you infer that from how the links wire the carriageways together, which is exactly roadlab's `(legs, type)` classification run backwards.
2. **Real cities calibrate the synthetic generator.** `streetlab`'s SNDi metrics (degree, circuity, sinuosity, node-mix) measure any network; `roadview` is now a free, endless corpus of *real* neighbourhoods. Classify a district → "superblock, circuity 1.6" → the generator emits more like it. Closes field-note-004's "learned generative models" question **without ML** — just metric-matching against reality.
3. **The grammar synthesizes the lanes OSM lacks.** OSM has ~no lane/curb detail; `streetlab`'s typed cross-section (HW = sum of lanes) *infers* a plausible section from the OSM class (`secondary` → 2 lanes + maybe bike). The real city gets lanes you can **drive in, follow with `traffic-ai`, cross on foot** — the thing real data can't give that the grammar can.
4. **One render adapter, two geometry sources.** `citydrive` already projects pseudo-3D over arbitrary polygons + terrain; streetlab/roadlab geometry is 2D-top-down *with pseudo-3D as the adapter* — so the **same** projection renders real OSM *and* synthetic junctions. `cityview` (boxes) and `citydrive` (real) are just two feeds into it.

> **UPDATE (2026-07-01): the trigger fired — `roadkit.h` is now GO, and its first consumer is `citydrive`.**
> The OSM work gave real consumers and the rung-3 finding above pins the N-arm-native renderer as the
> needed piece, so the "not yet" below is superseded. **The grammar lands in `citydrive` first** (the
> pseudo-3D real-OSM view — the close camera where markings/curb-returns are *visible*, not sub-pixel as
> in sloop's fast top-down drive); sloop's rig hooks in later over the shared seam. Decision + the full
> extraction plan (scope, interface, spec-gated phasing, the scale-mismatch reasoning): **[`roadkit.md`](roadkit.md)**.
> The build-order note below is kept for the history of *why* we waited.

**Build order — where the code lives (decided 2026-06-30, now superseded — see above):** *don't extract `roadkit.h` yet.* **Rung 2 stays in `streetlab`** — add the ~30-line `.rvb` reader (from `roadview`) + a node→incident-bearings extractor, feed one *real* node into the existing `'o'` OSM mode, render on the field path. It needs only streetlab's renderer, so extracting now would mean *guessing* the shared interface (the trap `road-program-state.md` warns against: *"extract only if Phase 2 makes both carts share primitives — then, not now"*). **Rung 3 is the real trigger:** routing grade-separated crossings to `roadlab.make_junction()` *and* at-grade to streetlab **from one consumer** is the first moment two renderers must be callable from a third place — extract `roadkit.h` there, designed from a working rung-2 consumer, not from a guess. Keep the rung-2 ingest code fenced (like the `RUNG 1` block) so it lifts out cleanly; if it starts tangling with the spec-locked geometry, that's the early-extract signal.

**The deep version of all four:** a **common intermediate model — a typed centerline network + a junction/cross-section grammar — that both ends emit and one renderer consumes.** A synthetic junction and a real OSM polyline are both "centerline + width + class." [`field-based-road-rendering.md`](field-based-road-rendering.md) already points here (every feature is a threshold on a lateral distance field; it doesn't care whether the centerline came from a seed or from Breda).

### Why this reframes the spine

The thing the world spine should *be* is this common model — **a centerline-network + junction-grammar world, seedable from OSM *or* procedurally.** Both ingest halves exist (`roadview` = real-in, `streetlab` = synth-in) and the render half exists (`citydrive`). roadnet/roadnet2 were attempts at the *generation* half that predate the grammar and the renderer, so they re-invented pieces that now live better elsewhere — which is exactly why "neither makes it as-is." See the reframed **P0** below.

## The missing capability — realistic roadgen (and OSM as its oracle)

The honest gap (named 2026-07-01): **we don't yet have *realistic* procedural roadgen.** roadnet/roadnet2 build networks from a **lattice + jitter + β-skeleton web** — mechanical; it reads as "procedural grid," not "a place that grew." The thing that makes generated roads look real — the **city-growth grammar** (Parish-Müller L-systems, Chen **tensor fields**: arterials bending with terrain/density, local streets filling each district *differently*) — is exactly the **unbuilt Phase-2 piece**. [`road-program-state.md`](road-program-state.md) admits it: streetlab's network is "toy-scale … NOT the actual L-system or tensor-field generators the papers describe." So the unease is real, not a vibe.

**The key insight: OSM is not a way to dodge this gap — it's the way to close it.** Two roles:
1. **Stopgap** — real Delft *is* realistic roads, for free, so you drive something convincing **now** (P1, Rung B-OSM) instead of blocking on the hard generator.
2. **Oracle** — real cities become the **measuring stick** for building realism, with **no ML** (this is bridge #2, promoted to a headline):
   - **Measure** real OSM cities with `streetlab`'s SNDi metrics (degree distribution, circuity, sinuosity, node-mix, block size) via the `roadview` + `osm-junction.js` pipeline. Delft has *numbers* — "circuity 1.4, 60% T-junctions, ~80 m blocks."
   - **Build/tune** the procedural generator (tensor-field / grammar) to **reproduce those statistics**.
   - **A/B** generated-vs-real side by side — drive both, compare the SNDi profiles. The generator is "realistic" when its statistics match a real city's.

   That closes field-note-004's "learned generative models" question *statistically, without ML* — and it's only possible because the OSM pipeline can measure ground truth.

**The reframe this gives:** roadnet2 isn't "the realistic generator we have" — it's the **clean substrate the realistic generator will plug into**, calibrated against OSM. So the story isn't "our roadgen must get good before we can drive"; it's **drive real Delft now → measure it → grow a generator that matches → swap it behind the same `road_at()`.** Same seam (P1), better producer over time. This is Phase-2/Stage-3 work (the world composer); it comes *after* P1 proves the drive-the-real-world seam.

## ★ Finish the right stuff first

The honest read: **the sandboxes are done; the payoff now is integration, not more sandbox polish.** roadlab, streetlab, cityplan, cityview, roadview are all shipped or spec-locked. The risk is endlessly adding junction primitives and metrics (diminishing returns — the grammar is complete) while the three seams above stay open. Recommended order:

**P0 — Define the spine as the common model, not as a salvage of roadnet/roadnet2.** *Blocks layer 2 and seam 1, and it's the genuinely open question.* The reframe (see §Convergence): the spine we want is **a typed centerline-network + junction-grammar world, seedable from OSM *or* procedurally** — and we've already built both ingest halves (`roadview` real-in, `streetlab` synth-in) and the render half (`citydrive`). So P0 is *not* "which old cart survives." It's:
- **(a)** Settle the common model — the centerline + class + junction-node representation that `streetlab`'s `gen_network` output, an OSM `.rvb`, and `citydrive`'s renderer all agree on (bridge #4's intermediate form).
- **(b)** Cold re-read roadnet/roadnet2 (`node tools/orient.js roadnet` / `roadnet2`, then drive them) with one question: *what generation-over-terrain do they uniquely have that nothing else covers?* — the spline-highway-over-heightmap core, the LOD streaming, the terrain-aware bends. **Graft those onto the model; don't ask either cart to be the whole spine.** Don't throw the baby out with the bathwater — but don't crown it either.

The outcome either way: **one spine, one `road_at()`,** fed from a seed or from reality.

**✅ P0 resolved (2026-07-01) — the spine = `roadnet2` (network) + `streetlab` (junctions) + `roadnet`'s recipes (content).** Re-read both carts cold (orient + drive):
- **`roadnet`** (1130 loc) is the *fuller* stack — world + hierarchy + a road graph + an L2 street level (blocks/lots/zones) + a working `road_at()` (`RoadHit`) + magnifier loupes. It has everything sloop needs, **but carries the v1 flaw** (two diverging street representations, field + graph). Content proven; code tangled.
- **`roadnet2`** (580 loc) is the *clean vector-native* restart — spline highway core + β-skeleton web + valley-aware bridges + a drive mode. The **right single-representation architecture, but only the highway tier** (no street/zone/building content yet).

The call: **carry `roadnet2` forward as the network layer** (it's already the clean core with web+bridges+drive). **Supply junctions from a two-specialist dispatcher** keyed on grade (ADR-0021): at-grade crossings → **`streetlab`**'s grammar (now N-arm-native — see the 5-way); grade-separated/motorway-tier crossings → **`roadlab`**'s `make_junction(legs,type)` interchange catalogue. The *same* dispatch is fed from a seed (roadnet2's road classes) or from OSM (`bridge`/`layer`/`*_link` + shared-node topology) — so *one junction layer serves both procedural and OSM nodes* (the convergence). **Regenerate the street content (zones/blocks/lots) on roadnet2's clean graph, using `roadnet` as the reference recipe — not a wholesale port.** Build **`road_at()` on roadnet2's single graph** (roadnet's `RoadHit` shows which fields to expose). **`roadnet` → reference/petri-dish; `roadnet2` → the spine; `streetlab`+`roadlab` → the called junction renderers.** The shared leg/node model + the grade dispatcher are what a future `roadkit.h` should hold (the real Phase-2 extract trigger). So the spine is assembled from pieces already built and proven, not a from-scratch rewrite.

**P1 — Wire `road_at()` into sloop, OSM-Delft first (seam 1). ✅ LANDED 2026-07-01 (OSM half).** *The single highest-leverage move in the whole program* — "the moment the rig drives the real world" (field note 004; backlog "rung 4"). sloop queried its own `hash2(chunk)` world; P1 replaced those queries (render + surface + buildings) with `road_at(world_x, world_y) → { on_road, cls, zone }` fed — today — by real OSM Delft. **The seam, the spatial index, and the whole consumer (render/grip/collision) are built and proven; the remaining half is pointing the same seam at the *procedural* spine (Rung B-proc → roadnet2).** Detail in the rung ladder below.

**The key reframe (2026-07-01):** `road_at()` is producer-agnostic — the common model means it's fed from a **seed OR OSM** interchangeably. So the *first* proof should be the cheaper, more compelling one: **drive the rig around real Delft.** The OSM geometry is real and already loaded (`roadview` reads the `.rvb` into `ways[]`; `osm-junction.js` parses it; `citydrive` proved it drives at scale), and `road_at`-over-OSM is the same nearest-edge query as over roadnet2 — only the data behind the index differs. It's also `sloop` (rig physics) ∪ `citydrive` (real city), and it absorbs the deferred rung-2b in-cart loader.

Rung ladder:
- **Rung A — ✅ landed 2026-07-01.** Added the `road_at(x,y) → RoadHit{on_road, zone, cls}` contract in `sloop.c` (fields mirror roadnet's `RoadHit` so the swap survives), with a **stub body that reproduces sloop's own grid world** (tarmac bands at `ZONE_PITCH ± ZONE_LANE/2`) — so behaviour is unchanged. Proven live three ways: a HUD **ON ROAD/OFF-ROAD** readout at the rig, a deterministic `watch("on_road")` (flips correctly — 272 on / 228 off over a diagonal drive across the grid), and it agrees with the rendered roads by construction. *Scope note:* the seam exists and is proven; `draw_course`/`gen_chunk` still inline their own grid math — they get routed through `road_at()` in **Rung B**, where making it the single source of truth is what lets the OSM body change render + gen + collision together.
- **Rung B-OSM — ✅ landed 2026-07-01. You build a rig and drive it around real Delft.** Commits `b86808b1` (index+query) · `b6a3ebc3` (render) · `f5026eb2` (surface) · `8669dfdc` (buildings) · `7aa0fc8c` (run-path). What shipped, in `sloop.c`:
    - **The spatial index (the crux).** `osm_load()` reads a `.rvb`'s road polylines into segments (real-metre half-width per OSM class) and builds a **uniform-grid CSR index** (32 m cells, 3×3-cell nearest-edge query). `road_at()`'s body swapped from the stub grid to this query: maps sloop pixels→metres (`OSM_PPM`, Y-flip, rig origin = the road nearest bbox centre so you spawn *on* a street) → `{on_road, cls}`. Deterministic; delft-centre = 2.8k segments.
    - **Render (single source of truth).** `draw_course()` now paints the real road network from the same segments as GPU `rectfill_rot` ribbons (coloured by class, hole-free under the heading-up camera), inverse-mapped so **what you see == what `road_at()` says**.
    - **Surface drives handling.** The `GROUND_GRIP` term is scaled per-frame by `road_at(sx,sy).on_road` — on tarmac = full grip; off it = `OFFROAD_GRIP` 0.55 through every friction term + `OFFROAD_DRAG` (≈half top speed). Off-road slides 4× more (measured). OSM-gated, so the default cart's uniform-grip feel is byte-identical.
    - **Buildings (Rung C, below).** OSM footprints are collidable.
    - **Run-path.** Drag a `.rvb` onto the window (`de_dropped_file`, roadview's hooks) → drive that city; `O` reveals the data folder; startup `--data`/`$DE_DATA` honoured. HUD shows the real city name. This *is* the in-cart loader (rung 2b), landed.
- **Rung B-proc — next.** Point the *same* seam at **roadnet2** (give it a `road_at()` = nearest-edge + index over its graph) for the *infinite* deterministic world. The sloop wiring + index + surface + building path transfer straight over; only the producer swaps. This is now the highest-leverage open move.
- **Rung C — ✅ OSM buildings landed (`8669dfdc`).** `osm_load()` fits each `K_BUILDING` footprint an **oriented box** (longest-edge frame) and `gen_chunk()` emits the ones in each live chunk as solid `OB_HOUSE` obstacles — reusing sloop's collision/streaming/demolition path (the rig is physically blocked; total travel dropped ~4× on the same track). *Still open:* **zones** from `road_at`'s `zone` field (roadnet's recipes regenerated) — that's the *proc* side, arrives with Rung B-proc. **Rung D = P2** — NPC traffic follows `road_at` (trackgen's behaviour, `cl[]`→`road_at()`).

**Honest caveat:** OSM Delft is a **fixed bbox** (drive across it, not forever); the infinite north star is roadnet2's job (Rung B-proc). But both feed the *identical* `road_at()`, so OSM-first wasted nothing — the deterministic, per-frame-fast **spatial index is built and proven**, and it's shared by both producers; pointing it at roadnet2 is now mostly wiring. *MVP caveats to revisit:* building boxes are single oriented rects (L-shapes over-cover; a few may clip a street), demolition is fixed 3×3 rubble, and `gen_chunk` linear-scans buildings per chunk-load (fine at delft-centre's 5.3k; a whole-city set wants a chunk-bucket index).

**P2 — Land NPC traffic on the real world (seam 1, layer 7).** traffic-ai's behaviour is built in trackgen; only the followed line changes (`cl[]` → `road_at()`). Once P1 lands, this is mostly a provider swap and makes the world feel *alive*, not empty.

**P3 — The world composer (seam 2 / Phase 2).** Two-tier major→minor generator stitching streetlab patterns per region. **Only after P0–P1** — composing before there's a spine + consumer is premature.

**★ Active track — the geometry-first render, in `citydrive` first (seam 3, decided 2026-07-01).** citydrive proves pseudo-3D over real data; it's the **first home for the road grammar**, because that's the close/pitched view where the geometry is *visible* rather than sub-pixel. **Progress (2026-07-01):** ✅ connected asphalt surface (roadkit Phase 1) · ✅ canals-under-roads · ✅ OSM bridges as raised decks + tunnels dashed (osm-roads carries bridge/tunnel/layer). **Remaining in this arc:** markings → pavements → curb-return junctions (roadkit Phase 2–4). This is where `roadkit.h` gets built and consumed (`streetlab` stays the spec-locked source; `sloop`'s rig hooks in later via the shared `road_at()` seam — cheap because the seam is renderer-agnostic). Plan: [`roadkit.md`](roadkit.md).

**Explicitly defer (don't let these masquerade as progress):**
- More streetlab primitives — staggered junctions, the modal active-travel layer, dendricity metric. The grammar is done; these are sandbox polish. ([`road-program-state.md`](road-program-state.md) tracks them.)
- The field-based road cutover — correctly blocked on `software-canvas.md` (already in flight).
- cityplan multi-storey tenements — nice, but not on the critical path to a drivable world.

## Decisions to make (open forks)

1. **The world spine — what *is* it?** ✅ **Resolved 2026-07-01 (see P0):** the spine = **`roadnet2` (clean vector network: spline core + β-skeleton web + bridges + drive) + a grade-keyed junction dispatcher → `streetlab` (at-grade) / `roadlab` (grade-separated interchanges) + `roadnet`'s street-content recipes regenerated on roadnet2's graph**, exposing one `road_at()`. `roadnet2` carries forward; `streetlab`+`roadlab` are the called junction renderers; `roadnet` becomes a reference/petri-dish (its content is proven but its dual-representation code is not ported).
2. **Is the abstraction "roads" or "networks"?** Field note 004's open question. If networks, the spine's `road_at()` generalizes to rivers/rails/paths — worth knowing *before* P0 locks the data model.
3. **Player view: top-down (sloop today) or pseudo-3D (citydrive's adapter)?** Affects whether seam 3 is on the main path or a side quest.

## Pointers

- **Whole road-geometry tier (layer 3), one screen:** [`road-program-state.md`](road-program-state.md)
- **What's left per cart + the cross-cutting seams:** [`big-game-backlog.md`](big-game-backlog.md)
- **The trailer as a forcing function (the gap list):** [`showreel-teaser.md`](showreel-teaser.md)
- **The convergence discovery (field note):** [`field-notes/004-roads-as-convergence-layer.md`](../field-notes/004-roads-as-convergence-layer.md)
- **The narrative arc (how it grew):** the *"Roads & a world to drive on"* thread in [`history.html`](../history.html) — *"the longest thread in the project"* — seeded from `procgen-places.md`, auto-collecting this whole cluster (`docs/history-spine.json` → thread `roads`).
- **The render policy:** [ADR-0021](../decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md)
- **Carts:** `tools/carts/{sloop,roadnet,roadnet2,procplaces,roadlab,streetlab,cityview,citydrive,roadview,cityplan,lotfill,interiors,trackgen}.c`
</content>
