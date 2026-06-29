/* de:meta
{
  "title": "pizza tycoon",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "schedule-driven-agents"
  ],
  "lineage": "Pizza Tycoon (1994) homage; novel in the district-taste / price-tolerance revenue formula and the procedurally-acting rival + mafia event-card system layered over a node city map.",
  "genre": "simulation",
  "homage": "Pizza Tycoon (1994)",
  "description": "Build a pizza empire across six districts with their own foot-traffic, rent and TASTE — each loves one topping and loathes another. Tune each shop's recipe on a live pizza you top slice-by-slice, set the price to the volume-vs-margin sweet spot, then hit NEXT DAY and read the ledger. A rival magnate keeps buying shops and undercutting you; shady MAFIA JOBS offer fast cash for rising HEAT (skip the protection money and a shop gets torched). pal()-recolored crowds, a day/night tint sweep, a jaunty cafe bed and an ominous mafia sting. Out-earn the rival by day 20. MOUSE: click a district (yours opens MANAGE, a free one shows BUY); toggle toppings, [-]/[+] or wheel for price, NEXT DAY to advance, ACCEPT/DECLINE the cards. KEYS: arrows pick, Z buy/manage, ENTER next day, 1-6 toppings, M back."
}
de:meta */
#include "studio.h"

// PIZZA TYCOON — a tune-and-tally business sim with a shady underbelly.
//
// Buy pizzerias across six districts, dial in each shop's TOPPINGS and PRICE,
// then click NEXT DAY and watch the ledger tally. Every district has its own
// foot-traffic, rent and TASTE (a topping it loves and one it loathes) — so the
// recipe that mints money in CAMPUS bombs in UPTOWN. Out-earn a rival magnate
// who keeps buying shops and undercutting you, and take the odd MAFIA JOB for
// fast cash (and rising HEAT) before day 20.
//
// It's menu + map driven: a node CITY MAP for buying/travel, a MANAGE screen
// with a live pizza you build topping-by-topping and a profit forecast, an
// end-of-day LEDGER, and slide-in mafia EVENT cards you resolve by clicking.
//
// MOUSE: click a district on the map (yours opens MANAGE, a free one shows BUY).
//   In MANAGE, click toppings to toggle, [-]/[+] or the wheel to set price.
//   NEXT DAY advances; resolve mafia cards with ACCEPT / DECLINE.
// KEYS: arrows pick a district, Z buy/manage, ENTER next day; in MANAGE 1-6
//   toggle toppings, LEFT/RIGHT price +-1, UP/DOWN +-5, M/ESC back.

// ---- toppings -------------------------------------------------------------
enum { PEP, MUSH, OLIVE, BASIL, ANCHOVY, TRUFFLE, NTOP };
static const char *TNAME[NTOP] = { "PEPPERONI", "MUSHROOM", "OLIVES", "BASIL", "ANCHOVY", "TRUFFLE" };
static const int   TAP[NTOP]   = { 3, 2, 1, 2, 1, 6 };   // appeal each adds
static const int   TCOST[NTOP] = { 2, 1, 1, 1, 2, 5 };   // ingredient cost each adds
static const int   TCOL[NTOP]  = { CLR_RED, CLR_LIGHT_GREY, CLR_DARKER_PURPLE, CLR_GREEN, CLR_MEDIUM_GREY, CLR_DARK_BROWN };

// ---- districts ------------------------------------------------------------
enum { L_ITALY, DOCKS, UPTOWN, CAMPUS, SUBURBS, PIAZZA, NDIST };
static const char *DNAME[NDIST]  = { "LITTLE ITALY", "THE DOCKS", "UPTOWN", "CAMPUS", "SUBURBS", "THE PIAZZA" };
static const char *DSHORT[NDIST] = { "L.ITALY", "DOCKS", "UPTOWN", "CAMPUS", "SUBURB", "PIAZZA" };
static const int   TRAF[NDIST]   = { 34, 24, 16, 40, 26, 30 };    // foot traffic (max customers ~)
static const int   RENT[NDIST]   = { 30, 12, 40, 20, 18, 35 };    // daily rent
static const int   TOL[NDIST]    = { 100, 85, 175, 68, 110, 130 };// price tolerance % (rich = high)
static const int   BUYC[NDIST]   = { 1200, 500, 1600, 800, 700, 1400 }; // cost to buy the shop
static const int   LOVED[NDIST]  = { BASIL, ANCHOVY, TRUFFLE, PEP, MUSH, OLIVE };
static const int   HATED[NDIST]  = { TRUFFLE, TRUFFLE, ANCHOVY, TRUFFLE, ANCHOVY, ANCHOVY };
static const int   NEON[NDIST]   = { CLR_RED, CLR_BLUE, CLR_MAUVE, CLR_GREEN, CLR_YELLOW, CLR_ORANGE };

// node positions on the city map (mirrored by update + draw_map)
static const int NX[NDIST] = { 70, 46, 268, 158, 256, 158 };
static const int NY[NDIST] = { 52, 138, 54, 150, 132, 86 };

// ---- per-shop state (indexed by district; valid when owner==0) -------------
static int   owner[NDIST];     // -1 free, 0 you, 1 rival
static int   recipe[NDIST];    // topping bitmask
static int   price[NDIST];
static float rep[NDIST];       // reputation 0..100

