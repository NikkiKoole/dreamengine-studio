# The game you're building: CONNECT FOUR

## Premise
The kitchen-table classic, you against the machine. A **7-wide × 6-tall** grid stands
upright. You and a minimax AI take turns dropping discs into columns; a disc falls to
the lowest empty cell. First to line up **four** — horizontal, vertical, or either
diagonal — wins, and the four winning discs flash in celebration. Fill the board with
nobody connecting and it's a **draw**. It's a tiny, complete, one-screen board game —
the whole point is that it *plays well* against a real opponent, not that it's big.

## The locked slice (build exactly this)
- 7×6 grid, classic Connect Four geometry, drawn from primitives (a blue board with
  round holes, red/yellow discs) — no sprite sheet.
- **Click a column** (or move a cursor with A/D / arrows and drop with Space/Z) to drop
  your disc; it animates falling to the lowest empty cell.
- Detect **4-in-a-row** in all four directions, highlight the winning run, celebrate.
- Opponent is a **minimax AI at depth ~4** with alpha-beta pruning and a simple
  board-evaluation heuristic (center bias + count of open 2s/3s).
- **Full-board draw** handled. Clean win/lose/draw → restart loop. `save()` a small
  win/loss/draw tally that survives between runs.

## Engine features it leans into (and why)
- **Primitives over sprites** — a board game is cleaner drawn with `circfill`/`rectfill`;
  the holes are just background-colored circles punched into the blue board, discs are
  colored circles. No 64-slot budget needed.
- **`lerp` + `ease_in` drop animation** — the disc accelerates down the column under
  gravity, then a tiny `shake` + click on landing. That single bit of juice is what
  makes a static grid feel physical.
- **Minimax with alpha-beta** — depth-4 search over a 7×6 board is trivially fast at
  native speed, and a real adversary is the entire reason to ship this. Plain recursion
  in fixed arrays, no heap.
- **Sound that punctuates** — a soft synth `note` on each drop (pitched by row so a tall
  stack rises), a triumphant `strum`/`chord` on a win, a low tone on a loss.
- **Win-run flash** — `blink()` the four winning discs bright white; `fade` the board
  slightly on game-over so the overlay reads.
- **`save()`** the running W/L/D tally — the only persistent state a session game needs.

## Controls
Mouse-primary: **hover a column** to see the drop preview, **left-click** to drop.
Keyboard fallback: **A/D** or **Left/Right** to move the column cursor, **Space** or
**Z** to drop. After a result, **click** or **Space/Z** restarts. The AI plays yellow
and moves automatically on its turn.
