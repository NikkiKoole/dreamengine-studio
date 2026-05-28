#ifndef STUDIO_H
#define STUDIO_H

#include <stdbool.h>

// ------------------------------------------------------------
// screen
// ------------------------------------------------------------

#ifndef SCREEN_W
#define SCREEN_W  320
#endif
#ifndef SCREEN_H
#define SCREEN_H  200
#endif

// ------------------------------------------------------------
// buttons
// ------------------------------------------------------------

#define BTN_UP     0
#define BTN_DOWN   1
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_A      4   // z / gamepad a
#define BTN_B      5   // x / gamepad b

// ------------------------------------------------------------
// palette (32 fixed colors, 0-31)
// ------------------------------------------------------------

#define CLR_BLACK   0
#define CLR_WHITE   1
#define CLR_RED     2
#define CLR_GREEN   3
#define CLR_BLUE    4
// ... rest TBD when palette is finalised

// ------------------------------------------------------------
// callbacks — you implement these (update is optional)
// ------------------------------------------------------------

void draw(void);
void update(void);

// ------------------------------------------------------------
// api
// ------------------------------------------------------------

// input
bool btn(int player, int button);  // player: 0 or 1

// graphics
void cls(int color);
void spr(int index, int x, int y);
void print(const char *text, int x, int y, int color);
void line(int x1, int y1, int x2, int y2, int color);
void pixel(int x, int y, int color);
void rect(int x, int y, int w, int h, int color);       // rectangle border
void rectfill(int x, int y, int w, int h, int color);   // filled rectangle
void circle(int x, int y, int radius, int color);       // circle border
void circlefill(int x, int y, int radius, int color);   // filled circle

#endif // STUDIO_H
