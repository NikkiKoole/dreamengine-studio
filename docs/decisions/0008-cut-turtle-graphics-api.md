# 0008 — Turtle graphics moves from engine API to cart code
Date: 2026-06-01 · Status: accepted

## Context
`studio.h` shipped a full Logo-style turtle: `turtle_home/move/turn/face/at` +
`pen_down/up/color/size` (9 functions, ~75 lines in `studio.c`). A survey of all 176
carts found **exactly one** user — `16-spirograph.c` — and nothing in the docs/examples
beyond it. Meanwhile a turtle is trivially expressible in a cart: it's three numbers
(x, y, heading) plus a pen, and the engine already exposes `dx`/`dy` + `line()`.

## Decision
Remove the turtle/pen API from the engine. Re-implement the turtle **inside
`16-spirograph.c`** as a handful of local `static` functions, keeping the cart's body
(`turtle_home(); pen_down(); … turtle_move(); turtle_turn();`) verbatim.

## Why
Squarely [decision 0006](0006-library-carts-not-engine.md): in a learn-C console the
algorithm *is* the content. A turtle that one cart uses is a teaching artifact, not core
surface — and burying it in the kernel hides how short it actually is. Moving it into the
cart makes the lesson visible ("a turtle is just dx/dy + line") and shrinks the API by 9
functions for zero loss of capability. Verified: `studio.c` + the rewritten cart compile,
run, and bake an identical thumbnail.

## Consequences
- Removed from all four wiring points: `runtime/studio.h`, `runtime/studio.c`,
  `editor/src/studioDocs.js`, `editor/src/shell.js` (the "turtle" help section is gone).
- `16-spirograph.c` is self-contained; its pocket turtle is copy-pasteable into any cart.
- If turtle graphics ever want a real home again, it's a **library cart**, not engine API
  (same shelf as the `astar.c`/`boids.c` seeds in 0006).
- The `dx`/`dy`/`line`/`sin_deg`/`cos_deg` primitives the turtle was built on stay — they
  are the genuinely cross-cutting core.
