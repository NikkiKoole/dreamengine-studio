# The game you're building: WORMS

## Premise
A 2-player, turn-based **artillery duel on destructible pixel terrain** — the genre
PICO-8 never quite shipped and Gorillas only half-did. Two worms sit on a rolling,
hand-rolled `noise`-terrain landscape. On your turn you **walk** your worm left and
right along the ground, **aim a bazooka** (angle), **charge the shot** by holding a
key (a swinging power meter), then **fire**. The rocket arcs under **gravity + wind**;
on impact it **blows a crater** into the terrain (carved pixel by pixel with `pset`)
and damages any worm inside the blast radius. Then the turn passes. First to knock the
other worm's health to zero wins. Best of nothing — one stock each, one clean
win/lose/restart loop.

We already have `gorillas.c` (static throwers, indestructible buildings). This must
**surpass** it on three axes that define the genre: **you can walk**, **the ground is
destructible** (craters reshape the battlefield, and a worm can fall when the ground
under it is blown away), and there's a **live wind indicator** that actually bends the
shot. Everything else stays tight.

## The locked slice (build exactly this)
- A continuous **terrain heightmap** (one ground height per screen column), generated
  from layered `noise` so it rolls and varies each round.
- A **solid/empty pixel mask** under the surface so craters can punch real holes —
  drawn with `pset`, queried for collision. (Heightmap for the silhouette; mask for
  destruction.)
- **Two worms**, one per player, with **HP**. Each sits on the ground.
- **Turn loop:** AIM/WALK → CHARGE → FIRE → projectile FLIGHT → IMPACT (crater +
  damage + gravity-settle) → hand over to the other player. A turn timer is optional;
  keep it simple.
- **Walking:** LEFT/RIGHT move the active worm along the surface (it follows the
  ground height, can't walk through a hill that's too steep — clamp the climb).
- **Aiming:** UP/DOWN sweep the bazooka angle; the barrel and a dotted predictive
  pip-trail show the aim.
- **Power:** hold **Space** → a power bar fills (and swings back down so timing
  matters, or just fills to a cap); release to fire at that power.
- **Projectile:** integrates `vx,vy` with gravity each frame, plus a constant **wind**
  acceleration set per round (shown by a HUD arrow). Trails a little smoke.
- **Explosion:** carve a circular crater into the mask + redraw terrain; radial
  **damage** falloff to any worm within blast radius; `shake` + flash + a noise `sfx`.
- **Falling:** after a crater, if the ground under a worm is gone, it falls until it
  lands (small fall damage optional — keep it gentle or skip).
- **Win/lose:** when a worm hits 0 HP, show "PLAYER n WINS", press a key to restart.

## Engine features it leans into (and why)
- **`pset` + a pixel mask** — the heart of the game. Destructible terrain is *the*
  thing Gorillas lacked; carving craters with `pset` and reading them back for
  collision is exactly what the pixel-level API is for. (`sand.c` is the per-pixel
  idiom; this is its turn-based cousin.)
- **`noise`** — gives an organic, never-the-same battlefield for free instead of a
  hand-authored level.
- **Projectile physics in degrees** — `cos_deg`/`sin_deg` for the launch, per-frame
  gravity + wind, `dt()` for framerate independence. Direct extension of `gorillas.c`'s
  banana, plus wind that *bends* instead of just nudging.
- **`shake` + flash (`pal`/`fade`) + synth `sfx`** — the impact is the money moment;
  juice it. A charged-launch *whoosh* (rising pitch), an explosion *boom* (noise burst),
  a tiny *step* tick while walking.
- **HUD primitives** — `bar` for the power meter and HP, `line`/`pset` for the wind
  arrow and the dotted aim trail, `print_*` for turn/score. Primitives, not sprites,
  because they're UI and effects.
- Worms themselves are tiny **sprite** characters (one 16×16 slot, `pal()`-recolored
  per team so two players need only one sprite + `sprf` to face left/right).

## Controls
- **LEFT / RIGHT** — walk the active worm along the ground.
- **UP / DOWN** — raise / lower the bazooka aim angle.
- **Hold SPACE** — charge power (meter swings); **release** — fire.
- After a win: **SPACE / Z** — restart.
- Player 1 is green, player 2 is pink; the HUD names whose turn it is.

## Lean into / read
`gorillas.c` (projectile + wind + turn loop base to extend), `sand.c` (per-pixel
terrain), `lander.c` (gravity integration + soft landing), `juice.c`/`effects.c`
(`shake`/flash/ease on impact). Skip: more than one worm per side, weapon menus,
ropes/jetpacks/inventory — those are the *next* slice, not this one.
