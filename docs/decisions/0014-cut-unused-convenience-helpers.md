# 0014 — Cut three zero-adoption convenience helpers (`bezier_cubic`, `anim_once`, `bounce_at_edges`)
Date: 2026-06-04 · Status: accepted

## Context
The API usage audit ([`design/api-usage-audit.md`](../design/api-usage-audit.md),
2026-06-04: 182 functions × 236 carts) found three convenience helpers with **zero
cart users** — each one the less-loved sibling of a helper that did get adopted:

| cut | the sibling that won | why the sibling won |
|---|---|---|
| `bezier_cubic` (2 control points) | `bezier` (quadratic, 1 control point) | one control point covers the cart-scale need; even `bezier` is only in 2 carts |
| `anim_once` (play once, hold last frame) | `anim` (looping, 23 carts) | a one-shot is one line of cart code: `min((int)((now() - t0) * fps), n - 1)` |
| `bounce_at_edges` (DVD-logo bounce) | hand-rolled bouncing everywhere | bouncing is two `if`s, and every cart wants its own flavor (margins, squash on impact, a blip sound) — the 4-pointer-arg signature is more friction than the two `if`s it saves |

## Decision
Remove all three from the engine. No replacement API; each is trivially
expressible in cart code with primitives that stay (`bezier`/`line`, `now`/`min`,
plain comparisons).

## Why
Same bar as [0008](0008-cut-turtle-graphics-api.md) (turtle cut) and
[0013](0013-cut-music-api.md) (music cut): in a learn-C console, a helper that
loses to hand-rolling across 200+ carts is evidence the hand-rolled version *is*
the right teaching surface. Convenience API that nobody adopts still costs
autocomplete noise, help-tab space, and doc maintenance forever.

`anim_once` is the instructive one: the looping `anim` removes a genuinely fiddly
modulo-of-time expression, so it got adopted. The one-shot variant removes a
`min()` — not enough to be worth learning a third function name.

## Consequences
- Removed from all four wiring points: `runtime/studio.h`, `runtime/studio.c`,
  `editor/src/studioDocs.js`, `editor/src/shell.js` (collision/animation/graphics
  help sections shrink by one key each). `studio_tcc_symbols.h` regenerated.
- `design/api-notes.md` proposal sections annotated "shipped, then cut" (the
  original rationale stays as history); surface tables updated.
- If a future cart genuinely needs a cubic curve, two chained `bezier` calls or a
  10-line `for` loop over the cubic formula in the cart is the answer — or it
  becomes a **library cart** snippet per [0006](0006-library-carts-not-engine.md).
