#include "studio.h"

// HOTLINE — top-down, one-hit-kill, instant-retry neon carnage.
//
// Storm a building floor and clear every room. You die in ONE hit; so does every
// thug. Start with a bat, pry guns off the corpses, kick doors into the guards
// behind them, and chain kills for a multiplier. Clear the floor, walk to the
// exit, do it again — three floors of it.
//
//   MOVE      WASD
//   AIM       mouse (twin-stick feel)
//   ATTACK    LEFT-CLICK   (bat swing, or fire your gun)
//   THROW     RIGHT-CLICK / Q  — hurl your gun to knock a guard down, then finish
//   KICK DOOR SPACE  — bursts the nearest door; a guard behind it goes down
//   PICK UP   walk over a dropped weapon
//   RESTART   R  (instant)
//
// Built on the cannonfodder LOS + tilemap idiom, robotron-style entity/bullet
// pools, the crowd.c pal()-recolor trick (one body sprite → player/thug/guard),
// and a driving synthwave bed (filtered saw bass + LFO cutoff, square arp,
// euclid-ish drums). Blood pools persist, kills punch with shake + flash +
// hit-stop, and the last kill of a floor drops into slow-mo.

// ---- tiles (double as sprite-sheet slots) ----
#define T_FLOOR    1
#define T_WALL     2
#define T_DOOR     3   // closed — blocks move + LOS
#define T_DOOROPEN 4
#define T_EXIT     5

// ---- world (cells) ----
#define MW 30
#define MH 18
#define WORLD_W (MW * 16)
#define WORLD_H (MH * 16)
#define CAM_ZOOM 1.14f   // slight zoom in — the HM look, and overscan that hides the tilt's corner gaps
#define CAM_TILT 2.5f    // subtle constant world rotation (degrees), à la Hotline Miami
#define CAM_BORDER 40    // let the camera overscan the map by this much, so the neon field shows around the room

// ---- sprite + magic recolor ----
#define SPR_GUY    0
#define MAGIC_SHIRT 28   // pal()'d per entity type

// ---- pools ----
#define MAXENE   16
#define MAXBUL   140
#define MAXPART  220
#define MAXPICK  16
#define MAXDECAL 240
#define MAXDOOR  12
#define MAXTHROW 4
#define NFLOORS  3

enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_WIN };
enum { TYPE_THUG, TYPE_GUARD };
enum { S_PATROL, S_INVEST, S_CHASE };
enum { W_BAT, W_PISTOL, W_SHOTGUN, W_UZI };
enum { K_BLOOD, K_CASING, K_SPARK };

typedef struct {
    bool on; float x, y, aim;
    int type, state, weapon;
    float hx, hy;                 // home (patrol anchor)
    float scan, wander;           // idle scan timer / wander timer
    float fire_cd, telegraph, muzzle;
    float invx, invy, inv_t;      // investigate target + timeout
    float down_t;                 // knocked down (free kill) timer
} Ent;

typedef struct { bool on; float x, y, vx, vy, life; bool enemy; } Bullet;
typedef struct { bool on; float x, y, vx, vy, life, size; int color, kind; } Part;
typedef struct { bool on; float x, y, t; int weapon, ammo; } Pickup;
typedef struct { bool on; int cx, cy; } Door;
typedef struct { bool on; float x, y, r; int color; } Decal;
typedef struct { bool on; float x, y, vx, vy, spin; int weapon, ammo; float life; } Thrown;

// ---- player ----
static struct {
    float x, y, aim;
    int weapon, ammo;
    bool alive;
    float swing, fire_cd, muzzle;
} pl;

// ---- pools ----
static Ent     enes[MAXENE];
static Bullet  buls[MAXBUL];
static Part    parts[MAXPART];
static Pickup  picks[MAXPICK];
static Door    doors[MAXDOOR];
static Decal   decals[MAXDECAL];
static Thrown  throws[MAXTHROW];
static int     ndoors, decal_head;

// ---- game state ----
static int   state, cur_floor, score, best;
static int   combo;
static float combo_t, flash_t, time_scale;
static int   freeze;
static bool  cleared;
static int   cam_x, cam_y;
static bool  cheat = false;   // C toggles: unlimited ammo (+ auto-arm) for testing
static int   last_step = -1;
static float clear_t;

// ---- weapon stats ----
static const char *WNAME[4]   = { "BAT", "PISTOL", "SHOTGUN", "UZI" };
static const int   WAMMO[4]    = { 0, 7, 5, 26 };
static const float WCD[4]      = { 0, 0.20f, 0.62f, 0.07f };
static const int   WPELLET[4]  = { 0, 1, 5, 1 };
static const float WSPREAD[4]  = { 0, 1.5f, 13.0f, 5.0f };

// =====================================================================
// helpers
// =====================================================================

static int enemies_left(void) {
    int n = 0;
    for (int i = 0; i < MAXENE; i++) if (enes[i].on) n++;
    return n;
}

// any living enemy actively chasing = "spotted" → the music goes hectic
static bool any_chasing(void) {
    for (int i = 0; i < MAXENE; i++)
        if (enes[i].on && enes[i].down_t <= 0 && enes[i].state == S_CHASE) return true;
    return false;
}

static bool passable_px(float x, float y) {
    int t = tile_at((int)x, (int)y);
    return t == T_FLOOR || t == T_DOOROPEN || t == T_EXIT;
}

