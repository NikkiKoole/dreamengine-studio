#include "studio.h"

// CENTIPEDE — the arcade classic. A segmented bug winds down through a field of
// mushrooms; you're a blaster locked to the bottom band, firing upward. Shoot a
// mid-body segment and the centipede SPLITS in two (and a mushroom sprouts there);
// shoot the head and it just shortens. A spider skitters through the lower third
// eating mushrooms and hunting you. Clear every segment to advance to a faster wave.
//
// Arrows / WASD move    Z / Space fire upward

// ---- grid ----  cell = 10px. play field is the whole screen below the HUD.
#define CELL   10
#define GW     (SCREEN_W / CELL)            // 32
#define GH     (SCREEN_H / CELL)            // 20
#define HUD    14
#define TOPROW 2                            // top grid row of play (below HUD)
#define BOTBAND 5                           // bottom rows the player may roam
#define PLY_TOP (GH - BOTBAND)              // grid row where the player band starts

#define MAXSEG 40
#define MUSH_HP 4

typedef struct { int alive, head; float x, y; int dir; int dropping; float drop_to; } Seg;

static Seg seg[MAXSEG];
static unsigned char mush[GW * GH];         // 0 = empty, else remaining hp (1..MUSH_HP)

// player
static float plx, ply;                      // pixel center
static float blx, bly; static int bullet;   // single bullet
// spider
static float spx, spy, spvx, spvy; static int spider_on; static float sp_timer;
// game state
static int score, hi, lives, wave, state;   // state 0 play, 1 wave-clear, 2 dead, 3 gameover
static float step_t, step_period;           // centipede cadence
static float death_t, clear_t, flash_t;

static inline int mat(int cx, int cy){ return mush[cy * GW + cx]; }
static inline void mset_h(int cx, int cy, int v){ if(cx>=0&&cx<GW&&cy>=0&&cy<GH) mush[cy*GW+cx]=(unsigned char)v; }

static void seed_mushrooms(int n){
    int placed = 0, guard = 0;
    while (placed < n && guard++ < 2000){
        int cx = rnd(GW), cy = rnd_between(TOPROW, PLY_TOP);   // not in HUD, not in player band
        if (mat(cx, cy) == 0){ mset_h(cx, cy, MUSH_HP); placed++; }
    }
}

static void spawn_centipede(int len){
    for (int i = 0; i < MAXSEG; i++) seg[i].alive = 0;
    if (len > MAXSEG) len = MAXSEG;
    for (int i = 0; i < len; i++){
        Seg *s = &seg[i];
        s->alive = 1; s->head = (i == 0);
        s->dir = 1;
        s->x = (TOPROW * 0) + (-(i + 1)) * CELL + CELL/2.0f;  // off-screen left, single file
        s->y = TOPROW * CELL + CELL/2.0f;
        s->dropping = 0; s->drop_to = s->y;
    }
}

static void spawn_spider(void){
    spider_on = 1;
    int left = rnd(2);
    spx = left ? -8 : SCREEN_W + 8;
    spy = (PLY_TOP - 1) * CELL;
    spvx = (left ? 1 : -1) * rnd_float_between(55, 80);
    spvy = rnd_float_between(40, 70);
}

static void start_wave(int w){
    wave = w;
    step_period = clamp(0.32f - w * 0.025f, 0.10f, 0.32f);
    spawn_centipede(9 + (w < 4 ? w : 3));
    bullet = 0;
    spider_on = 0; sp_timer = rnd_float_between(3.0f, 6.0f);
    step_t = 0;
}

void init(void){
    hi = load_int("hi", 0);
    score = 0; lives = 3; wave = 1; state = 0;
    death_t = clear_t = flash_t = 0;
    for (int i = 0; i < GW*GH; i++) mush[i] = 0;
    seed_mushrooms(28);
    plx = SCREEN_W / 2.0f; ply = (GH - 1) * CELL + CELL/2.0f;
    start_wave(1);
}

