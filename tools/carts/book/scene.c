// Book illustration (ch.3): a whole little scene, and NOTHING in it is a sprite —
// every piece is one plain shape call.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    circfill(268, 44, 22, CLR_YELLOW);              // sun            — circfill
    ovalfill(160, 230, 220, 90, CLR_DARK_GREEN);    // rolling hill   — ovalfill
    // the house
    rectfill(96, 118, 72, 56, CLR_LIGHT_GREY);      // walls          — rectfill
    trifill(90, 118, 174, 118, 132, 86, CLR_RED);   // roof           — trifill
    rectfill(120, 142, 22, 32, CLR_BROWN);          // door           — rectfill
    rect(146, 130, 16, 16, CLR_DARK_BLUE);          // window frame   — rect
    // a little tree
    line(210, 174, 210, 140, CLR_BROWN);            // trunk          — line
    circfill(210, 130, 16, CLR_GREEN);              // leaves         — circfill
    print("home.", 132, 182, CLR_WHITE);            // a label        — print
}
