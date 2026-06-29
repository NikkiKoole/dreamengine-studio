/* de:meta
{
  "title": "the love parade",
  "status": "active",
  "kind": [
    "toy"
  ],
  "teaches": [
    "gene-based-procgen",
    "no-sprite-vehicles",
    "genetic-crossover"
  ],
  "lineage": "Sibling of trafficjam's gene-bag rendering, but with player-triggered BREEDING: offspring genes are a Mendelian 50/50 mix plus colour-blend-toward-nearest-palette and occasional mutation, yielding centaurs/hybrids.",
  "description": "A generative toy in the rectangular style of Donkey Kong and Boulder Dash — every little person and animal is built from rectfill/circfill (no sprite art), from a random bag of GENES: proportions, colours, haircut, clothing pattern (sometimes), head shape, ears, tail, neck, horns, wings, antennae, eyes. Roll endlessly and a parade strolls past across depth lanes, never the same one twice. The hook: PICK ONE UP and drop it on another and they BREED — an offspring pops out (a tiny baby that grows up over ~10s), its genes a 50/50 mix of the two parents plus the odd mutation. Breeding only ever happens when YOU trigger it. Crazy outcomes: person × beast can make a CENTAUR, two humans can make a HUMAN-ON-ALL-FOURS, and giants/cyclops/winged hybrids turn up. Drag a critter onto another to breed; SPACE re-rolls the crowd; right-click adds a newcomer."
}
de:meta */
#include "studio.h"

// ── THE LOVE PARADE ───────────────────────────────────────────────────────
// A generative toy in the "rectangular style" of Donkey Kong and Boulder Dash —
// every creature is built from rectfill/circfill, no sprite art. A creature is
// a BAG OF GENES (proportions, colours, haircut, clothing pattern, head shape,
// ears, tail, neck, horns, wings, antennae, eyes…). Roll genes → an endless
// parade of little people and animals strolls past, never the same one twice.
//
// It is also a living ecosystem:
//   • PICK ONE UP and drop it on another to breed them — breeding is ONLY ever
//     triggered by you; the parade never pairs off on its own.
//   • BABIES grow up — newborns start small and mature over ~10s (any two can breed).
//   • CRAZY OUTCOMES — person × beast can make a CENTAUR; two humans can make
//     a HUMAN-ON-ALL-FOURS; plus horns, wings, antennae, cyclops eyes, giants.
//
//   drag a critter onto another → breed     SPACE re-roll crowd     right-click add one

// ── palette gene pools ───────────────────────────────────────────────────────
static const int SKIN[]  = { CLR_LIGHT_PEACH, CLR_PEACH, CLR_DARK_PEACH, CLR_BROWN, CLR_MEDIUM_GREY };
static const int SHIRT[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_TRUE_BLUE,
                             CLR_INDIGO, CLR_PINK, CLR_DARK_PURPLE, CLR_LIME_GREEN, CLR_MEDIUM_GREEN, CLR_DARK_RED };
static const int PANTS[] = { CLR_DARK_BLUE, CLR_DARKER_BLUE, CLR_DARK_PURPLE, CLR_BROWN,
                             CLR_DARK_BROWN, CLR_DARK_GREY, CLR_DARK_GREEN };
static const int HAIR[]  = { CLR_BLACK, CLR_DARK_BROWN, CLR_BROWN, CLR_YELLOW, CLR_DARK_GREY, CLR_DARK_RED, CLR_WHITE };
static const int FUR[]   = { CLR_BROWN, CLR_DARK_BROWN, CLR_MEDIUM_GREY, CLR_LIGHT_GREY, CLR_ORANGE,
                             CLR_YELLOW, CLR_DARK_GREEN, CLR_PINK, CLR_WHITE, CLR_DARK_RED, CLR_LIGHT_PEACH };
static const int BELLY[] = { CLR_LIGHT_PEACH, CLR_WHITE, CLR_LIGHT_GREY, CLR_PEACH, CLR_LIGHT_YELLOW };
static const int PATC[]  = { CLR_WHITE, CLR_BLACK, CLR_DARK_BROWN, CLR_DARK_RED, CLR_YELLOW, CLR_DARK_BLUE };
static const int WING[]  = { CLR_PINK, CLR_BLUE, CLR_YELLOW, CLR_LIME_GREEN, CLR_ORANGE, CLR_PEACH, CLR_INDIGO, CLR_WHITE, CLR_LIGHT_YELLOW };
#define PICK(a) ((a)[rnd(sizeof(a)/sizeof((a)[0]))])

enum { KIND_PERSON, KIND_BEAST };
enum { CH_NONE, CH_CENTAUR, CH_QUADHUMAN, CH_ANTHRO };  // ANTHRO = animal upper on human legs

