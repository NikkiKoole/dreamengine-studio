/* de:meta
{
  "title": "Strip Poker",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine"
  ],
  "lineage": "Extends poker.c's deck/hand-eval core and borrows smooch.c's lounge tone; adds a heads-up five-card draw betting loop, a tiered reveal meter, and character animation — poker as a flirtation escalation system.",
  "genre": "tabletop",
  "description": "A velvet after-hours lounge where you play heads-up five-card draw against Sable, your silhouette sweetheart. Ante up, click cards to HOLD, then DRAW and choose to BET, CALL, or FOLD; at showdown the best hand takes the pot. Win and the cheeky REVEAL meter lights a notch — fill it and Sable strikes a coy new pose, blows a kiss, and the flirtation escalates a tier (TEASE to SCANDALOUS); lose and the meter slips. Strictly PG-13: soft spotlit silhouettes, a fluttering feather boa, a wink, and double-entendre banter over a slow sultry lounge bass. Mouse: click your cards to HOLD, click BET / CALL / FOLD, click DEAL to start a hand and advance the title / game over. Keys: arrows pick a card, Z hold, B bet, C call, F fold, SPACE or ENTER deal & advance."
}
de:meta */
#include "studio.h"
#include "cards.h"
#include <stddef.h>

// ── STRIP POKER (PG-13) — a velvet-lounge card flirtation ─────────
//
// 5-card draw, heads-up against Sable, your silhouette sweetheart.
// Win a showdown and the REVEAL meter nudges up a notch; lose and it
// slips. Fill it and Sable strikes a coy new pose and throws a wink.
// Art stays PG-13: soft silhouettes, a feather boa, double-entendre
// banter — never explicit. Smooch-lounge cheek at a poker table.
//
//   mouse: click your cards to HOLD, click BET / CALL / FOLD,
//          click DEAL to start a hand / advance the title & game over
//   keys : <- -> pick a card, Z hold, B bet, C call, F fold,
//          SPACE / ENTER deal & advance
//
// Extends poker.c's deal + hand-eval; borrows smooch.c's lounge tone.

// card: rank 0..12 = 2..10,J,Q,K,A   suit 0..3 = heart diamond club spade
typedef struct { int rank, suit; } Card;
static const char *RSTR[13] = { "2","3","4","5","6","7","8","9","10","J","Q","K","A" };

// hand categories, low→high
enum { H_HIGH, H_PAIR, H_TWOPAIR, H_TRIPS, H_STRAIGHT, H_FLUSH,
       H_FULL, H_QUADS, H_STRFLUSH, H_ROYAL };
static const char *HNAME[10] = { "HIGH CARD","PAIR","TWO PAIR","3 OF A KIND","STRAIGHT",
                                 "FLUSH","FULL HOUSE","4 OF A KIND","STRAIGHT FLUSH","ROYAL FLUSH" };

// ── layout ────────────────────────────────────────────────────────
#define CW 42
#define CH 58
#define GAP 6
#define HAND_W (5 * CW + 4 * GAP)
#define MY_X  ((SCREEN_W - HAND_W) / 2)
#define MY_Y  126
#define OPP_X ((SCREEN_W - HAND_W) / 2)
#define OPP_Y 20

// ── game state ────────────────────────────────────────────────────
enum { ST_TITLE, ST_HOLD, ST_BET, ST_SHOWDOWN, ST_OVER };

static Card deck[52];
static Card you[5], opp[5];
static bool held[5];
static int  deckpos;

static int  state = ST_TITLE;
static int  credits = 30;
static int  oppcred = 30;
static int  pot;
static int  sel;
static bool ready;

static int  you_cat, opp_cat, you_key, opp_key;   // showdown results
static int  outcome;                              // -1 lose, 0 tie, 1 win
static int  reveal;                               // 0..5 notches in this tier
static int  tier;                                 // escalation tier 0..3
static int  best_tier;                            // saved meta-score
static bool show_opp;                             // are Sable's cards face up?

// per-card flip animation: 0 = face down, 1 = fully face up
static float flip_you[5], flip_opp[5];

// flirty banter
static const char *banter = "";
static int   banter_col = CLR_WHITE;
static float banter_t;

// sable animation
static float wink_t;        // >0 = winking
static float pose_t;        // squash on a pose change
static float ambient;       // shuffle-riffle visual timer

static const char *TIER_NAME[4] = { "TEASE", "FLIRT", "SWOON", "SCANDALOUS" };

