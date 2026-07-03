/* de:meta
{
  "title": "bleed rig - blue",
  "status": "active",
  "created": "2026-07-03",
  "kind": ["tech-demo"],
  "teaches": [],
  "lineage": "The clean-canvas half of the multi-cart cross-cart BLEED RIG (bleedred + bleedblue). Draws with NO pal/fillp of its own and its own BLUE sprite sheet, so anything red / patterned / critter-shaped that appears here after a switch from bleedred is state the engine leaked across de_switch_cart. Companion to the video-reset + per-cart-sheet fix. See docs/design/share-panel.md.",
  "description": {
    "summary": "The BLUE half of a cross-cart bleed test: draws plain (no pal/fillp) with a blue fish + a tone.",
    "detail": "The clean canvas. Its solid WHITE bar and WHITE text should stay white, its rectfills solid, and spr(0) should be its own blue fish. If red tint, a fill pattern, or the red critter shows up, that state leaked from bleedred across the switch. Bundle: apps/bleedtest.",
    "controls": "TAB switches carts in the bundle. Standalone it just runs."
  }
}
de:meta */
// bleedblue — the "clean" half of the cross-cart bleed rig. It sets NO pal/fillp and
// uses its OWN blue sprite sheet. After switching from bleedred, any red tint /
// fill pattern / red-critter sprite here is state the engine failed to reset.

#include "studio.h"

void update(void) {
    if (frame() % 45 == 0) note(72, INSTR_SINE, 4);   // a clean high ping
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // NO pal(), NO fillp() — this cart relies on a clean slate after a switch.
    // a fat WHITE bar: stays solid white if nothing leaked; red/checkered if it did.
    rectfill(20, 30, SCREEN_W - 20, 70, CLR_WHITE);

    // bouncing blue fish (this cart's OWN slot-0 sprite)
    int x = 40 + (int)((SCREEN_W - 96) * (0.5 + 0.5 * (float)((frame() % 90) - 45) / 45.0f));
    spr(0, x, 110);
    spr(0, SCREEN_W - 16 - x, 150);

    font(FONT_NORMAL);
    print("CART B (blue)", 20, 12, CLR_WHITE);
    font(FONT_SMALL);
    print("plain: solid white bar + blue fish", 20, 200, CLR_WHITE);
}
