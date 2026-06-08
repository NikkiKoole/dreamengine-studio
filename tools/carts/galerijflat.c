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
#define MAXW     60   // gallery walkers + lift queues (the building's "alive" signal)

#define DAY_REAL_SECS  300.0f
#define TOD_START      20.0f
#define TOD_RATE       (24.0f / (DAY_REAL_SECS * 60.0f))

enum { A_VACANT, A_ELDER, A_COUPLE, A_FAMILY, A_STUDENT };
enum { TR_NONE, TR_VITRAGE, TR_CURTAIN, TR_ROLLER, TR_VENETIAN };
enum { SI_EMPTY, SI_SYMM, SI_RANDOM };
// walker lifecycle around the lift (GROUND is the street-level lobby):
//  leaving:  door → WK_TO_LIFT (walk to tower) → WK_WAIT (queue on home floor)
//            → WK_RIDING (dest GROUND) → WK_EXIT (walk the pavement out) → gone
//  arriving: WK_ENTER (walk in from the street) → WK_WAIT (queue at GROUND,
//            dest = home floor) → WK_RIDING → WK_FROM_LIFT (tower→door) → home
enum { WK_ENTER, WK_TO_LIFT, WK_WAIT, WK_RIDING, WK_FROM_LIFT, WK_EXIT };

// lift car state machine (sys 7)
enum { LIFT_IDLE, LIFT_MOVING, LIFT_DOORS };
#define LIFT_CAP   3       // people the cab holds
#define DOOR_HOLD  30      // frames doors stay open
#define GROUND    (-1)     // the lift's bottom stop: the street-level lobby
#define LOBBY_DROP 14      // px the cab descends below floor 0 — a full plinth, to street level
#define NO_DEPART (-100)   // liftDepart sentinel (GROUND is a real floor now)

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
    int   occ;                 // presence override: -1 follow schedule, 0 left (dark), 1 home (lit)
} Home;

typedef struct {
    int   active;
    int   state;               // WK_ENTER/TO_LIFT/WAIT/RIDING/FROM_LIFT/EXIT
    int   floor;               // band the walker is physically on (gallery / GROUND)
    int   home_floor;          // their dwelling floor
    int   dest;                // floor they want the lift to carry them to
    int   bay;                 // which door on home_floor
    int   q;                   // join order — fixes their place in the queue
    float x, tx;               // position + fixed target (door / street edge)
    float vx;                  // current signed velocity (0 = standing)
    float spd;                 // walk speed magnitude
    int   pause;               // frames fumbling at the door before entering
    int   skin, hair, shirt, pants;
} Walker;

static int   NF, NB, BW;
static int   baysX, towerX, towerLeft;
static int   baseY, wallTop;
static int   wallC, slabC, towerC, panelC, doorBase;
static int   lampX[3], nLamp;
static float liftCarY;     // eased pixel y of the lift car's floor line
static int   carFloor;     // floor the car is at / nearest to
static int   liftTarget;   // floor it's travelling toward
static int   liftDir;      // -1 down, +1 up, 0 idle
static int   liftState;    // LIFT_IDLE / LIFT_MOVING / LIFT_DOORS
static int   liftDepart;   // floor just left (don't re-stop there on the way out)
static int   liftClosing;  // in the DOORS phase: 0 = opening/holding, 1 = closing
static int   liftDoorTimer;
static float liftDoor;     // 0 = shut, 1 = fully open (eased)
static Home  homes[MAXF][MAXB];
static float tod;
static Walker walkers[MAXW];
static int    spawn_cooldown;
static int    joinSeq;           // monotonic queue join counter
static float  floor_y(int fl);   // cab/floor-line y for a lift floor (GROUND-aware)
#ifdef DE_TRACE
static int    dbg_lit = -1;   // last home (floor*100+bay) a walker lit by arriving
static int    dbg_open = 0;   // cumulative door-open events
static int    dbg_serve = 0;  // cumulative boards + alights
#endif

