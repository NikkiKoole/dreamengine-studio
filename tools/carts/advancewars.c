/* de:meta
{
  "title": "advance wars",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "grid-movement",
    "tilemap-collision",
    "finite-state-ai"
  ],
  "lineage": "Demake of Advance Wars (GBA, 2001); novel in distilling the damage-matrix / terrain-defense-stars / capture mechanic into a single-screen hand-authored map with a CPU opponent that takes a full turn.",
  "genre": "strategy",
  "homage": "Advance Wars (2001)",
  "description": "A bright turn-based tactics game (an Advance Wars demake), you RED vs the CPU BLUE on one hand-made tile map. The crunch is three systems working together: a unit-vs-unit DAMAGE MATRIX (mechs wreck tanks up close, recon mauls infantry, artillery hits at range 2-3 but can't be countered or move-and-fire), TERRAIN DEFENSE STARS that scale how much a defender soaks (forests and mountains save lives), and CAPTURE — march infantry onto a building to flip it over two turns for income, or take the enemy HQ to win instantly. Factories build new units from your funds, a forecast shows damage before you commit, and every clash kicks a screen-shake, flash, sparks and HP pops. Mouse-primary: click a unit, click a reachable tile (it traces a move arrow over the highlighted range), then pick ATTACK / CAPTURE / WAIT. Keyboard: WASD or arrows move the cursor, Z confirm, X cancel/undo, SPACE ends turn. Win by capturing the enemy HQ or destroying every enemy unit."
}
de:meta */
#include "studio.h"
#include "cursor.h"

// ── FIELD COMMANDER — turn-based tactics (an Advance Wars demake) ─────────────
// You (RED) vs the CPU (BLUE) on one hand-made tile map. Move units, capture
// buildings for income, and crush the enemy. Win by capturing the enemy HQ or
// destroying every enemy unit.
//
// The crunch lives in three places: a unit-vs-unit DAMAGE MATRIX (mech beats
// tank up close, recon mauls infantry, artillery hits at range but can't be
// countered), TERRAIN DEFENSE STARS that scale how much damage a defender soaks,
// and CAPTURE (infantry/mech stand on a building to flip it over ~2 turns).
//
// Mouse-primary: click your unit → click a reachable tile → pick an action.
// Keyboard: WASD/arrows move the cursor, Z confirm, X cancel, SPACE ends turn.

// ── layout ───────────────────────────────────────────────────────────────────
#define COLS    20
#define ROWS    10
#define TILE    16
#define HUD_Y   24                 // top HUD bar height
#define MAP_Y   HUD_Y              // map starts right below it
#define INFO_Y  (MAP_Y + ROWS*TILE)// bottom info strip (184..200)

// ── terrain ──────────────────────────────────────────────────────────────────
enum { PLAINS, FOREST, MTN, ROAD, SEA, SHOAL, T_CITY, T_FACT, T_HQ, NTERR };
static const int  GROUND_SLOT[NTERR] = { 1,2,3,4,5,6, 1,1,1 }; // map() tile (buildings sit on grass)
static const int  STARS[NTERR]       = { 1,2,4,0,0,0, 3,3,4 }; // terrain defense stars
static const char *TNAME[NTERR]      = { "PLAINS","FOREST","MTN","ROAD","SEA","SHOAL","CITY","FACTORY","HQ" };

// move cost per movement class × terrain (99 = impassable)
enum { FOOT, TIRE, TREAD };
static const int MCOST[3][NTERR] = {
    { 1,1,2,1,99,1, 1,1,1 },   // FOOT  (infantry, mech)
    { 2,3,99,1,99,1, 1,1,1 },  // TIRE  (recon)
    { 1,2,99,1,99,1, 1,1,1 },  // TREAD (tank, artillery)
};

// ── units ────────────────────────────────────────────────────────────────────
enum { INF, MECH, RECON, TANK, ARTY, NTYPE };
static const int  MOVE_PTS[NTYPE]  = { 3, 2, 8, 6, 5 };
static const int  MCLASS[NTYPE]    = { FOOT, FOOT, TIRE, TREAD, TREAD };
static const int  COST[NTYPE]      = { 1000, 3000, 4000, 7000, 6000 };
static const int  USPRITE[NTYPE]   = { 16, 17, 18, 19, 20 };
static const char *UNAME[NTYPE]    = { "INFANTRY","MECH","RECON","TANK","ARTILLERY" };
static const bool INDIRECT[NTYPE]  = { false,false,false,false,true };  // artillery = ranged, no counter

// base damage % : DMG[attacker][defender], at full HP on 0-star terrain
static const int DMG[NTYPE][NTYPE] = {
    /* INF   */ { 55, 45, 12,  5, 15 },
    /* MECH  */ { 65, 55, 85, 55, 70 },
    /* RECON */ { 70, 65, 35,  6, 45 },
    /* TANK  */ { 75, 70, 85, 55, 70 },
    /* ARTY  */ { 90, 85, 80, 70, 75 },
};

#define MAXU 24
typedef struct { int type, team, x, y, hp, moved, acted; } Unit; // type<0 = empty slot
static Unit army[MAXU];

