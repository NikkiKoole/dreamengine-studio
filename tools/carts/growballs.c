/* de:meta
{
  "title": "grow balls",
  "status": "active",
  "created": "2026-06-25",
  "kind": [
    "game"
  ],
  "teaches": [
    "collision-detection",
    "camera-follow",
    "verlet-integration"
  ],
  "lineage": "Built on the platform-rects per-axis mover; the twist is a single growing variable (ball radius from eaten fruit) that drives BOTH the character art and the jump physics — at threshold the jump becomes an auto-pogo whose impulse scales with size, so growth IS progression. The arms, legs, balls and willy are floppy verlet chains anchored to the body, so they ragdoll as he bounces (cosmetic — the collision/pogo still use the box). A gag cart with an emergent difficulty curve (bigger balls reach higher fruit which grow bigger balls).",
  "genre": "platformer",
  "description": "A silly pogo-platformer. You're a little naked cartoon guy with visible balls — eat the fruit and veg scattered through the world and your balls GROW. Tiny balls only manage a small hop, but once they get big enough they turn into a spring: land and you POGO, and the bigger your balls, the higher you bounce. Steer in the air to land on the next platform (and the next snack), keep growing, and pogo a tower of one-way platforms all the way to the MOON. Arrows/WASD move, Z or Up to jump / super-bounce (tap on landing), hold Down to rest on a platform, R to restart."
}
de:meta */
#include "studio.h"

// GROW BALLS — a silly side-view pogo-platformer. You are a little naked cartoon
// guy (sprite, with a fine head of hair). EAT the fruit & veg scattered up a tall
// tower — every bite makes your balls bigger and hairier.
//
// Small balls: only a tiny hop. Once they are big enough they become a SPRING:
// land and you POGO, and the bigger your balls the higher you bounce. Steer in
// the air to land on the next platform (and the next snack). The tower gets
// taller and the leaps get bigger as you climb. The MOON is only reachable from
// the very top — and ONLY once you have eaten EVERY last vegetable. 🌕
//
// Move: arrows / WASD.   Jump / super-bounce: Z or Up (tap on landing).
// Rest on a platform: hold Down.   Restart: R.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING ─────────────────────────────────────────────────────────────────
#define GRAV       0.42f
#define MAX_FALL   12.0f
#define RUN_ACCEL  0.6f
#define RUN_FRIC   0.55f
#define AIR_FRIC   0.94f
#define RUN_MAX    2.8f
#define SMALL_JUMP -6.0f
#define COYOTE     6
#define BUFFER     8
#define POGO_MIN   8.0f    // ball radius before the pogo spring works
#define POGO_BASE  3.5f
#define POGO_K     0.55f   // bounce speed per pixel of ball radius
#define SUPER      1.25f   // timed-jump-on-landing multiplier
#define BALL_MIN   3.0f
#define BALL_MAX   34.0f

#define BODY_W 12
#define BODY_H 16
#define WORLD_W 480
#define PWID   92          // platform width
#define GROUND_H 16
#define GROUND_FRUIT 5     // snacks on the ground
#define NPLAT 18           // platforms in the tower (lots of verticality!)
#define MOON_R 44
#define MOON_TOP 80        // moon centre y (near the world top)

#define MAXF 64
#define MAXS 64

// ── floppy ragdoll bits (verlet) ─────────────────────────────────────────────
// Purely cosmetic: arms/legs/balls/willy dangle off the body and flop as the guy
// bounces. The collision + pogo still use the box (cw,ch) — this never touches it.
typedef struct { float x, y, px, py; bool pin; } VPt;
enum { LSH, LELB, LHAND, RSH, RELB, RHAND,           // arms
       LHIP, LKNEE, LFOOT, RHIP, RKNEE, RFOOT,       // legs
       CROTCH, LBALL, RBALL, WILLY, NPT };           // crotch + balls + willy
#define VGRAV  0.55f
#define VDAMP  0.96f

STATE {
    bool  started, won;
    float px, py;          // top-left of the collision box (body + balls)
    float vx, vy;
    int   cw, ch;
    int   face;
    bool  grounded;
    int   coyote, buffer;
    float squash;
    int   eaten;
    bool  gone[MAXF];
    float ptx[48], pty[48], pvx[48], pvy[48], plife[48]; int pcol[48];
    VPt   vp[NPT];         // ragdoll points
    long  mstep;           // last scheduled 16th-note step (BGM clock)
    bool  unlocked;        // has the pogo spring been unlocked yet?
    float unlockT;         // "POGO POWER!" banner timer
    float rx[6], ry[6], rt[6];   // pop-ring effects (x, y, age 0→1)
};

