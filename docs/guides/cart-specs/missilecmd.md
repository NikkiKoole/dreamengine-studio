# The game you're building: MISSILE COMMAND

## Premise
The 1980 Atari classic, faithful to the slice. Six cities huddle along the bottom of
the screen under a night sky. ICBMs streak down from the top in long diagonal trails.
You command three batteries — left, center, right — each with limited ammo. Aim with
the mouse and click: a counter-missile launches from the **nearest battery**, flies to
the cursor, and **detonates into an expanding ring**. Anything the ring touches dies.
Waves escalate in speed and count. The game ends when every city has fallen.

It is a pure single-screen primitives game — no tile world, no sprite sheet. The whole
thing is lines, circles and arcs over a dark sky, which is exactly what the original
was: glowing vector trails and blooming fireballs.

## The locked slice (build exactly this)
- **6 cities** along the bottom; each is destroyed when an enemy missile or blast
  reaches it.
- **Incoming ICBMs**: spawn at the top, travel in straight diagonal lines toward a
  random surviving city (or a battery). Leave a fading trail.
- **3 batteries** with **limited ammo per wave**; ammo refills between waves. A battery
  with no ammo can't fire.
- **Mouse-aim, click to fire** from the **nearest battery** with ammo. The
  counter-missile flies to the cursor, then **detonates into an expanding/contracting
  ring** that destroys any enemy missile that enters it.
- **Chain reactions**: an enemy missile destroyed mid-air still scores; the player blast
  ring is the only thing that kills them (cities just die on impact).
- **Wave escalation**: each wave adds more missiles and more speed.
- **Game over** when all 6 cities are gone. Show score + best (`save()`), click to restart.

## Engine features it leans into (and why)
- **Primitives only** — `line` for falling ICBM trails and the player missile's flight
  path, `circfill`/`circ` for the blast rings (the signature mechanic), `arc`/`ring` or
  `circfill` for the city domes and battery turrets, `rectfill` for the ground. This is
  the genre's native look; sprites would only get in the way.
- **Particle pool** — the workhorse from `particles.c`: sparks when a missile is
  vaporized, debris when a city falls. A fixed array, move/age/recycle.
- **Blast rings as their own pool** — each detonation is a ring that grows to a max
  radius then shrinks and dies; collision against enemy missiles each frame (the
  `near()` / `circles_touch()` idiom).
- **Mouse** — `mouse_x/y`, `mouse_pressed(MOUSE_LEFT)`, with a drawn crosshair. Nearest
  battery chosen by `distance()`.
- **dt()-scaled movement** so missile speed is framerate-independent; speed/count scale
  with wave number.
- **Sound** — a thin launch `note`, a chunky filtered-`INSTR_NOISE` detonation `hit`,
  a descending sting when a city is lost. A low pulsing alarm bed when only one city
  remains. `shake()` + a brief screen flash on every detonation and city loss.
- **Persistence** — `save()` the high score.

## Controls
Mouse aim, **left-click to fire** from the nearest battery that still has ammo. On the
game-over screen, click (or SPACE) to redeploy.

## Lean into / read
`opwolf.c` (mouse-aim + crosshair + particle/boom pools), `particles.c` (the pool
idiom), `effects.c`/`juice.c` (shake/flash). Skip: tile maps, sprite sheets, free-roam.
