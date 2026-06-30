/* de:meta
{
  "title": "organ",
  "status": "active",
  "created": "2026-06-07",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "additive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "INSTR_ORGAN engine showcase and tuning rig — the Leslie is a verbatim navkit processLeslie port (band-split rotary with mechanical rotor inertia), replacing an earlier per-voice LFO recipe.",
  "description": "INSTR_ORGAN showcase - the fourth modeled ENGINE: every key sums nine drawbar sines at the Hammond footages, so what you hear is a registration (a recipe of harmonics), not one wave. Unlike pluck/mallet it is HELD - hold a key and it sustains, so the macros morph the tone LIVE while you lean on a chord. The same three macros every engine answers: instrument_harmonics = registration (snapped drawbar recipes, thin reggae to full gospel); instrument_timbre = brightness tilt + key click; instrument_morph = animation (0 still combo organ, up = scanner chorus shimmer + percussion chip). Eight presets (1 reggae, 2 combo, 3 bookerT, 4 jimmy, 5 larry, 6 ballad, 7 jonlord, 8 gospel) are baked knob positions. THE LESLIE is not in the engine - it is a per-voice recipe (tremolo + doppler LFOs, rate lerping slow/fast = the spin-up inertia); press L to hear it. A S D F G H J K hold keys (chords welcome), Z/X octave, 1..8 presets, drag a slider (morphs held notes live) or LEFT/RIGHT + UP/DOWN, L leslie, M autoplay. Multitouch.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
// organ — INSTR_ORGAN showcase: a tonewheel manual + the three engine macros + drive + Leslie.
//
// The fourth modeled ENGINE: every key sums NINE drawbar sines at the Hammond footages, so
// what you hear is a registration — a recipe of harmonics, not a single wave. Unlike pluck
// and mallet (struck, ring down on their own), the organ is HELD: hold a key and it sustains,
// release and it stops. So this cart is a little keyboard, and the macros morph the held tone
// LIVE while you lean on a chord — which is exactly how you judge them. The same three knobs:
//   harmonics = registration  (snapped recipes — thin reggae .. full gospel; each detent a sound)
//   timbre    = brightness     (dark drawbars .. bright + key click)
//   morph     = animation      (0 still combo organ .. scanner chorus shimmer + percussion chip)
// ...plus a fourth slider, DRIVE — NOT a macro, it's the per-voice overdrive (instrument_drive):
// the cranked-tube / overdriven-Leslie growl. Bright registration + drive = the combo-organ
// approximation from decision 0016 — push it and hear why.
//
// The named instruments are just KNOB POSITIONS (audio-notes §8.1 / §8.8.4): if pressing
// "jimmy" doesn't sound like a jazz B3, the MAPPING is wrong, not the preset — this cart is
// the engine's tuning rig.
//
// THE LESLIE is the real bus rotary speaker — instrument_leslie(), a VERBATIM navkit processLeslie
// port (an 800 Hz horn/drum crossover, a Doppler delay-line on the horn, two rotors with their own
// spin-up/down inertia). It REPLACED an earlier per-voice tremolo+doppler recipe: decision 0015
// had refused a Leslie effect as "just a recipe of the 3 LFOs," but building the real one proved an
// LFO can't band-split or model a moving source — so the gate flipped its own verdict (0015 correction
// 2026-06-12). Cycle OFF / slow (chorale) / fast (tremolo); the engine ramps the rotors, so the
// slow<->fast SWELL is real mechanical inertia now, not a cart-side lerp — held chords swirl too.
//
// controls: white keys  A S D F G H J K   ·   black keys  W E . T Y U
//           hold for sustain (chords welcome — hold several)
//           Z / X  octave down / up   ·   1..8 presets   ·   L leslie   ·   M autoplay
//           drag a slider (morphs every held note LIVE), or LEFT/RIGHT pick + UP/DOWN turn
// MULTITOUCH: every finger is its own pointer — hold keys with several fingers while another
// rides a slider; tap the on-screen octave +/- and Leslie buttons. The desktop mouse arrives
// as one synthetic finger, so the same code path covers it.

#include "studio.h"
#define KEYBED_WHITE_KEYS "ASDFGHJK"   // one-octave-plus-C manual (matches the organ's layout)
#define KEYBED_BLACK_KEYS "WE TYU"
#include "keybed.h"
#include <math.h>

#define I_ORG 5

// a piano octave: 8 white keys (C..C) + 5 black keys. handles/glow index white 0..7 then
// black 8..12. WSEMI/BSEMI = semitone offsets from the octave's C; BWHICH = which white key
// each black sits to the right of (no black after E(2) or B(6)).
#define NWHITE 8                                          // 8 white keys = one octave + the top C
static const char WKEY[NWHITE] = { 'A','S','D','F','G','H','J','K' };   // QWERTY labels (drawing only)
static const char BLBL[NWHITE] = { 'W','E', 0 ,'T','Y','U', 0 , 0 };   // black-key label after white k

// four sliders: the THREE engine macros + DRIVE (an effect, not a macro — styled apart).
#define NSL 4
enum { SL_HARM, SL_TIMB, SL_MOR, SL_DRIVE };
static const char *SL_NAME[NSL] = { "harmonics", "timbre", "morph", "drive" };
static const char *SL_LO[NSL]   = { "thin", "dark",  "still",   "clean" };
static const char *SL_HI[NSL]   = { "full", "bright","shimmer", "growl" };

// presets = slider positions with a hardware name (drive baked in — jon lord growls). The
// harmonics value lands on the matching registration detent (8 detents, h ~ (i+0.5)/8).
typedef struct { const char *name; float v[NSL]; } Preset;   // v = {harm, timb, morph, drive}
static const Preset PRESET[8] = {
    { "reggae",  { 0.06f, 0.55f, 0.00f, 0.00f } },   // hollow upstroke, no motion
    { "combo",   { 0.19f, 0.45f, 0.30f, 0.00f } },   // soft cocktail combo
    { "bookerT", { 0.31f, 0.45f, 0.22f, 0.00f } },   // 60s clean, light chorus
    { "jimmy",   { 0.44f, 0.55f, 0.75f, 0.00f } },   // fat jazz B3, perc + C3 chorus
    { "larry",   { 0.56f, 0.60f, 0.65f, 0.00f } },   // modern jazz
    { "ballad",  { 0.69f, 0.60f, 0.55f, 0.00f } },   // sub+fund+sparkle
    { "jonlord", { 0.81f, 0.70f, 0.40f, 0.30f } },   // rock growl — baked DRIVE (the combo-grit preview)
    { "gospel",  { 0.94f, 0.65f, 0.88f, 0.00f } },   // all bars out, full shimmer + perc
};

static float val[NSL] = { 0.44f, 0.55f, 0.75f, 0.0f };   // boot on "jimmy"
static int   sel = 0;
static int   cur_preset = 3;
static bool  autoplay = true;

// the Leslie: 0 off, 1 slow (chorale), 2 fast (tremolo). NOW the real bus rotary (instrument_leslie
// = navkit's processLeslie) — the engine spins the rotors up/down with real inertia. rotor_rate
// here is VISUAL-ONLY, easing toward the target just to animate the on-screen rotor. (NOT `leslie`
// — that name is now the leslie() API.)
static int   lesmode = 0;
static float rotor_rate = 0.0f;
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

// slider geometry — four across; shared by draw() and the hit-test
#define KNOB_W   64
#define KNOB_Y   (SCREEN_H - 30)
#define KNOB_X(k) (12 + (k) * 76)

// push all four sliders onto a single voice (held key or autoplay comp): the three engine
// macros + the drive effect. Called live so dragging morphs every sounding note.
static void apply_voice(int h) {
    note_harmonics(h, val[SL_HARM]);
    note_timbre(h, val[SL_TIMB]);
    note_morph(h, val[SL_MOR]);
    note_drive(h, val[SL_DRIVE]);
}

// apply to the slot template AND to every held voice (the live morph — the point of the cart)
static void apply_live(void) {
    instrument_harmonics(I_ORG, val[SL_HARM]);
    instrument_timbre(I_ORG, val[SL_TIMB]);
    instrument_morph(I_ORG, val[SL_MOR]);
    instrument_drive(I_ORG, val[SL_DRIVE]);
    for (int m = 0; m < 128; m++) if (keybed_held(m)) apply_voice(keybed_handle(m));
    for (int i = 0; i < 3; i++)    if (ap_h[i] >= 0)   apply_voice(ap_h[i]);
}

// THE LESLIE — the real bus rotary speaker (instrument_leslie = navkit's processLeslie, ported
// VERBATIM: an 800 Hz horn/drum crossover, a Doppler delay-line on the horn, two rotors with
// independent spin-up/down inertia). This REPLACED the old per-voice tremolo+doppler RECIPE —
// decision 0015 reversed its "Leslie is a recipe" refusal once the real thing proved an LFO can't
// band-split or model a moving source. Set on the slot from init (mix 0 = bypass) so the organ is
// routed through its Leslie bus from the first key — then flipping L just changes the mix/speed and
// even HELD chords swirl. The engine ramps the rotors, so slow<->fast gives the spin-up swell free.
static void apply_leslie(void) {
    int speed = (lesmode == 2) ? LESLIE_FAST : LESLIE_SLOW;
    instrument_leslie(I_ORG, speed, 0.0f, 0.5f, 0.7f, lesmode ? 1.0f : 0.0f);   // off → mix 0 = dry organ
}

static void octave_step(int d) { keybed_octave_shift(d); }   // keybed drops held notes + clamps

static void set_preset(int p) {
    for (int k = 0; k < NSL; k++) val[k] = PRESET[p].v[k];
    cur_preset = p;
    apply_live();
}

void init(void) {
    instrument(I_ORG, INSTR_ORGAN, 1, 0, 7, 200);   // gate unused for held notes; release tail
    keybed_config(I_ORG, 4, NWHITE);                 // base C4, 8 white keys
    keybed_layout(KEY_X(0), KEY_Y, NWHITE * (KEY_W + KEY_GAP), KEY_H);
    keybed_glissando(false);                         // an organ retriggers — sliding a finger shouldn't slur
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    set_preset(3);
    apply_leslie();    // route I_ORG through its Leslie bus from note 1 (mix 0 = bypass until L)
    bpm(84);
}

void update(void) {
    keybed_update();    // keys: touch + QWERTY (ASDF../WETYU) + MIDI + Z/X octave (no glissando)
    // fresh + held voices follow the live macros (the point of the cart)
    for (int m = 0; m < 128; m++) if (keybed_held(m)) apply_voice(keybed_handle(m));

    for (int p = 0; p < 8; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSL - 1) % NSL;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSL;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        val[sel] = clamp(val[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_live();
    }

    if (keyp('L')) { lesmode = (lesmode + 1) % 3; apply_leslie(); }
    if (keyp('M')) autoplay = !autoplay;

    // the on-screen rotor's spin: a VISUAL ease toward the target speed (the AUDIO rotor inertia is
    // the engine's now). off → idle, slow → ~0.8 Hz, fast → ~6.6 Hz (navkit's horn rates).
    float target = lesmode == 2 ? 6.6f : lesmode == 1 ? 0.8f : 0.0f;
    rotor_rate += (target - rotor_rate) * 0.06f;

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
            if (point_in_box(tx, ty, LES_X, LES_Y, LES_W, LES_H)) { lesmode = (lesmode + 1) % 3; apply_leslie(); continue; }
            if (point_in_box(tx, ty, OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(-1); continue; }
            if (point_in_box(tx, ty, OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(+1); continue; }
            if (ty >= KNOB_Y - 26 && ty < KNOB_Y - 12) {    // preset labels tappable
                for (int q = 0; q < 8; q++)
                    if (tx >= 12 + q * 38 && tx < 12 + q * 38 + 36) { set_preset(q); break; }
                continue;
            }
            for (int k = 0; k < NSL; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
            // keys themselves are handled by keybed_update() (touches over the key area)
        } else if (p->mode == PTR_DRAG) {
            val[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_live();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)      // free the slider/button pointer slots
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

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
                apply_voice(ap_h[i]);
            }
            ap_step++;
        }
    } else {
        for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", val[SL_HARM]);
    watch("timb", "%.2f", val[SL_TIMB]);
    watch("mor",  "%.2f", val[SL_MOR]);
    watch("drive","%.2f", val[SL_DRIVE]);
    watch("preset", "%d", cur_preset);
    watch("octave", "%d", keybed_octave());
    watch("leslie", "%d", lesmode);
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
    print_scaled(str("%d", keybed_octave()), OCT_DN_X + 26, OCT_BTN_Y, CLR_LIGHT_YELLOW, 2);
    rectfill(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_DARK_BROWN);
    rect(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_BROWN);
    print("X", OCT_UP_X + 7, OCT_BTN_Y + 5, CLR_LIGHT_PEACH);

    // LESLIE button — a spinning rotor (speed = the live eased rate, so you SEE the spin-up)
    rotor_ph += 0.02f + rotor_rate * 0.06f;
    rectfill(LES_X, LES_Y, LES_W, LES_H, lesmode ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
    rect(LES_X, LES_Y, LES_W, LES_H, lesmode ? CLR_ORANGE : CLR_DARK_GREY);
    int rx = LES_X + 12, ry = LES_Y + LES_H / 2, rr = 7;
    line(rx - (int)(cosf(rotor_ph) * rr), ry - (int)(sinf(rotor_ph) * rr),
         rx + (int)(cosf(rotor_ph) * rr), ry + (int)(sinf(rotor_ph) * rr),
         lesmode ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(str("L leslie %s", lesmode == 2 ? "fast" : lesmode == 1 ? "slow" : "off"),
          LES_X + 24, ry - 3, lesmode ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the manual — keybed.h owns layout/voices/glow; we draw it in the organ's colours
    int nw = keybed_white_count();
    for (int b = 0; b < nw; b++) {
        int x, y, w, h; keybed_white_rect(b, &x, &y, &w, &h);
        int midi = keybed_white_midi(b); bool down = keybed_held(midi);
        int col = down ? CLR_LIGHT_YELLOW : keybed_glow(midi) > 0.1f ? CLR_PEACH : CLR_LIGHT_PEACH;
        rectfill(x, y, w - 1, h, col);
        rect(x, y, w - 1, h, down ? CLR_WHITE : CLR_DARK_BROWN);
        print(str("%c", WKEY[b]), x + w / 2 - 2, y + h - 12, down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }
    for (int b = 0; b < nw; b++) {
        int x, y, w, h, midi; if (!keybed_black_rect(b, &x, &y, &w, &h, &midi)) continue;
        bool down = keybed_held(midi);
        int col = down ? CLR_ORANGE : keybed_glow(midi) > 0.1f ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK;
        rectfill(x, y, w, h, col);
        rect(x, y, w, h, down ? CLR_LIGHT_YELLOW : CLR_DARK_BROWN);
        if (BLBL[b]) { font(FONT_TINY); print(str("%c", BLBL[b]), x + w / 2 - 1, y + h - 9, down ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL); }
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

    // the four sliders — 3 engine macros + DRIVE (an effect, tinted apart in red/orange)
    for (int k = 0; k < NSL; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        bool fx = (k == SL_DRIVE);
        font(FONT_SMALL);
        print(SL_NAME[k], x, y - 8, on ? CLR_YELLOW : fx ? CLR_DARK_ORANGE : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, val[k], on ? CLR_ORANGE : fx ? CLR_DARK_ORANGE : CLR_BROWN, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(SL_LO[k], x, y + 9, CLR_DARK_GREY);
        print_right(SL_HI[k], x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    print("white A..K  black W E T Y U   Z/X octave   1..8 presets   drag a slider (live)   L leslie",
          8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
