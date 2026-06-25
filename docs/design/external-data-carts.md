# External data → carts (EXPERIMENT)

**Status:** experimental, not committed API. The question being probed: can a cart load a
data blob at **runtime** instead of baking the data into its C source? If this proves its
worth it graduates; if not, the pieces below delete cleanly (they touch almost nothing).

## Session handoff — pick up here

**What's working and committed** (`roadview`, registered as a tech-demo/toy):
- The whole pipeline: `osm-roads.js` (`--demo` / `--bbox` / `--place`, mirror-fallback) → a
  `data/<slug>.rvb` → `roadview` loads it at runtime via `de_data_path()` + `runtime/json.h`.
- **Packed binary format (`.rvb`) — the load is now ~instant.** Real cities are tens of MB of
  JSON and tokenising that text (jsmn + `strtod` per coordinate) was the bottleneck: **Breda
  (27 MB) took >2 min, Rotterdam timed out.** `osm-roads.js` now emits a packed binary by
  default (magic `RVB1`); the cart auto-detects it by magic bytes and walks it with `memcpy`.
  **Rotterdam (28 MB / 2.8 M points / 420 k ways) now loads + renders in ~0.66 s, fully (no
  truncation).** `.json` still works (drag-drop accepts either); `--json` emits the readable IR.
- **Layers** (bottom→top): zoning blocks (farm→residential→commercial→industrial→parking→sand)
  → green (parks/woods) → water → building footprints → rail → canals → coast → roads → tree
  dots. SimCity palette: residential = green, commercial = blue, industrial = yellow, farm =
  brown, parking = grey. Footprints get a **1px outline** (separable for a downstream consumer
  that extrudes buildings; the JSON also carries the OSM `building=*` type in a `sub` field).
- **Load UX:** default-loads `data/demo.rvb`; **drag a data file onto the window** to switch
  towns (`de_dropped_file()`); **`OPEN` button** reveals the `data/` folder (`de_open_path()`).
- Footprints (`B`) and tree dots (`T`) are **LOD-gated** (drawn only zoomed in). Pools sized for
  a whole big city (`MAXPOLY` 450k / `MAXPTS` 3.2 M); a `(truncated)` HUD flag fires if a dataset
  still overflows instead of silently clipping.
- **Harness wheel injection** (separate, general win): a `wheel <frame> <delta>` script directive
  drives `mouse_wheel()` under `--script`/`--replay`, so roadview's zoom is now scriptable for
  golden clips (commit `ab828136`).
- Commits this session (newest last): `ab828136` (harness wheel), `09749264` (SimCity zoning +
  outlined footprints), `bceda9a9` (bigger pools + truncated flag), `a7519538` (the `.rvb` binary
  format). Engine hooks are tiny EXPERIMENTAL additive functions in `studio.c`/`.h`; `data/` is
  git-ignored (so a fresh clone has no `demo.rvb` until you run `--demo`).

**Quick resume:**
```bash
node tools/osm-roads.js --place "Utrecht, Netherlands"        # → data/utrecht-netherlands.rvb
node tools/osm-roads.js --convert data/breda-netherlands.json # repack an existing .json → .rvb (no re-fetch)
DE_DATA="$(pwd)/data/delft-netherlands.rvb" node tools/play.js roadview run   # or just: roadview run (demo), then drag a file
```
> **Gotcha that cost an hour:** `play.js` builds `build/roadview-dbg`, **not** `build/cart`.
> Time/test against `build/roadview-dbg` (or just run through `play.js`); a stale `build/cart`
> will silently render the old behaviour.

**Open / next (none started):**
- **Hover inspector panel.** Surface what's under the cursor — street name + type (e.g. "A13 ·
  highway", "Stationsweg · arterial", "apartments"). The data is already there: every feature
  carries `name`, and buildings carry the `sub` type; we just don't surface either yet. Needs a
  cheap hit-test (point→nearest polyline / point-in-polygon over the on-screen features, probably
  gated to the zoom where labels make sense) + a small `mouse_x/y` panel. Biggest UX win on the list.
