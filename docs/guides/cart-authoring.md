# Cart authoring tools

Scripts in `tools/` for creating and updating `.cart.png` files without the editor UI.
Designed to be used by Claude in-session via the Bash tool.

> **#1 cart gotcha — sticky render state.** Global render state comes in two kinds:
> - **The engine clears these for you each frame** — `clip` (scissor off at frame end),
>   `shake` (decays on its own), and `fade` (zeroed each frame — immediate-mode, see
>   [decision 0010](../decisions/0010-fade-is-immediate-mode.md)). Set them and forget them;
>   re-assert `fade()` on whatever frames you want the screen dimmed (e.g. inside a game-over
>   overlay) and it clears itself the moment that state ends.
> - **You own these — they persist across frames *and game states* until you change them** —
>   `camera`/`camera_ex`, `pal`, `fillp`. A value left by one scope leaks into the next
>   (`mouse_world_*` reading a camera the HUD reset to 0; a leftover `pal()` recoloring the
>   next scene).
>
> Habit: reset the ones you own at the top of `draw()` — `camera(0,0); pal_reset();
> fillp_reset();` — and set the camera in `update()` before reading `mouse_world_*`. (`fade`
> used to be on this list; it no longer needs a manual `fade(0)` — the engine clears it.) See
> the postscript in [`../design/cart-survey-api-priorities.md`](../design/cart-survey-api-priorities.md).

---

## `tools/make-cart.js`

All cart operations go through this one script.

### Create a cart

```bash
node tools/make-cart.js <source.c> <output.cart.png>
```

Embeds the C source, a sprite sheet, and a map into a PNG.
If a `<source>.cart.js` config file exists alongside the `.c` file, sprites and map are
taken from there. Otherwise sprites are blank and the map is empty.

### Bake a real screenshot into an existing cart

```bash
node tools/make-cart.js --run <cart.png>
```

Extracts the cart's source/sprites/map, compiles with clang, runs the binary with
`--screenshot` (3 frames, window flashes briefly), reads the resulting `screenshot.png`,
and writes it back as the cart's visible thumbnail. The data chunks are untouched.