typedef struct {
    int   kind, lane, dir, u;          // u = base draw unit (lane size + giant)
    float x, speed, walk, age;         // age 0..1 = juvenile→adult
    int   cMain, cAccent, cHead, cBelly, cHat, cLimb, cPat, cWing;
    int   headRound, hair, pattern;    // hair 0..6, pattern 0 none/1 spots/2 stripes
    int   ear, neck, legTall, tailUp;  // beast
    int   horns, antennae, wings, eyes, giant, chimera, gen;
} Figure;

#define MAXF 48
static Figure crew[MAXF];
static int ncrew = 0, held = -1, births = 0, maxGen = 0;

// audio — a happy house groove + the pick-up filter-sweep voice
#define I_KICK 5
#define I_BASS 6
#define I_STAB 7
#define I_LEAD 8
#define I_DRAG 9
static bool musicOn   = true;
static int  lastStep  = -1;     // last sixteenth-note we triggered
static int  dragVoice = -1;     // held synth voice while carrying someone

// I–V–vi–IV in C major — the universally-happy progression
static const int CH_ROOT[4] = { 60, 67, 69, 65 };
static const int CH_TYPE[4] = { CHORD_MAJ7, CHORD_DOM7, CHORD_MIN7, CHORD_MAJ7 };

#define NLANE 4
static const int LANE_Y[NLANE] = { 96, 118, 146, 192 };
static const int LANE_U[NLANE] = {  2,   2,   3,   4 };
static const int LANE_N[NLANE] = {  7,   6,   5,   4 };

typedef struct { float x, y, vy; int life, col; } Heart;
#define NHEART 56
static Heart hearts[NHEART];

// ── gene roll ─────────────────────────────────────────────────────────────────
static void roll(Figure *f, int lane, bool at_edge) {
    f->lane = lane;
    f->dir  = chance(50) ? 1 : -1;
    f->age  = 1.0f;
    f->walk = (float)rnd(360);
    f->headRound = chance(55);
    f->giant = chance(7);
    f->u = LANE_U[lane] + f->giant;
    f->speed = rnd_float_between(0.15f, 0.42f) * LANE_U[lane];
    f->gen = 0;
    f->pattern = (rnd(100) < 35) ? (chance(50) ? 1 : 2) : 0;   // patterns only sometimes
    f->cPat = PICK(PATC);
    f->wings = chance(12);
    f->cWing = PICK(WING);
    f->antennae = chance(12);
    f->eyes = (rnd(100) < 12) ? (chance(50) ? 1 : 3) : 2;
    f->chimera = CH_NONE;

    f->kind = chance(58) ? KIND_PERSON : KIND_BEAST;
    if (f->kind == KIND_PERSON) {
        f->cHead = PICK(SKIN); f->cLimb = f->cHead;
        f->cMain = PICK(SHIRT); f->cAccent = PICK(PANTS);
        f->hair  = rnd(7);
        f->cHat  = PICK(HAIR);
        f->horns = chance(7) ? 1 : 0;             // rare little devil
        f->ear = f->neck = f->legTall = f->tailUp = 0;
        if (chance(4)) f->chimera = CH_QUADHUMAN;  // the odd crawler in the wild
    } else {
        f->cMain = PICK(FUR); f->cBelly = PICK(BELLY);
        f->cLimb = f->cMain; f->cHead = f->cMain;
        f->cAccent = PICK(PANTS);                  // legs, if it turns out upright
        f->hair = 0; f->cHat = PICK(HAIR);
        f->ear = rnd(4); f->neck = rnd(3);
        f->legTall = chance(40); f->tailUp = chance(55);
        f->horns = chance(28) ? (chance(40) ? 2 : 1) : 0;
        if (chance(4)) f->chimera = CH_ANTHRO;     // the odd upright beast in the wild
    }
    int span = 60;
    f->x = at_edge ? ((f->dir > 0) ? -span : SCREEN_W + span) : (float)rnd(SCREEN_W);
}

static void repopulate(bool at_edge) {
    ncrew = 0;
    for (int l = 0; l < NLANE; l++)
        for (int i = 0; i < LANE_N[l] && ncrew < MAXF; i++)
            roll(&crew[ncrew++], l, at_edge);
    held = -1;
}

