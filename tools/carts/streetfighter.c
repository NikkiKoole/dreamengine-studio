/* de:meta
{
  "slug": "streetfighter",
  "title": "Street Fighter",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "finite-state-ai",
    "screen-shake-juice",
    "title-play-gameover-loop"
  ],
  "lineage": "Homage to Street Fighter II (1991); novel in implementing startup/active/recovery frame data, a quarter-circle input buffer, and a reactive whiff-punishing AI all within a single-file cart.",
  "genre": "fighting",
  "homage": "Street Fighter II (1991)",
  "description": "A 1-on-1 versus fighter on a dusk rooftop stage — you against a pushy, reactive CPU, with health bars up top, a round clock ticking between you, and a best-of-three. Each stick-fighter is a little state machine: walk in and out, jump, crouch, hold back to BLOCK, and throw a fast LIGHT, a heavy KNOCKBACK swing, or a quarter-circle-forward FIREBALL. Attacks have real startup/active/recovery, so spacing and timing decide everything; hits land with screen shake, a white hit-flash, a couple of frozen impact frames, and chip damage on block. The CPU reads the gap and blocks your swings, whiff-punishes your recovery, jumps in, and zones with fireballs — and gets tougher each round you take. Win the match to grow your saved win streak. Controls: A/D walk, W jump, S crouch, hold back to block, Z light, X heavy, down then forward + Z for a fireball, Z to rematch.",
  "todo": [
    "Needs two rounds, like real Street Fighter; check it with a gamepad."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// STREET FIGHTER — a 1-on-1 versus fighter (you vs. a reactive AI).
//
// Two stick-fighters, one screen. Health bars up top, a round clock between
// them, best-of-3. Each fighter is a little STATE MACHINE: idle / walk / jump /
// crouch / block / attack / hitstun. Every attack runs startup -> active ->
// recovery, so timing and spacing decide everything.
//
// Three attacks: a LIGHT (fast/short), a HEAVY (slow/long/knockback), and a
// SPECIAL fireball thrown with a quarter-circle-forward motion (down ->
// down-forward -> forward, then light). An input buffer recognises the motion.
//
//   A/D walk    W jump    S crouch    hold back = BLOCK
//   Z light     X heavy   down,forward+Z = FIREBALL
//
//   Best of 3. After the match, Z for a rematch.

#define GY        168          // ground (feet) line
#define STAGE_L   18
#define STAGE_R   302
#define SPD       62.0f        // px/sec (framerate-independent via dt())
#define JUMP      210.0f
#define GRAV      560.0f
#define KNOCK     150.0f
#define HITSTUN   0.26f        // seconds
#define BLOCKSTUN 0.14f
#define MINSEP    20
#define ROUND_TIME 60          // seconds
#define WIN_ROUNDS 2

enum { ATK_NONE, ATK_LIGHT, ATK_HEAVY, ATK_FIRE };
enum { G_INTRO, G_FIGHT, G_ROUNDEND, G_MATCHEND };

typedef struct {
    bool  ai;                   // true => computer-controlled
    float x, y, vx, vy;
    int   facing;               // +1 right, -1 left
    int   hp, wins;
    int   atk; float af;        // attack id + frame timer (seconds)
    bool  hit_done, blocking, crouch, onground;
    float stun, flash;
    bool  fb_on; float fb_x, fb_y; int fb_dir;
    float t_down, t_fwd;        // last time (now()) these inputs were held
    // AI scratch
    float think;                // countdown to next decision
    int   intent;               // current AI plan
} Fighter;

enum { AI_IDLE, AI_APPROACH, AI_RETREAT, AI_BLOCK, AI_POKE, AI_FIRE, AI_JUMP };

static Fighter p1, p2;
static int   gphase, round_n, ai_level;
static float gtimer, round_clock, hitstop, sky_t;
static const char *result = "";
static const char *winner = "";
static int   best_streak, cur_streak;

typedef struct { float x, y, t; } Spark;
static Spark sparks[12];

static void spark(float x, float y) {
    for (int i = 0; i < 12; i++) if (sparks[i].t <= 0) { sparks[i] = (Spark){ x, y, 0.18f }; return; }
}

static void reset_fighter(Fighter *f, bool ai, float x, int facing) {
    f->ai = ai; f->x = x; f->y = GY; f->vx = f->vy = 0; f->facing = facing;
    f->hp = 100; f->atk = ATK_NONE; f->af = 0;
    f->hit_done = f->blocking = f->crouch = false; f->onground = true;
    f->stun = f->flash = 0; f->fb_on = false; f->t_down = f->t_fwd = -999;
    f->think = 0.3f; f->intent = AI_IDLE;
}

static void start_round(void) {
    reset_fighter(&p1, false, 96,  +1);
    reset_fighter(&p2, true,  224, -1);
    round_clock = ROUND_TIME;
    gphase = G_INTRO; gtimer = 1.4f;
}

static void new_match(void) {
    p1.wins = p2.wins = 0; round_n = 1; ai_level = 0;
    start_round();
}

void init(void) {
    best_streak = load_int("best_streak", 0);
    cur_streak = 0;
    instrument(5, INSTR_SAW, 4, 60, 4, 120);
    instrument_filter(5, FILTER_LOW, 1400, 8);
    new_match();
}

// attack timing tables (seconds): startup, active-end, total
static void atk_window(int atk, float *a0, float *a1, float *total, int *reach, int *yoff, int *dmg) {
    if (atk == ATK_LIGHT) { *a0 = 0.06f; *a1 = 0.11f; *total = 0.22f; *reach = 18; *yoff = 32; *dmg = 6; }
    else if (atk == ATK_HEAVY) { *a0 = 0.13f; *a1 = 0.20f; *total = 0.40f; *reach = 28; *yoff = 20; *dmg = 13; }
    else { *a0 = 0.18f; *a1 = 0.20f; *total = 0.44f; *reach = 0; *yoff = 0; *dmg = 0; } // fire (spawns)
}

// ── P1 input → intent ──
static void human_act(Fighter *f, Fighter *o) {
    bool fwd = f->facing > 0 ? btn(0, BTN_RIGHT) : btn(0, BTN_LEFT);
    bool bak = f->facing > 0 ? btn(0, BTN_LEFT)  : btn(0, BTN_RIGHT);
    bool dn  = btn(0, BTN_DOWN);
    if (dn)  f->t_down = now();
    if (fwd) f->t_fwd  = now();

    if (f->onground) {
        if (btnp(0, BTN_UP)) { f->vy = -JUMP; f->onground = false; f->crouch = false; return; }
        f->crouch   = dn;
        f->blocking = bak && !dn;
        if (!dn) f->vx = fwd && !bak ? SPD * f->facing : bak && !fwd ? -SPD * f->facing : 0;
        else     f->vx = 0;

        if (btnp(0, BTN_A)) {
            bool qcf = !f->fb_on && (now() - f->t_down <= 0.30f)
                       && (f->t_fwd >= f->t_down) && (now() - f->t_fwd <= 0.22f);
            f->atk = qcf ? ATK_FIRE : ATK_LIGHT; f->af = 0; f->vx = 0;
        } else if (btnp(0, BTN_B)) { f->atk = ATK_HEAVY; f->af = 0; f->vx = 0; }
    } else {
        f->vx = fwd ? SPD * 0.6f * f->facing : bak ? -SPD * 0.6f * f->facing : f->vx;
        f->crouch = f->blocking = false;
    }
}

// ── P2 AI → intent ──
static void ai_act(Fighter *f, Fighter *o) {
    float gap = fabsf(f->x - o->x);
    bool o_attacking = o->atk != ATK_NONE && o->af < 0.13f; // they just committed
    bool o_recovering = o->atk != ATK_NONE && o->af > 0.13f;

    f->think -= dt();
    if (f->think <= 0) {
        // think delay shrinks a little as ai_level rises (harder)
        f->think = rnd_float_between(0.10f, 0.34f) - ai_level * 0.02f;
        if (f->think < 0.05f) f->think = 0.05f;

        if (o_attacking && gap < 46) f->intent = AI_BLOCK;          // defend a swing
        else if (o_recovering && gap < 40) f->intent = AI_POKE;     // whiff-punish
        else if (gap > 120 && chance(35 + ai_level * 5)) f->intent = AI_FIRE;
        else if (gap > 60) f->intent = AI_APPROACH;
        else if (gap < 26) f->intent = chance(50) ? AI_RETREAT : AI_POKE;
        else f->intent = chance(40 + ai_level * 6) ? AI_POKE : AI_APPROACH;
        if (f->onground && chance(8 + ai_level * 2) && gap > 50 && gap < 110) f->intent = AI_JUMP;
    }

    f->blocking = false; f->crouch = false; f->vx = 0;
    if (!f->onground) {
        // drift toward opponent while airborne
        f->vx = (o->x > f->x ? 1 : -1) * SPD * 0.5f;
        return;
    }
    switch (f->intent) {
        case AI_APPROACH: f->vx = f->facing * SPD * 0.9f; break;
        case AI_RETREAT:  f->vx = -f->facing * SPD * 0.8f; break;
        case AI_BLOCK:    f->blocking = true; break;
        case AI_POKE:
            if (gap < 36) { f->atk = chance(70) ? ATK_LIGHT : ATK_HEAVY; f->af = 0; f->intent = AI_IDLE; }
            else f->vx = f->facing * SPD * 0.9f;
            break;
        case AI_FIRE:
            if (!f->fb_on) { f->atk = ATK_FIRE; f->af = 0; f->intent = AI_IDLE; }
            break;
        case AI_JUMP:
            f->vy = -JUMP; f->onground = false; f->intent = AI_IDLE; break;
        default: break;
    }
}

static void fighter_act(Fighter *f, Fighter *o) {
    if (f->stun > 0) { f->crouch = f->blocking = false; return; }
    if (f->atk != ATK_NONE) { f->crouch = f->blocking = false; return; } // locked in a move
    if (f->onground && o->x != f->x) f->facing = (o->x > f->x) ? 1 : -1;
    if (f->ai) ai_act(f, o); else human_act(f, o);
}

static void apply_hit(Fighter *f, Fighter *o, int dmg) {
    if (o->blocking && o->onground) {
        o->stun = BLOCKSTUN; o->hp -= (dmg > 6 ? 1 : 0); o->vx = f->facing * 70.0f;
        hit(72, INSTR_NOISE, 2, 35);
    } else {
        o->hp -= dmg; o->stun = HITSTUN; o->vx = f->facing * KNOCK;
        o->atk = ATK_NONE; o->crouch = false; o->flash = 0.10f;
        spark(o->x, o->y - 28);
        shake(dmg > 8 ? 4 : 2);
        hitstop = dmg > 8 ? 0.07f : 0.04f;
        hit(dmg > 8 ? 44 : 52, INSTR_NOISE, 4, dmg > 8 ? 90 : 55);
    }
    if (o->hp < 0) o->hp = 0;
}

static void fighter_resolve(Fighter *f, Fighter *o) {
    if (f->atk == ATK_NONE) return;
    float a0, a1, total; int reach, yoff, dmg;
    atk_window(f->atk, &a0, &a1, &total, &reach, &yoff, &dmg);

    if (f->atk == ATK_FIRE) {
        if (f->af >= a0 && !f->fb_on && !f->hit_done) {
            f->hit_done = true;
            f->fb_on = true; f->fb_x = f->x + f->facing * 12; f->fb_y = f->y - 30; f->fb_dir = f->facing;
            note(57, 5, 5);
        }
        return;
    }
    bool active = f->af >= a0 && f->af <= a1;
    if (active && !f->hit_done) {
        int ax = f->facing > 0 ? (int)f->x + 6 : (int)f->x - 6 - reach;
        int oh = o->crouch ? 24 : 42, ob = (int)o->y - oh;
        if (boxes_touch(ax, (int)f->y - yoff, reach, 12, (int)o->x - 7, ob, 14, oh)) {
            f->hit_done = true; apply_hit(f, o, dmg);
        }
    }
}

static void fighter_phys(Fighter *f, Fighter *o) {
    float d = dt();
    if (f->stun > 0)  f->stun  -= d;
    if (f->flash > 0) f->flash -= d;
    if (f->atk != ATK_NONE) {
        f->af += d;
        float a0, a1, total; int reach, yoff, dmg;
        atk_window(f->atk, &a0, &a1, &total, &reach, &yoff, &dmg);
        if (f->af > total) { f->atk = ATK_NONE; f->hit_done = false; }
    }
    if (!f->onground) {
        f->vy += GRAV * d; f->y += f->vy * d;
        if (f->y >= GY) { f->y = GY; f->onground = true; f->vy = 0; }
    }
    f->x += f->vx * d;
    if (f->stun > 0 || !f->onground) f->vx *= (1.0f - 4.0f * d);
    f->x = clamp(f->x, STAGE_L, STAGE_R);

    float dd = f->x - o->x;
    if (fabsf(dd) < MINSEP) {
        float push = (MINSEP - fabsf(dd)) / 2; int s = dd < 0 ? -1 : 1;
        f->x = clamp(f->x + s * push, STAGE_L, STAGE_R);
        o->x = clamp(o->x - s * push, STAGE_L, STAGE_R);
    }
}

static void fireball_update(Fighter *f, Fighter *o) {
    if (!f->fb_on) return;
    f->fb_x += f->fb_dir * 200.0f * dt();
    if (f->fb_x < -12 || f->fb_x > SCREEN_W + 12) { f->fb_on = false; return; }
    int oh = o->crouch ? 24 : 42, ob = (int)o->y - oh;
    if (boxes_touch((int)f->fb_x - 5, (int)f->fb_y - 4, 10, 8, (int)o->x - 7, ob, 14, oh)) {
        if (o->blocking && o->onground) { o->stun = BLOCKSTUN; o->hp -= 2; o->vx = f->fb_dir * 80.0f; }
        else {
            o->hp -= 10; o->stun = HITSTUN + 0.08f; o->vx = f->fb_dir * KNOCK;
            o->atk = ATK_NONE; o->flash = 0.10f; spark(o->x, o->y - 28); shake(5); hitstop = 0.06f;
        }
        if (o->hp < 0) o->hp = 0;
        f->fb_on = false; hit(40, INSTR_NOISE, 5, 140);
    }
}

static void end_round(void) {
    if (p1.hp > p2.hp)      { p1.wins++; winner = "PLAYER 1"; }
    else if (p2.hp > p1.hp) { p2.wins++; winner = "CPU"; }
    else                      winner = "DRAW";
    result = (p1.hp <= 0 || p2.hp <= 0) ? "K.O.!" : "TIME UP";
    gphase = G_ROUNDEND; gtimer = 2.4f;
    hit(33, INSTR_TRI, 6, 260);
}

void update(void) {
    sky_t += dt();
    if (hitstop > 0) { hitstop -= dt(); return; }   // freeze frames for impact

    if (gphase == G_INTRO)    { if ((gtimer -= dt()) <= 0) { gphase = G_FIGHT; round_clock = ROUND_TIME; } return; }
    if (gphase == G_ROUNDEND) {
        if ((gtimer -= dt()) <= 0) {
            if (p1.wins >= WIN_ROUNDS || p2.wins >= WIN_ROUNDS) {
                if (p1.wins > p2.wins) {
                    winner = "PLAYER 1"; cur_streak++;
                    if (cur_streak > best_streak) { best_streak = cur_streak; save_int("best_streak", best_streak); }
                } else { winner = "CPU"; cur_streak = 0; }
                gphase = G_MATCHEND;
            } else {
                if (p1.wins > p2.wins && ai_level < 4) ai_level++;  // CPU gets tougher as you win
                round_n++; start_round();
            }
        }
        return;
    }
    if (gphase == G_MATCHEND) { if (btnp(0, BTN_A)) new_match(); return; }

    fighter_act(&p1, &p2);     fighter_act(&p2, &p1);
    fighter_resolve(&p1, &p2); fighter_resolve(&p2, &p1);
    fighter_phys(&p1, &p2);    fighter_phys(&p2, &p1);
    fireball_update(&p1, &p2); fireball_update(&p2, &p1);
    for (int i = 0; i < 12; i++) if (sparks[i].t > 0) sparks[i].t -= dt();

    if (round_clock > 0) round_clock -= dt();
    if (round_clock < 0) round_clock = 0;
    if (p1.hp <= 0 || p2.hp <= 0 || round_clock <= 0) end_round();
}

// ── drawing ──
static void draw_fighter(Fighter *f, int col) {
    if (f->flash > 0) { pal(col, CLR_WHITE); col = CLR_WHITE; }
    int fx = (int)f->x, footY = (int)f->y, face = f->facing;
    int hipY = footY - (f->crouch ? 9 : 18);
    int shY  = hipY - (f->crouch ? 12 : 16);
    int headY = shY - 5 + (f->stun > 0 ? 1 : 0);
    bool light_active = f->atk == ATK_LIGHT && f->af >= 0.06f && f->af <= 0.13f;
    bool heavy_active = f->atk == ATK_HEAVY && f->af >= 0.13f && f->af <= 0.21f;

    // shadow
    ovalfill(fx, GY + 2, 9, 2, CLR_DARKER_GREY);

    // legs
    if (heavy_active) {
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
    if (light_active) {
        line(fx, shY + 2, fx + face * 18, shY + 2, col); circfill(fx + face * 18, shY + 2, 2, col);
        line(fx, shY + 2, fx - face * 5, shY + 9, col);
    } else if (f->atk == ATK_FIRE && f->af >= 0.12f) {
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
    pal_reset();
}

static void health_bar(int hp, int x, int wins, bool left, const char *name) {
    int BW = 132, BH = 9;
    int col = hp > 50 ? CLR_GREEN : hp > 25 ? CLR_YELLOW : CLR_RED;
    if (hp <= 25 && hp > 0 && blink(8)) col = CLR_WHITE;     // critical-HP pulse
    int w = hp * (BW - 2) / 100;
    rectfill(x - 1, 11, BW + 2, BH + 2, CLR_DARKER_GREY);
    rect(x, 12, BW, BH, CLR_WHITE);
    if (left) rectfill(x + 1, 13, w, BH - 2, col);
    else      rectfill(x + 1 + (BW - 2 - w), 13, w, BH - 2, col);
    print(name, x, 2, CLR_WHITE);
    for (int i = 0; i < wins; i++)
        circfill(left ? x + 50 + i * 8 : x + BW - 54 - i * 8, 6, 2, CLR_YELLOW);
}

void draw(void) {
    // dusk sky that slowly shifts hue over the match
    cls(CLR_DARKER_BLUE);
    rectfill(0, 0, SCREEN_W, 92, CLR_DARK_BLUE);
    int sun = 40 + (int)(8 * sin_deg(sky_t * 12));
    circfill(160, sun, 30, CLR_DARK_RED);
    circfill(160, sun, 26, CLR_DARK_ORANGE);
    // distant skyline
    for (int i = 0; i < 9; i++) {
        int bx = i * 38 - 6, bw = 26, bh = 30 + (i * 37 % 34);
        rectfill(bx, GY - bh, bw, bh, CLR_DARKER_PURPLE);
        if ((i + frame() / 60) % 3 == 0) rectfill(bx + 6, GY - bh + 8, 4, 4, CLR_LIGHT_YELLOW);
    }
    // floor
    rectfill(0, GY, SCREEN_W, SCREEN_H - GY, CLR_BROWN);
    line(0, GY, SCREEN_W, GY, CLR_DARK_BROWN);
    rectfill(STAGE_L - 6, GY, STAGE_R - STAGE_L + 12, 3, CLR_DARK_BROWN);

    draw_fighter(&p1, CLR_BLUE);
    draw_fighter(&p2, CLR_RED);
    for (int i = 0; i < 12; i++)
        if (sparks[i].t > 0) {
            int r = (int)(sparks[i].t * 20) + 1;
            circfill((int)sparks[i].x, (int)sparks[i].y, r, CLR_WHITE);
            circ((int)sparks[i].x, (int)sparks[i].y, r + 2, CLR_YELLOW);
        }

    health_bar(p1.hp, 8, p1.wins, true, "YOU");
    health_bar(p2.hp, SCREEN_W - 8 - 132, p2.wins, false, "CPU");
    int clk = (int)round_clock + (round_clock > 0 ? 1 : 0);
    if (clk > ROUND_TIME) clk = ROUND_TIME;
    print_scaled(str("%d", clk), 152, 1, clk <= 10 ? CLR_RED : CLR_WHITE, 1);
    print_centered(str("%d", clk), SCREEN_W/2, 22, clk <= 10 ? CLR_RED : CLR_WHITE);

    if (gphase == G_INTRO) {
        if (gtimer > 0.5f) print_centered(str("ROUND %d", round_n), SCREEN_W/2, 84, CLR_YELLOW);
        else if (blink(4)) print_centered("FIGHT!", SCREEN_W/2, 84, CLR_RED);
        print_centered("down,forward + Z = FIREBALL", SCREEN_W/2, SCREEN_H - 10, CLR_LIGHT_GREY);
    } else if (gphase == G_ROUNDEND) {
        fade(0.25f);
        print_scaled(result, 132, 78, CLR_WHITE, 2);
        if (winner[0] != 'D') print_centered(str("%s wins the round", winner), SCREEN_W/2, 104, CLR_YELLOW);
        else                  print_centered("DRAW", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    } else if (gphase == G_MATCHEND) {
        fade(0.4f);
        bool win = (winner[0] == 'P');
        print_scaled(win ? "YOU WIN!" : "YOU LOSE", 110, 70, win ? CLR_YELLOW : CLR_RED, 2);
        print_centered(str("win streak %d   best %d", cur_streak, best_streak), SCREEN_W/2, 96, CLR_WHITE);
        print_centered("press Z for a rematch", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }
}
