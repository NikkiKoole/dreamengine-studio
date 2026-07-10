/* de:meta
{
  "slug": "missilecmd",
  "title": "Missile Command",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "particle-system",
    "title-play-gameover-loop"
  ],
  "lineage": "Faithful port of Missile Command (1980); expanding detonation rings and chained kills are the signature mechanic preserved from the arcade original.",
  "genre": "shooter",
  "homage": "Missile Command (1980)",
  "description": "ICBMs streak down a night sky in long glowing diagonals, each one falling toward one of your six cities. You command three batteries with limited ammo per wave; aim the crosshair and click, and a counter-missile launches from the nearest battery with shells to spare, races to the cursor, and blooms into an expanding ring that vaporizes any warhead it touches — chain-detonating into more rings as it goes. Survive the wave to refill ammo and bank a bonus for every city and shell left standing; each wave throws more missiles, faster. When the last city falls it's the end. Mouse to aim, left-click to fire from the nearest battery, click or SPACE to defend again."
}
de:meta */
#include "studio.h"

// MISSILE COMMAND — defend 6 cities from a rain of ICBMs.
//
// Pure primitives over a night sky: glowing diagonal trails fall from the top,
// you mouse-aim and click to launch a counter-missile from the NEAREST battery
// with ammo. It flies to the cursor and DETONATES into an expanding ring; any
// enemy missile the ring touches is vaporized (and scores). Ammo is limited per
// wave and refills between waves; waves escalate. All 6 cities down = game over.
//
// MOUSE: aim - LEFT-CLICK fire from nearest battery - click/SPACE to restart.

// ---- layout ----
#define GROUND_Y   (SCREEN_H - 18)        // top of the ground strip
#define N_CITY     6
#define N_BATTERY  3

// ---- pools ----
#define MAX_ENEMY  20                     // incoming ICBMs in flight
#define MAX_SHOT   12                     // player counter-missiles in flight
#define MAX_BLAST  16                     // detonation rings
#define MAX_PART   200                    // sparks + debris

// ---- game states ----
#define G_PLAY   0
#define G_WAVE   1                        // between-wave breather
#define G_OVER   2

typedef struct {
    float x, y;          // current head position
    float sx, sy;        // start (for the trail line)
    float vx, vy;        // velocity (px/frame at 60fps baseline)
    float speed;
    int   target;        // ground x it's aimed at
    bool  on;
} Enemy;

typedef struct {
    float x, y;          // current head
    float sx, sy;        // launch origin (battery muzzle)
    float tx, ty;        // detonation target (where the cursor was)
    float vx, vy;
    bool  on;
} Shot;

typedef struct {
    float x, y;
    float r, maxr;
    float life, maxlife;
    bool  growing;       // grow to maxr, then shrink
    bool  on;
} Blast;

typedef struct {
    float x, y, vx, vy, grav, life;
    int   maxlife, color;
    float size;
} Part;

typedef struct { float x; int alive; int flash; } City;
typedef struct { float x; int ammo; } Battery;

static Enemy  enemies[MAX_ENEMY];
static Shot   shots[MAX_SHOT];
static Blast  blasts[MAX_BLAST];
static Part   parts[MAX_PART];
static City   cities[N_CITY];
static Battery batteries[N_BATTERY];

static int   gstate;
static int   wave;
static int   score, best;
static int   to_spawn;        // ICBMs still to launch this wave
static float spawn_cd;        // seconds until next ICBM
static float wave_t;          // breather countdown
static float crosshair_blink;
static int   flash;           // white-screen frames after a detonation
static int   alarm_cd;        // last-city alarm

static const int AMMO_PER_WAVE = 10;

// =====================================================================
// pools
// =====================================================================

static void spark(float x, float y, float vx, float vy, float grav,
                  int life, float size, int color) {
    for (int i = 0; i < MAX_PART; i++) {
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, vx, vy, grav, (float)life, life, color, size };
            return;
        }
    }
}

static void puff(float x, float y, int n, int c) {
    for (int i = 0; i < n; i++) {
        float a = rnd(360), s = rnd_float_between(0.4f, 2.6f);
        spark(x, y, dx(s, a), dy(s, a), 0.03f, rnd_between(12, 26),
              rnd_float_between(1, 2.5f), c);
    }
}

