#include "studio.h"
#include "ui.h"

// TRACKGEN — a procedural kart/race-track generator you can drive.
//
// A jittered ellipse can only ever make "a circle with wobble": its control
// points march evenly around one centre, so the loop can't fold in, double back
// or run a straight into a hairpin. Real circuits come from a different recipe —
// the classic procedural-racetrack algorithm:
//
//   1. SCATTER random points in a box.
//   2. CONVEX HULL of them → a guaranteed simple (non-crossing) loop.
//   3. PUSH APART points that crowd, so corners aren't cramped.
//   4. DISPLACE the midpoint of each edge perpendicular — this carves the
//      concavities (the in-and-out) that read as a real track.
//   5. RELAX any corner too sharp to drive (the drivability clamp).
//   6. Cardinal-spline through the result → the centre line.
//
// Layered on top: CORNER VARIETY (hairpins mixed with sweepers), CHICANES
// (quick S-bends), a MAIN STRAIGHT (longest edge kept straight), and STYLE
// presets that bundle it all — Grand Prix, Technical, Speedway (a stadium oval),
// Rally, Classic (the old ellipse) and Figure-8 (an ellipse blended toward a
// lemniscate so it crosses itself).
//
// SETUP PANEL — cycle the STYLE, drag the levers, watch the preview, then DRIVE.
// DRIVING — a RACE against six AI rivals over RACE_LAPS laps. Every car (you +
// rivals) runs ONE shared physics step (step_car); the rivals just feed it a
// turn/throttle from drive_ai() instead of the keys. drive_ai is the whole brain:
// look a few samples ahead on the centre line, steer at that point, and brake for
// the bend it reads further ahead. The six personalities are NOT six code paths —
// they are rows in PERS[] (look-ahead, braking margin, speed cap, line offset,
// wobble, drift threshold, aggro, rubber-band): PRO hugs the line, HUNTER chases
// you, DRIFT slides wide, SUNDAY pootles, ROOKIE wanders and spins, RUBBER catches
// up when behind. This brain ports straight to sloop — only the line it follows
// changes (cl[] here, road_at() in the open world later).
// Cars also COLLIDE (resolve_collisions): overlapping bodies are pushed apart and
// trade momentum along the contact normal, so you can shove a rival wide into a
// corner — and get shoved. A hard hit bleeds speed and (if you're in it) clacks
// and kicks the camera. Contact is MASS-weighted per personality (PERS[].mass):
// HUNTER leans in (heavy, barely budges and flings you), SUNDAY/ROOKIE yield
// (light, get knocked away) — so collisions express character too.
//
// TWO MODES (toggle in the setup panel): RACE (above) and TRAFFIC — same track,
// not a race. In TRAFFIC a crowd (TRAFFIC_CARS) of AI runs drive_ai_traffic: hold a
// lane, follow the car ahead at a safe time-gap (IDM-style car-following), lift for
// corners, and OVERTAKE — pull into a clear adjacent lane to pass a slower car (so a
// car blocked behind you, even a stopped you, goes around rather than stacking up).
// Lanes are emergent from lateral position, so the player participates: sit in a lane
// and traffic follows/passes you. Cars are tinted by SPEED (red=stopped → green=flowing)
// so jams are visible. A TRAFFIC LIGHT cycles on the loop — a red is a stop-line all
// lanes queue behind (the bottleneck that breeds stop-and-go). And a small REACTION
// LAG (REACT_N) on following makes dense traffic UNSTABLE — phantom jams emerge with no
// cause, exactly the ring-road experiment. Design + remaining systems (figure-8
// right-of-way, speed zones, hazards): docs/design/traffic-ai.md. (Known rough edge:
// on the tightest procedural corner a fast car can still clip the apex and recover.)
//   LEFT/RIGHT steer · UP gas · DOWN brake · X drift · Z new track · R restart
//   M: back to the setup panel.  RACE: standings on the right + results on finish.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── car tuning — edit + hot-reload ──────────────────────────────────────────
#define ACCEL      0.07f
#define BRAKE      0.13f
#define REV_MAX   -1.0f
#define FRICTION   0.985f
#define MAX_SPD    3.1f
#define STEER      3.6f
#define GRIP_RACE  0.25f
#define GRIP_DRIFT 0.07f
#define SLIDE_MIN  0.55f

#define GRASS_DRAG 0.965f
#define GRASS_MAX  1.8f

#define CAR_R      5.0f        // car body radius for car-to-car collision (world px)
#define BUMP_REST  0.35f       // restitution: 0 = dead thud, 1 = fully elastic bounce
#define BUMP_BLEED 0.90f       // speed retained by both cars after a hard contact

// ── traffic mode (free-flow lane driving — see docs/design/traffic-ai.md) ──
#define LANE_MIN   12.0f       // min lane band width (px) — keeps side-by-side cars off each other
#define LANES_MAX  3
#define TRAF_LA    12          // traffic steering look-ahead (samples)
#define TG_GAP0    12.0f       // car-following: standstill gap to the leader (px)
#define TG_HEAD    14.0f       // time headway (frames) — gap target grows with speed
#define TG_BRAKE   0.05f       // comfortable decel used to size the safe gap

#define CAM_LEAD   16.0f
#define CAM_LERP   0.09f

#define NSAMP   360
#define MAXCTRL 80          // hull corners + displaced midpoints + chicane points
#define MAXSEED 24          // scattered points before the hull
#define MAXSKID 160

// levers — NAME the indices (CLAUDE.md): a reorder must fail at the compiler,
// not silently cross-wire sliders ↔ presets
enum { LV_SIZE, LV_WIDTH, LV_CORNERS, LV_VARIETY, LV_TWIST,
       LV_CHICANE, LV_STRAIGHT, LV_SMOOTH, NLEV };

enum { ST_GRANDPRIX, ST_TECHNICAL, ST_CIRCUIT, ST_SPEEDWAY, ST_RALLY,
       ST_CLASSIC, ST_FIGURE8, ST_COUNT };
static const char *STYLE_NAME[ST_COUNT] = {
    "GRAND PRIX", "TECHNICAL", "CIRCUIT", "SPEEDWAY", "RALLY", "CLASSIC", "FIGURE-8" };

// preset bundles, indexed [style][lever] — picking a style snaps the levers here
static const float PRESET[ST_COUNT][NLEV] = {
    /*              size  wid  corn  var  twst chic  str  smth */
    /* GRANDPRIX */ {0.70,0.45,0.55,0.55,0.45,0.30,0.65,0.60},
    /* TECHNICAL */ {0.55,0.35,0.80,0.80,0.70,0.65,0.30,0.40},
    /* CIRCUIT   */ {0.74,0.38,0.55,0.45,0.55,0.30,0.45,0.55},
    /* SPEEDWAY  */ {0.78,0.65,0.30,0.10,0.05,0.00,0.92,0.80},
    /* RALLY     */ {0.88,0.72,0.45,0.65,0.40,0.10,0.40,0.70},
    /* CLASSIC   */ {0.55,0.40,0.40,0.30,0.20,0.00,0.45,0.50},
    /* FIGURE-8  */ {0.58,0.45,0.45,0.30,0.25,0.00,0.45,0.55},
};

typedef struct { float x, y; } V2;
typedef struct { int x, y; int life; } Skid;

// ── racers ───────────────────────────────────────────────────────────────────
// One Car struct, one array. Car 0 is the player (reads btn()); cars 1..N are AI
// rivals (read drive_ai()). EVERY car runs the SAME physics (step_car) — the only
// difference between player and AI is who supplies turn_in/throttle each frame.
// That is the whole point: the brain built here ports to sloop unchanged; only the
// *line to follow* changes (cl[] here, road_at() in the open world later).
#define RACE_CARS    7         // RACE: 1 player + 6 AI, one per personality
#define TRAFFIC_CARS 18        // TRAFFIC: a crowd — enough density for queues & jams
#define CARS_MAX     28        // array bound (>= both of the above)
#define RACE_LAPS 3
#define COLL_R    16.0f        // car-to-car avoidance radius (world px)
#define REACT_N   5            // car-following reaction lag (frames) — seeds phantom jams

typedef struct {
    float px, py, vx, vy, spd, ang;
    bool  drift, offtrack;
    int   prog, cp, lap;       // prog = nearest cl[] sample; cp = last checkpoint 0..3
    int   pers;                // personality index into PERS[] (AI only)
    float wob;                 // rookie wobble phase
    float mass;                // collision weight (player 1.0; AI from PERS[].mass)
    int   place;               // live race position (1 = leading), recomputed each frame
    int   lane;                // current lane (computed from lateral offset each frame)
    int   want_lane;           // intended lane (traffic) — overtaking sets this; steering aims at it
    float thist[REACT_N];      // recent throttle commands — applied delayed (reaction lag)
    int   thead;               // ring head into thist[]
} Car;

