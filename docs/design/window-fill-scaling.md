# Window-fill scaling — the fractional "fill the window" step we never ported

STATUS: IDEA (2026-07-01). Not scheduled, not designed in detail — a fully-scoped idea to pick
up later, written down so it can be picked up cold. Companion to
[`touch-controls.md`](touch-controls.md) (the placement brain this would extend) and
[`pixel-perfect-scaling.md`](pixel-perfect-scaling.md) (the rendering-quality half, already
shipped and reusable as-is).

## The gap, in one sentence

The engine ported the *crisp* half of the maker's old pixel-perfect-scaling exploration but not
the *fill-the-window* half — so on any resizable/web/fullscreen surface, the game canvas is
capped to the last integer scale step and every pixel beyond that is just dead black space.

## Where this came from

Found while eyeballing the touch-controls RAILS fix with the new `DE_RESIZABLE=1` env var
(2026-07-01) — dragging the window a lot wider without also dragging it taller left the game
canvas locked at its old size, with the RAILS band (and the space beyond it) growing into a big
empty black gap. That's not a touch-controls bug; it's this doc's gap made visible by a dev tool
that happened to make resizing interactive for the first time.

## The original idea (and what actually shipped from it)

The maker's own old LÖVE sketch —
`~/Projects/love/vector-sketch/experiments/pixelperfect/main.lua` (personal machine, not in this
repo; quoted in full in `pixel-perfect-scaling.md`) — computes **two** scales:

1. `perfectScale = floor(min(w/imgW, h/imgH))` — integer, used once to pre-scale the source into a
   crisp intermediate canvas (nearest-neighbour, no wobble).
2. `lessPerfectScale = min(w/cw, h/ch)` — **fractional**, used every frame in `love.draw()` to
   stretch that intermediate canvas to fill the *actual* window.

The softening from step 2 being non-integer is what step 1's crisp intermediate exists to hide —
classic "sharp bilinear" / integer-prescale trick.

Dreamengine shipped step 1's *quality* half faithfully: `runtime/studio.c`'s `scaleFilter`
setting (modes 0-3, sharp-bilinear shader, `SCALE_FS`) gives a crisp look even at a non-integer
final scale. **But the present blit never actually uses a non-integer final scale on desktop** —
`studio.c`'s draw call (`DrawTexturePro(canvas.texture, …, { game_rect.x, game_rect.y, SCREEN_W *
game_rect.scale, SCREEN_H * game_rect.scale }, …)`, around line 1968) sizes the destination
directly from `game_rect.scale`, and `game_rect.scale` is *always* `gr_place()`'s integer `s`
(`runtime/game_rect.h`: `int sx = win_w / screen_w` — integer division, on purpose, "largest
INTEGER scale that fits (crisp pixels)"). So step 2 — the actual window-fill — never happens on
desktop. The shader modes 2/3 are built and correct, but structurally idle for this case: there's
no non-integer scale for them to smooth.

`gr_place()` postdates the pixel-perfect-scaling work; it was built for a different reason (the
touch-controls DECK/RAILS/OVERLAY letterbox brain — see `touch-controls.md`) and, as a side
effect, foreclosed the fractional-fill idea by routing every leftover pixel into a letterbox band
instead. Neither doc knew about the other's constraint at the time.

## The tension to resolve, whenever this gets picked up

Both goals want the *same* leftover space:

- **Touch controls** want it as a dedicated, stable-shaped band to draw a stick/d-pad/buttons
  into (DECK/RAILS), sized in *window pixels*, not stretched with the game.
- **Window-fill** wants it *reclaimed into the game* so there's no dead space on a phone
  screen or a resized window that will basically never land on an exact integer multiple.

These aren't reconcilable by "just do both" — a cart that opted into `touch_layout()` has a real
claim on the band; a cart that never calls it has no letterbox content to lose. So the natural
fork is: **fill fractionally only when nothing needs the band this frame.**

Sketch (not designed — the actual mechanism needs real thought, not just this paragraph):

- `gr_place()` already computes `PLACE_OVERLAY` for the no-band case (matched aspect) — extend it
  (or add a sibling query) so a cart with `show_touch_ui == false` gets a **fractional** OVERLAY
  scale (`min(win_w/screen_w, win_h/screen_h)`, no floor) instead of the integer one.
- A cart that *did* opt into touch controls keeps today's integer-only DECK/RAILS behavior — its
  band is real estate other code depends on, not leftover space.
- Whenever the chosen scale is non-integer, `scaleFilter` mode 2/3 needs to actually be on (or
  auto-force to at least mode 2) or the result reintroduces the exact wobble/blur the original
  exploration solved — that's the one hard dependency between the two docs.
- Touches: `runtime/game_rect.h` (`gr_place()`, `GameRect.scale`), `runtime/studio.c` (the present
  blit at ~line 1968, `scale_shader`/`SCALE_FILTER`), possibly a new `gr_fill_scale()` alongside
  `gr_place()` rather than overloading `gr_place()` itself.

## Where this matters most (not just desktop dev-preview)

- **Desktop resizable window** (`DE_RESIZABLE=1`, new 2026-07-01 dev/preview flag) — easiest to
  see live, lowest real-world stakes.
- **Web build** — fit-to-viewport almost never lands on an exact integer multiple of a browser
  window; this is probably the highest-value case in practice.
- **iOS fullscreen** ([`ios-plan.md`](ios-plan.md) Phase 4/5, not wired yet) — a phone screen essentially *never*
  matches `SCREEN_W*k × SCREEN_H*k` exactly, so once iOS lands, this gap stops being a resize
  curiosity and becomes the normal, permanent state of the screen.

## Demo path, once someone picks this up

`tools/carts/pixelperfect.c` (the existing filter-comparison demo, see
[`pixel-perfect-scaling.md`](pixel-perfect-scaling.md)) is the natural place to *also* show
integer-locked vs. fractional-fill side by side, the same way it already shows the four filter
kernels — same cart, same magnifier trick, one more comparison row.

## Companions

- [`touch-controls.md`](touch-controls.md) — the placement brain (`gr_place`, DECK/RAILS/OVERLAY)
  this would extend, and the reason the fractional step was dropped in the first place.
- [`pixel-perfect-scaling.md`](pixel-perfect-scaling.md) — the rendering-quality half (shipped);
  read this first for *why* a non-integer scale doesn't have to mean blurry.
- `tools/carts/pixelperfect.c` — the filter-comparison demo; would host the eventual side-by-side.
- `~/Projects/love/vector-sketch/experiments/pixelperfect/main.lua` — the original two-stage LÖVE
  sketch (personal machine, not in this repo) whose step 2 (`lessPerfectScale`) is the missing
  piece described above.
