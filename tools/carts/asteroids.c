/* de:meta
{
  "title": "asteroids",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Asteroids (1979) clone; screen-wrap toroidal space, thrust-with-friction inertia, seeded procedural rock polygons, splitting on hit, respawn invincibility.",
  "genre": "shooter",
  "homage": "Asteroids (1979)",
  "description": "Classic vector shooter. Rotate and thrust your ship, blast rocks apart, survive endless waves. Left/right to rotate, up to thrust, Z to fire."
}
de:meta */
#include "studio.h"

// ASTEROIDS
// left/right: rotate   up: thrust   Z: fire
// 3 lives, waves get harder each round

#define MAX_ROCKS  24
#define MAX_BULS    5
#define VERTS       8   // vertices per asteroid polygon

static const int ROCK_R[3]   = { 22, 13, 7 };
static const int ROCK_PTS[3] = { 20, 50, 100 };

typedef struct { float x, y, vx, vy; bool on; int age; } Bul;
typedef struct { float x, y, vx, vy; int sz, seed; bool on; } Rock;

// ship
static float shx, shy, shvx, shvy, shang;
static bool  ship_on;
static float born_t, died_t;

// game objects
static Bul  buls[MAX_BULS];
static Rock rocks[MAX_ROCKS];
static int  n_rocks;

// state
static int  score, hiscore, lives, wave;
static bool gameover;

// ---- wrap helpers ----

static float wrx(float v) { return v<0?v+SCREEN_W:v>=SCREEN_W?v-SCREEN_W:v; }
static float wry(float v) { return v<0?v+SCREEN_H:v>=SCREEN_H?v-SCREEN_H:v; }

// ---- rock management ----

static void kill_rock(int i) { rocks[i].on = false; n_rocks--; }

static void add_rock(float x, float y, float vx, float vy, int sz, int seed) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (!rocks[i].on) {
            rocks[i].x=x; rocks[i].y=y;
            rocks[i].vx=vx; rocks[i].vy=vy;
            rocks[i].sz=sz; rocks[i].seed=seed;
            rocks[i].on=true; n_rocks++;
            return;
        }
    }
}

static void spawn_wave() {
    wave++;
    int count = min(2 + wave, 8);
    for (int i = 0; i < count; i++) {
        float ang = rnd(360), d = 70.0f + rnd(40);
        float rx = wrx(SCREEN_W/2 + dx(d, ang));
        float ry = wry(SCREEN_H/2 + dy(d, ang));
        float va = rnd(360), sp = 0.4f + rnd_float() * (0.5f + wave * 0.08f);
        add_rock(rx, ry, dx(sp, va), dy(sp, va), 0, rnd(254)+1);
    }
}

static void respawn() {
    shx=SCREEN_W/2; shy=SCREEN_H/2;
    shvx=shvy=0; shang=-90;
    ship_on=true; born_t=now();
}

static void new_game() {
    for (int i=0;i<MAX_ROCKS;i++) rocks[i].on=false;
    for (int i=0;i<MAX_BULS; i++) buls[i].on =false;
    n_rocks=wave=score=0; lives=3; gameover=false;
    respawn();
    spawn_wave();
}

// ---- draw one asteroid polygon ----

static void draw_rock(int x, int y, int sz, int seed, int col) {
    float px=0, py=0;
    for (int v=0; v<=VERTS; v++) {
        int vi = v % VERTS;
        float ang = vi * (360.0f / VERTS);
        int h = (seed * 7 + vi * 13) & 15;
        float r = ROCK_R[sz] * (1.0f + (h/7.5f - 1.0f) * 0.28f);
        float qx = x + dx(r, ang), qy = y + dy(r, ang);
        if (v > 0) line((int)px,(int)py,(int)qx,(int)qy, col);
        px=qx; py=qy;
    }
}

// ---- lifecycle ----

void init() {
    hiscore = load(0);
    new_game();
}

