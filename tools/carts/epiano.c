// epiano — INSTR_EPIANO showcase: a piano manual + the three engine macros + a WAH + TREMOLO.
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
// THE WAH (V toggles off / on) — the funky-clav-through-a-wah, three layers stacked:
//   • a per-note resonant filter QUACK (FILTER_LOW + ENV_CUTOFF) — each note's bite, the navkit
//     "Clav Funky" filter-env (a real D6 uses exactly this per-note envelope filter)
//   • a per-voice LFO PUMP on that filter — the rhythmic "wah-WAH-wah" (a follower alone opens
//     once on attack and can't oscillate, so the motion is its own LFO; ~2.5-6 Hz)
//   • the bus FOLLOWER (instrument_wah) — the dynamic "talking" character on the summed signal
// The clav preset boots into it. The super-70s funk; navkit's Clav-Funky filter-env + a wah pedal.
// (Earlier this cart toured auto/env/navkit flavours too — cut once only this one earned its place
//  by ear; the rest collapsed to "quack + an LFO at some rate," a slider not a mode. The realistic
//  auto-wah being a BUS effect, not a per-voice filter, is the "wah detour" scar — 0015 / §8.10.1.)
// (The DX/digital EP is INSTR_FM; this is the real one.)
//
// THE ENGINE got two funk upgrades (2026-06-11, sound.h): a velocity+hardness-scaled TANGENT
// CLICK on attack (the clav's percussive chink — navkit clickLevel 0.35; subtle on Rhodes/Wurli)
// and a velocity→drive link on the clav nonlinearity (dig in → more honk). Plus filter
// key-tracking here (ep_keytrack) so the quack/wah floor opens higher up the keyboard.
//
// The named instruments are just KNOB POSITIONS (audio-notes §8.1 / §8.8.5): if pressing
// "wurli" doesn't sound like a Wurlitzer, the MAPPING is wrong, not the preset.
//
// controls: white keys  A S D F G H J K   ·   black keys  W E . T Y U
//           Z / X  octave   ·   1..6 presets   ·   V wah (off/on)   ·   M autoplay
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

#define NSL 5
enum { SL_INST, SL_BRITE, SL_BARK, SL_WAH, SL_TREM };
static const char *SL_NAME[NSL] = { "instrument", "bright", "bark", "wah", "tremolo" };
static const char *SL_LO[NSL]   = { "rhodes", "mellow", "clean", "subtle", "off" };
static const char *SL_HI[NSL]   = { "clav",   "bright", "growl", "deep",   "throb" };
static const char *INSTRUMENT[3]= { "RHODES", "WURLI", "CLAV" };
static const char *WAHNAME[2]   = { "off", "on" };

// presets = slider positions + a wah mode. harmonics lands on an instrument detent.
typedef struct { const char *name; float v[NSL]; int wah; } Preset;
static const Preset PRESET[6] = {
    { "rhodes",   { 0.15f, 0.30f, 0.25f, 0.5f, 0.35f }, 0 },   // warm suitcase-ish + classic wobble
    { "rho brite",{ 0.15f, 0.78f, 0.55f, 0.5f, 0.30f }, 0 },   // bright stage, barky
    { "suitcase", { 0.15f, 0.20f, 0.12f, 0.5f, 0.45f }, 0 },   // mellow, clean, long, deep tremolo
    { "wurli",    { 0.50f, 0.35f, 0.30f, 0.5f, 0.50f }, 0 },   // soul ballad — the 200A's deeper trem
    { "wur buzz", { 0.50f, 0.66f, 0.82f, 0.6f, 0.55f }, 1 },   // cranked reed + the wah + trem
    { "clav",     { 0.85f, 0.75f, 0.55f, 0.6f, 0.0f  }, 1 },   // funky bridge pickup THROUGH the wah (quack + LFO pump + bus follower, navkit's clav+wah pairing). NO tremolo — a real clav has none
};

static int   handle[NKEY];
static float glow[NKEY];
static int   octave = 4;
static float val[NSL] = { 0.15f, 0.30f, 0.25f, 0.5f, 0.35f };   // boot on "rhodes"
static int   sel = 0;
static int   cur_preset = 0;
static int   wahmode = 0;        // 0 off, 1 on (the funky-clav wah: quack + per-voice LFO pump + bus follower). NOT `wah` — clashes with the wah() API

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

#define KNOB_W   52
#define KNOB_Y   (SCREEN_H - 30)
#define KNOB_X(k) (8 + (k) * 62)

