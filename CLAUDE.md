# dreamengine

A fantasy console / learning programming environment. Write C, hit run, get a native game window. Inspired by PICO-8, DIV Game Studio, and BlitzMax.

> **This file is rules + pointers.** Reference detail lives in `docs/` (start at `docs/README.md`) and in each tool's own file header. Keep this file lean — it loads into every conversation.

## Making new things discoverable

When you add a tool, design, decision, or feature, make it discoverable **by placing it in the
right home and linking from the index — NOT by expanding this file.** CLAUDE.md holds *rules and
one-line pointers only*; it loads into every conversation, so prose here is the most expensive place
to put anything.

**Project knowledge lives in the repo, NOT in Claude's per-machine memory.** This repo is worked
from several machines, and Claude's memory is per-machine — a fact saved there is invisible
everywhere else. Persist anything durable (how a system works, a gotcha, a decision, a preference)
to the repo via the homes below; treat memory as a scratchpad for the live session only.

- **Tool** (`tools/x.js`) → full contract in the file's header comment + ONE line in the tools list
  below (the header is the source of truth; tools aren't documented in `docs/`).
- **Design / idea** → `docs/design/<topic>.md`, linked from `docs/README.md`'s layout tree.
- **Decision** ("why we did/didn't X") → an ADR in `docs/decisions/`.
- **Shipped / open / cut** → the `docs/STATUS.md` ledger.
- **How-to / workflow** → `docs/guides/*.md`.
- **API fn** → the four places in "Adding a new API function". **Cart** → `index.json` tags (feeds
  the compendium).

If you're tempted to add more than ~2 lines to CLAUDE.md, it belongs in a doc — link it instead.

## Git — NEVER branch; commit on the current branch

**Do not create or switch git branches. Ever — even when committing would normally warrant a branch.** Commit directly to the current branch (normally `master`). **Always commit by explicit pathspec** — `git add <your files>` then `git commit -m "…" -- <the same files>` — never a bare `git commit` (see hazard 1 below). No `git checkout -b`, no `git switch -c`, no feature/PR branches.

Why: **multiple agents work in parallel on the same branch.** If any agent creates or switches branches, the others get confused and lose their place. Merge conflicts are rare because work is naturally isolated per cart. This rule **overrides** the usual "branch before committing on the default branch" default.

Two parallel-agent commit hazards (both have bitten):

1. **The git index is shared.** Another agent may have files *staged* while you work; a bare
   `git commit` after your `git add` sweeps their staged WIP into your commit. **Fix: commit
   by explicit pathspec.** `git add <your files>` then `git commit -m "…" -- <the same files>`
   commits ONLY those paths and leaves any foreign staged files untouched. Rules that make it reliable:
   - **List EVERY file your change touches** (`.c` + `index.json` + `.cart.png` + docs). Pathspec
     commits only what you name — miss one and you ship a broken partial commit.
   - **`git add` first, always** — pathspec can't commit a new *untracked* file on its own.
   - **Exact filenames, never a directory/glob** (`git commit -- editor/` re-sweeps foreign
     changes). And `-m` goes *before* `--`.
   - Backstop: `git diff --cached --name-only` before committing if unsure.
2. **Shared registry files** (`editor/public/carts/index.json` is the big one) often carry
   another agent's uncommitted entries. Pathspec does NOT save you here — it commits the current
   working-tree version, foreign edits and all, which may reference uncommitted cart files (broken
   refs). If the file is dirty with foreign edits: stash your copy, `git checkout HEAD -- <file>`,
   splice in ONLY your entry, commit by pathspec, then restore.

**Destructive-restore guard — a restore you don't notice wipes work you can't see.** `git checkout
-- <file>` / `git restore <file>` silently throw away *all* uncommitted changes to that path — with
no undo. The trap that bit: running it to "clean up" a throwaway edit (a test probe appended to a
file whose real changes were never committed) — and the rewrite went with it. Before any
restore/stash/`reset`, ask: **is my work on this path committed?** If not, commit it first (by
pathspec) or operate on a copy.

The shared working tree makes this WORSE, not better (same reason as the commit hazards above): the
tree is shared across agents, so a restore's blast radius is *every* agent's uncommitted work on the
touched paths, not just yours — and the sibling agent gets no warning, no conflict, nothing. So:
**never run a tree-wide destructive form** — `git checkout .`, `git restore .`, `git reset --hard`,
or a bare `git stash` (stashes the whole tree, foreign WIP and all). Scope every restore to YOUR one
exact named path, never a directory/glob/`.`. (Cart backstop if you do lose one: the `.cart.png`
still holds your last *baked* source in `de:source` — `node tools/cart-info.js <name> --source >
tools/carts/<name>.c`. Don't rely on it; commit.)

### Hot shared source files: `runtime/sound.h`, `runtime/studio.h`

These two giant single files are edited by several agents at once in the one shared working tree.
Same hazard as #2 but worse — a revert here is **silent and semantic** (it still compiles, just
reverts a tuning fix or clobbers a half-finished refactor). Git sees "different content," not a
conflict. Rules that prevent and catch it:

- **NEVER full-file `Write` these two — targeted `Edit`s only.** A `Write` from stale in-context
  content is the #1 clobber.
- **Re-`Read` the exact region immediately before editing** (line numbers drift), and **right
  after committing confirm your change survived:**
  `git show HEAD:runtime/sound.h | grep '<a sentinel from your change>'` — if it's gone, a parallel
  commit reverted you; re-apply and re-commit.
- **Compile-gate before committing:** `node tools/play.js soundcheck script /dev/null --headless
  --frames 2` (must print `compiling… ok`, no `[sound] WARNING`). For **pitched-engine** edits also
  `node tools/tune-check.js --quiet`. Enable the pre-commit hook once per clone:
  `git config core.hooksPath .githooks`.

## Running the editor

```bash
make               # kills stale Electron/Vite, then starts fresh (easiest)
# manually: cd editor && nvm use 22 && npm start   # Vite + Electron together
```

The Electron window opens once Vite is ready at `localhost:5173`. The browser tab edits too, but
the **▶ run** button only works inside Electron (it spawns the compiler).

## Project structure

```
runtime/   studio.h (public API: constants + declarations), studio.c (Raylib impl + main()),
           + library headers — cart-land capabilities the engine deliberately doesn't own
           (ADR-0006). CHECK HERE before hand-rolling input/UI plumbing in a cart:
             ui.h        widgets (ui_button/ui_slider/ui_knob) — per-finger capture, fat-finger
                         hit pads, focus ring; ui_loupe() magnifier; ui_get_widgets() live rects
             gestures.h  per-finger swipes judged at lift + pinch_scale (whole-view zoom)
             pointer.h   multi-finger pointer pool (PTR_CLEAR/ACQUIRE/FIND) — layer below ui.h,
                         for bespoke per-finger targets, not widgets
             keybed.h    polyphonic chromatic keybed (touch+mouse+QWERTY+MIDI) — every keybed
                         cart copies it (epiano/moog/touchpiano/mellotron); NOT ribbons/radio strip
             solo.h      scale-locked solo strip the player drives over a radio (pairs radio.h)
             radio.h     radio-station chrome (chassis, seeded songs, draggable control knobs)
             improv.h    melodic improvisation for the radio stations (auto-solo)
             cards.h     shared playing-card rendering (blackjack/poker/solitaire/strippoker)
           Full table + contract: docs/guides/cart-authoring.md → "Cart-land library headers".
           Sound/instrument cart? docs/guides/instrument-carts.md indexes the shelf by block copied.
editor/    electron/ (main.cjs compiles+runs carts; preload.cjs exposes window.studio.*),
           src/ (shell.js IDE chrome, main.js CodeMirror+cmd-click, navigate.js engine-source
           viewer, outline.js, sprite-editor.js, map-editor.js, studioDocs.js = single source for
           API docs, settings.js, theme.js), public/ (fonts, dos_8x8.png, palettes/pico32.json),
           vite.config.js (serveDocs plugin → Docs tab + engine-source viewer), index.html
Makefile   `make` kills stale processes + starts editor (targets: make / start / install / help)
tools/     repo-root CLI tools (plain `node`, CommonJS). One line each — read the file header for
           the full contract:
             make-cart.js    build/bake .cart.png from tools/carts/<name>.c; also a lib for play.js
             play.js         debug harness driver (record/replay/script + trace + --wav)
             make-gif.js     capture an animated clip of a cart (webm/webp/gif/mp4/apng + audio)
             compose-clips.js stitch baked clips into one reel (ffmpeg xfade) from a .reel manifest
             ui-audit.js     UI bug finder (off-screen text, overlaps, dead widgets, hidden panels)
             mirror-diff.js  golden-pixel-diff harness: assert a render's symmetry invariant headless
             build-site.js / publish-cart.sh   build wasm carts + gallery → site/; publish + push
             mobile-lint.js  static report card: can a phone play this cart?
             wav-analyze.js / tune-check.js / dc-check.js / level-check.js / fx-check.js /
                             soak-check.js / web-audio-check.js   audio gates (see "Key things to know")
             wav-correlate.js / wav-envelope.js / wav-modrate.js / harmonic-spec.js   WAV A/B
                             analysis (sample correlation / amp+brightness envelope / LFO rate+depth /
                             harmonic series) — for A/B-ing a render against navkit
             sprite-draw.js  reusable 2D pixel-canvas API for programmatic .cart.js sprites
             sprite-preview.js  render a .cart.js's sprites to one labelled PNG (no compile/run) — the tight loop for code-drawn sprites
             font-bake.js    bake real-TTF text into sprite-draw canvases at build time
             gen-rom-font.js bake the "extra" bitmap fonts (ROM dumps + EPX) into the shared atlas
             lint-carts.js   validate index.json (tags + registration); owns the tag vocabulary
             spec.js         run each cart's spec() — the gameplay-logic gate (twin of tune-check)
             cart-info.js    orient on ONE cart: screen/GW×GH, embedded de:source DRIFT vs the .c, registration
             cart-status.js  what's out of date (rebake / publish / stale / compendium)
             cart-analyze.js complexity + global-state report; ranks spec-worthiness
             cart-index.js   computed technique index ("what cart teaches X") + coverage
             cart-dupes.js   cross-cart duplication finder → refactor / drift candidates
             build-compendium.js  generate docs/cart-compendium.html (--check gates staleness)
             build-all.js    compile-check every cart vs current studio.h (catches API rot)
             profile-fleet.js batch CPU-profile a set of carts → which engine primitive is hottest
             lint-docs.js    validate docs/ cross-references
             gen-tcc-symbols.js   regenerate runtime/studio_tcc_symbols.h from studio.h (libtcc)
             build-history.js     generate docs/history.html from git + the spine
             api-usage.js    bulk "how often is X used"; cross-checks studio.h ↔ docs ↔ shell.js
           carts/   <name>.c (+ optional <name>.cart.js config). .cart.js exports
                    { sprites, map, charMap, mapW, mapH }; three sprite patterns — see
                    docs/guides/cart-authoring.md.
build/     compile output (cart.c, binary, sprites.png, fonts, traces; .bake/ for live inspection)
docs/      all project docs — start at docs/README.md. VISION.md / STATUS.md, decisions/ (ADR-lite),
           design/ (api-notes, audio-notes), guides/ (cart-authoring, sharing, debug-harness).
```

## How ▶ run works

1. Export sprite sheet → `build/sprites.png`; copy `dos_8x8.png`; write code → `build/cart.c`
2. Compile: `clang cart.c studio.c -I runtime -I raylib/include libraylib.a -framework … -DSCREEN_W=X -DSCREEN_H=Y -DSCALE=Z -o build/cart`
3. Spawn `build/cart` with `cwd = build/` (so it finds sprites.png + font)

Raylib is Homebrew; `main.cjs` auto-detects the path (Apple Silicon vs Intel).

**Run backends** (settings → "run mode"): **native (clang)** — the flow above, default, optimized,
+ a background Windows cross-build. **live (libtcc)** — a persistent host JIT-compiles the cart
in-process and **hot-reloads** on edit; state in `de_state()` survives the swap (macOS arm64+x64).
Design: [`docs/design/cart-as-script.md`](docs/design/cart-as-script.md). The web build (emcc →
`cart.html/js/wasm`) is its own "Build for web" button.

## The runtime model

- `studio.c` owns `main()` — opens the Raylib window, runs the loop.
- User implements `draw()` (required) and `update()` (optional weak stub).
- Draw calls go into a `RenderTexture2D` at native resolution, then scale up to the window.
- Screen 320×200 default, 4× scale = 1280×800 (all configurable in settings).

## studio.h API

PICO-8-style **short** names — `pset`/`circ`/`circfill`, **NOT** `pixel`/`circle`/`circlefill`
(the `sprite-draw.js` *JS* lib uses long names; the C API does not — a recurring trip-up).
`cls` `spr` `print` `rect`/`rectfill` `circ`/`circfill` `line` `pset`; input `btn(player, button)`;
constants `SCREEN_W/H`, `BTN_*`, `CLR_*` (all 32 palette colors, 0–31). **Full, authoritative
signatures: `editor/src/studioDocs.js`** (also drives autocomplete/hover/help) or LSP
`documentSymbol` on `studio.h`.

### Adding a new API function (or constant)

Must land in **four** places in the same change, or it won't compile / autocomplete / show in help:

1. **Declare in `runtime/studio.h`** with a trailing `//` one-liner (beginner-readable house style).
2. **Implement in `runtime/studio.c`** with Raylib; respect `camera()`/`clip()` and the 0–31
   palette-index convention.
3. **Document in `editor/src/studioDocs.js`** — keyed by bare name, needs `sig` (matches studio.h
   exactly) + `doc` (`\n` line breaks, end with a one-line usage example). Constants get entries too
   (`sig` = the `#define`).
4. **List the key in `editor/src/shell.js`** — add to the right `sections` entry (controls help-tab
   grouping/order). Constants too.

API signatures churn during design — re-read the *current* `studio.h` declaration before updating
the other three. **Then usually ship a cart that exercises it, with a screenshot** (tutorial or
example) — see "Tutorial carts".

## Sprite editor

Ported from `../eventually`. 16×16 sprites, 64 slots (8×8 grid → 128×128 sheet), pico32 palette.
Tools: pixel/fill/select/stamp/line/circle/rectangle. Animation frame strip (1/2/3/4/d keys). The
tilemap canvas IS the sprite sheet — click a tile to edit. Auto-exported as `build/sprites.png`.

**Sprites don't have to be hand-drawn here.** Need a sprite *in code* — UI icons/buttons, HUD
glyphs, procedural tiles, anything using the extended palette 16–31? Author it with
`tools/sprite-draw.js` from a `<cart>.cart.js` (don't hand-roll flat arrays). Worked examples:
`boom` (toolbar button icons), `masseffect` (units + tiles). Full API + the three sprite patterns:
[`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md). Iterate with
`node tools/sprite-preview.js <cart>` — renders the slots to one labelled PNG (no compile/run), so
you tweak the JS and re-look in seconds.

## Fonts

- **Editor**: Comic Mono (TTF). **In-game**: `dos_8x8.png` (16×16 grid of 8×8 glyphs, loaded via
  `LoadFontFromImage`).
- More in-game fonts via `font(FONT_*)`: `FONT_SMALL` 4×6, `FONT_TINY` 3×5, `FONT_LARGE` (MDA 9×14),
  `FONT_BOOT` (VGA 9×16), `FONT_SMOOTH` (8×8 EPX-upscaled). **Adding a font:**
  [`docs/design/font-rendering.md`](docs/design/font-rendering.md) → "the bitmap-font pipeline".

## Tutorial carts

Carts show up in the tutorials panel, driven by `editor/public/carts/index.json`. Each `.cart.png`
embeds source/sprites/map/settings as zTXt chunks (`de:source` etc.); the visible image is the
thumbnail. The `de:settings` chunk restores the cart's intended screen/scale/cell/map dims.
Source-of-truth files live in `tools/carts/`.

**Sound/instrument or radio-station cart?** START at
[`docs/guides/radio-station-howto.md`](docs/guides/radio-station-howto.md) (radio) — and for ANY
music cart, **imagine the ideal band from the genre BEFORE reading any existing cart** (the firewall:
[`docs/guides/cart-authoring-prompt.md`](docs/guides/cart-authoring-prompt.md)). Only then open
[`docs/guides/instrument-carts.md`](docs/guides/instrument-carts.md) to pick the closest chassis.
**Keep the recipe docs current when you ship** (a rule): radio → `radio-voices.md` +
`instrument-presets.md`; other music carts → `instrument-recipes.md`; engine effects →
`effects-recipes.md` (+ §17 ledger in `audio-notes.md` for new effect functions).

**Adding a cart** (full walkthrough: [`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md)):

1. Write `tools/carts/<name>.c`, opening with a docblock (title + player-facing prose).
2. *(Optional)* `tools/carts/<name>.cart.js` for sprites/map.
3. `node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png` (build).
4. `node tools/make-cart.js --run editor/public/carts/<name>.cart.png` (bake real screenshot).
5. Register in `editor/public/carts/index.json` **with tags** (`kind[]` required; games need
   `genre`; `teaches` is a required controlled vocabulary in `tools/teaches-vocab.js`; `lineage`
   one-liner). Then `node tools/lint-carts.js`.

> **When the owner should eyeball or play-test a cart, BAKE IT FIRST** (step 3 re-embed + step 4
> `--run`) so they can just load it in the editor — that's the preferred way to hand something over
> for a look, over dumped frames or screenshots. (Use `play.js --dump` yourself for diagnosis; bake
> for the owner to check.)
> **`--run` updates only the thumbnail, NOT the embedded source.** Iterating on logic: always
> re-run step 3 (re-embeds the C source the editor actually loads) *then* step 4. Skip step 3 and
> the editor keeps loading old code from `de:source` — "the cart ignores my changes."
> **Verify a bake by reading the embedded thumbnail or `build/.bake/<name>/screenshot.png`, NEVER
> `build/screenshot.png`** (that one belongs to the running editor and drifts).
> **After editing carts:** `node tools/cart-status.js` reports what's out of date (`--quiet` exits
> nonzero if pending). Got a good demo run? Park its input track at
> `tools/clips/<cart>/NN-label.{script,beats,rec}` — a committed, deterministic seed that mints a
> webm/gif on demand (`make-gif.js … --recipe <NN-label>`); cheapest win in the repo.

