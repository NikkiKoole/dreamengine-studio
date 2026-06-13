#include "studio.h"
#include <stdio.h>

// ============================================================================
// COASTER & SLIDE BUILDER  —  draw a track, then watch it run.
//
// A port of an old LÖVE toy ("achtbaan") into dreamengine. One physics core,
// two modes you flip with M:
//
//   COASTER (closed loop) — a coupled train runs forever around the loop. It
//                           bleeds speed to friction, so give it a HOIST (chain
//                           lift) or a BOOST zone to top its energy back up.
//   SLIDE   (open flume)  — riders flow from the top down to the open end,
//                           splash, and ride again. Pure gravity, no loop.
//
// DRAW:   click + drag in empty space to lay a track.
// EDIT:   click a point and drag it to reshape the track.
// ZONES:  hold a key WHILE drawing (or hover + tap after) to paint a stretch:
//            B = boost (green)    S = slow/brake (red)    H = hoist (yellow)
// CARTS:  up / down arrows  —  longer / shorter train (coaster mode).
// M = swap coaster/slide      SPACE = pause      N = new/clear      ? / TAB = help
// ============================================================================

// ── tuning ──────────────────────────────────────────────────────────────────
#define MAX_PTS        800
#define MIN_DIST       7.0f     // px between sampled track points
#define SUPPORT_SPACING 38.0f   // px of arc between support beams
#define MIN_BEAM_GAP   14.0f    // merge beams closer than this horizontally
#define GROUND_PAD     16       // ground sits this far above the bottom edge

#define GRAV     180.0f         // px/s^2 of gravity pull along the track tangent
#define FRICTION 0.020f         // linear drag
#define AIRDRAG  0.00040f       // v^2 drag
#define MAXV     900.0f         // speed clamp px/s
#define HOIST_SP 64.0f          // chain-lift speed px/s
#define BOOST_A  520.0f         // boost accel px/s^2
#define BRAKE_A  620.0f         // brake decel px/s^2

#define MAX_BODY 28             // train length cap / slide rider pool
#define CART_GAP 11.0f          // arc spacing between coupled carts
#define RIDE_GAP 26             // frames between slide rider releases
#define MAXP     140            // particle pool
#define FLIP_OFF 3.0f           // px a body rides off-centre; a flip point swaps the side
#define RIDE_ZOOM 2.4f          // POV ride-cam zoom factor

// riders — 2 per cart (front/back), each a fixed body + two 2-segment verlet arms
#define CHARS_PER_CART 2
#define CHAR_OFF 2.6f           // char distance from cart centre along the tangent
#define BODY_W   1.4f           // torso half-width (along tangent); shoulders sit at the top corners
#define BODY_H   3.0f           // torso height (toward body-up); = shoulder height
#define ARM_UP   3.4f           // upper-arm rest length (shoulder→elbow)
#define ARM_LO   3.0f           // lower-arm rest length (elbow→hand)
#define ARM_GRAV 0.30f          // world gravity pulling the free arm points down
#define ARM_DAMP 0.90f          // verlet velocity damping (arms settle)

// ── zones ─────────────────────────────────────────────────────────────────
// zone is one-of (boost/brake/hoist); flip is an orthogonal flag — a point can
// be both a boost AND a flip point, just like the original achtbaan.
enum { Z_NONE = 0, Z_BOOST, Z_BRAKE, Z_HOIST };

typedef struct { float x, y; unsigned char zone, flip; } Pt;
static Pt    pts[MAX_PTS];
static int   n_pts;
static float seglen[MAX_PTS];   // arc length of segment i -> i+1
static float total_len;
static int   closed;            // 1 = coaster loop, 0 = open slide
static int   wood = 1;          // construction: 1 = wooden trestle, 0 = tubular steel
static float flip_pos[MAX_PTS]; // arc positions of flip points (for crossing tests)
static int   n_flip;

// beams
static float beam_x[MAX_PTS], beam_y[MAX_PTS];
static int   n_beams;

// bodies (train carts in coaster mode, riders in slide mode)
typedef struct {
    float pos, vel;             // arc position + speed along track
    float x, y, ux, uy;         // cached world pos + unit tangent
    int   wait;                 // slide: frames until release (<=0 = riding)
    int   hue;                  // body colour
    float offset_dir;           // +1/-1: which side of the track this body rides
} Body;
static Body bodies[MAX_BODY];
static int  n_bodies = 10;

// a rider's arm: 3 verlet points — [0] shoulder (pinned to the cart each frame),
// [1] elbow, [2] hand (free). px/py current, ox/oy previous (velocity = the diff).
typedef struct { float px[3], py[3], ox[3], oy[3]; } VArm;
typedef struct {
    VArm  arm[2];               // two arms, offset fore/aft so both show in profile
    float tension;              // 0 = loose flailer .. 1 = tense clutcher (personality)
    int   clothes;              // body colour
} Rider;
static Rider riders[MAX_BODY][CHARS_PER_CART];
static int   riders_primed = 0; // arms snapped to a start pose yet?

// particles
typedef struct { float x, y, vx, vy; int life, col; } Part;
static Part parts[MAXP];

// interaction
static int   drawing = 0;
static int   drag_idx = -1;
static int   is_paused = 0;
static int   show_help = 0;          // hidden behind the on-screen ? button
static float run_started = 0;

// the ? help toggle button, bottom-right corner
#define HELP_BW 13
#define HELP_BH 13
#define HELP_BX (SCREEN_W - HELP_BW - 4)
#define HELP_BY (SCREEN_H - HELP_BH - 4)

// POV ride-cam — smoothed camera state, eased toward its target every frame
static int   ride = 0;
static float cam_x, cam_y, cam_zoom = 1, cam_angle;

// g-force felt by the riders (1 = rest) + the crowd scream tied to it
#define SCREAM_VOICES 3                 // a few different voices, not one soloist
static float gforce = 1, gpeak = 1;     // total g-load magnitude + the lap's peak
static float g_long, g_norm;            // load split along travel / across the seat (g)
static float fgx = 0, fgy = 1;          // smoothed load VECTOR in g's (rest = (0,1))
static float excite = 0;                // jerk-fed excitement envelope → drives the scream
static float prev_vel, prev_g = 1;      // last frame's speed / g-load (for accel + jerk)
static int   warmup = 0;                // frames to ignore jerk after a (re)start
static int   scream_h[SCREAM_VOICES] = { -1, -1, -1 };   // held note handles (-1 = silent)
static const int SCREAM_OFF[SCREAM_VOICES] = { -3, 1, 6 }; // pitch spread → a crowd, not unison

static int ground_y(void) { return SCREEN_H - GROUND_PAD; }

