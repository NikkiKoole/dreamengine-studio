/* de:meta
{
  "title": "beltworks",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "factory-automation",
    "state-machine"
  ],
  "lineage": "The belt layer of navkit's mechanisms experiment (one-item-per-cell conveyors with backpressure) made into a Factorio-lite: miners + a five-machine crafting tree (smelter/assembler/charger/fabricator + hub) plumbed with belts, on the logic cart's editor + cursor.h. Sibling of the `logic` signal sandbox and `runecrawl` (the signal layer); this is the transport/production layer.",
  "genre": "simulation",
  "homage": "Factorio (2020)",
  "description": "A factory-builder in miniature, after Factorio. Three raw ores sit in the ground — ORE, COAL, CRYSTAL — and five machines refine them up a tree you plumb together with belts: a SMELTER turns ore+coal into PLATES, an ASSEMBLER presses 2 plates into a GEAR, a CHARGER turns crystal+coal into a CELL, and a FABRICATOR combines a gear+cell into an ENGINE. Belts carry one item per cell and BACK UP when blocked, so throughput is all about the layout — and coal feeds two machines, so you end up splitting that line. Miners only work placed ON a deposit. Deliver engines to the HUB to hit the quota (25), then keep optimizing. Opens with a complete demo factory already running. Real-time — you build while it runs. Mouse: pick a tool, left-click/drag to lay belts and place machines, right-click erases, R rotates; SPACE pause, [ ] speed."
}
de:meta */
#include "studio.h"
#include "cursor.h"
#include <stdio.h>

// BELTWORKS
// A factory-builder in miniature, after Factorio. Three raw ores in the ground —
// ORE, COAL, CRYSTAL — and five machines you plumb together with belts to refine
// them up a tree: smelt ORE+COAL into PLATES, press PLATES into GEARS, charge
// CRYSTAL+COAL into CELLS, and fabricate GEARS+CELLS into ENGINES. Belts carry
// items one cell at a time and BACK UP when blocked, so throughput is all about
// the layout — and COAL feeds two machines, so you'll be splitting that line.
// Deliver engines to the HUB to hit the quota, then keep optimizing. Real-time:
// you build while it runs.
//
// Mouse: click a tool in the palette, left-click/drag to place (belts paint),
//   right-click erases. R rotates what you place (and the tile under the cursor).
//   SPACE pause, [ ] speed.  A miner only works placed ON a deposit.

#define CELL    8
#define GW      35
#define GH      22
#define PANEL_X (GW * CELL)
#define PANEL_W (SCREEN_W - PANEL_X)
#define STRIP_Y (GH * CELL)
#define BUFCAP  4          // machine input buffer cap (backpressure)
#define QUOTA   25

enum { I_NONE, I_ORE, I_COAL, I_CRYSTAL, I_PLATE, I_GEAR, I_CELL, I_ENGINE, I_COUNT };
enum { D_NONE, D_BELT, D_MINER, D_SMELTER, D_ASSEMBLER, D_CHARGER, D_FABRICATOR, D_HUB, D_COUNT };
enum { DN, DE, DS, DW };

// item appearance
static const int  ICOL[I_COUNT] = { 0, CLR_ORANGE, CLR_DARK_GREY, CLR_BLUE, CLR_LIGHT_GREY, CLR_YELLOW, CLR_GREEN, CLR_PINK };
static const char *INAME[I_COUNT] = { "", "ore", "coal", "crystal", "plate", "gear", "cell", "engine" };

// recipe per machine type (inA qA inB qB -> out, ticks). MINER/HUB are special.
typedef struct { int inA, qA, inB, qB, out, ticks; } Recipe;
static const Recipe RCP[D_COUNT] = {
    [D_SMELTER]    = { I_ORE, 1, I_COAL, 1, I_PLATE, 20 },
    [D_ASSEMBLER]  = { I_PLATE, 2, 0, 0, I_GEAR, 24 },
    [D_CHARGER]    = { I_CRYSTAL, 1, I_COAL, 1, I_CELL, 20 },
    [D_FABRICATOR] = { I_GEAR, 1, I_CELL, 1, I_ENGINE, 30 },
};
static int is_machine(int d) { return d >= D_SMELTER && d <= D_FABRICATOR; }

