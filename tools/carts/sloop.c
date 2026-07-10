/* de:meta
{
  "title": "sloop",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "rigid-body",
    "verlet-integration",
    "spring-damper",
    "noise-terrain"
  ],
  "lineage": "Inspired by Cataclysm: DDA's vehicle system; sibling to orbit (same integrator) and coaster (same physics honesty); extends worldgen's infinite chunk-streamed city as its drive world.",
  "genre": "sandbox",
  "description": "Build a vehicle from parts on a grid, then drive it across a procedural world - Cataclysm: DDA's vehicle system as the seed. The one honest core: the rig is NOT a sprite with stats, it's a GRID OF PARTS, and how it drives is COMPUTED from what you bolted on. Total mass, centre of mass and moment of inertia are all derived from the grid, then the whole thing is one 2-D rigid body: accel = engine_force / mass (heavy rig crawls), turn rate = steer_torque / I (big rig turns lazily), and an engine bolted off the centre-line makes its own yaw torque so an asymmetric build genuinely pulls. Tire grip bleeds away sideways velocity so it tracks its nose like a car, not an air-hockey puck - and the handbrake (hold SPACE) turns that same grip term DOWN so the tail breaks loose and you DRIFT, laying tire marks on the asphalt, then hooks back up on release. The world is COLLIDABLE: an infinite, deterministic, chunk-streamed city of roads, houses, scattered cones and PARKED CARS, all resolved by one number - the contact impulse J vs each object's strength. Run a cone over (it gives, scatters), crash a house (it holds - until a heavy rig at speed smashes through), and shunt a parked car: the car is a REAL rigid body with mass and inertia, so the same impulse knocks and spins it like a billiard ball of steel and glass while kicking back on your rig (a buggy bounces, an 18-wheeler bulldozes), then it slides and spins a long way before settling - only an extreme hit crumples one. Clip any of them off your centre-line and the off-centre impulse spins you. Hit TAB for the BUILD editor: place parts on the grid (frame/engine/wheel/caster/seat) and watch the centre-of-mass crosshair and the mass/top-speed/turn/understeer readouts move LIVE - then flip back and drive what you built. Caster wheels roll but barely grip sideways, so an all-caster rig slides and pivots like a pushed piano. Each rig runs a transmission (cycle G: 1-gear / automatic / manual with a real H-gate, and reverse is a GEAR not the brake) and an ENGINE KIND (cycle K in BUILD): electric (flat, snappy), gas (revvy mid-band), diesel (low-end grunt), steam (spool-up), nuclear (huge, flat) - each is just power + mass + a delivery curve, so they drive AND sound distinct with no special-casing in the drive loop. The engine kind also carries its GEARING, which is what unlocks the extremes: press 9 for a SUPERCAR (RACE V12, tall gears -> 300 km/h) and 0 for a TRUCK (TRACTOR, ultra-short gears -> ~45 km/h of grunt). Press 1-0 plus -,= to load twelve preset rigs as editable templates and FEEL the difference - balanced buggy, heavy hauler, twin-engine sprinter, loose jalopy, darty motorbike, FWD/RWD/4WD drivetrains, the 300 supercar, the 45 km/h tractor-truck, a long lumbering 18-wheeler SEMI and a SCHOOLBUS - all from the same core with zero per-rig tuning. Grip is PER-AXLE (the two-axle model): push too hard and the tyres let go - the front washes wide (understeer), the rear steps out (oversteer), power-on breaks the driven axle loose, and the handbrake cuts the REAR for a drift; stand on the brake at speed and you lock up with skid marks and a screech. TAB build/drive, click to place, Z/UP gas, X/DOWN brake, LEFT/RIGHT steer, SPACE handbrake-drift, Q/E or the H-gate to shift (down at a stop = reverse), I ignition, G transmission, K engine kind, 1-0/-/= templates, R reset. GET OUT OF THE CAR: stop and press F -- you step out of the seat cell as a one-tile person (the seat's occupant, same scale as the rig's parts), walk with arrows/WASD (Z jogs), and the same world pushes back on foot: buildings and parked cars block you, the camera eases in to 1:1, and the GROUND drives your pace - tarmac and the pavement strip beside it walk full speed, grass drags you to ~0.6x (the same road_at() surface seam that drives the rig's grip, so it works identically in the procedural grid and in real OSM Delft). Walk back to your seat and press F to drive again -- on a long rig that means walking to the cab, because entry is the SEAT cell, not 'the vehicle'. A rig with no seat can't be entered, the same rule that makes it drivable. AND WALK INTO A HOUSE: every house has one door on its street-facing wall (found by probing road_at() -- houses face the street because streets are where houses face; a small dark notch marks it while you're on foot). The door is a real hole in the wall: step through it and THAT house's roof lifts off, revealing a deterministic BSP floor plan -- rooms, interior doorways, furniture hugging the corners -- generated from the building's identity, so it's the same house every visit, and guaranteed walkable (the generator re-rolls any plan whose rooms aren't all reachable from the front door). Walls and furniture collide; the camera eases in to 2x while you're under the roof; walk out the door and the roof drops back. Works identically over the procedural grid and real OSM Delft footprints. DRIVE A REAL CITY: drag a .rvb road-data file (e.g. data/delft-centre.rvb, made by data-tools/roadview/osm-roads.js) onto the window to leave the procedural world and drive real OpenStreetMap streets — the road surface drives handling (grip drops the moment you slide off the tarmac) and the real building footprints are solid obstacles you crash into; O reveals the data folder. Same runtime data hooks as roadview; swap the file, drive a different city. AND DRIVE THE INFINITE PROCEDURAL SPINE (worldgen rung 1): press N and the stub grid swaps for roadnet2's deterministic world (runtime/worldnet.h — real terrain, ranked towns and cities, terrain-aware spline highways with bridges), the rig dropped onto the nearest road facing along it. The SAME road_at() seam now reads the same edge graph roadnet2 itself queries — one data model — so grip drops the moment you slide off the tarmac, in an endless world where the same seed is the same world everywhere. No buildings out there yet; that's the next rung. AND DRIVE A GENERATED CITY (worldgen rung 5.5): press M and the world becomes a city grown by runtime/citygen.h — the calibrated worldgen grammar (a population-density field, per-city tensor-field arterials, and district minor-street fill, all tuned until the street network's statistics MATCH a real Dutch city, Rotterdam). The rig drops onto a generated street facing along it, and the SAME road_at() seam reads citygen_road_at, so the moment you turn off a street onto the grass the grip drops exactly like it does over real Delft — one seam, now three producers (stub grid, real OSM, the roadnet2 spine, and the generated city). Same seed = same city."
}
de:meta */
#include "studio.h"
#include "worldnet.h" // Rung A1 (worldgen-plan rung 1): the shared WORLD SPINE — roadnet2's
                      // terrain + road graph + wn_road_at(), behind this cart's road_at() seam
#include "citygen.h"  // Rung 5.5c: the calibrated CITY grammar — density + arterials + minor
                      // streets + citygen_road_at(), behind the same road_at() seam (press M)
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>   // malloc/calloc/free — the OSM spatial index (Rung B)
#include <string.h>   // memcpy/memcmp — .rvb binary read (Rung B)
#include <math.h>

// ============================================================================
// SLOOP  —  build a vehicle from parts, drive it across a procedural world.
//           ── MVP rung 1: drive a fixed rig on one biome (road). ──
//
//   ◄ / ►        steer
//   Z / ▲        gas
//   X / ▼        brake / reverse
//   SPACE (hold) handbrake — break the tires loose and DRIFT
//   R            reset
//   O            reveal the data folder (drag a .rvb onto the window → drive that REAL city)
//
// ── the one honest core ──────────────────────────────────────────────────────
// The vehicle is NOT a sprite with stats. It is a GRID OF PARTS, and how it
// drives is COMPUTED from the parts you bolted on. From the grid we derive three
// numbers — total mass, centre of mass (COM), and moment of inertia (I) — and the
// whole rig is then ONE 2-D rigid body:
//
//     accel    = engine_force / mass          (F = ma, no fudge — heavy rig crawls)
//     ang_accel = steer_torque / I            (big heavy rig turns lazily)
//     + an engine bolted off the COM's centre-line produces its OWN yaw torque,
//       so an asymmetric build genuinely pulls to one side. Nothing is faked.
//
// Same DNA as orbit (one integrator, no lies) and coaster (one physics core, two
// modes). This rung proves the DRIVE feels right with a hardcoded symmetric rig;
// rung 2 bolts a BUILD-grid editor onto this exact core (see docs/design/sloop.md).
// The larger procedural WORLD this rig is meant to drive lives in roadnet
// (tools/carts/roadnet.c, docs/design/roadnet.md + roadnet-handoff.md): roadnet's
// rung 4 = wire road_at() into sloop. sloop's own world here is the in-cart test rig.
//
// ── why it feels like a car and not an air-hockey puck ───────────────────────
// The trap with torque-only steering is ice-skating. The cure is TIRE GRIP: each
// frame we split velocity into forward + lateral components and bleed the lateral
// one away (LAT_GRIP). Turning the heading then leaves the old velocity pointing
// "sideways", grip kills it, and the car tracks its nose. More wheels / better
// ground = more grip = less slide. That single line is the difference.
//
// ── rung 1.5: drifting (the steer.c handbrake, on this rig) ──────────────────
// Drift is just the same line with the grip turned DOWN. Hold the handbrake and
// lateral grip drops to DRIFT_GRIP_MULT of normal: the back end stops killing its
// sideways velocity, so the nose rotates while momentum keeps carrying you the old
// way — the car points one direction and slides another. Release and grip snaps
// back, the slide is killed, and you shoot off along the nose (the drift exit).
// Tires lay marks on the asphalt exactly while the lateral slide exceeds SKID_SLIP,
// same trigger steer.c uses. Honest: it's the one grip term, scaled.
//
// ── rung 2.5: unsupported cells scrape (wheels become spatial) ───────────────
// Wheels aren't just a grip number — they hold the chassis off the ground. A cell
// that hangs OUTSIDE the bounding span of the wheels (cantilevered past them) drags:
// extra drag + a lateral anchor + an off-centre yaw, with sparks, heat, and a grind.
// Cells *between* the wheels are carried by the wheelbase (you don't need a wheel
// under every cell). So losing a wheel makes the rig scrape, and wheel-spam costs you
// mass + drag. The sparks/heat are metal-on-tarmac — offroad becomes a furrow (rung 3).
//
// ── rung 2.55: dynamic tipping (the build tips when you corner) ──────────────
// 2.5's scrape is STATIC — a cell always on the floor. This is DYNAMIC: cornering
// load shifts the COM toward the turn's OUTSIDE, and if it leaves the SUPPORT
// POLYGON (the convex hull of the wheels) the rig tips onto an unsupported corner —
// the lateral grip collapses (it pushes wide / breaks loose) and the corner scrapes.
// A 4-wheeler's quad rarely tips; a 3-wheeler's triangle tips toward its missing
// corner but corners clean the other way (asymmetric, emergent from the geometry).
// A single-track rig (<3 wheels, a bike) is exempt — we don't model lean, it balances.
//
// ── rung 2.6: drivetrain — where power hits the ground (FWD vs RWD) ──────────
// The engine lays its power down THROUGH the DRIVE wheels (the orange-hub part),
// at their position. The wheelbarrow rule: a traction point AHEAD of the COM in the
// travel direction PULLS the rig (tracks straight, stable, understeer); BEHIND it
// PUSHES (the heavy end wants to swing round → loose, oversteer, spin). Reversing
// flips which end leads — so a rear-wheel-only rig genuinely drives better backwards.
// Place drive wheels at the front = FWD, rear = RWD, both = AWD. No drive wheels = the
// engine powers all wheels (AWD), so the presets are unchanged.
//
// ── rung 2.7: transmission & gears ───────────────────────────────────────────
// A gearbox sits between engine and wheels. A gear ratio sets the engine RPM
// (rpm = speed·ratio/V_REF) and multiplies torque: low gear = grunt + low top,
// tall gear = speed. powerband(rpm) is the torque curve (cut past redline). Cycle
// transmission with G: 1-GEAR (direct drive, the electric feel) / AUTO (auto-shifts
// in band) / MANUAL (Q/E shift — over-rev or lug if you pick wrong). Reverse is a
// GEAR (Q at a stop), so the brake (X) is pure deceleration. Engine pitch tracks RPM
// and drops on each upshift.
//
// ── engine kinds (§1a): distinct powerband curves + power-to-weight ──────────
// The engine is rig-wide and has a KIND (cycle with K in BUILD): electric / gas /
// diesel / steam / nuclear. A kind is just power + mass + a DELIVERY curve — electric
// flat (snappy off the line), gas revvy (mid-band peak), diesel low-end grunt, steam
// spool-up (a boiler that builds over ~4 s), nuclear huge + flat. delivery(kind,rpm)
// is the only kind-aware line in the drive loop; power sets top speed (= thrust/drag,
// so the strong kinds raise the ceiling) and power÷mass sets acceleration. Nothing faked.
// ============================================================================

// ── part vocabulary ──────────────────────────────────────────────────────────
// Addressed by enum, never raw index (CLAUDE.md data-driven rule). Each kind:
// mass, engine power, wheel grip, colour.
enum { P_NONE, P_FRAME, P_ENGINE, P_WHEEL, P_CASTER, P_SEAT, P_DRIVE, P_CARGO, P_KINDS };
// grip = lateral (nose-tracking) grip; roll = rolling support (traction + can-move).
// A fixed WHEEL has both; a CASTER rolls (support) but barely grips sideways — it
// swivels, so the rig slides any way and won't track its nose (the piano-dolly feel).
// A DRIVE wheel is a fixed wheel that ALSO receives the engine's power (a powered
// axle): the engine lays its thrust down THROUGH the drive wheels, at their position.
// So WHERE you put them sets the drivetrain — front = pull (FWD, stable/understeer),
// rear = push (RWD, tail-happy/oversteer). No drive wheels placed = the engine powers
// all wheels (AWD), so the presets are unchanged. `drive` flags the kind.
typedef struct { float mass, power, grip, roll, drive; int col; const char *name; } PartKind;
static PartKind KIND[P_KINDS];   // filled in init() (avoid designated inits for libtcc)

// ── engine kinds (§1a) — rig-wide, cycled in BUILD with K ─────────────────────
// The same honest rule as everything else: an engine is just POWER + MASS + a
// DELIVERY curve (how torque comes on with revs — see delivery() below). Picking a
// kind only changes those numbers; the felt differences (snappy electric, revvy gas,
// diesel grunt, steam spool-up, nuclear shove) fall straight out of delivery(kind,rpm)
// and the EXISTING thrust/mass math — there is NO per-kind branch in the drive loop.
// Engines still SUM and share one rig-wide kind (hybrids / per-engine kinds are parked
// to rung 6). `power` also sets TOP SPEED (= thrust/drag): the strong kinds out-thrust
// the old generic engine, which is what raises the 112 km/h ceiling — and power÷mass
// (power-to-weight) sets ACCELERATION. So a kind change moves both, honestly.
// The last two are the EXTREME demos (beyond §1a): a RACE powertrain (huge power, TALL
// gearing, light → ~300 km/h) and a TRACTOR (grunt + ULTRA-SHORT gearing → geared out
// at ~45 km/h). They prove how wide the honest range is. The trick that lets them exist
// without a global re-tune: GEARING is part of the engine kind (`vref` below) — a real
// powertrain differs exactly this way. A tall vref pushes the top-gear redline (= the hard
// speed ceiling) out to 300; a tiny vref pulls it in to 45. No extra gears in the H-gate.
enum { EK_ELECTRIC, EK_GAS, EK_DIESEL, EK_STEAM, EK_NUCLEAR, EK_RACE, EK_TRACTOR, EK_KINDS };
typedef struct { float power, mass, vref; int deftrans, col; const char *name, *feel; } EngineSpec;
static EngineSpec ENG[EK_KINDS];   // filled in init() (no designated inits for libtcc)
static int   eng_kind = EK_GAS;    // rig-wide engine kind (a BUILD setting; persists)
static float boiler;               // steam ONLY: boiler pressure 0..1 — the spool-up state, the
                                   // one kind that needs a per-rig variable (builds while running)

// ── the rig: a grid of parts ─────────────────────────────────────────────────
// Rung 1 had one hardcoded layout; here we toggle between a few PRESET rigs (1-4)
// to FEEL how the derived physics changes with the build — orbit's playbook (its
// 1/2/3 rockets) before the parts-bin builder (rung 2). Same drive core, no tuning
// per rig: every difference below is purely the mass / COM / I / grip falling out
// of where the parts sit.
#define GW   9                     // max grid footprint (rigs pad unused cells with P_NONE). 9 long
                                   // so LONG vehicles fit — the SEMI (18-wheeler) and SCHOOLBUS use
                                   // the full length; shorter rigs just leave the extra cells empty.
#define GH   3
#define CELL 7.0f                  // world px per cell
static int grid[GH][GW];

