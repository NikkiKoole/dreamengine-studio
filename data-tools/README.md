# data-tools/ — per-cart data importers

**`tools/` is general** — build, lint, bake, play, the verification oracles: things every cart and
every contributor uses. **`data-tools/` is the opposite**: small tool collections that exist to get
*external data into a few specific carts*. Each subfolder serves a cart family and is free to be
messy in its own way (its own cache, samples, README, TODO) without cluttering the shared `tools/`.

The rule of thumb: if a script **fetches / converts / packs external data for a particular cart**,
it lives here. If it's a **gate, builder, or workflow used across the catalog** (e.g.
`road-check.js` is a correctness oracle), it stays in `tools/`.

Run the tools **from the repo root** (paths like the `data/` output dir and `tools/carts/<cart>.c`
are resolved relative to the working directory).

## Collections

- **`fmltools/`** — Floorplanner `.fml` → a playable top-down level. `floorplanner.js` fetches a
  project by id (caches under `fmltools/cache/`), `fml2cart.js` bakes geometry into a cart, and
  `fml-assets.js`/`fml-sprites.js`/`fml-textures.js` bake the furniture sprites; `make-floor.sh` runs
  the whole pipeline. Feeds **floorwalker / seinelaan / floorplan**. See `fmltools/README.md`.
- **`roadview/`** — `osm-roads.js`: fetch real OpenStreetMap road geometry (or a synthetic `--demo`
  set) and emit the shared "vector features" `JSON`/`.rvb` that the **roadview** cart loads at
  runtime — swap cities by swapping files, no cart regeneration. Design + schema:
  [`../docs/design/external-data-carts.md`](../docs/design/external-data-carts.md).
- **`breaks/`** — `breaks.js`: ingest *any* drum loop (URL or file) → a frozen mono float32 PCM
  artifact the **break-chopper** cart (WIP) loads at runtime via `sample_load()`, auto-slices, and
  lays across a pad grid. Source-agnostic ("throw in your own loop"); artifacts are gitignored.
  The amen fixture is a **copyrighted dev placeholder** — see `breaks/README.md` → the release gate.
  Capture-then-freeze: [`../docs/design/mic-and-sampling.md`](../docs/design/mic-and-sampling.md).
