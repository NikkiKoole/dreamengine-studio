/* de:meta
{
  "slug": "pitfall",
  "title": "jungle run",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Direct tribute to Activision Pitfall! (1982); replaces the original LFSR world generator with a deterministic hash-per-screen (shash), adds a pendulum-style vine swing computed from a sine of elapsed time rather than real pendulum physics.",
  "genre": "platformer",
  "homage": "Pitfall! (1982)",
  "description": "A tribute to Pitfall! (1982). Run through a procedurally generated jungle, jumping rolling logs and open holes, swinging across tar pits on a vine, and grabbing gold before the 2-minute clock runs out — with three lives. LEFT/RIGHT run (walk off a screen edge to reach the next screen), A jumps; in mid-air over a tar pit, A grabs the vine and A again lets go, so time your swing. Each screen is built from its number, so the jungle is the same every visit, just like the original.",
  "todo": [
    "Bug: can't get off the rope."
  ]
}
de:meta */
#include "studio.h"

// ── JUNGLE RUN — a Pitfall! tribute ───────────────────────────
// Run through a procedurally generated jungle. Jump the rolling
// logs and open holes, swing across tar pits on a VINE, and grab
// the gold before the clock runs out. Three lives.
//
//   • LEFT / RIGHT — run   (walk off a screen edge to the next)
//   • A            — jump  (and, in mid-air over a pit, grab the vine;
//                           press A again on the vine to let go)
//
// Each screen is built from its number, so the jungle is the same
// every time — just like the LFSR world of the 1982 original.

#define GROUND_Y 150        // Harry's feet rest here
#define UNDER_Y  168        // top of the black underground band
#define RUN_SPD  1.8f
#define JUMP_V   -4.6f
#define GRAV     0.26f
#define TIME_TOTAL 120

// screen archetypes
enum { K_OPEN, K_LOGS, K_HOLE, K_VINE, K_BEAST };

// vine pivot / pit
#define PVX 160
#define PVY 22
#define VLEN 106
#define VAMP 0.80f

static float hx, hy, hvx, hvy;     // Harry
static int   face = 1;
static bool  jumping, on_vine;
static int   scr, kind;
static bool  has_treasure;         // treasure still on this screen?
static int   score, lives;
static float start_t;
static int   state;                // 0 play, 1 gameover
static int   hurt_cd;              // log-stumble cooldown
static float tipx, tipy, ptipx, ptipy;   // vine tip + previous (for release velocity)
static bool  ready = false;

// per-screen hazard params
static float hole0, hole1, pit0, pit1, beast_x;
static float logx[3]; static int nlogs;