static int alive_segs(void){ int n=0; for(int i=0;i<MAXSEG;i++) if(seg[i].alive) n++; return n; }

// add a mushroom-blocked cell; returns 1 if the cell ahead is a wall/mushroom
static int blocked(int cx, int cy){
    if (cx < 0 || cx >= GW) return 1;
    if (cy < TOPROW) return 1;
    return mat(cx, cy) != 0;
}

static void player_die(void){
    if (death_t > 0) return;
    lives--; death_t = 1.4f; flash_t = 0.4f;
    shake(6); hit(30, INSTR_NOISE, 5, 260);
    if (lives < 0){ lives = 0; state = 3;
        if (score > hi){ hi = score; save_int("hi", hi); } }
    else state = 2;
}

static void kill_segment(int idx){
    Seg *s = &seg[idx];
    int cx = (int)(s->x / CELL), cy = (int)(s->y / CELL);
    // drop a mushroom where it died
    if (cx >= 0 && cx < GW && cy >= TOPROW && cy < GH && mat(cx, cy) == 0)
        mset_h(cx, cy, MUSH_HP);
    int was_head = s->head;
    s->alive = 0;
    score += was_head ? 100 : 10;
    note(was_head ? 72 : 64, INSTR_SQUARE, 3);
    // the chain is single-file in array order: a run of alive segments led by a head.
    // killing one splits it — whoever was the very next alive segment after this one
    // becomes the head of the trailing piece. (If that segment was already this chain's
    // head it can't be, since we just killed an earlier index; if it's a later chain's
    // head, setting head=1 is a no-op.)
    for (int j = idx + 1; j < MAXSEG; j++){
        if (seg[j].alive){ seg[j].head = 1; break; }
    }
}

