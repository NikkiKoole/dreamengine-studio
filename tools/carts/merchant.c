/* de:meta
{
  "title": "Merchant",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop"
  ],
  "lineage": "Descends from Taipan! (1982); the novelty is a readable price-band market UI with per-port skew, daily drift, and rare shortage/glut events that visibly shift arbitrage.",
  "genre": "tycoon",
  "homage": "Taipan! (1982)",
  "description": "Age-of-sail buy-low/sell-high trading sim: 4 ports, 6 goods, daily price drift, shortages/gluts, storms & pirates, a ticking debt and a 40-day clock. MOUSE: click a good to BUY 1, right-click SELL 1; bottom bar BUY/SELL/QTY/DEBT/SET SAIL; click a port on the chart to sail. KEYS: up/down pick a good, Z buy / X sell, Q cycle qty, D moneylender, M or ENTER open the chart (left/right pick port, ENTER sails).",
  "todo": [
    "Clicking a product (spice/silk) shouldn't buy it — that's the Buy button's job.",
    "The houses need some work."
  ]
}
de:meta */
#include "studio.h"

// MERCHANT — a clean age-of-sail buy-low / sell-high trading sim.
//
// Four ports ring a small sea. Six goods drift in price every day; now and
// then a SHORTAGE spikes a good at one port or a GLUT craters another. Buy
// cheap, carry it in a limited hold, sail to where it sells dear. Each voyage
// burns days. Sometimes a STORM or PIRATES on the crossing costs you cargo.
// A moneylender's debt grows each day. Reach the wealth GOAL before the season
// runs out to retire a merchant prince — run out of days and the books close.
//
// The market core is the whole point: a per-port price skew + a daily drift +
// rare events, all readable at a glance (arrows, BUY!/SELL! tags, a price band).
//
// MOUSE: click a good row to BUY 1 (right-click SELL 1); bottom bar
//   BUY / SELL / QTY / DEBT / SET SAIL; on the chart, click a port to sail.
// KEYS: up/down pick a good, Z buy / X sell, Q cycle qty, D moneylender,
//   M or ENTER open the chart; on the chart left/right pick a port, ENTER sails.

// ---- goods -----------------------------------------------------------------
enum { SPICE, SILK, RUM, IRON, TEA, POWDER, NGOOD };
static const char *GOOD[NGOOD] = { "SPICE", "SILK", "RUM", "IRON", "TEA", "GUNPOWDER" };
static const int   BASE[NGOOD] = {   60,    150,    45,   90,   75,    200    };

// ---- ports -----------------------------------------------------------------
enum { HARBORTON, SALTMARSH, GOLDHAVEN, EASTREACH, NPORT };
static const char *PORT[NPORT]  = { "HARBORTON", "SALTMARSH", "GOLDHAVEN", "EASTREACH" };
static const char *SHORT[NPORT] = { "HARBORTON", "SALTMARSH", "GOLDHAVEN", "EASTREACH" };

// chart node pixel centres (a port at each corner of the sea)
static const int NX[NPORT] = {  58, 262,  58, 262 };
static const int NY[NPORT] = {  60,  60, 150, 150 };

// price level per port x good as a percent of base — each port's "flavour":
// where a good is cheap to BUY (<100) and where it sells dear (>100). Built so
// every port both buys something cheap and sells something dear (clear arbitrage).
static const int FAC[NPORT][NGOOD] = {
//    SPICE SILK  RUM  IRON  TEA  PWDR
    {  60, 150,  90, 130, 110, 120 },  // HARBORTON — spice source; sells iron/silk dear
    { 145,  90, 130,  55,  95, 110 },  // SALTMARSH — iron source; sells spice/rum dear
    { 130, 120,  55, 110,  95, 150 },  // GOLDHAVEN — rum source; sells powder/spice dear
    {  95,  60, 115, 100, 150,  60 },  // EASTREACH — silk & powder source; sells tea dear
};

// per-port harbour mood: accent colour + island fill colour
static const int ACCENT[NPORT] = { CLR_YELLOW, CLR_GREEN, CLR_ORANGE, CLR_PINK };
static const int LAND[NPORT]   = { CLR_DARK_GREEN, CLR_BROWN, CLR_DARK_ORANGE, CLR_DARK_PURPLE };

// ---- season / goal ---------------------------------------------------------
#define SEASON 40
#define GOAL   6000
#define START_GOLD 1000
#define START_DEBT 1500
#define START_HOLD 50

