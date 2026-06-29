/* de:meta
{
  "title": "hurdy-gurdy",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Original instrument cart; models the hurdy-gurdy's rosined wheel as a continuous angular-velocity sensor — crank speed maps live to note_vol and note_cutoff on two sustained drone voices, a mechanic no other cart in the library uses.",
  "description": "The cranked drone-fiddle, built on the HELD NOTE api around a mechanic nothing else uses: the crank IS the sound. HOLD the mouse and CIRCLE it around the wheel — TWO expressive axes from one gesture: how FAST your hand travels = loudness, how WIDE you circle = brightness (note_vol + note_cutoff across all three voices). Drone voices (tonic + fifth) only sing while the wheel turns. Press a tangent key (A S D F G H J K) to stop the melody string at a pitch (note_pitch), highest key wins; keys 1-5 switch the scale (major/minor/penta/p-min/blues). The 'chien': a sharp FLICK of the crank snaps the buzzing dog bridge for a percussive accent — toggle it with B. Watch the CRANK and TONE meters and the coloured contact ring."
}
de:meta */
#include "studio.h"
#include <math.h>

// Hurdy-gurdy — the cranked drone-fiddle, built on the new HELD NOTE api around
// one mechanic nothing else here uses: the crank is the SOUND. Hold the mouse and
// CIRCLE it around the wheel — your angular speed is the rosined wheel rubbing the
// strings. Stop circling and the wheel coasts to a stop and the sound dies; the
// faster you crank, the louder and brighter it sings.
//
//   * two drone strings (tonic + fifth) are sustained voices (note_on) whose
//     volume you ride with the crank (note_vol), opening their filter as you speed
//     up (note_cutoff) — they only sing while the wheel turns;
//   * the melody string is a third held voice; press a tangent key (A S D F G H J K)
//     to stop it at a pitch (note_pitch), highest key wins, loudness still from the
//     crank;
//   * the "chien" (the buzzing dog): a sharp FLICK of the crank — a sudden burst of
//     speed — snaps the buzzing bridge and fires a percussive noise accent. The
//     rhythmic trick at the heart of hurdy-gurdy playing.
//
// CONTROLS: hold + circle the wheel to crank (mouse or finger) · A S D F G H J K
// = melody tangents (the on-screen tangent buttons are tappable and HELD, so on
// touch you crank with one hand and finger tangents with the other) · flick the
// crank for the buzz · scale labels + the buzz label are tappable too.

#define DRONE   5
#define MEL     6
#define WCX     232          // wheel centre
#define WCY     96
#define WR      46           // wheel radius
#define GAIN    0.085f       // px/frame of hand travel → 0..1 loudness (radius-independent)
#define NKEYS   8

static const char TANGENTS[NKEYS + 1] = "ASDFGHJK";

static int   drone_lo, drone_hi, mel;     // held-voice handles
static float speed;                       // smoothed crank speed 0..1 (the wheel)
static float prev_speed;                  // last frame, for chien (flick) detection
static float wheel_vis;                   // visual spin angle (degrees)
static float prev_ang;                    // last mouse angle around the wheel
static int   was_down;                    // was the crank held last frame?
static int   buzz_vis;                    // frames the buzzing bridge jitters
static int   buzz_cd;                     // cooldown so one flick = one buzz
static int   active_key = -1;             // which tangent is sounding (-1 = none)
static int   scale_idx;                   // which scale A-K play in
static int   buzz_on = 1;                 // is the chien (flick buzz) armed?
static float crank_r = 46;                // smoothed distance of the hand from the hub
static float bright;                        // 0 (warm, near hub) .. 1 (bright, near rim)

static const int   SCALES[5]      = { SCALE_MAJOR, SCALE_MINOR, SCALE_PENTA,
                                       SCALE_PENTA_MIN, SCALE_BLUES };
static const char *SCALE_NAMES[5] = { "major", "minor", "penta", "p-min", "blues" };

// touch: the first finger near the wheel CLAIMS the crank; every other finger
// holds tangent buttons / taps labels (the desktop mouse = one synthetic finger)
#define NOID (-999)
static int   crank_id = NOID;        // the finger turning the crank
static float crank_px, crank_py;     // that finger's position (draw uses it too)
static int   tang_hot[NKEYS];        // tangent i held by a finger this frame
static int   scale_rx[5], scale_rw[5];   // scale-label tap rects, recorded by draw()
static int   buzz_rx, buzz_rw;           // buzz-label tap rect
#define TANG_X(i) (14 + (i) * 16)        // tangent button hit boxes
#define TANG_Y    40
#define TANG_W    15
#define TANG_H    30

