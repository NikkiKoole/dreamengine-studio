# The game you're building: POD RACER

## Premise
A breakneck, more-genuinely-3D racer through canyon chasms — twin-engine pods screaming
between rock walls at the edge of control. Faster and more dimensional than OutRun: real
textured geometry for the canyon, a boost-with-heat risk system, and a wobbling sense of
barely-contained speed.

## Slice (locked)
**One circuit**, 2–3 laps, a handful of CPU racers. The headline tech: **`tritex`
textured canyon walls + a `mode7`-style ground plane**, not a flat scanline road. Boost/
heat as the one deep mechanic. 🔴 Keep scope to one well-tuned track — depth over count.

## Core mechanics
- **Track:** a centerline path of gates/segments in a heightfielded canyon; walls are
  **textured triangles (`tritex`)**, the floor a `mode7`/projected ground. The cart does
  its own 3D→2D projection (rotate → project → painter's sort), feeding screen coords to
  `tritex` (read `textured3d.c`).
- **Pod handling:** throttle + steer + air-brake into corners; hugging the racing line
  keeps speed; clipping a wall = sparks + speed loss + heat.
- **Boost/heat (signature):** hold boost for massive speed, but engine **heat** climbs;
  overheat = blowout (brief stall). Vents cool when off-boost. Risk/reward.
- **Opponents:** a few CPU pods on the same spline at varying skill; positions + lap HUD.

## Sprites & maps
Canyon walls/rocks = `tritex`-textured triangles sampling sheet textures (generate brick/
rock textures as index arrays in the `.cart.js`, like `textured3d.cart.js`). Pods, gates,
and debris as billboard sprites scaled by distance. Skybox via `fillp` gradient.

## Juice
Heavy speed wobble + FOV pulse on boost, motion/speed lines, wall-scrape sparks
(particles), heat-gauge that glows/pulses red near overheat with a rising alarm tone,
screen `shake` on scrapes + boost, a sonic-boom flash at top speed. Audio: layered engine
drone (two detuned instruments) that pitches with speed, boost roar (filter sweep via
`LFO_CUTOFF`), whoosh as you pass a rival.

## Data model
Track spline/segments with wall texture ids; camera along the spline; `struct Pod {
float t, lane, speed, heat; }` for player + AI; billboard/particle pools. Project + sort
each frame in `draw`.

## Controls
Up/Z = throttle, down/X = air-brake, left/right steer, **hold A (or shift) = boost**.
Gamepad-friendly. (No mouse.)

## Lean into / read
`textured3d.c` (+`.cart.js`) and `solid3d.c` (`tritex` pipeline, the RL_QUADS gotcha),
`mode7.c` (ground plane), `elite.c` (3D projection), `effects.c`/`particles.c`,
`21-lfo.c`/`22-filter.c` (engine/boost sound). Skip: multiple tracks, car customization,
weapons — one track, nailed.
