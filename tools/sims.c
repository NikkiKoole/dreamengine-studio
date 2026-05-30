#include "studio.h"

// LIL LIFE — a tiny Sims. Build a house, drop in furniture, then watch your
// sim look after itself: it tracks its own needs, walks (pathfinding around
// your walls) to whatever object fixes the most urgent one, and uses it.
//
// MOVE cursor: WASD / arrows     Z place     X erase
// ,  toggle BUILD / LIVE         .  cycle furniture

#define TS  12
#define OY  26                  // play area starts below the needs bars
#define GW  26
#define GH  13
#define N   (GW * GH)

// tile types
enum { EMPTY, WALL, BED, FRIDGE, TOILET, SHOWER, TV };
// needs
enum { FOOD, NRG, BLAD, HYG, FUN, NEEDS };
// sim states
enum { IDLE, WALK, USE };

static unsigned char cell[GH][GW];
static float need[NEEDS];
static const float decay[NEEDS]    = { 1.4f, 1.0f, 1.8f, 0.9f, 1.2f };
static const int   needFurn[NEEDS] = { FRIDGE, BED, TOILET, SHOWER, TV };
static const char *needName[NEEDS] = { "FOOD", "NRG", "BLAD", "HYG", "FUN" };
static const char *doingName[NEEDS] = { "eating", "sleeping", "on toilet", "showering", "watching TV" };

static const int   toolType[6]  = { WALL, BED, FRIDGE, TOILET, SHOWER, TV };
static const char *toolName[6]  = { "WALL", "BED", "FRIDGE", "TOILET", "SHOWER", "TV" };

static float simx, simy;        // pixel center
static int   state, useNeed, dir;
static float useT, decideT;
static int   mode, tool, curx, cury;   // mode 0=LIVE 1=BUILD

// path
static int pathx[N], pathy[N], pn, pi;

static int   walkable(int x, int y) { return x >= 0 && y >= 0 && x < GW && y < GH && cell[y][x] == EMPTY; }
static int   stx(void) { return (int)(simx / TS); }
static int   sty(void) { return (int)((simy - OY) / TS); }

// ---- BFS pathfinding over floor tiles ----
static int prevd[N], q[N];
static bool find_path(int sx, int sy, int gx, int gy) {
    if (!walkable(gx, gy)) return false;
    for (int i = 0; i < N; i++) prevd[i] = -2;
    int head = 0, tail = 0, s = sy * GW + sx, g = gy * GW + gx;
    prevd[s] = -1; q[tail++] = s;
    int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    bool found = false;
    while (head < tail) {
        int c = q[head++]; if (c == g) { found = true; break; }
        int cx = c % GW, cy = c / GW;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (!walkable(nx, ny)) continue;
            int ni = ny * GW + nx; if (prevd[ni] != -2) continue;
            prevd[ni] = c; q[tail++] = ni;
        }
    }
    if (!found) return false;
    int tx[N], ty[N], tn = 0;
    for (int c = g; c != -1; c = prevd[c]) { tx[tn] = c % GW; ty[tn] = c / GW; tn++; }
    pn = 0;
    for (int i = tn - 2; i >= 0; i--) { pathx[pn] = tx[i]; pathy[pn] = ty[i]; pn++; }
    pi = 0; return true;
}

// best reachable use-spot next to a piece of furniture that fixes need n
static int bx[N], by[N], bn;
static bool choose_target(int n) {
    int ft = needFurn[n]; bn = -1; int best = 1 << 30;
    int sX = stx(), sY = sty();
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            if (cell[gy][gx] != ft) continue;
            int nb[4][2] = { { gx + 1, gy }, { gx - 1, gy }, { gx, gy + 1 }, { gx, gy - 1 } };
            for (int k = 0; k < 4; k++)
                if (walkable(nb[k][0], nb[k][1]) && find_path(sX, sY, nb[k][0], nb[k][1]) && pn < best) {
                    best = pn; bn = pn; for (int i = 0; i < pn; i++) { bx[i] = pathx[i]; by[i] = pathy[i]; }
                }
        }
    if (bn < 0) return false;
    pn = bn; for (int i = 0; i < pn; i++) { pathx[i] = bx[i]; pathy[i] = by[i]; }
    pi = 0; useNeed = n; return true;
}

