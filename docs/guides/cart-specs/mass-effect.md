# The game you're building: MASS EFFECT

## Premise
A sci-fi action-adventure in miniature: command your ship from the bridge, pick a planet
from the galaxy map, drop into a top-down shooting mission with squadmates and cover,
and steer the story through a dialogue wheel. A whole space opera, distilled to its
loop: hub → choose → mission → choice.

## Slice (locked)
🔴 The biggest concept — cut hard to: **bridge hub + galaxy-map planet select + ONE
top-down cover-shooter mission + a dialogue wheel with consequential choices.** 2–3
planets selectable but one fully built mission; a paragon/renegade-style choice that
changes the ending beat. No inventory grind, no leveling sprawl.

## Core mechanics
- **Hub (bridge):** a small explorable ship scene; talk to 1–2 crew (dialogue wheel),
  then use the galaxy map to choose a destination.
- **Galaxy map:** a few star nodes (`elite.c`-ish); pick a planet to launch the mission.
- **Mission (the meat):** top-down twin-stick-ish shooting — move + aim + fire, **duck
  behind cover** (tiles that block shots/LOS), 1–2 AI **squadmates** who follow and
  shoot, enemy waves, a boss/objective. Health + regenerating shield, a special power
  (e.g. a `pal`-flashy biotic blast / grenade).
- **Dialogue wheel:** radial choices in conversations; choices set a morality flag that
  forks the resolution.

## Sprites & maps
Bridge + mission = tile `map()` scenes (ship interior; alien base/cover layout), themed
per planet via `pal()`. Hero, crew, enemies as small sprites (`pal()` recolor, the
`crowd.c` idiom); cover drawn as solid tiles; the dialogue wheel + HUD as `fillp`/circle
UI. Galaxy map = stars (`pset`/`circ`) + node labels.

## Juice
Shooting: muzzle flash, tracers, impact + cover chips, shield-break flash + shake, a
screen-edge red vignette at low health, a cinematic slow-mo on the biotic power, enemy
hit-flash + death pop. Dialogue: wheel sweep-in, voice-blip text, a sting on a big
choice. Hub: ambient hum, holographic flicker (`blink`/`pal`). Audio: a synthy orchestral
bed (slow pad + ar/`euclid`), weapon fire, power whoosh (filter sweep), UI chimes.

## Data model
A scene/state machine (HUB / MAP / MISSION / DIALOGUE / END); `struct Actor { float x,y,
vx,vy; int team,hp,state; }` pool for mission; cover/LOS over the tile grid; a `morality`
flag + dialogue node tables; chosen planet. `save_bytes` the choice/progress (optional).

## Controls
**Mission:** WASD move, aim with mouse (or right-stick feel), Z fire, X power, hold to
take cover near a cover tile. **Hub/map/dialogue:** mouse to click crew/stars/wheel
options. State the scheme clearly.

## Lean into / read
`robotron.c` (top-down shooting + entity pools), `adventure.c` (scenes/dialogue/flags),
`elite.c` (galaxy map + ship-y framing), `crowd.c` (pal squad/enemies), `effects.c`/
`particles.c` (combat juice), `omnichord.c`/`22-filter.c` (orchestral-synth bed). Skip:
RPG inventory/leveling, multiple full missions, open exploration — hub + one great
mission + a choice that matters.