// A personality is JUST a bag of numbers fed to the same follow-controller. No
// per-car code paths — "same brain, different soul." Reorder at your peril; the
// PERS[] rows below line up with these fields by position.
typedef struct {
    const char *name;
    int   color;
    float la;          // steer look-ahead (samples along cl[])
    float corner_la;   // braking look-ahead (samples) — how far it reads the bend
    float brake_g;     // 0..1 how much an upcoming bend cuts target speed
    float spd_cap;     // fraction of MAX_SPD it is willing to use
    float line_off;    // lateral bias on the line (+ = outside), in half-widths
    float wobble;      // random target jitter (half-widths) — rookie sloppiness
    float drift_corner;// engage drift when bend exceeds this (0 = never)
    float aggro;       // tailgater: pull target toward the player when close
    float rubber;      // blocker: speed-cap boost when behind the player on track
    float mass;        // contact weight (player = 1.0): heavy leans in, light yields
} Pers;

enum { AI_PRO, AI_HUNTER, AI_DRIFT, AI_SUNDAY, AI_ROOKIE, AI_RUBBER, AI_COUNT };
static const Pers PERS[AI_COUNT] = {
    /*           name      color           la  cLA brk  cap  off  wob  drft aggr rub  mass */
    /* PRO    */{"PRO",    CLR_WHITE,      22, 26, 0.55,1.00,0.00,0.00,0.85,0.00,0.00,1.00},
    /* HUNTER */{"HUNTER", CLR_ORANGE,     12, 16, 0.30,0.98,0.00,0.00,0.70,0.55,0.00,1.60},
    /* DRIFT  */{"DRIFT",  CLR_PINK,       18, 22, 0.40,0.95,0.40,0.05,0.45,0.00,0.00,0.95},
    /* SUNDAY */{"SUNDAY", CLR_BLUE,       24, 30, 0.70,0.72,0.00,0.00,0.00,0.00,0.00,0.70},
    /* ROOKIE */{"ROOKIE", CLR_LIME_GREEN, 14, 14, 0.25,0.90,0.00,0.55,0.00,0.00,0.00,0.80},
    /* RUBBER */{"RUBBER", CLR_YELLOW,     20, 24, 0.50,0.86,0.00,0.00,0.00,0.00,0.50,1.25},
};

STATE {
    int   mode;                // 0 = setup panel, 1 = driving, 2 = race results
    bool  traffic;             // false = RACE, true = TRAFFIC (free-flow, no laps)
    int   style;
    float lev[NLEV];
    unsigned int seed;
    int   nctrl, half;
    int   nlanes;              // lanes across the cross-section (from track width)
    float lane_w;              // lane band width (px) = 2*half / nlanes
    float seg_len;             // avg world distance between adjacent cl[] samples (px)
    V2    cl[NSAMP];           // centre line (world)
    V2    nl[NSAMP];           // unit normals
    Car   car[CARS_MAX];       // car[0] = player, car[1..] = AI
    int   ncars;               // active cars this mode (RACE_CARS or TRAFFIC_CARS)
    int   light;               // TRAFFIC: traffic-light sample on the loop (-1 = none)
    int   light_t;             // light phase timer (frames)
    bool  light_red;           // is the light currently red?
    float camx, camy;
    float shake;               // screen-shake impulse (decays), set on a hard player hit
    float best, last;          // player lap times
    bool  finished;            // player has completed RACE_LAPS
    int   result_place;        // player's finishing position (snapshot)
    Skid  skid[MAXSKID];
    int   skid_head;
    int   eng;
};

#define P (S->car[0])          // the player car

// ── seeded RNG so a seed reproduces its track ───────────────────────────────
static unsigned int g_rng;
static float frand(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return (float)((g_rng >> 8) & 0xFFFFFF) / 16777216.0f;
}
static float frange(float a, float b) { return a + (b - a) * frand(); }
static float fabsf_(float x) { return x < 0 ? -x : x; }

static float atan2_deg(float y, float x) {
    const float PI = 3.14159265f;
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float ay = (y < 0 ? -y : y) + 1e-9f, r, a;
    if (x >= 0) { r = (x - ay) / (x + ay); a = 0.1963f*r*r*r - 0.9817f*r + PI/4; }
    else        { r = (x + ay) / (ay - x); a = 0.1963f*r*r*r - 0.9817f*r + 3*PI/4; }
    a *= 180.0f / PI;
    return (y < 0) ? -a : a;
}
static float adiff(float a, float b) {                 // smallest |angle a-b|, deg
    float d = a - b;
    while (d > 180) d -= 360; while (d < -180) d += 360;
    return d < 0 ? -d : d;
}
static float awrap(float d) {                          // wrap to signed [-180,180]
    while (d > 180) d -= 360; while (d < -180) d += 360;
    return d;
}

// cardinal spline value on p1→p2; tension c (0.5 = Catmull-Rom, →0 = straighter)
static float card(float p0, float p1, float p2, float p3, float u, float c) {
    float m1 = c * (p2 - p0), m2 = c * (p3 - p1);
    float u2 = u * u, u3 = u2 * u;
    return (2*u3 - 3*u2 + 1)*p1 + (u3 - 2*u2 + u)*m1
         + (-2*u3 + 3*u2)*p2 + (u3 - u2)*m2;
}

// ── convex hull (Andrew's monotone chain); out needs room for 2*n+1 ─────────
static float cross2(V2 o, V2 a, V2 b) {
    return (a.x - o.x)*(b.y - o.y) - (a.y - o.y)*(b.x - o.x);
}
static int hull_of(V2 *p, int n, V2 *out) {
    for (int i = 0; i < n; i++)                        // sort by x then y (n small)
        for (int j = 0; j < n-1-i; j++)
            if (p[j].x > p[j+1].x || (p[j].x == p[j+1].x && p[j].y > p[j+1].y)) {
                V2 t = p[j]; p[j] = p[j+1]; p[j+1] = t;
            }
    int k = 0;
    for (int i = 0; i < n; i++) {
        while (k >= 2 && cross2(out[k-2], out[k-1], p[i]) <= 0) k--;
        out[k++] = p[i];
    }
    int lo = k + 1;
    for (int i = n-2; i >= 0; i--) {
        while (k >= lo && cross2(out[k-2], out[k-1], p[i]) <= 0) k--;
        out[k++] = p[i];
    }
    return k - 1;                                       // last == first; distinct = k-1
}

static int style_mode(int s) {                          // 0 ellipse 1 hull 2 oval 3 fourier
    if (s == ST_SPEEDWAY) return 2;
    if (s == ST_CIRCUIT)  return 3;
    if (s == ST_CLASSIC || s == ST_FIGURE8) return 0;
    return 1;
}

// CIRCUIT — a random FOURIER LOOP: the centre line is a sum of sinusoid
// harmonics in x and y. The fundamental is the overall loop; each higher
// harmonic folds it inward (winding back on itself like a real kart track),
// and with enough amplitude (twist) it weaves through the middle and CROSSES
// itself many times. Smooth and always closed by construction.
static int gen_fourier(V2 *ctrl, float RX, float RY) {
    int   H       = 3 + (int)(S->lev[LV_CORNERS] * 4.0f);   // 3..7 harmonics
    float twist   = S->lev[LV_TWIST];
    float variety = S->lev[LV_VARIETY];
    if (H > 7) H = 7;

    float ax[8], ay[8], phx[8], phy[8];
    for (int k = 1; k <= H; k++) {
        float dec = 1.0f / (float)k;                        // higher harmonics smaller
        float v   = 1.0f + variety * (frand()*2 - 1) * 0.8f;
        ax[k] = ay[k] = twist * 1.4f * dec * (v < 0.1f ? 0.1f : v);
        phx[k] = frange(0, 360); phy[k] = frange(0, 360);
    }
    ax[1] = 1.0f; ay[1] = 1.0f; phx[1] = 0; phy[1] = 90;    // fundamental = ellipse

    int NC = 8 * H; if (NC > 64) NC = 64; if (NC < 24) NC = 24;
    float mx = 1e-3f, my = 1e-3f;
    for (int i = 0; i < NC; i++) {
        float t = (float)i / NC * 360.0f, x = 0, y = 0;
        for (int k = 1; k <= H; k++) {
            x += ax[k] * cos_deg(k*t - phx[k]);
            y += ay[k] * cos_deg(k*t - phy[k]);
        }
        ctrl[i].x = x; ctrl[i].y = y;
        if (fabsf_(x) > mx) mx = fabsf_(x);
        if (fabsf_(y) > my) my = fabsf_(y);
    }
    float sx = RX / mx, sy = RY / my;                       // normalise into the box
    for (int i = 0; i < NC; i++) { ctrl[i].x *= sx; ctrl[i].y *= sy; }
    return NC;
}

// ── the three layout generators → fill ctrl[], return count ─────────────────
static int gen_ellipse(V2 *ctrl, float RX, float RY) {
    int n = 6 + (int)(S->lev[LV_CORNERS] * 9.99f);
    float cross = (S->style == ST_FIGURE8) ? 1.0f : 0.0f;
    float sharp = S->lev[LV_VARIETY];
    for (int i = 0; i < n; i++) {
        float a  = (float)i/n * 360.0f + frange(-1,1) * (180.0f/n) * (0.25f + sharp*0.45f);
        float ex = cos_deg(a),  ey = sin_deg(a);
        float gx = cos_deg(a),  gy = sin_deg(2*a);      // lemniscate → figure-8
        float bx = lerp(ex, gx, cross), by = lerp(ey, gy, cross);
        float rs = 1.0f - frand() * sharp * 0.55f;
        ctrl[i].x = bx * RX * rs; ctrl[i].y = by * RY * rs;
    }
    return n;
}

