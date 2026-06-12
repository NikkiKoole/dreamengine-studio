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

## Open / next
- [ ] **Real photo floor textures** — blocked: areas/surfaces use Roomstyler `rs-####` IDs that don't
      resolve via the render/texture CDN. Need an `rs-####` → texture-filename resolver, then bake
      (fetch JPEG → `sips` to PNG → quantize → tile the room polygon). Until then: distinct flat colours.
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
