# Cart authoring tools

Scripts in `tools/` for creating and updating `.cart.png` files without the editor UI.
Designed to be used by Claude in-session via the Bash tool.

> **Deciding *what* to build?** [`design/cart-library-direction.md`](../design/cart-library-direction.md)
> is the "what to build next" memo — the missing teachable games and the on-brand toys, plus
> § 2b's scored shortlist of new instruments (ranked by how much *new sound* each adds). For a
> sound/instrument cart specifically, [`instrument-carts.md`](instrument-carts.md) is the shelf
> (find the closest cousin to copy) and [`instrument-recipes.md`](instrument-recipes.md) is the
> palette (every grabbable preset, by engine).

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

Other optional keys: `screenW`/`screenH`/`scale` (canvas), `cellW`/`cellH`/`mapW`/`mapH`
(tiles), `touchControls: true` (on-screen stick + A/B), and **`worklet`** — the web
**AudioWorklet** backend (dedicated-thread, sample-tight audio vs the main-thread
ScriptProcessor). It's **auto-on for `kind: "instrument"` carts** (incl. radios) — no code
needed; set `worklet: true` to opt a *non*-instrument cart in, or `worklet: false` to opt out.
Rationale + build: [`../design/audio-threading.md`](../design/audio-threading.md).

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

> **Which carts need rebaking?** `node tools/cart-status.js` flags every `.cart.png`
> whose embedded `de:source` no longer matches `tools/carts/<name>.c` (the editor loads
> the *embedded* source, so a stale embed silently ignores your code edits — the classic
> "the cart ignores my changes" trap). It also reports publish status. Run it after a
> round of cart edits; `--quiet` exits non-zero if anything's pending.

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

## Cart-land library headers — `#include` superpowers

Beyond `studio.h`, `runtime/` carries a small set of **library headers**: pure
cart-land C you `#include` when a cart needs a capability the engine
deliberately doesn't own ([ADR-0006](../decisions/0006-library-carts-not-engine.md)
— richness lives in readable cart code, not the kernel). Everything in them is
`static` (each cart compiles its own copy), tunables are `#define`d *before*
the include, and they never touch engine internals or the tcc symbol table.

| Header | What it gives you | When to reach for it | Showcase carts |
|---|---|---|---|
| `ui.h` | cross-input widgets: `ui_button`/`ui_slider`/`ui_knob` + per-finger drag-capture, fat-finger hit pads, opt-in keyboard/gamepad focus ring, **`ui_loupe(1)` magnifier** (drag a corner handle → a 3× lens over the tiny widgets for fat-finger editing on a phone) | **editing a value** (slider/knob) or a **panel of mixed widgets** you want keyboard-navigable — don't hand-roll the drag-capture machine. **Small knobs/sliders cramped on a phone? `ui_loupe(1)` is one line** (see `uiloupe`, `docs/design/loupe-notes.md`). NOT for plain hit-targets (next row) | `uikit`, `sfxgen`, `uiloupe` |
| `studio.h` `tapp()`/`tapr()` (not a header — the built-in) | "did a touch begin / end in this rect this frame" — edge-triggered, stateless, no capture | **discrete hit-targets on your own drawn surface**: toggle a grid cell, a preset chip, a transport button, a piano key | `mt70`, `drummachine`, `mallet` |
| `gestures.h` | per-finger swipes judged at lift (`swiped`/`swiped_in`) + **`pinch_scale()`** (two-finger pinch → per-frame zoom factor; feed `camera_ex`) | touch games where fingers act independently (a swipe must survive other fingers drumming); **`pinch_scale` is the mobile zoom for a dense grid/keybed you SWEEP** (vs `ui_loupe` for tiny widgets you precision-target) | `touchlab` (test 9), `modrack` (pinch-zoom) |
| `improv.h` | melodic improvisation helpers — the *computer* solos | generative music carts that want an **auto**-soloist | `cocktail`, `roadhouse` |
| `radio.h` | radio-station chrome: chassis draw blocks, seeded-song plumbing (PRNG/clock/voice-leading/input), draggable control knobs (`rad_knob_int`/`_sel`/`_float`) | building a new generative radio station | the radio carts, `tango`, `yacht` |
| `solo.h` | the jam layer: a scale-locked solo strip the *player* drives (mono + glide, chord-tone highlight, per-station vertical expression `SOLO_Y_*`, octave shift, tap-to-toggle) on `ui.h` capture | letting a listener **play along** with a generative radio without playing wrong — pairs with `radio.h`. NOT for stations with no soloist (`ambient`, `satie`) | `bossa`, `citypop`, `dub`, `jangle`, `jingle` |
| `keybed.h` | a polyphonic **chromatic keybed**: white/black keys played by touch (multitouch + glissando) + mouse + QWERTY rows + a plugged-in **MIDI keyboard** (engine `midi_get()`), all → `note_on`/`note_off` into one slot, with chords + velocity. Voices keyed by MIDI note; default drawn manual or geometry helpers (`keybed_white_rect`/`keybed_black_rect`/`keybed_midi_at` + `keybed_held`/`keybed_glow`/`keybed_handle`) for a custom look | a **piano/synth keyboard** instrument cart — **never hand-roll the key tables + per-finger pool + glissando again**. Config by white-key count (`keybed_config(slot, baseOct, nwhite)`). NOT for continuous-pitch **ribbons** (`martenot`, `heldnotes`) or the radio scale strip (`solo.h`) — those share only the engine `midi_get()` layer. See `docs/design/midi-and-keybed.md` | `keybedtest` (epiano/moog/touchpiano/mellotron: migration pending) |
| `shadermath.h` | a GLSL-vocabulary toolkit for **CPU shaders**: `vec2`/`vec3` + `dot`/`normalize`/`length`/`cross`, the scalar idioms `studio.h` lacks (`fract`/`smoothstep`/`mix`/`step`/`sat`), colour helpers (`rgb`/`hsv`/`vrgb`/`palette()` — the Iñigo-Quílez cosine palette), and signed-distance-field primitives + combinators (`sd_sphere`/`box`/`torus`/`plane`, `op_union`/`op_sub`/`op_smin`) | a cart drawing per-pixel through `pset_rgb`/`rectfill_rgb` that wants its math to read like Shadertoy GLSL — gradients, distance fields, **raymarching**. Include **after** `studio.h` (it uses `clamp`/`fsqrt`/`sin_deg`/`cos_deg`) | `raymarch` |

