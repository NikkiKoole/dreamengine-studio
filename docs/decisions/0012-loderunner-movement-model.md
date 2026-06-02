# 0012 — Lode Runner movement model: floor division, not rounding

## Decision

`colof`/`rowof` use **floor division** (`(int)(x / CELL)`), not rounding
(`(int)((x + CELL/2) / CELL)`). Support (standing on ground) is checked
against **both the left and right edge columns** of the actor. The fall
code does **not** snap the actor's x to a column grid line.

## Why

The original code labelled `colof`/`rowof` as "snap helpers" and used
rounding. This blended two distinct concerns:

- **Cell lookup** — "which cell does this pixel coordinate belong to?"
  Needs floor division. `py=24` is in row 1 (y=16..31), not row 2.
- **Snap destination** — "which grid line is nearest?" Uses rounding.

Mixing them caused `rowof(24) = 2`, which made the support check look one
row too far down, find a solid brick there, and falsely report support.
The player floated in mid-air because the engine thought they were standing
on a floor two rows below.

Once floor division was used for cell lookup, the alignment check and
snap logic were fixed separately and explicitly:

- **Alignment** (`aligned_x/y`): `ax_mod < 4 || ax_mod > CELL-4` — checks
  proximity to either edge of the cell, not just the top.
- **Support**: checks `solid_cell(cx, cy+1) || solid_cell(cxr, cy+1)` where
  `cxr` is the right-edge column. The player keeps walking until *both*
  edges are over air, matching player expectation ("walk fully off the
  platform, then fall").
- **Fall**: no column snap. The actor drops from wherever they are.

## What not to do

Do not change `colof`/`rowof` back to `(int)((x + CELL/2) / CELL)` to
"fix rounding". The rounding was the bug. If snap-to-nearest-grid is
needed for a specific operation (e.g. ladder x-alignment on climb), do it
explicitly at the call site with `roundf(*ax / CELL) * CELL`, not by
corrupting the cell-lookup functions.