#define NDES 12
static const int DESIGNS[NDES][GH][GW] = {
    { // 0 BUGGY — balanced 4-wheeler, engine centred: drives clean
        { P_WHEEL, P_FRAME, P_FRAME,  P_WHEEL, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_WHEEL, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 1 HAULER — long & heavy, one engine: crawls, turns lazily (big mass + I)
        { P_WHEEL, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_FRAME, P_FRAME, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE },
    },
    { // 2 SPRINTER — light, TWIN engine centred: huge accel, snappy
        { P_WHEEL,  P_FRAME,  P_FRAME,  P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_SEAT,   P_ENGINE, P_ENGINE, P_FRAME, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL,  P_FRAME,  P_FRAME,  P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 3 JALOPY — 3 wheels + off-centre engine: loose, pulls, slides
        { P_WHEEL, P_FRAME, P_ENGINE, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_FRAME,  P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_WHEEL,  P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 4 MOTORBIKE — narrow inline 2-wheeler: feather-light, darty, twitchy
        { P_NONE,  P_NONE,   P_NONE, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_ENGINE, P_SEAT, P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_NONE,  P_NONE,   P_NONE, P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 5 FWD — drive wheels at the FRONT (c3): front pulls → planted, understeers
        { P_WHEEL, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 6 RWD — drive wheels at the REAR (c0), rear engine: rear pushes → tail-happy
        { P_DRIVE, P_FRAME,  P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_ENGINE, P_SEAT,  P_FRAME, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_DRIVE, P_FRAME,  P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 7 4WD — drive wheels at all four corners: power everywhere → grippy, neutral
        { P_DRIVE, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_DRIVE, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 8 SUPERCAR — low (2-row → slippery), light, rear-drive + the RACE engine → ~300 km/h
        { P_DRIVE, P_FRAME, P_ENGINE, P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_DRIVE, P_FRAME, P_SEAT,   P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE, P_NONE },
        { P_NONE,  P_NONE,  P_NONE,   P_NONE,  P_NONE,  P_NONE, P_NONE, P_NONE, P_NONE },
    },
    { // 9 TRUCK — heavy, wide, 6 wheels + the TRACTOR engine (short gears) → ~45 km/h grunt
        { P_DRIVE, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_DRIVE, P_NONE, P_NONE, P_NONE },
        { P_DRIVE, P_SEAT,  P_ENGINE, P_FRAME, P_FRAME, P_DRIVE, P_NONE, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_WHEEL, P_NONE, P_NONE, P_NONE },
    },
    { // 10 SEMI — full-length 18-wheeler: cab front-right, long trailer, tandem axles. DIESEL,
      //    huge mass + wheelbase → very slow to wind up and a massive lazy turning circle.
        { P_WHEEL, P_WHEEL, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_DRIVE, P_FRAME, P_WHEEL },
        { P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_ENGINE, P_SEAT },
        { P_WHEEL, P_WHEEL, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_DRIVE, P_FRAME, P_WHEEL },
    },
    { // 11 SCHOOLBUS — long boxy body, front engine, rear-drive, 2 axles (front+rear). DIESEL;
      //    long + heavy so it leans into corners and lumbers, but only two axles (not the semi).
        { P_DRIVE, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_WHEEL },
        { P_FRAME, P_FRAME, P_SEAT,  P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_ENGINE, P_FRAME },
        { P_DRIVE, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_FRAME, P_WHEEL },
    },
};
// the engine kind each preset loads with — a template is a WHOLE vehicle (body + engine), so
// loading one always sets a sensible engine (you can still swap it after with K in BUILD). The
// eight handling demos take the everyday GAS engine; SUPERCAR/TRUCK carry the extreme powertrains.
// (-1 would mean "keep the current kind", which surprised people — a V12 stuck on the buggy.)
static const int DES_ENGINE[NDES] = {
    EK_GAS, EK_GAS, EK_GAS, EK_GAS, EK_GAS, EK_GAS, EK_GAS, EK_GAS, EK_RACE, EK_TRACTOR,
    EK_DIESEL, EK_DIESEL,   // SEMI + SCHOOLBUS: diesel haulers (grunt, ~road-speed top, but heavy → slow)
};
static const char *DES_NAME[NDES] = {
    "BUGGY \x07 balanced",
    "HAULER \x07 heavy, sluggish",
    "SPRINTER \x07 twin-engine, fast",
    "JALOPY \x07 off-centre, loose",
    "MOTORBIKE \x07 narrow, darty",
    "FWD \x07 front-drive, planted",
    "RWD \x07 rear-drive, tail-happy",
    "4WD \x07 all-drive, grippy",
    "SUPERCAR \x07 RACE V12, ~300 km/h",
    "TRUCK \x07 TRACTOR grunt, ~45 km/h",
    "SEMI \x07 18-wheeler, lumbering",
    "SCHOOLBUS \x07 long, boxy diesel",
};
static int cur_des = 0;

// derived body properties (recomputed when the build changes)
static float M;                   // total mass
static float comX, comY;          // centre of mass, in local grid px
static float I;                   // moment of inertia about the COM
static float wheelGrip;           // Σ lateral grip (nose-tracking; casters add ~none)
static float wheelRoll;           // Σ rolling support (traction + can-move; casters count)
static float frontX;              // local-x of the rig's front edge (for the nose marker)
static int   nEngines, nWheels;   // for the readout
static int   nDrive;              // powered wheels; 0 = engine falls back to all wheels (AWD)
static float driveX;              // local-x of the drive point (where thrust is laid down)
static float driveRoll;           // Σ rolling support of the DRIVE wheels (their traction cap)
static int   frontalCells;        // rig height across the direction of travel (aero profile)
static float balance;             // COM vs wheelbase: +1 front-heavy (understeer) .. -1 rear-heavy
// rig footprint (COM-local px) — the oriented box the world collides against (§9). +x = forward.
static float rigL0, rigL1;        // extent along local +x (rear edge .. front edge)
static float rigW0, rigW1;        // extent along local +y (the rig's right .. left)
static float rigReach;            // COM → farthest cell corner (collision broad phase)
// ── per-axle grip (the two-axle / "bicycle" model — rung handling-depth) ───────
// Lateral grip split FRONT vs REAR of the COM, so each axle can let go on its own:
// front lets go → understeer (push wide), rear lets go → oversteer (tail steps out /
// spin). frontAxleX/rearAxleX are the mean x of the wheels on each side of the COM;
// aF/aR are their lever arms (aF>0 ahead, aR<0 behind). rearDriveFrac = how much of the
// drive is at the rear (the friction-circle "power eats rear grip" → power-on oversteer).
// twoAxle is false for a single-axle/degenerate layout → fall back to one body bleed.
static float frontGrip, rearGrip; // Σ lateral grip ahead of / behind the COM
static float aF, aR;              // front/rear axle lever arms (px from COM; aF>0, aR<0)
static float rearDriveFrac;       // fraction of drive roll on the rear axle (0..1)
static int   twoAxle;             // 1 = front and rear both have grip at distinct x
// ── ground contact: which cells hang off the ground (no wheel under them) ──────
// A cell scrapes only if it sits OUTSIDE the span of the support points (wheels +
// casters) — i.e. cantilevered out past the outermost wheels. Cells *between* the
// wheels are held up by the wheelbase; you don't need a wheel under every cell.
static int dragging[GH][GW];      // 1 = this occupied cell hangs past the wheel span
static int nDrag;                 // how many cells are scraping the floor

// ── dynamic stability: the support polygon (convex hull of the wheels) ─────────
// 2.5's scrape is STATIC (a cell permanently off the ground). Tipping is DYNAMIC:
// under cornering load the COM shifts toward the OUTSIDE of the turn, and if it
// leaves the support polygon the rig tips onto an unsupported corner → transient
// scrape + lateral grip collapse. A 4-wheeler has a roomy quad; a 3-wheeler has a
// triangle with a short edge, so it tips turning toward the missing corner but not
// the other way. <3 non-collinear wheels = single-track (a bike) — assumed to
// balance (we don't model lean), so it's exempt. Hull stored in COM-local px.
#define MAXHULL 20
static float hullX[MAXHULL], hullY[MAXHULL];
static int   nHull;
static float stabL, stabR;        // lateral reach of the hull from the COM (left/right) — BUILD readout

// ── per-wheel contact points (the §8 spring model) ────────────────────────────
// Every support point (wheel/caster/drive), its COM-local position, grid cell (for the
// BUILD draw), drive flag, and solved vertical LOAD. Filled in recompute_body; the load
// is solved by solve_wheel_loads() — statically determinate for any count via the springs.
#define MAXWP (GH * GW)
static float wheelPX[MAXWP], wheelPY[MAXWP];   // contact positions, COM-local px
static int   wheelPR[MAXWP], wheelPC[MAXWP];   // their grid row/col
static int   wheelPD[MAXWP];                   // 1 = a drive wheel
static float wheelG[MAXWP];                    // lateral grip coefficient (wheel 1.0, caster 0.12)
static float wheelLoad[MAXWP];                 // solved vertical load (mass units; Σ ≈ M)
static int   nWheelP;

// ╔══ FEEL TUNING — START HERE (handling / drift / camera) ════════════════════════════════╗
// Want to change how it FEELS? Find the goal, tweak the named #define(s) (all live below in their
// labelled blocks). Engine power / top speed / gearing live separately — see "ENGINE TUNING" in init().
//   • PUNCHIER / LOOSER DRIFT  → SLIP_MAX ↓ (tyres let go sooner) · SELF_ALIGN_K ↓ (less auto-straighten)
//                                · DRIFT_RECOVER ↑ / DRIFT_DECAY ↓ (longer tail-out hang)
//   • MORE GRIP / HARDER TO SLIDE → SLIP_MAX ↑ · SELF_ALIGN_K ↑
//   • MORE / LESS WHEELSPIN     → MU_TRACTION ↓/↑ (lower = breaks traction more easily)
//   • HANDBRAKE bite           → DRIFT_GRIP_MULT (lower = tail snaps out harder)
//   • STEERING quick vs lazy   → STEER_RATE (wind-on) · STEER_RETURN (re-centre) · STEER_RESP (authority)
//   • YAW calm vs darty        → GRIP_YAW_K · ANG_DAMP_AXLE
//   • WEIGHT-TRANSFER strength → WT_LONG_K · WHEEL_H (dive / squat / tip sensitivity)
//   • SENSE OF SPEED           → CAM_ZOOM_PULL (pull-back) · CAM_LEAD (look-ahead)
//   • BRAKING power / coast    → BRAKE · ROLL_FRIC
// ╚════════════════════════════════════════════════════════════════════════════════════════╝
// ── tuning ───────────────────────────────────────────────────────────────────
// ENGINE_POWER is the GAS baseline (the everyday engine); the other kinds scale off it
// in the ENG[] table — `power` is now per engine-kind (§1a), not one global. The WHOLE
// force budget (this + the drags + ROLL_FRIC) was scaled ~0.45× together vs an early cut
// so acceleration is realistic (gas now ~8 s to 100 km/h) and you dwell in gears 1-3
// instead of blasting through them. Raising this (and the strong kinds above it) lifts
// top speed — top = thrust/drag, so more thrust = a higher ceiling (the engine-focus fix
// for the slightly-low 112 km/h). Power-to-weight (power÷mass per kind) sets accel.
#define ENGINE_POWER  600.0f      // forward force per GAS engine (other kinds scale in ENG[])
// Drag is a FORCE (DDA's model): top speed = thrust / drag, MASS-INDEPENDENT — mass
// sets acceleration, not top speed. Drag = base + per-wheel rolling resistance + a
// frontal-profile aero term, so SHAPE and WHEEL COUNT set top speed, not just weight.
// Drag set LOW so top speed is high (~2x): the launch stays punchy (accel = thrust/M,
// drag negligible at low speed) but the rig keeps PULLING to a far higher top — which
// both stretches the gears into a real progression (off in 1st, build 2nd, cruise 3/4)
// and makes the world rip by (sense of speed). The speed-dependent handling below
// (V_REF, STAB_H, SKID_SLIP…) is scaled to match.
#define DRAG_BASE     0.27f       // baseline drag (force per px/s) — scaled with the force budget
#define DRAG_WHEEL    0.113f      // lever: each wheel adds rolling resistance (grip↑, top speed↓)
#define DRAG_AERO     0.27f       // lever: drag per cell of frontal profile (narrow = fast)
#define BRAKE         560.0f      // max braking decel (px/s^2) — real brakes >> engine accel;
                                  // capped per-rig by tyre grip below (GRIP_TO_FORCE·grip/M),
                                  // so MORE/BETTER WHEELS = harder stops (an under-wheeled rig
                                  // can't haul up as fast). At slow speed this stops you dead.
#define ROLL_FRIC     7.2f        // CONSTANT rolling/bearing friction (px/s^2) — what actually
                                  // STOPS a coasting rig. Drag ∝ v only asymptotes to 0 (floaty);
                                  // this constant term dominates at low speed and snaps v to rest.
                                  // Scaled with the force budget (16→7.2); a touch longer coast,
                                  // but ANY positive value still kills the asymptote → a real stop.
// ── idle creep: engine idles in gear → the car trundles with the throttle released ──
// Real manual idle-in-gear (clutch out, no gas): ~6 km/h in 1st (≈4.4 mph/1000rpm × 800rpm
// idle ≈ 3.5 mph), and creep ∝ idle_rpm/ratio so it tracks the gear. IDLE_CREEP is anchored
// to that real 1st-gear figure (~6 km/h ≈ 8.3 px/s × ratio 3.5), the rest fall out by 1/ratio:
// 1st ~6, 2nd ~10, 3rd ~15, 4th ~22, 5th ~29 km/h. (Real manuals stall in tall gears at idle;
// our forgiving no-stall model just lets the creep get faster — see §1b.)
#define IDLE_CREEP    29.0f       // creep "speed×ratio": idle holds vf ≈ IDLE_CREEP / ratio
#define CREEP_ACCEL   55.0f       // how briskly idle eases you onto the creep speed (px/s^2)
// ── transmission & gears (§1b) ───────────────────────────────────────────────
// A gear ratio multiplies torque and sets the engine RPM = speed·ratio/V_REF. Low
// gear = high ratio = lots of thrust but revs climb fast (low top speed); high gear
// = the reverse. powerband(rpm) is the torque curve: weak at idle (lug), peak mid,
// cut past redline. SINGLE = one flat direct-drive ratio (the electric/EV feel, no
// band to manage). Reverse is a GEAR (0), so the brake is pure deceleration.
enum { TR_SINGLE, TR_AUTO, TR_MANUAL };
#define NGEAR         5           // forward gears (AUTO/MANUAL)
#define V_REF         135.0f      // DEFAULT gearing: speed (px/s) where a ratio-1.0 gear hits
                                  // redline (absorbs the real final-drive ~3.6 + wheel size +
                                  // px↔world units). Now a per-engine-kind value (ENG[].vref) —
                                  // this is the gearing of the normal kinds; RACE goes much taller
                                  // (→300 km/h), TRACTOR much shorter (→45). top-gear redline =
                                  // vref / 0.72, which is the rig's hard top-speed ceiling.
#define REV_RATIO     3.50f       // reverse ≈ 1st gear (real gearboxes share the ratio)
#define REV_ENGAGE_SPD 12.0f      // shift into/out of reverse only below this (px/s) — ABOVE 1st-gear
                                  // idle creep (~8.3) so a stopped-but-creeping car still qualifies
#define AUTO_BRAKE_REVERSE 1      // 1 = (arcade, forgiving) in AUTO/1-GEAR, holding BRAKE past a dead
                                  // stop engages reverse AND drives you back — so you don't have to
                                  // change gear to wriggle out of a jam (GAS at a stop returns to DRIVE).
                                  // 0 = (realistic) BRAKE is pure deceleration; you select R yourself
                                  // (the gate / a down-tap). MANUAL is ALWAYS realistic, toggle or not.
#define SINGLE_RATIO  0.95f       // the one direct-drive ratio (electric): flat, strong, no band
#define AUTO_UP       0.90f       // AUTO upshifts above this rpm (revs high → you HEAR the gear)
#define AUTO_DOWN     0.42f       // AUTO downshifts below this rpm
// ── ignition + stall ("uitvallen", §1b) ──────────────────────────────────────
// The engine has an on/off state (I to crank). Stall = lugging a too-tall gear
// below idle revs while STILL ROLLING. The threshold falls out of the gear math:
// idle creep holds rpm at IDLE_CREEP/V_REF ≈ 0.21 in EVERY gear (vcreep·ratio/V_REF
// cancels the ratio), so a launch or a dead-stop idle never dips under STALL_RPM —
// only braking/coasting a tall gear down past idle does. AUTO downshifts at 0.42 so
// it's naturally stall-proof (like a real automatic); the bite lands in MANUAL.
#define STALL_RPM     0.12f       // below this (while rolling) the engine cuts out
#define VSTALL_MIN    8.0f        // ... but only above this speed (px/s) — protects idle/launch
#define RESTART_GRACE 0.5f        // s of stall-immunity after a crank (starter catches; downshift)
#define BOILER_RISE   0.26f       // steam: boiler pressure gained/s while running (~4 s to full spool)
#define BOILER_FALL   0.12f       // ... and bled/s when the engine's off (slower than it builds)
#define LAT_GRIP      32.0f       // lateral velocity killed per second (tire grip)
#define STEER_RESP    680.0f      // steering authority (deg/s^2) at speed
// ── steering ramp: digital keys → an analog steer ANGLE (the counter-steer fix) ──
// The input is binary (full-left / centre / full-right), which can't trim a drift. So the
// physics steers off a smoothed `steer_pos` that RAMPS toward the held direction and eases
// back when released: hold = winds to full lock, a quick opposite tap = backs it off a notch
// (fine counter-steer from digital input). The on-screen wheel mirrors steer_pos.
#define STEER_RATE    3.4f        // how fast steer_pos winds toward full lock while you hold (per s)
#define STEER_RETURN  5.0f        // how fast it self-centres when you release (per s)
// gas + brake are RAMPED the same way (analog feel from digital keys) — but tuned FAST, since mass +
// gears already grade acceleration: flooring stays punchy, only the instant-100% spike is gone. The
// analog middle unlocks FEATHERING (ease the gas just under the grip limit → max launch, no wheelspin)
// and TRAIL-BRAKING (a light brake into a corner rotates the rig via the friction circle).
#define THROTTLE_RATE 6.0f        // gas winds to full in ~0.17 s held
#define THROTTLE_RETURN 7.0f      // eases off on release (a natural engine-braking taper)
#define BRAKE_RATE    9.0f        // brake bites fast (responsive) ...
#define BRAKE_RETURN  12.0f       // ... and releases fast
#define ANG_DAMP      5.0f        // angular self-centering (1/s) for SINGLE-TRACK / fallback rigs
                                  // (no per-axle couple to lean on, so they keep the original value)
#define ANG_DAMP_AXLE 2.6f        // self-centering for TWO-AXLE rigs: lower, because the per-axle
                                  // grip couple (GRIP_YAW_K) supplies the rest of the yaw damping
                                  // PHYSICALLY (∝ grip·wheelbase²/I) — long rig calm, short one darty.
                                  // ANG_DAMP_AXLE + the couple ≈ the old ~5/s on the buggy.
#define GRIP_YAW_K    0.26f       // scales the yaw couple from the two front/rear grip forces (the
                                  // per-rig damping spread + the source of emergent under/oversteer)
#define REF_GYRO      130.0f      // gyradius^2 (px^2) a "normal" rig turns easily at
#define ENG_YAW_K     0.9f        // how hard an off-centre engine yaws the rig
#define BALANCE_K     0.4f        // lever: front-heavy understeers, rear-heavy oversteers
#define GRIP_TO_FORCE 2000.0f     // wheel grip → max traction force
#define MU_TRACTION   100.0f      // §8 load-sensitive traction: drive-wheel LOAD → max laid-down force.
                                  // SET LOW ENOUGH THAT GRIP BINDS: flooring (esp. low gear = high thrust)
                                  // exceeds it → wheels spin. Worst in 1st, hooks up as you upshift; a
                                  // heavy/cargo'd or few-drive-wheel rig spins more. Tune by feel.
#define SPIN_SQUEAL   0.12f       // wheelspin amount above which tyres squeal + lay burnout marks + smoke
#define GROUND_GRIP   1.0f        // road: plenty. Off-road is now the OSM surface (Rung B step 5): the
                                  // GROUND_GRIP term is scaled per-frame by surf_grip below.
#define OFFROAD_GRIP  0.55f       // OSM off-road: grip drops → slides wide, understeers, longer to stop
#define OFFROAD_DRAG  0.4f        // OSM off-road: extra rolling drag per wheel → rough ground bleeds
                                  // top speed to ~half (top = thrust/drag). Tune by feel with OFFROAD_GRIP.
static float surf_grip = 1.0f;    // per-frame surface grip (1 = on road, OFFROAD_GRIP = off). OSM-only.
#define KMH           0.72f       // px/s → km/h for the readout (top ~166 px/s ≈ 120 km/h,
                                  // so the SPEED number lines up with the zone limit signs)
#define DRIFT_GRIP_MULT 0.13f     // handbrake: REAR axle's grip limit drops to this fraction → tail out
#define SKID_SLIP     28.0f       // lateral speed (px/s) where tires start marking (sideways scrub)
#define BRAKE_SKID_SPD 70.0f      // forward speed (px/s) above which STANDING ON THE BRAKE locks the
                                  // tyres → straight skid marks + a screech (only on a hard, fast stop)
// ── friction circle (per-axle let-go — "uit de bocht vliegen") ────────────────
// A tyre converts lateral slip into grip only up to SLIP_MAX of slip velocity; beyond that
// the force saturates — it has LET GO and slides. Front lets go → understeer (push wide);
// rear lets go → oversteer (tail steps out / spin). Each wheel's lateral budget ALSO shrinks
// with the longitudinal force it's carrying (combined slip, the latFactor below) — so flooring
// a rear-drive rig mid-corner, or braking into one, breaks the tyres loose. Emergent.
#define SLIP_MAX      36.0f       // lateral slip velocity (px/s) a tyre holds before it lets go
// ── drift-exit hysteresis: the rear stays loose for a beat after a slide ───────
// Grip recovers INSTANTLY today (the cap is recomputed each frame), so a drift ends crisply
// the moment you straighten. This lingers a "looseness" that spikes when the rear breaks away
// and bleeds off slowly — while it bleeds, the rear keeps a touch less grip, so the tail HANGS
// out a beat and settles (the satisfying trailing exit) instead of snapping straight.
#define DRIFT_RECOVER 0.45f       // how much the lingering looseness cuts rear grip (at full memory)
#define DRIFT_DECAY   0.05f       // per-frame bleed of the looseness (lower = longer, lazier tail-out)
#define DRIFT_TRIG    0.5f        // rear-slide level that COUNTS as a breakaway (below = normal hard
                                  // cornering, no lingering; the looseness ramps from here up to a full slide)
// ── weight transfer (dynamic load shift — the engine of a controllable drift) ──
// Static weight DISTRIBUTION (where the part masses sit → COM/I/balance/frontGrip/rearGrip)
// already decides the rig's character. This is the DYNAMIC half: the load on each axle
// shifts in real time as longitudinal forces act on that mass. BRAKING (decel) pitches load
// onto the FRONT axle (nose dives → fronts grip harder, rear goes light → lift-off / trail-
// braking rotates the car); THROTTLE (accel) loads the REAR (squat → rear hooks up). The
// shift scales each axle's friction-circle cap, so an unloaded tyre lets go sooner and a
// loaded one holds longer + makes more force. This is what makes a drift CONTROLLABLE:
// ease the throttle → rear unloads → tail steps out; feed it back → rear loads → it settles.
#define WT_LONG_K     0.0011f     // px/s^2 of longitudinal accel → load-shift fraction
#define WT_MAX        0.5f        // cap on the shift (±50% — a tyre never fully unloads in normal driving)
#define WT_FLOOR      0.35f       // min load multiplier on an axle (it always keeps SOME grip)
#define WT_CEIL       1.65f       // max load multiplier on the loaded axle
#define WT_LAG        0.18f       // load-shift low-pass (per frame) — models suspension settle time
// ── per-wheel spring contact model (§8) — the unified core, built in phases ────
// Each wheel is a vertical spring; the chassis heaves+pitches+rolls so per-wheel LOAD
// solves from 3 balances (carry weight, longitudinal moment, lateral moment) — determinate
// for ANY wheel count. WHEEL_H is the COM-height coefficient that turns longitudinal/lateral
// accel into a load-transfer moment (bigger = more transfer). PHASE 1: solve + visualise the
// loads only; forces still come from the 2-axle core until the per-wheel force loop lands.
#define WHEEL_H       0.020f      // COM-height coeff: accel → load-transfer moment (tune by feel)
// ── self-aligning torque (caster / pneumatic trail — what HOLDS a drift) ──────
// Real front tyres, when the car slides, make a torque that rotates the rig toward the
// direction it's actually TRAVELLING — the car counter-steers itself. That's why a drifter
// can hold opposite lock without superhuman reflexes, and it's what arrests a slide into a
// stable angle instead of a spin. We add it as a yaw toward the velocity vector, ∝ the slip
// angle (atan2(vl,vf)) and speed. Gentle at small slip (normal driving barely feels it),
// strong in a big slide → the rear breaks loose, this catches it, and THROTTLE (via the
// friction circle) trims the held angle: the controllable, sustainable, realistic drift.
#define SELF_ALIGN_K  0.18f       // strength of the self-steer toward the travel direction. LOW enough
                                  // that a drift can develop and be HELD; just enough to catch a spin
                                  // and give the player automatic counter-steer help (digital input)
#define SELF_ALIGN_SIN 0.82f      // max slip it corrects, as sin(angle) (~55°); past that it's spinning, let it
// ── ground-scrape: a cell hanging past the wheel span drags on the floor ──────
#define SCRAPE_DRAG   9.0f        // extra drag force per dragging cell (top speed ↓)
#define SCRAPE_LAT    7.0f        // extra lateral resistance per dragging cell (anchors sideways)
#define SCRAPE_YAW    160.0f      // an off-centre scrape twists the rig (deg/s^2 per cell·unit)
#define SCRAPE_MINSPD 18.0f       // below this speed nothing scrapes (kinetic friction only when moving)
#define HEAT_RISE     1.4f        // heat gained per second per dragging cell while moving
#define HEAT_COOL     0.8f        // heat lost per second when not scraping
// ── dynamic stability (tipping under cornering load) ─────────────────────────
#define STAB_H        0.013f      // COM-height stand-in: lateral-g → COM load-shift (px per px/s^2)
#define STAB_GRIP_LOSS 0.85f      // fraction of lateral grip lost when fully tipped (tires unload)
#define DEG2RAD       0.017453f   // angVel is deg/s; centripetal accel needs rad/s
// ── drivetrain: where power hits the ground sets push/pull directional stability ──
#define STAB_YAW_K    0.42f       // how hard drive-point fore/aft of the COM (de)stabilizes yaw
#define DRIVE_OFF_MAX 12.0f       // clamp the drive-point offset (px) so push can't spin instantly
#define DRIFT_PUSH_FADE 0.85f     // fade the RWD push anti-damping as the slide saturates (0..1 of it
                                  // removed at full slide) so a drift HOLDS instead of winding to a spin

// ── skid marks (laid in world space while the tires scrub) ───────────────────
#define MAXSKID 512
typedef struct { float x, y; int life; } Skid;
static Skid skid[MAXSKID];
static int  skid_head;

// ── sparks (thrown off a scraping cell, in world space) ──────────────────────
#define MAXSPARK 96
typedef struct { float x, y, vx, vy; int life, col; } Spark;
static Spark spark[MAXSPARK];
static int   spark_head;
static float heat;                // 0..1, rises while cells scrape under load, cools off
static float tip_amt;             // 0..1, how hard the rig is tipping this frame (HUD)
static float slide_amt;           // 0..1, how far past the friction limit a tyre is (spin-out feedback)
static int   slide_rear;          // 1 = the REAR let go (oversteer/spin), 0 = front (understeer/push)
static float wheel_spin;          // 0..~1.5 wheelspin: how much engine force exceeded what the drive
                                  // wheels can lay down (eased) — drives squeal + burnout marks + smoke
static float wt_long;             // low-passed longitudinal accel (px/s^2) driving weight transfer
static float wt_xfer;             // load-shift fraction this frame (+ = rear under accel, - = front under braking)
static float drift_loose;         // 0..1 lingering rear looseness (drift-exit hysteresis) — spikes on a
                                  // rear slide, bleeds off slowly so the tail hangs out a beat on exit
// ── transmission state (§1b) ─────────────────────────────────────────────────
static int   trans_mode = TR_AUTO;  // SINGLE / AUTO / MANUAL — player setting, persists
static int   gear = 1;            // -1 = REVERSE, 0 = NEUTRAL (manual only), 1..NGEAR = forward
static float rpm;                 // 0..~1.15 normalised engine revs (tach + sound)
static int   shift_snd;           // 1 = a gear change happened this frame (play a clunk)
static int   engine_on = 1;       // ignition: I cranks/kills it; stall flips it off
static int   stalled;             // engine died from lugging (vs a deliberate key-off) — HUD wording
static float restart_grace;       // s of stall-immunity after a crank (see RESTART_GRACE)

// ── the collidable world (§9): chunked procedural baseline + sparse dirty deltas ──
// The world is infinite + deterministic: hash2(chunk) regenerates the baseline obstacles
// for any chunk, so an untouched chunk costs nothing. Deviations (a cone you ran over and
// shoved) live as sparse per-chunk DELTAS. A LIVE RING of chunks around the camera is
// materialised into `pool[]`; entering a chunk loads it (regenerate baseline + replay its
// delta), leaving evicts it (stash any dirty delta, drop beyond the bound). Bounded
// in-memory now; the evict step is the one line that later becomes save_bytes (forever).
// EVERY obstacle is a CELL-GRID (a cone is 1×1) — the seam that makes the future demolition
// rung (distribute J to tiles, detach over-strength ones) a pure addition, not a rewrite.
#define CHUNK      256            // world px per chunk (bookkeeping grid; independent of road zones)
#define MAXOB      1280           // live obstacles cap (raised for the spawn parking lot's dense car grid)
#define PARKING_CH 2              // spawn PARKING LOT spans chunks |cx|,|cy| ≤ this (≈ a 4×4-chunk, ~1000 px
                                  // square of cars and NO houses at the start — the billiard playground)
#define MAXLOADED  64             // live chunks cap
#define MAXDELTA   192            // remembered deltas for evicted chunks (bounded LRU)
#define OB_GW      4              // obstacle cell-grid (cone 1×1, house up to 4×4 — the demolition tiles)
#define OB_GH      4
#define OB_CELL    7.0f           // px per obstacle cell (matches the rig's CELL → tiles read same scale)
#define OB_PERCHUNK 80            // max obstacles a single chunk can generate (a dense city chunk's houses)
#define WRECK_GIVE 0.4f           // a wrecked car keeps 40% of its strength → keeps shoving easily after
enum { OM_NONE, OM_CONE, OM_BRICK, OM_CAR, OM_KINDS };  // cell MATERIALS (mass+strength)
enum { OB_CONE, OB_HOUSE, OB_CAR, OB_KINDS };   // obstacle KINDS (OB_HOUSE = solid building; OB_CAR = movable rigid body)
typedef struct { float mass, strength; int col; const char *name; } ObMat;
static ObMat OM[OM_KINDS];        // filled in init() (no designated inits — libtcc)
typedef struct {
    unsigned char alive, dirty, destroyed;  // dirty = deviates from baseline; destroyed = smashed (persists)
    int   kind;                   // OB_*
    int   cx, cy, idx;            // home chunk + baseline index (the delta key)
    float bx, by;                 // baseline (pristine) position — identity + reset
    float x, y, ang;              // current pose (= baseline + run-over shove)
    float vx, vy, vr;             // knocked velocity (tumbles, then settles back to rest)
    float hw, hh;                 // footprint HALF-extents (world px) — the collision box
    unsigned char gw, gh;         // cell-grid dims (cone 1×1; house N×M demolition tiles)
    unsigned char cell[OB_GH][OB_GW];  // material per cell (OM_*); 0 = empty
    int   col;                    // draw colour (house roof varies; cone = its material colour)
    float mass, strength, rad;    // derived at load (mass/strength from cells; rad from the footprint)
    signed char door;             // F2: street-facing wall (0 +x · 1 -x · 2 +y · 3 -y), -1 = not probed yet
} Obstacle;
static Obstacle pool[MAXOB];      // the live working set
typedef struct { int cx, cy, idx; float dx, dy, dang; unsigned char destroyed; int age; unsigned char used; } Delta;
static Delta wdelta[MAXDELTA];    // remembered deviations of chunks NOT currently live (LRU by age)
static int   wclock;              // monotonically bumped each save → LRU age
typedef struct { int cx, cy; } ChunkRef;
static ChunkRef loaded[MAXLOADED];
static int   nLoaded;
static int   last_hit;            // 1 the frame an obstacle is run over / smashed (HUD/trace pulse)
#define HOUSE_FACADE 38           // px per house plot (~5 m); the drawn house is ~34px ≈ 4.9 m
// forward decls (defined down in the world section, after hash2)
static void collide_world(void);
static void world_sync(void);
static void obstacles_integrate(float dt_);
static void collide_obstacles_chain(void);   // §9f: moving obstacle vs obstacle — the chain reaction
static void draw_obstacles(void);
static void world_reset(void);
static int  zone_at(float x, float y);   // (defined below; gen_chunk needs it to place houses)

// ── P1 SEAM (docs/design/driving-world-program.md, Rung A) — the world's FRONT DOOR. ─────────────
// Today a STUB that reproduces sloop's own grid world (tarmac bands at ZONE_PITCH ± ZONE_LANE/2),
// so behaviour is unchanged. The point: callers ask road_at() instead of inlining the grid math, so
// Rung B can swap the BODY for OSM (real Delft .rvb) or roadnet2 with NO change here. `cls` is a coarse
// hierarchy (the zone, for now). Fields mirror roadnet's RoadHit so the contract survives the swap.
typedef struct { int on_road; int zone; int cls; int on_pave; } RoadHit;   // on_pave (rung F1, appended):
                                             // carriageway OR the ~2m strip beside it — the pavement band
                                             // the avatar walks at full pace; the rig's grip ignores it
static RoadHit road_at(float x, float y);   // (defined below, beside zone_at)
static int osm_loaded;                       // Rung B: 1 once a .rvb is loaded (defined with the OSM block below)
// Rung A1: the ROADNET2 procedural world behind the same seam (worldnet.h). Press N to toggle:
// the rig leaves the stub grid for the infinite deterministic spine — same seed, same world,
// the SAME edge graph roadnet2 queries (one data model). No buildings yet (content rung A2).
static int  rn2_on;                          // 1 = driving the worldnet spine
static void rn2_toggle(void);                // (defined with the rn2 block, below the OSM one)
static int  cg_on;                           // Rung 5.5c: 1 = driving a generated citygen city
static void cg_toggle(void);                 // (defined with the citygen block, below the rn2 one)
// Rung C: OSM building footprints → collidable OB_HOUSE obstacles. Declared here (gen_chunk, above the
// OSM block, emits them); filled by osm_load(). Each is an oriented box (world px + heading) + home chunk.
#define OSM_MAXBLD 60000                     // delft-centre ≈ 5.3k buildings; whole-city sets more
#define OSM_BLDCAP 512                       // max footprint verts read for the oriented-box fit (huge → AABB)
typedef struct { float wx, wy, hw, hh, ang; int cx, cy; } OsmBld;
static OsmBld obld[OSM_MAXBLD];
static int    n_obld;
static char   osm_name[64];                  // dataset name from the .rvb header (shown on the HUD)
static void   osm_load(const char *path);    // (defined with the OSM block below; called on drag-drop + init)

// ── rigid body state (sx,sy = the COM in world space; rotation pivots about it) ─
static float sx, sy;              // world position of the COM
static float vx, vy;              // world velocity
static float ang, angVel;         // heading (deg, 0 = facing +x / east) + spin

static float cam_x, cam_y;
static float lead_x, lead_y;      // low-passed camera lead (smooth, no curb jitter)
static float cam_zoom = 1.0f;     // QUANTIZED speed-zoom actually rendered (snapped to steps so thin
                                  // world lines don't re-rasterize every frame — kills the "breathing")
static float cam_zoom_smooth = 1.0f;  // the continuously-eased accumulator (quantized into cam_zoom)
static int   smooth_mode = 1;  //TT  // A/B (V key): 0 off (quantized zoom-out, today) · 1 smooth zoom-out
                                  // (1:1 render + bilinear downscale, no crawl). TEMP scaffolding for the test.
// ── heading-up camera (rotates so the rig's travel points UP; eased = the "glide") ──
static int   cam_head_up = 0;     // 0 = NORTH-UP (grid aligned, today) · 1 = HEADING-UP (travel up). Toggle: C
static float cam_ang = 0;         // eased camera rotation (deg) actually rendered — lags the heading = the glide
#define CAM_GLIDE     0.09f       // how fast the heading-up camera eases toward your heading per frame.
                                  // LOWER = more glide/lag (a hard drift swings the nose fast while the camera
                                  // trails, so you slide sideways then the world swings round to catch up)
// ── sense-of-speed camera (eased so it never jitters) ─────────────────────────
#define CAM_ZOOM_PULL 0.34f       // how far the camera pulls BACK (zooms out) at full speed
#define CAM_ZOOM_REF  420.0f      // speed (px/s) where the pull-back maxes out — set near the supercar's
                                  // ~300 km/h top (≈417 px/s) so fast rigs zoom out to see further ahead
                                  // while normal rigs (≤~166 px/s) barely change
#define CAM_ZOOM_STEP 0.04f       // QUANTIZE the zoom to this grid — a fractional zoom re-rasterizes
                                  // thin world lines (curbs, dashes) every frame it changes (the
                                  // shimmer/"breathing"); snapping to steps holds them rock-steady
                                  // between levels (5 levels over the pull → a few tiny pops, no crawl)
#define CAM_LEAD      0.34f       // how far ahead the camera leads the rig (world px per px/s of vel)
#define LEAD_MAX      42.0f       // HARD cap on the lead in SCREEN px — guarantees the rig stays on-screen
                                  // (and clear of the dashboard) at ANY speed; uncapped, a 300 km/h rig flew
                                  // ~120 px off-centre and vanished under the dash
static float t_skid_snd, t_scrape_snd, t_spin_snd;
static int   is_paused;
static float wheel_ang;           // eased steering-wheel angle (deg) for the cockpit dial
static float steer_pos;           // ramped steer angle (-1..+1) — analog feel from digital keys
// --- grab-the-wheel steering (touch + mouse): sweep a finger around the hub and the
// rotation IS the steer angle. WHEEL_LOCK of sweep = full lock; release self-centres.
#define WHEEL_NO_GRAB (-999)
#define WHEEL_MOUSE   (-1000)
#define WHEEL_LOCK    90.0f       // finger sweep (deg) for full steering lock — a quarter-turn each way
static int   wheel_grab = WHEEL_NO_GRAB;  // finger id (or WHEEL_MOUSE) turning the wheel; else released
static float wheel_prev_ang;      // pointer angle (deg) last frame — for the incremental sweep
static float wheel_turn;          // accumulated wheel rotation (deg), clamped ±WHEEL_LOCK → steer
static int   steer_grab;          // 1 while the wheel is being turned → drives steer_pos directly
static float gas_pos, brake_pos;  // ramped throttle/brake (0..1) — analog feel from digital keys

// ── modes: BUILD (paused grid editor) ↔ DRIVE (the rig loose in the world) ───
enum { MODE_DRIVE, MODE_BUILD, MODE_FOOT };
static int mode = MODE_DRIVE;
static int sel_part = P_WHEEL;    // palette selection; P_NONE = the eraser

static float af(float v) { return v < 0 ? -v : v; }

// gear ratios — a real Getrag/Muncie 5-speed (3.50/2.05/1.38/0.94/0.72, final drive ~3.6
// folded into V_REF + ENGINE_POWER). 1st is a low torque gear, 4th ≈ direct (1.0), 5th an
// overdrive (<1). Spacing is PROGRESSIVE (big drop 1→2, gears close up top) — exactly the
// real-world "staged" pattern. Each gear's redline speed (V_REF/ratio) still steps up.
static const float GEAR_RATIO[NGEAR] = { 3.50f, 2.05f, 1.38f, 0.94f, 0.72f };

// ── delivery(kind, rpm, boiler): the powerband per engine kind (§1a) ──────────
// Torque ÷ peak-torque vs normalised RPM. ONE function, branched only by kind — the
// whole "how engines handle" story. Each curve is fitted to the real dyno data captured
// in docs/design/sloop.md (the §1a reference table): peak-RPM, idle %, redline %. The
// drive loop calls this once for thrust = power · throttle · delivery · ratio; nothing
// else is kind-aware. Over-revving past redline (a too-low gear) cuts via the limiter.
// (This is the CURVE shape only — each kind's power/mass/gearing/sound live in the
//  "ENGINE TUNING" block in init(); that's the place to start when tuning a kind.)
static float delivery(int kind, float r, float blr) {
    switch (kind) {
    case EK_ELECTRIC:                                // max torque from a standstill, tapering
        if (r > 1.0f) return clamp(0.9f - (r - 1.0f) * 7.0f, 0.0f, 0.9f);   // toward the motor's
        return clamp(1.15f - 0.6f * r, 0.5f, 1.1f);                         // max revs (EV feel)
    case EK_DIESEL: {                                // grunt: peaks LOW (~0.45), broad flat, weak top
        if (r > 1.0f) return clamp(0.7f - (r - 1.0f) * 9.0f, 0.0f, 0.7f);   // (Toyota D-4D dyno:
        if (r < 0.45f) return clamp(0.80f + r * 0.45f, 0.0f, 1.0f);         //  idle ~80%, peak 0.45,
        return clamp(1.0f - (r - 0.45f) * 0.55f, 0.7f, 1.0f);              //  ~70% at redline)
    }
    case EK_STEAM:                                   // spool-up: delivery IS the boiler pressure —
        return clamp(blr, 0.0f, 1.0f) * 0.95f;       // sluggish until it builds, then steady
    case EK_NUCLEAR:                                 // flat + immense, no band to manage — just go
        if (r > 1.0f) return clamp(1.0f - (r - 1.0f) * 6.0f, 0.0f, 1.0f);
        return 1.0f;
    case EK_RACE:                                    // turbo/exotic: early boost, broad flat plateau
        if (r > 1.0f) return clamp(0.92f - (r - 1.0f) * 9.0f, 0.0f, 0.92f);  // (Saab 2.0T dyno feel:
        return clamp(0.62f + r * 1.4f, 0.0f, 1.0f);                          //  spools to a flat top)
    case EK_TRACTOR: {                               // pure grunt: massive low-end, falls off hard up top
        if (r > 1.0f) return clamp(0.6f - (r - 1.0f) * 9.0f, 0.0f, 0.6f);
        if (r < 0.35f) return clamp(0.9f + r * 0.6f, 0.0f, 1.0f);            // peaks very low
        return clamp(1.0f - (r - 0.35f) * 0.7f, 0.55f, 1.0f);               // then droops away
    }
    case EK_GAS:                                     // NA gasoline (Honda 2.0 SI): broad mid peak
    default:                                          //  idle 0.61, PEAK @ 0.66 redline, 0.79 at redline
        if (r > 1.0f) return clamp(0.79f - (r - 1.0f) * 9.0f, 0.0f, 0.79f); // rev limiter
        { float d = r - 0.66f; return clamp(1.0f - 1.82f * d * d, 0.6f, 1.0f); }
    }
}

// ── support-hull geometry (all in COM-local px) ───────────────────────────────
// Build the convex hull of the wheel/caster positions (monotone chain). <3 points
// or all-collinear collapses to a degenerate hull → treated as single-track.
static float cross2(float ox, float oy, float ax, float ay, float bx, float by) {
    return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
}
static void build_hull(float *px, float *py, int n) {
    nHull = 0;
    if (n < 1) return;
    // sort points by x then y (insertion sort — n is tiny)
    for (int i = 1; i < n; i++) {
        float kx = px[i], ky = py[i]; int j = i - 1;
        while (j >= 0 && (px[j] > kx || (px[j] == kx && py[j] > ky))) {
            px[j + 1] = px[j]; py[j + 1] = py[j]; j--;
        }
        px[j + 1] = kx; py[j + 1] = ky;
    }
    float hx[MAXHULL * 2], hy[MAXHULL * 2]; int k = 0;
    for (int i = 0; i < n; i++) {                       // lower hull
        while (k >= 2 && cross2(hx[k-2], hy[k-2], hx[k-1], hy[k-1], px[i], py[i]) <= 0) k--;
        hx[k] = px[i]; hy[k] = py[i]; k++;
    }
    int lower = k + 1;
    for (int i = n - 2; i >= 0; i--) {                  // upper hull
        while (k >= lower && cross2(hx[k-2], hy[k-2], hx[k-1], hy[k-1], px[i], py[i]) <= 0) k--;
        hx[k] = px[i]; hy[k] = py[i]; k++;
    }
    nHull = (k > 1) ? k - 1 : k;                        // last point repeats the first
    if (nHull > MAXHULL) nHull = MAXHULL;
    for (int i = 0; i < nHull; i++) { hullX[i] = hx[i]; hullY[i] = hy[i]; }
}
// signed distance from a local point to the hull: >0 inside (dist to nearest edge),
// <0 outside. <3 hull points = single-track → always "inside" (it balances).
static float hull_margin(float px, float py) {
    if (nHull < 3) return 999.0f;
    float area = 0;
    for (int i = 0; i < nHull; i++) { int j = (i + 1) % nHull; area += hullX[i]*hullY[j] - hullX[j]*hullY[i]; }
    float w = (area >= 0) ? 1.0f : -1.0f;               // winding sign
    float minD = 1e9f;
    for (int i = 0; i < nHull; i++) {
        int j = (i + 1) % nHull;
        float ex = hullX[j] - hullX[i], ey = hullY[j] - hullY[i];
        float len = fsqrt(ex * ex + ey * ey); if (len < 1e-4f) continue;
        float d = w * (ex * (py - hullY[i]) - ey * (px - hullX[i])) / len;
        if (d < minD) minD = d;
    }
    return minD;
}
// how far the COM can shift along the lateral axis (0,dir) before leaving the hull
static float lateral_reach(float dir) {
    if (nHull < 3) return 999.0f;                       // single-track: not a tip risk
    float t = 0;
    while (t < 40.0f && hull_margin(0, t * dir) > 0) t += 0.5f;
    return t;
}

// ── per-wheel spring load solve (§8) ──────────────────────────────────────────
// Treat each contact as a vertical spring; the rigid chassis heaves (z0) and tilts
// (pitch p about lateral, roll r about forward). Each wheel's load = z0 + p·x + r·y.
// Solve (z0,p,r) from 3 balances — carry the weight, match the longitudinal-accel
// moment (pitch → fore/aft transfer), match the lateral-accel moment (roll → left/right):
//     Σ load        = M
//     Σ load · x_i  = −M · aLong · WHEEL_H      (braking loads the front, +x)
//     Σ load · y_i  = −M · aLat  · WHEEL_H      (cornering loads the outside)
// 3×3, determinate for ANY wheel count. A wheel whose load solves NEGATIVE has lifted →
// drop it and re-solve (that clamp IS tipping, emergent). <3 contacts (single-track) or a
// collinear/degenerate layout → even split (the single-track path owns those dynamics).
static void solve_wheel_loads(float aLong, float aLat) {
    int n = nWheelP;
    for (int i = 0; i < n; i++) wheelLoad[i] = 0;
    if (n <= 0) return;
    if (n < 3) { for (int i = 0; i < n; i++) wheelLoad[i] = M / n; return; }
    int active[MAXWP];
    for (int i = 0; i < n; i++) active[i] = 1;
    for (int iter = 0; iter < 3; iter++) {
        float Sn = 0, Sx = 0, Sy = 0, Sxx = 0, Sxy = 0, Syy = 0;
        int cnt = 0;
        for (int i = 0; i < n; i++) {
            if (!active[i]) continue;
            float x = wheelPX[i], y = wheelPY[i];
            Sn += 1; Sx += x; Sy += y; Sxx += x * x; Sxy += x * y; Syy += y * y; cnt++;
        }
        float b0 = M, b1 = -M * aLong * WHEEL_H, b2 = -M * aLat * WHEEL_H;
        float det = Sn * (Sxx * Syy - Sxy * Sxy) - Sx * (Sx * Syy - Sxy * Sy) + Sy * (Sx * Sxy - Sxx * Sy);
        if (cnt < 3 || af(det) < 1e-3f) {            // degenerate (collinear) → even split
            for (int i = 0; i < n; i++) wheelLoad[i] = active[i] ? M / cnt : 0;
            return;
        }
        float z0 = (b0 * (Sxx * Syy - Sxy * Sxy) - Sx * (b1 * Syy - Sxy * b2) + Sy * (b1 * Sxy - Sxx * b2)) / det;
        float p  = (Sn * (b1 * Syy - Sxy * b2) - b0 * (Sx * Syy - Sxy * Sy) + Sy * (Sx * b2 - b1 * Sy)) / det;
        float r  = (Sn * (Sxx * b2 - b1 * Sxy) - Sx * (Sx * b2 - b1 * Sy) + b0 * (Sx * Sxy - Sxx * Sy)) / det;
        int anyNeg = 0;
        for (int i = 0; i < n; i++) {
            if (!active[i]) { wheelLoad[i] = 0; continue; }
            float L = z0 + p * wheelPX[i] + r * wheelPY[i];
            if (L < 0) { active[i] = 0; wheelLoad[i] = 0; anyNeg = 1; }   // this corner lifted (tipping)
            else wheelLoad[i] = L;
        }
        if (!anyNeg) break;
    }
}

// ── derive body properties from the part grid ────────────────────────────────
static void recompute_body(void) {
    M = 0; comX = 0; comY = 0; wheelGrip = 0; wheelRoll = 0; frontX = 0; nEngines = 0; nWheels = 0;
    nDrive = 0; driveRoll = 0;
    int minRow = GH, maxRow = -1;
    float bbX0 = 1e9f, bbX1 = -1e9f, bbY0 = 1e9f, bbY1 = -1e9f;  // occupied-cell bbox (footprint)
    float wMinX = 1e9f, wMaxX = -1e9f;     // wheelbase extent (x of frontmost/rearmost wheel)
    int sMinR = GH, sMaxR = -1, sMinC = GW, sMaxC = -1;  // support span, in CELL INDICES (wheels+casters)
    float supX[GH * GW], supY[GH * GW]; int nSup = 0;    // support positions (world-local px) for the hull
    float driveSumX = 0, allWheelSumX = 0;               // for the drive point (x of powered wheels)
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int p = grid[r][c];
            PartKind *k = &KIND[p];
            if (k->mass <= 0) continue;
            float pm = (p == P_ENGINE) ? ENG[eng_kind].mass : k->mass;   // engine mass = the kind's
            float cx = (c + 0.5f) * CELL, cy = (r + 0.5f) * CELL;
            M += pm; comX += pm * cx; comY += pm * cy;
            wheelGrip += k->grip; wheelRoll += k->roll;
            if ((c + 1) * CELL > frontX) frontX = (c + 1) * CELL;
            if (c * CELL < bbX0) bbX0 = c * CELL; if ((c + 1) * CELL > bbX1) bbX1 = (c + 1) * CELL;
            if (r * CELL < bbY0) bbY0 = r * CELL; if ((r + 1) * CELL > bbY1) bbY1 = (r + 1) * CELL;
            if (r < minRow) minRow = r;
            if (r > maxRow) maxRow = r;
            if (p == P_ENGINE) nEngines++;
            if (k->roll > 0) {                 // a support point (wheel / caster / drive)
                nWheels++;
                wheelPR[nSup] = r; wheelPC[nSup] = c; wheelPD[nSup] = (k->drive > 0);  // for the load solve + BUILD draw
                wheelG[nSup] = k->grip;          // lateral grip coef (per-wheel force loop, §8)
                supX[nSup] = cx; supY[nSup] = cy; nSup++;
                allWheelSumX += cx;
                if (cx < wMinX) wMinX = cx; if (cx > wMaxX) wMaxX = cx;
                if (r < sMinR) sMinR = r; if (r > sMaxR) sMaxR = r;
                if (c < sMinC) sMinC = c; if (c > sMaxC) sMaxC = c;
            }
            if (k->drive > 0) { nDrive++; driveRoll += k->roll; driveSumX += cx; }
        }
    if (M <= 0) M = 1;
    comX /= M; comY /= M;
    // rig footprint as an oriented box about the COM (the world collides against this, §9)
    if (bbX1 < bbX0) { bbX0 = bbX1 = bbY0 = bbY1 = 0; }   // empty grid guard
    rigL0 = bbX0 - comX; rigL1 = bbX1 - comX;
    rigW0 = bbY0 - comY; rigW1 = bbY1 - comY;
    float cx0 = af(rigL0) > af(rigL1) ? af(rigL0) : af(rigL1);
    float cy0 = af(rigW0) > af(rigW1) ? af(rigW0) : af(rigW1);
    rigReach = fsqrt(cx0 * cx0 + cy0 * cy0);
    // drivetrain: the engine lays power down THROUGH the drive wheels, at their average
    // x. No drive wheels placed → power goes to all wheels (AWD), traction = all rolling.
    if (nDrive > 0) driveX = driveSumX / nDrive;
    else            { driveX = (nWheels > 0) ? allWheelSumX / nWheels : comX; driveRoll = wheelRoll; }
    // support polygon: hull of the wheels, in COM-local px (drives the tip model)
    for (int i = 0; i < nSup; i++) { supX[i] -= comX; supY[i] -= comY; }
    nWheelP = nSup;                                  // mirror into the contact arrays (COM-local)
    for (int i = 0; i < nSup; i++) { wheelPX[i] = supX[i]; wheelPY[i] = supY[i]; }
    build_hull(supX, supY, nSup);
    stabL = lateral_reach(+1.0f);          // how far the COM can shift each way before
    stabR = lateral_reach(-1.0f);          // leaving the hull (BUILD readout)
    // frontal profile: how many cells tall the rig is ACROSS the direction of travel
    // (forward is +x, so the perpendicular span is the rows) — the aero cross-section.
    frontalCells = (maxRow >= minRow) ? (maxRow - minRow + 1) : 1;
    // weight balance: where the COM sits along the wheelbase. +1 = over/ahead of the
    // front axle (nose-heavy → understeer), -1 = behind the rear axle (tail-heavy → oversteer).
    float wbHalf = (wMaxX - wMinX) * 0.5f;
    float wbMid  = (wMaxX + wMinX) * 0.5f;
    balance = (wbHalf > 0.5f) ? clamp((comX - wbMid) / wbHalf, -1.0f, 1.0f) : 0.0f;
    // ── per-axle grip split: which wheels sit ahead of / behind the COM (two-axle model) ──
    frontGrip = rearGrip = 0;
    float fSumX = 0, rSumX = 0, fDrive = 0, rDrive = 0; int nF = 0, nR = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            PartKind *k = &KIND[grid[r][c]];
            if (k->grip <= 0) continue;                 // only laterally-gripping wheels (incl. casters)
            float cx = (c + 0.5f) * CELL;
            if (cx >= comX) { frontGrip += k->grip; fSumX += cx; nF++; if (k->drive > 0) fDrive += k->roll; }
            else            { rearGrip  += k->grip; rSumX += cx; nR++; if (k->drive > 0) rDrive += k->roll; }
        }
    aF = (nF ? fSumX / nF : comX) - comX;               // front/rear axle lever arms (px from COM)
    aR = (nR ? rSumX / nR : comX) - comX;
    float totDrive = fDrive + rDrive;
    rearDriveFrac = (totDrive > 0) ? rDrive / totDrive : (nDrive == 0 ? 0.5f : 0.0f);  // AWD ≈ half rear
    // two-axle needs grip front AND rear at distinct x, AND a real track (nHull>=3) — a
    // single-track inline bike (nHull<3) keeps the old single-bleed feel (darty), the same
    // exemption tipping uses; we don't model its lean, so the yaw-couple damping doesn't fit it.
    twoAxle = (frontGrip > 0.01f && rearGrip > 0.01f && (aF - aR) > CELL * 0.5f && nHull >= 3);
    // moment of inertia about the COM
    I = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int p = grid[r][c];
            PartKind *k = &KIND[p];
            if (k->mass <= 0) continue;
            float pm = (p == P_ENGINE) ? ENG[eng_kind].mass : k->mass;
            float dx = (c + 0.5f) * CELL - comX, dy = (r + 0.5f) * CELL - comY;
            I += pm * (dx * dx + dy * dy);
        }
    if (I <= 0) I = 1;

    // ── ground contact: a cell scrapes if it falls OUTSIDE the support span ────
    // The span is the box bounding all wheels/casters. Interior cells are carried
    // by the wheelbase (no wheel needed under them); only cells cantilevered past
    // the outermost support drag. A wheel/caster never drags. No support → all drag.
    nDrag = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            dragging[r][c] = 0;
            PartKind *k = &KIND[grid[r][c]];
            if (k->mass <= 0 || k->roll > 0) continue;   // empty cells & supports never scrape
            int outside = (sMaxR < 0) ||                 // no support at all
                          r < sMinR || r > sMaxR || c < sMinC || c > sMaxC;
            if (outside) { dragging[r][c] = 1; nDrag++; }
        }
    solve_wheel_loads(0, 0);                          // resting per-wheel loads (shown in BUILD)
}

static void reset_vehicle(void) {
    recompute_body();
    sx = 0; sy = 0; vx = vy = 0; ang = 0; angVel = 0;
    heat = 0; tip_amt = 0; gear = 1; rpm = 0;   // trans_mode + eng_kind persist (player settings)
    wt_long = 0; wt_xfer = 0;                   // weight transfer starts settled
    drift_loose = 0;                            // no lingering slide
    wheel_spin = 0;
    steer_pos = 0;                              // wheel centred
    wheel_grab = WHEEL_NO_GRAB; wheel_turn = 0; steer_grab = 0;   // wheel released
    gas_pos = 0; brake_pos = 0;                 // pedals released

    engine_on = 1; stalled = 0; restart_grace = 0;   // fresh rig starts cranked
    boiler = 0;                                  // steam starts cold → you feel it spool up
    wheel_ang = 0; cam_ang = 0;                  // cam_head_up persists (player setting)
    for (int i = 0; i < MAXSKID; i++) skid[i].life = 0;
    for (int i = 0; i < MAXSPARK; i++) spark[i].life = 0;
    world_reset();                              // §9: fresh world — origin chunks regenerate pristine
}

// live readout estimates — the same formulas the drive core uses, so BUILD shows
// the truth about a rig before you ever drive it.
static float est_top_speed(void) {
    // The terminal speed is the BEST any gear can sustain — not just top gear. A light rig pulls
    // top gear (drag-limited there); a HEAVY rig can't, and tops out in a lower gear where the
    // higher ratio gives more thrust (still below that gear's redline). So we check every gear:
    // in each, top = min(drag-limited speed, the gear's redline) — and take the max. This is the
    // same force balance the drive core reaches (geared thrust vs aero/rolling drag + ROLL_FRIC).
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells + SCRAPE_DRAG * nDrag;
    if (drag <= 0) return 0;
    float tract = driveRoll * GROUND_GRIP * GRIP_TO_FORCE;     // powered wheels cap the laid-down force
    float vref  = ENG[eng_kind].vref;
    int   single = (ENG[eng_kind].deftrans == TR_SINGLE);
    int   ng = single ? 1 : NGEAR;
    float best = 0;
    for (int g = 0; g < ng; g++) {
        float ratio = single ? SINGLE_RATIO : GEAR_RATIO[g];
        float thrust = nEngines * ENG[eng_kind].power * delivery(eng_kind, 0.9f, 1.0f) * ratio;
        if (tract > 0 && thrust > tract) thrust = tract;
        float vdrag   = (thrust - ROLL_FRIC * M) / drag;       // drag-limited speed in THIS gear
        float redline = vref / ratio;                          // this gear's redline (rpm = 1)
        float topg = (vdrag < redline) ? vdrag : redline;      // whichever binds first
        if (topg > best) best = topg;
    }
    return best < 0 ? 0 : best;
}
static float est_turn_rate(void) {                 // steady deg/s at speed
    float turnEase = REF_GYRO / (I / M + REF_GYRO);
    return STEER_RESP * turnEase * (1.0f - BALANCE_K * balance) / ANG_DAMP;
}
// estimated 0→100 km/h, by forward-integrating the launch like the drive core does: at each
// speed take the best gear's geared+delivered thrust (capped by traction), minus drag and the
// constant rolling-friction force (∝ mass) — F=ma stepped at 60 Hz. Returns seconds, or -1 if
// the rig can't reach 100 (under-engined / overloaded: thrust never beats drag+roll). This is
// the honest power-to-weight readout — it folds in mass, engines, gearing AND the torque curve.
static float est_0_100(void) {
    float target = 100.0f / KMH;                              // px/s for 100 km/h
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells + SCRAPE_DRAG * nDrag;
    float tract = driveRoll * GROUND_GRIP * GRIP_TO_FORCE;
    float vref  = ENG[eng_kind].vref;
    int   single = (ENG[eng_kind].deftrans == TR_SINGLE);
    int   ng = single ? 1 : NGEAR;
    float v = 0, t = 0, dt = 1.0f / 60.0f;
    for (int step = 0; step < 1800; step++) {                 // cap at 30 s
        float bestThrust = 0;
        for (int g = 0; g < ng; g++) {                        // the box picks the gear that pulls hardest here
            float ratio = single ? SINGLE_RATIO : GEAR_RATIO[g];
            float rp = clamp(v * ratio / vref, 0, 1.1f);
            float th = nEngines * ENG[eng_kind].power * delivery(eng_kind, rp, 1.0f) * ratio;
            if (th > bestThrust) bestThrust = th;
        }
        if (tract > 0 && bestThrust > tract) bestThrust = tract;
        float a = (bestThrust - drag * v) / M - ROLL_FRIC;    // net accel (F=ma, minus rolling friction)
        if (a <= 0) return -1.0f;                             // can't pull any harder → never reaches 100
        v += a * dt; t += dt;
        if (v >= target) return t;
    }
    return -1.0f;
}

static void load_design(int idx) {
    cur_des = (idx + NDES) % NDES;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) grid[r][c] = DESIGNS[cur_des][r][c];
    if (DES_ENGINE[cur_des] >= 0) {                 // some presets carry an engine kind (the extremes)
        eng_kind = DES_ENGINE[cur_des];
        trans_mode = ENG[eng_kind].deftrans;
    }
    reset_vehicle();
}

// rotate a local offset (relative to COM) into world space
static void rot(float lx, float ly, float *wx, float *wy) {
    float c = cos_deg(ang), s = sin_deg(ang);
    *wx = sx + lx * c - ly * s;
    *wy = sy + lx * s + ly * c;
}

// ╔══ FOOT MODE (seam #1, rung F0): get out of the rig, walk, get back in ══════╗
// The avatar is the OCCUPANT OF THE SEAT CELL — one rig tile (7px), GTA1-scale;
// no new abstraction. Getting out spawns you beside the seat cell's world
// position (stepped outward until clear of the hull), getting in means walking
// back within reach of that same cell — so a long rig means walking to the cab.
// A rig with no seat can't be entered, the same one rule that makes it drivable.
// The rig's state is untouched while you're out. Fenced so it lifts out cleanly
// (docs/design/big-game-backlog.md seam #1; driving-world-program.md scoreboard).
#define FOOT_R        2.5f     // avatar collision radius (px) — just under half a cell
#define FOOT_SPD     26.0f     // walk speed, world px/s
#define FOOT_RUN     46.0f     // hold Z: jog
#define FOOT_REACH   14.0f     // F re-enters within this of the seat cell (~2 cells)
#define FOOT_EXIT_SPD 14.0f    // rig must be this slow to step out (≈10 km/h, a crawl — deliberately
                               // ABOVE the automatic's idle creep ~8.3 px/s, so brake→F just works)
#define FOOT_OFFROAD  0.62f    // F1: grass/rough drags the walk — the on-foot twin of OFFROAD_GRIP
#define FOOT_PAVE_M   2.0f     // F1: the pavement strip beside a carriageway (metres, OSM) walks full pace
static float foot_x, foot_y;             // avatar world position
static float foot_dx = 1, foot_dy = 0;   // last walk direction (unit) — the head pixel leads
static int   foot_deny;                  // frames left on the "stop first" flash (F while moving)

// the seat cell as a COM-local offset; 0 if the build has no seat (then there's no door)
static int seat_local(float *lx, float *ly) {
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++)
            if (grid[r][c] == P_SEAT) {
                *lx = (c + 0.5f) * CELL - comX;
                *ly = (r + 0.5f) * CELL - comY;
                return 1;
            }
    return 0;
}

// push a circle out of one oriented box (local extents x0..x1 / y0..y1); 1 if it touched.
// Pushing along the local clamp normal makes the avatar SLIDE along walls, not stick to them.
static int circle_box_push(float *px, float *py, float r,
                           float bx, float by, float bang,
                           float x0, float x1, float y0, float y1) {
    float c = cos_deg(bang), s = sin_deg(bang);
    float dx = *px - bx, dy = *py - by;
    float lx =  c * dx + s * dy, ly = -s * dx + c * dy;   // into the box frame
    float qx = lx < x0 ? x0 : (lx > x1 ? x1 : lx);
    float qy = ly < y0 ? y0 : (ly > y1 ? y1 : ly);
    float ex = lx - qx, ey = ly - qy, d2 = ex * ex + ey * ey;
    if (d2 >= r * r) return 0;
    if (d2 > 0.0001f) {                     // outside the box: push back out to radius
        float d = fsqrt(d2), k = (r - d) / d;
        ex *= k; ey *= k;
    } else {                                // fully inside: eject through the nearest face
        float m0 = lx - x0, m1 = x1 - lx, m2 = ly - y0, m3 = y1 - ly, m = m0;
        ex = -(m0 + r); ey = 0;
        if (m1 < m) { m = m1; ex = m1 + r; ey = 0; }
        if (m2 < m) { m = m2; ex = 0; ey = -(m2 + r); }
        if (m3 < m) {         ex = 0; ey = m3 + r; }
    }
    *px += ex * c - ey * s;                 // the push, rotated back to world
    *py += ex * s + ey * c;
    return 1;
}

// would the avatar overlap anything at (x,y)? the rig hull + live obstacles (rubble is walkable)
static int foot_blocked(float x, float y) {
    float tx = x, ty = y;
    if (circle_box_push(&tx, &ty, FOOT_R, sx, sy, ang, rigL0, rigL1, rigW0, rigW1)) return 1;
    for (int i = 0; i < MAXOB; i++) {
        Obstacle *o = &pool[i];
        if (!o->alive || o->destroyed) continue;
        float dx = x - o->x, dy = y - o->y;
        if (dx * dx + dy * dy > (o->rad + 8.0f) * (o->rad + 8.0f)) continue;
        tx = x; ty = y;
        if (circle_box_push(&tx, &ty, FOOT_R, o->x, o->y, o->ang, -o->hw, o->hw, -o->hh, o->hh)) return 1;
    }
    return 0;
}

// step out: walk the spawn point outward from the seat cell (sides first, then
// fore/aft), first clear spot wins — so you appear beside YOUR seat, wherever the
// build put it. 0 = boxed in on every side (stay behind the wheel).
static int foot_exit(void) {
    float slx, sly;
    if (!seat_local(&slx, &sly)) return 0;
    static const float DIRX[4] = { 0, 0, -1, 1 }, DIRY[4] = { -1, 1, 0, 0 };
    for (int k = 1; k <= 5; k++)
        for (int d = 0; d < 4; d++) {
            float wx, wy;
            rot(slx + DIRX[d] * k * CELL, sly + DIRY[d] * k * CELL, &wx, &wy);
            if (!foot_blocked(wx, wy)) { foot_x = wx; foot_y = wy; return 1; }
        }
    return 0;
}

// ── F2: INTERIORS — walk in the door, the roof lifts, the floor plan is real ──
// Every big-enough house has ONE door on its street-facing wall (probed with road_at —
// houses face the street because streets are where houses face) and a deterministic
// BSP floor plan seeded from its identity (cx,cy,idx): perimeter walls with a doorway
// gap, split walls with interior doorways, a block of furniture per room. The plan is
// 2D geometry in the building's LOCAL frame; collision is the same circle-vs-box slide
// the rest of foot mode uses, run against wall rects instead of the footprint box —
// so "entering" is not a mode, it's walking through a hole in the wall. The roof (the
// box draw) lifts while the avatar is inside the footprint; the camera eases in.
// interiors/cityplan are the reference recipes (docs/design/interiors-brief.md).
#define IN_POOL      8         // plan cache (LRU) — you're only ever near a few houses
#define IN_MAXWALL  40
#define IN_MAXFURN  10
#define IN_MAXROOM  10
#define IN_WALL_T    1.0f      // wall half-thickness (2px walls)
#define IN_DOOR_W    7.0f      // doorway gap, full width (the avatar is 5px)
#define IN_MINROOM  11.0f      // a room side never shrinks below this
#define IN_MIN_HW    8.0f      // smaller footprints stay solid boxes (a shed, not a house)
#define FOOT_ZOOM_IN 2.0f      // the camera eases here under a lifted roof (the interior tier)
typedef struct {
    int cx, cy, idx;                           // building identity — the cache key
    unsigned age; unsigned char used, nw, nf, nr;
    float w[IN_MAXWALL][4];                    // wall rects, LOCAL x0,y0,x1,y1 (box-centre frame)
    float f[IN_MAXFURN][4];                    // furniture rects (solid, collide like walls)
    float rc[IN_MAXROOM][2];                   // leaf-room centres (connectivity + spec)
    float dx_, dy_;                            // the entry door's centre, local
    signed char ds;                            // the entry door's side (0..3, = house_door_side)
} Interior;
static Interior ipool[IN_POOL];
static unsigned iage;
static int foot_house = -1;                    // pool index of the house the avatar is inside; -1 = none

static unsigned in_rand(unsigned *st) { *st = *st * 1664525u + 1013904223u; return *st >> 10; }
static float in_frand(unsigned *st, float a, float b) { return a + (float)(in_rand(st) % 1000) / 999.0f * (b - a); }

static int house_enterable(Obstacle *o) {
    return o->kind == OB_HOUSE && !o->destroyed && o->hw >= IN_MIN_HW && o->hh >= IN_MIN_HW;
}
// which wall faces the street? probe just outside each wall's midpoint (cached on the obstacle)
static int house_door_side(Obstacle *o) {
    if (o->door >= 0) return o->door;
    float c = cos_deg(o->ang), s = sin_deg(o->ang);
    static const float NX[4] = { 1, -1, 0, 0 }, NY[4] = { 0, 0, 1, -1 };
    int best = 0, bscore = -1;
    for (int d = 0; d < 4; d++) {
        float ox = NX[d] * (o->hw + 6.0f), oy = NY[d] * (o->hh + 6.0f);
        RoadHit h = road_at(o->x + c * ox - s * oy, o->y + s * ox + c * oy);
        int sc = h.on_road ? 2 : (h.on_pave ? 1 : 0);
        if (sc > bscore) { bscore = sc; best = d; }
    }
    o->door = (signed char)best;
    return best;
}
static void ob_l2w(Obstacle *o, float lx, float ly, float *wx, float *wy) {   // building local → world
    float c = cos_deg(o->ang), s = sin_deg(o->ang);
    *wx = o->x + c * lx - s * ly;
    *wy = o->y + s * lx + c * ly;
}
static void in_wall(Interior *in, float x0, float y0, float x1, float y1) {
    if (in->nw >= IN_MAXWALL) return;
    in->w[in->nw][0] = x0; in->w[in->nw][1] = y0; in->w[in->nw][2] = x1; in->w[in->nw][3] = y1; in->nw++;
}
// a wall along one axis at cross-position p, from a to b, with a doorway gap centred at g
static void in_wall_gap(Interior *in, int vert, float p, float a, float b, float g) {
    float h = IN_DOOR_W * 0.5f, t = IN_WALL_T;
    if (vert) {                                // wall runs along y, at x = p
        if (g - h > a) in_wall(in, p - t, a, p + t, g - h);
        if (b > g + h) in_wall(in, p - t, g + h, p + t, b);
    } else {                                   // wall runs along x, at y = p
        if (g - h > a) in_wall(in, a, p - t, g - h, p + t);
        if (b > g + h) in_wall(in, g + h, p - t, b, p + t);
    }
}
static void in_split(Interior *in, unsigned *st, float x0, float y0, float x1, float y1, int depth) {
    float w_ = x1 - x0, h_ = y1 - y0;
    if (depth <= 0 || (w_ < IN_MINROOM * 2 && h_ < IN_MINROOM * 2)) {   // a LEAF room
        if (in->nr < IN_MAXROOM) { in->rc[in->nr][0] = (x0 + x1) * 0.5f; in->rc[in->nr][1] = (y0 + y1) * 0.5f; in->nr++; }
        if (in->nf < IN_MAXFURN && w_ >= 10 && h_ >= 10) {              // one block of furniture, hugging a
            float fw = in_frand(st, 1.0f, 1.6f), fh = in_frand(st, 1.0f, 1.6f);   // CORNER (couch against the
            int corner = (int)(in_rand(st) & 3);                                  // wall — can't pinch a doorway,
            float fx = (corner & 1) ? x1 - 1.5f - fw : x0 + 1.5f + fw;            // gaps sit ≥3.5px from corners)
            float fy = (corner & 2) ? y1 - 1.5f - fh : y0 + 1.5f + fh;
            in->f[in->nf][0] = fx - fw; in->f[in->nf][1] = fy - fh;
            in->f[in->nf][2] = fx + fw; in->f[in->nf][3] = fy + fh; in->nf++;
        }
        return;
    }
    if (w_ > h_) {                             // split the long axis; every split wall gets a doorway
        float p = in_frand(st, x0 + IN_MINROOM, x1 - IN_MINROOM);
        float g = in_frand(st, y0 + IN_DOOR_W, y1 - IN_DOOR_W);
        in_wall_gap(in, 1, p, y0, y1, g);
        in_split(in, st, x0, y0, p, y1, depth - 1);
        in_split(in, st, p, y0, x1, y1, depth - 1);
    } else {
        float p = in_frand(st, y0 + IN_MINROOM, y1 - IN_MINROOM);
        float g = in_frand(st, x0 + IN_DOOR_W, x1 - IN_DOOR_W);
        in_wall_gap(in, 0, p, x0, x1, g);
        in_split(in, st, x0, y0, x1, p, depth - 1);
        in_split(in, st, x0, p, x1, y1, depth - 1);
    }
}
static void interior_build(Interior *in, Obstacle *o, unsigned salt) {
    in->cx = o->cx; in->cy = o->cy; in->idx = o->idx;
    in->used = 1; in->nw = in->nf = in->nr = 0;
    unsigned st = ((unsigned)o->cx * 73856093u) ^ ((unsigned)o->cy * 19349663u) ^ ((unsigned)o->idx * 83492791u);
    st = (st + salt) * 2654435761u + 1u;
    float t = IN_WALL_T, hw = o->hw, hh = o->hh;
    int d = house_door_side(o);                // the entry door: CENTRED on the street wall (legible, not seeded)
    in->ds = (signed char)d;
    if (d == 0) { in_wall_gap(in, 1,  hw - t, -hh, hh, 0); in->dx_ =  hw - t; in->dy_ = 0; }
    else          in_wall(in,  hw - 2 * t, -hh,  hw,  hh);
    if (d == 1) { in_wall_gap(in, 1, -hw + t, -hh, hh, 0); in->dx_ = -hw + t; in->dy_ = 0; }
    else          in_wall(in, -hw, -hh, -hw + 2 * t,  hh);
    if (d == 2) { in_wall_gap(in, 0,  hh - t, -hw, hw, 0); in->dx_ = 0; in->dy_ =  hh - t; }
    else          in_wall(in, -hw,  hh - 2 * t,  hw,  hh);
    if (d == 3) { in_wall_gap(in, 0, -hh + t, -hw, hw, 0); in->dx_ = 0; in->dy_ = -hh + t; }
    else          in_wall(in, -hw, -hh,  hw, -hh + 2 * t);
    in_split(in, &st, -hw + 2 * t, -hh + 2 * t, hw - 2 * t, hh - 2 * t, 3);
}
// is every room reachable from the front door? Flood the local plan with avatar-radius
// clearance. A BSP can seal a room (a child wall landing across its parent's doorway) —
// this is the oracle interior_gen re-rolls against, so a generated house NEVER cheats you.
static int interior_connected(Interior *in, float hw, float hh) {
    static unsigned char cvis[96][96]; static short cqx[4096], cqy[4096];
    float pitch = 2.0f;
    { float span = (hw > hh ? hw : hh) * 2.0f;
      if (span / pitch > 94.0f) pitch = span / 94.0f; }   // huge OSM footprints sample coarser
    int gw2 = (int)(hw * 2.0f / pitch), gh2 = (int)(hh * 2.0f / pitch);
    if (gw2 > 95) gw2 = 95; if (gh2 > 95) gh2 = 95;
    for (int a = 0; a <= gh2; a++) for (int b = 0; b <= gw2; b++) cvis[a][b] = 0;
    float inx = (in->ds == 0) - (in->ds == 1), iny = (in->ds == 2) - (in->ds == 3);
    float sx0 = in->dx_ - inx * 4.0f, sy0 = in->dy_ - iny * 4.0f;      // just INSIDE the door
    int qn = 0, qh2 = 0;
    int sgx = (int)((sx0 + hw) / pitch), sgy = (int)((sy0 + hh) / pitch);
    if (sgx < 0 || sgy < 0 || sgx > gw2 || sgy > gh2) return 0;
    cqx[qn] = (short)sgx; cqy[qn] = (short)sgy; qn++; cvis[sgy][sgx] = 1;
    while (qh2 < qn && qn < 4090) {
        int cxg = cqx[qh2], cyg = cqy[qh2]; qh2++;
        static const int DX4[4] = { 1, -1, 0, 0 }, DY4[4] = { 0, 0, 1, -1 };
        for (int d4 = 0; d4 < 4; d4++) {
            int nx2 = cxg + DX4[d4], ny2 = cyg + DY4[d4];
            if (nx2 < 0 || ny2 < 0 || nx2 > gw2 || ny2 > gh2 || cvis[ny2][nx2]) continue;
            float lx = -hw + nx2 * pitch, ly = -hh + ny2 * pitch;
            if (af(lx) > hw - 0.5f || af(ly) > hh - 0.5f) continue;
            int hit = 0;
            for (int k = 0; k < in->nw + in->nf && !hit; k++) {
                float *rc2 = (k < in->nw) ? in->w[k] : in->f[k - in->nw];
                float qx2 = lx < rc2[0] ? rc2[0] : (lx > rc2[2] ? rc2[2] : lx);
                float qy2 = ly < rc2[1] ? rc2[1] : (ly > rc2[3] ? rc2[3] : ly);
                float ex = lx - qx2, ey = ly - qy2;
                if (ex * ex + ey * ey < (FOOT_R - 0.3f) * (FOOT_R - 0.3f)) hit = 1;
            }
            if (hit) continue;
            cvis[ny2][nx2] = 1; cqx[qn] = (short)nx2; cqy[qn] = (short)ny2; qn++;
        }
    }
    for (int r = 0; r < in->nr; r++) {          // a reached cell within ~2 cells of each room centre
        int rgx = (int)((in->rc[r][0] + hw) / pitch), rgy = (int)((in->rc[r][1] + hh) / pitch);
        int found = 0;
        for (int ay = rgy - 2; ay <= rgy + 2 && !found; ay++)
            for (int ax = rgx - 2; ax <= rgx + 2 && !found; ax++)
                if (ay >= 0 && ax >= 0 && ay <= gh2 && ax <= gw2 && cvis[ay][ax]) found = 1;
        if (!found) return 0;
    }
    return 1;
}
static void interior_gen(Interior *in, Obstacle *o) {
    for (unsigned salt = 0; salt < 8; salt++) {           // deterministic: first CONNECTED roll wins
        interior_build(in, o, salt);
        if (interior_connected(in, o->hw, o->hh)) return;
    }                                                     // 8 misses (huge/degenerate): keep the last roll
}
static Interior *interior_get(Obstacle *o) {
    int lru = 0;
    for (int i = 0; i < IN_POOL; i++) {
        if (ipool[i].used && ipool[i].cx == o->cx && ipool[i].cy == o->cy && ipool[i].idx == o->idx) {
            ipool[i].age = ++iage; return &ipool[i];
        }
        if (!ipool[i].used || ipool[i].age < ipool[lru].age) lru = i;
    }
    interior_gen(&ipool[lru], o);
    ipool[lru].age = ++iage;
    return &ipool[lru];
}

// the FOOT update: 8-way walk (arrows/WASD/d-pad, Z jogs), then the world pushes back
static void update_foot(float dt_) {
    float mx = (key(KEY_RIGHT) || key('D') ? 1.0f : 0.0f) - (key(KEY_LEFT) || key('A') ? 1.0f : 0.0f);
    float my = (key(KEY_DOWN)  || key('S') ? 1.0f : 0.0f) - (key(KEY_UP)   || key('W') ? 1.0f : 0.0f);
    mx += (btn(0, BTN_RIGHT) ? 1.0f : 0.0f) - (btn(0, BTN_LEFT) ? 1.0f : 0.0f);
    my += (btn(0, BTN_DOWN)  ? 1.0f : 0.0f) - (btn(0, BTN_UP)   ? 1.0f : 0.0f);
    float m2 = mx * mx + my * my;
    if (m2 > 0.01f) {
        float inv = 1.0f / fsqrt(m2);       // diagonals walk no faster
        float spd = (key('Z') || btn(0, BTN_A)) ? FOOT_RUN : FOOT_SPD;
        RoadHit fh = road_at(foot_x, foot_y);            // F1: the ground drives the feet — the SAME
        if (!fh.on_road && !fh.on_pave) spd *= FOOT_OFFROAD;   // seam as the rig's tyres. Tarmac + the
                                                         // pavement strip walk full pace; grass drags.
        foot_x += mx * inv * spd * dt_;
        foot_y += my * inv * spd * dt_;
        foot_dx = mx * inv; foot_dy = my * inv;
    }
    circle_box_push(&foot_x, &foot_y, FOOT_R, sx, sy, ang, rigL0, rigL1, rigW0, rigW1);
    foot_house = -1;
    for (int i = 0; i < MAXOB; i++) {
        Obstacle *o = &pool[i];
        if (!o->alive || o->destroyed) continue;
        float dx = foot_x - o->x, dy = foot_y - o->y;
        if (dx * dx + dy * dy > (o->rad + 8.0f) * (o->rad + 8.0f)) continue;
        if (house_enterable(o)) {              // F2: near a house, the WALLS collide, not the box —
            float c = cos_deg(o->ang), s = sin_deg(o->ang);      // the doorway gap is a real hole
            float lx = c * dx + s * dy, ly = -s * dx + c * dy;
            if (af(lx) < o->hw + FOOT_R + 3.0f && af(ly) < o->hh + FOOT_R + 3.0f) {
                Interior *in = interior_get(o);
                for (int k = 0; k < in->nw; k++)
                    circle_box_push(&foot_x, &foot_y, FOOT_R, o->x, o->y, o->ang,
                                    in->w[k][0], in->w[k][2], in->w[k][1], in->w[k][3]);
                for (int k = 0; k < in->nf; k++)
                    circle_box_push(&foot_x, &foot_y, FOOT_R, o->x, o->y, o->ang,
                                    in->f[k][0], in->f[k][2], in->f[k][1], in->f[k][3]);
                dx = foot_x - o->x; dy = foot_y - o->y;          // inside? (post-push) → this roof lifts
                lx = c * dx + s * dy; ly = -s * dx + c * dy;
                if (af(lx) < o->hw && af(ly) < o->hh) foot_house = i;
                continue;
            }
        }
        circle_box_push(&foot_x, &foot_y, FOOT_R, o->x, o->y, o->ang, -o->hw, o->hw, -o->hh, o->hh);
    }
}
// ╚══ end FOOT MODE state + sim (input hooks live in handle_input; render in draw_foot) ══╝

// ── input ─────────────────────────────────────────────────────────────────────
static int in_gas, in_brk, in_steer, in_hand, in_up, in_down;
static int gear_req = -99;            // a gear tapped directly in the gate this frame (-99 = none);
                                      // consumed (and the change validated) in update_drive

// ── cockpit dashboard: touch / mouse / keyboard controls + round gauges ───────
// The layout is shared between handle_input (hit-testing) and hud (drawing), so
// the rects live here as #defines. Every control is LABELLED with its key and
// fires on a tap, a mouse press, OR the key — all three at once. Hold controls
// (steer/gas/brake/drift) use a level test; discrete ones (shift/ignition/trans/
// build) an edge test. Built on tap()/tapp()+mouse rather than ui.h capture:
// the zones don't overlap, so two fingers (hold gas + tap a gear) just work.
//
// LAYOUT (phone-first): the pedals sit at the far-LEFT edge (left thumb) and the
// steering wheel at the far-RIGHT edge (right thumb) — OPPOSITE edges, so steer and
// gas never crowd one thumb. The gauges (speed, tach), the mode buttons (IGN/TRANS/
// BUILD) and the gear selector fill the middle, where no thumb rests while driving.
#define DASH_Y   148                  // top of the dashboard band (… SCREEN_H)
// LEFT EDGE — pedals (held, left thumb): gas / brake / drift
#define PED_X    2
#define PED_W    42
#define PED_H    16
#define GAS_Y    150
#define BRK_Y    167
#define DRF_Y    184
// CENTRE — gauges (display) + the mode buttons
#define SPD_X    50                   // speed LED readout
#define SPD_Y    152
#define CAM_BTN_X 50                  // camera-mode toggle — left half of the strip under the speed LED
#define CAM_BTN_Y 180
#define CAM_BTN_W 24
#define CAM_BTN_H 16
#define SCL_BTN_X 76                  // scaling toggle (was the V-key debug A/B) — right half
#define SCL_BTN_Y 180
#define SCL_BTN_W 24
#define SCL_BTN_H 16
#define DIAL_CX  128                  // round RPM tach
#define DIAL_CY  174
#define DIAL_R   21
#define MODE_BTN_X    154                  // mode buttons: ignition / trans / build
#define BTN_W    52
#define BTN_H    15
#define IGN_Y    150
#define TRN_Y    167
#define BLD_Y    184
// GEAR SELECTOR — between the gauges and the wheel (right thumb reaches it). MANUAL =
// a full H-gate, every slot individually tappable; AUTO/1-GEAR = a D / N / R selector.
#define GEAR_X   210
#define GEAR_Y   150
#define GEAR_W   52
#define GEAR_H   48
// RIGHT EDGE — steering wheel (held, right thumb). Far from the pedals = max spacing.
#define WHEEL_CX 291                  // wheel centre + radius
#define WHEEL_CY 174
#define WHEEL_R  22
#define WHL_X    264                  // wheel hit-area: left half steers left, right half right
#define WHL_Y    150
#define WHL_W    54
#define WHL_H    48

// gear-selector slot rects — shared by handle_input (hit-test) and hud (draw) so the
// tappable zones and the drawn slots always agree. MANUAL: a real H-gate — 1·3·5 across
// the top, 2·4·R across the bottom, N in the centre channel; every slot is its own tap
// target (tap R directly to reverse). AUTO/1-GEAR: three stacked D / N / R buttons.
static void mgate_rect(int g, int *x, int *y, int *w, int *h) {
    int cw = GEAR_W / 3;                          // column width (~17)
    if (g == 0) { *x = GEAR_X + cw; *y = GEAR_Y + 18; *w = cw; *h = 12; return; }  // N — centre channel
    int col = (g == -1) ? 2 : (g - 1) / 2;        // R shares col 2 with 5; 1/2→0, 3/4→1, 5→2
    int top = (g == -1) ? 0 : ((g - 1) % 2 == 0); // R = bottom row; 1/3/5 top, 2/4 bottom
    *x = GEAR_X + col * cw; *w = cw;
    if (top) { *y = GEAR_Y;      *h = 18; }
    else     { *y = GEAR_Y + 30; *h = 18; }
}
static void agate_rect(int i, int *x, int *y, int *w, int *h) {   // i: 0=DRIVE 1=NEUTRAL 2=REVERSE
    *x = GEAR_X; *y = GEAR_Y + i * 16; *w = GEAR_W; *h = 15;
}

static int pt_in(int x, int y, int w, int h) {        // mouse pointer inside rect?
    int mx = mouse_x(), my = mouse_y();
    return mx >= x && mx < x + w && my >= y && my < y + h;
}
static int ctl_held(int x, int y, int w, int h) {     // finger resting here, or mouse held over it
    return tap(x, y, w, h) || (mouse_down(MOUSE_LEFT) && pt_in(x, y, w, h));
}
static int ctl_hit(int x, int y, int w, int h) {      // touch began here, or mouse clicked here, this frame
    return tapp(x, y, w, h) || (mouse_pressed(MOUSE_LEFT) && pt_in(x, y, w, h));
}
static void do_ignition(void) {                       // crank or kill (key I or the IGN button)
    engine_on = !engine_on;
    if (engine_on) { stalled = 0; restart_grace = RESTART_GRACE; hit(32, INSTR_SAW, 2, 70); hit(45, INSTR_SAW, 2, 120); }
    else           { stalled = 0; hit(26, INSTR_NOISE, 2, 90); }
}
static void do_trans(void) { trans_mode = (trans_mode + 1) % 3; gear = 1; }   // cycle SINGLE/AUTO/MANUAL
// cycle the rig-wide engine kind (BUILD). Mass changes → recompute the body; also drop the
// transmission to the kind's sensible default (electric → 1-GEAR, combustion → AUTO).
static void do_kind(void) {
    eng_kind = (eng_kind + 1) % EK_KINDS;
    trans_mode = ENG[eng_kind].deftrans; gear = 1;
    recompute_body();
}

// a labelled cockpit button: fills with `actcol` when active, shows label + key hint
static void dash_btn(int x, int y, int w, int h, const char *lbl, const char *keyhint, int active, int actcol) {
    rectfill(x, y, w, h, active ? actcol : CLR_DARKER_GREY);
    rect(x, y, w, h, CLR_DARK_GREY);
    font(FONT_SMALL);
    print_centered(lbl, x + w / 2, y + 2, active ? CLR_BLACK : CLR_LIGHT_GREY);
    if (keyhint) print_centered(keyhint, x + w / 2, y + h - 7, active ? CLR_BLACK : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

static void clear_grid(void) {
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) grid[r][c] = P_NONE;
    recompute_body();
}
static void handle_input(void) {
    if (mode == MODE_FOOT) {                   // ── ON FOOT (rung F0): a deliberately tiny input surface ──
        if (keyp('P')) is_paused = !is_paused;
        if (keyp('R')) { mode = MODE_DRIVE; reset_vehicle(); }   // reset = back behind the wheel, fresh
        if (keyp('F')) {                       // F re-enters — but only within reach of the SEAT cell
            float slx, sly, swx, swy;
            if (seat_local(&slx, &sly)) {
                rot(slx, sly, &swx, &swy);
                float dx = foot_x - swx, dy = foot_y - swy;
                if (dx * dx + dy * dy < FOOT_REACH * FOOT_REACH) mode = MODE_DRIVE;
            }
        }
        return;                                // no cockpit, no BUILD, no template keys while out of the rig
    }
    if (keyp(KEY_TAB) || keyp('B')) {          // flip between BUILD and DRIVE (TAB or B)
        mode = (mode == MODE_DRIVE) ? MODE_BUILD : MODE_DRIVE;
        if (mode == MODE_DRIVE) reset_vehicle();   // drive your current build, fresh
    }
    // templates (1-5) load an editable starting rig in either mode
    if (keyp('1')) load_design(0);
    if (keyp('2')) load_design(1);
    if (keyp('3')) load_design(2);
    if (keyp('4')) load_design(3);
    if (keyp('5')) load_design(4);
    if (keyp('6')) load_design(5);   // FWD
    if (keyp('7')) load_design(6);   // RWD
    if (keyp('8')) load_design(7);   // 4WD
    if (keyp('9')) load_design(8);   // SUPERCAR (RACE engine → ~300 km/h)
    if (keyp('0')) load_design(9);   // TRUCK (TRACTOR engine → ~45 km/h)
    if (keyp('-')) load_design(10);  // SEMI — the long 18-wheeler (diesel)
    if (keyp('=')) load_design(11);  // SCHOOLBUS — long boxy diesel

    if (mode == MODE_BUILD) {
        if (keyp('R')) clear_grid();           // R clears the grid to empty
        if (keyp('K')) do_kind();              // K cycles the rig's engine kind (§1a)
        // part-select hotkeys mirror the palette
        if (keyp('F')) sel_part = P_FRAME;
        if (keyp('E')) sel_part = P_ENGINE;
        if (keyp('W')) sel_part = P_WHEEL;
        if (keyp('D')) sel_part = P_DRIVE;     // powered (driven) wheel
        if (keyp('C')) sel_part = P_CASTER;
        if (keyp('S')) sel_part = P_SEAT;
        if (keyp('O')) sel_part = P_CARGO;     // heavy crate (dead weight)
        if (keyp('X')) sel_part = P_NONE;      // eraser
        return;
    }
    // ---- DRIVE input: keyboard OR the on-screen cockpit (touch + mouse) ----
    if (keyp('R')) reset_vehicle();
    if (keyp('P')) is_paused = !is_paused;
    if (keyp('I') || ctl_hit(MODE_BTN_X, IGN_Y, BTN_W, BTN_H)) do_ignition();   // IGN button
    if (keyp('G') || ctl_hit(MODE_BTN_X, TRN_Y, BTN_W, BTN_H)) do_trans();      // TRANS button
    if (keyp('F')) {                           // F: GET OUT (rung F0) — only at a stop, only via a seat
        if (vx * vx + vy * vy < FOOT_EXIT_SPD * FOOT_EXIT_SPD && foot_exit()) {
            vx = vy = angVel = 0;              // parking brake — the rig holds still while you're out
            mode = MODE_FOOT;
        } else foot_deny = 90;                 // flash why not (still rolling / no seat / boxed in)
    }
    if (ctl_hit(MODE_BTN_X, BLD_Y, BTN_W, BTN_H)) mode = MODE_BUILD;            // BUILD button
    if (mode == MODE_DRIVE &&                                              // CAM toggle: north-up / heading-up
        (keyp('C') || ctl_hit(CAM_BTN_X, CAM_BTN_Y, CAM_BTN_W, CAM_BTN_H)))  // ('C' is caster-select in BUILD)
        cam_head_up = !cam_head_up;
    if (mode == MODE_DRIVE &&                                              // SCALE toggle: stepped / smooth zoom-out
        (keyp('V') || ctl_hit(SCL_BTN_X, SCL_BTN_Y, SCL_BTN_W, SCL_BTN_H))) {
        smooth_mode = !smooth_mode;
        smooth_zoom(smooth_mode != 0);
    }

    // --- steering: keyboard / gamepad stay digital; the on-screen WHEEL is now GRABBED
    // and turned (touch + mouse). Grab anywhere on the wheel and sweep around the hub —
    // the rotation maps straight to the steer angle (a quarter-turn = full lock); release
    // and it self-centres. (Replaces the old left/right tap-halves.) -------------------
    int key_l = key(KEY_LEFT)  || btn(0, BTN_LEFT);
    int key_r = key(KEY_RIGHT) || btn(0, BTN_RIGHT);

    // locate the pointer on the wheel: a tracked finger, a new finger landing in the
    // hit-circle, or the mouse pressed/held there (WHEEL_MOUSE marks a mouse grab).
    float wpx = 0, wpy = 0; int have_ptr = 0;
    float grab_r2 = (WHEEL_R + 9) * (WHEEL_R + 9);
    if (wheel_grab == WHEEL_MOUSE) {
        if (mouse_down(MOUSE_LEFT)) { wpx = mouse_x(); wpy = mouse_y(); have_ptr = 1; }
    } else if (wheel_grab != WHEEL_NO_GRAB) {
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == wheel_grab) { wpx = touch_x(i); wpy = touch_y(i); have_ptr = 1; break; }
    }
    if (wheel_grab == WHEEL_NO_GRAB) {                 // not holding it yet → try to claim a grab
        for (int i = 0; i < touch_count(); i++) {
            float tx = touch_x(i), ty = touch_y(i), dx = tx - WHEEL_CX, dy = ty - WHEEL_CY;
            if (dx * dx + dy * dy <= grab_r2) {
                wheel_grab = touch_id(i); wpx = tx; wpy = ty; have_ptr = 1;
                wheel_turn = steer_pos * WHEEL_LOCK; wheel_prev_ang = atan2f(dy, dx) * 57.29578f;
                break;
            }
        }
        if (!have_ptr && mouse_pressed(MOUSE_LEFT)) {
            float dx = mouse_x() - WHEEL_CX, dy = mouse_y() - WHEEL_CY;
            if (dx * dx + dy * dy <= grab_r2) {
                wheel_grab = WHEEL_MOUSE; wpx = mouse_x(); wpy = mouse_y(); have_ptr = 1;
                wheel_turn = steer_pos * WHEEL_LOCK; wheel_prev_ang = atan2f(dy, dx) * 57.29578f;
            }
        }
    }
    steer_grab = 0;
    if (have_ptr) {                                    // accumulate the finger's sweep around the hub
        float pa = atan2f(wpy - WHEEL_CY, wpx - WHEEL_CX) * 57.29578f;
        float d = pa - wheel_prev_ang;
        while (d >  180.0f) d -= 360.0f;               // shortest signed step (no wrap glitch)
        while (d < -180.0f) d += 360.0f;
        wheel_prev_ang = pa;
        wheel_turn = clamp(wheel_turn + d, -WHEEL_LOCK, WHEEL_LOCK);
        steer_grab = 1;
    } else {
        wheel_grab = WHEEL_NO_GRAB;                    // finger lifted / mouse released → let it re-centre
    }

    in_gas   = key('Z') || key(KEY_UP)   || btn(0, BTN_A) || btn(0, BTN_UP)   || ctl_held(PED_X, GAS_Y, PED_W, PED_H);
    in_brk   = key('X') || key(KEY_DOWN) || btn(0, BTN_B) || btn(0, BTN_DOWN) || ctl_held(PED_X, BRK_Y, PED_W, PED_H);
    in_hand  = key(KEY_SPACE) || ctl_held(PED_X, DRF_Y, PED_W, PED_H);
    in_steer = key_r - key_l;                          // digital (keyboard / gamepad) only
    in_up    = keyp('E');             // keyboard up/down still step the gears sequentially
    in_down  = keyp('Q');
    // direct gear selection — tap any slot in the gate (mode-aware). Recorded here,
    // applied (and reverse validated against speed) in update_drive.
    gear_req = -99;
    if (trans_mode == TR_MANUAL) {
        static const int MG[7] = { 1, 2, 3, 4, 5, 0, -1 };
        for (int i = 0; i < 7; i++) {
            int gx, gy, gw, gh; mgate_rect(MG[i], &gx, &gy, &gw, &gh);
            if (ctl_hit(gx, gy, gw, gh)) gear_req = MG[i];
        }
    } else {                          // AUTO / 1-GEAR: D / N / R
        static const int AG[3] = { 1, 0, -1 };
        for (int i = 0; i < 3; i++) {
            int gx, gy, gw, gh; agate_rect(i, &gx, &gy, &gw, &gh);
            if (ctl_hit(gx, gy, gw, gh)) gear_req = AG[i];
        }
    }
}

static void lay_skid(float x, float y) {
    skid[skid_head % MAXSKID] = (Skid){ x, y, 150 };
    skid_head++;
}

// sparks fly off a scraping cell, mostly opposite the rig's travel (like a grinder).
// NOTE (terrain, rung 3): sparks are METAL-ON-TARMAC. On road the belly grinds and
// throws sparks + heats up; OFFROAD (sand/mud/grass) there's no spark and little heat —
// the cell ploughs a DIRT FURROW instead, and drag is *worse* (it digs in, not skates).
// When biomes land, gate throw_spark()/heat on a hard-surface terrain flag and swap in
// a dust/furrow effect + a higher SCRAPE_DRAG off-road. Today everything is tarmac.
static void throw_spark(float x, float y, float vfx, float vfy) {
    int col = (rnd_float() < 0.4f) ? CLR_WHITE : (rnd_float() < 0.6f) ? CLR_ORANGE : CLR_YELLOW;
    spark[spark_head % MAXSPARK] = (Spark){
        x, y,
        -vfx * 0.4f + rnd_float_between(-22, 22),
        -vfy * 0.4f + rnd_float_between(-22, 22),
        rnd_between(8, 18), col };
    spark_head++;
}

// a puff of tyre smoke (reuses the spark pool) — grey/white, slow drift, longer-lived than a spark.
static void smoke_puff(float x, float y) {
    spark[spark_head % MAXSPARK] = (Spark){
        x + rnd_float_between(-3, 3), y + rnd_float_between(-3, 3),
        rnd_float_between(-9, 9), rnd_float_between(-9, 9),
        rnd_between(16, 28), (rnd_float() < 0.5f) ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY };
    spark_head++;
}

// ── physics: the honest core ──────────────────────────────────────────────────
static void update_drive(float dt_) {
    // Rung B step 5: the OSM road SURFACE drives handling. On a real road → full grip; off it → the
    // tyres let go (slide/understeer) and rough ground adds drag. OSM-only (surf_grip stays 1 with no
    // .rvb) so the default cart's uniform-grip feel is unchanged. gg = the surface-scaled ground grip
    // fed through every friction term (traction cap, braking, the friction circle, lateral bleed).
    surf_grip = (osm_loaded || rn2_on || cg_on) ? (road_at(sx, sy).on_road ? 1.0f : OFFROAD_GRIP) : 1.0f;
    float gg = GROUND_GRIP * surf_grip;

    float fwx = cos_deg(ang), fwy = sin_deg(ang);   // forward unit vector
    float ltx = -fwy, lty = fwx;                    // lateral (left) unit vector

    // decompose velocity into forward + lateral
    float vf = vx * fwx + vy * fwy;
    float vl = vx * ltx + vy * lty;
    float vf0 = vf;                                 // start-of-frame forward speed → longitudinal accel
                                                    // (vf - vf0)/dt after thrust+brake, for weight transfer

    // --- ground scrape: cells hanging past the wheel span drag on the floor -----
    // Same force model as the wheels, applied to the cells with NO support under
    // them: extra forward drag (top speed ↓), a lateral anchor, and — if the scrape
    // is off-centre — a yaw that pulls the rig to that side. Each scraping cell also
    // throws sparks and heats up. Kinetic: only bites once the rig is actually moving.
    float spd0 = fsqrt(vf * vf + vl * vl);
    int   scraping = (nDrag > 0 && spd0 > SCRAPE_MINSPD);
    float scrape_drag = 0, scrape_torque = 0;
    if (scraping) {
        scrape_drag = SCRAPE_DRAG * nDrag;
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++) {
                if (!dragging[r][c]) continue;
                float oy = (r + 0.5f) * CELL - comY;       // off-centre → twists the rig
                scrape_torque += -oy * SCRAPE_DRAG;        // a corner drag pulls to that side
                if (rnd_float() < clamp(spd0 / 220.0f, 0.05f, 0.7f)) {
                    float wx, wy; rot((c + 0.5f) * CELL - comX, oy, &wx, &wy);
                    throw_spark(wx, wy, fwx * spd0, fwy * spd0);
                }
            }
    }

    // --- dynamic stability: cornering load shifts the COM toward the turn's ----
    //     OUTSIDE; if it leaves the support hull the rig tips onto an unsupported
    //     corner → transient scrape + the loaded tires let go (lateral grip
    //     collapses, so the rig pushes wide / breaks loose). 2.5's scrape is a cell
    //     ALWAYS on the floor; this is the *build* tipping under cornering load. A
    //     4-wheeler's quad rarely tips; a 3-wheeler's triangle tips toward its gap.
    float aLat  = vf * (angVel * DEG2RAD);          // centripetal accel (px/s^2), signed
    float loadY = -aLat * STAB_H;                   // COM load-shift along local lateral (outside)
    float reach = (loadY >= 0) ? stabL : stabR;     // hull reach on the loaded side
    float over  = af(loadY) - reach;                // px past the hull edge → tipping
    int   tipping = (nHull >= 3 && over > 0 && spd0 > SCRAPE_MINSPD);
    tip_amt = tipping ? clamp(over / (reach + CELL), 0, 1) : 0;
    if (tipping) {                                   // the outside corner digs in
        float oySign = (loadY >= 0) ? 1.0f : -1.0f;
        if (rnd_float() < clamp(spd0 / 200.0f, 0.1f, 0.8f)) {
            float wx, wy; rot(frontX - comX - CELL, oySign * (reach + CELL * 0.5f), &wx, &wy);
            throw_spark(wx, wy, fwx * spd0, fwy * spd0);
        }
        heat = clamp(heat + HEAT_RISE * dt_, 0, 1.0f);
    }

    // --- shifting: gear is REVERSE (-1) · NEUTRAL (0) · forward 1..NGEAR. -------------------
    //   MANUAL  : up/down step the whole sequence R ↔ N ↔ 1 ↔ 2 ↔ … (a real H-gate with neutral).
    //   AUTO/1  : the box manages forward gears; up/down just pick DRIVE vs REVERSE (one tap from a
    //             stop drops it into R — so you CAN reverse in automatic). No exposed neutral.
    //   Engaging reverse needs a near-stop (REV_ENGAGE_SPD) and zeroes vf for a clean swap.
    // direct gear selection — a slot tapped in the gate (recorded in handle_input). MANUAL
    // grabs any forward gear / N / R outright; AUTO/1 picks D (=1, the box manages it) / N / R.
    // Reverse only engages near a stop (REV_ENGAGE_SPD); everything else is unconditional.
    if (gear_req != -99) {
        if (trans_mode == TR_MANUAL) {
            if (gear_req >= 1 && gear_req <= NGEAR) { if (gear < 0) vf = 0; gear = gear_req; shift_snd = 1; }
            else if (gear_req == 0)  { gear = 0; shift_snd = 1; }
            else if (gear_req == -1 && af(vf) < REV_ENGAGE_SPD) { gear = -1; vf = 0; shift_snd = 1; }
        } else {                                     // AUTO / 1-GEAR: DRIVE / NEUTRAL / REVERSE
            if (gear_req == 1)       { if (gear < 0) vf = 0; gear = 1; shift_snd = 1; }   // D (only zero vf when leaving REVERSE — N→D keeps momentum)
            else if (gear_req == 0)  { gear = 0; shift_snd = 1; }                          // N
            else if (gear_req == -1 && af(vf) < REV_ENGAGE_SPD) { gear = -1; vf = 0; shift_snd = 1; }
        }
        gear_req = -99;
    }
    if (in_up) {                                     // E / keyboard up
        if (trans_mode == TR_MANUAL) {
            if (gear < NGEAR) { if (gear < 0) vf = 0; gear++; shift_snd = 1; }   // R→N→1→…→5
        } else if (gear < 1) {                       // AUTO/1: R → N → D
            if (gear < 0) vf = 0; gear++; shift_snd = 1;   // only R→N zeroes vf; N→D keeps momentum (coast in N, re-couple in D)
        }
    }
    if (in_down) {                                   // Q / keyboard down
        if (trans_mode == TR_MANUAL) {
            if (gear > 0) { gear--; shift_snd = 1; }                              // 5→…→1→N
            else if (gear == 0 && af(vf) < REV_ENGAGE_SPD) { gear = -1; vf = 0; shift_snd = 1; }  // N→R
        } else {                                     // AUTO/1: D → N → R
            if (gear >= 1) { gear = 0; shift_snd = 1; }
            else if (gear == 0 && af(vf) < REV_ENGAGE_SPD) { gear = -1; vf = 0; shift_snd = 1; }
        }
    }
    // arcade auto-reverse (AUTO_BRAKE_REVERSE): in AUTO/1-GEAR, at a near-stop GAS picks DRIVE and
    // BRAKE picks REVERSE — so holding the brake past a stop backs you out of a jam without shifting.
    // Only fires on the transition (the gear guards stop it re-clunking). MANUAL is never arcade.
    if (AUTO_BRAKE_REVERSE && trans_mode != TR_MANUAL && engine_on && af(vf) < REV_ENGAGE_SPD) {
        if (in_gas && gear < 1)                  { gear = 1;  vf = 0; shift_snd = 1; }   // gas → DRIVE
        else if (in_brk && !in_gas && gear >= 1) { gear = -1; vf = 0; shift_snd = 1; }   // brake → REVERSE
    }
    if (trans_mode == TR_SINGLE && gear > 1) gear = 1;          // single keeps one forward gear
    if (trans_mode == TR_AUTO && gear >= 1) {                   // auto-shift to stay in the band
        if (rpm > AUTO_UP && gear < NGEAR) { gear++; shift_snd = 1; }
        else if (rpm < AUTO_DOWN && gear > 1) { gear--; shift_snd = 1; }
    }

    // --- transmission: ratio sets RPM + multiplies torque; reverse drives backward
    float curvref = ENG[eng_kind].vref;             // gearing per engine kind = the speed scale:
                                                    // top-gear redline = vref/0.72 is the hard ceiling
    float ratio = (trans_mode == TR_SINGLE) ? SINGLE_RATIO
                : (gear == -1) ? REV_RATIO
                : (gear <= 0)  ? GEAR_RATIO[0]              // neutral: nominal (thrust is zeroed below)
                : GEAR_RATIO[gear - 1];
    float gdir  = (gear == -1) ? -1.0f : 1.0f;
    if (gear == 0) {                                        // NEUTRAL — engine disconnected: free-revs
        float idle = IDLE_CREEP / V_REF;                   // ≈ the idle rpm
        rpm = lerp(rpm, !engine_on ? 0.0f : (in_gas ? 0.9f : idle), 0.12f);   // gas → vroom, no drive
    } else {
        rpm = clamp(af(vf) * ratio / curvref, 0, 1.15f);
    }
    // --- stall: lug a too-tall gear below idle revs while still rolling → it cuts out.
    // Only the COMBUSTION kinds (gas/diesel) stall — electric/nuclear have no idle to lose,
    // and steam is gated by its boiler, not revs. SINGLE and reverse never stall either.
    // Skipped briefly after a crank (RESTART_GRACE) so re-ignition always takes.
    int combusts = (eng_kind == EK_GAS || eng_kind == EK_DIESEL);
    if (restart_grace > 0) restart_grace -= dt_;
    if (engine_on && restart_grace <= 0 && combusts && trans_mode != TR_SINGLE && gear >= 1
        && rpm < STALL_RPM && af(vf) > VSTALL_MIN) {
        engine_on = 0; stalled = 1;
        hit(28, INSTR_NOISE, 3, 200);                 // the cough as it dies
    }
    if (!engine_on) rpm = 0;                           // dead engine → tach drops to zero
    // steam boiler: pressure builds while the engine runs (the spool-up), bleeds when off.
    // delivery(EK_STEAM,…) reads it, so a freshly-cranked steam rig is sluggish for ~4 s.
    if (engine_on) boiler = clamp(boiler + BOILER_RISE * dt_, 0, 1.0f);
    else           boiler = clamp(boiler - BOILER_FALL * dt_, 0, 1.0f);
    // the powerband per engine KIND × the gear ratio (low gear = more thrust, revs climb
    // fast). delivery() is the only kind-aware line; SINGLE folds its one flat ratio in here.
    float gmul = delivery(eng_kind, rpm, boiler) * ratio;

    // --- engine: thrust through the gear, + the yaw torque from an off-centre engine
    // arcade reverse swaps the pedals: BRAKE is the go-backward throttle, GAS is the slow-down pedal
    // (ease off / press gas to stop the reverse and, at a near-stop, return to DRIVE).
    int arcade_rev = (AUTO_BRAKE_REVERSE && trans_mode != TR_MANUAL && gear == -1);
    int gas_eff   = arcade_rev ? in_brk : in_gas;   // "drive in the gear's direction" pedal
    int brake_eff = arcade_rev ? in_gas : in_brk;   // "slow down" pedal
    // ramp the binary pedals into analog throttle/brake (same trick as steering): wind toward held,
    // ease off on release. Tuned fast (mass+gears already grade accel) — only the 100% spike is gone.
    float gtarg = gas_eff ? 1.0f : 0.0f, grate = (gas_eff ? THROTTLE_RATE : THROTTLE_RETURN) * dt_;
    if (gas_pos < gtarg) { gas_pos += grate; if (gas_pos > gtarg) gas_pos = gtarg; }
    else                 { gas_pos -= grate; if (gas_pos < gtarg) gas_pos = gtarg; }
    float btarg = brake_eff ? 1.0f : 0.0f, brate = (brake_eff ? BRAKE_RATE : BRAKE_RETURN) * dt_;
    if (brake_pos < btarg) { brake_pos += brate; if (brake_pos > btarg) brake_pos = btarg; }
    else                   { brake_pos -= brate; if (brake_pos < btarg) brake_pos = btarg; }
    float throttle = (engine_on && gear != 0) ? gas_pos : 0.0f;   // analog: neutral / dead engine = no drive
    float thrust = 0, eng_torque = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != P_ENGINE) continue;
            float t = ENG[eng_kind].power * throttle * gmul * gdir;
            float oy = (r + 0.5f) * CELL - comY;     // lateral offset of this engine
            thrust += t;
            eng_torque += -oy * t;                   // off the centre-line → it yaws
        }
    // traction caps thrust to what the DRIVE wheels can lay down — fewer powered
    // wheels = less grip to the ground = the engine can't deploy all its power.
    // §8: scale by the live LOAD on the drive wheels (last frame's spring solve), not a constant —
    // so weight transfer + cargo make traction emergent (squat-and-go, FWD wheelspin, overload = spin).
    float driveLoad = 0;
    for (int i = 0; i < nWheelP; i++)
        if (nDrive == 0 || wheelPD[i]) driveLoad += wheelLoad[i];       // nDrive==0 = AWD (all wheels)
    float tract = MU_TRACTION * gg * driveLoad;
    // wheelspin: engine force that EXCEEDS what the tyres can lay down is wasted as spin (no extra
    // accel, but it squeals + smokes + lays burnout marks). Worst in low gear (high thrust). Eased.
    float spinNow = (tract > 0 && af(thrust) > tract) ? clamp((af(thrust) - tract) / tract, 0, 1.5f) : 0;
    wheel_spin = lerp(wheel_spin, spinNow, 0.4f);
    if (af(thrust) > tract && tract > 0) {
        float s = tract / af(thrust);
        thrust *= s; eng_torque *= s;
    }
    float thrustLaid = af(thrust);                   // force actually put down (post-cap) — for combined slip

    // --- linear: net force / mass. Drag is a FORCE (base + per-wheel rolling
    //     resistance + frontal-profile aero), so top speed = thrust/drag is
    //     mass-INDEPENDENT — mass only governs how fast you reach it. -----------
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells + scrape_drag
               + (surf_grip < 1.0f ? OFFROAD_DRAG * nWheels : 0.0f);   // OSM off-road: rough ground drag
    vf += ((thrust - drag * vf) / M) * dt_;
    // braking: PURE deceleration, both directions (reverse is a gear now, not the brake).
    // Strong, but capped by what the tyres can grip — a well-wheeled rig stops hard.
    float brakeAccel = 0;
    if (brake_pos > 0.001f && af(vf) > 0.5f) {      // analog: brake force ∝ brake_pos (feathered / trail-brake)
        brakeAccel = clamp(GRIP_TO_FORCE * wheelGrip * gg / M, 0, BRAKE) * brake_pos;
        float d = brakeAccel * dt_;
        if (vf > 0) { vf -= d; if (vf < 0) vf = 0; }
        else        { vf += d; if (vf > 0) vf = 0; }
    }

    // --- tire grip: PER-AXLE, the two-axle ("bicycle") model -------------------
    // Each axle resists the lateral velocity AT ITS OWN POSITION (front/rear of the COM).
    // The two corrective forces act fore/aft of the COM, so they form a YAW COUPLE that
    // damps spin for free, scaling with grip·wheelbase²/I — a long heavy rig is naturally
    // calm, a short light one darty, no hand-tuned term. Symmetric grip + below the limit
    // reduces to the old single body bleed; front-vs-rear letting go INDEPENDENTLY
    // (understeer if the front goes, oversteer if the rear does) arrives with the
    // friction-circle cap (next step). Handbrake + tipping still scale both axles here.
    float tipMul = 1.0f - STAB_GRIP_LOSS * tip_amt;  // tipping unloads the tires → they let go
    slide_amt = 0;
    // weight transfer: the realized longitudinal accel this frame (thrust+drag+brake are already
    // in vf above) pitches load front↔rear, low-passed to settle like suspension. Computed HERE
    // (before the grip model) because BOTH the per-wheel spring solve and the old axle caps use it.
    float aLong = (vf - vf0) / dt_;
    wt_long = lerp(wt_long, aLong, WT_LAG);
    wt_xfer = clamp(wt_long * WT_LONG_K, -WT_MAX, WT_MAX);   // + = rear loads, - = front loads
    solve_wheel_loads(wt_long, aLat);                // §8: per-wheel vertical loads for THIS frame
    // combined-slip inputs: the longitudinal force each wheel is carrying this frame — drive force
    // (its share of the laid-down thrust) + brake force (even split). Used by the friction circle
    // below to shrink that wheel's LATERAL grip (power-/brake-eats-cornering) — the friction circle.
    float nDriveActive = (nDrive > 0) ? (float)nDrive : (float)nWheels;
    float fxDrive = (nDriveActive > 0) ? thrustLaid / nDriveActive : 0;
    float fxBrake = (nWheels > 0) ? (brakeAccel * M) / nWheels : 0;
    if (nHull >= 3) {
        // ── §8: per-wheel lateral force resolution — THE lateral core ────────────
        // Each wheel resists the lateral slip AT ITS POSITION, capped by a friction circle sized
        // by ITS OWN load (from the spring solve — so longitudinal + lateral transfer AND static
        // cargo all bias grip per wheel). A lifted wheel (load→0) makes ~no grip = tipping, emergent
        // (no tipMul here). Handbrake cuts the rear wheels; a driven wheel eats its own grip under
        // power. Sum forces → lateral accel; sum force×lever → the yaw couple.
        float wRad = angVel * DEG2RAD;
        float avgLoad = M / nWheelP;
        float sumLat = 0, sumYaw = 0, satF = 0, satR = 0;
        for (int i = 0; i < nWheelP; i++) {
            float g = wheelG[i];
            if (g <= 0) continue;
            float vlat = vl + wRad * wheelPX[i];               // lateral slip velocity at this wheel
            float loadScale = wheelLoad[i] / avgLoad;          // >1 loaded, ~0 lifted
            float cap = SLIP_MAX * loadScale;
            if (wheelPX[i] < 0 && in_hand) cap *= DRIFT_GRIP_MULT;                 // handbrake = the rear wheels
            // friction circle: the longitudinal force (drive + brake) this wheel carries eats into its
            // LATERAL budget. latFactor = √(1−(Fx/Fmax)²): flooring a drive wheel → it lets go laterally
            // (power-oversteer); braking an unloaded rear → it steps out (trail-braking). All emergent.
            float Fmax = MU_TRACTION * gg * wheelLoad[i] + 1.0f;
            float fx = fxBrake + ((wheelPD[i] || nDrive == 0) ? fxDrive : 0);
            float u = clamp(fx / Fmax, 0, 1);
            cap *= fsqrt(1.0f - u * u);
            if (wheelPX[i] < 0) cap *= (1.0f - DRIFT_RECOVER * drift_loose);       // hysteresis: rear hangs loose on exit
            float cl = clamp(vlat, -cap, cap);
            float acc = cl * (g * gg / M) * LAT_GRIP;  // peak force ∝ cap ∝ load
            sumLat += acc;
            sumYaw += acc * wheelPX[i];
            float sat = af(vlat) - cap;
            if (sat > 0) { if (wheelPX[i] >= 0) { if (sat > satF) satF = sat; }
                           else                { if (sat > satR) satR = sat; } }
        }
        vl     -= sumLat * dt_;
        angVel -= sumYaw * (M / I) * GRIP_YAW_K / DEG2RAD * dt_;
        slide_rear = (satR >= satF);
        float sat = (satR > satF) ? satR : satF;
        slide_amt = clamp(sat / (SLIP_MAX + 1.0f), 0, 1);
        // drift-exit hysteresis: a real rear BREAKAWAY (slide past DRIFT_TRIG) spikes the looseness
        // (instant attack); it then bleeds off slowly, so the rear keeps a touch less grip on the way
        // out → the tail trails, not snaps. Gated above DRIFT_TRIG so normal hard cornering doesn't loosen.
        float over = slide_amt - DRIFT_TRIG;
        float looseTgt = (slide_rear && over > 0) ? clamp(over / (1.0f - DRIFT_TRIG), 0, 1) : 0.0f;
        if (looseTgt > drift_loose) drift_loose = looseTgt;
        else drift_loose += (0.0f - drift_loose) * DRIFT_DECAY;
    } else {                                          // single-track (bike): whole-body grip bleed
        drift_loose = 0;
        float lat_mult = (in_hand ? DRIFT_GRIP_MULT : 1.0f) * tipMul;
        float grip = clamp((wheelGrip * gg / M) * LAT_GRIP * lat_mult, 0, 1.0f / dt_);
        vl -= vl * grip * dt_;
    }
    if (scraping)                                    // a dragging belly also anchors sideways
        vl -= vl * clamp((SCRAPE_LAT * nDrag) / M, 0, 1.0f / dt_) * dt_;

    // --- rolling friction: a CONSTANT decel (rolling/bearing resistance) that drag ∝ v
    //     can't provide — it's what actually brings a coasting rig to a full STOP and
    //     snaps the last creep to zero (no more floaty asymptote). Engine thrust easily
    //     overcomes it, so it barely touches accel/top speed; it bites at low speed.
    float roll = ROLL_FRIC * dt_;
    if (vf >  roll) vf -= roll; else if (vf < -roll) vf += roll; else vf = 0;
    if (vl >  roll) vl -= roll; else if (vl < -roll) vl += roll; else vl = 0;

    // --- idle creep: throttle released, in gear, no brake → the idling engine trundles you
    //     at a gear-set floor (taller gear = faster, capped). Only pulls UP to the floor (it
    //     won't fight drag from above). Brake overrides → sit still (a manual at a light).
    if (gas_pos < 0.02f && brake_pos < 0.02f && engine_on && gear != 0) {   // both pedals off → coast/creep
        // idle holds a CONSTANT rpm (IDLE_CREEP/V_REF) whatever the gearing → creep speed scales
        // with vref, so a tall-geared supercar idles faster and a short-geared tractor crawls,
        // and neither false-stalls (idle rpm stays above STALL_RPM regardless of vref).
        float vcreep = IDLE_CREEP * (curvref / V_REF) / ratio;   // idle RPM in this gear → 1/ratio law
        if (gear == -1) {                                 // reverse idles backward
            if (vf > -vcreep) { vf -= CREEP_ACCEL * dt_; if (vf < -vcreep) vf = -vcreep; }
        } else if (vf >= 0 && vf < vcreep) {              // forward: ease up to the floor
            vf += CREEP_ACCEL * dt_; if (vf > vcreep) vf = vcreep;
        }
    }

    // recombine
    vx = fwx * vf + ltx * vl;
    vy = fwy * vf + lty * vl;
    sx += vx * dt_; sy += vy * dt_;
    collide_world();        // §9: run over / crash into world obstacles (may bleed vx/vy)

    // --- skid marks: lay at every wheel while the tires scrub sideways OR lock under a
    //     hard brake at speed (a straight stopping skid, laid frame-by-frame as you slide) -
    int hard_brake = brake_pos > 0.5f && af(vf) > BRAKE_SKID_SPD;   // analog: only a firm brake locks up
    if (af(vl) > SKID_SLIP || hard_brake)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++)
                if (grid[r][c] == P_WHEEL || grid[r][c] == P_DRIVE) {
                    float wx, wy;
                    rot((c + 0.5f) * CELL - comX, (r + 0.5f) * CELL - comY, &wx, &wy);
                    lay_skid(wx, wy);
                }
    // wheelspin → burnout marks + tyre smoke at the DRIVE wheels (they're scrabbling for grip)
    if (wheel_spin > SPIN_SQUEAL)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++)
                if (grid[r][c] == P_DRIVE || (nDrive == 0 && grid[r][c] == P_WHEEL)) {
                    float wx, wy;
                    rot((c + 0.5f) * CELL - comX, (r + 0.5f) * CELL - comY, &wx, &wy);
                    lay_skid(wx, wy);
                    if (rnd_float() < clamp(wheel_spin, 0.12f, 0.7f)) smoke_puff(wx, wy);
                }
    for (int i = 0; i < MAXSKID; i++) if (skid[i].life > 0) skid[i].life--;

    // --- steering: torque about the COM, scaled by how fast you're going -------
    // ramp the binary key (-1/0/+1) into an analog steer angle: wind toward the held
    // direction at STEER_RATE, ease back to centre at STEER_RETURN when released. A quick
    // opposite tap backs the lock off a notch — the fine counter-steer a drift needs.
    if (steer_grab) {                                // wheel grabbed: its rotation IS the steer angle
        steer_pos = clamp(wheel_turn / WHEEL_LOCK, -1.0f, 1.0f);
    } else {
        float starg = (float)in_steer;
        float srate = (in_steer != 0 ? STEER_RATE : STEER_RETURN) * dt_;
        if (steer_pos < starg) { steer_pos += srate; if (steer_pos > starg) steer_pos = starg; }
        else if (steer_pos > starg) { steer_pos -= srate; if (steer_pos < starg) steer_pos = starg; }
    }
    float speed_factor = clamp(af(vf) / 50.0f, 0, 1);
    float dir = vf >= 0 ? 1.0f : -1.0f;
    float gyro = I / M;                              // radius of gyration squared
    float turnEase = REF_GYRO / (gyro + REF_GYRO);   // small/light rig → turns easier
    // weight balance: nose-heavy (balance>0) pushes wide (understeer), tail-heavy
    // (balance<0) turns in eagerly (oversteer). Uses the COM we already derive.
    float steer_bal = 1.0f - BALANCE_K * balance;
    float ang_acc = steer_pos * STEER_RESP * speed_factor * dir * turnEase * steer_bal;
    ang_acc += ENG_YAW_K * (eng_torque / I);         // off-centre engine pulls
    if (scraping) ang_acc += SCRAPE_YAW * (scrape_torque / I) * dir;  // off-centre scrape drags
    angVel += ang_acc * dt_;
    angVel -= angVel * (twoAxle ? ANG_DAMP_AXLE : ANG_DAMP) * dt_;   // axle rigs lean on the couple too
    // self-aligning torque (caster/trail): rotate the heading toward the travel direction,
    // ∝ slip angle + speed. Gentle when tracking straight; in a big slide it catches the
    // spin and settles the rig into a held drift angle (which throttle then trims). Only
    // forward (vf>0) — reversing has no meaningful caster self-steer.
    if (vf > VSTALL_MIN) {
        float sp2 = fsqrt(vf * vf + vl * vl);
        float slipFrac = clamp(vl / sp2, -SELF_ALIGN_SIN, SELF_ALIGN_SIN);   // sin(slip angle), bounded
        angVel += SELF_ALIGN_K * slipFrac * STEER_RESP * speed_factor * dt_; // rotate heading toward travel
    }
    // directional stability: the drive point ahead of the COM (in the travel frame)
    // PULLS the rig → extra self-centering (stable, understeer); behind it PUSHES →
    // anti-damping (the heavy end wants to swing round → oversteer / spin). Reversing
    // flips which end leads, so a rear-drive rig is calmer backwards — and a one-wheel
    // "bike" genuinely drives better in reverse (wheel leads, the bare stub trails).
    float driveOff = clamp(driveX - comX, -DRIVE_OFF_MAX, DRIVE_OFF_MAX);  // >0 = ahead of COM
    float lead = driveOff * dir;                     // travel frame: >0 pull (lead), <0 push (trail)
    // fade the push anti-damping as the slide saturates: it makes RWD loose enough to KICK
    // out, but if it kept amplifying yaw through a full slide it'd wind every drift into a
    // spin faster than the self-align can catch. Past the limit, the slide is the boss.
    angVel -= STAB_YAW_K * lead * angVel * clamp(spd0 / 45.0f, 0, 1)
              * (1.0f - DRIFT_PUSH_FADE * slide_amt) * dt_;
    ang += angVel * dt_;
    if (ang < 0) ang += 360; else if (ang >= 360) ang -= 360;

    // steering-wheel visual: the rim rotates with the steer angle — full lock = a quarter
    // turn, so a grabbed finger drags the wheel ~1:1 (negative so it follows the finger /
    // turns the way you steer). Snappier while grabbed so it tracks the hand tightly.
    wheel_ang = lerp(wheel_ang, -steer_pos * WHEEL_LOCK, steer_grab ? 0.6f : 0.3f);

    // --- sound -----------------------------------------------------------------
    // The engine itself is ONE sustained voice driven live (see engine_sound() in
    // update) — not a re-triggered beep. Here we only fire the transient one-shots.
    float spd = fsqrt(vx * vx + vy * vy);
    if (shift_snd) { hit(40, INSTR_NOISE, 2, 45); shift_snd = 0; }   // gear-change clunk
    if (af(vl) > 35 || hard_brake) {                 // tires scrubbing sideways OR locked braking
        t_skid_snd -= dt_;
        if (t_skid_snd <= 0) { hit(hard_brake ? 48 : 54, INSTR_NOISE, 2, 70); t_skid_snd = 0.05f; }
    }
    if (scraping) {                                  // a low grinding scrape, pitch tracks speed
        t_scrape_snd -= dt_;
        if (t_scrape_snd <= 0) { hit(36 + (int)(spd * 0.06f), INSTR_NOISE, 2, 60); t_scrape_snd = 0.045f; }
    }
    if (wheel_spin > SPIN_SQUEAL) {                  // tyres spinning — a high, scrabbling squeal
        t_spin_snd -= dt_;
        if (t_spin_snd <= 0) { hit(60 + (int)(wheel_spin * 8), INSTR_NOISE, 2, 50); t_spin_snd = 0.05f; }
    }

    // --- heat: rises while the belly grinds under load, cools off otherwise -----
    if (scraping) heat = clamp(heat + HEAT_RISE * nDrag * dt_, 0, 1.0f);
    else          heat = clamp(heat - HEAT_COOL * dt_, 0, 1.0f);

    // --- tick sparks (top-down: no gravity, just air-drag + fade) ---------------
    for (int i = 0; i < MAXSPARK; i++)
        if (spark[i].life > 0) {
            spark[i].x += spark[i].vx * dt_;
            spark[i].y += spark[i].vy * dt_;
            spark[i].vx *= 0.90f; spark[i].vy *= 0.90f;
            spark[i].life--;
        }
}