// LOS clear unless a wall or a closed door blocks it
static bool los(float x1, float y1, float x2, float y2) {
    float d = distance((int)x1, (int)y1, (int)x2, (int)y2);
    int n = (int)(d / 6.0f);
    if (n < 1) return true;
    for (int k = 1; k < n; k++) {
        float t = (float)k / n;
        int tt = tile_at((int)lerp(x1, x2, t), (int)lerp(y1, y2, t));
        if (tt == T_WALL || tt == T_DOOR || tt == 0) return false;
    }
    return true;
}

static void add_part(float x, float y, float vx, float vy, float life, float size, int color, int kind) {
    for (int i = 0; i < MAXPART; i++) if (!parts[i].on) {
        parts[i] = (Part){ true, x, y, vx, vy, life, size, color, kind };
        return;
    }
}

static void add_decal(float x, float y, float r, int color) {
    decals[decal_head] = (Decal){ true, x, y, r, color };
    decal_head = (decal_head + 1) % MAXDECAL;
}

static void splatter(float x, float y, int n) {
    for (int k = 0; k < n; k++) {
        float a = rnd(360), s = rnd_float_between(28, 110);
        add_part(x, y, dx(s, a), dy(s, a), rnd_float_between(0.2f, 0.5f),
                 rnd_float_between(1, 2), (k & 3) ? CLR_RED : CLR_DARK_RED, K_BLOOD);
    }
    add_decal(x, y, rnd_float_between(4, 7), CLR_DARK_RED);
    add_decal(x + rnd_between(-5, 5), y + rnd_between(-5, 5), rnd_float_between(2, 4), CLR_RED);
}

static void add_bullet(float x, float y, float deg, float spd, bool enemy) {
    for (int i = 0; i < MAXBUL; i++) if (!buls[i].on) {
        buls[i] = (Bullet){ true, x, y, dx(spd, deg), dy(spd, deg), 1.1f, enemy };
        return;
    }
}

static void add_pickup(float x, float y, int weapon, int ammo) {
    for (int i = 0; i < MAXPICK; i++) if (!picks[i].on) {
        picks[i] = (Pickup){ true, x, y, 0, weapon, ammo };
        return;
    }
}

// gunfire is LOUD — ping nearby guards to come investigate
static void hear_shot(float x, float y) {
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (!e->on || e->state == S_CHASE || e->down_t > 0) continue;
        if (near((int)x, (int)y, (int)e->x, (int)e->y, 155)) {
            e->state = S_INVEST; e->invx = x; e->invy = y; e->inv_t = 4.0f;
        }
    }
}

static void kill_enemy(Ent *e, bool by_player) {
    e->on = false;
    splatter(e->x, e->y, 12);
    shake(4);
    flash_t = clamp(flash_t + 0.35f, 0, 0.7f);
    freeze = 3;                                   // hit-stop
    hit(60 + rnd(8), INSTR_NOISE, 4, 35);
    note(34, INSTR_TRI, 5);
    if (e->weapon != W_BAT)                        // drop the gun off the corpse
        add_pickup(e->x, e->y, e->weapon, max(2, WAMMO[e->weapon] / 2));
    if (by_player) {
        combo++; combo_t = 2.2f;
        score += 100 * combo;
    }
    if (enemies_left() == 0) {                      // last kill of the floor → slow-mo
        cleared = true; clear_t = 0;
        time_scale = 0.22f;
        // bass-drop sting
        schedule(0,   33, INSTR_SAW, 6);
        schedule(110, 31, INSTR_SAW, 6);
        schedule(240, 28, INSTR_SAW, 7);
        hit(40, INSTR_NOISE, 5, 200);
    }
}

static void kill_player(void) {
    if (!pl.alive) return;
    pl.alive = false;
    splatter(pl.x, pl.y, 16);
    shake(7);
    flash_t = 0.6f;
    time_scale = 0.3f;
    state = ST_DEAD;
    schedule(0,   46, INSTR_SAW, 5);
    schedule(140, 41, INSTR_SAW, 5);
    schedule(300, 35, INSTR_SAW, 6);
}

static void knockdown_near(float x, float y, float r) {
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (e->on && near((int)x, (int)y, (int)e->x, (int)e->y, (int)r)) {
            e->down_t = 1.7f; e->state = S_CHASE;
            add_part(e->x, e->y, 0, 0, 0.2f, 2, CLR_LIGHT_PEACH, K_SPARK);
        }
    }
}

// kick the nearest closed door open; anyone behind it goes down
static void kick_door(void) {
    int best = -1; float bd = 30;
    for (int i = 0; i < ndoors; i++) {
        if (!doors[i].on) continue;
        float cx = doors[i].cx * 16 + 8, cy = doors[i].cy * 16 + 8;
        float d = distance((int)pl.x, (int)pl.y, (int)cx, (int)cy);
        if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) return;
    doors[best].on = false;
    int cx = doors[best].cx, cy = doors[best].cy;
    mset(cx, cy, T_DOOROPEN);
    knockdown_near(cx * 16 + 8, cy * 16 + 8, 22);
    shake(3);
    hit(30, INSTR_NOISE, 6, 110);
    note(33, INSTR_TRI, 5);
    for (int k = 0; k < 6; k++) {
        float a = rnd(360), s = rnd_float_between(20, 70);
        add_part(cx * 16 + 8, cy * 16 + 8, dx(s, a), dy(s, a), 0.3f, 1, CLR_BROWN, K_SPARK);
    }
}

// =====================================================================
// player attacks
// =====================================================================

static void melee_swing(void) {
    pl.swing = 0.16f;
    hit(38, INSTR_NOISE, 4, 70);
    note(31, INSTR_TRI, 4);
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (!e->on) continue;
        float d = distance((int)pl.x, (int)pl.y, (int)e->x, (int)e->y);
        if (d > 19) continue;
        float ad = e->down_t > 0 ? 180 : 75;       // downed = kill from any angle
        float diff = abs((int)(angle_to((int)pl.x, (int)pl.y, (int)e->x, (int)e->y) - pl.aim));
        while (diff > 180) diff -= 360;
        if (abs((int)diff) <= ad) kill_enemy(e, true);
    }
}

