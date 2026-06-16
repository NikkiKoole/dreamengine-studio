# dreamengine

A fantasy console / learning programming environment. Write C, hit run, get a native game window. Inspired by PICO-8, DIV Game Studio, and BlitzMax.

## Git — NEVER branch; commit on the current branch

**Do not create or switch git branches. Ever — even when committing would normally warrant a branch.** Commit directly to the current branch (normally `master`). **Always commit by explicit pathspec** — `git add <your files>` then `git commit -m "…" -- <the same files>` — never a bare `git commit` (see hazard 1 below). No `git checkout -b`, no `git switch -c`, no feature/PR branches.

Why: **multiple agents work in parallel on the same branch.** If any agent creates or switches branches, the others get confused and lose their place. Merge conflicts are rare in practice because work is naturally isolated per cart (each task touches separate files). This rule **overrides** the usual "branch before committing on the default branch" default.

Two parallel-agent commit hazards (both have bitten):

1. **The git index is shared.** Another agent may have files *staged* while you work; a bare
   `git commit` after your `git add` sweeps their staged WIP into your commit. **Fix: commit
   by explicit pathspec.** `git add <your files>` then `git commit -m "…" -- <the same files>`
   commits ONLY those paths and leaves any foreign staged files staged & untouched (no reset,
   no data loss — verified). Rules that make it reliable:
   - **List EVERY file your change touches** (`.c` + `index.json` + `.cart.png` + docs). Pathspec
     commits only what you name — miss one and you ship a broken partial commit. Completeness
     is on you.
   - **`git add` first, always** — pathspec can't commit a new *untracked* file on its own; it's
     silently omitted (the commit still succeeds). The `add` stages new files; the `-- <paths>`
     is the safety net that ignores everything else in the index.
   - **Exact filenames, never a directory/glob** (`git commit -- editor/` re-sweeps foreign
     changes under it). And `-m` goes *before* `--` (a flag after the path is parsed as a pathspec).
   - Backstop: still `git diff --cached --name-only` before committing if unsure; but the
     pathspec form means a stray staged file can't reach your commit even if you don't.
2. **Shared registry files** (`editor/public/carts/index.json` is the big one) often carry
   another agent's uncommitted entries. **Pathspec commit does NOT save you here** — it commits
   the current working-tree version of the shared file, foreign edits and all. Don't commit the
   whole file blindly — their entries may reference cart files that aren't committed yet (broken
   refs). If the file is dirty with foreign edits: stash your working copy, `git checkout HEAD --
   <file>`, splice in ONLY your entry, commit (by pathspec), then restore the working copy.

### Hot shared source files: `runtime/sound.h`, `runtime/studio.h`

These two are giant single files that *every* audio/API change touches, so several agents
edit them at once in the one shared working tree. The same hazard as #2, but worse because
a revert here is **silent and semantic**, not just a broken ref. Two ways it bit this repo:
a **build-breaking** clobber (a half-finished refactor committed mid-flight), and a **silent
tuning revert** (an engine fix reverted to an older line by a stale full-file edit — it still
compiled, it was just out of tune again). Git sees "different content," not a conflict, so it
never warns. Rules that prevent and catch it:

- **NEVER full-file `Write` these two — targeted `Edit`s only.** A `Write` (or any
  regenerate-the-whole-file step) from stale in-context content is the #1 clobber. Edits that
  replace a small unique string can't revert a neighbour's lines.
- **Re-`Read` the exact region immediately before editing** (line numbers drift as others
  edit), and **right after you commit, confirm your change survived:**
  `git show HEAD:runtime/sound.h | grep '<a sentinel from your change>'` — if it's gone, a
  parallel commit reverted you; re-apply and re-commit. Keep these commits small and prompt.
