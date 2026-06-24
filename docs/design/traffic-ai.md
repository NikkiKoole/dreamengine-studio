# traffic-ai — NPC driving behaviours, prototyped as a TRAFFIC mode in trackgen

STATUS: BUILDING (2026-06-23). **Shipped:** the RACE/TRAFFIC toggle + lane-keeping, car-following
(IDM-style time-gap), OVERTAKING (blocked car pulls into a clear lane; a follower brakes to a stop,
never reverses), a crowd of TRAFFIC_CARS at density, speed-tinted cars (red=stopped→green=flowing),
a cycling TRAFFIC LIGHT (red = a stop-line all lanes queue behind), and a reaction lag (REACT_N)
that makes dense following unstable → PHANTOM JAMS emerge with no cause (the ring-road experiment).
Lanes are emergent from lateral position, so the player participates (stop in a lane → traffic
passes you). Collision is an ORIENTED box (long-along-heading, narrow-across), so adjacent-lane cars
never falsely touch. Spec (66 assertions) covers closing→brake, clear→accelerate, blocked→change-lane,
flow, no box-overlap, red-builds-a-queue / no-bolt / no-reverse, stop-and-go spread, and the
**second-track crossing** with TWO selectable junction types (geometry/crossings; second-track stream
spawns + flows + stays on its track; **SIGNAL reservation** and **ROUNDABOUT** — each crash-free with
no gridlock and both tracks flowing, across 4 seeds). **Rough
edge:** on the tightest procedural corner a fast car can still clip the apex (localized, recovers).

> **DESIGN PIVOT (2026-06-24):** the "cross-road" started as a straight road cutting the loop
> (Phases A–C below), but it's now a **second full race track** — an independent loop (`gen_track2`,
> a derived seed rotated 90°, same centre/size) crossing the first. It "feels more real and has the
> same situations" (the owner's call). Mechanically it was a *simplification*: both tracks are closed
> loops, so `road_loops` is true for both and the brain needed no new cases — the second track's cars
> just go round (no respawn-at-ends). Crossings became loop×loop segment intersection (`find_crossings`,
> ~4–6 points). Everything below describing a straight "cross-road" now means "the second track"; the
> right-of-way logic is unchanged (priority track 1, give-way track 2). `TRACKS:2` setup toggle.
**Next:** the cross-road intersection (#4) — a big L→R road cutting the loop (~twice) with its own
traffic, built in phases A–D (geometry → cross-traffic → right-of-way → tune); see the sketch below.
**Phase A is shipped (2026-06-23):** a `CROSS` setup toggle lays a straight cross-road across the
loop's bbox (overhanging into the grass) through the loop's centroid-y, so it cuts the closed loop
at an even number (≥2) of points; same ribbon model as the loop (`cl2[]`/`nl2[]`), crossings found
by straddle-test and stored in `xpt[]` with the sample index on **both** roads (`prog1` loop,
`prog2` cross). Rendered in-world and to-scale in the setup preview; markers (indigo discs) sit on
both ribbons. **Phase B is shipped too (2026-06-23):** a `TRAFFIC_CROSS`-car stream now drives the
cross-road via the SAME `drive_ai_traffic` brain — the only generalization is one `Car.road` field
+ a `road_*()` accessor layer (the loop wraps, the cross-road clamps), exactly the portability
thesis. **Phase C is shipped too (2026-06-24):** priority-road right-of-way — the cross-road gives
way at each junction via gap acceptance (`crossing_yield_gap`), with a crash-avoidance brake on the
priority road; tuned to **0 T-bones with both roads flowing** (a thinner loop crowd, `TRAFFIC_CARS_X`,
opens the gaps). 12 spec assertions now span A+B+C. **Next: Phase D** — feel/tune + setup toggles
(cross-road on/off, which rule). Then speed zones / hazards / the more-ideas list.

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
  crossing" are all measurable in `spec()` (the cart already runs 24 assertions).

## The systems to test (rough dependency / leverage order)

| # | System | What it tests | Ports to | Spec angle |
|---|---|---|---|---|
| 1 | **Lane-keeping + car-following (IDM)** ✅ | hold a lane; adjust *throttle* to keep a safe time-gap to the leader **in your lane** (not just braking for corners) | every road in sloop | platoons form; gap never drops below a min |
| 2 | **Phantom traffic jam** ✅ | a stop-and-go wave emerges from density + reaction lag, no obstacle | highway flow realism | speed-variance wave travels *backward* round the ring |
| — | **Traffic light** ✅ (bonus) | a cycling red/green signal; a red is a stop-line all lanes queue behind — a controllable bottleneck | streetlab signal-state gap | a red builds a queue of stopped cars |
| 3 | **Overtaking (gap acceptance)** ✅ | change lanes to pass a slower leader *only* when the adjacent lane's gap is clear ahead+behind | multi-lane roads | no lane-change into an occupied gap |
| 4 | **Intersection / cross-road right-of-way** | a SECOND road (its own traffic stream) crosses the loop; cars on both must yield so they don't T-bone at the crossing. The figure-8 self-crossing is the cheap prototype; a dedicated cross-road through the world is the real thing (player's ask, 2026-06-23). Rules to try: priority road, give-way, 4-way-stop, or the cross-road's own light. | streetlab junctions; the world composer's road graph | zero collisions at the crossing over a long run |
| 5 | **Speed zones** | obey a per-section speed cap (corner / school zone) | road speed limits | cars under cap inside the zone |
| 6 | **Static hazard + merge** | route around a broken-down car and merge back | obstacles, parking, breakdowns | all cars clear the hazard, no pileup |
| 7 | **Pedestrians at crossings** | peds wait, then cross at a crosswalk; cars yield/stop for them | streetlab's open ped-logic `＋` gap | a ped crossing halts the cars; nobody is hit |
| 8 | **Two-way traffic** | an oncoming lane — some cars drive the loop the other way; head-on lane discipline | two-way roads | oncoming cars never stray into the wrong lane / collide head-on |
| 9 | **Coordinated lights ("green wave")** | multiple signals phased so a platoon catches greens in sequence | streetlab signal-state, timing | a car at the speed limit hits consecutive greens |
| 10 | **Emergency vehicle** | a siren vehicle others yield to (pull aside / stop) | game events, NPC reactions | regular cars clear a lane for it |
| — | **Light-bar polish** (small) | the signal reads subtly from far away; make it pop more / optional camera nudge to it | — | n/a (visual) |