typedef struct { int x, y, w, h; bool oneway; } Box;
typedef struct { int x, y, type; } Fruit;

// ── the procedurally-built world (rebuilt deterministically in init) ─────────
static Box   solids[MAXS]; static int NSOLID;
static Fruit fruits[MAXF]; static int NFRUIT;
static int   WORLD_H, MOON_X, MOON_Y, GROUND_TOP;

static float ballr_for(int eaten) {
    float step = (BALL_MAX - BALL_MIN) / (float)(GROUND_FRUIT + NPLAT);
    float r = BALL_MIN + eaten * step;
    return r > BALL_MAX ? BALL_MAX : r;
}
static float ball_r(void) { return ballr_for(S->eaten); }

// the vertical gap a pogo can clear with `eaten` fruit (with margin)
static float gap_for(int eaten) {
    float r = ballr_for(eaten);
    float v = POGO_BASE + r * POGO_K;
    float H = v * v / (2.0f * GRAV);
    float g = 0.66f * H;
    if (g < 60.0f)  g = 60.0f;
    if (g > 340.0f) g = 340.0f;
    return g;
}

static void build_world(void) {
    NFRUIT = GROUND_FRUIT + NPLAT;

    float relY[NPLAT]; int xs[NPLAT], ws[NPLAT];
    float rel  = gap_for(GROUND_FRUIT);          // p0 sits this far above ground
    float prevx = WORLD_W / 2.0f;
    int   dir   = 1;
    for (int k = 0; k < NPLAT; k++) {
        relY[k] = rel;
        float r    = ballr_for(GROUND_FRUIT + k);
        // wide enough to land the (growing) balls on, never below PWID
        int pw = 2 * (int)r + 44; if (pw < PWID) pw = PWID;
        ws[k] = pw;
        // horizontal offset: a fraction of how far you can travel in the airtime
        float v     = POGO_BASE + r * POGO_K;
        float air   = 2.0f * v / GRAV;
        float reach = RUN_MAX * air * 0.42f;
        float nx = prevx + dir * reach;
        if (nx < 40 || nx > WORLD_W - 40 - pw) { dir = -dir; nx = prevx + dir * reach; }
        nx = clamp(nx, 40, WORLD_W - 40 - pw);
        xs[k] = (int)nx; prevx = nx; dir = -dir;
        rel += gap_for(GROUND_FRUIT + k + 1);    // gap up to the next platform
    }
    float moon_rel = rel;                         // top of the climb (incl. final gap)

    MOON_Y     = MOON_TOP;
    MOON_X     = xs[NPLAT - 1] + ws[NPLAT - 1] / 2;  // straight up off the top ledge
    GROUND_TOP = (int)(MOON_TOP + moon_rel);
    WORLD_H    = GROUND_TOP + GROUND_H;

    int s = 0;
    solids[s++] = (Box){ 0, GROUND_TOP, WORLD_W, GROUND_H, false };  // ground
    solids[s++] = (Box){ -8, 0, 8, WORLD_H, false };                 // walls
    solids[s++] = (Box){ WORLD_W, 0, 8, WORLD_H, false };
    for (int k = 0; k < NPLAT; k++) {
        int wy = (int)(MOON_TOP + (moon_rel - relY[k]));
        solids[s++] = (Box){ xs[k], wy, ws[k], 12, true };
    }
    NSOLID = s;

    int f = 0;
    for (int g = 0; g < GROUND_FRUIT; g++) {
        int fx = 50 + g * (WORLD_W - 100) / (GROUND_FRUIT - 1);
        fruits[f++] = (Fruit){ fx, GROUND_TOP - 1, g % 5 };
    }
    for (int k = 0; k < NPLAT; k++) {
        int wy = (int)(MOON_TOP + (moon_rel - relY[k]));
        fruits[f++] = (Fruit){ xs[k] + ws[k] / 2, wy - 1, (k + 2) % 5 };
    }
    NFRUIT = f;
}

static void resize_box(void) {
    int r   = (int)ball_r();
    int ncw = BODY_W > 2 * r ? BODY_W : 2 * r;
    int nch = BODY_H + 2 * r;
    float bcx = S->px + S->cw * 0.5f;
    float feet = S->py + S->ch;
    S->cw = ncw; S->ch = nch;
    S->px = bcx - ncw * 0.5f;
    S->py = feet - nch;
}

