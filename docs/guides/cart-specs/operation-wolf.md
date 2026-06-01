# The game you're building: OPERATION WOLF

## Premise
An on-rails light-gun arcade shooter. A **mouse crosshair** roams an **auto-scrolling**
scene while enemies pop up, advance, and shoot back; you gun them down before they hit
you. Limited ammo you **reload by shooting supply crates**, a heavy rocket on the side,
and civilians you must NOT shoot. Three themed stages, wave-based, score-chasing. It
should feel loud, kinetic, and juicy.

## Core loop
1. The scene scrolls on a rail; enemies spawn from cover / the treeline and telegraph
   before firing (a wind-up frame + a tick) so the player can react.
2. **Left mouse = machine-gun** (limited magazine), **right mouse = rocket** (scarce,
   big AoE). Aim is the mouse crosshair; fire hits whatever's under it.
3. **Reload-by-shooting (signature):** ammo/rocket **crate pickups** drift through —
   shoot one to refill that magazine. Running dry = empty-mag click until you do.
4. Enemy bullets/grenades chip your **health bar**; **shooting a civilian/hostage**
   costs health or score. Survive the waves → advance stage → final score, `save()` best.

## Stages (sprites + maps)
Three auto-scrolling levels themed by a scrolling tile `map()` + `pal()` recolor:
**jungle → enemy camp → airfield**. Each ramps enemy count/speed and introduces a type.
Background scrolls via `camera()`; optionally parallax a far layer. A light **boss**
(a turret/chopper with a health bar) to cap stage 3 — keep it simple if it fits.

## Enemy / actor types (sprites, `pal()` for variety)
- **Grunt** — runs in, shoots after a telegraph.
- **Thrower** — lobs a grenade (arcs in, explodes on a timer → AoE).
- **Armored** — takes several hits, flashes on each.
- **Civilian / hostage** — DON'T shoot (penalty); shooting the rope/guard frees one for
  a bonus (optional classic touch).
- **Supply crate** — shootable; drops ammo or a rocket.
Recolor a small body set per type/uniform with `pal()` (the `crowd.c` magic-color trick).

## HUD / states
- **HUD:** `bar()` health + `bar()` ammo, rocket count, score, stage, wave.
- **States:** TITLE (brief) → PLAY (per stage) → STAGE CLEAR → GAME OVER (score + best).
- Damage feedback: red full-screen flash + `shake`; low-health = pulsing red vignette +
  an alarm tone.

## Make it juicy (this genre lives on it)
- **Gun:** muzzle flash, recoil kick, **shell casings** (particles), tracer/​impact puff
  (`pset` + a few `particles`) where each shot lands.
- **Explosions:** expanding `circfill` + `fillp` smoke ring, big `shake`, white flash.
- **Enemies:** hit-flash (`pal` to white), a death pop/ragdoll, hit-marker on the
  crosshair.
- **Crosshair:** snappy, slightly lagged follow; tightens when over a valid target.
- **Audio:** chunky machine-gun bursts (filtered-noise hits on a fast `every`), distinct
  enemy fire, low-noise explosions, empty-mag click, reload chunk, civilian-hit sting,
  a jungle/ambience bed per stage, low-health alarm.

## Data model (suggested)
- `struct Actor { float x,y,vx,vy; int type, hp, state; float telegraph; } actors[N];`
- `struct Shot/Grenade/Particle` pools (typed static arrays, the `robotron.c` idiom).
- player: `health, ammo, ammo_max, rockets, score, stage, wave, scroll_x`.
- crosshair from `mouse_x/mouse_y`; `mouse_pressed(LEFT/RIGHT)` to fire.

## Controls
**Mouse-core:** move to aim, **left = shoot, right = rocket**, **shoot a crate to
reload**. (No keyboard needed; if you add a fallback, keep mouse primary.)

## Lean into / read
`paratrooper.c` (sky/ground shooting + waves), `robotron.c` (entity + bullet pools),
`effects.c`/`juice.c`/`particles.c` (muzzle/explosion/shake/flash), `crowd.c` (pal
enemy recolor), `drummachine.c`/`omnichord.c` (gunfire + ambience via the synth). Use
the full `mouse_*` API. Skip: free movement (it's on-rails), inventory, branching —
it's a tight arcade gun game.

## Notes / locked choices
- 3 scrolling stages, shoot-crates-to-reload, civilians-cost-you, machine gun + rockets,
  health bar (no lives), save best score. Boss + hostage-rescue are optional polish.
