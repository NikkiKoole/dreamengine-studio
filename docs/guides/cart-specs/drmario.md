# The game you're building: DR. MARIO

## Premise
A falling-pill puzzler. A bottle is seeded with colored **viruses**; the doctor drops
in **two-color pills**. Steer and rotate each pill as it falls, then line up **four or
more** capsule-halves (or viruses) of the same color in a row or column — horizontal
or vertical — to clear them. Clear **every virus** in the bottle to beat the level;
let the pills stack up to the neck and you **top out**. After a clear, orphaned
half-pills **fall** (gravity) and can trigger chain reactions.

This is the tetris.c engine retargeted: same fixed grid, same falling-piece /
lock / gravity skeleton — but the match rule is "4-in-a-line by color" instead of
"full rows", the piece is always a 1×2 capsule, the board is pre-seeded with virus
cells, and the win condition is virus count == 0.

## The locked slice (build exactly this)
- A **8×16** bottle grid. Three colors (red / blue / yellow).
- **Two-color pills** spawn at the neck and fall on a timer; **Left/Right** move,
  **Up** rotates (4 orientations: the two halves pivot around each other),
  **Down** soft-drops.
- On lock: scan the whole grid for **runs of 4+** same-color cells horizontally and
  vertically; clear all matched cells (viruses included).
- **Gravity pass** after a clear: any capsule-half (or half-pair) with empty space
  below settles down; re-scan; repeat until stable — that's the **chain**.
- **Level**: N viruses seeded in the lower rows. **Win** when virus count hits 0.
  **Lose** when a freshly spawned pill can't fit (bottle full to the neck).
- Score for clears (more for combos/chains), a **level counter**, **best score**
  saved with `save()`. A clean **win / lose / next-level / restart** loop.

## Engine features it leans into (and why)
- **Fixed-array grid + falling-piece loop** — straight from `tetris.c`: a
  `cell[H][W]` of `0`/`color+1`, a `collide()`/`try_move()`/`lock()` trio, DAS
  auto-repeat on Left/Right so movement feels native.
- **Primitives, no sprite sheet** — capsule halves are rounded `rectfill` + a
  highlight `pset`; viruses are a `circfill` body with two `pset` eyes and a little
  wiggle via `anim`. The whole game is cheap chunky pixels; a sheet would be overkill.
- **Juice at the moments that earn it** (`juice.c` idiom): a white **flash** +
  `shake` on every clear, a brief **hit-stop** on a 4+ multi-clear, the matched
  cells **blink** for a few frames before they vanish. Chains escalate the pitch.
- **Synth SFX where they belong**: a soft square `note()` on rotate, a `hit()` thunk
  on lock, a bright `INSTR_SINE` arpeggio that climbs with the chain depth on a
  clear, a low noise sting on top-out, a little victory `strum` on win.
- **`save()`** for the best score; `dt()` is not needed for the discrete grid step
  (a frame-counted fall timer like tetris.c is the proven idiom here).

## Controls
- **Left / Right** — move the pill sideways
- **Up** — rotate
- **Down** — soft drop (hold to fall fast)
- **Z / X** — start next level / restart after win or loss
