# The game you're building: SUMMER GAMES

## Premise
A button-mashing track-and-field cart in the *Track & Field* / *Summer Games* arcade
lineage. Pick an event off a stadium menu, hammer two keys to build raw speed, then
nail one timing press at the right instant. TWO events sharing one event-shell:
**100m sprint** (mash to accelerate, *lean* at the tape) and **long jump** (mash the
runway, then stab the jump button at the best takeoff angle). A scoreboard ties them
together: each event posts a time/distance and a points score, and the cart tracks a
personal best with `save()`.

This is the **event-shell idiom**: a menu state that lists events → an event runs →
results post to a shared scoreboard → back to the menu. It's deliberately tiny and
deep on *feel*: the whole game is in the tactile mash + the single clutch press.

## The locked slice (build exactly this)
- **Event select** screen: a stadium backdrop with the two events as menu rows
  (100M SPRINT, LONG JUMP), Up/Down to choose, A to enter. A persistent **scoreboard**
  panel shows your last result + best per event.
- **100m sprint:** alternate Z/X to build speed (each alternating press adds a kick;
  same key twice does little — the classic mash). A runner sprite legs it across a
  side-scrolling track. Near the line, a **LEAN** window opens — press A inside it to
  dip at the tape and shave the time. Posts a time + points.
- **Long jump:** mash Z/X down the runway to build speed, hit the **foul line**, then
  press A to leap. The takeoff **angle** is a live sweeping gauge (a needle oscillating
  ~10°–60°); your distance = speed × angle-quality. Overstep the foul line = scratch.
  Posts a distance + points.
- A clean **results** beat per event (READY → RUN → RESULT) then back to the menu.
- Win/lose framing is the scoreboard + personal best; restart is just re-entering.

## Engine features it leans into (and why)
- **State machine via enums** (`MODE_MENU / MODE_EVENT`, phases `PH_READY / PH_RUN /
  PH_RESULT`) — the excitebike idiom, the spine of an event shell.
- **`btnp` mash detection** — alternating-key speed is `btnp` on Z vs X, tracking the
  *last* key so a true alternation pays out and double-tapping the same key doesn't.
  This is the heart of the genre and the engine gives it for free.
- **A timing window for the clutch press** — `btnp(A)` checked against a position/angle
  window; the lean and the takeoff angle are both "press at the right instant" beats,
  which is exactly what `btnp` + a live gauge express well.
- **`dt()`-framerate-independent motion** — runner speed decays and integrates per
  second so the mash feels identical at any framerate.
- **Side-scroll `camera()`** following the runner across a track longer than the screen.
- **Juice that earns it:** `shake()` on the lean/takeoff, dust particles off the feet,
  a screen flash + fanfare on a personal best, a sweeping angle needle, a speed bar.
- **Synth feel:** per-stride footfall ticks whose pitch rides speed (the excitebike
  engine-pitch trick), a crowd-roar noise swell as you near the line, a `schedule()`
  fanfare on results, a starter's-pistol `hit()` on GO.
- **Sprites + `pal()`** — one 16×16 runner across a few stride frames; `pal()` could
  recolor for flavor but the slice keeps a single athlete. Track is drawn with
  primitives (lane lines, sand pit) — cheap and crisp, no tilemap needed for a
  single-lane track.
- **`save()`** — personal-best time and distance survive between runs.

## Controls
- **Menu:** Up/Down choose event, A (Z) enter, B (X) back.
- **Sprint:** alternate **Z** and **X** to run; **A**… wait — A is Z. Use the
  on-screen prompt: mash Z/X to build speed, then tap **Up** (or the dedicated lean
  shown on screen) at the tape to LEAN. *(Implementation note: Z/X are BTN_A/BTN_B,
  so the "clutch" press uses BTN_UP to stay distinct from the mash keys.)*
- **Long jump:** mash Z/X down the runway, press **Up** to JUMP at your chosen angle.
- **Anywhere:** B / ESC backs out to the menu.

## Lean into / read
`excitebike.c` (the mode/phase state machine, side-scroll camera, dust + engine-pitch
sound, results panel + `save_bytes` best), `juice.c` (shake/flash/squash/particles at
the impact moments). Skip: a tilemap (single-lane track is primitives), multiplayer,
more than two events — depth on the mash + clutch press over breadth of events.
