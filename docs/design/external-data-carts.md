# External data → carts

**Status:** SHIPPED (graduated 2026-07-02) — the runtime hooks (`de_data_path`/`de_dropped_file`/
`de_open_path`) are **committed API** per [ADR-0025](../decisions/0025-external-data-hooks-are-committed-api.md)
(documented in all four places; the commitment is paths-not-formats — what's inside the file
stays cart-land policy). The question this experiment probed — can a cart load a data blob at
**runtime** instead of baking it into its C source? — was answered by its consumers: roadview,
citydrive and sloop all drive real OSM cities through these hooks. This doc remains the design
story: the formats (`.rvb`), the pipeline (`osm-roads.js`), and what each consumer does with
the data.

## Session handoff — pick up here

**What's working and committed** (`roadview`, registered as a tech-demo/toy):
- The whole pipeline: `osm-roads.js` (`--demo` / `--bbox` / `--place`, mirror-fallback) → a
  `data/<slug>.rvb` → `roadview` loads it at runtime via `de_data_path()` + `runtime/json.h`.
- **Packed binary format (`.rvb`) — the load is now ~instant.** Real cities are tens of MB of
  JSON and tokenising that text (jsmn + `strtod` per coordinate) was the bottleneck: **Breda
  (27 MB) took >2 min, Rotterdam timed out.** `osm-roads.js` now emits a packed binary by
  default (magic `RVB2`, was `RVB1`); the cart auto-detects it by magic bytes and walks it with `memcpy`.
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
node data-tools/roadview/osm-roads.js --place "Utrecht, Netherlands"        # → data/utrecht-netherlands.rvb
node data-tools/roadview/osm-roads.js --convert data/breda-netherlands.json # repack an existing .json → .rvb (no re-fetch)
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
- Plus the three "to graduate" items below (editor `--data` wiring, web/wasm file loading,
  whether `fmltools` should retarget this schema).

**Done (the "use ALL the data" pass):**
- **Full road ladder + bike paths.** The 4-tier collapse is gone: `highway` / `arterial` /
  `secondary` / `tertiary` / `road` (ordinary street) / `service` / `track` / `cycleway` (a red
  Dutch fietspad) / `footway` (dashed) are each their own painted class. Appended to the `K_*`
  enum / `KIND_IX` (append-only, so old `.rvb` files still load).
- **Nothing dropped — the muted hashed understory.** `classifyWay` no longer returns `null`:
  any uncategorised way becomes `other_area` (closed ring → filled) or `other_line` (open →
  stroked), carrying its defining `key=value` (e.g. `amenity=school`) in `sub`. The cart FNV-hashes
  that tag into a stable `(muted colour, 4×4 dither pattern)` so each category reads distinct
  without a dedicated palette slot — a quiet textured layer *beneath* the curated roads/water/parks.
  The Overpass query was widened to fetch all `natural`/`landuse`/`leisure`/`amenity` + `man_made` +
  `aeroway` wholesale (was narrow subsets) so there's actually a long tail to draw.
- **Two-row → three-row legend.** `legend()` now uses `FONT_TINY`, a grouped `LEG_ORDER`, and wraps
  across up to 3 rows, so the full class list shows instead of hard-stopping at the screen edge.
- **Multipolygon relations assembled (the "hollow water" fix).** Big lakes/rivers/forests are OSM
  *relations*; fetching ways alone rendered them as hollow slivers (blue rim, grey middle).
  `osm-roads.js` now fetches the relations too and stitches their outer rings into filled areas —
  see "Relations" under "Data we could pull" below. (Inner-ring holes are Stage 2.)

## The road hierarchy — the full ladder

OSM's `highway=*` key is a ~20-value functional classification. `osm-roads.js` now paints **the
full ladder** — every density its own class (colour + real-world half-width in metres, so they
fatten as you zoom):

| class (cart) | colour | half-width | OSM `highway=*` values |
|---|---|---|---|
| `highway`   | orange       | 7 m   | `motorway`, `trunk` (+ `_link` ramps) |
| `arterial`  | yellow       | 5 m   | `primary` (+ `_link`) |
| `secondary` | light yellow | 4 m   | `secondary` (+ `_link`) |
| `tertiary`  | peach        | 3 m   | `tertiary` (+ `_link`) |
| `road`      | grey         | 3 m   | `residential`, `unclassified`, `living_street`, `road` (ordinary streets) |
| `service`   | mid grey     | 1.5 m | `service` (driveways, parking aisles, alleys) |
| `cycleway`  | dark red     | 1.2 m | `cycleway` — the Dutch red fietspad |
| `footway`   | indigo (dashed) | 0.9 m | `footway`, `path`, `steps`, `bridleway`, `pedestrian`, `corridor` |
| `track`     | brown        | 1.5 m | `track` |