// ── board state ──────────────────────────────────────────────────────────────
static int terr[ROWS][COLS];
static int bo[ROWS][COLS];        // building owner: -2 none, -1 neutral, 0 red, 1 blue
static int capLeft[ROWS][COLS];   // capture points remaining (20 = full)
static int capTeam[ROWS][COLS];   // which team is mid-capture here
static int funds[2];
static int turn_no, winner = -1;

// ── interaction ──────────────────────────────────────────────────────────────
enum { S_IDLE, S_MOVE, S_ACT, S_TARGET, S_BUILD, S_WIN };
static int  state = S_IDLE;
static int  cx = 9, cy = 8;       // cursor cell
static int  sel = -1;             // selected unit
static int  origX, origY;         // pre-move position (for undo)
static int  buildX, buildY;       // factory cell the build menu spawns into
static int  menuSel;
static int  enemyMsg;             // "enemy turn" flash timer
static int  lastmx = -1, lastmy = -1;
static bool ready = false;

// ── tiny effect pools (sparks + floating numbers) ────────────────────────────
typedef struct { float x, y, vx, vy; int life, col; } Spark;
typedef struct { float x, y; int life, col; char txt[8]; } Pop;
static Spark sparks[48];
static Pop   pops[10];
static int   flash;               // white attack-flash frames

static void add_spark(float x, float y, int col) {
    for (int i = 0; i < 48; i++) if (sparks[i].life <= 0) {
        float a = rnd(360), s = rnd_float_between(0.6f, 2.2f);
        sparks[i] = (Spark){ x, y, dx(s,a), dy(s,a) - 0.6f, rnd_between(14,26), col };
        return;
    }
}
static void add_pop(float x, float y, int col, const char *t) {
    for (int i = 0; i < 10; i++) if (pops[i].life <= 0) {
        pops[i].x = x; pops[i].y = y; pops[i].life = 38; pops[i].col = col;
        int k = 0; while (t[k] && k < 7) { pops[i].txt[k] = t[k]; k++; } pops[i].txt[k] = 0;
        return;
    }
}

// ── helpers ──────────────────────────────────────────────────────────────────
static bool in_grid(int x, int y) { return x >= 0 && y >= 0 && x < COLS && y < ROWS; }
static int  unit_at(int x, int y) {
    for (int i = 0; i < MAXU; i++) if (army[i].type >= 0 && army[i].x == x && army[i].y == y) return i;
    return -1;
}
static bool is_building(int x, int y) { return in_grid(x,y) && bo[y][x] >= -1; }
static bool capturable_by(int ui)  {   // can THIS unit capture the tile it stands on?
    Unit *u = &army[ui];
    return (u->type == INF || u->type == MECH) && is_building(u->x, u->y) && bo[u->y][u->x] != u->team;
}
static int  dmg_calc(int at, int ah, int dt, int dh, int dx_, int dy_) {
    int base = DMG[at][dt];
    if (base <= 0) return 0;
    float raw = base * (ah / 10.0f);
    float f   = 1.0f - 0.1f * STARS[terr[dy_][dx_]] * (dh / 10.0f);
    if (f < 0.1f) f = 0.1f;
    int loss = (int)(raw * f / 10.0f + 0.5f);
    if (loss < 0) loss = 0; if (loss > dh) loss = dh;
    return loss;
}
static bool can_hit(int ai, int di) {           // attacker (post-move) can strike defender?
    Unit *a = &army[ai], *d = &army[di];
    if (d->team == a->team) return false;
    int md = abs(a->x - d->x) + abs(a->y - d->y);
    if (INDIRECT[a->type]) return (a->x == origX && a->y == origY) && md >= 2 && md <= 3;
    return md == 1;
}

// ── Dijkstra movement range ──────────────────────────────────────────────────
#define BIG 9999
static int dist[ROWS][COLS];
static int prevc[ROWS][COLS];

