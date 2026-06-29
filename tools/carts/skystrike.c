/* de:meta
{
  "title": "sky strike",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "camera-follow",
    "state-machine"
  ],
  "lineage": "Raiden/1943/Tyrian-style vertical shmup; tilemap scroll with two stacked map() draws for seamless looping, scripted wave timeline, and pal()-recolored enemy squadron variants are the main structural novelties over a generic entity-pool shooter.",
  "genre": "shooter",
  "homage": "Raiden (1990)",
  "description": "A vertical-scrolling shoot-em-up (Raiden / 1943 / Tyrian), built around its weapon-upgrade loop. Your fighter hugs the bottom while a tile map() of sea, islands and an airbase scrolls down past you and waves of enemy craft pour in from the top. Shoot down P pods to climb your main gun L1->L5 (single -> twin -> 3-way -> 4-way -> 5-way spread), then keep grabbing them to bolt on side OPTION wingmen that mirror your fire; B pods stock a screen-clearing bomb that wipes every bullet in a white flash. Six foes off a scripted, escalating timeline — sine-weaving popcorn flyers, poppers and gunships that aim shots at you, ground turrets riding the scroll — all one sprite each, pal()-recolored into whole squadrons and flashed white when hit, capped by a 3-phase WARLORD boss with a health bar. A tight 1-pixel-core hitbox, X focus mode to thread bullet curtains, particle explosions with screen-shake, hit-stop on the boss kill, a driving chiptune bed and a last-life red alarm. Death drops you one weapon tier, never to zero; hi-score saved. Arrows/WASD fly (8-way), Z bomb (gun autofires), X focus/slow, SPACE or ENTER to start / restart."
}
de:meta */
// SKY STRIKE — a vertical-scrolling top-down shmup (Raiden / 1943 / Tyrian).
// The hook is the weapon-upgrade loop: collect P pods to climb your gun L1->L5,
// then bolt on side options; B pods stock a screen-clear bomb. Six enemy types on
// a scripted, escalating wave timeline that caps in a multi-phase boss.
//
// The world is a real tile map() of sea + islands + an airbase that scrolls DOWN
// past a screen-space player (camera + two stacked map draws = seamless loop). One
// enemy sprite is pal()-recolored into a whole squadron; bullets/explosions/pickups
// are primitives. Driving chiptune bed off bpm()/beat()/every(), reactive SFX.

#include "studio.h"

// ---- tunables -------------------------------------------------------------
#define MAX_ENE   48
#define MAX_PB    96
#define MAX_EB   160
#define MAX_PK    16
#define MAX_FX   220

#define HUD_H     20
#define SCROLL    46.0f          // background scroll speed (px/s)
#define MAGIC     28             // sprite body index swapped per enemy by pal()

// sprite slots
#define SPR_PLAYER  0
#define SPR_POPCORN 3
#define SPR_POPPER  4
#define SPR_GUNSHIP 5
#define SPR_TURRET  6
#define SPR_BOSS_TL 8           // 32x32 boss = slots 8,9,10,11 (TL,TR,BL,BR)
#define TILE_OCEAN  40

// enemy types
enum { E_POPCORN, E_POPPER, E_GUNSHIP, E_TURRET };
// game states
enum { ST_TITLE, ST_PLAY, ST_OVER, ST_WIN };
// boss sub-states
enum { BS_NONE, BS_WARN, BS_ACTIVE, BS_DYING };

// ---- structs --------------------------------------------------------------
typedef struct { float x, y; int lives, bombs, wlevel, options; float iframes; } Player;
typedef struct { float x, y, vx, vy; int alive; } PB;          // player bullet
typedef struct { float x, y, vx, vy; int alive, kind; } EB;    // enemy bullet
typedef struct { float x, y, vx, vy, t, fire, ph; int type, hp, alive, flash, variant; } Ene;
typedef struct { float x, y, vy, t; int alive, kind; } PK;     // pickup (0=P,1=B)
typedef struct { float x, y, vx, vy, life, maxlife, size; int alive, kind, color; } FX;
typedef struct { float x, y, t, fire, spin; int hp, maxhp, alive, flash; } Boss;

// ---- globals --------------------------------------------------------------
static int   state;
static int   score, hiscore;
static float gt;                 // seconds since play started
static Player pl;
static Ene   ene[MAX_ENE];
static PB    pb[MAX_PB];
static EB    eb[MAX_EB];
static PK    pk[MAX_PK];
static FX    fx[MAX_FX];
static Boss  boss;

static int   bstate;             // boss sub-state
static float bwarn;              // warning countdown
static float winT;              // win sequence timer
static int   hitstop;            // frames frozen
static float flash; static int  flashcol;   // full-screen flash
static float fcd;                // player fire cooldown
static int   fparity;            // pew every other shot
static float puT;                // "POWER UP!" popup timer
static int   waveIdx;
static float lastWaveT;

// scenery
typedef struct { float x, y, s; } Cloud;
typedef struct { float x, y; } Cap;
static float scroll;
static Cloud clouds[6];
static Cap   caps[20];                       // fast whitecaps (speed lines)

