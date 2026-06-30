/* de:meta
{
  "title": "outrun",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "mode7",
    "parallax"
  ],
  "lineage": "Extends racer.c's segment/scanline pseudo-3D projection with a live binary stage tree, forking road ribbons, and biome palette-swaps; homage to OutRun (1986).",
  "genre": "racing",
  "homage": "OutRun (1986)",
  "description": "A sun-drenched pseudo-3D arcade racer with BRANCHING roads. Blast down a curving, hilly highway faked from projected triangles, weave through traffic (each car packed with little passengers), and beat each checkpoint before the clock runs out. The twist: at every checkpoint the road SPLITS into two diverging ribbons with a grass island between them — steer onto the left or right fork to choose your next biome, and the scenery ahead recolours to the road you're leaning toward. Five biomes (coast, desert, city, forest, alpine) share one procedural road via pal() recolour; the stages are a binary TREE generated on the fly, so no two runs take the same path. Clip a car and you spin out for a time-costing tumble; off-road bogs you down in a cloud of dust. Engine pitch tracks your speed, tyres screech through hard corners, speed-lines streak past flat out. Reach the goal stage to win. Z / Up: gas — X / Down: brake — Left/Right: steer (and pick a fork).",
  "todo": [
    "Bug: trees and cars flicker.",
    "Use real polygons for the car drawing; dislike the various resolutions.",
    "The crash roll feels weird — find something better.",
    "Add a visible guy and girl in the car; make it look like a pico32 Testarossa."
  ]
}
de:meta */
#include "studio.h"

// OUTRUN — a sun-drenched pseudo-3D arcade racer with BRANCHING ROADS.
//
// Blast down a curving, hilly road, weave through traffic (each car packed with
// little passengers), and beat each checkpoint before the clock runs out. At
// every checkpoint the road SPLITS in two — steer into the left or right fork to
// pick your next biome. Five biomes (coast, desert, city, forest, mountains)
// share one procedural road, recoloured per stage. Reach the goal to win.
//
// Built on the same segment/scanline projection as racer.c, extended with:
//   • a procedural TREE of stages (forks regenerate the road ahead live)
//   • two diverging road ribbons during a fork, with a grass island between
//   • billboard traffic + roadside props scaled & depth-sorted by distance
//   • a checkpoint countdown, spin-out crashes, dust, speed-lines, engine pitch
//
// Z / UP = gas    X / DOWN = brake    LEFT / RIGHT = steer (and pick a fork)

// ---- track / camera constants (world units) ----
#define SEGL        200.0f   // length of one road segment
#define ROAD_W      1000.0f  // half road width
#define CAM_H       1000.0f  // camera height above the road
#define CAM_D       0.84f    // camera depth — fov ~100°
#define PLAYER_Z    840.0f   // player sits this far ahead of the camera
#define DRAW_DIST   90       // segments rendered ahead
#define RUMBLE      4        // segments per rumble/colour stripe
#define LANES       3
#define CENTRIFUGAL 0.55f    // how hard curves sling you outward

// ---- stage tree ----
#define MAX_DEPTH   5                       // path length: 4 forks + a goal
#define NODES       ((1 << MAX_DEPTH) - 1)  // 31 nodes in the full binary tree
#define NODE_LEN    240      // segments per stage  (DRAW_DIST < NODE_LEN: ≤1 crossing)
#define FORK_LEN    70       // last N segments of a stage are the split
#define GAP_MAX     1.9f     // half-road-widths the two ribbons spread apart

// ---- biomes ----
enum { COAST = 0, DESERT, CITY, FOREST, MOUNT, N_BIOME };
static const char *BIOME_NAME[N_BIOME] = { "COAST", "DESERT", "CITY", "FOREST", "ALPINE" };

// ---- sprite slots (see outrun.cart.js) ----
#define SPR_PLAYER  1
#define SPR_TRAFFIC 2
#define SPR_PALM    3
#define SPR_CACTUS  4
#define SPR_PINE    5
#define SPR_TOWER   6
#define SPR_ROCK    7
#define SPR_PORSCHE 8        // traffic silhouettes (all recoloured via MAGIC_BODY)
#define SPR_BEETLE  9
#define SPR_PICKUP  10
#define SPR_TRUCK   11
#define SPR_WAGON   12
#define MAGIC_BODY  28       // traffic body painted in this index, recoloured per car

