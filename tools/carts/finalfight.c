/* de:meta
{
  "title": "final fight",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "screen-shake-juice",
    "finite-state-ai",
    "title-play-gameover-loop",
    "particle-system"
  ],
  "lineage": "Final Fight (Capcom 1989) — belt-scroll brawler chassis with y-sorted depth, wave-locked camera, and a full juice stack (hit-stop, flash, slow-mo, particles).",
  "genre": "fighting",
  "homage": "Final Fight (1989)",
  "description": "A belt-scroll beat-'em-up — and a reusable brawler engine. Walk a chunky 16×32 hero right through a city; the camera LOCKS into scroll arenas of thugs and a GO arrow releases you on clear, street giving way to a warehouse and a boss. Fighters have x, depth (y) and jump height (z) with y-sort overlap. Tap Z for a 3-hit combo (the third launches a knockdown), X to jump, Z in the air to jump-kick; walk into a thug to grab him then push a direction to THROW him into the others, Z+X for a spin special. Break barrels for food, grab dropped pipes & knives. Enemy variety is all pal() shirt/skin recolors of one sprite set (grunt, knife, bruiser, boss). Hit-stop, shake, white flash, POW stars, slow-mo boss kill, combo counter, and a reactive synth groove. WASD move, Z attack, X jump, Z+X special."
}
de:meta */
#include "studio.h"

// FINAL FIGHT — a linear arcade BELT-SCROLL beat-'em-up, and a reusable brawler
// engine. Walk right through a city, fists up, into scroll-locked arenas of
// thugs; clear a wave and the camera unlocks with a "GO →". Two stage looks
// (street → warehouse) and a boss at the end.
//
// THE BELT: every fighter has x (along the street), y (DEPTH — up/down the belt)
// and z (jump height). Drawing y-sorts so nearer fighters overlap farther ones.
// The camera only scrolls x; it LOCKS the instant a wave spawns.
//
// COMBAT: tap Z for a 3-hit punch combo (the third launches a knockdown). X
// jumps; Z in the air is a jump-kick. Walk INTO a thug to grab him, then Z knees,
// or push a direction to THROW him into the others. Z+X is a spin special that
// hits all around and costs a sliver of health. Break barrels for food (heal),
// pick up pipes & knives that thugs drop.
//
// JUICE: hit-stop, screen shake, white hit-flash, "POW" stars, dust, knockdown
// bounce, slow-mo on the boss kill, a combo counter, and a synth groove that
// reacts to the action.
//
//   Move: WASD / d-pad   Attack: Z (tap = combo)   Jump: X   Special: Z+X
//   Grab: walk into a thug   Throw: push a direction while grabbing

// ───────────────────────── world / belt geometry ─────────────────────────
#define WORLD_W   1280
#define BELT_TOP  138        // nearest a fighter's FEET can be to the top of the belt
#define BELT_BOT  196        // ...and the bottom (closest to camera)
#define DEPTH      11        // how close in y two fighters must be to interact

// ───────────────────────── tuning ─────────────────────────
#define GRAVZ     0.55f
#define HERO_SPD  1.55f
#define MAXF      14
#define MAXP      48         // particles
#define MAXI       8         // ground items (weapons, food)
#define MAXB       6         // barrels

// teams
#define T_HERO 0
#define T_ENE  1

// enemy types
#define E_GRUNT   0
#define E_KNIFE   1
#define E_BRUISER 2
#define E_BOSS    3

// states
enum { ST_IDLE, ST_WALK, ST_PUNCH, ST_KICK, ST_HURT, ST_DOWN, ST_GETUP,
       ST_GRAB, ST_GRABBED, ST_THROWN, ST_SPECIAL };

// weapons
#define W_NONE 0
#define W_PIPE 1
#define W_KNIFE 2

// game phases
enum { G_INTRO, G_PLAY, G_OVER, G_WIN };

typedef struct {
    bool  on;
    int   team, type;
    float x, y, z, vx, vy, vz;
    int   facing;             // +1 right / -1 left
    int   hp, maxhp;
    int   state;
    float stt;                // state timer, in frames
    int   combo;              // hero combo step 0..2
    float anim;               // walk-cycle phase
    bool  hitdone;            // this swing already connected
    int   weapon;
    int   flash;              // white hit-flash frames
    int   iframe;             // invulnerability frames
    float scale;              // draw scale (boss is big)
    int   aicd;               // enemy attack pacing
    float aoff;               // preferred depth offset, so thugs surround
    int   grab;               // index this fighter is grabbing, or -1
} Fighter;

typedef struct { bool on; float x, y, z, vx, vy, vz, life; int col, kind; } Part;
typedef struct { bool on; float x, y, z, vx, vy, vz; int kind; } Item;   // kind = W_PIPE / W_KNIFE / 3 food
typedef struct { bool on; float x, y; int hp, hasFood; } Barrel;

#define K_FOOD 3
#define P_DUST 0
#define P_STAR 1
#define P_SPARK 2

static Fighter f[MAXF];
static Part    parts[MAXP];
static Item    items[MAXI];
static Barrel  barrels[MAXB];

