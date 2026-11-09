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

**Repo split (2026-07): this engine now lives at `dreamengine-studio`.** The original `dreamengine`
GitHub repo was renamed `dreamengine-studio` (the full engine — `runtime/`/`editor/`/`tools/`); the
name `dreamengine` was reused for a NEW, separate public repo holding only the published gallery
(unrelated history). This clone's `origin` should point at `git@github.com:NikkiKoole/dreamengine-studio.git`.
If a pull fails with *"specifies to merge with 'refs/heads/master' … but no such ref was fetched"*,
your `origin` is still aimed at the old (gallery) URL — fix with
`git remote set-url origin git@github.com:NikkiKoole/dreamengine-studio.git` then `git fetch --prune`.

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
   - **A pathspec commit records those paths' WORKING-TREE content, not the staged state.**
     Same thing for normal edits — but an index-only change (`git rm --cached` untrack, partial
     `add -p`) is silently DISCARDED by it (bit us: the site/ untrack no-op'd). For index
     surgery: verify `git diff --cached --name-only` is exactly yours, then commit bare.
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
make               # kills THIS clone's stale Electron/Vite, then starts fresh (easiest)
# manually: cd editor && nvm use 22 && npm start   # Vite + Electron together
```

The Electron window opens once Vite is ready at `localhost:5273` (override with `DE_EDITOR_PORT=<n>`; 5173 is avoided on purpose — it collides with other dev servers). The browser tab edits too, but
the **▶ run** button only works inside Electron (it spawns the compiler).

## Project structure

One line per entry. The FULL contract lives in each file's own header comment; the longer notes
that used to sit here (gotchas, measurements, history) are kept verbatim in
[`docs/guides/tool-shelf-notes.md`](docs/guides/tool-shelf-notes.md). Add a new entry as ONE line.

```
runtime/   studio.h (public API), studio.c (Raylib impl + main()), sound.h (audio engine) + cart-land
           library headers (ADR-0006). CHECK HERE before hand-rolling input/UI plumbing in a cart.
           Full table + contract: docs/guides/cart-authoring.md → "Cart-land library headers".
           Sound/instrument cart? docs/guides/instrument-carts.md indexes the shelf by block copied.
           ── layout / input / UI ──
             lay.h       immediate-mode responsive layout (Box + split/cell/grid/lay_lane); design/device-adaptive-layout.md
             ui.h        widgets (button/slider/knob + _cell twins), per-finger capture, fat-finger pads, ui_loupe()
             face.h      device-face GRAMMAR over lay.h (BAND/LANE/HERO zones, enforced); responsive-first-device-face.md
             disclose.h  priority + finger-budget panel DISCLOSURE (expanded/folded/compact) for racks on a phone
             gestures.h  per-finger swipes judged at lift + pinch_scale
             pointer.h   multi-finger pointer pool (PTR_CLEAR/ACQUIRE/FIND), the layer below ui.h
             cursor.h    pixel mouse cursor drawn in-canvas (auto-hides OS arrow on real mouse); call LAST in draw()
             pro.h       in-app-purchase gate (pro_unlocked/_buy + shared Pro sheet). ⚠ FAILS OPEN; gate = tools/pro-check.js. ADR-0035
             fxicons.h   shared icon + colours per FX_* kind; reuse the glyph, never redraw a pedal
             shadermath.h GLSL-vocabulary CPU-shader kit (vec2/vec3, smoothstep/mix, IQ palette, SDFs); raymarch
             demath.h    DETERMINISTIC transcendentals (de_sinf/… bit-identical on arm64/x86/wasm); use when output is COMPARED. design/determinism.md
           ── music ──
             keybed.h    polyphonic chromatic keybed (touch+mouse+QWERTY+MIDI)
             mono.h      monosynth key assign (note priority + trigger mode) + mono_selfcheck
             patgen.h    latched pattern generator (orders, rest masks, octave chance) + selfcheck; latchbox
             tonnetz.h   chords as places (P/L/R/N/S/H moves, triangle map, automaton) + selfcheck
             solo.h      scale-locked solo strip over a radio (pairs radio.h)
             radio.h     radio-station chrome (chassis, seeded songs, knobs)
             improv.h    melodic auto-solo for the radio stations
             harmony.h   harmony brain: roman-numeral vocab + style Markov tables, pick/suggest/analyze; design/harmony-brain.md
             drumkit.h   playable drum PAD MAP (role vocab + dk_fire); kit build() owns the sound
             acid303.h   shared TB-303 voice (slide/accent/two-decay); acid_stock() = faithful voicing. tb303/acidrack/acidcandy
             tr808.h     faithful TR-808 voice bank (tr808_build/_fire/_fire_semi)
             tr909.h     faithful TR-909 voice bank (+ metal XY, flam/drag/ratchet strokes)
             morphdrum.h one parametric drum voice per role, CHARACTER knob morphs 808↔909 and beyond; morphbox
             sideman.h   Wurlitzer Side Man (1959) tube voice bank; the box/amp is the cart's (outboard.h). design/sideman.md
             tr77.h      Roland TR-77 voice bank tuned vs real samples; autorhythm
             drumpat.h   GENERATED: 565 modern 16-step patterns in 92 groups (dp_hit/dp_flam/dp_accent); groovebook
             rhythmbox.h GENERATED from design/rhythm-box-patterns.md: 101 sourced organ-era presets; read with rb_trigger. ⚠ per-rhythm per_beat/bars
             steinway.h  GENERATED: baked Steinway D samples for INSTR_PIANO hybrid; CC BY 4.0, show STEINWAY_CREDIT
             ampcab.h    guitar amp/cab voicing table (presets of existing effects); design/effects-bus-architecture.md §E
             outboard.h  master-bus analog chain (EQ→IRON→COMP→PLATE) as one table; outboard_apply owns insert order. design/analog-outboard-chain.md
             pmbreed.h   cart-land face of patch-match (pm_mutate/pm_apply_slot)
           ── games / world / physics ──
             cards.h     playing-card rendering
             endcard.h   shared WIN/LOSE end screen; never hand-roll fade+box
             worldnet.h  infinite deterministic world spine (terrain + lattice + wn_road_at); roadnet2/sloop
             roadkit.h   at-grade junction geometry (curb_return, RkField); streetlab = spec-locked source
             citygen.h   calibrated procedural city grammar (SNDi-matched to Rotterdam); citygrow/sloop
             physics.h   tiny Verlet toolkit (ropes/cloth/ragdolls); not a rigid-body engine
             boxrig.h    sprite region → Box2D polygon + texture; needs build-box2d.sh. design/box2d-integration.md
             json.h      EXPERIMENTAL runtime JSON reader (jsmn) for --data files; design/external-data-carts.md
           cart_ctx.h  per-instance header state (DE_CTX_STATICS default / DE_CTX_BLOCK under DE_CART_CTX); design/engine-context.md
           lockup/     NOT shelf: one cart's private modules (frozen model.h contract, ADR-0034). Don't #include elsewhere
           tenement/   NOT shelf: the tenement sim's frozen contract; design/tenement.md
           loficity/   NOT shelf: loficity's port of Lofi Cities' arranger (generated data)
           dreamatlas/ NOT shelf: GENERATED Mutable Dreams biomes (tools/gen-dreamatlas.js); design/mutable-dreams.md
           isoroom/    GENERATED (voxel-bake.js --emit-c) rotation table; design/iso-rooms.md
editor/    electron/ (main.cjs compiles+runs carts; preload.cjs), src/ (shell.js, main.js, navigate.js,
           outline.js, sprite/map editors, studioDocs.js = API docs source, settings.js, theme.js),
           public/ (fonts, palettes), vite.config.js. What the editor can DO → docs/guides/editor-features.md
Makefile   `make` kills stale processes + starts editor (targets: make / start / serve / install / help)
tools/     repo-root CLI tools (plain node, CommonJS). Header = contract.
           ── carts: build / run / ship ──
             make-cart.js    build/bake .cart.png from tools/carts/<name>.c; also a lib for play.js
             play.js         debug harness (record/replay/script, --trace/--wav/--solo-slot/--midi-note/--param)
             build-all.js    compile-check every cart vs current studio.h
             build-nr.sh     build+run a cart on the DE_NO_RAYLIB software engine
             build-box2d.sh  build vendored Box2D per platform (--check smoke test)
             build-cart-index.js  GENERATE editor/public/carts/index.json from de:meta (--check)
             lint-carts.js   validate de:meta + source hazards (watch() format, shadowing, finger pools); --selfcheck
             cart-commit.js  per-cart commit helper (re-embed, lint, regen, scope-check, pathspec commit)
             cart-status.js  what's out of date (rebake / publish / stale / compendium)
             cart-info.js    one cart: screen size, de:source drift vs .c, registration
             cart-todos.js   view over every cart's de:meta.todo[]
             cart-analyze.js complexity + global-state report, ranks spec-worthiness; --selfcheck
             cart-index.js   "what cart teaches X" + coverage
             cart-dupes.js   cross-cart duplication finder; --selfcheck
             backfill-slug.js re-embed helper for de:meta slug (backfill complete)
             collections-vocab.js  controlled vocab for de:meta collection[] (each slug → a doc)
             collections.js  roll-up view over collection[]
             spec.js         run each cart's spec() gameplay gate
             squishy-features.js  feature×brush coverage oracle for squishy; --selfcheck
             build-site.js / publish-cart.sh   build wasm carts + gallery → site/
             publish-all.js  batch publish in one deploy (checks, then prompts)
             mobile-lint.js  can a phone play this cart? report card; --selfcheck
             mac-app.sh      signed + notarized .app from an exported cart
             build-app.js    build a multi-cart APP from apps/<name>/app.json (--mac/--ios); --check = store-path gate
             bundle-spike/   PASSED probe: two carts in one binary
           ── clips / video / store / marketing ──
             make-gif.js     capture a clip (webm/webp/gif/mp4/apng + audio)
             dress-clip.js   dress a clip into a 9:16 Short with pixel-font text in the bars; design/export-ratios.md
             compose-clips.js  stitch clips into a reel from a .reel manifest
             build-app-reel.js  app trailer from app.json carts[]; design/demand-generation.md
             youtube-push.js bake + composite + upload a Short; design/video-distribution.md
             store-shots.js  cart frame → App Store screenshots at exact device sizes
             store-contact.js contact sheet from a --dump dir, to pick hero shots
             icon-mask.js    app-icon mask template + clipping check (measured from Apple's renderer); design/app-icon-mask.md
             press-kit.js    presskit()-style press page + zip; design/press-kit.md
             asc-push.js     push metadata/screenshots/IAPs to App Store Connect (ADR-0026); --dry-run/--check
             aso-research.js keyword research from free iTunes data (difficulty, competitors, rank)
             aso-suggest.js  demand-side keyword suggestions via Google Autocomplete
             aso-brief.js    generate apps/<name>/seo-brief.md (vocabulary, never prose)
             aso-coverage.js check press.md + listing vs seo-brief (fails only on stuffing)
             aso-score.js    score a listing + A/B a tweak (--deep = winnability)
             aso-lint.js     lint App Store metadata fields offline
             aso-compose.js  pack the 100-char keyword field deterministically
                             (all aso-*: docs/design/store-agents.md §ASO)
             leads.js        tribe/venue matcher + outreach tracker; design/demand-generation.md
             reddit-gaps.js  demand discovery from subreddit RSS (wishes/showcase/launches); design/demand-discovery.md
             reddit-gaps-drip.sh  launchd runner for reddit-gaps --drip
           ── render / UI oracles ──
             ui-audit.js     UI bug finder (off-screen text, overlaps, dead widgets); --selfcheck
             mirror-diff.js  golden-pixel symmetry diff
             canvas-diff.js  GPU vs software canvas A/B
             road-check.js   framebuffer invariants for coverage-based roads
             sndi-check.js   street-network SNDi metric (realism number for roadgen)
           ── audio gates & analysis (which to run: docs/guides/checks-and-oracles.md) ──
             wav-analyze.js / tune-check.js / dc-check.js / level-check.js / fx-check.js /
                             soak-check.js / web-audio-check.js   audio gates (see "Key things to know")
             schedule-check/ gate for schedule_at()/audio_time() sample-exact timing
             insert-latency.js  mic-in → output latency (insert-effect viability); --check
             click-check.js  click/splice discontinuity finder; --quiet gate
             stereo-check.js the only L/R-aware oracle (width, balance, pan trace); --expect, --check
             bypass-check.js does the mix reconverge bit-exact when an effect is switched out; --selfcheck
             arrange-score.js arrangement scorecard from traced notes (+ --sound); --selfcheck
             voice-trace.js  why a voice stopped (alloc/steal/choke events)
             wav-correlate.js / wav-envelope.js / wav-modrate.js / harmonic-spec.js   WAV A/B analysis
             ab-render.js    A/B a cart against itself by flipping one value; exits 2 if variants identical
             ref-render/     render from STK/luthier/Open303 so our models have something to be wrong against
             mode-extract.py struck-object modes from a recording → instrument_modes() tables
             patch-match/    sample in → eight matching patches (analysis-by-synthesis); --selftest = oracle
             filter-spec.js  measure a voice filter's response
             disp-model.js   dispersion-allpass design calculator (+ --body comb sizing); use instead of patching sound.h
             inharm-spec.js  partial positions in cents + stiff-string B fit (+ --decay); --check first
             formant-check.js f0 + formant oracle for pitch-shift
             psola-check.js  PSOLA artifact oracle (splice/periodicity/doubling); --selfcheck
             grain-check.js  INSTR_GRAIN gate
             scrape-check.js INSTR_SCRAPE gate (guiro/quijada vs samples)
             oneshot-compare.js tune a one-shot against a real recording; --selfcheck
             fetch-refs.js   rebuild gitignored CC0 reference audio from tools/refs/<set>.txt
           ── MIDI / host / threading / multiplayer gates ──
             midi-check/     MIDI out+in+cart-to-cart gate (zsh run.sh); run after midi_output.h/midi_input.h edits
             midi-note-check/ host MIDI notes reaching a cart (acidcandy mapping); design/host-midi-notes.md
             param-check/    host parameters reaching cart knobs (engine half); design/host-parameters.md
             sync-spike/     MIDI clock probes + end-to-end sync gate (needs IAC)
             mic-spike/      original mic probe (mic SHIPPED); design/mic-and-sampling.md
             input-ring-check/ host→engine input ring thread-safety (-tsan, -bypass control)
             present-race-check/ frame snapshot + deferred resize thread-safety (-tsan, -bypass)
             net-relay.js    lockstep-netplay relay for web carts; design/multiplayer-research.md
             net-check.js    lockstep gate
             webrtc-spike/   PASSED probe: browser WebRTC P2P
           ── per-instance engine / AUv3 ──
             instance-check/ N independent engines in one process (+ destroy, interleaving)
             instance-check/run-uictx.sh  cart-land header state per-instance under DE_CART_CTX
             engine-dylib-spike/ PASSED probe: K engines via K dylib copies
             tls-spike/      measured: ctx parameter free, thread-local costs per call
             state-check/    session save/restore acceptance test (+ migration)
             pro-check.js    Pro entitlement gate, both directions + AU target linkage
             ctx-gen.js      mechanically move engine statics into the context struct; --verify
             refactor-guard.js pure-refactor safety net: --bless then compare audio/frames/trace
             engine-statics.js how much process-global mutable state remains (clang AST)
             lint-engine-seam.js host↔engine boundary (one engine per host, handles honoured, no cart externs)
             lint-saved-state.js nothing instance-only in a saved slice + append-only layout
           ── sprites / art / generators ──
             sprite-draw.js  2D pixel-canvas API for .cart.js sprites
             sprite-preview.js render a .cart.js's sprites to one PNG
             pixelsnap.js    clean up "AI pixel art" onto a grid + small palette
             voxel-bake.js   ASCII voxel models → rotated sprite atlas; design/iso-rooms.md
             font-bake.js    bake TTF text into sprite canvases
             gen-rom-font.js bake extra bitmap fonts into the shared atlas
             gen-drumpat.js  GENERATE runtime/drumpat.h; --check/--selfcheck
             gen-rhythmbox.js GENERATE runtime/rhythmbox.h from the doc; --check/--selfcheck
             gen-dreamatlas.js GENERATE runtime/dreamatlas/biomes.h; --check
             gen-wavescan.js GENERATE runtime/wavescan_data.h (Plinky recipes); --check/--selfcheck
             gen-steinway.js GENERATE runtime/steinway.h; --check/--selfcheck
             gen-tcc-symbols.js regenerate runtime/studio_tcc_symbols.h
           ── orient / docs / repo health ──
             orient.js       bare = session front door; <cart> = build-context + cart-outline
             build-context.js reading briefing for one cart (meta, todos, mentions)
             cart-outline.js per-cart reading map (--fn <name> dumps one function)
             topic-brief.js  feature dossier across the repo from a term-set
             design-board.js every design doc + ADR by phase; --lint
             doc-status.js   (lib) STATUS-line parse + phase vocab
             api.js          one-shot studio.h API lookup
             api-usage.js    how often is X used (carts + elsewhere); --where; --selfcheck
             repo-doctor.js  one health strip over every meta-check; --quiet = CI
             lint-docs.js    doc links/§-refs + discoverability (tools + headers indexed here); --selfcheck
             cart-land-headers.js (lib) shelf vs engine-internal header split
             lint-xrefs.js   docs that should cross-link but don't; --selfcheck
             stale-doc-check.js docs citing gone code / tool drift / --driftable; --selfcheck
             lint-capability-claims.js docs saying we CAN'T do what we now can; --selfcheck
             wants-check.js  what a *-wants.md doc is actually still blocked on; --selfcheck
             capability-roster.js (lib) capability → proof/wire symbols
             lint-aux-params.js the five places the aux-param width must agree; --selfcheck
             lint-fxicons.js every FX_* kind has a glyph in fxicons.h; --selfcheck
             status-check.js index + drift check for docs/STATUS.md (never renumber); --selfcheck
             handoff.js      active-lane tracker for docs/HANDOFF.md (--check)
             gate-controls.js which gates can prove they can FAIL; --list
             build-compendium.js  generate docs/cart-compendium.html
             build-design-board.js generate docs/design-board.html
             build-reflections.js generate docs/reflections.html
             build-band-briefs.js generate docs/band-briefs.html
             build-field-notes.js generate docs/field-notes/FIELD-NOTES.md
             build-book.js   generate docs/learn-you-a-dreamengine.html from real harness output
             build-history.js generate docs/history.html
           data dirs: tools/assets/ · tools/fixtures/ (--selfcheck inputs) · tools/icon-masks/ (icon-mask.js) ·
                      tools/oneshot-specs/ (oneshot-compare.js) · tools/voxel-models/ (voxel-bake.js)
           det-probes/  cross-compile determinism oracles + software-rasterizer studies (README = index)
           carts/   <name>.c (+ optional <name>.cart.js); see docs/guides/cart-authoring.md
apps/      multi-cart app manifests (apps/<name>/app.json); tools/build-app.js builds them
data-tools/ per-cart data-acquisition tools (see data-tools/README.md):
             fmltools/   Floorplanner .fml → playable level; feeds floorwalker/seinelaan/floorplan
             roadview/   osm-roads.js: real OSM roads → JSON/.rvb for the roadview cart
build/     compile output (cart.c, binary, sprites.png, traces; .bake/ for live inspection)
docs/      all project docs, start at docs/README.md
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
- More in-game fonts via `font(FONT_*)`: `FONT_SMALL` 4×6, `FONT_TINY` 3×5, `FONT_COMIC` (Comic Mono
  Bold 10×20), `FONT_THIN` (IBM CGA thin 8×8), `FONT_TIC` (TIC-80 wide 6×6) — six total; `rottext`
  cycles them. **Adding a font:**
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
**Sounds like a keyboard / sequencer / cheap synth?** [`docs/guides/humanizing-sounds.md`](docs/guides/humanizing-sounds.md):
the player-model pattern, `instrument_vibrato` / `instrument_analog` / `MODE_BOW_PLUCK`, the template carts, measuring against CC0 recordings.

**Adding a cart** (full walkthrough: [`docs/guides/cart-authoring.md`](docs/guides/cart-authoring.md)):

1. Write `tools/carts/<name>.c`, opening with a `de:meta` block then a docblock (title + prose).
   The `de:meta` carries the metadata: `title`, `slug` (**= the `.c` filename stem** — the anchor
   from a baked `.cart.png` back to its source, for save-back/rebake; `lint-carts.js` enforces the
   match), `kind[]` (games need `genre`), `teaches` (from the `tools/teaches-vocab.js` vocab),
   `created` (today's `YYYY-MM-DD`), optional `lineage`/`homage`, and `description` (a string or
   `{summary,detail,controls}`). Schema:
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
  **zsh arrays are 1-INDEXED too** (`a=(x y z)` → `$a[1]` is `x`, `$a[0]` is EMPTY), so a bash-style
  `${a[$i]}` loop from 0 silently reads one slot early and an empty first element — it produced a
  confidently wrong measurement in this repo rather than an error.
  **zsh also SHADOWS some commands as builtins:** `log show …` hits zsh's `log` builtin, which prints
  "too many arguments" on *stderr* and nothing on stdout — indistinguishable from "nothing was logged",
  which is how it cost two wrong conclusions in one afternoon. Use `/usr/bin/log`.
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
- **To make a cart fill a bigger screen, REFLOW — never scale the render with `camera()`/`camera_ex()`.**
  `ui.h` hit-tests in raw canvas coords (`mouse_x`/`touch_x`), so a zoom/scale camera *silently*
  desyncs every button/knob/pad — taps land in the wrong place, no error. This is why device-adaptive
  is reflow-only (the fixed-canvas *scale* path was rejected, [`window-fill-scaling.md`](docs/design/window-fill-scaling.md)):
  make the cart `resizable`, read `screen_w()`/`screen_h()` + `safe_rect()`, lay out there (canvas ==
  device px → `ui.h` is 1:1). See [`device-adaptive-layout.md`](docs/design/device-adaptive-layout.md).
- **Don't name a variable after a built-in.** Cart code shares the `studio.h` namespace. `map` is the
  common trap (clashes with `map()`); use `grid`/`dmap`. Same for `line`/`rect`/`circ`/`print`/`spr`/
  `timer`/`now`. The starter cart also `#define`s `STATE`/`S`/`GameState` (cart-local persistent-state
  sugar over `de_state()`); a cart wanting `S` for else just removes those defines. **`SCREEN_W`/`SCREEN_H`/
  `SCALE` are `-D` compile flags too**, so `static const int SCALE[30]` expands to `int 4[30]` — the error
  points at the array, not the name, which is why it reads as nonsense (bit `keytrack`).
- **A regex `.replace()` whose replacement is a STRING lets the SOURCE text inject `$1`/`$&`.** Building a
  replacement by concatenation means any `$` sequence in the matched content expands: a cart whose `de:meta`
  contained `$100` produced invalid JSON when `backfill-slug.js` rewrote it. Pass a **function** as the
  replacement (`(m) => …`) and the string is used literally. Bites any tool that rewrites cart source.
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
  `road-check`, `build-all`, `spec`, the audio gates below, …). Its "Orienting" section also holds the
  **verify-a-claim-by-reading** rule: a grep finds *candidates* — read them before asserting a count
  ("N carts do X") or a capability ("the engine has no Y"); a keyword/API-*name* match misses recipes
  and hits prose.
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
  Linted: `node tools/lint-fx-frame.js` (`--quiet` = CI, `--strict` = gate-and-print; waive with
  `// fx-lint-ignore`; `--selfcheck` = known-answer fixture, 30 assertions — gated in repo-doctor,
  where the lint itself now sits at zero. Note a `for` BODY IS NOT a gate: `for (i…) crush(i, …)`
  rebuilds the DSP N times a frame, so put the per-channel `if` INSIDE the loop).
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
