/* de:meta
{
  "slug": "wintergames",
  "title": "Winter Games",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine"
  ],
  "lineage": "Epyx Winter Games homage; the primary teaching artifact is the reusable event-shell pattern (table of fn-pointer structs + shared chrome + fade transitions) explicitly designed to be lifted into summergames/calgames.",
  "genre": "sports",
  "homage": "Winter Games (1985)",
  "description": "An Epyx-style winter meet built on a clean, reusable event shell — pick your nation, then work down a scoreboard of skill events one at a time, with bests saved between runs. SKI JUMP is two-phase: ride the in-run and stab Z to fire the takeoff on a sweeping power meter, then balance your lean in the air (Up=back, Down=forward) — keep the form gauge centered for distance, over-rotate and you tumble down the slope. SPEED SKATING is an alternating-key rhythm: tap Z, X, Z, X in a steady cadence to push each skate while holding your lane around the ice oval — mash one key and you stumble, hit the rail and you bleed speed. Snow drifts on a noise wind, a synth whoosh rides the launch, skate strides click on the beat, and a medal ceremony crowns your total. Controls: Title — Left/Right pick nation, Z start; Scoreboard — Up/Down choose, Z enter, X title; Ski Jump — Z run then Z takeoff, Up/Down lean; Speed Skating — alternate Z/X, Up/Down hold lane; Z returns to the scoreboard after each event."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// WINTER GAMES — an Epyx-style multi-event meet built on a REUSABLE EVENT SHELL.
//
//   TITLE        : Left/Right pick your nation, Z start.
//   SCOREBOARD   : Up/Down choose an event, Z enter, X back to title.
//   SKI JUMP     : Z starts the run; Z again to fire the takeoff near the sweet
//                  spot; in the air Up = lean back, Down = lean forward — keep the
//                  wobble centered. Land clean for distance, over-rotate and tumble.
//   SPEED SKATING: alternate Z / X in a steady cadence to push each skate; Up/Down
//                  hold your lane. Don't mash one key. Beat the clock over the lap.
//
// The shell (Event table + state machine + shared chrome) is the real deliverable —
// it's meant to be lifted whole into summergames/calgames. An event is just a struct
// of three fn pointers; adding one is one row in the table.

// ================================================================
//  shell types & state
// ================================================================
enum { S_TITLE, S_BOARD, S_EVENT, S_CEREMONY };
static int screen = S_TITLE;
static float fadeT;          // transition fade, counts down
static int   pendingScreen = -1;

// nations: name + primary + accent palette colors
typedef struct { const char *name; int c0, c1; } Nation;
static const Nation nations[] = {
    { "NORGE",  CLR_RED,        CLR_WHITE },
    { "SUOMI",  CLR_BLUE,       CLR_WHITE },
    { "CANADA", CLR_RED,        CLR_LIGHT_GREY },
    { "NIPPON", CLR_RED,        CLR_LIGHT_PEACH },
    { "SVERIGE",CLR_TRUE_BLUE,  CLR_YELLOW },
    { "USA",    CLR_TRUE_BLUE,  CLR_RED },
};
#define NNATIONS ((int)(sizeof nations / sizeof nations[0]))
static int nation = 0;

// per-event records
#define NEVENTS 2
static int bestScore[NEVENTS];
static int lastScore[NEVENTS];
static int played[NEVENTS];

typedef struct {
    const char *name;
    const char *unit;          // score unit suffix
    void (*reset)(void);       // called when entering the event
    void (*update)(void);      // per-frame logic; set ev_done + ev_score when finished
    void (*draw)(void);        // draws only the event field (shell draws the chrome)
} Event;

static int boardSel = 0;       // 0..NEVENTS-1 = events, NEVENTS = "CEREMONY"
static int curEvent = 0;
static int ev_done;            // event sets this true when its result is ready
static int ev_score;           // and writes the score here
static float resultT;          // result-banner timer inside an event
static int newBest;            // flash flag

// ================================================================
//  audio
// ================================================================
static void setup_audio(void) {
    // slot 5 — jump whoosh: filtered saw with a pitch wobble
    instrument(5, INSTR_SAW, 4, 200, 4, 120);
    instrument_filter(5, FILTER_LOW, 900, 5);
    instrument_lfo(5, 0, LFO_PITCH, 5.0f, 0.3f);
    // slot 6 — skate stride: short bright pluck
    instrument(6, INSTR_TRI, 2, 40, 2, 30);
    // slot 7 — wind pad
    instrument(7, INSTR_NOISE, 300, 200, 3, 400);
    instrument_filter(7, FILTER_BAND, 500, 8);
}

static void fanfare(void) {
    schedule(0,   67, INSTR_SINE, 5);
    schedule(120, 71, INSTR_SINE, 5);
    schedule(240, 74, INSTR_SINE, 5);
    schedule(360, 79, INSTR_SINE, 6);
}

static void blip(int midi) { note(midi, INSTR_SQUARE, 3); }

// ================================================================
//  snow particles (ambient, shared across screens)
// ================================================================
#define NSNOW 70
typedef struct { float x, y, vx, spd; int sz; } Flake;
static Flake snow[NSNOW];
static float wind;

static void init_snow(void) {
    for (int i = 0; i < NSNOW; i++) {
        snow[i].x = (float)rnd(SCREEN_W);
        snow[i].y = (float)rnd(SCREEN_H);
        snow[i].spd = rnd_float_between(12, 36);
        snow[i].sz = rnd(3) == 0 ? 2 : 1;
    }
}
static void update_snow(void) {
    wind = noise(now() * 0.2f) * 40 - 20;
    for (int i = 0; i < NSNOW; i++) {
        snow[i].y += snow[i].spd * dt();
        snow[i].x += (wind + sin_deg(now() * 60 + i * 30) * 8) * dt();
        if (snow[i].y > SCREEN_H) { snow[i].y = -2; snow[i].x = (float)rnd(SCREEN_W); }
        if (snow[i].x < -2) snow[i].x += SCREEN_W;
        if (snow[i].x > SCREEN_W) snow[i].x -= SCREEN_W;
    }
}
static void draw_snow(void) {
    for (int i = 0; i < NSNOW; i++) {
        int c = snow[i].sz == 2 ? CLR_WHITE : CLR_LIGHT_GREY;
        if (snow[i].sz == 2) circfill((int)snow[i].x, (int)snow[i].y, 1, c);
        else pset((int)snow[i].x, (int)snow[i].y, c);
    }
}

// little helper: draw the chosen flag glyph
static void draw_flag(int x, int y, int w, int h) {
    rectfill(x, y, w, h, nations[nation].c0);
    rectfill(x + w / 3, y, w / 4, h, nations[nation].c1);
    rect(x, y, w, h, CLR_BLACK);
}

// ================================================================
//  EVENT 1 — SKI JUMP
//   phase 0: in-run (Z to fire takeoff on the sweeping meter)
//   phase 1: flight (lean balance) → distance
//   phase 2: landed/crashed result
// ================================================================
enum { SJ_RUN, SJ_AIR, SJ_RESULT };
static int   sj_phase;
static float sj_meter;        // 0..1 sweeping power
static int   sj_dir;          // sweep direction
static float sj_launch;       // captured launch power 0..1
static float sj_jx, sj_jy;    // jumper screen pos
static float sj_vx, sj_vy;
static float sj_lean;         // -1 (back) .. +1 (forward), drifts forward
static float sj_wob;          // balance error accumulator → flight quality
static float sj_quality;      // 1 = perfect form, 0 = ragged
static int   sj_crash;
static float sj_dist;
static float sj_tumble;
static float sj_squash;

#define SJ_GROUND 150
#define SJ_LIP_X  92
#define SJ_LIP_Y  90
#define SJ_SWEET  0.80f        // sweet spot on the meter

static void sj_reset(void) {
    sj_phase = SJ_RUN; sj_meter = 0; sj_dir = 1;
    sj_launch = 0; sj_lean = 0; sj_wob = 0; sj_quality = 1;
    sj_crash = 0; sj_dist = 0; sj_tumble = 0; sj_squash = 0;
    sj_jx = 30; sj_jy = 40;
    ev_done = 0; resultT = 0; newBest = 0;
}

static void sj_update(void) {
    float d = dt();

    if (sj_phase == SJ_RUN) {
        // jumper slides down the in-run toward the lip
        sj_jx = lerp(sj_jx, SJ_LIP_X, clamp(3 * d, 0, 1));
        sj_jy = lerp(sj_jy, SJ_LIP_Y, clamp(3 * d, 0, 1));
        // power meter sweeps up and down
        sj_meter += sj_dir * 1.4f * d;
        if (sj_meter > 1) { sj_meter = 1; sj_dir = -1; }
        if (sj_meter < 0) { sj_meter = 0; sj_dir = 1; }
        if (frame() % 6 == 0) hit(48, 6, 1, 30);   // ski hiss

        if (btnp(0, BTN_A)) {
            sj_launch = sj_meter;
            sj_phase = SJ_AIR;
            // launch quality: closeness to the sweet spot scales speed
            float off = sj_launch - SJ_SWEET; if (off < 0) off = -off;
            float power = clamp(1.0f - off * 1.6f, 0.15f, 1.0f);
            sj_vx = 60 + power * 70;
            sj_vy = -(40 + power * 70);
            sj_lean = 0.15f;            // slight forward bias to fight
            // whoosh
            hit(50 + (int)(power * 18), 5, 6, 600);
            shake(1.5f);
        }
        return;
    }

    if (sj_phase == SJ_AIR) {
        // gravity arc
        sj_vy += 130 * d;
        sj_jx += sj_vx * d;
        sj_jy += sj_vy * d;

        // lean drifts forward; player fights it with Up (back) / Down (forward)
        sj_lean += 0.55f * d;                       // passive nose-over drift
        if (btn(0, BTN_UP))   sj_lean -= 1.7f * d;
        if (btn(0, BTN_DOWN)) sj_lean += 1.7f * d;
        sj_lean = clamp(sj_lean, -1.4f, 1.4f);

        // wobble error accumulates when off the ideal slight-forward tuck
        float err = sj_lean - 0.2f; if (err < 0) err = -err;
        sj_wob += err * d;
        sj_quality = clamp(1.0f - sj_wob * 0.55f, 0, 1);

        if (frame() % 10 == 0 && sj_quality > 0.4f) hit(60, 7, 1, 120);

        // landing when we reach the slope (slope descends to the right)
        float slopeY = SJ_LIP_Y + (sj_jx - SJ_LIP_X) * 0.55f;
        if (slopeY > SJ_GROUND) slopeY = SJ_GROUND;
        if (sj_jy >= slopeY) {
            sj_jy = slopeY;
            // crash if leaned too hard at landing OR ragged flight
            float over = sj_lean; if (over < 0) over = -over;
            sj_crash = (over > 1.1f) || (sj_quality < 0.25f);
            sj_dist = (sj_jx - SJ_LIP_X) * 0.9f * (0.5f + sj_quality * 0.5f);
            if (sj_dist < 0) sj_dist = 0;
            if (sj_crash) {
                shake(5); hit(36, INSTR_NOISE, 6, 350); note(40, INSTR_SAW, 4);
                sj_dist *= 0.45f;
            } else {
                sj_squash = 1.0f; shake(2); note(64, INSTR_SINE, 4);
            }
            ev_score = (int)sj_dist;
            sj_phase = SJ_RESULT; resultT = 0;
        }
        return;
    }

    // SJ_RESULT
    resultT += d;
    if (sj_crash) sj_tumble += 220 * d;
    if (sj_squash > 0) sj_squash -= d * 3;
    if (resultT > 0.6f && (btnp(0, BTN_A) || btnp(0, BTN_B)))
        ev_done = 1;
}

static void sj_draw(void) {
    // sky + mountains
    cls(CLR_DARKER_BLUE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 50, CLR_DARK_BLUE);
    fillp_reset();
    trifill(0, SJ_GROUND, 70, 30, 150, SJ_GROUND, CLR_DARK_GREY);
    trifill(120, SJ_GROUND, 220, 24, 320, SJ_GROUND, CLR_DARKER_GREY);

    // the ramp: in-run down to the lip, then the descending out-run slope
    trifill(10, 30, 30, 36, SJ_LIP_X, SJ_LIP_Y, CLR_LIGHT_GREY);
    trifill(10, 30, SJ_LIP_X, SJ_LIP_Y, 10, SJ_GROUND, CLR_WHITE);
    rectfill(0, 30, 14, SCREEN_H - 30, CLR_WHITE);
    // out-run snow slope
    trifill(SJ_LIP_X, SJ_LIP_Y, SCREEN_W, SJ_GROUND - 30, SCREEN_W, SCREEN_H, CLR_WHITE);
    trifill(SJ_LIP_X, SJ_LIP_Y, SCREEN_W, SCREEN_H, SJ_LIP_X, SCREEN_H, CLR_WHITE);
    rectfill(0, SJ_GROUND, SCREEN_W, SCREEN_H - SJ_GROUND, CLR_LIGHT_GREY);

    // distance markers on the landing slope
    for (int m = 1; m <= 5; m++) {
        int mx = SJ_LIP_X + m * 34;
        if (mx < SCREEN_W) {
            int my = SJ_LIP_Y + (int)((mx - SJ_LIP_X) * 0.55f);
            if (my > SJ_GROUND) my = SJ_GROUND;
            line(mx, my, mx, my + 6, CLR_BLUE);
            print(str("%d", m * 20), mx - 6, my + 7, CLR_TRUE_BLUE);
        }
    }

    draw_snow();

    // the jumper
    int jx = (int)sj_jx, jy = (int)sj_jy;
    if (sj_phase == SJ_RESULT && sj_crash) {
        // tumbling ragdoll
        float a = sj_tumble;
        line(jx - 6, jy - 4, jx + 6, jy - 4, nations[nation].c0);
        circfill(jx + (int)cos_deg(a) * 4, jy - 6 + (int)sin_deg(a) * 4, 3, CLR_LIGHT_PEACH);
        line(jx, jy, jx + (int)cos_deg(a + 40) * 8, jy + (int)sin_deg(a + 40) * 8, nations[nation].c1);
    } else {
        // body angle from lean; skis below
        float ang = sj_lean * 35;        // forward lean = nose down
        // skis
        int sxa = jx + (int)cos_deg(ang) * 8, sya = jy + 6 + (int)sin_deg(ang) * 8;
        int sxb = jx - (int)cos_deg(ang) * 8, syb = jy + 6 - (int)sin_deg(ang) * 8;
        line(sxa, sya, sxb, syb, CLR_DARK_GREY);
        line(sxa, sya - 2, sxb, syb - 2, CLR_DARK_GREY);
        // body (lean forward into a tuck)
        int bw = 6, bh = 9;
        if (sj_squash > 0.02f) { bw += (int)(4 * sj_squash); bh -= (int)(4 * sj_squash); }
        int leanx = (int)(sj_lean * 6);
        rectfill(jx - bw / 2 + leanx, jy - bh, bw, bh, nations[nation].c0);
        circfill(jx + leanx + (int)(sj_lean * 2), jy - bh, 3, CLR_LIGHT_PEACH);  // head
    }

    // ---- event-specific UI ----
    if (sj_phase == SJ_RUN) {
        // power meter with a marked sweet spot
        int mx = SCREEN_W / 2 - 50, my = SCREEN_H - 24;
        print_centered("TIME THE TAKEOFF - Z at the mark", SCREEN_W/2, SCREEN_H - 38, blink(12) ? CLR_YELLOW : CLR_WHITE);
        bar(mx, my, 100, 8, sj_meter, CLR_GREEN, CLR_DARKER_GREY);
        int sweetx = mx + (int)(SJ_SWEET * 100);
        rectfill(sweetx - 1, my - 2, 3, 12, CLR_RED);
        rect(mx, my, 100, 8, CLR_WHITE);
    } else if (sj_phase == SJ_AIR) {
        // lean gauge: keep the needle centered (slight forward)
        int gx = SCREEN_W / 2, gy = 22;
        print_centered("BALANCE - Up=back  Down=fwd", SCREEN_W/2, 8, CLR_WHITE);
        rectfill(gx - 50, gy, 100, 6, CLR_DARKER_GREY);
        rectfill(gx - 3, gy - 2, 6, 10, CLR_DARK_GREEN);          // ideal zone
        int needle = gx + (int)((sj_lean - 0.2f) * 60);
        rectfill(needle - 1, gy - 3, 3, 12, sj_quality > 0.5f ? CLR_GREEN : CLR_RED);
        print_right(str("FORM %d%%", (int)(sj_quality * 100)), SCREEN_W - 4, gy + 8,
                    sj_quality > 0.5f ? CLR_GREEN : CLR_ORANGE);
        print(str("%dm", (int)((sj_jx - SJ_LIP_X) * 0.9f)), 6, gy + 8, CLR_LIGHT_YELLOW);
    } else {
        // result banner
        if (resultT > 0.2f) {
            const char *t = sj_crash ? "WIPEOUT!" : (sj_dist > 80 ? "HUGE!" : "CLEAN LANDING");
            int c = sj_crash ? CLR_RED : CLR_GREEN;
            rectfill(SCREEN_W / 2 - 70, 40, 140, 40, CLR_BLACK);
            rect(SCREEN_W / 2 - 70, 40, 140, 40, c);
            print_centered(t, SCREEN_W/2, 48, c);
            print_centered(str("%d METERS", (int)sj_dist), SCREEN_W/2, 60, CLR_WHITE);
            print_centered("Z = scoreboard", SCREEN_W/2, 70, blink(15) ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        }
    }
}

// ================================================================
//  EVENT 2 — SPEED SKATING
//   alternate Z/X in cadence to build speed; Up/Down hold the lane;
//   skate one lap, score = 100 - finish time (faster = higher).
// ================================================================
enum { SK_GO, SK_RESULT };
static int   sk_phase;
static float sk_speed;        // px/s along the track
static float sk_prog;         // 0..1 around the lap
static int   sk_expect;       // which key is wanted next: 0 = Z(A), 1 = X(B)
static float sk_window;       // time since last good stride (cadence)
static float sk_lane;         // -1..1 lane offset
static float sk_laneVel;
static float sk_time;
static int   sk_strides;
static float sk_glow;         // visual pulse on a good stride
static float sk_lean;         // body lean for animation

#define SK_LAP_DIST 1.0f
#define SK_IDEAL_LO 0.18f     // ideal cadence window (seconds between strides)
#define SK_IDEAL_HI 0.55f

static void sk_reset(void) {
    sk_phase = SK_GO; sk_speed = 0; sk_prog = 0;
    sk_expect = 0; sk_window = 0; sk_lane = 0; sk_laneVel = 0;
    sk_time = 0; sk_strides = 0; sk_glow = 0; sk_lean = 0;
    ev_done = 0; resultT = 0; newBest = 0;
}

static void sk_stride(int good) {
    if (good) {
        // cadence reward: a stride landing in the window adds the most
        float cad = sk_window;
        float bonus;
        if (cad >= SK_IDEAL_LO && cad <= SK_IDEAL_HI) bonus = 70;
        else if (cad < SK_IDEAL_LO) bonus = 25;       // too fast / mashing
        else bonus = 40;                               // too slow
        sk_speed = clamp(sk_speed + bonus, 0, 230);
        sk_strides++;
        sk_glow = 1.0f;
        sk_lean = sk_expect ? 1.0f : -1.0f;
        hit(55 + sk_strides % 4, 6, 4, 80);
    } else {
        // wrong key (didn't alternate): stumble
        sk_speed *= 0.7f;
        hit(40, INSTR_NOISE, 3, 60);
        shake(1.5f);
    }
    sk_window = 0;
}

static void sk_update(void) {
    float d = dt();

    if (sk_phase == SK_RESULT) {
        resultT += d;
        if (sk_glow > 0) sk_glow -= d * 3;
        if (resultT > 0.6f && (btnp(0, BTN_A) || btnp(0, BTN_B))) ev_done = 1;
        return;
    }

    sk_time += d;
    sk_window += d;

    // alternating-key rhythm
    int z = btnp(0, BTN_A), x = btnp(0, BTN_B);
    if (z || x) {
        int pressed = z ? 0 : 1;            // 0 = Z, 1 = X
        if (pressed == sk_expect) { sk_stride(1); sk_expect ^= 1; }
        else                       sk_stride(0);   // wrong key, no flip — must correct
    }

    // speed bleeds without input (coasting)
    sk_speed = clamp(sk_speed - 38 * d, 0, 230);

    // lane control — drift toward the rail, fight with Up/Down
    sk_laneVel += 0.4f * d;                  // drift outward
    if (btn(0, BTN_UP))   sk_laneVel -= 2.2f * d;
    if (btn(0, BTN_DOWN)) sk_laneVel += 2.2f * d;
    sk_laneVel = clamp(sk_laneVel, -1.2f, 1.2f);
    sk_lane += sk_laneVel * d;
    // hitting the rail costs speed
    if (sk_lane < -1) { sk_lane = -1; sk_laneVel = 0; sk_speed *= 0.92f; }
    if (sk_lane > 1)  { sk_lane = 1;  sk_laneVel = 0; sk_speed *= 0.92f; }

    if (sk_glow > 0) sk_glow -= d * 3;
    sk_lean = lerp(sk_lean, 0, clamp(4 * d, 0, 1));

    // wind ambience
    if (frame() % 30 == 0) hit(45, 7, 1, 200);

    // progress around the lap (lane penalty scales travel slightly)
    float lanePen = 1.0f - 0.15f * (sk_lane < 0 ? -sk_lane : sk_lane);
    sk_prog += (sk_speed * d * 0.0016f) * lanePen;
    if (sk_prog >= SK_LAP_DIST) {
        sk_phase = SK_RESULT; resultT = 0;
        int sc = (int)clamp(120 - sk_time * 6, 5, 120);
        ev_score = sc;
        fanfare();
    }
}

static void sk_draw(void) {
    cls(CLR_DARKER_BLUE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 40, CLR_DARK_BLUE);
    fillp_reset();
    // crowd hint band
    rectfill(0, 36, SCREEN_W, 10, CLR_DARKER_GREY);
    for (int k = 0; k < 40; k++) {
        int px = (k * 9) % SCREEN_W;
        int cc[4] = { CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_PINK };
        pset(px, 38 + (k % 4), cc[(k + frame() / 30) % 4]);
    }

    // the ice oval — two nested ellipses, blue ice
    int cx = SCREEN_W / 2, cy = 118;
    ovalfill(cx, cy, 140, 56, CLR_BLUE);
    ovalfill(cx, cy, 122, 44, CLR_LIGHT_GREY);
    oval(cx, cy, 140, 56, CLR_WHITE);
    oval(cx, cy, 122, 44, CLR_TRUE_BLUE);
    // lane line the skater must hug
    oval(cx, cy, 131, 50, CLR_WHITE);

    draw_snow();

    // skater position around the oval, lane offset radial
    float ang = sk_prog * 360 - 90;
    float laneR = 9 * sk_lane;
    int rx = 131 + (int)laneR, ry = 50 + (int)(laneR * 0.4f);
    int px = cx + (int)(cos_deg(ang) * rx);
    int py = cy + (int)(sin_deg(ang) * ry);
    // shadow
    ovalfill(px, py + 6, 6, 2, CLR_DARK_BLUE);
    // body, leaning into stride
    int lean = (int)(sk_lean * 3);
    rectfill(px - 3 + lean, py - 12, 6, 11, nations[nation].c0);
    circfill(px + lean, py - 13, 3, CLR_LIGHT_PEACH);
    // pumping arm + skate splay
    line(px + lean, py - 8, px + lean + (sk_expect ? 7 : -7), py - 6, nations[nation].c1);
    line(px - 3, py - 1, px - 7, py + 2, CLR_DARK_GREY);
    line(px + 3, py - 1, px + 7, py + 2, CLR_DARK_GREY);
    if (sk_glow > 0) circ(px, py - 6, 10 + (int)(sk_glow * 4), CLR_LIGHT_YELLOW);

    // ---- event UI ----
    if (sk_phase == SK_GO) {
        // the alternating prompt — big Z / X that lights the expected key
        print_centered("ALTERNATE  Z  X  - keep the rhythm", SCREEN_W/2, SCREEN_H - 28, CLR_WHITE);
        int zc = (sk_expect == 0) ? CLR_YELLOW : CLR_DARK_GREY;
        int xc = (sk_expect == 1) ? CLR_YELLOW : CLR_DARK_GREY;
        print_scaled("Z", cx - 26, SCREEN_H - 22, zc, 2);
        print_scaled("X", cx + 14, SCREEN_H - 22, xc, 2);
        // speed bar
        bar(8, SCREEN_H - 8, 90, 5, sk_speed / 230.0f, CLR_GREEN, CLR_DARKER_GREY);
        print(str("%dkph", (int)(sk_speed * 0.5f)), 8, SCREEN_H - 16, CLR_LIGHT_GREY);
        print_right("Up/Down hold lane", SCREEN_W - 4, SCREEN_H - 16, CLR_LIGHT_GREY);
        // lap progress
        bar(SCREEN_W / 2 - 45, 50, 90, 4, sk_prog, CLR_YELLOW, CLR_DARKER_GREY);
        print_centered(str("%.1fs", sk_time), SCREEN_W/2, 6, CLR_LIGHT_YELLOW);
        // rail warning
        if (sk_lane < -0.85f || sk_lane > 0.85f)
            print_centered("RAIL!", SCREEN_W/2, 60, blink(4) ? CLR_RED : CLR_ORANGE);
    } else {
        rectfill(SCREEN_W / 2 - 70, 80, 140, 42, CLR_BLACK);
        rect(SCREEN_W / 2 - 70, 80, 140, 42, CLR_GREEN);
        print_centered("FINISH!", SCREEN_W/2, 88, CLR_GREEN);
        print_centered(str("%.2f sec", sk_time), SCREEN_W/2, 100, CLR_WHITE);
        print_centered("Z = scoreboard", SCREEN_W/2, 110, blink(15) ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }
}

// ================================================================
//  the EVENT TABLE — adding an event is one row + three fns
// ================================================================
static const Event events[NEVENTS] = {
    { "SKI JUMP",      "m",   sj_reset, sj_update, sj_draw },
    { "SPEED SKATING", "pts", sk_reset, sk_update, sk_draw },
};

// ================================================================
//  shell — transitions
// ================================================================
static void go_to(int s) { pendingScreen = s; fadeT = 0.45f; }

static void enter_event(int i) {
    curEvent = i;
    events[i].reset();
    screen = S_EVENT;
}

static void finish_event(void) {
    int i = curEvent;
    lastScore[i] = ev_score;
    played[i] = 1;
    if (ev_score > bestScore[i]) {
        bestScore[i] = ev_score;
        save(i, bestScore[i]);
        newBest = 1;
    }
    go_to(S_BOARD);
}

// ================================================================
//  update
// ================================================================
void init(void) {
    setup_audio();
    init_snow();
    for (int i = 0; i < NEVENTS; i++) bestScore[i] = load(i);
    fanfare();
}

void update(void) {
    update_snow();

    // resolve a pending faded transition at the midpoint
    if (pendingScreen >= 0 && fadeT <= 0.225f) {
        screen = pendingScreen; pendingScreen = -1;
    }
    if (fadeT > 0) fadeT -= dt();

    if (screen == S_TITLE) {
        if (btnp(0, BTN_LEFT))  { nation = (nation + NNATIONS - 1) % NNATIONS; blip(64); }
        if (btnp(0, BTN_RIGHT)) { nation = (nation + 1) % NNATIONS; blip(67); }
        if (btnp(0, BTN_A))     { go_to(S_BOARD); fanfare(); }
    } else if (screen == S_BOARD) {
        int n = NEVENTS + 1;   // +1 for CEREMONY row
        if (btnp(0, BTN_UP))   { boardSel = (boardSel + n - 1) % n; blip(62); }
        if (btnp(0, BTN_DOWN)) { boardSel = (boardSel + 1) % n; blip(62); }
        if (btnp(0, BTN_A)) {
            if (boardSel < NEVENTS) enter_event(boardSel);
            else go_to(S_CEREMONY);
        }
        if (btnp(0, BTN_B)) go_to(S_TITLE);
    } else if (screen == S_EVENT) {
        events[curEvent].update();
        // guard: once the return-to-board transition is armed, don't re-arm it every
        // frame — go_to() resets fadeT, so an unguarded call pins the fade open forever
        // and the "Z = scoreboard" press never actually lands you back on the board.
        if (ev_done && pendingScreen < 0) finish_event();
        if (keyp(KEY_ESCAPE)) go_to(S_BOARD);
    } else { // S_CEREMONY
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) go_to(S_BOARD);
    }
}

// ================================================================
//  drawing — shell chrome + dispatch
// ================================================================
static void draw_hud_bar(const char *title) {
    rectfill(0, 0, SCREEN_W, 12, CLR_BLACK);
    draw_flag(3, 2, 12, 8);
    print(nations[nation].name, 18, 2, nations[nation].c1 == CLR_BLACK ? CLR_WHITE : nations[nation].c1);
    print_right(title, SCREEN_W - 4, 2, CLR_LIGHT_YELLOW);
}

static void draw_title(void) {
    cls(CLR_DARKER_BLUE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 60, CLR_DARK_BLUE);
    fillp_reset();
    // mountain skyline
    trifill(0, 130, 80, 40, 160, 130, CLR_LIGHT_GREY);
    trifill(120, 130, 200, 30, 300, 130, CLR_WHITE);
    trifill(240, 130, 320, 60, SCREEN_W, 130, CLR_LIGHT_GREY);
    rectfill(0, 130, SCREEN_W, SCREEN_H - 130, CLR_WHITE);
    draw_snow();

    print_scaled("WINTER", (SCREEN_W - text_width("WINTER") * 4) / 2, 24, CLR_BLUE, 4);
    print_scaled("GAMES",  (SCREEN_W - text_width("GAMES") * 4) / 2, 56, CLR_WHITE, 4);

    // nation picker
    print_centered("- choose your nation -", SCREEN_W/2, 96, CLR_LIGHT_YELLOW);
    draw_flag(SCREEN_W / 2 - 18, 106, 36, 22);
    print_centered(nations[nation].name, SCREEN_W/2, 132, CLR_WHITE);
    print(blink(15) ? "<" : " ", SCREEN_W / 2 - 38, 113, CLR_YELLOW);
    print(blink(15) ? ">" : " ", SCREEN_W / 2 + 32, 113, CLR_YELLOW);

    print_centered("Left/Right pick   Z start", SCREEN_W/2, SCREEN_H - 14, CLR_LIGHT_GREY);
}

static void draw_board(void) {
    cls(CLR_DARKER_BLUE);
    rectfill(0, 0, SCREEN_W, 14, CLR_BLACK);
    draw_flag(3, 3, 12, 8);
    print(nations[nation].name, 18, 3, CLR_WHITE);
    print_right("SCOREBOARD", SCREEN_W - 4, 3, CLR_LIGHT_YELLOW);
    draw_snow();

    int total = 0;
    for (int i = 0; i < NEVENTS; i++) {
        int y = 30 + i * 22;
        int on = (boardSel == i);
        if (on) { rectfill(10, y - 3, SCREEN_W - 20, 19, CLR_DARK_BLUE); rect(10, y - 3, SCREEN_W - 20, 19, CLR_YELLOW); }
        print(on ? ">" : " ", 16, y + 1, CLR_YELLOW);
        print(events[i].name, 28, y + 1, on ? CLR_WHITE : CLR_LIGHT_GREY);
        if (played[i]) print(str("last %d%s", lastScore[i], events[i].unit), 150, y + 1, CLR_LIGHT_GREY);
        print_right(str("best %d", bestScore[i]), SCREEN_W - 18, y + 1,
                    bestScore[i] > 0 ? CLR_GREEN : CLR_DARK_GREY);
        total += bestScore[i];
    }
    // CEREMONY row
    int cy = 30 + NEVENTS * 22;
    int conB = (boardSel == NEVENTS);
    if (conB) { rectfill(10, cy - 3, SCREEN_W - 20, 19, CLR_DARK_BLUE); rect(10, cy - 3, SCREEN_W - 20, 19, CLR_YELLOW); }
    print(conB ? ">" : " ", 16, cy + 1, CLR_YELLOW);
    print("MEDAL CEREMONY", 28, cy + 1, conB ? CLR_WHITE : CLR_LIGHT_GREY);
    print_right(str("total %d", total), SCREEN_W - 18, cy + 1, CLR_LIGHT_YELLOW);

    print_centered("Up/Down choose   Z enter   X title", SCREEN_W/2, SCREEN_H - 12, CLR_LIGHT_GREY);
}

static void draw_event(void) {
    events[curEvent].draw();
    draw_hud_bar(events[curEvent].name);
    if (newBest && screen == S_EVENT)
        print_centered("NEW BEST!", SCREEN_W/2, 16, blink(6) ? CLR_YELLOW : CLR_WHITE);
}

static void draw_ceremony(void) {
    cls(CLR_DARKER_BLUE);
    draw_snow();
    print_scaled("MEDAL CEREMONY", (SCREEN_W - text_width("MEDAL CEREMONY") * 2) / 2, 14, CLR_YELLOW, 2);

    int total = 0;
    for (int i = 0; i < NEVENTS; i++) {
        int y = 44 + i * 16;
        print(events[i].name, 30, y, CLR_LIGHT_GREY);
        print_right(str("%d %s", bestScore[i], events[i].unit), SCREEN_W - 30, y, CLR_WHITE);
        total += bestScore[i];
    }
    line(30, 80, SCREEN_W - 30, 80, CLR_DARK_GREY);

    // podium
    int bx = SCREEN_W / 2;
    rectfill(bx - 14, 130, 28, 30, CLR_YELLOW);       // 1st
    rectfill(bx - 46, 142, 28, 18, CLR_LIGHT_GREY);   // 2nd
    rectfill(bx + 18, 148, 28, 12, CLR_BROWN);        // 3rd
    print("1", bx - 2, 116, CLR_WHITE);
    // your athlete on the top step, flag colors
    rectfill(bx - 3, 114, 6, 12, nations[nation].c0);
    circfill(bx, 112, 3, CLR_LIGHT_PEACH);

    print_centered(str("%s  -  TOTAL %d", nations[nation].name, total), SCREEN_W/2, 92, CLR_LIGHT_YELLOW);
    print_centered("Z = back", SCREEN_W/2, SCREEN_H - 12, CLR_LIGHT_GREY);
}

void draw(void) {
    if (screen == S_TITLE)         draw_title();
    else if (screen == S_BOARD)    draw_board();
    else if (screen == S_EVENT)    draw_event();
    else                           draw_ceremony();

    // transition fade — triangle that peaks (black) at the midpoint
    if (fadeT > 0) {
        float d = fadeT - 0.225f; if (d < 0) d = -d;
        fade(clamp(1.0f - d / 0.225f, 0, 1));
    }
}
