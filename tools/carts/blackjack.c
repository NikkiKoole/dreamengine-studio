/* de:meta
{
  "title": "Blackjack",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "save-load-persistence"
  ],
  "lineage": "Stand-alone card game; no close sibling in the library — the five-phase deal/play/settle state machine and the animated card-fly-in are its notable structural choices.",
  "genre": "tabletop",
  "description": "Sit down at the felt and play the casino classic against a by-the-book dealer: push out a bet from your bankroll, take two cards against the dealer's one-up-one-down, then hit, stand, or double down. Aces ride soft (1 or 11), a two-card 21 pays 3:2, the dealer hits 16 and stands on 17, and the whole table is drawn from crisp card primitives with cards that ease in off the deck, a hole card that flips on the reveal, a rising chord on a win and a red-flashing screen shake on a bust. Your bankroll persists between runs via save(), so the slow grind of the house edge follows you home. Controls: BETTING — Left/Right or A/D to lower/raise the bet, Z or Enter to Deal; YOUR TURN — Z Hit, X Stand, C or Down Double; RESULT — Z or Enter for the next hand (Z rebuys when broke); or click the on-screen buttons."
}
de:meta */
#include "studio.h"
#include "cards.h"

// ── BLACKJACK — you vs. the dealer ─────────────────────────────
// Push out a bet, get two cards, and play the eternal question:
// HIT or STAND? Aces are soft (1 or 11). The dealer plays by the
// book — hits 16, stands on 17. Blackjack pays 3:2. Your bankroll
// is saved between runs, so the slow bleed of the house edge
// carries over.
//
//   BETTING:  ◀ ▶ / A D  bet -/+      Z / Enter  DEAL
//   YOUR TURN: Z HIT   X STAND   C / ▼ DOUBLE
//   RESULT:   Z / Enter  next hand   (Z = rebuy when broke)
//   ...or just click the buttons.

// card: rank 0..12 = 2,3,..,10,J,Q,K,A   suit 0..3 = ♥ ♦ ♣ ♠
typedef struct { int rank, suit; } Card;

static const char *RSTR[13] = { "2","3","4","5","6","7","8","9","10","J","Q","K","A" };

#define CW 44
#define CH 62
#define GAP 8
#define HAND_MAX 11
#define DEAL_T 0.18f   // seconds for a card to fly in

enum { ST_BET, ST_DEAL_ANIM, ST_PLAYER, ST_DEALER, ST_SETTLE };
enum { R_NONE, R_WIN, R_LOSE, R_PUSH, R_BLACKJACK, R_BUST, R_DEALERBUST };

static Card deck[52];
static int  deckpos;
static Card phand[HAND_MAX], dhand[HAND_MAX];
static int  pn, dn;            // cards in each hand
static float pdeal[HAND_MAX], ddeal[HAND_MAX];  // 0..1 deal-in progress per card
static int  state = ST_BET;
static int  bankroll = 100;
static int  bet = 10;
static int  hands_played = 0;
static int  result = R_NONE;
static int  payout = 0;        // net change to bankroll this hand (signed)
static bool doubled = false;
static bool hole_shown = false;
static float dealer_step;      // countdown between dealer draws
static float anim_step;        // pacing for the opening deal
static int   deal_idx;         // how many cards dealt so far in the opening
static float flashT, shk;
static bool  ready = false;

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
static Card draw_card_from_deck(void) {
    if (deckpos >= 52) { shuffle(); }   // reshuffle if the shoe runs dry
    return deck[deckpos++];
}

// value of one card toward a hand (face cards = 10, ace = 11 here)
static int card_val(Card c) {
    if (c.rank >= 8) return 10;   // 10,J,Q,K
    if (c.rank == 12) return 11;  // ace (rank index 12)
    return c.rank + 2;            // 2..9
}

// total a hand, demoting aces from 11 to 1 while busted.
// returns total; sets *soft to true if an ace is still counting as 11.
static int hand_total(Card *h, int n, bool *soft) {
    int total = 0, aces = 0;
    for (int i = 0; i < n; i++) {
        int v = card_val(h[i]);
        total += v;
        if (h[i].rank == 12) aces++;
    }
    while (total > 21 && aces > 0) { total -= 10; aces--; }
    if (soft) *soft = (aces > 0);
    return total;
}

static bool is_blackjack(Card *h, int n) {
    bool soft; return n == 2 && hand_total(h, n, &soft) == 21;
}

static void persist(void) { save(0, bankroll); save(1, hands_played); }

void init(void) {
    bankroll = load(0); if (bankroll <= 0) bankroll = 100;
    hands_played = load(1);
    build_deck(); shuffle();
    if (bet > bankroll) bet = bankroll;
    ready = true;
}