static const int CURT[][2] = {
    { CLR_RED,          CLR_DARK_RED      },
    { CLR_ORANGE,       CLR_DARK_ORANGE   },
    { CLR_GREEN,        CLR_DARK_GREEN    },
    { CLR_BLUE,         CLR_TRUE_BLUE     },
    { CLR_PINK,         CLR_MAUVE         },
    { CLR_LIGHT_PEACH,  CLR_MEDIUM_GREY   },
    { CLR_YELLOW,       CLR_DARK_ORANGE   },
    { CLR_LIME_GREEN,   CLR_DARK_GREEN    },
    { CLR_PEACH,        CLR_DARK_PEACH    },
    { CLR_LIGHT_YELLOW, CLR_BROWN         },
    { CLR_WHITE,        CLR_LIGHT_GREY    },
    { CLR_INDIGO,       CLR_DARKER_PURPLE },
    { CLR_MEDIUM_GREEN, CLR_BLUE_GREEN    },
    { CLR_TRUE_BLUE,    CLR_DARKER_BLUE   },
};
#define NCURT 14

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

// walker clothing — non-light-source colours so they pick up the ambient tint
static const int WALK_SHIRTS[] = {
    CLR_DARK_RED, CLR_ORANGE, CLR_GREEN, CLR_DARK_GREEN, CLR_INDIGO,
    CLR_MAUVE, CLR_BLUE_GREEN, CLR_WHITE, CLR_DARK_BROWN, CLR_PINK,
};
#define N_WALK_SHIRTS 10
static const int WALK_HAIR[] = {
    CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN, CLR_LIGHT_GREY,
};
#define N_WALK_HAIR 4

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
    if (h->occ == 1) return 1;        // saw them come home → lit (overrides schedule)
    if (h->occ == 0) return 0;        // saw them leave → dark
    float s = h->sleep_h;             // occ == -1: follow the wake/sleep schedule
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
    h->occ = -1;                 // follow the schedule until a walker proves otherwise
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
    nLamp = rnd_between(2, 4);
    for (int i = 0; i < nLamp; i++)
        lampX[i] = 20 + (SCREEN_W - 40) * (i * 2 + 1) / (nLamp * 2);

    for (int f = 0; f < NF; f++)
        for (int b = 0; b < NB; b++)
            roll_home(&homes[f][b]);

    for (int i = 0; i < MAXW; i++) walkers[i].active = 0;
    spawn_cooldown = 0; joinSeq = 0;
    carFloor = rnd(NF); liftTarget = carFloor; liftDir = 0;
    liftState = LIFT_IDLE; liftDepart = NO_DEPART; liftClosing = 0; liftDoorTimer = 0;
    liftDoor = 0.0f;
    liftCarY = floor_y(carFloor);   // car starts parked at a floor
}

// ── gallery walkers ─────────────────────────────────────────────────────────
// Self-contained ambient agents (the full elevator/routine sim is a later
// step): a walker steps onto a band at the lift-tower end and walks to a door
// (ARRIVE), or leaves a door and walks to the tower (LEAVE). Speed is a calm
// stroll; the tower's lift indicator follows wherever a walker just appeared
// or vanished, so the climbing light finally *means* someone is coming/going.

// x where the gallery meets the lift tower (walkers enter/leave the world here)
static int tower_edge_x(void) {
    return towerLeft ? baysX + 1 : baysX + NB * BW - 2;
}
// standing spot in front of bay b's front door
static int bay_door_x(int b) {
    return baysX + b * BW + BAY_PAD + DW / 2;
}
static float walk_speed(void) { return rnd_float_between(0.28f, 0.45f); }

// cab floor-line y for a lift floor — floors sit on the FH grid; GROUND drops
// the cab into the plinth lobby below floor 0
static float floor_y(int fl) { return (fl < 0) ? (float)(baseY + LOBBY_DROP) : (float)(baseY - fl * FH); }
// feet line for someone on the pavement — aligned with the cab's ground floor
// so a passenger steps straight out of the lift onto the street (no hop)
static int   ground_feet(void) { return (int)floor_y(GROUND); }
// the lobby door at the tower base, and the screen edge the pavement runs to
static int   lobby_x(void) { return towerX + 14; }
static int   exit_x(void)  { return towerLeft ? SCREEN_W + 6 : -6; }