static int gen_oval(V2 *ctrl, float RX, float RY) {     // stadium: 2 straights + 2 arcs
    float hs = RX * 0.55f, rr = RY * 0.92f;             // traversed clockwise so the
    int straN = 4, arcN = 6, nc = 0;                    // ribbon never folds on itself
    // bottom straight, left → right (y=+rr)
    for (int i = 0; i < straN; i++) { float t=(float)i/straN; ctrl[nc].x=-hs+2*hs*t; ctrl[nc++].y= rr; }
    // right arc, bottom → top (angle 90 → -90, bulging right)
    for (int i = 0; i < arcN;  i++) { float a= 90-180.0f*(i+1)/(arcN+1); ctrl[nc].x= hs+cos_deg(a)*rr; ctrl[nc++].y=sin_deg(a)*rr; }
    // top straight, right → left (y=-rr)
    for (int i = 0; i < straN; i++) { float t=(float)i/straN; ctrl[nc].x= hs-2*hs*t; ctrl[nc++].y=-rr; }
    // left arc, top → bottom (angle -90 → -270, bulging left)
    for (int i = 0; i < arcN;  i++) { float a=-90-180.0f*(i+1)/(arcN+1); ctrl[nc].x=-hs+cos_deg(a)*rr; ctrl[nc++].y=sin_deg(a)*rr; }
    return nc;
}

static int gen_hull(V2 *ctrl, float RX, float RY) {
    float variety = S->lev[LV_VARIETY], twist = S->lev[LV_TWIST];
    float chicane = S->lev[LV_CHICANE], straight = S->lev[LV_STRAIGHT];

    int K = 8 + (int)(S->lev[LV_CORNERS] * 10.0f);      // 8..18 scattered points
    if (K > MAXSEED) K = MAXSEED;
    V2 pts[MAXSEED];
    for (int i = 0; i < K; i++) { pts[i].x = frange(-RX, RX); pts[i].y = frange(-RY, RY); }

    V2 hbuf[2*MAXSEED + 1];
    int h = hull_of(pts, K, hbuf);
    if (h < 3) return gen_ellipse(ctrl, RX, RY);        // degenerate scatter → fall back

    float mind = (RX + RY) * 0.16f;                     // push crowded corners apart
    for (int pass = 0; pass < 4; pass++)
        for (int i = 0; i < h; i++)
            for (int j = i+1; j < h; j++) {
                float dx = hbuf[j].x - hbuf[i].x, dy = hbuf[j].y - hbuf[i].y;
                float d = fsqrt(dx*dx + dy*dy);
                if (d > 0.01f && d < mind) {
                    float k = (mind - d) * 0.5f / d;
                    hbuf[i].x -= dx*k; hbuf[i].y -= dy*k;
                    hbuf[j].x += dx*k; hbuf[j].y += dy*k;
                }
            }

    // mark the m longest edges as the main straight(s)
    char keep[2*MAXSEED + 1]; for (int i = 0; i < h; i++) keep[i] = 0;
    int m = (int)(straight * h * 0.4f);
    for (int s = 0; s < m; s++) {
        int best = -1; float blen = -1;
        for (int i = 0; i < h; i++) {
            if (keep[i]) continue;
            float dx = hbuf[(i+1)%h].x - hbuf[i].x, dy = hbuf[(i+1)%h].y - hbuf[i].y;
            float L = dx*dx + dy*dy;
            if (L > blen) { blen = L; best = i; }
        }
        if (best >= 0) keep[best] = 1;
    }

    int nc = 0;
    for (int i = 0; i < h && nc < MAXCTRL - 3; i++) {
        V2 a = hbuf[i], b = hbuf[(i+1)%h];
        ctrl[nc++] = a;
        float ex = b.x - a.x, ey = b.y - a.y;
        float L = fsqrt(ex*ex + ey*ey); if (L < 0.01f) L = 1;
        if (keep[i]) {                                  // main straight: plain midpoint
            ctrl[nc].x = a.x + ex*0.5f; ctrl[nc++].y = a.y + ey*0.5f;
            continue;
        }
        float px = -ey/L, py = ex/L;                    // perpendicular
        float base = L * (0.10f + twist*0.28f);
        float vf = 1.0f + variety * (frand()*2 - 1) * 1.3f; if (vf < 0.15f) vf = 0.15f;
        float disp = base * vf * (frand() < 0.5f ? -1.0f : 1.0f);
        float cap = L * 0.48f; if (disp > cap) disp = cap; if (disp < -cap) disp = -cap;
        if (frand() < chicane*0.6f && L > RX*0.25f) {   // chicane: a quick S-bend
            float s = disp * 0.6f;
            ctrl[nc].x = a.x + ex*0.34f + px*s; ctrl[nc++].y = a.y + ey*0.34f + py*s;
            ctrl[nc].x = a.x + ex*0.66f - px*s; ctrl[nc++].y = a.y + ey*0.66f - py*s;
        } else {
            ctrl[nc].x = a.x + ex*0.5f + px*disp; ctrl[nc++].y = a.y + ey*0.5f + py*disp;
        }
    }
    return nc;
}

// drivability clamp: relax only corners too sharp to drive (pull toward the
// neighbour midpoint). Leaves genuine hairpins/sweepers alone; kills cusps.
static void relax_drivability(V2 *ctrl, int nc) {
    float smooth = S->lev[LV_SMOOTH];
    float maxturn = 150.0f - smooth*40.0f;              // 150..110° before we relax
    int passes = 2 + (int)(smooth * 4.0f);
    for (int pass = 0; pass < passes; pass++)
        for (int i = 0; i < nc; i++) {
            V2 p0 = ctrl[(i-1+nc)%nc], p1 = ctrl[i], p2 = ctrl[(i+1)%nc];
            float hin  = atan2_deg(p1.y - p0.y, p1.x - p0.x);
            float hout = atan2_deg(p2.y - p1.y, p2.x - p1.x);
            float turn = adiff(hout, hin);
            if (turn > maxturn) {
                float t = (turn - maxturn) / 90.0f; if (t > 1) t = 1; t *= 0.5f;
                ctrl[i].x = lerp(p1.x, (p0.x + p2.x)*0.5f, t);
                ctrl[i].y = lerp(p1.y, (p0.y + p2.y)*0.5f, t);
            }
        }
}


// lane geometry: lane centre as a lateral offset from the centre line, and the
// reverse — which lane a lateral offset falls in. Lanes tile the road full width.
static float lane_offset(int lane) { return (lane + 0.5f) * S->lane_w - (float)S->half; }
static int   lane_at(float lateral) {
    int l = (int)((lateral + (float)S->half) / S->lane_w);
    if (l < 0) l = 0; if (l >= S->nlanes) l = S->nlanes - 1;
    return l;
}

// place car `slot` on the starting grid: slot 0 (player) on the line, the rest
// staggered behind it, alternating sides — a real grid, so nobody starts stacked.
static void put_car_at_start(Car *c, int slot) {
    int s  = (NSAMP - slot * 5) % NSAMP;               // further back per slot
    float lateral = ((slot & 1) ? 1.0f : -1.0f) * S->half * 0.45f;
    c->px = S->cl[s].x + S->nl[s].x * lateral;
    c->py = S->cl[s].y + S->nl[s].y * lateral;
    c->ang = atan2_deg(S->cl[(s+1)%NSAMP].y - S->cl[(s-1+NSAMP)%NSAMP].y,
                       S->cl[(s+1)%NSAMP].x - S->cl[(s-1+NSAMP)%NSAMP].x);
    c->vx = 0; c->vy = 0; c->spd = 0;
    c->prog = s; c->cp = 0; c->lap = 0; c->offtrack = false;
    c->drift = false; c->wob = (float)slot * 7.0f; c->place = slot + 1;
    c->mass = (slot == 0) ? 1.0f : PERS[c->pers].mass;   // player baseline; AI by personality
    c->lane = c->want_lane = lane_at(lateral);
}

// traffic placement: spread cars evenly all the way around the loop, one per lane
// in rotation, already cruising — so the road looks alive the instant you arrive.
static void put_car_in_traffic(Car *c, int slot) {
    int s = (slot * NSAMP) / S->ncars;
    int lane = slot % S->nlanes;
    float loff = lane_offset(lane);
    c->px = S->cl[s].x + S->nl[s].x * loff;
    c->py = S->cl[s].y + S->nl[s].y * loff;
    c->ang = atan2_deg(S->cl[(s+1)%NSAMP].y - S->cl[(s-1+NSAMP)%NSAMP].y,
                       S->cl[(s+1)%NSAMP].x - S->cl[(s-1+NSAMP)%NSAMP].x);
    float v0 = (slot == 0) ? 0.0f : MAX_SPD * PERS[c->pers].spd_cap * 0.55f;
    c->spd = v0; c->vx = dx(v0, c->ang); c->vy = dy(v0, c->ang);
    c->prog = s; c->cp = 0; c->lap = 0; c->offtrack = false;
    c->drift = false; c->wob = (float)slot * 7.0f; c->place = slot + 1;
    c->mass = (slot == 0) ? 1.0f : PERS[c->pers].mass;
    c->lane = c->want_lane = lane;
}

