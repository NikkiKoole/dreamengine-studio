/* de:meta
{
  "title": "Lil Farm",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "save-load-persistence"
  ],
  "lineage": "Harvest Moon (1996) shrunk to one screen — the tile-state crop loop (grass→tilled→planted→grown) and energy/day cycle are its direct inheritance; novel in being a complete resource loop in ~250 lines with no tilemap engine.",
  "genre": "simulation",
  "homage": "Harvest Moon (1996)",
  "description": "A pocket Harvest-Moon day loop on one screen: walk your little plot, swap between hoe, seed pouch and watering can, and work the soil — till grass into furrows, plant a seed, splash it wet. Watered crops climb a growth stage every morning you sleep, ripening into bobbing red harvests you scoop into your bag, haul to the sell bin for gold, and spend on fresh seeds at the shop. An energy bar drains with every swing and a sun arcs across a sky that warms from peach dawn to dusk purple; sleep in the bed to bank your growth and roll to the next day, or run the clock out and pass out cold. WASD/arrows walk, 1/2/3 pick hoe/seed/can, Z use tool or interact (bed sleep, bin sell, shop buy), X sleep-or-sell, C buy a seed."
}
de:meta */
#include "studio.h"
#include <string.h>

// ── LIL FARM — a one-screen Harvest-Moon day loop ──────────────
// Walk your plot, equip a tool, till soil, plant a seed, water it.
// Watered crops grow one stage each new morning. Harvest grown
// crops into your bag, sell them at the BIN for cash, buy more
// seeds. Energy drains with every action; sleep in the BED to end
// the day, restore energy, and grow your crops.
//
//   WASD / arrows  walk            1 HOE   2 SEED   3 CAN
//   Z              use tool on the faced tile (or interact)
//   X (at BED)     sleep — ends the day
//   X (at BIN)     sell whole bag      C (at BIN)  buy a seed (8g)
//
// Out of energy? You pass out and wake at dawn (half energy, no grow).

#define TS   18
#define OX   28
#define OY   30
#define GW   13
#define GH   8

// tile state
enum { T_GRASS, T_TILLED, T_PLANTED, T_BED, T_BIN, T_SHOP };
enum { TOOL_HOE, TOOL_SEED, TOOL_CAN };

static unsigned char tile[GH][GW];   // tile type
static unsigned char stage[GH][GW];  // crop growth 0..3 (3 = ready)
static unsigned char wet[GH][GW];    // watered today?

static const char *toolName[3] = { "HOE", "SEED", "CAN" };

// player (pixel center)
static float px, py;
static int   facex = 0, facey = 1;   // facing direction (start: down)
static int   tool = TOOL_HOE;

// economy / meters
static int   cash, bag, seeds;       // bag = harvested crops; seeds = seed packets
static float energy;                 // 0..100
static int   day;
static float clockT;                 // 0..1 across the day
static bool  passedOut;

// juice
static float popT;                   // little "+cash"/feedback pop timer
static char  popMsg[24];
static int   popColor;
static float dayFade;                // sleep transition

static bool  inited = false;

static void say(const char *m, int c) {
    strncpy(popMsg, m, sizeof(popMsg) - 1);
    popMsg[sizeof(popMsg) - 1] = 0;
    popColor = c; popT = 1.4f;
}

static void reset_game(void) {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) { tile[y][x] = T_GRASS; stage[y][x] = 0; wet[y][x] = 0; }
    // fixtures along the edges
    tile[0][1]      = T_BED;
    tile[0][GW - 2] = T_BIN;
    tile[GH - 1][GW / 2] = T_SHOP;

    px = OX + (GW / 2) * TS + TS / 2;
    py = OY + (GH / 2) * TS + TS / 2;
    facex = 0; facey = 1; tool = TOOL_HOE;

    cash = 12;          // tiny starting stake
    bag = 0; seeds = 3; // a few seeds to begin
    energy = 100; day = 1; clockT = 0; passedOut = false;
    popT = 0; dayFade = 0;
    inited = true;
}

static int ptx(void) { return (int)((px - OX) / TS); }
static int pty(void) { return (int)((py - OY) / TS); }

static bool blocked_cell(int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= GW || cy >= GH) return true;
    int t = tile[cy][cx];
    return t == T_BED || t == T_BIN || t == T_SHOP;   // fixtures block walking, you act from beside them
}

static void spend(float e) {
    energy = clamp(energy - e, 0, 100);
}

