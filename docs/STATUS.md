# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-06-04 eighth pass (**API usage audit + `music()` cut** — `tools/api-usage.js` cross-checks all 182 API functions against all 233 carts (+ the four-places doc coverage); findings in [`design/api-usage-audit.md`](design/api-usage-audit.md): 11 functions never used, `paused()` doc gap closed. Biggest finding acted on: **`music()` cut** ([decision 0013](decisions/0013-cut-music-api.md)) — zero cart users, patterns were never cart-authorable (only the hardcoded bass+hihat demo), and the project's real music path is the generative beat clock. `Pattern`/`music_bank`/`from_music` machinery removed from `sound.h` (voice stealing simplifies to free → non-held → voice 0); `sfx()` stays — its living role is `sfx(-1)` "silence ringing sounds" (8 of 11 cart call sites) + 6 first-contact demo slots. Soundcheck tripwire: PASS. Same-day follow-up: **three more zero-adoption helpers cut** — `bezier_cubic`/`anim_once`/`bounce_at_edges`, each the unloved sibling of an adopted helper ([decision 0014](decisions/0014-cut-unused-convenience-helpers.md))). Prior: seventh pass (**sound reliability sweep** — (1) the silent-drop bug class is now LOUD: the engine counts dropped requests (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED`; found via the `wave_set` queue flood that made every modrack wav position play square (fix: wave_set packs 4 samples/request, queue 256→512). (2) **`soundcheck` cart** — the sound-engine self-test: worst-case define burst + full-API walk + a 40-shot `schedule_hit` burst; PASS = silent log + 9 audibly distinct waves; run it after touching `sound.h` — see [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire". (3) **modrack: drawn user waves** — wav knob 5→9 positions (`org/vox/bel/fld` baked via `wave_set`); `SOUND_INSTR_SLOTS` 16→32. (4) **modrack knob-index enums** (`VK_*`/`BK_*`) replacing raw param numbers — the rename-by-intent flushed six more preset cross-wires left over from the flt-knob insertion; a reordered knob list now fails at the compiler instead of silently cross-wiring). Prior: sixth pass (**per-cart save folders** — new `--save-dir` runtime flag; the editor (all three spawn paths: run / profile / live host) and `play.js` pass `saves/<cart>`, so `cart.sav`/`cart.kv`/`cart.blob` live under `build/saves/<cart>/` instead of one shared set in `build/` — a fresh cart can no longer inherit another cart's hiscore. Same isolation idea as `build/.bake/<name>`, but for persistence. **monster mix lab cart** — the `sprite-draw.js` `stamp()` showcase: 9 drawn parts composited into all 27 combos at bake time, ×4 `pal()` recolor schemes = 108 monsters from 9 parts; gameplay: assemble the customer's order and piston-stamp it). Prior: fifth pass (**sfx editor: per-step filter CUT lane + RES slider** — a third lane after pitch/volume: per-step lowpass (top = wide open / FILTER_OFF, exponential 150Hz–4.8kHz) for wah-plucks and opening sweeps. Implementation note: scheduled-ahead steps each carry their settings in a rotating instrument slot (10–15), since defines apply immediately but hits fire later; the export emits the slot-rotating player only when the filter is used). Prior: fourth pass (**profiler always-on in native builds** — `PROF()` counters + frame timing now compiled into every native build, not just `-DDE_PROFILE`; `profiler_request` trigger file lets you snapshot `perf.json` from any running cart the same way `screenshot_request`/`state_request` do. **Release build mode** — settings → run mode → "release" compiles with `-O2 -DDE_RELEASE`, stripping `PROF()`, timing, and `harness_inspect` polling). Prior: third pass (**live inspection shipped** — on-demand screenshot + state snapshot via trigger files (`build/.bake/screenshot_request` / `build/.bake/state_request`); handshake: game captures on next frame tick and deletes request. Before/after pairs work across a live run. See [`guides/debug-harness.md`](guides/debug-harness.md)). Prior: second pass (**sound tooling sprint**: `schedule_hit` shipped (delay+duration note — sub-frame sfx steps); `wave_set` + `INSTR_USER0..3` shipped (drawable single-cycle waves, §8.4 partially resolved); three sound-tool carts shipped — `sfx editor`, `sfx generator` (sfxr categories + mutate), `wave editor` (live-morph drone) — all exporting paste-ready C, validating the §5.6 "editor cart, no engine banks" direction). Earlier same day (**sound: mod-envelopes SHIPPED** — `instrument_env`/`note_env` (ENV_CUTOFF/PITCH/DUTY, kinds 18/19) + demo carts `filterenv`/`pitchenv`, wired into modrack (onboard fenv/penv, VCA `a` jack + flat bank, independent clocks, crisp zoom, scaling cables) and the dream synth (AMP/FILTER/PITCH envelope tabs). audio-notes §2 refreshed as the current surface map (32 fns + 32 consts, the 3-way mod matrix); §9 resolved entries struck (handles won, per-instrument filter, duty placement). **SFX authoring direction:** prototype as a PICO-8-style editor cart, zero new engine API (§5.6). Prior 2026-06-03: **modulation envelopes decided as the next feature**, built before the navkit instrument engines — a routable second EG (`instrument_env`, dests `ENV_CUTOFF`/`ENV_PITCH`), the one-shot twin of the LFO; surfaced ear-testing navkit's pluck (filter-env + pitch-env are one primitive). New audio-notes §11; item #5 reordered. Prior same day: Picotron API comparison — added four ideas: `menuitem` folded into the Pause item (#4 — same feature, two ends), frame-spanning **sequence scripts** (#17, kept distinct from the cut DIV process model), **blend tables** (#18 — index-only translucency/fog/additive, the real capability gap), and a **userdata/offscreen-buffer reframe** folded into the rotation-atlas item (#13 — `sset`/canvas/rotation-cache are one general primitive). Sound comparison corrected: Picotron's audio is a deep node-graph synth + tracker, so the real distinction is code-first vs GUI, not depth). Prior: 2026-06-02 (session 14 — `fps()` shipped as the perf read-out; **one-click profiler shipped** (⏱ profile button, see [`guides/profiler.md`](guides/profiler.md)); **off-screen poly bbox clamp shipped** (item 14) — a cliff guard, ~17× on the synthetic stress cart, modest on real carts; `trifill_stress` regression cart added). Prior: session 13 — `fade()` made immediate-mode, fixing a 27-cart stuck-dim bug._

