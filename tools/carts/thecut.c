/* de:meta
{
  "title": "The Cut",
  "status": "active",
  "created": "2026-06-17",
  "kind": [
    "game",
    "probe"
  ],
  "teaches": [
    "schedule-driven-agents",
    "save-load-persistence"
  ],
  "lineage": "Prototype for the sloop production-economy design (see docs/design/sloop-production.md); the pure brew_eval function + delegated-worker off-screen time-skip is the novel mechanic — DIY-to-manager progression loop.",
  "genre": "simulation",
  "description": "A process-optimization bench — the 'tune a recipe for best quality / most money' itch (Schedule 1 mixing x Dwarf Fortress maker-skill x a demand curve), prototyped as a subsystem for the sloop product economy. Build a BATCH from an ORDERED sequence of reagents, watch THREE numbers move — QUALITY (how clean), STRENGTH (how potent), EFFECT (what it does) — then STIR it by hand for a purity bonus and sell to the client whose taste fits. Order matters: reagents synergize or clash with their neighbour, and SOLVENT cleans whatever crude thing follows it. The whole product is one pure function, so a hired worker runs the same bench at their own skill — the do-it-yourself -> manage arc. Skill levels up as you craft, unlocking recipe slots. Or DELEGATE: open CREW, hire someone, hand them your recipe as a standing order, then LEAVE for a few days and come back to a ledger (batches, spoilage, their quality vs yours, gross minus wages) — they earn while you're away, but a skilled hire costs a wage and morale, and your own stirred hands eventually beat theirs. MOUSE: click a reagent to append, a chip to remove, CLEAR resets; tap the stir bar when the marker is in the green for a purity bonus, SELL to market, click a client to sell, CREW to manage workers. KEYS: 1-6 add reagent, BACKSPACE pop last, SPACE stir, C crew, ENTER to market / sell to best client."
}
de:meta */
#include "studio.h"

// THE CUT — a process-optimization bench for sloop's product economy.
//
// A back-room prototype of the "tune a recipe for best quality / most money"
// itch (Schedule 1 mixing x Dwarf Fortress maker-skill x a pizzatycoon demand
// curve). You build a BATCH from a base + an ORDERED sequence of reagents, STIR
// it by hand (the live skill bonus), then take it to MARKET and sell to the
// client whose taste it fits best.
//
// The whole product is one PURE FUNCTION — brew_eval(seq, n, skill, focus) —
// so a hired worker could run the same bench later at their own skill with no
// stir bonus. That's the DIY->manage seam from docs/design/sloop-production.md:
// do it yourself for the quality, or (eventually) delegate it for your time.
//
// THREE legible numbers move as you tweak: QUALITY (how clean), STRENGTH (how
// potent), EFFECT (what it does). Order matters — reagents synergize or clash
// with their neighbour, and SOLVENT cleans whatever crude thing follows it.
//
// MOUSE: click a reagent to append it, click a chip to remove it, CLEAR resets.
//   Hold the STIR button at the right moment for a purity bonus, then TO MARKET.
//   In MARKET click a client to sell the batch. KEYS: 1-6 add reagent,
//   BACKSPACE pop last, SPACE stir, ENTER to market / sell to best client.

// ---- effects (what a batch DOES) ------------------------------------------
enum { E_MELLOW, E_RUSH, E_DREAMY, E_SHARP, E_HEAVY, NEFF };
static const char *ENAME[NEFF] = { "MELLOW", "RUSH", "DREAMY", "SHARP", "HEAVY" };
static const int   ECOL[NEFF]  = { CLR_GREEN, CLR_ORANGE, CLR_PINK, CLR_BLUE, CLR_LIGHT_GREY };

// ---- reagents (the ordered sequence you build) -----------------------------
enum { R_HERB, R_CAFF, R_POPPY, R_MENTH, R_IRON, R_SOLV, NADD };
static const char *RNAME[NADD] = { "HERB", "CAFFEINE", "POPPY", "MENTHOL", "IRON", "SOLVENT" };
static const char *RSHORT[NADD]= { "HERB", "CAFF", "POPPY", "MENTH", "IRON", "SOLV" };
static const int   REFF[NADD]  = { E_MELLOW, E_RUSH, E_DREAMY, E_SHARP, E_HEAVY, -1 };  // -1 = pure refiner
static const int   RAMT[NADD]  = { 3, 3, 3, 3, 4, 0 };   // effect amount it adds
static const int   RPOT[NADD]  = { 1, 2, 1, 1, 3, 0 };   // strength (potency) it adds
static const int   RPUR[NADD]  = { 0, -2, -1, 1, -3, 5 };// purity delta (crude = negative)
static const int   RCOST[NADD] = { 2, 3, 3, 2, 4, 5 };   // ingredient cost per unit
static const int   RCOL[NADD]  = { CLR_GREEN, CLR_BROWN, CLR_MAUVE, CLR_BLUE, CLR_MEDIUM_GREY, CLR_LIGHT_GREY };

