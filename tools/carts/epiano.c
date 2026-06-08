// epiano — INSTR_EPIANO showcase: a piano manual + the three engine macros + a WAH.
//
// The fifth modeled ENGINE: Rhodes / Wurlitzer / Clavinet in ONE. Every key sums 12 decaying
// inharmonic sine modes and pushes them through a PICKUP NONLINEARITY — the bark/buzz/honk
// that makes it sound electric instead of a dull bell. It's STRUCK and rings down on its own
// (like pluck/mallet): hold a key and it sustains+decays, release and the damper stops it.
// The same three knobs every engine answers:
//   harmonics = instrument  (snapped: Rhodes ·· Wurli ·· Clav — each its own ratio table + pickup)
//   timbre    = brightness  (mellow, centered pickup .. bright/snappy, offset + hard hammer)
//   morph     = bark        (0 clean fundamental .. dig-in growl — the pickup driven hard)
//
// THE WAH is a recipe — a resonant BANDPASS swept (the moving peak IS the wah vowel). Toggle
// off / AUTO / TOUCH: AUTO is an LFO (rhythmic, plays regardless of touch); TOUCH is the
// envelope FOLLOWER — it opens the bandpass from the note's own amplitude (fast attack, slow
// release), so it responds to how hard you play. That's the funky envelope filter (the clav
// preset's Stevie "Superstition" sound). (The DX/digital EP is INSTR_FM; this is the real one.)
//
// The named instruments are just KNOB POSITIONS (audio-notes §8.1 / §8.8.5): if pressing
// "wurli" doesn't sound like a Wurlitzer, the MAPPING is wrong, not the preset.
//
// controls: white keys  A S D F G H J K   ·   black keys  W E . T Y U
//           Z / X  octave   ·   1..6 presets   ·   V wah (off/auto/env)   ·   M autoplay
//           drag a slider (re-strikes to audition), or LEFT/RIGHT pick + UP/DOWN turn
// MULTITOUCH: every finger is its own pointer; tap the on-screen octave +/- and wah buttons.

#include "studio.h"
#include <math.h>

#define I_EP 5

#define NWHITE 8
#define NBLACK 5
#define NKEY   (NWHITE + NBLACK)
static const char WKEY[NWHITE]  = { 'A','S','D','F','G','H','J','K' };
static const int  WSEMI[NWHITE] = { 0, 2, 4, 5, 7, 9, 11, 12 };
static const char BKEY[NBLACK]  = { 'W','E','T','Y','U' };
static const int  BSEMI[NBLACK] = { 1, 3, 6, 8, 10 };
static const int  BWHICH[NBLACK]= { 0, 1, 3, 4, 5 };

#define NSL 4
enum { SL_INST, SL_BRITE, SL_BARK, SL_WAH };
static const char *SL_NAME[NSL] = { "instrument", "bright", "bark", "wah" };
static const char *SL_LO[NSL]   = { "rhodes", "mellow", "clean", "subtle" };
static const char *SL_HI[NSL]   = { "clav",   "bright", "growl", "deep" };
static const char *INSTRUMENT[3]= { "RHODES", "WURLI", "CLAV" };
static const char *WAHNAME[3]   = { "off", "auto", "touch" };

// presets = slider positions + a wah mode. harmonics lands on an instrument detent.
typedef struct { const char *name; float v[NSL]; int wah; } Preset;
static const Preset PRESET[6] = {
    { "rhodes",   { 0.15f, 0.30f, 0.25f, 0.5f }, 0 },   // warm suitcase-ish
    { "rho brite",{ 0.15f, 0.78f, 0.55f, 0.5f }, 0 },   // bright stage, barky
    { "suitcase", { 0.15f, 0.20f, 0.12f, 0.5f }, 0 },   // mellow, clean, long
    { "wurli",    { 0.50f, 0.35f, 0.30f, 0.5f }, 0 },   // soul ballad
    { "wur buzz", { 0.50f, 0.66f, 0.82f, 0.6f }, 1 },   // cranked reed + auto-wah movement
    { "clav",     { 0.85f, 0.75f, 0.55f, 0.6f }, 2 },   // funky bridge pickup + TOUCH-WAH (envelope follower — the Stevie "Superstition" envelope filter, responds to how hard you play)
};

static int   handle[NKEY];
static float glow[NKEY];
static int   octave = 4;
static float val[NSL] = { 0.15f, 0.30f, 0.25f, 0.5f };   // boot on "rhodes"
static int   sel = 0;
static int   cur_preset = 0;
static int   wah = 0;            // 0 off, 1 auto (LFO_CUTOFF), 2 env (ENV_CUTOFF quack)
static bool  autoplay = true;

