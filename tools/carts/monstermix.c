/* de:meta
{
  "title": "monster mix lab",
  "status": "active",
  "kind": [
    "game",
    "tech-demo"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "screen-shake-juice",
    "particle-system"
  ],
  "lineage": "Original cart; uses the pal()-recolor sprite-stacking trick from sprite-draw.js to composite 27 monster combos at bake time — the juicy piston-stamp feedback loop is the novel teaching moment.",
  "genre": "arcade",
  "description": "Assemble the monster the customer ordered, then STAMP it with the piston press. The sprite side is the show: tools/sprite-draw.js draws 9 parts (3 heads, 3 bodies, 3 legs) and stamp()-composites all 27 combos at bake time; the magic pal() indices (28/29) recolor them into 4 schemes at draw time — 108 monsters from 9 drawn parts. Also: a 32-wide machine split() across two slots, a concave polyfill star, noise() speckle tiles, mirror()ed symmetric parts and outlined() borders. Right order = points + a streak of climbing chords; wrong or too slow = a lost heart. LEFT/RIGHT choose a slot, UP/DOWN change the part, Z stamps."
}
de:meta */
// MONSTER MIX LAB — assemble the monster the customer ordered, then STAMP it.
//
// The sprite side is the show: monstermix.cart.js draws 9 parts (3 heads,
// 3 bodies, 3 legs) and stamp()-composites all 27 combos into slots 0–26 at
// bake time. Parts use the magic pal() indices (28 body / 29 accent), so this
// cart recolors any combo into 4 schemes at draw time — 108 monsters total.
//
// A customer walks in with an order (combo + color). Pick parts with the
// selector, then hit Z: a piston stamps the monster (squash + dust + shake)
// and the verdict lands. Right = points + combo streak (chords climb with
// the streak). Wrong or too slow = lose a heart. Three hearts, then it's over.
//
// LEFT/RIGHT choose a slot, UP/DOWN change the part, Z stamp.

#include "studio.h"

// ---- monster genetics -------------------------------------------------------
#define SLOTS 4                       // head, body, legs, color
static const int   NOPT[SLOTS]  = { 3, 3, 3, 4 };
static const char *SLOT_NAME[SLOTS] = { "HEAD", "BODY", "LEGS", "COLOR" };
static const char *OPT_NAME[SLOTS][4] = {
    { "ROUND",  "BOXY",  "SPIKY",  "" },
    { "BLOB",   "ARMOR", "FUZZY",  "" },
    { "STUBBY", "LONG",  "WHEELS", "" },
    { "GREEN",  "PINK",  "BLUE",   "ORANGE" },
};
static const int MAIN[4] = { CLR_GREEN, CLR_PINK, CLR_BLUE, CLR_ORANGE };
static const int DARK[4] = { CLR_DARK_GREEN, CLR_DARK_PURPLE, CLR_DARK_BLUE, CLR_BROWN };

static int ord[SLOTS];                // what the customer wants
static int bld[SLOTS];                // what the player has assembled
static int cust[SLOTS];               // the customer's own look (random)

// ---- game state -------------------------------------------------------------
enum { TITLE, PLAY, PRESS, GAMEOVER };
static int   phase = TITLE;
static int   sel;                     // selected slot 0..3
static int   score, best, combo, lives, orders;
static float tleft, tmax;             // order timer
static int   last_sec;                // for the countdown tick
static float cust_in;                 // customer walk-in 0..1
static int   press_t;                 // press animation clock
static int   verdict;                 // 1 = correct (computed when piston lands)
static float squash;                  // build-monster squash 0..1
static int   flash_red;               // damage vignette frames
static float combo_pop;               // combo counter pop scale

// build-monster display position (3x = 48px tall)
#define BX 200
#define BY 100
// machine drawn 2x at:
#define MX 192
#define MY 46

// ---- particles (CLAUDE.md pool pattern) --------------------------------------
typedef struct { float x, y, vx, vy; int life, col; } Dust;
static Dust dust[32];

static void spawn_dust(float px, float py) {
    for (int k = 0; k < 10; k++)
        for (int i = 0; i < 32; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ px + rnd_between(-20, 21), py,
                    rnd_float_between(-1.6f, 1.6f), rnd_float_between(-1.4f, -0.2f),
                    rnd_between(10, 20), (k & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY };
                break;
            }
}

typedef struct { float x, y, vx, vy, ang, spin; int life; } Star;
static Star stars[8];

static void spawn_stars(float px, float py) {
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 8; i++)
            if (stars[i].life <= 0) {
                stars[i] = (Star){ px + rnd_between(-12, 13), py,
                    rnd_float_between(-0.9f, 0.9f), rnd_float_between(-2.2f, -1.0f),
                    (float)rnd(360), rnd_float_between(-9, 9), rnd_between(30, 45) };
                break;
            }
}

