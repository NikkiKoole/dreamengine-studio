#include "studio.h"

// TRADE WINDS — an age-of-sail spice-trading game.
// Sail an archipelago of 6 ports, buy goods cheap and sell them dear as prices
// swing. Voyages cost days and may pitch you into a STORM (steer the wheel to
// ride the swell) or a PIRATE (broadside duel — or outrun them). Spend gold at
// the SHIPYARD on a bigger hold, faster hull, more cannons, and hull repairs.
// Build the greatest fortune before the season's 60 days run out.
//
// It reuses the Druglord economy core (per-port price skews + daily re-roll +
// limited hold), wrapped in a sailed sea-chart and two light encounters.
//
// MOUSE: click a good to BUY 1 (right-click SELL 1); bottom bar BUY/SELL/QTY/
//   SHIPYARD/SET SAIL; on the chart click a port to sail there.
// KEYS: up/down pick a good, Z buy / X sell, Q qty, S shipyard, M/ENTER chart.
//   Storm: steer with the mouse or ← →. Pirate: Z fires, X flees.

// ---- goods ----------------------------------------------------------------
enum { SPICE, SILK, TEA, RUM, COTTON, IVORY, NGOOD };
static const char *GOOD[NGOOD] = { "SPICE", "SILK", "TEA", "RUM", "COTTON", "IVORY" };
static const int   BASE[NGOOD] = {   40,    140,   80,   55,   60,      260  };

// ---- ports ----------------------------------------------------------------
enum { LISBOA, CANTON, MALACCA, GOA, ZANZIBAR, BANDA, NPORT };
static const char *PORT[NPORT]  = { "LISBOA","CANTON","MALACCA","GOA","ZANZIBAR","BANDA" };
static const char *SHORT[NPORT] = { "LISBOA","CANTON","MALACCA","GOA","ZANZBR","BANDA" };

// chart node pixel centres — MUST match the island cells baked in the .cart.js
static const int NX[NPORT] = {  40, 280, 264,  56, 152, 296 };
static const int NY[NPORT] = {  24,  24,  88, 104, 136, 136 };

// price level per port×good as a percent of base (the port's "flavour": where a
// good is cheap to BUY (<100) and where it sells dear (>100)).
static const int FAC[NPORT][NGOOD] = {
//    SPICE SILK  TEA  RUM  COT IVORY
    { 155, 150, 135, 120, 115, 165 },  // LISBOA   — the home market: everything sells dear
    { 115,  55,  55, 105,  95, 130 },  // CANTON   — silk & tea source: buy them cheap
    { 100,  75,  90, 100, 110, 120 },  // MALACCA  — the crossroads
    { 105, 100,  95,  60,  65, 115 },  // GOA      — rum & cotton hub
    {  55, 115, 105, 105, 100,  55 },  // ZANZIBAR — spice & ivory source
    {  45, 125, 115, 120, 135,  90 },  // BANDA    — the remote spice isles
};

// per-port harbour mood: accent colour + shanty tempo/scale/chord root
static const int ACCENT[NPORT] = { CLR_YELLOW, CLR_RED, CLR_ORANGE, CLR_GREEN, CLR_PEACH, CLR_PINK };
static const int BPMV[NPORT]   = { 104, 92, 100, 96, 88, 84 };
static const int ROOTV[NPORT]  = { 50, 52, 48, 47, 45, 43 };
static const int CHTV[NPORT]   = { CHORD_MAJ, CHORD_MAJ7, CHORD_DOM7, CHORD_MAJ, CHORD_MIN, CHORD_MIN7 };
static const int SCLV[NPORT]   = { SCALE_MAJOR, SCALE_PENTA, SCALE_MAJOR, SCALE_PENTA, SCALE_MINOR, SCALE_PENTA_MIN };

// ---- season / goal --------------------------------------------------------
#define SEASON 60
#define GOAL   5000

// ---- state ----------------------------------------------------------------
enum { TITLE, MARKET, MAP, SAIL, STORM, PIRATE, OVER };
static int state;

static int   price[NPORT][NGOOD];
static int   seen[NPORT][NGOOD];
static int   arrow[NGOOD];          // -1 down / 0 / +1 up vs last seen here
static int   cargo[NGOOD];

static int   gold, day, here, capacity, speed, guns, hull, hullmax;
static int   holdLvl, speedLvl, gunsLvl;
static int   sel, qtyMode, modal, best, over_saved, won, sunk;
static float showGold;              // rolls toward gold

// voyage
static int   sail_dest, sail_days, pending;   // pending: 0 none / 1 storm / 2 pirate
static bool  pending_done;
static float voyage, shipx, shipy;

// storm minigame
static float stormT, stability, helm;

// pirate skirmish
static int   pstate;                // 0 = choice, 1 = fighting
static float pirateHP, pirateMax, gunMark, gunDir, pirateFireT;

