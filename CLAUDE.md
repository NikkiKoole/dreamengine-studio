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
             lay.h       immediate-mode responsive layout — a `Box` + CSS-flavoured helpers
                         (split/at/cell/grid/wrap/aspect/fluid/pad + lay_lane = shared COLUMN REGISTER,
                         align stacked strips) that replace hand-rolled rect math for HUDs/panels/menus;
                         rect-in/rect-out, reflows live. See docs/design/device-adaptive-layout.md; demos respond/rackfit/acidfit
             ui.h        widgets (ui_button/ui_spr_button/ui_slider/ui_knob + Box/cell-sized twins
                         ui_button_cell/ui_knob_cell) — per-finger capture, fat-finger hit pads, focus
                         ring; ui_loupe() magnifier; ui_get_widgets() live rects
             face.h      the device-face GRAMMAR over lay.h: declare a FaceZone[] (BAND/LANE/HERO) and
                         face_layout() owns the chunky resize + safe-area box + zone-carving + shared
                         register, ENFORCING the rules (bands top/bottom only = no side-rail; hero = the
                         remainder). facedemo = grammar demo; deviceface = the raw-lay.h mechanism. Layer 3
                         of responsive-first-device-face.md
             disclose.h  device-adaptive panel DISCLOSURE over lay.h: a priority + finger-footprint
                         BUDGET pass (disclose_shape/_budget/_stack) that shows the panel you're on
                         EXPANDED, promotes the rest FOLDED→COMPACT while the budget allows, and
                         reflows as the viewport changes. For a rack with more panels than a phone
                         fits; it decides WHERE + HOW BIG, the cart draws the contents. acidwire
             gestures.h  per-finger swipes judged at lift + pinch_scale (whole-view zoom)
             pointer.h   multi-finger pointer pool (PTR_CLEAR/ACQUIRE/FIND) — layer below ui.h,
                         for bespoke per-finger targets, not widgets
             cursor.h    pixel MOUSE CURSOR in the canvas — cursor_draw(CUR_HAND/GRAB/ARROW/CROSS/MOVE)
                         + _tint; auto-hides the OS arrow ONLY when a real mouse is seen (no-op on touch),
                         call LAST in draw(); shows in screenshots/GIFs (the OS cursor never does)
             fxicons.h   the shared VISUAL LANGUAGE for the engine effects: one icon + body/accent
                         colour per FX_* kind, so every cart's "pedals" read the same. Drawing an
                         effect toggle/stompbox/mode chip? Reuse the glyph, don't redraw it.
                         pedalboard/epiano/pedalicon. Sibling of ampcab.h (the TONE half)
             shadermath.h a GLSL-vocabulary toolkit for CPU SHADERS: vec2/vec3 + dot/normalize/cross, the
                         scalar idioms studio.h lacks (fract/smoothstep/mix/step/sat), colour helpers
                         (rgb/hsv/palette = the IQ cosine palette) + SDF primitives & combinators
                         (sd_sphere/box/torus/plane, op_union/sub/smin). For a cart drawing per-pixel
                         through pset_rgb/rectfill_rgb that wants to read like Shadertoy. raymarch
             demath.h    DETERMINISTIC float math — the same BITS on arm64/x86-64/wasm: de_sinf/cosf/
                         tanf/expf/logf/powf/tanhf/atan2f/asinf/acosf/hypotf + de_sin_turns (takes TURNS,
                         the phase-accumulator form: exact reduction, 45% faster than libm). Auto-included
                         by studio.h. WHY: IEEE 754 pins down + - * / and sqrt but says NOTHING about the
                         transcendentals, so every libm rounds them differently — which silently breaks
                         replays/ghosts/lockstep and made carts sound different in the browser. Reach for
                         de_* in a cart whose OUTPUT is COMPARED (a spec() oracle, a replay, a golden
                         image); plain libm is fine for decoration. sqrtf/fabsf/floorf/fmodf/fminf/fmaxf
                         are ALREADY bit-exact by spec — no de_ version, none needed. Pairs with the
                         FP_CONTRACT pragma in studio.h (both are required; neither alone works).
                         Gated by det-probes/demath.c. docs/design/determinism.md
             keybed.h    polyphonic chromatic keybed (touch+mouse+QWERTY+MIDI) — every keybed
                         cart copies it (epiano/moog/touchpiano/mellotron); NOT ribbons/radio strip
             mono.h      MONOSYNTH KEY ASSIGN (audit §B3): which held note sounds (priority LAST/LOW/
                         HIGH/FIRST) and whether a press restarts the envelope (trigger SINGLE/MULTI/ANY),
                         as one Mono struct + mono_press/_release returning START/GLIDE/RETRIG/STOP. Reid
                         calls this what decides whether a synth feels playable; tb303/acidrack/moog/sh101
                         each hand-rolled a different answer. Pure logic, no engine surface, no UI — so it
                         carries its OWN spec (mono_selfcheck, Part 18's four-priority table). sh101 drives
                         it (PRIO/TRIG on the panel). NOT keybed.h (the widget) or solo.h (the radio strip)
             solo.h      scale-locked solo strip the player drives over a radio (pairs radio.h)
             radio.h     radio-station chrome (chassis, seeded songs, draggable control knobs)
             improv.h    melodic improvisation for the radio stations (auto-solo)
             cards.h     shared playing-card rendering (blackjack/poker/solitaire/strippoker)
             endcard.h   the shared WIN/LOSE end-screen treatment (dither curtain + pop-in card) —
                         use it, never hand-roll fade(0.5)+box (the recurring "end-fade issue")
             worldnet.h  the infinite deterministic WORLD SPINE (terrain + ranked lattice + spline
                         links + the wn_road_at() edge-graph query) — roadnet2 = home, sloop drives
                         it (N). ONE data model: query this, never re-roll a street field
             roadkit.h   the shared AT-GRADE JUNCTION GEOMETRY (pure, param-driven): curb_return
                         (tangent-arc fillet) + edge_corner + rk_count_corners(brg,n) + rk_cross_hw
                         + the N-arm-native FIELD (RkField + rk_field_build/rk_field_road = arm-union ∪
                         fillets ∪ disc, space-agnostic). A junction = N arms at bearings; caller
                         supplies the array. streetlab = spec-locked source (104/0, scans in screen px);
                         citydrive draws curb-return junctions through it in ground metres (J). B4 grade
                         dispatch (roadlab interchanges) = WIP
             citygen.h   the calibrated procedural CITY GRAMMAR (worldgen rungs 2–5): density field
                         + tensor-field arterials + district minor-street fill, SNDi-matched to real
                         Rotterdam; citygen_road_at() = the drive seam. citygrow = tuning bench/home;
                         SLOOP DRIVES IT (press M — a bounded generated city, grip on/off street).
                         Open: reconciling its world model with worldnet.h so N-spine + M-city are one
             physics.h   tiny shared VERLET toolkit — PhysPt + phys_integrate/link/collide/bounds/aim
                         (ropes/cloth/ragdolls/blobs/pseudo-solids). Cart owns its arrays+step loop; NOT
                         a rigid-body engine (that's the Layer-1 seam parked in design/physics-notes.md).
                         verlet=demo, physics=inline teacher; used by ragdoll/linerider/coaster/jelly/sloop/tentacle/waterjar (PBD fluid)
             boxrig.h    the "sprite region → ≤8-vert Box2D polygon + texture" toolkit (the puppetmaker
                         representation): boxrig_hull (sprite alpha → b2Hull) + boxrig_draw (tritex the
                         polygon from its OWN verts so paint covers the hull exactly) + point_in_body +
                         boxrig_resolve_box/poly (verlet↔rigid coupling with physics.h). Cart still owns
                         body/joint/density policy. Needs box2d/box2d.h (build-box2d.sh); takes ppm +
                         screenH, no globals. puppet/boxlab/boxjelly; design/box2d-integration.md
             drumkit.h   shared PLAYABLE drum kit (the engine has no INSTR_KIT): a role vocab
                         (KICK/SNARE/HAT/OPEN/CLAP/TOM/CRASH) + fixed slot layout + dk_fire(role,midi,vel)
                         with pitch as a param. Header owns the trigger SKELETON; each kit's build()
                         owns the SOUND (808≠909 stays). Built-in DK_ELECTRO/DK_ACOUSTIC; sampler uses it
                         to sample drums. A kit is a pad MAP, NOT keybed.h (which pitches one voice).
             acid303.h   the shared TB-303 acid VOICE (extracted because tb303/acidrack had drifted their
                         own copies): an Acid struct + acid_note/acid_ride/acid_gate/acid_off/acid_tie — a
                         FILTER_DIODE held voice with non-retriggering slide, accent kick, two-decay, soft
                         attack, filter tracking + octave-down sub-osc (acidrack's superset). Params floats
                         0..1 (ACID_* enum). Cart owns the PATTERN, header owns the SOUND. acid_stock() =
                         vanilla voicing (cut_top 6.0 + slapback), else Devil Fish wide — BOTH from one header,
                         chosen by data. Used by tb303 + acidrack (both byte-identical A/B, 1.00000) + acidcandy.
                         NOT drumkit.h (that's the pad MAP, this pitches one voice).
             tr808.h     the shared TR-808 VOICE BANK (the acid303.h move, for drums — a faithful machine, not
                         the generic drumkit.h): tr808_build(base) = the 14-slot bank from the reverse-engineered
                         circuit values; tr808_fire(base,role,boost,delay,kt,kd,kc) = the layered trigger + tune/
                         decay/colour knob maths. TR_* 16-voice roster; cart owns the knob arrays. tr808 + acidcandy.
             tr909.h     the shared TR-909 VOICE BANK: tr909_build + tr909_metal (metal-highpass XY) + tr909_fire
                         + tr909_fire_stroke (flam/drag/ratchet). TR9_* 11-voice roster. tr909 + acidcandy.
             morphdrum.h the MORPHING DRUM VOICE bank, the honest core of "supergroovebox": an 808 kick and a
                         909 kick are the SAME structure at different parameter values, so each voice is ONE
                         parametric synth with a deep knob panel. CHARACTER (0=808…1=909) morphs the structural
                         voicing; the rest are absolute controls reaching PAST both machines. morph_build/_ride/
                         _fire, params 0..1 by MD_*, MD_PANEL[] = the knobs to lay out. NOT tr808/tr909.h (two
                         FIXED faithful recipes) and NOT drumkit.h (a pad MAP): this is the SPACE between. morphbox
             ampcab.h    the shared guitar AMP/CABINET voicing table: five amps as preset BUNDLES of effects we
                         already ship (instrument_drive + a DRIVE_* shaper + eq + glue), pinned like leslie, NOT
                         new DSP. ONE truth so the standalone amp + pinned cabinet agree. combo/pedalboard/
                         afrobeat/mixbooth/wba; design/effects-bus-architecture.md §E. Sibling of fxicons.h (LOOK)
             harmony.h   the shared HARMONY BRAIN (bidirectional): 13-function roman-numeral vocab + per-style
                         Markov tables (HB_BOSSA/HB_COCKTAIL, byte-exact) — hb_pick (generate) + hb_suggest
                         (ranked next chords + reason) + hb_analyze (key-in numerals, -1 = honest ?). Voicing
                         stays rad_lead_to. bossa + cocktail + chordwise; design/harmony-brain.md
             json.h      EXPERIMENTAL cart-land JSON reader (jsmn, zero-alloc tokenizer): slurp
                         de_data_path() (the --data flag / $DE_DATA) + walk it, so a cart loads its data at
                         RUNTIME instead of baking C arrays. Swap the file, don't regenerate the cart.
                         citydrive/floorplan/roadview; design/external-data-carts.md. Not committed API.
           Full table + contract: docs/guides/cart-authoring.md → "Cart-land library headers".
           Sound/instrument cart? docs/guides/instrument-carts.md indexes the shelf by block copied.
           lockup/    NOT shelf — ONE cart's private modules (the `lockup` prison sim), on the
                      include path only because -I runtime already is. model.h is a FROZEN CONTRACT
                      (types + signatures + the data tables + the sprite slot map); grid/path/actors/
                      econ/art/score/hud each implement one slice, include ONLY the contract, and
                      prefix every static (lkg_/lkp_/lka_/lke_/lkr_/lks_/lkh_) because it is all one
                      translation unit. Written by 8 parallel agents; the pattern is ADR-0034.
                      Don't #include these from another cart — copy what you need.
           tenement/  same pattern, NOT shelf: the frozen contract for the `tenement` sim (several
                      households in one building), specced + compile-checked, cart not written yet.
                      Its ONE rule: nothing enumerates instances, everything matches on TAGS —
                      no place owns a recipe, nothing switches on an object kind. docs/design/tenement.md
           isoroom/   generated (tools/voxel-bake.js --emit-c), not hand-edited: the baked rotation
                      cell table for the `isoroom` probe. docs/design/iso-rooms.md
editor/    electron/ (main.cjs compiles+runs carts; preload.cjs exposes window.studio.*),
           src/ (shell.js IDE chrome, main.js CodeMirror+cmd-click, navigate.js engine-source
           viewer, outline.js, sprite-editor.js, map-editor.js, studioDocs.js = single source for
           API docs, settings.js, theme.js), public/ (fonts, dos_8x8.png, palettes/pico32.json),
           vite.config.js (serveDocs plugin → Docs tab + engine-source viewer), index.html.
           **What the editor can DO** (record/bake clips, 9:16 ratio-variants, the trailer builder,
           the Apps ASO lab, Share) → the capability index docs/guides/editor-features.md — check it
           before hand-building an editor feature (the tools/ list below covers CLI tools only)
Makefile   `make` kills stale processes + starts editor, +floorplan `--serve` fetch-bridge if a token is set (targets: make / start / serve / install / help)
tools/     repo-root CLI tools (plain `node`, CommonJS). One line each — read the file header for
           the full contract:
             make-cart.js    build/bake .cart.png from tools/carts/<name>.c; also a lib for play.js
             play.js         debug harness driver (record/replay/script + trace + --wav + --solo-slot stem)
             make-gif.js     capture an animated clip of a cart (webm/webp/gif/mp4/apng + audio)
             dress-clip.js   DRESS a clip into a 9:16 Short with hand-typed text in the letterbox bars —
                             drawn in the REAL engine dos_8x8 pixel font with BOIL + a tween-in entrance (bakes two
                             titlecard cards on magic-green + colour-keys them into the bars, framed console; --bg/
                             --accent, --boil/--breathe/--anim, --mp4|webm; --preview <png> = fast drawtext LAYOUT
                             still, --secs <n> = short motion preview). The "add text" step of record→bake→dress→post;
                             DRIVEABLE FROM THE EDITOR (Promote §A ✨ dress → modal: layout preview + ▶ preview motion +
                             kinetic knobs). Reuses titlecard (the trailer text renderer). Follow-up: youtube-push
                             --dress. Design: docs/design/export-ratios.md "Dressed composite"
             compose-clips.js stitch baked clips into one reel (ffmpeg xfade) from a .reel manifest; bakes @card
                             text parts + composites timed `over` overlays (colorkey); `# frame letterbox` = the
                             DRESSED style (console centred + device frame + bars; pos top/center/bottom = bar/
                             over-console/bar) — the multi-overlay twin of dress-clip.js
             build-app-reel.js  APP TRAILER: apps/<name>/app.json carts[] → bake a clip per rack (skips racks with
                             no committed clip) → generate tools/reels/<name>.reel → compose-clips → one reel. The
                             multi-cart video unit; the .reel is committed + hand-editable. Design: docs/design/demand-generation.md
             net-relay.js    lockstep-netplay RELAY for web/wasm carts (zero-dep hand-rolled WS): rooms by code,
                             BLIND byte-forwarding (never parses game packets — one relay serves every cart);
                             --serve <dir> = the one-wifi-box setup (cart + relay in one process); --check
                             self-test. Rung 5a — docs/design/multiplayer-research.md
             midi-check/     the MIDI gate, BOTH DIRECTIONS (`zsh tools/midi-check/run.sh`, `-v` keeps the logs).
                             Phase A (OUT): runs the `midiout` cart plus a REAL CoreMIDI listener (`listen.c`) in a
                             SECOND PROCESS and asserts what arrived — channel, note, velocity, controller, no stuck
                             notes, clock at 24ppqn — plus a **negative control** (the same run without `--midi-out`
                             must publish NOTHING, the only way to tell "correctly gated" from "not gated at all").
                             Phase B (IN): `send-cc.c` publishes a virtual SOURCE (the engine listens to sources and
                             is never a destination, so an output port has nothing to push into) sending CC on
                             channels 1/10/16, asserted out of the cart's `--trace`; the check that carries the
                             weight is **channel ISOLATION** — cc74 read on ch2 must be -1, since per-cc checks
                             alone cannot see a dropped channel nibble. Phase C (CART TO CART): `epianojam`
                             sends, `epiano` RECEIVES and renders to a WAV that must be loud — the only
                             end-to-end cover of the NOTE input path (parse → ring → midi_get → keybed.h →
                             a voice), which A and B never touch. Its control turns epiano's AUTOPLAY off
                             (on by default, a triad every 2 beats), without which "it made noise" proves
                             nothing; that control measures peak -inf. Needs NO IAC bus and no DAW, unlike
                             sync-spike. Run after touching runtime/midi_output.h or midi_input.h.
                             THREE traps it documents, each of which made a healthy engine look broken: det-turbo
                             compressing a 7s run into ~100ms (the port is born and disposed between polls — hence
                             no `--headless` on the timed runs) · `CFRunLoopRunInMode` returning INSTANTLY when the
                             run loop has no sources, so a "wait 12s" loop finishes in microseconds · and a
                             long-lived CoreMIDI client needing a NOTIFY PROC + a pumped run loop, or
                             `MIDIGetNumberOfSources()` answers 0 forever while a fresh process sees the port fine.
                             Plus two the gate itself got wrong: `kill`ing a play.js PID leaves the CART
                             orphaned and still sending (so phase C runs its control FIRST rather than
                             managing the race), and MIDIReceived is ASYNC so disposing the endpoint right
                             after the shutdown note-offs drops them — which made "no stuck notes" pass by luck
             net-check.js    the one-liner LOCKSTEP GATE (netplay twin of tune-check): echo-mirror + netdemo
                             pair + relay wire-protocol sim, PASS/FAIL; run after touching net.h / the net seams
             webrtc-spike/   PASSED probe (multiplayer rung 5b): browser WebRTC P2P DataChannel, Mac↔iPhone at
                             ~12ms over wifi (index.html — open on both peers). The relay-free twin of net-relay;
                             design/multiplayer-research.md
             input-ring-check/  THREAD-SAFETY gate for the host→engine INPUT RING (`de_touch_*`/`de_key_event`
                             → `de_input_beginframe`, in runtime/raylib_compat.c). `bash run.sh` · `-tsan` (the
                             real gate: ThreadSanitizer sees the race even on a run that wins) · `-bypass` (the
                             NEGATIVE CONTROL — rebuilds with the producer writing engine state directly, the
                             pre-ring design, and FAILS as it must). #includes raylib_compat.c so it gates the
                             shipping code, and plays host + engine on two real threads because the race only
                             exists where de_frame runs off the input thread = the AUv3. Also pins the
                             ONE-FRAME PRESS rule (a tap arriving between two frames must still be visible)
             present-race-check/  THREAD-SAFETY gate for the other half of that split: `de_copy_frame`
                             (the published frame SNAPSHOT) + the DEFERRED resize path in studio.c. Builds the
                             REAL engine like build-nr.sh, then blits + resizes from a second thread while the
                             engine draws. `-tsan` (the real gate — it caught a pointer published outside the
                             seqlock) · `-bypass` (negative control: the naive host reading the LIVE canvas,
                             which tears). WHY: de_resize reallocs the framebuffer, so a plug-in view doing
                             the obvious thing is a use-after-free in the HOST, blamed on us
             ui-audit.js     UI bug finder (off-screen text, overlaps, dead widgets, hidden panels).
                             `--selfcheck` = known-answer fixture (31 assertions, runs NO cart: the
                             analyzer is pure, so it judges synthetic draw records) covering every
                             finding kind AND every exempt class (clip / occlusion / identical strings /
                             the ≤3px widget threshold / slivers / transients) + the whole
                             `// ui-audit-ignore` waiver subsystem, which no cart exercises. In repo-doctor
             mirror-diff.js  golden-pixel-diff harness: assert a render's symmetry invariant headless
             canvas-diff.js  GPU-vs-software-canvas render oracle: A/Bs a cart in both modes + pixel-diffs;
                             guards the sw_force_gpu/DE_CPU_RASTER gotchas; --bytecheck (sha) / --raw / --max / --heatmap
             road-check.js   correctness oracle for coverage-based roads: framebuffer invariants (no naked
                             edges / strays / floating kerb) at ANY angle; --all = config-matrix gate; --overlay
             sndi-check.js   street-network METRIC oracle (worldgen-plan rung 0): the SNDi composite (degree
                             shares/dendricity/circuity/sinuosity/orient-entropy) from a real .rvb OR a
                             generated-graph JSON; multiple files = side-by-side A/B; --check self-test.
                             THE realism number for procedural roadgen (generated matches real = done)
             build-site.js / publish-cart.sh   build wasm carts + gallery → site/; publish + push
             publish-all.js  batch publish in ONE deploy — run it bare: it CHECKS cart-status, then
                             PROMPTS (smart=not-published+stale / +engine-stale / --all / build-all gate).
                             Resilient build, reuses publish-cart.sh; -y non-interactive, --dry-run preview
             mobile-lint.js  static report card: can a phone play this cart? The verdict is a
                             PRECEDENCE chain (best input path a phone can use, not worst) over source that
                             is comment-stripped + library-header-inlined with studio.h SKIPPED (its
                             prototypes name every input fn, so inlining it makes EVERY cart read
                             touch-ready). `--selfcheck` = known-answer fixture, 27 assertions
             aso-research.js App Store keyword research from FREE public data (iTunes Search API, no account):
                             per term a difficulty proxy + top competitors + your own rank (--app) + mined
                             keyword candidates; --json snapshots. The Search-Term-Rank popularity column waits
                             on Apple's beta. Design: docs/design/store-agents.md §ASO
             aso-suggest.js  the DEMAND-side twin of aso-research (which measures competition): free stand-in for
                             Apple's popularity column via Google Autocomplete ("alphabet soup" — no key/account).
                             Two outputs: single WORDS → the App Store keyword field (→ aso-compose) + natural
                             PHRASES → website/press-kit SEO. --quick/--json. Design: docs/design/store-agents.md §ASO
             aso-brief.js    the PALETTE: generate a committed SEO worksheet (apps/<name>/seo-brief.md) you write
                             press.md + the listing AGAINST — char budgets, store WORDS (→ compose), press PHRASES,
                             a competition table. Seeds from de:meta; runs research+suggest. Emits vocabulary, NEVER
                             prose (no AI-generated press kits). Design: docs/design/store-agents.md §"palette + mirror"
             aso-coverage.js the MIRROR: check finished press.md + listing vs seo-brief.md — phrase/word coverage +
                             a STUFFING warning (exits nonzero only on stuffing: fails you for reading robotic, not
                             for a missed keyword). The ADR-0022 two-part bar on the store page. §"palette + mirror"
             aso-score.js    SCORE a listing (title/subtitle/keywords) + A/B a TWEAK against the committed one with
                             deltas, in the terminal for the tweak→score→tweak loop WITH the agent. budget + hygiene
                             (aso-lint rules as penalties) + reach (seo-brief demand-word coverage) + WINNABILITY
                             (--deep: per-keyword difficulty from aso-research — flags HARD terms a new app can't
                             rank for). CLI drives the A/B tweak loop; editor Apps card has a 📊 score GLANCE
                             (renders the scorecard WITH its gotchas). Design: docs/design/store-agents.md §"palette + mirror"
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
                             centered on a bg + caption (Comic Mono Bold, the editor font) in the breathing room; --font
                             swaps it (Bungee etc). Zero engine work. ffmpeg-based,
                             no node deps. Feed it a play.js --dump frame. Design: docs/design/store-agents.md §1
             icon-mask.js    the APP-ICON MASK template + "which pixels get cut off" oracle. The mask is MEASURED
                             from Apple's own renderer (Xcode 26's Icon Composer `ictool`: the alpha of a flat
                             full-bleed render IS the mask), committed as tools/icon-masks/ios26-2048.png; `--check`
                             re-derives + diffs it (repo-doctor gate, skips without Xcode). `template` = the guide you
                             draw against (red = gone; --overlay = a transparent LAYER; --inset px = the offset curve to
                             put your own chassis border ON, because a hand-drawn rounded rect has circular corners
                             where the mask has continuous ones and gets shaved, the trap that bit two apps in review) ·
                             `check <icon.png>` = per-corner flat-background-vs-lost-detail report + a 3-up proof PNG,
                             --quiet gates a release · `preview` = what it will LOOK like, masked + shrunk to every
                             real display size (1024 store · 192 home · 120 Spotlight · 87 Settings · 60 notif),
                             light + dark, which is where a lo-fi icon dies · `device` = GROUND TRUTH, installs the
                             icon into a booted iOS 26 simulator and crops what the home screen actually drew (the
                             mask matched it to mean 0.13px, and a flat PNG gets NO gloss). Runs itself from
                             build-app.js icon staging + the editor Apps tab 🎨 icon button. SAFE RULE: the
                             inscribed circle is entirely inside the mask. Design: docs/design/app-icon-mask.md
             store-contact.js  build a labelled CONTACT SHEET from a play.js --dump dir (evenly-sampled, numbered
                             tiles + a tile→frame map) so an agent eyeballs a whole clip and PICKS the hero shots to
                             feed store-shots.js — the deterministic half of §1's hero-frame director (agent chooses).
                             ffmpeg-based, no node deps. Design: docs/design/store-agents.md §1
             press-kit.js    generate a self-contained presskit()-style PRESS PAGE + assets zip for an app, from
                             apps/<name>/app.json + apps/<name>/press.md + apps/studio.md + store-shots/make-gif assets
                             (prose in markdown, structured in flat frontmatter; no node deps). Channel-A own-domain
                             artifact. Design: docs/design/press-kit.md
             asc-push.js     PUSH the non-cart product surface to App Store Connect from apps/<name>/app.json — the
                             in-house upload tool ADR-0026 chose over Fastlane (zero deps: Node fetch + ES256 JWT).
                             --metadata (title/subtitle/keywords/desc/promo/URLs/copyright) · --screenshots · --iap
                             (create→localize→price→availability→review-shot→1024² promo image → READY_TO_SUBMIT,
                             idempotent; images from apps/<app>/iap-images/<slug>.png) · --promote (mark IAPs
                             promoted purchases → product-page + search surface; pairs with Store.swift PurchaseIntent) ·
                             --new-version (a LIVE app has nothing editable, so an UPDATE starts by creating the
                             next version; idempotent) · --screenshots --replace (an update INHERITS the old
                             shots and upload APPENDS) · --dry-run GETs live + diffs · --check offline gate.
                             Auth: ~/.appstoreconnect/ (.p8 + config.json), never git.
                             Design: docs/design/store-agents.md §"ASC upload + TestFlight tool"
             leads.js        the local MARKETEER (demand GENERATION, the twin of the aso-* capture tools): maps a
                             cart's de:meta to its TRIBE(s) + the venues where that tribe gathers (`match`), hunts
                             NEW venues (`discover` = ready search urls + Google-autocomplete community signals; Reddit's
                             free API is dead), scaffolds a gift-first post from the cart's OWN words + the tribe hook
                             (`draft` — no invented prose, per store-agents' rule), and tracks outreach (`track add`).
                             Ledger: tools/leads-ledger.json (committed, hand-editable; seeded from tinyjam-marketing §3.9).
                             Design: docs/design/demand-generation.md (lever #3 "showing up in the tribe")
             reddit-gaps.js  demand DISCOVERY (what to BUILD next; the third demand tool after aso-* capture +
                             leads.js generation): mine a tribe's public RSS (listing + `search.rss` wish-probes —
                             the API is approval-gated + broken, RSS is the open front door) → wish-mine (regex
                             patterns) → cluster by topic → cross-reference the cart shelf → rank GAPS (high
                             demand × low coverage × on-grain). No score in RSS → ranks by CLUSTER SIZE. No
                             auth/deps; polite+cached; `--check` self-test; `--drip` = fetch the stalest sub
                             (for a cron/launchd drip); `--ingest <file>` = paste-bridge for browser-saved RSS.
                             THREE passes, each blind to what the others see: wishes (what the tribe ASKED
                             for) · `--showcase` (what it UPVOTED — needs a post old enough to have trended) ·
                             `--launches` (who SHIPPED into our lane, date-windowed — a build from this week
                             has no ask and no time to trend, so the other two CANNOT see it; runs in the drip
                             log too, since supply news rots in days). `THIN` (◐) = covered on paper but
                             demand ≥3× our carts — on a 255-cart shelf `GAP` is nearly extinct and THIN is
                             where the next candidate comes from. Design: docs/design/demand-discovery.md
             reddit-gaps-drip.sh  the scheduled-drip RUNNER for reddit-gaps (loaded by the macOS LaunchAgent
                             com.dreamengine.reddit-gaps-drip): resolves node under launchd's bare env + fires
                             `reddit-gaps.js --drip` (stalest sub in reddit-gaps-subs.txt) every 6h. `zsh tools/reddit-gaps-drip.sh` to fire once
             youtube-push.js the DISTRIBUTION last mile of demand lever #2 (twin of asc-push, ADR-0033): recipe →
                             bake mp4 (make-gif) → composite a 9:16 SHORT (integer-upscale + pad, never stretch; >60s
                             refused) → resumable upload → URL back. --landscape = full 16:10; --reel <app> = an app
                             trailer; metadata derived from cart de:meta / app listing (no hand-typed copy). OAuth2
                             creds in ~/.youtube/ (--auth one-time), NEVER git. --dry-run plan / --check offline gate.
                             Design: docs/design/video-distribution.md
             wav-analyze.js / tune-check.js / dc-check.js / level-check.js / fx-check.js /
                             soak-check.js / web-audio-check.js   audio gates (see "Key things to know")
             click-check.js  the CLICK/SPLICE oracle: waveform DISCONTINUITIES in a WAV and WHERE (first
                             difference vs the LOCAL step-rms, so a saw's flyback isn't a false positive; a cart's
                             own slope ≈2-3x, an audible click 6-20x). `--quiet` = PASS/FAIL gate. Run it after any
                             mid-note wave_set / table swap / envelope-shape edit — an envelope plot cannot tell a
                             clean ramp from a splice, which is how martenot's 8-step morph shipped crackling
             stereo-check.js the STEREO oracle — the ONLY gate that reads L and R apart. Every other audio
                             tool's `readWavMono()` averages the channels at the door, so autopan / pan_law /
                             the stereo soft-clip / chorus width had ZERO coverage; worse, a mono downmix is
                             actively BLIND to antiphase panning (gL+gR ≈ constant, so summing removes it).
                             Reports corr · width (side/mid) · balance · mono-fold loss · a pan TRACE over
                             time (excursion + LFO rate — a hard-sweeping pan and a dead-centre file have the
                             same MEAN pan). `--expect mono|wide|decorrelated|autopan [--rate hz]` = PASS/FAIL
                             gate, `--check` = self-test vs synthetic signals (RUN IT FIRST — a broken analyser
                             and a mono file print the same thing). Run after any pan/width/stereo edit
             voice-trace.js  read a --trace run's voice-allocation events (on/off/reuse/steal/choke, naming the
                             victim) → why a voice stopped; twin of play.js --solo-slot (stem render). For "a solo got
                             cut off by another instrument". Design: docs/design/audio-voice-debugging.md
             wav-correlate.js / wav-envelope.js / wav-modrate.js / harmonic-spec.js   WAV A/B
                             analysis (sample correlation / amp+brightness+spectral-centroid envelope,
                             `--from/--to` region / LFO rate+depth / harmonic series) — for A/B-ing a
                             render against navkit (or before/after an fx like crush/filter/EQ)
             ab-render.js    A/B a cart against ITSELF: flip one file-scope value (`static x = …;` or
                             `#define x …`), render each variant, print sha+peak+brightness+centroid in one
                             table, and ALWAYS restore the source (finally-block, survives Ctrl-C). Use it
                             instead of hand-sedding a flag — it EXITS 2 and shouts if two variants render
                             byte-identical audio, i.e. the flag never reached the DSP and the numbers are
                             meaningless (that bug already cost a bogus finding once). The LISTEN-item
                             workhorse for design/synth-secrets-plan.md; measures, never judges
             filter-spec.js  measure a per-voice FILTER's actual response (slope dB/oct, resonance peak,
                             bass drain per res step) via a generated probe cart — acceptance evidence for
                             any sound.h filter change; born from the 303-fidelity spike (audio-notes §25)
             disp-model.js   the DESIGN counterpart to inharm-spec (which measures a render): what a
                             dispersion allpass cascade does to a waveguide's partials, computed
                             ANALYTICALLY from the loop phase condition. Solves the coefficient for a
                             target inharmonicity B per note/stage-count + reports the phase delay it
                             adds at the fundamental (which must come OUT of the delay line or the note
                             plays flat) and what is left of the line. `--curve` = forward sweep,
                             `--check` = self-test incl. an engine-validated point. Use it INSTEAD of
                             patching sound.h to search a grid — that is slow, and it holds a shared
                             engine broken while parallel agents compile (it bit twice; a timeout's
                             SIGTERM skips the restore). Patch the engine only to CONFIRM one point.
                             `--body` = the §M2 COMB BODY half (an instrument body is a small reverberant
                             room): resonances + colouring for a set of 1-4 ms lines, and `--lowest <hz>`
                             SIZES the set for a bigger instrument (violin air ~280 Hz, cello ~110) while
                             reporting the BUFFER BUDGET — which is the constraint: a cello needs 401
                             samples/line against the engine's 256 stride, so it does not fit pn_ks2
             inharm-spec.js  WHERE the partials sit, in cents vs the ideal n·f0 — the third leg beside
                             harmonic-spec (how LOUD) and filter-spec (what the filter did). Fits the
                             stiff-string B of f_n = n·f0·√(1+Bn²) + prints the residual, so "the partials
                             moved, but not the way a string moves them" is visible. Probe mode = any engine
                             × velocity × time-window (does inharmonicity respond to LEVEL, does it RELAX);
                             WAV mode for any region. `--check` self-tests against synthetic known-B spectra —
                             RUN IT BEFORE BELIEVING A NULL, since a broken tool and an harmonic engine print
                             the same table. Found audit §I4b/§I4c/§I4d (PIANO's dispersion inert + only half
                             its Railsback stretch reaching the sound + a loop tuning offset). Modal engines
                             (MALLET/MEMBRANE) are out of frame — they have no energy at n·f0 at all.
                             `--decay` = PER-PARTIAL decay rate (dB/s): wav-envelope gives the whole
                             signal, which cannot tell "the fundamental dies faster" (a loop bug) from
                             "only the upper partials do" (spectral). That distinction settled the §I4b
                             sustain scare — reach for it whenever sustain changes and you need to know
                             WHERE the energy went
             formant-check.js  the VOICE oracle: f0 (autocorrelation, mean + WOBBLE) + F1/F2/F3 formant
                             peaks (Welch-smoothed spectral envelope) of a WAV region — the pitch-moved-AND-
                             formants-held gate for sample_autotune / any pitch-shift (design/transparent-autotune.md)
             psola-check.js  the ARTIFACT oracle for the PSOLA pitch engine — the "does it CLICK" twin of
                             formant-check's "is it in TUNE". Renders voxshift's four takes and runs THREE
                             detectors, because each is blind to a defect the others catch: SPLICE (first
                             difference) · PERIODICITY (x[n]-x[n-T] vs the RAW take as CONTROL) · DOUBLING
                             (f0 vs expected — the only one that sees period doubling, which is still
                             perfectly periodic so PERIODICITY scores it as an IMPROVEMENT). Run it before
                             AND after any at_psola_slot edit; --quiet = CI, --save re-blesses. It killed a
                             bad epoch-marking change in one command that had previously cost several
                             listen-and-report rounds. contemporary-rebirth.md §"Rung B … postscript"
             sprite-draw.js  reusable 2D pixel-canvas API for programmatic .cart.js sprites
             sprite-preview.js  render a .cart.js's sprites to one labelled PNG (no compile/run) — the tight loop for code-drawn sprites
             pixelsnap.js    clean up "AI pixel art": snap soft/off-grid pixels onto a real grid + posterize
                             to a small palette (median-cut / --palette pico32 / --two ink,paper), OKLab match,
                             FS or ordered/diagonal dither, --clean despeckle; never overwrites the input
             voxel-bake.js   bake tiny ASCII VOXEL models (`tools/voxel-models/*.js`) into ROTATED sprite
                             cells + a packed atlas. Solves the art wall under a rotating isometric view:
                             8 rotations hand-drawn is 8 drawings per sofa, and rendering voxels with
                             tritex every frame is the one thing measured too slow on device (ADR-0024).
                             So author once, bake all rotations at BUILD time, runtime is sspr() + a
                             painter's sort. LIGHT IS FIXED IN SCREEN SPACE (shade by world normal and it
                             appears to rotate with the room). Reports TOTAL ATLAS PIXELS, because
                             make-cart.js caps a cart sheet at 128×128/64 slots. `--check` = 32 known
                             answers. docs/design/iso-rooms.md
             font-bake.js    bake real-TTF text into sprite-draw canvases at build time
             gen-rom-font.js bake the "extra" bitmap fonts (ROM dumps + EPX) into the shared atlas
             build-cart-index.js  GENERATE editor/public/carts/index.json from each cart's de:meta block
                             (cart owns its metadata; index.json is a derived view); --check gates staleness
             lint-carts.js   validate each cart's de:meta (tags/status/created/description) + assert
                             index.json in sync; owns the tag vocabulary. Also the SOURCE hazards
                             (promoted gotchas): watch() 2nd-arg-must-be-format-string + built-in shadowing
                             + hand-rolled finger pools. `--selfcheck` = known-answer fixture (48 assertions:
                             every hazard AND every one of its exempt classes — comment / struct field /
                             waiver / grandfathered cart / paren-nested arg — plus the de:meta case table),
                             gated in repo-doctor
             collections-vocab.js  the CONTROLLED, DOC-ANCHORED vocab for cart collection[] — the
                             cross-cutting research THREADS ({slug,title,doc,blurb}, like teaches-vocab.js).
                             A collection = a sprawling experiment that owns a doc; every slug MUST point at
                             one (lint-carts asserts it exists). Field/design: design/cart-metadata.md, 003-curation
             collections.js  the roll-up view over cart collection[] (node tools/collections.js [slug] /
                             --counts / --json): "show me the road stuff / the radio stations", each with its doc
             backfill-slug.js  add the canonical `slug` (=filename stem) to every cart's de:meta + re-embed
                             de:source — the .cart.png→.c provenance anchor (design/editor-cart-workflow.md
                             Gap 1b); dry-run by default, `--write` to apply (449 carts pending)
             spec.js         run each cart's spec() — the gameplay-logic gate (twin of tune-check)
             squishy-features.js  feature×brush COVERAGE oracle for the squishy cart (renders its matrix
                             grid, pixel-diffs each cell vs baseline → flags silently-no-op features).
                             `--json`; `--selfcheck` = known-answer fixture (21 assertions over SYNTHETIC
                             grid PNGs, runs no cart) — incl. all five PNG scanline filters through its
                             hand-rolled decoder, which nothing else in the repo covers
             build-context.js  assemble a reading briefing for ONE cart: de:meta + todos + related carts +
                             every doc/note that MENTIONS it (with the line) — finds the differently-named home
             cart-outline.js a per-cart READING MAP (twin of build-context, for the SOURCE): data shapes
                             verbatim + global state + a FUNCTION INDEX (line · sig · role) + entry-point line
                             ranges — understand/navigate a 1–3k-line cart at ~1/7th the tokens of reading it.
                             --fn <name> dumps ONE function's body (no guessing the end line); --full adds macro values
             orient.js       go cold on a cart in ONE call: build-context + cart-outline back to back (the pair you
                             always want first). Flags pass through to the outline (--full / --fn <name>).
                             BARE (no cart) = the SESSION front door — starting cold or resuming? run it:
                             active handoff lanes + the repo-doctor health strip + what's pending, counts only
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
             cart-analyze.js complexity + global-state report; ranks spec-worthiness. The verdict is a
                             FALL-THROUGH chain whose ORDER is the judgement (`simple` is tested first, so a
                             tiny widget cart is simple, not reactive). `--selfcheck` = known-answer fixture,
                             23 assertions incl. the score formula recomputed from the metrics
             cart-index.js   computed technique index ("what cart teaches X") + coverage
             cart-dupes.js   cross-cart duplication finder → refactor / drift candidates. Its power is
                             NORMALIZATION: cart-local identifiers collapse to V while the engine vocab
                             (every identifier in runtime/*.h CODE — decommented, or 56% of the "vocabulary"
                             is header PROSE) stays literal, so API calls are the fingerprint. `--selfcheck`
                             = known-answer fixture (20 assertions) pinning that trick in BOTH directions
             build-compendium.js  generate docs/cart-compendium.html (--check gates staleness)
             build-design-board.js  generate docs/design-board.html — the VISUAL ★ designs page (every design
                             doc + ADR by lifecycle phase, clickable cards) from design-board.js; --check gates staleness
             build-reflections.js  generate docs/reflections.html — the VISUAL ★ reflections page (the research
                             journal by lifecycle) from build-field-notes.js's collectNotes(); --check gates staleness
             build-band-briefs.js  generate docs/band-briefs.html — the VISUAL ★ briefs page: every blind band
                             brief (docs/design/*-blind-brief.md) paired with the radio cart it became; --check gates staleness
             build-field-notes.js  GENERATE docs/field-notes/FIELD-NOTES.md — navigable index of the research
                             journal (lifecycle board / timeline / related-note graph / conformance); --check gates staleness
             build-book.js   generate docs/learn-you-a-dreamengine.html — the illustrated "Learn You a Haskell"-style
                             field guide to the studio.h API; every illustration is REAL harness output (carts in
                             tools/carts/book/, stills via play.js --dump / anims via make-gif) so it can't drift
             build-all.js    compile-check every cart vs current studio.h (catches API rot)
             build-nr.sh     build+run a cart with the DE_NO_RAYLIB software engine (no Raylib/frameworks) — the desktop twin of the iOS build (ios/)
             build-box2d.sh  build the vendored Box2D v3 (runtime/box2d/) into build/box2d/<target>/libbox2d.a per platform (--mac/--win/--wasm/--ios, --check runs a drop-box smoke test). Pure-C rigid-body lib for the physics experiment; opt-in, not in the default cart build. See docs/design/box2d-integration.md
             mac-app.sh      bundle an exported cart binary into a signed + notarized + stapled .app that opens on ANY Mac (Gatekeeper-clean); needs a Developer ID cert + a notarytool cred profile (header has the one-time setup)
             bundle-spike/   PASSED probe: TWO carts in ONE binary (per-TU -Ddraw=<slug>_draw renames + a dispatcher shim; carts unmodified) — the Tinyjam multi-cart app shape (design/share-panel.md §spike); proof-sound.sh = the de_switch_cart round-trip oracle
             engine-dylib-spike/  PASSED probe: **K independent engines in ONE process**, by loading the
                             engine as a dylib K times instead of refactoring its globals. `bash run.sh`.
                             WHY: two AUv3 instances land in one extension process (measured) and engine
                             state is process-global (~204 statics in studio.c/sound.h + every cart's own),
                             so two DAW tracks fight over one rack. dyld keys images by FILE, so two
                             COPIES of one dylib are two data segments — every static duplicated, ZERO
                             changes to studio.c/sound.h/any cart. Ships as K pre-signed copies in the
                             bundle (no runtime copy = no "may a sandboxed appex dlopen what it wrote").
                             Carries a NEGATIVE CONTROL (same path twice must come back byte-identical,
                             reproducing the defect so the main assertion can go red) + a BONUS assertion
                             that two DIFFERENT carts run side by side at their own canvas sizes — which
                             de_switch_cart cannot do. 9 MB for two engines. NOT covered: dlopen from
                             inside the sandboxed .appex, the Swift-side frame worker (one static per
                             process today), K same-named CoreMIDI virtual sources, K instances sharing
                             one cart.blob. docs/design/ios-plan.md
             sync-spike/     the two MIDI-CLOCK probes + the end-to-end gate for external sync
                             (runtime/sync.h): midimon = LISTEN, naming every transport byte on the wire ·
                             midisend = SEND a clock to the IAC bus, and `midisend <bpm> <secs>` WITHOUT
                             `start` reproduces the case that shipped broken (a DAW already playing, so the
                             cart joins mid-flow and never sees a START → tempo-only, see sync_transport).
                             `run.sh` bare = builds both + asserts the whole arc (arrive → START → run →
                             STOP → hand back) through synccheck's trace, so the REAL CoreMIDI path has a
                             gate too — the synthetic --midi-clock one can't cover it, since a deterministic
                             run ignores real MIDI by design. Needs IAC online; macOS only
             mic-spike/      SPIKE (audio-input frontier): can the engine HEAR? miniaudio mic capture → live mic_level()/mic_pitch() (Tier-1, docs/design/mic-and-sampling.md). run.sh fetches miniaudio.h + builds; CONFIRMED LIVE on Mac (webcam mic, peak −17 dBFS — level clean, zero-crossing pitch is octave-noisy)
             build-app.js    build a MULTI-CART app from apps/<name>/app.json: per-TU renames + generated dispatcher + per-cart sound/video/sheet contexts (de_switch_cart umbrella) — adding a rack = one manifest line. Bare = a native binary; --mac wraps it signed+notarized via mac-app.sh; --ios stages the set for the Xcode build (ios/device.sh|build.sh APP=<name>)
             profile-fleet.js batch CPU-profile a set of carts → which engine primitive is hottest
             status-check.js the front+back door for docs/STATUS.md, the shipped/open/cut LEDGER — the one
                             doc everything tells you to TRUST and the only one held to no standard (both
                             linters carve it out; neither can see a ledger losing sync with the repo,
                             because none of it is a broken link). Bare = the index it lacks (counts,
                             shipped newest-first, the REAL open list). `--check` = drift: a DONE marker
                             inside Open (38% of the backlog on day one), an over-long entry, an undated
                             one, Shipped out of order, numbering inversions, a bloated headline, a dead
                             "see Decided-against" pointer. ⚠ NEVER renumber to fix an inversion — ~30
                             `STATUS #N` refs across docs/tools/carts resolve today; reorder instead.
                             `--selfcheck` = the KNOWN-ANSWER fixture (tools/fixtures/status-check/): asserts
                             the checker itself, incl. regression guards for the two false-positive shapes that
                             fooled v1. Gated in repo-doctor. Copy this pattern into any linter that JUDGES
             lint-aux-params.js  the per-engine AUX PARAM channel (`instrument_mode`/`eng_p[]`) writes its
                             width in FIVE places that must agree (both `eng_p[]` decls, BOTH `idx >= N`
                             bounds — the setter AND the SR_ENG_TUNE handler — the note-on copy, and every
                             MODE_* constant + its 4-place registration). Miss one and the parameter
                             SILENTLY does nothing: the setter accepts it, queues it, the handler drops it.
                             Has bitten twice (piano decay/knock dead for months; MODE_PIANO_STRETCH read
                             as 0). `--quiet` = CI, `--json`. Run after touching instrument_mode or adding a
                             MODE_*. `--selfcheck` = known-answer fixture (14 assertions): every check here is
                             a regex over C, and THREE of the five pass VACUOUSLY on zero matches (no bounds
                             found → "all bounds equal the width"), so a rotted pattern prints the same green
                             ✓ as a healthy engine — the fixture pins those guards. Gated in repo-doctor
             lint-docs.js    validate docs/ cross-references (links resolve, §-refs) + the two
                             DISCOVERABILITY gates: every tools/* and every cart-land runtime/*.h is
                             indexed in CLAUDE.md (headers also in cart-authoring's table) — the "it
                             exists but no agent finds it, so they hand-roll it again" class.
                             `--json`; `--selfcheck` = known-answer fixture (pins the HARD-vs-SOFT §-ref split:
                             a parent-resolved ref is a NOTE, never an error), gated in repo-doctor
             lint-fxicons.js every `FX_*` insert kind must have a shared GLYPH in runtime/fxicons.h
                             (body colour + accent + name + icon). The failure is SILENT and worse than
                             blank: an unregistered kind falls through fx_icon()'s `else` and draws a
                             convincing REVERB pedal labelled "FX" — which FX_DRIVE + FX_MULTIBAND both
                             did for six weeks, while TWO carts hand-rolled a private `od_icon()` for
                             FX_DRIVE (the copy-paste fxicons.h exists to prevent). The fallback kind is
                             DERIVED from the `else` comment, not hardcoded. `--strict` gates in
                             repo-doctor; `--selfcheck` = known-answer fixture. Sits at ZERO
             lint-capability-claims.js  docs that say we CAN'T do something we now CAN — the INVERSE of
                             stale-doc-check (which finds docs citing code that once existed and is gone).
                             "missing (no reverb engine)", "not a vocoder — a future effect once the
                             sidechain lands": true when written, now a lie an agent believes and routes
                             around, hand-rolling a workaround for a shipped effect. Precision comes from
                             6 discriminators, chiefly: a capability must PROVABLY ship (a studio.h proof
                             symbol, so "no octaver" is never flagged) and a PARAGRAPH-scope acknowledgement
                             ("reverb ✓ SHIPPED") silences it — which is also the escape hatch for
                             deliberately-historical prose. ADVISORY by design (it judges prose);
                             `--selfcheck` = 26 known answers (recall AND suppression), gated in repo-doctor.
                             Roster shared with wants-check via capability-roster.js
             wants-check.js  what a `*-wants.md` doc is ACTUALLY still waiting on — the STRUCTURED twin of
                             lint-capability-claims (which reads prose and therefore has a recall ceiling: it
                             missed afrobeat entirely, because this repo denies a capability by SCHEDULING it,
                             "still open: wah, tape, leslie"). This one never reads prose. It parses the
                             `unblocked by` TABLE COLUMN the genre already writes — that column IS the
                             dependency list — and reports a want only when THREE facts hold: the doc declares
                             it blocked on X · X ships (a studio.h proof symbol) · the CART makes no call that
                             would use it. So the output is a work-list, not a nag, and a row clears itself
                             when the cart wires it. ⧗ STILL BLOCKED = the doc is telling the truth. Also
                             cross-checks the doc's own STATUS line (parked in exploring/building while every
                             want ships = stale by definition, no phrasing involved). `--todo` = actionable
                             rows only, `--strict` gates, `--selfcheck` = 11 known answers
             capability-roster.js  (lib, not CLI) the shared capability list: cap → the studio.h `proof`
                             symbol that shows we ship it → the `wire` symbols that mean a CART used it.
                             Owned here so lint-capability-claims + wants-check can't drift; same lib shape as
                             doc-status.js. Drop an API and the capability leaves both tools automatically
             lint-xrefs.js   the inverse of lint-docs: find docs that SHOULD cross-link but don't —
                             unlinked doc-name mentions + missing backlinks (A→B but not B→A). Advisory;
                             scope to a feature (`node tools/lint-xrefs.js touch`) to act on it.
                             `--json`; `--selfcheck` = known-answer fixture (both tiers + the HUB and
                             fenced-code exempt classes), gated in repo-doctor
             stale-doc-check.js  doc-freshness finder. BROKEN REFERENCES tier = a doc cites a code path
                             that ONCE EXISTED and is now gone (renamed/deleted — prose left behind by a move), or
                             a dead `tool --flag`. A never-built file and another repo's path are NOT that and are
                             suppressed with a count (`--all` lists them) — before that discriminator landed the
                             tier was 47 findings at a 0% true-positive rate, all proposals; mtime tiers = nudges (TOOL DRIFT:
                             a doc names a tool changed after it; DOC CHURN, --docs). `--driftable` = the CURATED
                             tier: docs that declare a `de:driftable` snapshot of a tool's output, flagged when the
                             tool's inputs moved after the snapshot (see docs/design/driftable-docs.md; surfaced by
                             cart-status.js). grep + git dates, no dep graph; advisory. `--all` lists what the
                             broken-refs tier SUPPRESSED (proposals / other repos' paths — it always prints the
                             count); `--selfcheck` = known-answer fixture, gated in repo-doctor
             handoff.js      the ACTIVE-LANE tracker for docs/HANDOFF.md — keeps it recent + the reliable place to
                             resume complex in-flight work. Bare = FRONT DOOR: list the ▶ ACTIVE THREAD lanes + age
                             (wired into orient.js). --check = BACK DOOR: flag lanes >2wk old / broken doc links /
                             broken #section anchors (write Resume-ats as [text](doc.md#section) so a renamed section
                             is caught; surfaced by cart-status.js). Same two-door pattern as driftable-docs.
                             `--selfcheck` = known-answer fixture (all five lane judgements + all three false
                             positives this tool once shipped; dates are TEMPLATED so it can't rot), gated
             gate-controls.js  which of our GATES can prove they are able to FAIL? A gate never seen to go
                             red is indistinguishable from one gone BLIND, and nothing tells you which — so
                             this counts the ones carrying evidence (a `--selfcheck` known-answer fixture, or
                             a NEGATIVE CONTROL / `-bypass` that must fail). `--list` = the work-list ·
                             `--excluded` = what it dropped · `--quiet` = the repo-doctor row. ADVISORY: a
                             missing control is a QUESTION, not a defect (loud failures need none) — spend one
                             where PASS is the steady state and failure is SILENT (timing, threading,
                             suppressing guards). Born from three assertions that were green while the thing
                             they named was broken, each differently: a pair-counter blind to TIMING, a guard
                             inert inside an `#ifdef`, and a flush that won a RACE. Two precision rules it
                             learned: a control must be STRUCTURAL (the word "control" in prose does not
                             count) and exiting nonzero on BAD USAGE is not judging (that draft reported 60
                             "gates", half of them generators). `--selfcheck` = 11 known answers, mutation-
                             tested. docs/guides/checks-and-oracles.md → "The OTHER way a green check lies"
             repo-doctor.js  ONE health strip over every meta-check (lint-docs/lint-carts + the build-* --check
                             staleness gates as ✗, the advisory linters as ⚠) run in parallel, counts only —
                             listings via --full or the named tool itself; embedded in bare orient. --quiet = CI
             gen-tcc-symbols.js   regenerate runtime/studio_tcc_symbols.h from studio.h (libtcc)
             build-history.js     generate docs/history.html from git + the spine; --check = spine edited after page
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
