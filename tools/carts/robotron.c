/* de:meta
{
  "title": "robotron 2084",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "finite-state-ai"
  ],
  "lineage": "Direct Robotron: 2084 (1982) homage; the Brain enemy's civilian-to-zombie conversion is the only meaningful AI state transition; primary value is as a clean twin-stick arena-shooter scaffold with wave progression.",
  "genre": "shooter",
  "homage": "Robotron: 2084 (1982)",
  "description": "Twin-stick arena shooter. WASD to move, arrows to shoot 8-way. Rescue civilians, blast grunts and enforcers, dodge bullets. Brains convert civilians to zombies. Hulks can't be killed!"
}
de:meta */
#include "studio.h"

// ROBOTRON 2084
// WASD = move   Arrow keys = shoot (8-way, hold to auto-fire)
// Rescue civilians (green), destroy enemies, survive endless waves.
// Brains convert civilians to zombies. Hulks can't be killed!

#define BORDER  14
#define PX1     BORDER
#define PY1     BORDER
#define PX2     (SCREEN_W - BORDER)
#define PY2     (SCREEN_H - BORDER)

#define MAX_ENE   64
#define MAX_CIV   10
#define MAX_PBUL  24
#define MAX_EBUL  32

#define T_GRUNT     0
#define T_ENFORCER  1
#define T_BRAIN     2
#define T_HULK      3
#define T_ELECTRODE 4

static const int ENE_PTS[5] = { 100, 150, 500, 0, 0 };

typedef struct { float x, y, vx, vy; bool on; int type; int cd; } Enemy;
typedef struct { float x, y, vx, vy; bool on; bool zombie; } Civ;
typedef struct { float x, y, vx, vy; bool on; } Bul;

static float  px, py;
static int    pdir_x, pdir_y, shoot_cd;
static bool   player_on;
static float  born_t, died_t;

static Enemy enes[MAX_ENE];
static Civ   civs[MAX_CIV];
static Bul   pbuls[MAX_PBUL];
static Bul   ebuls[MAX_EBUL];

static int   score, hiscore, lives, wave;
static bool  gameover, wave_clear;
static float wave_clear_t;

// ---- helpers ----

static void spawn_enemy(int type, float x, float y) {
    for (int i = 0; i < MAX_ENE; i++) {
        if (!enes[i].on) {
            enes[i].x = x; enes[i].y = y;
            enes[i].vx = enes[i].vy = 0;
            enes[i].type = type; enes[i].on = true;
            enes[i].cd = 60 + rnd(60);
            return;
        }
    }
}

static int count_enemies() {
    int n = 0;
    for (int i = 0; i < MAX_ENE; i++)
        if (enes[i].on && enes[i].type != T_ELECTRODE && enes[i].type != T_HULK) n++;
    return n;
}

static void spawn_wave() {
    for (int i = 0; i < MAX_ENE;  i++) enes[i].on  = false;
    for (int i = 0; i < MAX_CIV;  i++) civs[i].on  = false;
    for (int i = 0; i < MAX_PBUL; i++) pbuls[i].on = false;
    for (int i = 0; i < MAX_EBUL; i++) ebuls[i].on = false;

    // civilians
    int nc = min(MAX_CIV, 3 + wave);
    for (int i = 0; i < nc; i++) {
        civs[i].x = PX1 + 12 + rnd(PX2-PX1-24);
        civs[i].y = PY1 + 12 + rnd(PY2-PY1-24);
        civs[i].vx = rnd_float_between(-0.35f, 0.35f);
        civs[i].vy = rnd_float_between(-0.35f, 0.35f);
        civs[i].on = true; civs[i].zombie = false;
    }

    // enemies — spawn away from center
    int grunts    = min(22, 4 + wave * 3);
    int enforcers = (wave > 1) ? min(8,  (wave-1)*2) : 0;
    int brains    = (wave > 2) ? min(4,   wave-2)    : 0;
    int hulks     = (wave > 2) ? min(6,  (wave-2)*2) : 0;
    int elecs     = min(10, wave * 2);

    for (int i = 0; i < grunts; i++) {
        float a = rnd(360), d = 60 + rnd(50);
        spawn_enemy(T_GRUNT,
            clamp(SCREEN_W/2 + dx(d,a), PX1+4, PX2-4),
            clamp(SCREEN_H/2 + dy(d,a), PY1+4, PY2-4));
    }
    for (int i = 0; i < enforcers; i++)
        spawn_enemy(T_ENFORCER, PX1+12+rnd(PX2-PX1-24), PY1+12+rnd(PY2-PY1-24));
    for (int i = 0; i < brains; i++)
        spawn_enemy(T_BRAIN,    PX1+12+rnd(PX2-PX1-24), PY1+12+rnd(PY2-PY1-24));
    for (int i = 0; i < hulks; i++)
        spawn_enemy(T_HULK,     PX1+12+rnd(PX2-PX1-24), PY1+12+rnd(PY2-PY1-24));
    for (int i = 0; i < elecs; i++)
        spawn_enemy(T_ELECTRODE, PX1+12+rnd(PX2-PX1-24), PY1+12+rnd(PY2-PY1-24));
}