static void fire_gun(void) {
    int w = pl.weapon;
    if (pl.ammo <= 0 || pl.fire_cd > 0) return;
    pl.ammo--; pl.fire_cd = WCD[w]; pl.muzzle = 0.06f;
    for (int p = 0; p < WPELLET[w]; p++) {
        float sp = rnd_float_between(-WSPREAD[w], WSPREAD[w]);
        add_bullet(pl.x + dx(9, pl.aim), pl.y + dy(9, pl.aim), pl.aim + sp, 235, false);
    }
    // brass + sound + recoil
    add_part(pl.x, pl.y, dx(40, pl.aim - 90), dy(40, pl.aim - 90), 0.5f, 1, CLR_YELLOW, K_CASING);
    shake(w == W_SHOTGUN ? 3.0f : 1.2f);
    if (w == W_SHOTGUN) hit(44, INSTR_NOISE, 6, 80);
    else if (w == W_UZI) hit(58 + rnd(6), INSTR_NOISE, 3, 24);
    else hit(52, INSTR_NOISE, 5, 45);
    hear_shot(pl.x, pl.y);
}

static void throw_gun(void) {
    if (pl.weapon == W_BAT) return;
    for (int i = 0; i < MAXTHROW; i++) if (!throws[i].on) {
        throws[i] = (Thrown){ true, pl.x + dx(8, pl.aim), pl.y + dy(8, pl.aim),
                              dx(260, pl.aim), dy(260, pl.aim), 0, pl.weapon, pl.ammo, 0.8f };
        pl.weapon = W_BAT; pl.ammo = 0;
        hit(48, INSTR_NOISE, 3, 30);
        return;
    }
}

// =====================================================================
// build a floor
// =====================================================================

static void clear_pools(void) {
    for (int i = 0; i < MAXENE;   i++) enes[i].on   = false;
    for (int i = 0; i < MAXBUL;   i++) buls[i].on   = false;
    for (int i = 0; i < MAXPART;  i++) parts[i].on  = false;
    for (int i = 0; i < MAXPICK;  i++) picks[i].on  = false;
    for (int i = 0; i < MAXDECAL; i++) decals[i].on = false;
    for (int i = 0; i < MAXTHROW; i++) throws[i].on = false;
    decal_head = 0; ndoors = 0;
}

static void carve(int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (xx >= 0 && xx < MW && yy >= 0 && yy < MH) mset(xx, yy, T_FLOOR);
}

static void put_door(int cx, int cy) {
    if (ndoors < MAXDOOR) { doors[ndoors++] = (Door){ true, cx, cy }; mset(cx, cy, T_DOOR); }
}

static void add_enemy(int cx, int cy, int type, int weapon) {
    for (int i = 0; i < MAXENE; i++) if (!enes[i].on) {
        Ent *e = &enes[i];
        *e = (Ent){0};
        e->on = true; e->type = type; e->weapon = weapon; e->state = S_PATROL;
        e->x = e->hx = cx * 16 + 8; e->y = e->hy = cy * 16 + 8;
        e->aim = rnd(360); e->wander = rnd_float_between(0.3f, 1.8f);
        e->fire_cd = rnd_float_between(0.4f, 1.2f);
        return;
    }
}