// adjacency synergy — prev row, cur col. Discoverable combos:
//   CAFF<->MENTH "electric", HERB<->POPPY "smooth", SOLV->IRON tames it,
//   IRON->CAFF is harsh. (Same-reagent-twice penalty handled separately.)
static const int SYN_STR[NADD][NADD] = {        // strength delta
//          HERB CAFF POPPY MENTH IRON SOLV
/*HERB */  {  0,   0,   2,    0,    0,   0 },
/*CAFF */  {  0,   0,   0,    2,    0,   0 },
/*POPPY*/  {  2,   0,   0,    0,    0,   0 },
/*MENTH*/  {  0,   2,   0,    0,    0,   0 },
/*IRON */  {  0,  -3,   0,    0,    0,   0 },
/*SOLV */  {  0,   0,   0,    0,    2,   0 },
};
static const int SYN_PUR[NADD][NADD] = {        // purity delta
/*HERB */  {  0,   0,   0,    0,    0,   0 },
/*CAFF */  {  0,   0,   0,    0,    0,   0 },
/*POPPY*/  {  0,   0,   0,    0,    0,   0 },
/*MENTH*/  {  0,   0,   0,    0,    0,   0 },
/*IRON */  {  0,  -3,   0,    0,    0,   0 },
/*SOLV */  {  3,   3,   2,    1,    5,   0 },     // solvent cleans the next thing
};

// ---- clients (the demand side) ---------------------------------------------
enum { C_DOCKS, C_CAMPUS, C_UPTOWN, C_NIGHT, C_SUBURB, NCLI };
static const char *CNAME[NCLI]   = { "DOCKHANDS", "CAMPUS", "UPTOWN", "NIGHTCLUB", "SUBURBS" };
static const int   CWANT[NCLI]   = { E_HEAVY, E_RUSH, E_DREAMY, E_SHARP, E_MELLOW };
static const int   CWEALTH[NCLI] = { 80, 70, 170, 130, 110 };   // price multiplier %
static const int   CVOL[NCLI]    = { 12, 14, 7, 10, 9 };         // units they'll take

// ---- batch (the locked product you carry to market) ------------------------
#define MAXSEQ 5
#define BATCH_UNITS 8
static int   seq[MAXSEQ], seqn;
static int   skill, xp, cash, day, best;
static float showCash;
static int   focus;          // stir bonus 0..15, baked into the batch
static int   batched;        // is there a stirred batch ready for market?

// ---- stir meter ------------------------------------------------------------
static float stir_t;         // 0..1 bouncing marker
static int   stir_dir = 1;

// ---- crew (v2: delegation — the "then you manage" half) --------------------
// A worker runs the SAME bench via the SAME pure function, at THEIR skill with
// focus=0 (no stir). You hand off a standing order, leave, and resolve on return.
#define NCAND 3
static const char *CAND_NAMES[] = { "PIP", "MARLO", "DUTCH", "REE", "SAPI", "NOOR", "VOSS", "KAZ" };
typedef struct { const char *name; int skill, wage; } Cand;
static Cand        cand[NCAND];           // current hire pool
static int         w_hired;
static const char *w_name;
static int         w_skill, w_wage, w_morale;
static int         w_order[MAXSEQ], w_ordern;   // the standing order (a saved recipe)
static int         leave_days = 3;

// ---- away report (resolve-on-return ledger) --------------------------------
static int   rp_days, rp_batches, rp_spoiled, rp_gross, rp_wages, rp_qsum, rp_qyou, rp_levelup, rp_raise;
static float rp_t;

// ---- ui --------------------------------------------------------------------
enum { TITLE, BENCH, MARKET, SOLD, MANAGE, REPORT };
static int   state, sold_cli, sold_profit, sold_units;
static float sold_t, swing;

static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }
static void blip(int n) { note(n, INSTR_SQUARE, 1); }
static void ching(void) { hit(81, INSTR_SQUARE, 3, 40); schedule(60, 88, INSTR_SQUARE, 3); }
static void nope(void)  { note(45, INSTR_SQUARE, 2); }

static int max_slots(void) { return skill >= 7 ? 5 : skill >= 4 ? 4 : 3; }

// ===========================================================================
// THE PURE FUNCTION. Everything the product is, from the recipe + who made it.
// No globals read except the const tables — a worker could call this verbatim.
typedef struct { int quality, strength, purity, dom, eff[NEFF]; } Brew;
static Brew brew_eval(const int *s, int n, int skl, int foc) {
    Brew b = { 0, 0, 0, 0, { 0 } };
    int purity = 55 + skl * 3;           // skilled hands start cleaner
    int strength = 0;
    for (int i = 0; i < n; i++) {
        int r = s[i];
        strength += RPOT[r];
        purity   += RPUR[r];
        if (REFF[r] >= 0) b.eff[REFF[r]] += RAMT[r];
        if (i > 0) {
            int p = s[i - 1];
            strength += SYN_STR[p][r];
            purity   += SYN_PUR[p][r];
            // solvent immediately before a crude reagent negates its dirt
            if (p == R_SOLV && RPUR[r] < 0) purity -= RPUR[r];
            if (p == r) { strength -= 2; purity -= 3; }   // repeating clashes
        }
    }
    purity += foc;                       // the live STIR bonus (0 for a worker)
    purity = mid(0, purity, 100);
    if (strength < 0) strength = 0;

    int sum = 0, dom = 0;
    for (int e = 0; e < NEFF; e++) { sum += b.eff[e]; if (b.eff[e] > b.eff[dom]) dom = e; }
    int focused = b.eff[dom] * 100 / (sum + 1);          // concentration 0..100
    int quality = (purity * 60 + focused * 40) / 100;    // clean AND focused
    if (n == 0) quality = 0;

    b.quality = mid(0, quality, 100);
    b.strength = strength;
    b.purity = purity;
    b.dom = dom;
    return b;
}

