// organ — INSTR_ORGAN showcase: a tonewheel manual + the three engine macros + the Leslie.
//
// The fourth modeled ENGINE: every key sums NINE drawbar sines at the Hammond footages, so
// what you hear is a registration — a recipe of harmonics, not a single wave. Unlike pluck
// and mallet (struck, ring down on their own), the organ is HELD: hold a key and it sustains,
// release and it stops. So this cart is a little keyboard, and the macros morph the held tone
// LIVE while you lean on a chord — which is exactly how you judge them. The same three knobs:
//   harmonics = registration  (snapped recipes — thin reggae .. full gospel; each detent a sound)
//   timbre    = brightness     (dark drawbars .. bright + key click)
//   morph     = animation      (0 still combo organ .. scanner chorus shimmer + percussion chip)
//
// The named instruments are just KNOB POSITIONS (audio-notes §8.1 / §8.8.4): if pressing
// "jimmy" doesn't sound like a jazz B3, the MAPPING is wrong, not the preset — this cart is
// the engine's tuning rig.
//
// THE LESLIE is NOT in the engine — it's a per-voice RECIPE (decision 0015): tremolo
// (LFO_VOLUME) + doppler (LFO_PITCH), the rate lerping slow<->fast = the horn's mechanical
// spin-up inertia. Cycle OFF / slow (chorale) / fast (tremolo) and hear the engine + its
// companion recipe together — the whole §8.10/0015 story in one cart.
//
// controls: white keys  A S D F G H J K   ·   black keys  W E . T Y U
//           hold for sustain (chords welcome — hold several)
//           Z / X  octave down / up   ·   1..8 presets   ·   L leslie   ·   M autoplay
//           drag a slider (morphs every held note LIVE), or LEFT/RIGHT pick + UP/DOWN turn
// MULTITOUCH: every finger is its own pointer — hold keys with several fingers while another
// rides a slider; tap the on-screen octave +/- and Leslie buttons. The desktop mouse arrives
// as one synthetic finger, so the same code path covers it.

#include "studio.h"
#include <math.h>

#define I_ORG 5

// a piano octave: 8 white keys (C..C) + 5 black keys. handles/glow index white 0..7 then
// black 8..12. WSEMI/BSEMI = semitone offsets from the octave's C; BWHICH = which white key
// each black sits to the right of (no black after E(2) or B(6)).
#define NWHITE 8
#define NBLACK 5
#define NKEY   (NWHITE + NBLACK)
static const char WKEY[NWHITE]  = { 'A','S','D','F','G','H','J','K' };
static const int  WSEMI[NWHITE] = { 0, 2, 4, 5, 7, 9, 11, 12 };
static const char BKEY[NBLACK]  = { 'W','E','T','Y','U' };
static const int  BSEMI[NBLACK] = { 1, 3, 6, 8, 10 };
static const int  BWHICH[NBLACK]= { 0, 1, 3, 4, 5 };

static const char *KNOB_NAME[3] = { "harmonics (reg)", "timbre (bright)", "morph (anim)" };
static const char *KNOB_LO[3]   = { "thin",  "dark",  "still" };
static const char *KNOB_HI[3]   = { "full",  "bright","shimmer" };

// presets = knob positions with a hardware name (+ a baked drive — jon lord growls). The
// harmonics value lands on the matching registration detent (8 detents, h ~ (i+0.5)/8).
typedef struct { const char *name; float h, t, m, drv; } Preset;
static const Preset PRESET[8] = {
    { "reggae",  0.06f, 0.55f, 0.00f, 0.00f },   // hollow upstroke, no motion
    { "combo",   0.19f, 0.45f, 0.30f, 0.00f },   // soft cocktail combo
    { "bookerT", 0.31f, 0.45f, 0.22f, 0.00f },   // 60s clean, light chorus
    { "jimmy",   0.44f, 0.55f, 0.75f, 0.00f },   // fat jazz B3, perc + C3 chorus
    { "larry",   0.56f, 0.60f, 0.65f, 0.00f },   // modern jazz
    { "ballad",  0.69f, 0.60f, 0.55f, 0.00f },   // sub+fund+sparkle
    { "jonlord", 0.81f, 0.70f, 0.40f, 0.18f },   // rock growl (baked drive — the combo-grit preview)
    { "gospel",  0.94f, 0.65f, 0.88f, 0.00f },   // all bars out, full shimmer + perc
};

