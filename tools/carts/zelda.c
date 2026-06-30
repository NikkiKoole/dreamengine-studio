/* de:meta
{
  "title": "lil hero (zelda)",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "tilemap-collision",
    "state-machine",
    "finite-state-ai"
  ],
  "lineage": "Direct homage to The Legend of Zelda (1986); novel in compressing the room-grid exploration, sliding-screen transition, and enemy AI (Octorok ranged/Moblin chase) into a single-file primitive cart with no sprites.",
  "genre": "rpg",
  "homage": "The Legend of Zelda (1986)",
  "description": "A tiny Legend of Zelda. Explore a 3x3 grid of rooms — the screen slides when you step through a doorway — stab enemies with your sword, and at full hearts the sword throws a beam. Octoroks spit rocks, Moblins chase you; killing them drops hearts and rupees. Find the room with the Triforce and grab it. WASD/arrows move, Z sword.",
  "todo": [
    "Open: needs direction — what should this become? (maker unsure)"
  ]
}
de:meta */
#include "studio.h"

// LIL HERO — a tiny Legend of Zelda. Explore a grid of rooms (the screen
// slides when you step through a doorway), stab enemies with your sword, and
// at full hearts your sword throws a beam. Octoroks spit rocks, Moblins chase.
// Find the room with the Triforce and grab it.
//
// WASD / arrows move    Z sword

#define TS  16
#define OY  24                  // HUD height
#define GW  20
#define GH  11
#define RX  3                   // rooms across
#define RY  3                   // rooms down
#define LW  12
#define LH  12

enum { FLOOR, WALL };
enum { S_PLAY, S_WIN, S_DEAD };
enum { OCTO, MOBLIN };

static unsigned char room[GH][GW], oldroom[GH][GW];
static int  rx, ry;             // current room
static float gx0_start;         // (unused placeholder removed)

// link
static float lx, ly, lvx, lvy;
static int   lfx, lfy;          // facing
static int   hp, maxhp;
static float iframe, atkT, kbT;
static float kbx, kby;

// entities
typedef struct { float x, y, vx, vy; int type, hp, alive; float moveT, shootT; } Enemy;
typedef struct { float x, y, vx, vy; int owner, alive; float life; } Shot;   // owner 0=enemy 1=player
typedef struct { float x, y; int type, alive; float life; } Pick;            // 0 heart 1 rupee 2 triforce

#define NE 16
#define NS 32
#define NP 12
static Enemy en[NE];
static Shot  sh[NS];
static Pick  pk[NP];

static int   rupees, state;

// transition
static int   transOn, transDir;     // dir 0 L 1 R 2 U 3 D
static float transT;

static const int GOAL_RX = 1, GOAL_RY = 0, START_RX = 1, START_RY = 2;

static int   exists(int x, int y) { return x >= 0 && y >= 0 && x < RX && y < RY; }

// is the in-bounds tile here a wall? (out of bounds is NOT a wall, so the hero can step off through a gap)
static int wall_link(float px, float py) {
    int tx = (int)(px / TS), ty = (int)((py - OY) / TS);
    if (tx < 0 || ty < 0 || tx >= GW || ty >= GH) return 0;
    return room[ty][tx] == WALL;
}
// for enemies/shots: off-screen counts as solid
static int wall_solid(float px, float py) {
    int tx = (int)(px / TS), ty = (int)((py - OY) / TS);
    if (tx < 0 || ty < 0 || tx >= GW || ty >= GH) return 1;
    return room[ty][tx] == WALL;
}
static int box_link(float x, float y) {
    return wall_link(x, y) || wall_link(x + LW - 1, y) || wall_link(x, y + LH - 1) || wall_link(x + LW - 1, y + LH - 1);
}
static int box_solid(float x, float y, int s) {
    return wall_solid(x, y) || wall_solid(x + s - 1, y) || wall_solid(x, y + s - 1) || wall_solid(x + s - 1, y + s - 1);
}

