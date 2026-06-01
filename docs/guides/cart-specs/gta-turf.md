# The game you're building: GTA — TURF WAR

## Premise
A top-down gang-turf takeover game (the San Andreas gang-territory mechanic, abstracted
into an arcade strategy-action). Your crew rolls a city block by block, fighting rival
gangs to **flip territories to your color** until you dominate the map. Keep it stylized
and arcade — gangs and turf as a colored-grid power struggle, not a crime simulator.

## Slice (locked)
**Territory-capture sandbox** on a city map: drive/walk your crew into a rival zone,
win the fight, flip the tile-region to your color; defend against counter-attacks; win =
control X% of the city. *Extends the existing `gta.c` — read it.* The capture mechanic is
the `paperio.c`/`splatoon.c` idiom on a city grid. No story missions.

## Core mechanics
- **City (tile `map()`):** divided into **territory regions**, each owned by a gang
  (colored). A region is contested when your members outnumber/defeat the rivals in it.
- **Crew:** you lead a small gang (the `crowd.c` recolor idiom); recruit more at owned
  turf; members follow and brawl/shoot rivals.
- **Capture/defend:** clear a region of rivals + hold it briefly → it **flips to your
  color** (region fill recolor via `pal`/`rectfill`). Rivals launch counter-attacks on
  your borders; the AI gangs expand too.
- **Win/lose:** dominate (own ≥ target %) to win; lose all turf = game over. A live
  territory-share bar.

## Sprites & maps
Top-down city tile `map()` (streets/buildings/lots), region ownership shown by a
translucent `fillp` color overlay per zone. Gang members = small sprites, **`pal()`
recolor per gang** (your crew vs rivals). Cars optional for fast travel between zones.

## Juice
Territory-flip sweep (a color wash + chime across the captured zone), fight dust/impact
particles, screen `shake` on big brawls, a "TURF TAKEN" `print_scaled`, a pulsing border
on contested zones, a rising share-bar with a fanfare at milestones. Audio: a hip-hop-ish
bpm bed (filtered bass + `euclid` hats), brawl thwacks, takeover chime, alarm when a
zone of yours is under attack.

## Data model
`struct Member { float x,y; int gang,hp,state; }` pool; `region[ ]` with owner + contest
meter; city grid; gang AI intents (expand/defend). Territory-share counts per gang.

## Controls
Mouse or D-pad to lead your crew / set a rally point; button to attack; click/enter a
zone to contest it. Keep it simple — crew follows the leader. (Pick mouse-lead or
direct-control; state which.)

## Lean into / read
`gta.c` (+`.cart.js`, the existing top-down city — extend it), `paperio.c`/`splatoon.c`
(territory capture/flood-fill), `crowd.c` (pal-recolored crew + y-sort), `effects.c`/
`particles.c` (brawl juice), `drummachine.c` (bed + stings). Keep it **abstract/arcade**;
skip story, pedestrians-as-victims, vehicles-as-weapons framing — it's colored-turf
strategy with gang flavor.
