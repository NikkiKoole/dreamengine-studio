# The game you're building: ADVANCE WARS

## Premise
A bright, chunky turn-based tactics game on a tile grid. Two armies take turns moving
units, capturing buildings, and fighting over terrain that grants defense. Capture the
enemy HQ (or rout them) to win. Clean, readable, and satisfyingly crunchy.

## Slice (locked)
**One hand-made map**, you vs CPU, ~5 unit types (infantry, mech, recon, tank,
artillery) + buildings (HQ, city, factory). Win = capture enemy HQ or destroy all units.
No fog, no COs, no campaign. *Differentiate from the existing `fourx.c`* by leaning into
the **damage table + terrain-defense + capture** crunch, not 4X economy.

## Core mechanics
- **Turn structure:** select a unit → highlight reachable tiles (movement cost by terrain
  via a small BFS/Dijkstra) → move → act (attack adjacent/in-range, capture, or wait).
  END TURN hands to the CPU.
- **Combat:** a unit-vs-unit **damage matrix** (e.g. tanks crush infantry, artillery
  hits at range but can't counter, mechs beat tanks up close); damage scales with
  attacker HP and **defender's terrain stars**. Show forecast % before confirming.
- **Capture:** infantry/mech stand on a building to capture over 1–2 turns; factories
  build new units from income; HQ capture = instant win.
- **CPU:** simple but competent — capture nearby buildings, gang up on weak/valuable
  targets, defend the HQ.

## Sprites & maps
Tile `map()` terrain (plains/forest/mountain/road/sea/buildings) — defense + move cost
ride on the tile (consider `fget`-style flags or a lookup). Units = small sprites,
**`pal()` recolor per army** (red vs blue), idle bob + a fire/hit anim. Reachable-tile
and attack-range overlays drawn with translucent `fillp` rects.

## Juice
Snappy cursor + move arrow, unit "fight" cut-in flash on attack (shake + spark), HP
chips popping off, capture progress ring, terrain-star popup, a victory fanfare. Audio:
move blip, cannon boom (low noise + `shake`), capture jingle, a martial bpm bed, turn-
change sting.

## Data model
`struct Unit { int x,y,type,hp,team,moved,acted; float cap; }` pool; terrain grid + a
`terrain_def[]`/`move_cost[]` table; the damage matrix `dmg[atk][def]`. Turn/team state.

## Controls
Mouse-primary: click a unit → click a destination → pick an action from a small menu
(attack/capture/wait). Keyboard fallback: arrows + Z/X. END TURN button.

## Lean into / read
`fourx.c` (turn-based tile tactics, mouse, units, capture — read it, then go crunchier
on combat), `rogue.c` (grid movement/pathing), `effects.c` (attack flash/shake),
`drummachine.c` (martial bed + stings). Skip: fog, COs, economy depth, multi-map campaign.
