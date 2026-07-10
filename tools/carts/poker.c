/* de:meta
{
  "slug": "poker",
  "title": "video poker",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine"
  ],
  "lineage": "Standard arcade video poker (Jacks or Better); straightforward card-game implementation with a clean hand-evaluation algorithm and a three-state machine (waiting/holding/showing).",
  "genre": "tabletop",
  "description": "Jacks-or-Better video poker, the arcade classic. You are dealt five cards; click the ones to KEEP (or use Left/Right + A), then DRAW to replace the rest. A pair of Jacks or better pays out, with bigger hands paying more — see the schedule up top. The BET button cycles 1-5; max bet earns a Royal Flush bonus. Mouse-driven, B also deals/draws."
}
de:meta */
#include "studio.h"
#include "cards.h"

// ── video poker — Jacks or Better ─────────────────────────────
// The arcade classic. You're dealt 5 cards; click the ones you
// want to KEEP, then DRAW to replace the rest. Get a pair of
// Jacks or better to win — bigger hands pay more.
//
//   • click a card (or ◀ ▶ + A) to hold / unhold it
//   • DEAL / DRAW button, or B
//   • BET button cycles your bet 1–5 (max bet pays a Royal bonus)
//
// Shows off the mouse API for clicking cards and buttons.

// card: rank 0..12 = 2,3,..,10,J,Q,K,A   suit 0..3 = ♥ ♦ ♣ ♠
typedef struct { int rank, suit; } Card;

static const char *RSTR[13] = { "2","3","4","5","6","7","8","9","10","J","Q","K","A" };

// hand categories (index) and their payout per credit (9/6 Jacks or Better)
enum { C_NONE, C_JACKS, C_TWOPAIR, C_TRIPS, C_STRAIGHT, C_FLUSH,
       C_FULL, C_QUADS, C_STRFLUSH, C_ROYAL };
static const int   PAY[10]   = { 0, 1, 2, 3, 4, 6, 9, 25, 50, 250 };
static const char *NAME[10]  = { "", "JACKS+", "TWO PAIR", "3 OF KIND", "STRAIGHT",
                                 "FLUSH", "FULL HOUSE", "4 OF KIND", "STR FLUSH", "ROYAL" };

#define CW 50
#define CH 70
#define GAP 8
#define CARD_X 19
#define CARD_Y 66

enum { ST_WAITING, ST_HOLDING, ST_SHOWING };

static Card deck[52], hand[5];
static bool held[5];
static int  deckpos;
static int  state = ST_WAITING;
static int  credits = 100;
static int  bet = 1;
static int  sel = 0;
static int  lastcat = -1, lastwin = 0;
static bool ready = false;

// ── deck ──────────────────────────────────────────────────────
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
}

// ── hand evaluation ───────────────────────────────────────────
static int evaluate(void) {
    int rc[13] = {0}, sc[4] = {0};
    for (int i = 0; i < 5; i++) { rc[hand[i].rank]++; sc[hand[i].suit]++; }

    bool flush = false;
    for (int s = 0; s < 4; s++) if (sc[s] == 5) flush = true;

    int distinct = 0, lo = -1, hi = -1;
    for (int r = 0; r < 13; r++) if (rc[r]) { distinct++; if (lo < 0) lo = r; hi = r; }
    bool straight = false, royal = false;
    if (distinct == 5) {
        if (hi - lo == 4) straight = true;
        if (rc[12] && rc[0] && rc[1] && rc[2] && rc[3]) straight = true;   // wheel A-2-3-4-5
        if (rc[12] && rc[11] && rc[10] && rc[9] && rc[8]) royal = true;    // 10-J-Q-K-A
    }

    int four = 0, three = 0, pairs = 0; bool jacks = false;
    for (int r = 0; r < 13; r++) {
        if (rc[r] == 4) four = 1;
        else if (rc[r] == 3) three = 1;
        else if (rc[r] == 2) { pairs++; if (r >= 9) jacks = true; }   // J,Q,K,A
    }

    if (straight && flush && royal) return C_ROYAL;
    if (straight && flush)          return C_STRFLUSH;
    if (four)                       return C_QUADS;
    if (three && pairs == 1)        return C_FULL;
    if (flush)                      return C_FLUSH;
    if (straight)                   return C_STRAIGHT;
    if (three)                      return C_TRIPS;
    if (pairs == 2)                 return C_TWOPAIR;
    if (jacks)                      return C_JACKS;
    return C_NONE;
}

static void reset_game(void) {
    build_deck();
    credits = 100; bet = 1; state = ST_WAITING;
    lastcat = -1; lastwin = 0; sel = 0;
    ready = true;
}

static void do_deal(void) {
    if (credits < bet) return;
    credits -= bet;
    shuffle();
    for (int i = 0; i < 5; i++) { hand[i] = deck[deckpos++]; held[i] = false; }
    state = ST_HOLDING; sel = 0; lastcat = -1; lastwin = 0;
    note(60, INSTR_SQUARE, 2);
}

