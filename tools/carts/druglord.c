/* de:meta
{
  "slug": "druglord",
  "title": "druglord",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Direct port of the classic Drugwars/Dope Wars formula (1984/1984-inspired text game) into a living tilemap city, adding pal()-recolored pedestrians, ambient music beds, and a per-district price-flavor matrix.",
  "genre": "tycoon",
  "homage": "Drug Wars (1984)",
  "description": "A Drugwars-style economy game. You have 30 DAYS to flip a small stake — and a loan-shark debt that grows 10%/day — into a fortune, trading six substances across six city districts whose prices swing daily. Buy where a good is cheap, haul it to where it sells dear, stash cash in the bank away from muggers, and pay the debt down before day 30. Travelling to a district costs a day, re-rolls every price, and can trigger an event card: a crackdown that makes a price SOAR, a glut that CRASHES it, a mugging, a cop bust (siren + flashing lights, you ditch your stash), a lucky dumpster find or a bigger coat. Each district is a living tile-map scene — glass-tower downtown, the crate-stacked docks, leafy suburbs, grimy tower blocks, neon Chinatown, the park — with pal()-recolored pedestrians and a day/night sky that re-lights the windows as the days pass. The money counter rolls, prices show green/red arrows and blinking HOT tags, and each place has its own ambient sound bed. Mouse: click a good to buy 1 (right-click sell 1); the bottom bar is BUY/SELL/QTY/BANK/LOAN/TRAVEL; on the city map click a district to travel. Keys: up/down pick a good, Z buy, X sell, Q cycle quantity, M map, ENTER confirm."
}
de:meta */
#include "studio.h"

// DRUGLORD — a Drugwars-style economy game.
// You're a small-time dealer with 30 DAYS to flip a stake (and a loan-shark
// debt) into as much cash as you can. Six substances, six districts whose
// prices swing daily. Buy low, sell high, dodge cops and muggers, stash cash
// in the bank and pay down a debt that grows 10%/day. Travelling = one day.
//
// It's a menu + map game, but each district is a living tile-map scene with
// pal()-recolored pedestrians and a day/night sky that sweeps as the days pass.
//
// MOUSE: click a good to BUY 1 (right-click SELL 1); the bottom bar has
// BUY/SELL/QTY/BANK/LOAN/TRAVEL; on the map click a district to travel.
// KEYS: up/down pick a good, Z buy / X sell, Q cycle qty, M map, ENTER confirm.

// ---- substances ----------------------------------------------------------
enum { WEED, SHROOMS, SPEED, ACID, COKE, SMACK, NGOOD };
static const char *GOOD[NGOOD] = { "WEED", "SHROOMS", "SPEED", "ACID", "COKE", "SMACK" };
static const int   BASE[NGOOD] = {   40,    110,      220,     480,    1100,   650 };

// ---- districts ------------------------------------------------------------
enum { DOWNTOWN, DOCKS, SUBURBS, BLOCKS, CHINATOWN, PARK, NPLACE };
static const char *PLACE[NPLACE] = { "DOWNTOWN", "THE DOCKS", "SUBURBS", "THE BLOCKS", "CHINATOWN", "UPTOWN PARK" };
static const char *SHORT[NPLACE] = { "DWNTWN", "DOCKS", "SUBURB", "BLOCKS", "C-TOWN", "PARK" };

// price level per district×good, as a percent of base (the place's "flavour":
// where a good is cheap to BUY (<100) and where it sells dear (>100)).
static const int FAC[NPLACE][NGOOD] = {
//   WEED SHRM SPD ACID COKE SMACK
    { 120, 110,  80, 140, 150, 110 },  // Downtown — luxury: coke/acid sell high
    { 100, 100, 110,  90,  60,  55 },  // Docks — imports: coke/smack cheap
    {  55,  60, 100, 110, 120, 110 },  // Suburbs — homegrown: weed/shrooms cheap
    {  90, 100,  60, 100, 110,  70 },  // The Blocks — speed/smack cheap
    { 100, 110,  65,  55, 100,  95 },  // Chinatown — labs: acid/speed cheap
    { 150, 140, 100, 120, 130,  90 },  // Uptown Park — festival: weed/shrooms sell high
};