void update(void){
    float d = dt();
    if (flash_t > 0) flash_t -= d;

    if (state == 3){ if (btnp(0,BTN_A)||btnp(1,BTN_A)||keyp(KEY_SPACE)) init(); return; }

    if (state == 2){                       // brief death pause then respawn
        death_t -= d;
        if (death_t <= 0){ state = 0; plx = SCREEN_W/2.0f; bullet = 0;
                           spider_on = 0; sp_timer = rnd_float_between(2.5f, 5.0f); }
        return;
    }

    if (state == 1){                       // wave-clear flourish
        clear_t -= d;
        if (clear_t <= 0){ state = 0; seed_mushrooms(6); start_wave(wave + 1); }
        return;
    }

    // ---- player ----
    float spd = 130;
    int ix = (btn(0,BTN_RIGHT)||btn(1,BTN_RIGHT)) - (btn(0,BTN_LEFT)||btn(1,BTN_LEFT));
    int iy = (btn(0,BTN_DOWN)||btn(1,BTN_DOWN)) - (btn(0,BTN_UP)||btn(1,BTN_UP));
    plx = clamp(plx + ix*spd*d, CELL/2.0f, SCREEN_W - CELL/2.0f);
    ply = clamp(ply + iy*spd*d, PLY_TOP*CELL + CELL/2.0f, (GH-1)*CELL + CELL/2.0f);

    // fire — one bullet on screen
    if (!bullet && (btnp(0,BTN_A)||btnp(1,BTN_A)||keyp(KEY_SPACE))){
        bullet = 1; blx = plx; bly = ply - 6; note(84, INSTR_SQUARE, 2);
    }

    // ---- bullet ----
    if (bullet){
        bly -= 360 * d;
        if (bly < HUD){ bullet = 0; }
        else {
            int cx = (int)(blx / CELL), cy = (int)(bly / CELL);
            // hit a mushroom?
            if (cx >= 0 && cx < GW && cy >= TOPROW && cy < GH && mat(cx, cy) > 0){
                int hp = mat(cx, cy) - 1; mset_h(cx, cy, hp);
                bullet = 0; score += 1; hit(60, INSTR_NOISE, 2, 40);
            }
        }
    }
    // bullet vs segments
    if (bullet){
        for (int i = 0; i < MAXSEG; i++){
            if (!seg[i].alive) continue;
            if (distance((int)blx,(int)bly,(int)seg[i].x,(int)seg[i].y) < 6){
                kill_segment(i); bullet = 0; shake(2); break;
            }
        }
    }

    // ---- centipede stepping ----
    step_t += d;
    if (step_t >= step_period){
        step_t -= step_period;
        for (int i = 0; i < MAXSEG; i++){
            Seg *s = &seg[i]; if (!s->alive) continue;
            int cx = (int)(s->x / CELL), cy = (int)(s->y / CELL);
            int ncx = cx + s->dir;
            if (blocked(ncx, cy)){
                // drop a row and reverse; if at bottom, wrap up toward player band edge
                s->dir = -s->dir;
                int ncy = cy + 1;
                if (ncy >= GH){ ncy = TOPROW; }      // reached the floor — loop back up
                s->y = ncy * CELL + CELL/2.0f;
                // nudge x to the cell center so the grid stays clean
                s->x = cx * CELL + CELL/2.0f;
            } else {
                s->x = ncx * CELL + CELL/2.0f;
                s->y = cy * CELL + CELL/2.0f;
            }
        }
    }

    // ---- centipede vs player ----
    if (state == 0){
        for (int i = 0; i < MAXSEG; i++){
            if (!seg[i].alive) continue;
            if (distance((int)plx,(int)ply,(int)seg[i].x,(int)seg[i].y) < 8){ player_die(); break; }
        }
    }

    // ---- spider ----
    if (state == 0){
        if (!spider_on){ sp_timer -= d; if (sp_timer <= 0) spawn_spider(); }
        else {
            spx += spvx * d; spy += spvy * d;
            int topb = (PLY_TOP - 2) * CELL, botb = (GH - 1) * CELL;
            if (spy < topb){ spy = topb; spvy = -spvy; }
            if (spy > botb){ spy = botb; spvy = -spvy; }
            if (chance(3)) spvy = -spvy;                 // erratic darting
            // eat mushrooms it crosses
            int cx = (int)(spx / CELL), cy = (int)(spy / CELL);
            if (cx>=0&&cx<GW&&cy>=TOPROW&&cy<GH && mat(cx,cy)>0){ mset_h(cx,cy,0); hit(48,INSTR_NOISE,2,40); }
            // off-screen — gone
            if (spx < -16 || spx > SCREEN_W + 16){ spider_on = 0; sp_timer = rnd_float_between(3.0f, 7.0f); }
            // vs player
            if (distance((int)plx,(int)ply,(int)spx,(int)spy) < 9){ player_die(); }
            // vs bullet
            if (bullet && distance((int)blx,(int)bly,(int)spx,(int)spy) < 8){
                bullet = 0; spider_on = 0; score += 300; shake(2);
                note(76, INSTR_SQUARE, 3); sp_timer = rnd_float_between(3.0f, 7.0f);
            }
        }
    }

    // ---- wave clear ----
    if (state == 0 && alive_segs() == 0){
        state = 1; clear_t = 1.4f; score += 50 * wave;
        note(72, INSTR_SQUARE, 3); schedule(140, 76, INSTR_SQUARE, 3); schedule(280, 79, INSTR_SQUARE, 3);
    }
}

// ---- drawing ----
static void draw_mushroom(int cx, int cy, int hp){
    int x = cx*CELL + CELL/2, y = cy*CELL + CELL/2;
    int cap = hp >= 3 ? CLR_RED : hp == 2 ? CLR_ORANGE : CLR_DARK_RED;
    rectfill(x-2, y, 4, 4, CLR_LIGHT_PEACH);     // stalk
    ovalfill(x, y-1, 4, 3, cap);                  // cap
    pset(x-2, y-1, CLR_WHITE); pset(x+2, y-1, CLR_WHITE);
}

