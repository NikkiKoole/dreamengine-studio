# street-level content — a fill language for lots & land (design seed, 2026-06-16)

> Where the network stops and the *stuff* begins. [`roadnet-streetlevel.md`](roadnet-streetlevel.md)
> carves the partition (highway→arterial→collector→local→access→lots) and lists **footprints**,
> **park contents**, the **football field** as the open gaps. This doc is the language that fills
> them — *and* the land outside the cities (forest / meadow / field / corn) the road docs never touch.
> Built on the same seams as everything else: pure-fn-of-cell, screen==collision, LOD-is-a-draw-gate.

## The one idea

> **Everything street-level is a *bounded region* + a *seeded fill-rule*, expressed as a pure
> function of `(region, lot-hash, zone)` that answers `draw()` and the collision/query call from
> the same code.**

That's the existing `screen == collision` + locality contracts pushed one tier finer than
`building_at()`. Hold it and infinite + deterministic + zoomable come free, exactly as for roads.

## Atoms vs. biomes vs. recipes — the distinction that decides everything

Three different things, constantly conflated. Keep them separate and the library grows cheaply:

- **Atom = a VERB. *How* you fill a region.** A pure function `fill(region, hash, params) → geometry`.
  The set is small and nearly closed: `scatter / rows / footprint / border / pave / stamp /
  subdivide`. You add one *only* when a fundamentally new fill-verb is missing (see below) — almost
  never.
- **Biome = a NOUN. *What* a place is.** forest, meadow, desert, jungle, dunes, wasteland,
  beachfront, tundra, swamp, urban. A biome is a **value of the terrain/climate fields** (elevation,
  moisture, temperature, urbanization) — the *input* the selector reads. Adding biomes = enriching
  those fields, not the engine.
- **Recipe = the BINDING. Which verbs fill this noun, with which palette/params.** e.g.
  `desert = scatter(cacti, very sparse) + sand base + rare stamp(ruin)`. This is a row in a table.
  **This is where nearly all variety lives**, and it's cheap — params + instance art, no new code-shape.

So **desert / jungle / dunes / wasteland / beachfront are biomes, realised as recipes over the
atoms you already have** — not atoms. If you ever catch yourself writing a "desert atom," that's the
smell: it's a recipe. (`jungle` = `scatter` cranked dense + palm/fern art; `wasteland` = `scatter`
sparse dead shrubs + grey palette; `beachfront` = sand + `scatter(palms)` + a selector rule
"adjacent to water". All existing verbs.)

### The kinds at a glance — what's an atom, and what's "the rest"

The vocabulary grew past three. Here's the **complete set of kinds** and, crucially, **which are atoms
and which aren't** — the thing to be unambiguous about. Only the first two rows are atoms; everything
else is a field, a param, a pass, or plumbing:

| kind | one-liner | atom? | examples |
|---|---|---|---|
| **atom** (fill verb) | pure `fill(region, hash, params) → geometry`; *how* you fill | ✅ **the atom** | `scatter` `rows` `footprint` `pave` `border` `stamp`; `ribbon`/`line` `contour`/`cliff` (flow verbs) |
| **process atom** | an atom that *simulates a force* (iterative, **bounded**, fixed-step, deterministic) | ✅ a *kind of* atom | `erode` `crack` `flow` `spread` |
| **partition generator** | produces/splits **bounded regions** (+ holes) for atoms to fill | ⚙️ plumbing, not a fill | `subdivide` `blob` `carve` `plat` |
| **field (infra)** | a read-only **surface** every atom reads; not a tab | ❌ | `relief` `cover` `density` `zone` `fertility` `taint` |
| **modifier** | reads *already-placed* state and **transforms** it (pipeline phase 5) | ❌ a pass | `age` `collapse` `overgrow` `dress` |
| **composition** | a bounded **mini-driver** recursing into atoms | ~ atom-shaped | `cluster` `square` `quay` `conurbation` |
| **flavor / recipe** | **params** for an atom — what to place, how dense; *never* a new atom | ❌ | woodland/desert/jungle (scatter), house/shop/tower (footprint), `massing` `grove` `relic` `ring` `pool` `hedgerow` |
| **biome** | a **noun** — a value of the fields, realised as a recipe | ❌ | forest, desert, jungle, wasteland, beachfront |

