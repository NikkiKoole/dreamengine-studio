/* de:meta
{
  "slug": "platform-rects",
  "title": "platform engine (rects)",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "tilemap-collision",
    "screen-shake-juice"
  ],
  "lineage": "Classic PICO-8-style platformer reference build; novel in that it exposes every feel-assist (coyote time, jump buffer, variable jump, floaty apex, squash/stretch) as hot-reloadable #defines so the learner can feel each one in isolation.",
  "genre": "platformer",
  "description": "A pixel-perfect platformer feel built from nothing but colored rects. The level is an array of solid boxes; the player moves with floats and resolves one axis at a time. Includes coyote time, jump buffering, variable jump height, squash/stretch, and one-way platforms (the lighter ledges — jump up through them, land on top). Press X to toggle the feel assists and feel the difference. Tuning lives in #defines at the top: run it in live mode, edit GRAV/JUMP_V/COYOTE and hot-reload in place. Arrows/WASD move, Z or up to jump."
}
de:meta */
#include "studio.h"

// PLATFORM ENGINE (rect-list) — a pixel-perfect platformer feel built from
// nothing but colored rects. The level is an array of solid boxes; the player
// is a box that moves with floats and resolves ONE AXIS AT A TIME.
//
// HOT-RELOAD WORKFLOW: run this in "live" mode, then edit any TUNING value
// below and save — the cart recompiles in place while your position carries
// across (state lives in STATE/S, which survives a reload). Tweak GRAV, JUMP_V,
// COYOTE… and feel the change instantly. Press B in-game to toggle the feel
// assists on/off so you can feel exactly what each one buys.
//
// Move: arrows / WASD.   Jump: Z or Up.   Toggle feel: X (B button).
//
// ── THE FEEL CONCEPTS ──────────────────────────────────────────────────────
// These are invisible "forgiveness" timers. A player never sees them, but a
// game without them feels broken — like it ignored your button press. The
// on-screen readout (coyote / buffer / grounded) lets you watch them tick.
//
//  • PER-AXIS RESOLUTION — move fully along X and resolve, THEN fully along Y.
//    Splitting the axes is what stops you snagging on corners and gives clean
//    wall / floor / ceiling contact. This is the heart of the mover below.
//
//  • COYOTE TIME (named after the cartoon coyote who hangs over the cliff
//    edge): for a few frames AFTER you walk off a ledge you can still jump. Without
//    it, a jump pressed a hair too late gets eaten and the controls feel dead.
//
//  • JUMP BUFFERING — the mirror image: a jump pressed a few frames BEFORE you
//    land is remembered and fires the instant you touch down, instead of being
//    dropped for "you weren't on the ground yet."
//
//  • VARIABLE JUMP HEIGHT — tap for a small hop, hold for a full jump. Releasing
//    while still rising cuts your upward speed (CUT_JUMP).
//
//  • FLOATY APEX — gravity eases off at the top of the arc (APEX_GRAV) so the
//    peak of a jump hangs a moment. Makes airtime feel deliberate, not slippery.
//
// Press X (feel toggle) to turn all of this OFF and feel the strict version:
// run off a ledge and jump late, or jump just before landing — the jumps get
// eaten. That gap is exactly what these assists buy.

// ── friendly state sugar — survives a live reload ──────────────────────────
#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload to feel the difference ──────────────────────
#define GRAV       0.50f   // downward pull added to vy each frame
#define MAX_FALL   7.0f    // terminal fall speed (cap)
#define RUN_ACCEL  0.7f    // how fast you reach top speed
#define RUN_FRIC   0.55f   // ground stop (×vx when no input) — lower = snappier
#define AIR_FRIC   0.90f   // air stop — higher = more floaty air control
#define RUN_MAX    2.6f    // top horizontal speed
#define JUMP_V    -8.2f    // jump impulse (negative = upward)
#define COYOTE     6       // frames you can still jump after walking off a ledge
#define BUFFER     6       // frames a jump press is remembered before you land
#define CUT_JUMP   0.40f   // release jump while rising → keep this much upward speed
#define APEX_GRAV  0.55f   // gravity ×this near the top of a jump (floaty apex)
#define APEX_BAND  1.2f    // |vy| under this counts as "at the apex"

#define PW 10              // player width  (px)
#define PH 14              // player height (px)

STATE {
    float px, py;          // position (float = smooth sub-pixel motion)
    float vx, vy;          // velocity
    int   face;            // 1 = right, -1 = left
    bool  grounded;        // standing on a solid this frame?
    int   coyote, buffer;  // the two jump-feel timers
    float squash;          // >0 land-squash, <0 takeoff-stretch
    bool  feel;            // are the feel assists on?
};

// ── the level: solid boxes, drawn as plain colored rects ───────────────────
// `oneway` platforms (drawn lighter) only stop you from ABOVE: you jump up
// through them and land on top, but never bonk your head or get blocked sideways.
typedef struct { int x, y, w, h; bool oneway; } Box;
static const Box solids[] = {
    {   0, 186, 320,  14, false },   // ground
    {   0,   0,   8, 200, false },   // left wall
    { 312,   0,   8, 200, false },   // right wall
    {  40, 150,  64,   8, false },
    {   8, 104,  44,   8, false },
    {  92,  62,  48,   8, false },
    { 148, 120,  56,   8, true  },   // one-way: jump up through, land on top
    { 204,  82,  48,   8, true  },   // one-way
    { 232, 150,  60,   8, false },
};
#define NSOLID ((int)(sizeof(solids) / sizeof(solids[0])))