// ── flow ───────────────────────────────────────────────────────
static void start_deal(void) {
    if (bet < 1 || bet > bankroll) return;
    shuffle();
    pn = dn = 0;
    for (int i = 0; i < HAND_MAX; i++) { pdeal[i] = ddeal[i] = 0; }
    doubled = false; hole_shown = false; result = R_NONE; payout = 0;
    deal_idx = 0; anim_step = 0;
    state = ST_DEAL_ANIM;
    note(55, INSTR_SINE, 3);
}

// settle the hand against the bankroll based on `result`
static void settle(void) {
    int stake = doubled ? bet * 2 : bet;
    if (result == R_BLACKJACK)       payout = (stake * 3) / 2;   // 3:2
    else if (result == R_WIN ||
             result == R_DEALERBUST) payout = stake;
    else if (result == R_LOSE ||
             result == R_BUST)       payout = -stake;
    else                             payout = 0;                 // push
    bankroll += payout;
    if (bankroll < 0) bankroll = 0;
    hands_played++;
    persist();
    if (payout > 0)      strum(60, result == R_BLACKJACK ? CHORD_MAJ7 : CHORD_MAJ, INSTR_TRI, 4, 45);
    else if (payout < 0) { note(40, INSTR_TRI, 4); shk = 6; flashT = 5; }
    else                 note(52, INSTR_SINE, 3);
    state = ST_SETTLE;
}

// dealer plays out, then settle
static void resolve(void) {
    bool soft;
    int pt = hand_total(phand, pn, &soft);
    if (pt > 21) { result = R_BUST; settle(); return; }
    // dealer reveals + draws to 17
    hole_shown = true;
    state = ST_DEALER;
    dealer_step = 0.45f;
}

static void player_hit(void) {
    if (state != ST_PLAYER) return;
    phand[pn] = draw_card_from_deck(); pdeal[pn] = 0; pn++;
    note(48, INSTR_SINE, 2);
    bool soft; int pt = hand_total(phand, pn, &soft);
    if (pt > 21) { result = R_BUST; }
}

static void player_stand(void) {
    if (state != ST_PLAYER) return;
    resolve();
}

static void player_double(void) {
    if (state != ST_PLAYER) return;
    if (pn != 2) return;                 // double only on the first two
    if (bankroll < bet * 2) return;      // must be able to cover it
    doubled = true;
    phand[pn] = draw_card_from_deck(); pdeal[pn] = 0; pn++;
    note(50, INSTR_SINE, 3);
    bool soft; int pt = hand_total(phand, pn, &soft);
    if (pt > 21) { result = R_BUST; settle(); }
    else resolve();
}

// after the opening deal animation finishes, check for naturals
static void after_deal(void) {
    bool pj = is_blackjack(phand, pn);
    bool dj = is_blackjack(dhand, dn);
    if (pj || dj) {
        hole_shown = true;
        if (pj && dj) result = R_PUSH;
        else if (pj)  result = R_BLACKJACK;
        else          result = R_LOSE;
        settle();
    } else {
        state = ST_PLAYER;
    }
}

// ── input ──────────────────────────────────────────────────────
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) &&
           mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}
static bool keyp_enter(void) { return keyp(KEY_ENTER); }

// button hit-zones for the bottom bar (shared by mouse + draw)
#define BTN_ROW_Y  178
#define BTN_H  18

