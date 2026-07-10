/* de:meta
{
  "slug": "podracer",
  "title": "pod racer",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game",
    "tech-demo"
  ],
  "teaches": [
    "mode7",
    "no-sprite-vehicles",
    "particle-system",
    "screen-shake-juice"
  ],
  "lineage": "Extends the pseudo-3D segment-projection road renderer with tritex-textured canyon walls and ground (affine quads, painter's order far-to-near), adding a boost/heat risk mechanic and CPU pod rivals sharing the spline.",
  "genre": "racing",
  "homage": "Star Wars Episode I: Racer (1999)",
  "description": "Twin-engine pods screaming through a textured rock chasm — and NOT a flat scanline road. It clads the whole canyon in real tritex geometry: the ground is a tech-floor that stretches to the horizon and the walls are rock strata rising on both sides, drawn far→near as a painter's pass (short segments keep the affine warp small, like textured3d's subdivision). The one deep mechanic is BOOST/HEAT — hold A for massive speed while engine heat climbs to a blowout stall; vents cool you off-boost, so it's pure risk/reward. Scrape a wall and you shed speed in a shower of sparks. A layered detuned engine drone pitches with speed, the boost adds a filter-swept roar (FILTER_LOW + LFO_CUTOFF) and a rising overheat alarm, with an FOV-pulse, speed lines, screen-shake and a sonic-boom flash at top speed. Race 3 laps against 4 CPU pods, best lap saved. Up/Z throttle, Down/X air-brake, Left/Right steer, hold A (or Shift) to BOOST."
}
de:meta */
#include "studio.h"

// POD RACER — twin-engine pods screaming through a textured rock chasm.
//
// The headline tech: this is NOT a flat scanline road. It takes the proven
// segment projection (rotate the track's curve/hills into screen space, near
// vs far boundaries) and clads the whole canyon in REAL textured geometry:
//   - the GROUND is a tritex tech-floor that stretches out to the horizon
//   - the WALLS are tritex rock strata rising on both sides of the chasm
// drawn far -> near as a painter's pass. tritex is affine (PS1-style), so the
// short segments keep the warp small — many little textured quads, like the
// subdivision trick in textured3d.c.
//
// The one deep mechanic is BOOST / HEAT: hold boost for massive speed, but
// engine heat climbs toward a blowout (a brief stall). Vents cool you off-boost.
// Scraping a wall sheds speed + sparks + heat. A few CPU pods share the spline.
//
//   Up/Z throttle   Down/X air-brake   Left/Right steer   hold A (or Shift) BOOST

// ---- track / camera ----
#define SEGL        200.0f
#define N_SEG       600
#define LAPS        3
#define ROAD_W      850.0f      // half road width (world units)
#define WALL_H      2900.0f     // canyon wall height (world units)
#define CAM_H       1100.0f
#define CAM_D       0.84f       // camera depth -> fov
#define PLAYER_Z    924.0f      // CAM_H * CAM_D
#define DRAW_DIST   70          // segments rendered ahead
#define NCPU        4
#define FPS         60.0f
#define HORIZON     (SCREEN_H / 2)
#define CENTRIFUGAL 0.22f

static const float TRACK_LEN = N_SEG * SEGL;
static const float DT        = 1.0f / FPS;

// texture sub-rects on the sheet (inset 0.5px so affine sampling never bleeds
// into the neighbouring slot). slot 0 = rock wall, slot 1 = tech floor.
#define ROCK_U0 0.5f
#define ROCK_U1 15.5f
#define ROCK_V0 0.5f
#define ROCK_V1 15.5f
#define GRND_U0 16.5f
#define GRND_U1 31.5f
#define GRND_V0 0.5f
#define GRND_V1 15.5f

// ---- instrument slots ----
#define ENG_A   5
#define ENG_B   6
#define BOOSTI  7
#define ALARMI  8
#define SCRAPEI 9
#define WHOOSHI 10

// derived speed limits (units / second)
static float MAX_SPD, BOOST_SPD, ACCEL, BRAKE, DECEL, OFF_DECEL;

// ---- track data ----
static float seg_curve[N_SEG];
static float seg_y[N_SEG + 1];