// per-district mood: window/neon glow colour + ambient music
static const int  NEON[NPLACE] = { CLR_BLUE, CLR_ORANGE, CLR_YELLOW, CLR_PINK, CLR_RED, CLR_GREEN };
static const int  BPMV[NPLACE] = { 96, 80, 100, 84, 104, 92 };
static const int  ROOTV[NPLACE]= { 48, 45, 52, 43, 50, 47 };
static const int  CHTV[NPLACE] = { CHORD_MIN7, CHORD_MIN, CHORD_MAJ7, CHORD_MIN7, CHORD_DOM7, CHORD_MAJ };
static const int  SCLV[NPLACE] = { SCALE_MINOR, SCALE_PENTA_MIN, SCALE_MAJOR, SCALE_BLUES, SCALE_PENTA, SCALE_PENTA };

// ---- state ----------------------------------------------------------------
enum { TITLE, MARKET, MAP, EVENT, OVER };
enum { M_NONE, M_BANK, M_LOAN };

static int   price[NPLACE][NGOOD];     // today's prices everywhere
static int   seen[NPLACE][NGOOD];      // last price the player saw in each market
static int   arrow[NGOOD];             // -1 down / 0 none / +1 up, for current district
static int   inv[NGOOD];

static int   cash, bank, debt, day, here, capacity;
static int   sel, qtyMode, modal, state, mapSel;
static int   best, over_saved;
static float showCash;                 // rolls toward cash

// event card
enum { EV_NONE, EV_MUG, EV_BUST, EV_SPIKE, EV_CRASH, EV_FIND, EV_COAT, EV_SHARK };
static int   ev_kind, ev_g, ev_amt, ev_bad;
static float ev_t;

// flash overlay
static float flashT; static int flashColor;

// ---- pedestrians (scene ambiance) -----------------------------------------
#define MAGIC_SHIRT 28
#define NPED 7
static const int SHIRTS[] = { 8, 9, 12, 14, 7, 26, 30, 11 };
typedef struct { float x, vx; int type, shirt; float phase; } Ped;
static Ped ped[NPED];

// ---- layout constants -----------------------------------------------------
#define SCENE_Y 11
#define PANEL_Y 75
#define ROW0    98
#define ROWH    13
#define BY      180
#define BH      16
#define BW      50
#define BX(i)   (4 + (i) * 52)
// modal box
#define MBX 48
#define MBY 54
#define MBW 224
#define MBH 92

// ===========================================================================
static int  units(void) { int t = 0; for (int i = 0; i < NGOOD; i++) t += inv[i]; return t; }
static int  typical(int d, int g) { return BASE[g] * FAC[d][g] / 100; }
static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }

static void coin(void) { hit(81, INSTR_SQUARE, 3, 40); schedule(45, 88, INSTR_SQUARE, 3); }
static void cha(void)  { hit(88, INSTR_SQUARE, 3, 40); schedule(45, 81, INSTR_SQUARE, 3); }
static void nope(void) { note(45, INSTR_SQUARE, 2); }

static int qtyAmt(void) { switch (qtyMode) { case 0: return 1; case 1: return 5; case 2: return 10; default: return 99999; } }

static void reroll(void) {
    for (int d = 0; d < NPLACE; d++)
        for (int g = 0; g < NGOOD; g++) {
            int p = (int)(typical(d, g) * rnd_float_between(0.62f, 1.55f));
            if (chance(7)) p = p * 5 / 2;     // occasional sell spike
            if (chance(7)) p = p / 3;         // occasional buy dip
            price[d][g] = p < 1 ? 1 : p;
        }
}

