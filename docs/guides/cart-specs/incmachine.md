# The game you're building: INCMACHINE (The Incredible Machine)

## Premise
A gravity-physics puzzle sandbox in the spirit of *The Incredible Machine*. The board
is a single screen with a few fixed obstacles, a **ball start**, and a **bucket** to
catch it. A **tray of parts** lines one edge — ramp, see-saw, fan, wall, and a spare
ball. You **drag parts from the tray onto the playfield**, then press **R to RUN** the
simulation: gravity pulls the ball, it bounces off your placed parts pinball-style, a
fan can blow it sideways, a see-saw tips. Goal: route the ball into the bucket. Press
**R again to reset** (parts stay placed so you can tweak); RIGHT-CLICK a placed part to
pick it up; the **C** key clears the board. Ship **2 levels**.

## The locked slice — build exactly this, no more
- One physics ball that obeys gravity and reflects off line-segments (pinball reflect).
- A small parts tray: **ramp** (angled reflector), **see-saw** (a ramp that tips toward
  the heavier side while running), **fan** (a zone that pushes the ball away from it),
  **wall** (vertical reflector), and a **spare ball** part you can drop in.
- Fixed level geometry: a few pre-placed pegs/walls + the bucket, different per level.
- **RUN / RESET** toggle on R. Reset returns the ball to its spawn, keeps placed parts.
- **2 levels**, advance on win. **C** clears placed parts. Win = ball settles in bucket.

## Engine features it leans into (and why)
- **Pinball-style segment reflection** (`pinball.c`'s `reflect_seg`): the whole game is
  a ball-vs-line-segment solver. Every part is reduced to one or two segments + maybe a
  force zone, so one tested routine drives all collisions. This is the spine.
- **`dt()`-scaled gravity with physics substeps** (`lander.c`/`sand.c` grain): the ball
  integrates gravity each substep so fast motion doesn't tunnel through thin segments.
- **Full mouse API** (`mouse_x/y`, `mouse_pressed/down/released`, `mouse_right`): the
  entire authoring loop is drag-and-drop. Tray cells, ghost preview while dragging,
  snap onto the board, right-click to lift a placed part.
- **Primitives over sprites**: ramps/walls/fan/bucket are lines, triangles and arcs —
  crisp at any size, no sprite-slot budget, and they read instantly as "machine parts."
- **Juice**: `shake` + particle puff on hard bounces, a fan draws animated wind arcs,
  the bucket flashes and a little chord plays on a win; `fillp` dither on the fan zone.
- **Sound**: short synth `note()`s pitched by impact speed for bounces, a fan whoosh
  (filtered noise instrument), a rising arpeggio on win, a low thunk on reset.

## Controls
- **Left-drag** a tray part onto the board to place it (ghost preview follows the mouse;
  release to drop, snapped inside the playfield).
- **Right-click** a placed part to pick it back up (returns to the tray).
- **R** — run the simulation / reset the ball to spawn (parts stay).
- **C** — clear all placed parts.
- Win drops the ball in the bucket → next level; after level 2, a victory screen, R restarts.

## Lean into / read
`pinball.c` (segment + circle reflection, substeps, impact SFX/particles), `lander.c`
and `sand.c` (gravity feel), `juice.c`/`effects.c` (shake/flash/particles). Skip:
tilemaps (single screen), sprites (primitives are the right tool), networking, scores.