// ---- player / game state ----
static float position;     // camera z along the track
static float playerX;      // -1..1 = road edges; beyond = scraping the wall
static float speed, prev_pos;
static float heat;         // 0..1 engine heat
static bool  boosting, stalled;
static float stall_t;      // seconds of blowout stall remaining
static float fov;          // FOV multiplier — pulses up on boost
static int   lap;
static float best_lap, last_lap, race_t;
static bool  finished;
static float shx, shy;     // screen-shake offset
static int   boom_t;       // sonic-boom flash frames
static bool  boomed;       // already flashed this top-speed run

// ---- CPU pods ----
typedef struct { float z, lane, basespd, prev_z; int col, lapn, wcd; } Pod;
static Pod cpu[NCPU];

// ---- spark particles (wall scrapes) ----
typedef struct { float x, y, vx, vy, life; int col; } Spark;
static Spark spark[60];

// ---- projection scratch (per boundary) ----
static int   bx[DRAW_DIST + 1], by[DRAW_DIST + 1], bw[DRAW_DIST + 1], wh[DRAW_DIST + 1];
static float bscale[DRAW_DIST + 1];
static bool  bvalid[DRAW_DIST + 1];

// ====================================================================

static float wrapf(float v, float maxv) {
    while (v >= maxv) v -= maxv;
    while (v < 0)     v += maxv;
    return v;
}
static float fabsf2(float v) { return v < 0 ? -v : v; }

static void build_track(void) {
    for (int i = 0; i < N_SEG; i++) {
        float t = (float)i / N_SEG;                 // integer cycles -> seamless loop
        seg_curve[i] = sin_deg(t * 360 * 3) * 2.4f + sin_deg(t * 360 * 7) * 1.5f;
    }
    for (int i = 0; i <= N_SEG; i++) {
        float t = (i == N_SEG) ? 0.0f : (float)i / N_SEG;
        seg_y[i] = sin_deg(t * 360 * 2) * 850.0f + sin_deg(t * 360 * 5) * 320.0f;
    }
}

static void spawn_spark(float x, float y, float dir) {
    for (int n = 0; n < 6; n++)
        for (int i = 0; i < 60; i++)
            if (spark[i].life <= 0) {
                int hot[] = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_DARK_ORANGE };
                spark[i] = (Spark){ x, y,
                    dx(rnd_float_between(1.5f, 4.5f), dir + rnd_between(-40, 40)),
                    dy(rnd_float_between(1.5f, 4.5f), dir + rnd_between(-40, 40)) - 1.0f,
                    rnd_between(10, 20), hot[rnd(4)] };
                break;
            }
}

static void new_game(void) {
    MAX_SPD   = SEGL / DT;            // 1 segment / frame
    BOOST_SPD = MAX_SPD * 1.55f;
    ACCEL     =  MAX_SPD / 3.5f;
    BRAKE     = -MAX_SPD * 1.1f;
    DECEL     = -MAX_SPD / 6.0f;
    OFF_DECEL = -MAX_SPD * 0.9f;

    position = 0; playerX = 0; speed = 0; prev_pos = 0;
    heat = 0; boosting = false; stalled = false; stall_t = 0; fov = 1.0f;
    lap = 1; last_lap = 0; race_t = 0; finished = false;
    shx = shy = 0; boom_t = 0; boomed = false;
    best_lap = load(0) / 1000.0f;

    int pal6[NCPU] = { CLR_PINK, CLR_LIME_GREEN, CLR_ORANGE, CLR_TRUE_BLUE };
    for (int i = 0; i < NCPU; i++) {
        cpu[i].z       = (i + 1) * (TRACK_LEN / (NCPU + 2));
        cpu[i].prev_z  = cpu[i].z;
        cpu[i].lane    = rnd_float_between(-0.55f, 0.55f);
        cpu[i].basespd = MAX_SPD * rnd_float_between(0.62f, 0.82f);
        cpu[i].col     = pal6[i];
        cpu[i].lapn    = 0;
        cpu[i].wcd     = 0;
    }
    for (int i = 0; i < 60; i++) spark[i].life = 0;
    timer_reset();
}

