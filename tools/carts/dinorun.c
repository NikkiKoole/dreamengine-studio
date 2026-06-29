/* de:meta
{
  "title": "dino run",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "screen-shake-juice",
    "particle-system"
  ],
  "lineage": "Chrome offline dino homage; the main teaching value is the complete game-feel toolkit assembled in one small cart — squash-stretch, dust particles, input buffering, ghost trail, screen shake, and score pop all wired to their triggering events.",
  "genre": "platformer",
  "homage": "Chrome Dino (2014)",
  "description": "Chrome-offline-dino homage. You auto-run to the right — jump over cacti and duck under pterodactyls before they clip you. Speed climbs the longer you last, gaps between obstacles shrink, and birds only start appearing once you hit 300. Z / Up to jump; Down to duck. Score and best run saved."
}
de:meta */
#include "studio.h"

// DINO RUN — infinite side-scroller after the Chrome offline game.
// Z / W / Up / left-click: jump over cacti.   S / Down / right-click: duck under birds.
// Speed climbs the longer you survive. Best score saved.

#define GY        160    // ground line y
#define DX         48    // dino fixed screen x
#define DW         14    // collision width (standing + ducking)
#define DH         24    // collision height (standing)
#define DH_DUCK    12    // collision height (ducking)
#define GRAV       0.50f
#define JUMP_V    -8.0f

#define MAX_OBS  8
typedef struct { float x, y; int w, h, type; } Obs;
// type: 0=small cactus  1=tall cactus  2=double cactus  3=bird

#define MAX_CLOUDS 5
typedef struct { float x, y, w; } Cloud;

#define ST_IDLE  0
#define ST_PLAY  1
#define ST_DEAD  2

// feedback/juice state
typedef struct { float x, y, vx, vy; int life, col; } Dust;
static Dust  dust[24];

#define TRAIL 5
static float trail_y[TRAIL];
static bool  trail_air[TRAIL];
static int   trail_head;

static int    state;
static float  dino_y, dvy;
static bool   grounded, duck;
static float  squash;           // >0=squash (land), <0=stretch (takeoff)
static int    jump_buf;         // input buffer: jump pressed in air, fires on landing
static int    score, hiscore;
static float  speed, scroll;
static float  spawn_t, next_spawn;
static int    flash, score_pop;
static Obs    obs[MAX_OBS];
static Cloud  clouds[MAX_CLOUDS];

// ─── helpers ─────────────────────────────────────────────────────────────────

static void obs_off(Obs *o) { o->type = -1; }

static void do_spawn(void) {
    for (int i = 0; i < MAX_OBS; i++) {
        if (obs[i].type >= 0) continue;
        obs[i].x = SCREEN_W + 16;
        int t = (score < 300) ? rnd_between(0, 3) : rnd_between(0, 4);
        obs[i].type = t;
        switch (t) {
            case 0: obs[i].w=10; obs[i].h=24; obs[i].y=GY-24; break;
            case 1: obs[i].w=14; obs[i].h=32; obs[i].y=GY-32; break;
            case 2: obs[i].w=28; obs[i].h=26; obs[i].y=GY-26; break;
            case 3: obs[i].w=20; obs[i].h=14; obs[i].y=GY-30; break;
        }
        return;
    }
}

static void cloud_reset(Cloud *c, bool far) {
    c->x = far ? SCREEN_W + rnd_between(10, 80) : rnd_between(0, SCREEN_W);
    c->y = rnd_between(14, 50);
    c->w = rnd_between(32, 62);
}

static void spawn_dust(float px, float py) {
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < 24; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ px + rnd_between(-4, 5), py,
                    rnd_float_between(-1.5f, 1.5f), rnd_float_between(-1.8f, -0.1f),
                    rnd_between(8, 15), (k & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY };
                break;
            }
}

static void do_jump(void) {
    dvy = JUMP_V; grounded = false;
    squash = -0.85f;
    spawn_dust(DX + 5, GY);
    hit(70 + rnd_between(-2, 3), INSTR_SQUARE, 2, 40);  // pitch-varied jump sound
    jump_buf = 0;
}

// ─── init / start ────────────────────────────────────────────────────────────

static void reset_state(void) {
    dino_y = GY - DH; dvy = 0; grounded = true; duck = false;
    squash = 0; jump_buf = 0; flash = 0; score_pop = 0;
    scroll = 0; spawn_t = 0; next_spawn = 60;
    trail_head = 0;
    for (int i = 0; i < TRAIL; i++) { trail_y[i] = GY - DH; trail_air[i] = false; }
    for (int i = 0; i < MAX_OBS; i++) obs_off(&obs[i]);
    for (int i = 0; i < 24; i++) dust[i].life = 0;
}

