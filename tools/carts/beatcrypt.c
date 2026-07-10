/* de:meta
{
  "slug": "beatcrypt",
  "title": "Beatcrypt",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "dungeon-generation",
    "euclidean-rhythm",
    "turn-based-loop"
  ],
  "lineage": "Miniature Crypt of the NecroDancer (2015) — the core innovation is enemies each running their own beat-locked rhythm (every/euclid()) so the dungeon is a rhythmic ensemble, not just a metronome.",
  "genre": "rpg",
  "homage": "Crypt of the NecroDancer (2015)",
  "description": "A beat-locked roguelike: a looping 2-bar synth bed (kick/snare/euclidean hats + a minor bassline) drives turn-based dungeon crawling — move/attack ONLY on the beat to build a combo, miss the window and you stumble; enemies step on their own rhythms (skeleton every 2nd beat, bat on a euclid(3,8) syncopation, slime every 4th). Bump enemies on-beat to kill, clear the floor, reach the > stairs. Controls: WASD / Arrows to move + attack (press ON the beat), R to restart.",
  "todo": [
    "Add an onscreen, clickable restart label.",
    "Touch: a d-pad (not a joystick)."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ============================================================
//  BEATCRYPT — a beat-locked roguelike. Crypt of the NecroDancer in
//  miniature: the dungeon is turn-based, but turns advance on the MUSICAL
//  BEAT, not on a keypress. A 2-bar synth bed (bass + drums) loops forever;
//  a pulsing indicator marks each beat. Move/attack ON the beat to act and
//  build a combo — miss the window and you stumble (no move that beat).
//
//  Enemies step on the beat too, each with its OWN rhythm (via euclid()):
//    SKELETON  every 2nd beat        (steady plodder)
//    BAT       euclid(3,8) beats     (jittery, syncopated)
//    SLIME     every 4th beat        (slow, heavy)
//  Bump an enemy ON the beat to kill it. Reach the stairs ">" to win.
//
//  Move:  WASD / Arrows  (press on the beat!)        R: restart
// ============================================================

#define GW    36
#define GH    18
#define TILE  8
#define ORG_X 16          // grid origin on screen
#define ORG_Y 14
#define MAXR  12
#define MAXM  24

enum { WALL, FLOOR, STAIRS };
enum { PLAY, DEAD, WIN };

// enemy types
enum { E_SKEL, E_BAT, E_SLIME };
static const int   EHP [3] = { 1, 1, 2 };
static const char  EGLY[3] = { 'S', 'w', 'o' };
static const int   ECOL[3] = { CLR_LIGHT_GREY, CLR_PINK, CLR_LIME_GREEN };
static const char *ENAME[3]= { "skeleton", "bat", "slime" };

typedef struct { int x, y, hp, type; bool alive; int hitT; int phase; } Mon;

static int  dmap[GH][GW];
static Mon  mon[MAXM];

static int  px, py, hp, maxhp, depth, state;
static int  combo, bestcombo, kills, killgoal;
static char msg[64];

// ── beat clock ──
static int   curbeat   = -1;     // last beat we processed a turn on
static float pulseT    = 0;      // now() of last beat tick (for the pulse anim)
static float hitFlashT = 0;      // player hit/miss flash
static int   moveBuf   = 0;      // queued direction this beat: 0 none, else dir+1
static int   lastScore = 0;      // 1 = on-beat hit, -1 = miss (for HUD color)
static float lastScoreT = 0;

// the beat-window: you may act when beat_pos() is within WINDOW of a beat edge
#define WINDOW 0.28f

static void set_msg(const char *s) { int i = 0; while (s[i] && i < 63) { msg[i] = s[i]; i++; } msg[i] = 0; }

static void dig_h(int x1, int x2, int y) { for (int x = min(x1, x2); x <= max(x1, x2); x++) if (dmap[y][x] == WALL) dmap[y][x] = FLOOR; }
static void dig_v(int y1, int y2, int x) { for (int y = min(y1, y2); y <= max(y1, y2); y++) if (dmap[y][x] == WALL) dmap[y][x] = FLOOR; }

static int monster_at(int x, int y) {
    for (int i = 0; i < MAXM; i++) if (mon[i].alive && mon[i].x == x && mon[i].y == y) return i;
    return -1;
}

static void gen_level() {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) dmap[y][x] = WALL;
    for (int i = 0; i < MAXM; i++) mon[i].alive = false;

    int rcx[MAXR], rcy[MAXR], rx0[MAXR], ry0[MAXR], rw_[MAXR], rh_[MAXR], nr = 0;
    for (int cj = 0; cj < 2; cj++)
        for (int ci = 0; ci < 4; ci++) {
            int cellx = ci * 9, celly = cj * 9;
            int rw = rnd_between(4, 8), rh = rnd_between(3, 6);
            int rx = cellx + 1 + rnd(9 - rw - 1);
            int ry = celly + 1 + rnd(9 - rh - 1);
            for (int y = ry; y < ry + rh; y++)
                for (int x = rx; x < rx + rw; x++) dmap[y][x] = FLOOR;
            rx0[nr] = rx; ry0[nr] = ry; rw_[nr] = rw; rh_[nr] = rh;
            rcx[nr] = rx + rw / 2; rcy[nr] = ry + rh / 2; nr++;
        }
    for (int i = 1; i < nr; i++) {
        dig_h(rcx[i - 1], rcx[i], rcy[i - 1]);
        dig_v(rcy[i - 1], rcy[i], rcx[i]);
    }

    px = rcx[0]; py = rcy[0];
    dmap[rcy[nr - 1]][rcx[nr - 1]] = STAIRS;

    // monsters: one per non-start room, type by index for variety
    int placed = 0;
    for (int i = 1; i < nr && placed < MAXM; i++) {
        for (int tries = 0; tries < 8; tries++) {
            int x = rx0[i] + rnd(rw_[i]), y = ry0[i] + rnd(rh_[i]);
            if (dmap[y][x] != FLOOR || monster_at(x, y) >= 0) continue;
            if (x == px && y == py) continue;
            int t = (i % 3);                        // cycle skel / bat / slime
            mon[placed].x = x; mon[placed].y = y; mon[placed].type = t;
            mon[placed].hp = EHP[t]; mon[placed].alive = true; mon[placed].hitT = 0;
            mon[placed].phase = rnd(2);             // bats stagger their euclid start
            placed++;
            break;
        }
    }
    killgoal = placed;
}

static void new_game() {
    hp = maxhp = 6;
    depth = 1; state = PLAY;
    combo = 0; bestcombo = 0; kills = 0;
    curbeat = -1; moveBuf = 0;
    set_msg("move ON the beat");
    gen_level();
}

void init() {
    bestcombo = load(0);
    // a warm plucked bass for the synth bed
    instrument(5, INSTR_TRI, 2, 90, 3, 140);
    instrument_filter(5, FILTER_LOW, 1400, 6);
    bpm(124);
    new_game();
}

// ── the synth bed: a looping 2-bar groove off the beat clock ──
// drums via euclid()/every(), a bassline that walks the bar. called once per
// new beat (b = global beat counter).
static void play_groove(int b) {
    int bar = (b / 4) % 2;        // which of the 2 bars
    int pos = b % 4;              // beat within the bar (0..3)

    // KICK — four on the floor, the dungeon's heartbeat
    hit(36, INSTR_TRI, 6, 90);
    // SNARE — backbeat on 2 and 4
    if (pos == 1 || pos == 3) hit(55, INSTR_NOISE, 4, 110);
    // HIHAT — a euclidean (5,8) sprinkle across beats for swing
    if (euclid(5, 8, b)) hit(84, INSTR_NOISE, 2, 26);

    // BASS — a 2-bar minor-key walk (MIDI notes), tonal square-ish pluck
    static const int LINE[2][4] = {
        { 33, 33, 40, 36 },   // bar A:  A1 A1 E2 C2
        { 31, 36, 38, 28 },   // bar B:  G1 C2 D2 E1(low)
    };
    note(LINE[bar][pos], 5, 4);
}

// resolve all enemy steps for beat b: move toward player on their own rhythm,
// attack if adjacent. returns nothing; sets state on death.
static void monsters_step(int b) {
    for (int i = 0; i < MAXM; i++) {
        Mon *m = &mon[i];
        if (!m->alive) continue;

        bool acts = false;
        switch (m->type) {
            case E_SKEL:  acts = (b % 2) == 0; break;            // every 2nd beat
            case E_BAT:   acts = euclid(3, 8, b + m->phase); break; // (3,8) syncopation
            case E_SLIME: acts = (b % 4) == 0; break;            // every 4th beat
        }
        if (!acts) continue;

        int dxp = px - m->x, dyp = py - m->y;
        int adist = abs(dxp) + abs(dyp);
        if (adist == 1) {                                        // adjacent → bite
            hp -= 1;
            m->hitT = frame();
            hitFlashT = now();
            shake(2);
            note(43, INSTR_SAW, 3);
            combo = 0;
            set_msg(str("the %s bites you!", ENAME[m->type]));
            if (hp <= 0) {
                hp = 0; state = DEAD;
                if (bestcombo > load(0)) save(0, bestcombo);
            }
            continue;
        }
        // step one tile toward the player (axis with the larger gap first)
        int sx = sgn(dxp), sy = sgn(dyp);
        int nx = m->x, ny = m->y;
        if (abs(dxp) >= abs(dyp)) nx += sx; else ny += sy;
        if (nx >= 0 && nx < GW && ny >= 0 && ny < GH &&
            dmap[ny][nx] != WALL && monster_at(nx, ny) < 0 && !(nx == px && ny == py)) {
            m->x = nx; m->y = ny;
        } else {                                                 // blocked → other axis
            nx = m->x; ny = m->y;
            if (abs(dxp) >= abs(dyp)) ny += sy; else nx += sx;
            if (nx >= 0 && nx < GW && ny >= 0 && ny < GH &&
                dmap[ny][nx] != WALL && monster_at(nx, ny) < 0 && !(nx == px && ny == py)) {
                m->x = nx; m->y = ny;
            }
        }
    }
}

// the player's queued move lands on this beat (dir 1..4 = L R U D)
static void player_act(int dir, int b) {
    int dx = 0, dy = 0;
    if (dir == 1) dx = -1;
    else if (dir == 2) dx = 1;
    else if (dir == 3) dy = -1;
    else if (dir == 4) dy = 1;

    int nx = px + dx, ny = py + dy;
    if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) return;

    int mi = monster_at(nx, ny);
    if (mi >= 0) {                                               // bump = attack
        mon[mi].hp -= 1;
        mon[mi].hitT = frame();
        note(72, INSTR_SQUARE, 3);
        shake(1);
        if (mon[mi].hp <= 0) {
            mon[mi].alive = false;
            kills++;
            combo++;
            if (combo > bestcombo) bestcombo = combo;
            // a rising blip whose pitch climbs with the combo — the reward sound
            hit(72 + min(combo, 12), INSTR_SQUARE, 5, 90);
            set_msg(str("x%d  slew the %s!", 1 + combo / 2, ENAME[mon[mi].type]));
            if (kills >= killgoal) set_msg("floor cleared! reach the > stairs");
        } else {
            set_msg(str("hit the %s", ENAME[mon[mi].type]));
        }
    } else if (dmap[ny][nx] != WALL) {
        px = nx; py = ny;
        combo++;
        if (combo > bestcombo) bestcombo = combo;
        if (dmap[py][px] == STAIRS && kills >= killgoal) {
            state = WIN;
            if (bestcombo > load(0)) save(0, bestcombo);
            set_msg("you escape the crypt!");
        }
    }
}

void update() {
    bpm(124);

    if (state != PLAY) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || key('R')) new_game();
        return;
    }

    int b = beat();
    float bp = beat_pos();

    // a new beat edge → advance the world one turn
    if (b != curbeat) {
        curbeat = b;
        pulseT  = now();
        play_groove(b);

        // resolve the player's queued action FIRST (so a kill stops the bite)
        if (moveBuf) { player_act(moveBuf, b); }
        moveBuf = 0;

        if (state == PLAY) monsters_step(b);
    }

    // capture a direction press; only valid inside the on-beat window
    int dir = 0;
    if (btnp(0, BTN_LEFT))  dir = 1;
    else if (btnp(0, BTN_RIGHT)) dir = 2;
    else if (btnp(0, BTN_UP))    dir = 3;
    else if (btnp(0, BTN_DOWN))  dir = 4;

    if (dir) {
        bool on_beat = (bp < WINDOW) || (bp > 1.0f - WINDOW);
        if (on_beat && moveBuf == 0) {
            moveBuf = dir;                  // queued; lands on the (near) beat edge
            lastScore = 1; lastScoreT = now();
            // if we're just PAST the edge, this beat already advanced — act now
            if (bp < WINDOW) { player_act(dir, b); moveBuf = 0; }
        } else if (!on_beat) {
            // off-beat stumble: lose the combo, no move
            combo = 0;
            lastScore = -1; lastScoreT = now();
            hitFlashT = now();
            note(40, INSTR_NOISE, 2);
            set_msg("off-beat! combo lost");
        }
    }

#ifdef DE_TRACE
    watch("beat", "%d", b);
    watch("bpos", "%.2f", bp);
    watch("combo", "%d", combo);
    watch("hp", "%d", hp);
#endif
}

// ====================================================================
// drawing
// ====================================================================

static void glyph(char c, int gx, int gy, int col) {
    print(str("%c", c), ORG_X + gx * TILE, ORG_Y + gy * TILE, col);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    float since = now() - pulseT;
    float beatT = 60.0f / 124.0f;
    float pulse = clamp(1.0f - since / beatT, 0, 1);     // 1 at the tick, fades to 0

    // ── dungeon floor + walls ──
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = ORG_X + x * TILE, sy = ORG_Y + y * TILE;
            if (dmap[y][x] == WALL) {
                // walls breathe faintly with the beat
                rectfill(sx, sy, TILE, TILE, pulse > 0.5f ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
            } else if (dmap[y][x] == STAIRS) {
                bool open = kills >= killgoal;
                glyph('>', x, y, open ? (blink(8) ? CLR_WHITE : CLR_YELLOW) : CLR_DARK_GREY);
            } else {
                pset(sx + TILE / 2, sy + TILE / 2, CLR_DARKER_GREY);
            }
        }

    // ── enemies ──
    for (int i = 0; i < MAXM; i++) {
        if (!mon[i].alive) continue;
        bool flash = (frame() - mon[i].hitT) < 4;
        glyph(EGLY[mon[i].type], mon[i].x, mon[i].y, flash ? CLR_WHITE : ECOL[mon[i].type]);
    }

    // ── player — flashes red when hit ──
    bool phurt = (now() - hitFlashT) < 0.18f && lastScore != 1;
    glyph('@', px, py, phurt ? CLR_RED : CLR_LIGHT_PEACH);

    // ── HUD top bar: beat indicator ──
    int barY = ORG_Y + GH * TILE + 4;
    rectfill(0, barY, SCREEN_W, SCREEN_H - barY, CLR_DARKER_BLUE);

    // beat pulse: a ring that pops on the tick + a 4-step bar marker
    int pb = beat() % 4;
    int cx = 22, cy = barY + 12;
    circ(cx, cy, 9, CLR_DARK_GREY);
    circfill(cx, cy, (int)(3 + pulse * 5), pulse > 0.4f ? CLR_WHITE : CLR_INDIGO);
    for (int i = 0; i < 4; i++) {
        int dotx = 40 + i * 8;
        bool on = (i == pb);
        circfill(dotx, cy, on ? 3 : 1, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    print("BEAT", 8, barY + 1, CLR_INDIGO);

    // combo
    int mult = 1 + combo / 2;
    int ccol = (now() - lastScoreT) < 0.2f ? (lastScore == 1 ? CLR_GREEN : CLR_RED)
                                           : (combo > 0 ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print(str("COMBO x%d", mult), 84, barY + 1, ccol);
    print(str("(%d)", combo), 84, barY + 10, CLR_DARK_GREY);

    // HP hearts
    print("HP", 156, barY + 1, CLR_RED);
    for (int i = 0; i < maxhp; i++)
        print("\x03", 174 + i * 8, barY + 1, i < hp ? CLR_RED : CLR_DARKER_GREY);

    // floor goal
    print_right(str("KILLS %d/%d", kills, killgoal), SCREEN_W - 4, barY + 1, CLR_LIME_GREEN);
    print_right(str("BEST x%d", bestcombo), SCREEN_W - 4, barY + 10, CLR_LIGHT_GREY);

    // message line
    print(msg, 8, barY + 22, CLR_LIGHT_PEACH);
    print("WASD/arrows ON the beat   R restart", 8, SCREEN_H - 9, CLR_DARKER_GREY);

    // ── end states ──
    if (state == DEAD) {
        rectfill(SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 56, CLR_BLACK);
        rect    (SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 56, CLR_RED);
        print_centered("THE BEAT GOES SILENT", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_RED);
        print_centered(str("kills %d   best combo x%d", kills, bestcombo), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_YELLOW);
        print_centered("Z / R to dance again", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
    if (state == WIN) {
        rectfill(SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 56, CLR_BLACK);
        rect    (SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 56, CLR_YELLOW);
        print_centered("CRYPT CLEARED!", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_YELLOW);
        print_centered(str("best combo x%d", bestcombo), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_LIGHT_PEACH);
        print_centered("Z / R to play again", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
}
