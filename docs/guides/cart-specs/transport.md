# The game you're building: TRANSPORT

## Premise
A pocket-sized **Transport Tycoon**: two towns sit on a green tile world with a river
cutting between them. You **lay a rail line by hand** — clicking tiles to grow track
out from town A — and once the line reaches town B you **dispatch a train** that
shuttles back and forth, hauling cargo from the producing town to the consuming town
and paying out **cash on every delivery**. It's a build-then-watch loop: the joy is
seeing your own crooked little line come alive with a chuffing locomotive while the
demand counter climbs and the bank balance ticks up.

## The locked slice (build exactly this — nothing more)
- A tile `map()` world: grass, water (impassable), and two **town tiles** (A and B).
- **Lay track** by clicking grass tiles. Track only attaches to a tile orthogonally
  adjacent to an existing track end (or to town A to start the line) — so the player
  draws one continuous path, not a scatter of disconnected rails.
- When the track **connects A to B**, the line is "open" and town A starts
  **accumulating cargo** (a demand/stockpile counter that fills over time).
- **Click a town** to dispatch the idle train. It runs the path A→B, **delivers**
  whatever cargo A had stockpiled, pays cash (price × units), then runs back B→A empty
  and waits. Click again to send it on the next run.
- HUD: **cash**, **cargo waiting at A**, line status, and the per-delivery payout.
- A gentle win flourish at a cash goal; restart key.

**Cut (do not build):** signals, multiple vehicles, terrain shaping/bulldozing,
AI competitors, multiple cargo types, station catchment radius, curves/junctions
beyond a single linear path.

## The engine features it leans into (and why)
- **`map()` + `mget`/`mset`** — the world is a real tile grid (grass/water/town), and
  track is written into a parallel grid; the path the train follows is just the cells
  the player marked. This is the "maps when the game has a world" call.
- **Mouse-first input** — `mouse_x/y`, `mouse_pressed(MOUSE_LEFT)`, and a
  pixel→cell conversion drive both track-laying and town-dispatch. Hover highlight
  shows the next legal tile.
- **Path following with `lerp`/`dt`** — the train interpolates between consecutive
  track cells at a constant speed, framerate-independent. (Idiom borrowed from
  `towerdefense.c` creeps + `racer.c` motion.)
- **Sound that matches the act** — a soft *tick* on each track tile placed, a
  rhythmic chuff (`note` on a short-decay square instrument) while the train rolls, a
  bright `chord` cha-ching on delivery, a low denied blip on an illegal click.
- **Juice where it earns it** — the cash counter `lerp`s toward its target so payouts
  roll up; a coin pops over the town on delivery; `shake` a touch on the big delivery.

## Controls
Mouse-primary. **Left-click a grass tile** next to your track's growing end (or on
town A to begin) to **lay one rail tile**. Once the rail reaches town B the line opens
and cargo builds at A. **Left-click either town** to **dispatch the idle train** down
the line. **R** restarts the map. (Right-click does nothing — kept simple on purpose.)
