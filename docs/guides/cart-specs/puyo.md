# The game you're building: PUYO

## Premise
A falling-pair color matcher in the Puyo Puyo lineage. Pairs of colored blobs ("puyos")
drop into a narrow well; you slide and **rotate the pair around its pivot** as it falls.
When **4 or more** puyos of the same color touch orthogonally, they **pop**. Pops drop
everything above into the gaps — and if the new landing makes *another* group of 4+,
that pops too: a **CHAIN**. Long chains are the whole point — they detonate in sequence
for escalating score with a rising sound sting. Single-player score-attack; the stack
tops out and you lose. Best score is saved.

## The core loop
1. A pair of two random-colored puyos spawns at the top, falling under gravity.
2. **Move** left/right, **rotate** the pair (the second puyo orbits the first through the
   4 compass positions), **soft-drop** to speed it down.
3. When the pair can't fall further it **locks**. The two puyos settle independently —
   each column-drops to its own resting height (a pair split over a ledge separates).
4. **Resolve**: flood-fill every color group; any group of 4+ pops. Above-puyos fall to
   fill gaps. Re-resolve — each successive pop in the same settle is the next **chain
   step**, worth more. Score = group sizes × a chain multiplier that ramps hard.
5. Spawn the next pair (previewed). If the spawn cell is blocked → **top-out**, game over.

## The slice (locked — build exactly this)
- A 6-wide × 12-tall well, 4 puyo colors (4 is the classic readable count).
- Falling pair: move, rotate (Up or Z), soft-drop (Down). No hard-drop, no hold, no
  garbage/nuisance puyos, no AI opponent — pure single-player chain score-attack.
- Real chain resolution with escalating multiplier — the heart of the game.
- Title → play → top-out → restart loop. `save()` the best score.

## Engine features it leans into (and why)
- **Primitives, no sprite sheet.** Puyos are `circfill` blobs with a highlight pset and
  a darker outline `circ` — cheaper, crisper, and recolorable than 4 sprites. The well,
  HUD and previews are all rect/print. This game doesn't want a tilesheet.
- **`dt()`-based gravity** so fall speed is framerate-independent; soft-drop just shortens
  the fall interval. DAS-style auto-repeat on left/right (the tetris idiom).
- **A resolution state machine** (FALLING → POPPING → CHAIN settle → FALLING) driven by
  short timers, so pops *read*: the popping group flashes/shrinks before vanishing, then
  the stack visibly falls. This staging is what makes a chain feel like a chain.
- **Juice at the earned moments:** `shake` scaling with chain length, a white flash on
  pop, popped puyos shrink out (`ease_out`), and a **rising pitch sting per chain step**
  (`note()` climbing a scale) so a 5-chain *sounds* like a 5-chain. Tiny particle sparks
  on a pop using a fixed pool.
- **Sound** — soft lock thunk, ascending chain notes, a fanfare `chord` on a big chain,
  a low buzz on top-out. Built from the synth, not note-spam.
- **`save()`** for the best score; a chain-count readout and "MAX CHAIN" stat.

## Controls
Left/Right move the pair, **Up or Z** rotate it (clockwise), **Down** soft-drop.
On the game-over screen, **Z** restarts.