static int seq_cost(const int *s, int n) {
    int c = 2;                            // base material per unit
    for (int i = 0; i < n; i++) c += RCOST[s[i]];
    return c;
}

// per-unit value of a batch to one client (the pizzatycoon-style demand math)
static float unit_value(Brew b, int cli, float sw) {
    int want = CWANT[cli];
    float match = (b.dom == want && b.eff[want] > 0) ? 1.3f
                : b.eff[want] > 0                     ? 0.65f : 0.35f;
    float strf = 1.0f + mid(0, b.strength, 12) / 12.0f * 0.8f;   // up to +80%
    return 24.0f * (b.quality / 100.0f) * match * strf * (CWEALTH[cli] / 100.0f) * sw;
}
static int client_profit(Brew b, int cli, int cost, float sw) {
    int units = mid(0, CVOL[cli], BATCH_UNITS);
    return (int)(units * (unit_value(b, cli, sw) - cost));
}

// ===========================================================================
static void roll_candidates(void);   // defined with the crew logic below
static void new_swing(void) { swing = 0.82f + noise(day * 0.47f + 3.1f) * 0.40f; }

static void new_game(void) {
    seqn = 0; focus = 0; batched = 0;
    skill = 1; xp = 0; cash = 40; day = 1; showCash = 40;
    w_hired = 0; w_ordern = 0; leave_days = 3; roll_candidates();
    new_swing();
}

void init(void) {
    best = load(0);
    colorkey(0);
    bpm(96);
    instrument(1, INSTR_SQUARE, 2, 80, 2, 80); instrument_duty(1, 0.25f);
    new_game();
    state = TITLE;
}

static void add_reagent(int r) {
    if (seqn >= max_slots()) { nope(); return; }
    seq[seqn++] = r; batched = 0; focus = 0;
    blip(60 + r * 3);
}
static void pop_at(int i) {
    for (int k = i; k < seqn - 1; k++) seq[k] = seq[k + 1];
    seqn--; batched = 0; focus = 0; blip(50);
}

static void do_stir(void) {                 // lock the batch with a focus bonus
    float d = stir_t - 0.5f; if (d < 0) d = -d;
    float closeness = 1.0f - d * 2.0f;                      // 1 at centre
    focus = (int)(closeness * closeness * 15);              // reward the sweet spot
    batched = 1;
    if (focus > 11) { ching(); shake(2); } else blip(72);
}

static void sell_to(int cli) {
    Brew b = brew_eval(seq, seqn, skill, focus);
    int cost = seq_cost(seq, seqn);
    int profit = client_profit(b, cli, cost, swing);
    cash += profit;
    sold_cli = cli; sold_profit = profit; sold_units = mid(0, CVOL[cli], BATCH_UNITS);
    xp++; if (skill < 10 && xp >= skill * 2) { xp = 0; skill++; }   // learn by doing
    day++; new_swing();
    batched = 0; focus = 0;
    if (cash > best) { best = cash; save(0, best); }
    sold_t = 0; state = SOLD;
    if (profit >= 0) ching(); else nope();
}

static int best_client(Brew b, int cost) {
    int bd = 0, bv = -999999;
    for (int c = 0; c < NCLI; c++) { int v = client_profit(b, c, cost, swing); if (v > bv) { bv = v; bd = c; } }
    return bd;
}

// ---- crew ------------------------------------------------------------------
static void roll_candidates(void) {
    int used[8] = { 0 };
    for (int i = 0; i < NCAND; i++) {
        int n; do { n = rnd(8); } while (used[n]); used[n] = 1;
        cand[i].name  = CAND_NAMES[n];
        cand[i].skill = 2 + rnd(4);              // 2..5 (you start at 1 and outpace them)
        cand[i].wage  = cand[i].skill * 2 + 1;   // 5..11 / day
    }
}
static void hire(int i) {
    int fee = cand[i].wage * 4;
    if (cash < fee) { nope(); return; }
    cash -= fee;
    w_hired = 1; w_name = cand[i].name; w_skill = cand[i].skill; w_wage = cand[i].wage; w_morale = 70;
    w_ordern = 0;
    ching();
}
static void assign_order(void) {
    if (seqn == 0) { nope(); return; }
    for (int i = 0; i < seqn; i++) w_order[i] = seq[i];
    w_ordern = seqn; blip(74);
}
static void give_raise(void) { w_wage += 2; w_morale = mid(0, w_morale + 30, 100); blip(76); }
static void fire_worker(void) { w_hired = 0; roll_candidates(); blip(40); }

