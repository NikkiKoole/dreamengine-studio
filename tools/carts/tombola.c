/* de:meta
{
  "slug": "tombola",
  "title": "Tombola",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "rigid-body",
    "collision-detection",
    "generative-sequencer"
  ],
  "lineage": "The OP-1 'tombola' module as a dreamengine cart (design/tombola.md). The FOURTH Box2D v3 cart (runtime/box2d/): a slowly rotating hexagonal drum full of note-balls under gravity — a note fires because a ball PHYSICALLY crosses the trigger line, never a hidden step sequencer. Balls on real rigid-body dynamics; the trigger is a b2 sensor; scale-quantised note wiring borrowed from `circlemachine`.",
  "description": {
    "summary": "A physics sequencer you drop notes into: balls tumble in a spinning drum and play their note when they cross the centre line — the rhythm is emergent, nothing is sequenced.",
    "detail": "Tap inside the drum to drop a note-ball; it falls, the slowly rotating hexagon carries it up one side and it tumbles back across the trigger line, sounding its note. Pick the next ball's pitch on the bottom strip (scale-locked, so nothing plays wrong). Ride the four encoders: SPIN (drum speed), GRAV (gravity), TONE (brightness) and GATE (note length). SCALE cycles the key; BUILD freezes the drum so you can place balls precisely; CLR empties it. The pattern falls out of where you put the balls plus the physics — the honest core is that every note-on IS a real ball crossing a sensor.",
    "controls": "Tap in drum = drop a ball · bottom strip = pick next note · four knobs = SPIN/GRAV/TONE/GATE · SCALE = cycle key · BUILD = freeze/edit · CLR (or C) = clear · R = reseed"
  },
  "todo": [
    "BUILD's ghost trajectory arcs — predict where a ball will fall from the same physics (the doc's honest-readout payoff); currently BUILD only freezes for precise placement",
    "flick-to-remove a single ball (needs stable per-ball ids under removal, not the append-only array)",
    "ball selected -> the four encoders edit THAT ball (pitch/velocity/size), per the paper design's contextual p-lock",
    "spec() asserting every note-on corresponds to a contact hit event (prove nothing is faked)",
    "try a polygon count / tilt as a feel knob (circle vs hex vs heptagon — the container-shape open question)"
  ]
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>

// TOMBOLA — the OP-1 module as a rigid-body cart. A hexagonal drum (a KINEMATIC
// body) turns slowly; note-balls (DYNAMIC bodies) tumble inside under gravity.
// The honest core: a note fires because a ball PHYSICALLY BOUNCES off the drum
// wall (a Box2D contact HIT event above a speed threshold) — harder bounce =
// louder. No step grid, no sequence — the rhythm is the emergent consequence of
// physics + where you dropped the balls + how fast the drum stirs them.
//   Box2D metres/y-up  <->  studio pixels/y-down, world origin AT the drum centre.

#define PPM   10.0f                       // pixels per metre
#define DCX   160                         // drum centre on screen (x)
#define DCY   122                         // drum centre on screen (y)
#define SX(wx)  (DCX + (int)((wx) * PPM))
#define SY(wy)  (DCY - (int)((wy) * PPM))

#define R       5.0f                       // drum radius (metres)
#define NHEX    6                          // hexagon walls
#define NPAD    6                          // inward PADDLES — scoop balls up, drop them once per turn
#define PADLEN  1.9f                       // paddle length (metres, reaching in from the wall)
#define BALLR   0.52f                      // note-ball radius (metres) — big enough that each hit reads
#define MAXBALLS 32
#define NSEL    8                          // pitch-selector strip width (degrees)
#define TAU     6.2831853f

// audio slot + the one voice
#define SL_VOICE 6

#define WALL_TAG  1000                     // drum body userData sentinel (balls tag 1..MAXBALLS)
#define HIT_SPEED 1.4f                     // min bounce speed (m/s) that sounds a note

// ── scales (semitone offsets from root, borrowed from circlemachine) ──
typedef struct { const char *name; int n; int iv[7]; } Scale;
static const Scale SCALES[] = {
    { "PENTA", 5, { 0, 3, 5, 7, 10 } },       // minor pentatonic — always pretty
    { "MAJOR", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "DORIAN",7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "BLUES", 6, { 0, 3, 5, 6, 7, 10 } },
};
#define NSCALE (int)(sizeof(SCALES)/sizeof(SCALES[0]))
#define ROOT   48                          // C3

// a warm, friendly colour per scale degree (bright = high)
static const int DEGCOL[NSEL] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
    CLR_BLUE, CLR_INDIGO, CLR_PINK, CLR_WHITE
};

typedef struct {
    b2BodyId  body;
    b2ShapeId shape;
    int       deg;        // scale degree (selector index)
    int       midi;
    int       flash;      // frame it last triggered (for the pulse)
} Ball;

static b2WorldId world;
static b2BodyId  drum;                     // the kinematic hexagon
static Ball      balls[MAXBALLS];
static int       nballs = 0;
static int       built = 0;

static int   scale = 0;                    // current scale index
static int   sel = 2;                      // selected degree for the next ball
static int   mode_build = 0;               // 0 = SPIN (perform), 1 = BUILD (frozen)

// encoders (0..1)
static float k_spin = 0.34f, k_grav = 0.60f, k_tone = 0.55f, k_gate = 0.35f;
static float ap_tone = -1;                 // last-applied filter (set-and-hold)

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// a degree of the current scale -> a MIDI note (~1.5 octaves of range)
static int deg_midi(int deg) {
    const Scale *s = &SCALES[scale];
    return ROOT + (deg / s->n) * 12 + s->iv[deg % s->n];
}

static void spawn_ball(float wx, float wy, int deg) {
    if (nballs >= MAXBALLS) return;
    Ball *b = &balls[nballs];
    b->deg = deg;
    b->midi = deg_midi(deg);
    b->flash = -999;

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){wx, wy};
    bd.isBullet = true;                            // fast tumble -> no tunnelling
    bd.userData = (void*)(intptr_t)(nballs + 1);   // 1..MAXBALLS (0 = not a ball; WALL_TAG = wall)
    b->body = b2CreateBody(world, &bd);

    b2Circle c = {{0, 0}, BALLR};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = 0.5f;
    sd.material.restitution = 0.45f;               // lively bounce -> a note per real impact
    sd.enableHitEvents = true;                     // fire notes off genuine collisions
    b->shape = b2CreateCircleShape(b->body, &sd, &c);
    nballs++;
}

static void build_world(void) {
    if (built) b2DestroyWorld(world);
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0, -12.0f};
    world = b2CreateWorld(&wd);
    b2World_SetHitEventThreshold(world, HIT_SPEED);   // only real bounces ring a note
    nballs = 0;
    built = 1;

    static int audio = 0;
    if (!audio) {
        instrument(SL_VOICE, INSTR_MALLET, 0, 220, 0, 260);   // struck bar — rings on its own
        reverb(0.72f, 0.35f);
        instrument_reverb(SL_VOICE, 0.35f);
        audio = 1;
        ap_tone = -1;                                          // force TONE re-apply
    }

    // the drum — a KINEMATIC hexagon; segments in local coords around the origin,
    // so setting angular velocity spins the whole cage about the drum centre.
    b2BodyDef dd = b2DefaultBodyDef();
    dd.type = b2_kinematicBody;
    dd.position = (b2Vec2){0, 0};
    dd.userData = (void*)(intptr_t)WALL_TAG;
    drum = b2CreateBody(world, &dd);
    for (int i = 0; i < NHEX; i++) {
        float a0 = (float)i / NHEX * TAU;
        float a1 = (float)(i + 1) / NHEX * TAU;
        b2Segment s = {{cosf(a0) * R, sinf(a0) * R}, {cosf(a1) * R, sinf(a1) * R}};
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.friction = 0.7f;               // grip so the wall CARRIES balls round
        sd.material.restitution = 0.2f;
        sd.enableHitEvents = true;                 // the wall a ball rings against
        b2CreateSegmentShape(drum, &sd, &s);
    }
    // the paddle-wheel: fins reaching inward from the rim. As the drum turns, a fin
    // sweeping the bottom SCOOPS the pile, carries balls up the rising side, and
    // spills them near the top — so each ball tumbles and rings once per turn. The
    // steady rotation makes that periodic: a loop, not a wash.
    for (int i = 0; i < NPAD; i++) {
        float a = ((float)i + 0.5f) / NPAD * TAU;      // mid-face, between the vertices
        b2Segment s = {{cosf(a) * R, sinf(a) * R},
                       {cosf(a) * (R - PADLEN), sinf(a) * (R - PADLEN)}};
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.friction = 0.8f;
        sd.material.restitution = 0.15f;
        sd.enableHitEvents = true;
        b2CreateSegmentShape(drum, &sd, &s);
    }
}

static void seed(void) {
    build_world();
    // a few balls, dropped near the bottom so the paddles pick them straight up —
    // spread the pitches so the emergent loop has a shape, not one note
    int seedDeg[4] = { 0, 2, 4, 5 };
    for (int i = 0; i < 4; i++)
        spawn_ball((i - 1.5f) * 1.1f, -1.0f, seedDeg[i]);
}

void update(void) {
    if (!built) seed();
    if (keyp('R')) { seed(); return; }
    if (keyp('C')) { build_world(); return; }

    // TONE (set-and-hold): the mallet's low-pass brightness
    if (fabsf(k_tone - ap_tone) > 0.005f) {
        instrument_filter(SL_VOICE, FILTER_LOW, 500.0f + k_tone * 5000.0f, 2);
        ap_tone = k_tone;
    }

    // GRAV rides live
    b2World_SetGravity(world, (b2Vec2){0, -(4.0f + k_grav * 18.0f)});

    // SPIN: the drum's angular velocity (0 in BUILD — frozen for placing balls).
    // ~0.5..4.7 rad/s -> a turn every ~12s..1.3s; the turn IS the loop's bar.
    float w = mode_build ? 0.0f : -(0.5f + k_spin * 4.2f);    // clockwise
    b2Body_SetAngularVelocity(drum, w);

    // drop a ball where you tap inside the drum (region-gated, so it never
    // fights the top-bar / knob / strip widgets which live outside the cage)
    if (mouse_pressed(MOUSE_LEFT)) {
        float wx = (mouse_x() - DCX) / PPM;
        float wy = (DCY - mouse_y()) / PPM;
        if (wx * wx + wy * wy < (R * 0.85f) * (R * 0.85f))
            spawn_ball(wx, wy, sel);
    }

    if (mode_build) return;                          // BUILD = frozen; no stepping
    b2World_Step(world, 1.0f / 60.0f, 4);

    // the honest core: a note fires because a ball BOUNCED (a real contact hit).
    // For each qualifying impact, ring the note of whichever body/bodies is a ball.
    b2ContactEvents ce = b2World_GetContactEvents(world);
    int gate = 60 + (int)(k_gate * 840);
    for (int i = 0; i < ce.hitCount; i++) {
        b2ContactHitEvent *h = &ce.hitEvents[i];
        if (h->approachSpeed < HIT_SPEED) continue;
        b2ShapeId pair[2] = { h->shapeIdA, h->shapeIdB };
        int vol = 3 + (int)clampf(h->approachSpeed * 0.5f, 0, 4);   // harder bounce = louder
        for (int k = 0; k < 2; k++) {
            int idx = (int)(intptr_t)b2Body_GetUserData(b2Shape_GetBody(pair[k]));
            if (idx < 1 || idx > nballs) continue;   // not a ball (wall = WALL_TAG)
            Ball *b = &balls[idx - 1];
            if (frame() - b->flash < 4) continue;    // de-bounce a single impact's twin contacts
            hit(b->midi, SL_VOICE, vol, gate);
            b->flash = frame();
        }
    }

#ifdef DE_TRACE
    watch("balls", "%d", nballs);
    watch("scale", "%d", scale);
    watch("build", "%d", mode_build);
#endif
}

// ── drawing ──
// transform a drum-local point (metres) to screen, through the drum's live rotation
static void drum_pt(b2Transform t, float lx, float ly, int *sx, int *sy) {
    float wx = t.p.x + (t.q.c * lx - t.q.s * ly);
    float wy = t.p.y + (t.q.s * lx + t.q.c * ly);
    *sx = SX(wx); *sy = SY(wy);
}

static void draw_drum(void) {
    b2Transform t = b2Body_GetTransform(drum);
    int px[NHEX], py[NHEX];
    for (int i = 0; i < NHEX; i++) {
        float a = (float)i / NHEX * TAU;
        drum_pt(t, cosf(a) * R, sinf(a) * R, &px[i], &py[i]);
    }
    for (int i = 0; i < NHEX; i++)
        line(px[i], py[i], px[(i + 1) % NHEX], py[(i + 1) % NHEX], CLR_INDIGO);
    // the paddles
    for (int i = 0; i < NPAD; i++) {
        float a = ((float)i + 0.5f) / NPAD * TAU;
        int ox, oy, ix, iy;
        drum_pt(t, cosf(a) * R, sinf(a) * R, &ox, &oy);
        drum_pt(t, cosf(a) * (R - PADLEN), sinf(a) * (R - PADLEN), &ix, &iy);
        line(ox, oy, ix, iy, CLR_BLUE);
    }
}

void draw(void) {
    if (!built) { cls(CLR_DARKER_PURPLE); return; }
    cls(CLR_DARKER_PURPLE);
    ui_begin();

    // ① nav spine
    if (ui_button(2, 2, 44, 11, mode_build ? "BUILD" : "SPIN")) mode_build = !mode_build;
    if (ui_button(50, 2, 60, 11, SCALES[scale].name)) scale = (scale + 1) % NSCALE;
    if (ui_button(278, 2, 40, 11, "CLR")) build_world();
    char hud[24];
    snprintf(hud, sizeof hud, "%d", nballs);
    print(hud, 200, 4, CLR_LIGHT_GREY);

    // ② four encoders
    ui_knob(&k_spin, 40, 26, "SPIN");
    ui_knob(&k_grav, 120, 26, "GRAV");
    ui_knob(&k_tone, 200, 26, "TONE");
    ui_knob(&k_gate, 280, 26, "GATE");

    // ③ the drum (the hero)
    draw_drum();
    for (int i = 0; i < nballs; i++) {
        Ball *b = &balls[i];
        b2Vec2 p = b2Body_GetPosition(b->body);
        int col = DEGCOL[b->deg % NSEL];
        int rr = (int)(BALLR * PPM);
        int lit = (frame() - b->flash) < 8;            // pulse just after it fired
        if (lit) circfill(SX(p.x), SY(p.y), rr + 2, CLR_WHITE);
        circfill(SX(p.x), SY(p.y), rr, col);
        circ(SX(p.x), SY(p.y), rr, lit ? CLR_WHITE : CLR_DARK_GREY);
    }

    // ⑤ pitch-selector strip (a SELECTOR — it chooses, the drum sounds)
    int bw = SCREEN_W / NSEL;
    for (int i = 0; i < NSEL; i++) {
        int x = i * bw;
        char lbl[4];
        snprintf(lbl, sizeof lbl, "%d", i + 1);
        if (ui_button(x, 178, bw - 1, 20, lbl)) sel = i;
        if (i == sel) rect(x + 1, 179, bw - 3, 17, CLR_WHITE);
        rectfill(x + 3, 180, 5, 5, DEGCOL[i]);          // the degree's colour swatch
    }

    ui_end();
}
