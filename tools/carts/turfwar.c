/* de:meta
{
  "slug": "turfwar",
  "title": "turf war",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "steering-behaviors",
    "flocking",
    "state-machine"
  ],
  "lineage": "Inspired by Paper.io / Splatoon territory presence mechanics; each gang's crew uses simple seek/arrive steering toward a rally district, with brawl-state transitions — a compact multi-faction AI without pathfinding.",
  "genre": "sandbox",
  "description": "Top-down gang-turf takeover: lead your green crew block by block, out-number the rivals holding a district and the whole zone flips to your color with a wash and a chime. Three AI gangs expand into neutral blocks and counter-attack your borders — capture is paper.io/splatoon \"presence\" applied to a city grid, the crew is the crowd.c pal()-recolor trick (one sprite, four gang colors via a magic shirt index), and a 92 BPM boom-bap bed (filtered bass + euclid hats) rolls under brawl dust, screen shake, a pulsing share-bar and a district mini-map. Own 8 of 12 districts to win; lose your last block and you're wiped out. WASD lead your crew (they follow and break off to brawl), Z for a gang-up melee burst, Z to run it back."
}
de:meta */
#include "studio.h"
#include "endcard.h"

// TURF WAR — top-down gang-territory takeover.
//
// You lead a small crew (green). Walk into a rival zone, out-number/beat the
// rivals holding it, and the whole district flips to your color. Three AI gangs
// are doing the same thing — expanding into neutral blocks and counter-attacking
// your borders. Control 8 of the 12 districts to win; lose your last block and
// it's over.
//
// The capture mechanic is paper.io/splatoon "presence" applied to a city grid:
// each district counts how many of each gang stand inside it; the gang with the
// majority pushes a contest meter until the zone flips. The crew is the crowd.c
// pal()-recolor idiom — one tiny sprite, swapped per gang via a magic color.
//
//   WASD     lead your crew (they follow you, break off to brawl)
//   Z        gang up — a melee burst that decks nearby rivals
//   (game over) Z to play again

// ── city / region grid ───────────────────────────────────────────
#define REG_COLS 4
#define REG_ROWS 3
#define NREG     (REG_COLS * REG_ROWS)   // 12 districts
#define RW       7                       // region width  in cells
#define RH       5                       // region height in cells
#define MW       (REG_COLS * RW)         // 28 map cells across
#define MH       (REG_ROWS * RH)         // 15 map cells down
#define WC       CELL_W                  // 16 px per cell
#define WW       (MW * WC)               // 448 px world width
#define WH       (MH * WC)               // 240 px world height
#define TARGET_REG 8                     // own this many districts to win

// ── gangs ────────────────────────────────────────────────────────
#define NGANGS   4
#define MAGIC    29                      // sprite "shirt" placeholder pal()-swapped per gang
static const int G_TERR[NGANGS] = { CLR_DARK_GREEN, CLR_DARK_RED,  CLR_DARK_BLUE, CLR_DARK_ORANGE };
static const int G_MEM [NGANGS] = { CLR_GREEN,      CLR_RED,       CLR_BLUE,      CLR_YELLOW };
static const int START [NGANGS] = { 8, 3, 0, 11 };   // four corner districts

// ── members ──────────────────────────────────────────────────────
#define MAX_MEM   64
#define MEM_BASE   4      // crew size at one district; +1 per extra district
#define MEM_CAP   13
#define SPD        0.78f  // member walk speed (px/frame @60)
#define LEAD_SPD   1.55f
#define AGGRO     34      // start chasing an enemy within this
#define ATK_RANGE 12

typedef struct {
    bool  active, leader;
    int   gang, hp, state;   // state: 0 alive, 1 down
    float x, y, face;
    float atkcd, flash, downt, bob;
} Member;

static Member mem[MAX_MEM];
static int    leaderIdx;

// ── regions ──────────────────────────────────────────────────────
typedef struct {
    int   owner;     // gang id, or -1 neutral
    int   capBy;     // gang currently contesting, or -1
    float capp;      // capture progress 0..1
    float wash;      // flip-sweep animation 1..0
} Region;
static Region regs[NREG];
static int    gang_target[NGANGS];   // AI rally district per gang

