#include "studio.h"
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
// ============================================================================

// ── part vocabulary ──────────────────────────────────────────────────────────
// Addressed by enum, never raw index (CLAUDE.md data-driven rule). Each kind:
// mass, engine power, wheel grip, colour.
enum { P_NONE, P_FRAME, P_ENGINE, P_WHEEL, P_SEAT, P_KINDS };
typedef struct { float mass, power, grip; int col; const char *name; } PartKind;
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

#define NDES 5
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
};
static const char *DES_NAME[NDES] = {
    "BUGGY \x07 balanced",
    "HAULER \x07 heavy, sluggish",
    "SPRINTER \x07 twin-engine, fast",
    "JALOPY \x07 off-centre, loose",
    "MOTORBIKE \x07 narrow, darty",
};
static int cur_des = 0;

// derived body properties (recomputed when the build changes)
static float M;                   // total mass
static float comX, comY;          // centre of mass, in local grid px
static float I;                   // moment of inertia about the COM
static float wheelGrip;           // Σ wheel grip
static float frontX;              // local-x of the rig's front edge (for the nose marker)
static int   nEngines, nWheels;   // for the readout
static int   frontalCells;        // rig height across the direction of travel (aero profile)
static float balance;             // COM vs wheelbase: +1 front-heavy (understeer) .. -1 rear-heavy

// ── tuning ───────────────────────────────────────────────────────────────────
#define ENGINE_POWER  2600.0f     // forward force units per engine at full throttle
// Drag is a FORCE (DDA's model): top speed = thrust / drag, MASS-INDEPENDENT — mass
// sets acceleration, not top speed. Drag = base + per-wheel rolling resistance + a
// frontal-profile aero term, so SHAPE and WHEEL COUNT set top speed, not just weight.
#define DRAG_BASE     4.0f        // baseline drag (force per px/s)
#define DRAG_WHEEL    1.5f        // lever: each wheel adds rolling resistance (grip↑, top speed↓)
#define DRAG_AERO     3.9f        // lever: drag per cell of frontal profile (narrow = fast)
#define BRAKE         240.0f      // extra deceleration when braking (px/s^2)
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

// ── skid marks (laid in world space while the tires scrub) ───────────────────
#define MAXSKID 512
typedef struct { float x, y; int life; } Skid;
static Skid skid[MAXSKID];
static int  skid_head;

// ── rigid body state (sx,sy = the COM in world space; rotation pivots about it) ─
static float sx, sy;              // world position of the COM
static float vx, vy;              // world velocity
static float ang, angVel;         // heading (deg, 0 = facing +x / east) + spin

static float cam_x, cam_y;
static float t_eng_snd, t_skid_snd;
static int   is_paused;

static float af(float v) { return v < 0 ? -v : v; }

// ── derive body properties from the part grid ────────────────────────────────
static void recompute_body(void) {
    M = 0; comX = 0; comY = 0; wheelGrip = 0; frontX = 0; nEngines = 0; nWheels = 0;
    int minRow = GH, maxRow = -1;
    float wMinX = 1e9f, wMaxX = -1e9f;     // wheelbase extent (x of frontmost/rearmost wheel)
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            int p = grid[r][c];
            PartKind *k = &KIND[p];
            if (k->mass <= 0) continue;
            float cx = (c + 0.5f) * CELL, cy = (r + 0.5f) * CELL;
            M += k->mass; comX += k->mass * cx; comY += k->mass * cy;
            wheelGrip += k->grip;
            if ((c + 1) * CELL > frontX) frontX = (c + 1) * CELL;
            if (r < minRow) minRow = r;
            if (r > maxRow) maxRow = r;
            if (p == P_ENGINE) nEngines++;
            if (p == P_WHEEL) { nWheels++; if (cx < wMinX) wMinX = cx; if (cx > wMaxX) wMaxX = cx; }
        }
    if (M <= 0) M = 1;
    comX /= M; comY /= M;
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
}