// ── engine audio: ONE sustained voice, driven live ───────────────────────────
// A re-triggered hit() every 80ms is the PC-speaker buzz. Instead we hold a single
// note and steer it: a saw through a resonant lowpass (the muffled body), pitch
// tracking the revs (so it climbs in a gear and drops on each upshift), the filter
// opening as you rev/throttle (bright under power, muffled at idle), and a tremolo
// for the engine throb. It plays the whole time the engine runs — including the low
// idle/creep rumble — and stops (note_off) when it stalls, keys off, or you pause.
#define INSTR_ENGINE 9
static int eng_voice = -1;

static void engine_sound_init(void) {
    instrument(INSTR_ENGINE, INSTR_SAW, 40, 0, 7, 250);        // a held drone (full sustain)
    instrument_filter(INSTR_ENGINE, FILTER_LOW, 350, 7);       // muffled + a little resonant growl
    instrument_lfo(INSTR_ENGINE, 0, LFO_VOLUME, 9.0f, 0.4f);   // the throb
}
static void engine_sound(int audible) {
    if (!audible) { if (eng_voice >= 0) { note_off(eng_voice); eng_voice = -1; } return; }
    float r = clamp(rpm, 0, 1.0f);
    // per-kind voice (§1a): base pitch + span + brightness. Electric whines high & open;
    // diesel/steam growl low & muffled; nuclear a steady mid hum; gas the mid default.
    // (SOUND only — power/mass/gearing/curve live in the "ENGINE TUNING" block in init().)
    float pbase, pspan; int cbase, bright;
    switch (eng_kind) {
    case EK_ELECTRIC: pbase = 34; pspan = 28; cbase = 520; bright = 1; break;   // high, open whine
    case EK_DIESEL:   pbase = 19; pspan = 22; cbase = 160; bright = 0; break;   // low, muffled growl
    case EK_STEAM:    pbase = 18; pspan = 18; cbase = 140; bright = 0; break;   // low, soft chuff
    case EK_NUCLEAR:  pbase = 28; pspan = 22; cbase = 320; bright = 0; break;   // steady mid hum
    case EK_RACE:     pbase = 32; pspan = 38; cbase = 480; bright = 1; break;   // high, screaming wail
    case EK_TRACTOR:  pbase = 15; pspan = 14; cbase = 120; bright = 0; break;   // very low, lugging chug
    case EK_GAS: default: pbase = 24; pspan = 28; cbase = 220; bright = 0; break;
    }
    float pitch = pbase + r * pspan;                           // idle low, climbs to redline
    int   vol   = gas_pos > 0.15f ? 5 : 3;                     // idle/creep quieter than under power
    int   cut   = cbase + (int)((r * (bright ? 0.9f : 0.75f) + gas_pos * 0.25f) * 1500);  // brightens with throttle
    if (eng_voice < 0) { eng_voice = note_on((int)pitch, INSTR_ENGINE, vol); note_glide(eng_voice, 70); }
    note_pitch (eng_voice, pitch);                             // glided → smooth rev tracking
    note_vol   (eng_voice, vol);
    note_cutoff(eng_voice, cut);
}