static const char *WIN_LINES[] = {
    "ooh, you cheat...",
    "lucky hands, hot stuff",
    "*the boa slips a little*",
    "well well WELL",
    "you've got my attention",
};
static const char *LOSE_LINES[] = {
    "not so fast, darling",
    "*fans self, smirking*",
    "is that ALL you've got?",
    "buy a girl a drink first",
    "mmm, try harder",
};
static const char *TIE_LINES[] = {
    "a draw? how tense.",
    "you and me, even...",
    "deadlock, sweetheart",
};
static const char *FOLD_LINES[] = {
    "folding already? tsk.",
    "cold feet, cowboy?",
    "*pouts*",
};

// ── helpers ───────────────────────────────────────────────────────
static void say(const char *t, int col) { banter = t; banter_col = col; banter_t = 2.6f; }

static void build_deck(void) {
    int n = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 0; r < 13; r++) { deck[n].rank = r; deck[n].suit = s; n++; }
}
static void shuffle(void) {
    for (int i = 51; i > 0; i--) {
        int j = rnd(i + 1);
        Card t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
    deckpos = 0;
    ambient = 0.5f;
}

// evaluate a 5-card hand → category, and a tiebreak key (sum of pip ranks,
// weighted so the made-hand cards dominate). good enough for heads-up.
static int eval_hand(Card *h, int *out_key) {
    int rc[13] = {0}, sc[4] = {0};
    for (int i = 0; i < 5; i++) { rc[h[i].rank]++; sc[h[i].suit]++; }

    bool flush = false;
    for (int s = 0; s < 4; s++) if (sc[s] == 5) flush = true;

    int distinct = 0, lo = -1, hi = -1;
    for (int r = 0; r < 13; r++) if (rc[r]) { distinct++; if (lo < 0) lo = r; hi = r; }
    bool straight = false, royal = false;
    if (distinct == 5) {
        if (hi - lo == 4) straight = true;
        if (rc[12] && rc[0] && rc[1] && rc[2] && rc[3]) straight = true;        // wheel
        if (rc[12] && rc[11] && rc[10] && rc[9] && rc[8]) royal = true;
    }

    int four = -1, three = -1, hipair = -1, lopair = -1;
    for (int r = 0; r < 13; r++) {
        if (rc[r] == 4) four = r;
        else if (rc[r] == 3) three = r;
        else if (rc[r] == 2) { if (r > hipair) { lopair = hipair; hipair = r; } else lopair = r; }
    }

    // tiebreak key: dominant group ranks scaled up, then kickers as the high card sum
    int kick = 0;
    for (int r = 0; r < 13; r++) if (rc[r] == 1) kick = kick * 13 + (r + 2);
    int cat, key;
    if (straight && flush && royal) { cat = H_ROYAL;    key = 0; }
    else if (straight && flush)     { cat = H_STRFLUSH; key = hi; }
    else if (four >= 0)             { cat = H_QUADS;    key = four * 100 + kick; }
    else if (three >= 0 && hipair >= 0) { cat = H_FULL; key = three * 13 + hipair; }
    else if (flush)                 { cat = H_FLUSH;    key = kick; }
    else if (straight)              { cat = H_STRAIGHT; key = hi; }
    else if (three >= 0)            { cat = H_TRIPS;    key = three * 1000 + kick; }
    else if (lopair >= 0)           { cat = H_TWOPAIR;  key = hipair * 100 + lopair * 10 + kick % 13; }
    else if (hipair >= 0)           { cat = H_PAIR;     key = hipair * 1000 + kick; }
    else                            { cat = H_HIGH;     key = kick; }
    if (out_key) *out_key = key;
    return cat;
}

// ── Sable's draw AI: keep pairs / better, otherwise chase high cards ──
static void opp_choose_draw(void) {
    int rc[13] = {0};
    for (int i = 0; i < 5; i++) rc[opp[i].rank]++;
    for (int i = 0; i < 5; i++) {
        int r = opp[i].rank;
        bool keep = rc[r] >= 2 || r >= 10;            // pairs+ or face cards/aces
        if (!keep && chance(15)) keep = true;          // a little unpredictability
        if (!keep) { opp[i] = deck[deckpos++]; flip_opp[i] = 0; }
    }
}

// ── hand flow ─────────────────────────────────────────────────────
static void reset_game(void) {
    build_deck();
    credits = 30; oppcred = 30; pot = 0;
    state = ST_TITLE; sel = 0; reveal = 0; tier = 0; outcome = 0;
    show_opp = false; banter = ""; banter_t = 0;
    ready = true;
}

static void deal_hand(void) {
    int ante = 2;
    if (credits < ante || oppcred < ante) { state = ST_OVER; return; }
    credits -= ante; oppcred -= ante; pot = ante * 2;
    shuffle();
    for (int i = 0; i < 5; i++) {
        you[i] = deck[deckpos++]; held[i] = false; flip_you[i] = 0;
        opp[i] = deck[deckpos++]; flip_opp[i] = 0;
    }
    show_opp = false; sel = 0; outcome = 0;
    state = ST_HOLD;
    say("click cards to HOLD, then DRAW", CLR_LIGHT_PEACH);
    sfx(-1); note(50, INSTR_NOISE, 2);
}

static void do_draw(void) {
    for (int i = 0; i < 5; i++) if (!held[i]) { you[i] = deck[deckpos++]; flip_you[i] = 0; }
    opp_choose_draw();
    note(55, INSTR_NOISE, 2);
    state = ST_BET;
    say("BET, CALL, or FOLD?", CLR_YELLOW);
}

static void notch_up(void) {
    reveal++;
    note(72, INSTR_SINE, 3);
    if (reveal >= 5) {                                  // BIG REVEAL
        reveal = 2;
        if (tier < 3) tier++;
        pose_t = 1; wink_t = 1; shake(3);
        strum(57, CHORD_MAJ7, INSTR_SAW, 5, 36);
        say(str("*%s*", TIER_NAME[tier]), CLR_PINK);
        if (tier > best_tier) { best_tier = tier; save_int("strip_tier", best_tier); }
    }
}

static void showdown(void) {
    show_opp = true;
    you_cat = eval_hand(you, &you_key);
    opp_cat = eval_hand(opp, &opp_key);
    if (you_cat != opp_cat) outcome = (you_cat > opp_cat) ? 1 : -1;
    else if (you_key != opp_key) outcome = (you_key > opp_key) ? 1 : -1;
    else outcome = 0;

    if (outcome > 0) {
        credits += pot;
        notch_up();
        say(WIN_LINES[rnd(5)], CLR_PINK);
        wink_t = 1;
    } else if (outcome < 0) {
        oppcred += pot;
        reveal = (reveal > 0) ? reveal - 1 : 0;
        say(LOSE_LINES[rnd(5)], CLR_PEACH);
        note(45, INSTR_TRI, 3);
    } else {
        credits += pot / 2; oppcred += pot / 2;
        say(TIE_LINES[rnd(3)], CLR_LIGHT_GREY);
    }
    pot = 0;
    state = ST_SHOWDOWN;
}

// the bet round resolves into a showdown (or a fold)
static void player_bet(void) {            // push another chip; Sable answers
    int b = 3;
    if (credits < b) b = credits;
    credits -= b; pot += b;
    note(64, INSTR_SINE, 2);
    // Sable's call decision based on her hand strength
    int oc = eval_hand(opp, NULL);
    bool sable_calls = oc >= H_PAIR || chance(35);
    int match = (oppcred < b) ? oppcred : b;
    oppcred -= match; pot += match;
    if (!sable_calls && oc == H_HIGH && chance(55)) {
        // Sable folds to the raise — you take it down
        credits += pot; pot = 0;
        outcome = 1; show_opp = true;
        you_cat = eval_hand(you, &you_key);
        notch_up();
        say("*she folds, biting her lip*", CLR_PINK);
        wink_t = 1;
        state = ST_SHOWDOWN;
        return;
    }
    showdown();
}
static void player_call(void) { note(60, INSTR_SINE, 2); showdown(); }
static void player_fold(void) {
    oppcred += pot; pot = 0;
    show_opp = false; outcome = -1;
    reveal = (reveal > 0) ? reveal - 1 : 0;
    say(FOLD_LINES[rnd(3)], CLR_PEACH);
    note(43, INSTR_TRI, 2);
    state = ST_SHOWDOWN;
}

// ── input helpers ─────────────────────────────────────────────────
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && point_in_box(mouse_x(), mouse_y(), x, y, w, h);
}
static bool advance_pressed(void) {
    return keyp(KEY_SPACE) || keyp(KEY_ENTER) || btnp(0, BTN_A) || btnp(0, BTN_B);
}