void init(void) {
    instrument(ENG_A, INSTR_TRI, 4, 0, 6, 90);
    instrument(ENG_B, INSTR_SAW, 4, 0, 5, 90);
    instrument(BOOSTI, INSTR_SAW, 6, 0, 7, 170);
    instrument_filter(BOOSTI, FILTER_LOW, 650, 11);
    instrument_lfo(BOOSTI, 0, LFO_CUTOFF, 6.0f, 700);    // roar = sweeping cutoff
    lfo_shape(BOOSTI, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    instrument(ALARMI, INSTR_SQUARE, 2, 0, 6, 80);
    instrument_duty(ALARMI, 0.5f);
    instrument(SCRAPEI, INSTR_NOISE, 1, 0, 4, 60);
    instrument(WHOOSHI, INSTR_NOISE, 40, 0, 3, 220);
    instrument_filter(WHOOSHI, FILTER_BAND, 1600, 9);
    build_track();
    new_game();
}

static int seg_index(float z) { return ((int)(z / SEGL)) % N_SEG; }

// player's absolute distance travelled, for race ranking
static float player_prog(void) { return (lap - 1) * TRACK_LEN + position; }

void update(void) {
    if (finished) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) new_game(); return; }

    race_t = timer();
    int   pseg    = seg_index(position + PLAYER_Z);
    float curve   = seg_curve[pseg];
    float spd_pct = speed / MAX_SPD;

    // ---- boost / heat (the signature mechanic) ----
    bool want_boost = btn(0, BTN_A) || btn(1, BTN_A) || key(KEY_SPACE)
                      || key(340) /* L-shift */ || key(344) /* R-shift */;
    boosting = want_boost && !stalled && heat < 1.0f && speed > 30;

    if (stalled) {
        stall_t -= DT;
        heat -= 0.85f * DT;                          // vent hard during blowout
        if (stall_t <= 0 && heat < 0.5f) stalled = false;
    } else if (boosting) {
        heat += 0.78f * DT;
        if (heat >= 1.0f) {                           // BLOWOUT
            heat = 1.0f; stalled = true; stall_t = 1.3f;
            speed *= 0.4f; boosting = false;
            shx = 7; shy = 4;
            hit(28, SCRAPEI, 6, 320);
            note(33, ALARMI, 6);
        }
    } else {
        heat -= 0.50f * DT;                           // vents cool when off boost
    }
    heat = clamp(heat, 0, 1);

    // ---- throttle / brake / steer ----
    float top = boosting ? BOOST_SPD : MAX_SPD;
    if (boosting)                                          speed += ACCEL * 1.9f * DT;
    else if (btn(0, BTN_UP) || btn(1, BTN_UP) || key('Z')) speed += ACCEL * DT;
    else if (btn(0, BTN_DOWN) || btn(1, BTN_DOWN) || key('X')) speed += BRAKE * DT;
    else                                                   speed += DECEL * DT;
    if (stalled) speed += DECEL * DT;                       // sluggish while stalled

    float steer = DT * 2.5f * clamp(spd_pct, 0.15f, 1.2f);
    if (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  playerX -= steer;
    if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) playerX += steer;
    // curves sling you toward the outside wall — but it's ALWAYS beatable: this pull
    // peaks well under your steer authority (~0.014 vs ~0.05 per frame at top speed),
    // so leaning into a curve holds the line; let go and you drift wide (recoverable),
    // never pinned. (Was tied to `steer` * spd_pct * raw curve * 0.34 — ~2× steering,
    // which slammed you into the rock with no way out.)
    playerX -= DT * spd_pct * clamp(curve, -2.5f, 2.5f) * CENTRIFUGAL;

    speed   = clamp(speed, 0, top);
    playerX = clamp(playerX, -1.18f, 1.18f);

    // ---- wall scrape: hugging the rock costs speed + heat + sparks ----
    if (fabsf2(playerX) > 1.0f) {
        speed += OFF_DECEL * DT;
        heat = clamp(heat + 0.55f * DT, 0, 1);
        playerX *= 0.985f;                                   // shove back toward track
        shx = clamp(shx + 2.0f, 0, 6);
        bool right = playerX > 0;
        if (frame() % 2 == 0) {
            spawn_spark(right ? SCREEN_W - 18 : 18, SCREEN_H - 34, right ? 200 : 340);
            hit(44 + rnd(6), SCRAPEI, 4, 45);
        }
    }

    // ---- engine sound: two layered detuned voices, pitched by speed ----
    if (speed > 35 && frame() % 3 == 0) {
        int em = 22 + (int)(clamp(speed / MAX_SPD, 0, 1.6f) * 20);
        hit(em,      ENG_A, 1, 70);
        hit(em + 12, ENG_B, 1, 55);                          // octave layer = body
    }
    if (boosting && frame() % 5 == 0) {
        int em = 22 + (int)(clamp(speed / MAX_SPD, 0, 1.6f) * 20);
        hit(em - 10, BOOSTI, 3, 150);                        // filter-swept roar
    }
    // heat alarm rises in pitch as you near the blowout
    if (heat > 0.78f && !stalled && frame() % 6 == 0)
        hit(58 + (int)((heat - 0.78f) / 0.22f * 14), ALARMI, 3, 90);

    // ---- sonic-boom flash at top speed ----
    if (speed > MAX_SPD * 1.42f) {
        if (!boomed) { boomed = true; boom_t = 4; shx = clamp(shx + 5, 0, 8);
                       hit(24, BOOSTI, 5, 260); }
    } else if (speed < MAX_SPD * 1.2f) boomed = false;

    // ---- CPU pods cruise the spline; player can pass (whoosh) + bump ----
    float pprog = player_prog();
    for (int i = 0; i < NCPU; i++) {
        Pod *c = &cpu[i];
        c->lane = clamp(c->lane + sin_deg(now() * 55 + i * 90) * 0.012f, -0.8f, 0.8f);
        float curveslow = 1.0f - fabsf2(seg_curve[seg_index(c->z + PLAYER_Z)]) * 0.06f;
        c->prev_z = c->z;
        c->z = wrapf(c->z + DT * c->basespd * curveslow, TRACK_LEN);
        if (c->prev_z - c->z > TRACK_LEN * 0.5f) c->lapn++;
        if (c->wcd > 0) c->wcd--;

        // relative position to player
        float cz = c->z; while (cz < position) cz += TRACK_LEN;
        float ahead = cz - position;
        if (ahead > 0 && ahead < SEGL * 1.4f && fabsf2(playerX - c->lane) < 0.5f) {
            speed = min(speed, c->basespd * 0.55f);          // rear-end
            playerX += (playerX > c->lane ? 0.22f : -0.22f);
            heat = clamp(heat + 0.06f, 0, 1);
            shx = 6; shy = 4;
            hit(30, SCRAPEI, 5, 80);
        }
        // whoosh as you overtake
        float cprog = c->lapn * TRACK_LEN + c->z;
        if (cprog < pprog && ahead < SEGL * 3 && ahead > 0 && c->wcd == 0
            && speed > c->basespd) {
            note(50, WHOOSHI, 3); c->wcd = 30;
        }
    }

    position = wrapf(position + DT * speed, TRACK_LEN);

    // ---- lap timing ----
    if (prev_pos - position > TRACK_LEN * 0.5f) {
        last_lap = timer(); timer_reset();
        if (best_lap == 0 || last_lap < best_lap) {
            best_lap = last_lap; save(0, (int)(best_lap * 1000));
        }
        lap++;
        if (lap > LAPS) finished = true;
    }
    prev_pos = position;

    // FOV pulse toward a wider lens on boost
    fov = lerp(fov, boosting ? 1.16f : 1.0f, 0.12f);

    // sparks
    for (int i = 0; i < 60; i++)
        if (spark[i].life > 0) {
            spark[i].x += spark[i].vx; spark[i].y += spark[i].vy;
            spark[i].vy += 0.22f; spark[i].life--;
        }

    // shake decays
    shx *= 0.80f; shy *= 0.80f;
    if (boom_t > 0) boom_t--;
}