// audio
static int   bstep;
static const int bassseq[8] = { 0, 0, 3, 0, 5, 3, 7, 5 };

// squadron recolor palette for popcorn variants
static const int variants[4] = { CLR_RED, CLR_ORANGE, CLR_PINK, CLR_DARK_ORANGE };
static const int hot[4]      = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_RED };

// ---- wave timeline --------------------------------------------------------
// t=spawn time(s), type, x (formation center), n=count, form (0 stream,1 line,2 V)
typedef struct { float t; int type; float x; int n; int form; } Wave;
static const Wave waves[] = {
    {  2, E_POPCORN,  60, 5, 1 },
    {  6, E_POPCORN, 180, 5, 1 },
    { 10, E_POPCORN,  40, 6, 0 },
    { 14, E_POPPER,  120, 2, 1 },
    { 18, E_POPCORN, 200, 6, 0 },
    { 22, E_POPCORN, 120, 5, 2 },
    { 27, E_TURRET,   50, 2, 1 },
    { 28, E_POPPER,  170, 3, 2 },
    { 33, E_GUNSHIP, 120, 1, 1 },
    { 34, E_POPCORN,  30, 6, 0 },
    { 40, E_POPPER,   80, 4, 1 },
    { 44, E_TURRET,  190, 2, 1 },
    { 46, E_POPCORN, 120, 6, 2 },
    { 52, E_GUNSHIP,  70, 2, 1 },
    { 53, E_POPPER,  160, 3, 0 },
    { 58, E_POPCORN, 120, 7, 1 },
    { 62, E_POPPER,  120, 4, 2 },
    { 66, E_GUNSHIP, 120, 2, 1 },
};
#define NWAVES ((int)(sizeof(waves)/sizeof(waves[0])))

// ---- pool spawners --------------------------------------------------------
static void spawn_fx(float x, float y, float vx, float vy, float life, float size, int color, int kind) {
    for (int i = 0; i < MAX_FX; i++) if (!fx[i].alive) {
        fx[i] = (FX){ x, y, vx, vy, life, life, size, 1, kind, color };
        return;
    }
}

static void explosion(float x, float y, int big) {
    int n = big ? 36 : 14;
    for (int i = 0; i < n; i++) {
        float a = rnd(360), sp = rnd_float_between(big ? 50 : 30, big ? 180 : 95);
        spawn_fx(x, y, dx(sp, a), dy(sp, a), rnd_float_between(0.30f, 0.70f),
                 rnd_float_between(1.6f, 3.6f), hot[rnd(4)], 0);
    }
    spawn_fx(x, y, 0, 0, big ? 0.42f : 0.26f, big ? 30 : 15, CLR_WHITE, 1);   // shock ring
    if (big) {
        spawn_fx(x, y, 0, 0, 0.55f, 42, CLR_ORANGE, 1);
        for (int i = 0; i < 8; i++) {                                        // smoke puffs
            float a = rnd(360), sp = rnd_float_between(8, 28);
            spawn_fx(x, y, dx(sp, a), dy(sp, a), rnd_float_between(0.6f, 1.1f), 4, CLR_DARK_GREY, 3);
        }
        shake(7);
    } else shake(2);
    hit(rnd_between(30, 46), INSTR_NOISE, big ? 6 : 4, big ? 190 : 90);
    if (big) note(40, INSTR_SAW, 4);
}

static void spawn_pb(float x, float y, float ang) {
    for (int i = 0; i < MAX_PB; i++) if (!pb[i].alive) {
        pb[i] = (PB){ x, y, dx(380, ang), dy(380, ang), 1 };
        return;
    }
}

static void spawn_eb(float x, float y, float ang, float spd, int kind) {
    for (int i = 0; i < MAX_EB; i++) if (!eb[i].alive) {
        eb[i] = (EB){ x, y, dx(spd, ang), dy(spd, ang), 1, kind };
        return;
    }
}

static void spawn_pk(float x, float y, int kind) {
    for (int i = 0; i < MAX_PK; i++) if (!pk[i].alive) {
        pk[i] = (PK){ x, y, SCROLL + 12, 0, 1, kind };
        return;
    }
}

static void spawn_enemy(int type, float x, float y, int variant) {
    for (int i = 0; i < MAX_ENE; i++) if (!ene[i].alive) {
        int hp = type == E_POPCORN ? 1 : type == E_POPPER ? 3 : type == E_GUNSHIP ? 8 : 4;
        ene[i] = (Ene){ x, y, 0, 0, 0, rnd_float_between(0.6f, 1.4f),
                        rnd_float_between(0, 360), type, hp, 1, 0, variant };
        return;
    }
}

static void spawn_wave(const Wave *w) {
    for (int i = 0; i < w->n; i++) {
        float x = w->x, y = -16;
        if (w->form == 1) x = w->x + (i - (w->n - 1) / 2.0f) * 26;   // line across
        else if (w->form == 0) { x = w->x; y = -16 - i * 28; }        // stream
        else { x = w->x + (i - (w->n - 1) / 2.0f) * 22; y = -16 - abs(i - (w->n - 1) / 2) * 18; } // V
        if (w->type == E_TURRET) { x = w->x + i * 90; y = -20; }
        spawn_enemy(w->type, clamp(x, 12, SCREEN_W - 12), y, rnd(4));
    }
}

