# The game you're building: HANGMAN

## Premise
The classic word-guessing game, rendered cleanly from primitives. A hidden word sits
as a row of blanks; you guess one letter at a time off an on-screen QWERTY-ish alphabet.
A right letter fills every matching blank with a satisfying pop; a wrong letter burns a
guess and draws the next body part onto the gallows — head, torso, two arms, two legs,
then the noose tightens. Complete the word to win; reach the full hanging to lose. A new
word is dealt on restart, drawn from an embedded list. No sprite sheet — the whole game
is lines, circles, and text.

## The locked slice (build exactly this)
- A hidden word, shown as underscored blanks; correct guesses reveal letters in place.
- Type a letter (or click it on the on-screen alphabet grid) to guess.
- Wrong guess → next gallows body part is drawn + a guess is burned (6 wrong = loss).
- Win when every blank is filled; lose at the full hanging figure.
- WIN / LOSE banner, then ENTER (or click) starts a fresh word from the embedded list.
- Embedded word list (10–16 words), one picked at random per round, no repeats back-to-back.

No categories, no hints, no multiplayer, no difficulty select. One screen, one loop.

## Engine features it leans into (and why)
- **Primitives for everything** — the gallows is `line`/`circ` drawn part-by-part as a
  6-stage figure; the alphabet is a grid of `rectfill` keys with `print`. No `.cart.js`,
  no sprite budget — this game is geometry + text, so it should be drawn that way.
- **Keyboard + mouse input** — `text_input()` captures typed letters (the natural way to
  play); `mouse_pressed(MOUSE_LEFT)` + `point_in_box` lets you click the alphabet keys.
  Both feed the same `guess(letter)` path.
- **Juice at the earned moments** — a green flash + rising chime on a correct letter, a
  `shake()` + red flash + low buzz on a wrong one. The losing body part drops in with a
  short ease. Win plays an ascending arpeggio; loss a descending one.
- **State that persists** — `save_int("wins", …)` / `save_int("streak", …)` track a
  session win count and current streak across runs, shown in the HUD.
- A clean **PLAY → WIN/LOSE → restart** loop with a small banner overlay.

## Controls
- **Type a letter** (A–Z) to guess it.
- **Click a key** on the on-screen alphabet to guess it.
- A guessed letter greys out (can't be reused).
- **ENTER** or **click anywhere** on the WIN/LOSE banner to deal a new word.