static void reset_vehicle(void) {
    recompute_body();
    sx = 0; sy = 0; vx = vy = 0; ang = 0; angVel = 0;
    for (int i = 0; i < MAXSKID; i++) skid[i].life = 0;
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
static void handle_input(void) {
    if (keyp('R')) reset_vehicle();
    if (keyp('P')) is_paused = !is_paused;
    if (keyp('1')) load_design(0);
    if (keyp('2')) load_design(1);
    if (keyp('3')) load_design(2);
    if (keyp('4')) load_design(3);
    if (keyp('5')) load_design(4);
    in_gas = key('Z') || key(KEY_UP) || btn(0, BTN_A) || btn(0, BTN_UP);
    in_brk = key('X') || key(KEY_DOWN) || btn(0, BTN_B) || btn(0, BTN_DOWN);
    in_steer = (key(KEY_RIGHT) || btn(0, BTN_RIGHT)) - (key(KEY_LEFT) || btn(0, BTN_LEFT));
    in_hand = key(KEY_SPACE);
}

static void lay_skid(float x, float y) {
    skid[skid_head % MAXSKID] = (Skid){ x, y, 150 };
    skid_head++;
}

// ── physics: the honest core ──────────────────────────────────────────────────
static void update_drive(float dt_) {
    float fwx = cos_deg(ang), fwy = sin_deg(ang);   // forward unit vector
    float ltx = -fwy, lty = fwx;                    // lateral (left) unit vector

    // decompose velocity into forward + lateral
    float vf = vx * fwx + vy * fwy;
    float vl = vx * ltx + vy * lty;

    // --- engine: sum thrust + the yaw torque from any off-centre engine --------
    float throttle = in_gas ? 1.0f : (in_brk && vf <= 5.0f ? -REVERSE : 0.0f);
    float thrust = 0, eng_torque = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != P_ENGINE) continue;
            float t = KIND[P_ENGINE].power * throttle;
            float oy = (r + 0.5f) * CELL - comY;     // lateral offset of this engine
            thrust += t;
            eng_torque += -oy * t;                   // off the centre-line → it yaws
        }
    // traction caps how much of that thrust the ground can actually take
    float tract = wheelGrip * GROUND_GRIP * GRIP_TO_FORCE;
    if (af(thrust) > tract && tract > 0) {
        float s = tract / af(thrust);
        thrust *= s; eng_torque *= s;
    }

    // --- linear: net force / mass. Drag is a FORCE (base + per-wheel rolling
    //     resistance + frontal-profile aero), so top speed = thrust/drag is
    //     mass-INDEPENDENT — mass only governs how fast you reach it. -----------
    float drag = DRAG_BASE + DRAG_WHEEL * nWheels + DRAG_AERO * frontalCells;
    vf += ((thrust - drag * vf) / M) * dt_;
    if (in_brk && vf > 0) { vf -= BRAKE * dt_; if (vf < 0) vf = 0; }

    // --- tire grip: bleed the sideways velocity away (the car-feel line) -------
    // the handbrake breaks the tires loose — same grip term, turned down → drift.
    float lat_mult = in_hand ? DRIFT_GRIP_MULT : 1.0f;
    float grip = clamp((wheelGrip * GROUND_GRIP / M) * LAT_GRIP * lat_mult, 0, 1.0f / dt_);
    vl -= vl * grip * dt_;

    // recombine
    vx = fwx * vf + ltx * vl;
    vy = fwy * vf + lty * vl;
    sx += vx * dt_; sy += vy * dt_;

    // --- skid marks: lay at every wheel while the tires are scrubbing sideways -
    if (af(vl) > SKID_SLIP)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++)
                if (grid[r][c] == P_WHEEL) {
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
    angVel += ang_acc * dt_;
    angVel -= angVel * ANG_DAMP * dt_;
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
}

void update(void) {
    handle_input();
    if (is_paused) return;
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

// ── vehicle: draw each occupied cell as a rotated quad ───────────────────────
static void draw_cell(int r, int c, int col) {
    float lx = (c + 0.5f) * CELL - comX, ly = (r + 0.5f) * CELL - comY;
    float h = CELL * 0.5f;
    float ax, ay, bx, by, cx2, cy2, dx, dy;
    rot(lx - h, ly - h, &ax, &ay); rot(lx + h, ly - h, &bx, &by);
    rot(lx + h, ly + h, &cx2, &cy2); rot(lx - h, ly + h, &dx, &dy);
    int xy[8] = { (int)ax, (int)ay, (int)bx, (int)by, (int)cx2, (int)cy2, (int)dx, (int)dy };
    polyfill(xy, 4, col);
}

static void draw_vehicle(void) {
    // body cells first, wheels on top so they read as round-ish dark pads
    for (int pass = 0; pass < 2; pass++)
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++) {
                int p = grid[r][c];
                if (p == P_NONE) continue;
                int isWheel = (p == P_WHEEL);
                if (isWheel != pass) continue;
                draw_cell(r, c, KIND[p].col);
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
    if (in_hand && spd > 8) print("DRIFT", 4, 40, CLR_YELLOW);
    print("1-5 swap rig  \x1b\x1a steer  \x18\x19 gas/brake  SPACE drift",
          SCREEN_W / 2 - 132, SCREEN_H - 12, CLR_MEDIUM_GREY);
    if (is_paused) print("PAUSED", SCREEN_W / 2 - 22, SCREEN_H / 2, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARKER_GREY);                 // asphalt
    camera((int)cam_x, (int)cam_y);
    draw_ground();
    draw_course();
    for (int i = 0; i < MAXSKID; i++)     // tire marks burned into the road
        if (skid[i].life > 0)
            pset((int)skid[i].x, (int)skid[i].y,
                 skid[i].life > 90 ? CLR_BLACK : CLR_BROWNISH_BLACK);
    draw_vehicle();
    camera(0, 0);
    hud();
}

void init(void) {
    // part vocabulary (ordered by enum — no designated inits, libtcc-safe)
    KIND[P_NONE]   = (PartKind){ 0,    0,            0,    0,               "." };
    KIND[P_FRAME]  = (PartKind){ 1.0f, 0,            0,    CLR_LIGHT_GREY,  "frame" };
    KIND[P_ENGINE] = (PartKind){ 4.0f, ENGINE_POWER, 0,    CLR_RED,         "engine" };
    KIND[P_WHEEL]  = (PartKind){ 1.5f, 0,            1.0f, CLR_BLACK,       "wheel" };
    KIND[P_SEAT]   = (PartKind){ 1.2f, 0,            0,    CLR_BLUE,        "seat" };

    load_design(0);                       // start on the balanced buggy
    cam_x = sx - SCREEN_W / 2.0f;
    cam_y = sy - SCREEN_H / 2.0f;
}