// ---- state -----------------------------------------------------------------
enum { TITLE, MARKET, MAP, SAIL, EVENT, OVER };
static int state;

static int   price[NPORT][NGOOD];   // today's prices everywhere
static int   seen[NPORT][NGOOD];    // last price the player saw at each port
static int   arrow[NGOOD];          // -1 down / 0 / +1 up vs last seen here
static int   cargo[NGOOD];

static int   gold, day, here, capacity, debt;
static int   sel, qtyMode, modal, best, over_saved, won;
static float showGold;              // rolls toward gold for a counting HUD

// voyage
static int   sail_dest, sail_days;
static float voyage, shipx, shipy;

// event card (resolved when a voyage arrives)
enum { EV_NONE, EV_STORM, EV_PIRATE, EV_SHORTAGE, EV_GLUT, EV_FAIR };
static int   ev_kind, ev_g, ev_amt, ev_bad;
static float ev_t;

// banner + flash juice
static char  bannerBuf[48];
static float bannerT, flashT;
static int   bannerCol, flashColor;

// ---- particles (wake / spray) ----------------------------------------------
#define NPART 90
typedef struct { float x, y, vx, vy, life; int col; } Part;
static Part part[NPART];

// ---- layout ----------------------------------------------------------------
#define PANEL_Y 80
#define ROW0    102
#define ROWH    11
#define BY      174
#define BH      20
#define BW      58
#define BX(i)   (5 + (i) * 63)
#define NBTN    5

// ===========================================================================
static float fabsf2(float v) { return v < 0 ? -v : v; }
static int   units(void) { int t = 0; for (int i = 0; i < NGOOD; i++) t += cargo[i]; return t; }
static int   typical(int d, int g) { return BASE[g] * FAC[d][g] / 100; }
static bool  hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool  clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }
static int   qtyAmt(void) { switch (qtyMode) { case 0: return 1; case 1: return 5; case 2: return 10; default: return 99999; } }

// ---- audio stings ----------------------------------------------------------
static void coin(void)    { hit(81, 6, 3, 40); schedule(45, 88, 6, 3); }
static void chime(void)   { hit(88, 6, 3, 40); schedule(45, 81, 6, 3); }
static void nope(void)    { note(45, INSTR_SQUARE, 2); }
static void gull(void)    { hit(95, INSTR_SQUARE, 1, 40); schedule(70, 99, INSTR_SQUARE, 1); }
static void thunder(void) { hit(24, INSTR_NOISE, 5, 350); }

static void banner(const char *t, int c) {
    int i = 0; for (; t[i] && i < 47; i++) bannerBuf[i] = t[i];
    bannerBuf[i] = 0; bannerT = 0; bannerCol = c;
}
static void flash(int c, float t) { flashColor = c; flashT = t; }

static void spawn(float x, float y, float vx, float vy, float life, int col) {
    for (int i = 0; i < NPART; i++) if (part[i].life <= 0) {
        part[i].x = x; part[i].y = y; part[i].vx = vx; part[i].vy = vy;
        part[i].life = life; part[i].col = col; return;
    }
}

// ===========================================================================
// the market core — the reusable engine bit. price formation lives here.
// ===========================================================================
static void reroll(void) {
    for (int d = 0; d < NPORT; d++)
        for (int g = 0; g < NGOOD; g++) {
            // daily drift: today's price wobbles around the port's typical level
            int p = (int)(typical(d, g) * rnd_float_between(0.70f, 1.45f));
            price[d][g] = p < 1 ? 1 : p;
        }
}
static void buy(int g, int q) {
    int pr = price[here][g], room = capacity - units(), afford = pr > 0 ? gold / pr : 0;
    q = min(q, min(room, afford));
    if (q <= 0) { nope(); return; }
    gold -= q * pr; cargo[g] += q; coin();
}
static void sell(int g, int q) {
    int pr = price[here][g];
    q = min(q, cargo[g]);
    if (q <= 0) { nope(); return; }
    gold += q * pr; cargo[g] -= q; chime();
}
static int drop_cargo(int n) {                  // lose n random units (storm/pirates)
    int dropped = 0;
    for (int k = 0; k < n; k++) {
        int held = -1, cnt = 0;
        for (int i = 0; i < NGOOD; i++) if (cargo[i] > 0 && rnd(++cnt) == 0) held = i;
        if (held < 0) break;
        cargo[held]--; dropped++;
    }
    return dropped;
}

// ---- moneylender -----------------------------------------------------------
static void payloan(int n) { n = min(n, min(gold, debt)); if (n <= 0) { nope(); return; } gold -= n; debt -= n; coin(); }

