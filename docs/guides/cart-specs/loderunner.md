# The game you're building: LODE RUNNER

## Premise
A faithful little **Lode Runner**: a tiny runner sprints across a brick maze, scoops up
every chunk of gold, then escapes up the exit ladder that only appears once the level is
clean. A guard hunts you — but you can **dig a hole** in the brick floor to its left or
right, drop it in, and run on while the brick **refills** behind you (it crushes a guard
still sitting in the hole). It's a single-screen, grid-based action-puzzle: precise
running, climbing, hand-over-hand rope crossings, and well-timed digging. Two levels.

## The core loop
1. Spawn on a brick-and-ladder level littered with gold. A guard patrols.
2. Run left/right on solid ground, **climb ladders** up/down, hang and shuffle **across
   ropes** (hand-over-hand), and **fall** when there's nothing underfoot.
3. Touch every gold piece to bank it. The guard chases on the same rules you obey.
4. **Dig**: Z removes the brick one cell down-and-left, X one cell down-and-right —
   opening a pit. A guard that walks (or is herded) into the pit is stuck; when the
   brick **regrows** a moment later, a still-trapped guard is crushed and respawns up
   top. You die if a guard touches you on open ground.
5. Collect all gold → the **exit ladder** lights up to the top of the screen. Climb out
   to clear the level. Clear level 2 → you win. `save()` the best (fastest / most gold).

## The locked slice (build exactly this)
- One screen per level, **two levels**, no scrolling.
- Run / climb / rope / fall / **dig-left + dig-right** with timed brick regrowth.
- **One guard** (the slice allows "one or two") that chases, falls in pits, gets crushed.
- Collect-all-gold → reveal exit → climb out → next level → win. Clean win/lose/restart.
- Cuts honored: no level editor, no enemy gold-carrying, no multiple guards swarm, no
  trap-chains. Depth in the dig/refill timing and the chase, not breadth.

## Engine features it leans into (and why)
- **A real tile grid** as the world — bricks, solid stone, ladders, ropes, gold, exit.
  Stored in a `static char grid[][]` (named `grid`, never `map`, to dodge the builtin).
  Drawn from **primitives** (rectfill brick hatching, ladder rungs, rope dashes): the
  art is small, regular and recolorable, so primitives beat spending sprite slots — the
  engine-grain move for a clean blocky maze.
- **Grid-snapped pixel movement** with `dt()` so running/climbing read smoothly but
  always resolve to cells (the classic Lode Runner feel). Falling is just "no support
  below."
- **Per-cell dig timers**: a parallel `float regrow[][]` array counts a dug hole back to
  brick — `dt()`-driven, no engine timers needed. The crush check is a box test the
  frame the brick returns.
- **Guard AI** in the `robotron.c` spirit: greedy pursuit (move toward the player on the
  axis that helps, obeying the same climb/fall/rope rules), but cheap — one entity.
- **Juice & sound**: `shake` + a noise thud on a dig, a bright `note` arpeggio per gold,
  a low buzz on death, a rising run when the exit reveals; `fade` on level transitions.
  `save()` keeps a best-gold / best-time record.

## Controls
- **Arrows** — run left/right, climb up/down a ladder, shuffle along a rope.
- **Z** — dig the brick down-and-left.  **X** — dig the brick down-and-right.
- **R** / button — restart after win or death.
