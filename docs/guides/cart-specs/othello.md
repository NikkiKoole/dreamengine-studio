# The game you're building: OTHELLO

## Premise
Reversi on an 8×8 board. You are **black**, the CPU is **white**. Place a disc only
where it **flanks** at least one straight line of opponent discs between your new disc
and another of yours — that line then **flips** to your color, with a little
edge-to-edge flip animation. Legal squares glow; the score is live; you **pass** when
you have no legal move; the game ends when the board is full or both players pass in a
row. The CPU plays a **greedy, corner-weighted** heuristic: it loves corners, fears the
squares next to them, and otherwise grabs the most flips.

## The core loop
1. It's black's turn. Every square where black has a legal move is **highlighted** with
   a soft pulsing dot.
2. **Click a highlighted square** → a black disc drops in, every flanked line is walked
   outward and the captured discs **flip** (a quick scale-squash that crosses through
   the board green at the midpoint, like a real disc turning over). A click sound on
   placement, a little run of flip ticks as the line turns.
3. Turn passes to **white** (the CPU). It thinks for a short beat, then plays its best
   move by the corner-weighted score and flips its line the same way.
4. If a side has **no legal move**, it **passes** (a banner says so). If **both** pass
   back-to-back, or the board fills, the game is **over**.
5. **Game over**: the higher disc count wins; banner shows the final tally. Click to play
   again. Best win margin is `save()`d.

## Engine features it leans into (and why)
- **Primitives only, no sprite sheet.** A board is cheap and crisp from `rectfill`
  (felt squares + grid lines) and `circfill`/`ovalfill` (discs). Drawing discs as
  squashed ovals is exactly how you fake a flip — no art budget spent, and it scales
  perfectly at the chunky native resolution.
- **The flip animation via `ovalfill` + `ease_in_out`.** Each flipping disc keeps a
  `0..1` timer; its horizontal radius is `lerp`'d from full → 0 → full, and the color
  swaps to the new owner exactly at the midpoint. Staggered start times along the
  captured line make the flip *travel* outward from the placed disc.
- **Juice at the moments that earn it:** a `shake` nudge + flash when a corner is taken,
  pulsing legal-move dots via `blink`/`anim`, a disc "drop" that eases its radius in,
  rolling score numbers (`lerp`) in the HUD.
- **Sound that fits a quiet board game:** a soft click instrument on placement, a short
  pitched tick per flipped disc (rising with the run length), a warm chord on a corner
  capture and a little fanfare on game over. No music bed — the game wants calm.
- **A clean win/lose/restart loop** with `save()` for the best margin.
- **Mouse-primary** input — the natural fit for a board game — with a keyboard cursor
  fallback so it's playable without a mouse.

## Data model
- `static int board[8][8]` — 0 empty, 1 black, 2 white.
- `static int legal[8][8]` — recomputed each turn: flips available if you play there.
- per-cell flip animation state: an owner, a target owner, a `0..1` timer, a stagger
  delay — a small `Flip` struct array or parallel arrays sized 8×8.
- `turn` (1 black / 2 white), `passes` (consecutive), `phase` (PLAY / CPU_THINK /
  ANIMATING / OVER), CPU think timer.
- corner-weight table for the AI: corners huge positive, corner-adjacent (X/C squares)
  negative, edges mildly positive.

## Controls
Mouse-primary: **click a highlighted square** to place your black disc. Keyboard
fallback: **arrows/WASD** move a cursor, **Z / ENTER** to place on the highlighted cell.
**Click** (or Z) on the game-over banner to start a new game. You are black; white is the
CPU.