- **Breda framing.** The data bbox balloons (Breda → 53×46 km) because a few long ways (a canal,
  a road, a water body) have one node in the query area but their full Overpass geometry runs to
  the corners — so the city renders as a small off-centre blob. Fix: clip way geometry to the
  query bbox in `osm-roads.js`, or fit the cart to a percentile/building extent.
- **Two-line legend.** With the zoning classes added, the bottom legend overflows one row — it
  currently hard-stops at the screen edge (`if (x > SCREEN_W - 24) break;` in `legend()`), so the
  last classes (rail, sand, …) don't show. Wrap to a second row (bump the black bar to ~18px,
  track an `x`/`y` cursor, wrap when `x` exceeds the width).
- **More road tiers.** See "The road hierarchy" below — we collapse OSM's ~20 `highway=*` values
  into 4 painted tiers. Cycleways (huge in NL) and service roads are the obvious teases-out; the
  `primary`/`secondary` split is why a motorway can look like it has gaps (it continues as yellow
  arterial). Adding a tier is a ~10-line pattern: classify in `osm-roads.js` → a `K_*` + style in
  `roadview.c` → `fill_areas()` or `draw_class()` in the paint order.
- Plus the three "to graduate" items below (editor `--data` wiring, web/wasm file loading,
  whether `fmltools` should retarget this schema).

## The road hierarchy — what we show, collapse, and drop

OSM's `highway=*` key is a ~20-value functional classification. `osm-roads.js` collapses it into
**4 painted tiers** (colour + real-world half-width in metres, so they fatten as you zoom):

| tier (cart) | colour | half-width | OSM `highway=*` values folded in |
|---|---|---|---|
| `highway`  | orange | 7 m | `motorway`, `trunk` (+ `_link` ramps) |
| `arterial` | yellow | 5 m | `primary`, `secondary` (+ `_link`) |
| `road`     | grey   | 3 m | `tertiary`, `unclassified`, `residential`, `living_street`, `service`, `road` (+ `_link`) |
| `track`    | brown  | 1.5 m | `track`, `path`, `footway`, `cycleway`, `bridleway`, `steps`, `pedestrian` |

Two consequences worth knowing:
- **The `primary`/`secondary` → arterial split is why "the highway" can look broken.** Where a
  `motorway`/`trunk` (orange) continues as a `primary` ring road (yellow), the orange visibly
  stops — nothing is dropped, it's recoloured. (Breda: 391 highways vs **1015 arterials**.)
- **Non-car ways are all one brown `track`.** `cycleway` / `footway` / `pedestrian` / `path` /
  `steps` are merged — in cycling-dense places (NL) splitting out a CYCLEWAY tier would add a lot.
  Service roads (driveways, parking aisles, alleys) are merged into `road` and add clutter.

**Dropped entirely** (`classifyWay` returns `null`): the niche/non-routable `highway=*` values —
`construction`, `proposed`, `raceway`, `escape`, `busway`/`bus_guideway`, `corridor`, `elevator`,
`platform`, plus point-only tags (`crossing`, `traffic_signals`, `street_lamp`, `turning_circle`).
Non-`highway` ways we *do* show as their own classes: `rail` (dashed), `canal`, `coast`.

## Data we could pull (OSM has much more)

We currently tap a slice of OSM. The rest, organized by how the cart would draw it — each add is
the same ~10-line pattern (classify in `osm-roads.js` → a `K_*` + style in `roadview.c` →
`fill_areas()`/`draw_class()` in the paint order), *except* the relation gap at the bottom.

**Area fills** (`fill_areas` path) — *have: water, green, building, sand, residential, commercial,
industrial, farm, parking*
- `aeroway=aerodrome / apron / runway` — **airports**, instantly recognisable, currently invisible
- `natural=wetland / heath / bare_rock / scree / glacier` — terrain beyond water/sand
- `landuse=quarry / military / landfill / religious / greenhouse_horticulture / plant_nursery`
- `leisure=stadium / sports_centre / swimming_pool / marina / playground`
- `amenity=school / hospital / university` grounds (civic land — a campus reads as a block)
- `boundary=national_park / protected_area`

