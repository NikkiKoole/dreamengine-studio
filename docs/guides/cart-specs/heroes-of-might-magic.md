# The game you're building: HEROES OF MIGHT & MAGIC

## Premise
A turn-based fantasy strategy game in miniature. Lead a hero across an adventure map
collecting resources and growing an army, build up your town's creature dwellings, and
fight enemy armies in a separate **stack-based** tactical battle. Capture the enemy town
or defeat their hero to win. The magic is the loop: **explore → recruit → build → do
battle → repeat.**

## Slice (locked) — 🔴 cut hard
Three systems, each trimmed: **(1) adventure map** with one hero (movement points/day),
resource/treasure pickups, a flaggable mine, your town + an enemy town/hero; **(2) town
screen** — build ~3 dwellings, recruit creatures into up to 5 army stacks; **(3) stack
battle** on a small grid. One small continent, you vs one CPU, ~4 creature types, gold +
ONE resource. Win = take the enemy town or rout their hero. Cut: hero skill trees, big
spell books (1–2 combat spells max), multiple towns, the 7-resource economy.

## The distinctive mechanic — STACKS (vs one-unit-per-tile)
A battle unit is a **stack**: one icon labeled with a count ("x22"). Damage is computed
against the stack and **removes whole creatures** from it (22 → 17); a stack with HP left
in its top creature fights on. Include ranged units (no melee retaliation, ammo
optional), **retaliation** (defender hits back once per round), and simple initiative
(speed order). This is what separates it from `advancewars`; the battle *grid + forecast
+ HP/shake juice* can otherwise reuse the [advance-wars](advance-wars.md) engine.

## The three modes (a clean state machine)
- **ADVENTURE (default):** tile `map()` overworld; hero has **movement points** spent by
  walking, refreshed each day. Step onto pickups (gold/resource piles, treasure,
  artifacts), a **mine** (flag it → daily income), your **town** (enter), or the enemy
  hero (→ battle). Fog of war optional. END DAY refreshes movement + ticks the CPU hero.
- **TOWN:** build dwellings (cost gold + resource) that unlock creature recruitment;
  recruit available creatures into your hero's army stacks. A weekly tick grows dwelling
  populations.
- **BATTLE:** triggered when heroes/armies meet. A small grid; your stacks vs theirs;
  move/attack with the stack rules above; 1–2 hero spells (e.g. bolt, bless). Win →
  back to adventure (loser's hero/army removed); lose → game over.

## Sprites & maps
Adventure map = tile `map()` (grass/forest/mountain/water/road, town, mine, resource
piles) — terrain blocks/cost movement. Hero + creatures = small sprites, **`pal()`
recolor** per owner and per creature tier (the `crowd.c` trick). Battle grid = its own
tilemap + stack icons with a count label (`print` the number on the sprite). Town screen
= a `fillp`-panel building menu + creature portraits.

## Juice
Adventure: hero walk-bob, pickup sparkle + gold rollup (`lerp`) with a chime, mine-flag
flag-raise, fog peel. Battle: attack lunge, HP/creature-count pops, melee `shake` +
spark, ranged arrows, a spell flash (`pal`/particles), a morale/luck sparkle, victory
fanfare. Town: build-complete flourish, recruit chime. Audio: a wandering fantasy-bard
bed on the map (lute-y instrument + `every`), a martial battle theme, spell whooshes,
UI chimes.

## Data model (suggested)
- `struct Hero { int x,y, move, gold, res; Stack army[5]; }` (player + CPU).
- `struct Stack { int type, count; }`; `creature_def[]` = {hp, atk, dmg, speed, ranged}.
- adventure: tile grid + object list (pickups/mines/towns); day counter.
- town: built-dwellings bitset + available-to-recruit counts.
- battle: a separate grid + a copy of both armies as battle stacks with positions/HP.
- mode enum (ADVENTURE/TOWN/BATTLE). Persist a best result with `save`/`save_bytes`.

## Controls
Mouse-primary: on the map click a tile to move the hero (path/range shown), click your
town/mine/pickup to interact, click the enemy to engage. Town + battle are click-driven
(build/recruit buttons; click a stack then a target). Keyboard fallback: arrows + Z/X,
SPACE/END DAY to pass the turn.

## Lean into / read
`advancewars.c` (**the battle engine to reuse** — grid, damage forecast, terrain, HP
pops, turn flow), `fourx.c` (adventure map + towns + mouse + turn structure), `rogue.c`
(grid exploration + fog), `crowd.c` (`pal()` army recolor), `drummachine.c`/`omnichord.c`
(fantasy music beds + stings). Skip: skill trees, full spell book, multi-town empires,
seven resources — explore → recruit → build → fight, nailed small.
