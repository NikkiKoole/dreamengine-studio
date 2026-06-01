# The game you're building: COLUMNS

## Premise
The 1990 SEGA classic. A vertical **column of 3 gems** falls into a narrow well. While
it drops you can't change its shape — only **cycle the three colors within the column**
and slide it left/right. Land it so that **3+ same-color gems line up in any direction**
(horizontal, vertical, OR diagonal) and they clear, everything above collapses under
gravity, and freshly-made matches **chain** for escalating score. Speed ramps as you
clear. Stack to the top and you top-out. It's a single-screen, primitives-only puzzle —
no sprites, no map.

## The core loop
1. A 3-gem column spawns at the top-center of a `6×13` well, each cell a random color
   from a small palette (start with 6 jewel colors).
2. **Falling:** the column descends on a gravity timer. Left/Right slide it (with
   collision against the stack + walls). **Up / Z** cycles the three colors within the
   column (top→middle→bottom→top). **Down** soft-drops faster.
3. **Lock:** when the bottom gem can't descend, the three gems settle into the grid.
4. **Resolve:** scan the whole grid for runs of 3+ in all 4 line directions (horiz,
   vert, both diagonals); mark every matched cell; clear them with a flash + sound;
   apply gravity so floating gems fall; **re-scan** — repeat while matches keep forming.
   Each successive resolve in the cascade is a higher **chain** multiplier.
5. Score from cleared count × chain. Speed level climbs every N clears. Spawn the next
   column; if it can't spawn (top blocked) → **top-out / game over**.

## The engine features it leans into and why
- **A grid + falling-piece engine adapted from `tetris.c`** — same `static int grid[H][W]`
  (0 empty, else color+1), same collision/lock idiom, but the piece is a fixed 3-tall
  column and the clear rule is *match-3-any-direction* instead of *full-row*. The
  cascade/collapse loop is the heart of the game.
- **Juice at the moments that earn it (`juice.c`)** — matched gems **flash white**
  before they vanish, a small **`shake`** on big/chained clears, a rising **note pitch
  per chain step** so a 4-chain literally sounds like a climbing arpeggio. A landing
  "thock" on lock.
- **Sound from the synth** — `note`/`hit` for move/cycle/lock/clear, pitch keyed to the
  chain depth (`degree` up a pentatonic scale), so feedback is musical not noisy.
- **`save()`** the best score (high score survives between runs).
- **Primitives only** — gems are `rectfill` jewels with a `pset` highlight + inner facet
  lines; the well is a framed `rectfill`. No sprite sheet, no map (cleaner for a
  single-screen puzzle, per the authoring brief).

## Controls
- **Left / Right** — slide the column
- **Up / Z** — cycle the three colors within the column
- **Down** — soft-drop (faster fall)
- **Z / X** — restart on the game-over screen

## Notes
- Match scan is "3+ in a line in any of 4 directions"; mark-then-clear (don't clear
  mid-scan or you'll miss overlapping runs). Collapse = per-column gravity compaction.
- Keep the well narrow (6 wide) so matches are frequent and chains feel achievable.
- Speed ramp via shrinking the gravity tick, floored so it stays playable.