// x of the k-th person in the queue, backed away from the lift door
static int wait_x(int fl, int k) {
    int away = towerLeft ? 1 : -1;
    if (fl == GROUND) return lobby_x()      + away * (4 + k * 4);   // on the pavement
    return                   tower_edge_x() + away * (2 + k * 4);   // on the gallery
}
// a walker's place in its floor's queue = how many joined ahead of it. Recomputed
// each frame, so when the front boards everyone behind shuffles forward a slot.
static int queue_rank(Walker *w) {
    int r = 0;
    for (int i = 0; i < MAXW; i++) {
        Walker *o = &walkers[i];
        if (!o->active || o == w || o->floor != w->floor) continue;
        if (o->state != WK_WAIT && o->state != WK_TO_LIFT && o->state != WK_ENTER) continue;
        if (o->q < w->q) r++;
    }
    return r;
}
// step toward tgt at the walker's pace; sets vx (for facing) and returns 1 when there
static int walk_toward(Walker *w, float tgt) {
    float dx = tgt - w->x, a = dx < 0 ? -dx : dx;
    if (a <= w->spd) { w->x = tgt; w->vx = 0.0f; return 1; }
    w->vx = (dx > 0) ? w->spd : -w->spd;
    w->x += w->vx;
    return 0;
}

static void walker_dress(Walker *w) {
    w->skin = chance(70) ? CLR_PEACH : CLR_DARK_PEACH;
    w->hair = WALK_HAIR[rnd(N_WALK_HAIR)];
    // shirt is the big mass above the rail — keep it distinct from wall, door
    // and railing (under day light and every tint) so it never melts in
    int tries = 0;
    do { w->shirt = WALK_SHIRTS[rnd(N_WALK_SHIRTS)]; tries++; }
    while (tries < 12 && (tint_clash(w->shirt, wallC) ||
                          tint_clash(w->shirt, doorBase) ||
                          tint_clash(w->shirt, panelC)));
    w->pants = chance(50) ? CLR_DARK_BLUE
             : chance(50) ? CLR_DARKER_GREY : CLR_DARK_BROWN;
}

static void spawn_walker(void) {
    int slot = -1;
    for (int i = 0; i < MAXW; i++) if (!walkers[i].active) { slot = i; break; }
    if (slot < 0) return;

    int f = rnd(NF);
    int b = rnd(NB);                                   // prefer an occupied dwelling
    for (int tries = 0; tries < 6 && homes[f][b].arch == A_VACANT; tries++)
        b = rnd(NB);

    // arrive vs leave biased by time of day: morning down-rush, evening up-rush
    int arrive;
    if      (tod >= 6.5f && tod <  9.0f)  arrive = chance(25);
    else if (tod >= 16.5f && tod < 19.5f) arrive = chance(75);
    else                                  arrive = chance(50);

    Walker *w = &walkers[slot];
    *w = (Walker){0};
    w->active = 1;
    w->home_floor = f;
    w->bay = b;
    w->q = joinSeq++;          // claim a place in line up front
    w->spd = walk_speed();
    walker_dress(w);

    if (arrive) {
        // walks in off the street straight to the back of the lobby queue, rides up
        w->state = WK_ENTER; w->floor = GROUND; w->dest = f;
        w->x = exit_x();
    } else {
        // leaving: walk the gallery straight to the back of the queue, ride down
        w->state = WK_TO_LIFT; w->floor = f; w->dest = GROUND;
        w->x = bay_door_x(b);
        homes[f][b].occ = 0;   // they've stepped out — the window goes dark
    }
}

