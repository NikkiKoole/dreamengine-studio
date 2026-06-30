/* de:meta
{
  "title": "tempo",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "turn-based-loop",
    "algorithm-visualization",
    "schedule-driven-agents"
  ],
  "lineage": "Implements nadako's roguelike energy-scheduler (next-event-time priority queue); the timeline ribbon at the top is the explicit teaching artifact, showing the scheduling order rewrite live as you change speed.",
  "description": "Turn-based time scheduling by SPEED -- the roguelike energy-scheduler (nadako's turn-scheduling rant). Naive roguelikes loop 'for each actor: act()' so everyone moves at one rate; real ones schedule by energy/speed so a fast actor acts more often, interleaved. Each actor here has a 'next turn' time; the scheduler always runs whoever's next is soonest, then pushes their next turn out by COST/speed -- higher speed = sooner and more often (the same idea as energy accumulation, expressed as next-event times so the ORDER is drawable). The payoff is the NEXT ribbon up top: the upcoming ~14 turns in order. Press H to re-speed yourself (slow/normal/fast/blink) and watch the ribbon rewrite -- at blink you take two or three steps between a troll's single lurch. Five monsters at different speeds (slow troll -> fast hound). Arrows move/bump to attack, H change speed, R reset.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include <stdio.h>

// TEMPO — turn-based time scheduling by SPEED (the energy-scheduler nugget:
// nadako.github.io/rants/.../roguelike-turn-based-time-scheduling).
// Naive roguelikes loop "for each actor: act()" — everyone moves at one rate.
// Real ones schedule by ENERGY/SPEED: a fast actor acts more often than a slow
// one, naturally interleaved. Here each actor has a `next` time; the scheduler
// always runs whoever's `next` is smallest, then pushes their next turn out by
// COST/speed — so higher speed = sooner & more often. (Same idea as energy
// accumulation; expressed as next-event times so the ORDER is drawable.)
//
// The payoff is the TIMELINE up top: the next ~14 turns, in order. Press H to
// re-speed yourself (slow/normal/fast/blink) and watch the ribbon rewrite — at
// "blink" you take two or three steps between a troll's single lurch.
//
// arrows/WASD move (bump to attack)   H change your speed   R reset

#define NA 6                 // actor 0 = player, 1..5 = monsters
#define GW 40
#define GH 18
#define TILE 8
#define ARENA_Y 16
#define HUD_Y (ARENA_Y + GH * TILE)     // 160
#define COST 10000.0f

typedef struct { int x, y, hp, maxhp, alive; float next; int speed; char g; int col; const char *name; } Actor;
static Actor act[NA];
static unsigned char wall[GH][GW];
static int php_dead, cleared, step_cd, turn_count, pspeed_i;
static char msg[40]; static int msg_t;

static const int PSPEED[4] = { 55, 100, 165, 260 };
static const char *PSPNAME[4] = { "slow", "normal", "fast", "blink" };
// monster templates: glyph, colour, hp, speed, name
static const struct { char g; int col, hp, speed; const char *name; } MT[5] = {
    { 'T', CLR_INDIGO,     10, 55,  "troll"  },   // slow + tough
    { 'o', CLR_RED,         6, 80,  "orc"    },
    { 'g', CLR_GREEN,       4, 100, "goblin" },
    { 'r', CLR_LIGHT_GREY,  2, 150, "rat"    },
    { 'h', CLR_ORANGE,      3, 205, "hound"  },   // fast
};

// ---- voices ----------------------------------------------------------------
typedef struct { int h, ttl; } Voice; static Voice voices[12];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 12; i++) if (voices[i].ttl <= 0) { voices[i].h = note_on(midi,instr,vol); note_pan(voices[i].h,pan); voices[i].ttl = ttl; return; }
}
static void voices_tick(void) { for (int i = 0; i < 12; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h); }
static float panx(int x) { return (float)(x*TILE)/SCREEN_W*2 - 1; }

static bool wallish(int x, int y) { return x < 0 || x >= GW || y < 0 || y >= GH || wall[y][x]; }
static int actor_at(int x, int y) { for (int i = 0; i < NA; i++) if (act[i].alive && act[i].x == x && act[i].y == y) return i; return -1; }

static void setmsg(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_t = 100; }

static void reset(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        wall[y][x] = (x==0||y==0||x==GW-1||y==GH-1);
    for (int p = 0; p < 7; p++) { int wx = 5+p*5, wy = 4+(p%3)*4; if (wx<GW-1&&wy<GH-1) wall[wy][wx] = 1; }
    pspeed_i = 1; turn_count = 0; php_dead = 0; cleared = 0; step_cd = 0; msg_t = 0;
    act[0] = (Actor){ GW/2, GH/2, 20, 20, 1, 0, PSPEED[pspeed_i], '@', CLR_YELLOW, "you" };
    for (int i = 1; i < NA; i++) {
        const int m = i-1; int x, y, tries = 0;
        do { x = 2+rnd(GW-4); y = 2+rnd(GH-4); tries++; } while ((wallish(x,y)||actor_at(x,y)>=0) && tries<80);
        act[i] = (Actor){ x, y, MT[m].hp, MT[m].hp, 1, (float)rnd(50), MT[m].speed, MT[m].g, MT[m].col, MT[m].name };
    }
}
void init(void) { reverb(0.3f, 0.5f); reset(); }

// whoever's next turn comes soonest
static int next_actor(void) {
    int b = -1; float bt = 1e30f;
    for (int i = 0; i < NA; i++) if (act[i].alive && act[i].next < bt) { bt = act[i].next; b = i; }
    return b;
}
static void spend(int i) { act[i].next += COST / act[i].speed; turn_count++; }

static void hurt(int i, int dmg, float pan) {
    act[i].hp -= dmg;
    if (act[i].hp <= 0) {
        act[i].alive = 0;
        play_pan(40, INSTR_NOISE, 4, pan, 8);
        if (i == 0) { php_dead = 1; setmsg("you were overwhelmed. R to try again."); }
        else { setmsg(str("the %s falls.", act[i].name));
               int any = 0; for (int k = 1; k < NA; k++) if (act[k].alive) any = 1; if (!any) { cleared = 1; setmsg("arena cleared! R for more."); } }
    } else play_pan(54, INSTR_MEMBRANE, 3, pan, 6);
}

// monster turn: step toward the player (greedy, slides along walls), bump to hit
static void ai_act(int i) {
    Actor *m = &act[i];
    int dx = (act[0].x > m->x) - (act[0].x < m->x), dy = (act[0].y > m->y) - (act[0].y < m->y);
    if (m->x + dx == act[0].x && m->y + dy == act[0].y && (abs(act[0].x-m->x)<=1 && abs(act[0].y-m->y)<=1)) {
        int d = 1 + rnd(3); hurt(0, d, panx(m->x));            // attack
        play_pan(44, INSTR_NOISE, 3, panx(m->x), 6);
        return;
    }
    int nx = m->x+dx, ny = m->y+dy;
    if (wallish(nx,ny) || actor_at(nx,ny) >= 0) {              // blocked: try one axis
        if (!wallish(m->x+dx, m->y) && actor_at(m->x+dx,m->y)<0) ny = m->y;
        else if (!wallish(m->x, m->y+dy) && actor_at(m->x,m->y+dy)<0) nx = m->x;
        else return;
    }
    if (!(nx==act[0].x && ny==act[0].y)) { m->x = nx; m->y = ny; }
}

static void player_move(int dx, int dy) {
    int nx = act[0].x+dx, ny = act[0].y+dy;
    if (wallish(nx,ny)) return;
    int t = actor_at(nx,ny);
    if (t > 0) { hurt(t, 3+rnd(3), panx(nx)); play_pan(60, INSTR_MALLET, 4, panx(nx), 8); spend(0); }
    else { act[0].x = nx; act[0].y = ny; play_pan(72, INSTR_PLUCK, 1, panx(nx), 5); spend(0); }
}

void update(void) {
    voices_tick();
    if (msg_t > 0) msg_t--;
    if (keyp('R')) { reset(); return; }
    if (php_dead || cleared) return;
    if (keyp('H')) { pspeed_i = (pspeed_i+1) % 4; act[0].speed = PSPEED[pspeed_i]; setmsg(str("you are now %s", PSPNAME[pspeed_i])); }

    int cur = next_actor();
    if (cur == 0) {                                  // player's turn — wait for input
        int dx = 0, dy = 0;
        if (keyp(KEY_LEFT)||keyp('A')) dx=-1; else if (keyp(KEY_RIGHT)||keyp('D')) dx=1;
        else if (keyp(KEY_UP)||keyp('W')) dy=-1; else if (keyp(KEY_DOWN)||keyp('S')) dy=1;
        if (dx || dy) player_move(dx, dy);
    } else if (step_cd > 0) step_cd--;               // pace AI turns so they're watchable
    else { ai_act(cur); spend(cur); step_cd = 9; }

#ifdef DE_TRACE
    int alive=0; for (int i=1;i<NA;i++) alive+=act[i].alive;
    watch("turn", "%d", turn_count); watch("whose", "%d", next_actor());
    watch("pspeed", "%d", act[0].speed); watch("alive", "%d", alive); watch("hp", "%d", act[0].hp);
#endif
}

// ---- the turn-order timeline (simulate the scheduler forward, no side effects)
#define PREVIEW 14
static void draw_timeline(void) {
    float sim[NA]; for (int i = 0; i < NA; i++) sim[i] = act[i].next;
    print("NEXT", 2, 4, CLR_MEDIUM_GREY);
    for (int slot = 0; slot < PREVIEW; slot++) {
        int b = -1; float bt = 1e30f;
        for (int i = 0; i < NA; i++) if (act[i].alive && sim[i] < bt) { bt = sim[i]; b = i; }
        if (b < 0) break;
        int x = 36 + slot * 19;
        if (slot == 0) rectfill(x-2, 1, 13, 13, CLR_DARKER_GREY);
        print(str("%c", act[b].g), x, 4, act[b].col);
        sim[b] += COST / act[b].speed;
    }
}

void draw(void) {
    cls(CLR_BLACK);
    rectfill(0, 0, SCREEN_W, ARENA_Y, CLR_DARKER_BLUE);
    draw_timeline();

    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (wall[y][x]) rectfill(x*TILE, ARENA_Y + y*TILE, TILE, TILE, CLR_DARKER_GREY);
        else pset(x*TILE+TILE/2, ARENA_Y + y*TILE+TILE/2, CLR_DARKER_GREY);
    int cur = next_actor();
    for (int i = 0; i < NA; i++) {
        if (!act[i].alive) continue;
        int sx = act[i].x*TILE, sy = ARENA_Y + act[i].y*TILE;
        if (i == cur) rect(sx-1, sy-1, TILE+1, TILE+1, CLR_WHITE);   // whose turn it is
        print(str("%c", act[i].g), sx+1, sy, act[i].col);
    }

    // HUD: player hp + speed + a compact speed legend
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H-HUD_Y, CLR_BLACK);
    rect(4, HUD_Y+3, 42, 6, CLR_DARK_GREY);
    rectfill(5, HUD_Y+4, 40*act[0].hp/act[0].maxhp, 4, act[0].hp>6?CLR_GREEN:CLR_RED);
    print(str("HP %d", act[0].hp), 50, HUD_Y+2, CLR_WHITE);
    print(str("you: %s (spd %d)", PSPNAME[pspeed_i], act[0].speed), 96, HUD_Y+2, CLR_YELLOW);
    if (cleared)  print_right("CLEARED  R", SCREEN_W-4, HUD_Y+2, CLR_GREEN);
    else if (php_dead) print_right("DEAD  R", SCREEN_W-4, HUD_Y+2, CLR_RED);
    font(FONT_SMALL);
    if (msg_t > 0) print(msg, 4, HUD_Y+12, CLR_LIGHT_PEACH);
    else print("arrows move/bump   H re-speed yourself (watch the NEXT ribbon)   R reset   fast actors act more often",
               4, HUD_Y+12, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
