/* de:meta
{
  "slug": "trackgen",
  "title": "trackgen (kart racing)",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "game"
  ],
  "teaches": [
    "procedural-mesh",
    "camera-follow",
    "no-sprite-vehicles",
    "steering-behaviors",
    "car-following"
  ],
  "lineage": "Descends from the classic procedural racetrack algorithm (convex hull + perpendicular displacement + cardinal spline) with Fourier-loop and lemniscate variants; the driving layer is the steer.c car model, and the AI rivals are a parameter-driven follow-controller (one brain, six personality rows) designed to port to sloop's open world.",
  "genre": "racing",
  "description": "A procedural kart/race-track generator you can drive. A SETUP PANEL picks a STYLE — Grand Prix, Technical, Circuit, Speedway, Rally, Classic, Figure-8 — and eight levers (size, width, corners, variety, twist, chicanes, straights, smooth) reshape it with a live preview, then DRIVE. The hull styles come from the classic procedural-racetrack recipe, not a wobbly circle: scatter random points, take their CONVEX HULL (a guaranteed non-crossing loop), push crowded points apart, DISPLACE each edge midpoint perpendicular to carve the in-and-out concavities, then RELAX any corner too sharp to drive (the drivability clamp). CHICANES inject quick S-bends, the longest edge is kept as a MAIN STRAIGHT, and CORNER VARIETY mixes hairpins with sweepers. CIRCUIT is a different beast — a random FOURIER LOOP (the centre line is a sum of sinusoid harmonics) that winds back through the interior like a real kart track and, with twist cranked, crosses itself many times. A cardinal spline runs through the result (Speedway uses a stadium oval; Figure-8 blends an ellipse toward a lemniscate so it crosses itself once). Offset the spline ±half-width along its normal and the ribbon, curbs, racing line and chequered start/finish all fall out. The car is the steer model (speed-scaled steering + grip/drift) in a world up to ~3 screens wide, with a LOOK-AHEAD camera that leads the car by its velocity. Leave the asphalt and you bog down (gentle) with a gravel rumble. Each track has a SEED. LEFT/RIGHT steer, UP gas, DOWN brake, X drift, Z new track, R restart, M back to the panel. RACE six AI rivals over three laps. Every car — you and the rivals — runs ONE shared physics step; the rivals just feed it a steer/throttle from a tiny follow-controller instead of the keys. That controller IS the whole brain: look a few samples ahead on the centre line, steer at that point, and brake for the bend it reads further ahead. The six personalities are not six code paths — they are rows in a parameter table (look-ahead, braking margin, speed cap, line offset, wobble, drift threshold, aggression, rubber-band): PRO hugs the racing line, HUNTER chases you down when near, DRIFT slides wide through corners, SUNDAY pootles under a low speed cap, ROOKIE wanders and spins off, RUBBER catches up when it falls behind. Light car-to-car avoidance keeps them overtaking instead of stacking; live standings sit on the right, and finishing shows the final order. The same brain ports straight to sloop's open world — only the line it follows changes (the track centre line here, road_at() out there). Cars also COLLIDE: overlapping bodies push apart and trade momentum along the contact normal, so you can shove a rival wide into a corner and get shoved back — a hard hit bleeds speed and kicks the camera with a clack. Contact is mass-weighted per personality: HUNTER leans in (heavy, barely budges and flings you), while SUNDAY and ROOKIE yield (light, get knocked away) — so a collision expresses character too. TWO MODES (toggle in the setup panel): RACE, and TRAFFIC — same track, not a race. In traffic the AI holds a lane, follows the car ahead at a safe time-gap (IDM-style car-following), lifts for corners, and OVERTAKES — pulling into a clear adjacent lane to pass a slower car, so a car blocked behind you (even a stopped you) goes around instead of stacking up. Lanes are emergent from lateral position, so you participate: sit in a lane and traffic follows and passes you. Traffic gets a crowd of cars tinted by SPEED (red=stopped, green=flowing) so jams show; a cycling TRAFFIC LIGHT whose red is a stop-line all lanes queue behind; and a small reaction lag so dense following turns unstable and PHANTOM JAMS — stop-and-go waves with no cause — emerge on the loop, exactly the famous ring-road experiment."
}
de:meta */
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
// cause, exactly the ring-road experiment.
// TWO TRACKS (setup toggle "TRACKS:2"): a SECOND independent circuit (gen_track2 — same
// generator, a derived seed rotated 90°, same centre/size so they reliably overlap) that
// crosses the first. It is a full closed loop too; its cars (road = 1) just go round it via
// the SAME drive_ai_traffic brain — the only generalization is one Car.road field + a
// road_*() accessor (both wrap), the whole portability thesis. Crossings (xpt[], two loops
// cross an even number of times) are found by segment intersection (find_crossings), each
// recording its sample index on BOTH tracks. RIGHT-OF-WAY is a JUNCTION RESERVATION
// (update_reservations): each crossing is owned by at most ONE car at a time, so two cars
// can NEVER share a junction — no T-bones, by construction, not by tuning. A car claims a
// free crossing once within XAPPROACH (and only if its exit is clear, so it won't stall IN
// the box); the owner holds it until it has driven through, then frees it (a timeout breaks
// any stall). Everyone else stops short (crossing_yield_gap). The grant uses a CLAIM SCORE
// (soonest-to-arrive, biased by aggression = priority track + PERS.aggro) — the hook for
// "crazy vs careful" drivers. Junctions draw as a ring (orange = reserved); debug overlay
// (key D) draws a line from each owner to its junction, and pink "stuck" cars to their
// blocker. Design + remaining systems (speed zones, hazards, peds, green-wave):
// docs/design/traffic-ai.md.
// (Known rough edge: on the tightest procedural corner a fast car can still clip the
// apex and recover.)
//   LEFT/RIGHT steer · UP gas · DOWN brake · Z new track · R restart · M: setup panel.
//   In-drive: tap the pixel-art HUD buttons (bottom-right) — DEBUG (eye), DRIFT (skid),
//   CHASE (cop lights, two-tracks only); keys X/D/C still work. Icons live in trackgen.cart.js.
//   RACE: standings on the right + results on finish.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── car tuning — edit + hot-reload ──────────────────────────────────────────
#define ACCEL      0.07f
#define BRAKE      0.13f
#define REV_MAX   -1.0f
#define FRICTION   0.985f
#define MAX_SPD    3.1f
#define STEER      3.6f
#define KT_REACH   16.0f   // K-TURN: radius of the disk a 3-point-turning car stays within (keeps it local)
#define GRIP_RACE  0.25f
#define GRIP_DRIFT 0.07f
#define SLIDE_MIN  0.55f

#define GRASS_DRAG 0.965f
#define GRASS_MAX  1.8f

#define CAR_HALF_L 6.0f        // car collision half-extent along heading (long)
#define CAR_HALF_W 3.6f        // car collision half-extent across heading (narrow) — so side-by-side
                               // cars in adjacent lanes (centres ~16px apart) never falsely touch
#define BUMP_REST  0.35f       // restitution: 0 = dead thud, 1 = fully elastic bounce
#define BUMP_BLEED 0.90f       // speed retained by both cars after a hard contact

// ── traffic mode (free-flow lane driving — see docs/design/traffic-ai.md) ──
#define LANE_MIN   12.0f       // min lane band width (px) — keeps side-by-side cars off each other
#define LANES_MAX  3
#define TRAF_LA    12          // traffic steering look-ahead (samples)
#define TG_GAP0    20.0f       // car-following: standstill gap to the leader (px) — well clear of the 10px collision diameter
#define TG_HEAD    28.0f       // time headway (frames) — gap grows with speed; big enough to stop before touching
#define TG_BRAKE   0.05f       // comfortable decel used to size the safe gap

#define CAM_LEAD   16.0f
#define CAM_LERP   0.09f

#define NSAMP   360
#define MAXCTRL 80          // hull corners + displaced midpoints + chicane points
#define MAXSEED 24          // scattered points before the hull
#define MAXSKID 160

// ── second track (a closed loop crossing the first; cars on it have road = 1) ──
#define NSAMP2       240        // samples around the second track's loop
#define MAXX         16         // max track×track crossings stored (two loops can cross many times)
// ── right-of-way at the crossings (junction RESERVATION — one car owns a crossing
//    at a time, so two cars can never share it: crash-proof, not tuning-dependent) ──
#define XBOX_LOOK    300.0f     // how far ahead a car cares about a crossing (px)
#define XAPPROACH    120.0f     // a car claims a free crossing once this close (≥ its stopping distance,
                                // so a car that loses the claim still has room to halt at the line)
#define XHOLD_MAX    300        // frames an owner may hold a crossing before a forced release (anti-stall)

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
// a loop×cross crossing: the world point + the sample index on each road
// (prog1 = cl[] on the loop, prog2 = cl2[] on the cross-road). Phase C will read
// prog1/prog2 to size the yield "stop-line" on each road.
typedef struct { V2 p; int prog1, prog2; } Xing;

// ── racers ───────────────────────────────────────────────────────────────────
// One Car struct, one array. Car 0 is the player (reads btn()); cars 1..N are AI
// rivals (read drive_ai()). EVERY car runs the SAME physics (step_car) — the only
// difference between player and AI is who supplies turn_in/throttle each frame.
// That is the whole point: the brain built here ports to sloop unchanged; only the
// *line to follow* changes (cl[] here, road_at() in the open world later).
#define RACE_CARS    7         // RACE: 1 player + 6 AI, one per personality
#define TRAFFIC_CARS 32        // TRAFFIC: a big crowd on the loop — long queues behind a red
#define TRAFFIC_CARS_X 13      // TRAFFIC + CROSS: a thin loop crowd so arrivals don't outpace one-at-a-time junctions
#define TRAFFIC_CROSS 10       // TRAFFIC + CROSS: the second track's stream (kept light for the same reason)
#define CARS_MAX     48        // array bound (>= loop crowd + cross stream)
#define RACE_LAPS 3
#define COLL_R    16.0f        // car-to-car avoidance radius (world px)
#define REACT_N   3            // car-following reaction lag (frames) — seeds phantom jams
#define LIGHT_RED 480          // red phase (frames) — long, so a real line of cars builds
#define LIGHT_GRN 300          // green phase (frames)
// ── chase: a pursuer cuts off-road to intercept, then boxes the suspect ──
#define BEELINE_RANGE 220.0f   // this close, a pursuer drives STRAIGHT at the target (leaves the road if shorter)
#define LOCKIN_RANGE   75.0f   // this close, a cop stops tailing and parks in a slot against the suspect
#define LOCKIN_PARK    13.0f   // park-slot distance from the suspect (≈ a car length → it parks touching)
#define SCATTER_RANGE  46.0f   // a civilian within this of a reckless car pulls aside & brakes (scatter)