**Lines** (`draw_class` path) — *have: highway, arterial, road, track, canal, coast, rail*
- `aeroway=runway / taxiway` — airport strips (very legible)
- `barrier=city_wall / wall / hedge` — **city walls**, great for historic European towns
- `boundary=administrative` (dashed) — the **municipal outline** (also helps the Breda framing fix)
- `power=line` / `man_made=pipeline`; `natural=cliff / tree_row`; `route=ferry`

**Points** (like `tree` — dots or small glyphs; these power the hover panel best) — *have: tree*
- `railway=station / tram_stop / subway_entrance` — **transit stops**
- `amenity=*` (hospital/school/place_of_worship/fuel/pharmacy/…) and `shop=*`
- `man_made=tower / lighthouse / windmill / water_tower` — **landmarks**; `historic=castle / monument / ruins`
- `natural=peak` (elevation), `tourism=hotel / museum / attraction`
- `place=city / town / suburb / neighbourhood` — **district name labels**

**Attributes we currently discard** (no new query — they ride on features we already fetch; worth
keeping in the schema, likely alongside `sub`)
- **`building:levels` / `height`** — the big one for the *extrude-buildings-downstream* goal: keep
  real heights, not just footprint + type.
- Roads: `ref` (road number "A13", for shields/labels), `bridge` / `tunnel` (draw tunnels dashed,
  bridges with casing), `oneway`, `maxspeed`, `surface`, `lanes`.
- `name` on waterways (river names), `addr:*` on buildings.

**Structural gap — relations.** `osm-roads.js` reads only **ways + `node[natural=tree]`**, not
**relations**. Big areas modelled as multipolygon relations (a forest with a clearing, a lake with
an island, the city boundary itself) are **silently missed**, not just simplified. Assembling
multipolygons (`out geom` on relations + ring-stitching) is the one non-trivial add here, and it's
why some large green/water areas can look absent.

**Rough priority for a broad-strokes map:** building heights (serves extrusion) → airports →
transit stops + POI points (also make the hover panel rich) → place-name labels → city walls /
admin boundary.

## Building heights (for the extrude-buildings-downstream goal)

Two ways to get them, cheapest first:

**A. OSM `height` / `building:levels` ride-along** (no new query — we already fetch building tags).
- In `osm-roads.js`, when `kind==='building'`: parse `tags.height` (metres, e.g. `"12.5"`), else
  fall back to `tags['building:levels'] * 3` (≈3 m/storey). Stash it on the feature as a new `h`
  field (next to `sub`); thread it through `writeBin` (one more `float32`/building) and the `.json`.
- The cart ignores `h` for now (like `sub`) — or could shade footprints by height; the downstream
  extruder reads it directly.
- **Caveat: partial coverage.** Many buildings have neither tag → `h=0`/unknown; downstream needs a
  fallback (a default storey height, or skip-if-unknown). Coverage varies wildly by region.

**B. 3DBAG normalizer** (NL-only, authoritative — a *new* per-source normalizer, the bigger follow-up).
- The Netherlands' authoritative source is **3DBAG** (`3dbag.nl`): LoD1/LoD2 building models with real
  heights from AHN lidar + the BAG registry — near-complete coverage, far better than OSM height tags.
- This is its own tool — `tools/bag-buildings.js` → the *same* `.rvb`/"vector features" schema (with
  `h`) — exactly the "one cart, per-source normalizer" pattern (see "The pattern" above). A Dutch city
  would look dramatically better from 3DBAG than from OSM tags.
- Not OSM/Overpass, so a separate fetch/parse (3DBAG ships GeoPackage / CityJSON tiles); the messy
  source-specific work stays in the normalizer, the cart still only sees the normalized schema.

## The smell this fixes

`floorwalker` and `seinelaan` are the *same cart* — identical movement, rendering, collision —
with a different floorplan **baked into the source**. `fmltools/fml2cart.js` regenerates the
`.c` per `.fml`, splicing a C data block between `/* >>>FML_DATA */` sentinels. Two datasets →
two near-duplicate carts. N datasets → N carts. That's data masquerading as code.

