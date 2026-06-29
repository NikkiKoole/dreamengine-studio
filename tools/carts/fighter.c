/* de:meta
{
  "title": "street brawler",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop"
  ],
  "lineage": "Sibling of streetfighter.c; the distinctive addition is the two-player 1v1 frame-data system (startup/active/recovery) and the input-buffer quarter-circle motion recogniser.",
  "genre": "fighting",
  "description": "A 2-player 1v1 fighting game. Each fighter is a state machine — walk, jump, crouch, block (hold away), punch, kick — and attacks have startup/active/recovery frames so spacing and timing decide the match. The fireball is a real quarter-circle motion (down, down-forward, forward + punch) read from an input buffer, the trick behind every fighting-game special. Best of 3. P1: WASD + Z/X. P2: arrows + comma/period."
}
de:meta */
#include "studio.h"
#include <math.h>

// STREET BRAWLER — a two-player 1-v-1 fighting game.
//
// Each fighter is a little STATE MACHINE: idle / walk / jump / crouch / attack /
// blockstun / hitstun. Holding away from your opponent BLOCKS. An attack has
// three phases — startup, active (the hitbox is live), recovery — so timing and
// spacing decide everything, just like a real fighter.
//
// The special move is a HADOUKEN, thrown with a "quarter-circle forward": roll
// down → down-toward-them → toward-them, then punch. The game watches your
// recent inputs (an input buffer) to recognise the motion — that's the trick
// behind every fireball in every fighting game.
//
//   P1: WASD move, W jump, S crouch, Z punch, X kick   (down→fwd→Z = fireball)
//   P2: arrows move, ↑ jump, ↓ crouch, , punch, . kick
//
//   Best of 3 rounds. After the match, Z or , for a rematch.

#define GY        168          // ground (feet) line
#define STAGE_L   18
#define STAGE_R   302
#define SPD       1.7f
#define JUMP      4.8f
#define GRAV      0.30f
#define KNOCK     3.2f
#define HITSTUN   15
#define BLOCKSTUN 8
#define MINSEP    20
#define ROUND_TIME (60 * 60)
#define WIN_ROUNDS 2

enum { ATK_NONE, ATK_PUNCH, ATK_KICK, ATK_FIRE };
enum { G_INTRO, G_FIGHT, G_ROUNDEND, G_MATCHEND };

typedef struct {
    int   p;                    // player index for btn()
    float x, y, vx, vy;
    int   facing;               // +1 right, -1 left
    int   hp, wins;
    int   atk, af;              // attack id + frame counter
    bool  hit_done, blocking, crouch, onground;
    int   stun;
    bool  fb_on; float fb_x, fb_y; int fb_dir;
    int   t_down, t_fwd;        // last frame these inputs were held (for motions)
} Fighter;

static Fighter p1, p2;
static int   gphase, gtimer, round_n, round_clock;
static const char *result = "";
static const char *winner = "";

typedef struct { float x, y; int t; } Spark;
static Spark sparks[10];

static void spark(float x, float y) {
    for (int i = 0; i < 10; i++) if (sparks[i].t <= 0) { sparks[i] = (Spark){ x, y, 10 }; return; }
}

static void reset_fighter(Fighter *f, int p, float x, int facing) {
    f->p = p; f->x = x; f->y = GY; f->vx = f->vy = 0; f->facing = facing;
    f->hp = 100; f->atk = ATK_NONE; f->af = 0;
    f->hit_done = f->blocking = f->crouch = false; f->onground = true;
    f->stun = 0; f->fb_on = false; f->t_down = f->t_fwd = -999;
}

static void start_round(void) {
    reset_fighter(&p1, 0, 100, +1);
    reset_fighter(&p2, 1, 220, -1);
    round_clock = ROUND_TIME;     // so the clock reads full during the intro
    gphase = G_INTRO; gtimer = 90;
}

static void new_match(void) {
    p1.wins = p2.wins = 0; round_n = 1;
    start_round();
}

void init(void) { new_match(); }

