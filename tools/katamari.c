#include "studio.h"

// KATAMARI — roll a sticky ball around, swallow everything smaller than you,
// and grow. Once you're big enough, the stuff that used to block you becomes
// snack. Soak up as much as you can in 60 seconds.
//
// WASD / arrows to roll.  Z to replay when the time's up.

#define KW   560               // world is bigger than the screen
#define KH   360
#define NITEMS 170
#define NDEC   72              // how many absorbed bits we draw stuck on the ball

typedef struct { float x, y; int size, col, shape; bool alive; } Item;
typedef struct { float ang, frac; int size, col, shape; } Dec;

static float px, py, r;
static Item  items[NITEMS];
static Dec   decs[NDEC];
static int   ndec, count;
static float t_end;
static bool  over;

static float fsqrt(float v) { if (v <= 0) return 0; float g = v; for (int i = 0; i < 8; i++) g = 0.5f * (g + v / g); return g; }

static int item_color(void) {
    int c[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK,
                CLR_LIGHT_PEACH, CLR_LIME_GREEN, CLR_MAUVE, CLR_DARK_PURPLE };
    return c[rnd(10)];
}

static void scatter(void) {
    for (int i = 0; i < NITEMS; i++) {
        int tier = rnd(10);
        int sz = tier < 6 ? rnd_between(2, 6)       // mostly tiny
               : tier < 9 ? rnd_between(8, 16)      // some medium
                          : rnd_between(20, 40);    // a few big
        items[i] = (Item){ rnd_between(16, KW - 16), rnd_between(16, KH - 16),
                           sz, item_color(), rnd(2), true };
    }
}

void init(void) {
    px = KW / 2; py = KH / 2; r = 6;
    ndec = count = 0; over = false;
    scatter();
    t_end = now() + 60;
}

void update(void) {
    if (over) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }
    if (now() >= t_end) { over = true; return; }

    // roll — bigger katamari is a touch slower
    float spd = clamp(3.6f - r * 0.018f, 1.6f, 3.6f);
    float mx = (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) - (btn(0, BTN_LEFT) || btn(1, BTN_LEFT));
    float my = (btn(0, BTN_DOWN)  || btn(1, BTN_DOWN))  - (btn(0, BTN_UP)   || btn(1, BTN_UP));
    px = clamp(px + mx * spd, r, KW - r);
    py = clamp(py + my * spd, r, KH - r);

    for (int i = 0; i < NITEMS; i++) {
        if (!items[i].alive) continue;
        float d = distance((int)px, (int)py, (int)items[i].x, (int)items[i].y);

        if (items[i].size <= r * 0.9f && d < r) {
            // SWALLOW: grow by area, stick a piece on the ball, climb the scale
            r = fsqrt(r * r + items[i].size * items[i].size * 0.35f);
            items[i].alive = false;
            count++;
            decs[ndec % NDEC] = (Dec){ rnd(360), rnd_float_between(0.45f, 0.9f),
                                       min(items[i].size, 7), items[i].col, items[i].shape };
            ndec++;
            note(44 + (count % 14), INSTR_SQUARE, 3);
        } else if (items[i].size > r * 0.9f && d < r + items[i].size) {
            // too big — bump off it
            float dx = px - items[i].x, dy = py - items[i].y, dd = d < 0.01f ? 1 : d;
            px = items[i].x + dx / dd * (r + items[i].size);
            py = items[i].y + dy / dd * (r + items[i].size);
            px = clamp(px, r, KW - r); py = clamp(py, r, KH - r);
        }
    }
}

static void shape_at(int x, int y, int s, int shape, int col) {
    if (shape == 0) circfill(x, y, s, col);
    else            rectfill(x - s, y - s, s * 2, s * 2, col);
}

void draw(void) {
    cls(CLR_DARK_GREEN);
    follow((int)px, (int)py, KW, KH);

    // world floor texture + border
    for (int gx = 0; gx <= KW; gx += 28) line(gx, 0, gx, KH, CLR_MEDIUM_GREEN);
    for (int gy = 0; gy <= KH; gy += 28) line(0, gy, KW, gy, CLR_MEDIUM_GREEN);
    rect(0, 0, KW, KH, CLR_LIME_GREEN);

    // items
    for (int i = 0; i < NITEMS; i++) {
        if (!items[i].alive) continue;
        shape_at((int)items[i].x, (int)items[i].y, items[i].size, items[i].shape, items[i].col);
        if (items[i].size > r * 0.9f)                          // mark the ones too big to eat
            circ((int)items[i].x, (int)items[i].y, items[i].size + 2, CLR_DARK_GREY);
    }

    // the katamari: dirt ball + everything stuck to it
    circfill((int)px, (int)py, (int)r, CLR_BROWN);
    circ((int)px, (int)py, (int)r, CLR_DARK_BROWN);
    int n = ndec < NDEC ? ndec : NDEC;
    for (int i = 0; i < n; i++) {
        int dx = (int)(px + cos_deg(decs[i].ang) * decs[i].frac * r);
        int dy = (int)(py + sin_deg(decs[i].ang) * decs[i].frac * r);
        shape_at(dx, dy, decs[i].size, decs[i].shape, decs[i].col);
    }

    camera(0, 0);

    // HUD
    print(str("SIZE %d", (int)r), 8, 8, CLR_WHITE);
    print(str("things %d", count), 8, 20, CLR_LIGHT_GREY);
    int secs = (int)(t_end - now()); if (secs < 0) secs = 0;
    print_right(str("%d", secs), SCREEN_W - 8, 8, secs <= 10 ? CLR_RED : CLR_WHITE);

    if (over) {
        rectfill(70, 76, 180, 48, CLR_BLACK);
        rect(70, 76, 180, 48, CLR_LIME_GREEN);
        print_centered("TIME!", 84, CLR_YELLOW);
        print_centered(str("size %d  -  %d things", (int)r, count), 98, CLR_WHITE);
        print_centered("Z to replay", 110, CLR_LIGHT_GREY);
    }
}