The road experiments have the same shape: real OpenStreetMap roads are *data*, and we'd rather
not mint a cart per city.

## The pattern

> One cart + a per-source normalizer that emits a shared JSON; the cart loads the JSON at
> runtime via `--data <file>`. Swap the file, not the source.

```
  source            normalizer (tools/)          shared JSON          cart (runtime)
  ──────            ───────────────────          ───────────          ──────────────
  OpenStreetMap  →  tools/osm-roads.js        →  features blob   →    roadview.c
  Floorplanner   →  (fmltools, could target ───┘   (same schema)      (--data <file>)
                     this schema instead of baking)
```

The messy, source-specific work (OSM tag→class mapping, lat/lon projection; or FML
door-splitting, mm scaling) stays in the per-source normalizer. The cart only ever sees the
normalized schema. **Don't build a generic any-JSON→C importer** — the value is in the
per-source normalization, not the emit step; a generic tool saves ~20 lines and can't do the
domain work anyway.

## The shared schema — "vector features"

A trimmed, already-projected feature collection. Geometry is pre-projected to **local metres**
(origin at the SW corner, Y north-up) so the cart needs zero geo math — it just fits the bbox
to the screen.

```json
{
  "source": "overpass",
  "name": "Delft centre",
  "bbox": [0, 0, 2782.9, 2671.0],
  "features": [
    { "kind": "highway",  "name": "A13", "pts": [x0,y0, x1,y1, x2,y2, ...] },
    { "kind": "building", "name": "", "sub": "apartments", "pts": [ ... ] }
  ]
}
```

- `kind` ∈ **lines** `highway | arterial | road | track | canal | coast | rail` (rail is drawn
  dashed) · **filled areas** `water`, `green` (parks/woods), `building` (closed rings via
  `polyfill`), `sand`, and the SimCity-style **zoning blocks** `residential | commercial |
  industrial | farm | parking` · **point** `tree` (one point — `osm-roads.js` reads
  `node[natural=tree]`, the one place it processes nodes, not ways).
- `sub` (optional) is a refining OSM tag — currently the `building=*` type (house / apartments /
  industrial / …), carried for a downstream consumer that extrudes individual buildings. The
  roadview cart ignores it.
- `building` footprints draw with a **1px outline** (so each stays separable); footprints (`B`)
  and `tree` dots (`T`) are **LOD-gated** — only drawn once you zoom in past `BUILD_GATE_PPM` /
  `TREE_GATE_PPM`. Everything else draws at every zoom (zoning blocks are the ground layer).
- `pts` is a **flat** `[x0,y0,x1,y1,…]` polyline in the bbox's metre frame.
- Floorplans (see "Floorplanner — implemented" below) reuse the *pattern* but **not** this exact
  schema: a top-down floor needs furniture sprites, oriented-box collision, and door-swing markers
  that a flat polyline IR can't carry, so `floorplan.c` has its own richer schema. The shared idea
  (one cart + a per-source normalizer emitting runtime data) is what carries over, not the field list.

## Floorplanner — implemented

`floorwalker` + `seinelaan` (the baked duplicates above) now have a runtime twin:

- **`fmltools/floorplanner.js -pid=<projectid>`** — fetches a project's `.fml` from the v2 API
  (`/projects/<id>/download_json.fml`; auth via `$FP_AUTH_TOKEN` JWT or `$FP_SESSION` cookie),
  then emits `data/floorplan/<id>.json`. `--baked` instead runs the old `make-floor.sh` per-project
  bake (option A); the default is the runtime data file (option B).
- **`tools/carts/floorplan.c`** — the shared cart. Loads the data via `de_data_path()` / `$DE_DATA`
  (drag a `.json` onto the window to swap plans), renders identically to `floorwalker`. Default
  (editor, no `--data`) loads `data/floorplan/demo.json` (Seinelaan).
- The normalizers gained a `--json` path: `fml2cart.js --json` writes geometry; `fml-sprites.js
  --json` fills `sprites[]` from the CDN furniture bake. Same numbers as the baked C, just serialised.

