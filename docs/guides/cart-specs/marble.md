# The game you're building: MARBLE

## Premise
A tiny tribute to **Marble Madness**. You roll a chrome marble through one short
**isometric obstacle course** — from a START tile to the GOAL — before a countdown
timer runs out. The course is built from a **height map**: flat runs, ramps that
**accelerate** you downhill, narrow ledges with **empty void off the edge** that
drop you to your death, and a couple of **hazards** (acid pools, a bottomless gap).
Tilt-style control: the arrows nudge the marble; momentum and gravity do the rest.
It's a physics + nerve game, not a puzzle — one focused course you learn to thread.

## The core loop
1. Marble spawns on START. The clock starts ticking down.
2. **Arrows nudge** the marble (mapped to the iso view — up = away/up-screen, etc.).
   Acceleration is gentle; the marble keeps its **velocity** and coasts.
3. The **height map** under the marble defines a slope. The downhill gradient adds a
   constant pull, so ramps speed you up and you have to brake against them. Flat tiles
   have light rolling friction.
4. Roll **off an edge** (no tile underneath) → you fall, lose a life, **respawn** at
   the last checkpoint with the clock continuing. Same for **acid** tiles.
5. Reach the **GOAL** tile → win; time left becomes bonus score. `save()` the best time.
6. Timer hits 0 or lives hit 0 → game over → restart.

## Engine features it leans into (and why)
- **Float-velocity physics with `dt()`** (the `lander.c` idiom): position/velocity in
  floats, gravity-from-slope added each frame, friction as a multiplicative decay —
  framerate-independent. This is the heart of the game.
- **Hand-rolled iso projection from primitives** — each course tile is a `trifill`/
  `quad` diamond with a shaded left/right face for its height (a cheap "cube" look,
  like `cube3d.c`'s flavor but 2.5D and far simpler). No sprite sheet: the marble is a
  shaded `circfill` with a highlight, its shadow an `ovalfill` on the ground. Drawn
  **back-to-front** (painter's order) so heights overlap correctly.
- **A real tile grid** for the course (a `static` byte grid, not the `map()` API — we
  need per-tile *height* + *type*, so a custom grid is the right grain). Tile types:
  floor, ramp-N/E/S/W (height delta), wall, acid, void, start, goal, checkpoint.
- **`camera()`** to keep the marble framed as it travels the course.
- **Juice:** `shake` on a fall/death, a rising **rolling tone** whose pitch tracks the
  marble's speed (a filtered synth slot), a chime on checkpoint, a fanfare on the goal,
  a low buzz on acid. `fade` for the death/respawn beat.
- **Persistence:** best completion time via `save_int`.

## Controls
Arrows roll the marble (relative to the iso camera). Z / Enter restarts after a
win or game-over. That's the whole control surface — the difficulty is in the rolling.

## Notes / scope (locked 🟡 slice — build exactly this)
- **One** short course (a single screen-ish grid you scroll a little with the camera).
- No level editor, no multiple courses, no enemies/marbles-that-chase. The hazards are
  static (acid pools + void edges + maybe one wall to bank off). Momentum is the game.
- Drawn entirely from primitives — no `.cart.js` needed.