static void compute_reach(int ui) {
    Unit *u = &army[ui];
    int cls = MCLASS[u->type], mv = MOVE_PTS[u->type];
    bool done[ROWS][COLS];
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        dist[y][x] = BIG; prevc[y][x] = -1; done[y][x] = false;
    }
    dist[u->y][u->x] = 0;
    for (;;) {
        int bx = -1, by = -1, bd = BIG;
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
            if (!done[y][x] && dist[y][x] < bd) { bd = dist[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        done[by][bx] = true;
        static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
        for (int k = 0; k < 4; k++) {
            int nx = bx + DX[k], ny = by + DY[k];
            if (!in_grid(nx, ny)) continue;
            int c = MCOST[cls][terr[ny][nx]];
            if (c >= 99) continue;                       // impassable
            int oc = unit_at(nx, ny);
            if (oc >= 0 && army[oc].team != u->team) continue;  // enemy blocks passage
            int nd = dist[by][bx] + c;
            if (nd <= mv && nd < dist[ny][nx]) { dist[ny][nx] = nd; prevc[ny][nx] = by * COLS + bx; }
        }
    }
}
static bool can_stop(int ui, int x, int y) {
    if (!in_grid(x, y) || dist[y][x] > MOVE_PTS[army[ui].type]) return false;
    int oc = unit_at(x, y);
    return oc < 0 || oc == ui;   // can't share a tile with another unit
}

// ── sound ────────────────────────────────────────────────────────────────────
static void snd_move(void)    { note(74, 8, 2); }
static void snd_cannon(void)  { note(38, 7, 4); shake(3.5f); }
static void snd_captick(int p){ note(58 + p, 9, 2); }
static void snd_capdone(void) { strum(60, CHORD_MAJ, 9, 4, 60); }
static void snd_turn(void)    { chord(48, CHORD_POWER, 0, 3); }
static void snd_build(void)   { note(66, 8, 3); }
static void snd_fanfare(void) {
    schedule(0,   60, 2, 5); schedule(140, 64, 2, 5);
    schedule(280, 67, 2, 5); schedule(420, 72, 2, 6);
}

// ── combat / capture / spawn ─────────────────────────────────────────────────
static int  scx(int x) { return x * TILE; }
static int  scy(int y) { return MAP_Y + y * TILE; }

static void kill_unit(int i) {
    add_spark(scx(army[i].x)+8, scy(army[i].y)+8, CLR_ORANGE);
    add_spark(scx(army[i].x)+8, scy(army[i].y)+8, CLR_YELLOW);
    army[i].type = -1;
}
static void resolve_attack(int ai, int di) {
    Unit *a = &army[ai], *d = &army[di];
    int loss = dmg_calc(a->type, a->hp, d->type, d->hp, d->x, d->y);
    d->hp -= loss;
    snd_cannon();
    flash = 3;
    for (int s = 0; s < 6; s++) add_spark(scx(d->x)+8, scy(d->y)+8, s&1 ? CLR_ORANGE : CLR_YELLOW);
    add_pop(scx(d->x)+5, scy(d->y)-2, CLR_RED, str("-%d", loss));
    if (d->hp <= 0) { kill_unit(di); }
    else {
        // counterattack: only if defender is direct-fire and now adjacent & alive
        if (!INDIRECT[a->type] && !INDIRECT[d->type] && abs(a->x-d->x)+abs(a->y-d->y) == 1) {
            int back = dmg_calc(d->type, d->hp, a->type, a->hp, a->x, a->y);
            a->hp -= back;
            add_pop(scx(a->x)+5, scy(a->y)-2, CLR_WHITE, str("-%d", back));
            for (int s = 0; s < 3; s++) add_spark(scx(a->x)+8, scy(a->y)+8, CLR_LIGHT_GREY);
            if (a->hp <= 0) kill_unit(ai);
        }
    }
    a->moved = a->acted = 1;
}
static void do_capture(int ui) {
    Unit *u = &army[ui]; int x = u->x, y = u->y;
    if (capTeam[y][x] != u->team) { capLeft[y][x] = 20; capTeam[y][x] = u->team; }
    capLeft[y][x] -= u->hp;
    snd_captick(20 - capLeft[y][x]);
    if (capLeft[y][x] <= 0) {
        bool wasHQ = (terr[y][x] == T_HQ);
        bo[y][x] = u->team; capLeft[y][x] = 20; capTeam[y][x] = -1;
        snd_capdone();
        add_pop(scx(x), scy(y)-4, CLR_GREEN, "TAKEN");
        if (wasHQ) winner = u->team;
    }
    u->moved = u->acted = 1;
}
static int spawn(int type, int team, int x, int y) {
    for (int i = 0; i < MAXU; i++) if (army[i].type < 0) {
        army[i] = (Unit){ type, team, x, y, 10, 0, 0 };
        return i;
    }
    return -1;
}
static void move_unit(int ui, int nx, int ny) {
    Unit *u = &army[ui];
    // leaving a half-captured building resets its progress
    if (is_building(u->x, u->y) && capLeft[u->y][u->x] < 20 && capTeam[u->y][u->x] == u->team) {
        capLeft[u->y][u->x] = 20; capTeam[u->y][u->x] = -1;
    }
    u->x = nx; u->y = ny; u->moved = 1;
}

// ── economy + win check ──────────────────────────────────────────────────────
static int income(int team) {
    int n = 0;
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (bo[y][x] == team) n++;
    return n * 1000;
}
static int count_units(int team) {
    int n = 0; for (int i = 0; i < MAXU; i++) if (army[i].type >= 0 && army[i].team == team) n++;
    return n;
}
static void check_winner(void) {
    if (winner >= 0) return;
    if (count_units(1) == 0) winner = 0;
    else if (count_units(0) == 0) winner = 1;
}

// ── CPU turn ─────────────────────────────────────────────────────────────────
// best attack for an enemy unit: scan its stoppable cells, find adjacent (or
// in-range, for artillery) targets, maximise damage with a bonus for kills and
// for hitting units sitting on our buildings.
static bool ai_try_attack(int ui) {
    Unit *u = &army[ui];
    int bestT = -1, bestX = u->x, bestY = u->y, bestScore = 0;
    origX = u->x; origY = u->y;          // can_hit() reads these for the indirect rule
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        if (!can_stop(ui, x, y)) continue;
        int sx = u->x, sy = u->y; u->x = x; u->y = y;   // pretend we moved here
        for (int di = 0; di < MAXU; di++) {
            if (army[di].type < 0 || army[di].team == u->team) continue;
            if (!can_hit(ui, di)) continue;
            int loss = dmg_calc(u->type, u->hp, army[di].type, army[di].hp, army[di].x, army[di].y);
            int score = loss * 10;
            if (loss >= army[di].hp) score += 60;                       // kill bonus
            if (is_building(army[di].x, army[di].y) && bo[army[di].y][army[di].x] == u->team) score += 40; // defend
            // prefer not eating a counter: subtract expected counter
            if (!INDIRECT[u->type] && !INDIRECT[army[di].type] && loss < army[di].hp)
                score -= dmg_calc(army[di].type, army[di].hp - loss, u->type, u->hp, x, y) * 4;
            if (score > bestScore) { bestScore = score; bestT = di; bestX = x; bestY = y; }
        }
        u->x = sx; u->y = sy;
    }
    if (bestT < 0) return false;
    origX = u->x; origY = u->y;
    move_unit(ui, bestX, bestY);
    origX = bestX; origY = bestY;
    resolve_attack(ui, bestT);
    return true;
}
static void ai_move_toward(int ui, int tx, int ty) {
    Unit *u = &army[ui];
    int bx = u->x, by = u->y, bd = abs(u->x-tx)+abs(u->y-ty);
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        if (!can_stop(ui, x, y)) continue;
        int d = abs(x-tx) + abs(y-ty);
        if (d < bd) { bd = d; bx = x; by = y; }
    }
    move_unit(ui, bx, by);
}
static void ai_unit(int ui) {
    Unit *u = &army[ui];
    compute_reach(ui);
    if (u->type == INF || u->type == MECH) {
        if (capturable_by(ui)) { do_capture(ui); return; }      // already standing on a target
        // head for the nearest capturable building (player HQ pulls hardest)
        int tx = -1, ty = -1, bd = BIG;
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
            if (!is_building(x,y) || bo[y][x] == u->team) continue;
            int d = abs(u->x-x) + abs(u->y-y);
            if (terr[y][x] == T_HQ) d -= 6;
            if (d < bd) { bd = d; tx = x; ty = y; }
        }
        if (ai_try_attack(ui)) return;
        if (tx >= 0) { ai_move_toward(ui, tx, ty); compute_reach(ui); }
        if (capturable_by(ui)) { do_capture(ui); return; }
        u->acted = 1; return;
    }
    // combat vehicle: attack if possible, else hunt the nearest enemy / their HQ
    if (ai_try_attack(ui)) return;
    int tx = -1, ty = -1, bd = BIG;
    for (int i = 0; i < MAXU; i++) if (army[i].type >= 0 && army[i].team != u->team) {
        int d = abs(u->x-army[i].x) + abs(u->y-army[i].y);
        if (d < bd) { bd = d; tx = army[i].x; ty = army[i].y; }
    }
    if (tx >= 0) ai_move_toward(ui, tx, ty);   // ai_try_attack already scanned every reachable cell
    u->acted = 1;
}
static void ai_build(void) {
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        if (terr[y][x] != T_FACT || bo[y][x] != 1 || unit_at(x,y) >= 0) continue;
        int want = -1;
        int nInf = 0;
        for (int i = 0; i < MAXU; i++) if (army[i].type==INF && army[i].team==1) nInf++;
        if (nInf < 2 && funds[1] >= COST[INF])      want = INF;
        else if (funds[1] >= COST[TANK])            want = TANK;
        else if (funds[1] >= COST[RECON])           want = RECON;
        else if (funds[1] >= COST[MECH])            want = MECH;
        else if (funds[1] >= COST[INF])             want = INF;
        if (want >= 0) { int i = spawn(want, 1, x, y); if (i>=0){ army[i].acted=1; funds[1]-=COST[want]; } }
    }
}
static void run_ai(void) {
    for (int i = 0; i < MAXU; i++) if (army[i].type>=0 && army[i].team==1) { army[i].moved=army[i].acted=0; }
    for (int i = 0; i < MAXU; i++) {
        if (army[i].type < 0 || army[i].team != 1 || army[i].acted) continue;
        ai_unit(i);
        if (winner >= 0) return;
    }
    ai_build();
}
static void end_turn(void) {
    if (winner >= 0) return;
    sel = -1; state = S_IDLE;
    // enemy plays
    funds[1] += income(1);
    run_ai();
    check_winner();
    if (winner >= 0) { state = S_WIN; snd_fanfare(); return; }
    // back to the player
    turn_no++;
    funds[0] += income(0);
    for (int i = 0; i < MAXU; i++) if (army[i].type>=0 && army[i].team==0) { army[i].moved=army[i].acted=0; }
    enemyMsg = 60;
    snd_turn();
}