// ═══ MOVER — per-axis, sub-stepped so fast bounces never tunnel ══════════════
static bool overlap(float ax, float ay, int aw, int ah, Box b) {
    return ax < b.x + b.w && ax + aw > b.x && ay < b.y + b.h && ay + ah > b.y;
}
static void move_x(void) {
    float rem = S->vx;
    while (rem != 0.0f) {
        float st = rem > 3 ? 3 : (rem < -3 ? -3 : rem);
        S->px += st; rem -= st;
        for (int i = 0; i < NSOLID; i++) {
            if (solids[i].oneway) continue;
            if (!overlap(S->px, S->py, S->cw, S->ch, solids[i])) continue;
            if      (S->vx > 0) S->px = solids[i].x - S->cw;
            else if (S->vx < 0) S->px = solids[i].x + solids[i].w;
            S->vx = 0; rem = 0; break;
        }
    }
}
static void move_y(void) {
    float rem = S->vy;
    S->grounded = false;
    while (rem != 0.0f) {
        float feet_before = S->py + S->ch;
        float st = rem > 3 ? 3 : (rem < -3 ? -3 : rem);
        S->py += st; rem -= st;
        for (int i = 0; i < NSOLID; i++) {
            if (!overlap(S->px, S->py, S->cw, S->ch, solids[i])) continue;
            if (solids[i].oneway) {
                if (S->vy > 0 && feet_before <= solids[i].y + 1) {
                    S->py = solids[i].y - S->ch;
                    S->grounded = true; S->vy = 0; rem = 0;
                }
                continue;
            }
            if (S->vy > 0) { S->py = solids[i].y - S->ch; S->grounded = true; }
            else if (S->vy < 0) S->py = solids[i].y + solids[i].h;
            S->vy = 0; rem = 0; break;
        }
        if (rem == 0.0f) break;
    }
}

static void spawn(float x, float y, int col, int n, float spread, float up) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < 48; i++) {
            if (S->plife[i] > 0) continue;
            S->ptx[i] = x; S->pty[i] = y;
            S->pvx[i] = rnd_float_between(-1.0f, 1.0f) * spread;
            S->pvy[i] = rnd_float_between(-1.0f, 1.0f) * spread - up;
            S->plife[i] = 0.6f + rnd_float_between(0.0f, 0.5f);
            S->pcol[i]  = col; break;
        }
}

