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
// and drops on each upshift. Per-engine-kind curves (diesel/steam/…) are still to come.
// ============================================================================

// ── part vocabulary ──────────────────────────────────────────────────────────
// Addressed by enum, never raw index (CLAUDE.md data-driven rule). Each kind:
// mass, engine power, wheel grip, colour.
enum { P_NONE, P_FRAME, P_ENGINE, P_WHEEL, P_CASTER, P_SEAT, P_DRIVE, P_KINDS };
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

// ── the rig: a grid of parts ─────────────────────────────────────────────────
// Rung 1 had one hardcoded layout; here we toggle between a few PRESET rigs (1-4)
// to FEEL how the derived physics changes with the build — orbit's playbook (its
// 1/2/3 rockets) before the parts-bin builder (rung 2). Same drive core, no tuning
// per rig: every difference below is purely the mass / COM / I / grip falling out
// of where the parts sit.
#define GW   6                     // max grid footprint (rigs pad unused cells with P_NONE)
#define GH   3
#define CELL 7.0f                  // world px per cell
static int grid[GH][GW];

#define NDES 8
static const int DESIGNS[NDES][GH][GW] = {
    { // 0 BUGGY — balanced 4-wheeler, engine centred: drives clean
        { P_WHEEL, P_FRAME, P_FRAME,  P_WHEEL, P_NONE,  P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE,  P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_WHEEL, P_NONE,  P_NONE },
    },
    { // 1 HAULER — long & heavy, one engine: crawls, turns lazily (big mass + I)
        { P_WHEEL, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_WHEEL },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_FRAME, P_FRAME },
        { P_WHEEL, P_FRAME, P_FRAME,  P_FRAME, P_FRAME, P_WHEEL },
    },
    { // 2 SPRINTER — light, TWIN engine centred: huge accel, snappy
        { P_WHEEL,  P_FRAME,  P_FRAME,  P_WHEEL, P_NONE, P_NONE },
        { P_SEAT,   P_ENGINE, P_ENGINE, P_FRAME, P_NONE, P_NONE },
        { P_WHEEL,  P_FRAME,  P_FRAME,  P_WHEEL, P_NONE, P_NONE },
    },
    { // 3 JALOPY — 3 wheels + off-centre engine: loose, pulls, slides
        { P_WHEEL, P_FRAME, P_ENGINE, P_NONE,  P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_FRAME,  P_NONE,  P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_WHEEL,  P_NONE,  P_NONE, P_NONE },
    },
    { // 4 MOTORBIKE — narrow inline 2-wheeler: feather-light, darty, twitchy
        { P_NONE,  P_NONE,   P_NONE, P_NONE,  P_NONE, P_NONE },
        { P_WHEEL, P_ENGINE, P_SEAT, P_WHEEL, P_NONE, P_NONE },
        { P_NONE,  P_NONE,   P_NONE, P_NONE,  P_NONE, P_NONE },
    },
    { // 5 FWD — drive wheels at the FRONT (c3): front pulls → planted, understeers
        { P_WHEEL, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE, P_NONE },
        { P_WHEEL, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE },
    },
    { // 6 RWD — drive wheels at the REAR (c0), rear engine: rear pushes → tail-happy
        { P_DRIVE, P_FRAME,  P_FRAME, P_WHEEL, P_NONE, P_NONE },
        { P_FRAME, P_ENGINE, P_SEAT,  P_FRAME, P_NONE, P_NONE },
        { P_DRIVE, P_FRAME,  P_FRAME, P_WHEEL, P_NONE, P_NONE },
    },
    { // 7 4WD — drive wheels at all four corners: power everywhere → grippy, neutral
        { P_DRIVE, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE },
        { P_FRAME, P_SEAT,  P_ENGINE, P_FRAME, P_NONE, P_NONE },
        { P_DRIVE, P_FRAME, P_FRAME,  P_DRIVE, P_NONE, P_NONE },
    },
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

