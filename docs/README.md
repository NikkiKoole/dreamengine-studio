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
│   ├── cpu-shaders.md   CPU "fragment shaders" (pset_rgb/pget_rgb): the shipped teaching trilogy (shadelab → caustics → raymarch) + shadermath.h, and the parked ideas (#3 make per-pixel cost visible, #5 a true offscreen buffer for multipass)
│   ├── cart-as-script.md  EXPLORATION: run cart C without an external compiler via libtcc (in-memory compile + hot-reload); the 3-symbol host↔cart boundary, why Wasmer clang/WASIX won't do graphics, native goal vs. browser goal
│   ├── audio-notes.md   sound HUB: current surface map (§2), parameter placement (§4), §12 instrument gaps, §13 SCWs-vs-engines, §14 Roland machine readiness — tops with the audio-family index ("where does an audio idea go?")
│   ├── instrument-engines.md  the navkit rich-instrument port program (was audio-notes §8 — numbering preserved): 3-macro discipline §8.1.1, phase ordering §8.5, shipping playbook §8.8.2, §8.9 candidate engine catalog, §8.10 effects layer (+ §8.10.1 PARKED interim items)
│   ├── audio-timing.md   EXPLORATION: why web playback feels "drunk"/drifts — the beat clock advances by per-frame dt (frame-quantized triggers), web's dt is jittery AND was unclamped so hitches lurch the sequence & drop beats; the clamp fix + the schedule-ahead (radio.h) vs frame-triggered (every()) split + the sample-clock cure
│   ├── audio-threading.md  SPEC: the game-thread/audio-thread model — the lock-free SPSC queue (already in sound.h), who runs the audio thread per platform (native/web-ScriptProcessor/web-worklet/iOS/Switch), the atomics-vs-volatile correctness tax, web's SharedArrayBuffer/COOP-COEP wrinkle, the runtime backend pick (worklet when isolated else ScriptProcessor), and the staged build plan
│   ├── stereo.md         SPEC (design call ahead of build): resolves §9 "Stereo?" — instrument_pan/note_pan + LFO_PAN, linear pan law (center byte-identical), echo mono in v1; the §8.10 effects-layer prerequisite + a pre-flight bite checklist
│   ├── spatial.md        positional audio (OpenAL-style listener + sources): v1 per-voice (listener/note_pos/note_motion/hit_at) + v2 emitter buses (instrument_pos/_motion — a whole effected source moves) both SHIPPED; v3 acoustic zones (inside/outside/occlusion/materials) PROPOSED. Showcase cart = spatial
│   ├── future-stations.md  the radio-station candidates parking lot (was game-music's "Future stations"): ranked candidates, the citypop conditions, new-brains-per-cart axis, IDM + functional-music wings, the "newest demand" scoring
│   ├── radio-genre-fidelity.md  gap ledger: each station's genre-true IDEAL (researched blind) vs what we built — the holes (faked/missing instruments where a real engine sits unused, the generative brains we lack), aggregated + per-station
│   ├── sound-next-steps.md  the actionable digest of the 2026-06-10 sound research: prioritized upgrade-wirings, new instruments/stations to build, new generative brains, and decisions pending — each linking to the doc with detail
│   ├── effects-bus-architecture.md  detail map for 3 parked routing increments: reorderable per-bus inserts (A), multi-reverb sparse tank pool (B), reverb-as-bus-insert (C) — sketches + cost ledger; "the engine is already a mixing console"
│   ├── radio-instrument-options.md  per-station timbre swaps / engine retrofits, ranked per engine (mallet → lowend's vibraphone, etc.)
│   ├── held-notes.md    SHIPPED spec: note_on/note_off handles + live note_pitch/vol/cutoff/duty + slew — the sustain layer the instruments ride on
│   ├── input-recording-looper.md  exploration: can a cart record the player and loop it back — recording levels for music looping + gameplay ghosts
│   ├── modular-synth.md  modrack design spec: Eurorack-style control-rate patcher cart (modules, cables, data model, shipped ledger)
│   ├── product-notes.md IDEA: the audio line as a (maybe) product — market thesis (Pocket Operators / Koala precedents), sketch-first decision, the parked builder items (MIDI tiers, AUv3/Link, native iOS) each with cost + un-park trigger, the web-latency measurement plan, the Roland-trademark paywall rule
│   ├── tinydaws.md      IDEA: ReBirth-style genre racks (generate → play → export) — radios as song generators feeding editable sequencer carts; the lane format, seed-as-song-code handoff, song.h export for game soundtracks, the 8-rack genre table (incl. rebirth-classic, the RB-338 homage itself) + the wider map (unbuilt stations × engine wishlist, cross-indexed from future-stations/instrument-engines)
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
│   ├── second-north-star.md  REFLECTION (philosophy companion to VISION.md): the honest, adult pursuit grown alongside "learn C, make things" — deep simulations behind a humble lo-fi/SNES surface; the legendary-carts thesis (one honest core, the second face); audio as the same soul at scale; why the combination is load-bearing; the honest tensions/forks
│   ├── galerijflat.md   IN PROGRESS (multi-session): design seed for an experimental/arty cart — the Dutch gallery-access apartment slab as a generative tableau of ordinary life (gallery facade straight-on, the archetype; the facade as a pixel grid of households)
│   ├── procgen-places.md  PROPOSAL: vary procplaces' roads-&-cities generator — split the one density field into two orthogonal fields (land-use R/C/I+G × intensity light/med/heavy, SimCity 2000 model), road-class-by-hierarchy, domain-warp to de-grid; the draw/road_at() shared-geometry rule that keeps sloop safe; v1/v1.5/v2 scope (arced roads = the v2 wall)
│   ├── roadnet.md  IN PROGRESS (cart roadnet.c): the infinite/deterministic SPLINE road world that breaks procgen-places' "arced roads" wall (node-lattice splines are local) — ranked hub/town hierarchy, terrain-aware bend/bridge, + the L2 street level in the magnifier (blocks/lots, grid aligned to arterials, class-aware carved roads, outskirts gradient + Voronoi fields) exposed via the road_at() query seam
│   ├── roadnet-handoff.md  ★ START HERE for roadnet — orientation: what's built, the LOD stack (L0 map → L2 street → L3 to come), commit trail, key invariants, and the recommended NEXT-TIME order
│   ├── roadnet-streetlevel.md  DESIGN SEED: L3, the on-the-street level (the missing access-street tier + building footprints, where sloop will drive) — the seams that keep it deterministic + the deeper "BLOCK" loupe harness; has a "starting point for next session" note
│   ├── menigte.md  EXPERIMENT (v1 cart shipped): the SCALE axis of "drive a large world full of meaningful NPCs" — 10k people, all always-live, because state is a pure-function-of-(identity,time) closed-form schedule (cheap, all 10k/frame) while only the on-screen few pay for walk-anim/steering; no hydrate/dehydrate; the HOT-tracks-closed-form invariant (the procplaces shared-geometry rule, one level up); companion to procplaces (world) + sloop (car)
│   ├── headless-autoplay.md  growing the debug harness toward windowless fast runs + agents that play to find bugs (navkit-informed)
│   ├── multiplayer-research.md  RESEARCH: netplay for native C + wasm — WebRTC DataChannels (libdatachannel/datachannel-wasm) as the one cross-target transport, input-lockstep reusing the --det harness, join-code UX + LAN discovery, iPad path, friendly net_host()/net_join() API sketch, staged plan
│   ├── multitouch-and-generalization-audit.md  AUDIT (2026-06-13): all 85 instrument carts swept for mono-vs-poly, which carts read only one touch but shouldn't (upright bass double-stops, tr909 XY pad, glassharmonica chords…), and the hand-rolled plumbing ripe for a shared header — top win `runtime/pointer.h` (the 10-finger Ptr pool copied into 15+ carts)
│   ├── touch-notes.md  multitouch: the touch API + identity model, the release API (tapr/touch_ended_*, shipped with the touch piano), the never-wrap-rgestures verdict + gestures.h, the touchlab on-device checklist, §7: the web phantom-touch fix (DOM mirror, DEVICE-PASSED), and device finding #5 (iPhone's 6th finger mass-cancels ALL touches)
│   ├── mobile-web-notes.md  desktop carts meeting phones: device findings (5-touch iPhone ceiling, tiny text), mobile-lint + gallery badges, screen-fit policy, fullscreen/PWA install, §4c responsive display (resizable window / scale-to-fit / rotation), §5: the device-facts trio proposal (touch_available/device_class/touch_ceiling + the ceiling-safeguard pattern, iPad-pretends-to-be-a-Mac detection traps), and §6b the keycap (tappable-key-label) proposal
│   ├── keycap-audit.md  AUDIT (2026-06-13): all 348 carts swept for mobile-touch readiness, centered on the keycap pattern (mobile-web-notes §6b) — per-cart verdict (touchControls-flag 114 / keycap 100 with 331 extracted chips / none 98 / touch-first-redesign 18 / wontfix-desktop 18); top win = one `radio.h` change makes 24 radios' R/[/]/M tappable at once. Re-runnable via the audit workflow
│   ├── responsive-layout.md  BRAINSTORM: a four-primitive layout set (flex / fluid clamp+% / anchor+inset / aspect) for the hard "layout-responsive" half of mobile-web-notes §4c — prototyped in cart-land first (tools/carts/respond.c: drag a fake screen's corner, watch it reflow) before SCREEN_W/H ever become runtime vars; candidate runtime/lay.h API + graduation path
│   ├── frame-pacing.md     EXPLORATION: keeping phones cool — why a pinned 60fps cooks the device, Lever A (per-cart fps cap; the web emscripten_set_main_loop gotcha), Lever B (on-demand/reactive rendering + repaint(), the pause-freeze precedent, why not frame-hashing); the two compose
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
│   ├── radio-station-howto.md    ★ START HERE to build a new radio station: the ordered runbook (firewall → design blind → shop palette → chassis → brains → build → register), a spine that links each deep doc at the right step
│   ├── instrument-carts.md       the sound-toy shelf: every synth/machine/station/showcase cart, grouped by the building block it copies (radio.h / held-notes / ui.h / INSTR_*) — browse one to play, or find the closest relative when building a new one
│   ├── instrument-recipes.md     the supply-side palette: every grabbable instrument recipe across the sound carts (showcase presets, Roland drum voices, whimsical patches), organized by ENGINE (107 recipes); the "which recipe" companion to instrument-carts.md's "which cart"
│   ├── effects-recipes.md        the effects cookbook: named, copy-paste settings for every engine EFFECT (echo/reverb/chorus/flanger/tape/wah/drive[+modes]/bitcrush) with the showcase cart + used-by list — the effects analog of instrument-recipes.md
│   ├── instrument-presets.md     the named-patch catalog: every radio station's voice recipes given clear names, with sharing tiers (shared/variant/cousin/kin) so copy-propagation across stations is visible; paired with radio-voices.md
│   ├── porting-from-navkit.md    how to faithfully port an instrument/effect from the navkit sibling synth: port the oscillator VERBATIM (oscillator-first, not piecemeal), the gotchas (velocity×2, the ratio blend, amp normalize, always-on filter, drive-scale mismatch), macro mapping, the clav worked example + a checklist
│   ├── radio-voices.md           per-station voice charts (slot → role → preset name) + the findings summary (reuse clusters by idiom, the synth-kit/bass extract candidates, the 5 showcase-cart preset lineages, upgrade candidates); the reading view over instrument-presets.md
│   ├── sharing.md                ways to publish finished carts
│   ├── debug-harness.md          deterministic record/replay/script + trace + live on-demand inspect + WAV capture/analysis + UI audit, off-screen text/overlap/hidden-panel finder (tools/play.js, tools/wav-analyze.js, tools/ui-audit.js)
│   ├── profiler.md               one-click CPU profiler (⏱ profile button): hot functions + call paths, frame budget, draw-call counts
│   ├── live-coding.md            how-to for the live (libtcc) run mode: edit-while-running, hot reload, STATE/S state that survives reloads
│   └── game-music.md             generative soundtrack recipes: step clock, chord brain, voice leading, time feel, seeds, brain catalog, radio.h; worked example = bossa.c (station candidates live in design/future-stations.md)
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
- New **audio idea**? → the **family index at the top of `design/audio-notes.md`** routes it
  (engines, stations, styles, racks, retrofits — one table). Rule of thumb: an audio
  investigation longer than ~a screen gets its own `design/` file + a one-line pointer,
  never a new § appended to the hub.
- Product direction or a core principle changed? → **`VISION.md`**.
- Splitting a section out of a grown doc? **If other files reference its §-numbers widely,
  keep the numbers verbatim in the new doc + leave a stub at the old spot**
  (`instrument-engines.md` is the worked example); otherwise just renumber. The kept
  numbering exists *only* to serve live references — once those age out, renumbering the
  doc is ordinary cleanup, not a rule violation.
- Moved / split / renamed any doc? → **`node tools/lint-docs.js`** — verifies every
  relative `.md` link and doc-qualified §-reference (stub/parent resolutions show as
  soft notes; bare §-refs are deliberately unchecked). It caught 7 broken links from a
  2026-06 file move on its first run; trust it over grep.
- A doc went stale or started duplicating another? → prune it. This README is the contract.
