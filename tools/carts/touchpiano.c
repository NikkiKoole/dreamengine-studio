// touch piano — the touch-release API's first customer (touch-notes §3)
//
// A piano cannot exist without releases: every key needs its note_off when THE
// FINGER THAT PRESSED IT lifts — and a lifted finger has no touch_x() index, so
// before touch_ended_* a release had no address. The loop that makes it work:
//
//   for (int i = 0; i < touch_ended_count(); i++)      // fingers that lifted
//       if (key_finger[k] == touch_ended_id(i)) ...    // whose key was it?
//
// Slide a finger across keys for glissando (old key off, new key on, same id).
// Lift ripples spawn at touch_ended_x/y — where the finger was last seen.
// iPhone allows ~5 simultaneous fingers, iPad ~10. Desktop mouse = one finger.
#include "studio.h"
#include <stdio.h>

#define SLOT       5
#define BASE_MIDI  60                 // C4; two octaves up from here
#define NWHITE     14                 // two octaves of white keys
#define WKW        (SCREEN_W / NWHITE)
#define KEY_TOP    56
#define BLACK_H    84
#define BLACK_W    16                 // ≥16px — the phone tap-target floor

static const int wsemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int bsemi[7] = { 1, 3, -1, 6, 8, 10, -1 };  // black key right of white i

// per-semitone voice state (2 octaves = 24)
#define NSEMI 24
#define NOFINGER (-999)
static int s_handle[NSEMI];
static int s_finger[NSEMI];           // touch id holding this key, or NOFINGER

// lift ripples — drawn at the released finger's last position
typedef struct { int x, y, life; } Rip;
static Rip rips[16];

static const int FCOLS[6] = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN };
static int id_color(int id) { return FCOLS[(id < 0 ? -id : id) % 6]; }

// which semitone (0..23) is under canvas point x,y? -1 = none
static int semi_at(int x, int y) {
    if (y < KEY_TOP || y >= SCREEN_H) return -1;
    int wk = x / WKW;
    if (wk < 0) wk = 0;
    if (wk >= NWHITE) wk = NWHITE - 1;
    if (y < KEY_TOP + BLACK_H) {                  // black row: check both neighbours
        for (int k = wk - 1; k <= wk; k++) {
            if (k < 0 || k >= NWHITE) continue;
            int pos = k % 7;
            if (bsemi[pos] < 0) continue;
            int bx = (k + 1) * WKW - BLACK_W / 2; // straddles the white-key boundary
            if (x >= bx && x < bx + BLACK_W) return (k / 7) * 12 + bsemi[pos];
        }
    }
    return (wk / 7) * 12 + wsemi[wk % 7];
}

static void key_off(int k) {
    note_off(s_handle[k]);
    s_finger[k] = NOFINGER;
}

void update(void) {
    static bool booted = false;
    if (!booted) {
        instrument(SLOT, INSTR_SAW, 8, 140, 6, 260);   // snappy synth-piano
        for (int k = 0; k < NSEMI; k++) s_finger[k] = NOFINGER;
        booted = true;
    }

    // live fingers: claim keys / glissando (finger moved to a different key)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        int s  = semi_at(touch_x(i), touch_y(i));
        int cur = -1;
        for (int k = 0; k < NSEMI; k++)
            if (s_finger[k] == id) { cur = k; break; }
        if (s == cur) continue;                   // same key (or still off-keys)
        if (cur >= 0) key_off(cur);               // slid off its old key
        if (s >= 0 && s_finger[s] == NOFINGER) {  // claim, unless another finger has it
            s_handle[s] = note_on(BASE_MIDI + s, SLOT, 6);
            s_finger[s] = id;
        }
    }

    // lifted fingers: THE NEW API — release exactly the keys those fingers held
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        for (int k = 0; k < NSEMI; k++)
            if (s_finger[k] == id) key_off(k);
        for (int r = 0; r < 16; r++)              // ripple where the finger let go
            if (rips[r].life <= 0) {
                rips[r] = (Rip){ touch_ended_x(i), touch_ended_y(i), 14 };
                break;
            }
    }

    for (int r = 0; r < 16; r++)
        if (rips[r].life > 0) rips[r].life--;
}

void draw(void) {
    cls(CLR_BLACK);

    // white keys
    for (int k = 0; k < NWHITE; k++) {
        int s = (k / 7) * 12 + wsemi[k % 7];
        int x = k * WKW;
        bool held = s_finger[s] != NOFINGER;
        rectfill(x + 1, KEY_TOP, WKW - 2, SCREEN_H - KEY_TOP, held ? id_color(s_finger[s]) : CLR_WHITE);
        rect(x, KEY_TOP, WKW, SCREEN_H - KEY_TOP, CLR_DARK_GREY);
        if (k % 7 == 0) {                          // label the Cs
            char lbl[4];
            snprintf(lbl, sizeof lbl, "C%d", 4 + k / 7);
            print(lbl, x + 4, SCREEN_H - 12, CLR_MEDIUM_GREY);
        }
    }
    // black keys
    for (int k = 0; k < NWHITE; k++) {
        int pos = k % 7;
        if (bsemi[pos] < 0) continue;
        int s = (k / 7) * 12 + bsemi[pos];
        int x = (k + 1) * WKW - BLACK_W / 2;
        bool held = s_finger[s] != NOFINGER;
        rectfill(x, KEY_TOP, BLACK_W, BLACK_H, held ? id_color(s_finger[s]) : CLR_DARK_GREY);
        rect(x, KEY_TOP, BLACK_W, BLACK_H, CLR_BLACK);
    }

    // lift ripples (touch_ended_x/y — last seen position of the lifted finger)
    for (int r = 0; r < 16; r++)
        if (rips[r].life > 0)
            circ(rips[r].x, rips[r].y, 16 - rips[r].life, CLR_LIGHT_GREY);

    // header
    int held = 0;
    for (int k = 0; k < NSEMI; k++)
        if (s_finger[k] != NOFINGER) held++;
    char buf[48];
    snprintf(buf, sizeof buf, "TOUCH PIANO   fingers:%d  notes:%d", touch_count(), held);
    print(buf, 4, 6, CLR_WHITE);
    font(FONT_SMALL);
    print("hold keys - chords up to 5 fingers (iphone)", 4, 20, CLR_LIGHT_GREY);
    print("slide for glissando - lift to release", 4, 28, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
