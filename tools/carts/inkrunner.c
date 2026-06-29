/* de:meta
{
  "title": "ink runner",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "game"
  ],
  "teaches": [
    "verlet-integration",
    "state-machine",
    "save-load-persistence"
  ],
  "lineage": "Original cart; novel in using pget pixel-color as the sole terrain data structure so painted ink and procedural ledges share one collision model, with verlet-integrated limbs producing an emergent walk cycle.",
  "genre": "platformer",
  "description": "THE GROUND IS WHEREVER YOU HAVE PAINTED — the touching_color/pget showcase. An endless scroller: a tiny verlet walker trots along procedural ledges while you paint bridges and ramps with the mouse, racing the ever-faster scroll. There is no terrain data for the feet — each step scans the canvas downward for the first solid-colored pixel (ledge brown or your wet peach ink) and really stands on it. The walk cycle is emergent: planted feet are pinned in world space, so the scroll drags them back under the body like a treadmill. And the rescue: a runner that walks off an edge FALLS, but its dangling feet keep scanning — scribble under it and the feet catch the ink mid-air. Ink is metered and regenerates. Hold left mouse to paint, Z restarts, best distance saved."
}
de:meta */
#include "studio.h"

// INK RUNNER — the touching_color/pget showcase: THE GROUND IS WHEREVER YOU
// HAVE PAINTED. An endless scroller where a little verlet walker trots along
// procedural ledges, and you paint bridges with the mouse to keep it alive.
//
// Why pixel collision is the whole game:
//  • There is no terrain data structure for the feet. Each step, a foot finds
//    its landing by SCANNING THE CANVAS DOWNWARD for the first solid-colored
//    pixel (ledge brown or ink peach — pget). A foot really does stand on one
//    individual pixel, whoever drew it.
//  • Your wet ink is solid the moment it's on screen, because solidity IS the
//    pixel color. Paint a scribble, walk on the scribble.
//  • The rescue: when the runner walks off an edge, it FALLS — but its
//    dangling feet keep scanning below. Scribble under a falling runner and
//    the feet catch the ink mid-air. (pget reads last frame's canvas, so ink
//    becomes solid one frame after you draw it — invisible at 60fps.)
//
// The walker is a tiny verlet puppet: hip + head + two dangling hands
// integrate freely; the legs are a state machine (planted feet are pinned in
// WORLD space, so the scroll drags them back under the body — the walk cycle
// emerges from the treadmill, no animation data).
//
// Paint: hold the left mouse button (watch the ink meter).
// The world speeds up forever. Z restarts. Best distance is saved.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define WALKX     92.0f    // the body's home column on screen
#define LEGH      22.0f    // hip rides this high above the support pixels
#define STRIDE    26.0f    // step length
#define SWING_SPD 0.085f   // swing phase per frame (also scales with speed)
#define ARC        9.0f    // how high a swinging foot lifts
#define GRAV      0.34f
#define BASE_SPD  0.85f    // scroll px/frame at the start
#define MAX_SPD   2.1f
#define INK_MAX   100.0f
#define INK_COST  1.0f     // per ink dot
#define INK_REGEN 0.30f    // per frame

#define MAXINK   700
#define MAXLEDGE 12

typedef struct { float x, y, ox, oy; } Vp;     // a verlet point
// foot states: 0 = PLANTED (pinned at world wx,wy), 1 = SWINGING (arc from
// lift point lx,ly to world target twx,twy), 2 = DANGLING (no grip — falling)
typedef struct { int st; float wx, wy, lx, ly, t, twx, twy; } Foot;
typedef struct { float wx; int w, y; } Ledge;

STATE {
    float scroll, speed;
    Vp    hip, head, hand[2];
    Foot  foot[2];
    Ledge ledge[MAXLEDGE]; int ln;
    float gen_x;                       // terrain generated up to this world x
    float inkx[MAXINK], inky[MAXINK];  // ink dots, world coords (ring buffer)
    int   ink_n;
    float ink;                         // the meter
    float lastmx, lastmy; bool was_held;
    bool  dead;
    int   meters, hi;
    float ping_x, ping_y; int ping_t;  // "foot found a pixel" blip
};

static void vp_set(Vp *p, float x, float y) { p->x = p->ox = x; p->y = p->oy = y; }