static int midi_of(int idx) {
    int base = (octave + 1) * 12;
    return base + (idx < NWHITE ? WSEMI[idx] : BSEMI[idx - NWHITE]);
}

// bark's lower half = the pickup nonlinearity (morph); its upper half ALSO folds in tanh DRIVE
// — two stacked dirts (pickup bark + amp growl), the navkit driven-EP funk, on one knob.
static float bark_drive(float bark) { return bark > 0.5f ? (bark - 0.5f) * 1.4f : 0.0f; }  // 0 → 0.7

static void apply_slot(void) {
    // damper RELEASE tracks the instrument: a clav has a tight damper (~140ms, like navkit's
    // Clav-Funky 120ms), a Rhodes tine rings longer after you lift the key. (SR_INSTR only sets
    // wave+ADSR, so re-calling instrument() here leaves the macros below untouched.)
    int ty = (int)(val[SL_INST] * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    instrument(I_EP, INSTR_EPIANO, 1, 0, 7, ty == 2 ? 140 : ty == 1 ? 280 : 450);   // clav / wurli / rhodes
    instrument_harmonics(I_EP, val[SL_INST]);
    instrument_timbre(I_EP, val[SL_BRITE]);
    instrument_morph(I_EP, val[SL_BARK]);
    // drive (post-filter soft-clip): rhodes/wurli get growl from bark at the top. The CLAV gets a
    // FIXED 0.10 — navkit's Clav-Funky reads exactly "Drive: Soft, 0.10" (the slight saturation
    // that rounds it / "a bit more muted"; DRIVE_SOFT/tanh is our default mode = navkit's Soft).
    // Its honk comes from morph→pickupDist in the oscillator, so the post-filter drive stays fixed.
    instrument_drive(I_EP, ty == 2 ? 0.20f : bark_drive(val[SL_BARK]));
}

// THE WAH — slot-level, so every strike (keys, auditions, autoplay) inherits it. One flavour now,
// the funky-clav-through-a-wah that won the A/B vs navkit: a per-note resonant filter QUACK (the
// voice's own bite) + a per-voice LFO PUMP (the rhythmic "wah-WAH-wah", since a follower alone
// opens once on attack and can't oscillate) feeding the bus envelope FOLLOWER (the dynamic
// character). navkit's Clav-Funky filter-env + a wah pedal, in one. amt = the wah slider.
// (Earlier this cart toured auto/env/navkit flavours too — cut once only this one earned its
//  place by ear; the difference between them collapsed to LFO rate, a slider not a mode.)
static void apply_wah(void) {
    float amt = val[SL_WAH];
    int  ty   = (int)(val[SL_INST] * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    bool clav = (ty == 2);
    bool on   = (wahmode != 0);
    instrument_lfo(I_EP, 0, LFO_CUTOFF, 0.0f, 0.0f);     // clear all three; re-armed below
    instrument_env(I_EP, 0, ENV_CUTOFF, 0, 0, 0.0f);
    instrument_wah(I_EP, 0.0f, 0.0f, 0.0f);              // bus follower off unless re-armed (mix 0 = bypass)

    // BASELINE VOICE FILTER — the CLAV is filtered by default, exactly like navkit's Clav-Funky
    // preset (it always has filterEnabled): a static resonant lowpass + a fast per-note QUACK.
    // This rolls off the pickup nonlinearity's inharmonic intermod that a bare clav exposes as
    // mid-band "noisy bell" hash (proven against navkit's render — its clav is NEVER unfiltered).
    // Rhodes/Wurli stay open unless the wah is on (their gentler nonlinearity doesn't need it).
    if (clav) {
        instrument_filter(I_EP, FILTER_LOW, 1500, 6);                // gentle — keeps H4/H5 like navkit (the
        instrument_env(I_EP, 0, ENV_CUTOFF, 2, 100, 1600.0f);        // verbatim osc is clean, so don't over-filter).
                                                                     // env = the funk quack: opens on strike, ~100ms
    } else if (on) {
        instrument_filter(I_EP, FILTER_LOW, 700, 9);
        instrument_env(I_EP, 0, ENV_CUTOFF, 2, 110, 1700.0f + amt * 700.0f);
    } else {
        instrument_filter(I_EP, FILTER_OFF, 4000, 0);
    }

    if (on) {                                    // WAH MOTION on top of the voice: LFO pump + bus follower
        instrument_lfo(I_EP, 0, LFO_CUTOFF, 2.5f + amt * 3.5f, 500.0f + amt * 900.0f);
        instrument_wah(I_EP, 0.4f + amt * 0.6f, 0.45f + amt * 0.4f, 0.75f + amt * 0.25f);
    }
}

// filter KEY-TRACKING — the per-note quack/wah floor opens higher up the keyboard (so high
// comps stay bright, not muffled). navkit filterKeyTrack 0.6. Base + ENV_CUTOFF are additive
// (sound.h), so raising the per-note base lifts the floor the quack settles to. Only meaningful
// when the wah is on (its per-note filter is live); call right after every note_on.
static void ep_keytrack(int h, int midi) {
    if (h < 0 || wahmode == 0) return;               // only when the wah's per-note filter is live
    note_cutoff(h, 300 + (midi - 36) * 14);          // ~+14 Hz/semitone above C2
}

// THE TREMOLO RECIPE — the suitcase/Wurli amp wobble, the "electric" signature: an LFO on the
// voice VOLUME (~5-6 Hz, depth scales with the slider). Slot-level so every strike inherits it.
// LFO slot 1 — the wah owns slot 0 (LFO_VOLUME and LFO_CUTOFF run independently). Rhodes &
// Wurli want this (the 200A's is deeper); the clav preset zeroes it — a real clav has no tremolo.
static void apply_trem(void) {
    float amt = val[SL_TREM];
    instrument_lfo(I_EP, 1, LFO_VOLUME, 5.0f + amt * 1.5f, amt * 0.85f);
}

static void key_down(int b) {
    if (handle[b] >= 0) { note_off(handle[b]); handle[b] = -1; }
    handle[b] = note_on(midi_of(b), I_EP, 6);
    ep_keytrack(handle[b], midi_of(b));
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
    ep_keytrack(aud_h, midi_of(3));
    glow[3] = 1.0f;
}

static void set_preset(int p) {
    for (int k = 0; k < NSL; k++) val[k] = PRESET[p].v[k];
    wahmode = PRESET[p].wah;
    cur_preset = p;
    apply_slot(); apply_wah(); apply_trem();
    audition();
}

void init(void) {
    instrument(I_EP, INSTR_EPIANO, 1, 0, 7, 450);    // defines the slot; apply_slot() then sets release per type
    for (int b = 0; b < NKEY; b++) handle[b] = -1;
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    apply_slot(); apply_wah(); apply_trem();
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
        apply_slot(); apply_wah(); apply_trem();
        if (frame() % 14 == 0) audition();
    }

    if (keyp('V')) { wahmode = (wahmode + 1) % 2; apply_wah(); audition(); }
    if (keyp('M')) autoplay = !autoplay;

    for (int b = 0; b < NKEY; b++) if (handle[b] >= 0) {   // live bark on ringing notes (morph + drive)
        note_morph(handle[b], val[SL_BARK]);
        note_drive(handle[b], bark_drive(val[SL_BARK]));
    }

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
            if (point_in_box(tx, ty, WAH_X, WAH_Y, WAH_W, WAH_H)) { wahmode = (wahmode + 1) % 2; apply_wah(); audition(); continue; }
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
            apply_slot(); apply_wah(); apply_trem();
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
            for (int i = 0; i < 3; i++) { ap_h[i] = note_on(notes[i], I_EP, 4); note_morph(ap_h[i], val[SL_BARK]); ep_keytrack(ap_h[i], notes[i]); }
            ap_step++;
        }
    } else {
        for (int i = 0; i < 3; i++) if (ap_h[i] >= 0) { note_off(ap_h[i]); ap_h[i] = -1; }
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", val[SL_INST]);
    watch("timb", "%.2f", val[SL_BRITE]);
    watch("bark", "%.2f", val[SL_BARK]);
    watch("trem", "%.2f", val[SL_TREM]);
    watch("wah",  "%d", wahmode);
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

    // WAH button (tappable; off/on). On = the funky-clav wah (quack + LFO pump + bus follower) —
    // see the header note + decision 0015.
    bool won = (wahmode != 0);
    rectfill(WAH_X, WAH_Y, WAH_W, WAH_H, won ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
    rect(WAH_X, WAH_Y, WAH_W, WAH_H, won ? CLR_ORANGE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(str("V wah: %s", WAHNAME[wahmode]), WAH_X + 6, WAH_Y + 6, won ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
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
        bool fx = (k == SL_WAH || k == SL_TREM);
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
