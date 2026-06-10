#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// SLOOP  —  build a vehicle from parts, drive it across a procedural world.
//           ── MVP rung 1: drive a fixed rig on one biome (road). ──
//
//   ◄ / ►        steer
//   Z / ▲        gas
//   X / ▼        brake / reverse
//   SPACE (hold) handbrake — break the tires loose and DRIFT
//   R            reset
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
#define GROUND_GRIP   1.0f        // road: plenty (sand/mud come in rung 3)
#define KMH           0.72f       // px/s → km/h for the readout (top ~166 px/s ≈ 120 km/h,
                                  // so the SPEED number lines up with the zone limit signs)
#define DRIFT_GRIP_MULT 0.13f     // handbrake: REAR axle's grip limit drops to this fraction → tail out
#define SKID_SLIP     28.0f       // lateral speed (px/s) where tires start marking (sideways scrub)
#define BRAKE_SKID_SPD 70.0f      // forward speed (px/s) above which STANDING ON THE BRAKE locks the
                                  // tyres → straight skid marks + a screech (only on a hard, fast stop)
// ── friction circle (per-axle let-go — "uit de bocht vliegen") ────────────────
// A tyre converts lateral slip into grip only up to SLIP_MAX of slip velocity; beyond that
// the force saturates — it has LET GO and slides. Front lets go → understeer (push wide);
// rear lets go → oversteer (tail steps out / spin). The rear's limit also shrinks with the
// drive force it's laying down (POWER_EAT), so flooring a rear-drive rig mid-corner breaks
// the back loose — power-on oversteer, emergent from the same circle.
#define SLIP_MAX      36.0f       // lateral slip velocity (px/s) a tyre holds before it lets go
#define POWER_EAT     0.72f       // fraction of the rear's grip budget eaten at full power (rear-drive).
                                  // higher → throttle keeps the rear loose → a power-drift SUSTAINS
                                  // (balanced against SELF_ALIGN_K, which pulls it straight)
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
// strong in a big slide → the rear breaks loose, this catches it, and THROTTLE (via
// POWER_EAT) trims the held angle: the controllable, sustainable, realistic drift.
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

// ── rigid body state (sx,sy = the COM in world space; rotation pivots about it) ─
static float sx, sy;              // world position of the COM
static float vx, vy;              // world velocity
static float ang, angVel;         // heading (deg, 0 = facing +x / east) + spin

static float cam_x, cam_y;
static float lead_x, lead_y;      // low-passed camera lead (smooth, no curb jitter)
static float cam_zoom = 1.0f;     // eased speed-zoom: pulls back at speed (sense of speed + see-ahead)
// ── sense-of-speed camera (eased so it never jitters) ─────────────────────────
#define CAM_ZOOM_PULL 0.16f       // how far the camera pulls BACK (zooms out) at full speed
#define CAM_ZOOM_REF  260.0f      // speed (px/s) at which the pull-back maxes out
#define CAM_LEAD      0.34f       // how far ahead the camera leads the rig (world px per px/s of vel)
static float t_skid_snd, t_scrape_snd;
static int   is_paused;
static float wheel_ang;           // eased steering-wheel angle (deg) for the cockpit dial
static float steer_pos;           // ramped steer angle (-1..+1) — analog feel from digital keys