static void wander(void) {
    int sX = stx(), sY = sty();
    for (int t = 0; t < 24; t++) {
        int gx = rnd(GW), gy = rnd(GH);
        if (walkable(gx, gy) && find_path(sX, sY, gx, gy)) { state = WALK; useNeed = -1; return; }
    }
    state = IDLE;
}

static void house(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) cell[y][x] = EMPTY;
    int x0 = 3, y0 = 1, x1 = 22, y1 = 11;
    for (int x = x0; x <= x1; x++) { cell[y0][x] = WALL; cell[y1][x] = WALL; }
    for (int y = y0; y <= y1; y++) { cell[y][x0] = WALL; cell[y][x1] = WALL; }
    cell[6][x0] = EMPTY;                 // a doorway
    cell[3][5]  = BED;
    cell[3][20] = FRIDGE;
    cell[9][5]  = SHOWER;
    cell[9][20] = TOILET;
    cell[9][12] = TV;
}

void init(void) {
    house();
    for (int i = 0; i < NEEDS; i++) need[i] = rnd_between(55, 85);
    simx = 12 * TS + TS / 2; simy = OY + 6 * TS + TS / 2;
    state = IDLE; useNeed = -1; dir = 1; useT = 0; decideT = 0;
    mode = 0; tool = 1; curx = 12; cury = 6;
}

void update(void) {
    float d = dt();

    if (btnp(1, BTN_A)) mode = !mode;                  // , toggles mode

    if (mode == 1) {                                   // ---- BUILD ----
        if (btnp(0, BTN_UP)    || btnp(1, BTN_UP))    cury = max(0, cury - 1);
        if (btnp(0, BTN_DOWN)  || btnp(1, BTN_DOWN))  cury = min(GH - 1, cury + 1);
        if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  curx = max(0, curx - 1);
        if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) curx = min(GW - 1, curx + 1);
        if (btnp(1, BTN_B)) tool = (tool + 1) % 6;     // . cycles furniture
        if (btn(0, BTN_A))  cell[cury][curx] = toolType[tool];
        if (btn(0, BTN_B))  cell[cury][curx] = EMPTY;
        return;                                        // time is paused while building
    }

    // ---- LIVE ----
    for (int i = 0; i < NEEDS; i++) need[i] = clamp(need[i] - decay[i] * d, 0, 100);

    if (state == IDLE) {
        decideT -= d;
        if (decideT <= 0) {
            decideT = 0.5f;
            int low = 0; for (int i = 1; i < NEEDS; i++) if (need[i] < need[low]) low = i;
            if (need[low] < 78) { if (choose_target(low)) state = WALK; else wander(); }
            else if (chance(2)) wander();
        }
    } else if (state == WALK) {
        if (pi >= pn) {                                // arrived
            if (useNeed >= 0) { state = USE; useT = 6; }
            else state = IDLE;
        } else {
            float tx = pathx[pi] * TS + TS / 2.0f, ty = OY + pathy[pi] * TS + TS / 2.0f;
            float ddx = tx - simx, ddy = ty - simy, d = fsqrt(ddx * ddx + ddy * ddy), step = 42 * d;
            if (d <= step) { simx = tx; simy = ty; pi++; }
            else { simx += ddx / d * step; simy += ddy / d * step;
                   dir = (ddx < 0) ? -1 : (ddx > 0) ? 1 : dir; }
        }
    } else { // USE
        need[useNeed] = min(100, need[useNeed] + 22 * d);
        useT -= d;
        if (useT <= 0 || need[useNeed] >= 97) { state = IDLE; useNeed = -1; }
    }
}