Schema (floorplan-specific; engine-pixel coords, palette colour indices):
```json
{ "name", "scale", "w", "h", "spawn":[x,y],
  "walls":[ax,ay,bx,by,thick, ...], "windows":[...same...],
  "doors":[cx,cy,w,rot, ...], "furn":[cx,cy,w,h,rot,ref, ...],
  "areas":[{"c":colorIdx, "poly":[x,y,...]}, ...],
  "sprites":[{"w","h","px":[idx, 255=transparent]}, ...] }   // furn.ref indexes sprites[]
```

### Floor materials — what the `.fml` actually carries (open)

The geometry is rich, but **floor finishes are only partly used**:
- `design.areas[]` carry a flat `color` (usually a grey room-outline tone like `#727272`).
- `design.surfaces[]` carry the *real* floor finishes: each has a hex `color` (e.g. a kitchen's
  `#E4DCC5` beige), a `name`/`role`, **and** a `roomstyle_id` / `rs-####` reference to an actual
  Roomstyler texture (wood, tile, …).

Today `fml2cart.js` reads only `areas[]` and assigns each room a *synthetic* distinct palette colour
(`AREA_COLORS`) for legibility — it ignores `surfaces[]` entirely. Two upgrades available:
1. **Free, now:** use the `surfaces[]` hex `color` (quantised to the palette) for realistic flat
   floors instead of the rainbow.
2. **Blocked (the known gap):** the `rs-####` ids point at real texture photos but don't resolve via
   the render/texture CDN — needs an `rs-####` → texture-filename resolver to bake tiled floor art.

### The binary form (`.rvb`) — same IR, packed

For real cities the JSON is tens of MB and *parsing the text* is the whole load cost. So
`osm-roads.js` emits a packed binary by default; it's the **same feature collection**, just
without the text-parse tax. Layout (all multi-byte ints little-endian):

```
magic "RVB1" | int32 nfeat | int32 namelen | name bytes | float32 bbox[4]
per feature:  int32 kind | int32 sublen | sub bytes | int32 npts | float32 pts[npts*2]
```

`kind` is the numeric **`K_*` index** — `KIND_IX` in `osm-roads.js` and the `enum` in
`roadview.c` are twins and **must stay in sync (append only, never reorder)**. The cart
distinguishes the two formats by the first 4 bytes (`RVB1` → binary fast path, else JSON), so a
`.json` still loads. `--json` writes the readable sibling; `--convert f.json` repacks one without
re-querying Overpass.

## The machinery (all experimental)

| piece | what | revert cost |
|---|---|---|
| `runtime/studio.c` / `studio.h` | `--data <file>` → `de_data_path()` (falls back to `$DE_DATA`); plus `de_dropped_file()` (drag-&-drop a file onto the window) and `de_open_path()` (reveal a folder in Finder/Explorer). All additive. | small, all tagged EXPERIMENTAL |
| `runtime/json.h` | cart-land JSON reader: vendored **jsmn** (zero-alloc tokenizer, MIT) + walk helpers (`json_slurp` / `json_parse` / `json_get` / `json_num` / `json_span`). Used for the `.json` path + `json_slurp` for both. A capability the engine deliberately doesn't own (ADR-0006, like `ui.h`). | delete the file |
| `tools/osm-roads.js` | OSM → schema. `--demo` (synthetic, offline), `--bbox S,W,N,E`, `--place "name"` (Nominatim geocode), `--convert f.json` (repack to `.rvb`). Fetches via Overpass (auto-falls-back across mirrors), projects web-mercator, classes roads/water/zoning/rail, radial-distance simplify. Writes **`data/<slug>.rvb`** (packed binary) by default; `--json` adds the readable sibling. `KIND_IX` must mirror `roadview.c`'s enum. | delete the file |
| `tools/carts/roadview.c` | the one consumer: default-loads `data/demo.rvb`, auto-detects `.rvb`/`.json` by magic bytes (`load_bin` vs the jsmn path), fits bbox, draws zoning blocks + roads + rail + water + green + **zoom-gated footprints (`B`) and tree dots (`T`)**, pan/zoom. **Drag a data file onto the window to switch towns**; `OPEN` reveals `data/`. | delete the cart |
| `data/` (git-ignored) | the town library — every fetched `.rvb` (+ optional `.json`) lands here; the cart's `OPEN` button opens it. | `rm -rf data/` |