// ── update ────────────────────────────────────────────────────────
void init(void) {
    best_tier = load_int("strip_tier", 0);
    bpm(72);
    instrument(5, INSTR_TRI, 12, 160, 4, 200);  instrument_filter(5, FILTER_LOW, 620, 4);  // sultry bass
    instrument_lfo(5, 0, LFO_PITCH, 5.0f, 0.18f);
    instrument(6, INSTR_SAW, 8, 140, 4, 220);   instrument_filter(6, FILTER_BAND, 900, 9);  // sax stab
}

// walking lounge bass, two bars of slow quarters
static const int BASS[8] = { 33, 36, 40, 38, 41, 40, 36, 31 };
static int last_q = -999;

static void play_bed(void) {
    int q = beat();
    if (q == last_q) return;
    last_q = q;
    note(BASS[((q % 8) + 8) % 8], 5, 2);
    if (q % 4 == 2) hit(54, INSTR_NOISE, 1, 40);     // brushed hat
}

void update(void) {
    if (!ready) reset_game();
    play_bed();

    banter_t = clamp(banter_t - dt(), 0, 3);
    wink_t   = clamp(wink_t   - dt() * 1.5f, 0, 1);
    pose_t   = clamp(pose_t   - dt() * 1.8f, 0, 1);
    ambient  = clamp(ambient  - dt(), 0, 1);

    // card flips ease toward face-up
    for (int i = 0; i < 5; i++) {
        flip_you[i] = clamp(flip_you[i] + dt() * 6.0f, 0, 1);
        bool opp_face = show_opp || false;
        flip_opp[i]  = clamp(flip_opp[i] + (opp_face ? dt() * 6.0f : 0), 0, 1);
    }

    if (state == ST_TITLE) {
        if (advance_pressed() || clicked(MY_X + HAND_W / 2 - 40, MY_Y + CH + 6, 80, 18))
            deal_hand();
        return;
    }
    if (state == ST_OVER) {
        if (advance_pressed() || mouse_pressed(MOUSE_LEFT)) reset_game();
        return;
    }

    if (state == ST_HOLD) {
        for (int i = 0; i < 5; i++) {
            int x = MY_X + i * (CW + GAP);
            if (clicked(x, MY_Y, CW, CH)) { held[i] = !held[i]; sel = i; note(67, INSTR_SINE, 2); }
        }
        if (btnp(0, BTN_LEFT))  sel = (sel + 4) % 5;
        if (btnp(0, BTN_RIGHT)) sel = (sel + 1) % 5;
        if (keyp('Z') || btnp(0, BTN_A)) { held[sel] = !held[sel]; note(67, INSTR_SINE, 2); }
        // DRAW button
        if (clicked(MY_X + HAND_W / 2 - 40, MY_Y + CH + 6, 80, 18) || advance_pressed())
            do_draw();
        return;
    }

    if (state == ST_BET) {
        int by = MY_Y + CH + 6;
        if (clicked(MY_X, by, 56, 18)               || keyp('B')) player_bet();
        else if (clicked(MY_X + 64, by, 56, 18)     || keyp('C')) player_call();
        else if (clicked(MY_X + 128, by, 56, 18)    || keyp('F')) player_fold();
        return;
    }

    if (state == ST_SHOWDOWN) {
        if (advance_pressed() || clicked(MY_X + HAND_W / 2 - 40, MY_Y + CH + 6, 80, 18)) {
            if (credits < 2 || oppcred < 2) state = ST_OVER;
            else deal_hand();
        }
        return;
    }
}