static int   cam;                 // camera x
static int   gphase;
static int   waveIdx;             // next wave to trigger
static bool  locked;              // camera/arena locked for a wave
static int   lockCam;             // frozen camera x while locked
static int   hitstop;
static float slowmo;              // >0 = slow time (boss kill)
static int   score, lives;
static int   combo;               // on-screen hit combo
static float comboT;              // combo decay timer
static int   order[MAXF];         // y-sorted draw order
static int   goArrowX;            // x past which the "GO" prompt hides
static float bannerT;             // stage banner timer
static const char *bannerS = "";

// hero is always f[0]
#define HERO (&f[0])

// ───────────────────────── palettes per fighter type ─────────────────────────
// {skin, shirt, pants, hair}
static const int PAL_HERO[4]    = { 15, 12, 1,  4 };   // blue jeans hero
static const int PAL_GRUNT[4]   = { 15, 11, 5,  4 };   // green punk
static const int PAL_KNIFE[4]   = { 4,  10, 1,  16 };  // yellow knife thug
static const int PAL_BRUISER[4] = { 31, 8,  5,  16 };  // red fat bruiser
static const int PAL_BOSS[4]    = { 4,  2,  16, 7 };   // purple-suit white-hair boss

static const int *type_pal(int team, int type) {
    if (team == T_HERO) return PAL_HERO;
    switch (type) {
        case E_KNIFE:   return PAL_KNIFE;
        case E_BRUISER: return PAL_BRUISER;
        case E_BOSS:    return PAL_BOSS;
        default:        return PAL_GRUNT;
    }
}

// ───────────────────────── spawning ─────────────────────────
static int spawn_enemy(int type, float x, float y) {
    for (int i = 1; i < MAXF; i++) {
        if (f[i].on) continue;
        Fighter *e = &f[i];
        *e = (Fighter){0};
        e->on = true; e->team = T_ENE; e->type = type;
        e->x = x; e->y = y; e->facing = -1; e->state = ST_WALK;
        e->scale = (type == E_BRUISER) ? 1.25f : (type == E_BOSS) ? 1.9f : 1.0f;
        e->maxhp = e->hp = (type == E_GRUNT) ? 14 : (type == E_KNIFE) ? 11
                          : (type == E_BRUISER) ? 34 : 90;
        e->weapon = (type == E_KNIFE) ? W_KNIFE : W_NONE;
        e->aicd = 30 + rnd(50);
        e->aoff = rnd_float_between(-9.0f, 9.0f);
        e->grab = -1;
        return i;
    }
    return -1;
}

static void spawn_part(int kind, float x, float y, float z, int col, int n) {
    for (int c = 0; c < n; c++)
        for (int i = 0; i < MAXP; i++)
            if (!parts[i].on) {
                Part *p = &parts[i];
                p->on = true; p->kind = kind; p->x = x; p->y = y; p->z = z;
                p->vx = rnd_float_between(-1.8f, 1.8f);
                p->vy = rnd_float_between(-0.8f, 0.8f);
                p->vz = (kind == P_STAR) ? rnd_float_between(1.0f, 2.6f)
                                         : rnd_float_between(0.4f, 2.2f);
                p->life = (kind == P_STAR) ? 16 : rnd_between(12, 22);
                p->col = col;
                break;
            }
}

static void spawn_item(int kind, float x, float y) {
    for (int i = 0; i < MAXI; i++)
        if (!items[i].on) {
            items[i] = (Item){ true, x, y, 18.0f, 0, 0, 3.0f, kind };
            return;
        }
}

// ───────────────────────── waves ─────────────────────────
// each wave: a trigger x; when the hero crosses it the camera locks and the
// listed enemies stream in from the right (and left for later waves).
typedef struct { int trig; int types[6]; int n; const char *banner; } Wave;
static const Wave WAVES[] = {
    { 210, { E_GRUNT, E_GRUNT, E_GRUNT },                  3, 0 },
    { 470, { E_GRUNT, E_KNIFE, E_GRUNT, E_KNIFE },         4, 0 },
    { 700, { E_BRUISER, E_GRUNT, E_GRUNT },                3, "STAGE 2:  WAREHOUSE" },
    { 940, { E_KNIFE, E_KNIFE, E_GRUNT, E_BRUISER },       4, 0 },
    { 1180,{ E_BOSS, E_GRUNT, E_GRUNT },                   3, "WARNING:  BOSS" },
};
#define NWAVES ((int)(sizeof WAVES / sizeof *WAVES))

static int count_enemies(void) {
    int n = 0;
    for (int i = 1; i < MAXF; i++) if (f[i].on) n++;
    return n;
}

static void start_wave(int w) {
    locked = true;
    lockCam = (int)clamp(HERO->x - 70, 0, WORLD_W - SCREEN_W);
    if (WAVES[w].banner) { bannerS = WAVES[w].banner; bannerT = 110; sfx(-1); note(40, INSTR_SAW, 6); }
    int right = 0, left = 0;
    for (int i = 0; i < WAVES[w].n; i++) {
        int t = WAVES[w].types[i];
        float y = clamp(BELT_TOP + 8 + rnd(BELT_BOT - BELT_TOP - 16), BELT_TOP, BELT_BOT);
        float x;
        if (t == E_BOSS || (i & 1) == 0) x = lockCam + SCREEN_W + 16 + right++ * 26;  // from the right
        else                              x = lockCam - 16 - left++ * 26;             // from the left
        spawn_enemy(t, x, y);
    }
    // barrels with food in the warehouse arenas
    if (w >= 2) {
        for (int b = 0; b < 2; b++)
            for (int i = 0; i < MAXB; i++)
                if (!barrels[i].on) {
                    barrels[i] = (Barrel){ true, lockCam + 60 + b * 200,
                                           BELT_TOP + 6 + rnd(14), 3, b == 0 };
                    break;
                }
    }
}

