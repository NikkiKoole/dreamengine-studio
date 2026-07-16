# fmltools — Floorplanner `.fml` → playable top-down level

Project-specific tooling (not general cart tooling) that turns a Floorplanner
`.fml` export into a walkable top-down level with real walls, doorways, room
floors, and furniture drawn as pixel-art sprites baked from Floorplanner's CDN
renders.

## One command: a project id → playable (the easy path)

```bash
# Auth ONCE — paste a JWT (or a fp_site_session=... cookie) into a gitignored file, and every
# `<id> --play` afterwards just works (no re-exporting per shell). Refresh it when a fetch 403s.
echo 'eyJ...' > data-tools/fmltools/.token
# (env vars still work + override the file: export FP_AUTH_TOKEN='eyJ...'  or  FP_SESSION='fp_site_session=...')

node data-tools/fmltools/floorplanner.js -pid=187256440 --play
#   -> fetches the .fml, emits data/floorplan/187256440.json, and LAUNCHES the cart on it.
#   Drop --play to just build the data file (it prints the run command).

node data-tools/fmltools/floorplanner.js -pid=187256440 --baked
#   -> instead bakes a self-contained per-project cart via make-floor.sh (option A)
```

`floorplanner.js` is the front door — it does the fetch and then runs the right pipeline below.
**Dynamic (default)** = `tools/carts/floorplan.c` + a `data/floorplan/<id>.json` data file (drag the
JSON onto the cart's window to swap plans). **`--play`** builds *and* opens it; **`--baked`** is the
original per-`.fml` cart. All look identical. Real floor materials (wood/tile/terracotta) are
resolved + tiled when the `.fml` has them (`fml-textures.js`). Design + data schema:
`docs/design/external-data-carts.md` → "Floorplanner — implemented".

## Pipeline (the baked path, run from repo root, in order)

```bash
F="/path/to/plan.fml"

# 1. geometry: .fml -> level data spliced into tools/carts/floorwalker.c
node data-tools/fmltools/fml2cart.js "$F" --scale 8 --maxfurn 280

# 2. art: fetch each furniture/door/window top-down render -> palette pixel sprite
node data-tools/fmltools/fml-assets.js "$F" --max 24 --outline --posterize 5 --saturate 2.2

# 3. embed the baked sprites into the cart (keyed to lv_refs order)
node data-tools/fmltools/fml-sprites.js

# build + bake a thumbnail
node tools/make-cart.js tools/carts/floorwalker.c editor/public/carts/floorwalker.cart.png
node tools/make-cart.js --run editor/public/carts/floorwalker.cart.png
# play it
node tools/play.js floorwalker run
```

## Tools

- **floorplanner.js** — the front door: `-pid=<id>` fetches the `.fml` from the v2 API
  (`download_json.fml`; `$FP_AUTH_TOKEN` JWT preferred, `$FP_SESSION` cookie fallback) into
  `cache/<id>.fml`, then drives the pipeline. Default = **dynamic** (emit `data/floorplan/<id>.json`
  for the shared `floorplan` cart); `--baked` = per-project cart via `make-floor.sh`. Flags:
  `--name --floor --scale --maxfurn --fetch-only --force --baked`.
  **`--search`** (no pid, **token-free**) browses the public plan feed
  (`projects/search.json?all=true`, newest first) — prints id · floors · name · thumbnail so you can
  pick an id to fetch (that download still needs a token). `--count N` pages past the first 50;
  `--json` for machine output. One token then unlocks the whole public catalog.
- **fml2cart.js** — parses walls (door openings become walkable gaps, windows stay
  solid), room polygons (each a distinct colour, see `AREA_COLORS`), furniture and
  door/window openings as oriented sprites. Units are centimetres; `--scale` is
  cm/px, `--maxfurn` (cm) splits furniture from oversized surface planes. Splices a
  `FML_DATA` block into `floorwalker.c` — **or**, with `--json <out>`, writes a runtime data
  file for the `floorplan` cart instead. Flags: `--scale --maxfurn --floor --design --out
  --json --stdout`.
- **fml-assets.js** — for each refid, fetches `cdb/renders/<2>/<refid>_NN.top.x3.png`
  (probes NN), downscales (alpha-aware), `--saturate`, `--posterize`, `--outline`,
  then quantises to a **feedable palette** (`--palette`, default pico32 — note: the
  engine's 32 colours are fixed, so the palette only chooses *which* of them to use).
  Outputs `<refid>.png`, `manifest.json`, and a `preview.png` contact sheet under
  `build/.fml-assets/`. Raw downloads cached under `cache/`.
- **fml-sprites.js** — reads `manifest.json` + the cart's `lv_refs[]`, embeds each
  sprite's palette indices into the cart's `FML_SPRITES` block. With `--json <data>` it instead
  fills `sprites[]` in a `fml2cart.js --json` data file (keyed to its `refs[]`).
- **fml-textures.js** — `--json <data>` only: resolves each surface's `rs-####` Roomstyler material
  via `POST search.floorplanner.com/materials/ids` (no auth) → fetches the texture JPEG → `sips` to
  PNG → downscale/quantise → fills the data file's `textures[]` and sets each `surfaces[].tex`/`.tile`
  (cm-true tiling). The `floorplan` cart tiles them across the room polygons. Flags: `--out --tile
  --saturate --posterize`. (Low-contrast materials like plain grey tile can quantise to a flat colour.)

## Known gaps

- **Photo floor textures**: areas/surfaces reference textures by Roomstyler IDs
  (`rs-####`), which don't resolve through the render/texture CDN paths. Real tiled
  textures need an `rs-####` → texture-filename resolver. Until then `fml2cart.js`
  gives each room a distinct flat colour.
- Some refids have no published top-down render (reported as failures by `fml-assets.js`);
  those furniture items fall back to placeholder boxes (toggle with `T` in-cart).