// ── drawing ───────────────────────────────────────────────────────
static void draw_suit(int suit, int cx, int cy, int col) { card_suit(suit, cx, cy, col, 0.85f); }

// draw a card with a flip envelope (0 = back, 1 = face). squashes in x.
static void draw_card(Card c, int x, int y, float flip, bool gold, bool selring) {
    float ph = clamp(flip * 2.0f, 0, 2.0f);
    float w = (ph < 1.0f) ? CW * (1.0f - ph) : CW * (ph - 1.0f);  // mid = thin
    if (w < 2) w = 2;
    int cx = x + CW / 2;
    int rx = (int)(w / 2);
    int lx = cx - rx, rw = (int)w;

    bool face = flip >= 0.5f;
    if (!face) {
        rectfill(lx, y, rw, CH, CLR_DARK_RED);
        rect(lx, y, rw, CH, CLR_DARK_PURPLE);
        if (rw > 14) {
            for (int yy = y + 4; yy < y + CH - 4; yy += 6)
                line(lx + 3, yy, lx + rw - 4, yy, CLR_DARKER_PURPLE);
        }
        return;
    }
    int col = (c.suit < 2) ? CLR_RED : CLR_BLACK;
    rectfill(lx, y, rw, CH, CLR_WHITE);
    rect(lx, y, rw, CH, CLR_DARK_GREY);
    if (rw > 24) {
        print(RSTR[c.rank], lx + 3, y + 3, col);
        print_right(RSTR[c.rank], lx + rw - 3, y + CH - 11, col);
        draw_suit(c.suit, cx, y + CH / 2, col);
    }
    if (gold) {
        rect(lx, y, rw, CH, CLR_YELLOW);
        rect(lx - 1, y - 1, rw + 2, CH + 2, CLR_YELLOW);
    }
    if (selring) rect(lx - 3, y - 3, rw + 6, CH + 6, CLR_GREEN);
}

