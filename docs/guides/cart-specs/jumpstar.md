# The game you're building: JUMPSTAR

## Premise
An endless vertical jumper in the Doodle-Jump grain. Your little astronaut
**auto-bounces** off platforms forever — you never press jump. All you do is steer
left/right (wrapping around the screen edges) to keep landing on the next platform up.
The camera climbs with you, platforms scroll off the bottom, and the moment you miss
and fall below the bottom edge, the run ends. **Height is your score**, and the best
run is `save()`d between sessions.

## The core loop
1. Gravity pulls you down; the instant your feet touch a platform from above you get an
   automatic upward bounce (a fixed jump impulse) — no input needed.
2. **Steer** with Left/Right (or A/D). Walk off the left edge and you wrap to the right,
   and vice-versa.
3. As you rise past the middle of the screen the **camera follows up** and new platforms
   are procedurally spawned above; platforms that scroll off the bottom are recycled.
4. **Stars** float between platforms — touch one for bonus points and a sparkle.
5. Miss every platform and fall off the bottom → **GAME OVER**. Press A to restart.
6. Score = max height reached (in "metres") + star bonus. Best score persists via `save()`.

## Platform variety (the spice)
A small typed pool of platforms, weighted to get harder the higher you climb:
- **Normal** (green) — solid, always there.
- **Moving** (blue) — slides left/right and bounces at the edges; lands fine but you
  have to time it.
- **Breakable** (brown, cracked) — gives one bounce, then crumbles and falls away with a
  puff of debris. Don't dawdle.

## Make it juicy
- **Squash on bounce** — the astronaut flattens and springs back each landing (`ease_out`).
- **Bounce SFX** — a short rising blip on every jump; a brighter chime on a star; a dry
  noise crack when a breakable shatters.
- **Star sparkle** — a few particles + a flash when collected; stars gently bob (`sin_deg`).
- **Screen shake** on a breakable shatter.
- **Parallax starfield** behind everything, scrolling slower than the world for depth.
- **Death**: a quick red `fade`/`shake`, then the score card slides in.
- **Combo-free, readable HUD**: live height top-left, best top-right.

## Data model
- player: `px, py` (world Y grows downward but score = -py), `vx, vy`, squash timer, facing.
- `cam_y` world-space camera top; `follow`-style clamp not needed (infinite world).
- `Plat plats[N]` typed pool: `x, y, w, type, vx (moving), broken, fall_vy`.
- `Star stars[N]`: `x, y, alive, phase`.
- recycle: when a platform's world Y drops below `cam_y + SCREEN_H`, respawn it a screen
  above the current highest platform with a fresh random type/width.
- `best = load(0)`; on death `if (score > best) save(0, score)`.

## Controls
Left/Right or A/D to steer (wraps at edges). Jump is automatic on landing. A (Z) to
restart after a game over.

## Lean into / read
`lander.c` (gravity + terrain-style landing + win/lose/restart card), `juice.c`
(squash, shake, flash, particles, hit-feel), `effects.c` (more particle idioms). No tile
map and no sprites — the astronaut and platforms are primitives (cheap, recolorable,
and procedural fits the endless spawn). Default 320×200 screen.
