# The game you're building: QBERT

## Premise
The arcade classic, drawn from pure primitives. A **pyramid of 28 cubes** (7 rows) sits
in isometric space against a black void. You're a fuzzy orange ball-with-a-snout who
**hops diagonally** from cube to cube. Every cube you land on **flips its top face** to a
target color — paint all 28 and the board clears. Hop off the edge of the pyramid and you
**plummet to your death**. A purple snake, **Coily**, hatches and bounces down the pyramid
after you. Lives + score, a clean win/lose/restart loop. It's a single-screen, turn-by-hop
arcade game — no scrolling, no tilemap, all geometry.

## The locked slice (build exactly this)
- An **isometric pyramid of cubes**, 7 rows (1+2+3+4+5+6+7 = 28 cubes), drawn as iso
  diamonds from `trifill`/primitives (top face + two shaded side faces).
- **Diagonal hopping** cube-to-cube: Up-Left / Up-Right / Down-Left / Down-Right. Each
  landing **flips the cube to the target color**.
- **Flip every cube** to clear the board → advance / win.
- Hopping **off the edge falls to death** (lose a life, respawn at top).
- **Coily** — one bouncing enemy that chases you down the pyramid; collision = death.
- **Lives + score**, hi-score saved.

## Engine features it leans into (and why)
- **Primitives only (`trifill`/`tri`/`line`/`circfill`)** — a cube is a flat-top diamond
  plus two side parallelograms in three shades; the whole board is geometry, no sprite
  sheet. This is the right tool: 28 procedurally-placed cubes recolored on the fly is far
  cleaner than 28 sprites, and `pal()` isn't even needed because we pick the color index
  directly per face. (Reads from `cube3d.c`'s iso-projection feel.)
- **`lerp`/`ease_out` for the hop arc** — the player and Coily don't teleport between
  cubes; they trace a parabolic jump (`ease_out` across X/Y plus a `sin_deg` lift) so the
  motion reads as a *hop*. Framerate-independent via a normalized 0..1 progress driven by
  `dt()`.
- **`shake` + `fade` + sound for juice** — a square-wave "boop" per hop, a noise-burst
  fall, a descending arpeggio on death, a `strum` major chord on board-clear; screen
  `shake` on landing-streaks and `fade` on the death plummet.
- **`save_int`** for the hi-score.
- **Turn-based input** — input only fires a hop when the player is at rest on a cube
  (`btnp`), so the controls feel discrete and deliberate like the original, while the
  visible motion is still smooth.

## Controls
Diagonal hops only:
- **Q** = Up-Left, **E** = Up-Right, **A** = Down-Left, **D** = Down-Right
- Arrow keys mapped diagonally as a fallback: **Up**=Up-Right, **Left**=Up-Left,
  **Down**=Down-Right(?)… kept simple: arrows Up/Left/Down/Right map to UL/UR/DL/DR.
- **Z / ENTER** restart after game-over.

## Data model (suggested)
- A triangular grid: `cube[row][col]` for row 0..6, col 0..row. Each cell holds a `flipped`
  flag (and we treat "all flipped" as the clear condition).
- Player & Coily each: current `(row,col)`, a hop in progress `(fromRow,fromCol → toRow,
  toCol, t)`. Convert `(row,col)` to a screen pixel with a fixed iso basis.
- A diagonal move changes `(row,col)`: Down-Left = `row+1`, Down-Right = `row+1,col+1`,
  Up-Left = `row-1,col-1`, Up-Right = `row-1,col`. Off-grid target = a fall.