static void draw_hand(Card *h, bool *holds, int hx, int hy, float *flip, bool mine) {
    for (int i = 0; i < 5; i++) {
        int x = hx + i * (CW + GAP);
        bool gold = mine && holds && holds[i];
        bool ring = mine && state == ST_HOLD && sel == i;
        draw_card(h[i], x, hy, flip[i], gold, ring);
        if (gold) print("HOLD", x + CW / 2 - 14, hy + CH + 1, CLR_YELLOW);
    }
}

// the reveal meter — five hearts that light as you win
static void draw_meter(void) {
    int cx0 = SCREEN_W - 86;
    print("REVEAL", cx0 - 2, 4, CLR_PINK);
    for (int i = 0; i < 5; i++) {
        int hx = cx0 + i * 17, hy = 16;
        int lit = i < reveal;
        int col = lit ? ((i == reveal - 1 && pose_t > 0.2f && (frame() & 2)) ? CLR_WHITE : CLR_RED)
                      : CLR_DARKER_PURPLE;
        circfill(hx - 3, hy, 3, col);
        circfill(hx + 3, hy, 3, col);
        trifill(hx - 6, hy, hx + 6, hy, hx, hy + 7, col);
        if (!lit) { circ(hx - 3, hy, 3, CLR_DARK_PURPLE); circ(hx + 3, hy, 3, CLR_DARK_PURPLE); }
    }
    print(str("- %s -", TIER_NAME[tier]), cx0 - 2, 28, (frame() & 4) ? CLR_PEACH : CLR_PINK);
}