// ── breeding ──────────────────────────────────────────────────────────────────
// the 32 palette colours as RGB, so breeding can BLEND two colours into the
// nearest palette colour between them (red × blue → purple) — real DNA mixing.
static const unsigned char PAL_RGB[32][3] = {
    {0,0,0},{29,43,83},{126,37,83},{0,135,81},{171,82,54},{95,87,79},{194,195,199},{255,241,232},
    {255,0,77},{255,163,0},{255,236,39},{0,228,54},{41,173,255},{131,118,156},{255,119,168},{255,204,170},
    {41,24,20},{17,29,53},{66,33,54},{18,83,89},{116,47,41},{73,51,59},{162,136,121},{243,239,125},
    {190,18,80},{255,108,36},{168,231,46},{0,181,67},{6,90,181},{117,70,101},{255,110,89},{255,157,129},
};
static int nearest_palette(int r, int g, int b) {
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 32; i++) {
        int dr = r - PAL_RGB[i][0], dg = g - PAL_RGB[i][1], db = b - PAL_RGB[i][2];
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
// a categorical gene: inherit one parent's value (Mendelian)
static int gene(int a, int b) { return chance(50) ? a : b; }
// a colour gene: usually blend toward the colour between the parents, else inherit one
static int mixc(int a, int b) {
    if (a == b) return a;
    if (chance(60)) return nearest_palette((PAL_RGB[a][0] + PAL_RGB[b][0]) / 2,
                                           (PAL_RGB[a][1] + PAL_RGB[b][1]) / 2,
                                           (PAL_RGB[a][2] + PAL_RGB[b][2]) / 2);
    return chance(50) ? a : b;
}

static void breed(Figure *c, const Figure *A, const Figure *B, int lane) {
    c->cMain     = mixc(A->cMain, B->cMain);
    c->cAccent   = mixc(A->cAccent, B->cAccent);
    c->cHead     = mixc(A->cHead, B->cHead);
    c->cBelly    = mixc(A->cBelly, B->cBelly);
    c->cHat      = mixc(A->cHat, B->cHat);
    c->cPat      = mixc(A->cPat, B->cPat);
    c->cWing     = mixc(A->cWing, B->cWing);
    c->headRound = gene(A->headRound, B->headRound);
    c->hair      = gene(A->hair, B->hair);
    c->pattern   = gene(A->pattern, B->pattern);
    c->ear       = gene(A->ear, B->ear);
    c->neck      = gene(A->neck, B->neck);
    c->legTall   = gene(A->legTall, B->legTall);
    c->tailUp    = gene(A->tailUp, B->tailUp);
    c->horns     = gene(A->horns, B->horns);
    c->antennae  = gene(A->antennae, B->antennae);
    c->wings     = gene(A->wings, B->wings);
    c->eyes      = gene(A->eyes, B->eyes);
    c->giant     = gene(A->giant, B->giant);
    c->kind      = gene(A->kind, B->kind);

    // crazy outcomes
    c->chimera = CH_NONE;
    bool mixed = (A->kind == KIND_PERSON) != (B->kind == KIND_PERSON);
    if (mixed && chance(55)) {                                // person × beast → half-and-half
        const Figure *P = (A->kind == KIND_PERSON) ? A : B;   // the human parent
        const Figure *Z = (A->kind == KIND_PERSON) ? B : A;   // the animal parent
        if (chance(50)) {                                     // centaur: human top, animal legs
            c->chimera = CH_CENTAUR;
            c->cMain = P->cMain; c->cHead = P->cHead; c->hair = P->hair; c->cHat = P->cHat;
            c->cAccent = Z->cMain; c->cBelly = Z->cBelly; c->legTall = Z->legTall;
        } else {                                              // anthro: animal top, human legs
            c->chimera = CH_ANTHRO;
            c->cMain = Z->cMain; c->cHead = Z->cMain; c->cBelly = Z->cBelly; c->ear = Z->ear;
            c->cAccent = P->cAccent;
        }
    } else if (!mixed && A->kind == KIND_PERSON && chance(11)) {  // two humans → on all fours!
        c->chimera = CH_QUADHUMAN; c->kind = KIND_PERSON;
    } else c->chimera = gene(A->chimera, B->chimera);

    if (chance(8)) switch (rnd(4)) {       // a rare, gentle mutation — not a re-roll
        case 0: c->hair = mid(0, c->hair + (chance(50) ? 1 : -1), 6); break;
        case 1: c->headRound = !c->headRound; break;
        case 2: c->ear = rnd(4); break;
        case 3: c->wings = !c->wings; break;
    }

    // keep colours coherent without introducing a colour neither parent had
    if (c->chimera == CH_NONE) {
        if (c->kind == KIND_PERSON) {
            c->cLimb = c->cHead;
            if (c->cAccent == c->cMain) c->cAccent = (A->cAccent != c->cMain) ? A->cAccent : B->cAccent;
        } else {
            c->cLimb = c->cMain; c->cHead = c->cMain;
            if (c->cBelly == c->cMain) c->cBelly = (A->cBelly != c->cMain) ? A->cBelly : B->cBelly;
        }
    } else c->cLimb = c->cHead;

    c->lane = lane; c->dir = chance(50) ? 1 : -1;
    c->u = LANE_U[lane] + c->giant;
    c->speed = rnd_float_between(0.15f, 0.42f) * LANE_U[lane];
    c->walk = (float)rnd(360);
    c->gen = max(A->gen, B->gen) + 1;
    if (c->gen > maxGen) maxGen = c->gen;
    births++;
}

static void add_figure(const Figure *f, int avoid) {
    if (ncrew < MAXF) { crew[ncrew++] = *f; return; }
    int v; do { v = rnd(ncrew); } while (v == avoid);
    crew[v] = *f;
}

// ── hearts ─────────────────────────────────────────────────────────────────────
static void spawn_hearts(int x, int y, int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < NHEART; i++)
            if (hearts[i].life <= 0) {
                hearts[i] = (Heart){ x + rnd_between(-7, 8), y + rnd_between(-5, 5),
                                     rnd_float_between(-1.3f, -0.4f), rnd_between(20, 34),
                                     chance(50) ? CLR_PINK : CLR_RED };
                break;
            }
}
static void draw_heart(int cx, int cy, int c) {
    rectfill(cx - 2, cy - 1, 2, 2, c);
    rectfill(cx + 1, cy - 1, 2, 2, c);
    rectfill(cx - 2, cy + 1, 5, 1, c);
    rectfill(cx - 1, cy + 2, 3, 1, c);
    pset(cx, cy + 3, c);
}

