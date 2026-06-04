#include "studio.h"

// GALERIJFLAT — step 1: the static facade.
//
// The archetypal Dutch gallery-access apartment slab, gallery side, straight
// on, at dusk. No clock, no people yet — this step is the canvas: roll a
// building (storeys, bays, concrete tints, railing style), roll a household
// for every dwelling (archetype FIRST, then curtains/sill/light as correlated
// traits of it — see docs/design/galerijflat.md, systems 2/3/5), and render
// the grid of lives. A stranger should be able to read the facade: vitrage +
// symmetrical plants = elderly, bare glass + blue TV flicker = student.
//
//   SPACE — re-roll the building

#define GROUND_H 12
#define PLINTH_H 14
#define FH       24          // floor band height
#define TW       20          // lift tower width
#define WW       10          // window width
#define WH       10          // window height
#define DW        7          // door width
#define SLAB_H    2          // gallery slab thickness
#define SPANDREL  4          // wall gap above door/window before next slab
#define BAY_PAD   3          // left margin inside each bay before the door
#define WIN_GAP   3          // gap between door right edge and window left edge
#define MAXF     12
#define MAXB     12

enum { A_VACANT, A_ELDER, A_COUPLE, A_FAMILY, A_STUDENT };
enum { TR_NONE, TR_VITRAGE, TR_CURTAIN, TR_ROLLER, TR_VENETIAN };
enum { SI_EMPTY, SI_SYMM, SI_RANDOM };
enum { RAIL_BARS, RAIL_PANEL };

typedef struct {
    int   arch;
    int   lit, tv;             // dusk snapshot of the lights
    int   treat;               // window treatment (system 2)
    int   tBright, tDark;      // its color when lit / unlit behind it
    float roller;              // 0..1 how far down the roller blind hangs
    int   curtOpen;            // curtains pulled aside?
    int   sill, nIt;           // vensterbank (system 3)
    int   itX[4], itPlant[4], itCol[4];
    int   doorCol;
    int   bike;                // a bike leans on the railing at this bay
} Home;

static int  NF, NB, BW;                       // floors, bays, bay width
static int  baysX, towerX, towerLeft;         // layout
static int  baseY, wallTop;                   // plinth top, wall top
static int  wallC, slabC, towerC, railStyle, panelC, doorBase;
static int  liftFloor, lampX[3], nLamp;
static Home homes[MAXF][MAXB];

// curtain fabric: {lit-behind, unlit-behind} palette pairs
static const int CURT[][2] = {
    { CLR_RED,        CLR_DARK_RED    }, { CLR_ORANGE,     CLR_DARK_ORANGE },
    { CLR_GREEN,      CLR_DARK_GREEN  }, { CLR_BLUE,       CLR_TRUE_BLUE   },
    { CLR_PINK,       CLR_MAUVE       }, { CLR_LIGHT_PEACH, CLR_MEDIUM_GREY },
};
#define NCURT 6

