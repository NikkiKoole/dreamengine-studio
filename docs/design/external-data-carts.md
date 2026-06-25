# External data → carts (EXPERIMENT)

**Status:** experimental, not committed API. The question being probed: can a cart load a
data blob at **runtime** instead of baking the data into its C source? If this proves its
worth it graduates; if not, the pieces below delete cleanly (they touch almost nothing).

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
    { "kind": "highway", "name": "A13", "pts": [x0,y0, x1,y1, x2,y2, ...] },
    { "kind": "road",    "name": "",    "pts": [ ... ] }
  ]
}
```

- `kind` ∈ `highway | arterial | road | track` (road classes, drawn as polylines) plus
  `canal` (a water line) and the **filled-area** kinds `water` and `building` (closed rings,
  drawn with `polyfill`). The cart styles each; `building` footprints are **LOD-gated** —
  only filled once you zoom in past `BUILD_GATE_PPM` (sub-pixel at fit = wasted fills + clutter),
  toggleable with `B`.
- `pts` is a **flat** `[x0,y0,x1,y1,…]` polyline in the bbox's metre frame.
- Floorplans would reuse the same shape (walls = polylines, rooms = closed polylines) with a
  different `kind` vocabulary — that's the point of standardizing the IR, not the parser.

## The machinery (all experimental)

| piece | what | revert cost |
|---|---|---|
| `runtime/studio.c` / `studio.h` | `--data <file>` → `de_data_path()` (falls back to `$DE_DATA`); plus `de_dropped_file()` (drag-&-drop a file onto the window) and `de_open_path()` (reveal a folder in Finder/Explorer). All additive. | small, all tagged EXPERIMENTAL |
| `runtime/json.h` | cart-land JSON reader: vendored **jsmn** (zero-alloc tokenizer, MIT) + walk helpers (`json_slurp` / `json_parse` / `json_get` / `json_num` / `json_span`). A capability the engine deliberately doesn't own (ADR-0006, like `ui.h`). | delete the file |
| `tools/osm-roads.js` | OSM → schema. `--demo` (synthetic, offline), `--bbox S,W,N,E`, or `--place "name"` (Nominatim geocode). Fetches via Overpass (auto-falls-back across mirrors), projects web-mercator, classes roads + water, radial-distance simplify. Writes **`data/<slug>.json`** by default. | delete the file |
| `tools/carts/roadview.c` | the one consumer: default-loads `data/demo.json`, fits bbox, draws classed roads + filled water + **zoom-gated building footprints** (`B` toggles), pan/zoom. **Drag a `.json` onto the window to switch towns**; the `OPEN` button reveals the `data/` folder. | delete the cart |
| `data/` (git-ignored) | the town library — every fetched `.json` lands here; the cart's `OPEN` button opens it. | `rm -rf data/` |

## Getting towns into the `data/` folder

Every town is one `.json` in `data/` (at the repo root; git-ignored). The loop is:
**fetch a town → it lands in `data/` → drag it onto the running cart** (or hit `OPEN` to
find it in Finder). You never edit the cart. Three ways to fetch:

```bash
# 1. by PLACE NAME — easiest. Nominatim geocodes the name, Overpass fetches the whole
#    administrative area. Disambiguate with a country so you don't get the wrong "Delft".
node tools/osm-roads.js --place "Utrecht, Netherlands"      # → data/utrecht-netherlands.json
node tools/osm-roads.js --place "Bruges, Belgium"          # → data/bruges-belgium.json
#    ⚠ a whole city can be 20k+ ways — big but fine; for a tight area prefer a bbox:

# 2. by BOUNDING BOX — precise control over the area (and size). Order is S,W,N,E
#    (south,west,north,east, in degrees). Grab one by dragging a rectangle on
#    https://bboxfinder.com  (it prints the four numbers) or openstreetmap.org → Export.
node tools/osm-roads.js --bbox 52.010,4.355,52.020,4.370 --name "Delft centre"
#                              └ S ───┘ └ W ─┘ └ N ──┘ └ E ─┘   → data/delft-centre.json

# 3. SYNTHETIC — no network, deterministic, for testing the cart itself.
node tools/osm-roads.js --demo                              # → data/demo.json
```

Useful flags: `--name "Nice Name"` sets the title (and the `data/<slug>.json` filename);
`--out path.json` overrides the location; `--simplify M` drops points closer than `M`
metres (default 8 — raise it for broader strokes / smaller files).

Then view it — any of:

```bash
node tools/play.js roadview run                 # opens on data/demo.json; DRAG a town's .json on to switch
DE_DATA="$(pwd)/data/utrecht-netherlands.json" node tools/play.js roadview run   # force one file
```

In the window: `OPEN` (top-right) reveals the `data/` folder, then **drag any `.json` from
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