// ---- player / rival / clock ------------------------------------------------
static int   cash, heat, day, rcash;
static float showCash;         // rolls toward cash
static int   best;

// ---- ui / state ------------------------------------------------------------
enum { TITLE, MAP, MANAGE, LEDGER, EVENT, OVER };
static int   state, sel_d, manage_d, over_saved;

// ---- daily ledger snapshot -------------------------------------------------
static int   led_cust[NDIST], led_profit[NDIST];
static int   day_total;
static int   sabotaged[NDIST];     // rival promo halves customers next day
static int   rival_act, rival_d;   // 0 none, 1 bought rival_d, 2 undercut rival_d
static float ledger_t;             // ledger slide / tally timer

// ---- mafia event ----------------------------------------------------------
enum { JOB_DELIVERY, JOB_PROTECT, JOB_SABOTAGE, JOB_LAUNDER, JOB_RAID };
enum { OUT_NONE, OUT_OK, OUT_BUST, OUT_PAID, OUT_TORCH, OUT_SAFE, OUT_HIT, OUT_BROKE, OUT_CLEAN, OUT_DECLINE, OUT_RAIDED };
static int   ev_kind, ev_phase, ev_out, ev_cash, ev_dist;
static float ev_t;
static int   raid_queued;

// ---- pedestrians (city life, crowd.c-style pal recolor) --------------------
#define MAGIC_SHIRT 28
#define NPED 7
static const int SHIRTS[] = { 8, 9, 11, 12, 14, 26, 30, 7 };
typedef struct { float x, vx; int type, shirt; float phase; } Ped;
static Ped ped[NPED];

// ---- flash overlay ---------------------------------------------------------
static float flashT; static int flashColor;

// ---- layout ----------------------------------------------------------------
#define SCENE_Y 11
#define PANEL_Y 75

// ===========================================================================
static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }

static void ching(void) { hit(81, INSTR_SQUARE, 3, 40); schedule(50, 88, INSTR_SQUARE, 3); }
static void nope(void)  { note(45, INSTR_SQUARE, 2); }

static int my_shops(void)    { int n = 0; for (int d = 0; d < NDIST; d++) if (owner[d] == 0) n++; return n; }
static int rival_shops(void) { int n = 0; for (int d = 0; d < NDIST; d++) if (owner[d] == 1) n++; return n; }
static int net_worth(int who){ int n = (who == 0) ? cash : rcash; for (int d = 0; d < NDIST; d++) if (owner[d] == who) n += BUYC[d]; return n; }

// ---- the one legible revenue formula --------------------------------------
typedef struct { int customers, profit, appeal, fair, ingredient; } Sim;
static Sim sim_shop(int d, int pr, int mask, int sab, float swing) {
    int appeal = 4, ing = 3;                       // plain cheese base
    for (int t = 0; t < NTOP; t++) if (mask & (1 << t)) { appeal += TAP[t]; ing += TCOST[t]; }
    if (mask & (1 << LOVED[d])) appeal += 5;        // district's favourite
    if (mask & (1 << HATED[d])) appeal -= 4;        // district's pet hate
    if (appeal < 1) appeal = 1;

    int fair = appeal * 2 * TOL[d] / 100;           // what this district will happily pay
    if (fair < 1) fair = 1;
    float ratio = (float)pr / fair;                 // revenue peaks ~0.85*fair (volume vs margin)
    float pf = clamp(1.7f - ratio, 0.0f, 1.3f);
    float repb = 1.0f + rep[d] / 200.0f;            // loyalty boost up to +50%
    int cust = (int)(TRAF[d] * pf * repb * swing);
    if (sab) cust /= 2;                             // rival undercut
    if (cust < 0) cust = 0;
    int cap = TRAF[d] * 3 / 2; if (cust > cap) cust = cap;

    Sim s = { cust, cust * (pr - ing) - RENT[d], appeal, fair, ing };
    return s;
}

// ===========================================================================
// rival's whole turn: collect income, maybe expand, maybe undercut you
static void rival_turn(void) {
    rival_act = 0;
    for (int d = 0; d < NDIST; d++) if (owner[d] == 1) rcash += TRAF[d] * 16 - RENT[d];

    if (day >= 2) {                                  // expand into the best affordable free shop
        int best_d = -1;
        for (int d = 0; d < NDIST; d++)
            if (owner[d] < 0 && BUYC[d] <= rcash && (best_d < 0 || TRAF[d] > TRAF[best_d])) best_d = d;
        if (best_d >= 0 && chance(70)) { owner[best_d] = 1; rcash -= BUYC[best_d]; rival_act = 1; rival_d = best_d; }
    }
    if (rival_act == 0 && my_shops() > 0 && chance(26)) {   // undercut one of your shops tomorrow
        int picks[NDIST], np = 0;
        for (int d = 0; d < NDIST; d++) if (owner[d] == 0) picks[np++] = d;
        int d = picks[rnd(np)];
        sabotaged[d] = 1; rival_act = 2; rival_d = d;
    }
}