// ── verlet ragdoll ───────────────────────────────────────────────────────────
static void vintegrate(VPt *p) {
    if (p->pin) return;
    float vx = (p->x - p->px) * VDAMP, vy = (p->y - p->py) * VDAMP;
    vx = clamp(vx, -16, 16); vy = clamp(vy, -16, 16);   // keep big bounces stable
    p->px = p->x; p->py = p->y;
    p->x += vx; p->y += vy + VGRAV;
}
static void vconstr(int a, int b, float rest) {
    VPt *pa = &S->vp[a], *pb = &S->vp[b];
    float dx = pb->x - pa->x, dy = pb->y - pa->y;
    float d = fsqrt(dx * dx + dy * dy); if (d < 0.001f) d = 0.001f;
    float diff = (d - rest) / d;
    float wa = pa->pin ? 0.0f : 1.0f, wb = pb->pin ? 0.0f : 1.0f;
    float sum = wa + wb; if (sum == 0) return;
    wa /= sum; wb /= sum;
    pa->x += dx * diff * wa; pa->y += dy * diff * wa;
    pb->x -= dx * diff * wb; pb->y -= dy * diff * wb;
}
// angular spring (à la ragdoll.c): nudge bone a→b toward direction (tx,ty).
// Used to keep the legs straight & pointing down instead of folding at the knee.
static void vangsp(int ai, int bi, float tx, float ty, float str) {
    VPt *a = &S->vp[ai], *b = &S->vp[bi];
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = fsqrt(dx * dx + dy * dy); if (d < 0.001f) return;
    float cx = dx / d, cy = dy / d;
    if (cx * tx + cy * ty <= 0.0f) return;          // >90° off: let distance springs handle it
    float cross = cx * ty - cy * tx;
    if (!b->pin) { b->x += -cy * cross * str; b->y += cx * cross * str; }
    if (!a->pin) { a->x -= -cy * cross * str; a->y -= cx * cross * str; }
}
static void init_verlet(void) {
    float cx = S->px + S->cw * 0.5f, by = S->py, r = ball_r();
    S->vp[LSH]    = (VPt){ cx - 4, by + 7,  0, 0, true };
    S->vp[RSH]    = (VPt){ cx + 4, by + 7,  0, 0, true };
    S->vp[LHIP]   = (VPt){ cx - 3, by + 13, 0, 0, true };
    S->vp[RHIP]   = (VPt){ cx + 3, by + 13, 0, 0, true };
    S->vp[CROTCH] = (VPt){ cx,     by + 16, 0, 0, true };
    S->vp[LELB]   = (VPt){ cx - 5, by + 11, 0, 0, false };
    S->vp[LHAND]  = (VPt){ cx - 6, by + 16, 0, 0, false };
    S->vp[RELB]   = (VPt){ cx + 5, by + 11, 0, 0, false };
    S->vp[RHAND]  = (VPt){ cx + 6, by + 16, 0, 0, false };
    S->vp[LKNEE]  = (VPt){ cx - 3, by + 19, 0, 0, false };
    S->vp[LFOOT]  = (VPt){ cx - 3, by + 24, 0, 0, false };
    S->vp[RKNEE]  = (VPt){ cx + 3, by + 19, 0, 0, false };
    S->vp[RFOOT]  = (VPt){ cx + 3, by + 24, 0, 0, false };
    S->vp[LBALL]  = (VPt){ cx - r * 0.5f, by + 16 + r, 0, 0, false };
    S->vp[RBALL]  = (VPt){ cx + r * 0.5f, by + 16 + r, 0, 0, false };
    S->vp[WILLY]  = (VPt){ cx, by + 19, 0, 0, false };
    for (int i = 0; i < NPT; i++) { S->vp[i].px = S->vp[i].x; S->vp[i].py = S->vp[i].y; }
}
static void vphysics(void) {
    float cx = S->px + S->cw * 0.5f, by = S->py, r = ball_r();
    S->vp[LSH].x    = cx - 4; S->vp[LSH].y    = by + 7;     // anchors track the body
    S->vp[RSH].x    = cx + 4; S->vp[RSH].y    = by + 7;
    S->vp[LHIP].x   = cx - 3; S->vp[LHIP].y   = by + 13;
    S->vp[RHIP].x   = cx + 3; S->vp[RHIP].y   = by + 13;
    S->vp[CROTCH].x = cx;     S->vp[CROTCH].y = by + 16;
    for (int i = 0; i < NPT; i++) vintegrate(&S->vp[i]);
    for (int it = 0; it < 4; it++) {
        vconstr(LSH, LELB, 5);  vconstr(LELB, LHAND, 5);
        vconstr(RSH, RELB, 5);  vconstr(RELB, RHAND, 5);
        vconstr(LHIP, LKNEE, 6); vconstr(LKNEE, LFOOT, 6);
        vconstr(RHIP, RKNEE, 6); vconstr(RKNEE, RFOOT, 6);
        // keep the legs stiff & straight down (knee doesn't fold, only a little sway)
        vangsp(LHIP, LKNEE, 0, 1, 0.75f); vangsp(LKNEE, LFOOT, 0, 1, 0.85f);
        vangsp(RHIP, RKNEE, 0, 1, 0.75f); vangsp(RKNEE, RFOOT, 0, 1, 0.85f);
        vconstr(CROTCH, LBALL, r); vconstr(CROTCH, RBALL, r);
        vconstr(LBALL, RBALL, r * 1.1f);
        vconstr(CROTCH, WILLY, 4 + r * 0.16f);
    }
    // extra damping on the leg joints so they settle fast (less floppy)
    int legs[] = { LKNEE, LFOOT, RKNEE, RFOOT };
    for (int i = 0; i < 4; i++) {
        VPt *p = &S->vp[legs[i]];
        p->px += (p->x - p->px) * 0.35f;   // bleed off most of the swing velocity
    }
}

// ── background music: a bouncy major-pentatonic chiptune (always pretty), ────
// sequenced one 16th-note ahead off the engine beat clock so it never jitters.
#define TEMPO 138
#define REST  -99
// two bars of 16ths. lead = pentatonic degree (oct 5), bass = degree (oct 3).
static const int LEAD[32] = {
    0, REST, 4, REST,  7, REST, 4, 2,    3, REST, 4, REST, 2, REST, REST, REST,
    4, REST, 7, REST,  9, REST, 7, 4,    3, REST, 2, REST, 0, REST,  4,   REST,
};
static const int BASS[32] = {
    0, REST, REST, REST, 0, REST, REST, REST, 3, REST, REST, REST, 3, REST, REST, REST,
    4, REST, REST, REST, 4, REST, REST, REST, 0, REST, REST, REST, 0, REST,  4,  REST,
};