static void buy(int g, int q) {
    int pr = price[here][g], room = capacity - units(), afford = pr > 0 ? cash / pr : 0;
    q = min(q, min(room, afford));
    if (q <= 0) { nope(); return; }
    cash -= q * pr; inv[g] += q; coin();
}
static void sell(int g, int q) {
    int pr = price[here][g];
    q = min(q, inv[g]);
    if (q <= 0) { nope(); return; }
    cash += q * pr; inv[g] -= q; cha();
}
static void deposit(int n)  { n = min(n, cash); if (n <= 0) { nope(); return; } cash -= n; bank += n; coin(); }
static void withdraw(int n) { n = min(n, bank); if (n <= 0) { nope(); return; } bank -= n; cash += n; coin(); }
static void payloan(int n)  { n = min(n, min(cash, debt)); if (n <= 0) { nope(); return; } cash -= n; debt -= n; coin(); }

static void set_card(int kind, int g, int amt, int bad) {
    ev_kind = kind; ev_g = g; ev_amt = amt; ev_bad = bad; ev_t = 0;
    if (bad) { shake(7); flashT = 0.5f; flashColor = CLR_RED; }
    else      { flashT = 0.35f; flashColor = CLR_GREEN; }
}

static void fire_event(void) {
    int r = rnd(100);
    if (r < 20 && cash > 0) {                                   // mugging
        int loss = cash * rnd_between(25, 55) / 100;
        cash -= loss; set_card(EV_MUG, -1, loss, 1);
    } else if (r < 38) {                                        // cop bust
        int held = -1, cnt = 0;
        for (int i = 0; i < NGOOD; i++) if (inv[i] > 0 && rnd(++cnt) == 0) held = i;
        if (held >= 0) { int lose = inv[held] / 2 + 1; if (lose > inv[held]) lose = inv[held]; inv[held] -= lose; set_card(EV_BUST, held, lose, 1); }
        else { int fine = min(cash, 150); cash -= fine; set_card(EV_BUST, -1, fine, 1); }
    } else if (r < 56) {                                        // price spike
        int g = rnd(NGOOD); price[here][g] = price[here][g] * rnd_between(28, 42) / 10;
        set_card(EV_SPIKE, g, 0, 0);
    } else if (r < 72) {                                        // price crash
        int g = rnd(NGOOD); price[here][g] = max(1, price[here][g] / 3);
        set_card(EV_CRASH, g, 0, 0);
    } else if (r < 86) {                                        // lucky find
        int g = rnd(NGOOD), room = capacity - units();
        int amt = min(room, rnd_between(4, 14));
        if (amt > 0) { inv[g] += amt; set_card(EV_FIND, g, amt, 0); }
        else { capacity += 30; set_card(EV_COAT, -1, 0, 0); }
    } else if (r < 93) {                                        // bigger coat
        capacity += 30; set_card(EV_COAT, -1, 0, 0);
    } else {                                                    // loan shark
        if (debt > 0) { int add = debt * rnd_between(10, 20) / 100; debt += add; set_card(EV_SHARK, -1, add, 1); }
        else { int g = rnd(NGOOD), room = capacity - units(), amt = min(room, rnd_between(4, 10));
               if (amt > 0) inv[g] += amt; set_card(EV_FIND, g, amt, 0); }
    }
}

static void do_travel(int d) {
    if (d == here) return;
    here = d; day++;
    if (debt > 0) debt += (debt + 9) / 10;          // ~10% interest
    reroll();
    for (int g = 0; g < NGOOD; g++) {               // arrows vs last seen here
        arrow[g] = seen[here][g] < 0 ? 0 : sgn(price[here][g] - seen[here][g]);
        seen[here][g] = price[here][g];
    }
    hit(40, INSTR_NOISE, 2, 150);
    if (day > 30) { state = OVER; over_saved = 0; return; }
    ev_kind = EV_NONE;
    if (chance(62)) { fire_event(); state = EVENT; }
    else state = MARKET;
}

static void enter_map(void) { state = MAP; modal = M_NONE; mapSel = (here + 1) % NPLACE; }
static int  other_sel(int dir) { int s = mapSel; for (int k = 0; k < NPLACE; k++) { s = (s + dir + NPLACE) % NPLACE; if (s != here) return s; } return mapSel; }