**Foundation = #1.** Today `drive_ai` only brakes for *corners* and "lanes" are a static lateral
bias (`line_off`); real **longitudinal car-following** (the Intelligent Driver Model — match the
leader's speed, keep a time-gap) is the one primitive that unlocks #2 (jams), #3 (overtaking),
and realistic platooning. It is also the most portable piece to sloop and matches the
`car-following` teaches-tag the cart already claims.

## Sketch: the cross-road intersection (#4, the next build)

The vision (2026-06-23, resolved): lay **one big straight road across the world, left → right**,
cutting through the loop. Because the loop is a closed circuit, a straight chord across it
**crosses it at (at least) two points**. The cross-road has **its own traffic**, and cars on both
roads must negotiate each crossing so they don't T-bone. The figure-8 self-crossing was the cheap
warm-up (one path, two passes); this is two *independent* streams meeting — the first time **two
roads, not one loop, share the sim** (the direct prototype of streetlab junctions + the world
composer's road graph).

**Shape in trackgen's model:** a second centre line `cl2[]` (a straight L→R line spanning the map,
with its own normals). Cross cars are just more `Car`s with a `road` field (0 = loop, 1 = cross),
run by the **same `drive_ai_traffic`** following `cl2[]` instead of `cl[]` — the brain doesn't care
which line it follows (the whole portability thesis; the only generalization is "which line am I
on"). Crossing points = segment-intersections of `cl[]` against `cl2[]` (expect ~2). Cars wrap /
respawn at the cross-road's ends (it's not a loop) or it bends back — decide in Phase A.

### Phases (build in order, spec each)

- **Phase A — geometry. ✅ (2026-06-23)** Generate `cl2[]` as a straight road L→R across the world
  bounds + its normals; render it (same ribbon/curb code). Compute the crossing point(s) by
  segment-intersection vs `cl[]`; store them (`xpt[]`, with the prog index on each road). Draw a
  marker at each crossing. *No cross-traffic yet.* Spec: ≥1 crossing found; markers sit on both
  ribbons. **Done:** `CROSS` setup toggle; `gen_cross()` lays the road through the loop centroid-y
  (guarantees an even ≥2 crossings); straddle-test finds crossings (cheaper than full segment×segment
  since the road is horizontal); rendered in-world + to-scale in the preview; white/blue curbs +
  blue-green centre dashes distinguish it from the loop's red/white. 4 spec assertions.
- **Phase B — cross-traffic, naive. ✅ (2026-06-23)** Spawn a pool of cars on `cl2[]` driven by
  `drive_ai_traffic` (generalized to follow either line). They drive across and respawn/loop at the
  ends. They will *collide* with loop cars at the crossings — that's expected; it sets up Phase C.
  Spec: cross cars make forward progress along `cl2[]`. **Done:** one new field `Car.road` (0 loop /
  1 cross) + a `road_*()` accessor layer (`road_cl/road_nl/road_idx/road_seg/prog_ahead`) is the
  *entire* generalization — `step_car`, `drive_ai_traffic`, `lead_gap`, `lane_clear` now read their
  line through it and don't care which road they're on (the loop wraps, the cross-road clamps). The
  cross stream is `TRAFFIC_CROSS` cars packed into `car[ncars .. ncars+ncross)`, spawned/respawned
  by `put_car_on_cross`; `lead_gap`/`lane_clear` filter to same-road leaders; `resolve_collisions`
  spans both pools (so crossings collide — Phase C's job). The loop traffic-light only affects road 0.
  4 spec assertions (stream spawns, makes L→R progress, flows, every car stays on its road).
- **Phase C — right-of-way (the heart). ✅ (2026-06-24)** At each crossing, a car computes its
  time-to-cross; if a conflicting car (other road) will be inside the **intersection box** within
  that window, it **yields** — reuse the red-light trick: treat the crossing as a temporary
  stop-line leader (gap = distance to the crossing, vlead = 0) until the box is clear. Start with
  **priority road** (the loop has right-of-way; the cross-road gives way); later try give-way /
  4-way-stop (first-come) / a light on the cross-road. Spec: **zero box-overlaps at any crossing**
  over a long run, while both roads keep flowing (not gridlocked). **Done** (`crossing_yield_gap`):
  the give-way road does proper **gap acceptance** — it enters a junction only when (a) the box is
  physically empty *and* (b) no priority car will reach it within the car's time-to-clear (predicted
  at city pace, so a car idling at the light still counts). **A subtlety the build surfaced:** pure
  priority-road + slow car acceleration (ACCEL 0.07) + a dense loop = the give-way road *starves*
  (real, but it failed the "both flow" spec). Two fixes, both realistic: **thin the loop crowd**
  when a cross-road is active (`TRAFFIC_CARS_X` 20 vs 32, so gaps actually open), and give the
  **priority car a crash-avoidance brake** — it keeps right of way but won't drive *through* a
  give-way car that has mis-committed into the box. Net: **0 T-bones over ~1440 frames, both roads
  flow, cross cars get through** (4 spec assertions). Junctions render as a ring (red = a priority
  car holds it). **Flow tuning (the "they don't dare" fix):** the first cut was too timid — only ~13
  traversals/1440f and visible queues — because the box-occupancy guard blocked entry on *any* car,
  serialising the give-way road to one car per gap. Fix: only a *priority* car blocks the box (a
  same-road car ahead just platoons through), assume a brisker crossing speed, and let the priority
  car start its crash-brake at the give-way car's *commit* point (not dead-centre) so it has room to
  stop. Result: ~21 traversals/1440f, still 0 T-bones; the spec now asserts `crossings_done ≥ 12` so
  a regression to timid behaviour fails. The remaining rules (give-way / 4-way-stop / cross-road
  light) are Phase D toggles.
- **Phase D — feel + tune.** Visualize yields (a car waiting at a crossing), balance densities so it
  doesn't deadlock, and add a setup toggle (cross-road on/off, or which rule). Spec: no deadlock
  (both roads still moving after N frames). **In progress:** a **debug overlay** (key `D`) shows
  *why* each car is braking — a coloured dot above it (orange = car-following, red = red light,
  yellow = yielding, green = cruising) and, for a yielding car, a **line to the junction it's giving
  way at**. Built when the owner asked "why do the cars stop?" — the yellow lines make a queue's
  cause legible at a glance (yielding vs. a car-following cascade). A **`WHY_BLOCKED` (pink "stuck")**
  state answers the follow-up "they want to go but can't — show *that* too": a car commanding throttle
  yet not moving is physically wedged by another car, drawn pink with a line to the car blocking it.
  Density balancing (`TRAFFIC_CARS_X`) and the junction ring also landed here.
  **RIGHT-OF-WAY is now a JUNCTION RESERVATION (2026-06-24), replacing gap-acceptance.** The earlier
  gap-acceptance approach couldn't *guarantee* no crash — tuning (margins, look-ahead, crash-brake)
  only shuffled which frame the rare cross-track contact landed on, and on some reseeded tracks two
  streams WEDGED into a sustained standstill (the pink overlay surfaced it). The fix: each crossing is
  owned by at most **one car at a time** (`xowner[]`, `update_reservations`). A car claims a free
  crossing once within `XAPPROACH` *and* only if its exit is clear (so it never stalls in the box); the
  owner holds it until it drives through, then frees it; a timeout (`XHOLD_MAX`) breaks any stall.
  Everyone else stops short (`crossing_yield_gap` now just "do I own it? else stop"). Two cars can
  never share a junction → **no T-bones by construction, not by tuning**, and no wedge-deadlock. Spec
  proves it across **4 seeds** (no collision, no gridlock, both tracks still moving after a long run).
  The grant uses a **claim score** (soonest-to-arrive, biased by aggression = priority track + `PERS.aggro`)
  — the hook for **"crazy vs careful" drivers** (widen the bias, shorten a bold driver's stop gap, etc).
  The ring shows orange = reserved; the `D` overlay draws a line from each owner to its junction.
  **Throughput vs. crash-freeness (2026-06-24):** single-car ownership passes only one car per claim, so
  on a long run congestion can build (the junction clears slower than two busy streams arrive). A
  **per-track "green phase"** (a whole platoon streams through, then switch) was built to fix that — but
  it could NOT be made crash-free across seeds: at the phase boundary a prog-vs-physical mismatch on
  curved tracks let a rare cross-track contact through (the spec's over-time, multi-seed collision check
  caught 3 on one seed; tuning all-red/guards only shuffled it). Reverted to the proven single-car (0
  collisions over time on all 4 spec seeds) and instead **lightened the density** (`TRAFFIC_CARS_X` 13 /
  `TRAFFIC_CROSS` 10) so arrivals match one-at-a-time capacity — flows without buildup, stays crash-free.
  A crash-free per-track would need physical-distance (not prog-distance) box-clear checks at the switch;
  parked as a future option if higher density is wanted. Remaining: per-driver personality on the claim,
  and a rule toggle (priority / 4-way / signal).
- **ROUNDABOUT junction (2026-06-24) — a selectable second junction type** (setup `JCT:` toggle:
  SIGNAL = the reservation above, or RING). Both tracks merge onto a **one-way ring** centred on the
  crossing and circulate the **same direction**, so there is **no perpendicular conflict** — a botched
  merge is a same-direction graze, never a T-bone. Geometry: `ring_angle` finds where each track meets
  a radius-`RING_MULT×half` ring (`ringin`/`ringout` per track); a car yields to enter (`ring_entry_gap`:
  wait *outside* the ring, and — the owner's key insight — **"look before you merge"**, a physical check
  of the spot you're pulling into, which is what finally made it crash-free where abstract phase logic
  couldn't), circulates (`ring_move`, car-following by arc), and exits onto its track. **Crash-free
  across 4 seeds** in spec, with real throughput and continuous flow (no stop-and-go). Lesson learned:
  a car must wait *outside* the ring, never *on* it (a waiting-on-the-tarmac car got swept by a
  circulating one — the failing case the debug overlay pinpointed). Why it matters: a road-network cart
  wants **both** junction types. Density note: pushing the ring much past the signal's density grazes on
  degenerate shallow crossings, so its real win is smooth flow + being the right primitive, not raw count.
  Remaining: per-driver personality on the claim/merge; speed zones / hazards / peds / green-wave.

**Watch out for:** deadlock (two priority rules that both yield → everyone stops); pick an asymmetric
rule first. And the existing TRAFFIC light is on the loop — decide whether it stays, moves to the
cross-road, or governs the intersection.

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