## Game feel — feedback

Make every player action **noticeable** — feel a hit land, a jump take off, damage taken, without
reading a number. The one rule: **every effect is tied to a specific event**; if removing it wouldn't
make an action feel less clear, cut it. Event→feedback table, effects catalog, and copy-paste recipes:
**[`docs/guides/game-feel.md`](docs/guides/game-feel.md)**. Canonical carts: `juice.c`, `dinorun.c`.

## Debugging carts — the "play together" harness

When a bug is about **timing, input, or "why did nothing happen when I pressed the key"** — use the
harness instead of guessing. It makes a run deterministic and inspectable. Full how-to (read before
using): **[`docs/guides/debug-harness.md`](docs/guides/debug-harness.md)**.

```bash
node tools/play.js <name> run | record <out.rec> | replay <in.rec> | beats <in.beats> | script <in.script>
# options: --trace <f> --frames <n> --dump <dir> --headless --seed <n> --bpm <n> --wav <f>
```

`play.js` compiles `tools/carts/<name>.c` **with `-DDE_TRACE`** and runs the native runtime under
harness flags (all off in a normal build). A `--trace` file is JSONL, one line/frame: auto fields
(`f`, `t`, `beat`, `bpos`) + every `watch()` value. Wrap `watch()` in `#ifdef DE_TRACE`
(`tools/carts/smooch.c` is the worked example). Park any good input track at
`tools/clips/<cart>/NN-label.*` (see Tutorial carts).