// banner + flash
static char  bannerBuf[48];
static float bannerT, flashT;
static int   bannerCol, flashColor;

// ---- particles (wake / rain / splash / spray) -----------------------------
#define NPART 140
typedef struct { float x, y, vx, vy, life, max; int col; } Part;
static Part part[NPART];

// ---- layout ----------------------------------------------------------------
#define PANEL_Y 82
#define ROW0    104
#define ROWH    11
#define BY      174
#define BH      20
#define BW      58
#define BX(i)   (5 + (i) * 63)
#define NBTN    5

// ===========================================================================
static float fabsf2(float v) { return v < 0 ? -v : v; }
static int  units(void) { int t = 0; for (int i = 0; i < NGOOD; i++) t += cargo[i]; return t; }
static int  typical(int d, int g) { return BASE[g] * FAC[d][g] / 100; }
static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }
static int  qtyAmt(void) { switch (qtyMode) { case 0: return 1; case 1: return 5; case 2: return 10; default: return 99999; } }

// ---- audio stings ----------------------------------------------------------
static void coin(void)   { hit(81, 6, 3, 40); schedule(45, 88, 6, 3); }
static void chime(void)  { hit(88, 6, 3, 40); schedule(45, 81, 6, 3); }
static void nope(void)   { note(45, INSTR_SQUARE, 2); }
static void gull(void)   { hit(95, INSTR_SQUARE, 1, 40); schedule(70, 99, INSTR_SQUARE, 1); }
static void cannon(void) { hit(34, INSTR_NOISE, 5, 200); note(28, INSTR_NOISE, 4); }
static void thunder(void){ hit(24, INSTR_NOISE, 5, 400); }

static void banner(const char *t, int c) {
    int i = 0; for (; t[i] && i < 47; i++) bannerBuf[i] = t[i];
    bannerBuf[i] = 0; bannerT = 0; bannerCol = c;
}
static void flash(int c, float t) { flashColor = c; flashT = t; }

static void spawn(float x, float y, float vx, float vy, float life, int col) {
    for (int i = 0; i < NPART; i++) if (part[i].life <= 0) {
        part[i].x = x; part[i].y = y; part[i].vx = vx; part[i].vy = vy;
        part[i].life = part[i].max = life; part[i].col = col; return;
    }
}