static void respawn_player() {
    px = SCREEN_W/2; py = SCREEN_H/2;
    player_on = true; born_t = now();
    pdir_x = 0; pdir_y = -1; shoot_cd = 0;
}

static void new_game() {
    score = 0; lives = 3; wave = 1; gameover = false; wave_clear = false;
    respawn_player();
    spawn_wave();
}

// ----

void init() { hiscore = load(0); new_game(); }

void update() {
    if (gameover) {
        if (btnp(0,BTN_A)||btnp(0,BTN_B)) new_game();
        return;
    }
    if (wave_clear) {
        if (now() - wave_clear_t > 1.8f) { wave++; spawn_wave(); wave_clear = false; }
        return;
    }

    // player movement (WASD = player 0)
    if (player_on) {
        float ps = 1.6f;
        float mvx = 0, mvy = 0;
        if (btn(0,BTN_UP))    mvy -= ps;
        if (btn(0,BTN_DOWN))  mvy += ps;
        if (btn(0,BTN_LEFT))  mvx -= ps;
        if (btn(0,BTN_RIGHT)) mvx += ps;
        px = clamp(px+mvx, PX1+4, PX2-4);
        py = clamp(py+mvy, PY1+4, PY2-4);

        // shoot direction (arrows = player 1)
        int sx = 0, sy = 0;
        if (btn(1,BTN_UP))    sy = -1;
        if (btn(1,BTN_DOWN))  sy =  1;
        if (btn(1,BTN_LEFT))  sx = -1;
        if (btn(1,BTN_RIGHT)) sx =  1;

        if (sx || sy) { pdir_x = sx; pdir_y = sy; }
        if (shoot_cd > 0) shoot_cd--;

        if ((sx || sy) && shoot_cd == 0) {
            for (int i = 0; i < MAX_PBUL; i++) {
                if (!pbuls[i].on) {
                    float n = (sx && sy) ? 0.707f : 1.0f;
                    pbuls[i].x = px; pbuls[i].y = py;
                    pbuls[i].vx = sx * 4.5f * n;
                    pbuls[i].vy = sy * 4.5f * n;
                    pbuls[i].on = true;
                    hit(78, INSTR_SQUARE, 2, 35);
                    shoot_cd = 6;
                    break;
                }
            }
        }
    }

    // player bullets
    for (int i = 0; i < MAX_PBUL; i++) {
        if (!pbuls[i].on) continue;
        pbuls[i].x += pbuls[i].vx;
        pbuls[i].y += pbuls[i].vy;
        if (pbuls[i].x<PX1||pbuls[i].x>PX2||pbuls[i].y<PY1||pbuls[i].y>PY2) {
            pbuls[i].on = false; continue;
        }
        for (int j = 0; j < MAX_ENE && pbuls[i].on; j++) {
            if (!enes[j].on || enes[j].type==T_HULK || enes[j].type==T_ELECTRODE) continue;
            if (near((int)pbuls[i].x,(int)pbuls[i].y,(int)enes[j].x,(int)enes[j].y, 5)) {
                score += ENE_PTS[enes[j].type];
                hit(48, INSTR_NOISE, 3, 60);
                enes[j].on = false;
                pbuls[i].on = false;
            }
        }
    }

    float ws = 1.0f + (wave-1) * 0.06f;

    // enemy AI
    for (int i = 0; i < MAX_ENE; i++) {
        if (!enes[i].on) continue;
        float ex = enes[i].x, ey = enes[i].y;

        if (enes[i].type == T_GRUNT) {
            float spd = clamp((0.5f + wave*0.08f) * ws, 0.5f, 2.4f);
            float a   = angle_to((int)ex,(int)ey,(int)px,(int)py);
            enes[i].x = clamp(ex+dx(spd,a), PX1+2, PX2-2);
            enes[i].y = clamp(ey+dy(spd,a), PY1+2, PY2-2);

        } else if (enes[i].type == T_ENFORCER) {
            float spd = clamp(0.45f * ws, 0.45f, 1.6f);
            float a   = angle_to((int)ex,(int)ey,(int)px,(int)py);
            enes[i].x = clamp(ex+dx(spd,a), PX1+2, PX2-2);
            enes[i].y = clamp(ey+dy(spd,a), PY1+2, PY2-2);
            if (--enes[i].cd <= 0) {
                enes[i].cd = 85 + rnd(55);
                for (int b = 0; b < MAX_EBUL; b++) {
                    if (!ebuls[b].on) {
                        float ba = angle_to((int)ex,(int)ey,(int)px,(int)py);
                        ebuls[b].x = ex; ebuls[b].y = ey;
                        ebuls[b].vx = dx(2.3f,ba);
                        ebuls[b].vy = dy(2.3f,ba);
                        ebuls[b].on = true; break;
                    }
                }
            }

        } else if (enes[i].type == T_BRAIN) {
            // find nearest non-zombie civilian, else chase player
            int   target = -1; float best = 9999;
            for (int c = 0; c < MAX_CIV; c++) {
                if (!civs[c].on || civs[c].zombie) continue;
                float d = distance((int)ex,(int)ey,(int)civs[c].x,(int)civs[c].y);
                if (d < best) { best = d; target = c; }
            }
            float tx = (target>=0) ? civs[target].x : px;
            float ty = (target>=0) ? civs[target].y : py;
            float a = angle_to((int)ex,(int)ey,(int)tx,(int)ty);
            enes[i].x = clamp(ex+dx(0.33f,a), PX1+2, PX2-2);
            enes[i].y = clamp(ey+dy(0.33f,a), PY1+2, PY2-2);
            if (target >= 0 && near((int)enes[i].x,(int)enes[i].y,
                                    (int)civs[target].x,(int)civs[target].y, 7)) {
                civs[target].zombie = true;
                hit(44, INSTR_SAW, 4, 220);
            }

        } else if (enes[i].type == T_HULK) {
            if (enes[i].vx == 0 && enes[i].vy == 0) {
                float a = rnd(360);
                enes[i].vx = dx(0.85f,a); enes[i].vy = dy(0.85f,a);
            }
            enes[i].x += enes[i].vx; enes[i].y += enes[i].vy;
            if (enes[i].x < PX1+5) { enes[i].x=PX1+5; if(enes[i].vx<0) enes[i].vx=-enes[i].vx; }
            if (enes[i].x > PX2-5) { enes[i].x=PX2-5; if(enes[i].vx>0) enes[i].vx=-enes[i].vx; }
            if (enes[i].y < PY1+5) { enes[i].y=PY1+5; if(enes[i].vy<0) enes[i].vy=-enes[i].vy; }
            if (enes[i].y > PY2-5) { enes[i].y=PY2-5; if(enes[i].vy>0) enes[i].vy=-enes[i].vy; }
        }
        // T_ELECTRODE: static — no movement
    }

    // enemy bullets
    for (int i = 0; i < MAX_EBUL; i++) {
        if (!ebuls[i].on) continue;
        ebuls[i].x += ebuls[i].vx; ebuls[i].y += ebuls[i].vy;
        if (ebuls[i].x<PX1||ebuls[i].x>PX2||ebuls[i].y<PY1||ebuls[i].y>PY2)
            ebuls[i].on = false;
    }

    // civilians: wander or zombie-chase
    for (int i = 0; i < MAX_CIV; i++) {
        if (!civs[i].on) continue;
        if (civs[i].zombie) {
            float a = angle_to((int)civs[i].x,(int)civs[i].y,(int)px,(int)py);
            civs[i].x = clamp(civs[i].x+dx(0.5f,a), PX1+2, PX2-2);
            civs[i].y = clamp(civs[i].y+dy(0.5f,a), PY1+2, PY2-2);
        } else {
            if (rnd(100) < 2) {
                civs[i].vx = rnd_float_between(-0.4f, 0.4f);
                civs[i].vy = rnd_float_between(-0.4f, 0.4f);
            }
            civs[i].x = clamp(civs[i].x+civs[i].vx, PX1+2, PX2-2);
            civs[i].y = clamp(civs[i].y+civs[i].vy, PY1+2, PY2-2);
        }
    }

    // player collisions (skip invincibility window)
    if (player_on && now() - born_t > 1.2f) {
        bool dead = false;

        for (int i = 0; i < MAX_ENE && !dead; i++) {
            if (!enes[i].on) continue;
            int r = (enes[i].type == T_HULK) ? 7 : 5;
            if (near((int)px,(int)py,(int)enes[i].x,(int)enes[i].y,r)) dead = true;
        }
        for (int i = 0; i < MAX_EBUL && !dead; i++) {
            if (!ebuls[i].on) continue;
            if (near((int)px,(int)py,(int)ebuls[i].x,(int)ebuls[i].y,4)) {
                ebuls[i].on = false; dead = true;
            }
        }
        for (int i = 0; i < MAX_CIV && !dead; i++) {
            if (!civs[i].on || !civs[i].zombie) continue;
            if (near((int)px,(int)py,(int)civs[i].x,(int)civs[i].y,5)) dead = true;
        }

        if (dead) {
            player_on = false; died_t = now(); lives--;
            hit(36, INSTR_NOISE, 7, 400);
            if (lives <= 0) {
                gameover = true;
                if (score > hiscore) { hiscore = score; save(0, hiscore); }
            }
        }

        // rescue civilians
        for (int i = 0; i < MAX_CIV; i++) {
            if (!civs[i].on || civs[i].zombie) continue;
            if (near((int)px,(int)py,(int)civs[i].x,(int)civs[i].y,8)) {
                civs[i].on = false;
                score += 250;
                hit(84, INSTR_SINE, 5, 150);
            }
        }

    } else if (!player_on && !gameover && lives > 0) {
        if (now() - died_t > 2.0f) respawn_player();
    }

    // wave clear when all killable enemies gone
    if (!wave_clear && !gameover && count_enemies() == 0) {
        wave_clear = true; wave_clear_t = now();
        for (int i = 0; i < MAX_CIV; i++)
            if (civs[i].on && !civs[i].zombie) score += 250;
        note(72, INSTR_SINE, 5);
    }
}

