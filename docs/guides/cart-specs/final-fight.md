# The game you're building: FINAL FIGHT

## Premise
A linear arcade belt-scroll beat-'em-up. Walk right through a city stage, fists up,
clobbering waves of thugs in scroll-locked arenas until a boss. Big chunky 16×32
sprites, weighty hits, lots of juice. One playable fighter (optional 2-player co-op).

## Slice (locked)
2 stages (street → warehouse) + a boss each. One hero, 3–4 enemy types via `pal()`
recolor (grunt, knife, fat bruiser, boss). No RPG/shops (that's River City's job —
this is the pure arcade engine). This cart IS the reusable brawler engine; River City
will build on it.

## Core mechanics
- **Belt-scroll plane:** characters have x, y (depth) and a z for jumps; **y-sort** for
  overlap (the `crowd.c` idiom). Camera scrolls right, **locks** when a wave spawns and
  releases when cleared ("GO →" arrow on clear).
- **Combat:** punch combo (3-hit → knockdown), jump kick, grab + throw on contact, a
  panic/special that costs a sliver of health. Hitboxes via `boxes_touch` on the active
  frames. Hitstop + knockback + flash on every connect.
- **Enemies:** approach on the belt, telegraph attacks, can be juggled; weapons
  (pipe/knife) dropped on death, pick up and swing; breakable barrels drop food (heal).
- **Lives/health bar**, continues, score.

## Sprites & maps
- Hero + enemies = a 16×32 set (idle, walk, 3 punch frames, kick, hurt, down, grab) —
  enemy variety from `pal()` shirt/skin swaps. Boss is bigger (use two stacked + wider).
- Stage backdrop = scrolling tile `map()` (street, then warehouse) with a parallax far
  layer; foreground props (lamp posts, crates) drawn after actors for depth.

## Juice
Hitstop on impact, screen `shake` on heavy hits/throws, white hit-flash (`pal`), impact
"POW" stars, sweat/dust particles, knockdown bounce + squash, slow-mo on a boss kill,
combo counter `print_scaled`. Audio: meaty punch thwacks (filtered noise + low tone),
whiffs, enemy grunts, barrel smash, a driving stage bassline (bpm + `every`), boss sting.

## Data model
`struct Fighter { float x,y,z,vx,vy,vz; int team,type,hp,state,anim; float stun; }`
pool; player-controlled index; `state` machine (idle/walk/attack/hurt/down/grab). Wave
tables per arena. Weapon/pickup/particle pools (`robotron.c` idiom).

## Controls
D-pad/WASD move (x = walk, y = depth); **Z = attack** (tap combo), **X = jump**,
Z+X = special, toward+grab on contact. Player 2 on arrows for co-op (optional). Gamepad-friendly.

## Lean into / read
`crowd.c` (16×32 two-slot sprites, pal recolor, y-sort), `fighter.c` (existing brawler —
read it, then surpass it), `effects.c`/`juice.c`/`particles.c` (hitstop/shake/flash),
`drummachine.c` (stage groove + hit SFX). Skip: shops, stats, overworld, branching.