// ===========================================================================
// events — fired on arrival. transit risks (storm/pirate) cost cargo; market
// events (shortage/glut/fair) reshape THIS port's prices so arbitrage shifts.
// ===========================================================================
static void set_card(int kind, int g, int amt, int bad) {
    ev_kind = kind; ev_g = g; ev_amt = amt; ev_bad = bad; ev_t = 0;
    if (bad) { shake(7); flash(CLR_RED, 0.45f); thunder(); }
    else     { flash(CLR_GREEN, 0.3f); chime(); }
}

static void fire_event(void) {
    int r = rnd(100);
    if (r < 16 && units() > 0) {                                  // STORM — lose cargo to the waves
        int lost = drop_cargo(rnd_between(2, 5));
        set_card(EV_STORM, -1, lost, 1);
    } else if (r < 30 && units() > 0) {                           // PIRATES — they board you
        int lost = drop_cargo(rnd_between(3, 7));
        set_card(EV_PIRATE, -1, lost, 1);
    } else if (r < 58) {                                          // SHORTAGE — a good spikes here
        int g = rnd(NGOOD);
        price[here][g] = price[here][g] * rnd_between(25, 40) / 10;
        set_card(EV_SHORTAGE, g, 0, 0);
    } else if (r < 86) {                                          // GLUT — a good craters here
        int g = rnd(NGOOD);
        price[here][g] = max(1, price[here][g] / 3);
        set_card(EV_GLUT, g, 0, 0);
    } else {                                                      // TRADE FAIR — free crate of a good
        int g = rnd(NGOOD), room = capacity - units();
        int amt = min(room, rnd_between(3, 8));
        if (amt > 0) { cargo[g] += amt; set_card(EV_FAIR, g, amt, 0); }
        else { int g2 = rnd(NGOOD); price[here][g2] = max(1, price[here][g2] / 3); set_card(EV_GLUT, g2, 0, 0); }
    }
}

// ===========================================================================
// voyage / arrival
// ===========================================================================
static void check_season(void) {
    if (day > SEASON) { won = (gold - debt >= GOAL); state = OVER; over_saved = 0; }
}
static void arrive(void) {
    here = sail_dest; day += sail_days;
    if (debt > 0) debt += (debt * sail_days + 19) / 20;       // ~5%/day interest
    reroll();
    for (int g = 0; g < NGOOD; g++) {                         // arrows vs last seen here
        arrow[g] = seen[here][g] < 0 ? 0 : sgn(price[here][g] - seen[here][g]);
    }
    gull(); flash(CLR_LIGHT_PEACH, 0.2f);
    if (day > SEASON) { check_season(); return; }
    ev_kind = EV_NONE;
    if (chance(60)) { fire_event(); state = EVENT; }
    else            { state = MARKET; }
    for (int g = 0; g < NGOOD; g++) seen[here][g] = price[here][g];  // remember post-event prices
}

static void set_sail(int dest) {
    if (dest == here) return;
    sail_dest = dest;
    float d = distance(NX[here], NY[here], NX[dest], NY[dest]);
    sail_days = max(1, (int)(d / 130.0f + 0.5f));            // diagonal legs cost 2 days, edges 1
    voyage = 0; shipx = NX[here]; shipy = NY[here];
    state = SAIL;
}

// ===========================================================================
static void new_game(void) {
    gold = START_GOLD; capacity = START_HOLD; debt = START_DEBT;
    day = 1; here = HARBORTON;
    for (int i = 0; i < NGOOD; i++) cargo[i] = 0;
    for (int d = 0; d < NPORT; d++) for (int g = 0; g < NGOOD; g++) seen[d][g] = -1;
    reroll();
    for (int g = 0; g < NGOOD; g++) { arrow[g] = 0; seen[here][g] = price[here][g]; }
    sel = 0; qtyMode = 0; modal = 0; showGold = gold; over_saved = won = 0;
    bannerT = 99;
}

void init(void) {
    best = load(0);
    // shanty pad (accordion-ish) + a pluck for melody/coins
    instrument(5, INSTR_SAW, 120, 200, 4, 500); instrument_filter(5, FILTER_LOW, 900, 3);
    instrument_lfo(5, 0, LFO_PITCH, 5.0f, 0.3f);
    instrument(6, INSTR_SQUARE, 4, 70, 0, 90); instrument_duty(6, 0.45f);
    new_game();
    state = TITLE;
}