**Reading them:** each header opens with its own manual (why it exists, usage,
tunables). In the editor they're all listed under the **docs tab → "engine
source"** sidebar group (alongside `studio.h`); or **cmd-click the filename**
of an `#include "ui.h"` in your cart and it jumps to that same view.

**Writing one:** a new library header must follow the same contract (all
`static`, `#define` tunables, header-comment manual) and ships per the
second-customer rule — a showcase cart *plus* one real cart using it.

### Touch input: `ui.h` vs `tapp()` — the decision (settled 2026-06-07)

Both build on the same virtual-touch pool, so **both are mouse + touch capable
for free** — the desktop mouse arrives as one synthetic finger either way. The
choice is therefore *never* "which one supports touch"; it's **what kind of
control you're drawing:**

- **`ui.h`** — reach for it when you're **editing a continuous value** (a
  slider or knob needs *drag-capture*: the value keeps tracking a finger that
  slid off the widget), or building a **cluster of mixed widgets** that should
  share one keyboard/gamepad focus ring. That drag-capture + focus-ring
  machinery is exactly what you must not hand-roll — it's why `ui.h` exists.
- **`tapp()` / `tapr()`** (built into `studio.h`) — reach for these for
  **discrete hit-targets you draw yourself**: toggle a sequencer cell, pick a
  preset chip, hit a transport button, press a piano key. There's no value to
  track and no capture needed — just "did a tap begin in this rect?". Wrapping
  these in `ui_button` buys nothing and forces `ui.h`'s visual + focus model
  onto a surface you're already drawing in your own style.
- **Multitouch *drag gestures*** (paint across a grid, glissando across keys,
  sweep across mallet bars) are **neither** — they're the per-finger pool
  (`touch_count`/`touch_id`/`touch_ended_*`); `ui.h` captures one contact per
  widget and won't drag *across* targets. `mallet.c`/`drummachine.c` are the
  pattern; `gestures.h` covers swipes/pinch.

Rule of thumb: **value → `ui.h`; hit-target → `tapp()`; drag-across → the
finger pool.** A standalone button is a `tapp()`; a button *inside a `ui.h`
panel* (so it joins the focus ring) is a `ui_button`. The scope boundary lives
in [`../design/ui-widgets-notes.md`](../design/ui-widgets-notes.md).

**Cramped on a phone?** Two on-the-shelf aids, one per interaction style — don't
shrink your layout, magnify the input instead:
- **Precision-target small widgets** (knobs, narrow sliders you grab one at a
  time): `ui_loupe(1)` — a draggable corner-handle lens that magnifies the `ui.h`
  widgets 3× for fat-finger editing. One line. (`uiloupe`,
  [`../design/loupe-notes.md`](../design/loupe-notes.md))
- **Sweep a dense surface** (a step grid you drag-paint, a keybed you glissando):
  whole-view zoom — `gestures.h` `pinch_scale()` × `camera_ex()`. A loupe shows
  only a slice, so it's the wrong tool here. (`modrack` is the worked example.)

## House conventions for instrument carts

> **Building a sound/instrument cart? Start at the shelf:**
> [`instrument-carts.md`](instrument-carts.md). It indexes every existing synth,
> classic-machine homage, held-note instrument, engine showcase, radio station and
> sound toy, grouped by the **building block each one copies** (`radio.h` /
> held-notes / `ui.h` / `INSTR_*`). Find the cart closest to what you're making and
> start from its skeleton instead of a blank file — and add a row there when you ship.

**Piano-style computer keys = the GarageBand musical-typing map** (owner
decision, 2026-06-07 — one layout to learn, not one per cart). Copy `moog.c`'s
arrays verbatim:

```c
const char wkey[]  = "ASDFGHJKL;'";                 // home row = white keys, A = C
int   wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
char  bkey[7]   = { 'W','E','T','Y','U','O','P' };  // QWERTY row = black keys
int   bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
```

Carts using it: `moog`, `mt70` (sh101's two-manual layout is its own beast).
Cart control keys go on the **bottom row** (Z X C V…), which the layout leaves
free. Carts with *bars/pads* instead of a piano (mallet, fm, handpan) use plain
home-row `A S D F G H J K` — same starting key, no blacks.

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
