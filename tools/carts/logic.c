#include "studio.h"
#include <stdio.h>

// LOGIC
// A redstone-lite logic sandbox. Place switches, wires and gates on a grid and
// watch signal flow, tick by tick. Build an oscillator from a NOT gate, an SR
// latch from two NORs, a half-adder from XOR+AND. The signal layer ported from
// navkit's "mechanisms" experiment — the first of a planned family (pipes,
// belts, gears), and the engine that will later drive a circuit-puzzle dungeon.
//
// Mouse: click palette to pick a tool, left-click grid to place, right-click to
//   erase. Pick the HAND to toggle switches / hold buttons / cycle a clock.
// Keyboard: arrows move the cursor, Z place, X erase, C use, R rotate a gate.
//   Number/letter keys pick tools (see palette). SPACE run/pause, . step.

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define CELL    8
#define GW      30                 // grid columns  (0..240 px)
#define GH      22                 // grid rows     (0..176 px)
#define PANEL_X (GW * CELL)        // palette panel left edge = 240
#define PANEL_W (SCREEN_W - PANEL_X)
#define STRIP_Y (GH * CELL)        // status strip top = 176

// ---------------------------------------------------------------------------
// Components (the digital signal layer)
// ---------------------------------------------------------------------------
enum {
    CT_EMPTY = 0,   // eraser
    CT_HAND,        // interact tool (cart-only, not a placeable cell)
    CT_SWITCH,
    CT_BUTTON,
    CT_LIGHT,
    CT_WIRE,
    CT_NOT,
    CT_AND,
    CT_OR,
    CT_XOR,
    CT_NOR,
    CT_LATCH,
    CT_CLOCK,
    CT_REPEATER,
    CT_PULSE,
    CT_COUNT
};

enum { DIR_N = 0, DIR_E, DIR_S, DIR_W };

// draw styles
enum { DS_NONE = 0, DS_TOOL, DS_LABEL, DS_CIRCLE, DS_GATE, DS_WIRE, DS_CLOCK };

typedef struct {
    const char *name;
    const char *label;   // 1-char glyph drawn in-cell / palette
    int   col, acol;     // base (off) and active (on) palette colors
    int   key;           // keyboard shortcut (char or KEY_*)
    const char *keyl;    // key label for palette
    int   draw;          // DS_*
    int   dir;           // directional? (needs a facing arrow)
    int   isCell;        // can it live in the grid?
} Meta;

static const Meta META[CT_COUNT] = {
//   name        label col              acol            key   keyl draw       dir cell
    {"Eraser",   "",  CLR_DARKER_GREY, CLR_BLACK,       '0', "0", DS_NONE,   0, 0},
    {"Hand",     "",  CLR_LIGHT_GREY,  CLR_WHITE,       'C', "C", DS_TOOL,   0, 0},
    {"Switch",   "S", CLR_BROWN,       CLR_YELLOW,      '1', "1", DS_LABEL,  0, 1},
    {"Button",   "B", CLR_DARK_RED,    CLR_PEACH,       '2', "2", DS_LABEL,  0, 1},
    {"Light",    "",  CLR_DARK_GREEN,  CLR_GREEN,       '3', "3", DS_CIRCLE, 0, 1},
    {"Wire",     "",  CLR_DARK_GREY,   CLR_YELLOW,      '4', "4", DS_WIRE,   0, 1},
    {"NOT",      "!", CLR_RED,         CLR_PEACH,       '5', "5", DS_GATE,   1, 1},
    {"AND",      "&", CLR_TRUE_BLUE,   CLR_BLUE,        '6', "6", DS_GATE,   1, 1},
    {"OR",       "|", CLR_DARK_GREEN,  CLR_GREEN,       '7', "7", DS_GATE,   1, 1},
    {"XOR",      "^", CLR_DARK_PURPLE, CLR_PINK,        '8', "8", DS_GATE,   1, 1},
    {"NOR",      "v", CLR_BROWN,       CLR_ORANGE,      '9', "9", DS_GATE,   1, 1},
    {"Latch",    "M", CLR_DARK_ORANGE, CLR_YELLOW,      'Q', "Q", DS_GATE,   1, 1},
    {"Clock",    "",  CLR_BROWN,       CLR_ORANGE,      'W', "W", DS_CLOCK,  0, 1},
    {"Repeat",   "r", CLR_BLUE_GREEN,  CLR_BLUE,        'E', "E", DS_GATE,   1, 1},
    {"Pulse",    "p", CLR_MAUVE,       CLR_PINK,        'A', "A", DS_GATE,   1, 1},
};