static void update_walkers(void) {
    if (spawn_cooldown > 0) {
        spawn_cooldown--;
    } else {
        int rate;   // per-frame 1/rate spawn chance — lower = busier. Rush now out-paces
                    // the single 3-seat lift on purpose, so the queues visibly build.
        if      (tod < 6.0f || tod >= 23.0f)  rate = 220;   // night: still quiet
        else if (tod >= 6.5f && tod <  9.0f)  rate = 32;    // morning rush
        else if (tod >= 16.5f && tod < 19.5f) rate = 30;    // evening rush
        else                                  rate = 60;    // daytime trickle
        if (rnd(rate) == 0) { spawn_walker(); spawn_cooldown = rnd_between(6, 16); }
    }

    for (int i = 0; i < MAXW; i++) {
        Walker *w = &walkers[i];
        if (!w->active) continue;

        switch (w->state) {
        case WK_ENTER:                         // in off the street → back of the lobby queue
            if (walk_toward(w, wait_x(GROUND, queue_rank(w)))) w->state = WK_WAIT;
            break;
        case WK_TO_LIFT:                       // along the gallery → back of the queue
            if (walk_toward(w, wait_x(w->floor, queue_rank(w)))) w->state = WK_WAIT;
            break;
        case WK_WAIT:                          // in line — shuffle up as those ahead board
            walk_toward(w, wait_x(w->floor, queue_rank(w)));
            break;
        case WK_FROM_LIFT:                     // out of the lift → walk to the front door
            if (w->pause > 0) {
                if (--w->pause == 0) {
                    homes[w->home_floor][w->bay].occ = 1;   // inside now — the window lights up
#ifdef DE_TRACE
                    dbg_lit = w->home_floor * 100 + w->bay;
#endif
                    w->active = 0;
                }
                break;
            }
            if (walk_toward(w, w->tx)) w->pause = rnd_between(20, 45);   // fumble keys, then inside
            break;
        case WK_EXIT:                          // out the lobby → walk the pavement off-screen
            if (walk_toward(w, w->tx)) w->active = 0;
            break;
        case WK_RIDING:                        // inside the cab — moved by the lift
            break;
        }
    }
}

// ── the lift, for real (sys 7) ──────────────────────────────────────────────
// A directional LOOK car with up/down hall calls. A waiter presses up OR down;
// a car sweeping up only stops for up-waiters (and riders alighting), and picks
// the down-waiters up on the way back. It still *travels* to the furthest floor
// with anyone waiting (the turning point), then flips and serves them on the
// return. GROUND (below floor 0) is the street lobby — waiters there always want
// up; leavers ride down to it and walk out.

static int count_riders(void) {
    int n = 0;
    for (int i = 0; i < MAXW; i++)
        if (walkers[i].active && walkers[i].state == WK_RIDING) n++;
    return n;
}
// the direction a waiting walker pressed (their dest is always up or down of them)
static int want_dir(Walker *w) { return (w->dest > w->floor) ? 1 : -1; }

static int rider_alights(int fl) {       // is a rider getting off at fl?
    for (int i = 0; i < MAXW; i++)
        if (walkers[i].active && walkers[i].state == WK_RIDING && walkers[i].dest == fl) return 1;
    return 0;
}
static int waiter_going(int fl, int dir) {   // a waiter at fl who pressed `dir`
    for (int i = 0; i < MAXW; i++) {
        Walker *w = &walkers[i];
        if (w->active && w->state == WK_WAIT && w->floor == fl && want_dir(w) == dir) return 1;
    }
    return 0;
}
// open the doors here while travelling `dir`: riders off, or same-direction
// boarders we have room for (a full car only stops to let people OFF, never to
// pointlessly open for a waiter it can't take)
static int stop_here(int fl, int dir) {
    if (rider_alights(fl)) return 1;
    return waiter_going(fl, dir) && count_riders() < LIFT_CAP;
}
// any reason to ever visit fl (used to find the turning point — direction-agnostic)
static int any_call_here(int fl) {
    for (int i = 0; i < MAXW; i++) {
        Walker *w = &walkers[i];
        if (!w->active) continue;
        if (w->state == WK_RIDING && w->dest  == fl) return 1;
        if (w->state == WK_WAIT   && w->floor == fl) return 1;
    }
    return 0;
}
static int furthest_call(int dir) {      // furthest floor in dir with anyone to serve (turning point)
    int best = GROUND - 1;
    if (dir > 0) { for (int fl = carFloor + 1; fl < NF; fl++)      if (any_call_here(fl)) best = fl; }
    else         { for (int fl = carFloor - 1; fl >= GROUND; fl--) if (any_call_here(fl)) best = fl; }
    return best;
}
static int nearest_call(int dir) {
    if (dir > 0) { for (int fl = carFloor + 1; fl < NF; fl++)      if (any_call_here(fl)) return fl; }
    else         { for (int fl = carFloor - 1; fl >= GROUND; fl--) if (any_call_here(fl)) return fl; }
    return GROUND - 1;
}