The litmus test, in one line: **if it has its own `fill(region,seed)` it's an atom; if it's a surface
others read it's a field; if it's params it's a flavor; if it reads placed output it's a modifier.**

### Flavors — one atom, many parameter sets

A recipe is realised as a **flavor**: the atom's *params*, not a copy of the atom. You never have
"multiple scatter atoms" — you have ONE `scatter`, pointed at different `(instance palette, density,
clustering)` per biome. Built in `lotfill` (2026-06-17) as a `Flavor` table + a `flavor_for(cover)`
recipe; the one scatter loop reads it:

| flavor | = scatter(params) | cover kind |
|---|---|---|
| woodland | trees, density from forest mask, big in old growth | forest |
| meadow | lone trees + flowers (clumped by colour) | meadow |
| grass | grass tufts, no canopy | grass |
| beach | palms + tufts, sparse | sand |
| scree | rocks, sparse | rock (upland) |
| wasteland | dead trees + scrub, sparse | bare |

Adding desert (cacti) or jungle (dense palms/ferns) = **one row** + maybe one instance art — never a
new function. The rule that keeps it honest: if two "atoms" differ only in a palette and a number,
they're **one atom + two flavors**. (Same shape as `footprint`: house/shop/tower are flavors of one
atom, keyed by zone instead of cover.)

### When something genuinely IS a new atom — the bounded-vs-open rule

An atom must fill a region as a **pure function of `(region, seed)`**. There are two regimes, and the
rule that separates "atom" from "not an atom" is **bounded vs. open**:

1. **Open / streamed** (forest, fields tiled across an infinite world): must be *pure-fn-of-cell,
   NO growth* — a lattice/scatter. A grown, order-dependent, history-carrying process **tears at
   chunk seams** (the node-lattice lesson: *lattice instances are local; grown sets are not*).
2. **Bounded region** (a building interior, one dungeon level, a single maze plot): generated
   **wholesale from that region's single hash-seed**. Inside a bounded region, **growth / recursion /
   backtracking / random-walk are all fine** — the region is self-contained, never spans a seam, and
   one seed makes it deterministic. This is the same footing as `footprint` filling a lot.

So, the worked examples (this is the test to apply to any new idea):

| idea | atom? | why |
|---|---|---|
| **maze** | ✅ atom (bounded-fill) | fills a region with a labyrinth, pure fn of `(rect, seed)` — like `rows` fills with crop rows |
| **room + corridor dungeon** (roguelike) | ✅ atom (bounded-fill) | exactly what fills a *building interior* or a dungeon-level region; generate the whole level from its seed |
| **drunken-walk carver** | ⚠️ a *technique*, not an atom | legal *inside* a bounded atom (the level is computed all at once); **illegal as an open-world cover fill** — it's the canonical grown/seam-tearing process |