static void put_all_at_start(void) {
    S->ncars = S->traffic ? TRAFFIC_CARS : RACE_CARS;
    if (S->ncars > CARS_MAX) S->ncars = CARS_MAX;
    for (int i = 0; i < S->ncars; i++) {
        if (i > 0) S->car[i].pers = (i - 1) % AI_COUNT;  // cycle personalities for the crowd
        if (S->traffic) put_car_in_traffic(&S->car[i], i);
        else            put_car_at_start(&S->car[i], i);
        for (int k = 0; k < REACT_N; k++) S->car[i].thist[k] = 0.0f;
        S->car[i].thead = 0;
    }
    // TRAFFIC: a single traffic light half a lap from the start (a clean bottleneck),
    // starting green. RACE: no light.
    S->light = S->traffic ? (NSAMP / 2) : -1;
    S->light_t = 0; S->light_red = false;
    S->camx = P.px; S->camy = P.py;
    S->finished = false; S->result_place = 0;
    for (int i = 0; i < MAXSKID; i++) S->skid[i].life = 0;
    timer_reset();
}

static void gen_track(unsigned int seed) {
    S->seed = seed;
    g_rng = seed ? seed : 1u;
    for (int i = 0; i < 8; i++) frand();

    float RX = 200 + S->lev[LV_SIZE] * 340.0f;
    float RY = 150 + S->lev[LV_SIZE] * 250.0f;
    S->half  = (int)(9 + S->lev[LV_WIDTH] * 16.0f);

    V2 ctrl[MAXCTRL];
    int nc, mode = style_mode(S->style);
    if      (mode == 3) nc = gen_fourier(ctrl, RX, RY);
    else if (mode == 2) nc = gen_oval(ctrl, RX, RY);
    else if (mode == 1) nc = gen_hull(ctrl, RX, RY);
    else                nc = gen_ellipse(ctrl, RX, RY);
    if (mode != 2) relax_drivability(ctrl, nc);         // oval is already drivable
    if (nc < 3) nc = gen_ellipse(ctrl, RX, RY);
    S->nctrl = nc;

    float tens = 0.5f - S->lev[LV_STRAIGHT] * 0.42f;    // long straights = low tension
    for (int s = 0; s < NSAMP; s++) {
        float t = (float)s / NSAMP * nc;
        int seg = (int)t; float u = t - seg;
        V2 p0 = ctrl[(seg-1+nc)%nc], p1 = ctrl[seg%nc],
           p2 = ctrl[(seg+1)%nc],    p3 = ctrl[(seg+2)%nc];
        S->cl[s].x = card(p0.x, p1.x, p2.x, p3.x, u, tens);
        S->cl[s].y = card(p0.y, p1.y, p2.y, p3.y, u, tens);
    }

    // start line on the most-UPWARD-facing sample → gas drives up into the track
    int s0 = 0; float best_up = -1e9f;
    for (int s = 0; s < NSAMP; s++) {
        V2 a = S->cl[(s-1+NSAMP)%NSAMP], b = S->cl[(s+1)%NSAMP];
        float tx = b.x - a.x, ty = b.y - a.y;
        float l = fsqrt(tx*tx + ty*ty); if (l < 0.001f) l = 1;
        float up = -ty / l;
        if (up > best_up) { best_up = up; s0 = s; }
    }
    if (s0 != 0) {
        V2 tmp[NSAMP];
        for (int s = 0; s < NSAMP; s++) tmp[s] = S->cl[(s0 + s) % NSAMP];
        for (int s = 0; s < NSAMP; s++) S->cl[s] = tmp[s];
    }
    for (int s = 0; s < NSAMP; s++) {
        V2 a = S->cl[(s-1+NSAMP)%NSAMP], b = S->cl[(s+1)%NSAMP];
        float tx = b.x - a.x, ty = b.y - a.y;
        float l = fsqrt(tx*tx + ty*ty); if (l < 0.001f) l = 1;
        S->nl[s].x = -ty / l; S->nl[s].y = tx / l;
    }

    // lanes + sample spacing (for traffic mode car-following)
    S->nlanes = (int)(2.0f * S->half / LANE_MIN);
    if (S->nlanes < 1) S->nlanes = 1;
    if (S->nlanes > LANES_MAX) S->nlanes = LANES_MAX;
    S->lane_w = 2.0f * S->half / S->nlanes;            // band ≥ LANE_MIN ≥ 2·CAR_R
    float per = 0;
    for (int s = 0; s < NSAMP; s++) {
        float dx = S->cl[(s+1)%NSAMP].x - S->cl[s].x, dy = S->cl[(s+1)%NSAMP].y - S->cl[s].y;
        per += fsqrt(dx*dx + dy*dy);
    }
    S->seg_len = per / NSAMP;

    S->best = 0; S->last = 0;
    put_all_at_start();
}

static void apply_style(int style) {
    S->style = style;
    for (int i = 0; i < NLEV; i++) S->lev[i] = PRESET[style][i];
    gen_track(S->seed);
}

void init(void) {
    S->style = ST_GRANDPRIX;
    for (int i = 0; i < NLEV; i++) S->lev[i] = PRESET[ST_GRANDPRIX][i];
    S->mode = 0;
    gen_track(0x1234u);
    if (S->eng == 0) { S->eng = note_on(28, INSTR_SAW, 0); note_glide(S->eng, 40); }
}

static void lay_skid(float x, float y) {
    S->skid[S->skid_head % MAXSKID] = (Skid){ (int)x, (int)y, 150 };
    S->skid_head++;
}

// ── the shared physics — player and AI both run this; only turn_in/throttle
//    differ. throttle in [-1,1]: +gas, -brake/reverse. ────────────────────────
static void step_car(Car *c, float turn_in, float throttle, bool drift) {
    float sc = c->spd / MAX_SPD; if (sc < 0) sc = -sc;
    c->ang += turn_in * STEER * sc;

    if (throttle > 0) c->spd += ACCEL * throttle;
    else              c->spd += BRAKE * throttle;        // throttle<0 brakes
    c->spd *= FRICTION;
    if (c->offtrack) { c->spd *= GRASS_DRAG; c->spd = clamp(c->spd, -GRASS_MAX, GRASS_MAX); }
    c->spd = clamp(c->spd, REV_MAX, MAX_SPD);

    float g = drift ? GRIP_DRIFT : GRIP_RACE;
    c->vx = lerp(c->vx, dx(c->spd, c->ang), g);
    c->vy = lerp(c->vy, dy(c->spd, c->ang), g);
    c->px += c->vx; c->py += c->vy;
    c->drift = drift;

    int bi = c->prog; float bd = 1e9f;                   // nearest sample = progress
    for (int o = -18; o <= 18; o++) {
        int i = (c->prog + o + NSAMP) % NSAMP;
        float ex = c->px - S->cl[i].x, ey = c->py - S->cl[i].y;
        float d = ex*ex + ey*ey;
        if (d < bd) { bd = d; bi = i; }
    }
    c->prog = bi;
    c->offtrack = fsqrt(bd) > (float)S->half;

    // current lane (from lateral offset) — emergent, so the player participates in
    // traffic too: an AI behind you in your lane will follow and brake for you.
    float lateral = (c->px - S->cl[bi].x) * S->nl[bi].x + (c->py - S->cl[bi].y) * S->nl[bi].y;
    c->lane = lane_at(lateral);

    // checkpoints & laps — by QUADRANT of prog (not a spatial gate point): a car
    // must pass through the four quarter-arcs in order, so an offset racing line
    // or a clipped apex can't miss the count the way a point-radius gate does.
    int q = (c->prog * 4) / NSAMP;                       // 0..3, which quarter-arc
    int want = (c->cp + 1) % 4;
    if (q == want) {
        c->cp = want;
        if (want == 0) {                                 // completed a lap
            if (c == &P) {
                float t = timer();
                if (c->lap > 0) { S->last = t; if (S->best == 0 || t < S->best) S->best = t; }
                timer_reset();
            }
            c->lap++;
            if (c == &P) note(72 + (c->lap % 5) * 2, INSTR_TRI, 4);
        }
    }

    float lat = c->vx * dx(1, c->ang + 90) + c->vy * dy(1, c->ang + 90);
    float as = c->spd < 0 ? -c->spd : c->spd;
    if ((lat > SLIDE_MIN || lat < -SLIDE_MIN) && as > 1.0f) {    // any car that slides
        float rx = c->px + dx(-5, c->ang), ry = c->py + dy(-5, c->ang);
        lay_skid(rx + dx(3, c->ang + 90), ry + dy(3, c->ang + 90));
        lay_skid(rx + dx(3, c->ang - 90), ry + dy(3, c->ang - 90));
    }
}

