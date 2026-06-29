/* de:meta
{
  "title": "Transport Tycoon",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "grid-movement",
    "title-play-gameover-loop"
  ],
  "lineage": "Pocket homage to Transport Tycoon (1994); novel in using a random-walk river generator to ensure a guaranteed-solvable crossing and wiring a simple cargo-accumulation economy onto a player-laid rail path.",
  "genre": "simulation",
  "homage": "Transport Tycoon (1994)",
  "description": "A pocket-sized Transport Tycoon. Two towns sit on a green tile world split by a wandering river; you lay a rail line by hand, one tile at a time, growing track out from town A until it reaches town B. The moment the line connects, town A starts stockpiling cargo and a chuffing little locomotive is yours to dispatch — it rolls A to B hauling goods, pays cash on delivery (the balance rolls up with a cha-ching and a coin pop), then runs back empty to wait for the next run. Build it, then watch it earn until you hit the tycoon goal. Left-click a grass tile next to your rail's end (or town A) to lay rail, left-click a town to dispatch the idle train, R for a new map."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── TRANSPORT TYCOON (deluxe pocket edition) ──────────────────────
// Two towns on a green map split by a river. Lay a rail line tile by
// tile from town A, and once it reaches town B, dispatch a train to
// haul cargo A->B for cash. Build it, then watch it run.
//
//   MOUSE LEFT on grass next to your rail end (or on town A) — lay rail
//   MOUSE LEFT on a town — dispatch the idle train down the line
//   R — new map
//
// No sprites: the whole world is primitives so it bakes clean.

#define GW    20      // grid columns
#define GH    11      // grid rows  (rest of screen is HUD)
#define TILE  16
#define N     (GW * GH)
#define HUDY  (GH * TILE)         // 176; HUD lives 176..199

enum { TER_GRASS, TER_WATER, TER_TOWN };
enum { TRAIN_IDLE, TRAIN_GOING, TRAIN_RETURN };
enum { ST_BUILD, ST_RUN, ST_WIN };

static int  ter[N];               // terrain per cell
static bool rail[N];              // is there track on this cell
static int  path[N];              // ordered list of track cell indices (the line)
static int  path_len;             // how many cells in the path so far

static int  town_a, town_b;       // cell indices of the two towns
static bool line_open;            // rail connects A..B

// train: it lives at fractional position `seg + t` along path[]
static int   state;
static int   train_mode;
static float seg;                 // current segment index (0..path_len-1)
static float move_t;              // 0..1 progress across current segment
static int   carrying;            // cargo units on board

static int   cash;
static float cash_shown;          // lerped display value
static float cargo_a;             // stockpile building at town A
static int   last_pay;            // last delivery payout (for HUD flash)
static float pay_flash;

// a little coin pop on delivery
static float coin_t;              // >0 = animating
static int   coin_cx, coin_cy;

#define PRICE   7                 // cash per cargo unit
#define GOAL    600               // win at this cash
#define CARGO_RATE 1.6f           // units/sec building at A while line open
#define CARGO_CAP  40             // max stockpile
#define TRAIN_SPEED 3.2f          // segments per second

static int cx_(int i) { return i % GW; }
static int cy_(int i) { return i / GW; }
static int idx(int x, int y) { return y * GW + x; }
static int px_(int i) { return cx_(i) * TILE + TILE / 2; }
static int py_(int i) { return cy_(i) * TILE + TILE / 2; }

static bool adj(int a, int b) {   // orthogonally adjacent cells
    int ax = cx_(a), ay = cy_(a), bx = cx_(b), by = cy_(b);
    return (abs(ax - bx) + abs(ay - by)) == 1;
}

// ── world generation ──────────────────────────────────────────────
static void gen_world(void) {
    for (int i = 0; i < N; i++) { ter[i] = TER_GRASS; rail[i] = false; }

    // a wandering river roughly down the middle
    int riverx = GW / 2;
    for (int y = 0; y < GH; y++) {
        riverx += rnd(3) - 1;
        riverx = mid(4, riverx, GW - 5);
        ter[idx(riverx, y)] = TER_WATER;
        if (chance(55)) ter[idx(riverx + 1, y)] = TER_WATER;
    }
    // one guaranteed dry crossing so a path is always possible
    int bridge_y = rnd_between(2, GH - 2);
    for (int x = 0; x < GW; x++) ter[idx(x, bridge_y)] = TER_GRASS;

    // towns: left third and right third, on the bridge row for an easy start
    town_a = idx(rnd_between(1, 4), bridge_y);
    town_b = idx(rnd_between(GW - 5, GW - 2), bridge_y);
    ter[town_a] = TER_TOWN;
    ter[town_b] = TER_TOWN;
}

static void reset_game(void) {
    gen_world();
    path_len = 0;
    line_open = false;
    state = ST_BUILD;
    train_mode = TRAIN_IDLE;
    seg = 0; move_t = 0; carrying = 0;
    cash = 0; cash_shown = 0;
    cargo_a = 0;
    last_pay = 0; pay_flash = 0;
    coin_t = 0;

    // the rail line always begins anchored at town A
    rail[town_a] = true;
    path[path_len++] = town_a;
}

void init(void) {
    instrument(5, INSTR_SQUARE, 2, 70, 0, 40);   // chuff
    instrument_duty(5, 0.5f);
    reset_game();
}

// ── track laying ──────────────────────────────────────────────────
static int rail_end(void) { return path[path_len - 1]; }

static bool try_lay(int cell) {
    if (state != ST_BUILD) return false;
    if (rail[cell]) return false;
    if (!adj(cell, rail_end())) return false;
    if (ter[cell] == TER_WATER) return false;

    // reaching town B closes the line; any grass cell is fine to lay on
    if (ter[cell] == TER_TOWN && cell != town_b) return false;

    rail[cell] = true;
    path[path_len++] = cell;
    sfx(-1);
    note(64 + (path_len % 5), INSTR_TRI, 1);

    if (cell == town_b) {
        line_open = true;
        state = ST_RUN;
        strum(60, CHORD_MAJ, INSTR_SINE, 3, 40);
    }
    return true;
}

// ── dispatch + train motion ────────────────────────────────────────
static void dispatch(void) {
    if (!line_open || train_mode != TRAIN_IDLE) return;
    // pick up everything waiting at A, head out
    train_mode = TRAIN_GOING;
    seg = 0; move_t = 0;
    carrying = (int)cargo_a;
    cargo_a -= carrying;
    note(48, INSTR_SQUARE, 3);
}

static void deliver(void) {
    last_pay = carrying * PRICE;
    cash += last_pay;
    pay_flash = 1.0f;
    coin_t = 1.0f; coin_cx = px_(town_b); coin_cy = py_(town_b) - 10;
    carrying = 0;
    if (last_pay > 0) {
        strum(67, CHORD_MAJ7, INSTR_SINE, 4, 35);
        shake(min(4, 1 + last_pay / 60));
    }
    if (cash >= GOAL) state = ST_WIN;
}

void update(void) {
    if (keyp('R') || btnp(0, BTN_B)) { reset_game(); return; }

    float d = dt();

    // cash counter rolls toward true value
    cash_shown = lerp(cash_shown, (float)cash, clamp(d * 6.0f, 0, 1));
    if (pay_flash > 0) pay_flash -= d * 1.6f;
    if (coin_t > 0)   coin_t   -= d * 1.2f;

    // cargo builds at A whenever the line is open and goods aren't on the train
    if (line_open && cargo_a < CARGO_CAP)
        cargo_a = clamp(cargo_a + CARGO_RATE * d, 0, CARGO_CAP);

    if (state == ST_WIN) return;

    // ── mouse: lay rail or dispatch ──
    if (mouse_pressed(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y();
        if (my < HUDY) {
            int cell = idx(mid(0, mx / TILE, GW - 1), mid(0, my / TILE, GH - 1));
            if ((cell == town_a || cell == town_b) && line_open) {
                if (train_mode == TRAIN_IDLE) dispatch();
                else note(40, INSTR_NOISE, 1);          // already running
            } else if (!try_lay(cell)) {
                note(36, INSTR_NOISE, 1);               // illegal placement blip
            }
        }
    }

    // ── train along the path ──
    if (train_mode != TRAIN_IDLE && path_len > 1) {
        move_t += TRAIN_SPEED * d;
        // chuff per segment crossed
        while (move_t >= 1.0f) {
            move_t -= 1.0f;
            seg += 1;
            if (train_mode == TRAIN_GOING) {
                if (seg >= path_len - 1) {              // arrived at B
                    seg = path_len - 1; move_t = 0;
                    deliver();
                    train_mode = TRAIN_RETURN;
                } else note(60, 5, 1);
            } else { // RETURN
                if (seg >= path_len - 1) {              // arrived back at A
                    seg = path_len - 1; move_t = 0;
                    train_mode = TRAIN_IDLE;
                    seg = 0;
                } else note(55, 5, 1);
            }
        }
    }
}

// ── train screen position (interpolated along path) ────────────────
static void train_pos(float *ox, float *oy) {
    int i0, i1; float t = move_t;
    if (train_mode == TRAIN_RETURN) {
        // walk path backwards: from B (end) to A (0)
        int s = (int)seg;
        i0 = path[path_len - 1 - s];
        int j = path_len - 1 - s - 1;
        i1 = (j >= 0) ? path[j] : i0;
    } else {
        int s = (int)seg;
        i0 = path[mid(0, s, path_len - 1)];
        i1 = path[mid(0, s + 1, path_len - 1)];
    }
    *ox = lerp(px_(i0), px_(i1), t);
    *oy = lerp(py_(i0), py_(i1), t);
}

// ── drawing ────────────────────────────────────────────────────────
static void draw_track_cell(int i) {
    int x = cx_(i) * TILE, y = cy_(i) * TILE, cx = x + 8, cy = y + 8;
    // ballast
    rectfill(x + 1, y + 1, TILE - 2, TILE - 2, CLR_DARK_BROWN);
    // connect a rail line toward each rail neighbour
    int nb[4] = { i - 1, i + 1, i - GW, i + GW };
    bool drew = false;
    for (int k = 0; k < 4; k++) {
        int j = nb[k];
        if (j < 0 || j >= N) continue;
        if (k == 0 && cx_(i) == 0) continue;
        if (k == 1 && cx_(i) == GW - 1) continue;
        if (!rail[j] && !(j == town_a || j == town_b)) continue;
        line(cx, cy, px_(j), py_(j), CLR_LIGHT_GREY);
        drew = true;
    }
    if (!drew) circfill(cx, cy, 2, CLR_LIGHT_GREY);
    // sleepers / dot
    pset(cx, cy, CLR_WHITE);
}

static void draw_town(int i, const char *label, bool is_a) {
    int x = cx_(i) * TILE, y = cy_(i) * TILE;
    int roof = is_a ? CLR_RED : CLR_BLUE;
    rectfill(x + 2, y + 5, 12, 9, CLR_LIGHT_PEACH);  // walls
    rect    (x + 2, y + 5, 12, 9, CLR_DARK_GREY);
    trifill(x + 1, y + 6, x + 8, y + 1, x + 15, y + 6, roof);  // roof
    rectfill(x + 6, y + 9, 4, 5, CLR_DARK_BROWN);    // door
    pset(x + 4, y + 8, CLR_DARK_BLUE);               // windows
    pset(x + 11, y + 8, CLR_DARK_BLUE);
    print(label, x + 6, y - 7, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARK_GREEN);

    // terrain
    for (int i = 0; i < N; i++) {
        int x = cx_(i) * TILE, y = cy_(i) * TILE;
        if (ter[i] == TER_WATER) {
            rectfill(x, y, TILE, TILE, CLR_DARK_BLUE);
            // gentle ripples
            int o = (int)(sin_deg((x + now() * 60)) * 1.5f);
            line(x + 2, y + 6 + o, x + TILE - 3, y + 6 + o, CLR_TRUE_BLUE);
            line(x + 3, y + 11 - o, x + TILE - 2, y + 11 - o, CLR_TRUE_BLUE);
        } else {
            rectfill(x, y, TILE, TILE, ((cx_(i) + cy_(i)) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
            if ((i * 7) % 11 == 0) pset(x + 4 + (i % 6), y + 5 + (i % 7), CLR_LIME_GREEN); // shrubs
        }
    }

    // rail
    for (int i = 0; i < N; i++) if (rail[i] && ter[i] != TER_TOWN) draw_track_cell(i);

    // towns (draw over rail so they read clearly)
    draw_town(town_a, "A", true);
    draw_town(town_b, "B", false);

    // hover highlight: next legal rail tile, or a dispatchable town
    if (state != ST_WIN) {
        int mx = mouse_x(), my = mouse_y();
        if (my < HUDY) {
            int cell = idx(mid(0, mx / TILE, GW - 1), mid(0, my / TILE, GH - 1));
            int hx = cx_(cell) * TILE, hy = cy_(cell) * TILE;
            bool town_click = (cell == town_a || cell == town_b) && line_open && train_mode == TRAIN_IDLE;
            bool can_lay = state == ST_BUILD && !rail[cell] && adj(cell, rail_end())
                           && ter[cell] != TER_WATER
                           && (ter[cell] != TER_TOWN || cell == town_b);
            if (town_click) rect(hx, hy, TILE, TILE, blink(12) ? CLR_YELLOW : CLR_ORANGE);
            else rect(hx, hy, TILE, TILE, can_lay ? CLR_WHITE : CLR_RED);
        }
    }

    // train
    if (path_len > 1 && (train_mode != TRAIN_IDLE || line_open)) {
        float tx, ty; train_pos(&tx, &ty);
        // heading for orientation
        float hx, hy;
        float save_t = move_t; move_t = clamp(move_t + 0.2f, 0, 1);
        train_pos(&hx, &hy); move_t = save_t;
        float ang = angle_to((int)tx, (int)ty, (int)hx, (int)hy);
        // loco body
        int body = train_mode == TRAIN_IDLE ? CLR_DARK_GREY : CLR_INDIGO;
        circfill((int)tx, (int)ty, 5, CLR_BLACK);
        circfill((int)tx, (int)ty, 4, body);
        // front
        int fxp = (int)(tx + dx(4, ang)), fyp = (int)(ty + dy(4, ang));
        circfill(fxp, fyp, 2, CLR_YELLOW);
        // cargo marker if loaded
        if (carrying > 0) circfill((int)tx, (int)ty, 2, CLR_ORANGE);
        // smoke when running
        if (train_mode != TRAIN_IDLE) {
            int sx = (int)(tx - dx(5, ang)), sy = (int)(ty - dy(5, ang));
            int puff = (int)(now() * 6) % 3;
            circfill(sx, sy - puff, 1 + puff, CLR_LIGHT_GREY);
        }
    }

    // coin pop on delivery
    if (coin_t > 0) {
        int yoff = (int)((1 - coin_t) * 14);
        int c = blink(4) ? CLR_YELLOW : CLR_ORANGE;
        print(str("+$%d", last_pay), coin_cx - 8, coin_cy - yoff, c);
    }

    // ── HUD ──
    rectfill(0, HUDY, SCREEN_W, SCREEN_H - HUDY, CLR_BLACK);
    line(0, HUDY, SCREEN_W, HUDY, CLR_DARK_GREY);
    int hy = HUDY + 3;
    print(str("CASH $%d", (int)(cash_shown + 0.5f)), 4, hy,
          pay_flash > 0 && blink(3) ? CLR_YELLOW : CLR_GREEN);

    // cargo-demand bar at A
    print("CARGO@A", 96, hy, CLR_LIGHT_GREY);
    bar(150, hy, 48, 6, cargo_a / CARGO_CAP, CLR_ORANGE, CLR_DARK_BROWN);
    print(str("%d", (int)cargo_a), 202, hy, CLR_ORANGE);

    int hy2 = HUDY + 13;
    if (!line_open) {
        print("LAY RAIL A->B   (click tiles)", 4, hy2,
              blink(20) ? CLR_WHITE : CLR_LIGHT_GREY);
    } else if (state == ST_WIN) {
        print("TYCOON! press R", 4, hy2, CLR_YELLOW);
    } else if (train_mode == TRAIN_IDLE) {
        print("LINE OPEN -- click a town to dispatch", 4, hy2,
              blink(16) ? CLR_GREEN : CLR_MEDIUM_GREEN);
    } else {
        print(carrying > 0 ? str("HAULING %d -> B", carrying) : "RETURNING EMPTY",
              4, hy2, CLR_BLUE);
    }
    print_right(str("GOAL $%d", GOAL), SCREEN_W - 4, hy, CLR_DARK_GREY);

    // win banner
    if (state == ST_WIN) {
        int w = 180, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 70, w, 40, CLR_DARK_GREEN);
        rect    (bx, 70, w, 40, CLR_YELLOW);
        print_centered("TRANSPORT TYCOON!", SCREEN_W/2, 80, CLR_YELLOW);
        print_centered(str("you banked $%d", cash), SCREEN_W/2, 92, CLR_WHITE);
        print_centered("press R for a new map", SCREEN_W/2, 100, CLR_LIGHT_GREY);
    }
}