typedef struct {
    float px, py, vx, vy, spd, ang;
    bool  drift, offtrack;
    int   prog, cp, lap;       // prog = nearest sample on THIS car's road; cp = last checkpoint 0..3
    int   road;                // 0 = the loop (cl[]), 1 = the cross-road (cl2[]) — Phase B
    int   lastx;               // last crossing this car made a turn-decision at (-1) — the routing seed
    int   target;              // CHASE: car index this one is pursuing (-1 = none) — routes toward it
    bool  reckless;            // CHASE: ignores junction right-of-way and the light (runs them)
    signed char kt;            // K-TURN state: 0 none · 1 reversing · 2 forward (get-unstuck maneuver)
    float ktgoal;              // K-TURN: the heading (deg) it's swinging the nose toward (captured at entry)
    float ktx, kty;            // K-TURN: the pivot point — it stays within a small disk of this while turning
    int   pers;                // personality index into PERS[] (AI only)
    float wob;                 // rookie wobble phase
    float mass;                // collision weight (player 1.0; AI from PERS[].mass)
    int   place;               // live race position (1 = leading), recomputed each frame
    int   lane;                // current lane (computed from lateral offset each frame)
    int   want_lane;           // intended lane (traffic) — overtaking sets this; steering aims at it
    float thist[REACT_N];      // recent throttle commands — applied delayed (reaction lag)
    int   thead;               // ring head into thist[]
    int   why;                 // why it's braking this frame (WHY_*) — for the debug overlay
} Car;

// why a traffic car is slowing — surfaced by the debug overlay (key D). WHY_BLOCKED =
// it WANTS to go (throttle on) but isn't moving → physically wedged by another car.
enum { WHY_CRUISE, WHY_FOLLOW, WHY_LIGHT, WHY_YIELD, WHY_BLOCKED };

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
    bool  cross;               // PHASE A: draw a straight cross-road through the loop (geometry only — no cross-traffic yet)
    bool  reroute;             // ROUTING SEED: AI cars may turn onto the other track at a crossing
    V2    cl2[NSAMP2];         // cross-road centre line (straight L→R, world)
    V2    nl2[NSAMP2];         // cross-road unit normals (constant — straight road)
    float seg_len2;            // avg world distance between adjacent cl2[] samples (px)
    Xing  xpt[MAXX];           // loop×cross crossings (computed in gen_track)
    int   nx;                  // number of crossings found
    int   xowner[MAXX];        // car index that currently OWNS each crossing (-1 = free) — the reservation
    int   xtimer[MAXX];        // frames the current owner has held it (anti-stall timeout)
    int   ncross;              // active cross-road cars, in car[ncars .. ncars+ncross) (Phase B)
    bool  dbg;                 // debug overlay: show why each car is braking (key D)
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

// ── road accessor (Phase B) — the ONE generalization that lets the same brain
//    drive either road. A car reads its line ONLY through these, so step_car /
//    drive_ai_traffic don't care which road they're on (the portability thesis).
//    The loop (road 0) wraps; the cross-road (road 1) is finite, so its index
//    clamps at the ends. ───────────────────────────────────────────────────────
static int   road_n(int road)     { return road ? NSAMP2 : NSAMP; }
static bool  road_loops(int road) { (void)road; return true; }   // both tracks are closed loops
static int   road_idx(int road, int i) {
    int n = road_n(road);
    i %= n; if (i < 0) i += n; return i;
}
static V2    road_cl(int road, int i) { int j = road_idx(road, i); return road ? S->cl2[j] : S->cl[j]; }
static V2    road_nl(int road, int i) { int j = road_idx(road, i); return road ? S->nl2[j] : S->nl[j]; }
static float road_seg(int road)   { return road ? S->seg_len2 : S->seg_len; }
// signed forward distance (samples) from `from` to `to`: + ahead, − behind.
static int   prog_ahead(int road, int from, int to) {
    int n = road_n(road), d = to - from;
    if (road_loops(road)) { d %= n; if (d > n/2) d -= n; if (d < -n/2) d += n; }
    return d;
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
    c->prog = s; c->cp = 0; c->lap = 0; c->offtrack = false; c->road = 0; c->lastx = -1; c->target = -1; c->reckless = false; c->kt = 0;
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
    c->prog = s; c->cp = 0; c->lap = 0; c->offtrack = false; c->road = 0; c->lastx = -1; c->target = -1; c->reckless = false; c->kt = 0;
    c->drift = false; c->wob = (float)slot * 7.0f; c->place = slot + 1;
    c->mass = (slot == 0) ? 1.0f : PERS[c->pers].mass;
    c->lane = c->want_lane = lane;
}

// place a car on the SECOND track at sample `prog0`, in `lane`, already cruising
// along it. Same shape as put_car_in_traffic but on cl2[] (road = 1).
static void put_car_on_cross(Car *c, int prog0, int lane) {
    float loff = lane_offset(lane);
    V2 cc = road_cl(1, prog0), nn = road_nl(1, prog0);
    V2 ahead = road_cl(1, prog0 + 1), behind = road_cl(1, prog0 - 1);
    c->px = cc.x + nn.x * loff; c->py = cc.y + nn.y * loff;
    c->ang = atan2_deg(ahead.y - behind.y, ahead.x - behind.x);   // tangent along the loop
    float v0 = MAX_SPD * PERS[c->pers].spd_cap * 0.55f;
    c->spd = v0; c->vx = dx(v0, c->ang); c->vy = dy(v0, c->ang);
    c->prog = prog0; c->cp = 0; c->lap = 0; c->offtrack = false; c->road = 1; c->lastx = -1; c->target = -1; c->reckless = false; c->kt = 0;
    c->drift = false; c->wob = (float)lane * 7.0f; c->place = 0;
    c->mass = PERS[c->pers].mass;
    c->lane = c->want_lane = lane;
    for (int k = 0; k < REACT_N; k++) c->thist[k] = 0.0f;
    c->thead = 0;
}

#ifdef DE_SPEC
// place a car on the LOOP (road 0) at sample `prog0`, in `lane`, stopped — spec helper for
// the flow-around test (an arbitrary spot/lane, unlike put_car_in_traffic's slot grid).
static void put_car_on_loop(Car *c, int prog0, int lane) {
    float loff = lane_offset(lane);
    V2 cc = road_cl(0, prog0), nn = road_nl(0, prog0);
    V2 ahead = road_cl(0, prog0 + 1), behind = road_cl(0, prog0 - 1);
    c->px = cc.x + nn.x * loff; c->py = cc.y + nn.y * loff;
    c->ang = atan2_deg(ahead.y - behind.y, ahead.x - behind.x);
    c->spd = 0; c->vx = 0; c->vy = 0;
    c->prog = prog0; c->cp = 0; c->lap = 0; c->offtrack = false; c->road = 0;
    c->lastx = -1; c->target = -1; c->reckless = false; c->kt = 0; c->drift = false;
    c->lane = c->want_lane = lane;
    for (int k = 0; k < REACT_N; k++) c->thist[k] = 0.0f;
    c->thead = 0;
}
#endif

static void put_all_at_start(void) {
    S->ncars = S->traffic ? TRAFFIC_CARS : RACE_CARS;
    // With a cross-road active, thin the loop crowd so real gaps open for the give-way
    // road to take — otherwise a bumper-to-bumper priority road starves the crossing.
    if (S->traffic && S->cross && S->nx > 0 && S->ncars > TRAFFIC_CARS_X) S->ncars = TRAFFIC_CARS_X;
    if (S->ncars > CARS_MAX) S->ncars = CARS_MAX;
    for (int i = 0; i < S->ncars; i++) {
        if (i > 0) S->car[i].pers = (i - 1) % AI_COUNT;  // cycle personalities for the crowd
        if (S->traffic) put_car_in_traffic(&S->car[i], i);
        else            put_car_at_start(&S->car[i], i);
        for (int k = 0; k < REACT_N; k++) S->car[i].thist[k] = 0.0f;
        S->car[i].thead = 0;
    }
    S->reroute = S->traffic && S->cross;                 // routing seed on by default with two tracks
    // PHASE B: a second stream of cars on the cross-road (TRAFFIC + CROSS only),
    // packed into car[ncars .. ncars+ncross). They follow cl2[] via the same brain.
    S->ncross = (S->traffic && S->cross && S->nx > 0) ? TRAFFIC_CROSS : 0;
    if (S->ncars + S->ncross > CARS_MAX) S->ncross = CARS_MAX - S->ncars;
    for (int k = 0; k < S->ncross; k++) {
        int i = S->ncars + k;
        S->car[i].pers = k % AI_COUNT;                   // cycle personalities (varied desired speeds)
        put_car_on_cross(&S->car[i], (k * NSAMP2) / S->ncross, k % S->nlanes);   // spread round the loop
    }

    for (int k = 0; k < MAXX; k++) { S->xowner[k] = -1; S->xtimer[k] = 0; }   // clear junction reservations

    // TRAFFIC: a single traffic light half a lap from the start (a clean bottleneck),
    // starting green. RACE: no light.
    S->light = S->traffic ? (NSAMP / 2) : -1;
    S->light_t = 0; S->light_red = false;
    S->camx = P.px; S->camy = P.py;
    S->finished = false; S->result_place = 0;
    for (int i = 0; i < MAXSKID; i++) S->skid[i].life = 0;
    timer_reset();
}

// build a control loop for the chosen style at radius RX×RY (uses g_rng, so seed it first)
static int gen_ctrl(V2 *ctrl, float RX, float RY, int style) {
    int nc, mode = style_mode(style);
    if      (mode == 3) nc = gen_fourier(ctrl, RX, RY);
    else if (mode == 2) nc = gen_oval(ctrl, RX, RY);
    else if (mode == 1) nc = gen_hull(ctrl, RX, RY);
    else                nc = gen_ellipse(ctrl, RX, RY);
    if (mode != 2) relax_drivability(ctrl, nc);
    if (nc < 3) nc = gen_ellipse(ctrl, RX, RY);
    return nc;
}

// cardinal-spline a control loop into a centre line + its unit normals; returns the
// average sample spacing (world px). Shared by both tracks.
static float spline_road(const V2 *ctrl, int nc, V2 *cl, V2 *nl, int nsamp, float tens) {
    for (int s = 0; s < nsamp; s++) {
        float t = (float)s / nsamp * nc;
        int seg = (int)t; float u = t - seg;
        V2 p0 = ctrl[(seg-1+nc)%nc], p1 = ctrl[seg%nc],
           p2 = ctrl[(seg+1)%nc],    p3 = ctrl[(seg+2)%nc];
        cl[s].x = card(p0.x, p1.x, p2.x, p3.x, u, tens);
        cl[s].y = card(p0.y, p1.y, p2.y, p3.y, u, tens);
    }
    for (int s = 0; s < nsamp; s++) {
        V2 a = cl[(s-1+nsamp)%nsamp], b = cl[(s+1)%nsamp];
        float tx = b.x - a.x, ty = b.y - a.y;
        float l = fsqrt(tx*tx + ty*ty); if (l < 0.001f) l = 1;
        nl[s].x = -ty / l; nl[s].y = tx / l;
    }
    float per = 0;
    for (int s = 0; s < nsamp; s++) {
        float dx = cl[(s+1)%nsamp].x - cl[s].x, dy = cl[(s+1)%nsamp].y - cl[s].y;
        per += fsqrt(dx*dx + dy*dy);
    }
    return per / nsamp;
}

