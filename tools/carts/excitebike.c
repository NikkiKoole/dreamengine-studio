/* de:meta
{
  "title": "excitebike",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "state-machine",
    "screen-shake-juice",
    "save-load-persistence"
  ],
  "lineage": "Homage to NES Excitebike (1984); the lander.c air-physics idiom turned sideways, with a mouse-driven tile-grid track editor persisted via save_bytes() and pal()-recolored CPU rivals.",
  "genre": "racing",
  "homage": "Excitebike (1984)",
  "description": "Side-view NES motocross with the famous landing-angle skill, an overheat meter, and a track editor. Hit a ramp and you launch into the air with the nose drifting down on its own — hold Up/Down to control the tilt and touch down within a hair of LEVEL to keep your speed; nose-down or tail-down and you wipe out. Hold turbo for more top speed but the temp gauge climbs to a flashing-red redline and a stall, so mud (slow but cooling) becomes a real choice. Race one track against three pal()-recolored CPU rivals you bump past, chasing your best lap (saved). Then switch to BUILD: a mouse editor paints ramps, jumps, whoops and mud onto the lane grid, persisted with save_bytes — save it and test-ride instantly. RACE: Right/Z throttle, X turbo, Up/Down tilt in the air, Left brake. BUILD: 1-5 or click the palette, left-click paint / right-click erase, arrows scroll, S save, Z ride, B title."
}
de:meta */
#include "studio.h"

// EXCITEBIKE — side-view motocross with the famous LANDING-ANGLE skill,
// an OVERHEAT meter, and a mouse TRACK EDITOR you can save & ride.
//
// RACE : Right/Z throttle · X turbo (heats up!) · Up/Down tilt in the air
//        · Left brake. Land roughly LEVEL off a jump to keep your speed —
//        nose-down or tail-down = wipeout. Redline the engine = stall.
// BUILD: mouse paints track pieces (1-5 or click the palette), S saves,
//        Z test-rides, B back to title.
//
// The air physics are the lander.c idiom turned sideways; the track is an
// editable grid persisted with save_bytes(); CPU rivals are pal()-recolors
// of one bike sprite. Engine pitch rides the throttle through a filtered saw.

// ---- tuning ------------------------------------------------------
#define COL_W       24
#define TRACK_LEN   128
#define WORLD_W     (TRACK_LEN * COL_W)
#define GROUND_Y    165             // ground surface line (px)
#define BIKE_W      16
#define BIKE_H      16
#define REST_Y      (GROUND_Y - BIKE_H)

#define SLOT_BIKE   1
#define SLOT_CRASH  2

#define GRAV        900.0f          // air gravity px/s^2
#define NOSE_DROP   30.0f           // passive nose-down rotation deg/s in air
#define PITCH_UP    115.0f
#define PITCH_DN    135.0f
#define LAND_TOL    24.0f           // |angle| within this = clean landing
#define PERFECT_TOL 9.0f

#define MAXSPD      210.0f
#define TURBOSPD    300.0f
#define MUD_MAX     115.0f
#define ACCEL       150.0f
#define TURBO_ACC   240.0f
#define BRAKE       260.0f
#define FRICTION    70.0f

#define NB          4               // bikes[0] = player, 1..3 = CPU
#define FINISH_COL  (TRACK_LEN - 4)
#define FINISH_X    (FINISH_COL * COL_W)

// track piece types
enum { P_FLAT, P_RAMP, P_BUMP, P_JUMP, P_MUD, P_COUNT };
#define RAMP_H  18
#define JUMP_H  34

// bike states
enum { ST_RIDING, ST_AIR, ST_CRASH, ST_STALL };

// modes & race phases
enum { MODE_TITLE, MODE_RACE, MODE_BUILD };
enum { PH_COUNT, PH_RUN, PH_RESULT };

#define ABSF(v) ((v) < 0 ? -(v) : (v))
#define FMAX(a, b) ((a) > (b) ? (a) : (b))   // max() is int-only here

typedef struct {
    float x, by;      // world x (sprite left), sprite-top y
    float vy;         // vertical velocity (air)
    float angle;      // tilt deg, + = nose up
    float speed;      // horizontal px/s
    float heat;       // 0..1 (player)
    int   state;
    float st_t;       // state timer
    int   color;      // shirt recolor index (8 = player red)
    float target;     // CPU target speed
    int   armed;      // last ramp col launched from (-1 = none)
    int   bigAir;     // launched off a JUMP?
    int   finished;
    float finishMs;
    float squash;     // landing squash 0..1
    float wob;        // wobble tilt offset (deg), decays
} Bike;