void init(void) {
    colorkey(-1);
    repopulate(false);
    // instruments for the house beat
    instrument(I_KICK, INSTR_SINE,   1,  70, 0,  30);                              // punchy thump
    instrument(I_BASS, INSTR_SAW,    4,  90, 4,  70); instrument_filter(I_BASS, FILTER_LOW,  700, 5);
    instrument(I_STAB, INSTR_SQUARE, 3, 130, 3, 120); instrument_duty(I_STAB, 0.5f);
                                                      instrument_filter(I_STAB, FILTER_LOW, 1800, 6);
    instrument(I_LEAD, INSTR_TRI,    2,  90, 2,  80);
    // the pick-up voice — a juicy resonant saw you sweep by dragging
    instrument(I_DRAG, INSTR_SAW,   20, 250, 6, 200); instrument_filter(I_DRAG, FILTER_LOW, 1200, 11);
    bpm(124);
}

// one sixteenth-note of the four-on-the-floor groove
static void tick_music(int s) {
    int bar = (beat() / 4) % 4, root = CH_ROOT[bar];
    if (s % 4 == 0)              hit(36, I_KICK, 5, 90);            // four-on-the-floor kick
    if (s == 4 || s == 12)       hit(60, INSTR_NOISE, 4, 70);       // clap on 2 & 4
    if (s % 4 == 2)              hit(90, INSTR_NOISE, 2, 28);       // closed hat (offbeat)
    if (s % 8 == 6)              hit(92, INSTR_NOISE, 2, 110);      // open hat (the "and")
    if (s % 4 == 2)              note(root - 12, I_BASS, 4);        // bouncy offbeat bass
    if (s == 0)                  strum(root, CH_TYPE[bar], I_STAB, 3, 20);   // chord stab
    if (s == 6 || s == 10)       strum(root, CH_TYPE[bar], I_STAB, 2, 14);   // syncopated stab
    if (s % 4 == 0 && chance(30)) note(degree(SCALE_PENTA, 6, rnd(5)), I_LEAD, 2);  // sparkle
}

// ── shared part helpers ─────────────────────────────────────────────────────────
static void draw_head(int x, int hcy, int hr, bool round, int col) {
    if (round) circfill(x, hcy, hr, col);
    else       rectfill(x - hr, hcy - hr, 2 * hr, 2 * hr, col);
}
static void draw_eyes(int x, int hcy, int hr, int dir, int n) {
    if (n <= 1) { pset(x, hcy, CLR_BLACK); return; }     // cyclops
    if (n > 3) n = 3;
    int gap = max(1, hr / 2), x0 = x - (n - 1) * gap / 2 + dir * (hr / 4);
    for (int i = 0; i < n; i++) pset(x0 + i * gap, hcy, CLR_BLACK);
}
static void draw_hair(int x, int hcy, int hr, int u, int hair, int col, int dir) {
    int htop = hcy - hr;
    switch (hair) {
        case 0: break;                                               // bald
        case 1: rectfill(x - hr, htop, 2 * hr, u, col); break;        // short
        case 2: rectfill(x - 1, htop - 2 * u, max(2, u), 2 * u + 1, col); break;  // mohawk
        case 3: rectfill(x - hr, htop, 2 * hr, u, col);               // ponytail
                rectfill((dir > 0) ? x - hr : x + hr - u, hcy - u, u, 3 * u, col); break;
        case 4: circfill(x, htop, hr, col); break;                    // afro
        case 5: rectfill(x - hr, htop, 2 * hr, u, col);               // long
                rectfill(x - hr - 1, hcy - hr + u, u, 2 * hr, col);
                rectfill(x + hr,     hcy - hr + u, u, 2 * hr, col); break;
        case 6: rectfill(x - hr - 1, htop + u, 2 * hr + 2, 1, col);    // flat cap
                rectfill(x - hr + 1, htop, 2 * hr - 2, u, col); break;
    }
}
static void draw_horns(int x, int htop, int hr, int u, bool big, int col) {
    int h = (big ? 3 : 2) * u;
    trifill(x - hr, htop, x - hr + 2 * u, htop, x - hr, htop - h, col);
    trifill(x + hr, htop, x + hr - 2 * u, htop, x + hr, htop - h, col);
}
static void draw_antennae(int x, int htop, int u, int col) {
    line(x - u, htop, x - 2 * u, htop - 3 * u, col); circfill(x - 2 * u, htop - 3 * u, 1, CLR_YELLOW);
    line(x + u, htop, x + 2 * u, htop - 3 * u, col); circfill(x + 2 * u, htop - 3 * u, 1, CLR_YELLOW);
}
static void draw_wings(int x, int cy, int u, int flap, int col) {
    // butterfly/fairy wings: an upper + lower rounded lobe each side, a tiny eyespot,
    // a soft outline. `flap` lifts the tips so they beat with the walk cycle.
    int rx = 3 * u, ry = 2 * u, spread = 3 * u;
    for (int s = -1; s <= 1; s += 2) {
        int wx = x + s * spread, uy = cy - u - flap, ly = cy + ry;
        ovalfill(wx, uy, rx, ry, col);                 // upper lobe
        ovalfill(wx, ly, ry, u + 1, col);              // lower lobe
        oval(wx, uy, rx, ry, CLR_BROWNISH_BLACK);      // soft outline
        pset(wx, uy, CLR_WHITE);                       // eyespot
    }
}
static void apply_pattern(int x, int y, int w, int h, int p, int col) {
    if (p == 1) { int s = max(1, h / 3);
        rectfill(x + w / 5, y + h / 5, s, s, col);
        rectfill(x + 3 * w / 5, y + h / 3, s, s, col);
        rectfill(x + 2 * w / 5, y + 3 * h / 5, s, s, col);
    } else if (p == 2) { int sw = max(1, w / 8);
        for (int sx = x + w / 6; sx < x + w - 1; sx += max(2, w / 3)) rectfill(sx, y, sw, h, col);
    }
}