// ── the entire AI: read the line ahead, steer at it, brake for the bend. The
//    six personalities are PERS[] rows — same code, different numbers. ─────────
static void drive_ai(Car *c, int idx, float *out_turn, float *out_thr, bool *out_drift) {
    const Pers *p = &PERS[c->pers];

    // 1. steering target — a point look-ahead on the line, biased across the track
    int ti = (c->prog + (int)p->la) % NSAMP;
    float tx = S->cl[ti].x + S->nl[ti].x * p->line_off * S->half;
    float ty = S->cl[ti].y + S->nl[ti].y * p->line_off * S->half;
    if (p->wobble > 0) {                                 // rookie: a wandering target
        c->wob += 4.7f;
        float w = sin_deg(c->wob) * p->wobble * S->half;
        tx += S->nl[ti].x * w; ty += S->nl[ti].y * w;
    }
    if (p->aggro > 0) {                                  // hunter: chase the player when near
        float rx = P.px - c->px, ry = P.py - c->py, d = fsqrt(rx*rx + ry*ry);
        if (d < 130.0f) { float k = p->aggro * (1.0f - d / 130.0f);
            tx = lerp(tx, P.px, k); ty = lerp(ty, P.py, k); }
    }
    float desired = atan2_deg(ty - c->py, tx - c->px);
    float turn = clamp(awrap(desired - c->ang) / 8.0f, -1.0f, 1.0f);

    // 2. braking — read the bend `corner_la` samples ahead → a target speed
    int ca = (c->prog + (int)p->corner_la) % NSAMP;
    float h0 = atan2_deg(S->cl[(c->prog+4)%NSAMP].y - S->cl[c->prog].y,
                         S->cl[(c->prog+4)%NSAMP].x - S->cl[c->prog].x);
    float h1 = atan2_deg(S->cl[(ca+4)%NSAMP].y - S->cl[ca].y,
                         S->cl[(ca+4)%NSAMP].x - S->cl[ca].x);
    float bendn = fabsf_(awrap(h1 - h0)) / 90.0f; if (bendn > 1) bendn = 1;
    float cap = MAX_SPD * p->spd_cap * (1.0f - p->brake_g * bendn);
    if (p->rubber > 0) {                                 // rubber-band: catch up when behind
        long mine = (long)c->lap * NSAMP + c->prog, pl = (long)P.lap * NSAMP + P.prog;
        if (mine < pl) cap *= (1.0f + p->rubber);
    }
    if (cap < 0.7f) cap = 0.7f;
    float thr = (c->spd < cap) ? 1.0f : -0.55f;

    // 3. car-to-car avoidance — veer off a car just ahead so they overtake, not stack
    for (int j = 0; j < S->ncars; j++) {
        if (j == idx) continue;
        Car *o = &S->car[j];
        float rx = o->px - c->px, ry = o->py - c->py;
        if (rx*rx + ry*ry > COLL_R*COLL_R) continue;
        if (rx * dx(1, c->ang) + ry * dy(1, c->ang) <= 0) continue;   // only what's ahead
        float side = rx * dx(1, c->ang + 90) + ry * dy(1, c->ang + 90);
        turn += (side > 0 ? -0.7f : 0.7f);
    }

    *out_turn  = clamp(turn, -1.0f, 1.0f);
    *out_thr   = thr;
    *out_drift = (p->drift_corner > 0 && bendn > p->drift_corner);
}

// nearest car AHEAD in the same lane: returns the along-track gap (px) and the
// leader's speed. 1e9 if the lane is clear ahead.
static float lead_gap(Car *c, int idx, float *vlead) {
    int best = NSAMP; *vlead = MAX_SPD;
    for (int j = 0; j < S->ncars; j++) {
        if (j == idx) continue;
        Car *o = &S->car[j];
        if (o->lane != c->lane) continue;
        int d = (o->prog - c->prog + NSAMP) % NSAMP;     // forward index distance
        if (d > 0 && d < best) { best = d; *vlead = o->spd < 0 ? -o->spd : o->spd; }
    }
    if (best >= NSAMP) return 1e9f;
    return best * S->seg_len;                            // index distance → world px
}

// is `lane` open around this car's position — clear enough ahead AND behind to merge?
// (looks at where cars are AND where they're heading, so two cars don't pick the same gap)
static bool lane_clear(Car *c, int idx, int lane) {
    for (int j = 0; j < S->ncars; j++) {
        if (j == idx) continue;
        Car *o = &S->car[j];
        if (o->lane != lane && o->want_lane != lane) continue;
        int fwd  = (o->prog - c->prog + NSAMP) % NSAMP;
        int back = (c->prog - o->prog + NSAMP) % NSAMP;
        if (fwd < 22 || back < 10) return false;         // someone too close to merge in front of/behind
    }
    return true;
}

// TRAFFIC brain: hold your lane, follow the car ahead (IDM-style), and OVERTAKE —
// when stuck behind a slower car, pull into an adjacent clear lane to pass. A
// following car brakes to a stop but never reverses.
static void drive_ai_traffic(Car *c, int idx, float *out_turn, float *out_thr, bool *out_drift) {
    float v  = c->spd < 0 ? -c->spd : c->spd;
    float v0 = MAX_SPD * PERS[c->pers].spd_cap * 0.68f;  // city speed, not race pace

    // lift for the bend ahead (a car still has to make the corner)
    int ca = (c->prog + 22) % NSAMP;
    float h0 = atan2_deg(S->cl[(c->prog+4)%NSAMP].y - S->cl[c->prog].y,
                         S->cl[(c->prog+4)%NSAMP].x - S->cl[c->prog].x);
    float h1 = atan2_deg(S->cl[(ca+4)%NSAMP].y - S->cl[ca].y,
                         S->cl[(ca+4)%NSAMP].x - S->cl[ca].x);
    float bendn = fabsf_(awrap(h1 - h0)) / 90.0f; if (bendn > 1) bendn = 1;
    v0 *= 1.0f - 0.6f * bendn;
    if (v0 < 1.2f) v0 = 1.2f;

    // car-following: how close is the leader in my current lane, and how fast?
    float vlead, s = lead_gap(c, idx, &vlead);

    // OVERTAKE: blocked by a slower CAR (not a light) and not mid-bend? pull into a
    // clear lane. Decided on the real leader — you can't lane-change around a red.
    float ssafe0 = TG_GAP0 + v * TG_HEAD;
    if (S->nlanes > 1 && bendn < 0.4f && s < ssafe0 * 1.4f && vlead < v0 * 0.85f) {
        for (int dl = -1; dl <= 1; dl += 2) {
            int nl = c->want_lane + dl;
            if (nl >= 0 && nl < S->nlanes && lane_clear(c, idx, nl)) { c->want_lane = nl; break; }
        }
    }

    // a RED light ahead is a stopped "leader" parked at the stop line (it spans all
    // lanes), so cars queue behind it — the bottleneck that breeds stop-and-go waves.
    if (S->light >= 0 && S->light_red) {
        int ld = (S->light - c->prog + NSAMP) % NSAMP;
        float lgap = ld * S->seg_len;
        if (lgap < s) { s = lgap; vlead = 0.0f; }
    }

    float ssafe = TG_GAP0 + v * TG_HEAD;
    float dv = v - vlead;                                // closing speed (+ = approaching)
    if (dv > 0) ssafe += dv * dv / (2.0f * TG_BRAKE);

    // steer toward the intended lane's centre line (look-ahead). If we've slid off,
    // aim at the nearest bare centre point — a direct pull back onto the road.
    int ti = (c->prog + (c->offtrack ? 4 : TRAF_LA)) % NSAMP;
    float loff = c->offtrack ? 0.0f : lane_offset(c->want_lane);
    float tx = S->cl[ti].x + S->nl[ti].x * loff;
    float ty = S->cl[ti].y + S->nl[ti].y * loff;
    float desired = atan2_deg(ty - c->py, tx - c->px);
    float turn = clamp(awrap(desired - c->ang) / 7.0f, -1.0f, 1.0f);

    // throttle: cruise toward v0, but keep the safe gap to the leader/light
    float thr = (v < v0) ? 1.0f : -0.15f;
    if (s < ssafe) {                                     // too close → brake, deeper = harder
        float over = (ssafe - s) / (ssafe + 1.0f);
        thr = -clamp(over * 1.6f, 0.0f, 1.0f);
    } else if (s < ssafe * 1.8f && v >= v0) {
        thr = -0.1f;
    }

    // REACTION LAG: act on the command from REACT_N frames ago. This delay is what
    // makes dense following UNSTABLE — tiny gaps amplify backward into phantom jams.
    c->thist[c->thead % REACT_N] = thr;
    c->thead++;
    float out = c->thist[c->thead % REACT_N];            // oldest entry = REACT_N frames old

    // NEVER REVERSE. The clamp must use the speed RIGHT NOW (not when the command was
    // decided), or a stale brake from before the car stopped would back it up — the
    // light/queue oscillation. Allow braking only down to a dead stop this frame.
    if (out < 0.0f) {
        float min_thr = -c->spd / BRAKE;                 // brake that lands exactly at spd 0
        if (out < min_thr) out = min_thr;
    }

    *out_thr   = out;
    *out_turn  = turn;
    *out_drift = false;
}

// live race order: place 1 = furthest along (laps then progress)
static void update_standings(void) {
    for (int i = 0; i < S->ncars; i++) {
        long pi = (long)S->car[i].lap * NSAMP + S->car[i].prog;
        int place = 1;
        for (int j = 0; j < S->ncars; j++) {
            if (j == i) continue;
            long pj = (long)S->car[j].lap * NSAMP + S->car[j].prog;
            if (pj > pi) place++;
        }
        S->car[i].place = place;
    }
}

