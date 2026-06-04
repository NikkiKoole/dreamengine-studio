#include "studio.h"

// GALERIJFLAT — sys 6 starter: clock + light schedules + global facade tinting.
//
// Time-of-day tint: remap all 32 palette entries through t_mul[filter] before
// drawing. Sky + stars are already in the framebuffer before the remap fires so
// they're unaffected. Five "light-source" colours are restored after the global
// remap so they stay vivid against the darkened scene: LIGHT_YELLOW, LIGHT_PEACH,
// TRUE_BLUE, BLUE, RED. Everything else — concrete, doors, curtains, grass —
// picks up the ambient light.
//
//   SPACE   — re-roll the building
//   T       — jump forward 1 hour (for testing)
//   B       — toggle facade tint on/off (compare)

#define GROUND_H 12
#define PLINTH_H 14
#define FH       24
#define TW       20
#define WW       10
#define WH       10
#define DW        7
#define SLAB_H        2
#define SPANDREL      4
#define GALLERY_FLOOR 2   // visible walkway floor strip between door and railing
#define BAY_PAD       3
#define WIN_GAP   3
#define MAXF     12
#define MAXB     12

#define DAY_REAL_SECS  300.0f
#define TOD_START      20.0f
#define TOD_RATE       (24.0f / (DAY_REAL_SECS * 60.0f))

enum { A_VACANT, A_ELDER, A_COUPLE, A_FAMILY, A_STUDENT };
enum { TR_NONE, TR_VITRAGE, TR_CURTAIN, TR_ROLLER, TR_VENETIAN };
enum { SI_EMPTY, SI_SYMM, SI_RANDOM };

typedef struct {
    int   arch;
    int   treat;
    int   tBright, tDark;
    float roller;
    int   curtOpen;
    int   sill, nIt;
    int   itX[4], itPlant[4], itCol[4];
    int   doorCol;
    int   fillPat;             // fillp pattern for this household's treatment
    float wake_h, sleep_h;
} Home;

static int   NF, NB, BW;
static int   baysX, towerX, towerLeft;
static int   baseY, wallTop;
static int   wallC, slabC, towerC, panelC, doorBase;
static int   liftFloor, lampX[3], nLamp;
static Home  homes[MAXF][MAXB];
static float tod;

static const int CURT[][2] = {
    { CLR_RED,        CLR_DARK_RED    }, { CLR_ORANGE,     CLR_DARK_ORANGE },
    { CLR_GREEN,      CLR_DARK_GREEN  }, { CLR_BLUE,       CLR_TRUE_BLUE   },
    { CLR_PINK,       CLR_MAUVE       }, { CLR_LIGHT_PEACH, CLR_MEDIUM_GREY },
};
#define NCURT 6

// fillp pattern sets per treatment type
static const int VITRAGE_PATS[] = {
    0xA5A5,      // dense checker
    0x8020,      // sparse dots
    FILL_DIAG,   // 0x8421 diagonal weave
    FILL_GRID,   // 0xF888 open grid
    FILL_VLINES, // 0xAAAA vertical net
    0x5A5A,      // diagonal variant
    0xCCCC,      // wide vertical
    0x3333,      // offset wide vertical
    0x2222,      // very sparse vertical (ultra-fine lace)
    0x4444,      // very sparse vertical offset
    0x9669,      // diamond lattice
    0x6996,      // inverse diamond
    0xC3C3,      // thick diagonal
    0x6666,      // 2px vertical spaced 4px
    0x5555,      // offset 50% vertical
    0x0F0F,      // inverted hlines
};
#define N_VITRAGE_PATS 16

static const int CURTAIN_PATS[] = {
    0xFFFF,      // plain solid
    FILL_VLINES, // 0xAAAA vertical weave
    FILL_DIAG,   // 0x8421 diagonal
    0xCCCC,      // ribbed 2px
    FILL_GRID,   // 0xF888 grid weave
    0x3333,      // offset ribbed
    0x5555,      // offset vertical
    FILL_HLINES, // 0xF0F0 horizontal stripe
    0xA5A5,      // checker texture
    0xFF00,      // thick horizontal bands
};
#define N_CURTAIN_PATS 10

static const int VENETIAN_PATS[] = {
    FILL_HLINES, // 0xF0F0 fine 1px slats
    0xFF00,      // coarser 2px slats
};
#define N_VENETIAN_PATS 2