static void do_draw(void) {
    for (int i = 0; i < 5; i++) if (!held[i]) hand[i] = deck[deckpos++];
    lastcat = evaluate();
    lastwin = PAY[lastcat] * bet;
    if (lastcat == C_ROYAL && bet == 5) lastwin = 4000;   // max-bet royal bonus
    credits += lastwin;
    state = ST_SHOWING;
    if (lastwin > 0) { strum(60, CHORD_MAJ, INSTR_TRI, 4, 40); }
    else             { note(48, INSTR_TRI, 3); }
}

// ── input helpers ─────────────────────────────────────────────
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) &&
           mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}

void update(void) {
    if (!ready) reset_game();

    // card clicks while holding
    if (state == ST_HOLDING) {
        for (int i = 0; i < 5; i++) {
            int x = CARD_X + i * (CW + GAP);
            if (clicked(x, CARD_Y, CW, CH)) { held[i] = !held[i]; sel = i; note(67, INSTR_SINE, 2); }
        }
        if (btnp(0, BTN_LEFT))  sel = (sel + 4) % 5;
        if (btnp(0, BTN_RIGHT)) sel = (sel + 1) % 5;
        if (btnp(0, BTN_A))     { held[sel] = !held[sel]; note(67, INSTR_SINE, 2); }
    }

    // BET button (bottom-left)
    if (state != ST_HOLDING && (clicked(4, 178, 64, 18) || btnp(0, BTN_UP))) {
        bet = bet % 5 + 1; note(72, INSTR_SINE, 2);
    }

    // DEAL / DRAW button (bottom-right) — also B
    bool action = clicked(252, 178, 64, 18) || btnp(0, BTN_B);
    if (action) {
        if (state == ST_HOLDING) do_draw();
        else                     do_deal();
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_suit(int suit, int cx, int cy, int col) { card_suit(suit, cx, cy, col, 1.0f); }

static void draw_card(int i) {
    int x = CARD_X + i * (CW + GAP), y = CARD_Y;
    bool face = (state == ST_HOLDING || state == ST_SHOWING);
    if (!face) {
        rectfill(x, y, CW, CH, CLR_DARK_RED);
        rect(x + 3, y + 3, CW - 6, CH - 6, CLR_LIGHT_PEACH);
        return;
    }
    int col = (hand[i].suit < 2) ? CLR_RED : CLR_BLACK;
    rectfill(x, y, CW, CH, CLR_WHITE);
    rect(x, y, CW, CH, CLR_DARK_GREY);
    print(RSTR[hand[i].rank], x + 4, y + 4, col);
    print_right(RSTR[hand[i].rank], x + CW - 4, y + CH - 12, col);
    draw_suit(hand[i].suit, x + CW / 2, y + CH / 2, col);
    if (held[i]) {
        rect(x, y, CW, CH, CLR_YELLOW);
        rect(x - 1, y - 1, CW + 2, CH + 2, CLR_YELLOW);
        print("HELD", x + CW / 2 - 15, y + CH + 2, CLR_YELLOW);
    }
    if (state == ST_HOLDING && sel == i) rect(x - 3, y - 3, CW + 6, CH + 6, CLR_GREEN);
}

static void payout_table(void) {
    // two columns of the pay schedule; highlight the hand you just made
    int order[9] = { C_ROYAL, C_STRFLUSH, C_QUADS, C_FULL, C_FLUSH,
                     C_STRAIGHT, C_TRIPS, C_TWOPAIR, C_JACKS };
    for (int k = 0; k < 9; k++) {
        int cat = order[k];
        int col = (k < 5) ? 4 : 164;
        int row = (k < 5) ? k : k - 5;
        int y = 3 + row * 10;
        int c = (cat == lastcat) ? CLR_YELLOW : CLR_LIGHT_GREY;
        if (cat == lastcat) rectfill(col - 2, y - 1, 156, 9, CLR_DARK_PURPLE);
        int payout = (cat == C_ROYAL && bet == 5) ? 4000 : PAY[cat] * bet;   // max-bet royal bonus
        print(NAME[cat], col, y, c);
        print_right(str("%d", payout), col + 150, y, c);
    }
}

static void button(int x, int y, int w, int h, const char *label, bool on) {
    rectfill(x, y, w, h, on ? CLR_DARK_GREEN : CLR_DARK_GREY);
    rect(x, y, w, h, CLR_WHITE);
    print(label, x + 6, y + 5, CLR_WHITE);
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_DARK_GREEN);
    rectfill(0, 0, SCREEN_W, 52, CLR_DARKER_PURPLE);

    payout_table();
    for (int i = 0; i < 5; i++) draw_card(i);

    // status line
    const char *msg = state == ST_WAITING ? "press DEAL to play"
                    : state == ST_HOLDING ? "click cards to HOLD, then DRAW"
                    : lastwin > 0 ? str("%s  +%d!", NAME[lastcat], lastwin)
                                  : "no win - deal again";
    print_centered(msg, SCREEN_W/2, 152, lastwin > 0 ? CLR_YELLOW : CLR_LIGHT_PEACH);

    print(str("CREDITS %d", credits), 78, 168, CLR_WHITE);
    print_centered(str("BET %d", bet), SCREEN_W/2, 184, CLR_YELLOW);

    button(4, 178, 64, 18, str("BET %d", bet), false);
    button(252, 178, 64, 18, state == ST_HOLDING ? "  DRAW" : "  DEAL", true);
}
