// guitar — INSTR_GUITAR showcase + tuning rig: eight strings, the three engine macros,
// and eight hardware presets (the acceptance tests).
//
// The bodied plucked string (§8.8.9): a Karplus-Strong string — like PLUCK — but run through
// a resonant BODY (four parallel formant filters), which is the box that bare PLUCK lacks.
// It decays on its own like a real string. The same three 0..1 macros shape it:
//   harmonics = body   (0 open & bodyless, a harp .. 1 resonant bright box, a banjo)
//   timbre    = string brightness (0 warm nylon .. 1 bright steel)
//   morph     = mute   (0 long open ring .. 1 tight muted pizzicato stop)
//
// controls: A S D F G H J K  pluck a string
//             GRAB a string (press on it) + drag up/down = BEND its pitch live
//             press OPEN SPACE = a pick; sweep past strings to STRUM them
//           1..8  load a preset (harp/nylon/steel/banjo/koto/uke/pizz/oud) — the test:
//                 if "banjo" doesn't sound like a banjo, the MAPPING is wrong, not the preset
//           drag a slider (auditions as you drag), or LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE strum   ·   M autoplay on/off ('P' is the runtime pause overlay)
//
// MULTITOUCH: every finger is its own pointer — bend two strings at once, strum with one
// hand while another holds a slider. The desktop mouse is one synthetic finger, same path.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_STR 5
#define NSTR  8

static const char STRKEY[NSTR] = { 'A','S','D','F','G','H','J','K' };
// row 1 = the 3 engine macros; row 2 = TUNING controls (weight/attack via instrument_mode(), width via
// per-note instrument_pan()) — scaffolding to dial the sound in by ear, baked to constants after.
static const char *KNOB_NAME[6] = { "body", "string", "mute", "weight", "attack", "width" };

// hardware presets = baked (harmonics, timbre, morph) triples — the playbook's acceptance test
#define NPRESET 8
static const char *PRESET_NAME[NPRESET] = { "harp","nylon","steel","banjo","koto","uke","pizz","oud" };
static const float PRESET[NPRESET][3] = {
    { 0.05f, 0.55f, 0.05f },   // harp     — open/clear, long ring
    { 0.45f, 0.20f, 0.22f },   // nylon    — mid body, warm, medium decay
    { 0.55f, 0.70f, 0.20f },   // steel    — resonant box, bright, medium
    { 0.95f, 0.85f, 0.45f },   // banjo    — bright resonant box, short twang
    { 0.80f, 0.90f, 0.28f },   // koto     — bright high body, long ring
    { 0.60f, 0.40f, 0.42f },   // ukulele  — small warm box, shorter
    { 0.20f, 0.60f, 0.85f },   // pizzicato— little body, tight mute
    { 0.50f, 0.20f, 0.55f },   // oud      — round dark body, short
};

static int   midi_of[NSTR];
static float amp[NSTR];        // visual string vibration, decays each frame
static float vib_ph[NSTR];
static int   pend[NSTR];       // frames until a scheduled strum note visually plucks
static float knob[6] = { 0.55f, 0.6f, 0.2f, 0.4f, 0.25f, 0.0f };   // macros + weight/attack/width
static int   sel = 0;
static int   preset = 2;       // which preset last loaded (steel), for the readout
static bool  autoplay = true;
static int   apos = 0;
static int   goct = 3;         // base octave for the strings (Z/X shift it)

static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) midi_of[s] = degree(SCALE_PENTA, goct, s + 2);
}

enum { PTR_IDLE, PTR_DRAG, PTR_BEND, PTR_PICK };
typedef struct { int id, mode, k, s, h, y0, x, y, prevY; } Ptr;   // id MUST be first (pointer.h)
static Ptr   ptr[PTR_MAX];     // .id == PTR_NONE → slot free
static float bowSt[NSTR];      // live bend per string, semitones (bows the drawn string)

// slider geometry — shared by draw() and the touch hit-test
#define KNOB_W    88
#define KNOB_TOP  (SCREEN_H - 52)            // row 1 y; row 2 sits 26px below
#define KNOB_X(k) (14 + ((k) % 3) * 102)
#define KNOB_Y(k) (KNOB_TOP + ((k) < 3 ? 0 : 26))

