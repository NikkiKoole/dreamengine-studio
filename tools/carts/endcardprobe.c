// throwaway visual probe for runtime/endcard.h — not a registered cart
#include "studio.h"
#include "endcard.h"

static float t;

void update(void) { t += dt(); }

void draw(void) {
    // a busy fake scene so the dim has something to recede
    cls(CLR_DARK_BLUE);
    rectfill(0, 130, SCREEN_W, 70, CLR_DARK_GREEN);
    for (int i = 0; i < 12; i++) {
        rectfill(10 + i * 26, 90 + (i % 3) * 12, 18, 40, CLR_BROWN);
        circfill(19 + i * 26, 84 + (i % 3) * 12, 12, CLR_GREEN);
    }
    circfill(270, 30, 14, CLR_LIGHT_YELLOW);
    print("score 4200", 8, 8, CLR_WHITE);

    EndCard c = endcard(t, 240, 60, 48, CLR_DARK_PURPLE, CLR_PINK, CLR_RED);
    if (c.settled) {
        print_centered("- YOU WIN -", SCREEN_W / 2, c.y + 12, CLR_LIGHT_YELLOW);
        print_centered("A closing line of flavor text.", SCREEN_W / 2, c.y + 26, CLR_LIGHT_PEACH);
        if (blink(18)) print_centered("- click to play again -", SCREEN_W / 2, c.y + c.h - 14, CLR_LIGHT_GREY);
    }
}
