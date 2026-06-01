# The game you're building: POOL

## Premise
A single-table game of **8-ball-ish billiards**, hotseat for two players. You aim the
cue with the **mouse** (a dotted guide line shows exactly where the cue ball will go),
**hold to charge power** (a meter fills), and **release to strike**. Honest 2D physics
do the rest: elastic ball-to-ball collisions, rail bounces with friction so the table
**settles** on its own, and balls that roll over a pocket mouth **sink**. Turns alternate;
**scratching** (sinking the cue ball) hands the table over and re-spots the cue. It's a
single-screen physics toy with a clean turn loop — not a full ruleset.

## The locked slice (build exactly this)
- One pool table drawn from **primitives**: felt, wooden rails, 6 pockets, a head-string.
- **15 object balls** racked in a triangle + 1 cue ball, classic colors.
- **Mouse aim**: angle from cue ball to pointer, drawn as a dotted guide line.
- **Hold-to-charge power**: press and hold the mouse, a power bar fills (oscillating or
  ramping); release to shoot the cue ball along the aim.
- **Physics**: per-ball velocity, **circle-circle elastic collisions** (equal mass,
  resolve overlap + exchange normal velocity), **rail reflection** with restitution,
  rolling **friction** so everything decelerates to rest.
- **Pockets**: a ball whose center passes within a pocket radius is **sunk** (removed,
  scored). Sinking the **cue ball = scratch**.
- **2P hotseat**: alternate turns once the table settles. A potted object ball lets the
  same player shoot again (light "made it" rule); a scratch or a dry shot passes turn.
- **Win/restart**: clear all object balls → winner banner → restart.

## Engine features it leans into (and why)
- **`mouse_x/_y` + `mouse_down`/`mouse_pressed`/`mouse_released`** — the whole aim-and-shoot
  verb is mouse-native: pointer sets the angle, the hold/release edges drive the charge.
- **Degree trig (`angle_to`, `cos_deg`/`sin_deg`, `dx`/`dy`)** — aim line and launch vector.
- **`fsqrt`/`distance` + hand-rolled vector math** — circle collisions and rail bounces,
  exactly the `pinball.c` reflection idiom but ball-vs-ball and with friction-to-rest.
- **`dt()`** — framerate-independent rolling, friction decay, and a fixed substep loop so
  fast balls don't tunnel through each other or the rails (the `SUBSTEPS` pattern).
- **`circfill`/`oval`/`rectfill`/`line`** — the entire table and balls; no sprite sheet
  earns its keep here, primitives are crisper and cheaper.
- **Synth SFX** — a soft **click** on ball-ball contact (pitch by impact speed), a deeper
  **rail thunk**, a satisfying **pocket drop**, and a sad **scratch** sting. Plus a faint
  break crack on the opening strike.
- **`shake`** on the break and on hard pots; **`save`** for a running pots-per-player tally.

## Controls
- **Mouse** to aim — move the pointer; a dotted guide shows the cue ball's path.
- **Hold the left mouse button** to charge power (bar fills); **release** to strike.
- **Right-click / SPACE** to cancel a charge without shooting.
- **R** (or SPACE on the win screen) to rack a fresh game.

## Notes / scope cuts
- No spin/english, no cushions-first foul logic, no stripes-vs-solids assignment — just
  "sink object balls, don't scratch." Same-player-continues on a successful pot is the
  only nod to real rules. Depth lives in the **feel of the physics**, not the rulebook.