// ---- traffic ----
#define MAX_CARS    14
typedef struct { float z, off, spd; int col, kind; } Car;
static Car cars[MAX_CARS];

// vehicle silhouettes — display width (world units), height aspect, collision
// half-width (in road-half-widths). Trucks ride taller and clip wider.
typedef struct { int slot; float wworld, aspect, chw; } VKind;
static const VKind VKINDS[] = {
    { SPR_TRAFFIC, 760.0f, 1.00f, 0.55f },   // sports car / Ferrari
    { SPR_PORSCHE, 720.0f, 1.00f, 0.55f },   // Porsche 911
    { SPR_BEETLE,  640.0f, 1.00f, 0.50f },   // VW Beetle
    { SPR_PICKUP,  830.0f, 1.05f, 0.62f },   // pickup truck
    { SPR_TRUCK,   900.0f, 1.55f, 0.72f },   // box truck
    { SPR_WAGON,   800.0f, 1.10f, 0.60f },   // station wagon
};
#define N_VKIND 6

// ---- dust / smoke particles ----
#define MAX_PARTS   80
typedef struct { float x, y, vx, vy, life; int col; } Part;
static Part parts[MAX_PARTS];

// ---- game state ----
enum { RACING = 0, FINISHED, DEAD };
static int   mode;
static int   cur_node;          // stage we're driving through
static int   node_base_seg;     // global segment index where cur_node begins
static int   biome_of[NODES];   // each node's biome
static float position;          // camera z along the path (always increasing)
static float pworld;            // player lateral offset, in road-half-widths
static float speed;
static float time_left;
static int   stage;             // checkpoints passed (1..MAX_DEPTH)
static float chk_flash;         // "CHECKPOINT!" splash timer
static int   chk_bonus;         // last time bonus, for the rollup
static float spin;              // spin-out timer (seconds), 0 = driving
static float sky_x;             // distant-mountain parallax
static float screech;           // tyre-screech meter

// derived speed limits (units / second)
static float MAX_SPD, ACCEL, BRAKE, DECEL, OFF_DECEL, OFF_LIMIT;

// ---- projection scratch (per boundary, near→far) ----
static int   bx[DRAW_DIST + 1], by[DRAW_DIST + 1], bw[DRAW_DIST + 1];
static float bscale[DRAW_DIST + 1], gapb[DRAW_DIST + 1];
static int   biomeb[DRAW_DIST + 1];
static bool  bvalid[DRAW_DIST + 1];
static bool  seg_drawn[DRAW_DIST];

// ====================================================================
// helpers

static float fabsf2(float v) { return v < 0 ? -v : v; }
static int   is_leaf(int id)  { return 2 * id + 1 >= NODES; }

// procedural per-stage road shape. seeded by node id; both return 0 at the stage
// ends so adjacent stages join without a kink.
static float node_curve(int id, int i) {
    float t = (float)i / NODE_LEN;
    int   f1 = 2 + (id % 4), f2 = 5 + (id % 5);
    float amp = 1.6f + (id % 3) * 0.7f;
    return (sin_deg(t * 360 * f1) + sin_deg(t * 360 * f2) * 0.6f) * amp;
}
static float node_hill(int id, int i) {
    float t = (float)i / NODE_LEN;
    int   h1 = 1 + (id % 3), h2 = 2 + (id % 4);
    return sin_deg(t * 360 * h1) * 1300.0f + sin_deg(t * 360 * h2) * 500.0f;
}

