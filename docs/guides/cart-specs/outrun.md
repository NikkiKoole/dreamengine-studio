# The game you're building: OUTRUN

## Premise
A sun-drenched pseudo-3D arcade racer. Blast down a curving, hilly road at huge speed,
weave through traffic, and hit each checkpoint before the timer runs out. The signature
twist (and your ask): **branching road forks** between stages and **varied biomes**, plus
**traffic cars with little people in them**. All about SPEED and feel, not lap times.

## Slice (locked)
Time-attack, **no laps** — a tree of stages you pick at each fork (left/right) ending at
a goal, OutRun-style. ~5 biomes (coast, desert, city, forest, mountains) via shared road
+ `pal()` recolor + biome props. Traffic to dodge, checkpoint timer, top-speed chase.
*This extends the existing `racer.c` — read it, then go bigger.*

## Core mechanics
- **Pseudo-3D road:** the classic **segment/scanline projection** — a list of road
  segments with curvature + hill (dy) values; project each to a trapezoid from the
  bottom of the screen up, interpolating x-offset and scale. No real 3D.
- **Driving:** accelerate/brake, steer; centrifugal push on curves; off-road = slow +
  rumble + dust. Speed sense from road-stripe scroll rate, FOV widen, side-prop whoosh.
- **Forks:** at a fork the road splits into two projected paths; steer into one before
  the split. Each branch = a different next biome.
- **Traffic:** slower cars as scaled **billboard sprites** (`sspr` scaled by distance,
  depth-sorted), with tiny passenger pixels; clipping one = spin-out + time loss.
- **Timer/checkpoints:** beat each checkpoint to extend the clock; miss = game over.

## Sprites & maps
Road + ground + sky are drawn procedurally (no tile map needed for the road). Roadside
**props** (palms, signs, rocks, buildings) as billboard sprites placed per segment,
scaled with distance. Player car + traffic = sprite sets, `pal()` for color variety.
Sky/ground = `fillp` gradient recolored per biome.

## Juice
Speed lines + FOV stretch at top speed, screen `shake` on rumble strips/collisions,
spin-out tumble, tire-smoke particles, a big "CHECKPOINT!" `print_scaled` + time-bonus
rollup, sun glare. Audio: an engine tone whose **pitch tracks speed** (instrument +
`LFO`/filter), tire screech on hard curves, a breezy bpm music bed, crash thud.

## Data model
`struct Seg { float curve, hill; int biome; }` track list (with fork markers); camera
`pos`/`speed`; `struct Car`/`struct Prop` billboard pools with world-z. Project in `draw`
back-to-front.

## Controls
Left/right steer, **Z/up = accelerate, X/down = brake**; steer into a fork to choose.
Gamepad-friendly. (No mouse.)

## Lean into / read
`racer.c` (existing pseudo-3D racer — extend it), `mode7.c` (ground-plane projection
ideas), `effects.c`/`particles.c` (smoke/shake/speed-lines), `22-filter.c`/`21-lfo.c`
(engine-pitch sound). Skip: real 3D, lap counting, car upgrades, damage model.
