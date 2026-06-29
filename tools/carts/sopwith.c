/* de:meta
{
  "title": "sopwith camel",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "finite-state-ai"
  ],
  "lineage": "Homage to Sopwith (1984); the flight model (thrust along nose angle, gravity, drag — Euler) and the enemy angle_to steering chase are the two technical ideas; everything else is standard shooter structure.",
  "genre": "shooter",
  "homage": "Sopwith (1984)",
  "description": "A WWI biplane bomber with a real flight model — the engine pushes you forward along the nose, gravity always pulls down, drag bleeds speed, so you stall if you climb too steeply and loop if you hold up. Bomb every enemy hangar and dogfight the red planes. W/S pitch the nose, Z machine gun, X drop a bomb."
}
de:meta */
#include "studio.h"

// SOPWITH CAMEL — a WWI biplane bomber.
//
// The heart of it is a real flight model: the engine always pushes the plane
// forward along its nose, gravity always pulls down, and drag bleeds speed.
// Pull the nose up and gravity fights you — climb too steeply and you stall
// and fall. Point down and you dive faster. Hold up through the top and you
// loop. No "move left/right" — you fly an aeroplane.
//
// Mission: bomb every enemy hangar on the ground. Dogfight the red planes
// for points. Don't fly into the dirt.
//
//   W / S   pitch the nose up / down
//   Z       machine gun
//   X       drop a bomb

#define WORLD_W   1280
#define GROUND_Y  182
#define THRUST    0.32f    // higher thrust + lower drag = velocity snaps to the
#define GRAVITY   0.080f    // nose faster, killing the floaty glide
#define DRAG      0.940f
#define TURN      4.5f      // pitch rate — how fast the nose comes up

#define MAX_BULLETS 48
#define MAX_BOMBS   10
#define MAX_TARGETS 7
#define MAX_ENEMIES 3
#define MAX_BOOM    20

typedef enum { PLAYING, GAMEOVER, WIN } GState;

// ---- player ----
static float px, py, vx, vy, pa;
static int   lives, score;
static int   invuln, respawn;
static int   gun_cd, bomb_cd;
static GState gstate;

// ---- entities ----
typedef struct { float x, y, vx, vy; int life, owner; bool on; } Bullet; // owner 0=player 1=enemy
typedef struct { float x, y, vx, vy; bool on; } Bomb;
typedef struct { float x; bool alive; } Target;
typedef struct { float x, y, pa; int shoot_cd, respawn; bool alive; } Enemy;
typedef struct { float x, y; int t; } Boom;

static Bullet bullets[MAX_BULLETS];
static Bomb   bombs[MAX_BOMBS];
static Target targets[MAX_TARGETS];
static Enemy  enemies[MAX_ENEMIES];
static Boom   booms[MAX_BOOM];

// ---- helpers ----
static void spawn_boom(float x, float y) {
    for (int i = 0; i < MAX_BOOM; i++)
        if (booms[i].t <= 0) { booms[i] = (Boom){ x, y, 18 }; return; }
}

static void add_bullet(float x, float y, float vx, float vy, int owner) {
    for (int i = 0; i < MAX_BULLETS; i++)
        if (!bullets[i].on) { bullets[i] = (Bullet){ x, y, vx, vy, 45, owner, true }; return; }
}

static void reset_plane(void) {
    px = 140; py = 110; pa = 0; vx = 2.6f; vy = 0;
    invuln = 90;
}

static void crash_plane(void) {
    spawn_boom(px, py);
    hit(38, INSTR_NOISE, 6, 220);
    lives--;
    if (lives <= 0) gstate = GAMEOVER;
    else reset_plane();
}

static int targets_left(void) {
    int n = 0;
    for (int i = 0; i < MAX_TARGETS; i++) if (targets[i].alive) n++;
    return n;
}

static void spawn_enemy(Enemy *e) {
    e->x = rnd_between(200, WORLD_W - 200);
    e->y = rnd_between(30, 90);
    e->pa = chance(50) ? 0 : 180;
    e->shoot_cd = rnd_between(60, 160);
    e->respawn = 0;
    e->alive = true;
}

static void reset_game(void) {
    gstate = PLAYING;
    lives = 3; score = 0;
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].on = false;
    for (int i = 0; i < MAX_BOMBS; i++)   bombs[i].on = false;
    for (int i = 0; i < MAX_BOOM; i++)    booms[i].t = 0;
    for (int i = 0; i < MAX_TARGETS; i++)
        targets[i] = (Target){ 220.0f + i * 150.0f, true };
    for (int i = 0; i < MAX_ENEMIES; i++) spawn_enemy(&enemies[i]);
    reset_plane();
}

void init(void) { reset_game(); }