void init(void) {
    hiscore = load(0);
    state = ST_IDLE;
    score = 0; speed = 0;
    reset_state();
    for (int i = 0; i < MAX_CLOUDS; i++) cloud_reset(&clouds[i], false);
}

static void start(void) {
    state = ST_PLAY;
    score = 0; speed = 3.5f;
    reset_state();
}

// ─── update ──────────────────────────────────────────────────────────────────

// The engine feeds the mouse in as a synthetic touch, so on a touch device we
// drive everything off the keyboard + screen zones, and on desktop off the
// keyboard + mouse buttons — never both, or a low left-click hits the duck zone.
static bool on_touch_device(void) { return touch_ceiling() > 0; }

static bool press_jump(void) {
    bool keys = btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(0, BTN_UP)
             || btnp(1, BTN_A) || btnp(1, BTN_UP);
    if (on_touch_device())
        return keys || tapp(0, 0, SCREEN_W, GY - 28);   // tap upper screen = jump
    return keys || mouse_pressed(MOUSE_LEFT);            // left-click anywhere = jump
}

static bool hold_duck(void) {
    bool keys = btn(0, BTN_DOWN) || btn(1, BTN_DOWN);
    if (on_touch_device())
        return keys || tap(0, GY - 28, SCREEN_W, SCREEN_H - (GY - 28));  // hold low = duck
    return keys || mouse_down(MOUSE_RIGHT);              // right-click = duck
}

void update(void) {
    if (state != ST_PLAY) { if (press_jump()) start(); return; }

    score++;
    if (score > hiscore) { hiscore = score; save(0, hiscore); }
    if (score % 100 == 0) { strum(72, CHORD_MAJ, INSTR_SQUARE, 2, 28); score_pop = 10; }

    speed = 3.5f + score * 0.005f;
    if (speed > 10.0f) speed = 10.0f;
    scroll += speed;

    // tick particles
    for (int i = 0; i < 24; i++)
        if (dust[i].life > 0) {
            dust[i].x += dust[i].vx; dust[i].y += dust[i].vy;
            dust[i].vy += 0.12f; dust[i].life--;
        }

    squash = lerp(squash, 0.0f, 0.22f);

    // jump buffer: if jump pressed in air, hold it for 8 frames
    if (press_jump() && !grounded) jump_buf = 8;
    if (jump_buf > 0) jump_buf--;

    duck = hold_duck() && grounded;
    int cur_h = duck ? DH_DUCK : DH;
    bool was_gr = grounded;

    if (grounded) {
        dino_y = GY - (float)cur_h;
        dvy = 0;
        if (!duck && (press_jump() || jump_buf > 0)) do_jump();
    }
    if (!grounded) {
        dvy    += GRAV;
        dino_y += dvy;
        if (dino_y + cur_h >= GY) {
            dino_y = GY - (float)cur_h; dvy = 0; grounded = true;
            if (!was_gr) {                          // proper landing frame
                squash = 1.0f;
                spawn_dust(DX + 5, GY);
                shake(1.2f);
                if (jump_buf > 0) do_jump();        // buffered jump fires immediately on land
            }
        }
    }

    // record trail position
    trail_y[trail_head % TRAIL]   = dino_y;
    trail_air[trail_head % TRAIL] = !grounded;
    trail_head++;

    for (int i = 0; i < MAX_CLOUDS; i++) {
        clouds[i].x -= speed * 0.22f;
        if (clouds[i].x + clouds[i].w < 0) cloud_reset(&clouds[i], true);
    }

    if (++spawn_t >= next_spawn) {
        do_spawn(); spawn_t = 0;
        float gap = 72.0f / (speed / 3.5f);
        if (gap < 30) gap = 30;
        next_spawn = rnd_float_between(gap, gap * 1.8f);
    }

    int dyi = (int)dino_y, m = 2;
    for (int i = 0; i < MAX_OBS; i++) {
        if (obs[i].type < 0) continue;
        float prev_rx = obs[i].x + obs[i].w - m;   // inner right edge before moving
        obs[i].x -= speed;
        float curr_rx = obs[i].x + obs[i].w - m;

        if (obs[i].x + obs[i].w < 0) { obs_off(&obs[i]); continue; }

        // near miss: obstacle inner right just cleared dino inner left
        if (prev_rx >= DX + m && curr_rx < DX + m) {
            int obs_bot = (int)obs[i].y + obs[i].h - m;
            int obs_top = (int)obs[i].y + m;
            int din_top = dyi + m;
            int din_bot = dyi + cur_h - m;
            if (obs_bot + 10 >= din_top && obs_top - 10 <= din_bot)
                hit(60 + rnd_between(-2, 2), INSTR_SAW, 1, 45);  // quiet whoosh
        }

        if (boxes_touch(DX+m, dyi+m, DW-m*2, cur_h-m*2,
                        (int)obs[i].x+m, (int)obs[i].y+m, obs[i].w-m*2, obs[i].h-m*2)) {
            state = ST_DEAD; flash = 6;
            shake(5);
            hit(36, INSTR_NOISE, 5, 200);
            return;
        }
    }
}