static void gen_room(int Rx, int Ry, unsigned char out[GH][GW]) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) out[y][x] = FLOOR;
    for (int x = 0; x < GW; x++) { out[0][x] = WALL; out[GH - 1][x] = WALL; }
    for (int y = 0; y < GH; y++) { out[y][0] = WALL; out[y][GW - 1] = WALL; }
    if (exists(Rx, Ry - 1)) { out[0][GW / 2 - 1] = FLOOR; out[0][GW / 2] = FLOOR; }
    if (exists(Rx, Ry + 1)) { out[GH - 1][GW / 2 - 1] = FLOOR; out[GH - 1][GW / 2] = FLOOR; }
    if (exists(Rx - 1, Ry)) { out[GH / 2 - 1][0] = FLOOR; out[GH / 2][0] = FLOOR; }
    if (exists(Rx + 1, Ry)) { out[GH / 2 - 1][GW - 1] = FLOOR; out[GH / 2][GW - 1] = FLOOR; }
    // deterministic interior pillars
    int s = Rx * 5 + Ry * 11;
    for (int cyq = 3; cyq <= GH - 4; cyq += 3)
        for (int cxq = 4; cxq <= GW - 5; cxq += 4)
            if ((s + cxq + cyq) % 3 == 0) { out[cyq][cxq] = WALL; out[cyq][cxq + 1] = WALL; }
}

static void spawn_enemies(void) {
    for (int i = 0; i < NE; i++) en[i].alive = 0;
    int depth = abs(rx - START_RX) + abs(ry - START_RY);
    int cnt = 2 + min(3, depth);
    int made = 0;
    for (int c = 0; c < cnt; c++)
        for (int t = 0; t < 30; t++) {
            int gx = rnd_between(1, GW - 1), gy = rnd_between(1, GH - 1);
            if (room[gy][gx] != FLOOR) continue;
            float px = gx * TS + 2, py = OY + gy * TS + 2;
            if (distance((int)px, (int)py, (int)lx, (int)ly) < 44) continue;
            int isMob = rnd(100) < (depth >= 2 ? 45 : depth >= 1 ? 25 : 8);
            en[made] = (Enemy){ px, py, 0, 0, isMob ? MOBLIN : OCTO, isMob ? 3 : 2, 1,
                                rnd_float_between(0.4f, 1.2f), rnd_float_between(1.0f, 2.4f) };
            made++; break;
        }
}

void init(void) {
    rx = START_RX; ry = START_RY;
    gen_room(rx, ry, room);
    lx = GW * TS / 2 - LW / 2; ly = OY + GH * TS / 2;
    lfx = 0; lfy = 1; lvx = lvy = 0;
    maxhp = 6; hp = 6; iframe = 0; atkT = 0; kbT = 0;
    rupees = 0; state = S_PLAY; transOn = 0; transT = 0;
    for (int i = 0; i < NS; i++) sh[i].alive = 0;
    for (int i = 0; i < NP; i++) pk[i].alive = 0;
    spawn_enemies();
}

static void add_shot(float x, float y, float vx, float vy, int owner) {
    for (int i = 0; i < NS; i++) if (!sh[i].alive) { sh[i] = (Shot){ x, y, vx, vy, owner, 1, 2.4f }; return; }
}
static void add_pick(float x, float y, int type) {
    for (int i = 0; i < NP; i++) if (!pk[i].alive) { pk[i] = (Pick){ x, y, type, 1, type == 2 ? 9999 : 8.0f }; return; }
}

static void hurt_link(int dmg, float fromx, float fromy) {
    if (iframe > 0) return;
    hp -= dmg; iframe = 1.1f;
    float a = angle_to((int)fromx, (int)fromy, (int)lx, (int)ly);
    kbx = cos_deg(a) * 150; kby = sin_deg(a) * 150; kbT = 0.16f;
    hit(40, INSTR_NOISE, 4, 90);
    if (hp <= 0) { hp = 0; state = S_DEAD; }
}

static void start_trans(int dir) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) oldroom[y][x] = room[y][x];
    int nx = rx + (dir == 1) - (dir == 0);
    int ny = ry + (dir == 3) - (dir == 2);
    rx = nx; ry = ny;
    gen_room(rx, ry, room);
    if      (dir == 0) lx = GW * TS - LW - 2;     // went left -> appear on right
    else if (dir == 1) lx = 2;
    else if (dir == 2) ly = OY + GH * TS - LH - 2;
    else               ly = OY + 2;
    for (int i = 0; i < NS; i++) sh[i].alive = 0;
    transOn = 1; transDir = dir; transT = 0;
    kbT = 0; lvx = lvy = 0;
}