static int   handle[NKEY];     // held note_on handle per key (-1 = up)
static float glow[NKEY];       // visual key-down flash
static int   octave = 4;
static float knob[3] = { 0.44f, 0.55f, 0.75f };   // boot on "jimmy"
static float drv = 0.0f;
static int   sel = 0;
static int   cur_preset = 3;
static bool  autoplay = true;

// the Leslie recipe: 0 off, 1 slow (chorale), 2 fast (tremolo). rate lerps toward target.
static int   leslie = 0;
static float leslie_rate = 0.0f;
static float rotor_ph = 0.0f;

// autoplay state — a managed 3-voice held comp that walks a gospel-ish progression
static int   ap_h[3] = { -1, -1, -1 };
static int   ap_step = 0;

// per-finger pointer table (mallet pattern): a finger holds a key or drags a slider
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_KEY };
typedef struct { int id, mode, k, key; } Ptr;
static Ptr ptr[NPTR];

// key geometry — a row of white keys with black keys overlaid
#define KEY_W   36
#define KEY_GAP 1
#define KEY_X(b) (10 + (b) * (KEY_W + KEY_GAP))
#define KEY_Y    48
#define KEY_H    72
#define BLACK_W  20
#define BLACK_H  42
#define BLACK_X(k) (KEY_X(BWHICH[k]) + KEY_W - BLACK_W / 2)

// top-bar button hit zones (shared by draw + touch)
#define OCT_DN_X 12
#define OCT_UP_X 56
#define OCT_BTN_Y 24
#define OCT_BTN_W 20
#define OCT_BTN_H 18
#define LES_X (SCREEN_W - 96)
#define LES_Y 22
#define LES_W 90
#define LES_H 20

// slider geometry — shared by draw() and the hit-test
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 30)
#define KNOB_X(k) (14 + (k) * 102)

static int midi_of(int idx) {
    int base = (octave + 1) * 12;
    return base + (idx < NWHITE ? WSEMI[idx] : BSEMI[idx - NWHITE]);
}

// apply the macros to the slot template AND to every held voice (the live morph — the point
// of the cart). Leslie LFOs ride the same held voices when engaged.
static void apply_live(void) {
    instrument_harmonics(I_ORG, knob[0]);
    instrument_timbre(I_ORG, knob[1]);
    instrument_morph(I_ORG, knob[2]);
    instrument_drive(I_ORG, drv);
    for (int b = 0; b < NKEY; b++) {
        if (handle[b] < 0) continue;
        note_harmonics(handle[b], knob[0]);
        note_timbre(handle[b], knob[1]);
        note_morph(handle[b], knob[2]);
        note_drive(handle[b], drv);
    }
    for (int i = 0; i < 3; i++) {
        if (ap_h[i] < 0) continue;
        note_harmonics(ap_h[i], knob[0]);
        note_timbre(ap_h[i], knob[1]);
        note_morph(ap_h[i], knob[2]);
        note_drive(ap_h[i], drv);
    }
}

// push the Leslie recipe (tremolo + doppler) onto every sounding voice at the current rate
static void apply_leslie(void) {
    float depth_v = leslie ? 0.45f : 0.0f;       // volume tremolo
    float depth_p = leslie ? 0.16f : 0.0f;       // pitch doppler (small — it's a rotation, not vibrato)
    instrument_lfo(I_ORG, 0, LFO_VOLUME, leslie_rate, depth_v);
    instrument_lfo(I_ORG, 1, LFO_PITCH,  leslie_rate, depth_p);
    for (int b = 0; b < NKEY; b++) if (handle[b] >= 0) {
        note_lfo(handle[b], 0, LFO_VOLUME, leslie_rate, depth_v);
        note_lfo(handle[b], 1, LFO_PITCH,  leslie_rate, depth_p);
    }
    for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) {
        note_lfo(ap_h[i], 0, LFO_VOLUME, leslie_rate, depth_v);
        note_lfo(ap_h[i], 1, LFO_PITCH,  leslie_rate, depth_p);
    }
}