// ---- player ---------------------------------------------------------------
static void fanfare(void) {                       // rising power-up arpeggio
    note(72, 5, 4); schedule(70, 76, 5, 4); schedule(140, 79, 5, 4); schedule(210, 84, 5, 4);
    puT = 1.2f;
}

static void fire_player(void) {
    float x = pl.x, y = pl.y - 8;
    switch (pl.wlevel) {
        case 1: spawn_pb(x, y, 270); break;
        case 2: spawn_pb(x - 4, y, 270); spawn_pb(x + 4, y, 270); break;
        case 3: spawn_pb(x, y, 270); spawn_pb(x, y, 252); spawn_pb(x, y, 288); break;
        case 4: spawn_pb(x - 5, y, 270); spawn_pb(x + 5, y, 270);
                spawn_pb(x, y, 256); spawn_pb(x, y, 284); break;
        default: spawn_pb(x, y, 270); spawn_pb(x, y, 257); spawn_pb(x, y, 283);
                 spawn_pb(x, y, 244); spawn_pb(x, y, 296); break;
    }
    if (pl.options >= 1) spawn_pb(x - 18, pl.y + 4, 270);
    if (pl.options >= 2) spawn_pb(x + 18, pl.y + 4, 270);
    if ((fparity++ & 1) == 0) hit(72, 5, 2, 32);   // short pew, every other shot
}

static void do_bomb(void) {
    if (pl.bombs <= 0 || pl.iframes > 0.8f) return;
    pl.bombs--;
    for (int i = 0; i < MAX_EB; i++) if (eb[i].alive) {     // sweep all enemy bullets to sparks
        spawn_fx(eb[i].x, eb[i].y, 0, 0, 0.25f, 2, CLR_LIGHT_PEACH, 0);
        eb[i].alive = 0;
    }
    for (int i = 0; i < MAX_ENE; i++) if (ene[i].alive) {   // heavy damage to all
        ene[i].hp -= 6; ene[i].flash = 8;
        if (ene[i].hp <= 0) { explosion(ene[i].x, ene[i].y, 1); ene[i].alive = 0; score += 30; }
    }
    if (boss.alive) { boss.hp -= 24; boss.flash = 10; }
    pl.iframes = 1.3f;
    flash = 1.0f; flashcol = CLR_WHITE; shake(9);
    hit(28, INSTR_NOISE, 7, 420); note(48, INSTR_SAW, 5); schedule(120, 36, INSTR_SAW, 4);
}

static void damage_player(void) {
    if (pl.iframes > 0) return;
    explosion(pl.x, pl.y, 1);
    for (int i = 0; i < MAX_EB; i++) eb[i].alive = 0;     // clear the screen on death
    pl.lives--;
    pl.wlevel = max(1, pl.wlevel - 1);                    // drop a tier, never to zero
    if (pl.options > 0) pl.options--;
    pl.iframes = 2.2f;
    pl.x = SCREEN_W / 2; pl.y = SCREEN_H - 40;
    flash = 0.8f; flashcol = CLR_RED; shake(8);
    hit(34, INSTR_NOISE, 6, 240);
    if (pl.lives < 0) {
        state = ST_OVER;
        if (score > hiscore) { hiscore = score; save_int("skystrike_hi", hiscore); }
    }
}

// ---- enemy logic ----------------------------------------------------------
static int enemy_radius(int type) {
    return type == E_POPCORN ? 7 : type == E_POPPER ? 8 : type == E_GUNSHIP ? 12 : 9;
}

static void hurt_enemy(Ene *e, int dmg) {
    e->hp -= dmg; e->flash = 5;
    if (e->hp <= 0) {
        e->alive = 0;
        explosion(e->x, e->y, e->type >= E_GUNSHIP);
        score += e->type == E_POPCORN ? 10 : e->type == E_POPPER ? 30 : e->type == E_GUNSHIP ? 120 : 60;
        // drops
        if (e->type == E_GUNSHIP) spawn_pk(e->x, e->y, chance(40) ? 1 : 0);
        else if (e->type == E_TURRET) { if (chance(40)) spawn_pk(e->x, e->y, chance(35) ? 1 : 0); }
        else if (e->type == E_POPPER) { if (chance(28)) spawn_pk(e->x, e->y, chance(30) ? 1 : 0); }
        else if (chance(7)) spawn_pk(e->x, e->y, 0);
    }
}

