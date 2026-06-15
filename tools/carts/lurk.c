#include "studio.h"
#include <stdio.h>

// LURK — the predator dungeon. You're @; the monsters think.
// (RogueBasin: A Better Monster AI / Roguelike Intelligence / prevent A* jams.)
//
// Each monster runs a STATE MACHINE, its glyph coloured by state:
//   WANDER (grey)  idle until it SEES you (line of sight + range) or HEARS you near
//   HUNT   (red)   rolls downhill on a Dijkstra map flooded from you -> shortest
//                  path around walls; bumps you to attack
//   SEARCH (yellow) lost sight? follow your SCENT TRAIL uphill to where you went
//   FLEE   (blue)  wounded -> rolls downhill on the NEGATED map = smart retreat
// A hurt monster SHOUTS (a horn) -> nearby monsters snap to HUNT (sound = alarm).
// Pursuers take different routes and never conga-line (the simple A*-jam fix).
// Every bark/stab/yelp/howl is an instrument PANNED by where it is — you hear the
// pack circle you in the dark.
//
// arrows/WASD move (bump a monster to attack)   R restart

#define GW 40
#define GH 22
#define TILE 8
#define HUD_Y (GH * TILE)
#define MAXM 14
#define INF 1e9f
#define DIAG 1.41421356f

enum { WALL, FLOOR };
enum { S_WANDER, S_HUNT, S_SEARCH, S_FLEE, S_DEAD };
static const int STCOL[5] = { CLR_DARK_GREY, CLR_RED, CLR_YELLOW, CLR_BLUE, CLR_BLACK };

// monster species: glyph, colour, hp, dmg, sight, hear, flee-fraction, ticks/move, instr, note
typedef struct { char g; int col, hp, dmg, sight, hear; float fleef; int speed, instr, note; const char *name; } Kind;
static const Kind KIND[4] = {
    { 'r', CLR_LIGHT_GREY,  3,  1, 5, 3, 0.99f, 7,  INSTR_MEMBRANE, 76, "rat"    }, // skittish
    { 'g', CLR_GREEN,       6,  2, 7, 4, 0.40f, 9,  INSTR_REED,     60, "goblin" }, // calls for help
    { 'o', CLR_RED,        13,  3, 6, 4, 0.00f, 13, INSTR_BRASS,    46, "orc"    }, // fearless
    { 'h', CLR_ORANGE,      4,  2, 8, 5, 0.35f, 6,  INSTR_PLUCK,    67, "hound"  }, // fast tracker
};

typedef struct {
    int x, y, k, hp, state, cd, atkcd, alert, hurt, searchT;
} Mon;
static Mon mon[MAXM];
static int nmon;

static unsigned char dmap[GH][GW];
static float dist[GH][GW], flee[GH][GW], scent[GH][GW];
static int px, py, php, kills, pmove_cd, dirty = 1, state_msg_t;
static char msg[48];

// ---- panned voice pool -----------------------------------------------------
typedef struct { int h, ttl; } Voice;
static Voice voices[16];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl <= 0) {
        voices[i].h = note_on(midi, instr, vol); note_pan(voices[i].h, pan); voices[i].ttl = ttl; return;
    }
}
static void voices_tick(void) { for (int i = 0; i < 16; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h); }
static float panx(int x) { return (float)(x * TILE) / SCREEN_W * 2 - 1; }

// ---- grid / dijkstra -------------------------------------------------------
static bool wallish(int x, int y) { return x < 0 || x >= GW || y < 0 || y >= GH || dmap[y][x] == WALL; }
static int  mon_at(int x, int y) { for (int i = 0; i < nmon; i++) if (mon[i].state != S_DEAD && mon[i].x == x && mon[i].y == y) return i; return -1; }

static void relax(float f[GH][GW]) {
    bool ch = true; int g = 0;
    while (ch && g++ < GW * GH) { ch = false;
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
            if (dmap[y][x] == WALL) continue;
            float b = f[y][x];
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                if (wallish(x+dx, y+dy)) continue;
                if (dx && dy && (wallish(x+dx, y) || wallish(x, y+dy))) continue;
                float v = f[y+dy][x+dx] + (dx && dy ? DIAG : 1.0f);
                if (v < b) b = v;
            }
            if (b < f[y][x]) { f[y][x] = b; ch = true; }
        }
    }
}
static void recompute(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) dist[y][x] = INF;
    dist[py][px] = 0; relax(dist);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        flee[y][x] = (dmap[y][x] == FLOOR && dist[y][x] < INF) ? dist[y][x] * -1.2f : INF;
    relax(flee);
    dirty = 0;
}