static void new_game(void) {
    cash = 2000; bank = 0; debt = 5500; day = 1; here = DOWNTOWN; capacity = 100;
    for (int i = 0; i < NGOOD; i++) inv[i] = 0;
    for (int d = 0; d < NPLACE; d++) for (int g = 0; g < NGOOD; g++) seen[d][g] = -1;
    reroll();
    for (int g = 0; g < NGOOD; g++) { arrow[g] = 0; seen[here][g] = price[here][g]; }
    sel = 0; qtyMode = 0; modal = M_NONE; showCash = 2000; over_saved = 0;
}

void init(void) {
    best = load(0);
    colorkey(0);
    instrument(5, INSTR_TRI, 300, 150, 3, 600); instrument_filter(5, FILTER_LOW, 700, 3);   // pad
    instrument(6, INSTR_SQUARE, 4, 80, 0, 120);  instrument_duty(6, 0.30f); instrument_filter(6, FILTER_LOW, 1500, 4); // pluck
    instrument(7, INSTR_SAW, 4, 60, 7, 60);       instrument_lfo(7, 0, LFO_PITCH, 6.0f, 7.0f); // siren
    for (int i = 0; i < NPED; i++) {
        ped[i].x = rnd(SCREEN_W); ped[i].vx = rnd_float_between(0.25f, 0.6f) * (rnd(2) ? 1 : -1);
        ped[i].type = rnd(2); ped[i].shirt = SHIRTS[rnd((int)(sizeof SHIRTS / sizeof *SHIRTS))];
        ped[i].phase = rnd_float();
    }
    new_game();
    state = TITLE;
}

static void play_bed(void) {
    bpm(BPMV[here]);
    if (every(8)) chord(ROOTV[here], CHTV[here], 5, 2);
    if (every(2) && chance(55)) tone(SCLV[here], 4, 6, 2);
}