// ═══ THE MOVER ═════════════════════════════════════════════════════════════
// The whole trick: move the box along X completely and resolve, THEN along Y
// completely and resolve. Splitting the axes is what stops you snagging on
// corners and gives clean wall / floor / ceiling contact. Copy these three
// functions into any cart — they only need the `solids[]` array above.

static bool overlap(float ax, float ay, int aw, int ah, Box b) {
    return ax < b.x + b.w && ax + aw > b.x &&
           ay < b.y + b.h && ay + ah > b.y;
}

static void move_x(void) {
    S->px += S->vx;
    for (int i = 0; i < NSOLID; i++) {
        if (solids[i].oneway) continue;                            // pass through sideways
        if (!overlap(S->px, S->py, PW, PH, solids[i])) continue;
        if      (S->vx > 0) S->px = solids[i].x - PW;              // wall on the right
        else if (S->vx < 0) S->px = solids[i].x + solids[i].w;     // wall on the left
        S->vx = 0;
    }
}

static void move_y(void) {
    float feet_before = S->py + PH;               // where our feet were BEFORE moving
    S->py += S->vy;
    S->grounded = false;
    for (int i = 0; i < NSOLID; i++) {
        if (!overlap(S->px, S->py, PW, PH, solids[i])) continue;
        if (solids[i].oneway) {
            // only catch us landing on top: must be falling, and our feet must
            // have been at or above the platform's surface a frame ago (so we
            // came from above — not rising up through it).
            if (S->vy > 0 && feet_before <= solids[i].y) {
                S->py = solids[i].y - PH;
                S->grounded = true;
                S->vy = 0;
            }
            continue;                             // otherwise fall/jump right through
        }
        if (S->vy > 0) {                          // falling → we landed on top
            S->py = solids[i].y - PH;
            S->grounded = true;
        } else if (S->vy < 0) {                   // rising → bonked our head
            S->py = solids[i].y + solids[i].h;
        }
        S->vy = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════

void init(void) {
    S->px = 60; S->py = 150;
    S->face = 1;
    S->feel = true;
}

void update(void) {
    if (btnp(0, BTN_B)) S->feel = !S->feel;       // toggle the assists live
    bool feel = S->feel;

    // ── horizontal: accelerate toward input, else glide to a stop ──────────
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) {
        S->vx = clamp(S->vx + dir * RUN_ACCEL, -RUN_MAX, RUN_MAX);
        S->face = dir;
    } else {
        S->vx *= S->grounded ? RUN_FRIC : AIR_FRIC;
        if (S->vx > -0.1f && S->vx < 0.1f) S->vx = 0;
    }

    // ── jump feel: coyote time, input buffer, variable height ──────────────
    bool was_gr = S->grounded;
    if (was_gr)               S->coyote = feel ? COYOTE : 0;
    else if (S->coyote > 0)   S->coyote--;

    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) S->buffer = feel ? BUFFER : 1;
    else if (S->buffer > 0)                S->buffer--;

    if (S->buffer > 0 && (S->grounded || S->coyote > 0)) {
        S->vy = JUMP_V;
        S->buffer = 0; S->coyote = 0;
        S->squash = -0.7f;                        // stretch tall on takeoff
        note(72, INSTR_TRI, 3);
    }
    // release the button early while still rising → cut the jump short
    if (feel && (btnr(0, BTN_A) || btnr(0, BTN_UP)) && S->vy < 0)
        S->vy *= CUT_JUMP;

    // ── gravity (lighter near the apex so the top of the arc hangs) ────────
    float g = GRAV;
    if (feel && !S->grounded && S->vy > -APEX_BAND && S->vy < APEX_BAND)
        g *= APEX_GRAV;
    S->vy += g;
    if (S->vy > MAX_FALL) S->vy = MAX_FALL;

    // ── move & resolve, one axis at a time ─────────────────────────────────
    move_x();
    move_y();

    // landing feedback: squash flat + a small kick on touchdown
    if (!was_gr && S->grounded) {
        S->squash = 1.0f;
        note(48, INSTR_NOISE, 2);
        shake(0.6f);
    }
    S->squash = lerp(S->squash, 0.0f, 0.22f);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    for (int i = 0; i < NSOLID; i++) {
        if (solids[i].oneway)                     // lighter = jump up through it
            rectfill(solids[i].x, solids[i].y, solids[i].w, solids[i].h, CLR_PEACH);
        else
            rectfill(solids[i].x, solids[i].y, solids[i].w, solids[i].h, CLR_BROWN);
    }

    // player — squash/stretch deforms the box but keeps the feet planted
    int sq = (int)(S->squash * 4.0f);             // -4..4 px
    int w  = PW + (sq > 0 ? sq : 0);              // wider when squashed
    int h  = PH - sq;                             // shorter squashed / taller stretched
    int x  = (int)(S->px + 0.5f) - (w - PW) / 2;
    int y  = (int)(S->py + 0.5f) + (PH - h);      // grow upward, feet stay put
    rectfill(x, y, w, h, CLR_GREEN);
    rectfill(x + (S->face > 0 ? w - 4 : 2), y + 3, 2, 2, CLR_BLACK);  // eye

    rectfill(0, 0, SCREEN_W, 12, CLR_DARKER_BLUE);
    print("arrows/WASD move   Z/up jump", 4, 3, CLR_WHITE);
    print(S->feel ? "feel: ON" : "feel: OFF",
          SCREEN_W - 64, 3, S->feel ? CLR_GREEN : CLR_RED);

    // the assists, made visible
    print(str("coyote %d   buffer %d   grounded %d",
              S->coyote, S->buffer, S->grounded ? 1 : 0), 4, 16, CLR_LIGHT_GREY);
}