Splitting `secondary` off `arterial` fixes the old "the highway looks broken" artefact (orange
`motorway` → yellow `primary` → lighter `secondary` now step down in shade instead of one jump).
`cycleway` and `footway` are no longer merged into `track` — a big deal in cycling-dense NL.

**Nothing is dropped any more.** What `classifyWay` doesn't recognise (niche `highway=*` like
`construction`/`raceway`/`busway`, and every non-road tag) is no longer returned as `null` →
discarded. Instead the caller routes it to `other_area` (closed ring) or `other_line` (open way)
with its defining `key=value` in `sub`, and the cart hashes that into a muted colour+dither. Point-only
tags (`crossing`, `traffic_signals`, `street_lamp`) still have no geometry to draw. Non-`highway`
ways with dedicated classes: `rail` (dashed), `canal`, `coast`.

## Data we could pull (OSM has much more)

### Road & street data — the leverage tiers (2026-07-01 audit)

The dropped/available OSM road data, ranked by how directly it upgrades the street render, with real
coverage measured on central Delft (~311 ways sampled) and current status. Twin of the road-dressing
consumer plan in [`roadkit.md`](roadkit.md).

**Tier 1 — cheap way-tags (ride on ways we already fetch; packed into the `sub` token string).** ✅ **fetched
2026-07-01** via `roadSub()` (keys `B/T O L W U M C R`, backward-compatible):
| tag | token | Delft coverage | consumed? |
|---|---|---|---|
| `surface` (asphalt/paving_stones/sett/…) | `U a/p/g/w/o` | **264/311** | ✅ citydrive carriageway colour (brick vs asphalt) |
| `maxspeed` | `M<n>` | 109 | ⏳ captured, not rendered (30-zones) |
| `sidewalk` both/left/right/none | `W b/l/r/n` | 68 | ✅ citydrive pavement (real, vs class heuristic) |
| `oneway` | `O` | 62 | ✅ citydrive suppresses the centre-line AND narrows the carriageway (see width model) |
| `bridge`/`tunnel`/`layer` | `B<L>`/`T<L>` | 32/15 | ✅ citydrive decks + dashed tunnels |
| `lanes` | `L<n>` | 9 | ✅ citydrive lane dividers + a width signal (`road_hw`) |
| `width` (carriageway m) | `X<n>` | 2 | ✅ citydrive authoritative width override (rare but exact) |
| on-road `cycleway=lane` / `cycleway:*` | `C b/l/r` | ~11 | ✅ citydrive red fietsstrook along the carriageway edge (tagged side/s) |
| `junction=roundabout` | `R` | rare | ⏳ captured (resolves the equal-class voorrang case) |

**Tier 2 — node-level control.** ✅ **fetched 2026-07-02:** `osm-roads.js` now queries these nodes and emits
them as new POINT kinds (like `tree`), appended to `KIND_IX` **and** the `K_*` enums in `citydrive.c` +
`roadview.c` (indices 24–28, append-only). Delft centre: 43 crossings, 92 traffic-signals, 43 give-way, 57
calming (0 stop — NL uses give-way). The *real* versions of things we used to infer:
- `highway=crossing` (`K_CROSSING`) — ✅ **citydrive renders a real zebra** at the node, oriented across the
  nearest carriageway (`draw_zebra` + `nearest_motor_dir`). Placed from data, not guessed per junction-arm
  (why the earlier blanket per-arm zebra was pulled).
- `highway=give_way`/`stop` (`K_GIVEWAY`/`K_STOP`) — ✅ **feed the haaientanden**: each node attaches to its
  junction arm (`build_junctions` → per-arm `yield` bitmask), so teeth mark exactly the arms OSM says yield
  (real per-approach voorrang), class-inference only where no node exists. 21/─ Delft junctions real-driven.
