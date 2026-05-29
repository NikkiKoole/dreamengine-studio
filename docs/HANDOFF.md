# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.

_Last updated: 2026-05-29_

---

## Where we are right now

- **Debug tools shipped** (`printh` / `watch` / `watch_visible` / crash capture).
  Design notes archived at [`docs/archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).
- **API expansion pass 1 shipped** — math, collision, animation, strings, timer.
  See [`docs/API_RESEARCH.md`](./API_RESEARCH.md) for what's done and what's still open (passes 2–3).
- **Cart format shipped** — `.cart.png` files embed source + sprites + map as `zTXt`
  PNG chunks. The visible image is a screenshot of the game (saved automatically on
  exit). See below for full details.
- **Tutorials page shipped** — 10 tutorial carts in `editor/public/carts/`, a gallery
  panel in the editor, and a cart authoring toolchain in `tools/`. See
  [`docs/TOOLS.md`](./TOOLS.md) for the full workflow.

---

## Next on the todo list (by impact)

1. **Inline error markers** — map clang `cart.c:line:col` errors to CodeMirror gutter
   marks. The compile pipe is already wired in `main.cjs`. Tightest follow-on to the
   debug tools.
2. **`follow(target_x, target_y, world_w, world_h)`** — camera follow helper proposed
   in API_RESEARCH §8. Currently inlined in `tools/10-world.c`; should be a real API
   function. One-liner internally.
3. **Persistence** — `save(slot, val)` / `load(slot)`, 64 int slots per cart stored as
   `build/cart.dat`. Proposed in API_RESEARCH §6. Trivial in main.cjs land.
4. **Noise** — `noise(x)`, `noise2(x,y)`, `noise3(x,y,z)`. Perlin/value noise.
   Proposed in API_RESEARCH §5.
5. **Process/coroutine model** — the Level-2 differentiator from VISION.md. Weeks of
   architectural work.
6. **Browser sharing** — emscripten build so carts run in a browser tab.

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

10 tutorial carts are already shipped (01-hello through 10-world).

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
- **`follow()`** is in API_RESEARCH but NOT yet implemented in studio.c — cart 10
  inlines the camera math manually. Don't use it until it's shipped.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