void update() {
    if (gameover) {
        if (btnp(0,BTN_A)||btnp(0,BTN_B)) new_game();
        return;
    }

    // ship control
    if (ship_on) {
        if (btn(0,BTN_LEFT))  shang -= 4.0f;
        if (btn(0,BTN_RIGHT)) shang += 4.0f;
        if (btn(0,BTN_UP)) {
            shvx += dx(0.18f, shang);
            shvy += dy(0.18f, shang);
        }
        shvx = clamp(shvx * 0.988f, -4.5f, 4.5f);
        shvy = clamp(shvy * 0.988f, -4.5f, 4.5f);
        shx = wrx(shx + shvx);
        shy = wry(shy + shvy);

        // fire
        if (btnp(0,BTN_A)) {
            for (int i=0; i<MAX_BULS; i++) {
                if (!buls[i].on) {
                    buls[i].x  = shx + dx(11, shang);
                    buls[i].y  = shy + dy(11, shang);
                    buls[i].vx = dx(6.5f, shang) + shvx;
                    buls[i].vy = dy(6.5f, shang) + shvy;
                    buls[i].on = true; buls[i].age = 0;
                    hit(72, INSTR_SQUARE, 3, 50);
                    break;
                }
            }
        }

        // collision — skip during 1.8s invincibility window after respawn
        if (now() - born_t > 1.8f) {
            for (int i=0; i<MAX_ROCKS; i++) {
                if (!rocks[i].on) continue;
                if (near((int)shx,(int)shy,(int)rocks[i].x,(int)rocks[i].y, ROCK_R[rocks[i].sz])) {
                    ship_on=false; died_t=now(); lives--;
                    note(36, INSTR_NOISE, 6);
                    if (lives <= 0) {
                        gameover=true;
                        if (score > hiscore) { hiscore=score; save(0,hiscore); }
                    }
                    break;
                }
            }
        }
    } else if (!gameover) {
        if (now() - died_t > 2.2f) respawn();
    }

    // bullets
    for (int i=0; i<MAX_BULS; i++) {
        if (!buls[i].on) continue;
        buls[i].x = wrx(buls[i].x + buls[i].vx);
        buls[i].y = wry(buls[i].y + buls[i].vy);
        if (++buls[i].age > 55) { buls[i].on=false; continue; }
        for (int j=0; j<MAX_ROCKS; j++) {
            if (!rocks[j].on) continue;
            if (near((int)buls[i].x,(int)buls[i].y,(int)rocks[j].x,(int)rocks[j].y, ROCK_R[rocks[j].sz])) {
                buls[i].on = false;
                score += ROCK_PTS[rocks[j].sz];
                int  new_sz = rocks[j].sz + 1;
                float ex=rocks[j].x, ey=rocks[j].y;
                int  eseed = rocks[j].seed;
                note(36 + rocks[j].sz * 12, INSTR_NOISE, 4);
                kill_rock(j);
                if (new_sz <= 2) {
                    float a1=rnd(360), sp=1.0f+rnd_float()*1.5f;
                    add_rock(ex, ey, dx(sp,a1), dy(sp,a1), new_sz, rnd(254)+1);
                    float a2=a1+100+rnd(160);
                    add_rock(ex, ey, dx(sp,a2), dy(sp,a2), new_sz, rnd(254)+1);
                }
                break;
            }
        }
    }

    // move rocks
    for (int i=0; i<MAX_ROCKS; i++) {
        if (!rocks[i].on) continue;
        rocks[i].x = wrx(rocks[i].x + rocks[i].vx);
        rocks[i].y = wry(rocks[i].y + rocks[i].vy);
    }

    if (n_rocks == 0 && !gameover) spawn_wave();
}

void draw() {
    cls(CLR_BLACK);

    // static star field
    for (int i=0; i<42; i++) {
        int sx = (i * 97 + 11) % SCREEN_W;
        int sy = (i * 73 + 53) % SCREEN_H;
        pset(sx, sy, i%4==0 ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }

    // rocks
    for (int i=0; i<MAX_ROCKS; i++) {
        if (!rocks[i].on) continue;
        int col = rocks[i].sz==0 ? CLR_WHITE :
                  rocks[i].sz==1 ? CLR_LIGHT_GREY : CLR_DARK_GREY;
        draw_rock((int)rocks[i].x,(int)rocks[i].y, rocks[i].sz, rocks[i].seed, col);
    }

    // bullets
    for (int i=0; i<MAX_BULS; i++) {
        if (!buls[i].on) continue;
        circfill((int)buls[i].x,(int)buls[i].y, 1, CLR_YELLOW);
    }

    // ship
    if (ship_on) {
        bool inv  = (now() - born_t) < 1.8f;
        bool show = !inv || (frame()%6 < 3);
        if (show) {
            float nx=shx+dx(10,shang), ny=shy+dy(10,shang);
            float lx=shx+dx(8,shang+142), ly=shy+dy(8,shang+142);
            float rx=shx+dx(8,shang-142), ry=shy+dy(8,shang-142);
            int col = inv ? CLR_BLUE : CLR_WHITE;
            line((int)nx,(int)ny,(int)lx,(int)ly, col);
            line((int)nx,(int)ny,(int)rx,(int)ry, col);
            line((int)lx,(int)ly,(int)rx,(int)ry, col);
            // thrust flame
            if (btn(0,BTN_UP) && frame()%3 < 2) {
                float tx=shx+dx(12+rnd(4),shang+180);
                float ty=shy+dy(12+rnd(4),shang+180);
                line((int)lx,(int)ly,(int)tx,(int)ty, CLR_ORANGE);
                line((int)rx,(int)ry,(int)tx,(int)ty, CLR_YELLOW);
            }
        }
    }

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W-4, 3, CLR_YELLOW);
    print_centered(str("WAVE %d", wave), SCREEN_W/2, 3, CLR_DARK_GREY);

    // life icons (mini ships)
    for (int i=0; i<lives; i++) {
        int lx=5+i*13, ly=SCREEN_H-10;
        line(lx+5, ly-4, lx,    ly+4, CLR_LIGHT_GREY);
        line(lx+5, ly-4, lx+10, ly+4, CLR_LIGHT_GREY);
        line(lx+2, ly+2, lx+8,  ly+2, CLR_LIGHT_GREY);
    }

    // game over screen
    if (gameover) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-24, 128, 54, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-24, 128, 54, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-14, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2+14, CLR_LIGHT_GREY);
    }
}