void update(void) {
    float d = dt();

    if (state != S_PLAY) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }

    if (transOn) {                                   // slide between rooms
        transT += d * 3.2f;
        if (transT >= 1) { transOn = 0; transT = 0; spawn_enemies(); }
        return;
    }

    if (iframe > 0) iframe -= d;
    if (atkT   > 0) atkT   -= d;

    // ---- move ----
    float spd = 78;
    int ix = (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) - (btn(0, BTN_LEFT) || btn(1, BTN_LEFT));
    int iy = (btn(0, BTN_DOWN)  || btn(1, BTN_DOWN))  - (btn(0, BTN_UP)   || btn(1, BTN_UP));
    if (kbT > 0) { lvx = kbx; lvy = kby; kbT -= d; }
    else {
        lvx = ix * spd; lvy = iy * spd;
        if (ix || iy) { lfx = ix; lfy = (ix ? 0 : iy); if (ix == 0) { lfx = 0; lfy = iy; } else { lfx = ix; lfy = 0; } }
    }

    float nx = lx + lvx * d;
    if (!box_link(nx, ly)) lx = nx; else if (kbT <= 0) lvx = 0;
    float ny = ly + lvy * d;
    if (!box_link(lx, ny)) ly = ny; else if (kbT <= 0) lvy = 0;

    // ---- attack ----
    if ((btnp(0, BTN_A) || btnp(1, BTN_A)) && atkT <= 0) {
        atkT = 0.26f;
        note(72, INSTR_SQUARE, 3);
        if (hp >= maxhp) {                            // sword beam at full health
            add_shot(lx + LW / 2, ly + LH / 2, lfx * 150, lfy * 150, 1);
        }
    }
    // sword hitbox (active early in the swing)
    int swinging = atkT > 0.13f;
    float sx = lx, sy = ly, sw = LW, sh_ = LH;
    if (swinging) {
        if      (lfx > 0) { sx = lx + LW; sw = 14; }
        else if (lfx < 0) { sx = lx - 14; sw = 14; }
        else if (lfy > 0) { sy = ly + LH; sh_ = 14; }
        else              { sy = ly - 14; sh_ = 14; }
    }

    // ---- room transition when stepping off through a gap ----
    if      (lx < -2          && exists(rx - 1, ry)) start_trans(0);
    else if (lx + LW > GW * TS + 2 && exists(rx + 1, ry)) start_trans(1);
    else if (ly < OY - 2      && exists(rx, ry - 1)) start_trans(2);
    else if (ly + LH > OY + GH * TS + 2 && exists(rx, ry + 1)) start_trans(3);
    else {
        lx = clamp(lx, 1, GW * TS - LW - 1);
        ly = clamp(ly, OY + 1, OY + GH * TS - LH - 1);
    }
    if (transOn) return;

    // ---- enemies ----
    int aliveCount = 0;
    for (int i = 0; i < NE; i++) {
        Enemy *e = &en[i]; if (!e->alive) continue; aliveCount++;
        if (e->type == OCTO) {
            e->moveT -= d;
            if (e->moveT <= 0) { int d = rnd(4); int dxs[4] = {1,-1,0,0}, dys[4] = {0,0,1,-1};
                                 e->vx = dxs[d] * 48; e->vy = dys[d] * 48; e->moveT = rnd_float_between(0.5f, 1.4f); }
            e->shootT -= d;
            if (e->shootT <= 0) { e->shootT = rnd_float_between(1.4f, 2.8f);
                                  float vx = e->vx ? sgn((int)e->vx) : 0, vy = e->vy ? sgn((int)e->vy) : 0;
                                  if (!vx && !vy) vy = 1;
                                  add_shot(e->x + 4, e->y + 4, vx * 96, vy * 96, 0); }
        } else {                                       // moblin chases
            float a = angle_to((int)e->x, (int)e->y, (int)lx, (int)ly);
            e->vx = cos_deg(a) * 60; e->vy = sin_deg(a) * 60;
        }
        float ex = e->x + e->vx * d; if (!box_solid(ex, e->y, 10)) e->x = ex; else e->moveT = 0;
        float ey = e->y + e->vy * d; if (!box_solid(e->x, ey, 10)) e->y = ey; else e->moveT = 0;
        e->x = clamp(e->x, TS, GW * TS - TS - 10); e->y = clamp(e->y, OY + TS, OY + GH * TS - TS - 10);

        // sword hit?
        if (swinging && boxes_touch((int)sx, (int)sy, (int)sw, (int)sh_, (int)e->x, (int)e->y, 10, 10)) {
            e->hp--; hit(60, INSTR_NOISE, 2, 40);
            float a = angle_to((int)lx, (int)ly, (int)e->x, (int)e->y);
            e->x += cos_deg(a) * 6; e->y += sin_deg(a) * 6;
            if (e->hp <= 0) { e->alive = 0; aliveCount--;
                              if (chance(45)) add_pick(e->x, e->y, rnd(2));   // drop heart or rupee
                              for (int p = 0; p < 6; p++) add_shot(e->x + 4, e->y + 4, rnd_float_between(-40,40), rnd_float_between(-40,40), 2); }
        }
        // touch damage
        if (e->alive && boxes_touch((int)lx, (int)ly, LW, LH, (int)e->x, (int)e->y, 10, 10)) hurt_link(2, e->x, e->y);
    }

    // ---- shots ----
    for (int i = 0; i < NS; i++) {
        Shot *p = &sh[i]; if (!p->alive) continue;
        p->x += p->vx * d; p->y += p->vy * d; p->life -= d;
        if (p->life <= 0 || wall_solid(p->x, p->y)) { p->alive = 0; continue; }
        if (p->owner == 0) { if (boxes_touch((int)p->x - 2, (int)p->y - 2, 4, 4, (int)lx, (int)ly, LW, LH)) { hurt_link(2, p->x, p->y); p->alive = 0; } }
        else { for (int j = 0; j < NE; j++) if (en[j].alive && boxes_touch((int)p->x - 2, (int)p->y - 2, 4, 4, (int)en[j].x, (int)en[j].y, 10, 10)) {
                   en[j].hp--; p->alive = 0; if (en[j].hp <= 0) { en[j].alive = 0; if (chance(45)) add_pick(en[j].x, en[j].y, rnd(2)); } break; } }
    }

    // ---- triforce appears once the goal room is cleared ----
    if (rx == GOAL_RX && ry == GOAL_RY && aliveCount == 0) {
        int has = 0; for (int i = 0; i < NP; i++) if (pk[i].alive && pk[i].type == 2) has = 1;
        if (!has) add_pick(GW * TS / 2 - 4, OY + GH * TS / 2 - 4, 2);
    }

    // ---- pickups ----
    for (int i = 0; i < NP; i++) {
        Pick *q = &pk[i]; if (!q->alive) continue;
        if (q->type != 2) { q->life -= d; if (q->life <= 0) { q->alive = 0; continue; } }
        if (boxes_touch((int)lx, (int)ly, LW, LH, (int)q->x, (int)q->y, 10, 10)) {
            if (q->type == 0)      { hp = min(maxhp, hp + 2); note(76, INSTR_SINE, 3); }
            else if (q->type == 1) { rupees++; note(80, INSTR_SQUARE, 2); }
            else                   { state = S_WIN; strum(60, CHORD_MAJ7, INSTR_SQUARE, 5, 60); }
            q->alive = 0;
        }
    }
}

