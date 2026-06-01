# The game you're building: SOLITAIRE (Klondike)

## Premise
The patience classic. A full **52-card deck** dealt into **7 tableau piles** (the
cascade), a **stock** you click to deal one card at a time to a **waste** pile, and
**4 suit foundations** you build up from Ace to King. Move cards and valid runs around
the tableau in **alternating-color, descending** order; send cards home to the
foundations to win. Pure mouse, no clutter — just the green felt and the satisfying
*snap* of a card finding its home.

## The locked slice (build exactly this)
- 7 tableau piles, dealt 1..7 cards, only the top of each face-up.
- A **stock** (face-down): click it to deal one card to the **waste** (draw-1). When the
  stock empties, click it to recycle the waste back under.
- **4 foundations**, one per suit, built up A→K.
- **Drag** a single card or a valid face-up **run** between tableau piles, or a single
  card onto a foundation.
- **Auto-flip** the newly-exposed tableau card after a move.
- **Win** when all four foundations reach King.
- That's it: draw-1, no scoring timer, no undo, no deal variations. A clean
  win + restart loop.

## Move rules (Klondike standard)
- **Tableau onto tableau:** the moved card's color must alternate and its rank be one
  below the destination's top card (red Q on black K). Empty tableau accepts a **King**
  (or a run headed by a King) only.
- **Tableau/waste onto foundation:** same suit, next rank up (A then 2…K). Only one card
  at a time goes to a foundation.
- **Run grab:** you may pick up a face-up card and every face-up card below it as a unit,
  but only if they already form a valid alternating-color descending sequence (they
  always do, since that's the only way they got there — but the grab respects it).

## Engine features it leans into & why
- **Full mouse API** (`mouse_pressed`/`mouse_down`/`mouse_released`, `mouse_x/y`) — the
  whole game is point/drag/drop. This is the showcase: pick up on press, the run follows
  the cursor every frame, drop validates on release.
- **Primitives, no sprite sheet** — cards are `rectfill` + `rect` rounded-ish frames with
  hand-drawn pip suits (the `poker.c` `draw_suit` idiom: hearts/diamonds/clubs/spades
  from circles + triangles). Cleaner and sharper than 16×16 sprites at this size, and it
  costs zero slots. Felt-green table via `cls`.
- **Juice at the moments that earn it:** a soft *tick* (`note`) when dealing from stock,
  a rising *chime* (`strum`) when a card lands on a foundation, a gentle `shake` + bright
  fanfare (`strum` major chord) on the win, an invalid-drop *thunk*. A picked-up run
  draws with a subtle drop **shadow** offset so it reads as lifted.
- **`save()`** the number of games won, shown small in the corner — the only persistence
  this game wants.
- **Win overlay** with `fade` darkening the felt and a centered banner; click to deal a
  fresh shuffle.

## Layout (320×200 canvas)
- Top row: stock + waste on the left, 4 foundations on the right.
- Below: 7 tableau columns, cards overlapping downward (~16px fan for face-up, tighter
  for face-down) so long piles still fit.
- Card size ~36×48 to fit 7 columns across 320px.

## Controls
Mouse only:
- **Click the stock** to deal one card to the waste (click again when empty to recycle).
- **Drag** a card or run from the waste / a tableau pile onto another tableau pile or a
  foundation; release to drop. Invalid drops snap back.
- **Click anywhere on the win banner** (or after a win) to deal a new game. **R** key
  resets at any time.

## Lean into / read
`poker.c` (the deck build/shuffle, the `draw_suit` pip art, the `clicked()` mouse
helper, card frames). Skip: sprites, tile maps, scoring tables, undo — the locked slice
is a tight single-screen mouse game.
