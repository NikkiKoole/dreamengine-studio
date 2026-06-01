# The game you're building: DEFENDER

## Premise
The arcade classic. You pilot a fast little ship skimming left and right over a
**wrapping mountain landscape**. Alien **Landers** drift down from the sky, grab the
**humanoids** walking the ground, and try to haul them up to the top of the screen —
where, if they make it, the lander **mutates** into a fast, swarming killer. Shoot a
lander while it carries a human and the human drops; dive, **catch the falling human**,
and ferry it back to the ground. A **scanner** strip across the top shows the whole
wide level at a glance — that's how you know where the danger is, because the playfield
is far wider than the screen. Clear all the aliens to advance the wave. Lose all your
humans and the planet is lost; survive on lives.

## The locked slice (build exactly this)
- Side-scrolling **horizontal shooter** over a wrapping landscape (world ~3× screen wide).
- Fly **up/down** freely; **left/right** thrusts and **faces** that way; **Z fires** forward.
- **Landers** descend, lock onto a humanoid, abduct it (rising), and **mutate** at the top.
- Shoot a carrying lander → the **human drops**; **catch it midair** and lower it home.
- A **scanner minimap** shows the whole level (ship, aliens, humans) in one strip.
- **Waves** of enemies; clean win/lose/restart loop; high score saved.
- **Cut:** smart bombs and hyperspace (the spec says cut these if tight — they are cut).

## Engine features it leans into (and why)
- **`camera()` over a wide world + horizontal wrap** — the signature Defender feel is a
  playfield bigger than the window; the camera scrolls and the world wraps seamlessly.
  This is the `sopwith.c`/`racer.c` scroll idiom pushed to a ring topology.
- **Typed fixed-size pools** (`robotron.c` idiom) for landers, mutants, bullets,
  humans, explosions — no heap, all `static` arrays, framerate-independent with `dt()`.
- **Primitives only, no sprite sheet.** Ships, landers, mutants and humanoids are tiny
  vector/blob shapes drawn with `trifill`/`rectfill`/`line`/`circfill`. The art budget
  goes into motion and the scanner, not a tile sheet — this game reads better as crisp
  glowing shapes than as 16×16 pixels.
- **The scanner** is the star secondary view: a clipped strip drawn with `pset`/`rect`,
  mapping the whole wrapping world into ~SCREEN_W pixels, with the camera window marked.
- **Sound** from the synth: a thin `INSTR_NOISE` laser, a descending alien warble
  (`INSTR_SAW`), abduction siren, explosion `hit()`s, and a pickup chime — reactive,
  not a music bed.
- **Juice:** `shake()` on death/explosions, hit-flash via `pal()`, a starfield, the
  mutate moment flashing, the catch-the-human chime.

## Controls
- **Up / Down** — fly up and down.
- **Left / Right** — thrust + face that direction (the world scrolls under you).
- **Z** — fire forward.
- After game over: **Z** to restart.