// ====================================================================
// drawing
// ====================================================================

static void texquad(int x0, int y0, float u0, float v0,
                    int x1, int y1, float u1, float v1,
                    int x2, int y2, float u2, float v2,
                    int x3, int y3, float u3, float v3) {
    tritex(x0, y0, u0, v0, x1, y1, u1, v1, x2, y2, u2, v2);
    tritex(x0, y0, u0, v0, x2, y2, u2, v2, x3, y3, u3, v3);
}

static void draw_background(void) {
    // sky gradient bands
    rectfill(0, 0,      SCREEN_W, HORIZON - 26, CLR_DARKER_PURPLE);
    rectfill(0, HORIZON - 26, SCREEN_W, 14,     CLR_DARK_PURPLE);
    rectfill(0, HORIZON - 12, SCREEN_W, 12,     CLR_DARK_RED);
    // distant canyon rim silhouette (fills the gap above the walls at the horizon)
    for (int x = 0; x < SCREEN_W; x++) {
        float u = (x + position * 0.0008f) * 0.025f;
        int h = (int)(noise(u) * 26 + noise(u * 2.6f + 17) * 12);
        line(x, HORIZON - h, x, HORIZON, CLR_BROWNISH_BLACK);
    }
    // canyon-shadow floor base under everything
    rectfill(0, HORIZON, SCREEN_W, SCREEN_H - HORIZON, CLR_BROWNISH_BLACK);
}