// resolve one global segment to its stage data, walking into the predicted fork
typedef struct { float curve, hill, gap, lat; int biome; } Sample;
static Sample sample_seg(int gseg) {
    Sample s = { 0, 0, 0, 0, COAST };
    int local = gseg - node_base_seg;
    int id    = cur_node;
    int sidep = (pworld >= 0) ? 1 : -1;     // which fork we're leaning toward

    if (local >= NODE_LEN) {
        if (is_leaf(cur_node)) {
            local = NODE_LEN - 1;           // goal stage: hold the last segment
        } else {
            local -= NODE_LEN;
            id    = (sidep > 0) ? 2 * cur_node + 2 : 2 * cur_node + 1;
            s.lat = sidep * GAP_MAX;        // child road rides the chosen ribbon
        }
    } else if (!is_leaf(cur_node) && local >= NODE_LEN - FORK_LEN) {
        float p = (float)(local - (NODE_LEN - FORK_LEN)) / FORK_LEN;
        s.gap   = clamp(p, 0, 1) * GAP_MAX; // the split opening up
    }
    if (local < 0) local = 0;
    s.curve = node_curve(id, local);
    s.hill  = node_hill(id, local);
    s.biome = biome_of[id];
    return s;
}

// ---- biome palettes ----
static int sky_col(int b) {
    switch (b) { case COAST: return CLR_BLUE;     case DESERT: return CLR_ORANGE;
                 case CITY:  return CLR_DARK_BLUE; case FOREST: return CLR_TRUE_BLUE;
                 default:    return CLR_LIGHT_GREY; }
}
static int haze_col(int b) {
    switch (b) { case COAST: return CLR_TRUE_BLUE; case DESERT: return CLR_DARK_ORANGE;
                 case CITY:  return CLR_DARKER_PURPLE; case FOREST: return CLR_BLUE_GREEN;
                 default:    return CLR_WHITE; }
}
static int hill_col(int b) {
    switch (b) { case COAST: return CLR_DARKER_PURPLE; case DESERT: return CLR_BROWN;
                 case CITY:  return CLR_DARKER_GREY; case FOREST: return CLR_DARK_GREEN;
                 default:    return CLR_LIGHT_GREY; }
}
static int grass_col(int b, bool dark) {
    switch (b) {
        case COAST:  return dark ? CLR_DARK_GREEN  : CLR_MEDIUM_GREEN;
        case DESERT: return dark ? CLR_BROWN       : CLR_DARK_ORANGE;
        case CITY:   return dark ? CLR_DARKER_GREY : CLR_DARK_GREY;
        case FOREST: return dark ? CLR_DARK_GREEN  : CLR_BLUE_GREEN;
        default:     return dark ? CLR_LIGHT_GREY  : CLR_WHITE;
    }
}
static int prop_slot(int b)  { switch (b) { case COAST: return SPR_PALM; case DESERT: return SPR_CACTUS;
                                            case CITY: return SPR_TOWER; default: return SPR_PINE; } }

// ====================================================================

static void build_tree() {
    biome_of[0] = COAST;
    for (int id = 0; id < NODES; id++) {
        if (is_leaf(id)) continue;
        int b = biome_of[id];
        biome_of[2 * id + 1] = (b + 1) % N_BIOME;   // left  fork
        biome_of[2 * id + 2] = (b + 3) % N_BIOME;   // right fork — always differs
    }
}

static void spawn_car(int i, float min_ahead) {
    cars[i].z   = position + min_ahead + rnd((int)(SEGL * 50));
    cars[i].off = rnd_float_between(-0.7f, 0.7f);
    cars[i].spd = MAX_SPD * rnd_float_between(0.20f, 0.5f);
    static const int pal[7] = { CLR_RED, CLR_BLUE, CLR_YELLOW, CLR_GREEN,
                                CLR_ORANGE, CLR_PINK, CLR_WHITE };
    cars[i].col  = pal[rnd(7)];
    cars[i].kind = rnd(N_VKIND);
}

