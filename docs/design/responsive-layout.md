# Responsive layout — a four-primitive set, prototyped in cart-land first

*Status: brainstorm / prototype. Researched 2026-06-11. Companion prototype:
`tools/carts/respond.c` (the "drag the corner" demo).*

**The maker's call (2026-07-02):** for iOS and the product being built to sell, this — reflow, not
just display-scale — is the likely better route. See
[`window-fill-scaling.md`](window-fill-scaling.md) for the sibling idea this was weighed against
(keep `SCREEN_W/H` fixed, fill the window with a fractional scale instead) and why reflow wins for
a real app: a native window is never a clean multiple of a fixed console resolution, and a
genuinely reflowed UI reads as an app rather than a scaled poster, however crisp the scaling gets.
That doc's fractional-fill idea isn't dead — a game viewport *inside* a reflowed layout still needs
`lay_aspect()` to fit itself into whatever cell it lands in — but graduating **this** doc's toolkit
(cart-land prototype → `runtime/lay.h` → opt-in runtime `SCREEN_W/H`) is probably the higher-value
thread to pull on next.

## The question

If we picked a few **modern-CSS** ideas to make responsive layout *possible* in
this engine, what's the minimal set worth having? Not "port CSS" — pick the 3–4
concepts that carry the weight, and find the immediate-mode shape that fits a C
fantasy console.

## Where this sits — read [`mobile-web-notes.md`](mobile-web-notes.md) §4c first

That section already split "responsive?" into two asks, and this doc lives
entirely in the **hard half**:

- **(a) Display-responsive** — scale the fixed canvas to whatever space exists
  (letterbox / integer / stretch). Cheap; the cart never sees a different size.
  *Already mostly handled / speced. Not this doc.*
- **(b) Layout-responsive** — the cart **reflows** to the real size, like a web
  app. Needs `SCREEN_W/H` to become **runtime variables**, which breaks the
  determinism + pixel-art contract the engine and ~45 carts lean on. §4c's
  recommendation stands: **do not make this global**; at most an opt-in
  capability for a narrow class (UI tools, dashboards, radios that fill the
  screen).

**This doc is the toolkit for that opt-in (b) mode** — and, deliberately, a way
to *defer the scary engine change* by proving the toolkit against a fake screen
in cart-land before `SCREEN_W/H` ever moves.

## The precondition (the gate, not a primitive)

Every primitive below is just "resolve a child rect against the *current*
container each frame." So the one thing that must become true for real
layout-responsiveness is a **live, queryable size** — CSS's `vw`/`vh`. Until
`screen_w()`/`screen_h()` (or a `safe_rect()`) are runtime values, none of this
can key off the real window. That's the §4c(b) wall, and we are explicitly
**not** climbing it yet — we climb it only once the cart-land prototype proves
the API is worth it.

## The four primitives

Picked for power-to-weight. Each maps to a CSS concept and to a one-line
immediate-mode function `(container Box) -> (child Box)`. No retained layout
tree — you call them each frame, like every other draw call.

| # | Primitive | CSS lineage | Why it earns a slot |
|---|-----------|-------------|---------------------|
| 1 | **flex** — i-th of n equal children along row/column + gap | `display:flex; gap; flex:1` | Highest leverage. Toolbars, menus, HUD stacks, button rows. The **reflow** primitive: same children, flip `dir` row↔column at a breakpoint. |
| 2 | **fluid** — `clamp(lo, pct% of container, hi)` | `clamp()`, `%`, `vw/vh` | The fluid *number*. Margins, font sizes, bar heights that scale **smoothly** instead of snapping. The difference between "responsive" and "two hardcoded layouts". |
| 3 | **anchor + inset** — pin a w×h box to one of 9 grid spots, inset from edges | `position:absolute; inset; env(safe-area-inset-*)` | Replaces every hardcoded `x=300`. Corners/edges/center. Safe-area (notch/home-bar) folds in here for free. |
| 4 | **aspect** — fit a `ratio` box inside the container, centered | `aspect-ratio; object-fit:contain` | A game viewport / preview that stays proportional and letterboxes itself. The engine already thinks in aspect. |

### Deliberately left out

- **Full CSS Grid** — flex + a wrap covers ~90%; grid is a big retained model.
  The one grid feature worth stealing is `repeat(auto-fit, minmax())`
  (auto-wrapping responsive grid). That single feature is now in the prototype
  as **`lay_wrap()`** (a fifth helper — the swatch palette in `respond`'s
  viewport); the rest of Grid stays out.
- **Media / container queries (as syntax)** — unneeded. `device_class()`
  (mobile-web-notes §5) plus a plain `if (screen_w() < BP)` **is** a breakpoint
  in C. Query syntax buys nothing in an imperative cart; the `respond` demo's
  row→column flip is literally one `narrow ? col : row`.

## The prototype: `tools/carts/respond.c`