static void build_floor(int n) {
    clear_pools();
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) mset(x, y, T_WALL);

    int pcx = 3, pcy = 3, ecx = 27, ecy = 15;

    if (n == 0) {
        carve(1, 1, 13, 7);  carve(15, 1, 14, 7);
        carve(1, 9, 13, 8);  carve(15, 9, 14, 8);
        put_door(14, 4); put_door(14, 12); put_door(7, 8); put_door(21, 8);
        pcx = 3; pcy = 3; ecx = 27; ecy = 15;
        add_enemy(20, 3, TYPE_THUG,  W_BAT);
        add_enemy(25, 4, TYPE_GUARD, W_PISTOL);
        add_enemy(5, 13, TYPE_GUARD, W_PISTOL);
        add_enemy(10, 14, TYPE_THUG, W_BAT);
        add_enemy(20, 12, TYPE_GUARD, W_SHOTGUN);
        add_enemy(25, 11, TYPE_GUARD, W_PISTOL);
        add_pickup(12 * 16 + 8, 6 * 16 + 8, W_PISTOL, 7);
    } else if (n == 1) {
        carve(1, 1, 9, 7);  carve(11, 1, 9, 7);  carve(21, 1, 8, 7);
        carve(1, 9, 9, 8);  carve(11, 9, 9, 8);  carve(21, 9, 8, 8);
        put_door(10, 4); put_door(20, 4);
        put_door(10, 13); put_door(20, 13);
        put_door(5, 8); put_door(15, 8); put_door(25, 8);
        pcx = 3; pcy = 3; ecx = 26; ecy = 14;
        add_enemy(15, 3, TYPE_THUG,  W_BAT);
        add_enemy(25, 3, TYPE_GUARD, W_PISTOL);
        add_enemy(23, 5, TYPE_GUARD, W_SHOTGUN);
        add_enemy(5, 12, TYPE_GUARD, W_PISTOL);
        add_enemy(8, 15, TYPE_THUG,  W_BAT);
        add_enemy(15, 12, TYPE_GUARD, W_UZI);
        add_enemy(13, 15, TYPE_THUG, W_BAT);
        add_enemy(24, 11, TYPE_GUARD, W_PISTOL);
        add_enemy(26, 15, TYPE_GUARD, W_SHOTGUN);
        add_pickup(8 * 16 + 8, 6 * 16 + 8, W_PISTOL, 7);
    } else {
        carve(1, 1, 9, 7);  carve(11, 1, 9, 7);  carve(21, 1, 8, 7);
        carve(1, 9, 9, 8);  carve(11, 9, 9, 8);  carve(21, 9, 8, 8);
        put_door(10, 4); put_door(20, 4);
        put_door(10, 13); put_door(20, 13);
        put_door(5, 8); put_door(15, 8); put_door(25, 8);
        pcx = 3; pcy = 15; ecx = 26; ecy = 3;
        add_enemy(8, 15, TYPE_THUG,  W_BAT);
        add_enemy(15, 12, TYPE_GUARD, W_SHOTGUN);
        add_enemy(13, 15, TYPE_THUG, W_BAT);
        add_enemy(25, 15, TYPE_GUARD, W_UZI);
        add_enemy(23, 11, TYPE_GUARD, W_PISTOL);
        add_enemy(5, 3, TYPE_GUARD,  W_PISTOL);
        add_enemy(7, 6, TYPE_THUG,   W_BAT);
        add_enemy(15, 3, TYPE_GUARD, W_UZI);
        add_enemy(13, 6, TYPE_THUG,  W_BAT);
        add_enemy(25, 3, TYPE_GUARD, W_SHOTGUN);
        add_enemy(23, 6, TYPE_GUARD, W_PISTOL);
        add_pickup(8 * 16 + 8, 15 * 16 + 8, W_SHOTGUN, 5);
    }

    mset(ecx, ecy, T_EXIT);
    pl.x = pcx * 16 + 8; pl.y = pcy * 16 + 8; pl.aim = 0;
    pl.weapon = W_BAT; pl.ammo = 0; pl.alive = true;
    pl.swing = pl.fire_cd = pl.muzzle = 0;
    cleared = false; combo = 0; combo_t = 0;
    flash_t = 0; time_scale = 1; freeze = 0; clear_t = 0;
}

static void restart_floor(void) { build_floor(cur_floor); state = ST_PLAY; }

static void start_game(void) { cur_floor = 0; score = 0; build_floor(0); state = ST_PLAY; }

// =====================================================================
// init
// =====================================================================

void init(void) {
    colorkey(0);
    bpm(126);
    best = load(0);
    // synthwave instruments
    instrument(5, INSTR_SAW, 6, 130, 6, 110);          // bass
    instrument_filter(5, FILTER_LOW, 700, 9);
    instrument_lfo(5, 0, LFO_CUTOFF, 0.45f, 1000.0f);   // slow filter sweep
    instrument(6, INSTR_SQUARE, 3, 70, 3, 80);          // arp / lead
    instrument_duty(6, 0.30f);
    state = ST_TITLE;
    cur_floor = 0;
    build_floor(0);
}

// =====================================================================
// the synthwave bed — driven off the synth's own beat clock
// =====================================================================

static void music_tick(bool full) {
    int step = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (step == last_step) return;
    last_step = step;
    int s = step % 64;                 // 4 bars × 16 sixteenths
    int bar = s / 16, st = s % 16;
    int roots[4] = { 45, 41, 48, 43 }; // A minor: i  VI  III  VII
    int root = roots[bar];

    if (full) {
        if (st % 4 == 0)              hit(36, INSTR_TRI,   5, 95);   // kick (4 on floor)
        if (st == 4 || st == 12)      hit(58, INSTR_NOISE, 4, 110);  // snare backbeat
        if (st % 2 == 1)              hit(86, INSTR_NOISE, 2, 26);   // offbeat hat
    }
    // syncopated saw bass with the LFO-swept lowpass
    if (st == 0 || st == 3 || st == 6 || st == 8 || st == 11 || st == 14)
        hit(root - 12, 5, 5, 150);
    // square arp climbing the minor triad
    int arp[4] = { 0, 3, 7, 12 };
    if (st % 2 == 0)
        hit(root + 12 + arp[(st / 2) % 4], 6, full ? 3 : 2, 90);
}

// =====================================================================
// update
// =====================================================================

static void move_axis(float *px, float *py, float nx, float ny) {
    if (passable_px(nx, *py)) *px = nx;
    if (passable_px(*px, ny)) *py = ny;
}