// ---- particles ---------------------------------------------------
#define MAX_DUST 160
typedef struct { float x, y, vx, vy, life, maxlife, size; int color; } Dust;
static Dust dust[MAX_DUST];

static void spawn_dust(float x, float y, float vx, float vy, float life, float size, int color) {
    for (int i = 0; i < MAX_DUST; i++) if (dust[i].life <= 0) {
        dust[i] = (Dust){ x, y, vx, vy, life, life, size, color };
        return;
    }
}

// ---- persisted state ---------------------------------------------
#define SAVE_MAGIC 0x42494B45       // 'BIKE'
typedef struct { int magic; int bestMs; unsigned char track[TRACK_LEN]; } Save;
static Save sv;
static unsigned char *trk;          // = sv.track

// ---- globals -----------------------------------------------------
static int   mode = MODE_TITLE;
static int   menuSel;               // title: 0=RACE 1=BUILD
static Bike  bikes[NB];
static int   racePhase;
static float raceClock, countT;
static int   playerPlace, finishedCount;
static float perfectT;              // "PERFECT!" popup timer
static int   buildCamX, selPiece = P_RAMP;
static float savedT;                // "SAVED" flash

// ================================================================
//  track helpers
// ================================================================
static int piece_at(float worldx) {
    int c = (int)(worldx / COL_W);
    if (c < 0 || c >= TRACK_LEN) return P_FLAT;
    return trk[c];
}

static void gen_default_track(void) {
    for (int c = 0; c < TRACK_LEN; c++) {
        if (c < 6 || c > FINISH_COL - 2)        trk[c] = P_FLAT;
        else if (c % 17 == 8)                   trk[c] = P_RAMP;
        else if (c % 29 == 14)                  trk[c] = P_JUMP;
        else if (c % 19 >= 15 && c % 19 <= 17)  trk[c] = P_MUD;
        else if (c % 13 == 5 || c % 13 == 6)    trk[c] = P_BUMP;
        else                                    trk[c] = P_FLAT;
    }
}

static void persist(void) { sv.magic = SAVE_MAGIC; save_bytes(&sv, sizeof sv); }

// ================================================================
//  sound
// ================================================================
static void setup_audio(void) {
    // slot 5 — engine: a filtered saw, pitch driven by speed each frame
    instrument(5, INSTR_SAW, 3, 30, 6, 25);
    instrument_filter(5, FILTER_LOW, 600, 4);
    instrument_lfo(5, 0, LFO_PITCH, 7.0f, 0.25f);
    // slot 6 — turbo whine: thin nasal square with vibrato
    instrument(6, INSTR_SQUARE, 5, 60, 6, 40);
    instrument_duty(6, 0.18f);
    instrument_lfo(6, 0, LFO_PITCH, 6.0f, 0.4f);
}

static void fanfare(void) {
    schedule(0,   60, INSTR_SQUARE, 4);
    schedule(110, 64, INSTR_SQUARE, 4);
    schedule(220, 67, INSTR_SQUARE, 4);
    schedule(330, 72, INSTR_SINE,   5);
}

// ================================================================
//  race setup
// ================================================================
static void start_race(void) {
    mode = MODE_RACE;
    racePhase = PH_COUNT; countT = 3.0f; raceClock = 0;
    finishedCount = 0; playerPlace = 0; perfectT = 0;
    for (int i = 0; i < NB; i++) {
        Bike *b = &bikes[i];
        b->x = (i == 0) ? 24 : (float)(120 + i * 90);
        b->by = REST_Y; b->vy = 0; b->angle = 0; b->wob = 0;
        b->speed = 0; b->heat = 0; b->state = ST_RIDING; b->st_t = 0;
        b->armed = -1; b->bigAir = 0; b->finished = 0; b->finishMs = 0;
        b->squash = 0;
    }
    bikes[0].color = CLR_RED;        // player
    int cc[3] = { CLR_GREEN, CLR_TRUE_BLUE, CLR_YELLOW };
    float tg[3] = { 165, 178, 152 };
    for (int i = 1; i < NB; i++) { bikes[i].color = cc[i-1]; bikes[i].target = tg[i-1]; }
    for (int i = 0; i < MAX_DUST; i++) dust[i].life = 0;
}