// ---- economy ---------------------------------------------------------------
static void reroll(void) {
    for (int d = 0; d < NPORT; d++)
        for (int g = 0; g < NGOOD; g++) {
            int p = (int)(typical(d, g) * rnd_float_between(0.60f, 1.60f));
            if (chance(7)) p = p * 5 / 2;     // a sudden shortage — price soars
            if (chance(7)) p = p / 3;         // a glut — price collapses
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
static int drop_cargo(int n) {
    int dropped = 0;
    for (int k = 0; k < n; k++) {
        int held = -1, cnt = 0;
        for (int i = 0; i < NGOOD; i++) if (cargo[i] > 0 && rnd(++cnt) == 0) held = i;
        if (held < 0) break;
        cargo[held]--; dropped++;
    }
    return dropped;
}

// ---- voyage / arrival ------------------------------------------------------
static void check_season(void) {
    if (day > SEASON) { won = (gold >= GOAL); state = OVER; over_saved = 0; }
}
static void arrive(void) {
    here = sail_dest; day += sail_days;
    reroll();
    for (int g = 0; g < NGOOD; g++) {
        arrow[g] = seen[here][g] < 0 ? 0 : sgn(price[here][g] - seen[here][g]);
        seen[here][g] = price[here][g];
    }
    banner(str("PORT REACHED  -  %s", PORT[here]), ACCENT[here]);
    gull(); flash(CLR_LIGHT_PEACH, 0.2f);
    state = MARKET;
    check_season();
}
static void start_storm(void) { stormT = 0; stability = 0.75f; helm = 0.5f; state = STORM; shake(6); thunder(); banner("STORM!", CLR_LIGHT_GREY); }
static void start_pirate(void){ pstate = 0; pirateMax = pirateHP = 45 + day; gunMark = 0; gunDir = 1; pirateFireT = 0; state = PIRATE; shake(4); banner("PIRATES!", CLR_RED); cannon(); }

static void set_sail(int dest) {
    if (dest == here) return;
    sail_dest = dest;
    float d = distance(NX[here], NY[here], NX[dest], NY[dest]);
    sail_days = max(1, (int)(d / (60.0f + speed * 22.0f) + 0.5f));
    voyage = 0; shipx = NX[here]; shipy = NY[here];
    pending = 0; pending_done = false;
    int risk = (int)(d / 7) + day / 4;                       // longer & later legs are riskier
    if (chance(min(72, 22 + risk))) pending = chance(50) ? 1 : 2;
    state = SAIL;
}

// ---- shipyard --------------------------------------------------------------
static int repair_cost(void) { return (hullmax - hull) * 7; }
static int hold_cost(void)   { return 350 * (holdLvl + 1); }
static int speed_cost(void)  { return 600 * (speedLvl + 1); }
static int guns_cost(void)   { return 500 * (gunsLvl + 1); }

static void new_game(void) {
    gold = 1200; capacity = 60; speed = 1; guns = 1; hull = hullmax = 100;
    holdLvl = speedLvl = gunsLvl = 0; day = 1; here = LISBOA;
    for (int i = 0; i < NGOOD; i++) cargo[i] = 0;
    for (int d = 0; d < NPORT; d++) for (int g = 0; g < NGOOD; g++) seen[d][g] = -1;
    reroll();
    for (int g = 0; g < NGOOD; g++) { arrow[g] = 0; seen[here][g] = price[here][g]; }
    sel = 0; qtyMode = 0; modal = 0; showGold = gold; over_saved = won = sunk = 0;
    bannerT = 99;
}

void init(void) {
    best = load(0);
    colorkey(0);
    // shanty pad (accordion-ish) + pluck for melody/coins
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
    bpm(BPMV[here]);
    if (every(4)) chord(ROOTV[here], CHTV[here], 5, 2);
    if (every(2) && chance(45)) tone(SCLV[here], 5, 6, 2);
}

// ---- the sailing animation -------------------------------------------------
static void update_sail(void) {
    voyage += dt() / 2.4f;
    shipx = lerp(NX[here], NX[sail_dest], voyage);
    shipy = lerp(NY[here], NY[sail_dest], voyage);
    // wake trail behind the ship
    if (chance(60)) spawn(shipx + rnd_between(-2, 3), shipy + 6 + rnd_between(-2, 3),
                          rnd_float_between(-6, 6), rnd_float_between(-4, 4), 0.5f, CLR_WHITE);
    if (frame() % 90 == 0) gull();
    if (frame() % 50 == 0) hit(36, INSTR_NOISE, 1, 220);     // wave wash

    if (pending && !pending_done && voyage >= 0.5f) {
        if (pending == 1) start_storm(); else start_pirate();
        return;
    }
    if (voyage >= 1.0f) arrive();
}

// ---- storm minigame --------------------------------------------------------
static void resolve_storm(void) {
    if (stability < 0.22f) {
        hull -= 28; int d = drop_cargo(rnd_between(4, 9)); sail_days += 1;
        banner(d ? str("BATTERED!  -%d cargo, hull hit", d) : "BATTERED!  hull hit", CLR_RED);
        flash(CLR_RED, 0.4f); shake(8);
    } else if (stability < 0.58f) {
        hull -= 12; int d = drop_cargo(rnd_between(1, 3));
        banner(d ? str("ROUGH SEAS  -%d cargo", d) : "ROUGH SEAS  hull hit", CLR_ORANGE);
    } else {
        banner("RODE IT OUT!", CLR_GREEN); flash(CLR_GREEN, 0.2f);
    }
    pending_done = true;
    if (hull <= 0) { hull = 0; sunk = 1; won = 0; state = OVER; over_saved = 0; }
    else state = SAIL;
}
static void update_storm(void) {
    stormT += dt();
    if (btn(0, BTN_LEFT) || btn(0, BTN_RIGHT)) {
        if (btn(0, BTN_LEFT))  helm -= dt() * 0.9f;
        if (btn(0, BTN_RIGHT)) helm += dt() * 0.9f;
    } else helm = lerp(helm, (float)mouse_x() / SCREEN_W, 0.30f);
    helm = clamp(helm, 0, 1);

    float swell = clamp(0.12f + noise(now() * 0.55f) * 0.76f, 0.05f, 0.95f);
    float off = fabsf2(helm - swell);
    if (off > 0.14f) { stability -= dt() * 0.42f; if (chance(50)) spawn(SCREEN_W / 2 + rnd_between(-30, 30), 120, rnd_float_between(-40, 40), -60, 0.4f, CLR_WHITE); }
    else stability += dt() * 0.24f;
    stability = clamp(stability, 0, 1);

    shake(2 + off * 9);
    for (int i = 0; i < 3; i++) spawn(rnd(SCREEN_W), -2, -30, 240, 0.8f, CLR_LIGHT_GREY);  // rain
    if (frame() % 64 == 0) { flash(CLR_WHITE, 0.12f); thunder(); }

    if (stormT > 5.5f || stability <= 0) resolve_storm();
}

// ---- pirate skirmish -------------------------------------------------------
#define YOURX 70
#define ENEMX 250
#define SHIPY 100
static void pirate_loot(void) {
    int loot = rnd_between(150, 420) + day * 8;
    gold += loot;
    banner(str("PIRATE SUNK!  +%d gold", loot), CLR_YELLOW);
    flash(CLR_YELLOW, 0.3f); chime();
    pending_done = true; state = SAIL;
}
static void pirate_fire_back(void) {
    int dmg = rnd_between(6, 12);
    hull -= dmg; flash(CLR_RED, 0.25f); shake(5); cannon();
    for (int i = 0; i < 8; i++) spawn(YOURX + 24, SHIPY, rnd_float_between(-50, 50), rnd_float_between(-50, 10), 0.5f, CLR_LIGHT_GREY);
    if (hull <= 0) { hull = 0; sunk = 1; won = 0; state = OVER; over_saved = 0; }
}
static void player_fire(void) {
    float half = 0.10f + guns * 0.008f;
    cannon(); shake(2);
    for (int i = 0; i < 6; i++) spawn(YOURX + 24, SHIPY, rnd_float_between(20, 90), rnd_float_between(-30, 10), 0.4f, CLR_ORANGE);
    if (fabsf2(gunMark - 0.5f) < half) {
        pirateHP -= 14 + guns * 6;
        flash(CLR_ORANGE, 0.2f); shake(4);
        for (int i = 0; i < 10; i++) spawn(ENEMX + rnd_between(-8, 8), SHIPY, rnd_float_between(-50, 50), rnd_float_between(-60, 0), 0.6f, CLR_DARK_GREY);
        if (pirateHP <= 0) { pirate_loot(); return; }
    } else {
        for (int i = 0; i < 6; i++) spawn(ENEMX + rnd_between(-14, 14), SHIPY + 8, rnd_float_between(-30, 30), -70, 0.5f, CLR_WHITE);  // splash
    }
}
static void do_flee(void) {
    int esc = min(90, 28 + speed * 16);
    if (chance(esc)) { banner("ESCAPED THEM!", CLR_GREEN); flash(CLR_GREEN, 0.2f); pending_done = true; state = SAIL; }
    else { hull -= 12; flash(CLR_RED, 0.3f); shake(5); cannon(); banner("THEY CAUGHT YOU!", CLR_RED); pstate = 1;
           if (hull <= 0) { hull = 0; sunk = 1; state = OVER; over_saved = 0; } }
}
static void update_pirate(void) {
    if (pstate == 0) {                                       // FIGHT / FLEE choice
        if (clicked(70, 130, 80, 24) || keyp('Z') || btnp(0, BTN_A)) { pstate = 1; return; }
        if (clicked(170, 130, 80, 24) || keyp('X') || btnp(0, BTN_B)) { do_flee(); return; }
        return;
    }
    // gunnery gauge sweeps; fire when it's centred
    gunMark += gunDir * dt() * 1.25f;
    if (gunMark > 1) { gunMark = 1; gunDir = -1; }
    if (gunMark < 0) { gunMark = 0; gunDir = 1; }
    if (mouse_pressed(MOUSE_LEFT) || keyp('Z') || keyp(KEY_SPACE) || btnp(0, BTN_A)) player_fire();
    if (state != PIRATE) return;
    if (keyp('X') || btnp(0, BTN_B) || clicked(4, 178, 60, 18)) { do_flee(); return; }

    pirateFireT += dt();
    if (pirateFireT > 1.4f) { pirateFireT = 0; pirate_fire_back(); }
}

// ===========================================================================
void update(void) {
    if (bannerT < 90) bannerT += dt();
    if (flashT > 0)   flashT -= dt();
    update_parts();
    showGold = lerp(showGold, (float)gold, 0.18f);

    if (state == TITLE) { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) state = MARKET; return; }
    if (state == OVER)  { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_ENTER)) { new_game(); state = TITLE; } return; }
    if (state == SAIL)   { update_sail();   return; }
    if (state == STORM)  { update_storm();  return; }
    if (state == PIRATE) { update_pirate(); return; }

    if (state == MAP) {
        for (int d = 0; d < NPORT; d++)
            if (d != here && clicked(NX[d] - 22, NY[d] - 12, 44, 28)) { set_sail(d); return; }
        if (keyp('M') || keyp(KEY_TAB) || keyp(KEY_ESCAPE)) state = MARKET;
        return;
    }

    // ---- MARKET ----
    play_shanty();

    if (modal) {                                             // SHIPYARD
        int bx = 56, bw = 208, bh = 18;
        int y0 = 64, y1 = 86, y2 = 108, y3 = 130, y4 = 152;
        if (clicked(bx, y0, bw, bh) && hull < hullmax && gold >= repair_cost()) { gold -= repair_cost(); hull = hullmax; coin(); }
        if (clicked(bx, y1, bw, bh) && gold >= hold_cost())  { gold -= hold_cost();  capacity += 40; holdLvl++; coin(); }
        if (clicked(bx, y2, bw, bh) && gold >= speed_cost() && speedLvl < 4) { gold -= speed_cost(); speed++; speedLvl++; coin(); }
        if (clicked(bx, y3, bw, bh) && gold >= guns_cost()  && gunsLvl  < 5) { gold -= guns_cost();  guns++;  gunsLvl++;  coin(); }
        if (clicked(bx, y4, bw, bh) || keyp('S') || keyp(KEY_ESCAPE)) modal = 0;
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
    if (clicked(BX(4), BY, BW, BH)) { state = MAP; }

    if (btnp(0, BTN_A)) buy(sel, qtyAmt());
    if (btnp(0, BTN_B)) sell(sel, qtyAmt());
    if (keyp('Q')) qtyMode = (qtyMode + 1) % 4;
    if (keyp('S')) modal = 1;
    if (keyp('M') || keyp(KEY_TAB) || keyp(KEY_ENTER)) state = MAP;
}

