// MT-70 — a 1982 Casiotone home keyboard, rebuilt with ZERO new engine code.
//
// THE PROBE (instrument-engines.md §8.9 "MT70", cart-library-direction §2b.7):
// the real MT-70 mixes 2 (sometimes 3) sine oscillators per key, each with its
// own decay. dreamengine has no second oscillator — and this cart proves it
// doesn't need one: a sine at ratio R is just ANOTHER NOTE, R-as-semitones up,
// on its own slot with its own faster ADSR. Two voices summed by the mixer ==
// two oscillators mixed by the hardware. Exact ratios via note_pitch() floats
// (ratio 3.0 = +19.02 st — integer rounding would BEAT against the fundamental).
//
//   press a key  ->  note_on(fund, slot5) + note_on(partial, slot6) [+ slot7]
//                    note_pitch(h, exact_float)   // the 2-cent correction
//   lift         ->  note_off both — gate-based, exactly like the hardware
//
// Preset data transcribed from navkit instrument_presets.h:2813 (the corrected
// 2026-06-07 reading: osc2Ratio/Level/Decay are non-zero in 9 of 10 presets).
// SRC switch = the probe's A/B: struck presets can also play on INSTR_MALLET
// (one voice, macro positions) — ear decides if the engine corner is enough.
//
// controls: touch the keys (multitouch, slide = glissando, lift = release)
//           Z S X D C V B H N J M , = lower octave on the computer keyboard
//           1..9,0 presets · A vibrato · K src 2-osc/mallet · SPACE demo tune
#include "studio.h"
#include <stdio.h>

// ---------------------------------------------------------------- slots
#define SL_FUND   5   // fundamental sine
#define SL_OSC2   6   // 2nd partial (own decay)
#define SL_OSC3   7   // 3rd partial (Bells/Banjo)
#define SL_MALLET 8   // the A/B alternative for struck presets
#define SL_CLICK  9   // key-click transient (Cosmic/JzOrg2)

// ---------------------------------------------------------------- presets
// adsr ms · sustain 0..7 · lowpass Hz · per-layer vol 0..7 · partial offsets in
// EXACT semitones (12*log2(ratio)) · partial decay ms (0 = follow the main env)
typedef struct {
    const char *name, *quote;
    int   a, d, s, r, cut, vol;          // fundamental (slot 5)
    float o2_st; int o2_vol, o2_d;       // 2nd sine    (slot 6)
    float o3_st; int o3_vol, o3_d;       // 3rd sine    (slot 7)
    int   click_vol;                     // noise tick at press (slot 9)
    bool  struck;                        // eligible for the MALLET A/B
    float mh, mt, mm;                    // mallet macro position
} P;

static const P PRE[10] = {
//  name      quote (from the hardware notes)            a    d    s  r    cut  v   o2st    v  o2d    o3st    v  o3d  clk strk  mallet h,t,m
{ "PIANO",  "plain sine + decay: the dullest piano ever", 2,  800, 0, 300, 2400, 5,  0.0f,  0,   0,   0.0f,  0,   0,  0, false, 0,0,0 },
{ "ORGAN",  "fundamental + sustained 3rd harmonic",      10,  100, 6,  80, 3400, 4, 19.02f, 2,   0,   0.0f,  0,   0,  0, false, 0,0,0 },
{ "FLUTE",  "octave overtone fades to a pure sine",      50,  100, 5, 120, 2850, 5, 12.00f, 1, 830,   0.0f,  0,   0,  0, false, 0,0,0 },
{ "VIBES",  "4:1 partial decays first - the realistic one",2,1200, 0, 400, 4000, 5, 24.00f, 2, 500,   0.0f,  0,   0,  0, true,  0.25f,0.50f,0.90f },
{ "CHIME",  "two tones a perfect 4th apart, same decay",  1, 1500, 0, 500, 5700, 5,  5.01f, 4,   0,   0.0f,  0,   0,  0, true,  0.80f,0.60f,0.55f },
{ "BELLS",  "three sines: +0, +2 and +1 octaves",         1, 1000, 0, 600, 6800, 4, 24.00f, 2, 625,  12.00f, 2,1000,  0, true,  0.90f,0.70f,0.60f },
{ "COSMIC", "click, then high octave fades to medium",    2,  150, 4, 150, 3400, 5, 12.00f, 4, 415,   0.0f,  0,   0,  2, false, 0,0,0 },
{ "JZORG",  "Hammond with a percussive key click",        2,  100, 5,  60, 2850, 4, 19.02f, 1, 310,   0.0f,  0,   0,  3, false, 0,0,0 },
{ "CELSTA", "music box; bass resembles steel drum",       1,  600, 0, 300, 5700, 5, 19.02f, 3, 625,   0.0f,  0,   0,  0, true,  0.50f,0.55f,0.45f },
{ "BANJO",  "hollow + resonant: fast-fading octave",      1,  400, 0, 150, 3400, 5, 12.00f, 3, 310,  19.02f, 2, 210,  0, true,  0.10f,0.90f,0.10f },
};

