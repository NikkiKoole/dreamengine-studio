#include "studio.h"

// spirograph — a loop with a fixed turn angle makes a pattern.
//
// a "turtle" is just three numbers — a position and a heading — plus a
// pen. that's small enough to live right here in the cart: the little
// turtle below is the whole thing, built from dx/dy + line(). keep it,
// copy it into your own carts, or change how it draws.
//
// try changing ANGLE to see different shapes:
//   91  = expanding square spiral
//   121 = rose  (triangle-based, default)
//   144 = five-pointed star
//  97.2 = nine-pointed rose

#define ANGLE 121.0f

// ---- a pocket turtle, implemented in the cart ----
static float t_x, t_y, t_heading;
static int   t_color;
static bool  t_pen;

static void turtle_home(void) {
    t_x = SCREEN_W / 2.0f;
    t_y = SCREEN_H / 2.0f;
    t_heading = 0.0f;   // 0 = facing right
    t_pen = false;
}
static void pen_down(void)        { t_pen = true; }
static void pen_color(int color)  { t_color = color; }
static void turtle_turn(float d)  { t_heading += d; }
static void turtle_move(float steps) {
    float nx = t_x + dx(steps, t_heading);
    float ny = t_y + dy(steps, t_heading);
    if (t_pen) line((int)t_x, (int)t_y, (int)nx, (int)ny, t_color);
    t_x = nx;
    t_y = ny;
}
// --------------------------------------------------

static int n = 0;

void update() {
    n += 2;
    if (n > 360) n = 0;
}

void draw() {
    cls(CLR_BLACK);

    turtle_home();
    pen_down();

    for (int i = 0; i < n; i++) {
        pen_color(1 + (i * 3) % 15);
        turtle_move(70.0f);
        turtle_turn(ANGLE);
    }

    print("change ANGLE to explore", 4, SCREEN_H - 12, CLR_DARK_GREY);
}