// car-to-car collision: after everyone has moved, push overlapping cars apart and
// trade momentum along the contact normal. Equal masses, so each takes half. A
// hard hit bleeds a little speed (and shakes/clacks if the player is in it) — you
// can shove a rival wide into a corner, and they can do it to you.
static void resolve_collisions(void) {
    const float MIN = 2.0f * CAR_R;
    for (int a = 0; a < S->ncars; a++)
        for (int b = a + 1; b < S->ncars; b++) {
            Car *A = &S->car[a], *B = &S->car[b];
            float dx = B->px - A->px, dy = B->py - A->py;
            float d2 = dx*dx + dy*dy;
            if (d2 >= MIN*MIN) continue;                     // not touching
            float d = fsqrt(d2), nx, ny;
            if (d < 1e-3f) { nx = 1.0f; ny = 0.0f; d = 0.0f; }  // exactly stacked → pick an axis
            else           { nx = dx/d; ny = dy/d; }
            float pen = MIN - d;

            // distribute by MASS: each car yields in proportion to the OTHER's weight,
            // so HUNTER (heavy) barely budges and SUNDAY (light) gets flung.
            float imA = 1.0f / A->mass, imB = 1.0f / B->mass, inv = imA + imB;
            A->px -= nx*pen*(imA/inv); A->py -= ny*pen*(imA/inv);
            B->px += nx*pen*(imB/inv); B->py += ny*pen*(imB/inv);

            float vn = (B->vx - A->vx)*nx + (B->vy - A->vy)*ny;
            if (vn < 0) {                                    // closing → trade momentum
                float j = -(1.0f + BUMP_REST) * vn / inv;    // impulse magnitude
                A->vx -= j*imA*nx; A->vy -= j*imA*ny;        // lighter car gets the bigger kick
                B->vx += j*imB*nx; B->vy += j*imB*ny;
                if (-vn > 0.8f) {                            // a real bump, not a graze
                    A->spd *= BUMP_BLEED; B->spd *= BUMP_BLEED;
                    if (a == 0 || b == 0) {                  // player involved → feel it
                        S->shake = clamp(-vn * 1.6f, S->shake, 4.0f);
                        hit(rnd_between(20, 28), INSTR_NOISE, 3, 90);
                    }
                }
            }
        }
}

#ifdef DE_TRACE
static float latd(int i) {                               // lateral distance from centre line (px)
    int pg = S->car[i].prog;
    float ex = S->car[i].px - S->cl[pg].x, ey = S->car[i].py - S->cl[pg].y;
    return fsqrt(ex*ex + ey*ey);
}
#endif

void update(void) {
    if (S->mode == 0) { note_vol(S->eng, 0); return; }   // panel handled in draw()
    if (S->mode == 2) {                                  // race results — frozen
        if (keyp('R') || btnp(0, BTN_A)) { put_all_at_start(); S->mode = 1; }
        if (keyp('M'))                   { S->mode = 0; }
        note_vol(S->eng, 0);
        return;
    }

    if (keyp('M'))                   { S->mode = 0; return; }
    if (keyp('Z') || btnp(0, BTN_A)) { gen_track(S->seed + 0x9E37u); return; }
    if (keyp('R'))                   { put_all_at_start(); return; }
    if (btnp(0, BTN_B)) P.drift = !P.drift;

    if (S->traffic && S->light >= 0) {                   // cycle the light (green longer than red)
        if (++S->light_t >= (S->light_red ? 200 : 360)) { S->light_t = 0; S->light_red = !S->light_red; }
    }

    float p_turn = (btn(0, BTN_RIGHT) ? 1.0f : 0.0f) - (btn(0, BTN_LEFT) ? 1.0f : 0.0f);
    float p_thr  = (btn(0, BTN_UP)    ? 1.0f : 0.0f) - (btn(0, BTN_DOWN) ? 1.0f : 0.0f);
    step_car(&P, p_turn, p_thr, P.drift);

    for (int i = 1; i < S->ncars; i++) {                    // the AI — race rivals or traffic
        float turn, thr; bool dr;
        if (S->traffic) drive_ai_traffic(&S->car[i], i, &turn, &thr, &dr);
        else            drive_ai(&S->car[i], i, &turn, &thr, &dr);
        step_car(&S->car[i], turn, thr, dr);
    }
    resolve_collisions();
    S->shake *= 0.82f;
    if (!S->traffic) update_standings();

    S->camx = lerp(S->camx, P.px + P.vx * CAM_LEAD, CAM_LERP);
    S->camy = lerp(S->camy, P.py + P.vy * CAM_LEAD, CAM_LERP);

    float as = P.spd < 0 ? -P.spd : P.spd;
    static int gravel_t = 0;
    if (P.offtrack && as > 0.5f) {
        float rx = P.px + dx(-5, P.ang), ry = P.py + dy(-5, P.ang);
        lay_skid(rx + dx(rnd_float_between(-3, 3), P.ang + 90),
                 ry + dy(rnd_float_between(-3, 3), P.ang + 90));
        if (--gravel_t <= 0) { hit(rnd_between(26, 34), INSTR_NOISE, 2, 70); gravel_t = 4; }
    } else gravel_t = 0;
    for (int i = 0; i < MAXSKID; i++) if (S->skid[i].life > 0) S->skid[i].life--;

    note_pitch(S->eng, 30.0f + (as / MAX_SPD) * 22.0f - (P.offtrack ? 7.0f : 0.0f));
    note_vol(S->eng, as / MAX_SPD * 3.0f);

    if (!S->traffic && !S->finished && P.lap >= RACE_LAPS) {   // chequered flag (race only)
        S->finished = true;
        S->result_place = P.place;
        S->mode = 2;
        note(72 - clamp(S->result_place - 1, 0, 4) * 3, INSTR_TRI, 30);
    }

#ifdef DE_TRACE
    watch("laps", "%d %d %d %d %d %d %d", S->car[0].lap, S->car[1].lap,
          S->car[2].lap, S->car[3].lap, S->car[4].lap, S->car[5].lap, S->car[6].lap);
    watch("prog", "%d %d %d %d %d %d %d", S->car[0].prog, S->car[1].prog,
          S->car[2].prog, S->car[3].prog, S->car[4].prog, S->car[5].prog, S->car[6].prog);
    watch("pl_off", "place=%d off=%d", P.place, P.offtrack);
    watch("off", "%d %d %d %d %d %d %d", S->car[0].offtrack, S->car[1].offtrack,
          S->car[2].offtrack, S->car[3].offtrack, S->car[4].offtrack, S->car[5].offtrack, S->car[6].offtrack);
    watch("spd", "%.1f %.1f %.1f %.1f %.1f %.1f %.1f", S->car[0].spd, S->car[1].spd,
          S->car[2].spd, S->car[3].spd, S->car[4].spd, S->car[5].spd, S->car[6].spd);
    watch("lane", "%d %d %d %d %d %d %d", S->car[0].lane, S->car[1].lane,
          S->car[2].lane, S->car[3].lane, S->car[4].lane, S->car[5].lane, S->car[6].lane);
    watch("lat", "%.0f %.0f %.0f %.0f %.0f %.0f %.0f h=%d",
          latd(0), latd(1), latd(2), latd(3), latd(4), latd(5), latd(6), S->half);
#endif
}

static void draw_grass(int ox, int oy) {
    cls(CLR_DARK_GREEN);
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            unsigned h = (unsigned)(x + ox)*374761393u + (unsigned)(y + oy)*668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            if ((h & 31u) == 0) pset(x, y, (h & 64u) ? CLR_MEDIUM_GREEN : CLR_GREEN);
        }
}

// one car, in world space. Player gets its red body + blue cockpit + a tag dot.
// RACE: rivals show their personality colour. TRAFFIC: cars are tinted by SPEED
// (red = stopped → green = flowing) so stop-and-go waves are visible at a glance.
static void draw_car(Car *c, bool is_player) {
    int body;
    if (is_player)       body = c->offtrack ? CLR_DARK_RED : CLR_RED;
    else if (S->traffic) {
        float v = c->spd < 0 ? -c->spd : c->spd;
        body = v < 0.3f ? CLR_RED : v < 0.9f ? CLR_ORANGE : v < 1.7f ? CLR_YELLOW : CLR_LIME_GREEN;
    } else               body = PERS[c->pers].color;
    float fx = dx(6, c->ang),         fy = dy(6, c->ang);
    float sx = dx(3.5f, c->ang + 90), sy = dy(3.5f, c->ang + 90);
    quadfill((int)(c->px+fx+sx),(int)(c->py+fy+sy), (int)(c->px+fx-sx),(int)(c->py+fy-sy),
             (int)(c->px-fx-sx),(int)(c->py-fy-sy), (int)(c->px-fx+sx),(int)(c->py-fy+sy), body);
    float cx = dx(1, c->ang), cy = dy(1, c->ang);
    float kx = dx(2.2f, c->ang + 90), ky = dy(2.2f, c->ang + 90);
    quadfill((int)(c->px+cx*3+kx),(int)(c->py+cy*3+ky), (int)(c->px+cx*3-kx),(int)(c->py+cy*3-ky),
             (int)(c->px-cx*2-kx),(int)(c->py-cy*2-ky), (int)(c->px-cx*2+kx),(int)(c->py-cy*2+ky),
             is_player ? CLR_TRUE_BLUE : CLR_BLACK);
    if (is_player)                                       // a beacon so you find yourself
        circ((int)c->px, (int)c->py, 9, CLR_WHITE);
}