**Live inspection** — while any cart runs you can pull a screenshot + state without stopping it. Do
this yourself with Bash; don't ask the user:

```bash
echo "$(pwd)/build/.bake/snap.png"  > build/.bake/screenshot_request
echo "$(pwd)/build/.bake/snap.json" > build/.bake/state_request   # also: profiler_request, wav_request
sleep 0.5                                                          # wait one frame
ls build/.bake/screenshot_request 2>&1                            # gone = captured
```

Then **Read** the PNG (Claude sees images). State JSON has `f`/`t`/`w` (all `watch()` values);
profiler JSON has `workMsAvg/Max`, `calls[]`, `work[]`. Both work in any native build. Full recipe:
`docs/guides/debug-harness.md` → "Live inspection".

## Key things to know

- `node_modules` requires Node 22 — `nvm use 22` before any npm commands.
- `main.cjs`/`preload.cjs` are CommonJS (`.cjs`) because `package.json` is `"type": "module"`;
  editing them needs an Electron restart (`npm start`); Vite hot-reloads everything else.
- `SCREEN_W`/`SCREEN_H`/`SCALE` are `-D` compile flags from the settings tab.
- Palette is the 32-color PICO-8 set (0–31), all named `CLR_*` in studio.h. **Greys are British —
  `CLR_DARK_GREY`/`CLR_LIGHT_GREY`/`CLR_MEDIUM_GREY`/`CLR_DARKER_GREY`, never `GRAY`** (a recurring
  compile error). The lavender colour is `CLR_INDIGO` (13) — there is no `CLR_LAVENDER`.