static void new_game() {
    MAX_SPD   = SEGL * 60.0f;        // ~1 segment / frame flat out
    ACCEL     =  MAX_SPD / 3.5f;
    BRAKE     = -MAX_SPD;
    DECEL     = -MAX_SPD / 5.0f;
    OFF_DECEL = -MAX_SPD / 2.0f;
    OFF_LIMIT =  MAX_SPD / 4.0f;

    mode = RACING;
    cur_node = 0; node_base_seg = 0;
    position = 0; pworld = 0; speed = 0;
    time_left = 32.0f; stage = 0; chk_flash = 0; chk_bonus = 0;
    spin = 0; sky_x = 0; screech = 0;

    for (int i = 0; i < MAX_CARS; i++) spawn_car(i, SEGL * 6);
    for (int i = 0; i < MAX_PARTS; i++) parts[i].life = 0;
    bpm(112);
}

void init() {
    colorkey(0);                                  // index 0 transparent for sprites
    build_tree();

    // engine — a buzzy saw with a warm lowpass and a touch of vibrato; pitch tracks speed
    instrument(5, INSTR_SAW, 4, 40, 5, 50);
    instrument_filter(5, FILTER_LOW, 850, 5);
    instrument_lfo(5, 0, LFO_PITCH, 7.0f, 0.3f);
    // breezy bass bed
    instrument(6, INSTR_TRI, 8, 120, 4, 220);
    instrument_filter(6, FILTER_LOW, 500, 3);

    new_game();
}

static void dust(float x, float y, int col) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, rnd_float_between(-30, 30),
                               rnd_float_between(-60, -10), rnd_float_between(0.3f, 0.7f), col };
            return;
        }
}

// ====================================================================

void update() {
    float D = dt();
    if (mode != RACING) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(0, BTN_UP)) new_game();
        return;
    }

    // ---- clock ----
    time_left -= D;
    if (chk_flash > 0) chk_flash -= D;
    if (time_left <= 0) { time_left = 0; mode = DEAD; note(34, INSTR_NOISE, 7); shake(6); return; }

    int   pseg    = (int)((position + PLAYER_Z) / SEGL);
    Sample sp     = sample_seg(pseg);
    float curve   = sp.curve;
    float spd_pct = speed / MAX_SPD;

    // player's own fork gap (so we know where the ribbons are around the car)
    float gap_p = sp.gap;

    // ---- steering / throttle (disabled while spinning out) ----
    if (spin > 0) {
        spin -= D;
        speed += DECEL * D;
        if (frame() % 2 == 0) dust(SCREEN_W / 2 + rnd(20) - 10, SCREEN_H - 22, CLR_LIGHT_GREY);
    } else {
        float steer = D * 2.7f * spd_pct;
        if (btn(0, BTN_LEFT))  pworld -= steer;
        if (btn(0, BTN_RIGHT)) pworld += steer;
        pworld -= steer * spd_pct * curve * CENTRIFUGAL;   // curves sling you out

        if (btn(0, BTN_UP) || btn(0, BTN_A))         speed += ACCEL * D;
        else if (btn(0, BTN_DOWN) || btn(0, BTN_B))  speed += BRAKE * D;
        else                                         speed += DECEL * D;

        // tyre screech building through fast corners
        float hard = fabsf2(curve) * spd_pct;
        screech = lerp(screech, hard > 1.6f ? 1.0f : 0.0f, D * 6.0f);
        if (screech > 0.4f && frame() % 5 == 0) hit(64, INSTR_NOISE, 1, 40);
    }

    // ---- off-road: are we on a road ribbon, or out on the grass/island? ----
    float dl = fabsf2(pworld - gap_p), dr = fabsf2(pworld + gap_p);
    float dist_road = (gap_p > 0.05f) ? (dl < dr ? dl : dr)     // two ribbons
                                      : fabsf2(pworld);          // one road
    bool offroad = dist_road > 1.0f;
    if (offroad && speed > OFF_LIMIT) {
        speed += OFF_DECEL * D;
        shake(1.5f);
        if (frame() % 3 == 0) {
            dust(SCREEN_W / 2 + rnd(24) - 12, SCREEN_H - 20, CLR_LIGHT_PEACH);
            note(40, INSTR_NOISE, 1);
        }
    }

    speed  = clamp(speed, 0, MAX_SPD);
    pworld = clamp(pworld, -4.0f, 4.0f);

    // ---- traffic ----
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].z += cars[i].spd * D;
        if (cars[i].z < position - SEGL * 2) spawn_car(i, SEGL * (DRAW_DIST - 10));

        float ahead = cars[i].z - (position + PLAYER_Z);
        if (spin <= 0 && ahead > -SEGL && ahead < SEGL * 1.4f
            && fabsf2(pworld - cars[i].off) < VKINDS[cars[i].kind].chw + 0.05f) {
            // clipped a car — spin out
            spin   = 1.1f;
            speed *= 0.35f;
            shake(7);
            note(28, INSTR_NOISE, 7);
            for (int n = 0; n < 10; n++) dust(SCREEN_W / 2, SCREEN_H - 24, CLR_WHITE);
        }
    }

    position += speed * D;
    sky_x    += curve * spd_pct * 0.8f;

    // ---- engine note rises with speed ----
    if (frame() % 5 == 0)
        hit(30 + (int)(spd_pct * 24), 5, 2 + (int)(spd_pct * 2), 90);
    // breezy bed
    if (every(1)) hit(36 + (beat() % 2) * 7, 6, 1, 180);

    // ---- particles ----
    for (int i = 0; i < MAX_PARTS; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].x += parts[i].vx * D;
        parts[i].y += parts[i].vy * D;
        parts[i].vy += 90 * D;
        parts[i].life -= D;
    }

    // ---- checkpoint commit: camera crosses a (non-leaf) stage end ----
    float cam_local = position - (float)node_base_seg * SEGL;
    if (!is_leaf(cur_node) && cam_local >= NODE_LEN * SEGL) {
        int side = (pworld >= 0) ? 1 : -1;
        cur_node = (side > 0) ? 2 * cur_node + 2 : 2 * cur_node + 1;
        node_base_seg += NODE_LEN;
        pworld -= side * GAP_MAX;            // recentre onto the chosen road
        stage++;
        chk_bonus = 8 + stage * 2;
        time_left += chk_bonus;
        chk_flash = 2.2f;
        // a bright ascending chime
        for (int n = 0; n < 4; n++) schedule(n * 70, 72 + n * 4, INSTR_SINE, 5);
    }

    // ---- goal: player passes the end of a leaf stage ----
    float plr_local = (position + PLAYER_Z) - (float)node_base_seg * SEGL;
    if (is_leaf(cur_node) && plr_local >= NODE_LEN * SEGL) {
        mode = FINISHED;
        for (int n = 0; n < 6; n++) schedule(n * 90, 60 + n * 3, INSTR_SINE, 6);
    }
}