static void make_blast(float x, float y, float maxr) {
    for (int i = 0; i < MAX_BLAST; i++) {
        if (!blasts[i].on) {
            blasts[i] = (Blast){ x, y, 1, maxr, 1, 1, true, true };
            return;
        }
    }
}

// the signature detonation: ring + sparks + sound + kick
static void detonate(float x, float y, float maxr) {
    make_blast(x, y, maxr);
    puff(x, y, 10, CLR_ORANGE);
    puff(x, y, 6, CLR_YELLOW);
    shake(3);
    flash = 2;
    hit(34, 5, 5, 160);          // chunky filtered-noise boom (instrument slot 5)
    note(40, INSTR_SINE, 4);
}

// =====================================================================
// spawning / waves
// =====================================================================

static int live_cities(void) {
    int n = 0;
    for (int i = 0; i < N_CITY; i++) if (cities[i].alive) n++;
    return n;
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMY; i++) {
        if (enemies[i].on) continue;
        Enemy *e = &enemies[i];
        // aim at a surviving city most of the time, else a battery
        int tx;
        if (chance(78) && live_cities() > 0) {
            int pick = rnd(N_CITY), guard = 0;
            while (!cities[pick].alive && guard++ < 32) pick = rnd(N_CITY);
            tx = (int)cities[pick].x;
        } else {
            tx = (int)batteries[rnd(N_BATTERY)].x;
        }
        e->sx = rnd_between(8, SCREEN_W - 8);
        e->sy = -4;
        e->x = e->sx; e->y = e->sy;
        e->target = tx;
        float spd = 0.30f + wave * 0.07f;          // escalate speed
        e->speed = spd;
        float a = angle_to((int)e->sx, (int)e->sy, tx, GROUND_Y);
        e->vx = dx(spd, a);
        e->vy = dy(spd, a);
        e->on = true;
        return;
    }
}

static int wave_quota(void) { return 6 + wave * 2; }

static void start_wave(void) {
    to_spawn = wave_quota();
    spawn_cd = 0.6f;
    for (int i = 0; i < N_BATTERY; i++) batteries[i].ammo = AMMO_PER_WAVE;
    gstate = G_PLAY;
}

static void new_game(void) {
    for (int i = 0; i < MAX_ENEMY; i++) enemies[i].on = false;
    for (int i = 0; i < MAX_SHOT;  i++) shots[i].on = false;
    for (int i = 0; i < MAX_BLAST; i++) blasts[i].on = false;
    for (int i = 0; i < MAX_PART;  i++) parts[i].life = 0;

    // 6 cities in two clusters, with batteries at left / center / right edges
    int margin = 38;
    int span = SCREEN_W - margin * 2;
    for (int i = 0; i < N_CITY; i++) {
        // skip the center column where the middle battery sits
        int slot = i < 3 ? i : i + 1;        // 0,1,2, _ ,4,5,6
        cities[i].x = margin + span * (slot + 1) / 8;
        cities[i].alive = 1;
        cities[i].flash = 0;
    }
    batteries[0].x = 12;
    batteries[1].x = SCREEN_W / 2;
    batteries[2].x = SCREEN_W - 12;

    wave = 1; score = 0; flash = 0; alarm_cd = 0;
    start_wave();
}

// =====================================================================
// firing
// =====================================================================

static void fire_at(int mx, int my) {
    if (my > GROUND_Y - 4) my = GROUND_Y - 4;          // don't aim into the dirt
    // nearest battery WITH ammo
    int bi = -1; float bd = 1e9f;
    for (int i = 0; i < N_BATTERY; i++) {
        if (batteries[i].ammo <= 0) continue;
        float d = distance((int)batteries[i].x, GROUND_Y, mx, my);
        if (d < bd) { bd = d; bi = i; }
    }
    if (bi < 0) { hit(80, INSTR_SQUARE, 1, 25); return; }   // dry click

    for (int i = 0; i < MAX_SHOT; i++) {
        if (shots[i].on) continue;
        Shot *s = &shots[i];
        s->sx = batteries[bi].x; s->sy = GROUND_Y - 6;
        s->x = s->sx; s->y = s->sy;
        s->tx = mx; s->ty = my;
        float a = angle_to((int)s->sx, (int)s->sy, mx, my);
        float spd = 4.2f;
        s->vx = dx(spd, a); s->vy = dy(spd, a);
        s->on = true;
        batteries[bi].ammo--;
        note(72, INSTR_SQUARE, 2);          // launch chirp
        return;
    }
}