// palette / tool metadata
typedef struct { const char *name; int key; const char *keyl; int col; } ToolMeta;
static const ToolMeta TOOL[D_COUNT] = {
    [D_NONE]       = { "Eraser",  '0', "0", CLR_DARKER_GREY },
    [D_BELT]       = { "Belt",    '1', "1", CLR_MEDIUM_GREY },
    [D_MINER]      = { "Miner",   '2', "2", CLR_BROWN },
    [D_SMELTER]    = { "Smelter", '3', "3", CLR_DARK_RED },
    [D_ASSEMBLER]  = { "Assembler",'4', "4", CLR_TRUE_BLUE },
    [D_CHARGER]    = { "Charger", '5', "5", CLR_DARK_GREEN },
    [D_FABRICATOR] = { "Fabricator",'6',"6", CLR_DARK_PURPLE },
    [D_HUB]        = { "Hub",     '7', "7", CLR_DARK_ORANGE },
};

static unsigned char dep[GH][GW];     // deposit yield (I_ORE/I_COAL/I_CRYSTAL, 0=none)
static unsigned char dev[GH][GW], face[GH][GW];
static unsigned char cargo[GH][GW];   // item on a belt cell
static unsigned char in0[GH][GW], in1[GH][GW], outp[GH][GW];   // machine buffers
static short mt[GH][GW];               // machine/miner timer
static int delivered, running = 1, speed = 3, frameAcc;
static int brush = D_BELT, curx = GW/2, cury = GH/2, placeFacing = DE;
static int pmx = -1, pmy = -1;

static int in_grid(int x, int y) { return x >= 0 && x < GW && y >= 0 && y < GH; }
static void doff(int d, int *dx, int *dy) { *dx = 0; *dy = 0; if (d==DN)*dy=-1; else if (d==DE)*dx=1; else if (d==DS)*dy=1; else *dx=-1; }

// ---------------------------------------------------------------------------
// simulation — one factory step
// ---------------------------------------------------------------------------
#define MINE_TICKS 18
static void take_into(int mx, int my) {   // a machine at (mx,my) pulls needed items off in-pointing belts
    const Recipe *r = &RCP[dev[my][mx]];
    for (int d = 0; d < 4; d++) {
        int dx, dy; doff(d, &dx, &dy); int bx = mx - dx, by = my - dy;   // belt that must FACE the machine
        if (!in_grid(bx, by) || dev[by][bx] != D_BELT || !cargo[by][bx]) continue;
        if (face[by][bx] != d) continue;                                 // belt must point toward the machine
        int it = cargo[by][bx];
        if (it == r->inA && in0[my][mx] < BUFCAP) { in0[my][mx]++; cargo[by][bx] = 0; }
        else if (r->inB && it == r->inB && in1[my][mx] < BUFCAP) { in1[my][mx]++; cargo[by][bx] = 0; }
    }
}
static void push_out(int mx, int my) {    // a machine/miner pushes its output onto the belt it faces
    if (!outp[my][mx]) return;
    int dx, dy; doff(face[my][mx], &dx, &dy); int nx = mx + dx, ny = my + dy;
    if (in_grid(nx, ny) && dev[ny][nx] == D_BELT && !cargo[ny][nx]) { cargo[ny][nx] = outp[my][mx]; outp[my][mx] = 0; }
}

static void sim_step(void) {
    // 1. miners dig
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dev[y][x] != D_MINER || !dep[y][x]) continue;
        if (mt[y][x] > 0) mt[y][x]--;
        if (mt[y][x] <= 0 && !outp[y][x]) { outp[y][x] = dep[y][x]; mt[y][x] = MINE_TICKS; }
    }
    // 2. machines pull inputs
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) if (is_machine(dev[y][x])) take_into(x, y);
    // 3. machines craft
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (!is_machine(dev[y][x])) continue;
        const Recipe *r = &RCP[dev[y][x]];
        if (mt[y][x] == 0 && !outp[y][x] && in0[y][x] >= r->qA && (!r->inB || in1[y][x] >= r->qB)) {
            in0[y][x] -= r->qA; if (r->inB) in1[y][x] -= r->qB; mt[y][x] = r->ticks;   // begin
        }
        if (mt[y][x] > 0) { mt[y][x]--; if (mt[y][x] == 0) outp[y][x] = r->out; }       // finish
    }
    // 4. machines + miners push outputs onto belts
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (dev[y][x] == D_MINER || is_machine(dev[y][x])) push_out(x, y);
    // 5. belts advance one cell (snapshot so each item moves at most once)
    unsigned char old[GH][GW];
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) old[y][x] = cargo[y][x];
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dev[y][x] != D_BELT || !old[y][x]) continue;
        int dx, dy; doff(face[y][x], &dx, &dy); int nx = x + dx, ny = y + dy;
        if (in_grid(nx, ny) && dev[ny][nx] == D_BELT && !cargo[ny][nx]) { cargo[ny][nx] = old[y][x]; cargo[y][x] = 0; }
    }
    // 6. hub collects engines off in-pointing belts
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dev[y][x] != D_HUB) continue;
        for (int d = 0; d < 4; d++) {
            int dx, dy; doff(d, &dx, &dy); int bx = x - dx, by = y - dy;
            if (in_grid(bx, by) && dev[by][bx] == D_BELT && face[by][bx] == d && cargo[by][bx] == I_ENGINE) { cargo[by][bx] = 0; delivered++; }
        }
    }
}