// ================================================================
//  physics — one bike
// ================================================================
static void try_launch(Bike *b) {
    if (b->state != ST_RIDING || b->speed < 45) return;
    int c = (int)((b->x + 14) / COL_W);
    int p = (c >= 0 && c < TRACK_LEN) ? trk[c] : P_FLAT;
    if ((p == P_RAMP || p == P_JUMP) && b->armed != c) {
        b->armed = c;
        b->state = ST_AIR; b->st_t = 0;
        float v = (p == P_JUMP) ? (300 + b->speed * 0.9f) : (200 + b->speed * 0.7f);
        b->vy = -clamp(v, 0, p == P_JUMP ? 360 : 260);
        b->angle = 16;                 // natural nose-up off the lip
        b->bigAir = (p == P_JUMP);
        // dirt off the back wheel
        for (int k = 0; k < 6; k++)
            spawn_dust(b->x + 2, GROUND_Y, rnd_float_between(-40, -10),
                       rnd_float_between(-90, -20), 0.5f, 2, CLR_BROWN);
    }
}

static void land(Bike *b, int isPlayer) {
    b->by = REST_Y;
    float a = ABSF(b->angle);
    if (a <= LAND_TOL) {
        b->state = ST_RIDING; b->squash = 1.0f;
        for (int k = 0; k < 10; k++)
            spawn_dust(b->x + 4 + rnd(8), GROUND_Y, rnd_float_between(-60, 60),
                       rnd_float_between(-70, -10), 0.45f, 2,
                       k % 2 ? CLR_BROWN : CLR_DARK_PEACH);
        if (isPlayer) {
            shake(2);
            if (b->bigAir && a <= PERFECT_TOL) {
                b->speed += 32; perfectT = 1.1f;
                schedule(0, 72, INSTR_SINE, 5); schedule(70, 79, INSTR_SINE, 5);
            } else note(58, INSTR_SINE, 3);
        }
        b->angle = 0; b->armed = -1;
    } else {
        b->state = ST_CRASH; b->st_t = 0; b->speed = 0; b->angle = 0;
        for (int k = 0; k < 18; k++)
            spawn_dust(b->x + 8, GROUND_Y - 2, rnd_float_between(-110, 110),
                       rnd_float_between(-120, -20), 0.7f, 2,
                       k % 3 ? CLR_BROWN : CLR_LIGHT_GREY);
        if (isPlayer) { shake(6); hit(36, INSTR_NOISE, 6, 300); note(40, INSTR_SAW, 4); }
    }
    b->bigAir = 0;
}

