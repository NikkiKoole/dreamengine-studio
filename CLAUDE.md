# dreamengine

A fantasy console for building **deep, honest simulations behind a humble lo-fi surface**, built by the maker and Claude together. Write C, hit run, get a native game window. Inspired by PICO-8, DIV Game Studio, and BlitzMax.

> **North star** ([VISION](docs/VISION.md) · [ADR-0022](docs/decisions/0022-collaboration-is-the-north-star.md)): one cart = one honest core. Learn-to-code is **lineage**, not a goal — but the beginner is kept as a *critic*: every cart must clear a **two-part bar — verifiable** (focused, oracle-judgeable) **AND legible-and-delightful to a stranger** (the soul; no oracle checks it). Don't let "passes the gates" become the whole bar.

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
- **API fn** → the four places in "Adding a new API function". **Cart** → a `de:meta` block in the
  cart's `.c` (`index.json` is *generated* from it — feeds the compendium); see
  [`docs/design/cart-metadata.md`](docs/design/cart-metadata.md).
- **Derived numbers in a doc** (a table/count from a tool that will drift as the repo grows) →
  prefer generating them; else drop a `de:driftable` marker so the drift is tracked, not silently
  rotting. See [`docs/design/driftable-docs.md`](docs/design/driftable-docs.md).

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
2. **Generated registry files.** `editor/public/carts/index.json` is now **generated** from each
   cart's `de:meta` block (`tools/build-cart-index.js`) — you never hand-edit it, so a cart's
   metadata edit touches only its own `.c` (the old shared-index conflict is gone). It's regenerated +
   committed on bake; if it ever conflicts, don't hand-merge — rerun `node tools/build-cart-index.js`
   and commit. (For any OTHER hand-maintained shared file dirty with a foreign edit: stash your copy,
   `git checkout HEAD -- <file>`, splice in ONLY your entry, commit by pathspec, then restore.)

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
             ui.h        widgets (ui_button/ui_spr_button/ui_slider/ui_knob) — per-finger capture,
                         fat-finger hit pads, focus ring; ui_loupe() magnifier; ui_get_widgets() live rects
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
             canvas-diff.js  GPU-vs-software-canvas render oracle: A/Bs a cart in both modes + pixel-diffs;
                             guards the sw_force_gpu/DE_CPU_RASTER gotchas; --bytecheck (sha) / --raw / --max / --heatmap
             road-check.js   correctness oracle for coverage-based roads: framebuffer invariants (no naked
                             edges / strays / floating kerb) at ANY angle; --all = config-matrix gate; --overlay
             build-site.js / publish-cart.sh   build wasm carts + gallery → site/; publish + push
             publish-all.js  batch publish in ONE deploy — run it bare: it CHECKS cart-status, then
                             PROMPTS (smart=not-published+stale / +engine-stale / --all / build-all gate).
                             Resilient build, reuses publish-cart.sh; -y non-interactive, --dry-run preview
             mobile-lint.js  static report card: can a phone play this cart?
             aso-research.js App Store keyword research from FREE public data (iTunes Search API, no account):
                             per term a difficulty proxy + top competitors + your own rank (--app) + mined
                             keyword candidates; --json snapshots. The Search-Term-Rank popularity column waits
                             on Apple's beta. Design: docs/design/store-agents.md §ASO
             aso-lint.js     lint App Store metadata FIELDS (offline): title/subtitle/keyword char limits, wasted
                             comma-spaces + stopwords + multi-word entries, cross-field repeats (a word only ranks
                             once), and --research coverage. The deterministic half of the metadata composer (agent
                             owns the taste). Design: docs/design/store-agents.md §ASO
             aso-compose.js  PACK the 100-char keyword field deterministically: given title+subtitle + a priority-
                             ordered candidate pool, drop stopwords + words already visible, split multi-word entries
                             (Apple auto-combines), greedy-fill to the limit, and report what got CUT. The mechanical
                             core of the composer; agent writes title/subtitle. Design: docs/design/store-agents.md §ASO
             store-shots.js  native cart frame → App Store screenshots at EXACT device sizes (iphone69/ipad13/…).
                             Solves the aspect-ratio gap by COMPOSITING not stretching: crisp integer-upscaled cart
                             centered on a bg + Bungee caption in the breathing room (zero engine work). ffmpeg-based,
                             no node deps. Feed it a play.js --dump frame. Design: docs/design/store-agents.md §1
             wav-analyze.js / tune-check.js / dc-check.js / level-check.js / fx-check.js /
                             soak-check.js / web-audio-check.js   audio gates (see "Key things to know")
             wav-correlate.js / wav-envelope.js / wav-modrate.js / harmonic-spec.js   WAV A/B
                             analysis (sample correlation / amp+brightness envelope / LFO rate+depth /
                             harmonic series) — for A/B-ing a render against navkit
             filter-spec.js  measure a per-voice FILTER's actual response (slope dB/oct, resonance peak,
                             bass drain per res step) via a generated probe cart — acceptance evidence for
                             any sound.h filter change; born from the 303-fidelity spike (audio-notes §25)
             sprite-draw.js  reusable 2D pixel-canvas API for programmatic .cart.js sprites
             sprite-preview.js  render a .cart.js's sprites to one labelled PNG (no compile/run) — the tight loop for code-drawn sprites
             pixelsnap.js    clean up "AI pixel art": snap soft/off-grid pixels onto a real grid + posterize
                             to a small palette (median-cut / --palette pico32 / --two ink,paper), OKLab match,
                             FS or ordered/diagonal dither, --clean despeckle; never overwrites the input
             font-bake.js    bake real-TTF text into sprite-draw canvases at build time
             gen-rom-font.js bake the "extra" bitmap fonts (ROM dumps + EPX) into the shared atlas
             build-cart-index.js  GENERATE editor/public/carts/index.json from each cart's de:meta block
                             (cart owns its metadata; index.json is a derived view); --check gates staleness
             lint-carts.js   validate each cart's de:meta (tags/status/created/description) + assert
                             index.json in sync; owns the tag vocabulary
             spec.js         run each cart's spec() — the gameplay-logic gate (twin of tune-check)
             squishy-features.js  feature×brush COVERAGE oracle for the squishy cart (renders its matrix
                             grid, pixel-diffs each cell vs baseline → flags silently-no-op features)
             build-context.js  assemble a reading briefing for ONE cart: de:meta + todos + related carts +
                             every doc/note that MENTIONS it (with the line) — finds the differently-named home
             cart-outline.js a per-cart READING MAP (twin of build-context, for the SOURCE): data shapes
                             verbatim + global state + a FUNCTION INDEX (line · sig · role) + entry-point line
                             ranges — understand/navigate a 1–3k-line cart at ~1/7th the tokens of reading it.
                             --fn <name> dumps ONE function's body (no guessing the end line); --full adds macro values
             orient.js       go cold on a cart in ONE call: build-context + cart-outline back to back (the pair you
                             always want first). Flags pass through to the outline (--full / --fn <name>)
             topic-brief.js  the HORIZONTAL twin of build-context: a FEATURE/theme dossier across the whole repo
                             from a term-set — design docs (with STATUS + plan progress) + ADRs + ledger + the
                             engine seam + carts that demo/want it + related tools. `node tools/topic-brief.js "touch controls" gamepad`
             design-board.js ONE overview of every design doc + ADR and what phase it's in (ready/building/
                             exploring/shipped) — derived from each doc's STATUS line. Headlines the READY-TO-BUILD
                             backlog (specced work nobody's on). `--lint` = docs with no/off-vocabulary status
             doc-status.js   (lib, not CLI) shared STATUS-line parse + the lifecycle-phase vocabulary; owned here,
                             used by topic-brief.js + design-board.js
             api.js          one-shot studio.h API lookup — sig + doc for a fn/constant (exact, else substring →
                             discovery) without reading the ~500-entry studioDocs.js or an LSP round trip
             cart-info.js    orient on ONE cart: screen/GW×GH, embedded de:source DRIFT vs the .c, registration
             cart-status.js  what's out of date (rebake / publish / stale / compendium)
             cart-commit.js  the per-cart COMMIT helper: re-embed + lint + regen views + SCOPE-CHECK
                             (refuses to sweep a foreign cart out of index.json) + commit by a derived
                             pathspec. Dry-run by default; `-m "…" --commit` to fire. Bakes the two
                             commit hazards (missed file / foreign-registry sweep) in as guards
             cart-todos.js   the navigable view over every cart's de:meta.todo[] polish punch-list (--grep/--count/<name>)
             cart-analyze.js complexity + global-state report; ranks spec-worthiness
             cart-index.js   computed technique index ("what cart teaches X") + coverage
             cart-dupes.js   cross-cart duplication finder → refactor / drift candidates
             build-compendium.js  generate docs/cart-compendium.html (--check gates staleness)
             build-design-board.js  generate docs/design-board.html — the VISUAL ★ designs page (every design
                             doc + ADR by lifecycle phase, clickable cards) from design-board.js; --check gates staleness
             build-reflections.js  generate docs/reflections.html — the VISUAL ★ reflections page (the research
                             journal by lifecycle) from build-field-notes.js's collectNotes(); --check gates staleness
             build-band-briefs.js  generate docs/band-briefs.html — the VISUAL ★ briefs page: every blind band
                             brief (docs/design/*-blind-brief.md) paired with the radio cart it became; --check gates staleness
             build-field-notes.js  GENERATE docs/field-notes/FIELD-NOTES.md — navigable index of the research
                             journal (lifecycle board / timeline / related-note graph / conformance); --check gates staleness
             build-all.js    compile-check every cart vs current studio.h (catches API rot)
             build-nr.sh     build+run a cart with the DE_NO_RAYLIB software engine (no Raylib/frameworks) — the desktop twin of the iOS build (ios/)
             mac-app.sh      bundle an exported cart binary into a signed + notarized + stapled .app that opens on ANY Mac (Gatekeeper-clean); needs a Developer ID cert + a notarytool cred profile (header has the one-time setup)
             bundle-spike/   PASSED probe: TWO carts in ONE binary (per-TU -Ddraw=<slug>_draw renames + a dispatcher shim; carts unmodified) — the Tinyjam multi-cart app shape (design/share-panel.md §spike); proof-sound.sh = the de_switch_cart round-trip oracle
             build-app.js    build a MULTI-CART app binary from apps/<name>/app.json: per-TU renames + generated dispatcher + per-cart sound contexts (de_switch_cart) — adding a rack = one manifest line
             profile-fleet.js batch CPU-profile a set of carts → which engine primitive is hottest
             lint-docs.js    validate docs/ cross-references (links resolve, §-refs, tool-index)
             lint-xrefs.js   the inverse of lint-docs: find docs that SHOULD cross-link but don't —
                             unlinked doc-name mentions + missing backlinks (A→B but not B→A). Advisory;
                             scope to a feature (`node tools/lint-xrefs.js touch`) to act on it
             stale-doc-check.js  doc-freshness finder. BROKEN REFERENCES tier = real issues (a doc cites a
                             code path or `tool --flag` that doesn't exist now); mtime tiers = nudges (TOOL DRIFT:
                             a doc names a tool changed after it; DOC CHURN, --docs). `--driftable` = the CURATED
                             tier: docs that declare a `de:driftable` snapshot of a tool's output, flagged when the
                             tool's inputs moved after the snapshot (see docs/design/driftable-docs.md; surfaced by
                             cart-status.js). grep + git dates, no dep graph; advisory
             gen-tcc-symbols.js   regenerate runtime/studio_tcc_symbols.h from studio.h (libtcc)
             build-history.js     generate docs/history.html from git + the spine
             api-usage.js    bulk "how often is X used"; cross-checks studio.h ↔ docs ↔ shell.js
           det-probes/  standalone cross-compile DETERMINISM oracles + design-exploration probes for the
                    software canvas (run.sh = bit-identical arm64/x86/wasm gate; rotfill/rotline/rotspr/
                    stritex/… are the rotated-primitive studies that already settled the SW conventions).
                    README is the index. CHECK HERE before re-researching a software-rasterizer question.
           carts/   <name>.c (+ optional <name>.cart.js config). .cart.js exports
                    { sprites, map, charMap, mapW, mapH }; three sprite patterns — see
                    docs/guides/cart-authoring.md.
apps/      multi-cart APP manifests (apps/<name>/app.json — carts, bundle id, later store metadata
           per ADR-0026's next-to-manifest layout); tools/build-app.js turns one into a binary
data-tools/ per-cart DATA-acquisition tools (NOT general — that's tools/). Each subfolder feeds a
           few specific carts with external data; see data-tools/README.md. Growing collection:
             fmltools/   Floorplanner .fml → playable top-down level (fml2cart/fml-assets/fml-sprites,
                         floorplanner.js fetches by project id, make-floor.sh runs the pipeline) — feeds
                         floorwalker/seinelaan/floorplan. Has its own README + TODO + cache/ + samples/.
             roadview/   osm-roads.js — EXPERIMENTAL: fetch real OSM road geometry → "vector features"
                         JSON/.rvb the roadview cart loads at RUNTIME (--bbox/--place/--demo/--convert);
                         swap cities by swapping files, no cart regen (docs/design/external-data-carts.md).
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

Must land in **four** places in the same change (a **fifth** for draw commands), or it won't compile / autocomplete / show in help:

1. **Declare in `runtime/studio.h`** with a trailing `//` one-liner (plain, beginner-legible house style — the kept "would a stranger get it?" critic, per [ADR-0022](docs/decisions/0022-collaboration-is-the-north-star.md)).
2. **Implement in `runtime/studio.c`** with Raylib; respect `camera()`/`clip()` and the 0–31
   palette-index convention.
3. **Document in `editor/src/studioDocs.js`** — keyed by bare name, needs `sig` (matches studio.h
   exactly) + `doc` (`\n` line breaks, end with a one-line usage example). Constants get entries too
   (`sig` = the `#define`).
4. **List the key in `editor/src/shell.js`** — add to the right `sections` entry (controls help-tab
   grouping/order). Constants too.
5. **If it's a DRAW command, add a call to it in `tools/carts/drawall.c`** — the everything-cart that
   exercises every draw primitive (with per-frame rotation) so a software-canvas regression in any one
   shows up in a single `canvas-diff` run. New draw command → it must appear there, or it's untested.

API signatures churn during design — re-read the *current* `studio.h` declaration before updating
the other three. Changing an *existing* signature? `node tools/api-usage.js` first for its blast
radius (call counts + the studio.h↔docs↔shell.js cross-check), then `node tools/build-all.js` after
to confirm all carts still compile. **Then usually ship a cart that exercises it, with a screenshot**
(tutorial or example) — see "Tutorial carts".

## Sprite editor

Ported from `../eventually`. 16×16 sprites, 64 slots (8×8 grid → 128×128 sheet), pico32 palette.
Tools: pixel/fill/select/stamp/line/circle/rectangle. Animation frame strip (1/2/3/4/d keys). The
tilemap canvas IS the sprite sheet — click a tile to edit. Auto-exported as `build/sprites.png`.

**Sprites don't have to be hand-drawn here.** Need a sprite *in code* — UI icons/buttons, HUD
glyphs, procedural tiles, anything using the extended palette 16–31? Author it with
`tools/sprite-draw.js` from a `<cart>.cart.js` (don't hand-roll flat arrays). Worked examples:
`boom` (toolbar button icons), `masseffect` (units + tiles), `flank` (HUD/menu glyphs). Full API +
the three sprite patterns: [`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md). Iterate
with `node tools/sprite-preview.js <cart>` — renders the slots to one labelled PNG (no compile/run),
so you tweak the JS and re-look in seconds.

A cart's sprites have **two sources of truth that don't know about each other** — the in-editor pixel
canvas and a `.cart.js` generator — so hand-painting a generator cart's sprites in the editor ships
in *that* build but the next CLI bake silently reverts it. Rule: generator carts get sprite changes
in the generator, hand-drawn carts in the editor — never both. Why + the proposed fixes (options A–D,
incl. the patch/overlay round-trip): the **sprite story** —
[`docs/design/editor-cart-workflow.md`](docs/design/editor-cart-workflow.md) §Gap 2 / STATUS item 23.
A small icon glyph reads where a word can't: a cramped text HUD/panel (`HP 80  ammo 12|36`) becomes
**minimal and beautiful** by swapping labels for code-drawn glyphs (♥/clip/skull) — `flank` is the
worked example (HUD row, start-menu legend, mode-tile + per-slider icons).

## Fonts

- **Editor**: Comic Mono (TTF). **In-game**: `dos_8x8.png` (16×16 grid of 8×8 glyphs, loaded via
  `LoadFontFromImage`).
- More in-game fonts via `font(FONT_*)`: `FONT_SMALL` 4×6, `FONT_TINY` 3×5, `FONT_LARGE` (MDA 9×14),
  `FONT_BOOT` (VGA 9×16), `FONT_SMOOTH` (8×8 EPX-upscaled). **Adding a font:**
  [`docs/design/font-rendering.md`](docs/design/font-rendering.md) → "the bitmap-font pipeline".

## Tutorial carts

Carts show up in the tutorials panel, driven by `editor/public/carts/index.json` — which is
**generated** from each cart's `de:meta` block (`tools/build-cart-index.js`, see
[`docs/design/cart-metadata.md`](docs/design/cart-metadata.md)); never hand-edit it. Each `.cart.png`
embeds source/sprites/map/settings as zTXt chunks (`de:source` etc.); the visible image is the
thumbnail. The `de:settings` chunk restores the cart's intended screen/scale/cell/map dims.
Source-of-truth files live in `tools/carts/`.

**Sound/instrument or radio-station cart?** START at
[`docs/guides/radio-station-howto.md`](docs/guides/radio-station-howto.md) (radio) — and for ANY
music cart, **imagine the ideal band from the genre BEFORE reading any existing cart** (the firewall:
[`docs/guides/cart-authoring-prompt.md`](docs/guides/cart-authoring-prompt.md)). Only then open
[`docs/guides/instrument-carts.md`](docs/guides/instrument-carts.md) to pick the closest chassis.
"Does a voice/effect already exist?" → `node tools/topic-brief.js <term>` then `node tools/api.js
<term>` (its no-match points back) — never hand-grep the shelf; cart titles ≠ instrument names.
**Keep the recipe docs current when you ship** (a rule): radio → `radio-voices.md` +
`instrument-presets.md`; other music carts → `instrument-recipes.md`; engine effects →
`effects-recipes.md` (+ §17 ledger in `audio-notes.md` for new effect functions).

**Adding a cart** (full walkthrough: [`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md)):

1. Write `tools/carts/<name>.c`, opening with a `de:meta` block then a docblock (title + prose).
   The `de:meta` carries the metadata: `title`, `kind[]` (games need `genre`), `teaches` (from the
   `tools/teaches-vocab.js` vocab), `created` (today's `YYYY-MM-DD`), optional `lineage`/`homage`,
   and `description` (a string or `{summary,detail,controls}`). Schema:
   [`docs/design/cart-metadata.md`](docs/design/cart-metadata.md).
2. *(Optional)* `tools/carts/<name>.cart.js` for sprites/map.
3. `node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png` (build) —
   **this auto-registers the cart** by regenerating `index.json` from your `de:meta`. No manual edit.
4. `node tools/make-cart.js --run editor/public/carts/<name>.cart.png` (bake real screenshot).
5. `node tools/lint-carts.js` — validates your `de:meta` + that `index.json` is in sync.

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
> nonzero if pending). **To ship the cart, `node tools/cart-commit.js <name>`** runs the whole
> routine (re-embed → lint → regen views → scope-check → commit by derived pathspec) with the
> parallel-agent guards built in — dry-run by default, `-m "…" --commit` to fire. Got a good demo run? Park its input track at
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
profiler JSON has `workMsAvg/Max`, `calls[]`, `work[]`. Both work in any native build.
**Several carts alive (editor + play.js run, a bundle)? They ALL poll these files — add a
`pid:<n>` line or the first poller serves it (silently the WRONG app's frame).** Full recipe:
`docs/guides/debug-harness.md` → "Live inspection".

## Key things to know

- **The shell is `zsh`: an unquoted `$VAR` does NOT word-split** (a bash-ism). `cmd $LIST` passes the
  whole string as ONE argument (e.g. `profile-fleet.js $SET` → one bogus "cart"; a multi-file `clang
  $FLAGS` → "no such file"). Fix: inline the words, build an **array** `args=(a b c)` + `"${args[@]}"`,
  or force-split with `${=VAR}`. (A recurring agent trip-up — it bit this repo's profiling loops.)
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
- **`watch()`'s 2nd arg is a printf FORMAT STRING, not a value** — `watch(name, fmt, ...)`. Passing a
  bare value (`watch("mode", 4)`) makes `vsnprintf` treat the int as the `char*` format → SIGSEGV
  (fault at the int address, e.g. `0x4`). Always `watch("mode", "%d", 4)`. It's a rare, nasty crash:
  it only fires under `-DDE_TRACE` (the harness builds — `play.js`/`ui-audit`/`spec`, never the
  editor) AND only when that code path runs, so it hides until a specific state is reached (bit
  loderunner: `watch("mode", N)` crashed the moment the player moved).
- **Which check to run for a change → [`docs/guides/checks-and-oracles.md`](docs/guides/checks-and-oracles.md)**
  — the reverse index (task → gate) for render/perf/audio/cart-logic/docs. Check it before hand-rolling
  a verification; the matching deterministic oracle usually already exists (`canvas-diff`, `mirror-diff`,
  `road-check`, `build-all`, `spec`, the audio gates below, …).
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