// ── setup ────────────────────────────────────────────────────────────────────
static const char *LAYOUT[ROWS] = {
    "~~s...ff....ff...s~~",
    "~s..f....rr...f...s~",
    "s..m.....rr....m...s",
    "..m.....rrrr....m...",
    ".f...rrrr..rrr....f.",
    ".f...rrrr..rrr....f.",
    "..m.....rrrr....m...",
    "s..m.....rr....m...s",
    "~s..f....rr...f...s~",
    "~~s...ff....ff...s~~",
};
static int char_terr(char c) {
    switch (c) {
        case 'f': return FOREST; case 'm': return MTN; case 'r': return ROAD;
        case '~': return SEA;    case 's': return SHOAL; default: return PLAINS;
    }
}
static void reset_game(void) {
    for (int y = 0; y < ROWS; y++) {
        const char *row = LAYOUT[y];
        for (int x = 0; x < COLS; x++) {
            char c = row[x] ? row[x] : '.';
            terr[y][x] = char_terr(c);
            bo[y][x] = -2; capLeft[y][x] = 20; capTeam[y][x] = -1;
        }
    }
    // buildings: {x, y, type, owner(-1 neutral / 0 red / 1 blue)}
    static const int B[][4] = {
        {10,1,T_HQ,1},{8,1,T_FACT,1},{11,1,T_CITY,1},
        {9,8,T_HQ,0},{11,8,T_FACT,0},{8,8,T_CITY,0},
        {5,3,T_CITY,-1},{14,3,T_CITY,-1},{5,6,T_CITY,-1},{14,6,T_CITY,-1},
    };
    for (int i = 0; i < (int)(sizeof B / sizeof B[0]); i++) {
        terr[B[i][1]][B[i][0]] = B[i][2];
        bo[B[i][1]][B[i][0]]   = B[i][3];
    }
    // write the ground layer into the map grid for map() to draw
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
        mset(x, y, GROUND_SLOT[terr[y][x]]);

    for (int i = 0; i < MAXU; i++) army[i].type = -1;
    static const int U[][4] = {  // {x, y, type, team}
        {8,7,INF,0},{11,7,INF,0},{10,8,MECH,0},{13,8,RECON,0},{9,7,TANK,0},{7,8,ARTY,0},
        {11,2,INF,1},{8,2,INF,1},{9,1,MECH,1},{6,1,RECON,1},{10,2,TANK,1},{12,1,ARTY,1},
    };
    for (int i = 0; i < (int)(sizeof U / sizeof U[0]); i++)
        spawn(U[i][2], U[i][3], U[i][0], U[i][1]);

    for (int i = 0; i < 48; i++) sparks[i].life = 0;
    for (int i = 0; i < 10; i++) pops[i].life = 0;
    funds[0] = funds[1] = 4000;
    turn_no = 1; winner = -1; sel = -1; state = S_IDLE;
    cx = 9; cy = 8; enemyMsg = 0; flash = 0;
    ready = true;
}