// ===========================================================================
// drawing
// ===========================================================================
static void draw_ship(float cx, float cy, float scl, bool pirate, float rock) {
    int dw = (int)(16 * scl), dh = (int)(16 * scl);
    int dx = (int)(cx - dw / 2), dy = (int)(cy - dh / 2);
    int fr = anim(2, 3.0f, 0);                               // slots 0/1 = the two sail frames
    int sw = pirate ? -16 : 16;                              // negative source width = face the other way
    if (pirate) { pal(7, CLR_DARKER_GREY); pal(4, CLR_BROWNISH_BLACK); pal(20, CLR_BLACK); pal(14, CLR_RED); }
    sspr_ex(fr * 16, 0, sw, 16, dx, dy, dw, dh, pirate ? -rock : rock, dw / 2, dh / 2);
    if (pirate) pal_reset();
}

static void draw_chart_ship(float cx, float cy, bool faceLeft, bool pirate) {
    float bob = sin_deg(now() * 120) * 1.0f;
    if (pirate) { pal(7, CLR_DARKER_GREY); pal(4, CLR_BROWNISH_BLACK); pal(14, CLR_RED); }
    sprf(anim(2, 3.0f, 0), (int)(cx - 8), (int)(cy - 8 + bob), faceLeft, false);
    if (pirate) pal_reset();
}