void update(void) {
    if (!ready) init();
    if (flashT > 0) flashT -= dt() * 60.0f;
    if (shk > 0.2f) shk *= 0.85f; else shk = 0;

    if (state == ST_BET) {
        // adjust bet
        int step = 10;
        if (btnp(0, BTN_LEFT)  || keyp('a') || clicked(4, BTN_ROW_Y, 50, BTN_H)) {
            bet -= step; if (bet < step) bet = step; note(64, INSTR_SINE, 2);
        }
        if (btnp(0, BTN_RIGHT) || keyp('d') || clicked(58, BTN_ROW_Y, 50, BTN_H)) {
            bet += step; if (bet > bankroll) bet = bankroll; note(67, INSTR_SINE, 2);
        }
        if (bet > bankroll) bet = bankroll;
        if (bet < 1) bet = bankroll >= 1 ? 1 : 0;
        if ((btnp(0, BTN_A) || keyp_enter() || clicked(244, BTN_ROW_Y, 72, BTN_H)) && bet >= 1)
            start_deal();
        return;
    }

    if (state == ST_DEAL_ANIM) {
        // deal four cards in order: player, dealer, player, dealer
        anim_step -= dt();
        // advance every dealt card's fly-in
        for (int i = 0; i < pn; i++) if (pdeal[i] < 1) { pdeal[i] += dt() / DEAL_T; if (pdeal[i] > 1) pdeal[i] = 1; }
        for (int i = 0; i < dn; i++) if (ddeal[i] < 1) { ddeal[i] += dt() / DEAL_T; if (ddeal[i] > 1) ddeal[i] = 1; }
        if (anim_step <= 0 && deal_idx < 4) {
            if (deal_idx % 2 == 0) { phand[pn] = draw_card_from_deck(); pdeal[pn] = 0; pn++; }
            else                   { dhand[dn] = draw_card_from_deck(); ddeal[dn] = 0; dn++; }
            note(50 + deal_idx * 2, INSTR_SINE, 2);
            deal_idx++;
            anim_step = 0.16f;
        }
        // done dealing + last card landed
        if (deal_idx >= 4 && pdeal[1] >= 1 && ddeal[1] >= 1) after_deal();
        return;
    }

    if (state == ST_PLAYER) {
        // animate any incoming hit card
        for (int i = 0; i < pn; i++) if (pdeal[i] < 1) { pdeal[i] += dt() / DEAL_T; if (pdeal[i] > 1) pdeal[i] = 1; }
        if (result == R_BUST) { settle(); return; }
        if (btnp(0, BTN_A) || keyp('z') || clicked(244 - 156, BTN_ROW_Y, 72, BTN_H)) player_hit();
        else if (btnp(0, BTN_B) || keyp('x') || clicked(244 - 78, BTN_ROW_Y, 72, BTN_H)) player_stand();
        else if (btnp(0, BTN_DOWN) || keyp('c') || clicked(244, BTN_ROW_Y, 72, BTN_H)) player_double();
        return;
    }

    if (state == ST_DEALER) {
        for (int i = 0; i < dn; i++) if (ddeal[i] < 1) { ddeal[i] += dt() / DEAL_T; if (ddeal[i] > 1) ddeal[i] = 1; }
        dealer_step -= dt();
        if (dealer_step <= 0) {
            bool soft;
            int dt_total = hand_total(dhand, dn, &soft);
            if (dt_total < 17) {
                dhand[dn] = draw_card_from_deck(); ddeal[dn] = 0; dn++;
                note(46, INSTR_SINE, 2);
                dealer_step = 0.45f;
            } else {
                // dealer done — compare
                int pt = hand_total(phand, pn, &soft);
                if (dt_total > 21)      result = R_DEALERBUST;
                else if (pt > dt_total) result = R_WIN;
                else if (pt < dt_total) result = R_LOSE;
                else                    result = R_PUSH;
                settle();
            }
        }
        return;
    }

    if (state == ST_SETTLE) {
        if (btnp(0, BTN_A) || keyp('z') || keyp_enter() ||
            clicked(244, BTN_ROW_Y, 72, BTN_H)) {
            if (bankroll < 1) { bankroll = 100; persist(); note(72, INSTR_SQUARE, 3); }
            if (bet > bankroll) bet = bankroll;
            if (bet < 1) bet = bankroll >= 10 ? 10 : bankroll;
            state = ST_BET;
        }
        return;
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_suit(int suit, int cx, int cy, int col) { card_suit(suit, cx, cy, col, 1.0f); }

static void draw_card_back(int x, int y) {
    rectfill(x, y, CW, CH, CLR_DARK_RED);
    rect(x, y, CW, CH, CLR_LIGHT_PEACH);
    rect(x + 4, y + 4, CW - 8, CH - 8, CLR_PINK);
    for (int gy = 8; gy < CH - 8; gy += 6)
        line(x + 4, y + gy, x + CW - 5, y + gy, CLR_DARK_PURPLE);
}

static void draw_card_face(Card c, int x, int y) {
    int col = (c.suit < 2) ? CLR_RED : CLR_BLACK;
    rectfill(x, y, CW, CH, CLR_WHITE);
    rect(x, y, CW, CH, CLR_DARK_GREY);
    print(RSTR[c.rank], x + 3, y + 3, col);
    print_right(RSTR[c.rank], x + CW - 3, y + CH - 11, col);
    draw_suit(c.suit, x + CW / 2, y + CH / 2, col);
}

// draw a hand row; `hidden_idx` (>=0) is shown as a face-down back
static void draw_hand(Card *h, int n, float *deal, int baseY, bool dealer, int hidden_idx) {
    int total_w = n * CW + (n - 1) * GAP;
    int startX = SCREEN_W / 2 - total_w / 2;
    for (int i = 0; i < n; i++) {
        int tx = startX + i * (CW + GAP);
        // fly-in: start above (dealer) or below (player) and ease into place
        float t = deal[i];
        float e = ease_out(t);
        int fromY = dealer ? baseY - 60 : baseY + 60;
        int y = (int)lerp((float)fromY, (float)baseY, e);
        int x = (int)lerp((float)(SCREEN_W / 2 - CW / 2), (float)tx, e);
        if (dealer && i == hidden_idx) draw_card_back(x, y);
        else                           draw_card_face(h[i], x, y);
    }
}

static void button_c(int x, int w, const char *label, bool on) {
    rectfill(x, BTN_ROW_Y, w, BTN_H, on ? CLR_DARK_GREEN : CLR_DARK_GREY);
    rect(x, BTN_ROW_Y, w, BTN_H, CLR_WHITE);
    int tw = text_width(label);
    print(label, x + (w - tw) / 2, BTN_ROW_Y + 5, CLR_WHITE);
}

void draw(void) {
    if (!ready) init();

    // shake on a loss
    if (shk > 0.4f)
        camera((int)rnd_float_between(-shk, shk), (int)rnd_float_between(-shk, shk));

    cls(CLR_DARK_GREEN);
    // felt arcs
    oval(SCREEN_W / 2, 38, 150, 26, CLR_DARK_BROWN);
    rectfill(0, 0, SCREEN_W, 13, CLR_DARKER_PURPLE);

    bool psoft, dsoft;

    // dealer hand (top)
    if (dn > 0) {
        int hidden = hole_shown ? -1 : 1;
        draw_hand(dhand, dn, ddeal, 24, true, hidden);
        if (hole_shown) {
            int dt_total = hand_total(dhand, dn, &dsoft);
            print_centered(str("DEALER  %d%s", dt_total, dsoft ? " (soft)" : ""), SCREEN_W/2, 90, dt_total > 21 ? CLR_RED : CLR_LIGHT_PEACH);
        } else {
            print_centered("DEALER", SCREEN_W/2, 90, CLR_LIGHT_GREY);
        }
    }

    // player hand (bottom)
    if (pn > 0) {
        draw_hand(phand, pn, pdeal, 100, false, -1);
        int pt = hand_total(phand, pn, &psoft);
        int c = pt > 21 ? CLR_RED : (pt == 21 ? CLR_YELLOW : CLR_WHITE);
        print_centered(str("YOU  %d%s", pt, (psoft && pt != 21) ? " (soft)" : ""), SCREEN_W/2, 166, c);
    }

    camera(0, 0);

    // flash overlay on a loss
    if (flashT > 0) { fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_RED); fillp_reset(); }

    // ── HUD top bar ──
    print(str("BANK $%d", bankroll), 4, 3, CLR_LIGHT_YELLOW);
    print_centered(str("BET $%d%s", bet, doubled ? " x2" : ""), SCREEN_W/2, 3, CLR_YELLOW);
    print_right(str("HANDS %d", hands_played), SCREEN_W - 4, 3, CLR_LIGHT_GREY);

    // ── status / result banner ──
    const char *msg = "";
    int mc = CLR_LIGHT_PEACH;
    if (state == ST_BET) { msg = bankroll < 1 ? "broke! press DEAL to rebuy" : "place your bet, then DEAL"; }
    else if (state == ST_DEAL_ANIM) msg = "dealing...";
    else if (state == ST_PLAYER) { msg = "HIT, STAND, or DOUBLE"; }
    else if (state == ST_DEALER) msg = "dealer plays...";
    else if (state == ST_SETTLE) {
        switch (result) {
            case R_BLACKJACK:  msg = str("BLACKJACK!  +$%d", payout); mc = CLR_YELLOW; break;
            case R_WIN:        msg = str("YOU WIN  +$%d", payout); mc = CLR_GREEN; break;
            case R_DEALERBUST: msg = str("DEALER BUSTS  +$%d", payout); mc = CLR_GREEN; break;
            case R_PUSH:       msg = "PUSH — bet returned"; mc = CLR_LIGHT_GREY; break;
            case R_LOSE:       msg = str("DEALER WINS  -$%d", -payout); mc = CLR_RED; break;
            case R_BUST:       msg = str("BUST!  -$%d", -payout); mc = CLR_RED; break;
            default:           msg = ""; break;
        }
    }
    print_centered(msg, SCREEN_W/2, 138, mc);

    // ── bottom button bar (state-dependent) ──
    if (state == ST_BET) {
        button_c(4, 50, "BET -", false);
        button_c(58, 50, "BET +", false);
        button_c(244, 72, bankroll < 1 ? "REBUY" : "DEAL", true);
    } else if (state == ST_PLAYER) {
        button_c(244 - 156, 72, "HIT", true);
        button_c(244 - 78, 72, "STAND", true);
        bool can_dbl = (pn == 2 && bankroll >= bet * 2);
        button_c(244, 72, "DOUBLE", can_dbl);
    } else if (state == ST_SETTLE) {
        button_c(244, 72, bankroll < 1 ? "REBUY" : "NEXT", true);
    }
}