- `highway=traffic_signals` (`K_SIGNALS`), `traffic_calming` (`K_CALMING`) — captured (`pnode[]`); not drawn yet.
- roadview loads none of them visually (its loader drops non-tree single-point features) — safe, just ignored.

**Tier 3 — carried-but-unused / minor.** `name` is already packed per feature (unused by citydrive — free labels);
`ref` (road numbers A13/N470 → shields) is *not* captured; `parking:lane`, `turn:lanes`, `building:colour`/`roof:shape` dropped.

Since the "use ALL the data" pass, most of the area/line families below **already render** — as
the muted hashed `other_area`/`other_line` understory (any `landuse`/`leisure`/`amenity`/`natural`/
`man_made`/`aeroway` we don't curate). So this list is now about **promoting** the high-value ones
from generic hash to a *dedicated* class (recognisable colour/glyph) — same ~10-line pattern
(classify in `osm-roads.js` → a `K_*` + style in `roadview.c` → `fill_areas()`/`draw_class()`) —
plus the genuinely-not-yet-fetched points/relations.

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
- Roads — **DONE (2026-07-01):** `bridge`/`tunnel`/`layer` (deck code) plus `surface`, `sidewalk`,
  `oneway`, `lanes`, `maxspeed`, on-road `cycleway`, `junction=roundabout` now ride in the road's `sub`
  as a `;`-delimited token list (`roadSub()`; keys `B/T O L W U M C R`, bridge token first so `parse_deck`
  is unchanged — backward-compatible, no format bump). citydrive reads them via `sub_tok()`: **surface →
  carriageway colour** (brick/klinker vs asphalt) and **`sidewalk` → real per-street pavement**. *Still on
  the wish-list:* `ref` (road-number shields/labels).
- `name` on waterways (river names), `addr:*` on buildings.
- **Node-level control (needs new point-feature kinds + a query):** `highway=crossing` (marked zebras),
  `highway=give_way`/`stop`/`traffic_signals` (real priority, vs our class-inferred voorrang),
  `traffic_calming`. These want new `KIND_IX`/enum slots in `citydrive.c` + `roadview.c` — the next chunk.

**Relations — multipolygons (Stage 1 DONE).** Big lakes/rivers/forests are modelled as
multipolygon *relations* whose member ways are just boundary arcs — so fetching ways alone gave a
**hollow sliver** (a 46 km-perimeter ribbon enclosing 1.6 km² for the Nieuwe Maas → "blue rim, grey
middle"). `osm-roads.js` now also queries `relation["natural"|"landuse"|"leisure"|"waterway"=riverbank]`,
skips their member ways, and stitches the **outer** rings end-to-end (`assembleRings()`) into proper
filled areas. The cart's `MAXAREAPTS` was bumped to 16384 to hold a whole assembled river ring.
**Still TODO:** inner-ring **holes** (an island in a lake, a courtyard in a building) — Stage 1 fills
the outer ring solid, so islands are currently painted over; needs the even-odd bridge trick or a
multi-ring feature in the schema. Relations of `type=boundary` (the city outline) are still not read.

**Rough priority for a broad-strokes map:** building heights (serves extrusion) → airports →
transit stops + POI points (also make the hover panel rich) → place-name labels → city walls /
admin boundary.

## Building heights (for the extrude-buildings-downstream goal)

Two ways to get them, cheapest first:

**A. OSM `height` / `building:levels` ride-along** (no new query — we already fetch building tags).
**DONE** — `osm-roads.js` `buildingHeight(tags)` parses `tags.height` (metres), else
`tags['building:levels'] * 3` (≈3 m/storey), else 0. Stashed as the feature's `h` field, threaded
through `buildDoc` + `writeBin` (the **RVB2** format bump — one `float32`/feature) and the `.json`.
`roadview` skips it; `citydrive` extrudes it.
- **Caveat confirmed: partial coverage.** A tight Amersfoort-centre fetch tagged only **~6%** of
  buildings (`h>0`); the rest are unknown. So `citydrive` applies a **per-type fallback** (a default
  storey height by `sub`) for `h=0` — the data honestly carries 0, the consumer fills the gap.

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
with a different floorplan **baked into the source**. `data-tools/fmltools/fml2cart.js` regenerates the
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
  OpenStreetMap  →  data-tools/roadview/osm-roads.js        →  features blob   →    roadview.c
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
    { "kind": "building", "name": "", "sub": "apartments", "h": 15, "pts": [ ... ] }
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
- `h` (optional) is the **building height in metres** (omitted/0 = unknown), from OSM
  `height` / `building:levels` (see "Building heights"). The pseudo-3D consumer (`citydrive`)
  extrudes it; `roadview` ignores it. **Coverage is sparse** — only ~6% of buildings are tagged in
  a typical NL town, so the consumer applies a per-type fallback for the rest.
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

- **`data-tools/fmltools/floorplanner.js -pid=<projectid>`** — fetches a project's `.fml` from the v2 API
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
  "areas":[{"c":colorIdx, "poly":[x,y,...]}, ...],                 // c = real floor colour (quantised)
  "surfaces":[{"c":colorIdx, "tex":idx, "tile":px, "poly":[...]}, ...],  // floor coverings; tex<0 = flat
  "rgb": 0|1,                                                     // 1 = furniture sprites are 24-bit (px = 0xRRGGBB, -1 transparent)
  "sprites":[{"w","h","px":[idx, 255=transparent]}, ...],         // furn.ref indexes sprites[]; px = 0xRRGGBB when rgb:1
  "textures":[{"w","h","px":[idx, ...]}, ...] }                   // surfaces[].tex indexes textures[]
```

### Loader — SHIPPED (an in-cart id picker + `floorplanner.js --serve` fetch-bridge)

**Status: shipped 2026-07-13** (both stages). A start screen when the cart boots (or on TAB) lists the
plans you've already fetched and lets you pick one, drag a `.json`, or **type/paste an id**. Typing an
id you don't have locally writes `build/.fp-request`; **`floorplanner.js --serve`**, run alongside the
editor, watches that file, fetches+builds the plan, and hands the path back via `build/.fp-ready` — the
cart polls it and loads the plan (spinner while it works; ~90s timeout with a "is --serve running?"
hint if not). So: type an id → it fetches → the plan appears, no CLI round-trip.

Implementation: picker + signal-file handling in `tools/carts/floorplan.c` (dir scan via `<dirent.h>`,
digits via `keyp()` so it's script-testable, paste via the new `studio.h` `paste()`); the `--serve`
loop + reusable `ensureFml`/`buildDynamic` in `data-tools/fmltools/floorplanner.js`. Signal files live
in `build/` (cart cwd), `.fp-ready` written atomically. The cart CANNOT fetch itself (no HTTPS — see
the rejected alternative below); `--serve` is the node side doing the network + bake.

**Prerequisite — done (2026-07-13): paste-once auth.** `floorplanner.js` now reads the credential
from a gitignored `data-tools/fmltools/.token` file (a JWT or an `fp_site_session=…` cookie;
auto-detected), in addition to `$FP_AUTH_TOKEN` / `$FP_SESSION` (env still overrides). Paste a token
once → `<id> --play` just works every shell until it expires. So "load any old id" is already a
one-liner; the loader below is the GUI in front of it.

**The decided route: JS, not cart-side HTTPS.** The cart genuinely can't do the *whole* fetch, and
even if it could it shouldn't — see the rejected alternative at the end. So node keeps doing the
network + bake; the cart just picks + loads. Two patterns already in the repo are the model:
- **`net-relay.js --serve`** — "cart + helper in one process": one launch command runs the cart *and*
  a node helper alongside it. Copy this shape.
- **The live-inspection request-file trick** (`build/.bake/screenshot_request` — the cart writes a
  request file, an external process fulfils it). Same idea, used here for "please fetch this id".

**Design (two pieces):**

1. **Cart start screen** (`tools/carts/floorplan.c`). Shown on boot when nothing is loaded (today it's
   the bare "drag a .json" screen), and summonable with a key.
   - **Pick a saved plan.** Scan the plans folder and list them. Runtime facts confirmed: no
     dir-listing API in `studio.h`, so use `<dirent.h>` (`opendir`/`readdir` — fine in the native
     build; not wasm, which is dev-only anyway). The folder = the dirname of `de_data_path()` if set,
     else `../data/floorplan` (cart runs with `cwd=build/`). Arrow/click to select, Enter → existing
     `load_from()`. Pure in-cart, zero new processes — **build this first** for instant value.
   - **Type an id.** `text_input()` returns typed chars (digits) — that's the entry field. (True
     clipboard *paste* isn't exposed by `studio.h`; typing a short id is enough.) On Enter, if the
     plan is already local → load it; if not → write the id to a request file (below).
   - Drag-drop and `$DE_DATA` keep working as the "jump straight in" paths.

2. **`floorplanner.js --serve`** (mirrors `net-relay.js --serve`). Launches the floorplan cart, then
   watches `build/.fp-request`. On a new id: run the existing fetch+build pipeline
   (`fml2cart`/`fml-assets`/`fml-sprites`/`fml-textures`), write `data/floorplan/<id>.json`, then
   signal the cart (drop a "ready" marker file / touch the json) so it auto-loads — same window, the
   plan just appears. Net effect: type an id → it fetches → the real plan shows up, one launch
   command, nothing to babysit. **Build second, on top of the picker.**

**No editor changes** — this lives entirely in cart-land + the CLI, per the owner's constraint.

**Rejected alternative — cart does its own HTTPS.** Theoretically possible (the cart is a native
process; `net.h` already opens UDP sockets, so only TLS is missing, and `popen("curl …")` closes that
in ~1 line on native). But it's a bad trade: downloading the `.fml` is the easy ~10%. The other ~90%
doesn't want to live in the cart — (1) the cart parses the *processed* JSON, not raw `.fml`, so
fetching in-cart means porting `fml2cart.js`'s `.fml` parser to C; (2) furniture sprites need
`fml-assets.js` (many CDN image fetches + downscale/quantise); (3) floor textures need
`fml-textures.js` (a second API + `sips`). So cart-HTTPS would remove the one part that was never the
problem and force re-implementing the parts that are. (Aside: on the *web* build the browser's `fetch`
makes HTTPS easy — an odd inversion — but that forks the fetch path. A geometry-only "quick peek"
mode, curl the `.fml` + draw bare boxes, is a possible fun spike but still needs the C `.fml` parser.)

### Floor finishes — how the cart colours/textures floors

The `.fml` carries floor finishes in two places, both now used:
- `design.areas[]` — a hex `color` per room. `fml2cart.js --json` quantises it to the palette
  (`areas[].c`) for a true-to-plan look. (The baked-C path still uses the synthetic `AREA_COLORS`
  rainbow for max legibility — that's `areaOut.color` vs `realc`.)
- `design.surfaces[]` — floor coverings (a polygon + a hex `color` + a `roomstyle_id`/`refid`
  `rs-####` Roomstyler material + an `sx` tile-scale). These get a flat `c` *and*, when the material
  resolves, a tiled texture.

**Resolving `rs-####` → a tiled texture** (`fml-textures.js`, no auth):
1. `POST` the bare id to `https://search.floorplanner.com/materials/ids`
2. read `_source.texture` (+ `_source.color`, `_source.width` in cm)
3. fetch `…/cdb/textures/floor_and_wall/original/<texture>` (JPEG)
4. `sips` → PNG → downscale → quantise → `textures[]`; the cart tiles it across the surface polygon
   in world space, at a cm-true `tile` px size (material width × `sx` ÷ plan cm/px).

**Caveat (the real limit now):** the 32-colour palette. High-contrast materials (Floor Herringbone,
coloured tile) tile visibly; low-saturation ones (plain grey tile, concrete, terracotta) collapse to
1–2 palette entries and read as a flat colour. Options if this matters: a luma-contrast stretch in
the bake, dithering, or a wider/auto palette.

### `citydrive` — the pseudo-3D consumer (same data, extruded)

`citydrive` is roadview's **3D sibling**: it loads the *same* `.rvb` but, instead of drawing it
flat top-down, **extrudes each building footprint straight up the screen** (the cityview
GTA1-meets-Zelda look — `project()` world→screen, height up, ADR-0021). One cart, two front-ends
over one data file — the pattern paying off a second time.

What it took (the three prerequisites, in order):
1. **Arbitrary-polygon extrusion** — cityview only extruded axis-aligned boxes; real OSM footprints
   are N-gons. `citydrive`'s `draw_bldg()` uses **winding-based outward normals** (signed area → a
   consistent per-edge perpendicular, concave-safe) for the visible-wall cull, and a `polyfill`
   roof cap (even-odd coverage handles concave roofs). No hardcoded normals.
