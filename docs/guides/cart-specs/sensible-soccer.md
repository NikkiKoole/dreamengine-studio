# The game you're building: SENSIBLE SOCCER

## Premise
Top-down arcade football. One timed match, your team vs the CPU, on a vertically-
scrolling pitch. Fast, fluid, one-button. The soul of the game is **after-touch** —
bend the ball in flight by holding a direction after you kick. Get that feeling right
and everything else follows.

## View & camera
Top-down, **vertical** orientation (you attack up the pitch, then swap ends at half).
The pitch world is taller (and a touch wider) than the 320×200 screen; `follow()` the
ball, clamped to pitch bounds. Players are **small** (~8–12px) so a big slice of pitch
is on screen at once.

## Core mechanics
- The **ball** is its own entity: float position + velocity + friction, plus a **height
  (z/vz)** for lobs, with its shadow drawn separately on the ground.
- **Kick:** press the button → ball leaves in the player's run/facing direction; power
  by tap-vs-hold.
- **After-touch (headline):** while the ball is live and you hold a direction, gently
  steer/curve it — the Sensi bend. This is the skill ceiling; tune it until it feels
  great (subtle accel toward the held dir, stronger on lobs).
- **Control:** you drive the teammate **nearest the ball**; control **auto-switches** as
  proximity/possession changes. Run into a loose ball to gain it; button near an
  opponent-with-ball = tackle (with a slide + dust).
- Movement: 8-direction with momentum; dribbling nudges the ball just ahead of the
  running player.

## AI
Opponent team + your other teammates are CPU. Keep it readable, not perfect: nearest
defender chases the ball (`boids` seek/arrive), the rest hold **formation slots** that
shift with ball position and side; goalie tracks the ball on its line and lunges at
close shots.

## Match flow / states
KICKOFF → PLAY → GOAL (celebration + reset) → HALF-TIME (swap ends) → FULL-TIME
(win/lose/draw screen, `save()` best result). Timed halves (~2 min, configurable),
score + clock HUD. Out-of-play → auto throw-in / goal-kick / corner placed for you
(**no manual set-piece aiming**).

## Pitch & teams (sprites + maps)
- **Pitch** = tile `map()` green with mow-stripe tiles; markings (touchlines, center
  circle, penalty boxes, spots) drawn with `line`/`circ`/`oval`; goals + nets as
  primitives/sprites.
- **Players** = one small sprite set (idle + a 2–4 frame run cycle), **recolored per
  team via `pal()`** (shirt + shorts placeholder indices — the `crowd.c` magic-color
  trick). Goalies a distinct color. Two team palettes picked at kickoff from one set.
- Depth/feel: y-sort players, ball shadow, a simple crowd/stand border.

## Make it juicy
- Ball **shadow lifts** off the ball on lobs (height); ball **squash** on bounce.
- **Goal:** net ripple, screen `shake`, white flash, a **crowd-roar swell** (filtered-
  noise crescendo) + a chant/stab chord, a brief freeze/slow-mo, `print_scaled` "GOAL!".
- **Ref whistle** (kickoff / goal / full-time) — a sharp instrument blip.
- **Crowd-ambience bed** that rises in brightness/volume as the ball nears either goal
  (tie the sound to danger).
- Slide-tackle dust particles; speed lines on a long ball.

## Data model (suggested)
- `struct Player { float x,y,vx,vy; int team, role; ... } players[2][N];` + a
  controlled-index per side.
- `struct Ball { float x,y,vx,vy,z,vz; int owner; };`
- match: `score[2], clock, half, state`.
- formation slot tables, offset by ball position + side.

## Controls
D-pad / WASD move; **one button (Z)** = kick (power by tap/hold) / tackle when
defending; **after-touch = hold a direction after the kick** while the ball is live.
Optional **2-player**: player 2 on the arrow keys. No mouse.

## Lean into / read
`crowd.c` (pal team colors + tiny sprites + tile map + y-sort), `rocketleague.c` (ball
physics + sports match loop), `boids.c` (formation/seek AI), `effects.c`/`juice.c`
(shake/flash/slow-mo/particles), `drummachine.c`/`omnichord.c` (crowd bed + stings).
Skip: leagues/management, manual set-pieces, offside, fouls beyond a simple tackle —
keep it arcade.