// door colours — broader than curtains; Dutch housing block palette
static const int DOOR_COLORS[] = {
    CLR_DARK_RED, CLR_DARK_ORANGE, CLR_DARK_GREEN, CLR_DARK_BROWN,
    CLR_TRUE_BLUE, CLR_MAUVE, CLR_BLUE_GREEN, CLR_DARKER_PURPLE,
    CLR_MEDIUM_GREY, CLR_BROWN, CLR_DARK_BLUE, CLR_INDIGO,
};
#define N_DOOR_COLORS 12

// ── tint — average blend table (from blendlab) ───────────────────────────────
// All 32 palette entries are remapped through t_avg[filter] each frame.
// Average-blend shifts every colour toward the filter regardless of brightness,
// so dark doors/concrete/grass visibly pick up the night/dusk cast (unlike
// multiply which barely moves already-dark colours). Light-source colours are
// restored after the loop so they stay vivid.

static const unsigned char PAL_RGB[32][3] = {
    {0x00,0x00,0x00},{0x1d,0x2b,0x53},{0x7e,0x25,0x53},{0x00,0x87,0x51},
    {0xab,0x52,0x36},{0x5f,0x57,0x4f},{0xc2,0xc3,0xc7},{0xff,0xf1,0xe8},
    {0xff,0x00,0x4d},{0xff,0xa3,0x00},{0xff,0xec,0x27},{0x00,0xe4,0x36},
    {0x29,0xad,0xff},{0x83,0x76,0x9c},{0xff,0x77,0xa8},{0xff,0xcc,0xaa},
    {0x29,0x18,0x14},{0x11,0x1d,0x35},{0x42,0x21,0x36},{0x12,0x53,0x59},
    {0x74,0x2f,0x29},{0x49,0x33,0x3b},{0xa2,0x88,0x79},{0xf3,0xef,0x7d},
    {0xbe,0x12,0x50},{0xff,0x6c,0x24},{0xa8,0xe7,0x2e},{0x00,0xb5,0x43},
    {0x06,0x5a,0xb5},{0x75,0x46,0x65},{0xff,0x6e,0x59},{0xff,0x9d,0x81},
};
static unsigned char t_mul[32][32];
static unsigned char t_avg[32][32];
static int tint_on = 1;

