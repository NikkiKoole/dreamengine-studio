/* de:meta
{
  "title": "sightlines",
  "status": "active",
  "kind": [
    "probe",
    "tech-demo"
  ],
  "teaches": [
    "raycasting",
    "audio-occlusion"
  ],
  "lineage": "Probe cart distilled from rogue.c's per-tile FOV ray; novel in placing both Bresenham single-ray LOS and recursive octant shadowcasting side-by-side on the same occluder grid as candidates for a future los.h/fov.h header.",
  "description": "PROBE (probe-carts.md): is a tilemap line-of-sight / FOV primitive worth owning in the engine? Two candidates on the same occluder grid. SINGLE-RAY LOS — one Bresenham walk answers 'can A see B?' (enemy vision / stealth / aim / audio occlusion); each guard draws a ray to the eye, red when it has line of sight. FIELD OF VIEW — recursive shadowcasting reveals everything the eye can see, walls casting shadows (lighting / fog-of-war). ARROWS move the eye, L/R-drag paint & erase walls, TAB toggles LOS<->FOV, wheel sets FOV radius, C clears, R randomizes. Generalizes the acoustics-probe raycast beyond audio."
}
de:meta */
#include "studio.h"

// SIGHTLINES
// A testbed for two tilemap visibility primitives — the candidates for a future
// los.h / fov.h cart-land header:
//   1. SINGLE-RAY LOS — "can A see B?" One Bresenham walk, stops at the first
//      wall. The general one: enemy vision/stealth, aiming, AND audio occlusion.
//   2. FIELD OF VIEW   — "what can the eye see from here?" Recursive
//      shadowcasting (Björn Bergström, 8 octants). The set version: lighting,
//      fog-of-war, torchlight.
// Both read the SAME occluder source — is_wall(x,y) — so a real header would
// take that as a callback.
//
// Arrows/WASD move the eye.  Left-drag paints walls, right-drag erases.
// TAB / Z toggles LOS <-> FOV.  Wheel changes FOV radius.  C clears, R randomizes.

#define TILE 8
#define GW   40            // 40*8 = 320
#define GH   22            // 22*8 = 176  (HUD below)
#define HUD_Y (GH * TILE)

enum { MODE_LOS, MODE_FOV };

static unsigned char wall[GH][GW];   // 1 = blocks sight
static unsigned char vis[GH][GW];    // FOV result: 1 = lit this frame
static int  ex, ey;                  // the eye / light source (cell coords)
static int  mode;
static int  radius = 12;
static int  paint_cd;                // debounce so wheel/keys don't autorepeat too fast

// up to 8 guards for the LOS demo — each shows whether it can see the eye
#define NG 6
static int gx[NG], gy[NG];

// ---------------------------------------------------------------- occluder
static bool is_wall(int x, int y) {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return true;   // edges block sight
    return wall[y][x] != 0;
}

// ---------------------------------------------------------------- single-ray LOS
// True if nothing blocks the line between (x0,y0) and (x1,y1). Endpoints don't
// count as occluders — only the cells strictly between them. (Bresenham, lifted
// and generalized from rogue.c's per-tile FOV ray.)
static bool los_clear(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, x = x0, y = y0;
    while (1) {
        if (x == x1 && y == y1) return true;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
        if (x == x1 && y == y1) return true;   // reached target before any wall
        if (is_wall(x, y)) return false;
    }
}

// Visualize the walk: green dots up to the first wall, red dots after it.
static void draw_ray(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, x = x0, y = y0;
    bool blocked = false;
    while (!(x == x1 && y == y1)) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
        if (!(x == x1 && y == y1))
            pset(x * TILE + TILE / 2, y * TILE + TILE / 2,
                 blocked ? CLR_DARK_PURPLE : CLR_LIME_GREEN);
        if (is_wall(x, y)) blocked = true;
    }
}