// ── tuning ───────────────────────────────────────────────────────────────────
#define ENGINE_POWER  1180.0f     // forward force per engine. LOW (vs drag, also low) so accel
                                  // is gentle and the climb to top speed takes several seconds —
                                  // that's what lets you DWELL in each gear (off in 1st, build
                                  // 2nd, cruise 3rd/4th) instead of blasting to top in a blink.
// Drag is a FORCE (DDA's model): top speed = thrust / drag, MASS-INDEPENDENT — mass
// sets acceleration, not top speed. Drag = base + per-wheel rolling resistance + a
// frontal-profile aero term, so SHAPE and WHEEL COUNT set top speed, not just weight.
// Drag set LOW so top speed is high (~2x): the launch stays punchy (accel = thrust/M,
// drag negligible at low speed) but the rig keeps PULLING to a far higher top — which
// both stretches the gears into a real progression (off in 1st, build 2nd, cruise 3/4)
// and makes the world rip by (sense of speed). The speed-dependent handling below
// (V_REF, STAB_H, SKID_SLIP…) is scaled to match.
#define DRAG_BASE     0.6f        // baseline drag (force per px/s)
#define DRAG_WHEEL    0.25f       // lever: each wheel adds rolling resistance (grip↑, top speed↓)
#define DRAG_AERO     0.6f        // lever: drag per cell of frontal profile (narrow = fast)
#define BRAKE         560.0f      // max braking decel (px/s^2) — real brakes >> engine accel;
                                  // capped per-rig by tyre grip below (GRIP_TO_FORCE·grip/M),
                                  // so MORE/BETTER WHEELS = harder stops (an under-wheeled rig
                                  // can't haul up as fast). At slow speed this stops you dead.
#define ROLL_FRIC     16.0f       // CONSTANT rolling/bearing friction (px/s^2) — what actually
                                  // STOPS a coasting rig. Drag ∝ v only asymptotes to 0 (floaty);
                                  // this constant term dominates at low speed and snaps v to rest.
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
#define V_REF         135.0f      // speed (px/s) where a ratio-1.0 gear hits redline (absorbs
                                  // the real final-drive ~3.6 + wheel size + px↔world units)
#define REV_RATIO     3.50f       // reverse ≈ 1st gear (real gearboxes share the ratio)
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
#define LAT_GRIP      32.0f       // lateral velocity killed per second (tire grip)
#define STEER_RESP    680.0f      // steering authority (deg/s^2) at speed
#define ANG_DAMP      5.0f        // angular self-centering (1/s)
#define REF_GYRO      130.0f      // gyradius^2 (px^2) a "normal" rig turns easily at
#define ENG_YAW_K     0.9f        // how hard an off-centre engine yaws the rig
#define BALANCE_K     0.4f        // lever: front-heavy understeers, rear-heavy oversteers
#define GRIP_TO_FORCE 2000.0f     // wheel grip → max traction force
#define GROUND_GRIP   1.0f        // road: plenty (sand/mud come in rung 3)
#define KMH           0.72f       // px/s → km/h for the readout (top ~166 px/s ≈ 120 km/h,
                                  // so the SPEED number lines up with the zone limit signs)
#define DRIFT_GRIP_MULT 0.13f     // handbrake: lateral grip drops to this fraction
#define SKID_SLIP     28.0f       // lateral speed (px/s) where tires start marking
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
// ── transmission state (§1b) ─────────────────────────────────────────────────
static int   trans_mode = TR_AUTO;  // SINGLE / AUTO / MANUAL — player setting, persists
static int   gear = 1;            // 0 = reverse, 1..NGEAR = forward
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
static float t_eng_snd, t_skid_snd, t_scrape_snd;
static int   is_paused;
static float wheel_ang;           // eased steering-wheel angle (deg) for the cockpit dial

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

