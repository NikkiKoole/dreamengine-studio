// touchlab — device test card for the touch API
//
// PROBE (see docs/design/touch-notes.md §5 and probe-carts.md): pick this up
// the day a real touchscreen is available. Settings → Build for web, open the
// LAN url from the build log on the iPad/phone, then run down the card:
//
//   1 RINGS  crosshair must sit EXACTLY under each finger (CSS-scale accuracy)
//   2 IDS    hold one finger, tap others — the held finger's id must not change
//   3 TAPS   drum the button — counter must +1 once per physical tap
//   4 HOLD   zone lights only while a finger is on it
//   5 LIFT   lift a finger — readout shows its id + last position
//   6 SOUND  every landing finger blips (iOS audio unlock check)
//   7 PAGE   pinch / double-tap / drag / edge-swipe must NOT move the page
//   8 GEST   swipe / flick / pinch / hold — raylib's gesture detector readout:
//            does each fire when it should, and never when a finger just rests?
//            (VERDICT 2026-06-05: drag dies when a 2nd finger taps — never wrap)
//   9 SWIPE  the same swipes through gestures.h (per-finger, cart-land) — must
//            keep firing while other fingers drum. The fix for what 8 proves.
//
// On desktop the mouse is one finger (id -2) — enough to smoke-test 3/4/5/6/8/9.
#include "studio.h"
#include "gestures.h"
#include <stdio.h>

// ── test 8: raw raylib rgestures readout (touch-notes §4: probe before wrapping)
// extern decls instead of #include "raylib.h" — its KEY_* enum members collide
// with studio.h's KEY_* defines. Struct layout matches raylib's Vector2.
typedef struct { float x, y; } RlV2;
extern int   GetGestureDetected(void);
extern float GetGestureHoldDuration(void);
extern RlV2  GetGestureDragVector(void);

static int  gest_now = 0;          // gesture this frame (0 = none)
static int  gest_prev = 0;
static char gest_hist[40] = "";    // last few onsets, newest first
static char swipe_hist[40] = "";   // test 9: gestures.h swipes, newest first

static const char *gname(int g) {
    switch (g) {
        case 1:   return "TAP";   case 2:   return "DTAP";
        case 4:   return "HOLD";  case 8:   return "DRAG";
        case 16:  return "SW-R";  case 32:  return "SW-L";
        case 64:  return "SW-U";  case 128: return "SW-D";
        case 256: return "PN-IN"; case 512: return "PN-OUT";
    }
    return "?";
}

// ── cart-side finger bookkeeping (the §2 diff pattern, reference copy) ──
#define MAXF 16
static int prev_id[MAXF], prev_x[MAXF], prev_y[MAXF];
static int prev_n = 0;

static int max_seen = 0;       // most simultaneous fingers ever
static int tapp_count = 0;     // TAPS button presses
static int blips = 0;          // landing blips fired
static int lift_id = 0;        // last lifted finger
static int lift_x = -1, lift_y = -1;
static bool any_lift = false;

static const int FCOLS[8] = {
    CLR_RED, CLR_BLUE, CLR_GREEN, CLR_YELLOW,
    CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN, CLR_PEACH,
};
static int id_color(int id) { return FCOLS[(id < 0 ? -id : id) % 8]; }

// button rects (canvas-space)
#define TAPS_X (SCREEN_W - 70)
#define TAPS_Y 22
#define HOLD_X (SCREEN_W - 70)
#define HOLD_Y 42
#define BTN_W  66
#define BTN_H  16

