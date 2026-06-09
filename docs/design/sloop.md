# sloop — build-your-own-vehicle, travel a procedural world (design seed)

**Status: building — rungs 1–1.95 (drive/drift/course/rigs/handling) + 2 (BUILD editor) + 2.5 (scrape) + 2.55 (tipping) + 2.6 (drivetrain FWD/RWD) + 2.7 (gears/transmission) done.** Cart:
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
| **wheel** | traction | med | `grip`, `roll` | free-rolling: grip + support, undriven (see drive) |
| **drive** | powered wheel | med | `grip`, `roll`, `drive` | a wheel that carries the engine's power; placement = FWD/RWD/AWD (built rung 2.6) |
| **caster** | swivel support | med | `roll` | rolls + supports but barely grips → piano-dolly slide |
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

### 1a. Engines — kinds, how they drive, what they need

The single `engine` part above is a placeholder. Engines are sloop's richest axis, and
the honest-core rule still holds: **an engine is just five numbers, and the felt
differences fall out of them — no per-engine special-casing in the drive loop.**

```
engine = { power, mass, delivery(curve), source(what it burns), throttle(how you feed it) }
```

`power` and `mass` already work (thrust sums, mass divides). The two *new* axes are
**delivery** (how power comes on with speed — the "feel") and **source/throttle** (what
gates it). Inspired by Cataclysm: DDA's engine roster, trimmed to a legible set.

| Engine | Power | Mass | Delivery (feel) | Source | Throttle |
|---|---|---|---|---|---|
| **electric** | med | med | flat — full torque from a standstill; instant, snappy | battery (charge) | hold |
| **gas** (spark) | high | med | revvy — bogs low, peaks mid-range, falls near top; must find the power band | tank: gasoline / alcohol | hold |
| **diesel** | high | heavy | grunt — strong low-end pull, runs out of top end; the hauler's engine | tank: diesel / JP-8 | hold |
| **steam** | med | very heavy | spool-up — sluggish until the boiler builds pressure (~3-5 s), then steady | firebox: solid fuel + water | hold |
| **nuclear / exotic** | huge | extreme | flat + immense; no management, just go | none (infinite) | hold |
| **foot crank** | low-med | light | per-stroke pulses; legs = bigger impulse, slower cadence, deep stamina | the driver (stamina) | **rhythmic press** |
| **hand crank** | low | light | per-stroke pulses; arms = smaller impulse, faster cadence, tires quickly | the driver (stamina) | **rhythmic press** |

**Delivery is one cheap function, `delivery(kind, u)`,** where `u = vf / v_ref` is
normalized speed standing in for "revs" (same honest-stand-in trick as rows→aero-profile).
`thrust += engine.power * throttle * delivery(kind, u)`:

- **electric** → ~1.0 flat (tiny taper at top). Instant — the reason it feels snappy.
- **gas** → a bump: ~0.4 at `u=0`, peak ~1.1 around `u≈0.55`, ~0.5 near `u=1`.
- **diesel** → ~1.1 at `u=0` sloping to ~0.6 at `u=1`. Low-end torque, weak top.
- **steam** → `delivery = boiler`, a 0..1 state that charges while running and bleeds when
  idle — the spool-up. (The one engine that needs a per-rig state variable.)
- **nuclear** → 1.0 flat, but `power` is several× the others.

That single curve is the whole "how engines handle" story; everything else (accel, top
speed, turn-in) is the existing mass/drag/I math reacting to the thrust it produces.

**Muscle engines — the novel control mode (and the no-fuel starter vehicle).** The
driver *is* the power source: no fuel part, gated by a **stamina** meter (0..1, drains
per crank stroke, regens while coasting; at 0 you can't crank — you coast). This is the
exact pattern oersoep's stamina-gated dash already uses — reuse it. Throttle is
**`THR_IMPULSE`**: each gas press = one stroke = one thrust impulse (hold = auto-cadence,
but rhythmic tapping rewards timing). Foot vs hand is the legs-vs-arms trade (impulse,
cadence, drain). A soapbox / pedal-cart / railway pump-trolley — the rig you build before
you scavenge a real engine, and steam's cousin in the "when you have no fuel" niche.

**Source = the gating resource (this is system 2, generalized).** Each engine names a
required source part; BUILD warns if it's missing (extends the soft "no engine!" /
"no wheels!" checks → "no fuel tank!" / "no battery!"):

- **battery** (electric) — charge drains with use, **rechargeable**: a solar part (passive)
  and/or regen braking (free + emergent — brake force tops the battery up). Electric's hook.