Rather than touch the engine, the demo makes a **fake screen**: a rectangle you
resize by dragging its lower-right corner. That rect is passed as the container
to the four functions, and the layout reflows live as you drag — exactly what a
real resizable window would do, with zero engine risk.

```
  ┌───────────────────────────┐         narrow ↓
  │ Dashboard              ●   │      ┌──────────┐
  │  ┌─────────────────────┐  │      │Dashboard ●│
  │  │   16:9 viewport     │  │      │ ┌───────┐ │
  │  └─────────────────────┘  │      │ │ 16:9  │ │
  │ [ Play ][ Share ][ More ] │      │ └───────┘ │
  └──────────────────────────◣│      │ [ Play  ] │
         wide: buttons in a ROW       │ [ Share ] │
                                      │ [ More  ] │
                                      └─────────◣─┘
                                    narrow: buttons COLUMN
```

What each element demonstrates:

- **Title bar** — `anchor` top + `fluid` height (`14%` of screen height, clamped).
- **Notification badge** — `anchor` to the title's **top-right** corner + inset
  (the safe-area analog).
- **16:9 viewport** — `aspect`, letterboxing itself inside the leftover middle,
  with an 8-swatch **`wrap`** palette inside it whose column count auto-fits the
  width (8 across when wide, falling to fewer rows as you drag narrower) — a
  second, breakpoint-free kind of reflow next to the button bar's flip.
- **Button bar** — `flex`; `dir` flips **row → column** under a `150px` width
  breakpoint, and the cart reports `[wide]` / `[phone]` live.
- **Fluid type** — the body font steps TINY → SMALL → NORMAL as the screen
  widens (a breakpoint chain, the cart-land form of a media query).

The cart also **clips** all inner drawing to the virtual-screen rect — so it
behaves exactly like the real engine clipping a cart to `SCREEN_W/H`. Input is
mouse **and** touch (first finger), with a fat-finger pad on the grab handle.

### The API as prototyped (candidate `runtime/lay.h`)

```c
typedef struct { float x, y, w, h; } Box;

float lay_clamp(float v, float lo, float hi);
float lay_fluid(float pct, float container, float lo, float hi);  // clamp(pct*container, lo, hi)
Box   lay_inset(Box c, float m);
Box   lay_at   (Box c, int anchor, float w, float h, float inset); // anchor ∈ L_TL..L_BR (9-grid)
Box   lay_cell (Box c, int dir, int n, int i, float gap);          // dir 0=row 1=col; i-th of n
Box   lay_wrap (Box c, int n, int i, float minItem, float gap);    // auto-fit columns, wrap to rows
Box   lay_aspect(Box c, float ratio);                              // ratio = w/h
```

That's the whole surface. It's deliberately rect-in/rect-out so **nothing
changes** when it graduates — the only difference is what you pass as the
container.

## What this is NOT — out of scope (and that's fine, it's an experiment)

To be explicit: **this is just an experiment.** It's a positioning vocabulary,
nothing more — proof that a small set of `lay_*` helpers feels right, run against
a fake screen so it costs nothing and commits to nothing. It is *not* a layout
engine and it does not pretend to cover everything a responsive UI eventually
wants. Two known gaps, each its own concern, to add **only if a real cart hits
them** (don't pre-build them):

- **Text reflow.** Today we only do the font-step trick (TINY → SMALL → NORMAL by
  width). Genuine **word-wrap inside a box** — break a string across lines to fit
  a width, with truncation/ellipsis when even that won't fit — is a separate text
  helper on top of `text_width()`. Not a layout primitive; a text one.
- **Overflow / scroll.** When content can't shrink to fit even at its minimum
  sizes (a long list on a phone), CSS reaches for `overflow:scroll`. That needs a
  **scroll offset + clip** (and, on touch, momentum) — a whole mechanism the
  layout helpers deliberately don't touch. `lay_*` decides *where things go*; it
  has no opinion on *what to do when they don't fit*.

Neither blocks the experiment. They're noted here so nobody mistakes "the layout
math is done" for "responsive UIs are done."

## Path to graduation (if the prototype proves out)

1. **Live now** — keep iterating in `respond.c` (already has the five helpers
   incl. `lay_wrap`). Pure cart-land, no engine risk.
2. **Promote** the helpers to `runtime/lay.h` (an ADR-0006 cart-land library
   header, like `ui.h`/`gestures.h`) — still operating on an arbitrary `Box`,
   so non-responsive carts can use it for sub-panels today.
3. **Only then** consider the §4c(b) engine work: opt-in runtime `screen_w()/h()`
   + a resizable window + `safe_rect()`, behind the same wall §4b drew for
   device-resolution rendering. `lay.h` carts pass `safe_rect()` as the
   container instead of `vs` and become genuinely responsive — API unchanged.

The point of doing it in this order: we get to *feel* whether four primitives is
the right set, on real layouts, before paying for the part that can't be undone
cheaply.