// ── particles ───────────────────────────────────────────────────────────────
static void spawn(float x, float y, float vx, float vy, int life, int col) {
    for (int i = 0; i < MAXP; i++)
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, vx, vy, life, col };
            return;
        }
}
static void splash(float x, float y, int n, int col) {
    for (int k = 0; k < n; k++)
        spawn(x, y, rnd_float_between(-40, 40), rnd_float_between(-90, -10),
              rnd_between(10, 22), col);
}
static void tick_parts(float dt) {
    for (int i = 0; i < MAXP; i++) {
        Part *p = &parts[i];
        if (p->life <= 0) continue;
        p->x += p->vx * dt; p->y += p->vy * dt;
        p->vy += 220 * dt;          // gravity on spray
        p->life--;
        if (p->y > ground_y()) p->life = 0;
    }
}

// ── track sampling ──────────────────────────────────────────────────────────
static int n_segs(void) { return closed ? n_pts : n_pts - 1; }

static void recompute(void) {
    total_len = 0;
    int segs = n_segs();
    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % n_pts;
        seglen[i] = distance((int)pts[i].x, (int)pts[i].y, (int)pts[j].x, (int)pts[j].y);
        total_len += seglen[i];
    }
    // arc position of every flip point, so we can test crossings by distance
    n_flip = 0;
    float acc = 0;
    for (int i = 0; i < n_pts; i++) {
        if (pts[i].flip) flip_pos[n_flip++] = acc;
        if (i < segs) acc += seglen[i];
    }
    // support beams: one every SUPPORT_SPACING of arc, then merge near ones
    n_beams = 0;
    if (total_len > 0) {
        int nb = (int)(total_len / SUPPORT_SPACING);
        float tx[MAX_PTS]; float ty[MAX_PTS]; int nt = 0;
        for (int b = 1; b <= nb; b++) {
            float target = b * SUPPORT_SPACING, acc = 0;
            for (int i = 0; i < segs; i++) {
                if (acc + seglen[i] >= target) {
                    float t = seglen[i] > 0 ? (target - acc) / seglen[i] : 0;
                    int j = (i + 1) % n_pts;
                    tx[nt] = pts[i].x + (pts[j].x - pts[i].x) * t;
                    ty[nt] = pts[i].y + (pts[j].y - pts[i].y) * t;
                    nt++;
                    break;
                }
                acc += seglen[i];
            }
        }
        int i = 0;
        while (i < nt) {
            float bx = tx[i], by = ty[i];     // keep the highest in each cluster
            int j = i + 1;
            while (j < nt && (tx[j] - tx[i] < MIN_BEAM_GAP) && (tx[j] - tx[i] > -MIN_BEAM_GAP)) {
                if (ty[j] < by) { bx = tx[j]; by = ty[j]; }
                j++;
            }
            beam_x[n_beams] = bx; beam_y[n_beams] = by; n_beams++;
            i = j;
        }
    }
}

// sample world position + unit tangent at arc distance d
static void sample(float d, float *ox, float *oy, int *oseg, float *ux, float *uy) {
    int segs = n_segs();
    if (closed) { while (d < 0) d += total_len; while (d >= total_len) d -= total_len; }
    else        { if (d < 0) d = 0; if (d > total_len) d = total_len; }
    float acc = 0;
    for (int i = 0; i < segs; i++) {
        float L = seglen[i];
        if (acc + L >= d || i == segs - 1) {
            float t = L > 0 ? (d - acc) / L : 0;
            int j = (i + 1) % n_pts;
            float dx = pts[j].x - pts[i].x, dy = pts[j].y - pts[i].y;
            *ox = pts[i].x + dx * t;
            *oy = pts[i].y + dy * t;
            if (L > 0) { *ux = dx / L; *uy = dy / L; } else { *ux = 1; *uy = 0; }
            *oseg = i;
            return;
        }
        acc += L;
    }
    *ox = pts[0].x; *oy = pts[0].y; *ux = 1; *uy = 0; *oseg = 0;
}

// ── bodies ──────────────────────────────────────────────────────────────────
static const int CLOTHES[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
                               CLR_BLUE, CLR_PINK, CLR_WHITE, CLR_INDIGO };

static void init_bodies(void) {
    for (int i = 0; i < MAX_BODY; i++) {
        bodies[i].pos = 0; bodies[i].vel = 0;
        bodies[i].wait = closed ? 0 : i * RIDE_GAP;   // slide: stagger releases
        bodies[i].hue = (i * 5) % 6;
        bodies[i].offset_dir = 1;
        for (int c = 0; c < CHARS_PER_CART; c++) {    // a varied crowd
            riders[i][c].tension = rnd_float();
            riders[i][c].clothes = CLOTHES[rnd(8)];
        }
    }
    riders_primed = 0;
    warmup = 20; prev_g = 1; gpeak = 1;               // let g settle before screaming
    if (closed && n_bodies > 0) bodies[0].vel = 40;   // nudge the train off
}

// A body normally rides FLIP_OFF px to one side of the centre line; crossing a
// flip point swaps the side (that's what makes a corkscrew look right). We test
// crossings by the ARC the body swept this frame (d = signed px moved), NOT the
// segment it landed on — so a cart that leaps over a flip point in one fast
// frame still flips. Toggles once per flip point in the swept interval.
static int flip_cross(Body *b, float d) {
    if (d == 0 || total_len <= 0) return 0;
    float ad = d > 0 ? d : -d;
    float start = b->pos - d;                          // where the body was last frame
    int crossings = 0;
    for (int i = 0; i < n_flip; i++) {
        float rel = d > 0 ? flip_pos[i] - start : start - flip_pos[i];
        while (rel < 0) rel += total_len;
        while (rel >= total_len) rel -= total_len;
        if (rel > 0 && rel <= ad) { b->offset_dir = -b->offset_dir; crossings++; }
    }
    return crossings;
}

// shift a body to its riding side; call after sample() set b->ux/b->uy
static void apply_offset(Body *b) {
    float nx = b->uy, ny = -b->ux;                     // segment normal
    b->x += nx * FLIP_OFF * b->offset_dir;
    b->y += ny * FLIP_OFF * b->offset_dir;
}