static float wrap_pi(float a) {
    while (a >  3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}

void init(void) {
    // warm reedy saw drones; bright reedy saw melody — the hurdy-gurdy buzz.
    instrument(DRONE, INSTR_SAW, 140, 300, 7, 500);
    instrument_filter(DRONE, FILTER_LOW, 600, 4);
    instrument(MEL, INSTR_SAW, 40, 200, 7, 220);
    instrument_filter(MEL, FILTER_LOW, 1400, 6);
    instrument_lfo(MEL, 0, LFO_PITCH, 5.5f, 0.15f);   // a little fiddle vibrato

    drone_lo = note_on(36, DRONE, 0);                 // C2 tonic
    drone_hi = note_on(43, DRONE, 0);                 // G2 fifth
    mel      = note_on(60, MEL, 0);                   // silent until a tangent + crank
}

void update(void) {
    // ---- sort the fingers: crank vs tangents vs label taps ----
    for (int i = 0; i < NKEYS; i++) tang_hot[i] = 0;
    int down = 0;
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i); float tx = touch_x(i), ty = touch_y(i);
        if (id == crank_id) { down = 1; crank_px = tx; crank_py = ty; continue; }
        float ddx = tx - WCX, ddy = ty - WCY;
        if (crank_id == NOID && sqrtf(ddx * ddx + ddy * ddy) <= WR + 24) {
            crank_id = id; down = 1; crank_px = tx; crank_py = ty;   // claimed
            continue;
        }
        for (int k = 0; k < NKEYS; k++)              // held tangent buttons
            if (tx >= TANG_X(k) && tx < TANG_X(k) + TANG_W &&
                ty >= TANG_Y    && ty < TANG_Y + TANG_H) tang_hot[k] = 1;
    }
    for (int i = 0; i < touch_ended_count(); i++)
        if (touch_ended_id(i) == crank_id) crank_id = NOID;
    if (crank_id == NOID) down = 0;

    // ---- the crank: angular speed of the crank finger circling the wheel ----
    float dx = crank_px - WCX, dy = crank_py - WCY;
    float dist = sqrtf(dx * dx + dy * dy);
    float ang  = atan2f(dy, dx);
    float raw  = 0;
    // LINEAR hand speed = how far the hand travelled along its arc (radius × angle).
    // So loudness tracks actual hand velocity, fully independent of how wide you circle.
    if (down && was_down) raw = fabsf(wrap_pi(ang - prev_ang)) * dist;
    prev_ang = ang;
    was_down = down;

    // how far from the hub your hand circles = the TONE: tight = warm, wide = bright
    if (down) crank_r = lerp(crank_r, dist, 0.25f);
    bright = clamp((crank_r - 18) / (78 - 18), 0, 1);

    float target = down ? clamp(raw * GAIN, 0, 1) : 0;
    // fast to spin up, slow to coast down — the wheel has momentum
    speed = lerp(speed, target, target > speed ? 0.5f : 0.06f);
    wheel_vis += speed * 7.0f;                        // visual spin (always one way)

    int cv  = (int)(speed * 7 + 0.5f);                // crank speed → loudness
    int cut = (int)lerp(380, 2800, bright);             // circle width → brightness

    // ---- drones: sing only while the wheel turns ----
    note_vol(drone_lo, cv);  note_cutoff(drone_lo, cut);
    note_vol(drone_hi, cv);  note_cutoff(drone_hi, cut);
    note_cutoff(mel, cut);                            // whole instrument brightens together

    // ---- scale select: 1-5 (or tap a label) toggle which scale the tangents play in ----
    for (int i = 0; i < 5; i++)
        if (keyp('1' + i) || (scale_rw[i] > 0 && tapp(scale_rx[i], 161, scale_rw[i], 13)))
            scale_idx = i;

    // ---- melody: highest pressed tangent wins (key or finger), voiced in the chosen scale ----
    active_key = -1;
    for (int i = 0; i < NKEYS; i++) if (key(TANGENTS[i]) || tang_hot[i]) active_key = i;
    if (active_key >= 0) {
        note_pitch(mel, degree(SCALES[scale_idx], 4, active_key));   // up from C4
        note_vol(mel, cv);
    } else {
        note_vol(mel, 0);
    }

    // ---- chien: a deliberate burst of speed ON TOP of a steady crank snaps the
    // buzzing bridge. Gating on prev_speed (already cranking) keeps the big
    // spin-up-from-rest spike from firing it — that's not a flick, just starting.
    if (keyp('B') || (buzz_rw > 0 && tapp(buzz_rx, 183, buzz_rw, 13)))
        buzz_on = !buzz_on;                       // arm / disarm the chien
    float accel = speed - prev_speed;
    if (buzz_on && accel > 0.13f && prev_speed > 0.45f && buzz_cd <= 0) {
        hit(38, INSTR_NOISE, cv, 70);                 // dry percussive buzz
        buzz_vis = 7;
        buzz_cd  = 14;
    }
    prev_speed = speed;
    if (buzz_cd  > 0) buzz_cd--;
    if (buzz_vis > 0) buzz_vis--;
}

