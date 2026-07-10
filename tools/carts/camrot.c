/* de:meta
{
  "slug": "camrot",
  "title": "camrot",
  "status": "active",
  "created": "2026-06-29",
  "kind": ["probe"],
  "teaches": [],
  "description": "Camera-rotation test card. A little town — checker ground, colored buildings, round trees, and a red NORTH arrow planted in the world — under a camera that continuously rotates (and gently breathes its zoom). The whole world banks around the screen centre while the HUD stays upright. On the GPU it's a Camera2D matrix; on the software canvas (iOS/Switch) it's the offscreen-world-layer rotate-composite — this card is the quick on-device 'is camera rotation working?' check, no input needed."
}
de:meta */
// camrot — a continuously auto-rotating camera over a small town, so camera rotation is
// obvious at a glance (no input). World draws under camera_ex(angle); HUD after camera() reset.
#include "studio.h"
#include <math.h>

typedef struct { int x, y, w, h, col; } Bldg;
static Bldg town[7] = {
    { -90, -50, 26, 40, CLR_RED      }, {  -40, -70, 22, 60, CLR_ORANGE },
    {  10,  -44, 30, 34, CLR_INDIGO  }, {   54, -64, 20, 54, CLR_PINK   },
    {  92,  -40, 24, 30, CLR_YELLOW  }, {  -70,  30, 28, 24, CLR_BLUE   },
    {  60,   34, 26, 28, CLR_GREEN   },
};
static const int tree[6][2] = { {-110,10},{-20,40},{30,46},{110,18},{-50,-90},{80,-86} };

void update(void) {}

void draw(void) {
    float t = frame() / 60.0f;
    float angle = 25.0f + frame() * 0.8f;         // start banked (so it reads as rotation at a glance) + ~0.8°/frame
    float zoom  = 1.3f + 0.2f * sinf(t * 1.2f);   // gentle breathing zoom

    cls(CLR_DARK_BLUE);                            // sky (screen space — fills the banked-out corners)
    // camera_ex(x,y,...) puts world (x,y) at the TOP-LEFT; (-160,-100) centres world origin on screen
    camera_ex(-160, -100, zoom, angle);           // world centred on origin, rotating

    // checker ground tiles (world space) — the rotation reads clearly off the grid
    for (int gx = -160; gx < 160; gx += 32)
        for (int gy = -120; gy < 120; gy += 32)
            rectfill(gx, gy, 30, 30, ((gx / 32 + gy / 32) & 1) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN);

    for (int i = 0; i < 6; i++) {                  // round trees
        circfill(tree[i][0], tree[i][1] + 6, 4, CLR_BROWN);
        circfill(tree[i][0], tree[i][1], 9, CLR_GREEN);
    }
    for (int i = 0; i < 7; i++) {                  // buildings
        Bldg *b = &town[i];
        rectfill(b->x, b->y, b->w, b->h, b->col);
        rect(b->x, b->y, b->w, b->h, CLR_WHITE);
    }
    // a NORTH arrow planted in the world — the clearest rotation tell
    line(0, 0, 0, -80, CLR_RED);
    line(0, -80, -6, -68, CLR_RED);
    line(0, -80,  6, -68, CLR_RED);
    print("N", -3, -98, CLR_RED);

    camera(0, 0);                                  // reset → HUD draws upright
    print("CAMERA ROTATION", 4, 4, CLR_WHITE);
    print(str("software canvas  %d deg", ((int)angle) % 360), 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
