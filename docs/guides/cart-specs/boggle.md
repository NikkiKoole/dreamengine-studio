# The game you're building: BOGGLE

## Premise
A pocket word-hunt on a **4×4 grid of letters**. Drag the mouse across a chain of
adjacent letters — orthogonal *or* diagonal — to trace a word; release to submit. If
it's a real word of **3+ letters** (checked against an EMBEDDED dictionary) and you
haven't found it yet, it scores by length and joins your found list. A **90-second
round** ticks down at the top; when it hits zero the board freezes and shows your tally.
One key restarts with a fresh shuffle. Single screen, no sprites, no map — pure
primitives + the keyboard/mouse API.

## The locked slice (build exactly this)
- 4×4 grid of letters, randomized each round from a weighted letter bag (vowels common,
  Q/Z/X rare) so playable words actually appear.
- **Drag-to-trace**: press on a tile to start a chain; as the pointer moves, each new
  tile that is grid-adjacent (8-way) to the last and not already in the chain extends
  the word. A live line connects the chained cells.
- **Release to submit**: validate length ≥ 3 against the embedded word list; reject
  duplicates already found this round. Accept → score `len` points, push to found list,
  happy chime + brief flash. Reject → buzz, shake.
- **Round timer** counts down from 90s; a HUD bar drains. At zero → ROUND OVER overlay
  with words-found count + score. Press R (or click) to reshuffle and play again.
- **Persist** the best score with `save_int("best", ...)`.

## Engine features it leans into (and why)
- **`mouse_*`** is the whole control scheme: `mouse_pressed`/`mouse_down`/
  `mouse_released` drive begin-trace / extend / submit. `mouse_x/y` map straight to the
  chunky canvas, so cell hit-testing is a simple divide.
- **Primitives over sprites** — letters are `print_scaled` glyphs on `rectfill` tiles;
  the trace is `line` segments + `circfill` nodes. A word game is cleaner with no sheet,
  so there is no `.cart.js`.
- **`dt()` timer** for the framerate-independent countdown; a `bar()` HUD for the drain.
- **Juice at the moments that earn it**: `shake` + red flash on a bad word, a green tile
  pulse + ascending `note()`/`chord()` on a good one, score length spoken back as points.
- **`save_int`** keeps a best score across runs.

## Controls
Drag the mouse across adjacent letters (8-way) to trace a word; release to submit it.
3+ letters that are in the dictionary score (longer = more). R reshuffles / restarts
after the round ends.
