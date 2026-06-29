/* de:meta
{
  "title": "orbit",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "verlet-integration",
    "particle-system"
  ],
  "lineage": "Kerbal Space Program in miniature; novel in running the live ship and the dotted predicted-trajectory through the exact same symplectic-Euler integrator so the prediction never lies.",
  "genre": "space",
  "description": "A gravity sandbox - build a rocket, launch, and watch the dotted PREDICTED PATH curve into orbit (or, far more often, into the ground). One honest physics core: point-mass inverse-square gravity, with the SAME integrator running the live ship AND the prediction, so the dots never lie - they show exactly where you'd coast if you cut the engine now. Burn until the path closes into a loop that clears the surface and you're in orbit; apoapsis (peach) and periapsis (blue) are marked live. Hold Z to thrust (on the pad it only lifts if TWR > 1), LEFT/RIGHT steer, SPACE stages, R resets. Press 1/2/3 to swap rockets and feel each way to fail: FALCON flies a clean gravity turn, finless PENCIL cartwheels, underpowered BRICK never leaves the pad. Crashes scatter the rocket into its own parts. A Kerbal-Space-Program-in-miniature; MVP - the parts-bin builder is next."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// ============================================================================
// ORBIT  —  a gravity sandbox.  Build a rocket, launch, fail miserably.
//
// SPEC + ROADMAP: docs/guides/cart-specs/orbit.md — the achievement ladder, what's
// reachable today vs. gated, and the next build (Mun-orbit capture) with code hooks.
// This header is the deep "why"; that doc is the map above it.
//
//   ◄ / ►        steer (rotate the rocket)
//   Z (hold)     thrust  — on the pad it only lifts if TWR > 1
//   SPACE        stage / relaunch from the wreck screen
//   R            reset to the pad
//   1 / 2 / 3    swap rocket: balanced · finless tumbler · underpowered
//
// ── what this is ─────────────────────────────────────────────────────────────
// A sibling of coaster.c and galerijflat.c: a *systemic sandbox toy*. You set a
// thing up, then watch the system tell you the truth. Coaster: draw a track,
// gravity runs it. Galerijflat: a building that lives on its own clock. Here:
// point a rocket, burn, and let orbital mechanics judge your design. A
// Kerbal-Space-Program-in-miniature — and like KSP, the 95% where the rocket
// betrays you is the loop, not the rare success.
//
// The chosen KSP "transcendent moment": the first time the predicted path curves
// all the way around and *closes into a loop clear of the ground* — "I'm in
// orbit." Everything is built to make that one moment land.
//
// Three milestones now ride that same honest core, in KSP's own order of escalation:
//   ORBIT ACHIEVED  — the predicted path closes clear of the ground (above).
//   WELCOME HOME     — reach orbit, then set back down softly (a *round trip*, not a
//                      crash): try_contact() rewards a gentle planet landing only
//                      after orbit_done.
//   MUN ENCOUNTER    — the coast reaches a SECOND, static gravity well (the Mun).
//                      grav_acc() sums both wells, so the SAME predict() that draws
//                      your orbit also bends the dotted path around the Mun the
//                      instant your apoapsis reaches it. No new math — just a second
//                      attractor. (Closing a loop *around* the Mun, and a moving Mun,
//                      are the next phase — see the Mun tuning block below.)
//
// ── the one honest core ──────────────────────────────────────────────────────
// Point-mass inverse-square gravity. The SAME integrator (grav_acc + symplectic
// Euler) runs the live ship AND the dotted PREDICTED PATH (predict()), so the
// dots never lie: they show exactly where you'd coast if you cut the engine now.
// Watch the path bend as you burn. This shared-core honesty is the whole design
// — never give the prediction its own cheaper math, or it will start lying.
//
// ── why the world is big and gravity is gentle (DON'T "fix" this) ────────────
// The load-bearing tuning insight, learned the hard way. A small/light planet
// has a low escape velocity, which leaves almost no delta-v room: any rocket
// powerful enough to orbit is powerful enough to fling itself to escape, and the
// high surface gravity forces sub-second burns you can't fly by hand. A big,
// heavy planet (R_PLANET 120, G_SURF 8) fixes BOTH at once — higher escape
// velocity = room for bound orbits, and burn_time ∝ sqrt(R/g)/TWR, so low g
// buys long, steerable burns. If orbits ever balloon to a speck-sized planet or
// burns get twitchy, the lever is the world scale, not the rocket.
//
// ── the delta-v knife-edge ────────────────────────────────────────────────────
// With 1-button thrust (no throttle), the gap between "falls short of orbit"
// (~circular velocity) and "escapes" (~1.41× that) is narrow, so FALCON is tuned
// to sit right around escape velocity. A clean orbit therefore wants a deliberate
// TWO-burn: gravity-turn up, coast to apoapsis, then a *small* prograde nudge at
// the apex to lift periapsis off the surface (watch the PERI number climb). The
// real cure for fiddliness is a throttle — or the builder's delta-v budget.
//
// ── how each failure is produced (not magic) ─────────────────────────────────
//   TWR < 1     → update_pad() never leaves the pad; it just shudders + smokes.
//   finless     → update_fly() aero term DIVERGES (any tilt grows) + a constant
//                 lean seeds it → it cartwheels. Fins flip the sign → weathervane.
//   out of fuel → thrust cuts, you coast, gravity wins, you fall back and smash.
//
// MVP-1: the rocket is hardcoded (DESIGNS[] — three test designs that exercise
// the failure taxonomy). MVP-2 bolts a parts-bin builder onto this same core,
// feeding DESIGNS[] from parts you snap together instead of these literals.
// ============================================================================

// ── world / physics tuning ──────────────────────────────────────────────────
// Gentle gravity (low G_SURF) buys long, flyable burns and keeps orbits a few
// planet-radii across — so the planet stays a presence on screen, not a speck.
#define R_PLANET   120.0f         // planet radius (world px)
#define G_SURF     8.0f           // surface gravity px/s^2
#define MU         (G_SURF * R_PLANET * R_PLANET)   // gravitational parameter G*M
#define DESPAWN_R  (R_PLANET * 14)   // drift past here = gone (a true escape coasts out)

// ── the Mun — a second, STATIC gravity well ───────────────────────────────────
// Deliberately fixed in space: predict() integrates the exact same two wells the
// live ship feels, so the dotted path stays honest (the cart's whole principle).
// A *moving* Mun would force predict() to propagate the Mun's future position
// across its lookahead — patched-conics / SOI territory, a later phase. Two fixed
// attractors + one test mass is fully deterministic and DOES admit closed orbits
// around either body, so a real Mun orbit is reachable on this same core.
// Distance is the tuning lever (like world scale for the planet): too close/heavy
// and the Mun yanks you off the pad; too far/light and the window is a needle.
#define R_MUN      42.0f          // Mun radius (world px)
#define G_MUN      6.0f           // Mun surface gravity px/s^2
#define MU_MUN     (G_MUN * R_MUN * R_MUN)   // Mun gravitational parameter
#define MUN_X      240.0f         // Mun centre (world px) — up & east of the pad.
#define MUN_Y      -200.0f        //   Distance ~310 px sits just inside FALCON's
                                  //   apoapsis reach (~330), so a committed eastward
                                  //   gravity turn coasts THROUGH it — an achievement,
                                  //   not a gimme. Place it past ~340 and no stock
                                  //   rocket can climb to it; distance is the lever.
#define MUN_SOI    (R_MUN * 3.5f) // predicted pass within here = "encounter"

#define CTRL_AUTH  150.0f         // steering authority (deg/s^2 from ◄/►)
#define ANG_DAMP   0.6f           // angular velocity damping per second
#define STAB       0.14f          // fin weathervane strength (toward velocity)
#define INSTAB     0.45f          // finless aero divergence (away from velocity)

#define LAND_SPEED 26.0f          // touch slower than this (nose-up) = a landing
#define LAND_TILT  22.0f          // ...and tilted less than this many degrees

// ── prediction ───────────────────────────────────────────────────────────────
#define PDT        0.045f         // prediction timestep
#define PRED_MAX   2400           // max integration steps looked ahead
#define PRED_STEP  4              // record a sample every Nth step
#define PRED_PTS   560            // sample buffer

// ── pools ─────────────────────────────────────────────────────────────────────
#define MAXP   220                // smoke / spark particles
#define MAXD   40                 // debris chunks
#define MAXSTAR 90

// ── rocket ──────────────────────────────────────────────────────────────────
#define MAX_STAGES 3
typedef struct {
    float dry, fuel, fuelMax;     // mass units
    float thrust;                 // mass*px/s^2
    float burn;                   // fuel units / sec at full throttle
    int   fins;                   // 1 = this stage carries fins
} Stage;

typedef struct {
    Stage stage[MAX_STAGES];
    int   nstages;
    float pod;                    // payload (command pod + nose) mass — never drops
    const char *name;
} Design;

static Stage  st[MAX_STAGES];     // live mutable copy of the current design
static int    nstages, activeStage;
static float  pod;
static const char *design_name;

// three test rockets so we can FEEL each failure before the builder exists
static const Design DESIGNS[3] = {
    // [0] balanced + finned: can actually make orbit with a gravity turn
    { {{2.2f, 9.0f, 9.0f, 168.f, 4.4f, 1},
       {1.1f, 4.0f, 4.0f, 60.f, 3.5f, 1}}, 2, 1.2f, "FALCON (balanced)" },
    // [1] finless: aerodynamically unstable — tips over and cartwheels
    { {{2.2f, 9.0f, 9.0f, 168.f, 4.4f, 0},
       {1.1f, 4.0f, 4.0f, 60.f, 3.5f, 0}}, 2, 1.2f, "PENCIL (no fins)" },
    // [2] underpowered: TWR < 1 — roars on the pad and goes nowhere
    { {{3.5f, 11.f, 11.f, 105.f, 4.0f, 1}}, 1, 2.0f, "BRICK (underpowered)" },
};

// ── ship state ────────────────────────────────────────────────────────────────
enum { ST_PAD, ST_FLY, ST_WRECK };
static int   phase = ST_PAD;
static float sx, sy, vx, vy;      // world position / velocity
static float ang, angVel;         // heading (deg, 0 = nose toward space at pad) + spin
static int   thrusting;
static int   orbit_done, escaped; // one-shot celebration flags
static int   mun_enc;             // one-shot: predicted path reached the Mun
static int   mun_orbit;           // one-shot: predicted path closed a loop around the Mun

// flight recorder
static float maxAlt, maxSpeed, tAloft;
static const char *fate;

// prediction results
static float predPx[PRED_PTS], predPy[PRED_PTS];
static int   predN;
static int   predCrash, predEllipse, predEscape;
static float predApo, predPeri;   // distances from planet centre
static int   predMunEnc;          // predicted path passes within the Mun's SOI
static float predMunClosest;      // closest predicted approach to the Mun centre
static int   predMunOrbit;        // predicted path closes a loop WITHIN the Mun's SOI
static float predMunPeri, predMunApo;  // Mun-relative peri/apo while inside the SOI

// camera (smoothed)
static float cam_x, cam_y, cam_zoom = 1;

// ── pools ─────────────────────────────────────────────────────────────────────
typedef struct { float x, y, vx, vy; int life, life0, col; } Particle;
static Particle parts[MAXP];
typedef struct { float x, y, vx, vy, a, av, len, wid; int life, col; } Debris;
static Debris debris[MAXD];
typedef struct { float x, y; int col; } Star;
static Star stars[MAXSTAR];

static int   is_paused;
static float t_thrust_snd;        // engine-rumble retrigger timer

// ── small helpers ─────────────────────────────────────────────────────────────
static float af(float v) { return v < 0 ? -v : v; }
static float g0(void) { return G_SURF; }   // surface gravity, for TWR

// total live mass: payload + every alive stage (activeStage .. top)
static float mass(void) {
    float m = pod;
    for (int i = activeStage; i < nstages; i++) m += st[i].dry + st[i].fuel;
    return m;
}
static int any_fins(void) {
    for (int i = activeStage; i < nstages; i++) if (st[i].fins) return 1;
    return 0;
}
// thrust available from the active stage right now (0 if dropped or dry)
static float cur_thrust(void) {
    if (activeStage >= nstages) return 0;
    return st[activeStage].fuel > 0 ? st[activeStage].thrust : 0;
}
// visual half-length grows with how much rocket is left
static float vis_half(void) { return 5.0f + 4.5f * (nstages - activeStage); }

static void grav_acc(float x, float y, float *ax, float *ay) {
    // planet at the origin
    float dx = x, dy = y, r2 = dx * dx + dy * dy, r = fsqrt(r2);
    if (r < 1) r = 1;
    float a = MU / r2;
    *ax = -a * dx / r;
    *ay = -a * dy / r;
    // + the Mun (static well) — this single addition is what makes the predicted
    // path bend around the Mun, for free, in both the live ship and predict().
    dx = x - MUN_X; dy = y - MUN_Y; r2 = dx * dx + dy * dy; r = fsqrt(r2);
    if (r < 1) r = 1;
    a = MU_MUN / r2;
    *ax += -a * dx / r;
    *ay += -a * dy / r;
}

// nose direction from heading. ang=0 → straight up (0,-1); +ang turns clockwise.
static void nose_dir(float a, float *fx, float *fy) { *fx = sin_deg(a); *fy = -cos_deg(a); }

// ── spawn helpers ──────────────────────────────────────────────────────────────
static void spawn_particle(float x, float y, float vx_, float vy_, int life, int col) {
    for (int i = 0; i < MAXP; i++)
        if (parts[i].life <= 0) {
            parts[i] = (Particle){ x, y, vx_, vy_, life, life, col };
            return;
        }
}

static void explode(const char *why) {
    phase = ST_WRECK;
    fate = why;
    thrusting = 0;
    shake(7.0f);
    hit(34, INSTR_NOISE, 8, 420);
    hit(28, INSTR_NOISE, 7, 600);
    // spark + smoke burst
    for (int i = 0; i < 90; i++) {
        float a = rnd_float_between(0, 360);
        float sp = rnd_float_between(20, 150);
        int col = (i & 3) == 0 ? CLR_WHITE : (i & 1) ? CLR_ORANGE : CLR_YELLOW;
        spawn_particle(sx, sy, sin_deg(a) * sp, -cos_deg(a) * sp,
                       rnd_between(14, 40), col);
    }
    // the rocket breaks into the parts you flew: a debris chunk per alive stage
    int spawned = 0;
    for (int i = activeStage; i < nstages && spawned < MAXD; i++) {
        for (int k = 0; k < MAXD; k++)
            if (debris[k].life <= 0) {
                float a = rnd_float_between(0, 360);
                float sp = rnd_float_between(30, 90);
                debris[k] = (Debris){ sx, sy, vx * 0.3f + sin_deg(a) * sp,
                    vy * 0.3f - cos_deg(a) * sp, ang, rnd_float_between(-300, 300),
                    7 + 5 * (nstages - i), 3.2f, rnd_between(80, 150),
                    st[i].fins ? CLR_MEDIUM_GREY : CLR_LIGHT_GREY };
                spawned++;
                break;
            }
    }
}

// ── (re)load a design onto the pad ───────────────────────────────────────────
static void load_design(int idx) {
    const Design *d = &DESIGNS[idx];
    nstages = d->nstages;
    pod = d->pod;
    design_name = d->name;
    for (int i = 0; i < nstages; i++) st[i] = d->stage[i];
}

static int cur_design = 0;

static void reset_pad(void) {
    load_design(cur_design);
    activeStage = 0;
    phase = ST_PAD;
    ang = 0; angVel = 0; vx = vy = 0;
    thrusting = 0; orbit_done = 0; escaped = 0; mun_enc = 0; mun_orbit = 0;
    maxAlt = maxSpeed = tAloft = 0;
    fate = 0;
    // sit on the pad at the top of the planet, nose pointing to space
    sx = 0;
    sy = -(R_PLANET + vis_half());
    for (int i = 0; i < MAXP; i++) parts[i].life = 0;
    for (int i = 0; i < MAXD; i++) debris[i].life = 0;
}

// ── trajectory prediction (gravity-only coast from the current state) ─────────
static void predict(void) {
    predN = 0; predCrash = 0; predEllipse = 0; predEscape = 0;
    predMunEnc = 0; predMunClosest = 1e9f;
    predMunOrbit = 0; predMunPeri = 1e9f; predMunApo = 0;
    int   munIn = 0;          // currently inside the Mun's SOI this pass?
    float munSwept = 0, munPrevA = 0;
    float px = sx, py = sy, pvx = vx, pvy = vy;
    float r0 = fsqrt(px * px + py * py);
    predApo = r0; predPeri = r0;
    // specific orbital energy: >= 0 means unbound (parabola/hyperbola) — a true
    // escape. This is the honest test; the path simply never closes.
    float eps = (pvx * pvx + pvy * pvy) * 0.5f - MU / r0;
    predEscape = eps >= 0;
    float prevA = angle_to(0, 0, (int)px, (int)py);
    float swept = 0;
    for (int i = 0; i < PRED_MAX; i++) {
        float ax, ay; grav_acc(px, py, &ax, &ay);
        pvx += ax * PDT; pvy += ay * PDT;
        px += pvx * PDT; py += pvy * PDT;
        float rr = fsqrt(px * px + py * py);
        if (rr < predPeri) predPeri = rr;
        if (rr > predApo)  predApo = rr;
        if ((i % PRED_STEP) == 0 && predN < PRED_PTS) { predPx[predN] = px; predPy[predN] = py; predN++; }
        if (rr <= R_PLANET) {           // it comes back down
            predCrash = 1;
            if (predN < PRED_PTS) { predPx[predN] = px; predPy[predN] = py; predN++; }
            break;
        }
        // distance to the Mun — track the closest approach + a hard impact
        float mdx = px - MUN_X, mdy = py - MUN_Y;
        float rm = fsqrt(mdx * mdx + mdy * mdy);
        if (rm < predMunClosest) predMunClosest = rm;
        if (rm <= R_MUN) {              // path runs straight into the Mun
            predCrash = 1;
            if (predN < PRED_PTS) { predPx[predN] = px; predPy[predN] = py; predN++; }
            break;
        }
        // Mun-orbit detection (PROVISIONAL — see orbit.md "Mun orbit is blocked on
        // physics, not detection"). Accumulate the angle swept around the Mun while
        // the path stays in the Mun's neighbourhood; reset the instant it RECEDES.
        // 358° swept without receding = a loop bound to the Mun (a capture), not a
        // flyby (recedes having swept <360) nor a big planet orbit (recedes). This
        // logic is UNVALIDATED against a real captured path: at the Mun's current
        // distance no stable capture orbit exists to fire it (a circular seed
        // crashes or escapes within one revolution). The 2×SOI reset bound and the
        // 358° threshold are best-guesses to be re-tuned once a throttle + a farther
        // Mun make capture reachable and real captured paths can be measured.
        if (rm < MUN_SOI * 2.0f) {
            float ma = angle_to((int)MUN_X, (int)MUN_Y, (int)px, (int)py);
            if (!munIn) {               // just entered the SOI — start fresh
                munIn = 1; munSwept = 0; munPrevA = ma;
                predMunPeri = rm; predMunApo = rm;
            } else {
                if (rm < predMunPeri) predMunPeri = rm;
                if (rm > predMunApo)  predMunApo = rm;
                float mda = ma - munPrevA;
                if (mda > 180) mda -= 360; else if (mda < -180) mda += 360;
                munSwept += mda; munPrevA = ma;
                // closed loop clear of the Mun's surface = MUN ORBIT
                if (af(munSwept) >= 358 && predMunPeri > R_MUN + 4) {
                    predMunOrbit = 1;
                    if (predN < PRED_PTS) { predPx[predN] = px; predPy[predN] = py; predN++; }
                    break;
                }
            }
        } else {
            munIn = 0;                   // left the SOI — any partial sweep is void
        }
        if (rr > DESPAWN_R) break;      // escape: stop drawing once it's left the frame
        float a = angle_to(0, 0, (int)px, (int)py);
        float da = a - prevA;
        if (da > 180) da -= 360; else if (da < -180) da += 360;
        swept += da; prevA = a;
        if (af(swept) >= 358) { predEllipse = 1; break; }   // a full closed orbit
    }
    predMunEnc = predMunClosest < MUN_SOI;   // did the coast graze the Mun's pull?
}

// ── input ──────────────────────────────────────────────────────────────────────
static void handle_input(void) {
    if (keyp('R')) reset_pad();
    if (keyp('1')) { cur_design = 0; reset_pad(); }
    if (keyp('2')) { cur_design = 1; reset_pad(); }
    if (keyp('3')) { cur_design = 2; reset_pad(); }
    if (keyp('P')) is_paused = !is_paused;

    if (phase == ST_WRECK) {
        if (keyp(KEY_SPACE)) reset_pad();
        return;
    }
    // staging
    if (keyp(KEY_SPACE) && activeStage < nstages - 1) {
        // drop the active (spent or not) stage → it becomes debris, ship lightens
        for (int k = 0; k < MAXD; k++)
            if (debris[k].life <= 0) {
                float fx, fy; nose_dir(ang, &fx, &fy);
                float bx = sx - fx * vis_half(), by = sy - fy * vis_half();
                debris[k] = (Debris){ bx, by, vx - fx * 40, vy - fy * 40, ang,
                    rnd_float_between(-120, 120), 7 + 5 * (nstages - activeStage),
                    3.2f, 120, CLR_MEDIUM_GREY };
                break;
            }
        activeStage++;
        hit(48, INSTR_NOISE, 5, 120);
        shake(1.5f);
    }
    thrusting = btn(0, BTN_A) || key('Z');
}

// ── update ──────────────────────────────────────────────────────────────────────
static void tick_particles(float dt_) {
    for (int i = 0; i < MAXP; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0) continue;
        p->x += p->vx * dt_; p->y += p->vy * dt_;
        // smoke drifts & slows; sparks fall under a little gravity
        p->vx *= 0.94f; p->vy = p->vy * 0.94f + 30 * dt_;
        p->life--;
    }
    for (int i = 0; i < MAXD; i++) {
        Debris *d = &debris[i];
        if (d->life <= 0) continue;
        float ax, ay; grav_acc(d->x, d->y, &ax, &ay);
        d->vx += ax * dt_; d->vy += ay * dt_;
        d->x += d->vx * dt_; d->y += d->vy * dt_;
        d->a += d->av * dt_;
        float r = fsqrt(d->x * d->x + d->y * d->y);
        if (r <= R_PLANET) {            // settle on the surface
            d->vx *= 0.3f; d->vy *= 0.3f; d->av *= 0.4f;
            float nx = d->x / r, ny = d->y / r;
            d->x = nx * (R_PLANET + 1); d->y = ny * (R_PLANET + 1);
        }
        d->life--;
    }
}

static void exhaust(float dt_) {
    if (!thrusting || cur_thrust() <= 0) return;
    float fx, fy; nose_dir(ang, &fx, &fy);
    float bx = sx - fx * vis_half(), by = sy - fy * vis_half();
    for (int n = 0; n < 3; n++) {
        float spread = rnd_float_between(-22, 22);
        float ex = -fx, ey = -fy;
        // rotate exhaust dir by spread degrees
        float c = cos_deg(spread), s = sin_deg(spread);
        float dx = ex * c - ey * s, dy = ex * s + ey * c;
        float sp = rnd_float_between(60, 130);
        int hot = rnd_float() < 0.6f;
        spawn_particle(bx, by, vx * 0.2f + dx * sp, vy * 0.2f + dy * sp,
                       rnd_between(6, 16),
                       hot ? (rnd_float() < 0.5f ? CLR_YELLOW : CLR_ORANGE)
                           : (rnd_float() < 0.5f ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY));
    }
}

// contact with a body (planet or Mun). returns 1 if it ended the flight.
// the "up" normal is radial from THAT body's centre, so the landing test works
// the same whether you're setting down on the planet or the Mun.
static int try_contact(float cx, float cy, float radius, int is_mun) {
    float dx = sx - cx, dy = sy - cy;
    float r = fsqrt(dx * dx + dy * dy);
    if (r > radius + vis_half() * 0.6f) return 0;
    float fx, fy; nose_dir(ang, &fx, &fy);
    float nx = dx / r, ny = dy / r;              // local "up" (radial from body)
    float fdotn = fx * nx + fy * ny;             // 1 = nose straight up
    float spd = fsqrt(vx * vx + vy * vy);
    float c = clamp(fdotn, -1, 1); float tilt = (1 - c) * 90;
    if (spd < LAND_SPEED && fdotn > cos_deg(LAND_TILT) && af(angVel) < 40) {
        phase = ST_WRECK; thrusting = 0;
        sx = cx + nx * (radius + vis_half()); sy = cy + ny * (radius + vis_half());
        vx = vy = 0;
        if (is_mun) {                            // set down on another world
            fate = "THE MUN. You landed on another world.";
            hit(72, INSTR_SINE, 6, 360); hit(79, INSTR_SINE, 5, 420);
        } else if (orbit_done) {                 // round trip: orbit → home alive
            fate = "Welcome home, Kerbonaut.";
            hit(67, INSTR_SINE, 6, 340); hit(72, INSTR_SINE, 5, 380);
        } else {
            fate = "Touchdown. (You... landed?)";
            hit(60, INSTR_SINE, 5, 200);
        }
    } else {
        explode(spd > 90 ? "High-velocity lithobraking."
              : tilt > 40 ? "Came in sideways." : "Rapid Unscheduled Disassembly.");
    }
    return 1;
}

static void update_fly(float dt_) {
    float fx, fy; nose_dir(ang, &fx, &fy);
    float m = mass();
    float T = cur_thrust();

    // gravity
    float ax, ay; grav_acc(sx, sy, &ax, &ay);
    // thrust
    if (thrusting && T > 0) {
        ax += fx * (T / m);
        ay += fy * (T / m);
        st[activeStage].fuel -= st[activeStage].burn * dt_;
        if (st[activeStage].fuel < 0) st[activeStage].fuel = 0;
    }
    // integrate (semi-implicit Euler — same as the prediction, so the dots match)
    vx += ax * dt_; vy += ay * dt_;
    sx += vx * dt_; sy += vy * dt_;

    // --- attitude --------------------------------------------------------------
    // player steering
    if (key(KEY_LEFT)  || btn(0, BTN_LEFT))  angVel -= CTRL_AUTH * dt_;
    if (key(KEY_RIGHT) || btn(0, BTN_RIGHT)) angVel += CTRL_AUTH * dt_;
    // aerodynamics: signed angle between nose and velocity
    float spd = fsqrt(vx * vx + vy * vy);
    if (spd > 5) {
        float vdx = vx / spd, vdy = vy / spd;
        float cross = fx * vdy - fy * vdx;   // sin of (nose → velocity)
        float aero = spd * 0.5f;
        if (any_fins()) {
            angVel += cross * STAB * aero;                 // weathervane toward velocity
        } else {
            angVel -= cross * INSTAB * aero;               // any tilt grows — runaway
            angVel += 0.05f * aero;                        // a constant lean seeds the tumble
        }
    }
    angVel -= angVel * ANG_DAMP * dt_;
    ang += angVel * dt_;
    if (ang < 0) ang += 360; else if (ang >= 360) ang -= 360;

    // --- bookkeeping -----------------------------------------------------------
    float r = fsqrt(sx * sx + sy * sy);
    float alt = r - R_PLANET;
    if (alt > maxAlt) maxAlt = alt;
    if (spd > maxSpeed) maxSpeed = spd;
    tAloft += dt_;

    // contact with either body (planet first, then the Mun)
    if (try_contact(0, 0, R_PLANET, 0)) return;
    if (try_contact(MUN_X, MUN_Y, R_MUN, 1)) return;
    if (r > DESPAWN_R) { phase = ST_WRECK; fate = "Drifted out of range."; return; }

    // engine sound while burning
    if (thrusting && T > 0) {
        t_thrust_snd -= dt_;
        if (t_thrust_snd <= 0) { hit(30, INSTR_NOISE, 4, 90); t_thrust_snd = 0.07f; }
        if (alt < 30) shake(0.6f);
    }
    exhaust(dt_);
}

static void update_pad(float dt_) {
    float twr = cur_thrust() / (mass() * g0());
    if (thrusting && cur_thrust() > 0) {
        // straining on the pad: rumble + smoke regardless
        t_thrust_snd -= dt_;
        if (t_thrust_snd <= 0) { hit(twr > 1 ? 30 : 26, INSTR_NOISE, 4, 90); t_thrust_snd = 0.06f; }
        exhaust(dt_);
        shake(twr > 1 ? 0.5f : 0.9f);   // a doomed rocket just shudders harder
        if (twr > 1) {                  // liftoff!
            phase = ST_FLY;
            vy = -2;                     // a nudge off the pad
        }
    }
}

void update(void) {
    handle_input();
    if (is_paused) return;
    float dt_ = dt(); if (dt_ > 0.05f) dt_ = 0.05f;

    if (phase == ST_PAD) update_pad(dt_);
    else if (phase == ST_FLY) update_fly(dt_);

    tick_particles(dt_);

    if (phase == ST_FLY) {
        predict();
        // celebrate the first time the path is a closed loop clear of the ground
        if (!orbit_done && predEllipse && predPeri > R_PLANET + 6) {
            orbit_done = 1;
            hit(72, INSTR_SINE, 5, 300); hit(76, INSTR_SINE, 4, 320); hit(79, INSTR_SINE, 4, 360);
        }
        if (!escaped && predEscape) { escaped = 1; hit(55, INSTR_SINE, 4, 400); }
        // the new transcendent beat: the coast now reaches all the way to the Mun
        if (!mun_enc && predMunEnc) {
            mun_enc = 1;
            hit(67, INSTR_SINE, 5, 320); hit(71, INSTR_SINE, 4, 360); hit(74, INSTR_SINE, 4, 380);
        }
        // the milestone above the encounter: the coast now CLOSES around the Mun
        if (!mun_orbit && predMunOrbit) {
            mun_orbit = 1;
            hit(74, INSTR_SINE, 5, 320); hit(78, INSTR_SINE, 4, 360); hit(81, INSTR_SINE, 4, 420);
        }
    } else predN = 0;

#ifdef DE_TRACE
    float r = fsqrt(sx * sx + sy * sy);
    watch("phase", "%d", phase);
    watch("alt", "%.0f", r - R_PLANET);
    watch("twr", "%.2f", cur_thrust() / (mass() * g0()));
    watch("peri", "%.0f", predPeri - R_PLANET);
    watch("apo", "%.0f", predApo - R_PLANET);
    watch("ang", "%.0f", ang);
    watch("angvel", "%.0f", angVel);
    watch("munclose", "%.0f", predMunClosest - R_MUN);
    watch("munenc", "%d", predMunEnc);
    watch("munorbit", "%d", predMunOrbit);
    watch("munperi", "%.0f", predMunOrbit ? predMunPeri - R_MUN : -1.0f);
#endif
}

// ── camera: frame the planet and the whole orbit ────────────────────────────
static void update_camera(void) {
    // the extent we need to see = the larger of where we are and our apoapsis
    float r = fsqrt(sx * sx + sy * sy);
    float ext = r;
    if (phase == ST_FLY && predN > 1) { if (predApo > ext) ext = predApo; }
    // once the coast reaches the Mun, pull back far enough to keep it in frame
    if (phase == ST_FLY && predMunEnc) {
        float dm = fsqrt((float)MUN_X * MUN_X + (float)MUN_Y * MUN_Y) + R_MUN + 30;
        if (dm > ext) ext = dm;
    }
    if (ext < R_PLANET + 40) ext = R_PLANET + 40;

    float tz = (SCREEN_H * 0.42f) / ext;          // fit the extent to ~84% of height
    tz = clamp(tz, 0.10f, 1.6f);
    // centre between the planet (0,0) and the ship, biased toward the ship up close
    float cw = clamp(tz - 0.2f, 0, 1);            // zoomed in → follow the ship
    float tx = lerp(0, sx, cw) - SCREEN_W / 2.0f;
    float ty = lerp(0, sy, cw) - SCREEN_H / 2.0f;

    cam_x = lerp(cam_x, tx, 0.12f);
    cam_y = lerp(cam_y, ty, 0.12f);
    cam_zoom = lerp(cam_zoom, tz, 0.08f);
}

// ── draw helpers ─────────────────────────────────────────────────────────────
// rotate local (along nose, side right) → world
static float g_fx, g_fy, g_rx, g_ry;
static void set_frame(float a) {
    nose_dir(a, &g_fx, &g_fy);
    g_rx = -g_fy; g_ry = g_fx;        // right = nose rotated +90°
}
static void L2W(float along, float side, int *wx, int *wy) {
    *wx = (int)(sx + g_fx * along + g_rx * side);
    *wy = (int)(sy + g_fy * along + g_ry * side);
}

static void draw_rocket(void) {
    set_frame(ang);
    float h = vis_half();
    float w = 3.0f;
    int xy[8];
    // body tube
    int x0, y0, x1, y1, x2, y2, x3, y3;
    L2W(h - 4, -w, &x0, &y0); L2W(h - 4, w, &x1, &y1);
    L2W(-h + 3, w, &x2, &y2); L2W(-h + 3, -w, &x3, &y3);
    xy[0] = x0; xy[1] = y0; xy[2] = x1; xy[3] = y1;
    xy[4] = x2; xy[5] = y2; xy[6] = x3; xy[7] = y3;
    polyfill(xy, 4, CLR_LIGHT_GREY);
    // nose cone
    int nxp, nyp, b0x, b0y, b1x, b1y;
    L2W(h, 0, &nxp, &nyp);
    L2W(h - 4, -w, &b0x, &b0y); L2W(h - 4, w, &b1x, &b1y);
    trifill(nxp, nyp, b0x, b0y, b1x, b1y, CLR_RED);
    // fins (only if the live rocket has them)
    if (any_fins()) {
        int fa, fb, fc, fd, fe, ff;
        int gx, gy;
        L2W(-h + 4, w, &fa, &fb); L2W(-h + 3, w + 3, &fc, &fd); L2W(-h + 1, w, &fe, &ff);
        trifill(fa, fb, fc, fd, fe, ff, CLR_MEDIUM_GREY);
        L2W(-h + 4, -w, &fa, &fb); L2W(-h + 3, -w - 3, &fc, &fd); L2W(-h + 1, -w, &fe, &ff);
        trifill(fa, fb, fc, fd, fe, ff, CLR_MEDIUM_GREY);
        (void)gx; (void)gy;
    }
    // nozzle
    L2W(-h + 3, -w + 1, &x0, &y0); L2W(-h + 3, w - 1, &x1, &y1);
    L2W(-h, w, &x2, &y2); L2W(-h, -w, &x3, &y3);
    xy[0] = x0; xy[1] = y0; xy[2] = x1; xy[3] = y1;
    xy[4] = x2; xy[5] = y2; xy[6] = x3; xy[7] = y3;
    polyfill(xy, 4, CLR_DARK_GREY);
}

static void draw_planet(void) {
    // faint atmosphere halo
    circfill(0, 0, (int)(R_PLANET + 8), CLR_DARKER_BLUE);
    circfill(0, 0, (int)(R_PLANET + 3), CLR_DARK_BLUE);
    circfill(0, 0, (int)R_PLANET, CLR_DARK_GREEN);
    // a couple of land blobs + a brighter lit edge toward the top-left
    circfill((int)(-R_PLANET * 0.3f), (int)(-R_PLANET * 0.25f), (int)(R_PLANET * 0.45f), CLR_MEDIUM_GREEN);
    circfill((int)(R_PLANET * 0.35f), (int)(R_PLANET * 0.2f), (int)(R_PLANET * 0.3f), CLR_GREEN);
    circ(0, 0, (int)R_PLANET, CLR_LIME_GREEN);
    // launch pad marker at the top
    line(-4, (int)(-R_PLANET - 1), 4, (int)(-R_PLANET - 1), CLR_LIGHT_GREY);
}

static void draw_mun(void) {
    int mx = (int)MUN_X, my = (int)MUN_Y;
    circfill(mx, my, (int)R_MUN, CLR_MEDIUM_GREY);
    // lit edge toward the planet-ward side + a couple of craters
    circfill(mx - (int)(R_MUN * 0.3f), my - (int)(R_MUN * 0.25f), (int)(R_MUN * 0.42f), CLR_LIGHT_GREY);
    circ(mx + (int)(R_MUN * 0.25f), my + (int)(R_MUN * 0.10f), (int)(R_MUN * 0.18f), CLR_DARK_GREY);
    circ(mx - (int)(R_MUN * 0.35f), my + (int)(R_MUN * 0.35f), (int)(R_MUN * 0.12f), CLR_DARK_GREY);
    circ(mx, my, (int)R_MUN, CLR_LIGHT_GREY);
}

static void draw_marker(float r, int up, int col, const char *label) {
    // place the marker along the current radial-to-apsis is hard; instead drop it
    // where the predicted path reaches that distance — found in draw_path. Here we
    // just draw a small triangle + label at a world point passed via globals.
    (void)r; (void)up; (void)col; (void)label;
}

// remembered screen-ish world points of the apo/peri samples (filled in draw_path)
static float apoWX, apoWY, periWX, periWY; static int haveApo, havePeri;

static void draw_path(void) {
    if (predN < 2) return;
    haveApo = havePeri = 0;
    float bestApo = -1, bestPeri = 1e9f;
    for (int i = 0; i < predN; i++) {
        float r = fsqrt(predPx[i] * predPx[i] + predPy[i] * predPy[i]);
        if (r > bestApo)  { bestApo = r;  apoWX = predPx[i];  apoWY = predPy[i];  haveApo = 1; }
        if (r < bestPeri) { bestPeri = r; periWX = predPx[i]; periWY = predPy[i]; havePeri = 1; }
        int col = predCrash ? CLR_RED : CLR_LIGHT_GREY;   // red = you'll hit the ground
        pset((int)predPx[i], (int)predPy[i], col);
    }
    // apoapsis ▲ / periapsis ▼ — only meaningful when we don't just crash
    if (!predCrash) {
        if (haveApo)  circfill((int)apoWX, (int)apoWY, 2, CLR_LIGHT_PEACH);
        if (havePeri) circfill((int)periWX, (int)periWY, 2, CLR_TRUE_BLUE);
    }
}

static void draw_particles(void) {
    for (int i = 0; i < MAXP; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0) continue;
        int col = p->col;
        // smoke fades grey→dark as it dies
        if ((col == CLR_LIGHT_GREY || col == CLR_MEDIUM_GREY) && p->life < p->life0 / 2)
            col = CLR_DARK_GREY;
        pset((int)p->x, (int)p->y, col);
    }
    for (int i = 0; i < MAXD; i++) {
        Debris *d = &debris[i];
        if (d->life <= 0) continue;
        // a tumbling little bar
        float c = cos_deg(d->a), s = sin_deg(d->a);
        int x0 = (int)(d->x - c * d->len / 2), y0 = (int)(d->y - s * d->len / 2);
        int x1 = (int)(d->x + c * d->len / 2), y1 = (int)(d->y + s * d->len / 2);
        line(x0, y0, x1, y1, d->col);
    }
}

// ── HUD (screen space) ──────────────────────────────────────────────────────
static void hud(void) {
    char buf[64];
    float r = fsqrt(sx * sx + sy * sy);
    float alt = r - R_PLANET;
    float spd = fsqrt(vx * vx + vy * vy);
    float m = mass();
    float twr = cur_thrust() / (m * g0());

    print(design_name, 4, 4, CLR_WHITE);

    snprintf(buf, sizeof buf, "ALT %5.0f m", alt < 0 ? 0 : alt);
    print(buf, 4, 14, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "VEL %5.0f", spd);
    print(buf, 4, 22, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "TWR %4.2f", twr);
    print(buf, 4, 30, twr >= 1 ? CLR_GREEN : CLR_RED);

    // fuel bar of the active stage
    if (activeStage < nstages) {
        float f = st[activeStage].fuelMax > 0 ? st[activeStage].fuel / st[activeStage].fuelMax : 0;
        print("FUEL", 4, 40, CLR_LIGHT_GREY);
        bar(34, 40, 40, 5, f, f > 0.25f ? CLR_YELLOW : CLR_RED, CLR_DARK_GREY);
    }
    snprintf(buf, sizeof buf, "STAGE %d/%d", activeStage + 1, nstages);
    print(buf, 4, 48, CLR_LIGHT_GREY);

    // apoapsis / periapsis readout — the orbit, in two numbers
    if (phase == ST_FLY && predN > 1 && !predCrash) {
        snprintf(buf, sizeof buf, "APO %5.0f", predApo - R_PLANET);
        print(buf, SCREEN_W - 72, 14, CLR_LIGHT_PEACH);
        snprintf(buf, sizeof buf, "PERI %4.0f", predPeri - R_PLANET);
        print(buf, SCREEN_W - 72, 22, CLR_TRUE_BLUE);
    }
    if (phase == ST_FLY && predMunOrbit) {
        snprintf(buf, sizeof buf, "M-APO %4.0f", predMunApo - R_MUN);
        print(buf, SCREEN_W - 72, 30, CLR_MEDIUM_GREY);
        snprintf(buf, sizeof buf, "M-PERI %3.0f", predMunPeri - R_MUN);
        print(buf, SCREEN_W - 72, 38, CLR_MEDIUM_GREY);
    } else if (phase == ST_FLY && predMunEnc) {
        snprintf(buf, sizeof buf, "MUN %5.0f", predMunClosest - R_MUN);
        print(buf, SCREEN_W - 72, 30, CLR_MEDIUM_GREY);
    }

    // status banners
    if (phase == ST_PAD)
        print("HOLD Z TO LAUNCH", SCREEN_W / 2 - 60, SCREEN_H - 14, CLR_YELLOW);
    if (orbit_done && phase == ST_FLY)
        print("ORBIT ACHIEVED", SCREEN_W / 2 - 52, 6, CLR_GREEN);
    else if (escaped && phase == ST_FLY)
        print("ESCAPE TRAJECTORY", SCREEN_W / 2 - 64, 6, CLR_LIGHT_PEACH);
    if (mun_orbit && phase == ST_FLY)
        print("MUN ORBIT", SCREEN_W / 2 - 36, 14, CLR_GREEN);
    else if (mun_enc && phase == ST_FLY)
        print("MUN ENCOUNTER", SCREEN_W / 2 - 52, 14, CLR_LIGHT_PEACH);

    if (phase == ST_WRECK) {
        int bx = SCREEN_W / 2 - 80, by = SCREEN_H / 2 - 34;
        rectfill(bx, by, 160, 64, CLR_DARKER_BLUE);
        rect(bx, by, 160, 64, CLR_LIGHT_GREY);
        print("FLIGHT RECORDER", bx + 30, by + 5, CLR_WHITE);
        snprintf(buf, sizeof buf, "APOAPSIS  %6.0f m", maxAlt);   print(buf, bx + 10, by + 17, CLR_LIGHT_GREY);
        snprintf(buf, sizeof buf, "MAX VEL   %6.0f",   maxSpeed); print(buf, bx + 10, by + 25, CLR_LIGHT_GREY);
        snprintf(buf, sizeof buf, "ALOFT     %6.1f s", tAloft);   print(buf, bx + 10, by + 33, CLR_LIGHT_GREY);
        print(fate ? fate : "", bx + 10, by + 43, CLR_ORANGE);
        print("[SPACE] relaunch", bx + 30, by + 53, CLR_YELLOW);
    }

    if (is_paused) print("PAUSED", SCREEN_W / 2 - 22, SCREEN_H / 2, CLR_WHITE);
}

void draw(void) {
    update_camera();

    cls(CLR_BLACK);
    // starfield in screen space (stable while the world pans)
    for (int i = 0; i < MAXSTAR; i++)
        pset((int)stars[i].x, (int)stars[i].y, stars[i].col);

    camera_ex((int)cam_x, (int)cam_y, cam_zoom, 0);

    draw_planet();
    draw_mun();
    if (phase == ST_FLY) draw_path();
    draw_particles();
    if (phase != ST_WRECK) draw_rocket();

    camera(0, 0);
    hud();
}

void init(void) {
    // a static starfield laid out across the screen
    for (int i = 0; i < MAXSTAR; i++) {
        stars[i].x = rnd_between(0, SCREEN_W);
        stars[i].y = rnd_between(0, SCREEN_H);
        float b = rnd_float();
        stars[i].col = b < 0.6f ? CLR_DARK_GREY : b < 0.9f ? CLR_LIGHT_GREY : CLR_WHITE;
    }
    reset_pad();
    cam_zoom = (SCREEN_H * 0.42f) / (R_PLANET + 40);
    cam_x = sx - SCREEN_W / 2.0f;
    cam_y = sy - SCREEN_H / 2.0f;
}