// ───────────────────────── reset / lifecycle ─────────────────────────
static void reset_hero(void) {
    Fighter *h = HERO;
    int keepw = h->weapon;
    *h = (Fighter){0};
    h->on = true; h->team = T_HERO; h->facing = 1;
    h->x = (cam > 0) ? cam + 60 : 60; h->y = 176; h->scale = 1.0f;
    h->maxhp = 100; h->hp = 100; h->state = ST_IDLE; h->iframe = 110;
    h->weapon = keepw; h->grab = -1;
}

static void new_game(void) {
    for (int i = 0; i < MAXF; i++) f[i].on = false;
    for (int i = 0; i < MAXP; i++) parts[i].on = false;
    for (int i = 0; i < MAXI; i++) items[i].on = false;
    for (int i = 0; i < MAXB; i++) barrels[i].on = false;
    cam = 0; waveIdx = 0; locked = false; hitstop = 0; slowmo = 0;
    score = 0; lives = 3; combo = 0; comboT = 0; goArrowX = -1; bannerT = 0;
    HERO->weapon = W_NONE;
    reset_hero();
    gphase = G_INTRO; bannerT = 90; bannerS = "STAGE 1:  THE STREET";
    bpm(112);
    // punch / impact instruments
    instrument(5, INSTR_NOISE, 1, 60, 0, 30);   instrument_filter(5, FILTER_LOW, 1400, 6);
    instrument(6, INSTR_TRI,   1, 90, 0, 60);                       // body thud
    instrument(7, INSTR_SQUARE, 4, 220, 3, 120);                    // groove bass
    instrument_duty(7, 0.30f);  instrument_filter(7, FILTER_LOW, 900, 3);
    instrument(8, INSTR_SAW, 6, 180, 2, 150);   instrument_filter(8, FILTER_LOW, 1200, 7);
}

void init(void) { colorkey(0); new_game(); }

// ───────────────────────── combat ─────────────────────────
static void pow_sound(int dmg) {
    hit(50 - dmg, INSTR_NOISE, 6, 30 + dmg * 3);   // crack
    hit(34, 6, 6, 50);                             // body thud (instr slot 6)
}

// returns true on a clean (non-whiffed) connect
static bool apply_hit(Fighter *a, Fighter *t, int dmg, float knock, bool launch) {
    if (!t->on || t->iframe > 0) return false;
    if (t->state == ST_DOWN || t->state == ST_GETUP || t->state == ST_THROWN) return false;

    t->hp -= dmg;
    t->flash = 4;
    int dir = (t->x >= a->x) ? 1 : -1;
    t->facing = -dir;                       // turn to face the blow
    t->vx = dir * knock;
    t->grab = -1;
    if (t->state == ST_GRABBED) t->state = ST_HURT;

    float cx = (a->x + t->x) / 2, cy = t->y - 16 - t->z;
    spawn_part(P_STAR,  cx, cy, 0, CLR_WHITE, 1);
    spawn_part(P_SPARK, cx, cy, 0, CLR_YELLOW, 4);
    spawn_part(P_DUST,  t->x, t->y, 0, CLR_LIGHT_GREY, 3);
    pow_sound(dmg);
    shake(launch ? 4.0f : 2.2f);
    hitstop = launch ? 7 : 4;

    if (t->team == T_ENE) {
        score += dmg + combo;
        combo++; comboT = 80;
    }

    if (launch || t->hp <= 0) {
        t->state = ST_DOWN; t->stt = 0; t->vz = 4.6f; t->vx = dir * (knock + 1.6f);
    } else {
        t->state = ST_HURT; t->stt = 0;
    }
    return true;
}

