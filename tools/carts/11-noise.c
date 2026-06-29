/* de:meta
{
  "title": "11. noise",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Smooth organic randomness. Two layers of scrolling hills and twinkling stars, all from noise2()."
}
de:meta */
#include "studio.h"

void draw() {
    cls(CLR_DARKER_BLUE);

    // stars — each one twinkles at its own noise frequency
    for (int i = 0; i < 50; i++) {
        int sx = (i * 97) % SCREEN_W;
        int sy = (i * 53) % 70;
        if (noise2(i * 0.4f, now() * 0.8f) > 0.55f)
            pset(sx, sy, CLR_WHITE);
    }

    // far hills — slow drift, low frequency
    for (int x = 0; x < SCREEN_W; x++) {
        int h = (int)(noise2(x * 0.012f, now() * 0.04f) * 60) + 20;
        line(x, SCREEN_H - h, x, SCREEN_H, CLR_DARK_GREEN);
    }

    // near hills — faster drift, higher frequency
    for (int x = 0; x < SCREEN_W; x++) {
        int h = (int)(noise2(x * 0.025f + 50, now() * 0.08f) * 40) + 8;
        line(x, SCREEN_H - h, x, SCREEN_H, CLR_GREEN);
    }

    // labels
    print("noise2(x * 0.012, now() * 0.04)", 4, 4, CLR_LIGHT_GREY);
    print("scale x small = gentle hills.", 4, 14, CLR_LIGHT_GREY);
    print("scale time small = slow drift.", 4, 24, CLR_LIGHT_GREY);
}