// ── rolling one household (archetype first, traits derived) ──────────────────
static void roll_home(Home *h) {
    *h = (Home){0};
    int r = rnd(100);
    h->arch = r < 8 ? A_VACANT : r < 30 ? A_ELDER : r < 55 ? A_COUPLE
            : r < 82 ? A_FAMILY : A_STUDENT;
    int c = rnd(NCURT);
    h->tBright = CURT[c][0]; h->tDark = CURT[c][1];
    { int dc = chance(15) ? CURT[rnd(NCURT)][1] : doorBase;
      h->doorCol = (dc == wallC) ? doorBase : dc; }             // never same as wall

    switch (h->arch) {
    case A_VACANT:
        h->treat = TR_NONE; h->sill = SI_EMPTY; h->lit = 0;
        break;
    case A_ELDER:
        h->treat = chance(60) ? TR_VITRAGE : chance(70) ? TR_CURTAIN : TR_VENETIAN;
        h->curtOpen = 1;                       // elderly curtains stay open at dusk
        h->sill = chance(82) ? SI_SYMM : SI_RANDOM;
        h->nIt  = 2 + rnd(3);
        h->lit  = chance(72);
        h->bike = chance(6);
        break;
    case A_COUPLE:
        h->treat = chance(40) ? TR_VENETIAN : chance(50) ? TR_CURTAIN
                 : chance(50) ? TR_VITRAGE : TR_NONE;
        h->curtOpen = chance(60);
        h->sill = chance(55) ? SI_SYMM : SI_EMPTY;
        h->nIt  = 1 + rnd(2);
        h->lit  = chance(68); h->tv = h->lit && chance(25);
        h->bike = chance(16);
        break;
    case A_FAMILY:
        h->treat = chance(50) ? TR_CURTAIN : chance(60) ? TR_ROLLER : TR_VITRAGE;
        h->curtOpen = chance(35);              // mostly drawn by dusk, kids
        h->roller = rnd_float_between(0.25f, 0.6f);
        h->sill = chance(70) ? SI_RANDOM : SI_EMPTY;   // toys, not vases
        h->nIt  = 2 + rnd(3);
        h->lit  = chance(85); h->tv = h->lit && chance(35);
        h->bike = chance(30);
        break;
    case A_STUDENT:
        h->treat = chance(50) ? TR_NONE : chance(70) ? TR_ROLLER : TR_VENETIAN;
        h->roller = rnd_float_between(0.3f, 0.7f);     // half-stuck
        h->sill = chance(50) ? SI_EMPTY : SI_RANDOM;
        h->nIt  = 1 + rnd(2);
        h->lit  = chance(60); h->tv = h->lit && chance(60);
        h->bike = chance(34);
        break;
    }

    // vensterbank item layout (positions in 0..7 inside the 10px window)
    if (h->sill == SI_EMPTY) h->nIt = 0;
    for (int i = 0; i < h->nIt && i < 4; i++) {
        h->itPlant[i] = (h->arch == A_ELDER) ? chance(80)
                      : (h->arch == A_FAMILY) ? chance(25) : chance(45);
        h->itCol[i] = h->itPlant[i] ? (chance(60) ? CLR_BROWN : CLR_LIGHT_GREY)
            : (h->arch == A_FAMILY) ? CURT[rnd(NCURT)][0]                  // toys
            : (h->arch == A_STUDENT) ? (chance(50) ? CLR_BROWN : CLR_LIGHT_GREY) // bottles
            : CLR_LIGHT_GREY;                                              // vases
    }
    if (h->sill == SI_SYMM) {
        // mirrored around the window center — the deliberate household
        int pos2[2] = { 1, 6 }, pos3[3] = { 1, 3, 6 }, pos4[4] = { 0, 2, 5, 7 };
        int *p = h->nIt <= 2 ? pos2 : h->nIt == 3 ? pos3 : pos4;
        if (h->nIt == 1) h->itX[0] = 3;
        else for (int i = 0; i < h->nIt && i < 4; i++) h->itX[i] = p[i];
        // symmetry includes the items themselves: mirror plant/color
        for (int i = 0; i < h->nIt / 2; i++) {
            h->itPlant[h->nIt - 1 - i] = h->itPlant[i];
            h->itCol[h->nIt - 1 - i]   = h->itCol[i];
        }
    } else {
        for (int i = 0; i < h->nIt && i < 4; i++) h->itX[i] = rnd(8);
    }
}

static void roll_building(void) {
    NF = 7;
    BW = rnd_between(26, 30);
    NB = (SCREEN_W - 16 - TW) / BW;
    towerLeft = chance(50);

    int bldW = TW + NB * BW;
    int mx = (SCREEN_W - bldW) / 2;
    if (towerLeft) { towerX = mx; baysX = mx + TW; }
    else           { baysX = mx; towerX = mx + NB * BW; }

    baseY   = SCREEN_H - GROUND_H - PLINTH_H;
    wallTop = baseY - NF * FH;

    // concrete scheme
    {
        static const int WP[] = {
            CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_DARKER_GREY,
            CLR_INDIGO, CLR_DARKER_PURPLE, CLR_DARK_BROWN,
            CLR_DARK_GREEN, CLR_DARK_PEACH,
        };
        wallC = WP[rnd(8)];
    }
    slabC  = chance(60) ? CLR_LIGHT_GREY : CLR_WHITE;
    towerC = (wallC == CLR_INDIGO) ? CLR_DARKER_PURPLE : CLR_DARK_GREY;
    railStyle = chance(55) ? RAIL_BARS : RAIL_PANEL;
    {
        int pc[4] = { CLR_BLUE_GREEN, CLR_MAUVE, CLR_DARK_GREEN, CLR_TRUE_BLUE };
        panelC = pc[rnd(4)];
    }
    do { doorBase = CURT[rnd(NCURT)][1]; } while (doorBase == wallC);
    liftFloor = rnd(NF);
    nLamp = rnd_between(2, 4);
    for (int i = 0; i < nLamp; i++)
        lampX[i] = 20 + (SCREEN_W - 40) * (i * 2 + 1) / (nLamp * 2);

    for (int f = 0; f < NF; f++)
        for (int b = 0; b < NB; b++)
            roll_home(&homes[f][b]);
}

void init(void) { roll_building(); }

void update(void) { if (keyp(KEY_SPACE)) roll_building(); }

// ── drawing ──────────────────────────────────────────────────────────────────