- **Saves are per cart**: `save()`/`save_int()`/`save_bytes()` write into `build/saves/<cart>/`
  (editor + play.js pass `--save-dir saves/<cart>`). A fresh cart reading non-zero `load_int()` = a
  stale folder, not another cart's data.
- **A key the cart reads is the cart's key.** `key()/keyp()/keyr()` claim the keycode; the pause
  hotkey skips claimed keys. Just read the keys you need (claims reset on libtcc hot-reload;
  `-DPAUSE_KEY` rebinds).
- **Don't name a variable after a built-in.** Cart code shares the `studio.h` namespace. `map` is the
  common trap (clashes with `map()`); use `grid`/`dmap`. Same for `line`/`rect`/`circ`/`print`/`spr`/
  `timer`/`now`. The starter cart also `#define`s `STATE`/`S`/`GameState` (cart-local persistent-state
  sugar over `de_state()`); a cart wanting `S` for else just removes those defines.
- **Data-driven carts: name your indices** via an enum (`m->param[VK_FENV]`), never raw numbers —
  inserting a knob mid-list once silently cross-wired knobs + presets.
- **Audio self-tests** (run the matching one after the matching edit; all deterministic, `--save`
  re-blesses an intended change; findings in [`docs/design/audio-notes.md`](docs/design/audio-notes.md)):
  - touched `runtime/sound.h` (queues/requests): `node tools/play.js soundcheck script /dev/null
    --headless --frames 900 | grep "\[sound\]"` — silence = PASS. (sound.h is only compiled inside
    studio.c; standalone analyzers show false "undeclared" errors — ignore.)
  - touched a **pitched** engine: `node tools/tune-check.js --quiet` (SINE = 0¢ control).
  - touched **levels/effects**: `level-check.js`, `fx-check.js`, and for feedback/voice-lifetime
    edits `soak-check.js`; engine-math/optimizer change → `web-audio-check.js` (wasm-vs-native parity).
