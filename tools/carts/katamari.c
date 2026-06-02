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

typedef struct { float x, y; int size, col, shape, pat, hole; bool alive; } Item;
typedef struct { float ang, frac; int size, col, shape, pat, hole; } Dec;

static float px, py, r;
static Item  items[NITEMS];
static Dec   decs[NDEC];
static int   ndec, count;
static float t_end;
static bool  over;


static int item_color(void) {
    int c[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK,
                CLR_LIGHT_PEACH, CLR_LIME_GREEN, CLR_MAUVE, CLR_DARK_PURPLE };
    return c[rnd(10)];
}

// a random fill pattern (sometimes a fully random 16-bit one) for variety
static int item_pattern(void) {
    int p[] = { FILL_SOLID, FILL_CHECKER, FILL_DOTS, FILL_HLINES, FILL_VLINES, FILL_DIAG, FILL_GRID };
    return rnd(10) < 7 ? p[rnd(7)] : rnd(0x10000);
}
// a darker companion shade per base color, so the dither reads as shading
// (a lit sphere/box) instead of two clashing random colors.
static int shade_of(int col) {
    switch (col) {
        case CLR_RED:         return CLR_DARK_RED;
        case CLR_ORANGE:      return CLR_DARK_ORANGE;
        case CLR_YELLOW:      return CLR_ORANGE;
        case CLR_GREEN:       return CLR_MEDIUM_GREEN;
        case CLR_BLUE:        return CLR_TRUE_BLUE;
        case CLR_PINK:        return CLR_MAUVE;
        case CLR_LIGHT_PEACH: return CLR_DARK_PEACH;
        case CLR_LIME_GREEN:  return CLR_MEDIUM_GREEN;
        case CLR_MAUVE:       return CLR_DARKER_PURPLE;
        case CLR_DARK_PURPLE: return CLR_DARKER_PURPLE;
        default:              return CLR_DARK_GREY;
    }
}

static void scatter(void) {
    for (int i = 0; i < NITEMS; i++) {
        int tier = rnd(10);
        int sz = tier < 6 ? rnd_between(2, 6)       // mostly tiny
               : tier < 9 ? rnd_between(8, 16)      // some medium
                          : rnd_between(20, 40);    // a few big
        int col = item_color();
        items[i] = (Item){ rnd_between(16, KW - 16), rnd_between(16, KH - 16),
                           sz, col, rnd(2), item_pattern(), shade_of(col), true };
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
                                       min(items[i].size, 7), items[i].col, items[i].shape,
                                       items[i].pat, items[i].hole };
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

static void shape_at(int x, int y, int s, int shape, int col, int pat, int shade) {
    // ONE dithered shape — base color on the pattern's 0-bits, a darker shade of
    // itself on the 1-bits. Both are the item's own colors and it's a single fill,
    // so the silhouette stays clean (no second shape behind, nothing pokes out).
    bool dith = (pat != FILL_SOLID && shade != col);
    if (dith) { fillp(pat, shade); fillp_anchor(x, y); }  // anchor to this shape's center so the
                                                          // dither travels with it (no crawl)
    if (shape == 0) circfill(x, y, s, col);
    else            rectfill(x - s, y - s, s * 2, s * 2, col);
    if (dith) { fillp_reset(); fillp_anchor(0, 0); }
}

// the "too big to eat" marker: a slightly larger SOLID shape drawn BEHIND the
// fill, so the fill (even dithered) sits on top and leaves a clean 1px ring.
// matches the fill's own shape exactly — no stroke-vs-fill rasterizer mismatch.
static void backing(int x, int y, int s, int shape, int col) {
    if (shape == 0) circfill(x, y, s + 1, col);
    else            rectfill(x - s - 1, y - s - 1, (s + 1) * 2, (s + 1) * 2, col);
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
        // every item gets a crisp 1px outline (a darker shade of itself); the ones
        // too big to roll up get a dark-grey ring instead, as the marker.
        int ring = items[i].size > r * 0.9f ? CLR_DARK_GREY : items[i].hole;
        backing((int)items[i].x, (int)items[i].y, items[i].size, items[i].shape, ring);
        shape_at((int)items[i].x, (int)items[i].y, items[i].size, items[i].shape, items[i].col, items[i].pat, items[i].hole);
    }

    // the katamari: dirt ball + everything stuck to it
    circfill((int)px, (int)py, (int)r, CLR_BROWN);
    circ((int)px, (int)py, (int)r, CLR_DARK_BROWN);
    int n = ndec < NDEC ? ndec : NDEC;
    for (int i = 0; i < n; i++) {
        int dx = (int)(px + cos_deg(decs[i].ang) * decs[i].frac * r);
        int dy = (int)(py + sin_deg(decs[i].ang) * decs[i].frac * r);
        shape_at(dx, dy, decs[i].size, decs[i].shape, decs[i].col, decs[i].pat, decs[i].hole);
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
