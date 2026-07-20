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

> **Two sources of truth.** A `.cart.js` generator and the editor's pixel canvas
> both produce sprites and don't know about each other — hand-painting a generator
> cart's sprites in the editor ships in *that* build but the next bake silently
> reverts it. Generator carts get sprite changes in the generator, hand-drawn carts
> in the editor — never both. The problem + proposed fixes (the "sprite story",
> options A–D): [`../design/editor-cart-workflow.md`](../design/editor-cart-workflow.md)
> §Gap 2 / STATUS item 23.

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
(`stamp()` part composition + magic `pal()` indices). For a self-contained set of
16×16 UI button icons, see **`boom.cart.js`**.

**Iterate without baking the cart.** `node tools/sprite-preview.js <cart>` renders
every slot in the `.cart.js` to one labelled PNG (zoomed, slot numbers, a checker
background so index-0 transparency reads) — no compile, no run. Tweak the drawing
code, re-run, look again in seconds; `--slots 0-5,28` limits it, `--scale`/`--cols`
size it. The output is pixel-identical to the baked sheet (shared palette + encoder).

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
| `lay.h` | immediate-mode **responsive layout**: a `Box{x,y,w,h}` + CSS-flavoured helpers — `lay_split` (dock a fixed band off an edge, remainder in `*rest`), `lay_at` (9-grid anchor = position:absolute + safe-area inset), `lay_cell` (equal flex row/col), `lay_grid` (fixed columns), `lay_wrap` (auto-fit columns), `lay_aspect` (contain a ratio), `lay_lane`/`lay_lane_cell` (a shared **column register** — bind two stacked strips to one `LayLane` so their columns line up by construction; a step roll over a step grid), `lay_fluid`/`lay_clamp`, `lay_pad` (per-side / asymmetric safe-area) + `boxfill`/`boxrect`/`binside` sugar | **any hand-positioned UI** — a HUD, a control panel, a menu, a docked bar — instead of hardcoding x/y math; and the basis for **device-adaptive** racks (feed it a different container and the layout reflows). rect-in/rect-out, no retained tree, called every frame. See [device-adaptive-layout.md](../design/device-adaptive-layout.md) + [responsive-layout.md](../design/responsive-layout.md) | `respond`, `rackfit`, `acidfit` |
| `ui.h` | cross-input widgets: `ui_button`/`ui_slider`/`ui_knob` (+ the Box/cell-sized twins `ui_button_cell`/`ui_knob_cell` — pass a `lay.h` cell, the widget owns its sizing) + per-finger drag-capture, fat-finger hit pads, opt-in keyboard/gamepad focus ring, **`ui_loupe(1)` magnifier** (drag a corner handle → a 3× lens over the tiny widgets for fat-finger editing on a phone) | **editing a value** (slider/knob) or a **panel of mixed widgets** you want keyboard-navigable — don't hand-roll the drag-capture machine. **Small knobs/sliders cramped on a phone? `ui_loupe(1)` is one line** (see `uiloupe`, `docs/design/loupe-notes.md`). NOT for plain hit-targets (next row) | `uikit`, `sfxgen`, `uiloupe` |
| `face.h` | the **device-face GRAMMAR** on top of `lay.h`: declare a `FaceZone[]` (`FACE_BAND`/`FACE_LANE`/`FACE_HERO`, top/bottom + height-fraction) and `face_layout()` owns the chunky-canvas resize (`face_resize`), the safe-area content box (`face_area`), carving the zones (declaration order = visual top→bottom), and the shared per-step register (`face_col`). **Enforces the arrangement principles** — bands dock top/bottom only (no width-stealing side-rail), the lane is full-width, the hero is the remainder | a **new device-face instrument** (a 303/drum/synth face) — declare the layout instead of re-copying the resize + zone-carving boilerplate and re-deriving the rules. Scaffold not straitjacket: zones are plain `Box`es, a bespoke face skips `face_layout()`. See [responsive-first-device-face.md](../design/responsive-first-device-face.md) L3 + [device-face-paradigm.md](../design/device-face-paradigm.md) | `facedemo` (grammar) · `deviceface` (the raw-`lay.h` mechanism it replaces) |
| `studio.h` `tapp()`/`tapr()` (not a header — the built-in) | "did a touch begin / end in this rect this frame" — edge-triggered, stateless, no capture | **discrete hit-targets on your own drawn surface**: toggle a grid cell, a preset chip, a transport button, a piano key | `mt70`, `drummachine`, `mallet` |
| `pointer.h` | a **multi-finger pointer pool**: `PTR_CLEAR`/`PTR_ACQUIRE`/`PTR_FIND` over a cart-defined per-finger struct (first field `int id`) — owns the "find this finger's slot or claim a free one" + release bookkeeping over `touch_count()`/`touch_ended_*()` | a cart whose **fingers drive bespoke targets** (grab a string to bend, sweep a row of bars/keys, pick open space) — the layer *below* `ui.h` (which is for widgets). **Never re-copy the `Ptr`/`NPTR`/`NOID` loop again.** Set pool size with `#define PTR_MAX` before the include. See `docs/design/multitouch-and-generalization-audit.md` | 15 carts: `pluck`, `mallet`, `tabla`, `handpan`, `fm`, `pd`, `bowed`, `guitar`, `brass`, `spacecho`, `pedalboard`, `drummachine`, `tb303`, `groovebox`, `sh101` |
| `gestures.h` | per-finger swipes judged at lift (`swiped`/`swiped_in`) + **`pinch_scale()`** (two-finger pinch → per-frame zoom factor; feed `camera_ex`) | touch games where fingers act independently (a swipe must survive other fingers drumming); **`pinch_scale` is the mobile zoom for a dense grid/keybed you SWEEP** (vs `ui_loupe` for tiny widgets you precision-target) | `touchlab` (test 9), `modrack` (pinch-zoom) |
| `disclose.h` | **device-adaptive panel disclosure**: a priority + finger-footprint budget pass that shows the panel you're working on large, the next few smaller, the rest as a glance, and reflows that split as the viewport changes | a **dense rack** with more panels than a phone can show at once — disclose them instead of cramming everything on. Pairs with `lay.h`. See [device-adaptive-layout.md](../design/device-adaptive-layout.md) R2 + [acidrack-layout-brief.md](../design/acidrack-layout-brief.md) §2 | `acidwire` |
| `cursor.h` | a **pixel mouse cursor** drawn into the canvas: `cursor_draw(shape)` (`CUR_ARROW`/`HAND`/`GRAB`/`CROSS`/`MOVE`) + `cursor_draw_tint(shape, col)`. Auto-hides the OS pointer once a real mouse is seen and leaves it alone on touch. Call **last** in `draw()` | a **mouse-driven** cart (editor/tool/sandbox) that wants a pointer matching the pixel art — and one that **shows up in screenshots/clips** (the OS cursor never does, since the capture is the render texture). Tint a crosshair to the active tool. NOT needed on touch-only carts. Built on the engine `mouse_hide()`/`mouse_show()` primitives | `logic`, `dreamcad`, `dune`, `heroes`, `modrack`, `advancewars`, `beltworks`, `acidcandy` |
| `fxicons.h` | the shared **visual language for the engine effects**: one little icon + a body/accent colour per `FX_*` effect, so every cart's "pedals" read the same. Write the icon once, place it many ways (the `ui.h` cart-land discipline). Sibling of `ampcab.h` | a cart drawing **effect toggles / stompboxes / mode chips** — a pedalboard, an instrument's mode switches, any effect-toy — reuse the glyph set instead of redrawing per cart | `pedalboard`, `epiano` |
| `json.h` | an **EXPERIMENTAL cart-land JSON reader** for data-driven carts: read `de_data_path()` (the `--data <file>` flag / `$DE_DATA`) and parse it at RUNTIME instead of baking C arrays into the source | a cart that **loads external data at runtime** (levels, maps, vector features) so you swap the data file without regenerating the cart. See [external-data-carts.md](../design/external-data-carts.md) | `citydrive`, `floorplan`, `roadview` |
| `endcard.h` | the shared **win/lose end-screen treatment**: `endcard(t, w, h, y, bg, edge, edge2)` = an eased ordered-dither curtain that dims the world (in-canvas — deliberately NOT `fade()`, which dims the card too and never shows in dumps/gifs/thumbnails), a drop-shadowed double-bordered card that pops in (`ease_back`), and a returned `EndCard` rect + `.settled` flag you print your own title/lines/blinking prompt against. `endcard_dim(t)` is the curtain alone | **any game with a win/lose/game-over screen** — the hand-rolled `fade(0.5f)` + instant box reads as unfinished (the recurring "end-fade issue", [action-plan.md](../design/action-plan.md)) | `larry`, `monkey`, `hotline` (its panel also serves the title card) + 20 more — every win/lose game converted in the 2026-07-10 sweep |
| `improv.h` | melodic improvisation helpers — the *computer* solos | generative music carts that want an **auto**-soloist | `cocktail`, `roadhouse` |
| `radio.h` | radio-station chrome: chassis draw blocks, seeded-song plumbing (PRNG/clock/voice-leading/input), draggable control knobs (`rad_knob_int`/`_sel`/`_float`) | building a new generative radio station | the radio carts, `tango`, `yacht` |
| `solo.h` | the jam layer: a scale-locked solo strip the *player* drives (mono + glide, chord-tone highlight, per-station vertical expression `SOLO_Y_*`, octave shift, tap-to-toggle) on `ui.h` capture | letting a listener **play along** with a generative radio without playing wrong — pairs with `radio.h`. NOT for stations with no soloist (`ambient`, `satie`) | `bossa`, `citypop`, `dub`, `jangle`, `jingle` |
| `keybed.h` | a polyphonic **chromatic keybed**: white/black keys played by touch (multitouch + glissando) + mouse + QWERTY rows + a plugged-in **MIDI keyboard** (engine `midi_get()`), all → `note_on`/`note_off` into one slot, with chords + velocity. Voices keyed by MIDI note; default drawn manual or geometry helpers (`keybed_white_rect`/`keybed_black_rect`/`keybed_midi_at` + `keybed_held`/`keybed_glow`/`keybed_handle`) for a custom look | a **piano/synth keyboard** instrument cart — **never hand-roll the key tables + per-finger pool + glissando again**. Config by white-key count (`keybed_config(slot, baseOct, nwhite)`). NOT for continuous-pitch **ribbons** (`martenot`, `heldnotes`) or the radio scale strip (`solo.h`) — those share only the engine `midi_get()` layer. See `docs/design/midi-and-keybed.md` (incl. **exceptions**: `mt70`, `sh101` wire MIDI cart-side via `midi_get()` instead — keybed.h adoption deferred) | `epiano`, `moog`, `touchpiano`, `mellotron`, `organ`, `solina`, `piano`, `keybedtest` |
| `worldnet.h` | the infinite deterministic **world spine** (worldgen-plan rung 1): the terrain heightmap + ranked hub/town lattice + terrain-aware spline links from `roadnet2`, materialized into a typed edge graph (edge-type field: road/rail/water) with **`wn_road_at(x,y)`** — the ONE nearest-edge query (surface + class + pavement + centre-line distance) over a per-anchor cache + spatial hash — plus `wn_nearest_road_point()` to spawn a car ON the network. 1 unit = 1 m, y-down; pure functions of (cell, `P_*` params, `seedZ`) → same seed = same world in every consumer | a cart that wants to **drive/query the procedural world** behind a `road_at()`-style seam — never re-roll your own street field (that's how roadnet v1 died; ONE data model). Include after `studio.h`; consumers may stroke the shared `eg_e[]` edge cache directly (screen == query). Ladder: [worldgen-plan.md](../design/worldgen-plan.md) | `roadnet2` (the spine's home + tuning bench), `sloop` (drives it — press N) |
| `shadermath.h` | a GLSL-vocabulary toolkit for **CPU shaders**: `vec2`/`vec3` + `dot`/`normalize`/`length`/`cross`, the scalar idioms `studio.h` lacks (`fract`/`smoothstep`/`mix`/`step`/`sat`), colour helpers (`rgb`/`hsv`/`vrgb`/`palette()` — the Iñigo-Quílez cosine palette), and signed-distance-field primitives + combinators (`sd_sphere`/`box`/`torus`/`plane`, `op_union`/`op_sub`/`op_smin`) | a cart drawing per-pixel through `pset_rgb`/`rectfill_rgb` that wants its math to read like Shadertoy GLSL — gradients, distance fields, **raymarching**. Include **after** `studio.h` (it uses `clamp`/`fsqrt`/`sin_deg`/`cos_deg`) | `raymarch` |
| `physics.h` | a tiny shared **verlet (position-based) toolkit**: a `PhysPt{x,y,px,py,w,r}` the cart owns (inverse-mass `w`, radius `r`; `w=0` = pinned/grabbed) + the verbs `phys_integrate` / `phys_link` (rigid distance stick) / `phys_spring` / `phys_collide` (circle↔circle) / `phys_collide_seg` (point↔segment) / `phys_bounds` (AABB clamp + bounce + floor friction) / `phys_aim` (angular spring) + `phys_grab`/`phys_kick`/`phys_vx`. The cart keeps its own arrays + step loop. Include **after** `studio.h` | any cart with **ropes, cloth, ragdolls, blobs, chains, bouncy balls, or stick-built pseudo-solids** — the recurring hand-rolled verlet loop, now shared (the math had been copy-pasted across ~10 carts). NOT a managed world or rigid-body engine — that's the parked **Layer-1 seam** in [physics-notes.md](../design/physics-notes.md); the cart owns its step + any bespoke collision/bounds | `physics` (teaches it inline), `verlet` (toolkit demo), `ragdoll`, `linerider`, `coaster`, `jelly`, `sloop`, `tentacle`, `waterjar` (PBD fluid) |
| `drumkit.h` | a shared **playable drum kit** — the engine has no `INSTR_KIT`, so every drum cart hand-builds ~6-16 voices from primitives. This owns the *skeleton*: a role vocab (`DK_KICK`/`DK_SNARE`/`DK_HHC`/`DK_HHO`/`DK_CLAP`/`DK_TOM_LO`/`DK_TOM_HI`/`DK_CRASH`), a fixed slot layout at a caller-chosen base, and `dk_fire(role, midi, vel)` (pitch is a first-class param — pass 0 for the role default; bend later without a refactor). Each kit's `build(base)` callback owns the *sound*, so distinct machines stay distinct (808≠909). `dk_use(&def, base)`. Built-in `DK_ELECTRO` / `DK_ACOUSTIC` | a cart that needs a **generic drum kit** to play or sample — a kit is a pad **MAP** (N different sounds, one per key at fixed pitch), the opposite of `keybed.h` (which pitches ONE voice), so drums live on the pad row, not the keybed. For a *faithful specific machine* use `tr808.h`/`tr909.h` below instead. **Retrofit candidates** (not yet migrated — share the trigger, keep their sound): `drummachine`/`fingerdrums`, the radio stations, `notebass`/`onenotebass`, `omnichord`, `chordblossom`. Choke groups not wired yet (the engine's `instrument_choke` already does it — tr808/909 use it for hats — `drumkit.h` just doesn't declare one). See [mic-and-sampling.md](../design/mic-and-sampling.md) | `sampler` (drum sources) |
| `acid303.h` | the shared **TB-303 acid voice** (extracted after `tb303`/`acidrack` grew drifting copies): an `Acid` struct + `acid_note`/`acid_ride`/`acid_gate`/`acid_off`/`acid_tie` — a `FILTER_DIODE` held voice with non-retriggering slide, accent filter-env kick, two-decay, soft attack, filter tracking + an octave-down sub-osc. Params floats 0..1 (`ACID_*` enum). `acid_stock()` = vanilla voicing, else Devil Fish wide — both from one header, chosen by data | a cart that wants an **honest 303** — cart owns the PATTERN, header owns the SOUND, so it can't drift. NOT `drumkit.h` (that's the pad MAP; this pitches one voice) | `tb303`, `acidrack`, `acidcandy` (all byte-identical A/B) |
| `tr808.h` | the shared **TR-808 voice bank** (the `acid303.h` move, for drums): `tr808_build(base)` = the 14-slot bank from the reverse-engineered circuit values (six-oscillator metal bank, bridged-T boom kick, dual-mode snare, 3-retrigger clap) + `tr808_fire(base, role, boost, delay, kt, kd, kc)` = the layered trigger + tune/decay/colour knob maths. `TR_*` 16-voice roster; the cart owns the per-voice knob arrays | a cart that wants a **faithful 808** byte-identical to the `tr808` cart — a full machine at a caller-chosen `base` (clear your other voices), richer than `drumkit.h`'s generic 8-role kit | `tr808`, `acidcandy` |
| `tr909.h` | the shared **TR-909 voice bank**: `tr909_build(base)` + `tr909_metal(base, cut, res)` (the metal-highpass XY over hats/crash/ride) + `tr909_fire(...)` + `tr909_fire_stroke(...)` (flam/drag/ratchet — the stroke family). `TR9_*` 11-voice roster; cart owns the knob arrays | a cart that wants a **faithful 909** byte-identical to the `tr909` cart (punchy house kick, FM-clang metal, strokes) | `tr909`, `acidcandy` |
| `harmony.h` | the shared **HARMONY BRAIN** — one 13-function roman-numeral vocab (`HB_I`…`HB_I7`, `hb_off`/`hb_qual`/`hb_fname`) + per-style weighted transition tables (`HB_BOSSA` = bossa's `TRANS` verbatim, `HB_COCKTAIL` = cocktail's `FNEXT`), **bidirectional**: `hb_pick` (generate — the cart's PRNG stays in charge, pinned seeds byte-exact), `hb_suggest` (the table read *forward*: ranked next-chord options + a one-word reason), `hb_analyze`/`hb_chord_fn` (the *inverse*: chords in a KNOWN key → functions; triads map to their seventh family; out-of-vocab = `-1`, never a guess). `hb_selfcheck()` under `-DDE_SPEC` = its own assertions, call from your `spec()` | a cart that **picks or names chords**: a radio's changes generator, a suggester/analyzer toy, a chord track. Voicing is NOT here — `rad_lead_to` (radio.h) stays the voicing layer. Model + research: [harmony-brain.md](../design/harmony-brain.md) | `bossa`, `cocktail`, `chordwise` |
| `ampcab.h` | the shared **guitar amp/cabinet voicing table**: five amp voicings as PRESET BUNDLES of effects we already ship (`instrument_drive` + a `DRIVE_*` waveshaper + `instrument_eq` + glue), pinned as the output stage like `leslie` — NOT new DSP. ONE source of truth so the standalone amp and the pinned cabinet agree | a cart that wants a **guitar amp/cab tone** (clean/crunch/lead voicings) without hand-tuning the drive+EQ+glue stack each time. See [effects-bus-architecture.md](../design/effects-bus-architecture.md) §E | `combo`, `pedalboard`, `afrobeat`, `mixbooth`, `wba` |

**Reading them:** each header opens with its own manual (why it exists, usage,
tunables). In the editor they're all listed under the **docs tab → "engine
source"** sidebar group (alongside `studio.h`); or **cmd-click the filename**
of an `#include "ui.h"` in your cart and it jumps to that same view.

**Writing one:** a new library header must follow the same contract (all
`static`, `#define` tunables, header-comment manual) and ships per the
second-customer rule — a showcase cart *plus* one real cart using it.

**Candidates (not headers yet — waiting on a second customer):**

- **`sched.h` — energy/speed turn scheduler.** The roguelike "who acts next"
  loop: each actor carries a `next`-turn time, the scheduler runs whoever's
  `next` is smallest, then pushes it out by `COST/speed` (higher speed = acts
  sooner & more often, naturally interleaved — the energy-accumulation idea as
  next-event times). It's ~15 generic lines, currently living inline in its
  **showcase `tempo`** (the visible version, with the turn-order ribbon). Source:
  [nadako's turn-scheduling rant](https://nadako.github.io/rants/posts/2013-03-26_roguelike-turn-based-time-scheduling.html).
  **Promote when a second cart needs speed-ordered turns** — the obvious one is
  giving `lurk`'s monsters real speed stats (the AI carts `needs`/`pasture`/`lurk`
  currently fake timing with fixed per-actor frame counters; this scheduler is
  the honest layer under them). Until then it stays inline in `tempo`.

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

### Keyboard shortcuts make a cart *filmable* (and multi-resolution-portable)

Most carts read the keyboard already — but be aware of a payoff that pulls the other way from
touch: **keyboard shortcuts are what let a cart demo itself and capture truthfully at every screen
ratio.** A key press (`key()`/`btn()`) has **no coordinates**, so one recorded take replays
identically on iPad, iPhone-portrait, a 9:16 TikTok crop — the layout can reflow completely and the
take still lands (and the audio comes out byte-identical). A **tap is an absolute pixel**: reflow
moves the widget and the tap misses. So the moment a cart becomes a **multi-resolution** thing you
want **autoplay video captures** of (App Store previews, a 9:16 social clip), a handful of extra
keyboard shortcuts — transport (play/stop), pattern/preset cycling, whatever you'd *show off* — are
**gold**: they turn "record once → render every ratio" from impossible into free. Bind them up front
even if touch is the primary input; a mouse-only cart needs a re-record per shape (or a
letterbox/composite fallback). Full reasoning:
[`../design/export-ratios.md`](../design/export-ratios.md) (the render side) +
[`../design/resolution-portable-input.md`](../design/resolution-portable-input.md) (the input side).

## House conventions for instrument carts

> **Building a sound/instrument cart? Start at the shelf:**
> [`instrument-carts.md`](instrument-carts.md). It indexes every existing synth,
> classic-machine homage, held-note instrument, engine showcase, radio station and
> sound toy, grouped by the **building block each one copies** (`radio.h` /
> [held-notes](../design/held-notes.md) / `ui.h` / `INSTR_*`). Find the cart closest to what you're making and
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
| `editor/public/carts/index.json` | **generated** metadata list (from each cart's `de:meta` block via `build-cart-index.js`) — never hand-edited |
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

## Adding a cart to the editor

Each cart owns its metadata in a **`de:meta` block** at the top of its `.c`; the global
`editor/public/carts/index.json` is **generated** from those blocks (`build-cart-index.js`), so you
never hand-edit it — and parallel agents stop colliding on it. Full design + schema:
[`../design/cart-metadata.md`](../design/cart-metadata.md).

1. Open `tools/carts/XX-name.c` with a `de:meta` block (then the human docblock):

```c
/* de:meta
{
  "title": "X. short title",
  "slug": "XX-name",
  "created": "2026-06-29",
  "kind": ["tutorial"],
  "teaches": [],
  "description": "One sentence. What the student learns."
}
de:meta */
#include "studio.h"
```

> `slug` is the cart's canonical `<name>` — it **must equal the `.c` filename
> stem** (here `XX-name`). It's the anchor a `.cart.png` uses to find its own
> source (`tools/carts/<slug>.c`) when handed over in isolation; `lint-carts.js`
> enforces the match. See [`../design/cart-metadata.md`](../design/cart-metadata.md)
> and [`../design/editor-cart-workflow.md`](../design/editor-cart-workflow.md) Gap 1b.

2. *(Optional)* `tools/carts/XX-name.cart.js` for sprites/map.
3. Run the two-step build above — **the build auto-registers the cart** by regenerating `index.json`
   from your `de:meta` (no manual edit). A cart with no `de:meta` won't appear, and the build warns
   when that happens.
4. Validate: `node tools/lint-carts.js` — checks your `de:meta` and that `index.json` is in sync.

Required fields: `title`, `created` (today's `YYYY-MM-DD`), a non-empty `kind[]` (`tutorial`,
`game`, `tech-demo`, `instrument`, `toy`, `tool`, `probe`, `generative`), `teaches[]` (use `[]` if
nothing distinctive), and `description`. `genre` is required when `kind` includes `game`. Optional:
`status`, `lineage`, `homage`, and `collection[]` (the doc-anchored cross-cutting threads this cart
belongs to — `road`/`radio`/`tinyjam`/… from [`tools/collections-vocab.js`](../../tools/collections-vocab.js);
browse with `node tools/collections.js`). `description` may be a string or `{summary, detail,
controls}`. The vocabulary + rules live in `tools/lint-carts.js`.

The tutorials panel picks it up automatically on next reload — no code changes needed.
