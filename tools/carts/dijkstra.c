/* de:meta
{
  "title": "dijkstra maps",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "probe",
    "tech-demo"
  ],
  "teaches": [
    "pathfinding",
    "algorithm-visualization"
  ],
  "lineage": "Direct interactive implementation of the RogueBasin 'incredible power of Dijkstra maps' article — makes the three canonical techniques (approach, flee, combined hunt map) live and paintable.",
  "description": "PROBE (probe-carts.md): 'the incredible power of Dijkstra maps' (RogueBasin) made playable — is a Dijkstra/flow-field map worth owning, the AI companion to astar's single path? One flood-fill (every floor cell = distance to the goal) drives three behaviours, rendered as a live contour-ring heatmap with downhill flow arrows. APPROACH: agents roll downhill to the goal, routing around walls. FLEE: negate the map and re-flood -> agents flee via real escape routes, not into dead ends. HUNT: add a 'seek gold' map to a 'dread the monster' map -> agents grab loot while dodging a monster that itself hunts them by rolling downhill on a distance-to-agents map. ARROWS drive the goal (or the monster in hunt), L/R-drag paint & erase walls, TAB cycles mode, F toggles flow arrows, R/C/G randomize/clear/rescatter."
}
de:meta */
#include "studio.h"

// DIJKSTRA MAPS
// "The incredible power of Dijkstra maps" (RogueBasin) made playable. A Dijkstra
// map is just a flood-fill: every floor cell holds its distance to the nearest
// GOAL. Once you have that field, intelligence is almost free —
//   * roll DOWNHILL  -> walk the shortest path to the goal, routing around walls
//   * negate it (x -1.2) and re-flood -> flee SMARTLY (toward escape routes, not
//     into dead ends — the trick a naive "move away" can't do)
//   * ADD two maps  -> "seek gold AND avoid the monster" falls out for free
// One primitive, three behaviours. Same occluder grid as sightlines.
//
// TAB / Z : cycle mode      arrows : drive the marked actor      F : flow arrows
// L-drag paint walls   R-drag erase   C clear   R randomize   G rescatter agents

#define TILE 8
#define GW   40
#define GH   22
#define HUD_Y (GH * TILE)
#define INF  1e9f
#define DIAG 1.41421356f
#define BANDW 3.0f
#define NA   8                 // agents
#define HUNT_W 3.0f            // how strongly hunters dread the monster

enum { M_APPROACH, M_FLEE, M_HUNT, M_COUNT };
static const char *MNAME[3] = { "APPROACH", "FLEE", "HUNT" };

static unsigned char wall[GH][GW];
static float field[GH][GW];     // the field agents roll downhill on (also drawn)
static float mfield[GH][GW];    // the monster's field (distance to nearest agent)
static float s1[GH][GW], s2[GH][GW];   // scratch
static float fmin;              // for heatmap normalization

typedef struct { int cx, cy, px, py; float t; } Agent;
static Agent ag[NA];

static int gx, gy;              // goal / scary-point cursor (approach, flee)
static int mx_, my_;           // monster (hunt) — you drive it
static int tx, ty;             // treasure (hunt)
static int mode, score, flow = 1;
static bool dirty = true;
static int  mon_cd;

static const int RAMP[8] = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_RED,
                             CLR_PINK, CLR_DARK_PURPLE, CLR_INDIGO, CLR_DARK_BLUE };

// ---------------------------------------------------------------- grid helpers
static bool blocked(int x, int y) { return x < 0 || x >= GW || y < 0 || y >= GH || wall[y][x]; }
static int  open_cell(int *ox, int *oy) {
    for (int tries = 0; tries < 200; tries++) {
        int x = rnd(GW), y = rnd(GH);
        if (!wall[y][x]) { *ox = x; *oy = y; return 1; }
    }
    return 0;
}

