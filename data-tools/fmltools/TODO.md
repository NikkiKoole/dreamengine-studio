# floorwalker / fmltools — TODO

Status of the Floorplanner-`.fml` → top-down cart pipeline and the carts built from it.

## Done
- [x] `fml2cart.js` — `.fml` → level data (walls, door/window openings, room polygons, furniture boxes).
- [x] `fml-assets.js` — fetch CDN top-down renders → downscale + saturate + posterize → palette pixel sprites (+ `manifest.json`, `preview.png`).
- [x] `fml-sprites.js` — embed baked sprites into the cart, keyed to `lv_refs`.
- [x] `make-floor.sh` — one-shot pipeline for a new `.fml`.
- [x] `floorwalker.c` engine: segment/box collision, scanline room floors, scaled+rotated sprite blit, camera-cull.
- [x] Two carts shipped: `floorwalker` (house-10juni), `seinelaan` (Seinelaan 31, Eindhoven).
- [x] Walkable doorways (rectangle wall collision, no end-caps bridging the gap).
- [x] Doors drawn as open-swing symbols; windows as thin in-wall glass.
- [x] Render-time furniture outlines (crisp at any scale); `O` toggles, `T` toggles sprites/boxes.
- [x] Each room a distinct floor colour (`AREA_COLORS`).
- [x] **Runtime-loaded twin** — `floorplanner.js -pid=<id>` fetches the `.fml` and emits a JSON data
      file; one shared `floorplan.c` cart loads it via `--data`/`$DE_DATA` (drag to swap). No recompile
      per project. `fml2cart.js --json` + `fml-sprites.js --json` are the data-emit paths.
      See `docs/design/external-data-carts.md` → "Floorplanner — implemented".

- [x] **Real photo floor textures** (dynamic cart) — `fml-textures.js` resolves each surface's
      `rs-####` via `POST search.floorplanner.com/materials/ids` (no auth) → fetches the texture
      JPEG → quantises → fills `textures[]`; `floorplan.c` tiles them across the surface polygons
      (cm-true tile size from material width × `sx`). `surfaces[]` now also carries a flat material
      colour fallback. Works great on high-contrast materials (wood/herringbone/terracotta).

## Open / next — the three big ones (parked 2026-06 at close-of-shop)

- [ ] **Object heights → collision (rugs walkable).** Every furniture item currently collides as a
      solid oriented box (`in_box` in `floorplan.c`'s `passable()`), so a rug/mat/floor-runner blocks
      the player. The `.fml` items carry a 3D height / `z` — read it and SKIP collision for low/flat
      items (below a height threshold, or tagged floor-level). This is the proper version of the
      "don't drop big flat items" call: keep them, draw them, but walk over them.
- [ ] **Better colour — true 32-bit RGB fallback.** Sprites + floor textures currently quantise to
      the 32-colour palette → grey materials go flat, busy furniture goes rainbow. The engine
      supports 32-bit colour, so explore storing/drawing furniture renders and floor textures in
      true RGB (a real-colour `pset`/image-blit path) instead of palette indices. Biggest single
      lever on "it looks muddy". (Check what true-colour draw studio.h already exposes.)
- [ ] **Z-ordering (`z` + `z_height`).** Draw order is a fixed paint sequence (areas → surfaces →
      furniture → walls → doors → player) and furniture is drawn in array order. Sort draws by base
      `z` + object height so taller things layer over shorter, surfaces/rugs sit under furniture,
      and the player occludes / is occluded correctly. Items carry `z`; pair with the height work above.

### Smaller / cosmetic
- [ ] **Low-contrast materials quantise flat** — e.g. "Hatch Tiles Grey" collapses to one palette
      grey, so it reads as a flat floor. Could dither, or widen the palette match. (Subsumed by the
      true-32-bit-colour idea above.)
- [ ] **Bake textures into the BAKED cart too** — `fml-textures.js` is `--json`-only; `floorwalker`/
      `seinelaan` (the spliced-C carts) still get flat `AREA_COLORS`. Add a C-splice path if wanted.
- [ ] **`areas[]` real colours** — still uses the synthetic rainbow for legibility; textures now
      cover the floors that matter, so this is lower priority.
- [ ] **Door look tuning** — swing-symbol colour (`C_DOOR`, currently orange) / size; confirm exits are
      easy to spot on every floor colour. Maybe per-floor tint.
- [ ] **Doorway width feel** — widen the cut slightly if the player clips the frame.
- [ ] **Multi-floor** — files have several floors (`--floor`), but a cart only loads one. Stairs / floor
      switching not handled.
- [ ] **Missing renders** — some refids have no published top-down render (fall back to boxes). Could log
      which, or find an alternate view.
- [ ] **`--maxfurn` tuning** — items above the cap are dropped as surfaces; revisit so large-but-real
      furniture (sectionals, counter runs) isn't lost.
- [ ] **Gameplay** — it's a walk-through tech demo. The leap to Hotline-style play: enemies, line-of-sight,
      a goal, weapons (reuse `hotline.c` AI/combat over this geometry).
- [ ] **Look knobs** — settle defaults for `--saturate` / `--posterize`; consider a custom furniture palette.
- [ ] **Perf** — render-time outline is ~5× the blit; fine with culling, but watch it on dense floors.
- [ ] **In-cart loader** (planned, parked 2026-07-13) — a start screen to pick an already-fetched plan
      or type a new id, via a `floorplanner.js --serve` fetch-bridge (JS route, no cart-side HTTPS).
      `.token` paste-once auth is the prerequisite and is DONE. Full plan + the rejected cart-HTTPS
      alternative: `docs/design/external-data-carts.md` → "Loader — planned".