static int   ap_h[3] = { -1, -1, -1 };
static int   ap_step = 0;
static int   aud_h = -1;         // audition scratch voice (so preset/slider strikes carry the wah)

#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_KEY };
typedef struct { int id, mode, k, key; } Ptr;
static Ptr ptr[NPTR];

#define KEY_W   36
#define KEY_GAP 1
#define KEY_X(b) (10 + (b) * (KEY_W + KEY_GAP))
#define KEY_Y    48
#define KEY_H    72
#define BLACK_W  20
#define BLACK_H  42
#define BLACK_X(k) (KEY_X(BWHICH[k]) + KEY_W - BLACK_W / 2)

#define OCT_DN_X 12
#define OCT_UP_X 56
#define OCT_BTN_Y 24
#define OCT_BTN_W 20
#define OCT_BTN_H 18
#define WAH_X (SCREEN_W - 92)
#define WAH_Y 22
#define WAH_W 86
#define WAH_H 20

#define KNOB_W   64
#define KNOB_Y   (SCREEN_H - 30)
#define KNOB_X(k) (12 + (k) * 76)

static int midi_of(int idx) {
    int base = (octave + 1) * 12;
    return base + (idx < NWHITE ? WSEMI[idx] : BSEMI[idx - NWHITE]);
}

static void apply_slot(void) {
    instrument_harmonics(I_EP, val[SL_INST]);
    instrument_timbre(I_EP, val[SL_BRITE]);
    instrument_morph(I_EP, val[SL_BARK]);
}

// THE WAH RECIPE — all slot-level, so every strike (keys, auditions, autoplay) inherits it.
// wah is just the per-voice SVF swept: AUTO = an LFO on the cutoff; ENV = the cutoff envelope
// opening on each strike (the funky-clav quack). No engine code — see decision 0015.
static void apply_wah(void) {
    float amt = val[SL_WAH];
    if (wah == 0) {
        instrument_filter(I_EP, FILTER_OFF, 4000, 0);
        instrument_lfo(I_EP, 0, LFO_CUTOFF, 0.0f, 0.0f);
        instrument_follow(I_EP, LFO_CUTOFF, 0, 0, 0.0f);
    } else if (wah == 1) {                       // AUTO: an LFO sweeps a resonant BANDPASS (the
        // moving resonant peak IS the wah vowel). The sweep STAYS in a musical band (~400-2200
        // Hz) — the old wide depth dove to the 20Hz clamp where a bandpass passes nothing, so
        // the wah pulsed to mud. Centre 1300 ± (400..900). Rhythmic, plays regardless of touch.
        instrument_filter(I_EP, FILTER_BAND, 1300, 11);
        instrument_follow(I_EP, LFO_CUTOFF, 0, 0, 0.0f);
        instrument_lfo(I_EP, 0, LFO_CUTOFF, 1.5f + amt * 6.0f, 400.0f + amt * 500.0f);
    } else {                                     // TOUCH: the envelope FOLLOWER opens the bandpass
        // from the note's own amplitude (fast attack, slow release) — the funky envelope-filter
        // that responds to how hard you play. Centre opens ~350 -> ~2300, capped (not 3kHz+).
        instrument_filter(I_EP, FILTER_BAND, 350, 11);
        instrument_lfo(I_EP, 0, LFO_CUTOFF, 0.0f, 0.0f);
        instrument_follow(I_EP, LFO_CUTOFF, 3, 200, 1000.0f + amt * 1500.0f);
    }
}

static void key_down(int b) {
    if (handle[b] >= 0) { note_off(handle[b]); handle[b] = -1; }
    handle[b] = note_on(midi_of(b), I_EP, 6);
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
    for (int b = 0; b < NKEY; b++) key_up(b);
    octave = n;
}

static void audition(void) {
    if (aud_h >= 0) note_off(aud_h);
    aud_h = note_on(midi_of(3), I_EP, 5);
    glow[3] = 1.0f;
}

static void set_preset(int p) {
    for (int k = 0; k < NSL; k++) val[k] = PRESET[p].v[k];
    wah = PRESET[p].wah;
    cur_preset = p;
    apply_slot(); apply_wah();
    audition();
}