// ====================================================================
// drawing

// filled quad from four corners
static void fquad(int ax, int ay, int b1, int b2, int cx, int cy, int d1, int d2, int col) {
    trifill(ax, ay, b1, b2, cx, cy, col);
    trifill(ax, ay, cx, cy, d1, d2, col);
}

// one road ribbon: rumble strips + tarmac, near (y1,w1) → far (y2,w2)
static void draw_ribbon(int cx1, int w1, int cx2, int w2, int y1, int y2,
                        int road, int rum) {
    int r1 = w1 / 5 + 1, r2 = w2 / 5 + 1;
    fquad(cx1 - w1 - r1, y1, cx2 - w2 - r2, y2, cx2 - w2, y2, cx1 - w1, y1, rum);
    fquad(cx1 + w1, y1, cx2 + w2, y2, cx2 + w2 + r2, y2, cx1 + w1 + r1, y1, rum);
    fquad(cx1 - w1, y1, cx2 - w2, y2, cx2 + w2, y2, cx1 + w1, y1, road);
}

static void draw_background(int b) {
    cls(sky_col(b));
    int horizon = SCREEN_H / 2;
    rectfill(0, horizon - 26, SCREEN_W, 26, haze_col(b));
    // the sun (coast / desert)
    if (b == COAST || b == DESERT) {
        int sx = SCREEN_W / 2 - (int)(sky_x * 0.3f) % SCREEN_W;
        circfill(sx, horizon - 14, 16, CLR_YELLOW);
        circfill(sx, horizon - 14, 12, CLR_LIGHT_YELLOW);
    }
    // distant ridge, parallax with cornering
    int hc = hill_col(b);
    for (int x = 0; x < SCREEN_W; x++) {
        float u = (x + sky_x) * 0.02f;
        float h = noise(u) * 30.0f + noise(u * 2.7f + 40.0f) * 12.0f;
        line(x, horizon - (int)h, x, horizon, hc);
    }
}