static void next_day(bool slept) {
    day++;
    clockT = 0;
    // grow watered crops, then dry everything
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (tile[y][x] == T_PLANTED && wet[y][x] && stage[y][x] < 3 && slept)
                stage[y][x]++;
            wet[y][x] = 0;
        }
    energy = slept ? 100 : 50;
    passedOut = !slept;
    dayFade = 1.0f;
    if (slept) { strum(60, CHORD_MAJ7, INSTR_TRI, 3, 60); say("a new morning", CLR_LIGHT_YELLOW); }
    else       { note(45, INSTR_SAW, 3); say("you passed out!", CLR_RED); shake(3); }
    save_int("farm_best_cash", max(load_int("farm_best_cash", 0), cash));
}

// the cell the player is facing
static void faced_cell(int *fx, int *fy) {
    *fx = ptx() + facex;
    *fy = pty() + facey;
}

static void use_tool(void) {
    int fx, fy;
    faced_cell(&fx, &fy);
    if (fx < 0 || fy < 0 || fx >= GW || fy >= GH) return;
    int t = tile[fy][fx];

    if (energy <= 0) return;

    if (tool == TOOL_HOE) {
        if (t == T_GRASS) { tile[fy][fx] = T_TILLED; spend(4);
            note(48, INSTR_NOISE, 2); say("tilled soil", CLR_BROWN); }
        else if (t == T_PLANTED && stage[fy][fx] >= 3) {   // harvest with hoe too (faced)
            tile[fy][fx] = T_GRASS; stage[fy][fx] = 0; wet[fy][fx] = 0;
            bag++; spend(2); note(72, INSTR_TRI, 3); say("+1 crop", CLR_GREEN);
        }
    } else if (tool == TOOL_SEED) {
        if (t == T_TILLED) {
            if (seeds <= 0) { say("no seeds! buy @shop", CLR_RED); note(45, INSTR_SQUARE, 1); return; }
            seeds--;
            tile[fy][fx] = T_PLANTED; stage[fy][fx] = 0; spend(3);
            note(64, INSTR_SQUARE, 2); say("planted", CLR_LIME_GREEN);
        }
    } else if (tool == TOOL_CAN) {
        if (t == T_PLANTED && !wet[fy][fx]) {
            wet[fy][fx] = 1; spend(3);
            hit(70, INSTR_SINE, 2, 120); say("watered", CLR_BLUE);
        }
    }
}

// interact with a fixture you're standing next to (Z near it)
static int faced_fixture(void) {
    int fx, fy; faced_cell(&fx, &fy);
    if (fx < 0 || fy < 0 || fx >= GW || fy >= GH) return -1;
    int t = tile[fy][fx];
    if (t == T_BED || t == T_BIN || t == T_SHOP) return t;
    return -1;
}

void init(void) {
    bpm(96);
    instrument(5, INSTR_SINE, 4, 80, 4, 200);
    reset_game();
}

void update(void) {
    if (!inited) reset_game();
    float d = dt();

    if (dayFade > 0) dayFade = max(0, dayFade - d * 1.6f);
    if (popT > 0) popT = max(0, popT - d);

    // clock advances; running out of day forces sleep-less rollover (pass out)
    clockT += d * 0.012f;   // ~83s real -> a full day; plenty for actions
    if (clockT >= 1.0f) next_day(false);
    if (energy <= 0 && !passedOut) { /* let them keep walking but can't act */ }

    // ── movement ──
    float spd = 56.0f;
    float mvx = 0, mvy = 0;
    if (btn(0, BTN_LEFT))  mvx -= 1;
    if (btn(0, BTN_RIGHT)) mvx += 1;
    if (btn(0, BTN_UP))    mvy -= 1;
    if (btn(0, BTN_DOWN))  mvy += 1;
    if (mvx != 0 || mvy != 0) {
        if (mvx != 0) { facex = (int)sgn((int)mvx); facey = 0; }
        else          { facey = (int)sgn((int)mvy); facex = 0; }
        float nx = px + mvx * spd * d;
        float ny = py + mvy * spd * d;
        // keep inside the plot bounds (with a small margin) and out of fixtures
        float minx = OX + 3, maxx = OX + GW * TS - 3;
        float miny = OY + 3, maxy = OY + GH * TS - 3;
        int ncx = (int)((nx - OX) / TS), pcy = pty();
        if (nx > minx && nx < maxx && !blocked_cell(ncx, pcy)) px = nx;
        int pcx = ptx(), ncy = (int)((ny - OY) / TS);
        if (ny > miny && ny < maxy && !blocked_cell(pcx, ncy)) py = ny;
    }

    // ── tool select ──
    if (key('1')) tool = TOOL_HOE;
    if (key('2')) tool = TOOL_SEED;
    if (key('3')) tool = TOOL_CAN;

    // ── actions ──
    if (btnp(0, BTN_A)) {           // Z — use tool / interact
        int fix = faced_fixture();
        if (fix == T_BIN) {
            if (bag > 0) { int gain = bag * 5; cash += gain; bag = 0;
                strum(67, CHORD_MAJ, INSTR_SQUARE, 3, 40);
                say(str("sold +%dg", gain), CLR_YELLOW); }
            else say("bag empty", CLR_LIGHT_GREY);
        } else if (fix == T_SHOP) {
            if (cash >= 8) { cash -= 8; seeds++;
                note(80, INSTR_SQUARE, 2); say("bought seed", CLR_LIME_GREEN); }
            else say("need 8g", CLR_RED);
        } else if (fix == T_BED) {
            next_day(true);
        } else {
            use_tool();
        }
    }
    if (btnp(0, BTN_B)) {           // X — sleep at bed / sell at bin (convenience)
        int fix = faced_fixture();
        if (fix == T_BED) next_day(true);
        else if (fix == T_BIN) {
            if (bag > 0) { int gain = bag * 5; cash += gain; bag = 0;
                strum(67, CHORD_MAJ, INSTR_SQUARE, 3, 40);
                say(str("sold +%dg", gain), CLR_YELLOW); }
        }
    }
    if (keyp('C') || keyp('c')) {   // C — buy a seed when facing the shop
        if (faced_fixture() == T_SHOP) {
            if (cash >= 8) { cash -= 8; seeds++;
                note(80, INSTR_SQUARE, 2); say("bought seed", CLR_LIME_GREEN); }
            else say("need 8g", CLR_RED);
        }
    }
}

