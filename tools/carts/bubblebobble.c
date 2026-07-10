/* de:meta
{
  "slug": "bubblebobble",
  "title": "bubble bobble",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "state-machine"
  ],
  "lineage": "Tribute to Taito's Bubble Bobble (1986); monsters cycle through a FREE/BUBBLED/FRUIT FSM that mirrors the original's trap-and-pop loop.",
  "genre": "platformer",
  "homage": "Bubble Bobble (1986)",
  "description": "A tribute to Bubble Bobble. You are Bub, the bubble dragon: run and jump across the platforms, press B to blow a bubble that drifts up and traps a monster, then jump into the trapped monster to POP it into fruit — grab the fruit for points. Clear every monster to advance to the next, tougher wave. Don’t touch a free monster or you lose one of your three lives. LEFT/RIGHT run, A jumps, B blows a bubble.",
  "todo": [
    "When a captured enemy in a bubble floats to the top, there's no way to catch it — the original had a way back into the game."
  ]
}
de:meta */
#include "studio.h"

// ── BUBBLE DRAGON — a Bubble Bobble tribute ───────────────────
// You are Bub, the bubble dragon. Trap every monster in a bubble,
// then pop the bubble to turn it into fruit. Clear the screen to
// move to the next, tougher wave.
//
//   • LEFT / RIGHT — run
//   • A            — jump
//   • B            — blow a bubble (it drifts up and traps a monster)
//   • jump into a trapped monster to POP it, then grab the fruit
//
// Don't touch a free monster — you'll lose one of your three lives.

#define HUD_H 11

typedef struct { int x, y, w; } Plat;
static const Plat plats[] = {
    { 8, 182, 304 },                          // floor
    { 8, 150, 88 }, { 224, 150, 88 },         // lower sides
    { 74, 118, 80 }, { 166, 118, 80 },        // middle
    { 8, 84, 60 }, { 252, 84, 60 },           // upper sides
    { 110, 84, 100 },                         // top centre
};
#define NPLAT ((int)(sizeof(plats) / sizeof(plats[0])))

// ── player ──
static float px, py, pvx, pvy;
static int   pface = 1;
static bool  pground;
static int   inv;                             // invulnerability frames after a hit

// ── bubbles (blown by Bub, empty) ──
#define MAXB 10
typedef struct { float x, y, vx, vy; int life; bool on; } Bubble;
static Bubble bub[MAXB];
static int    shoot_cd;

// ── monsters ──
#define MAXE 16
enum { E_OFF, E_FREE, E_BUBBLED };
typedef struct { float x, y, vx, vy; int state, p0, p1, life, col; } Mon;
static Mon mon[MAXE];

// ── fruit ──
#define MAXF 16
typedef struct { float x, y, vy; int life; bool on; } Fruit;
static Fruit fruit[MAXF];

static int  score, lives, level, level_flash;
static bool gameover;
static bool ready = false;

// ── platform helpers ──────────────────────────────────────────
// landing: returns platform top y if a faller at (x, yprev→y) lands, else -1
static int land_y(float x, float yprev, float y) {
    for (int i = 0; i < NPLAT; i++) {
        const Plat *p = &plats[i];
        if (x >= p->x && x <= p->x + p->w && yprev <= p->y + 1 && y >= p->y)
            return p->y;
    }
    return -1;
}

static void spawn_wave(void) {
    int n = 3 + level;
    if (n > MAXE) n = MAXE;
    int cols[4] = { CLR_RED, CLR_PINK, CLR_ORANGE, CLR_LIME_GREEN };
    for (int i = 0; i < MAXE; i++) mon[i].state = E_OFF;
    for (int i = 0; i < n; i++) {
        const Plat *p = &plats[1 + (i % (NPLAT - 1))];   // skip the floor
        mon[i].state = E_FREE;
        mon[i].p0 = p->x + 6; mon[i].p1 = p->x + p->w - 6;
        mon[i].x = p->x + 8 + (i * 13) % (p->w - 16);
        mon[i].y = p->y;
        mon[i].vx = (i & 1) ? 0.7f : -0.7f;
        mon[i].vy = 0;
        mon[i].col = cols[i % 4];
    }
    level_flash = 90;
}

static void respawn_player(void) {
    px = 160; py = 182; pvx = pvy = 0; pground = true; pface = 1; inv = 90;
}

static void reset_game(void) {
    score = 0; lives = 3; level = 0; gameover = false;
    for (int i = 0; i < MAXB; i++) bub[i].on = false;
    for (int i = 0; i < MAXF; i++) fruit[i].on = false;
    spawn_wave();
    respawn_player();
    ready = true;
}