The lesson: maze / dungeon are atoms because they're **bounded**. The drunken walk is the *algorithm
inside* such an atom — fine when the region is the unit, fatal when streamed across the open plane.
(A building's floor-plan is literally a bounded room+corridor atom filling the `footprint` rect.)

The only genuinely-missing area-fill *verbs* on the horizon: **`ribbon`** (fill *along a line/spline* —
rivers, trails, a hedge beside a road; dunes' wind-ridges) and maybe **`contour`** (bands following an
iso-elevation — terraces, strata). Everything else tends to be a biome or a recipe.

## Beyond fill: the other atom families, and the build pipeline (proposed)

The 7 built atoms are all **area fills** (write into an empty region). A fuller world needs a few more
*families* — and pricing each against the rules above shows most aren't "new fill atoms" at all. The
proposed set, classified:

| proposed | family | verdict | note |
|---|---|---|---|
| **meander** | flow | ✅ new atom (`ribbon`, organic) | wandering curve. Open-world → **lattice-anchored** (roadnet's spline-between-hash-nodes), free-grown only inside a bounded region |
| **route** | flow | ⚠️ already exists — it's **roadnet** | pathfinding-as-generator = roadnet's network. *Consume* it. Bounded-region route = the dungeon-corridor atom |
| **bridge** | flow | ⚠️ connectivity **rule** (network tier) | fires where `route ∩ water`; roadnet already bridges |
| **pool** | water | 🔁 recipe = `blob` + `relief` | "water settles low" — organic blob gated by elevation |
| **relief** | infra | ✅ **infra field** (not a tab) | like `subdivide`; half-built (`elev_raw`). Promote so atoms *read slope* |
| **cliff** | terrain | ✅ new atom = `contour` | fill along an iso-elevation; reads relief |
| **blob** | area | ↪ **partition** generator (sibling of `subdivide`) | region SHAPE is orthogonal to fill verb — blob makes an organic region, the fills fill it |
| **carve** | area | ↪ partition + modifier (subtractive hole) | a negative region the makers respect; runs as a pass |
| **ring** | placement | 🔁 placement **flavor** | `scatter` with a radial locus (one "place" verb, locus = grid/radial/along-path) |
| **line** | placement | ✅ new atom = `ribbon` (decorate a path) | props along a path/edge; `border` is its perimeter special-case |
| **cluster** | composite | ✅ **new category: composition** | a bounded mini-driver recursing into atoms (hamlet = footprints + a shared green + scatter; it fronts roadnet's road, doesn't make one) |
| **age** | modifier | ✅ **new category: modifier** | reads + transforms existing state; needs a post-process *phase* |
| **dress** | modifier | 🔁 a finishing `scatter` pass (additive) | runs in the modifier phase but doesn't read/transform like `age` |

**Scope line: lotfill FILLS; it never generates connectivity.** Roads, trails, canals, bridges — the
whole route/network tier — are **roadnet's**, full stop. `route`/`bridge` (and `meander`-as-a-road) are
out of scope here; lotfill *reads* roadnet's graph as an input and fills the land around it. (River/
coastline `meander` is a water/terrain feature, not a road — that's worldgen/terrain, also upstream.)

**The structural change this forces — the driver becomes a phased PIPELINE.** Today the WORLD driver is
a single "makers" pass. Modifiers (which read prior output) only make sense as a later phase:

```
1. fields/infra   relief, cover, climate            (read-only)
2. partition      subdivide / blob / carve           → bounded regions (+ holes)
3. connectivity   roadnet's graph + worldgen terrain (INPUT — not generated here)
4. makers         scatter rows footprint pave border line ring cluster
5. modifiers      age, dress                         (read phase-4 output, transform)
```

Determinism survives because each phase is still pure-fn-of-`(region, seed)` reading the *deterministic*
output of earlier phases in fixed order. **Phase 5 is the new concept** and the highest-leverage
addition — it's what turns a tidy map into a lived-in, weathered one. (Everything built so far is phase
4 only.) The new *categories* this reveals: **flow verbs** (meander/line/cliff), **partition generators**
(blob/carve), **composition** (cluster), **modifiers** (age), **infra** (relief).

## The wilds — process atoms, fertility/taint, and ruins

The land *outside* cities and roads adds the **third kind of atom** (the first new kind since
modifiers): a **process atom** generates form by *simulating a force* rather than stamping a shape —
erosion, cracking, viscous flow, creeping growth. They're iterative (CA / downhill-flow /
reaction-diffusion) yet deterministic: pure-fn-of-`(region, seed, input-field)` run for a **fixed** step
count. This is what makes a wasteland read as *weathered* instead of *designed*.