// ── rider arms (verlet) ──────────────────────────────────────────────────────
// Each arm is a tiny pinned rope (rope.c's trick): the shoulder is pinned to the
// cart, the elbow + hand are free. World-down gravity makes them hang; the free
// points keep their world velocity, so when the cart yanks the shoulder they lag
// and whip — gravity hang + inertial fling for free. A "muscle" spring then pulls
// the hand toward a target: on the hoist everyone throws their arms UP (raised by
// 1-tension), elsewhere tense riders pull their arms in (loose ones just flail).
static void arm_step(VArm *a, float sx, float sy, float tx, float ty, float muscle) {
    a->px[0] = a->ox[0] = sx; a->py[0] = a->oy[0] = sy;     // pin shoulder
    for (int i = 1; i <= 2; i++) {                          // verlet the free points
        float vx = (a->px[i] - a->ox[i]) * ARM_DAMP;
        float vy = (a->py[i] - a->oy[i]) * ARM_DAMP;
        a->ox[i] = a->px[i]; a->oy[i] = a->py[i];
        a->px[i] += vx; a->py[i] += vy + ARM_GRAV;
    }
    a->px[2] += (tx - a->px[2]) * muscle;                   // muscle pulls the hand
    a->py[2] += (ty - a->py[2]) * muscle;
    for (int it = 0; it < 4; it++) {                        // relax segment lengths
        float dx = a->px[1] - a->px[0], dy = a->py[1] - a->py[0];
        float d = fsqrt(dx * dx + dy * dy); if (d < 0.001f) d = 0.001f;
        float k = (ARM_UP - d) / d;                         // shoulder pinned → move elbow
        a->px[1] += dx * k; a->py[1] += dy * k;
        dx = a->px[2] - a->px[1]; dy = a->py[2] - a->py[1];
        d = fsqrt(dx * dx + dy * dy); if (d < 0.001f) d = 0.001f;
        k = (ARM_LO - d) / d * 0.5f;
        a->px[1] -= dx * k; a->py[1] -= dy * k;
        a->px[2] += dx * k; a->py[2] += dy * k;
    }
}

// drive both riders' arms on one cart for this frame
static void step_arms(Body *b, int ci) {
    float nx = b->uy, ny = -b->ux;                          // body-up = track normal
    for (int c = 0; c < CHARS_PER_CART; c++) {
        float fb = (c == 0) ? 1.0f : -1.0f;                 // front / back along tangent
        float cx = b->x + b->ux * CHAR_OFF * fb;
        float cy = b->y + b->uy * CHAR_OFF * fb;
        Rider *r = &riders[ci][c];
        for (int arm = 0; arm < 2; arm++) {
            float fa = (arm == 0) ? BODY_W : -BODY_W;       // top corners of the torso
            float shx = cx + b->ux * fa + nx * BODY_H;
            float shy = cy + b->uy * fa + ny * BODY_H;
            VArm *A = &r->arm[arm];
            if (!riders_primed) {                           // snap to a raised pose
                A->px[0] = A->ox[0] = shx;            A->py[0] = A->oy[0] = shy;
                A->px[1] = A->ox[1] = shx + nx * ARM_UP;          A->py[1] = A->oy[1] = shy + ny * ARM_UP;
                A->px[2] = A->ox[2] = shx + nx * (ARM_UP + ARM_LO); A->py[2] = A->oy[2] = shy + ny * (ARM_UP + ARM_LO);
            }
            // arms always WANT their personal pose — bold riders point straight up,
            // timid ones (high tension) blend toward the chest. The muscle is modest,
            // so it holds the pose in calm stretches but the cart's inertia overwhelms
            // it under heavy g (drops/loops/launches): forces take over, then the arms
            // drift back. No special-casing — the verlet does it.
            float reach = ARM_UP + ARM_LO;
            float upx = shx + nx * reach, upy = shy + ny * reach;
            float t = r->tension;
            float tx = lerp(upx, cx, t), ty = lerp(upy, cy, t);
            float muscle = lerp(0.16f, 0.28f, t);
            arm_step(A, shx, shy, tx, ty, muscle);
        }
    }
}

// ── ride sound ───────────────────────────────────────────────────────────────
// One primitive: the train clicks once per `spacing` px of track travelled, so
// the click RATE is the speed — no timers. Each zone picks the click's voice.
// The hoist's heavy chain-dog ratchet is the signature; plain track is a light
// wheel-joint tick. The `wood` toggle shifts the whole table lower+longer (a
// hollow timber clatter) vs higher (a brighter steel clack).
static float clack_accum;     // px travelled since the last click

// Map a WORLD point to a stereo pan from where it lands ON SCREEN, so a sound
// plays from its actual position: ride clacks and screams track the train as it
// sweeps the loop. Replays the camera transform (translate → zoom → rotate about
// the screen centre) on cam_* state; cam_* lag one frame here (they're eased in
// draw()), which is inaudible. 0 = screen centre, ±1 = the screen edges.
static float world_pan(float wx, float wy) {
    float c = cos_deg(cam_angle), s = sin_deg(cam_angle);
    float rx = (wx - (cam_x + SCREEN_W / 2.0f)) * cam_zoom;
    float ry = (wy - (cam_y + SCREEN_H / 2.0f)) * cam_zoom;
    float sx  = rx * c - ry * s + SCREEN_W / 2.0f;      // on-screen x
    return clamp((sx / SCREEN_W) * 2.0f - 1.0f, -1, 1); // 0..W → -1..+1
}

static void coaster_sound(int zone, float sp, float ad, float pan) {
    if (sp < 12) { clack_accum = 0; return; }          // parked → silent
    float spacing; int instr, midi, vol, dur;
    switch (zone) {
        case Z_HOIST: spacing = 7;  instr = INSTR_NOISE;  midi = 38;                vol = 5; dur = 26; break;  // percussive chain-dog clack, not a pitched tone
        case Z_BOOST: spacing = 5;  instr = INSTR_SAW;    midi = 46 + (int)(sp / 50); vol = 5; dur = 26; break;
        case Z_BRAKE: spacing = 4;  instr = INSTR_NOISE;  midi = 30;                vol = 4; dur = 55; break;
        default:      spacing = 15; instr = INSTR_NOISE;  midi = 50 + (int)(sp / 90); vol = 3; dur = 20; break;
    }
    if (wood) { midi -= 7; dur += 6; }                  // hollow wooden clatter — lower, longer
    else      { midi += 3; }                            // brighter steel clack
    clack_accum += ad;
    int fired = 0;
    while (clack_accum >= spacing && fired < 3) {       // cap: never dump a burst
        clack_accum -= spacing;
        instrument_pan(instr, pan);                     // clack plays from the train's screen position
        hit(midi, instr, vol, dur);
        fired++;
    }
    if (clack_accum > spacing) clack_accum = 0;         // outran the cap → resync
}

static void flip_sound(float pan) {                     // a quick "fwip" on inversion
    instrument_pan(INSTR_SINE,  pan);
    instrument_pan(INSTR_NOISE, pan);
    hit(57, INSTR_SINE,  5, 120);
    hit(43, INSTR_NOISE, 3, 70);
}

static void silence_scream(void) {
    for (int i = 0; i < SCREAM_VOICES; i++)
        if (scream_h[i] >= 0) { note_off(scream_h[i]); scream_h[i] = -1; }
}