void update(void) {
    if (gstate != PLAYING) {
        if (btnp(0, BTN_A)) reset_game();
        return;
    }

    if (invuln > 0) invuln--;
    if (gun_cd  > 0) gun_cd--;
    if (bomb_cd > 0) bomb_cd--;

    // ── flight: pitch, thrust along nose, gravity, drag ──
    if (btn(0, BTN_UP))   pa -= TURN;   // pull back — nose climbs
    if (btn(0, BTN_DOWN)) pa += TURN;   // push      — nose drops
    vx += dx(THRUST, pa);
    vy += dy(THRUST, pa);
    vy += GRAVITY;
    vx *= DRAG;
    vy *= DRAG;
    px += vx;
    py += vy;

    // world bounds: clamp the sky ceiling and the world edges
    if (py < 6) { py = 6; vy = 0; }
    if (px < 12) { px = 12; vx *= -0.4f; }
    if (px > WORLD_W - 12) { px = WORLD_W - 12; vx *= -0.4f; }

    // ── weapons ──
    if (btn(0, BTN_A) && gun_cd <= 0) {
        add_bullet(px + dx(12, pa), py + dy(12, pa),
                   vx + dx(7, pa), vy + dy(7, pa), 0);
        hit(72, INSTR_NOISE, 2, 22);
        gun_cd = 6;
    }
    if (btnp(0, BTN_B) && bomb_cd <= 0) {
        for (int i = 0; i < MAX_BOMBS; i++)
            if (!bombs[i].on) {
                bombs[i] = (Bomb){ px, py, vx * 0.6f, vy * 0.6f, true };
                note(45, INSTR_SQUARE, 3);
                bomb_cd = 18;
                break;
            }
    }

    // crash into the ground?
    if (invuln <= 0 && py >= GROUND_Y - 3) crash_plane();

    // ── bullets ──
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (!b->on) continue;
        b->x += b->vx; b->y += b->vy;
        if (--b->life <= 0 || b->y >= GROUND_Y || b->x < 0 || b->x > WORLD_W) { b->on = false; continue; }
        if (b->owner == 0) {
            for (int e = 0; e < MAX_ENEMIES; e++)
                if (enemies[e].alive && near((int)b->x, (int)b->y, (int)enemies[e].x, (int)enemies[e].y, 9)) {
                    enemies[e].alive = false; enemies[e].respawn = 180;
                    spawn_boom(enemies[e].x, enemies[e].y);
                    hit(40, INSTR_NOISE, 5, 160);
                    score += 250; b->on = false; break;
                }
        } else if (invuln <= 0 && near((int)b->x, (int)b->y, (int)px, (int)py, 8)) {
            b->on = false; crash_plane();
        }
    }

    // ── bombs (gravity, hit ground / hangars) ──
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb *b = &bombs[i];
        if (!b->on) continue;
        b->vy += GRAVITY * 2.2f;
        b->x += b->vx; b->y += b->vy;
        if (b->y >= GROUND_Y) {
            spawn_boom(b->x, GROUND_Y - 6);
            hit(36, INSTR_NOISE, 5, 180);
            for (int t = 0; t < MAX_TARGETS; t++)
                if (targets[t].alive && near((int)b->x, GROUND_Y, (int)targets[t].x, GROUND_Y, 16)) {
                    targets[t].alive = false; score += 100;
                }
            b->on = false;
            if (targets_left() == 0) gstate = WIN;
        }
    }

    // ── enemy planes: drift, loosely chase, take pot-shots ──
    for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &enemies[e];
        if (!en->alive) { if (--en->respawn <= 0) spawn_enemy(en); continue; }
        // steer gently toward the player
        float want = angle_to((int)en->x, (int)en->y, (int)px, (int)py);
        float diff = want - en->pa;
        while (diff > 180) diff -= 360;
        while (diff < -180) diff += 360;
        en->pa += clamp(diff, -1.2f, 1.2f);
        en->x += dx(1.7f, en->pa);
        en->y += dy(1.7f, en->pa);
        en->y = clamp(en->y, 20, GROUND_Y - 30);
        if (en->x < 20 || en->x > WORLD_W - 20) en->pa += 180;
        if (--en->shoot_cd <= 0 && near((int)en->x, (int)en->y, (int)px, (int)py, 170)) {
            float aim = angle_to((int)en->x, (int)en->y, (int)px, (int)py);
            add_bullet(en->x, en->y, dx(3.2f, aim), dy(3.2f, aim), 1);
            en->shoot_cd = rnd_between(70, 150);
        }
        // ram the player
        if (invuln <= 0 && near((int)en->x, (int)en->y, (int)px, (int)py, 11)) {
            en->alive = false; en->respawn = 180;
            spawn_boom(en->x, en->y);
            crash_plane();
        }
    }

    for (int i = 0; i < MAX_BOOM; i++) if (booms[i].t > 0) booms[i].t--;
}