// ── particles ────────────────────────────────────────────────────
#define MAX_PARTS 56
typedef struct { float x, y, vx, vy, life, max; int color; } Part;
static Part parts[MAX_PARTS];

// ── game state ───────────────────────────────────────────────────
static int   cnt[NREG][NGANGS];   // members per gang per district (recomputed each frame)
static int   state;               // 0 play, 1 win, 2 lose
static float end_t;               // seconds since the end card came up
static int   best;
static float banner_t;            // "TURF TAKEN" pop timer
static float alarm_t;             // last under-attack beep
static float atk_fx;              // leader swing flash
static int   last16 = -1;         // sixteenth-note clock for the music bed

// ── geometry helpers ─────────────────────────────────────────────
static int   reg_at(float x, float y) {
    int rx = mid(0, (int)(x / (RW * WC)), REG_COLS - 1);
    int ry = mid(0, (int)(y / (RH * WC)), REG_ROWS - 1);
    return ry * REG_COLS + rx;
}
static float reg_cx(int r) { return ((r % REG_COLS) + 0.5f) * RW * WC; }
static float reg_cy(int r) { return ((r / REG_COLS) + 0.5f) * RH * WC; }
static float reg_sx(int r) { return ((r % REG_COLS) * RW + 3) * WC + 8; }  // an open spawn cell (top-center, no wall)
static float reg_sy(int r) { return ((r / REG_COLS) * RH + 0) * WC + 8; }

static int owned_count(int g) {
    int n = 0;
    for (int r = 0; r < NREG; r++) if (regs[r].owner == g) n++;
    return n;
}
static int pick_owned(int g) {
    int list[NREG], n = 0;
    for (int r = 0; r < NREG; r++) if (regs[r].owner == g) list[n++] = r;
    return n ? list[rnd(n)] : -1;
}

static bool wall_box(float x, float y, int r) {
    return touching_map((int)x - r, (int)y - r, r * 2, r * 2);
}
static void move_slide(Member *m, float mvx, float mvy) {
    int r = 4;
    if (!wall_box(m->x + mvx, m->y, r)) m->x += mvx;
    if (!wall_box(m->x, m->y + mvy, r)) m->y += mvy;
    m->x = clamp(m->x, r, WW - r);
    m->y = clamp(m->y, r, WH - r);
}

static void add_part(float x, float y, int col) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (parts[i].life <= 0) {
            float a = rnd(360), s = rnd_float_between(0.5f, 1.8f);
            parts[i].x = x; parts[i].y = y;
            parts[i].vx = dx(s, a); parts[i].vy = dy(s, a) - 0.5f;
            parts[i].life = parts[i].max = rnd_float_between(0.22f, 0.5f);
            parts[i].color = col;
            return;
        }
}

// ── spawning ─────────────────────────────────────────────────────
static int find_free(void) {
    for (int i = 1; i < MAX_MEM; i++) if (!mem[i].active) return i;  // slot 0 reserved for the leader
    return -1;
}
static void place(Member *m, int g, int r, bool leader) {
    m->active = true; m->leader = leader; m->gang = g;
    m->hp = leader ? 5 : 3; m->state = 0;
    m->x = reg_sx(r) + rnd(12) - 6; m->y = reg_sy(r) + rnd(12) - 6;
    m->face = rnd(360); m->atkcd = 0; m->flash = 0; m->bob = rnd_float();
}
static int alive_count(int g) {
    int n = 0;
    for (int i = 0; i < MAX_MEM; i++)
        if (mem[i].active && mem[i].gang == g) n++;
    return n;
}

static void apply_hit(int j, int dmg, float fx, float fy) {
    Member *t = &mem[j];
    t->hp -= dmg; t->flash = 0.14f;
    float a = angle_to((int)fx, (int)fy, (int)t->x, (int)t->y);
    t->x = clamp(t->x + dx(3.0f, a), 4, WW - 4);
    t->y = clamp(t->y + dy(3.0f, a), 4, WH - 4);
    add_part(t->x, t->y, G_MEM[t->gang]);
    add_part(t->x, t->y, CLR_WHITE);
    if (t->hp <= 0 && t->state == 0) {
        t->state = 1; t->downt = now();
        for (int k = 0; k < 4; k++) add_part(t->x, t->y, CLR_LIGHT_GREY);
        hit(44, INSTR_NOISE, 3, 90);
    }
}