void update(void) {
    static int snd_ready = 0;
    if (!snd_ready) { engine_sound_init(); snd_ready = 1; }

    // Rung B/C run-path: DRAG a .rvb (data/delft-centre.rvb, …) onto the window → drive that real
    // city. Same de_dropped_file() hook as roadview; a valid load respawns the rig on a road. Press
    // O to reveal the data folder in Finder. (Startup --data / $DE_DATA is honoured in init().)
    { const char *dropped = de_dropped_file();
      if (dropped) { int was = osm_loaded; osm_load(dropped); if (osm_loaded || was) reset_vehicle(); } }
    if (keyp('O')) de_open_path("../data");
    if (keyp('N')) rn2_toggle();               // Rung A1: stub grid ↔ the roadnet2 spine
    if (keyp('M')) cg_toggle();                // Rung 5.5c: drive a generated citygen city

    handle_input();
    if (foot_deny > 0) foot_deny--;
    if (mode != MODE_FOOT) foot_house = -1;        // F2: roofs stay on unless you're in there on foot
    engine_sound(mode == MODE_DRIVE && !is_paused && engine_on);   // every frame (also kills it in BUILD/pause)
    if (mode == MODE_BUILD || is_paused) return;   // BUILD pauses the simulation
    float dt_ = dt(); if (dt_ > 0.05f) dt_ = 0.05f;
    if (mode == MODE_FOOT) update_foot(dt_);       // rung F0: the rig is parked; you're the sim now
    else                   update_drive(dt_);

    // speed-zoom: pull the camera back as you go faster — more world streams through the frame
    // (and you see further ahead). Eased slowly so it never jitters; resets to 1 in BUILD/at rest.
    // Computed BEFORE the lead so the lead cap below can work in screen space.
    // On foot camspd is 0 by construction (the rig is parked), so the zoom eases back to 1:1 —
    // the walking view is the close view, no extra camera tier needed (a 7px avatar reads at 1:1).
    float camspd = fsqrt(vx * vx + vy * vy);
    float zoomTarget = 1.0f - CAM_ZOOM_PULL * clamp(camspd / CAM_ZOOM_REF, 0, 1);
    if (mode == MODE_FOOT && foot_house >= 0) zoomTarget = FOOT_ZOOM_IN;   // F2: under a lifted roof,
                                                   // ease into the interior tier (rooms are ~14px)
    cam_zoom_smooth = lerp(cam_zoom_smooth, zoomTarget, 0.05f);
    if (smooth_mode) {
        // smooth_zoom handles the resample at 1:1, so a continuous fractional zoom no longer
        // crawls — drop the quantization (and its stepping pops) and ease freely.
        cam_zoom = cam_zoom_smooth;
    } else {
        // OFF (baseline): snap to the step grid so thin world lines hold steady between levels.
        // Position is already pixel-snapped ((int)cam_x/cam_y). Mild even scroll-crawl remains.
        cam_zoom = ((int)(cam_zoom_smooth / CAM_ZOOM_STEP + 0.5f)) * CAM_ZOOM_STEP;
    }

    // camera LEADS the rig in the travel direction (you see where you're rushing into —
    // reads as speed). The lead is HEAVILY low-passed (lead_x/y ease toward vx/vy) so it
    // doesn't jitter the bright curbs frame-to-frame or snap through the city's 90° corners.
    lead_x = lerp(lead_x, vx * CAM_LEAD, 0.04f);
    lead_y = lerp(lead_y, vy * CAM_LEAD, 0.04f);
    // CAP the lead in SCREEN space: past LEAD_MAX the rig would slide off-frame / under the dash
    // (the 300 km/h disappearing-car bug). Scale the world-space lead so its on-screen length holds.
    float lsx = lead_x * cam_zoom, lsy = lead_y * cam_zoom, ll = fsqrt(lsx * lsx + lsy * lsy);
    if (ll > LEAD_MAX) { float k = LEAD_MAX / ll; lead_x *= k; lead_y *= k; }
    float focx = sx + lead_x, focy = sy + lead_y;                  // camera focus: the rig...
    if (mode == MODE_FOOT) { focx = foot_x; focy = foot_y; }       // ...or the avatar, on foot
    cam_x = lerp(cam_x, focx - SCREEN_W / 2.0f, 0.15f);
    cam_y = lerp(cam_y, focy - SCREEN_H / 2.0f, 0.15f);

    // heading-up rotation: spin the world so the rig's heading points UP. Eased toward the
    // target = the GLIDE — a hard drift swings the nose fast while cam_ang trails, so the rig
    // slides sideways across the frame, then the world swings round to catch it. NORTH-UP
    // eases the angle back to 0 (so the toggle itself glides, never snaps).
    float ang_targ = (cam_head_up && mode != MODE_FOOT) ? (-90.0f - ang) : 0.0f;   // on foot: ease to north-up (arrows = world axes)
    float dA = ang_targ - cam_ang;
    while (dA >  180.0f) dA -= 360.0f;
    while (dA < -180.0f) dA += 360.0f;
    cam_ang += dA * CAM_GLIDE;
    if      (cam_ang >  180.0f) cam_ang -= 360.0f;
    else if (cam_ang < -180.0f) cam_ang += 360.0f;

    world_sync();                  // §9: stream chunks (load/evict) for the new camera
    obstacles_integrate(dt_);      // knocked obstacles tumble away and settle
    collide_obstacles_chain();     // §9f: a moving obstacle shoves the next → chain reaction through the lot

#ifdef DE_TRACE
    float fwx = cos_deg(ang), fwy = sin_deg(ang);
    watch("vf", "%.1f", vx * fwx + vy * fwy);
    { RoadHit _rh = road_at(sx, sy);   // P1 seam — deterministic trace of road_at() AT THE RIG (drives handling)
      watch("on_road", "%d", _rh.on_road);
      watch("surf_grip", "%.2f", surf_grip);
      watch("road_cls", "%d", _rh.cls);
      watch("osm", "%d", osm_loaded);
      watch("rn2", "%d", rn2_on); watch("cg", "%d", cg_on); }
    watch("vl", "%.1f", vx * (-fwy) + vy * fwx);
    watch("foot", "%d", mode == MODE_FOOT);        // rung F0: the car↔foot seam, traced
    watch("foot_x", "%.1f", foot_x);
    watch("foot_y", "%.1f", foot_y);
    { RoadHit _fh = road_at(foot_x, foot_y);       // rung F1: the ground under the feet
      watch("foot_road", "%d", _fh.on_road);
      watch("foot_pave", "%d", _fh.on_pave); }
    watch("foot_house", "%d", foot_house);         // rung F2: pool index of the lifted roof (-1 = outside)
    watch("ang", "%.0f", ang);
    watch("angvel", "%.0f", angVel);
    watch("cam_ang", "%.0f", cam_ang);
    watch("cam_headup", "%d", cam_head_up);
    watch("cam_zoom", "%.2f", cam_zoom);
    watch("mass", "%.1f", M);
    watch("I", "%.0f", I);
    watch("front", "%d", frontalCells);
    watch("bal", "%.2f", balance);
    watch("ndrag", "%d", nDrag);
    watch("heat", "%.2f", heat);
    watch("tip", "%.2f", tip_amt);
    watch("stabL", "%.1f", stabL);
    watch("stabR", "%.1f", stabR);
    watch("ndrive", "%d", nDrive);
    watch("driveoff", "%.1f", driveX - comX);
    watch("gear", "%d", gear);
    watch("rpm", "%.2f", rpm);
    watch("trans", "%d", trans_mode);
    watch("engine_on", "%d", engine_on);
    watch("stalled", "%d", stalled);
    watch("ekind", "%d", eng_kind);
    watch("boiler", "%.2f", boiler);
    watch("fgrip", "%.1f", frontGrip);
    watch("rgrip", "%.1f", rearGrip);
    watch("aF", "%.1f", aF);
    watch("aR", "%.1f", aR);
    watch("2axle", "%d", twoAxle);
    watch("slide", "%.2f", slide_amt);
    watch("slidR", "%d", slide_rear);
    watch("wt", "%.2f", wt_xfer);
    watch("steer", "%.2f", steer_pos);
    watch("gasp", "%.2f", gas_pos);
    watch("brkp", "%.2f", brake_pos);
    {   // per-wheel load distribution (§8): front/rear + right/left sums
        // NB: +x = front (frontX side), +y = the vehicle's RIGHT (screen-y is down here)
        float lF = 0, lR = 0, lRight = 0, lLeft = 0;
        for (int i = 0; i < nWheelP; i++) {
            if (wheelPX[i] >= 0) lF += wheelLoad[i]; else lR += wheelLoad[i];
            if (wheelPY[i] >= 0) lRight += wheelLoad[i]; else lLeft += wheelLoad[i];
        }
        watch("loadF", "%.1f", lF);    watch("loadR", "%.1f", lR);
        watch("loadRt", "%.1f", lRight); watch("loadLf", "%.1f", lLeft);
    }
    watch("loose", "%.2f", drift_loose);
    watch("spin", "%.2f", wheel_spin);
    {   // §9 world: live obstacle count + a run-over pulse (the frame a cone is hit)
        int nob = 0; for (int i = 0; i < MAXOB; i++) if (pool[i].alive) nob++;
        watch("nob", "%d", nob);
        watch("ohit", "%d", last_hit);
    }
#endif
}