// deterministic per-screen RNG
static unsigned shash(int s, int salt) {
    unsigned h = (unsigned)(s * 73856093) ^ (unsigned)(salt * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

static void enter_screen(void) {
    kind = (scr == 0) ? K_OPEN : (int)(shash(scr, 1) % 5);
    has_treasure = (kind == K_OPEN || kind == K_BEAST) && (shash(scr, 2) % 3 == 0);
    hole0 = 124; hole1 = 168;
    pit0 = 106;  pit1 = 214;
    beast_x = 150 + (shash(scr, 3) % 40);
    nlogs = 2 + (int)(shash(scr, 4) % 2);
    for (int i = 0; i < nlogs; i++) logx[i] = 200 + i * 70;
}

static void respawn(void) {
    hx = 14; hy = GROUND_Y; hvx = hvy = 0;
    jumping = on_vine = false; face = 1;
    enter_screen();
}

static void reset_game(void) {
    score = 0; lives = 3; scr = 0; state = 0;
    start_t = now();
    respawn();
    ready = true;
}

static void die(void) {
    lives--;
    note(40, INSTR_SAW, 5);
    if (lives < 0) { state = 1; return; }
    respawn();
}

static bool gap_at(float x) {
    if (kind == K_HOLE) return x > hole0 && x < hole1;
    if (kind == K_VINE) return x > pit0 && x < pit1;
    return false;
}

void update(void) {
    if (!ready) reset_game();

    if (state == 1) { if (btnp(0, BTN_A)) reset_game(); return; }

    if (TIME_TOTAL - (now() - start_t) <= 0) { state = 1; return; }
    if (hurt_cd > 0) hurt_cd--;

    // ── vine swing ──
    float ang = VAMP * sin_deg((now() - start_t) * 150.0f);
    ptipx = tipx; ptipy = tipy;
    tipx = PVX + VLEN * sin_deg(ang * 57.2958f);
    tipy = PVY + VLEN * cos_deg(ang * 57.2958f);

    if (on_vine) {
        hx = tipx; hy = tipy + 18;
        if (btnp(0, BTN_A)) {                 // let go — keep the vine's momentum
            on_vine = false; jumping = true;
            hvx = (tipx - ptipx) * 1.4f; hvy = (tipy - ptipy) * 1.4f;
        }
        return;
    }

    // ── running ──
    if (!jumping) {
        if (btn(0, BTN_LEFT))  { hx -= RUN_SPD; face = -1; }
        if (btn(0, BTN_RIGHT)) { hx += RUN_SPD; face =  1; }
        if (btnp(0, BTN_A)) { jumping = true; hvy = JUMP_V; hvx = face * RUN_SPD; }
    } else {
        hx += hvx; hy += hvy; hvy += GRAV;
        if (btn(0, BTN_LEFT))  hx -= 0.6f;    // a little air control
        if (btn(0, BTN_RIGHT)) hx += 0.6f;
        // grab the vine on a screen with a tar pit
        if (kind == K_VINE && distance((int)hx, (int)hy - 14, (int)tipx, (int)tipy) < 16) {
            on_vine = true; jumping = false; note(72, INSTR_SINE, 3); return;
        }
        if (hy >= GROUND_Y) {                 // landing
            hy = GROUND_Y; hvy = 0;
            if (gap_at(hx)) { die(); return; }
            jumping = false;
        }
    }

    // walk off the edges → next / previous screen
    if (hx > SCREEN_W - 6) { scr++;        enter_screen(); hx = 10;  jumping = on_vine = false; hy = GROUND_Y; }
    if (hx < 4 && scr > 0) { scr--;        enter_screen(); hx = SCREEN_W - 12; jumping = false; hy = GROUND_Y; }
    if (hx < 4)            hx = 4;

    // fall through a gap while standing on the ground
    if (!jumping && gap_at(hx)) { die(); return; }

    // ── hazards ──
    if (kind == K_LOGS) {
        for (int i = 0; i < nlogs; i++) {
            logx[i] -= 1.4f;
            if (logx[i] < -12) logx[i] += SCREEN_W + 40;
            if (hurt_cd == 0 && hy > GROUND_Y - 10 && near((int)hx, (int)hy, (int)logx[i], GROUND_Y, 9)) {
                score = max(0, score - 100); hx -= face * 10; hurt_cd = 40; note(48, INSTR_NOISE, 3);
            }
        }
    } else if (kind == K_BEAST) {
        if (hy > GROUND_Y - 10 && near((int)hx, (int)hy, (int)beast_x, GROUND_Y, 9)) { die(); return; }
    }

    // treasure
    if (has_treasure && hy > GROUND_Y - 12 && near((int)hx, (int)hy, 250, GROUND_Y - 6, 12)) {
        has_treasure = false; score += 2000; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40);
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_harry(void) {
    int x = (int)hx, y = (int)hy;
    bool moving = btn(0, BTN_LEFT) || btn(0, BTN_RIGHT);
    int step = (jumping || on_vine) ? 2 : (moving ? anim(3, 10, 0) - 1 : 0);
    circfill(x, y - 1, 3, CLR_DARKER_GREY);                 // shadow
    rectfill(x - 2 - step, y - 8, 3, 8, CLR_DARK_BLUE);     // back leg
    rectfill(x + step,     y - 8, 3, 8, CLR_DARK_BLUE);     // front leg
    rectfill(x - 3, y - 17, 7, 10, CLR_RED);                // shirt
    rectfill(x + face, y - 14, 4, 3, CLR_LIGHT_PEACH);      // arm
    circfill(x, y - 20, 3, CLR_LIGHT_PEACH);                // head
    pset(x + face * 2, y - 20, CLR_BLACK);                  // eye
}

static void draw_log(int x) {
    rectfill(x - 7, GROUND_Y - 8, 14, 8, CLR_BROWN);
    rect(x - 7, GROUND_Y - 8, 14, 8, CLR_DARK_BROWN);
    line(x - 4, GROUND_Y - 8, x - 4, GROUND_Y - 1, CLR_DARK_BROWN);
    line(x + 2, GROUND_Y - 8, x + 2, GROUND_Y - 1, CLR_DARK_BROWN);
    circ(x - 7, GROUND_Y - 4, 1, CLR_DARK_ORANGE);
}

void draw(void) {
    if (!ready) reset_game();

    // jungle backdrop — trees against a dark jungle depth (Pitfall style)
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 10, SCREEN_W, 46, CLR_DARK_GREEN);          // canopy band
    for (int i = 0; i < 12; i++)
        circfill((i * 31 + scr * 17) % (SCREEN_W + 20) - 10, 20 + (i % 3) * 9, 12, CLR_MEDIUM_GREEN);
    for (int i = 0; i < 3; i++)                              // trunks
        rectfill(40 + i * 110, 52, 10, GROUND_Y - 52, CLR_DARK_BROWN);

    rectfill(0, GROUND_Y, SCREEN_W, UNDER_Y - GROUND_Y, CLR_BROWN);    // ground path
    line(0, GROUND_Y, SCREEN_W, GROUND_Y, CLR_DARK_ORANGE);           // path edge
    rectfill(0, UNDER_Y, SCREEN_W, SCREEN_H - UNDER_Y, CLR_BLACK);     // underground
    line(0, UNDER_Y, SCREEN_W, UNDER_Y, CLR_DARK_BROWN);

    // hazards
    if (kind == K_HOLE) {
        rectfill((int)hole0, GROUND_Y, (int)(hole1 - hole0), UNDER_Y - GROUND_Y, CLR_BLACK);
    } else if (kind == K_VINE) {
        rectfill((int)pit0, GROUND_Y, (int)(pit1 - pit0), UNDER_Y - GROUND_Y, CLR_BLACK);   // tar
        for (int x = (int)pit0; x < (int)pit1; x += 8)
            pset(x + (frame() / 6) % 8, GROUND_Y + 2, CLR_DARKER_PURPLE);
        line(PVX, PVY, (int)tipx, (int)tipy, CLR_DARK_BROWN);          // vine
        circfill((int)tipx, (int)tipy, 3, CLR_DARK_GREEN);
        circfill(PVX, PVY, 2, CLR_DARK_BROWN);
    } else if (kind == K_LOGS) {
        for (int i = 0; i < nlogs; i++) draw_log((int)logx[i]);
    } else if (kind == K_BEAST) {
        int x = (int)beast_x;                                          // a snake
        for (int i = 0; i < 5; i++)
            circfill(x - 6 + i * 3, GROUND_Y - 3 - (i % 2) * 2, 2, CLR_GREEN);
        circfill(x + 7, GROUND_Y - 5, 2, CLR_LIME_GREEN);
        pset(x + 8, GROUND_Y - 6, CLR_RED);
    }

    // treasure (a gold bar)
    if (has_treasure) {
        int gy = GROUND_Y - 6, gx = 250;
        rectfill(gx - 6, gy - 3, 12, 6, CLR_YELLOW);
        rect(gx - 6, gy - 3, 12, 6, CLR_ORANGE);
        if (!blink(8)) pset(gx - 3, gy - 1, CLR_WHITE);
    }

    draw_harry();

    // HUD
    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    print(str("SCORE %d", score), 4, 1, CLR_YELLOW);
    int tleft = (int)(TIME_TOTAL - (now() - start_t)); if (tleft < 0) tleft = 0;
    print_centered(str("TIME %d:%02d", tleft / 60, tleft % 60), SCREEN_W/2, 1, CLR_WHITE);
    for (int i = 0; i < lives; i++) {                       // life icons
        int lx = SCREEN_W - 8 - i * 9;
        rectfill(lx - 1, 2, 3, 5, CLR_RED); circfill(lx, 2, 1, CLR_LIGHT_PEACH);
    }
    print_right(str("scr %d", scr), SCREEN_W - 44, 1, CLR_DARK_GREY);

    if (state == 1) {
        int w = 220, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 80, w, 36, CLR_DARK_PURPLE);
        rect(bx, 80, w, 36, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, 88, CLR_RED);
        print_centered(str("final score %d", score), SCREEN_W/2, 100, CLR_YELLOW);
        print_centered("- press A to play again -", SCREEN_W/2, 108, CLR_LIGHT_GREY);
    }
}