static int alive_monsters(void) {
    int n = 0;
    for (int i = 0; i < MAXE; i++) if (mon[i].state != E_OFF) n++;
    return n;
}

static void drop_fruit(float x, float y) {
    for (int i = 0; i < MAXF; i++)
        if (!fruit[i].on) { fruit[i] = (Fruit){ x, y, 0, 360, true }; return; }
}

// ── update ────────────────────────────────────────────────────
void update(void) {
    if (!ready) reset_game();

    if (gameover) { if (btnp(0, BTN_A)) reset_game(); return; }
    if (level_flash > 0) level_flash--;
    if (inv > 0) inv--;
    if (shoot_cd > 0) shoot_cd--;

    // ── player movement ──
    if (btn(0, BTN_LEFT))  { pvx = -1.6f; pface = -1; }
    else if (btn(0, BTN_RIGHT)) { pvx = 1.6f; pface = 1; }
    else pvx = 0;
    if (btnp(0, BTN_A) && pground) { pvy = -4.6f; pground = false; note(72, INSTR_SQUARE, 3); }

    float ppy = py;
    pvy += 0.25f; py += pvy; px += pvx;
    px = clamp(px, 10, 310);
    pground = false;
    if (pvy >= 0) {
        int ly = land_y(px, ppy, py);
        if (ly >= 0) { py = ly; pvy = 0; pground = true; }
    }
    if (py > SCREEN_H + 8) { py = HUD_H + 2; pvy = 0; }    // fall off bottom → reappear up top

    // ── shoot a bubble ──
    if (btnp(0, BTN_B) && shoot_cd == 0) {
        for (int i = 0; i < MAXB; i++) if (!bub[i].on) {
            bub[i] = (Bubble){ px + pface * 8, py - 8, pface * 3.0f, 0, 360, true };
            shoot_cd = 12; note(84, INSTR_SINE, 2);
            break;
        }
    }

    // ── bubbles ──
    for (int i = 0; i < MAXB; i++) {
        if (!bub[i].on) continue;
        bub[i].life--;
        if (bub[i].life > 330) { bub[i].x += bub[i].vx; }          // shoot phase
        else { bub[i].y -= 0.55f; bub[i].x += sin_deg(bub[i].life * 6) * 0.4f; }  // float up
        if (bub[i].y < HUD_H + 6 || bub[i].life <= 0) { bub[i].on = false; continue; }
        // capture a free monster
        for (int e = 0; e < MAXE; e++) {
            if (mon[e].state != E_FREE) continue;
            if (near((int)bub[i].x, (int)bub[i].y, (int)mon[e].x, (int)mon[e].y - 6, 10)) {
                mon[e].state = E_BUBBLED; mon[e].life = 480; mon[e].vy = -0.5f;
                mon[e].y -= 6; bub[i].on = false; note(60, INSTR_SINE, 3);
                break;
            }
        }
    }

    // ── monsters ──
    for (int e = 0; e < MAXE; e++) {
        Mon *m = &mon[e];
        if (m->state == E_FREE) {
            m->x += m->vx;
            if (m->x < m->p0) { m->x = m->p0; m->vx = -m->vx; }
            if (m->x > m->p1) { m->x = m->p1; m->vx = -m->vx; }
            if (inv == 0 && near((int)px, (int)py - 6, (int)m->x, (int)m->y - 6, 10)) {
                lives--; note(36, INSTR_SAW, 5);
                if (lives < 0) { gameover = true; return; }
                respawn_player();
            }
        } else if (m->state == E_BUBBLED) {
            m->life--;
            m->y += m->vy;
            m->x += sin_deg(m->life * 5) * 0.3f;
            if (m->y < HUD_H + 12) m->y = HUD_H + 12;               // pinned at the ceiling
            if (m->life <= 0) { m->state = E_FREE; m->vx = 0.7f; }  // escaped!
            else if (near((int)px, (int)py - 6, (int)m->x, (int)m->y, 11)) {       // POP
                m->state = E_OFF; score += 100; drop_fruit(m->x, m->y);
                strum(72, CHORD_MAJ, INSTR_TRI, 4, 30);
            }
        }
    }

    // ── fruit ──
    for (int i = 0; i < MAXF; i++) {
        if (!fruit[i].on) continue;
        float fy = fruit[i].y;
        fruit[i].vy += 0.25f; fruit[i].y += fruit[i].vy;
        int ly = land_y(fruit[i].x, fy, fruit[i].y);
        if (fruit[i].vy >= 0 && ly >= 0) { fruit[i].y = ly; fruit[i].vy = 0; }
        if (--fruit[i].life <= 0) { fruit[i].on = false; continue; }
        if (near((int)px, (int)py - 6, (int)fruit[i].x, (int)fruit[i].y - 3, 10)) {
            fruit[i].on = false; score += 500; note(96, INSTR_SINE, 3);
        }
    }

    if (alive_monsters() == 0) { level++; spawn_wave(); }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_bub(int x, int y, int dir, int col) {
    circfill(x, y - 6, 6, col);                       // body
    circfill(x + dir * 2, y - 8, 2, CLR_WHITE);       // eye white
    pset(x + dir * 3, y - 8, CLR_BLACK);              // pupil
    trifill(x - 4, y, x, y - 3, x - 1, y, CLR_DARK_GREEN);   // little feet
    trifill(x + 4, y, x, y - 3, x + 1, y, CLR_DARK_GREEN);
    trifill(x - dir, y - 11, x + dir * 4, y - 13, x + dir * 2, y - 9, col); // spine/horn
}

static void draw_monster(int x, int y, int col) {
    circfill(x, y - 6, 6, col);
    rectfill(x - 4, y - 13, 8, 3, col);               // mohawk base
    for (int i = -1; i <= 1; i++) trifill(x + i * 3 - 1, y - 12, x + i * 3 + 1, y - 12, x + i * 3, y - 16, col);
    circfill(x - 2, y - 7, 1, CLR_WHITE); circfill(x + 2, y - 7, 1, CLR_WHITE);
    pset(x - 2, y - 7, CLR_BLACK); pset(x + 2, y - 7, CLR_BLACK);
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_BLACK);
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_DARKER_BLUE);

    // platforms — chunky blue blocks
    for (int i = 0; i < NPLAT; i++) {
        const Plat *p = &plats[i];
        rectfill(p->x, p->y, p->w, 6, CLR_TRUE_BLUE);
        rectfill(p->x, p->y, p->w, 2, CLR_BLUE);
        rect(p->x, p->y, p->w, 6, CLR_DARK_BLUE);
    }

    // fruit
    for (int i = 0; i < MAXF; i++) if (fruit[i].on) {
        int x = (int)fruit[i].x, y = (int)fruit[i].y;
        circfill(x - 1, y - 2, 2, CLR_RED); circfill(x + 2, y - 2, 2, CLR_RED);
        line(x, y - 3, x + 1, y - 6, CLR_DARK_GREEN);
    }

    // monsters + bubbled monsters
    for (int e = 0; e < MAXE; e++) {
        Mon *m = &mon[e];
        if (m->state == E_FREE) draw_monster((int)m->x, (int)m->y, m->col);
        else if (m->state == E_BUBBLED) {
            circ((int)m->x, (int)m->y, 9, CLR_LIGHT_PEACH);     // bubble shell
            circfill((int)m->x, (int)m->y, 4, m->col);          // trapped monster
            pset((int)m->x - 3, (int)m->y - 3, CLR_WHITE);      // shine
        }
    }

    // empty bubbles
    for (int i = 0; i < MAXB; i++) if (bub[i].on) {
        circ((int)bub[i].x, (int)bub[i].y, 7, CLR_LIGHT_PEACH);
        pset((int)bub[i].x - 2, (int)bub[i].y - 3, CLR_WHITE);
    }

    // Bub (flashes while invulnerable)
    if (inv <= 0 || blink(4)) draw_bub((int)px, (int)py, pface, CLR_GREEN);

    // HUD
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    print(str("SCORE %d", score), 4, 2, CLR_YELLOW);
    print_centered(str("LEVEL %d", level + 1), SCREEN_W/2, 2, CLR_WHITE);
    for (int i = 0; i < lives; i++) draw_bub(SCREEN_W - 10 - i * 12, 10, 1, CLR_GREEN);

    if (level_flash > 0)
        print_centered(str("WAVE %d  -  READY!", level + 1), SCREEN_W/2, 90, CLR_LIGHT_YELLOW);
    if (gameover) {
        int w = 200, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 80, w, 34, CLR_DARK_PURPLE);
        rect(bx, 80, w, 34, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, 88, CLR_RED);
        print_centered(str("score %d  -  press A", score), SCREEN_W/2, 100, CLR_YELLOW);
    }
}
