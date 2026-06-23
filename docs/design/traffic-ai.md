# traffic-ai — NPC driving behaviours, prototyped as a TRAFFIC mode in trackgen

STATUS: BUILDING (2026-06-23). **Shipped:** the RACE/TRAFFIC toggle + the foundation —
lane-keeping, car-following (IDM-style time-gap), and OVERTAKING (a blocked car pulls into a
clear adjacent lane to pass; a following car brakes to a stop and never reverses). Lanes are
emergent from lateral position, so the player participates (stop in a lane → traffic passes you).
Spec covers it (closing→brake, clear→accelerate, blocked→change lane, flow, no pile-ups). **Rough
edge:** on the tightest procedural corners a fast car can still clip the apex (concentrated at one
corner per track; it recovers). **Next:** phantom jams, figure-8 right-of-way, speed zones, hazard/merge.

The racing rivals in `trackgen.c` proved the core:
one shared physics step (`step_car`) + a parameter-driven follow-controller (`drive_ai`) +
mass-weighted collisions. **Racing is one ruleset on that brain; traffic is another.** This
doc plans the **TRAFFIC mode** — same generated track, same cars, *not a race* — and the
behaviours worth testing there. It is the buildable home for two open backlog items:
sloop's **rung 4 "NPC-driven traffic"** and streetlab's `＋` gaps **traffic-signal *state*** and
**pedestrian-crossing *logic*** (see [`big-game-backlog.md`](big-game-backlog.md)).

## Decision: a MODE in trackgen, not a new cart

Racing and traffic share ~90% of the cart — the generated track (`cl[]`/`nl[]`), the `Car`
struct, `step_car` physics, the `drive_ai` follow-controller, and the collision resolver. A
separate cart would duplicate all of that (which `cart-dupes.js` exists to catch) or force a
header extraction now. So: add a **RACE / TRAFFIC** toggle to the setup panel; mode 1 stays the
race, a new mode runs free-flowing traffic. This *is* the cart's thesis — "same brain, different
soul" — lifted from the personality level to the mode level. `trafficjam.c` is a gag cart and
does **not** overlap.

**Escape hatch (the split trigger):** if TRAFFIC mode grows large (full IDM + lanes +
intersections + density UI), that is the signal to extract the shared core (`Car` + `step_car` +
`drive_ai`) into a header (`steer.h`-style) that *both* a race cart and a traffic cart include —
a clean split with zero duplication. Flag it when we cross that line; do not pre-build it.

## What makes trackgen a good traffic lab

- **The closed loop is the ring-road experiment.** Enough cars on a loop with reaction delay
  produce a stop-and-go wave from *nothing* — the canonical phantom-jam testbed.
- **The Figure-8 style crosses itself** — a free intersection for right-of-way prototyping.
- **Everything is deterministic + headless-specable** — flow, jams, and "no collision at the
  crossing" are all measurable in `spec()` (the cart already runs 14 assertions).

## The systems to test (rough dependency / leverage order)

| # | System | What it tests | Ports to | Spec angle |
|---|---|---|---|---|
| 1 | **Lane-keeping + car-following (IDM)** ✅ | hold a lane; adjust *throttle* to keep a safe time-gap to the leader **in your lane** (not just braking for corners) | every road in sloop | platoons form; gap never drops below a min |
| 2 | **Phantom traffic jam** | a stop-and-go wave emerges from density + reaction lag, no obstacle | highway flow realism | speed-variance wave travels *backward* round the ring |
| 3 | **Overtaking (gap acceptance)** ✅ | change lanes to pass a slower leader *only* when the adjacent lane's gap is clear ahead+behind | multi-lane roads | no lane-change into an occupied gap |
| 4 | **Intersection right-of-way** | at the figure-8 crossing, yield/priority so cars don't T-bone | streetlab junctions | zero collisions at the crossing point |
| 5 | **Speed zones** | obey a per-section speed cap (corner / school zone) | road speed limits | cars under cap inside the zone |
| 6 | **Static hazard + merge** | route around a broken-down car and merge back | obstacles, parking, breakdowns | all cars clear the hazard, no pileup |

**Foundation = #1.** Today `drive_ai` only brakes for *corners* and "lanes" are a static lateral
bias (`line_off`); real **longitudinal car-following** (the Intelligent Driver Model — match the
leader's speed, keep a time-gap) is the one primitive that unlocks #2 (jams), #3 (overtaking),
and realistic platooning. It is also the most portable piece to sloop and matches the
`car-following` teaches-tag the cart already claims.

## How TRAFFIC mode differs from RACE mode

- No laps / standings / finish; cars cruise at their own desired speed indefinitely.
- Mixed desired speeds per car (the existing `spd_cap` already varies by personality — reuse it).
- Lanes instead of one racing line; cars hold a lane and only leave it to overtake (#3).
- Player is just *in* the traffic (free-drive, no win condition) — or a "spectate/observe"
  camera for watching jams form. Density (car count) becomes a setup lever.
- Optional two-way: some cars driving the loop in the opposite sense in an oncoming lane.

## Related

- [`big-game-backlog.md`](big-game-backlog.md) — sloop rung 4 (NPC traffic) + streetlab's
  signal-state / ped-logic `＋` gaps that this feeds.
- [`showreel-teaser.md`](showreel-teaser.md) — the larger lo-fi-GTA the traffic AI serves.
- `trackgen.c` header — the source-of-truth contract for `Car` / `step_car` / `drive_ai` /
  `resolve_collisions` (the shared brain this mode reuses).
- [`flank-tactical-ai.md`](flank-tactical-ai.md) — the sibling agent-AI subsystem (combat brains).
