/* de:meta
{
  "slug": "pet",
  "title": "lil blob (pet)",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "toy"
  ],
  "teaches": [
    "save-load-persistence",
    "state-machine"
  ],
  "lineage": "Classic Tamagotchi virtual-pet loop with a multi-stat degradation sim; the novel angle is the real-time 'while you were away' catch-up that fast-forwards neglect from epoch() delta on load.",
  "description": "A real-time virtual pet, the slow ambient opposite of everything else here. Your blob gets hungry, bored, tired and dirty on its own, poops, can fall sick if neglected, and grows from egg to elder as it ages. Feed/play/wash/sleep/medicine to keep it happy. Saved with save()/load(), so it is still here — same age, same stats — next time you open the cart. Left/Right pick an action, Z to do it.",
  "todo": [
    "Bug: the outline doesn't align with the shape.",
    "Needs mouse/touch support."
  ]
}
de:meta */
#include "studio.h"

// LIL BLOB — a real-time virtual pet. It gets hungry, bored, tired and dirty
// whether you act or not, can fall sick if you neglect it, and grows up as it
// ages. Everything is saved with save()/load(), so your blob is still here
// (same age, same stats) the next time you open the cart.
//
// Left/Right pick an action,  Z do it.   FEED  PLAY  WASH  SLEEP  MEDS

// ---- save slots (ints only) ----
#define S_FLAG 0
#define S_HUN  1
#define S_HAP  2
#define S_NRG  3
#define S_CLN  4
#define S_HP   5
#define S_AGE  6
#define S_ALV  7
#define S_POO  8
#define S_SIK  9
#define S_EPO  10            // real-world clock at last save — for "while you were away"

// expressions
enum { E_HAPPY, E_NEUTRAL, E_SAD, E_SICK, E_SLEEP, E_DEAD };

static float hunger, happy, energy, clean, health, lived;
static int   alive, poop, sick;
static int   sleeping;
static float sickT, poopT, saveT, msgT, awayT;
static int   sel, prevStage, awaySecs;
static const char *msg = "";

static const char *ACT[5]  = { "FEED", "PLAY", "WASH", "SLEEP", "MEDS" };
static const char *STAGE[6] = { "EGG", "BABY", "CHILD", "TEEN", "ADULT", "ELDER" };

typedef struct { float x, y, vx, vy, life; int col; } Fx;
static Fx fx[28];

static void  pbox(const char *s, int bx, int bw, int y, int c) { print(s, bx + (bw - text_width(s)) / 2, y, c); }
static void  say(const char *s) { msg = s; msgT = 2.2f; }

static int stage_of(float a) {
    return a < 12 ? 0 : a < 75 ? 1 : a < 180 ? 2 : a < 360 ? 3 : a < 720 ? 4 : 5;
}

static void spawn_fx(float x, float y, float vx, float vy, int col) {
    for (int i = 0; i < 28; i++)
        if (fx[i].life <= 0) { fx[i] = (Fx){ x, y, vx, vy, rnd_between(18, 30), col }; return; }
}

static void save_all(void) {
    save(S_FLAG, 1);
    save(S_HUN, (int)hunger); save(S_HAP, (int)happy); save(S_NRG, (int)energy);
    save(S_CLN, (int)clean);  save(S_HP, (int)health); save(S_AGE, (int)lived);
    save(S_ALV, alive); save(S_POO, poop); save(S_SIK, sick);
    save(S_EPO, epoch());
}

// one chunk of life — used both for the live frame and to fast-forward the
// time the player was away. `asleep` only happens during a live session.
static void simulate(float d, int asleep) {
    if (stage_of(lived) == 0) return;             // egg only ages, no care yet
    if (asleep) { energy = min(100, energy + 8 * d); hunger = max(0, hunger - 0.3f * d); }
    else        { hunger = max(0, hunger - 0.7f * d); energy = max(0, energy - 0.4f * d); }
    clean = max(0, clean - (poop ? 0.9f : 0.3f) * d);

    float drain = 0.5f;
    if (hunger < 25) drain += 0.6f;
    if (clean  < 25) drain += 0.4f;
    if (poop)        drain += 0.3f;
    if (sick)        drain += 0.8f;
    if (energy < 10 && !asleep) drain += 0.5f;
    happy = max(0, happy - drain * d);

    if (!asleep) { poopT -= d; if (poopT <= 0 && !poop) poop = 1; }
    if (hunger <= 0 || clean <= 4) sickT += d; else sickT = max(0, sickT - d * 0.5f);
    if (sickT > 8) sick = 1;
    if (sick) health = max(0, health - 1.6f * d);
    else      health = min(100, health + 0.6f * d);
    if (health <= 0) alive = 0;
}

