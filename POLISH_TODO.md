# Polish & Toolbox TODO

A working list for raising the quality bar across the ~85 carts in `tools/`.

> **Scope note (reconciliation).** API/engine design is already tracked in
> [`docs/API_RESEARCH.md`](docs/API_RESEARCH.md) (deep survey + proposed signatures,
> shipped/open markers) and [`docs/API_IDEAS.md`](docs/API_IDEAS.md) (cart-source
> pattern analysis → `hud()`, `game_over_screen()`, `explode()`). Another agent is
> **actively adding functions to `runtime/studio.{h,c}` right now** — do not edit
> those files from here. So this doc defers API design to those two and focuses on
> **Part B — per-game polish**, which neither covers. Part A below is just a
> *status snapshot* of the gaps that block polish, so the per-game work knows what
> it can lean on.

---

## Part A — API gaps that block polish (status snapshot, not a work list)

Don't implement these here — they belong to the API docs + the in-flight agent.
This is just "what can a juice pass rely on yet?"

All the 🟡 rows are in the working tree (uncommitted; HEAD still `06694cb`).

| Primitive | Status | Notes / source |
|---|---|---|
| **`spr_rot` + `spr_ex`** — sprite rotation/scale/flip around a pivot | 🟡 landed (wt) | the highest-value gap — now closed. API_RESEARCH §18. Unblocks asteroids, racer, lander, sopwith, paratrooper, galaga, xevious. |
| `print_scaled(text,x,y,color,scale)` — big titles/scores | 🟡 landed (wt) | closes the text-scaling gap |
| `save_bytes` / `load_bytes` — block/struct persistence | 🟡 landed (wt) | closes the string-persistence gap (hi-score tables, names) more generally than `save_str` |
| `key` / `keyp` / `text_input()` — raw keyboard + typed chars | 🟡 landed (wt) | bonus — name entry, word games. Not on any prior list |
| `oval` / `ovalfill` — ellipses | 🟡 landed (wt) | bonus — eyes, shadows, squashed circles |
| `dt()` — delta time | 🟡 landed (wt) | bonus — framerate-independent movement/decay |
| `pal` / `pal_reset` — recolor & hit-flash | 🟡 landed (wt) | white-flash-on-hit primitive |
| `fade(amount)` — darken screen for transitions | 🟡 landed (wt) | state fades, pause dim |
| `shake(amount)` — self-decaying screenshake | 🟡 landed (wt) | replaces hand-rolled shake in `juice.c` |
| `text_width(text)` — measure for layout | 🟡 landed (wt) | custom HUD boxes / menus |
| `bar` / `blink` / `fsqrt` | ✅ committed | `06694cb` |
| Particle/explosion helper (`explode()`) | 🔴 open (proposed) | API_IDEAS §3 — or ship a `particles` library cart |
| `hud()` / `game_over_screen()` | 🔴 open (proposed) | API_IDEAS §1–2 — appears in 11+ / 12+ carts |
| `pause()` / `paused()`, `fps()`, `voices_active()` | 🔴 open | API_RESEARCH §16 |
| Events: `broadcast()` / `received()` | 🔴 open | API_RESEARCH §11 |
| In-code sfx/music definition | 🔴 open question | API_RESEARCH (code-first vs. tracker UI) |
| Gamepad axes (`gp_axis`) | 🔴 open | API_RESEARCH §15 |
| Strudel extras / Dilla groove timing | 🔴 open | API_RESEARCH §13–14 |
| **Resolved tile collision** (`move_and_collide`) | 🔴 open — **not in either doc** | platform/alleycat/pitfall/burgertime each re-derive wall push-out |
| Gradient / dither fill | 🔴 open — **not in either doc** | flat skies in gorillas/racer/sopwith/lander |
| Looping ambience (`drone`) / `volume` / mute | 🔴 open — **not in either doc** | sustained pads for sims; player mute |

Net: the three gaps I'd flagged as blockers — **sprite rotation, text scaling, and
string/struct persistence — have all landed** (plus keyboard input, ellipses, and
`dt()` as bonuses). Nothing graphics-side is blocking a juice pass anymore. The
remaining 🔴 rows are convenience helpers (`hud`/`explode`/`game_over`), structure
(`pause`/events), audio polish, and `move_and_collide` — none of which block
starting Part B.

---

## Part B — Per-game polish

Cross-cutting checklist to apply per game (the "juice pass"):

