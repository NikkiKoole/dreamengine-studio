/* de:meta
{
  "title": "Worms",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "noise-terrain",
    "state-machine",
    "particle-system",
    "title-play-gameover-loop"
  ],
  "lineage": "Homage to Worms (1995), surpassing the Gorillas lineage by adding destructible pixel terrain carved at runtime and physics-driven worm settling.",
  "genre": "tactics",
  "homage": "Worms (1995)",
  "description": "A 2-player turn-based artillery duel on fully destructible pixel terrain — the genre Gorillas only half-shipped. Two worms perch on a rolling noise-generated landscape; on your turn you walk along the ground, sweep your bazooka's aim, then hold to charge a swinging power meter and release to fire. The rocket arcs under gravity and a live wind that actually bends the shot, then blows a real crater into the ground (carved pixel by pixel) and damages anyone in the blast — worms even tumble when the dirt under them is gone. First to knock the other's HP to zero wins. P1 LEFT/RIGHT walk, UP/DOWN aim, hold SPACE to charge power and release to fire; SPACE to restart after a win.",
  "todo": [
    "The explosions need juice (particles, smoke dithering — see boom for inspiration).",
    "The movement sound is annoying — make it a simple shuffling sound instead.",
    "Add a simple chiptune background."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// WORMS — 2-player turn-based artillery on DESTRUCTIBLE pixel terrain.
//
// Surpasses gorillas: you WALK along the ground, the terrain is real pixels you
// can blow craters in, and the wind bends the rocket. The battlefield is a solid/
// empty pixel mask (one byte per screen pixel) generated from layered noise; the
// explosion carves a circle of that mask with pset and reads it back to settle worms.
//
//   LEFT/RIGHT walk   UP/DOWN aim   hold SPACE charge power, release to FIRE
//   first to knock the other worm to 0 HP wins   SPACE/Z restart

#define W            SCREEN_W
#define H            SCREEN_H
#define HUD_H        16
#define GRAV         140.0f      // px/s^2
#define WORM_W       6
#define WORM_H       8
#define MAX_HP       100
#define BLAST_R      18
#define WALK_SPD     34.0f       // px/s
#define MAX_CLIMB    6           // steepest step the worm can walk over per column

// terrain pixel mask: 1 = solid ground, 0 = air. one byte per screen pixel.
static unsigned char solid[W * H];

// per-team
static float wormx[2], wormy[2];     // worm foot position (float for physics)
static int   hp[2];
static int   facing[2];              // -1 left, +1 right
static int   cur;                    // whose turn (0/1)
static int   teamcol[2] = { CLR_GREEN, CLR_PINK };

// aim / power
static float aim;                    // degrees, 0 = right, sweeps up to ~85
static float power;                  // 0..1 charge
static int   charging;
static float swingdir;               // power meter swings up/down while charging

// projectile
static int   flying;
static float px, py, pvx, pvy;
static float trailx[16], traily[16]; // smoke trail ring
static int   traili;

// explosion fx
static int   booming;
static float boomx, boomy, boomt, boomr;

static float wind;                   // px/s^2 horizontal, per round

// 0 aim/walk/charge  1 flight  2 settle-pause  3 gameover
static int   gstate;
static float statet;                 // timer in current settle/gameover

// ---- terrain helpers ----------------------------------------------------

static int is_solid(int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H) return 0;
    return solid[y * W + x];
}

// topmost solid y in a column at/below ystart; H if none
static int ground_below(int x, int ystart) {
    if (x < 0 || x >= W) return H;
    for (int y = max(ystart, HUD_H); y < H; y++)
        if (solid[y * W + x]) return y;
    return H;
}

static int surface_y(int x) { return ground_below(x, HUD_H); }

static void gen_terrain(void) {
    float seed = rnd_float() * 100.0f;
    for (int i = 0; i < W * H; i++) solid[i] = 0;
    for (int x = 0; x < W; x++) {
        float n = noise2(x * 0.012f + seed, seed * 0.3f);
        n += 0.5f * noise2(x * 0.045f + seed, 7.0f);
        n += 0.25f * noise2(x * 0.11f + seed, 21.0f);
        n /= 1.75f;                                  // ~0..1
        int top = (int)(H * 0.40f + n * (H * 0.45f));
        top = mid(HUD_H + 30, top, H - 6);
        for (int y = top; y < H; y++) solid[y * W + x] = 1;
    }
}

// carve a circular crater into the mask
static void carve(int cx, int cy, int r) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y > r * r) continue;
            int gx = cx + x, gy = cy + y;
            if (gx >= 0 && gx < W && gy >= HUD_H && gy < H) solid[gy * W + gx] = 0;
        }
}

// ---- worms --------------------------------------------------------------

static void settle_worm(int t) {
    int x = (int)wormx[t];
    int g = ground_below(x, (int)wormy[t] - 2);
    if (g >= H) {                       // fell off the world bottom
        hp[t] = 0;
        return;
    }
    wormy[t] = (float)g;
}

static void place_worms(void) {
    int x0 = W / 6, x1 = W - W / 6;
    wormx[0] = (float)x0; wormy[0] = (float)surface_y(x0);
    wormx[1] = (float)x1; wormy[1] = (float)surface_y(x1);
    facing[0] = +1; facing[1] = -1;
}

// ---- round / game -------------------------------------------------------

static void new_round(void) {
    gen_terrain();
    place_worms();
    wind = rnd_float_between(-60.0f, 60.0f);
    flying = booming = charging = 0;
    aim = 50.0f; power = 0.0f;
    gstate = 0;
}

static void new_game(void) {
    hp[0] = hp[1] = MAX_HP;
    cur = 0;
    new_round();
}

void init(void) {
    colorkey(0);
    instrument(5, INSTR_SINE, 4, 60, 4, 120);     // launch whoosh
    instrument_lfo(5, 0, LFO_PITCH, 6.0f, 0.0f);
    new_game();
}

static void fire(void) {
    int dir = facing[cur];
    float a = (dir > 0) ? aim : 180.0f - aim;     // mirror for left-facers
    px = wormx[cur];
    py = wormy[cur] - WORM_H;
    float speed = 70.0f + power * 200.0f;
    pvx = cos_deg(a) * speed;
    pvy = -sin_deg(a) * speed;
    flying = 1;
    charging = 0;
    for (int i = 0; i < 16; i++) { trailx[i] = px; traily[i] = py; }
    traili = 0;
    gstate = 1;
    note(72, 5, 4);                                // whoosh
}

static void explode(float x, float y) {
    boomx = x; boomy = y; boomt = now(); booming = 1; boomr = 0;
    carve((int)x, (int)y, BLAST_R);
    flying = 0;
    shake(5.0f);
    note(36, INSTR_NOISE, 7);
    hit(28, INSTR_NOISE, 6, 220);

    // radial damage
    for (int t = 0; t < 2; t++) {
        float d = distance((int)x, (int)y, (int)wormx[t], (int)wormy[t] - WORM_H / 2);
        if (d < BLAST_R + 4) {
            int dmg = (int)remap(d, 0, BLAST_R + 4, 45, 0);
            if (dmg < 0) dmg = 0;
            hp[t] -= dmg;
            if (hp[t] < 0) hp[t] = 0;
            // a little knockback away from the blast
            if (d > 0.5f) {
                float kb = remap(d, 0, BLAST_R + 4, 8, 0);
                wormx[t] += (wormx[t] - x) / d * kb;
                wormx[t] = clamp(wormx[t], 4, W - 4);
            }
        }
    }
    // settle both worms onto the (now changed) ground
    settle_worm(0);
    settle_worm(1);

    gstate = (hp[0] <= 0 || hp[1] <= 0) ? 3 : 2;
    statet = now();
}

// ---- update -------------------------------------------------------------

static void aim_walk_update(void) {
    float t = dt();
    // aim
    if (btn(cur, BTN_UP))   aim = clamp(aim + 60.0f * t, 5.0f, 85.0f);
    if (btn(cur, BTN_DOWN)) aim = clamp(aim - 60.0f * t, 5.0f, 85.0f);

    // walk along the surface
    int mv = 0;
    if (btn(cur, BTN_LEFT))  { mv = -1; facing[cur] = -1; }
    if (btn(cur, BTN_RIGHT)) { mv = +1; facing[cur] = +1; }
    if (mv != 0 && !charging) {
        float nx = wormx[cur] + mv * WALK_SPD * t;
        nx = clamp(nx, 4, W - 4);
        int gy = surface_y((int)nx);
        if (gy < H) {
            // refuse to climb a wall steeper than MAX_CLIMB
            if ((int)wormy[cur] - gy <= MAX_CLIMB) {
                wormx[cur] = nx;
                wormy[cur] = (float)gy;
                if (blink(4)) note(60, INSTR_SQUARE, 1);   // soft footstep tick
            }
        }
    }

    // charge power: hold A/Space, swing the meter, release to fire
    bool press = btn(cur, BTN_A) || key(KEY_SPACE);
    if (press) {
        if (!charging) { charging = 1; power = 0.0f; swingdir = 1.0f; }
        power += swingdir * 0.9f * t;
        if (power >= 1.0f) { power = 1.0f; swingdir = -1.0f; }
        if (power <= 0.0f) { power = 0.0f; swingdir = 1.0f; }
    } else if (charging) {
        fire();
    }
}

static void flight_update(void) {
    float t = dt();
    pvy += GRAV * t;
    pvx += wind * t;
    px  += pvx * t;
    py  += pvy * t;

    // record trail
    traili = (traili + 1) % 16;
    trailx[traili] = px; traily[traili] = py;

    int ix = (int)px, iy = (int)py;

    // off the sides → dud, pass turn
    if (ix < -20 || ix > W + 20) {
        flying = 0; cur = 1 - cur; aim = 50.0f; power = 0.0f; gstate = 0;
        return;
    }
    // direct worm hit (opponent or self — full radius applies anyway)
    for (int tm = 0; tm < 2; tm++) {
        if (near(ix, iy, (int)wormx[tm], (int)wormy[tm] - WORM_H / 2, 5)) {
            explode(px, py); return;
        }
    }
    // ground hit
    if (iy >= HUD_H && is_solid(ix, iy)) { explode(px, py); return; }
    // dropped below the world
    if (iy > H) { explode(px, (float)(H - 2)); return; }
}

void update(void) {
    if (gstate == 3) {
        if (now() - statet > 0.8f && (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp(KEY_SPACE)))
            new_game();
        return;
    }
    if (gstate == 0) { aim_walk_update(); return; }
    if (gstate == 1) { flight_update(); return; }
    if (gstate == 2) {
        if (now() - statet > 0.6f) {
            booming = 0;
            cur = 1 - cur; aim = 50.0f; power = 0.0f;
            settle_worm(cur);
            gstate = 0;
        }
    }
}

// ---- draw ---------------------------------------------------------------

static void draw_terrain(void) {
    // shade by depth-from-surface for a little dimension; cheap per-column
    for (int x = 0; x < W; x++) {
        int top = -1;
        for (int y = HUD_H; y < H; y++) {
            if (!solid[y * W + x]) { top = -1; continue; }
            if (top < 0) top = y;
            int depth = y - top;
            int c = (depth < 2) ? CLR_LIME_GREEN
                  : (depth < 5) ? CLR_DARK_GREEN
                  : (depth < 14) ? CLR_BROWN
                  : CLR_DARK_BROWN;
            pset(x, y, c);
        }
    }
}

static void draw_worm(int t) {
    int x = (int)wormx[t], y = (int)wormy[t];
    int col = teamcol[t];
    // body
    rectfill(x - WORM_W / 2, y - WORM_H, WORM_W, WORM_H, col);
    rectfill(x - WORM_W / 2, y - WORM_H, WORM_W, 3, CLR_WHITE);   // light top
    // eye looking the way it faces
    int ex = (facing[t] > 0) ? x + 1 : x - 1;
    pset(ex, y - WORM_H + 2, CLR_BLACK);
    // tiny HP pip above
    int barw = 14;
    bar(x - barw / 2, y - WORM_H - 6, barw, 2,
        (float)hp[t] / MAX_HP, (hp[t] > 35 ? CLR_GREEN : CLR_RED), CLR_DARKER_GREY);
}

static void draw_bazooka(void) {
    if (gstate != 0) return;
    int x = (int)wormx[cur], y = (int)wormy[cur] - WORM_H / 2 - 1;
    int dir = facing[cur];
    float a = (dir > 0) ? aim : 180.0f - aim;
    int bx = x + (int)dx(8, a);
    int by = y - (int)(sin_deg(a) * 8.0f);
    line(x, y, bx, by, CLR_LIGHT_GREY);
    line(x, y - 1, bx, by - 1, CLR_WHITE);

    // dotted predictive trajectory
    float sx = (float)x, sy = (float)y;
    float speed = 70.0f + power * 200.0f;
    float vx = cos_deg(a) * speed;
    float vy = -sin_deg(a) * speed;
    float st = 0.02f;
    for (int i = 0; i < 60; i++) {
        vy += GRAV * st; vx += wind * st;
        sx += vx * st;   sy += vy * st;
        if (sx < 0 || sx > W || sy > H) break;
        if (i % 4 == 0) pset((int)sx, (int)sy, CLR_LIGHT_YELLOW);
    }
}

static void draw_hud(void) {
    rectfill(0, 0, W, HUD_H, CLR_BLACK);
    line(0, HUD_H, W, HUD_H, CLR_DARK_GREY);

    // HP bars per team
    bar(4, 3, 60, 4, (float)hp[0] / MAX_HP, CLR_GREEN, CLR_DARKER_GREY);
    print("P1", 4, 9, cur == 0 && gstate == 0 ? CLR_WHITE : CLR_DARK_GREY);
    bar(W - 64, 3, 60, 4, (float)hp[1] / MAX_HP, CLR_PINK, CLR_DARKER_GREY);
    print_right("P2", W - 4, 9, cur == 1 && gstate == 0 ? CLR_WHITE : CLR_DARK_GREY);

    // wind indicator (centered)
    int wmid = W / 2;
    print_centered("WIND", SCREEN_W/2, 2, CLR_DARK_GREY);
    int wlen = (int)(wind * 0.45f);
    if (wlen != 0) {
        line(wmid, 11, wmid + wlen, 11, CLR_BLUE);
        int tip = wmid + wlen;
        int d = (wlen > 0) ? -1 : 1;
        pset(tip + d, 10, CLR_LIGHT_GREY);
        pset(tip + d, 12, CLR_LIGHT_GREY);
        pset(tip, 11, CLR_WHITE);
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // sky stars (deterministic, above terrain)
    for (int i = 0; i < 24; i++) {
        int sx = (i * 137 + 11) % W;
        int sy = HUD_H + 2 + (i * 41 + 7) % 40;
        pset(sx, sy, (i % 4 == 0) ? CLR_INDIGO : CLR_DARKER_BLUE);
    }

    draw_terrain();
    draw_worm(0);
    draw_worm(1);
    draw_bazooka();

    // projectile + smoke trail
    if (flying) {
        for (int i = 0; i < 16; i++) {
            int j = (traili - i + 32) % 16;
            int c = (i < 4) ? CLR_LIGHT_GREY : (i < 9) ? CLR_DARK_GREY : CLR_DARKER_GREY;
            pset((int)trailx[j], (int)traily[j], c);
        }
        circfill((int)px, (int)py, 2, CLR_ORANGE);
        pset((int)px, (int)py, CLR_YELLOW);
    }

    // explosion
    if (booming) {
        float age = now() - boomt;
        int r = min((int)(age * 90.0f), BLAST_R);
        if (r > 0) {
            circfill((int)boomx, (int)boomy, r, CLR_DARK_ORANGE);
            circ((int)boomx, (int)boomy, r, CLR_YELLOW);
            circfill((int)boomx, (int)boomy, r * 2 / 3, CLR_ORANGE);
            circfill((int)boomx, (int)boomy, r / 3, CLR_WHITE);
        }
    }

    draw_hud();

    // bottom status / power meter while aiming
    if (gstate == 0) {
        int bc = teamcol[cur];
        rectfill(0, H - 11, W, 11, CLR_BLACK);
        print(str("P%d  ANGLE %d", cur + 1, (int)aim), 4, H - 9, bc);
        // power meter
        int pw = 90;
        print_right(charging ? "RELEASE!" : "hold SPACE", W - pw - 8, H - 9,
                    charging ? CLR_YELLOW : CLR_DARK_GREY);
        bar(W - pw - 4, H - 9, pw, 6, power,
            power > 0.75f ? CLR_RED : CLR_ORANGE, CLR_DARKER_GREY);
    }

    // game over
    if (gstate == 3 && now() - statet > 0.8f) {
        int w = (hp[0] > 0) ? 1 : 2;
        fade(0.3f);
        rectfill(W / 2 - 78, H / 2 - 20, 156, 42, CLR_BLACK);
        rect(W / 2 - 78, H / 2 - 20, 156, 42, teamcol[w - 1]);
        print_centered(str("PLAYER %d WINS!", w), SCREEN_W/2, H / 2 - 10, teamcol[w - 1]);
        print_centered("SPACE to play again", SCREEN_W/2, H / 2 + 6, CLR_LIGHT_GREY);
    }
}