typedef struct { float x, y; int val, life; } Floater;
static Floater floats[6];

static void spawn_float(float px, float py, int val) {
    for (int i = 0; i < 6; i++)
        if (floats[i].life <= 0) { floats[i] = (Floater){ px, py, val, 45 }; break; }
}

// ---- drawing helpers ----------------------------------------------------------

// draw a monster combo recolored into scheme c (the pal() magic-index trick)
static void monster(const int *m, int x, int y, int sc) {
    int s = m[0] * 9 + m[1] * 3 + m[2];
    pal(28, MAIN[m[3]]); pal(29, DARK[m[3]]);
    sspr((s % 8) * 16, (s / 8) * 16, 16, 16, x, y, 16 * sc, 16 * sc);
    pal_reset();
}

// same, squashed flat by sq (0..1) — feet stay planted, body widens
static void monster_squash(const int *m, int x, int y, int sc, float sq) {
    int s  = m[0] * 9 + m[1] * 3 + m[2];
    int dh = (int)(16 * sc * (1.0f - 0.35f * sq));
    int dw = (int)(16 * sc * (1.0f + 0.30f * sq));
    pal(28, MAIN[m[3]]); pal(29, DARK[m[3]]);
    sspr((s % 8) * 16, (s / 8) * 16, 16, 16, x - (dw - 16 * sc) / 2, y + 16 * sc - dh, dw, dh);
    pal_reset();
}

static void heart(int x, int y, int c) {
    circfill(x + 2, y + 2, 2, c);
    circfill(x + 7, y + 2, 2, c);
    trifill(x - 1, y + 3, x + 10, y + 3, x + 4, y + 9, c);
}

// ---- round flow ----------------------------------------------------------------

static void new_order(void) {
    for (int i = 0; i < SLOTS; i++) { ord[i] = rnd(NOPT[i]); cust[i] = rnd(NOPT[i]); }
    orders++;
    tmax  = clamp(13.0f - orders * 0.35f, 6.0f, 13.0f);
    tleft = tmax;
    last_sec = 99;
    cust_in  = 0;
    hit(72, 5, 3, 50);                        // door blip
}

static void reset_game(void) {
    score = 0; combo = 0; lives = 3; orders = 0;
    for (int i = 0; i < SLOTS; i++) bld[i] = 0;
    sel = 0;
    new_order();
}

static void lose_heart(void) {
    lives--; combo = 0; flash_red = 8;
    shake(3);
    chord(34, CHORD_DIM, INSTR_SAW, 5);       // sour buzz
    if (lives <= 0) {
        phase = GAMEOVER;
        if (score > best) { best = score; save_int("best", best); }
    }
}

void init(void) {
    colorkey(CLR_BLACK);                           // index 0 = transparent in sprites
    instrument(5, INSTR_SQUARE, 2, 60, 3, 80);     // UI pluck
    instrument_duty(5, 0.25f);
    instrument(6, INSTR_TRI, 5, 150, 5, 250);      // soft chime
    best = load_int("best", 0);
}

// ---- update ---------------------------------------------------------------------

