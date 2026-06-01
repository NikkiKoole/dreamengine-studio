# The game you're building: BEJEWELED

## Premise
A match-3 jewel board. An **8×8 grid** of seven colored gems sits in a framed well.
You **swap two adjacent gems** — but only if the swap forms a line of three or more
same-color gems. Matches **clear for points**, the gems above **fall** into the gaps,
fresh gems **drop in** from the top, and any new matches the fall creates **cascade**
for escalating bonus. When the board has **no legal move left**, it **reshuffles**.
You're racing a **move budget** (and the clock keeps you honest) for the highest score —
best score persists. It's a single-screen, mouse-first toy carried almost entirely by
**juice** at the moment of the clear.

## The locked slice — build exactly this, no more
- 8×8 grid, 7 gem colors.
- Click a gem, then click an adjacent gem to swap (or click-drag from one to a neighbor).
- A swap that creates no match **rejects** — gems wiggle back, no move spent.
- Any horizontal OR vertical run of 3+ same-color clears.
- Cleared cells empty; gems above fall; new random gems drop in from the top.
- Re-evaluate after settling → **cascades** chain, each chain step worth more.
- Score = `gems × 10 × cascade_multiplier`. A move budget (e.g. 30 swaps) ends the game;
  a soft timer adds light pressure. `save()` the best score.
- Detect "no legal move" → auto **reshuffle** (don't deadlock).
- Win/lose is just "moves ran out" → GAME OVER with score + best + restart.

NOT in scope: special gems (flame/hypercube), level progression, themed boards,
multiple modes. One board, one loop, lots of feel.

## Engine features it leans into (and why)
- **Mouse** (`mouse_pressed`/`mouse_x`/`mouse_y`): the whole control surface — pick &
  swap by clicking adjacent cells, no keyboard needed.
- **A tiny state machine** (IDLE → SWAPPING → CLEARING → FALLING → back) driven by
  `dt()` timers so swaps, pops and falls *animate* rather than snap. This is the spine.
- **Juice at the clear** (the priority): each clearing gem flashes white then **shrinks
  to nothing** (`ease_out` on a per-cell pop timer), a **particle puff** in the gem's
  color sprays out, `shake()` scales with cascade depth, and a rising **chord/note**
  pitches up per cascade step so a big chain *sounds* like a payoff.
- **`lerp` falling**: gems slide from their old row to their new row with ease, and
  drop-ins fall from above the well — `ease_out` makes it land with weight.
- **Primitives only** — gems are faceted diamonds (a `trifill` body + a bright `tri`
  highlight + outline), distinct by both **color and shape accent** for colorblind
  readability. No sprite sheet; clean and self-contained.
- **Sound:** soft `note()` blip on a valid pick, a "nope" buzz on an illegal swap, a
  bright `chord()`/`note()` that climbs with cascade depth on clears.
- **`save()`**: best score survives between runs.

## Controls
Mouse only. **Click a gem** to select it (it lifts and pulses). **Click an adjacent
gem** to swap — legal swaps resolve, illegal ones bounce back and cost nothing.
Click-and-**drag** from a gem toward a neighbor also swaps. On GAME OVER, **click**
(or any key) to start a fresh board.