static void next_day(void) {
    day++;
    day_total = 0;
    float swing = 0.78f + noise(day * 0.41f) * 0.44f;     // shared demand mood for the day
    for (int d = 0; d < NDIST; d++) {
        led_cust[d] = led_profit[d] = 0;
        if (owner[d] != 0) continue;
        float sw = swing * (0.88f + noise(d * 2.3f + day * 0.7f) * 0.24f);
        Sim s = sim_shop(d, price[d], recipe[d], sabotaged[d], sw);
        led_cust[d] = s.customers; led_profit[d] = s.profit;
        cash += s.profit; day_total += s.profit;
        int target = 30 + (s.fair - price[d]) * 2 + (s.appeal - 8) * 4;   // value-for-money builds rep
        rep[d] += (clamp((float)target, 0, 100) - rep[d]) * 0.30f;
    }
    for (int d = 0; d < NDIST; d++) sabotaged[d] = 0;     // consumed; rival may re-arm below

    rival_turn();
    heat = max(0, heat - 5);

    raid_queued = 0;
    if (heat > 55 && chance(heat / 2)) raid_queued = 1;   // the law catches up

    ledger_t = 0;
    state = LEDGER;

    // ledger fanfare — a little ascending arpeggio
    if (day_total >= 0) { note(72, 5, 3); schedule(90, 76, 5, 3); schedule(180, 79, 5, 3); }
    else nope();
}

// ===========================================================================
static void start_event(int kind, int phase) {
    ev_kind = kind; ev_phase = phase; ev_out = OUT_NONE; ev_t = 0;
    if (kind == JOB_RAID) {
        ev_cash = max(150, cash / 5); cash -= ev_cash; heat = max(0, heat - 40);
        ev_out = OUT_RAIDED; shake(8); flashT = 0.5f; flashColor = CLR_TRUE_BLUE;
    } else {
        strum(38, CHORD_DIM, 7, 3, 70);              // ominous mafia sting
    }
}

static void resolve_accept(void) {
    switch (ev_kind) {
        case JOB_DELIVERY:
            if (chance(18)) { ev_cash = 200; cash -= ev_cash; heat += 30; ev_out = OUT_BUST;
                              shake(6); flashT = 0.4f; flashColor = CLR_RED; }
            else            { ev_cash = rnd_between(320, 620); cash += ev_cash; heat += 18; ev_out = OUT_OK; ching(); }
            break;
        case JOB_PROTECT:
            ev_cash = 250; cash -= ev_cash; ev_out = OUT_PAID; ching();
            break;
        case JOB_SABOTAGE: {
            if (cash < 150) { ev_out = OUT_BROKE; nope(); break; }
            int best_d = -1;
            for (int d = 0; d < NDIST; d++) if (owner[d] == 1 && (best_d < 0 || TRAF[d] > TRAF[best_d])) best_d = d;
            if (best_d < 0) { ev_out = OUT_BROKE; nope(); break; }
            cash -= 150; heat += 15; owner[best_d] = -1; ev_dist = best_d; ev_out = OUT_HIT;
            shake(4); note(40, INSTR_NOISE, 3); break;
        }
        case JOB_LAUNDER:
            if (cash < 300) { ev_out = OUT_BROKE; nope(); break; }
            ev_cash = 300; cash -= 300; heat = 0; ev_out = OUT_CLEAN; ching();
            break;
    }
    ev_phase = 1; ev_t = 0;
}

static void resolve_decline(void) {
    if (ev_kind == JOB_PROTECT && chance(55)) {      // refuse protection → an "accident"
        int picks[NDIST], np = 0;
        for (int d = 0; d < NDIST; d++) if (owner[d] == 0) picks[np++] = d;
        if (np > 0) { int d = picks[rnd(np)]; rep[d] *= 0.4f; ev_cash = 220; cash -= ev_cash; heat += 10;
                      ev_dist = d; ev_out = OUT_TORCH; shake(6); flashT = 0.4f; flashColor = CLR_DARK_ORANGE; }
        else { ev_out = OUT_SAFE; }
    } else if (ev_kind == JOB_PROTECT) {
        ev_out = OUT_SAFE;
    } else {
        ev_out = OUT_DECLINE;
    }
    ev_phase = 1; ev_t = 0;
}

// what happens after the player dismisses the ledger
static void after_ledger(void) {
    if (day > 20) { state = OVER; over_saved = 0; return; }
    if (raid_queued) { raid_queued = 0; start_event(JOB_RAID, 1); state = EVENT; return; }
    if (day >= 2 && chance(45)) {                    // a mafia job knocks
        int pool[4], np = 0;
        pool[np++] = JOB_DELIVERY;
        if (my_shops() > 0) pool[np++] = JOB_PROTECT;
        if (rival_shops() > 0) pool[np++] = JOB_SABOTAGE;
        if (heat >= 40) pool[np++] = JOB_LAUNDER;
        start_event(pool[rnd(np)], 0);
        state = EVENT; return;
    }
    state = MAP;
}

// ===========================================================================
static void new_game(void) {
    for (int d = 0; d < NDIST; d++) { owner[d] = -1; recipe[d] = 0; price[d] = 12; rep[d] = 30; sabotaged[d] = 0; }
    owner[DOCKS] = 0;  recipe[DOCKS] = 1 << ANCHOVY; price[DOCKS] = 14;   // a starter pizzeria
    owner[UPTOWN] = 1;                                                    // the rival's flagship
    cash = 600; heat = 0; day = 1; rcash = 900; showCash = 600;
    sel_d = DOCKS; over_saved = 0; rival_act = 0;
}

