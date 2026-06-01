# The game you're building: DUNE (base building)

## Premise
The harvest-and-build heart of the RTS that became Command & Conquer. On a desert tile
map, **harvesters** crawl out to spice fields, haul it back to a refinery for credits,
and you spend those credits building structures and units — while a rival faction does
the same and probes your base. Mouse-driven, readable, satisfying economy loop.

## Slice (locked)
**Economy + build + light defense**, not a full war. One map, you vs one CPU faction.
Buildings: **refinery, factory, turret, power**. Units: **harvester, a couple combat
units**. Win = destroy the enemy refinery/base (or survive N minutes with the bigger
economy). The spice→credits→build loop is the depth.

## Core mechanics
- **Spice (tile `map()`):** spice tiles hold an amount that depletes as harvested and
  slowly regrows; harvesters **auto-path** (`astar`) to the nearest spice, fill up,
  return to the refinery → credits.
- **Build:** a sidebar build menu; placing a building deducts credits and drops it on a
  buildable tile; the factory produces units (queue + timer); turrets auto-fire at
  enemies in range.
- **Command:** left-click select (unit/group/building), left-click ground = move,
  left-click enemy = attack; drag-box select for groups. Crosshair from `mouse_x/y`.
- **Enemy AI:** harvests, builds, and sends the odd attack wave — enough to create
  pressure and a reason for turrets/units.

## Sprites & maps
Desert tile `map()` (sand/rock/spice, spice shimmer via `pal()`/`blink`); buildings +
units as sprites, **`pal()` faction recolor** (your color vs enemy). Selection rings,
health bars, and build-placement ghost drawn as primitives. Sandworm cameo optional.

## Juice
Harvester "full" sparkle + credit rollup (`lerp`) with a chime, build-complete flash +
sound, turret muzzle flashes, explosions (`circfill`+`fillp`+`shake`) on destroyed
units/buildings, spice-bloom puff, low-power warning. Audio: ambient desert wind bed,
build/ready stings, weapon fire, a rumble when something big explodes.

## Data model
`struct Unit { float x,y; int type,hp,team,state; int target; }` pool; `struct Building`
list; spice amount per tile in the map/aux grid; `credits`, `power`, build queue.
`astar`-style pathing for harvesters/units.

## Controls
Mouse: click/drag select, click to move/attack, click sidebar to build then click to
place. Keyboard fallback for build hotkeys. Mouse primary.

## Lean into / read
`fourx.c` (tile map + mouse units + factions), `astar.c`/`pathfinding.c` (harvester +
unit routing), `towerdefense.c` (turrets + waves), `crowd.c` (pal-recolor units),
`effects.c`/`particles.c` (explosions). Skip: tech trees, multiple factions, fog,
campaign — one map, the economy loop nailed.