static void draw_track_world(void) {
    int hw = S->half;
    float L = S->camx - SCREEN_W/2.0f - hw, R = S->camx + SCREEN_W/2.0f + hw;
    float T = S->camy - SCREEN_H/2.0f - hw, B = S->camy + SCREEN_H/2.0f + hw;
    for (int s = 0; s < NSAMP; s++) {
        int t = (s + 1) % NSAMP;
        if ((S->cl[s].x < L && S->cl[t].x < L) || (S->cl[s].x > R && S->cl[t].x > R) ||
            (S->cl[s].y < T && S->cl[t].y < T) || (S->cl[s].y > B && S->cl[t].y > B)) continue;
        float lx0 = S->cl[s].x + S->nl[s].x*hw, ly0 = S->cl[s].y + S->nl[s].y*hw;
        float rx0 = S->cl[s].x - S->nl[s].x*hw, ry0 = S->cl[s].y - S->nl[s].y*hw;
        float lx1 = S->cl[t].x + S->nl[t].x*hw, ly1 = S->cl[t].y + S->nl[t].y*hw;
        float rx1 = S->cl[t].x - S->nl[t].x*hw, ry1 = S->cl[t].y - S->nl[t].y*hw;
        quadfill((int)lx0,(int)ly0,(int)rx0,(int)ry0,(int)rx1,(int)ry1,(int)lx1,(int)ly1, CLR_DARK_GREY);
        int curb = (s / 3) & 1 ? CLR_RED : CLR_WHITE;
        line((int)lx0,(int)ly0,(int)lx1,(int)ly1, curb);
        line((int)rx0,(int)ry0,(int)rx1,(int)ry1, curb);
    }
    for (int s = 0; s < NSAMP; s += 6)
        line((int)S->cl[s].x, (int)S->cl[s].y,
             (int)S->cl[(s+3)%NSAMP].x, (int)S->cl[(s+3)%NSAMP].y, CLR_LIGHT_YELLOW);
    {
        float nx = S->nl[0].x, ny = S->nl[0].y;
        for (int j = -hw; j < hw; j += 3) {
            int col = ((j / 3) & 1) ? CLR_WHITE : CLR_BLACK;
            line((int)(S->cl[0].x + nx*j),     (int)(S->cl[0].y + ny*j),
                 (int)(S->cl[0].x + nx*(j+3)), (int)(S->cl[0].y + ny*(j+3)), col);
        }
    }
    for (int i = 0; i < MAXSKID; i++)
        if (S->skid[i].life > 0)
            pset(S->skid[i].x, S->skid[i].y,
                 S->skid[i].life > 60 ? CLR_BROWNISH_BLACK : CLR_DARKER_GREY);

    if (S->light >= 0) {                                 // traffic light: bold stop-line + signal heads
        int col = S->light_red ? CLR_RED : CLR_LIME_GREEN;
        for (int d = -1; d <= 1; d++) {                  // 3 samples thick = a visible stripe
            int g = (S->light + d + NSAMP) % NSAMP;
            float nx = S->nl[g].x, ny = S->nl[g].y;
            for (int w = -hw; w <= hw; w++)
                pset((int)(S->cl[g].x + nx*w), (int)(S->cl[g].y + ny*w), col);
        }
        int g = S->light; float nx = S->nl[g].x, ny = S->nl[g].y;   // a signal head each side
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            int hx = (int)(S->cl[g].x + nx*sgn*(hw+5)), hy = (int)(S->cl[g].y + ny*sgn*(hw+5));
            circfill(hx, hy, 4, CLR_BLACK);
            circfill(hx, hy, 3, col);
        }
    }

    for (int i = 1; i < S->ncars; i++) draw_car(&S->car[i], false);   // rivals first
    draw_car(&P, true);                                            // player on top
}

// fixed-scale ribbon sketch so SIZE grows/shrinks and WIDTH shows as thickness
static void draw_preview(int bx, int by, int bw, int bh) {
    const float MAXR = 540.0f;
    float sc  = (bw - 14) / (2.0f * MAXR);
    float oxp = bx + bw/2.0f, oyp = by + bh/2.0f;
    float hw  = (float)S->half;
    rrectfill(bx, by, bw, bh, 4, CLR_DARK_GREEN);
    clip(bx, by, bw, bh);
    for (int s = 0; s < NSAMP; s++) {
        int t = (s + 1) % NSAMP;
        float ax = oxp + (S->cl[s].x + S->nl[s].x*hw)*sc, ay = oyp + (S->cl[s].y + S->nl[s].y*hw)*sc;
        float bx2= oxp + (S->cl[s].x - S->nl[s].x*hw)*sc, by2= oyp + (S->cl[s].y - S->nl[s].y*hw)*sc;
        float cx2= oxp + (S->cl[t].x - S->nl[t].x*hw)*sc, cy2= oyp + (S->cl[t].y - S->nl[t].y*hw)*sc;
        float dx2= oxp + (S->cl[t].x + S->nl[t].x*hw)*sc, dy2= oyp + (S->cl[t].y + S->nl[t].y*hw)*sc;
        quadfill((int)ax,(int)ay,(int)bx2,(int)by2,(int)cx2,(int)cy2,(int)dx2,(int)dy2, CLR_DARK_GREY);
    }
    circfill((int)(oxp + S->cl[0].x*sc), (int)(oyp + S->cl[0].y*sc), 2, CLR_YELLOW);
    clip(0, 0, 0, 0);
}

static void draw_setup(void) {
    cls(CLR_BLACK);
    print_outline("TRACK GENERATOR", 8, 5, CLR_YELLOW, CLR_BLACK);

    ui_begin();
    // style selector: ◀  NAME  ▶
    if (ui_button(8, 16, 14, 12, "\x11")) apply_style((S->style + ST_COUNT - 1) % ST_COUNT);
    if (ui_button(146, 16, 14, 12, "\x10")) apply_style((S->style + 1) % ST_COUNT);
    print_centered(STYLE_NAME[S->style], 84, 18, CLR_LIGHT_PEACH);

    static const char *names[NLEV] = { "size", "width", "corners", "variety",
                                       "twist", "chicanes", "straights", "smooth" };
    int changed = 0;
    for (int i = 0; i < NLEV; i++)
        if (ui_slider(&S->lev[i], 8, 32 + i*13, 150, names[i])) changed = 1;
    if (changed) gen_track(S->seed);

    if (ui_button(8, 140, 72, 14, "RESEED"))
        gen_track(S->seed * 1664525u + 1013904223u);
    if (ui_button(84, 140, 74, 14, S->traffic ? "MODE:TRAFFIC" : "MODE:RACE"))
        S->traffic = !S->traffic;
    int go = ui_button(8, 158, 150, 18, S->traffic ? "\x10 DRIVE TRAFFIC" : "\x10 RACE");
    ui_end();

    draw_preview(170, 16, 142, 152);
    print(str("seed #%u", S->seed % 100000u), 170, 172, CLR_MEDIUM_GREY);

    if (go || keyp(KEY_ENTER) || keyp(KEY_SPACE)) { S->mode = 1; gen_track(S->seed); }
}

void draw(void) {
    if (S->mode == 0) { draw_setup(); return; }

    int ox = (int)(S->camx - SCREEN_W/2.0f), oy = (int)(S->camy - SCREEN_H/2.0f);
    if (S->shake > 0.2f) {                               // collision kick
        ox += (int)rnd_float_between(-S->shake, S->shake);
        oy += (int)rnd_float_between(-S->shake, S->shake);
    }
    camera(0, 0);
    draw_grass(ox, oy);
    camera(ox, oy);
    draw_track_world();
    camera(0, 0);

    // top bar
    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    if (S->traffic) {
        print("TRAFFIC", 4, 2, CLR_LIGHT_PEACH);
        print(str("%d cars", S->ncars), 64, 2, CLR_WHITE);
        print(str("lane %d/%d", P.lane + 1, S->nlanes), 122, 2, CLR_YELLOW);
        print(S->light_red ? "RED" : "GRN", 184, 2, S->light_red ? CLR_RED : CLR_LIME_GREEN);
        print(str("#%u", S->seed % 100000u), 220, 2,
              P.drift ? CLR_ORANGE : CLR_MEDIUM_GREY);
    } else {
        print(str("P%d/%d", P.place, S->ncars), 4, 2,
              P.place == 1 ? CLR_LIGHT_YELLOW : CLR_WHITE);
        print(str("lap %d/%d", P.lap < RACE_LAPS ? P.lap + 1 : RACE_LAPS, RACE_LAPS),
              44, 2, CLR_YELLOW);
        print(str("%.1fs", timer()), 102, 2, CLR_LIGHT_YELLOW);
        if (S->best > 0) print(str("best %.1f", S->best), 148, 2, CLR_GREEN);
        print(str("#%u", S->seed % 100000u), 210, 2,
              P.drift ? CLR_ORANGE : CLR_MEDIUM_GREY);

        // standings sidebar (live order, one row per car) — race only
        font(FONT_SMALL);
        for (int slot = 1; slot <= S->ncars; slot++)
            for (int i = 0; i < S->ncars; i++)
                if (S->car[i].place == slot) {
                    bool me = (i == 0);
                    int y = 12 + (slot - 1) * 7;
                    print(str("%d", slot), SCREEN_W - 56, y, CLR_MEDIUM_GREY);
                    print(me ? "YOU" : PERS[S->car[i].pers].name, SCREEN_W - 46, y,
                          me ? CLR_WHITE : PERS[S->car[i].pers].color);
                    break;
                }
        font(FONT_NORMAL);
    }

    print(S->traffic ? "M: generator   Z: new track   R: reset   X: drift"
                     : "M: generator   Z: new track   R: restart   X: drift",
          4, SCREEN_H - 9, P.offtrack ? CLR_ORANGE : CLR_LIGHT_GREY);

    // ── race results overlay ──
    if (S->mode == 2) {
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);   // (drawn opaque over the frozen frame)
        int pl = S->result_place;
        const char *head = pl == 1 ? "YOU WIN!" : pl <= 3 ? "PODIUM!" : "FINISHED";
        print_outline(head, SCREEN_W/2 - 28, 28, pl == 1 ? CLR_LIGHT_YELLOW : CLR_WHITE, CLR_BLACK);
        print_centered(str("%d%s of %d", pl,
                       pl==1?"st":pl==2?"nd":pl==3?"rd":"th", S->ncars),
                       SCREEN_W/2, 44, CLR_LIGHT_PEACH);
        int y = 64;
        for (int slot = 1; slot <= S->ncars; slot++)
            for (int i = 0; i < S->ncars; i++)
                if (S->car[i].place == slot) {
                    bool me = (i == 0);
                    print_centered(str("%d.  %s", slot, me ? "YOU" : PERS[S->car[i].pers].name),
                                   SCREEN_W/2, y, me ? CLR_WHITE : PERS[S->car[i].pers].color);
                    y += 11; break;
                }
        print_centered("R: race again    M: generator", SCREEN_W/2, y + 8, CLR_MEDIUM_GREY);
    }
}