---

## Shipped ✓

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help),
  sprite editor, map editor — all in one PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — 20 numbered tutorial carts + ~80 example game carts (~100 total).
- **`sprite-draw.js` post-processing batch + `foundry.cart.png`** (2026-06-04) — five new ops for programmatic `.cart.js` sprites: `shade()` (auto light/shadow via the curated `RAMP_DARKER`/`RAMP_LIGHTER` palette LUTs — *the* "one step darker/lighter" table for the fixed palette), `rotate()`/`rotations()` (baked headings, still post-processable unlike runtime `spr_rot`), `scale2x()` (EPX: sketch 16×16, bake 32×32), `replace()`/`clone()` (bake-time variants); `split()` now grid-splits a 32×32 into 4 slots as its comment always claimed. Showcase: **SPRITE FOUNDRY** — "watch the code draw": `foundry.cart.js` snapshots the canvas into the next slot after every drawing step, and the cart plays each subject's time-lapse back with the code line per frame (dragon → `shade()`, ship → `mirror()`+`rotations()`, boss → `noise()`/`replace()`/`scale2x()`). Tutorial 15 (animation phase) also rebuilt on the library: its 6-frame walk cycle is one parametric `walker(t)` sampled over the stride. See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "sprite-draw.js".
- **`ragdoll.cart.png`** — Verlet physics sandbox. Up to 50 stick-figure ragdolls hop autonomously across the screen, bounce off each other and off rolling balls. Grab + throw with mouse (whole-body drag), right-click to spawn balls, C to spawn characters, R to reset. Key techniques: Störmer-Verlet integration, distance constraints (12 sticks/character, 8 iterations), position springs chained bottom-up (feet → knees → hip → chest → head), angular springs per bone with 90° guard (cross-product direction inverts past 90°), per-character standing/ragdoll timer that only recovers when feet are on the floor, hop impulse that immediately releases the foot pin so it isn't erased, broad-phase character collision (hip-to-hip > 40px early-out then 12×12 point pairs with velocity impulse). Debug session used the play.js harness + DE_TRACE watches to trace `rtimer`, `whop`, `hip_y`, `knee_y` and diagnose: rest lengths mismatched standing pose (hip-knee was 9, actual √73 ≈ 8.54 → knee pushed to floor); hop velocity erased by foot pre-pin each frame (fixed by setting `rtimer=0` inside `hop_tick` and re-reading `sk`). See `tools/carts/ragdoll.c`.
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- **Live (libtcc) backend + hot reload** — a "run mode" toggle (settings) switches ▶ run from the clang static build to a persistent `-DDE_TCC` host that JIT-compiles the cart in-process via vendored `runtime/libtcc/`. Editing the code auto-reloads it (debounced, no Run press) without restarting the window; compile errors mark the line and keep the last good cart running. State survives reloads via **`de_state()`** — promoted to a first-class `studio.h` API and fronted by the starter cart's friendly `STATE { ... }; / S->field` sugar (clickable to help). arm64-macOS only; sprite/screen changes relaunch. Full record + rationale: [`design/cart-as-script.md`](design/cart-as-script.md).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).
- **Live inspection** — on-demand screenshot + state snapshot while a cart runs. Write the desired output path into `build/.bake/screenshot_request` or `build/.bake/state_request`; the game captures the current frame on its next tick and deletes the request file as the handshake (gone = fresh, ready to read). Lets you bracket a specific moment: capture before + capture after = instant diff without a filmstrip. Works alongside `play.js run --headless` and any other harness mode. See [`guides/debug-harness.md` → Live inspection](guides/debug-harness.md).
- **Profiler** — one-click `⏱ profile` button (hidden behind a settings toggle). Compiles a profiling build (`-O1 -fno-inline -DDE_PROFILE`), runs the cart ~4s, and reports into the build log: frame CPU budget (ms vs the 16.6ms 60fps target), hottest functions **with the call paths that reach them** (macOS `sample` call-graph attribution, rolled up to `studio.h` primitives), and exact per-frame draw-call counts (in-engine `PROF` counters, re-entrancy-guarded). Behind the scenes — no Instruments GUI. macOS-only (uses the `sample` CLI). **`PROF` counters + frame timing are now always on in native builds** (not just `-DDE_PROFILE`) — `perf.json` is written every 30 frames in any normal run; snapshot it on demand with `profiler_request` (same trigger-file pattern as `screenshot_request`). `-DDE_RELEASE` strips all overhead (new settings toggle, see below). See [`guides/profiler.md`](guides/profiler.md).
- **Release build mode** — settings → run mode → "build mode" select. `normal` (default): profiler counters + `harness_inspect` polling on, `-Os`. `release`: `-O2 -DDE_RELEASE` — strips `PROF()`, timing measurement, and all per-frame trigger-file probes. For when you want to benchmark or ship without any instrumentation overhead.
- **Per-cart save folders** — `save()`/`save_int()`/`save_bytes()` files (`cart.sav`/`cart.kv`/`cart.blob`) live in `build/saves/<cart>/`, not one shared set in `build/`. Runtime takes `--save-dir DIR` (any native build, default cwd); the editor slugs `cartName` and `play.js` uses the cart's file stem, so editor saves and harness saves are separate folders per cart — a scripted test run can't clobber a real hiscore. Web build unchanged (no argv). See [`guides/debug-harness.md`](guides/debug-harness.md) flags table.
- **`monstermix.cart.png`** — the `sprite-draw.js` `stamp()` showcase cart. The `.cart.js` draws 9 parts (3 heads, 3 bodies, 3 legs, `mirror()`ed) and `stamp()`-composites all 27 combos into slots at bake time; magic `pal()` indices 28/29 recolor them into 4 schemes at draw time — 108 monsters from 9 parts. Also exercises `split()` (32-wide machine), concave `polyfill` (star), `noise()` tiles, `outlined()` with a custom outline color. Gameplay: assemble the customer's order, piston-stamp it (squash + dust + shake), combo chords climb with the streak. See `tools/carts/monstermix.c` / `.cart.js`.
- **`tools/sprite-draw.js`** — shared programmatic sprite-authoring library for `.cart.js` files. Exports a 2D pixel-canvas API aligned with the C drawing API names: `blank`, `pixel`, `rectfill`, `rrectfill`, `line`, `circlefill`, `ovalfill`, `trifill`, `polyfill`, `ngonfill`, `noise`, `outlined`, `mirror`, `stamp`, `flat`, `split`, `OUT`. All 14 programmatic `.cart.js` files `require('../sprite-draw.js')`. Three `.cart.js` authoring styles documented: (1) settings-only, (2) ASCII art with `DEFAULT_CHAR_MAP` (palette 0–15), (3) programmatic arrays via sprite-draw (palette 0–31, geometry). See `tools/sprite-draw.js` and `CLAUDE.md` → project structure.

