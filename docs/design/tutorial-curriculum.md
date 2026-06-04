# Tutorial curriculum — extending the on-ramp (25+)

> **Exploratory.** A plan (2026-06-03) for the next wave of tutorial carts, sourced from
> studying the [Nerdy Teachers PICO-8 course](https://nerdyteachers.com/PICO-8/). Companion
> to [`cart-library-direction.md`](cart-library-direction.md) — that memo says *stop cloning
> games, fill the tutorial cliff*; this is the actual lesson list. Not a committed backlog;
> re-check the shipped set before building.

## The principle: adapt the arc, rewrite in C

Nerdy Teachers is the best beginner course in the fantasy-console world and PICO-8 is our
closest cousin, so its *teaching order* ports almost perfectly. But it is **not a port** —
it's an adaptation, for two reasons:

1. **Language + console differ.** PICO-8 is Lua, 128×128, 8×8 sprites, 16 colors, tracker
   audio. We are **C**, 320×200, 16×16 sprites, 32 colors, `sfx`/`note`/`music`. Every
   example gets reimplemented — and that rewrite is *where our own idioms get taught*:
   `STATE { }/S->field`, the `Enemy e[64]; bool on;` entity pool, `boxes_touch`, our
   palette names. Lua `tables` become C arrays + structs — a real lesson, not a translation.
2. **Licensing.** Their text and code are theirs (study the structure, write our own). We
   adapt pedagogy; we do not copy.

## What we already ship (1–24) — do NOT duplicate

The current numbered on-ramp (`editor/public/carts/index.json`):

| # | cart | # | cart |
|---|---|---|---|
| 1 | hello screen | 13 | easing |
| 2 | shapes & colors | 14 | hud layout |
| 3 | things that move | 15 | animation phase |
| 4 | press a button | 16 | spirograph |
| 5 / 5b | draw character / colorkey | 17 | hypotrochoid |
| 6 | sounds & music | 18 | particles |
| 7 | keeping score | 19 | juice |
| 8 | catch the star *(first tiny game)* | 20 | effects |
| 9 | enemies! | 21 | type & save |
| 10 | build a world *(map editor)* | 22 | fill patterns |
| 11 | noise | 23 | holes & colors |
| 12 | high score | 24 | moving patterns |

So these Nerdy Teachers topics are **already covered — skip them**: animate sprite (→15),
function spr / facing (→5), screen shake + particles (→18, 19), simple movement (→3, 4),
sound & music (→6), score & hi-score (→7, 12), the map editor (→10).

**The cliff:** 1–24 teach *primitives and effects* but never teach (a) the **C language
itself** as explicit lessons, (b) **collision** as a topic, or (c) how to **assemble a
whole game start-to-finish**. That's the entire next wave.

## The plan — four tracks, ~24 new carts (25–48)

Numbers are sequential suggestions; tracks can ship in any order. Each cart = source in
`tools/carts/<name>.c`, baked screenshot, registered in `index.json` (see
[`cart-authoring.md`](../guides/cart-authoring.md)).

### Track A — How code actually works *(the missing foundation)*

Our tutorials *use* variables, loops and functions from cart 3 onward but never *teach*
them. For a learning tool that's the biggest hole. Mirrors Nerdy Teachers' "Intro to
Coding" roadmap (conditionals → inputs → tables → loops), retargeted to C.

| # | cart | teaches |
|---|---|---|
| 25 | variables & types | `int` / `float` / `bool`; why the compiler needs a type |
| 26 | conditionals | `if / else if / else`, comparisons, `&&` `||` |
| 27 | loops | `for` / `while`; drawing a row/grid with one loop |
| 28 | functions | writing your own; parameters and `return` |
| 29 | arrays | `int things[8]`; looping over a fixed bank |
| 30 | structs | bundling fields; `typedef struct { } Thing;` |
| 31 | the entity pool | `Enemy e[64]; bool on;` + skip-inactive — VISION's *core* pattern |
| 32 | friendly state | `STATE { } / S->field` sugar; what `de_state()` is for |

### Track B — Collision *(adapted from their 8-part track — our biggest acknowledged gap)*

| # | cart | teaches |
|---|---|---|
| 33 | what is collision? | overlap as a yes/no question; the bounding-box idea |
| 34 | point in rect | is a dot / the cursor inside a box |
| 35 | box vs box (AABB) | two rectangles touch — teach the `boxes_touch` helper |
| 36 | circles & distance | point/circle and circle/circle via distance |
| 37 | screen edges | clamp vs bounce vs wrap at the boundary |
| 38 | tilemap collision | reading map flags: "what tile am I on", walls you can't pass |
| 39 | reading pixels | color/pixel-level collision off the canvas |

### Track C — Build a whole game *(the capstones — `cart-library-direction.md`'s #1)*

Each is a *complete* game with title → play → game-over, the arc nothing currently teaches.
Modeled on their Bitesize set, scaled to teachable size.

| # | cart | teaches | source echo |
|---|---|---|---|
| 40 | **flappy (one-button)** | the smallest complete game; state machine + hi-score + juice | *quickest high-value win* |
| 41 | pong | ball physics, two paddles, 2-player input | Nerdy Pong |
| 42 | catcher / fruit drop | spawn waves, lives, "miss = lose" | Fruit Drop |
| 43 | breakout | brick array + rect collision (pays off Track B) | — |
| 44 | fishy | eat smaller / flee bigger; relative size & growth | Fishy |
| 45 | snake | grid movement, a growing tail array | Cells |
| 46 | tiny platformer | gravity, coyote time, tilemap collision | — |

### Track D — Presentation polish *(the few mechanics we lack)*

| # | cart | teaches |
|---|---|---|
| 47 | text animations | typewriter / wipe / shine reveals (their text-wipe + text-shine) |
| 48 | advanced movement | acceleration + friction; 8-way top-down vs platformer feel |

## Sequencing note

A beginner's natural path becomes: **1–10** (primitives → first game → world) → **25–32**
(understand the code they've been copying) → **33–39** (collision) → **40–46** (ship whole
games) → **11–24 + 47–48** (juice/effects/polish, dip in as desired). The capstones (Track
C) are the payoff and the most shareable; Track A is the foundation that makes the rest
*stick* instead of being copied.

## Count

~24 net-new (25–48) on top of the existing 24 → a ~48-cart course. "About 30" is the right
order of magnitude; trim Track D or fold a couple of Track C games if it runs long. Suggested
first build, per the direction memo: **#40 flappy**.

## Addendum (2026-06-04) — movement-paradigm clusters

A pattern emerged while building: pick a *movement paradigm*, distill it to a minimal
colored-rects cart, verify the feel with the debug harness, then ship variants. The
paradigm map and where each cluster stands:

| paradigm | position is… | carts |
|---|---|---|
| **solid-space** | free 2D + gravity; collision discovers ground | ✅ `platform-rects`, `platform-paint`, `platform-tiles` |
| **rail / graph** | which edge + how far along it | ✅ `platform-rails`, `platform-loop` |
| **grid / discrete** | which cell (+ a tween to the next) | ✅ `grid-move`, `zelda-walk` (+ sokoban/boulderdash as the rule references) |
| **steering / inertia** | position + heading + velocity | ✅ `thrust`, `steer` |

**Grid cluster — remaining:**
- ✅ **`zelda-walk` (grid-nudge)** — SHIPPED 2026-06-04. The hybrid the original *Legend of Zelda* used: Link
  moves freely (feels analog), but while walking, the **perpendicular axis gently eases
  back onto the half-grid** — walk any longer distance and you're aligned again. The
  payoff is you almost never snag on block corners: free feel, grid forgiveness. Great
  A/B cart: same room, toggle the nudge off and feel corner-snagging appear. (Spotted
  by Nikki — the secret middle ground between `grid-move` and `platform-rects`-style
  free movement.)
- ~~`grid-push`~~ / ~~`grid-fall`~~ — **COVERED, not building** (decided 2026-06-04):
  the shelf already proves both, cleanly. `sokoban.c` (193 lines) is the reference
  push-rule implementation — and carries a bonus teachable pattern, its undo history.
  `boulderdash.c` (341 lines) is the reference settle-pass, self-documented ("gravity
  is one bottom-up settle pass (the sand.c idiom), pushing is the sokoban rule").
  Per the library-direction rule: don't duplicate what the shelf already proves —
  point learners at those sources. Grid cluster complete:
  `grid-move` (feel) → `zelda-walk` (hybrid) → sokoban/boulderdash (rules applied).

**Steering cluster:**
- ✅ **`thrust`** — SHIPPED 2026-06-04. Asteroids ship done well: *facing ≠ velocity*,
  thrust adds along the heading, drag, screen wrap, exhaust puffs that shoot backward
  from the nozzle. The velocity vector is drawn as a green line so the facing/velocity
  disagreement is visible mid-drift; X toggles "tank mode" (velocity = facing,
  release stops dead) as the A/B.
- ✅ **`steer`** — SHIPPED 2026-06-04. The car model: speed-scaled steering (a parked
  car can't turn), gas along the heading, and GRIP as a velocity→heading lerp — fast
  lerp carves, slow lerp drifts. Skid marks fire exactly when lateral slide exceeds
  the threshold (harness-validated: racing tires peak at lat 0.51 vs the 0.55
  threshold; drift tires hit 1.40 on identical input). X swaps tire types.

**With this, all four movement-paradigm clusters are COVERED** (solid-space, rail,
grid, steering) — the taxonomy that began with the platformer carts is complete.

**Smaller candidates:** `drag-and-drop` done well (pick-up threshold, hover, drop
targets, spring-back — every toy hand-rolls this); waypoint followers (tower-defense
creeps — conceptually *rails for enemies*).

**Collision labs (Track B, consolidated to three interactive carts).** A new cart
style: draggable mouse-driven visualizers rather than mini-games — the test itself
is the content. (Also the first tutorials to teach the mouse API at all.)
- ✅ **`collision-lab-1` (points + boxes)** — SHIPPED 2026-06-04. Axis-shadow bars
  show that AABB overlap = X-shadows overlap AND Y-shadows overlap; the four live
  comparisons print with real numbers and flip green/red as you drag; the cursor is
  the point for `point_in_box`.
- ✅ **`collision-lab-2` (circles + distance)** — SHIPPED 2026-06-04. The right
  triangle between the centers drawn live (Pythagoras), a 1:1 ruler showing dist vs
  ra+rb, and the no-sqrt readout: `dist² <= (ra+rb)²` with real numbers — what
  `near`/`circles_touch` actually compute.
- ✅ **`collision-lab-3` (the world)** — SHIPPED 2026-06-04. The three environment
  flavors, cheapest first: edges (clamp/bounce/wrap on the same wall, X cycles),
  tiles (`touching_map` with the overlapped cells highlighted live), pixels
  (`touching_color` against a drawn blob — the Worms-terrain trick), plus
  `tile_at`/`pget` cursor readouts.

- ✅ **`collision-lab-4` (circle vs box)** — SHIPPED 2026-06-04. The clamp trick:
  two clamps find Q, the closest point on the box, then it's lab 2's distance test
  again — "new shape pairs reduce to old tests". Center-inside falls out free.
- ✅ **`collision-lab-5` (lines)** — SHIPPED 2026-06-04. Point-vs-segment (clamped-t
  projection, the foot drawn, thickline as the visible hit band) and segment-vs-
  segment (four cross products, opposite-sides test) — the anti-tunneling test that
  `linerider` hand-rolled.

**Track B (collision) complete** — the original 33–39 plan, consolidated into five
interactive labs: points/boxes, circles/distance, the world, circle-vs-box, lines.