// ===========================================================================
static void update_parts(void) {
    for (int i = 0; i < NPART; i++) if (part[i].life > 0) {
        part[i].x += part[i].vx * dt();
        part[i].y += part[i].vy * dt();
        part[i].life -= dt();
    }
}
static void draw_parts(void) {
    for (int i = 0; i < NPART; i++) if (part[i].life > 0)
        pset((int)part[i].x, (int)part[i].y, part[i].col);
}

static void play_shanty(void) {
    bpm(96);
    if (every(4)) chord(48, CHORD_MAJ7, 5, 2);
    if (every(2) && chance(45)) tone(SCALE_PENTA, 5, 6, 2);
}

// ---- the sailing animation -------------------------------------------------
static void update_sail(void) {
    voyage += dt() / 1.8f;
    shipx = lerp(NX[here], NX[sail_dest], clamp(voyage, 0, 1));
    shipy = lerp(NY[here], NY[sail_dest], clamp(voyage, 0, 1));
    if (chance(55)) spawn(shipx + rnd_between(-2, 3), shipy + 4 + rnd_between(-2, 3),
                          rnd_float_between(-6, 6), rnd_float_between(-4, 4), 0.5f, CLR_WHITE);
    if (frame() % 90 == 0) gull();
    if (frame() % 55 == 0) hit(36, INSTR_NOISE, 1, 200);     // wave wash
    if (voyage >= 1.0f) arrive();
}

// ===========================================================================
void update(void) {
    if (bannerT < 90) bannerT += dt();
    if (flashT > 0)   flashT -= dt();
    update_parts();
    showGold = lerp(showGold, (float)gold, 0.18f);

    if (state == TITLE) { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) state = MARKET; return; }
    if (state == OVER)  { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) { new_game(); state = TITLE; } return; }
    if (state == SAIL)  { update_sail(); return; }

    if (state == EVENT) {
        ev_t = clamp(ev_t + dt() * 4.0f, 0, 1);
        if (ev_t > 0.5f && (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || btnp(0, BTN_B) || keyp(KEY_ENTER) || keyp(KEY_SPACE))) {
            state = MARKET; check_season();
        }
        return;
    }

    if (state == MAP) {
        for (int d = 0; d < NPORT; d++)
            if (d != here && clicked(NX[d] - 26, NY[d] - 16, 52, 34)) { set_sail(d); return; }
        if (btnp(0, BTN_LEFT))  { do { sel = (sel + NPORT - 1) % NPORT; } while (sel == here); }
        if (btnp(0, BTN_RIGHT)) { do { sel = (sel + 1) % NPORT; } while (sel == here); }
        if (btnp(0, BTN_A) || keyp(KEY_ENTER)) { set_sail(sel); return; }
        if (keyp('M') || keyp(KEY_TAB) || keyp(KEY_ESCAPE)) state = MARKET;
        return;
    }

    // ---- MARKET ----
    play_shanty();

    if (modal) {                                             // moneylender
        int bx = 64, bw = 192, bh = 20;
        int y0 = 92, y1 = 118, y2 = 144;
        if (clicked(bx, y0, bw, bh)) payloan(100);
        if (clicked(bx, y1, bw, bh)) payloan(min(gold, debt));
        if (clicked(bx, y2, bw, bh) || keyp('D') || keyp(KEY_ESCAPE)) modal = 0;
        return;
    }

    if (btnp(0, BTN_UP))   sel = (sel + NGOOD - 1) % NGOOD;
    if (btnp(0, BTN_DOWN)) sel = (sel + 1) % NGOOD;

    for (int i = 0; i < NGOOD; i++) {
        int ry = ROW0 + i * ROWH;
        if (hover(4, ry, 312, ROWH)) {
            if (mouse_pressed(MOUSE_LEFT))  { sel = i; buy(i, 1); }
            if (mouse_pressed(MOUSE_RIGHT)) { sel = i; sell(i, 1); }
        }
    }

    if (clicked(BX(0), BY, BW, BH)) buy(sel, qtyAmt());
    if (clicked(BX(1), BY, BW, BH)) sell(sel, qtyAmt());
    if (clicked(BX(2), BY, BW, BH)) qtyMode = (qtyMode + 1) % 4;
    if (clicked(BX(3), BY, BW, BH)) modal = 1;
    if (clicked(BX(4), BY, BW, BH)) { state = MAP; sel = (here + 1) % NPORT; }

    if (btnp(0, BTN_A)) buy(sel, qtyAmt());
    if (btnp(0, BTN_B)) sell(sel, qtyAmt());
    if (keyp('Q')) qtyMode = (qtyMode + 1) % 4;
    if (keyp('D')) modal = 1;
    if (keyp('M') || keyp(KEY_TAB) || keyp(KEY_ENTER)) { state = MAP; sel = (here + 1) % NPORT; }
}

