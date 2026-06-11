# Frame pacing & on-demand rendering — keeping phones cool

> EXPLORATION (researched 2026-06-10). Source: a real phone-play session — carts
> from the live gallery make the device run **hot**. Three levers; **Lever C (render
> cadence / `renderEvery`) shipped 2026-06-11** — see below. `fps` cap and reactive
> (`repaint`) are still designs. Mobile-display companion (resizable / scale-to-fit /
> rotation) lives in [`mobile-web-notes.md`](mobile-web-notes.md) §4c — this doc owns **power**.

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

### Why opt-in, not the default

It's tempting to make reactive the standard — most of the catalog *is* static most of
the time. The reason not to comes down to **who pays vs. who benefits**: the cost of
reactive is borne by *every* cart and *every* author; the benefit lands only on the
subset that's genuinely static. Opt-in aligns those two sets. Default does not.

The deeper framing: always-redraw is immediate mode's whole gift — **statelessness**.
`draw()` runs every frame, you `cls()`, you repaint the world from scratch, and the
invariant is *"if you drew it, it's on screen."* `repaint()` smuggles retained-mode
bookkeeping back in — it asks the author to prove a negative about their **entire
program** ("nothing changed this frame") every frame, forever. That's worth it for a
radio idling at ~0 GPU; it's a bad tax on a platformer.

The concrete dangers, deepest first:

1. **The failure mode is silent, intermittent, and content-dependent — the worst kind.**
   Forgetting a `repaint()` after a state change shows a **stale screen**. And
   auto-repaint-on-input *hides* the bug during authoring — the author is clicking around,
   every input repaints, everything looks fine. It surfaces later for a user *watching*
   something change untouched: a ticking timer, a scripted enemy, a beat flash. Invisible
   when written, emergent in the field.
2. **Self-animation is the killer, and the engine cannot detect it.** Anything driven by
   `t()` / `frame()` / `sin(now())` — pulsing cursors, scrolling starfields, VU meters, a
   clock — raises no input edge and trips no engine animation. The engine has no way to
   tell a static radio face from one with a blinking 1px LED; only the author knows.
   Reactive-by-default freezes all of it unless the cart repaints every frame — which is
   just always-redraw with a footgun. **This asymmetry is the reason the safe default must
   be the one that's correct without the engine knowing anything about the cart.**
3. **It leaks into every shared header.** `ui.h` / `radio.h` / `solo.h` / `gestures.h`
   all draw, and a library author can't see the cart's reactive setting. Default-on means
   every widget (a knob drag, a focus ring appearing, a loupe opening) must `repaint()` at
   the right moment or silently break — threading render *policy* into UI *widgets*. Opt-in
   contains the blast radius to carts that asked and were tested that way.
4. **It breaks the teachability invariant.** This is a *learning* environment; step one is
   "make something move." Always-redraw means "you drew it, it's there" — the property
   that makes the engine teachable. Reactive-by-default means a beginner's first animation
   silently won't move until they learn a render-policy call: a retained-mode concept in
   lesson one of an immediate-mode tool.
5. **Depth-2 doesn't fail soft — it can hard-lock.** Stale pixels annoy; a missed
   input-poll / wake-up (especially the web rAF timing dance) leaves the cart **hung**, not
   stale. The more valuable the optimization, the scarier its failure mode.

### "Why not just auto-detect change?" — the dirty flag & frame-hashing

The natural intuition — keep one dirty flag in the back and auto-raise it whenever
anything changes — is the right instinct, but **can't be made automatic** in this model.
"Did anything change" has no engine-answerable form, because there's no scene state to
watch:

- The engine can only observe what **it** owns — input edges, `shake`/`fade`, pause,
  resize, hot-reload — and it already raises the flag for exactly those (see the design
  above). That half *is* automatic.
- It **cannot** see that the *cart's* own state changed: cart state is opaque C memory.
  And even a full record of every cart variable write wouldn't help, because
  **state-changed ≠ pixels-changed** — a cart can mutate a variable that never reaches the
  screen, or redraw an identical frame from new numbers.