static void play_step(long abs, double pos, bool big) {
    double stepMs = 60000.0 / (TEMPO * 4);
    int dly = (int)((abs - pos) * stepMs); if (dly < 1) dly = 1;
    int s = (int)(abs % 32), s16 = s % 16;
    if (LEAD[s] != REST) schedule_hit(dly, degree(SCALE_PENTA, 5, LEAD[s]), INSTR_MALLET, 4, 150);
    if (BASS[s] != REST) schedule_hit(dly, degree(SCALE_PENTA, 3, BASS[s]), INSTR_PLUCK, 5, 200);
    if (s16 == 0 || s16 == 8)  schedule_hit(dly, 36, INSTR_MEMBRANE, 5, 90);   // kick
    if (s16 % 4 == 2)          schedule_hit(dly, 64, INSTR_NOISE, 1, 18);      // soft hat
    // when the balls are big, a sparkle octave layer rides on top (juice)
    if (big && LEAD[s] != REST) schedule_hit(dly + 30, degree(SCALE_PENTA, 6, LEAD[s]), INSTR_MALLET, 2, 90);
}

static void ring_add(float x, float y) {
    for (int i = 0; i < 6; i++) if (S->rt[i] <= 0) { S->rx[i] = x; S->ry[i] = y; S->rt[i] = 0.001f; return; }
}

static void reset(void) {
    for (int i = 0; i < 48; i++) S->plife[i] = 0;
    for (int i = 0; i < 6; i++)  S->rt[i] = 0;
    S->mstep = -1; S->unlocked = false; S->unlockT = 0;
    for (int i = 0; i < NFRUIT; i++) S->gone[i] = false;
    S->eaten = 0; S->won = false;
    S->vx = S->vy = 0; S->face = 1; S->squash = 0;
    S->cw = BODY_W; S->ch = BODY_H + 2 * (int)BALL_MIN;
    S->px = 60; S->py = GROUND_TOP - S->ch;
    init_verlet();
}

void init(void) {
    colorkey(0);            // index 0 = transparent for the guy/fruit sprites
    bpm(TEMPO);
    reverb(0.35f, 0.5f);
    instrument_reverb(INSTR_MALLET, 0.18f);   // a little air on the lead
    build_world();
    S->started = false;
    reset();
}