// ===========================================================================
// drawing
// ===========================================================================
static void draw_ship(float cx, float cy, float scl, bool faceLeft) {
    // a tiny primitive caravel: hull + two sails + mast
    float w = 9 * scl, h = 5 * scl;
    int hx = (int)(cx - w), hy = (int)cy;
    trifill((int)(cx - w), hy, (int)(cx + w), hy, (int)(cx + w - 3 * scl), (int)(cy + h), CLR_BROWN);
    trifill((int)(cx - w), hy, (int)(cx - w + 3 * scl), (int)(cy + h), (int)(cx + w - 3 * scl), (int)(cy + h), CLR_BROWN);
    int mx = (int)cx;
    line(mx, hy, mx, (int)(cy - h * 2.4f), CLR_DARK_BROWN);                 // mast
    int sw = (int)(5 * scl), sh = (int)(4 * scl), sy = (int)(cy - h * 2.2f);
    int dir = faceLeft ? -1 : 1;
    trifill(mx, sy, mx + dir * sw, sy + sh / 2, mx, sy + sh, CLR_WHITE);    // sail
    trifill(mx, (int)(cy - h * 0.9f), mx + dir * sw, (int)(cy - h * 0.4f), mx, (int)(cy - h * 0.1f), CLR_LIGHT_PEACH);
    (void)hx;
}

// --- harbour backdrop for the MARKET ---
static void draw_harbor(void) {
    int ph = (day - 1) * 4 / SEASON; if (ph > 3) ph = 3;     // 0 morning .. 3 dusk
    static const int SKY[4] = { CLR_BLUE, CLR_BLUE, CLR_INDIGO, CLR_MAUVE };
    cls(SKY[ph]);
    static const int ORB[4] = { CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_PEACH };
    circfill(30 + day * 250 / SEASON, 20 + ph * 5, 7, ORB[ph]);            // sun lowers across season

    // sea band
    rectfill(0, 52, SCREEN_W, PANEL_Y - 52, CLR_TRUE_BLUE);
    fillp(FILL_HLINES, -1);
    rectfill(0, 62, SCREEN_W, PANEL_Y - 62, CLR_DARK_BLUE);
    fillp_reset();

    // the port town on the right: a little skyline of houses in the port's land colour
    int baseY = 52;
    rectfill(180, baseY - 2, SCREEN_W - 180, 4, CLR_LIGHT_PEACH);          // quay
    for (int i = 0; i < 5; i++) {
        int x = 192 + i * 26, hh = 14 + (i * 7 % 12);
        rectfill(x, baseY - hh, 18, hh, LAND[here]);
        rect(x, baseY - hh, 18, hh, CLR_BROWNISH_BLACK);
        // lit windows in the port accent
        for (int wy = baseY - hh + 3; wy < baseY - 3; wy += 6)
            pset(x + 4, wy, ACCENT[here]), pset(x + 12, wy, ACCENT[here]);
    }
    print(PORT[here], 184, baseY - 46, ACCENT[here]);

    // gulls
    for (int i = 0; i < 3; i++) {
        int gx = (int)(60 + i * 70 + sin_deg(now() * 30 + i * 90) * 30);
        int gy = 20 + i * 5 + (int)(sin_deg(now() * 90 + i * 60) * 2);
        line(gx, gy, gx + 3, gy - 2, CLR_WHITE); line(gx + 3, gy - 2, gx + 6, gy, CLR_WHITE);
    }
    // your ship docked, bobbing
    draw_ship(70, 56 + sin_deg(now() * 100) * 1.2f, 1.6f, false);
    draw_parts();
}

static void arrow_at(int x, int y, int dir) {
    if (dir > 0)      trifill(x, y + 5, x + 6, y + 5, x + 3, y, CLR_RED);     // up = pricier
    else if (dir < 0) trifill(x, y, x + 6, y, x + 3, y + 5, CLR_GREEN);       // down = cheaper
}

