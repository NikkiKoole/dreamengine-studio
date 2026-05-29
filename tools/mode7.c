#include "studio.h"

// MODE 7 — the SNES floor trick: one flat ground texture, sampled once PER
// SCANLINE with a perspective divide, so it stretches out to a horizon and
// rotates under you. This is the real thing (not faked road segments).
//
// You pilot a craft over a procedurally generated world. Fly through the
// glowing rings before the clock runs out — each one buys you more time.
//
// Left/Right turn   Up/Down throttle   Z climb   X dive

#define TEX   256
#define MASK  255
#define HZ    72                 // horizon row on screen
#define BS    2                  // ground block size (chunky = faster + retro)
#define NRINGS 6

static unsigned char world[TEX * TEX];   // baked terrain: palette index per cell

static float cx, cy, ang, spd, H;        // camera world pos, heading, speed, height
static struct { float x, y; } ring[NRINGS];
static int   score;
static float t_end;
static bool  over;

// build the world once: layered noise -> terrain bands
void init(void) {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            float h = 0.55f * noise2(x * 0.025f, y * 0.025f)
                    + 0.30f * noise2(x * 0.060f, y * 0.060f)
                    + 0.15f * noise2(x * 0.130f, y * 0.130f);
            int c;
            if      (h < 0.30f) c = CLR_DARK_BLUE;       // deep sea
            else if (h < 0.42f) c = CLR_TRUE_BLUE;       // sea
            else if (h < 0.47f) c = CLR_BLUE;            // shallows
            else if (h < 0.51f) c = CLR_LIGHT_PEACH;     // beach
            else if (h < 0.64f) c = CLR_DARK_GREEN;      // grass
            else if (h < 0.74f) c = CLR_MEDIUM_GREEN;    // forest
            else if (h < 0.85f) c = CLR_BROWN;           // rock
            else if (h < 0.93f) c = CLR_DARK_GREY;       // crag
            else                c = CLR_WHITE;           // snow
            world[y * TEX + x] = c;
        }

    cx = 128; cy = 128; ang = 0; spd = 1.4f; H = 26;
    for (int i = 0; i < NRINGS; i++) { ring[i].x = rnd(TEX); ring[i].y = rnd(TEX); }
    score = 0; over = false; t_end = now() + 45;
}

// shortest wrapped delta (the world tiles every 256 units)
static float wrapd(float d) { while (d > 128) d -= TEX; while (d < -128) d += TEX; return d; }

void update(void) {
    if (over) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }
    if (now() >= t_end) { over = true; return; }

    if (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  ang -= 2.2f;
    if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) ang += 2.2f;
    if (btn(0, BTN_UP)    || btn(1, BTN_UP))    spd = clamp(spd + 0.06f, 0.4f, 3.4f);
    if (btn(0, BTN_DOWN)  || btn(1, BTN_DOWN))  spd = clamp(spd - 0.06f, 0.4f, 3.4f);
    if (btn(0, BTN_A)     || btn(1, BTN_A))     H = clamp(H + 0.8f, 14, 60);
    if (btn(0, BTN_B)     || btn(1, BTN_B))     H = clamp(H - 0.8f, 14, 60);

    cx += cos_deg(ang) * spd;
    cy += sin_deg(ang) * spd;

    // fly through rings
    for (int i = 0; i < NRINGS; i++) {
        float d = length((int)wrapd(ring[i].x - cx), (int)wrapd(ring[i].y - cy));
        if (d < 9) {
            score++; t_end += 5;                 // +5 seconds per ring
            ring[i].x = rnd(TEX); ring[i].y = rnd(TEX);
            chord(67, CHORD_MAJ, INSTR_SQUARE, 4);
        }
    }
}

static void draw_ring(int sx, int sy, float rx, float ry, int col) {
    int pxq = sx + (int)rx, pyq = sy;
    for (int a = 15; a <= 360; a += 15) {
        int nx = sx + (int)(rx * cos_deg(a)), ny = sy + (int)(ry * sin_deg(a));
        line(pxq, pyq, nx, ny, col);
        pxq = nx; pyq = ny;
    }
}