// bresenham line of sight (player visible to a monster?)
static bool los(int x0, int y0, int x1, int y1) {
    int dx = abs(x1-x0), dy = abs(y1-y0), sx = x0<x1?1:-1, sy = y0<y1?1:-1, e = dx-dy, x = x0, y = y0;
    while (1) {
        if (x == x1 && y == y1) return true;
        int e2 = 2*e; if (e2 > -dy) { e -= dy; x += sx; } if (e2 < dx) { e += dx; y += sy; }
        if (x == x1 && y == y1) return true;
        if (wallish(x, y)) return false;
    }
}

// step toward the lowest (downhill) open neighbour of a field, avoiding other monsters (no jam)
static bool step_down(Mon *m, float f[GH][GW]) {
    float best = f[m->y][m->x]; int bx = m->x, by = m->y;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int nx = m->x+dx, ny = m->y+dy;
        if (wallish(nx, ny)) continue;
        if (dx && dy && (wallish(m->x+dx, m->y) || wallish(m->x, m->y+dy))) continue;
        if (mon_at(nx, ny) >= 0 || (nx == px && ny == py)) continue;        // jam/player avoidance
        if (f[ny][nx] < best) { best = f[ny][nx]; bx = nx; by = ny; }
    }
    if (bx == m->x && by == m->y) return false;
    m->x = bx; m->y = by; return true;
}
// step toward the FRESHEST scent (uphill) — tracking a lost target
static bool step_scent(Mon *m) {
    float best = scent[m->y][m->x]; int bx = m->x, by = m->y;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int nx = m->x+dx, ny = m->y+dy;
        if (wallish(nx, ny) || mon_at(nx, ny) >= 0 || (nx==px && ny==py)) continue;
        if (scent[ny][nx] > best) { best = scent[ny][nx]; bx = nx; by = ny; }
    }
    if (bx == m->x && by == m->y) return false;
    m->x = bx; m->y = by; return true;
}

// ---- dungeon generation (rooms + corridors, adapted from rogue.c) ----------
static void dig_h(int x1, int x2, int y) { for (int x = (x1<x2?x1:x2); x <= (x1<x2?x2:x1); x++) dmap[y][x] = FLOOR; }
static void dig_v(int y1, int y2, int x) { for (int y = (y1<y2?y1:y2); y <= (y1<y2?y2:y1); y++) dmap[y][x] = FLOOR; }

static void new_game(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { dmap[y][x] = WALL; scent[y][x] = 0; }
    int rcx[12], rcy[12], rx[12], ry[12], rw[12], rh[12], nr = 0;
    for (int cj = 0; cj < 3; cj++) for (int ci = 0; ci < 4; ci++) {
        int cx = ci*10, cy = cj*7, w = rnd_between(4,9), h = rnd_between(3,6);
        int x0 = cx + 1 + rnd(10-w-1), y0 = cy + 1 + rnd(7-h-1);
        for (int y = y0; y < y0+h; y++) for (int x = x0; x < x0+w; x++) dmap[y][x] = FLOOR;
        rx[nr]=x0; ry[nr]=y0; rw[nr]=w; rh[nr]=h; rcx[nr]=x0+w/2; rcy[nr]=y0+h/2; nr++;
    }
    for (int i = 1; i < nr; i++) { dig_h(rcx[i-1], rcx[i], rcy[i-1]); dig_v(rcy[i-1], rcy[i], rcx[i]); }

    px = rcx[0]; py = rcy[0]; php = 24; kills = 0;
    nmon = 0;
    for (int r = 1; r < nr && nmon < MAXM; r++) {
        int cnt = 1 + rnd(2);
        for (int c = 0; c < cnt && nmon < MAXM; c++) {
            int x = rx[r] + rnd(rw[r]), y = ry[r] + rnd(rh[r]);
            if (dmap[y][x] != FLOOR || mon_at(x,y) >= 0) continue;
            int k = rnd(4);
            mon[nmon] = (Mon){ x, y, k, KIND[k].hp, S_WANDER, rnd(KIND[k].speed), 0, 0, 0, 0 };
            nmon++;
        }
    }
    // two already in your room — an immediate threat (and they'll be HUNTing on sight)
    int sc[2] = { 3, 1 }, scx[2] = { rx[0], rx[0]+rw[0]-1 }, scy[2] = { ry[0], ry[0]+rh[0]-1 };
    for (int s = 0; s < 2 && nmon < MAXM; s++)
        if (mon_at(scx[s],scy[s]) < 0 && !(scx[s]==px && scy[s]==py))
            { mon[nmon] = (Mon){ scx[s], scy[s], sc[s], KIND[sc[s]].hp, S_WANDER, 0, 0, 0, 0, 0 }; nmon++; }
    dirty = 1;
    snprintf(msg, sizeof msg, "the pack stirs...");
    state_msg_t = 120;
}
void init(void) { reverb(0.4f, 0.6f); new_game(); }