// ---- drawing ----
static void draw_tiles(unsigned char arr[GH][GW], int ox, int oy, int goal) {
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int px = gx * TS + ox, py = OY + gy * TS + oy;
            if (arr[gy][gx] == WALL) { rectfill(px, py, TS, TS, CLR_DARK_BROWN); rectfill(px, py, TS, 3, CLR_BROWN); }
            else { rectfill(px, py, TS, TS, goal ? CLR_DARKER_BLUE : CLR_DARK_GREEN);
                   pset(px + 4, py + 9, goal ? CLR_BLUE : CLR_MEDIUM_GREEN); pset(px + 11, py + 5, goal ? CLR_BLUE : CLR_MEDIUM_GREEN); }
        }
}

static void draw_link(int x, int y) {
    if (iframe > 0 && ((int)(now() * 20) & 1)) return;       // blink while invincible
    rectfill(x + 2, y + 5, LW - 4, LH - 5, CLR_DARK_GREEN);  // tunic
    circfill(x + LW / 2, y + 4, 3, CLR_LIGHT_PEACH);         // face
    tri(x + 1, y + 3, x + LW - 1, y + 3, x + LW / 2, y - 2, CLR_GREEN);  // cap
    // sword
    if (atkT > 0.13f) {
        int cx = x + LW / 2, cy = y + LH / 2;
        line(cx, cy, cx + lfx * 14, cy + lfy * 14, CLR_LIGHT_GREY);
        line(cx + 1, cy, cx + 1 + lfx * 14, cy + lfy * 14, CLR_WHITE);
    }
}

