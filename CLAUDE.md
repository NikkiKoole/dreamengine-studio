# dreamengine

A fantasy console / learning programming environment. Write C, hit run, get a native game window. Inspired by PICO-8, DIV Game Studio, and BlitzMax.

## Git — NEVER branch; commit on the current branch

**Do not create or switch git branches. Ever — even when committing would normally warrant a branch.** Commit directly to the current branch (normally `master`): just `git add` + `git commit`. No `git checkout -b`, no `git switch -c`, no feature/PR branches.

Why: **multiple agents work in parallel on the same branch.** If any agent creates or switches branches, the others get confused and lose their place. Merge conflicts are rare in practice because work is naturally isolated per cart (each task touches separate files). This rule **overrides** the usual "branch before committing on the default branch" default.

Two parallel-agent commit hazards (both have bitten):

1. **The git index is shared.** Another agent may have files *staged* while you work; a bare
   `git commit` after your `git add` sweeps their staged WIP into your commit. Before every
   commit: `git diff --cached --name-only` and confirm the list is exactly your files
   (unstage strays with `git restore --staged <file>`).
2. **Shared registry files** (`editor/public/carts/index.json` is the big one) often carry
   another agent's uncommitted entries. Don't commit the whole file blindly — their entries
   may reference cart files that aren't committed yet (broken refs). If the file is dirty
   with foreign edits: stash your working copy, `git checkout HEAD -- <file>`, splice in
   ONLY your entry, commit, then restore the working copy.

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
│   └── studio.c        # Raylib implementation of the API + main()
├── editor/
│   ├── electron/
│   │   ├── main.cjs    # Electron main process — compiles + runs cartridges
│   │   └── preload.cjs # exposes window.studio.run() and window.studio.saveSprites()
│   ├── src/
│   │   ├── shell.js/css    # IDE chrome — tabs, run button, build log
│   │   ├── main.js         # CodeMirror editor setup + cmd-click dispatch (help / go-to-def / include preview)
│   │   ├── navigate.js     # read-only engine-source preview: cmd-click an #include "x.h"
│   │   │                   # filename → runtime file overlays the editor (✕/Esc closes);
│   │   │                   # clicks chain inside it, outline follows the previewed file
│   │   ├── outline.js      # function-list sidebar (cart by default; previewed file when open)
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
│   │                   # plus /runtime-src/<f>.h|c from runtime/ — powers the include preview
│   │                   # (route changes need a dev-server restart, unlike src/ edits)
│   └── index.html      # shell HTML — all panels live here
├── Makefile            # repo-root convenience: `make` kills stale processes + starts editor
│                       # targets: make / make start / make install / make help
├── tools/              # repo-root CLI tools (plain `node`, CommonJS)
│   ├── make-cart.js    #   build/bake .cart.png from tools/carts/<name>.c
│   │                   #   also a library module: play.js requires it for buildSpriteSheet/buildMap/etc.
│   ├── play.js         #   debug harness driver (record/replay/script + trace + --wav audio render)
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
- **live (libtcc)** — a persistent host (`studio.c` built with `-DDE_TCC`, linked against vendored `runtime/libtcc/`) JIT-compiles the cart in-process. The first run opens the window; after that, editing the code auto-rewrites `cart.c` (debounced) and the host's file-watch **hot-reloads** it without restarting — state in `de_state()` survives the swap. arm64-macOS only. Full design + rationale: [`docs/design/cart-as-script.md`](docs/design/cart-as-script.md).

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

The goal is not to make a game flashy. The goal is to make every player action feel
**noticeable and impactful**. The player presses a button — did it register? Did
something happen? Can they feel the difference between a hit and a miss, between
taking damage and not? Good feedback answers those questions instantly, in the ears
and on the screen, without the player having to read a number.

"Game juice" is the popular term for this, but it's a misleading one — it implies
adding effects for their own sake. The real question to ask is always: **what does the
player need to feel right now, and am I communicating that clearly?**

The canonical reference is **`tools/carts/juice.c`** (tutorial 19): the same ball with
all effects on a live toggle so you can feel exactly what each one adds. **`tools/carts/dinorun.c`**
shows squash/stretch + dust applied to a real game.

### Think in events, not effects

