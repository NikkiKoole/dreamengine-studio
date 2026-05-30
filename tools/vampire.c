#include "studio.h"

// VAMPIRE SURVIVORS — stay alive in a thickening swarm. You only steer; your
// weapon fires on its own at the nearest enemy. Kills drop XP gems; scoop them
// up to level, and every level you pick one of three upgrades. It only gets
// worse from here.
//
// WASD / arrows to move.  At LEVEL UP: Left/Right pick, Z confirm.

#define NE   140               // enemy pool
#define NB    90               // bullet pool
#define NG   160               // gem pool

#define ST_PLAY  0
#define ST_PICK  1
#define ST_OVER  2

typedef struct { float x, y, hp, speed; int size, col, dmg; bool alive; } Enemy;
typedef struct { float x, y, vx, vy; int dmg, pierce, life; bool alive; } Bullet;
typedef struct { float x, y; int xp; bool alive; } Gem;

static float px, py, face;
static int   hp, maxhp, hitcd;
static int   dmg, cooldown, nproj, projspd, pierce, fireT;   // weapon
static float movespd, pickup;
static int   xp, xpnext, level;
static int   state, kills, choice[3], sel;
static float t0;

static Enemy en[NE];
static Bullet bu[NB];
static Gem    gm[NG];

static const char *UP_NAME[8] = { "DAMAGE","FIRE RATE","MULTISHOT","MOVE SPD","MAGNET","MAX HP","PIERCE","PROJ SPD" };
static const char *UP_DESC[8] = { "+2 dmg","faster","+1 shot","+move","+range","+25 hp","+1 pierce","+speed" };

static float survived(void) { return now() - t0; }
static void  print_box(const char *s, int bx, int bw, int y, int col) { print(s, bx + (bw - text_width(s)) / 2, y, col); }

void init(void) {
    px = SCREEN_W / 2; py = SCREEN_H / 2; face = 0;
    hp = maxhp = 100; hitcd = 0;
    dmg = 3; cooldown = 22; nproj = 1; projspd = 4; pierce = 0; fireT = 0;
    movespd = 2.2f; pickup = 24;
    xp = 0; xpnext = 5; level = 1; kills = 0;
    state = ST_PLAY; t0 = now();
    for (int i = 0; i < NE; i++) en[i].alive = false;
    for (int i = 0; i < NB; i++) bu[i].alive = false;
    for (int i = 0; i < NG; i++) gm[i].alive = false;

    // seed a small starting swarm so it's never an empty room
    for (int i = 0; i < 10; i++) {
        en[i] = (Enemy){ rnd(SCREEN_W), rnd(SCREEN_H), 5, 0.8f, 6, CLR_RED, 8, true };
        if (distance((int)en[i].x, (int)en[i].y, (int)px, (int)py) < 50) en[i].x += 60;
    }
}

static void spawn_enemy(void) {
    float t = survived();
    int idx = -1; for (int i = 0; i < NE; i++) if (!en[i].alive) { idx = i; break; }
    if (idx < 0) return;

    int roll = rnd(100), type;
    if      (t < 12) type = 0;
    else if (t < 30) type = roll < 70 ? 0 : 1;
    else             type = roll < 50 ? 0 : roll < 80 ? 1 : 2;

    float hpmul = 1 + t / 25.0f, spmul = 1 + t / 120.0f;
    Enemy e; e.alive = true;
    if (type == 0)      { e.size = 6;  e.hp = 5  * hpmul; e.speed = 0.8f * spmul; e.col = CLR_RED;         e.dmg = 8;  }
    else if (type == 1) { e.size = 4;  e.hp = 3  * hpmul; e.speed = 1.7f * spmul; e.col = CLR_PINK;        e.dmg = 6;  }
    else                { e.size = 10; e.hp = 18 * hpmul; e.speed = 0.5f * spmul; e.col = CLR_DARK_PURPLE; e.dmg = 14; }

    int side = rnd(4);
    if      (side == 0) { e.x = rnd(SCREEN_W); e.y = -12; }
    else if (side == 1) { e.x = rnd(SCREEN_W); e.y = SCREEN_H + 12; }
    else if (side == 2) { e.x = -12;           e.y = rnd(SCREEN_H); }
    else                { e.x = SCREEN_W + 12; e.y = rnd(SCREEN_H); }
    en[idx] = e;
}