// ── flip a district to a new gang ────────────────────────────────
static void flip(int r, int g) {
    int prev = regs[r].owner;
    regs[r].owner = g; regs[r].capp = 0; regs[r].capBy = -1; regs[r].wash = 1.0f;
    strum(60 + g * 2, CHORD_MAJ7, 6, 3, 35);        // takeover chime
    if (g == 0) { banner_t = now(); shake(2.0f); }   // you took it
    else if (prev == 0) {                            // a rival took yours
        shake(3.0f); note(34, INSTR_SQUARE, 4);
    }
}

// ── reset / init ─────────────────────────────────────────────────
static void reset_game(void) {
    for (int i = 0; i < MAX_MEM; i++) mem[i].active = false;
    for (int i = 0; i < MAX_PARTS; i++) parts[i].life = 0;
    for (int r = 0; r < NREG; r++) { regs[r].owner = -1; regs[r].capBy = -1; regs[r].capp = 0; regs[r].wash = 0; }
    for (int g = 0; g < NGANGS; g++) { regs[START[g]].owner = g; gang_target[g] = START[g]; }

    // leader = slot 0
    place(&mem[0], 0, START[0], true);
    leaderIdx = 0;
    // a couple of starters per gang
    for (int g = 0; g < NGANGS; g++)
        for (int k = 0; k < (g == 0 ? 2 : 3); k++) {
            int s = find_free();
            if (s >= 0) place(&mem[s], g, START[g], false);
        }
    state = 0; end_t = 0; banner_t = -9; alarm_t = 0; atk_fx = 0;
}