Before reaching for a technique, identify the event and ask what the player needs to
feel:

| Event | What to communicate | Typical feedback |
|---|---|---|
| Jump takeoff | "I left the ground" | stretch body upward + dust puff + short blip sound |
| Landing | "I hit the ground hard" | squash body flat + dust puff + small shake |
| Shooting / firing | "I fired" | tiny camera kick backward + muzzle flash pixel + sharp sound |
| Hitting an enemy | "My attack connected" | hit-stop 2–4 frames + enemy flashes white + impact sound |
| Enemy knockback | "It felt that" | enemy slides 4–8px away from hit, then settles |
| Taking damage | "That hurt me" | screen flashes red/white + shake + HP bar visibly drains |
| Player knockback | "I was pushed" | player briefly loses control + stumble animation |
| Near miss | "That was close" | whoosh sound only — no visual; silence would feel like nothing happened |
| Blocking / parry | "I deflected it" | sparks burst at contact point + clank sound + hit-stop |
| Collecting a coin / item | "I got something" | item pops outward then vanishes + ascending blip + score floats up |
| Powerup collected | "I got stronger" | brief full-screen tint to the powerup's color + ascending chord |
| Combo building | "I'm on a roll" | combo counter scales up with each hit + each hit sound rises in pitch |
| Combo broken | "I lost my streak" | counter shrinks and greys out + low thud |
| Enemy death | "I killed it" | particle burst at position + body briefly flashes + death sound |
| Big enemy / boss hit | "This one matters" | longer hit-stop (5–8 frames) + louder sound + screen flash |
| Level complete | "I succeeded" | fanfare chord + screen brightens + results animate in one by one |
| Death / game over | "It's over" | large shake + white flash + heavy low sound + slow reveal of score |
| Score milestone | "I hit a target" | score display inverts briefly + musical strum |
| UI button press | "My input registered" | button scales up 1–2px then snaps back + short tick sound |
| Spawning / respawning | "I'm back" | brief invincibility blink (pal() alternates) + spawn pop sound |

The rule: **every effect must be tied to a specific event.** Adding shake or flash
"because it looks cool" adds noise, not feedback. If removing an effect wouldn't make
an action feel less clear, remove it.

### Particularly satisfying effects — notes on why they work

**Enemy flashing white on hit** (`pal(CLR_DARK_GREY, CLR_WHITE)` for 2 frames then `pal_reset()`) —
the player knows the hit registered even before they see the HP drop. Works because white is the
brightest possible contrast against any background.

**Floating damage numbers** — a small number (+10, -3) that floats upward from the hit point and
fades out over ~40 frames. Satisfying because it converts an abstract HP change into a spatial
event the player can see exactly where it happened.

**Pitch-climbing hit sounds** — each hit in a combo plays one semitone higher (`midi + combo_count`).
Pure audio feedback that lets the player count their combo without looking at any counter.

**Camera kick on shooting** — offset the camera 2–3px opposite the shot direction for 3 frames
then lerp back. Makes the gun feel like it has mass and recoil. Extremely cheap.

**Anticipation pause before big attacks** — freeze the attacker for 4–8 frames (wind-up) before
their hit lands. Counterintuitively this makes the impact feel *heavier*, not slower, because
the player has a moment to brace for it.

**Slow motion on near death** — when HP drops to 1, briefly set a `slow` flag that only runs
`update()` every 2nd frame. The world slowing down communicates danger without words.

**Item pop-scale on pickup** — draw the collected item at 2× scale for 3 frames before it
disappears. The brief enlargement makes the pickup feel physically satisfying.

**Screen edge vignette on damage** — flash the screen border red (4 `line()` calls) for 6 frames
when taking damage. Common in 3D shooters; works just as well in 2D to sell the "I was hit"
feeling without covering the whole screen.

### Techniques and how to wire them

**Sound** — the most immediate feedback channel. The player hears the hit before they
see any visual effect. A satisfying `hit(midi, INSTR_NOISE, vol, dur_ms)` on impact
does more work than almost any visual trick.

**Screen shake** — `shake(amount)` decays automatically; call it on impacts and deaths.
`shake(4–6)` for death/explosions, `shake(1–2)` for a landing or minor hit.