void init(void) {
    instrument(I_EP, INSTR_EPIANO, 1, 0, 7, 1500);   // long gate: never chop the ring
    for (int b = 0; b < NKEY; b++) handle[b] = -1;
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    apply_slot(); apply_wah();
    bpm(76);
}

void update(void) {
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

    for (int p = 0; p < 6; p++) if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSL - 1) % NSL;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSL;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        val[sel] = clamp(val[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_slot(); apply_wah();
        if (frame() % 14 == 0) audition();
    }

    if (keyp('V')) { wah = (wah + 1) % 3; apply_wah(); audition(); }
    if (keyp('M')) autoplay = !autoplay;

    for (int b = 0; b < NKEY; b++) if (handle[b] >= 0) note_morph(handle[b], val[SL_BARK]);

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) { autoplay = !autoplay; continue; }
            if (point_in_box(tx, ty, WAH_X, WAH_Y, WAH_W, WAH_H)) { wah = (wah + 1) % 3; apply_wah(); audition(); continue; }
            if (point_in_box(tx, ty, OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(-1); continue; }
            if (point_in_box(tx, ty, OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H)) { octave_step(+1); continue; }
            if (ty >= KNOB_Y - 26 && ty < KNOB_Y - 12) {
                for (int q = 0; q < 6; q++)
                    if (tx >= 12 + q * 50 && tx < 12 + q * 50 + 48) { set_preset(q); break; }
                continue;
            }
            for (int k = 0; k < NSL; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_DRAG; p->k = sel = k;
                }
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
            val[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_slot(); apply_wah();
            if (frame() % 14 == 0) audition();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) {
                if (ptr[j].mode == PTR_KEY && ptr[j].key >= 0) key_up(ptr[j].key);
                ptr[j].id = NOID;
            }

    if (autoplay) {
        if (every(2)) {
            static const int ROOT[4] = { 50, 55, 48, 57 };
            static const int THIRD[4]= { 3, 4, 4, 3 };
            for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
            int r = ROOT[ap_step % 4];
            int notes[3] = { r, r + THIRD[ap_step % 4], r + 7 };
            for (int i = 0; i < 3; i++) { ap_h[i] = note_on(notes[i], I_EP, 4); note_morph(ap_h[i], val[SL_BARK]); }
            ap_step++;
        }
    } else {
        for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", val[SL_INST]);
    watch("timb", "%.2f", val[SL_BRITE]);
    watch("bark", "%.2f", val[SL_BARK]);
    watch("wah",  "%d", wah);
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("EPIANO", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    int inst = (int)(val[SL_INST] * 2.999f); if (inst > 2) inst = 2;
    print(INSTRUMENT[inst], 64, 8, CLR_PEACH);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 6,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // OCTAVE control
    print("OCTAVE", OCT_DN_X, 14, CLR_MEDIUM_GREY);
    rectfill(OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_DARK_BROWN);
    rect(OCT_DN_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_BROWN);
    print("Z", OCT_DN_X + 7, OCT_BTN_Y + 5, CLR_LIGHT_PEACH);
    print_scaled(str("%d", octave), OCT_DN_X + 26, OCT_BTN_Y, CLR_LIGHT_YELLOW, 2);
    rectfill(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_DARK_BROWN);
    rect(OCT_UP_X, OCT_BTN_Y, OCT_BTN_W, OCT_BTN_H, CLR_BROWN);
    print("X", OCT_UP_X + 7, OCT_BTN_Y + 5, CLR_LIGHT_PEACH);

    // WAH button (tappable; off/auto/env)
    bool won = (wah != 0);
    rectfill(WAH_X, WAH_Y, WAH_W, WAH_H, won ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
    rect(WAH_X, WAH_Y, WAH_W, WAH_H, won ? CLR_ORANGE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(str("V wah: %s", WAHNAME[wah]), WAH_X + 6, WAH_Y + 6, won ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the manual
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
    for (int p = 0; p < 6; p++) {
        int x = 12 + p * 50;
        bool on = (p == cur_preset);
        print(str("%d", p + 1), x, KNOB_Y - 24, on ? CLR_YELLOW : CLR_DARK_GREY);
        font(FONT_TINY);
        print(PRESET[p].name, x + 7, KNOB_Y - 23, on ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
        font(FONT_SMALL);
    }
    font(FONT_NORMAL);

    // the four sliders — 3 engine macros + WAH amount (an effect, tinted apart)
    for (int k = 0; k < NSL; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        bool fx = (k == SL_WAH);
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
    print("white A..K  black W E T Y U   Z/X octave   1..6 presets   V wah   drag a slider",
          8, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