// ── a person from stacked rectangles ──────────────────────────────────────────
static void draw_person(const Figure *f, int x, int yb, int u) {
    int sw = (int)(u * 0.7f * sin_deg(f->walk));
    int legh = 3 * u, legw = u, top = yb - 8 * u, ar = f->dir;
    int hr = (int)(1.6f * u), hcy = top - hr, htop = hcy - hr;
    if (f->wings) draw_wings(x, top + 2 * u, u, (int)(u * sin_deg(f->walk * 1.5f)), f->cWing);
    rectfill(x - legw + sw, yb - legh, legw, legh, f->cAccent);
    rectfill(x        - sw, yb - legh, legw, legh, f->cAccent);
    rectfill(x - 2 * u, top, 4 * u, 5 * u, f->cMain);
    rectfill(x - 2 * u, top + 3 * u, 4 * u, 2 * u, f->cAccent);
    if (f->pattern) apply_pattern(x - 2 * u, top, 4 * u, 3 * u, f->pattern, f->cPat);
    rectfill(x - 3 * u, top - sw, u, 4 * u, f->cMain);
    rectfill(x + 2 * u, top + sw, u, 4 * u, f->cMain);
    pset(x - 3 * u + u / 2, top - sw + 4 * u, f->cHead);
    pset(x + 2 * u + u / 2, top + sw + 4 * u, f->cHead);
    draw_head(x, hcy, hr, f->headRound, f->cHead);
    draw_eyes(x, hcy, hr, ar, f->eyes);
    draw_hair(x, hcy, hr, u, f->hair, f->cHat, ar);
    if (f->horns)    draw_horns(x, htop, hr, u, f->horns == 2, CLR_LIGHT_GREY);
    if (f->antennae) draw_antennae(x, htop, u, CLR_BLACK);
}

