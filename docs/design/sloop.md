# sloop — build-your-own-vehicle, travel a procedural world (design seed)

**Status: building — rung 1 (drive) + 1.5 (drift) + 1.75 (schematic course) done.** Cart:
`tools/carts/sloop.c`, registered in `index.json`, lint clean. Captures a design
conversation (2026-06-09).
A new entry in the "legendary series" alongside `coaster` and `orbit`
(`tools/carts/coaster.c`, `tools/carts/orbit.c`) and
[`galerijflat`](galerijflat.md): one big honest system, executed straight, with the
world built to prove it. Working name **`sloop`** (Dutch *sloop* = demolition /
scrapping, and English = a boat — both fit a scrap-built rig crossing a wrecked
land). Alternatives parked under "Name" below; not committed.

## The idea

Build a vehicle out of parts on a grid — frame, engine, wheels, tank, seats, cargo,
plating — then drive it out into a procedurally generated world and see how far your
build actually gets you. Cataclysm: DDA's vehicle system is the direct ancestor.

The pitch in one line: **the vehicle is not a sprite with stats — it is a grid of
parts, and how it drives is *computed* from the parts you bolted on.** A big engine
on a heavy tank crawls; the same engine on a light frame rips; a lopsided wheel
layout pulls to one side; clip a rock too fast and the wheel that hit it snaps off
and your handling goes with it. Nothing is faked. That is the series' signature —
the same DNA as orbit ("the same integrator runs the live ship *and* the prediction,
so the dots never lie") and coaster ("same gravity-along-the-tangent physics core
for both modes").

Decisions so far:

- ✓ **2D top-down.** The series does real 3D elsewhere (flyover, textured3d, racers).
  This cart's honesty lives in the part-grid physics, not the renderer. Top-down
  means the build grid and the drive sprite are *literally the same cells* — that is
  what sells "you are driving the thing you built."
- ✓ **One honest rigid body.** The vehicle is a 2D rigid body: position, heading,
  linear velocity, angular velocity. Mass, centre of mass, and moment of inertia are
  all *derived from the part grid* every time the build changes — never hand-tuned.
- ✓ **A mode flip, like coaster's M.** **BUILD** (paused, place/remove parts, live
  readouts) ↔ **DRIVE** (that same grid is now a body loose in the world). The toggle
  is the spine of the cart.
- ✓ **The world is the test rig, not the point.** Biomes exist to make build choices
  *matter* (traction); scrap exists to make the loop turn. Not a survival sim.

## The honest core (emergent physics)

Everything below is computed from the grid. This is the whole reason the cart exists;
get this right and the rest is chrome.

### Part grid → body properties

The vehicle is a small grid (start 8×8 cells, each cell ~one tile). Each occupied
cell holds one part with a **mass** and a **role**. From the set of parts:

```
total_mass     = Σ part.mass
com (cx,cy)    = (Σ part.mass · cell.pos) / total_mass        // centre of mass, in vehicle-local coords
I (inertia)    = Σ part.mass · |cell.pos − com|²              // moment of inertia about the COM
```

`com` and `I` are recomputed in BUILD whenever a part is added/removed (cheap — a few
dozen cells). They are the bridge between "what you built" and "how it moves."

### Drive force & traction

```
engine_force   = Σ engine.power · throttle                   // forward thrust, vehicle-local +x
wheels_down    = count of wheel parts (later: weighted by ground type under each)
traction       = clamp(wheels_down · ground.grip, 0, engine_force)   // ground can't push more than it grips
drive          = min(engine_force, traction)                 // sand starves a powerful engine; road doesn't
accel          = drive / total_mass                          // F = ma, honestly
```

So accel falls out of force ÷ mass with no fudge: heavy build → sluggish; underpowered
engine → never gets up to speed; too few wheels on sand → spins out, goes nowhere.

### Steering = torque about the COM

Steering does **not** rotate the sprite directly. It applies a **torque**, and the
body's angular velocity integrates from it:

```
steer_torque   = steer_input · (wheel_lever)                 // wheels far from COM = more leverage
ang_accel      = steer_torque / I                            // big heavy rig turns lazily; light kart snaps
ang_vel       += ang_accel · dt;   heading += ang_vel · dt
```

The payoff: an **asymmetric build genuinely misbehaves**. Wheels all bunched on one
side, or an engine bolted far off-centre, shifts the COM and the lever arms — the rig
pulls, oversteers, or won't track straight. You can *feel* a bad build, and fix it by
moving parts, not by reading a stat. (This is the part that will be most satisfying
and most fiddly to tune — see Open Questions.)

### Integration

One integrator, fixed timestep, same as the orbit discipline:

```
vel += (heading_dir · accel - drag·vel) · dt
pos += vel · dt
// angular as above; drag/rolling-resistance scales with ground type
```

## Systems (running list — collected as we design, not final)

### 1. The part grid & part vocabulary

Small, legible vocabulary for MVP (DDA has hundreds; resist). Each part: `mass`,
`role`, `strength` (collision threshold), `cost` (scrap to build), a colour, and
role-specific fields:

| Part | Role | Mass | Key field | Notes |
|---|---|---|---|---|
| **frame** | structural | low | — | the skeleton; parts attach to frames; cheap |
| **engine** | drive | high | `power`, `fuel_rate` | thrust ∝ power; drains fuel ∝ throttle |
| **wheel** | traction | med | `grip` | turns engine force into motion; lever for steering |
| **tank** | fuel | med (full) | `capacity` | range; mass drops as it empties (nice emergent touch) |
| **seat** | required | low | — | need ≥1 to drive (where the driver is); defines facing |
| **cargo** | storage | low | `slots` | holds scavenged parts/scrap you can't bolt on yet |
| **plate** | armor | high | — | raises adjacent cells' effective strength; costs speed |