static void heart(int x, int y, int c) {
    circfill(x + 2, y + 2, 2, c); circfill(x + 6, y + 2, 2, c);
    trifill(x, y + 2, x + 8, y + 2, x + 4, y + 7, c);
}

void draw(void) {
    cls(CLR_BLACK);

    int goal = (rx == GOAL_RX && ry == GOAL_RY);
    if (transOn) {
        float p = ease_in_out(transT), W = GW * TS, Ph = GH * TS;
        int oox = 0, ooy = 0, nox = 0, noy = 0;
        if      (transDir == 0) { oox = (int)(p * W);  nox = (int)(p * W - W); }
        else if (transDir == 1) { oox = (int)(-p * W); nox = (int)(W - p * W); }
        else if (transDir == 2) { ooy = (int)(p * Ph); noy = (int)(p * Ph - Ph); }
        else                    { ooy = (int)(-p * Ph); noy = (int)(Ph - p * Ph); }
        int oldGoal = 0; // old room rarely the goal; fine to draw normal
        draw_tiles(oldroom, oox, ooy, oldGoal);
        draw_tiles(room, nox, noy, goal);
        draw_link((int)lx + nox, (int)ly + noy);
    } else {
        draw_tiles(room, 0, 0, goal);

        for (int i = 0; i < NP; i++) {
            Pick *q = &pk[i]; if (!q->alive) continue;
            if (q->type == 0)      heart((int)q->x, (int)q->y, CLR_RED);
            else if (q->type == 1) { rectfill((int)q->x + 2, (int)q->y, 4, 9, CLR_GREEN); pset((int)q->x + 1, (int)q->y + 4, CLR_LIME_GREEN); }
            else { int bx = (int)q->x, by = (int)q->y + (int)(sin_deg(now() * 120) * 2);   // triforce
                   trifill(bx + 4, by - 4, bx, by + 4, bx + 8, by + 4, CLR_YELLOW);
                   trifill(bx + 4, by, bx + 1, by + 8, bx + 7, by + 8, CLR_LIGHT_YELLOW); }
        }
        for (int i = 0; i < NE; i++) {
            Enemy *e = &en[i]; if (!e->alive) continue;
            if (e->type == OCTO) { rectfill((int)e->x, (int)e->y, 10, 10, CLR_RED);
                                   pset((int)e->x + 2, (int)e->y + 3, CLR_WHITE); pset((int)e->x + 7, (int)e->y + 3, CLR_WHITE); }
            else { rectfill((int)e->x, (int)e->y, 10, 10, CLR_DARK_PURPLE);
                   rectfill((int)e->x + 2, (int)e->y - 2, 6, 3, CLR_DARK_PURPLE);   // ears
                   pset((int)e->x + 2, (int)e->y + 3, CLR_RED); pset((int)e->x + 7, (int)e->y + 3, CLR_RED); }
        }
        for (int i = 0; i < NS; i++) { Shot *p = &sh[i]; if (!p->alive) continue;
            circfill((int)p->x, (int)p->y, 2, p->owner ? CLR_LIGHT_YELLOW : CLR_ORANGE); }

        draw_link((int)lx, (int)ly);
    }

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, OY, CLR_BLACK);
    line(0, OY, SCREEN_W, OY, CLR_DARK_GREY);
    print("-LIFE-", 8, 4, CLR_RED);
    for (int i = 0; i < maxhp / 2; i++) {
        int v = hp - i * 2;
        heart(58 + i * 12, 6, v >= 2 ? CLR_RED : v == 1 ? CLR_DARK_RED : CLR_DARKER_GREY);
    }
    print(str("$%d", rupees), SCREEN_W - 60, 8, CLR_YELLOW);
    print(str("%d,%d", rx, ry), SCREEN_W - 24, 8, CLR_DARK_GREY);

    if (state == S_WIN) {
        rectfill(70, 80, 180, 40, CLR_BLACK); rect(70, 80, 180, 40, CLR_YELLOW);
        print_centered("YOU GOT THE TRIFORCE!", SCREEN_W/2, 90, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    }
    if (state == S_DEAD) {
        rectfill(80, 80, 160, 40, CLR_BLACK); rect(80, 80, 160, 40, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, 90, CLR_RED);
        print_centered("Z to try again", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    }
}
