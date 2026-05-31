# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-05-30 (session 9 — navbar + in-app docs wiki). HEAD: `e372d25`._

---

## Shipped ✓

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help),
  sprite editor, map editor — all in one PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — 20 numbered tutorial carts + ~80 example game carts (~100 total).
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).

**API surface** — ~125 functions + ~90 constants in `runtime/studio.h`.
For the full grouped inventory see [`design/api-notes.md` → "What dreamengine has today"](design/api-notes.md).
Recently landed and worth calling out:
- Juice batch: `pal`/`pal_reset`, `fade`, `shake`, `print_scaled`, `text_width`.
- Sprite transforms: `spr_rot`, `sspr_ex` (rotation/scale/flip around a pivot).
- Fill patterns: `fillp`/`fillp_reset` + `FILL_*` (PICO-8-style fillp).
- Shapes/helpers: `oval`/`ovalfill`, `bar`, `blink`, `fsqrt`.
- Pseudo-3D: `tritex` (affine texture-mapped triangle; used by `mode7`/`raycaster`/`cube3d`).
- Input: full mouse (`mouse_x/y/down/pressed/released/wheel`), keyboard
  (`key`/`keyp`/`text_input`).
- Time/persistence: `dt`, `epoch`, `save_bytes`/`load_bytes`.

**Code-first sound** — 8-voice synth; `note`/`hit`/`chord`/`strum`/`tone`/`degree`,
`bpm`/`beat`, `every`/`euclid`/`chance`, `schedule`. (Banks `sfx`/`music` play built-in
demo data only — see "Open" below.)
- **Instrument synth** — `instr` is now an instrument slot (0–4 = the raw waves,
  unchanged; 5–15 cart-defined). Four expressive axes bundled per slot, all on the raw
  waveforms: `instrument()` (per-voice **ADSR**), `instrument_duty()` (pulse width),
  `instrument_lfo()` (**3 LFOs**/slot → vibrato/PWM/tremolo/wah), `instrument_filter()`
  (resonant SVF: low/high/band/notch). Demo carts: `instruments`, `lfo`, `filter`, and
  `dream synth` (a playable Moog-style patch panel + keyboard). See
  [`design/audio-notes.md`](design/audio-notes.md) §10.

---

## Open — prioritized

Ordered by leverage. Section refs point at the design doc that specs each item.

1. **Cart-pattern helpers** — `explode()`, then `hud()`, then `game_over_screen()`.
   Highest priority because they came from real cart-frequency analysis (each recurs
   in 11–12+ carts). [`design/api-notes.md`](design/api-notes.md) → "Cart-pattern analysis" §A–C.
2. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
3. **Pause + debug** — `pause()`/`paused()`, `fps()`, `voices_active()`.
   [`design/api-notes.md`](design/api-notes.md) §16.
4. **Sound expansion** — _instrument bank (ADSR/duty/LFO/filter) now SHIPPED, see above._
   Still open: `slide()` (portamento); zero-setup **preset instruments** (`INSTR_PLUCK`/
   `PAD`/…); **held channels + per-param slew** (sustained, game-driven voices — the only
   way to get hold-to-sustain, which the synth cart currently fakes with a fixed gate);
   the **navkit rich-instrument port** (organ/Rhodes/piano as `INSTR_*` presets — drops
   into the timbre slot with no API change); and cart-side **SFX/pattern authoring**
   (banks are hardcoded today). [`design/audio-notes.md`](design/audio-notes.md) §5–8.
5. **Sprite flags** — `fget`/`fset` (per-sprite 8-bit flags; 256 bytes). Pairs with an
   8-checkbox row in the sprite editor. [`design/api-notes.md`](design/api-notes.md) 2026-05-30 review.
6. **Gamepad** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal `btn()` augment.
   [`design/api-notes.md`](design/api-notes.md) §15.
7. **Strudel extras / Dilla groove timing** — `pitch`, `sometimes`/`often`/`rarely`,
   `arp`, `stutter`, `palindrome`, `off_beat`; `groove` + `groove_swing/jitter/push`,
   `tick`/`PPQ`. [`design/api-notes.md`](design/api-notes.md) §13–14.
8. **Per-game polish pass** — title/game-over screens, hi-scores, sound on every event,
   juice, difficulty curves, readable HUDs. (Reframed as a reference idea bank, not an
   active backlog — see [`POLISH_TODO.md`](POLISH_TODO.md).)
9. **Browser URL-sharing** — the web *build* ships; a one-click hosted **link** does not.
   [`guides/sharing.md`](guides/sharing.md).
10. **iPad runtime** — touch is wired in the runtime; needs a build path. [`VISION.md`](VISION.md).
11. **Sound tracker UI** — open question; depends on whether code-first sound proves
    sufficient. Prereq is cart-side SFX authoring (the instrument bank itself shipped).
    [`VISION.md`](VISION.md), [`design/audio-notes.md`](design/audio-notes.md) §5.6.

12. **Baked rotation atlas** *(exploratory — full design note, not yet started)* — an
    offscreen-canvas primitive (`make_canvas`/`target`/`blit`) plus pre-rotated sprite/shape
    "stamps" so bodies are fast translate-blits instead of per-frame rasterization (for
    crowds, rich shapes, low-end). Centerline/pivot model, `pal()` recolor for free color
    variety, parts capped at 16px (native slot size). The path to scaling the `bones`
    animator past realtime drawing. [`design/baked-rotation-atlas.md`](design/baked-rotation-atlas.md).
13. **Rasterization consistency** *(exploratory — point-fixes shipped, unified rule OPEN)* —
    the engine rasterizes a shape three different ways (raylib GPU, CPU `*_pat` scanline,
    per-pixel `sector_fill`) and they disagree at edges → seams, off-by-one, outline-vs-fill
    mismatch (hit in arcs, solid3d, katamari). Each got a local fix; the real goal — *one*
    coverage definition shared by fill/outline/dither/solid — isn't done.
    [`design/rasterization-consistency.md`](design/rasterization-consistency.md).

> `tritex` (affine textured triangle) shipped in session 8 — it was Open here; now in the API.

**Smaller open items (no design doc yet):** gradient/dither fill; looping ambience
(`drone`)/`volume`/mute. Noted in [`POLISH_TODO.md`](POLISH_TODO.md).

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
  **tools-as-carts / VFS / fantasy-OS / peek-poke**, **general 3D** — out of scope.
- **Particle systems** and **pathfinding** — ship as *library carts* (seeds: `astar.c`,
  `boids.c`), not engine surface.
- **Pixel-perfect sprite collision** — eventually maybe; AABB covers 95% first.
