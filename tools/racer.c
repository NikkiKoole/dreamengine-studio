#include "studio.h"

// PSEUDO-3D RACER
// Up: gas   Down: brake   Left/Right: steer
// Dodge the traffic, hold the road through curves and hills.
// Your best lap is saved between runs.

// ---- track / camera constants ----
#define SEGL        200.0f   // length of one road segment (world units)
#define N_SEG       1200     // segments on the (looping) track
#define ROAD_W      1000.0f  // half road width (world units)
#define CAM_H       1000.0f  // camera height above the road
#define CAM_D       0.84f    // camera depth — fov ~100°
#define PLAYER_Z    840.0f   // CAM_H * CAM_D — player sits this far ahead of camera
#define DRAW_DIST   120      // segments rendered ahead
#define RUMBLE      4        // segments per rumble/colour stripe
#define LANES       3
#define CENTRIFUGAL 0.32f    // how hard curves sling you outward
#define FPS         60.0f
#define MAX_CARS    16

static const float TRACK_LEN = N_SEG * SEGL;
static const float DT        = 1.0f / FPS;

// derived speed limits (units / second)
static float MAX_SPD, ACCEL, BRAKE, DECEL, OFF_DECEL, OFF_LIMIT;

// ---- track data ----
static float seg_curve[N_SEG];     // bend of each segment
static float seg_y[N_SEG + 1];     // hill elevation at each boundary
static float seg_tree[N_SEG];      // roadside tree offset in road-half-widths (0 = none)

// ---- traffic ----
typedef struct { float z, off, spd; int col; } Car;
static Car cars[MAX_CARS];

// ---- player / game state ----
static float position;     // camera z along the track
static float playerX;      // -1..1 = road edges (can drift onto grass)
static float speed;
static float prev_pos;
static float sky_x;        // distant-mountain parallax offset
static int   lap;
static float best_lap, last_lap;
static int   shk;

// ---- projection scratch (per boundary, near→far) ----
static int   bx[DRAW_DIST + 1], by[DRAW_DIST + 1], bw[DRAW_DIST + 1];
static float bscale[DRAW_DIST + 1];
static bool  bvalid[DRAW_DIST + 1];
static bool  seg_drawn[DRAW_DIST];

// ====================================================================

static float increase(float start, float inc, float maxv) {
    float r = start + inc;
    while (r >= maxv) r -= maxv;
    while (r < 0)     r += maxv;
    return r;
}

static float fabsf2(float v) { return v < 0 ? -v : v; }

static void build_track() {
    // sine-built so the track loops seamlessly (whole number of cycles)
    for (int i = 0; i < N_SEG; i++) {
        float t = (float)i / N_SEG;
        seg_curve[i] = sin_deg(t * 360 * 4) * 2.2f + sin_deg(t * 360 * 9) * 1.6f;
    }
    for (int i = 0; i <= N_SEG; i++) {
        float t = (i == N_SEG) ? 0.0f : (float)i / N_SEG;
        seg_y[i] = sin_deg(t * 360 * 3) * 1500.0f + sin_deg(t * 360 * 7) * 600.0f;
    }
    for (int i = 0; i < N_SEG; i++) {
        seg_tree[i] = 0;
        int h = (i * 131 + 7) & 255;
        if (h < 42) {                                   // ~16% of segments get a tree
            float side = (h & 1) ? 1.0f : -1.0f;
            float dist = 1.5f + ((h >> 1) % 5) * 0.55f; // 1.5 .. 3.7 road-widths out
            seg_tree[i] = side * dist;
        }
    }
}

static void new_game() {
    MAX_SPD   = SEGL / DT;          // 1 segment / frame at top speed
    ACCEL     =  MAX_SPD / 4.0f;
    BRAKE     = -MAX_SPD;
    DECEL     = -MAX_SPD / 5.0f;
    OFF_DECEL = -MAX_SPD / 2.0f;
    OFF_LIMIT =  MAX_SPD / 4.0f;

    position = 0; playerX = 0; speed = 0; prev_pos = 0;
    sky_x = 0; lap = 0; last_lap = 0; shk = 0;
    best_lap = load(0) / 1000.0f;   // stored as milliseconds

    static const int palette[6] = {
        CLR_RED, CLR_BLUE, CLR_YELLOW, CLR_GREEN, CLR_ORANGE, CLR_PINK
    };
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].z   = (float)i / MAX_CARS * TRACK_LEN + rnd((int)SEGL * 3);
        cars[i].off = rnd_float_between(-0.7f, 0.7f);
        cars[i].spd = MAX_SPD * rnd_float_between(0.22f, 0.55f);
        cars[i].col = palette[i % 6];
    }
    timer_reset();
}