// at carFloor with the doors open: drop riders, then board waiters going liftDir
static void lift_service(void) {
    for (int i = 0; i < MAXW; i++) {
        Walker *w = &walkers[i];
        if (w->active && w->state == WK_RIDING && w->dest == carFloor) {
            if (w->dest == w->home_floor) {            // home — step out and walk to the door
                w->state = WK_FROM_LIFT; w->floor = carFloor;
                w->x = tower_edge_x(); w->tx = bay_door_x(w->bay);
            } else {                                    // reached the ground lobby
                w->state = WK_EXIT; w->floor = GROUND;   // step out, walk off across the pavement
                w->x = lobby_x(); w->tx = exit_x();
            }
#ifdef DE_TRACE
            dbg_serve++;
#endif
        }
    }
    for (int i = 0; i < MAXW; i++) {                    // only those heading the car's way
        Walker *w = &walkers[i];
        if (w->active && w->state == WK_WAIT && w->floor == carFloor &&
            want_dir(w) == liftDir && count_riders() < LIFT_CAP) {
            w->state = WK_RIDING;
#ifdef DE_TRACE
            dbg_serve++;
#endif
        }
    }
}

static void lift_open(void) {
    liftCarY = floor_y(carFloor);
    liftState = LIFT_DOORS; liftClosing = 0; liftDoorTimer = DOOR_HOLD;
#ifdef DE_TRACE
    dbg_open++;
#endif
}

// decide what the idle/just-closed car does next: serve here, travel, or sleep
static void lift_dispatch(void) {
    // 1. is there someone to serve right here? choose the direction we'll leave in
    if (any_call_here(carFloor)) {
        int d = liftDir;
        if (d == 0 || furthest_call(d) < GROUND)        // no continuation that way → (re)choose
            d = (waiter_going(carFloor, 1) || furthest_call(1) >= GROUND) ? 1
              : (waiter_going(carFloor, -1) || furthest_call(-1) >= GROUND) ? -1
              : (liftDir != 0 ? liftDir : 1);
        if (stop_here(carFloor, d)) { liftDir = d; lift_open(); return; }
    }
    // 2. travel toward calls — keep direction, else reverse, else (idle) pick the nearer
    int d0, d1;
    if (liftDir > 0)      { d0 = 1;  d1 = -1; }
    else if (liftDir < 0) { d0 = -1; d1 = 1;  }
    else {
        int up = nearest_call(1), dn = nearest_call(-1);
        d0 = (up >= GROUND && (dn < GROUND || (up - carFloor) <= (carFloor - dn))) ? 1 : -1;
        d1 = -d0;
    }
    int t = furthest_call(d0);
    if (t >= GROUND) { liftDir = d0; liftTarget = t; liftDepart = carFloor; liftState = LIFT_MOVING; return; }
    t = furthest_call(d1);
    if (t >= GROUND) { liftDir = d1; liftTarget = t; liftDepart = carFloor; liftState = LIFT_MOVING; return; }
    liftDir = 0; liftState = LIFT_IDLE;
}

static void update_lift(void) {
    switch (liftState) {
    case LIFT_IDLE:
        liftDoor = lerp(liftDoor, 0.0f, 0.3f);
        lift_dispatch();
        break;

    case LIFT_MOVING: {
        liftDoor = lerp(liftDoor, 0.0f, 0.4f);
        int far = furthest_call(liftDir);               // extend/retract as calls change
        if (far >= GROUND) liftTarget = far;
        float tY = floor_y(liftTarget);
        float d = tY - liftCarY, ad = d < 0 ? -d : d;
        float spd = ad < 12.0f ? ad * 0.2f : 2.4f;      // cruise, decelerate near the slab
        if (spd < 0.3f) spd = 0.3f;
        if (ad > 0.5f) liftCarY += (d > 0) ? spd : -spd; else liftCarY = tY;

        // once we've truly left the departed floor, allow stopping there again
        if (liftDepart != NO_DEPART) {
            float dy = liftCarY - floor_y(liftDepart);
            if (dy < 0) dy = -dy;
            if (dy > FH * 0.6f) liftDepart = NO_DEPART;
        }
        // pass-through stop at grid floors we'd serve going liftDir
        int nf = (int)((baseY - liftCarY) / (float)FH + 0.5f);
        if (nf < 0) nf = 0; if (nf >= NF) nf = NF - 1;
        float nfY = baseY - nf * FH, ndy = liftCarY - nfY;
        if (ndy < 0) ndy = -ndy;
        if (ndy < 1.0f) {                               // aligned with a floor
            carFloor = nf;
            if (carFloor != liftDepart && stop_here(carFloor, liftDir)) { lift_open(); break; }
        }
        if (ad <= 0.5f) { carFloor = liftTarget; lift_dispatch(); }   // reached the turning point
        break; }

    case LIFT_DOORS:
        if (!liftClosing) {
            liftDoor = lerp(liftDoor, 1.0f, 0.3f);
            if (liftDoor > 0.92f) { liftDoor = 1.0f; lift_service(); if (--liftDoorTimer <= 0) liftClosing = 1; }
        } else {
            liftDoor = lerp(liftDoor, 0.0f, 0.3f);
            if (liftDoor < 0.08f) { liftDoor = 0.0f; liftDepart = carFloor; lift_dispatch(); }
        }
        break;
    }
}

