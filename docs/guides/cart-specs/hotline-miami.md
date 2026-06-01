# The game you're building: HOTLINE MIAMI

## Premise
A top-down, lightning-fast, ultra-lethal action game. Storm a building floor and clear
every guard room by room with fists, a bat, or whatever gun you pry off a corpse —
but you die in ONE hit too, so it's all twitch, nerve, and instant retries. Neon, blood,
and a pounding synthwave bassline. The feel is maximal; the design is minimal.

## Slice (locked) — 🟡 contained
**One-hit-kill both ways + instant restart** is the whole hook. One building floor (or a
short stack of 2–3) as a tile `map()` of rooms/walls/doors; ~3 weapon types (melee +
two guns); 2 enemy types (melee thug + pistol guard) with vision + sound aggro; combo/
score; clear all enemies → walk to the exit. Cut: masks/abilities, story, sprawling
levels. Pixel-art violence stays stylized.

## Core mechanics
- **Lethality + restart:** the player and every enemy die in one hit. Death is immediate
  → `R` (or auto) restarts the floor in a blink. Make the restart *snappy* — it's the
  addiction.
- **Weapons:** start with a **melee** (bat/knife, fast swing, instant kill in reach).
  **Pick up** dropped guns (pistol/shotgun/uzi) with **limited ammo**; out of ammo →
  **throw** the weapon to stun a guard, then melee-finish. Guns are loud (see sound aggro).
- **Door-slam (signature):** bursting a door into a guard standing behind it knocks them
  down for a free kill.
- **Enemy AI:** patrol routes + a **vision cone** (LOS over the tile map — reuse cannon
  fodder's LOS) + **hearing** (a gunshot pings nearby guards to investigate/charge).
  Telegraph before they fire.
- **Score/combo:** chaining kills quickly racks a **multiplier**; a fat end-of-floor
  tally. Best score saved.

## Sprites & maps
Top-down building floor = tile `map()` (floor/wall/door/blood-decal), themed via `pal()`.
Player + guards = small sprites (walk + attack frames), **`pal()` recolor** per type
(`crowd.c` idiom); 8-direction facing. Weapons as little ground sprites. Vision cones
drawn as translucent `fillp`/`tri` fans.

## Juice (this IS the game)
A hot **neon palette** (`pal()`), heavy screen `shake` + a quick **zoom/chroma punch**
on each kill, **blood pools + splatter** that persist on the floor (draw decals with
`pset`/`rectfill`/`circfill` into a layer), gibs/casings particles, hit-stop on a kill,
a combo-multiplier `print_scaled` that pops, a slow-mo beat on the last kill of a room.
**Audio is the headline:** a driving synthwave bed — a fat filtered bass (custom
`instrument` + `FILTER_LOW` + `LFO_CUTOFF`), an arp, and `euclid` drums via the synth —
plus meaty melee thwacks, loud gunshots (filtered noise), door slams, and a bass-drop
sting on floor-clear. Lean on the `moog`/`22-filter`/`drummachine` carts.

## Data model (suggested)
- `struct Ent { float x,y, dir; int type, alive, state; float telegraph; int weapon, ammo; }`
  pool for player + guards (`robotron.c` idiom).
- `struct Bullet`, `struct Pickup`, `struct Particle`, plus a **blood-decal** buffer.
- floor as a tile grid (walls/doors); patrol paths; per-guard vision/hearing state.
- score + combo + multiplier timer; current floor; best score (`save`).

## Controls
Top-down, **mouse-aim** (twin-stick feel): WASD move, mouse to aim/face, **left-click =
attack/fire**, walk over a weapon to pick it up, **right-click or a key = throw** the
weapon, a key to slam-open a door, **R = restart**. Keep the restart instant.

## Lean into / read
`cannonfodder.c` (top-down LOS + guns + tile map — closest cousin), `robotron.c`
(twin-stick + entity/bullet pools), `crowd.c` (`pal()` recolor sprites), `effects.c`/
`juice.c`/`particles.c` (shake/flash/blood/hit-stop), `moog.c`/`22-filter.c`/
`drummachine.c` (**the synthwave bed — go big here**), `maze.c` (room layouts). Skip:
masks, story, vehicle bits, huge level count — one savage, replayable floor with a
killer soundtrack.