static int  preset  = 3;       // boot on VIBES, "one of the more realistic"
static bool vibrato = false;
static bool use_mallet = false;
static bool demo    = false;

// ---------------------------------------------------------------- keyboard
#define BASE_MIDI 48           // C3 — two octaves up from here
#define NSEMI     25           // C3..C5 inclusive
#define NWHITE    15
#define KB_X0     ((SCREEN_W - NWHITE * WKW) / 2)
#define WKW       21
#define KEY_TOP   88
#define BLACK_H   62
#define BLACK_W   16           // the phone tap-target floor

static const int wsemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int bsemi[7] = { 1, 3, -1, 6, 8, 10, -1 };

#define NOFINGER (-999)
#define FNG_KB   (-1234)       // "held by the computer keyboard"
#define FNG_DEMO (-5678)       // "held by the demo sequencer"
typedef struct { int handle[3], nh; int finger; long since; } KeyState;
static KeyState ks[NSEMI];

// one octave of computer keys, classic layout: Z=C S=C# X=D ... M=B ,=C
static const char KBMAP[13] = { 'Z','S','X','D','C','V','G','B','H','N','J','M',',' };

// ---------------------------------------------------------------- voices
static int voices_used(void) {
    int n = 0;
    for (int s = 0; s < NSEMI; s++) n += ks[s].nh;
    return n;
}

static void release_semi(int s) {
    for (int i = 0; i < ks[s].nh; i++) note_off(ks[s].handle[i]);
    ks[s].nh = 0;
    ks[s].finger = NOFINGER;
}

// program slots 5/6/7 (+8) for the current preset; partials with o2_d==0 follow
// the main envelope (the Organ's sustaining 3rd, the Chime's equal decay)
static void apply_preset(void) {
    const P *p = &PRE[preset];
    instrument(SL_FUND, INSTR_SINE, p->a, p->d, p->s, p->r);
    instrument_filter(SL_FUND, FILTER_LOW, p->cut, 0);
    if (p->o2_vol) {
        if (p->o2_d) instrument(SL_OSC2, INSTR_SINE, p->a, p->o2_d, 0, p->r);
        else         instrument(SL_OSC2, INSTR_SINE, p->a, p->d, p->s, p->r);
        instrument_filter(SL_OSC2, FILTER_LOW, p->cut, 0);
    }
    if (p->o3_vol) {
        instrument(SL_OSC3, INSTR_SINE, p->a, p->o3_d ? p->o3_d : p->d, p->o3_d ? 0 : p->s, p->r);
        instrument_filter(SL_OSC3, FILTER_LOW, p->cut, 0);
    }
    if (p->struck) {
        instrument_harmonics(SL_MALLET, p->mh);
        instrument_timbre(SL_MALLET, p->mt);
        instrument_morph(SL_MALLET, p->mm);
    }
    // vibrato rides slots 5..7 — applies to the NEXT notes; held ones get note_lfo below
    float depth = vibrato ? 0.25f : 0.0f;
    for (int sl = SL_FUND; sl <= SL_OSC3; sl++) instrument_lfo(sl, 0, LFO_PITCH, 5.5f, depth);
    instrument_lfo(SL_MALLET, 0, LFO_PITCH, 5.5f, depth);
}