// the live hitbox for the attacker's current move → world rect + dmg + knock
static void resolve_attack(Fighter *a, int idx) {
    if (a->state != ST_PUNCH && a->state != ST_KICK && a->state != ST_SPECIAL) return;

    // SPECIAL: a ring that hits everything close, once
    if (a->state == ST_SPECIAL) {
        if (a->stt >= 4 && a->stt <= 16 && !a->hitdone) {
            for (int j = 0; j < MAXF; j++) {
                if (j == idx || !f[j].on || f[j].team == a->team) continue;
                if (near((int)a->x, (int)a->y, (int)f[j].x, (int)f[j].y, 30))
                    apply_hit(a, &f[j], 7, 4.0f, true);
            }
            if (a->stt >= 14) a->hitdone = true;
        }
        return;
    }

    int wbonus_r = a->weapon == W_PIPE ? 9 : a->weapon == W_KNIFE ? 6 : 0;
    int wbonus_d = a->weapon == W_PIPE ? 3 : a->weapon == W_KNIFE ? 5 : 0;

    bool active; int reach, dmg; float knock; bool launch = false;
    if (a->state == ST_KICK) {                       // jump-kick or enemy kick
        active = a->stt >= 4 && a->stt <= 11;
        reach = 18 + wbonus_r; dmg = 7 + wbonus_d; knock = 3.0f;
    } else {                                          // punch / combo
        active = a->stt >= 4 && a->stt <= 9;
        reach = 15 + wbonus_r;
        if (a->team == T_HERO && a->combo >= 2) { dmg = 8 + wbonus_d; knock = 3.4f; launch = true; }
        else { dmg = (a->team == T_HERO ? 5 : (a->type == E_BRUISER ? 12 : a->type == E_BOSS ? 13 : 6)) + wbonus_d;
               knock = (a->type == E_BRUISER || a->type == E_BOSS) ? 3.2f : 2.0f; }
    }
    if (!active || a->hitdone) return;

    int near_x = a->facing > 0 ? (int)a->x : (int)a->x - reach;
    bool pipe = a->weapon == W_PIPE;            // pipe sweeps — can tag two
    int hits = 0;
    for (int j = 0; j < MAXF; j++) {
        if (j == idx || !f[j].on || f[j].team == a->team) continue;
        Fighter *t = &f[j];
        if (abs((int)t->y - (int)a->y) > DEPTH) continue;             // not in the same lane
        if ((t->x - a->x) * a->facing < -4) continue;                 // behind us
        if (boxes_touch(near_x, (int)a->y - 30, reach, 30,
                        (int)t->x - 6, (int)t->y - 30 - (int)t->z, 12, 30)) {
            if (apply_hit(a, t, dmg, knock, launch)) { a->hitdone = true; hits++; if (!pipe || hits >= 2) break; }
        }
    }
}

// ───────────────────────── hero control ─────────────────────────
static void try_attack(Fighter *h) {
    if (h->state == ST_GRAB) {                       // knee while grabbing
        h->state = ST_PUNCH; h->combo = 1; h->stt = 0; h->hitdone = false; return;
    }
    if (h->state == ST_PUNCH && h->stt > 8 && h->combo < 2) {   // chain the combo
        h->combo++; h->stt = 0; h->hitdone = false; return;
    }
    if (h->state == ST_IDLE || h->state == ST_WALK) {
        h->state = ST_PUNCH; h->combo = 0; h->stt = 0; h->hitdone = false; h->vx = 0;
    }
}

static void control_hero(float k) {
    Fighter *h = HERO;
    bool busy = h->state == ST_PUNCH || h->state == ST_KICK || h->state == ST_HURT ||
                h->state == ST_DOWN  || h->state == ST_GETUP || h->state == ST_SPECIAL;

    // SPECIAL: Z+X together (and we have health to spend)
    bool sp = (btnp(0, BTN_A) && btn(0, BTN_B)) || (btnp(0, BTN_B) && btn(0, BTN_A));
    if (sp && !busy && h->hp > 12) {
        h->state = ST_SPECIAL; h->stt = 0; h->hitdone = false; h->vx = 0;
        h->hp -= 8; h->iframe = 24; shake(3); note(64, INSTR_SAW, 5); return;
    }

    if (!busy && h->state != ST_GRAB) {
        if (btnp(0, BTN_B) && h->z == 0) { h->vz = 5.6f; note(76, INSTR_SQUARE, 2); }  // jump
        float mx = 0, my = 0;
        if (btn(0, BTN_LEFT))  mx -= 1;
        if (btn(0, BTN_RIGHT)) mx += 1;
        if (btn(0, BTN_UP))    my -= 1;
        if (btn(0, BTN_DOWN))  my += 1;
        if (mx != 0) h->facing = mx > 0 ? 1 : -1;
        float spd = HERO_SPD * (h->z > 0 ? 0.9f : 1.0f);
        h->vx = mx * spd; h->vy = my * spd * 0.8f;
        h->state = (mx || my) ? ST_WALK : ST_IDLE;
        if (h->state == ST_WALK) h->anim += 0.18f * k;

        if (btnp(0, BTN_A)) {
            if (h->z > 0) { h->state = ST_KICK; h->stt = 0; h->hitdone = false; }  // jump-kick
            else try_attack(h);
        }
    } else if (h->state == ST_GRAB) {
        // grabbing: Z knees, a direction throws
        int tj = h->grab;
        if (tj >= 0 && f[tj].on) {
            float tx = h->x + h->facing * 11;
            f[tj].x = tx; f[tj].y = h->y; f[tj].facing = -h->facing; f[tj].state = ST_GRABBED;
            f[tj].vx = f[tj].vy = 0;
            if (btnp(0, BTN_A)) try_attack(h);
            else if (btnp(0, BTN_LEFT) || btnp(0, BTN_RIGHT)) {
                int dir = btn(0, BTN_RIGHT) ? 1 : -1;
                h->facing = dir;
                Fighter *t = &f[tj];
                t->state = ST_THROWN; t->vx = dir * 5.5f; t->vz = 4.2f; t->vy = 0;
                t->iframe = 0; t->facing = -dir;
                h->grab = -1; h->state = ST_IDLE;
                shake(3.5f); note(48, INSTR_NOISE, 5); score += 20;
            }
        } else { h->grab = -1; h->state = ST_IDLE; }
    }
}