void init() {
    build_track();
    new_game();
}

// ---- which segment & how far through it is a z position ----
static int seg_index(float z) {
    return ((int)(z / SEGL)) % N_SEG;
}

void update() {
    int   pseg     = seg_index(position + PLAYER_Z);
    float curve    = seg_curve[pseg];
    float spd_pct  = speed / MAX_SPD;

    // steering
    float steer = DT * 2.4f * spd_pct;
    if (btn(0, BTN_LEFT))  playerX -= steer;
    if (btn(0, BTN_RIGHT)) playerX += steer;
    // curves sling you to the outside
    playerX -= steer * spd_pct * curve * CENTRIFUGAL;

    // throttle / brake
    if (btn(0, BTN_UP) || btn(0, BTN_A))      speed += ACCEL * DT;
    else if (btn(0, BTN_DOWN) || btn(0, BTN_B)) speed += BRAKE * DT;
    else                                       speed += DECEL * DT;

    // off-road penalty
    bool offroad = (playerX < -1 || playerX > 1);
    if (offroad && speed > OFF_LIMIT) {
        speed += OFF_DECEL * DT;
        if (frame() % 4 == 0) note(42, INSTR_NOISE, 2);
    }

    speed   = clamp(speed, 0, MAX_SPD);
    playerX = clamp(playerX, -2.2f, 2.2f);

    // traffic moves + collision
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].z = increase(cars[i].z, DT * cars[i].spd, TRACK_LEN);

        float pz = position + PLAYER_Z;
        float cz = cars[i].z;
        while (cz < pz) cz += TRACK_LEN;
        float ahead = cz - pz;
        if (ahead > 0 && ahead < SEGL * 1.6f) {
            if (fabsf2(playerX - cars[i].off) < 0.55f) {
                // rear-end: slow hard, get knocked aside
                speed = cars[i].spd * 0.4f;
                playerX += (playerX > cars[i].off ? 0.18f : -0.18f);
                shk = 8;
                note(30, INSTR_NOISE, 6);
            }
        }
    }

    position = increase(position, DT * speed, TRACK_LEN);

    // distant-mountain parallax drifts as you corner
    sky_x += curve * spd_pct * 0.6f;

    // engine note rises with speed
    if (speed > 60 && frame() % 7 == 0)
        hit(26 + (int)(spd_pct * 18), INSTR_TRI, 1, 90);

    // lap timing — detect the wrap past the finish line
    if (prev_pos - position > TRACK_LEN * 0.5f) {
        last_lap = timer();
        timer_reset();
        lap++;
        if (lap >= 2) {                          // first lap from spawn doesn't count
            if (best_lap == 0 || last_lap < best_lap) {
                best_lap = last_lap;
                save(0, (int)(best_lap * 1000));
            }
        }
    }
    prev_pos = position;

    if (shk > 0) shk--;
}

// ---- filled quad from four corners (bl, tl, tr, br) ----
static void fquad(int ax, int ay, int bx_, int by_, int cx, int cy, int dx_, int dy_, int col) {
    trifill(ax, ay, bx_, by_, cx, cy, col);
    trifill(ax, ay, cx, cy, dx_, dy_, col);
}

static void draw_background() {
    cls(CLR_DARK_BLUE);
    // warm haze band near the horizon
    rectfill(0, SCREEN_H / 2 - 28, SCREEN_W, 28, CLR_TRUE_BLUE);
    // distant mountains (parallax with cornering)
    int base_y = SCREEN_H / 2;
    for (int x = 0; x < SCREEN_W; x++) {
        float u = (x + sky_x) * 0.02f;
        float h = noise(u) * 34.0f + noise(u * 2.7f + 40.0f) * 14.0f;
        line(x, base_y - (int)h, x, base_y, CLR_DARKER_PURPLE);
    }
}

