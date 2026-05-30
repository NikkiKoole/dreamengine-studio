# dreamengine

A fantasy console / learning programming environment. Write C, hit run, get a native game window. Inspired by PICO-8, DIV Game Studio, and BlitzMax.

## Running the editor

```bash
cd editor
nvm use 22
npm start          # starts Vite + Electron together
```

The Electron window opens automatically once Vite is ready at `localhost:5173`. The browser tab also works for editing but the **▶ run** button only works inside Electron (it needs to spawn the compiler).

## Project structure

```
eventually2/
├── runtime/
│   ├── studio.h        # the public API — all constants, function declarations
│   └── studio.c        # Raylib implementation of the API + main()
├── editor/
│   ├── electron/
│   │   ├── main.cjs    # Electron main process — compiles + runs cartridges
│   │   └── preload.cjs # exposes window.studio.run() and window.studio.saveSprites()
│   ├── src/
│   │   ├── shell.js/css    # IDE chrome — tabs, run button, build log
│   │   ├── main.js         # CodeMirror editor setup
│   │   ├── sprite-editor.js/css  # pixel sprite editor
│   │   ├── studioDocs.js   # single source of truth for API docs (autocomplete + hover + help tab)
│   │   ├── settings.js     # screen size + scale, persisted to localStorage
│   │   └── theme.js        # custom CodeMirror theme (unused — currently oneDark)
│   ├── public/
│   │   ├── ComicMono.ttf       # editor font
│   │   ├── dos_8x8.png         # in-game bitmap font (loaded via LoadFontFromImage)
│   │   ├── Ac437_Acer_VGA_8x8.ttf  # DOS OEM font (TTF backup)
│   │   └── palettes/pico32.json    # 32-color palette used by sprite editor
│   └── index.html      # shell HTML — all panels live here
├── build/              # compile output (cart.c, cart binary, sprites.png, fonts)
└── docs/               # all project docs — start at docs/README.md (the map)
    ├── VISION.md       #   why & what; STATUS.md = shipped/open/cut ledger
    ├── decisions/      #   ADR-lite: frozen rationale ("why we (didn't) do X")
    ├── design/         #   api-notes.md, audio-notes.md (design exploration)
    └── guides/         #   cart-authoring.md, sharing.md (how-to)
```

## How ▶ run works

1. Editor exports sprite sheet from tilemap canvas → `build/sprites.png`
2. Copies `dos_8x8.png` font → `build/dos_8x8.png`
3. Writes editor code → `build/cart.c`
4. Compiles: `clang cart.c studio.c -I runtime -I raylib/include libraylib.a -framework ... -DSCREEN_W=X -DSCREEN_H=Y -DSCALE=Z -o build/cart`
5. Spawns `build/cart` with `cwd = build/` (so it finds sprites.png and the font)

Raylib is installed via Homebrew. `main.cjs` auto-detects the path: `/opt/homebrew/opt/raylib` on Apple Silicon, `/usr/local/opt/raylib` on Intel.

## The runtime model

- `studio.c` owns `main()` — opens the Raylib window, runs the game loop
- User implements `draw()` (required) and `update()` (optional, has a weak stub)
- Rendering: draw calls go into a `RenderTexture2D` at native resolution, then scaled up to the window
- Screen is 320×200 by default, 4× scale = 1280×800 window (all configurable in settings tab)

## studio.h API

```c
// input
bool btn(int player, int button);   // player 0=WASD+ZX, player 1=Arrows+,.

// graphics
void cls(int color);
void spr(int index, int x, int y);
void print(const char *text, int x, int y, int color);
void rect(int x, int y, int w, int h, int color);       // border
void rectfill(int x, int y, int w, int h, int color);   // filled
void circle(int x, int y, int radius, int color);       // border
void circlefill(int x, int y, int radius, int color);   // filled
void line(int x1, int y1, int x2, int y2, int color);
void pixel(int x, int y, int color);

// constants
SCREEN_W, SCREEN_H          // canvas dimensions
BTN_UP/DOWN/LEFT/RIGHT/A/B  // button indices
CLR_BLACK, CLR_DARK_BLUE, ... CLR_PEACH  // all 32 PICO-8 palette colors (0-31), named in studio.h
```

## Adding a new API function

A new function (or constant) has to land in **four** places to be fully wired up.
Miss one and it either won't compile, won't autocomplete, or won't show in the help
tab. Do all four in the same change:

1. **Declare in `runtime/studio.h`** — with a trailing `//` comment. That comment is
   the human-facing one-liner; keep it tight and beginner-readable (it's the house
   style — look at the neighbours).
2. **Implement in `runtime/studio.c`** using Raylib. Respect `camera()`/`clip()` and
   the palette-index convention (colors are `0–31`, not RGB) like the existing calls.
