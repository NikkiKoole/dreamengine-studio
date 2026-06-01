# The game you're building: LEMMINGS

## Premise
A one-screen tribute to **Lemmings** (DMA Design, 1991). A trapdoor opens, a stream of
brainless green-haired lemmings drops in, and they **trudge right** across a chunk of
destructible dirt. They have no sense of self-preservation — they walk straight off
ledges, into walls, and toward the lava. Your only power is to **assign jobs** from a
toolbar to *individual* lemmings, one click at a time, and steer enough of the herd into
the exit before you run out of lemmings. Save **N of M** to win.

It is NOT a platformer you control directly — you never move a lemming, you only change
what each one *does*. The drama is crowd management: too few diggers and they pile up at a
wall; one badly placed blocker and they all walk into the pit.

## The locked slice — build exactly this, no more
- **One hand-built level.** A solid dirt landscape with a raised spawn ledge, a couple of
  walls/ledges, a gap, and an **exit** on the far side. No level select, no scrolling —
  it all fits on 320×200.
- **The terrain is a pixel buffer.** A `static unsigned char terrain[W*H]` grid where each
  cell is SOLID or EMPTY. Lemmings sense the world by reading it (the cart's own `pget`);
  diggers and bashers **erase** it (the cart's own `pset`). This is the whole point of the
  slice — collision and destruction are the *same* data.
- **Lemming behaviour:** WALK (trudge in facing direction, step up small lips, **turn at a
  wall**), FALL (no floor under feet → drop; a long fall splats and kills), and the
  assignable jobs.
- **Four jobs, from a bar:** DIG (carve straight down, then keep falling/walking),
  BUILD (lay a short diagonal **bridge** of new SOLID pixels upward-forward, a few steps
  then done), BLOCK (plant feet and become a **wall** that turns other lemmings around —
  a living obstacle), BASH (chew **sideways** through dirt in the facing direction).
- **Spawn / save loop:** lemmings drop on a timer until M have spawned. Each one that
  reaches the exit is **saved**; each that splats or hits lava is **lost**. Reach the save
  quota → WIN; run out before the quota → LOSE. R restarts.

## Engine features it leans into (and why)
- **Pixel-buffer terrain via the cart's own get/set** — `sand.c` is the model: a flat
  byte grid drawn as 2×2 (or 1×1) rects, mutated in place. Digging/bashing is just writing
  EMPTY; collision is just reading SOLID. One source of truth for shape *and* physics, the
  way the real game worked. (`pitfall.c` is the reference for "feet-pixel" collision.)
- **Agent pool** — a fixed `Lem lems[MAX]` array with a small state enum per lemming
  (`robotron.c`/`sims.c` idiom). No heap; spawn by activating the next slot.
- **Mouse-first UI** — `mouse_*`: click a job tile in the bottom bar to arm a job, then
  click near a lemming to assign it. A hover ring shows which lemming you'll hit. This is
  the natural Lemmings control scheme and shows off the mouse API.
- **dt()-based movement** — lemmings trudge at a fixed pixels/second so the sim is
  framerate-independent; jobs tick on accumulators, not raw frames.
- **Juice & sound** — a soft *blip* per spawn, a rising **arpeggio** when a lemming is
  saved (the iconic "yippee"), a low **splat** noise on death, a UI click on job-arm, and
  a small `shake` on a splat. A win fanfare via `strum`/`chord`. Optional gentle music bed
  with `every`/`euclid`.
- **Readable HUD** — OUT / SAVED / NEEDED counters, the armed-job highlight, and a
  win/lose banner with restart. `bar`/`print_*` carry it; no map() needed (the world is
  the pixel buffer, not tiles).

## Controls
Mouse-primary. **Click a job** in the bottom toolbar to arm it (armed tile lights up),
then **click on/near a lemming** to assign that job to it. Click an armed tile again (or
right-click) to disarm back to "no tool". **R** restarts the level. That's the whole verb
set — pick a job, pick a lemming, repeat, and herd them home.