static void reset_game(void) {
    S->scroll = 0; S->speed = BASE_SPD;
    S->ln = 0; S->gen_x = 0;
    S->ledge[S->ln++ % MAXLEDGE] = (Ledge){ -60, 340, 150 };   // safe start runway
    S->gen_x = 280;
    S->ink_n = 0; S->ink = INK_MAX;
    S->was_held = false;
    S->dead = false; S->meters = 0;
    S->hi = load_int("inkrun_hi", 0);
    vp_set(&S->hip, WALKX, 150 - LEGH);
    vp_set(&S->head, WALKX, 150 - LEGH - 11);
    vp_set(&S->hand[0], WALKX - 5, 150 - LEGH - 2);
    vp_set(&S->hand[1], WALKX + 5, 150 - LEGH - 2);
    S->foot[0] = (Foot){ 0, 78, 150, 0, 0, 0, 0, 0 };
    S->foot[1] = (Foot){ 0, 102, 150, 0, 0, 0, 0, 0 };
    S->ping_t = 0;
}

void init(void) { enable_pget(true); reset_game(); }

// ── the heart of the cart: is THIS PIXEL something a foot can stand on? ─────
static bool solid_px(int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 18 || y >= SCREEN_H) return false;
    int c = pget(x, y);
    return c == CLR_BROWN || c == CLR_DARK_BROWN || c == CLR_PEACH;
}
// walls for the BODY: ledge colors only — your ink never shoves the walker,
// so a rescue scribble near the body is always safe
static bool wall_px(int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 18 || y >= SCREEN_H) return false;
    int c = pget(x, y);
    return c == CLR_BROWN || c == CLR_DARK_BROWN;
}
// scan a column downward for the first standable SURFACE pixel — solid with
// empty above. (Without the surface rule, a body clipping into a cliff face
// would "catch" interior pixels and pop up through the terrain.)
static int find_support(int sx, int y0, int y1) {
    if (y0 < 18) y0 = 18;
    for (int y = y0; y <= y1 && y < SCREEN_H; y++)
        if (solid_px(sx, y) && !solid_px(sx, y - 1)) return y;
    return -1;
}

static void gen_terrain(void) {
    while (S->gen_x < S->scroll + SCREEN_W + 80) {
        int gap = rnd_between(18, 34) + (int)(S->speed * S->speed * 20);  // gaps grow with speed²
        int w   = rnd_between(55, 130);
        // height variance eases in: the opening is a gentle stroll, then the
        // cliffs grow until painted ramps are the only way over
        int spread = 8 + (int)(S->gen_x * 0.012f); if (spread > 34) spread = 34;
        int y = 150 + rnd_between(-spread, spread + 1);
        y = (int)clamp(y, 106, 170);
        S->ledge[S->ln++ % MAXLEDGE] = (Ledge){ S->gen_x + gap, w, y };
        S->gen_x += gap + w;
    }
}