static void vibrato_live(void) {            // push the toggle onto ringing notes too
    float depth = vibrato ? 0.25f : 0.0f;
    for (int s = 0; s < NSEMI; s++)
        for (int i = 0; i < ks[s].nh; i++)
            note_lfo(ks[s].handle[i], 0, LFO_PITCH, 5.5f, depth);
}

// the press: 1-3 stacked sines, float-corrected — or one mallet voice in SRC B
static void press_semi(int s, int finger) {
    const P *p = &PRE[preset];
    int need = (use_mallet && p->struck) ? 1 : 1 + (p->o2_vol ? 1 : 0) + (p->o3_vol ? 1 : 0);

    // budget guard: never ask the engine to steal a held note — retire the
    // oldest key ourselves (and watch it: this ceiling is the probe's data)
    while (voices_used() + need > 15) {
        int old = -1;
        for (int k = 0; k < NSEMI; k++)
            if (ks[k].nh && (old < 0 || ks[k].since < ks[old].since)) old = k;
        if (old < 0) break;
        release_semi(old);
    }

    float fm = (float)(BASE_MIDI + s);
    KeyState *K = &ks[s];
    if (K->nh) release_semi(s);              // demo/finger collision: no orphans
    K->nh = 0;
    if (use_mallet && p->struck) {
        K->handle[K->nh++] = note_on(BASE_MIDI + s, SL_MALLET, 6);
    } else {
        K->handle[K->nh++] = note_on(BASE_MIDI + s, SL_FUND, p->vol);
        if (p->o2_vol) {
            int h = note_on(BASE_MIDI + s + (int)(p->o2_st + 0.5f), SL_OSC2, p->o2_vol);
            note_pitch(h, fm + p->o2_st);          // the exact-ratio correction
            K->handle[K->nh++] = h;
        }
        if (p->o3_vol) {
            int h = note_on(BASE_MIDI + s + (int)(p->o3_st + 0.5f), SL_OSC3, p->o3_vol);
            note_pitch(h, fm + p->o3_st);
            K->handle[K->nh++] = h;
        }
        if (p->click_vol) hit(BASE_MIDI + s + 24, SL_CLICK, p->click_vol, 8);
    }
    K->finger = finger;
    K->since  = frame();
}

// ---------------------------------------------------------------- demo tune
// Ode to Joy, eighth notes, melody + alternating bass — the obligatory
// home-keyboard demo button. Plays on whatever preset is selected.
static const int DEMO_MEL[32] = {            // semis from BASE+12; -1 = rest/hold
    16,16,17,19, 19,17,16,14, 12,12,14,16, 16,-1,14,14,
    16,16,17,19, 19,17,16,14, 12,12,14,16, 14,-1,12,12 };
static int demo_step = -1, demo_mel_semi = -1, demo_bass_semi = -1;

static void demo_off(void) {
    if (demo_mel_semi  >= 0) { release_semi(demo_mel_semi);  demo_mel_semi  = -1; }
    if (demo_bass_semi >= 0) { release_semi(demo_bass_semi); demo_bass_semi = -1; }
    demo_step = -1;
}

static void demo_tick(void) {
    int sub  = (beat_pos() >= 0.5f) ? 1 : 0;
    int stp  = (beat() * 2 + sub) % 32;
    if (stp == demo_step) return;
    demo_step = stp;
    int m = DEMO_MEL[stp];
    if (m >= 0) {
        if (demo_mel_semi >= 0) release_semi(demo_mel_semi);
        demo_mel_semi = m;
        press_semi(m, FNG_DEMO);
    }
    if (stp % 8 == 0) {                          // bass on the bar: C3 / G3
        if (demo_bass_semi >= 0) release_semi(demo_bass_semi);
        demo_bass_semi = ((stp / 8) & 1) ? 7 : 0;
        press_semi(demo_bass_semi, FNG_DEMO);
    }
}

