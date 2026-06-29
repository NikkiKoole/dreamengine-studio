/* de:meta
{
  "title": "20. effects",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "A gallery of the visual + game-feel calls: print_scaled (big text), spr_rot (spin a sprite), sspr_ex (scale+rotate+pivot), oval/ovalfill, pal() recolor, shake(), fade(), bar() meters, blink(), and dt() for framerate-independent timing. Z: shake, hold X: fade, hold UP: drain the boost bar."
}
de:meta */
#include "studio.h"

// EFFECTS — a gallery of the newer drawing & game-feel calls:
//   print_scaled  big text          spr_rot   spin a whole sprite
//   sspr_ex       scale+rotate+pivot oval/ovalfill   ellipses
//   pal()         recolor on the fly shake()   screen kick
//   fade()        fade to black      bar()     a meter
//   blink()       flashing           dt()      framerate-independent timing
//
// Z: shake     hold X: fade     hold UP: drain the boost bar

static float fuel = 100, fadeLvl = 0;

void init(void) { colorkey(CLR_BLACK); }   // sprite's '.' (black) shows through

void update(void) {
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) shake(8);            // a kick on press

    // fade ramps up while X is held, back down when released — timed with dt()
    if (btn(0, BTN_B) || btn(1, BTN_B)) fadeLvl = clamp(fadeLvl + dt() * 2.5f, 0, 0.85f);
    else                                fadeLvl = clamp(fadeLvl - dt() * 2.5f, 0, 0.85f);
    fade(fadeLvl);

    // boost meter drains while UP is held, refills otherwise. dt() => same on any framerate
    if (btn(0, BTN_UP) || btn(1, BTN_UP)) { fuel = clamp(fuel - 60 * dt(), 0, 100); shake(2); }
    else                                    fuel = clamp(fuel + 25 * dt(), 0, 100);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // big title, centered with text_width (×2 scale → scaled width is text_width*2)
    const char *t = "EFFECTS";
    print_scaled(t, (SCREEN_W - text_width(t) * 2) / 2, 6, CLR_YELLOW, 2);

    int teams[4] = { CLR_RED, CLR_GREEN, CLR_BLUE, CLR_ORANGE };
    int team = teams[(frame() / 45) % 4];

    // 1) spr_rot — spin a whole 16×16 sprite around its center
    spr_rot(1, 38, 72, now() * 80);
    print("spr_rot", 26, 100, CLR_LIGHT_GREY);

    // 2) sspr_ex + pal — slot 1 source, drawn 2× (32×32), rotated, recolored each second
    pal(CLR_WHITE, team);
    sspr_ex(16, 0, 16, 16, 144, 56, 32, 32, now() * 60, 16, 16);
    pal_reset();
    print("sspr_ex + pal", 112, 100, CLR_LIGHT_GREY);

    // 3) oval / ovalfill
    ovalfill(264, 72, 16, 10, team);
    oval(264, 72, 21, 14, CLR_WHITE);
    print("oval", 252, 100, CLR_LIGHT_GREY);

    // bar() — boost meter, drained via dt()
    print("boost (hold UP)", 8, 150, CLR_LIGHT_GREY);
    bar(8, 160, 150, 8, fuel / 100.0f, CLR_GREEN, CLR_DARKER_GREY);

    // blink() — a flashing hint
    if (blink(20)) print("Z: shake     hold X: fade", 8, 184, CLR_YELLOW);
}