// ---- a wounded monster calls the pack -------------------------------------
static void shout(Mon *s) {
    play_pan(KIND[s->k].note - 12, INSTR_BRASS, 5, panx(s->x), 40);
    for (int i = 0; i < nmon; i++) {
        Mon *m = &mon[i];
        if (m->state == S_DEAD || m == s) continue;
        if (abs(m->x - s->x) + abs(m->y - s->y) <= 9 && m->state == S_WANDER) { m->state = S_HUNT; m->alert = 12; }
    }
    snprintf(msg, sizeof msg, "the %s shrieks for the pack!", KIND[s->k].name);
    state_msg_t = 90;
}

static void hurt_mon(int i) {
    Mon *m = &mon[i];
    int d = 3 + rnd(3); m->hp -= d;
    play_pan(KIND[m->k].note + 4, INSTR_NOISE, 4, panx(m->x), 6);   // thwack
    if (m->hp <= 0) {
        m->state = S_DEAD; kills++;
        play_pan(KIND[m->k].note - 7, INSTR_REED, 3, panx(m->x), 20);
        snprintf(msg, sizeof msg, "the %s falls.", KIND[m->k].name); state_msg_t = 80;
    } else {
        play_pan(KIND[m->k].note + 7, KIND[m->k].instr, 3, panx(m->x), 8);   // yelp
        m->hurt = 6; shout(m);
    }
}

// ---- per-monster turn ------------------------------------------------------
static void mon_turn(int i) {
    Mon *m = &mon[i]; const Kind *k = &KIND[m->k];
    int adj = (abs(m->x-px) <= 1 && abs(m->y-py) <= 1);
    bool see = (abs(m->x-px)+abs(m->y-py) <= k->sight) && los(m->x, m->y, px, py);
    bool hear = abs(m->x-px) + abs(m->y-py) <= k->hear;
    int prev = m->state;

    // --- transitions ---
    if (m->hp <= k->hp * k->fleef && k->fleef > 0) m->state = S_FLEE;
    else if (see || hear || m->alert > 0) m->state = S_HUNT;
    else if (m->state == S_HUNT) { m->state = S_SEARCH; m->searchT = 40; }
    else if (m->state == S_SEARCH && --m->searchT <= 0) m->state = S_WANDER;
    if (m->alert > 0) m->alert--;
    // give-up flee once safe
    if (m->state == S_FLEE && abs(m->x-px)+abs(m->y-py) > k->sight + 4) m->state = S_WANDER;

    if (prev != S_HUNT && m->state == S_HUNT)        // just spotted you -> growl
        play_pan(k->note - 5, k->instr, 3, panx(m->x), 12);

    // --- act ---
    if (m->state == S_HUNT && adj) {                 // attack instead of moving
        if (m->atkcd <= 0) { php -= k->dmg; m->atkcd = 26;
            play_pan(40, INSTR_NOISE, 4, panx(m->x), 7);
            if (php <= 0) { php = 0; snprintf(msg, sizeof msg, "you fall to the pack. R to rise again."); state_msg_t = 600; } }
        return;
    }
    if (m->state == S_HUNT)        step_down(m, dist);
    else if (m->state == S_FLEE)   step_down(m, flee);
    else if (m->state == S_SEARCH) { if (!step_scent(m)) m->searchT = 0; }
    else if (rnd(3) == 0) {        // WANDER: occasional drift
        int dx = rnd(3)-1, dy = rnd(3)-1;
        if (!wallish(m->x+dx, m->y+dy) && mon_at(m->x+dx, m->y+dy) < 0 && !(m->x+dx==px && m->y+dy==py)) { m->x += dx; m->y += dy; }
    }
}