void update(void) {
    // particles tick in every phase
    for (int i = 0; i < 32; i++) if (dust[i].life > 0) {
        dust[i].x += dust[i].vx; dust[i].y += dust[i].vy;
        dust[i].vy += 0.12f; dust[i].life--;
    }
    for (int i = 0; i < 8; i++) if (stars[i].life > 0) {
        stars[i].x += stars[i].vx; stars[i].y += stars[i].vy;
        stars[i].vy += 0.06f; stars[i].ang += stars[i].spin; stars[i].life--;
    }
    for (int i = 0; i < 6; i++) if (floats[i].life > 0) { floats[i].y -= 0.7f; floats[i].life--; }
    squash    = lerp(squash, 0.0f, 0.18f);
    combo_pop = lerp(combo_pop, 0.0f, 0.15f);
    if (flash_red > 0) flash_red--;

    if (phase == TITLE) {
        if (btnp(0, BTN_A)) { strum(48, CHORD_MAJ, 6, 5, 40); reset_game(); phase = PLAY; }
        return;
    }

    if (phase == GAMEOVER) {
        if (btnp(0, BTN_A)) { reset_game(); phase = PLAY; }
        return;
    }

    if (phase == PLAY) {
        cust_in += dt() * 2.0f;
        tleft   -= dt();

        // countdown ticks for the last 3 seconds
        if (tleft < 3.0f && tleft > 0 && (int)tleft != last_sec) {
            last_sec = (int)tleft;
            hit(88, 5, 4, 35);
        }
        if (tleft <= 0) {                      // customer storms out
            lose_heart();
            if (phase == PLAY) new_order();
            return;
        }

        if (btnp(0, BTN_LEFT))  { sel = (sel + SLOTS - 1) % SLOTS; hit(64 + sel * 2, 5, 3, 30); }
        if (btnp(0, BTN_RIGHT)) { sel = (sel + 1) % SLOTS;         hit(64 + sel * 2, 5, 3, 30); }
        if (btnp(0, BTN_UP))    { bld[sel] = (bld[sel] + 1) % NOPT[sel];                  hit(76, 5, 3, 25); squash = -0.4f; }
        if (btnp(0, BTN_DOWN))  { bld[sel] = (bld[sel] + NOPT[sel] - 1) % NOPT[sel];      hit(74, 5, 3, 25); squash = -0.4f; }
        if (btnp(0, BTN_A)) {                  // STAMP IT
            phase = PRESS; press_t = 0;
            hit(45, INSTR_NOISE, 4, 140);      // piston whoosh
        }
        return;
    }

    if (phase == PRESS) {
        press_t++;
        if (press_t == 10) {                   // piston lands
            squash = 1.0f;
            shake(2.5f);
            spawn_dust(BX + 24, BY + 46);
            hit(36, INSTR_NOISE, 6, 80);
            verdict = 1;
            for (int i = 0; i < SLOTS; i++) if (bld[i] != ord[i]) verdict = 0;
        }
        if (press_t >= 10 && press_t < 22) squash = 1.0f;   // pinned under the pad
        if (press_t == 16) {                   // verdict feedback
            if (verdict) {
                int pts = 10 + 2 * combo;
                score += pts; combo++; combo_pop = 1.0f;
                spawn_stars(BX + 24, BY + 10);
                spawn_float(BX + 16, BY - 6, pts);
                strum(60 + min(combo * 2, 14), CHORD_MAJ, 6, 5, 25);  // climbs with the streak
            } else {
                lose_heart();
            }
        }
        if (press_t >= 40) {                   // piston gone — next customer
            if (phase != GAMEOVER) { new_order(); phase = PLAY; }
        }
        return;
    }
}

// ---- draw ------------------------------------------------------------------------

static void draw_lab(void) {
    cls(CLR_DARKER_BLUE);
    map(0, 0, 0, 0, 20, 13);                  // wall + floor tiles from the .cart.js
}

static void draw_machine_and_build(void) {
    // mixer machine: 32x16 across sheet slots 28+29, drawn 2x
    sspr(64, 48, 32, 16, MX, MY, 64, 32);

    // pedestal under the build
    rrectfill(BX - 2, BY + 46, 52, 6, 2, CLR_DARK_GREY);
    rectfill(BX, BY + 47, 48, 2, CLR_MEDIUM_GREY);

    // the build, squashed if the piston has it
    if (squash > 0.04f || squash < -0.04f) monster_squash(bld, BX, BY, 3, squash);
    else                                   monster(bld, BX, BY, 3);

    // piston during the press
    if (phase == PRESS) {
        float pad_y = 56;
        if      (press_t <= 10) pad_y = lerp(56, BY + 9, ease_in(press_t / 10.0f));
        else if (press_t <  22) pad_y = BY + 9;
        else                    pad_y = lerp(BY + 9, 56, ease_out(clamp((press_t - 22) / 10.0f, 0, 1)));
        rectfill(BX + 20, MY + 30, 8, (int)pad_y - MY - 30, CLR_DARK_GREY);   // arm
        rrectfill(BX + 2, (int)pad_y, 44, 8, 2, CLR_LIGHT_GREY);              // pad
        rectfill(BX + 2, (int)pad_y + 6, 44, 2, CLR_DARK_GREY);
    }
}

static void draw_customer(void) {
    float t  = ease_out(clamp(cust_in, 0, 1));
    int   cx = -40 + (int)(t * 66);                    // slides in to x=26
    int   cy = 104 + (int)(sin_deg(now() * 300) * 1.5f);
    monster(cust, cx, cy, 2);

    if (cust_in >= 1) {                                // speech bubble with the order
        int bx = cx + 22, by = 44;
        rrectfill(bx, by, 56, 52, 4, CLR_WHITE);
        trifill(bx + 6, by + 48, bx + 20, by + 48, bx + 4, by + 58, CLR_WHITE);
        monster(ord, bx + 12, by + 4, 2);
        font(FONT_TINY);
        print_centered(OPT_NAME[3][ord[3]], bx + 28, by + 42, DARK[ord[3]]);
        font(FONT_NORMAL);
    }
}