static void update_bike(Bike *b, int i) {
    int isPlayer = (i == 0);
    float d = dt();

    // --- per-state ---
    if (b->state == ST_CRASH) {
        b->st_t += d;
        if (b->st_t > 1.2f) { b->state = ST_RIDING; b->speed = 45; }
        return;
    }
    if (b->state == ST_STALL) {                 // player overheated
        b->st_t += d; b->speed = FMAX(0.0f, b->speed - 400 * d);
        b->heat = FMAX(0.0f, b->heat - 0.4f * d);
        if (b->st_t > 1.6f) { b->state = ST_RIDING; b->heat = 0.5f; }
        b->x += b->speed * d;
        return;
    }

    int curPiece = piece_at(b->x + 8);

    if (b->state == ST_AIR) {
        b->vy += GRAV * d; b->by += b->vy * d;
        if (isPlayer) {
            b->angle -= NOSE_DROP * d;          // nose drifts down — you fight it
            if (btn(0, BTN_UP))   b->angle += PITCH_UP * d;
            if (btn(0, BTN_DOWN)) b->angle -= PITCH_DN * d;
        } else {
            b->angle = lerp(b->angle, 0, 8 * d);   // CPU auto-levels
        }
        b->angle = clamp(b->angle, -90, 90);
        b->x += b->speed * d;
        if (b->by >= REST_Y) land(b, isPlayer);
        return;
    }

    // --- ST_RIDING ---
    if (isPlayer) {
        int throttle = btn(0, BTN_RIGHT) || btn(0, BTN_A);
        int turbo    = btn(0, BTN_B) && throttle;
        int brake    = btn(0, BTN_LEFT);
        float topspd = (curPiece == P_MUD) ? MUD_MAX : (turbo ? TURBOSPD : MAXSPD);

        if (brake)         b->speed = FMAX(0.0f, b->speed - BRAKE * d);
        else if (throttle) b->speed += (turbo ? TURBO_ACC : ACCEL) * d;
        else               b->speed = FMAX(0.0f, b->speed - FRICTION * d);
        b->speed = clamp(b->speed, 0, topspd);

        // heat
        if (turbo) b->heat += 0.42f * d;
        else       b->heat -= (curPiece == P_MUD ? 0.55f : 0.20f) * d;
        b->heat = clamp(b->heat, 0, 1);
        if (b->heat >= 1.0f) {
            b->state = ST_STALL; b->st_t = 0; shake(4);
            hit(30, INSTR_NOISE, 6, 250);
        }
        // overheat alarm
        if (b->heat > 0.82f && frame() % 18 == 0)
            note(72 + (int)(b->heat * 10), INSTR_SQUARE, 4);
    } else {
        float topspd = (curPiece == P_MUD) ? MUD_MAX : b->target;
        if (b->speed < topspd) b->speed += 140 * d;
        else                   b->speed = FMAX(topspd, b->speed - 60 * d);
    }

    // bump: bob + small slowdown + wobble, no air
    if (curPiece == P_BUMP) {
        b->wob = sin_deg(b->x * 6) * 7;
        if (b->speed > 130) b->speed -= 40 * d;
        if (isPlayer && frame() % 9 == 0) {
            hit(46, INSTR_NOISE, 2, 35);
            spawn_dust(b->x + 2, GROUND_Y, rnd_float_between(-30, 0),
                       rnd_float_between(-50, -10), 0.35f, 1, CLR_BROWN);
        }
    } else if (b->wob != 0) {
        b->wob -= b->wob * clamp(6 * d, 0, 1);
        if (ABSF(b->wob) < 0.4f) b->wob = 0;
    }

    // rear-wheel dirt spray while driving hard
    if (b->speed > 90 && frame() % 4 == 0)
        spawn_dust(b->x + 1, GROUND_Y, rnd_float_between(-50, -15),
                   rnd_float_between(-40, -5), 0.3f, 1,
                   curPiece == P_MUD ? CLR_DARK_BROWN : CLR_BROWN);

    b->x += b->speed * d;
    try_launch(b);

    // finish line
    if (!b->finished && b->x + 8 >= FINISH_X) {
        b->finished = 1; b->finishMs = raceClock * 1000;
        if (isPlayer) {
            playerPlace = 1 + finishedCount;
            racePhase = PH_RESULT;
            if (sv.bestMs == 0 || b->finishMs < sv.bestMs) { sv.bestMs = (int)b->finishMs; }
            persist();
            schedule(0, 72, INSTR_SINE, 5); schedule(120, 76, INSTR_SINE, 5);
            schedule(240, 79, INSTR_SINE, 5);
        } else finishedCount++;
    }
}

// ================================================================
//  update — RACE
// ================================================================
static void update_race(void) {
    if (racePhase == PH_COUNT) {
        countT -= dt();
        if (countT <= 0) { racePhase = PH_RUN; raceClock = 0; }
        return;
    }
    if (racePhase == PH_RESULT) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) { mode = MODE_TITLE; fanfare(); }
        return;
    }

    // RUN
    raceClock += dt();
    if (perfectT > 0) perfectT -= dt();

    for (int i = 0; i < NB; i++) update_bike(&bikes[i], i);

    // player vs CPU bumps (both grounded, overlapping)
    Bike *p = &bikes[0];
    if (p->state == ST_RIDING) {
        for (int i = 1; i < NB; i++) {
            Bike *c = &bikes[i];
            if (c->state != ST_RIDING) continue;
            if (ABSF(p->x - c->x) < 13 && boxes_touch((int)p->x, (int)p->by, BIKE_W, BIKE_H,
                                                       (int)c->x, (int)c->by, BIKE_W, BIKE_H)) {
                float pushed = p->x < c->x ? -1.0f : 1.0f;
                p->speed *= 0.72f; c->speed *= 0.72f;
                p->wob = pushed * 12; c->wob = -pushed * 12;
                p->x += pushed * 2;  c->x -= pushed * 2;
                if (frame() % 5 == 0) { shake(2); hit(50, INSTR_NOISE, 3, 40); }
            }
        }
    }

    // engine drone — pitch rides throttle
    if (p->state == ST_RIDING || p->state == ST_AIR) {
        int cut = 450 + (int)(p->speed * 2.2f);
        instrument_filter(5, FILTER_LOW, cut, 4);
        if (frame() % 4 == 0) {
            int m = 28 + (int)(p->speed * 0.085f);
            hit(m, 5, 2, 95);
        }
        if (btn(0, BTN_B) && (btn(0, BTN_RIGHT) || btn(0, BTN_A)) && frame() % 6 == 0)
            hit(76 + (int)(p->speed * 0.04f), 6, 2, 90);   // turbo whine
    }
}