// ── modes: BUILD (paused grid editor) ↔ DRIVE (the rig loose in the world) ───
enum { MODE_DRIVE, MODE_BUILD };
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
    steer_pos = 0;                              // wheel centred

    engine_on = 1; stalled = 0; restart_grace = 0;   // fresh rig starts cranked
    boiler = 0;                                  // steam starts cold → you feel it spool up
    wheel_ang = 0;
    for (int i = 0; i < MAXSKID; i++) skid[i].life = 0;
    for (int i = 0; i < MAXSPARK; i++) spark[i].life = 0;
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
#define DIAL_CX  128                  // round RPM tach
#define DIAL_CY  174
#define DIAL_R   21
#define BTN_X    154                  // mode buttons: ignition / trans / build
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
    if (keyp('I') || ctl_hit(BTN_X, IGN_Y, BTN_W, BTN_H)) do_ignition();   // IGN button
    if (keyp('G') || ctl_hit(BTN_X, TRN_Y, BTN_W, BTN_H)) do_trans();      // TRANS button
    if (ctl_hit(BTN_X, BLD_Y, BTN_W, BTN_H)) mode = MODE_BUILD;            // BUILD button

    int steer_l = key(KEY_LEFT)  || btn(0, BTN_LEFT)  || ctl_held(WHL_X, WHL_Y, WHL_W / 2, WHL_H);
    int steer_r = key(KEY_RIGHT) || btn(0, BTN_RIGHT) || ctl_held(WHL_X + WHL_W / 2, WHL_Y, WHL_W / 2, WHL_H);
    in_gas   = key('Z') || key(KEY_UP)   || btn(0, BTN_A) || btn(0, BTN_UP)   || ctl_held(PED_X, GAS_Y, PED_W, PED_H);
    in_brk   = key('X') || key(KEY_DOWN) || btn(0, BTN_B) || btn(0, BTN_DOWN) || ctl_held(PED_X, BRK_Y, PED_W, PED_H);
    in_hand  = key(KEY_SPACE) || ctl_held(PED_X, DRF_Y, PED_W, PED_H);
    in_steer = steer_r - steer_l;
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

