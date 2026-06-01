# The game you're building: DIG DUG

## Premise
The arcade classic, distilled. You are a little digger dropped into a field of
solid earth. **Walk and you carve a tunnel** — the dirt you pass through is gone for
good, leaving a passage. Two kinds of monsters live in the dirt: round red **Pooka**
and green fire-breathing **Fygar**. Beat them two ways: face one down a tunnel and
**hold the harpoon to pump it full of air until it pops**, or **dig out the dirt under
a rock** so it crashes down on a monster's head. Clear every monster to advance to a
deeper, busier layer. It's a single-screen real-time game, not a menu game — it should
feel alive and a little frantic.

## The core loop
1. You start in a small pre-cut nook. The rest of the screen is solid earth with a few
   **rocks** suspended in it and a handful of monsters nested in their own pockets.
2. **Move** with arrows. Moving into dirt carves it away (the tile becomes open) and
   leaves a tunnel behind you. You can only travel through open tunnel.
3. **Pump (Z, held):** if a monster is directly in front of you within reach, each pump
   inflates it one stage (a balloon that swells with a *pfft* each stage). Let go and it
   slowly deflates. Four full pumps → **POP**, points scored. Release lets it shrink back.
4. **Rocks:** dig away the dirt directly beneath a rock and it teeters, then falls. A
   falling rock kills any monster (or you!) in its column, then shatters at the bottom.
5. **Ghost mode:** when a monster has been alone too long or is cornered, it flattens
   into a pair of floating **eyes** and drifts *through the dirt* in a straight line
   toward you, rematerializing when it reaches open tunnel. You can't pump a ghost.
6. Clear all monsters → **next layer**: deeper start, more monsters, faster. Lose all
   lives → **game over**, best depth/score saved.

## Engine features it leans into (and why)
- **Destructible tile grid** as the whole game: a `char grid[][]` of dirt/open, drawn
  with `rectfill` in earthy `CLR_BROWN`/`CLR_DARK_BROWN` bands so depth reads visually.
  Carving = setting a cell open; this is the entire mechanic, so it stays primitive-drawn
  (no sprite sheet) for crisp control over the tunnel edges.
- **Grid-locked actors with smooth interpolation** (the pacman idiom): tile coords +
  a 0..1 `prog` between tiles, so movement is framerate-independent via `dt()` yet snaps
  cleanly to the dig grid. Monsters chase along open tunnel with greedy targeting; ghost
  mode is the straight-line-through-dirt escape valve.
- **Juice at the earned moments:** `shake` on a rock impact, a hit-flash via `pal()` while
  pumping, the inflating monster drawn as a growing `circfill`, a quick `fade` on
  death/clear. The pump is a held-button mechanic with audible feedback.
- **Synth SFX that match actions:** a rising pump *pfft* (`note` climbing per stage), a
  balloon-pop, a rock-rumble (`INSTR_NOISE`), a soft footstep tick while digging, and a
  short layer-clear fanfare. No music bed — the arcade originals were sparse and tense.
- **`save()`** for the best score across runs.

## Controls
- **Arrows** — move / dig in that direction (also aims the harpoon).
- **Z (hold)** — pump the monster you're facing; keep holding to inflate it to a pop.
- After game over / clear: **Z** to continue.