That leaves only two ways to detect change *without* cart cooperation, both rejected:

- **Hash the rendered pixels** and present only on change — tempting because it's zero
  cart-cooperation, but it needs a **GPU readback**, which this codebase already calls out
  as "expensive and triggers GL errors on WebGL1" — the reason `pget` is disabled on web
  (`:1110`–`:1114`).
- **Hash a CPU-side draw-call log** — avoids the readback, but amounts to building a
  retained-mode scene every frame: out of scope, and it throws away the immediate-mode
  simplicity that's the whole point.

So the flag exists either way — the real question is **who sets it**. The explicit
`repaint()` is cheaper, portable, and honest about the immediate-mode model: the engine
raises it for its half, the cart calls `repaint()` for theirs, because the author is the
only one who knows that *this* state change maps to a visible one.

### Safer-default candidates

If reach matters more than the last drop of idle savings, the move is to **kill the
catastrophic failure modes, not the optimization**:

- **Idle floor instead of true zero.** Reactive with a minimum ~4–8 fps repaint while
  idle. Self-animation degrades to *choppy* rather than *frozen*; a deaf-input lockup
  becomes impossible. Every correctness bug above turns into a mild quality bug. This is
  the concrete answer to the open question below ("should reactive auto-imply a low `fps`
  cap"): **yes — and the floored variant is the only one safe enough to even consider
  defaulting.** True 0-present stays an explicit opt-*down* for hand-verified radios.
- **Default-on only inside chrome you control** — bake the `repaint()` discipline into
  `radio.h` and enable it for *stations*, where one maintainer guarantees the invariant,
  rather than engine-wide across ~50 authors.
- **The genuinely near-defaultable lever is A, not B.** A 30 fps cap on a `dt()`-paced
  cart halves the heat with *zero* stale-screen risk and no whole-program claim. The only
  casualties are `frame()%N` carts — which is why even that stays opt-in.

## Lever C — render cadence (`renderEvery`) — SHIPPED 2026-06-11

The decoupling the "NOT update12" section gestures at, in the *useful* direction: keep
`update()` + `sound_tick()` + input at full rate, but **present only every Nth tick**.
Heat is the *present* (the GPU blit/compositor), so dropping just the present to 30 cools
the phone **without** the sluggish logic/input a whole-loop `fps` cap causes.

Shipped as a per-cart `renderEvery` (`.cart.js` → `de:settings` → `-DRENDER_EVERY=N`,
default 1 = every tick). Present fps = displayHz / `renderEvery`, so only clean divisions:
2→30, 3→20, 5→12… `loop_step` runs every tick; on off-ticks it skips the canvas redraw +
the present and calls `PollInputEvents()` itself (raylib normally polls inside
`EndDrawing()`, so without that the loop goes deaf). The pget snapshot is skipped on
off-ticks too — the canvas didn't change, so the last snapshot still matches.

**Web-only by design:** native pacing lives inside `EndDrawing()` (`SetTargetFPS`), so
skipping the present there would un-cap `update()`. Native always renders every tick
(`skip_render` is a compile-time `false`); desktop isn't power-bound. Validated on device
(iOS Safari): `renderEvery:2` → `fps()` reads 30 while logic stays 60. `studio.c`
`loop_step` + `tools/build-site.js` + `editor/electron/main.cjs` build-web.

## They compose

Orthogonal knobs, not either/or:
- **Reactive** (`repaint`, not yet built) — for carts static most of the time (radios,
  tools, menus, turn-based). Idle → ~0 GPU.
- **`fps` cap** (not yet built) — for carts that animate continuously but don't need 60
  (a calm scroller at 30). Whole loop slows, including logic.
- **Render cadence** (`renderEvery`, SHIPPED) — logic stays 60, only the *present* drops.
  The fit for an animating-but-too-hot cart that must stay responsive.

A cart can mix them: render-cadence while animating, (eventually) reactive while idle.

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
