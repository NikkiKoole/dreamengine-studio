# Frame pacing & on-demand rendering — keeping phones cool

> EXPLORATION (researched 2026-06-10). Source: a real phone-play session — carts
> from the live gallery make the device run **hot**. Two levers exist; this note
> works through both and recommends which to build first. Nothing shipped yet.
> Mobile-display companion (resizable / scale-to-fit / rotation) lives in
> [`mobile-web-notes.md`](mobile-web-notes.md) §4c — this doc owns **power**.

## The problem

A cart pinned at 60fps redraws and **presents a full frame to the GPU/compositor
60×/second forever** — even a radio that's just a static face with audio playing.
On a phone that's a continuous thermal load with nothing to show for it. The goal:
spend GPU/battery in proportion to what's actually changing on screen.

## How the loop works today

`loop_step()` (`runtime/studio.c:1029`) runs **one `update()` + one `draw()` per
frame**, no accumulator, no decoupling. Its phases are already cleanly separated —
which is what makes both levers below cheap:

| Phase | Code | Cost | Must run every frame? |
|---|---|---|---|
| dt + `sound_tick()` | `:1058` / `:1069` | CPU, tiny | **yes** — feeds the audio buffer |
| input snapshot, pause | `:1117` | CPU, tiny | **yes** — or input goes deaf |
| `update()` (cart logic) | `:1135` | CPU, usually tiny | logic-dependent |
| `BeginTextureMode…draw()…EndTextureMode` | `:1141` | **GPU** (the canvas redraw) | only if visuals changed |
| `BeginDrawing…DrawTexturePro…EndDrawing` | `:1181` | **GPU** — blit + buffer swap | only if visuals changed |

Frame-rate control:
- **Native**: `SetTargetFPS(60)` (`:1394`), hardcoded — no runtime knob.
- **Web**: `emscripten_set_main_loop(loop_step, 0, 1)` (`:1454`). The `0` means "use
  the browser's `requestAnimationFrame`" — so **`SetTargetFPS` is ignored on web**;
  you run at display vsync (~60fps), which is exactly what cooks the phone.

The "slow on near death" pattern in CLAUDE.md is a *cart-side* convention
(`frame() % 2`), not an engine feature. There is no `update12`, no frame-skip, no
`slow` flag in the runtime.

## Lever A — global frame-rate cap (per-cart `fps` setting)

Cap the **whole loop** (update + draw + present) at a chosen rate. ~5 lines:
- Add optional `fps` to `.cart.js` → `de:settings`, inject `-DTARGET_FPS=n` (same
  path SCREEN_W/H/SCALE already take through `build-site.js` and `main.cjs`).
- Native: `SetTargetFPS(TARGET_FPS)`.
- **Web: `emscripten_set_main_loop(loop_step, TARGET_FPS, 1)`** — this is the arg
  that actually throttles the loop on a phone; the native `SetTargetFPS` does nothing
  there.

Cart impact: carts that pace off `dt()` (`studio.h:401`) adapt for free; carts that
count frames (`frame() % N`) run at half/third speed. That's why it's **per-cart
opt-in**, not global. A radio/ambient/Satie cart at 30 — or even 20 — fps feels
identical and runs far cooler.

**Why this is NOT `update12`.** Decoupling *logic* (run `update()` at 12Hz, draw at
60) does **not** cut the present rate, and the present rate is the heat. At 320×200
the logic is already nearly free; a decoupled tick buys almost nothing thermally
(it's a CPU optimization for genuinely expensive `update()`s, which we don't have).
The thing worth capping is the **draw/present**, which Lever A does directly.

## Lever B — on-demand / reactive rendering (the bigger win here)

Most of the catalog — radios, instruments, `dpaint`, tools, menus, paused screens,
ambient/Satie visualizers — is **visually static while still producing audio**. A cap
still presents 30–60×/sec; reactive rendering drops a truly static screen to **~0 GPU
presents** and wakes only on change. For *this* mix of carts, that's the real floor.

**The architecture already proves it.** The pause overlay does the freeze trick today:
`if (pause_active) goto draw_window` (`:1107`) skips `update()`+`draw()` and the
comment says "last frame stays frozen" — i.e. the `RenderTexture` **retains its pixels**
when you don't repaint. Reactive rendering is that same move, generalized to any frame
where nothing changed.