// ── physics: the honest core ──────────────────────────────────────────────────
static void update_drive(float dt_) {
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
            if (gear_req == 1)       { if (gear < 1) vf = 0; gear = 1; shift_snd = 1; }   // D
            else if (gear_req == 0)  { gear = 0; shift_snd = 1; }                          // N
            else if (gear_req == -1 && af(vf) < REV_ENGAGE_SPD) { gear = -1; vf = 0; shift_snd = 1; }
        }
        gear_req = -99;
    }
    if (in_up) {                                     // E / keyboard up
        if (trans_mode == TR_MANUAL) {
            if (gear < NGEAR) { if (gear < 0) vf = 0; gear++; shift_snd = 1; }   // R→N→1→…→5
        } else if (gear < 1) {                       // AUTO/1: R → N → D
            gear++; if (gear == 1) vf = 0; shift_snd = 1;
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
    float throttle = (in_gas && engine_on && gear != 0) ? 1.0f : 0.0f;  // neutral / dead engine = no drive
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
    float tract = driveRoll * GROUND_GRIP * GRIP_TO_FORCE;
    if (af(thrust) > tract && tract > 0) {
        float s = tract / af(thrust);
        thrust *= s; eng_torque *= s;
    }

    // --- linear: net force / mass. Drag is a FORCE (base + per-wheel rolling
    //     resistance + frontal-profile aero), so top speed = thrust/drag is
    //     mass-INDEPENDENT — mass only governs how fast you reach it. -----------
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells + scrape_drag;
    vf += ((thrust - drag * vf) / M) * dt_;
    // braking: PURE deceleration, both directions (reverse is a gear now, not the brake).
    // Strong, but capped by what the tyres can grip — a well-wheeled rig stops hard.
    if (in_brk && af(vf) > 0.5f) {
        float brake = clamp(GRIP_TO_FORCE * wheelGrip * GROUND_GRIP / M, 0, BRAKE);
        float d = brake * dt_;
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
            if (wheelPD[i] && throttle > 0) cap *= (1.0f - POWER_EAT * throttle);  // a driven wheel eats its grip
            if (wheelPX[i] < 0) cap *= (1.0f - DRIFT_RECOVER * drift_loose);       // hysteresis: rear hangs loose on exit
            float cl = clamp(vlat, -cap, cap);
            float acc = cl * (g * GROUND_GRIP / M) * LAT_GRIP;  // peak force ∝ cap ∝ load
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
        float grip = clamp((wheelGrip * GROUND_GRIP / M) * LAT_GRIP * lat_mult, 0, 1.0f / dt_);
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
    if (!in_gas && !in_brk && engine_on && gear != 0) {   // NEUTRAL freewheels — no creep, just coast
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

    // --- skid marks: lay at every wheel while the tires scrub sideways OR lock under a
    //     hard brake at speed (a straight stopping skid, laid frame-by-frame as you slide) -
    int hard_brake = in_brk && af(vf) > BRAKE_SKID_SPD;
    if (af(vl) > SKID_SLIP || hard_brake)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++)
                if (grid[r][c] == P_WHEEL || grid[r][c] == P_DRIVE) {
                    float wx, wy;
                    rot((c + 0.5f) * CELL - comX, (r + 0.5f) * CELL - comY, &wx, &wy);
                    lay_skid(wx, wy);
                }
    for (int i = 0; i < MAXSKID; i++) if (skid[i].life > 0) skid[i].life--;

    // --- steering: torque about the COM, scaled by how fast you're going -------
    // ramp the binary key (-1/0/+1) into an analog steer angle: wind toward the held
    // direction at STEER_RATE, ease back to centre at STEER_RETURN when released. A quick
    // opposite tap backs the lock off a notch — the fine counter-steer a drift needs.
    float starg = (float)in_steer;
    float srate = (in_steer != 0 ? STEER_RATE : STEER_RETURN) * dt_;
    if (steer_pos < starg) { steer_pos += srate; if (steer_pos > starg) steer_pos = starg; }
    else if (steer_pos > starg) { steer_pos -= srate; if (steer_pos < starg) steer_pos = starg; }
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

    // steering-wheel visual: mirror the ramped steer position (±~26° lock)
    wheel_ang = lerp(wheel_ang, steer_pos * 26.0f, 0.3f);

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
    int   vol   = in_gas ? 5 : 3;                              // idle/creep quieter than under power
    int   cut   = cbase + (int)((r * (bright ? 0.9f : 0.75f) + (in_gas ? 0.25f : 0.0f)) * 1500);
    if (eng_voice < 0) { eng_voice = note_on((int)pitch, INSTR_ENGINE, vol); note_glide(eng_voice, 70); }
    note_pitch (eng_voice, pitch);                             // glided → smooth rev tracking
    note_vol   (eng_voice, vol);
    note_cutoff(eng_voice, cut);
}

void update(void) {
    static int snd_ready = 0;
    if (!snd_ready) { engine_sound_init(); snd_ready = 1; }
    handle_input();
    engine_sound(mode == MODE_DRIVE && !is_paused && engine_on);   // every frame (also kills it in BUILD/pause)
    if (mode == MODE_BUILD || is_paused) return;   // BUILD pauses the simulation
    float dt_ = dt(); if (dt_ > 0.05f) dt_ = 0.05f;
    update_drive(dt_);

    // camera LEADS the rig in the travel direction (you see where you're rushing into —
    // reads as speed). The lead is HEAVILY low-passed (lead_x/y ease toward vx/vy) so it
    // doesn't jitter the bright curbs frame-to-frame or snap through the city's 90° corners.
    lead_x = lerp(lead_x, vx * CAM_LEAD, 0.04f);
    lead_y = lerp(lead_y, vy * CAM_LEAD, 0.04f);
    cam_x = lerp(cam_x, sx + lead_x - SCREEN_W / 2.0f, 0.15f);
    cam_y = lerp(cam_y, sy + lead_y - SCREEN_H / 2.0f, 0.15f);
    // speed-zoom: pull the camera back as you go faster — more world streams through the frame
    // (and you see further ahead). Eased slowly so it never jitters; resets to 1 in BUILD/at rest.
    float camspd = fsqrt(vx * vx + vy * vy);
    float zoomTarget = 1.0f - CAM_ZOOM_PULL * clamp(camspd / CAM_ZOOM_REF, 0, 1);
    cam_zoom = lerp(cam_zoom, zoomTarget, 0.05f);

#ifdef DE_TRACE
    float fwx = cos_deg(ang), fwy = sin_deg(ang);
    watch("vf", "%.1f", vx * fwx + vy * fwy);
    watch("vl", "%.1f", vx * (-fwy) + vy * fwx);
    watch("ang", "%.0f", ang);
    watch("angvel", "%.0f", angVel);
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
#endif
}

// ── world: a single biome (road) that scrolls under you ──────────────────────
static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

static void draw_ground(void) {
    int step = 32;
    // when the speed-zoom pulls back (cam_zoom<1) the view shows more world than SCREEN_W/H —
    // widen the draw range by that margin so the zoomed-out edges aren't left undrawn.
    int mx = (int)(SCREEN_W * (1.0f / cam_zoom - 1.0f) * 0.5f) + step;
    int my = (int)(SCREEN_H * (1.0f / cam_zoom - 1.0f) * 0.5f) + step;
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
static int zone_at(float x, float y) {
    float d = fsqrt(x * x + y * y);
    for (int z = 0; z < Z_N; z++) if (d < ZONE_R[z]) return z;
    return Z_SUPER;
}

// fill a city/town block with houses of a FIXED real size — ~5 m facade. At CELL≈1 m the car is
// ~4 m, so a house now reads BIGGER than the car (a believable building), not the old car-sized
// box. Tiled into the block interior and centred; the remainder reads as yards/gaps. Bigger blocks
// (town) just fit more of the same-size houses → consistent scale across zones.
#define HOUSE_FACADE 38           // px per house plot (~5 m); the drawn house is ~34px ≈ 4.9 m
static void draw_houses(int bx, int by, int p, int hw) {
    int x0 = bx * p + hw + 3, x1 = (bx + 1) * p - hw - 3;
    int y0 = by * p + hw + 3, y1 = (by + 1) * p - hw - 3;
    int nx = (x1 - x0) / HOUSE_FACADE, ny = (y1 - y0) / HOUSE_FACADE;
    if (nx < 1 || ny < 1) return;                       // block too small for even one 5 m house
    int ox = x0 + ((x1 - x0) - nx * HOUSE_FACADE) / 2;  // centre the fixed-size houses in the block
    int oy = y0 + ((y1 - y0) - ny * HOUSE_FACADE) / 2;
    int roof[4] = { CLR_BROWN, CLR_RED, CLR_DARK_PURPLE, CLR_DARK_GREY };
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++) {
            unsigned h = hash2(bx * 131 + i, by * 131 + j);
            if ((h & 7) == 0) continue;                 // some empty lots / yards
            int hx = ox + i * HOUSE_FACADE, hy = oy + j * HOUSE_FACADE;
            rectfill(hx + 1, hy + 1, HOUSE_FACADE - 3, HOUSE_FACADE - 3, roof[h & 3]);
            rect(hx + 1, hy + 1, HOUSE_FACADE - 3, HOUSE_FACADE - 3, CLR_BROWNISH_BLACK);
        }
}

static void draw_course(void) {
    // widen the drawn area to cover the speed-zoom pull-back (see draw_ground)
    int mx = (int)(SCREEN_W * (1.0f / cam_zoom - 1.0f) * 0.5f) + 4;
    int my = (int)(SCREEN_H * (1.0f / cam_zoom - 1.0f) * 0.5f) + 4;
    int L = (int)cam_x - mx, R = (int)cam_x + SCREEN_W + mx, T = (int)cam_y - my, B = (int)cam_y + SCREEN_H + my;
    cur_zone = zone_at(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f);
    int p = ZONE_PITCH[cur_zone], hw = ZONE_LANE[cur_zone] / 2;
    int kx0 = ifloordiv(L, p) - 1, kx1 = ifloordiv(R, p) + 1;
    int ky0 = ifloordiv(T, p) - 1, ky1 = ifloordiv(B, p) + 1;

    // 0. fields between rural+ roads (green), or houses in the city/town blocks
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            if (cur_zone == Z_CITY || cur_zone == Z_TOWN) draw_houses(kx, ky, p, hw);
            else if (cur_zone >= Z_RURAL && (hash2(kx, ky) & 1)) {     // patchwork fields
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

    // 2. curbs + dashed centre line; SUPER adds a 2nd dashed line (multi-lane)
    for (int kx = kx0; kx <= kx1; kx++) {
        int cx = kx * p;
        line(cx - hw, T, cx - hw, B, CLR_LIGHT_GREY);
        line(cx + hw, T, cx + hw, B, CLR_LIGHT_GREY);
        if (cur_zone != Z_CITY)               // city streets are one-way → no centre line
            for (int y = ifloordiv(T, 24) * 24; y < B; y += 24) line(cx, y, cx, y + 11, CLR_YELLOW);
        if (cur_zone == Z_SUPER)
            for (int y = ifloordiv(T, 20) * 20; y < B; y += 20) {
                line(cx - hw / 2, y, cx - hw / 2, y + 9, CLR_MEDIUM_GREY);
                line(cx + hw / 2, y, cx + hw / 2, y + 9, CLR_MEDIUM_GREY);
            }
    }
    for (int ky = ky0; ky <= ky1; ky++) {
        int cy = ky * p;
        line(L, cy - hw, R, cy - hw, CLR_LIGHT_GREY);
        line(L, cy + hw, R, cy + hw, CLR_LIGHT_GREY);
        if (cur_zone != Z_CITY)
            for (int x = ifloordiv(L, 24) * 24; x < R; x += 24) line(x, cy, x + 11, cy, CLR_YELLOW);
        if (cur_zone == Z_SUPER)
            for (int x = ifloordiv(L, 20) * 20; x < R; x += 20) {
                line(x, cy - hw / 2, x + 9, cy - hw / 2, CLR_MEDIUM_GREY);
                line(x, cy + hw / 2, x + 9, cy + hw / 2, CLR_MEDIUM_GREY);
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
                    for (int s = -hw + 2; s < hw; s += 4) line(cx + s, cy - hw, cx + s, cy + hw, CLR_WHITE);
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
    float h = CELL * 0.5f;
    float ax, ay, bx, by, cx2, cy2, dx, dy;
    rot(lx - h, ly - h, &ax, &ay); rot(lx + h, ly - h, &bx, &by);
    rot(lx + h, ly + h, &cx2, &cy2); rot(lx - h, ly + h, &dx, &dy);
    int xy[8] = { (int)ax, (int)ay, (int)bx, (int)by, (int)cx2, (int)cy2, (int)dx, (int)dy };
    int col = dragging[r][c] ? hot_col() : part_col(p);   // scraping cells glow
    polyfill(xy, 4, col);
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
    print_centered(ZONE_NAME[cur_zone], SCREEN_W / 2, 4, CLR_YELLOW);
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

    // STEERING WHEEL — left half steers left, right half right (arrows light up)
    int hl = ctl_held(WHL_X, WHL_Y, WHL_W / 2, WHL_H);
    int hr = ctl_held(WHL_X + WHL_W / 2, WHL_Y, WHL_W / 2, WHL_H);
    circ(WHEEL_CX, WHEEL_CY, WHEEL_R, CLR_LIGHT_GREY);
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
    dash_btn(BTN_X, IGN_Y, BTN_W, BTN_H, engine_on ? "IGN ON" : "IGN OFF", "I", engine_on, CLR_GREEN);
    dash_btn(BTN_X, TRN_Y, BTN_W, BTN_H, trans_label(), "G", 0, CLR_DARKER_GREY);
    dash_btn(BTN_X, BLD_Y, BTN_W, BTN_H, "BUILD", "TAB", 0, CLR_DARKER_GREY);

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
    camera_ex((int)cam_x, (int)cam_y, cam_zoom, 0);   // speed-zoom (pulls back at speed)
    draw_ground();
    draw_course();
    for (int i = 0; i < MAXSKID; i++)     // tire marks burned into the road
        if (skid[i].life > 0)
            pset((int)skid[i].x, (int)skid[i].y,
                 skid[i].life > 90 ? CLR_BLACK : CLR_BROWNISH_BLACK);
    draw_vehicle();
    for (int i = 0; i < MAXSPARK; i++)    // sparks thrown off the grinding belly (over the rig)
        if (spark[i].life > 0) {
            int col = spark[i].life > 12 ? CLR_WHITE : spark[i].col;   // hot core fades to its tint
            pset((int)spark[i].x, (int)spark[i].y, col);
        }
    camera(0, 0);
    hud();
}

void init(void) {
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

    load_design(0);                       // start on the balanced buggy
    cam_x = sx - SCREEN_W / 2.0f;
    cam_y = sy - SCREEN_H / 2.0f;
}