static void new_pet(void) {
    hunger = 80; happy = 80; energy = 90; clean = 90; health = 100; lived = 0;
    alive = 1; poop = 0; sick = 0; sleeping = 0;
    sickT = 0; poopT = rnd_between(18, 40);
    prevStage = 0;
    save_all();
}

void init(void) {
    saveT = 0; msgT = 0; awayT = 0; awaySecs = 0; sel = 0;
    for (int i = 0; i < 28; i++) fx[i].life = 0;

    if (load(S_FLAG) == 1) {
        hunger = load(S_HUN); happy = load(S_HAP); energy = load(S_NRG);
        clean  = load(S_CLN); health = load(S_HP); lived = load(S_AGE);
        alive  = load(S_ALV); poop = load(S_POO); sick = load(S_SIK);
        sickT = 0; poopT = rnd_between(18, 40); sleeping = 0;
        prevStage = stage_of(lived);

        // catch up on the real time that passed while the cart was closed
        int last = load(S_EPO);
        int away = last > 0 ? epoch() - last : 0;
        if (away > 86400) away = 86400;            // cap a long absence at one day of neglect
        if (away > 0 && alive) {
            awaySecs = away;
            for (float r = away; r > 0 && alive; r -= 30) { float s = r > 30 ? 30 : r; lived += s; simulate(s, 0); }
            prevStage = stage_of(lived);
            awayT = 4.0f;                           // show a welcome-back note
            save_all();
        }
    } else {
        new_pet();
    }
}

static void do_action(void) {
    if (!alive) return;
    if (sleeping && sel != 3) { say("Zzz..."); return; }
    switch (sel) {
        case 0: hunger = min(100, hunger + 32); happy = min(100, happy + 4); say("Yum!");
                for (int i = 0; i < 4; i++) spawn_fx(160 + rnd(20) - 10, 96, rnd_float_between(-1, 1), 1.4f, CLR_BROWN);
                break;
        case 1: if (energy < 10) { say("too sleepy"); break; }
                happy = min(100, happy + 26); energy = max(0, energy - 12); hunger = max(0, hunger - 5); say("Wheee!");
                for (int i = 0; i < 5; i++) spawn_fx(160 + rnd(40) - 20, 100, rnd_float_between(-0.8f, 0.8f), -1.6f, CLR_PINK);
                break;
        case 2: clean = 100; poop = 0; poopT = rnd_between(18, 40); say("Squeaky!");
                for (int i = 0; i < 6; i++) spawn_fx(160 + rnd(48) - 24, 110, rnd_float_between(-0.6f, 0.6f), -1.2f, CLR_WHITE);
                break;
        case 3: sleeping = !sleeping; say(sleeping ? "Zzz..." : "Morning!"); break;
        case 4: if (sick) { sick = 0; sickT = 0; health = min(100, health + 35); say("All better!"); }
                else { happy = max(0, happy - 4); say("bleh, fine!"); }
                break;
    }
    save_all();
}

void update(void) {
    float d = dt();

    // particles always drift
    for (int i = 0; i < 28; i++)
        if (fx[i].life > 0) { fx[i].x += fx[i].vx; fx[i].y += fx[i].vy; fx[i].vy += 0.04f; fx[i].life--; }
    if (msgT > 0)  msgT  -= d;
    if (awayT > 0) awayT -= d;

    if (!alive) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) new_pet(); return; }

    // input
    if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  sel = (sel + 4) % 5;
    if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) sel = (sel + 1) % 5;
    if (btnp(0, BTN_A)     || btnp(1, BTN_A))     do_action();

    int wasAlive = alive;
    lived += d;
    simulate(d, sleeping);
    if (sleeping && energy >= 100) { sleeping = 0; say("Morning!"); }
    if (wasAlive && !alive) { sleeping = 0; say("..."); save_all(); }

    // grew up?
    int st = stage_of(lived);
    if (st != prevStage) {
        say(st == 1 ? "It hatched!" : "Grew up!");
        for (int i = 0; i < 8; i++) spawn_fx(160, 96, rnd_float_between(-2, 2), rnd_float_between(-2, 0), CLR_YELLOW);
        prevStage = st; save_all();
    }

    saveT += d; if (saveT > 2) { save_all(); saveT = 0; }
}