// ---------------------------------------------------------------------------
// editing
// ---------------------------------------------------------------------------
static void clear_cell(int x, int y) {
    dev[y][x] = D_NONE; face[y][x] = 0; cargo[y][x] = 0;
    in0[y][x] = in1[y][x] = outp[y][x] = 0; mt[y][x] = 0;
}
static void place(int x, int y, int type, int f) {
    if (!in_grid(x, y)) return;
    clear_cell(x, y);
    if (type == D_NONE) return;
    dev[y][x] = type; face[y][x] = f;
}

// ---------------------------------------------------------------------------
// demo factory: ore + coal -> smelter -> plate -> ... (built running on load)
// ---------------------------------------------------------------------------
static void belt_run(int x, int y, int f, int n) { for (int i = 0; i < n; i++) { int dx,dy; doff(f,&dx,&dy); place(x,y,D_BELT,f); x+=dx; y+=dy; } }

void init(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { dep[y][x]=0; clear_cell(x,y); }
    delivered = 0;

    // deposits
    for (int y=2;y<=3;y++)  for (int x=2;x<=4;x++) dep[y][x] = I_ORE;
    for (int y=5;y<=7;y++)  for (int x=2;x<=4;x++) dep[y][x] = I_COAL;
    for (int y=8;y<=10;y++) for (int x=2;x<=4;x++) dep[y][x] = I_CRYSTAL;

    // a complete demo line: ORE+COAL->plate->gear, CRYSTAL+COAL->cell, gear+cell->ENGINE->hub.
    // -- smelter branch --
    place(3,3,D_MINER,DE); belt_run(4,3,DE,1); place(5,3,D_SMELTER,DE);   // ore -> smelter (ore from W)
    place(3,5,D_MINER,DE); belt_run(4,5,DE,1);                            // coal miner #1 -> E
    place(5,5,D_BELT,DN); place(5,4,D_BELT,DN);                           // coal up into smelter from S
    belt_run(6,3,DE,2); place(8,3,D_ASSEMBLER,DE);                        // plate -> assembler (2 plate -> gear)
    place(9,3,D_BELT,DE); place(10,3,D_BELT,DS); place(10,4,D_BELT,DS); place(10,5,D_BELT,DS);  // gear down to fabricator (from N)
    // -- charger branch --
    place(3,9,D_MINER,DE); belt_run(4,9,DE,1); place(5,9,D_CHARGER,DE);   // crystal -> charger (from W)
    place(3,6,D_MINER,DS); place(3,7,D_BELT,DS); place(3,8,D_BELT,DE); place(4,8,D_BELT,DE); place(5,8,D_BELT,DS); // coal #2 into charger from N
    belt_run(6,9,DE,4); place(10,9,D_BELT,DN); place(10,8,D_BELT,DN); place(10,7,D_BELT,DN);    // cell up to fabricator (from S)
    // -- assembly --
    place(10,6,D_FABRICATOR,DE);                                         // gear from N, cell from S, engine out E
    belt_run(11,6,DE,3); place(14,6,D_HUB,DN);
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
static void draw_arrow(int X, int Y, int f, int col) {
    int cx = X+4, cy = Y+4, dx, dy; doff(f, &dx, &dy);
    line(cx - dx*2, cy - dy*2, cx + dx*2, cy + dy*2, col);
    pset(cx + dx*2, cy + dy*2, col);
}
static void draw_item(int X, int Y, int it) { if (it) circfill(X+4, Y+4, 2, ICOL[it]); }

static void draw_cell(int x, int y) {
    int X = x*CELL, Y = y*CELL;
    // ground / deposit
    int dp = dep[y][x];
    int bg = dp==I_ORE ? CLR_DARK_BROWN : dp==I_COAL ? CLR_DARKER_GREY : dp==I_CRYSTAL ? CLR_BLUE_GREEN : CLR_BROWNISH_BLACK;
    rectfill(X, Y, CELL, CELL, bg);
    if (dp) { pset(X+2,Y+2, ICOL[dp]); pset(X+5,Y+5, ICOL[dp]); pset(X+5,Y+2, ICOL[dp]); }

    switch (dev[y][x]) {
        case D_BELT:
            draw_arrow(X, Y, face[y][x], CLR_DARK_GREY);
            draw_item(X, Y, cargo[y][x]);
            break;
        case D_MINER:
            rectfill(X+1, Y+1, 6, 6, CLR_BROWN);
            draw_arrow(X, Y, face[y][x], CLR_LIGHT_PEACH);
            break;
        case D_HUB:
            rectfill(X, Y, CELL, CELL, CLR_DARK_ORANGE); rect(X, Y, CELL, CELL, CLR_ORANGE);
            break;
        default:
            if (is_machine(dev[y][x])) {
                int t = dev[y][x];
                rectfill(X+1, Y+1, 6, 6, TOOL[t].col);
                rect(X+1, Y+1, 6, 6, mt[y][x] > 0 ? CLR_WHITE : CLR_DARKER_GREY);   // flash white while working
                char g[2] = { (char)(INAME[RCP[t].out][0] - 32), 0 };               // first letter of product, upper
                font(FONT_SMALL); print(g, X+2, Y+1, CLR_WHITE);
                draw_arrow(X, Y, face[y][x], CLR_LIGHT_PEACH);
                draw_item(X, Y, outp[y][x]);
            }
            break;
    }
}

static void draw_palette(void) {
    rectfill(PANEL_X, 0, PANEL_W, SCREEN_H, CLR_BROWNISH_BLACK);
    line(PANEL_X, 0, PANEL_X, SCREEN_H-1, CLR_DARKER_GREY);
    int rh = 13, mx = mouse_x(), my = mouse_y();
    int hov = (mx >= PANEL_X && my >= 2) ? (my-2)/rh : -1;
    for (int t = 0; t < D_COUNT; t++) {
        int ry = 2 + t*rh;
        if (t == brush) rectfill(PANEL_X+1, ry-1, PANEL_W-2, rh, CLR_DARK_BLUE);
        else if (t == hov) rectfill(PANEL_X+1, ry-1, PANEL_W-2, rh, CLR_DARKER_GREY);
        if (t == D_NONE) rect(PANEL_X+3, ry+1, 6, 6, CLR_DARKER_GREY);
        else rectfill(PANEL_X+3, ry+1, 6, 6, TOOL[t].col);
        font(FONT_SMALL);
        print(TOOL[t].name, PANEL_X+12, ry+1, t==brush ? CLR_WHITE : CLR_LIGHT_GREY);
        print(TOOL[t].keyl, PANEL_X+PANEL_W-7, ry+1, CLR_DARK_ORANGE);
    }
}

static void draw_strip(void) {
    rectfill(0, STRIP_Y, PANEL_X, SCREEN_H-STRIP_Y, CLR_BLACK);
    line(0, STRIP_Y, PANEL_X-1, STRIP_Y, CLR_DARKER_GREY);
    font(FONT_SMALL);
    char l[48];
    snprintf(l, sizeof l, "HUB  %d / %d engines%s", delivered, QUOTA, delivered >= QUOTA ? "   FACTORY COMPLETE!" : "");
    print(l, 4, STRIP_Y+3, delivered >= QUOTA ? CLR_LIME_GREEN : CLR_LIGHT_GREY);
    snprintf(l, sizeof l, "%s   %s x%d   SPACE run   R turn   right-click erase", TOOL[brush].name, running?"RUN":"PAUSE", speed);
    print(l, 4, STRIP_Y+13, running ? CLR_GREEN : CLR_ORANGE);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) draw_cell(x, y);
    if (in_grid(curx, cury)) {
        rect(curx*CELL, cury*CELL, CELL, CELL, CLR_WHITE);
        if (brush != D_NONE) draw_arrow(curx*CELL, cury*CELL, placeFacing, CLR_LIGHT_GREY);
    }
    draw_palette();
    draw_strip();
    cursor_draw(mouse_x() >= PANEL_X ? CUR_ARROW : CUR_CROSS);
}

