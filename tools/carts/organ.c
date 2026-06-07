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
// spin-up inertia. Press L to cycle OFF / slow (chorale) / fast (tremolo) and hear the
// engine + its companion recipe together — the whole §8.10/0015 story in one cart.
//
// THE QUESTION THIS CART ANSWERS (ADR-0016): is `morph` a FULL, distinct axis, or thin? Sweep
// it under a held chord. Full -> combo organ folds onto it later; thin -> combo needs its own
// engine. Listen, then record the verdict.
//
// controls: A S D F G H J K   hold a key (chords welcome — hold several)
//           Z / X             octave down / up
//           1..8  presets: reggae combo bookerT jimmy larry ballad jonlord gospel
//           drag a slider (morphs every held note LIVE), or LEFT/RIGHT pick + UP/DOWN turn
//           L Leslie (off/slow/fast)   ·   M autoplay on/off
// MULTITOUCH: every finger is its own pointer — hold keys with several fingers while another
// rides a slider; the desktop mouse arrives as one synthetic finger.

#include "studio.h"
#include <math.h>

#define I_ORG 5
#define NKEY  8

static const char KEYCH[NKEY] = { 'A','S','D','F','G','H','J','K' };

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

// key geometry — a row of manual keys
#define KEY_W   34
#define KEY_GAP 4
#define KEY_X(b) (14 + (b) * (KEY_W + KEY_GAP))
#define KEY_Y    44
#define KEY_H    74

// slider geometry — shared by draw() and the hit-test
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 30)
#define KNOB_X(k) (14 + (k) * 102)

static int midi_of(int b) { return degree(SCALE_MAJOR, octave, b + 1); }

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
    for (int b = 0; b < NKEY; b++) {
        if (keyp(KEYCH[b])) key_down(b);
        if (keyr(KEYCH[b])) key_up(b);
    }
    if (keyp('Z') && octave > 1) octave--;
    if (keyp('X') && octave < 7) octave++;

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

    // touch: every finger holds a key or drags a slider, independently
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
            if (ty >= KNOB_Y - 26 && ty < KNOB_Y - 12) {    // preset labels tappable
                for (int q = 0; q < 8; q++)
                    if (tx >= 12 + q * 38 && tx < 12 + q * 38 + 36) { set_preset(q); break; }
                continue;
            }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            if (p->mode == PTR_IDLE && ty >= KEY_Y && ty < KEY_Y + KEY_H) {
                for (int b = 0; b < NKEY; b++)
                    if (point_in_box(tx, ty, KEY_X(b), KEY_Y, KEY_W, KEY_H)) {
                        key_down(b); p->mode = PTR_KEY; p->key = b; break;
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
    watch("leslie", "%d", leslie);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("ORGAN", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("tonewheel drawbar engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the Leslie rotor — a spinning bar, speed = the live (eased) rate, so you SEE the spin-up
    rotor_ph += 0.02f + leslie_rate * 0.06f;
    int rx = SCREEN_W - 26, ry = 26, rr = 9;
    const char *lname = leslie == 2 ? "fast" : leslie == 1 ? "slow" : "off";
    circ(rx, ry, rr, leslie ? CLR_ORANGE : CLR_DARK_GREY);
    line(rx - (int)(cosf(rotor_ph) * rr), ry - (int)(sinf(rotor_ph) * rr),
         rx + (int)(cosf(rotor_ph) * rr), ry + (int)(sinf(rotor_ph) * rr),
         leslie ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
    font(FONT_TINY);
    print_right(str("L leslie %s", lname), rx - rr - 4, ry - 3, leslie ? CLR_ORANGE : CLR_DARK_GREY);
    print_right(str("oct %d  Z/X", octave), SCREEN_W - 10, 18, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the manual
    for (int b = 0; b < NKEY; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.90f;
        bool down = handle[b] >= 0;
        int col = down ? CLR_LIGHT_YELLOW : glow[b] > 0.1f ? CLR_PEACH : CLR_LIGHT_PEACH;
        rectfill(x, KEY_Y, KEY_W, KEY_H, col);
        rect(x, KEY_Y, KEY_W, KEY_H, down ? CLR_WHITE : CLR_DARK_BROWN);
        print(str("%c", KEYCH[b]), x + KEY_W / 2 - 2, KEY_Y + KEY_H - 12,
              down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
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
    print("A..K hold (chords)   1..8 presets   L leslie   drag a slider (morphs held notes live)",
          10, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
