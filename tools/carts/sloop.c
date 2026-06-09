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
#define ENGINE_POWER  2600.0f     // forward force units per engine at full throttle
// Drag is a FORCE (DDA's model): top speed = thrust / drag, MASS-INDEPENDENT — mass
// sets acceleration, not top speed. Drag = base + per-wheel rolling resistance + a
// frontal-profile aero term, so SHAPE and WHEEL COUNT set top speed, not just weight.
#define DRAG_BASE     4.0f        // baseline drag (force per px/s)
#define DRAG_WHEEL    1.5f        // lever: each wheel adds rolling resistance (grip↑, top speed↓)
#define DRAG_AERO     3.9f        // lever: drag per cell of frontal profile (narrow = fast)
#define BRAKE         560.0f      // max braking decel (px/s^2) — real brakes >> engine accel;
                                  // capped per-rig by tyre grip below (GRIP_TO_FORCE·grip/M),
                                  // so MORE/BETTER WHEELS = harder stops (an under-wheeled rig
                                  // can't haul up as fast). At slow speed this stops you dead.
#define REV_DWELL     10          // frames braked at a standstill before reverse engages — lets
                                  // a slam-brake LAND a firm stop first, then back up if held
#define ROLL_FRIC     16.0f       // CONSTANT rolling/bearing friction (px/s^2) — what actually
                                  // STOPS a coasting rig. Drag ∝ v only asymptotes to 0 (floaty);
                                  // this constant term dominates at low speed and snaps v to rest.
#define REVERSE       0.55f       // reverse throttle fraction
#define LAT_GRIP      32.0f       // lateral velocity killed per second (tire grip)
#define STEER_RESP    680.0f      // steering authority (deg/s^2) at speed
#define ANG_DAMP      5.0f        // angular self-centering (1/s)
#define REF_GYRO      130.0f      // gyradius^2 (px^2) a "normal" rig turns easily at
#define ENG_YAW_K     0.9f        // how hard an off-centre engine yaws the rig
#define BALANCE_K     0.4f        // lever: front-heavy understeers, rear-heavy oversteers
#define GRIP_TO_FORCE 2000.0f     // wheel grip → max traction force
#define GROUND_GRIP   1.0f        // road: plenty (sand/mud come in rung 3)
#define DRIFT_GRIP_MULT 0.13f     // handbrake: lateral grip drops to this fraction
#define SKID_SLIP     16.0f       // lateral speed (px/s) where tires start marking
// ── ground-scrape: a cell hanging past the wheel span drags on the floor ──────
#define SCRAPE_DRAG   9.0f        // extra drag force per dragging cell (top speed ↓)
#define SCRAPE_LAT    7.0f        // extra lateral resistance per dragging cell (anchors sideways)
#define SCRAPE_YAW    160.0f      // an off-centre scrape twists the rig (deg/s^2 per cell·unit)
#define SCRAPE_MINSPD 10.0f       // below this speed nothing scrapes (kinetic friction only when moving)
#define HEAT_RISE     1.4f        // heat gained per second per dragging cell while moving
#define HEAT_COOL     0.8f        // heat lost per second when not scraping
// ── dynamic stability (tipping under cornering load) ─────────────────────────
#define STAB_H        0.022f      // COM-height stand-in: lateral-g → COM load-shift (px per px/s^2)
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
static int   brake_dwell;         // frames held at a standstill under braking (delays reverse)
static int   reversing;           // latched: braking past the dwell flipped us into reverse

// ── rigid body state (sx,sy = the COM in world space; rotation pivots about it) ─
static float sx, sy;              // world position of the COM
static float vx, vy;              // world velocity
static float ang, angVel;         // heading (deg, 0 = facing +x / east) + spin

static float cam_x, cam_y;
static float t_eng_snd, t_skid_snd, t_scrape_snd;
static int   is_paused;

// ── modes: BUILD (paused grid editor) ↔ DRIVE (the rig loose in the world) ───
enum { MODE_DRIVE, MODE_BUILD };
static int mode = MODE_DRIVE;
static int sel_part = P_WHEEL;    // palette selection; P_NONE = the eraser