static void update_enemy(Ene *e, float d) {
    e->t += d;
    if (e->flash > 0) e->flash--;
    switch (e->type) {
        case E_POPCORN:
            e->y += 78 * d;
            e->x += sin_deg(e->t * 150 + e->ph * 360) * 36 * d;
            break;
        case E_POPPER:
            if (e->y < 70) e->y += 70 * d;
            else { e->y += 14 * d; e->x += sin_deg(e->t * 90) * 40 * d; }
            e->fire -= d;
            if (e->y > 24 && e->fire <= 0) {
                spawn_eb(e->x, e->y + 6, angle_to(e->x, e->y, pl.x, pl.y), 135, 0);
                e->fire = 1.4f;
            }
            break;
        case E_GUNSHIP:
            if (e->y < 56) e->y += 44 * d;
            else e->x += sin_deg(e->t * 50) * 30 * d;
            e->fire -= d;
            if (e->y > 24 && e->fire <= 0) {
                float a = angle_to(e->x, e->y, pl.x, pl.y);
                for (int k = -1; k <= 1; k++) spawn_eb(e->x, e->y + 8, a + k * 16, 120, 1);
                e->fire = 1.7f;
            }
            break;
        case E_TURRET:                                   // rides the scrolling ground
            e->y += SCROLL * d;
            e->fire -= d;
            if (e->y > 24 && e->y < SCREEN_H - 30 && e->fire <= 0) {
                spawn_eb(e->x, e->y, angle_to(e->x, e->y, pl.x, pl.y), 150, 2);
                e->fire = 1.6f;
            }
            break;
    }
    if (e->x < -20 || e->x > SCREEN_W + 20 || e->y > SCREEN_H + 24) e->alive = 0;
}

// ---- boss -----------------------------------------------------------------
static void spawn_boss(void) {
    boss = (Boss){ SCREEN_W / 2, -24, 0, 0, 0, 160, 160, 1, 0 };
    note(48, INSTR_SAW, 5); schedule(160, 55, INSTR_SAW, 5);
}

static void update_boss(float d) {
    boss.t += d;
    if (boss.flash > 0) boss.flash--;
    if (boss.y < 52) { boss.y += 26 * d; return; }       // entrance
    boss.x = SCREEN_W / 2 + sin_deg(boss.t * 32) * (SCREEN_W / 2 - 30);
    boss.fire -= d;
    if (boss.fire > 0) return;
    float frac = (float)boss.hp / boss.maxhp;
    float bx = boss.x, by = boss.y + 12;
    if (frac > 0.66f) {                                  // phase 0: aimed 3-burst
        float a = angle_to(bx, by, pl.x, pl.y);
        for (int k = -1; k <= 1; k++) spawn_eb(bx, by, a + k * 12, 140, 2);
        boss.fire = 0.85f;
    } else if (frac > 0.33f) {                            // phase 1: wide fan + aimed
        for (int k = 0; k < 13; k++) spawn_eb(bx, by, 55 + k * (70.0f / 12), 110, 2);
        spawn_eb(bx, by, angle_to(bx, by, pl.x, pl.y), 165, 1);
        boss.fire = 1.25f;
    } else {                                             // phase 2: spinning spray
        boss.spin += 27;
        for (int k = 0; k < 3; k++) spawn_eb(bx, by, boss.spin + k * 120, 130, 2);
        boss.fire = 0.16f;
    }
}

// ---- the director ---------------------------------------------------------
static int count_enemies(void) {
    int n = 0; for (int i = 0; i < MAX_ENE; i++) if (ene[i].alive) n++;
    return n;
}

static void director(void) {
    while (waveIdx < NWAVES && waves[waveIdx].t <= gt) {
        spawn_wave(&waves[waveIdx]);
        lastWaveT = waves[waveIdx].t;
        waveIdx++;
    }
    if (bstate == BS_NONE && waveIdx >= NWAVES && gt > lastWaveT + 3 && count_enemies() == 0) {
        bstate = BS_WARN; bwarn = 2.6f;
    }
    if (bstate == BS_WARN) {
        bwarn -= dt();
        if (frame() % 20 == 0) note(frame() % 40 == 0 ? 84 : 79, INSTR_SQUARE, 3);  // siren
        if (bwarn <= 0) { spawn_boss(); bstate = BS_ACTIVE; }
    }
}

// ---- audio bed ------------------------------------------------------------
static void audio_step(void) {
    if (every(1)) {
        int b = beat();
        hit(36, INSTR_TRI, 5, 70);                       // kick
        hit(95, INSTR_NOISE, 1, 14);                     // hat
        if (b % 2 == 1) hit(60, INSTR_NOISE, 3, 55);     // snare backbeat
        note(degree(SCALE_MINOR, 2, bassseq[b % 8]), 6, 3);   // bass
        if (b % 4 == 0) note(degree(SCALE_MINOR, 3, 0), 7, 2); // pad/drone
    }
}

// ---- lifecycle ------------------------------------------------------------
static void start_game(void) {
    state = ST_PLAY; gt = 0; score = 0;
    waveIdx = 0; lastWaveT = 0; bstate = BS_NONE; bwarn = 0; winT = 0; hitstop = 0;
    flash = 0; puT = 0; fcd = 0; fparity = 0; bstep = 0;
    pl = (Player){ SCREEN_W / 2, SCREEN_H - 40, 3, 2, 1, 0, 2.0f };
    boss.alive = 0;
    for (int i = 0; i < MAX_ENE; i++) ene[i].alive = 0;
    for (int i = 0; i < MAX_PB; i++)  pb[i].alive = 0;
    for (int i = 0; i < MAX_EB; i++)  eb[i].alive = 0;
    for (int i = 0; i < MAX_PK; i++)  pk[i].alive = 0;
    for (int i = 0; i < MAX_FX; i++)  fx[i].alive = 0;
    bpm(140);
}