static void draw_selector(void) {
    rrectfill(8, 156, 304, 40, 4, CLR_DARKER_GREY);
    for (int i = 0; i < SLOTS; i++) {
        int cx = 14 + i * 75, on = (i == sel);
        rrectfill(cx, 162, 68, 28, 3, on ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        font(FONT_TINY);
        print_centered(SLOT_NAME[i], cx + 34, 165, on ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
        font(FONT_SMALL);
        int tc = on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY;
        print_centered(OPT_NAME[i][bld[i]], cx + 34, 178, tc);
        font(FONT_NORMAL);
        if (on && blink(16)) {                         // up/down nudge arrows
            trifill(cx + 30, 160, cx + 38, 160, cx + 34, 156, CLR_WHITE);
            trifill(cx + 30, 192, cx + 38, 192, cx + 34, 196, CLR_WHITE);
        }
    }
    // color swatch dot next to the color name
    circfill(14 + 3 * 75 + 12, 181, 3, MAIN[bld[3]]);
}

static void draw_hud(void) {
    font(FONT_SMALL);
    print_outline(str("SCORE %d", score), 10, 5, CLR_WHITE, CLR_BROWNISH_BLACK);
    print_outline(str("BEST %d", best), 10, 13, CLR_MEDIUM_GREY, CLR_BROWNISH_BLACK);
    for (int i = 0; i < 3; i++)
        heart(282 + i * 13, 5, i < lives ? CLR_RED : CLR_DARKER_GREY);
    if (combo > 1) {
        int sc = combo_pop > 0.4f ? 2 : 1;
        font(FONT_NORMAL);
        print_scaled(str("x%d", combo), 160 - 8 * sc, 4, CLR_YELLOW, sc);
    }
    font(FONT_NORMAL);
    // order timer
    int bc = (tleft < 3.0f && blink(8)) ? CLR_RED : CLR_GREEN;
    bar(80, 22, 160, 6, tleft / tmax, bc, CLR_BROWNISH_BLACK);
}

static void draw_particles(void) {
    for (int i = 0; i < 32; i++) if (dust[i].life > 0)
        pset((int)dust[i].x, (int)dust[i].y, dust[i].col);
    for (int i = 0; i < 8; i++) if (stars[i].life > 0)
        spr_rot(30, (int)stars[i].x, (int)stars[i].y, stars[i].ang);
    font(FONT_SMALL);
    for (int i = 0; i < 6; i++) if (floats[i].life > 0)
        print_outline(str("+%d", floats[i].val), (int)floats[i].x, (int)floats[i].y,
                      CLR_YELLOW, CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);
}

void draw(void) {
    if (phase == TITLE) {
        cls(CLR_DARKER_BLUE);
        map(0, 0, 0, 0, 20, 13);
        // a wall of all 27 combos, colors drifting — the whole bake on display
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 9; c++) {
                int m[4] = { r, c / 3, c % 3, (c + r + frame() / 40) % 4 };
                monster(m, 17 + c * 32, 64 + r * 36, 2);
            }
        print_scaled("MONSTER MIX LAB", 160 - text_width("MONSTER MIX LAB"), 24, CLR_YELLOW, 2);
        if (blink(20)) print_centered("PRESS Z TO START", 160, 180, CLR_WHITE);
        font(FONT_SMALL);
        print_centered("ARROWS PICK PARTS - Z STAMPS THE MONSTER", 160, 50, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        return;
    }

    draw_lab();
    draw_machine_and_build();
    draw_customer();
    draw_selector();
    draw_particles();
    draw_hud();

    // damage vignette — red border flash on a lost heart
    if (flash_red > 0) {
        rectfill(0, 0, SCREEN_W, 3, CLR_RED);
        rectfill(0, SCREEN_H - 3, SCREEN_W, 3, CLR_RED);
        rectfill(0, 0, 3, SCREEN_H, CLR_RED);
        rectfill(SCREEN_W - 3, 0, 3, SCREEN_H, CLR_RED);
    }

    if (phase == GAMEOVER) {
        fade(0.55f);
        print_scaled("GAME OVER", 160 - text_width("GAME OVER"), 70, CLR_RED, 2);
        print_centered(str("SCORE %d   BEST %d", score, best), 160, 100, CLR_WHITE);
        if (blink(20)) print_centered("PRESS Z TO RESTART", 160, 120, CLR_YELLOW);
    }

#ifdef DE_TRACE
    watch("phase", "%d", phase);
    watch("ord", "%d%d%d%d", ord[0], ord[1], ord[2], ord[3]);
    watch("bld", "%d%d%d%d", bld[0], bld[1], bld[2], bld[3]);
    watch("tleft", "%.1f", tleft);
#endif
}