// ── a parametric four-legged beast ────────────────────────────────────────────
static void draw_beast(const Figure *f, int x, int yb, int u) {
    int sw = (int)(u * 0.6f * sin_deg(f->walk));
    int legh = (f->legTall ? 3 : 2) * u, bodyW = 6 * u, bodyH = 3 * u;
    int bx = x - bodyW / 2, by = yb - legh - bodyH, fr = f->dir;
    int frontx = (fr > 0) ? bx + bodyW : bx, backx = (fr > 0) ? bx : bx + bodyW;
    if (f->wings) draw_wings(x, by + u, u, (int)(u * sin_deg(f->walk * 1.5f)), f->cWing);
    int lx[4] = { bx, bx + u, bx + bodyW - 2 * u, bx + bodyW - u };
    int ls[4] = { sw, -sw, -sw, sw };
    for (int i = 0; i < 4; i++) rectfill(lx[i] + ls[i], yb - legh, u, legh, f->cLimb);
    rectfill(bx, by, bodyW, bodyH, f->cMain);
    rectfill(bx, by + bodyH - u, bodyW, u, f->cBelly);
    if (f->pattern) apply_pattern(bx, by, bodyW, bodyH, f->pattern, f->cPat);
    int tdir = -fr;
    if (f->tailUp) rectfill(backx + (tdir < 0 ? -u : 0), by - 2 * u, u, 3 * u, f->cMain);
    else           rectfill(backx + (tdir < 0 ? -u : 0), by + u, u, 2 * u, f->cMain);
    int hd = 3 * u, lift = f->neck * 2 * u;
    int headTop = by - (hd - bodyH) - lift; if (headTop > by - hd) headTop = by - hd;
    int hx = (fr > 0) ? frontx - u : frontx - hd + u, hcx = hx + hd / 2;
    if (lift > 0) { int nx = (fr > 0) ? frontx - 2 * u : frontx;
                    rectfill(nx, headTop + hd - u, 2 * u, by - (headTop + hd - u) + u, f->cMain); }
    rectfill(hx, headTop, hd, hd, f->cHead);
    int etop = headTop - u;
    switch (f->ear) {
        case 1: rectfill(hx, etop, u, u, f->cHead); rectfill(hx + hd - u, etop, u, u, f->cHead); break;
        case 2: circfill(hx + u, headTop, u, f->cHead); circfill(hx + hd - u, headTop, u, f->cHead); break;
        case 3: rectfill(hx - u, headTop, u, 2 * u, f->cHead); rectfill(hx + hd, headTop, u, 2 * u, f->cHead); break;
        default: break;
    }
    int sx = (fr > 0) ? hx + hd : hx - u;
    rectfill(sx, headTop + hd - 2 * u, u, u, f->cBelly);
    draw_eyes(hcx, headTop + hd / 3, hd / 2, fr, f->eyes);
    if (f->horns)    draw_horns(hcx, headTop, hd / 2, u, f->horns == 2, CLR_LIGHT_GREY);
    if (f->antennae) draw_antennae(hcx, headTop, u, CLR_BLACK);
}

// ── centaur: human torso on a four-legged body ────────────────────────────────
static void draw_centaur(const Figure *f, int x, int yb, int u) {
    int sw = (int)(u * 0.6f * sin_deg(f->walk));
    int legh = (f->legTall ? 3 : 2) * u, bodyW = 6 * u, bodyH = 3 * u;
    int bx = x - bodyW / 2, by = yb - legh - bodyH, fr = f->dir;
    int lx[4] = { bx, bx + u, bx + bodyW - 2 * u, bx + bodyW - u };
    int ls[4] = { sw, -sw, -sw, sw };
    for (int i = 0; i < 4; i++) rectfill(lx[i] + ls[i], yb - legh, u, legh, f->cBelly);
    rectfill(bx, by, bodyW, bodyH, f->cAccent);
    rectfill(bx, by + bodyH - u, bodyW, u, f->cBelly);
    if (f->pattern) apply_pattern(bx, by, bodyW, bodyH, f->pattern, f->cPat);
    int backx = (fr > 0) ? bx : bx + bodyW;
    rectfill(backx + (fr > 0 ? -u : 0), by - 2 * u, u, 3 * u, f->cAccent);   // tail
    int ucx = (fr > 0) ? bx + bodyW - 2 * u : bx + 2 * u, utop = by - 5 * u;
    rectfill(ucx - 2 * u, utop, 4 * u, 5 * u, f->cMain);                     // human torso
    if (f->pattern) apply_pattern(ucx - 2 * u, utop, 4 * u, 3 * u, f->pattern, f->cPat);
    rectfill(ucx - 3 * u, utop, u, 4 * u, f->cMain);
    rectfill(ucx + 2 * u, utop, u, 4 * u, f->cMain);
    int hr = (int)(1.6f * u), hcy = utop - hr, htop = hcy - hr;
    draw_head(ucx, hcy, hr, f->headRound, f->cHead);
    draw_eyes(ucx, hcy, hr, fr, f->eyes);
    draw_hair(ucx, hcy, hr, u, f->hair, f->cHat, fr);
    if (f->horns) draw_horns(ucx, htop, hr, u, f->horns == 2, CLR_LIGHT_GREY);
}

// ── a human walking on all fours ──────────────────────────────────────────────
static void draw_quadhuman(const Figure *f, int x, int yb, int u) {
    int sw = (int)(u * 0.6f * sin_deg(f->walk));
    int legh = 2 * u, bodyW = 6 * u, bodyH = 3 * u;
    int bx = x - bodyW / 2, by = yb - legh - bodyH, fr = f->dir;
    int lx[4] = { bx, bx + u, bx + bodyW - 2 * u, bx + bodyW - u };
    int ls[4] = { sw, -sw, -sw, sw };
    for (int i = 0; i < 4; i++) rectfill(lx[i] + ls[i], yb - legh, u, legh, f->cHead);  // skin limbs
    rectfill(bx, by, bodyW, bodyH, f->cMain);                                            // shirt back
    rectfill(bx, by + bodyH - u, bodyW, u, f->cAccent);
    if (f->pattern) apply_pattern(bx, by, bodyW, bodyH, f->pattern, f->cPat);
    int hd = (int)(3.2f * u);
    int hx = (fr > 0) ? bx + bodyW - u : bx - (hd - u);
    int hcx = hx + hd / 2, hcy = by - hd / 2, hr = hd / 2, htop = hcy - hr;
    rectfill((fr > 0) ? bx + bodyW - 2 * u : bx, by - u, 2 * u, u + bodyH / 2, f->cMain); // neck stub
    draw_head(hcx, hcy, hr, true, f->cHead);
    draw_eyes(hcx, hcy, hr, fr, f->eyes);
    draw_hair(hcx, hcy, hr, u, f->hair, f->cHat, fr);
    if (f->horns) draw_horns(hcx, htop, hr, u, f->horns == 2, CLR_LIGHT_GREY);
}

