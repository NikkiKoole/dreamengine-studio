/* de:meta
{
  "title": "13. easing",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "ease_in, ease_out, ease_in_out — shape motion from robotic to natural. Four balls, four curves."
}
de:meta */
#include "studio.h"

void draw() {
    cls(CLR_BLACK);

    float t = (float)((int)(now() * 1000) % 2000) / 2000.0f;  // 0..1 every 2 seconds

    // ease_back overshoots past the target, so leave room on the right for the
    // dot to shoot past x1 and spring back without running off the screen edge.
    int x0 = 20, x1 = SCREEN_W - 40;
    int y_in     = 36;
    int y_out    = 76;
    int y_inout  = 116;
    int y_linear = 156;
    int y_back   = 188;

    // draw track lines
    line(x0, y_in,     x1, y_in,     CLR_DARK_GREY);
    line(x0, y_out,    x1, y_out,    CLR_DARK_GREY);
    line(x0, y_inout,  x1, y_inout,  CLR_DARK_GREY);
    line(x0, y_linear, x1, y_linear, CLR_DARK_GREY);
    line(x0, y_back,   x1, y_back,   CLR_DARK_GREY);

    // a dot per easing, riding its curve
    int px_in     = x0 + (int)((x1 - x0) * ease_in(t));
    int px_out    = x0 + (int)((x1 - x0) * ease_out(t));
    int px_inout  = x0 + (int)((x1 - x0) * ease_in_out(t));
    int px_linear = x0 + (int)((x1 - x0) * t);
    int px_back   = x0 + (int)((x1 - x0) * ease_back(t));

    circfill(px_in,     y_in,     6, CLR_RED);
    circfill(px_out,    y_out,    6, CLR_GREEN);
    circfill(px_inout,  y_inout,  6, CLR_YELLOW);
    circfill(px_linear, y_linear, 6, CLR_LIGHT_GREY);
    circfill(px_back,   y_back,   6, CLR_PINK);

    print("ease_in     - slow start",      x0, y_in     - 12, CLR_RED);
    print("ease_out    - slow end",        x0, y_out    - 12, CLR_GREEN);
    print("ease_in_out - slow-fast-slow",  x0, y_inout  - 12, CLR_YELLOW);
    print("linear      - no easing",       x0, y_linear - 12, CLR_LIGHT_GREY);
    print("ease_back   - overshoot + pop", x0, y_back   - 12, CLR_PINK);
}
