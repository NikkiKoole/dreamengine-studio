/* de:meta
{
  "title": "bleed rig - red",
  "status": "active",
  "created": "2026-07-03",
  "kind": ["tech-demo"],
  "teaches": [],
  "lineage": "Half of the multi-cart cross-cart BLEED RIG (bleedred + bleedblue), built to expose (then verify the fix for) state that leaks across de_switch_cart: the sprite SHEET, and the set-and-hold VIDEO state pal()/fillp(). The sound half of de_switch_cart is already fixed (ADR-0027); this rig is the video twin's test case. See docs/design/share-panel.md.",
  "description": {
    "summary": "The RED half of a cross-cart bleed test: sets pal(white->red) + a fill pattern + drums, draws a red critter.",
    "detail": "Deliberately dirties the shared video state: remaps white to red with pal(), turns on a fillp() checker, and plays a membrane drum. Its sibling bleedblue draws plain, so switching red->blue reveals whether pal/fillp/the sprite sheet survived the switch. Bundle: apps/bleedtest.",
    "controls": "TAB switches carts in the bundle. Standalone it just runs."
  }
}
de:meta */
// bleedred — the "dirty" half of the cross-cart bleed rig. It sets set-and-hold
// VIDEO state (pal + fillp) and draws its own RED sprite sheet, so that after a
// switch its sibling (bleedblue) reveals anything the engine failed to reset.

#include "studio.h"

void update(void) {
    if (frame() % 30 == 0) note(36, INSTR_MEMBRANE, 4);   // a kick every half-second-ish
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // ── the set-and-hold state that MUST NOT leak into the next cart ──
    pal(CLR_WHITE, CLR_RED);          // remap: everything "white" now draws RED
    fillp(0xa5a5, -1);                // a checker fill pattern (holes transparent)

    // a fat bar drawn in "white" (→ red, checkered) — the leak signature
    rectfill(20, 30, SCREEN_W - 20, 70, CLR_WHITE);

    // bouncing red critter (this cart's OWN slot-0 sprite)
    int x = 40 + (int)((SCREEN_W - 96) * (0.5 + 0.5 * (float)((frame() % 120) - 60) / 60.0f));
    spr(0, x, 110);
    spr(0, SCREEN_W - 16 - x, 150);

    font(FONT_NORMAL);
    print("CART A (red)", 20, 12, CLR_WHITE);         // "white" → red here
    font(FONT_SMALL);
    print("pal white->red + fillp on + drum", 20, 200, CLR_WHITE);
}