// ===========================================================================
void update(void) {
    if (flashT > 0) flashT -= dt();
    float k = dt() * 60.0f;
    for (int i = 0; i < NPED; i++) {
        ped[i].x += ped[i].vx * k;
        if (ped[i].x < -16) ped[i].x = SCREEN_W;
        if (ped[i].x > SCREEN_W) ped[i].x = -16;
    }
    showCash = lerp(showCash, (float)cash, 0.18f);

    if (state == TITLE) { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) state = MARKET; return; }

    if (state == OVER) { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) { new_game(); state = TITLE; } return; }

    if (state == EVENT) {
        ev_t = clamp(ev_t + dt() * 4.0f, 0, 1);
        if (ev_kind == EV_BUST) {
            flashColor = (frame() / 6) % 2 ? CLR_RED : CLR_TRUE_BLUE; flashT = 0.35f;
            if (frame() % 18 == 0) note((frame() / 18) % 2 ? 77 : 70, 7, 4);
        }
        if (ev_t > 0.5f && (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || btnp(0, BTN_B) || keyp(KEY_ENTER) || keyp(KEY_SPACE)))
            state = MARKET;
        return;
    }

    if (state == MAP) {
        // node geometry mirrors draw_map_screen()
        static const int NX[NPLACE] = { 160, 55, 265, 80, 160, 255 };
        static const int NY[NPLACE] = { 55, 95, 75, 150, 125, 160 };
        for (int d = 0; d < NPLACE; d++)
            if (d != here && clicked(NX[d] - 26, NY[d] - 14, 52, 30)) { do_travel(d); return; }
        if (btnp(0, BTN_LEFT))  mapSel = other_sel(-1);
        if (btnp(0, BTN_RIGHT)) mapSel = other_sel(1);
        if (btnp(0, BTN_A) || keyp(KEY_ENTER)) { do_travel(mapSel); return; }
        if (keyp('M') || keyp(KEY_TAB) || keyp(KEY_ESCAPE)) state = MARKET;
        return;
    }

    // ---- MARKET ----
    play_bed();

    if (modal == M_BANK) {
        int c0 = MBX + 12, c1 = MBX + 82, c2 = MBX + 152, bw = 60, bh = 20, r1 = MBY + 38, r2 = MBY + 64;
        if (clicked(c0, r1, bw, bh)) deposit(100);
        if (clicked(c1, r1, bw, bh)) deposit(cash);
        if (clicked(c2, r1, bw, bh)) modal = M_NONE;
        if (clicked(c0, r2, bw, bh)) withdraw(100);
        if (clicked(c1, r2, bw, bh)) withdraw(bank);
        if (keyp(KEY_ESCAPE) || keyp('B')) modal = M_NONE;
        return;
    }
    if (modal == M_LOAN) {
        int c0 = MBX + 12, c1 = MBX + 82, c2 = MBX + 152, bw = 60, bh = 20, r1 = MBY + 44;
        if (clicked(c0, r1, bw, bh)) payloan(100);
        if (clicked(c1, r1, bw, bh)) payloan(min(cash, debt));
        if (clicked(c2, r1, bw, bh)) modal = M_NONE;
        if (keyp(KEY_ESCAPE) || keyp('L')) modal = M_NONE;
        return;
    }

    if (btnp(0, BTN_UP))   sel = (sel + NGOOD - 1) % NGOOD;
    if (btnp(0, BTN_DOWN)) sel = (sel + 1) % NGOOD;

    for (int i = 0; i < NGOOD; i++) {
        int ry = ROW0 + i * ROWH;
        if (hover(4, ry, 308, ROWH)) {
            if (mouse_pressed(MOUSE_LEFT))  { sel = i; buy(i, 1); }
            if (mouse_pressed(MOUSE_RIGHT)) { sel = i; sell(i, 1); }
        }
    }

    if (clicked(BX(0), BY, BW, BH)) buy(sel, qtyAmt());
    if (clicked(BX(1), BY, BW, BH)) sell(sel, qtyAmt());
    if (clicked(BX(2), BY, BW, BH)) qtyMode = (qtyMode + 1) % 4;
    if (clicked(BX(3), BY, BW, BH)) modal = M_BANK;
    if (clicked(BX(4), BY, BW, BH)) modal = M_LOAN;
    if (clicked(BX(5), BY, BW, BH)) enter_map();

    if (btnp(0, BTN_A)) buy(sel, qtyAmt());
    if (btnp(0, BTN_B)) sell(sel, qtyAmt());
    if (keyp('Q')) qtyMode = (qtyMode + 1) % 4;
    if (keyp('M') || keyp(KEY_TAB)) enter_map();
}

// ===========================================================================
static void draw_scene(void) {
    int ph = (day - 1) % 4;                                   // 0 dawn 1 day 2 dusk 3 night
    static const int SKY[4] = { CLR_DARK_PURPLE, CLR_BLUE, CLR_MAUVE, CLR_DARKER_BLUE };
    cls(SKY[ph]);

    // sun / moon
    int sunx = 30 + (day % 6) * 50, suny = 28;
    static const int ORB[4] = { CLR_ORANGE, CLR_YELLOW, CLR_DARK_PEACH, CLR_LIGHT_GREY };
    circfill(sunx, suny, 7, ORB[ph]);

    // windows are drawn in palette index 10 — dark by day, neon-lit otherwise
    pal(10, ph == 1 ? CLR_DARK_GREY : NEON[here]);
    map(0, here * 4, 0, SCENE_Y, 20, 4);
    pal_reset();

    // chinatown: blinking red lantern lights over the stalls
    if (here == CHINATOWN && blink(18))
        for (int x = 24; x < 320; x += 48) circfill(x, 60, 2, CLR_RED);

    // pedestrians on the ground row
    for (int i = 0; i < NPED; i++) {
        int ix = (int)ped[i].x, base = ped[i].type * 2;
        int fr = base + anim(2, 4.0f, ped[i].phase);
        pal(MAGIC_SHIRT, ped[i].shirt);
        sprf(fr, ix, 58, ped[i].vx < 0, false);
    }
    pal_reset();

    // dusk / night darken overlay (scene band only)
    if (ph == 2 || ph == 3) {
        fillp(ph == 3 ? FILL_CHECKER : FILL_DOTS, -1);
        rectfill(0, SCENE_Y, SCREEN_W, PANEL_Y - SCENE_Y, CLR_DARKER_BLUE);
        fillp_reset();
    }
}