2. **Heights** — the **RVB2** `h` field (above). ~6% of OSM buildings are tagged; the rest get a
   per-footprint fallback low-rise (so the tagged talls still stand out). The data carries 0 = unknown;
   the consumer fills the gap, not the format.
3. **Spawn + cull** — spawns at the building **nearest the mass centroid** (robust against the OSM
   bbox-balloon, which a plain centre would land in a gap of). Per frame it range-culls buildings to
   ~320 m of the camera then depth-sorts; roads draw as flat ground ribbons (world-space quads) beneath.

**Terrain (RVB3).** OSM carries no elevation, so the ground was flat. `osm-roads --dem` now fetches an
SRTM heightfield (opentopodata API) over the bbox and stores a compact `GxG` grid (default 96) trailing
the features — **RVB3** (magic-gated; RVB1/2 still load, roadview ignores it). citydrive bilinear-samples
`ground_z(x,y)` from it and **drapes everything**: roads/footprint-bases sit at ground height, roofs at
+height, the car + camera ride the terrain (`camz`), and a shaded low-poly **mesh** of the grid cells
near the camera is drawn as the base ground so hills actually read. Elevation is **vertically exaggerated**
(`TERRAIN_EXAG`) because the parallel-oblique projection (no camera pitch) renders true relief subtly —
it reads best in motion and in less-built areas; dense downtown hides it. Fetch e.g.
`--bbox 37.791,-122.421,37.807,-122.404 --dem` for San Francisco's hills.