// Sable, all silhouette — recolored body shapes, boa, blinking eyes, a wink
static void draw_sable(void) {
    int cx = 44;
    int baseY = 150;
    float breathe = sin_deg(now() * 40) * 2.0f;
    float lean = (tier >= 1 ? 4 : 0) + (tier >= 3 ? 4 : 0);   // pose escalates the lean
    float boaWave = sin_deg(now() * 90) * (3 + wink_t * 4);
    int body = CLR_DARKER_PURPLE;        // silhouette fill
    int edge = CLR_DARK_PURPLE;
    float ph = 1.0f + 0.10f * pose_t;

    // spotlight cone
    fillp(FILL_CHECKER, -1);
    trifill(cx, 8, cx - 30, baseY, cx + 34, baseY, CLR_LIGHT_YELLOW);
    fillp_reset();

    // shadow
    ovalfill(cx + 2, baseY + 2, 22, 4, CLR_BROWNISH_BLACK);

    // skirt / lower body
    int by = (int)(baseY - 4 + breathe);
    trifill(cx - 16 + (int)lean, by, cx + 18 + (int)lean, by, cx + (int)lean / 2, by - 56, body);
    ovalfill(cx + (int)lean / 2, by, 17, 6, body);
    // torso
    int tx = cx + (int)lean / 2;
    int ty = (int)(by - 52 + breathe);
    ovalfill(tx, ty, (int)(11 * ph), 16, body);
    // hip accent
    ovalfill(tx, ty + 14, (int)(13 * ph), 9, edge);

    // boa across the shoulders — fluttering ovals
    for (int i = -3; i <= 3; i++) {
        int bxp = tx + i * 6;
        int byp = ty - 14 + (int)(boaWave * sin_deg(i * 40 + now() * 120));
        int bc = ((frame() / 5 + i) % 3 == 0) ? CLR_PINK : CLR_DARK_PURPLE;
        ovalfill(bxp, byp, 5, 3, bc);
    }

    // arm — on hip when leaning, raised for a kiss at top tier
    if (tier >= 2) {
        // raised arm / blown kiss
        int ax = tx + 14, ay = ty - 8;
        line(tx + 6, ty - 4, ax, ay, body);
        circfill(ax, ay, 3, body);
    } else {
        int ax = tx + 12, ay = ty + 6;
        line(tx + 6, ty, ax, ay, body);
        ovalfill(ax, ay, 3, 4, body);
    }

    // head
    int hx = tx + 2, hy = ty - 24;
    circfill(hx, hy, 8, body);
    // hair sweep
    arcfill(hx, hy, 10, 180, 360, edge);
    ovalfill(hx - 6, hy + 2, 4, 7, edge);

    // eyes — blink, and a wink (one eye) on a win
    bool blinking = blink(50) && (frame() % 90 < 4);
    if (!blinking) {
        circfill(hx - 3, hy - 1, 1, CLR_LIGHT_PEACH);
        if (wink_t > 0.2f) line(hx + 2, hy - 1, hx + 4, hy - 1, CLR_LIGHT_PEACH);  // winking eye
        else circfill(hx + 3, hy - 1, 1, CLR_LIGHT_PEACH);
    } else {
        line(hx - 4, hy - 1, hx - 2, hy - 1, CLR_LIGHT_PEACH);
        line(hx + 2, hy - 1, hx + 4, hy - 1, CLR_LIGHT_PEACH);
    }
    // lips
    int lipc = (wink_t > 0.2f || tier >= 2) ? CLR_RED : CLR_DARK_RED;
    ovalfill(hx, hy + 4, 2, 1, lipc);

    // blown kiss heart on a wink — floats up from her lips
    if (wink_t > 0.3f) {
        int khx = hx + 16, khy = hy - (int)((1 - wink_t) * 18);
        circfill(khx - 2, khy, 2, CLR_PINK);
        circfill(khx + 2, khy, 2, CLR_PINK);
        trifill(khx - 4, khy, khx + 4, khy, khx, khy + 5, CLR_PINK);
    }

    // chip stack label
    print(str("SABLE  $%d", oppcred), cx - 20, baseY + 6, CLR_PEACH);
}

static void draw_backdrop(void) {
    cls(CLR_DARKER_PURPLE);
    // velvet drapes
    fillp(FILL_VLINES, CLR_DARKER_PURPLE);
    rectfill(0, 0, SCREEN_W, 14, CLR_DARK_RED);
    rectfill(SCREEN_W - 18, 0, 18, SCREEN_H, CLR_DARK_RED);
    fillp_reset();
    for (int x = 6; x < SCREEN_W; x += 16) circfill(x, 14, 6, CLR_DARK_RED);

    // green felt table — an oval the cards sit on
    ovalfill(SCREEN_W / 2, 105, 150, 78, CLR_DARK_GREEN);
    oval(SCREEN_W / 2, 105, 150, 78, CLR_DARK_RED);
    oval(SCREEN_W / 2, 105, 146, 74, CLR_GREEN);

    // marquee
    int f = frame();
    int neon = (f / 6) % 3 == 0 ? CLR_PINK : (f / 6) % 3 == 1 ? CLR_PEACH : CLR_RED;
    print_centered("S T R I P   P O K E R", SCREEN_W/2, 4, neon);
}

static void draw_pot(void) {
    int px = SCREEN_W / 2, py = 100;
    int n = pot / 2; if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        int cc = (i & 1) ? CLR_RED : CLR_WHITE;
        ovalfill(px, py - i * 2, 9, 3, cc);
        oval(px, py - i * 2, 9, 3, CLR_BLACK);
    }
    if (pot > 0) print_centered(str("POT $%d", pot), SCREEN_W/2, 110, CLR_YELLOW);
}

// a button that centers its label within its own rect
static void btn_rect(int x, int y, int w, const char *label, int col, bool hot) {
    rectfill(x, y, w, 18, hot ? CLR_DARK_GREEN : col);
    rect(x, y, w, 18, CLR_WHITE);
    int tw = text_width(label);
    print(label, x + (w - tw) / 2, y + 5, CLR_WHITE);
}

