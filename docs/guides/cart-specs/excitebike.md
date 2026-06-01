# The game you're building: EXCITEBIKE

## Premise
A side-view NES motocross racer with the famous **landing-angle** skill and an
**overheat** meter — plus a **track editor** so you build your own courses. Hit ramps,
control your tilt in the air, land clean to keep speed, and don't redline the engine.

## Slice (locked)
Two modes off the title: **RACE** (one track, vs CPU bikes or time) and **BUILD** (a
mouse level editor — place ramps/bumps/jumps/mud, `save_bytes` the track). The
landing-angle physics + temp meter are the depth; keep the rest light.

## Core mechanics
- **Side-scroll track** = a heightfield / tile lane the camera follows horizontally.
- **Air control (signature):** off a ramp, tilt the bike forward/back; land roughly
  **level** to keep speed — nose-down or tail-down landing = wipeout + remount delay
  (the `lander.c` angle idiom).
- **Turbo/overheat:** hold turbo for speed; the **temp gauge** climbs and redlines into
  a stall/cooldown; mud/ground type and ramps affect heat. Risk/reward.
- **Race:** lanes of CPU bikes to weave past (bump = both wobble), lap/finish, time.
- **Build mode:** mouse-place track pieces from a palette onto the lane grid; test-ride
  instantly; persist with `save_bytes` and reload.

## Sprites & maps
Bike + rider sprite set (lean-forward/level/lean-back, airborne, crash), CPU bikes via
`pal()` recolor. Track built from a tile `map()` of ramp/bump/jump/mud/flat pieces (the
editor writes the map; race reads it). Parallax crowd/sky backdrop.

## Juice
Dirt-spray particles on landings + turns, screen `shake` on hard landings/crashes, a
satisfying squash on touchdown, a flashing red temp gauge + rising alarm near redline,
wheelie wobble, "PERFECT!" on a clean big jump. Audio: an engine tone whose pitch rides
throttle (instrument + `LFO`), turbo whine, crash clatter, a chiptune race jingle.

## Data model
`struct Bike { float x,y,vy,angle,vangle,heat,speed; int state; }` for player + CPU;
track as an editable tile grid; editor cursor from `mouse_x/y`. `save_bytes` the grid.

## Controls
**Race:** right/Z = throttle, X = turbo, up/down = tilt in air, left = brake.
**Build:** mouse place/erase pieces, a piece palette, S = save / test. Toggle modes from
the title.

## Lean into / read
`lander.c` (angle-on-landing physics), `platform.c` (+`.cart.js`, side-scroll + tile
collision), `10-world.c` (tile map + camera), `typesave.c`/`12-hiscore.c`
(`save_bytes`/`save`), `effects.c`/`particles.c`. Skip: 3D, multiple championships,
bike upgrades.
