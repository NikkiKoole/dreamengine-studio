# The game you're building: CANNON FODDER

## Premise
A top-down run-and-gun squad-tactics game. You lead a tiny squad across a hostile
landscape, mouse-driving them to move and shoot, blowing up huts and gun nests. Soldiers
**die permanently** and survivors carry between missions — the bittersweet hook. Tense,
darkly funny, explosive.

## Slice (locked)
3 short missions on scrolling tile maps; squad of up to ~5; **mouse command** (move /
fire / grenade); permadeath roster persisted between missions; objective = destroy all
targets / reach the exit. Promotions for survivors. No base management.

## Core mechanics
- **Command:** left-click ground = the squad moves there (leader + followers in a loose
  trail); left-click an enemy/target = fire toward it; **right-click = throw grenade/
  rocket** (arc + AoE). Crosshair from `mouse_x/y`.
- **Terrain (tile `map()`):** water (impassable), trees (cover/block LOS), buildings/
  huts (destroyable, often the objective), open ground. `touching_map`/`tile_at` for
  collision + cover.
- **Enemies:** patrolling/guarding soldiers that spot and shoot you (LOS), gun nests,
  exploding huts. Simple chase/shoot AI.
- **Permadeath:** a hit can kill a soldier instantly — survivors gain rank (better aim/
  HP). Lose the whole squad = mission fail.

## Sprites & maps
Tiny 16×16 soldiers (your squad vs enemies) recolored via `pal()` (`crowd.c` idiom),
walk + fire frames. Scrolling tile maps per biome (jungle/snow/desert via `pal()`
recolor of a shared tileset). Explosions/craters drawn over the map.

## Juice
Big satisfying explosions (expanding `circfill` + `fillp` smoke + `shake` + flash),
muzzle flashes, blood/dirt particles, screen shake scaled to blast size, a grim little
"man down" sting + a name on the casualty list, rank-up flourish. Audio: gunfire bursts,
grenade whump, ambient wind/birds bed, a jaunty militaristic ditty (`euclid` snare).

## Data model
`struct Soldier { float x,y; int hp,rank,alive; }` roster (persisted via `save_bytes`);
`struct Enemy`, `struct Bullet`, `struct Grenade`, `struct Particle` pools
(`robotron.c` idiom). Mission target list; `astar`-style pathing optional for followers.

## Controls
**Mouse:** left = move-to / fire-at, right = grenade. Optional: SPACE to halt, number
keys to select an individual. Keep mouse primary.

## Lean into / read
`robotron.c` (entity/bullet pools + top-down combat), `crowd.c` (pal soldiers + tile
map + y-sort), `astar.c`/`pathfinding.c` (follower routing), `effects.c`/`particles.c`
(explosions/shake), `towerdefense.c` (map targeting). Skip: cover-system depth,
turn-based anything — it's real-time arcade tactics.
