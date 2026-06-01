# 0010 — `fade()` is immediate-mode, not a sticky setter
Date: 2026-06-01 · Status: accepted

## Context
A survey of the cart corpus found the **same bug in 27 carts** (incmachine, othello,
turfwar, calgames, connect4, bejeweled, boggle, qbert, boulderdash, civ, doom, outrun,
marble, streetfighter, dwarffort, solitaire, battleship, worms, finalfight, advancewars,
pool, sandburrow, swarmturf, heroes, cannonfodder, wildfire, farm): a game-over / win /
pause overlay calls `fade(0.5f)` inside a conditional, and nothing ever calls `fade(0)`.
`fade_amt` was a persistent static that the runtime never reset, so once the player left
that state (e.g. "press R to play again"), the 50 % dim stayed overlaid on the *entire*
rest of the session.

The original guidance (see [`../guides/cart-authoring.md`](../guides/cart-authoring.md) and
[`../design/cart-survey-api-priorities.md`](../design/cart-survey-api-priorities.md)) was a
**per-cart habit**: "reset the sticky globals you own at the top of `draw()` —
`fade(0); camera(0,0); pal_reset(); fillp_reset();`." That habit works but didn't stick: 27
of ~40 carts that use `fade` got it wrong. A bug that recurs in two-thirds of the cases
isn't an author mistake — it's the wrong default.

## Decision
Make `fade` **immediate-mode**, like every other draw call. The runtime zeroes `fade_amt`
at the top of every frame (in `loop_step`, just before `update()`), so a cart must
**re-assert `fade()` every frame it wants the screen dimmed**:

```c
// runtime/studio.c, top of the frame tick
fade_amt = 0.0f;
update();
```

A conditional `if (gameover) fade(0.5f);` now re-applies while in-state and clears itself
the instant the state ends. **No cart ever needs to call `fade(0)` by hand.** All 27 carts
are fixed by this single change; the now-dead `fade(0)` reset calls were removed from the 7
carts that had them (hotline, loderunner, deckbuilder, digdug, rivercity, larry, zak).

## Why fade and not camera/pal/fillp
The four "you own it" setters are **not** symmetric:
- `fade` has a single global scalar with an obvious neutral (`0`), is read once per frame at
  composite time, and is almost always wanted *conditionally* (only on an overlay). Auto-clear
  is unambiguous and matches how `cls`/draw calls already work.
- `camera`, `pal`, `fillp` are **drawing-scope** state read by many calls *within* a frame —
  you set the camera, draw the world, reset it, draw the HUD. Auto-clearing them per frame
  would be useless (you'd re-set them every frame anyway) and per-call resets would defeat
  their purpose. Their stickiness is load-bearing; `fade`'s was not.

So this is a targeted fix, not "make all render state immediate." `camera`/`pal`/`fillp`
remain sticky setters and the "reset what you own at the top of `draw()`" habit still applies
to *them* — just no longer to `fade`.

## Consequences
- `fade_amt` reset added to `runtime/studio.c`; the `fade` doc comment in `studio.h` and the
  `studioDocs.js` entry (EN + NL, drives autocomplete/hover/help) updated — the old "⚠ STICKY,
  reset with `fade(0)`" warning is replaced with "immediate-mode, call it every frame you want
  it (e.g. inside a game-over overlay)."
- 27 carts fixed with zero per-cart logic; 7 carts had dead `fade(0)` calls removed and their
  `.cart.png` source re-embedded + thumbnails re-baked.
- **Behavioural change for any cart that set `fade()` once and relied on it persisting across
  frames.** None exist in the corpus (every non-zero `fade` call is inside a per-frame
  draw/overlay path; the only one-shot calls were `fade(0)` resets), but a future cart that
  fades-to-black over time must drive it from a per-frame value, not a single call.
- The write-side leak example in `cart-survey-api-priorities.md` is now historical — kept as
  the motivating case, annotated as fixed in the engine.