static void draw_segment(Seg *s){
    int x = (int)s->x, y = (int)s->y;
    int body = s->head ? CLR_YELLOW : CLR_GREEN;
    circfill(x, y, 4, body);
    if (s->head){
        // eyes facing travel direction
        int e = s->dir > 0 ? 2 : -2;
        pset(x + e, y-1, CLR_BLACK); pset(x + e, y+1, CLR_BLACK);
    } else {
        pset(x, y, CLR_DARK_GREEN);
    }
}

static void draw_spider(int x, int y){
    // legs
    for (int i = -1; i <= 1; i += 2){
        line(x, y, x - 8, y - 3*i, CLR_LIGHT_GREY);
        line(x, y, x - 8, y + 3*i, CLR_LIGHT_GREY);
        line(x, y, x + 8, y - 3*i, CLR_LIGHT_GREY);
        line(x, y, x + 8, y + 3*i, CLR_LIGHT_GREY);
    }
    ovalfill(x, y, 5, 4, CLR_INDIGO);
    pset(x-2, y-1, CLR_RED); pset(x+2, y-1, CLR_RED);
}

void draw(void){
    cls(CLR_BLACK);

    // play-field background tint
    rectfill(0, HUD, SCREEN_W, SCREEN_H - HUD, CLR_BROWNISH_BLACK);
    // player band marker
    line(0, PLY_TOP*CELL, SCREEN_W, PLY_TOP*CELL, CLR_DARKER_GREY);

    if (flash_t > 0){ pal(CLR_BROWNISH_BLACK, CLR_DARK_RED); }

    // mushrooms
    for (int cy = TOPROW; cy < GH; cy++)
        for (int cx = 0; cx < GW; cx++){
            int hp = mat(cx, cy); if (hp > 0) draw_mushroom(cx, cy, hp);
        }

    // centipede
    for (int i = 0; i < MAXSEG; i++) if (seg[i].alive) draw_segment(&seg[i]);

    // spider
    if (spider_on) draw_spider((int)spx, (int)spy);

    // bullet
    if (bullet) rectfill((int)blx-1, (int)bly-4, 2, 6, CLR_WHITE);

    // player blaster
    if (state != 2 || ((int)(now()*16) & 1)){
        int px = (int)plx, py = (int)ply;
        trifill(px, py-6, px-6, py+5, px+6, py+5, CLR_BLUE);
        rectfill(px-1, py-7, 2, 3, CLR_LIGHT_PEACH);
        rectfill(px-5, py+3, 10, 3, CLR_TRUE_BLUE);
    }

    pal_reset();

    // HUD
    rectfill(0, 0, SCREEN_W, HUD, CLR_BLACK);
    line(0, HUD, SCREEN_W, HUD, CLR_DARK_GREY);
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_centered(str("WAVE %d", wave), 3, CLR_LIME_GREEN);
    print_right(str("HI %d", hi), SCREEN_W - 4, 3, CLR_LIGHT_GREY);
    // lives as little blasters
    for (int i = 0; i < lives; i++){
        int lx = SCREEN_W/2 + 30 + i*10;
        trifill(lx, 10, lx-3, 11, lx+3, 11, CLR_BLUE);
    }

    if (state == 1){
        print_centered(str("WAVE %d CLEAR!", wave), SCREEN_H/2 - 4, CLR_YELLOW);
    }
    if (state == 3){
        rectfill(70, 78, 180, 44, CLR_BLACK); rect(70, 78, 180, 44, CLR_RED);
        print_centered("GAME OVER", 86, CLR_RED);
        print_centered(str("SCORE %d", score), 100, CLR_WHITE);
        print_centered("Z to play again", 112, CLR_LIGHT_GREY);
    }
}