// ── world: a single biome (road) that scrolls under you ──────────────────────
static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// world-space HALF-EXTENTS of the on-screen viewport, accounting for BOTH the speed-zoom
// (cam_zoom<1 shows more world) AND the heading-up rotation (a rotated screen rectangle's
// axis-aligned bounding box is up to ~1.4× wider). Every draw/stream range widens by this so
// nothing pops at the screen edges or in the corners when the camera is turned. At cam_ang=0
// it reduces to the old SCREEN_W/2 / cam_zoom margin (north-up is byte-identical to before).
static void view_half_extent(float *hw, float *hh) {
    float ca = af(cos_deg(cam_ang)), sa = af(sin_deg(cam_ang));
    *hw = (ca * SCREEN_W + sa * SCREEN_H) * 0.5f / cam_zoom;
    *hh = (sa * SCREEN_W + ca * SCREEN_H) * 0.5f / cam_zoom;
}

static void draw_ground(void) {
    int step = 32;
    // widen the draw range so the zoomed-out / rotated edges aren't left undrawn (see helper).
    float hw, hh; view_half_extent(&hw, &hh);
    int mx = (int)(hw - SCREEN_W * 0.5f) + step;
    int my = (int)(hh - SCREEN_H * 0.5f) + step;
    int x0 = ((int)(cam_x - mx) / step - 1) * step;
    int y0 = ((int)(cam_y - my) / step - 1) * step;
    int x1 = (int)cam_x + SCREEN_W + mx;
    int y1 = (int)cam_y + SCREEN_H + my;
    // grid of asphalt seams — gives motion + a rotation reference
    for (int x = x0; x <= x1; x += step) line(x, y0, x, y1, CLR_DARK_GREY);
    for (int y = y0; y <= y1; y += step) line(x0, y, x1, y, CLR_DARK_GREY);
    // deterministic speckle — at speed each fleck STREAKS opposite travel (motion blur),
    // the strongest sense-of-speed cue and tied to the rig's actual velocity.
    float spd = fsqrt(vx * vx + vy * vy);
    float sl = clamp(spd * 0.13f, 0, 20.0f);         // streak length (px) — longer = stronger speed cue
    float ux = 0, uy = 0;
    if (spd > 1.0f) { ux = vx / spd; uy = vy / spd; }
    for (int x = x0; x <= x1; x += step)
        for (int y = y0; y <= y1; y += step) {
            unsigned h = hash2(x / step, y / step);
            if ((h & 7) == 0) {
                int px = x + (int)(h >> 4) % step, py = y + (int)(h >> 12) % step;
                int col = (h & 8) ? CLR_MEDIUM_GREY : CLR_BROWN;
                if (sl > 1.5f) line(px, py, px - (int)(ux * sl), py - (int)(uy * sl), col);
                else pset(px, py, col);
            }
        }
}

// ── the course: a schematic DUTCH road hierarchy, zoned by distance from origin ─
// Purely VISUAL for now (nothing collides — rung 3 makes it solid). Driving OUTWARD
// from spawn walks you up the hierarchy: tight 30-zone city grid (narrow streets,
// houses packed everywhere → lots of detail streaking past = feels fast even slow) →
// 50-zone town with roundabouts → open 80 rural straights with fields → 100 highway →
// 120 superhighway (wide, multi-lane, very long straights). Pitch (block spacing) and
// lane width grow per ring, so the roads thin and lengthen the faster the zone. Block
// pitches are origin-anchored multiples so coarser roads nest inside finer ones (the
// grid thins rather than jumps at a ring boundary). Deterministic → stable + headless.
enum { Z_CITY, Z_TOWN, Z_RURAL, Z_HWY, Z_SUPER, Z_N };
static const int   ZONE_PITCH[Z_N] = { 150, 300, 600, 1200, 2400 };  // block spacing (px) — city/town
                                                                     // enlarged so rows of 5 m houses fit
                                                                     // (still 2× steps → roads nest cleanly)
static const int   ZONE_LANE[Z_N]  = { 26,  50,  50,  76,   104   }; // road width: city = 1 lane,
                                                                     // town/rural = 2 lanes (~2×),
                                                                     // hwy wider, super widest
static const float ZONE_R[Z_N]     = { 1800.f, 4500.f, 8500.f, 15000.f, 1e9f }; // outer radius
static const char *ZONE_NAME[Z_N]  = { "CITY 30", "TOWN 50", "RURAL 80", "HWY 100", "SUPER 120" };
static int cur_zone;               // set each frame in draw_course, read by the HUD

static int ifloordiv(int a, int b) {
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}

// ══ collidable world (§9) ════════════════════════════════════════════════════
// The chunk machine: baseline-from-seed + sparse dirty deltas + a live ring.
// Determinism note: placement + collision response are all derived from the seed
// and the (deterministic) rig motion — no rnd_float in the physics path, so --det
// replay holds. Only the cosmetic debris sparks use rnd.

// derive a cone/house's footprint + collision numbers from its cell grid (the seam:
// MVP sums to a rigid solid; demolition later reads the same per-cell mass/strength).
static void ob_derive(Obstacle *o) {
    o->mass = 0; o->strength = 0;
    for (int r = 0; r < o->gh; r++)
        for (int c = 0; c < o->gw; c++) {
            int m = o->cell[r][c];
            if (m) { o->mass += OM[m].mass; o->strength += OM[m].strength; }
        }
    o->rad = fsqrt(o->hw * o->hw + o->hh * o->hh);   // broad-phase radius covers the box corners
}

// start a blank obstacle (common fields) — caller sets kind/pose/cells then calls ob_derive.
static void ob_init(Obstacle *o, int cx, int cy, int idx) {
    o->alive = 1; o->dirty = 0; o->destroyed = 0;
    o->cx = cx; o->cy = cy; o->idx = idx;
    o->ang = 0; o->vx = o->vy = o->vr = 0;
    o->door = -1;                 // F2: probed lazily (road_at on each wall) the first time it's needed
    o->gw = 1; o->gh = 1;
    for (int r = 0; r < OB_GH; r++)
        for (int c = 0; c < OB_GW; c++) o->cell[r][c] = OM_NONE;
}

// regenerate a chunk's BASELINE obstacles from the seed (deterministic, identical every
// time). Returns the count; fills out[] (idx = the stable per-chunk index = the delta key).
// idx is just the running emit order k — stable because gen order is fixed (cones, then houses).
static int gen_chunk(int cx, int cy, Obstacle *out) {
    int k = 0;
    if (rn2_on || cg_on) return 0;            // Rung A1/5.5c: nothing solid over the spine/city yet — buildings
                                              // arrive with the content rung (A2), on the clean graph
    if (osm_loaded) {                         // Rung C: emit the real OSM buildings whose home chunk is this one
        int roof[4] = { CLR_BROWN, CLR_RED, CLR_DARK_PURPLE, CLR_DARK_GREY };
        for (int b = 0; b < n_obld && k < OB_PERCHUNK; b++) {
            if (obld[b].cx != cx || obld[b].cy != cy) continue;
            Obstacle o; ob_init(&o, cx, cy, k);
            o.kind = OB_HOUSE;                                    // immovable, solid — holds till a heavy hit smashes it
            o.bx = o.x = obld[b].wx; o.by = o.y = obld[b].wy;
            o.hw = obld[b].hw; o.hh = obld[b].hh; o.ang = obld[b].ang;
            o.gw = 3; o.gh = 3;                                   // 3×3 brick → house-like mass/strength (ob_derive)
            for (int rr = 0; rr < 3; rr++) for (int cc = 0; cc < 3; cc++) o.cell[rr][cc] = OM_BRICK;
            o.col = roof[b & 3];
            ob_derive(&o); out[k++] = o;
        }
        return k;                             // (no stub cones/cars over Delft; roads are drawn by draw_course)
    }
    int x0 = cx * CHUNK, y0 = cy * CHUNK;     // this chunk's world span [x0,x0+CHUNK)

    // 1. cones — light roadworks clutter, scattered anywhere (run-over class)
    unsigned h = hash2(cx, cy);
    int ncone = h % 4;                        // 0..3 per chunk
    for (int i = 0; i < ncone && k < 60; i++) {
        unsigned hi = hash2(cx * 131 + i * 17, cy * 131 + 7);
        Obstacle o; ob_init(&o, cx, cy, k);
        o.kind = OB_CONE;
        o.bx = (float)(x0 + (int)(hi % CHUNK));
        o.by = (float)(y0 + (int)((hi >> 9) % CHUNK));
        o.x = o.bx; o.y = o.by; o.ang = (float)((hi >> 18) % 360);
        o.hw = o.hh = 0.5f * OB_CELL;         // ~3.5px footprint
        o.cell[0][0] = OM_CONE; o.col = OM[OM_CONE].col;
        ob_derive(&o); out[k++] = o;
    }

    // 2. houses (crash class) — only in CITY/TOWN zones. Tiled into block interiors exactly as
    //    draw_houses did, but emitted as solid obstacles. Each house is owned by the chunk its
    //    CENTRE falls in (so no double-emit across chunk/block borders).
    int zone = zone_at(x0 + CHUNK * 0.5f, y0 + CHUNK * 0.5f);
    // the spawn PARKING LOT: chunks near the origin get NO houses and a dense grid of cars instead
    // (section 4 below) — a big billiard playground right where you start.
    int lot = (cx >= -PARKING_CH && cx <= PARKING_CH && cy >= -PARKING_CH && cy <= PARKING_CH);
    if (!lot && (zone == Z_CITY || zone == Z_TOWN)) {
        int p = ZONE_PITCH[zone], hwr = ZONE_LANE[zone] / 2;
        int bx0 = ifloordiv(x0, p) - 1, bx1 = ifloordiv(x0 + CHUNK, p) + 1;
        int by0 = ifloordiv(y0, p) - 1, by1 = ifloordiv(y0 + CHUNK, p) + 1;
        for (int bx = bx0; bx <= bx1; bx++)
            for (int by = by0; by <= by1; by++) {
                int lx0 = bx * p + hwr + 3, lx1 = (bx + 1) * p - hwr - 3;   // block interior (off the roads)
                int ly0 = by * p + hwr + 3, ly1 = (by + 1) * p - hwr - 3;
                int nx = (lx1 - lx0) / HOUSE_FACADE, ny = (ly1 - ly0) / HOUSE_FACADE;
                if (nx < 1 || ny < 1) continue;
                int ox = lx0 + ((lx1 - lx0) - nx * HOUSE_FACADE) / 2;
                int oy = ly0 + ((ly1 - ly0) - ny * HOUSE_FACADE) / 2;
                int roof[4] = { CLR_BROWN, CLR_RED, CLR_DARK_PURPLE, CLR_DARK_GREY };
                for (int i = 0; i < nx && k < OB_PERCHUNK; i++)
                    for (int j = 0; j < ny && k < OB_PERCHUNK; j++) {
                        unsigned hh2 = hash2(bx * 131 + i, by * 131 + j);
                        if ((hh2 & 7) == 0) continue;                  // empty lots / yards
                        float side = HOUSE_FACADE - 3;                 // drawn house size
                        float ccx = ox + i * HOUSE_FACADE + 1 + side * 0.5f;
                        float ccy = oy + j * HOUSE_FACADE + 1 + side * 0.5f;
                        if (ifloordiv((int)ccx, CHUNK) != cx || ifloordiv((int)ccy, CHUNK) != cy) continue;  // not ours
                        Obstacle o; ob_init(&o, cx, cy, k);
                        o.kind = OB_HOUSE;
                        o.bx = o.x = ccx; o.by = o.y = ccy;
                        o.hw = o.hh = side * 0.5f;
                        o.gw = 3; o.gh = 3;                            // 3×3 rubble tiles (demolition rung)
                        for (int rr = 0; rr < 3; rr++)
                            for (int cc = 0; cc < 3; cc++) o.cell[rr][cc] = OM_BRICK;
                        o.col = roof[hh2 & 3];
                        ob_derive(&o); out[k++] = o;
                    }
            }
    }

    // 3. parked cars (the MOVABLE 2-body class) — line the curbs of CITY/TOWN streets. Unlike a house
    //    (immovable) or a cone (weightless projectile), a car is a real rigid body with mass + inertia:
    //    crash one and the SAME contact impulse SHOVES + SPINS it AND kicks back on your rig (momentum
    //    transfers both ways). It then rolls to a stop on its tyres (anisotropic friction). Emitted after
    //    houses so cone/house idx stay stable (idx = the delta key).
    if (!lot && (zone == Z_CITY || zone == Z_TOWN)) {
        int p = ZONE_PITCH[zone], hwr = ZONE_LANE[zone] / 2;
        float carHH = OB_CELL;                         // half-WIDTH (2 cells / 2 × OB_CELL)
        float lat = hwr - carHH - 1.0f;                // park just inside the curb
        int ncar = 2 + ((h >> 5) % 3);                 // 2..4 per city/town chunk
        for (int i = 0; i < ncar && k < OB_PERCHUNK && lat >= 2.0f; i++) {
            unsigned hc = hash2(cx * 197 + i * 53, cy * 197 + 41);
            int span = (CHUNK / p) + 1;
            int kx = ifloordiv(x0, p) + (int)(hc % (unsigned)(span < 1 ? 1 : span));
            float px_ = kx * p + ((hc & 1) ? lat : -lat);          // sit beside a vertical road's curb
            float py_ = (float)(y0 + (int)((hc >> 9) % CHUNK));
            if (ifloordiv((int)px_, CHUNK) != cx) continue;        // owned by the chunk px_ falls in
            Obstacle o; ob_init(&o, cx, cy, k);
            o.kind = OB_CAR;
            o.bx = o.x = px_; o.by = o.y = py_;
            o.ang = 90.0f;                             // parked along the vertical road (forward = +y)
            o.gw = 2; o.gh = 4;                        // 2 wide × 4 long (top-down car)
            for (int rr = 0; rr < 4; rr++)
                for (int cc = 0; cc < 2; cc++) o.cell[rr][cc] = OM_CAR;
            o.hw = o.gh * OB_CELL * 0.5f;              // half-LENGTH along local +x (forward / long axis)
            o.hh = o.gw * OB_CELL * 0.5f;              // half-WIDTH (lateral)
            int pal[6] = { CLR_RED, CLR_TRUE_BLUE, CLR_DARK_GREEN, CLR_LIGHT_GREY, CLR_YELLOW, CLR_MAUVE };
            o.col = pal[(hc >> 16) % 6];
            ob_derive(&o); out[k++] = o;
        }
    }

    // 4. the SPAWN PARKING LOT — a dense grid of parked cars filling the start-area chunks (no houses
    //    here). Rows laid out in the city block interiors, kept clear of the road bands; all face the same
    //    way (parked). This is the playground for the billiard-knock feel — lots to bump and scatter.
    if (lot) {
        int p = ZONE_PITCH[Z_CITY], hwr = ZONE_LANE[Z_CITY] / 2;   // lot lives on the dense city grid
        int pal[6] = { CLR_RED, CLR_TRUE_BLUE, CLR_DARK_GREEN, CLR_LIGHT_GREY, CLR_YELLOW, CLR_MAUVE };
        for (int gx = x0 + 14; gx < x0 + CHUNK && k < OB_PERCHUNK; gx += 22) {
            int mx = ((gx % p) + p) % p;
            if (mx < hwr + 9 || mx > p - hwr - 9) continue;        // keep the car's 14px width off a road
            for (int gy = y0 + 22; gy < y0 + CHUNK && k < OB_PERCHUNK; gy += 40) {
                int my = ((gy % p) + p) % p;
                if (my < hwr + 16 || my > p - hwr - 16) continue;  // keep its 28px length off a road
                unsigned hc = hash2(gx, gy);
                Obstacle o; ob_init(&o, cx, cy, k);
                o.kind = OB_CAR;
                o.bx = o.x = (float)gx; o.by = o.y = (float)gy; o.ang = 90.0f;
                o.gw = 2; o.gh = 4;
                for (int rr = 0; rr < 4; rr++)
                    for (int cc = 0; cc < 2; cc++) o.cell[rr][cc] = OM_CAR;
                o.hw = o.gh * OB_CELL * 0.5f; o.hh = o.gw * OB_CELL * 0.5f;
                o.col = pal[(hc >> 16) % 6];
                ob_derive(&o); out[k++] = o;
            }
        }
    }
    return k;
}

static Delta *find_delta(int cx, int cy, int idx) {
    for (int i = 0; i < MAXDELTA; i++)
        if (wdelta[i].used && wdelta[i].cx == cx && wdelta[i].cy == cy && wdelta[i].idx == idx)
            return &wdelta[i];
    return NULL;
}

// stash an obstacle's deviation so it survives eviction (bounded — evict the oldest if full).
static void save_delta(Obstacle *o) {
    Delta *d = find_delta(o->cx, o->cy, o->idx);
    if (!d) {
        int slot = -1, oldest = 0x7fffffff;
        for (int i = 0; i < MAXDELTA; i++) {
            if (!wdelta[i].used) { slot = i; break; }
            if (wdelta[i].age < oldest) { oldest = wdelta[i].age; slot = i; }
        }
        d = &wdelta[slot];
        d->used = 1; d->cx = o->cx; d->cy = o->cy; d->idx = o->idx;
    }
    d->dx = o->x - o->bx; d->dy = o->y - o->by; d->dang = o->ang;
    d->destroyed = o->destroyed;
    d->age = ++wclock;
}

static int pool_alloc(void) {
    for (int i = 0; i < MAXOB; i++) if (!pool[i].alive) return i;
    return -1;
}