// auto-grab: hero standing against a grabbable thug latches on
static void try_grab(Fighter *h, float k) {
    static float contact = 0;
    if (h->state != ST_IDLE && h->state != ST_WALK) { contact = 0; return; }
    if (h->iframe > 80) { contact = 0; return; }
    for (int j = 1; j < MAXF; j++) {
        if (!f[j].on) continue;
        Fighter *t = &f[j];
        if (t->type == E_BOSS) continue;             // can't grab the boss
        if (t->state == ST_DOWN || t->state == ST_THROWN || t->state == ST_GETUP) continue;
        if (abs((int)t->y - (int)h->y) > DEPTH) continue;
        if ((t->x - h->x) * h->facing < 0) continue; // must face him
        if (near((int)h->x + h->facing * 8, (int)h->y, (int)t->x, (int)t->y, 11)) {
            contact += k;
            if (contact > 12) { h->state = ST_GRAB; h->grab = j; t->state = ST_GRABBED; contact = 0; }
            return;
        }
    }
    contact = 0;
}

// ───────────────────────── enemy AI ─────────────────────────
static int attacking_count(void) {
    int n = 0;
    for (int i = 1; i < MAXF; i++)
        if (f[i].on && (f[i].state == ST_PUNCH || f[i].state == ST_KICK)) n++;
    return n;
}

static void enemy_ai(Fighter *e, float k) {
    int s = e->state;
    if (s == ST_PUNCH || s == ST_KICK || s == ST_HURT || s == ST_DOWN ||
        s == ST_GETUP || s == ST_THROWN || s == ST_GRABBED) return;

    Fighter *h = HERO;
    float ddx = h->x - e->x, ddy = (h->y + e->aoff) - e->y;
    e->facing = ddx >= 0 ? 1 : -1;

    int reach = (e->type == E_KNIFE) ? 22 : (e->type == E_BOSS) ? 24 : 16;
    bool inrange = (abs((int)ddx) < reach) && (abs((int)(h->y - e->y)) < DEPTH);

    if (e->aicd > 0) e->aicd--;
    if (inrange && e->aicd == 0 && attacking_count() < 2 && h->iframe < 90) {
        e->state = ST_PUNCH; e->combo = 0; e->stt = 0; e->hitdone = false;
        e->vx = e->vy = 0;
        e->aicd = (e->type == E_BOSS) ? 28 : (e->type == E_KNIFE) ? 45 : 70 + rnd(40);
        return;
    }

    float spd = (e->type == E_KNIFE) ? 1.25f : (e->type == E_BRUISER) ? 0.7f
              : (e->type == E_BOSS) ? 0.85f : 0.95f;
    float adx = ddx < 0 ? -1 : 1;
    e->vx = (abs((int)ddx) > reach - 4) ? adx * spd : 0;
    e->vy = (abs((int)ddy) > 2) ? (ddy < 0 ? -1 : 1) * spd * 0.7f : 0;
    e->state = (e->vx || e->vy) ? ST_WALK : ST_IDLE;
    if (e->state == ST_WALK) e->anim += 0.16f * k;
}

// ───────────────────────── physics, per fighter ─────────────────────────
static void phys(Fighter *e, float k) {
    e->stt += k;
    if (e->flash  > 0) e->flash--;
    if (e->iframe > 0) e->iframe--;

    // vertical (jump / launch / bounce)
    if (e->z > 0 || e->vz != 0) {
        e->z += e->vz * k; e->vz -= GRAVZ * k;
        if (e->z <= 0) {
            e->z = 0;
            if (e->state == ST_THROWN) {
                e->state = ST_DOWN; e->stt = 0; e->vz = 0; spawn_part(P_DUST, e->x, e->y, 0, CLR_LIGHT_GREY, 4);
            } else if (e->state == ST_DOWN) {
                if (e->vz < -2.0f) { e->vz = -e->vz * 0.42f; spawn_part(P_DUST, e->x, e->y, 0, CLR_LIGHT_GREY, 3); }
                else { e->vz = 0; e->stt = 0; e->state = ST_GETUP; }
            } else { e->vz = 0; }
        }
    }

    e->x += e->vx * k; e->y += e->vy * k;
    if (e->state == ST_HURT || e->state == ST_DOWN || e->state == ST_THROWN)
        e->vx *= 0.86f;

    e->y = clamp(e->y, BELT_TOP, BELT_BOT);
    e->x = clamp(e->x, 8, WORLD_W - 8);
    if (locked && e->team == T_HERO)
        e->x = clamp(e->x, lockCam + 10, lockCam + SCREEN_W - 10);

    // thrown bodies bowl over other enemies
    if (e->state == ST_THROWN)
        for (int j = 1; j < MAXF; j++)
            if (f[j].on && &f[j] != e && f[j].state != ST_DOWN && f[j].state != ST_THROWN &&
                abs((int)f[j].y - (int)e->y) < DEPTH && near((int)e->x, (int)e->y, (int)f[j].x, (int)f[j].y, 11))
                apply_hit(e, &f[j], 8, 3.0f, true);

    // state-timer transitions
    switch (e->state) {
        case ST_PUNCH: {
            int dur = (e->team == T_HERO && e->combo == 2) ? 22 : 16;
            if (e->stt > dur) { e->state = ST_IDLE; e->combo = 0; }
            break; }
        case ST_KICK:    if (e->stt > 16) e->state = (e->z > 0) ? ST_KICK : ST_IDLE; if (e->stt > 22) e->state = ST_IDLE; break;
        case ST_SPECIAL: if (e->stt > 26) e->state = ST_IDLE; break;
        case ST_HURT:    if (e->stt > (e->type == E_BRUISER ? 10 : 14)) e->state = ST_IDLE; break;
        case ST_GETUP:   if (e->stt > 22) { e->state = ST_IDLE; e->iframe = e->team == T_HERO ? 40 : 16; } break;
    }
}