// ─── draw helpers ─────────────────────────────────────────────────────────────

static void draw_cactus(int x, int y, int t) {
    int c = CLR_DARK_GREY;
    if (t == 0) {
        rectfill(x+3, y,    4, 24, c);
        rectfill(x,   y+7,  4,  2, c);  rectfill(x,   y+5,  2, 6, c);
        rectfill(x+7, y+11, 3,  2, c);  rectfill(x+8, y+9,  2, 5, c);
    } else if (t == 1) {
        rectfill(x+5,  y,    4, 32, c);
        rectfill(x,    y+9,  6,  2, c);  rectfill(x,    y+7,  2, 7, c);
        rectfill(x+9,  y+13, 5,  2, c);  rectfill(x+12, y+11, 2, 8, c);
    } else {
        rectfill(x+3,  y+2,  4, 24, c);
        rectfill(x,    y+9,  4,  2, c);  rectfill(x,    y+7,  2, 6, c);
        rectfill(x+7,  y+13, 3,  2, c);  rectfill(x+8,  y+11, 2, 5, c);
        rectfill(x+18, y,    4, 26, c);
        rectfill(x+14, y+8,  5,  2, c);  rectfill(x+14, y+5,  2, 8, c);
        rectfill(x+22, y+12, 4,  2, c);  rectfill(x+24, y+10, 2, 7, c);
    }
}

static void draw_bird(int x, int y, int wing) {
    int c = CLR_DARK_GREY;
    rectfill(x+5,  y+4, 10, 6, c);
    rectfill(x+13, y+1,  6, 5, c);
    rectfill(x+17, y+2,  3, 2, c);
    if (wing == 0) rectfill(x, y,   14, 5, c);
    else           rectfill(x, y+8, 14, 5, c);
}

// sq: >0=squash (land), <0=stretch (takeoff)
static void draw_dino(int x, int y, bool ducking, int leg, float sq) {
    int c = CLR_DARK_GREY;
    if (ducking) {
        rectfill(x,    y+2, 18, 10, c);
        rectfill(x+11, y,    7,  7, c);
        pset(x+16, y+2, CLR_WHITE);
        rectfill(x+3,  y+10, 3, 3, c);
        rectfill(x+10, y+10, 3, 3, c);
        return;
    }
    int sp  = (int)(sq * 3.0f);          // -3..3 px
    int hdy = (sp > 0) ? sp / 2 : 0;    // head shifts down on squash
    int ext = (sp < 0) ? -sp : 0;       // extra body height on stretch

    rectfill(x+3, y+hdy,        9, 9, c);
    pset(x+10, y+hdy+2, CLR_WHITE);
    rectfill(x+5, y+hdy+7,      5, 3, c);
    rectfill(x+1, y+8+hdy,  12, 12+ext, c);
    rectfill(x-3, y+9+hdy,   5,  4, c);
    rectfill(x-5, y+11+hdy,  3,  3, c);
    rectfill(x+4, y+15+hdy,  5,  2, c);
    int ldy = hdy + ext;
    if (leg == 0) { rectfill(x+3, y+20+ldy, 3, 5, c); rectfill(x+9, y+20+ldy, 3, 2, c); }
    else          { rectfill(x+3, y+20+ldy, 3, 2, c); rectfill(x+9, y+20+ldy, 3, 5, c); }
}

// ─── draw ────────────────────────────────────────────────────────────────────