void draw(void) {
    if (!ready) reset_game();
    draw_backdrop();
    draw_sable();
    draw_meter();

    if (state == ST_TITLE) {
        // a coy face-down hand teasing on the felt
        for (int i = 0; i < 5; i++)
            draw_card((Card){0,0}, MY_X + i * (CW + GAP), MY_Y, 0, false, false);
        for (int i = 0; i < 5; i++)
            draw_card((Card){0,0}, OPP_X + i * (CW + GAP), OPP_Y, 0, false, false);
        fillp(FILL_CHECKER, -1);
        rectfill(MY_X - 6, 70, HAND_W + 12, 34, CLR_BLACK);
        fillp_reset();
        print_centered("STRIP POKER", SCREEN_W/2, 72, CLR_PINK);
        print_centered("five-card draw with Sable", SCREEN_W/2, 84, CLR_LIGHT_PEACH);
        if (best_tier > 0)
            print_centered(str("best tease reached: %s", TIER_NAME[best_tier]), SCREEN_W/2, 94, CLR_LIGHT_YELLOW);
        btn_rect(MY_X + HAND_W / 2 - 40, MY_Y + CH + 6, 80, "DEAL", CLR_DARK_RED, true);
        return;
    }

    if (state == ST_OVER) {
        draw_hand(opp, NULL, OPP_X, OPP_Y, flip_opp, false);
        draw_hand(you, held, MY_X, MY_Y, flip_you, true);
        fillp(FILL_CHECKER, -1);
        rectfill(60, 70, SCREEN_W - 120, 60, CLR_BLACK);
        fillp_reset();
        rect(60, 70, SCREEN_W - 120, 60, CLR_DARK_RED);
        bool won = credits > oppcred;
        print_centered(won ? "the night is YOURS" : "tapped out, lover", SCREEN_W/2, 80, won ? CLR_PINK : CLR_PEACH);
        print_centered(str("you $%d   Sable $%d", credits, oppcred), SCREEN_W/2, 96, CLR_WHITE);
        print_centered(str("reached: %s", TIER_NAME[tier]), SCREEN_W/2, 108, CLR_LIGHT_YELLOW);
        if (blink(20)) print_centered("click to play again", SCREEN_W/2, 120, CLR_LIGHT_GREY);
        return;
    }

    // table hands
    draw_hand(opp, NULL, OPP_X, OPP_Y, flip_opp, false);
    draw_pot();
    draw_hand(you, held, MY_X, MY_Y, flip_you, true);

    // your chips
    print(str("YOU  $%d", credits), MY_X, MY_Y - 12, CLR_WHITE);

    // banter line above the felt
    if (banter_t > 0.05f)
        print_centered(banter, SCREEN_W/2, 116, banter_col);

    // action buttons
    int by = MY_Y + CH + 6;
    if (state == ST_HOLD) {
        btn_rect(MY_X + HAND_W / 2 - 40, by, 80, "DRAW", CLR_DARK_RED, true);
    } else if (state == ST_BET) {
        btn_rect(MY_X,       by, 56, "BET",  CLR_DARK_RED,  false);
        btn_rect(MY_X + 64,  by, 56, "CALL", CLR_TRUE_BLUE, false);
        btn_rect(MY_X + 128, by, 56, "FOLD", CLR_DARKER_GREY, false);
    } else if (state == ST_SHOWDOWN) {
        // result banner
        const char *res = outcome > 0 ? "YOU WIN" : outcome < 0 ? "SABLE WINS" : "PUSH";
        int rc = outcome > 0 ? CLR_PINK : outcome < 0 ? CLR_PEACH : CLR_LIGHT_GREY;
        if (show_opp) {
            print_centered(str("%s  vs  %s", HNAME[you_cat], HNAME[opp_cat]), SCREEN_W/2, 6, CLR_LIGHT_GREY);
        }
        print_centered(res, SCREEN_W/2, MY_Y - 24, rc);
        btn_rect(MY_X + HAND_W / 2 - 40, by, 80, "NEXT", CLR_DARK_RED, true);
    }

    // mouse-hover ring on your cards during HOLD
    if (state == ST_HOLD) {
        for (int i = 0; i < 5; i++) {
            int x = MY_X + i * (CW + GAP);
            if (point_in_box(mouse_x(), mouse_y(), x, MY_Y, CW, CH))
                rect(x - 1, MY_Y - 1, CW + 2, CH + 2, CLR_PEACH);
        }
    }
}
