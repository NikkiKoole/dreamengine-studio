// Chapter-15 illustration: your game, running in a browser window on someone else's
// machine. A little scene plays inside the page — because you shared it.
#include "studio.h"

void draw(void) {
    cls(CLR_INDIGO);
    int t = frame();
    int wx = 26, wy = 26, ww = SCREEN_W - 52, wh = SCREEN_H - 60;
    // the browser window
    rectfill(wx, wy, ww, wh, CLR_LIGHT_GREY);
    rectfill(wx, wy, ww, 16, CLR_DARK_GREY);                       // title bar
    circfill(wx + 9, wy + 8, 3, CLR_RED);
    circfill(wx + 20, wy + 8, 3, CLR_YELLOW);
    circfill(wx + 31, wy + 8, 3, CLR_GREEN);
    rectfill(wx + 44, wy + 3, ww - 54, 10, CLR_WHITE);             // address bar
    font(FONT_SMALL);
    print("dreamengine.app/your-game", wx + 48, wy + 5, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the game itself, live inside the page
    int gx = wx + 6, gy = wy + 22, gw = ww - 12, gh = wh - 28;
    rectfill(gx, gy, gw, gh, CLR_DARK_BLUE);
    rectfill(gx, gy + gh - 18, gw, 18, CLR_DARK_GREEN);
    int cbob = gy + 24 + (int)(sin_deg(t * 4) * 6);                // a bobbing coin
    circfill(gx + gw - 30, cbob, 5, CLR_YELLOW);
    int hx = gx + 12 + (t * 2) % (gw - 30);                        // a walking hero
    int hy = gy + gh - 30;
    ovalfill(hx, hy, 9, 12, CLR_GREEN);
    ovalfill(hx, hy + 3, 5, 6, CLR_YELLOW);
    circfill(hx - 3, hy - 4, 2, CLR_WHITE); circfill(hx + 3, hy - 4, 2, CLR_WHITE);

    print_centered("your friend is playing it.", SCREEN_W / 2, SCREEN_H - 22, CLR_WHITE);
}