void draw(void) {
    // --- sky ---
    rectfill(0, 0,     SCREEN_W, 26,      CLR_DARKER_BLUE);
    rectfill(0, 26,    SCREEN_W, 22,      CLR_DARK_BLUE);
    rectfill(0, 48,    SCREEN_W, 16,      CLR_TRUE_BLUE);
    rectfill(0, 64,    SCREEN_W, HZ - 64, CLR_LIGHT_PEACH);
    circfill(SCREEN_W - 62, 30, 11, CLR_LIGHT_YELLOW);

    // --- MODE 7 GROUND: one perspective-correct texture sample per scanline ---
    float ca = cos_deg(ang), sa = sin_deg(ang);
    float F  = 140.0f, cxs = SCREEN_W / 2.0f;
    for (int sy = HZ; sy < SCREEN_H; sy += BS) {
        int   dy = sy - HZ + 1;
        float z  = (H * F) / dy;                 // distance of this row's ground
        if (z > 1600) z = 1600;                  // tame horizon shimmer
        float stepx = -sa * (z / F) * BS;        // world move per BS pixels across
        float stepy =  ca * (z / F) * BS;
        float lat0  = (0 - cxs) / F * z;          // leftmost column's lateral offset
        float wx = cx + ca * z + (-sa) * lat0;
        float wy = cy + sa * z + ( ca) * lat0;
        for (int sx = 0; sx < SCREEN_W; sx += BS) {
            rectfill(sx, sy, BS, BS, world[(((int)wy) & MASK) * TEX + (((int)wx) & MASK)]);
            wx += stepx; wy += stepy;
        }
    }

    // --- rings: project each onto the screen with the same math, far ones first ---
    int order[NRINGS]; float fwd[NRINGS];
    for (int i = 0; i < NRINGS; i++) {
        float rx = wrapd(ring[i].x - cx), ry = wrapd(ring[i].y - cy);
        fwd[i] = rx * ca + ry * sa;
        order[i] = i;
    }
    for (int a = 0; a < NRINGS; a++)            // sort by depth, far -> near
        for (int b = a + 1; b < NRINGS; b++)
            if (fwd[order[b]] > fwd[order[a]]) { int t = order[a]; order[a] = order[b]; order[b] = t; }

    for (int k = 0; k < NRINGS; k++) {
        int i = order[k];
        if (fwd[i] < 8) continue;               // behind us / too close
        float rx = wrapd(ring[i].x - cx), ry = wrapd(ring[i].y - cy);
        float rightd = rx * (-sa) + ry * ca;
        int   sy = HZ + (int)((H * F) / fwd[i]);
        int   sx = (int)(cxs + rightd * F / fwd[i]);
        float rad = 9.0f * F / fwd[i];          // screen size shrinks with distance
        if (sy > SCREEN_H + 40 || rad < 1) continue;
        draw_ring(sx, sy, rad, rad * 0.42f, CLR_YELLOW);
        draw_ring(sx, sy - 1, rad - 2, rad * 0.42f - 1, CLR_ORANGE);
    }

    // --- HUD ---
    print(str("RINGS %d", score), 6, 6, CLR_WHITE);
    print(str("ALT %d", (int)H), 6, 18, CLR_LIGHT_GREY);
    int secs = (int)(t_end - now()); if (secs < 0) secs = 0;
    print_right(str("%d", secs), SCREEN_W - 6, 6, secs <= 8 ? CLR_RED : CLR_WHITE);
    print_right("Z climb  X dive", SCREEN_W - 6, SCREEN_H - 12, CLR_DARK_GREY);

    if (over) {
        rectfill(80, 80, 160, 40, CLR_BLACK);
        rect(80, 80, 160, 40, CLR_YELLOW);
        print_centered("OUT OF TIME", 90, CLR_YELLOW);
        print_centered(str("%d rings - Z to fly again", score), 104, CLR_LIGHT_GREY);
    }
}