// ── drawing ──
static void draw_tile(int gx, int gy) {
    int x = OX + gx * TS, y = OY + gy * TS;
    int t = tile[gy][gx];

    if (t == T_GRASS) {
        rectfill(x, y, TS, TS, ((gx + gy) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
        // a few grass specks
        pset(x + 4 + (gx * 3 % 7), y + 5, CLR_LIME_GREEN);
        pset(x + 9, y + 11 - (gy % 3), CLR_GREEN);
    } else if (t == T_TILLED || t == T_PLANTED) {
        int soil = wet[gy][gx] ? CLR_DARK_BROWN : CLR_BROWN;
        rectfill(x, y, TS, TS, soil);
        // furrows
        line(x + 1, y + 5,  x + TS - 2, y + 5,  CLR_BROWNISH_BLACK);
        line(x + 1, y + 11, x + TS - 2, y + 11, CLR_BROWNISH_BLACK);
        if (wet[gy][gx]) { pset(x + 3, y + 8, CLR_BLUE); pset(x + 12, y + 3, CLR_BLUE); }
        if (t == T_PLANTED) {
            int s = stage[gy][gx];
            int cx = x + TS / 2, by = y + TS - 3;
            if (s == 0) { pset(cx, by - 1, CLR_LIME_GREEN); pset(cx, by - 2, CLR_GREEN); }
            else if (s == 1) { line(cx, by, cx, by - 5, CLR_DARK_GREEN);
                               line(cx, by - 3, cx - 2, by - 5, CLR_GREEN);
                               line(cx, by - 3, cx + 2, by - 5, CLR_GREEN); }
            else if (s == 2) { line(cx, by, cx, by - 7, CLR_DARK_GREEN);
                               circfill(cx, by - 8, 2, CLR_GREEN); }
            else { // ready — bob
                int bob = (int)(sin_deg(now() * 200 + gx * 40) * 1.5f);
                line(cx, by, cx, by - 7, CLR_DARK_GREEN);
                circfill(cx, by - 9 + bob, 3, CLR_RED);
                pset(cx, by - 12 + bob, CLR_GREEN);
            }
        }
    }
}

static void draw_fixture(int gx, int gy) {
    int x = OX + gx * TS, y = OY + gy * TS;
    int t = tile[gy][gx];
    // grass under fixtures
    rectfill(x, y, TS, TS, CLR_DARK_GREEN);
    if (t == T_BED) {
        rectfill(x + 1, y + 4, TS - 2, TS - 6, CLR_BROWN);
        rectfill(x + 1, y + 4, 5, TS - 6, CLR_WHITE);
        rectfill(x + 6, y + 6, TS - 8, TS - 9, CLR_RED);
        print("bed", x - 1, y - 7, CLR_WHITE);
    } else if (t == T_BIN) {
        rectfill(x + 2, y + 3, TS - 4, TS - 4, CLR_LIGHT_GREY);
        rect(x + 2, y + 3, TS - 4, TS - 4, CLR_DARK_GREY);
        rectfill(x + 4, y + 5, TS - 8, 3, CLR_YELLOW);
        print("sell", x - 4, y - 7, CLR_YELLOW);
    } else if (t == T_SHOP) {
        rectfill(x + 2, y + 5, TS - 4, TS - 6, CLR_DARK_PURPLE);
        rectfill(x + 1, y + 2, TS - 2, 4, CLR_RED);
        for (int i = 0; i < 4; i++) pset(x + 3 + i * 3, y + 3, CLR_WHITE);
        print("shop", x - 4, y + TS, CLR_LIME_GREEN);
    }
}

static void draw_player(void) {
    int x = (int)px, y = (int)py;
    int bob = (btn(0,BTN_LEFT)||btn(0,BTN_RIGHT)||btn(0,BTN_UP)||btn(0,BTN_DOWN))
              ? (int)(sin_deg(now() * 520) * 1) : 0;
    y += bob;
    ovalfill(x, y + 6, 5, 2, CLR_BROWNISH_BLACK);     // shadow
    rectfill(x - 4, y - 2, 8, 8, CLR_TRUE_BLUE);       // overalls
    circfill(x, y - 5, 4, CLR_LIGHT_PEACH);            // head
    rectfill(x - 4, y - 9, 8, 3, CLR_DARK_ORANGE);     // straw hat brim
    rectfill(x - 2, y - 11, 4, 2, CLR_ORANGE);         // hat top
    // facing pip + tool color
    int tc = tool == TOOL_HOE ? CLR_LIGHT_GREY : tool == TOOL_SEED ? CLR_LIME_GREEN : CLR_BLUE;
    pset(x + facex * 5, y - 1 + facey * 5, tc);
    pset(x + facex * 6, y - 1 + facey * 6, tc);
}

void draw(void) {
    if (!inited) reset_game();

    // sky tint shifts across the day (morning peach -> noon blue -> dusk purple)
    int skyc = clockT < 0.25f ? CLR_DARK_PEACH
             : clockT < 0.7f  ? CLR_DARKER_BLUE
             : clockT < 0.88f ? CLR_DARK_PURPLE
                              : CLR_DARKER_PURPLE;
    cls(skyc);

    // field border / fence
    rectfill(OX - 3, OY - 3, GW * TS + 6, GH * TS + 6, CLR_DARK_BROWN);

    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int t = tile[gy][gx];
            if (t == T_BED || t == T_BIN || t == T_SHOP) draw_fixture(gx, gy);
            else draw_tile(gx, gy);
        }

    // faced-cell highlight
    int fx, fy; faced_cell(&fx, &fy);
    if (fx >= 0 && fy >= 0 && fx < GW && fy < GH)
        rect(OX + fx * TS, OY + fy * TS, TS, TS,
             blink(8) ? CLR_YELLOW : CLR_LIGHT_YELLOW);

    draw_player();

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, OY - 4, CLR_BLACK);
    print(str("DAY %d", day), 4, 3, CLR_WHITE);
    // energy bar
    print("NRG", 60, 3, CLR_LIGHT_GREY);
    int ec = energy > 50 ? CLR_GREEN : energy > 20 ? CLR_YELLOW : CLR_RED;
    bar(86, 3, 60, 7, energy / 100.0f, ec, CLR_DARKER_GREY);
    print(str("SEED %d", seeds), 156, 3, CLR_LIME_GREEN);
    print(str("BAG %d", bag), 220, 3, CLR_PEACH);
    print_right(str("%dg", cash), SCREEN_W - 4, 3, CLR_YELLOW);

    // clock sun arc position
    int sx = 4 + (int)(clockT * (SCREEN_W - 12));
    circfill(sx, 17, 2, clockT < 0.85f ? CLR_YELLOW : CLR_DARK_ORANGE);

    // tool selector
    rectfill(0, OY - 4, SCREEN_W, 12, CLR_BROWNISH_BLACK);
    for (int i = 0; i < 3; i++)
        print(str("%d:%s", i + 1, toolName[i]), 4 + i * 60, OY - 2,
              i == tool ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print("Z use  X sleep/sell  C buy", 196, OY - 2, CLR_MEDIUM_GREY);

    // feedback pop above the player
    if (popT > 0) {
        int alpha_y = (int)((1.0f - popT / 1.4f) * 8);
        print_centered(popMsg, SCREEN_W/2, (int)py - 18 - alpha_y, popColor);
    }

    // sleep / pass-out flash
    if (dayFade > 0) fade(dayFade * 0.85f);
    if (energy <= 0 && !passedOut)
        print_centered("EXHAUSTED — sleep in the bed", SCREEN_W/2, SCREEN_H - 12, blink(10) ? CLR_RED : CLR_DARK_RED);
}