static void update_play(void) {
    float d = dt() * time_scale;
    time_scale = lerp(time_scale, 1.0f, dt() * 2.5f);
    if (flash_t > 0) flash_t = clamp(flash_t - dt() * 2.2f, 0, 1);
    if (combo_t > 0) { combo_t -= dt(); if (combo_t <= 0) combo = 0; }
    if (cleared) clear_t += dt();

    // cheat: press C for unlimited ammo (auto-arms an uzi if you're on the bat)
    if (keyp('C')) cheat = !cheat;
    if (cheat && pl.alive) {
        if (pl.weapon == W_BAT) pl.weapon = W_UZI;
        pl.ammo = 999;
    }

    // ---- player ----
    if (pl.alive) {
        if (pl.swing > 0)   pl.swing  -= d;
        if (pl.fire_cd > 0) pl.fire_cd -= d;
        if (pl.muzzle > 0)  pl.muzzle -= d;

        // move FIRST, so the camera + aim below reflect where the player actually is
        int mvx = (key('D') ? 1 : 0) - (key('A') ? 1 : 0);
        int mvy = (key('S') ? 1 : 0) - (key('W') ? 1 : 0);
        if (mvx || mvy) {
            float a = angle_to(0, 0, (int)(mvx * 100), (int)(mvy * 100));
            float step = 80.0f * d;
            move_axis(&pl.x, &pl.y, pl.x + dx(step, a), pl.y + dy(step, a));
        }

        // Set the camera HERE in update(), from the just-moved position, BEFORE reading
        // the world-mouse. draw() ends with camera(0,0) for the HUD, so without this the
        // mouse_world_*() below would invert the IDENTITY camera (= raw screen coords)
        // and the aim drifts off as the world scrolls. (Same trap as the worldpointer cart.)
        cam_x = (int)clamp(pl.x - SCREEN_W / 2, -CAM_BORDER, WORLD_W - SCREEN_W + CAM_BORDER);
        cam_y = (int)clamp(pl.y - SCREEN_H / 2, -CAM_BORDER, WORLD_H - SCREEN_H + CAM_BORDER);
        camera_ex(cam_x, cam_y, CAM_ZOOM, CAM_TILT);
        pl.aim = angle_to((int)pl.x, (int)pl.y, mouse_world_x(), mouse_world_y());

        // attack
        if (pl.weapon == W_BAT) {
            if (mouse_pressed(MOUSE_LEFT)) melee_swing();
        } else {
            bool wants = (pl.weapon == W_UZI) ? mouse_down(MOUSE_LEFT) : mouse_pressed(MOUSE_LEFT);
            if (wants) fire_gun();
        }
        if (mouse_pressed(MOUSE_RIGHT) || keyp('Q')) throw_gun();
        if (keyp(KEY_SPACE)) kick_door();

        // pick up a weapon by walking over it
        for (int i = 0; i < MAXPICK; i++) {
            if (!picks[i].on || picks[i].t < 0.4f) continue;
            if (near((int)pl.x, (int)pl.y, (int)picks[i].x, (int)picks[i].y, 11)) {
                if (pl.weapon != W_BAT)                       // swap: drop current gun
                    add_pickup(pl.x, pl.y, pl.weapon, pl.ammo);
                pl.weapon = picks[i].weapon; pl.ammo = picks[i].ammo;
                picks[i].on = false;
                note(64, INSTR_SINE, 3);
            }
        }

        // reach the exit once the floor is clear
        if (cleared && tile_at((int)pl.x, (int)pl.y) == T_EXIT) {
            cur_floor++;
            if (cur_floor >= NFLOORS) {
                state = ST_WIN;
                if (score > best) { best = score; save(0, best); }
            } else build_floor(cur_floor);
            return;
        }
    }

    for (int i = 0; i < MAXPICK; i++) if (picks[i].on) picks[i].t += dt();

    // ---- enemies ----
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (!e->on) continue;
        if (e->muzzle > 0) e->muzzle -= d;

        if (e->down_t > 0) {                       // knocked down — helpless, then gets up
            e->down_t -= d;
            continue;
        }
        if (e->fire_cd > 0) e->fire_cd -= d;

        bool see = pl.alive &&
                   near((int)e->x, (int)e->y, (int)pl.x, (int)pl.y, 100) &&
                   los(e->x, e->y, pl.x, pl.y);
        if (see) {
            float diff = angle_to((int)e->x, (int)e->y, (int)pl.x, (int)pl.y) - e->aim;
            while (diff > 180) diff -= 360; while (diff < -180) diff += 360;
            if (e->state == S_CHASE || abs((int)diff) < 34) {
                if (e->state != S_CHASE) note(72, INSTR_SQUARE, 3);  // alert blip
                e->state = S_CHASE; e->invx = pl.x; e->invy = pl.y; e->inv_t = 3.0f;
            }
        }

        if (e->state == S_CHASE) {
            float pd = distance((int)e->x, (int)e->y, (int)pl.x, (int)pl.y);
            if (pl.alive && los(e->x, e->y, pl.x, pl.y)) { e->invx = pl.x; e->invy = pl.y; e->inv_t = 3.0f; }
            e->aim = angle_to((int)e->x, (int)e->y, (int)e->invx, (int)e->invy);

            if (e->type == TYPE_THUG) {            // charge to melee
                float step = 66.0f * d;
                move_axis(&e->x, &e->y, e->x + dx(step, e->aim), e->y + dy(step, e->aim));
                if (pl.alive && pd < 13) kill_player();
            } else {                                // ranged: telegraph, then fire
                if (pd > 78) {
                    float step = 50.0f * d;
                    move_axis(&e->x, &e->y, e->x + dx(step, e->aim), e->y + dy(step, e->aim));
                }
                bool can = pl.alive && pd < 130 && los(e->x, e->y, pl.x, pl.y);
                if (can && e->telegraph <= 0 && e->fire_cd <= 0) e->telegraph = 0.36f;
                if (e->telegraph > 0) {
                    e->telegraph -= d;
                    if (e->telegraph <= 0 && can) {
                        int pel = (e->weapon == W_SHOTGUN) ? 4 : 1;
                        float spr = (e->weapon == W_SHOTGUN) ? 12 : 5;
                        for (int p = 0; p < pel; p++)
                            add_bullet(e->x + dx(8, e->aim), e->y + dy(8, e->aim),
                                       e->aim + rnd_float_between(-spr, spr), 150, true);
                        e->muzzle = 0.06f;
                        e->fire_cd = (e->weapon == W_UZI) ? 0.16f : rnd_float_between(0.9f, 1.5f);
                        if (e->weapon == W_SHOTGUN) hit(44, INSTR_NOISE, 4, 70);
                        else hit(54, INSTR_NOISE, 3, 35);
                    }
                }
            }
            // lose the player → investigate last spot
            e->inv_t -= d;
            if (e->inv_t <= 0 && !see) e->state = S_INVEST, e->inv_t = 2.5f;
        } else if (e->state == S_INVEST) {
            e->aim = angle_to((int)e->x, (int)e->y, (int)e->invx, (int)e->invy);
            float pd = distance((int)e->x, (int)e->y, (int)e->invx, (int)e->invy);
            if (pd > 8) {
                float step = 44.0f * d;
                move_axis(&e->x, &e->y, e->x + dx(step, e->aim), e->y + dy(step, e->aim));
            }
            e->inv_t -= d;
            if (e->inv_t <= 0) { e->state = S_PATROL; e->wander = 0; }
        } else {                                    // PATROL — wander + scan near home
            e->wander -= d;
            if (e->wander <= 0) { e->wander = rnd_float_between(0.8f, 2.4f); e->aim = rnd(360); }
            float step = 16.0f * d;
            float nx = e->x + dx(step, e->aim), ny = e->y + dy(step, e->aim);
            if (distance((int)nx, (int)ny, (int)e->hx, (int)e->hy) < 46 && passable_px(nx, ny)) {
                e->x = nx; e->y = ny;
            } else e->wander = 0;
        }
    }

    // ---- bullets ----
    for (int i = 0; i < MAXBUL; i++) {
        Bullet *b = &buls[i];
        if (!b->on) continue;
        b->x += b->vx * d; b->y += b->vy * d; b->life -= d;
        int t = tile_at((int)b->x, (int)b->y);
        if (b->life <= 0 || t == T_WALL || t == T_DOOR || t == 0) {
            if (t == T_WALL || t == T_DOOR) add_part(b->x, b->y, 0, 0, 0.15f, 1, CLR_LIGHT_PEACH, K_SPARK);
            b->on = false; continue;
        }
        if (b->enemy) {
            if (pl.alive && near((int)b->x, (int)b->y, (int)pl.x, (int)pl.y, 5)) { b->on = false; kill_player(); }
        } else {
            for (int j = 0; j < MAXENE; j++)
                if (enes[j].on && near((int)b->x, (int)b->y, (int)enes[j].x, (int)enes[j].y, 7)) {
                    b->on = false; kill_enemy(&enes[j], true); break;
                }
        }
    }

    // ---- thrown weapons ----
    for (int i = 0; i < MAXTHROW; i++) {
        Thrown *t = &throws[i];
        if (!t->on) continue;
        t->x += t->vx * d; t->y += t->vy * d; t->spin += 720 * d; t->life -= d;
        int tile = tile_at((int)t->x, (int)t->y);
        bool stop = (t->life <= 0 || tile == T_WALL || tile == T_DOOR || tile == 0);
        for (int j = 0; j < MAXENE; j++)
            if (enes[j].on && near((int)t->x, (int)t->y, (int)enes[j].x, (int)enes[j].y, 9)) {
                enes[j].down_t = 1.8f; enes[j].state = S_CHASE;
                shake(2); hit(40, INSTR_NOISE, 4, 60);
                add_part(enes[j].x, enes[j].y, 0, 0, 0.2f, 2, CLR_LIGHT_PEACH, K_SPARK);
                stop = true; break;
            }
        if (stop) { add_pickup(t->x, t->y, t->weapon, t->ammo); t->on = false; }
    }

    // ---- particles ----
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        p->x += p->vx * d; p->y += p->vy * d;
        if (p->kind == K_BLOOD) p->vy += 60 * d;
        p->vx *= 0.92f; p->vy *= 0.92f;
        p->life -= d;
        if (p->life <= 0) {
            if (p->kind == K_BLOOD) add_decal(p->x, p->y, rnd_float_between(1, 2.5f), CLR_DARK_RED);
            p->on = false;
        }
    }

    // ---- the music bed ---- calm (bass + soft arp) until you're spotted, then the
    // full hectic kit (kick/snare/hats + brighter arp) kicks in
    music_tick(any_chasing());
}

