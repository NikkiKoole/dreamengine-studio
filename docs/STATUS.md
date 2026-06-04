# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-06-04 second pass (**sound tooling sprint**: `schedule_hit` shipped (delay+duration note — sub-frame sfx steps); `wave_set` + `INSTR_USER0..3` shipped (drawable single-cycle waves, §8.4 partially resolved); three sound-tool carts shipped — `sfx editor`, `sfx generator` (sfxr categories + mutate), `wave editor` (live-morph drone) — all exporting paste-ready C, validating the §5.6 "editor cart, no engine banks" direction). Earlier same day (**sound: mod-envelopes SHIPPED** — `instrument_env`/`note_env` (ENV_CUTOFF/PITCH/DUTY, kinds 18/19) + demo carts `filterenv`/`pitchenv`, wired into modrack (onboard fenv/penv, VCA `a` jack + flat bank, independent clocks, crisp zoom, scaling cables) and the dream synth (AMP/FILTER/PITCH envelope tabs). audio-notes §2 refreshed as the current surface map (32 fns + 32 consts, the 3-way mod matrix); §9 resolved entries struck (handles won, per-instrument filter, duty placement). **SFX authoring direction:** prototype as a PICO-8-style editor cart, zero new engine API (§5.6). Prior 2026-06-03: **modulation envelopes decided as the next feature**, built before the navkit instrument engines — a routable second EG (`instrument_env`, dests `ENV_CUTOFF`/`ENV_PITCH`), the one-shot twin of the LFO; surfaced ear-testing navkit's pluck (filter-env + pitch-env are one primitive). New audio-notes §11; item #5 reordered. Prior same day: Picotron API comparison — added four ideas: `menuitem` folded into the Pause item (#4 — same feature, two ends), frame-spanning **sequence scripts** (#17, kept distinct from the cut DIV process model), **blend tables** (#18 — index-only translucency/fog/additive, the real capability gap), and a **userdata/offscreen-buffer reframe** folded into the rotation-atlas item (#13 — `sset`/canvas/rotation-cache are one general primitive). Sound comparison corrected: Picotron's audio is a deep node-graph synth + tracker, so the real distinction is code-first vs GUI, not depth). Prior: 2026-06-02 (session 14 — `fps()` shipped as the perf read-out; **one-click profiler shipped** (⏱ profile button, see [`guides/profiler.md`](guides/profiler.md)); **off-screen poly bbox clamp shipped** (item 14) — a cliff guard, ~17× on the synthetic stress cart, modest on real carts; `trifill_stress` regression cart added). Prior: session 13 — `fade()` made immediate-mode, fixing a 27-cart stuck-dim bug._

---

## Shipped ✓

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help),
  sprite editor, map editor — all in one PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — 20 numbered tutorial carts + ~80 example game carts (~100 total).
- **`ragdoll.cart.png`** — Verlet physics sandbox. Up to 50 stick-figure ragdolls hop autonomously across the screen, bounce off each other and off rolling balls. Grab + throw with mouse (whole-body drag), right-click to spawn balls, C to spawn characters, R to reset. Key techniques: Störmer-Verlet integration, distance constraints (12 sticks/character, 8 iterations), position springs chained bottom-up (feet → knees → hip → chest → head), angular springs per bone with 90° guard (cross-product direction inverts past 90°), per-character standing/ragdoll timer that only recovers when feet are on the floor, hop impulse that immediately releases the foot pin so it isn't erased, broad-phase character collision (hip-to-hip > 40px early-out then 12×12 point pairs with velocity impulse). Debug session used the play.js harness + DE_TRACE watches to trace `rtimer`, `whop`, `hip_y`, `knee_y` and diagnose: rest lengths mismatched standing pose (hip-knee was 9, actual √73 ≈ 8.54 → knee pushed to floor); hop velocity erased by foot pre-pin each frame (fixed by setting `rtimer=0` inside `hop_tick` and re-reading `sk`). See `tools/carts/ragdoll.c`.
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- **Live (libtcc) backend + hot reload** — a "run mode" toggle (settings) switches ▶ run from the clang static build to a persistent `-DDE_TCC` host that JIT-compiles the cart in-process via vendored `runtime/libtcc/`. Editing the code auto-reloads it (debounced, no Run press) without restarting the window; compile errors mark the line and keep the last good cart running. State survives reloads via **`de_state()`** — promoted to a first-class `studio.h` API and fronted by the starter cart's friendly `STATE { ... }; / S->field` sugar (clickable to help). arm64-macOS only; sprite/screen changes relaunch. Full record + rationale: [`design/cart-as-script.md`](design/cart-as-script.md).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).
- **Profiler** — one-click `⏱ profile` button (hidden behind a settings toggle). Compiles a profiling build (`-O1 -fno-inline -DDE_PROFILE`), runs the cart ~4s, and reports into the build log: frame CPU budget (ms vs the 16.6ms 60fps target), hottest functions **with the call paths that reach them** (macOS `sample` call-graph attribution, rolled up to `studio.h` primitives), and exact per-frame draw-call counts (in-engine `PROF` counters, re-entrancy-guarded). Behind the scenes — no Instruments GUI. Normal builds byte-identical (`PROF` → no-op). macOS-only (uses the `sample` CLI). See [`guides/profiler.md`](guides/profiler.md).

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
sample-accurate sub-frame sfx/arp steps). (Banks `sfx`/`music` play built-in
demo data only — see "Open" below.)
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

1. **Cart-pattern helpers** — `hud()` and `game_over_screen()` cut (see Decided-against).
   - **AABB collision already SHIPPED as `boxes_touch()`** (+ `point_in_box`, `circles_touch`,
     `near`, `touching_map`, `tile_at`, `touching_color`, `bounce_at_edges` — the full collision
     section). *Not* a missing primitive. Open question is **discoverability, not API**: a rough
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
3. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
4. **Pause + debug** — `pause()`/`paused()`, `voices_active()`. (`fps()` SHIPPED — see
   Shipped above.) [`design/api-notes.md`](design/api-notes.md) §16.
   - **`menuitem(index, label, callback)`** *(idea — from the Picotron API comparison)* — let a
     cart hang custom rows ("restart", "toggle music", "easy mode") off a **built-in pause menu**:
     one line, no layout/input/focus to hand-roll. **Pairs with the pause work above** — it's the
     same feature from two ends: the runtime grows a pause overlay (own the pause state + render
     it), and `menuitem` is the cart's hook into it. High "feels like a real console" payoff for
     the size; the cost is `studio.c` owning the overlay. Today ~30 carts hand-draw their own
     options menus, which is the gap this closes.
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
   **SFX/pattern authoring** (banks are hardcoded today) — *direction 2026-06-04: prototype
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
    implementation: shader + per-scope canvas snapshot, the decision-0007 lane. Next step: ADR.

> `tritex` (affine textured triangle) shipped in session 8 — it was Open here; now in the API.

**Smaller open items (no design doc yet):** looping ambience (`drone`)/`volume`/mute. Noted
in [`POLISH_TODO.md`](POLISH_TODO.md).

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