// all loop×loop intersections of cl[] and cl2[] (segment-segment), recording each
// crossing's world point + nearest sample index on BOTH tracks. Two closed loops
// cross an even number of times; nearby duplicates (adjacent segments) are merged.
static void find_crossings(void) {
    S->nx = 0;
    for (int k = 0; k < MAXX; k++) { S->xowner[k] = -1; S->xtimer[k] = 0; }
    for (int a = 0; a < NSAMP && S->nx < MAXX; a++) {
        V2 p1 = S->cl[a], p2 = S->cl[(a+1)%NSAMP];
        float d1x = p2.x-p1.x, d1y = p2.y-p1.y;
        for (int b = 0; b < NSAMP2 && S->nx < MAXX; b++) {
            V2 p3 = S->cl2[b], p4 = S->cl2[(b+1)%NSAMP2];
            float d2x = p4.x-p3.x, d2y = p4.y-p3.y;
            float den = d1x*d2y - d1y*d2x;
            if (den < 1e-5f && den > -1e-5f) continue;         // parallel
            float ox = p3.x-p1.x, oy = p3.y-p1.y;
            float t = (ox*d2y - oy*d2x) / den;
            float u = (ox*d1y - oy*d1x) / den;
            if (t < 0 || t > 1 || u < 0 || u > 1) continue;     // not within both segments
            float xx = p1.x + t*d1x, yy = p1.y + t*d1y;
            bool dup = false;                                   // merge a crossing found twice
            for (int k = 0; k < S->nx; k++) {
                float rx = xx-S->xpt[k].p.x, ry = yy-S->xpt[k].p.y;
                if (rx*rx + ry*ry < 100.0f) { dup = true; break; }
            }
            if (dup) continue;
            S->xpt[S->nx].p.x = xx; S->xpt[S->nx].p.y = yy;
            S->xpt[S->nx].prog1 = a; S->xpt[S->nx].prog2 = b;
            S->nx++;
        }
    }
}