// =====================================================================
// update wrapper
// =====================================================================

void update(void) {
    if (keyp('R') && state != ST_TITLE) {
        if (state == ST_WIN) { state = ST_TITLE; return; }
        restart_floor(); return;
    }
    if (state == ST_TITLE) {
        music_tick(false);
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp(KEY_ENTER)) start_game();
    } else if (state == ST_PLAY) {
        if (freeze > 0) { freeze--; music_tick(true); }
        else update_play();
    } else if (state == ST_DEAD) {
        music_tick(true);
        time_scale = lerp(time_scale, 1.0f, dt() * 2.0f);
        if (flash_t > 0) flash_t -= dt() * 2.0f;
        // age the blood/particles so the death scene settles
        for (int i = 0; i < MAXPART; i++) {
            Part *p = &parts[i];
            if (!p->on) continue;
            p->x += p->vx * dt(); p->y += p->vy * dt();
            if (p->kind == K_BLOOD) p->vy += 60 * dt();
            p->vx *= 0.9f; p->vy *= 0.9f; p->life -= dt();
            if (p->life <= 0) p->on = false;
        }
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) restart_floor();
    } else { // ST_WIN
        music_tick(true);
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) state = ST_TITLE;
    }
}

// =====================================================================
// draw
// =====================================================================