static int pal_nearest(int r, int g, int b) {
    int best = 0; long bd = 0x7fffffff;
    for (int i = 0; i < 32; i++) {
        int dr = r-(int)PAL_RGB[i][0], dg = g-(int)PAL_RGB[i][1], db = b-(int)PAL_RGB[i][2];
        long d = 2L*dr*dr + 4L*dg*dg + 3L*db*db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void build_tint_table(void) {
    for (int s = 0; s < 32; s++)
        for (int d = 0; d < 32; d++) {
            int sr=PAL_RGB[s][0], sg=PAL_RGB[s][1], sb=PAL_RGB[s][2];
            int dr=PAL_RGB[d][0], dg=PAL_RGB[d][1], db=PAL_RGB[d][2];
            t_mul[s][d] = pal_nearest(sr*dr/255, sg*dg/255, sb*db/255);
            t_avg[s][d] = pal_nearest((sr+dr)/2, (sg+dg)/2, (sb+db)/2);
        }
}

// colours that emit their own light — exempt from ambient tint
static const int LIGHT_COLORS[] = {
    CLR_LIGHT_YELLOW,   // warm window glow + lamp heads
    CLR_LIGHT_PEACH,    // lit vitrage (glow through net curtains)
    CLR_TRUE_BLUE,      // blue glass / TV background
    CLR_BLUE,           // TV flicker
    CLR_RED,            // antenna blink
};
#define N_LIGHT_COLORS 5

// all filters used by push_tint — keep in sync with the cases below
static const int TINT_FILTERS[]  = { CLR_DARK_BLUE, CLR_BROWN, CLR_DARKER_BLUE };
#define N_TINT_FILTERS 3

static void push_tint(float t) {
    if (!tint_on) return;
    int filter;
    if      (t <  7.0f || t >= 22.0f)       filter = CLR_DARK_BLUE;     // night
    else if (t <  9.0f)                      filter = CLR_BROWN;         // dawn
    else if (t >= 17.0f && t < 20.5f)        filter = CLR_BROWN;         // dusk
    else if (t >= 20.5f)                     filter = CLR_DARKER_BLUE;   // cooling evening
    else return;                                                           // 9-17: day, no tint
    for (int i = 0; i < 32; i++) pal(i, t_avg[filter][i]);
    // restore light-source colours — they don't reflect ambient, they emit
    for (int i = 0; i < N_LIGHT_COLORS; i++) pal(LIGHT_COLORS[i], LIGHT_COLORS[i]);
}

// true if colours a and b look identical at day OR under any tint filter
static int tint_clash(int a, int b) {
    if (a == b) return 1;
    for (int i = 0; i < N_TINT_FILTERS; i++)
        if (t_avg[TINT_FILTERS[i]][a] == t_avg[TINT_FILTERS[i]][b]) return 1;
    return 0;
}

// ── schedule helpers ──────────────────────────────────────────────────────────

static int is_dark(float t) { return (t < 7.5f || t >= 17.5f); }

static int home_lit(Home *h, float t) {
    if (h->arch == A_VACANT || !is_dark(t)) return 0;
    float s = h->sleep_h;
    if (s > 24.0f) return (t >= h->wake_h) || (t < s - 24.0f);
    return (t >= h->wake_h && t < s);
}

static int home_tv(Home *h, float t) {
    if (!home_lit(h, t)) return 0;
    switch (h->arch) {
    case A_ELDER:   return (t >= 15.0f && t < h->sleep_h);
    case A_COUPLE:  return (t >= 19.5f);
    case A_FAMILY:  return (t >= 17.5f && t < 21.5f);
    case A_STUDENT: return (t >= 18.0f || (h->sleep_h > 24.0f && t < h->sleep_h - 24.0f));
    default:        return 0;
    }
}

static int home_curt_open(Home *h, float t) {
    if (h->treat != TR_CURTAIN) return h->curtOpen;
    return (t < 6.0f || t > 22.0f) ? 0 : h->curtOpen;
}

// ── rolling ───────────────────────────────────────────────────────────────────

static void roll_home(Home *h) {
    *h = (Home){0};
    int r = rnd(100);
    h->arch = r < 8 ? A_VACANT : r < 30 ? A_ELDER : r < 55 ? A_COUPLE
            : r < 82 ? A_FAMILY : A_STUDENT;
    int c = rnd(NCURT);
    h->tBright = CURT[c][0]; h->tDark = CURT[c][1];
    h->doorCol = doorBase;

    switch (h->arch) {
    case A_VACANT:
        h->treat = TR_NONE; h->sill = SI_EMPTY;
        h->wake_h = 0; h->sleep_h = 0;
        break;
    case A_ELDER:
        h->treat = chance(60) ? TR_VITRAGE : chance(70) ? TR_CURTAIN : TR_VENETIAN;
        h->curtOpen = 1;
        h->sill = chance(82) ? SI_SYMM : SI_RANDOM;
        h->nIt  = 2 + rnd(3);

        h->wake_h  = rnd_float_between(5.5f, 7.0f);
        h->sleep_h = rnd_float_between(21.0f, 22.5f);
        break;
    case A_COUPLE:
        h->treat = chance(40) ? TR_VENETIAN : chance(50) ? TR_CURTAIN
                 : chance(50) ? TR_VITRAGE : TR_NONE;
        h->curtOpen = chance(60);
        h->sill = chance(55) ? SI_SYMM : SI_EMPTY;
        h->nIt  = 1 + rnd(2);

        h->wake_h  = rnd_float_between(6.5f, 8.0f);
        h->sleep_h = rnd_float_between(22.5f, 24.0f);
        break;
    case A_FAMILY:
        h->treat = chance(50) ? TR_CURTAIN : chance(60) ? TR_ROLLER : TR_VITRAGE;
        h->curtOpen = chance(35);
        h->roller = rnd_float_between(0.25f, 0.6f);
        h->sill = chance(70) ? SI_RANDOM : SI_EMPTY;
        h->nIt  = 2 + rnd(3);

        h->wake_h  = rnd_float_between(6.0f, 7.5f);
        h->sleep_h = rnd_float_between(21.5f, 23.0f);
        break;
    case A_STUDENT:
        h->treat = chance(50) ? TR_NONE : chance(70) ? TR_ROLLER : TR_VENETIAN;
        h->roller = rnd_float_between(0.3f, 0.7f);
        h->sill = chance(50) ? SI_EMPTY : SI_RANDOM;
        h->nIt  = 1 + rnd(2);

        h->wake_h  = rnd_float_between(9.0f, 12.0f);
        h->sleep_h = rnd_float_between(24.0f, 27.0f);
        break;
    }

    switch (h->treat) {
    case TR_VITRAGE:  h->fillPat = VITRAGE_PATS[rnd(N_VITRAGE_PATS)];   break;
    case TR_CURTAIN:  h->fillPat = CURTAIN_PATS[rnd(N_CURTAIN_PATS)];   break;
    case TR_VENETIAN: h->fillPat = VENETIAN_PATS[rnd(N_VENETIAN_PATS)]; break;
    default:          h->fillPat = 0; break;
    }

    if (h->sill == SI_EMPTY) h->nIt = 0;
    for (int i = 0; i < h->nIt && i < 4; i++) {
        h->itPlant[i] = (h->arch == A_ELDER) ? chance(80)
                      : (h->arch == A_FAMILY) ? chance(25) : chance(45);
        h->itCol[i] = h->itPlant[i] ? (chance(60) ? CLR_BROWN : CLR_LIGHT_GREY)
            : (h->arch == A_FAMILY) ? CURT[rnd(NCURT)][0]
            : (h->arch == A_STUDENT) ? (chance(50) ? CLR_BROWN : CLR_LIGHT_GREY)
            : CLR_LIGHT_GREY;
    }
    if (h->sill == SI_SYMM) {
        int pos2[2] = { 1, 6 }, pos3[3] = { 1, 3, 6 }, pos4[4] = { 0, 2, 5, 7 };
        int *p = h->nIt <= 2 ? pos2 : h->nIt == 3 ? pos3 : pos4;
        if (h->nIt == 1) h->itX[0] = 3;
        else for (int i = 0; i < h->nIt && i < 4; i++) h->itX[i] = p[i];
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
    // panel must stay distinct from wall
    { static const int pc[4] = { CLR_BLUE_GREEN, CLR_MAUVE, CLR_DARK_GREEN, CLR_TRUE_BLUE };
      int tries = 0;
      do { panelC = pc[rnd(4)]; tries++; }
      while (tries < 30 && tint_clash(panelC, wallC)); }
    // door must stay distinct from wall AND panel
    { int tries = 0;
      do { doorBase = DOOR_COLORS[rnd(N_DOOR_COLORS)]; tries++; }
      while (tries < 30 && (tint_clash(doorBase, wallC) || tint_clash(doorBase, panelC))); }
    liftFloor = rnd(NF);
    nLamp = rnd_between(2, 4);
    for (int i = 0; i < nLamp; i++)
        lampX[i] = 20 + (SCREEN_W - 40) * (i * 2 + 1) / (nLamp * 2);

    for (int f = 0; f < NF; f++)
        for (int b = 0; b < NB; b++)
            roll_home(&homes[f][b]);
}

void init(void) {
    build_tint_table();
    tod = TOD_START;
    roll_building();
}

void update(void) {
    tod += TOD_RATE;
    if (tod >= 24.0f) tod -= 24.0f;
    if (keyp(KEY_SPACE)) roll_building();
    if (keyp('T')) { tod += 1.0f; if (tod >= 24.0f) tod -= 24.0f; }
    if (keyp('B')) tint_on = !tint_on;
}

// ── drawing ───────────────────────────────────────────────────────────────────

static void draw_window(Home *h, int f, int b, int wx, int wy) {
    int lit  = home_lit(h, tod);
    int tv   = home_tv(h, tod);
    int curt = home_curt_open(h, tod);

    int glass = lit ? (tv ? CLR_TRUE_BLUE : CLR_LIGHT_YELLOW)
                    : (h->arch == A_VACANT ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
    rectfill(wx, wy, WW, WH, glass);
    if (tv && lit && ((frame() / 3 + f * 13 + b * 7) & 7) < 3)
        rectfill(wx + 2 + ((frame() / 5 + b) & 3), wy + WH/2, 3, 2, CLR_BLUE);

    switch (h->treat) {
    case TR_VITRAGE:
        fillp(h->fillPat, -1);
        rectfill(wx, wy, WW, WH, lit ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
        fillp_reset();
        break;
    case TR_CURTAIN:
        if (h->fillPat != 0xFFFF) fillp(h->fillPat, -1);
        if (curt) {
            rectfill(wx, wy, 2, WH, lit ? h->tBright : h->tDark);
            rectfill(wx + WW - 2, wy, 2, WH, lit ? h->tBright : h->tDark);
        } else {
            rectfill(wx, wy, WW, WH, lit ? h->tBright : h->tDark);
        }
        if (h->fillPat != 0xFFFF) fillp_reset();
        break;
    case TR_ROLLER: {
        int rh = (int)(h->roller * (WH - 1.0f)) + 1;
        rectfill(wx, wy, WW, rh, h->tDark);
        break; }
    case TR_VENETIAN:
        fillp(h->fillPat, -1);
        rectfill(wx, wy, WW, WH, CLR_DARK_GREY);
        fillp_reset();
        break;
    }

    // windowsill ledge — structural concrete, same material as the gallery slabs
    rectfill(wx, wy + WH - 1, WW, 1, slabC);

    int closedOff = (h->treat == TR_CURTAIN && !curt) ||
                    (h->treat == TR_ROLLER && h->roller > 0.75f);
    if (!closedOff)
        for (int i = 0; i < h->nIt && i < 4; i++) {
            int ix = wx + 1 + h->itX[i], iy = wy + WH - 1;
            pset(ix, iy, lit ? CLR_BROWNISH_BLACK : h->itCol[i]);
            if (h->itPlant[i])
                pset(ix, iy - 1, lit ? CLR_BROWNISH_BLACK : CLR_DARK_GREEN);
        }
}

static void draw_band(int f) {
    int yb = baseY - f * FH;
    rectfill(baysX, yb - FH, NB * BW, 1, CLR_DARKER_GREY);
    fillp(0x8888, -1);
    rectfill(baysX, yb - FH + 1, NB * BW, 2, CLR_DARKER_GREY);
    fillp_reset();
    rectfill(baysX, yb - SLAB_H, NB * BW, SLAB_H, slabC);

    for (int b = 0; b < NB; b++) {
        Home *h = &homes[f][b];
        int x = baysX + b * BW;
        rectfill(x + BAY_PAD, yb - FH + SPANDREL, DW, FH - SLAB_H - SPANDREL - GALLERY_FLOOR, h->doorCol);
        pset(x + BAY_PAD + DW - 2, yb - 12, CLR_BROWNISH_BLACK);
        rectfill(x + BAY_PAD + 1, yb - FH + SPANDREL + 1, DW - 2, 3,
                 h->arch == A_VACANT ? h->doorCol : CLR_DARKER_BLUE);
        draw_window(h, f, b, x + BAY_PAD + DW + WIN_GAP, yb - FH + SPANDREL);
    }

    // gallery walkway floor — visible through bar gaps, hidden by panel
    rectfill(baysX, yb - SLAB_H - GALLERY_FLOOR, NB * BW, GALLERY_FLOOR, slabC);

    // railing — handrail cap in slabC, bars in panelC (clash-guarded against wallC)
    rectfill(baysX, yb - 9, NB * BW, 1, slabC);
    for (int x = baysX; x < baysX + NB * BW; x += 3)
        rectfill(x, yb - 8, 1, 6, panelC);

}

static void draw_tower(void) {
    int top = wallTop - 6, bot = SCREEN_H - GROUND_H;
    rectfill(towerX, top, TW, bot - top, towerC);
    int sx = towerX + 4;
    rectfill(sx, wallTop, 5, baseY - wallTop, CLR_DARKER_BLUE);
    for (int f = 0; f < NF; f++) {
        int yb = baseY - f * FH;
        if (f & 1) line(sx, yb - 4, sx + 4, yb - FH + 4, CLR_DARK_GREY);
        else       line(sx, yb - FH + 4, sx + 4, yb - 4, CLR_DARK_GREY);
    }
    for (int f = 0; f < NF; f++) {
        int yb = baseY - f * FH;
        rectfill(towerX + 12, yb - 10, 4, 4,
                 f == liftFloor ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE);
    }
    rectfill(towerX + 3, top - 9, TW - 6, 9, CLR_DARKER_GREY);
    line(towerX + TW - 7, top - 9, towerX + TW - 7, top - 17, CLR_DARK_GREY);
    pset(towerX + TW - 7, top - 18, blink(40) ? CLR_RED : CLR_DARK_RED);
}

static void draw_plinth(void) {
    rectfill(baysX, baseY, NB * BW, PLINTH_H, CLR_DARKER_GREY);
    int ex = towerLeft ? baysX + 2 : baysX + NB * BW - 14;
    for (int x = baysX + 3; x < baysX + NB * BW - 8; x += 10) {
        if (x > ex - 8 && x < ex + 14) continue;
        rectfill(x, baseY + 4, 6, 9, CLR_BROWNISH_BLACK);
        pset(x + 4, baseY + 6, CLR_DARK_GREY);
    }
    rectfill(ex - 1, baseY + 2, 14, 1, slabC);
    rectfill(ex, baseY + 3, 12, 11, CLR_LIGHT_YELLOW);
    rectfill(ex + 5, baseY + 3, 1, 11, CLR_DARK_GREY);
    rect    (ex, baseY + 3, 12, 11, CLR_DARK_GREY);
}

static void draw_sky(float t) {
    int horizon = SCREEN_H - GROUND_H;
    int top, bot;
    if      (t <  5.5f)  { top = CLR_BLACK;       bot = CLR_DARKER_BLUE;   }
    else if (t <  7.5f)  { top = CLR_DARKER_BLUE;  bot = CLR_DARK_ORANGE;  }
    else if (t < 18.0f)  { top = CLR_BLUE;         bot = CLR_TRUE_BLUE;    }
    else if (t < 20.5f)  { top = CLR_DARKER_BLUE;  bot = CLR_DARK_PEACH;   }
    else if (t < 22.5f)  { top = CLR_DARK_BLUE;    bot = CLR_DARKER_PURPLE;}
    else                 { top = CLR_BLACK;         bot = CLR_DARKER_BLUE;  }
    gradient(0, 0, SCREEN_W, horizon, top, bot, 90);
}

void draw(void) {
    cls(0);
    int horizon = SCREEN_H - GROUND_H;

    // sky + stars: in the framebuffer BEFORE any pal() swap, so always unaffected
    draw_sky(tod);
    if (tod < 7.5f || tod > 19.5f)
        for (int i = 0; i < 26; i++)
            pset((i * 73 + 19) % SCREEN_W, (i * 41 + 7) % wallTop,
                 i % 4 ? CLR_INDIGO : CLR_LIGHT_GREY);

    // global tint: all 32 colours remapped, light-source colours restored
    push_tint(tod);

    // building
    rectfill(baysX, wallTop, NB * BW, baseY - wallTop, wallC);
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
    rectfill(baysX - 1, wallTop - 3, NB * BW + 2, 3, slabC);
    draw_tower();
    draw_plinth();

    // ground (also tinted — pavement + grass pick up the ambient light)
    rectfill(0, horizon, SCREEN_W, 5, CLR_DARKER_GREY);
    rectfill(0, horizon + 5, SCREEN_W, GROUND_H - 5, CLR_DARK_GREEN);
    for (int i = 0; i < nLamp; i++) {
        rectfill(lampX[i], horizon - 13, 1, 13, CLR_DARK_GREY);
        pset(lampX[i] + 1, horizon - 13, CLR_DARK_GREY);
        pset(lampX[i] + 2, horizon - 13, CLR_LIGHT_YELLOW);   // lamp head: exempt, stays warm
        pset(lampX[i] + 2, horizon - 12, CLR_LIGHT_YELLOW);
    }

    pal_reset();  // HUD always in real colours

    {
        int h24 = (int)tod % 24, m = (int)((tod - (int)tod) * 60.0f);
        char buf[6] = { '0'+h24/10, '0'+h24%10, ':', '0'+m/10, '0'+m%10, 0 };
        font(FONT_TINY);
        print(buf, 4, SCREEN_H - 6, CLR_DARK_GREY);
        print("GALERIJFLAT", 28, SCREEN_H - 6, CLR_MEDIUM_GREY);
        print_right(tint_on ? "B=tint:ON  T=+1h  SPACE=re-roll"
                             : "B=tint:OFF T=+1h  SPACE=re-roll",
                    SCREEN_W - 3, SCREEN_H - 6,
                    tint_on ? CLR_DARK_GREY : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }
}