// a smile (up>0) / frown (up<0) / flat mouth
static void mouth(int cx, int my, int w, int up, int c) {
    for (int dx = -w; dx <= w; dx++) {
        int dyq = (int)((1.0f - (float)(dx * dx) / (w * w)) * up);
        pset(cx + dx, my + dyq, c); pset(cx + dx, my + dyq + 1, c);
    }
}

static void draw_pet(int cx, int cy, int stage, int expr) {
    int bob = (int)(sin_deg(now() * 90) * 2);
    if (expr == E_SLEEP) bob = (int)(sin_deg(now() * 40) * 1);
    cy += bob;

    if (expr == E_DEAD) {                                   // a little ghost
        circfill(cx, cy - 4, 12, CLR_WHITE);
        rectfill(cx - 12, cy - 4, 24, 12, CLR_WHITE);
        for (int i = -2; i <= 2; i++) circfill(cx + i * 5, cy + 8, 2, CLR_WHITE);
        circfill(cx - 4, cy - 6, 2, CLR_BLACK); circfill(cx + 4, cy - 6, 2, CLR_BLACK);
        return;
    }
    if (stage == 0) {                                       // egg
        int s = 15;
        for (int dy = -s - 2; dy <= s; dy++) {
            float f = 1.0f - (float)(dy * dy) / ((s + 2) * (s + 2));
            int hw = (int)((s - 2) * (f < 0 ? 0 : f) + 2);
            line(cx - hw, cy + dy, cx + hw, cy + dy, CLR_LIGHT_PEACH);
        }
        circ(cx, cy - 1, s, CLR_PINK);
        pset(cx - 5, cy, CLR_DARK_PURPLE); pset(cx + 6, cy - 4, CLR_DARK_PURPLE);
        return;
    }

    int sz[6] = { 0, 13, 17, 21, 25, 23 };
    int col[6] = { 0, CLR_PINK, CLR_LIME_GREEN, CLR_BLUE, CLR_MAUVE, CLR_LIGHT_GREY };
    int s = sz[stage], body = col[stage];

    // antenna for teen+
    if (stage >= 3) { line(cx, cy - s, cx - 3, cy - s - 6, CLR_DARK_GREY); circfill(cx - 3, cy - s - 6, 2, CLR_YELLOW); }

    circfill(cx, cy, s, body);
    if (sick) { circfill(cx - 6, cy - 3, 2, CLR_GREEN); circfill(cx + 7, cy + 2, 2, CLR_GREEN); }
    circ(cx, cy, s, CLR_DARKER_GREY);
    circfill(cx, cy + s / 3, s * 2 / 3, CLR_LIGHT_PEACH);   // belly

    // feet
    circfill(cx - s / 2, cy + s - 1, 3, body); circfill(cx + s / 2, cy + s - 1, 3, body);

    // eyes
    int ex = s / 3, ey = -s / 4;
    if (expr == E_SLEEP) {
        line(cx - ex - 2, cy + ey, cx - ex + 2, cy + ey, CLR_BLACK);
        line(cx + ex - 2, cy + ey, cx + ex + 2, cy + ey, CLR_BLACK);
    } else if (expr == E_SICK) {
        line(cx - ex - 2, cy + ey - 2, cx - ex + 2, cy + ey + 2, CLR_BLACK);
        line(cx - ex - 2, cy + ey + 2, cx - ex + 2, cy + ey - 2, CLR_BLACK);
        line(cx + ex - 2, cy + ey - 2, cx + ex + 2, cy + ey + 2, CLR_BLACK);
        line(cx + ex - 2, cy + ey + 2, cx + ex + 2, cy + ey - 2, CLR_BLACK);
    } else {
        circfill(cx - ex, cy + ey, 3, CLR_WHITE); circfill(cx + ex, cy + ey, 3, CLR_WHITE);
        circfill(cx - ex, cy + ey + 1, 1, CLR_BLACK); circfill(cx + ex, cy + ey + 1, 1, CLR_BLACK);
    }

    // mouth by mood
    int my = cy + s / 3;
    if      (expr == E_HAPPY) mouth(cx, my,     4,  3, CLR_BLACK);
    else if (expr == E_SAD)   mouth(cx, my + 2, 4, -3, CLR_BLACK);
    else if (expr != E_SLEEP) mouth(cx, my,     3,  0, CLR_BLACK);
}