// draw a biplane centered at (cx,cy) facing angle a (vector style)
static void draw_plane(float cx, float cy, float a, int body, int wing) {
    float ca = cos_deg(a), sa = sin_deg(a);
    #define WX(lx,ly) ((int)(cx + (lx)*ca - (ly)*sa))
    #define WY(lx,ly) ((int)(cy + (lx)*sa + (ly)*ca))
    trifill(WX(12,0),WY(12,0), WX(-9,-3),WY(-9,-3), WX(-9,3),WY(-9,3), body); // fuselage
    line(WX(-3,-6),WY(-3,-6), WX(5,-6),WY(5,-6), wing);   // top wing
    line(WX(-3, 6),WY(-3, 6), WX(5, 6),WY(5, 6), wing);   // bottom wing
    line(WX(1,-6),WY(1,-6),  WX(1,6),WY(1,6),   wing);    // strut
    line(WX(-9,0),WY(-9,0),  WX(-12,-5),WY(-12,-5), body);// tail fin
    line(WX(-9,-3),WY(-9,-3),WX(-9,3),WY(-9,3),  wing);   // tailplane
    #undef WX
    #undef WY
}

void draw(void) {
    cls(CLR_BLUE);  // sky

    int camx = (int)clamp(px - SCREEN_W / 2.0f, 0, WORLD_W - SCREEN_W);
    camera(camx, 0);

    // distant clouds
    for (int i = 0; i < 9; i++) {
        int cx = 90 + i * 150, cy = 26 + (i % 3) * 14;
        circfill(cx, cy, 9, CLR_WHITE);
        circfill(cx + 10, cy + 2, 7, CLR_WHITE);
        circfill(cx - 9, cy + 2, 6, CLR_WHITE);
    }

    // ground
    rectfill(0, GROUND_Y, WORLD_W, SCREEN_H - GROUND_Y, CLR_DARK_GREEN);
    line(0, GROUND_Y, WORLD_W, GROUND_Y, CLR_GREEN);

    // hangars (targets) — scorched stump once bombed
    for (int i = 0; i < MAX_TARGETS; i++) {
        int tx = (int)targets[i].x;
        if (targets[i].alive) {
            rectfill(tx - 8, GROUND_Y - 9, 16, 9, CLR_DARK_GREY);
            trifill(tx - 9, GROUND_Y - 9, tx + 9, GROUND_Y - 9, tx, GROUND_Y - 17, CLR_RED);
            rectfill(tx - 2, GROUND_Y - 6, 4, 6, CLR_BROWNISH_BLACK);
        } else {
            rectfill(tx - 7, GROUND_Y - 2, 14, 2, CLR_BROWNISH_BLACK);
        }
    }

    // bombs
    for (int i = 0; i < MAX_BOMBS; i++)
        if (bombs[i].on) circfill((int)bombs[i].x, (int)bombs[i].y, 2, CLR_LIGHT_YELLOW);

    // bullets
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].on)
            pset((int)bullets[i].x, (int)bullets[i].y, bullets[i].owner ? CLR_ORANGE : CLR_WHITE);

    // enemy planes
    for (int e = 0; e < MAX_ENEMIES; e++)
        if (enemies[e].alive) draw_plane(enemies[e].x, enemies[e].y, enemies[e].pa, CLR_DARK_RED, CLR_RED);

    // player — solid on spawn, then blink out the rest of the invuln window
    if (gstate != GAMEOVER && !(invuln > 0 && invuln < 60 && !blink(3)))
        draw_plane(px, py, pa, CLR_BROWN, CLR_LIGHT_PEACH);

    // explosions
    for (int i = 0; i < MAX_BOOM; i++)
        if (booms[i].t > 0) {
            int r = (18 - booms[i].t);
            circfill((int)booms[i].x, (int)booms[i].y, r / 2 + 2,
                     booms[i].t > 9 ? CLR_LIGHT_YELLOW : CLR_ORANGE);
        }

    camera(0, 0);

    // ── HUD ──
    print(str("SCORE %d", score), 4, 4, CLR_WHITE);
    print(str("LIVES %d", lives), 4, 13, CLR_LIGHT_PEACH);
    print_right(str("HANGARS %d", targets_left()), 316, 4, CLR_RED);

    if (gstate == GAMEOVER) {
        print_centered("SHOT DOWN", SCREEN_W/2, 86, CLR_RED);
        print_centered("press Z to fly again", SCREEN_W/2, 100, CLR_WHITE);
    } else if (gstate == WIN) {
        print_centered("MISSION COMPLETE", SCREEN_W/2, 86, CLR_YELLOW);
        print_centered("press Z for a new sortie", SCREEN_W/2, 100, CLR_WHITE);
    } else {
        print("W/S pitch   Z gun   X bomb", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
    }
}