static void load_chunk(int cx, int cy) {
    Obstacle tmp[OB_PERCHUNK];
    int n = gen_chunk(cx, cy, tmp);
    for (int i = 0; i < n; i++) {
        Delta *d = find_delta(cx, cy, tmp[i].idx);   // replay a remembered deviation on top
        if (d) { tmp[i].x = tmp[i].bx + d->dx; tmp[i].y = tmp[i].by + d->dy; tmp[i].ang = d->dang;
                 tmp[i].destroyed = d->destroyed; tmp[i].dirty = 1;
                 if (d->destroyed && tmp[i].kind == OB_CAR) tmp[i].strength *= WRECK_GIVE; }  // reload a wreck soft
        int s = pool_alloc();
        if (s < 0) return;                            // pool full — drop (bounded)
        pool[s] = tmp[i];
    }
}

static void evict_chunk(int cx, int cy) {
    for (int i = 0; i < MAXOB; i++)
        if (pool[i].alive && pool[i].cx == cx && pool[i].cy == cy) {
            if (pool[i].dirty) save_delta(&pool[i]);  // remember the deviation; else just drop
            pool[i].alive = 0;
        }
}

static int chunk_is_loaded(int cx, int cy) {
    for (int i = 0; i < nLoaded; i++) if (loaded[i].cx == cx && loaded[i].cy == cy) return 1;
    return 0;
}

// keep the live ring in sync with the camera: load chunks that entered, evict those that left.
// Incremental (not a rebuild) so live obstacles keep their un-evicted run-over state across frames.
static void world_sync(void) {
    float hw, hh; view_half_extent(&hw, &hh);   // widen for zoom + heading-up rotation
    int mx = (int)(hw - SCREEN_W * 0.5f) + CHUNK;   // margin ≥ a chunk → no pop
    int my = (int)(hh - SCREEN_H * 0.5f) + CHUNK;
    int x0 = ifloordiv((int)cam_x - mx, CHUNK), x1 = ifloordiv((int)cam_x + SCREEN_W + mx, CHUNK);
    int y0 = ifloordiv((int)cam_y - my, CHUNK), y1 = ifloordiv((int)cam_y + SCREEN_H + my, CHUNK);
    // evict any loaded chunk now outside the ring (compact the list as we go)
    for (int i = 0; i < nLoaded; ) {
        if (loaded[i].cx < x0 || loaded[i].cx > x1 || loaded[i].cy < y0 || loaded[i].cy > y1) {
            evict_chunk(loaded[i].cx, loaded[i].cy);
            loaded[i] = loaded[--nLoaded];
        } else i++;
    }
    // load any chunk in the ring not yet live
    for (int cx = x0; cx <= x1; cx++)
        for (int cy = y0; cy <= y1; cy++)
            if (!chunk_is_loaded(cx, cy) && nLoaded < MAXLOADED) {
                load_chunk(cx, cy);
                loaded[nLoaded].cx = cx; loaded[nLoaded].cy = cy; nLoaded++;
            }
}

// run-over / crash test: the rig (an oriented box about the COM) vs each nearby obstacle.
// run over a cone: knock it along travel + a sideways kick away from the centreline, give it a
// tumble, bleed a sliver of rig speed (∝ its tiny mass) and mark it dirty (so the shove persists).
static void run_over_cone(Obstacle *o, float ltx, float lty, float ly, float spd) {
    float side = (ly >= 0) ? 1.0f : -1.0f;
    float kick = clamp(spd * 1.4f, 60.0f, 300.0f);   // sideways scatter scales with the hit
    o->vx = vx * 1.0f + ltx * side * kick;           // scooped along travel + deflected off the bumper
    o->vy = vy * 1.0f + lty * side * kick;
    o->vr = side * 480.0f;                           // a visible tumble
    o->dirty = 1;
    float loss = clamp(o->mass / M, 0, 1) * 0.30f;
    vx -= vx * loss; vy -= vy * loss;
    last_hit = 1;
    hit(74 + rnd_between(0, 12), INSTR_TRI, 3, 70);   // a little high "ping" (pitch varies per cone)
    shake(1.2f);
    for (int s = 0; s < 5; s++) throw_spark(o->x, o->y, vx, vy);
}

// OBB-vs-OBB minimum-translation vector (separating-axis test). The HONEST contact test for two oriented
// boxes: it finds the axis of least overlap, which IS the true shortest push-out depth + normal — unlike
// the old deepest-corner heuristic, which measured a corner's nearest-face distance and so UNDER-pushed on
// broadside / long-rig contacts (the rig stayed buried and ground through a car; see docs/design/sloop.md §9e).
// Both boxes given as centre + two UNIT axes + half-extents. On overlap returns 1 and sets the unit normal
// (pointing from B toward A — move A by +n·depth to separate), the depth, and a contact point (the deepest
// vertex of the incident box, for the yaw lever). 4 axes only (2 per box); a house passes identity axes → AABB.
static int obb_mtv(float cax, float cay, float aux, float auy, float avx, float avy, float ahx, float ahy,
                   float cbx, float cby, float bux, float buy, float bvx, float bvy, float bhx, float bhy,
                   float *nx, float *ny, float *depth, float *cpx, float *cpy) {
    float dx = cbx - cax, dy = cby - cay;                  // A → B
    float axes[4][2] = { { aux, auy }, { avx, avy }, { bux, buy }, { bvx, bvy } };
    float minov = 1e30f, mnx = 0, mny = 0; int mi = -1;
    for (int i = 0; i < 4; i++) {
        float Lx = axes[i][0], Ly = axes[i][1];
        float dist = af(dx * Lx + dy * Ly);
        float rA = ahx * af(aux * Lx + auy * Ly) + ahy * af(avx * Lx + avy * Ly);
        float rB = bhx * af(bux * Lx + buy * Ly) + bhy * af(bvx * Lx + bvy * Ly);
        float ov = rA + rB - dist;
        if (ov <= 0) return 0;                             // a separating axis → no overlap
        if (ov < minov) { minov = ov; mi = i; mnx = Lx; mny = Ly; }
    }
    if (dx * mnx + dy * mny > 0) { mnx = -mnx; mny = -mny; }   // orient B→A (push A out of B along +n)
    *nx = mnx; *ny = mny; *depth = minov;
    // contact point = support vertex of the INCIDENT box (the one NOT owning the MTV axis), toward the other.
    if (mi < 2) {                                          // MTV on A's axis → incident is B; B's vertex toward A (+n)
        float best = -1e30f;
        for (int sa = -1; sa <= 1; sa += 2) for (int sb = -1; sb <= 1; sb += 2) {
            float vx2 = cbx + sa * bhx * bux + sb * bhy * bvx, vy2 = cby + sa * bhx * buy + sb * bhy * bvy;
            float d = (vx2 - cbx) * mnx + (vy2 - cby) * mny;
            if (d > best) { best = d; *cpx = vx2; *cpy = vy2; }
        }
    } else {                                               // MTV on B's axis → incident is A; A's vertex toward B (-n)
        float best = -1e30f;
        for (int sa = -1; sa <= 1; sa += 2) for (int sb = -1; sb <= 1; sb += 2) {
            float vx2 = cax + sa * ahx * aux + sb * ahy * avx, vy2 = cay + sa * ahx * auy + sb * ahy * avy;
            float d = (vx2 - cax) * (-mnx) + (vy2 - cay) * (-mny);
            if (d > best) { best = d; *cpx = vx2; *cpy = vy2; }
        }
    }
    return 1;
}

// crash into a SOLID box obstacle — ONE resolver for two cases that are the two LIMITS of a rigid-body
// collision: a HOUSE is immovable (infinite mass — only the rig responds) and a CAR is MOVABLE (finite
// mass + inertia — the SAME impulse is applied equal-and-opposite to BOTH bodies, so momentum transfers).
// Finds the contact via the OBB SAT minimum-translation (true normal + depth, even on a broadside/long-rig
// contact — obb_mtv), depenetrates fully, then a contact impulse: an off-centre clip yaws the rig (J×r/I, the same
// form steering uses), and for a car it spins the car too. J vs strength still decides give-vs-hold — for
// a house "give" = SMASH (stays gone); for a car "give" = WRECK (crumples, but stays a shoveable wreck).
// The cone (weightless projectile, run_over_cone) is the third limit: M_obstacle → 0. One core, three uses.
#define CRASH_E   0.25f                              // restitution into a house wall (a little bounce)
#define CAR_E     0.6f                               // restitution car↔rig: steel-ball bounce — they scatter,
                                                     // not thud-and-stick (the billiard feel)
#define CRASH_FX_MIN 28.0f                           // min approach speed (px/s) for crash FX (thud +
                                                     // sparks + shake) — below this it's a resting push,
                                                     // not an impact, so no continuous crashing at a wall
static void crash_body(Obstacle *o, float fwx, float fwy, float ltx, float lty) {
    int movable = (o->kind == OB_CAR);
    float oc = movable ? cos_deg(o->ang) : 1.0f, os = movable ? sin_deg(o->ang) : 0.0f;
    // rig OBB: its COM-local box (rigL0..rigW1) → world centre + unit axes + half-extents.
    float rcx, rcy; rot((rigL0 + rigL1) * 0.5f, (rigW0 + rigW1) * 0.5f, &rcx, &rcy);   // rig box centre (rot adds sx,sy)
    float rhx = (rigL1 - rigL0) * 0.5f, rhy = (rigW1 - rigW0) * 0.5f;
    // SAT minimum-translation: the TRUE push-out depth + normal (out of the obstacle) + contact point. The
    // old deepest-corner test UNDER-pushed on broadside / long-rig contacts → the rig stayed buried and ground
    // through the car (the bug). SAT clears it fully every frame, and the corner / engulf special-cases collapse
    // into it. Obstacle axes: a car is oriented, a house passes identity (oc=1,os=0) → an AABB test.
    float bnx, bny, bestPen, bcx, bcy;
    if (!obb_mtv(rcx, rcy, fwx, fwy, ltx, lty, rhx, rhy,
                 o->x, o->y, oc, os, -os, oc, o->hw, o->hh,
                 &bnx, &bny, &bestPen, &bcx, &bcy)) return;   // no overlap
    float rx = bcx - sx, ry = bcy - sy;             // contact lever from the rig COM
    float wrad = angVel * DEG2RAD;
    float vcx = vx - wrad * ry, vcy = vy + wrad * rx;   // rig velocity at the contact point
    // movable obstacle: its mass + box inertia + contact velocity (a house: all → 0/infinite, terms vanish)
    float Mo = movable ? o->mass : 0.0f;
    float Io = movable ? (1.0f / 3.0f) * Mo * (o->hw * o->hw + o->hh * o->hh) : 1.0f;
    if (movable && Io < 1.0f) Io = 1.0f;
    float rox = bcx - o->x, roy = bcy - o->y;       // contact lever from the car COM
    float worad = movable ? o->vr * DEG2RAD : 0.0f;
    float ovx = movable ? o->vx - worad * roy : 0.0f;
    float ovy = movable ? o->vy + worad * rox : 0.0f;
    float vn = (vcx - ovx) * bnx + (vcy - ovy) * bny;   // RELATIVE normal velocity (< 0 = closing)
    // Depenetration: push the RIG fully clear every frame (never let it sink into / grind through a car,
    // even when the impulse early-outs below because the rig is just catching a gliding car at ~0 closing
    // speed), and ALSO shove the car out by its mass share so a heavy hit still visibly displaces it.
    sx += bnx * bestPen; sy += bny * bestPen;
    if (movable) {
        float carShare = M / (M + Mo);                  // light car yields more; a semi barely budges it
        o->x -= bnx * bestPen * carShare; o->y -= bny * bestPen * carShare; o->dirty = 1;
    }
    if (vn >= 0) return;                            // already separating — just the push-out
    last_hit = 1;
    float rxn  = rx  * bny - ry  * bnx;             // rig lever ⟂ normal
    float rxno = rox * bny - roy * bnx;             // car lever ⟂ normal
    float invMo = movable ? 1.0f / Mo : 0.0f, invIo = movable ? 1.0f / Io : 0.0f;
    float denom = 1.0f / M + invMo + rxn * rxn / I + rxno * rxno * invIo;   // house: invMo=invIo=0 → 1/M+rxn²/I
    float jimp = -(1.0f + (movable ? CAR_E : CRASH_E)) * vn / denom;   // contact impulse (>0); cars bounce more
    // apply to the rig (and, for a car, equal-and-opposite to the car — Newton's third law)
    vx += jimp * bnx / M; vy += jimp * bny / M;
    angVel += (rxn * jimp / I) / DEG2RAD;
    if (movable) {
        o->vx -= jimp * bnx * invMo; o->vy -= jimp * bny * invMo;
        o->vr -= (rxno * jimp * invIo) / DEG2RAD;
    }
    if (!movable) {
        // HOUSE: J vs strength on the infinite-mass impulse the rig delivers (M·|vn|, the original measure).
        if (M * (-vn) >= o->strength) {             // SMASH — whole-obstacle destroy (tile shatter = demo rung)
            o->destroyed = 1; o->dirty = 1; last_hit = 2;
            // undo the bounce we just applied: a rig heavy enough to break through barely slows, doesn't rebound
            vx -= jimp * bnx / M; vy -= jimp * bny / M; angVel -= (rxn * jimp / I) / DEG2RAD;
            hit(24, INSTR_NOISE, 4, 320);           // a heavy crunch
            shake(clamp(M * (-vn) / 120.0f, 3.0f, 9.0f));
            for (int s = 0; s < 18; s++)
                throw_spark(o->x + rnd_float_between(-o->hw, o->hw),
                            o->y + rnd_float_between(-o->hh, o->hh), vx, vy);
            vx *= 0.9f; vy *= 0.9f;                  // barely slows a rig heavy enough to break through
            return;
        }
        if (-vn > CRASH_FX_MIN) {                    // a genuine hit, not a resting nudge against the wall
            hit(30, INSTR_NOISE, 3, 120);            // a solid thud
            shake(clamp(-vn / 18.0f, 1.5f, 6.0f));
            for (int s = 0; s < 6; s++) throw_spark(bcx, bcy, -vx, -vy);
        } else last_hit = 0;                        // contact, but no impact this frame
        return;
    }
    // CAR: the impulse already shoved + spun it. J vs strength now decides WRECK vs a clean shunt.
    if (jimp >= o->strength && !o->destroyed) {     // WRECK — crumples, but STAYS a (softer) shoveable wreck
        o->destroyed = 1; last_hit = 2;
        o->strength *= WRECK_GIVE;                   // gives more easily on the next hit
        hit(26, INSTR_NOISE, 4, 260);               // a metal crunch
        shake(clamp(jimp / 100.0f, 3.0f, 8.0f));
        for (int s = 0; s < 14; s++)
            throw_spark(bcx + rnd_float_between(-o->hh, o->hh),
                        bcy + rnd_float_between(-o->hh, o->hh), vx, vy);
    } else if (-vn > CRASH_FX_MIN) {                // a clean KNOCK (no wreck) — a bright steel-and-glass clack
        hit(66 + rnd_between(0, 8), INSTR_TRI, 2, 90);   // high ping (the "glass") — pitch varies per hit
        hit(34, INSTR_NOISE, 2, 70);                     // a short metal tick under it (the "steel")
        shake(clamp(-vn / 22.0f, 1.0f, 5.0f));
        for (int s = 0; s < 5; s++) throw_spark(bcx, bcy, -vx, -vy);
    } else last_hit = 0;
}

// the world collides against the rig each frame: cones are run over (give), houses crash (hold,
// or smash if hit hard enough). The decision is one number — the contact impulse vs strength.
static void collide_world(void) {
    float fwx = cos_deg(ang), fwy = sin_deg(ang);
    float ltx = -fwy, lty = fwx;
    float spd = fsqrt(vx * vx + vy * vy);
    last_hit = 0;
    if (spd < 1.0f) return;                          // parked — nothing to run into
    for (int i = 0; i < MAXOB; i++) {
        Obstacle *o = &pool[i];
        if (!o->alive || o->destroyed) continue;
        float dx = o->x - sx, dy = o->y - sy;
        float gross = rigReach + o->rad;
        if (dx * dx + dy * dy > gross * gross) continue;        // broad phase
        if (o->kind == OB_HOUSE || o->kind == OB_CAR) { crash_body(o, fwx, fwy, ltx, lty); continue; }
        // cone (run-over class): a knocked one is already leaving — don't re-trigger
        if (o->vx * o->vx + o->vy * o->vy > 100.0f) continue;
        float lx = dx * fwx + dy * fwy, ly = dx * ltx + dy * lty;   // into the rig's oriented box
        float r = o->rad;
        if (lx < rigL0 - r || lx > rigL1 + r || ly < rigW0 - r || ly > rigW1 + r) continue;  // miss
        if (M * spd >= o->strength) run_over_cone(o, ltx, lty, ly, spd);  // J ≥ strength → it gives
    }
}

// knocked obstacles tumble away under friction, then settle (they STAY where they land —
// dirty, so the delta persists). Also sets up loose debris for the demolition rung.
static void obstacles_integrate(float dt_) {
    for (int i = 0; i < MAXOB; i++) {
        Obstacle *o = &pool[i];
        if (!o->alive) continue;
        if (o->vx == 0 && o->vy == 0 && o->vr == 0) continue;
        o->x += o->vx * dt_; o->y += o->vy * dt_; o->ang += o->vr * dt_;
        if (o->kind == OB_CAR) {
            // a knocked car GLIDES like a struck ball: low friction so it carries the hit a long way and
            // keeps spinning, only gently favouring its rolling direction (a hint of tyre grip, not a brake).
            float c = cos_deg(o->ang), s = sin_deg(o->ang);
            float vf = o->vx * c + o->vy * s, vl = -o->vx * s + o->vy * c;   // car-frame fwd / lateral
            vf *= 0.992f; vl *= 0.96f;                     // mostly free glide, a touch of nose-tracking
            o->vx = vf * c - vl * s; o->vy = vf * s + vl * c;
            o->vr *= 0.97f;                                // it keeps spinning a while
            if (o->vx * o->vx + o->vy * o->vy < 4.0f && af(o->vr) < 4.0f) { o->vx = 0; o->vy = 0; o->vr = 0; }
        } else {
            o->vx *= 0.96f; o->vy *= 0.96f; o->vr *= 0.94f;   // cone: light friction → it skitters a good way
            if (o->vx * o->vx + o->vy * o->vy < 4.0f) { o->vx = 0; o->vy = 0; o->vr = 0; }  // settled
        }
    }
}

// ── §9f: obstacle-vs-obstacle — the CHAIN REACTION ───────────────────────────
// The same two-body impulse as crash_body, but the ACTIVE body is a moving obstacle (a knocked car)
// instead of the rig. So a hit car shoves the next car (which wakes and shoves the next…), piles into a
// house, or scatters a cone — all from the one resolver. Only MOVING obstacles initiate (a resting car is
// just a target), which is what both makes the chain ripple AND keeps the packed parking lot stable at rest.
#define CONE_E        0.10f      // a knocked cone barely bounces (vs CAR_E for car↔car)

static int chain_sounds;          // per-frame budget so a big pile-up doesn't machine-gun the mixer

// resolve a moving obstacle `a` against a neighbour `b` (a car, a house, or a cone). Bidirectional contact,
// mass-split depenetration, the 2-body impulse, then the same outcomes as crash_body: wreck a struck car,
// (rarely) smash a house, scatter a cone — all decided by J vs strength.
static void collide_obstacle_pair(Obstacle *a, Obstacle *b) {
    float ac = cos_deg(a->ang), as = sin_deg(a->ang);
    int bMov = (b->kind != OB_HOUSE);
    float bc = bMov ? cos_deg(b->ang) : 1.0f, bs = bMov ? sin_deg(b->ang) : 0.0f;
    // SAT minimum-translation (obb_mtv): true normal + depth, even on a broadside contact — same fix as the
    // rig resolver. n points from b toward a (push a out of b). Replaces the deepest-corner test that under-pushed.
    float pen, nx, ny, ccx, ccy;
    if (!obb_mtv(a->x, a->y, ac, as, -as, ac, a->hw, a->hh,
                 b->x, b->y, bc, bs, -bs, bc, b->hw, b->hh,
                 &nx, &ny, &pen, &ccx, &ccy)) return;
    float Ma = a->mass, Ia = (1.0f / 3.0f) * Ma * (a->hw * a->hw + a->hh * a->hh); if (Ia < 1.0f) Ia = 1.0f;
    float Mb = bMov ? b->mass : 0.0f;
    float Ib = bMov ? (1.0f / 3.0f) * Mb * (b->hw * b->hw + b->hh * b->hh) : 1.0f; if (bMov && Ib < 1.0f) Ib = 1.0f;
    float rax = ccx - a->x, ray = ccy - a->y, rbx = ccx - b->x, rby = ccy - b->y;
    float wa = a->vr * DEG2RAD, wb = bMov ? b->vr * DEG2RAD : 0.0f;
    float vax = a->vx - wa * ray, vay = a->vy + wa * rax;
    float vbx = bMov ? b->vx - wb * rby : 0.0f, vby = bMov ? b->vy + wb * rbx : 0.0f;
    float vn = (vax - vbx) * nx + (vay - vby) * ny;            // relative normal velocity (<0 = closing)
    float aShare = bMov ? Mb / (Ma + Mb) : 1.0f;               // a yields more vs a heavy/immovable b
    a->x += nx * pen * aShare; a->y += ny * pen * aShare; a->dirty = 1;
    if (bMov) { b->x -= nx * pen * (1.0f - aShare); b->y -= ny * pen * (1.0f - aShare); b->dirty = 1; }
    if (vn >= 0) return;                                       // separating — just the push-out
    float ran = rax * ny - ray * nx, rbn = rbx * ny - rby * nx;
    float invMa = 1.0f / Ma, invIa = 1.0f / Ia, invMb = bMov ? 1.0f / Mb : 0.0f, invIb = bMov ? 1.0f / Ib : 0.0f;
    float denom = invMa + invMb + ran * ran * invIa + rbn * rbn * invIb;
    float e = (b->kind == OB_CONE) ? CONE_E : CAR_E;
    float jimp = -(1.0f + e) * vn / denom;
    a->vx += jimp * nx * invMa; a->vy += jimp * ny * invMa; a->vr += (ran * jimp * invIa) / DEG2RAD;
    if (bMov) { b->vx -= jimp * nx * invMb; b->vy -= jimp * ny * invMb; b->vr -= (rbn * jimp * invIb) / DEG2RAD; }
    last_hit = 1;
    // outcomes: J vs strength (same currency as the rig). A heavy enough relative hit wrecks a car or
    // (rarely) smashes a house; a cone just scatters (the impulse already flung it).
    if (b->kind == OB_HOUSE) {
        if (Ma * (-vn) >= b->strength) { b->destroyed = 1; b->dirty = 1; }
    } else if (b->kind == OB_CAR && jimp >= b->strength && !b->destroyed) {
        b->destroyed = 1; b->strength *= WRECK_GIVE;
    }
    if (a->kind == OB_CAR && jimp >= a->strength && !a->destroyed) {   // the hitter can crumple too
        a->destroyed = 1; a->strength *= WRECK_GIVE;
    }
    if (-vn > CRASH_FX_MIN) {                                  // a real impact (not a resting settle) → FX
        for (int s = 0; s < 4; s++) throw_spark(ccx, ccy, a->vx, a->vy);
        if (chain_sounds < 3) { hit(60 + rnd_between(0, 10), INSTR_TRI, 2, 70); chain_sounds++; }  // a glassy tick, budgeted
    }
}

// the chain pass: resolve EVERY overlapping obstacle pair once (i<j). The pair resolver depenetrates
// always but only applies the bouncy impulse on a CLOSING contact — so two resting, non-overlapping cars
// cost only a broad-phase reject (no jitter), an overlapping pair is always pushed apart (no more sliding
// through / settling interpenetrated), and a closing hit transfers momentum → the wake ripples on.
static void collide_obstacles_chain(void) {
    chain_sounds = 0;
    for (int i = 0; i < MAXOB; i++) {
        if (!pool[i].alive) continue;
        for (int j = i + 1; j < MAXOB; j++) {
            if (!pool[j].alive) continue;
            Obstacle *a = &pool[i], *b = &pool[j];
            if (a->kind == OB_HOUSE && b->kind == OB_HOUSE) continue;     // two immovables never interact
            if (a->kind == OB_HOUSE) { Obstacle *t = a; a = b; b = t; }   // make `a` the movable body
            float dx = b->x - a->x, dy = b->y - a->y, reach = a->rad + b->rad;
            if (dx * dx + dy * dy > reach * reach) continue;             // broad phase
            collide_obstacle_pair(a, b);
        }
    }
}

// reset the whole world layer (R / fresh rig) — drop the pool, deltas and loaded set so the
// origin chunks regenerate pristine; world_sync repopulates next DRIVE frame.
static void world_reset(void) {
    for (int i = 0; i < MAXOB; i++) pool[i].alive = 0;
    for (int i = 0; i < MAXDELTA; i++) wdelta[i].used = 0;
    nLoaded = 0; wclock = 0; last_hit = 0;
}

// fill an ORIENTED rectangle (centre, forward angle, half-length along forward, half-width).
// Uses the engine's GPU rectfill_rot so it stays HOLE-FREE under the heading-up camera: a
// software scanline/coverage fill rasterises in world space and the camera rotation staircases
// it into a dotted lattice (the artifact that showed on rotated/collided cars) — a GPU quad is
// filled in screen space after the transform, solid at any angle. w = the length (along forward).
static void fill_orect(float cx_, float cy_, float ang_, float hl, float hwid, int col) {
    rectfill_rot((int)cx_, (int)cy_, (int)(hl * 2), (int)(hwid * 2), ang_, col);
}

// draw the live obstacles in world space (called inside the camera transform, under the rig).
static void draw_obstacles(void) {
    for (int i = 0; i < MAXOB; i++) {
        Obstacle *o = &pool[i];
        if (!o->alive) continue;
        if (o->kind == OB_HOUSE) {
            int x = (int)(o->x - o->hw), y = (int)(o->y - o->hh);
            int w = (int)(o->hw * 2), h = (int)(o->hh * 2);
            if (o->destroyed) {                      // smashed → a low rubble pile (it stays demolished)
                rectfill(x + 2, y + h / 2, w - 4, h / 2 - 1, CLR_DARKER_GREY);
                rectfill(x + 4, y + h / 2 + 2, 3, 2, CLR_MEDIUM_GREY);
                rectfill(x + w - 8, y + h - 4, 3, 2, CLR_MEDIUM_GREY);
            } else if (i == foot_house) {            // F2: the roof is OFF — draw the plan, not the box
                Interior *in = interior_get(o);
                fill_orect(o->x, o->y, o->ang, o->hw + 1, o->hh + 1, CLR_BROWNISH_BLACK);   // plot edge
                fill_orect(o->x, o->y, o->ang, o->hw, o->hh, CLR_MEDIUM_GREY);              // the floor
                for (int k = 0; k < in->nf; k++) {   // furniture first (walls draw over their ends)
                    float wx, wy;
                    ob_l2w(o, (in->f[k][0] + in->f[k][2]) * 0.5f, (in->f[k][1] + in->f[k][3]) * 0.5f, &wx, &wy);
                    fill_orect(wx, wy, o->ang, (in->f[k][2] - in->f[k][0]) * 0.5f,
                                               (in->f[k][3] - in->f[k][1]) * 0.5f, CLR_BROWN);
                }
                for (int k = 0; k < in->nw; k++) {
                    float wx, wy;
                    ob_l2w(o, (in->w[k][0] + in->w[k][2]) * 0.5f, (in->w[k][1] + in->w[k][3]) * 0.5f, &wx, &wy);
                    fill_orect(wx, wy, o->ang, (in->w[k][2] - in->w[k][0]) * 0.5f,
                                               (in->w[k][3] - in->w[k][1]) * 0.5f, CLR_BROWNISH_BLACK);
                }
            } else {
                if (o->ang != 0.0f) {                // Rung C: OSM footprint — ORIENTED box (matches its OBB)
                    fill_orect(o->x, o->y, o->ang, o->hw + 1, o->hh + 1, CLR_BROWNISH_BLACK);   // 1px outline
                    fill_orect(o->x, o->y, o->ang, o->hw, o->hh, o->col);
                } else {                             // default sloop houses (axis-aligned) — unchanged
                    rectfill(x, y, w, h, o->col);
                    rect(x, y, w, h, CLR_BROWNISH_BLACK);
                }
                if (mode == MODE_FOOT && house_enterable(o)) {   // F2: advertise the door while on foot
                    int d = house_door_side(o);
                    float lx = (d == 0) ? o->hw - 1 : (d == 1) ? -o->hw + 1 : 0;
                    float ly = (d == 2) ? o->hh - 1 : (d == 3) ? -o->hh + 1 : 0;
                    float wx, wy; ob_l2w(o, lx, ly, &wx, &wy);
                    fill_orect(wx, wy, o->ang, (d < 2) ? 1.2f : 3.0f, (d < 2) ? 3.0f : 1.2f, CLR_BROWNISH_BLACK);
                }
            }
        } else if (o->kind == OB_CAR) {
            // top-down car: body (oriented), a darker cabin strip toward the rear, light windshield, outline.
            int body = o->destroyed ? CLR_DARK_GREY : o->col;
            fill_orect(o->x, o->y, o->ang, o->hw, o->hh, body);
            if (o->destroyed) {                          // wreck — crumple: a dent + scattered debris dots
                float c = cos_deg(o->ang), s = sin_deg(o->ang);
                fill_orect(o->x + c * o->hw * 0.4f, o->y + s * o->hw * 0.4f, o->ang, o->hw * 0.35f, o->hh * 0.7f, CLR_DARKER_GREY);
                pset((int)(o->x - s * o->hh), (int)(o->y + c * o->hh), CLR_MEDIUM_GREY);
            } else {
                float c = cos_deg(o->ang), s = sin_deg(o->ang);
                // cabin (rear half, darker) + windshield band (front, light) read the car's facing
                fill_orect(o->x - c * o->hw * 0.25f, o->y - s * o->hw * 0.25f, o->ang, o->hw * 0.4f, o->hh * 0.78f, CLR_DARKER_BLUE);
                line((int)(o->x + c * o->hw * 0.45f + s * o->hh * 0.7f), (int)(o->y + s * o->hw * 0.45f - c * o->hh * 0.7f),
                     (int)(o->x + c * o->hw * 0.45f - s * o->hh * 0.7f), (int)(o->y + s * o->hw * 0.45f + c * o->hh * 0.7f), CLR_LIGHT_GREY);
            }
        } else if (o->kind == OB_CONE) {
            int moving = (o->vx != 0 || o->vy != 0);
            if (moving) {                            // knocked over — a toppled streak
                float c = cos_deg(o->ang), s = sin_deg(o->ang);
                line((int)(o->x - c * 4), (int)(o->y - s * 4),
                     (int)(o->x + c * 4), (int)(o->y + s * 4), CLR_ORANGE);
                pset((int)o->x, (int)o->y, CLR_WHITE);
            } else {                                 // standing — concentric rings read as a cone from above
                circfill((int)o->x, (int)o->y, 3, CLR_ORANGE);
                circfill((int)o->x, (int)o->y, 2, CLR_WHITE);
                pset((int)o->x, (int)o->y, CLR_ORANGE);
            }
        }
    }
}

static int zone_at(float x, float y) {
    float d = fsqrt(x * x + y * y);
    for (int z = 0; z < Z_N; z++) if (d < ZONE_R[z]) return z;
    return Z_SUPER;
}

// ══ Rung B (OSM) — drive the REAL world (docs/design/driving-world-program.md, P1 Rung B-OSM) ══
// OPT-IN: run with `--data data/delft-centre.rvb` (or $DE_DATA). With no data the cart is UNCHANGED —
// road_at() falls back to the stub grid world below. When a .rvb IS loaded, road_at() answers from a
// nearest-edge query over real OSM road polylines, indexed by a uniform grid so each lookup touches
// O(few) segments — the "deterministic, per-frame-fast spatial index" P1 calls the crux. STEP 1–3:
// only the HUD/watch read the new answer; the screen still paints the stub grid (draw_course routes
// through road_at in a later step). Points in a .rvb are LOCAL METRES, Y NORTH-UP (roadview.c); sloop's
// world is PIXELS, Y DOWN — bridged by OSM_PPM + a Y flip, rig origin (0,0) = the data's bbox centre.
#define OSM_PPM    4.0f     // pixels per real metre (a 6 m street ≈ 24 px ≈ sloop's city lane width)
#define OSM_CELL   32.0f    // index cell size, metres (> max road half-width ⇒ a 3×3 cell scan suffices)
#define OSM_MAXSEG 80000    // delft-centre ≈ 2.8k · delft-netherlands ≈ 57k road segments

static struct { float ax, ay, bx, by, hw; int cls; } oseg[OSM_MAXSEG];  // metres, y north-up
static int    n_oseg = 0;                  // osm_loaded is forward-declared up by the road_at() seam
static float  osm_cx, osm_cy;              // data bbox centre (metres) — maps to sloop world (0,0)
static float  gminx, gminy;                // index origin (metres)
static int    gcol = 1, grow = 1;          // index dims (cells)
static int   *ocell_start = NULL, *oidx = NULL;   // CSR: per-cell [start..start+1) into oidx (seg ids)

// real-world road half-width per OSM kind (K_* from roadview.c; matches ST[].hw_m). <0 ⇒ not a road.
static float osm_road_hw(int kind) {
    switch (kind) {
        case 0:  return 7.0f;   // highway      case 1: arterial 5, etc.
        case 1:  return 5.0f;
        case 2:  return 3.0f;   // road / residential
        case 3:  return 1.5f;   // track
        case 17: return 4.0f;   // secondary
        case 18: return 3.0f;   // tertiary
        case 19: return 1.5f;   // service
        case 20: return 1.2f;   // cycleway
        case 21: return 0.9f;   // footway
        default: return -1.0f;  // not a drivable road way → skip
    }
}

static int osm_cellcol(float mx) { int c = (int)((mx - gminx) / OSM_CELL); return c < 0 ? 0 : (c >= gcol ? gcol - 1 : c); }
static int osm_cellrow(float my) { int r = (int)((my - gminy) / OSM_CELL); return r < 0 ? 0 : (r >= grow ? grow - 1 : r); }

static char *osm_slurp(const char *path, long *outlen) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *b = (char *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    long got = (long)fread(b, 1, (size_t)n, f); fclose(f);
    if (got != n) { free(b); return NULL; }
    if (outlen) *outlen = n;
    return b;
}