// what the riders FEEL: net of (gravity − the cart's acceleration), projected onto
// the seat-down axis, in g units. 1 at rest; >1 heavy (fast valley); →0/negative is
// airtime (floaty crest). Cart accel = how fast the velocity vector turned/grew.
static void update_gforce(Body *lead, float dt) {
    if (dt <= 0) return;
    // Build the cart's acceleration from SMOOTH geometry, not a frame-to-frame
    // velocity diff (which spikes at every polyline vertex):
    //   • tangential a_t = change in speed (already smooth, from the physics)
    //   • centripetal a_n = v²·curvature, curvature from a wide arc stencil
    float a_t = (lead->vel - prev_vel) / dt;
    prev_vel = lead->vel;
    float D = 14, ax1, ay1, ax2, ay2, u1x, u1y, u2x, u2y; int s;
    sample(lead->pos - D, &ax1, &ay1, &s, &u1x, &u1y);
    sample(lead->pos + D, &ax2, &ay2, &s, &u2x, &u2y);
    float a1 = angle_to(0, 0, (int)(u1x * 512), (int)(u1y * 512));
    float a2 = angle_to(0, 0, (int)(u2x * 512), (int)(u2y * 512));
    float dth = a2 - a1; while (dth > 180) dth -= 360; while (dth < -180) dth += 360;
    float kappa = (dth * 0.0174533f) / (2 * D);         // signed curvature, 1/px
    float a_n = lead->vel * lead->vel * kappa;          // centripetal accel
    float tx = lead->ux, ty = lead->uy, nx = -lead->uy, ny = lead->ux;
    float acx = a_t * tx + a_n * nx, acy = a_t * ty + a_n * ny;
    // load VECTOR = (gravity − cart accel), in g's, smoothed
    float Fx = clamp(-acx / GRAV, -8, 8), Fy = clamp((GRAV - acy) / GRAV, -8, 8);
    fgx = lerp(fgx, Fx, 0.25f); fgy = lerp(fgy, Fy, 0.25f);
    // split into travel-axis (launch/brake) and seat-axis (valley/airtime)
    g_long = fgx * tx + fgy * ty;
    g_norm = fgx * nx + fgy * ny;
    gforce = fsqrt(fgx * fgx + fgy * fgy);
    if (gforce > gpeak) gpeak = gforce;
    // JERK = how fast the FELT load (its magnitude) is changing. A steady curve
    // holds a constant g, so it stays quiet; only a real jolt — slam into a
    // valley, drop to airtime, launch kick — spikes it. The jolt pumps `excite`,
    // which decays, so the scream fires on the transition and rings out.
    float jerk = (gforce - prev_g) / dt; if (jerk < 0) jerk = -jerk;
    prev_g = gforce;
    excite *= 0.84f;                                    // shorter ring-out
    if (warmup > 0) { warmup--; return; }               // ignore the settling transient
    float pump = clamp((jerk - 10.0f) * 0.06f, 0, 1);   // high bar → only the real jolts
    if (pump > excite) excite = pump;
}

static float scream_base[SCREAM_VOICES];   // rolled vowel floor per voice (the relaxed mouth shape)
static float scream_q[SCREAM_VOICES];      // rolled peak sharpness per voice

// roll a fresh vowel for a scream voice on its onset: a per-voice base (a U/O→A→E→I
// spread so the crowd isn't unison) jittered a little so no two screams match, plus a
// random peak sharpness. Stored — the vowel then OPENS with excitement in update_scream.
static void scream_vowel_roll(int i) {
    static const float base[SCREAM_VOICES] = { 0.30f, 0.45f, 0.60f };
    scream_base[i] = clamp(base[i] + rnd_float_between(-0.12f, 0.12f), 0.0f, 1.0f);
    scream_q[i]    = rnd_float_between(0.55f, 0.78f);
}

// the crowd scream rides the EXCITEMENT (driven by jerk): it kicks in on a jolt,
// glides its pitch with the intensity, and fades as `excite` decays. More voices
// join on bigger jolts — a little bump is one yelp, a big plunge is the whole car.
static void update_scream(float sp, float pan) {
    int want = 0;
    if (excite > 0.18f && sp > 50)
        want = 1 + (excite > 0.35f) + (excite > 0.55f);  // 1..3 voices by intensity
    float midi = 69 + excite * 15;
    int vol = 1 + (int)(excite * 1.2f); if (vol > 2) vol = 2;   // soft; layering carries it
    for (int i = 0; i < SCREAM_VOICES; i++) {
        if (i < want) {
            float m = midi + SCREAM_OFF[i];
            if (scream_h[i] < 0) { scream_vowel_roll(i); scream_h[i] = note_on((int)m, 5 + i, vol); note_glide(scream_h[i], 130); }
            else { note_pitch(scream_h[i], m); note_vol(scream_h[i], vol); }
            // the vowel OPENS as excitement climbs — a rider goes "ah… AAAH/eee" the
            // scarier it gets. Safe to re-push per frame: formant is a coefficient update.
            float vowel = clamp(scream_base[i] + excite * 0.45f, 0.0f, 1.0f);
            instrument_formant(5 + i, vowel, scream_q[i], 0.9f);
            note_pan(scream_h[i], pan);                  // the crowd screams from the train's screen position
        } else if (scream_h[i] >= 0) {
            note_off(scream_h[i]); scream_h[i] = -1;
        }
    }
}

static void integrate(Body *b, float dt) {
    int seg; sample(b->pos, &b->x, &b->y, &seg, &b->ux, &b->uy);
    int zone = pts[seg].zone;
    if (zone == Z_HOIST) {
        b->vel = HOIST_SP;
    } else {
        float v = b->vel, av = v < 0 ? -v : v;
        float a = GRAV * b->uy - FRICTION * v - AIRDRAG * v * av;
        if (zone == Z_BOOST) a += BOOST_A;
        if (zone == Z_BRAKE) a -= BRAKE_A * (v > 0 ? 1 : v < 0 ? -1 : 0);
        float pv = v;
        b->vel += a * dt;
        // don't let drag/brake push the cart backwards through zero
        if (zone != Z_BOOST) {
            if (pv > 0 && b->vel < 0 && a < 0) b->vel = 0;
            if (pv < 0 && b->vel > 0 && a > 0) b->vel = 0;
        }
    }
    if (b->vel >  MAXV) b->vel =  MAXV;
    if (b->vel < -MAXV) b->vel = -MAXV;
    b->pos += b->vel * dt;
}