// ================================================================
//  update — BUILD
// ================================================================
static void update_build(void) {
    // palette select (keys 1-5)
    for (int i = 0; i < P_COUNT; i++) if (keyp('1' + i)) selPiece = i;

    // scroll
    if (btn(0, BTN_LEFT))  buildCamX -= (int)(260 * dt());
    if (btn(0, BTN_RIGHT)) buildCamX += (int)(260 * dt());
    buildCamX -= (int)(mouse_wheel() * 40);
    buildCamX = (int)clamp((float)buildCamX, 0, WORLD_W - SCREEN_W);

    int mx = mouse_x(), my = mouse_y();

    // click palette buttons (top bar)
    if (mouse_pressed(MOUSE_LEFT) && my < 20) {
        int bw = SCREEN_W / P_COUNT;
        int hit_i = mx / bw;
        if (hit_i >= 0 && hit_i < P_COUNT) selPiece = hit_i;
    } else if (my >= 22) {
        // paint onto the track lane
        int col = (mx + buildCamX) / COL_W;
        if (col >= 1 && col <= FINISH_COL - 1) {
            if (mouse_down(MOUSE_LEFT))  trk[col] = selPiece;
            if (mouse_down(MOUSE_RIGHT)) trk[col] = P_FLAT;
        }
    }

    if (keyp('S')) { persist(); savedT = 1.4f; note(76, INSTR_SINE, 4); }
    if (btnp(0, BTN_A))                       start_race();   // Z test-ride
    if (btnp(0, BTN_B) || keyp(KEY_ESCAPE)) { mode = MODE_TITLE; }
    if (savedT > 0) savedT -= dt();
}

// ================================================================
//  update
// ================================================================
void init(void) {
    trk = sv.track;
    int n = load_bytes(&sv, sizeof sv);
    if (n != (int)sizeof sv || sv.magic != SAVE_MAGIC) {
        sv.magic = SAVE_MAGIC; sv.bestMs = 0; gen_default_track();
    }
    colorkey(0);
    setup_audio();
    fanfare();
}

void update(void) {
    if (mode == MODE_TITLE) {
        if (btnp(0, BTN_UP) || btnp(0, BTN_DOWN)) { menuSel ^= 1; note(70, INSTR_SQUARE, 3); }
        if (btnp(0, BTN_A) || keyp(KEY_ENTER)) {
            if (menuSel == 0) start_race(); else { mode = MODE_BUILD; buildCamX = 0; }
        }
    } else if (mode == MODE_RACE) {
        update_race();
        if (keyp(KEY_ESCAPE)) mode = MODE_TITLE;
    } else {
        update_build();
    }

    // age dust everywhere
    for (int i = 0; i < MAX_DUST; i++) if (dust[i].life > 0) {
        dust[i].vy += 240 * dt();
        dust[i].x  += dust[i].vx * dt();
        dust[i].y  += dust[i].vy * dt();
        dust[i].life -= dt();
    }
}

// ================================================================
//  drawing
// ================================================================
static void draw_backdrop(int camX) {
    // sky gradient (dithered bands)
    cls(CLR_DARK_BLUE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 40, CLR_BLUE);
    rectfill(0, 30, SCREEN_W, 30, CLR_TRUE_BLUE);
    fillp_reset();

    // far hills (parallax 0.2)
    int hx = -(int)(camX * 0.2f);
    for (int i = -1; i < 6; i++) {
        int cx = hx + i * 150 + (i * 53 % 40);
        ovalfill(cx, GROUND_Y - 20, 80, 46, CLR_DARK_GREEN);
    }
    // clouds (parallax 0.3)
    int clx = -(int)(camX * 0.3f);
    for (int i = 0; i < 6; i++) {
        int cx = ((clx + i * 130) % (SCREEN_W + 120)) - 60;
        int cy = 14 + (i * 27) % 30;
        ovalfill(cx, cy, 16, 6, CLR_WHITE);
        ovalfill(cx + 10, cy + 2, 12, 5, CLR_WHITE);
    }
    // grandstand + crowd (parallax 0.5)
    int gx = -(int)(camX * 0.5f);
    for (int s = -1; s < 8; s++) {
        int sx = gx + s * 96;
        rectfill(sx, GROUND_Y - 34, 88, 20, CLR_DARK_GREY);
        rectfill(sx, GROUND_Y - 16, 88, 6, CLR_BROWN);
        for (int k = 0; k < 30; k++) {
            int px = sx + 4 + (k * 13) % 80;
            int py = GROUND_Y - 32 + (k * 7) % 14;
            int cc[5] = { CLR_RED, CLR_YELLOW, CLR_WHITE, CLR_GREEN, CLR_PINK };
            pset(px, py, cc[(k + s) % 5]);
        }
    }
}