void update(void) {
    if (!S->started) {
        if (btnp(0, BTN_A) || btnp(0, BTN_UP) || btnp(0, BTN_RIGHT) || btnp(0, BTN_LEFT))
            S->started = true;
        return;
    }
    if (keyp('r') || keyp('R')) { reset(); return; }

    float r = ball_r();
    bool pogo_ready = r >= POGO_MIN;

    // POGO POWER! — the moment the balls first become a working spring
    if (pogo_ready && !S->unlocked) {
        S->unlocked = true; S->unlockT = 2.2f;
        for (int n = 0; n < 6; n++) schedule(n * 70, degree(SCALE_PENTA, 5, n), INSTR_MALLET, 5);
        ring_add(S->px + S->cw * 0.5f, S->py + S->ch);
        shake(1.6f);
    }

    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) { S->vx = clamp(S->vx + dir * RUN_ACCEL, -RUN_MAX, RUN_MAX); S->face = dir; }
    else { S->vx *= S->grounded ? RUN_FRIC : AIR_FRIC; if (S->vx > -0.1f && S->vx < 0.1f) S->vx = 0; }

    bool was_gr = S->grounded;
    if (was_gr)             S->coyote = COYOTE;
    else if (S->coyote > 0) S->coyote--;
    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) S->buffer = BUFFER;
    else if (S->buffer > 0)                S->buffer--;
    bool hold_down = btn(0, BTN_DOWN);

    S->vy += GRAV;
    if (S->vy > MAX_FALL) S->vy = MAX_FALL;
    move_x();
    move_y();

    bool landed    = (!was_gr && S->grounded);
    bool want_jump = S->buffer > 0;
    bool can_grnd  = S->grounded || S->coyote > 0;
    if (pogo_ready) {
        if (!hold_down && (landed || (want_jump && can_grnd))) {
            float v = POGO_BASE + r * POGO_K;
            int pitch = 84 - (int)r;
            note(pitch < 36 ? 36 : pitch, INSTR_TRI, 4);                 // the boing
            if (want_jump) {                                            // timed = SUPER bounce
                v *= SUPER; S->buffer = 0;
                schedule(0, 76, INSTR_MALLET, 5); schedule(70, 83, INSTR_MALLET, 5);  // wahoo!
                ring_add(S->px + S->cw * 0.5f, S->py + S->ch);
            }
            S->vy = -v; S->grounded = false; S->coyote = 0;
            S->squash = 1.0f;
            spawn(S->px + S->cw * 0.5f, S->py + S->ch, CLR_LIGHT_GREY, 6, 1.6f, 0.4f);
            shake(clamp(v * 0.06f, 0.4f, 2.2f));
        } else if (landed) {
            S->squash = 0.7f;
            spawn(S->px + S->cw * 0.5f, S->py + S->ch, CLR_LIGHT_GREY, 3, 1.0f, 0.2f);
        }
    } else {
        if (landed) {
            S->squash = 0.7f;
            spawn(S->px + S->cw * 0.5f, S->py + S->ch, CLR_LIGHT_GREY, 3, 1.0f, 0.2f);
        }
        if (want_jump && can_grnd) {
            S->vy = SMALL_JUMP; S->buffer = 0; S->coyote = 0; S->squash = -0.5f;
            note(64, INSTR_TRI, 3);
        }
    }
    S->squash = lerp(S->squash, 0.0f, 0.18f);

    for (int i = 0; i < NFRUIT; i++) {
        if (S->gone[i]) continue;
        int fx = fruits[i].x, fy = fruits[i].y - 8;
        if (fx > S->px - 6 && fx < S->px + S->cw + 6 &&
            fy > S->py - 6 && fy < S->py + S->ch + 6) {
            S->gone[i] = true; S->eaten++; resize_box();
            note(72 + (S->eaten % 8), INSTR_SQUARE, 3);
            note(79 + (S->eaten % 6), INSTR_TRI, 2);
            spawn(fruits[i].x, fruits[i].y - 8, CLR_PINK, 8, 2.2f, 0.6f);
            spawn(fruits[i].x, fruits[i].y - 8, CLR_GREEN, 6, 1.8f, 0.8f);
            ring_add(fruits[i].x, fruits[i].y - 8);
            shake(0.5f);
        }
    }

    for (int i = 0; i < 48; i++) {
        if (S->plife[i] <= 0) continue;
        S->ptx[i] += S->pvx[i]; S->pty[i] += S->pvy[i];
        S->pvy[i] += 0.12f; S->plife[i] -= 0.02f;
    }

    vphysics();              // flop the ragdoll bits (cosmetic)

    // background music — schedule one 16th-note ahead off the beat clock
    double mpos = (double)beat() * 4.0 + beat_pos() * 4.0;
    if (S->mstep < 0) S->mstep = (long)mpos;        // sync to "now" on first tick
    long mtarget = (long)mpos + 1;
    while (S->mstep < mtarget) { S->mstep++; play_step(S->mstep, mpos, pogo_ready); }

    // pop rings + unlock banner timers
    for (int i = 0; i < 6; i++) if (S->rt[i] > 0) { S->rt[i] += 0.06f; if (S->rt[i] >= 1) S->rt[i] = 0; }
    if (S->unlockT > 0) S->unlockT -= 0.016f;

    // WIN: every veg eaten AND you've bounced up to the moon's altitude
    if (!S->won && S->eaten >= NFRUIT && S->py <= MOON_Y + MOON_R) {
        S->won = true;
        for (int n = 0; n < 5; n++) note(72 + n * 3, INSTR_TRI, 4);
        spawn(S->px + S->cw * 0.5f, S->py, CLR_YELLOW, 24, 3.0f, 0.0f);
        shake(3.0f);
    }
#ifdef DE_TRACE
    watch("py", "%.0f", S->py);
    watch("vy", "%.1f", S->vy);
    watch("r",  "%.0f", ball_r());
    watch("eaten", "%d", S->eaten);
#endif
}