// SECOND TRACK — an independent loop crossing the first. Same generator, a derived
// seed (a different shape) rotated 90° (so it reads as a distinct circuit, not a
// near-copy), same size/centre as track 1 so the two reliably overlap and cross.
// It is a full closed loop too — its cars just go round, same brain, road = 1.
static void gen_track2(void) {
    unsigned int saved = g_rng;
    g_rng = (S->seed * 2246822519u) ^ 0x9E3779B9u; if (!g_rng) g_rng = 1u;
    for (int i = 0; i < 8; i++) frand();
    float RX = 200 + S->lev[LV_SIZE] * 340.0f;
    float RY = 150 + S->lev[LV_SIZE] * 250.0f;
    V2 ctrl[MAXCTRL];
    int nc = gen_ctrl(ctrl, RX, RY, S->style);
    for (int i = 0; i < nc; i++) { float x = ctrl[i].x, y = ctrl[i].y; ctrl[i].x = -y; ctrl[i].y = x; }
    float tens = 0.5f - S->lev[LV_STRAIGHT] * 0.42f;
    S->seg_len2 = spline_road(ctrl, nc, S->cl2, S->nl2, NSAMP2, tens);
    g_rng = saved;
    find_crossings();
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
    S->lane_w = 2.0f * S->half / S->nlanes;            // band ≥ LANE_MIN > car width (no side-by-side touch)
    float per = 0;
    for (int s = 0; s < NSAMP; s++) {
        float dx = S->cl[(s+1)%NSAMP].x - S->cl[s].x, dy = S->cl[(s+1)%NSAMP].y - S->cl[s].y;
        per += fsqrt(dx*dx + dy*dy);
    }
    S->seg_len = per / NSAMP;

    gen_track2();                                      // the second crossing track + crossings (drawn only when S->cross)

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

    int rd = c->road;
    int bi = c->prog; float bd = 1e9f;                   // nearest sample on THIS car's road = progress
    for (int o = -18; o <= 18; o++) {
        int i = road_idx(rd, c->prog + o);
        V2 cc = rd ? S->cl2[i] : S->cl[i];
        float ex = c->px - cc.x, ey = c->py - cc.y;
        float d = ex*ex + ey*ey;
        if (d < bd) { bd = d; bi = i; }
    }
    c->prog = bi;
    c->offtrack = fsqrt(bd) > (float)S->half;

    // current lane (from lateral offset) — emergent, so the player participates in
    // traffic too: an AI behind you in your lane will follow and brake for you.
    V2 bc = road_cl(rd, bi), bn = road_nl(rd, bi);
    float lateral = (c->px - bc.x) * bn.x + (c->py - bc.y) * bn.y;
    c->lane = lane_at(lateral);

    // checkpoints & laps — by QUADRANT of prog (not a spatial gate point): a car
    // must pass through the four quarter-arcs in order, so an offset racing line
    // or a clipped apex can't miss the count the way a point-radius gate does. Only
    // the loop laps; the cross-road is a finite road, not a circuit.
    if (road_loops(rd)) {
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
    c->why = (thr < -0.02f) ? WHY_FOLLOW : WHY_CRUISE;   // race AI: braking ≈ for a bend/car
}

// nearest car AHEAD in the same lane: returns the along-track gap (px) and the
// leader's speed. 1e9 if the lane is clear ahead.
static float lead_gap(Car *c, int idx, float *vlead) {
    int rd = c->road, n = road_n(rd), best = n; *vlead = MAX_SPD;
    int total = S->ncars + S->ncross;
    for (int j = 0; j < total; j++) {
        if (j == idx) continue;
        Car *o = &S->car[j];
        if (o->road != rd || o->lane != c->lane) continue;   // only a leader on MY road, MY lane
        int d;
        if (road_loops(rd)) d = (o->prog - c->prog + n) % n; // forward index distance (wraps)
        else { d = o->prog - c->prog; if (d <= 0) continue; }// finite road: ahead = higher prog
        if (d > 0 && d < best) { best = d; *vlead = o->spd < 0 ? -o->spd : o->spd; }
    }
    if (best >= n) return 1e9f;
    return best * road_seg(rd);                          // index distance → world px
}

// is `lane` open around this car's position — clear enough ahead AND behind to merge?
// (looks at where cars are AND where they're heading, so two cars don't pick the same gap)
static bool lane_clear(Car *c, int idx, int lane) {
    int rd = c->road, total = S->ncars + S->ncross;
    for (int j = 0; j < total; j++) {
        if (j == idx) continue;
        Car *o = &S->car[j];
        if (o->road != rd) continue;                     // a car on the other road isn't in my lanes
        if (o->lane != lane && o->want_lane != lane) continue;
        int d = prog_ahead(rd, c->prog, o->prog);        // signed: + ahead, − behind
        if (d > -10 && d < 22) return false;             // too close to merge in front of/behind
    }
    return true;
}

// distance (px) a car is from a crossing along its OWN track: + ahead, − past it.
static float xing_dist(Car *c, int k) {
    int xprog = c->road ? S->xpt[k].prog2 : S->xpt[k].prog1;
    return prog_ahead(c->road, c->prog, xprog) * road_seg(c->road);
}

// JUNCTION RESERVATIONS — run once per frame, before the AI. Each crossing is owned by
// at most ONE car; only the owner may enter it, so two cars never share a junction (no
// T-bones, by construction — no tuning). A car claims a free crossing once it's within
// XAPPROACH (and only if its exit is clear, so it won't stall IN the box); the owner
// holds it until it has driven through, then it frees. A timeout breaks any stall.
//
// PERSONALITY HOOK (the "crazy vs careful" knob, ready to grow): the winner of a free
// crossing is the car with the best CLAIM SCORE = how soon it reaches the box, biased by
// aggression (an aggressive driver effectively arrives "sooner" and wins ties / noses in;
// a careful one defers). Today aggression = the priority track (road 0) + PERS.aggro; per
// driver you can later widen the bias, shorten a bold driver's stop gap, etc.
static void update_reservations(void) {
    int total = S->ncars + S->ncross;
    float Rocc = (float)S->half, occ = Rocc + CAR_HALF_L;
    for (int k = 0; k < S->nx; k++) {
        int o = S->xowner[k];
        if (o >= 0) {                                    // ── currently owned: release when cleared/stale
            if (o >= total) { S->xowner[k] = -1; continue; }
            float d = xing_dist(&S->car[o], k);
            S->xtimer[k]++;
            if (d < -(Rocc + CAR_HALF_L) || d > XBOX_LOOK || S->xtimer[k] > XHOLD_MAX) S->xowner[k] = -1;
            else continue;                               // still held
        }
        // the PLAYER is a manually-driven car — it parks, wanders, drives off, so it must never
        // HOLD a junction (it would starve everyone). While it sits physically in the box, the AI
        // treat it as an obstacle to ROUTE AROUND, not a wall: a car on the player's OWN road may
        // still claim the crossing and go around him in a parallel lane (the standstill go-around);
        // only CROSS-road cars must keep yielding — driving through would T-bone the parked player.
        bool player_in_box = false;
        { float prx = S->car[0].px - S->xpt[k].p.x, pry = S->car[0].py - S->xpt[k].p.y;
          if (prx*prx + pry*pry < occ*occ) player_in_box = true; }

        // ── free: grant to the best approaching AI candidate (claim score), if its exit is clear
        int best = -1; float bestscore = -1e9f;
        for (int i = 1; i < total; i++) {                // skip the player (i == 0): AI cars only
            Car *c = &S->car[i];
            if (player_in_box && c->road != S->car[0].road) continue;  // cross-road yields to the parked player
            float d = xing_dist(c, k);
            if (d <= 0 || d > XAPPROACH) continue;       // not approaching / not close enough yet
            // exit-clear: don't grant if a stopped car sits just past the junction (would stall in box)
            int xprog = c->road ? S->xpt[k].prog2 : S->xpt[k].prog1;
            V2 ep = road_cl(c->road, xprog + (int)((Rocc + 2.0f*CAR_HALF_L)/road_seg(c->road)) + 2);
            bool exit_blocked = false;
            for (int j = 0; j < total; j++) {
                if (j == i) continue;
                float sv = S->car[j].spd < 0 ? -S->car[j].spd : S->car[j].spd;
                if (sv > 0.6f) continue;
                float rx = S->car[j].px - ep.x, ry = S->car[j].py - ep.y;
                if (rx*rx + ry*ry < Rocc*Rocc) { exit_blocked = true; break; }
            }
            if (exit_blocked) continue;
            // claim score: nearer = higher; aggression (priority track + PERS.aggro) adds a bias
            float aggr = (c->road == 0 ? 30.0f : 0.0f) + (i > 0 ? PERS[c->pers].aggro * 20.0f : 0.0f);
            float score = -d + aggr;
            if (score > bestscore) { bestscore = score; best = i; }
        }
        if (best >= 0) { S->xowner[k] = best; S->xtimer[k] = 0; }
    }
}

// the crossing as a stop-line for the brain: if I'm approaching a crossing I do NOT own,
// stop short of it (return the gap). The owner gets 1e9 (proceed). This + the reservation
// above is the whole crash-proof junction: only the owner is ever inside the box.
static float crossing_yield_gap(Car *c, int idx) {
    if (!S->cross || S->nx == 0) return 1e9f;             // no second track → no junctions to yield at
    float Rstop = (float)S->half + 2.0f * CAR_HALF_L + 2.0f;
    float best = 1e9f;
    for (int i = 0; i < S->nx; i++) {
        if (S->xowner[i] == idx) continue;               // I own it → go
        float dme = xing_dist(c, i);
        if (dme <= Rstop || dme > XBOX_LOOK) continue;    // behind/committed, or too far to matter
        float gap = dme - Rstop;
        if (gap < best) best = gap;
    }
    return best;
}

// nearest squared distance from a world point to a track (coarse scan) — "how close does
// this loop pass to the target," used by chase routing to pick the target's loop.
static float road_min_dist2(int road, V2 p) {
    int n = road_n(road); float best = 1e18f;
    for (int s = 0; s < n; s += 3) {
        V2 q = (road ? S->cl2[s] : S->cl[s]);
        float dx = q.x - p.x, dy = q.y - p.y, d = dx*dx + dy*dy;
        if (d < best) best = d;
    }
    return best;
}

// ROUTING SEED — the two crossing loops are a tiny 2-edge network: at a crossing an AI car
// may TURN onto the other track instead of going straight. Called after step_car (so prog is
// fresh) for AI traffic cars only — the player never auto-switches (keeps control). The
// reservation already cleared the car to be in the box, so turning on the way out is safe;
// we just release the reservation we held and snap onto the other track.
static void maybe_switch_loops(Car *c, int idx) {
    if (!S->reroute || !S->cross || S->nx == 0) return;
    for (int k = 0; k < S->nx; k++) {
        int xprog = c->road ? S->xpt[k].prog2 : S->xpt[k].prog1;
        int d = prog_ahead(c->road, c->prog, xprog);     // signed samples to this crossing
        if (d > 0 || d < -2) continue;                   // only right as we pass the crossing sample
        if (c->lastx == k) return;                       // already decided this pass
        c->lastx = k;
        bool turn;
        if (c->target >= 0 && c->target < S->ncars + S->ncross) {
            // CHASE routing: get onto the loop the target is on, then drive forward to it. A car
            // only goes forward on a loop, so the only choice at a crossing is WHICH loop — pick
            // whichever passes nearest the target overall (then forward motion reaches it).
            V2 tg = (V2){ S->car[c->target].px, S->car[c->target].py };
            turn = road_min_dist2(!c->road, tg) < road_min_dist2(c->road, tg);
        } else {
            // ambient: ~3/8 of passes turn (deterministic per car+crossing+lap → varied, reproducible)
            unsigned h = (unsigned)idx*2654435761u + (unsigned)k*40503u + (unsigned)c->lap*2246822519u;
            turn = ((h >> 13) & 7u) < 3u;
        }
        if (turn) {
            if (S->xowner[k] == idx) S->xowner[k] = -1;  // release the junction I held (I'm leaving it)
            c->road = !c->road;
            c->prog = c->road ? S->xpt[k].prog2 : S->xpt[k].prog1;   // the crossing sample on the new track
            V2 a = road_cl(c->road, c->prog + 1), b = road_cl(c->road, c->prog - 1);
            c->ang = atan2_deg(a.y - b.y, a.x - b.x);     // face along the new track
        }
        return;
    }
}

// clear space in a box just off my nose / tail? (so a K-turn won't ram anyone). dir = +1 ahead, -1 behind.
static bool room_dir(Car *c, int idx, int dir) {
    int total = S->ncars + S->ncross;
    for (int j = 0; j < total; j++) {
        if (j == idx) continue;
        float rx = S->car[j].px - c->px, ry = S->car[j].py - c->py;
        float along = dir * (rx*dx(1, c->ang) + ry*dy(1, c->ang));     // + = in that direction
        float lat   = rx*dx(1, c->ang + 90) + ry*dy(1, c->ang + 90);
        if (along > -2.0f && along < 20.0f && (lat < 0 ? -lat : lat) < 11.0f) return false;
    }
    return true;
}

// K-TURN — the get-unstuck maneuver. A car can't turn in place (steering scales with speed) and the
// turn radius is speed-independent, so a badly-misaligned car would loop forward in a big circle.
// Instead, when genuinely pointed the wrong way (vs the ROAD TANGENT at its own spot — so a curve
// doesn't false-trigger), nearly stopped, with the road ahead CLEAR (it wants to go, not stopped
// for a light/leader), and away from junctions, it does a 3-point turn: alternate reverse/forward
// (whichever side is clear) while counter-steering toward a CAPTURED goal heading, until aligned.
// Returns true (and sets turn/thr) while it's k-turning. Never for pursuers or near a junction.
static bool k_turn(Car *c, int idx, float want_thr, float *out_turn, float *out_thr) {
    if (c->target >= 0) { c->kt = 0; return false; }            // pursuers don't k-turn
    for (int k = 0; k < S->nx; k++) {                            // not at/near a junction (would back into cross-traffic)
        float d = xing_dist(c, k); if (d < 0) d = -d;
        if (d < 80.0f) { c->kt = 0; return false; }
    }
    float v = c->spd < 0 ? -c->spd : c->spd;
    V2 ta = road_cl(c->road, c->prog + 2), tb = road_cl(c->road, c->prog - 2);
    float roaddir = atan2_deg(ta.y - tb.y, ta.x - tb.x);
    float herr = awrap(roaddir - c->ang); float ah = herr < 0 ? -herr : herr;

    // the maneuver sweeps a whole disk, so it needs room: no other car within the swept radius of the
    // pivot (current spot at entry; the captured pivot mid-turn). If a car's in the way, just WAIT —
    // never reverse into a crowd. This is what keeps a wrong-angled car from clipping cross-traffic.
    bool clear = true;
    { int total = S->ncars + S->ncross;
      for (int j = 0; j < total; j++) { if (j == idx) continue;
          float rx = S->car[j].px - c->px, ry = S->car[j].py - c->py;   // from my CURRENT body, not the pivot
          if (rx*rx + ry*ry < 24.0f * 24.0f) { clear = false; break; } } }

    if (c->kt == 0) {                                            // entry: genuinely wrong-way + clear ahead + slow + room
        if (ah <= 75.0f || want_thr <= 0.05f || v > 0.4f || !clear) return false;   // only a genuinely stopped, wrong-way car
        c->kt = 1; c->ktgoal = roaddir; c->ktx = c->px; c->kty = c->py;  // capture goal heading + pivot, start reversing
    }
    if (!clear) { c->kt = 0; return false; }   // someone wandered into my space → bail to normal driving (it obeys right-of-way)
    float gerr = awrap(c->ktgoal - c->ang); float ag = gerr < 0 ? -gerr : gerr;
    if (ag < 22.0f) { c->kt = 0; return false; }                // aligned → done, hand back to normal driving
    bool back = room_dir(c, idx, -1), fwd = room_dir(c, idx, +1);

    // CONFINE-TO-A-DISK. step_car rotates the nose by turn*STEER*|speed| — SAME direction whether
    // reversing or going forward (it uses |sc|) — so to swing the nose toward the goal we hold the
    // SAME steer sign on both legs. The trick to staying put (a 3-point turn, not a wide arc) is to
    // keep the car inside a small disk around its pivot: whenever it reaches the rim, pick the leg
    // (reverse vs forward) that heads back toward the pivot. So it shuffles across the disk diameter
    // — building enough speed each crossing to actually rotate — while the nose rotates over in place.
    float ddx = c->px - c->ktx, ddy = c->py - c->kty;
    float dpiv = fsqrt(ddx*ddx + ddy*ddy);
    if (dpiv > KT_REACH) {                                       // outside the disk → drive back toward the pivot
        float toPiv = (-ddx) * dx(1, c->ang) + (-ddy) * dy(1, c->ang);   // pivot ahead of the nose?
        c->kt = (toPiv > 0) ? 2 : 1;                            // yes → forward · behind → reverse
    }
    if (c->kt == 1 && !back) c->kt = fwd ? 2 : 1;               // chosen leg blocked → take the clear one
    if (c->kt == 2 && !fwd)  c->kt = back ? 1 : 2;
    float st = (gerr > 0 ? 1.0f : -1.0f);
    if (c->kt == 1 && back)      { *out_thr = -0.5f; *out_turn = st; }   // reverse leg
    else if (c->kt == 2 && fwd)  { *out_thr =  0.5f; *out_turn = st; }   // forward leg
    else                         { *out_thr =  0.0f; *out_turn = 0.0f; } // boxed → hold, wait for room
    return true;
}

// TRAFFIC brain: hold your lane, follow the car ahead (IDM-style), and OVERTAKE —
// when stuck behind a slower car, pull into an adjacent clear lane to pass. A
// following car brakes to a stop but never reverses. At a crossing, the give-way
// road yields to the priority road (crossing_yield_gap).
static void drive_ai_traffic(Car *c, int idx, float *out_turn, float *out_thr, bool *out_drift) {
    int rd = c->road;                                    // read the line via road_*; either road works
    float v  = c->spd < 0 ? -c->spd : c->spd;
    // CHASE: a pursuer drives near race pace, not city pace.
    float v0 = MAX_SPD * PERS[c->pers].spd_cap * (c->target >= 0 ? 1.05f : 0.68f);

    // lift for the bend ahead (a car still has to make the corner; a straight reads ~0)
    int ca = c->prog + 22;
    V2 a0 = road_cl(rd, c->prog), a1 = road_cl(rd, c->prog + 4);
    V2 b0 = road_cl(rd, ca),      b1 = road_cl(rd, ca + 4);
    float h0 = atan2_deg(a1.y - a0.y, a1.x - a0.x);
    float h1 = atan2_deg(b1.y - b0.y, b1.x - b0.x);
    float bendn = fabsf_(awrap(h1 - h0)) / 90.0f; if (bendn > 1) bendn = 1;
    v0 *= 1.0f - 0.6f * bendn;
    if (v0 < 1.2f) v0 = 1.2f;

    // car-following: how close is the leader in my current lane, and how fast?
    float vlead, s = lead_gap(c, idx, &vlead);
    int why = (s < 1e8f) ? WHY_FOLLOW : WHY_CRUISE;      // debug: what's limiting me (tracked as s shrinks)

    // OVERTAKE: blocked by a slower CAR (not a light)? pull into a clear lane. Decided on the
    // real leader — you can't lane-change around a red. Two cases:
    //   ROLLING — passing a slower car while still moving, only on a straight-ish stretch.
    //   JAMMED  — stopped dead behind the PARKED PLAYER (he sat down in the lane, maybe on a
    //             junction): GO AROUND him instead of waiting forever, even on a curve. Gated to
    //             "blocker IS the stopped player" so dense AI-vs-AI traffic + the junction
    //             reservation are completely untouched (that's where squeezing causes T-bones).
    float ssafe0 = TG_GAP0 + v * TG_HEAD;
    bool blocked_by_car = s < ssafe0 * 1.4f && vlead < v0 * 0.85f;
    bool player_blocks = false;                          // is the stopped player right in front, my lane?
    if (idx != 0) {
        Car *pl = &S->car[0]; float pv = pl->spd < 0 ? -pl->spd : pl->spd;
        if (pl->road == rd && pl->lane == c->lane && pv < 0.3f) {
            int dd = prog_ahead(rd, c->prog, pl->prog);
            if (dd > 0 && dd * road_seg(rd) < ssafe0 * 1.6f) player_blocks = true;
        }
    }
    bool rolling = v > 0.5f && bendn < 0.4f && blocked_by_car;
    bool jammed  = v < 0.6f && player_blocks;
    if (S->nlanes > 1 && (rolling || jammed)) {
        for (int dl = -1; dl <= 1; dl += 2) {
            int nl = c->want_lane + dl;
            if (nl >= 0 && nl < S->nlanes && lane_clear(c, idx, nl)) { c->want_lane = nl; break; }
        }
    }
    // JAMMED behind the player + committed to a lane change? follow the lane I'm ENTERING, not the
    // one I'm leaving, so the player dead ahead in my old lane stops pinning me (else I brake for
    // him forever and can never pull out — steering needs speed). Only this exact case is affected.
    if (jammed && c->want_lane != c->lane) {
        int save = c->lane; c->lane = c->want_lane;
        float vl2, s2 = lead_gap(c, idx, &vl2); c->lane = save;
        if (s2 > s) { s = s2; vlead = vl2; why = (s < 1e8f) ? WHY_FOLLOW : WHY_CRUISE; }
    }

    // a RED light ahead is a stopped "leader" parked at the stop line (it spans all
    // lanes), so cars queue behind it — the bottleneck that breeds stop-and-go waves.
    // The light lives on the loop, so only loop cars (road 0) see it. A reckless car runs it.
    if (S->light >= 0 && S->light_red && road_loops(rd) && !c->reckless) {
        int ld = (S->light - c->prog + NSAMP) % NSAMP;   // forward samples to the light
        if (ld > NSAMP - 6) ld = 0;                       // a tiny overshoot pins to the line —
                                                          // WITHOUT this the wrap reads ~a full lap
                                                          // away and the front car bolts the red
        if (ld <= NSAMP / 2) {                            // only brake when the light is ahead
            float lgap = ld * S->seg_len;
            if (lgap < s) { s = lgap; vlead = 0.0f; why = WHY_LIGHT; }
        }
    }

    // a crossing I must give way at is a temporary stop-line leader — unless I'm reckless (run it).
    if (!c->reckless) {
        float ygap = crossing_yield_gap(c, idx);
        if (ygap < s) { s = ygap; vlead = 0.0f; why = WHY_YIELD; }
    }

    float ssafe = TG_GAP0 + v * TG_HEAD;
    float dv = v - vlead;                                // closing speed (+ = approaching)
    if (dv > 0) ssafe += dv * dv / (2.0f * TG_BRAKE);

    // steer toward the intended lane's centre line (look-ahead). If we've slid off,
    // aim at the nearest bare centre point — a direct pull back onto the road.
    float loff = c->offtrack ? 0.0f : lane_offset(c->want_lane);
    V2 tc = road_cl(rd, c->prog + (c->offtrack ? 4 : TRAF_LA));
    V2 tn = road_nl(rd, c->prog + (c->offtrack ? 4 : TRAF_LA));
    float tx = tc.x + tn.x * loff;
    float ty = tc.y + tn.y * loff;
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
    if (s < TG_GAP0 && v < 0.8f) thr = -1.0f;            // snug behind the obstacle & slow → settle
                                                          // to a dead stop (no reaction-lag creep)

    // K-TURN: if I'm pointed the wrong way and stuck, do a 3-point turn instead of a big circle.
    if (k_turn(c, idx, thr, out_turn, out_thr)) { *out_drift = false; c->why = WHY_CRUISE; return; }

    // CHASE override: a pursuer that's closed in cuts STRAIGHT at the target (off-road if the road
    // would take the long way — it just eats the grass penalty), and when very close it BOXES the
    // suspect: aim ahead of it (cut off the escape) and to one flank, so two cops pincer it in.
    // Cops push (no polite gap). Far → beeline straight at the suspect (off-road allowed). Close →
    // PARK on the suspect: each cop parks at a slot on the side it's ALREADY approaching from (so it
    // never has to loop around — that was the circling), shedding speed first so it can actually stop
    // instead of overshooting. A small per-cop spread fans the two onto different sides → a box.
    if (c->target >= 0 && c->target < S->ncars + S->ncross) {
        Car *t = &S->car[c->target];
        float rx = t->px - c->px, ry = t->py - c->py;
        float dist = fsqrt(rx*rx + ry*ry);
        if (dist < BEELINE_RANGE) {
            bool boxing = dist < LOCKIN_RANGE;
            float ax = t->px, ay = t->py;
            if (boxing) {                                 // slot = R_park straight out toward where I'm coming
                float toCop = atan2_deg(c->py - t->py, c->px - t->px);   // from (radial → drive in & stop, no orbit)
                ax = t->px + dx(LOCKIN_PARK, toCop); ay = t->py + dy(LOCKIN_PARK, toCop);
            }
            float sdx = ax - c->px, sdy = ay - c->py, sdist = fsqrt(sdx*sdx + sdy*sdy);
            float des = atan2_deg(sdy, sdx);
            turn = clamp(awrap(des - c->ang) / 7.0f, -1.0f, 1.0f);
            if (!boxing) thr = 1.0f;                      // beeline: full chase
            else if (v > 1.2f)        thr = -1.0f;        // boxing: shed speed first so I won't overshoot…
            else if (sdist > 7.0f)    thr =  0.4f;        // …creep to the slot…
            else                      thr = -1.0f;        // …and park on it
        }
    }

    // AMBIENT SCATTER: a civilian flinches from a chase tearing past — when a reckless car (a
    // cop) is close, it pulls aside (steer toward the edge away from the cop) and lifts/brakes to
    // let the chase through. The "everyone reacts" moment, emergent from one nearby-reckless check.
    if (!c->reckless && c->target < 0) {
        int total = S->ncars + S->ncross;
        for (int j = 0; j < total; j++) {
            if (j == idx || !S->car[j].reckless) continue;
            float rx = S->car[j].px - c->px, ry = S->car[j].py - c->py;
            if (rx*rx + ry*ry > SCATTER_RANGE*SCATTER_RANGE) continue;
            float side = rx*dx(1, c->ang + 90) + ry*dy(1, c->ang + 90);   // cop on my + side?
            turn += (side > 0 ? -0.7f : 0.7f);                            // veer to the far side
            if (thr > -0.3f) thr = -0.3f;                                 // brake/yield, let it pass
            break;
        }
        turn = clamp(turn, -1.0f, 1.0f);
    }

    // REACTION LAG — but ASYMMETRIC. Braking is PROMPT (you hit the brakes the instant
    // you see danger); only ACCELERATION is sluggish (slow to get going again). Delaying
    // the brake is what rear-ended the car ahead; lagging only the throttle still gives
    // stop-and-go — a jam discharges slowly because cars dawdle speeding up, not braking.
    c->thist[c->thead % REACT_N] = thr;
    c->thead++;
    float delayed = c->thist[c->thead % REACT_N];        // command REACT_N frames ago
    float out = thr < delayed ? thr : delayed;           // the more cautious of now vs lagged

    // NEVER REVERSE. Clamp against the speed RIGHT NOW (a stale brake from before the car
    // stopped would otherwise back it up). Brake only down to a dead stop this frame.
    if (out < 0.0f) {
        float min_thr = -c->spd / BRAKE;
        if (out < min_thr) out = min_thr;
    }

    // classify for the overlay: braking on purpose → that reason; commanding throttle yet
    // barely moving → WEDGED (physically held by another car, can't go); else cruising.
    if (out < -0.02f)                            c->why = why;
    else if (out > 0.05f && v < 0.25f)           c->why = WHY_BLOCKED;
    else                                         c->why = WHY_CRUISE;
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

// car-to-car collision: cars are ORIENTED boxes (long along heading, narrow across),
// not circles — so two cars side-by-side in adjacent lanes don't falsely "touch" the
// way fat circles did. Overlap is tested in A's local frame; we separate along the
// axis of least penetration (a separating-axis resolve) and trade momentum there.
// Mass-weighted: HUNTER barely budges, SUNDAY gets flung. A hard hit clacks/shakes.
static void resolve_collisions(void) {
    const float RL = 2.0f * CAR_HALF_L, RW = 2.0f * CAR_HALF_W;   // sum of half-extents
    int total = S->ncars + S->ncross;                    // loop + cross cars: pairs across roads collide too
    for (int a = 0; a < total; a++)
        for (int b = a + 1; b < total; b++) {
            Car *A = &S->car[a], *B = &S->car[b];
            float rx = B->px - A->px, ry = B->py - A->py;
            if (rx*rx + ry*ry >= RL*RL) continue;            // far apart (cheap reject)

            float fx = dx(1, A->ang),      fy = dy(1, A->ang);       // A's forward
            float sx = dx(1, A->ang + 90), sy = dy(1, A->ang + 90);  // A's side
            float fwd = rx*fx + ry*fy, lat = rx*sx + ry*sy;
            float pf = RL - (fwd < 0 ? -fwd : fwd);          // overlap along / across
            float pl = RW - (lat < 0 ? -lat : lat);
            if (pf <= 0 || pl <= 0) continue;                // boxes don't overlap

            float nx, ny, pen;                               // separate along least overlap
            if (pl < pf) { float g = lat < 0 ? -1.0f : 1.0f; nx = sx*g; ny = sy*g; pen = pl; }
            else         { float g = fwd < 0 ? -1.0f : 1.0f; nx = fx*g; ny = fy*g; pen = pf; }

            // distribute by MASS: each car yields in proportion to the OTHER's weight.
            float imA = 1.0f / A->mass, imB = 1.0f / B->mass, inv = imA + imB;
            A->px -= nx*pen*(imA/inv); A->py -= ny*pen*(imA/inv);
            B->px += nx*pen*(imB/inv); B->py += ny*pen*(imB/inv);

            float vn = (B->vx - A->vx)*nx + (B->vy - A->vy)*ny;
            if (vn < 0) {                                    // closing → trade momentum
                float j = -(1.0f + BUMP_REST) * vn / inv;
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

// CHASE toggle (key C): if no pursuers are active, turn the nearest 2 AI traffic cars into
// reckless cops chasing the player; otherwise call off the chase. The drama of a getaway with
// the same brain dialed to "pursuit" — routing toward you, running lights, through the crowd.
static void toggle_chase(void) {
    int total = S->ncars + S->ncross, active = 0;
    for (int i = 1; i < total; i++) if (S->car[i].target >= 0) active++;
    if (active) {                                         // call it off
        for (int i = 1; i < total; i++) { S->car[i].target = -1; S->car[i].reckless = false; }
        return;
    }
    for (int n = 0; n < 2; n++) {                         // recruit the 2 nearest AI cars
        int best = -1; float bd = 1e18f;
        for (int i = 1; i < total; i++) {
            if (S->car[i].target >= 0) continue;
            float rx = S->car[i].px - P.px, ry = S->car[i].py - P.py;
            float d = rx*rx + ry*ry;
            if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) { S->car[best].target = 0; S->car[best].reckless = true; }
    }
}

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
    if (keyp('D'))                   { S->dbg = !S->dbg; }    // debug overlay: why cars brake
    if (keyp('C') && S->traffic && S->cross) toggle_chase();  // sic / call off pursuers on the player
    if (btnp(0, BTN_B)) P.drift = !P.drift;

    if (S->traffic && S->light >= 0) {                   // cycle the light
        if (++S->light_t >= (S->light_red ? LIGHT_RED : LIGHT_GRN)) { S->light_t = 0; S->light_red = !S->light_red; }
    }

    if (S->cross) update_reservations();                 // assign junction ownership before anyone drives

    float p_turn = (btn(0, BTN_RIGHT) ? 1.0f : 0.0f) - (btn(0, BTN_LEFT) ? 1.0f : 0.0f);
    float p_thr  = (btn(0, BTN_UP)    ? 1.0f : 0.0f) - (btn(0, BTN_DOWN) ? 1.0f : 0.0f);
    step_car(&P, p_turn, p_thr, P.drift);

    for (int i = 1; i < S->ncars; i++) {                    // the AI — race rivals or traffic
        float turn, thr; bool dr;
        if (S->traffic) drive_ai_traffic(&S->car[i], i, &turn, &thr, &dr);
        else            drive_ai(&S->car[i], i, &turn, &thr, &dr);
        step_car(&S->car[i], turn, thr, dr);
        if (S->traffic) maybe_switch_loops(&S->car[i], i);  // routing seed: turn onto the other track at a crossing
    }
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) { // the second track's stream (loops, like track 1)
        float turn, thr; bool dr;
        drive_ai_traffic(&S->car[i], i, &turn, &thr, &dr);
        step_car(&S->car[i], turn, thr, dr);
        maybe_switch_loops(&S->car[i], i);
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
    if (S->light >= 0) {                                 // length of the queue stopped behind the light
        int q = 0;
        for (int i = 0; i < S->ncars; i++) {
            int d = (S->light - S->car[i].prog + NSAMP) % NSAMP;
            float v = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
            if (d < 60 && v < 0.4f) q++;
        }
        watch("queue", "%d red=%d", q, S->light_red);
    }
    { float ms2 = 1e18f; int bi = 0, bj = 0;             // closest any two cars get (collision < 10px)
      for (int i = 0; i < S->ncars; i++)
        for (int j = i + 1; j < S->ncars; j++) {
            float ax = S->car[i].px - S->car[j].px, ay = S->car[i].py - S->car[j].py;
            float d2 = ax*ax + ay*ay; if (d2 < ms2) { ms2 = d2; bi = i; bj = j; }
        }
      watch("minsep", "%.1f lanes=%d/%d samelane=%d", fsqrt(ms2),
            S->car[bi].lane, S->car[bj].lane, S->car[bi].lane == S->car[bj].lane); }
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
    bool cop = c->target >= 0;                            // a pursuer (chase)
    if (is_player)       body = c->offtrack ? CLR_DARK_RED : CLR_RED;
    else if (cop)        body = CLR_DARKER_BLUE;           // cop car
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
    if (cop)                                             // flashing red/blue roof light
        circfill((int)c->px, (int)c->py, 2, ((int)(now()*6) & 1) ? CLR_RED : CLR_TRUE_BLUE);
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

    if (S->cross) {                                      // the SECOND track — same ribbon model, a closed loop too
        for (int s = 0; s < NSAMP2; s++) {
            int t = (s + 1) % NSAMP2;
            if ((S->cl2[s].x < L && S->cl2[t].x < L) || (S->cl2[s].x > R && S->cl2[t].x > R) ||
                (S->cl2[s].y < T && S->cl2[t].y < T) || (S->cl2[s].y > B && S->cl2[t].y > B)) continue;
            float lx0 = S->cl2[s].x + S->nl2[s].x*hw, ly0 = S->cl2[s].y + S->nl2[s].y*hw;
            float rx0 = S->cl2[s].x - S->nl2[s].x*hw, ry0 = S->cl2[s].y - S->nl2[s].y*hw;
            float lx1 = S->cl2[t].x + S->nl2[t].x*hw, ly1 = S->cl2[t].y + S->nl2[t].y*hw;
            float rx1 = S->cl2[t].x - S->nl2[t].x*hw, ry1 = S->cl2[t].y - S->nl2[t].y*hw;
            quadfill((int)lx0,(int)ly0,(int)rx0,(int)ry0,(int)rx1,(int)ry1,(int)lx1,(int)ly1, CLR_DARK_GREY);
            int curb = (s / 3) & 1 ? CLR_BLUE : CLR_WHITE;   // white/blue curbs distinguish it from track 1's red/white
            line((int)lx0,(int)ly0,(int)lx1,(int)ly1, curb);
            line((int)rx0,(int)ry0,(int)rx1,(int)ry1, curb);
        }
        for (int s = 0; s < NSAMP2; s += 6)                 // centre dashes
            line((int)S->cl2[s].x, (int)S->cl2[s].y,
                 (int)S->cl2[(s+3)%NSAMP2].x, (int)S->cl2[(s+3)%NSAMP2].y, CLR_BLUE_GREEN);
        int rbox = S->half;                                 // junction radius (= reservation Rocc)
        for (int i = 0; i < S->nx; i++) {
            int mx = (int)S->xpt[i].p.x, my = (int)S->xpt[i].p.y;
            int owner = S->xowner[i];
            circ(mx, my, rbox, owner >= 0 ? CLR_ORANGE : CLR_DARKER_GREY);   // ring: orange = reserved, grey = free
            circfill(mx, my, 5, CLR_INDIGO);
            circ(mx, my, 5, CLR_LIGHT_YELLOW);
            if (S->dbg && owner >= 0)                        // a line from the car that holds this junction
                line(mx, my, (int)S->car[owner].px, (int)S->car[owner].py, CLR_ORANGE);
        }
    }

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

    for (int i = 1; i < S->ncars; i++) draw_car(&S->car[i], false);   // loop rivals first
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) draw_car(&S->car[i], false);  // cross-road stream
    draw_car(&P, true);                                            // player on top

    if (S->dbg) {                                       // WHY each car is slowed/stuck
        int tot = S->ncars + S->ncross;
        for (int i = 1; i < tot; i++) {
            Car *c = &S->car[i];
            int col = c->why == WHY_FOLLOW  ? CLR_ORANGE
                    : c->why == WHY_LIGHT   ? CLR_RED
                    : c->why == WHY_YIELD   ? CLR_YELLOW
                    : c->why == WHY_BLOCKED ? CLR_PINK         // wants to go but wedged
                    :                         CLR_LIME_GREEN;  // cruising
            circfill((int)c->px, (int)c->py - 9, 2, col);
            if (c->why == WHY_YIELD) {                  // draw to the crossing it's waiting on
                int rd = c->road, bestd = 1 << 30, bi = -1;
                for (int k = 0; k < S->nx; k++) {
                    int xprog = rd ? S->xpt[k].prog2 : S->xpt[k].prog1;
                    int d = prog_ahead(rd, c->prog, xprog);
                    if (d > 0 && d < bestd) { bestd = d; bi = k; }
                }
                if (bi >= 0) line((int)c->px, (int)c->py, (int)S->xpt[bi].p.x, (int)S->xpt[bi].p.y, CLR_YELLOW);
            }
            if (c->why == WHY_BLOCKED) {                // draw to the car physically wedging it (the nearest one touching)
                float bd = (3.0f*CAR_HALF_L)*(3.0f*CAR_HALF_L); int bj = -1;
                for (int j = 0; j < tot; j++) {
                    if (j == i) continue;
                    float rx = S->car[j].px - c->px, ry = S->car[j].py - c->py;
                    float d = rx*rx + ry*ry;
                    if (d < bd) { bd = d; bj = j; }
                }
                if (bj >= 0) line((int)c->px, (int)c->py, (int)S->car[bj].px, (int)S->car[bj].py, CLR_PINK);
            }
        }
    }
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
    if (S->cross) {                                     // the second track + its crossings, to scale
        for (int s = 0; s < NSAMP2; s++) {
            int t = (s + 1) % NSAMP2;
            float ax = oxp + (S->cl2[s].x + S->nl2[s].x*hw)*sc, ay = oyp + (S->cl2[s].y + S->nl2[s].y*hw)*sc;
            float bx2= oxp + (S->cl2[s].x - S->nl2[s].x*hw)*sc, by2= oyp + (S->cl2[s].y - S->nl2[s].y*hw)*sc;
            float cx2= oxp + (S->cl2[t].x - S->nl2[t].x*hw)*sc, cy2= oyp + (S->cl2[t].y - S->nl2[t].y*hw)*sc;
            float dx2= oxp + (S->cl2[t].x + S->nl2[t].x*hw)*sc, dy2= oyp + (S->cl2[t].y + S->nl2[t].y*hw)*sc;
            quadfill((int)ax,(int)ay,(int)bx2,(int)by2,(int)cx2,(int)cy2,(int)dx2,(int)dy2, CLR_DARK_GREY);
        }
        for (int i = 0; i < S->nx; i++)
            circfill((int)(oxp + S->xpt[i].p.x*sc), (int)(oyp + S->xpt[i].p.y*sc), 2, CLR_INDIGO);
    }
    circfill((int)(oxp + S->cl[0].x*sc), (int)(oyp + S->cl[0].y*sc), 2, CLR_YELLOW);
    clip(0, 0, 0, 0);
}

static void draw_setup(void) {
    cls(CLR_BLACK);
    print_outline("TRACK GENERATOR", 8, 5, CLR_YELLOW, CLR_BLACK);

    ui_begin();
    if (ui_button(238, 3, 74, 12, S->cross ? "TRACKS:2" : "TRACKS:1")) S->cross = !S->cross;
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
        print(str("%d cars", S->ncars + S->ncross), 64, 2, CLR_WHITE);
        print(str("lane %d/%d", P.lane + 1, S->nlanes), 122, 2, CLR_YELLOW);
        bool chasing = false;                            // any pursuer hunting the player?
        for (int i = 1; i < S->ncars + S->ncross; i++) if (S->car[i].target >= 0) { chasing = true; break; }
        if (chasing) { if ((int)(now()*4) & 1) print("CHASE!", 182, 2, CLR_RED); }
        else if (S->light >= 0) print(S->light_red ? "RED" : "GRN", 184, 2, S->light_red ? CLR_RED : CLR_LIME_GREEN);
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

    print(S->traffic ? "M:menu  Z:new  R:reset"
                     : "M:menu  Z:new  R:restart",
          4, SCREEN_H - 9, P.offtrack ? CLR_ORANGE : CLR_LIGHT_GREY);

    // cute pixel-art HUD buttons (icons in trackgen.cart.js) for the in-drive toggles —
    // tap instead of hunting a key. CHASE only shows with two tracks (it needs the network).
    {
        int mx = mouse_x(), my = mouse_y(), bx = SCREEN_W - 19;
        bool chasing = false;
        for (int i = 1; i < S->ncars + S->ncross; i++) if (S->car[i].target >= 0) { chasing = true; break; }
        struct { int slot; bool on; bool show; } btn[3] = {
            { 2, S->dbg,    true },                          // debug (eye)
            { 1, P.drift,   true },                          // drift (skid)
            { 0, chasing,   S->traffic && S->cross },        // chase (cop lights)
        };
        for (int b = 0; b < 3; b++) {
            if (!btn[b].show) continue;
            int x = bx, y = SCREEN_H - 19;
            rrectfill(x - 1, y - 1, 18, 18, 3, btn[b].on ? CLR_DARKER_BLUE : CLR_BROWNISH_BLACK);
            rrect(x - 1, y - 1, 18, 18, 3, btn[b].on ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
            spr(btn[b].slot, x, y);
            if (mouse_pressed(0) && mx >= x-1 && mx < x+17 && my >= y-1 && my < y+17) {
                if (btn[b].slot == 2) S->dbg = !S->dbg;
                else if (btn[b].slot == 1) P.drift = !P.drift;
                else toggle_chase();
            }
            bx -= 20;
        }
    }

    if (S->dbg) {                                       // legend for the why-overlay dots
        font(FONT_SMALL);
        print("DBG:", 4, SCREEN_H - 17, CLR_WHITE);
        print("follow", 26, SCREEN_H - 17, CLR_ORANGE);
        print("light", 58, SCREEN_H - 17, CLR_RED);
        print("yield", 84, SCREEN_H - 17, CLR_YELLOW);
        print("go", 110, SCREEN_H - 17, CLR_LIME_GREEN);
        print("stuck", 124, SCREEN_H - 17, CLR_PINK);
        font(FONT_NORMAL);
    }

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
    hv->ang = lt->ang = 0.0f;                         // both face +x, so a +x gap is longitudinal
    hv->px = 100.0f; hv->py = 100.0f;
    lt->px = 105.0f; lt->py = 100.0f;                // overlapping nose-to-tail along +x
    float hx0 = hv->px, lx0 = lt->px;
    resolve_collisions();
    expect(hv->mass > lt->mass, "HUNTER outweighs SUNDAY");
    expect((hx0 - hv->px) < (lt->px - lx0), "heavy HUNTER yields less ground than light SUNDAY");

    // ── TRAFFIC mode: lane-keeping + car-following ──
    S->traffic = true;
    put_all_at_start();              // spreads cars round the loop, one per lane
    S->mode = 1;
    step(1500);                      // let the flow settle

    // car-following prevents pile-ups: no two car BOXES overlap after settling
    // (oriented test, the same one resolve_collisions uses — center distance alone is
    // misleading now that cars are long-and-narrow, not fat circles)
    int overlaps = 0;
    for (int i = 0; i < S->ncars; i++)
        for (int j = i + 1; j < S->ncars; j++) {
            float rx = S->car[j].px - S->car[i].px, ry = S->car[j].py - S->car[i].py;
            float fwd = rx*dx(1,S->car[i].ang)      + ry*dy(1,S->car[i].ang);
            float lat = rx*dx(1,S->car[i].ang + 90) + ry*dy(1,S->car[i].ang + 90);
            if ((fwd < 0 ? -fwd : fwd) < 2.0f*CAR_HALF_L - 0.5f &&
                (lat < 0 ? -lat : lat) < 2.0f*CAR_HALF_W - 0.5f) overlaps++;
        }
    expect_eq(overlaps, 0, "no pile-ups — no two car boxes overlap after settling");

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
    S->light_red = true; S->light_t = 0;     // force red and hold it (we step < LIGHT_RED)
    step(300);
    int queued = 0; float worst = 1e9f;
    for (int i = 0; i < S->ncars; i++) {
        int d = (S->light - S->car[i].prog + NSAMP) % NSAMP;   // samples until the light
        float v = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
        if (d < 60 && v < 0.4f) queued++;     // stopped, within range of the line
        if (S->car[i].spd < worst) worst = S->car[i].spd;
    }
    expect(queued >= 3, "traffic light: a red builds a LINE of stopped cars");
    expect(worst > -0.05f, "no reversing: cars stop at the light, never roll backward");

    // doesn't run the red: a lone car approaching a held red stops AT the line and
    // does NOT roll across (regression — the prog-wrap once let the front car bolt).
    put_all_at_start();
    S->mode = 1; S->light_red = true; S->light_t = 0;
    S->light = 20;                           // put the line on the start straight (geometry-stable)
    for (int i = 0; i < S->ncars; i++) {     // clear the road
        S->car[i].px = -9000.0f - i * 50.0f; S->car[i].py = -9000.0f;
        S->car[i].vx = S->car[i].vy = S->car[i].spd = 0;
    }
    {
        Car *A = &S->car[1];
        int s0 = (S->light - 40 + NSAMP) % NSAMP;        // 40 samples before the line
        float lo = lane_offset(0);
        A->lane = A->want_lane = 0;
        A->px = S->cl[s0].x + S->nl[s0].x * lo; A->py = S->cl[s0].y + S->nl[s0].y * lo;
        A->ang = atan2_deg(S->cl[(s0+1)%NSAMP].y - S->cl[s0].y, S->cl[(s0+1)%NSAMP].x - S->cl[s0].x);
        A->prog = s0; A->spd = MAX_SPD * 0.6f; A->offtrack = false;
        for (int k = 0; k < REACT_N; k++) A->thist[k] = 0; A->thead = 0;
        step(400);                                        // reaches the line, still red (< LIGHT_RED)
        int dd = (S->light - A->prog + NSAMP) % NSAMP;
        if (dd > NSAMP / 2) dd -= NSAMP;                  // signed: negative = past the line
        float av = A->spd < 0 ? -A->spd : A->spd;
        expect(dd > -8, "red light: car did NOT bolt through (stopped at/near the line)");
        expect(av < 0.4f, "red light: approaching car came to a stop");
    }

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
    S->light = -1; S->nx = 0;                 // isolate from the light AND the junctions
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

    // ── second track (geometry): a second loop crosses the first at ≥2 points (an
    //    even number — two closed loops always cross an even number of times), and
    //    every crossing sits on BOTH ribbons (within a half-width of each track's
    //    centre line at the recorded sample). ──
    S->cross = true;
    gen_track2();
    expect(S->nx >= 2, "two tracks: the second loop crosses the first at least twice");
    expect((S->nx & 1) == 0, "two tracks: two closed loops cross an even number of times");
    for (int i = 0; i < S->nx; i++) {
        Xing *x = &S->xpt[i];
        float dx1 = x->p.x - S->cl[x->prog1].x, dy1 = x->p.y - S->cl[x->prog1].y;
        float lat1 = dx1*S->nl[x->prog1].x + dy1*S->nl[x->prog1].y;       // across track 1
        expect((lat1 < 0 ? -lat1 : lat1) <= S->half + S->seg_len + 1.0f,
               "two tracks: crossing sits on track 1's ribbon");
        float dx2 = x->p.x - S->cl2[x->prog2].x, dy2 = x->p.y - S->cl2[x->prog2].y;
        float lat2 = dx2*S->nl2[x->prog2].x + dy2*S->nl2[x->prog2].y;     // across track 2
        expect((lat2 < 0 ? -lat2 : lat2) <= S->half + S->seg_len2 + 1.0f,
               "two tracks: crossing sits on track 2's ribbon");
    }

    // ── second-track traffic: with TRACKS:2 in TRAFFIC mode, a stream spawns on the
    //    second loop and drives it via the SAME brain. They make forward progress
    //    (prog advances, wrap-aware) and flow. Collisions at the crossings are
    //    expected here — that's what the right-of-way phase resolves. ──
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->reroute = false;                              // test the basic stream without track-switching
    expect(S->ncross > 0, "two tracks: a stream spawns on the second track");

    int prog0b[CARS_MAX];                             // each car's prog before stepping
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) prog0b[i] = S->car[i].prog;
    step(60);
    int advanced = 0; float vmaxc = 0;
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) {
        if (prog_ahead(1, prog0b[i], S->car[i].prog) > 2) advanced++;   // moved forward (wrap-aware)
        float vc = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
        if (vc > vmaxc) vmaxc = vc;
    }
    expect(advanced >= S->ncross - 3, "two tracks: the second-track stream makes forward progress");
    expect(vmaxc > 0.5f, "two tracks: the second track flows (a car is moving)");

    // with re-routing OFF, a second-track car holds its road (never swaps)
    int on_cross = 0;
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) if (S->car[i].road == 1) on_cross++;
    expect_eq(on_cross, S->ncross, "two tracks: with reroute off, every second-track car stays on its track");

    // ── right-of-way: priority-road rule. Over a long run, a track-1 car and a
    //    track-2 car NEVER collide (no T-bones) — measured as actual car-BODY overlap
    //    (the oriented-box test resolve_collisions uses; two cars merely both inside a
    //    junction but in different lanes pass cleanly and don't count). Both tracks
    //    keep flowing and second-track cars get round. ~1440 frames. ──
    S->traffic = true; S->cross = true;
    put_all_at_start();
    step(150);                                       // warm-up: let the initial grid settle into flow
    int conflicts = 0; long totprog = 0;             // totprog = total forward samples covered by track-2 cars
    float vloop = 0, vcross = 0;                      // peak speeds seen over the whole run
    int prev[CARS_MAX];
    for (int i = S->ncars; i < S->ncars + S->ncross; i++) prev[i] = S->car[i].prog;
    for (int chunk = 0; chunk < 240; chunk++) {
        step(6);
        for (int a = 0; a < S->ncars; a++)                   // track-1 × track-2 body overlaps = real T-bones
            for (int b = S->ncars; b < S->ncars + S->ncross; b++) {
                if (S->car[a].road == S->car[b].road) continue;  // a T-bone is CROSS-track; a rerouted car can
                                                                 // share a road with a "track-2" car — that's a
                                                                 // same-road graze (resolved by the pusher), not a T-bone
                float rx = S->car[b].px - S->car[a].px, ry = S->car[b].py - S->car[a].py;
                if (rx*rx + ry*ry >= (2.0f*CAR_HALF_L)*(2.0f*CAR_HALF_L)) continue;   // cheap reject
                float fwd = rx*dx(1,S->car[a].ang)      + ry*dy(1,S->car[a].ang);
                float lat = rx*dx(1,S->car[a].ang + 90) + ry*dy(1,S->car[a].ang + 90);
                if ((fwd<0?-fwd:fwd) < 2.0f*CAR_HALF_L && (lat<0?-lat:lat) < 2.0f*CAR_HALF_W) conflicts++;
            }
        for (int i = 1; i < S->ncars; i++) { float vv = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd; if (vv > vloop) vloop = vv; }
        for (int i = S->ncars; i < S->ncars + S->ncross; i++) {
            float vv = S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd; if (vv > vcross) vcross = vv;
            int d = prog_ahead(1, prev[i], S->car[i].prog); if (d > 0) totprog += d;   // forward samples this chunk
            prev[i] = S->car[i].prog;
        }
    }
    expect_eq(conflicts, 0, "right-of-way: track-1 and track-2 cars never collide at a junction (no T-bones)");
    expect(vloop  > 0.5f, "right-of-way: the priority track keeps flowing");
    expect(vcross > 0.5f, "right-of-way: the give-way track still flows (not starved to a stop)");
    expect(totprog >= (long)S->ncross * NSAMP2 / 4, "right-of-way: second-track cars get round the junctions (gaps accepted, not timid)");

    // ── no sustained gridlock, across SEEDS (the deadlock was seed-specific): for a
    //    handful of reseeds, run a while and assert the junctions never wedge the world
    //    to a standstill — several cars are still moving AND no two cars from different
    //    tracks are overlapped at the end. The reservation makes this hold by design. ──
    unsigned int seeds[4] = { 0x1234u, 0xBEEFu, 0x51A7u, 0x0C0Fu };
    for (int t = 0; t < 4; t++) {
        gen_track(seeds[t]);
        S->traffic = true; S->cross = true;
        put_all_at_start();
        int collisions = 0;
        for (int chunk = 0; chunk < 150; chunk++) {       // 900 frames, sampled — collisions caught over TIME
            step(6);
            for (int a = 0; a < S->ncars; a++)
                for (int b = S->ncars; b < S->ncars + S->ncross; b++) {
                    float rx = S->car[b].px - S->car[a].px, ry = S->car[b].py - S->car[a].py;
                    if (rx*rx + ry*ry >= (2.0f*CAR_HALF_L)*(2.0f*CAR_HALF_L)) continue;
                    float fwd = rx*dx(1,S->car[a].ang) + ry*dy(1,S->car[a].ang);
                    float lat = rx*dx(1,S->car[a].ang+90) + ry*dy(1,S->car[a].ang+90);
                    if ((fwd<0?-fwd:fwd) < 2.0f*CAR_HALF_L && (lat<0?-lat:lat) < 2.0f*CAR_HALF_W) collisions++;
                }
        }
        int moving = 0;
        for (int i = 0; i < S->ncars + S->ncross; i++)
            if ((S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd) > 0.5f) moving++;
        expect_eq(collisions, 0, "reservation: no cross-track collision over a long run (every seed)");
        expect(moving >= 4, "reservation: no gridlock — cars still moving after a long run (every seed)");
    }

    // ── routing seed: with reroute ON, cars TURN onto the other track at crossings (the
    //    two loops become a tiny network), and it stays crash-free. Count how many cars end
    //    up on a track different from the one they spawned on. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();                              // sets reroute = true
    int spawn_road[CARS_MAX];
    for (int i = 0; i < S->ncars + S->ncross; i++) spawn_road[i] = S->car[i].road;
    int rcollisions = 0;
    for (int chunk = 0; chunk < 200; chunk++) {     // 1200 frames
        step(6);
        for (int a = 0; a < S->ncars; a++)
            for (int b = S->ncars; b < S->ncars + S->ncross; b++) {
                float rx = S->car[b].px - S->car[a].px, ry = S->car[b].py - S->car[a].py;
                if (rx*rx + ry*ry >= (2.0f*CAR_HALF_L)*(2.0f*CAR_HALF_L)) continue;
                float fwd = rx*dx(1,S->car[a].ang) + ry*dy(1,S->car[a].ang);
                float lat = rx*dx(1,S->car[a].ang+90) + ry*dy(1,S->car[a].ang+90);
                if ((fwd<0?-fwd:fwd) < 2.0f*CAR_HALF_L && (lat<0?-lat:lat) < 2.0f*CAR_HALF_W) rcollisions++;
            }
    }
    int switched = 0;
    for (int i = 0; i < S->ncars + S->ncross; i++) if (S->car[i].road != spawn_road[i]) switched++;
    expect(switched >= 3, "routing: cars turn onto the other track at crossings (the 2 loops route)");
    expect_eq(rcollisions, 0, "routing: still no cross-track collision with re-routing on");

    // ── chase: a pursuer (target = the player) routes toward it and closes the distance.
    //    Park the player still, mark one AI car a reckless pursuer, and check it gets closer. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->ncars = 2; S->ncross = 0;                      // isolate the pursuit: just the player + one cop
    P.spd = 0; P.vx = P.vy = 0;                       // player sits still (no input in spec)
    float ppx = P.px, ppy = P.py;
    Car *cop = &S->car[1];
    cop->target = 0; cop->reckless = true;
    float rx0 = cop->px - ppx, ry0 = cop->py - ppy;
    float d0 = fsqrt(rx0*rx0 + ry0*ry0), dmin = d0;
    bool cut_grass = false;
    for (int f = 0; f < 2000; f++) {                 // keep the player pinned while the cop hunts it down
        P.px = ppx; P.py = ppy; P.spd = 0; P.vx = P.vy = 0;
        step(1);
        float rx = cop->px - ppx, ry = cop->py - ppy, d = fsqrt(rx*rx + ry*ry);
        if (d < dmin) dmin = d;
        if (cop->offtrack) cut_grass = true;         // it left the road to cut across
    }
    expect(dmin < 30.0f, "chase: a pursuer routes to and reaches its target (closes the distance)");
    expect(cut_grass, "chase: a pursuer cuts off-road (grass) to intercept, not only follows the road");
    // having boxed the (pinned) suspect, the cop PARKS on it — stays close and comes to rest,
    // rather than circling at speed.
    float rxe = cop->px - ppx, rye = cop->py - ppy;
    float de = fsqrt(rxe*rxe + rye*rye), ve = cop->spd < 0 ? -cop->spd : cop->spd;
    expect(de < 40.0f && ve < 0.6f, "chase: a cop PARKS on the suspect (close and stopped), not circling");

    // ── flow-around: a STOPPED player must not freeze traffic — cars behind go around it
    //    (overtake from a standstill). Park the player as a dead obstacle in lane 0 and line
    //    cars up behind it; they should pass, not pile up forever. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->nx = 0; S->light = -1; S->reroute = false;
    for (int i = 0; i < S->ncars + S->ncross; i++) {  // park everyone far away
        S->car[i].px = -9000.0f - i*80.0f; S->car[i].py = -9000.0f;
        S->car[i].prog = 9000; S->car[i].spd = 0; S->car[i].lane = S->car[i].want_lane = 9;
        S->car[i].reckless = false; S->car[i].target = -1; S->car[i].road = 0; S->car[i].offtrack = false;
    }
    { int pprog = 200;
      put_car_on_loop(&P, pprog, 0); P.spd = 0;        // player: a dead car in lane 0
      float ppx = P.px, ppy = P.py;
      int nq = 5;                                       // five cars queued behind, lane 0
      for (int i = 1; i <= nq; i++) put_car_on_loop(&S->car[i], pprog - 14*i, 0);
      for (int f = 0; f < 360; f++) {                  // pin the player as a fixed obstacle
          P.spd = 0; P.px = ppx; P.py = ppy; P.prog = pprog; P.lane = P.want_lane = 0;
          step(1);
      }
      int passed = 0; float vsum = 0;
      for (int i = 1; i <= nq; i++) {
          if (prog_ahead(0, pprog, S->car[i].prog) > 5) passed++;       // got past the player
          vsum += S->car[i].spd < 0 ? -S->car[i].spd : S->car[i].spd;
      }
      expect(passed >= 3, "flow-around: a stopped player gets overtaken — cars go around, not pile up");
      expect(vsum > 1.0f, "flow-around: traffic is still flowing after passing (not deadlocked behind the player)");
    }

    // ── flow-around AT A JUNCTION: the player parks right on a crossing (the screenshot bug).
    //    Cars behind on the same loop must still get past — not pile up + spill onto the grass. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->light = -1; S->reroute = false;
    for (int i = 0; i < S->ncars + S->ncross; i++) {
        S->car[i].px = -9000.0f - i*80.0f; S->car[i].py = -9000.0f;
        S->car[i].prog = 9000; S->car[i].spd = 0; S->car[i].lane = S->car[i].want_lane = 9;
        S->car[i].reckless = false; S->car[i].target = -1; S->car[i].road = 0; S->car[i].offtrack = false;
    }
    if (S->nx > 0) {
      int pprog = S->xpt[0].prog1;                      // park the player ON the crossing
      put_car_on_loop(&P, pprog, 0); P.spd = 0;
      float ppx = P.px, ppy = P.py;
      int nq = 5;
      for (int i = 1; i <= nq; i++) put_car_on_loop(&S->car[i], pprog - 14*i, 0);
      for (int f = 0; f < 720; f++) {
          P.spd = 0; P.px = ppx; P.py = ppy; P.prog = pprog; P.lane = P.want_lane = 0;
          step(1);
      }
      int passed = 0;
      for (int i = 1; i <= nq; i++)
          if (prog_ahead(0, pprog, S->car[i].prog) > 5) passed++;
      // a player parked ON a crossing DOES block it (realistic) — but it must not be a TOTAL
      // freeze: cars squeeze around him one-by-one, so several get past over time.
      expect(passed >= 2, "flow-around@junction: cars get past a player parked on a crossing (not a total deadlock)");
    }

    // ── scatter: a civilian yields (brakes) when a reckless car (cop) is right beside it. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->nx = 0; S->light = -1;                         // isolate from junctions and the light
    for (int i = 0; i < S->ncars + S->ncross; i++) {  // park everyone out of the way (prog 9000, lane 9)
        S->car[i].px = -9000.0f - i*80.0f; S->car[i].py = -9000.0f;
        S->car[i].prog = 9000; S->car[i].spd = 0; S->car[i].lane = S->car[i].want_lane = 9;
        S->car[i].reckless = false; S->car[i].target = -1; S->car[i].road = 0; S->car[i].offtrack = false;
    }
    Car *civ = &S->car[1];
    civ->prog = 120; civ->spd = MAX_SPD * 0.5f; civ->lane = civ->want_lane = 0;
    { V2 q = road_cl(0, 120); civ->px = q.x; civ->py = q.y;
      V2 a = road_cl(0, 121), b = road_cl(0, 119); civ->ang = atan2_deg(a.y-b.y, a.x-b.x); }
    for (int k = 0; k <= REACT_N; k++) drive_ai_traffic(civ, 1, &turn, &thr, &dr);   // clear road → accelerate
    float thr_clear = thr;
    Car *cop2 = &S->car[2];                           // a reckless car appears right beside the civilian
    cop2->reckless = true; cop2->px = civ->px + 10.0f; cop2->py = civ->py + 8.0f;
    for (int k = 0; k <= REACT_N; k++) drive_ai_traffic(civ, 1, &turn, &thr, &dr);
    expect(thr < thr_clear - 0.1f, "scatter: a civilian brakes/yields when a reckless car is right beside it");

    // ── K-turn: a wrong-way stopped car does a 3-point turn (reverses) to realign roughly IN
    //    PLACE, instead of looping forward in a big circle. Isolated on a clear road. ──
    gen_track(0x1234u);
    S->traffic = true; S->cross = true;
    put_all_at_start();
    S->nx = 0; S->light = -1; S->reroute = false;
    for (int i = 0; i < S->ncars + S->ncross; i++) {  // clear the road (park everyone far, idle)
        S->car[i].px = -9000.0f - i*80.0f; S->car[i].py = -9000.0f;
        S->car[i].prog = 9000; S->car[i].spd = 0; S->car[i].lane = S->car[i].want_lane = 9;
        S->car[i].reckless = false; S->car[i].target = -1; S->car[i].road = 0;
        S->car[i].offtrack = false; S->car[i].kt = 0;
    }
    Car *m = &S->car[1];
    m->prog = 120; m->lane = m->want_lane = 0; m->spd = 0; m->offtrack = false; m->kt = 0;
    { V2 q = road_cl(0, 120), a = road_cl(0, 121), b = road_cl(0, 119);
      m->px = q.x; m->py = q.y; m->ang = atan2_deg(a.y - b.y, a.x - b.x) + 150.0f; }   // wrong way
    float kx0 = m->px, ky0 = m->py;
    bool reversed = false, aligned = false; float maxexc = 0;
    for (int f = 0; f < 320; f++) {
        step(1);
        if (m->spd < -0.1f) reversed = true;
        V2 a = road_cl(0, m->prog + 2), b = road_cl(0, m->prog - 2);
        float e = awrap(atan2_deg(a.y - b.y, a.x - b.x) - m->ang); if (e < 0) e = -e;
        if (e < 30.0f) aligned = true;
        if (!aligned) { float ex = fsqrt((m->px-kx0)*(m->px-kx0) + (m->py-ky0)*(m->py-ky0)); if (ex > maxexc) maxexc = ex; }
    }
    expect(reversed, "k-turn: a wrong-way car reverses (3-point turn), not a forward loop");
    expect(aligned,  "k-turn: it ends up aligned with the road");
    expect(maxexc < 50.0f, "k-turn: it realigns roughly in place, not via a big circle");
}
#endif