static void draw_track(int camX) {
    // ground slab
    rectfill(0, GROUND_Y, WORLD_W, SCREEN_H - GROUND_Y, CLR_BROWN);
    rectfill(0, GROUND_Y, WORLD_W, 3, CLR_DARK_PEACH);

    int c0 = max(0, camX / COL_W - 1);
    int c1 = min(TRACK_LEN - 1, (camX + SCREEN_W) / COL_W + 1);
    for (int c = c0; c <= c1; c++) {
        int x = c * COL_W;
        switch (trk[c]) {
        case P_RAMP:
            trifill(x, GROUND_Y, x + COL_W, GROUND_Y, x + COL_W, GROUND_Y - RAMP_H, CLR_LIGHT_GREY);
            line(x + COL_W, GROUND_Y, x + COL_W, GROUND_Y - RAMP_H, CLR_WHITE);
            line(x, GROUND_Y, x + COL_W, GROUND_Y - RAMP_H, CLR_WHITE);
            break;
        case P_JUMP:
            trifill(x, GROUND_Y, x + COL_W, GROUND_Y, x + COL_W, GROUND_Y - JUMP_H, CLR_DARK_GREY);
            line(x, GROUND_Y, x + COL_W, GROUND_Y - JUMP_H, CLR_YELLOW);
            rectfill(x + COL_W - 3, GROUND_Y - JUMP_H, 3, 4, CLR_RED);   // takeoff lip
            break;
        case P_BUMP:
            ovalfill(x + COL_W / 2, GROUND_Y, COL_W / 2, 7, CLR_MEDIUM_GREY);
            ovalfill(x + COL_W / 2, GROUND_Y, COL_W / 2 - 3, 5, CLR_LIGHT_GREY);
            break;
        case P_MUD:
            fillp(FILL_CHECKER, CLR_BROWNISH_BLACK);
            rectfill(x, GROUND_Y - 1, COL_W, 6, CLR_DARK_BROWN);
            fillp_reset();
            break;
        default: break;
        }
    }

    // start banner
    rectfill(0, GROUND_Y - 40, 4, 40, CLR_WHITE);
    // finish line (checkered post)
    for (int r = 0; r < 12; r++)
        rectfill(FINISH_X, GROUND_Y - 48 + r * 4, 6, 4, (r % 2) ? CLR_WHITE : CLR_BLACK);
    rectfill(FINISH_X, GROUND_Y - 48, 1, 48, CLR_LIGHT_GREY);
}

static void draw_dust(void) {
    for (int i = 0; i < MAX_DUST; i++) {
        if (dust[i].life <= 0) continue;
        float t = dust[i].life / dust[i].maxlife;
        int r = (int)(dust[i].size * t + 0.5f);
        if (r <= 0) pset((int)dust[i].x, (int)dust[i].y, dust[i].color);
        else        circfill((int)dust[i].x, (int)dust[i].y, r, dust[i].color);
    }
}

static void draw_bike(Bike *b, int isPlayer) {
    int sx = (int)b->x;
    if (b->color != CLR_RED) pal(CLR_RED, b->color);

    if (b->state == ST_AIR) {
        ovalfill(sx + 8, GROUND_Y, 7, 2, CLR_BROWNISH_BLACK);   // shadow
        spr_rot(SLOT_BIKE, sx, (int)b->by, -b->angle);
    } else if (b->state == ST_CRASH) {
        float tumble = b->st_t * 220;
        spr_rot(SLOT_CRASH, sx, (int)b->by, tumble);
    } else { // RIDING / STALL
        float wheelie = (isPlayer && (btn(0, BTN_RIGHT) || btn(0, BTN_A)) && b->speed > 150)
                        ? sin_deg(now() * 600) * 4 : 0;
        float tilt = b->wob + wheelie;
        if (b->squash > 0.02f) {
            int dw = BIKE_W + (int)(6 * b->squash);
            int dh = BIKE_H - (int)(6 * b->squash);
            int ox = (SLOT_BIKE % 8) * 16, oy = (SLOT_BIKE / 8) * 16;
            sspr(ox, oy, 16, 16, sx - (dw - BIKE_W) / 2, GROUND_Y - dh, dw, dh);
        } else if (ABSF(tilt) > 0.5f) {
            spr_rot(SLOT_BIKE, sx, (int)b->by, -tilt);
        } else {
            spr(SLOT_BIKE, sx, (int)b->by);
        }
    }
    if (b->color != CLR_RED) pal_reset();
    if (b->squash > 0) b->squash -= dt() * 4;
}