static void apply_knobs(void) {
    instrument_harmonics(I_STR, knob[0]);
    instrument_timbre(I_STR, knob[1]);
    instrument_morph(I_STR, knob[2]);
    instrument_mode(I_STR, MODE_STRING_WEIGHT, knob[3]);   // TUNING: fundamental weight
    instrument_mode(I_STR, MODE_STRING_CLICK, knob[4]);   // TUNING: attack click
}

// gate scales with mute (morph): a tight pizz lets go fast, an open ring holds the voice long
static int gate_ms(void) { return 600 + (int)((1.0f - knob[2]) * (1.0f - knob[2]) * 14000.0f); }

static void pluck_str(int s, int vol) {
    instrument_pan(I_STR, (s / (float)(NSTR - 1) - 0.5f) * 1.8f * knob[5]);   // TUNING: width = pan by string
    hit(midi_of[s], I_STR, vol, gate_ms());   // long gate — the string decides its own decay
    amp[s]    = 1.0f;
    vib_ph[s] = 0.0f;
}

static void load_preset(int p) {
    preset  = p;
    knob[0] = PRESET[p][0];
    knob[1] = PRESET[p][1];
    knob[2] = PRESET[p][2];
    apply_knobs();
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
    instrument(I_STR, INSTR_GUITAR, 1, 0, 7, 1200);
    apply_knobs();
    PTR_CLEAR(ptr);
    build_strings();
    bpm(96);
    amp[1] = 0.7f; amp[4] = 0.9f; amp[6] = 0.5f;   // a lively first frame
}

void update(void) {
    for (int s = 0; s < NSTR; s++) {
        if (keyp(STRKEY[s])) pluck_str(s, 6);
        if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }
    }

    // presets on the number row — the acceptance test (auditions a string on load)
    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p)) { load_preset(p); pluck_str(4, 6); }

    // knobs: pick with LEFT/RIGHT, turn with UP/DOWN (held); audition every 12 frames
    if (keyp(KEY_LEFT))  sel = (sel + 5) % 6;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 6;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        apply_knobs();
        if (frame() % 12 == 0) pluck_str(4, 5);
    }

    if (keyp(KEY_SPACE)) strum_all();
    if (keyp('M')) autoplay = !autoplay;   // M like the radio carts ('P' is the runtime pause key)
    if (keyp('Z') && goct > 1) { goct--; build_strings(); }   // octave down
    if (keyp('X') && goct < 6) { goct++; build_strings(); }   // octave up

    // touch: every finger is its own pointer — grab a slider, grab a string (bend), or
    // pick open space (strum), all independently and at once
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
            for (int k = 0; k < 6; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y(k) - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            // ...or the string area: ON a string = grab it (bend); open space = a pick (strum)
            if (p->mode == PTR_IDLE && ty < KNOB_TOP - 8) {
                int grabbed = -1;
                for (int s = 0; s < NSTR; s++)
                    if (ty >= 42 + s * 13 - 4 && ty <= 42 + s * 13 + 4) grabbed = s;
                for (int j = 0; j < PTR_MAX; j++)                 // one bending finger per string
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
    print("GUITAR", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("karplus-strong string + body resonator", 64, 8, CLR_MEDIUM_GREY);
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

    // two rows of knobs: row 1 = macros, row 2 = weight / attack / width (tuning)
    for (int k = 0; k < 6; k++) {
        int x = KNOB_X(k), y = KNOB_Y(k);
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : k < 3 ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 6, knob[k], on ? CLR_ORANGE : k < 3 ? CLR_BROWN : CLR_DARKER_GREY, CLR_DARKER_GREY);
        if (on) print(">", x - 9, y - 1, CLR_YELLOW);
    }

    // a pick under every strumming finger
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].mode == PTR_PICK)
            trifill(ptr[j].x - 3, ptr[j].y - 4, ptr[j].x + 3, ptr[j].y - 4,
                    ptr[j].x, ptr[j].y + 4, CLR_LIGHT_PEACH);

    // preset row (1..8) — the named acceptance tests; the loaded one highlights
    font(FONT_TINY);
    int prx = 10;
    for (int p = 0; p < NPRESET; p++)
        prx = print(str("%d %s  ", p + 1, PRESET_NAME[p]), prx, 20,
                    p == preset ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print(str("A..K pluck   bend / strum   Z/X octave (%d)   1..8 preset", goct),
          10, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