// ── anthro: animal head + furry torso on two human legs (the centaur's opposite) ──
static void draw_anthro(const Figure *f, int x, int yb, int u) {
    int sw = (int)(u * 0.7f * sin_deg(f->walk));
    int legh = 3 * u, legw = u, top = yb - 8 * u, ar = f->dir;
    if (f->wings) draw_wings(x, top + 2 * u, u, (int)(u * sin_deg(f->walk * 1.5f)), f->cWing);
    rectfill(x - legw + sw, yb - legh, legw, legh, f->cAccent);   // human legs (pants)
    rectfill(x        - sw, yb - legh, legw, legh, f->cAccent);
    rectfill(x - 2 * u, top, 4 * u, 5 * u, f->cMain);             // furry torso
    rectfill(x - 2 * u, top + 3 * u, 4 * u, 2 * u, f->cBelly);
    if (f->pattern) apply_pattern(x - 2 * u, top, 4 * u, 3 * u, f->pattern, f->cPat);
    rectfill(x - 3 * u, top - sw, u, 4 * u, f->cMain);            // furry arms
    rectfill(x + 2 * u, top + sw, u, 4 * u, f->cMain);
    int hd = 3 * u, htop = top - hd, hx = x - hd / 2;             // beast head
    rectfill(hx, htop, hd, hd, f->cHead);
    switch (f->ear) {
        case 1: rectfill(hx, htop - u, u, u, f->cHead); rectfill(hx + hd - u, htop - u, u, u, f->cHead); break;
        case 2: circfill(hx + u, htop, u, f->cHead); circfill(hx + hd - u, htop, u, f->cHead); break;
        case 3: rectfill(hx - u, htop, u, 2 * u, f->cHead); rectfill(hx + hd, htop, u, 2 * u, f->cHead); break;
        default: break;
    }
    int snx = (ar > 0) ? hx + hd : hx - u;
    rectfill(snx, htop + hd - 2 * u, u, u, f->cBelly);            // snout
    draw_eyes(x, htop + hd / 3, hd / 2, ar, f->eyes);
    if (f->horns)    draw_horns(x, htop, hd / 2, u, f->horns == 2, CLR_LIGHT_GREY);
    if (f->antennae) draw_antennae(x, htop, u, CLR_BLACK);
}

static void draw_figure(const Figure *f, int x, int yb) {
    int u = f->u; if (f->age < 1.0f) u = max(1, u - 1);    // babies are smaller
    if      (f->chimera == CH_CENTAUR)   draw_centaur(f, x, yb, u);
    else if (f->chimera == CH_QUADHUMAN) draw_quadhuman(f, x, yb, u);
    else if (f->chimera == CH_ANTHRO)    draw_anthro(f, x, yb, u);
    else if (f->kind == KIND_PERSON)     draw_person(f, x, yb, u);
    else                                 draw_beast(f, x, yb, u);
}

// ── picking up / drop targets (a generous box around the figure) ──────────────
static void fig_box(const Figure *f, int *bx, int *by, int *bw, int *bh) {
    int u = f->u; if (f->age < 1.0f) u = max(1, u - 1);
    int yb = LANE_Y[f->lane], x = (int)f->x;
    *bw = 8 * u; *bx = x - 4 * u; *by = yb - 15 * u; *bh = 15 * u;
}
static int hit_test(int mx, int my, int avoid) {
    for (int l = NLANE - 1; l >= 0; l--)
        for (int i = 0; i < ncrew; i++) {
            if (i == avoid || crew[i].lane != l) continue;
            int bx, by, bw, bh; fig_box(&crew[i], &bx, &by, &bw, &bh);
            if (point_in_box(mx, my, bx, by, bw, bh)) return i;
        }
    return -1;
}