void init(void) {
    colorkey(0);
    best = load(0);

    bpm(92);
    // filtered bass for the hip-hop bed
    instrument(5, INSTR_SQUARE, 6, 90, 5, 90);
    instrument_duty(5, 0.34f);
    instrument_filter(5, FILTER_LOW, 520, 7);
    instrument_lfo(5, 0, LFO_CUTOFF, 0.4f, 180.0f);
    lfo_shape(5, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    // sine pad for takeover chimes
    instrument(6, INSTR_SINE, 4, 180, 3, 380);

    reset_game();
}

// ── AI: pick each rival gang's rally district ────────────────────
static bool adj_owned(int r, int g) {
    int rx = r % REG_COLS, ry = r / REG_COLS;
    if (rx > 0           && regs[r - 1].owner == g) return true;
    if (rx < REG_COLS-1  && regs[r + 1].owner == g) return true;
    if (ry > 0           && regs[r - REG_COLS].owner == g) return true;
    if (ry < REG_ROWS-1  && regs[r + REG_COLS].owner == g) return true;
    return false;
}
static void ai_targets(void) {
    for (int g = 1; g < NGANGS; g++) {
        // defend a held district that's under attack
        int def = -1; float worst = 0;
        for (int r = 0; r < NREG; r++)
            if (regs[r].owner == g && regs[r].capp > worst && regs[r].capBy != g) { worst = regs[r].capp; def = r; }
        if (def >= 0) { gang_target[g] = def; continue; }

        // else expand along the frontier — neutral first, then the lightest-defended enemy
        int best_r = -1, best_s = -1000000;
        for (int r = 0; r < NREG; r++) {
            if (regs[r].owner == g || !adj_owned(r, g)) continue;
            int enemies = 0;
            for (int o = 0; o < NGANGS; o++) if (o != g) enemies += cnt[r][o];
            int s = (regs[r].owner < 0 ? 100 : 0) - enemies * 6 + rnd(12);
            if (s > best_s) { best_s = s; best_r = r; }
        }
        gang_target[g] = (best_r >= 0) ? best_r : pick_owned(g);
    }
}

// ── update ───────────────────────────────────────────────────────
void update(void) {
    float k = dt() * 60.0f;

    // music bed (only while playing)
    if (state == 0) {
        int s16 = beat() * 4 + (int)(beat_pos() * 4);
        if (s16 != last16) {
            last16 = s16;
            int step = ((s16 % 16) + 16) % 16;
            static const int BASS[16] = { 36,-1,-1,36, 43,-1,36,-1, 39,-1,39,-1, 41,-1,43,-1 };
            if (step == 0 || step == 8)            hit(36, INSTR_TRI, 5, 95);   // kick
            if (step == 4 || step == 12)           hit(52, INSTR_NOISE, 4, 110);// snare
            if (euclid(11, 16, step))              hit(86, INSTR_NOISE, 2, 26); // hats
            if (step == 14)                        hit(86, INSTR_NOISE, 2, 150);// open hat
            if (BASS[step] >= 0)                   note(BASS[step], 5, 3);      // filtered bass
        }
    }

    if (state != 0) {
        end_t += dt();
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        follow((int)mem[leaderIdx].x, (int)mem[leaderIdx].y, WW, WH);
        return;
    }

    // recompute district headcounts
    for (int r = 0; r < NREG; r++) for (int g = 0; g < NGANGS; g++) cnt[r][g] = 0;
    for (int i = 0; i < MAX_MEM; i++)
        if (mem[i].active && mem[i].state == 0) cnt[reg_at(mem[i].x, mem[i].y)][mem[i].gang]++;

    if (frame() % 80 == 0) ai_targets();

    // ── leader input ─────────────────────────────────────────────
    Member *L = &mem[leaderIdx];
    if (L->state == 0) {
        float mvx = 0, mvy = 0, sp = LEAD_SPD * k;
        if (btn(0, BTN_UP))    mvy -= sp;
        if (btn(0, BTN_DOWN))  mvy += sp;
        if (btn(0, BTN_LEFT))  mvx -= sp;
        if (btn(0, BTN_RIGHT)) mvx += sp;
        if (mvx || mvy) { move_slide(L, mvx, mvy); L->face = angle_to(0, 0, (int)(mvx * 100), (int)(mvy * 100)); }

        if (btnp(0, BTN_A) && L->atkcd <= 0) {       // gang-up burst
            L->atkcd = 0.45f; atk_fx = 0.22f; shake(2.5f);
            hit(58, INSTR_SQUARE, 4, 60); hit(72, INSTR_NOISE, 3, 40);
            for (int j = 0; j < MAX_MEM; j++)
                if (mem[j].active && mem[j].state == 0 && mem[j].gang != 0
                    && near((int)L->x, (int)L->y, (int)mem[j].x, (int)mem[j].y, 22))
                    apply_hit(j, 2, L->x, L->y);
        }
    }

    // ── members ──────────────────────────────────────────────────
    for (int i = 0; i < MAX_MEM; i++) {
        Member *m = &mem[i];
        if (!m->active) continue;
        if (m->atkcd > 0) m->atkcd -= dt();
        if (m->flash > 0) m->flash -= dt();

        if (m->state == 1) {                 // down
            if (m->leader) {
                if (now() - m->downt > 1.6f) {
                    int r = pick_owned(0);
                    if (r >= 0) { m->state = 0; m->hp = 5; m->flash = 0; m->x = reg_sx(r); m->y = reg_sy(r); }
                }
            } else if (now() - m->downt > 2.2f) {
                m->active = false;
            }
            continue;
        }

        // nearest enemy
        int ne = -1; float nd = 1e9f;
        for (int j = 0; j < MAX_MEM; j++) {
            if (!mem[j].active || mem[j].state != 0 || mem[j].gang == m->gang) continue;
            float d = distance((int)m->x, (int)m->y, (int)mem[j].x, (int)mem[j].y);
            if (d < nd) { nd = d; ne = j; }
        }

        if (m->leader) {                     // leader auto-jabs but the player drives the walking
            if (ne >= 0 && nd <= ATK_RANGE && m->atkcd <= 0) {
                apply_hit(ne, 1, m->x, m->y); m->atkcd = 0.5f;
            }
            continue;
        }

        m->bob += dt() * 6.0f;
        // target point: crew follows the leader, rivals head to their rally district
        float tx, ty;
        if (m->gang == 0) { tx = L->x; ty = L->y; }
        else              { tx = reg_cx(gang_target[m->gang]); ty = reg_cy(gang_target[m->gang]); }

        if (ne >= 0 && nd < AGGRO) {
            m->face = angle_to((int)m->x, (int)m->y, (int)mem[ne].x, (int)mem[ne].y);
            if (nd <= ATK_RANGE) {
                if (m->atkcd <= 0) {
                    apply_hit(ne, 1, m->x, m->y); m->atkcd = 0.55f;
                    if (chance(28)) hit(70, INSTR_NOISE, 2, 28);
                }
            } else {
                move_slide(m, dx(SPD * k, m->face), dy(SPD * k, m->face));
            }
        } else if (distance((int)m->x, (int)m->y, (int)tx, (int)ty) > 13) {
            m->face = angle_to((int)m->x, (int)m->y, (int)tx, (int)ty);
            move_slide(m, dx(SPD * k, m->face), dy(SPD * k, m->face));
        }
    }

    // gentle separation so crews don't stack into one dot
    for (int i = 0; i < MAX_MEM; i++) {
        if (!mem[i].active || mem[i].state != 0) continue;
        for (int j = i + 1; j < MAX_MEM; j++) {
            if (!mem[j].active || mem[j].state != 0) continue;
            float ddx = mem[j].x - mem[i].x, ddy = mem[j].y - mem[i].y;
            float d = fsqrt(ddx * ddx + ddy * ddy);
            if (d >= 9.0f || d <= 0.01f) continue;
            float push = (9.0f - d) * 0.5f, nx = ddx / d, ny = ddy / d;
            mem[i].x = clamp(mem[i].x - nx * push, 4, WW - 4);
            mem[i].y = clamp(mem[i].y - ny * push, 4, WH - 4);
            mem[j].x = clamp(mem[j].x + nx * push, 4, WW - 4);
            mem[j].y = clamp(mem[j].y + ny * push, 4, WH - 4);
        }
    }

    // ── capture meters ───────────────────────────────────────────
    for (int r = 0; r < NREG; r++) {
        int dom = -1, domc = 0; bool tie = false;
        for (int g = 0; g < NGANGS; g++) {
            if (cnt[r][g] > domc) { domc = cnt[r][g]; dom = g; tie = false; }
            else if (cnt[r][g] == domc && cnt[r][g] > 0) tie = true;
        }
        if (tie) dom = -1;
        int own = regs[r].owner, ownc = (own >= 0) ? cnt[r][own] : 0;

        if (dom >= 0 && dom != own && domc > ownc) {
            if (regs[r].capBy != dom) { regs[r].capBy = dom; if (regs[r].capp < 0) regs[r].capp = 0; }
            regs[r].capp += dt() * 0.45f * (1.0f + 0.25f * (domc - ownc));
            if (regs[r].capp >= 1.0f) flip(r, dom);
        } else {
            regs[r].capp -= dt() * 0.7f;
            if (regs[r].capp <= 0) { regs[r].capp = 0; regs[r].capBy = -1; }
        }
        if (regs[r].wash > 0) regs[r].wash -= dt() * 2.2f;
    }

    // ── reinforcements / recruiting ──────────────────────────────
    static float spawn_cd[NGANGS];
    for (int g = 0; g < NGANGS; g++) {
        if (now() < spawn_cd[g]) continue;
        spawn_cd[g] = now() + 1.1f;
        int ow = owned_count(g);
        int cap = mid(0, MEM_BASE + ow - 1, MEM_CAP);
        if (ow > 0 && alive_count(g) < cap) {
            int s = find_free();
            if (s >= 0) place(&mem[s], g, pick_owned(g), false);
        }
    }

    // ── particles ────────────────────────────────────────────────
    for (int i = 0; i < MAX_PARTS; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].x += parts[i].vx * k; parts[i].y += parts[i].vy * k;
        parts[i].vy += 0.06f * k; parts[i].life -= dt();
    }

    if (atk_fx > 0) atk_fx -= dt();

    // ── win / lose + alarm ───────────────────────────────────────
    int yours = owned_count(0);
    if (yours > best) { best = yours; save(0, best); }
    if (yours >= TARGET_REG) {
        state = 1; strum(60, CHORD_MAJ7, 6, 6, 70);
        schedule(180, 67, 6, 5); schedule(360, 72, 6, 5);
    } else if (yours == 0) {
        state = 2; note(31, INSTR_NOISE, 6);
    } else {
        for (int g = 1; g < NGANGS; g++) if (owned_count(g) >= TARGET_REG) { state = 2; note(31, INSTR_NOISE, 6); }
    }

    bool under_attack = false;
    for (int r = 0; r < NREG; r++)
        if (regs[r].owner == 0 && regs[r].capp > 0 && regs[r].capBy != 0) under_attack = true;
    if (under_attack && now() - alarm_t > 0.6f) { alarm_t = now(); hit(48, INSTR_SQUARE, 3, 80); }

    follow((int)L->x, (int)L->y, WW, WH);
}