// the line runs without you: closed-form resolve over N days at the worker's skill
static void simulate_away(int N) {
    rp_days = N; rp_batches = rp_spoiled = rp_gross = rp_wages = rp_qsum = rp_levelup = rp_raise = 0;
    int start_skill = w_skill;
    int cost = seq_cost(w_order, w_ordern);
    for (int d = 0; d < N; d++) {
        day++; new_swing();
        rp_wages += w_wage;
        Brew wb = brew_eval(w_order, w_ordern, w_skill, 0);    // focus 0 — no stir bonus
        int spoil = mid(2, 22 - w_skill * 2 + (w_morale < 40 ? 14 : 0), 45);
        if (chance(spoil)) rp_spoiled++;
        else { rp_gross += client_profit(wb, best_client(wb, cost), cost, swing); rp_batches++; rp_qsum += wb.quality; }
        if (w_skill < 10 && chance(22)) w_skill++;             // learn by doing
        w_morale = mid(0, w_morale - 4, 100);                  // the grind wears them down
    }
    cash += rp_gross - rp_wages;
    rp_levelup = w_skill - start_skill;
    rp_qyou = brew_eval(w_order, w_ordern, skill, 11).quality; // your own hand-made version, for the gap
    rp_raise = (w_morale < 40);
    if (cash > best) { best = cash; save(0, best); }
    rp_t = 0; state = REPORT;
}

// ===========================================================================
void update(void) {
    showCash = lerp(showCash, (float)cash, 0.2f);

    if (state == TITLE) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A)) state = BENCH;
        return;
    }
    if (state == SOLD) {
        sold_t = clamp(sold_t + dt() * 1.6f, 0, 2);
        if (sold_t > 0.4f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || keyp(KEY_SPACE)))
            state = BENCH;
        return;
    }

    if (state == BENCH) {
        if (!batched) {                            // marker bounces until you lock it in
            stir_t += stir_dir * dt() * 1.3f;
            if (stir_t > 1) { stir_t = 1; stir_dir = -1; }
            if (stir_t < 0) { stir_t = 0; stir_dir = 1; }
        }
        for (int r = 0; r < NADD; r++) {           // reagent palette (bottom row)
            int bx = 6 + r * 52;
            if (clicked(bx, 168, 48, 26) || keyp('1' + r)) add_reagent(r);
        }
        for (int i = 0; i < seqn; i++)              // click a chip to remove it
            if (clicked(8 + i * 44, 96, 40, 22)) { pop_at(i); break; }
        if (keyp(KEY_BACKSPACE) && seqn > 0) pop_at(seqn - 1);
        if (clicked(232, 96, 44, 16)) { seqn = 0; batched = 0; focus = 0; blip(40); }  // CLEAR

        // tap the moving bar ITSELF to lock the stir; tap again to retry
        if ((clicked(8, 136, 252, 14) || keyp(KEY_SPACE)) && seqn > 0) {
            if (batched) { batched = 0; focus = 0; }
            else do_stir();
        }
        if (clicked(268, 134, 46, 22) || keyp(KEY_ENTER)) {
            if (seqn == 0) nope();
            else { if (!batched) do_stir(); state = MARKET; }
        }
        if (clicked(244, 50, 72, 16) || keyp('C')) state = MANAGE;   // crew / delegation
        return;
    }

    if (state == MANAGE) {
        if (!w_hired) {
            for (int i = 0; i < NCAND; i++)
                if (clicked(8, 40 + i * 30, 304, 26)) { hire(i); return; }
        } else {
            if (clicked(8, 150, 120, 16)) assign_order();
            if (clicked(134, 150, 80, 16)) give_raise();
            if (clicked(220, 150, 50, 16)) fire_worker();
            if (clicked(120, 172, 16, 16)) leave_days = mid(1, leave_days - 1, 7);
            if (clicked(160, 172, 16, 16)) leave_days = mid(1, leave_days + 1, 7);
            if (clicked(190, 170, 122, 20)) { if (w_ordern > 0) simulate_away(leave_days); else nope(); }
        }
        if (clicked(6, 14, 56, 12) || keyp(KEY_ESCAPE) || keyp(KEY_TAB) || keyp('C')) state = BENCH;
        return;
    }

    if (state == REPORT) {
        rp_t = clamp(rp_t + dt() * 1.6f, 0, 2);
        if (rp_t > 0.4f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || keyp(KEY_SPACE)))
            state = BENCH;
        return;
    }

    if (state == MARKET) {
        for (int c = 0; c < NCLI; c++)
            if (clicked(8, 30 + c * 30, 304, 28)) { sell_to(c); return; }
        if (keyp(KEY_ENTER)) {
            Brew b = brew_eval(seq, seqn, skill, focus);
            sell_to(best_client(b, seq_cost(seq, seqn)));
            return;
        }
        if (clicked(6, 180, 56, 16) || keyp(KEY_ESCAPE) || keyp(KEY_TAB)) state = BENCH;
        return;
    }
}