static void arrow_at(int x, int y, int dir) {
    if (dir > 0)      trifill(x, y + 5, x + 6, y + 5, x + 3, y, CLR_RED);     // up = pricier
    else if (dir < 0) trifill(x, y, x + 6, y, x + 3, y + 5, CLR_GREEN);       // down = cheaper
}

static void draw_market(void) {
    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARKER_PURPLE);
    line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_DARK_PURPLE);

    print("MARKET", 8, 78, CLR_LIGHT_YELLOW);
    int coatcol = units() >= capacity ? CLR_RED : CLR_LIGHT_GREY;
    print_right(str("COAT %d/%d", units(), capacity), 200, 78, coatcol);
    print_right(str("BANK $%d", bank), 312, 78, CLR_GREEN);
    print("PRICE", 158, 88, CLR_DARK_GREY);
    print_right("OWN", 312, 88, CLR_DARK_GREY);

    for (int i = 0; i < NGOOD; i++) {
        int ry = ROW0 + i * ROWH, typ = typical(here, i), pr = price[here][i];
        if (i == sel) { rectfill(4, ry - 1, 308, ROWH, CLR_DARKER_GREY); rect(4, ry - 1, 308, ROWH, CLR_INDIGO); }
        print(GOOD[i], 10, ry + 2, CLR_WHITE);
        print_right(str("$%d", pr), 198, ry + 2, pr > typ * 14 / 10 ? CLR_GREEN : pr < typ * 7 / 10 ? CLR_ORANGE : CLR_LIGHT_GREY);
        arrow_at(202, ry + 2, arrow[i]);
        if (pr < typ * 6 / 10 && blink(20))      print("LOW", 214, ry + 2, CLR_ORANGE);
        else if (pr > typ * 15 / 10 && blink(20)) print("HI",  214, ry + 2, CLR_GREEN);
        print_right(str("x%d", inv[i]), 312, ry + 2, inv[i] ? CLR_WHITE : CLR_DARK_GREY);
    }

    // bottom action bar
    const char *qlab = qtyMode == 3 ? "MAX" : str("x%d", qtyAmt());
    const char *lab[6] = { "BUY", "SELL", qlab, "BANK", "LOAN", "TRAVEL" };
    int col[6] = { CLR_GREEN, CLR_RED, CLR_YELLOW, CLR_BLUE, CLR_ORANGE, CLR_LIGHT_YELLOW };
    for (int i = 0; i < 6; i++) {
        int x = BX(i); bool hv = hover(x, BY, BW, BH);
        rectfill(x, BY, BW, BH, hv ? CLR_DARK_GREY : CLR_BROWNISH_BLACK);
        rect(x, BY, BW, BH, col[i]);
        print(lab[i], x + (BW - text_width(lab[i])) / 2, BY + 5, hv ? CLR_WHITE : col[i]);
    }
}

static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showCash), 4, 2, CLR_YELLOW);
    print_centered(str("%s  DAY %d/30", PLACE[here], day), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    if (debt > 0) print_right(str("DEBT $%d", debt), 316, 2, blink(24) && debt > cash + bank ? CLR_WHITE : CLR_RED);
    else          print_right("DEBT CLEAR", 316, 2, CLR_GREEN);
}