static float af(float v) { return v < 0 ? -v : v; }

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
    heat = 0; tip_amt = 0; brake_dwell = 0; reversing = 0;
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
static int in_gas, in_brk, in_steer, in_hand;
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
    // ---- DRIVE input ----
    if (keyp('R')) reset_vehicle();
    if (keyp('P')) is_paused = !is_paused;
    in_gas = key('Z') || key(KEY_UP) || btn(0, BTN_A) || btn(0, BTN_UP);
    in_brk = key('X') || key(KEY_DOWN) || btn(0, BTN_B) || btn(0, BTN_DOWN);
    in_steer = (key(KEY_RIGHT) || btn(0, BTN_RIGHT)) - (key(KEY_LEFT) || btn(0, BTN_LEFT));
    in_hand = key(KEY_SPACE);
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

    // --- engine: sum thrust + the yaw torque from any off-centre engine --------
    // reverse engages only once braking has LANDED a stop (dwelled at ~0), then LATCHES
    // so the rig actually backs up — a slam-brake stops you dead first, reverses if held.
    if (!in_brk || in_gas) reversing = 0;
    else if (af(vf) <= 1.0f && brake_dwell >= REV_DWELL) reversing = 1;
    float throttle = in_gas ? 1.0f : (reversing ? -REVERSE : 0.0f);
    float thrust = 0, eng_torque = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != P_ENGINE) continue;
            float t = KIND[P_ENGINE].power * throttle;
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
    // braking: strong, but capped by what the tyres can grip (GRIP_TO_FORCE·grip/M) —
    // a well-wheeled rig stops hard, an under-wheeled one can't. Works fwd or reverse.
    if (in_brk && af(vf) > 1.0f) {
        float brake = clamp(GRIP_TO_FORCE * wheelGrip * GROUND_GRIP / M, 0, BRAKE);
        float d = brake * dt_;
        if (vf > 0) { vf -= d; if (vf < 0) vf = 0; }
        else if (throttle == 0) { vf += d; if (vf > 0) vf = 0; }   // braking while rolling back
    }
    if (in_brk && af(vf) <= 1.0f) brake_dwell++; else brake_dwell = 0;

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
    float speed_factor = clamp(af(vf) / 30.0f, 0, 1);
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
    angVel -= STAB_YAW_K * lead * angVel * clamp(spd0 / 25.0f, 0, 1) * dt_;
    ang += angVel * dt_;
    if (ang < 0) ang += 360; else if (ang >= 360) ang -= 360;

    // --- sound -----------------------------------------------------------------
    float spd = fsqrt(vx * vx + vy * vy);
    if (in_gas) {
        t_eng_snd -= dt_;
        if (t_eng_snd <= 0) { hit(28 + (int)(spd * 0.12f), INSTR_SAW, 3, 90); t_eng_snd = 0.08f; }
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

    cam_x = lerp(cam_x, sx - SCREEN_W / 2.0f, 0.15f);
    cam_y = lerp(cam_y, sy - SCREEN_H / 2.0f, 0.15f);

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
    // deterministic speckle so speed is legible
    for (int x = x0; x <= x1; x += step)
        for (int y = y0; y <= y1; y += step) {
            unsigned h = hash2(x / step, y / step);
            if ((h & 7) == 0) {
                int px = x + (int)(h >> 4) % step, py = y + (int)(h >> 12) % step;
                pset(px, py, (h & 8) ? CLR_MEDIUM_GREY : CLR_BROWN);
            }
        }
}

// ── the course: schematic lanes + roundabouts + cones ────────────────────────
// Purely VISUAL for now (rung 1.5) — lines to use as roads and objects to weave
// through, drawn in the same design style as the grid. Nothing collides yet; this
// is the parcours skeleton that rung 3+ will make solid (traction zones, obstacles).
// Everything is a deterministic function of world position, so it's stable as you
// drive and the world is effectively infinite.
#define LANE_SP   192             // block size: roads laid on this world grid
#define LANE_W    64              // lane band width = 2 of the 32px grid cells

static int ifloordiv(int a, int b) {
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}