**API surface** — ~125 functions + ~90 constants in `runtime/studio.h`.
For the full grouped inventory see [`design/api-notes.md` → "What dreamengine has today"](design/api-notes.md).
Recently landed and worth calling out:
- Juice batch: `pal`/`pal_reset`, `fade`, `shake`, `print_scaled`, `text_width`.
- **Font system:** `font(FONT_NORMAL/FONT_SMALL/FONT_TINY)` state setter; two extra fonts baked (`font4x6.png` ~64 chars/320px, `font3x5.png` ~80 chars/320px); `print_shadow`, `print_outline`; all `print*` functions now return x-after-last-char for chaining and overflow detection. Demo: `fonts.cart.png`. See [`design/font-rendering.md`](design/font-rendering.md).
- Sprite transforms: `spr_rot`, `sspr_ex` (rotation/scale/flip around a pivot).
- **`sget(sx,sy)`** — palette index of a *spritesheet* pixel (companion to `pget`, which reads the canvas). Lets carts treat sprites as data: paint a level in the sprite editor (1 pixel = 1 block, color = type) and read it back at runtime, or build lookup tables. Shipped with two paired platformer tutorial carts — **`platform-rects`** (a pixel-perfect AABB mover: per-axis resolution + sub-pixel position, coyote time, jump buffering, variable jump height, one-way platforms; level as a hard-coded `Box[]`) and **`platform-paint`** (same mover, level read from a painted sprite via `sget`). Same engine, two level sources — the "level as code vs level as data" teaching pair from [`design/tutorial-curriculum.md`](design/tutorial-curriculum.md).
- Fill patterns: `fillp`/`fillp_reset` + `FILL_*` (PICO-8-style fillp).
- Shapes/helpers: `oval`/`ovalfill`, `bar`, `blink`, `fsqrt`.
- Pseudo-3D: `tritex` (affine texture-mapped triangle; used by `mode7`/`raycaster`/`cube3d`).
- 3D leaf-helpers: `V3` + `rot3`/`project3`/`zsort`/`quadfill` — the rotate→project→sort→fill
  pipeline the solid-3D carts re-derived by hand. `cube3d`/`solid3d`/`textured3d`/`flyover`
  refactored onto them. [decision 0009](decisions/0009-small-3d-leaf-helpers.md).
- **`fade()` is now immediate-mode** — the runtime zeroes it each frame, so a cart re-asserts
  `fade()` only on the frames it wants the screen dimmed and never calls `fade(0)` by hand. Fixed
  the same stuck-dim bug in **27 carts** at once (conditional overlay fade that never cleared on
  exit). `camera`/`pal`/`fillp` remain sticky setters. [decision 0010](decisions/0010-fade-is-immediate-mode.md).