static void draw_walker(Walker *w, int yb) {
    int cx  = (int)w->x;
    int fy  = yb - 4;                       // feet rest on the gallery walkway
    int dir = (w->vx >= 0) ? 1 : -1;
    int standing = (w->vx > -0.05f && w->vx < 0.05f);         // not moving this frame = still
    int step = standing ? 0 : (((int)w->x >> 1) & 1);         // walk cycle driven by position
    int sk = w->skin, hr = w->hair, sh = w->shirt, pa = w->pants;

    // ~15px figure — door-height: head + torso stand above the handrail
    // (cap at yb-9), pelvis and legs scissor behind the sparse bars.
    rectfill(cx - 1, fy - 14, 3, 1, hr);                             // hair top  (yb-18)
    pset(cx - 1, fy - 13, hr); pset(cx, fy - 13, sk); pset(cx + 1, fy - 13, hr); // (yb-17)
    rectfill(cx - 1, fy - 12, 3, 1, sk);                             // face      (yb-16)
    rectfill(cx - 1, fy - 11, 3, 5, sh);                             // torso     (yb-15..11)
    if (!standing)                                                   // a swinging hand sells the walk
        pset(step ? cx - 2 : cx + 2, fy - 10, sh);
    rectfill(cx - 1, fy - 6, 3, 2, pa);                              // pelvis    (yb-10..9)

    // legs: scissor between hip (yb-8) and feet (yb-4)
    int ly = fy - 4;
    int lFoot, rFoot;
    if (standing)  { lFoot = cx - 1;       rFoot = cx + 1;       }
    else if (step) { lFoot = cx - 1 + dir; rFoot = cx + 1 - dir; }
    else           { lFoot = cx - 1 - dir; rFoot = cx + 1 + dir; }
    line(cx - 1, ly, lFoot, fy, pa);
    line(cx + 1, ly, rFoot, fy, pa);
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
    update_walkers();
    update_lift();
#ifdef DE_TRACE
    {
        int na = 0, nw = 0, nr = 0, wmask = 0, gwait = 0;
        for (int i = 0; i < MAXW; i++) {
            if (!walkers[i].active) continue;
            na++;
            if (walkers[i].state == WK_WAIT) { nw++; wmask |= 1 << (walkers[i].floor + 1); if (walkers[i].floor == GROUND) gwait++; }
            if (walkers[i].state == WK_RIDING) nr++;
        }
        watch("active", "%d", na);
        watch("liftSt", "%d", liftState); watch("carF", "%d", carFloor);
        watch("tgt", "%d", liftTarget);   watch("dir", "%d", liftDir);
        watch("riders", "%d", nr);        watch("waiting", "%d", nw);
        watch("wmask", "%d", wmask);      watch("gwait", "%d", gwait);
        watch("opens", "%d", dbg_open);   watch("serve", "%d", dbg_serve);
        watch("lit", "%d", dbg_lit);
    }
#endif
}

// ── drawing ───────────────────────────────────────────────────────────────────