- **Compile-gate before committing** (catches build-breaks): `node tools/play.js soundcheck
  script /dev/null --headless --frames 2` — must print `compiling… ok` and no `[sound] WARNING`.
  For **pitched-engine** edits also run `node tools/tune-check.js --quiet` (catches the silent
  tuning revert a compile can't see). A repo pre-commit hook automates the compile-gate on these
  files — enable it once per clone: `git config core.hooksPath .githooks`.

## Running the editor

```bash
make               # easiest — kills any stale Electron/Vite, then starts fresh
# or manually:
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
│   ├── studio.c        # Raylib implementation of the API + main()
│   └── *.h library headers — cart-land capabilities the engine deliberately
│       doesn't own (ADR-0006); all static, tunables #define'd before the
│       include. CHECK HERE before hand-rolling input/UI plumbing in a cart:
│       ├── ui.h        #   widgets: ui_button/ui_slider/ui_knob — per-finger
│       │               #   capture, fat-finger hit pads, opt-in focus ring
│       │               #   (mouse+touch+keyboard at once); carts with knobs/
│       │               #   sliders/buttons include this, never hand-roll the
│       │               #   drag machine (uikit + sfxgen are the references).
│       │               #   ui_loupe(1) = one-line MAGNIFIER for tiny knobs/
│       │               #   sliders on a phone (uiloupe; design/loupe-notes.md).
│       │               #   ui_get_widgets() exposes this frame's live widget rects
│       │               #   (used by tools/ui-audit.js to find overlapping/hidden UI)
│       ├── gestures.h  #   per-finger swipes judged at lift + pinch_scale
│       │               #   (the mobile whole-view ZOOM for swept grids/keybeds:
│       │               #   pinch_scale × camera_ex — modrack is the example)
│       ├── pointer.h   #   a multi-finger POINTER POOL: PTR_CLEAR/PTR_ACQUIRE/
│       │               #   PTR_FIND over a cart's own per-finger struct (first
│       │               #   field int id). The layer BELOW ui.h — for carts whose
│       │               #   fingers drive bespoke targets (grab a string to bend,
│       │               #   sweep bars/keys, pick open space), NOT widgets. Never
│       │               #   re-copy the Ptr/NPTR/NOID loop again (15 carts use it)
│       ├── improv.h    #   melodic improvisation for the radio stations (auto-solo)
│       ├── radio.h     #   radio-station chrome (chassis, seeded-song plumbing,
│       │               #   draggable rad_knob_int/_sel/_float control knobs)
│       ├── keybed.h    #   a polyphonic CHROMATIC keybed: touch+mouse+QWERTY+MIDI
│       │               #   → note_on/off into one slot, with chords/glissando/
│       │               #   velocity. The shared piano every keybed cart copies
│       │               #   (epiano/moog/touchpiano/mellotron) — NEVER hand-roll
│       │               #   the key tables + finger pool again. Default drawn
│       │               #   manual or geometry helpers for custom keys. A USB MIDI
│       │               #   keyboard plays it for free (engine midi_get(); macOS).
│       │               #   NOT for ribbons (martenot/heldnotes) or the radio
│       │               #   scale strip (solo.h) — design/midi-and-keybed.md
│       └── solo.h      #   the jam layer — a scale-locked solo strip the PLAYER
│                       #   drives over a radio (pairs with radio.h; built on ui.h
│                       #   capture). NOT for soloist-less stations (ambient/satie)
│       full table + contract: docs/guides/cart-authoring.md → "Cart-land
│       library headers". Building a SOUND/instrument cart? The shelf —
│       docs/guides/instrument-carts.md — indexes every existing one by which
│       of these blocks it copies; find the closest relative and start there.
├── editor/
│   ├── electron/
│   │   ├── main.cjs    # Electron main process — compiles + runs cartridges
│   │   └── preload.cjs # exposes window.studio.run() and window.studio.saveSprites()
│   ├── src/
│   │   ├── shell.js/css    # IDE chrome — tabs, run button, build log
│   │   ├── main.js         # CodeMirror editor setup + cmd-click dispatch (help / go-to-def / open engine source)
│   │   ├── navigate.js     # read-only engine-source viewer (lives in the DOCS tab):
│   │   │                   # the docs sidebar's "engine source" group + cmd-click an
│   │   │                   # #include "x.h" → switches to docs tab, opens the runtime
│   │   │                   # file there; clicks chain (includes → other headers,
│   │   │                   # documented symbols → API reference)
│   │   ├── outline.js      # function-list sidebar for the cart (code tab)
│   │   ├── sprite-editor.js/css  # pixel sprite editor
│   │   ├── map-editor.js   # tile map editor
│   │   ├── studioDocs.js   # single source of truth for API docs (autocomplete + hover + help tab)
│   │   ├── settings.js     # screen size + scale, persisted to localStorage
│   │   └── theme.js        # custom CodeMirror theme (unused — currently oneDark)
│   ├── public/
│   │   ├── ComicMono.ttf       # editor font
│   │   ├── dos_8x8.png         # in-game bitmap font (loaded via LoadFontFromImage)
│   │   ├── Ac437_Acer_VGA_8x8.ttf  # DOS OEM font (TTF backup)
│   │   └── palettes/pico32.json    # 32-color palette used by sprite editor
│   ├── vite.config.js  # Vite config + serveDocs() plugin: serves /docs-list.json
│   │                   # and /docs/<rel>.md live from docs/ — powers the Docs tab —
│   │                   # plus /runtime-src/<f>.h|c from runtime/ — powers the engine-source viewer
│   │                   # (route changes need a dev-server restart, unlike src/ edits)
│   └── index.html      # shell HTML — all panels live here
├── Makefile            # repo-root convenience: `make` kills stale processes + starts editor
│                       # targets: make / make start / make install / make help
├── tools/              # repo-root CLI tools (plain `node`, CommonJS)
│   ├── make-cart.js    #   build/bake .cart.png from tools/carts/<name>.c
│   │                   #   also a library module: play.js requires it for buildSpriteSheet/buildMap/etc.
│   ├── play.js         #   debug harness driver (record/replay/script + trace + --wav audio render)
│   ├── ui-audit.js     #   UI bug finder: text that runs off the screen edge, overlapping
│   │                   #   labels, widgets that draw but never respond to clicks (a missing
│   │                   #   ui_end() — from ui.h's DE_TRACE tripwire), and panels that only
│   │                   #   open on input. Runs the cart headless
│   │                   #   under studio.c's --uiaudit draw-box log; --explore presses each key
│   │                   #   the cart reads + taps each ui.h widget to reveal hidden panels;
│   │                   #   --overlay writes an SVG of the boxes on the screenshot. False
│   │                   #   positives are waived in the cart .c, linter-style:
│   │                   #   `// ui-audit-ignore off "TEXT" bottom` (see guides/debug-harness.md)
│   ├── build-site.js   #   build playable wasm carts + gallery into site/ for GitHub Pages
│   │                   #   (https://nikkikoole.github.io/dreamengine/); per-cart workdirs in
│   │                   #   build/.site/; gallery lists every cart with a built site/<name>/
│   ├── publish-cart.sh #   build-site.js + commit site/ + push, one command → live in ~1 min
│   │                   #   (.github/workflows/pages.yml publishes the committed site/, no CI emcc)
│   ├── mobile-lint.js  #   static report card: can a phone play this cart? verdicts
│   │                   #   touch-ready / tap-as-mouse / fixable (add touchControls:true) /
│   │                   #   keyboard-only; warnings: hover/wheel/right-click/tiny-target/touch>5
│   │                   #   run `--site` after publishing; see docs/design/mobile-web-notes.md
│   ├── wav-analyze.js  #   audio metrics from an engine WAV (peak/RMS/crest/clipping, two-file
│   │                   #   compare with bytes-identical check) — pairs with --wav and the
│   │                   #   .bake/wav_request live-capture trigger; see guides/debug-harness.md
│   ├── tune-check.js   #   is each engine IN TUNE? renders the tunecheck cart (sweep of every
│   │                   #   non-standard engine × 4 octaves), measures each note's pitch, reports
│   │                   #   cents-of-detune. SINE = control (reads 0¢). --json/--quiet(CI)/--keep;
│   │                   #   <wav> --note <midi> measures one file. findings: audio-notes.md §18
│   ├── sprite-draw.js  #   reusable 2D pixel-canvas API for programmatic .cart.js sprites
│   │                   #   exports: blank, pixel, rectfill, rrectfill, line,
│   │                   #            circlefill, ovalfill, trifill, polyfill, ngonfill,
│   │                   #            noise, shade, rotate, rotations, scale2x, replace,
│   │                   #            clone, outlined, mirror, stamp, flat, split,
│   │                   #            OUT, RAMP_DARKER, RAMP_LIGHTER
│   │                   #   require('../sprite-draw.js') from any .cart.js in tools/carts/
│   │                   #   showcases: foundry (watch each op draw), monstermix (stamp composition)
│   ├── font-bake.js    #   bake real-TTF text (any Google Font) into sprite-draw canvases
│   │                   #   at build time — titles/logos with zero runtime font code
│   │                   #   exports: bakeBanner (THE entry point: fit+center+outline+shadow
│   │                   #            into a slot-rect → ready tiles), bakeText, measure, loadFont
│   │                   #   carts drawing banners MUST colorkey(0) in init() or the empty
│   │                   #   slot-rect draws as an opaque black box
│   │                   #   fonts in tools/fonts/ (+ license); vendored opentype.js in tools/vendor/
│   │                   #   showcases: fontbake (treatments), highnoon (words-as-gameplay)
│   │                   #   see guides/cart-authoring.md → font-bake.js
│   ├── lint-carts.js   #   validate index.json: every cart tagged (kind[] from the
│   │                   #   vocabulary, genre required for games) + every .cart.png
│   │                   #   registered. Owns the tag vocabulary; run after adding carts
│   ├── cart-status.js  #   what's out of date: thumbnails whose EMBEDDED de:source !=
│   │                   #   tools/carts/<name>.c (need rebake), carts with no site/<name>/
│   │                   #   (need publish), + stale-published (source newer than the site
│   │                   #   build, git-time), + ENGINE-STALE (advisory: an instrument cart
│   │                   #   published BEFORE the latest runtime/sound.h commit → its web
│   │                   #   audio predates an engine/tuning change; doesn't gate --quiet).
│   │                   #   --quiet exits 1 if anything pending; --json
│   ├── build-all.js    #   COMPILE-CHECK every cart against the current studio.h
│   │                   #   (clang -fsyntax-only, -I runtime + -DDE_TRACE, parallel; ~5s).
│   │                   #   Catches cart-vs-API ROT nothing else does: a cart var named
│   │                   #   after a new API fn (the namespace trap), a changed signature,
│   │                   #   a stale watch() call. MANUAL/occasional (not a commit hook) —
│   │                   #   run after editing studio.h / adding API / before a publish.
│   │                   #   --quiet; exit 1 on any failure. (Found mouthharp/pixelperfect
│   │                   #   autopan + loderunner's stale watch — all latent, all invisible.)
│   ├── lint-docs.js    #   validate docs/ cross-references: relative .md links resolve
│   │                   #   + doc-qualified §-refs ("audio-notes §8.9") hit a real
│   │                   #   heading (resolving via a split-stub/parent = soft note, not
│   │                   #   error; bare §-refs deliberately unchecked). Run after any
│   │                   #   doc split / move / rename
│   ├── gen-tcc-symbols.js  # auto-generate runtime/studio_tcc_symbols.h from studio.h
│   │                   #   the editor's live-host build re-runs it automatically; after
│   │                   #   editing studio.h, also run it manually so the regenerated
│   │                   #   file lands in the same commit (libtcc live backend)
│   └── carts/          #   <name>.c (+ optional <name>.cart.js) — cart source of truth
│                       #   .cart.js exports { sprites, map, charMap, mapW, mapH }
│                       #   Three use patterns:
│                       #   1. Settings-only — just { screenW, screenH, scale } when the cart
│                       #      draws everything with primitives (bones, dpaint, pinball, wordle…)
│                       #   2. ASCII art sprites: { slotIndex: "16×16 string" } with charMap
│                       #      DEFAULT_CHAR_MAP covers palette 0–15: R=red W=white b=blue .=black
│                       #      custom charMap extends the defaults; only declare extra chars
│                       #      USE WHEN: sprite fits in palette indices 0–15
│                       #   3. Programmatic sprites: { slotIndex: flat_int_array } built with
│                       #      helper fns defined in the .cart.js itself — blank()/box()/dot() etc.
│                       #      USE WHEN: you need palette indices 16–31 (magic pal() recolor
│                       #      colors like body=28/dark=29 that ASCII charmap can't reach), or
│                       #      need geometric precision / auto-outlines.
│                       #      See advancewars.cart.js (box/dot/outlined), zoo.cart.js (tri/disc),
│                       #      skystrike.cart.js (mirror), doom.cart.js (32-wide dual-slot weapon)
├── build/              # compile output (cart.c, cart binary, sprites.png, fonts, traces)
└── docs/               # all project docs — start at docs/README.md (the map)
    ├── VISION.md       #   why & what; STATUS.md = shipped/open/cut ledger
    ├── decisions/      #   ADR-lite: frozen rationale ("why we (didn't) do X")
    ├── design/         #   api-notes.md, audio-notes.md (design exploration)
    └── guides/         #   cart-authoring.md, sharing.md, debug-harness.md (how-to)
```

## How ▶ run works

1. Editor exports sprite sheet from tilemap canvas → `build/sprites.png`
2. Copies `dos_8x8.png` font → `build/dos_8x8.png`
3. Writes editor code → `build/cart.c`
4. Compiles: `clang cart.c studio.c -I runtime -I raylib/include libraylib.a -framework ... -DSCREEN_W=X -DSCREEN_H=Y -DSCALE=Z -o build/cart`
5. Spawns `build/cart` with `cwd = build/` (so it finds sprites.png and the font)

Raylib is installed via Homebrew. `main.cjs` auto-detects the path: `/opt/homebrew/opt/raylib` on Apple Silicon, `/usr/local/opt/raylib` on Intel.

### Run backends (settings → "run mode")

▶ run has two backends, picked in the settings tab:

- **native (clang)** — the flow above. Default; optimized, full diagnostics, + a background Windows cross-build.
- **live (libtcc)** — a persistent host (`studio.c` built with `-DDE_TCC`, linked against vendored `runtime/libtcc/`) JIT-compiles the cart in-process. The first run opens the window; after that, editing the code auto-rewrites `cart.c` (debounced) and the host's file-watch **hot-reloads** it without restarting — state in `de_state()` survives the swap. macOS only (arm64 + x64; libtcc is vendored per-arch under `runtime/libtcc/<arch>/`). Full design + rationale: [`docs/design/cart-as-script.md`](docs/design/cart-as-script.md).

The web build (emcc → `cart.html/js/wasm`) is its own "Build for web" button, unchanged.

## The runtime model

- `studio.c` owns `main()` — opens the Raylib window, runs the game loop
- User implements `draw()` (required) and `update()` (optional, has a weak stub)
- Rendering: draw calls go into a `RenderTexture2D` at native resolution, then scaled up to the window
- Screen is 320×200 by default, 4× scale = 1280×800 window (all configurable in settings tab)

## studio.h API

```c
// input
bool btn(int player, int button);   // player 0=Arrows+Z/X, player 1=WASD+J/K (default; rebind in settings → controls)

// graphics — note the PICO-8-style short names: pset/circ/circfill,
// NOT pixel/circle/circlefill (the sprite-draw.js *JS* lib uses the long
// names; the C API does not — a recurring agent trip-up)
void cls(int color);
void spr(int index, int x, int y);
int  print(const char *text, int x, int y, int color);  // returns x after last char
void rect(int x, int y, int w, int h, int color);       // border
void rectfill(int x, int y, int w, int h, int color);   // filled
void circ(int x, int y, int radius, int color);         // border
void circfill(int x, int y, int radius, int color);     // filled
void line(int x1, int y1, int x2, int y2, int color);
void pset(int x, int y, int color);                     // single pixel

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
   **two** fields — `sig` and `doc`:
   ```js
   shake: { sig: 'void shake(float amount)',
            doc: 'Kick the screen by `amount` pixels; decays on its own. Call on impacts.\nshake(4);' },
   ```
   - `sig` must match the `studio.h` declaration exactly.
   - Use `\n` for line breaks; end the `doc` with a tiny one-line usage example.
   - **Constants get entries too** — `sig` is the `#define` line, e.g.
     `KEY_SPACE: { sig: '#define KEY_SPACE 32', doc: '…' }`.
4. **List the key in `editor/src/shell.js`** — add the name to the `keys` array of the
   right section in the `sections` array (this controls grouping + display order in
   the help tab). Add constants here too (e.g. the `KEY_*`, `FILL_*`, `CLR_*` keys). If
   it's a genuinely new category, add a new `{ title, keys }` section.

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
- 16×16 sprites, 64 sprite slots (8×8 grid → a 128×128 sheet), pico32 palette
- Tools: pixel, fill, select, stamp, line/circle/rectangle
- Frames: animation frame strip at the bottom, 1/2/3/4/d keys navigate
- The tilemap canvas IS the sprite sheet — click a tile to edit it
- Auto-exported as `build/sprites.png` on every run

## Fonts

- **Editor**: Comic Mono (TTF, loaded via `@font-face`)
- **In-game**: `dos_8x8.png` — a 145×145 bitmap sheet, 16×16 grid of 8×8 glyphs, yellow separator lines, loaded with Raylib's `LoadFontFromImage(image, YELLOW, 0)` — no TrueType rasterization, raw pixels
- **More in-game fonts** via `font(FONT_*)`: `FONT_SMALL` 4×6, `FONT_TINY` 3×5, `FONT_LARGE` (IBM MDA 9×14), `FONT_BOOT` (IBM VGA 9×16), `FONT_SMOOTH` (the 8×8 EPX-upscaled to 16×16). The MDA/VGA/smooth atlases are baked by `tools/gen-rom-font.js` (ROM dumps in `tools/fonts/` + an EPX upscale) into `editor/public/font*.png` + `runtime/font*_data.h`. **Adding a font: see [`docs/design/font-rendering.md`](docs/design/font-rendering.md) → "the bitmap-font pipeline"** (atlas format contract + the studio.c five-spot wiring).

## Tutorial carts

Carts (tutorials + example games) show up in the tutorials panel, driven by `editor/public/carts/index.json`. Each `.cart.png` in `editor/public/carts/` is a valid PNG with source/sprites/map embedded as zTXt chunks (`de:source`, `de:sprites`, `de:map`, `de:settings`). The visible PNG image is the thumbnail. The `de:settings` chunk carries the cart's intended screen/scale/cell/map dims (`{screenW,screenH,scale,cellW,cellH,mapW,mapH}`) so loading it restores that config instead of inheriting the editor's current globals — without it a cart can render wrong (e.g. its map tiles drawn at the wrong `CELL_W`).

Source-of-truth files live in `tools/carts/`; the build tool sits beside that folder:
- `tools/carts/<name>.c` — the cart's C source
- `tools/carts/<name>.cart.js` — *optional* config (sprites and/or tile map); only needed if the cart uses them
- `tools/make-cart.js` — the build tool (CommonJS; uses `require`)

**Adding a new cart:**

> Making a **sound/instrument cart** (synth, machine, station, engine showcase, sound
> toy)? **Building a self-playing RADIO STATION? There's one ordered runbook that
> sequences the whole arc (firewall → design blind → shop palette → chassis → brains →
> build → register): [`docs/guides/radio-station-howto.md`](docs/guides/radio-station-howto.md)
> — START THERE.** For any music cart: **imagine the ideal band from
> the genre BEFORE you read any existing cart** — write the lineup from the music itself,
> then shop the palette, *then* copy a cousin's chassis. Reading a cousin first anchors
> you to its voices and homogenizes the library (the why + the firewall:
> [`docs/guides/cart-authoring-prompt.md`](docs/guides/cart-authoring-prompt.md)
> §"Designing a sound cart's voices"). Only *after* the blind-band step, open
> [`docs/guides/instrument-carts.md`](docs/guides/instrument-carts.md) — it indexes the
> whole shelf by the building block each cart copies (`radio.h` / held-notes / `ui.h` /
> `INSTR_*`), so you start the *chassis* from the closest existing cart (its 🛑 banner
> restates this order). Add a row there when you ship.
>
> **Shipping a sound cart? Keep the recipe docs current — a rule, not a nicety** (they only
> stay useful if maintained):
> - **A radio station** → add its voice chart to [`docs/guides/radio-voices.md`](docs/guides/radio-voices.md)
>   (slot → role → preset), and in [`docs/guides/instrument-presets.md`](docs/guides/instrument-presets.md)
>   name any new patch recipes / add your cart to a reused preset's **used by** line (don't
>   mint a duplicate). Together they answer "what plays what, and where are we reusing a recipe?"
> - **Any other instrument/machine/whimsical cart that makes music** (a synth, a drum machine,
>   a plaything with grabbable presets) → name its recipes, by engine, in
>   [`docs/guides/instrument-recipes.md`](docs/guides/instrument-recipes.md) (the supply-side
>   palette). If it's a one-off toy with no fixed/reusable patch, note it under that doc's
>   "By cart" index as live/sequenced rather than skipping it.
> - **Adding OR meaningfully using an engine EFFECT** (echo/reverb/chorus/flanger/tape/wah/
>   drive[+`DRIVE_*` modes]/bitcrush/`instrument_tune` shimmer) → name the recipe in
>   [`docs/guides/effects-recipes.md`](docs/guides/effects-recipes.md) (the effects cookbook):
>   the exact call + values, one line of character, your cart under **used by** (add to an
>   existing row, don't duplicate). New effect FUNCTIONS also need the four-place API wiring
>   (studio.h / studio.c|sound.h / studioDocs.js / shell.js) + the §17 ledger note in
>   [`docs/design/audio-notes.md`](docs/design/audio-notes.md).

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
5. Register it — add an entry to `editor/public/carts/index.json`, **tags included**:
   ```json
   { "title": "...", "description": "... + controls", "file": "<name>.cart.png",
     "kind": ["game"], "genre": "arcade" }
   ```
   `kind[]` is required; games also need a `genre`; optional `homage` credits the
   original ("Space Invaders (1978)"). Then validate:
   ```bash
   node tools/lint-carts.js
   ```
   The vocabulary lives at the top of `tools/lint-carts.js` — extending it is fine,
   in the same commit as the first cart that uses the new value.

> **After editing carts, check what's out of date:** `node tools/cart-status.js` reports
> thumbnails whose embedded source drifted from `tools/carts/<name>.c` (NEEDS REBAKE — the
> editor loads the *embedded* source, so this is the "cart ignores my changes" trap), carts
> with no web build (NOT PUBLISHED), and carts whose source changed after their last
> `site/` build (STALE PUBLISHED). `--quiet` exits non-zero if anything's pending.

**Other `make-cart.js` commands:**
```bash
node tools/make-cart.js --update <cart.png> <screenshot.png>  # swap in a thumbnail manually
```

> **`--run` only updates the thumbnail, NOT the embedded source.** When iterating on cart
> logic, always re-run the full build first to re-embed the updated C source, then `--run`
> to bake the screenshot:
> ```bash
> node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png
> node tools/make-cart.js --run editor/public/carts/<name>.cart.png
> ```
> If you skip the first step, the editor loads the old code from the `de:source` chunk — the
> cart appears to ignore every code change you make.
>
> **To verify a bake, read the embedded thumbnail (or `build/.bake/<name>/screenshot.png`) —
> NEVER `build/screenshot.png`.** That file belongs to the running editor and shows whatever
> *it* last rendered, so it drifts under you and will show an unrelated cart. (`--run` bakes
> into the isolated `build/.bake/<name>/`, not the shared `build/`.) See
> [`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md).

Note: `make-cart.js` is run with plain `node` (it's CommonJS via `require`, not affected by `editor/package.json`'s `"type": "module"` since it lives in the repo root `tools/`, which has no `package.json`).

## Game feel — feedback

Make every player action **noticeable** — the player should feel a hit land, a jump take
off, damage taken, without reading a number. The one rule: **every effect is tied to a
specific event**; if removing it wouldn't make an action feel less clear, cut it.

Full how-to — the event→feedback table, the satisfying-effects catalog, and copy-paste
recipes (shake / hit-stop / squash-stretch / particles / tint / trails / coyote time) —
lives in **[`docs/guides/game-feel.md`](docs/guides/game-feel.md)**. Canonical carts:
`tools/carts/juice.c` (every effect on a live toggle) and `tools/carts/dinorun.c`.

## Debugging carts — the "play together" harness

When a cart's bug is about **timing, input, or "why did nothing happen when I
pressed the key"** — the kind you can't see by reading source — use the debug
harness instead of guessing. It makes a run **deterministic and inspectable**:
record/replay a play session, or script exact inputs, while a per-frame trace
shows what the engine did. Full how-to: **`docs/guides/debug-harness.md`** (read it
before using). The short version:

```bash
node tools/play.js <name> run                 # windowed live play
node tools/play.js <name> record <out.rec>    # play live, capture inputs
node tools/play.js <name> replay <in.rec>     # replay a recording deterministically
node tools/play.js <name> beats  <in.beats>   # author inputs in musical beats, run them
node tools/play.js <name> script <in.script>  # run a raw frame-script
# options: --trace <f> --frames <n> --dump <dir> --dump-every <n> --headless --seed <n> --bpm <n>
#          --wav <f>  render audio to WAV (byte-reproducible under --det) → analyze with tools/wav-analyze.js
```

`play.js` compiles `tools/carts/<name>.c` **with `-DDE_TRACE`** and runs the native
runtime (`runtime/studio.c`) under harness flags (`--det --record --replay --script
--trace --frames --dump`, all off in a normal build). A `--trace` file is JSONL,
one line per frame: auto fields (`f`, `t`, `beat`, `bpos`) plus every `watch()`
value the cart set that frame.

To make a cart legible to the trace, wrap `watch()` calls in `#ifdef DE_TRACE`
(they cost nothing in a normal build; `tools/carts/smooch.c` is the worked
example). A typical loop: author a `.beats` script for the exact moment, run it
`--headless` with `--trace`, then `grep` the trace to see the engine's decision.

**Live inspection** — while any cart runs (editor ▶ run or `play.js`) you can pull a
screenshot and state snapshot without stopping it. As an agent, do this yourself with
the Bash tool — don't ask the user to run these commands:

```bash
# 1. write trigger files with absolute paths
echo "/abs/path/build/.bake/screen.png"  > /abs/path/build/.bake/screenshot_request
echo "/abs/path/build/.bake/state.json"  > /abs/path/build/.bake/state_request
echo "/abs/path/build/.bake/perf.json"   > /abs/path/build/.bake/profiler_request
printf "/abs/path/cap.wav\n5\n"          > /abs/path/build/.bake/wav_request   # record next N seconds of audio (line 2, default 5)

# 2. wait one frame for the game to pick them up
sleep 0.5

# 3. verify the request files are gone (handshake: deleted = captured)
ls /abs/path/build/.bake/screenshot_request 2>&1   # should say "No such file"
```

Then use the **Read tool** on the PNG — Claude can see images directly. The state JSON
has `f` (frame), `t` (seconds), and `w` (all active `watch()` values). The profiler JSON
has `frames`, `workMsAvg`, `workMsMax`, `frameMsAvg`, `calls[]` (draw-call counts),
`work[]` (per-frame ms). Both profiler and state work in any native build — no special
flags needed. The game creates `build/.bake/` on startup so the directory always exists
once a cart has been launched.

Real example — asking the user which cart they're running, then capturing live:
```bash
echo "$(pwd)/build/.bake/snap.png"  > build/.bake/screenshot_request
echo "$(pwd)/build/.bake/snap.json" > build/.bake/state_request
sleep 0.5
# then: Read build/.bake/snap.png  →  you see exactly what's on screen right now
```

Before/after pairs: write state_request twice at different moments, diff the two JSON
files. Full recipe: `docs/guides/debug-harness.md` → "Live inspection".

## Key things to know

- `node_modules` requires Node 22 — use `nvm use 22` before any npm commands
- The Electron main process (`main.cjs`) and preload (`preload.cjs`) use CommonJS (`.cjs` extension) because `package.json` has `"type": "module"`
- Changing `main.cjs` or `preload.cjs` requires restarting Electron (`npm start`); Vite hot-reloads everything else
- The build log auto-hides after 3 seconds on success, stays open on compile errors
- `SCREEN_W`, `SCREEN_H`, and `SCALE` are passed as `-D` flags at compile time from the settings tab
- The palette in `studio.c` is the full 32-color PICO-8 palette (indices 0–31), matching the sprite editor's `pico32.json`. All 32 are named `CLR_*` in `studio.h`. **The greys are spelled the British way — `CLR_DARK_GREY` / `CLR_LIGHT_GREY` / `CLR_MEDIUM_GREY` / `CLR_DARKER_GREY`, never `GRAY`.** (`CLR_*_GRAY` is a recurring compile error — `use of undeclared identifier`.)
- **Saves are per cart**: `save()`/`save_int()`/`save_bytes()` write `cart.sav`/`cart.kv`/`cart.blob` into `build/saves/<cart>/` — the editor and `play.js` pass `--save-dir saves/<cart>` at launch (no flag = files land in the cwd, the old shared behavior). So a fresh cart reading a non-zero `load_int()` value means a stale folder, not another cart's data
- **A key the cart reads is the cart's key.** `key()/keyp()/keyr()` *claim* the keycode — the runtime pause hotkey skips claimed keys, so a full-keyboard cart (sh101's two-manual piano) keeps P as a note while ENTER still opens the pause overlay. Don't design around the hotkeys; just read the keys you need. (Claims reset on libtcc hot-reload; pause key rebind = `-DPAUSE_KEY` from settings → controls.)
- Cart code shares one namespace with the whole `studio.h` API, so **don't name a variable after a built-in function**. `map` is the common trap — a tilemap/grid array called `map` clashes with the `map()` draw function (`error: redefinition of 'map' as different kind of symbol`); use `grid`/`dmap` instead. Same goes for `line`, `rect`, `circ`, `print`, `spr`, `timer`, `now`, etc.
- The starter cart also `#define`s `STATE` / `S` / `GameState` for the persistent-state sugar (`STATE { ... };` + `S->field`, over `de_state()` — see [`docs/design/cart-as-script.md`](docs/design/cart-as-script.md)). These are **cart-local** macros (deliberately *not* in `studio.h`, because ~45 existing carts use `S` as a variable). A cart that wants `S` for something else just removes those `#define`s.
- **Data-driven carts: name your indices.** A cart that stores knob/param values in arrays (modrack's `param[]` is the archetype) must address them via an enum (`m->param[VK_FENV]`), never raw numbers — inserting a knob mid-list once silently cross-wired three knobs *and* six presets. The enum makes a reorder fail at the compiler.
- **After touching `runtime/sound.h`** (queue sizes, request kinds, bulk APIs): run the self-test — `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"` — silence = PASS; any `[sound] WARNING` means requests were silently dropped (see [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "Sound tripwire"). Also note `sound.h` is only compiled inside `studio.c` — analyzers parsing it standalone show false "undeclared identifier" errors; ignore those, trust the cart build.
- **After touching a pitched engine** (waveguide/Karplus/modal voice in `sound.h`, or anything affecting `freq`/delay-line length): run the **tuning** self-test — `node tools/tune-check.js --quiet` (exit 1 = something drifted out of tune) or just `node tools/tune-check.js` for the per-engine cents report. SINE is the control (reads 0¢); see [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "Tuning" and the first-engine audit in [`docs/design/audio-notes.md`](docs/design/audio-notes.md) §18 (PIPE is octave-low + flat; PLUCK/REED/BRASS flatten at the top — open).
- **After touching engine LEVELS or EFFECTS in `sound.h`** (gains, envelopes, amp-normalize, a shaper, any effect DSP): run the two stability gates — `node tools/level-check.js --quiet` (loudness regression: an engine that got louder/quieter or now slams the limiter — exit 1 on >4 dB drift vs `tools/level-baseline.json`) and `node tools/fx-check.js --quiet` (effect stability at extremes: NaN-collapse / DC runaway / self-oscillation — exit 1 on a regression vs `tools/fx-baseline.json`). Both are twins of tune-check, both deterministic, re-bless an INTENDED change with `--save`. See [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "Level" / "Effect stability" and the first-run findings + open items in [`docs/design/audio-notes.md`](docs/design/audio-notes.md) §20 (BOWED/BRASS run hot; the phaser/echo accumulate DC at max feedback — open).
- **Effects are SET-AND-HOLD — (re)configure them only when a value CHANGES, never every frame.** Wiring a knob straight into `update()`/`draw()` so `crush()`/`tape()`/`eq()`/`chorus()`/`reverb()`… fires 60×/s reconfigures the bus DSP (ring buffers, coefficients) every frame, churning the audio thread → **stutter/lag, not a crash** (silent, easy to misattribute). Re-apply only when the value moved (keep a last-applied copy and compare, or gate on the `ui_*` "changed" return); same for live `note_cutoff`/`note_reverb`/`note_vol` (push only while the target is actually moving). Copy `groovebox`'s `apply_fx()`. Full note: [`docs/guides/effects-recipes.md`](docs/guides/effects-recipes.md) (intro callout). **Linted: `node tools/lint-fx-frame.js`** flags an unconditional per-frame call to a buffer-rebuilding effect in `update()`/`draw()` (`--quiet` = CI gate; waive a confirmed-safe line with `// fx-lint-ignore`). `filter()`/`varispeed()`/`note_*` are excluded — they're built to be ridden live.
- **Porting/tuning a sound engine? A/B against navkit, the reference.** Most `INSTR_*` engines are ports of **navkit** (`~/Projects/navkit/soundsystem`), which ships headless sound tooling perfect for "does ours match the original": `tools/preset_audition.c` renders any preset to a WAV (build standalone — no raylib: `clang -O2 -o /tmp/nav_audition tools/preset_audition.c -lm`, then `/tmp/nav_audition <idx> -n <midi> -o out.wav`; preset indices live in `engines/instrument_presets.h`), plus `golden_wavs/` reference renders, `tools/wav_analyze.h`, and `song_render.c`. Render navkit's voice + ours (`play.js … --wav`) and compare spectra — ears flag "messier," a noise-floor/FFT diff pins down *why*. **Porting a whole instrument/effect over? Read [`docs/guides/porting-from-navkit.md`](docs/guides/porting-from-navkit.md) first** — port the oscillator VERBATIM (not piecemeal), and mind the gotchas (velocity×2, the ratio-blend, amp-normalize, always-on filter, knob-scale mismatch). A/B tooling workflow: [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "A/B against navkit".
- **Navigating C code: use the LSP tool** (clangd works on this repo) — `documentSymbol` on `studio.h` lists the whole API with signatures in one call; goToDefinition/findReferences/call-hierarchy resolve semantically (it won't confuse a local `grid` with the `map()` builtin like grep does). Caveat: it can't follow references *into* `sound.h` (only compiled inside `studio.c`).
  - **clangd must be ≥ 14 — fresh machine setup.** Carts resolve their includes via `tools/carts/compile_flags.txt` (relative `-I../../runtime`, portable — don't make it absolute); the **engine headers** under `runtime/` resolve via `runtime/.clangd` (parses each `.h` as C11, adds `-I.`, and prepends `studio.h` so the cart-land library headers — `ui.h`/`gestures.h`/`pointer.h`/`keybed.h`/`radio.h`/`solo.h`/`improv.h` — don't show a wall of false "undeclared `cls`/`CLR_*`/`ui_*`" errors from being parsed standalone as ObjC++). Stock `/usr/bin/clangd` tracks the Xcode Command Line Tools — current macOS ships **15** (resolves carts + engine headers cleanly); only an **older CLT install is stuck on 13**, which has a CWD bug that mis-resolves the relative `-I` → false "`studio.h` file not found" / "undeclared `CLR_*`/`SCREEN_W`" diagnostics on every cart. **The repo files are correct; the binary is the problem.** Fix per machine (keg-only brew llvm isn't on PATH by default): `brew install llvm && ln -sf "$(brew --prefix llvm)/bin/clangd" "$(brew --prefix)/bin/clangd"` — `/usr/local/bin` (or `/opt/homebrew/bin`) beats `/usr/bin` on PATH, so `clangd` then resolves to the modern build. Verify: `clangd --version` (want ≥15) and `clangd --check=tools/carts/<any>.c` (want "0 errors"). If you still see those false positives, you're on clangd 13 — fix the binary, don't touch the repo. (Aside: `clangd --check` on a bare engine `.h` still *prints* an "N errors" tally — those are `DefineOutline ==> FAIL` refactor-self-test misses on a header with no .cpp, NOT diagnostics; the editor shows none. Trust `clang -fsyntax-only`, which reads 0.) For bulk "how often is X used across all carts" questions, run `node tools/api-usage.js` instead — it also cross-checks studio.h ↔ studioDocs.js ↔ shell.js coverage. Findings snapshot: [`docs/design/api-usage-audit.md`](docs/design/api-usage-audit.md).
