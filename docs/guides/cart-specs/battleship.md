# The game you're building: BATTLESHIP

## Premise
Classic two-grid naval Battleship against the CPU. You place a fleet of **5 ships** on
your 10×10 grid, then trade salvos with a hidden enemy fleet: click an enemy cell to
fire, mark hit/miss, and hear the announcer call a ship sunk. The CPU fires back with a
simple **hunt-then-target** AI — random shots until it draws blood, then it probes the
neighbours of its last hit until the ship is dead. First fleet sunk loses.

## The locked slice (build exactly this)
- **SETUP phase:** place the 5 standard ships (Carrier 5, Battleship 4, Cruiser 3,
  Submarine 3, Destroyer 2) on the left (your) grid. Hover shows a ghost ship under the
  cursor; **click** to drop it, **R** to rotate horizontal/vertical. Reject placements
  that overlap or fall off the grid (flash red). A "RANDOM" button auto-places the rest.
- **BATTLE phase:** the right grid is the enemy ocean (hidden). **Click** an enemy cell
  to fire. Mark **miss** (white peg) or **hit** (red peg). When every cell of a ship is
  hit, announce **"YOU SUNK THE <SHIP>"**. Then the CPU fires at your grid using the
  hunt/target AI; show its hits/misses on your board.
- **WIN / LOSE:** sink all 5 enemy ships → win; lose all yours → lose. Show a banner and
  a restart prompt; tally and `save()` wins.

Cuts honoured: no two-player hotseat, no ship-health bars beyond peg marks, no campaign,
no animated water sprites — single screen, two grids, primitives only.

## Engine features it leans into (and why)
- **`mouse_*`** — the whole game is point-and-click on a grid; `mouse_x/y` →
  cell coords, `mouse_pressed(MOUSE_LEFT)` to place/fire. `keyp('R')` rotates.
- **Primitives over sprites** — a board is `rectfill` cells + grid `line`s + `circfill`
  pegs + `print` labels. No sprite sheet needed; cleaner and pixel-crisp.
- **`shake` + `pal`/flash + sound** for juice: a hit shakes the screen and plays a low
  `INSTR_NOISE` boom; a miss is a soft splash; a **sink** triggers a descending
  `strum`/arpeggio sting and a red banner flash.
- **A small turn timer** (`now`/`timer`) paces the CPU reply so its shot lands a beat
  after yours instead of instantly — readable back-and-forth.
- **`save`/`load`** keeps a win/loss tally across runs.

## Controls
- SETUP: move mouse to aim the ghost ship, **click** to place, **R** to rotate,
  **click RANDOM** to auto-fill the rest, **click READY** (or it auto-advances when the
  fleet is placed) to start the battle.
- BATTLE: **click** an enemy cell (right grid) to fire.
- ANY TIME after game over: **click / press Z** to play again.
