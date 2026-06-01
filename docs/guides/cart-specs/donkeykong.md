# The game you're building: DONKEY KONG

## Premise
The arcade **25m girder stage**, faithfully sliced. You're a little jumping man at the
bottom-left of a tower of **slanted red girders** linked by **ladders**. At the top sits
**Kong**, who hurls **barrels** that roll down the girders, tumble down ladders, and pick a
random path. Climb to the **princess** at the very top before a barrel touches you or you
fall too far. Jump barrels for points; grab a **hammer** to smash them for a few mad seconds.
Three lives. One screen, no scrolling — the whole stage fits the canvas.

## The locked slice (build exactly this — no more)
- One static stage: ~6 slanted girders + the ladders that connect them, drawn from
  primitives (no scroll, no extra levels, no bonus girl-rescue cutscene beyond a win flash).
- **Run** left/right along a girder (you follow its slope), **climb** up/down a ladder when
  you're lined up with one, **jump** a short arc.
- **Kong** at top-left lobs a barrel every couple of seconds. Barrels roll down the current
  girder, fall off its low end onto the next, and *sometimes* take a ladder straight down.
- **Jump a barrel** as it passes under you → +100, a little "boing". **Touch a barrel** → lose
  a life (shake + flash). **Fall** more than ~1.5 girder-heights → lose a life.
- One **hammer** pickup mid-stage: grab it and for a few seconds you swing it; any barrel you
  touch is smashed for points instead of killing you (the classic frenzy).
- Reach the **princess** platform at the top → you win, score bonus, restart.
- Lives, score, hi-score (`save()`), and a win/lose/restart loop.

## Engine features it leans into (and why)
- **Primitives over a sprite sheet.** Girders, ladders, Kong, the man, barrels, the princess
  and hammer are all `rectfill`/`circfill`/`line`/`tri` — chunky, recolorable, zero slot budget,
  and the slanted girders are trivial as slope math instead of fighting a tile grid.
- **Slope-aware platforming.** Each girder is a line `y = base - slope*x`; the man's feet snap
  to it, so "run uphill" falls out of the geometry — a clean fit for `lerp`/`clamp`/`sgn`.
- **A pool of barrels** (fixed array, no heap) each carrying a simple state machine
  (`ROLL`/`LADDER`/`FALL`), the boids/robotron idiom scaled down.
- **Sound where it earns it:** Kong's barrel-toss thump (NOISE), a rolling tick, the jump
  "boing" (SQUARE), a hammer-smash crunch, a death sting, and a little victory `strum`.
- **Juice at the impacts:** `shake` + red `fade` flash on death, `blink` on the hammer and the
  princess, a hammer-swing arc, anim on Kong's arms.

## Controls
- **Left / Right** — run along the girder (auto-follows the slope)
- **Up / Down** — climb a ladder you're standing on / under
- **Z** — jump (over barrels; you can't climb mid-jump)
- Any key / Z — restart after win or game over
