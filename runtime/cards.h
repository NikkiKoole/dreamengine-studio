#ifndef CARDS_H
#define CARDS_H

// cards.h — shared playing-card rendering for the card-game carts
// (blackjack / poker / solitaire / strippoker). Include AFTER studio.h, like ui.h.
//
// card_suit() draws one suit pip centred at (cx,cy) in colour `col`. The geometry
// scales from the canonical LARGE pip by factor `s`: s = 1.0 is the blackjack/poker
// size (reproduced pixel-exact at s=1), smaller s for tighter cards. A cart wraps it
// with its own size:
//   static void draw_suit(int suit,int cx,int cy,int col){ card_suit(suit,cx,cy,col,1.0f); }
//
// suit: 0 = heart, 1 = diamond, 2 = club, 3 = spade. Colour is the cart's call
// (red for suit < 2, black otherwise — the usual convention).

static int card__r(float v) { return (int)(v < 0 ? v - 0.5f : v + 0.5f); }  // round-to-nearest

static void card_suit(int suit, int cx, int cy, int col, float s) {
    #define R(v) card__r((v) * s)
    if (suit == 0) {                 // heart — two lobes + a point
        circfill(cx - R(3), cy - R(2), R(4), col);
        circfill(cx + R(3), cy - R(2), R(4), col);
        trifill(cx - R(7), cy, cx + R(7), cy, cx, cy + R(9), col);
    } else if (suit == 1) {          // diamond — two mirrored triangles
        trifill(cx, cy - R(8), cx - R(6), cy, cx + R(6), cy, col);
        trifill(cx, cy + R(8), cx - R(6), cy, cx + R(6), cy, col);
    } else if (suit == 2) {          // club — three lobes + stem
        circfill(cx, cy - R(4), R(4), col);
        circfill(cx - R(4), cy + R(1), R(4), col);
        circfill(cx + R(4), cy + R(1), R(4), col);
        rectfill(cx - R(1), cy, R(3), R(8), col);
        trifill(cx - R(4), cy + R(9), cx + R(5), cy + R(9), cx + R(1), cy + R(4), col);
    } else {                         // spade — point + two lobes + stem
        trifill(cx, cy - R(8), cx - R(7), cy + R(3), cx + R(7), cy + R(3), col);
        circfill(cx - R(3), cy + R(3), R(4), col);
        circfill(cx + R(3), cy + R(3), R(4), col);
        rectfill(cx - R(1), cy + R(3), R(3), R(7), col);
        trifill(cx - R(4), cy + R(11), cx + R(5), cy + R(11), cx + R(1), cy + R(6), col);
    }
    #undef R
}

#endif // CARDS_H