// ---------------------------------------------------------------------------
// Cell + simulation state
// ---------------------------------------------------------------------------
typedef struct {
    int  type;
    int  facing;
    int  state;        // on/off (switch, button, clock, latch, gate output cache)
    int  signalIn;
    int  signalOut;
    int  setting;      // clock period / repeater delay / pulse length
    int  timer;
    int  delayBuf[4];
} Cell;

static Cell grid[GH][GW];

// double-buffered wire signal field (1-tick latency on gate inputs — exactly
// what makes a NOT-gate-into-itself oscillate and an SR latch hold)
static int sig[2][GH][GW];
static int sread = 0;

// scratch BFS queue for the wire flood-fill
static int qx[GW * GH], qy[GW * GH];
static int seedv[GH][GW];

// ---------------------------------------------------------------------------
// Editor state
// ---------------------------------------------------------------------------
static int  brush = CT_WIRE;
static int  curx = GW / 2, cury = GH / 2;   // cell cursor
static int  placeFacing = DIR_E;
static int  running = 1;
static int  speed = 6;          // frames per sim tick
static int  frameAcc = 0;
static int  stepReq = 0;        // single-step request when paused
static int  usingMouse = 1;
static int  pmx = -1, pmy = -1; // previous mouse pos (to detect movement)
static int  btnCellX = -1, btnCellY = -1;   // cell holding a momentary button press

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int in_grid(int x, int y) { return x >= 0 && x < GW && y >= 0 && y < GH; }

static void dir_offset(int d, int *dx, int *dy) {
    *dx = 0; *dy = 0;
    if (d == DIR_N) *dy = -1;
    else if (d == DIR_E) *dx = 1;
    else if (d == DIR_S) *dy = 1;
    else if (d == DIR_W) *dx = -1;
}

static int opposite_dir(int d) { return (d + 2) & 3; }