#ifdef DE_SPEC
#include "spec.h"
// The gameplay gate: the AI rivals must actually navigate the loop, and the
// personality knobs must produce the intended speed order. Deterministic — the
// default track (seed 0x1234) and the fixed PERS[] table mean the same outcome
// every run. spec() shares this translation unit, so it drives the race straight
// (mode/grid normally come from draw(), which step() skips).
void spec(void) {
    step(1);                               // triggers init(): track built, mode 0
    put_all_at_start();
    S->mode = 1;                           // start the race
    step(1800);

    Car *pro = &S->car[1], *hunter = &S->car[2], *rookie = &S->car[5];
    Car *sunday = &S->car[4];              // the cautious one (pers AI_SUNDAY)

    expect(pro->lap    >= 1, "PRO completes at least one lap");
    expect(hunter->lap >= 1, "HUNTER completes at least one lap");
    expect(rookie->lap >= 1, "ROOKIE completes at least one lap");

    // every rival makes real forward progress — even cautious SUNDAY is moving,
    // not stuck (half a lap's distance is a generous floor for the slowest car)
    for (int i = 1; i < S->ncars; i++) {
        long adv = (long)S->car[i].lap * NSAMP + S->car[i].prog;
        expect(adv > NSAMP / 2, "rival advances at least half a lap of distance");
    }

    // the speed-cap knob bites: PRO out-distances the cautious SUNDAY
    long pro_d = (long)pro->lap * NSAMP + pro->prog;
    long sun_d = (long)sunday->lap * NSAMP + sunday->prog;
    expect(pro_d > sun_d, "PRO out-distances SUNDAY (speed cap differentiates)");

    // standings are a permutation 1..S->ncars (no duplicate or missing places)
    int seen = 0;
    for (int i = 0; i < S->ncars; i++) seen |= 1 << (S->car[i].place - 1);
    expect_eq(seen, (1 << S->ncars) - 1, "places form a clean 1..N ranking");

    // collision: stack two cars on the same spot, step, and they must separate
    put_all_at_start();
    S->mode = 1;
    S->car[1].px = S->car[2].px; S->car[1].py = S->car[2].py;
    S->car[1].vx = S->car[1].vy = S->car[2].vx = S->car[2].vy = 0;
    step(2);
    float sx = S->car[1].px - S->car[2].px, sy = S->car[1].py - S->car[2].py;
    expect(sx*sx + sy*sy > 16.0f, "collision pushes overlapping cars apart");

    // per-personality mass: in the SAME contact, heavy HUNTER yields less ground
    // than light SUNDAY. Isolate the pair (park everyone else far apart) and call
    // the resolver directly — no AI, fully deterministic.
    put_all_at_start();
    for (int i = 0; i < S->ncars; i++) {
        S->car[i].px = -9000.0f - i * 100.0f; S->car[i].py = -9000.0f;
        S->car[i].vx = S->car[i].vy = 0;
    }
    Car *hv = &S->car[2], *lt = &S->car[4];          // HUNTER (heavy) vs SUNDAY (light)
    hv->px = 100.0f; hv->py = 100.0f;
    lt->px = 105.0f; lt->py = 100.0f;                // overlapping along +x
    float hx0 = hv->px, lx0 = lt->px;
    resolve_collisions();
    expect(hv->mass > lt->mass, "HUNTER outweighs SUNDAY");
    expect((hx0 - hv->px) < (lt->px - lx0), "heavy HUNTER yields less ground than light SUNDAY");

    // ── TRAFFIC mode: lane-keeping + car-following ──
    S->traffic = true;
    put_all_at_start();              // spreads cars round the loop, one per lane
    S->mode = 1;
    step(1500);                      // let the flow settle

    // car-following prevents pile-ups: no two cars overlap after settling
    float minsep2 = 1e18f;
    for (int i = 0; i < S->ncars; i++)
        for (int j = i + 1; j < S->ncars; j++) {
            float ddx = S->car[i].px - S->car[j].px, ddy = S->car[i].py - S->car[j].py;
            float d2 = ddx*ddx + ddy*ddy;
            if (d2 < minsep2) minsep2 = d2;
        }
    expect(minsep2 > 64.0f, "traffic keeps a gap — no pile-ups (min sep > 8px)");

    // the road is flowing, not gridlocked: at least one car is moving with purpose
    float vmax = 0;
    for (int i = 1; i < S->ncars; i++) {
        float v = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
        if (v > vmax) vmax = v;
    }
    expect(vmax > 0.5f, "traffic flows (a car is moving, not jammed to a stop)");

    // ── traffic light: a RED forms a queue of stopped cars just behind the line ──
    put_all_at_start();
    S->mode = 1;
    S->light_red = true; S->light_t = 0;     // force red (red phase = 200f; we step < that)
    step(180);
    int queued = 0; float worst = 1e9f;
    for (int i = 0; i < S->ncars; i++) {
        int d = (S->light - S->car[i].prog + NSAMP) % NSAMP;   // samples until the light
        float v = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
        if (d < 16 && v < 0.4f) queued++;     // stopped, just short of the line
        if (S->car[i].spd < worst) worst = S->car[i].spd;
    }
    expect(queued >= 1, "traffic light: a red builds a queue of stopped cars");
    expect(worst > -0.05f, "no reversing: cars stop at the light, never roll backward");

    // ── phantom jam: density + reaction lag → stop-and-go (speeds spread out), not
    //    a uniform convoy. Measure the speed spread across the fleet after settling. ──
    float vmin = 1e9f, vmx = 0;
    for (int i = 0; i < S->ncars; i++) {
        float v = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
        if (v < vmin) vmin = v; if (v > vmx) vmx = v;
    }
    expect(vmx - vmin > 0.8f, "stop-and-go: a real speed spread exists (jam + free flow coexist)");

    // ── the controller, tested directly (no track noise). Reaction lag delays the
    //    output, so prime the pipeline (REACT_N+1 calls) before reading the command. ──
    S->traffic = true; put_all_at_start();
    S->light = -1;                           // isolate from the light
    Car *F = &S->car[1], *L = &S->car[2];
    float turn, thr; bool dr;

    F->prog = 100; L->prog = 108; L->lane = 0;     // leader ~8 samples (small gap) ahead
    F->lane = F->want_lane = 0; F->spd = MAX_SPD; L->spd = 0.4f; F->offtrack = false;
    for (int k = 0; k <= REACT_N; k++) drive_ai_traffic(F, 1, &turn, &thr, &dr);
    expect(thr < 0.0f, "car-following: closing on a near leader → brake");

    L->prog = 300; F->spd = 0.4f;                  // leader far away → clear road
    for (int k = 0; k <= REACT_N; k++) drive_ai_traffic(F, 1, &turn, &thr, &dr);
    expect(thr > 0.0f, "car-following: clear road → accelerate to desired speed");

    // overtaking: blocked by a slow leader with a clear neighbour lane → change lanes.
    put_all_at_start(); S->light = -1;
    if (S->nlanes > 1) {
        for (int i = 0; i < S->ncars; i++) { S->car[i].prog = 9000; S->car[i].lane = S->car[i].want_lane = 9; }
        F = &S->car[1]; L = &S->car[2];
        F->prog = 200; F->lane = F->want_lane = 0; F->spd = MAX_SPD; F->offtrack = false;
        L->prog = 210; L->lane = L->want_lane = 0; L->spd = 0.3f;       // slow blocker just ahead
        drive_ai_traffic(F, 1, &turn, &thr, &dr);
        expect(F->want_lane != 0, "overtaking: blocked by a slow leader → pull into a clear lane");
    }
}
#endif