// ---------------------------------------------------------------- shadowcasting FOV
// Classic recursive shadowcasting over 8 octants. Each octant is scanned row by
// row; a wall splits the scanned arc into a narrower one (the "shadow") and
// recurses. Reads is_wall(), writes vis[].
static const int mult[4][8] = {
    { 1, 0, 0,-1,-1, 0, 0, 1 },
    { 0, 1,-1, 0, 0,-1, 1, 0 },
    { 0, 1, 1, 0, 0,-1,-1, 0 },
    { 1, 0, 0, 1,-1, 0, 0,-1 },
};

static void cast_light(int cx, int cy, int row, float start, float end,
                       int xx, int xy, int yx, int yy) {
    if (start < end) return;
    float new_start = 0;
    for (int j = row; j <= radius; j++) {
        int dx = -j - 1, dy = -j;
        bool blocked = false;
        while (dx <= 0) {
            dx++;
            int X = cx + dx * xx + dy * xy;
            int Y = cy + dx * yx + dy * yy;
            float l_slope = (dx - 0.5f) / (dy + 0.5f);
            float r_slope = (dx + 0.5f) / (dy - 0.5f);
            if (start < r_slope) continue;
            if (end > l_slope) break;
            if (dx * dx + dy * dy <= radius * radius &&
                X >= 0 && X < GW && Y >= 0 && Y < GH)
                vis[Y][X] = 1;
            if (blocked) {
                if (is_wall(X, Y)) { new_start = r_slope; continue; }
                blocked = false; start = new_start;
            } else if (is_wall(X, Y) && j < radius) {
                blocked = true;
                cast_light(cx, cy, j + 1, start, l_slope, xx, xy, yx, yy);
                new_start = r_slope;
            }
        }
        if (blocked) break;
    }
}

static void compute_fov(int cx, int cy) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) vis[y][x] = 0;
    if (cx >= 0 && cx < GW && cy >= 0 && cy < GH) vis[cy][cx] = 1;
    for (int oct = 0; oct < 8; oct++)
        cast_light(cx, cy, 1, 1.0f, 0.0f,
                   mult[0][oct], mult[1][oct], mult[2][oct], mult[3][oct]);
}

// ---------------------------------------------------------------- setup
static void scatter_guards() {
    for (int i = 0; i < NG; i++) {
        int x, y, tries = 0;
        do { x = rnd_between(1, GW - 1); y = rnd_between(1, GH - 1); tries++; }
        while (is_wall(x, y) && tries < 50);
        gx[i] = x; gy[i] = y;
    }
}

static void randomize_walls() {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            wall[y][x] = (rnd(100) < 18) ? 1 : 0;
    wall[ey][ex] = 0;
    scatter_guards();
}

void init() {
    ex = GW / 2; ey = GH / 2;
    mode = MODE_LOS;
    // a couple of rooms so there's something to be occluded by
    for (int x = 6; x < 18; x++) { wall[5][x] = 1; wall[14][x] = 1; }
    for (int y = 5; y <= 14; y++) { wall[y][6] = 1; wall[y][17] = 1; }
    wall[9][17] = 0; wall[10][17] = 0;                 // doorway
    for (int y = 3; y < 19; y++) wall[y][28] = 1;      // a long wall
    wall[11][28] = 0;
    scatter_guards();
}

// ---------------------------------------------------------------- update
void update() {
    if (paint_cd > 0) paint_cd--;

    // move the eye (one cell per press, plus slow autorepeat)
    static int mcd;
    int dx = 0, dy = 0;
    if (btn(0, BTN_LEFT))  dx = -1; else if (btn(0, BTN_RIGHT)) dx = 1;
    if (btn(0, BTN_UP))    dy = -1; else if (btn(0, BTN_DOWN))  dy = 1;
    if (dx || dy) {
        if (mcd <= 0) {
            int nx = clamp(ex + dx, 0, GW - 1), ny = clamp(ey + dy, 0, GH - 1);
            ex = nx; ey = ny; mcd = 6;
        }
    } else mcd = 0;
    if (mcd > 0) mcd--;

    if (btnp(0, BTN_A) || keyp(KEY_TAB)) mode = (mode == MODE_LOS) ? MODE_FOV : MODE_LOS;
    if (keyp('C')) for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) wall[y][x] = 0;
    if (keyp('R')) randomize_walls();

    float w = mouse_wheel();
    if (w > 0) radius = min(radius + 1, 30);
    if (w < 0) radius = max(radius - 1, 2);

    // paint / erase walls with the mouse
    int mxc = mouse_x() / TILE, myc = mouse_y() / TILE;
    if (mouse_y() < HUD_Y && mxc >= 0 && mxc < GW && myc >= 0 && myc < GH) {
        if (mouse_down(0)) wall[myc][mxc] = 1;
        if (mouse_down(1)) wall[myc][mxc] = 0;
    }
    wall[ey][ex] = 0;   // never trap the eye inside a wall

    if (mode == MODE_FOV) compute_fov(ex, ey);