static void seed_attract(void) {                          // decorative formation for the title
    for (int i = 0; i < 5; i++)
        spawn_enemy(E_POPCORN, SCREEN_W / 2 + (i - 2) * 24, 60 + abs(i - 2) * 14, i);
    spawn_enemy(E_GUNSHIP, 60, 36, 0);
    spawn_enemy(E_POPPER, SCREEN_W - 56, 44, 1);
}

void init(void) {
    colorkey(CLR_BLACK);
    hiscore = load_int("skystrike_hi", 0);
    instrument(5, INSTR_SQUARE, 2, 40, 3, 50); instrument_duty(5, 0.30f);     // pew / lead
    instrument(6, INSTR_SAW, 3, 90, 4, 120); instrument_filter(6, FILTER_LOW, 620, 6); // bass
    instrument(7, INSTR_SINE, 220, 320, 4, 700);                              // pad drone
    instrument_lfo(7, 0, LFO_PITCH, 5, 0.25f);
    for (int i = 0; i < 6; i++)  clouds[i] = (Cloud){ rnd(SCREEN_W), rnd(SCREEN_H), rnd_float_between(0.7f, 1.4f) };
    for (int i = 0; i < 20; i++) caps[i]   = (Cap){ rnd(SCREEN_W), rnd(SCREEN_H) };
    state = ST_TITLE;
    pl = (Player){ SCREEN_W / 2, SCREEN_H - 40, 3, 2, 1, 0, 0 };
    seed_attract();
}