// --- harbour backdrop for the MARKET ---
static void draw_harbor(void) {
    int ph = (day - 1) * 4 / SEASON; if (ph > 3) ph = 3;     // 0 morning .. 3 dusk
    static const int SKY[4] = { CLR_BLUE, CLR_BLUE, CLR_INDIGO, CLR_MAUVE };
    cls(SKY[ph]);
    // sun lowering across the season
    static const int ORB[4] = { CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_PEACH };
    circfill(40 + day * 240 / SEASON, 22 + ph * 6, 7, ORB[ph]);

    // sea band
    rectfill(0, 54, SCREEN_W, PANEL_Y - 54, CLR_TRUE_BLUE);
    fillp(FILL_HLINES, -1);
    rectfill(0, 64, SCREEN_W, PANEL_Y - 64, CLR_DARK_BLUE);
    fillp_reset();

    // the port town on the right (sand strip + sprites)
    rectfill(196, 50, SCREEN_W - 196, 8, CLR_LIGHT_PEACH);
    pal(10, ACCENT[here]);                                   // windows glow in the port accent
    spr(20, 208, 38); spr(19, 232, 38); spr(20, 256, 38); spr(19, 280, 38);
    pal_reset();
    print(PORT[here], 200, 44, ACCENT[here]);

    // gulls
    for (int i = 0; i < 3; i++) {
        int gx = (int)(60 + i * 70 + sin_deg(now() * 30 + i * 90) * 30);
        int gy = 22 + i * 5 + (int)(sin_deg(now() * 90 + i * 60) * 2);
        line(gx, gy, gx + 3, gy - 2, CLR_WHITE); line(gx + 3, gy - 2, gx + 6, gy, CLR_WHITE);
    }
    // your ship docked, bobbing
    draw_chart_ship(70, 58 + sin_deg(now() * 100) * 1.5f, false, false);
    draw_parts();
}

static void arrow_at(int x, int y, int dir) {
    if (dir > 0)      trifill(x, y + 5, x + 6, y + 5, x + 3, y, CLR_RED);     // up = pricier
    else if (dir < 0) trifill(x, y, x + 6, y, x + 3, y + 5, CLR_GREEN);       // down = cheaper
}