- **Removed:** turtle graphics (`turtle_*`/`pen_*`) — one user, trivially a cart; see Cut
  below + [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
- Input: full mouse (`mouse_x/y/down/pressed/released/wheel`), keyboard
  (`key`/`keyp`/`text_input`).
- Time/persistence: `dt`, `epoch`, `save_bytes`/`load_bytes`.
- **`fps()`** — measured frames-per-second right now, `int`, smoothed over the last second
  (wraps Raylib `GetFPS()`). 60 = smooth; lower = the cart is too slow this frame. Independent
  of `dt()`/`det_mode`: even when the sim's `frame_dt` is pinned for deterministic replay,
  `fps()` still reports *real* render throughput, so scripted/headless runs stay reproducible
  while showing true speed. Intended as the read-out for the triangle-perf work (open item 14):
  `watch("fps", "%d", fps())` on a haze-heavy `podracer` frame, before/after a change. *(A
  dedicated profiling setup is in progress separately.)*

**Code-first sound** — 8-voice synth; `note`/`hit`/`chord`/`strum`/`tone`/`degree`,
`bpm`/`beat`, `every`/`euclid`/`chance`, `schedule`, `schedule_hit` (delay **+** duration —
sample-accurate sub-frame sfx/arp steps). (The `sfx` bank plays built-in demo data only —
see "Open" below. **`music()` is CUT** as of 2026-06-04 — zero cart users, patterns were
never cart-authorable, the generative beat-clock path won;
[decision 0013](decisions/0013-cut-music-api.md).)
- **Modulation envelopes** — `instrument_env()`/`note_env()`: 2 routable one-shot AD
  envelopes per slot (`ENV_CUTOFF` = the pluck "pew", `ENV_PITCH` = drum punch/zap,
  `ENV_DUTY`), bipolar amount, exp decay — the second EG (audio-notes §11). Demo carts:
  `filter env`, `pitch env`; wired into `modrack` (onboard fenv/penv + a VCA `a` jack) and
  `dream synth` (AMP/FILTER/PITCH envelope tabs).
- **Drawable waveforms** — `wave_set()` + `INSTR_USER0..3`: four 64-sample single-cycle
  tables you can draw and play like any wave; live-morphable (a ringing note changes as the
  table is rewritten). Demo cart: `wave editor` (draw the cycle, SPACE-drone + live morph,
  seed shapes, exports `wave_set()` code). The cart-authorable half of audio-notes §8.4.
- **Sound-tool carts (draw → export-as-code)** — `sfx editor` (paint 32 steps),
  `sfx generator` (sfxp/bfxr-style: 17 sliders + RANDOM/MUTATE + sfxr category buttons),
  `wave editor`. All export paste-ready C (a data array + a tiny player) — the
  decision-0003 answer to sfx authoring, zero engine banks needed.
- **Sound tripwire + `soundcheck` self-test** — the engine counts dropped requests
  (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED` when sound
  calls are lost (the silent class: defines that never land, notes that never play).
  `soundcheck` cart = worst-case burst + full-API walk; run after touching `sound.h`:
  `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  — silence = PASS. See [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire".
- **Instrument synth** — `instr` is now an instrument slot (0–4 = the raw waves,
  unchanged; 5–15 cart-defined). Four expressive axes bundled per slot, all on the raw
  waveforms: `instrument()` (per-voice **ADSR**), `instrument_duty()` (pulse width),
  `instrument_lfo()` (**3 LFOs**/slot → vibrato/PWM/tremolo/wah), `instrument_filter()`
  (resonant SVF: low/high/band/notch). Demo carts: `instruments`, `lfo`, `filter`, and
  `dream synth` (a playable Moog-style patch panel + keyboard). See
  [`design/audio-notes.md`](design/audio-notes.md) §10.
- **Held notes** — `note_on()→handle`/`note_off()` plus live setters
  `note_pitch`/`note_vol`/`note_cutoff`/`note_res`/`note_duty`/`note_lfo`/`note_filter`/`note_glide`
  (+ `note_off_all`). A sustained voice
  you drive frame-by-frame, the opposite of fire-and-forget `note()`: hold-to-sustain,
  engine revs, sirens, theremins. Handles are `index + generation` so a stale handle
  safely no-ops; per-param slew smooths live writes (no zipper). `note()`/`hit()` keep
  their fixed-duration behavior. Demo cart: `held notes` (a theremin); `dream synth`
  retrofitted onto them (real hold-to-sustain + live filter sweep). See
  [`design/held-notes.md`](design/held-notes.md).
- **Input release edges** — `keyr(k)` / `btnr(player, button)`, the falling-edge partners
  to `keyp`/`btnp` (true on the frame a key/button is released). Needed for hold gestures
  (note_on on press, note_off on release).

---

## Open — prioritized

Ordered by leverage. Section refs point at the design doc that specs each item.
Which *carts* are probing these questions (and every verdict so far) →
[`design/probe-carts.md`](design/probe-carts.md); probe carts carry `"probe"` in
their `kind[]` tags.

1. **Cart-pattern helpers** — `hud()` and `game_over_screen()` cut (see Decided-against).
   - **AABB collision already SHIPPED as `boxes_touch()`** (+ `point_in_box`, `circles_touch`,
     `near`, `touching_map`, `tile_at`, `touching_color` — the full collision section;
     `bounce_at_edges` later cut for zero adoption, [decision 0014](decisions/0014-cut-unused-convenience-helpers.md)).
     *Not* a missing primitive. Open question is **discoverability, not API**: a rough
     grep finds ~30 carts still hand-rolling inline rectangle overlap even though `boxes_touch`
     exists and 15 carts use it — so the lever is teaching (a collision tutorial / converting
     example carts), and *maybe* a more-guessable alias name, not a new function. Adding a bare
     `overlap()` synonym would just grow the already-large surface (see VISION's prune note).
   - `explode()` / particle system is a **research topic** before any build: a no-param
     `explode()` risks making all carts look identical (same concern that killed `hud()`).
     Needs design work on color, shape, lifetime, and movement params first.
     See particle survey + open questions in [`design/api-notes.md`](design/api-notes.md) §C.
2. ~~**2D geometry helpers**~~ — **SHIPPED** as `ngon`/`ngonfill`, `star`/`starfill`, `poly`/`polyfill`, `thickline`, `rrect`/`rrectfill`, `vgradient`/`hgradient` (+ outline siblings `arcoutline`/`ringoutline`/`thicklineoutline` so every filled shape has a matching boundary-ring outline). Demo: `shapes.cart.png`. See [`design/geometry-helpers.md`](design/geometry-helpers.md).
   - *Parked thought (not a build item):* true smooth color interpolation (`lerp_color`/`rgb`) — splits the color model; needs its own ADR. Gradients are dithered.
   - ~~**BUG: `vgradient` renders INVERTED.**~~ **FIXED 2026-06-04** — swapped color roles in `gradient_band()` (`fillp(pat, cb); rectfill(..., ca)`). Thumbnails re-baked for trafficjam, loveparade, loopstation, shapes.
3. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
4. **Pause** — runtime-owned pause overlay. (`fps()` SHIPPED — see Shipped above.)
   [`design/api-notes.md`](design/api-notes.md) §16.

   **First pass (ready to build):**
   - **P / ENTER** opens the overlay; **ESC** while paused resumes; **ESC** while
     not paused closes the window (already works — unchanged).
   - Overlay dims the scene behind it, freezes `update()`, mutes sound.
   - Two items only: **Continue** / **Restart** (restart = `execv` re-exec, NES-style
     full reset — no checkpoints).
   - `paused()` API so carts can check state (e.g. stop a timer).

   **Deferred — Options submenu (document now, build later):**
   Matches PICO-8's pause → Options screen:
   - Sound: ON/OFF
   - Volume: slider/bar
   - Fullscreen: OFF/ON (one `ToggleFullscreen()` call)
   - Controls: read-only display of current P1/P2 key bindings (rebinding stays in
     the editor settings tab)
   - Back

   **Further deferred:**
   - **Per-player pause key** — currently one shared `PAUSE_KEY` for both players.
     When gamepad support lands, each player should have their own pause button
     (P0_PAUSE_KEY / P1_PAUSE_KEY, same `-D` flag pattern as the other bindings).
     The architecture already supports it — just not exposed in the UI yet.
   - **`menuitem(index, label, callback)`** — let carts add custom rows ("restart
     level", "toggle music", "easy mode") to the pause menu. Zero layout work for
     the cart; ~30 carts currently hand-roll their own options screens.
5. **Sound expansion** — _instrument bank (ADSR/duty/LFO/filter) and **held notes**
   (`note_on`/`note_off` + live setters + slew) now SHIPPED, see above._
   **NEXT (decided 2026-06-03, built BEFORE the engines): modulation envelopes** — a second/third
   envelope generator routed to a destination, the one-shot twin of the routable LFO. One call
   `instrument_env(slot, which, dest, attack_ms, decay_ms, amount)` (+ live `note_env`), **2 dests
   to ship** — `ENV_CUTOFF` (the pluck "pew/dwow") and `ENV_PITCH` (drum punch / attack snap / zap;
   `ENV_DUTY` optional) — **2 env slots** so both run at once, **AD shape, exp decay**. Deliberately
   no `ENV_VOLUME` (= the amp ADSR). This was the missing second EG; doing it first makes every
   engine + raw wave better and frees the pluck `morph` knob to be *inharmonicity* instead of
   filter-decay. Demo carts: `pitchenv`, `filterenv`. Spec: [`design/audio-notes.md`](design/audio-notes.md) **§11**.
   Then, behind it: the **navkit rich-instrument port** (rich non-chiptune voices as `INSTR_*`
   engines, each played behind a tiny fixed 3-macro API: harmonics/timbre/morph, §8.1.1) —
   **agreed, on hold until mod-envelopes land.** The bite order (audio-notes §8
   status note + §8.8 port sketch):
     1. **`INSTR_PLUCK`** (Karplus-Strong) — first bite: smallest engine, sounds real instantly,
        proves the per-voice delay buffer (§8.2) the whole buffered family then rides.
     2. **`INSTR_MALLET`** — buffer-free celesta / music-box voice.
     3. **`INSTR_ORGAN` + Leslie** — Hammond drawbars → scanner → shared rotary, as one package.
     4. **EP / acoustic-piano / guitar** family (buffered, on the proven path).
   Two findings already resolved: the **MT70 presets** (Flute/Bells/Organ/Vibes/JzOrg2…) are
   **all pure sine + ADSR + filter — not an engine**, so they need *no port* and ship as
   demo/preset carts on existing API (§8.9); and **`INSTR_SINE` = Additive at harmonics 0**, so
   it ships now and the future Additive engine subsumes the MT70/sine family via macros.
   Also still open: zero-setup **preset instruments** (`INSTR_PLUCK`/`PAD`/…) and cart-side
   **SFX authoring** (the sfx bank is hardcoded today; *pattern* authoring is settled-no —
   `music()` cut, [decision 0013](decisions/0013-cut-music-api.md)) — *direction 2026-06-04: prototype
   as a PICO-8-style editor CART first (draw the pitch contour, toggle per step), zero new
   engine API — hit()/schedule() + beat clock for playback, save_bytes for persistence,
   export-as-C-code into games; `sfx_def()` only if the prototype proves the engine should
   own banks. See audio-notes §5.6 direction note.*
   ⚠️ The port touches `runtime/sound.h`/`studio.c` — shared with the live/libtcc runtime work;
   sync before starting. [`design/audio-notes.md`](design/audio-notes.md) §5–8, [`design/held-notes.md`](design/held-notes.md).
6. **Sprite flags** — `fget`/`fset` (per-sprite 8-bit flags; 256 bytes). Pairs with an
   8-checkbox row in the sprite editor. [`design/api-notes.md`](design/api-notes.md) 2026-05-30 review.
7. **Gamepad** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal `btn()` augment.
   [`design/api-notes.md`](design/api-notes.md) §15.
8. **Strudel extras / Dilla groove timing** — `pitch`, `sometimes`/`often`/`rarely`,
   `arp`, `stutter`, `palindrome`, `off_beat`; `groove` + `groove_swing/jitter/push`,
   `tick`/`PPQ`. [`design/api-notes.md`](design/api-notes.md) §13–14.
9. **Per-game polish pass** — title/game-over screens, hi-scores, sound on every event,
   juice, difficulty curves, readable HUDs. (Reframed as a reference idea bank, not an
   active backlog — see [`POLISH_TODO.md`](POLISH_TODO.md).)
10. **Browser URL-sharing** — the web *build* ships; a one-click hosted **link** does not.
   [`guides/sharing.md`](guides/sharing.md).
11. **iPad runtime** — touch is wired in the runtime; needs a build path. [`VISION.md`](VISION.md).
12. **Sound tracker UI** — open question; depends on whether code-first sound proves
    sufficient. *Direction 2026-06-04: leaning PICO-8-style, prototyped as a CART with
    zero new engine API (see #5 + audio-notes §5.6) — the cheap way to find out if the
    editor earns a place before any engine-side bank API exists.*
    [`VISION.md`](VISION.md), [`design/audio-notes.md`](design/audio-notes.md) §5.6, §9.

13. **Baked rotation atlas** *(exploratory — full design note, not yet started)* — an
    offscreen-canvas primitive (`make_canvas`/`target`/`blit`) plus pre-rotated sprite/shape
    "stamps" so bodies are fast translate-blits instead of per-frame rasterization (for
    crowds, rich shapes, low-end). Centerline/pivot model, `pal()` recolor for free color
    variety, parts capped at 16px (native slot size). The path to scaling the `bones`
    animator past realtime drawing. [`design/baked-rotation-atlas.md`](design/baked-rotation-atlas.md).
    - *Reframe (from the Picotron manual):* Picotron collapses "sprite sheet," "offscreen
      canvas," and "raw memory" into ONE primitive — `userdata(type,w,h)`, a typed 2D buffer
      that sprites/map/screen all are. Suggests the primitive here should be **general**, not
      rotation-specific: a draw target you can render into, read/write per-pixel (`sset`/`sget`),
      and stamp — the offscreen canvas, the rotation cache, and runtime sprite editing are then
      the *same feature*, not three. Worth designing the buffer primitive first; the rotation
      atlas becomes one use of it. (Still index-only — no RGB/alpha, unlike Picotron's userdata.)
14. **Rasterization consistency** *(SHIPPED — every filled primitive on one coverage path)* —
    every filled primitive now shares one pixel-center coverage definition, so the outline is
    exactly the boundary of the fill (no rasterizer drift, dither = solid path):
    `rect`, `circ`/`oval`/`rrect` via `disc_inside`/`ellipse_inside`/`rrect_inside`;
    `tri`/`trifill`, `quadfill`, `ngon`/`star`/`poly` via even-odd `poly_inside` (concave-safe,
    winding-independent; `trifill_pat` deleted); `arc`/`arcfill`/`ring` via `sector_fill` (same
    pixel-centre disc); `thickline` via a capsule coverage (was `quadfill`+caps — the test found
    a 1px seam crack from a `w*0.5` body vs `w/2` cap mismatch). `trifill` is now CPU per-pixel —
    3D carts (`solid3d`/`cube3d`) smoke-tested OK; solid3d's face hairlines gone.
    Detector rewritten to a global invariant (outline == boundary of `fill ∪ outline`): catches
    a 1px offset at any angle, never false-flags sharp tips; verified to have teeth (GPU tris →
    282). Open strokes verified by a 4th-page equivalence self-test (ring==annulus, sector-tiling
    ==disc, thickline solid). Verified: all 11 marker states (3 pages × 4) + 3 equivalence checks
    = 0.
    **Still open (verification, not design):** ~~perf of CPU `trifill` vs old GPU~~ **measured
    (2026-06-01, `podracer`): the cost is real.** CPU `trifill` froze podracer when its haze
    spammed ~190 large software-filled tris/frame; fixed cart-side by moving bulk fills to GPU
    (`rectfill`/`line`). **Off-screen bbox clamp — SHIPPED (2026-06-02).** `poly_fill_cov`/
    `poly_stroke_cov` now intersect their scan box with the on-screen region before scanning,
    mapped through the camera (`GetScreenToWorld2D` on the four screen corners → conservative
    AABB, so translate/zoom/rotation are all correct and the image is byte-identical). Huge
    off-screen tris no longer iterate their full bbox doing point-in-poly tests on cells that
    plot nothing. Verified output-identical: `raster_test` reports `mismatches:"0"` on all 46
    analyse frames + `eq total=0`.
    **It's a performance-cliff guard, not a general speedup** — the cost of a software polygon
    now scales with its *visible* area, not its *total* area. The effect tracks how far a
    poly spills off-screen, so it ranges from huge to nil (all measured with the profiler):
    - `trifill_stress` (synthetic: 12 thin spokes reaching ~1100px off-screen) — **46.7 →
      2.7ms/frame (~17×), ~21fps → 60fps.** This is the worst-case win, not a typical one.
    - `solid3d` (real 3D, model fits the screen so faces only spill a little) — 3.18 → 3.02ms
      avg (~5%), 4.6 → 3.9ms **peak (~15%)**. Modest; the gain is on the frames a face is
      largest.
    - `podracer` — **no effect** (0.81 → 0.75ms, noise): it draws zero software polys (haze
      already on GPU `tritex`/`line`/`rectfill`), so there's nothing on this path to clamp.
    Takeaway: existing well-behaved carts don't get visibly faster; the value is that a future
    cart flying the camera into a `quadfill`/`trifill` surface degrades gracefully instead of
    freezing (the cliff `podracer`'s author had to hand-work-around). *Per-call overhead* is 4
    `GetScreenToWorld2D` (a matrix inverse) — negligible at observed call counts (solid3d got
    faster, not slower); if a cart ever draws thousands of tiny on-screen polys/frame, cache
    the clamp box once per frame (invalidate in `camera()`/`camera_ex()`) instead of per-call.
    Still open: web GL ES confirmation (`pget` disabled on web); an ADR for the GPU→CPU
    `tri`/`trifill` + `thickline` behaviour change.
    **Regression test:** `tools/carts/raster_test.c` + `tools/raster_test.script` —
    drag `editor/public/carts/raster_test.cart.png` into the editor (Z outline, X dither,
    C cycle 4 pages, SPACE analyse / run equiv), or run headless:
    `node tools/play.js raster_test script tools/raster_test.script --headless --trace build/raster_trace.jsonl --frames 60`
    then check every `fs=2` frame reports `mismatches:"0"` and the `eq` line shows `total=0`.
    **Perf-regression test** (the bbox clamp): `tools/carts/trifill_stress.c` — a pinwheel of
    thin off-screen tris. In the editor it should hold 60fps even with reach cranked to max; if
    the clamp regresses, pushing reach tanks the fps. It runs `raster_test` for correctness and
    the ⏱ profiler for the budget (was ~46.7ms unclamped, ~2.7ms clamped at the defaults).
    [`design/rasterization-consistency.md`](design/rasterization-consistency.md).
15. ~~**Tiny fonts**~~ — **SHIPPED** as `font(FONT_SMALL/FONT_TINY)`. See Shipped above.
16. **Packaging & public distribution** *(not started)* — dreamengine only runs as a dev
    build today. A dev-only icon + app name stopgap landed this session (the running app was
    a generic "Electron"); real packaging (bundler, `.icns`, code-signing/notarization, load
    the built renderer instead of `localhost:5173`) is unaddressed. The actual blocker isn't
    Electron — it's that the ▶ run model shells out to a developer's `clang` + Homebrew raylib,
    which a consumer machine doesn't have; web/wasm is the likely public path. Full breakdown:
    [`design/packaging-distribution.md`](design/packaging-distribution.md). Related: browser
    URL-sharing (item 10), iPad runtime (item 11).

17. **Frame-spanning sequence scripts** *(idea — from the Picotron API comparison; needs design)* —
    the *useful half* of Lua's `yield`/coroutines: write time-based logic (cutscenes, scripted
    AI, juice sequences, dialog) as **linear top-to-bottom code** — `walk_to(100); wait(30);
    say("hi"); wait(60); …` — instead of a `switch(state)` shredded across `update()` calls.
    C has no native coroutines, but the pattern is emulable with **protothreads** (Dunkels'
    switch/case macro): stackless, so locals that must survive a `wait` go in `de_state()`.
    **Distinct from the cut "process / coroutine model"** below — that was a full DIV-style
    Level-2 *execution model* (every entity its own process); this is a *small optional helper*
    for sequencing, not a new way to structure carts. Open question is whether a macro trick fits
    dreamengine's "readable C" ethos, or whether it's better shipped as a documented example cart
    than as core API. Worth prototyping one `sequence`/`wait` helper to feel the ergonomics.

18. **Blend tables** *(idea — from the Picotron manual; the substantive capability gap)* —
    index-only translucency/fog/tinting/additive via a precomputed lookup `result = blend[src][dst]`,
    staying entirely in the 32-color palette (**no RGB, no real alpha** — just a table that says
    "drawing color `a` over existing color `b` resolves to `c`"). Unlocks the things carts fake
    with `fillp` dither today: translucent water/glass, fog, additive glows, drop shadows. This is
    a real *capability* dreamengine lacks — `pal()` swaps and `fillp` are the closest, neither
    blends with what's already on screen. Deliberately framed as a lookup table so it does **not**
    trip the "splits the color model" concern flagged on the `lerp_color`/`rgb` parked thought
    (item 2) — the output is always a palette index. Picotron pairs this with stencil clipping;
    that's a separate, lower-value follow-on. **Design note now exists →
    [`design/blend-tables.md`](design/blend-tables.md)**, and the concept is **validated in
    cart-space**: the `blend lab` tech-demo (`tools/carts/blendlab.c`, 2026-06-04) builds
    AVG/ADD/MUL tables and blends per-pixel against a cart-owned scene fn, zero engine API.
    Verdict: the look works (additive glow / glass / fog all read correctly, in-palette), and
    the engine crux is identified — dst must be read from the *in-progress* frame (a `pget`
    last-frame read feeds back and blooms; demonstrated by the cart's `P` mode). Candidate
    implementation: shader + per-scope canvas snapshot, the decision-0007 lane. Next step: ADR —
    **after the palette decision**: blend tables are computed *from* the palette, and the default
    palette (lifted verbatim from PICO-8) should become our own / settable first, or #18 bakes the
    borrowed palette one layer deeper. See [`design/palette-and-color.md`](design/palette-and-color.md)
    (Picotron findings + three-layer plan: new default → `palette_set` + `de:palette` chunk →
    tables-from-active-palette).

19. **Per-cell parameter locks in the cr-78 cart** *(cart-space idea, zero engine API — parked
    2026-06-05)* — Elektron-style p-locks for `tools/carts/cr78.c`: each lit step optionally
    carries a pitch offset (melodic bongos/congas/metal riffs, TR-727 style) and a cutoff
    override (hats opening across the bar). Historically cheeky on purpose: CR-78 voices (1978)
    + the cart's swing knob (LM-1, 1980) + p-locks (Machinedrum, 2001) in one box. Pitch is
    trivial (`fire()` already passes midi). **The known crux is the filter**: one-shot cutoff
    lives on the instrument slot and scheduled notes snapshot their slot at fire time
    ([`design/audio-notes.md`](design/audio-notes.md) §2.2) — per-step cutoff therefore needs
    the sfx-editor **rotating scratch-slot pattern** (cr78 uses slots 9–24; 25–31 free, pool of
    2-3 suffices at one-step lookahead). UI sketch in the cart header: hover+wheel = pitch,
    C+wheel = cutoff, notch markers on the 9×7px cells. Full design notes at the top of
    `tools/carts/cr78.c`.

20. ~~**TB-303 bassline cart**~~ — **SHIPPED same-day 2026-06-05** as `tools/carts/tb303.c` /
    `tb303.cart.png` ("parked for another time" lasted about an hour — user asked for it).
    Exactly as sketched: one `note_on()` voice, `note_glide(60)` slides that don't refire the
    filter envelope (authentic to the circuit), accent = vol 7 + env amount × ACC knob,
    staccato gate at 70% of step, five mouse-draggable knobs with CUT/RES applied live to the
    ringing voice (`note_cutoff`/`note_res`), saw/square switch, mouse piano roll with
    OCT/ACC/SLD flag rows, and an N-key random acid-line generator (root-heavy minor
    pentatonic). The classic-machine family is now cr78 + tr808 + tb303.

21. **More classic boxes — the museum shortlist** *(cart-space, zero engine API — parked
    2026-06-05; the family so far is cr78 + tr808 + tb303, all in `tools/carts/`)*. Curated by
    API fit; the curatorial line is **analog-circuit machines only** — sample/tape boxes
    (LinnDrum, DMX, SP-1200, Mellotron) would be caricatures since the engine has no sample
    playback. Ranked:
    - **TR-606 Drumatix (1981)** — first pick: the TB-303's actual companion (sold as a silver
      pair, sync'd together); all-analog, same six-oscillator metal-bank trick as the 808, both
      cart templates already exist. An afternoon.
    - **TR-909 (1983)** — analog kick/snare/toms doable (house kick = pitch-swept sine + click);
      hats/cymbals are 6-bit samples → honest fakes. Completes the swing-knob saga: the 909 is
      where real shuffle arrived at Roland.
    - **EKO ComputeRhythm (1972)** — Jarre's punch-card programmable box; UI gimmick = draw the
      punch card. Pre-dates the CR-78's "first programmable" claim by six years.
    - **Wurlitzer Sideman (1959)** — the FIRST rhythm machine: tubes + rotating contact wheel;
      UI = a circular playhead instead of left-to-right. Oldest piece by two decades.
    - **Casio VL-1 (1979)** — "Da Da Da"; calculator keys + the 8-digit ADSR number code you
      literally type to design a sound.
    - **Maestro Rhythm King (1970)** — Sly Stone's funk preset box; simpler/weirder than CR-78.
    - **Stylophone (1968)** — mouse-as-stylus, ~20 lines, instant Bowie.

    **Pre-Roland wing — Raymond Scott** (Manhattan Research Inc., the basement where all of
    this started; Bob Moog sold him circuits in the '50s):
    - **Circle Machine (~1959)** — strongest candidate of the whole list: a RING of bulbs, each
      with a brightness knob, swept by a rotating photocell arm — a step sequencer a decade
      before the word. Cart: circular sequencer, drag bulb brightness (= pitch/volume per
      step), rotating playhead instead of left-to-right, rotation speed = tempo. `euclid()`
      would feel period-appropriate. Visually unlike every other music cart in the studio.
    - **Clavivox (1956)** — keyboard theremin with portamento; the great-grandfather of the
      tb303 cart's `note_glide`. Could be a small mouse-played instrument cart.
    - **Electronium (1959-70s)** — not an instrument, a collaborative composition machine you
      NUDGE ("faster", "more like that"); Motown bought one and hired Scott as electronic R&D
      director. Already has descendants here: the bossa/ambient/jangle radio carts. A cart
      that adds the nudge-interface to a generative engine would close the circle.

> `tritex` (affine textured triangle) shipped in session 8 — it was Open here; now in the API.

**Smaller open items (no design doc yet):** looping ambience (`drone`)/`volume`/mute. Noted
in [`POLISH_TODO.md`](POLISH_TODO.md).

**`sprite-draw.js` next steps:**
- ~~Gap audit~~ **DONE** — `ovalfill`, `rrectfill`, `ngonfill`, `polyfill`, `noise` added (2026-06-04).
- **Remaining Tier 2** — `starfill`, `arcfill`, `thickline`, `vgradient`/`hgradient`, `bezier`. Lower priority; current set covers most sprites.
- **JS-only extras** — `hflip`/`vflip`, `rotate90` (reuse one sprite in 4 orientations). Also: migrate terrain-tile carts (cannonfodder, druglord, heroes, hotline, etc.) to use `noise()` instead of their copy-pasted `(x*a + y*b) % m` patterns.
- **Stress-test cart** — a cart exercising all primitives as a visual reference + regression guard.

---

## Decided-against / deferred ✗

These were considered and **cut** — kept here so the decision isn't relitigated.
Rationale lives in [`design/api-notes.md`](design/api-notes.md)'s "What to defer or skip" and the 2026-05-30 review.

- **Process / coroutine model (DIV-style)** — the would-be "Level-2" learning model.
  Every shipped cart works cleanly with plain typed static pools, so it's weeks
  of coroutine/transformer machinery for a model we don't need. [`VISION.md`](VISION.md).
- **Engine-owned entity system** (God-struct / `SELF` / `val[16]` / ECS) — per-game
  typed pools with *named* fields beat all of them for a learn-C console.
- **MMF movement/qualifier engines** (`move_platform`, `move_8dir`) — removes the lesson.
- **`move_and_collide`** (resolved tile push-out) — low demand; only `platform.c` does
  the full pattern, and `zelda`/`gta` test against their own data, not `mget`.
- **DS structures** (lists/maps/grids), **memory arenas**, **PS1 z-sort/ordering table**,
  **tools-as-carts / VFS / fantasy-OS / peek-poke**, a **3D *engine*** (scene graph / mat4
  stack / z-buffer / per-pixel depth) — out of scope. *(The small 3D leaf-helpers
  `rot3`/`project3`/`zsort`/`quadfill` + `V3` ARE shipped — see below and [decision 0009].)*
- **Particle systems** and **pathfinding** — ship as *library carts* (seeds: `astar.c`,
  `boids.c`), not engine surface.
- **Pixel-perfect sprite collision** — eventually maybe; AABB covers 95% first.
- **Turtle graphics API** (`turtle_*`/`pen_*`) — shipped, then **removed 2026-06-01**: only
  `16-spirograph.c` used it, and a turtle is just `dx`/`dy` + `line()`, so it lives in the
  cart now. [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