void update(void) {
    float d = dt();
    scroll += SCROLL * d;

    // scenery scroll (runs in every state, sells motion)
    for (int i = 0; i < 6; i++) {
        clouds[i].y += SCROLL * 1.7f * clouds[i].s * d;
        if (clouds[i].y > SCREEN_H + 24) { clouds[i].y = -24; clouds[i].x = rnd(SCREEN_W); }
    }
    for (int i = 0; i < 20; i++) {
        caps[i].y += SCROLL * 2.3f * d;
        if (caps[i].y > SCREEN_H) { caps[i].y = -4; caps[i].x = rnd(SCREEN_W); }
    }
    if (flash > 0) flash -= d * 3.2f;
    if (puT > 0)   puT  -= d;

    if (state != ST_PLAY) {
        if (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp(KEY_ENTER) || keyp(KEY_SPACE)) {
            if (state == ST_TITLE || state == ST_OVER || state == ST_WIN) start_game();
        }
        return;
    }

    if (hitstop > 0) { hitstop--; if (winT > 0) { winT -= d; } return; }

    gt += d;
    audio_step();
    director();

    // ---- input / player ----
    int lf = btn(0, BTN_LEFT)  || btn(1, BTN_LEFT);
    int rt = btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT);
    int up = btn(0, BTN_UP)    || btn(1, BTN_UP);
    int dn = btn(0, BTN_DOWN)  || btn(1, BTN_DOWN);
    int focus = btn(0, BTN_B)  || btn(1, BTN_B);
    float spd = focus ? 66 : 142;
    if (lf) pl.x -= spd * d;
    if (rt) pl.x += spd * d;
    if (up) pl.y -= spd * d;
    if (dn) pl.y += spd * d;
    pl.x = clamp(pl.x, 9, SCREEN_W - 9);
    pl.y = clamp(pl.y, HUD_H + 6, SCREEN_H - 10);
    if (pl.iframes > 0) pl.iframes -= d;
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) do_bomb();

    fcd -= d;
    if (fcd <= 0) {
        fire_player();
        const float cad[6] = { 0, 0.13f, 0.12f, 0.11f, 0.095f, 0.085f };
        fcd = cad[pl.wlevel];
    }

    // ---- player bullets ----
    for (int i = 0; i < MAX_PB; i++) {
        if (!pb[i].alive) continue;
        pb[i].x += pb[i].vx * d; pb[i].y += pb[i].vy * d;
        if (pb[i].y < -6 || pb[i].x < -6 || pb[i].x > SCREEN_W + 6) { pb[i].alive = 0; continue; }
        for (int j = 0; j < MAX_ENE; j++) {
            if (!ene[j].alive) continue;
            if (distance((int)pb[i].x, (int)pb[i].y, (int)ene[j].x, (int)ene[j].y) < enemy_radius(ene[j].type)) {
                pb[i].alive = 0; hurt_enemy(&ene[j], 1); break;
            }
        }
        if (pb[i].alive && boss.alive && boss.y > 0 &&
            boxes_touch((int)pb[i].x, (int)pb[i].y, 2, 2, (int)boss.x - 14, (int)boss.y - 14, 28, 26)) {
            pb[i].alive = 0; boss.hp--; boss.flash = 3; score += 2;
            if (chance(20)) spawn_fx(pb[i].x, pb[i].y, 0, 0, 0.18f, 2, CLR_YELLOW, 0);
        }
    }

    // ---- enemies ----
    for (int i = 0; i < MAX_ENE; i++) {
        if (!ene[i].alive) continue;
        update_enemy(&ene[i], d);
        if (ene[i].alive && pl.iframes <= 0 &&
            distance((int)pl.x, (int)pl.y, (int)ene[i].x, (int)ene[i].y) < enemy_radius(ene[i].type) + 4) {
            if (ene[i].type != E_TURRET) hurt_enemy(&ene[i], 2);
            damage_player();
        }
    }

    // ---- boss ----
    if (boss.alive) {
        update_boss(d);
        if (boss.hp <= 0 && bstate == BS_ACTIVE) {        // death sequence
            bstate = BS_DYING; boss.alive = 0;
            explosion(boss.x, boss.y, 1);
            hitstop = 16; winT = 1.7f; flash = 1.0f; flashcol = CLR_WHITE; shake(10);
            score += 3000;
            for (int k = 0; k < 6; k++)
                spawn_fx(boss.x + rnd(40) - 20, boss.y + rnd(30) - 15, 0, 0, 0.5f + k * 0.1f, 8, hot[rnd(4)], 1);
        }
    }
    if (bstate == BS_DYING) {
        winT -= d;
        if (frame() % 5 == 0) explosion(boss.x + rnd(48) - 24, boss.y + rnd(36) - 18, 0);
        if (winT <= 0) {
            state = ST_WIN;
            if (score > hiscore) { hiscore = score; save_int("skystrike_hi", hiscore); }
        }
    }

    // ---- enemy bullets ----
    for (int i = 0; i < MAX_EB; i++) {
        if (!eb[i].alive) continue;
        eb[i].x += eb[i].vx * d; eb[i].y += eb[i].vy * d;
        if (eb[i].x < -8 || eb[i].x > SCREEN_W + 8 || eb[i].y < -8 || eb[i].y > SCREEN_H + 8) { eb[i].alive = 0; continue; }
        if (pl.iframes <= 0 && distance((int)eb[i].x, (int)eb[i].y, (int)pl.x, (int)pl.y) < 5) {
            eb[i].alive = 0; damage_player();
        }
    }

    // ---- pickups ----
    for (int i = 0; i < MAX_PK; i++) {
        if (!pk[i].alive) continue;
        pk[i].t += d; pk[i].y += pk[i].vy * d;
        pk[i].x += sin_deg(pk[i].t * 160) * 18 * d;
        if (pk[i].y > SCREEN_H + 10) { pk[i].alive = 0; continue; }
        if (near((int)pl.x, (int)pl.y, (int)pk[i].x, (int)pk[i].y, 12)) {
            pk[i].alive = 0;
            if (pk[i].kind == 0) {                        // weapon pod
                if (pl.wlevel < 5)      { pl.wlevel++;  fanfare(); }
                else if (pl.options < 2){ pl.options++; fanfare(); }
                else score += 200;
            } else {                                      // bomb pod
                pl.bombs = min(pl.bombs + 1, 6);
                note(84, INSTR_SINE, 4); schedule(80, 91, INSTR_SINE, 4);
            }
        }
    }

    // ---- particles ----
    for (int i = 0; i < MAX_FX; i++) {
        if (!fx[i].alive) continue;
        fx[i].x += fx[i].vx * d; fx[i].y += fx[i].vy * d;
        fx[i].vx *= 0.92f; fx[i].vy *= 0.92f;
        fx[i].life -= d;
        if (fx[i].life <= 0) fx[i].alive = 0;
    }
}

// ---- drawing --------------------------------------------------------------
static void draw_world(void) {
    cls(CLR_DARKER_BLUE);
    int worldH = MAP_H * CELL_H;
    int sc = (int)scroll % worldH; if (sc < 0) sc += worldH;
    camera(0, -sc);                                       // two stacked draws = seamless loop
    map(0, 0, 0, -worldH, MAP_W, MAP_H);
    map(0, 0, 0, 0, MAP_W, MAP_H);
    camera(0, 0);                                         // back to screen space

    for (int i = 0; i < 20; i++)                          // fast whitecaps (speed)
        line((int)caps[i].x, (int)caps[i].y, (int)caps[i].x, (int)caps[i].y + 3, CLR_BLUE);

    fillp(FILL_CHECKER, -1);                              // soft dithered clouds, planes fly over them
    for (int i = 0; i < 6; i++) {
        int cx = (int)clouds[i].x, cy = (int)clouds[i].y, w = (int)(12 * clouds[i].s);
        ovalfill(cx, cy, w, w / 2, CLR_LIGHT_GREY);
        ovalfill(cx + w / 2, cy + 2, w * 2 / 3, w / 3, CLR_WHITE);
    }
    fillp_reset();
}