Still flat / TODO in `citydrive`: **the roads themselves** — today flat class-coloured quads (`road_seg`),
no markings/pavements/curb returns. **`citydrive` is now the promoted first home for the geometry-first
road render** (the close pseudo-3D view where that grammar is visible, unlike sloop's fast top-down drive):
markings → pavements/kerbs → curb-return junctions on its projected ground plane, via `roadkit.h`. Decision
+ plan: [`roadkit.md`](roadkit.md).

**Bridges & tunnels — DONE (2026-07-01).** Three parts, all shipped:
- *Flat bridges* — motor roads draw over canals (`K_CANAL`) and water, so a road-over-water crossing reads
  as a flat bridge instead of blue-on-road.
- *Data* — **`osm-roads.js` now carries bridge/tunnel/layer** for line roads in the per-feature `sub`
  string (`"B<layer>"`/`"T<layer>"`, absent = at grade). No format bump — the field already existed and
  readers skip it. Verified against real Delft (79 bridges/tunnels; the named canal *bruggen*). This is
  also the grade-dispatch signal roadlab/streetlab want, so it pays off twice.
- *Render (citydrive)* — a `bridge` way (`deck>0`) draws as a **raised deck** (running surface + fascia +
  ground shadow, ramped up at both ends, subdivided so 2-node spans hump smoothly); `DECK_H` lift/layer is
  a one-line tune (Delft's low canal bridges = 3 m). A `tunnel` way (`deck<0`) draws as a **faint dashed
  strip** ("goes underground here"). Height extrudes up-screen via `project()`'s z — reads in the 3D
  camera modes, flat in top-down.

*Refinements left:* tunnel **portals** (dark mouths at the ends), **pillars** under long spans, and larger
lift for motorway/hilly cities (Delft is the gentle case). Other citydrive flat/TODO: inner-ring holes,
the >64-vertex footprint clamp (`MAXBV`), and the hashed `other_area` understory. Web/wasm file loading is
shared with roadview (below).

### The binary form (`.rvb`) — same IR, packed

For real cities the JSON is tens of MB and *parsing the text* is the whole load cost. So
`osm-roads.js` emits a packed binary by default; it's the **same feature collection**, just
without the text-parse tax. Layout (all multi-byte ints little-endian):

```
magic "RVB2" | int32 nfeat | int32 namelen | name bytes | float32 bbox[4]
per feature:  int32 kind | float32 h | int32 sublen | sub bytes | int32 npts | float32 pts[npts*2]
```

`h` is the **building height in metres** (0 = unknown / non-building), parsed from OSM
`height` / `building:levels` (see "Building heights" above) — the pseudo-3D consumer
(`citydrive`) extrudes it; `roadview` is 2D and skips it. **`RVB1` (no `h`) still loads** — the
reader switches per-feature layout on the magic's version byte (`'1'` vs `'2'` vs `'3'`).

**`RVB3`** is `RVB2` plus a trailing **terrain heightfield** (from `--dem`): after the features,
`int32 G | float32 grid[G*G]` — a `GxG` elevation grid (metres, relative) mapped uniformly over the
bbox, row 0 = south. citydrive drapes its geometry over it; `roadview` stops after the features and
ignores it.

`kind` is the numeric **`K_*` index** — `KIND_IX` in `osm-roads.js` and the `enum` in
`roadview.c` are twins and **must stay in sync (append only, never reorder)**. The cart
distinguishes binary from JSON by the first 3 bytes (`RVB` → binary fast path, else JSON), so a
`.json` still loads. `--json` writes the readable sibling; `--convert f.json` repacks one without
re-querying Overpass.

## The machinery (all experimental)

| piece | what | revert cost |
|---|---|---|
| `runtime/studio.c` / `studio.h` | `--data <file>` → `de_data_path()` (falls back to `$DE_DATA`); plus `de_dropped_file()` (drag-&-drop a file onto the window) and `de_open_path()` (reveal a folder in Finder/Explorer). All additive. | small, all tagged EXPERIMENTAL |
| `runtime/json.h` | cart-land JSON reader: vendored **jsmn** (zero-alloc tokenizer, MIT) + walk helpers (`json_slurp` / `json_parse` / `json_get` / `json_num` / `json_span`). Used for the `.json` path + `json_slurp` for both. A capability the engine deliberately doesn't own (ADR-0006, like `ui.h`). | delete the file |
| `data-tools/roadview/osm-roads.js` | OSM → schema. `--demo` (synthetic, offline), `--bbox S,W,N,E`, `--place "name"` (Nominatim geocode), `--convert f.json` (repack to `.rvb`). Fetches via Overpass (auto-falls-back across mirrors), projects web-mercator, classes roads/water/zoning/rail, radial-distance simplify. Writes **`data/<slug>.rvb`** (packed binary) by default; `--json` adds the readable sibling. `KIND_IX` must mirror `roadview.c`'s enum. | delete the file |
| `tools/carts/roadview.c` | the one consumer: default-loads `data/demo.rvb`, auto-detects `.rvb`/`.json` by magic bytes (`load_bin` vs the jsmn path), fits bbox, draws zoning blocks + roads + rail + water + green + **zoom-gated footprints (`B`) and tree dots (`T`)**, pan/zoom. **Drag a data file onto the window to switch towns**; `OPEN` reveals `data/`. | delete the cart |
| `data/` (git-ignored) | the town library — every fetched `.rvb` (+ optional `.json`) lands here; the cart's `OPEN` button opens it. | `rm -rf data/` |

## Getting towns into the `data/` folder

Every town is one `.rvb` in `data/` (at the repo root; git-ignored). The loop is:
**fetch a town → it lands in `data/` → drag it onto the running cart** (or hit `OPEN` to
find it in Finder). You never edit the cart. Ways to fetch:

```bash
# 1. by PLACE NAME — easiest. Nominatim geocodes the name, Overpass fetches the whole
#    administrative area. Disambiguate with a country so you don't get the wrong "Delft".
node data-tools/roadview/osm-roads.js --place "Utrecht, Netherlands"      # → data/utrecht-netherlands.rvb
node data-tools/roadview/osm-roads.js --place "Bruges, Belgium"          # → data/bruges-belgium.rvb
#    ⚠ a whole city can be hundreds of k ways — fine now (.rvb loads in <1s); for a tight area:

# 2. by BOUNDING BOX — precise control over the area (and size). Order is S,W,N,E
#    (south,west,north,east, in degrees). Grab one by dragging a rectangle on
#    https://bboxfinder.com  (it prints the four numbers) or openstreetmap.org → Export.
node data-tools/roadview/osm-roads.js --bbox 52.010,4.355,52.020,4.370 --name "Delft centre"
#                              └ S ───┘ └ W ─┘ └ N ──┘ └ E ─┘   → data/delft-centre.rvb

# 3. SYNTHETIC — no network, deterministic, for testing the cart itself.
node data-tools/roadview/osm-roads.js --demo                              # → data/demo.rvb

# 4. CONVERT an existing .json → .rvb (no network); --json on any fetch also keeps the .json.
node data-tools/roadview/osm-roads.js --convert data/utrecht-netherlands.json
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
