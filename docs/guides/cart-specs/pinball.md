# The game you're building: PINBALL (one cool table)

## Premise
A real, physical-feeling **digital pinball table** — gravity, two flippers, a plunger
launch, and a steel ball that ricochets off bumpers, slingshots and ramps. The whole
thing is hand-built from **line-segment walls + circle bumpers**: the ball is a vector
that reflects off geometry, so the feel comes from honest physics, not scripting. One
well-tuned table with a clear chase goal beats ten flat ones.

## Slice (locked) — 🟢
**One vertical table, three balls, one objective loop.** Plunger launch up a side lane;
two bottom flippers; pop bumpers + two slingshots; a bank of **drop targets** that lights
a letter set (spell a word) to arm **MULTIBALL**; one ramp/orbit that feeds a multiplier;
a center spinner for points; a drain gutter that costs a ball; tilt if you nudge too hard;
a HUD (score, ball, multiplier, lit letters) and a saved hi-score. Game over after ball 3.
Skip: multiple tables, video modes, deep rule trees — one table, tuned until it feels alive.

## Core mechanics (the depth = the physics)
- **Ball as a vector:** `float x,y,vx,vy`; gravity adds to `vy` each frame, scaled by
  `dt()`. Cap speed so it never tunnels through a wall.
- **Walls = line segments:** the table is a static array of segments. Collide by
  point-to-segment distance ≤ ball radius, then **reflect** velocity about the segment
  normal (`v -= 2*(v·n)*n`) with a restitution < 1 so it loses a little energy.
- **Flippers:** two pivoting bars (a segment from a pivot, swung by an angle that eases up
  when the button's held and falls back when released). A struck ball gets the flipper's
  **tip velocity** added — that's what gives a live flip its "snap" and lets you aim.
- **Bumpers & slingshots:** circle bumpers reflect the ball *and* kick it outward with a
  fixed impulse (the satisfying *pop*); slingshots are angled segments with a kick. Both
  flash + score on hit.
- **Targets / modes:** drop targets fall when hit (light a letter); complete the set →
  **multiball** (spawn 2–3 balls from a fixed pool) + raise the multiplier. Ramp/orbit
  shots bump the multiplier; spinner spins for rapid points.
- **Drain & tilt:** ball below the flippers between them = lost; `bounce_at_edges`-style
  nudge with the bump keys shoves the ball slightly, but over-nudging **TILTs** (dead
  flippers for the rest of the ball).

## Sprites & maps
Mostly **primitives** — the playfield is line/arc art, bumpers are `circfill` with a lit
ring, the ball is a shaded `circfill` with a highlight pixel. A few sprites for flavor:
the ball can be one sprite, the bumper caps and the backglass art (a 16×32 themed
centerpiece = stacked slots). No tile map needed — the table is geometry data, not tiles.

## Juice
This genre lives on feel: bumper **flash + pop** (white ring, `shake` scaled to impact),
spark `particles` off slingshots, a glowing trail behind a fast ball, screen `shake` on
big hits, lit inserts that pulse, a **multiball** firework + flash, drain = red flash +
sad slide, "TILT" stamp with a screen wobble. Audio: a *ding* per bumper (pitch varies),
a *clack* on flippers, a rising arpeggio as letters light, a klaxon for multiball, a
spinner whir (`LFO`-wobbled tone), and a cheesy lounge bed. Make the ball-speed audible.

## Data model
`struct Ball { float x,y,vx,vy; bool alive; }` (pool of 3 for multiball).
`Seg { float x1,y1,x2,y2; int kind; }[]` for walls/slings (kind sets restitution/kick).
`Bumper { int x,y,r; float lit; }[]`. `Flipper { int px,py; float ang,vang; bool up; }` ×2.
Drop targets as a bitmask + the lit-letters set. `int score, ball, mult; save()` hi-score.

## Controls
Left flipper = `Z` / left, right flipper = `X` / right (or A/B). Hold to raise, release to
drop. Pull/release `DOWN` (or hold then release SPACE) charges + fires the **plunger**.
`Left/Right` arrow taps = **nudge** (careful — TILT!). ENTER = launch next ball / restart.

## Lean into / read
`boids.c` (vector motion + steering math), `gorillas.c` (gravity/projectile arc),
`particles.c` / `effects.c` / `juice.c` (sparks, shake, flash), `20-instruments.c` /
`21-lfo.c` (bumper dings, spinner whir, lounge bed), `12-hiscore.c` (`save()`),
trig helpers (`dx`/`dy`/`angle_to` for the flipper swing + reflections). Skip: any physics
engine / `move_and_collide` (none exists — reflect off segments yourself), tile `map()`
(the table is geometry, not tiles), multiple tables.