- **Effects are SET-AND-HOLD — (re)configure only when a value CHANGES, never every frame.** Wiring
  a knob straight into `update()`/`draw()` so `crush()`/`tape()`/`eq()`/`chorus()`/`reverb()` fires
  60×/s rebuilds the bus DSP every frame → **stutter, not a crash** (silent). Re-apply only on change
  (copy `groovebox`'s `apply_fx()`); same for live `note_cutoff`/`note_reverb`/`note_vol`.
  Linted: `node tools/lint-fx-frame.js` (`--quiet` = CI; waive with `// fx-lint-ignore`).
  `filter()`/`varispeed()`/`note_*` are built to be ridden live and excluded.
- **Porting/tuning a sound engine? A/B against navkit** (`~/Projects/navkit/soundsystem`), the
  reference most `INSTR_*` engines port from. Read
  [`docs/guides/porting-from-navkit.md`](docs/guides/porting-from-navkit.md) first (port the
  oscillator VERBATIM; mind velocity×2, ratio-blend, amp-normalize, always-on filter, knob-scale).
  A/B workflow: [`docs/guides/debug-harness.md`](docs/guides/debug-harness.md) → "A/B against navkit".
- **Carts with game logic can carry a `spec()`** — the gameplay twin of the audio gates: deterministic,
  headless, live only under `-DDE_SPEC` (zero footprint otherwise; API in `runtime/spec.h`). Run
  `node tools/spec.js [cart]` (`--quiet` = CI); carts without one are skipped. Opt-in; write one on
  complex sims (`cart-analyze.js` ranks candidates). `streetlab` is the reference. Design:
  [`docs/design/spec-harness.md`](docs/design/spec-harness.md).
- **Navigating C: use the LSP tool** (clangd ≥ 14; `documentSymbol` on studio.h lists the whole API).
  It can't follow refs into `sound.h` (only compiled inside studio.c). Carts resolve includes via
  `tools/carts/compile_flags.txt`; engine headers via `runtime/.clangd`. Stale clangd 13 throws false
  "studio.h not found" / "undeclared CLR_*" — **fix the binary, not the repo**:
  `brew install llvm && ln -sf "$(brew --prefix llvm)/bin/clangd" "$(brew --prefix)/bin/clangd"`
  (verify `clangd --version` ≥ 15). Bulk usage questions: `node tools/api-usage.js`.
