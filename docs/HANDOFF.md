# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.

_Last updated: 2026-05-29 (session 4)_

---

## Where we are right now

- **Debug tools shipped** (`printh` / `watch` / `watch_visible` / crash capture).
  Design notes archived at [`docs/archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).
- **API expansion pass 1 shipped** — math, collision, animation, strings, timer.
  See [`docs/API_RESEARCH.md`](./API_RESEARCH.md) for what's done and what's still open (passes 2–3).
- **Cart format shipped** — `.cart.png` files embed source + sprites + map as `zTXt`
  PNG chunks. The visible image is a screenshot of the game (saved automatically on
  exit). See below for full details.
- **Tutorials page shipped** — 12 tutorial carts in `editor/public/carts/`, a gallery
  panel in the editor, and a cart authoring toolchain in `tools/`. See
  [`docs/TOOLS.md`](./TOOLS.md) for the full workflow.
- **API pass 2 shipped** — `follow()`, `save()`/`load()`, `noise()`/`noise2()`/`noise3()`,
  inline error markers in the CodeMirror editor.
- **API pass 3 shipped** — `ease_in/out/in_out`, `rnd_between`/`rnd_float`/`rnd_float_between`,
  `print_centered`/`print_right`, `frame()`. 14 tutorial carts total.
- **`tri()` / `trifill()` added** — triangle border and filled triangle.
- **`rect()` corner overshoot fixed** — was using `DrawRectangleLines` then `DrawLine` (both add line caps); now uses four 1px `DrawRectangle` slices. Pixel-perfect corners.

---

## Next on the todo list (by impact)

1. **Remaining pass 2 API** — `broadcast(msg_id)` / `received(msg_id)` event bus
   (touches main-loop drain semantics — small but architectural). Strudel extras:
   `pitch("c4")`, `sometimes`/`often`/`rarely`, `arp`, `stutter`, `palindrome`,
   `off_beat`. Dilla timing: `groove` + `groove_swing/jitter/push`.
2. **Process/coroutine model** — the Level-2 differentiator from VISION.md. Weeks of
   architectural work. `wait N seconds` / `loop … frame;` inside a process.
3. **Browser sharing** — emscripten build so carts run in a browser tab.
4. **Sound tracker UI** — the sound tab is disabled; code-first sound works but a
   tracker would let you design sfx/music in the editor.
5. **Pixel-perfect sprite collision** — walk the sprite alpha; AABB covers 95% of cases
   but this would be the next collision improvement.

---

## Cart format

Carts are `.cart.png` files — a PNG whose visible image is the last game screenshot,
with data embedded as compressed `zTXt` chunks:

| Chunk key | Content |
|---|---|
| `de:source` | C source (cart.c text) |
| `de:sprites` | sprite sheet as a PNG data URL |
| `de:map` | map bytes as base64 |

**Code:** chunk helpers live in `editor/electron/main.cjs` (functions `embedCartChunks`,
`extractCartChunks`, `makeZtxtChunk`, `crc32`). Same logic is duplicated in
`tools/make-cart.js` (standalone, no Electron dependency).

**Screenshot:** `studio.c` saves `build/screenshot.png` (3 frames, vertically flipped
corrected) on every clean exit. The save-cart handler in `main.cjs` prefers
`screenshot.png` over `sprites.png` as the base image.

**`--screenshot` mode:** pass `--screenshot` to the compiled cart binary to render
3 frames and exit automatically (used by `tools/make-cart.js --run`).

**IPC surface** (`preload.cjs` → `main.cjs`):
- `studio.saveCart(payload)` — opens save dialog, writes `.cart.png`
- `studio.loadCart()` — opens file picker
- `studio.loadCartFile(path)` — load from a known filesystem path
- `studio.loadCartBuffer(bytes)` — load from raw bytes (used by tutorials panel)
- `studio.getFilePath(file)` — `webUtils.getPathForFile` (Electron 32+ required)

**Drag-and-drop:** dropping a `.png` onto the editor window loads it as a cart.

---

## Tutorial cart toolchain

Full docs in [`docs/TOOLS.md`](./TOOLS.md). Quick reference:

```bash
# create cart (with optional sprites/map from XX-name.cart.js config)
node tools/make-cart.js tools/XX-name.c editor/public/carts/XX-name.cart.png

# compile + run 3 frames + bake screenshot into cart
node tools/make-cart.js --run editor/public/carts/XX-name.cart.png

# just update the screenshot of an existing cart
node tools/make-cart.js --update <cart.png> <screenshot.png>
```

Cart sources live in `tools/XX-name.c`. Config files (sprites + map) live in
`tools/XX-name.cart.js`. Finished carts go in `editor/public/carts/`.
`editor/public/carts/index.json` is the metadata list the tutorials panel reads.

12 tutorial carts are shipped (01-hello through 12-hiscore).
- 11-noise: two-layer scrolling terrain + twinkling stars via `noise2()`
- 12-hiscore: button-mashing game with `save()`/`load()` high score persistence
- 13-easing: four dots on tracks showing all three easing curves vs linear
- 14-hud: catch-the-shrinking-star game using `print_centered`, `print_right`, `rnd_between`

---

## Gotchas / environment facts

- **`main.cjs` / `preload.cjs` changes need an Electron restart** (`npm start`);
  Vite hot-reloads everything else.
- **`▶ run` only works in Electron** (it spawns clang); the browser tab edits but can't run.
- Use **Node 22** (`nvm use 22`) before any npm command.
- **`--screenshot` mode** opens a real window briefly (Raylib needs a display).
  3 frames is enough for static carts; carts that randomise initial state on frame 1
  will look fine; carts that need user input obviously won't show gameplay.
- **arm64 integer divide-by-zero does NOT trap** (returns 0) — SIGFPE won't fire on
  Apple Silicon. Use a `volatile` null read for reliable test crashes.
- **`follow()`** is now implemented. Cart 10 uses it correctly.
- **`save()`/`load()`** write to `build/cart.sav` (cwd of the running cart). All carts
  share this file for now — per-cart persistence comes with named cart files later.
- **Inline error markers** — red gutter dot + red line background on clang error lines.
  Cleared on next compile. Pattern matched: `cart.c:LINE:` in stderr output.
- **`frame()`** increments in the main loop after `update()`, declared static at the
  top of `studio.c` alongside the other static state.
- **`print_centered`/`print_right`** use `strlen(text) * 8` for width — the dos_8x8
  font is exactly 8px per character with 0 spacing (`DrawTextEx` size=8, spacing=0).
- **`rnd_float()`** uses `rand()` from `<stdlib.h>` (added to studio.c includes).
  All carts share the same `rand()` seed — not seeded per-cart; same sequence every run.
- **CLAUDE_CODE_TMPDIR** fills up occasionally. Workaround: redirect compile output
  to `build/compile-test.log` and read it back with the Read tool.
- **`trifill()` winding order** — Raylib's `DrawTriangle` needs counter-clockwise winding
  in Y-down screen coords. In Y-down space, cross product > 0 means clockwise visually
  (opposite of math convention), so swap when `cross > 0`.
- **`colorkey`** was added to studio.h by the user (visible in the system reminder) —
  check studio.h for current state before adding more graphics functions.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