void init(void) {
    colorkey(0);
    instrument(8, INSTR_SQUARE, 1, 40, 0, 30);                 // move blip
    instrument(9, INSTR_TRI,    2, 60, 4, 120);                // capture
    instrument(7, INSTR_NOISE,  1, 120, 2, 200);              // cannon
    instrument_filter(7, FILTER_LOW, 500, 3);
    instrument(5, INSTR_NOISE,  1, 50, 0, 40);                 // snare
    instrument(6, INSTR_SINE,   1, 70, 0, 30);                 // kick
    bpm(96);
    reset_game();
}

// ── input plumbing ───────────────────────────────────────────────────────────
static bool confirm_pressed(void) { return mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A); }
static bool cancel_pressed(void)  { return mouse_pressed(MOUSE_RIGHT) || btnp(0, BTN_B); }
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && mouse_x()>=x && mouse_x()<x+w && mouse_y()>=y && mouse_y()<y+h;
}
static void sync_cursor(void) {
    if (mouse_x() != lastmx || mouse_y() != lastmy) {     // mouse moved → follow it
        lastmx = mouse_x(); lastmy = mouse_y();
        int mx = mouse_x()/TILE, my = (mouse_y()-MAP_Y)/TILE;
        if (mouse_y() >= MAP_Y && in_grid(mx, my)) { cx = mx; cy = my; }
    }
    if (btnp(0,BTN_LEFT)  || keyp(KEY_LEFT))  cx = max(0, cx-1);
    if (btnp(0,BTN_RIGHT) || keyp(KEY_RIGHT)) cx = min(COLS-1, cx+1);
    if (btnp(0,BTN_UP)    || keyp(KEY_UP))    cy = max(0, cy-1);
    if (btnp(0,BTN_DOWN)  || keyp(KEY_DOWN))  cy = min(ROWS-1, cy+1);
}

static void lblcpy(char *d, const char *s) { int k = 0; while (s[k] && k < 11) { d[k]=s[k]; k++; } d[k]=0; }
// count current action-menu options for the selected (post-move) unit
static int menu_opts(char labels[4][12]) {
    int n = 0;
    bool atk = false;
    for (int i = 0; i < MAXU; i++) if (army[i].type>=0 && can_hit(sel, i)) { atk = true; break; }
    if (atk)                lblcpy(labels[n++], "ATTACK");
    if (capturable_by(sel)) lblcpy(labels[n++], "CAPTURE");
    lblcpy(labels[n++], "WAIT");
    return n;
}