## Getting towns into the `data/` folder

Every town is one `.rvb` in `data/` (at the repo root; git-ignored). The loop is:
**fetch a town → it lands in `data/` → drag it onto the running cart** (or hit `OPEN` to
find it in Finder). You never edit the cart. Ways to fetch:

```bash
# 1. by PLACE NAME — easiest. Nominatim geocodes the name, Overpass fetches the whole
#    administrative area. Disambiguate with a country so you don't get the wrong "Delft".
node tools/osm-roads.js --place "Utrecht, Netherlands"      # → data/utrecht-netherlands.rvb
node tools/osm-roads.js --place "Bruges, Belgium"          # → data/bruges-belgium.rvb
#    ⚠ a whole city can be hundreds of k ways — fine now (.rvb loads in <1s); for a tight area:

# 2. by BOUNDING BOX — precise control over the area (and size). Order is S,W,N,E
#    (south,west,north,east, in degrees). Grab one by dragging a rectangle on
#    https://bboxfinder.com  (it prints the four numbers) or openstreetmap.org → Export.
node tools/osm-roads.js --bbox 52.010,4.355,52.020,4.370 --name "Delft centre"
#                              └ S ───┘ └ W ─┘ └ N ──┘ └ E ─┘   → data/delft-centre.rvb

# 3. SYNTHETIC — no network, deterministic, for testing the cart itself.
node tools/osm-roads.js --demo                              # → data/demo.rvb

# 4. CONVERT an existing .json → .rvb (no network); --json on any fetch also keeps the .json.
node tools/osm-roads.js --convert data/utrecht-netherlands.json
```

Useful flags: `--name "Nice Name"` sets the title (and the `data/<slug>.rvb` filename);
`--out path` overrides the location; `--json` also writes the readable `.json`; `--simplify M`
drops points closer than `M` metres (default 8 — raise it for broader strokes / smaller files).

Then view it — any of:

```bash
node tools/play.js roadview run                 # opens on data/demo.rvb; DRAG a town's file on to switch
DE_DATA="$(pwd)/data/utrecht-netherlands.rvb" node tools/play.js roadview run   # force one file
```

In the window: `OPEN` (top-right) reveals the `data/` folder, then **drag any data file from
it onto the window** to load that town. Headless render for a screenshot:
`DE_DATA=… node tools/play.js roadview run --headless --frames 3 --dump /tmp/rv`.

## Where to get real road data

| source | access | notes |
|---|---|---|
| **OpenStreetMap / Overpass** | free, no key — `POST` a bbox query, get tagged polylines | the default; prototype queries at overpass-turbo.eu. Be polite (cache, small bboxes). |
| OSM extracts (Geofabrik `.pbf`) | bulk region download | for whole-region / offline; needs an osmium/`osmtogeojson` parse step |
| US Census TIGER/Line | public-domain shapefiles | US only; cleanest attributes anywhere |
| Natural Earth | coarse global vectors | worldgen *continental* scale, not streets |
| Mapbox / Google / HERE | keyed, commercial | wrong tool — tiles/routing, not raw editable geometry |

## Open follow-ups (before this could graduate)

1. **Loading inside the editor.** Solved for the native window via **drag-and-drop** +
   the `OPEN` button + a `data/demo.json` default-load (so ▶ run shows something even
   without `--data`). What's still missing: the editor's ▶ run could thread `--data`
   through `main.cjs`, but drag-and-drop largely removes the need for an OS file-picker.
2. **Web/wasm file loading.** Native `fopen` works now; the emscripten build needs the file
   in its virtual FS (preload or fetch) before a data cart can ship to the gallery.
3. **Should `fmltools` retarget this schema?** Only worth it if the runtime-load pattern is
   kept — then `floorwalker`/`seinelaan` collapse into one cart fed two files.
