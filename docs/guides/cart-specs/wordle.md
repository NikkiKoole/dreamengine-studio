# The game you're building: WORDLE

## Premise
The daily word puzzle, distilled. A hidden **5-letter word** sits behind six empty rows.
You type a guess, hit ENTER, and each letter flips to a color: **green** (right letter,
right spot), **yellow** (in the word, wrong spot), **gray** (not in the word). Six guesses
to crack it. An on-screen **QWERTY keyboard** dims and tints its keys as you learn the
answer. Win and the row dances; lose and the answer is revealed. One clean play → result
→ restart loop.

## The core loop
1. **PLAYING** — cursor sits on the current row. Type letters (max 5), BACKSPACE deletes,
   ENTER submits. A submit is rejected (a buzz + a little shake) if the row isn't full or
   the word isn't in the allowed list.
2. On a valid submit, the row's five tiles **flip one-by-one** (a quick stagger via
   `anim_once`/`ease`), revealing green/yellow/gray. The keyboard repaints to the best
   color each letter has earned.
3. If the guess equals the answer → **WIN**: the winning row bounces, a rising arpeggio
   plays. If that was the sixth guess and it's wrong → **LOSE**: the answer is shown.
4. Press ENTER on the result screen to **restart** with a fresh random answer.

## The locked slice (build exactly this — no more)
- Hidden 5-letter word, 6 tries, the three-color feedback with the **correct duplicate-letter
  rule** (yellow count is capped by remaining unmatched occurrences in the answer).
- On-screen QWERTY keyboard that tints used letters (green > yellow > gray priority).
- Win/lose screen that reveals the answer; ENTER restarts.
- A small embedded word list: a curated **answers** array + a larger **allowed-guess** set
  (so real words you type are accepted even if they're never the answer).
- Persist a tiny **win streak** with `save()`.

## Engine features it leans into and why
- **`text_input()`** — the natural way to capture typed letters this frame; the worked
  idiom is `typesave.c`. BACKSPACE/ENTER via `keyp(KEY_*)`.
- **On-screen keyboard with mouse** — `mouse_pressed(MOUSE_LEFT)` + `point_in_box()` so it
  plays on a trackpad too, mirroring the typed path exactly.
- **`anim_once` + `ease_out`** — the tile flip/reveal stagger; this is the whole "juice"
  budget and it's where the game earns its feel.
- **`shake()` + a noise buzz** — the rejected-guess feedback (invalid word / short row).
- **Sound** — a soft key click on each letter, a tile-reveal blip per flip, a rising
  arpeggio on win (`note`/`schedule`), a low buzz on lose.
- **`save()`** — a one-int win streak that survives between runs.
- All drawn from **primitives** (`rectfill`/`rect`/`print_scaled`): a tile grid and a
  keyboard are cleaner without a sprite sheet, so no `.cart.js` is needed.

## Controls
Type **A–Z** to fill the row, **BACKSPACE** to delete, **ENTER** to submit. Or click the
**on-screen QWERTY keyboard** (including its ENTER and DEL keys) with the mouse. On the
win/lose screen, **ENTER** (or click) starts a new word.
</content>
</invoke>