static void draw_player_car() {
    int cx = SCREEN_W / 2 + (shk > 0 ? rnd(3) - 1 : 0);
    int cy = SCREEN_H - 26 + (shk > 0 ? rnd(3) - 1 : 0);
    // small lean with steering
    int lean = (btn(0, BTN_LEFT) ? -2 : 0) + (btn(0, BTN_RIGHT) ? 2 : 0);
    cx += lean;
    int bounce = (speed > 0 && !blink(3)) ? 1 : 0;
    cy += bounce;

    // shadow
    rectfill(cx - 20, cy + 8, 40, 3, CLR_BLACK);
    // body
    rectfill(cx - 18, cy - 4, 36, 12, CLR_RED);
    rectfill(cx - 22, cy + 2, 44, 7,  CLR_RED);
    // rear window
    rectfill(cx - 12, cy - 4, 24, 6,  CLR_DARK_BLUE);
    // tail lights
    rectfill(cx - 21, cy + 3, 4, 4, CLR_YELLOW);
    rectfill(cx + 17, cy + 3, 4, 4, CLR_YELLOW);
    // wheels
    rectfill(cx - 24, cy + 7, 7, 4, CLR_BLACK);
    rectfill(cx + 17, cy + 7, 7, 4, CLR_BLACK);
}

