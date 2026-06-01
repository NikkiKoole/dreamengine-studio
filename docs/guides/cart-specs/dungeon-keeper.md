# The game you're building: DUNGEON KEEPER

## Premise
You're the evil Keeper. Carve a dungeon out of solid rock, claim it, build rooms that
attract and serve your monsters, and repel the do-gooder heroes who come to ruin
everything. A dark, gleeful underground build-and-defend sim driven by the iconic
**slap-hand** cursor.

## Slice (locked) — 🟡 (reuses the Dune engine hard)
An underground tile grid you **designate-to-dig** (imps mine it out over time, revealing
the map), **claim** floor to extend your territory, build **~3 room types** (treasury,
lair, training room), monsters move in and work/train, and periodic **hero parties**
invade and must be fought off. Win = defeat the hero lord / survive N waves. Economy =
gold dug from seams. Cut: many room/creature types, spell trees (1–2 keeper spells max),
multiple levels, possession mode.

## Core mechanics
- **Dig & claim (the loop):** left-click-drag to **mark rock for digging**; imps path to
  it (`astar`) and mine it out, revealing fog-covered tiles and any **gold seams**.
  Claim exposed floor (imps "paint" it yours) to build on it.
- **Rooms:** drop a room type onto claimed floor (costs gold). Treasury stores gold,
  lair lets monsters live (attracts them), training room levels them up. Rooms have a
  size/benefit relationship — bigger = better.
- **Monsters with needs:** creatures wander, sleep in the lair, train, and get paid from
  the treasury (`sims.c` idiom). Unpaid/unhappy ones leave.
- **The Hand (signature):** pick up a monster or imp with the cursor and **drop (slap)**
  it somewhere — to rush diggers to a seam or fling fighters at invaders.
- **Invasions:** hero parties enter from the map edge and head for your treasury/heart;
  your monsters auto-fight, and you slap reinforcements in. Lose your Dungeon Heart =
  game over.

## Sprites & maps
Underground tile `map()` (rock/dirt/claimed-floor/gold-seam/room tiles), **fog of war**
that peels as you dig, **torch-lit `pal()`** mood (warm pools of light, dark edges).
Imps, monsters, heroes = small sprites, **`pal()` recolor** per type/faction (`crowd.c`).
The hand cursor + room-placement ghost drawn as primitives.

## Juice
Dig crumble + dust particles and a satisfying chunk sound as rock falls, gold sparkle +
rollup (`lerp`) when a seam is tapped, room-complete flourish, the **slap** (a squashy
grab/drop with a comedic thwack), torch flicker (`blink`/`pal`), combat sparks + `shake`,
a panic alarm + red pulse when the heart is attacked. Audio: a creepy organ/dungeon bed,
pick-axe taps on a `euclid` rhythm, monster grunts, an invasion horn.

## Data model (suggested)
- tile grid: type + dig-marked + claimed + fog flags (+ gold amount on seams).
- `struct Creature { float x,y; int type,hp,job,state; float pay,happy; }` pool (yours +
  invaders), `astar` pathing; `struct Room` list; `gold`, dungeon-heart hp, wave timer.
- the held-entity pointer for the Hand.

## Controls
Mouse-primary: left-drag to mark dig / claim, click a room button then drag to place,
**click-hold a creature to pick up, release to slap-drop**. Keyboard hotkeys for rooms.
Mouse is the soul of this one.

## Lean into / read
`dune.c` (**the dig/claim/build/units/economy engine to reuse**), `towerdefense.c`
(invasion waves vs your defenders), `sims.c` (creature needs/jobs), `astar.c`/
`pathfinding.c` (imp + creature routing), `crowd.c` (`pal()` recolor), `effects.c`/
`particles.c` (dig/combat juice). Skip: possession/first-person, big tech trees,
multi-level dungeons — one dungeon, dug and defended.