void init(void) {
    best = load(0);
    colorkey(0);
    bpm(112);
    instrument(5, INSTR_SQUARE, 2, 90, 1, 90);  instrument_duty(5, 0.30f);                       // mandolin pluck
    instrument(6, INSTR_TRI,    4, 70, 4, 90);                                                   // upright bass
    instrument(7, INSTR_SAW,   30, 220, 3, 320); instrument_filter(7, FILTER_LOW, 620, 6);
    instrument_lfo(7, 0, LFO_PITCH, 5.0f, 0.4f);                                                 // mafia menace
    for (int i = 0; i < NPED; i++) {
        ped[i].x = rnd(SCREEN_W); ped[i].vx = rnd_float_between(0.25f, 0.6f) * (rnd(2) ? 1 : -1);
        ped[i].type = rnd(2); ped[i].shirt = SHIRTS[rnd((int)(sizeof SHIRTS / sizeof *SHIRTS))];
        ped[i].phase = rnd_float();
    }
    new_game();
    state = TITLE;
}

// jaunty Italian-café bed — a lilting mandolin lead over a walking bass
static const int CAFE_MEL[16]  = { 72, 76, 79, 76, 74, 77, 74, 0, 71, 74, 79, 77, 76, 72, 74, 0 };
static const int CAFE_BASS[16] = { 48, 0, 55, 0, 50, 0, 57, 0, 47, 0, 55, 0, 48, 0, 43, 0 };
static int last_step = -1;
static void play_cafe(void) {
    int s16 = beat() * 2 + (int)(beat_pos() * 2);
    if (s16 == last_step) return;
    last_step = s16;
    int s = s16 % 16;
    if (CAFE_MEL[s])  hit(CAFE_MEL[s],  5, 3, 110);
    if (CAFE_BASS[s]) hit(CAFE_BASS[s], 6, 4, 120);
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

    if (state == TITLE) {
        if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) state = MAP;
        return;
    }
    if (state == OVER) {
        if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) { new_game(); state = TITLE; }
        return;
    }

    if (state == LEDGER) {
        ledger_t = clamp(ledger_t + dt() * 1.6f, 0, 2);
        if (ledger_t > 0.4f && (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER) || keyp(KEY_SPACE)))
            after_ledger();
        return;
    }

    if (state == EVENT) {
        ev_t = clamp(ev_t + dt() * 4.0f, 0, 1);
        if (ev_phase == 0) {
            // ACCEPT / DECLINE buttons (mirrors draw)
            if (clicked(56, 132, 90, 20) || btnp(0, BTN_A) || keyp('Z')) resolve_accept();
            if (clicked(174, 132, 90, 20) || btnp(0, BTN_B) || keyp('X')) resolve_decline();
        } else if (ev_t > 0.35f && (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER) || keyp(KEY_SPACE))) {
            if (day > 20) { state = OVER; over_saved = 0; } else state = MAP;
        }
        return;
    }

    if (state == MAP) {
        play_cafe();
        // click a district node
        for (int d = 0; d < NDIST; d++) {
            if (clicked(NX[d] - 24, NY[d] - 16, 48, 34)) {
                if (owner[d] == 0) { manage_d = d; state = MANAGE; return; }
                sel_d = d;
            }
        }
        // context button: BUY (free + affordable) or MANAGE (yours)
        if (owner[sel_d] == 0 && clicked(248, 162, 68, 16)) { manage_d = sel_d; state = MANAGE; return; }
        if (owner[sel_d] < 0 && clicked(248, 162, 68, 16)) {
            if (cash >= BUYC[sel_d]) { cash -= BUYC[sel_d]; owner[sel_d] = 0; price[sel_d] = 14; recipe[sel_d] = 0; rep[sel_d] = 30; ching(); }
            else nope();
        }
        if (clicked(248, 180, 68, 16)) { next_day(); return; }

        // keyboard
        if (btnp(0, BTN_RIGHT) || btnp(0, BTN_DOWN)) sel_d = (sel_d + 1) % NDIST;
        if (btnp(0, BTN_LEFT)  || btnp(0, BTN_UP))   sel_d = (sel_d + NDIST - 1) % NDIST;
        if (btnp(0, BTN_A) || keyp('Z')) {
            if (owner[sel_d] == 0) { manage_d = sel_d; state = MANAGE; }
            else if (owner[sel_d] < 0) { if (cash >= BUYC[sel_d]) { cash -= BUYC[sel_d]; owner[sel_d] = 0; price[sel_d] = 14; recipe[sel_d] = 0; rep[sel_d] = 30; ching(); } else nope(); }
        }
        if (keyp(KEY_ENTER)) { next_day(); return; }
        return;
    }

    // ---- MANAGE ----
    if (state == MANAGE) {
        play_cafe();
        int d = manage_d;
        // topping rows (mirror draw): x 100, y 92, rowh 13
        for (int t = 0; t < NTOP; t++) {
            int ry = 92 + t * 13;
            if (clicked(100, ry, 112, 13) || keyp('1' + t)) { recipe[d] ^= (1 << t); ching(); }
        }
        // price controls
        int w = (int)mouse_wheel();
        if (w > 0) price[d]++; else if (w < 0) price[d]--;
        if (clicked(232, 110, 16, 16)) price[d] -= (mouse_down(MOUSE_RIGHT) ? 5 : 1);
        if (clicked(300, 110, 16, 16)) price[d] += (mouse_down(MOUSE_RIGHT) ? 5 : 1);
        if (btnp(0, BTN_LEFT))  price[d]--;
        if (btnp(0, BTN_RIGHT)) price[d]++;
        if (btnp(0, BTN_DOWN))  price[d] -= 5;
        if (btnp(0, BTN_UP))    price[d] += 5;
        price[d] = mid(1, price[d], 99);
        // back
        if (clicked(4, 182, 60, 14) || keyp(KEY_ESCAPE) || keyp('M') || keyp(KEY_TAB)) state = MAP;
        return;
    }
}