// ── drawing ──────────────────────────────────────────────────────
static void draw_member(Member *m) {
    int x = (int)m->x, y = (int)m->y;
    ovalfill(x, y + 6, 5, 2, CLR_BROWNISH_BLACK);    // shadow
    if (m->flash > 0) pal(MAGIC, CLR_WHITE); else pal(MAGIC, G_MEM[m->gang]);
    int fr = ((int)(m->bob) & 1);
    spr(fr, x - 8, y - 8);
    pal_reset();
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    map(0, 0, 0, 0, MW, MH);

    // district ownership tint (sparse dither so the streets read through)
    for (int r = 0; r < NREG; r++) {
        int rx = (r % REG_COLS) * RW * WC, ry = (r / REG_COLS) * RH * WC;
        if (regs[r].owner >= 0) {
            fillp(FILL_DOTS, -1);
            rectfill(rx, ry, RW * WC, RH * WC, G_TERR[regs[r].owner]);
            fillp_reset();
        }
        // contest wash + pulsing border
        if (regs[r].capp > 0 && regs[r].capBy >= 0) {
            int pat = regs[r].capp > 0.6f ? FILL_CHECKER : FILL_DOTS;
            fillp(pat, -1);
            rectfill(rx, ry, RW * WC, RH * WC, G_TERR[regs[r].capBy]);
            fillp_reset();
            if (blink(5)) rect(rx + 1, ry + 1, RW * WC - 2, RH * WC - 2, G_MEM[regs[r].capBy]);
        }
        // flip sweep — a bright bar washing across the captured zone
        if (regs[r].wash > 0) {
            int sweep = rx + (int)((1.0f - regs[r].wash) * RW * WC);
            rectfill(sweep - 4, ry, 8, RH * WC, CLR_WHITE);
        }
    }

    // district grid lines
    for (int c = 0; c <= REG_COLS; c++) line(c * RW * WC, 0, c * RW * WC, WH, CLR_BROWNISH_BLACK);
    for (int c = 0; c <= REG_ROWS; c++) line(0, c * RH * WC, WW, c * RH * WC, CLR_BROWNISH_BLACK);

    // downed bodies (drawn under the standing crowd)
    for (int i = 0; i < MAX_MEM; i++) {
        Member *m = &mem[i];
        if (!m->active || m->state != 1) continue;
        int x = (int)m->x, y = (int)m->y;
        ovalfill(x, y + 2, 6, 2, CLR_DARKER_GREY);
        line(x - 3, y, x + 3, y, G_TERR[m->gang]);
        line(x, y - 2, x, y + 2, G_TERR[m->gang]);
    }

    // standing members
    for (int i = 0; i < MAX_MEM; i++)
        if (mem[i].active && mem[i].state == 0 && !mem[i].leader) draw_member(&mem[i]);

    // particles
    for (int i = 0; i < MAX_PARTS; i++)
        if (parts[i].life > 0) pset((int)parts[i].x, (int)parts[i].y, parts[i].color);

    // leader, with crown + facing + selection ring
    Member *L = &mem[leaderIdx];
    if (L->state == 0) {
        int x = (int)L->x, y = (int)L->y;
        draw_member(L);
        circ(x, y, 9, CLR_WHITE);
        line(x, y, x + (int)dx(12, L->face), y + (int)dy(12, L->face), CLR_YELLOW);
        trifill(x - 3, y - 12, x + 3, y - 12, x, y - 16, CLR_YELLOW);   // crown
        if (atk_fx > 0) circ(x, y, (int)(22 * (1.0f - atk_fx / 0.22f)) + 4, CLR_WHITE);
    }

    // ── HUD ──────────────────────────────────────────────────────
    camera(0, 0);
    rectfill(0, 0, SCREEN_W, 13, CLR_BROWNISH_BLACK);

    // share bar — one segment per gang's district share, neutral in grey
    int bx = 52, bw = SCREEN_W - 52 - 52, by = 4, bh = 5, ix = bx;
    rectfill(bx, by, bw, bh, CLR_DARKER_GREY);
    for (int g = 0; g < NGANGS; g++) {
        int seg = owned_count(g) * bw / NREG;
        rectfill(ix, by, seg, bh, G_TERR[g]);
        ix += seg;
    }
    rect(bx, by, bw, bh, CLR_DARK_GREY);
    print(str("YOU %d", owned_count(0)), 4, 3, CLR_GREEN);
    print_right(str("WIN %d", TARGET_REG), SCREEN_W - 4, 3, CLR_YELLOW);

    // mini-map of the districts, bottom-left
    int mmx = 4, mmy = SCREEN_H - 4 - REG_ROWS * 7, cs = 6, gp = 1;
    rectfill(mmx - 2, mmy - 2, REG_COLS * (cs + gp) + 3, REG_ROWS * (cs + gp) + 3, CLR_BROWNISH_BLACK);
    for (int r = 0; r < NREG; r++) {
        int cx = mmx + (r % REG_COLS) * (cs + gp), cy = mmy + (r / REG_COLS) * (cs + gp);
        rectfill(cx, cy, cs, cs, regs[r].owner >= 0 ? G_TERR[regs[r].owner] : CLR_DARK_GREY);
        if (regs[r].capp > 0 && regs[r].capBy >= 0 && blink(6)) rect(cx, cy, cs, cs, G_MEM[regs[r].capBy]);
    }
    {   // leader blip on the mini-map
        int lr = reg_at(L->x, L->y);
        int cx = mmx + (lr % REG_COLS) * (cs + gp) + cs / 2, cy = mmy + (lr / REG_COLS) * (cs + gp) + cs / 2;
        pset(cx, cy, CLR_WHITE); pset(cx, cy - 1, CLR_WHITE);
    }

    print_right("WASD lead   Z brawl", SCREEN_W - 4, SCREEN_H - 9, CLR_DARK_GREY);

    // under-attack alert
    bool under_attack = false;
    for (int r = 0; r < NREG; r++)
        if (regs[r].owner == 0 && regs[r].capp > 0 && regs[r].capBy != 0) under_attack = true;
    if (under_attack && blink(8)) {
        rect(0, 0, SCREEN_W, SCREEN_H, CLR_RED);
        print_centered("DEFEND YOUR TURF!", SCREEN_W/2, 16, CLR_RED);
    }

    // "TURF TAKEN" pop
    if (now() - banner_t < 1.1f)
        print_scaled("TURF TAKEN!", SCREEN_W / 2 - text_width("TURF TAKEN!"), SCREEN_H / 2 - 16, CLR_GREEN, 2);

    // win / lose — the shared end-screen treatment (endcard.h)
    if (state != 0) {
        EndCard c = endcard(end_t, 156, 52, SCREEN_H / 2 - 26, CLR_BLACK, CLR_WHITE,
                            state == 1 ? CLR_GREEN : CLR_RED);
        if (c.settled) {
            if (state == 1) print_centered("CITY IS YOURS", SCREEN_W/2, c.y + 12, CLR_GREEN);
            else            print_centered("WIPED OUT", SCREEN_W/2, c.y + 12, CLR_RED);
            print_centered(str("you held %d of %d  -  best %d", owned_count(0), NREG, best), SCREEN_W/2, c.y + 26, CLR_YELLOW);
            if (blink(18)) print_centered("Z to run it back", SCREEN_W/2, c.y + 40, CLR_LIGHT_GREY);
        }
    }
}