- **tank** (combustion) — liquid fuel, mass bleeds off as it empties (system 2's nice touch).
- **firebox** (steam) — *two* consumables, solid fuel + water. Bulky. MVP: one `boiler`
  part holding both, for legibility (vs separate firebox + water tank).
- **none** (nuclear) — infinite, but extreme mass + a radiological hazard if damaged (a
  reason not to just bolt the best engine on everything; pairs with rung-4 breakage).

**Hybrid rigs.** DDA's classic move — electric for silent cruising, a big V8/V12 for
hauling or escape. Today engines just **sum**; that's fine until there's a reason to pick
one (noise, fuel scarcity). When there is, add an **active-source toggle** (a key) that
selects which engine kind drives. Recommend: keep summing for MVP, add selection alongside
fuel (rung 6). Same-throttle engines sum cleanly; mixing `hold` + `impulse` is the only
case that genuinely needs the toggle.

**Two more axes, parked with a home:**
- **Noise / stealth** — electric is silent, combustion is loud. Only matters if the world
  has things that *hear* you. No threats are planned yet, so park it — but it's electric's
  whole point, so don't lose it.
- **Overheat** — sustained combustion/nuclear load builds heat → power derate or damage.
  This reuses the `heat` value rung 2.5 already added and lands with rung-4 breakage.
  Muscle "overheats" the driver (= stamina); electric stays cool.

**Rung placement** (2.6 was taken by the drivetrain/drive-wheel work — see the build log):
- **Rung 2.7 — engine powerband + transmission** (electric / gas / diesel / steam /
  nuclear curves **+ gears**, see §1b). Pure drive-core feel, no fuel yet (engines just
  always have power); the powerband + gears are the deliverable, and gears decouple
  reverse from the brake. The natural sibling of the rung-1.95 handling levers.
- **Rung 2.8 — muscle engines** (foot/hand crank): the `THR_IMPULSE` interface + a stamina
  meter. Self-contained; could even ship before 2.7 as the "starter rig."
- **Rung 6 — sources as the clock**: battery / tank / firebox parts, fuel burn, range,
  recharge, the missing-source warning, the hybrid toggle. Where the resource layer belongs.
- **Rung 4 — overheat derate** (reuses rung-2.5 heat). **Parked:** noise/stealth (needs
  threats), multi-cell engine blocks (V12 = 2 cells — placement interest vs the 1-part =
  1-cell grid; decide later).

> Demo presets added with rung 2.6: **6 FWD** (drive at front → planted/understeer),
> **7 RWD** (drive at rear → tail-happy/oversteer), **8 4WD** (all corners driven → neutral,
> full traction). Templates `1`–`8` now. Verified: FWD `driveoff +9.8`, RWD `−9.8`, 4WD `−0.6`.
> (NB: which wheels *steer* is a separate, still-unbuilt lever — see per-axle, rungs 3–4.)

> **Rolling friction (2026-06-09 fix).** The rig felt floaty — it never quite stopped when
> coasting. Cause: deceleration was *only* velocity-proportional drag (`drag·v`), which
> asymptotes toward 0 but never reaches it (that's the aero model). Added `ROLL_FRIC`, a
> **constant** decel (rolling/bearing friction, mass-independent) applied to `vf`/`vl` and
> snapping tiny velocities to exactly 0 — what actually stops a real vehicle. Engine thrust
> dwarfs it, so accel is unaffected and top speed drops only ~12% (buggy ~120→106); it bites
> at low speed and brings the rig to a clean, dead stop in ~1.8 s from cruising.

> **Stronger, grip-limited braking (2026-06-09).** Braking felt weak (`BRAKE` 240 ≈ engine
> accel). Real brakes >> acceleration, so bumped to 560 — but **capped per-rig by tyre grip**
> (`min(BRAKE, GRIP_TO_FORCE·wheelGrip·GROUND_GRIP / M)`), so more/better wheels stop harder
> and an under-wheeled/heavy rig can't haul up as fast (the honest "how hard you can brake
> depends on the build", since a keyboard pedal is binary). A slam-brake now stops a buggy
> dead in ~0.16 s. Plus a **reverse dwell**: the brake key flips to reverse only after the
> rig has been stopped (`REV_DWELL` frames at ~0) and then *latches* — so a hard stop LANDS
> firmly before backing up, instead of mushily creeping into reverse at `vf ≤ 5`.

### 1b. Transmission & gears — the layer between engine and wheels

The missing piece between §1a's engine and the wheels. A gearbox trades **torque ↔ speed**
via a ratio, and — crucially for *feel* — gives the engine an **RPM** that rises within a
gear and drops on a shift (the satisfying part, both to drive and to hear). It also fixes
the unrealistic "↓ = brake **and** reverse at once": **reverse is a gear**, not a function
of the brake pedal.

**The honest model** (extends §1a's `delivery(kind,u)` — `u` becomes per-gear RPM, not
absolute speed):

```
rpm        = clamp(|vf| · ratio[gear] / V_REF, 0, 1.1)   // wheel speed → engine revs
torqueMul  = band(rpm)                                    // the powerband: low at idle,
                                                          //   peak mid, falls past redline
thrust     = engine_power · throttle · torqueMul · ratio[gear]   // low gear = high ratio
                                                          //   = more thrust, revs climb fast
```
Low gear → big `ratio` → lots of thrust but `rpm` hits redline early → **low top speed, fast
accel**. High gear → small `ratio` → less thrust, **high top speed**. So *each gear has its
own top speed*, and you climb through them — top speed = thrust/drag, now gear-gated.

**Transmission types** (the player's list — tied to the engine kind, the natural fit):

| Type | Who | Behaviour |
|---|---|---|
| **single (direct drive)** | **electric** (1 gear, as you said), karts, the muscle/crank rigs | one ratio, no shifting; `rpm` ∝ speed directly. Electric's flat torque means it just *goes* — no powerband to manage. Simplest + the reason EVs feel effortless |
| **automatic** | most combustion road rigs | shifts for you: up when `rpm > ~0.85`, down when `rpm < ~0.3`. Hold the gas; the box keeps you in band. Sound auto-drops on each shift |
| **manual ×N** | sportier/heavier rigs | you shift up/down (two keys). Keep it in the band yourself; **a wrong gear bites** (below) — the skill/risk option |

**Wrong gear bites (manual)** — the "putting it in the wrong gear causes stuff" you want:
- **Lugging** (too-high gear, low speed): `rpm` below the band → `torqueMul` tiny → bogs, barely pulls. Drop deep enough under load → **stall** (engine cuts, you coast; restart cost — or, friendlier, just bogs to a crawl. Open: how punishing?).
- **Over-rev** (too-low gear, high speed): `rpm` past redline → `torqueMul → 0` (rev limiter), harsh sound, no more accel; held there → engine **damage** (ties to rung-4 engine HP).

**Reverse = a gear (R).** Selecting reverse (shift below 1st / a dedicated press) lets the
gas drive you backward; the **foot brake (`X`/`↓`) goes back to being pure deceleration**,
always. This removes the brake/reverse conflation cleanly. (Automatic may still auto-pick R
from a standstill; manual makes it explicit.)

**Sound — the satisfying bit.** Engine pitch tracks `rpm`: it *climbs* as you accelerate in
a gear, then **drops on each upshift** (the classic "waaah-waaah" gear-change), climbs again.
Distinct per engine kind (electric whine that just rises and holds; combustion growl that
steps). Maps `rpm → midi` for the engine voice — far more alive than today's
`hit(28 + spd·0.12, …)` flat ramp.

**Controls:** manual needs **two shift keys** (up/down) on top of gas/brake/steer/handbrake
— e.g. a shoulder pair, or `Q`/`E`. Automatic + single need none. BUILD shows the gear
count + a live tach (RPM bar) so you can feel the band before driving.

**Open forks (decide before building):**
- **Where does the transmission live?** (a) a **property of the engine kind** (electric =
  single, combustion = auto-or-manual) — simplest, fewest parts, and matches "EVs have 1
  gear"; (b) an explicit **gearbox part** you bolt on (pick type + gear count) — most
  build-ethos but the most UI; (c) a **global rig setting** cycled in BUILD. Lean (a) for
  MVP, revisit (b) when the part vocabulary justifies it.
- **Stall punishment:** full stall + restart (realistic, punishing) vs bog-to-a-crawl
  (forgiving). Probably forgiving for MVP.

**Rung placement:** gears are the same subsystem as §1a's powerband (`delivery`/`band`) —
build them **together at rung 2.7** (rename it "engine powerband + transmission"), since a
powerband only matters if you can shift to stay in it, and gears only matter if there's a
band to chase. The reverse-as-gear decoupling lands here too. Muscle (2.8) and fuel/sources
(rung 6) are unaffected.

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
2. **The BUILD flip.** ✅ done (2026-06-09). Mode toggle; place/remove parts on the grid;
   live readouts + COM crosshair. Now you build *then* drive — and feel a bad build.
3. **Biomes + traction.** Noise biome map, grip/drag per biome; sand vs road makes
   the same rig behave differently. Camera scroll + minimap. *Levers here:* wheel-area /
   ground-pressure traction, per-axle grip, stability/tippiness (see lever catalogue).
4. **Collisions & breakage.** Obstacles, impact → parts detach + scatter. Connectivity.
   *Levers here:* plating absorbs shock, damaged-engine power ∝ HP.
5. **Scavenging loop.** Scrap currency, parts in wrecks → cargo → bolt on; fuel caches.
6. **The journey.** Beacon goal, fuel as the clock, win/score.

Cut for v1 (DDA rabbit holes): crew/NPCs, combat, electrical systems, part HP bars,
turrets, interior tiles, water/amphibious. Each is a clean v2.

Likely ~900–1200 lines at MVP (peers: orbit 703, coaster 970, galerijflat 1440).

## DDA reference — how parts decide performance (2026-06-09 research)

Captured from the Cataclysm: DDA source/wiki (sources at the bottom). DDA hangs *all*
vehicle performance on **engine power scaled by two master coefficients**:

```
max_velocity  = engine_power × 80                               // power alone caps the ceiling
safe_velocity = engine_power × m2c × K_dynamics × K_mass × 80
acceleration  = safe_velocity × K_mass / 20
```
- **K_mass** — power-to-weight (1 = mass doesn't slow you, 0 = won't move). Heavier →
  worse accel *and* lower safe speed.
- **K_dynamics** — combined **aero drag + wheel rolling resistance** (1 = frictionless).
  Where *shape* and *wheels* cost top speed.
- **m2c** — an engine's safe-power / max-power band.

Part → stat map:

| Part | Decides |
|---|---|
| **Frame** | mass + structural durability. A frame bridging two parts is *unbreakable* (spine). |
| **Engine** | power → top speed + accel; fuel burn. Multiple engines **stack**. Damaged engine → power ∝ remaining HP. |
| **Wheels** | total **area vs mass** decides if it can move (optimal ≈ 1.58·√mass; too little → bogs, esp. off-road); size/width/style set **rolling resistance** (more/wider/off-road = slower on road, better off-road); **steerable** wheels (placement) set turning. |
| **Tank / battery** | range. |
| **Plating** | protects the part it's installed *over* in a collision (shock still radiates to neighbours ÷ distance). |
| **Seat + controls** | required (same tile) to drive. |

Note DDA is **grid-based and never rotates** — it has *no* moment of inertia and *no*
off-centre torque. sloop already goes beyond it on those (our `I` and `eng_torque`).

### The lever catalogue — what makes handling depend on the build, and where it lands

✅ = in the cart now. Each is purely emergent (no per-rig tuning):

| Lever | What it does | Rung | Status |
|---|---|---|---|
| Mass → acceleration | `accel = thrust/M`; heavy rig accelerates slowly | 1 | ✅ |
| Engine power (stacks) | more/bigger engines → more thrust | 1 | ✅ |
| Moment of inertia → turn rate | long/heavy rig turns lazily (`STEER/I`) | 1 | ✅ |
| Off-centre engine → yaw | engine off the centre-line pulls under power | 1 | ✅ |
| **Aero drag from frontal profile** | top speed ∝ 1/(cells across travel) — narrow = fast | 1.95 | ✅ |
| **Wheels as a trade-off** | each wheel adds rolling resistance (grip↑, top speed↓) | 1.95 | ✅ |
| **Weight balance → under/oversteer** | nose-heavy pushes wide, tail-heavy turns in (COM vs wheelbase) | 1.95 | ✅ |
| Top speed mass-INDEPENDENT | drag is a force; mass sets accel, not top speed (DDA's insight) | 1.95 | ✅ |
| **Wheel type: caster vs fixed** | casters (piano dolly/cart) give support but ~no lateral grip → slides any way, no nose-tracking; fixed wheels track forward | 2 (part vocab) | ✅ |
| **Unsupported cells scrape** | a cell cantilevered past the wheel span drags on the floor: extra drag + lateral anchor + off-centre yaw, with sparks/heat/grind. Wheels become *spatial*, not a scalar; wheel-spam costs mass+drag, too few wheels drags | 2.5 | ✅ |
| **Engine delivery curve** | how power comes on with speed: electric flat (snappy), gas revvy (mid-range band), diesel low-end grunt, steam spool-up. One `delivery(kind,u)` curve; see §1a | 2.7 | ⬜ |
| **Transmission / gears** | a gear ratio trades torque ↔ top speed; RPM rises in a gear, drops on a shift. single (electric/EV = 1 gear) / automatic / manual×5 (wrong gear lugs or over-revs). Reverse is a gear, not a brake function. Engine pitch tracks RPM (the satisfying sound). See §1b | 2.7 | ✅ |
| **Muscle throttle (stamina + rhythm)** | foot/hand crank: each press = one stroke (`THR_IMPULSE`), gated by a stamina meter; the no-fuel starter rig. See §1a | 2.8 | ⬜ |
| **Wheel area / ground pressure** | traction = f(wheel area ÷ mass) per terrain; heavy-on-few-wheels bogs in sand | 3 (biomes) | ⬜ |
| **Per-axle grip** | front-steer/rear-drive split → rear-only handbrake, true oversteer drift | 3–4 | ⬜ |
| **Grip limit → spin-out ("uit de bocht vliegen")** | each tyre holds only so much sideways force (≈ grip·weight, the friction circle); corner too fast (demand `v²/r` > limit) and the tyres LET GO → plough wide or the rear breaks away and you spin. Sibling of 2.55 tipping (that's grip loss from load leaving the hull; this is from exceeding the friction limit at speed). Today grip is a soft per-frame bleed that never catastrophically lets go | 3–4 | ⬜ |
| **Aquaplaning / terrain grip** | `GROUND_GRIP` drops toward ~0 on water/ice/wet → the rig floats, steering does nothing; cross a puddle mid-corner → instant slide. Rides on the existing `GROUND_GRIP` hook (=1.0 road today), set per-biome | 3 (biomes) | ⬜ |
| **Dynamic stability / tipping** | cornering load shifts the COM toward the turn's outside; leaving the support polygon (hull of the wheels) tips the rig → transient scrape + lateral grip collapse. A 3-wheeler tips toward its gap but not the other way (asymmetric); single-track (bike) exempt. The 2-D stand-in for roll | 2.55 | ✅ |
| **Drivetrain location (FWD/RWD)** | power lays down through the *drive wheels*; drive point ahead of the COM (in travel) pulls → stable/understeer, behind pushes → loose/spin. Reversing flips it → a rear-wheel bike drives better backwards. Explicit `drive` part | 2.6 | ✅ |
| **Fuel burn ∝ power; damaged engine power ∝ HP** | range as the clock; a half-wrecked engine gives half thrust | 3–4 | ⬜ |
| **Plating absorbs collision shock for its cell** | armour trade (mass↑, speed↓, survives hits) | 4 (breakage) | ⬜ |

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
- **Two brakes (already in: foot brake `X`/`↓` + handbrake `SPACE`).** The foot brake
  decelerates the whole rig evenly (grip-limited, strong, lands a firm stop → reverse on
  hold). The handbrake breaks the tyres loose for a drift. *Refinement (with per-axle
  grip, rung 3–4):* a real handbrake should lock the **rear** wheels specifically → the
  tail steps out / spins, vs today's global `DRIFT_GRIP_MULT`. Same per-axle model as the
  true FWD/RWD oversteer and the rear-only-grip spin-out.

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
- DDA vehicle model (2026-06-09 research) — part→stat + K_mass/K_dynamics:
  [Vehicle parts (srgnis wiki)](https://srgnis.github.io/cdda-wiki/cdda_wiki/Vehicle_parts),
  [Vehicle construction (danmakudan)](https://cddawiki.danmakudan.com/wiki/index.php/Vehicle_construction),
  [New Contributor Guide: Vehicle Parts](https://github.com/CleverRaven/Cataclysm-DDA/wiki/New-Contributor-Guide-Vehicle-Parts),
  [speed rework #25652](https://github.com/CleverRaven/Cataclysm-DDA/issues/25652),
  [K mass/dynamics #6653](https://github.com/CleverRaven/Cataclysm-DDA/issues/6653),
  [offroad penalty #12549](https://github.com/CleverRaven/Cataclysm-DDA/issues/12549).

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

### Rung 1.9 — preset rig toggle (2026-06-09)

Before the BUILD editor, prove the derived physics *reads differently per build* —
orbit's playbook (its 1/2/3 rockets "feel each way to fail" before the parts-bin
builder). `1`–`5` swap preset layouts in `DESIGNS[]`; same drive core, **zero
per-rig tuning** — every difference is mass / COM / I / grip falling out of where the
parts sit. Grid footprint widened to 6×3 (rigs pad unused cells with `P_NONE`); the
nose marker now sits at the rig's real front edge (`frontX`), and the HUD shows the
rig name + mass + engine/wheel counts.

Measured (gas to terminal, then a hard right — all emergent, headless `--trace`):

| Rig | Mass | I | Top speed | Turn | Feel |
|---|---|---|---|---|---|
| BUGGY | 17.2 | 1479 | ~102 | 75°/s | balanced baseline |
| HAULER | 23.2 | 3923 | ~78 | 54°/s | heavy + long → crawls, turns lazily |
| SPRINTER | 20.2 | 1541 | ~169 | 78°/s | twin engine → rockets, snappy |
| JALOPY | 13.7 | 984 | ~124 | 83°/s | light, 3 wheels → loose; **pulls** |
| MOTORBIKE | 8.2 | 383 | ~264 | 91°/s | narrow inline 2-wheeler → fastest, dartiest, slides |

(`5` = MOTORBIKE, added per request: a single-row `WHEEL·ENGINE·SEAT·WHEEL` rig. At
this rung drag was still a mass-coupled rolling term, so the feather-light bike topped
out absurdly fast (~264, climbing) — rung 1.95 fixed that by making drag a force.
Top-down rigid body, so it doesn't lean/tip — an accepted arcade abstraction.)

JALOPY's off-centre engine fires the `eng_torque` term: heading drifts to ~2° under
pure throttle *before any steering input* — the honest-core "asymmetric build pulls"
claim, visible in the trace. (Small for a grid this size, as expected; the looseness
from 3 wheels + low mass is the bigger felt difference.) This is the proof rung 2
needs: the build genuinely drives the feel.

### Rung 1.95 — handling depth: three DDA levers (2026-06-09)

After researching DDA's vehicle model (see "DDA reference" above), added the three
levers that need no new systems and reuse data we already derive. All emergent, no
per-rig tuning:

1. **Drag is now a FORCE, not a mass-coupled rate** — DDA's key insight. Top speed =
   `thrust / drag` is **mass-independent**; mass governs *acceleration* only. This fixed
   the runaway feather-light bike (was ~264 and climbing → now a sane ~211) and made the
   HAULER reach roughly the BUGGY's top speed but *accelerate* like a truck — exactly right.
2. **Aero drag from frontal profile** — `DRAG_AERO × frontalCells`, where `frontalCells`
   is the rig's span across the direction of travel. The narrow MOTORBIKE (`front=1`) is
   fast because it's *slippery*, not just light; the others (`front=3`) pay full aero.
3. **Wheels as a trade-off** — `DRAG_WHEEL × nWheels`. Each wheel adds rolling resistance,
   so bolting on wheels for grip now *costs top speed* — a real decision, not always-yes.
4. **Weight balance → under/oversteer** — `balance` = COM position along the wheelbase
   (+1 nose-heavy … −1 tail-heavy), scaling steering authority (`1 − BALANCE_K·balance`).
   Nose-heavy pushes wide (understeer), tail-heavy turns in eagerly (oversteer). JALOPY's
   front-mounted engine reads `balance ≈ 0.18` → a mild understeer *on top of* its yaw-pull.

Re-measured all five (gas to terminal, then a hard right; headless `--trace`):

| Rig | Mass | front | balance | Top speed | Accel feel | Turn |
|---|---|---|---|---|---|---|
| BUGGY | 17.2 | 3 | +0.05 | ~109 | brisk | ~72°/s |
| HAULER | 23.2 | 3 | −0.03 | ~104 | sluggish (heavy) | lazy |
| SPRINTER | 20.2 | 3 | −0.01 | ~206 | strong (2 eng) | snappy |
| JALOPY | 13.7 | 3 | +0.18 | ~119 | quick | understeers + pulls |
| MOTORBIKE | 8.2 | 1 | −0.11 | ~211 | rocket | dartiest |

For the (fairly symmetric) presets `balance` is mild; its real payoff arrives with the
rung-2 BUILD editor, where mounting a heavy engine at the very nose or tail will swing
the car's character. Remaining levers (caster wheels, ground-pressure, per-axle grip,
tippiness, fuel/damage, plating) are catalogued above against their rungs.

**Piano-on-casters observation (parked for rung 2):** a pushed piano has a *vastly*
different feel from a car — caster wheels give support but **no preferred rolling
direction**, so it slides any way and pivots freely (no nose-tracking), and a tall
high-COM rig is *tippy*. Both are real levers we don't have yet: a **caster/omni wheel
type** (≈ zero lateral grip — the opposite of our tire-grip line) belongs in rung 2's
part vocabulary, and **stability/tippiness** (a 2-D stand-in for roll: spin-out above a
lateral-g threshold set by track width vs a notional COM height) fits rung 3–4. Logged
in the lever catalogue.

**Next — rung 2 (the BUILD flip):** mode toggle, place/remove parts on the grid,
`recompute_body()` already does the live readouts; add the COM crosshair *in the build
view* (it shifts as you place parts — the orbit apoapsis-marker move) and ui.h part
buttons. Fold in the caster wheel type while expanding the part vocabulary. The drive
core above does not change.

### Rung 2 — the BUILD editor (2026-06-09)

The titular feature: a **BUILD ↔ DRIVE flip** (TAB or `B`), the cart's spine like
coaster's M. The drive core was untouched — rung 2 is pure UI over the engine
`recompute_body()` already feeds.

- ✓ **BUILD mode** (paused, `cls` to a blue backdrop). A `ui.h` part palette down the
  left — frame / engine / wheel / caster / seat / erase, each with a colour chip and a
  white selection ring; click a button (or hotkeys F/E/W/C/S/X) to pick. The grid
  editor is centred; **click or drag a cell** to place the selected part (erase = the
  eraser). `1`–`5` load the presets as **editable templates**, `R` clears to empty.
- ✓ **Live readouts — the build telling the truth before you drive.** A **COM crosshair
  drawn on the grid that shifts as you place parts** (the orbit apoapsis-marker move —
  the most satisfying piece), plus a READOUT panel: mass, estimated top speed, turn
  rate, engine/wheel counts, and an understeer/neutral/oversteer label from `balance`.
  `est_top_speed()`/`est_turn_rate()` use the *same formulas the drive core uses*, so
  the numbers don't lie. Soft "no engine!" / "no wheels!" warnings; no hard validation
  (floating parts allowed — connectivity is a later add).
- ✓ **Round-trip verified** (headless script: B → place an engine → B → gas): the custom
  rig drives with its new mass (17.2 → 21.2 for +1 engine) and the added front engine
  read `balance 0.36` → understeer, exactly as built.
- ✓ **Caster wheel** (the piano-on-casters lever) shipped here: `PartKind` gained a
  `roll` field — `grip` is lateral nose-tracking, `roll` is rolling support. A fixed
  WHEEL has both; a CASTER has `roll=1` but `grip=0.12`, so it rolls/drives but barely
  tracks — build an all-caster rig and it slides any way and pivots, the piano-dolly
  feel. Traction now uses `wheelRoll`; lateral grip still uses `wheelGrip`. Drawn with a
  swivel-pivot dot.

Note: the BUILD↔DRIVE toggle is **TAB or B** — the play.js harness can't inject TAB
(its key-name table is SPACE/arrows only), so `B` doubles as the testable/alternate key.

**Deferred (clean later adds):** connectivity enforcement, save/load of designs, scrap
costs, touch-grid placement (palette already works via ui.h touch; grid placement is
mouse-only for now). Remaining levers (ground-pressure, per-axle grip, tippiness,
fuel/damage) per the catalogue → rungs 3–4.

**Next — rung 3 (biomes + traction):** noise biome map, grip/drag per terrain, the
wheel-area/ground-pressure lever (heavy-on-few-wheels bogs in sand), camera-follow
minimap. The BUILD editor + drive core are the stable base it builds on.

### Rung 2.5 — unsupported cells scrape the floor (2026-06-09)

Player-reported gap: you could drive a bike on one wheel, or spam wheels everywhere,
and it barely changed how the rig handled. The model treated wheels as a *scalar* (a
summed grip number) with no notion of whether they actually hold the chassis up. This
rung gives wheels a **spatial** role: a cell hanging out past the wheels drags on the
ground. Resolves the bike observation directly and gives wheel-spam a real downside.

- ✓ **Support span, not "a wheel under every cell."** `recompute_body()` computes the
  bounding box of all support points (wheels + casters), in cell indices. An occupied,
  non-support cell **scrapes** only if it falls *outside* that box — i.e. cantilevered
  past the outermost wheels. Cells *between* the wheels are carried by the wheelbase, so
  a big frame floor needs wheels only at its extremes (no wheels-everywhere). The bbox is
  a cheap stand-in for the support hull and lands all 5 presets at zero drag while a
  de-wheeled bike or a 1-wheel rig drags. `dragging[GH][GW]` + `nDrag` are the outputs.
- ✓ **Scrape physics — the same force model as the wheels, on the unsupported cells.**
  Extra forward drag (`SCRAPE_DRAG`·nDrag, folded into the drag force → lower top speed),
  a lateral anchor, and — if the scrape is off the centre-line — a **yaw** that pulls the
  rig to that side (`scrape_torque`, mirrors the off-centre-engine torque). Kinetic: only
  bites above `SCRAPE_MINSPD`.
- ✓ **Feel: sparks + heat + grind.** Each scraping cell throws sparks (white-hot core
  fading to orange/yellow, flung opposite travel), a `heat` value (0..1) rises under load
  and cools off, the scraping cells **glow** (red→orange→yellow→white by heat), and a low
  `INSTR_NOISE` grind plays. HUD shows `SCRAPE x{n}` + a heat bar.
- ✓ **Shown in BUILD too** — dragging cells get an orange ring + a "N cells scrape!"
  warning, and `est_top_speed()` now folds in the scrape drag *and* the traction cap, so
  the readout tells the truth (verified: a one-wheel bike reads TOPSPD 73 and drives 73,
  vs 213 intact).
- ✓ **Verified** (headless script: load motorbike → BUILD → erase a wheel → DRIVE → gas):
  `ndrag` 0→2, mass 8.2→6.7, top speed 213→73, `heat` climbs to 1.0; the intact rig stays
  `ndrag:0` (no false positives). Lever catalogue updated.

**Terrain detail (deferred to rung 3):** sparks + heat are **metal-on-tarmac**. Offroad
(sand / mud / grass) a dragging cell wouldn't spark — it would plough a **dirt furrow**,
and drag would be *worse* (it digs in rather than skating). When biomes land, gate
`throw_spark()`/heat on a hard-surface terrain flag and swap in a dust/furrow effect with
a higher off-road `SCRAPE_DRAG`. Noted at `throw_spark()` in the cart. Today: all tarmac.

### Rung 2.55 — dynamic tipping under cornering load (2026-06-09)

Player-reported follow-on to 2.5: take *one* wheel off a 4-wheel car and it still drove
fine. 2.5's scrape is **static** (a cell permanently off the ground), so a 3-wheeler —
whose three corners still span the bounding box — never scraped. But it *should* be
unstable: a real 3-wheeler tips onto its missing corner when you corner toward it. This
rung adds the **dynamic** half: the build tips under cornering load.

- ✓ **Support polygon.** `recompute_body()` builds the convex hull (monotone chain) of the
  wheel/caster positions, in COM-local px. `hull_margin()` gives signed distance to the
  hull (>0 inside); `lateral_reach(±1)` raycasts how far the COM can shift each way before
  leaving it (`stabL`/`stabR`). **<3 non-collinear wheels = single-track** (a bike) → exempt
  (we don't model lean; it's assumed to balance), so the motorbike preset is untouched.
- ✓ **Tip = the load leaving the hull.** In a turn, lateral-g (`vf · yaw-rate`) shifts the
  COM toward the **outside** by `aLat · STAB_H` (`STAB_H` = the COM-height stand-in — our
  2-D proxy for roll, since there's no z). If the shift exceeds the hull reach on the loaded
  side → `tip_amt` (0..1, how far past). Effect: **lateral grip collapses** (`× (1 −
  STAB_GRIP_LOSS · tip_amt)` → the tires let go, rig pushes wide / breaks loose) + a
  **transient scrape** at the digging corner (reuses 2.5's sparks + heat). HUD shows `TIP!`.
- ✓ **Asymmetric for free.** A 3-wheeler's triangle has a short edge toward the gap, so it
  tips turning *that* way and corners clean the other — the geometry alone produces it; no
  per-rig tuning. The front/rear character (understeer vs oversteer) still rides on the
  existing `balance` term, which already shifts when you remove a front vs rear wheel.
- ✓ **Visible in BUILD.** The support polygon is **drawn over the editor grid** (green =
  stable, orange = tippy), the COM crosshair sits inside it, and a `stable / tippy turns /
  single-track` readout. Same "see the physics before you drive" payoff as the COM marker.
- ✓ **Verified** (headless: buggy → BUILD → erase a front corner → DRIVE → corner both
  ways): intact `stabL 7.0 / stabR 7.0`; 3-wheel `stabL 6.5 / stabR 1.0` (one side
  collapses), `ndrag:0` (no static scrape); hard turn toward the weak side → `tip 0.25` +
  slide + heat, toward the strong side → `tip 0.00`, clean. The asymmetry confirmed.

**Why it's separate from per-axle grip (still rung 3–4):** tipping here collapses the
*whole* rig's lateral grip (→ understeer/push). True per-axle behaviour — front-only vs
rear-only grip loss giving distinct understeer vs power-on oversteer — needs grip applied
at each wheel, which is the per-axle lever. 2.55 is the body-level stand-in; it reads as
"the rig lets go," not yet "the back steps out."

### Rung 2.6 — drivetrain: drive wheels + push/pull directional stability (2026-06-09)

Two player observations, one principle: (a) a rear-wheel-only bike should drive *better
backwards*, and (b) front-engine vs rear-engine drive should handle very differently. Both
are **where a force acts, fore/aft of the COM, in the travel direction** — the wheelbarrow
rule: traction *ahead* of the COM in travel = the rig is **pulled** (tracks straight,
stable); *behind* = **pushed** (the heavy end wants to swing round → oversteer / spin).

- ✓ **Explicit drive-wheel part** (`P_DRIVE`, palette `drive`, hotkey `D`). A fixed wheel
  that also carries the engine's power — a powered axle. Drawn as a wheel with an **orange
  hub** (vs the caster's grey hub). Placement = the drivetrain: front = FWD, rear = RWD,
  both = AWD. **No drive wheels placed → the engine powers all wheels (AWD)**, so every
  preset is unchanged. `PartKind` gained a `drive` flag (chosen over engine-adjacency or
  nearest-wheel: explicit + unambiguous, the player's call).
- ✓ **Power lays down through the drive wheels.** Traction now caps thrust to
  `driveRoll · …` (only the *powered* wheels' rolling support), so a one-drive-wheel rig
  can't deploy all its engine — fewer driven wheels = less power to the ground.
- ✓ **Directional-stability term.** `driveX` = the drive point (mean x of the drive
  wheels). `lead = (driveX − comX) · sign(vf)` → in the travel frame, >0 pulls, <0 pushes.
  A yaw damping `angVel −= STAB_YAW_K · lead · angVel · speedFactor · dt`: pulling adds
  self-centering (stable, understeer), pushing removes it (loose, spin-prone). Clamped
  (`DRIVE_OFF_MAX`) so worst-case push can't exceed the baseline `ANG_DAMP` and spin
  instantly. Symmetric presets have `driveX ≈ comX` → term ≈ 0 → **today's feel untouched.**
- ✓ **Surfaced.** HUD + BUILD show `FWD pull / RWD push / AWD`; the drive-wheel hubs are
  orange in both the rig and the editor grid + palette chip.
- ✓ **Verified** (headless): rear-wheel bike `driveoff −6.7` — a steer pulse settles
  `121→21` over ~40 frames driving **forward** (push, twitchy) but `−49→0` in ~25 frames in
  **reverse** (pull, tracks true): drives better backwards, as predicted. An RWD 4-wheeler
  reads `RWD push`; FWD reads `FWD pull`.

**Caveat (the honest boundary, same as 2.55):** this is the *directional* feel (pull-stable
/ push-loose / reverse-bike). The full per-tire split — front tyres washing out under power
as true understeer vs the rears lighting up as power-on oversteer — is still the per-axle
grip lever (rung 3–4); 2.6 applies the stability at the body level off the drive point.

### Rung 2.7 — transmission & gears (2026-06-09)

The §1b spec, built: a gearbox between engine and wheels, with RPM, three transmission
types, gear-driven sound, and **reverse decoupled from the brake**.

- ✓ **Powerband + ratio.** `rpm = |vf|·ratio/V_REF`; `thrust = power·throttle·powerband(rpm)·ratio`.
  `powerband()` is a broad plateau (so the tall top gear still pulls) with a hard rev-limiter
  cut past redline. Ratios `{2.6, 2.0, 1.53, 1.22, 1.0}` are spaced so each gear's redline
  speed steps **up** — every gear out-tops the last (1st ≈42 → 5th drag-limited ≈100), fixing
  a first cut where 5th topped *below* 4th.
- ✓ **Three transmissions** (cycle with `G`): **1-GEAR** (direct drive — the electric/EV feel
  you flagged: one flat `SINGLE_RATIO`, no band to manage, ~101 top), **AUTO** (auto-shifts in
  band — `rpm > 0.82` up, `< 0.34` down; drives ≈ the old flat model, tops ~100), **MANUAL**
  (`Q`/`E` shift; **wrong gear bites** — stuck in 1st over-revs and caps at ~60, a too-tall
  gear lugs).
- ✓ **Reverse is a gear (0).** `Q` at a standstill drops into reverse, `E` climbs out; gas
  drives in the gear's direction. The foot brake (`X`) is now **pure bidirectional
  deceleration** — the unrealistic "↓ = brake *and* reverse at once" is gone.
- ✓ **Sound tracks RPM.** Engine note `hit(30 + rpm·30, INSTR_SAW)` climbs within a gear and
  **drops on each upshift** (the satisfying gear-change), plus a `INSTR_NOISE` clunk on every
  shift. HUD gains a gear readout (`R`/`1`–`5`), a tach (RPM bar, reddens near redline), and
  the transmission label.
- ✓ **Verified headless:** AUTO climbs 1→3→5, tops ~100; SINGLE stays in gear 1, ~101; MANUAL
  stuck-in-1st caps ~60 (over-rev), upshifting reaches top; reverse hits −67. Preserves the
  drive-feel — traction cap, drivetrain push/pull, tipping, braking all intact.

**Still open (the §1a half of "engine powerband + transmission"):** per-engine-kind *curves*
(electric flat / gas revvy / diesel grunt / steam spool) and the engine roster — 2.7 gives
the gearbox + a single generic powerband; distinct engine kinds remain (§1a, a follow-on).
**Stall** went the forgiving way (lug/bog, no full stall+restart) per the spec's open fork.

**Speed + gear-dwell pass (same day, after a playtest).** Two coupled complaints: the box
ripped 1→5 in ~1 s, and the rig "didn't look fast." Root cause: the whole speed scale was
small *and* acceleration was huge, so it hit a low top instantly — gears flashed by. Fix:
- **Lower acceleration + lower drag together.** `ENGINE_POWER` 2600→850 and drag ~⅓ — accel
  drops (gentle launch) but the LOW drag keeps the rig pulling to a high top (~165). Climb to
  top now takes ~6 s, so the gears are a real progression: off in 1st (~0.7 s), build 2nd/3rd,
  cruise 4th, top out in 5th. `V_REF` 110→200 stretches the gears over the bigger range;
  `AUTO_UP` 0.82→0.90 revs each gear high before shifting (you hear it).
- **Sense of speed (rendering):** the ground speckle now **streaks** opposite travel (length
  ∝ speed), and the **camera leads** the rig in the travel direction — you see where you're
  rushing into. Plus the world simply scrolls ~1.6× faster.
- Speed-dependent handling rescaled to match: `STAB_H` 0.022→0.013, `SKID_SLIP` 16→28,
  `SCRAPE_MINSPD` 10→18, steering/dir-stab speed ramps /30→/50, /25→/45. Re-verified headless:
  the 3-wheeler still tips asymmetrically (right 0.19 / left 0.00), braking still dead-stops
  (144→0 in ~0.5 s), drivetrain push/pull intact.

### Course → a zoned Dutch road hierarchy (2026-06-09)

The schematic course (rung 1.75) was a uniform grid. Reworked `draw_course()` into a
**road hierarchy zoned by distance from origin**, so driving outward walks you up the
speed tiers and you *feel* the speed context (still just lines/rects on the floor —
nothing collides, rung 3 makes it solid). Concentric rings (euclidean `zone_at`):

| Zone | radius | pitch / lane | character |
|---|---|---|---|
| **CITY 30** | <1800 | 100 / 16 | tight grid, narrow streets, **houses packed in every block** (detail streaking past = feels fast even slow), zebra/school crossings |
| **TOWN 50** | <4500 | 200 / 26 | broader streets, bigger blocks, fewer/larger houses, roundabouts |
| **RURAL 80** | <8500 | 600 / 40 | long straights, sparse crossings, patchwork green/brown fields |
| **HWY 100** | <15000 | 1200 / 60 | wide, very sparse, long straights |
| **SUPER 120** | ≥15000 | 2400 / 104 | broad multi-lane (extra dashed lane dividers), longest straights |

Block pitches are origin-anchored multiples so coarser roads nest inside finer ones (the
grid *thins* across a ring boundary rather than popping). Everything deterministic by
world position → stable + headless-renderable. The HUD shows the current zone + its limit
(`zone_at(camera centre)` → `cur_zone`). Verified by driving a long straight and dumping
frames in each ring — all five read distinctly. **Note:** the zone is picked from the
camera centre so the whole screen is one zone; crossing a ring still nudges house/field
decoration in/out (acceptable for the schematic pass; rung 3's solid roads will smooth it).
