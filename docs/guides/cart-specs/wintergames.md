# The game you're building: WINTER GAMES

## Premise
An Epyx-style **multi-event** sports cart. Pick a flag, then compete in a roster of
events one at a time, each a tiny timing/rhythm skill game; scores accumulate on a
shared **scoreboard**, and a **result screen** crowns your run. This batch ships the
reusable **event shell** (event select → play → result → back to scoreboard) plus
**two events** so the framework is proven and visibly fun:

1. **SKI JUMP** — two-phase skill. Slide down the in-run; **time the takeoff** at the
   lip (a sweeping power meter — fire near the sweet spot for launch speed), then in
   the air **balance the lean angle** with the arrows: too far forward and you nose
   over and tumble; too far back and you stall and drop short. Distance = launch ×
   flight quality. A wobble line shows your balance.
2. **SPEED SKATING** — alternating-key **rhythm** to build speed: tap **Z, X, Z, X…**
   in a clean cadence to push off each skate; mis-timing (double-tapping the same key,
   or tapping off-beat) bleeds speed. **Hold the line** (Up/Down nudge) to stay in your
   lane around the oval. Beat the clock over the lap.

The shell is the deliverable as much as the events — it's designed to be lifted whole
into **summergames** and **calgames** (just swap the event table + the per-event
update/draw fns). Keep it clean and table-driven.

## The core loop
1. **TITLE** — choose your nation (flag colors) with Left/Right, Z to start.
2. **EVENT SELECT** — a list of events with your best score beside each; Up/Down to
   move, Z to enter. A "DONE" entry shows the medal ceremony.
3. **EVENT** — a `prepare → play → score` arc inside the event. On finish, the score
   is recorded and we ease back to the scoreboard.
4. **CEREMONY** — total score, per-event breakdown, a podium, `save()` the best total.

## The event shell (the reusable part — make it clean)
- An `Event` is a struct of `{ name, void (*reset)(), void (*update)(), void (*draw)() }`
  plus a `done` flag and a `score` it writes back. The shell drives the common chrome
  (HUD bar with event name + your flag + attempt scoreline, the "press Z" prompts,
  the fade transitions) so an event fn only does its own field.
- Scores live in `static int best[NEVENTS]` and `static int last[NEVENTS]`, persisted
  with `save()`/`load()`. Adding an event = one row in the table + three fns. That's
  the whole extension story.

## Engine features it leans into (and why)
- **`dt()` everywhere** — both events are about timing, so framerate independence is
  load-bearing, not cosmetic.
- **Ski-jump air physics = the `excitebike.c` landing-angle idiom turned to the side**:
  gravity on `vy`, player fights a passive lean drift with Up/Down, `spr_rot`/rotated
  primitives for the jumper, a `LAND_TOL`-style window deciding clean vs. tumble.
- **Rhythm via `btnp` + a cadence timer** — alternating Z/X with a tracked "expected
  key" and a beat window; reward tight timing, punish mashing. `note()` ticks per push.
- **`bar`/`arc`/`ring`** for the power-meter sweep and the lean gauge — primitives are
  the right tool for gauges; no sprite budget spent.
- **Sound that matches the moment** — a synth whoosh on launch (filtered saw + pitch
  LFO), skate-stride clicks (short noise hits), a fanfare on a personal best.
- **Juice** — `shake` on a crash landing, screen `fade` between shell states, a
  blinking "NEW BEST", squash on a clean landing, snow particles drifting in the wind.
- **No tile map / no sprites** — these are single-screen skill fields; everything is
  drawn with primitives (mountains, ice oval, jumper, skater) so the cart is one file
  and the shell stays portable.

## Controls
- **Title:** Left/Right pick nation · Z start.
- **Event select:** Up/Down choose · Z enter · X back to title.
- **Ski jump:** Z to start the run, then Z to fire the takeoff near the sweet spot;
  in the air Up = lean back, Down = lean forward (keep the wobble centered).
- **Speed skating:** alternate Z / X in a steady cadence to skate; Up/Down hold your
  lane. Don't mash one key.
- **Anywhere in an event:** when it ends, Z returns to the scoreboard; X aborts early.