static void draw_market_panel(void) {
    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARKER_BLUE);
    line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_DARK_BLUE);

    print("CARGO MARKET", 8, 84, CLR_LIGHT_YELLOW);
    int holdc = units() >= capacity ? CLR_RED : CLR_LIGHT_GREY;
    print_right(str("HOLD %d/%d", units(), capacity), 312, 84, holdc);
    print("PRICE", 196, 94, CLR_DARK_GREY);
    print_right("HOLD", 312, 94, CLR_DARK_GREY);

    for (int i = 0; i < NGOOD; i++) {
        int ry = ROW0 + i * ROWH, typ = typical(here, i), pr = price[here][i];
        if (i == sel) { rectfill(4, ry - 1, 312, ROWH, CLR_DARKER_GREY); rect(4, ry - 1, 312, ROWH, CLR_INDIGO); }
        print(GOOD[i], 10, ry + 2, CLR_WHITE);
        int pc = pr > typ * 14 / 10 ? CLR_RED : pr < typ * 7 / 10 ? CLR_GREEN : CLR_LIGHT_GREY;
        print_right(str("$%d", pr), 236, ry + 2, pc);
        arrow_at(240, ry + 2, arrow[i]);
        if (pr < typ * 6 / 10 && blink(20))       print("BUY!",  250, ry + 2, CLR_GREEN);
        else if (pr > typ * 15 / 10 && blink(20)) print("SELL!", 250, ry + 2, CLR_RED);
        print_right(str("x%d", cargo[i]), 312, ry + 2, cargo[i] ? CLR_WHITE : CLR_DARK_GREY);
    }

    const char *qlab = qtyMode == 3 ? "MAX" : str("x%d", qtyAmt());
    const char *lab[NBTN] = { "BUY", "SELL", qlab, "DEBT", "SET SAIL" };
    int col[NBTN] = { CLR_GREEN, CLR_RED, CLR_YELLOW, CLR_ORANGE, CLR_LIGHT_YELLOW };
    for (int i = 0; i < NBTN; i++) {
        int x = BX(i); bool hv = hover(x, BY, BW, BH);
        rectfill(x, BY, BW, BH, hv ? CLR_DARK_GREY : CLR_BROWNISH_BLACK);
        rect(x, BY, BW, BH, col[i]);
        print(lab[i], x + (BW - text_width(lab[i])) / 2, BY + 6, hv ? CLR_WHITE : col[i]);
    }
}

static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showGold), 4, 2, CLR_YELLOW);
    print_centered(str("%s   DAY %d/%d", PORT[here], day, SEASON), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    if (debt > 0) print_right(str("DEBT $%d", debt), 314, 2, blink(24) && debt > gold ? CLR_WHITE : CLR_RED);
    else          print_right("DEBT CLEAR", 314, 2, CLR_GREEN);
}