void draw() {
    draw_background();

    int   base_i  = (int)(position / SEGL);
    float base_pc = (position - base_i * SEGL) / SEGL;

    // player elevation (so the camera rides the hills)
    int   ps      = seg_index(position + PLAYER_Z);
    float pz      = position + PLAYER_Z;
    float ppc     = (pz - (int)(pz / SEGL) * SEGL) / SEGL;
    float playerY = lerp(seg_y[ps], seg_y[(ps + 1) % N_SEG], ppc);
    float camY    = playerY + CAM_H;

    // ---- project every boundary, accumulating the curve ----
    float x  = 0;
    float dx = -(seg_curve[base_i % N_SEG] * base_pc);
    float camBaseX = playerX * ROAD_W;

    for (int k = 0; k <= DRAW_DIST; k++) {
        int   idx  = (base_i + k) % N_SEG;
        float wz   = (base_i + k) * SEGL;
        float camz = wz - position;
        bvalid[k]  = camz > 1.0f;
        float sc   = bvalid[k] ? (CAM_D / camz) : 0.0f;
        bscale[k]  = sc;
        bx[k] = (int)(SCREEN_W / 2 + sc * (x - camBaseX) * (SCREEN_W / 2));
        by[k] = (int)(SCREEN_H / 2 - sc * (seg_y[idx] - camY) * (SCREEN_H / 2));
        bw[k] = (int)(sc * ROAD_W * (SCREEN_W / 2));
        if (k < DRAW_DIST) {
            x  += dx;
            dx += seg_curve[(base_i + k) % N_SEG];
        }
    }

    // ---- draw road segments near → far, clipping behind hills ----
    int maxy = SCREEN_H;
    for (int n = 0; n < DRAW_DIST; n++) {
        seg_drawn[n] = false;
        if (!bvalid[n]) continue;
        int y1 = by[n], y2 = by[n + 1];       // near (bottom), far (top)
        if (y1 <= y2 || y2 >= maxy) continue;  // back-face / occluded by a nearer hill
        seg_drawn[n] = true;

        int   i    = (base_i + n) % N_SEG;
        bool  dark = ((i / RUMBLE) % 2) == 0;
        int   road   = dark ? CLR_DARK_GREY    : CLR_DARKER_GREY;
        int   grass  = dark ? CLR_DARK_GREEN   : CLR_MEDIUM_GREEN;
        int   rumble = dark ? CLR_WHITE        : CLR_RED;

        int x1 = bx[n],     w1 = bw[n];
        int x2 = bx[n + 1], w2 = bw[n + 1];

        // grass — full width band for this slice
        rectfill(0, y2, SCREEN_W, y1 - y2, grass);

        // rumble strips
        int r1 = w1 / 5 + 1, r2 = w2 / 5 + 1;
        fquad(x1 - w1 - r1, y1, x2 - w2 - r2, y2, x2 - w2, y2, x1 - w1, y1, rumble);
        fquad(x1 + w1, y1, x2 + w2, y2, x2 + w2 + r2, y2, x1 + w1 + r1, y1, rumble);

        // road
        fquad(x1 - w1, y1, x2 - w2, y2, x2 + w2, y2, x1 + w1, y1, road);

        // dashed lane markers (only on dark stripes → dashes)
        if (dark) {
            int lw1 = max(1, w1 / 22), lw2 = max(1, w2 / 22);
            for (int l = 1; l < LANES; l++) {
                float f = (float)l / LANES * 2.0f - 1.0f;   // -1..1 across road
                int m1 = x1 + (int)(f * w1), m2 = x2 + (int)(f * w2);
                fquad(m1 - lw1, y1, m2 - lw2, y2, m2 + lw2, y2, m1 + lw1, y1, CLR_LIGHT_GREY);
            }
        }

        maxy = y2;
    }

    // ---- sprites & traffic, far → near so they occlude correctly ----
    for (int n = DRAW_DIST - 1; n >= 0; n--) {
        if (!seg_drawn[n]) continue;
        int   i  = (base_i + n) % N_SEG;
        float sc = bscale[n];

        // roadside tree
        if (seg_tree[i] != 0) {
            int gx = bx[n] + (int)(sc * seg_tree[i] * ROAD_W * (SCREEN_W / 2));
            int gy = by[n];
            int tw = (int)(sc * 520.0f * (SCREEN_W / 2));
            int th = (int)(sc * 1700.0f * (SCREEN_H / 2));
            if (tw >= 1 && gx > -tw && gx < SCREEN_W + tw) {
                int trunk = max(1, tw / 4);
                rectfill(gx - trunk / 2, gy - th / 3, trunk, th / 3, CLR_BROWN);
                trifill(gx - tw / 2, gy - th / 3, gx, gy - th, gx + tw / 2, gy - th / 3, CLR_DARK_GREEN);
                trifill(gx - tw / 3, gy - th / 2, gx, gy - th + th / 6, gx + tw / 3, gy - th / 2, CLR_GREEN);
            }
        }
    }

    // traffic cars
    for (int c = 0; c < MAX_CARS; c++) {
        float cz = cars[c].z;
        while (cz < position) cz += TRACK_LEN;
        float ahead = cz - position;
        int n = (int)(ahead / SEGL);
        if (n < 1 || n >= DRAW_DIST || !seg_drawn[n]) continue;
        float sc = bscale[n];
        int cx = bx[n] + (int)(sc * cars[c].off * ROAD_W * (SCREEN_W / 2));
        int cy = by[n];
        int cw = (int)(sc * 720.0f * (SCREEN_W / 2));
        int ch = (int)(sc * 460.0f * (SCREEN_H / 2));
        if (cw < 2) continue;
        rectfill(cx - cw / 2, cy - ch, cw, ch, cars[c].col);
        rectfill(cx - cw / 2, cy - ch, cw, max(1, ch / 3), CLR_DARK_BLUE);  // window
        rectfill(cx - cw / 2, cy - 3, max(1, cw / 6), 3, CLR_YELLOW);       // tail lights
        rectfill(cx + cw / 2 - max(1, cw / 6), cy - 3, max(1, cw / 6), 3, CLR_YELLOW);
    }

    draw_player_car();

    // ---- HUD ----
    int kmh = (int)(speed / MAX_SPD * 240);
    print(str("%3d km/h", kmh), 4, 4, kmh > 180 ? CLR_YELLOW : CLR_WHITE);
    print_centered(str("LAP %d", lap), 4, CLR_LIGHT_GREY);
    print_right(str("%02d:%02d", (int)timer() / 60, (int)timer() % 60), SCREEN_W - 4, 4, CLR_WHITE);

    if (best_lap > 0)
        print_right(str("BEST %.1fs", best_lap), SCREEN_W - 4, 14, CLR_YELLOW);
    if (last_lap > 0)
        print(str("LAST %.1fs", last_lap), 4, 14, CLR_LIGHT_GREY);
}
