# The game you're building: BOULDER DASH

## Premise
The 1984 cave-digger, distilled. You're a little miner in a dirt-packed cave. Tunnel
through the dirt collecting **diamonds**; meanwhile **boulders** (and the diamonds
themselves) obey **gravity** — they fall the instant the dirt beneath them is gone,
and they **roll off** the rounded top of another boulder/diamond into an empty gap. A
falling rock that lands on your head **crushes you** (and crushes a wandering enemy
too). Grab the **quota** of diamonds, which **opens the exit**, then reach the exit
before the **timer** runs out. One hand-built level, one win.

## The core loop
1. You start in a cave of dirt with boulders, diamonds, an enemy or two, and a closed
   exit. A HUD shows diamonds collected / quota and the countdown timer.
2. **Arrows** step you one cell at a time. Walking into dirt **digs** it (it vanishes);
   walking into a diamond **collects** it; walking into empty space just moves you.
   You can **push** a boulder sideways into an empty cell beyond it (one at a time, only
   horizontally) — you cannot push up or down.
3. Every "tick" (a fixed cadence, not every frame) the cave **settles**: scanning
   bottom-up, any boulder/diamond with empty space below **falls** one cell; if it's
   resting on another rounded rock with an empty diagonal it **rolls** off. A rock that
   falls onto the player's cell **kills** them; onto an enemy, kills the enemy.
4. Collect **quota** diamonds → the exit starts **flashing open**. Step onto the open
   exit → **WIN**. Run out of time, or get crushed → **LOSE**.
5. Press a key to restart. `save()` the best (fastest / most diamonds) run.

## The engine features it leans into and why
- **A cell grid + a bottom-up "settle" pass** — this is exactly the `sand.c` idiom
  (scan from the bottom, each cell makes one local move) specialized to two materials
  (rock, diamond) that fall and roll. Gravity *emerges* from the rule; nothing is
  hand-animated. This is the heart of the game.
- **Grid-stepped movement with a push rule** — straight out of `sokoban.c`: the player
  occupies one cell, moves on a press, and pushing a boulder is "is the cell beyond it
  empty?". Boulder Dash = Sokoban pushing + sand gravity, and both idioms already exist
  in the exemplars.
- **A real `dt()`-paced tick** so the cave settles at a steady, readable cadence
  independent of framerate (and so a falling-rock chain reads as a cascade, not a
  teleport).
- **Camera follow** over a cave bigger than the screen (`follow()` clamped to the
  cave bounds) so digging feels like exploring.
- **Juice at the earning moments** — `shake()` + a thud when a rock lands, a bright
  chime when a diamond is grabbed, the exit `blink()`ing once the quota's met, a red
  `fade` on death, a `strum` fanfare on the win.
- **Primitives, no sprite sheet** — dirt/rock/diamond/wall/exit/player all read fine as
  chunky shaded rects + circles, which keeps the cart self-contained and the cave easy
  to author as ASCII.

## Controls
- **Arrow keys** — step/dig in a direction; push a boulder by walking into it sideways
  with an empty cell beyond.
- **R** (or B) — restart the level.
- Collect the diamond quota to open the exit, then reach it before time runs out.
