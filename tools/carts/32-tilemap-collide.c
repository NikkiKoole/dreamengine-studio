/* de:meta
{
  "slug": "32-tilemap-collide",
  "title": "32. tilemap collision",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "tilemap-collision"
  ],
  "description": "Walk a dot through a brick maze — the walls are just tiles in the map, and the map stops you."
}
de:meta */
#include "studio.h"

// THE BIG IDEA: a tilemap is a grid of tile numbers (see 32-tilemap-collide.cart.js).
//   0 = empty floor (you can walk here).  1 = our brick wall sprite (solid).
// Two engine helpers read that grid for us:
//   touching_map(x, y, w, h)  -> true if the pixel box overlaps ANY non-zero tile
//   tile_at(px, py)           -> the tile number under one pixel (0 = floor)
// So collision is just: "would my new box touch a wall? then don't move there."

int px = 32;   // player position in PIXELS (starts on open floor, cell 2,2)
int py = 32;
int pw = 12;   // player is a little 12x12 dot, a bit smaller than one 16px cell
int ph = 12;

// Try to move the player by (dx, dy) on ONE axis, but only if the destination
// is clear. We do X and Y separately so that when you press into a wall
// diagonally you still SLIDE along it instead of sticking.
void try_move(int dx, int dy) {
    int nx = px + dx;
    int ny = py + dy;
    // touching_map asks the map: does the box at (nx,ny) sized pw x ph hit a wall?
    if (!touching_map(nx, ny, pw, ph)) {
        px = nx;
        py = ny;
    } else {
        // We hit a wall this frame — feel it with a short low blip.
        note(48, INSTR_SQUARE, 2);
    }
}

void update() {
    // Move each axis on its own so walls block one direction, not both.
    if (btn(0, BTN_LEFT))  try_move(-2, 0);
    if (btn(0, BTN_RIGHT)) try_move(2, 0);
    if (btn(0, BTN_UP))    try_move(0, -2);
    if (btn(0, BTN_DOWN))  try_move(0, 2);
}

void draw() {
    cls(CLR_DARK_GREY);

    // Draw the whole map: 20 cells across, 12 down, starting at screen (0,0).
    // Each non-zero cell paints its sprite (our brick wall); 0-cells are skipped.
    map(0, 0, 0, 0, 20, 12);

    // The player dot. Green when free, flashing white when pressed against a wall.
    // We peek one pixel just outside our right edge to sense a wall for the HUD.
    bool blocked = touching_map(px - 1, py, pw + 2, ph)
                || touching_map(px, py - 1, pw, ph + 2);
    rectfill(px, py, pw, ph, blocked ? CLR_WHITE : CLR_GREEN);

    // HUD: name the concept and show the live tile number under the player.
    rectfill(0, 0, SCREEN_W, 12, CLR_DARKER_GREY);
    print("arrows to walk the maze", 4, 3, CLR_WHITE);
    int under = tile_at(px + pw / 2, py + ph / 2);   // tile the player is standing on
    print(str("tile under you: %d (0=floor)", under), 4, SCREEN_H - 10,
          blocked ? CLR_YELLOW : CLR_LIGHT_GREY);
}
