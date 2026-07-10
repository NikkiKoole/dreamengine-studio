/* de:meta
{
  "slug": "raycaster",
  "title": "raycaster",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "raycasting"
  ],
  "lineage": "Wolfenstein-3D-style grid DDA raycaster with cos-correction for fisheye; no sprites, pure column strips.",
  "description": "A Wolfenstein-style first-person maze with no 3D and no sprites — one DDA ray per screen column, drawn as a vertical wall strip with fisheye correction. Walk through a colored grid world with a live minimap. WASD to move and turn, B toggles the map."
}
de:meta */
#include "studio.h"
#include <math.h>

// raycaster — a Wolfenstein-style first-person maze, no 3D, no sprites.
//
// The world is a flat 16×16 grid of walls. For every column of the screen we
// shoot one ray out from the player, step it across the grid until it hits a
// wall (the "DDA" algorithm — march to the next grid line, x or y, whichever
// is closer), and measure how far it travelled. Near walls → long vertical
// strip, far walls → short strip. 320 strips side by side = a 3D corridor.
//
// The only trick is fisheye correction: a ray aimed at the edge of your view
// travels farther to the same wall, so we multiply by cos(angle-from-center)
// to flatten the bulge back out.
//
//   UP/DOWN (W/S)  walk forward / back
//   LEFT/RIGHT (A/D)  turn
//   B  toggle the minimap

#define MAP_SIZE 16
#define FOV      60.0f      // horizontal field of view, degrees

static const char *MAP[MAP_SIZE] = {
    "1111111111111111",
    "1..............1",
    "1..222....333..1",
    "1..2........3..1",
    "1..2..55....3..1",
    "1.....5........1",
    "1.....5.....44.1",
    "1...........4..1",
    "1..66.......4..1",
    "1..6...........1",
    "1..6....888....1",
    "1.......8......1",
    "1..77...8......1",
    "1...7..........1",
    "1...7..........1",
    "1111111111111111",
};

static float px = 1.5f, py = 1.5f;   // player position, in grid cells
static float pa = 40.0f;             // player heading, degrees (0 = east, 90 = south)
static bool  minimap = true;

// wall value at a grid cell, 0 = empty
static int cell(int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_SIZE || cy >= MAP_SIZE) return 1;
    char c = MAP[cy][cx];
    return (c >= '1' && c <= '8') ? c - '0' : 0;
}

// each wall type gets a lit color and a shaded color (used for the dim side
// faces and for anything far away — cheap fake lighting)
static int wall_color(int v, int dark) {
    static const int lit [] = { 6, 6, 8, 11, 10, 12, 9, 14, 6 };
    static const int dim [] = { 5, 5, 24, 3, 9, 28, 4, 2, 5 };
    if (v < 1 || v > 8) v = 1;
    return dark ? dim[v] : lit[v];
}

static int solid(float x, float y) { return cell((int)x, (int)y) != 0; }

void update() {
    if (btnp(0, BTN_B)) minimap = !minimap;

    if (btn(0, BTN_LEFT))  pa -= 2.6f;
    if (btn(0, BTN_RIGHT)) pa += 2.6f;

    // walk forward/back along the heading, sliding along walls we bump
    float step = 0.0f;
    if (btn(0, BTN_UP))   step =  0.07f;
    if (btn(0, BTN_DOWN)) step = -0.07f;
    if (step != 0.0f) {
        float mx = dx(step, pa);   // step * cos(pa)
        float my = dy(step, pa);   // step * sin(pa)
        float pad = 0.2f;
        float nx = px + mx;
        if (!solid(nx + (mx > 0 ? pad : -pad), py)) px = nx;
        float ny = py + my;
        if (!solid(px, ny + (my > 0 ? pad : -pad))) py = ny;
    }
}

void draw() {
    // ceiling and floor — two flat bands meeting at the horizon
    rectfill(0, 0, SCREEN_W, SCREEN_H / 2, CLR_DARKER_BLUE);
    rectfill(0, SCREEN_H / 2, SCREEN_W, SCREEN_H / 2, CLR_BROWNISH_BLACK);

    for (int x = 0; x < SCREEN_W; x++) {
        // angle of this column's ray, fanned out across the FOV
        float offset = ((float)x / (SCREEN_W - 1) - 0.5f) * FOV;
        float ra  = pa + offset;
        float rdx = cos_deg(ra);
        float rdy = sin_deg(ra);

        // --- DDA: march to the next grid line until we hit a wall ---
        int mapX = (int)px, mapY = (int)py;
        float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
        float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
        int stepX, stepY;
        float sideX, sideY;
        if (rdx < 0) { stepX = -1; sideX = (px - mapX) * ddx; }
        else         { stepX =  1; sideX = (mapX + 1.0f - px) * ddx; }
        if (rdy < 0) { stepY = -1; sideY = (py - mapY) * ddy; }
        else         { stepY =  1; sideY = (mapY + 1.0f - py) * ddy; }

        int side = 0, hit = 0;
        for (int guard = 0; guard < 64 && !hit; guard++) {
            if (sideX < sideY) { sideX += ddx; mapX += stepX; side = 0; }
            else               { sideY += ddy; mapY += stepY; side = 1; }
            if (cell(mapX, mapY)) hit = 1;
        }

        // perpendicular distance, then undo the fisheye bulge
        float dist = (side == 0) ? (sideX - ddx) : (sideY - ddy);
        dist *= cos_deg(offset);
        if (dist < 0.01f) dist = 0.01f;

        // project distance to a wall-slice height
        int h  = (int)(SCREEN_H / dist);
        int y0 = SCREEN_H / 2 - h / 2;
        int y1 = SCREEN_H / 2 + h / 2;
        if (y0 < 0) y0 = 0;
        if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;

        // y-side hits and distant walls get the shaded color
        int dark = (side == 1) || (dist > 5.0f);
        rectfill(x, y0, 1, y1 - y0 + 1, wall_color(cell(mapX, mapY), dark));
    }

    if (minimap) {
        const int MM = 3, ox = 4, oy = 4;
        rectfill(ox - 1, oy - 1, MAP_SIZE * MM + 2, MAP_SIZE * MM + 2, CLR_BLACK);
        for (int cy = 0; cy < MAP_SIZE; cy++)
            for (int cx = 0; cx < MAP_SIZE; cx++) {
                int v = cell(cx, cy);
                if (v) rectfill(ox + cx * MM, oy + cy * MM, MM, MM, wall_color(v, 0));
            }
        int dotx = ox + (int)(px * MM);
        int doty = oy + (int)(py * MM);
        line(dotx, doty, dotx + (int)dx(5, pa), doty + (int)dy(5, pa), CLR_WHITE);
        rectfill(dotx - 1, doty - 1, 2, 2, CLR_RED);
    }

    print("WASD move/turn  -  B: map", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
}