// ---- drawing ----
static void furn(int type, int px, int py) {
    if (type == WALL)        { rectfill(px, py, TS, TS, CLR_DARK_GREY); rectfill(px, py, TS, 2, CLR_LIGHT_GREY); }
    else if (type == BED)    { rectfill(px + 1, py + 3, TS - 2, TS - 4, CLR_BROWN);
                               rectfill(px + 1, py + 3, 4, TS - 4, CLR_WHITE);
                               rectfill(px + 5, py + 4, TS - 6, TS - 6, CLR_BLUE); }
    else if (type == FRIDGE) { rectfill(px + 2, py + 1, TS - 4, TS - 1, CLR_LIGHT_GREY);
                               line(px + 2, py + 6, px + TS - 2, py + 6, CLR_DARK_GREY);
                               rectfill(px + TS - 4, py + 3, 1, 2, CLR_DARK_GREY); }
    else if (type == TOILET) { rectfill(px + 3, py + 2, TS - 6, 3, CLR_WHITE);
                               circfill(px + TS / 2, py + 8, 3, CLR_WHITE); }
    else if (type == SHOWER) { rectfill(px + 1, py + 1, TS - 2, TS - 2, CLR_BLUE);
                               for (int i = 0; i < 4; i++) pset(px + 3 + i * 2, py + 4 + (i & 1) * 3, CLR_WHITE); }
    else if (type == TV)     { rectfill(px + 1, py + 2, TS - 2, TS - 5, CLR_BLACK);
                               rectfill(px + 2, py + 3, TS - 4, TS - 7, CLR_TRUE_BLUE);
                               rectfill(px + TS / 2 - 2, py + TS - 3, 4, 2, CLR_DARK_GREY); }
}

static void draw_sim(int x, int y) {
    int bob = (state == WALK) ? (int)(sin_deg(now() * 360) * 1) : 0;
    y += bob;
    circfill(x, y - 4, 3, CLR_LIGHT_PEACH);            // head
    rectfill(x - 3, y - 1, 6, 6, CLR_DARK_PURPLE);     // body
    pset(x - 1 + (dir > 0 ? 1 : -1), y - 4, CLR_BLACK);
    pset(x + 1 + (dir > 0 ? 1 : -1), y - 4, CLR_BLACK);
    if (state == USE) { print("z", x + 4, y - 9, CLR_WHITE); }   // little activity puff
}

static void statbar(int x, const char *lbl, float v) {
    int c = v > 50 ? CLR_GREEN : v > 25 ? CLR_YELLOW : CLR_RED;
    print(lbl, x, 2, CLR_LIGHT_GREY);
    bar(x, 12, 44, 5, v / 100.0f, c, CLR_DARKER_GREY);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // floor + furniture
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int px = gx * TS, py = OY + gy * TS;
            if (cell[gy][gx] == EMPTY) { rectfill(px, py, TS, TS, ((gx + gy) & 1) ? CLR_BROWNISH_BLACK : CLR_DARK_BROWN); }
            else furn(cell[gy][gx], px, py);
        }

    draw_sim((int)simx, (int)simy);

    // needs bars
    rectfill(0, 0, SCREEN_W, OY - 2, CLR_BLACK);
    for (int i = 0; i < NEEDS; i++) statbar(4 + i * 50, needName[i], need[i]);

    int avg = 0; for (int i = 0; i < NEEDS; i++) avg += (int)need[i]; avg /= NEEDS;

    // bottom bar
    rectfill(0, SCREEN_H - 14, SCREEN_W, 14, CLR_BLACK);
    if (mode == 1) {
        // build cursor
        rect(curx * TS, OY + cury * TS, TS, TS, CLR_YELLOW);
        for (int i = 0; i < 6; i++)
            print(toolName[i], 4 + i * 52, SCREEN_H - 11, i == tool ? CLR_YELLOW : CLR_DARK_GREY);
    } else {
        const char *act = state == USE ? doingName[useNeed]
                        : state == WALK ? "walking..."
                        : avg > 60 ? "relaxing" : avg > 30 ? "restless" : "miserable";
        print(str("[,] build    sim is %s", act), 6, SCREEN_H - 11, CLR_LIGHT_GREY);
    }
}