3. **Document in `editor/src/studioDocs.js`** — this single source drives autocomplete,
   hover tooltips, *and* the help tab. Each entry is keyed by the bare name and needs
   **three** fields — `sig`, English `doc`, **and a Dutch `docNL`** (every entry is
   bilingual — don't skip the translation):
   ```js
   shake: { sig: 'void shake(float amount)',
            doc:   'Kick the screen by `amount` pixels; decays on its own. Call on impacts.\nshake(4);',
            docNL: 'Schud het scherm met `amount` pixels; dooft vanzelf uit. Roep aan bij klappen.\nshake(4);' },
   ```
   - `sig` must match the `studio.h` declaration exactly.
   - Use `\n` for line breaks; end the `doc` with a tiny one-line usage example.
   - **Constants get entries too** — `sig` is the `#define` line, e.g.
     `KEY_SPACE: { sig: '#define KEY_SPACE 32', doc: '…', docNL: '…' }`.
4. **List the key in `editor/src/shell.js`** — add the name to the `keys` array of the
   right section in the `sections` array (this controls grouping + display order in
   the help tab). Add constants here too (e.g. the `KEY_*`, `FILL_*`, `CLR_*` keys). If
   it's a genuinely new category, add a new `{ title, titleNL, keys }` section — note
   `titleNL` is the Dutch section title.

**Then usually: ship a cart that exercises it, with a screenshot.** Most new API
should come with either a numbered **tutorial cart** (if it teaches a concept) or an
**example cart** (if it shows off a feature) — and bake a real screenshot thumbnail
for it. See "Tutorial carts" below; the short version is `tools/<name>.c` →
`node tools/make-cart.js …` → `node tools/make-cart.js --run …` (bakes the screenshot)
→ register in `editor/public/carts/index.json`. A new primitive without a cart
demonstrating it tends to go unnoticed and undertested.

> Note: API signatures churn while a feature is being designed — if you're editing
> `studio.h`, re-read the *current* declaration before updating `studioDocs.js`/`shell.js`
> rather than trusting an earlier draft, and keep all four places in sync.

## Sprite editor

- Ported from `../eventually` (the sibling project)
- 16×16 sprites, 256 sprite slots (16×16 grid), pico32 palette
- Tools: pixel, fill, select, stamp, line/circle/rectangle
- Frames: animation frame strip at the bottom, 1/2/3/4/d keys navigate
- The tilemap canvas IS the sprite sheet — click a tile to edit it
- Auto-exported as `build/sprites.png` on every run

## Fonts

- **Editor**: Comic Mono (TTF, loaded via `@font-face`)
- **In-game**: `dos_8x8.png` — a 145×145 bitmap sheet, 16×16 grid of 8×8 glyphs, yellow separator lines, loaded with Raylib's `LoadFontFromImage(image, YELLOW, 0)` — no TrueType rasterization, raw pixels

## Tutorial carts

Carts (tutorials + example games) show up in the tutorials panel, driven by `editor/public/carts/index.json`. Each `.cart.png` in `editor/public/carts/` is a valid PNG with source/sprites/map embedded as zTXt chunks (`de:source`, `de:sprites`, `de:map`, `de:settings`). The visible PNG image is the thumbnail. The `de:settings` chunk carries the cart's intended screen/scale/cell/map dims (`{screenW,screenH,scale,cellW,cellH,mapW,mapH}`) so loading it restores that config instead of inheriting the editor's current globals — without it a cart can render wrong (e.g. its map tiles drawn at the wrong `CELL_W`).

Source-of-truth files live in `tools/carts/`; the build tool sits beside that folder:
- `tools/carts/<name>.c` — the cart's C source
- `tools/carts/<name>.cart.js` — *optional* config (sprites and/or tile map); only needed if the cart uses them
- `tools/make-cart.js` — the build tool (CommonJS; uses `require`)

**Adding a new cart:**

1. Write the C source → `tools/carts/<name>.c`
2. *(Optional)* Add sprites/map → `tools/<name>.cart.js`. Exports `{ sprites, map, charMap, mapW, mapH }`:
   - `sprites`: `{ slotIndex: asciiArt }` — 16×16 strings, chars map to palette indices via the `DEFAULT_CHAR_MAP` in `make-cart.js` (`R`=red, `W`=white, `b`=bright blue, `.`=transparent/black, etc.)
   - `map`: `{ layout: ["####", "#..#"], tiles: { '#': 1 } }`
3. Build the cart (placeholder thumbnail):
   ```bash
   node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png
   ```
4. Bake a real screenshot as the thumbnail — compiles, runs 3 frames in a hidden window (`--screenshot` mode in `studio.c`), re-embeds `build/screenshot.png`:
   ```bash
   node tools/make-cart.js --run editor/public/carts/<name>.cart.png
   ```
5. Register it — add an entry to `editor/public/carts/index.json`:
   ```json
   { "title": "...", "description": "... + controls", "file": "<name>.cart.png" }
   ```

**Other `make-cart.js` commands:**
```bash
node tools/make-cart.js --update <cart.png> <screenshot.png>  # swap in a thumbnail manually
```

Note: `make-cart.js` is run with plain `node` (it's CommonJS via `require`, not affected by `editor/package.json`'s `"type": "module"` since it lives in the repo root `tools/`, which has no `package.json`).

## Key things to know

- `node_modules` requires Node 22 — use `nvm use 22` before any npm commands
- The Electron main process (`main.cjs`) and preload (`preload.cjs`) use CommonJS (`.cjs` extension) because `package.json` has `"type": "module"`
- Changing `main.cjs` or `preload.cjs` requires restarting Electron (`npm start`); Vite hot-reloads everything else
- The build log auto-hides after 3 seconds on success, stays open on compile errors
- `SCREEN_W`, `SCREEN_H`, and `SCALE` are passed as `-D` flags at compile time from the settings tab
- The palette in `studio.c` is the full 32-color PICO-8 palette (indices 0–31), matching the sprite editor's `pico32.json`. All 32 are named `CLR_*` in `studio.h`
- Cart code shares one namespace with the whole `studio.h` API, so **don't name a variable after a built-in function**. `map` is the common trap — a tilemap/grid array called `map` clashes with the `map()` draw function (`error: redefinition of 'map' as different kind of symbol`); use `grid`/`dmap` instead. Same goes for `line`, `rect`, `circ`, `print`, `spr`, `timer`, `now`, etc.
