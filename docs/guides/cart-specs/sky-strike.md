# The game you're building: SKY STRIKE (top-down shoot-em-up)

## Premise
A vertical-scrolling **top-down airplane shmup** — Raiden / 1943 / Tyrian. Your fighter
hugs the bottom of the screen, the world scrolls down past you, waves of enemy planes
pour in from the top, and the hook is the **weapon-upgrade progression**: shoot-down
power-ups level your gun through tiers and bolt on side **options/wingmen** and a smart
**bomb**, building toward a screen-filling spread — then a boss caps the run.

## Slice (locked) — 🟢
**One scrolling stage that loops/escalates into a boss.** A player plane with smooth
8-way movement; a tiered main weapon (collect `P` pods to climb levels); a screen-clear
bomb (collect `B`); 4–6 enemy types on scripted waves; readable enemy bullets you must
dodge; a HUD (lives, bomb count, weapon level, score); death drops you a weapon tier (not
to zero) so it never feels hopeless; a boss with a health bar to end the loop; `save()`
the hi-score. Single player, keyboard/gamepad. Skip: branching paths, a shop, two-player,
multiple distinct stages — escalate the one stage instead.

## Core mechanics
- **Scrolling world:** the backdrop is a tile `map()` (ocean/islands/airbase) that scrolls
  *down* — advance the map camera each frame; the player is screen-space, not world-space.
- **Player:** 8-way move clamped to screen, autofire main gun (hold or always-on), a bomb
  on its own button. Tight hitbox (the classic "shoot the 1-pixel core") — collide on a
  small inner box, not the whole sprite.
- **Weapon tiers (the depth):** main gun climbs L1→L5 — single shot → twin → 3-way spread →
  4-way + faster → 5-way; each extra `P` past max adds a trailing **option/wingman** that
  mirrors your fire. Branch a second pod color for a different weapon (straight vs. spread)
  if cheap.
- **Bombs:** clears all on-screen bullets + heavy damage, brief invuln, screen flash +
  `shake`. Limited stock.
- **Enemies & bullets:** fixed typed **pools** (no heap) — popcorn flyers on sine paths,
  poppers that fire aimed shots (`angle_to` the player), turrets on the scrolling ground,
  plus a boss. Enemy bullets are their own pool; cull off-screen.
- **Waves:** a small scripted timeline (spawn type N at time T from x) — `every`/`beat` or
  a frame timer drives it; difficulty ramps until the boss spawns.

## Sprites & maps
Player plane (banks left/level/right — 3 frames, or `sprf` flip + a tilt frame), 4–6 enemy
craft, bullets (yours vs. theirs in different colors), pickups (`P`/`B` pods), explosion
frames, a multi-slot **boss** (a 16×32 or 32×32 craft = stacked slots). `pal()` recolors
one enemy sprite into a squadron of variants. Scrolling sea/island terrain as a tile
`map()`; a parallax cloud layer over it sells the altitude.

## Juice
Muzzle flashes, shell-casing/spark particles, chunky **explosions** with `shake` on big
kills, hit-flash enemies white via `pal()`, screen flash + heavy shake on bombs, a
brief **hit-stop** when the boss dies, scrolling-fast starfield/whitecaps for speed,
"POWER UP!" pop and a rising chime as the weapon tiers up, a low-health red vignette/alarm.
Audio: a driving engine drone (`instrument` + `LFO`), a punchy pew per shot (keep it short
so autofire doesn't muddy), explosion noise bursts, a power-up arpeggio, a boss-warning
siren, and a chiptune action loop under it all.

## Data model
`struct Ent { float x,y,vx,vy; int type,hp,frame; bool alive; }` pools for enemies, your
bullets, enemy bullets, pickups, explosions. `struct Player { float x,y; int lives,bombs,
wlevel,options; float iframes; }`. A wave script as a static array of `{t,type,x}`. Boss
as its own struct with a phase counter. `save()` the hi-score (one int slot).

## Controls
Arrows / WASD = fly (8-way), Z = bomb (gun autofires), X = focus/slow precision move
(optional), SPACE/ENTER = start / restart. Gamepad maps the same. Keep it one-thumb simple.

## Lean into / read
`robotron.c` / `vampire.c` / `boids.c` (typed entity **pools** + bullet management),
`galaga.c` & `xevious.c` (the existing shooters — read them, then differentiate on the
**upgrade loop** and the scrolling tile world they lack), `10-world.c` (scrolling tile
`map()` + camera), `particles.c` / `effects.c` / `juice.c` (explosions, shake, hit-flash),
`20-instruments.c` / `21-lfo.c` (engine drone + weapon voices), `12-hiscore.c` (`save()`).
Skip: an entity system / coroutines (none exist — drive waves off a frame/`beat` timeline),
world-space player physics (player is screen-space; only the map scrolls).
