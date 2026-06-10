# sloop — build-your-own-vehicle, travel a procedural world (design seed)

**Status: building — rungs 1–1.95 (drive/drift/course/rigs/handling) + 2 (BUILD editor) + 2.5 (scrape) + 2.55 (tipping) + 2.6 (drivetrain FWD/RWD) + 2.7 (gears/transmission) + 2.7-engines (per-kind powerband
+ power-to-weight + extreme 45–300 km/h range) + handling-depth (per-axle grip + friction-circle
spin-out + rear-only handbrake) + long vehicles (9-long grid, SEMI/SCHOOLBUS)
+ neutral & reverse-in-auto + phone cockpit rework + drift dynamics (weight transfer
+ self-aligning torque + ramped steering — feel-tuning ongoing) done.** Cart:
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

### 7. Articulation & attachments — trailers, caravans, jackknife (future rung) ⬜

**The idea (player ask, 2026-06-09):** tow things behind you — a cargo **trailer**, a **caravan**,
a **semi's** box, or a working **attachment** (plough, tanker, gritter). Anything hitched behind
the vehicle is a *second body on a hinge*, and the headline payoff is the **jackknife**: brake too
hard in a bend and the trailer's momentum swings it around the hitch, folds toward the cab, and
takes the whole rig with it. This is the one thing the current rigid SEMI/SCHOOLBUS **can't** do
(they're single rigid bodies); it needs a genuinely new physics core, so it's its own rung.

**Two halves, and the build half comes first.** The physics is the flashy part, but the honest
question is *how does an attachment exist in the build grid* — and that deserves its own phase, not
a hand-wave:

- **Phase A — attachments in the BUILD grid (prerequisite).** A **hitch** part you place at the
  rig's rear (the kingpin / tow-ball / 3-point mount). A trailer is then *its own part grid* you
  build (frame/wheels/cargo/tank/plate — the same vocabulary), authored in BUILD like the tractor,
  and snapped to the hitch. Open design questions for this phase: one trailer grid or a **chain**
  (road-train / multi-trailer)? Does the trailer get its own COM/I/wheels readout panel? How does
  the editor show two grids (tractor + trailer side by side, joined at the hitch)? Are
  caravans/implements just trailers with different parts, or named attachment *types* with special
  behaviour (a plough that digs, a tanker whose fluid sloshes)? **Decide this before the physics** —
  the attachment's mass/wheel layout is what the jackknife math runs on, so it must be a real built
  object, not a procedural box. (An MVP could ship a *procedural* trailer first to prove the
  physics, then promote it to a grid-built one — note that as the cut line.)
- **Phase B — articulated physics + jackknife.** Each attachment is a rigid body pinned at the
  hitch (1 rotational DOF relative to its tower). The honest model (sketched, to be built):
  - The trailer's rear wheels pull it back into line — the classic trailer equation, the heading
    rotates toward the hitch's direction of travel at a rate `∝ (v_forward / trailer_len)·sin(β)`,
    where β is the articulation angle. **Forward = self-correcting (tracks behind); reverse flips
    the sign → it folds** (why backing a trailer is hard — and a great feel to capture).
  - **Jackknife = the friction circle on the trailer's tyres.** A cap on that restoring force;
    **hard braking unloads the trailer's wheels** (or the trailer brakes lock), so braking hard in
    a bend lets β run past the fold angle. Past the fold it's unrecoverable; before it, easing off
    lets the rear tyres pull it straight again.
  - **The cab-shove (the reaction).** The swinging trailer yanks the hitch, so the fold reacts back
    on the tractor's yaw — the cab gets spun by its own trailer. That feedback is what makes a
    jackknife *scary* rather than cosmetic; it's the whole point.
  - Reuses what's already here: the per-axle friction circle (rung handling-depth) is the same
    grip-let-go model, now on the trailer's axle; breakage (rung 4) could **snap the hitch** on a
    bad enough fold (the trailer detaches and scatters — orbit's part-scatter feel).

**Rung placement:** after the current handling work and the breakage rung (it wants the
collision/detach machinery for a snapping hitch). Phase A (build-grid attachments) is the gate; it
likely pairs with the part-vocabulary/cargo work (rung 5). Logged in the lever catalogue.

### 8. The per-wheel contact model (spring loads) — the unified lateral core ⬜ (design seed, 2026-06-10)

**The idea.** Today's lateral physics is a **two-axle "bicycle"**: all the wheels are lumped into
`frontGrip`/`rearGrip` at two virtual axles, and a stack of bolt-on terms (per-axle split,
`balance`, the tipping stand-in, `POWER_EAT`, the weight-transfer caps, the self-aligning torque)
each model one consequence of "where the load is." The honest replacement is **one mechanism: every
wheel is its own contact patch, with a vertical LOAD, and grip + drive + brake all scale from that
load.** It's **N-contact, not 4** — the model iterates over whatever wheels the build has (2 →
single-track, 3 → tripod, 4 → car, 8 → the semi), which is the cart's whole premise applied to grip
itself. Decided 2026-06-10 (player): the spring model below, migrated via an A/B scaffold.

**The load solve (this is the crux — it's what makes N≥4 well-defined).** Per-wheel load for 4+
contacts is **statically indeterminate** (the four-legged-table problem). The fix the player chose:
treat each wheel as a **soft vertical spring**. The rigid chassis is a plane that heaves and tilts;
each wheel's compression — hence its load — falls out of a small, always-solvable linear system:

```
compression_i = z0 + θpitch · x_i + θroll · y_i           // x_i,y_i = wheel pos in COM-local px
load_i        = k · compression_i  (clamped ≥ 0)          // a wheel at 0 has LIFTED
```
Solve (z0, θpitch, θroll) — three unknowns — from three balances:
```
Σ load_i        = M·g                                     // carry the weight
Σ load_i · x_i  = −M · a_long · H                         // pitch: LONGITUDINAL transfer (brake/throttle)
Σ load_i · y_i  = −M · a_lat  · H                         // roll:  LATERAL transfer (cornering)
```
3×3, determinate for **any** wheel count, every frame (N is tiny). `a_long` and `a_lat` are the
accels we **already compute** (`wt_long` from the weight-transfer work; `a_lat = vf·yawrate` from
tipping) — so the rung-1.0…drifts work feeds straight in. `H` is the COM-height stand-in (today's
`STAB_H`). Any wheel whose solved load goes negative has **lifted** → clamp to 0 and re-solve once;
that clamp *is* tipping, now emergent instead of a separate hull check.

**What each wheel then computes:** a friction-circle radius ∝ its `load_i`; its slip velocity from
`v_COM + ω×r_i` (decomposed in the wheel's own frame — steered wheels rotate by the steer angle);
a longitudinal force (drive force if it's a `P_DRIVE` wheel, + brake) and a lateral force, the two
**combined-slip-limited** (`√(Fx²+Fy²) ≤ μ·load_i`). Sum all wheel forces → net force (→ accel) and
net torque about the COM (→ yaw). One loop, integrated like everything else.

**What collapses into it (the orbit-style "one honest core" payoff):**
- **Longitudinal + lateral weight transfer** — both fall out of the pitch/roll solve; no separate caps.
- **Tipping** — a wheel load hitting 0; the hull/`STAB_H` check retires.
- **FWD/RWD/AWD + power-on over/understeer** — drive force only at drive wheels, capped by *that
  wheel's* load (a light drive wheel spins); `POWER_EAT` becomes emergent.
- **Self-aligning torque** — front wheels' lateral forces act ahead of the COM → the aligning torque
  emerges; the bolt-on `SELF_ALIGN_K` term may retire (or stay as a small digital-input aid).
- **Per-axle grip / `balance` / under-oversteer** — emergent from where the loaded, gripping wheels sit.
- **Real steered wheels** — the still-unbuilt "which wheels steer" lever (catalogue) lands here: a
  `steer` flag rotates a wheel's contact frame, and the turn comes from its lateral force, not a
  body torque. (Open: keep steering as a body torque at first, or go full per-wheel steer? The latter
  is more honest but reshapes the steering feel we just tuned — likely a follow-on within the rung.)

**What stays a 2-D abstraction (NOT closed by this — be explicit):** load is a **scalar per wheel**,
not real suspension. No body-roll angle, no dive/squat travel you'd *see*, no banking, no slopes.
`H` remains a stand-in for COM height. The model gives transfer + lift **in-plane**; it is not 3-D.

**Migration (decided): an A/B scaffold, then replace.** The per-wheel core *replaces* the tuned
lateral handling, so it's built **alongside** behind a switch and validated against the saved
harness fixtures — `drift2` / `kick` / `powerover` / `turn`, whose known-good slip/yaw curves
(this doc's drift entry) are the **regression oracle** — until it matches-or-beats the current feel.
Then flip the default and **delete the old core**. End state is a single per-wheel model; the scaffold
is temporary. The harness is exactly the tool for this (build, measure, A/B, switch).

**Open sub-decisions for build time:** spring stiffness `k` uniform vs per-part (a heavier axle
sags more); how many clamp-and-re-solve iterations for multi-wheel liftoff (likely 1–2); whether
to keep the digital-input self-align aid after forces go per-wheel; steered-wheel timing (above).

**Rung placement & synergy:** the natural successor to the drifts work, and it **sets up rung-4
breakage** — per-wheel load means losing a wheel redistributes its load to the others *emergently*
(the spring solve just re-runs with one fewer contact), and impact force can be shared by load.
Single-track (≤2 wheels, the bike) stays its own exempt path, as it is now.

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
| **Engine delivery curve** | how power comes on with speed: electric flat (snappy), gas revvy (mid-range band), diesel low-end grunt, steam spool-up, nuclear huge+flat. One `delivery(kind,rpm)` curve; see §1a | 2.7 | ✅ |
| **Engine power-to-weight → top speed + accel** | each kind sets power + mass: power sets thrust (→ top speed via thrust/drag), power÷mass sets accel. The strong kinds out-thrust the old generic engine, raising the 112 km/h ceiling | 2.7 | ✅ |
| **Gearing per engine kind (the speed scale)** | a kind carries its own `vref` (gearing), so the top-gear redline — the hard speed ceiling — moves with it. Unlocks the extreme range: RACE V12 geared tall → 300 km/h, TRACTOR geared ultra-short → ~45 km/h, no global re-tune | 2.7 | ✅ |
| **Transmission / gears** | a gear ratio trades torque ↔ top speed; RPM rises in a gear, drops on a shift. single (electric/EV = 1 gear) / automatic / manual×5 (wrong gear lugs or over-revs). Reverse is a gear, not a brake function. Engine pitch tracks RPM (the satisfying sound). See §1b | 2.7 | ✅ |
| **Muscle throttle (stamina + rhythm)** | foot/hand crank: each press = one stroke (`THR_IMPULSE`), gated by a stamina meter; the no-fuel starter rig. See §1a | 2.8 | ⬜ |
| **Articulation / jackknife (trailers, caravans, attachments)** | a second body on a hitch: tows behind, the rear tyres pull it in line (forward-stable, reverse-folds); brake hard in a bend → the trailer swings past the fold angle and yanks the cab around. Needs a build-grid attachments phase first (a hitch part + a trailer grid). See §7 | future (post-breakage) | ⬜ |
| **Wheel area / ground pressure** | traction = f(wheel area ÷ mass) per terrain; heavy-on-few-wheels bogs in sand | 3 (biomes) | ⬜ |
| **Per-axle grip (two-axle model)** | lateral grip split front/rear of the COM, applied as a force at each axle → a yaw couple. Front lets go = understeer (push), rear = oversteer (tail out); rear-only handbrake; FWD washes / RWD spins under power. Replaces the body-level scalar bleed (single-track exempt) | handling-depth | ✅ |
| **Grip limit → spin-out ("uit de bocht vliegen")** | the friction circle: each axle holds slip only up to `SLIP_MAX`, then LETS GO (force saturates → it slides). Corner too hard/fast → the loaded axle breaks away → plough wide or spin. The driven axle's limit also shrinks with the power it lays down (power-on over/understeer). Sibling of 2.55 tipping (grip loss from load leaving the hull) — this is from exceeding the limit at speed | handling-depth | ✅ |
| **Dynamic weight transfer (longitudinal)** | the realized longitudinal accel shifts load front↔rear (low-passed like suspension), scaling each axle's grip cap: braking loads the front (turn-in, lift-off rotation), throttle loads the rear (squat, traction, the power-drift bite). The *dynamic* half of weight (static distribution → COM/I already exists) | drifts | ✅ |
| **Self-aligning torque (caster / trail)** | the front tyres rotate the rig toward its travel direction when sliding — the car counter-steers itself. Catches a slide into a held angle instead of a spin, and assists digital counter-steer. ∝ `sin(slip)`, capped (past ~55° it's spinning, let it). The thing that makes a drift HOLD | drifts | ✅ |
| **Ramped (analog) steering** | binary keys (−1/0/+1) wind a smoothed `steer_pos` toward full lock while held and ease back on release; a quick opposite tap trims the lock off a notch → fine counter-steer from digital/touch input. Without it a realistic drift is unholdable on a phone | drifts | ✅ |
| **Per-wheel spring contact (the unified core)** | every wheel its own contact patch with a vertical LOAD from a spring solve (heave+pitch+roll, determinate for any N wheels); lateral grip scales from load. SUBSUMES per-axle grip, longitudinal+lateral transfer, and tipping (load→0) into one mechanism; replaced the 2-axle bicycle for ≥3-wheel rigs (single-track keeps its bleed). See §8. *(Per-wheel DRIVE/brake force — fully emergent power-oversteer — is the remaining optional refinement.)* | drifts/§8 | ✅ |
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
- ✓ **Ignition + stall ("uitvallen") — DONE (2026-06-09).** `I` toggles the engine (off ⇒ no
  thrust, no creep, silent, coast); it **stalls** when `rpm < STALL_RPM (0.12)` while rolling
  above `VSTALL_MIN (8 px/s)` in a forward gear — so tall gears lug out at low speed but a
  launch / dead-stop idle / AUTO never false-stall. `RESTART_GRACE` lets re-ignition catch.
  Retires the "forgiving no-stall" caveat. See the build log → "Ignition + stall".
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
*(Superseded — full stall + ignition landed later; see the ignition/stall rung below.)*

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
| **CITY 30** | <1800 | 100 / 26 | tight grid, **one-lane** one-way streets (no centre line), **houses packed in every block** (detail streaking past = feels fast even slow), zebra/school crossings |
| **TOWN 50** | <4500 | 200 / 50 | **two lanes** (~2× city, centre line), bigger blocks, fewer/larger houses, roundabouts |
| **RURAL 80** | <8500 | 600 / 50 | two lanes like town, long straights, sparse crossings, patchwork green/brown fields |
| **HWY 100** | <15000 | 1200 / 76 | wider, very sparse, long straights |
| **SUPER 120** | ≥15000 | 2400 / 104 | broad multi-lane (extra dashed lane dividers), longest straights |

Block pitches are origin-anchored multiples so coarser roads nest inside finer ones (the
grid *thins* across a ring boundary rather than popping). Everything deterministic by
world position → stable + headless-renderable. The HUD shows the current zone + its limit
(`zone_at(camera centre)` → `cur_zone`). Verified by driving a long straight and dumping
frames in each ring — all five read distinctly. **Note:** the zone is picked from the
camera centre so the whole screen is one zone; crossing a ring still nudges house/field
decoration in/out (acceptable for the schematic pass; rung 3's solid roads will smooth it).

**Follow-ups (same day):** the SPEED readout now reads **km/h** (`spd · KMH`, 0.72 — top
~166 px/s ≈ 120 km/h) so it lines up with the zone limit signs; the limits aren't enforced
yet but the number is now meaningful against them (groundwork for rung 3 "speeding bites").
And **city streets are one-way** — no yellow centre line (two-way) like town/rural; the
centre dash is gated out of `Z_CITY`, so the city reads as a tight one-way grid and town
now matches rural's two-way treatment.

### Gear ratios grounded in real data (2026-06-09)

The first-cut gear ratios were eyeballed. Looked up real 5-speed manuals and replaced them:
a **Getrag/Muncie set — 3.50 / 2.05 / 1.38 / 0.94 / 0.72** (final drive ~3.6, folded into
`V_REF` + `ENGINE_POWER`). The real pattern our guess missed: 1st should be a *low* torque
gear (~3.5, not 2.6), 4th ≈ direct (1.0), **5th an overdrive (<1, ~0.72)** — and the spacing
is **progressive** (big drop 1→2, gears close up top), the "staged" factory pattern. Only
our old 2nd (2.0 ≈ 2.05) had been right.

- Re-derived the scale knobs to keep the feel: `V_REF` 200→135 (so the overdrive top gear
  redlines just above top speed), `ENGINE_POWER` 850→1180 (compensates the lower top-gear
  ratio). Reverse = 1st (3.50), as real boxes share it.
- Verified: AUTO climbs through all five, top ~159 px/s ≈ **114 km/h** (just under the SUPER
  120 sign); low gears are brief (real), you dwell in the overdrive 4th/5th.
- **SINGLE (electric)** retuned for the now-lower drag: instead of flat torque (which ran
  away to ~250 px/s) it's flat torque *tapering toward the motor's max revs* (`SINGLE_RATIO ·
  clamp(1.15 − 0.6·rpm)`) — snappy launch, no shifting, moderate top ~118 px/s ≈ 85 km/h (the
  single-speed EV trade: you sacrifice top end). Reverse/tipping/braking unaffected.

Sources: [5speeds.com — wide vs close ratios](https://5speeds.com/ratios.html),
[OnAllCylinders — GM/Chevy manual gear-ratio charts](https://www.onallcylinders.com/2017/10/27/manual-transmission-gear-ratio-chart-for-gmchevy-vehicles/),
[Transmission Digest — manual transmission theory](https://www.transmissiondigest.com/manual-transmission-theory-back-to-basics/).

### Powerband grounded in a real dyno (2026-06-09)

The torque curve `powerband(rpm)` was also eyeballed (a parabola peaking at 0.6). Looked up
real dyno data (x-engineer.org) and refitted it to a **Honda 2.0 SI naturally-aspirated
gasoline** engine, normalised (torque ÷ peak, rpm ÷ redline):

| rpm (norm) | torque (% peak) |
|---|---|
| idle ≈ 0.12 | **61 %** |
| **0.66 (peak)** | **100 %** |
| redline 1.0 | **79 %** |
| > 1.0 | rev-limiter cut → 0 |

Curve now `clamp(1.0 − 1.82·(r−0.66)², 0.6, 1.0)` with a hard cut past redline. What the
guess missed: **peak torque sits at ~⅔ of redline** (peak *power* is ~95 % of redline,
torque ~80 %, per the rule of thumb), the engine **still pulls ~79 % at redline** (not a
sag), and the low-rpm side is **flat-ish** (real curves don't drop to a parabola down low —
the 0.6 idle floor models that). Top speed shifted ~159→153 px/s (~110 km/h) — left as is.

**Reference curves for the future §1a engine *kinds*** (same dyno source, captured now so
they're not eyeballed later — normalised peak-RPM and idle/redline % of peak):

| Engine | peak torque @ | idle % | redline % | character |
|---|---|---|---|---|
| **NA gasoline** (Honda 2.0) | ~0.66 redline | 61 | 79 | the curve above — broad mid peak |
| **turbo petrol** (Saab 2.0T) | ~0.40 redline | 44 | 68 | early peak, flat plateau (boost) |
| **diesel** (Toyota 2.0 D-4D) | ~0.45 redline | 39 | 70 | low, flat plateau then drop — grunt |
| **electric** | 0 rpm | 100 | tapers | max torque from a standstill (SINGLE already approximates this) |

Sources: [x-engineer.org — power vs torque (dyno curves)](https://x-engineer.org/power-vs-torque/),
[Haltech — how to read power curves](https://www.haltech.com/news-events/how-to-read-power-curves/).

### Idle creep — the car trundles in gear with no throttle (2026-06-09)

Player note (manual driver): with the clutch out and your foot off the gas, the idling
engine still trundles the car at a gear-set speed — and it climbs with the gear. Looked up
real idle-in-gear figures: **1st ≈ 5–10 km/h** (measured 3.9 mph GPS / ~5 mph diesel van /
~3.5 mph theoretical at 800 rpm idle), **2nd ≈ ~20 km/h is the floor** before it lugs below
~1000 rpm, and **3rd+ stalls** on most cars. The mechanism: creep speed = idle_rpm ÷ (gear
ratio × final drive), so it's **∝ 1/ratio** — taller gear, faster idle.

Modelled as a per-gear creep floor `vf → IDLE_CREEP / ratio` (eased on at `CREEP_ACCEL`),
applied when off-gas + off-brake + in gear; the **brake overrides** it (hold to sit still,
like a manual at a light), and reverse idles backward. **Anchored to the real-world data**
(not the player's higher memory): `IDLE_CREEP` set so 1st ≈ the textbook **~6 km/h**
(≈ 4.4 mph/1000 rpm × 800 rpm idle ≈ 3.5 mph), the rest falling out by 1/ratio — verified
**1st 6.0, 2nd 10.2, 3rd 15.1** (4th ~22, 5th ~29) km/h. No artificial cap — the 1/ratio
law gives sane values for every gear on its own. (Real manuals *stall* in tall gears at
idle; our forgiving no-stall model just lets the creep get faster — see §1b.) Sources:
[Promaster forum — 1st-gear idle speed](https://www.promasterforum.com/threads/first-gear-idle-speed-and-how-to-engage-it.40914/),
[GR86 forum — how slow in 2nd before lugging](https://www.gr86.org/threads/how-slow-can-you-go-in-2nd-gear.10665/).

### Ignition + stall ("uitvallen") (2026-06-09)

Player request: be able to **turn the engine off/on**, and have it **stall** when you lug a
too-tall gear. Both landed as one self-contained unit (`engine_on` bool + `stalled` flag +
one key + a stall check + HUD/sound), retiring the "forgiving no-stall" caveat from rung 2.7.

- **Ignition (`I`).** Cranks or kills the engine. Engine off ⇒ throttle ignored (no thrust),
  no idle creep, engine note silenced — you **coast** (drag, rolling friction, braking and
  steering all still work). Cranking plays a two-note starter chirp; a deliberate key-off
  plays a low thunk. `reset_vehicle()` always starts cranked.
- **Stall.** Cuts the engine when `rpm < STALL_RPM (0.12)` **while still rolling** above
  `VSTALL_MIN (8 px/s)`, in a forward gear, not SINGLE/electric. The threshold falls straight
  out of the existing gear math: idle creep holds rpm at `IDLE_CREEP/V_REF ≈ 0.21` in **every**
  gear (the `vcreep·ratio/V_REF` cancels the ratio), so a launch or a dead-stop idle never dips
  under 0.12 — **only braking/coasting a tall gear down past idle does**. AUTO downshifts at
  0.42 so it's naturally stall-proof (like a real automatic); **the bite lands in MANUAL**,
  which is finally what makes MANUAL matter. A `RESTART_GRACE (0.5 s)` of stall-immunity after
  a crank means re-ignition always takes and gives you a beat to downshift (the starter
  catching). Stall plays a cough; the HUD blinks red **"STALLED ▸ I to start"** (a deliberate
  key-off shows steady grey **"ENGINE OFF ▸ I"** instead — the `stalled` flag distinguishes).
- **Verified headless** (`build/.bake/*.script` + trace): braking 5th down from 138→11 px/s
  stalls at f454 (`rpm` under 0.12, `engine_on→0`, `stalled→1`); a MANUAL 1st-gear launch and
  180 frames of idle creep stall **zero** times (creep sits at rpm ≈ 0.21); key-off→re-ignite
  cranks back and drives. Bumped the runtime `WATCH_MAX` 16→24 — the cart now sets 18 watches,
  so engine state was silently overflowing the trace/overlay.

**Still open:** per-engine-kind powerband *curves* (§1a) and the engine roster remain the
next transmission-side unit; touch controls for DRIVE (on-screen throttle/steer/ignition).

### Cleaner DRIVE HUD — a dashboard strip (2026-06-09)

The old HUD scattered text across all four corners and, worse, kept **build-time facts**
(mass, engine/wheel counts, drivetrain) on screen every frame even though they never change
while driving and are already shown in BUILD's readout. Reworked into a **dashboard strip**: a
solid panel across the bottom holds every *live* gauge in one row — **speed** (big, the hero,
via `print_scaled` ×2), **gear** (big; red `R` for reverse), a **tach** bar (greys out when the
engine's dead), and the **transmission + drivetrain** labels. The play area above stays clear
save the rig name (dim, top-left) and the zone's **speed limit** centred up top like a road
sign. Warnings (STALLED / ENGINE OFF / TIPPING / DRIFT) share **one banner** just above the
strip in priority order — no more corner-hunting. Control hints live on a single always-on
line there too, rendered in **`FONT_TINY` (3×5)** so the whole control list fits on one row
without crowding (the engine's 4×6/3×5 fonts, not just the default 8×8). Build stats now live
only in BUILD mode, where they belong.

### Cockpit dashboard — touch/mouse/keyboard controls (2026-06-09)

Took the flat strip the rest of the way to a **driveable cockpit** (player ask, modelled on
the classic Amiga/C64 racer dashboards). The bottom band is now interactive instruments, every
control **labelled with its key** and reading **keyboard, touch AND mouse at once**:

- **Steering wheel** (left): a rim + three spokes that rotate with an eased `wheel_ang`; its
  left half steers left, right half right (◄ ► light up when pressed).
- **Pedals**: GAS / BRAKE / DRIFT buttons (held), labelled Z / X / SPC, lit green/red/yellow.
- **Speed**: a digital green LED readout (km/h).
- **RPM**: a round tach — tick marks over a 240° sweep + a needle (red past the redline, greys
  out when the engine's dead).
- **Stickshift H-gate**: the signature piece — a 3-column gate with a knob at the current
  gear's position (1/2 · 3/4 · 5/R); tap the **upper** half to up-shift (▲E), **lower** to
  down-shift (▼Q).
- **Right buttons**: IGN (lit when running), TRANS (cycles mode), BUILD (→ editor); a "▶ drive"
  `ui_button` in BUILD closes the loop for touch.

Implementation: a shared `#define` layout block feeds both `handle_input` (hit-testing) and
`hud` (drawing). Two helpers — `ctl_held` (level: `tap()` ‖ `mouse_down`+in-rect) for hold
controls and `ctl_hit` (edge: `tapp()` ‖ `mouse_pressed`+in-rect) for discrete ones — are
OR'd into the existing key reads, so all three input paths coexist. Built on `tap`/`tapp`+mouse
rather than `ui.h` capture: the zones don't overlap, so two fingers (hold gas + tap shift) just
work. Verified by scripting mouse clicks through the harness — IGN toggles the engine, TRANS →
MANUAL, gate-upper/lower shift up/down. `mobile-lint`: **touch-ready**.

### Reverse fix + a real H-gate (2026-06-09)

Two coupled issues a playtest surfaced. **(1) Reverse was practically unreachable.** Engaging
it required `|vf| < 5` px/s, but 1st-gear idle creep *holds* the car at ~8.3 px/s — so a
"stopped" car was always creeping faster than the gate allowed, and pressing reverse silently
did nothing unless you happened to also hold the brake. Fix: a `REV_ENGAGE_SPD` of 12 px/s
(above the 8.3 creep, below a real roll) for the R↔1 swap, and zero forward velocity on that
swap so it's a clean change rather than a jarring flip. Now reverse engages from a standstill
creep in **every** mode — verified headless (AUTO, idle-creeping at 8.3 → tap down → gear 0,
backs up under gas). The model is consistent: reverse sits "below 1st"; down at a near-stop
engages it, up returns to 1st; MANUAL also steps the forward gears, AUTO/SINGLE auto-manage
them but leave reverse a manual choice.

**(2) The gate didn't *show* reverse.** Redrew the stickshift as a real H-pattern knob (player
reference photo): **1·3·5** across the top, **2·4·R** across the bottom, chrome verticals joined
by the centre channel, and a ball riding the engaged gear. R glows orange when you're slow
enough to drop into it — so "how do I reverse?" answers itself. The upper/lower tap zones still
shift up/down (tiny E/Q key hints kept in the corners). In **AUTO / 1-GEAR** the gate is
`FILL_CHECKER`-dithered (the box auto-manages the gears, so manual shifting isn't "yours").

### Engine sound — one sustained voice, not a beep (2026-06-09)

The old engine note was a `hit(30 + rpm·30, INSTR_SAW, …)` re-triggered every 80 ms — the
PC-speaker buzz — and it only played *under throttle* (idle/creep was silent). Replaced with a
single **held voice** (`note_on`, `INSTR_ENGINE` = a saw through a resonant `FILTER_LOW`)
driven live every frame: `note_pitch` tracks the revs over a low range (idle ~24, redline ~52
MIDI) with a 70 ms glide (so it climbs in a gear and **drops on each upshift** for free),
`note_cutoff` opens the filter with revs + throttle (muffled at idle, bright under power),
`note_vol` sits lower at idle/creep than on the gas, and an `LFO_VOLUME` tremolo gives the
engine **throb**. It plays the whole time the engine runs — so idle/creep now has its low
rumble — and `note_off`s on stall / key-off / pause / BUILD. Verified by rendering WAVs:
idle is a steady ~0.040 RMS (was silent), gas ~0.066 and brighter, no clipping, low crest
(a continuous tone, not a beep train). Transients (shift clunk, skid, scrape) stay one-shots.

### Realistic acceleration — 0-100 km/h in ~9 s (2026-06-09)

A playtest nailed it: the first three gears were a sub-second blur. A WOT trace confirmed
0-100 km/h in **4.1 s** (0-50 in 0.6 s!) — hypercar acceleration — so gears 1-3 (which cover
0-64 km/h) were over in 0.95 s combined. The ratios were fine; the *clock* was ~2.2× too fast.

Fix: scale the **whole force budget** — `ENGINE_POWER`, all three `DRAG_*`, and `ROLL_FRIC` —
down by ~0.45× together. Top speed is `thrust/drag`, a **ratio**, so it's unchanged (held to the
pixel at 155 px/s ≈ 112 km/h); but every acceleration is `force/mass`, so the climb stretches
~2.2×. The rolling-friction term *had* to scale too — at top speed the engine only just
out-muscles the resistances, so leaving `ROLL_FRIC` fixed would have collapsed the top end.
Grip, braking and handling are all mass-based → untouched. Result (verified headless): **0-100
in 9.0 s**, top speed unchanged, gears 1-3 now ~2.0 s combined (each meaningful), and the rev
climb per gear is long enough that the held-voice engine sound actually sweeps.

(Top speed itself reads a touch low at 112 km/h — deferred to the engine-focus phase, where
power-to-weight / aero per engine kind is the right place to raise it.)

### Rung 2.7-engines — per-kind powerband, power-to-weight, and the extreme range (2026-06-09)

The §1a half of "engine powerband + transmission" that 2.7 left open: distinct engine KINDS,
each with its own delivery curve — plus the engine-focus follow-on (raise the 112 km/h), which
opened straight into a player request for **extreme** examples (a 300 km/h supercar, a 45 km/h
truck). All landed together, all emergent, no per-kind branch in the drive loop.

- ✓ **`delivery(kind, rpm)` — one curve, branched only by kind.** Replaced the single generic
  `powerband()`. Each curve is fitted to the real-dyno reference table already captured above:
  **electric** flat from a standstill, tapering toward max revs (the EV snap); **gas** the Honda
  2.0 NA curve (peak @ 0.66 redline); **diesel** low-end grunt (peaks ~0.45, flat, weak top);
  **steam** `delivery = boiler` (a 0..1 spool-up state that builds over ~4 s while the engine
  runs — the one kind that needs a per-rig variable); **nuclear** flat + immense. The drive loop
  calls it once for `thrust = power · throttle · delivery · ratio`; nothing else is kind-aware.
- ✓ **Engine is rig-wide with a KIND** (cycle **K** in BUILD; a sensible default transmission
  comes with it — electric → 1-GEAR, combustion → AUTO). `ENG[]` holds power + mass + colour +
  feel per kind. Engine cells tint to the kind's colour, the DRIVE HUD + BUILD readout name it,
  and the held engine voice gets a per-kind profile (electric whines high/bright, diesel/steam
  growl low/muffled, nuclear a steady hum, race a scream). Engines still SUM and share one kind
  (hybrids parked to rung 6). Only gas/diesel can stall (electric/nuclear/steam have no idle to
  lose) — the one extra line in the stall gate.
- ✓ **Power-to-weight raises the ceiling, honestly.** `power` sets thrust (→ top speed via
  thrust/drag), `power÷mass` sets accel. The strong kinds out-thrust the old generic 531-engine,
  so the top — which stays mass-INDEPENDENT — climbs. Gas is now the baseline (`ENGINE_POWER`
  600) at **122 km/h / 0-100 in 6 s** (was 112 / 9 s); the spread runs electric 90 · steam 93 ·
  diesel 110 · gas 122 · nuclear 139 km/h on the buggy.
- ✓ **The extreme range — gearing is part of the engine kind.** The blocker for a 300 km/h car
  was the gearbox, not power: top gear (0.72) redlines at `V_REF/0.72 = 135 km/h`, a hard ceiling
  on every rig. The fix that needs no global re-tune: give each kind its own **`vref`** (gearing),
  so the top-gear redline moves with the kind. Two extreme kinds (beyond §1a): **RACE V12** (huge
  power, light, `vref` 300 → redlines at ~417 px/s = **300 km/h**) and **TRACTOR** (bottomless
  grunt, `vref` 45 → geared out at ~62 px/s = **45 km/h**). Two example rigs load them — template
  **9 = SUPERCAR** (low 2-row slippery RWD body), **0 = TRUCK** (heavy wide 6-wheeler). Idle creep
  was reformulated to hold a constant idle rpm regardless of `vref` (creep speed scales with
  gearing) so neither extreme false-stalls; the normal kinds (`vref` = `V_REF`) are byte-for-byte
  unchanged.
- ✓ **`est_top_speed()` made honest.** The BUILD readout now solves the same terminal balance the
  drive core reaches — geared, delivered thrust near redline minus the constant `ROLL_FRIC·M`
  force, capped by the gearing ceiling — and shows **km/h** (matching DRIVE + the zone signs). The
  supercar's BUILD readout reads **TOPSPD 300 km/h**, the truck **45**, and they drive exactly that.
- ✓ **Verified headless** (per-kind WOT + the two example rigs, `--trace`): supercar tops **300.0
  km/h** (gear 5, rpm 1.00 — at the cap), 0-100 in 1.1 s / 0-200 in 4.6 s; truck tops **45.6 km/h**
  (geared out, rpm 1.01); the five §1a kinds land 90–139 km/h as above; gas-at-531 re-measured at
  111 km/h / 8.9 s confirms the calibration. Visual dumps confirm the pink RACE rig at speed and
  the wide green TRACTOR truck rendering correctly.

**Still open:** muscle engines (foot/hand crank, `THR_IMPULSE` + stamina — rung 2.8); fuel /
sources as the clock (battery/tank/firebox/recharge — rung 6); the hybrid active-engine toggle
(rung 6). Steam's boiler is a delivery state only — it doesn't yet consume fuel/water (rung 6).
RACE/TRACTOR are demo powertrains beyond the §1a roster (they bundle gearing as a vehicle-class
shorthand); if the part vocabulary later justifies an explicit gearbox part, the per-kind `vref`
is where that knowledge already lives.

### Rung handling-depth — per-axle grip + friction-circle spin-out (2026-06-09)

The two catalogue levers that needed no new world systems, only a deeper lateral model:
**per-axle grip** (true understeer/oversteer, rear-only handbrake) and the **friction-circle
spin-out** ("uit de bocht vliegen"). Both required the same thing — replacing the single
body-level grip scalar with a **two-axle ("bicycle") model**, which is the honest core for
lateral dynamics the way the rigid-body integrator is for the rest.

- ✓ **Two-axle model.** `recompute_body()` now splits lateral grip FRONT vs REAR of the COM
  (`frontGrip`/`rearGrip` at lever arms `aF`/`aR`). Each axle resists the lateral velocity AT
  ITS OWN POSITION; the two forces act fore/aft of the COM, so they form a **yaw couple**. Two
  payoffs fall out: (a) yaw damping is now PHYSICAL (∝ grip·wheelbase²/I) — a long heavy rig is
  calm, a short one darty, no hand-tuned constant; (b) the front and rear can let go
  independently. Tuned (`ANG_DAMP_AXLE` + the couple ≈ the old ~5/s on the buggy) so it reduces
  to the previous feel when symmetric + gripping: verified buggy **79°/s @ 90** (was 77/89),
  moto **87 @ 131** (exact — single-track stays on the old bleed, see below).
- ✓ **Friction circle → spin-out.** Each axle converts lateral slip to grip only up to
  `SLIP_MAX`; beyond that the force saturates — it has LET GO and slides. Front lets go →
  understeer (plough wide); rear → oversteer (tail steps out / spin). Verified: gentle/straight
  driving grips (slide 0.00), full-throttle-full-lock breaks loose (buggy drifts controllably,
  the tail-happy RWD/supercar spin), heavy truck never exceeds the limit.
- ✓ **Power-on over/understeer (the same circle).** The DRIVEN axle's limit shrinks with the
  drive force it lays down (`POWER_EAT`): rear-drive → rear breaks loose under power = oversteer;
  front-drive → front washes = understeer; AWD splits it. Verified FWD ploughs (won't rotate),
  RWD spins, on full power.
- ✓ **Rear-only handbrake.** The handbrake now cuts the REAR axle's grip limit only
  (`DRIFT_GRIP_MULT`), so the tail steps out while the front keeps tracking — a real drift/spin,
  not the old whole-body slide. Verified rear lets go (`slide_rear` = 1).
- ✓ **Feel + reconciliation.** A `SLIDE`/`PUSH` HUD cue (rear vs front let-go) joins the warning
  banner; the existing skid marks + screech fire emergently on the lateral slip. Single-track
  rigs (inline bikes, `nHull < 3`) are exempt and keep the old single-bleed bite — same exemption
  tipping uses. Tipping still scales both axle limits. `WATCH_MAX` bumped 24→32 (the cart now sets
  ~29 watches). Top speeds, the engine kinds, and braking are all longitudinal → untouched
  (re-verified gas 121 / supercar 300 / truck 46).

**Open / for playtest tuning:** the drift/spin *fun* (how loose, how recoverable) lives in a few
constants — `SLIP_MAX` (when it lets go), `POWER_EAT` (power-oversteer strength), `DRIFT_GRIP_MULT`
(handbrake bite), `GRIP_YAW_K` (per-rig damping) — all commented for tweaking. The rung-2.6
drivetrain push/pull (`STAB_YAW_K`) now coexists with the at-limit per-axle let-go (sub-limit
directional feel vs over-the-limit slide); if RWD reads too loose, that's the term to trim.
Per-axle weight-transfer under braking/accel (front dives → more front grip) is the natural next
refinement; today axle load ≈ its grip share.

### Long vehicles + brake skids (2026-06-09)

A grab-bag pass from playtest notes: bigger rigs, harder-stopping feedback, and two bug fixes.

- ✓ **The build grid is now 9 cells long (was 6)**, so genuinely LONG vehicles fit. `ED_CELL`
  shrank 20→13 to keep 9 cells between the palette and the readout; every existing preset just
  pads the extra columns with `P_NONE` (COM/I are computed from occupied cells, so they're
  unchanged — verified the buggy drives identically). The drive-view rig is correspondingly
  longer at `CELL` = 7 px/cell.
- ✓ **SEMI (template `-`) — the real 18-wheeler.** A full-length rig: cab front-right, long
  trailer, tandem rear axles + drive axle + steer axle (8 wheel cells), DIESEL. Everything
  lumbering falls out of the build, no special engine: **mass 37, I ≈ 14,400 (≈10× the buggy →
  a vast lazy turning circle), tops ~84 km/h** (it can't even pull top gear — settles in 4th —
  and the 8 wheels + big body pile on rolling+aero drag). Glacial to wind up (heavy).
- ✓ **SCHOOLBUS (template `=`)** — long boxy body, front-engine rear-drive, two axles, DIESEL;
  ~85 km/h, heavy and long so it leans into corners but isn't the multi-axle semi.
- ✓ **Hard-braking skid marks + screech.** Standing on the foot brake above `BRAKE_SKID_SPD`
  (~50 km/h) now locks the tyres: straight skid marks laid frame-by-frame + a brake screech
  (a touch lower than the cornering scrub). Only on a hard, fast stop — gentle braking is silent.
- ✓ **Bug: engine no longer sticks across templates.** Loading the supercar then pressing `1`
  used to leave the RACE V12 bolted on the buggy (templates were "keep current engine"). Now a
  template is a WHOLE vehicle — the eight handling demos load GAS, the extremes their own
  powertrains. (`K` in BUILD still swaps the engine deliberately afterward.)
- ✓ **Bug: `est_top_speed()` was wrong for heavy rigs.** It assumed top gear, but a heavy rig
  tops out in a LOWER gear (more thrust). Now it takes the best gear — `max` over gears of
  `min(drag-limited, that gear's redline)` — so the semi's BUILD readout reads ~73 (was a
  nonsense 33; actual ~84). Light rigs unchanged (they reach top gear, which wins the max).

**Handbrake clarity:** there are two brakes — the foot BRAKE (`X`/`↓`, pure deceleration, stops
you) and the handbrake on `SPACE`, labelled **DRIFT** because that's its job (it cuts the REAR
grip → tail out). They're distinct; "DRIFT" is just the effect-name for the e-brake. (Open: could
relabel the pedal "HAND" if "DRIFT" reads as a separate thing.)

### Neutral + reverse-in-automatic (2026-06-09)

Two playtest gaps in the gearbox. Re-encoded `gear` as **-1 = REVERSE · 0 = NEUTRAL · 1..5 =
forward** (was 0 = reverse, no neutral), which fixed both at once:

- ✓ **MANUAL gets a neutral ("free").** Up/down now step the whole sequence R ↔ N ↔ 1 ↔ … ↔ 5
  (a real H-gate). In neutral the engine is disconnected — **no thrust, no idle creep, just
  coast** — but the throttle still **free-revs** it (rpm climbs with the gas, a satisfying vroom
  with the car standing still). N→R only engages at a near-stop.
- ✓ **Reverse works in AUTOMATIC.** AUTO/1-GEAR don't expose neutral; instead one **down-tap from
  a standstill drops straight into R** (and up returns to DRIVE) — so you can actually back up in
  automatic, which you couldn't before (reverse was only reachable through the dithered gate and
  read as disabled). Verified headless: AUTO tap-down → gear -1 → gas → vf goes negative; MANUAL
  down to N → gas revs (rpm 0.9) but vf stays 0; forward unchanged in both.
- ✓ **The H-gate shows it:** the ball rides the slot for 1-5, sits in the **centre channel for
  NEUTRAL**, and the **R slot** for reverse (glows orange when you're slow enough to drop into it).
  Removed the full-gate dither in AUTO/1 — the gate now reads live in every mode (it was hiding
  that reverse is reachable).

### Articulation / jackknife — documented as a future rung (2026-06-09)

Player asked to capture the trailer/caravan/semi/attachment idea (and the jackknife) rather than
build it now. Written up as **§7** above + a lever-catalogue row: the two-phase plan (a build-grid
**attachments** phase — a hitch part + a trailer grid — *before* the articulated-body physics), the
honest trailer model (rear-axle tracking, forward-stable/reverse-folds, friction-circle jackknife
under braking, the cab-shove reaction), and where it lands (after breakage, reusing the per-axle
grip + detach machinery). Not built — design seed only.

### Cockpit rework from a phone playtest (2026-06-10)

Three fixes from playing on a phone, all in the dashboard layout + the gear selector (drive
core untouched):

- ✓ **Pedals and wheel split to OPPOSITE edges.** They were jammed together in the left
  third (wheel at the far-left corner, gas pedal right beside it at x62), so both thumbs
  fought over one side. Now the **pedals sit at the far-LEFT edge** (left thumb) and the
  **steering wheel at the far-RIGHT edge** (right thumb) — max spacing on a phone. The gauges
  (speed, tach), the mode buttons (IGN/TRANS/BUILD) and the gear selector fill the middle,
  where no thumb rests while driving. Layout `#define`s rewritten; every control re-placed.
- ✓ **Every gear is individually tappable.** The old gate only let you tap "upper half = up /
  lower half = down" (sequential), so reverse in MANUAL was a fiddly step-down-to-N-then-R.
  Now each slot of the H-gate is its own tap target — **tap R directly to reverse**, or grab
  any forward gear / N outright (`mgate_rect()` defines the slots, shared by hit-test + draw;
  a new `gear_req` carries the tapped gear into `update_drive`, where reverse is still
  validated against `REV_ENGAGE_SPD`). Keys E/Q still shift sequentially for the keyboard.
- ✓ **AUTO/1-GEAR shows a clean D / N / R selector** instead of the 5-speed H-gate (the box
  manages the forward gears itself, so the numbered slots were just noise). Tapping DRIVE /
  NEUTRAL / REVERSE sets the gear directly; the engaged one lights. This also **added a real
  NEUTRAL to AUTO** (coast + free-rev), which it didn't expose before. MANUAL keeps the full
  tappable H-gate. `agate_rect()` lays out the three buttons.
- ✓ **Verified headless** (scripted mouse clicks + `--trace`): AUTO tap-REVERSE from a stop →
  gear -1, backs up, then tap-DRIVE → gear 1 and auto-shifts up; MANUAL tap gear "3" → jumps
  straight to 3rd, tap "R" at a stop (engine running) → reverses to -42 px/s. Screenshot dumps
  confirm both the AUTO D/N/R selector and the MANUAL H-gate (1·3·5 / 2·4·R, R orange when
  slow) render with the new edge-split layout. (Note: tapping straight into a tall gear from a
  standstill lugs and stalls — the existing "wrong gear bites", now reachable in one tap.)

### Drifts: weight transfer + self-aligning torque + ramped steering (2026-06-10) — *tuning ongoing*

Goal from a playtest: **long, controllable, realistic drifts.** A harness probe showed the
baseline couldn't sustain one — a handbrake flick peaked at ~39° slip then the grip snapped
back and the rig spun out inside ~1 second. Three coupled additions fixed the *mechanism*;
final feel is a play-test tuning loop (the constants are all `#define`d and commented).

The diagnosis split into three gaps, each with a fix:

- ✓ **Weight transfer (the requested "weight business").** Static weight *distribution* (part
  masses → COM/I/balance/per-axle grip) already drives the cart; this adds the *dynamic* half.
  The realized longitudinal accel each frame (thrust+drag+brake, already in `vf`) is low-passed
  (`WT_LAG`, models suspension settle) into a load shift that scales each axle's friction-circle
  cap: **braking loads the front** (turn-in, lift-off rotation), **throttle loads the rear**
  (squat, the power-drift bite). `WT_LONG_K / WT_MAX / WT_FLOOR / WT_CEIL`. Correct + emergent;
  at steady speed it's ~0 (no accel = no transfer), as it should be.
- ✓ **The binary-steering blocker → ramped steering.** `in_steer` is only −1/0/+1 (the keys AND
  the on-screen wheel), so there's no analog counter-steer — a knife-edge sim drift is unholdable
  on a phone *regardless* of the physics. Fix: the physics now steers off a smoothed `steer_pos`
  that **winds toward full lock while held and eases back when released** (`STEER_RATE` /
  `STEER_RETURN`) — a quick opposite tap backs the lock off a notch, giving fine counter-steer
  from digital input. The cockpit wheel mirrors `steer_pos`.
- ✓ **Self-aligning torque (caster / pneumatic trail) — what HOLDS the slide.** Real front tyres
  rotate the car toward its *travel* direction when sliding (the car counter-steers itself).
  Added as a yaw toward the velocity vector ∝ `sin(slip) = vl/speed` (no `atan2` — studio.h has
  no math.h), capped at `SELF_ALIGN_SIN` (~55°; past that it's spinning, let it). `SELF_ALIGN_K`
  is biased LOW so a drift can develop and be held — just enough to catch a spin and assist the
  digital counter-steer.
- ✓ **Killed the RWD spin-runaway.** The drivetrain push term (`STAB_YAW_K`, what makes RWD loose)
  is *anti-damping*; through a full slide it amplified yaw (~588°/s²) far faster than the self-align
  could arrest it (~100°/s²) → every drift wound into a spin. Now it **fades as the slide saturates**
  (`DRIFT_PUSH_FADE`): RWD still kicks out, but past the limit the slide is the boss, not the
  push. This is the change that turned "spins out" into "holds."
- ✓ **`POWER_EAT` 0.55 → 0.72** so throttle keeps the rear loose enough to *sustain* the angle
  (balanced against the self-align pulling it straight — their balance sets the held drift angle).
- ✓ **Verified headless** (`--trace`): a kick-then-counter sequence now builds a slide, **holds
  ~37–40° while you steer + throttle with the speed staying up (~80 px/s), and decays cleanly to
  straight when centred — no spin-out, no speed crash** (was: 39° → spin → 8 px/s in <1 s). Normal
  driving unhurt: the default buggy still corners to ~86°/s with a natural ~13° slip, not twitchy.

**Open (the play-test loop — needs hands on a phone, can't be tuned open-loop):** the *feel* of
the window — how easily it breaks loose, how wide the holdable angle, how much throttle trims it,
how forgiving the recovery. Lives in `SELF_ALIGN_K` (catch strength), `POWER_EAT` (how loose under
power), `DRIFT_PUSH_FADE` (RWD looseness through the slide), `STEER_RATE`/`STEER_RETURN` (counter-
steer responsiveness), and `WT_LONG_K` (how much brake/throttle shifts grip). All `#define`d at the
top of `sloop.c` with tuning notes. Lateral (left↔right) weight transfer is still the *tipping*
approximation — a true 4-contact model is the bigger future step if the outside-wheel-digs-in
detail is wanted.

### A basic cargo block (2026-06-10)

A simple **`P_CARGO`** part — heavy dead weight (mass 8, ~2× an engine; no engine/wheel function),
a brown crate in the BUILD palette (hotkey `O`). Shipped early (the full storage/scavenging `cargo`
of §5 comes later) to make the **static weight-distribution** lever tangible: mount it fore vs aft
and the COM / `balance` / `I` / per-axle load all shift. Verified headless — the same block reads
**balance −0.25 (oversteer) at the rear vs +0.33 (understeer) at the front** of the buggy. It's also
the intended heavy-payload **test fixture for the §8 per-wheel spring model**. Palette spacing
tightened (20→18 px) so 8 buttons fit above the hint line.

### Per-wheel model — Phase 1: the spring load solve (2026-06-10)

The hard, foundational part of §8, built and validated **in isolation** (forces UNCHANGED — still
the 2-axle core — so zero risk to the tuned drift; this is pure plumbing under it).

- ✓ **`solve_wheel_loads(aLong, aLat)`** — each contact a vertical spring; solve chassis heave +
  pitch + roll from the 3 balances (carry `M`, longitudinal moment, lateral moment) → per-wheel
  load, **determinate for any wheel count** (Cramer's on a 3×3). A wheel whose load solves negative
  has **lifted** → dropped and re-solved (≤3 iters); `<3` contacts or a collinear/degenerate layout
  → even split (single-track keeps its own path). Fed by the accels we already had — `wt_long`
  (longitudinal) and `aLat` (= `vf·yawrate`, from tipping). Wheel positions/cells/drive-flags
  mirrored out of `recompute_body` into `wheelP*[]`.
- ✓ **Visible in BUILD** — each wheel cell shows its resting load (warm = heavy, blue = light, red =
  ~lifted) + a `load/wheel` caption. Drop cargo at the rear and the rear wheels' numbers jump.
- ✓ **Validated** (`--trace` + BUILD dumps): rear cargo → rear wheels **18 vs 10** front (readout
  oversteer); hard braking → `loadF` **8→13**, `loadR` **8→4** (nose dive); cornering → load splits
  to the **outside** wheels. Coord gotcha logged: local `+y` is the vehicle's *right* (screen-y down).
- Trace headroom: `WATCH_MAX` 32→40 in `runtime/studio.c` (trace-only; left out of the cart commit
  since studio.c carries a parallel agent's edits — committing it wholesale would sweep their WIP).

**Next — Phase 2:** swap the lateral force resolution to a per-wheel loop (friction circle ∝ each
wheel's load; yaw = Σ force×lever), A/B'd against the saved `drift2`/`kick`/`powerover`/`turn`
traces until it matches-or-beats the feel. Then Phase 3 (per-wheel drive/brake, retire the bolt-ons),
flip the default, delete the old core.

### Per-wheel model — Phase 2: the per-wheel force loop (2026-06-10)

The lateral force resolution now has a **per-wheel path**, built **behind a runtime toggle (`M`)** so
the tuned 2-axle core stays intact and both run in one binary for A/B.

- ✓ **The loop:** each gripping wheel resists the lateral slip *at its position* (`vl + ω·x_i`),
  capped by a friction circle **sized by its own spring load** (`SLIP_MAX · load_i/avg`) — so
  longitudinal + lateral transfer *and* static cargo all bias grip per wheel. Handbrake cuts the
  rear wheels (`x<0`); a driven wheel eats its own grip under power. `vl -= Σacc`, `angVel -= Σacc·x_i`
  (the yaw couple, same form as before). **Tipping is emergent** — a lifted wheel has load→0 → cap→0 →
  no grip (no `tipMul` in this path). `wt_long`/`wt_xfer`/`solve_wheel_loads` hoisted ahead of the
  branch so both paths share them.
- ✓ **A/B verified** (`M`-on vs off, same scripts): **normal turn identical** (19/72 °/s both; 86 vs
  78 at the limit — a touch more grip); **the drift matches the tuned core to ~1–2°** across build /
  hold (~39°) / trail-out, carrying *slightly more speed* (less scrub); power-over likewise. Matches-
  or-beats — the migration's bar. Default stays the **old core** (`use_wheel_model=0`) pending a feel
  pass on desktop; `M` flips to the per-wheel model live (green "WHEEL-MODEL" tag).

**Next — Phase 3 (after the feel pass):** per-wheel drive/brake (power-oversteer + traction loss fully
emergent), retire the now-redundant bolt-ons (`POWER_EAT`, the tipping `tipMul`/hull check, maybe the
`SELF_ALIGN` aid), flip the default, delete the old 2-axle branch.

### Per-wheel model — Phase 3: consolidated, per-wheel is THE core (2026-06-10)

Felt good on desktop, so the migration completed: **the per-wheel model is now the unconditional
lateral core for ≥3-wheel rigs** (single-track bikes keep their whole-body bleed). The toggle (`M`),
the green tag, the `wmodel` watch and **the entire old 2-axle force branch are deleted** — the lateral
block is now just `if (nHull >= 3) { per-wheel } else { single-track }`.

- ✓ **Re-verified unconditionally** (no toggle): drift holds **~39°** then trails out (38.9→36→18.8,
  speed up), buggy turn **78°/s**, power-over **−60°**, motorbike drives on the single-track path
  (`2axle=0`, 84°/s) — all matching the validated toggle-on numbers. `recompute_body` left intact
  (its `frontGrip`/`aF`/… still feed the BUILD readout, trace, and the `ANG_DAMP_AXLE` choice).
- **Kept, not retired:** `POWER_EAT` (now a per-wheel cap term — power-oversteer), the `SELF_ALIGN`
  aid (digital-input counter-steer help), and `tipMul` (single-track path only). They're not dead.
- **Deferred (optional refinement, NOT done):** moving **drive/brake force per-wheel** (combined-slip
  circle) so power-oversteer + traction-loss are *fully* emergent and `POWER_EAT` can retire. Flagged
  optional because it touches the **longitudinal core** (top speed / accel / gears all key off the
  lumped `thrust` today) → broad blast radius; the current `POWER_EAT` power-oversteer already feels
  right. Pick it up only if we want a light drive wheel to literally spin up / lose traction.

**Net:** §8's headline collapse landed — weight transfer (long + lat), tipping, and per-axle grip are
now ONE mechanism (the spring loads), and it also makes **cargo placement bias grip per wheel** for
free. The per-wheel load model is the natural substrate for **rung-4 breakage** (lose a wheel → loads
redistribute on their own).
### Drift-exit hysteresis — the lazy trailing exit (2026-06-10)

The Stage-3 piece the drifts work flagged and skipped: grip recovered *instantly* (the cap is
recomputed each frame), so a drift ended crisply the moment you straightened. Added a lingering
**`drift_loose`** (0..1): a real rear **breakaway** (slide past `DRIFT_TRIG` 0.5) spikes it to ~1
(instant attack), then it **bleeds off slowly** (`DRIFT_DECAY`); while it bleeds, the rear caps are
cut by `DRIFT_RECOVER`·loose, so the tail **hangs out a beat and settles** instead of snapping
straight. Gated above `DRIFT_TRIG` so **normal hard cornering doesn't loosen** (verified: normal
buggy turn `loose` stays 0.00; a real drift trails from 45°→29° at f220 vs 19° before, then settles).
Rear-axle + ≥3-wheel only (single-track zeroes it). Knobs: `DRIFT_RECOVER` (how loose on exit),
`DRIFT_DECAY` (how long the hang), `DRIFT_TRIG` (what counts as a breakaway) — all in `sloop.c`.

### BUILD readouts: 0-100 + power-to-weight (2026-06-10)

"Many of my vehicles don't go hard enough" → the model was fine (torque = `delivery(rpm)` curve ×
gear ratio, then F=ma); the gap was **you couldn't SEE power-to-weight in BUILD** — an overloaded
rig looked fine and only felt gutless once driven. Added **`est_0_100()`** (forward-integrates the
launch like `est_top_speed()` — best-gear thrust − drag − rolling-friction, F=ma at 60 Hz; returns
−1 if it can't reach 100) and two readout lines: **`0-100  X.Xs`** (green <7s / orange <13s / red
"never" = overloaded, add an engine / drop weight) and **`PWR/WT`** (engines·power ÷ mass). Verified:
stock buggy 5.3s green / PWR-WT 34.9; +4 cargo → red "never" / 13.3; +2 more engines → punchy again.
Also the diagnostic for the *sense-of-speed* question: if a build reads fast here but feels slow to
drive, it's a rendering/scale issue, not power.

### World scale pass — houses to a believable size (2026-06-10)

Player: a house reading the *same size as the car* feels weird. It did — car ≈ 28px (4 cells × 7),
city houses ≈ 15px, so a house was *smaller* than the car. Established the convention **1 cell (7px)
≈ 1 m** (car ≈ 4 m ✓, road lane ≈ 26px ≈ 3.7 m ✓ — both already right) and fixed the houses:
`draw_houses` now tiles **fixed ~5 m-facade houses** (`HOUSE_FACADE` 38px, drawn ~34px) instead of
stretching N-per-block, so a house reads consistently across zones and **bigger than the car**.
City/town block pitch enlarged 100/200→**150/300** so rows of 5 m houses fit (kept the 2× steps:
150·300·600·1200·2400 all nest, cleaner than the old 3× jump). Lanes unchanged (already to scale).
Placeholder course still (rung-3 rebuilds it solid) — this is just the proportion fix.

### Sense-of-speed camera pass (2026-06-10)

After the scale fix, a pass on *felt* speed (distinct from the power readouts — those tell you if a
build IS fast; this is whether it FEELS it):
- **Speed-zoom** — `draw()` now uses `camera_ex(...,cam_zoom,0)`; `cam_zoom` eases from 1.0 toward
  `1−CAM_ZOOM_PULL` (0.16) as speed → `CAM_ZOOM_REF` (260 px/s). The camera pulls BACK at speed →
  more world streams through the frame + you see further ahead. Pivots on screen centre, so the rig
  stays put and proportions are preserved. `draw_ground`/`draw_course` widen their draw bounds by the
  zoom margin so the pulled-back edges aren't left undrawn (verified clean at 162 km/h).
- **Longer streaks** — the ground speckle streak length `spd·0.085 (cap 11)` → `spd·0.13 (cap 20)`.
- **More lead** — camera lead `0.26` → `CAM_LEAD 0.34` (see further into the travel direction).
Knobs all `#define`d. *Caveat:* in flat 2-D a pull-back widens the view but lowers per-object screen
speed — the streaks/lead carry the speed cue. If it reads calmer rather than faster, flip
`CAM_ZOOM_PULL` negative (zoom IN at speed) — a one-line change. Feel-tune on desktop.

### Per-wheel drive: load-sensitive traction + wheelspin (2026-06-10)

The deferred per-wheel-drive piece, done as the high-value/low-risk 80%: the traction cap is now
**`MU_TRACTION × load on the drive wheels`** (last frame's spring solve), not a constant. Player call:
do the physically-correct thing — let too much torque break grip. Set `MU_TRACTION` low enough that
**grip actually BINDS**: flooring (esp. low gear = high thrust) exceeds it → the excess is **wheelspin**
(no extra accel, but it squeals + smokes + lays burnout marks). Emergent + tied to weight:
- **Buggy** chirps in 1st off the line (spin 0.21), hooks up from 2nd — 0-100 unchanged (6.0s).
- **Supercar** lights up hard (spin ~1.5 through 1st/2nd), traction-limited launch — 0-100 0.9s→**3.3s**
  (far more believable), hooks up by 3rd. Few-drive-wheel / cargo-lightened / FWD-under-power rigs spin more.
- **Feedback:** a high scrabbling squeal (`INSTR_NOISE`, gated `SPIN_SQUEAL`), grey/white tyre **smoke**
  (`smoke_puff`, reuses the spark pool) + **burnout marks** at the drive wheels.
- Top speed (drag-limited) and the drift (mid-corner thrust stays under grip) both **unaffected** — verified.
`POWER_EAT` kept for cornering power-oversteer (the full combined-slip that would retire it is the
deeper, riskier option, still deferred). Felt good → **consolidated**: the `M` A/B toggle is gone, the
load-sensitive cap is now the permanent traction model. Knobs `MU_TRACTION` (how easily it spins) +
`SPIN_SQUEAL` (feedback threshold).