static void draw_moneylender(void) {
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int mx = 48, my = 56, mw = 224, mh = 110;
    rectfill(mx, my, mw, mh, CLR_BLACK); rect(mx, my, mw, mh, CLR_ORANGE);
    print("THE MONEYLENDER", mx + 12, my + 8, CLR_LIGHT_YELLOW);
    print(str("you owe $%d", debt), mx + 12, my + 22, CLR_RED);
    print_right("grows ~5% a day", mx + mw - 12, my + 22, CLR_DARK_GREY);

    int bx = 64, bw = 192, bh = 20;
    int ys[3] = { 92, 118, 144 };
    const char *lab[3] = { str("PAY $100"), str("PAY ALL ($%d)", min(gold, debt)), "CLOSE" };
    for (int i = 0; i < 3; i++) {
        bool hv = hover(bx, ys[i], bw, bh);
        int c = i == 2 ? CLR_LIGHT_GREY : CLR_GREEN;
        rectfill(bx, ys[i], bw, bh, hv ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rect(bx, ys[i], bw, bh, c);
        print(lab[i], bx + (bw - text_width(lab[i])) / 2, ys[i] + 6, hv ? CLR_WHITE : c);
    }
}

static void draw_island(int d) {
    int cx = NX[d], cy = NY[d];
    ovalfill(cx, cy + 6, 24, 9, CLR_DARK_BLUE);              // shallows
    ovalfill(cx, cy + 2, 20, 11, CLR_LIGHT_PEACH);          // sand
    ovalfill(cx, cy - 1, 14, 8, LAND[d]);                   // land
    // a couple of huts
    rectfill(cx - 8, cy - 4, 5, 5, CLR_BROWNISH_BLACK);
    rectfill(cx + 3, cy - 5, 6, 6, CLR_BROWNISH_BLACK);
    pset(cx - 6, cy - 2, ACCENT[d]); pset(cx + 6, cy - 3, ACCENT[d]);
}

static void draw_chart(void) {
    cls(CLR_TRUE_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 30, SCREEN_W, SCREEN_H - 30, CLR_DARK_BLUE); fillp_reset();
    // wave glints
    for (int i = 0; i < 22; i++) {
        int wx = (i * 53 + (int)(now() * 14)) % SCREEN_W;
        int wy = (i * 71) % SCREEN_H;
        if ((i + frame() / 20) % 3 == 0) pset(wx, wy, CLR_BLUE);
    }
    // route lines between ports (faint)
    for (int i = 0; i < NPORT; i++)
        for (int j = i + 1; j < NPORT; j++)
            line(NX[i], NY[i], NX[j], NY[j], CLR_DARKER_GREY);

    for (int d = 0; d < NPORT; d++) draw_island(d);

    for (int d = 0; d < NPORT; d++) {
        bool cur = d == here, hv = hover(NX[d] - 26, NY[d] - 16, 52, 34) || d == sel;
        int tw = text_width(SHORT[d]);
        int rc = cur ? CLR_YELLOW : hv ? CLR_WHITE : CLR_LIGHT_GREY;
        rectfill(NX[d] - tw / 2 - 2, NY[d] + 14, tw + 4, 9, CLR_BLACK);
        rect(NX[d] - tw / 2 - 2, NY[d] + 14, tw + 4, 9, cur ? CLR_YELLOW : rc);
        print(SHORT[d], NX[d] - tw / 2, NY[d] + 15, cur ? CLR_YELLOW : CLR_WHITE);
        if (cur) print("HERE", NX[d] - text_width("HERE") / 2, NY[d] - 22, CLR_GREEN);
        if (hv && !cur) {
            float dd = distance(NX[here], NY[here], NX[d], NY[d]);
            int days = max(1, (int)(dd / 130.0f + 0.5f));
            line(NX[here], NY[here], NX[d], NY[d], CLR_LIGHT_YELLOW);
            print(str("%dd", days), (NX[here] + NX[d]) / 2 - 4, (NY[here] + NY[d]) / 2 - 4, CLR_LIGHT_YELLOW);
        }
    }
    draw_ship(NX[here], NY[here] - 2 + sin_deg(now() * 100) * 1.2f, 1.4f, false);

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print_centered("SEA CHART  -  click a port to set sail", SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    print_right("M back", 314, 2, CLR_DARK_GREY);
}

static void draw_sail_screen(void) {
    cls(CLR_TRUE_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 30, SCREEN_W, SCREEN_H - 30, CLR_DARK_BLUE); fillp_reset();
    for (int d = 0; d < NPORT; d++) draw_island(d);
    line(NX[here], NY[here], NX[sail_dest], NY[sail_dest], CLR_DARKER_GREY);
    draw_parts();
    bool faceLeft = NX[sail_dest] < NX[here];
    draw_ship(shipx, shipy + sin_deg(now() * 200) * 1.5f, 1.5f, faceLeft);

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showGold), 4, 2, CLR_YELLOW);
    print_centered(str("SAILING TO %s  -  %dd", PORT[sail_dest], sail_days), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    bar(60, SCREEN_H - 10, SCREEN_W - 120, 5, clamp(voyage, 0, 1), CLR_LIGHT_YELLOW, CLR_DARKER_GREY);
}

static void draw_event_card(void) {
    int w = 224, x = (SCREEN_W - w) / 2, h = 80;
    int y = (int)lerp(-80, 48, ease_out(ev_t));
    int bc = ev_bad ? CLR_RED : CLR_GREEN;
    rectfill(x, y, w, h, CLR_BLACK);
    rect(x, y, w, h, bc); rect(x + 2, y + 2, w - 4, h - 4, bc);

    const char *title = "", *b1 = "", *b2 = "";
    switch (ev_kind) {
        case EV_STORM:    title = "STORM!";       b1 = "Waves swamp the deck.";
                          b2 = ev_amt ? str("%d cargo washed overboard.", ev_amt) : "You ride it out, just."; break;
        case EV_PIRATE:   title = "PIRATES!";     b1 = "Raiders board and plunder.";
                          b2 = ev_amt ? str("They take %d cargo.", ev_amt) : "Your hold was empty — they leave."; break;
        case EV_SHORTAGE: title = "SHORTAGE";     b1 = str("%s is suddenly scarce here", GOOD[ev_g]); b2 = "...the price SOARS! sell now."; break;
        case EV_GLUT:     title = "MARKET GLUT";  b1 = str("%s floods the docks here", GOOD[ev_g]);   b2 = "...the price CRASHES! buy cheap."; break;
        case EV_FAIR:     title = "TRADE FAIR";   b1 = "A merchant gifts a crate."; b2 = str("Free %d %s.", ev_amt, GOOD[ev_g]); break;
    }
    print_centered(title, SCREEN_W/2, y + 8, ev_bad ? CLR_RED : CLR_LIGHT_YELLOW);
    print_centered(b1, SCREEN_W/2, y + 30, CLR_WHITE);
    print_centered(b2, SCREEN_W/2, y + 46, CLR_LIGHT_GREY);
    if (ev_t > 0.5f && blink(20)) print_centered("press Z", SCREEN_W/2, y + 64, CLR_DARK_GREY);
}

static void draw_banner(void) {
    if (bannerT > 2.4f) return;
    int w = text_width(bannerBuf) + 24, x = (SCREEN_W - w) / 2;
    float in = clamp(bannerT * 4, 0, 1);
    int y = (int)lerp(-22, 26, ease_out(in));
    rectfill(x, y, w, 18, CLR_BLACK);
    rect(x, y, w, 18, bannerCol); rect(x + 2, y + 2, w - 4, 14, bannerCol);
    print(bannerBuf, x + 12, y + 5, bannerCol);
}
static void draw_flash(void) {
    if (flashT <= 0) return;
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, flashColor); fillp_reset();
}

