# cityplan — the lotfill + interiors merge (a world you can lift the roof off)

> **Built in `tools/carts/cityplan.c`.** The unification of the two sibling fill-language
> workbenches: [`lotfill`](streetlevel-content.md) (the world BETWEEN buildings — terrain, forests,
> farms, blocks, lots, streets) and [`interiors`](interiors-brief.md) (the floor-plan INSIDE one
> footprint). They were always the same fill-language at two scales; cityplan stacks the whole stack
> and lets you read it all by **zooming**. `lotfill` and `interiors` stay as the standalone
> workbenches — cityplan is the unified *read*, and **the cart we develop forward** (the parents are
> reference/petri-dishes now).

## The idea

A WORLD selector reads terrain + an urbanization field and picks, per region, what fills it —
**wild** land → scatter (forest/meadow/beach/scree/waste), **farmland** → rows of crops, **city** →
blocks of buildings. Then:

- **zoom OUT** → the world from above: terrain, woods, fields, and a city of **ROOFS**.
- **zoom IN** → each city roof **LIFTS** to its floor-plan (interiors' BSP rooms).
- **zoom IN+** → the **FURNITURE** (beds, sofas, shop shelving, warehouse pallets).
- **`F`** → force every roof off at once (the dollhouse / X-ray look).

The reveal is pure **level-of-detail**: a roof lifts only once its building is big enough ON SCREEN
to read (`b.w * zoom >= MIN_OPEN_PX`), so the wide world never shows a mush of beds — and the
floor-plan (the expensive part) is **generated lazily**, only for buildings you're close to. Far
buildings cost only a roof rect.

Controls: `WASD`/arrows pan · `Z`/`X`/wheel zoom · `R` new seed · `F` lift roofs · `O` field overlay
(world-kind / cover / density) · `G` debug grid.

## The one load-bearing decision: scale

lotfill draws a footprint ~10–18px (block pitch 72); that's far too small to hold a readable BSP
room plan (interiors wants a shell ~90px). cityplan resolves this by working at a **bigger world
scale** (`BLK_P = 230`, footprints ~55–80px) so a lot is **room-capable**, and the RES `min_w/min_h`
is dropped to ~13 so small houses still get a couple of rooms. The wilderness pitches
(`CANOPY_P 18`, `FIELD_P 96`) and all the field frequencies are scaled to match. So you can't see
the whole city AND legible rooms at the same zoom — which is exactly why "leave the roofs on" is the
right idiom: the roof lifts as you descend.

## Architecture (the compose order, top of `tile()`)

1. **terrain ground tint** everywhere (coarse cells, `cover_at` → `ground_col`).
2. **WILD scatter** — ground (flowers/bushes/tufts) then canopy (trees), each gated to
   `world_kind_at == WK_WILD`, per-flavor by cover kind.
3. **FARM rows** — `FIELD_P` parcels where `world_kind_at == WK_FARM`.
4. **CITY blocks** where `world_kind_at == WK_CITY` → `block_lots()` ring-subdivision → per building:
   roof (far) or floor-plan (near) + outdoor yard/driveway, then the **ruins** modifier if decayed.

Seams kept from both parents: pure-fn-of-hash (one block → its lots, one footprint → its whole
plan; every scattered tree is `hash2(cell, seed)`), **draw == query** (`room_at`/`world_kind_at` is
the renderer run as a collision/interaction query), and LOD gates drawing *and* generating.

`block_lots()` / `footprint_body()` are lotfill's improved (2026-06-23) subdivide — varied lot
widths from urban-density, terraced rows (shared party walls), massing classes, yards.

## Staged build plan + status

- ✅ **Stage A — terrain + world selector + scatter + rows** (2026-06-23). cityplan went from
  wall-to-wall city to a real world: elevation/biome cover fields, `world_kind_at` (wild/farm/city),
  the full SCATTER wilderness (woodland/meadow/grass/beach/scree/waste + all instance art), ROWS
  farmland, all composed with the city + roof-lift. Field overlays (`O`: world/cover/density).
- 🔄 **Stage B — ruins modifier** (in progress). `decay`/`fertility` fields + `collapse`/`overgrow`,
  applied per city building over the roof OR the floor-plan; gated by `RUIN_TH` (≈0.58) so ruins are
  **derelict districts**, not universal weathering. HUD `ruin N` tally.
- ✅ **Stage C — border/pave/stamp** (2026-06-23). lotfill's last three atoms, brought in
  *contextually* (not as tabs): `border_edge` = hedge/fence/wall around detached RES lots (per-lot
  style); `pave_interior` = asphalt/plaza/gravel + faint markings on COM/IND block interiors
  (parking lots / squares); `stamp_prop` = fountain/statue/well/obelisk centrepiece in ~1 of 5 block
  courtyards (a plaza or village green). All gated to the outdoor-detail zoom so the far city stays
  clean. **cityplan now carries everything both parents had.**

### Open follow-ups (lowest priority)

- **Terraced party walls**: adjacent terraced shells currently each draw windows on the shared wall
  (no neighbour-awareness). Suppress windows on a wall shared with a touching neighbour.
- **L/U footprints**: `plan`/roof are still rectangular shells (inherited from both parents).
- **Water as a hard constraint for cities**: cities currently only avoid water via `world_kind_at`
  returning wild on water/beach/rock; no quays/bridges (that's roadnet's job per the scope line).
- A `spec()` once the compose logic is worth locking (cart-analyze ranks it).

## Where it came from

Merges [`streetlevel-content.md`](streetlevel-content.md) (lotfill: the fill-language, the 7 atoms,
the phased pipeline, ruins) and [`interiors-brief.md`](interiors-brief.md) (interiors: BSP plans,
emergent room labels, door circulation). Nothing here is new *language* — it's the two existing
languages stacked behind one LOD seam.