static void try_move(int dx, int dy) {
    int nx = px+dx, ny = py+dy;
    if (wallish(nx, ny)) return;
    int mi = mon_at(nx, ny);
    if (mi >= 0) { hurt_mon(mi); play_pan(52, INSTR_MALLET, 4, panx(nx), 8); }   // bump attack
    else { px = nx; py = ny; scent[py][px] = 100; dirty = 1; }
}

void update(void) {
    voices_tick();
    if (state_msg_t > 0) state_msg_t--;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) scent[y][x] *= 0.992f;   // trail fades
    if (keyp('R')) { new_game(); return; }
    if (php <= 0) return;                                                    // dead: wait for R

    // player move (held-repeat)
    int dx = 0, dy = 0;
    if (btn(0,BTN_LEFT)||key('A')) dx=-1; else if (btn(0,BTN_RIGHT)||key('D')) dx=1;
    if (btn(0,BTN_UP)||key('W')) dy=-1; else if (btn(0,BTN_DOWN)||key('S')) dy=1;
    if ((dx||dy) && pmove_cd<=0) { try_move(dx, dy); pmove_cd = 6; } else if (!(dx||dy)) pmove_cd = 0;
    if (pmove_cd > 0) pmove_cd--;
    if (dirty) recompute();

    // monsters act on their own clocks
    for (int i = 0; i < nmon; i++) {
        if (mon[i].state == S_DEAD) continue;
        if (mon[i].atkcd > 0) mon[i].atkcd--;
        if (mon[i].hurt > 0) mon[i].hurt--;
        if (--mon[i].cd <= 0) { mon_turn(i); mon[i].cd = KIND[mon[i].k].speed; }
    }

#ifdef DE_TRACE
    int hunting = 0, alive = 0;
    for (int i = 0; i < nmon; i++) if (mon[i].state != S_DEAD) { alive++; if (mon[i].state == S_HUNT) hunting++; }
    watch("hp", "%d", php); watch("alive", "%d", alive); watch("hunting", "%d", hunting); watch("kills", "%d", kills);
#endif
}

// ---- draw ------------------------------------------------------------------
static void glyph(char c, int cx, int cy, int col) { print(str("%c", c), cx*TILE+1, cy*TILE, col); }

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dmap[y][x] == WALL) { rectfill(x*TILE, y*TILE, TILE, TILE, CLR_DARKER_GREY); continue; }
        if (scent[y][x] > 4) pset(x*TILE+TILE/2, y*TILE+TILE/2, scent[y][x] > 40 ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
        else pset(x*TILE+TILE/2, y*TILE+TILE/2, CLR_DARKER_GREY);
    }
    for (int i = 0; i < nmon; i++) {
        Mon *m = &mon[i]; if (m->state == S_DEAD) continue;
        int col = m->hurt > 0 ? CLR_WHITE : STCOL[m->state];
        glyph(KIND[m->k].g, m->x, m->y, col);
        if (m->state == S_HUNT && blink(10)) pset(m->x*TILE+TILE-1, m->y*TILE, CLR_RED);
    }
    glyph('@', px, py, php > 0 ? CLR_YELLOW : CLR_DARK_GREY);

    // HUD
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H-HUD_Y, CLR_DARKER_BLUE);
    rect(4, HUD_Y+3, 62, 6, CLR_DARK_GREY);
    rectfill(5, HUD_Y+4, 60*php/24, 4, php > 8 ? CLR_GREEN : CLR_RED);
    print(str("HP %d", php), 70, HUD_Y+2, CLR_WHITE);
    int alive = 0; for (int i = 0; i < nmon; i++) if (mon[i].state != S_DEAD) alive++;
    print(str("kills %d", kills), 116, HUD_Y+2, CLR_YELLOW);
    print(str("pack %d", alive), 172, HUD_Y+2, CLR_RED);
    if (alive == 0) print_right("CLEARED!  R", SCREEN_W-4, HUD_Y+2, CLR_GREEN);
    else if (php <= 0) print_right("DEAD  R", SCREEN_W-4, HUD_Y+2, CLR_RED);
    font(FONT_SMALL);
    if (state_msg_t > 0) print(msg, 4, HUD_Y+12, CLR_LIGHT_PEACH);
    else print("arrows/WASD move (bump to attack)   follow the colours: grey idle, red hunting, yellow tracking, blue fleeing", 4, HUD_Y+12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
