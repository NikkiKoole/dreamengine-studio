# The game you're building: XCOM — ENEMY UNKNOWN

## Premise
Turn-based squad tactics against aliens: move your soldiers across a procedurally
generated battlefield, use cover, take percentage-chance shots, and try to wipe the
threat without losing your people — who you keep, name, and level between missions.
Tense, lethal, readable.

## Slice (locked)
The **tactical battle** is the whole game (drop the geoscape/base building). One
**procedurally generated** map per mission, a squad of 4 vs alien pods, a **2-action**
turn system, cover + aim %, overwatch. Between missions: a light **squad screen** (heal,
rename, one promotion) and a persistent roster. Win a mission = kill all aliens; lose a
soldier = gone for good (`save_bytes` roster).

## Core mechanics
- **Turn/actions:** each soldier gets 2 actions/turn (move, move-move dash, shoot,
  overwatch, reload, hunker). Enemies act on their turn.
- **Cover & aim:** tiles give half/full cover (drawn as little shields); **hit % = base +
  aim − range − enemy cover**; show the forecast (hit/crit/dmg) before firing.
- **Line of sight / fog:** you see what your soldiers can see; aliens may be revealed as
  a pod that "activates" and scatters to cover.
- **Overwatch:** end a turn watching — first enemy to move in sight gets reaction fire.
- **Procedural map:** scatter cover/walls/props on a tile grid with `rnd`/`noise`; place
  squad at one edge, alien pods spread across.

## Sprites & maps
Tile `map()` battlefield (floor/wall/cover/door), generated per mission and themed via
`pal()` (urban/abandoned/alien). Soldiers + aliens = small sprites, **`pal()` recolor**
per soldier (and alien type); selected-unit highlight, move-range + shot-line overlays
via translucent `fillp`.

## Juice
Camera/cursor snap to the acting unit, shot tracer + muzzle flash + impact, hit-flash
(`pal` white) + HP chips, a crit "BOOM" `print_scaled` + `shake`, miss "whiff" dust, a
tense low drone that swells on enemy turns, an alarm sting on pod activation, a somber
note on a death (with the soldier's name). Reaction-fire snap on overwatch.

## Data model
`struct Soldier { int x,y,hp,aim,ap,state; char name[12]; int alive,rank; }` roster
(persist `save_bytes`); `struct Alien` pool; cover/LOS helpers over the tile grid; a
turn/AP/team state machine. Aim/damage formulas in one place.

## Controls
Mouse-primary: click a soldier, click a tile to move (range shown), click an enemy to
open the shot forecast, confirm to fire; buttons for overwatch/hunker/reload/end-turn.
Keyboard fallback: arrows + Z/X.

## Lean into / read
`fourx.c` (turn/tile/mouse tactics), `rogue.c` (grid + LOS/procedural), `pathfinding.c`/
`astar.c` (move ranges), `effects.c`/`particles.c` (shots/shake/flash), `typesave.c`
(`save_bytes` roster). Skip: geoscape, base building, research, item economy — battle +
roster only.