static void draw_course(void) {
    int L = (int)cam_x, R = L + SCREEN_W, T = (int)cam_y, B = T + SCREEN_H;
    int kx0 = ifloordiv(L, LANE_SP) - 1, kx1 = ifloordiv(R, LANE_SP) + 1;
    int ky0 = ifloordiv(T, LANE_SP) - 1, ky1 = ifloordiv(B, LANE_SP) + 1;
    int hw = LANE_W / 2;

    // 1. tarmac bands (a touch lighter than the ground), crossing in a grid
    for (int kx = kx0; kx <= kx1; kx++)
        rectfill(kx * LANE_SP - hw, T - 1, LANE_W, (B - T) + 2, CLR_DARK_GREY);
    for (int ky = ky0; ky <= ky1; ky++)
        rectfill(L - 1, ky * LANE_SP - hw, (R - L) + 2, LANE_W, CLR_DARK_GREY);

    // 2. bright curbs + dashed centre line (the bit that reads as "lane")
    for (int kx = kx0; kx <= kx1; kx++) {
        int cx = kx * LANE_SP;
        line(cx - hw, T, cx - hw, B, CLR_LIGHT_GREY);
        line(cx + hw, T, cx + hw, B, CLR_LIGHT_GREY);
        for (int y = ifloordiv(T, 24) * 24; y < B; y += 24) line(cx, y, cx, y + 11, CLR_YELLOW);
    }
    for (int ky = ky0; ky <= ky1; ky++) {
        int cy = ky * LANE_SP;
        line(L, cy - hw, R, cy - hw, CLR_LIGHT_GREY);
        line(L, cy + hw, R, cy + hw, CLR_LIGHT_GREY);
        for (int x = ifloordiv(L, 24) * 24; x < R; x += 24) line(x, cy, x + 11, cy, CLR_YELLOW);
    }

    // 3. roundabout islands at ~1/4 of crossings (steer AROUND them)
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++)
            if ((hash2(kx, ky) & 3) == 0) {
                int cx = kx * LANE_SP, cy = ky * LANE_SP;
                circfill(cx, cy, 13, CLR_DARK_GREEN);
                circ(cx, cy, 13, CLR_LIGHT_GREY);
            }

    // 4. cones scattered in the blocks (objects to weave through)
    int span = LANE_SP - LANE_W - 8;
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            unsigned h = hash2(kx * 7 + 1, ky * 7 + 3);
            if ((h & 3) != 0) continue;
            int cx = kx * LANE_SP + hw + 4 + (int)(h % span);
            int cy = ky * LANE_SP + hw + 4 + (int)((h >> 8) % span);
            circfill(cx, cy, 4, CLR_ORANGE);
            circ(cx, cy, 4, CLR_BROWNISH_BLACK);
        }
}

// glow ramp for a scraping cell — red (cold/parked) → orange → yellow → white (hot)
static int hot_col(void) {
    return heat > 0.66f ? CLR_WHITE : heat > 0.40f ? CLR_YELLOW
         : heat > 0.15f ? CLR_ORANGE : CLR_RED;
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

// ── HUD ────────────────────────────────────────────────────────────────────────
static void hud(void) {
    char buf[48];
    float spd = fsqrt(vx * vx + vy * vy);
    print(DES_NAME[cur_des], 4, 4, CLR_WHITE);
    snprintf(buf, sizeof buf, "SPEED %4.0f", spd);   print(buf, 4, 14, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "MASS  %4.1f", M);     print(buf, 4, 22, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "ENG %d  WHL %d", nEngines, nWheels);
    print(buf, 4, 30, CLR_MEDIUM_GREY);
    print(drive_label(), 4, 38, CLR_MEDIUM_GREY);
    if (in_hand && spd > 8) print("DRIFT", 4, 48, CLR_YELLOW);
    if (tip_amt > 0.05f) print("TIP!", 44, 48, CLR_ORANGE);   // tipping onto an unsupported corner
    if (nDrag > 0) {                       // scraping warning + heat bar
        snprintf(buf, sizeof buf, "SCRAPE x%d", nDrag);
        print(buf, SCREEN_W - 74, 4, hot_col());
        bar(SCREEN_W - 74, 13, 68, 4, heat, heat > 0.66f ? CLR_RED : CLR_ORANGE, CLR_DARKER_GREY);
    }
    print("TAB build  1-8 rig  \x1b\x1a steer  \x18\x19 gas/brake  SPACE drift",
          SCREEN_W / 2 - 140, SCREEN_H - 12, CLR_MEDIUM_GREY);
    if (is_paused) print("PAUSED", SCREEN_W / 2 - 22, SCREEN_H / 2, CLR_WHITE);
}

// ── BUILD mode: a paused grid editor — place parts, watch the numbers move ───
#define ED_CELL 20                        // editor cell size (px)
#define ED_X    76                        // grid left (palette to its left, readout to its right)
#define ED_Y    56

static void draw_build(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();

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
