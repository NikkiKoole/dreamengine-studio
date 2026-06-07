# docs/ — map & organizing rule

These docs are organized by **genre and durability**, not by topic — because the mess
they replaced came from one file trying to be reference *and* status *and* rationale at
once, so part of it was always stale. Each file now answers one kind of question.

> **The rule.** **Status** lives only in `STATUS.md`. **Settled rationale** lives only in
> `decisions/`. **Vision/philosophy** lives only in `VISION.md`. **Exploratory design**
> lives in `design/`. Docs *cross-reference*; they don't *duplicate*. If two docs say the
> same thing, one is wrong — fix it at the source and link.

## Layout

```
docs/
├── README.md          you are here — the map
├── VISION.md          why & what: product idea, audience, philosophy, console spec
├── STATUS.md          state: the one ledger of shipped / open / cut
├── POLISH_TODO.md     work list: the per-game "juice pass" across the carts
├── HANDOFF.md         narrative + environment gotchas (not obvious from code/git)
├── decisions/         frozen, dated decisions (ADR-lite) — the "why we (didn't) do X"
├── design/            exploratory design notes (scratchpads) — rationale + proposals
│   ├── api-notes.md     engine API: classics survey, signatures, naming, cart-patterns
│   ├── cart-as-script.md  EXPLORATION: run cart C without an external compiler via libtcc (in-memory compile + hot-reload); the 3-symbol host↔cart boundary, why Wasmer clang/WASIX won't do graphics, native goal vs. browser goal
│   ├── audio-notes.md   sound: current engine, chip comparison, expansion roadmap (incl. §8.9 candidate engine catalog, §12 instrument gaps, §14 Roland machine readiness)
│   ├── held-notes.md    SHIPPED spec: note_on/note_off handles + live note_pitch/vol/cutoff/duty + slew — the sustain layer the instruments ride on
│   ├── input-recording-looper.md  exploration: can a cart record the player and loop it back — recording levels for music looping + gameplay ghosts
│   ├── modular-synth.md  modrack design spec: Eurorack-style control-rate patcher cart (modules, cables, data model, shipped ledger)
│   ├── tinydaws.md      IDEA: ReBirth-style genre racks (generate → play → export) — radios as song generators feeding editable sequencer carts; the lane format, seed-as-song-code handoff, song.h export for game soundtracks, the 8-rack genre table (incl. rebirth-classic, the RB-338 homage itself) + the wider map (unbuilt stations × engine wishlist, cross-indexed from game-music/audio-notes)
│   ├── blend-tables.md  exploration behind STATUS item 18: index-only translucency/glow/shadow; cart-space prototype = blendlab.c
│   ├── palette-and-color.md  exploration: own palette vs the borrowed PICO-8 32 — one decision wearing two questions, sequenced before blend tables bake it in
│   ├── physics-notes.md  brainstorm: a tiny in-engine physics layer? (sparked by ragdoll.c; nothing decided)
│   ├── probe-carts.md   LEDGER (kept current): which cart was built to answer which API question, and the verdicts
│   ├── cart-library-direction.md  snapshot of the ~201-cart shelf + opinionated "what carts to build next" (tutorial on-ramp, more toys, the few missing teachable games; NOT more clones)
│   ├── tutorial-curriculum.md  plan for the next wave of tutorial carts (25+): the language-fundamentals / collision / whole-game-capstone tracks, adapted from the Nerdy Teachers PICO-8 course (adapt the arc, rewrite in C)
│   ├── cart-survey-api-priorities.md   cart-evidence-first memo: what real carts prove, filtered through existing decisions
│   ├── api-usage-audit.md   which API functions the carts actually use (least-used tail, the 11 never-used, doc-coverage gaps); re-runnable via `node tools/api-usage.js`
│   ├── baked-rotation-atlas.md   pre-rotated sprite/shape atlas: fast blitted bodies, the centerline model, offscreen canvases
│   ├── font-rendering.md         playful text beyond print: shadow/outline/wave/typewriter, inline codes, PICO-8 comparison; + 2 baked tiny fonts (3×5/4×6) awaiting print_small/print_tiny wiring
│   ├── rasterization-consistency.md  OPEN: one consistent fill/outline/dither rule so shape edges always agree (seams, off-by-1, outline mismatch)
│   ├── geometry-helpers.md  PROPOSAL: graphics taxonomy (which carts draw with circles/tris/dither/sprites/gradients) + 2D drawing primitives for the geometry-first style (ngon/star, poly/polyfill, gradient, rounded-rect, thick-line) ranked by cart boilerplate; also parks a thought on real palette color-lerp (lerp_color/rgb true-color) with the second-thoughts against it
│   ├── galerijflat.md   IN PROGRESS (multi-session): design seed for an experimental/arty cart — the Dutch gallery-access apartment slab as a generative tableau of ordinary life (gallery facade straight-on, the archetype; the facade as a pixel grid of households)
│   ├── headless-autoplay.md  growing the debug harness toward windowless fast runs + agents that play to find bugs (navkit-informed)
│   ├── multiplayer-research.md  RESEARCH: netplay for native C + wasm — WebRTC DataChannels (libdatachannel/datachannel-wasm) as the one cross-target transport, input-lockstep reusing the --det harness, join-code UX + LAN discovery, iPad path, friendly net_host()/net_join() API sketch, staged plan
│   ├── touch-notes.md  multitouch: the touch API + identity model, the release API (tapr/touch_ended_*, shipped with the touch piano), the never-wrap-rgestures verdict + gestures.h, the touchlab on-device checklist, §7: the web phantom-touch fix (DOM mirror, DEVICE-PASSED), and device finding #5 (iPhone's 6th finger mass-cancels ALL touches)
│   ├── mobile-web-notes.md  desktop carts meeting phones: device findings (5-touch iPhone ceiling, tiny text), mobile-lint + gallery badges, screen-fit policy, fullscreen/PWA install, and §5: the device-facts trio proposal (touch_available/device_class/touch_ceiling + the ceiling-safeguard pattern, iPad-pretends-to-be-a-Mac detection traps)
│   ├── editor-cart-workflow.md  the hand-editing gaps: no save-in-place, the sprite story (two sprite sources of truth, STATUS item 23), gallery metadata unreadable/uneditable — sliced proposals + priority table
│   ├── ui-widgets-notes.md  PROPOSAL: runtime/ui.h cart-land widget kit (button/slider/knob/panel/drag-from) that works for mouse + touch + keyboard/gamepad — capture model, value-address identity, focus ring; second customer = the sfx generator's 17 sliders
│   ├── profiler-extensions.md  PARKED: first-frame/init timing + region "zone" timers (extend the in-engine profiler; reuse perf.json)
│   └── packaging-distribution.md  NOT STARTED: shipping to non-dev machines — the dev icon/name stopgap, what real packaging needs (bundler/signing/notarization), and the real blocker (the ▶ run model needs a compiler; web/wasm is the likely public path)
├── guides/            how-to
│   ├── cart-authoring.md         the make-cart.js / tools/carts toolchain
│   ├── cart-authoring-prompt.md  reusable AI prompt for designing a new cart
│   ├── AGENT-MESSAGE.md          parallel-build add-on: paste into each agent session alongside its brief + spec
│   ├── BATCH-2.md                batch-2 cart index (16 games built via Workflow)
│   ├── BATCH-3.md                batch-3 cart index
│   ├── cart-specs-index.md       batch-1 spec index + assembly recipe + parallel-build workflow
│   ├── cart-specs/               per-game cart specs (individual game files, not shown in sidebar)
│   ├── sharing.md                ways to publish finished carts
│   ├── debug-harness.md          deterministic record/replay/script + trace + live on-demand inspect + WAV capture/analysis (tools/play.js, tools/wav-analyze.js)
│   ├── profiler.md               one-click CPU profiler (⏱ profile button): hot functions + call paths, frame budget, draw-call counts
│   ├── live-coding.md            how-to for the live (libtcc) run mode: edit-while-running, hot reload, STATE/S state that survives reloads
│   └── game-music.md             generative soundtrack recipes: step clock, chord brain, voice leading, time feel, seeds; worked example = bossa.c
└── archive/           superseded / done notes, kept for history
```