// ---------------------------------------------------------------------------
// input + loop
// ---------------------------------------------------------------------------
void update(void) {
    int mx = mouse_x(), my = mouse_y();
    if (mx != pmx || my != pmy) { pmx = mx; pmy = my; }

    for (int t = 0; t < D_COUNT; t++) if (keyp(TOOL[t].key)) brush = t;
    if (keyp('R')) {
        placeFacing = (placeFacing+1)&3;
        if (in_grid(curx,cury) && (dev[cury][curx]==D_BELT||dev[cury][curx]==D_MINER||is_machine(dev[cury][curx]))) face[cury][curx]=(face[cury][curx]+1)&3;
    }
    if (keyp(KEY_SPACE)) running = !running;
    if (keyp('[')) { speed++; if (speed>20) speed=20; }
    if (keyp(']')) { speed--; if (speed<1) speed=1; }

    if (mx >= PANEL_X) {                                  // palette click
        if (mouse_pressed(0)) { int t=(my-2)/13; if (t>=0 && t<D_COUNT) brush=t; }
    } else if (my < STRIP_Y) {                            // build area
        int gx = mx/CELL, gy = my/CELL; curx = gx; cury = gy;
        if (mouse_down(0)) place(gx, gy, brush, placeFacing);     // click or drag paints
        if (mouse_pressed(1)) clear_cell(gx, gy);
    }

    // keyboard cursor as a fallback
    if (keyp(KEY_LEFT)) curx--; if (keyp(KEY_RIGHT)) curx++;
    if (keyp(KEY_UP)) cury--; if (keyp(KEY_DOWN)) cury++;
    if (curx<0) curx=0; if (curx>=GW) curx=GW-1; if (cury<0) cury=0; if (cury>=GH) cury=GH-1;
    if (keyp('Z')) place(curx, cury, brush, placeFacing);
    if (keyp('X')) clear_cell(curx, cury);

    if (running && ++frameAcc >= speed) { frameAcc = 0; sim_step(); }

#ifdef DE_TRACE
    watch("delivered", "%d", delivered);
#endif
}