// ───────────────────────── death / items ─────────────────────────
static void kill_enemy(Fighter *e) {
    score += (e->type == E_BOSS) ? 1000 : (e->type == E_BRUISER) ? 300 : 100;
    spawn_part(P_DUST, e->x, e->y, 0, CLR_WHITE, 8);
    if (e->type == E_BOSS) { slowmo = 70; shake(6); hit(33, INSTR_TRI, 7, 600); }
    else hit(38, INSTR_NOISE, 5, 200);
    if (e->weapon == W_KNIFE) spawn_item(W_KNIFE, e->x, e->y);
    else if (e->type == E_BRUISER) spawn_item(W_PIPE, e->x, e->y);
    e->on = false;
}

static void items_update(float k) {
    Fighter *h = HERO;
    for (int i = 0; i < MAXI; i++) {
        Item *it = &items[i];
        if (!it->on) continue;
        if (it->z > 0 || it->vz != 0) { it->z += it->vz * k; it->vz -= GRAVZ * k; if (it->z <= 0) { it->z = 0; it->vz = 0; } }
        if (h->state != ST_DOWN && h->state != ST_GETUP &&
            abs((int)it->y - (int)h->y) < DEPTH && near((int)it->x, (int)it->y, (int)h->x, (int)h->y, 12)) {
            if (it->kind == K_FOOD) { h->hp = min(h->maxhp, h->hp + 35); note(72, INSTR_SINE, 5); spawn_part(P_STAR, h->x, h->y - 24, 0, CLR_GREEN, 4); }
            else { h->weapon = it->kind; note(60, INSTR_SQUARE, 3); }
            it->on = false;
        }
    }
    // barrels — break when hero's punch is active nearby
    for (int b = 0; b < MAXB; b++) {
        Barrel *ba = &barrels[b];
        if (!ba->on) continue;
        if ((h->state == ST_PUNCH || h->state == ST_KICK || h->state == ST_SPECIAL) &&
            h->stt >= 4 && h->stt <= 10 &&
            abs((int)ba->y - (int)h->y) < DEPTH &&
            near((int)h->x + h->facing * 10, (int)h->y, (int)ba->x, (int)ba->y, 14)) {
            ba->hp--;
            spawn_part(P_SPARK, ba->x, ba->y - 8, 0, CLR_ORANGE, 3);
            if (ba->hp <= 0) {
                ba->on = false; shake(2); hit(44, INSTR_NOISE, 4, 90);
                spawn_part(P_DUST, ba->x, ba->y, 0, CLR_BROWN, 8);
                if (ba->hasFood) spawn_item(K_FOOD, ba->x, ba->y);
            }
        }
    }
}

static void parts_update(float k) {
    for (int i = 0; i < MAXP; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        p->x += p->vx * k; p->y += p->vy * k; p->z += p->vz * k;
        p->vz -= (p->kind == P_STAR ? 0.10f : 0.22f) * k;
        p->life -= k;
        if (p->life <= 0) p->on = false;
    }
}

// ───────────────────────── update ─────────────────────────
void update(void) {
    if (gphase == G_INTRO) { if (bannerT > 0) bannerT -= 1; if (bannerT <= 0) gphase = G_PLAY; }
    if (gphase == G_OVER || gphase == G_WIN) { if (btnp(0, BTN_A)) new_game(); return; }

    if (hitstop > 0) { hitstop--; return; }

    // slow-mo: run the sim every other frame
    if (slowmo > 0) { slowmo -= 1; if (((int)slowmo) & 1) return; }

    float k = dt() * 60.0f;
    if (k > 2.2f) k = 2.2f;

    if (bannerT > 0) bannerT -= 1;
    if (comboT  > 0) { comboT -= 1; if (comboT <= 0) combo = 0; }

    // groove — a driving bassline + offbeat hat while fighting
    if (locked || count_enemies() > 0) {
        if (every(1))            note(degree(SCALE_PENTA_MIN, 2, beat()), 7, 4);
        if (euclid(3, 8, beat())) hit(84, INSTR_NOISE, 2, 28);
    }

    control_hero(k);
    try_grab(HERO, k);

    // wave trigger
    if (!locked && waveIdx < NWAVES && HERO->x > WAVES[waveIdx].trig) start_wave(waveIdx);

    for (int i = 1; i < MAXF; i++) if (f[i].on) enemy_ai(&f[i], k);

    for (int i = 0; i < MAXF; i++) if (f[i].on) phys(&f[i], k);
    for (int i = 0; i < MAXF; i++) if (f[i].on) resolve_attack(&f[i], i);

    // dead enemies (hp gone) crumple on the ground, then are removed
    for (int i = 1; i < MAXF; i++)
        if (f[i].on && f[i].hp <= 0 && (f[i].state == ST_GETUP)) kill_enemy(&f[i]);

    // hero death
    if (HERO->hp <= 0 && HERO->iframe == 0) {
        lives--;
        shake(6); hit(36, INSTR_NOISE, 7, 400);
        if (lives <= 0) { gphase = G_OVER; }
        else reset_hero();
    }

    items_update(k);
    parts_update(k);

    // wave cleared?
    if (locked && count_enemies() == 0) {
        locked = false; waveIdx++;
        for (int b = 0; b < MAXB; b++) barrels[b].on = false;
        if (waveIdx >= NWAVES) { gphase = G_WIN; note(72, INSTR_SINE, 6); }
        else { goArrowX = (int)HERO->x + 120; note(67, INSTR_SINE, 5); }
    }

    // camera
    if (locked) cam = lockCam;
    else        cam = (int)clamp(HERO->x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W);
    if (goArrowX > 0 && HERO->x > goArrowX) goArrowX = -1;
}