// ── input + decisions ──
static void fighter_act(Fighter *f, Fighter *o) {
    if (f->stun > 0) { f->crouch = f->blocking = false; return; }
    if (f->atk != ATK_NONE) { f->crouch = f->blocking = false; return; }  // locked in a move

    int p = f->p;
    if (f->onground && o->x != f->x) f->facing = (o->x > f->x) ? 1 : -1;

    bool fwd = f->facing > 0 ? btn(p, BTN_RIGHT) : btn(p, BTN_LEFT);
    bool bak = f->facing > 0 ? btn(p, BTN_LEFT)  : btn(p, BTN_RIGHT);
    bool dn  = btn(p, BTN_DOWN);
    if (dn)  f->t_down = frame();
    if (fwd) f->t_fwd  = frame();

    if (f->onground) {
        if (btnp(p, BTN_UP)) { f->vy = -JUMP; f->onground = false; f->crouch = false; return; }
        f->crouch   = dn;
        f->blocking = bak && !dn;
        if (!dn)      f->vx = fwd && !bak ? SPD * f->facing : bak && !fwd ? -SPD * f->facing : 0;
        else          f->vx = 0;

        if (btnp(p, BTN_A)) {
            bool qcf = !f->fb_on && (frame() - f->t_down <= 16)
                       && (f->t_fwd >= f->t_down) && (frame() - f->t_fwd <= 10);
            f->atk = qcf ? ATK_FIRE : ATK_PUNCH; f->af = 0; f->vx = 0;
        } else if (btnp(p, BTN_B)) { f->atk = ATK_KICK; f->af = 0; f->vx = 0; }
    } else {
        f->vx = fwd ? SPD * 0.6f * f->facing : bak ? -SPD * 0.6f * f->facing : f->vx;
        f->crouch = f->blocking = false;
    }
}

static void apply_hit(Fighter *f, Fighter *o, int dmg) {
    if (o->blocking && o->onground) {
        o->stun = BLOCKSTUN; o->hp -= (dmg > 4 ? 1 : 0); o->vx = f->facing * 1.3f;
        hit(72, INSTR_NOISE, 2, 35);
    } else {
        o->hp -= dmg; o->stun = HITSTUN; o->vx = f->facing * KNOCK;
        o->atk = ATK_NONE; o->crouch = false;
        spark(o->x, o->y - 28);
        hit(dmg > 8 ? 44 : 52, INSTR_NOISE, 4, dmg > 8 ? 90 : 55);
    }
    if (o->hp < 0) o->hp = 0;
}

// ── active-frame hit detection + fireball spawn ──
static void fighter_resolve(Fighter *f, Fighter *o) {
    if (f->atk == ATK_NONE) return;
    bool active = false; int reach = 0, ayoff = 0, dmg = 0;
    if (f->atk == ATK_PUNCH) { active = f->af >= 3 && f->af <= 5;  reach = 18; ayoff = 32; dmg = 6; }
    else if (f->atk == ATK_KICK) { active = f->af >= 6 && f->af <= 9; reach = 27; ayoff = 18; dmg = 11; }
    else if (f->atk == ATK_FIRE && f->af == 10 && !f->fb_on) {
        f->fb_on = true; f->fb_x = f->x + f->facing * 12; f->fb_y = f->y - 30; f->fb_dir = f->facing;
        note(57, INSTR_SAW, 4);
    }
    if (active && !f->hit_done) {
        int ax = f->facing > 0 ? (int)f->x + 6 : (int)f->x - 6 - reach;
        int oh = o->crouch ? 24 : 42, ob = (int)o->y - oh;
        if (boxes_touch(ax, (int)f->y - ayoff, reach, 12, (int)o->x - 7, ob, 14, oh)) {
            f->hit_done = true; apply_hit(f, o, dmg);
        }
    }
}

