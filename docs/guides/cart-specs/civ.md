# The game you're building: CIV (tiny)

## Premise
A pocket-sized **4X** — explore, expand, exploit — on a fogged tile world. You start
with one **settler** and one **warrior** in a sea of black fog. Walk the settler out,
**found a city**, and watch the unknown peel back tile by tile. Cities grow a
**population bar** and pump out one new unit every few turns; a single **research bar**
fills as your empire works, and when it caps it unlocks **Irrigation** — the one
improvement, which permanently boosts every city's growth. There's no rival army, no
diplomacy, no combat tree: the antagonist is the **map itself** and the clock. Win by
founding **3 cities** before turn 30; that's the whole arc.

This is deliberately the *gentle* corner of Civilization — the joy of the fog lifting
and a settlement taking root — not the war game. It's a turn-based, mouse-driven toy
that should feel calm, legible, and quietly satisfying.

## The locked slice (build exactly this)
- A small tile **grid** (terrain: water / grass / forest / mountain), generated with
  `noise2`, drawn as a real chunky map.
- **Fog of war** revealed in a radius around every owned unit and city; unseen tiles
  stay black. Tiles once seen stay revealed (explored memory).
- A **settler** founds **1–3 cities** on grass/forest (`B` or a FOUND button).
- Cities **grow a population bar** every turn (faster on good surrounding terrain, and
  faster still once Irrigation is researched). When the bar fills, population ticks up.
- Cities **produce one unit every few turns** (a warrior or, occasionally, another
  settler) from accumulated production.
- **Units move one tile per turn** (orthogonal/diagonal) — click a unit, then click an
  adjacent revealed tile.
- A **single research bar** fills from total city output each turn; filling it unlocks
  the one improvement (**Irrigation**) with a fanfare.
- **End Turn** advances everything: production, growth, research, fog, reset moves.
- **Win** at 3 cities; soft **lose** if turn 30 passes without 3 cities. Restart loop.

**Cut (do not build):** diplomacy, multiple techs, combat depth / enemy civ, wonders,
trade, unit stacking, Dijkstra pathing (single-step moves only).

## Engine features it leans into (and why)
- **A real tile `map`-style grid** drawn with primitives (`rectfill` + a little
  terrain decoration), because the world *is* the game. Fog is just "draw black if not
  seen", reveal is a radius stamp — the cheapest possible 4X feel.
- **`noise2` terrain** so every restart is a fresh continent worth re-exploring.
- **`bar`** for the three things that are fundamentally meters: city **population**,
  city **production**, and **research**. It's the exact primitive for this.
- **Mouse-first input** (`mouse_pressed`, `mouse_x/y`) mapped to tiles, with `B`/`E`
  keyboard shortcuts — the Civ idiom of "select then click".
- **Juice at the earned moments**: a soft `note`/`chord` when a city is founded, a
  `strum` fanfare when research completes, a tiny `shake` + flash when a unit pops out,
  a `blink` "GO" pulse on units that still have a move. A gentle `every`-driven ambient
  pad ties the turns together without nagging.
- **`save`** the best result (fewest turns to 3 cities) so runs accrete meaning.

## Controls
- **Click a unit** to select it; **click an adjacent revealed tile** to move there
  (one tile/turn).
- **B** (or the FOUND button) — settler founds a city on its tile.
- **E** (or the END TURN button) — advance the turn: grow cities, build units, fill
  research, re-reveal fog.
- After win/lose: **click** or **A** to start a fresh map.

## Lean into / read
`fourx.c` (the closest cousin — fog, settler/warrior, found-city, end-turn, side panel)
and `advancewars.c` (cursor + tile click idiom, building-recolor, win banner). Skip the
combat matrix, Dijkstra reach, and rival AI from those — this slice has no enemy.