// ---------------------------------------------------------------------------
// spec() — headless correctness gate (node tools/spec.js). Pins each machine's
// recipe (right inputs -> product; insufficient -> nothing) and belt transport
// (advance one cell/step + backpressure when the line is full).
// ---------------------------------------------------------------------------
#ifdef DE_SPEC
#include "spec.h"

static void sp_reset(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { dep[y][x] = 0; clear_cell(x, y); }
    delivered = 0;
}
// place a machine, prime its buffers to `have` x inputs, run, return its product slot.
static int sp_craft(int type, int have) {
    sp_reset();
    place(10, 10, type, DE);
    in0[10][10] = have * RCP[type].qA;
    if (RCP[type].inB) in1[10][10] = have * RCP[type].qB;
    for (int i = 0; i < RCP[type].ticks + 3; i++) sim_step();
    return outp[10][10];
}

void spec(void) {
    expect_eq(sp_craft(D_SMELTER, 1),    I_PLATE,  "smelter: ore+coal -> plate");
    expect_eq(sp_craft(D_ASSEMBLER, 1),  I_GEAR,   "assembler: 2 plate -> gear");
    expect_eq(sp_craft(D_CHARGER, 1),    I_CELL,   "charger: crystal+coal -> cell");
    expect_eq(sp_craft(D_FABRICATOR, 1), I_ENGINE, "fabricator: gear+cell -> engine");
    expect_eq(sp_craft(D_SMELTER, 0),    I_NONE,   "smelter idle without inputs");
    expect_eq(sp_craft(D_FABRICATOR, 0), I_NONE,   "fabricator idle without inputs");

    // belt transport: an item advances one cell per step into an empty belt ahead
    sp_reset();
    place(10, 10, D_BELT, DE); place(11, 10, D_BELT, DE); place(12, 10, D_BELT, DE); place(13, 10, D_BELT, DE);
    cargo[10][10] = I_ORE;
    sim_step();
    expect_eq(cargo[10][11], I_ORE, "belt carries the item one cell forward");
    expect_eq(cargo[10][10], I_NONE, "...and clears the cell behind it");

    // backpressure: a full line only advances at the FRONT
    sp_reset();
    place(10, 10, D_BELT, DE); place(11, 10, D_BELT, DE); place(12, 10, D_BELT, DE); place(13, 10, D_BELT, DE);
    cargo[10][10] = I_ORE; cargo[10][11] = I_ORE; cargo[10][12] = I_ORE;   // (13,10) is the only empty
    sim_step();
    expect_eq(cargo[10][13], I_ORE, "backpressure: only the front item advances");
    expect_eq(cargo[10][10], I_ORE, "backpressure: a blocked item stays put");
}
#endif
