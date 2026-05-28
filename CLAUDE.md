# eventually

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
└── VISION.md           # design notes — goals, open questions, history
```

## How ▶ run works

1. Editor exports sprite sheet from tilemap canvas → `build/sprites.png`
2. Copies `dos_8x8.png` font → `build/dos_8x8.png`
3. Writes editor code → `build/cart.c`
4. Compiles: `clang cart.c studio.c -I runtime -I raylib/include libraylib.a -framework ... -DSCREEN_W=X -DSCREEN_H=Y -DSCALE=Z -o build/cart`
5. Spawns `build/cart` with `cwd = build/` (so it finds sprites.png and the font)

Raylib is at `/usr/local/opt/raylib` (installed via Homebrew).

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
CLR_BLACK/WHITE/RED/GREEN/BLUE  // palette shortcuts (0-4, full palette is 0-31)
```

## Adding a new API function

1. Declare in `runtime/studio.h`
2. Implement in `runtime/studio.c` using Raylib
3. Add to `editor/src/studioDocs.js` — this automatically populates autocomplete, hover tooltips, and the help tab
4. Add the key to the right section in the `sections` array in `editor/src/shell.js`

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

## Key things to know

- `node_modules` requires Node 22 — use `nvm use 22` before any npm commands
- The Electron main process (`main.cjs`) and preload (`preload.cjs`) use CommonJS (`.cjs` extension) because `package.json` has `"type": "module"`
- Changing `main.cjs` or `preload.cjs` requires restarting Electron (`npm start`); Vite hot-reloads everything else
- The build log auto-hides after 3 seconds on success, stays open on compile errors
- `SCREEN_W`, `SCREEN_H`, and `SCALE` are passed as `-D` flags at compile time from the settings tab
- The palette in `studio.c` currently only has 5 colors defined (indices 0–4); the rest are uninitialized (TODO)