// someone moving about inside a window: a small figure that drifts slowly so a
// lit/curtained home reads as lived-in. Only drawn for occ==1 homes — the ones
// a walker was actually seen entering, so it's the payoff of that walk.
static void draw_occupant(int wx, int wy, int f, int b, int col) {
    int t  = frame() / 110;                    // a slow shuffle — pottering about the room
    int ox = (t * 7 + f * 5 + b * 11) & 3;     // 0..3, drifts side to side
    int fx = wx + 2 + ox;
    rectfill(fx,     wy + 2, 2, 2, col);       // head
    rectfill(fx - 1, wy + 4, 4, 4, col);       // shoulders/torso (lower body cut by the sill)
}

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

    // an occupant a walker brought home (occ==1):
    //  night → a dark backlit silhouette (shadow on the curtain / against the glow)
    //  day   → a dim blue figure, but only when you could actually see in (bare
    //          glass, open drapes, or net) — closed curtains/blinds hide them
    if (h->occ == 1) {
        if (lit) {
            draw_occupant(wx, wy, f, b, CLR_BROWNISH_BLACK);
        } else {
            int see_in = (h->treat == TR_NONE) ||
                         (h->treat == TR_CURTAIN && curt) ||
                         (h->treat == TR_VITRAGE);
            if (see_in) draw_occupant(wx, wy, f, b, CLR_INDIGO);
        }
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

    // walkers on this band — drawn after doors/floor, before the railing, so
    // they read as standing on the gallery behind the steel bars (riders are
    // hidden inside the cab, drawn by draw_tower)
    for (int i = 0; i < MAXW; i++)
        if (walkers[i].active && walkers[i].state != WK_RIDING && walkers[i].floor == f)
            draw_walker(&walkers[i], yb);

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
    // ── the lift: a glazed shaft with a lit car you can watch travel ──────────
    int lx = towerX + 11, lw = 7;            // shaft x, width
    int shTop = wallTop, shBot = baseY + LOBBY_DROP + 2;   // down into the ground lobby
    rectfill(lx, shTop, lw, shBot - shTop, CLR_DARKER_BLUE);   // dark shaft glass
    rectfill(lx - 1, shTop, 1, shBot - shTop, CLR_DARK_GREY);  // guide rails
    rectfill(lx + lw, shTop, 1, shBot - shTop, CLR_DARK_GREY);

    int carBot = (int)(liftCarY + 0.5f);     // car floor line (eased)
    int cabH   = FH - 5;                     // a touch shorter than the floor pitch
    int carTop = carBot - cabH;
    int ix0 = lx + 1, iw = lw - 2;           // lit interior

    rectfill(lx + lw / 2, shTop, 1, carTop - shTop, CLR_DARK_GREY);   // hoist cable from the machine room
    rectfill(lx, carTop, lw, cabH, CLR_LIGHT_GREY);                  // car frame
    rectfill(ix0, carTop + 1, iw, cabH - 2, CLR_LIGHT_YELLOW);        // lit glass cab (tint-exempt → glows)

    // passengers — little silhouettes visible through the glass
    int nr = 0;
    for (int i = 0; i < MAXW && nr < LIFT_CAP; i++) {
        if (!walkers[i].active || walkers[i].state != WK_RIDING) continue;
        int rx = ix0 + nr * 2, ry = carBot - 3;
        pset(rx, ry - 4, CLR_BROWNISH_BLACK);              // head
        rectfill(rx, ry - 3, 1, 4, CLR_BROWNISH_BLACK);    // body + legs
        nr++;
    }

    // sliding doors: a centre seam that parts to the sides as they open
    int half = iw / 2;
    int seam = (int)(liftDoor * half + 0.5f), cm = lx + lw / 2;
    rectfill(cm - seam, carTop + 1, 1, cabH - 2, CLR_DARK_GREY);
    if (seam > 0) rectfill(cm + seam, carTop + 1, 1, cabH - 2, CLR_DARK_GREY);

    line(lx, carTop, lx + lw - 1, carTop, CLR_DARK_GREY);            // ceiling
    line(lx, carBot - 1, lx + lw - 1, carBot - 1, CLR_DARK_GREY);    // floor

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

    // ground-level walkers — coming in off the street, leaving, or queuing at the lobby
    for (int i = 0; i < MAXW; i++)
        if (walkers[i].active && walkers[i].floor == GROUND && walkers[i].state != WK_RIDING)
            draw_walker(&walkers[i], ground_feet() + 4);

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