// a pod billboard from primitives — scales cleanly to any distance, recolored
static void draw_pod(int cx, int cy, int s, int col, bool glow) {
    if (s < 2) { pset(cx, cy, col); return; }
    int w = s, h = max(2, s / 2);
    // shadow on the track
    ovalfill(cx, cy + 2, w, max(1, h / 3), CLR_BROWNISH_BLACK);
    // tethers from the two engines back to the cockpit
    line(cx - w, cy, cx, cy + h / 2, CLR_DARK_GREY);
    line(cx + w, cy, cx, cy + h / 2, CLR_DARK_GREY);
    // two engine nacelles
    ovalfill(cx - w, cy, max(1, w / 2), max(1, h / 2), col);
    ovalfill(cx + w, cy, max(1, w / 2), max(1, h / 2), col);
    // intakes / glow facing us
    int gc = glow ? CLR_YELLOW : CLR_DARK_ORANGE;
    ovalfill(cx - w, cy, max(1, w / 3), max(1, h / 3), gc);
    ovalfill(cx + w, cy, max(1, w / 3), max(1, h / 3), gc);
    // cockpit
    circfill(cx, cy + h / 2, max(1, w / 3), CLR_LIGHT_GREY);
}

static void draw_player_pod(void) {
    int jitter = (speed > MAX_SPD * 0.8f) ? rnd(3) - 1 : 0;
    int cx = SCREEN_W / 2 + jitter;
    int cy = SCREEN_H - 28 + (blink(3) ? 1 : 0);
    int lean = (btn(0, BTN_LEFT) || btn(1, BTN_LEFT)) ? -4 : 0;
    lean    += (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) ? 4 : 0;
    cx += lean;

    int spread = 30;                         // engine separation
    // flames behind each engine — bigger + whiter under thrust/boost
    float thrust = boosting ? 1.0f : (speed / MAX_SPD);
    int   fl = 6 + (int)(thrust * 14) + rnd(3);
    int   fcol = boosting ? CLR_WHITE : (thrust > 0.6f ? CLR_YELLOW : CLR_ORANGE);
    for (int side = -1; side <= 1; side += 2) {
        int ex = cx + side * spread;
        ovalfill(ex, cy + 14, 4, fl, CLR_DARK_ORANGE);
        ovalfill(ex, cy + 12, 3, fl - 3, CLR_ORANGE);
        ovalfill(ex, cy + 10, 2, fl - 5, fcol);
    }
    // engine nacelles (the podracer's two pods)
    for (int side = -1; side <= 1; side += 2) {
        int ex = cx + side * spread;
        ovalfill(ex, cy, 9, 7, CLR_DARK_RED);
        ovalfill(ex, cy - 1, 9, 5, CLR_RED);
        ovalfill(ex, cy + 2, 6, 3, CLR_DARKER_GREY);   // exhaust mouth
    }
    // tethers + cockpit slung between/behind
    line(cx - spread, cy + 2, cx, cy + 12, CLR_MEDIUM_GREY);
    line(cx + spread, cy + 2, cx, cy + 12, CLR_MEDIUM_GREY);
    ovalfill(cx, cy + 13, 7, 4, CLR_LIGHT_GREY);
    ovalfill(cx, cy + 12, 4, 2, CLR_BLUE);             // canopy glint
}

static void draw_speed_lines(void) {
    float spd_pct = speed / MAX_SPD;
    if (spd_pct < 0.65f && !boosting) return;
    int n = boosting ? 16 : 9;
    int col = boosting ? CLR_WHITE : CLR_LIGHT_PEACH;
    for (int i = 0; i < n; i++) {
        float a = rnd(360);
        float r0 = 28 + rnd(20), r1 = r0 + 18 + (int)(spd_pct * 26) + rnd(20);
        int cx = SCREEN_W / 2, cy = HORIZON + 8;
        line(cx + (int)dx(r0, a), cy + (int)dy(r0, a),
             cx + (int)dx(r1, a), cy + (int)dy(r1, a), col);
    }
}

