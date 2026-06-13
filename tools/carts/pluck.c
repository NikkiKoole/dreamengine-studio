// pluck — INSTR_PLUCK showcase: eight strings + the three engine macros.
//
// The first modeled ENGINE (wave ids 16+): instead of reading a wavetable, every note
// excites a little string simulation (Karplus-Strong — a delay line + feedback), so what
// you hear is BEHAVIOR: the string decays and darkens on its own, like a real instrument.
// Every engine answers the same three 0..1 macro knobs — the API never grows:
//   harmonics = ring time        (0 dead slap   .. 1 near-endless sustain)
//   timbre    = pick brightness  (0 felt thud   .. 1 hard bright pick)
//   morph     = pick position    (0 by the bridge = full .. 1 mid-string = hollow)
//
// controls: A S D F G H J K  pluck a string
//           the string is a real object:
//             GRAB it (press right on it) and drag up/down = BEND its pitch
//             (note_pitch tracking the finger — the engine's read tap is
//             fractional, so bends reach the ringing string live)
//             press in OPEN SPACE = a pick; sweep past strings to STRUM them
//           V chorus warble on/off (the jangle trick: 5.5Hz pitch LFO on a string)
//           drag a slider (auditions as you drag), or
//           LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE strum   ·   M autoplay on/off ('P' belongs to the runtime pause overlay)
//
// MULTITOUCH: every finger is its own pointer — bend two strings at once,
// strum with one hand while another holds a slider. The desktop mouse arrives
// as one synthetic finger, so the same code path covers it.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_STR 5
#define NSTR  8

static const char STRKEY[NSTR] = { 'A','S','D','F','G','H','J','K' };
static const char *KNOB_NAME[3] = { "harmonics (ring)", "timbre (pick)", "morph (position)" };
static const char *KNOB_LO[3]   = { "slap",  "felt",   "bridge" };
static const char *KNOB_HI[3]   = { "drone", "bright", "hollow" };

static int   midi_of[NSTR];
static float amp[NSTR];        // visual string vibration, decays each frame
static float vib_ph[NSTR];
static int   pend[NSTR];       // frames until a scheduled strum note visually plucks
static float knob[3] = { 0.5f, 0.55f, 0.35f };
static int   sel = 0;
static bool  autoplay = true;
static bool  warble = false;   // V: jangle-style chorus warble (pitch LFO bending the string)
static int   apos = 0;
// string gestures — the string is a real object, and EVERY finger is a pointer:
//   press ON a string (±4px)   = grab it → drag up/down BENDS its pitch (a held note_on;
//                                the fractional KS read tap bending live)
//   press in OPEN space        = a pick → sweeping past strings STRUMS them, each string
//                                plucked the moment the finger crosses it
enum { PTR_IDLE, PTR_DRAG, PTR_BEND, PTR_PICK };
typedef struct { int id, mode, k, s, h, y0, x, y, prevY; } Ptr;   // id MUST be first (pointer.h)
static Ptr   ptr[PTR_MAX];     // .id == PTR_NONE → slot free
static float bowSt[NSTR];      // live bend per string, semitones (bows the drawn string)
static int   warble_rx = -1;   // footer "V warble" tap zone, recorded by draw()

// slider geometry — shared by draw() and the mouse hit-test
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 38)
#define KNOB_X(k) (14 + (k) * 102)

static void apply_knobs(void) {
    instrument_harmonics(I_STR, knob[0]);
    instrument_timbre(I_STR, knob[1]);
    instrument_morph(I_STR, knob[2]);
}

// gate scales with the ring knob — a slap doesn't hold a voice hostage, a drone isn't chopped
static int gate_ms(void) { return 800 + (int)(knob[0] * knob[0] * 15000.0f); }

static void pluck_str(int s, int vol) {
    hit(midi_of[s], I_STR, vol, gate_ms());   // long gate — the string decides its own decay
    amp[s]    = 1.0f;
    vib_ph[s] = 0.0f;
}

static void strum_all(void) {
    for (int s = 0; s < NSTR; s++) {
        schedule_hit(s * 45, midi_of[s], I_STR, 5, gate_ms());
        pend[s] = 1 + (s * 45 * 60) / 1000;   // matching visual pluck, frames
    }
}

void init(void) {
    // the engine id is just a waveform — wrap it in a slot like any wave.
    // long release: the gate ending should never chop a ringing string
    instrument(I_STR, INSTR_PLUCK, 1, 0, 7, 1200);
    apply_knobs();
    PTR_CLEAR(ptr);
    for (int s = 0; s < NSTR; s++) midi_of[s] = degree(SCALE_PENTA, 3, s + 2);
    bpm(96);
    amp[1] = 0.7f; amp[4] = 0.9f; amp[6] = 0.5f;   // a lively first frame
}