Each `--run` builds into its **own** scratch dir, `build/.bake/<name>/`, and never writes
the shared `build/` dir. So baking a thumbnail is safe to run in parallel (different carts
don't collide) and can't disturb a live (libtcc) editor session, whose host watches
`build/cart.c` and would otherwise hot-swap to whatever you just baked.

**This is the normal finishing step after creating a cart.**

> **Verifying a bake — read the right file.** A `--run` bake's actual output is the
> thumbnail **embedded in the `<cart>.cart.png`** (and its source is `build/.bake/<name>/screenshot.png`).
> Do **not** read `build/screenshot.png` to check a bake — that file belongs to the
> running Electron editor and shows whatever *it* last rendered, so it changes under you
> as the user works and will silently show an unrelated cart. To confirm a bake, view the
> embedded thumbnail (extract the PNG's image, or read `build/.bake/<name>/screenshot.png`).

### Update just the screenshot (manual)

```bash
node tools/make-cart.js --update <cart.png> <screenshot.png>
```

Replaces the thumbnail of an existing cart with any PNG you supply.
Useful if the user ran the cart themselves and you want to pull in `build/screenshot.png`.

---

## Typical workflow for a new tutorial cart

```bash
# 1. write the source
# (edit tools/carts/XX-name.c and optionally tools/carts/XX-name.cart.js)

# 2. generate the cart with sprites + map baked in
node tools/make-cart.js tools/carts/XX-name.c editor/public/carts/XX-name.cart.png

# 3. compile, run 3 frames, bake real screenshot
node tools/make-cart.js --run editor/public/carts/XX-name.cart.png
```

Done. The cart now has the correct thumbnail and can be loaded in the editor.

---

## The `.cart.js` config file

Place a `XX-name.cart.js` file next to `XX-name.c` to supply sprites and/or a map.
It's a CommonJS module — `require()` works, comments work.

```js
module.exports = {
  // optional: custom char→palette-index mapping (merged with default)
  charMap: { 'X': 8 },   // add red as 'X'

  // optional: sprites by slot index
  sprites: {
    1: `...sprite as ASCII art...`,
    2: [/* flat 256-element array of palette indices */],
  },

  // optional: map
  map: {
    layout: [             // one string per row, any width up to MAP_W
      '##########',
      '#        #',
      '#        #',
      '##########',
    ],
    tiles: { '#': 1 },   // char → tile index  (default: '#'→1, ' '/'.'→0)
  },
}
```

---

## Sprite format

Sprites are 16×16 pixels. Each sprite occupies one slot in the 128×128 sheet
(8 columns × 8 rows of slots, slot 0 top-left).

**ASCII art string** (preferred — readable):

```js
sprites: {
  1: `
................
.....YYYYYY.....
....YYYYYYYY....
....YYYYYYYY....
.....YYYYYY.....
....BBBBBBBB....
..BBBBBBBBBBBB..
..BBBBBBBBBBBB..
....BBBBBBBB....
....BBBBBBBB....
....BBBBBBBB....
...BBB....BBB...
...BBB....BBB...
...BBB....BBB...
................
................
`,
}
```

**Flat array** (useful when generated programmatically):

```js
sprites: {
  1: new Array(256).fill(0).map((_, i) => i % 16),
}
```

### Default char map

| Char | Color | Index |
|------|-------|-------|
| `.` `0` | black / transparent | 0 |
| `1`–`9` | palette indices 1–9 | 1–9 |
| `a`–`f` | palette indices 10–15 | 10–15 |
| `K` | blacK | 0 |
| `B` | dark Blue | 1 |
| `P` | dark Purple | 2 |
| `G` | dark Green | 3 |
| `N` | browN | 4 |
| `S` | dark grey (Slate) | 5 |
| `L` | Light grey | 6 |
| `W` | White | 7 |
| `R` | Red | 8 |
| `O` | Orange | 9 |
| `Y` | Yellow | 10 |
| `g` | bright Green | 11 |
| `b` | bright Blue | 12 |
| `I` | Indigo | 13 |
| `k` | pinK | 14 |
| `p` | Peach | 15 |

Add your own via `charMap: { 'X': 8 }` in the config. **A cart's `charMap`
*extends* this default table — you only declare the extra characters you need**
(e.g. `charMap: { 'M': 28 }` to add one extended-palette colour); the standard
letters above keep working in the same sprites. Your entries win on a conflict, so
you can also remap a default letter.

> **Gotcha (fixed Jun 2026):** `charMap` used to *replace* the default table, not
> extend it. A cart that declared `charMap: { 'M': 28 }` silently lost `R`/`W`/`P`/…
> — every sprite using them rendered transparent, so note heads / characters
> vanished while primitive-drawn UI (rects, lines, text) still showed. Symptom:
> "I only see the things drawn with shapes, not the sprites." If you see that on an
> old `.cart.png`, just **rebake it** (`make-cart.js <name>.c …` then `--run`) to
> pick up the merge. `buildSpriteSheet` in `tools/make-cart.js` now does
> `{ ...DEFAULT_CHAR_MAP, ...cartCharMap }`.

> **Extended palette (indices 16–31):** the default char map only reaches index
> 15. To use the extended `CLR_DARKER_*` / `CLR_TRUE_BLUE` / etc. colors in a
> sprite, either use the **flat array** form (palette indices go straight in,
> 0–31) or add the colors to `charMap` yourself. ASCII art alone can't reach
> them. See `tools/carts/textured3d.cart.js` for generated textures via arrays.

### sprite-draw.js — the programmatic sprite library

Don't hand-roll flat arrays: `require('../sprite-draw.js')` from any `.cart.js`
gives a 2D pixel-canvas API whose names match the C drawing API.

- **Canvas + primitives**: `blank(w,h)`, `pixel`, `line`, `rectfill`, `rrectfill`,
  `circlefill`, `ovalfill`, `trifill`, `polyfill`, `ngonfill`, `noise(x,y,mod)`
  (stable speckle textures).
- **Post-processing**: `shade(g, lightDeg)` (auto light/shadow via the curated
  `RAMP_DARKER`/`RAMP_LIGHTER` palette LUTs — one call turns flat fills into lit
  volumes; call *before* `outlined()`), `rotate(g, deg)` / `rotations(g, n)`
  (baked headings you can still outline/shade afterwards, unlike runtime
  `spr_rot()`), `scale2x(g)` (EPX upscale: sketch a boss at 16×16, bake it big),
  `replace(g, from, to)` (bake-time recolor for variants), `clone(g)`,
  `outlined(g)`, `mirror(g)`, `stamp(dst, src, x, y)`.
- **Export**: `flat(g)` → one slot; `split(g)` → 16×16 tiles row-major (a 32×32
  canvas yields `[TL, TR, BL, BR]` — place them at slots `s, s+1, s+8, s+9` so one
  `sspr()` reads the region back).

Worked showcases: **`foundry.cart.js`** (the step-by-step "watch the code draw"
cart — every function gets a frame on stage) and **`monstermix.cart.js`**
(`stamp()` part composition + magic `pal()` indices).

### font-bake.js — real TTF text as sprites

For title screens, logos, and big verdict words: `tools/font-bake.js`
rasterizes text from any TTF (every Google Font works) into the same 2D
canvases sprite-draw uses — at **build time**, so the cart has zero runtime
font code.

The everyday entry point is `bakeBanner` — it fits, centers, outlines and
shadows a word into a fixed slot-rect and returns ready 16×16 tiles:

```js
const { bakeBanner } = require('../font-bake.js')
// 8 cols × 2 rows of slots; color/aa/outline/shadow are palette indices (-1 = off)
const tiles = bakeBanner('fonts/Smokum-Regular.ttf', 'DRAW!', 8, 2,
                         { color: 8, aa: 24, outline: 16, shadow: 20 })
tiles.forEach((t, i) => { sprites[base + ((i / 8) | 0) * 8 + (i % 8)] = t })
```

Because the word is centered in the rect, the C side `sspr()`s a *constant*
sheet region regardless of the word's width — change the text, the C code
doesn't care.

Three rules learned the hard way:

- **`colorkey(0)` in `init()` is mandatory.** There is no default transparent
  color — without it every banner drags its empty slot-rect along as an opaque
  black box (invisible over a black sky, glaring over anything else).
- **Give every word two slot-rows.** One row leaves ~11px glyphs after the
  outline trim — too thin for most display fonts at 2x. Short words can share
  a row pair as half-width rects (highnoon packs five words into exactly 64
  slots that way: two 8×2 + two 4×2 + one 8×2).
- **If the cart recolors the text with `pal()`, remap the `aa` color too** —
  fill and edge are separate palette indices, and swapping only the fill
  leaves a clashing rim.

Lower-level pieces, for custom treatments:

- `bakeText(font, text, {px, color, aa, threshold, pad})` → tight 2D grid; an
  ordinary sprite-draw canvas (`outlined()` it, `stamp()` it, `split()` it).
  `aa` maps partial-coverage edge pixels to a darker companion color so small
  text doesn't look chewed.
- `measure(font, text, px)` → `{w, h}` without rasterizing — loop it to find
  the biggest `px` that fits a budget (bakeBanner does this internally).
- Fonts live in `tools/fonts/` (with their OFL/Apache license file). New ones
  are one curl from the google/fonts repo (check `ofl/` first, then `apache/`):
  ```bash
  curl -sL -o tools/fonts/<File>.ttf \
    "https://github.com/google/fonts/raw/main/ofl/<family>/<File>.ttf"
  ```
- **The sheet is the budget**: 64 slots = 128×128 px of glyph space total. This
  is a banner/title tool, not a text renderer — `print()` stays the workhorse
  for dynamic text.

Worked showcases: **`fontbake.cart.js`** (Bungee; all the treatments, waving
strips, live `pal()` recolor) and **`highnoon.cart.js`** (Smokum; five words
packed to exactly 64 slots — the words ARE the gameplay).

---

## Map format

The map is `MAP_W × MAP_H` bytes (default 128 × 64), one byte per cell = tile index.
Tile 0 is empty; tile 1+ are sprite slot numbers.

```js
map: {
  layout: [
    '##########',     // each string is one row
    '#        #',
    '#  ##    #',
    '#        #',
    '##########',
  ],
  tiles: {
    '#': 1,           // wall = sprite slot 1
    'w': 2,           // water = sprite slot 2
    // space and '.' are always 0 (empty)
  },
}
```

Rows shorter than `MAP_W` are zero-padded. Rows beyond `MAP_H` are ignored.

---

## Cart file locations

| Path | Purpose |
|------|---------|
| `tools/carts/XX-name.c` | cart source code |
| `tools/carts/XX-name.cart.js` | optional sprites + map config |
| `editor/public/carts/XX-name.cart.png` | finished cart (served to the tutorials page) |
| `editor/public/carts/index.json` | metadata list for the tutorials panel |
| `build/.bake/<name>/` | isolated scratch dir for a `--run` bake (its `screenshot.png` is read back into the cart) |
| `build/screenshot.png` | last screenshot from the editor |
| `tools/carts/compile_flags.txt` | clangd flags so the editor lints cart `.c` files correctly |

---

## Editor linting (clangd)

A cart `.c` includes only `studio.h`, with no build flags — so a stock clangd/clang
check can't find the header (`'studio.h' file not found`), and because `studio.h` is
where `<stdbool.h>` gets pulled in, that one miss cascades into a wall of bogus
`unknown type name 'bool'` errors on every `typedef struct { bool … }`. It's all
noise: the real builds (`make-cart.js`, the editor's ▶ run) pass `-I runtime` and
compile fine.

The fix is checked in: **`tools/carts/compile_flags.txt`**.

```
-I../../runtime
-std=c11
```

clangd applies the nearest-ancestor `compile_flags.txt` to each file and resolves
relative `-I` paths from *that file's* directory, so `../../runtime` (from
`tools/carts/`) points at the repo's `runtime/`. Header found → `bool` resolves → the
noise is gone for every cart, present and future. No per-cart pragmas or disclaimers
needed.

> **Why scoped to `tools/carts/`, not a root `compile_flags.txt`.** `runtime/studio.c`
> needs different flags — the Homebrew raylib include path (machine-specific:
> `/opt/homebrew/...` on Apple Silicon vs `/usr/local/...` on Intel) and the
> `build/*.h` headers generated only after a build. A root `-Iruntime` would quiet the
> carts but leave `studio.c` noisy-or-wrong, and hard-coding a raylib path isn't
> portable. If you ever want one central config, a root **`.clangd`** with a
> `tools/carts/**` match block is the cleaner home — but `studio.c` will still lint
> dirty on a fresh clone until `build/` exists, so it's not worth chasing.

---

## Adding a tutorial to the editor

1. Create `tools/carts/XX-name.c` (and optionally `tools/carts/XX-name.cart.js`)
2. Run the two-step build above
3. Add an entry to `editor/public/carts/index.json`:

```json
{
  "title": "X. short title",
  "description": "One sentence. What the student learns.",
  "file": "XX-name.cart.png",
  "kind": ["tutorial"]
}
```

Tags are required: `kind[]` always (`tutorial`, `game`, `tech-demo`, `toy`, …),
`genre` for anything with kind `game` (and optional elsewhere when it aids
browsing — the platformer tutorials carry `"genre": "platformer"`). The
vocabulary and rules live in `tools/lint-carts.js`; validate with:

```bash
node tools/lint-carts.js
```

The tutorials panel picks it up automatically on next reload — no code changes needed.