// ───────────────────────── drawing ─────────────────────────
static void set_pal(int team, int type, bool white) {
    if (white) {                              // hit-flash: everything → white
        pal(30, 7); pal(28, 7); pal(29, 7); pal(26, 7); pal(16, 7); pal(5, 7);
        return;
    }
    const int *pp = type_pal(team, type);
    pal(30, pp[0]); pal(28, pp[1]); pal(29, pp[2]); pal(26, pp[3]);
}

// draw one 16×16 slot, optionally scaled / flipped, at screen (sx,sy)
static void draw_slot(int slot, int sx, int sy, float scl, bool flip) {
    if (scl == 1.0f) { sprf(slot, sx, sy, flip, false); return; }
    int ssx = (slot % 8) * 16, ssy = (slot / 8) * 16;
    int dw = (int)(16 * scl + 0.5f), dh = (int)(16 * scl + 0.5f);
    int sw = flip ? -16 : 16;
    sspr_ex(ssx, ssy, sw, 16, sx, sy, dw, dh, 0, dw / 2, dh / 2);
}

static void draw_fighter(Fighter *e) {
    int top, bot;
    bool walking = e->state == ST_WALK;
    int wf = ((int)(e->anim) & 1);
    switch (e->state) {
        case ST_PUNCH:   top = 1; bot = 11; break;
        case ST_GRAB:    top = 3; bot = 8;  break;
        case ST_HURT:    top = 2; bot = 8;  break;
        case ST_GRABBED: top = 2; bot = 8;  break;
        case ST_KICK:    top = 4; bot = 12; break;
        case ST_SPECIAL: top = 1; bot = 11; break;
        default:         top = 0; bot = walking ? (wf ? 9 : 10) : 8; break;
    }

    float scl = e->scale;
    int w = (int)(16 * scl);
    int footY = (int)e->y - (int)e->z;
    int sx = (int)e->x - cam - w / 2;
    int topY = footY - (int)(32 * scl);
    bool flip = e->facing < 0;

    // shadow on the belt (shrinks with jump height)
    float sh = clamp(1.0f - e->z / 60.0f, 0.35f, 1.0f);
    ovalfill((int)e->x - cam, (int)e->y, (int)(7 * scl * sh), (int)(2.5f * scl * sh), CLR_DARK_BLUE);

    set_pal(e->team, e->type, e->flash > 0);

    if (e->state == ST_DOWN || e->state == ST_THROWN) {
        // knocked down: the standing figure rotated flat onto the belt
        int ssx = 0, ssy = 0;             // idle top region (slot 0) + legs below = a 16×32 column
        // draw whole body via two stacked scaled slots, rotated 90°
        sspr_ex(0, 0, flip ? -16 : 16, 32, (int)e->x - cam - 16, footY - 8,
                (int)(32 * scl), (int)(16 * scl), e->facing > 0 ? 90 : -90, (int)(16 * scl), (int)(8 * scl));
    } else {
        draw_slot(top, sx, topY,            scl, flip);
        draw_slot(bot, sx, topY + (int)(16 * scl), scl, flip);
    }
    pal_reset();

    // weapon glint in hand while attacking
    if (e->weapon && (e->state == ST_PUNCH || e->state == ST_IDLE || e->state == ST_WALK)) {
        int hx = (int)e->x - cam + e->facing * 12, hy = topY + (int)(11 * scl);
        if (e->weapon == W_PIPE) line(hx, hy, hx + e->facing * 9, hy - 2, CLR_LIGHT_GREY);
        else { line(hx, hy, hx + e->facing * 6, hy, CLR_WHITE); }
    }
}

