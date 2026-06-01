# The game you're building: LARRY (PG-13)

## Premise
A sleazy-lounge point-and-click in the Sierra tradition, played for laughs and groans.
You're **Larry Laffer** in a polyester leisure suit, parked at the **Lucky Lips
Lounge**, trying to charm the cool-as-ice cocktail waitress, **Roxy**, into a date.
She's heard every line in the book — so you have to gather the right props and pick the
right words. Cheeky double-entendre and innuendo only; nothing explicit. The whole
thing is a wink.

## The locked slice — build exactly this
- **2 scenes**: the **lounge bar** (bartender, jukebox, a rose on the counter, a
  splash of cheap cologne in the men's room dispenser) and the **back booth** where
  Roxy works.
- **Verb + inventory** point-and-click, lifted straight from the `zak.c` scene engine:
  four verbs (Look / Use / Take / Talk), click a hotspot, click the floor to walk,
  click an inventory item to hold it then click a hotspot to use it.
- **One dialogue exchange** with Roxy: a list of pickup lines you click. Most get a
  withering put-down. The **smooth-talk goal**: arrive holding the right props
  (a rose + a dab of cologne) AND pick the genuinely smooth line — that's the win.
- **A win/lose/restart loop**: land the date → curtain card → click to play again.

## The engine features it leans into (and why)
- **The `zak.c` SCUMM engine, reused wholesale** — verb bar, walk-to-hotspot,
  `pending` action queue, `held` inventory item, per-scene `act_*()` switch. This is
  the proven idiom for the genre; rebuilding it would be the wrong move.
- **Dialogue tree** (the new bit on top of zak) — a small fixed array of lines; clicking
  one runs Roxy's reaction. Borrowed in spirit from `papers.c`'s line-by-line panels and
  `smooch.c`'s cheeky writing voice.
- **A puzzle gate**: the smooth line only lands if `inv` has the rose AND the cologne —
  so the two scenes feed each other (classic adventure two-prop combo).
- **Sound for punctuation, not a bed** — a custom SINE "smooth sax" lick on the win,
  square-wave clunks on rejections, a jukebox `chord()` when you feed it a coin. Silence
  the rest of the time; the lounge isn't a rhythm game.
- **Juice where it earns it** — Roxy's eye-roll, a heart that pops on the win, `shake`
  on a face-slap rejection, `blink` on the exit arrows and the jukebox.

## Controls
Mouse only. Click a **verb** (bottom-left 2×2), then click a **hotspot** to act on it,
or click the floor to walk. Click an **inventory slot** to hold that item, then click a
hotspot to use it. In the booth, click a **dialogue line** to say it. Click to dismiss
the win card and play again.