void update(void) {
#ifdef DE_TRACE
    {
        int pl = (S->foot[0].st == 0) + (S->foot[1].st == 0);
        float ll = 0;
        for (int i = 0; i < 2; i++)
            if (S->foot[i].st == 0) {
                float lx_ = (S->foot[i].wx - S->scroll) - S->hip.x, ly_ = S->foot[i].wy - S->hip.y;
                float d_ = fsqrt(lx_ * lx_ + ly_ * ly_);
                if (d_ > ll) ll = d_;
            }
        watch("r", "pl=%d ll=%.0f ink=%.0f hy=%.0f m=%d d=%d",
              pl, ll, S->ink, S->hip.y, S->meters, S->dead);
    }
#endif
    if (S->dead) { if (btnp(0, BTN_A)) reset_game(); return; }

    S->speed = BASE_SPD + S->scroll * 0.00040f;
    if (S->speed > MAX_SPD) S->speed = MAX_SPD;
    S->scroll += S->speed;
    S->meters = (int)(S->scroll / 10);
    gen_terrain();

    // ── painting: dots in WORLD coords, interpolated along the drag ─────────
    S->ink += INK_REGEN; if (S->ink > INK_MAX) S->ink = INK_MAX;
    bool held = mouse_down(MOUSE_LEFT);
    if (held) {
        float mx = (float)mouse_x(), my = (float)mouse_y();
        if (my > 20) {
            float fx = S->was_held ? S->lastmx : mx, fy = S->was_held ? S->lastmy : my;
            float ddx = mx - fx, ddy = my - fy;
            float len = fsqrt(ddx * ddx + ddy * ddy);
            int   steps = (int)(len / 3) + 1;
            for (int i = 0; i < steps && S->ink >= INK_COST; i++) {
                float t = (float)(i + 1) / steps;
                S->inkx[S->ink_n % MAXINK] = fx + ddx * t + S->scroll;
                S->inky[S->ink_n % MAXINK] = fy + ddy * t;
                S->ink_n++;
                S->ink -= INK_COST;
            }
        }
        S->lastmx = mx; S->lastmy = my;
    }
    S->was_held = held;

    // ── the legs: a tiny state machine over real pixels ─────────────────────
    float fsx[2], fsy[2];
    for (int i = 0; i < 2; i++) {
        if (S->foot[i].st == 0) { fsx[i] = S->foot[i].wx - S->scroll; fsy[i] = S->foot[i].wy; }
    }
    int planted = (S->foot[0].st == 0) + (S->foot[1].st == 0);

    if (planted == 2) {
        // the rear foot lifts when the treadmill has dragged it far enough back
        int rear = fsx[0] < fsx[1] ? 0 : 1;
        if (fsx[rear] < WALKX - STRIDE * 0.45f) {
            int ty = -1, sxx = 0;
            for (int k = 0; k < 4 && ty < 0; k++) {            // a few columns ahead
                sxx = (int)(WALKX + STRIDE * 0.55f) + k * 5;
                ty = find_support(sxx, (int)S->hip.y - 6, SCREEN_H - 2);
            }
            if (ty > 0) {                                      // found a pixel to aim for
                Foot *f = &S->foot[rear];
                f->st = 1; f->lx = fsx[rear]; f->ly = fsy[rear]; f->t = 0;
                f->twx = sxx + S->scroll + S->speed * 10;      // lead the treadmill
                f->twy = (float)ty;
            } else if (fsx[rear] < WALKX - STRIDE) {
                S->foot[rear].st = 2;                          // nothing ahead — grip lost
            }
        }
    } else if (planted == 1) {
        int i = (S->foot[0].st == 0) ? 0 : 1;                  // last foothold slides away
        if (fsx[i] < WALKX - STRIDE * 1.2f) S->foot[i].st = 2;
    }

    for (int i = 0; i < 2; i++) {
        Foot *f = &S->foot[i];
        if (f->st == 1) {                                      // swing along the arc
            f->t += SWING_SPD * (0.6f + S->speed * 0.5f);
            if (f->t >= 1) {
                // RE-VALIDATE at the actual landing column — the treadmill lead
                // means we land to the right of the pixel we originally scanned,
                // and that ground may not exist (a ledge's edge). No surface
                // under the landing point = the step MISSES.
                int lsx = (int)(f->twx - S->scroll);
                int gy  = find_support(lsx, (int)f->twy - 6, (int)f->twy + 7);
                if (gy > 0) {
                    f->st = 0; f->wx = f->twx; f->wy = (float)gy;   // PLANT — on that pixel
                    hit(52 + rnd_between(0, 5), INSTR_NOISE, 1, 16);
                    S->ping_x = f->wx - S->scroll; S->ping_y = f->wy; S->ping_t = 8;
                } else {
                    f->st = 2;                                 // stepped onto nothing!
                }
            }
        } else if (f->st == 2) {                               // dangling: grab anything
            float dx_ = (i == 0) ? -4.0f : 4.0f;
            float px_ = S->hip.x + dx_, py_ = S->hip.y + LEGH * 0.8f;
            int gy = find_support((int)px_, (int)py_ - 2, (int)py_ + 7);
            if (gy > 0) {                                      // the mid-air rescue!
                f->st = 0; f->wx = px_ + S->scroll; f->wy = (float)gy;
                hit(62, INSTR_NOISE, 2, 30);
                S->ping_x = px_; S->ping_y = (float)gy; S->ping_t = 10;
            }
        }
    }

    // ── the body: verlet hip rides the planted feet (or falls) ──────────────
    planted = (S->foot[0].st == 0) + (S->foot[1].st == 0);
    for (int i = 0; i < 2; i++)
        if (S->foot[i].st == 0) { fsx[i] = S->foot[i].wx - S->scroll; fsy[i] = S->foot[i].wy; }

    Vp *h = &S->hip;
    float vx = (h->x - h->ox) * 0.86f, vy = (h->y - h->oy) * 0.86f;
    h->ox = h->x; h->oy = h->y;
    float axx, ayy;
    if (planted > 0) {
        float sy = (planted == 2) ? (fsy[0] + fsy[1]) * 0.5f
                                  : fsy[(S->foot[0].st == 0) ? 0 : 1];
        axx = (WALKX - h->x) * 0.02f;
        ayy = ((sy - LEGH) - h->y) * 0.07f;                    // spring up to leg height
    } else {
        axx = (WALKX - h->x) * 0.003f;
        ayy = GRAV;                                            // nothing under us
    }
    h->x += vx + axx; h->y += vy + ayy;

    // the body is NOT a ghost: probe chest + head height against ledge pixels
    // and push back out. A cliff face scrolling in scrapes the walker backward
    // — slide down it and fall, or get carried to the left edge and crushed.
    // Paint a ramp BEFORE the cliff arrives.
    for (int k = 0; k < 6; k++) {
        if (wall_px((int)h->x + 5, (int)h->y - 3) || wall_px((int)h->x + 5, (int)h->y - 12))
            h->x -= 1.3f;
        else break;
    }
    if (h->x < 14 && !S->dead) {                               // crushed off the screen
        S->dead = true;
        if (S->meters > S->hi) { S->hi = S->meters; save_int("inkrun_hi", S->hi); }
        shake(5);
        note(28, INSTR_SAW, 5);
    }

    // legs are not rubber: a planted foot further than MAXLEG from the hip
    // loses its grip (stops the mile-long-leg look when the body climbs or
    // a rescue catch lands far from the hip)
    for (int i = 0; i < 2; i++) {
        Foot *f = &S->foot[i];
        if (f->st != 0) continue;
        float lx_ = (f->wx - S->scroll) - h->x, ly_ = f->wy - h->y;
        float maxleg = LEGH * 1.55f;
        if (lx_ * lx_ + ly_ * ly_ > maxleg * maxleg) f->st = 2;
    }

    // head lags above the hip (the bob); hands are pure dangle (the flail)
    Vp *hd = &S->head;
    float hvx = (hd->x - hd->ox) * 0.8f, hvy = (hd->y - hd->oy) * 0.8f;
    hd->ox = hd->x; hd->oy = hd->y;
    hd->x += hvx + (h->x - hd->x) * 0.3f;
    hd->y += hvy + ((h->y - 11) - hd->y) * 0.3f;
    for (int i = 0; i < 2; i++) {
        Vp *a = &S->hand[i];
        float ax_ = (a->x - a->ox) * 0.92f, ay_ = (a->y - a->oy) * 0.92f;
        a->ox = a->x; a->oy = a->y;
        a->x += ax_; a->y += ay_ + 0.3f;
        float sx_ = h->x + (h->x - hd->x) * 0.4f, sy_ = h->y - 7;   // shoulder
        float ddx = a->x - sx_, ddy = a->y - sy_;
        float dl = fsqrt(ddx * ddx + ddy * ddy); if (dl < 0.01f) dl = 0.01f;
        float arm = 8.0f;
        a->x = sx_ + ddx / dl * arm; a->y = sy_ + ddy / dl * arm;   // rope constraint
    }

    if (S->ping_t > 0) S->ping_t--;

    if (h->y > SCREEN_H + 30) {                                // fell off the world
        S->dead = true;
        if (S->meters > S->hi) { S->hi = S->meters; save_int("inkrun_hi", S->hi); }
        shake(4);
        note(30, INSTR_SAW, 5);
    }
}

