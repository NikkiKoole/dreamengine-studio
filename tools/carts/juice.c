/* de:meta
{
  "title": "19. juice",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "screen-shake-juice",
    "particle-system"
  ],
  "lineage": "Direct homage to the 'juice it or lose it' game-feel canon; the toggle-on/off design is original to this cart so the learner feels the delta of each effect against an identical unjuiced baseline.",
  "description": "The cheap tricks that make a game FEEL good. The same bouncing ball, but every impact fires five classic effects: screen-shake, squash & stretch, a white flash, hit-stop (freeze a few frames for weight), and a puff of dust particles. Press Z to toggle ALL of it off and on so you feel exactly what the juice adds. X to bonk it yourself.",
  "todo": [
    "Make the text-label buttons clickable by mouse."
  ]
}
de:meta */
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
static float shk;                // current shake magnitude (the API now has shake(); this is our own demo value)
static int   flashT, squashT, sqAxis, hitstop;
static bool  juice_on = true;

typedef struct { float x, y, vx, vy, life; int col; } Dust;
static Dust dust[60];

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
    shk = clamp(shk + 4.5f * strength, 0, 9);
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

    shk *= 0.86f;
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
    if (juice_on && shk > 0.4f)
        camera((int)rnd_float_between(-shk, shk), (int)rnd_float_between(-shk, shk));

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
    ovalfill((int)bx, cy, (int)rx, (int)ry, col);

    camera(0, 0);

    // HUD + which techniques are firing right now
    print("JUICE", 8, 8, CLR_WHITE);
    print(juice_on ? "ON" : "OFF", 52, 8, juice_on ? CLR_GREEN : CLR_RED);
    print("Z toggle   X bonk", 120, 8, CLR_LIGHT_GREY);

    int alive = 0; for (int i = 0; i < 60; i++) if (dust[i].life > 0) alive++;
    tag("SHAKE",     8,   shk > 0.4f);
    tag("SQUASH",    58,  squashT > 0);
    tag("FLASH",     118, flashT > 0);
    tag("HIT-STOP",  166, hitstop > 0);
    tag("PARTICLES", 232, alive > 0);
}
