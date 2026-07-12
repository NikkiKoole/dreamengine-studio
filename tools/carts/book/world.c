// Chapter-11 illustration: a world twice as wide as the window. The hero walks; the
// camera() slides to keep him centred; a minimap up top shows how much you can't see.
#include "studio.h"

#define WORLD_W 640
#define GROUND  150
static float hx;
static int   ready;

void init(void) { hx = 30; ready = 1; }
void update(void) { if (!ready) init(); hx += 3.2f; if (hx > WORLD_W - 30) hx = 30; }

static void hero(int x, int y) {                   // a little green walker
    ovalfill(x, y, 11, 14, CLR_GREEN);
    ovalfill(x, y + 4, 6, 8, CLR_YELLOW);
    circfill(x - 4, y - 5, 3, CLR_WHITE);
    circfill(x + 4, y - 5, 3, CLR_WHITE);
    pset(x - 4, y - 5, CLR_BLACK); pset(x + 4, y - 5, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int camx = (int)hx - SCREEN_W / 2;             // follow the hero…
    if (camx < 0) camx = 0;                        // …but don't scroll past the edges
    if (camx > WORLD_W - SCREEN_W) camx = WORLD_W - SCREEN_W;

    camera(camx, 0);                               // shift the whole world by -camx
    rectfill(0, GROUND, WORLD_W, 60, CLR_DARK_GREEN);
    for (int i = 0; i < 7; i++) {                  // landmarks spread across the world
        int x = 50 + i * 96;
        if (i % 2 == 0) { line(x, GROUND, x, GROUND - 30, CLR_BROWN); circfill(x, GROUND - 42, 16, CLR_GREEN); }
        else { rectfill(x - 16, GROUND - 34, 32, 34, CLR_LIGHT_GREY);
               trifill(x - 20, GROUND - 34, x + 20, GROUND - 34, x, GROUND - 58, CLR_RED); }
    }
    circfill(WORLD_W - 40, 40, 20, CLR_YELLOW);    // a sun near the far end
    hero((int)hx, GROUND - 16);
    camera(0, 0);                                  // reset for the screen-fixed HUD

    int mmx = 8, mmy = 10, mmw = SCREEN_W - 16, mmh = 8;
    rectfill(mmx, mmy, mmw, mmh, CLR_DARK_GREY);                       // the whole world
    rectfill(mmx + camx * mmw / WORLD_W, mmy, SCREEN_W * mmw / WORLD_W, mmh, CLR_YELLOW); // the bit you see
    print("the world is wider than the window", 8, 26, CLR_LIGHT_GREY);
}