static void fighter_phys(Fighter *f, Fighter *o) {
    if (f->stun > 0) f->stun--;
    if (f->atk != ATK_NONE) {
        f->af++;
        int len = f->atk == ATK_PUNCH ? 12 : f->atk == ATK_KICK ? 21 : 24;
        if (f->af > len) { f->atk = ATK_NONE; f->hit_done = false; }
    }
    if (!f->onground) {
        f->vy += GRAV; f->y += f->vy;
        if (f->y >= GY) { f->y = GY; f->onground = true; f->vy = 0; }
    }
    f->x += f->vx;
    if (f->stun > 0 || !f->onground) f->vx *= 0.86f;
    f->x = clamp(f->x, STAGE_L, STAGE_R);

    float d = f->x - o->x;
    if (fabsf(d) < MINSEP) {
        float push = (MINSEP - fabsf(d)) / 2; int s = d < 0 ? -1 : 1;
        f->x = clamp(f->x + s * push, STAGE_L, STAGE_R);
        o->x = clamp(o->x - s * push, STAGE_L, STAGE_R);
    }
}

static void fireball_update(Fighter *f, Fighter *o) {
    if (!f->fb_on) return;
    f->fb_x += f->fb_dir * 3.3f;
    if (f->fb_x < -12 || f->fb_x > SCREEN_W + 12) { f->fb_on = false; return; }
    int oh = o->crouch ? 24 : 42, ob = (int)o->y - oh;
    if (boxes_touch((int)f->fb_x - 5, (int)f->fb_y - 4, 10, 8, (int)o->x - 7, ob, 14, oh)) {
        if (o->blocking && o->onground) { o->stun = BLOCKSTUN; o->hp -= 2; o->vx = f->fb_dir * 1.6f; }
        else { o->hp -= 9; o->stun = HITSTUN + 4; o->vx = f->fb_dir * KNOCK; o->atk = ATK_NONE; spark(o->x, o->y - 28); }
        if (o->hp < 0) o->hp = 0;
        f->fb_on = false; hit(40, INSTR_NOISE, 5, 140);
    }
}

static void end_round(void) {
    // higher HP takes the round — works for both a KO (loser at 0) and a timeout
    if (p1.hp > p2.hp)      { p1.wins++; winner = "PLAYER 1"; }
    else if (p2.hp > p1.hp) { p2.wins++; winner = "PLAYER 2"; }
    else                      winner = "DRAW";   // equal HP → nobody scores, round replays
    result = (p1.hp <= 0 || p2.hp <= 0) ? "K.O.!" : "TIME UP";
    gphase = G_ROUNDEND; gtimer = 150;
    hit(33, INSTR_TRI, 6, 260);
}

void update(void) {
    if (gphase == G_INTRO)    { if (--gtimer <= 0) { gphase = G_FIGHT; round_clock = ROUND_TIME; } return; }
    if (gphase == G_ROUNDEND) {
        if (--gtimer <= 0) {
            if (p1.wins >= WIN_ROUNDS || p2.wins >= WIN_ROUNDS) {
                winner = p1.wins > p2.wins ? "PLAYER 1" : "PLAYER 2"; gphase = G_MATCHEND;
            } else { round_n++; start_round(); }
        }
        return;
    }
    if (gphase == G_MATCHEND) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) new_match(); return; }

    fighter_act(&p1, &p2);     fighter_act(&p2, &p1);
    fighter_resolve(&p1, &p2); fighter_resolve(&p2, &p1);
    fighter_phys(&p1, &p2);    fighter_phys(&p2, &p1);
    fireball_update(&p1, &p2); fireball_update(&p2, &p1);
    for (int i = 0; i < 10; i++) if (sparks[i].t > 0) sparks[i].t--;

    if (round_clock > 0) round_clock--;
    if (p1.hp <= 0 || p2.hp <= 0 || round_clock <= 0) end_round();
}

