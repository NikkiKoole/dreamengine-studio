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

## Build plan (lens-first, same playbook as L2/L3)

1. **`scatter` atom — forest/meadow** *(chosen first slice, 2026-06-16)*. Highest variety-per-effort,
   covers the rural land that's currently flat green, and proves the jittered-grid determinism rule
   end-to-end. Build in a sandbox lens (`lotfill` cart, or a `procplaces` section) before wiring to
   the real partition.
2. **`rows` atom — corn/fields** — `scatter` with a lattice instead of jitter; reads as countryside
   immediately. Needs the coarse **rural parcel** partition (Voronoi/grid + hedgerow borders).
3. **`footprint` + selector — buildings** — generalise the RES `building_at` prototype via the
   C-table selector (house/shop/big-box from one atom). This is the GTA street the car drives.
4. **`pave` / `stamp`** — parking, plazas, the football field, set-pieces.

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