- [ ] Title screen + game-over screen with restart
- [ ] Hi-score saved with `save()` (and initials once A2 string-save lands)
- [ ] At least one sound on every meaningful event (hit, score, die, level-up)
- [ ] Screenshake + hit-stop + particles on impacts
- [ ] A difficulty curve / level progression rather than one static board
- [ ] Readable HUD (lives, score, level) using `print_centered`/`bar`

### Arcade classics — biggest wins from level progression + juice

- [ ] **invaders** — bonus UFO, shields that erode, wave layouts, speed-up sound.
- [ ] **breakout** — power-ups (multiball, wide paddle, lasers), multiple brick
  layouts per level, gold bricks.
- [ ] **pong** — single-player vs. AI mode, ball spin, score juice. Tiny game,
  cheap to make feel great.
- [ ] **snake** — obstacles/walls per level, portals, speed modes, "golden apple."
- [ ] **asteroids** — UFO enemy, hyperspace, ship-explode particles (needs A1 rot).
- [ ] **frogger** — already has rounds; add diving turtles, snakes, a timer bar,
  bonus flies.
- [ ] **pacman** — fruit bonuses, ghost personalities/AI states, cutscenes between
  levels, multiple maze layouts.
- [ ] **tetris** — hold piece, ghost piece, T-spin, level speed curve, line-clear
  flash (needs A1 alpha).
- [ ] **galaga / xevious** — formation entry patterns, dive-bomb attacks, more
  enemy types, stage flags. *(These are in the current git diff — good moment.)*
- [ ] **paratrooper / pang / bomberman / bubblebobble** — level variety + boss or
  escalation; bomberman wants 2-player.

### Sims & sandboxes — need goals & content variety

- [ ] **gta** — missions/objectives, weapon variety, pedestrian variety, day/night.
- [ ] **sims** — needs/mood loop with more interactions, multiple sims. *(in diff)*
- [ ] **city** — zoning feedback, growth milestones, disasters.
- [ ] **katamari** — multiple themed levels with size thresholds, a timer, a target
  size to win. *(in diff)*
- [ ] **vampire** (survivors-like) — weapon evolutions, more enemy types, level-up
  choices, a boss timer.
- [ ] **pet** — more activities, aging/evolution stages, mini-games. *(in diff)*
- [ ] **fourx / mule / elite / orion** — these 4X/strategy carts are deep; pick one
  to flesh out a real win condition rather than spreading thin.

### Tech demos — interactivity & presets turn demos into toys

- [ ] **raycaster / mode7 / cube3d** — add objects/sprites in the world, a goal,
  texture variety.
- [ ] **doomfire / fire / sand / ripple / rope / boids** — add UI to tweak
  parameters live (mouse already exists), color-palette presets, mouse interaction.
- [ ] **lsystem / spirograph / hypotrochoid** — preset gallery + "randomize" button;
  export/screenshot the pretty ones.
- [ ] **pathfinding / astar / mazegen / maze** — visualize step-by-step, let the
  user place walls with the mouse, race two algorithms.

### Music carts

- [ ] **drummachine** — save/load patterns (needs A3 in-code sfx + string save),
  more kits, swing, song mode (chain patterns).
- [ ] **garden** — more moods/scales, visual variety, a "seed" you can save.
- [ ] **fartsynth** — more instruments/presets, a recordable sequence.

### Tutorials (01–19) — keep them tiny, but tighten

- [ ] Audit that each tutorial demonstrates exactly one concept cleanly and that
  the comments still match the current API (several API funcs are newer than some
  tutorials). Low effort, high value for the "learning environment" mission.

---

## Suggested sequencing

1. **Let the in-flight API work land first** (flash/fade/shake/text_width are in the
   working tree). Don't start a juice sweep mid-edit — wait for it to commit.
2. **One reference "juice pass"** on a single small game (breakout or asteroids) using
   the freshly-landed `pal`/`fade`/`shake` to establish the pattern and prove the API.
   This is also the best way to *discover* whether sprite rotation is the blocker it
   looks like — feed that back to the API work.
3. **Roll the pattern across arcade classics**, then sims, then demos.
4. **Tutorial audit** last — cheap, and benefits from the API being settled.

> Note: `studio.c/.h`, `shell.js`, `studioDocs.js`, and several carts (galaga,
> juice, katamari, pet, sims, xevious, zelda) currently have uncommitted changes
> (some from the in-flight API agent). Coordinate / wait for that commit before a
> polish sweep so this builds on a clean base.