void update(void) {
    if (!ready) reset_game();
    if (enemyMsg > 0) enemyMsg--;

    // age effects
    for (int i = 0; i < 48; i++) if (sparks[i].life > 0) {
        sparks[i].x += sparks[i].vx; sparks[i].y += sparks[i].vy; sparks[i].vy += 0.12f; sparks[i].life--;
    }
    for (int i = 0; i < 10; i++) if (pops[i].life > 0) { pops[i].y -= 0.5f; pops[i].life--; }
    if (flash > 0) flash--;

    // subtle martial bed
    if (state != S_WIN) {
        static int lb = -1; int b = beat();
        if (b != lb) { lb = b; if (b % 2 == 0) note(36, 6, 2); else note(60, 5, 1); }
    }

    if (state == S_WIN) {
        if (confirm_pressed() || mouse_pressed(MOUSE_LEFT)) reset_game();
        return;
    }

    if (state == S_IDLE || state == S_MOVE) sync_cursor();

    // a left click only counts as a map action when it lands on the map itself
    bool mapClick = mouse_pressed(MOUSE_LEFT) && mouse_y() >= MAP_Y && mouse_y() < INFO_Y;
    bool confirm  = btnp(0, BTN_A) || mapClick;

    if (state == S_IDLE) {
        if (clicked(SCREEN_W-66, 4, 62, HUD_Y-8) || keyp(KEY_SPACE)) { end_turn(); return; }
        if (confirm) {
            int ui = unit_at(cx, cy);
            if (ui >= 0 && army[ui].team == 0 && !army[ui].acted) {
                sel = ui; origX = cx; origY = cy; compute_reach(ui); state = S_MOVE; snd_move();
            } else if (terr[cy][cx]==T_FACT && bo[cy][cx]==0 && unit_at(cx,cy)<0) {
                menuSel = 0; buildX = cx; buildY = cy; state = S_BUILD;
            }
        }
        return;
    }
    if (state == S_MOVE) {
        if (cancel_pressed()) { sel = -1; state = S_IDLE; return; }
        if (confirm && can_stop(sel, cx, cy)) { move_unit(sel, cx, cy); menuSel = 0; state = S_ACT; }
        return;
    }
    if (state == S_ACT) {
        char labels[4][12]; int n = menu_opts(labels);
        if (cancel_pressed()) {                       // undo the move
            army[sel].x = origX; army[sel].y = origY; army[sel].moved = 0;
            cx = origX; cy = origY; state = S_MOVE; return;
        }
        if (btnp(0,BTN_UP))   menuSel = (menuSel + n - 1) % n;
        if (btnp(0,BTN_DOWN)) menuSel = (menuSel + 1) % n;
        // mouse hover/click over the menu
        int mxp = clamp(scx(army[sel].x)+18, 2, SCREEN_W-66), myp = clamp(scy(army[sel].y), HUD_Y+2, INFO_Y-(n*12+4));
        for (int i = 0; i < n; i++)
            if (mouse_x()>=mxp && mouse_x()<mxp+62 && mouse_y()>=myp+2+i*12 && mouse_y()<myp+2+i*12+12) {
                menuSel = i; if (mouse_pressed(MOUSE_LEFT)) goto choose;
            }
        if (btnp(0,BTN_A)) goto choose;
        return;
choose: {
            const char *pick = labels[menuSel];
            if (pick[0]=='A') { menuSel = 0; state = S_TARGET; }
            else if (pick[0]=='C') { do_capture(sel); sel=-1; state=S_IDLE; check_winner(); if(winner>=0){state=S_WIN;snd_fanfare();} }
            else { army[sel].acted = 1; sel=-1; state=S_IDLE; }
        }
        return;
    }
    if (state == S_TARGET) {
        if (cancel_pressed()) { state = S_ACT; return; }
        // collect valid targets
        int tgt[8], nt = 0;
        for (int i = 0; i < MAXU && nt < 8; i++) if (army[i].type>=0 && can_hit(sel, i)) tgt[nt++] = i;
        if (nt == 0) { state = S_ACT; return; }
        // cursor cycling with keys
        if (btnp(0,BTN_LEFT)||btnp(0,BTN_UP))   menuSel = (menuSel + nt - 1) % nt;
        if (btnp(0,BTN_RIGHT)||btnp(0,BTN_DOWN))menuSel = (menuSel + 1) % nt;
        if (menuSel >= nt) menuSel = 0;
        // mouse: hovering a target cell selects it
        int mtx = mouse_x()/TILE, mty = (mouse_y()-MAP_Y)/TILE;
        for (int i = 0; i < nt; i++) if (army[tgt[i]].x==mtx && army[tgt[i]].y==mty) menuSel = i;
        cx = army[tgt[menuSel]].x; cy = army[tgt[menuSel]].y;   // cursor tracks the chosen target
        if (btnp(0,BTN_A) || mouse_pressed(MOUSE_LEFT)) {
            resolve_attack(sel, tgt[menuSel]);
            sel = -1; state = S_IDLE; check_winner();
            if (winner >= 0) { state = S_WIN; snd_fanfare(); }
        }
        return;
    }
    if (state == S_BUILD) {
        if (cancel_pressed()) { state = S_IDLE; return; }
        if (btnp(0,BTN_UP))   menuSel = (menuSel + NTYPE - 1) % NTYPE;
        if (btnp(0,BTN_DOWN)) menuSel = (menuSel + 1) % NTYPE;
        int bx = 70, by = 50;
        for (int i = 0; i < NTYPE; i++)
            if (mouse_x()>=bx && mouse_x()<bx+120 && mouse_y()>=by+i*16 && mouse_y()<by+i*16+16) {
                menuSel = i; if (mouse_pressed(MOUSE_LEFT)) goto buildit;
            }
        if (btnp(0,BTN_A)) goto buildit;
        return;
buildit:
        if (funds[0] >= COST[menuSel] && unit_at(buildX,buildY) < 0) {
            int i = spawn(menuSel, 0, buildX, buildY);
            if (i >= 0) { army[i].acted = 1; funds[0] -= COST[menuSel]; snd_build(); }
        }
        state = S_IDLE; return;
    }
}