// ===========================================================================
// shared day/night phase from the day counter — a slow tint sweep
static int phase_of_day(void) { return (day - 1) % 4; }
static const int SKY[4] = { CLR_DARK_PURPLE, CLR_BLUE, CLR_MAUVE, CLR_DARKER_BLUE };
static const int ORB[4] = { CLR_ORANGE, CLR_YELLOW, CLR_DARK_PEACH, CLR_LIGHT_GREY };

static void draw_scene(int d) {
    int ph = phase_of_day();
    cls(SKY[ph]);
    int sunx = 30 + (day % 6) * 50;
    circfill(sunx, 28, 7, ORB[ph]);

    pal(10, ph == 1 ? CLR_DARK_GREY : NEON[d]);      // windows: dark by day, neon-lit otherwise
    map(0, d * 4, 0, SCENE_Y, 20, 4);
    pal_reset();

    for (int i = 0; i < NPED; i++) {                 // pal()-recolored foot traffic
        int ix = (int)ped[i].x, base = ped[i].type * 2;
        int fr = base + anim(2, 4.0f, ped[i].phase);
        pal(MAGIC_SHIRT, ped[i].shirt);
        sprf(fr, ix, 58, ped[i].vx < 0, false);
    }
    pal_reset();

    if (ph == 2 || ph == 3) {                        // dusk/night darken band
        fillp(ph == 3 ? FILL_CHECKER : FILL_DOTS, -1);
        rectfill(0, SCENE_Y, SCREEN_W, PANEL_Y - SCENE_Y, CLR_DARKER_BLUE);
        fillp_reset();
    }
}

// the live pizza — crust, cheese, sauce, and topping marks per enabled topping
static void topping_marks(int cx, int cy, int r, int t) {
    int base = t * 41;
    for (int i = 0; i < 6; i++) {
        float a = base + i * 60 + t * 11;
        int rr = (int)(r * (0.30f + 0.16f * ((i + t) % 3)));
        int x = cx + (int)dx(rr, a), y = cy + (int)dy(rr, a);
        switch (t) {
            case PEP:     circfill(x, y, 2, CLR_RED); pset(x, y, CLR_DARK_RED); break;
            case MUSH:    ovalfill(x, y, 2, 1, CLR_LIGHT_GREY); pset(x, y, CLR_DARK_GREY); break;
            case OLIVE:   circfill(x, y, 1, CLR_BLACK); pset(x, y, CLR_DARK_PURPLE); break;
            case BASIL:   circfill(x, y, 1, CLR_GREEN); pset(x, y - 1, CLR_DARK_GREEN); break;
            case ANCHOVY: line(x - 1, y, x + 2, y, CLR_MEDIUM_GREY); break;
            case TRUFFLE: pset(x, y, CLR_DARK_BROWN); pset(x, y - 1, CLR_BROWN); pset(x + 1, y, CLR_BROWNISH_BLACK); break;
        }
    }
}
static void draw_pizza(int cx, int cy, int r, int mask) {
    circfill(cx, cy, r, CLR_BROWN);                  // crust
    circfill(cx, cy, r - 2, CLR_DARK_ORANGE);        // sauce rim
    circfill(cx, cy, r - 3, CLR_LIGHT_YELLOW);       // cheese
    for (int i = 0; i < 5; i++) {                    // a few sauce blotches
        int x = cx + (int)dx(r * 0.45f, i * 72 + 20), y = cy + (int)dy(r * 0.45f, i * 72 + 20);
        circfill(x, y, 2, CLR_ORANGE);
    }
    for (int t = 0; t < NTOP; t++) if (mask & (1 << t)) topping_marks(cx, cy, r, t);
}

static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showCash), 4, 2, CLR_YELLOW);
    print_centered(str("DAY %d/20   you %d  rival %d", day, my_shops(), rival_shops()), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    int hc = heat > 55 ? (blink(20) ? CLR_WHITE : CLR_RED) : heat > 30 ? CLR_ORANGE : CLR_DARK_GREY;
    print_right(str("HEAT %d", heat), 316, 2, hc);
}

