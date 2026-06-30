/* de:meta
{
  "title": "Summer Games",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Epyx Summer Games (1984) demake; introduces the event-shell idiom (shared MENU→READY→RUN→RESULT scaffold reused by calgames and excitebike siblings) and a button-alternation mash mechanic with a clutch-press timing window.",
  "genre": "sports",
  "homage": "Summer Games (1984)",
  "description": "Button-mash track and field in the Track & Field arcade lineage: pick an event off a sunlit stadium menu, then hammer two keys to build raw speed and nail one clutch press. TWO events on a shared event-shell — 100M SPRINT (mash Z/X to sprint, then dip at the tape with a timed LEAN) and LONG JUMP (mash the runway, then stab JUMP at the best takeoff angle on a sweeping needle — overrun the foul board and it's a SCRATCH). A scoreboard panel tracks your personal best per event, saved between runs, with crowd-roar swells, footfall ticks pitched to your speed, dust off the feet, screen shake and a fanfare on a new record. Menu: Up/Down choose, Z start. In an event: alternate Z and X to run, press UP to lean/jump, X or ESC backs out.",
  "todo": [
    "Input bug: alternating Z/X is needed, but X takes you back to the start, so you can't get past it."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// SUMMER GAMES — button-mash track & field, two events on one event-shell.
//
//   100M SPRINT : alternate Z/X to build speed; a runner legs it across the
//                 track. Near the tape a LEAN window opens — tap UP inside it
//                 to dip at the line and shave your time.
//   LONG JUMP   : mash Z/X down the runway, then tap UP to leap. A needle
//                 sweeps your takeoff angle (~10..60deg) — distance = speed x
//                 angle-quality. Cross the foul line before jumping = SCRATCH.
//
// Event-shell idiom (excitebike's spine): MENU lists events -> an event runs
// through READY/RUN/RESULT -> the result posts to a shared scoreboard with a
// saved personal best. Mash = btnp alternation (same key twice barely pays).
// Clutch press = btnp(UP) against a live window. Juice + synth where it earns.

// ---- tuning ------------------------------------------------------
#define GROUND_Y     150
#define RUN_W        16
#define RUN_H        16
#define SPRINT_DIST  920.0f          // world px to the tape
#define RUNWAY_DIST  640.0f          // world px to the foul line
#define LEAN_WINDOW  90.0f           // px before the tape the lean opens
#define DECAY        120.0f          // speed bleed px/s^2 when not mashing
#define KICK         42.0f           // px/s added per good alternating press
#define MAXSPD       330.0f
#define GRAV         900.0f

#define ABSF(v) ((v) < 0 ? -(v) : (v))
#define FMAX(a,b) ((a) > (b) ? (a) : (b))

// ---- modes / events / phases -------------------------------------
enum { MODE_MENU, MODE_EVENT };
enum { EV_SPRINT, EV_JUMP, EV_COUNT };
enum { PH_READY, PH_RUN, PH_RESULT };

// long-jump sub-states inside PH_RUN
enum { JS_RUN, JS_AIR, JS_DONE };

// ---- saved bests -------------------------------------------------
#define SAVE_MAGIC 0x53554d31        // 'SUM1'
typedef struct {
    int magic;
    int bestSprintMs;                // lower better; 0 = none
    int bestJumpCm;                  // higher better; 0 = none
} Save;
static Save sv;

// ---- particles ---------------------------------------------------
#define MAX_DUST 90
typedef struct { float x, y, vx, vy, life, maxlife; int color; } Dust;
static Dust dust[MAX_DUST];

static void spawn_dust(float x, float y, float vx, float vy, float life, int color) {
    for (int i = 0; i < MAX_DUST; i++) if (dust[i].life <= 0) {
        dust[i] = (Dust){ x, y, vx, vy, life, life, color };
        return;
    }
}

// ---- globals -----------------------------------------------------
static int   mode = MODE_MENU;
static int   menuSel = EV_SPRINT;
static int   curEvent;
static int   phase;
static float countT;                 // READY countdown

// shared run state
static float rx;                     // runner world x (sprite left)
static float ry;                     // runner top y
static float vy;                     // vertical velocity (jump)
static float speed;                  // px/s
static float runClock;               // seconds since GO
static int   lastKey;                // -1 none, 0 = Z(A), 1 = X(B): for alternation
static float crowd;                  // 0..1 crowd-roar swell

// sprint
static bool  leanOpen;
static bool  leaned;
static float leanBonusMs;            // negative time shaved
static int   resultSprintMs;

// jump
static int   jstate;
static float angle;                  // current takeoff-needle angle (deg)
static float angleDir;               // needle sweep dir
static float lockAngle;              // chosen angle at takeoff
static float jx0;                    // takeoff world x
static bool  scratch;
static int   resultJumpCm;
static int   resultJumpPts;

static float resultT;                // RESULT entry animation timer
static bool  newBest;
static float bestFlash;

// ================================================================
//  sound
// ================================================================
static void setup_audio(void) {
    // slot 5 — footfall thump: short filtered noise, pitch rides speed
    instrument(5, INSTR_NOISE, 1, 40, 0, 30);
    instrument_filter(5, FILTER_LOW, 700, 3);
    // slot 6 — crowd swell: airy filtered noise pad
    instrument(6, INSTR_NOISE, 120, 200, 5, 300);
    instrument_filter(6, FILTER_BAND, 900, 6);
}

static void fanfare(int big) {
    schedule(0,   67, INSTR_SQUARE, 4);
    schedule(110, 71, INSTR_SQUARE, 4);
    schedule(220, 74, INSTR_SINE,   5);
    if (big) { schedule(330, 79, INSTR_SINE, 6); schedule(440, 83, INSTR_SINE, 6); }
}

// ================================================================
//  event setup
// ================================================================
static void start_event(int ev) {
    mode = MODE_EVENT;
    curEvent = ev;
    phase = PH_READY; countT = 3.0f;
    rx = 24; ry = GROUND_Y - RUN_H; vy = 0;
    speed = 0; runClock = 0; lastKey = -1; crowd = 0;
    leanOpen = false; leaned = false; leanBonusMs = 0; resultSprintMs = 0;
    jstate = JS_RUN; angle = 30; angleDir = 1; lockAngle = 0; jx0 = 0;
    scratch = false; resultJumpCm = 0; resultJumpPts = 0;
    resultT = 0; newBest = false; bestFlash = 0;
    for (int i = 0; i < MAX_DUST; i++) dust[i].life = 0;
}

// the mash: a true Z/X alternation pays a kick; same key twice barely moves.
static void do_mash(void) {
    int pressed = -1;
    if (btnp(0, BTN_A)) pressed = 0;
    if (btnp(0, BTN_B)) pressed = 1;
    if (pressed < 0) return;
    if (lastKey < 0 || pressed != lastKey) {
        speed = FMAX(speed, 0) + KICK;          // good alternation
        speed = clamp(speed, 0, MAXSPD);
        // footfall — pitch rides speed
        int cut = 500 + (int)(speed * 1.6f);
        instrument_filter(5, FILTER_LOW, cut, 3);
        hit(40 + (int)(speed * 0.06f), 5, 3, 28);
        spawn_dust(rx + 2, GROUND_Y, rnd_float_between(-50, -10),
                   rnd_float_between(-40, -5), 0.3f, CLR_LIGHT_GREY);
    } else {
        speed += KICK * 0.18f;                   // same key twice — token gain
    }
    lastKey = pressed;
}

// ================================================================
//  update — SPRINT
// ================================================================
static void post_sprint(void) {
    int ms = (int)(runClock * 1000 + leanBonusMs);
    if (ms < 0) ms = 0;
    resultSprintMs = ms;
    newBest = (sv.bestSprintMs == 0 || ms < sv.bestSprintMs);
    if (newBest) { sv.bestSprintMs = ms; sv.magic = SAVE_MAGIC; save_bytes(&sv, sizeof sv); }
    phase = PH_RESULT; resultT = 0;
    shake(3);
    fanfare(newBest);
}

static void update_sprint(void) {
    float d = dt();
    runClock += d;
    do_mash();
    // friction when idle
    speed = FMAX(0.0f, speed - DECAY * d);
    rx += speed * d;

    crowd = clamp(rx / SPRINT_DIST, 0, 1);

    // lean window opens near the tape
    if (!leanOpen && rx >= SPRINT_DIST - LEAN_WINDOW && rx < SPRINT_DIST)
        leanOpen = true;
    if (leanOpen && !leaned && btnp(0, BTN_UP)) {
        leaned = true;
        // closer to the line when you lean = bigger shave
        float closeness = clamp(remap(rx, SPRINT_DIST - LEAN_WINDOW, SPRINT_DIST, 0, 1), 0, 1);
        leanBonusMs = -lerp(40, 180, closeness);
        shake(2); note(72, INSTR_SINE, 4);
        speed += 30;
    }

    if (rx >= SPRINT_DIST) { rx = SPRINT_DIST; post_sprint(); }
}

// ================================================================
//  update — LONG JUMP
// ================================================================
static int angle_quality_pct(float a) {
    // best around 42deg; falls off either side
    float q = 1.0f - ABSF(a - 42.0f) / 38.0f;
    return (int)(clamp(q, 0, 1) * 100);
}

static void post_jump(void) {
    if (scratch) { resultJumpCm = 0; resultJumpPts = 0; }
    else {
        // distance: speed feeds it, angle quality scales it
        float q = angle_quality_pct(lockAngle) / 100.0f;
        float base = remap(speed, 0, MAXSPD, 0, 9.2f);   // up to ~9.2m raw
        float m = base * (0.45f + 0.55f * q);            // angle gates it
        resultJumpCm = (int)(m * 100);
        resultJumpPts = (int)(m * 110);
        if (sv.bestJumpCm == 0 || resultJumpCm > sv.bestJumpCm) {
            newBest = true; sv.bestJumpCm = resultJumpCm;
            sv.magic = SAVE_MAGIC; save_bytes(&sv, sizeof sv);
        }
    }
    phase = PH_RESULT; resultT = 0;
    fanfare(newBest);
}

static void update_jump(void) {
    float d = dt();
    runClock += d;

    if (jstate == JS_RUN) {
        do_mash();
        speed = FMAX(0.0f, speed - DECAY * d);
        rx += speed * d;
        crowd = clamp(rx / RUNWAY_DIST, 0, 1);

        // the takeoff-angle needle sweeps once you're up to pace
        angle += angleDir * 70.0f * d;
        if (angle > 60) { angle = 60; angleDir = -1; }
        if (angle < 10) { angle = 10; angleDir = 1; }

        // overstep the foul line before jumping = scratch
        if (rx >= RUNWAY_DIST) {
            scratch = true; jstate = JS_DONE;
            shake(4); hit(34, INSTR_NOISE, 6, 220);
            post_jump();
            return;
        }
        // takeoff
        if (btnp(0, BTN_UP) && rx > 120) {
            lockAngle = angle;
            jx0 = rx;
            jstate = JS_AIR;
            vy = -sin_deg(lockAngle) * (speed * 0.9f);
            shake(3); note(76, INSTR_SINE, 5);
            for (int k = 0; k < 8; k++)
                spawn_dust(rx + 4, GROUND_Y, rnd_float_between(-60, 0),
                           rnd_float_between(-90, -20), 0.5f, CLR_LIGHT_PEACH);
        }
    } else if (jstate == JS_AIR) {
        vy += GRAV * d;
        ry += vy * d;
        rx += speed * cos_deg(lockAngle) * 0.9f * d;
        if (ry >= GROUND_Y - RUN_H) {
            ry = GROUND_Y - RUN_H;
            jstate = JS_DONE;
            shake(2);
            for (int k = 0; k < 14; k++)
                spawn_dust(rx + 6, GROUND_Y, rnd_float_between(-80, 80),
                           rnd_float_between(-70, -10), 0.6f,
                           k % 2 ? CLR_LIGHT_PEACH : CLR_DARK_PEACH);
            post_jump();
        }
    }
}

// ================================================================
//  update
// ================================================================
void init(void) {
    int n = load_bytes(&sv, sizeof sv);
    if (n != (int)sizeof sv || sv.magic != SAVE_MAGIC) {
        sv.magic = SAVE_MAGIC; sv.bestSprintMs = 0; sv.bestJumpCm = 0;
    }
    colorkey(0);
    setup_audio();
    bpm(120);
}

void update(void) {
    if (mode == MODE_MENU) {
        if (btnp(0, BTN_UP) || btnp(0, BTN_DOWN)) {
            menuSel = (menuSel + (btnp(0, BTN_UP) ? EV_COUNT - 1 : 1)) % EV_COUNT;
            note(72, INSTR_SQUARE, 3);
        }
        if (btnp(0, BTN_A) || keyp(KEY_ENTER)) { start_event(menuSel); note(60, INSTR_NOISE, 5); }
    } else {
        if (phase == PH_READY) {
            countT -= dt();
            if (countT <= 0) {
                phase = PH_RUN; runClock = 0;
                hit(50, INSTR_NOISE, 6, 120);   // starter's pistol
            }
        } else if (phase == PH_RUN) {
            if (curEvent == EV_SPRINT) update_sprint();
            else                       update_jump();
            // crowd roar swells as you near the goal
            if (crowd > 0.4f && frame() % 10 == 0) {
                instrument_filter(6, FILTER_BAND, 700 + (int)(crowd * 1400), 6);
                hit(48 + (int)(crowd * 12), 6, 1 + (int)(crowd * 3), 260);
            }
        } else { // PH_RESULT
            resultT += dt();
            if (bestFlash > 0) bestFlash -= dt();
            if (resultT > 0.4f && (btnp(0, BTN_A) || btnp(0, BTN_B) || keyp(KEY_ENTER)))
                mode = MODE_MENU;
        }
        if (keyp(KEY_ESCAPE) || (phase != PH_RESULT && btnp(0, BTN_B))) mode = MODE_MENU;
    }

    // age dust
    for (int i = 0; i < MAX_DUST; i++) if (dust[i].life > 0) {
        dust[i].vy += 240 * dt();
        dust[i].x  += dust[i].vx * dt();
        dust[i].y  += dust[i].vy * dt();
        dust[i].life -= dt();
    }
}

// ================================================================
//  drawing helpers
// ================================================================
static void draw_runner(int sx, int sy, bool airborne, bool leaning) {
    // legs animate with speed; a tiny stick athlete in primitives (no sprite needed)
    int torso = CLR_RED, skin = CLR_LIGHT_PEACH, leg = CLR_DARK_BLUE;
    int hx = sx + RUN_W / 2;
    // shadow
    ovalfill(hx, GROUND_Y, 7, 2, CLR_BROWNISH_BLACK);

    float lean = leaning ? 4 : (airborne ? 2 : 0);
    int head_y = sy - 1;
    // head
    circfill(hx + (int)lean, head_y, 3, skin);
    // torso
    line(hx + (int)lean, head_y + 3, hx, sy + 9, torso);
    rectfill(hx - 2, head_y + 3, 4, 7, torso);

    // running legs — alternate by stride phase tied to speed
    float ph = airborne ? 0 : sin_deg(now() * (200 + speed)) ;
    int foot1 = hx + (int)(ph * 5);
    int foot2 = hx - (int)(ph * 5);
    int legtop = sy + 9;
    if (airborne) {
        line(hx, legtop, hx - 5, sy + 14, leg);   // tucked
        line(hx, legtop, hx + 6, sy + 12, leg);
        // arms forward
        line(hx, sy + 4, hx + 6, sy + 2, skin);
    } else {
        line(hx, legtop, foot1, GROUND_Y, leg);
        line(hx, legtop, foot2, GROUND_Y, leg);
        // pumping arms
        line(hx, sy + 4, hx + (int)(ph * 4), sy + 1, skin);
        line(hx, sy + 4, hx - (int)(ph * 4), sy + 7, skin);
    }
}

static void draw_dust(void) {
    for (int i = 0; i < MAX_DUST; i++) {
        if (dust[i].life <= 0) continue;
        float t = dust[i].life / dust[i].maxlife;
        int r = (int)(2 * t + 0.5f);
        if (r <= 0) pset((int)dust[i].x, (int)dust[i].y, dust[i].color);
        else        circfill((int)dust[i].x, (int)dust[i].y, r, dust[i].color);
    }
}

static void draw_sky(void) {
    cls(CLR_BLUE);
    // sun
    circfill(SCREEN_W - 34, 28, 12, CLR_YELLOW);
    circfill(SCREEN_W - 34, 28, 9, CLR_LIGHT_YELLOW);
    // bleachers band behind the track
    rectfill(0, GROUND_Y - 44, SCREEN_W, 30, CLR_DARK_GREY);
    // dotted crowd
    for (int k = 0; k < 90; k++) {
        int px = (k * 23) % SCREEN_W;
        int py = GROUND_Y - 42 + (k * 7) % 26;
        int cc[5] = { CLR_RED, CLR_YELLOW, CLR_WHITE, CLR_GREEN, CLR_PINK };
        // animate a faint crowd shimmer
        if ((k + frame() / 6) % 11 != 0)
            pset(px, py, cc[(k) % 5]);
    }
    rectfill(0, GROUND_Y - 14, SCREEN_W, 3, CLR_LIGHT_GREY);
}

static const char *fmt_time(int ms) {
    int cs = (ms / 10) % 100, s = (ms / 1000);
    return str("%d.%02ds", s, cs);
}

// ================================================================
//  draw — SPRINT
// ================================================================
static void draw_sprint(int camX) {
    // track: lane stripes scrolling under camera
    rectfill(0, GROUND_Y, (int)SPRINT_DIST + 60, SCREEN_H - GROUND_Y, CLR_DARK_ORANGE);
    rectfill(0, GROUND_Y, (int)SPRINT_DIST + 60, 3, CLR_PEACH);
    for (int x = 0; x < (int)SPRINT_DIST + 60; x += 28)
        rectfill(x, GROUND_Y + 8, 14, 2, CLR_BROWN);
    // start + finish posts
    rectfill(0, GROUND_Y - 30, 2, 30, CLR_WHITE);
    int fx = (int)SPRINT_DIST;
    for (int r = 0; r < 8; r++)
        rectfill(fx, GROUND_Y - 40 + r * 5, 5, 5, (r % 2) ? CLR_WHITE : CLR_BLACK);
    rectfill(fx, GROUND_Y - 40, 1, 40, CLR_LIGHT_GREY);
    // the tape across the line
    line(fx, GROUND_Y - 18, fx, GROUND_Y - 18, CLR_WHITE);
    rectfill(fx - 1, GROUND_Y - 18, 3, 1, CLR_YELLOW);

    draw_dust();
    draw_runner((int)rx, (int)ry, false, leaned);
}

// ================================================================
//  draw — LONG JUMP
// ================================================================
static void draw_jump(int camX) {
    // runway
    rectfill(0, GROUND_Y, (int)RUNWAY_DIST, SCREEN_H - GROUND_Y, CLR_DARK_ORANGE);
    rectfill(0, GROUND_Y, (int)RUNWAY_DIST, 3, CLR_PEACH);
    for (int x = 0; x < (int)RUNWAY_DIST; x += 28)
        rectfill(x, GROUND_Y + 8, 14, 2, CLR_BROWN);
    // foul line (board)
    int fl = (int)RUNWAY_DIST;
    rectfill(fl - 4, GROUND_Y, 6, 5, CLR_WHITE);
    rectfill(fl - 4, GROUND_Y, 6, 2, CLR_RED);
    // sand pit beyond
    fillp(FILL_CHECKER, CLR_BROWN);
    rectfill(fl + 2, GROUND_Y, 600, SCREEN_H - GROUND_Y, CLR_DARK_PEACH);
    fillp_reset();
    rectfill(fl + 2, GROUND_Y, 600, 2, CLR_LIGHT_PEACH);
    // distance markers in the pit (every ~1m worth of px)
    for (int m = 1; m <= 10; m++) {
        int mx = fl + 2 + m * 38;
        line(mx, GROUND_Y, mx, GROUND_Y + 6, CLR_MEDIUM_GREY);
    }

    draw_dust();
    draw_runner((int)rx, (int)ry, jstate == JS_AIR, false);

    // landing flag once done
    if (jstate == JS_DONE && !scratch) {
        int lx = (int)rx + 6;
        line(lx, GROUND_Y, lx, GROUND_Y - 16, CLR_WHITE);
        trifill(lx, GROUND_Y - 16, lx, GROUND_Y - 9, lx + 9, GROUND_Y - 12, CLR_RED);
    }
}

// ================================================================
//  draw — HUD / overlays
// ================================================================
static void draw_speed_bar(void) {
    int bw = 90, bx = (SCREEN_W - bw) / 2;
    print("SPEED", bx, 4, CLR_WHITE);
    bar(bx + 40, 5, bw - 40, 6, clamp(speed / MAXSPD, 0, 1),
        speed > MAXSPD * 0.8f ? CLR_GREEN : CLR_YELLOW, CLR_DARK_GREY);
}

static void draw_event(void) {
    int camX = 0;
    if (curEvent == EV_SPRINT)
        camX = (int)clamp(rx - 90, 0, SPRINT_DIST - SCREEN_W + 80);
    else
        camX = (int)clamp(rx - 90, 0, RUNWAY_DIST + 200 - SCREEN_W + 80);

    draw_sky();
    camera(camX, 0);
    if (curEvent == EV_SPRINT) draw_sprint(camX);
    else                       draw_jump(camX);
    camera(0, 0);

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 13, CLR_BLACK);
    print(curEvent == EV_SPRINT ? "100M SPRINT" : "LONG JUMP", 4, 3, CLR_LIGHT_YELLOW);
    if (phase == PH_RUN) {
        print_right(fmt_time((int)(runClock * 1000)), SCREEN_W - 4, 3, CLR_WHITE);
        draw_speed_bar();
    }

    // angle needle (long jump, running)
    if (curEvent == EV_JUMP && phase == PH_RUN && jstate == JS_RUN) {
        int gx = SCREEN_W / 2, gy = SCREEN_H - 22;
        arc(gx, gy, 26, -180, 0, CLR_DARK_GREY);
        // sweet-zone wedge ~32..52deg (drawn as up-angles: 0=right going CCW up)
        int q = angle_quality_pct(angle);
        int ncol = q > 75 ? CLR_GREEN : q > 40 ? CLR_YELLOW : CLR_RED;
        float na = -angle;   // up is negative y
        line(gx, gy, gx + (int)(cos_deg(na) * 24), gy + (int)(sin_deg(na) * 24), ncol);
        print_centered(str("ANGLE %d", (int)angle), SCREEN_W/2, SCREEN_H - 6, ncol);
    }

    // prompts
    if (phase == PH_RUN) {
        if (curEvent == EV_SPRINT) {
            if (leanOpen && !leaned)
                print_centered("LEAN!  press UP", SCREEN_W/2, 22, blink(3) ? CLR_YELLOW : CLR_WHITE);
            else
                print_centered("mash Z / X", SCREEN_W/2, 22, CLR_LIGHT_GREY);
        } else if (jstate == JS_RUN) {
            print_centered("mash Z / X   -   UP to JUMP", SCREEN_W/2, 22, CLR_LIGHT_GREY);
        }
    }

    // READY countdown
    if (phase == PH_READY) {
        int n = (int)countT + 1;
        const char *t = n > 0 ? str("%d", n) : "GO!";
        print_scaled(t, (SCREEN_W - text_width(t) * 4) / 2, SCREEN_H / 2 - 18, CLR_YELLOW, 4);
        print_centered("alternate Z and X to run", SCREEN_W/2, SCREEN_H - 24, CLR_LIGHT_GREY);
    }

    // RESULT panel
    if (phase == PH_RESULT) {
        float in = ease_out(clamp(resultT / 0.35f, 0, 1));
        int ph_h = 64;
        int py = (int)lerp(-ph_h, SCREEN_H / 2 - 30, in);
        rectfill(SCREEN_W / 2 - 78, py, 156, ph_h, CLR_BLACK);
        rect    (SCREEN_W / 2 - 78, py, 156, ph_h, newBest ? CLR_YELLOW : CLR_LIGHT_GREY);
        if (curEvent == EV_SPRINT) {
            print_centered("FINISH", SCREEN_W/2, py + 6, CLR_GREEN);
            print_centered(str("TIME  %s", fmt_time(resultSprintMs)), SCREEN_W/2, py + 20, CLR_WHITE);
            if (leaned) print_centered("nice lean!", SCREEN_W/2, py + 32, CLR_GREEN);
            print_centered(str("BEST  %s", sv.bestSprintMs ? fmt_time(sv.bestSprintMs) : "--"), SCREEN_W/2, py + 44, CLR_LIGHT_YELLOW);
        } else {
            if (scratch) {
                print_centered("SCRATCH!", SCREEN_W/2, py + 14, blink(4) ? CLR_RED : CLR_ORANGE);
                print_centered("you overran the board", SCREEN_W/2, py + 30, CLR_LIGHT_GREY);
            } else {
                print_centered("LANDED", SCREEN_W/2, py + 6, CLR_GREEN);
                print_centered(str("%d.%02dm   +%d", resultJumpCm / 100, resultJumpCm % 100, resultJumpPts), SCREEN_W/2, py + 20, CLR_WHITE);
                print_centered(str("BEST  %d.%02dm", sv.bestJumpCm / 100, sv.bestJumpCm % 100), SCREEN_W/2, py + 44, CLR_LIGHT_YELLOW);
            }
        }
        if (newBest && blink(5))
            print_centered("NEW PERSONAL BEST!", SCREEN_W/2, py - 10, CLR_YELLOW);
        print_centered("Z = continue", SCREEN_W/2, py + ph_h + 6, CLR_DARK_GREY);
    }
}

// ================================================================
//  draw — MENU
// ================================================================
static void draw_menu(void) {
    draw_sky();
    // a hero athlete jogging in place on the track for the thumbnail
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_ORANGE);
    rectfill(0, GROUND_Y, SCREEN_W, 3, CLR_PEACH);
    for (int x = 0; x < SCREEN_W; x += 28)
        rectfill(x, GROUND_Y + 8, 14, 2, CLR_BROWN);
    speed = 200; draw_runner(44, GROUND_Y - RUN_H, false, false); speed = 0;
    if (frame() % 5 == 0) spawn_dust(48, GROUND_Y, rnd_float_between(-30, 0),
                                     rnd_float_between(-30, -5), 0.3f, CLR_LIGHT_GREY);
    draw_dust();

    print_scaled("SUMMER GAMES", (SCREEN_W - text_width("SUMMER GAMES") * 2) / 2, 14, CLR_LIGHT_YELLOW, 2);
    print_centered("mash to run - one clutch press to win", SCREEN_W/2, 34, CLR_WHITE);

    const char *names[EV_COUNT] = { "100M SPRINT", "LONG JUMP" };
    for (int i = 0; i < EV_COUNT; i++) {
        int y = 60 + i * 16;
        int on = (menuSel == i);
        print_centered(names[i], SCREEN_W/2, y, on ? CLR_YELLOW : CLR_LIGHT_GREY);
        if (on) {
            int w = text_width(names[i]);
            print(blink(15) ? ">" : " ", SCREEN_W / 2 - w / 2 - 14, y, CLR_YELLOW);
        }
    }

    // scoreboard panel
    int py = 96;
    rectfill(SCREEN_W / 2 - 70, py, 140, 36, CLR_BLACK);
    rect    (SCREEN_W / 2 - 70, py, 140, 36, CLR_DARK_GREY);
    print_centered("- PERSONAL BESTS -", SCREEN_W/2, py + 4, CLR_GREEN);
    print(str("100M  %s", sv.bestSprintMs ? fmt_time(sv.bestSprintMs) : "--"),
          SCREEN_W / 2 - 62, py + 15, CLR_WHITE);
    print(str("JUMP  %d.%02dm", sv.bestJumpCm / 100, sv.bestJumpCm % 100),
          SCREEN_W / 2 - 62, py + 25, CLR_WHITE);

    print_centered("Up/Down choose   Z start", SCREEN_W/2, SCREEN_H - 12, CLR_DARK_GREY);
}

void draw(void) {
    if (mode == MODE_MENU) draw_menu();
    else                   draw_event();
}