// ===========================================================================
static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("$%d", (int)showCash), 4, 2, CLR_YELLOW);
    print_centered(str("DAY %d", day), SCREEN_W / 2, 2, CLR_LIGHT_GREY);
    print_right(str("SKILL %d", skill), 316, 2, CLR_LIME_GREEN);
}

// the flask: liquid coloured by dominant effect, height by strength, murk by
// low quality (the at-a-glance read of the three numbers)
static void draw_flask(int cx, int cy, Brew b) {
    int top = cy - 34, bot = cy + 4, w = 22;
    rect(cx - w / 2, top, w, bot - top, CLR_LIGHT_GREY);            // glass
    rectfill(cx - 6, top - 6, 12, 6, CLR_DARK_GREY);                // neck
    if (seqn > 0) {
        int fillh = 6 + mid(0, b.strength, 12) * 2;                  // strength = height
        int ly = bot - fillh; if (ly < top + 2) ly = top + 2;
        rectfill(cx - w / 2 + 2, ly, w - 4, bot - ly, ECOL[b.dom]);
        if (b.quality < 55) {                                       // murky = low quality
            fillp(b.quality < 35 ? FILL_CHECKER : FILL_DOTS, -1);
            rectfill(cx - w / 2 + 2, ly, w - 4, bot - ly, CLR_BROWNISH_BLACK);
            fillp_reset();
        }
        for (int i = 0; i < 3; i++) if (blink(14 + i * 5))           // bubbles
            pset(cx - 4 + i * 4, ly + 4 + (i * 3), CLR_WHITE);
    }
}

static void draw_bench(void) {
    cls(CLR_DARKER_PURPLE);
    rectfill(0, 11, SCREEN_W, 78, CLR_DARK_PURPLE);

    Brew b = brew_eval(seq, seqn, skill, focus);
    int cost = seq_cost(seq, seqn);

    // ---- product readout (left): flask + the three numbers ----
    draw_flask(40, 64, b);
    print("QUALITY", 70, 18, CLR_LIGHT_GREY);
    print_scaled(str("%d", b.quality), 70, 26, b.quality >= 70 ? CLR_LIME_GREEN : b.quality >= 45 ? CLR_YELLOW : CLR_ORANGE, 2);
    print("STRENGTH", 150, 18, CLR_LIGHT_GREY);
    print_scaled(str("%d", b.strength), 150, 26, CLR_BLUE, 2);
    print("EFFECT", 230, 18, CLR_LIGHT_GREY);
    const char *en = seqn ? ENAME[b.dom] : "--";
    print_scaled(en, 316 - text_width(en) * 2, 26, seqn ? ECOL[b.dom] : CLR_DARK_GREY, 2);
    print("PURITY", 70, 48, CLR_LIGHT_GREY);
    bar(120, 49, 84, 6, b.purity / 100.0f, CLR_PINK, CLR_BROWNISH_BLACK);
    print(str("cost $%d  best $%d", cost, client_profit(b, best_client(b, cost), cost, swing)), 70, 60, CLR_LIGHT_GREY);

    // CREW button — the door to delegation (v2)
    bool hcr = hover(244, 50, 72, 16);
    rectfill(244, 50, 72, 16, hcr ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK); rect(244, 50, 72, 16, CLR_LIME_GREEN);
    print("CREW", 248, 52, CLR_LIME_GREEN);
    print(w_hired ? w_name : "hire help", 248, 60, w_hired ? CLR_WHITE : CLR_DARK_GREY);

    // ---- the recipe sequence (chips) ----
    print(str("RECIPE  (%d/%d)", seqn, max_slots()), 8, 84, CLR_LIGHT_YELLOW);
    for (int i = 0; i < max_slots(); i++) {
        int x = 8 + i * 44;
        if (i < seqn) {
            int r = seq[i];
            bool hv = hover(x, 96, 40, 22);
            rectfill(x, 96, 40, 22, RCOL[r]); rect(x, 96, 40, 22, hv ? CLR_WHITE : CLR_BLACK);
            print(RSHORT[r], x + (40 - text_width(RSHORT[r])) / 2, 102, CLR_BLACK);
            // synergy marker with the previous chip
            if (i > 0) {
                int s = SYN_STR[seq[i - 1]][r] + SYN_PUR[seq[i - 1]][r];
                if (seq[i - 1] == seq[i]) s = -1;
                if (seq[i - 1] == R_SOLV && RPUR[r] < 0) s = 1;
                if (s) { int c = s > 0 ? CLR_LIME_GREEN : CLR_RED; print(s > 0 ? "+" : "x", x - 6, 102, c); }
            }
        } else {
            rect(x, 96, 40, 22, CLR_DARKER_GREY);
            if (i >= seqn) print(i < max_slots() ? "+" : "", x + 17, 102, CLR_DARKER_GREY);
        }
    }
    bool hc = hover(232, 96, 44, 16);
    if (seqn > 0) { rectfill(232, 96, 44, 16, hc ? CLR_DARK_RED : CLR_BROWNISH_BLACK); rect(232, 96, 44, 16, CLR_RED); print("CLEAR", 238, 100, CLR_LIGHT_PEACH); }

    // ---- stir: TAP THE BAR ITSELF when the marker's in the green ----
    print(batched ? "STIRRED: tap the bar to redo" : "STIR: tap the bar in the green zone",
          8, 124, batched ? CLR_LIME_GREEN : CLR_LIGHT_GREY);
    int mx = 8, mw = 252;
    bool hs = hover(mx, 136, mw, 14);
    rectfill(mx, 136, mw, 14, CLR_BROWNISH_BLACK);
    int gz = mx + (int)(mw * 0.40f), gw = (int)(mw * 0.20f);
    rectfill(gz, 136, gw, 14, CLR_DARK_GREEN);                          // the sweet spot
    rect(mx, 136, mw, 14, batched ? CLR_GREEN : (hs || blink(22)) ? CLR_WHITE : CLR_LIGHT_GREY);
    int mk = mx + (int)(stir_t * mw);
    rectfill(mk - 1, 133, 3, 20, batched ? CLR_YELLOW : CLR_WHITE);     // marker freezes once locked
    if (batched) {
        const char *q = focus > 11 ? "GREAT!" : focus > 6 ? "GOOD" : "SLOPPY";
        print(str("focus +%d  %s", focus, q), mx + 6, 139, focus > 6 ? CLR_BLACK : CLR_WHITE);
    }
    // SELL
    bool hm = hover(268, 134, 46, 22);
    rectfill(268, 134, 46, 22, hm ? CLR_DARK_PURPLE : CLR_DARKER_GREY); rect(268, 134, 46, 22, CLR_LIGHT_YELLOW);
    print("SELL", 268 + (46 - text_width("SELL")) / 2, 137, CLR_LIGHT_YELLOW);
    print(">", 268 + (46 - text_width(">")) / 2, 145, CLR_LIGHT_YELLOW);

    // ---- reagent palette ----
    for (int r = 0; r < NADD; r++) {
        int bx = 6 + r * 52;
        bool hv = hover(bx, 168, 48, 26);
        rectfill(bx, 168, 48, 26, hv ? CLR_DARK_GREY : CLR_BROWNISH_BLACK); rect(bx, 168, 48, 26, RCOL[r]);
        rectfill(bx + 3, 171, 5, 5, RCOL[r]);
        print(RSHORT[r], bx + 11, 171, CLR_WHITE);
        const char *fx = REFF[r] >= 0 ? ENAME[REFF[r]] : "REFINE";
        print(fx, bx + 3, 180, REFF[r] >= 0 ? ECOL[REFF[r]] : CLR_LIGHT_GREY);
        print(str("$%d", RCOST[r]), bx + 3, 188, CLR_DARK_GREY);
        print(str("%d", r + 1), bx + 40, 188, CLR_DARKER_GREY);
    }
}