void draw(void) {
    cls(CLR_LIGHT_GREY);

    // clouds
    for (int i = 0; i < MAX_CLOUDS; i++) {
        int cx = (int)clouds[i].x, cy = (int)clouds[i].y, cw = clouds[i].w;
        rectfill(cx+4,  cy,   cw-8,  5, CLR_WHITE);
        rectfill(cx+8, cy-3,  cw-16, 5, CLR_WHITE);
        rectfill(cx,   cy+2,  cw,    4, CLR_WHITE);
    }

    // speed lines — appear as speed climbs past 5, more numerous toward 10
    if (speed > 5.0f && state == ST_PLAY) {
        float vis = (speed - 5.0f) / 5.0f;
        if (vis > 1.0f) vis = 1.0f;
        int n = 1 + (int)(vis * 4);              // 1..5 active tracks
        int ys[5]   = { 60, 82, 105, 125, 145 };
        int lens[5] = { 10, 16,   8,  14,  12 };
        int gaps[5] = {100, 90, 110,  80, 100 };
        int fscr = (int)(scroll * 1.7f);
        for (int i = 0; i < n; i++) {
            int span = gaps[i] + lens[i];
            int x0   = -(fscr % span);
            for (int x = x0; x < SCREEN_W; x += span)
                if (x > -lens[i])
                    line(x, ys[i], x + lens[i], ys[i], CLR_MEDIUM_GREY);
        }
    }

    // floor
    rectfill(0, GY, SCREEN_W, SCREEN_H - GY, CLR_MEDIUM_GREY);
    line(0, GY, SCREEN_W, GY, CLR_DARK_GREY);
    int gsp = 26, goff = (int)scroll % gsp;
    for (int x = -goff; x < SCREEN_W; x += gsp)
        line(x, GY+3, x+10, GY+3, CLR_DARK_GREY);

    // obstacles
    for (int i = 0; i < MAX_OBS; i++) {
        if (obs[i].type < 0) continue;
        int ox = (int)obs[i].x, oy = (int)obs[i].y;
        if (obs[i].type == 3) draw_bird(ox, oy, (frame() / 8) % 2);
        else                  draw_cactus(ox, oy, obs[i].type);
    }

    // dust particles
    for (int i = 0; i < 24; i++)
        if (dust[i].life > 0) pset((int)dust[i].x, (int)dust[i].y, dust[i].col);

    // motion trail — faded dino ghost at recent airborne positions
    for (int i = 1; i < TRAIL; i++) {
        int idx = ((trail_head - 1 - i) % TRAIL + TRAIL) % TRAIL;
        if (!trail_air[idx]) continue;
        int ty  = (int)trail_y[idx];
        int col = (i <= 2) ? CLR_MEDIUM_GREY : CLR_LIGHT_GREY;
        rectfill(DX+4, ty,    6, 8, col);   // faded head
        rectfill(DX+2, ty+8, 10, 8, col);   // faded body
    }

    // dino
    int leg = (grounded && !duck) ? (frame() / 6) % 2 : 0;
    draw_dino(DX, (int)dino_y, duck, leg, squash);

    // score HUD
    font(FONT_SMALL);
    print(str("HI %05d", hiscore), SCREEN_W - 100, 6, CLR_DARK_GREY);
    if (score_pop > 0) {
        rectfill(SCREEN_W - 50, 3, 48, 11, CLR_DARK_GREY);
        print(str("%05d", score), SCREEN_W - 42, 6, CLR_WHITE);
        score_pop--;
    } else {
        print(str("%05d", score), SCREEN_W - 42, 6, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    if (state == ST_IDLE) {
        print_centered("DINO RUN", SCREEN_W/2, 74, CLR_DARK_GREY);
        font(FONT_SMALL);
        print_centered("Z / UP / CLICK  jump     DOWN / R-CLICK  duck", SCREEN_W/2, 90, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // game over panel — only after death flash finishes
    if (state == ST_DEAD && flash == 0) {
        rrectfill(SCREEN_W/2 - 68, 78, 136, 40, 4, CLR_LIGHT_GREY);
        rrect    (SCREEN_W/2 - 68, 78, 136, 40, 4, CLR_DARK_GREY);
        print_centered("GAME OVER", SCREEN_W/2, 88, CLR_DARK_GREY);
        font(FONT_SMALL);
        if (blink(20)) print_centered("press Z or click to run again", SCREEN_W/2, 102, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // soft death tint — fillp overlay builds from 25% to solid white over 6 frames
    if (flash > 0) {
        if (flash >= 4) {
            int patterns[3] = { 0x1111, 0xAAAA, 0xEEEE };  // 25%, 50%, 75%
            int idx = 6 - flash;                             // 0, 1, 2 as flash counts 6→4
            fillp(patterns[idx], -1);
            rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
            fillp_reset();
        } else {
            rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);  // solid white for last 3 frames
        }
        flash--;
    }
}