// a vibrating string: a horizontal line that wiggles with amplitude `amp`
static void string_line(int y, float amp, int col) {
    int px = 14, py = y;
    for (int x = 16; x <= 150; x += 3) {
        int yy = y + (int)(sinf(frame() * 0.6f + x * 0.25f) * amp);
        line(px, py, x, yy, col);
        px = x; py = yy;
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    float amp = speed * 3.0f;

    // ---- the three strings on the left ----
    // melody string with tangent ticks; the sounding one glows
    string_line(50, active_key >= 0 ? amp : 0, active_key >= 0 ? CLR_LIGHT_PEACH : CLR_DARK_BROWN);
    for (int i = 0; i < NKEYS; i++) {
        int tx = 22 + i * 16, on = (i == active_key);
        rect(TANG_X(i), TANG_Y + 2, TANG_W, TANG_H - 4, on ? CLR_ORANGE : CLR_DARK_BROWN);   // tappable button
        line(tx, 44, tx, 56, on ? CLR_WHITE : CLR_DARK_BROWN);
        char lbl[2] = { TANGENTS[i], 0 };
        print(lbl, tx - 2, 60, on ? CLR_YELLOW : CLR_BROWN);
    }
    // two drone strings, lit by the crank
    int dcol = speed > 0.05f ? (speed > 0.5f ? CLR_ORANGE : CLR_BROWN) : CLR_DARK_BROWN;
    string_line(118, amp, dcol);
    string_line(136, amp * 0.7f, dcol);

    // ---- the wheel ----
    circ(WCX, WCY, WR, CLR_BROWN);
    circ(WCX, WCY, WR - 1, CLR_DARK_BROWN);
    for (int k = 0; k < 6; k++) {                     // spokes spin with wheel_vis
        float a = (wheel_vis + k * 60) * 0.0174533f;
        line(WCX, WCY, WCX + (int)(cosf(a) * (WR - 3)), WCY + (int)(sinf(a) * (WR - 3)),
             speed > 0.05f ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
    }
    circfill(WCX, WCY, 6, CLR_LIGHT_GREY);

    // the contact ring: the circle your hand is tracing, coloured by brightness —
    // tight (near the hub) = warm/dim, wide (near the rim) = bright
    int bcol = bright > 0.66f ? CLR_WHITE : bright > 0.33f ? CLR_ORANGE : CLR_BROWN;
    circ(WCX, WCY, (int)crank_r, bcol);

    // the crank arm + handle — follows your hand while cranking, else coasts
    float ha = (crank_id != NOID ? atan2f(crank_py - WCY, crank_px - WCX)
                                 : wheel_vis * 0.0174533f);
    int hx = WCX + (int)(cosf(ha) * (WR + 8)), hy = WCY + (int)(sinf(ha) * (WR + 8));
    line(WCX, WCY, hx, hy, CLR_LIGHT_GREY);
    circfill(hx, hy, 4, CLR_YELLOW);

    // ---- the buzzing dog (chien) bridge ----
    int bj = buzz_vis > 0 ? (buzz_vis & 1 ? 2 : -2) : 0;   // jitter on a buzz
    rectfill(WCX - 10, WCY + WR + 6 + bj, 20, 5, buzz_vis > 0 ? CLR_WHITE : CLR_DARK_BROWN);
    if (buzz_vis > 0) print("BZZT", WCX - 9, WCY + WR + 14, CLR_YELLOW);

    // ---- HUD ----
    print("HURDY-GURDY", 6, 6, CLR_ORANGE);

    // scale picker — press 1-5 or tap a label; the active scale glows
    font(FONT_SMALL);
    int sx = print("SCALE", 6, 166, CLR_MEDIUM_GREY) + 5;
    for (int i = 0; i < 5; i++) {
        int on = (i == scale_idx), x0 = sx;
        char num[3] = { (char)('1' + i), ' ', 0 };
        sx = print(num, sx, 166, on ? CLR_YELLOW : CLR_DARKER_GREY);
        sx = print(SCALE_NAMES[i], sx, 166, on ? CLR_WHITE : CLR_DARK_BROWN) + 7;
        scale_rx[i] = x0 - 1; scale_rw[i] = sx - x0 - 4;   // record the tap rect
    }
    font(FONT_NORMAL);

    print("hold + circle the wheel to crank", 6, 178, CLR_MEDIUM_GREY);
    int bx = print("A-K: melody   B: buzz ", 6, 188, CLR_DARK_BROWN);
    int be = print(buzz_on ? "[on]" : "[off]", bx, 188, buzz_on ? CLR_ORANGE : CLR_DARKER_GREY);
    buzz_rx = 6 + text_width("A-K: melody   ");           // "B: buzz [on]" is tappable
    buzz_rw = be - buzz_rx;
    // meters: crank speed (loudness) and circle width (brightness)
    font(FONT_SMALL);
    print("CRANK", 218, 11, CLR_DARKER_GREY);
    print("TONE",  222, 21, CLR_DARKER_GREY);
    font(FONT_NORMAL);
    rect(250, 10, 62, 6, CLR_DARK_BROWN);
    rectfill(251, 11, (int)(speed  * 60), 4, speed > 0.5f ? CLR_ORANGE : CLR_BROWN);
    rect(250, 20, 62, 6, CLR_DARK_BROWN);
    rectfill(251, 21, (int)(bright * 60), 4, bcol);
}