static void draw_market_panel(void) {
    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARKER_BLUE);
    line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_DARK_BLUE);

    print("CARGO MARKET", 8, 86, CLR_LIGHT_YELLOW);
    int holdc = units() >= capacity ? CLR_RED : CLR_LIGHT_GREY;
    print_right(str("HOLD %d/%d", units(), capacity), 312, 86, holdc);
    print("PRICE", 196, 96, CLR_DARK_GREY);
    print_right("HOLD", 312, 96, CLR_DARK_GREY);

    for (int i = 0; i < NGOOD; i++) {
        int ry = ROW0 + i * ROWH, typ = typical(here, i), pr = price[here][i];
        if (i == sel) { rectfill(4, ry - 1, 312, ROWH, CLR_DARKER_GREY); rect(4, ry - 1, 312, ROWH, CLR_INDIGO); }
        print(GOOD[i], 10, ry + 2, CLR_WHITE);
        int pc = pr > typ * 14 / 10 ? CLR_GREEN : pr < typ * 7 / 10 ? CLR_ORANGE : CLR_LIGHT_GREY;
        print_right(str("$%d", pr), 236, ry + 2, pc);
        arrow_at(240, ry + 2, arrow[i]);
        if (pr < typ * 6 / 10 && blink(20))       print("BUY!",  250, ry + 2, CLR_ORANGE);
        else if (pr > typ * 15 / 10 && blink(20)) print("SELL!", 250, ry + 2, CLR_GREEN);
        print_right(str("x%d", cargo[i]), 312, ry + 2, cargo[i] ? CLR_WHITE : CLR_DARK_GREY);
    }

    const char *qlab = qtyMode == 3 ? "MAX" : str("x%d", qtyAmt());
    const char *lab[NBTN] = { "BUY", "SELL", qlab, "SHIPYARD", "SET SAIL" };
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
    print(str("HULL"), 232, 2, hull < hullmax / 3 ? (blink(18) ? CLR_RED : CLR_DARK_RED) : CLR_LIGHT_GREY);
    bar(266, 2, 50, 7, (float)hull / hullmax, hull < hullmax / 3 ? CLR_RED : CLR_GREEN, CLR_DARK_RED);
}