void draw(void) {
    cls(CLR_BLACK);

    // parallax stars
    for (int i = 0; i < 36; i++) {
        int x = ((i * 53 - (int)(S->scroll * 0.25f)) % SCREEN_W + SCREEN_W) % SCREEN_W;
        pset(x, (i * 97 + 23) % (SCREEN_H - 40) + 20, CLR_DARK_GREY);
    }

    // ledges — pillars of standable brown
    for (int i = (S->ln > MAXLEDGE ? S->ln - MAXLEDGE : 0); i < S->ln; i++) {
        Ledge *L = &S->ledge[i % MAXLEDGE];
        int sx = (int)(L->wx - S->scroll);
        if (sx + L->w < 0 || sx >= SCREEN_W) continue;
        rectfill(sx, L->y, L->w, SCREEN_H - L->y, CLR_DARK_BROWN);
        rectfill(sx, L->y, L->w, 3, CLR_BROWN);
    }

    // the ink — every dot is standable terrain
    int n = S->ink_n < MAXINK ? S->ink_n : MAXINK;
    for (int i = 0; i < n; i++) {
        int sx = (int)(S->inkx[i] - S->scroll);
        if (sx < -3 || sx > SCREEN_W + 3) continue;
        circfill(sx, (int)S->inky[i], 2, CLR_PEACH);
    }

    // ── the walker ───────────────────────────────────────────────────────────
    Vp *h = &S->hip; Vp *hd = &S->head;
    // legs: hip → knee → foot (knee bows forward)
    for (int i = 0; i < 2; i++) {
        Foot *f = &S->foot[i];
        float fx, fy;
        if (f->st == 0)      { fx = f->wx - S->scroll; fy = f->wy; }
        else if (f->st == 1) {
            float txs = f->twx - S->scroll;
            fx = lerp(f->lx, txs, f->t);
            fy = lerp(f->ly, f->twy, f->t) - sin_deg(f->t * 180) * ARC;
        } else               { fx = h->x + (i == 0 ? -4 : 4); fy = h->y + LEGH * 0.8f; }
        float mxx = (h->x + fx) / 2, myy = (h->y + fy) / 2;
        float ddx = fx - h->x, ddy = fy - h->y;
        float dl = fsqrt(ddx * ddx + ddy * ddy);
        float bend = (LEGH + 4 - dl) * 0.5f; bend = clamp(bend, 0, 7);
        line((int)h->x, (int)h->y, (int)(mxx + bend), (int)myy, CLR_WHITE);
        line((int)(mxx + bend), (int)myy, (int)fx, (int)fy, CLR_WHITE);
        rectfill((int)fx - 1, (int)fy - 1, 3, 1, CLR_BLUE);      // the tiny foot,
        // standing one pixel above the pixel it found
    }
    line((int)h->x, (int)h->y, (int)hd->x, (int)hd->y + 3, CLR_WHITE);   // spine
    for (int i = 0; i < 2; i++)
        line((int)h->x, (int)(h->y - 7), (int)S->hand[i].x, (int)S->hand[i].y, CLR_LIGHT_GREY);
    circfill((int)hd->x, (int)hd->y, 3, CLR_WHITE);
    pset((int)hd->x + 2, (int)hd->y - 1, CLR_BLACK);             // eye, looking ahead

    // foot-plant ping: "this pixel, right here"
    if (S->ping_t > 0)
        circ((int)S->ping_x, (int)S->ping_y, 9 - S->ping_t, CLR_LIGHT_YELLOW);

    // ── HUD ──────────────────────────────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, 16, CLR_DARKER_BLUE);
    print(str("%dm", S->meters), 4, 4, CLR_WHITE);
    print(str("best %dm", S->hi), 60, 4, CLR_LIGHT_GREY);
    bar(SCREEN_W - 110, 5, 100, 6, S->ink / INK_MAX, CLR_BLUE, CLR_DARKER_GREY);
    font(FONT_SMALL);
    print("ink", SCREEN_W - 110, 12, CLR_BLUE);
    if (S->scroll < 400) print_centered("paint the path with the mouse!", SCREEN_W / 2, 22, blink(20) ? CLR_YELLOW : CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);

    if (S->dead) {
        rectfill(SCREEN_W / 2 - 76, 80, 152, 40, CLR_BLACK);
        rect(SCREEN_W / 2 - 76, 80, 152, 40, CLR_PEACH);
        print_centered(str("fell at %dm", S->meters), SCREEN_W / 2, 88, CLR_WHITE);
        print_centered("press Z to run again", SCREEN_W / 2, 102, CLR_LIGHT_GREY);
    }
}