#ifdef DE_TRACE
    int seen = 0;
    if (mode == MODE_LOS) { for (int i = 0; i < NG; i++) if (los_clear(ex, ey, gx[i], gy[i])) seen++; }
    else { for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) seen += vis[y][x]; }
    watch("mode", "%d", mode); watch("radius", "%d", radius); watch("seen", "%d", seen);
#endif
}

// ---------------------------------------------------------------- draw
static void glyph(char c, int cx, int cy, int col) { print(str("%c", c), cx * TILE + 1, cy * TILE, col); }

void draw() {
    cls(CLR_BLACK);

    // walls + (in FOV mode) the lit floor
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = x * TILE, sy = y * TILE;
            if (mode == MODE_FOV) {
                bool lit = vis[y][x];
                if (wall[y][x]) rectfill(sx, sy, TILE, TILE, lit ? CLR_LIGHT_GREY : CLR_DARKER_GREY);
                else if (lit)   pset(sx + TILE / 2, sy + TILE / 2, CLR_DARK_GREY);
            } else {
                if (wall[y][x]) rectfill(sx, sy, TILE, TILE, CLR_LIGHT_GREY);
            }
        }

    if (mode == MODE_LOS) {
        // a faint dot grid so empty floor reads as space
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
            if (!wall[y][x]) pset(x * TILE + TILE / 2, y * TILE + TILE / 2, CLR_DARKER_GREY);
        // a ray from each guard to the eye, then the guards themselves
        int seen = 0;
        for (int i = 0; i < NG; i++) draw_ray(ex, ey, gx[i], gy[i]);
        for (int i = 0; i < NG; i++) {
            bool can_see = los_clear(ex, ey, gx[i], gy[i]);
            if (can_see) seen++;
            glyph('G', gx[i], gy[i], can_see ? CLR_RED : CLR_GREEN);
            if (can_see) glyph('!', gx[i], gy[i] - 1, blink(12) ? CLR_RED : CLR_ORANGE);
        }
        (void)seen;
    } else {
        // guards only show when the eye can see them — a stealth reveal
        for (int i = 0; i < NG; i++)
            if (vis[gy[i]][gx[i]]) glyph('G', gx[i], gy[i], CLR_RED);
    }

    // the eye
    glyph('@', ex, ey, CLR_YELLOW);

    // HUD
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H - HUD_Y, CLR_DARKER_BLUE);
    print(mode == MODE_LOS ? "SINGLE-RAY LOS" : "FIELD OF VIEW", 4, HUD_Y + 2, CLR_WHITE);
    // re-draw the per-mode status line on top of the HUD bar
    if (mode == MODE_LOS) {
        int seen = 0; for (int i = 0; i < NG; i++) if (los_clear(ex, ey, gx[i], gy[i])) seen++;
        print_right(str("%d/%d guards see eye", seen, NG), SCREEN_W - 4, HUD_Y + 2, CLR_LIGHT_PEACH);
    } else {
        print_right(str("radius %d (wheel)", radius), SCREEN_W - 4, HUD_Y + 2, CLR_LIGHT_PEACH);
    }
    font(FONT_SMALL);
    print("arrows: move eye   L/R-drag: paint/erase walls   TAB: mode   wheel: radius   C: clear   R: random",
          4, HUD_Y + 13, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