// ---- draw helpers ----

static void draw_grunt(int x, int y) {
    circ(x, y, 4, CLR_RED);
    line(x-2,y-2, x+2,y+2, CLR_RED);
    line(x+2,y-2, x-2,y+2, CLR_RED);
}

static void draw_enforcer(int x, int y) {
    line(x,   y-5, x+4, y,   CLR_ORANGE);
    line(x+4, y,   x,   y+5, CLR_ORANGE);
    line(x,   y+5, x-4, y,   CLR_ORANGE);
    line(x-4, y,   x,   y-5, CLR_ORANGE);
    pset(x, y, CLR_YELLOW);
}

static void draw_brain(int x, int y) {
    circ(x-2, y, 4, CLR_PINK);
    circ(x+2, y, 4, CLR_DARK_PURPLE);
    pset(x, y, CLR_WHITE);
}

static void draw_hulk(int x, int y) {
    rectfill(x-6, y-7, 13, 15, CLR_DARK_GREEN);
    rect    (x-6, y-7, 13, 15, CLR_GREEN);
    pset(x-2, y-3, CLR_RED);
    pset(x+2, y-3, CLR_RED);
}

static void draw_electrode(int x, int y) {
    int c = blink(5) ? CLR_ORANGE : CLR_YELLOW;
    line(x-4, y,   x+4, y,   c);
    line(x,   y-4, x,   y+4, c);
    line(x-3, y-3, x+3, y+3, c);
    line(x+3, y-3, x-3, y+3, c);
}

