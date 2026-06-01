# The game you're building: CALIFORNIA GAMES

## Premise
An Epyx-flavored trick-sports cart with a single, polished event: **half-pipe
skateboarding**. You ride a U-shaped vert ramp under a sunset synthwave sky. The
loop is pure timing and flow: **pump at the bottom of each transition** to convert
the ramp into speed, launch off the lip into the air, **throw tricks while
airborne** for points, then **land clean** (skate angle matched to the ramp) or
**bail** and eat the deck. It is NOT a free-roam skater — it's the Epyx idea of one
mechanic distilled to a high-score chase against a 60-second clock.

## The locked slice (build exactly this)
- ONE event: half-pipe vert. A 2D side-on U-ramp, the skater swinging wall to wall.
- **Pump**: tap the timing button as you pass through the flat bottom of the ramp.
  On-beat pumps add energy → more height off the next lip. Mistimed pumps waste it.
- **Air + tricks**: above the lip, the board is airborne. Press a trick button to
  start a named trick (Ollie / Kickflip / 360 / Method / Indy). Each trick spins or
  poses the skater and banks points that only score **if you land clean**.
- **Land**: you must come down with the skater's lean roughly matched to the wall
  angle. Clean landing banks the air's points and keeps your speed; a sloppy or
  over-rotated landing is a **bail** — you lose the air's points, slam (shake +
  flash + crash sfx), and slowly recover.
- **Clock**: 60-second run, score chase, best saved with `save_int`. Restart on the
  game-over screen.
- **(Optional) surf** is intentionally cut — the half-pipe is the whole cart.

## Engine features it leans into (and why)
- **Trig in degrees + a swing model** (`sin_deg`, `dx`/`dy`, `lerp`): the skater's
  position on the ramp is an angle that swings like a pendulum; pumping adds energy,
  gravity/friction bleed it. This is the cheapest, most faithful half-pipe sim and it
  keeps motion framerate-independent via `dt()`.
- **`spr_rot`** for the skater+board so a trick is literally a rotation; **`trifill`**
  / **`arc`** / **`ring`** for the ramp curve, the sun, and the air-meter.
- **Rhythm hooks** (`bpm`/`beat`/`beat_pos`/`every`/`euclid`): a synthwave bed that
  drives the "pump on the beat" feel — a filtered saw bass, euclidean hats, an
  arpeggiated lead, all reactive to airtime.
- **Juice** (`shake`, `fade`, flashes, `ease_*`, a particle pool): launch sparks off
  the lip, a speed-line whoosh, screen shake + red flash on a bail, a points popup
  that eases up and fades.
- **Synthwave sky**: layered gradient bands, a banded setting sun, a scrolling neon
  grid horizon (the genre signature) drawn with primitives — no sprite sheet needed
  for the world, only a small skater.
- **`save_int`** for the hi-score; clean title → play → game-over → restart loop.

## Controls
- **Pump**: Down / A (Z) — tap it as you cross the bottom of the ramp (the PUMP cue
  flashes; on-beat is best).
- **Tricks (airborne only)**: A/Z = Kickflip, B/X = 360 spin, Left/Right = Method/Indy
  grab, Up = Ollie/grab-straighten. Hold to keep the pose; the rotation banks points.
- **Land**: ease off / counter-rotate so the skater is upright-ish over the wall.
- **Start / Restart**: A or X on the title and game-over screens.

## Lean into / read
`geometrydash.c` (one-button timing + beat-driven SFX + particle pool + win/lose
loop), `juice.c` (shake/flash/squash/hit-stop idioms), `skystrike.c` (sky layering),
`omnichord.c`/`drummachine.c` (synth bed). Skip: tile maps (the ramp is parametric),
free-roam, a second event.
