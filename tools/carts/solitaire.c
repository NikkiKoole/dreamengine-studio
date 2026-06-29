/* de:meta
{
  "title": "Klondike Solitaire",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "save-load-persistence"
  ],
  "lineage": "Classic Klondike from scratch — all cards drawn from primitives (no sprite sheet), drag-and-drop run validation, alternating-color tableau rules; no novel algorithmic concept beyond a clean rules implementation.",
  "genre": "tabletop",
  "description": "The patience classic on green felt. Seven tableau piles cascade in alternating-color descending runs, a face-down stock deals one card at a time to the waste, and four suit foundations build home from Ace to King. Grab a single card or a whole valid run and it lifts under your cursor with a soft drop shadow; drop it on a legal pile or a foundation and it snaps in with a chime, while the newly exposed tableau card flips itself. Clear all four suits to win — the felt darkens, a banner drops, and your running tally of games won is saved. Cards are hand-drawn from primitives (no sprite sheet) with pip suits and crisp white faces. Controls: click the stock to deal one card to the waste (click the empty stock to recycle the waste); drag a card or run between tableau piles or onto a foundation, invalid drops snap back; press R or click the win banner to deal a fresh shuffle."
}
de:meta */
#include "studio.h"
#include "cards.h"
#include <stddef.h>

// ── Klondike Solitaire ────────────────────────────────────────
// The patience classic, all mouse. Click the STOCK to deal one
// card to the WASTE (draw-1). Drag cards or valid runs between
// the 7 tableau piles; alternating colors, descending. Send each
// suit home to its FOUNDATION, Ace up to King. Clear all four to
// win. Newly-exposed tableau cards flip themselves.
//
//   • click the stock     → deal one to waste (click empty stock to recycle)
//   • drag a card / run    → tableau↔tableau or onto a foundation
//   • R                    → new shuffle
//
// Cards are drawn from primitives (no sprite sheet) — the poker.c
// pip idiom.

// card: rank 0..12 = A,2,..,10,J,Q,K   suit 0..3 = ♥ ♦ ♣ ♠ (0,1 red / 2,3 black)
typedef struct { int rank, suit; bool face; } Card;

static const char *RSTR[13] = { "A","2","3","4","5","6","7","8","9","10","J","Q","K" };

#define CW 36
#define CH 48
#define FAN_UP   13   // vertical offset for face-up cards in a tableau pile
#define FAN_DN   5    // tighter offset for face-down cards

// pile sizes (worst case fits well under these)
#define MAXPILE 24

// ── pile storage ──────────────────────────────────────────────
// tableau[7], foundation[4], stock, waste — each a small stack.
static Card tab[7][MAXPILE];   static int tabn[7];
static Card fnd[4][14];        static int fndn[4];   // index by suit
static Card stock[52];         static int stockn;
static Card waste[24];         static int wasten;

// ── drag state ────────────────────────────────────────────────
// where a grabbed run came from, the cards in flight, and the cursor grab offset
enum { SRC_NONE, SRC_TAB, SRC_WASTE };
static int   dragSrc = SRC_NONE;
static int   dragFrom;             // tableau index when SRC_TAB
static Card  dragCards[MAXPILE];
static int   dragn;
static int   grabDX, grabDY;       // offset from card top-left to cursor at grab

static int  won = 0;        // total games won (persisted)
static bool gameWon = false;
static bool ready = false;

// ── layout helpers ────────────────────────────────────────────
#define TOP_Y   6
#define STOCK_X 6
#define WASTE_X (STOCK_X + CW + 6)
#define FND_X0  170          // first foundation x
#define FND_GAP (CW + 4)
#define TAB_Y   62
#define TAB_X0  6
#define TAB_GAP ((SCREEN_W - 2*TAB_X0 - 7*CW) / 6)   // even spread of 7 columns

static int tab_x(int p) { return TAB_X0 + p * (CW + TAB_GAP); }
static int fnd_x(int s) { return FND_X0 + s * FND_GAP; }