// =====================================================================
// init / update
// =====================================================================

void init(void) {
    bpm(120);
    // slot 5 — the detonation: a short filtered-noise thud
    instrument(5, INSTR_NOISE, 0, 60, 0, 90);
    instrument_filter(5, FILTER_LOW, 900, 4);
    best = load(0);
    new_game();

    // a lively first frame for the thumbnail: missiles falling + a blast underway
    enemies[0].on = true; enemies[0].sx = 40; enemies[0].sy = -4;
    enemies[0].x = 120; enemies[0].y = 70; enemies[0].vx = 0.5f; enemies[0].vy = 0.5f;
    enemies[1].on = true; enemies[1].sx = 280; enemies[1].sy = -4;
    enemies[1].x = 220; enemies[1].y = 96; enemies[1].vx = -0.4f; enemies[1].vy = 0.6f;
    shots[0].on = true; shots[0].sx = SCREEN_W/2; shots[0].sy = GROUND_Y-6;
    shots[0].x = 170; shots[0].y = 90; shots[0].tx = 170; shots[0].ty = 80;
    shots[0].vx = -0.5f; shots[0].vy = -2;
    make_blast(150, 70, 18); blasts[0].r = 14;
    puff(150, 70, 8, CLR_ORANGE);
}

void update(void) {
    float k = dt() * 60.0f;
    crosshair_blink += dt();

    if (flash > 0) flash--;

    // particles + blasts always animate
    for (int i = 0; i < MAX_PART; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].vy += parts[i].grav * k;
        parts[i].x  += parts[i].vx * k;
        parts[i].y  += parts[i].vy * k;
        parts[i].life -= k;
    }
    for (int i = 0; i < MAX_BLAST; i++) {
        Blast *b = &blasts[i];
        if (!b->on) continue;
        if (b->growing) {
            b->r += 0.55f * k;
            if (b->r >= b->maxr) { b->r = b->maxr; b->growing = false; }
        } else {
            b->r -= 0.45f * k;
            if (b->r <= 0) { b->on = false; }
        }
    }

    if (gstate == G_OVER) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) new_game();
        return;
    }

    if (gstate == G_WAVE) {
        wave_t -= dt();
        if (wave_t <= 0) { wave++; start_wave(); }
        return;
    }

    // ---- last-city alarm bed ----
    if (live_cities() == 1) {
        alarm_cd -= (int)k;
        if (alarm_cd <= 0) { note(50, INSTR_SQUARE, 2); alarm_cd = 32; }
    }

    // ---- input: fire ----
    if (mouse_pressed(MOUSE_LEFT)) fire_at(mouse_x(), mouse_y());

    // ---- spawn drip ----
    if (to_spawn > 0) {
        spawn_cd -= dt();
        if (spawn_cd <= 0) {
            spawn_enemy();
            to_spawn--;
            float gap = clamp(1.4f - wave * 0.06f, 0.35f, 1.4f);
            spawn_cd = rnd_float_between(gap * 0.5f, gap);
        }
    }

    // ---- player counter-missiles ----
    for (int i = 0; i < MAX_SHOT; i++) {
        Shot *s = &shots[i];
        if (!s->on) continue;
        s->x += s->vx * k; s->y += s->vy * k;
        // arrived at target? (passed it along the travel direction)
        if (distance((int)s->x, (int)s->y, (int)s->sx, (int)s->sy)
            >= distance((int)s->tx, (int)s->ty, (int)s->sx, (int)s->sy)) {
            s->on = false;
            detonate(s->tx, s->ty, 16);
        }
    }

    // ---- incoming ICBMs ----
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &enemies[i];
        if (!e->on) continue;
        e->x += e->vx * k; e->y += e->vy * k;

        // hit the ground / a city / a battery?
        if (e->y >= GROUND_Y - 2) {
            e->on = false;
            // which city did it land on?
            for (int c = 0; c < N_CITY; c++) {
                if (cities[c].alive && near((int)e->x, GROUND_Y, (int)cities[c].x, GROUND_Y, 12)) {
                    cities[c].alive = 0; cities[c].flash = 12;
                    puff(cities[c].x, GROUND_Y - 4, 18, CLR_RED);
                    puff(cities[c].x, GROUND_Y - 4, 10, CLR_ORANGE);
                    shake(5); flash = 3;
                    hit(28, INSTR_NOISE, 6, 320);
                    schedule(60, 24, INSTR_SAW, 4);   // descending loss sting
                }
            }
            detonate(e->x, GROUND_Y - 4, 12);
        }
    }

    // ---- blast rings destroy enemy missiles ----
    for (int b = 0; b < MAX_BLAST; b++) {
        if (!blasts[b].on) continue;
        for (int i = 0; i < MAX_ENEMY; i++) {
            Enemy *e = &enemies[i];
            if (!e->on) continue;
            if (near((int)e->x, (int)e->y, (int)blasts[b].x, (int)blasts[b].y, (int)blasts[b].r)) {
                e->on = false;
                score += 25 + wave * 5;
                puff(e->x, e->y, 7, CLR_YELLOW);
                puff(e->x, e->y, 3, CLR_WHITE);
                note(60 + rnd(8), INSTR_SQUARE, 3);
                // small chain blast
                make_blast(e->x, e->y, 9);
            }
        }
    }

    // ---- end-of-wave check ----
    bool any_enemy = false;
    for (int i = 0; i < MAX_ENEMY; i++) if (enemies[i].on) { any_enemy = true; break; }
    if (to_spawn == 0 && !any_enemy) {
        // wave cleared — bonus per surviving city + leftover ammo
        int survivors = live_cities();
        if (survivors == 0) {
            // (handled below by game-over check, but guard anyway)
        } else {
            for (int i = 0; i < N_BATTERY; i++) score += batteries[i].ammo * 3;
            score += survivors * 50;
            gstate = G_WAVE; wave_t = 2.0f;
            note(72, INSTR_SINE, 4); schedule(160, 79, INSTR_SINE, 4);
        }
    }

    // ---- game over ----
    if (live_cities() == 0 && gstate == G_PLAY) {
        gstate = G_OVER;
        if (score > best) { best = score; save(0, best); }
        shake(8);
        for (int i = 0; i < N_BATTERY; i++) detonate(batteries[i].x, GROUND_Y - 6, 14);
    }
}