// ── update ─────────────────────────────────────────────────────────────────────
void update(void) {
    if (keyp(KEY_SPACE)) { repopulate(true); births = 0; maxGen = 0; }
    if (keyp('M')) musicOn = !musicOn;

    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_RIGHT)) {
        int best = 0, bd = 9999;
        for (int l = 0; l < NLANE; l++) { int d = abs(LANE_Y[l] - my); if (d < bd) { bd = d; best = l; } }
        Figure nf; roll(&nf, best, false); nf.x = (float)mx; add_figure(&nf, -1);
        note(72 + rnd(8), INSTR_TRI, 4);
    }

    // pick someone up → start a resonant voice you sweep by dragging
    if (mouse_pressed(MOUSE_LEFT)) {
        held = hit_test(mx, my, -1);
        if (held >= 0 && dragVoice < 0) dragVoice = note_on(54, I_DRAG, 4);
    }
    if (held >= 0) {
        crew[held].x = (float)mx;
        if (dragVoice >= 0) {
            note_pitch (dragVoice, 46 + (mx * 26.0f / SCREEN_W));                                  // X → pitch
            note_cutoff(dragVoice, 220 + (int)((1.0f - clamp((float)my / SCREEN_H, 0, 1)) * 4200)); // Y → brightness
        }
    }
    if (mouse_released(MOUSE_LEFT)) {
        if (dragVoice >= 0) { note_off(dragVoice); dragVoice = -1; }
        if (held >= 0) {
            int tgt = hit_test(mx, my, held);
            if (tgt >= 0) {                              // any pairing always makes a baby ;)
                Figure child; breed(&child, &crew[held], &crew[tgt], crew[tgt].lane);
                child.age = 0; child.x = (float)mx; add_figure(&child, held);
                spawn_hearts(mx, my - 6, 10);
                strum(72, CHORD_MAJ, INSTR_TRI, 5, 35);
            }
            held = -1;
        }
    }

    // house groove — fire each new sixteenth-note
    if (musicOn) {
        int s = beat() * 4 + (int)(beat_pos() * 4.0f);
        if (s != lastStep) { lastStep = s; tick_music(s % 16); }
    }

    // walk + age (the carried one stays put)
    for (int i = 0; i < ncrew; i++) {
        if (i == held) continue;
        Figure *f = &crew[i];
        if (f->age < 1.0f) { f->age += dt() * 0.10f; if (f->age > 1.0f) f->age = 1.0f; }
        float spd = f->speed * (0.5f + 0.5f * f->age);
        f->x += f->dir * spd;
        f->walk += spd * 9.0f;
        if (f->x < -60 || f->x > SCREEN_W + 60) roll(f, f->lane, true);
    }

    for (int i = 0; i < NHEART; i++)
        if (hearts[i].life > 0) { hearts[i].y += hearts[i].vy; hearts[i].life--; }
}

// ── render ─────────────────────────────────────────────────────────────────────
void draw(void) {
    vgradient(0, 0, SCREEN_W, 84, CLR_BLUE, CLR_LIGHT_GREY);
    gradient(0, 84, SCREEN_W, SCREEN_H - 84, CLR_DARK_GREEN, CLR_MEDIUM_GREEN, 90);
    for (int l = 0; l < NLANE; l++) rectfill(0, LANE_Y[l] - 1, SCREEN_W, 2, CLR_DARK_GREEN);

    int mx = mouse_x(), my = mouse_y();

    for (int l = 0; l < NLANE; l++)
        for (int i = 0; i < ncrew; i++)
            if (i != held && crew[i].lane == l)
                draw_figure(&crew[i], (int)crew[i].x, LANE_Y[l]);

    for (int i = 0; i < NHEART; i++)
        if (hearts[i].life > 0) draw_heart((int)hearts[i].x, (int)hearts[i].y, hearts[i].col);

    if (held >= 0) {
        Figure *f = &crew[held];
        int u = f->u, yb = mid(28, my, SCREEN_H - 2);
        ovalfill(mx, yb + 1, 4 * u, u, CLR_DARK_GREEN);
        draw_figure(f, mx, yb);
        int tgt = hit_test(mx, my, held);
        if (tgt >= 0) {
            int bx, by, bw, bh; fig_box(&crew[tgt], &bx, &by, &bw, &bh);
            rect(bx, by, bw, bh, blink(6) ? CLR_PINK : CLR_RED);
        }
        if (blink(10)) draw_heart(mx, yb - 13 * u, CLR_PINK);
    }

    // ── title — big with a drop shadow, festival-style ──
    print_scaled("LOVE PARADE", 5, 5, CLR_DARK_PURPLE, 2);
    print_scaled("LOVE PARADE", 4, 4, CLR_PINK, 2);

    // ── stats — small font, outlined so they read over the sky ──
    font(FONT_SMALL);
    const char *st = str("pop %d   gen %d   love %d", ncrew, maxGen, births);
    print_outline(st, SCREEN_W - 4 - text_width(st), 6, CLR_WHITE, CLR_DARK_PURPLE);
    print_outline(str("drag to breed    space re-roll    right-click add    M music %s", musicOn ? "on" : "off"),
                  4, SCREEN_H - 9, CLR_LIGHT_PEACH, CLR_DARKER_PURPLE);
    font(FONT_NORMAL);
}