static void update_coaster(float dt) {
    if (n_pts < 2 || total_len <= 0 || n_bodies < 1) return;
    Body *lead = &bodies[0];
    integrate(lead, dt);
    float d = lead->vel * dt;     // arc moved this frame (every cart moves the same)
    while (lead->pos < 0) lead->pos += total_len;
    while (lead->pos >= total_len) lead->pos -= total_len;
    float lead_pan = world_pan(lead->x, lead->y);   // the train's stereo position this frame
    if (flip_cross(lead, d)) flip_sound(lead_pan);
    apply_offset(lead);
    step_arms(lead, 0);
    for (int i = 1; i < n_bodies; i++) {
        bodies[i].pos = lead->pos - i * CART_GAP;
        int seg; sample(bodies[i].pos, &bodies[i].x, &bodies[i].y, &seg,
                        &bodies[i].ux, &bodies[i].uy);
        flip_cross(&bodies[i], d);
        apply_offset(&bodies[i]);
        step_arms(&bodies[i], i);
    }
    riders_primed = 1;   // all carts have placed their arms now
    // feel: spark + whoosh + shake scale with speed; louder on a steep drop
    float sp = lead->vel < 0 ? -lead->vel : lead->vel;
    if (sp > 360 && rnd(3) == 0) {
        float nx = lead->uy, ny = -lead->ux;
        spawn(lead->x + nx * 3, lead->y + ny * 3,
              nx * 30 + rnd_float_between(-12, 12), ny * 30 - 20,
              rnd_between(5, 10), CLR_LIGHT_YELLOW);
    }
    if (lead->uy > 0.45f && sp > 280) shake(sp * 0.006f);
    update_gforce(lead, dt);
    update_scream(sp, lead_pan);
    // ride sound: ratchet voiced by the lead's current zone, rate = speed
    { int ls; float lx, ly, lux, luy; sample(lead->pos, &lx, &ly, &ls, &lux, &luy);
      coaster_sound(pts[ls].zone, sp, d < 0 ? -d : d, lead_pan); }
#ifdef DE_TRACE
    watch("pos", "%.1f", lead->pos);
    watch("vel", "%.1f", lead->vel);
    watch("g", "%.2f", gforce);
    watch("excite", "%.2f", excite);
#endif
}

static void update_slide(float dt) {
    if (n_pts < 2 || total_len <= 0) return;
    for (int i = 0; i < n_bodies; i++) {
        Body *b = &bodies[i];
        if (b->wait > 0) { b->wait--; continue; }
        integrate(b, dt);
        flip_cross(b, b->vel * dt);
        apply_offset(b);
        float sp = b->vel < 0 ? -b->vel : b->vel;
        if (sp > 120 && rnd(2) == 0) {        // water spray off the rider
            float nx = b->uy, ny = -b->ux;
            spawn(b->x + nx * 2, b->y + ny * 2,
                  -b->ux * 20 + rnd_float_between(-15, 15), -25 - rnd(20),
                  rnd_between(6, 13), CLR_WHITE);
        }
        if (b->pos >= total_len - 0.5f) {     // reached the open end → splash
            splash(b->x, b->y, 10, CLR_BLUE);
            splash(b->x, b->y, 4, CLR_WHITE);
            shake(2.0f);
            hit(52 + rnd(8), INSTR_NOISE, 2, 120);
            b->pos = 0; b->vel = 16; b->wait = RIDE_GAP * n_bodies;
        }
    }
}

// ── drawing ─────────────────────────────────────────────────────────────────
static unsigned char held_zone(void) {
    if (key('B')) return Z_BOOST;
    if (key('S')) return Z_BRAKE;
    if (key('H')) return Z_HOIST;
    return Z_NONE;
}

static void add_point(float x, float y) {
    if (n_pts >= MAX_PTS) return;
    pts[n_pts].x = x; pts[n_pts].y = y;
    pts[n_pts].zone = held_zone();
    pts[n_pts].flip = key('F') ? 1 : 0;        // flip is orthogonal to the zone
    n_pts++;
}

static void start_new(void) {
    n_pts = 0; total_len = 0; n_beams = 0; drawing = 0; drag_idx = -1;
    for (int i = 0; i < MAXP; i++) parts[i].life = 0;
    silence_scream();
}

