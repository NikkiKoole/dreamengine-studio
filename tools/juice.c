#include "studio.h"

// JUICE — the cheap tricks that make a game FEEL good.
//
// Same bouncing ball, but every impact fires five classic effects:
//   SHAKE     — jolt the camera, decay it fast
//   SQUASH    — flatten on the axis you hit, spring back
//   FLASH     — blink white for a couple frames
//   HIT-STOP  — freeze everything for a few frames (weight!)
//   PARTICLES — a puff of dust at the contact point
//
// Press Z to toggle ALL of it off and on — the ball is identical underneath,
// so you feel exactly what the juice is adding. X = bonk it yourself.

#define BOX_L   16
#define BOX_R   (SCREEN_W - 16)
#define BOX_T   44
#define BOX_B   (SCREEN_H - 16)
#define R       12.0f            // ball radius
#define GRAV    0.40f
#define LAUNCH  9.4f             // floor bounce keeps it perpetual
#define SQF     14               // squash duration (frames)
#define V_AXIS  0                // squash axis: vertical impact
#define H_AXIS  1                // horizontal impact

static float bx, by, vx, vy;
static float shake;              // current shake magnitude
static int   flashT, squashT, sqAxis, hitstop;
static bool  juice_on = true;

typedef struct { float x, y, vx, vy, life; int col; } Dust;
static Dust dust[60];

// self-contained sqrt (no libm) so we can draw a squashed ellipse
static float fsqrt(float v) { if (v <= 0) return 0; float g = v; for (int i = 0; i < 8; i++) g = 0.5f * (g + v / g); return g; }

static void ellipsefill(int cx, int cy, float rx, float ry, int col) {
    if (rx < 1) rx = 1; if (ry < 1) ry = 1;
    int iry = (int)ry;
    for (int dy = -iry; dy <= iry; dy++) {
        float f = 1.0f - (float)(dy * dy) / (ry * ry);
        int hw = (int)(rx * fsqrt(f < 0 ? 0 : f));
        line(cx - hw, cy + dy, cx + hw, cy + dy, col);
    }
}

static void spawn_dust(float x, float y) {
    for (int n = 0; n < 10; n++)
        for (int i = 0; i < 60; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ x, y, rnd_float_between(-2.2f, 2.2f),
                                  rnd_float_between(-2.5f, -0.3f), rnd_between(12, 22),
                                  (n & 1) ? CLR_WHITE : CLR_LIGHT_GREY };
                break;
            }
}

// the impact — sound always plays, the JUICE only when it's switched on
static void impact(int axis, float strength, float ix, float iy) {
    hit(42, INSTR_NOISE, juice_on ? 5 : 3, juice_on ? 110 : 50);
    if (!juice_on) return;
    shake = clamp(shake + 4.5f * strength, 0, 9);
    flashT  = 4;
    squashT = SQF; sqAxis = axis;
    hitstop = mid(2, (int)(4 * strength), 6);
    spawn_dust(ix, iy);
}

void init(void) {
    bx = SCREEN_W / 2; by = BOX_B - R; vx = 1.9f; vy = -LAUNCH;
    for (int i = 0; i < 60; i++) dust[i].life = 0;
    impact(V_AXIS, 1.0f, bx, BOX_B);     // a juicy first frame for the thumbnail
}

void update(void) {
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) juice_on = !juice_on;
    if (btnp(0, BTN_B) || btnp(1, BTN_B)) { vy = -LAUNCH * 1.2f; vx = rnd_float_between(-3, 3);
                                            impact(V_AXIS, 1.4f, bx, by + R); }

    // HIT-STOP: freeze the whole simulation for a few frames
    if (juice_on && hitstop > 0) { hitstop--; return; }

    shake *= 0.86f;
    if (flashT  > 0) flashT--;
    if (squashT > 0) squashT--;

    for (int i = 0; i < 60; i++)
        if (dust[i].life > 0) { dust[i].x += dust[i].vx; dust[i].y += dust[i].vy;
                                dust[i].vy += 0.18f; dust[i].life--; }

    // plain physics — identical whether juice is on or off
    vy += GRAV; bx += vx; by += vy;
    if (by + R >= BOX_B) { by = BOX_B - R; vy = -LAUNCH;  impact(V_AXIS, 1.0f, bx, BOX_B); }
    if (by - R <= BOX_T) { by = BOX_T + R; vy = -vy;      impact(V_AXIS, 0.6f, bx, BOX_T); }
    if (bx - R <= BOX_L) { bx = BOX_L + R; vx = -vx;      impact(H_AXIS, 0.8f, BOX_L, by); }
    if (bx + R >= BOX_R) { bx = BOX_R - R; vx = -vx;      impact(H_AXIS, 0.8f, BOX_R, by); }
}

static void tag(const char *s, int x, bool on) {
    print(s, x, 30, on ? CLR_YELLOW : CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // SHAKE: nudge the whole scene; HUD is drawn later with no shake
    if (juice_on && shake > 0.4f)
        camera((int)rnd_float_between(-shake, shake), (int)rnd_float_between(-shake, shake));

    // arena
    rect(BOX_L, BOX_T, BOX_R - BOX_L, BOX_B - BOX_T, CLR_DARK_GREY);
    line(BOX_L, BOX_B, BOX_R, BOX_B, CLR_LIGHT_GREY);

    for (int i = 0; i < 60; i++)
        if (dust[i].life > 0) pset((int)dust[i].x, (int)dust[i].y, dust[i].col);

    // SQUASH & STRETCH: flatten on the impact axis, ease back to a circle
    float rx = R, ry = R;
    if (juice_on && squashT > 0) {
        float amt = ease_out((float)squashT / SQF) * 0.55f;
        if (sqAxis == V_AXIS) { rx = R * (1 + amt); ry = R * (1 - amt); }
        else                  { rx = R * (1 - amt); ry = R * (1 + amt); }
    }
    int cy = (int)by + (sqAxis == V_AXIS ? (int)(R - ry) : 0);   // keep it sitting on the floor
    int col = (juice_on && flashT > 0) ? CLR_WHITE : CLR_RED;     // FLASH
    ellipsefill((int)bx, cy, rx, ry, col);

    camera(0, 0);

    // HUD + which techniques are firing right now
    print("JUICE", 8, 8, CLR_WHITE);
    print(juice_on ? "ON" : "OFF", 52, 8, juice_on ? CLR_GREEN : CLR_RED);
    print("Z toggle   X bonk", 120, 8, CLR_LIGHT_GREY);

    int alive = 0; for (int i = 0; i < 60; i++) if (dust[i].life > 0) alive++;
    tag("SHAKE",     8,   shake > 0.4f);
    tag("SQUASH",    58,  squashT > 0);
    tag("FLASH",     118, flashT > 0);
    tag("HIT-STOP",  166, hitstop > 0);
    tag("PARTICLES", 232, alive > 0);
}