// ---- the city map ----------------------------------------------------------
static void draw_map(void) {
    int ph = phase_of_day();
    cls(SKY[ph]);
    fillp(FILL_DOTS, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    // river through the docks corner
    fillp(FILL_CHECKER, -1); rectfill(0, 150, 96, 50, CLR_TRUE_BLUE); fillp_reset();

    for (int i = 0; i < NDIST; i++)                  // faint roads
        for (int j = i + 1; j < NDIST; j++)
            line(NX[i], NY[i], NX[j], NY[j], CLR_DARKER_GREY);

    for (int d = 0; d < NDIST; d++) {
        bool sel = d == sel_d;
        int fc = sel ? CLR_YELLOW : (owner[d] == 0 ? CLR_GREEN : owner[d] == 1 ? CLR_RED : CLR_LIGHT_GREY);
        rectfill(NX[d] - 14, NY[d] - 10, 28, 16, NEON[d]);
        rect(NX[d] - 14, NY[d] - 10, 28, 16, fc);
        for (int wx = -9; wx <= 7; wx += 5) pset(NX[d] + wx, NY[d] - 3, CLR_BLACK);  // windows
        // ownership pin
        if (owner[d] == 0)      { trifill(NX[d] - 4, NY[d] - 18, NX[d] + 6, NY[d] - 18, NX[d] + 1, NY[d] - 24, CLR_GREEN); rectfill(NX[d], NY[d] - 24, 1, 8, CLR_WHITE); }
        else if (owner[d] == 1) { trifill(NX[d] - 4, NY[d] - 18, NX[d] + 6, NY[d] - 18, NX[d] + 1, NY[d] - 24, CLR_RED);   rectfill(NX[d], NY[d] - 24, 1, 8, CLR_WHITE); }
        else if (blink(24))     print("$", NX[d] - 3, NY[d] - 22, CLR_LIGHT_YELLOW);
        print(DSHORT[d], NX[d] - text_width(DSHORT[d]) / 2, NY[d] + 8, sel ? CLR_YELLOW : CLR_WHITE);
    }
    if (rival_act == 1 && blink(18)) print_centered(str("RIVAL BOUGHT %s!", DSHORT[rival_d]), SCREEN_W/2, 150, CLR_RED);

    // detail strip + buttons
    rectfill(0, 158, 244, 42, CLR_BLACK); line(0, 158, 244, 158, CLR_DARK_GREY);
    int d = sel_d;
    print(DNAME[d], 6, 162, NEON[d]);
    print(str("traffic %d   rent $%d", TRAF[d], RENT[d]), 6, 174, CLR_LIGHT_GREY);
    print(str("loves %s", TNAME[LOVED[d]]), 6, 184, CLR_GREEN);
    print_right(str("hates %s", TNAME[HATED[d]]), 240, 184, CLR_RED);

    // context button
    const char *lab = owner[d] == 0 ? "MANAGE" : owner[d] == 1 ? "RIVAL'S" : str("BUY $%d", BUYC[d]);
    int lc = owner[d] == 0 ? CLR_GREEN : owner[d] == 1 ? CLR_DARK_GREY : (cash >= BUYC[d] ? CLR_LIGHT_YELLOW : CLR_RED);
    bool hv = hover(248, 162, 68, 16) && owner[d] != 1;
    rectfill(248, 162, 68, 16, hv ? CLR_DARK_GREY : CLR_BROWNISH_BLACK); rect(248, 162, 68, 16, lc);
    print(lab, 248 + (68 - text_width(lab)) / 2, 167, lc);

    bool hv2 = hover(248, 180, 68, 16);
    rectfill(248, 180, 68, 16, hv2 ? CLR_DARK_GREEN : CLR_DARK_PURPLE); rect(248, 180, 68, 16, CLR_WHITE);
    print("NEXT DAY", 248 + (68 - text_width("NEXT DAY")) / 2, 185, CLR_LIGHT_PEACH);
}

// ---- manage a shop ---------------------------------------------------------
static void draw_manage(void) {
    int d = manage_d;
    draw_scene(d);

    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARKER_PURPLE);
    line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_DARK_PURPLE);
    print(DNAME[d], 6, 78, NEON[d]);

    Sim s = sim_shop(d, price[d], recipe[d], 0, 1.0f);

    // live pizza + star rating
    int cx = 50, cy = 130, r = 30;
    draw_pizza(cx, cy, r, recipe[d]);
    int stars = mid(1, s.appeal / 4, 5);
    for (int i = 0; i < 5; i++) print("*", 22 + i * 9, 166, i < stars ? CLR_YELLOW : CLR_DARK_GREY);
    print(str("appeal %d", s.appeal), cx - text_width("appeal 00") / 2, 176, CLR_LIGHT_GREY);
    if ((recipe[d] & (1 << LOVED[d])) && s.appeal >= 14 && blink(16))
        print("BUONO!", cx - text_width("BUONO!") / 2, 92, CLR_LIME_GREEN);

    // topping list
    print("RECIPE", 100, 80, CLR_LIGHT_YELLOW);
    for (int t = 0; t < NTOP; t++) {
        int ry = 92 + t * 13;
        bool on = recipe[d] & (1 << t);
        bool hv = hover(100, ry, 112, 13);
        if (on) rectfill(100, ry, 112, 12, CLR_DARKER_GREY);
        if (hv) rect(100, ry, 112, 12, CLR_INDIGO);
        rectfill(103, ry + 3, 6, 6, TCOL[t]); rect(103, ry + 3, 6, 6, CLR_BLACK);
        int nc = t == LOVED[d] ? CLR_GREEN : t == HATED[d] ? CLR_RED : (on ? CLR_WHITE : CLR_LIGHT_GREY);
        print(TNAME[t], 113, ry + 2, nc);
        print_right(str("+%d $%d", TAP[t], TCOST[t]), 210, ry + 2, on ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }

    // price + forecast
    print("PRICE", 248, 80, CLR_LIGHT_YELLOW);
    rectfill(232, 110, 16, 16, hover(232, 110, 16, 16) ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(232, 110, 16, 16, CLR_RED);
    print("-", 238, 114, CLR_WHITE);
    rectfill(300, 110, 16, 16, hover(300, 110, 16, 16) ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(300, 110, 16, 16, CLR_GREEN);
    print("+", 305, 114, CLR_WHITE);
    print_scaled(str("$%d", price[d]), 256, 110, CLR_YELLOW, 2);
    int pcol = price[d] > s.fair ? CLR_ORANGE : CLR_GREEN;
    print_right(str("fair ~$%d", s.fair), 316, 130, pcol);
    print_right(str("cost/pie $%d", s.ingredient), 316, 140, CLR_LIGHT_GREY);
    print_right(str("~%d/day", s.customers), 316, 152, CLR_WHITE);
    int prc = s.profit >= 0 ? CLR_LIME_GREEN : CLR_RED;
    print_right(str("profit $%d", s.profit), 316, 164, prc);
    print("REP", 232, 176, CLR_DARK_GREY);
    bar(252, 176, 64, 7, rep[d] / 100.0f, CLR_PINK, CLR_DARKER_GREY);

    // back
    rectfill(4, 182, 60, 14, hover(4, 182, 60, 14) ? CLR_DARK_GREY : CLR_BROWNISH_BLACK); rect(4, 182, 60, 14, CLR_LIGHT_YELLOW);
    print("< MAP", 14, 185, CLR_LIGHT_YELLOW);
}

// ---- end-of-day ledger -----------------------------------------------------
static void draw_ledger(void) {
    draw_map();                                       // ledger overlays the city
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int x = 40, y = 24, w = 240, h = 152;
    rectfill(x, y, w, h, CLR_BLACK); rect(x, y, w, h, CLR_LIGHT_YELLOW);
    print_centered(str("DAY %d LEDGER", day - 1), SCREEN_W/2, y + 6, CLR_LIGHT_YELLOW);

    int ry = y + 22, shown = 0;
    for (int d = 0; d < NDIST; d++) if (owner[d] == 0) {
        print(DSHORT[d], x + 10, ry, CLR_WHITE);
        print(str("%d cust", led_cust[d]), x + 90, ry, CLR_LIGHT_GREY);
        print_right(str("$%d", led_profit[d]), x + w - 10, ry, led_profit[d] >= 0 ? CLR_LIME_GREEN : CLR_RED);
        ry += 12; shown++;
    }
    if (shown == 0) print_centered("(no shops earning yet)", SCREEN_W/2, ry, CLR_DARK_GREY);

    line(x + 8, ry + 2, x + w - 8, ry + 2, CLR_DARK_GREY);
    int tcol = day_total >= 0 ? CLR_YELLOW : CLR_RED;
    int tally = (int)lerp(0, (float)day_total, ease_out(clamp(ledger_t, 0, 1)));   // money rolls up
    print("DAY TOTAL", x + 10, ry + 8, CLR_WHITE);
    print_right(str("$%d", tally), x + w - 10, ry + 8, tcol);

    if (rival_act == 1) print_centered(str("rival opened in %s!", DSHORT[rival_d]), SCREEN_W/2, y + h - 30, CLR_RED);
    else if (rival_act == 2) print_centered(str("rival will undercut %s tomorrow!", DSHORT[rival_d]), SCREEN_W/2, y + h - 30, CLR_ORANGE);

    if (ledger_t > 0.4f && blink(20)) print_centered("click / Z to continue", SCREEN_W/2, y + h - 16, CLR_LIGHT_GREY);
}

// ---- mafia event card ------------------------------------------------------
static void draw_event(void) {
    if (state != EVENT) return;
    int yy = (int)lerp(-90, 40, ease_out(ev_t));
    int w = 240, x = (SCREEN_W - w) / 2, h = 120;
    bool bad = (ev_out == OUT_BUST || ev_out == OUT_TORCH || ev_out == OUT_RAIDED);
    int bc = ev_phase == 0 ? CLR_DARK_PURPLE : bad ? CLR_RED : CLR_GREEN;
    rectfill(x, yy, w, h, CLR_BLACK); rect(x, yy, w, h, bc); rect(x + 2, yy + 2, w - 4, h - 4, bc);

    const char *title = "", *l1 = "", *l2 = "";
    if (ev_kind == JOB_RAID) { title = "POLICE RAID"; l1 = "The heat finally boils over."; l2 = str("Fined $%d. Heat cooled.", ev_cash); }
    else switch (ev_kind) {
        case JOB_DELIVERY: title = "A LITTLE DELIVERY"; l1 = "\"Drive this box across town."; l2 = "No questions. Good money.\""; break;
        case JOB_PROTECT:  title = "PROTECTION";        l1 = "\"Nice ovens. Be a shame if"; l2 = "they had an accident.\""; break;
        case JOB_SABOTAGE: title = "A QUIET WORD";      l1 = "\"We can lean on your rival."; l2 = "Their newest shop... gone.\""; break;
        case JOB_LAUNDER:  title = "WASH IT CLEAN";     l1 = "\"We can make your record"; l2 = "disappear. For a price.\""; break;
    }
    print_centered(title, SCREEN_W/2, yy + 8, ev_phase == 0 ? CLR_LIGHT_YELLOW : bad ? CLR_RED : CLR_LIME_GREEN);
    print_centered(l1, SCREEN_W/2, yy + 26, CLR_WHITE);
    print_centered(l2, SCREEN_W/2, yy + 38, CLR_LIGHT_GREY);

    if (ev_phase == 0) {
        const char *a = ev_kind == JOB_DELIVERY ? "ACCEPT" : ev_kind == JOB_PROTECT ? "PAY $250"
                      : ev_kind == JOB_SABOTAGE ? "DO IT $150" : "LAUNDER $300";
        bool ha = hover(56, 132, 90, 20), hd = hover(174, 132, 90, 20);
        rectfill(56, 132, 90, 20, ha ? CLR_DARK_GREEN : CLR_DARKER_GREY); rect(56, 132, 90, 20, CLR_GREEN);
        print(a, 56 + (90 - text_width(a)) / 2, 138, CLR_WHITE);
        rectfill(174, 132, 90, 20, hd ? CLR_DARK_RED : CLR_DARKER_GREY); rect(174, 132, 90, 20, CLR_RED);
        print("DECLINE", 174 + (90 - text_width("DECLINE")) / 2, 138, CLR_WHITE);
        print_centered("Z accept   X decline", SCREEN_W/2, yy + h - 14, CLR_DARK_GREY);
    } else {
        const char *o = "";
        switch (ev_out) {
            case OUT_OK:      o = str("Paid $%d. Heat rising.", ev_cash); break;
            case OUT_BUST:    o = str("BUSTED! Lost $%d, heat +30.", ev_cash); break;
            case OUT_PAID:    o = "Paid up. The ovens stay safe."; break;
            case OUT_TORCH:   o = str("A shop torched! -$%d, rep hit.", ev_cash); break;
            case OUT_SAFE:    o = "You called their bluff. Lucky."; break;
            case OUT_HIT:     o = str("%s is back on the market.", DSHORT[ev_dist]); break;
            case OUT_BROKE:   o = "Not enough cash for that."; break;
            case OUT_CLEAN:   o = "Record wiped. Heat = 0."; break;
            case OUT_DECLINE: o = "You wave them off. For now."; break;
            case OUT_RAIDED:  o = str("Fined $%d. Heat cooled.", ev_cash); break;
        }
        print_centered(o, SCREEN_W/2, yy + 62, bad ? CLR_RED : CLR_LIGHT_YELLOW);
        if (ev_t > 0.35f && blink(20)) print_centered("click to continue", SCREEN_W/2, yy + h - 14, CLR_DARK_GREY);
    }
}

static void draw_flash(void) {
    if (flashT <= 0) return;
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, flashColor); fillp_reset();
}

static void draw_title(void) {
    cls(CLR_DARK_RED);
    fillp(FILL_DOTS, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BROWNISH_BLACK); fillp_reset();
    draw_pizza(160, 70, 38, (1 << PEP) | (1 << BASIL) | (1 << MUSH));
    print_scaled("PIZZA", (SCREEN_W - text_width("PIZZA") * 3) / 2, 118, CLR_LIGHT_YELLOW, 3);
    print_scaled("TYCOON", (SCREEN_W - text_width("TYCOON") * 2) / 2, 146, CLR_ORANGE, 2);
    print_centered("buy shops - tune the recipe - out-earn the rival", SCREEN_W/2, 168, CLR_LIGHT_GREY);
    print_centered(str("best empire: $%d", best), SCREEN_W/2, 180, CLR_YELLOW);
    print_centered("click / Z to open for business", SCREEN_W/2, 190, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_over(void) {
    draw_map();
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int me = net_worth(0), rv = net_worth(1);
    bool won = me >= rv;
    if (!over_saved) { if (me > best) { best = me; save(0, best); } over_saved = 1; }
    rectfill(50, 46, 220, 108, CLR_BLACK); rect(50, 46, 220, 108, won ? CLR_LIME_GREEN : CLR_RED);
    print_centered(won ? "YOU RULE THE CITY!" : "THE RIVAL WINS", SCREEN_W/2, 56, won ? CLR_LIME_GREEN : CLR_RED);
    print_centered(str("YOU    $%d  (%d shops)", me, my_shops()), SCREEN_W/2, 76, CLR_YELLOW);
    print_centered(str("RIVAL  $%d  (%d shops)", rv, rival_shops()), SCREEN_W/2, 90, CLR_LIGHT_GREY);
    print_centered(str("best empire: $%d", best), SCREEN_W/2, 112, CLR_GREEN);
    print_centered("click / Z to play again", SCREEN_W/2, 138, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

// ===========================================================================
void draw(void) {
    if (state == TITLE)  { draw_title();  return; }
    if (state == MAP)    { draw_map();   draw_hud(); return; }
    if (state == MANAGE) { draw_manage(); draw_hud(); return; }
    if (state == LEDGER) { draw_ledger(); draw_hud(); draw_flash(); return; }
    if (state == EVENT)  { draw_map(); draw_hud(); draw_event(); draw_flash(); return; }
    if (state == OVER)   { draw_over(); return; }
}