static void gate_input_dirs(int facing, int *inA, int *inB) {
    *inA = (facing + 1) & 3;   // the two sides perpendicular to the output
    *inB = (facing + 3) & 3;
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------
static void place_cell(int gx, int gy, int type, int dir) {
    if (!in_grid(gx, gy)) return;
    Cell *c = &grid[gy][gx];
    *c = (Cell){0};
    if (type == CT_EMPTY) return;
    c->type = type;
    c->facing = dir;
    if (type == CT_CLOCK)    { c->setting = 4; c->timer = 4; }
    if (type == CT_REPEATER) { c->setting = 1; }
    if (type == CT_PULSE)    { c->setting = 5; }
}

static void erase_cell(int gx, int gy) {
    if (!in_grid(gx, gy)) return;
    grid[gy][gx] = (Cell){0};
}

// the HAND tool: toggle / hold / cycle the cell under the cursor
static void interact_cell(int gx, int gy, int pressed) {
    if (!in_grid(gx, gy)) return;
    Cell *c = &grid[gy][gx];
    switch (c->type) {
        case CT_SWITCH:   if (pressed) c->state = !c->state; break;
        case CT_BUTTON:   c->state = 1; btnCellX = gx; btnCellY = gy; break; // held; cleared each tick
        case CT_CLOCK:    if (pressed) { c->setting = c->setting % 8 + 1; } break;
        case CT_REPEATER: if (pressed) { c->setting = c->setting % 4 + 1; } break;
        case CT_PULSE:    if (pressed) { c->setting = c->setting % 8 + 1; } break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Simulation: signal propagation  (ported from navkit MechUpdateSignals)
// ---------------------------------------------------------------------------
static void seed_wire(int newSig[GH][GW], int nx, int ny, int value, int *qTail) {
    if (!in_grid(nx, ny) || grid[ny][nx].type != CT_WIRE) return;
    if (newSig[ny][nx] >= value) return;
    newSig[ny][nx] = value;
    seedv[ny][nx] = value;
    qx[*qTail] = nx; qy[*qTail] = ny; (*qTail)++;
}

static void seed_adjacent(int newSig[GH][GW], int x, int y, int value, int *qTail) {
    for (int d = 0; d < 4; d++) {
        int dx, dy; dir_offset(d, &dx, &dy);
        seed_wire(newSig, x + dx, y + dy, value, qTail);
    }
}

static void seed_facing(int newSig[GH][GW], int x, int y, int facing, int value, int *qTail) {
    int dx, dy; dir_offset(facing, &dx, &dy);
    seed_wire(newSig, x + dx, y + dy, value, qTail);
}

static int read_sig(int x, int y) {
    if (!in_grid(x, y)) return 0;
    return sig[sread][y][x];
}

static void sim_tick(void) {
    int newSig[GH][GW];
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { newSig[y][x] = 0; seedv[y][x] = 0; }

    int qHead = 0, qTail = 0;

    // 1. sources + gates seed the wire field
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            Cell *c = &grid[y][x];
            switch (c->type) {
                case CT_SWITCH:
                case CT_BUTTON: {
                    c->signalOut = c->state ? 1 : 0;
                    if (c->signalOut) seed_adjacent(newSig, x, y, 1, &qTail);
                    break;
                }
                case CT_CLOCK: {
                    c->timer--;
                    if (c->timer <= 0) { c->state = !c->state; c->timer = c->setting; }
                    c->signalOut = c->state ? 1 : 0;
                    if (c->signalOut) seed_adjacent(newSig, x, y, 1, &qTail);
                    break;
                }
                case CT_REPEATER: {
                    int dx, dy; dir_offset(opposite_dir(c->facing), &dx, &dy);
                    int input = read_sig(x + dx, y + dy);
                    c->signalIn = input;
                    int delay = c->setting; if (delay < 1) delay = 1; if (delay > 4) delay = 4;
                    for (int i = 0; i < delay - 1; i++) c->delayBuf[i] = c->delayBuf[i + 1];
                    c->delayBuf[delay - 1] = input;
                    int out = c->delayBuf[0];
                    c->signalOut = out; c->state = (out != 0);
                    if (out) seed_facing(newSig, x, y, c->facing, out, &qTail);
                    break;
                }
                case CT_PULSE: {
                    int dx, dy; dir_offset(opposite_dir(c->facing), &dx, &dy);
                    int input = read_sig(x + dx, y + dy);
                    c->signalIn = input;
                    if (input && !c->delayBuf[0]) c->timer = c->setting;
                    c->delayBuf[0] = input;
                    if (c->timer > 0) { c->timer--; c->signalOut = 1; c->state = 1;
                                        seed_facing(newSig, x, y, c->facing, 1, &qTail); }
                    else { c->signalOut = 0; c->state = 0; }
                    break;
                }
                case CT_NOT: {
                    int dx, dy; dir_offset(opposite_dir(c->facing), &dx, &dy);
                    int input = read_sig(x + dx, y + dy);
                    int out = input ? 0 : 1;
                    c->signalIn = input; c->signalOut = out; c->state = out;
                    if (out) seed_facing(newSig, x, y, c->facing, out, &qTail);
                    break;
                }
                case CT_AND: case CT_OR: case CT_XOR: case CT_NOR: {
                    int da, db; gate_input_dirs(c->facing, &da, &db);
                    int dx, dy;
                    dir_offset(da, &dx, &dy); int inA = read_sig(x + dx, y + dy);
                    dir_offset(db, &dx, &dy); int inB = read_sig(x + dx, y + dy);
                    int out;
                    if (c->type == CT_AND)      out = (inA && inB) ? 1 : 0;
                    else if (c->type == CT_XOR) out = (inA != inB) ? 1 : 0;
                    else if (c->type == CT_NOR) out = (!inA && !inB) ? 1 : 0;
                    else                        out = (inA || inB) ? 1 : 0;
                    c->signalIn = inA | (inB << 1); c->signalOut = out; c->state = out;
                    if (out) seed_facing(newSig, x, y, c->facing, out, &qTail);
                    break;
                }
                case CT_LATCH: {
                    int sd, rd; gate_input_dirs(c->facing, &sd, &rd);
                    int dx, dy;
                    dir_offset(sd, &dx, &dy); int setIn = read_sig(x + dx, y + dy);
                    dir_offset(rd, &dx, &dy); int rstIn = read_sig(x + dx, y + dy);
                    if (setIn && !rstIn) c->state = 1;
                    else if (rstIn && !setIn) c->state = 0;
                    c->signalIn = setIn | (rstIn << 1);
                    c->signalOut = c->state ? 1 : 0;
                    if (c->signalOut) seed_facing(newSig, x, y, c->facing, 1, &qTail);
                    break;
                }
                default: break;
            }
        }
    }

    // 2. BFS flood-fill the signal through connected wires
    while (qHead < qTail) {
        int wx = qx[qHead], wy = qy[qHead]; int val = seedv[wy][wx]; qHead++;
        for (int d = 0; d < 4; d++) {
            int dx, dy; dir_offset(d, &dx, &dy);
            int nx = wx + dx, ny = wy + dy;
            if (in_grid(nx, ny) && grid[ny][nx].type == CT_WIRE && newSig[ny][nx] < val) {
                newSig[ny][nx] = val; seedv[ny][nx] = val;
                qx[qTail] = nx; qy[qTail] = ny; qTail++;
            }
        }
    }

    // 3. publish the new field, swap buffers
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[1 - sread][y][x] = newSig[y][x];
    sread = 1 - sread;

    // 4. update sinks (lights read the freshly-published field)
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            Cell *c = &grid[y][x];
            if (c->type == CT_LIGHT) {
                int s = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy; dir_offset(d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (in_grid(nx, ny) && newSig[ny][nx] > s) s = newSig[ny][nx];
                }
                c->signalIn = s; c->state = (s > 0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
// does cell (x,y) carry/handle signal? (for wire connection rendering)
static int connects(int x, int y) {
    if (!in_grid(x, y)) return 0;
    return grid[y][x].type != CT_EMPTY;
}

static void draw_facing_arrow(int px, int py, int facing, int col) {
    int cx = px + 4, cy = py + 4;
    int dx, dy; dir_offset(facing, &dx, &dy);
    line(cx, cy, cx + dx * 3, cy + dy * 3, col);
    // little arrowhead pixel
    pset(cx + dx * 3, cy + dy * 3, col);
}

static void draw_cell(int x, int y) {
    Cell *c = &grid[y][x];
    if (c->type == CT_EMPTY) return;
    int px = x * CELL, py = y * CELL;
    const Meta *m = &META[c->type];
    int on = c->state || c->signalOut;
    int col = on ? m->acol : m->col;

    switch (m->draw) {
        case DS_WIRE: {
            int lit = sig[sread][y][x] > 0;
            int wc = lit ? CLR_YELLOW : CLR_DARK_GREY;
            int cx = px + 4, cy = py + 4;
            pset(cx, cy, wc); pset(cx - 1, cy, wc); pset(cx, cy - 1, wc);
            for (int d = 0; d < 4; d++) {
                int dx, dy; dir_offset(d, &dx, &dy);
                if (connects(x + dx, y + dy))
                    line(cx, cy, cx + dx * 4, cy + dy * 4, wc);
            }
            break;
        }
        case DS_LABEL:
            rectfill(px + 1, py + 1, 6, 6, col);
            print(m->label, px + 1, py + 1, on ? CLR_BLACK : CLR_WHITE);
            break;
        case DS_CIRCLE:
            circfill(px + 4, py + 4, 3, col);
            if (on) circ(px + 4, py + 4, 3, CLR_WHITE);
            break;
        case DS_CLOCK: {
            rectfill(px + 1, py + 1, 6, 6, col);
            char buf[2] = { (char)('0' + c->setting), 0 };
            print(buf, px + 2, py + 1, on ? CLR_BLACK : CLR_WHITE);
            break;
        }
        case DS_GATE:
            rectfill(px + 1, py + 1, 6, 6, on ? m->acol : m->col);
            print(m->label, px + 1, py + 1, on ? CLR_BLACK : CLR_WHITE);
            draw_facing_arrow(px, py, c->facing, on ? CLR_WHITE : CLR_LIGHT_GREY);
            break;
        default: break;
    }
}

static void draw_grid(void) {
    // faint dot grid background
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            pset(x * CELL, y * CELL, CLR_DARKER_BLUE);

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            draw_cell(x, y);

    // cursor highlight + ghost facing
    if (in_grid(curx, cury)) {
        int px = curx * CELL, py = cury * CELL;
        rect(px, py, CELL, CELL, CLR_WHITE);
        if (brush != CT_EMPTY && brush != CT_HAND && META[brush].dir)
            draw_facing_arrow(px, py, placeFacing, CLR_LIGHT_GREY);
    }
}

static void draw_palette(void) {
    rectfill(PANEL_X, 0, PANEL_W, SCREEN_H, CLR_BROWNISH_BLACK);
    line(PANEL_X, 0, PANEL_X, SCREEN_H - 1, CLR_DARKER_GREY);
    int rh = 13;
    int mx = mouse_x(), my = mouse_y();
    int hov = (mx >= PANEL_X && my >= 2) ? (my - 2) / rh : -1;
    for (int t = 0; t < CT_COUNT; t++) {
        const Meta *m = &META[t];
        int ry = 2 + t * rh;
        int sel = (t == brush);
        if (sel)           rectfill(PANEL_X + 1, ry - 1, PANEL_W - 2, rh, CLR_DARK_BLUE);
        else if (t == hov) rectfill(PANEL_X + 1, ry - 1, PANEL_W - 2, rh, CLR_DARKER_GREY);
        // swatch
        if (m->draw == DS_CIRCLE) circfill(PANEL_X + 6, ry + 4, 3, m->acol);
        else if (t == CT_EMPTY)   rect(PANEL_X + 3, ry + 1, 7, 7, CLR_DARKER_GREY);
        else if (t == CT_HAND) { // little pointer icon
            rectfill(PANEL_X + 5, ry + 1, 2, 6, CLR_WHITE);
            rectfill(PANEL_X + 4, ry + 4, 4, 3, CLR_WHITE);
        }
        else                      rectfill(PANEL_X + 3, ry + 1, 7, 7, m->col);
        if (m->label && m->label[0] && m->draw != DS_CIRCLE)
            print(m->label, PANEL_X + 4, ry + 1, CLR_WHITE);
        // name + key
        print(m->name, PANEL_X + 13, ry + 1, sel ? CLR_WHITE : CLR_LIGHT_GREY);
        print(m->keyl, PANEL_X + PANEL_W - 8, ry + 1, CLR_DARK_ORANGE);
    }
}

static void draw_strip(void) {
    rectfill(0, STRIP_Y, PANEL_X, SCREEN_H - STRIP_Y, CLR_BLACK);
    line(0, STRIP_Y, PANEL_X - 1, STRIP_Y, CLR_DARKER_GREY);
    font(FONT_SMALL);

    // line 1: tool + hovered cell info  (kept short so it clears the palette column)
    char l1[40];
    const char *bn = META[brush].name;
    if (in_grid(curx, cury) && grid[cury][curx].type != CT_EMPTY) {
        Cell *c = &grid[cury][curx];
        snprintf(l1, sizeof l1, "%s   %s s:%d", bn, META[c->type].name, sig[sread][cury][curx]);
    } else {
        snprintf(l1, sizeof l1, "%s", bn);
    }
    print(l1, 3, STRIP_Y + 3, CLR_WHITE);

    // line 2: run state + the two live-most controls (full list is in the docblock)
    char l2[40];
    snprintf(l2, sizeof l2, "%s x%d   SPACE run   R turn",
             running ? "RUNNING" : "PAUSED", speed);
    print(l2, 3, STRIP_Y + 13, running ? CLR_GREEN : CLR_ORANGE);

    font(FONT_NORMAL);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static void select_brush_keys(void) {
    for (int t = 0; t < CT_COUNT; t++)
        if (keyp(META[t].key)) { brush = t; return; }
}

static void apply_place(int gx, int gy) {
    if (brush == CT_EMPTY)      erase_cell(gx, gy);
    else if (brush == CT_HAND)  interact_cell(gx, gy, 1);
    else                        place_cell(gx, gy, brush, placeFacing);
}

// ---------------------------------------------------------------------------
// Demo circuits — seed the grid so the cart opens with something live
// ---------------------------------------------------------------------------
static void wire_run(int x, int y, int dx, int dy, int n) {
    for (int i = 0; i < n; i++) { place_cell(x, y, CT_WIRE, DIR_N); x += dx; y += dy; }
}

void init(void) {
    mouse_hide();   // we draw our own pixel cursor (see draw_cursor)

    // 1. switch -> wire -> light
    place_cell(2, 2, CT_SWITCH, DIR_N); grid[2][2].state = 1;
    wire_run(3, 2, 1, 0, 2);
    place_cell(5, 2, CT_LIGHT, DIR_N);

    // 2. two switches -> AND -> light  (AND faces E: inputs are N and S)
    place_cell(4, 4, CT_SWITCH, DIR_N); grid[4][4].state = 1;
    place_cell(4, 6, CT_SWITCH, DIR_N); grid[6][4].state = 1;
    place_cell(4, 5, CT_AND, DIR_E);
    wire_run(5, 5, 1, 0, 1);
    place_cell(6, 5, CT_LIGHT, DIR_N);

    // 3. clock -> wire -> light  (blinks)
    place_cell(2, 9, CT_CLOCK, DIR_N);
    wire_run(3, 9, 1, 0, 2);
    place_cell(5, 9, CT_LIGHT, DIR_N);

    // 4. NOT gate wired back into itself = a 1-tick oscillator
    place_cell(3, 12, CT_NOT, DIR_E);
    place_cell(2, 12, CT_WIRE, DIR_N);   // input (west of NOT)
    place_cell(4, 12, CT_WIRE, DIR_N);   // output (east of NOT)
    place_cell(4, 13, CT_WIRE, DIR_N);
    place_cell(3, 13, CT_WIRE, DIR_N);
    place_cell(2, 13, CT_WIRE, DIR_N);   // loop back up into the input

    // settle the combinational circuits so the opening frame is alive
    for (int i = 0; i < 3; i++) sim_tick();
}

void update(void) {
    // ---- detect input modality ----
    int mx = mouse_x(), my = mouse_y();
    if (mx != pmx || my != pmy) usingMouse = 1;
    pmx = mx; pmy = my;

    // ---- brush selection (keys) ----
    select_brush_keys();
    if (keyp('R')) {
        placeFacing = (placeFacing + 1) & 3;
        // also rotate the cell under the cursor if it's directional
        if (in_grid(curx, cury)) {
            Cell *c = &grid[cury][curx];
            if (c->type != CT_EMPTY && META[c->type].dir)
                c->facing = (c->facing + 1) & 3;
        }
    }
    if (keyp(KEY_SPACE)) running = !running;
    if (keyp('.') && !running) stepReq = 1;
    if (keyp('[')) { speed++; if (speed > 30) speed = 30; }
    if (keyp(']')) { speed--; if (speed < 1) speed = 1; }

    // clear momentary buttons (re-asserted below if still held)
    if (btnCellX >= 0) { grid[btnCellY][btnCellX].state = 0; btnCellX = btnCellY = -1; }

    // ---- mouse ----
    if (mx >= PANEL_X) {
        // palette clicks
        if (mouse_pressed(0)) {
            int rh = 13, t = (my - 2) / rh;
            if (t >= 0 && t < CT_COUNT) brush = t;
        }
    } else if (mx < PANEL_X && my < STRIP_Y) {
        int gx = mx / CELL, gy = my / CELL;
        if (usingMouse) { curx = gx; cury = gy; }
        if (mouse_down(0)) {
            // hold-to-paint for wire/eraser; single-shot otherwise
            if (brush == CT_WIRE || brush == CT_EMPTY) apply_place(gx, gy);
            else if (mouse_pressed(0)) apply_place(gx, gy);
            else if (brush == CT_HAND) interact_cell(gx, gy, 0); // keep button held
        }
        if (mouse_pressed(1)) erase_cell(gx, gy);
    }

    // ---- keyboard cursor (when not using mouse) ----
    if (btnp(0, BTN_LEFT)  || keyp(KEY_LEFT))  { curx--; usingMouse = 0; }
    if (btnp(0, BTN_RIGHT) || keyp(KEY_RIGHT)) { curx++; usingMouse = 0; }
    if (btnp(0, BTN_UP)    || keyp(KEY_UP))    { cury--; usingMouse = 0; }
    if (btnp(0, BTN_DOWN)  || keyp(KEY_DOWN))  { cury++; usingMouse = 0; }
    if (curx < 0) curx = 0; if (curx >= GW) curx = GW - 1;
    if (cury < 0) cury = 0; if (cury >= GH) cury = GH - 1;

    if (keyp('Z') || btnp(0, BTN_A)) apply_place(curx, cury);
    if (keyp('X') || btnp(0, BTN_B)) erase_cell(curx, cury);
    if (key('C')) interact_cell(curx, cury, keyp('C')); // hold for buttons

    // ---- tick the simulation ----
    if (running) {
        if (++frameAcc >= speed) { frameAcc = 0; sim_tick(); }
    } else if (stepReq) {
        stepReq = 0; sim_tick();
    }

#ifdef DE_TRACE
    watch("brush", "%s", META[brush].name);
    watch("cur_sig", "%d", sig[sread][cury][curx]);
    watch("running", "%d", running);
#endif
}

// ---------------------------------------------------------------------------
// Custom drawn cursor — we hide the OS pointer (init) and blit our own pixel
// cursor last, so it scales chunky with the canvas AND shows up in screenshots
// (the OS cursor never appears in the render-texture capture). Shape reflects
// the active tool. Bitmaps use 'X' = pixel; an auto black halo gives contrast.
// ---------------------------------------------------------------------------
static const char *CUR_ARROW[] = {   // hotspot (0,0) at the tip
    "X.......",
    "XX......",
    "XXX.....",
    "XXXX....",
    "XXXXX...",
    "XXXXXX..",
    "XXXXXXX.",
    "XXXXXXXX",
    "XXXX....",
    "X..XX...",
    "...XX...",
    "....XX..",
};
static const char *CUR_HAND[] = {    // pointing hand, hotspot ~ (2,0) at fingertip
    "..XX....",
    "..XX....",
    "..XX....",
    "..XXXXX.",
    "..XXX.XX",
    "XXXXXXXX",
    "XXXXXXXX",
    ".XXXXXXX",
    ".XXXXXXX",
    "..XXXXX.",
    "..XXXXX.",
};
static const char *CUR_CROSS[] = {   // crosshair, hotspot center (4,4), open middle
    "....X....",
    "....X....",
    "....X....",
    ".........",
    "XXX...XXX",
    ".........",
    "....X....",
    "....X....",
    "....X....",
};

static void blit_cursor(const char **rows, int n, int w, int px, int py, int fill) {
    for (int r = 0; r < n; r++)          // black halo (outline) pass
        for (int c = 0; c < w; c++)
            if (rows[r][c] == 'X')
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        pset(px + c + dx, py + r + dy, CLR_BLACK);
    for (int r = 0; r < n; r++)          // fill pass
        for (int c = 0; c < w; c++)
            if (rows[r][c] == 'X')
                pset(px + c, py + r, fill);
}

static void draw_cursor(void) {
    int mx = mouse_x(), my = mouse_y();
    if (mx >= PANEL_X || my >= STRIP_Y) {                 // over palette / strip: plain arrow
        blit_cursor(CUR_ARROW, 12, 8, mx, my, CLR_WHITE);
    } else if (brush == CT_HAND) {                        // grid + hand: pointing hand
        blit_cursor(CUR_HAND, 11, 8, mx - 2, my, CLR_WHITE);
    } else if (brush == CT_EMPTY) {                       // grid + eraser: red crosshair
        blit_cursor(CUR_CROSS, 9, 9, mx - 4, my - 4, CLR_RED);
    } else {                                              // grid + a tool: crosshair tinted to the tool
        blit_cursor(CUR_CROSS, 9, 9, mx - 4, my - 4, META[brush].acol);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    draw_grid();
    draw_palette();
    draw_strip();
    draw_cursor();   // last — on top of everything
}

// ---------------------------------------------------------------------------
// spec() — headless logic gate, live only under -DDE_SPEC (node tools/spec.js).
// Pins the ported signal sim: gate truth tables, the latch's memory, the
// NOT-loop oscillator, the clock, and wire flood-fill. This sim is the engine
// the planned pipes/belts/gears carts (and the circuit-dungeon) will reuse, so
// a regression here would ripple — exactly what the spec gate is for.
// ---------------------------------------------------------------------------
#ifdef DE_SPEC
#include "spec.h"

static void sp_reset(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) grid[y][x] = (Cell){0};
    for (int b = 0; b < 2; b++) for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[b][y][x] = 0;
    sread = 0;
}

// Build "switch -> wire -> [gate input], gate, output wire", tick to settle,
// return the gate's output. Gate faces E: inputs are its N and S wires.
static int sp_gate2(int type, int a, int b) {
    sp_reset();
    place_cell(5, 5, type, DIR_E);            // gate; inputs N(5,4) S(5,6), output E(6,5)
    place_cell(5, 4, CT_WIRE, DIR_N);
    place_cell(5, 6, CT_WIRE, DIR_N);
    place_cell(6, 5, CT_WIRE, DIR_N);         // output wire (gives the BFS something to fill)
    place_cell(5, 3, CT_SWITCH, DIR_N); grid[3][5].state = a;   // adjacent to input wire (5,4)
    place_cell(5, 7, CT_SWITCH, DIR_N); grid[7][5].state = b;   // adjacent to input wire (5,6)
    for (int i = 0; i < 6; i++) sim_tick();
    return grid[5][5].signalOut;
}

static int sp_not(int a) {
    sp_reset();
    place_cell(5, 5, CT_NOT, DIR_E);          // input W(4,5), output E(6,5)
    place_cell(4, 5, CT_WIRE, DIR_N);
    place_cell(3, 5, CT_SWITCH, DIR_N); grid[5][3].state = a;   // adjacent to input wire (4,5)
    for (int i = 0; i < 6; i++) sim_tick();
    return grid[5][5].signalOut;
}

static void sp_tickn(int n) { for (int i = 0; i < n; i++) sim_tick(); }

void spec(void) {
    // ── combinational truth tables ───────────────────────────────────────
    expect_eq(sp_not(0), 1, "NOT 0 = 1");
    expect_eq(sp_not(1), 0, "NOT 1 = 0");

    expect_eq(sp_gate2(CT_AND, 0, 0), 0, "AND 0,0 = 0");
    expect_eq(sp_gate2(CT_AND, 1, 0), 0, "AND 1,0 = 0");
    expect_eq(sp_gate2(CT_AND, 0, 1), 0, "AND 0,1 = 0");
    expect_eq(sp_gate2(CT_AND, 1, 1), 1, "AND 1,1 = 1");

    expect_eq(sp_gate2(CT_OR, 0, 0), 0, "OR 0,0 = 0");
    expect_eq(sp_gate2(CT_OR, 1, 0), 1, "OR 1,0 = 1");
    expect_eq(sp_gate2(CT_OR, 0, 1), 1, "OR 0,1 = 1");
    expect_eq(sp_gate2(CT_OR, 1, 1), 1, "OR 1,1 = 1");

    expect_eq(sp_gate2(CT_XOR, 0, 0), 0, "XOR 0,0 = 0");
    expect_eq(sp_gate2(CT_XOR, 1, 0), 1, "XOR 1,0 = 1");
    expect_eq(sp_gate2(CT_XOR, 0, 1), 1, "XOR 0,1 = 1");
    expect_eq(sp_gate2(CT_XOR, 1, 1), 0, "XOR 1,1 = 0");

    expect_eq(sp_gate2(CT_NOR, 0, 0), 1, "NOR 0,0 = 1");
    expect_eq(sp_gate2(CT_NOR, 1, 0), 0, "NOR 1,0 = 0");
    expect_eq(sp_gate2(CT_NOR, 0, 1), 0, "NOR 0,1 = 0");
    expect_eq(sp_gate2(CT_NOR, 1, 1), 0, "NOR 1,1 = 0");

    // ── wire flood-fill carries a signal across a run, and drops it ──────
    sp_reset();
    place_cell(1, 1, CT_SWITCH, DIR_N); grid[1][1].state = 1;
    place_cell(2, 1, CT_WIRE, DIR_N);
    place_cell(3, 1, CT_WIRE, DIR_N);
    place_cell(4, 1, CT_WIRE, DIR_N);
    place_cell(5, 1, CT_LIGHT, DIR_N);
    sp_tickn(6);
    expect(sig[sread][1][4] > 0, "signal floods 3 wires to the far end");
    expect(grid[1][5].state == 1, "light at the end of the run turns on");
    grid[1][1].state = 0;
    sp_tickn(6);
    expect(sig[sread][1][4] == 0, "wire run goes dark when the switch is off");
    expect(grid[1][5].state == 0, "light turns off too");

    // ── latch: set turns on, HOLDS when input drops, reset turns off ─────
    sp_reset();
    place_cell(5, 5, CT_LATCH, DIR_E);        // set=S(5,6), reset=N(5,4), out=E
    place_cell(5, 6, CT_WIRE, DIR_N);
    place_cell(5, 4, CT_WIRE, DIR_N);
    place_cell(5, 7, CT_SWITCH, DIR_N);       // set switch (adjacent to set wire)
    place_cell(5, 3, CT_SWITCH, DIR_N);       // reset switch (adjacent to reset wire)
    sp_tickn(4);
    expect(grid[5][5].signalOut == 0, "latch starts off");
    grid[7][5].state = 1; sp_tickn(4);
    expect(grid[5][5].signalOut == 1, "set pulse turns the latch on");
    grid[7][5].state = 0; sp_tickn(4);
    expect(grid[5][5].signalOut == 1, "latch HOLDS on after set drops (memory)");
    grid[3][5].state = 1; sp_tickn(4);
    expect(grid[5][5].signalOut == 0, "reset pulse turns the latch off");
    grid[3][5].state = 0; sp_tickn(4);
    expect(grid[5][5].signalOut == 0, "latch holds off after reset drops");

    // ── NOT wired back into itself oscillates with period 2 ──────────────
    sp_reset();
    place_cell(3, 12, CT_NOT, DIR_E);
    place_cell(2, 12, CT_WIRE, DIR_N);        // input (west)
    place_cell(4, 12, CT_WIRE, DIR_N);        // output (east)
    place_cell(4, 13, CT_WIRE, DIR_N);
    place_cell(3, 13, CT_WIRE, DIR_N);
    place_cell(2, 13, CT_WIRE, DIR_N);        // loop output back to input
    sp_tickn(5);
    int o1 = grid[12][3].signalOut; sim_tick();
    int o2 = grid[12][3].signalOut; sim_tick();
    int o3 = grid[12][3].signalOut;
    expect(o1 != o2, "NOT oscillator flips every tick");
    expect(o1 == o3, "NOT oscillator has period 2");

    // ── clock toggles between phases (doesn't stick) ─────────────────────
    sp_reset();
    place_cell(1, 1, CT_CLOCK, DIR_N);
    grid[1][1].setting = 2; grid[1][1].timer = 2;
    int sawLo = 0, sawHi = 0;
    for (int i = 0; i < 12; i++) { sim_tick(); if (grid[1][1].signalOut) sawHi = 1; else sawLo = 1; }
    expect(sawLo && sawHi, "clock cycles through both on and off");
}
#endif