// one kitchen window: glass + light + treatment shaping it + vensterbank
static void draw_window(Home *h, int f, int b, int wx, int wy) {
    // wy = top of the WW×WH glass
    int glass = h->lit ? (h->tv ? CLR_TRUE_BLUE : CLR_LIGHT_YELLOW)
                       : (h->arch == A_VACANT ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
    rectfill(wx, wy, WW, WH, glass);
    if (h->tv && h->lit && ((frame() / 3 + f * 13 + b * 7) & 7) < 3)
        rectfill(wx + 2 + ((frame() / 5 + b) & 3), wy + WH/2, 3, 2, CLR_BLUE);  // flicker

    switch (h->treat) {
    case TR_VITRAGE:
        fillp(0xA5A5, -1);
        rectfill(wx, wy, WW, WH, h->lit ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
        fillp_reset();
        break;
    case TR_CURTAIN:
        if (h->curtOpen) {
            rectfill(wx, wy, 2, WH, h->lit ? h->tBright : h->tDark);
            rectfill(wx + WW - 2, wy, 2, WH, h->lit ? h->tBright : h->tDark);
        } else {
            rectfill(wx, wy, WW, WH, h->lit ? h->tBright : h->tDark);
        }
        break;
    case TR_ROLLER: {
        int rh = (int)(h->roller * (WH - 1.0f)) + 1;
        rectfill(wx, wy, WW, rh, h->tDark);
        break; }
    case TR_VENETIAN:
        for (int yy = wy; yy < wy + WH; yy += 2)
            rectfill(wx, yy, WW, 1, CLR_DARK_GREY);
        break;
    }

    // vensterbank
    int closedOff = (h->treat == TR_CURTAIN && !h->curtOpen) ||
                    (h->treat == TR_ROLLER && h->roller > 0.75f);
    if (!closedOff)
        for (int i = 0; i < h->nIt && i < 4; i++) {
            int ix = wx + 1 + h->itX[i], iy = wy + WH - 1;
            int sil = h->lit;
            pset(ix, iy, sil ? CLR_BROWNISH_BLACK : h->itCol[i]);
            if (h->itPlant[i])
                pset(ix, iy - 1, sil ? CLR_BROWNISH_BLACK : CLR_DARK_GREEN);
        }
}

static void draw_band(int f) {
    int yb = baseY - f * FH;                       // band bottom
    // shadow cast by the slab above — dithered strip at band top
    rectfill(baysX, yb - FH, NB * BW, 1, CLR_DARKER_GREY);
    fillp(0x8888, -1);
    rectfill(baysX, yb - FH + 1, NB * BW, 2, CLR_DARKER_GREY);
    fillp_reset();
    // gallery slab — the signature horizontal
    rectfill(baysX, yb - SLAB_H, NB * BW, SLAB_H, slabC);

    for (int b = 0; b < NB; b++) {
        Home *h = &homes[f][b];
        int x = baysX + b * BW;
        // front door: SPANDREL gap at top, door height = FH - SLAB_H - SPANDREL
        rectfill(x + BAY_PAD, yb - FH + SPANDREL, DW, FH - SLAB_H - SPANDREL, h->doorCol);
        pset(x + BAY_PAD + DW - 2, yb - 9, CLR_BROWNISH_BLACK); // letter slot
        rectfill(x + BAY_PAD + 1, yb - FH + SPANDREL + 1, DW - 2, 3, h->arch == A_VACANT ? h->doorCol : CLR_DARKER_BLUE); // door glass (1px frame at top)
        // kitchen window
        draw_window(h, f, b, x + BAY_PAD + DW + WIN_GAP, yb - FH + SPANDREL);
    }

    // railing in front of everything
    if (railStyle == RAIL_BARS) {
        rectfill(baysX, yb - 7, NB * BW, 1, CLR_DARK_GREY);      // handrail
        for (int x = baysX; x < baysX + NB * BW; x += 3)
            rectfill(x, yb - 6, 1, 4, CLR_DARK_GREY);
    } else {
        rectfill(baysX, yb - 7, NB * BW, 1, CLR_LIGHT_GREY);     // handrail
        rectfill(baysX, yb - 6, NB * BW, 4, panelC);             // corrugated panel
        fillp(0xAAAA, -1);
        rectfill(baysX, yb - 6, NB * BW, 4, CLR_DARKER_GREY);    // corrugation shading
        fillp_reset();
    }

    // bikes lean on the railing, in front of it
    for (int b = 0; b < NB; b++)
        if (homes[f][b].bike) {
            int bx = baysX + b * BW + 14 + (b * 7) % 8;
            pset(bx, yb - 3, CLR_BROWNISH_BLACK); pset(bx + 4, yb - 3, CLR_BROWNISH_BLACK);
            line(bx, yb - 3, bx + 2, yb - 5, CLR_BROWNISH_BLACK);
            line(bx + 2, yb - 5, bx + 4, yb - 3, CLR_BROWNISH_BLACK);
            pset(bx + 2, yb - 6, CLR_BROWNISH_BLACK);
        }
}

static void draw_tower(void) {
    int top = wallTop - 6, bot = SCREEN_H - GROUND_H;
    rectfill(towerX, top, TW, bot - top, towerC);
    // glazed stairwell strip: the zigzag of flights
    int sx = towerX + 4;
    rectfill(sx, wallTop, 5, baseY - wallTop, CLR_DARKER_BLUE);
    for (int f = 0; f < NF; f++) {
        int yb = baseY - f * FH;
        if (f & 1) line(sx, yb - 4, sx + 4, yb - FH + 4, CLR_DARK_GREY);
        else       line(sx, yb - FH + 4, sx + 4, yb - 4, CLR_DARK_GREY);
    }
    // landing windows; the lit one is where the lift car waits
    for (int f = 0; f < NF; f++) {
        int yb = baseY - f * FH;
        rectfill(towerX + 12, yb - 10, 4, 4,
                 f == liftFloor ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE);
    }
    // machine room + antenna with its slow red light
    rectfill(towerX + 3, top - 9, TW - 6, 9, CLR_DARKER_GREY);
    line(towerX + TW - 7, top - 9, towerX + TW - 7, top - 17, CLR_DARK_GREY);
    pset(towerX + TW - 7, top - 18, blink(40) ? CLR_RED : CLR_DARK_RED);
}

static void draw_plinth(void) {
    rectfill(baysX, baseY, NB * BW, PLINTH_H, CLR_DARKER_GREY);
    // entrance beside the tower, lit from inside
    int ex = towerLeft ? baysX + 2 : baysX + NB * BW - 14;
    // bergingen: the row of little storage doors
    for (int x = baysX + 3; x < baysX + NB * BW - 8; x += 10) {
        if (x > ex - 8 && x < ex + 14) continue;                 // skip the entrance
        rectfill(x, baseY + 4, 6, 9, CLR_BROWNISH_BLACK);
        pset(x + 4, baseY + 6, CLR_DARK_GREY);                   // vent
    }
    rectfill(ex - 1, baseY + 2, 14, 1, slabC);                   // canopy
    rectfill(ex, baseY + 3, 12, 11, CLR_LIGHT_YELLOW);           // glass doors
    rectfill(ex + 5, baseY + 3, 1, 11, CLR_DARK_GREY);           // door split
    rect    (ex, baseY + 3, 12, 11, CLR_DARK_GREY);
}

void draw(void) {
    cls(0);
    int horizon = SCREEN_H - GROUND_H;
    gradient(0, 0, SCREEN_W, horizon, CLR_DARKER_BLUE, CLR_DARK_PEACH, 90);
    for (int i = 0; i < 26; i++)                                  // first stars
        pset((i * 73 + 19) % SCREEN_W, (i * 41 + 7) % wallTop, i % 4 ? CLR_INDIGO : CLR_LIGHT_GREY);

    // the slab: wall, then the bands of dwellings
    rectfill(baysX, wallTop, NB * BW, baseY - wallTop, wallC);
    // dithered shadow on facade next to the tower, fading away from it
    {
        int sx = towerLeft ? baysX : baysX + NB * BW - 4;
        int pat[4] = { 0x4444, 0x8888, 0xCCCC, 0xEEEE };
        for (int i = 0; i < 4; i++) {
            int x = towerLeft ? sx + i : sx + 3 - i;
            fillp(pat[i], -1);
            rectfill(x, wallTop, 1, baseY - wallTop, CLR_DARKER_GREY);
        }
        fillp_reset();
    }
    for (int f = 0; f < NF; f++) draw_band(f);
    rectfill(baysX - 1, wallTop - 3, NB * BW + 2, 3, slabC);      // roof edge
    draw_tower();
    draw_plinth();

    // ground: pavement + grass strip
    rectfill(0, horizon, SCREEN_W, 5, CLR_DARKER_GREY);
    rectfill(0, horizon + 5, SCREEN_W, GROUND_H - 5, CLR_DARK_GREEN);
    for (int i = 0; i < nLamp; i++) {                             // lampposts
        rectfill(lampX[i], horizon - 13, 1, 13, CLR_DARK_GREY);
        pset(lampX[i] + 1, horizon - 13, CLR_DARK_GREY);
        pset(lampX[i] + 2, horizon - 13, CLR_LIGHT_YELLOW);
        pset(lampX[i] + 2, horizon - 12, CLR_LIGHT_YELLOW);
    }

    font(FONT_TINY);
    print("GALERIJFLAT", 4, SCREEN_H - 6, CLR_MEDIUM_GREY);
    print_right("SPACE = re-roll", SCREEN_W - 3, SCREEN_H - 6, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