// ── the guy: sprite body + floppy verlet limbs / balls / willy ───────────────
static void ball_at(int bx, int byy, float brx, float bry, float r) {
    ovalfill(bx, byy, (int)brx, (int)bry, CLR_PEACH);
    oval(bx, byy, (int)brx, (int)bry, CLR_BROWN);
    ovalfill(bx - (int)(brx * 0.3f), byy - (int)(bry * 0.4f),
             (int)(brx * 0.3f) + 1, (int)(bry * 0.3f) + 1, CLR_WHITE);   // shine
    int nspk = 5 + (int)(r / 4);                                        // hair
    for (int i = 0; i < nspk; i++) {
        float a = 200.0f + 140.0f * i / (nspk - 1);
        float ca = cos_deg(a), sa = sin_deg(a);
        int x0 = bx + (int)(ca * brx * 0.85f), y0 = byy + (int)(sa * bry * 0.85f);
        int ln = 2 + (int)(r * 0.14f);
        line(x0, y0, bx + (int)(ca * (brx + ln)), byy + (int)(sa * (bry + ln)), CLR_BROWN);
    }
}
static void draw_guy(void) {
    float r  = ball_r();
    VPt  *v  = S->vp;
    int   cx = (int)(S->px + S->cw * 0.5f + 0.5f);
    int   feet = (int)(S->py + S->ch + 0.5f);
    float sq  = S->squash;
    float brx = r * (1.0f + sq * 0.35f);
    float bry = r * (1.0f - sq * 0.35f);

    ovalfill(cx, feet + 2, (int)brx + 2, 3, CLR_DARKER_BLUE);            // shadow

    // balls (behind body), each at its floppy verlet point
    ball_at((int)v[LBALL].x, (int)v[LBALL].y, brx, bry, r);
    ball_at((int)v[RBALL].x, (int)v[RBALL].y, brx, bry, r);
    // willy
    thickline((int)v[CROTCH].x, (int)v[CROTCH].y, (int)v[WILLY].x, (int)v[WILLY].y, 2, CLR_PEACH);
    circfill((int)v[WILLY].x, (int)v[WILLY].y, 1, CLR_PINK);
    // legs (behind torso)
    thickline((int)v[LHIP].x, (int)v[LHIP].y, (int)v[LKNEE].x, (int)v[LKNEE].y, 4, CLR_PEACH);
    thickline((int)v[LKNEE].x, (int)v[LKNEE].y, (int)v[LFOOT].x, (int)v[LFOOT].y, 3, CLR_PEACH);
    thickline((int)v[RHIP].x, (int)v[RHIP].y, (int)v[RKNEE].x, (int)v[RKNEE].y, 4, CLR_PEACH);
    thickline((int)v[RKNEE].x, (int)v[RKNEE].y, (int)v[RFOOT].x, (int)v[RFOOT].y, 3, CLR_PEACH);
    circfill((int)v[LFOOT].x, (int)v[LFOOT].y, 2, CLR_PEACH);
    circfill((int)v[RFOOT].x, (int)v[RFOOT].y, 2, CLR_PEACH);
    // body sprite (torso + head), flipped to face the way you move
    sprf(16, cx - 8, (int)(S->py + 0.5f) - 1, S->face < 0, false);
    // arms (in front)
    thickline((int)v[LSH].x, (int)v[LSH].y, (int)v[LELB].x, (int)v[LELB].y, 3, CLR_PEACH);
    thickline((int)v[LELB].x, (int)v[LELB].y, (int)v[LHAND].x, (int)v[LHAND].y, 2, CLR_PEACH);
    thickline((int)v[RSH].x, (int)v[RSH].y, (int)v[RELB].x, (int)v[RELB].y, 3, CLR_PEACH);
    thickline((int)v[RELB].x, (int)v[RELB].y, (int)v[RHAND].x, (int)v[RHAND].y, 2, CLR_PEACH);
    circfill((int)v[LHAND].x, (int)v[LHAND].y, 2, CLR_PEACH);
    circfill((int)v[RHAND].x, (int)v[RHAND].y, 2, CLR_PEACH);
}