// ── drawing ──
static void draw_fighter(Fighter *f, int col) {
    int fx = (int)f->x, footY = (int)f->y, face = f->facing;
    int hipY = footY - (f->crouch ? 9 : 18);
    int shY  = hipY - (f->crouch ? 12 : 16);
    int headY = shY - 5 + (f->stun > 0 ? 1 : 0);

    // legs
    if (f->atk == ATK_KICK && f->af >= 6 && f->af <= 9) {
        line(fx, hipY, fx + face * 26, hipY + 2, col); circfill(fx + face * 26, hipY + 2, 2, col);
        line(fx, hipY, fx - face * 6, footY, col);
    } else {
        line(fx, hipY, fx + face * 6, footY, col);
        line(fx, hipY, fx - face * 6, footY, col);
    }
    // torso + head
    rectfill(fx - 1, shY, 3, hipY - shY, col);
    circfill(fx, headY, 5, CLR_LIGHT_PEACH);
    pset(fx + face * 3, headY, CLR_BLACK);
    // arms
    if (f->atk == ATK_PUNCH && f->af >= 3 && f->af <= 5) {
        line(fx, shY + 2, fx + face * 18, shY + 2, col); circfill(fx + face * 18, shY + 2, 2, col);
        line(fx, shY + 2, fx - face * 5, shY + 9, col);
    } else if (f->atk == ATK_FIRE && f->af >= 8) {
        line(fx, shY + 3, fx + face * 12, shY + 6, col); circfill(fx + face * 12, shY + 6, 3, col);
        line(fx, shY + 3, fx + face * 11, shY + 9, col);
    } else if (f->blocking) {
        line(fx, shY + 2, fx + face * 6, shY - 2, col);
        line(fx + face * 6, shY - 2, fx + face * 6, shY + 8, col);
    } else {
        line(fx, shY + 2, fx + face * 5, shY + 9, col);
        line(fx, shY + 2, fx - face * 4, shY + 9, col);
    }

    if (f->fb_on) {
        int x = (int)f->fb_x, y = (int)f->fb_y;
        circfill(x - f->fb_dir * 4, y, 3, CLR_ORANGE);
        circfill(x, y, 5, CLR_LIGHT_YELLOW);
        circfill(x, y, 2, CLR_WHITE);
    }
}

static void health_bar(int hp, int x, int wins, bool left, const char *name) {
    int BW = 132, BH = 9;
    int col = hp > 50 ? CLR_GREEN : hp > 25 ? CLR_YELLOW : CLR_RED;
    int w = hp * (BW - 2) / 100;
    rect(x, 12, BW, BH, CLR_WHITE);
    if (left) rectfill(x + 1, 13, w, BH - 2, col);
    else      rectfill(x + 1 + (BW - 2 - w), 13, w, BH - 2, col);
    print(name, x, 2, CLR_WHITE);
    for (int i = 0; i < wins; i++)
        circfill(left ? x + 40 + i * 8 : x + BW - 44 - i * 8, 6, 2, CLR_YELLOW);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    circfill(160, 60, 34, CLR_DARK_RED);              // sunset
    rectfill(0, GY, SCREEN_W, SCREEN_H - GY, CLR_BROWN);
    line(0, GY, SCREEN_W, GY, CLR_DARK_BROWN);

    draw_fighter(&p1, CLR_BLUE);
    draw_fighter(&p2, CLR_RED);
    for (int i = 0; i < 10; i++)
        if (sparks[i].t > 0) circfill((int)sparks[i].x, (int)sparks[i].y, sparks[i].t / 2 + 1, CLR_WHITE);

    health_bar(p1.hp, 8, p1.wins, true, "P1");
    health_bar(p2.hp, SCREEN_W - 8 - 132, p2.wins, false, "P2");
    print_centered(str("%d", round_clock / 60), SCREEN_W/2, 2, CLR_WHITE);

    if (gphase == G_INTRO) {
        if (gtimer > 28) print_centered(str("ROUND %d", round_n), SCREEN_W/2, 88, CLR_YELLOW);
        else             print_centered("FIGHT!", SCREEN_W/2, 88, CLR_RED);
        print_centered("down-forward + punch = FIREBALL", SCREEN_W/2, SCREEN_H - 10, CLR_DARK_GREY);
    } else if (gphase == G_ROUNDEND) {
        print_centered(result, SCREEN_W/2, 84, CLR_WHITE);
        if (winner[0] != 'D') print_centered(str("%s wins the round", winner), SCREEN_W/2, 98, CLR_YELLOW);
    } else if (gphase == G_MATCHEND) {
        print_centered(str("%s WINS!", winner), SCREEN_W/2, 80, CLR_YELLOW);
        print_centered("press Z / , for a rematch", SCREEN_W/2, 98, CLR_WHITE);
    }
}