// ---------------------------------------------------------------- hit tests
static int semi_at(int x, int y) {
    if (y < KEY_TOP || y >= SCREEN_H) return -1;
    int wk = (x - KB_X0) / WKW;
    if (wk < 0 || wk >= NWHITE) return -1;
    if (y < KEY_TOP + BLACK_H) {
        for (int k = wk - 1; k <= wk; k++) {
            if (k < 0 || k >= NWHITE) continue;
            int pos = k % 7;
            if (bsemi[pos] < 0) continue;
            int bx = KB_X0 + (k + 1) * WKW - BLACK_W / 2;
            if (x >= bx && x < bx + BLACK_W) return (k / 7) * 12 + bsemi[pos];
        }
    }
    return (wk / 7) * 12 + wsemi[wk % 7];
}

#define PBT_Y 18
#define PBT_W 30
#define PBT_X(i) (4 + (i) * 31)
#define CBT_Y 40
static int cbt_x[3], cbt_w[3];                   // VIBRATO / DEMO / SRC zones, set by draw()

void init(void) {
    instrument(SL_MALLET, INSTR_MALLET, 1, 0, 7, 1200);
    instrument(SL_CLICK, INSTR_NOISE, 1, 30, 0, 30);
    instrument_filter(SL_CLICK, FILTER_HIGH, 5000, 0);
    for (int s = 0; s < NSEMI; s++) ks[s].finger = NOFINGER;
    apply_preset();
    bpm(110);
}

void update(void) {
    const P *p = &PRE[preset];

    // presets: 1..9 then 0
    for (int i = 0; i < 10; i++)
        if (keyp(i == 9 ? '0' : '1' + i)) { preset = i; apply_preset(); }
    if (keyp('A')) { vibrato = !vibrato; apply_preset(); vibrato_live(); }
    if (keyp('K') && p->struck) { use_mallet = !use_mallet; }
    if (keyp(KEY_SPACE)) { demo = !demo; if (!demo) demo_off(); }

    // computer keyboard: one octave, gate = key held
    for (int s = 0; s <= 12; s++) {
        if (keyp(KBMAP[s]) && ks[s].finger == NOFINGER) press_semi(s, FNG_KB);
        if (keyr(KBMAP[s]) && ks[s].finger == FNG_KB)   release_semi(s);
    }

    // touch: live fingers claim keys / glissando (touchpiano pattern)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        int s  = semi_at(tx, ty);
        int cur = -1;
        for (int k = 0; k < NSEMI; k++)
            if (ks[k].finger == id) { cur = k; break; }
        if (s == cur) continue;
        if (cur >= 0) release_semi(cur);
        if (s >= 0 && ks[s].finger == NOFINGER) press_semi(s, id);
    }
    // lifted fingers release exactly their keys
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        for (int k = 0; k < NSEMI; k++)
            if (ks[k].finger == id) release_semi(k);
    }
    // panel buttons (edge-triggered taps; mouse arrives as a synthetic finger)
    for (int i = 0; i < 10; i++)
        if (tapp(PBT_X(i), PBT_Y, PBT_W, 16)) { preset = i; apply_preset(); }
    if (tapp(cbt_x[0], CBT_Y, cbt_w[0], 16)) { vibrato = !vibrato; apply_preset(); vibrato_live(); }
    if (tapp(cbt_x[1], CBT_Y, cbt_w[1], 16)) { demo = !demo; if (!demo) demo_off(); }
    if (tapp(cbt_x[2], CBT_Y, cbt_w[2], 16) && p->struck) use_mallet = !use_mallet;

    if (demo) demo_tick();

#ifdef DE_TRACE
    watch("preset", "%d", preset);
    watch("voices", "%d", voices_used());
    watch("mallet", "%d", use_mallet ? 1 : 0);
    watch("demo", "%d", demo ? 1 : 0);
#endif
}