// ---------------------------------------------------------------- Dijkstra core
// The whole technique in two functions: seed the goal cells to 0, then RELAX —
// repeatedly pull every cell down to (cheapest reachable neighbour + step cost)
// until nothing changes. That settles into exact distances. (Diagonals cost ~1.41
// and may not cut a wall corner, so the rings come out round, not square.)
static void clear_field(float f[GH][GW]) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) f[y][x] = INF;
}
static void relax(float f[GH][GW]) {
    bool changed = true; int guard = 0;
    while (changed && guard++ < GW * GH) {
        changed = false;
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                if (wall[y][x]) continue;
                float best = f[y][x];
                // 4 cardinals
                if (!blocked(x-1,y) && f[y][x-1]+1.0f < best) best = f[y][x-1]+1.0f;
                if (!blocked(x+1,y) && f[y][x+1]+1.0f < best) best = f[y][x+1]+1.0f;
                if (!blocked(x,y-1) && f[y-1][x]+1.0f < best) best = f[y-1][x]+1.0f;
                if (!blocked(x,y+1) && f[y+1][x]+1.0f < best) best = f[y+1][x]+1.0f;
                // 4 diagonals (only if both orthogonal cells are open — no corner-cutting)
                if (!blocked(x-1,y-1) && !blocked(x-1,y) && !blocked(x,y-1) && f[y-1][x-1]+DIAG < best) best = f[y-1][x-1]+DIAG;
                if (!blocked(x+1,y-1) && !blocked(x+1,y) && !blocked(x,y-1) && f[y-1][x+1]+DIAG < best) best = f[y-1][x+1]+DIAG;
                if (!blocked(x-1,y+1) && !blocked(x-1,y) && !blocked(x,y+1) && f[y+1][x-1]+DIAG < best) best = f[y+1][x-1]+DIAG;
                if (!blocked(x+1,y+1) && !blocked(x+1,y) && !blocked(x,y+1) && f[y+1][x+1]+DIAG < best) best = f[y+1][x+1]+DIAG;
                if (best < f[y][x]) { f[y][x] = best; changed = true; }
            }
    }
}
// distance field flooded from a single source
static void flood1(float f[GH][GW], int sx, int sy) {
    clear_field(f);
    if (!blocked(sx, sy)) f[sy][sx] = 0;
    relax(f);
}
// turn a distance field into a smart-flee field: negate, then re-flood
static void make_flee(float f[GH][GW]) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (!wall[y][x] && f[y][x] < INF) f[y][x] *= -1.2f;
    relax(f);
}

// ---------------------------------------------------------------- recompute
static void recompute() {
    if (mode == M_APPROACH) {
        flood1(field, gx, gy);
    } else if (mode == M_FLEE) {
        flood1(field, gx, gy);
        make_flee(field);
    } else { // HUNT: agents want gold (s1, attract) but dread the monster (s2, flee)
        flood1(s1, tx, ty);
        flood1(s2, mx_, my_); make_flee(s2);
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
            field[y][x] = (s1[y][x] < INF && s2[y][x] < INF) ? s1[y][x] + HUNT_W * s2[y][x] : INF;
        // the monster's own brain: a Dijkstra map flooded from every agent
        clear_field(mfield);
        for (int i = 0; i < NA; i++) if (!blocked(ag[i].cx, ag[i].cy)) mfield[ag[i].cy][ag[i].cx] = 0;
        relax(mfield);
    }
    fmin = INF;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (!wall[y][x] && field[y][x] < INF && field[y][x] < fmin) fmin = field[y][x];
    if (fmin >= INF) fmin = 0;
    dirty = false;
}

// ---------------------------------------------------------------- step downhill
// pick the open neighbour with the lowest field value; move there if strictly
// lower than where we stand (else stay put — we're at a minimum).
static int step_downhill(float f[GH][GW], int cx, int cy, int *ox, int *oy) {
    float best = f[cy][cx]; int bx = cx, by = cy;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            int nx = cx + dx, ny = cy + dy;
            if (blocked(nx, ny)) continue;
            if (dx && dy && (blocked(cx+dx, cy) || blocked(cx, cy+dy))) continue; // no corner cut
            float v = f[ny][nx];
            if (v < best - 0.0001f) { best = v; bx = nx; by = ny; }
        }
    *ox = bx; *oy = by;
    return (bx != cx || by != cy);
}