static void drop_gem(float x, float y, int v) {
    for (int i = 0; i < NG; i++) if (!gm[i].alive) { gm[i] = (Gem){ x, y, v, true }; return; }
}

static void fire(void) {
    // aim at the nearest enemy (fall back to facing direction)
    float best = 1e9f, tx = px + cos_deg(face) * 50, ty = py + sin_deg(face) * 50;
    for (int i = 0; i < NE; i++)
        if (en[i].alive) { float d = distance((int)px, (int)py, (int)en[i].x, (int)en[i].y);
                           if (d < best) { best = d; tx = en[i].x; ty = en[i].y; } }
    float base = angle_to((int)px, (int)py, (int)tx, (int)ty);
    for (int k = 0; k < nproj; k++) {
        float a = base + (k - (nproj - 1) / 2.0f) * 12;   // small fan for extra shots
        for (int i = 0; i < NB; i++)
            if (!bu[i].alive) {
                bu[i] = (Bullet){ px, py, cos_deg(a) * projspd, sin_deg(a) * projspd,
                                  dmg, pierce, 80, true };
                break;
            }
    }
    note(72, INSTR_SQUARE, 2);
}

static void apply(int id) {
    if      (id == 0) dmg += 2;
    else if (id == 1) cooldown = max(6, (int)(cooldown * 0.82f));
    else if (id == 2) nproj = min(8, nproj + 1);
    else if (id == 3) movespd += 0.4f;
    else if (id == 4) pickup += 12;
    else if (id == 5) { maxhp += 25; hp = min(maxhp, hp + 25); }
    else if (id == 6) pierce += 1;
    else if (id == 7) projspd += 1;
}

