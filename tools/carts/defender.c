/* de:meta
{
  "slug": "defender",
  "title": "Defender",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "title-play-gameover-loop"
  ],
  "lineage": "Defender (1981) demake on a wrapping 3× world; the novel teaching is the three-state lander FSM (hunt → carry → mutate) with a minimap scanner strip that reads the full ring-world in 16px.",
  "genre": "shooter",
  "homage": "Defender (1981)",
  "description": "Skim a fast little ship over a wrapping mountain world far wider than the screen, while alien Landers drift down to snatch the humanoids walking the ground and haul them skyward — let one reach the top and it MUTATES into a fast, swarming red killer. Blast a carrying lander and its human plummets: dive, catch it midair, and lower it home for big points. A scanner strip across the top maps the whole ring-shaped level — ship, aliens and humans as blips — so you always know where the danger is. Clear every alien to advance the wave; lose all your humanoids and the planet is lost. Arrows fly and thrust (left/right faces + scrolls the world), Z fires forward; press Z to start and to play again."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// DEFENDER — skim a wrapping mountain world, blast Landers, save the humanoids.
//
// The world is far wider than the screen and wraps end-to-end. Landers drift
// down, grab a humanoid off the ground and haul it to the top — let one reach
// the top and it MUTATES into a fast swarming killer. Shoot a carrying lander
// and the human drops: dive, catch it, and lower it back to the ground. The
// scanner strip up top shows the whole level so you know where to fly.
//
//   Up / Down     fly
//   Left / Right  thrust + face
//   Z             fire

#define WORLD_W   960          // 3x screen, wraps
#define GROUND_Y  176
#define SKY_TOP   18           // below the scanner strip
#define SCANNER_H 16

#define MAX_LANDER 14
#define MAX_BULLET 24
#define MAX_HUMAN  8
#define MAX_BOOM   24
#define MAX_STAR   48

typedef enum { TITLE, PLAYING, DEAD, GAMEOVER, WIN } GState;

// ---- player ----
static float px, py;           // px in world coords (0..WORLD_W)
static float pvx, pvy;
static int   pface;            // +1 right, -1 left
static int   lives, score, hiscore, wave;
static int   gun_cd, invuln, respawn_t, flash;
static GState gstate;

// ---- landers / mutants ----
// state: 0 hunting (descend, pick a human), 1 carrying (rising), 2 mutant
typedef struct { float x, y, vx, vy; int st; int human; int cd; bool on; } Lander;
static Lander landers[MAX_LANDER];

// ---- bullets ----
typedef struct { float x, y, vx; int life; bool on; } Bullet;
static Bullet bullets[MAX_BULLET];

// ---- humans ----  st: 0 walking, 1 abducted(by lander), 2 falling, 3 carried(by player), 4 dead
typedef struct { float x, y, vy; int st; int by; bool on; } Human;
static Human humans[MAX_HUMAN];

// ---- explosions ----
typedef struct { float x, y; int t; int big; } Boom;
static Boom booms[MAX_BOOM];

// ---- stars ----
static float starx[MAX_STAR], stary[MAX_STAR];

// ridge heights, sampled across the world (wrapping)
#define RIDGE_N 64
static int ridge[RIDGE_N];

// ---- wrap helpers ----
static float wrapw(float x) {
    while (x < 0) x += WORLD_W;
    while (x >= WORLD_W) x -= WORLD_W;
    return x;
}
// shortest signed delta from a to b on the ring
static float wrapdelta(float a, float b) {
    float d = b - a;
    while (d >  WORLD_W/2) d -= WORLD_W;
    while (d < -WORLD_W/2) d += WORLD_W;
    return d;
}
static int ground_at(float wx) {
    wx = wrapw(wx);
    float f = wx / WORLD_W * RIDGE_N;
    int i = (int)f % RIDGE_N;
    int j = (i + 1) % RIDGE_N;
    float t = f - (int)f;
    return (int)lerp(ridge[i], ridge[j], t);
}

static int camx_of(void) {
    int c = (int)px - SCREEN_W/2;
    while (c < 0) c += WORLD_W;
    while (c >= WORLD_W) c -= WORLD_W;
    return c;
}

// ---- spawners ----
static void spawn_boom(float x, float y, int big) {
    for (int i = 0; i < MAX_BOOM; i++)
        if (booms[i].t <= 0) { booms[i] = (Boom){ x, y, big ? 22 : 12, big }; return; }
}
static void add_bullet(float x, float y, int face) {
    for (int i = 0; i < MAX_BULLET; i++)
        if (!bullets[i].on) { bullets[i] = (Bullet){ x, y, face * 6.2f, 40, true }; return; }
}

static int humans_alive(void) {
    int n = 0;
    for (int i = 0; i < MAX_HUMAN; i++) if (humans[i].on && humans[i].st != 4) n++;
    return n;
}
static int landers_alive(void) {
    int n = 0;
    for (int i = 0; i < MAX_LANDER; i++) if (landers[i].on) n++;
    return n;
}

static void spawn_lander(void) {
    for (int i = 0; i < MAX_LANDER; i++)
        if (!landers[i].on) {
            landers[i].x = rnd(WORLD_W);
            landers[i].y = SKY_TOP + rnd(40);
            landers[i].vx = rnd_float_between(-0.5f, 0.5f);
            landers[i].vy = 0;
            landers[i].st = 0; landers[i].human = -1;
            landers[i].cd = 30 + rnd(60);
            landers[i].on = true;
            return;
        }
}

static void reset_player(void) {
    px = WORLD_W/2; py = 90; pvx = pvy = 0; pface = 1;
    invuln = 90; gun_cd = 0;
}

static void start_wave(void) {
    for (int i = 0; i < MAX_LANDER; i++) landers[i].on = false;
    for (int i = 0; i < MAX_BULLET; i++) bullets[i].on = false;
    int n = min(MAX_LANDER, 3 + wave * 2);
    for (int i = 0; i < n; i++) spawn_lander();
    reset_player();
    gstate = PLAYING;
}

static void reset_game(void) {
    score = 0; lives = 3; wave = 1;
    for (int i = 0; i < MAX_BOOM; i++) booms[i].t = 0;
    // humans walk the ground, spaced out
    for (int i = 0; i < MAX_HUMAN; i++) {
        humans[i].x = (float)(i + 1) * WORLD_W / (MAX_HUMAN + 1);
        humans[i].y = 0;            // set to ground each frame
        humans[i].vy = 0;
        humans[i].st = 0; humans[i].by = -1; humans[i].on = true;
    }
    start_wave();
}

void init(void) {
    hiscore = load(0);
    // a rolling ridge line
    for (int i = 0; i < RIDGE_N; i++)
        ridge[i] = GROUND_Y - (int)(8 + 18 * (0.5f + 0.5f * sin_deg(i * 47.0f))
                                      + 10 * noise(i * 0.6f));
    for (int i = 0; i < MAX_STAR; i++) {
        starx[i] = rnd(WORLD_W);
        stary[i] = SKY_TOP + rnd(GROUND_Y - SKY_TOP - 40);
    }
    gstate = TITLE;
    reset_game();
    gstate = TITLE;
    touch_layout(TOUCH_ANALOG, 1);
}

static Human *find_free_human(float fromx) {
    Human *best = NULL; float bd = 99999;
    for (int i = 0; i < MAX_HUMAN; i++) {
        if (!humans[i].on || humans[i].st != 0) continue;
        float d = abs((int)wrapdelta(fromx, humans[i].x));
        if (d < bd) { bd = d; best = &humans[i]; }
    }
    return best;
}

void update(void) {
    if (gstate == TITLE) {
        if (btnp(0, BTN_A)) { reset_game(); }
        return;
    }
    if (gstate == GAMEOVER || gstate == WIN) {
        if (btnp(0, BTN_A)) { wave = (gstate == WIN) ? wave : 1; reset_game(); }
        return;
    }
    if (gstate == DEAD) {
        if (--respawn_t <= 0) {
            if (lives <= 0) { gstate = GAMEOVER; if (score > hiscore){ hiscore = score; save(0, hiscore);} }
            else if (humans_alive() == 0) { gstate = GAMEOVER; if (score > hiscore){ hiscore = score; save(0, hiscore);} }
            else { reset_player(); gstate = PLAYING; }
        }
        // keep world ticking visually via the shared update below — but bail
        for (int i = 0; i < MAX_BOOM; i++) if (booms[i].t > 0) booms[i].t--;
        return;
    }

    if (flash > 0) flash--;
    if (invuln > 0) invuln--;
    if (gun_cd > 0) gun_cd--;

    // ── flight ──
    float acc = 0.35f;
    if (btn(0, BTN_LEFT))  { pvx -= acc; pface = -1; }
    if (btn(0, BTN_RIGHT)) { pvx += acc; pface =  1; }
    if (btn(0, BTN_UP))    pvy -= acc;
    if (btn(0, BTN_DOWN))  pvy += acc;
    pvx *= 0.92f; pvy *= 0.88f;
    pvx = clamp(pvx, -4.2f, 4.2f);
    pvy = clamp(pvy, -3.2f, 3.2f);
    px = wrapw(px + pvx);
    py += pvy;
    if (py < SKY_TOP + 4) { py = SKY_TOP + 4; pvy = 0; }
    if (py > GROUND_Y - 6) { py = GROUND_Y - 6; pvy = 0; }

    // ── fire ──
    if (btn(0, BTN_A) && gun_cd <= 0) {
        add_bullet(px + pface * 8, py, pface);
        hit(70, INSTR_NOISE, 2, 24);
        gun_cd = 7;
    }

    // ── bullets ──
    for (int i = 0; i < MAX_BULLET; i++) {
        Bullet *b = &bullets[i];
        if (!b->on) continue;
        b->x = wrapw(b->x + b->vx);
        if (--b->life <= 0) { b->on = false; continue; }
        for (int j = 0; j < MAX_LANDER; j++) {
            Lander *l = &landers[j];
            if (!l->on) continue;
            if (abs((int)wrapdelta(b->x, l->x)) < 6 && abs((int)(b->y - l->y)) < 6) {
                // if carrying, drop the human
                if (l->st == 1 && l->human >= 0) {
                    humans[l->human].st = 2;            // falling
                    humans[l->human].vy = 0;
                    humans[l->human].by = -1;
                }
                spawn_boom(l->x, l->y, l->st == 2);
                hit(l->st == 2 ? 44 : 50, INSTR_NOISE, 4, 120);
                score += (l->st == 2) ? 150 : 100;
                l->on = false; b->on = false;
                shake(2);
                break;
            }
        }
    }

    // ── ground reference for humans ──
    for (int i = 0; i < MAX_HUMAN; i++)
        if (humans[i].on && (humans[i].st == 0))
            humans[i].y = ground_at(humans[i].x) - 6;

    // ── landers AI ──
    for (int i = 0; i < MAX_LANDER; i++) {
        Lander *l = &landers[i];
        if (!l->on) continue;

        if (l->st == 0) {                 // hunting: descend, pick a human
            l->y += 0.55f;
            l->x = wrapw(l->x + l->vx);
            if (rnd(100) < 2) l->vx = rnd_float_between(-0.6f, 0.6f);
            // near a human on the ground?
            for (int h = 0; h < MAX_HUMAN; h++) {
                if (!humans[h].on || humans[h].st != 0) continue;
                if (abs((int)wrapdelta(l->x, humans[h].x)) < 8 &&
                    l->y > humans[h].y - 14) {
                    l->st = 1; l->human = h;
                    humans[h].st = 1; humans[h].by = i;
                    note(60, INSTR_SAW, 3);
                    break;
                }
            }
            if (l->y > ground_at(l->x) - 8) l->y = ground_at(l->x) - 8;

        } else if (l->st == 1) {          // carrying: rise to the top
            l->y -= 0.7f;
            l->x = wrapw(l->x + l->vx * 0.5f);
            if (l->human >= 0) { humans[l->human].x = l->x; humans[l->human].y = l->y + 8; }
            if (l->y <= SKY_TOP + 2) {     // mutate!
                l->st = 2; l->y = SKY_TOP + 2;
                if (l->human >= 0) { humans[l->human].st = 4; humans[l->human].on = false; l->human = -1; }
                hit(40, INSTR_SAW, 5, 260);
                flash = 6; shake(2);
            }

        } else {                          // mutant: fast chase
            float aimx = px, aimy = py;
            float dxw = wrapdelta(l->x, aimx);
            l->vx = clamp(l->vx + sgn((int)dxw) * 0.12f, -2.6f, 2.6f);
            l->vy = clamp(l->vy + sgn((int)(aimy - l->y)) * 0.10f, -2.2f, 2.2f);
            if (rnd(100) < 4) l->vy += rnd_float_between(-1.0f, 1.0f);
            l->x = wrapw(l->x + l->vx);
            l->y = clamp(l->y + l->vy, SKY_TOP + 2, GROUND_Y - 4);
        }
    }

    // ── falling humans + player catch / ground landing ──
    for (int i = 0; i < MAX_HUMAN; i++) {
        Human *h = &humans[i];
        if (!h->on) continue;

        if (h->st == 2) {                 // falling
            h->vy += 0.10f;
            h->y += h->vy;
            // caught by player?
            if (abs((int)wrapdelta(h->x, px)) < 8 && abs((int)(h->y - py)) < 9) {
                h->st = 3; h->by = -1;
                note(76, INSTR_SINE, 5);
                score += 250;
            } else if (h->y >= ground_at(h->x) - 6) {
                // hit the ground from height: survives if fall was short, else splat
                h->y = ground_at(h->x) - 6;
                h->st = 0; h->vy = 0;
            }
        } else if (h->st == 3) {          // carried by player: follow under ship
            h->x = px; h->y = py + 9;
            if (py >= GROUND_Y - 10) {     // low enough — drop home
                h->st = 0; h->y = ground_at(h->x) - 6; h->vy = 0;
                note(84, INSTR_SINE, 5);
                score += 250;
            }
        } else if (h->st == 0) {          // walking
            if (rnd(100) < 3) h->vy = 0;  // (reuse vy as a tiny step latch — keep simple)
            h->x = wrapw(h->x + (frame() % 90 < 45 ? 0.12f : -0.12f) * ((i % 2) ? 1 : -1));
            h->y = ground_at(h->x) - 6;
        }
    }

    // ── player collisions ──
    if (invuln <= 0) {
        bool dead = false;
        for (int i = 0; i < MAX_LANDER && !dead; i++) {
            Lander *l = &landers[i];
            if (!l->on) continue;
            int r = (l->st == 2) ? 7 : 6;
            if (abs((int)wrapdelta(l->x, px)) < r && abs((int)(l->y - py)) < r) {
                spawn_boom(l->x, l->y, 1);
                l->on = false;            // ram trades a kill
                dead = true;
            }
        }
        if (dead) {
            spawn_boom(px, py, 1);
            hit(34, INSTR_NOISE, 7, 360);
            shake(6); flash = 8;
            lives--;
            gstate = DEAD; respawn_t = 70;
        }
    }

    // ── explosions tick ──
    for (int i = 0; i < MAX_BOOM; i++) if (booms[i].t > 0) booms[i].t--;

    // ── wave / lose ──
    if (gstate == PLAYING) {
        if (landers_alive() == 0) {
            wave++;
            if (score > hiscore) { hiscore = score; save(0, hiscore); }
            note(72, INSTR_SINE, 5);
            start_wave();
        }
        if (humans_alive() == 0) {
            gstate = GAMEOVER;
            if (score > hiscore) { hiscore = score; save(0, hiscore); }
        }
    }
}

// ============ DRAW ============

// world x -> screen x given camera, accounting for wrap (returns -999 if off-screen)
static int sx_of(float wx, int camx) {
    float d = wx - camx;
    while (d < -WORLD_W/2) d += WORLD_W;
    while (d >  WORLD_W/2) d -= WORLD_W;
    int s = (int)d;
    if (s < -24 || s > SCREEN_W + 24) return -999;
    return s;
}

static void draw_ship(int x, int y, int face, int col) {
    if (face > 0) {
        trifill(x - 6, y - 3, x - 6, y + 3, x + 7, y, col);
        rectfill(x - 7, y - 1, 4, 3, col);
    } else {
        trifill(x + 6, y - 3, x + 6, y + 3, x - 7, y, col);
        rectfill(x + 3, y - 1, 4, 3, col);
    }
    pset(x + face * 6, y, CLR_WHITE);
}

static void draw_lander(int x, int y, int st) {
    if (st == 2) {                      // mutant — jagged red
        int c = blink(4) ? CLR_RED : CLR_DARK_ORANGE;
        line(x-4, y, x+4, y, c);
        line(x, y-4, x, y+4, c);
        line(x-3, y-3, x+3, y+3, c);
        line(x+3, y-3, x-3, y+3, c);
        pset(x, y, CLR_YELLOW);
    } else {                            // lander — domed crab
        int body = (st == 1) ? CLR_PINK : CLR_GREEN;
        rectfill(x-3, y-1, 7, 3, body);
        line(x-3, y-2, x+3, y-2, CLR_WHITE);
        line(x-4, y+2, x-5, y+4, body);
        line(x+4, y+2, x+5, y+4, body);
        pset(x, y, CLR_YELLOW);
    }
}

static void draw_human(int x, int y, int col) {
    circfill(x, y - 3, 1, col);
    line(x, y - 2, x, y + 1, col);
    line(x, y + 1, x - 1, y + 4, col);
    line(x, y + 1, x + 1, y + 4, col);
}

void draw(void) {
    cls(CLR_BLACK);

    if (gstate == TITLE) {
        for (int i = 0; i < MAX_STAR; i++) pset((int)starx[i] % SCREEN_W, (int)stary[i], CLR_DARK_GREY);
        print_centered("D E F E N D E R", SCREEN_W/2, 70, CLR_GREEN);
        print_centered("save the humanoids from the landers", SCREEN_W/2, 92, CLR_LIGHT_GREY);
        print_centered("arrows fly + thrust    Z fire", SCREEN_W/2, 108, CLR_WHITE);
        if (blink(20)) print_centered("press Z to begin", SCREEN_W/2, 128, CLR_YELLOW);
        print_right(str("BEST %d", hiscore), SCREEN_W - 4, 4, CLR_YELLOW);
        return;
    }

    int camx = camx_of();

    // hit-flash whole screen
    if (flash > 0) pal(CLR_BLACK, CLR_DARK_RED);

    // starfield (parallax-ish: half camera speed)
    for (int i = 0; i < MAX_STAR; i++) {
        int s = sx_of(wrapw(starx[i] - camx * 0.5f) + camx, camx);
        if (s != -999) pset(s, (int)stary[i], CLR_DARK_GREY);
    }

    // ── ground ridge ── draw across the visible span
    int prevx = -1, prevy = 0;
    for (int s = -2; s <= SCREEN_W + 2; s += 4) {
        float wx = wrapw(camx + s);
        int gy = ground_at(wx);
        if (prevx >= 0) {
            line(prevx, prevy, s, gy, CLR_DARK_GREEN);
            line(prevx, prevy + 1, s, gy + 1, CLR_DARK_GREEN);
            rectfill(s, gy + 1, 4, SCREEN_H - gy, CLR_BROWNISH_BLACK);
        }
        prevx = s; prevy = gy;
    }

    // ── humans on the ground / falling / carried ──
    for (int i = 0; i < MAX_HUMAN; i++) {
        if (!humans[i].on) continue;
        int s = sx_of(humans[i].x, camx);
        if (s == -999) continue;
        int col = (humans[i].st == 2) ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH;
        draw_human(s, (int)humans[i].y, col);
        if (humans[i].st == 2 && blink(6)) circ(s, (int)humans[i].y - 2, 4, CLR_YELLOW);
    }

    // ── bullets ──
    for (int i = 0; i < MAX_BULLET; i++) {
        if (!bullets[i].on) continue;
        int s = sx_of(bullets[i].x, camx);
        if (s == -999) continue;
        rectfill(s - 2, (int)bullets[i].y, 5, 1, CLR_WHITE);
    }

    // ── landers ──
    for (int i = 0; i < MAX_LANDER; i++) {
        if (!landers[i].on) continue;
        int s = sx_of(landers[i].x, camx);
        if (s == -999) continue;
        if (landers[i].st == 1) {            // abduction tractor beam
            int hy = (int)landers[i].y + 14;
            for (int yy = (int)landers[i].y + 2; yy < hy; yy += 2)
                pset(s, yy, blink(2) ? CLR_PINK : CLR_DARK_PURPLE);
        }
        draw_lander(s, (int)landers[i].y, landers[i].st);
    }

    // ── explosions ──
    for (int i = 0; i < MAX_BOOM; i++) {
        if (booms[i].t <= 0) continue;
        int s = sx_of(booms[i].x, camx);
        if (s == -999) continue;
        int max_t = booms[i].big ? 22 : 12;
        int r = (max_t - booms[i].t) / 2 + 1;
        circfill(s, (int)booms[i].y, r, booms[i].t > max_t/2 ? CLR_LIGHT_YELLOW : CLR_ORANGE);
    }

    // ── player ── (always at world px; screen pos via sx_of)
    if (gstate == PLAYING) {
        int s = sx_of(px, camx);
        bool show = invuln <= 0 || frame() % 6 < 3;
        if (s != -999 && show) draw_ship(s, (int)py, pface, CLR_BLUE);
    }

    if (flash > 0) pal_reset();

    // ── SCANNER strip ──
    rectfill(0, 0, SCREEN_W, SCANNER_H, CLR_DARKER_BLUE);
    line(0, SCANNER_H, SCREEN_W, SCANNER_H, CLR_DARK_BLUE);
    // mini ridge
    for (int s = 0; s < SCREEN_W; s += 3) {
        float wx = (float)s / SCREEN_W * WORLD_W;
        int gy = ground_at(wx);
        int my = 2 + (int)remap(gy, GROUND_Y - 40, GROUND_Y, 1, SCANNER_H - 2);
        pset(s, my, CLR_BLUE_GREEN);
    }
    // camera window box
    {
        int a = (int)((float)camx / WORLD_W * SCREEN_W);
        int w = (int)((float)SCREEN_W / WORLD_W * SCREEN_W);
        rect(a, 1, w, SCANNER_H - 2, CLR_LIGHT_GREY);
        if (a + w > SCREEN_W) rect(a - SCREEN_W, 1, w, SCANNER_H - 2, CLR_LIGHT_GREY);
    }
    // blips
    for (int i = 0; i < MAX_HUMAN; i++)
        if (humans[i].on && humans[i].st != 2) {
            int bx = (int)(humans[i].x / WORLD_W * SCREEN_W);
            pset(bx, SCANNER_H - 3, CLR_LIGHT_PEACH);
        }
    for (int i = 0; i < MAX_LANDER; i++)
        if (landers[i].on) {
            int bx = (int)(landers[i].x / WORLD_W * SCREEN_W);
            int by = 3 + (int)((landers[i].y - SKY_TOP) / (GROUND_Y - SKY_TOP) * (SCANNER_H - 5));
            pset(bx, by, landers[i].st == 2 ? CLR_RED : CLR_GREEN);
        }
    {
        int bx = (int)(px / WORLD_W * SCREEN_W);
        pset(bx, 8, CLR_WHITE); pset(bx - 1, 8, CLR_WHITE); pset(bx + 1, 8, CLR_WHITE);
    }

    // ── HUD ──
    print(str("SCORE %d", score), 2, SCANNER_H + 2, CLR_WHITE);
    print_right(str("WAVE %d", wave), SCREEN_W - 2, SCANNER_H + 2, CLR_YELLOW);
    // lives + humans count bottom
    print(str("SHIPS %d", lives), 2, SCREEN_H - 9, CLR_BLUE);
    print_right(str("HUMANS %d", humans_alive()), SCREEN_W - 2, SCREEN_H - 9, CLR_LIGHT_PEACH);

    if (gstate == GAMEOVER) {
        rectfill(SCREEN_W/2 - 70, SCREEN_H/2 - 22, 140, 46, CLR_BLACK);
        rect    (SCREEN_W/2 - 70, SCREEN_H/2 - 22, 140, 46, CLR_RED);
        print_centered(humans_alive() == 0 ? "PLANET LOST" : "GAME OVER", SCREEN_W/2, SCREEN_H/2 - 12, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2 + 12, CLR_LIGHT_GREY);
    }
}
