# The game you're building: RIVER CITY (RANSOM)

## Premise
An open-ish beat-'em-up RPG: roam a small city of connected zones, brawl roving gangs,
collect the cash they drop, and **spend it at shops on stats and new moves**. Same
punchy combat as a belt brawler, but with light progression and a goal (clear the
zones / beat the boss gang). The "random" twist: enemy spawns and drops are procedural,
so each run differs.

## Slice (locked)
**Build on the brawler engine from `final-fight.c`** (read it first; reuse the combat,
y-sort, 16×32 sprites). Add: a small **overworld of ~4 zones** connected by exits, a
**shop** (mall/diner) screen, **4 stats** (HP, STR, DEF, STAMINA), 2–3 buyable special
moves, and procedural enemy placement. Win = beat the boss in the final zone.

## Core mechanics
- **Zones:** each is a scroll-locked brawl arena; walk off an edge to the next zone
  (a tiny zone map shows where you are). Enemies wander; some drop **coins/items**.
- **Combat:** inherited from the brawler. Damage scales with STR; you take less with DEF;
  STAMINA gates running/specials. Buying a move unlocks e.g. a dragon-kick or dash punch.
- **Shops:** spend cash on stat boosts, food (heal), and moves. Simple menu screen.
- **Procedural:** enemy types/counts/drops per zone rolled with `rnd`/`noise` each entry.

## Sprites & maps
Reuse the brawler's 16×32 hero/enemy set (more enemy "gangs" = `pal()` palette themes).
Each zone = a themed scrolling tile `map()` (downtown, park, mall exterior, docks). Shop
interiors = a small static map + item icons.

## Juice
All the brawler juice (hitstop/shake/flash/combos) **plus** coins that pop and arc to a
counter (`lerp` rollup + coin SFX), level-up flourish on a stat buy, zone-clear fanfare,
a different musical theme per zone.

## Data model
Brawler `Fighter` pool + a `player` with `cash, hp_max, str, def, stamina, moves[]`,
`zone`, and a zone-graph (`exits[zone][dir]`). Persist a best clear with `save_bytes`.

## Controls
As the brawler (Z attack / X jump / Z+X special). In shops: arrows + Z select / X back.
Walk into a zone exit to travel.

## Lean into / read
`final-fight.c` (the brawler engine you're extending), `crowd.c`, `mule.c`/`papers.c`
(shop/stat menus), `effects.c`/`juice.c`. Skip: a huge map, NPC quests, free-roam
driving — keep zones as arenas with a light RPG skin.