void update(void) {
    for (int s = 0; s < NSTR; s++) {
        if (keyp(STRKEY[s])) pluck_str(s, 6);
        if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }
    }

    // knobs: pick with LEFT/RIGHT, turn with UP/DOWN (held); audition every 12 frames
    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        apply_knobs();
        if (frame() % 12 == 0) pluck_str(4, 5);
    }

    if (keyp(KEY_SPACE)) strum_all();
    if (keyp('M')) autoplay = !autoplay;   // M like the radio carts ('P' is the runtime pause key)
    // chorus warble — the jangle trick, bending the string ±0.12 st at 5.5Hz
    if (keyp('V') || (warble_rx >= 0 && tapp(warble_rx - 2, SCREEN_H - 13, 56, 13))) {
        warble = !warble;
        instrument_lfo(I_STR, 0, LFO_PITCH, 5.5f, warble ? 0.12f : 0.0f);
    }

    // touch: every finger is its own pointer — grab a slider, grab a string
    // (bend), or pick open space (strum), all independently and at once
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                                      // pool full (>PTR_MAX fingers)
        if (fresh) {                                           // finger just landed
            *p = (Ptr){ id, PTR_IDLE, -1, -1, -1, ty, tx, ty, ty };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) {
                autoplay = !autoplay;                          // the top-right label is a button
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            // ...or the string area: ON a string = grab it (bend); open space = a pick (strum)
            if (p->mode == PTR_IDLE && ty < KNOB_Y - 14) {
                int grabbed = -1;
                for (int s = 0; s < NSTR; s++)
                    if (ty >= 42 + s * 13 - 4 && ty <= 42 + s * 13 + 4) grabbed = s;
                for (int j = 0; j < PTR_MAX; j++)              // one bending finger per string
                    if (ptr[j].id != PTR_NONE && ptr[j].mode == PTR_BEND && ptr[j].s == grabbed) grabbed = -1;
                if (grabbed >= 0) {
                    p->mode = PTR_BEND; p->s = grabbed;
                    p->h = note_on(midi_of[grabbed], I_STR, 6);   // held → pitch is drivable
                    note_glide(p->h, 25);                      // a touch of smoothing on the bend
                    amp[grabbed] = 1.0f; vib_ph[grabbed] = 0.0f;
                } else {
                    p->mode = PTR_PICK;                        // the pick is down
                }
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            apply_knobs();
            if (frame() % 12 == 0) pluck_str(4, 5);            // audition while dragging
        } else if (p->mode == PTR_BEND) {
            float st = clamp((float)(p->y0 - ty) / 12.0f, -7.0f, 7.0f);   // push up = pitch up
            bowSt[p->s] = st;
            note_pitch(p->h, (float)midi_of[p->s] + st);
        } else if (p->mode == PTR_PICK) {
            for (int s = 0; s < NSTR; s++) {                   // pluck each string the pick crosses
                int ys = 42 + s * 13;
                if ((p->prevY < ys && ty >= ys) || (p->prevY > ys && ty <= ys)) pluck_str(s, 5);
            }
            p->prevY = ty;
        }
        p->x = tx; p->y = ty;                                  // draw() reads these for the picks
    }
    for (int i = 0; i < touch_ended_count(); i++) {           // lifted fingers free their slot
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (!p) continue;
        if (p->mode == PTR_BEND) {
            note_off(p->h);                                    // ring out through the release
            bowSt[p->s] = 0;
        }
        p->id = PTR_NONE;
    }

    // autoplay: a wandering pentatonic noodle, strum at the top of every 16 beats
    if (autoplay && every(1)) {
        static const int seq[16] = { 4,2,5,4, 7,5,4,2, 0,2,4,5, 4,2,1,0 };
        if (beat() % 16 == 0) strum_all();
        else {
            pluck_str(seq[apos % 16], 5);
            if (chance(35)) schedule_hit(310, midi_of[(seq[apos % 16] + 2) % NSTR], I_STR, 3, 2000);
        }
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("PLUCK", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("karplus-strong string engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the strings
    for (int s = 0; s < NSTR; s++) {
        int y = 42 + s * 13;
        amp[s] *= 0.94f;
        vib_ph[s] += 0.55f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_PEACH : CLR_DARK_BROWN;
        float bow = bowSt[s] * -2.2f;                       // dragging bows the string
        int x0 = 34, x1 = SCREEN_W - 16, px = x0, py = y;
        for (int x = x0 + 8; x <= x1; x += 8) {
            float t  = (float)(x - x0) / (float)(x1 - x0);
            int   wy = y + (int)(amp[s] * 5.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f)
                               + bow * sinf(t * 3.1415f));
            line(px, py, x, wy, col);
            px = x; py = wy;
        }
        if (bowSt[s] > 0.05f || bowSt[s] < -0.05f) {
            font(FONT_TINY);
            print(str("%+.1f st", bowSt[s]), x1 - 28, y - 9, CLR_LIGHT_YELLOW);
            font(FONT_NORMAL);
        }
        print(str("%c", STRKEY[s]), 12, y - 3, amp[s] > 0.1f ? CLR_WHITE : CLR_MEDIUM_GREY);
        rect(8, y - 6, 14, 13, CLR_DARK_BROWN);
    }

    // the three macro knobs
    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_BROWN, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(KNOB_LO[k], x, y + 9, CLR_DARK_GREY);
        print_right(KNOB_HI[k], x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    // a pick under every strumming finger
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].mode == PTR_PICK)
            trifill(ptr[j].x - 3, ptr[j].y - 4, ptr[j].x + 3, ptr[j].y - 4,
                    ptr[j].x, ptr[j].y + 4, CLR_LIGHT_PEACH);

    font(FONT_TINY);
    warble_rx = print("A..K pluck   grab a string = bend   sweep open space = strum   ",
                      10, SCREEN_H - 9, CLR_DARK_GREY);
    print(str("V warble:%s", warble ? "ON" : "off"), warble_rx, SCREEN_H - 9,
          warble ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);   // tappable
    font(FONT_NORMAL);
}