static void draw_shipyard(void) {
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int mx = 48, my = 40, mw = 224, mh = 150;   // tall enough for the footer to clear the LEAVE button
    rectfill(mx, my, mw, mh, CLR_BLACK); rect(mx, my, mw, mh, CLR_LIGHT_YELLOW);
    print("THE SHIPYARD", mx + 12, my + 6, CLR_LIGHT_YELLOW);
    print_right(str("gold $%d", gold), mx + mw - 12, my + 6, CLR_YELLOW);

    int bx = 56, bw = 208, bh = 18;
    int ys[5] = { 64, 86, 108, 130, 152 };
    const char *l0 = hull >= hullmax ? "REPAIR HULL          full" : str("REPAIR HULL          $%d", repair_cost());
    const char *l1 = str("BIGGER HOLD (+40)    $%d", hold_cost());
    const char *l2 = speedLvl >= 4 ? "FASTER HULL          maxed" : str("FASTER HULL (+1)     $%d", speed_cost());
    const char *l3 = gunsLvl  >= 5 ? "MORE CANNONS         maxed" : str("MORE CANNONS (+1)    $%d", guns_cost());
    const char *lab[5] = { l0, l1, l2, l3, "LEAVE THE SHIPYARD" };

    for (int i = 0; i < 5; i++) {
        bool hv = hover(bx, ys[i], bw, bh);
        int c = i == 4 ? CLR_LIGHT_GREY : CLR_GREEN;
        rectfill(bx, ys[i], bw, bh, hv ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rect(bx, ys[i], bw, bh, c);
        print(lab[i], bx + 8, ys[i] + 5, hv ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    print_centered(str("HOLD %d   SPEED %d   CANNONS %d", capacity, speed, guns), SCREEN_W/2, my + mh - 14, CLR_DARK_GREY);
}

static void draw_chart(void) {
    map(0, 0, 0, 0, 20, 13);                                 // the sea + islands
    // animated wave glints
    for (int i = 0; i < 24; i++) {
        int wx = (i * 53 + (int)(now() * 14)) % SCREEN_W;
        int wy = (i * 71) % SCREEN_H;
        if ((i + frame() / 20) % 3 == 0) pset(wx, wy, CLR_BLUE);
    }
    // route lines between every port (faint)
    for (int i = 0; i < NPORT; i++)
        for (int j = i + 1; j < NPORT; j++) {
            if ((i + j) % 2) continue;
            line(NX[i], NY[i], NX[j], NY[j], CLR_DARKER_GREY);
        }
    // nodes
    for (int d = 0; d < NPORT; d++) {
        bool cur = d == here, hv = hover(NX[d] - 22, NY[d] - 12, 44, 28);
        int rc = cur ? CLR_YELLOW : hv ? CLR_WHITE : CLR_LIGHT_GREY;
        circfill(NX[d], NY[d] - 4, 3, ACCENT[d]);
        circ(NX[d], NY[d] - 4, 3, rc);
        print_centered_x:; // (no-op label)
        int tw = text_width(SHORT[d]);
        rectfill(NX[d] - tw / 2 - 1, NY[d] + 6, tw + 2, 9, CLR_BLACK);
        print(SHORT[d], NX[d] - tw / 2, NY[d] + 7, cur ? CLR_YELLOW : CLR_WHITE);
        if (cur) print("HERE", NX[d] - text_width("HERE") / 2, NY[d] - 18, CLR_GREEN);
        if (hv && !cur) {
            float dd = distance(NX[here], NY[here], NX[d], NY[d]);
            int days = max(1, (int)(dd / (60.0f + speed * 22.0f) + 0.5f));
            line(NX[here], NY[here], NX[d], NY[d], CLR_LIGHT_YELLOW);
            print(str("%dd", days), NX[d] - 6, NY[d] - 28, CLR_LIGHT_YELLOW);
        }
    }
    draw_chart_ship(NX[here], NY[here] - 4, false, false);   // you, in port

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print_centered("SEA CHART  -  click a port to set sail", SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    print_right("M back", 314, 2, CLR_DARK_GREY);
}

static void draw_sail_screen(void) {
    map(0, 0, 0, 0, 20, 13);
    // faint route already sailed
    line(NX[here], NY[here], NX[sail_dest], NY[sail_dest], CLR_DARKER_GREY);
    draw_parts();
    bool faceLeft = NX[sail_dest] < NX[here];
    draw_chart_ship(shipx, shipy + sin_deg(now() * 200) * 1.5f, faceLeft, false);

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showGold), 4, 2, CLR_YELLOW);
    print_centered(str("SAILING TO %s  -  %dd", PORT[sail_dest], sail_days), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    bar(60, SCREEN_H - 10, SCREEN_W - 120, 5, clamp(voyage, 0, 1), CLR_LIGHT_YELLOW, CLR_DARKER_GREY);
}

static void draw_storm_screen(void) {
    cls(CLR_DARKER_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 110, SCREEN_W, 90, CLR_DARK_BLUE); fillp_reset();
    draw_parts();
    // pitching ship
    float rock = sin_deg(now() * 220) * 10 + (helm - 0.5f) * 30;
    draw_ship(SCREEN_W / 2, 120, 3.2f, false, rock);

    print_centered("RIDE OUT THE STORM", SCREEN_W/2, 18, CLR_LIGHT_YELLOW);
    print_centered("steer to meet the wave   (mouse  /  < >)", SCREEN_W/2, 30, CLR_LIGHT_GREY);

    // steering track
    int tx = 40, tw = SCREEN_W - 80, ty = 56;
    float swell = clamp(0.12f + noise(now() * 0.55f) * 0.76f, 0.05f, 0.95f);
    rectfill(tx, ty, tw, 10, CLR_BLACK); rect(tx, ty, tw, 10, CLR_DARK_GREY);
    int sX = tx + (int)(swell * tw);
    trifill(sX - 5, ty - 6, sX + 5, ty - 6, sX, ty, CLR_BLUE);          // the wave (target)
    int hX = tx + (int)(helm * tw);
    rectfill(hX - 3, ty + 1, 6, 8, fabsf2(helm - swell) < 0.14f ? CLR_GREEN : CLR_RED);  // your wheel

    // stability meter
    print("STABILITY", 40, 78, CLR_LIGHT_GREY);
    bar(40, 88, SCREEN_W - 80, 8, stability, stability > 0.55f ? CLR_GREEN : stability > 0.22f ? CLR_ORANGE : CLR_RED, CLR_DARKER_GREY);
}

static void draw_pirate_screen(void) {
    cls(CLR_DARKER_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, SHIPY + 10, SCREEN_W, SCREEN_H, CLR_DARK_BLUE); fillp_reset();

    float rock = sin_deg(now() * 160) * 6;
    draw_ship(YOURX, SHIPY, 2.6f, false, rock);
    draw_ship(ENEMX, SHIPY, 2.6f, true, -rock);
    draw_parts();

    // hull / enemy bars
    print("YOUR HULL", 6, 14, CLR_LIGHT_GREY);
    bar(6, 24, 90, 7, (float)hull / hullmax, CLR_GREEN, CLR_DARK_RED);
    print_right("PIRATE", 314, 14, CLR_RED);
    bar(224, 24, 90, 7, clamp(pirateHP / pirateMax, 0, 1), CLR_RED, CLR_DARKER_GREY);

    if (pstate == 0) {
        fillp(FILL_CHECKER, -1); rectfill(50, 110, 220, 60, CLR_BLACK); fillp_reset();
        rect(50, 110, 220, 60, CLR_LIGHT_YELLOW);
        print_centered("A PIRATE BEARS DOWN ON YOU!", SCREEN_W/2, 116, CLR_LIGHT_YELLOW);
        bool h0 = hover(70, 130, 80, 24), h1 = hover(170, 130, 80, 24);
        rectfill(70, 130, 80, 24, h0 ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(70, 130, 80, 24, CLR_RED);
        print("FIGHT (Z)", 70 + (80 - text_width("FIGHT (Z)")) / 2, 138, CLR_WHITE);
        rectfill(170, 130, 80, 24, h1 ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(170, 130, 80, 24, CLR_GREEN);
        print("FLEE (X)", 170 + (80 - text_width("FLEE (X)")) / 2, 138, CLR_WHITE);
        return;
    }
    // gunnery gauge
    int tx = 60, tw = 200, ty = 182;
    float half = 0.10f + guns * 0.008f;
    rectfill(tx, ty, tw, 10, CLR_BLACK); rect(tx, ty, tw, 10, CLR_DARK_GREY);
    rectfill(tx + (int)((0.5f - half) * tw), ty, (int)(half * 2 * tw), 10, CLR_DARK_GREEN);  // hit window
    int mX = tx + (int)(gunMark * tw);
    rectfill(mX - 1, ty - 3, 3, 16, CLR_YELLOW);
    print("FIRE (Z) when centred", 60, 168, CLR_LIGHT_GREY);
    print_right(str("CANNONS x%d", guns), 314, 168, CLR_ORANGE);
    bool hf = hover(4, 178, 60, 18);
    rectfill(4, 178, 60, 18, hf ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(4, 178, 60, 18, CLR_GREEN);
    print("FLEE", 4 + (60 - text_width("FLEE")) / 2, 183, CLR_WHITE);
}

static void draw_banner(void) {
    if (bannerT > 2.4f) return;
    int w = text_width(bannerBuf) + 24, x = (SCREEN_W - w) / 2, y = 26;
    float in = clamp(bannerT * 4, 0, 1);
    y = (int)lerp(-22, 26, ease_out(in));
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
    draw_ship(SCREEN_W / 2, 96, 4.0f, false, sin_deg(now() * 90) * 5);
    print_scaled("TRADE WINDS", (SCREEN_W - text_width("TRADE WINDS") * 3) / 2, 26, CLR_LIGHT_YELLOW, 3);
    print_centered("buy low, sail far, sell dear", SCREEN_W/2, 130, CLR_LIGHT_GREY);
    print_centered(str("reach $%d before %d days are out", GOAL, SEASON), SCREEN_W/2, 144, CLR_DARK_GREY);
    print_centered(str("best fortune: $%d", best), SCREEN_W/2, 160, CLR_YELLOW);
    print_centered("press Z / click to weigh anchor", SCREEN_W/2, 178, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_over(void) {
    int net = gold;
    for (int i = 0; i < NGOOD; i++) net += cargo[i] * price[here][i];
    if (!over_saved) { if (net > best) { best = net; save(0, best); } over_saved = 1; }

    cls(CLR_BROWNISH_BLACK);
    fillp(FILL_HLINES, -1); rectfill(0, 130, SCREEN_W, 70, CLR_DARK_BLUE); fillp_reset();
    rectfill(40, 40, 240, 110, CLR_BLACK); rect(40, 40, 240, 110, CLR_LIGHT_YELLOW);
    const char *head = sunk ? "-- YOUR SHIP IS LOST --" : won ? "-- A MERCHANT PRINCE! --" : "-- THE SEASON ENDS --";
    print_centered(head, SCREEN_W/2, 50, sunk ? CLR_RED : won ? CLR_YELLOW : CLR_LIGHT_YELLOW);
    if (sunk) {
        print_centered("the sea swallows your fortune.", SCREEN_W/2, 72, CLR_LIGHT_GREY);
        print_centered(str("you went down on day %d", day), SCREEN_W/2, 88, CLR_RED);
    } else {
        print_centered(str("gold $%d", gold), SCREEN_W/2, 72, CLR_YELLOW);
        print_centered(str("cargo worth $%d", net - gold), SCREEN_W/2, 86, CLR_GREEN);
        print_centered(str("FINAL FORTUNE  $%d", net), SCREEN_W/2, 104, won ? CLR_YELLOW : CLR_LIGHT_GREY);
    }
    print_centered(str("best  $%d", best), SCREEN_W/2, 122, CLR_LIGHT_GREY);
    print_centered("press Z to sail again", SCREEN_W/2, 138, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

void draw(void) {
    if (state == TITLE)  { draw_title(); draw_flash(); return; }
    if (state == OVER)   { draw_over();  draw_flash(); return; }
    if (state == MAP)    { draw_chart(); draw_banner(); draw_flash(); return; }
    if (state == SAIL)   { draw_sail_screen();   draw_banner(); draw_flash(); return; }
    if (state == STORM)  { draw_storm_screen();  draw_banner(); draw_flash(); return; }
    if (state == PIRATE) { draw_pirate_screen(); draw_banner(); draw_flash(); return; }

    // MARKET
    draw_harbor();
    draw_market_panel();
    draw_hud();
    if (modal) draw_shipyard();
    draw_banner();
    draw_flash();
}