static void statbar(int x, int y, const char *lbl, float v) {
    int c = v > 50 ? CLR_GREEN : v > 25 ? CLR_YELLOW : CLR_RED;
    print(lbl, x, y, CLR_LIGHT_GREY);
    bar(x, y + 9, 60, 5, v / 100.0f, c, CLR_DARKER_GREY);
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    rectfill(0, 130, SCREEN_W, SCREEN_H - 130, CLR_DARK_PURPLE);     // floor
    line(0, 130, SCREEN_W, 130, CLR_MAUVE);

    int stage = stage_of(lived);

    // expression
    int expr;
    if      (!alive)        expr = E_DEAD;
    else if (sleeping)      expr = E_SLEEP;
    else if (sick)          expr = E_SICK;
    else if (hunger < 20 || clean < 15 || energy < 12 || happy < 20) expr = E_SAD;
    else if (happy > 65)    expr = E_HAPPY;
    else                    expr = E_NEUTRAL;

    if (sleeping) { rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_DARKER_BLUE);   // night dim
                    print("z", 178, 80, CLR_LIGHT_GREY); print("Z", 186, 72, CLR_WHITE); }

    if (poop && alive) { circfill(206, 124, 5, CLR_BROWN); circfill(210, 121, 3, CLR_BROWN); }

    draw_pet(160, 104, stage, expr);

    for (int i = 0; i < 28; i++)
        if (fx[i].life > 0) circfill((int)fx[i].x, (int)fx[i].y, 1, fx[i].col);

    // meters
    statbar(8,   6, "FOOD", hunger);
    statbar(86,  6, "FUN",  happy);
    statbar(164, 6, "NRG",  energy);
    statbar(242, 6, "WASH", clean);

    // name / age / health
    print(str("%s  age %d", STAGE[stage], (int)lived), 8, 28, CLR_WHITE);
    print("HP", 242, 28, CLR_LIGHT_GREY);
    rectfill(262, 30, 50, 5, CLR_DARK_RED);
    rectfill(262, 30, (int)(50 * (health < 0 ? 0 : health) / 100), 5, CLR_RED);

    // status message
    if (awayT > 0)           print_centered(str("you were away %dm!", awaySecs / 60), SCREEN_W/2, 122, CLR_PINK);
    else if (msgT > 0)       print_centered(msg, SCREEN_W/2, 122, CLR_YELLOW);
    else if (!alive)         print_centered("your blob has passed on...", SCREEN_W/2, 122, CLR_LIGHT_GREY);
    else if (sick)           print_centered("I don't feel good...", SCREEN_W/2, 122, CLR_GREEN);
    else if (hunger < 20)    print_centered("I'm so hungry!", SCREEN_W/2, 122, CLR_ORANGE);
    else if (clean < 20)     print_centered("it's filthy in here!", SCREEN_W/2, 122, CLR_ORANGE);
    else if (energy < 15)    print_centered("so sleepy...", SCREEN_W/2, 122, CLR_LIGHT_GREY);

    // action bar
    for (int i = 0; i < 5; i++) {
        int x = 4 + i * 63, w = 60, y = SCREEN_H - 24;
        rectfill(x, y, w, 20, i == sel ? CLR_INDIGO : CLR_DARKER_BLUE);
        rect(x, y, w, 20, i == sel ? CLR_YELLOW : CLR_DARK_GREY);
        pbox(ACT[i], x, w, y + 6, i == sel ? CLR_YELLOW : CLR_WHITE);
    }

    if (!alive) {
        rectfill(70, 84, 180, 34, CLR_BLACK); rect(70, 84, 180, 34, CLR_RED);
        print_centered(str("lived to age %d", (int)lived), SCREEN_W/2, 92, CLR_WHITE);
        print_centered("Z for a new egg", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    }
}