static void draw_modal(void) {
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    rectfill(MBX, MBY, MBW, MBH, CLR_BLACK);
    rect(MBX, MBY, MBW, MBH, CLR_LIGHT_YELLOW);
    int c0 = MBX + 12, c1 = MBX + 82, c2 = MBX + 152, bw = 60, bh = 20;

    if (modal == M_BANK) {
        print("THE BANK", MBX + 12, MBY + 8, CLR_LIGHT_YELLOW);
        print(str("cash $%d", cash), MBX + 12, MBY + 20, CLR_YELLOW);
        print_right(str("safe in bank $%d", bank), MBX + MBW - 12, MBY + 20, CLR_GREEN);
        int r1 = MBY + 38, r2 = MBY + 64;
        const char *l[5] = { "DEP 100", "DEP ALL", "CLOSE", "WD 100", "WD ALL" };
        int bx[5] = { c0, c1, c2, c0, c1 }, by[5] = { r1, r1, r1, r2, r2 };
        for (int i = 0; i < 5; i++) { bool hv = hover(bx[i], by[i], bw, bh);
            rectfill(bx[i], by[i], bw, bh, hv ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(bx[i], by[i], bw, bh, CLR_GREEN);
            print(l[i], bx[i] + (bw - text_width(l[i])) / 2, by[i] + 6, CLR_WHITE); }
    } else {
        print("LOAN SHARK", MBX + 12, MBY + 8, CLR_LIGHT_YELLOW);
        print(str("debt $%d", debt), MBX + 12, MBY + 22, CLR_RED);
        print_right("grows 10%/day", MBX + MBW - 12, MBY + 22, CLR_DARK_GREY);
        int r1 = MBY + 44;
        const char *l[3] = { "PAY 100", "PAY ALL", "CLOSE" };
        int bx[3] = { c0, c1, c2 };
        for (int i = 0; i < 3; i++) { bool hv = hover(bx[i], r1, bw, bh);
            rectfill(bx[i], r1, bw, bh, hv ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(bx[i], r1, bw, bh, CLR_ORANGE);
            print(l[i], bx[i] + (bw - text_width(l[i])) / 2, r1 + 6, CLR_WHITE); }
    }
}

static void draw_map_screen(void) {
    static const int NX[NPLACE] = { 160, 55, 265, 80, 160, 255 };
    static const int NY[NPLACE] = { 55, 95, 75, 150, 125, 160 };
    cls(CLR_DARKER_BLUE);
    print_centered("CITY MAP", SCREEN_W/2, 18, CLR_LIGHT_YELLOW);
    print_centered("click a district  -  travel costs 1 day", SCREEN_W/2, 28, CLR_DARK_GREY);

    for (int i = 0; i < NPLACE; i++)                         // roads
        for (int j = i + 1; j < NPLACE; j++)
            line(NX[i], NY[i], NX[j], NY[j], CLR_DARKER_GREY);

    for (int d = 0; d < NPLACE; d++) {
        bool cur = d == here, hv = hover(NX[d] - 26, NY[d] - 14, 52, 30) || d == mapSel;
        int frame_c = cur ? CLR_YELLOW : hv ? CLR_WHITE : CLR_LIGHT_GREY;
        rectfill(NX[d] - 12, NY[d] - 10, 24, 14, NEON[d]);
        rect(NX[d] - 12, NY[d] - 10, 24, 14, frame_c);
        for (int wx = -8; wx <= 6; wx += 6) pset(NX[d] + wx, NY[d] - 4, CLR_BLACK);
        print(SHORT[d], NX[d] - text_width(SHORT[d]) / 2, NY[d] + 8, cur ? CLR_YELLOW : CLR_WHITE);
        if (cur) print(SHORT[d] && true ? "HERE" : "", NX[d] - text_width("HERE") / 2, NY[d] - 22, CLR_GREEN);
    }

    rectfill(0, 178, SCREEN_W, 22, CLR_BLACK);
    print(str("GO TO  %s", PLACE[mapSel]), 6, 182, CLR_LIGHT_YELLOW);
    print_right("ENTER travel   M back", 314, 182, CLR_DARK_GREY);
}

static void draw_event_card(void) {
    int y = (int)lerp(-70, 46, ease_out(ev_t));
    int w = 220, x = (SCREEN_W - w) / 2, h = 78;
    int bc = ev_bad ? CLR_RED : CLR_GREEN;
    rectfill(x, y, w, h, CLR_BLACK);
    rect(x, y, w, h, bc); rect(x + 2, y + 2, w - 4, h - 4, bc);

    const char *title = "", *b1 = "", *b2 = "";
    switch (ev_kind) {
        case EV_MUG:   title = "MUGGED!";   b1 = str("Thugs jump you in an alley."); b2 = str("Lost $%d.", ev_amt); break;
        case EV_BUST:  title = "BUSTED!";
            b1 = "Cops sweep the block!";
            b2 = ev_g >= 0 ? str("You ditch %d %s.", ev_amt, GOOD[ev_g]) : str("Pay a $%d fine.", ev_amt); break;
        case EV_SPIKE: title = "CRACKDOWN"; b1 = str("%s is suddenly scarce", GOOD[ev_g]); b2 = "...the price SOARS!"; break;
        case EV_CRASH: title = "MARKET GLUT"; b1 = str("%s floods the streets", GOOD[ev_g]); b2 = "...the price CRASHES."; break;
        case EV_FIND:  title = "LUCKY FIND"; b1 = "Score in a dumpster!"; b2 = str("Free %d %s.", ev_amt, GOOD[ev_g]); break;
        case EV_COAT:  title = "NEW COAT";   b1 = "You find a roomy trenchcoat."; b2 = "+30 carry space."; break;
        case EV_SHARK: title = "LOAN SHARK"; b1 = "The shark's goons find you."; b2 = str("Debt +$%d.", ev_amt); break;
    }
    print_centered(title, SCREEN_W/2, y + 8, ev_bad ? CLR_RED : CLR_LIGHT_YELLOW);
    print_centered(b1, SCREEN_W/2, y + 30, CLR_WHITE);
    print_centered(b2, SCREEN_W/2, y + 44, CLR_LIGHT_GREY);
    if (ev_t > 0.5f && blink(20)) print_centered("press Z", SCREEN_W/2, y + 62, CLR_DARK_GREY);
}

static void draw_over(void) {
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int net = cash + bank - debt;
    if (!over_saved) { if (net > best) { best = net; save(0, best); } over_saved = 1; }
    rectfill(50, 50, 220, 100, CLR_BLACK);
    rect(50, 50, 220, 100, CLR_LIGHT_YELLOW);
    print_centered("-- 30 DAYS UP --", SCREEN_W/2, 60, CLR_LIGHT_YELLOW);
    print_centered(str("cash $%d   bank $%d", cash, bank), SCREEN_W/2, 78, CLR_GREEN);
    print_centered(str("debt -$%d", debt), SCREEN_W/2, 90, CLR_RED);
    print_centered(str("NET WORTH  $%d", net), SCREEN_W/2, 108, net >= 0 ? CLR_YELLOW : CLR_RED);
    print_centered(str("best $%d", best), SCREEN_W/2, 122, CLR_LIGHT_GREY);
    print_centered("press Z to play again", SCREEN_W/2, 138, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_title(void) {
    cls(CLR_BROWNISH_BLACK);
    for (int i = 0; i < 40; i++) pset((i * 73) % SCREEN_W, (i * 91) % SCREEN_H, CLR_DARKER_GREY);
    print_scaled("DRUGLORD", (SCREEN_W - text_width("DRUGLORD") * 3) / 2, 44, CLR_GREEN, 3);
    print_centered("30 days. one debt. get rich.", SCREEN_W/2, 86, CLR_LIGHT_GREY);
    print_centered("buy low, sell high, dodge the cops", SCREEN_W/2, 100, CLR_DARK_GREY);
    print_centered(str("best net worth: $%d", best), SCREEN_W/2, 124, CLR_YELLOW);
    print_centered("press Z / click to deal", SCREEN_W/2, 150, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_flash(void) {
    if (flashT <= 0) return;
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, SCREEN_H, flashColor);
    fillp_reset();
}

void draw(void) {
    if (state == TITLE) { draw_title(); return; }
    if (state == MAP)   { draw_map_screen(); draw_hud(); return; }

    draw_scene();
    draw_market();
    draw_hud();
    if (modal != M_NONE) draw_modal();
    if (state == EVENT)  draw_event_card();
    draw_flash();
    if (state == OVER)   draw_over();
}