static const char *fmt_time(int ms) {
    int cs = (ms / 10) % 100, s = (ms / 1000) % 60, m = ms / 60000;
    return str("%d:%02d.%02d", m, s, cs);
}

static void draw_race(void) {
    int camX = (int)clamp(bikes[0].x - 96, 0, WORLD_W - SCREEN_W);
    draw_backdrop(camX);
    camera(camX, 0);
    draw_track(camX);
    draw_dust();
    for (int i = NB - 1; i >= 0; i--) draw_bike(&bikes[i], i == 0);
    camera(0, 0);

    // ---- HUD ----
    Bike *p = &bikes[0];
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(fmt_time((int)(raceClock * 1000)), 4, 2, CLR_WHITE);
    print_right(str("%dKPH", (int)(p->speed)), SCREEN_W - 4, 2, CLR_LIGHT_GREY);

    // temp gauge — flashes red near redline
    int tx = SCREEN_W / 2 - 30;
    int hot = p->heat > 0.82f;
    int tcol = p->heat > 0.82f ? CLR_RED : p->heat > 0.5f ? CLR_ORANGE : CLR_GREEN;
    print("TEMP", tx - 26, 2, hot && blink(4) ? CLR_RED : CLR_LIGHT_GREY);
    bar(tx, 3, 60, 5, p->heat, tcol, CLR_DARK_GREY);
    if (hot && blink(6)) rect(tx - 1, 2, 62, 7, CLR_RED);

    // progress pip
    float prog = clamp(p->x / FINISH_X, 0, 1);
    rectfill(4, SCREEN_H - 5, SCREEN_W - 8, 2, CLR_DARK_GREY);
    rectfill(4 + (int)(prog * (SCREEN_W - 8)) - 1, SCREEN_H - 6, 3, 4, CLR_YELLOW);

    if (perfectT > 0)
        print_centered("PERFECT!", SCREEN_W/2, 40, blink(3) ? CLR_YELLOW : CLR_WHITE);

    if (racePhase == PH_COUNT) {
        int n = (int)countT + 1;
        const char *t = n > 0 ? str("%d", n) : "GO!";
        print_scaled(t, (SCREEN_W - text_width(t) * 4) / 2, SCREEN_H / 2 - 16, CLR_YELLOW, 4);
        print_centered("level off your landings — don't redline", SCREEN_W/2, SCREEN_H - 28, CLR_LIGHT_GREY);
    }
    if (p->state == ST_STALL)  print_centered("OVERHEATED!", SCREEN_W/2, SCREEN_H / 2, blink(4) ? CLR_RED : CLR_ORANGE);
    if (p->state == ST_CRASH)  print_centered("WIPEOUT!", SCREEN_W/2, SCREEN_H / 2, CLR_RED);

    if (racePhase == PH_RESULT) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 28, 140, 60, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 28, 140, 60, CLR_YELLOW);
        const char *pl = playerPlace == 1 ? "1st!" : playerPlace == 2 ? "2nd" : playerPlace == 3 ? "3rd" : "4th";
        print_centered(str("FINISH — %s", pl), SCREEN_W/2, SCREEN_H / 2 - 20, CLR_GREEN);
        print_centered(str("TIME %s", fmt_time((int)p->finishMs)), SCREEN_W/2, SCREEN_H / 2 - 8, CLR_WHITE);
        print_centered(str("BEST %s", fmt_time(sv.bestMs)), SCREEN_W/2, SCREEN_H / 2 + 4, CLR_LIGHT_YELLOW);
        print_centered("Z = title", SCREEN_W/2, SCREEN_H / 2 + 18, CLR_LIGHT_GREY);
    }
}

// ---- BUILD --------------------------------------------------------
static const char *piece_name[P_COUNT] = { "FLAT", "RAMP", "BUMP", "JUMP", "MUD" };