**Connectivity rule** (DDA's, simplified): every part must be edge-adjacent to the
connected mass that contains a seat. No floating parts. A part whose only support
breaks off in a crash **detaches with everything it was holding up**.

Enum-name the parts and address arrays by enum, never raw index — the CLAUDE.md
data-driven-cart rule (modrack's lesson: a mid-list insert silently cross-wires
everything).

### 2. Fuel & range (the clock)

Tanks hold fuel; engines burn it ∝ throttle·power. Range is the real constraint that
turns "drive around" into "plan a route." Run dry and you're a stationary heap until
you scavenge fuel or walk (do we let the driver dismount? — open). Empty tanks still
weigh their dry mass; a full tank's mass bleeding off as you burn is a free, honest
detail (the rig gets nimbler late in a leg).

### 3. Collisions & breakage

Top-down body-vs-world and body-vs-obstacle. On impact, distribute the impact impulse
to the cells along the contact edge; any cell whose share exceeds its `strength`
**breaks off** and scatters as loose debris in the world (orbit already scatters a
rocket into its component parts on crash — reuse that feel). Consequences are
physical, not a health bar:

- lose a **wheel** → traction + steering lever change *mid-drive*, rig pulls
- lose the **engine** → stranded
- lose a **frame** → everything it supported detaches (connectivity rule)
- **plate** absorbs the hit for the cell behind it (its whole job)

This closes the loop with system 6: you crash, you shed parts, you get out and
scavenge replacements.

### 4. The procedural world & biomes

Top-down, generated from a seed (so `--det` / record-replay works for the harness).
Biome sets **ground.grip + drag**, which is the entire reason builds differ:

| Biome | grip | drag | build pressure |
|---|---|---|---|
| **road** | high | low | anything fast wins; few wheels fine |
| **sand** | low | high | needs many wheels / low mass or it bogs |
| **mud** | low | very high | torque + wheels; power alone spins |
| **rock** | med | med | rough — breakage risk from debris |
| **water** | — | — | impassable unless float parts (v2) |

Obstacles: rubble, wrecks (← also scrap sources), boulders. Some smashable if you hit
them hard enough with enough plated mass — which is itself a build choice (a ram).

Generation: simple value-noise biome map + Poisson-ish scatter for scrap/fuel/wrecks.
Chunked around the vehicle so the world is effectively unbounded (flyover already does
player-centred spatial-gridded props over an 8192² world — steal that structure).

### 5. Scavenging & the build loop

The loop that makes it a game, not a physics toy:

```
BUILD a rig  →  DRIVE out  →  world tests it (biomes + obstacles + fuel)  →
something breaks / you find scrap+parts  →  DRIVE to it, salvage into cargo  →
back to BUILD, bolt it on / repair  →  DRIVE further
```

Scrap is the currency (build/repair costs `cost`); whole parts found in wrecks go
into cargo to bolt on in BUILD. Fuel caches refill tanks. This is the "orbit MVP →
parts-bin builder next" cadence: ship the drive physics first, layer scavenging after.

### 6. The journey (the goal / win)

A destination far across the map (a marker / beacon). Fuel scarcity + breakage is the
tension; reaching it is the win. Keeps the cart from being open-ended-and-pointless
without bolting on survival systems. Distance travelled as the score even if you don't
make it. (Could be a series of waypoints — "convoy run." Open.)

## Rendering & screen budget (320×200, provisional)

- **DRIVE:** camera centred on the vehicle, world scrolls under it. Vehicle drawn as
  its grid of part-coloured cells, rotated to `heading` (baked-rotation atlas, or just
  `tritex`/rotated `sspr` per cell — open). Biomes as flat tile colours; dither seams
  between them (galerijflat / neonrain fillp tricks). Minimap inset showing the beacon
  + nearby scrap, flyover-style.
- **BUILD:** the grid fills the screen, paused, on a neutral backdrop. Palette of
  parts down one side (ui.h buttons — do **not** hand-roll). Live readout panel: total
  mass, COM marker (draw it on the grid — a crosshair that visibly shifts as you place
  parts, the orbit apoapsis-marker move), engine power, wheel count, fuel capacity,
  predicted top speed & turn rate. Numbers honest, computed from the same code DRIVE
  uses.
- The COM crosshair in BUILD is the single most important piece of feedback — it makes
  the abstract physics visible *before* you drive. Prioritise it.

## Scope discipline — the MVP ladder

Ship the honest core first on a single biome, exactly like orbit shipped the integrator
before the parts bin. Each rung is a runnable cart.

1. **Drive a fixed rig.** Hard-coded 4-part grid (frame+engine+2 wheels), one biome
   (road), full physics: mass/COM/I derived, force÷mass accel, torque steering,
   integrator. Prove it *feels* like driving. No build, no world gen.
2. **The BUILD flip.** Mode toggle; place/remove parts on the grid; live readouts +
   COM crosshair. Now you build *then* drive — and feel a bad build.
3. **Biomes + traction.** Noise biome map, grip/drag per biome; sand vs road makes
   the same rig behave differently. Camera scroll + minimap.
4. **Collisions & breakage.** Obstacles, impact → parts detach + scatter. Connectivity.
5. **Scavenging loop.** Scrap currency, parts in wrecks → cargo → bolt on; fuel caches.
6. **The journey.** Beacon goal, fuel as the clock, win/score.

Cut for v1 (DDA rabbit holes): crew/NPCs, combat, electrical systems, part HP bars,
turrets, interior tiles, water/amphibious. Each is a clean v2.

Likely ~900–1200 lines at MVP (peers: orbit 703, coaster 970, galerijflat 1440).

## Open questions (next sessions)

- **Steering tuning.** Torque-about-COM is correct and emergent but notoriously twitchy
  to make *fun*. How much rolling resistance / angular damping before it feels like a
  car and not an air-hockey puck? Probably the hardest tuning in the cart. Build the
  harness trace (`watch()` heading/ang_vel/COM) early.
- **Rotated grid rendering.** Per-cell rotated `sspr`, a baked-rotation atlas of the
  whole assembled rig, or draw axis-aligned in vehicle space to a RenderTexture and
  rotate that once? The last is cheapest and cleanest if it's available cart-side.
- **Dismount?** Can the driver leave the vehicle on foot to scavenge, or is salvage
  done by driving over it? On-foot adds a second control mode; driving-over keeps it
  one body. Lean driving-over for MVP.
- **Grid size & cell scale.** 8×8 cells big enough to be interesting, small enough to
  stay legible top-down? What's a cell in world pixels?
- **Fuel-out = dead?** Stranded-with-no-fuel is a real failure state; is walking back
  allowed, or is it game-over/restart (orbit's R)? Tied to the dismount question.
- **Touch story.** BUILD is a natural touch fit (tap parts onto the grid, ui.h handles
  it). DRIVE needs throttle + steer — on-screen pad. Worth `touchControls:true`?

## Name

`sloop` is the working title (Dutch demolition/scrap + English boat — fits a
scrap-built rig). Alternatives if it reads wrong: **`schroot`** (Dutch *scrap metal*),
or English single words in the series' plain-noun register — *Jalopy*, *Overland*,
*Convoy*, *Scrapheap*, *Trek*. Decide before step 1; the cart filename is sticky.

## References in-repo

- `tools/carts/orbit.c` — the closest relative: one honest integrator, prediction =
  simulation, crash scatters into parts. The "physics core first, builder next" cadence
  this cart should copy. (`orbit` MVP shipped 2026-06-09.)
- `tools/carts/coaster.c` — the two-mode flip (M) sharing one physics core; track as a
  player-built structure that then runs under physics — exactly the build↔drive split.
- `tools/carts/flyover.c` — player-centred, spatial-gridded props over a vast world +
  scrolling minimap with named locations. The world-streaming + minimap structure.
- `docs/design/galerijflat.md` — the legendary-series build-log discipline (✓ decisions,
  step-by-step log, handoff section) this doc should grow into once building starts.
- `runtime/ui.h` — BUILD-mode widgets (part palette buttons, sliders); never hand-roll.
- CLAUDE.md → "Data-driven carts: name your indices" — the part enum rule (modrack).
- CLAUDE.md → "Game feel" — breakage/impact wants hit-stop + shake + debris + sound.

## Build log

### Rung 1 — drive a fixed rig on one biome (2026-06-09)

The honest core, runnable. `tools/carts/sloop.c` (~270 lines). What landed:

- ✓ **Body derived from the grid.** `recompute_body()` walks the part grid and
  produces total mass, COM (mass-weighted cell centres), `I` (Σ m·d² about the COM),
  and Σ wheel grip. The hardcoded rig is a symmetric 4-wheel buggy (`GW`×`GH` = 4×3:
  corner wheels, centre-right engine, centre-left seat, frames between). Trace
  confirms M = 17.2, I = 1479, both falling straight out of the layout.
- ✓ **One rigid body.** `sx,sy` track the COM (rotation pivots about it); velocity is
  split each frame into forward + lateral. `accel = thrust/M`, rolling resistance sets
  top speed (~107 px/s), steering is `STEER_RESP·speed_factor / turnEase` integrated
  into `angVel` with `ANG_DAMP` self-centering. Off-centre engines add their own yaw
  torque (`-oy·thrust / I`) — zero for this symmetric rig, live for rung 2's builds.
- ✓ **Tire grip = car feel.** The load-bearing line: lateral velocity is bled away at
  `LAT_GRIP·(wheelGrip/M)` per second, so turning the heading makes the rig track its
  nose instead of skating. Tuned to ~10° slip in a hard corner settling to near-zero —
  planted with a little character.
- ✓ **One biome (road).** Scrolling asphalt grid + deterministic speckle for speed
  legibility; camera lerps to follow. HUD prints speed / mass / heading; a white COM
  crosshair is drawn on the rig (the readout that pays off in BUILD, rung 2).

**Tuning numbers** (all `#define` at the top, easy to revisit): `ENGINE_POWER 2600`,
`ROLL 1.2`, `LAT_GRIP 32`, `STEER_RESP 680`, `ANG_DAMP 5`, `REF_GYRO 130`. Turn rate
settled ~75°/s at speed. Verified headless via `tools/play.js sloop script … --trace`
(gas → steer → release): speed plateaus, heading ramps + self-centers, slip stays low.

Colours: chassis `LIGHT_GREY`, wheels `BLACK`, engine `RED`, seat `BLUE` — wheels were
`DARK_GREY` first and vanished against the asphalt; black reads as tires.

### Rung 1.5 — drifting (the steer.c handbrake) (2026-06-09)

Asked for: the drift feel from the `steer (car drift)` tutorial (`tools/carts/steer.c`),
on this rig. steer.c's model is a single grip value the velocity lerps toward the
heading at — low grip = the tail comes loose, marks appear when lateral slide exceeds
the tires. sloop already decomposes forward/lateral velocity, so drift is the *same
grip term, turned down*:

- ✓ **Handbrake = hold SPACE.** While held, lateral grip drops to `DRIFT_GRIP_MULT`
  (0.13) of normal. The back end stops killing its sideways velocity, so the nose
  rotates while momentum carries you the old way — point one direction, slide another.
  Release and grip snaps back, the slide is killed, and you shoot off along the nose
  (the satisfying drift exit). Honest: one grip term, scaled — no separate drift code path.
- ✓ **Tire marks.** A world-space pool (`MAXSKID` 512, ~150-frame fade) laid at every
  wheel while `|lateral| > SKID_SLIP` (16 px/s) — same trigger as steer.c. Drawn under
  the rig, black → brownish-black as they fade. Four wheels = two parallel curved pairs,
  the classic drift signature.
- ✓ **"DRIFT" HUD flag** + the existing skid screech (already keyed off lateral slide).

Trace (gas → handbrake+steer → release): slip angle hit **~34°** mid-drift (vl −60 vs
vf 89, heading swung to 49°) vs ~10° in a normal corner, then on release vl → −1.5 and
forward speed recovers to ~99 — clean carve-out. Thumbnail is now a mid-drift frame
(four trailing tire marks) rather than the static buggy.

### Rung 1.75 — a schematic course to steer around (2026-06-09)

Asked for: "lanes I can use to steer as if roads, and some objects" — a parcours
that doesn't fight you yet, drawn in the same design style as the grid. Added
`draw_course()`, **purely visual** (nothing collides — that's rung 3's job):

- ✓ **Lanes on a world grid.** 2-cell-wide (`LANE_W` 64) tarmac bands crossing on a
  `LANE_SP` 192 grid — a Manhattan street layout. Bright `LIGHT_GREY` curbs + a dashed
  `YELLOW` centre line do the "this is a lane" reading; the band fill is just a hair
  lighter than the ground so it stays schematic.
- ✓ **Roundabout islands** at ~1/4 of intersections (hash-gated) — a green island +
  ring to steer around.
- ✓ **Cones** scattered one-per-some-blocks in the off-road interiors — objects to
  weave through.
- All deterministic from world position (`hash2`), so the course is stable as you
  drive and the world stays effectively infinite. Drawn under the skid marks (tires
  mark the road) and the vehicle.

This is the **parcours skeleton**: rung 3 makes it solid — lanes become a traction
zone (on-road grip vs off-road drag), cones/roundabouts become collidable obstacles.
For now it's a playground to feel the handling against.

**Next — rung 2 (the BUILD flip):** mode toggle, place/remove parts on the grid,
`recompute_body()` already does the live readouts; add the COM crosshair *in the build
view* (it shifts as you place parts — the orbit apoapsis-marker move) and ui.h part
buttons. The drive core above does not change.