static void offer_upgrades(void) {
    int pool[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    for (int i = 0; i < 3; i++) { int j = i + rnd(8 - i); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; choice[i] = pool[i]; }
    sel = 0; state = ST_PICK;
}

void update(void) {
    if (state == ST_OVER) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }

    if (state == ST_PICK) {
        if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  sel = (sel + 2) % 3;
        if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) sel = (sel + 1) % 3;
        if (btnp(0, BTN_A) || btnp(1, BTN_A)) { apply(choice[sel]); state = ST_PLAY; chord(60, CHORD_MAJ, INSTR_SQUARE, 4); }
        return;
    }

    // ---- move ----
    float mx = (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) - (btn(0, BTN_LEFT) || btn(1, BTN_LEFT));
    float my = (btn(0, BTN_DOWN)  || btn(1, BTN_DOWN))  - (btn(0, BTN_UP)   || btn(1, BTN_UP));
    if (mx || my) face = angle_to(0, 0, (int)(mx * 10), (int)(my * 10));
    px = clamp(px + mx * movespd, 6, SCREEN_W - 6);
    py = clamp(py + my * movespd, 6, SCREEN_H - 6);

    // ---- auto-fire ----
    if (--fireT <= 0) { fire(); fireT = cooldown; }

    // ---- spawning: faster over time ----
    int interval = max(8, 46 - (int)(survived() * 0.6f));
    if (frame() % interval == 0) { spawn_enemy(); if (survived() > 40) spawn_enemy(); }

    // ---- bullets ----
    for (int i = 0; i < NB; i++) {
        if (!bu[i].alive) continue;
        bu[i].x += bu[i].vx; bu[i].y += bu[i].vy;
        if (--bu[i].life <= 0 || bu[i].x < -8 || bu[i].x > SCREEN_W + 8 || bu[i].y < -8 || bu[i].y > SCREEN_H + 8)
            { bu[i].alive = false; continue; }
        for (int j = 0; j < NE; j++) {
            if (!en[j].alive) continue;
            if (distance((int)bu[i].x, (int)bu[i].y, (int)en[j].x, (int)en[j].y) < en[j].size + 2) {
                en[j].hp -= bu[i].dmg;
                if (en[j].hp <= 0) { en[j].alive = false; kills++; drop_gem(en[j].x, en[j].y, en[j].size >= 10 ? 5 : 1);
                                     hit(48, INSTR_NOISE, 2, 40); }
                if (bu[i].pierce-- <= 0) { bu[i].alive = false; break; }
            }
        }
    }

    // ---- enemies chase, and bite you ----
    if (hitcd > 0) hitcd--;
    for (int i = 0; i < NE; i++) {
        if (!en[i].alive) continue;
        float a = angle_to((int)en[i].x, (int)en[i].y, (int)px, (int)py);
        en[i].x += cos_deg(a) * en[i].speed;
        en[i].y += sin_deg(a) * en[i].speed;
        if (hitcd <= 0 && distance((int)en[i].x, (int)en[i].y, (int)px, (int)py) < en[i].size + 6) {
            hp -= en[i].dmg; hitcd = 25; hit(36, INSTR_NOISE, 4, 90);
            if (hp <= 0) { hp = 0; state = ST_OVER; }
        }
    }

    // ---- gems: magnet in, collect, level up ----
    for (int i = 0; i < NG; i++) {
        if (!gm[i].alive) continue;
        float d = distance((int)gm[i].x, (int)gm[i].y, (int)px, (int)py);
        if (d < pickup) { float a = angle_to((int)gm[i].x, (int)gm[i].y, (int)px, (int)py);
                          gm[i].x += cos_deg(a) * 3.5f; gm[i].y += sin_deg(a) * 3.5f; }
        if (d < 8) {
            gm[i].alive = false; xp += gm[i].xp;
            while (xp >= xpnext) { xp -= xpnext; level++; xpnext = (int)(xpnext * 1.4f) + 1; offer_upgrades(); }
        }
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    for (int gx = 0; gx < SCREEN_W; gx += 24) line(gx, 0, gx, SCREEN_H, CLR_DARKER_GREY);
    for (int gy = 0; gy < SCREEN_H; gy += 24) line(0, gy, SCREEN_W, gy, CLR_DARKER_GREY);

    for (int i = 0; i < NG; i++)
        if (gm[i].alive) { circfill((int)gm[i].x, (int)gm[i].y, 2, CLR_BLUE);
                           pset((int)gm[i].x, (int)gm[i].y, CLR_WHITE); }

    for (int i = 0; i < NE; i++)
        if (en[i].alive) { circfill((int)en[i].x, (int)en[i].y, en[i].size, en[i].col);
                           circ((int)en[i].x, (int)en[i].y, en[i].size, CLR_BLACK); }

    for (int i = 0; i < NB; i++)
        if (bu[i].alive) circfill((int)bu[i].x, (int)bu[i].y, 2, CLR_YELLOW);

    // player
    int pcol = (hitcd > 20) ? CLR_WHITE : CLR_GREEN;
    circfill((int)px, (int)py, 6, pcol);
    circ((int)px, (int)py, 6, CLR_DARK_GREEN);
    line((int)px, (int)py, (int)(px + cos_deg(face) * 9), (int)(py + sin_deg(face) * 9), CLR_WHITE);

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 4, CLR_DARKER_BLUE);          // xp bar
    rectfill(0, 0, SCREEN_W * xp / xpnext, 4, CLR_BLUE);
    rectfill(6, 8, 80, 5, CLR_DARK_RED);                   // hp bar
    rectfill(6, 8, 80 * (hp < 0 ? 0 : hp) / maxhp, 5, CLR_RED);
    print(str("LV %d", level), 92, 7, CLR_WHITE);
    print(str("%d", kills), 6, 16, CLR_LIGHT_GREY);
    print_right(str("%d s", (int)survived()), SCREEN_W - 6, 7, CLR_WHITE);

    if (state == ST_PICK) {
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);     // dim (opaque box behind cards)
        print_centered("LEVEL UP!", 30, CLR_YELLOW);
        print_centered("Left / Right, then Z", 44, CLR_LIGHT_GREY);
        for (int i = 0; i < 3; i++) {
            int x = 14 + i * 100, y = 70, w = 92, h = 70;
            rectfill(x, y, w, h, CLR_DARKER_BLUE);
            rect(x, y, w, h, i == sel ? CLR_YELLOW : CLR_DARK_GREY);
            print_box(UP_NAME[choice[i]], x, w, y + 24, i == sel ? CLR_YELLOW : CLR_WHITE);
            print_box(UP_DESC[choice[i]], x, w, y + 40, CLR_LIGHT_GREY);
        }
    }

    if (state == ST_OVER) {
        rectfill(70, 74, 180, 52, CLR_BLACK);
        rect(70, 74, 180, 52, CLR_RED);
        print_centered("YOU DIED", 82, CLR_RED);
        print_centered(str("survived %d s", (int)survived()), 96, CLR_WHITE);
        print_centered(str("lv %d   kills %d", level, kills), 106, CLR_LIGHT_GREY);
        print_centered("Z to retry", 116, CLR_DARK_GREY);
    }
}