void draw(void) {
    if (!S->started) {                                   // clean title screen
        camera(0, 0);
        cls(CLR_DARK_BLUE);
        for (int i = 0; i < 70; i++)
            pset((i * 71) % SCREEN_W, (i * 47) % SCREEN_H, (i % 3) ? CLR_LIGHT_GREY : CLR_WHITE);
        sspr(0, 0, 128, 32, SCREEN_W / 2 - 104, 24, 208, 52);   // HairyBeard banner
        font(FONT_SMALL);
        print_centered("eat ALL the veg up the tower -> pogo to the MOON",
                       SCREEN_W / 2, 86, CLR_WHITE);
        font(FONT_NORMAL);
        print_centered("arrows / WASD move    Z or UP jump", SCREEN_W / 2, 102, CLR_LIGHT_GREY);
        print_centered("hold DOWN to rest on a platform", SCREEN_W / 2, 116, CLR_LIGHT_GREY);
        print_centered("press any key to start", SCREEN_W / 2, 150, CLR_YELLOW);
        return;
    }

    int cam_x = (int)clamp(S->px + S->cw * 0.5f - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W);
    int cam_y = (int)clamp(S->py - SCREEN_H * 0.55f, 0, WORLD_H - SCREEN_H);

    cls(CLR_DARK_BLUE);
    for (int b = 0; b < SCREEN_H; b++) {                 // sky: indigo near ground
        int wy = cam_y + b;
        if (wy > GROUND_TOP - 320) rectfill(0, b, SCREEN_W, 1, CLR_INDIGO);
    }
    camera(cam_x, cam_y);

    for (int i = 0; i < 90; i++) {                       // stars in the upper sky
        int sx = (i * 137) % WORLD_W;
        int sy = (i * 89) % (GROUND_TOP / 2 + 1);
        pset(sx, sy, (i % 4) ? CLR_LIGHT_GREY : CLR_WHITE);
    }

    circfill(MOON_X, MOON_Y, MOON_R, CLR_WHITE);         // moon
    circfill(MOON_X - 16, MOON_Y - 12, 6, CLR_LIGHT_GREY);
    circfill(MOON_X + 14, MOON_Y + 8, 5, CLR_LIGHT_GREY);
    circfill(MOON_X + 4, MOON_Y - 18, 3, CLR_LIGHT_GREY);
    circfill(MOON_X - 10, MOON_Y + 18, 4, CLR_LIGHT_GREY);

    for (int i = 0; i < NSOLID; i++) {                   // platforms
        if (solids[i].x < 0 || solids[i].w >= WORLD_W) continue;
        rectfill(solids[i].x, solids[i].y, solids[i].w, solids[i].h, CLR_BROWN);
        rectfill(solids[i].x, solids[i].y, solids[i].w, 3, CLR_GREEN);
    }
    // ground
    rectfill(0, GROUND_TOP, WORLD_W, GROUND_H, CLR_BROWN);
    rectfill(0, GROUND_TOP, WORLD_W, 4, CLR_GREEN);

    for (int i = 0; i < NFRUIT; i++)                     // fruit (sprites)
        if (!S->gone[i]) spr(17 + fruits[i].type, fruits[i].x - 8, fruits[i].y - 15);

    for (int i = 0; i < 48; i++)
        if (S->plife[i] > 0) circfill((int)S->ptx[i], (int)S->pty[i], 1, S->pcol[i]);

    for (int i = 0; i < 6; i++) if (S->rt[i] > 0) {        // pop rings
        int rr = (int)(S->rt[i] * 18);
        circ((int)S->rx[i], (int)S->ry[i], rr, CLR_WHITE);
        if (rr > 4) circ((int)S->rx[i], (int)S->ry[i], rr - 4, CLR_PINK);
    }

    draw_guy();

    // ── HUD ──────────────────────────────────────────────────────────────────
    camera(0, 0);
    rectfill(0, 0, SCREEN_W, 11, CLR_DARKER_BLUE);
    print("GROW BALLS", 4, 2, CLR_PINK);
    float r = ball_r();
    float pct = (r - BALL_MIN) / (BALL_MAX - BALL_MIN);
    bar(SCREEN_W - 70, 2, 66, 7, pct, r >= POGO_MIN ? CLR_GREEN : CLR_ORANGE, CLR_DARK_BLUE);
    print_right(str("veg %d/%d", S->eaten, NFRUIT), SCREEN_W - 74, 2, CLR_WHITE);

    font(FONT_SMALL);
    if (r < POGO_MIN)
        print_outline("eat the veg to grow your balls!  (Z = hop)", 4, SCREEN_H - 9, CLR_YELLOW, CLR_BLACK);
    else if (!S->won)
        print_outline(S->eaten >= NFRUIT ? "all veg eaten! POGO up to the MOON!"
                                         : "POGO up the tower - eat EVERY veg!",
                      4, SCREEN_H - 9, CLR_GREEN, CLR_BLACK);
    font(FONT_NORMAL);

    if (S->unlockT > 0) {                                  // pogo-unlock celebration
        int yy = 44 + (int)(sin_deg(S->unlockT * 600) * 3);
        print_scaled("POGO POWER!", SCREEN_W / 2 - 88, yy, CLR_YELLOW, 2);
        print_centered("your balls are a SPRING now - bounce!", SCREEN_W / 2, yy + 22, CLR_WHITE);
    }

    if (S->won) {
        fade(0.25f);
        sspr(0, 0, 128, 32, SCREEN_W / 2 - 84, 38, 168, 42);
        print_scaled("TO THE MOON!", SCREEN_W / 2 - 84, 84, CLR_YELLOW, 2);
        font(FONT_SMALL);
        print_centered("you grew the mightiest balls in the land!", SCREEN_W / 2, 110, CLR_WHITE);
        font(FONT_NORMAL);
        print_centered("press R to play again", SCREEN_W / 2, 124, CLR_PINK);
    }
}
