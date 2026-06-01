# The game you're building: TRADE WINDS (spice trader)

## Premise
An age-of-sail trading game. Sail your ship between ports on a sea map, buy goods cheap
and sell them dear as prices swing, weather storms, and fend off (or flee) pirates.
Upgrade your ship and grow a fortune before the season ends. A trading loop with a
little salt-spray adventure.

## Slice (locked)
**Trading loop + simple pirate/storm encounters + ship upgrades**, ~5–6 ports on a sea
map. Goal = most gold by day/season N. **Reuse the Druglord economy core** (read
`druglord.md`/that cart): markets, fluctuating prices, port skews, capacity. The sailing
+ encounters are what's new.

## Core mechanics
- **Sea map (tile `map()`):** ports as nodes on water; sail your ship sprite toward a
  chosen port — travel takes days and may trigger an encounter.
- **Markets:** each port buys/sells ~6 goods (spices, silk, rum, etc.) at prices that
  drift + react to events; cargo limited by hold size.
- **Encounters:** **storms** (lose time/cargo unless you ride it) and **pirates**
  (fight with cannons in a tiny side skirmish, or flee — speed vs their speed). Light,
  not a full combat game.
- **Upgrades:** spend gold on bigger hold, faster hull, more cannons.
- **Win/lose:** fortune goal by deadline; sunk/broke = game over. `save()` best.

## Sprites & maps
Sea tile `map()` (water/coast/port), ship sprite (yours + pirate, `pal()` recolor),
port towns as little sprite clusters. Market + encounter UI as `fillp` panels. Optional
parallax waves / sailing wake particles.

## Juice
Sailing wake + gull cries, a creaking ship, money rollup on a good trade (`lerp` +
coin), storm = screen `shake` + rain particles + thunder, cannon fire flashes + splashes,
"PORT REACHED" flourish, price arrows (▲▼) with flash. Audio: a rolling sea-shanty bed
(bpm + `every`), wave wash, thunder (low noise + shake), cannon boom, trade chime.

## Data model
`struct Port { int x,y; int price[6]; }` set; `player` gold + cargo[6] + hold/speed/guns
+ day + at_port; encounter resolver; price re-roll per day (base × port skew × `noise`).
Persist best with `save`.

## Controls
Mouse-primary: click a port to set sail, click goods to buy/sell, click upgrade/encounter
choices. Keyboard fallback (arrows + Z/X). Pirate skirmish: aim/fire or flee.

## Lean into / read
`druglord.md` + that cart (the economy backbone to reuse), `mule.c` (trade sim),
`elite.c` (ports + trading + a light skirmish), `fourx.c` (sea map), `effects.c`/
`particles.c` (storm/cannon juice), `omnichord.c`/`drummachine.c` (shanty bed). Skip:
crew management, full naval combat, exploration fog — trade loop + flavorful encounters.