static void draw_enemy(const Ene *e) {
    int x = (int)e->x - 8, y = (int)e->y - 8;
    int slot = e->type == E_POPCORN ? SPR_POPCORN : e->type == E_POPPER ? SPR_POPPER
             : e->type == E_GUNSHIP ? SPR_GUNSHIP : SPR_TURRET;
    if (e->flash > 0) { pal(MAGIC, CLR_WHITE); pal(CLR_BLUE, CLR_WHITE); pal(CLR_BROWNISH_BLACK, CLR_WHITE); }
    else if (e->type == E_POPCORN) pal(MAGIC, variants[e->variant]);
    else if (e->type == E_POPPER)  pal(MAGIC, CLR_GREEN);
    else if (e->type == E_GUNSHIP) pal(MAGIC, CLR_INDIGO);
    else                            pal(MAGIC, CLR_BROWN);
    spr(slot, x, y);
    pal_reset();
}

static void draw_player(int ix, int iy) {
    float tilt = (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) ? 14
               : (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  ? -14 : 0;
    if (pl.options >= 1) { trifill(ix - 22, iy + 9, ix - 14, iy + 9, ix - 18, iy + 1, CLR_BLUE); }
    if (pl.options >= 2) { trifill(ix + 14, iy + 9, ix + 22, iy + 9, ix + 18, iy + 1, CLR_BLUE); }
    spr_rot(SPR_PLAYER, ix - 8, iy - 8, tilt);
    if (btn(0, BTN_B) || btn(1, BTN_B)) circfill(ix, iy, 2, CLR_RED);   // focus: show hitbox
}

static void draw_pickup(const PK *p) {
    int x = (int)p->x, y = (int)p->y;
    int glow = blink(8) ? 2 : 0;
    int outer = p->kind == 0 ? CLR_TRUE_BLUE : CLR_DARK_GREEN;
    int inner = p->kind == 0 ? CLR_BLUE : CLR_GREEN;
    ovalfill(x, y, 8 + glow, 6 + glow, outer);
    ovalfill(x, y, 5, 4, inner);
    print(p->kind == 0 ? "P" : "B", x - 2, y - 3, CLR_WHITE);
}

static void draw_fx(void) {
    for (int i = 0; i < MAX_FX; i++) {
        if (!fx[i].alive) continue;
        float t = fx[i].life / fx[i].maxlife;            // 1 -> 0
        if (fx[i].kind == 1) {                           // shock ring
            int r = (int)((1 - t) * fx[i].size);
            if (r > 0) circ((int)fx[i].x, (int)fx[i].y, r, t > 0.4f ? fx[i].color : CLR_DARK_GREY);
        } else if (fx[i].kind == 3) {                    // smoke
            int r = (int)(fx[i].size * (0.6f + t));
            circfill((int)fx[i].x, (int)fx[i].y, r, CLR_DARK_GREY);
        } else {                                         // spark
            int idx = t > 0.66f ? 0 : t > 0.4f ? 1 : t > 0.18f ? 2 : 3;
            int r = (int)(fx[i].size * t);
            if (r <= 0) pset((int)fx[i].x, (int)fx[i].y, hot[idx]);
            else circfill((int)fx[i].x, (int)fx[i].y, r, hot[idx]);
        }
    }
}

static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BROWNISH_BLACK);
    line(0, HUD_H, SCREEN_W, HUD_H, CLR_DARK_GREY);
    print(str("SC %06d", score), 3, 2, CLR_WHITE);
    print_right(str("HI %06d", hiscore), SCREEN_W - 3, 2, CLR_LIGHT_YELLOW);
    // lives
    for (int i = 0; i < pl.lives; i++) trifill(5 + i * 8, 16, 9 + i * 8, 16, 7 + i * 8, 11, CLR_GREEN);
    // bombs
    for (int i = 0; i < pl.bombs; i++) circfill(SCREEN_W / 2 - 18 + i * 9, 14, 3, CLR_ORANGE);
    // weapon level
    print_right(str("WPN%d", pl.wlevel), SCREEN_W - 30, 12, CLR_BLUE);
    bar(SCREEN_W - 28, 13, 25, 4, pl.wlevel / 5.0f, CLR_BLUE, CLR_DARK_BLUE);

    if (boss.alive && boss.y > 0) {
        print_centered("WARLORD", SCREEN_W/2, HUD_H + 2, CLR_RED);
        bar(20, HUD_H + 11, SCREEN_W - 40, 4, (float)boss.hp / boss.maxhp, CLR_RED, CLR_DARK_RED);
    }
    if (bstate == BS_WARN && blink(8))
        print_centered("!! WARNING !!", SCREEN_W/2, SCREEN_H / 2 - 4, CLR_RED);
    if (puT > 0) {
        int yy = SCREEN_H - 70 - (int)((1.2f - puT) * 30);
        print_centered("POWER UP!", SCREEN_W/2, yy, blink(4) ? CLR_YELLOW : CLR_WHITE);
    }
    // last-life red alarm vignette
    if (pl.lives == 0 && state == ST_PLAY) {
        int a = (int)(2 + sin_deg(now() * 360) * 2);
        if (a > 0) { rect(0, 0, SCREEN_W, SCREEN_H, CLR_RED); rect(1, 1, SCREEN_W - 2, SCREEN_H - 2, CLR_DARK_RED); }
        if (frame() % 30 == 0) note(67, INSTR_SQUARE, 2);
    }
}