static int nearest_point(int x, int y, float thresh) {
    int best = -1; float bd = thresh * thresh;
    for (int i = 0; i < n_pts; i++) {
        float dx = pts[i].x - x, dy = pts[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void handle_input(void) {
    int mx = mouse_x(), my = mouse_y();

    // the ? button toggles the help panel — consume the click so it can't also
    // start drawing a track (left-click is overloaded for drawing here)
    if (mouse_pressed(MOUSE_LEFT) &&
        point_in_box(mx, my, HELP_BX - 3, HELP_BY - 3, HELP_BW + 6, HELP_BH + 6)) {
        show_help = !show_help;
        return;
    }

    if (mouse_pressed(MOUSE_LEFT)) {
        int near = (n_pts > 1 && !drawing) ? nearest_point(mx, my, 9) : -1;
        if (near >= 0) {
            drag_idx = near;                 // grab a point to reshape
        } else {
            start_new();                     // draw a fresh track
            add_point(mx, my);
            drawing = 1;
        }
    }

    if (drawing && mouse_down(MOUSE_LEFT)) {
        Pt *last = &pts[n_pts - 1];
        float d = distance((int)last->x, (int)last->y, mx, my);
        if (d >= MIN_DIST) {
            int steps = (int)(d / MIN_DIST);
            float sx = (mx - last->x) / (steps + 1), sy = (my - last->y) / (steps + 1);
            float bx = last->x, by = last->y;
            for (int i = 1; i <= steps; i++) add_point(bx + sx * i, by + sy * i);
        }
    }

    if (drag_idx >= 0 && mouse_down(MOUSE_LEFT)) {
        pts[drag_idx].x = mx; pts[drag_idx].y = my;
    }

    if (mouse_released(MOUSE_LEFT)) {
        if (drawing) {
            drawing = 0;
            recompute();
            init_bodies();
            run_started = now();
            note(60, INSTR_SQUARE, 2);
        }
        if (drag_idx >= 0) { recompute(); drag_idx = -1; }
    }

    // after-draw zone/flip toggle: hover a point, tap the key
    if (n_pts > 1 && !drawing) {
        int z = 0;
        if (keyp('B')) z = Z_BOOST;
        else if (keyp('S')) z = Z_BRAKE;
        else if (keyp('H')) z = Z_HOIST;
        if (z) {
            int i = nearest_point(mx, my, 14);
            if (i >= 0) pts[i].zone = (pts[i].zone == z) ? Z_NONE : z;
        }
        if (keyp('F')) {
            int i = nearest_point(mx, my, 14);
            if (i >= 0) { pts[i].flip = !pts[i].flip; recompute(); }  // rebuild flip_pos[]
        }
    }

    if (keyp('M')) { closed = !closed; recompute(); init_bodies(); silence_scream(); }
    if (keyp('K')) wood = !wood;                       // construction: wood / steel
    if (keyp('C')) ride = !ride;                       // POV ride-cam
    if (keyp(KEY_SPACE)) is_paused = !is_paused;
    if (keyp('N')) start_new();
    if (keyp(KEY_TAB)) show_help = !show_help;
    if (keyp(KEY_UP)   && n_bodies < MAX_BODY) { n_bodies++; init_bodies(); }
    if (keyp(KEY_DOWN) && n_bodies > 1)        { n_bodies--; init_bodies(); }
}

// a ready-made loop so the cart is alive the moment it opens — a hilly closed
// circuit with a chain-lift (hoist) up the back so it runs forever.
void init(void) {
    pan_law(PAN_POWER);   // even loudness as clacks/screams sweep the stereo field (positional audio)

    // three faked scream voices (slots 5..7) — each routed through the real
    // 4-bandpass formant (vowel) filter so they read as open "aah/eee" voices,
    // each a different default vowel, wobble rate and detune so together they're
    // a few people not one. The vowel is re-rolled per scream onset (scream_vowel_roll).
    // Kept soft; layering does the rest.
    instrument(5, INSTR_SAW,    20, 140, 6, 200); instrument_formant(5, 0.45f, 0.65f, 0.9f);
    instrument_lfo(5, 0, LFO_PITCH, 5.5f, 0.45f); instrument_drive(5, 0.18f); instrument_tune(5, -0.08f);
    instrument(6, INSTR_SAW,    18, 140, 6, 200); instrument_formant(6, 0.65f, 0.65f, 0.9f);
    instrument_lfo(6, 0, LFO_PITCH, 6.4f, 0.50f); instrument_drive(6, 0.22f);
    instrument(7, INSTR_SQUARE, 22, 130, 6, 190); instrument_formant(7, 0.85f, 0.65f, 0.9f);
    instrument_lfo(7, 0, LFO_PITCH, 7.2f, 0.42f); instrument_drive(7, 0.12f); instrument_tune(7, 0.10f);

    int N = 64;
    float cx = SCREEN_W / 2.0f, cy = 138, rx = 205, ry = 76;
    for (int i = 0; i < N; i++) {
        float a = 360.0f * i / N;
        float top = (-sin_deg(a) + 1) * 0.5f;          // 1 at top, 0 at bottom
        float x = cx + rx * cos_deg(a);
        float y = cy + ry * sin_deg(a) - top * top * 44 * (0.6f + 0.4f * cos_deg(a * 3));
        pts[n_pts].x = x; pts[n_pts].y = y;
        pts[n_pts].zone = (a >= 150 && a <= 255) ? Z_HOIST : Z_NONE;   // lift hill
        n_pts++;
    }
    closed = 1;
    recompute();
    init_bodies();
}

void update(void) {
    handle_input();
#ifdef DE_TRACE
    watch("npts", "%d", n_pts);
    watch("len", "%.0f", total_len);
    watch("closed", "%d", closed);
#endif
    if (is_paused) { silence_scream(); return; }
    float dt_ = dt(); if (dt_ > 0.05f) dt_ = 0.05f;
    if (closed) update_coaster(dt_); else update_slide(dt_);
    tick_parts(dt_);
}

// ── render ──────────────────────────────────────────────────────────────────
static int zone_col(unsigned char z) {
    return z == Z_BOOST ? CLR_GREEN : z == Z_BRAKE ? CLR_RED :
           z == Z_HOIST ? CLR_YELLOW : -1;
}

// one vertical bent (the post part). Real coaster supports are near-vertical
// posts, not triangles — the cross-bracing that ties them together (draw_bay)
// is what reads as a coaster instead of a row of radio towers.
//   wood  → a paired timber post with horizontal ledgers up its length
//   steel → a fat tubular column (a few parallel lines for thickness)
static void draw_post(float bx, int by, int gy) {
    int x = (int)bx;
    if (wood) {
        line(x - 1, by, x - 1, gy, CLR_DARK_BROWN);    // the bent = a pair of timbers
        line(x + 1, by, x + 1, gy, CLR_BROWN);
        for (int y = gy - 6; y > by; y -= 9)           // ledgers up the post
            line(x - 2, y, x + 2, y, CLR_DARK_BROWN);
        line(x - 2, gy, x + 2, gy, CLR_DARK_BROWN);    // footing
    } else {
        line(x - 1, by, x - 1, gy, CLR_DARK_GREY);     // fat steel tube (cool, shaded)
        line(x,     by, x,     gy, CLR_LIGHT_GREY);
        line(x + 1, by, x + 1, gy, CLR_DARK_GREY);
        line(x - 2, gy, x + 2, gy, CLR_DARK_GREY);     // concrete footer
    }
}

// the structure BETWEEN two adjacent bents — this is what makes it continuous.
//   wood  → a dense X cross-brace lattice + horizontal chords
//   steel → just a couple of cross-ties (sparse, like skyscraper framing)
static void draw_bay(float x0f, int y0, float x1f, int y1, int gy) {
    int x0 = (int)x0f, x1 = (int)x1f;
    int top = y0 > y1 ? y0 : y1;                       // brace only where both posts exist
    int h = gy - top; if (h <= 3) return;
    if (wood) {
        int bays = h / 9; if (bays < 1) bays = 1;
        for (int k = 0; k < bays; k++) {
            int ya = gy - h * k / bays, yb = gy - h * (k + 1) / bays;
            line(x0, ya, x1, yb, CLR_BROWN);           // X cross-brace
            line(x1, ya, x0, yb, CLR_BROWN);
            line(x0, yb, x1, yb, CLR_DARK_BROWN);      // horizontal chord
        }
    } else {
        line(x0, gy - h / 3,     x1, gy - h / 3,     CLR_DARK_GREY);   // two ties only
        line(x0, gy - 2 * h / 3, x1, gy - 2 * h / 3, CLR_DARK_GREY);
    }
}

static void draw_track(void) {
    int segs = n_segs();
    int gy = ground_y();

    // support structure behind everything: vertical bents tied together into a
    // continuous lattice (wood) or sparse braced columns (steel)
    for (int i = 0; i + 1 < n_beams; i++) {            // ties between neighbours first
        float dx = beam_x[i + 1] - beam_x[i]; if (dx < 0) dx = -dx;
        if (beam_y[i] < gy && beam_y[i + 1] < gy && dx < SUPPORT_SPACING * 2.2f)
            draw_bay(beam_x[i], (int)beam_y[i], beam_x[i + 1], (int)beam_y[i + 1], gy);
    }
    for (int i = 0; i < n_beams; i++)                 // then the posts on top
        if (beam_y[i] < gy) draw_post(beam_x[i], (int)beam_y[i], gy);

    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % n_pts;
        float x1 = pts[i].x, y1 = pts[i].y, x2 = pts[j].x, y2 = pts[j].y;
        int zc = zone_col(pts[i].zone);

        if (closed) {
            // steel rails: two lines offset along the segment normal + ties
            float dx = x2 - x1, dy = y2 - y1, L = seglen[i] > 0 ? seglen[i] : 1;
            float nx = dy / L * 2.0f, ny = -dx / L * 2.0f;
            int rail = zc >= 0 ? zc : CLR_LIGHT_GREY;
            line((int)(x1 + nx), (int)(y1 + ny), (int)(x2 + nx), (int)(y2 + ny), rail);
            line((int)(x1 - nx), (int)(y1 - ny), (int)(x2 - nx), (int)(y2 - ny), rail);
            if ((i & 1) == 0)
                line((int)(x1 + nx), (int)(y1 + ny), (int)(x1 - nx), (int)(y1 - ny), CLR_DARK_GREY);
        } else {
            // water flume: a fat blue band with a lighter sheen down the middle
            float dx = x2 - x1, dy = y2 - y1, L = seglen[i] > 0 ? seglen[i] : 1;
            float nx = dy / L * 3.0f, ny = -dx / L * 3.0f;
            int water = zc >= 0 ? zc : CLR_TRUE_BLUE;
            quadfill((int)(x1 + nx), (int)(y1 + ny), (int)(x2 + nx), (int)(y2 + ny),
                     (int)(x2 - nx), (int)(y2 - ny), (int)(x1 - nx), (int)(y1 - ny), water);
            line((int)x1, (int)y1, (int)x2, (int)y2, CLR_BLUE);
        }
    }

    // flip points: a pink cross-tie, always visible (shows up on rails and water)
    for (int i = 0; i < segs; i++)
        if (pts[i].flip) {
            int j = (i + 1) % n_pts;
            float dx = pts[j].x - pts[i].x, dy = pts[j].y - pts[i].y;
            float L = seglen[i] > 0 ? seglen[i] : 1;
            float nx = dy / L * 4.0f, ny = -dx / L * 4.0f;
            line((int)(pts[i].x + nx), (int)(pts[i].y + ny),
                 (int)(pts[i].x - nx), (int)(pts[i].y - ny), CLR_PINK);
        }

    // ── decals: the details that read as a real ride (coaster only) ──────────
    if (closed && total_len > 0) {
        // catwalk — a maintenance walkway dashed alongside the track, with posts
        for (int i = 0; i < segs; i += 2) {
            int j = (i + 1) % n_pts;
            float dx = pts[j].x - pts[i].x, dy = pts[j].y - pts[i].y, L = seglen[i] > 0 ? seglen[i] : 1;
            float ox = dy / L * 5.0f, oy = -dx / L * 5.0f;
            line((int)(pts[i].x + ox), (int)(pts[i].y + oy), (int)(pts[j].x + ox), (int)(pts[j].y + oy), CLR_DARKER_GREY);
            if (i % 6 == 0)
                line((int)pts[i].x, (int)pts[i].y, (int)(pts[i].x + ox), (int)(pts[i].y + oy), CLR_DARKER_GREY);
        }
        // per-zone mechanism: chain + anti-rollback sawtooth on the lift, fins on the brake
        for (int i = 0; i < segs; i++) {
            int j = (i + 1) % n_pts;
            float dx = pts[j].x - pts[i].x, dy = pts[j].y - pts[i].y, L = seglen[i] > 0 ? seglen[i] : 1;
            float nx = dy / L, ny = -dx / L;
            if (pts[i].zone == Z_HOIST) {
                int links = (int)(L / 3); if (links < 1) links = 1;
                for (int k = 0; k <= links; k++) {
                    float t = (float)k / links;
                    int cx = (int)lerp(pts[i].x, pts[j].x, t), cy = (int)lerp(pts[i].y, pts[j].y, t);
                    pset(cx, cy, (k & 1) ? CLR_LIGHT_GREY : CLR_DARK_GREY);                 // chain links
                    if (k & 1) pset((int)(cx + nx * 3), (int)(cy + ny * 3), CLR_MEDIUM_GREY); // sawtooth teeth
                }
            } else if (pts[i].zone == Z_BRAKE) {
                int fins = (int)(L / 4); if (fins < 1) fins = 1;
                for (int k = 0; k < fins; k++) {
                    float t = (k + 0.5f) / fins;
                    int cx = (int)lerp(pts[i].x, pts[j].x, t), cy = (int)lerp(pts[i].y, pts[j].y, t);
                    line((int)(cx + nx * 2), (int)(cy + ny * 2), (int)(cx - nx * 2), (int)(cy - ny * 2), CLR_LIGHT_GREY);
                }
            }
        }
        // summit flag at the highest point of the track
        int hi = 0; for (int i = 1; i < n_pts; i++) if (pts[i].y < pts[hi].y) hi = i;
        int fx = (int)pts[hi].x, fy = (int)pts[hi].y;
        line(fx, fy, fx, fy - 11, CLR_LIGHT_GREY);                       // flagpole
        trifill(fx, fy - 11, fx, fy - 6, fx + 6, fy - 9, CLR_RED);       // pennant
    }

    // points while editing
    if (drawing || drag_idx >= 0)
        for (int i = 0; i < n_pts; i++) {
            int zc = pts[i].flip ? CLR_PINK : zone_col(pts[i].zone);
            pset((int)pts[i].x, (int)pts[i].y, zc >= 0 ? zc : CLR_WHITE);
        }

    // live preview line while drawing
    if (drawing && n_pts > 0)
        line((int)pts[n_pts - 1].x, (int)pts[n_pts - 1].y, mouse_x(), mouse_y(), CLR_DARK_GREY);
}

static void draw_cart(Body *b) {
    float nx = b->uy, ny = -b->ux;
    float hl = 4.5f, hw = 3.0f;
    int col = CLR_RED + (b->hue % 5);   // varied warm hues
    quadfill((int)(b->x - b->ux * hl - nx * hw), (int)(b->y - b->uy * hl - ny * hw),
             (int)(b->x + b->ux * hl - nx * hw), (int)(b->y + b->uy * hl - ny * hw),
             (int)(b->x + b->ux * hl + nx * hw), (int)(b->y + b->uy * hl + ny * hw),
             (int)(b->x - b->ux * hl + nx * hw), (int)(b->y - b->uy * hl + ny * hw), col);
}

static void draw_rider(Body *b) {
    float nx = b->uy, ny = -b->ux;     // sit the rider just above the flume
    int rx = (int)(b->x - nx * 3), ry = (int)(b->y - ny * 3);
    circfill(rx, ry, 2, CLR_PEACH);                 // body
    pset(rx, ry - 2, CLR_DARK_BROWN);               // head
}

// the two coaster riders on cart ci: fixed torso + head, plus the verlet arms
static void draw_riders(Body *b, int ci) {
    float ux = b->ux, uy = b->uy, nx = b->uy, ny = -b->ux;   // tangent + body-up
    for (int c = 0; c < CHARS_PER_CART; c++) {
        float fb = (c == 0) ? 1.0f : -1.0f;
        float cx = b->x + ux * CHAR_OFF * fb, cy = b->y + uy * CHAR_OFF * fb;
        Rider *r = &riders[ci][c];
        float bt = BODY_W, top = BODY_H;                      // torso half-width / height
        quadfill((int)(cx - ux * bt), (int)(cy - uy * bt),
                 (int)(cx + ux * bt), (int)(cy + uy * bt),
                 (int)(cx + ux * bt + nx * top), (int)(cy + uy * bt + ny * top),
                 (int)(cx - ux * bt + nx * top), (int)(cy - uy * bt + ny * top), r->clothes);
        circfill((int)(cx + nx * (top + 1.4f)), (int)(cy + ny * (top + 1.4f)), 1, CLR_PEACH);
        for (int arm = 0; arm < 2; arm++) {                   // shoulder→elbow→hand
            VArm *A = &r->arm[arm];
            line((int)A->px[0], (int)A->py[0], (int)A->px[1], (int)A->py[1], CLR_DARK_PEACH);
            line((int)A->px[1], (int)A->py[1], (int)A->px[2], (int)A->py[2], CLR_DARK_PEACH);
        }
    }
}

// shortest-path angle lerp (handles the 360° wrap)
static float lerp_angle(float a, float b, float t) {
    float d = b - a;
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return a + d * t;
}

// ease the camera toward its target: overview (identity) when off, locked to the
// lead cart + zoomed + banked so forward points up-screen when riding.
static void update_camera(void) {
    float tx = 0, ty = 0, tz = 1, ta = 0;
    if (ride && n_pts >= 2 && total_len > 0) {
        Body *L = &bodies[0];
        tx = L->x - SCREEN_W / 2.0f;
        ty = L->y - SCREEN_H / 2.0f;
        tz = RIDE_ZOOM;
        float travel = angle_to(0, 0, (int)(L->ux * 512), (int)(L->uy * 512));
        ta = -travel;                      // rotate so the travel direction points right
    }
    cam_x = lerp(cam_x, tx, 0.4f);
    cam_y = lerp(cam_y, ty, 0.4f);
    cam_zoom = lerp(cam_zoom, tz, 0.12f);
    cam_angle = lerp_angle(cam_angle, ta, 0.15f);
}

void draw(void) {
    update_camera();

    // flat happy sky in SCREEN space (before the camera) so banking never reveals gaps
    cls(CLR_BLUE);

    camera_ex((int)cam_x, (int)cam_y, cam_zoom, cam_angle);
    int gy = ground_y();
    rectfill(0, gy, SCREEN_W, SCREEN_H - gy, closed ? CLR_DARK_GREEN : CLR_BLUE_GREEN);
    line(0, gy, SCREEN_W, gy, CLR_DARK_GREEN);

    if (n_pts >= 2) draw_track();

    // particles
    for (int i = 0; i < MAXP; i++)
        if (parts[i].life > 0)
            pset((int)parts[i].x, (int)parts[i].y, parts[i].col);

    // bodies
    if (n_pts >= 2 && total_len > 0) {
        if (closed) {
            // couplings, then carts
            for (int i = 0; i + 1 < n_bodies; i++)
                line((int)bodies[i].x, (int)bodies[i].y,
                     (int)bodies[i + 1].x, (int)bodies[i + 1].y, CLR_DARK_GREY);
            for (int i = 0; i < n_bodies; i++) { draw_cart(&bodies[i]); draw_riders(&bodies[i], i); }
        } else {
            for (int i = 0; i < n_bodies; i++)
                if (bodies[i].wait <= 0) draw_rider(&bodies[i]);
        }
    }

    // ── HUD (screen space) ────────────────────────────────────────────────
    camera(0, 0);
    int tx2 = print(closed ? "COASTER" : "SLIDE", 4, 4, CLR_WHITE);
    print(wood ? " WOOD" : " STEEL", tx2, 4, wood ? CLR_BROWN : CLR_LIGHT_GREY);
    if (n_pts < 2) {
        print("DRAG TO DRAW A TRACK", SCREEN_W / 2 - 78, SCREEN_H / 2, CLR_WHITE);
    } else if (closed) {
        char buf[32];
        float sp = bodies[0].vel < 0 ? -bodies[0].vel : bodies[0].vel;
        snprintf(buf, sizeof buf, "SPEED %4.0f", sp);
        print(buf, 4, 14, CLR_LIGHT_YELLOW);
        snprintf(buf, sizeof buf, "CARTS %d", n_bodies);
        print(buf, 4, 24, CLR_LIGHT_GREY);
        // g-meter: current g, lap peak, and an airtime flag
        int gcol = gforce < 0.45f ? CLR_BLUE : gforce > 2.2f ? CLR_RED : CLR_GREEN;
        snprintf(buf, sizeof buf, "%.1fG", gforce);
        print(buf, 4, 36, gcol);
        snprintf(buf, sizeof buf, "PEAK %.1f", gpeak);
        print(buf, 40, 36, CLR_DARK_GREY);
        if (gforce < 0.35f && sp > 110) print("AIRTIME!", 4, 46, CLR_LIGHT_YELLOW);
        // g-ball: a 2-axis gauge. rests at centre; up = heavy, down = airtime,
        // left/right = brake/launch (the longitudinal axis).
        int gbs = 28, gbx = SCREEN_W - gbs - 6, gby = 6;
        rectfill(gbx, gby, gbs, gbs, CLR_BROWNISH_BLACK);
        int gcx = gbx + gbs / 2, gcy = gby + gbs / 2;
        line(gbx, gcy, gbx + gbs, gcy, CLR_DARKER_GREY);
        line(gcx, gby, gcx, gby + gbs, CLR_DARKER_GREY);
        int dx = gcx + (int)clamp(g_long * 4.5f, -13, 13);
        int dy = gcy - (int)clamp((gforce - 1) * 4.5f, -13, 13);
        circfill(dx, dy, 2, gcol);
    }
    if (is_paused) print("PAUSED", SCREEN_W / 2 - 24, 4, CLR_YELLOW);

    if (show_help) {
        int x = 4, y = SCREEN_H - 76;
        rectfill(x - 2, y - 2, 244, 74, CLR_BROWNISH_BLACK);
        print("drag empty=draw  drag pt=move", x, y, CLR_LIGHT_GREY);          y += 9;
        print("hold B/S/H/F while drawing:", x, y, CLR_LIGHT_GREY);            y += 9;
        print(" boost / slow / hoist / flip", x, y, CLR_LIGHT_GREY);           y += 9;
        print("C ride  M mode  K wood/steel", x, y, CLR_LIGHT_GREY);           y += 9;
        print("N new SPACE pause up/dn carts", x, y, CLR_LIGHT_GREY);          y += 9;
        print("? or TAB to hide", x, y, CLR_LIGHT_GREY);
    } else if (ride) {
        print("RIDING  (C exit)", SCREEN_W - 70, SCREEN_H - 10, CLR_DARK_GREY);
    }

    // the ? help toggle — always on screen, highlighted while the panel is open
    rectfill(HELP_BX, HELP_BY, HELP_BW, HELP_BH, show_help ? CLR_YELLOW : CLR_BROWNISH_BLACK);
    rect(HELP_BX, HELP_BY, HELP_BW, HELP_BH, CLR_LIGHT_GREY);
    print("?", HELP_BX + 4, HELP_BY + 3, show_help ? CLR_BROWNISH_BLACK : CLR_WHITE);
}
