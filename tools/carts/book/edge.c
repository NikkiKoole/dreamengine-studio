// Book illustration (ch.2): the sun has walked half off the right edge — the problem.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    circfill(316, 100, 20, CLR_YELLOW);   // x too big — half of it is gone
    // a dashed line marking the edge it fell off
    for (int y = 0; y < 200; y += 8) pset(319, y, CLR_RED);
    print("the edge", 232, 150, CLR_LIGHT_GREY);
    print("of the world", 218, 162, CLR_LIGHT_GREY);
}
