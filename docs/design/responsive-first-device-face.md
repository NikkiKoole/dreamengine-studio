# Responsive-first device faces — building it in from the start

STATUS: EXPLORING (2026-07-20) — a forward-looking plan, not yet building. Answers the maker's
question "could we make a cart responsive-and-opinionated *from the beginning*, instead of
retrofitting it like we did to acidcandy?" The encouraging finding: the hard, irreversible half is
already done — what's left is cart-land layers (ADR-0006 style), sequenced cheapest → deepest.
Born from the [`acidwide`](../../tools/carts/acidwide.c) mockup session (eight arrangements A–H,
vibe-checked by *looking*). Supersedes nothing; it's the graduation target for
[`canvas-density-spectrum.md`](canvas-density-spectrum.md) + [`responsive-layout.md`](responsive-layout.md).

## The question

Every device face so far — acidcandy, chipjam, dubjam — was authored **fixed-canvas first** (160×100)
and made responsive **after**. That retrofit is the whole acidcandy reflow saga. Could a *new* cart
instead start responsive, with an opinionated layout grammar that gets the arrangement principles
right by default, so nobody re-derives them (or re-makes F's mistake)?

**Yes — and we've already paid for the scary half.**

## What we learned that makes it possible (the constraints, discovered by mocking)

From the `acidwide` A–H study, the reusable principles (this is what an opinionated system would *encode*):

- **Canvas density is one dial** ([canvas-density-spectrum.md](canvas-density-spectrum.md)): a face
  requests a chunky canvas keyed to the device ratio; the lo-fi soul survives because the density
  stays coarse. Reflow, never scale — so `ui.h` touch stays canvas-native (the camera-desync gotcha).
- **The column register.** Per-step things (the sequencer view + the step strip + any accent/slide/tie
  lane) must share one **16-column x-lane**, full-width and stacked, so a step reads as one vertical
  slice. This is the thing E had and F broke.
- **Per-step vs band.** Anything *not* per-step — knobs, mode selectors, PAT/KEY/GEN/PERF — goes in a
  **horizontal band or a nook, never a vertical side-rail** (a rail steals the grid's width and breaks
  the register). This single rule is what would have stopped F automatically.
- **Paging is fine.** You don't need every control visible at once (G proved "everything" is busy);
  a paged screen with a calm always-on aligned strip (H) reads better. The *screen* owns modes; the
  *lane* is permanent.
- **Widgets should size to their cell.** Knob radius/margin/bevel from the cell, not hand-tuned — we
  hit this ~4× in one cart.

## The layers — cheapest to deepest

**Layer 0 — the foundation (DONE).** The irreversible engine work is behind us: runtime
`screen_w()/screen_h()/safe_rect()`, *reflow-not-scale* present, and [`lay.h`](../../runtime/lay.h).
That was the `§4b` wall in [`responsive-layout.md`](responsive-layout.md) / the engine phases in
[`device-adaptive-layout.md`](device-adaptive-layout.md), and it's climbed. Everything below is
cart-land, low-risk, no more engine surgery.

**Layer 1 — a device-face *starter* (cheap, mostly docs + a template).** Today every face re-copies
the chunky-canvas `de_resize` block and re-derives its bands by hand. Promote that into **one template
cart + a recipe** so a new face starts responsive *by starting from it*. Pure capture of what
acidcandy/acidwide already taught; folds into [`design-language.md`](design-language.md).

**Layer 2 — widgets + a lane that own the rules (medium; `ui.h`/`lay.h` additions).**
- **`ui_knob(Box)` / `ui_button(Box)`** — controls that size to a cell (the backlogged widget-in-a-cell
  gap in [`responsive-layout.md`](responsive-layout.md); acidcandy's `_knobx` is the proven source).
- **A `Lane` concept** — the shared 16-column register. The sequencer view *and* the strip bind to one
  lane, so alignment is *guaranteed*, not hand-matched (acidwide's `grid_lane` is the hack this
  replaces). E's insight, made structural.

**Layer 3 — a face *grammar* (the real "opinionated" core; a `face.h` cart-land header).** A small
declarative scaffold over `lay.h`: declare **zones** (nav · knob-band · screen · step-lane) tagged
*per-step* (lane-bound) or *band* (horizontal), plus a per-breakpoint arrangement
(`device_class()` TALL/WIDE/ROOMY), and it reflows + **enforces the principles** — per-step always
shares the lane, non-per-step never becomes a rail. Prototype it against mockups (the acidwide loop)
before anything deeper.

**Layer 4 — make it the default (optional, deepest).** A capability so a "device-face" cart gets the
reflowing canvas + scaffold without opting in each time. Only worth it once Layer 3 proves out across
several carts — don't pre-build.

## The one thing to protect

**Opinionated must stay a scaffold, not a straitjacket.** It has to keep the Fixed-Canvas lo-fi soul
(the chunky-canvas density already ensures that — see [`canvas-density-spectrum.md`](canvas-density-spectrum.md)
+ [ADR-0029](../decisions/0029-320x200-is-the-base-resolution.md)), and any cart
that wants a bespoke layout must still drop to raw `lay.h` underneath. **Opinionated-by-default,
escapable-by-choice.**

## The method is the mockup loop

The way to design Layer 3 is exactly what produced this doc: mock the grammar against throwaway faces
(`acidwide`-style, draw-only), vibe-check by looking, keep the arrangement that survives, then
graduate. The mockup loop isn't a detour from building the system — it *is* how you build it, cheaply
and with the maker's eye in the loop.

## See also

- [`canvas-density-spectrum.md`](canvas-density-spectrum.md) — the density dial (Layer 0/1 mechanism)
- [`responsive-layout.md`](responsive-layout.md) — the `lay_*` toolkit + the widget/gap backlog (Layer 2)
- [`device-adaptive-layout.md`](device-adaptive-layout.md) — the engine foundation that shipped (Layer 0)
- [`design-language.md`](design-language.md) — the paradigm index this would extend (Layer 1)
- [`device-face-paradigm.md`](device-face-paradigm.md) — the device-face family this systematizes
- `tools/carts/acidwide.c` — the A–H arrangement study the constraints came from