**White flash** — skip the normal draw for 2–6 frames. The simplest version:
```c
static int flash;
// on death: flash = 6;   on hard hit: flash = 3;
// in draw(): if (flash > 0) { cls(CLR_WHITE); flash--; return; }
```

**Hit-stop** — freeze the whole simulation for 2–4 frames on heavy impacts. Nothing
communicates weight like time briefly stopping:
```c
static int hitstop;
// in update(), before any movement: if (hitstop > 0) { hitstop--; return; }
// on hit: hitstop = 3;
```

**Squash & stretch** — deform the character to show acceleration and impact. Jump
takeoff = stretch tall (body elongates); landing = squash flat (body widens). Use a
`float squash` that you set on the event and decay with `lerp` every frame:
```c
static float squash;   // >0 = squash (land), <0 = stretch (takeoff)

// on jump takeoff: squash = -0.85f;
// on landing:      squash =  1.0f;
squash = lerp(squash, 0.0f, 0.22f);   // in update(), every frame

// in your draw function, use squash to shift head/extend body:
int sp  = (int)(squash * 3.0f);    // -3..3 px
int ext = (sp < 0) ? -sp : 0;      // extra height when stretched
```

**Particles** — a small pool of structs (position, velocity, lifetime) spawned at the
event location. A puff at the feet on jump/land, a burst on enemy death, sparks on a
sword hit. Copy the pattern from `juice.c` or `dinorun.c` (~25 lines):
```c
typedef struct { float x, y, vx, vy; int life, col; } Dust;
static Dust dust[24];

static void spawn_dust(float px, float py) {
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < 24; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ px + rnd_between(-4, 5), py,
                    rnd_float_between(-1.5f, 1.5f), rnd_float_between(-1.8f, -0.1f),
                    rnd_between(8, 15), (k & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY };
                break;
            }
}
// tick each frame: x+=vx; y+=vy; vy+=0.12f; life--;
// draw each frame: if (life>0) pset((int)x, (int)y, col);
```

**Tint to white (soft flash)** — a `fillp` overlay fades the whole scene toward white without a hard cut. Draw it after your normal scene, before the HUD. Sequence the density over 3–4 frames for a smooth wash rather than a strobe:
```c
static int tint;   // frames remaining; set to e.g. 4 on hit

// in draw(), after scene but before HUD:
if (tint > 0) {
    // heavier pattern on the first frame, sparser as it fades
    int patterns[4] = { 0xFFFF, 0xA5A5, 0xAAAA, 0x8020 };
    int idx = 4 - tint;   // 0 = heaviest
    fillp(patterns[idx < 4 ? idx : 3], -1);  // -1 = transparent holes
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
    fillp_reset();
    tint--;
}
```
The `-1` hole color means "show the background through"; only the 0-bits of the pattern become white. For an enemy tinting rather than the whole screen, use `pal()` instead — remap the entity's colors toward white for 2–3 frames then `pal_reset()`.

**Motion trail** — keep a short ring-buffer of recent positions and draw faded copies behind the object. Effective for fast projectiles, dashing characters, teleports:
```c
#define TRAIL_LEN 6
static float trail_x[TRAIL_LEN], trail_y[TRAIL_LEN];
static int trail_head;

// in update(), after moving:
trail_x[trail_head % TRAIL_LEN] = px;
trail_y[trail_head % TRAIL_LEN] = py;
trail_head++;

// in draw(), before drawing the main sprite:
for (int i = 0; i < TRAIL_LEN; i++) {
    int idx = (trail_head - 1 - i + TRAIL_LEN * 2) % TRAIL_LEN;
    int alpha = TRAIL_LEN - i;   // older = more faded
    if (alpha > 1) pset((int)trail_x[idx], (int)trail_y[idx],
                        alpha > 3 ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
}
```

**Easing curves** — never tween with raw linear interpolation. `ease_out(t)` for things arriving (they rush in and settle), `ease_in(t)` for things leaving (they accelerate away). For UI popups that overshoot their target and spring back, combine `ease_out` with a brief negative overshoot: lerp past the target by ~10%, pause 2 frames, lerp back.