static void draw_civilian(float cx, float cy, bool zombie) {
    int x = (int)cx, y = (int)cy;
    int hc = zombie ? CLR_DARK_PURPLE  : CLR_WHITE;
    int bc = zombie ? CLR_PINK         : CLR_LIGHT_PEACH;
    circfill(x, y-5, 2, hc);
    line(x,   y-3, x,   y+2, bc);
    line(x,   y-1, x-3, y+1, bc);
    line(x,   y-1, x+3, y+1, bc);
    line(x,   y+2, x-2, y+5, bc);
    line(x,   y+2, x+2, y+5, bc);
    if (zombie && blink(8)) circ(x, y-5, 3, CLR_RED);
}

static void draw_player_shape(int x, int y, int col) {
    rectfill(x-3, y-1, 7, 3, col);
    rectfill(x-1, y-3, 3, 7, col);
    pset(x + pdir_x*4, y + pdir_y*4, CLR_YELLOW);
}

void draw() {
    static const int BCOLS[6] = {
        CLR_BLUE, CLR_GREEN, CLR_RED, CLR_ORANGE, CLR_PINK, CLR_YELLOW
    };
    cls(CLR_BLACK);

    // border — color shifts with wave
    int bc = BCOLS[(wave-1) % 6];
    rectfill(0,             0,             SCREEN_W, BORDER, bc);
    rectfill(0,             SCREEN_H-BORDER, SCREEN_W, BORDER, bc);
    rectfill(0,             0,             BORDER,   SCREEN_H, bc);
    rectfill(SCREEN_W-BORDER, 0,           BORDER,   SCREEN_H, bc);

    // civilians
    for (int i = 0; i < MAX_CIV; i++)
        if (civs[i].on) draw_civilian(civs[i].x, civs[i].y, civs[i].zombie);

    // enemies
    for (int i = 0; i < MAX_ENE; i++) {
        if (!enes[i].on) continue;
        int x = (int)enes[i].x, y = (int)enes[i].y;
        switch (enes[i].type) {
            case T_GRUNT:     draw_grunt(x, y);     break;
            case T_ENFORCER:  draw_enforcer(x, y);  break;
            case T_BRAIN:     draw_brain(x, y);     break;
            case T_HULK:      draw_hulk(x, y);      break;
            case T_ELECTRODE: draw_electrode(x, y); break;
        }
    }

    // enemy bullets
    for (int i = 0; i < MAX_EBUL; i++) {
        if (!ebuls[i].on) continue;
        pset((int)ebuls[i].x,   (int)ebuls[i].y, CLR_RED);
        pset((int)ebuls[i].x+1, (int)ebuls[i].y, CLR_ORANGE);
    }

    // player bullets
    for (int i = 0; i < MAX_PBUL; i++) {
        if (!pbuls[i].on) continue;
        pset((int)pbuls[i].x,   (int)pbuls[i].y, CLR_WHITE);
        pset((int)pbuls[i].x+1, (int)pbuls[i].y, CLR_YELLOW);
    }

    // player (blinks during invincibility)
    if (player_on) {
        bool inv  = now() - born_t < 1.2f;
        bool show = !inv || frame()%6 < 3;
        if (show) draw_player_shape((int)px, (int)py, CLR_WHITE);
    }

    // HUD — rendered inside the border strips
    print(str("SCORE %d", score),   BORDER+2,             3, CLR_WHITE);
    print_centered(str("WAVE %d", wave), SCREEN_W/2, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W-BORDER-2, 3, CLR_YELLOW);

    // lives as mini player icons (bottom-left border)
    for (int i = 0; i < lives; i++)
        draw_player_shape(BORDER+6+i*14, SCREEN_H-BORDER+7, CLR_LIGHT_GREY);

    // civilian count (bottom-right border)
    int alive = 0;
    for (int i = 0; i < MAX_CIV; i++)
        if (civs[i].on && !civs[i].zombie) alive++;
    if (alive > 0)
        print(str("CIV %d", alive), SCREEN_W/2-16, SCREEN_H-BORDER+3, CLR_LIGHT_PEACH);

    // wave clear flash
    if (wave_clear) {
        rectfill(SCREEN_W/2-56, SCREEN_H/2-12, 112, 22, CLR_BLACK);
        print_centered("WAVE CLEAR!", SCREEN_W/2, SCREEN_H/2-6, CLR_YELLOW);
    }

    // game over
    if (gameover) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-24, 128, 54, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-24, 128, 54, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-14, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2+14, CLR_LIGHT_GREY);
    }
}