> **The seam rule for process atoms (the one real wrinkle): bounded only.** An iterative sim is *grown*
> — exactly what the open/streamed regime forbids. So `erode`/`flow` over the **open global relief** is
> illegal (you can't iteratively rewrite an infinite shared field seam-free). A process atom **fills a
> bounded region by simulating *within* it**, iterating on its own *local* buffer, reading only phases
> strictly earlier than its own — never the world mid-build. Want erosion everywhere? That's a one-time
> **bake** into the relief field, not a streamed atom. "process" is a *capability*, not a phase slot:
> `erode`/`crack`/`flow` act early (on terrain), `spread`/`overgrow` act late (on placed features).

Two new **fields** carry the whole wilds spectrum (same payoff as `density` for cities — a dial, not a
generator):

- **`fertility`** — the vitality surface. lush ↔ scrub ↔ dead-waste is just *where you sit on it*;
  makers read it for both *density and health* (stunted, sparse, sickly in the troughs). You don't
  build a "wasteland generator" — you dig a trough in `fertility`.
- **`taint`** — an uncanny **overlay** that *warps the content/params* of makers you already have:
  a normal `grove` read through `taint` becomes petrified / bioluminescent / fungal / blighted. Weird
  nature for near-zero cost — pure reuse, no dedicated "weird" atom.

New **modifiers**, and why ruins need them:

- **`collapse`** — partially destroys a `footprint`: removes walls, exposes foundation, spills rubble.
  Genuinely new because **`age` weathers surfaces but can't invent *absence*** — a ruin is a structure
  with pieces *missing*, and only a modifier reading the *original massing* knows what's gone.
- **`overgrow`** — a `spread` aimed at *built* features: ivy-choked walls, roots buckling pavement,
  fields reclaimed by scrub.

> **Ruins = `footprint → collapse → overgrow`, in that order** — the cleanest proof of why the ordered
> pipeline matters. `collapse` reads intact massing to decide removals; `overgrow` reads the *wreckage*
> to creep growth into the gaps. Neither works without phases, and `era`-zoning makes a fresh ruin in
> one ring and a fully-reclaimed one in another from the same atoms.

Reclassified (not new atoms): **`grove`** = a clustered-`scatter` flavor / small `cluster`;
**`hedgerow`** = `border` over a partition's shared edges (the "network" is free from adjacency — `rows`
already emits it); **`relic`** (lone chimney, rotting fence, old well) = a `stamp`/`scatter` flavor,
hinted by a faded `density`.

**Buildable now — the wilds are lotfill's home turf.** Unlike the city set (mostly upstream), almost all
of this needs *no* roadnet/worldgen: `fertility`/`taint` are local fbm fields; `collapse`/`overgrow`
read `footprint` (exists); `grove`/`relic` are flavors; bounded `erode`/`spread` need nothing upstream.
The ruin pair is the highest-value first build — fully local, and it proves phase 5 end-to-end.

## Connectivity: read seams, not generation

lotfill **never generates a road** — that's roadnet's, full stop (see the scope line above). But its
makers constantly *read* roads: a building fronts/sets-back from the street, shops line arteries, density
falls off from them, and "fronts a road by construction" is only *true* if the maker can see where roads
are. So keep the **read seams** the fills depend on, shaped like what roadnet already exposes:

- **`road_at(x,y) → {on_road, class}`** — reuse roadnet/procplaces' *exact* signature; don't invent one.
- **`frontage(lot) → edge/direction`** — which side faces a street (`footprint`'s `outward` is a local
  version already).

Ship a **trivial local stub** behind that seam so the cart runs standalone — `footprint`'s hardcoded
block grid *is* one. Then integration is a **provider swap, not a rewrite** — the same trick as
`cover_at` (local terrain → worldgen) and `world_kind_at` (local selector → real one):

```
road_at   ──(workbench)──▶  lotfill's stub block grid
          ──(integrated)─▶  roadnet's road_at() + graph
```

**Do NOT keep the routing graph** (`reachable(a,b)`, navmesh) — that's roadnet's graph consumed by the
*sim* (sloop/flank), not by fill. A fill atom reads the street; it never plans a trip. Point-query yes,
graph no — that's the line.

## The settlement spectrum (city scale) — integration spec vs. buildable now

A separate proposal worked the *urban* end (density/zone/nucleus/massing/square/quay/era/conurbation).
Two ideas from it are keepers: **`density` is the town↔metro dial** (scale-as-a-field — hamlet→
megalopolis is a `(peak, N)` coordinate, not different generators), and **`conurbation` is to `nucleus`
what `cluster` is to `footprint`** (composition one level up). But most of the machinery **already exists
upstream**, so the proposal is really the *integration spec*, not new lotfill atoms:

