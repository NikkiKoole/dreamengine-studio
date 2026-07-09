// gestures.h — per-finger gesture detection, cart-land, on touch_id() bookkeeping.
//
// Why this exists (touch-notes §4 + §5.8): raylib's rgestures tracks ONE global
// gesture — an in-progress drag dies the instant a second finger taps (confirmed
// on an iPhone, 2026-06-05). Games need fingers to be independent agents: this
// header judges every finger on its own history, so a swipe completes correctly
// while other fingers drum, drag, or rest. Engine deliberately does NOT wrap
// gestures (decision in touch-notes §4); carts #include this instead.
//
// Built on the touch-release API (touch_ended_*): a swipe is "where did this
// finger land, where did it LIFT, how fast" — judged at lift time.
//
// Usage:
//   #include "gestures.h"
//   void update(void) {
//       gest_update();                          // FIRST LINE of update()
//       if (swiped(BTN_LEFT))  prev_page();     // any finger swiped left
//       if (swiped_in(BTN_UP, 0, 100, 320, 100)) jump();  // swipe began in rect
//       cam_zoom *= pinch_scale();              // two-finger pinch, 1.0 = none
//   }
//
// Tunables — #define BEFORE the #include to override:
//   GEST_SWIPE_MIN_PX   (24)  minimum travel for a swipe, canvas px
//   GEST_SWIPE_MAX_FR   (20)  maximum duration in frames (~1/3 s at 60fps)
//   GEST_SWIPE_AXIS     (1.5) dominance ratio: |major| must be ≥ ratio × |minor|
//
// Everything is static (each cart compiles its own copy — improv.h pattern).

#ifndef GESTURES_H
#define GESTURES_H

#include "studio.h"
#include <math.h>

#ifndef GEST_SWIPE_MIN_PX
#define GEST_SWIPE_MIN_PX 24
#endif
#ifndef GEST_SWIPE_MAX_FR
#define GEST_SWIPE_MAX_FR 20
#endif
#ifndef GEST_SWIPE_AXIS
#define GEST_SWIPE_AXIS 1.5f
#endif

#define GEST_MAXF 12

static int g_fid[GEST_MAXF];                    // tracked finger ids
static int g_fsx[GEST_MAXF], g_fsy[GEST_MAXF];  // where each finger landed
static int g_ffr[GEST_MAXF];                    // frame it landed
static int g_fn = 0;
static int g_frame = 0;

// this frame's completed swipes (one per lifted finger, max 4 a frame is plenty)
typedef struct { int dir, sx, sy; } GestSwipe;
static GestSwipe g_swipes[4];
static int g_swipe_n = 0;

// call at the TOP of update(), once per frame
static void gest_update(void) {
    g_frame++;
    g_swipe_n = 0;

    // new fingers → start tracking (id not seen yet)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        int known = 0;
        for (int j = 0; j < g_fn; j++)
            if (g_fid[j] == id) { known = 1; break; }
        if (!known && g_fn < GEST_MAXF) {
            g_fid[g_fn] = id;
            g_fsx[g_fn] = touch_x(i);
            g_fsy[g_fn] = touch_y(i);
            g_ffr[g_fn] = g_frame;
            g_fn++;
        }
    }

    // lifted fingers → judge their whole path: swipe or not, then forget them
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        for (int j = 0; j < g_fn; j++) {
            if (g_fid[j] != id) continue;
            int dx = touch_ended_x(i) - g_fsx[j];
            int dy = touch_ended_y(i) - g_fsy[j];
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            int fr  = g_frame - g_ffr[j];
            if (fr <= GEST_SWIPE_MAX_FR && g_swipe_n < 4) {
                int dir = -1;
                if (adx >= GEST_SWIPE_MIN_PX && adx >= (int)(ady * GEST_SWIPE_AXIS))
                    dir = dx < 0 ? BTN_LEFT : BTN_RIGHT;
                else if (ady >= GEST_SWIPE_MIN_PX && ady >= (int)(adx * GEST_SWIPE_AXIS))
                    dir = dy < 0 ? BTN_UP : BTN_DOWN;
                if (dir >= 0)
                    g_swipes[g_swipe_n++] = (GestSwipe){ dir, g_fsx[j], g_fsy[j] };
            }
            g_fid[j] = g_fid[--g_fn];             // swap-remove
            g_fsx[j] = g_fsx[g_fn]; g_fsy[j] = g_fsy[g_fn]; g_ffr[j] = g_ffr[g_fn];
            break;
        }
    }
}

// did any finger complete a swipe in direction dir (BTN_LEFT/RIGHT/UP/DOWN) this frame?
static bool swiped(int dir) {
    for (int i = 0; i < g_swipe_n; i++)
        if (g_swipes[i].dir == dir) return true;
    return false;
}

// ...that STARTED inside this canvas-space rect? (swipe ownership: "swipe on the
// left half steers, swipe on the right half attacks")
static bool swiped_in(int dir, int x, int y, int w, int h) {
    for (int i = 0; i < g_swipe_n; i++)
        if (g_swipes[i].dir == dir &&
            point_in_box(g_swipes[i].sx, g_swipes[i].sy, x, y, w, h)) return true;   // shared half-open rect test (studio.h)
    return false;
}

// two-finger pinch: this frame's scale factor. 1.0 = no pinch / steady;
// >1 fingers spreading, <1 squeezing. Multiply your zoom by it each frame.
static float pinch_scale(void) {
    if (touch_count() < 2) return 1.0f;
    static int   p_ida = -1, p_idb = -1;
    static float p_dist = 0;
    int ida = touch_id(0), idb = touch_id(1);
    float dx = (float)(touch_x(0) - touch_x(1));
    float dy = (float)(touch_y(0) - touch_y(1));
    float d  = dx * dx + dy * dy;
    if (d <= 0) return 1.0f;
    float dist = sqrtf(d);
    float out = 1.0f;
    if (ida == p_ida && idb == p_idb && p_dist > 0)
        out = dist / p_dist;                      // same pair as last frame
    p_ida = ida; p_idb = idb; p_dist = dist;
    return out;
}

#endif // GESTURES_H
