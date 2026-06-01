# The game you're building: MINESWEEPER

## Premise
The classic 1990s desktop time-killer, rebuilt as a clean fantasy-console cart. A grid
of hidden cells; some hide mines. Left-click to reveal, right-click to flag. Reveal a
cell touching no mines and the empty neighborhood **flood-fills** open in one satisfying
cascade. Numbers tell you how many of the eight neighbours are mined. Clear every safe
cell to win; tap a mine and the whole board detonates. A **first-click-is-always-safe**
rule means you never lose on move one. Mouse-primary, single screen, no scrolling.

## The locked slice (build exactly this)
- A fixed **beginner board** (e.g. 16×14 cells, ~30 mines) drawn from primitives.
- **Left-click reveals** a cell. Revealing a 0 (no adjacent mines) **flood-fills** its
  whole empty region plus the numbered border — the cascade.
- **Numbers 1–8** show adjacent mine counts, in the canonical per-number colors.
- **Right-click flags** / unflags a covered cell (can't flag a revealed one).
- **First click is always safe**: mines are placed *after* the first reveal, excluding
  that cell and its neighbours, so the first click always opens a region.
- **Win** by revealing all non-mine cells → auto-flag remaining mines, victory state.
- **Lose** by revealing a mine → reveal the whole board (show all mines, mark wrong
  flags), screen shake + a low detonation tone.
- **HUD**: a live **mine counter** (mines minus flags placed) and an **elapsed timer**
  that starts on the first reveal and freezes on win/lose.
- **Restart** with a click on the smiley / face button or a key — fresh board.
- `save()` the best (fastest) winning time as a record.

Cuts (do not build): question marks, chording (double-click reveal), variable
difficulty menus, hint systems, scrolling boards.

## Engine features it leans into (and why)
- **Pure primitives** — `rectfill`/`rect`/`line` for the beveled covered tiles,
  `circfill` for mines and the smiley, `print` for the numbers. A board game is cleaner
  with no sprite sheet; this is the engine-grain move for puzzle/board carts.
- **`mouse_*`** — `mouse_pressed(MOUSE_LEFT)` to reveal, `mouse_pressed(MOUSE_RIGHT)` to
  flag, hit-tested against a grid with simple integer math.
- **An iterative flood-fill** — a fixed-size stack array (no heap, no recursion) walks
  the empty region exactly like the BFS idiom in `pathfinding.c`.
- **`timer`/`timer_reset`** — the classic running clock, started on first reveal.
- **`shake` + a synth tone** — the detonation moment earns juice: screen kick + a low
  noise/triangle boom; the win earns a little rising arpeggio.
- **`save`/`load`** — persist the best winning time so a record survives between runs.
- **A beveled-tile look** via two `line()` highlights/shadows per covered cell, so the
  board reads as raised buttons that sink when revealed.

## Controls
- **Left-click** a cell — reveal it (flood-fills empty regions).
- **Right-click** a cell — place / remove a flag.
- **Click the face** (top HUD) or press **R** — new game.
- First click is always safe.