static void draw_market(void) {
    cls(CLR_DARKER_BLUE);
    print_centered("MARKET: who wants this batch?", SCREEN_W / 2, 13, CLR_LIGHT_YELLOW);

    Brew b = brew_eval(seq, seqn, skill, focus);
    int cost = seq_cost(seq, seqn);
    int bestc = best_client(b, cost);

    for (int c = 0; c < NCLI; c++) {
        int y = 26 + c * 28;
        bool hv = hover(8, y, 304, 25);
        bool fit = (b.dom == CWANT[c] && b.eff[CWANT[c]] > 0);
        rectfill(8, y, 304, 25, hv ? CLR_DARK_GREY : CLR_BLACK);
        rect(8, y, 304, 25, c == bestc ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        print(CNAME[c], 14, y + 3, CLR_WHITE);
        print(str("wants %s", ENAME[CWANT[c]]), 14, y + 13, fit ? CLR_LIME_GREEN : ECOL[CWANT[c]]);
        print_right(str("wealth %d%%", CWEALTH[c]), 210, y + 3, CLR_DARK_GREY);
        int p = client_profit(b, c, cost, swing);
        print_right(str("$%d", p), 306, y + 6, p >= 0 ? CLR_LIME_GREEN : CLR_RED);
        if (fit && blink(18)) print_right("GOOD FIT", 306, y + 16, CLR_LIME_GREEN);
    }
    print(str("QUAL %d  STR %d  %s  x%d   mood %d%%", b.quality, b.strength, ENAME[b.dom], BATCH_UNITS, (int)(swing * 100)), 10, 168, CLR_LIGHT_GREY);

    bool hb = hover(6, 180, 56, 16);
    rectfill(6, 180, 56, 16, hb ? CLR_DARK_GREY : CLR_BROWNISH_BLACK); rect(6, 180, 56, 16, CLR_LIGHT_YELLOW);
    print("< BENCH", 11, 184, CLR_LIGHT_YELLOW);
    print_right("ENTER = sell to best", 314, 184, CLR_DARK_GREY);
}

static void draw_sold(void) {
    draw_market();
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();
    int w = 220, x = (SCREEN_W - w) / 2, y = 60, h = 84;
    rectfill(x, y, w, h, CLR_BLACK); rect(x, y, w, h, sold_profit >= 0 ? CLR_LIME_GREEN : CLR_RED);
    print_centered(str("SOLD TO %s", CNAME[sold_cli]), SCREEN_W / 2, y + 10, CLR_LIGHT_YELLOW);
    int tally = (int)lerp(0, (float)sold_profit, ease_out(clamp(sold_t, 0, 1)));
    print_scaled(str("$%d", tally), x + (w - text_width(str("$%d", tally)) * 2) / 2, y + 28, sold_profit >= 0 ? CLR_LIME_GREEN : CLR_RED, 2);
    print_centered(str("%d units", sold_units), SCREEN_W / 2, y + 52, CLR_LIGHT_GREY);
    if (sold_t > 0.4f && blink(20)) print_centered("click to brew the next batch", SCREEN_W / 2, y + 68, CLR_DARK_GREY);
}

static void draw_title(void) {
    cls(CLR_DARKER_PURPLE);
    draw_flask(160, 96, brew_eval((int[]){ R_POPPY, R_HERB, R_POPPY }, 3, 6, 12));
    print_scaled("THE CUT", (SCREEN_W - text_width("THE CUT") * 3) / 2, 110, CLR_LIGHT_YELLOW, 3);
    print_centered("mix, stir, sell to the right client", SCREEN_W / 2, 140, CLR_LIGHT_GREY);
    print_centered("order matters: reagents clash or combo", SCREEN_W / 2, 152, CLR_DARK_GREY);
    print_centered(str("best stash: $%d", best), SCREEN_W / 2, 168, CLR_YELLOW);
    print_centered("click / ENTER to open the bench", SCREEN_W / 2, 184, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

// ---- a compact chip row (the standing order) -------------------------------
static void draw_chips(const int *s, int n, int x0, int y) {
    for (int i = 0; i < n; i++) {
        int x = x0 + i * 38, r = s[i];
        rectfill(x, y, 34, 16, RCOL[r]); rect(x, y, 34, 16, CLR_BLACK);
        print(RSHORT[r], x + (34 - text_width(RSHORT[r])) / 2, y + 5, CLR_BLACK);
    }
}

// ---- crew / standing-orders screen -----------------------------------------
static void draw_manage(void) {
    cls(CLR_DARKER_BLUE);
    bool hb = hover(6, 14, 56, 12);
    rectfill(6, 14, 56, 12, hb ? CLR_DARK_GREY : CLR_BROWNISH_BLACK); rect(6, 14, 56, 12, CLR_LIGHT_YELLOW);
    print("< BENCH", 10, 16, CLR_LIGHT_YELLOW);
    print_centered("CREW & STANDING ORDERS", SCREEN_W / 2, 16, CLR_LIGHT_YELLOW);

    if (!w_hired) {
        print("Hire someone to run the bench while you're away:", 8, 30, CLR_LIGHT_GREY);
        for (int i = 0; i < NCAND; i++) {
            int y = 40 + i * 30; bool hv = hover(8, y, 304, 26);
            int fee = cand[i].wage * 4; bool afford = cash >= fee;
            rectfill(8, y, 304, 26, hv ? CLR_DARK_GREY : CLR_BLACK); rect(8, y, 304, 26, afford ? CLR_LIME_GREEN : CLR_DARKER_GREY);
            print(cand[i].name, 14, y + 3, CLR_WHITE);
            print(str("skill %d", cand[i].skill), 14, y + 14, CLR_LIME_GREEN);
            print_right(str("$%d/day", cand[i].wage), 210, y + 3, CLR_LIGHT_GREY);
            print_right(afford ? str("hire $%d", fee) : str("need $%d", fee), 306, y + 9, afford ? CLR_YELLOW : CLR_RED);
        }
        print("higher skill = cleaner, but costs more", 8, 138, CLR_DARK_GREY);
        print("they work at their skill, no stir bonus", 8, 152, CLR_DARK_GREY);
        return;
    }

    // hired worker card
    rectfill(8, 30, 304, 64, CLR_BLACK); rect(8, 30, 304, 64, CLR_LIME_GREEN);
    print(w_name, 14, 34, CLR_WHITE);
    print(str("skill %d", w_skill), 14, 46, CLR_LIME_GREEN);
    print(str("wage $%d/day", w_wage), 90, 46, CLR_LIGHT_GREY);
    print("morale", 200, 34, CLR_DARK_GREY);
    bar(200, 44, 100, 7, w_morale / 100.0f, w_morale < 40 ? CLR_RED : CLR_LIME_GREEN, CLR_BROWNISH_BLACK);
    if (w_morale < 40 && blink(20)) print_right("wants a raise!", 306, 56, CLR_RED);
    print("ORDER:", 14, 64, CLR_LIGHT_YELLOW);
    if (w_ordern > 0) {
        draw_chips(w_order, w_ordern, 70, 62);
        print(str("they'd make ~quality %d", brew_eval(w_order, w_ordern, w_skill, 0).quality), 14, 80, CLR_LIGHT_GREY);
    } else print("none yet - tap ASSIGN below", 70, 66, CLR_DARK_GREY);

    print("while you're gone they run the order,", 8, 100, CLR_DARK_GREY);
    print("auto-sell daily; you keep value - wage.", 8, 112, CLR_DARK_GREY);

    // buttons
    bool ha = hover(8, 150, 120, 16);
    rectfill(8, 150, 120, 16, ha ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK); rect(8, 150, 120, 16, CLR_GREEN);
    print("ASSIGN RECIPE", 12, 154, CLR_WHITE);
    bool hr = hover(134, 150, 80, 16);
    rectfill(134, 150, 80, 16, hr ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK); rect(134, 150, 80, 16, CLR_LIME_GREEN);
    print("RAISE +$2", 138, 154, CLR_WHITE);
    bool hf = hover(220, 150, 50, 16);
    rectfill(220, 150, 50, 16, hf ? CLR_DARK_RED : CLR_BROWNISH_BLACK); rect(220, 150, 50, 16, CLR_RED);
    print("FIRE", 232, 154, CLR_WHITE);

    // leave control
    print("LEAVE FOR", 8, 175, CLR_LIGHT_GREY);
    bool hm = hover(120, 172, 16, 16), hp = hover(160, 172, 16, 16);
    rectfill(120, 172, 16, 16, hm ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(120, 172, 16, 16, CLR_RED);   print("-", 126, 175, CLR_WHITE);
    print_scaled(str("%d", leave_days), 142, 172, CLR_YELLOW, 2);
    rectfill(160, 172, 16, 16, hp ? CLR_DARK_GREY : CLR_DARKER_GREY); rect(160, 172, 16, 16, CLR_GREEN); print("+", 165, 175, CLR_WHITE);
    bool can = w_ordern > 0, hl = hover(190, 170, 122, 20);
    rectfill(190, 170, 122, 20, hl && can ? CLR_DARK_PURPLE : CLR_DARKER_GREY); rect(190, 170, 122, 20, can ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
    const char *ll = can ? "LEAVE & WORK" : "assign first";
    print(ll, 190 + (122 - text_width(ll)) / 2, 176, can ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
}

// ---- resolve-on-return ledger ----------------------------------------------
static void draw_report(void) {
    cls(CLR_DARKER_BLUE);
    int x = 30, y = 26, w = 260, h = 150, net = rp_gross - rp_wages;
    rectfill(x, y, w, h, CLR_BLACK); rect(x, y, w, h, net >= 0 ? CLR_LIME_GREEN : CLR_RED);
    print_centered(str("BACK AFTER %d DAYS", rp_days), SCREEN_W / 2, y + 8, CLR_LIGHT_YELLOW);
    int avgq = rp_batches ? rp_qsum / rp_batches : 0;
    print(str("%s made %d batches", w_name, rp_batches), x + 12, y + 26, CLR_WHITE);
    if (rp_spoiled) print_right(str("%d spoiled", rp_spoiled), x + w - 12, y + 26, CLR_ORANGE);
    print(str("avg quality %d", avgq), x + 12, y + 40, CLR_LIGHT_GREY);
    print(str("your hand: %d", rp_qyou), x + 150, y + 40, CLR_DARK_GREY);
    print(str("gross   $%d", rp_gross), x + 12, y + 60, CLR_LIME_GREEN);
    print(str("wages  -$%d", rp_wages), x + 12, y + 72, CLR_RED);
    line(x + 12, y + 86, x + 140, y + 86, CLR_DARK_GREY);
    int tally = (int)lerp(0, (float)net, ease_out(clamp(rp_t, 0, 1)));
    print(str("net     $%d", tally), x + 12, y + 92, net >= 0 ? CLR_YELLOW : CLR_RED);
    if (rp_levelup) print(str("%s reached skill %d!", w_name, w_skill), x + 12, y + 114, CLR_LIME_GREEN);
    else if (rp_raise) print(str("%s wants a raise", w_name), x + 12, y + 114, CLR_ORANGE);
    if (avgq && avgq < rp_qyou - 8) print("the quality gap: your hands still beat theirs", x + 12, y + 128, CLR_DARK_GREY);
    if (rp_t > 0.4f && blink(20)) print_centered("click to continue", SCREEN_W / 2, y + h - 12, CLR_DARK_GREY);
}

// ===========================================================================
void draw(void) {
    if (state == TITLE)  { draw_title(); return; }
    if (state == BENCH)  { draw_bench();  draw_hud(); return; }
    if (state == MARKET) { draw_market(); draw_hud(); return; }
    if (state == SOLD)   { draw_sold();   draw_hud(); return; }
    if (state == MANAGE) { draw_manage(); draw_hud(); return; }
    if (state == REPORT) { draw_report(); draw_hud(); return; }
}
