/* de:meta
{
  "slug": "glyphpup",
  "title": "glyph puppet",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "motion-sequencing"
  ],
  "lineage": "A fresh experiment out of fmbox's LCD dancer: instead of baking animation FRAMES, a figure is a loose CLUSTER of tiny 6x6 monochrome glyph cells, and it comes alive from cheap channels — glyph SWAP + a whole-pixel NUDGE (+ a proposed squash/stretch SCALE). The bank grows one shape at a time; motion is code, never frames. Design: docs/design/glyph-puppet.md.",
  "description": {
    "summary": "Puppets built from a tiny shared bank of 6x6 glyphs — animated by swapping shapes and nudging cells in whole pixels, never by baking frames. A dude, a dog and a bird share one bank, and events throw glyph confetti (hearts/rings/crosses).",
    "detail": "The bet: you don't need frames, you need TWO channels. A figure is a loose bag of 6x6 monochrome glyph cells, and it comes alive from (1) SWAPPING which glyph sits in a cell — legs A<->B for a walk, mouth open on a hit, wings up for a flap — and (2) NUDGING cells off their slot. The model is honest lo-fi: everything lives on a small LOGICAL pixel grid, a part lands on any WHOLE logical pixel (free of the 6-cell grid, but never a fractional x=8.5), and the grid is scaled up uniformly afterwards. Motion stays RIGID — idle bob and beat-bounce are one shared offset added to every cell, so connected parts never drift apart; parts move relative to each other ONLY for a deliberate articulation. Nothing is FFT-driven: an internal metronome fires kick/snare events (or SPACE/S by hand) and everything reacts. The payoff is the SHARED BANK: press V to cycle a DUDE, a DOG and a BIRD that reuse the exact same glyphs — the dog's body and the bird's body are the dude's TORSO, the dog's four legs are the LEGS pair placed twice (out of phase for a trot), the bird's two wings and the dog's tail are ARM glyphs; each animal added just ONE new shape (its head). Grow the bank by a glyph, get a whole new cast member. Events also throw PARTICLES — the same 6x6 glyphs as confetti: a white RING pops on every kick, a red HEART on every snare, a yellow CROSS on the off-beat, each rising in clean whole-pixel steps then vanishing. The GRID-SNAP toggle (G) is the A/B for channel 2: snap ON locks every cell on its slot so only glyph-swaps animate (stiff clockwork); snap OFF lets the whole-pixel nudges through and the same shapes breathe. The raw bank is drawn along the bottom so you can see it's shapes, not frames.",
    "controls": "SPACE or click = a KICK pulse (ring pop). S = a SNARE (dude ta-da / dog bark / bird chirp, + a heart). V = cycle cast (DUDE -> DOG -> BIRD). G = toggle GRID-SNAP (channel-2 life on/off). A = toggle the auto-metronome. LEFT/RIGHT = tempo."
  },
  "todo": [
    "SCALE channel: wire arbitrary 2-axis scaling (squash-on-kick / stretch-on-jump) via sspr — the 3rd channel after swap + nudge. Nearest-neighbor so it stays hard pixels; integer scale = uniform blocks, non-integer = chunky uneven pixels (fine for a deliberate squash). Requires moving the glyphs onto the sprite sheet — GENERATE the sheet from the XPM strings at startup so the plain-text bank stays the source of truth. See design/glyph-puppet.md.",
    "glyphkit.js authoring tool: draw a 6x6 glyph -> emit its XPM '..##..' string to paste in; render-bank -> one labelled PNG (twin of sprite-preview.js). ASCII stays source of truth. Bigger follow-on: a rig composer that places glyphs into slots and exports the layout too. See design/glyph-puppet.md.",
    "Author it at a genuinely tiny native resolution (e.g. 48x48) and drop the in-cart PX block-scaling — let the engine's own integer upscale be the 'scale the grid up' step (the honest form of the logical-grid model).",
    "Add a per-cell FLIP+ROTATE channel (sspr_ex gives rotation for free once glyphs are on the sheet) so one arm glyph serves both sides and a leg can kick out.",
    "If this graduates: add a 'procedural-animation' teaches vocab term (tools/teaches-vocab.js) instead of borrowing motion-sequencing."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// GLYPH PUPPET — animation without frames.
//
// A figure is NOT a filmstrip. It is a small cluster of 6x6 monochrome glyph
// cells, and it moves through just two channels:
//
//   1. GLYPH SWAP  — change which shape sits in a cell (legs A<->B = a walk,
//                    mouth open on a hit, arms up for a ta-da).
//   2. PIXEL NUDGE — offset a cell in WHOLE pixels off its slot (idle bob, a
//                    beat-bounce, a deliberate arm/head lift).
//
// The model is honest lo-fi. Everything lives on a small LOGICAL pixel grid: a
// part lands on any WHOLE logical pixel (free of the 6-cell grid, but never a
// fraction like x=8.5), and the grid is scaled up uniformly afterwards (PX).
// Motion stays RIGID — bob/bounce is ONE shared offset added to every cell, so
// connected parts never drift apart; a part moves relative to another ONLY for a
// deliberate articulation. Snap every cell to its slot (press G) and only glyph
// swaps animate (dead clockwork); release it and the whole-pixel nudges breathe.
//
// The payoff is the SHARED BANK: press V to cycle a DUDE, a DOG and a BIRD that
// reuse the SAME glyphs (body=TORSO, legs=the LEGS pair, tail/wings=ARM glyphs) —
// each animal added just ONE new shape (its head). Grow the bank by a glyph, get
// a new cast member. Events also throw the same glyphs as PARTICLES (heart/ring/
// cross confetti). Kick/snare come from an internal metronome, never an FFT —
// same lineage as fmbox's LCD dancer.

// ---- the glyph bank: shapes, not frames. add one as you need a new pose. ----
// 6 rows of 6 chars; '#' = ink. Authored as ASCII so growing the bank is trivial.
enum {
    G_HEAD, G_HEAD_SING,
    G_TORSO, G_TORSO_SQUASH,
    G_LEGS_A, G_LEGS_B,
    G_ARM_DOWN, G_ARM_UP,
    G_DOGHEAD,                 // the ONLY new shape the dog needs — everything
    G_BIRDHEAD,                // else (body/legs/tail/wings) is reused from above
    G_HEART, G_CIRCLE, G_CROSS, // particle shapes — same 6x6 glyphs, thrown as confetti
    G_COUNT
};

static const char *BANK[G_COUNT][6] = {
    [G_HEAD] = {
        ".####.",
        "#....#",
        "#.##.#",
        "#....#",
        "#....#",
        ".####." },
    [G_HEAD_SING] = {          // open mouth
        ".####.",
        "#....#",
        "#.##.#",
        "#....#",
        "#.##.#",
        ".####." },
    [G_TORSO] = {
        "..##..",
        ".####.",
        "######",
        "######",
        ".####.",
        "..##.." },
    [G_TORSO_SQUASH] = {       // wider + shorter — the beat squash
        "......",
        "######",
        "######",
        "######",
        "######",
        "......" },
    [G_LEGS_A] = {             // stride one
        ".#..#.",
        ".#..#.",
        ".#..#.",
        ".#..#.",
        "##..#.",
        "##..##" },
    [G_LEGS_B] = {             // stride two (spread)
        ".#..#.",
        ".#..#.",
        "#....#",
        "#....#",
        "#....#",
        "##..##" },
    [G_ARM_DOWN] = {           // left arm: shoulder top-right, hand down-left
        "....#.",
        "...#..",
        "..#...",
        ".#....",
        "#.....",
        "......" },
    [G_ARM_UP] = {             // left arm: shoulder bottom-right, hand up-left
        "#.....",
        ".#....",
        "..#...",
        "...#..",
        "....#.",
        "......" },
    [G_DOGHEAD] = {            // side view, facing RIGHT: ear, skull+eye, muzzle
        ".#....",
        "###...",
        "##.###",
        ".#####",
        ".####.",
        "..#..." },
    [G_BIRDHEAD] = {           // round head, eye, beak poking RIGHT
        ".###..",
        "#...#.",
        "#.#.##",
        "#...#.",
        ".###..",
        "......" },
    [G_HEART] = {
        ".##.##",
        "######",
        "######",
        ".####.",
        "..##..",
        "......" },
    [G_CIRCLE] = {             // a hollow ring — a pop
        ".####.",
        "#....#",
        "#....#",
        "#....#",
        "#....#",
        ".####." },
    [G_CROSS] = {              // a plus
        "..##..",
        "..##..",
        "######",
        "######",
        "..##..",
        "..##.." },
};

// ---- a puppet = placed cells. slot is in stage px (1 cell = 6 stage px). ----
typedef struct {
    int   glyph;        // current shape (swapped live)
    float sx, sy;       // slot position, LOGICAL px (top-left of the 6x6)
    float dx, dy;       // live nudge off the slot, logical px (rounds to whole steps)
    int   flip;         // mirror horizontally (right arm reuses left glyph)
} Cell;

// DUDE (upright): cells listed back-to-front so a plain loop draws in order.
enum { C_ARM_L, C_ARM_R, C_LEGS, C_TORSO, C_HEAD, C_COUNT };
static Cell pup[C_COUNT];

// DOG (side view): SAME bank — body=TORSO, all four legs=the LEGS pair placed
// twice, tail=an ARM glyph. Only the head shape is new. Back-to-front order.
enum { D_TAIL, D_LEG_B, D_BODY, D_LEG_F, D_HEAD, D_COUNT };
static Cell dog[D_COUNT];

// BIRD (upright chick): SAME bank — body=TORSO, two wings=ARM glyphs, feet=LEGS.
// Only the head (with a beak) is new. Back-to-front order.
enum { B_WING_L, B_WING_R, B_LEGS, B_BODY, B_HEAD, B_COUNT };
static Cell bird[B_COUNT];

enum { SCENE_DUDE, SCENE_DOG, SCENE_BIRD, SCENE_COUNT };
static int scene = SCENE_DUDE;

// ---- PARTICLES: little glyphs thrown as confetti on events (any cast) ----
// Same 6x6 glyphs, same whole-pixel model — they live in the logical grid and
// draw_cell rounds them, so they rise in clean pixel steps and never smear.
typedef struct {
    int   active, glyph, col;
    float x, y, vy, life;      // logical px; float internally, rounded at draw
} Particle;
#define NPART 28
static Particle parts[NPART];
static int spawn_i = 0;

static void spawn_particle(int glyph, int col) {
    for (int n = 0; n < NPART; n++) {
        int k = (spawn_i + n) % NPART;
        if (parts[k].active) continue;
        parts[k].active = 1;
        parts[k].glyph  = glyph;
        parts[k].col    = col;
        parts[k].x      = 6.0f + (float)((spawn_i % 5) - 2) * 2.0f; // spread across body
        parts[k].y      = 8.0f;                                     // emit from the chest
        parts[k].vy     = -6.0f - (float)(spawn_i % 3);             // float up
        parts[k].life   = 0.85f;
        spawn_i++;
        return;
    }
}

// event envelopes (jump to 1 on a hit, decay each frame)
static float kick_env = 0, snare_env = 0;
static int   snap = 0;      // grid-snap: kill all offsets -> dead clockwork
static int   automet = 1;   // internal metronome on
static int   tempo = 120;
static int   last_beat = -1;
static float prev_t = 0;

// stage -> screen. PX = zoom (SCALE is a compile flag; don't shadow it).
#define PX 8
#define ORIGIN_X 92
#define ORIGIN_Y 44

static void trig_kick(void)  { kick_env  = 1.0f; spawn_particle(G_CIRCLE, CLR_WHITE); }
static void trig_snare(void) { snare_env = 1.0f; spawn_particle(G_HEART,  CLR_RED);   }

// no init hook in dreamengine — lay out both casts lazily on the first update.
static void layout(void) {
    // DUDE: 3 cells wide x 3 tall. arms flank the torso row.
    pup[C_HEAD]  = (Cell){ G_HEAD,     6, 0, 0,0, 0 };
    pup[C_TORSO] = (Cell){ G_TORSO,    6, 6, 0,0, 0 };
    pup[C_LEGS]  = (Cell){ G_LEGS_A,   6,12, 0,0, 0 };
    pup[C_ARM_L] = (Cell){ G_ARM_DOWN, 2, 6, 0,0, 0 };
    pup[C_ARM_R] = (Cell){ G_ARM_DOWN,10, 6, 0,0, 1 };

    // DOG: same glyphs, new slots. Body-blob centred, head up front-right, two
    // leg-pairs fore & aft, tail an ARM glyph flipped to trail off the back-left.
    dog[D_BODY]  = (Cell){ G_TORSO,    5, 7, 0,0, 0 };
    dog[D_HEAD]  = (Cell){ G_DOGHEAD, 10, 4, 0,0, 0 };
    dog[D_LEG_F] = (Cell){ G_LEGS_A,   9,11, 0,0, 0 };
    dog[D_LEG_B] = (Cell){ G_LEGS_A,   3,11, 0,0, 0 };
    dog[D_TAIL]  = (Cell){ G_ARM_UP,   1, 4, 0,0, 1 };

    // BIRD: same glyphs again. Body-blob, beaked head up-right, two wings (ARM
    // glyphs) flanking, little LEGS feet below.
    bird[B_BODY]   = (Cell){ G_TORSO,    6, 7, 0,0, 0 };
    bird[B_HEAD]   = (Cell){ G_BIRDHEAD, 8, 3, 0,0, 0 };
    bird[B_WING_L] = (Cell){ G_ARM_DOWN, 2, 7, 0,0, 0 };
    bird[B_WING_R] = (Cell){ G_ARM_DOWN,10, 7, 0,0, 1 };
    bird[B_LEGS]   = (Cell){ G_LEGS_A,   6,12, 0,0, 0 };
}

// ---- DUDE animation: glyph swaps + a rigid shared-root bounce ----
static void animate_dude(float t) {
    // CHANNEL 1: glyph swaps (fire even when grid-snapped)
    int stepping = ((int)(t * 6.0f)) & 1;              // 1/8 walk phase
    pup[C_LEGS].glyph  = stepping ? G_LEGS_A : G_LEGS_B;
    pup[C_HEAD].glyph  = snare_env > 0.35f ? G_HEAD_SING : G_HEAD;
    pup[C_TORSO].glyph = kick_env  > 0.45f ? G_TORSO_SQUASH : G_TORSO;
    int arms_up = snare_env > 0.30f;
    pup[C_ARM_L].glyph = arms_up ? G_ARM_UP : G_ARM_DOWN;
    pup[C_ARM_R].glyph = arms_up ? G_ARM_UP : G_ARM_DOWN;

    // CHANNEL 2: whole-pixel nudges; shared root keeps the body rigid
    for (int i = 0; i < C_COUNT; i++) { pup[i].dx = 0; pup[i].dy = 0; }
    if (snap) return;
    float root_dy = sinf(t * 6.2831853f * 1.1f) * 1.3f - kick_env * 3.0f;
    for (int i = 0; i < C_COUNT; i++) pup[i].dy = root_dy;
    pup[C_ARM_L].dx -= snare_env * 1.0f;  pup[C_ARM_L].dy -= snare_env * 2.0f;
    pup[C_ARM_R].dx += snare_env * 1.0f;  pup[C_ARM_R].dy -= snare_env * 2.0f;
}

// ---- DOG animation: same channels, reusing the same glyphs ----
static void animate_dog(float t) {
    // CHANNEL 1: trot = the LEGS pair swapped A<->B, fore & aft OUT of phase;
    // tail wags (an ARM glyph up/down) on the kick.
    int step = ((int)(t * 8.0f)) & 1;                  // faster trot
    dog[D_LEG_F].glyph = step ? G_LEGS_A : G_LEGS_B;
    dog[D_LEG_B].glyph = step ? G_LEGS_B : G_LEGS_A;   // opposite stride
    dog[D_TAIL].glyph  = kick_env > 0.4f ? G_ARM_UP : G_ARM_DOWN;

    // CHANNEL 2: shared-root hop keeps the dog rigid; head lifts to "bark".
    for (int i = 0; i < D_COUNT; i++) { dog[i].dx = 0; dog[i].dy = 0; }
    if (snap) return;
    float root_dy = sinf(t * 6.2831853f * 1.3f) * 1.0f - kick_env * 3.0f;
    for (int i = 0; i < D_COUNT; i++) dog[i].dy = root_dy;
    dog[D_HEAD].dy -= snare_env * 2.0f;                 // deliberate: bark up
}

// ---- BIRD animation: wings flap on the kick, head chirps up on the snare ----
static void animate_bird(float t) {
    // CHANNEL 1: both wings swap ARM_DOWN<->ARM_UP — a flap driven by the kick,
    // with a gentle idle flap so it's alive between beats.
    int flap = kick_env > 0.30f || (((int)(t * 5.0f)) & 1);
    bird[B_WING_L].glyph = flap ? G_ARM_UP : G_ARM_DOWN;
    bird[B_WING_R].glyph = flap ? G_ARM_UP : G_ARM_DOWN;
    int step = ((int)(t * 6.0f)) & 1;                  // little foot shuffle
    bird[B_LEGS].glyph = step ? G_LEGS_A : G_LEGS_B;

    // CHANNEL 2: shared-root hop; head chirps up on the snare (deliberate).
    for (int i = 0; i < B_COUNT; i++) { bird[i].dx = 0; bird[i].dy = 0; }
    if (snap) return;
    float root_dy = sinf(t * 6.2831853f * 1.4f) * 1.0f - kick_env * 2.5f;
    for (int i = 0; i < B_COUNT; i++) bird[i].dy = root_dy;
    bird[B_HEAD].dy -= snare_env * 2.0f;
}

// advance every live particle: rise + age out (whole-pixel look via draw_cell)
static void update_particles(float dt) {
    for (int i = 0; i < NPART; i++) {
        if (!parts[i].active) continue;
        parts[i].y   += parts[i].vy * dt;
        parts[i].life -= dt;
        if (parts[i].life <= 0) parts[i].active = 0;
    }
}

void update(void) {
    static int inited = 0;
    if (!inited) { layout(); inited = 1; }

    float t = now();
    float dt = t - prev_t; prev_t = t;
    if (dt < 0) dt = 0; if (dt > 0.1f) dt = 0.1f;

    bpm(tempo);

    // --- events: internal metronome (kick every beat, snare on odd beats) ---
    if (automet) {
        int b = beat();
        if (b != last_beat) {
            last_beat = b;
            trig_kick();
            if (b & 1) trig_snare();
            if (b % 4 == 2) spawn_particle(G_CROSS, CLR_YELLOW);  // an off-beat garnish
        }
    }
    // manual triggers (mouse EDGE, so a held click doesn't flood particles)
    static int prev_mouse = 0;
    int md = mouse_down(MOUSE_LEFT);
    if (keyp(KEY_SPACE) || (md && !prev_mouse)) trig_kick();
    prev_mouse = md;
    if (keyp('S')) trig_snare();
    if (keyp('G')) snap = !snap;
    if (keyp('A')) { automet = !automet; last_beat = -1; }
    if (keyp('V')) scene = (scene + 1) % SCENE_COUNT;   // swap cast member
    if (keyp(KEY_LEFT))  tempo = tempo > 40  ? tempo - 5 : tempo;
    if (keyp(KEY_RIGHT)) tempo = tempo < 240 ? tempo + 5 : tempo;

    // --- decay envelopes ---
    kick_env  -= dt * 3.2f;  if (kick_env  < 0) kick_env  = 0;
    snare_env -= dt * 2.6f;  if (snare_env < 0) snare_env = 0;

    // ---- animate the active cast member (same events, same bank) ----
    if      (scene == SCENE_DOG)  animate_dog(t);
    else if (scene == SCENE_BIRD) animate_bird(t);
    else                          animate_dude(t);

    update_particles(dt);
}

static void draw_cell(int gid, float sx, float sy, int flip, int col) {
    const char **g = BANK[gid];
    // The MODEL: a small LOGICAL pixel grid. Quantize the glyph's position to a
    // whole logical pixel FIRST (round in logical space), THEN scale the grid up
    // by PX. So a part can sit on any whole logical pixel — free of the cell grid
    // — but never a fractional one like x=8.5. PX is just "make the grid bigger",
    // a separate uniform step that adds no fractions. Identical to authoring in a
    // 32x32 framebuffer and letting the engine upscale it.
    int lx = (int)lroundf(sx);          // whole logical pixel
    int ly = (int)lroundf(sy);
    int ox = ORIGIN_X + lx * PX;        // then scale up
    int oy = ORIGIN_Y + ly * PX;
    for (int y = 0; y < 6; y++) {
        const char *row = g[y];
        for (int x = 0; x < 6; x++) {
            int src = flip ? 5 - x : x;
            if (row[src] != '#') continue;
            rectfill(ox + x * PX, oy + y * PX, PX, PX, col);
        }
    }
}

// draw a whole figure: its cells are ordered back-to-front, so just loop.
static void draw_figure(const Cell *c, int n, int ink) {
    for (int i = 0; i < n; i++)
        draw_cell(c[i].glyph, c[i].sx + c[i].dx, c[i].sy + c[i].dy, c[i].flip, ink);
}

// draw the confetti on top of the figure
static void draw_particles(void) {
    for (int i = 0; i < NPART; i++) {
        if (!parts[i].active) continue;
        draw_cell(parts[i].glyph, parts[i].x, parts[i].y, 0, parts[i].col);
    }
}

// tiny 1:1 preview of the whole bank so you SEE it's shapes, not frames
static void draw_bank(void) {
    int bx = 8, by = SCREEN_H - 42;
    print("BANK (shapes, not frames)", bx, by - 8, CLR_DARK_GREY);
    for (int i = 0; i < G_COUNT; i++) {
        int ox = bx + i * 10;
        rectfill(ox - 1, by - 1, 8, 8, CLR_DARKER_GREY);
        const char **g = BANK[i];
        for (int y = 0; y < 6; y++)
            for (int x = 0; x < 6; x++)
                if (g[y][x] == '#') pset(ox + x, by + y, CLR_LIGHT_GREY);
    }
}

void draw(void) {
    cls(CLR_BLACK);

    print("GLYPH PUPPET", 8, 8, CLR_WHITE);
    print("animation from a tiny glyph bank - no baked frames", 8, 18, CLR_DARK_GREY);

    // stage backdrop (an LCD-ish panel)
    rectfill(ORIGIN_X - 8, ORIGIN_Y - 8, 3 * 6 * PX + 16, 3 * 6 * PX + 16, CLR_DARKER_GREY);
    int ink = snap ? CLR_MEDIUM_GREY : CLR_GREEN;

    if      (scene == SCENE_DOG)  draw_figure(dog,  D_COUNT, ink);
    else if (scene == SCENE_BIRD) draw_figure(bird, B_COUNT, ink);
    else                          draw_figure(pup,  C_COUNT, ink);
    draw_particles();

    draw_bank();

    // status + controls
    const char *names[SCENE_COUNT] = { "DUDE", "DOG", "BIRD" };
    int rx = SCREEN_W - 128;
    print(str("cast: %s", names[scene]), rx, 40, CLR_WHITE);
    print(str("GRID-SNAP %s", snap ? "ON " : "off"), rx, 52, snap ? CLR_YELLOW : CLR_DARK_GREY);
    print(snap ? " -> dead clockwork" : " -> it breathes", rx, 62, CLR_DARK_GREY);
    print(str("metro %s  %d bpm", automet ? "on" : "off", tempo), rx, 76, CLR_LIGHT_GREY);

    print("SPACE/click kick  S snare  V cast", 8, SCREEN_H - 20, CLR_DARK_GREY);
    print("G snap   A metro   <- -> tempo", 8, SCREEN_H - 12, CLR_DARK_GREY);
}