// the powerband: torque ÷ peak-torque vs normalised RPM, fitted to a real NA-gasoline dyno
// (Honda 2.0 SI): idle ≈ 0.61, PEAK at ≈ 0.66 of redline, ≈ 0.79 still pulling at redline —
// then the rev limiter cuts hard past 1.0 (over-revving a too-low gear is the bite). The
// low-rpm side is flat-ish (idle floor 0.6) — real curves don't sag to a parabola down low.
static float powerband(float r) {
    if (r > 1.0f) return clamp(0.79f - (r - 1.0f) * 9.0f, 0.0f, 0.79f); // rev limiter
    float d = r - 0.66f;
    return clamp(1.0f - 1.82f * d * d, 0.6f, 1.0f);                     // peak 1.0 @ 0.66 redline
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
            float cx = (c + 0.5f) * CELL, cy = (r + 0.5f) * CELL;
            M += k->mass; comX += k->mass * cx; comY += k->mass * cy;
            wheelGrip += k->grip; wheelRoll += k->roll;
            if ((c + 1) * CELL > frontX) frontX = (c + 1) * CELL;
            if (r < minRow) minRow = r;
            if (r > maxRow) maxRow = r;
            if (p == P_ENGINE) nEngines++;
            if (k->roll > 0) {                 // a support point (wheel / caster / drive)
                nWheels++;
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
    // moment of inertia about the COM
    I = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            PartKind *k = &KIND[grid[r][c]];
            if (k->mass <= 0) continue;
            float dx = (c + 0.5f) * CELL - comX, dy = (r + 0.5f) * CELL - comY;
            I += k->mass * (dx * dx + dy * dy);
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
}

static void reset_vehicle(void) {
    recompute_body();
    sx = 0; sy = 0; vx = vy = 0; ang = 0; angVel = 0;
    heat = 0; tip_amt = 0; gear = 1; rpm = 0;   // trans_mode persists (player setting)
    engine_on = 1; stalled = 0; restart_grace = 0;   // fresh rig starts cranked
    wheel_ang = 0;
    for (int i = 0; i < MAXSKID; i++) skid[i].life = 0;
    for (int i = 0; i < MAXSPARK; i++) spark[i].life = 0;
}

// live readout estimates — the same formulas the drive core uses, so BUILD shows
// the truth about a rig before you ever drive it.
static float est_top_speed(void) {
    float thrust = nEngines * KIND[P_ENGINE].power;
    // traction cap: too few wheels can't lay down all the engine's power (rolling support)
    float tract = wheelRoll * GROUND_GRIP * GRIP_TO_FORCE;
    if (tract > 0 && thrust > tract) thrust = tract;
    // include scrape drag so a cantilevered build shows its real (lower) top speed
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells + SCRAPE_DRAG * nDrag;
    return drag > 0 ? thrust / drag : 0;
}
static float est_turn_rate(void) {                 // steady deg/s at speed
    float turnEase = REF_GYRO / (I / M + REF_GYRO);
    return STEER_RESP * turnEase * (1.0f - BALANCE_K * balance) / ANG_DAMP;
}

static void load_design(int idx) {
    cur_des = (idx + NDES) % NDES;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) grid[r][c] = DESIGNS[cur_des][r][c];
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

// ── cockpit dashboard: touch / mouse / keyboard controls + round gauges ───────
// The layout is shared between handle_input (hit-testing) and hud (drawing), so
// the rects live here as #defines. Every control is LABELLED with its key and
// fires on a tap, a mouse press, OR the key — all three at once. Hold controls
// (steer/gas/brake/drift) use a level test; discrete ones (shift/ignition/trans/
// build) an edge test. Built on tap()/tapp()+mouse rather than ui.h capture:
// the zones don't overlap, so two fingers (hold gas + tap shift) just work.
#define DASH_Y   148                  // top of the dashboard band (… SCREEN_H)
#define WHEEL_CX 31                   // steering wheel centre + radius
#define WHEEL_CY 174
#define WHEEL_R  20
#define WHL_X    4                    // wheel hit-area: left half steers left, right half right
#define WHL_Y    150
#define WHL_W    54
#define WHL_H    48
#define PED_X    62                   // pedals (held), stacked: gas / brake / drift
#define PED_W    30
#define PED_H    16
#define GAS_Y    150
#define BRK_Y    167
#define DRF_Y    184
#define SPD_X    94                   // speed LED readout (display)
#define SPD_Y    152
#define DIAL_CX  170                  // round RPM tach (display)
#define DIAL_CY  174
#define DIAL_R   22
#define STK_X    198                  // stickshift gate: upper half = up (E), lower = down (Q)
#define STK_Y    150
#define STK_W    42
#define STK_H    48
#define BTN_X    244                  // right button column (edge): ignition / trans / build
#define BTN_W    54
#define BTN_H    15
#define IGN_Y    150
#define TRN_Y    167
#define BLD_Y    184

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

    if (mode == MODE_BUILD) {
        if (keyp('R')) clear_grid();           // R clears the grid to empty
        // part-select hotkeys mirror the palette
        if (keyp('F')) sel_part = P_FRAME;
        if (keyp('E')) sel_part = P_ENGINE;
        if (keyp('W')) sel_part = P_WHEEL;
        if (keyp('D')) sel_part = P_DRIVE;     // powered (driven) wheel
        if (keyp('C')) sel_part = P_CASTER;
        if (keyp('S')) sel_part = P_SEAT;
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
    in_up    = keyp('E') || ctl_hit(STK_X, STK_Y, STK_W, STK_H / 2);                 // upper half of the gate
    in_down  = keyp('Q') || ctl_hit(STK_X, STK_Y + STK_H / 2, STK_W, STK_H / 2);     // lower half
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

    // --- shifting: manual (Q/E) or auto; reverse is gear 0 (only when ~stopped) ---
    if (in_up) {                                     // E: out of reverse, or upshift
        if (gear == 0) { if (af(vf) < 5.0f) { gear = 1; shift_snd = 1; } }
        else if (trans_mode == TR_MANUAL && gear < NGEAR) { gear++; shift_snd = 1; }
    }
    if (in_down) {                                   // Q: downshift, or into reverse at rest
        if (trans_mode == TR_MANUAL && gear > 1) { gear--; shift_snd = 1; }
        else if (gear >= 1 && af(vf) < 5.0f) { gear = 0; shift_snd = 1; }
    }
    if (trans_mode == TR_SINGLE && gear > 1) gear = 1;          // single keeps one forward gear
    if (trans_mode == TR_AUTO && gear >= 1) {                   // auto-shift to stay in the band
        if (rpm > AUTO_UP && gear < NGEAR) { gear++; shift_snd = 1; }
        else if (rpm < AUTO_DOWN && gear > 1) { gear--; shift_snd = 1; }
    }

    // --- transmission: ratio sets RPM + multiplies torque; reverse drives backward
    float ratio = (trans_mode == TR_SINGLE) ? SINGLE_RATIO
                : (gear == 0) ? REV_RATIO : GEAR_RATIO[gear - 1];
    float gdir  = (gear == 0) ? -1.0f : 1.0f;
    rpm = clamp(af(vf) * ratio / V_REF, 0, 1.15f);
    // --- stall: lug a too-tall gear below idle revs while still rolling → it cuts out.
    // Skipped briefly after a crank (RESTART_GRACE) so re-ignition always takes and you
    // get a beat to downshift; SINGLE (electric) and reverse never stall.
    if (restart_grace > 0) restart_grace -= dt_;
    if (engine_on && restart_grace <= 0 && trans_mode != TR_SINGLE && gear >= 1
        && rpm < STALL_RPM && af(vf) > VSTALL_MIN) {
        engine_on = 0; stalled = 1;
        hit(28, INSTR_NOISE, 3, 200);                 // the cough as it dies
    }
    if (!engine_on) rpm = 0;                           // dead engine → tach drops to zero
    // SINGLE (electric): instant flat torque that just tapers toward the motor's max revs —
    // no powerband to chase, snappy off the line, moderate top (the single-speed EV trade).
    float gmul = (trans_mode == TR_SINGLE) ? SINGLE_RATIO * clamp(1.15f - 0.6f * rpm, 0.2f, 1.15f)
                                           : powerband(rpm) * ratio;

    // --- engine: thrust through the gear, + the yaw torque from an off-centre engine
    float throttle = (in_gas && engine_on) ? 1.0f : 0.0f;  // gas drives in the gear's direction (dead engine = no thrust)
    float thrust = 0, eng_torque = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != P_ENGINE) continue;
            float t = KIND[P_ENGINE].power * throttle * gmul * gdir;
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

    // --- tire grip: bleed the sideways velocity away (the car-feel line) -------
    // the handbrake breaks the tires loose — same grip term, turned down → drift.
    float lat_mult = in_hand ? DRIFT_GRIP_MULT : 1.0f;
    lat_mult *= (1.0f - STAB_GRIP_LOSS * tip_amt);   // tipping unloads the tires → they let go
    float grip = clamp((wheelGrip * GROUND_GRIP / M) * LAT_GRIP * lat_mult, 0, 1.0f / dt_);
    vl -= vl * grip * dt_;
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
    if (!in_gas && !in_brk && engine_on) {
        float vcreep = IDLE_CREEP / ratio;            // idle RPM in this gear → 1/ratio law
        if (gear == 0) {                                  // reverse idles backward
            if (vf > -vcreep) { vf -= CREEP_ACCEL * dt_; if (vf < -vcreep) vf = -vcreep; }
        } else if (vf >= 0 && vf < vcreep) {              // forward: ease up to the floor
            vf += CREEP_ACCEL * dt_; if (vf > vcreep) vf = vcreep;
        }
    }

    // recombine
    vx = fwx * vf + ltx * vl;
    vy = fwy * vf + lty * vl;
    sx += vx * dt_; sy += vy * dt_;

    // --- skid marks: lay at every wheel while the tires are scrubbing sideways -
    if (af(vl) > SKID_SLIP)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++)
                if (grid[r][c] == P_WHEEL || grid[r][c] == P_DRIVE) {
                    float wx, wy;
                    rot((c + 0.5f) * CELL - comX, (r + 0.5f) * CELL - comY, &wx, &wy);
                    lay_skid(wx, wy);
                }
    for (int i = 0; i < MAXSKID; i++) if (skid[i].life > 0) skid[i].life--;

    // --- steering: torque about the COM, scaled by how fast you're going -------
    float speed_factor = clamp(af(vf) / 50.0f, 0, 1);
    float dir = vf >= 0 ? 1.0f : -1.0f;
    float gyro = I / M;                              // radius of gyration squared
    float turnEase = REF_GYRO / (gyro + REF_GYRO);   // small/light rig → turns easier
    // weight balance: nose-heavy (balance>0) pushes wide (understeer), tail-heavy
    // (balance<0) turns in eagerly (oversteer). Uses the COM we already derive.
    float steer_bal = 1.0f - BALANCE_K * balance;
    float ang_acc = in_steer * STEER_RESP * speed_factor * dir * turnEase * steer_bal;
    ang_acc += ENG_YAW_K * (eng_torque / I);         // off-centre engine pulls
    if (scraping) ang_acc += SCRAPE_YAW * (scrape_torque / I) * dir;  // off-centre scrape drags
    angVel += ang_acc * dt_;
    angVel -= angVel * ANG_DAMP * dt_;
    // directional stability: the drive point ahead of the COM (in the travel frame)
    // PULLS the rig → extra self-centering (stable, understeer); behind it PUSHES →
    // anti-damping (the heavy end wants to swing round → oversteer / spin). Reversing
    // flips which end leads, so a rear-drive rig is calmer backwards — and a one-wheel
    // "bike" genuinely drives better in reverse (wheel leads, the bare stub trails).
    float driveOff = clamp(driveX - comX, -DRIVE_OFF_MAX, DRIVE_OFF_MAX);  // >0 = ahead of COM
    float lead = driveOff * dir;                     // travel frame: >0 pull (lead), <0 push (trail)
    angVel -= STAB_YAW_K * lead * angVel * clamp(spd0 / 45.0f, 0, 1) * dt_;
    ang += angVel * dt_;
    if (ang < 0) ang += 360; else if (ang >= 360) ang -= 360;

    // steering-wheel visual: ease toward the steer input (±~26° lock)
    wheel_ang = lerp(wheel_ang, (float)in_steer * 26.0f, 0.22f);

    // --- sound -----------------------------------------------------------------
    float spd = fsqrt(vx * vx + vy * vy);
    if (shift_snd) { hit(40, INSTR_NOISE, 2, 45); shift_snd = 0; }   // gear-change clunk
    if (in_gas && engine_on) {
        // engine note tracks RPM: climbs within a gear, DROPS on each upshift (the
        // satisfying gear-change), climbs again. (rpm resets per gear; spd would not.)
        t_eng_snd -= dt_;
        if (t_eng_snd <= 0) { hit(30 + (int)(rpm * 30.0f), INSTR_SAW, 3, 90); t_eng_snd = 0.08f; }
    }
    if (af(vl) > 35) {                               // tires scrubbing sideways
        t_skid_snd -= dt_;
        if (t_skid_snd <= 0) { hit(54, INSTR_NOISE, 2, 70); t_skid_snd = 0.05f; }
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

void update(void) {
    handle_input();
    if (mode == MODE_BUILD || is_paused) return;   // BUILD pauses the simulation
    float dt_ = dt(); if (dt_ > 0.05f) dt_ = 0.05f;
    update_drive(dt_);

    // camera LEADS the rig in the travel direction (you see where you're rushing into —
    // reads as speed). The lead is HEAVILY low-passed (lead_x/y ease toward vx/vy) so it
    // doesn't jitter the bright curbs frame-to-frame or snap through the city's 90° corners.
    lead_x = lerp(lead_x, vx * 0.26f, 0.04f);
    lead_y = lerp(lead_y, vy * 0.26f, 0.04f);
    cam_x = lerp(cam_x, sx + lead_x - SCREEN_W / 2.0f, 0.15f);
    cam_y = lerp(cam_y, sy + lead_y - SCREEN_H / 2.0f, 0.15f);

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
    int x0 = ((int)cam_x / step - 1) * step;
    int y0 = ((int)cam_y / step - 1) * step;
    int x1 = (int)cam_x + SCREEN_W + step;
    int y1 = (int)cam_y + SCREEN_H + step;
    // grid of asphalt seams — gives motion + a rotation reference
    for (int x = x0; x <= x1; x += step) line(x, y0, x, y1, CLR_DARK_GREY);
    for (int y = y0; y <= y1; y += step) line(x0, y, x1, y, CLR_DARK_GREY);
    // deterministic speckle — at speed each fleck STREAKS opposite travel (motion blur),
    // the strongest sense-of-speed cue and tied to the rig's actual velocity.
    float spd = fsqrt(vx * vx + vy * vy);
    float sl = clamp(spd * 0.085f, 0, 11.0f);        // streak length (px)
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
static const int   ZONE_PITCH[Z_N] = { 100, 200, 600, 1200, 2400 };  // block spacing (px)
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

// fill one city/town block's interior with packed houses (deterministic) — the detail
// that streaks past and screams "city". `n` = houses per row.
static void draw_houses(int bx, int by, int p, int hw, int n) {
    int x0 = bx * p + hw + 3, x1 = (bx + 1) * p - hw - 3;
    int y0 = by * p + hw + 3, y1 = (by + 1) * p - hw - 3;
    int cw = (x1 - x0) / n, ch = (y1 - y0) / n;
    if (cw < 4 || ch < 4) return;
    int roof[4] = { CLR_BROWN, CLR_RED, CLR_DARK_PURPLE, CLR_DARK_GREY };
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            unsigned h = hash2(bx * 131 + i, by * 131 + j);
            if ((h & 7) == 0) continue;                 // some empty lots / yards
            int hx = x0 + i * cw, hy = y0 + j * ch;
            rectfill(hx + 1, hy + 1, cw - 2, ch - 2, roof[h & 3]);
            rect(hx + 1, hy + 1, cw - 2, ch - 2, CLR_BROWNISH_BLACK);
        }
}

static void draw_course(void) {
    int L = (int)cam_x, R = L + SCREEN_W, T = (int)cam_y, B = T + SCREEN_H;
    cur_zone = zone_at(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f);
    int p = ZONE_PITCH[cur_zone], hw = ZONE_LANE[cur_zone] / 2;
    int kx0 = ifloordiv(L, p) - 1, kx1 = ifloordiv(R, p) + 1;
    int ky0 = ifloordiv(T, p) - 1, ky1 = ifloordiv(B, p) + 1;

    // 0. fields between rural+ roads (green), or houses in the city/town blocks
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            if (cur_zone == Z_CITY)      draw_houses(kx, ky, p, hw, 4);
            else if (cur_zone == Z_TOWN) draw_houses(kx, ky, p, hw, 2);
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

// ── vehicle: draw each occupied cell as a rotated quad ───────────────────────
static void draw_cell(int r, int c, int p) {
    float lx = (c + 0.5f) * CELL - comX, ly = (r + 0.5f) * CELL - comY;
    float h = CELL * 0.5f;
    float ax, ay, bx, by, cx2, cy2, dx, dy;
    rot(lx - h, ly - h, &ax, &ay); rot(lx + h, ly - h, &bx, &by);
    rot(lx + h, ly + h, &cx2, &cy2); rot(lx - h, ly + h, &dx, &dy);
    int xy[8] = { (int)ax, (int)ay, (int)bx, (int)by, (int)cx2, (int)cy2, (int)dx, (int)dy };
    int col = dragging[r][c] ? hot_col() : KIND[p].col;   // scraping cells glow
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
// The bottom band is a driveable dashboard: a steering wheel (press its left/right
// half), gas/brake/drift pedals, a digital speed readout, a round RPM tach with a
// needle, a stickshift H-gate (tap upper=up E / lower=down Q) showing the current
// gear, and ignition/trans/build buttons. Every control reads keyboard, touch AND
// mouse (see the helpers + handle_input). Play area + road-sign limit sit above.
static void hud(void) {
    char buf[48];
    float spd = fsqrt(vx * vx + vy * vy);

    // --- top of screen: rig identity (dim) + the zone's limit (a road sign) ----
    print(DES_NAME[cur_des], 4, 4, CLR_DARK_GREY);
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

    // STICKSHIFT GATE — tap upper half to up-shift (E), lower to down-shift (Q)
    int su = ctl_held(STK_X, STK_Y, STK_W, STK_H / 2);
    int sd = ctl_held(STK_X, STK_Y + STK_H / 2, STK_W, STK_H / 2);
    rectfill(STK_X, STK_Y, STK_W, STK_H, CLR_DARKER_GREY);
    rect(STK_X, STK_Y, STK_W, STK_H, CLR_DARK_GREY);
    int colx[3] = { STK_X + 9, STK_X + 21, STK_X + 33 };
    int gtopy = STK_Y + 14, gboty = STK_Y + STK_H - 8, gmidy = (gtopy + gboty) / 2;
    line(colx[0], gmidy, colx[2], gmidy, CLR_MEDIUM_GREY);          // gate channel
    for (int c = 0; c < 3; c++) line(colx[c], gtopy, colx[c], gboty, CLR_MEDIUM_GREY);
    int gi   = (gear == 0) ? 2 : (gear - 1) / 2;                    // column: 1/2 · 3/4 · 5/R
    int gtop = (gear != 0) && ((gear - 1) % 2 == 0);                // odd gears up, even down, R down
    circfill(colx[gi], gtop ? gtopy : gboty, 3, engine_on ? CLR_WHITE : CLR_DARK_GREY);
    // up/down labels in the 8×8 font (the small fonts lack the ▲▼ glyphs)
    print_centered("\x1e" "E", STK_X + STK_W / 2, STK_Y + 1,          su ? CLR_WHITE : CLR_MEDIUM_GREY);   // up E
    print_centered("\x1f" "Q", STK_X + STK_W / 2, STK_Y + STK_H - 8,  sd ? CLR_WHITE : CLR_MEDIUM_GREY);   // down Q

    // RIGHT BUTTONS — ignition (lit when running) / transmission / build
    dash_btn(BTN_X, IGN_Y, BTN_W, BTN_H, engine_on ? "IGN ON" : "IGN OFF", "I", engine_on, CLR_GREEN);
    dash_btn(BTN_X, TRN_Y, BTN_W, BTN_H, trans_label(), "G", 0, CLR_DARKER_GREY);
    dash_btn(BTN_X, BLD_Y, BTN_W, BTN_H, "BUILD", "TAB", 0, CLR_DARKER_GREY);

    if (is_paused) print_centered("PAUSED", SCREEN_W / 2, SCREEN_H / 2 - 20, CLR_WHITE);
}

// ── BUILD mode: a paused grid editor — place parts, watch the numbers move ───
#define ED_CELL 20                        // editor cell size (px)
#define ED_X    76                        // grid left (palette to its left, readout to its right)
#define ED_Y    56

static void draw_build(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();

    // back to driving (touch/mouse; TAB also works)
    if (ui_button(6, 6, 60, 18, "\x10 drive")) { mode = MODE_DRIVE; reset_vehicle(); }

    // --- part palette (left) ---------------------------------------------------
    static const int PAL[] = { P_FRAME, P_ENGINE, P_WHEEL, P_DRIVE, P_CASTER, P_SEAT, P_NONE };
    static const char *PAL_LBL[] = { "frame", "engine", "wheel", "drive", "caster", "seat", "erase" };
    print("PARTS", 6, 28, CLR_LIGHT_GREY);
    for (int i = 0; i < 7; i++) {
        int by = 38 + i * 20;
        if (ui_button(6, by, 60, 17, PAL_LBL[i])) sel_part = PAL[i];
        if (PAL[i] == sel_part) rect(5, by - 1, 62, 19, CLR_WHITE);     // selection ring
        if (PAL[i] != P_NONE) rectfill(56, by + 4, 8, 8, KIND[PAL[i]].col);  // colour chip
        if (PAL[i] == P_DRIVE) pset(60, by + 7, CLR_ORANGE);            // powered-hub mark
    }

    // --- the grid editor (centre) ---------------------------------------------
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int x = ED_X + c * ED_CELL, y = ED_Y + r * ED_CELL;
            int p = grid[r][c];
            if (p == P_NONE) { rect(x, y, ED_CELL, ED_CELL, CLR_DARK_GREY); }
            else {
                rectfill(x + 1, y + 1, ED_CELL - 2, ED_CELL - 2, KIND[p].col);
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

    // --- readouts (right) ------------------------------------------------------
    char buf[40];
    int rx = 206, ry = 40;
    print("READOUT", rx, ry, CLR_LIGHT_GREY); ry += 11;
    snprintf(buf, sizeof buf, "MASS  %4.1f", M);             print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    snprintf(buf, sizeof buf, "TOPSPD %3.0f", est_top_speed()); print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    snprintf(buf, sizeof buf, "TURN  %4.0f", est_turn_rate()); print(buf, rx, ry, CLR_LIGHT_GREY); ry += 9;
    snprintf(buf, sizeof buf, "ENG %d  WHL %d", nEngines, nWheels); print(buf, rx, ry, CLR_MEDIUM_GREY); ry += 9;
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
    print("TAB drive   click place/erase   1-8 templates   R clear",
          SCREEN_W / 2 - 152, SCREEN_H - 12, CLR_MEDIUM_GREY);

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
    camera((int)cam_x, (int)cam_y);
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

    load_design(0);                       // start on the balanced buggy
    cam_x = sx - SCREEN_W / 2.0f;
    cam_y = sy - SCREEN_H / 2.0f;
}
