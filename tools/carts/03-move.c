/* de:meta
{
  "title": "3. things that move",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Variables and the game loop — a ball bounces around using update() and velocity."
}
de:meta */
#include "studio.h"

float x  = 160;
float y  = 100;
float vx = 2.3f;
float vy = 1.7f;

void update() {
    x += vx;
    y += vy;
    if (x < 8 || x > SCREEN_W - 8) vx = -vx;
    if (y < 8 || y > SCREEN_H - 8) vy = -vy;
}

void draw() {
    cls(CLR_DARK_BLUE);
    circfill((int)x, (int)y, 8, CLR_YELLOW);
    print("update() runs 60x a second.", 4, 4, CLR_WHITE);
    print("x and y change every frame.", 4, 14, CLR_LIGHT_GREY);
    print(str("x=%.0f  y=%.0f", x, y), 4, 24, CLR_YELLOW);
}