static void draw_world(void) {
    // sky gradient
    for (int y = 0; y < 130; y++) {
        int c = y < 40 ? CLR_DARK_BLUE : y < 80 ? CLR_DARK_PURPLE : CLR_DARK_RED;
        line(0, y, SCREEN_W, y, c);
    }
    // far parallax skyline (half-speed)
    int px = -(cam / 2);
    for (int i = 0; i < 40; i++) {
        int bx = px + i * 46 - 40;
        int bw = 30 + (i * 37) % 22, bh = 30 + (i * 53) % 46;
        rectfill(bx, 130 - bh, bw, bh, (i & 1) ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
        for (int wy = 130 - bh + 4; wy < 126; wy += 9)
            for (int wx = bx + 3; wx < bx + bw - 2; wx += 7)
                if (((wx + wy + i) & 3) == 0) pset(wx, wy, CLR_DARK_ORANGE);
    }
    // tiled street / warehouse (camera-aware)
    camera(cam, 0);
    map(0, 0, 0, 0, MAP_W, MAP_H);
    camera(0, 0);
}

static void draw_hud(void) {
    // hero health + lives
    rectfill(6, 6, 104, 12, CLR_BLACK);
    bar(8, 8, 100, 8, (float)HERO->hp / HERO->maxhp, HERO->hp > 30 ? CLR_GREEN : CLR_RED, CLR_DARK_GREY);
    print("HERO", 8, 20, CLR_WHITE);
    for (int i = 0; i < lives; i++) circfill(46 + i * 9, 24, 3, CLR_BLUE);
    if (HERO->weapon)
        print(HERO->weapon == W_PIPE ? "PIPE" : "KNIFE", 80, 20, CLR_YELLOW);

    print_right(str("SCORE %d", score), SCREEN_W - 6, 8, CLR_WHITE);

    // boss bar
    for (int i = 1; i < MAXF; i++)
        if (f[i].on && f[i].type == E_BOSS) {
            rectfill(70, 184, 182, 11, CLR_BLACK);
            bar(72, 186, 178, 7, (float)f[i].hp / f[i].maxhp, CLR_RED, CLR_DARKER_GREY);
            print_centered("BIG BOSS", SCREEN_W/2, 174, CLR_RED);
        }

    if (combo > 1) print_scaled(str("%dx", combo), SCREEN_W / 2 - 14, 30, CLR_YELLOW, 2);

    if (goArrowX > 0 && blink(14)) {
        print_right("GO!", SCREEN_W - 30, SCREEN_H / 2 - 8, CLR_YELLOW);
        int ax = SCREEN_W - 22, ay = SCREEN_H / 2 - 2;
        tri(ax, ay - 6, ax, ay + 6, ax + 9, ay, CLR_YELLOW);
    }

    if (bannerT > 0) {
        int a = (int)bannerT;
        rectfill(0, SCREEN_H / 2 - 12, SCREEN_W, 22, CLR_BLACK);
        print_centered(bannerS, SCREEN_W/2, SCREEN_H / 2 - 5, (a / 4 % 2) ? CLR_RED : CLR_YELLOW);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    draw_world();

    // barrels (behind actors of greater depth handled loosely — draw before sort)
    camera(cam, 0);
    for (int b = 0; b < MAXB; b++)
        if (barrels[b].on) {
            int x = (int)barrels[b].x, y = (int)barrels[b].y;
            ovalfill(x, y, 7, 2, CLR_DARK_BLUE);
            rectfill(x - 6, y - 14, 12, 14, CLR_BROWN);
            rect(x - 6, y - 14, 12, 14, CLR_DARK_BROWN);
            line(x - 6, y - 9, x + 5, y - 9, CLR_DARK_BROWN);
            line(x - 6, y - 4, x + 5, y - 4, CLR_DARK_BROWN);
        }
    // ground items
    for (int i = 0; i < MAXI; i++)
        if (items[i].on) {
            int x = (int)items[i].x, y = (int)items[i].y - (int)items[i].z;
            if (items[i].kind == K_FOOD) { circfill(x, y - 2, 3, CLR_BROWN); circfill(x, y - 3, 2, CLR_ORANGE); }
            else if (items[i].kind == W_PIPE) { line(x - 6, y, x + 6, y - 2, CLR_LIGHT_GREY); }
            else { line(x - 4, y, x + 4, y, CLR_WHITE); pset(x + 4, y, CLR_LIGHT_GREY); }
        }
    camera(0, 0);

    // y-sorted fighters
    int n = 0;
    for (int i = 0; i < MAXF; i++) if (f[i].on) order[n++] = i;
    for (int i = 1; i < n; i++) { int v = order[i], j = i - 1;
        while (j >= 0 && f[order[j]].y > f[v].y) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
    for (int i = 0; i < n; i++) {
        Fighter *e = &f[order[i]];
        if (e->team == T_HERO && e->iframe > 0 && (frame() % 6 < 3) && e->iframe > 70) continue; // respawn blink
        draw_fighter(e);
    }

    // particles (screen space, but positioned in world → offset by cam)
    for (int i = 0; i < MAXP; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        int x = (int)p->x - cam, y = (int)p->y - (int)p->z;
        if (p->kind == P_STAR) {
            int r = (int)(p->life / 4) + 2;
            line(x - r, y, x + r, y, p->col); line(x, y - r, x, y + r, p->col);
            line(x - r + 1, y - r + 1, x + r - 1, y + r - 1, p->col);
            line(x - r + 1, y + r - 1, x + r - 1, y - r + 1, p->col);
        } else if (p->kind == P_SPARK) {
            pset(x, y, p->col); pset(x + 1, y, p->col);
        } else {
            pset(x, y, p->col);
        }
    }

    draw_hud();

    if (gphase == G_OVER) {
        fade(0.6f);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 12, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 + 2, CLR_YELLOW);
        print_centered("press Z to fight again", SCREEN_W/2, SCREEN_H / 2 + 16, CLR_LIGHT_GREY);
    } else if (gphase == G_WIN) {
        fade(0.5f);
        print_centered("YOU CLEANED UP THE CITY!", SCREEN_W/2, SCREEN_H / 2 - 12, CLR_YELLOW);
        print_centered(str("FINAL SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 + 2, CLR_WHITE);
        print_centered("press Z to play again", SCREEN_W/2, SCREEN_H / 2 + 16, CLR_LIGHT_GREY);
    }
}