void update(void) {
    int n = touch_count();
    if (n > max_seen) max_seen = n;

    // test 3: tapp must fire once per tap
    if (tapp(TAPS_X, TAPS_Y, BTN_W, BTN_H)) tapp_count++;

    // test 6: blip when a NEW finger lands (id absent last frame)
    for (int i = 0; i < n; i++) {
        int id = touch_id(i);
        bool fresh = true;
        for (int j = 0; j < prev_n; j++)
            if (prev_id[j] == id) { fresh = false; break; }
        if (fresh) {
            hit(48 + 36 * touch_x(i) / SCREEN_W, INSTR_MALLET, 4, 220);
            blips++;
        }
    }

    // test 5: spot lifted fingers (id present last frame, absent now)
    for (int j = 0; j < prev_n; j++) {
        bool gone = true;
        for (int i = 0; i < n; i++)
            if (touch_id(i) == prev_id[j]) { gone = false; break; }
        if (gone) {
            lift_id = prev_id[j];
            lift_x = prev_x[j];
            lift_y = prev_y[j];
            any_lift = true;
        }
    }

    // test 9: per-finger swipes via gestures.h — judged at finger-lift
    gest_update();
    {
        static const char *dn[4] = { "UP", "DOWN", "LEFT", "RIGHT" };
        for (int d = 0; d < 4; d++)
            if (swiped(d)) {
                char tmp[40];
                snprintf(tmp, sizeof tmp, "%s %.30s", dn[d], swipe_hist);
                snprintf(swipe_hist, sizeof swipe_hist, "%s", tmp);
            }
    }

    // test 8: log each gesture onset into the history strip
    gest_now = GetGestureDetected();
    if (gest_now != 0 && gest_now != gest_prev) {
        char tmp[40];
        snprintf(tmp, sizeof tmp, "%s %.30s", gname(gest_now), gest_hist);
        snprintf(gest_hist, sizeof gest_hist, "%s", tmp);
    }
    gest_prev = gest_now;

    // snapshot for next frame
    prev_n = n > MAXF ? MAXF : n;
    for (int i = 0; i < prev_n; i++) {
        prev_id[i] = touch_id(i);
        prev_x[i]  = touch_x(i);
        prev_y[i]  = touch_y(i);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    char buf[64];
    int n = touch_count();

    // tests 1+2: crosshair + ring + id label per finger
    for (int i = 0; i < n; i++) {
        int x = touch_x(i), y = touch_y(i), id = touch_id(i);
        int col = id_color(id);
        line(x - 16, y, x + 16, y, CLR_DARK_GREY);   // crosshair: must sit
        line(x, y - 16, x, y + 16, CLR_DARK_GREY);   // exactly under the finger
        circ(x, y, 12, col);
        circ(x, y, 11, col);
        snprintf(buf, sizeof buf, "%d", id);
        print(buf, x + 15, y - 14, CLR_WHITE);
    }

    // test 5 marker: X where the last finger lifted
    if (any_lift) {
        line(lift_x - 5, lift_y - 5, lift_x + 5, lift_y + 5, CLR_DARK_RED);
        line(lift_x - 5, lift_y + 5, lift_x + 5, lift_y - 5, CLR_DARK_RED);
    }

    // header
    snprintf(buf, sizeof buf, "TOUCHLAB  fingers:%d  max:%d  fps:%d", n, max_seen, fps());
    print(buf, 4, 4, CLR_WHITE);

    // test 3: TAPS button
    bool taps_held = tap(TAPS_X, TAPS_Y, BTN_W, BTN_H);
    rectfill(TAPS_X, TAPS_Y, BTN_W, BTN_H, taps_held ? CLR_BLUE : CLR_DARK_BLUE);
    rect(TAPS_X, TAPS_Y, BTN_W, BTN_H, CLR_BLUE);
    snprintf(buf, sizeof buf, "TAPS:%d", tapp_count);
    print(buf, TAPS_X + 6, TAPS_Y + 4, CLR_WHITE);

    // test 4: HOLD zone
    bool holding = tap(HOLD_X, HOLD_Y, BTN_W, BTN_H);
    rectfill(HOLD_X, HOLD_Y, BTN_W, BTN_H, holding ? CLR_GREEN : CLR_DARK_GREEN);
    rect(HOLD_X, HOLD_Y, BTN_W, BTN_H, CLR_GREEN);
    print(holding ? "HOLDING" : "HOLD", HOLD_X + 6, HOLD_Y + 4, CLR_WHITE);

    // the card (small font keeps it out of the way)
    font(FONT_SMALL);
    print("1 RINGS crosshair exactly under finger?", 4, 16, CLR_LIGHT_GREY);
    print("2 IDS   hold one, tap others: id stays?", 4, 24, CLR_LIGHT_GREY);
    print("3 TAPS  +1 per tap, even drumming fast?", 4, 32, CLR_LIGHT_GREY);
    print("4 HOLD  lights only while held?", 4, 40, CLR_LIGHT_GREY);
    if (any_lift)
        snprintf(buf, sizeof buf, "5 LIFT  id %d ended at %d,%d (X marks it)", lift_id, lift_x, lift_y);
    else
        snprintf(buf, sizeof buf, "5 LIFT  lift a finger...");
    print(buf, 4, 48, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "6 SOUND %d blips - heard every one?", blips);
    print(buf, 4, 56, CLR_LIGHT_GREY);
    print("7 PAGE  pinch/dbl-tap/edge-swipe: page", 4, 64, CLR_LIGHT_GREY);
    print("        must not zoom, scroll or bounce", 4, 70, CLR_LIGHT_GREY);
    if (gest_now) {
        RlV2 d = GetGestureDragVector();
        snprintf(buf, sizeof buf, "8 GEST  now:%s hold:%.1f drag:%.2f,%.2f",
                 gname(gest_now), GetGestureHoldDuration(), d.x, d.y);
    } else
        snprintf(buf, sizeof buf, "8 GEST  try: swipe flick pinch hold");
    print(buf, 4, 78, CLR_YELLOW);
    snprintf(buf, sizeof buf, "        log: %.32s", gest_hist);
    print(buf, 4, 86, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "9 SWIPE gestures.h log: %.24s", swipe_hist);
    print(buf, 4, 94, CLR_GREEN);
    print("        swipe WHILE drumming: must still fire", 4, 102, CLR_LIGHT_GREY);
    print("verdicts -> docs/design/touch-notes.md #5", 4, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