static void floor_tint(int n) {
    // the floor base is the very-dark idx 18 (DARKER_PURPLE) — lift it on every floor so
    // the room reads bright/neon instead of murky. (tune these CLR_* to taste)
    if (n == 0) {                                   // neon indigo/magenta
        pal(CLR_DARKER_PURPLE, CLR_INDIGO);  pal(CLR_DARK_PURPLE, CLR_MAUVE);
    } else if (n == 1) {                            // cyan
        pal(CLR_PINK, CLR_BLUE); pal(CLR_DARK_PURPLE, CLR_BLUE_GREEN);
        pal(CLR_DARKER_PURPLE, CLR_TRUE_BLUE);
    } else if (n == 2) {                            // hot red
        pal(CLR_PINK, CLR_ORANGE); pal(CLR_DARK_PURPLE, CLR_DARK_RED);
        pal(CLR_DARKER_PURPLE, CLR_DARK_RED);
    }
}

static void draw_weapon_in_hand(float x, float y, float aim, int weapon, float muzzle) {
    int hx = (int)(x + dx(8, aim)), hy = (int)(y + dy(8, aim));
    int len = weapon == W_SHOTGUN ? 11 : weapon == W_UZI ? 8 : weapon == W_BAT ? 12 : 6;
    int col = weapon == W_BAT ? CLR_BROWN : CLR_LIGHT_GREY;
    int tx = (int)(x + dx(8 + len, aim)), ty = (int)(y + dy(8 + len, aim));
    line(hx, hy, tx, ty, col);
    if (weapon == W_BAT) line(hx, hy, tx + (int)dx(1, aim), ty + (int)dy(1, aim), CLR_MEDIUM_GREY);
    if (weapon != W_BAT && muzzle > 0) {
        circfill(tx, ty, 2, CLR_YELLOW); pset(tx, ty, CLR_WHITE);
    }
}

static void draw_guy(float fx, float fy, float aim, int shirt, bool down, int weapon, float muzzle) {
    int x = (int)fx, y = (int)fy;
    ovalfill(x, y + 5, 5, 2, CLR_BROWNISH_BLACK);   // shadow
    if (down) {
        pal(MAGIC_SHIRT, CLR_DARK_GREY);
        spr_rot(SPR_GUY, x - 8, y - 8, aim + 80);    // sprawled
        pal_reset();
        return;
    }
    draw_weapon_in_hand(fx, fy, aim, weapon, muzzle);
    pal(MAGIC_SHIRT, shirt);
    spr_rot(SPR_GUY, x - 8, y - 8, aim);
    pal_reset();
}

static void draw_world(void) {
    cam_x = (int)clamp(pl.x - SCREEN_W / 2, -CAM_BORDER, WORLD_W - SCREEN_W + CAM_BORDER);
    cam_y = (int)clamp(pl.y - SCREEN_H / 2, -CAM_BORDER, WORLD_H - SCREEN_H + CAM_BORDER);
    camera_ex(cam_x, cam_y, CAM_ZOOM, CAM_TILT);

    floor_tint(cur_floor);
    map(0, 0, 0, 0, MW, MH);
    pal_reset();

    // exit pulse
    if (cleared) {
        for (int i = 0; i < MH; i++) for (int j = 0; j < MW; j++)
            if (mget(j, i) == T_EXIT) {
                int r = 6 + (int)(sin_deg(now() * 360) * 3);
                circ(j * 16 + 8, i * 16 + 8, r, CLR_GREEN);
            }
    }

    // blood pools / decals (under everything)
    for (int i = 0; i < MAXDECAL; i++)
        if (decals[i].on) ovalfill((int)decals[i].x, (int)decals[i].y,
                                   (int)decals[i].r, (int)(decals[i].r * 0.75f), decals[i].color);

    // weapon pickups
    for (int i = 0; i < MAXPICK; i++) {
        if (!picks[i].on) continue;
        int x = (int)picks[i].x, y = (int)picks[i].y;
        if (blink(18)) circ(x, y, 5, CLR_INDIGO);
        draw_weapon_in_hand(x - 4, y, 0, picks[i].weapon, 0);
    }

    // vision cones (alive, awake enemies)
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (!e->on || e->down_t > 0) continue;
        float a = e->aim, r = 100;
        int col = e->state == S_CHASE ? CLR_RED : e->state == S_INVEST ? CLR_ORANGE : CLR_YELLOW;
        int x0 = (int)e->x, y0 = (int)e->y;
        int x1 = (int)(e->x + dx(r, a - 33)), y1 = (int)(e->y + dy(r, a - 33));
        int xm = (int)(e->x + dx(r, a)),      ym = (int)(e->y + dy(r, a));
        int x2 = (int)(e->x + dx(r, a + 33)), y2 = (int)(e->y + dy(r, a + 33));
        fillp(FILL_DOTS, -1);
        trifill(x0, y0, x1, y1, xm, ym, col);
        trifill(x0, y0, xm, ym, x2, y2, col);
        fillp_reset();
        if (e->telegraph > 0) line(x0, y0, (int)(e->x + dx(140, a)), (int)(e->y + dy(140, a)), CLR_RED);
    }

    // bullets
    for (int i = 0; i < MAXBUL; i++)
        if (buls[i].on) {
            int c = buls[i].enemy ? CLR_PINK : CLR_LIGHT_YELLOW;
            line((int)buls[i].x, (int)buls[i].y,
                 (int)(buls[i].x - buls[i].vx * 0.02f), (int)(buls[i].y - buls[i].vy * 0.02f), c);
        }

    // thrown weapons (spinning)
    for (int i = 0; i < MAXTHROW; i++)
        if (throws[i].on) draw_weapon_in_hand(throws[i].x - 4, throws[i].y, throws[i].spin, throws[i].weapon, 0);

    // enemies (recolored: thug = brown, guard = red)
    for (int i = 0; i < MAXENE; i++) {
        Ent *e = &enes[i];
        if (!e->on) continue;
        int shirt = e->type == TYPE_THUG ? CLR_DARK_ORANGE : CLR_RED;
        draw_guy(e->x, e->y, e->aim, shirt, e->down_t > 0, e->weapon, e->muzzle);
    }

    // player + bat swing arc
    if (pl.alive) {
        if (pl.swing > 0) {
            float a = pl.aim, sweep = remap(pl.swing, 0, 0.16f, 50, -10);
            int x1 = (int)(pl.x + dx(20, a + sweep - 30)), y1 = (int)(pl.y + dy(20, a + sweep - 30));
            int x2 = (int)(pl.x + dx(20, a + sweep + 30)), y2 = (int)(pl.y + dy(20, a + sweep + 30));
            trifill((int)pl.x, (int)pl.y, x1, y1, x2, y2, CLR_WHITE);
        }
        draw_guy(pl.x, pl.y, pl.aim, CLR_WHITE, false, pl.weapon, pl.muzzle);
    }

    // particles
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        circfill((int)p->x, (int)p->y, (int)p->size, p->color);
    }

    camera(0, 0);
}