void draw(void) {
    draw_world();

    if (state == ST_TITLE) {
        for (int i = 0; i < MAX_ENE; i++) if (ene[i].alive) draw_enemy(&ene[i]);
        // a few decorative shots
        for (int i = 0; i < 3; i++) rectfill(SCREEN_W / 2 - 1, SCREEN_H - 60 - i * 22, 2, 6, CLR_LIGHT_YELLOW);
        draw_player(SCREEN_W / 2, SCREEN_H - 46);
        draw_fx();
        rectfill(0, SCREEN_H / 2 - 40, SCREEN_W, 80, CLR_BROWNISH_BLACK);
        print_scaled("SKY", SCREEN_W / 2 - 48, SCREEN_H / 2 - 34, CLR_LIGHT_YELLOW, 4);
        print_scaled("STRIKE", SCREEN_W / 2 - 96, SCREEN_H / 2 - 2, CLR_RED, 4);
        if (blink(20)) print_centered("PRESS Z / ENTER", SCREEN_W/2, SCREEN_H / 2 + 34, CLR_WHITE);
        print_centered("arrows/WASD fly  Z bomb  X focus", SCREEN_W/2, SCREEN_H - 14, CLR_LIGHT_GREY);
        print_right(str("HI %06d", hiscore), SCREEN_W - 4, 4, CLR_LIGHT_YELLOW);
        return;
    }

    // pickups under everything
    for (int i = 0; i < MAX_PK; i++) if (pk[i].alive) draw_pickup(&pk[i]);
    // enemies + boss
    for (int i = 0; i < MAX_ENE; i++) if (ene[i].alive) draw_enemy(&ene[i]);
    if (boss.alive && boss.y > -16) {
        int bx = (int)boss.x - 16, by = (int)boss.y - 16;
        if (boss.flash > 0) { pal(MAGIC, CLR_WHITE); pal(CLR_RED, CLR_WHITE); pal(CLR_BLUE, CLR_WHITE); }
        else pal(MAGIC, boss.hp < boss.maxhp / 3 ? CLR_DARK_RED : CLR_INDIGO);
        spr(SPR_BOSS_TL,     bx,      by);
        spr(SPR_BOSS_TL + 1, bx + 16, by);
        spr(SPR_BOSS_TL + 2, bx,      by + 16);
        spr(SPR_BOSS_TL + 3, bx + 16, by + 16);
        pal_reset();
    }
    // enemy bullets
    for (int i = 0; i < MAX_EB; i++) {
        if (!eb[i].alive) continue;
        int x = (int)eb[i].x, y = (int)eb[i].y;
        if (eb[i].kind == 0)      { circfill(x, y, 2, CLR_PINK); }
        else if (eb[i].kind == 1) { circfill(x, y, 2, CLR_ORANGE); pset(x, y, CLR_WHITE); }
        else                      { circfill(x, y, 3, CLR_RED); circfill(x, y, 1, CLR_LIGHT_PEACH); }
    }
    // player bullets
    for (int i = 0; i < MAX_PB; i++) if (pb[i].alive)
        rectfill((int)pb[i].x - 1, (int)pb[i].y - 3, 2, 6, CLR_LIGHT_YELLOW);
    // player (blink while invulnerable)
    if ((state == ST_PLAY) && (pl.iframes <= 0 || ((int)(now() * 18) & 1)))
        draw_player((int)pl.x, (int)pl.y);
    draw_fx();

    if (flash > 0) {
        int f = (int)(flash * 4);
        fillp(f >= 3 ? FILL_SOLID : f == 2 ? FILL_CHECKER : FILL_DOTS, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, flashcol);
        fillp_reset();
    }

    draw_hud();

    if (state == ST_OVER) {
        fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
        print_scaled("GAME OVER", SCREEN_W / 2 - 71, SCREEN_H / 2 - 20, CLR_RED, 2);
        print_centered(str("SCORE %06d", score), SCREEN_W/2, SCREEN_H / 2 + 4, CLR_WHITE);
        if (score >= hiscore) print_centered("NEW HI-SCORE!", SCREEN_W/2, SCREEN_H / 2 + 16, CLR_LIGHT_YELLOW);
        if (blink(20)) print_centered("PRESS Z / ENTER", SCREEN_W/2, SCREEN_H / 2 + 34, CLR_LIGHT_GREY);
    } else if (state == ST_WIN) {
        print_scaled("STAGE", SCREEN_W / 2 - 35, SCREEN_H / 2 - 26, CLR_LIGHT_YELLOW, 2);
        print_scaled("CLEAR!", SCREEN_W / 2 - 47, SCREEN_H / 2 - 6, CLR_GREEN, 2);
        print_centered(str("SCORE %06d", score), SCREEN_W/2, SCREEN_H / 2 + 18, CLR_WHITE);
        if (blink(20)) print_centered("PRESS Z / ENTER", SCREEN_W/2, SCREEN_H / 2 + 34, CLR_LIGHT_GREY);
    }
}