// =====================================================================
// draw
// =====================================================================

static void draw_city(City *c) {
    int x = (int)c->x;
    int base = GROUND_Y - 1;
    if (c->alive) {
        // a little cluster of buildings
        int col = CLR_BLUE;
        rectfill(x - 7, base - 6, 4, 6, col);
        rectfill(x - 2, base - 9, 4, 9, col);
        rectfill(x + 3, base - 5, 4, 5, col);
        // lit windows
        pset(x - 6, base - 4, CLR_LIGHT_YELLOW);
        pset(x - 1, base - 6, CLR_LIGHT_YELLOW);
        pset(x + 4, base - 3, CLR_LIGHT_YELLOW);
    } else {
        // rubble
        rectfill(x - 6, base - 2, 12, 2, CLR_DARK_GREY);
        pset(x - 4, base - 3, CLR_DARK_GREY);
        pset(x + 3, base - 3, CLR_DARK_GREY);
    }
}

static void draw_battery(Battery *b) {
    int x = (int)b->x;
    int base = GROUND_Y;
    // a turret mound
    trifill(x - 8, base, x + 8, base, x, base - 9, b->ammo > 0 ? CLR_DARK_GREEN : CLR_DARKER_GREY);
    // stacked ammo dots
    for (int i = 0; i < b->ammo; i++) {
        int col = i + (i % 3) * 2;            // unused but keeps it tidy
        (void)col;
        pset(x - 6 + (i % 5) * 3, base - 2 - (i / 5) * 3, CLR_LIME_GREEN);
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // a faint horizon glow
    fillp(FILL_CHECKER, -1);
    rectfill(0, GROUND_Y - 24, SCREEN_W, 24, CLR_DARKER_BLUE);
    fillp_reset();

    // ground
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_GREEN);
    line(0, GROUND_Y, SCREEN_W, GROUND_Y, CLR_MEDIUM_GREEN);

    // cities + batteries
    for (int i = 0; i < N_CITY; i++) draw_city(&cities[i]);
    for (int i = 0; i < N_BATTERY; i++) draw_battery(&batteries[i]);

    // incoming ICBM trails + heads
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &enemies[i];
        if (!e->on) continue;
        line((int)e->sx, (int)e->sy, (int)e->x, (int)e->y, CLR_DARK_RED);
        line((int)e->x, (int)e->y - 1, (int)e->x, (int)e->y, CLR_RED);
        pset((int)e->x, (int)e->y, CLR_YELLOW);
    }

    // player counter-missile trails + heads
    for (int i = 0; i < MAX_SHOT; i++) {
        Shot *s = &shots[i];
        if (!s->on) continue;
        line((int)s->sx, (int)s->sy, (int)s->x, (int)s->y, CLR_BLUE_GREEN);
        pset((int)s->x, (int)s->y, CLR_WHITE);
        // tiny target marker where it will detonate
        pset((int)s->tx, (int)s->ty, CLR_LIGHT_GREY);
    }

    // blast rings
    for (int i = 0; i < MAX_BLAST; i++) {
        Blast *b = &blasts[i];
        if (!b->on) continue;
        float t = b->r / b->maxr;
        int c = t > 0.7f ? CLR_WHITE : (t > 0.4f ? CLR_YELLOW : CLR_ORANGE);
        circfill((int)b->x, (int)b->y, (int)b->r, c);
        circ((int)b->x, (int)b->y, (int)b->r, CLR_RED);
    }

    // particles
    for (int i = 0; i < MAX_PART; i++) {
        if (parts[i].life <= 0) continue;
        float t = parts[i].life / (float)parts[i].maxlife;
        int r = (int)(parts[i].size * (0.4f + t));
        if (r <= 0) pset((int)parts[i].x, (int)parts[i].y, parts[i].color);
        else        circfill((int)parts[i].x, (int)parts[i].y, r, parts[i].color);
    }

    // crosshair at the cursor
    if (gstate != G_OVER) {
        int mx = mouse_x(), my = mouse_y();
        int col = CLR_LIGHT_GREY;
        line(mx - 5, my, mx - 2, my, col);
        line(mx + 2, my, mx + 5, my, col);
        line(mx, my - 5, mx, my - 2, col);
        line(mx, my + 2, mx, my + 5, col);
        pset(mx, my, CLR_WHITE);
    }

    // detonation flash
    if (flash > 0) {
        fillp(FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
        fillp_reset();
    }

    // ---- HUD ----
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("WAVE %d", wave), SCREEN_W - 4, 3, CLR_YELLOW);
    print_centered(str("CITIES %d", live_cities()), SCREEN_W/2, 3, CLR_LIGHT_PEACH);

    // last-city warning
    if (gstate == G_PLAY && live_cities() == 1 && blink(16))
        print_centered("LAST CITY!", SCREEN_W/2, 13, CLR_RED);

    // ---- banners ----
    if (gstate == G_WAVE) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 36, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 36, CLR_GREEN);
        print_centered("WAVE CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 10, CLR_GREEN);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 + 2, CLR_YELLOW);
    }
    if (gstate == G_OVER) {
        rectfill(SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 60, CLR_BLACK);
        rect    (SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 60, CLR_RED);
        print_centered("THE END", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_WHITE);
        print_centered(str("BEST %d", best), SCREEN_W/2, SCREEN_H / 2 + 6, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("CLICK TO DEFEND AGAIN", SCREEN_W/2, SCREEN_H / 2 + 18, CLR_GREEN);
    }
}