void draw(void) {
    draw_background();

    int   base_i  = (int)(position / SEGL);
    float base_pc = (position - base_i * SEGL) / SEGL;

    int   ps  = seg_index(position + PLAYER_Z);
    float pz  = position + PLAYER_Z;
    float ppc = (pz - (int)(pz / SEGL) * SEGL) / SEGL;
    float camY = lerp(seg_y[ps], seg_y[(ps + 1) % N_SEG], ppc) + CAM_H;

    // ---- project every boundary, accumulating the curve ----
    float x  = 0;
    float dxc = -(seg_curve[base_i % N_SEG] * base_pc);
    float camBaseX = playerX * ROAD_W;
    float HW = SCREEN_W / 2.0f, HH = SCREEN_H / 2.0f;

    for (int k = 0; k <= DRAW_DIST; k++) {
        int   idx  = (base_i + k) % N_SEG;
        float camz = (base_i + k) * SEGL - position;
        bvalid[k]  = camz > 1.0f;
        float sc   = bvalid[k] ? (CAM_D / camz) : 0.0f;
        bscale[k]  = sc;
        bx[k] = (int)(HW + sc * (x - camBaseX) * HW * fov);
        by[k] = (int)(HH - sc * (seg_y[idx] - camY) * HH);
        bw[k] = (int)(sc * ROAD_W * HW * fov);
        wh[k] = (int)(sc * WALL_H * HH * fov);
        if (k < DRAW_DIST) { x += dxc; dxc += seg_curve[(base_i + k) % N_SEG]; }
    }

    // ---- canyon geometry, far -> near (painter's) ----
    for (int n = DRAW_DIST - 1; n >= 0; n--) {
        if (!bvalid[n] || !bvalid[n + 1]) continue;
        int y1 = by[n], y2 = by[n + 1];
        if (y1 <= y2) continue;                          // over a crest -> hidden

        int xL1 = bx[n] - bw[n],     xR1 = bx[n] + bw[n];
        int xL2 = bx[n + 1] - bw[n + 1], xR2 = bx[n + 1] + bw[n + 1];
        int yT1 = y1 - wh[n], yT2 = y2 - wh[n + 1];

        // tech-floor ground (tritex)
        texquad(xL1, y1, GRND_U0, GRND_V1,  xL2, y2, GRND_U0, GRND_V0,
                xR2, y2, GRND_U1, GRND_V0,  xR1, y1, GRND_U1, GRND_V1);

        // rock walls (tritex) — strata stretched up the chasm
        texquad(xL1, y1, ROCK_U0, ROCK_V1,  xL2, y2, ROCK_U1, ROCK_V1,
                xL2, yT2, ROCK_U1, ROCK_V0, xL1, yT1, ROCK_U0, ROCK_V0);
        texquad(xR1, y1, ROCK_U1, ROCK_V1,  xR2, y2, ROCK_U0, ROCK_V1,
                xR2, yT2, ROCK_U0, ROCK_V0, xR1, yT1, ROCK_U1, ROCK_V0);

        // glowing edge strips at the foot of each wall (lane guides). GPU line(),
        // NOT trifill — trifill is a software per-pixel fill (see the haze note).
        bool bright = ((base_i + n) / 3) % 2 == 0;
        int edge = bright ? CLR_BLUE : CLR_TRUE_BLUE;
        line(xL1, y1, xL2, y2, edge);
        line(xR1, y1, xR2, y2, edge);
    }

    // ---- distance haze ----
    // NB: trifill() is a SOFTWARE per-pixel rasterizer (poly_fill_cov in studio.c).
    // The old haze dithered the far third with 6 big trifills PER far segment —
    // ~190 large software-filled triangles a frame, covering the tall canyon walls.
    // THAT (not the tritex geometry, which is GPU) is what made this cart chug.
    // Two dithered bands near the horizon give the same fade for a fraction of the
    // cost: each is one GPU textured-quad (fillp + rectfill), holes transparent so
    // the canyon shows through the dither.
    fillp(0x7BDE, -1);
    rectfill(0, HORIZON - 8, SCREEN_W, 30, CLR_DARKER_PURPLE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, HORIZON - 4, SCREEN_W, 12, CLR_DARKER_PURPLE);
    fillp_reset();

    // ---- screen shake wraps the world layer (pods + sparks) ----
    int ox = (int)rnd_float_between(-shx, shx), oy = (int)rnd_float_between(-shy, shy);
    camera(ox, oy);

    // CPU pods, far -> near
    for (int n = DRAW_DIST - 1; n >= 1; n--) {
        for (int i = 0; i < NCPU; i++) {
            float cz = cpu[i].z; while (cz < position) cz += TRACK_LEN;
            int sn = (int)((cz - position) / SEGL);
            if (sn != n || !bvalid[sn]) continue;
            float sc = bscale[sn];
            int px = bx[sn] + (int)(sc * cpu[i].lane * ROAD_W * HW * fov);
            int py = by[sn];
            int s  = (int)(sc * 460.0f * HH * fov);
            if (s < 1) continue;
            draw_pod(px, py, s, cpu[i].col, true);
        }
    }

    // scrape sparks
    for (int i = 0; i < 60; i++)
        if (spark[i].life > 0) {
            int r = spark[i].life > 12 ? 1 : 0;
            if (r) circfill((int)spark[i].x, (int)spark[i].y, 1, spark[i].col);
            else   pset((int)spark[i].x, (int)spark[i].y, spark[i].col);
        }

    draw_player_pod();
    draw_speed_lines();
    camera(0, 0);

    // sonic-boom + blowout full-screen flash
    if (boom_t > 0) { fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE); fillp_reset(); }

    // ---- HUD ----
    int kmh = (int)(speed / MAX_SPD * 320);
    print(str("%3d", kmh), 4, 4, boosting ? CLR_YELLOW : CLR_WHITE);
    print("KM/H", 30, 4, CLR_LIGHT_GREY);

    // position
    float pprog = player_prog();
    int rank = 1;
    for (int i = 0; i < NCPU; i++)
        if (cpu[i].lapn * TRACK_LEN + cpu[i].z > pprog) rank++;
    print_centered(str("%d/%d", rank, NCPU + 1), SCREEN_W/2, 4, CLR_WHITE);
    print_right(str("LAP %d/%d", lap, LAPS), SCREEN_W - 4, 4, CLR_LIGHT_GREY);

    // heat gauge — glows + pulses red near the blowout
    int hx = SCREEN_W - 60, hy = 16;
    int hot = heat > 0.8f ? (blink(3) ? CLR_RED : CLR_DARK_RED)
            : heat > 0.5f ? CLR_ORANGE : CLR_LIME_GREEN;
    bar(hx, hy, 54, 7, heat, hot, CLR_DARKER_GREY);
    print_right("HEAT", hx - 2, hy, heat > 0.8f ? CLR_RED : CLR_LIGHT_GREY);
    if (boosting) print(">>BOOST", hx - 2, hy + 10, blink(2) ? CLR_WHITE : CLR_YELLOW);

    if (best_lap > 0) print(str("BEST %.1fs", best_lap), 4, SCREEN_H - 11, CLR_YELLOW);
    if (last_lap > 0) print_right(str("LAST %.1fs", last_lap), SCREEN_W - 4, SCREEN_H - 11, CLR_LIGHT_GREY);

    if (stalled) {
        print_centered("!! OVERHEAT !!", SCREEN_W/2, HORIZON - 24, blink(3) ? CLR_RED : CLR_YELLOW);
        print_centered("venting...", SCREEN_W/2, HORIZON - 12, CLR_LIGHT_GREY);
    }

    if (finished) {
        rectfill(70, 70, 180, 56, CLR_BLACK);
        rect(70, 70, 180, 56, CLR_YELLOW);
        print_centered("RACE COMPLETE", SCREEN_W/2, 80, CLR_YELLOW);
        print_centered(str("finished %d / %d", rank, NCPU + 1), SCREEN_W/2, 94, CLR_WHITE);
        print_centered(str("best lap %.1fs", best_lap), SCREEN_W/2, 106, CLR_LIME_GREEN);
        print_centered("A to race again", SCREEN_W/2, 118, CLR_LIGHT_GREY);
    }

    // perf read-out while tuning the software-trifill haze cost (F1 toggles the
    // debug overlay; also captured by the play.js trace). 60 = smooth.
    watch("fps", "%d", fps());
}