static bool red(Card c) { return c.suit < 2; }

// ── deck / deal ───────────────────────────────────────────────
static void deal_new(void) {
    Card deck[52];
    int n = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 0; r < 13; r++) { deck[n].rank = r; deck[n].suit = s; deck[n].face = false; n++; }
    for (int i = 51; i > 0; i--) { int j = rnd(i + 1); Card t = deck[i]; deck[i] = deck[j]; deck[j] = t; }

    for (int p = 0; p < 7; p++) tabn[p] = 0;
    for (int s = 0; s < 4; s++) fndn[s] = 0;
    wasten = 0; stockn = 0;
    dragSrc = SRC_NONE; dragn = 0;
    gameWon = false;

    int d = 0;
    // standard Klondike deal: column p gets p+1 cards, last one face up
    for (int p = 0; p < 7; p++) {
        for (int k = 0; k <= p; k++) {
            Card c = deck[d++];
            c.face = (k == p);
            tab[p][tabn[p]++] = c;
        }
    }
    // remainder → stock (all face down)
    while (d < 52) { Card c = deck[d++]; c.face = false; stock[stockn++] = c; }

    ready = true;
}

static void check_win(void) {
    int total = fndn[0] + fndn[1] + fndn[2] + fndn[3];
    if (total == 52 && !gameWon) {
        gameWon = true;
        won++;
        save(0, won);
        shake(3.0f);
        strum(60, CHORD_MAJ7, INSTR_TRI, 5, 60);
    }
}

void init(void) {
    won = load(0);
    deal_new();
}

// ── move validation ───────────────────────────────────────────
static bool can_stack_tab(Card moving, int dest) {
    if (tabn[dest] == 0) return moving.rank == 12;        // only a King on empty
    Card top = tab[dest][tabn[dest] - 1];
    if (!top.face) return false;
    return (red(moving) != red(top)) && (moving.rank == top.rank - 1);
}
static bool can_stack_fnd(Card moving, int s) {
    if (moving.suit != s) return false;
    return moving.rank == fndn[s];                         // next rank up (A=0 first)
}

// auto-flip the now-top card of a tableau pile after cards leave
static void flip_top(int p) {
    if (tabn[p] > 0 && !tab[p][tabn[p] - 1].face) {
        tab[p][tabn[p] - 1].face = true;
        note(64, INSTR_SINE, 2);
    }
}

// ── pickup ────────────────────────────────────────────────────
// grab a run starting at index `start` of tableau pile p (all face up below it)
static void grab_from_tab(int p, int start, int cardx, int cardy) {
    dragn = 0;
    for (int i = start; i < tabn[p]; i++) dragCards[dragn++] = tab[p][i];
    tabn[p] = start;
    dragSrc = SRC_TAB; dragFrom = p;
    grabDX = mouse_x() - cardx; grabDY = mouse_y() - cardy;
}
static void grab_from_waste(int cardx, int cardy) {
    dragn = 0;
    dragCards[dragn++] = waste[wasten - 1];
    wasten--;
    dragSrc = SRC_WASTE;
    grabDX = mouse_x() - cardx; grabDY = mouse_y() - cardy;
}

// return a grabbed run to where it came from (invalid drop / no drop)
static void return_drag(void) {
    if (dragSrc == SRC_TAB) {
        for (int i = 0; i < dragn; i++) tab[dragFrom][tabn[dragFrom]++] = dragCards[i];
    } else if (dragSrc == SRC_WASTE) {
        waste[wasten++] = dragCards[0];
    }
    dragSrc = SRC_NONE; dragn = 0;
}