static void draw_piece_icon(int p, int cx, int gy) {
    switch (p) {
    case P_RAMP: trifill(cx - 8, gy, cx + 8, gy, cx + 8, gy - 9, CLR_LIGHT_GREY); break;
    case P_JUMP: trifill(cx - 8, gy, cx + 8, gy, cx + 8, gy - 13, CLR_DARK_GREY);
                 line(cx - 8, gy, cx + 8, gy - 13, CLR_YELLOW); break;
    case P_BUMP: ovalfill(cx, gy, 8, 5, CLR_LIGHT_GREY); break;
    case P_MUD:  fillp(FILL_CHECKER, CLR_BROWNISH_BLACK);
                 rectfill(cx - 8, gy - 4, 16, 4, CLR_DARK_BROWN); fillp_reset(); break;
    default:     rectfill(cx - 8, gy - 2, 16, 2, CLR_BROWN); break;
    }
}

static void draw_build(void) {
    draw_backdrop(buildCamX);
    camera(buildCamX, 0);
    draw_track(buildCamX);
    // ghost piece under cursor
    int col = (mouse_x() + buildCamX) / COL_W;
    if (mouse_y() >= 22 && col >= 1 && col <= FINISH_COL - 1) {
        int gx = col * COL_W;
        rect(gx, GROUND_Y - JUMP_H - 2, COL_W, JUMP_H + 2, CLR_WHITE);
    }
    camera(0, 0);

    // palette bar
    int bw = SCREEN_W / P_COUNT;
    for (int i = 0; i < P_COUNT; i++) {
        int bx = i * bw;
        rectfill(bx, 0, bw - 1, 20, i == selPiece ? CLR_DARK_GREY : CLR_BROWNISH_BLACK);
        if (i == selPiece) rect(bx, 0, bw - 1, 20, CLR_YELLOW);
        draw_piece_icon(i, bx + bw / 2, 17);
        print(str("%d", i + 1), bx + 2, 2, CLR_LIGHT_GREY);
        print(piece_name[i], bx + bw / 2 - text_width(piece_name[i]) / 2, 11, CLR_WHITE);
    }

    rectfill(0, SCREEN_H - 10, SCREEN_W, 10, CLR_BLACK);
    print("L-click paint  R-click erase  arrows scroll", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
    print_right("S save  Z ride  B title", SCREEN_W - 4, SCREEN_H - 9, CLR_YELLOW);
    if (savedT > 0) print_centered("TRACK SAVED", SCREEN_W/2, SCREEN_H / 2, blink(4) ? CLR_GREEN : CLR_WHITE);
}

// ---- TITLE --------------------------------------------------------
static void draw_title(void) {
    draw_backdrop(40);
    // a little decorative ramp + bike mid-jump for the thumbnail
    camera(0, 0);
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_BROWN);
    rectfill(0, GROUND_Y, SCREEN_W, 3, CLR_DARK_PEACH);
    trifill(60, GROUND_Y, 92, GROUND_Y, 92, GROUND_Y - RAMP_H, CLR_LIGHT_GREY);
    line(60, GROUND_Y, 92, GROUND_Y - RAMP_H, CLR_WHITE);
    int by = 96 + (int)(sin_deg(now() * 120) * 4);
    spawn_dust(150, by + 14, rnd_float_between(-30, 0), rnd_float_between(-30, 10), 0.4f, 2, CLR_BROWN);
    draw_dust();
    ovalfill(158, GROUND_Y, 8, 2, CLR_BROWNISH_BLACK);
    spr_rot(SLOT_BIKE, 150, by, 14);

    print_scaled("EXCITEBIKE", (SCREEN_W - text_width("EXCITEBIKE") * 3) / 2, 22, CLR_ORANGE, 3);
    print_centered("land level - mind the heat - build a track", SCREEN_W/2, 50, CLR_LIGHT_YELLOW);

    const char *opts[2] = { "RACE", "BUILD" };
    for (int i = 0; i < 2; i++) {
        int y = 130 + i * 16;
        int on = (menuSel == i);
        print_centered(opts[i], SCREEN_W/2, y, on ? CLR_YELLOW : CLR_LIGHT_GREY);
        if (on) {
            int w = text_width(opts[i]);
            print(blink(15) ? ">" : " ", SCREEN_W / 2 - w / 2 - 12, y, CLR_YELLOW);
        }
    }
    if (sv.bestMs > 0) print_centered(str("best lap  %s", fmt_time(sv.bestMs)), SCREEN_W/2, 170, CLR_GREEN);
    print_centered("Up/Down choose   Z select", SCREEN_W/2, SCREEN_H - 12, CLR_DARK_GREY);
}

void draw(void) {
    if (mode == MODE_TITLE)      draw_title();
    else if (mode == MODE_RACE)  draw_race();
    else                         draw_build();
}