static void draw_hud(void) {
    print(str("SCORE %d", score), 4, 4, CLR_WHITE);
    print_right(str("BEST %d", best), SCREEN_W - 4, 4, CLR_MEDIUM_GREY);
    print(str("FLOOR %d/%d", cur_floor + 1, NFLOORS), 4, 13, CLR_PINK);

    // weapon + ammo
    const char *wn = WNAME[pl.weapon];
    if (pl.weapon == W_BAT) print(wn, SCREEN_W / 2 - text_width(wn) / 2, SCREEN_H - 10, CLR_LIGHT_GREY);
    else {
        const char *t = cheat ? str("%s  INF", wn) : str("%s  %d", wn, pl.ammo);
        print(t, SCREEN_W / 2 - text_width(t) / 2, SCREEN_H - 10, pl.ammo > 0 ? CLR_YELLOW : CLR_RED);
    }
    if (cheat) print_centered("CHEAT: UNLIMITED AMMO", 30, CLR_GREEN);

    // combo multiplier pop
    if (combo > 1) {
        int sc = combo_t > 1.6f ? 3 : 2;
        const char *c = str("x%d", combo);
        print_scaled(c, SCREEN_W / 2 - text_width(c) * sc / 2, 22, CLR_ORANGE, sc);
    }

    // enemies-left ticker
    print_right(str("LEFT %d", enemies_left()), SCREEN_W - 4, 13, enemies_left() ? CLR_RED : CLR_GREEN);

    if (cleared && pl.alive) {
        if (blink(20)) print_centered("FLOOR CLEAR - REACH THE EXIT", 36, CLR_GREEN);
    }

    // crosshair
    int mx = mouse_x(), my = mouse_y();
    circ(mx, my, 4, CLR_WHITE);
    pset(mx, my, CLR_RED);
}

static void panel(const char *title, int tcol, const char *l1, const char *l2, const char *l3) {
    fade(0.5f);
    int w = 240, h = 96, x = SCREEN_W / 2 - w / 2, y = SCREEN_H / 2 - h / 2;
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);
    rect(x, y, w, h, tcol);
    print_scaled(title, SCREEN_W / 2 - text_width(title) * 2 / 2, y + 8, tcol, 2);
    if (l1) print_centered(l1, y + 36, CLR_WHITE);
    if (l2) print_centered(l2, y + 50, CLR_LIGHT_GREY);
    if (l3) print_centered(l3, y + 76, blink(24) ? CLR_YELLOW : CLR_LIGHT_GREY);
}

// the abstract neon space the room floats in — a slow two-colour dither field that
// drifts from one colour to the next over time (the Hotline-Miami void). screen-space.
static void draw_bg(void) {
    static const int field[] = { CLR_DARKER_BLUE, CLR_INDIGO, CLR_DARK_PURPLE,
                                 CLR_MAUVE, CLR_TRUE_BLUE, CLR_DARK_RED };
    int n = 6;
    int i = ((int)(now() * 0.4f)) % n;           // step to the next colour every ~2.5s
    int j = (i + 1) % n;
    rectfill(0, 0, SCREEN_W, SCREEN_H, field[i]);
    fillp(FILL_CHECKER, -1);                     // 50/50 dither toward the next colour = a slow lerp
    rectfill(0, 0, SCREEN_W, SCREEN_H, field[j]);
    fillp_reset();
}

void draw(void) {
    fade(0);             // clear last frame's fade — panel() re-applies it on title/dead/win,
                         // so gameplay isn't left permanently dimmed by the title's fade(0.5)
    camera(0, 0);        // identity for the screen-space background field
    draw_bg();           // neon space around the room (replaces the plain black void)
    draw_world();
    draw_hud();

    if (flash_t > 0.04f) {
        fillp(flash_t > 0.45f ? FILL_SOLID : FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
        fillp_reset();
    }

    if (state == ST_TITLE) {
        panel("HOTLINE", CLR_PINK, "One hit kills. So do you.",
              "WASD move - mouse aim - click attack",
              "click / SPACE to begin");
        print_centered("RIGHT-CLICK throw gun   SPACE kick door   R retry", SCREEN_H / 2 + 36, CLR_INDIGO);
    } else if (state == ST_DEAD) {
        panel("YOU DIED", CLR_RED, "Do not hesitate.",
              str("score %d", score), "R / click to retry");
    } else if (state == ST_WIN) {
        panel("CLEARED", CLR_GREEN, "The building is silent now.",
              str("final score %d   best %d", score, best), "click to continue");
    }
}
