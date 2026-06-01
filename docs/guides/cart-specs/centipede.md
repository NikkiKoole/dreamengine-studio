# The game you're building: CENTIPEDE

## Premise
The arcade classic. A **centipede** winds down through a field of **mushrooms** toward
you; you're a blaster pinned to the **bottom band** of the screen, firing upward. Every
shot matters: hit a **mid-body segment** and the centipede **splits in two** — and a new
mushroom sprouts where the segment died. Hit the **head** and it just gets shorter. The
column-by-column descent speeds up as you clear it; wipe out every segment and the next,
faster centipede marches in. A **spider** dances around the bottom band eating mushrooms
and threatening you. **Lives + score**, high score saved.

## The locked slice (build exactly this)
- A centipede = a chain of segments on a coarse grid. It moves horizontally a cell at a
  time, drops one row and reverses at a wall **or a mushroom**, weaving downward.
- Player moves in the **bottom band only** (4–5 rows), fires a single fast bullet upward.
- Bullet vs **mushroom**: chips it (4 hits to clear). Bullet vs **body segment**: kills
  it, **splits** the chain into two independent centipedes, **drops a mushroom** at that
  cell. Bullet vs **head**: kills the head, the next segment becomes the new head.
- **Spider**: bounces diagonally through the lower third, eats mushrooms it crosses,
  kills the player on contact. Worth points; respawns after a beat.
- Clear all segments → advance a wave (faster, more mushrooms). Lose all lives → game over
  → restart.

## Engine features it leans into (and why)
- **Pure primitives, no sprite sheet** — segments are stacked circles with eyes,
  mushrooms are little capped blobs, the player is a chunky blaster. This game is a grid
  of small round things; `circfill`/`ovalfill`/`rectfill` draw it cleaner and cheaper
  than a sprite sheet, and the split/grow logic is pure data.
- **Fixed-size pools** (`robotron.c` idiom): one `Seg[]` array where each segment carries
  its own heading and a "head" flag — splitting is just flipping a head flag mid-array.
  A `mush[GW*GH]` byte grid for mushroom health. No heap.
- **`dt()`-paced step timer** — the centipede advances on a cadence (cell steps/sec) that
  ramps with the wave; the bullet and spider move continuously. Framerate-independent.
- **Juice at the moments that earn it**: `shake` + white `pal`/flash on a player death,
  a satisfying pop sound on every segment kill, a rising blip as the wave clears, a
  spider-skitter tone. `note`/`hit` with `INSTR_NOISE`/`INSTR_SQUARE` — terse, arcade.
- **`save_int("hi", …)`** for the high score.

## Controls
- **Arrows / WASD** — move the blaster (constrained to the bottom band).
- **Z / Space** — fire upward (one bullet on screen at a time, like the arcade).
- On game over: **Z** to restart.