### The design (immediate-mode-honest)

There's no retained scene graph to diff, so don't try to auto-detect change by
inspecting drawing. Instead: an opt-in **`reactive` render mode + an explicit
`repaint()`** the cart calls when *its* state changes. The engine raises the dirty
flag automatically for the things it owns:

- any **input edge** this frame (key/btn/touch/mouse changed — already snapshotted at
  `:1117`),
- engine-driven animation still running: `shake_amt > 0` (`:1196`), a `fade()` this
  frame (`:1128`), pause toggle, watch-overlay toggle,
- first frame, window **resize** (if resizable lands — see mobile-web-notes §4c),
  DE_TCC **hot-reload** (`:1074`).

Contract:
- **`update()` and `sound_tick()` run every frame regardless** — this is the whole
  point: a radio keeps playing on a frozen picture. Reactive gates only the *render*.
- If dirty → run the `draw()` block + present, as today.
- If not dirty → skip the canvas redraw (and, at the deeper level below, the present);
  the last presented frame stays on screen.
- Default stays `"always"`. Reactive is per-cart (`.cart.js` → `de:settings`) + a
  settings-tab toggle.

### Two implementation depths

1. **Skip the canvas redraw only** (`:1141`–`:1151`). Saves the `draw()` GPU cost; still
   presents the unchanged texture each frame. Simplest; partial win.
2. **Skip the present too** (`:1181`–`:1195`) — the real battery win, because the buffer
   swap + compositor is the steady drain. Two requirements:
   - **Input must still be polled** every frame, or the loop goes deaf and can never
     un-freeze. raylib polls input inside `EndDrawing()`, so the idle path must call
     `PollInputEvents()` itself.
   - **Web**: additionally drop `emscripten_set_main_loop_timing()` to a slow tick (e.g.
     a few Hz `setTimeout`) while idle and restore rAF on the next dirty frame — this is
     what stops the browser from running the rAF callback 60×/sec at all.

### Rejected: automatic frame-hashing

Render then hash the pixels and present only on change — tempting because it needs zero
cart cooperation, but it requires a **GPU readback**, which this codebase already calls
out as "expensive and triggers GL errors on WebGL1" — the reason `pget` is disabled on
web (`:1110`–`:1114`). The explicit dirty flag is cheaper, portable, and honest about the
immediate-mode model. (A CPU-side draw-call-log hash avoids readback but amounts to a
retained-mode rewrite — out of scope.)

### Risk & mitigation

A cart that forgets `repaint()` after a state change shows a **stale screen**. Mitigations:
auto-repaint-on-input covers the common case (the change usually *follows* an input);
it's opt-in so beginners never hit it; and the failure is obvious and local during
authoring. Document the rule next to the radios that adopt it first.

## They compose

Orthogonal knobs, not either/or:
- **Reactive** — for carts that are static most of the time (radios, tools, menus,
  turn-based). Idle → ~0 GPU.
- **`fps` cap** — for carts that animate continuously but don't need 60 (a calm
  scroller at 30). Steady, lower, predictable.

A cart can use both: reactive while idle, capped when it does animate.

## Wiring & first customers

Both levers need the per-cart settings path (`.cart.js` → `de:settings` → both build
paths + the editor settings tab). `repaint()` is a new API call → the 4-place wiring
per CLAUDE.md (`studio.h` / `studio.c` / `studioDocs.js` / `shell.js`) + a cart that
exercises it. Natural first customers: the **radios** (audio-active, visually near-static
— the textbook reactive case), **sfxgen / modrack / dpaint** (UI that changes only on
interaction), any **paused/title** screen.

## What to prototype first

1. **Lever A `fps` cap** — ~5 lines, immediate measurable thermal drop on web; ship as
   the quick win and the fallback for carts that don't want to manage `repaint()`.
2. **Lever B depth-2 on one radio** — measure idle GPU/temperature with the present
   skipped + web timing dropped. If the win is as large as expected, write `repaint()`
   into the radio chrome (`radio.h`) so every station gets it for free.

Open questions: does any station's visualizer actually change every frame (defeating
reactive)? Should `reactive` auto-imply a low `fps` cap as a safety net? How does
reactive interact with the web debug overlay's rAF-driven fps readout
(mobile-web-notes §6d)?