void draw(void) {
    const P *p = &PRE[preset];
    cls(CLR_DARKER_GREY);
    // wood side panels + silver face
    rectfill(0, 0, 5, SCREEN_H, CLR_DARK_BROWN);
    rectfill(SCREEN_W - 5, 0, 5, SCREEN_H, CLR_DARK_BROWN);

    // nameplate
    print("DREAMTONE", 8, 4, CLR_WHITE);
    print("MT-70", 86, 4, CLR_ORANGE);
    font(FONT_SMALL);
    print("two sines per key - zero new engine", 130, 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // preset buttons
    font(FONT_TINY);
    for (int i = 0; i < 10; i++) {
        bool on = (i == preset);
        rectfill(PBT_X(i), PBT_Y, PBT_W, 16, on ? CLR_ORANGE : CLR_DARK_GREY);
        rect(PBT_X(i), PBT_Y, PBT_W, 16, on ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
        print(PRE[i].name, PBT_X(i) + 2, PBT_Y + 5, on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);

    // control row — VIBRATO / DEMO / SRC + the voice meter
    font(FONT_SMALL);
    const char *lbl[3];
    lbl[0] = vibrato ? "VIBRATO:ON" : "VIBRATO";
    lbl[1] = demo ? "DEMO:ON" : "DEMO";
    lbl[2] = !p->struck ? "SRC: 2-OSC" : (use_mallet ? "SRC:MALLET" : "SRC: 2-OSC");
    bool lit[3] = { vibrato, demo, use_mallet && p->struck };
    int x = 8;
    for (int b = 0; b < 3; b++) {
        int w = text_width(lbl[b]) + 10;
        cbt_x[b] = x; cbt_w[b] = w;
        rectfill(x, CBT_Y, w, 16, lit[b] ? CLR_LIME_GREEN : CLR_DARK_GREY);
        rect(x, CBT_Y, w, 16, CLR_MEDIUM_GREY);
        print(lbl[b], x + 5, CBT_Y + 5, lit[b] ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
        x += w + 6;
    }
    if (!p->struck) print("(mallet A/B: struck presets)", x + 4, CBT_Y + 5, CLR_DARK_GREY);
    // voice meter — the probe readout: stacked sines cost real voices
    int vu = voices_used();
    print_right(str("voices %d/16", vu), SCREEN_W - 10, CBT_Y + 5, vu >= 12 ? CLR_RED : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the hardware quote for the selected preset
    font(FONT_TINY);
    print(str("\"%s\"", p->quote), 8, CBT_Y + 22, CLR_LIGHT_GREY);
    int nl = (use_mallet && p->struck) ? 1 : 1 + (p->o2_vol ? 1 : 0) + (p->o3_vol ? 1 : 0);
    print_right(str("%d voice%s/key", nl, nl == 1 ? "" : "s"), SCREEN_W - 10, CBT_Y + 22, CLR_DARK_GREY);
    print("Z..M keys  1..0 presets  A vib  K src  SPACE demo", 8, CBT_Y + 32, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // keyboard
    for (int k = 0; k < NWHITE; k++) {
        int s = (k / 7) * 12 + wsemi[k % 7];
        if (s >= NSEMI) continue;
        int kx = KB_X0 + k * WKW;
        bool held = ks[s].finger != NOFINGER;
        rectfill(kx + 1, KEY_TOP, WKW - 2, SCREEN_H - KEY_TOP, held ? CLR_ORANGE : CLR_WHITE);
        rect(kx, KEY_TOP, WKW, SCREEN_H - KEY_TOP, CLR_DARK_GREY);
    }
    for (int k = 0; k < NWHITE; k++) {
        int pos = k % 7;
        if (bsemi[pos] < 0) continue;
        int s = (k / 7) * 12 + bsemi[pos];
        if (s >= NSEMI) continue;
        int bx = KB_X0 + (k + 1) * WKW - BLACK_W / 2;
        bool held = ks[s].finger != NOFINGER;
        rectfill(bx, KEY_TOP, BLACK_W, BLACK_H, held ? CLR_ORANGE : CLR_BROWNISH_BLACK);
        rect(bx, KEY_TOP, BLACK_W, BLACK_H, CLR_BLACK);
    }
}