**Randomised sound pitch** — the same sound repeated every frame becomes grating fast. Vary pitch by ±2–3 semitones on each play: `hit(base_midi + rnd_between(-2, 3), instr, vol, dur)`. The variation is subtle enough not to feel random but prevents the ear fatigue of a perfectly identical loop.

**Coyote time** — in platformers, let the player jump for 6–8 frames after they walk off a ledge. This makes controls feel responsive rather than punishing. Keep a `coyote` counter that starts counting when the player leaves the ground without jumping; allow a jump while `coyote > 0`. Invisible to the player, but they'll notice immediately if it's missing.

**Stack subtle effects on one event** — a single big effect (giant shake) feels cheap. The same event with five small effects stacked together (3-frame freeze + small shake + white flash + particle burst + impact sound) feels chunky and satisfying. Each effect alone is barely noticeable; together they're unmissable.

### Detecting the landing frame

The `was_grounded` pattern captures the exact frame a character touches down:
```c
bool was_gr = grounded;          // snapshot BEFORE physics runs
// ... physics that may flip grounded to true ...
if (!was_gr && grounded) {       // just landed THIS frame
    squash = 1.0f;
    shake(1.2f);
    spawn_dust(px, ground_y);
}
```

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
- The palette in `studio.c` is the full 32-color PICO-8 palette (indices 0–31), matching the sprite editor's `pico32.json`. All 32 are named `CLR_*` in `studio.h`
- **Saves are per cart**: `save()`/`save_int()`/`save_bytes()` write `cart.sav`/`cart.kv`/`cart.blob` into `build/saves/<cart>/` — the editor and `play.js` pass `--save-dir saves/<cart>` at launch (no flag = files land in the cwd, the old shared behavior). So a fresh cart reading a non-zero `load_int()` value means a stale folder, not another cart's data
- **A key the cart reads is the cart's key.** `key()/keyp()/keyr()` *claim* the keycode — the runtime pause hotkey skips claimed keys, so a full-keyboard cart (sh101's two-manual piano) keeps P as a note while ENTER still opens the pause overlay. Don't design around the hotkeys; just read the keys you need. (Claims reset on libtcc hot-reload; pause key rebind = `-DPAUSE_KEY` from settings → controls.)
- Cart code shares one namespace with the whole `studio.h` API, so **don't name a variable after a built-in function**. `map` is the common trap — a tilemap/grid array called `map` clashes with the `map()` draw function (`error: redefinition of 'map' as different kind of symbol`); use `grid`/`dmap` instead. Same goes for `line`, `rect`, `circ`, `print`, `spr`, `timer`, `now`, etc.
- The starter cart also `#define`s `STATE` / `S` / `GameState` for the persistent-state sugar (`STATE { ... };` + `S->field`, over `de_state()` — see [`docs/design/cart-as-script.md`](docs/design/cart-as-script.md)). These are **cart-local** macros (deliberately *not* in `studio.h`, because ~45 existing carts use `S` as a variable). A cart that wants `S` for something else just removes those `#define`s.
- **Data-driven carts: name your indices.** A cart that stores knob/param values in arrays (modrack's `param[]` is the archetype) must address them via an enum (`m->param[VK_FENV]`), never raw numbers — inserting a knob mid-list once silently cross-wired three knobs *and* six presets. The enum makes a reorder fail at the compiler.
- **After touching `runtime/sound.h`** (queue sizes, request kinds, bulk APIs): run the self-test — `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"` — silence = PASS; any `[sound] WARNING` means requests were silently dropped (see [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "Sound tripwire"). Also note `sound.h` is only compiled inside `studio.c` — analyzers parsing it standalone show false "undeclared identifier" errors; ignore those, trust the cart build.
- **Navigating C code: use the LSP tool** (clangd works on this repo) — `documentSymbol` on `studio.h` lists the whole API with signatures in one call; goToDefinition/findReferences/call-hierarchy resolve semantically (it won't confuse a local `grid` with the `map()` builtin like grep does). Caveat: it can't follow references *into* `sound.h` (only compiled inside `studio.c`). For bulk "how often is X used across all carts" questions, run `node tools/api-usage.js` instead — it also cross-checks studio.h ↔ studioDocs.js ↔ shell.js coverage. Findings snapshot: [`docs/design/api-usage-audit.md`](docs/design/api-usage-audit.md).