| urban piece | already lives in | so it's… |
|---|---|---|
| `density` | procplaces' **intensity** field | upstream (integration) |
| `zone` | procplaces' **land-use** field; lotfill's `zone_at` is a local stub | stub now → procplaces |
| `nucleus` siting | worldgen's **city placement** (terrain-biased) | upstream — *also* the answer to the "siting feedback loop": worldgen's terrain-biased placement IS the acyclic pre-pass; nuclei read relief, everything reads nuclei |
| `artery` / `belt` / tier-rank | roadnet **classes** + node **ranks**; ring family parked | upstream (roadnet) |
| `plat` (block→lot, frontage) | roadnet **L2/L3** subdivision | upstream (roadnet-streetlevel.md) |

### Buildable now in lotfill (pre-integration, on local stubs)

The subset that needs **no** roadnet/worldgen — it runs on the local stubs lotfill already has (`cover_at`,
`elev_raw`, `zone_at`, the block grid). This is the actionable backlog:

- **the phased pipeline + a modifier** (`dress`/`age`) — proves phases 4→5; the lived-in look. *Highest leverage.*
- **`relief` as readable slope** — promote `elev_raw` to a slope the makers read (footprints avoid steep, water pools low, rows terrace).
- **`massing`** — `footprint` reads a local `density` stub → cottage/midrise/tower height. A flavor, not a maker.
- **`square` / `quay`** — composite recipes (carve+pave+ring; water-edge using `cover_at`'s water). Content.
- **`era`** — a modifier *plus* a "distance-from-a-stub-nucleus" ring field that swaps the morphology recipe per ring.
- **`cluster`** — a bounded composite (hamlet = footprints + green + scatter; fronts the road stub, doesn't make one).
- **`blob` / `carve`** partition generators, **`line`/`cliff`/river-`meander`** flow verbs — all terrain/region work, no network needed.
- **the `road_at` / `frontage` read-seam stub** — formalize `footprint`'s block grid behind the seam, so integration is a swap.

Everything network-shaped (`route`/`artery`/`belt`/`bridge`, real `density`/`zone`/`nucleus`) waits for
integration and is consumed, not built, here.

## Two fill-modes (seeing them as different keeps each simple)

The features split cleanly — don't unify them in code, only in vocabulary:

- **Continuous cover** — forest, meadow, scrub, rough grass. **No parcel, no identity.** Answer
  `cover_at(x,y) → {kind, density}` as a *field*; the renderer **scatters** instances
  deterministically per visible cell. You never enumerate a tree — a tree is a pure fn of its
  scatter cell, recomputed locally (the node-lattice lesson: *lattice instances are local
  functions; grown sets are not*). Collision, if any, is against the nearest scattered trunk.
- **Bounded fills** — a house lot, a corn field, a parking lot, a supermarket. **Bounded region
  with identity** (`lot id`). A recipe fills the region. `building_at(x,y) → {solid, lot}` (the RES
  prototype in `roadnet-streetlevel.md` item 5) is the precursor. A corn field is just a *rural
  parcel* whose fill is "rows of a tall crop."

## The three layers (mirrors the interchange DSL exactly)

| interchange DSL | street-content DSL | status |
|---|---|---|
| legs + movements (junction partition) | **partition**: roads→blocks→lots (urban) + coarse Voronoi/grid for rural land → fields/woods | lots ✅; rural partition new |
| **topology layer** — `{movement → primitive}`, declarative | **selector** — `recipe_for(lot) = f(zone, intensity, size, frontage, neighbours, hash)` | new — the "grammar" |
| **geometry layer** — ramp atoms + solver | **fill atoms** — `footprint / subdivide / scatter / rows / border / pave / stamp` | new — the "atoms" |

### Layer 2 — the selector (the clean declarative DSL; emergent, not enumerated)

Authored as **inline C tables** (like `JUNCS[]` and `is_culdesac_district()` — no parser; add a
text format only if worldgen needs to *emit* content). The payoff, in interchange terms:

> A **house**, a **shop**, a **supermarket**, a **cinema** are the *same `footprint` atom* with
> different parameters — chosen by (lot size, zone intensity, frontage role). Just as
> diamond / cloverleaf / stack are the *same legs* differing only in the ramp primitive.

You never write "cinema." You write a rule: *large COM lot + arterial frontage + parking in front
→ big-box footprint + sign band* — which reads as a cinema/supermarket. The types **emerge** from
the selector reading context (the house preference for rules over special cases). This is also what
stops the library homogenising into hand-placed set-pieces.

### Layer 3 — the fill atoms (keep the set tiny, like the 5 ramp primitives)

Each is `atom(region, hash, params) → geometry`: pure, seam-safe, **answers both draw and query.**

| atom | what it does | fills |
|---|---|---|
| `footprint` | setback from frontage + rect/L/U outline + roof tint + driveway gap toward the fronting street | houses, shops, big-box (parameterised by size/frontage) |
| `subdivide` | split a region into sub-lots (the recursive-subdivision contract, reused) | block → building parcels |
| `scatter` | jittered-grid point distribution over a mask, instances from a palette | **forest, meadow, yard trees, street furniture, orchard-as-dots** |
| `rows` | planting lattice + orientation (⊥ to the access track) + crop instance | **corn, ploughed field, vineyard, orchard rows, car-park stalls, cemetery** |
| `border` | stroke the region edge with a hedge/fence/wall instance | field hedgerows, fenced yards |
| `pave` | flat tile-fill with a surface (asphalt/plaza/gravel) | parking lots, plazas, football-field base |
| `stamp` | drop one authored composite at an anchor | fountain, statue, pump (the rare set-piece escape hatch) |

Composition examples (the whole street level is combinations of seven atoms):
- **forest** = `scatter(trees)` — density from a low-freq forest mask, size jitter, clustering via a second mask.
- **meadow** = `pave(grass tint) + scatter(flowers, sparse)`.
- **corn field** = `rows(corn) + border(hedge)`, rows oriented to the access track.
- **supermarket** = `pave(asphalt) + footprint(big) + rows(parking) + stamp(sign)`.
- **house** = `footprint(small) + scatter(1–2 yard trees) + driveway`.

## The seams (load-bearing — restated for content)

1. **Every instance is a pure fn of its own cell/lot hash.** Scatter via jittered grid keyed on
   `hash2(cellx, celly)` — never a grown/accumulated set (would tear at chunk borders).
2. **The selector reads only stable, position-keyed context** — lot-centre hash / `gid`, *never* a
   gather-array index. (The "graph flips on drag" bug, restated: a lot's recipe must not change when
   it scrolls into view.)
3. **Draw and query are the same function in two modes.** `cover_at` / `building_at` / `solid_at`
   are the renderer's code run as a query. LOD gates *drawing*, never *generating*.

## The workbench vs. the world (reusability — built 2026-06-16)

The two wants — *fill a whole world* and *see each step on its own* — are the **same**
architecture, not a trade-off, **if the atom is a pure function and the driver is what
changes.** An atom takes the view rect + inspection flags and reads **no** cam/seed
globals; so the *identical* function runs in two drivers:

- **Workbench** (`lotfill.c`) — one atom on stage, overlays on. The atom REGISTRY is a
  vtable (`atoms[]`, the `procplaces` `gens[]` pattern); each new atom is a TAB. Inspection,
  built into the scatter atom and reusable by every future one: **layer peel** (`1/2/3`
  toggle base/ground/canopy independently, with live placed/culled counts), a **debug grid**
  (`G` draws the scatter lattice + a ring on every density-*culled* candidate — you watch the
  gate accept/reject), and a **field overlay** (`O` cycles cover-kind / density heatmap — the
  field that *drives* the atom). Because the atom is pure, the overlay reads the same values
  the world will.
- **World** (future cart, or sloop) — a thin driver that composes the partition + selector +
  all atoms over thousands of regions. Same atom fns. **What you tune in the workbench is
  bit-for-bit what ships.**

Extraction to `runtime/lotfill.h` (ADR-0006) waits for the **second customer** (the world cart
or sloop's collision) — but atoms are written extraction-ready now: explicit params, no global
reach. The selector (the C tables) stays per-cart; the atoms are the universal unit.

## Build plan (lens-first, same playbook as L2/L3)

1. ✅ **`scatter` atom — forest/meadow** *(built 2026-06-16)*. Highest variety-per-effort, covers the
   rural land that's currently flat green, and proves the jittered-grid determinism rule end-to-end.
   Lives in the `lotfill` workbench.
2. ✅ **`rows` atom — fields/corn** *(built 2026-06-16)*. The first **bounded-fill** atom: a coarse
   square **parcel grid** (the rural partition's MVP, square not Voronoi), each cell planted with rows
   of one crop (plough/corn/wheat/vineyard, per-parcel hash → crop + orientation + spacing) and a
   hedgerow border. `rows_fill(rect, hash, show[])` fills ONE parcel and reads no globals — the
   reusable unit; `rows_draw` just tiles it. *Refinements left:* field-**size** variation (uniform
   grid now), Voronoi/irregular parcels, dirt access tracks, and gating fields to open land via the
   selector (today every cell is a field — partition is the world driver's job, not the atom's).
3. ✅ **`footprint` atom — buildings** *(built 2026-06-17)*. The GTA street. A block grid carves
   streets; each block's **perimeter** lots front a street **by construction** (centre = courtyard/
   parking, so no orphan interiors). One `footprint_fill(lot, outward, zone, hash, show[])` makes
   **house / shop / tower** from a `zone` field (residential/commercial/downtown) + hash — the types
   *emerge*, none are enumerated (the thesis, proven). `outward` = the street side → setback (yard) +
   driveway face that way. *Refinements left:* the real partition (today a uniform block grid tiles a
   city everywhere; WHERE cities sit is the world selector's job, fed by roadnet), irregular/rotated
   lots along curved arterials, `building_at()` as the collision seam for sloop, and merging the RES
   `building_at` prototype from `roadnet-streetlevel.md` item 5.
4. ✅ **`world` driver — composing the atoms** *(built 2026-06-17)*. Not a new atom — the first
   **driver**. A self-contained local worldgen in the `lotfill` workbench: a **selector**
   (`world_kind_at`) reads terrain (the elevation/biome `cover_at`) + an urbanization fbm and picks,
   per region, which atom fills it — **wild → scatter, farmland → rows, town → footprint**. The atoms
   are unchanged (`rows_fill`/`footprint_fill`/`draw_tree` run gated by the selector), proving the
   workbench↔world reuse end-to-end with **no roadnet/worldgen dependency**. Layer-peel toggles the
   three domains; `G` draws the **selector map** (wild green / farm yellow / city red). *This is the
   bridge to the real thing:* swap `world_kind_at` for worldgen's terrain + roadnet's cities and
   `building_at()` for sloop collision, and the same atoms populate the actual game world.
5. ✅ **`border` / `pave` / `stamp` + `subdivide`** *(built 2026-06-17)* — the atom set is complete.
   `border` strokes a region edge (hedge/fence/wall), `pave` flat-fills a surface (asphalt with
   parking stalls / plaza tiles / gravel / sports court), `stamp` drops one authored composite
   (fountain/statue/well/obelisk) at the anchor. Each is a `*_fill(rect, hash, show[])` tiled by a
   shared `demo_grid()` helper — which **is** `subdivide` (splitting the plane into bounded plots),
   so the 7th atom is realised as that helper rather than a separate visual tab. All seven now live
   as workbench tabs (border/pave/stamp) or in-place primitives (subdivide).

## Open decisions (for build time)
- **Scatter distribution:** jittered grid (simplest, seam-safe) vs a relaxed Poisson — lean jittered.
- **Rural partition:** coarse square grid (simplest) vs Voronoi fields (more organic farm shapes).
- **Instance art:** drawn primitives (a tree = trunk line + canopy disc) vs sprite slots — lean
  primitives for MVP (no sheet dependency, recolours via `pal`).
- **Where the selector tables live:** in-cart sections first; extract to `runtime/lotfill.h` on the
  second customer (ADR-0006), the day sloop reads `cover_at`/`building_at` for collision.

Related: [`roadnet-streetlevel.md`](roadnet-streetlevel.md) (the partition this fills),
[`procgen-places.md`](procgen-places.md) (the two-field zone model the selector reads),
[`interchange-dsl.md`](interchange-dsl.md) (the topology+atoms+solver pattern this mirrors).