// ---------------------------------------------------------------- setup
static void scatter_agents() {
    for (int i = 0; i < NA; i++) {
        open_cell(&ag[i].cx, &ag[i].cy);
        ag[i].px = ag[i].cx; ag[i].py = ag[i].cy; ag[i].t = 1;
    }
    dirty = true;
}
static void randomize_walls() {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        wall[y][x] = (x==0||y==0||x==GW-1||y==GH-1) ? 1 : (rnd(100) < 16);
    wall[gy][gx] = 0;
    scatter_agents(); dirty = true;
}

void init() {
    gx = GW/2; gy = GH/2; mx_ = 4; my_ = 4; tx = GW-5; ty = GH-4;
    // a few rooms so paths have to bend
    for (int x = 8; x < 22; x++) { wall[6][x] = 1; wall[15][x] = 1; }
    for (int y = 6; y <= 15; y++) { wall[y][8] = 1; wall[y][21] = 1; }
    wall[10][8] = 0; wall[11][21] = 0;
    for (int y = 2; y < 19; y++) wall[y][29] = 1;
    wall[9][29] = 0; wall[14][29] = 0;
    for (int x = 0; x < GW; x++) { wall[0][x] = 1; wall[GH-1][x] = 1; }
    for (int y = 0; y < GH; y++) { wall[y][0] = 1; wall[y][GW-1] = 1; }
    scatter_agents();
}

// ---------------------------------------------------------------- update
void update() {
    // mode + toggles
    if (btnp(0, BTN_A) || keyp(KEY_TAB)) { mode = (mode+1) % M_COUNT; score = 0; dirty = true; }
    if (keyp('F')) flow = !flow;
    if (keyp('G')) scatter_agents();
    if (keyp('C')) { for (int y=1;y<GH-1;y++) for (int x=1;x<GW-1;x++) wall[y][x]=0; dirty=true; }
    if (keyp('R')) randomize_walls();

    // drive the marked actor: goal (approach/flee) or monster (hunt)
    static int mcd;
    int dx = 0, dy = 0;
    if (btn(0, BTN_LEFT)) dx=-1; else if (btn(0, BTN_RIGHT)) dx=1;
    if (btn(0, BTN_UP))   dy=-1; else if (btn(0, BTN_DOWN))  dy=1;
    if ((dx||dy) && mcd<=0) {
        if (mode == M_HUNT) {
            int nx=clamp(mx_+dx,0,GW-1), ny=clamp(my_+dy,0,GH-1);
            if (!wall[ny][nx]) { mx_=nx; my_=ny; dirty=true; }
        } else {
            int nx=clamp(gx+dx,0,GW-1), ny=clamp(gy+dy,0,GH-1);
            gx=nx; gy=ny; wall[gy][gx]=0; dirty=true;
        }
        mcd = 5;
    }
    if (!(dx||dy)) mcd = 0; else if (mcd>0) mcd--;

    // paint / erase walls
    int cxp = mouse_x()/TILE, cyp = mouse_y()/TILE;
    if (mouse_y() < HUD_Y && cxp>0 && cxp<GW-1 && cyp>0 && cyp<GH-1) {
        if (mouse_down(0)) { wall[cyp][cxp]=1; dirty=true; }
        if (mouse_down(1)) { wall[cyp][cxp]=0; dirty=true; }
    }

    if (dirty) recompute();

    // advance the smooth-move timer; tick agents to their next cell
    for (int i = 0; i < NA; i++) {
        Agent *a = &ag[i];
        a->t += 0.16f;
        if (a->t >= 1.0f) {
            a->t = 0;
            a->px = a->cx; a->py = a->cy;
            int nx, ny;
            if (step_downhill(field, a->cx, a->cy, &nx, &ny)) { a->cx = nx; a->cy = ny; }
            // reached the prize?
            if (mode == M_APPROACH && a->cx==gx && a->cy==gy) { open_cell(&a->cx,&a->cy); a->px=a->cx; a->py=a->cy; note(72,INSTR_SINE,1); }
            if (mode == M_HUNT && a->cx==tx && a->cy==ty) { score++; open_cell(&tx,&ty); dirty=true; note(79,INSTR_SQUARE,2); }
        }
    }
    // monster hunts the nearest agent (rolls downhill on mfield) when you're idle
    if (mode == M_HUNT) {
        if (mon_cd > 0) mon_cd--;
        if (!(dx||dy) && mon_cd<=0) {
            int nx, ny;
            if (step_downhill(mfield, mx_, my_, &nx, &ny)) { mx_=nx; my_=ny; dirty=true; }
            mon_cd = 10;
        }
    }

#ifdef DE_TRACE
    int total = 0; for (int i = 0; i < NA; i++) total += abs(ag[i].cx-gx) + abs(ag[i].cy-gy);
    watch("mode", "%d", mode); watch("fmin", "%d", (int)fmin); watch("score", "%d", score);
    watch("agentdist", "%d", total / NA);   // mean manhattan dist agents->goal
#endif
}