static void draw_title(void) {
    cls(CLR_DARKER_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 120, SCREEN_W, 80, CLR_DARK_BLUE); fillp_reset();
    for (int i = 0; i < 30; i++) pset((i * 73 + (int)(now() * 10)) % SCREEN_W, (i * 47) % 120, CLR_DARK_GREY);
    draw_ship(SCREEN_W / 2, 92, 3.4f, false);
    print_scaled("MERCHANT", (SCREEN_W - text_width("MERCHANT") * 3) / 2, 28, CLR_LIGHT_YELLOW, 3);
    print_centered("buy low, sail far, sell dear", SCREEN_W/2, 132, CLR_LIGHT_GREY);
    print_centered(str("clear $%d before %d days are out", GOAL, SEASON), SCREEN_W/2, 146, CLR_DARK_GREY);
    print_centered(str("best fortune: $%d", best), SCREEN_W/2, 162, CLR_YELLOW);
    print_centered("press Z / click to weigh anchor", SCREEN_W/2, 180, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_over(void) {
    int net = gold - debt;
    for (int i = 0; i < NGOOD; i++) net += cargo[i] * price[here][i];
    if (!over_saved) { if (net > best) { best = net; save(0, best); } over_saved = 1; }

    cls(CLR_BROWNISH_BLACK);
    fillp(FILL_HLINES, -1); rectfill(0, 130, SCREEN_W, 70, CLR_DARK_BLUE); fillp_reset();
    rectfill(40, 40, 240, 116, CLR_BLACK); rect(40, 40, 240, 116, CLR_LIGHT_YELLOW);
    const char *head = won ? "-- A MERCHANT PRINCE! --" : "-- THE SEASON ENDS --";
    print_centered(head, SCREEN_W/2, 50, won ? CLR_YELLOW : CLR_LIGHT_YELLOW);
    print_centered(str("gold $%d", gold), SCREEN_W/2, 70, CLR_YELLOW);
    print_centered(str("cargo worth $%d", net - gold + debt), SCREEN_W/2, 84, CLR_GREEN);
    if (debt > 0) print_centered(str("debt -$%d", debt), SCREEN_W/2, 98, CLR_RED);
    print_centered(str("NET FORTUNE  $%d", net), SCREEN_W/2, 116, net >= GOAL ? CLR_YELLOW : CLR_LIGHT_GREY);
    print_centered(str("best  $%d", best), SCREEN_W/2, 132, CLR_LIGHT_GREY);
    print_centered("press Z to sail again", SCREEN_W/2, 146, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

void draw(void) {
    if (state == TITLE) { draw_title(); draw_flash(); return; }
    if (state == OVER)  { draw_over();  draw_flash(); return; }
    if (state == MAP)   { draw_chart(); draw_banner(); draw_flash(); return; }
    if (state == SAIL)  { draw_sail_screen(); draw_banner(); draw_flash(); return; }

    // MARKET (and EVENT overlays on top of it)
    draw_harbor();
    draw_market_panel();
    draw_hud();
    if (modal) draw_moneylender();
    if (state == EVENT) draw_event_card();
    draw_banner();
    draw_flash();
}