## Which file does what

| Genre | File(s) | Changes… | Never holds |
|---|---|---|---|
| **Why/what** | `VISION.md` | rarely (direction shifts) | a roadmap or status (→ STATUS) |
| **State** | `STATUS.md` | constantly | design rationale (→ design/ or decisions/) |
| **Work list** | `POLISH_TODO.md` | as carts get polished | engine/API status (→ STATUS) |
| **Decisions** | `decisions/NNNN-*.md` | append-only (supersede, don't edit) | open questions (those are exploration) |
| **Exploration** | `design/*.md` | freely; it's a scratchpad | whether a thing shipped (→ STATUS) |
| **How-to** | `guides/*.md` | when the workflow changes | — |
| **Narrative** | `HANDOFF.md` | each work session | the roadmap (→ STATUS) |

The **engine API reference itself** is not in `docs/` — it lives in the code:
`runtime/studio.h` (declarations) and `editor/src/studioDocs.js` (the bilingual one-liners
that drive autocomplete/hover/help). Don't maintain a second copy of the function list in
prose — link to those. (This is why the old "~100 functions" counts kept going stale.)

## When you change something

- Shipped / cut / deferred a feature? → update **`STATUS.md`**, then the relevant design note.
- A choice is now *settled* (especially a "no")? → write a **`decisions/`** ADR-lite and link to it.
- Proposing new API? → **`design/api-notes.md`** (sound → **`design/audio-notes.md`**).
- New **audio idea**? Route by kind: a cart to build → `design/cart-library-direction.md` ·
  an engine/synthesis want → `audio-notes.md` (§8.9 engines, §12 gaps) · a musical style →
  `guides/game-music.md` style cheat-sheet · genre racks / generate-play-export →
  `design/tinydaws.md` · a per-station timbre swap → `design/radio-instrument-options.md`.
- Product direction or a core principle changed? → **`VISION.md`**.
- A doc went stale or started duplicating another? → prune it. This README is the contract.