// load a .rvb: flatten road polylines → segments, then build the uniform-grid index. Sets osm_loaded.
static void osm_load(const char *path) {
    long len; char *buf = osm_slurp(path, &len);
    if (!buf) return;
    if (len < 24 || memcmp(buf, "RVB", 3) != 0 || buf[3] < '1' || buf[3] > '3') { free(buf); return; }
    // valid RVB → committing to a (re)load: free any previous index so a re-drop doesn't leak.
    if (ocell_start) { free(ocell_start); ocell_start = NULL; }
    if (oidx) { free(oidx); oidx = NULL; }
    osm_loaded = 0;
    int ver = buf[3];
    const char *p = buf + 4, *end = buf + len;
    int nfeat, namelen;
    memcpy(&nfeat, p, 4);   p += 4;
    memcpy(&namelen, p, 4); p += 4;
    if (namelen < 0) namelen = 0;
    { int nl = namelen < 63 ? namelen : 63; memcpy(osm_name, p, (size_t)nl); osm_name[nl] = 0; }
    p += namelen;
    float bb[4]; memcpy(bb, p, 16); p += 16;
    gminx = bb[0]; gminy = bb[1];
    float gmaxx = bb[2], gmaxy = bb[3];
    osm_cx = (gminx + gmaxx) * 0.5f; osm_cy = (gminy + gmaxy) * 0.5f;

    n_oseg = 0; n_obld = 0;
    for (int f = 0; f < nfeat && p + 12 <= end; f++) {
        int kind, sublen, npts;
        memcpy(&kind, p, 4);   p += 4;
        if (ver == '2' || ver == '3') p += 4;   // skip the per-feature height float (RVB2+)
        memcpy(&sublen, p, 4); p += 4; p += sublen;
        memcpy(&npts, p, 4);   p += 4;

        if (kind == 6) {   // K_BUILDING → an ORIENTED-box obstacle (Rung C). Fit a box to the footprint:
            float bp[OSM_BLDCAP][2]; int nb = 0;                            // read verts (metres) up to the cap
            for (int j = 0; j < npts && p + 8 <= end; j++) {
                float xy[2]; memcpy(xy, p, 8); p += 8;
                if (nb < OSM_BLDCAP) { bp[nb][0] = xy[0]; bp[nb][1] = xy[1]; nb++; }
            }
            if (nb >= 3 && n_obld < OSM_MAXBLD) {
                float bestL = -1, ux = 1, uy = 0;                          // box axis = the LONGEST edge dir
                for (int j = 0; j < nb; j++) {
                    int j2 = (j + 1) % nb; float ex = bp[j2][0] - bp[j][0], ey = bp[j2][1] - bp[j][1];
                    float L = ex * ex + ey * ey;
                    if (L > bestL) { bestL = L; float l = fsqrt(L); if (l > 1e-4f) { ux = ex / l; uy = ey / l; } }
                }
                float vx2 = -uy, vy2 = ux;                                 // perpendicular axis
                float umin = 1e30f, umax = -1e30f, vmin = 1e30f, vmax = -1e30f;
                for (int j = 0; j < nb; j++) {                             // project all verts onto (u,v)
                    float pu = bp[j][0] * ux + bp[j][1] * uy, pv = bp[j][0] * vx2 + bp[j][1] * vy2;
                    if (pu < umin) umin = pu; if (pu > umax) umax = pu;
                    if (pv < vmin) vmin = pv; if (pv > vmax) vmax = pv;
                }
                float uc = (umin + umax) * 0.5f, vc = (vmin + vmax) * 0.5f;
                obld[n_obld].wx = ux * uc + vx2 * vc;                      // centre back in world metres (converted below)
                obld[n_obld].wy = uy * uc + vy2 * vc;
                obld[n_obld].hw = (umax - umin) * 0.5f;                    // metres half-extents (converted below)
                obld[n_obld].hh = (vmax - vmin) * 0.5f;
                obld[n_obld].ang = atan2f(uy, ux) * 57.29578f;            // metres-frame heading (converted below)
                n_obld++;
            }
            continue;
        }

        float hw = osm_road_hw(kind);
        if (hw < 0) { p += (long)npts * 8; continue; }   // not a road → skip its points wholesale
        float px = 0, py = 0; int have = 0;
        for (int j = 0; j < npts && p + 8 <= end; j++) {
            float xy[2]; memcpy(xy, p, 8); p += 8;
            if (have && n_oseg < OSM_MAXSEG) {
                oseg[n_oseg].ax = px; oseg[n_oseg].ay = py;
                oseg[n_oseg].bx = xy[0]; oseg[n_oseg].by = xy[1];
                oseg[n_oseg].hw = hw; oseg[n_oseg].cls = kind; n_oseg++;
            }
            px = xy[0]; py = xy[1]; have = 1;
        }
    }
    free(buf);
    if (n_oseg == 0) return;

    // spawn ON a road: map sloop world (0,0) to the road segment midpoint nearest the bbox centre,
    // so the rig starts on a street (not the 35 m-off-road bbox centre) and roads frame it immediately.
    { float bc_x = osm_cx, bc_y = osm_cy, best = 1e30f;
      for (int s = 0; s < n_oseg; s++) {
          float mmx = (oseg[s].ax + oseg[s].bx) * 0.5f, mmy = (oseg[s].ay + oseg[s].by) * 0.5f;
          float d = (mmx - bc_x) * (mmx - bc_x) + (mmy - bc_y) * (mmy - bc_y);
          if (d < best) { best = d; osm_cx = mmx; osm_cy = mmy; }
      } }

    // buildings: convert metres box → world px (same map + Y-flip as road_at) + bucket by home chunk,
    // so gen_chunk can emit the ones in a live chunk as OB_HOUSE obstacles you crash into.
    for (int b = 0; b < n_obld; b++) {
        float wx = (obld[b].wx - osm_cx) * OSM_PPM, wy = -(obld[b].wy - osm_cy) * OSM_PPM;
        obld[b].wx = wx; obld[b].wy = wy;
        obld[b].hw *= OSM_PPM; if (obld[b].hw < 2.0f) obld[b].hw = 2.0f;
        obld[b].hh *= OSM_PPM; if (obld[b].hh < 2.0f) obld[b].hh = 2.0f;
        obld[b].ang = -obld[b].ang;                        // Y flip negates the heading
        obld[b].cx = ifloordiv((int)wx, CHUNK); obld[b].cy = ifloordiv((int)wy, CHUNK);
    }

    // uniform-grid index (CSR). Each segment is bucketed into every cell its bounding box covers;
    // with hw < OSM_CELL, a 3×3 scan around the query point then catches every nearby segment.
    gcol = (int)((gmaxx - gminx) / OSM_CELL) + 1; if (gcol < 1) gcol = 1;
    grow = (int)((gmaxy - gminy) / OSM_CELL) + 1; if (grow < 1) grow = 1;
    int ncell = gcol * grow;
    ocell_start = (int *)calloc((size_t)ncell + 1, sizeof(int));
    if (!ocell_start) return;
    for (int s = 0; s < n_oseg; s++) {                                   // pass 1: count per cell
        int c0 = osm_cellcol(oseg[s].ax < oseg[s].bx ? oseg[s].ax : oseg[s].bx);
        int c1 = osm_cellcol(oseg[s].ax > oseg[s].bx ? oseg[s].ax : oseg[s].bx);
        int r0 = osm_cellrow(oseg[s].ay < oseg[s].by ? oseg[s].ay : oseg[s].by);
        int r1 = osm_cellrow(oseg[s].ay > oseg[s].by ? oseg[s].ay : oseg[s].by);
        for (int cc = c0; cc <= c1; cc++) for (int rr = r0; rr <= r1; rr++) ocell_start[rr * gcol + cc + 1]++;
    }
    for (int i = 0; i < ncell; i++) ocell_start[i + 1] += ocell_start[i]; // prefix sum → cell starts
    int total = ocell_start[ncell];
    oidx = (int *)malloc((size_t)(total > 0 ? total : 1) * sizeof(int));
    int *cur = (int *)malloc((size_t)ncell * sizeof(int));
    if (!oidx || !cur) { free(oidx); free(cur); free(ocell_start); oidx = ocell_start = NULL; return; }
    for (int i = 0; i < ncell; i++) cur[i] = ocell_start[i];
    for (int s = 0; s < n_oseg; s++) {                                   // pass 2: fill
        int c0 = osm_cellcol(oseg[s].ax < oseg[s].bx ? oseg[s].ax : oseg[s].bx);
        int c1 = osm_cellcol(oseg[s].ax > oseg[s].bx ? oseg[s].ax : oseg[s].bx);
        int r0 = osm_cellrow(oseg[s].ay < oseg[s].by ? oseg[s].ay : oseg[s].by);
        int r1 = osm_cellrow(oseg[s].ay > oseg[s].by ? oseg[s].ay : oseg[s].by);
        for (int cc = c0; cc <= c1; cc++) for (int rr = r0; rr <= r1; rr++) oidx[cur[rr * gcol + cc]++] = s;
    }
    free(cur);
    osm_loaded = 1;
}

// squared distance from point to segment (classic clamped projection). Metres².
static float osm_pt_seg_d2(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float c1 = vx * wx + vy * wy;
    if (c1 <= 0) return wx * wx + wy * wy;
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) { float dx = px - bx, dy = py - by; return dx * dx + dy * dy; }
    float t = c1 / c2, dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return dx * dx + dy * dy;
}

// road_at() — the P1 seam (declared up top). With an OSM .rvb loaded, a nearest-edge query over real
// roads; otherwise the STUB grid world (a point is ON a road within ZONE_LANE/2 of a grid line at the
// zone pitch — AGREES with the bands draw_course paints by construction). Rung B swapped in the OSM body.
// ── RUNG A1: the ROADNET2 spine (worldnet.h) behind the same seam. World px ↔ metres is
// the OSM mapping (RN2_PPM, no Y flip — the spine is y-down like sloop); rn2_ox/oy = the
// metres at world-px (0,0), snapped on toggle so the rig starts ON the network facing
// along it. road_at() answers from wn_road_at() — the exact edge graph roadnet2 queries.
#define RN2_PPM     OSM_PPM       // world px per real metre (4 — a 6 m street ≈ 24 px)
#define RN2_SPAWN_X 40000.0f      // roadnet2's land spawn (metres)
#define RN2_SPAWN_Y 30000.0f
static float rn2_ox = RN2_SPAWN_X, rn2_oy = RN2_SPAWN_Y;

// ── RUNG 5.5c: a generated CITYGEN city behind the same seam. Same px↔metre map
// as the spine (CG_PPM, y-down); cg_ox/cg_oy = the metres at world-px (0,0),
// snapped onto a street on toggle. road_at() answers from citygen_road_at().
#define CG_PPM  RN2_PPM
static float cg_ox = RN2_SPAWN_X, cg_oy = RN2_SPAWN_Y;

static void rn2_toggle(void) {
    rn2_on = !rn2_on;
    reset_vehicle();                             // fresh world, rig at world-px (0,0)
    if (rn2_on) {
        float rx, ry, rang;                      // snap the ORIGIN so (0,0) sits on the road
        if (wn_nearest_road_point(RN2_SPAWN_X, RN2_SPAWN_Y, &rx, &ry, &rang)) {
            rn2_ox = rx; rn2_oy = ry; ang = rang;
        } else { rn2_ox = RN2_SPAWN_X; rn2_oy = RN2_SPAWN_Y; }
    }
}

static RoadHit road_at(float x, float y) {
    RoadHit r; r.zone = zone_at(x, y); r.cls = r.zone; r.on_road = 0; r.on_pave = 0;

    if (cg_on) {                                             // ── Rung 5.5c: a generated citygen city ──
        CityHit h = citygen_road_at(cg_ox + x / CG_PPM, cg_oy + y / CG_PPM);
        r.on_road = h.on_road; r.on_pave = h.on_road;        // (walk the street = pavement, for foot mode)
        r.cls = h.minor ? CL_STREET : CL_ARTERIAL;
        return r;
    }

    if (rn2_on) {                                            // ── Rung A1: the roadnet2 spine ──
        WnHit h = wn_road_at(rn2_ox + x / RN2_PPM, rn2_oy + y / RN2_PPM);
        r.on_road = h.on_road; r.on_pave = h.on_pave; r.cls = h.cls; r.zone = h.zone;
        return r;
    }

    if (osm_loaded) {                                        // ── OSM: real Delft roads ──
        float mx = osm_cx + x / OSM_PPM;                     // pixels → metres, rig origin = bbox centre
        float my = osm_cy - y / OSM_PPM;                     // Y flip (north-up metres vs y-down pixels)
        int col = osm_cellcol(mx), row = osm_cellrow(my);
        float best = 1e30f;
        for (int cc = col - 1; cc <= col + 1; cc++)
            for (int rr = row - 1; rr <= row + 1; rr++) {
                if (cc < 0 || rr < 0 || cc >= gcol || rr >= grow) continue;
                int cell = rr * gcol + cc;
                for (int k = ocell_start[cell]; k < ocell_start[cell + 1]; k++) {
                    int s = oidx[k];
                    float d2 = osm_pt_seg_d2(mx, my, oseg[s].ax, oseg[s].ay, oseg[s].bx, oseg[s].by);
                    if (d2 <= oseg[s].hw * oseg[s].hw && d2 < best) { best = d2; r.on_road = 1; r.cls = oseg[s].cls; }
                    float pv = oseg[s].hw + FOOT_PAVE_M;     // F1: the pavement strip beside the carriageway
                    if (d2 <= pv * pv) r.on_pave = 1;
                }
            }
        return r;
    }

    r.cls = r.zone;                                          // ── STUB: sloop's own grid world ──
    int p = ZONE_PITCH[r.zone]; float hw = ZONE_LANE[r.zone] * 0.5f;
    float qx = x / p, qy = y / p;                            // nearest grid line in x / y (bands centred on k*p)
    float dx = x - (float)((int)(qx >= 0 ? qx + 0.5f : qx - 0.5f)) * p;
    float dy = y - (float)((int)(qy >= 0 ? qy + 0.5f : qy - 0.5f)) * p;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    r.on_road = (dx <= hw || dy <= hw);
    float pv = hw + FOOT_PAVE_M * OSM_PPM;                   // F1: same strip, stub world (px)
    r.on_pave = (dx <= pv || dy <= pv);
    return r;
}

// Rung B step 4: RENDER the OSM roads (when a .rvb is loaded), so you drive the real city — not the
// stub grid. Colour by class (roadview's palette). Each segment is a GPU rectfill_rot ribbon (not a
// software thickline) so it stays hole-free under the heading-up camera, same as the vehicle cells.
// The metres→world-px map is the exact inverse of road_at()'s, so what you SEE == what road_at() says.
static int osm_road_col(int kind) {
    switch (kind) {
        case 0:  return CLR_ORANGE;        // highway
        case 1:  return CLR_YELLOW;        // arterial
        case 2:  return CLR_LIGHT_GREY;    // street
        case 3:  return CLR_BROWN;         // track
        case 17: return CLR_LIGHT_YELLOW;  // secondary
        case 18: return CLR_PEACH;         // tertiary
        case 19: return CLR_MEDIUM_GREY;   // service
        case 20: return CLR_DARK_RED;      // cycleway (Dutch red)
        case 21: return CLR_INDIGO;        // footway
        default: return CLR_LIGHT_GREY;
    }
}

static void draw_osm_roads(void) {
    float vhw, vhh; view_half_extent(&vhw, &vhh);              // widen for zoom + heading-up rotation
    int mx = (int)(vhw - SCREEN_W * 0.5f) + 8, my = (int)(vhh - SCREEN_H * 0.5f) + 8;
    float L = cam_x - mx, R = cam_x + SCREEN_W + mx, T = cam_y - my, B = cam_y + SCREEN_H + my;
    for (int s = 0; s < n_oseg; s++) {
        float ax = (oseg[s].ax - osm_cx) * OSM_PPM, ay = -(oseg[s].ay - osm_cy) * OSM_PPM;  // metres→world px
        float bx = (oseg[s].bx - osm_cx) * OSM_PPM, by = -(oseg[s].by - osm_cy) * OSM_PPM;  // (Y flip, = road_at's inverse)
        float w = oseg[s].hw * 2.0f * OSM_PPM; if (w < 1.0f) w = 1.0f;
        float loX = ax < bx ? ax : bx, hiX = ax > bx ? ax : bx;                             // cull to the view (+width margin)
        float loY = ay < by ? ay : by, hiY = ay > by ? ay : by;
        if (hiX < L - w || loX > R + w || hiY < T - w || loY > B + w) continue;
        float dx = bx - ax, dy = by - ay, len = fsqrt(dx * dx + dy * dy);
        float deg = atan2f(dy, dx) * 57.29578f;
        // len + w so consecutive segments overlap at the joint (no gap); a stub of length w draws a cap.
        rectfill_rot((int)((ax + bx) * 0.5f), (int)((ay + by) * 0.5f),
                     (int)(len + w), (int)w, deg, osm_road_col(oseg[s].cls));
    }
}

// RUNG A1 renderer: terrain from the shared heightmap + roads stroked from the SAME cached
// edge pool the query answers from (screen == road_at, literally the same data). Ribbons are
// rectfill_rot like the OSM renderer so they stay hole-free under the heading-up camera.
static int rn2_road_col(int cls) {
    switch (cls) {
        case CL_MOTORWAY: return CLR_LIGHT_GREY;
        case CL_HIGHWAY:  return CLR_LIGHT_GREY;
        case CL_ARTERIAL: return CLR_LIGHT_GREY;
        case CL_STREET:   return CLR_MEDIUM_GREY;
        default:          return CLR_BROWN;        // dirt
    }
}
static void draw_rn2_world(void) {
    float vhw, vhh; view_half_extent(&vhw, &vhh);              // widen for zoom + rotation
    int step = 16;                                             // biome blocks: 16 px = 4 m
    int mx = (int)(vhw - SCREEN_W * 0.5f) + step, my = (int)(vhh - SCREEN_H * 0.5f) + step;
    int x0 = ((int)(cam_x - mx) / step - 1) * step, y0 = ((int)(cam_y - my) / step - 1) * step;
    int x1 = (int)cam_x + SCREEN_W + mx, y1 = (int)cam_y + SCREEN_H + my;
    for (int y = y0; y <= y1; y += step)
        for (int x = x0; x <= x1; x += step)
            rectfill(x, y, step, step,
                     biome_col(height_at(rn2_ox + x / RN2_PPM, rn2_oy + y / RN2_PPM)));
    // roads — rig-anchored cache, the same eg_ensure the query runs
    eg_ensure(rn2_ox + sx / RN2_PPM, rn2_oy + sy / RN2_PPM);
    float L = cam_x - mx, R = cam_x + SCREEN_W + mx, T = cam_y - my, B = cam_y + SCREEN_H + my;
    for (int e = 0; e < eg_n; e++) {
        float w = ROAD_HW_M[eg_e[e].cls] * 2.0f * RN2_PPM;
        int col = rn2_road_col(eg_e[e].cls);
        for (int i = 0; i < LINK_SAMPLES; i++) {
            float ax = (eg_e[e].x[i]   - rn2_ox) * RN2_PPM, ay = (eg_e[e].y[i]   - rn2_oy) * RN2_PPM;
            float bx = (eg_e[e].x[i+1] - rn2_ox) * RN2_PPM, by = (eg_e[e].y[i+1] - rn2_oy) * RN2_PPM;
            float loX = ax < bx ? ax : bx, hiX = ax > bx ? ax : bx;                 // cull to the view
            float loY = ay < by ? ay : by, hiY = ay > by ? ay : by;
            if (hiX < L - w || loX > R + w || hiY < T - w || loY > B + w) continue;
            float ddx = bx - ax, ddy = by - ay, len = fsqrt(ddx*ddx + ddy*ddy);
            float deg = atan2f(ddy, ddx) * 57.29578f;
            int c = (eg_e[e].br[i] || eg_e[e].br[i+1]) ? CLR_BROWN : col;           // bridge deck
            rectfill_rot((int)((ax + bx) * 0.5f), (int)((ay + by) * 0.5f),
                         (int)(len + w), (int)w, deg, c);
        }
    }
}

// Rung 5.5c: drop into a generated citygen city near the spawn, rig ON a street.
static void cg_toggle(void) {
    if (osm_loaded) return;                       // a dragged .rvb owns the world
    cg_on = !cg_on;
    if (cg_on) rn2_on = 0;                         // one world at a time
    reset_vehicle();                               // fresh world, rig at world-px (0,0)
    if (cg_on) {
        if (citygen_pick_city(RN2_SPAWN_X, RN2_SPAWN_Y)) {
            float rx, ry, rang;                    // snap the ORIGIN onto a street, face along it
            if (citygen_nearest_street(ar_cx, ar_cy, &rx, &ry, &rang)) {
                cg_ox = rx; cg_oy = ry; ang = rang;
            } else { cg_ox = ar_cx; cg_oy = ar_cy; }
        } else cg_on = 0;                          // no city near spawn — bail
    }
}

// draw the generated city: biome ground + arterials (light) + minor streets
// (medium), inverse-mapped so what you SEE is what road_at() reads (screen == query).
static void draw_cg_world(void) {
    float vhw, vhh; view_half_extent(&vhw, &vhh);
    int step = 16;
    int mx = (int)(vhw - SCREEN_W * 0.5f) + step, my = (int)(vhh - SCREEN_H * 0.5f) + step;
    int x0 = ((int)(cam_x - mx) / step - 1) * step, y0 = ((int)(cam_y - my) / step - 1) * step;
    int x1 = (int)cam_x + SCREEN_W + mx, y1 = (int)cam_y + SCREEN_H + my;
    for (int y = y0; y <= y1; y += step)
        for (int x = x0; x <= x1; x += step)
            rectfill(x, y, step, step, biome_col(height_at(cg_ox + x / CG_PPM, cg_oy + y / CG_PPM)));
    float L = cam_x - mx, R = cam_x + SCREEN_W + mx, T = cam_y - my, B = cam_y + SCREEN_H + my;
    #define CG_SEG(AX, AY, BX2, BY2, W, COL) do {                                          \
        float ax = ((AX) - cg_ox) * CG_PPM, ay = ((AY) - cg_oy) * CG_PPM;                  \
        float bx = ((BX2) - cg_ox) * CG_PPM, by = ((BY2) - cg_oy) * CG_PPM;                \
        float loX = ax<bx?ax:bx, hiX = ax>bx?ax:bx, loY = ay<by?ay:by, hiY = ay>by?ay:by;  \
        if (!(hiX < L-(W) || loX > R+(W) || hiY < T-(W) || loY > B+(W))) {                 \
            float ddx = bx-ax, ddy = by-ay, len = fsqrt(ddx*ddx + ddy*ddy);                \
            rectfill_rot((int)((ax+bx)*0.5f), (int)((ay+by)*0.5f),                         \
                         (int)(len+(W)), (int)(W), atan2f(ddy,ddx)*57.29578f, (COL));      \
        } } while (0)
    float aw = CG_ART_HW * 2.0f * CG_PPM, mw = CG_MINOR_HW * 2.0f * CG_PPM;
    for (int e = 0; e < me_n; e++)                 // minors first (arterials paint over)
        CG_SEG(msx[mea[e]], msy[mea[e]], msx[meb[e]], msy[meb[e]], mw, CLR_MEDIUM_GREY);
    for (int l = 0; l < ar_nl; l++)
        for (int i = 0; i + 1 < ar_np[l]; i++)
            CG_SEG(ar_px[l][i], ar_py[l][i], ar_px[l][i+1], ar_py[l][i+1], aw, CLR_LIGHT_GREY);
    #undef CG_SEG
}

// Houses are now SOLID obstacles (§9): generated per-chunk in gen_chunk (same ~5 m-facade tiling
// the old draw_houses used) and drawn from the pool in draw_obstacles, so they can be crashed into
// and (when smashed) stay demolished. draw_course only paints the flat road + fields under them.
static void draw_course(void) {
    if (cg_on)  { draw_cg_world();  return; }                  // Rung 5.5c: a generated citygen city
    if (rn2_on) { draw_rn2_world(); return; }                  // Rung A1: the roadnet2 spine
    if (osm_loaded) { draw_osm_roads(); return; }              // Rung B: real Delft roads (screen == road_at)

    // widen the drawn area to cover the speed-zoom pull-back (see draw_ground)
    float vhw, vhh; view_half_extent(&vhw, &vhh);   // widen for zoom + heading-up rotation
    int mx = (int)(vhw - SCREEN_W * 0.5f) + 4;
    int my = (int)(vhh - SCREEN_H * 0.5f) + 4;
    int L = (int)cam_x - mx, R = (int)cam_x + SCREEN_W + mx, T = (int)cam_y - my, B = (int)cam_y + SCREEN_H + my;
    cur_zone = zone_at(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f);
    int p = ZONE_PITCH[cur_zone], hw = ZONE_LANE[cur_zone] / 2;
    int kx0 = ifloordiv(L, p) - 1, kx1 = ifloordiv(R, p) + 1;
    int ky0 = ifloordiv(T, p) - 1, ky1 = ifloordiv(B, p) + 1;

    // 0. fields between rural+ roads (green); city/town houses are pool obstacles now (draw_obstacles)
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            if (cur_zone >= Z_RURAL && (hash2(kx, ky) & 1)) {          // patchwork fields
                int fx = kx * p + hw + 4, fy = ky * p + hw + 4;
                rectfill(fx, fy, p - ZONE_LANE[cur_zone] - 8, p - ZONE_LANE[cur_zone] - 8,
                         (hash2(kx, ky) & 2) ? CLR_DARK_GREEN : CLR_BROWN);
            }
        }

    // 1. tarmac bands (lighter than ground), crossing in a grid at the zone pitch
    for (int kx = kx0; kx <= kx1; kx++)
        rectfill(kx * p - hw, T - 1, hw * 2, (B - T) + 2, CLR_DARK_GREY);
    for (int ky = ky0; ky <= ky1; ky++)
        rectfill(L - 1, ky * p - hw, (R - L) + 2, hw * 2, CLR_DARK_GREY);

    // 2. curbs + dashed centre line; SUPER adds a 2nd dashed line (multi-lane).
    // 2px-wide rectfill BANDS, not 1px line()s — a 1px line scaled by the <1 speed-zoom renders at
    // sub-pixel width and sparkles/drops out; a 2px band still covers ≥1 full pixel, so it stays solid.
    int H = B - T, W = R - L;
    for (int kx = kx0; kx <= kx1; kx++) {
        int cx = kx * p;
        rectfill(cx - hw - 1, T, 2, H, CLR_LIGHT_GREY);   // curbs straddle the band edges
        rectfill(cx + hw - 1, T, 2, H, CLR_LIGHT_GREY);
        if (cur_zone != Z_CITY)               // city streets are one-way → no centre line
            for (int y = ifloordiv(T, 24) * 24; y < B; y += 24) rectfill(cx - 1, y, 2, 12, CLR_YELLOW);
        if (cur_zone == Z_SUPER)
            for (int y = ifloordiv(T, 20) * 20; y < B; y += 20) {
                rectfill(cx - hw / 2 - 1, y, 2, 10, CLR_MEDIUM_GREY);
                rectfill(cx + hw / 2 - 1, y, 2, 10, CLR_MEDIUM_GREY);
            }
    }
    for (int ky = ky0; ky <= ky1; ky++) {
        int cy = ky * p;
        rectfill(L, cy - hw - 1, W, 2, CLR_LIGHT_GREY);
        rectfill(L, cy + hw - 1, W, 2, CLR_LIGHT_GREY);
        if (cur_zone != Z_CITY)
            for (int x = ifloordiv(L, 24) * 24; x < R; x += 24) rectfill(x, cy - 1, 12, 2, CLR_YELLOW);
        if (cur_zone == Z_SUPER)
            for (int x = ifloordiv(L, 20) * 20; x < R; x += 20) {
                rectfill(x, cy - hw / 2 - 1, 10, 2, CLR_MEDIUM_GREY);
                rectfill(x, cy + hw / 2 - 1, 10, 2, CLR_MEDIUM_GREY);
            }
    }

    // 3. town roundabouts (rounded corners to steer around) + city zebra crossings
    if (cur_zone == Z_TOWN)
        for (int kx = kx0; kx <= kx1; kx++)
            for (int ky = ky0; ky <= ky1; ky++)
                if ((hash2(kx, ky) & 3) == 0) {
                    int cx = kx * p, cy = ky * p;
                    circfill(cx, cy, hw, CLR_DARK_GREEN);
                    circ(cx, cy, hw, CLR_LIGHT_GREY);
                }
    if (cur_zone == Z_CITY)                                  // pedestrian/school crossings
        for (int kx = kx0; kx <= kx1; kx++)
            for (int ky = ky0; ky <= ky1; ky++)
                if ((hash2(kx, ky) & 3) == 0) {
                    int cx = kx * p, cy = ky * p;
                    for (int s = -hw + 2; s < hw; s += 4) rectfill(cx + s, cy - hw, 2, hw * 2, CLR_WHITE);
                }
}

// glow ramp for a scraping cell — red (cold/parked) → orange → yellow → white (hot)
static int hot_col(void) {
    return heat > 0.66f ? CLR_WHITE : heat > 0.40f ? CLR_YELLOW
         : heat > 0.15f ? CLR_ORANGE : CLR_RED;
}

static const char *trans_label(void) {
    return trans_mode == TR_SINGLE ? "1-GEAR" : trans_mode == TR_AUTO ? "AUTO" : "MANUAL";
}

// drivetrain label from where power hits the ground vs the COM (front=pull, rear=push)
static const char *drive_label(void) {
    if (nWheels == 0) return "no drive";
    if (nDrive == 0)  return "AWD";                 // no drive wheels placed → all wheels
    float off = driveX - comX;
    return off > CELL * 0.5f ? "FWD pull" : off < -CELL * 0.5f ? "RWD push" : "AWD";
}

// a part's colour — engine cells tint to the rig's engine KIND, everything else fixed
static int part_col(int p) { return (p == P_ENGINE) ? ENG[eng_kind].col : KIND[p].col; }

// ── vehicle: draw each occupied cell as a rotated quad ───────────────────────
static void draw_cell(int r, int c, int p) {
    float lx = (c + 0.5f) * CELL - comX, ly = (r + 0.5f) * CELL - comY;
    float px, py; rot(lx, ly, &px, &py);                  // cell centre in world (rot spins it by the rig heading)
    int col = dragging[r][c] ? hot_col() : part_col(p);   // scraping cells glow
    // GPU rotated quad (not software polyfill) so the rig stays hole-free at any heading under the
    // spinning camera. +1px so abutting cells overlap a hair — no seam between parts of a different colour.
    rectfill_rot((int)px, (int)py, (int)CELL + 1, (int)CELL + 1, ang, col);
    if (p == P_CASTER || p == P_DRIVE) {          // a hub dot: grey = swivel caster, orange = powered
        float px, py; rot(lx, ly, &px, &py);
        pset((int)px, (int)py, p == P_DRIVE ? CLR_ORANGE : CLR_LIGHT_GREY);
    }
}

static void draw_vehicle(void) {
    // body cells first, wheels/casters on top so they read as round-ish dark pads
    for (int pass = 0; pass < 2; pass++)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++) {
                int p = grid[r][c];
                if (p == P_NONE) continue;
                int isWheel = (p == P_WHEEL || p == P_CASTER || p == P_DRIVE);
                if (isWheel != pass) continue;
                draw_cell(r, c, p);
            }
    // a nose marker so heading is unmistakable (at the rig's actual front edge)
    float fr = frontX - comX;
    float nx, ny, tx, ty;
    rot(fr + 1, -2, &nx, &ny);
    rot(fr + 1, 2, &tx, &ty);
    float bx, by; rot(fr + 6, 0, &bx, &by);
    trifill((int)nx, (int)ny, (int)tx, (int)ty, (int)bx, (int)by, CLR_YELLOW);
    // COM crosshair (the readout that makes the physics visible — pays off in BUILD)
    line((int)sx - 2, (int)sy, (int)sx + 2, (int)sy, CLR_WHITE);
    line((int)sx, (int)sy - 2, (int)sx, (int)sy + 2, CLR_WHITE);
}

// ── FOOT render (rung F0) ─────────────────────────────────────────────────────
// The avatar is ONE RIG TILE of person: a seat-blue torso disc (you ARE the seat's
// occupant — same colour, same scale) with a skin-tone head pixel leading the walk.
static void draw_foot(void) {
    circfill((int)foot_x, (int)foot_y, 3, KIND[P_SEAT].col);
    pset((int)(foot_x + foot_dx * 1.5f), (int)(foot_y + foot_dy * 1.5f), CLR_LIGHT_PEACH);
    // within reach of the seat: ring the door so "F gets in HERE" is visible, not guessed
    float slx, sly, swx, swy;
    if (seat_local(&slx, &sly)) {
        rot(slx, sly, &swx, &swy);
        float dx = foot_x - swx, dy = foot_y - swy;
        if (dx * dx + dy * dy < FOOT_REACH * FOOT_REACH) circ((int)swx, (int)swy, 6, CLR_YELLOW);
    }
}

// on-foot HUD: one line — the seat is the only interface out here
static void hud_foot(void) {
    float slx, sly, swx, swy; int near_seat = 0;
    if (seat_local(&slx, &sly)) {
        rot(slx, sly, &swx, &swy);
        float dx = foot_x - swx, dy = foot_y - swy;
        near_seat = dx * dx + dy * dy < FOOT_REACH * FOOT_REACH;
    }
    print(near_seat ? "F  GET IN" : "ARROWS WALK   Z JOG   WALK TO YOUR SEAT TO DRIVE",
          6, SCREEN_H - 12, near_seat ? CLR_YELLOW : CLR_LIGHT_GREY);
}