// ── drawing ──────────────────────────────────────────────────────────────────
static void team_pal(int team) {
    if (team == 0) { pal(28, CLR_RED);  pal(29, CLR_DARK_RED); }
    else           { pal(28, CLR_BLUE); pal(29, CLR_TRUE_BLUE); }
}
static void neutral_pal(void) { pal(28, CLR_LIGHT_GREY); pal(29, CLR_DARK_GREY); }

static void draw_stars(int x, int y, int n, int col) {
    for (int i = 0; i < n; i++) rectfill(x + i*5, y, 3, 3, col);
}
static void draw_unit(int i) {
    Unit *u = &army[i];
    int sx = scx(u->x), sy = scy(u->y);
    int bob = (state==S_IDLE && !u->acted) ? (anim(2, 2.0f, (float)i/MAXU) ? -1 : 0) : 0;
    team_pal(u->team);
    spr(USPRITE[u->type], sx, sy + bob);
    pal_reset();
    if (u->acted) { fillp(FILL_CHECKER, -1); rectfill(sx+1, sy+1, 14, 14, CLR_BLACK); fillp_reset(); }
    // HP chip (hidden at full)
    if (u->hp < 10 && u->hp > 0) {
        rectfill(sx+9, sy+10, 7, 6, CLR_BLACK);
        print(str("%d", u->hp), sx+10, sy+10, u->hp<=3 ? CLR_RED : CLR_WHITE);
    }
    // capture ring
    if (is_building(u->x,u->y) && capTeam[u->y][u->x]==u->team && capLeft[u->y][u->x]<20) {
        float pct = (20 - capLeft[u->y][u->x]) / 20.0f;
        for (float a = -90; a < -90 + 360; a += 24) circfill(sx+8+cos_deg(a)*8, sy+8+sin_deg(a)*8, 1, CLR_DARK_GREEN);
        for (float a = -90; a < -90 + 360*pct; a += 24) circfill(sx+8+cos_deg(a)*8, sy+8+sin_deg(a)*8, 1, CLR_GREEN);
    }
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_DARK_BLUE);

    map(0, 0, 0, MAP_Y, COLS, ROWS);                 // terrain

    // buildings (recolored by owner)
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (is_building(x,y)) {
        if (bo[y][x] < 0) neutral_pal(); else team_pal(bo[y][x]);
        int slot = terr[y][x]==T_HQ ? 9 : terr[y][x]==T_FACT ? 8 : 7;
        spr(slot, scx(x), scy(y));
        pal_reset();
    }

    // movement range overlay
    if (state == S_MOVE && sel >= 0) {
        fillp(FILL_CHECKER, -1);
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
            if (can_stop(sel, x, y)) rectfill(scx(x), scy(y), TILE, TILE, CLR_BLUE);
        fillp_reset();
        // move arrow: trace prevc from cursor back to the unit
        if (can_stop(sel, cx, cy)) {
            int px = cx, py = cy;
            while (px != origX || py != origY) {
                int p = prevc[py][px]; if (p < 0) break;
                int qx = p % COLS, qy = p / COLS;
                line(scx(px)+8, scy(py)+8, scx(qx)+8, scy(qy)+8, CLR_LIGHT_YELLOW);
                px = qx; py = qy;
            }
            circfill(scx(cx)+8, scy(cy)+8, 2, CLR_YELLOW);
        }
    }
    // attack-range overlay (targeting)
    if (state == S_TARGET) {
        fillp(FILL_CHECKER, -1);
        for (int i = 0; i < MAXU; i++) if (army[i].type>=0 && can_hit(sel, i))
            rectfill(scx(army[i].x), scy(army[i].y), TILE, TILE, CLR_RED);
        fillp_reset();
    }

    for (int i = 0; i < MAXU; i++) if (army[i].type >= 0) draw_unit(i);

    // sparks + pops
    for (int i = 0; i < 48; i++) if (sparks[i].life > 0)
        pset((int)sparks[i].x, (int)sparks[i].y, sparks[i].col);
    for (int i = 0; i < 10; i++) if (pops[i].life > 0)
        print(pops[i].txt, (int)pops[i].x, (int)pops[i].y, pops[i].col);

    // cursor
    if (state != S_WIN) {
        int p = (frame() % 30 < 15) ? 0 : 1;
        rect(scx(cx)-p, scy(cy)-p, TILE+2*p, TILE+2*p, state==S_TARGET ? CLR_RED : CLR_YELLOW);
    }

    if (flash > 0) { fillp(FILL_CHECKER, -1); rectfill(0, MAP_Y, SCREEN_W, ROWS*TILE, CLR_WHITE); fillp_reset(); }

    // ── action menu ──
    if (state == S_ACT && sel >= 0) {
        char labels[4][12]; int n = menu_opts(labels);
        int mx = clamp(scx(army[sel].x)+18, 2, SCREEN_W-66), my = clamp(scy(army[sel].y), HUD_Y+2, INFO_Y-(n*12+4));
        rectfill(mx, my, 62, n*12+4, CLR_DARKER_BLUE); rect(mx, my, 62, n*12+4, CLR_WHITE);
        for (int i = 0; i < n; i++) {
            if (i == menuSel) rectfill(mx+1, my+2+i*12, 60, 12, CLR_TRUE_BLUE);
            print(labels[i], mx+5, my+4+i*12, i==menuSel ? CLR_WHITE : CLR_LIGHT_GREY);
        }
    }
    // ── build menu ──
    if (state == S_BUILD) {
        int bx = 70, by = 50;
        rectfill(bx-4, by-16, 128, NTYPE*16+22, CLR_DARKER_BLUE); rect(bx-4, by-16, 128, NTYPE*16+22, CLR_WHITE);
        print("BUILD UNIT", bx, by-12, CLR_YELLOW);
        for (int i = 0; i < NTYPE; i++) {
            bool ok = funds[0] >= COST[i];
            if (i == menuSel) rectfill(bx-2, by+i*16, 124, 16, CLR_TRUE_BLUE);
            team_pal(0); spr(USPRITE[i], bx, by+i*16-2); pal_reset();
            print(UNAME[i], bx+18, by+i*16+1, ok ? CLR_WHITE : CLR_DARK_GREY);
            print_right(str("$%d", COST[i]), bx+118, by+i*16+1, ok ? CLR_YELLOW : CLR_DARK_GREY);
        }
    }

    // ── top HUD ──
    rectfill(0, 0, SCREEN_W, HUD_Y, CLR_BROWNISH_BLACK);
    rectfill(0, 2, 4, HUD_Y-4, CLR_RED);
    print(str("RED  $%d", funds[0]), 8, 3, CLR_PEACH);
    print(str("BLUE $%d", funds[1]), 8, 13, CLR_BLUE);
    print_centered(str("DAY %d", turn_no), SCREEN_W/2, 4, CLR_WHITE);
    if (enemyMsg > 0) print_centered("-- your turn --", SCREEN_W/2, 14, CLR_YELLOW);
    else              print_centered("capture the HQ", SCREEN_W/2, 14, CLR_LIGHT_GREY);
    rectfill(SCREEN_W-66, 4, 62, HUD_Y-8, CLR_DARK_GREEN); rect(SCREEN_W-66, 4, 62, HUD_Y-8, CLR_WHITE);
    print("END TURN", SCREEN_W-62, 9, CLR_WHITE);

    // ── bottom info strip: terrain + occupant under cursor ──
    rectfill(0, INFO_Y, SCREEN_W, SCREEN_H-INFO_Y, CLR_DARKER_PURPLE);
    int t = terr[cy][cx];
    print(is_building(cx,cy) && bo[cy][cx]>=0 ? (bo[cy][cx]==0?"RED ":"BLUE") : "", 2, INFO_Y+4, bo[cy][cx]==0?CLR_RED:CLR_BLUE);
    print(TNAME[t], 30, INFO_Y+4, CLR_WHITE);
    draw_stars(96, INFO_Y+5, STARS[t], CLR_YELLOW);
    int hu = unit_at(cx, cy);
    if (hu >= 0) {
        print(str("%s", UNAME[army[hu].type]), 130, INFO_Y+4, army[hu].team==0?CLR_PEACH:CLR_BLUE);
        print(str("HP %d", army[hu].hp), 224, INFO_Y+4, CLR_WHITE);
    } else {
        const char *hint = state==S_MOVE ? "click a tile  X cancel" :
                           state==S_TARGET ? "pick target  X back" :
                           state==S_ACT ? "choose action" : "click a unit   SPACE end turn";
        print_right(hint, SCREEN_W-3, INFO_Y+4, CLR_INDIGO);
    }

    // ── attack forecast ──
    if (state == S_TARGET && sel >= 0) {
        int tgt[8], nt = 0;
        for (int i = 0; i < MAXU && nt < 8; i++) if (army[i].type>=0 && can_hit(sel, i)) tgt[nt++] = i;
        if (nt > 0) {
            if (menuSel >= nt) menuSel = 0;
            Unit *d = &army[tgt[menuSel]];
            int loss = dmg_calc(army[sel].type, army[sel].hp, d->type, d->hp, d->x, d->y);
            int bx = clamp(scx(d->x)-24, 2, SCREEN_W-66), by = clamp(scy(d->y)-22, HUD_Y+2, INFO_Y-22);
            rectfill(bx, by, 64, 20, CLR_BLACK); rect(bx, by, 64, 20, CLR_RED);
            print(str("%s", UNAME[d->type]), bx+3, by+2, CLR_LIGHT_GREY);
            print(str("dmg -%d hp", loss), bx+3, by+11, loss>=d->hp?CLR_GREEN:CLR_YELLOW);
        }
    }

    // ── win banner ──
    if (state == S_WIN) {
        fade(0.5f);
        int w = 220, bx = (SCREEN_W-w)/2;
        rectfill(bx, 78, w, 44, winner==0?CLR_DARK_GREEN:CLR_DARK_PURPLE);
        rect(bx, 78, w, 44, CLR_WHITE);
        print_centered(winner==0 ? "VICTORY!" : "DEFEAT", SCREEN_W/2, 86, winner==0?CLR_GREEN:CLR_RED);
        print_centered(winner==0 ? "the enemy HQ has fallen" : "your forces are routed", SCREEN_W/2, 100, CLR_LIGHT_GREY);
        print_centered("click for a new battle", SCREEN_W/2, 110, CLR_YELLOW);
    }
    cursor_draw(CUR_ARROW);
}