static void key_down(int b) {
    if (handle[b] >= 0) return;
    handle[b] = note_on(midi_of(b), I_ORG, 6);
    note_harmonics(handle[b], knob[0]);
    note_timbre(handle[b], knob[1]);
    note_morph(handle[b], knob[2]);
    note_drive(handle[b], drv);
    glow[b] = 1.0f;
}
static void key_up(int b) {
    if (handle[b] < 0) return;
    note_off(handle[b]);
    handle[b] = -1;
}

static void octave_step(int d) {
    int n = octave + d;
    if (n < 1 || n > 7) return;
    for (int b = 0; b < NKEY; b++) key_up(b);   // drop held notes so they don't stick at the old pitch
    octave = n;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m; drv = PRESET[p].drv;
    cur_preset = p;
    apply_live();
}

void init(void) {
    instrument(I_ORG, INSTR_ORGAN, 1, 0, 7, 200);   // gate unused for held notes; release tail
    for (int b = 0; b < NKEY; b++) handle[b] = -1;
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    set_preset(3);
    bpm(84);
}

void update(void) {
    // manual keys — held while down (chords: hold several)
    for (int b = 0; b < NWHITE; b++) {
        if (keyp(WKEY[b])) key_down(b);
        if (keyr(WKEY[b])) key_up(b);
    }
    for (int k = 0; k < NBLACK; k++) {
        if (keyp(BKEY[k])) key_down(NWHITE + k);
        if (keyr(BKEY[k])) key_up(NWHITE + k);
    }
    if (keyp('Z')) octave_step(-1);
    if (keyp('X')) octave_step(+1);

    for (int p = 0; p < 8; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_live();
    }

    if (keyp('L')) leslie = (leslie + 1) % 3;
    if (keyp('M')) autoplay = !autoplay;

    // the Leslie spin-up: rate eases toward its target (the mechanical inertia IS the lerp)
    float target = leslie == 2 ? 6.6f : leslie == 1 ? 0.8f : 0.0f;
    leslie_rate += (target - leslie_rate) * 0.06f;
    apply_leslie();

    // touch: every finger holds a key, taps a button, or drags a slider, independently
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {                                  // finger just landed
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) { autoplay = !autoplay; continue; }
            if (point_in_box(tx, ty, LES_X, LES_Y, LES_W, LES_H)) { leslie = (leslie + 1) % 3; continue; }
            if (point_in_box(tx, ty, OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(-1); continue; }
            if (point_in_box(tx, ty, OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(+1); continue; }
            if (ty >= KNOB_Y - 26 && ty < KNOB_Y - 12) {    // preset labels tappable
                for (int q = 0; q < 8; q++)
                    if (tx >= 12 + q * 38 && tx < 12 + q * 38 + 36) { set_preset(q); break; }
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            // keys: black sit ON TOP, so test them first
            if (p->mode == PTR_IDLE) {
                for (int k = 0; k < NBLACK && p->mode == PTR_IDLE; k++)
                    if (point_in_box(tx, ty, BLACK_X(k), KEY_Y, BLACK_W, BLACK_H)) {
                        key_down(NWHITE + k); p->mode = PTR_KEY; p->key = NWHITE + k;
                    }
            }
            if (p->mode == PTR_IDLE && ty >= KEY_Y && ty < KEY_Y + KEY_H) {
                for (int b = 0; b < NWHITE && p->mode == PTR_IDLE; b++)
                    if (point_in_box(tx, ty, KEY_X(b), KEY_Y, KEY_W, KEY_H)) {
                        key_down(b); p->mode = PTR_KEY; p->key = b;
                    }
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_live();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)      // lifted fingers release their key + slot
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) {
                if (ptr[j].mode == PTR_KEY && ptr[j].key >= 0) key_up(ptr[j].key);
                ptr[j].id = NOID;
            }

    // autoplay: a managed 3-voice held comp through a gospel-ish ii-V-I-vi, the scanner
    // (morph) gently breathing so the live-macro motion is always audible
    if (autoplay) {
        if (every(2)) {
            static const int ROOT[4] = { 50, 55, 48, 57 };   // ii  V  I  vi (D G C A) in C
            static const int TYPE[4] = { CHORD_MIN7, CHORD_DOM7, CHORD_MAJ7, CHORD_MIN7 };
            for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
            int r = ROOT[ap_step % 4];
            int chord_notes[3] = { r, r + (TYPE[ap_step % 4] == CHORD_MIN7 ? 3 : 4), r + 7 };
            for (int i = 0; i < 3; i++) {
                ap_h[i] = note_on(chord_notes[i], I_ORG, 4);
                note_harmonics(ap_h[i], knob[0]);
                note_timbre(ap_h[i], knob[1]);
                note_morph(ap_h[i], knob[2]);
            }
            ap_step++;
        }
    } else {
        for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("preset", "%d", cur_preset);
    watch("octave", "%d", octave);
    watch("leslie", "%d", leslie);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("ORGAN", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("tonewheel drawbar engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 6,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // OCTAVE control — prominent, with tappable - / + buttons (keys Z / X)
    print("OCTAVE", OCT_DN_X, 14, CLR_MEDIUM_GREY);
    rectfill(OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_DARK_BROWN);
    rect(OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_BROWN);
    print("Z", OCT_DN_X + 7, OCT_BTN_Y + 5, CLR_LIGHT_PEACH);
    print_scaled(str("%d", octave), OCT_DN_X + 26, OCT_BTN_Y, CLR_LIGHT_YELLOW, 2);
    rectfill(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_DARK_BROWN);
    rect(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_BROWN);
    print("X", OCT_UP_X + 7, OCT_BTN_Y + 5, CLR_LIGHT_PEACH);

    // LESLIE button — a spinning rotor (speed = the live eased rate, so you SEE the spin-up)
    rotor_ph += 0.02f + leslie_rate * 0.06f;
    rectfill(LES_X, LES_Y, LES_W, LES_H, leslie ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
    rect(LES_X, LES_Y, LES_W, LES_H, leslie ? CLR_ORANGE : CLR_DARK_GREY);
    int rx = LES_X + 12, ry = LES_Y + LES_H / 2, rr = 7;
    line(rx - (int)(cosf(rotor_ph) * rr), ry - (int)(sinf(rotor_ph) * rr),
         rx + (int)(cosf(rotor_ph) * rr), ry + (int)(sinf(rotor_ph) * rr),
         leslie ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(str("L leslie %s", leslie == 2 ? "fast" : leslie == 1 ? "slow" : "off"),
          LES_X + 24, ry - 3, leslie ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the manual — white keys first, then black keys overlaid on top
    for (int b = 0; b < NWHITE; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.90f;
        bool down = handle[b] >= 0;
        int col = down ? CLR_LIGHT_YELLOW : glow[b] > 0.1f ? CLR_PEACH : CLR_LIGHT_PEACH;
        rectfill(x, KEY_Y, KEY_W, KEY_H, col);
        rect(x, KEY_Y, KEY_W, KEY_H, down ? CLR_WHITE : CLR_DARK_BROWN);
        print(str("%c", WKEY[b]), x + KEY_W / 2 - 2, KEY_Y + KEY_H - 12,
              down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }
    for (int k = 0; k < NBLACK; k++) {
        int x = BLACK_X(k), idx = NWHITE + k;
        glow[idx] *= 0.90f;
        bool down = handle[idx] >= 0;
        int col = down ? CLR_ORANGE : glow[idx] > 0.1f ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK;
        rectfill(x, KEY_Y, BLACK_W, BLACK_H, col);
        rect(x, KEY_Y, BLACK_W, BLACK_H, down ? CLR_LIGHT_YELLOW : CLR_DARK_BROWN);
        font(FONT_TINY);
        print(str("%c", BKEY[k]), x + BLACK_W / 2 - 1, KEY_Y + BLACK_H - 9,
              down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    // preset row
    font(FONT_SMALL);
    for (int p = 0; p < 8; p++) {
        int x = 12 + p * 38;
        bool on = (p == cur_preset);
        print(str("%d", p + 1), x, KNOB_Y - 24, on ? CLR_YELLOW : CLR_DARK_GREY);
        font(FONT_TINY);
        print(PRESET[p].name, x + 7, KNOB_Y - 23, on ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
        font(FONT_SMALL);
    }
    font(FONT_NORMAL);

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

    font(FONT_TINY);
    print("white A..K  black W E T Y U  hold (chords)   Z/X octave   1..8 presets   drag a slider (live)",
          8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