// ── HUD: a touch/mouse/keyboard COCKPIT ───────────────────────────────────────
// The bottom band is a driveable dashboard, split for two thumbs: gas/brake/drift
// pedals at the far-LEFT edge, the steering wheel (press its left/right half) at the
// far-RIGHT edge, and in the middle a digital speed readout, a round RPM tach, the
// gear selector (MANUAL = a tappable H-gate; AUTO/1 = a D/N/R selector) and the
// ignition/trans/build buttons. Every control reads keyboard, touch AND mouse (see
// the helpers + handle_input). Play area + road-sign limit sit above.
static void hud(void) {
    char buf[48];
    float spd = fsqrt(vx * vx + vy * vy);

    // --- top of screen: rig identity (dim) + the zone's limit (a road sign) ----
    print(DES_NAME[cur_des], 4, 4, CLR_DARK_GREY);
    print(ENG[eng_kind].name, 4, 12, ENG[eng_kind].col);   // the rig's engine kind (§1a)
    // top-centre: the real CITY name when driving OSM data, else the procedural zone label
    print_centered(cg_on ? "GENERATED CITY" : rn2_on ? "ROADNET2 SPINE" : osm_loaded ? osm_name : ZONE_NAME[cur_zone],
                   SCREEN_W / 2, 4, CLR_YELLOW);
    { RoadHit rh = road_at(sx, sy);   // P1 seam, live AT THE RIG COM — matches the surface driving handling
      print_centered(rh.on_road ? "ON ROAD" : "OFF-ROAD", SCREEN_W / 2, 12, rh.on_road ? CLR_GREEN : CLR_ORANGE); }
    // discoverability: the OSM run-path is invisible otherwise. A transient call-to-action — shows for
    // the first few seconds of a fresh run, then clears so it never clutters the default cart.
    { static int hint_t = 0; if (hint_t < 300) hint_t++;
      if (!osm_loaded && !rn2_on && hint_t < 300)
          print_centered("drag a .rvb here to drive a real city", SCREEN_W / 2, 20, CLR_DARK_GREY); }
    if (nDrag > 0) {                         // scrape heat — shown only while it bites
        print("SCRAPE", SCREEN_W - 52, 4, hot_col());
        bar(SCREEN_W - 70, 13, 62, 4, heat, heat > 0.66f ? CLR_RED : CLR_ORANGE, CLR_DARKER_GREY);
    }

    // --- warning banner just above the dashboard --------------------------------
    if (stalled)
        print_centered("\x07 STALLED  tap IGN", SCREEN_W / 2, DASH_Y - 9, blink(16) ? CLR_RED : CLR_DARK_GREY);
    else if (!engine_on)
        print_centered("ENGINE OFF  tap IGN", SCREEN_W / 2, DASH_Y - 9, CLR_MEDIUM_GREY);
    else if (tip_amt > 0.05f)
        print_centered("\x07 TIPPING", SCREEN_W / 2, DASH_Y - 9, CLR_ORANGE);
    else if (slide_amt > 0.18f)                      // a tyre let go past the friction limit
        print_centered(slide_rear ? "\x07 SLIDE" : "\x07 PUSH", SCREEN_W / 2, DASH_Y - 9,
                       slide_rear ? CLR_RED : CLR_ORANGE);   // rear out (oversteer) vs front wash (understeer)

    // --- the dashboard panel ----------------------------------------------------
    rectfill(0, DASH_Y, SCREEN_W, SCREEN_H - DASH_Y, CLR_BLACK);
    line(0, DASH_Y, SCREEN_W - 1, DASH_Y, CLR_DARK_GREY);

    // STEERING WHEEL — grab the rim and turn it (touch / mouse); the arrows show the
    // steer direction, and the rim lights up while you're holding it.
    int hl = steer_pos < -0.05f;
    int hr = steer_pos >  0.05f;
    int rim = (steer_grab ? CLR_WHITE : CLR_LIGHT_GREY);
    circ(WHEEL_CX, WHEEL_CY, WHEEL_R, rim);
    circ(WHEEL_CX, WHEEL_CY, WHEEL_R - 1, CLR_MEDIUM_GREY);
    for (int k = 0; k < 3; k++) {            // three spokes, rotated by the eased wheel angle
        float sa = wheel_ang + 90.0f + k * 120.0f;
        line(WHEEL_CX, WHEEL_CY,
             WHEEL_CX + (int)((WHEEL_R - 2) * cos_deg(sa)),
             WHEEL_CY - (int)((WHEEL_R - 2) * sin_deg(sa)), CLR_MEDIUM_GREY);
    }
    circfill(WHEEL_CX, WHEEL_CY, 4, CLR_LIGHT_GREY);
    print("\x1b", WHL_X + 2,          WHEEL_CY - 3, hl ? CLR_WHITE : CLR_DARK_GREY);   // ◄
    print("\x1a", WHL_X + WHL_W - 10, WHEEL_CY - 3, hr ? CLR_WHITE : CLR_DARK_GREY);   // ►

    // PEDALS — gas / brake / drift (held)
    dash_btn(PED_X, GAS_Y, PED_W, PED_H - 1, "GAS",   "Z",     in_gas,  CLR_GREEN);
    dash_btn(PED_X, BRK_Y, PED_W, PED_H - 1, "BRAKE", "X",     in_brk,  CLR_RED);
    dash_btn(PED_X, DRF_Y, PED_W, PED_H - 1, "DRIFT", "SPC",   in_hand, CLR_YELLOW);

    // SPEED — digital LED readout
    rectfill(SPD_X, SPD_Y, 50, 26, CLR_DARKER_GREY);
    rect(SPD_X, SPD_Y, 50, 26, CLR_DARK_GREY);
    snprintf(buf, sizeof buf, "%3.0f", spd * KMH);
    print_scaled(buf, SPD_X + 2, SPD_Y + 4, CLR_GREEN, 2);
    font(FONT_SMALL); print_centered("KM/H", SPD_X + 25, SPD_Y + 20, CLR_MEDIUM_GREY); font(FONT_NORMAL);

    // CAMERA mode toggle (N-UP = grid aligned · H-UP = your travel points up) + the SCALING
    // A/B (STEP = quantized zoom-out · SMTH = smooth, no crawl) — both tap- and key-toggleable
    dash_btn(CAM_BTN_X, CAM_BTN_Y, CAM_BTN_W, CAM_BTN_H, cam_head_up ? "H-UP" : "N-UP", "C", cam_head_up, CLR_BLUE);
    dash_btn(SCL_BTN_X, SCL_BTN_Y, SCL_BTN_W, SCL_BTN_H, smooth_mode ? "SMTH" : "STEP", "V", smooth_mode, CLR_LIME_GREEN);

    // RPM — round tach with ticks + a needle (240° sweep, 210°→-30°)
    circfill(DIAL_CX, DIAL_CY, DIAL_R, CLR_DARKER_GREY);
    circ(DIAL_CX, DIAL_CY, DIAL_R, CLR_DARK_GREY);
    for (int t = 0; t <= 10; t++) {
        float frac = t / 10.0f, ad = 210.0f - frac * 240.0f;
        line(DIAL_CX + (int)((DIAL_R - 1) * cos_deg(ad)), DIAL_CY - (int)((DIAL_R - 1) * sin_deg(ad)),
             DIAL_CX + (int)((DIAL_R - 4) * cos_deg(ad)), DIAL_CY - (int)((DIAL_R - 4) * sin_deg(ad)),
             frac > 0.85f ? CLR_RED : CLR_MEDIUM_GREY);
    }
    float nd = 210.0f - clamp(rpm, 0, 1.0f) * 240.0f;
    line(DIAL_CX, DIAL_CY,
         DIAL_CX + (int)((DIAL_R - 4) * cos_deg(nd)), DIAL_CY - (int)((DIAL_R - 4) * sin_deg(nd)),
         !engine_on ? CLR_DARK_GREY : rpm > 0.92f ? CLR_RED : CLR_WHITE);
    circfill(DIAL_CX, DIAL_CY, 2, CLR_LIGHT_GREY);
    font(FONT_SMALL); print_centered("RPM", DIAL_CX, DIAL_CY + DIAL_R - 10, CLR_MEDIUM_GREY); font(FONT_NORMAL);

    // GEAR SELECTOR — mode-aware. Every slot is its own tap target (tap R to reverse).
    //   MANUAL : a real H-gate, 1·3·5 top / 2·4·R bottom, N in the centre channel — tap
    //            any gear to grab it directly; the engaged slot is filled white.
    //   AUTO/1 : a simple D / N / R selector (the box manages the forward gears itself).
    // R lights orange when you're slow enough to drop into it. Keys E / Q still shift too.
    int gear_slow = (spd < REV_ENGAGE_SPD);
    rectfill(GEAR_X, GEAR_Y, GEAR_W, GEAR_H, CLR_BLACK);
    rect(GEAR_X, GEAR_Y, GEAR_W, GEAR_H, CLR_DARK_GREY);
    if (trans_mode == TR_MANUAL) {
        // chrome H behind the slots: three verticals joined by the centre channel
        int colx[3] = { GEAR_X + GEAR_W / 6, GEAR_X + GEAR_W / 2, GEAR_X + 5 * GEAR_W / 6 };
        int gtopy = GEAR_Y + 9, gboty = GEAR_Y + GEAR_H - 9, gmidy = GEAR_Y + GEAR_H / 2;
        for (int c = 0; c < 3; c++) line(colx[c], gtopy, colx[c], gboty, CLR_DARK_GREY);
        line(colx[0], gmidy, colx[2], gmidy, CLR_DARK_GREY);
        static const int   MG[7] = { 1, 2, 3, 4, 5, 0, -1 };
        static const char *ML[7] = { "1", "2", "3", "4", "5", "N", "R" };
        int ballx = colx[1], bally = gmidy;             // ball rides the engaged gear
        for (int i = 0; i < 7; i++) {
            int gx, gy, gw, gh; mgate_rect(MG[i], &gx, &gy, &gw, &gh);
            int on = (gear == MG[i]);
            int held = ctl_held(gx, gy, gw, gh);
            if (on) { rectfill(gx + 1, gy + 1, gw - 2, gh - 2, CLR_DARKER_GREY);
                      ballx = gx + gw / 2; bally = gy + gh / 2; }
            int col = on   ? CLR_WHITE
                    : (MG[i] == -1) ? (gear_slow ? CLR_ORANGE : CLR_DARK_GREY)   // R ready when slow
                    : held ? CLR_WHITE : CLR_MEDIUM_GREY;
            print_centered(ML[i], gx + gw / 2, gy + gh / 2 - 3, col);
        }
        circfill(ballx, bally, 2, engine_on ? CLR_WHITE : CLR_DARK_GREY);
        font(FONT_TINY); print("E/Q", GEAR_X + 2, GEAR_Y - 7, CLR_DARK_GREY); font(FONT_NORMAL);
    } else {
        static const char *AL[3]  = { "DRIVE", "NEUTRAL", "REVERSE" };
        static const int   AGv[3] = { 1, 0, -1 };
        font(FONT_SMALL);
        for (int i = 0; i < 3; i++) {
            int gx, gy, gw, gh; agate_rect(i, &gx, &gy, &gw, &gh);
            int on  = (AGv[i] == 1) ? (gear >= 1) : (gear == AGv[i]);
            int rdy = (AGv[i] == -1 && gear_slow);
            int actcol = (AGv[i] == -1) ? CLR_RED : (AGv[i] == 0) ? CLR_MEDIUM_GREY : CLR_GREEN;
            rectfill(gx, gy, gw, gh, on ? actcol : CLR_DARKER_GREY);
            rect(gx, gy, gw, gh, CLR_DARK_GREY);
            print_centered(AL[i], gx + gw / 2, gy + gh / 2 - 2,
                           on ? CLR_BLACK : rdy ? CLR_ORANGE : CLR_LIGHT_GREY);
        }
        font(FONT_NORMAL);
    }

    // MODE BUTTONS — ignition (lit when running) / transmission / build
    dash_btn(MODE_BTN_X, IGN_Y, BTN_W, BTN_H, engine_on ? "IGN ON" : "IGN OFF", "I", engine_on, CLR_GREEN);
    dash_btn(MODE_BTN_X, TRN_Y, BTN_W, BTN_H, trans_label(), "G", 0, CLR_DARKER_GREY);
    dash_btn(MODE_BTN_X, BLD_Y, BTN_W, BTN_H, "BUILD", "TAB", 0, CLR_DARKER_GREY);

    if (is_paused) print_centered("PAUSED", SCREEN_W / 2, SCREEN_H / 2 - 20, CLR_WHITE);
}

// ── BUILD mode: a paused grid editor — place parts, watch the numbers move ───
#define ED_CELL 13                        // editor cell size (px) — smaller now the grid is 9 wide,
                                          // so 9 cells (117px) still fit between palette and readout
#define ED_X    76                        // grid left (palette to its left, readout to its right)
#define ED_Y    56

static void draw_build(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();

    // back to driving (touch/mouse; TAB also works)
    if (ui_button(6, 6, 60, 18, "\x10 drive")) { mode = MODE_DRIVE; reset_vehicle(); }

    // --- part palette (left) ---------------------------------------------------
    static const int PAL[] = { P_FRAME, P_ENGINE, P_WHEEL, P_DRIVE, P_CASTER, P_SEAT, P_CARGO, P_NONE };
    static const char *PAL_LBL[] = { "frame", "engine", "wheel", "drive", "caster", "seat", "cargo", "erase" };
    print("PARTS", 6, 28, CLR_LIGHT_GREY);
    for (int i = 0; i < 8; i++) {                  // spacing tightened to 18 so 8 fit above the hint
        int by = 38 + i * 18;
        if (ui_button(6, by, 60, 16, PAL_LBL[i])) sel_part = PAL[i];
        if (PAL[i] == sel_part) rect(5, by - 1, 62, 18, CLR_WHITE);     // selection ring
        if (PAL[i] != P_NONE) rectfill(56, by + 4, 8, 8, part_col(PAL[i]));  // colour chip
        if (PAL[i] == P_DRIVE) pset(60, by + 7, CLR_ORANGE);            // powered-hub mark
    }

    // --- the grid editor (centre) ---------------------------------------------
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int x = ED_X + c * ED_CELL, y = ED_Y + r * ED_CELL;
            int p = grid[r][c];
            if (p == P_NONE) { rect(x, y, ED_CELL, ED_CELL, CLR_DARK_GREY); }
            else {
                rectfill(x + 1, y + 1, ED_CELL - 2, ED_CELL - 2, part_col(p));
                rect(x, y, ED_CELL, ED_CELL, dragging[r][c] ? CLR_RED : CLR_DARKER_GREY);
                if (dragging[r][c])               // hangs past the wheels → will scrape
                    rect(x + 1, y + 1, ED_CELL - 2, ED_CELL - 2, CLR_ORANGE);
                if (p == P_CASTER) circfill(x + ED_CELL / 2, y + ED_CELL / 2, 2, CLR_LIGHT_GREY);
                if (p == P_DRIVE)  circfill(x + ED_CELL / 2, y + ED_CELL / 2, 2, CLR_ORANGE);
            }
        }
    // "front" arrow on the right edge — the rig faces +x
    print("\x10", ED_X + GW * ED_CELL + 2, ED_Y + GH * ED_CELL / 2 - 3, CLR_YELLOW);
    print("front \x10", ED_X + 64, ED_Y - 9, CLR_MEDIUM_GREY);

    // support polygon — the hull of the wheels, drawn over the grid. The COM living
    // inside a roomy quad = stable; a 3-wheeler's triangle with the COM near an edge
    // = tips that way. Makes the tip model visible the way the COM crosshair does.
    if (nHull >= 3) {
        int col = (stabL < CELL * 0.6f || stabR < CELL * 0.6f) ? CLR_ORANGE : CLR_DARK_GREEN;
        for (int i = 0; i < nHull; i++) {
            int j = (i + 1) % nHull;
            int x0 = ED_X + (int)((hullX[i] + comX) / CELL * ED_CELL);
            int y0 = ED_Y + (int)((hullY[i] + comY) / CELL * ED_CELL);
            int x1 = ED_X + (int)((hullX[j] + comX) / CELL * ED_CELL);
            int y1 = ED_Y + (int)((hullY[j] + comY) / CELL * ED_CELL);
            line(x0, y0, x1, y1, col);
        }
    }

    // COM crosshair on the grid — shifts live as you place parts
    if (M > 1.01f) {
        int cx = ED_X + (int)(comX / CELL * ED_CELL), cy = ED_Y + (int)(comY / CELL * ED_CELL);
        line(cx - 4, cy, cx + 4, cy, CLR_WHITE);
        line(cx, cy - 4, cx, cy + 4, CLR_WHITE);
        print("COM", cx + 5, cy - 3, CLR_WHITE);
    }

    // per-wheel resting LOAD (§8 spring model) — heavier than average = warm, lighter = blue.
    // Shifts live as you place mass: drop cargo at the back and the rear wheels' numbers jump.
    if (nWheelP > 0 && M > 1.01f) {
        float avg = M / nWheelP;
        font(FONT_TINY);
        for (int i = 0; i < nWheelP; i++) {
            int wx = ED_X + wheelPC[i] * ED_CELL + 2, wy = ED_Y + wheelPR[i] * ED_CELL + 4;
            float L = wheelLoad[i];
            int col = (L < 0.5f) ? CLR_RED                  // ~lifted
                    : (L > avg * 1.25f) ? CLR_ORANGE        // heavy corner
                    : (L < avg * 0.75f) ? CLR_TRUE_BLUE     // light corner
                    : CLR_WHITE;
            char lb[8]; snprintf(lb, sizeof lb, "%d", (int)(L + 0.5f));
            print(lb, wx, wy, col);
        }
        print("load/wheel", ED_X, ED_Y + GH * ED_CELL + 1, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    // --- readouts (right) ------------------------------------------------------
    char buf[40];
    int rx = 206, ry = 40;
    print("READOUT", rx, ry, CLR_LIGHT_GREY); ry += 11;
    snprintf(buf, sizeof buf, "MASS  %4.1f", M);             print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    snprintf(buf, sizeof buf, "TOPSPD %3.0f km/h", est_top_speed() * KMH); print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    // 0-100: the honest power-to-weight readout — green = punchy, orange = sluggish, red = overloaded
    // (can't reach 100; add an engine or drop weight). Tells you a gutless build BEFORE you drive it.
    float zt = est_0_100();
    if (zt < 0) { print("0-100  never", rx, ry, CLR_RED); }   // can't reach 100 — add an engine / drop weight
    else {
        int zc = zt < 7.0f ? CLR_GREEN : zt < 13.0f ? CLR_ORANGE : CLR_RED;
        snprintf(buf, sizeof buf, "0-100  %.1fs", zt); print(buf, rx, ry, zc);
    }
    ry += 9;
    snprintf(buf, sizeof buf, "PWR/WT %4.1f", (nEngines * ENG[eng_kind].power) / M);  // grunt per mass
    print(buf, rx, ry, CLR_MEDIUM_GREY); ry += 9;
    snprintf(buf, sizeof buf, "TURN  %4.0f", est_turn_rate()); print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    snprintf(buf, sizeof buf, "ENG %d  WHL %d", nEngines, nWheels); print(buf, rx, ry, CLR_MEDIUM_GREY); ry += 11;
    // engine KIND (§1a) — cycle with K or this button. Sets power / mass / delivery curve,
    // so it moves TOPSPD + accel + the engine note. Cells tint to the kind's colour.
    if (ui_button(rx, ry, 82, 15, ENG[eng_kind].name)) do_kind();
    ry += 16;
    print(ENG[eng_kind].feel, rx, ry, ENG[eng_kind].col); ry += 11;
    // drivetrain: front = pull (stable), rear = push (loose); no drive wheels = AWD
    print(drive_label(), rx, ry, nDrive == 0 ? CLR_MEDIUM_GREY :
          (driveX - comX) < -CELL * 0.5f ? CLR_ORANGE : CLR_TRUE_BLUE); ry += 9;
    const char *bl = balance > 0.12f ? "understeer" : balance < -0.12f ? "oversteer" : "neutral";
    print(bl, rx, ry, balance > 0.12f ? CLR_ORANGE : balance < -0.12f ? CLR_TRUE_BLUE : CLR_MEDIUM_GREY);
    ry += 9;
    // stability: how far the COM can shift sideways before leaving the support polygon.
    // single-track (<3 wheels) balances; a 3-wheeler's triangle is tippy toward its gap.
    if (nWheels > 0) {
        const char *st; int sc;
        if (nHull < 3)                                  { st = "single-track"; sc = CLR_MEDIUM_GREY; }
        else if (stabL < CELL * 0.6f || stabR < CELL * 0.6f) { st = "tippy turns"; sc = CLR_ORANGE; }
        else                                            { st = "stable"; sc = CLR_TRUE_BLUE; }
        print(st, rx, ry, sc); ry += 9;
    }
    ry += 2;
    if (nEngines == 0) { print("no engine!", rx, ry, CLR_RED); ry += 9; }
    if (nWheels == 0)  { print("no wheels!", rx, ry, CLR_RED); ry += 9; }
    if (nDrag > 0) {                              // cells cantilevered past the wheels
        snprintf(buf, sizeof buf, "%d cell%s scrape!", nDrag, nDrag > 1 ? "s" : "");
        print(buf, rx, ry, CLR_ORANGE); ry += 9;
    }

    print("BUILD", 6, 4, CLR_WHITE);
    print("TAB drive  click place/erase  1-0,-,= templates (- semi, = bus)  K engine  R clear",
          SCREEN_W / 2 - 205, SCREEN_H - 12, CLR_MEDIUM_GREY);

    ui_end();

    // --- placement: click a grid cell (outside the palette) --------------------
    if (mouse_pressed(MOUSE_LEFT) || mouse_down(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y();
        int c = (mx - ED_X) / ED_CELL, r = (my - ED_Y) / ED_CELL;
        if (mx >= ED_X && my >= ED_Y && c >= 0 && c < GW && r >= 0 && r < GH) {
            if (grid[r][c] != sel_part) { grid[r][c] = sel_part; recompute_body(); }
        }
    }
}

void draw(void) {
    if (mode == MODE_BUILD) { draw_build(); return; }

    cls(CLR_DARKER_GREY);                 // asphalt
    camera_ex((int)cam_x, (int)cam_y, cam_zoom, cam_ang);   // speed-zoom + heading-up rotation
    draw_ground();
    draw_course();
    for (int i = 0; i < MAXSKID; i++)     // tire marks burned into the road
        if (skid[i].life > 0)
            pset((int)skid[i].x, (int)skid[i].y,
                 skid[i].life > 90 ? CLR_BLACK : CLR_BROWNISH_BLACK);
    draw_obstacles();                     // §9: cones etc. (under the rig — you drive over them)
    draw_vehicle();
    if (mode == MODE_FOOT) draw_foot();   // rung F0: the seat's occupant, out in the world
    for (int i = 0; i < MAXSPARK; i++)    // sparks thrown off the grinding belly (over the rig)
        if (spark[i].life > 0) {
            int col = spark[i].life > 12 ? CLR_WHITE : spark[i].col;   // hot core fades to its tint
            pset((int)spark[i].x, (int)spark[i].y, col);
        }
    camera(0, 0);
    if (mode == MODE_FOOT) hud_foot();    // no cockpit on foot — one line of HUD
    else {
        hud();
        if (foot_deny > 0)                // tried F while rolling (or seatless/boxed in)
            print("STOP FIRST \x07 F GETS YOU OUT", 6, DASH_Y - 10, CLR_YELLOW);
    }
}

void init(void) {
    // Rung B (OSM): if run with --data <file>.rvb (or $DE_DATA), load real roads + build the index.
    // No data → osm_loaded stays 0 and road_at() uses the stub grid world (cart unchanged by default).
    { const char *dp = de_data_path(); if (dp) osm_load(dp); }

    // part vocabulary (ordered by enum — no designated inits, libtcc-safe)
    //                            mass  power         grip  roll  drive colour           name
    KIND[P_NONE]   = (PartKind){ 0,    0,            0,    0,    0,    0,               "." };
    KIND[P_FRAME]  = (PartKind){ 1.0f, 0,            0,    0,    0,    CLR_LIGHT_GREY,  "frame" };
    KIND[P_ENGINE] = (PartKind){ 4.0f, ENGINE_POWER, 0,    0,    0,    CLR_RED,         "engine" };
    KIND[P_WHEEL]  = (PartKind){ 1.5f, 0,            1.0f, 1.0f, 0,    CLR_BLACK,       "wheel" };
    KIND[P_CASTER] = (PartKind){ 1.5f, 0,            0.12f,1.0f, 0,    CLR_DARK_GREY,   "caster" };
    KIND[P_SEAT]   = (PartKind){ 1.2f, 0,            0,    0,    0,    CLR_BLUE,        "seat" };
    KIND[P_DRIVE]  = (PartKind){ 1.6f, 0,            1.0f, 1.0f, 1.0f, CLR_BLACK,       "drive" };
    // a heavy dead-weight crate — no engine/wheel function, it's just MASS. Mount it fore vs
    // aft to feel the static weight-distribution lever (COM / balance / I / per-axle load all
    // shift) — and it's the ideal heavy payload to validate the §8 per-wheel model against.
    KIND[P_CARGO]  = (PartKind){ 8.0f, 0,            0,    0,    0,    CLR_BROWN,       "cargo" };

    // ╔══ ENGINE TUNING — START HERE ════════════════════════════════════════════════════════╗
    // Each engine kind is ONE row below. To tune the feel, edit the row — three dials:
    //   • power → thrust. Raises BOTH top speed and acceleration.
    //   • mass  → acceleration ONLY (accel = power÷total-mass). Top speed is mass-independent.
    //   • vref  → the GEARING = the top-speed CEILING, and it reads directly in km/h:
    //             vref 300 ⇒ caps at ~300 km/h, vref 45 ⇒ ~45. (Exact because the km/h factor
    //             KMH and the top-gear ratio are both 0.72 — keep them equal or this breaks.)
    //             Whether a rig actually REACHES its vref ceiling depends on power vs drag:
    //             strong + slippery hits it (RACE), weak/heavy/draggy drag-limits BELOW it.
    // So: "I want a 250 km/h car" → set vref 250 + enough power to reach it. "make it launch
    // harder" → more power or less mass. The BUILD readout (est_top_speed) shows the result live.
    //
    // Two deeper knobs, only if you want to reshape HOW power arrives (not how much):
    //   • torque CURVE shape → delivery() up top, one `case` per kind.
    //   • engine SOUND       → engine_sound(), one line per kind.
    // Add a whole new kind = enum (EK_*) + a row here + a delivery() case + an engine_sound()
    // case (+ optionally a preset rig in DESIGNS[] / DES_ENGINE[]). GAS uses ENGINE_POWER as the
    // baseline anchor (it carries the force-budget calibration); the rest are absolute numbers.
    // ╚════════════════════════════════════════════════════════════════════════════════════════╝
    //                                power        mass  vref     deftrans   colour            name        feel
    ENG[EK_ELECTRIC] = (EngineSpec){ 540.0f,       4.0f,  V_REF,   TR_SINGLE, CLR_TRUE_BLUE,    "ELECTRIC", "flat \x07 snappy" };
    ENG[EK_GAS]      = (EngineSpec){ ENGINE_POWER, 4.0f,  V_REF,   TR_AUTO,   CLR_RED,          "GAS",      "revvy mid-band" };
    ENG[EK_DIESEL]   = (EngineSpec){ 660.0f,       6.5f,  V_REF,   TR_AUTO,   CLR_ORANGE,       "DIESEL",   "low-end grunt" };
    ENG[EK_STEAM]    = (EngineSpec){ 520.0f,       9.0f,  V_REF,   TR_AUTO,   CLR_BROWN,        "STEAM",    "spool-up" };
    ENG[EK_NUCLEAR]  = (EngineSpec){ 820.0f,       14.0f, V_REF,   TR_AUTO,   CLR_LIME_GREEN,   "NUCLEAR",  "huge \x07 flat" };
    // the EXTREMES — gearing (vref) is what unlocks the range: RACE geared TALL (top-gear
    // redline ~417 px/s = 300 km/h) with the power to reach it; TRACTOR geared ULTRA-SHORT
    // (redline ~62 px/s = 45 km/h) with bottomless grunt. Tune verified headless below.
    ENG[EK_RACE]     = (EngineSpec){ 950.0f,       4.0f,  300.0f,  TR_AUTO,   CLR_PINK,         "RACE V12", "scream \x07 ~300" };
    ENG[EK_TRACTOR]  = (EngineSpec){ 1100.0f,      16.0f, 45.0f,   TR_AUTO,   CLR_DARK_GREEN,   "TRACTOR",  "grunt \x07 ~45" };

    // world obstacle materials (§9) — mass + strength per cell. A cone is light and weak,
    // so J = M_rig·speed dwarfs its strength → it's always run over. (Inc.2 adds brick/stone
    // with strengths a rig can only beat at speed → the emergent run-over/crash boundary.)
    //                          mass  strength  colour          name
    OM[OM_NONE]  = (ObMat){ 0,    0,        0,              "." };
    OM[OM_CONE]  = (ObMat){ 0.4f, 30.0f,    CLR_ORANGE,     "cone" };
    // brick: a 3×3 house sums to strength ~4050 — a buggy (M·v ≲ 2800) bounces, a heavy/fast rig
    // (semi, cargo-laden tank) pushes the contact impulse over it and SMASHES through (§9c boundary).
    OM[OM_BRICK] = (ObMat){ 6.0f, 450.0f,   CLR_LIGHT_GREY, "brick" };
    // car cell (a parked vehicle's 2×4 = 8-cell body): mass ~16 total → comparable to a mid rig, so a
    // balanced buggy BOUNCES off it while a semi BULLDOZES it (mass ratio falls out of the 2-body impulse).
    // strength ~1800 total = the WRECK threshold vs the contact impulse jimp, set HIGH on purpose: at
    // normal speeds a hit just KNOCKS the car clean across the road (billiard-ball scatter, no crumple).
    // Only an EXTREME impact (supercar near top speed, a heavy semi) clears it and wrecks — the demolition
    // seam stays, just out of reach of everyday driving for now. (Measured: a 104 km/h head-on lands the
    // first-frame jimp ~1200, under 1800; a tuned-up supercar gets there.)
    OM[OM_CAR]   = (ObMat){ 2.0f, 225.0f,   CLR_RED,        "car" };

    load_design(0);                       // start on the balanced buggy
    world_reset();                        // §9: clean world; first DRIVE frame streams chunks in
    cam_x = sx - SCREEN_W / 2.0f;
    cam_y = sy - SCREEN_H / 2.0f;
    smooth_zoom(true); //TT
}

// ── spec() — sloop's first: pins the rung-F0 car↔foot seam (docs/design/spec-harness.md).
//    Deterministic + headless; run `node tools/spec.js sloop`. The seam's contract: F at a stop
//    steps out beside the SEAT cell into a clear spot, the rig holds still while you're out,
//    re-entry works only within reach of that same cell, and walking actually moves you. ──
#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(2);                                                        // init (buggy, at rest) + settle
    expect(mode == MODE_DRIVE, "starts behind the wheel");
    float slx, sly, swx, swy;
    expect(seat_local(&slx, &sly), "the default buggy has a seat cell");
    float px = sx, py = sy, pa = ang;                               // the rig's pose, before

    spec_tap('F');                                                  // ── get out ──
    expect(mode == MODE_FOOT, "F at a stop steps out of the rig");
    expect(!foot_blocked(foot_x, foot_y), "the avatar spawns in a clear spot");
    rot(slx, sly, &swx, &swy);
    float dx = foot_x - swx, dy = foot_y - swy;
    expect(dx * dx + dy * dy < 6 * CELL * 6 * CELL, "...and spawns BESIDE the seat, not somewhere random");

    float wx0 = foot_x;                                             // ── walk ──
    key_down(KEY_RIGHT); step(60); key_up(KEY_RIGHT); step(1);
    expect(foot_x > wx0 + 5.0f, "holding right walks the avatar east");
    expect(spec_near(sx, px) && spec_near(sy, py) && spec_near(ang, pa),
           "the rig holds still while you're out");

    foot_x = swx + 60; foot_y = swy;                                // ── re-entry is gated on reach ──
    spec_tap('F');
    expect(mode == MODE_FOOT, "F far from the seat does NOT re-enter");
    foot_x = swx + FOOT_REACH * 0.5f; foot_y = swy;
    spec_tap('F');
    expect(mode == MODE_DRIVE, "F within reach of the seat gets back in");
    expect(spec_near(sx, px) && spec_near(sy, py) && spec_near(ang, pa),
           "the round trip left the rig exactly where it was parked");

    // ── a moving rig refuses the door ── (force a rolling rig directly; the gate reads velocity)
    vx = FOOT_EXIT_SPD * 3; vy = 0;
    spec_tap('F');
    expect(mode == MODE_DRIVE, "F while rolling does NOT step out (stop first)");
    vx = 0; vy = 0;

    // ── rung F1: the SURFACE drives the walk — road_at(), the same seam as the rig's grip ──
    spec_tap('F');                                                  // out again
    expect(mode == MODE_FOOT, "back out on foot for the surface tests");
    rot(slx, sly, &swx, &swy);
    // find one clear 12px eastward run on tarmac and one on grass, near the rig (loaded chunks
    // = honest collision probes). "clear" = unblocked AND the surface holds for the whole run.
    float rx = 0, ry = 0, gx = 0, gy = 0; int have_r = 0, have_g = 0;
    for (float oy = -160; oy <= 160 && !(have_r && have_g); oy += 8)
        for (float ox = -160; ox <= 160 && !(have_r && have_g); ox += 8) {
            float cx = swx + ox, cy = swy + oy;
            int road_run = 1, grass_run = 1;
            for (float d = 0; d <= 12; d += 3) {
                if (foot_blocked(cx + d, cy)) { road_run = grass_run = 0; break; }
                RoadHit h = road_at(cx + d, cy);
                if (!h.on_road) road_run = 0;
                if (h.on_road || h.on_pave) grass_run = 0;
            }
            if (road_run  && !have_r) { rx = cx; ry = cy; have_r = 1; }
            if (grass_run && !have_g) { gx = cx; gy = cy; have_g = 1; }
        }
    expect(have_r && have_g, "found a tarmac run and a grass run near the rig");
    foot_x = rx; foot_y = ry;
    key_down(KEY_RIGHT); step(15); key_up(KEY_RIGHT); step(1);
    float droad = foot_x - rx;
    foot_x = gx; foot_y = gy;
    key_down(KEY_RIGHT); step(15); key_up(KEY_RIGHT); step(1);
    float dgrass = foot_x - gx;
    expect(droad > dgrass * 1.2f, "tarmac walks faster than grass (the surface drives the feet)");
    expect(dgrass > 2.0f, "grass still walks, just slower");
    { RoadHit h = road_at(rx, ry);
      expect(h.on_pave, "a carriageway point is also on_pave (the strip contains the road)"); }

    // ── rung F2: interiors — the door is a hole, the roof lifts, the plan is real ──
    // find a deterministic house beyond the spawn parking lot from the chunk BASELINE (no load needed)
    static Obstacle tmp[OB_PERCHUNK];
    int hcx = 0, hcy = 0, hidx = -1; float hxw = 0, hyw = 0;
    for (int ccx = PARKING_CH + 1; ccx < PARKING_CH + 12 && hidx < 0; ccx++)
        for (int ccy = -4; ccy <= 4 && hidx < 0; ccy++) {
            int n = gen_chunk(ccx, ccy, tmp);
            for (int t2 = 0; t2 < n; t2++)
                if (tmp[t2].kind == OB_HOUSE && tmp[t2].hw >= IN_MIN_HW && tmp[t2].hh >= IN_MIN_HW) {
                    hcx = ccx; hcy = ccy; hidx = tmp[t2].idx; hxw = tmp[t2].x; hyw = tmp[t2].y; break;
                }
        }
    expect(hidx >= 0, "the world has a house beyond the parking lot");
    foot_x = hxw + 60; foot_y = hyw;                  // stand near it; the camera walks over, chunks stream
    step(160);
    Obstacle *ho = 0;
    for (int i2 = 0; i2 < MAXOB; i2++)
        if (pool[i2].alive && pool[i2].kind == OB_HOUSE &&
            pool[i2].cx == hcx && pool[i2].cy == hcy && pool[i2].idx == hidx) { ho = &pool[i2]; break; }
    expect(ho != 0, "the house's chunk streamed in around the avatar");
    if (ho) {
        static Interior ia, ib;                       // determinism: the same identity → the same plan
        interior_gen(&ia, ho); interior_gen(&ib, ho);
        expect(ia.nw == ib.nw && ia.nr == ib.nr &&
               spec_near(ia.w[0][0], ib.w[0][0]) && spec_near(ia.w[ia.nw - 1][3], ib.w[ib.nw - 1][3]),
               "the same house always generates the same plan");
        expect(ia.nr >= 2, "a house-sized footprint splits into rooms");

        // every leaf room is reachable from the front door — interior_gen re-rolls until its own
        // connectivity oracle passes, so this pins the guarantee, with the same fn as the arbiter
        expect(interior_connected(&ia, ho->hw, ho->hh), "every room is reachable from the front door");

        // walk in the door: outside → roof on; through the gap → THIS roof lifts
        int ds = house_door_side(ho);
        float nx3 = (ds == 0) - (ds == 1), ny3 = (ds == 2) - (ds == 3);
        foot_x = ho->x + ia.dx_ + nx3 * 4.0f; foot_y = ho->y + ia.dy_ + ny3 * 4.0f;
        step(1);
        expect(foot_house < 0, "outside the door the roof is on");
        int kin = ds == 0 ? KEY_LEFT : ds == 1 ? KEY_RIGHT : ds == 2 ? KEY_UP : KEY_DOWN;
        key_down(kin); step(60); key_up(kin); step(1);
        expect(foot_house >= 0 && &pool[foot_house] == ho, "walking through the door lifts THIS roof");

        key_down(kin); step(120); key_up(kin); step(1);   // keep pressing at the far wall
        { float dxx = foot_x - ho->x, dyy = foot_y - ho->y;   // (grid houses: ang=0, local = delta)
          expect(af(dxx) < ho->hw + 0.5f && af(dyy) < ho->hh + 0.5f,
                 "the far wall holds — no walking out through brick"); }

        // walk back out the door: the roof returns
        foot_x = ho->x + ia.dx_ - nx3 * 4.0f; foot_y = ho->y + ia.dy_ - ny3 * 4.0f;
        int kout = ds == 0 ? KEY_RIGHT : ds == 1 ? KEY_LEFT : ds == 2 ? KEY_DOWN : KEY_UP;
        key_down(kout); step(60); key_up(kout); step(1);
        expect(foot_house < 0, "walk out the door and the roof returns");
    }
}
#endif
