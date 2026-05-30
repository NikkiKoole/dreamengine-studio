# Polish & Toolbox TODO

A working list for raising the quality bar across the ~90 carts in `tools/carts/`.

> **Status (2026-05-30): not an active backlog.** Decided against retrofitting a
> juice pass onto the existing carts — better to keep them as-is and apply these
> learnings to *new* games, or build focused v2s of the few that are worth it.
> Treat what follows as a reference idea bank, not a to-do list.

> **Scope note.** This doc is **Part B — per-game polish** only. What's shipped vs.
> open across the engine/API now lives in one ledger, [`STATUS.md`](STATUS.md); the
> design rationale (classics survey, proposed signatures, and the cart-frequency
> analysis behind `hud()`/`game_over_screen()`/`explode()`) is in
> [`design/api-notes.md`](design/api-notes.md). Don't track API status here — it drifts.

**What a juice pass can rely on today:** nothing graphics-side is blocking — sprite
rotation (`spr_rot`/`sspr_ex`), text scaling (`print_scaled`/`text_width`), the
`pal`/`fade`/`shake` juice batch, ellipses, `dt()`, and string/struct persistence
(`save_bytes`/`load_bytes`) have all landed. The still-open convenience helpers
(`explode`/`hud`/`game_over_screen`, `pause`, events) don't block starting Part B —
see [`STATUS.md`](STATUS.md) for the full open list.

Two polish-specific gaps not yet in any design doc, noted here so they aren't lost:
gradient/dither fill (flat skies in gorillas/racer/sopwith/lander) and looping
ambience (`drone`)/`volume`/mute (sustained pads for sims; player mute).

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

The API work this doc was waiting on has landed and committed (juice batch,
`spr_rot`/`sspr_ex`, `fillp`, etc. — `c4f2801`), so the tree is a clean base.

1. **One reference "juice pass"** on a single small game (breakout or asteroids) using
   the landed `pal`/`fade`/`shake`/`spr_rot` to establish the pattern and prove the API.
2. **Roll the pattern across arcade classics**, then sims, then demos.
3. **Tutorial audit** last — cheap, and benefits from the API being settled.