// ── update ────────────────────────────────────────────────────
void update(void) {
    if (!ready) { init(); }

    if (keyp('R')) { deal_new(); return; }

    if (gameWon) {
        if (mouse_pressed(MOUSE_LEFT)) deal_new();
        return;
    }

    int mx = mouse_x(), my = mouse_y();

    // ── press: deal from stock, or pick up a card/run ──
    if (mouse_pressed(MOUSE_LEFT) && dragSrc == SRC_NONE) {
        // stock click
        if (point_in_box(mx, my, STOCK_X, TOP_Y, CW, CH)) {
            if (stockn > 0) {
                Card c = stock[--stockn]; c.face = true;
                waste[wasten++] = c;
                note(55, INSTR_SQUARE, 2);
            } else if (wasten > 0) {
                // recycle waste back into stock (reverse, face down)
                while (wasten > 0) { Card c = waste[--wasten]; c.face = false; stock[stockn++] = c; }
                note(48, INSTR_SQUARE, 2);
            }
            return;
        }
        // waste top
        if (wasten > 0 && point_in_box(mx, my, WASTE_X, TOP_Y, CW, CH)) {
            grab_from_waste(WASTE_X, TOP_Y);
            return;
        }
        // tableau: find which card was hit (topmost wins)
        for (int p = 0; p < 7; p++) {
            int x = tab_x(p);
            // walk from bottom card up; compute each card's y
            int y = TAB_Y;
            int hit = -1, hity = 0;
            for (int i = 0; i < tabn[p]; i++) {
                int h = (i == tabn[p] - 1) ? CH : (tab[p][i].face ? FAN_UP : FAN_DN);
                if (tab[p][i].face && my >= y && my < y + h && mx >= x && mx < x + CW) { hit = i; hity = y; }
                y += (i == tabn[p] - 1) ? 0 : h;
            }
            // also allow grabbing the topmost card by its full body
            if (tabn[p] > 0) {
                int topi = tabn[p] - 1;
                int topy = TAB_Y;
                for (int i = 0; i < topi; i++) topy += tab[p][i].face ? FAN_UP : FAN_DN;
                if (tab[p][topi].face && mx >= x && mx < x + CW && my >= topy && my < topy + CH) { hit = topi; hity = topy; }
            }
            if (hit >= 0) { grab_from_tab(p, hit, x, hity); return; }
        }
    }

    // ── release: try to drop ──
    if (mouse_released(MOUSE_LEFT) && dragSrc != SRC_NONE) {
        int dropx = mx - grabDX, dropy = my - grabDY;
        int cx = dropx + CW / 2, cy = dropy + CH / 2;   // use card center for hit-testing
        bool placed = false;

        // foundation (single card only)
        if (dragn == 1) {
            for (int s = 0; s < 4; s++) {
                if (point_in_box(cx, cy, fnd_x(s), TOP_Y, CW, CH) && can_stack_fnd(dragCards[0], s)) {
                    fnd[s][fndn[s]++] = dragCards[0];
                    placed = true;
                    strum(60 + dragCards[0].rank, CHORD_MAJ, INSTR_SINE, 4, 30);
                    if (dragSrc == SRC_TAB) flip_top(dragFrom);
                    break;
                }
            }
        }
        // tableau
        if (!placed) {
            for (int p = 0; p < 7; p++) {
                if (p == dragFrom && dragSrc == SRC_TAB) continue;
                int x = tab_x(p);
                // a generous drop zone: the whole column band
                int band_h = CH + (tabn[p] > 0 ? (tabn[p] - 1) * FAN_UP : 0);
                if (cx >= x && cx < x + CW && cy >= TAB_Y && cy < TAB_Y + band_h + CH &&
                    can_stack_tab(dragCards[0], p)) {
                    for (int i = 0; i < dragn; i++) tab[p][tabn[p]++] = dragCards[i];
                    placed = true;
                    note(60, INSTR_SINE, 2);
                    if (dragSrc == SRC_TAB) flip_top(dragFrom);
                    break;
                }
            }
        }

        if (placed) { dragSrc = SRC_NONE; dragn = 0; check_win(); }
        else        { return_drag(); note(40, INSTR_NOISE, 2); }
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_suit(int suit, int cx, int cy, int col) { card_suit(suit, cx, cy, col, 0.75f); }

static void draw_card(Card c, int x, int y) {
    if (!c.face) {
        rectfill(x, y, CW, CH, CLR_DARK_BLUE);
        rect(x, y, CW, CH, CLR_INDIGO);
        rect(x + 3, y + 3, CW - 6, CH - 6, CLR_TRUE_BLUE);
        return;
    }
    int col = red(c) ? CLR_RED : CLR_BLACK;
    rectfill(x, y, CW, CH, CLR_WHITE);
    rect(x, y, CW, CH, CLR_DARK_GREY);
    print(RSTR[c.rank], x + 3, y + 3, col);
    draw_suit(c.suit, x + CW - 8, y + 11, col);
    draw_suit(c.suit, x + CW / 2, y + CH / 2 + 4, col);
}

// empty slot outline (faint inset rectangle on the felt)
static void draw_slot(int x, int y) {
    rect(x, y, CW, CH, CLR_BLUE_GREEN);
    rect(x + 1, y + 1, CW - 2, CH - 2, CLR_DARK_GREEN);
}
// small label centered inside a card-sized slot
static void slot_label(int x, int y, const char *s) {
    int w = text_width(s);
    print(s, x + CW / 2 - w / 2, y + CH / 2 - 3, CLR_DARK_GREEN);
}

void draw(void) {
    if (!ready) init();
    cls(CLR_DARK_GREEN);

    // stock
    if (stockn > 0) draw_card(stock[stockn - 1], STOCK_X, TOP_Y);
    else { draw_slot(STOCK_X, TOP_Y); slot_label(STOCK_X, TOP_Y, wasten ? "O" : "-"); }

    // waste
    if (wasten > 0) draw_card(waste[wasten - 1], WASTE_X, TOP_Y);
    else            { draw_slot(WASTE_X, TOP_Y); }

    // foundations
    for (int s = 0; s < 4; s++) {
        int x = fnd_x(s);
        if (fndn[s] > 0) draw_card(fnd[s][fndn[s] - 1], x, TOP_Y);
        else { draw_slot(x, TOP_Y); draw_suit(s, x + CW / 2, TOP_Y + CH / 2, CLR_DARK_GREEN); }
    }

    // tableau
    for (int p = 0; p < 7; p++) {
        int x = tab_x(p);
        if (tabn[p] == 0) draw_slot(x, TAB_Y);
        int y = TAB_Y;
        for (int i = 0; i < tabn[p]; i++) {
            draw_card(tab[p][i], x, y);
            y += tab[p][i].face ? FAN_UP : FAN_DN;
        }
    }

    // dragged run (with a soft shadow) follows the cursor
    if (dragSrc != SRC_NONE) {
        int dx = mouse_x() - grabDX, dy = mouse_y() - grabDY;
        for (int i = 0; i < dragn; i++) {
            rectfill(dx + 2, dy + i * FAN_UP + 2, CW, CH, CLR_DARKER_GREY);  // shadow
        }
        for (int i = 0; i < dragn; i++) draw_card(dragCards[i], dx, dy + i * FAN_UP);
    }

    // HUD
    print(str("WON %d", won), 6, SCREEN_H - 10, CLR_DARK_GREEN);
    print_right("R = new game", SCREEN_W - 6, SCREEN_H - 10, CLR_DARK_GREEN);

    // win overlay
    if (gameWon) {
        fade(0.5f);
        rectfill(60, 78, 200, 44, CLR_DARK_BLUE);
        rect(60, 78, 200, 44, CLR_YELLOW);
        print_centered("YOU WIN!", SCREEN_W/2, 88, CLR_YELLOW);
        print_centered(str("games won: %d", won), SCREEN_W/2, 102, CLR_WHITE);
        print_centered("click to deal again", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }
}