// ---------------------------------------------------------------- draw
static int agent_color(void) { return mode==M_FLEE ? CLR_GREEN : mode==M_HUNT ? CLR_BLUE : CLR_WHITE; }

static void draw_actor(int cx, int cy, char glyph, int col, int ring) {
    int sx = cx*TILE+TILE/2, sy = cy*TILE+TILE/2;
    if (ring) circ(sx, sy, 4 + (blink(20)?1:0), col);
    print(str("%c", glyph), cx*TILE+1, cy*TILE, col);
}

void draw() {
    cls(CLR_BLACK);

    // heatmap: colour each floor cell by its distance band → contour rings
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = x*TILE, sy = y*TILE;
            if (wall[y][x]) { rectfill(sx, sy, TILE, TILE, CLR_DARK_GREY); continue; }
            if (field[y][x] >= INF) { pset(sx+TILE/2, sy+TILE/2, CLR_DARKER_GREY); continue; }
            int band = (int)((field[y][x]-fmin)/BANDW);
            if (band < 0) band = 0;
            rectfill(sx+1, sy+1, TILE-2, TILE-2, RAMP[band % 8]);
        }

    // flow field: a short arrow in each cell pointing downhill (the "roll" made visible)
    if (flow)
        for (int y = 1; y < GH-1; y += 2)
            for (int x = 1; x < GW-1; x += 2) {
                if (wall[y][x] || field[y][x] >= INF) continue;
                int nx, ny;
                if (!step_downhill(field, x, y, &nx, &ny)) continue;
                int cx = x*TILE+TILE/2, cy = y*TILE+TILE/2;
                int ex = cx + (nx-x)*3, ey = cy + (ny-y)*3;
                line(cx, cy, ex, ey, CLR_BLACK);
                pset(ex, ey, CLR_WHITE);
            }

    // agents
    for (int i = 0; i < NA; i++) {
        Agent *a = &ag[i];
        float fx = lerp(a->px, a->cx, a->t), fy = lerp(a->py, a->cy, a->t);
        int sx = (int)(fx*TILE)+TILE/2, sy = (int)(fy*TILE)+TILE/2;
        circfill(sx, sy, 2, CLR_BLACK);
        circfill(sx, sy, 1, agent_color());
    }

    // the marked actors per mode
    if (mode == M_HUNT) {
        draw_actor(tx, ty, '$', blink(12)?CLR_YELLOW:CLR_ORANGE, 0);
        draw_actor(mx_, my_, '&', CLR_RED, 1);
    } else {
        draw_actor(gx, gy, '+', mode==M_FLEE?CLR_RED:CLR_GREEN, 1);
    }

    // HUD
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H-HUD_Y, CLR_DARKER_BLUE);
    print(MNAME[mode], 4, HUD_Y+2, CLR_WHITE);
    const char *blurb = mode==M_APPROACH ? "agents roll downhill to the goal"
                      : mode==M_FLEE     ? "negate the map -> flee via escape routes"
                                         : "seek gold + dread monster = one summed map";
    print(blurb, 64, HUD_Y+2, CLR_LIGHT_PEACH);
    if (mode == M_HUNT) print_right(str("gold %d", score), SCREEN_W-4, HUD_Y+2, CLR_YELLOW);

    font(FONT_SMALL);
    const char *drive = mode==M_HUNT ? "arrows: drive monster" : "arrows: move goal";
    print(str("%s   L/R-drag: walls   TAB: mode   F: arrows   R: rnd   C: clear   G: rescatter", drive),
          4, HUD_Y+13, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