static void draw_player(void) {
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H - 30;
    int lean = (btn(0, BTN_LEFT) ? -1 : 0) + (btn(0, BTN_RIGHT) ? 1 : 0);
    int bounce = (speed > 0 && !blink(3)) ? 1 : 0;
    int dw = 64, dh = 64;
    float rot = (spin > 0) ? (1.1f - spin) / 1.1f * 720.0f : lean * 5.0f;
    // shadow
    ovalfill(cx, cy + 22, 26, 6, CLR_BLACK);
    sspr_ex((SPR_PLAYER % 8) * 16, (SPR_PLAYER / 8) * 16, 16, 16,
            cx - dw / 2, cy - dh / 2 + bounce, dw, dh, rot, dw / 2, dh / 2);
}

void draw() {
    int   pseg = (int)((position + PLAYER_Z) / SEGL);
    int   cam_b = biome_of[cur_node];
    draw_background(cam_b);

    int   base_i  = (int)(position / SEGL);
    float base_pc = (position - base_i * SEGL) / SEGL;

    // player elevation so the camera rides the hills
    Sample s0 = sample_seg(pseg), s1 = sample_seg(pseg + 1);
    float ppc     = ((position + PLAYER_Z) - pseg * SEGL) / SEGL;
    float playerY = lerp(s0.hill, s1.hill, clamp(ppc, 0, 1));
    float camY    = playerY + CAM_H;
    float camBaseX = pworld * ROAD_W;

    // ---- project every boundary, accumulating curve ----
    Sample sb = sample_seg(base_i);
    float x  = 0;
    float dx = -(sb.curve * base_pc);

    for (int k = 0; k <= DRAW_DIST; k++) {
        int    gseg = base_i + k;
        Sample s    = sample_seg(gseg);
        float  camz = gseg * SEGL - position;
        bvalid[k]   = camz > 1.0f;
        float  sc   = bvalid[k] ? (CAM_D / camz) : 0.0f;
        bscale[k]   = sc;
        bx[k] = (int)(SCREEN_W / 2 + sc * (x + s.lat * ROAD_W - camBaseX) * (SCREEN_W / 2));
        by[k] = (int)(SCREEN_H / 2 - sc * (s.hill - camY) * (SCREEN_H / 2));
        bw[k] = (int)(sc * ROAD_W * (SCREEN_W / 2));
        gapb[k]  = s.gap;
        biomeb[k] = s.biome;
        if (k < DRAW_DIST) { x += dx; dx += s.curve; }
    }

    // ---- road segments near → far, occluding behind hills ----
    int maxy = SCREEN_H;
    for (int n = 0; n < DRAW_DIST; n++) {
        seg_drawn[n] = false;
        if (!bvalid[n]) continue;
        int y1 = by[n], y2 = by[n + 1];
        if (y1 <= y2 || y2 >= maxy) continue;
        seg_drawn[n] = true;

        int  gseg = base_i + n;
        int  b    = biomeb[n];
        bool dark = ((gseg / RUMBLE) % 2) == 0;
        int  road = dark ? CLR_DARK_GREY : CLR_DARKER_GREY;
        int  grass = grass_col(b, dark);
        int  rum  = dark ? CLR_WHITE : CLR_RED;

        int x1 = bx[n], w1 = bw[n], x2 = bx[n + 1], w2 = bw[n + 1];

        // grass band for this slice
        rectfill(0, y2, SCREEN_W, y1 - y2, grass);

        if (gapb[n] > 0.02f || gapb[n + 1] > 0.02f) {
            // FORK: two ribbons with a grass island opening between them
            float g1 = gapb[n], g2 = gapb[n + 1];
            int   lx1 = x1 - (int)(g1 * w1), lx2 = x2 - (int)(g2 * w2);
            int   rx1 = x1 + (int)(g1 * w1), rx2 = x2 + (int)(g2 * w2);
            draw_ribbon(lx1, w1, lx2, w2, y1, y2, road, rum);
            draw_ribbon(rx1, w1, rx2, w2, y1, y2, road, rum);
        } else {
            // single road
            draw_ribbon(x1, w1, x2, w2, y1, y2, road, rum);
            if (dark) {
                int lw1 = max(1, w1 / 22), lw2 = max(1, w2 / 22);
                for (int l = 1; l < LANES; l++) {
                    float f = (float)l / LANES * 2.0f - 1.0f;
                    int m1 = x1 + (int)(f * w1), m2 = x2 + (int)(f * w2);
                    fquad(m1 - lw1, y1, m2 - lw2, y2, m2 + lw2, y2, m1 + lw1, y1, CLR_LIGHT_GREY);
                }
            }
            // finish line on the goal stage
            int loc = gseg - node_base_seg;
            if (is_leaf(cur_node) && loc >= NODE_LEN - 4 && loc <= NODE_LEN) {
                int checks = 8;
                for (int c = 0; c < checks; c++) {
                    float fa = (float)c / checks * 2.0f - 1.0f;
                    float fb = (float)(c + 1) / checks * 2.0f - 1.0f;
                    int a1 = x1 + (int)(fa * w1), a2 = x1 + (int)(fb * w1);
                    rectfill(a1, y2, a2 - a1, y1 - y2, ((c + loc) & 1) ? CLR_WHITE : CLR_BLACK);
                }
            }
        }
        maxy = y2;
    }

    // ---- roadside props, far → near ----
    colorkey(0);
    for (int n = DRAW_DIST - 1; n >= 1; n--) {
        if (!seg_drawn[n]) continue;
        int   gseg = base_i + n;
        int   h = (gseg * 131 + 7) & 255;
        if (h >= 46) continue;                       // ~18% of segments get a prop
        float sc   = bscale[n];
        int   b    = biomeb[n];
        int   slot = prop_slot(b);
        if ((b == MOUNT) && (h & 4)) slot = SPR_ROCK; // alpine: mix in rocks
        float side = (h & 1) ? 1.0f : -1.0f;
        float off  = side * (1.5f + ((h >> 1) % 5) * 0.5f);
        // gentle widen of props during a fork so they don't sit in the road
        if (gapb[n] > 0.1f) off *= 1.5f;

        int gx = bx[n] + (int)(sc * off * ROAD_W * (SCREEN_W / 2));
        int pw = (int)(sc * (slot == SPR_TOWER ? 1000.0f : 620.0f) * (SCREEN_W / 2));
        int ph = (int)(sc * (slot == SPR_TOWER ? 2600.0f : 1500.0f) * (SCREEN_H / 2));
        if (pw < 2 || gx < -pw || gx > SCREEN_W + pw) continue;
        int sx = (slot % 8) * 16, sy = (slot / 8) * 16;
        sspr(sx, sy, 16, 16, gx - pw / 2, by[n] - ph, pw, ph);
    }

    // ---- traffic, far → near ----
    for (int pass = DRAW_DIST - 1; pass >= 1; pass--) {
        for (int c = 0; c < MAX_CARS; c++) {
            float ahead = cars[c].z - position;
            int n = (int)(ahead / SEGL);
            if (n != pass || n < 1 || n >= DRAW_DIST || !seg_drawn[n]) continue;
            float sc = bscale[n];
            const VKind *vk = &VKINDS[cars[c].kind];
            int cx = bx[n] + (int)(sc * cars[c].off * ROAD_W * (SCREEN_W / 2));
            int cw = (int)(sc * vk->wworld * (SCREEN_W / 2));
            int ch = (int)(cw * vk->aspect);          // trucks ride taller than wide
            if (cw < 3) continue;
            pal(MAGIC_BODY, cars[c].col);             // recolour the body per car
            sspr((vk->slot % 8) * 16, (vk->slot / 8) * 16, 16, 16,
                 cx - cw / 2, by[n] - ch, cw, ch);
            pal_reset();
        }
    }

    // ---- dust / smoke ----
    for (int i = 0; i < MAX_PARTS; i++) {
        if (parts[i].life <= 0) continue;
        int r = parts[i].life > 0.4f ? 2 : 1;
        circfill((int)parts[i].x, (int)parts[i].y, r, parts[i].col);
    }

    draw_player();

    // ---- speed lines at the top end ----
    float spd_pct = speed / MAX_SPD;
    if (spd_pct > 0.72f && mode == RACING) {
        int n = (int)((spd_pct - 0.72f) / 0.28f * 9);
        for (int i = 0; i < n; i++) {
            int yy = SCREEN_H / 2 + rnd(SCREEN_H / 2);
            int len = 6 + rnd(14);
            int xx = (rnd(2)) ? rnd(40) : SCREEN_W - 40 - rnd(40);
            line(xx, yy, xx + len, yy, CLR_WHITE);
        }
    }

    // ====================== HUD ======================
    int kmh = (int)(spd_pct * 290);
    print(str("%3d KM/H", kmh), 4, 4, kmh > 220 ? CLR_YELLOW : CLR_WHITE);
    print_centered(str("STAGE %d/%d", stage + 1, MAX_DEPTH), SCREEN_W/2, 4, CLR_LIGHT_GREY);
    print(BIOME_NAME[cam_b], 4, 14, CLR_LIGHT_GREY);

    // big clock, flashing red when low
    int tcol = (time_left < 6 && blink(8)) ? CLR_RED : CLR_WHITE;
    print_right(str("%05.1f", time_left), SCREEN_W - 4, 4, tcol);

    // fork guidance — name the two roads while the split is open
    Sample sp = sample_seg(pseg);
    if (sp.gap > 0.15f && mode == RACING && !is_leaf(cur_node)) {
        int lb = biome_of[2 * cur_node + 1], rb = biome_of[2 * cur_node + 2];
        print_centered("PICK YOUR ROAD", SCREEN_W/2, 150, CLR_YELLOW);
        print(str("< %s", BIOME_NAME[lb]), 14, 162, pworld < 0 ? CLR_WHITE : CLR_DARK_GREY);
        print_right(str("%s >", BIOME_NAME[rb]), SCREEN_W - 14, 162, pworld >= 0 ? CLR_WHITE : CLR_DARK_GREY);
    }

    // checkpoint splash + time rollup
    if (chk_flash > 0) {
        float t = 1.0f - chk_flash / 2.2f;
        int yy = 40 + (int)(ease_out(clamp(t * 3, 0, 1)) * 10);
        print_scaled("CHECKPOINT!", SCREEN_W / 2 - 66, yy, blink(4) ? CLR_YELLOW : CLR_WHITE, 2);
        print_centered(str("+%d SECONDS", chk_bonus), SCREEN_W/2, yy + 22, CLR_LIME_GREEN);
    }

    if (mode == FINISHED) {
        fade(0.5f);
        print_scaled("YOU MADE IT!", SCREEN_W / 2 - 72, 70, CLR_YELLOW, 2);
        print_centered(str("FINISHED IN %s", BIOME_NAME[cam_b]), SCREEN_W/2, 96, CLR_WHITE);
        print_centered(str("%d CHECKPOINTS CLEARED", stage), SCREEN_W/2, 108, CLR_LIME_GREEN);
        print_centered("PRESS Z TO DRIVE AGAIN", SCREEN_W/2, 130, blink(15) ? CLR_WHITE : CLR_LIGHT_GREY);
    } else if (mode == DEAD) {
        fade(0.55f);
        print_scaled("OUT OF TIME", SCREEN_W / 2 - 66, 76, CLR_RED, 2);
        print_centered(str("REACHED STAGE %d", stage + 1), SCREEN_W/2, 100, CLR_WHITE);
        print_centered("PRESS Z TO RETRY", SCREEN_W/2, 124, blink(15) ? CLR_WHITE : CLR_LIGHT_GREY);
    }
}
