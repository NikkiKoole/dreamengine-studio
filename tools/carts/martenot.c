/* de:meta
{
  "slug": "martenot",
  "title": "ondes martenot",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "additive-synth"
  ],
  "lineage": "Models the 1928 Ondes Martenot ribbon synth; novel in summing harmonic stops live into a single-cycle wavetable (wave_set) and combining ribbon glissando, detuned-twin beating, pitch drift, and four diffuseur reverb colours in one instrument.",
  "description": "The 1928 ribbon synth (Radiohead, film scores, Messiaen) - a two-hand upgrade to the theremin, played MOSTLY on the RIBBON: drag it for continuous pitch (X) while the HEIGHT is the swell (Y) - a real theremin volume hand, the glowing orb rising as it gets louder. ONE eerie voice rings the whole time, plus a DETUNED TWIN that beats against it (the haunting shimmer), a slow analogue pitch-drift, a little drive and an ensemble chorus - deliberately NOT a clean synth. la TOUCHE D'INTENSITE (the left lever) is the dynamics hand: it drives note_vol AND note_cutoff (harder = louder + brighter), rides the swell for the keyboard, and overrides the ribbon's Y when you grab it with a second finger. The CLAVIER (piano) plays tempered notes; drag UP on a held key for VIBRATO (the key-wiggle). The combinable TIMBRE stops O/C/G/N are summed live into one drawn cycle (wave_set) + an S breath layer. The four DIFFUSEURS are the loudspeakers as reverb colour (note_reverb live + the reverb bus): Principal (dry), Resonance (spring), Metallique (a bright gong shimmer, the default) and Palme (a long string-halo). Plays three ways - TOUCH (two hands: touche + ribbon), MOUSE (one pointer), or the computer KEYBOARD: GarageBand note layout (A S D F... whites, W E T Y U... blacks, labelled on the keys), Z/X octave, UP/DOWN ride the touche, and the NUMBER ROW works the buttons (1-5 stops, 6-9 diffuseurs)."
}
de:meta */
#include "studio.h"

// ONDES MARTENOT — the 1928 ribbon synth (Radiohead, film scores, Messiaen's
// Turangalîla). You play it MOSTLY on the RIBBON: a real theremin surface where
// X = pitch (continuous glissando) and Y = swell (the volume hand). The piano is
// the secondary, tempered manual.
//
// ONE eerie voice rings the whole time (note_on at start, never note_off'd), plus a
// DETUNED twin that beats against it (the haunting shimmer) and an optional breath.
// Everything else shapes that live voice:
//   • le RUBAN (big strip)  → X note_pitch glissando · Y = swell (theremin volume)
//   • la TOUCHE (left lever)→ the dynamics hand: rides the swell for the clavier,
//     and overrides the ribbon's Y when you grab it (a 2nd finger / drag)
//   • le CLAVIER (piano)    → tempered note_pitch · drag UP on a held key = VIBRATO
//   • TIMBRE stops O/C/G/N  → wave_set: the lit stops summed into one drawn cycle
//   • DIFFUSEURS            → the four loudspeakers as reverb/chorus colour: the
//     spring (Résonance), the gong-cone (Métallique), the lyre-of-strings (Palme)
//
// What keeps it from sounding like a clean synth: a detuned twin (beating), a slow
// pitch DRIFT (analogue instability), a little drive, vibrato that deepens with
// effort, and the diffuseur reverb. Two hands on touch; one mouse on desktop; or the
// keyboard — GarageBand note layout, Z/X octave, number row works the buttons.

#define VSLOT    5          // main voice (drawn wave, INSTR_USER0)
#define SSLOT    6          // souffle: a breath-noise bed
#define DSLOT    7          // detuned twin (beats against VSLOT — the eerie shimmer)
#define LO_MIDI  48         // C3 — bottom of ribbon & keyboard
#define SPAN     24         // two octaves up to C5
#define GLIDE_MS 28         // tiny portamento so pitch never zippers

// layout — left drawer (tiroir), then ribbon (big) over the keybed
#define LVX 10
#define LVY 28
#define LVW 22
#define LVH 150            // touche lever travel
#define STX 34             // button column (stops + diffuseurs)
#define STW 60
#define STH 15
#define STY 28
#define DFY 126            // diffuseur column
#define KX  102            // play area left
#define KW  274            // play area width (ribbon + keybed share it, aligned)
#define RBY 22
#define RBH 96             // RIBBON — the primary surface (theremin: X pitch, Y swell)
#define KBY 124
#define KBH 86             // keybed — secondary tempered manual (Y = vibrato)

// ---- timbre stops -------------------------------------------------------------
enum { ST_ONDE, ST_CREUX, ST_GAMBE, ST_NASAL, ST_SOUFFLE, NSTOP };
const char *STOP_SHORT[NSTOP] = { "onde", "creux", "gambe", "nasal", "breath" };
bool stop_on[NSTOP] = { true, true, false, false, false };   // onde+creux = a little body

const char *DIFF_LBL[4]   = { "Principal", "Resonance", "Metallique", "Palme" };
const char *DIFF_SHORT[4] = { "dry", "spring", "metal", "palme" };
int diffuseur = 2;          // start on Métallique — the eerie shimmer, out of the box

// computer keyboard — the trusted GarageBand musical-typing layout (same as moog/mt70):
// home row A S D F G H J K... = whites, QWERTY row W E T Y U... = blacks. Index = semitone.
// Notes are letters, so the BUTTONS get the number row (1-5 stops, 6-9 diffuseurs).
#define NKBKEY 18
const char KBMAP[NKBKEY] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P',';','\'' };
int kb_oct = 0;             // Z/X octave shift, GarageBand-style (-1..+2)
int kb_semi = -1;           // semitone of the held computer key, or -1

float fracf(float x) { return x - (int)x; }

// Rebuild the single-cycle wave from whichever stops are lit, normalize, push it
// into INSTR_USER0 (both VSLOT and DSLOT read it). Called only when timbre changes.
void rebuild_wave(void) {
    static float buf[128];
    const int N = 128;
    float peak = 0.0001f;
    for (int i = 0; i < N; i++) {
        float t = (float)i / N, s = 0;
        if (stop_on[ST_ONDE])  s += sin_deg(t * 360.0f);
        if (stop_on[ST_CREUX]) s += clamp(sin_deg(t * 360.0f) * 1.8f, -0.7f, 0.7f);   // flat-topped = hollow
        if (stop_on[ST_GAMBE]) s += 2.0f * (t - 0.5f);                                // sawtooth
        if (stop_on[ST_NASAL]) s += (fracf(t) < 0.28f) ? 0.9f : -0.9f;                // narrow pulse
        buf[i] = s;
        float a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    if (!(stop_on[ST_ONDE] || stop_on[ST_CREUX] || stop_on[ST_GAMBE] || stop_on[ST_NASAL]))
        for (int i = 0; i < N; i++) buf[i] = sin_deg((float)i / N * 360.0f), peak = 1;  // never silent-by-stops
    for (int i = 0; i < N; i++) buf[i] /= peak;
    wave_set(0, buf, N);                                   // 0 → INSTR_USER0
}

int   mainV = -1, souffleV = -1, twinV = -1;   // held voices — declared before apply_diffuseur

// The diffuseurs are the loudspeakers, each a different SPACE. reverb() sets the bus
// (global, live); the per-voice SEND must be note_reverb() — a held voice snapshots
// instrument_reverb() at note-on, so changing the slot send later never reaches it.
void apply_diffuseur(void) {
    float size[4] = { 0.15f, 0.45f, 0.50f, 0.90f };
    float damp[4] = { 0.50f, 0.55f, 0.12f, 0.35f };   // metallique = bright (low-damp) tail
    float send[4] = { 0.00f, 0.50f, 0.45f, 0.80f };
    reverb(size[diffuseur], damp[diffuseur]);
    if (mainV    >= 0) note_reverb(mainV,    send[diffuseur]);
    if (twinV    >= 0) note_reverb(twinV,    send[diffuseur]);
    if (souffleV >= 0) note_reverb(souffleV, send[diffuseur] * 0.6f);
}

float lever = 0.55f;        // touche set-level / ceiling, 0..1 (a playable rest level)
float intens = 0.0f;        // current (slewed) intensity reaching the voice
float curMidi = 60.0f;      // pitch the voice is singing
int   fromRibbon = 1;       // was the last pitch set on the ribbon? (ribbon is primary)
int   midi_note = -1, midi_vel = 0;   // MIDI keyboard: last-note pitch + velocity (mono)

const int WHITE_SEMI[7] = { 0, 2, 4, 5, 7, 9, 11 };
const int HAS_BLACK[7]  = { 1, 1, 0, 1, 1, 1, 0 };   // black key sits after C D _ F G A _

float white_w(void) { return KW / 14.0f; }           // 14 white keys across 2 octaves
float midi_x(float m) { return KX + (m - LO_MIDI) / SPAN * KW; }

// hit-test the piano: blacks (upper strip) first, then whites. -1 = miss.
int key_at(int x, int y) {
    float ww = white_w();
    if (y < KBY + (int)(KBH * 0.58f)) {
        for (int w = 0; w < 13; w++) {
            if (!HAS_BLACK[w % 7]) continue;
            float bx = KX + (w + 1) * ww - ww * 0.3f, bw = ww * 0.6f;
            if (x >= bx && x < bx + bw)
                return LO_MIDI + (w / 7) * 12 + WHITE_SEMI[w % 7] + 1;
        }
    }
    int w = (int)((x - KX) / ww);
    if (w < 0 || w > 13) return -1;
    return LO_MIDI + (w / 7) * 12 + WHITE_SEMI[w % 7];
}

void init(void) {
    rebuild_wave();
    // the two pitched voices: a holding pad we drive live with note_vol/note_cutoff.
    int slots[2] = { VSLOT, DSLOT };
    for (int i = 0; i < 2; i++) {
        instrument(slots[i], INSTR_USER0, 12, 0, 7, 320);
        instrument_filter(slots[i], FILTER_LOW, 1200, 4);
        instrument_drive(slots[i], 0.14f);                          // a little grit, not a clean synth
        instrument_lfo(slots[i], 1, LFO_PITCH, 0.23f + i * 0.05f, 0.05f);  // slow analogue DRIFT
        instrument_chorus(slots[i], 0.7f, 0.35f, 0.40f);            // gentle ensemble (BEFORE note_on, bus snapshots)
    }
    instrument_tune(DSLOT, 0.12f);                                   // the twin sits a hair sharp → beating
    mainV = note_on(60, VSLOT, 0);   note_glide(mainV, GLIDE_MS);
    twinV = note_on(60, DSLOT, 0);   note_glide(twinV, GLIDE_MS);

    instrument(SSLOT, INSTR_NOISE, 30, 0, 7, 200);
    instrument_filter(SSLOT, FILTER_BAND, 2600, 5);
    souffleV = note_on(60, SSLOT, 0);

    apply_diffuseur();
}

// regions of the play half
enum { R_NONE, R_TOUCHE, R_RIBBON, R_KEYBED };
int region(int x, int y) {
    if (point_in_box(x, y, LVX, LVY, LVW, LVH))   return R_TOUCHE;
    if (point_in_box(x, y, KX, RBY, KW, RBH))     return R_RIBBON;
    if (point_in_box(x, y, KX, KBY, KW, KBH))     return R_KEYBED;
    return R_NONE;
}
float lever_from_y(int y)  { return clamp(1.0f - (float)(y - LVY) / LVH, 0, 1); }
float ribbon_swell(int y)  { return clamp(1.0f - (float)(y - RBY) / RBH, 0, 1); }  // top = loud

void toggle_stop(int s) { stop_on[s] = !stop_on[s]; if (s != ST_SOUFFLE) rebuild_wave(); }

void update(void) {
    int touchePtr = 0, pitchPtr = 0;
    int ribbonOn = 0;  float ribY = 0;
    int keyOn = 0;     float keyY = 0;

    // ---- buttons on the number row (letters are notes; digits are free) ----
    for (int s = 0; s < NSTOP; s++) if (keyp('1' + s)) toggle_stop(s);
    for (int d = 0; d < 4; d++)     if (keyp('6' + d)) { diffuseur = d; apply_diffuseur(); }

    // ---- MIDI keyboard (engine midi_get): mono, last-note priority. Notes set the
    //      ribbon pitch, velocity sets the swell, and the bend wheel bends the ribbon. ----
    int mn, mv, mt;
    while ((mt = midi_get(&mn, &mv)) != 0) {
        if (mt > 0)               { midi_note = mn; midi_vel = mv; }
        else if (mn == midi_note)   midi_note = -1;
    }

    // ---- computer keyboard: GarageBand musical typing = the CLAVIER (tempered) ----
    if (keyp('Z') && kb_oct > -1) kb_oct--;
    if (keyp('X') && kb_oct <  2) kb_oct++;
    if (key(KEY_UP))   lever = clamp(lever + 0.025f, 0, 1);   // ride the touche from the desk
    if (key(KEY_DOWN)) lever = clamp(lever - 0.025f, 0, 1);
    for (int s = 0; s < NKBKEY; s++) if (keyp(KBMAP[s])) kb_semi = s;     // last-note priority
    if (kb_semi >= 0 && !key(KBMAP[kb_semi])) {              // active key lifted → highest still-held
        kb_semi = -1;
        for (int s = 0; s < NKBKEY; s++) if (key(KBMAP[s])) kb_semi = s;
    }
    if (kb_semi >= 0) { curMidi = LO_MIDI + kb_oct * 12 + kb_semi; fromRibbon = 0; pitchPtr = 1; }

    // ---- pointer input: touch AND mouse, ONE path ----
    // (the desktop mouse is merged into the touch pool as a synthetic touch, so the
    //  touch loop + tapp() already cover it — handling the mouse twice double-fires.)
    for (int i = 0; i < touch_count(); i++) {
        int x = touch_x(i), y = touch_y(i), r = region(x, y);
        if (r == R_TOUCHE) { lever = lever_from_y(y); touchePtr = 1; }
        else if (r == R_RIBBON) {
            curMidi = LO_MIDI + clamp((float)(x - KX) / KW, 0, 1) * SPAN;   // continuous
            fromRibbon = 1; pitchPtr = 1; ribbonOn = 1; ribY = y;
        } else if (r == R_KEYBED) {
            int m = key_at(x, y);
            if (m >= 0) { curMidi = m; fromRibbon = 0; pitchPtr = 1; keyOn = 1; keyY = y; }
        }
    }
    for (int s = 0; s < NSTOP; s++)
        if (tapp(STX, STY + s * (STH + 3), STW, STH)) toggle_stop(s);
    for (int d = 0; d < 4; d++)
        if (tapp(STX, DFY + d * (STH + 2), STW, STH)) { diffuseur = d; apply_diffuseur(); }

    // MIDI drives the ribbon pitch when no on-screen pointer is, and the pitch-bend
    // wheel bends it (the Ondes ring/glissando — the one cart that wants midi_bend()).
    int midiOn = 0;
    if (midi_note >= 0 && !pitchPtr) {
        curMidi = midi_note + midi_bend() / 8192.0f * 2.0f;   // ±2 semitones of bend
        fromRibbon = 0; pitchPtr = 1; midiOn = 1;
    }

    // ---- intensity: touche overrides · else ribbon-Y is the theremin volume ·
    //      else MIDI velocity · else a tempered note swells to the touche ceiling · else release.
    float target;
    if (touchePtr)      target = lever;
    else if (ribbonOn)  target = ribbon_swell((int)ribY);
    else if (midiOn)    target = midi_vel / 127.0f;
    else if (pitchPtr)  target = lever;
    else                target = 0;
    float rate = (touchePtr || ribbonOn) ? 0.5f : (target > intens ? 0.22f : 0.14f);
    intens += (target - intens) * rate;
    if (intens < 0.002f) intens = 0;

    // vibrato: deepens with effort, and a held key wiggled UP adds more (the clavier wiggle)
    float vib = 0.05f + 0.12f * intens;
    if (keyOn) vib += clamp(1.0f - (keyY - KBY) / (float)KBH, 0, 1) * 0.5f;

    // drive the live voices (main + detuned twin)
    int v = (int)(intens * 7 + 0.5f);
    int cut = 320 + (int)(intens * intens * 4600);
    note_pitch(mainV, curMidi);   note_vol(mainV, v);   note_cutoff(mainV, cut);
    note_pitch(twinV, curMidi);   note_vol(twinV, v);   note_cutoff(twinV, cut);
    note_lfo(mainV, 0, LFO_PITCH, 6.2f, vib);
    note_lfo(twinV, 0, LFO_PITCH, 5.6f, vib * 0.9f);              // different rate = richer beating
    note_vol(souffleV, stop_on[ST_SOUFFLE] ? intens * 5.0f : 0.0f);
}

// ---- drawing ------------------------------------------------------------------
void panel(int x, int y, int w, int h, bool lit, int onc, int offc, int num, const char *lbl) {
    rectfill(x, y, w, h, lit ? onc : offc);
    rect(x, y, w, h, CLR_BROWNISH_BLACK);
    print(str("%d %s", num, lbl), x + 3, y + (h - 5) / 2, lit ? CLR_BLACK : CLR_MEDIUM_GREY);
}

const char *NOTE_NM[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, 16, CLR_DARK_BROWN);
    rectfill(2, 18, 94, SCREEN_H - 20, CLR_BROWN);            // the tiroir (drawer)
    rect(2, 18, 94, SCREEN_H - 20, CLR_BROWNISH_BLACK);
    print("ONDES  MARTENOT", 8, 5, CLR_LIGHT_PEACH);
    print("A..' notes  Z/X oct", 150, 5, CLR_MEDIUM_GREY);
    print_right("1928", SCREEN_W - 6, 5, CLR_PEACH);

    // touche d'intensité — the lever
    rectfill(LVX, LVY, LVW, LVH, CLR_DARKER_GREY);
    rect(LVX, LVY, LVW, LVH, CLR_BROWNISH_BLACK);
    int ky = LVY + (int)((1.0f - lever) * LVH);
    rectfill(LVX + 1, ky, LVW - 2, LVY + LVH - ky, intens > 0.01f ? CLR_ORANGE : CLR_DARK_ORANGE);
    rectfill(LVX - 2, ky - 3, LVW + 4, 6, CLR_LIGHT_PEACH);   // the knob
    rect(LVX - 2, ky - 3, LVW + 4, 6, CLR_BROWNISH_BLACK);
    print("touche", LVX - 2, LVY + LVH + 3, CLR_LIGHT_PEACH);

    // timbre stops (1-5) + diffuseurs (6-9)
    for (int s = 0; s < NSTOP; s++)
        panel(STX, STY + s * (STH + 3), STW, STH, stop_on[s], CLR_PINK, CLR_DARKER_PURPLE, s + 1, STOP_SHORT[s]);
    print("diffuseur", STX, DFY - 8, CLR_LIGHT_PEACH);
    for (int d = 0; d < 4; d++)
        panel(STX, DFY + d * (STH + 2), STW, STH, diffuseur == d, CLR_LIME_GREEN, CLR_BLUE_GREEN, d + 6, DIFF_SHORT[d]);

    // RIBBON — the primary theremin surface
    rectfill(KX, RBY, KW, RBH, CLR_DARKER_BLUE);
    rect(KX, RBY, KW, RBH, CLR_DARK_BLUE);
    for (int m = LO_MIDI; m <= LO_MIDI + SPAN; m++) {        // ticks: octaves run full height
        int tx = (int)midi_x(m);
        bool oct = (m % 12) == 0;
        line(tx, RBY, tx, RBY + (oct ? RBH : 6), oct ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
    }
    print("ruban   X = pitch   Y = swell", KX + 4, RBY + 3, CLR_INDIGO);
    int rx  = (int)clamp(midi_x(curMidi), KX, KX + KW);
    int oy  = RBY + (int)((1.0f - intens) * RBH);            // orb rises as it swells
    line(rx, RBY, rx, RBY + RBH, CLR_PINK);                  // pitch line
    if (intens > 0.01f) {
        line(KX, oy, KX + KW, oy, CLR_DARK_BLUE);            // swell line
        circ(rx, oy, 6 + (int)(intens * 10), CLR_BLUE);
    }
    circfill(rx, oy, 4, intens > 0.01f ? CLR_LIGHT_PEACH : CLR_INDIGO);

    // readout — between ribbon and keybed
    int mi = (int)(curMidi + 0.5f);
    print(str("%s%d  %s  oct%+d", NOTE_NM[mi % 12], mi / 12 - 1, fromRibbon ? "ruban" : "clavier", kb_oct),
          KX + 4, KBY - 9, intens > 0.01f ? CLR_LIME_GREEN : CLR_LIGHT_PEACH);
    print_right("drag a key UP = vibrato", SCREEN_W - 6, KBY - 9, CLR_DARK_BROWN);

    // KEYBED — tempered manual, drag UP a held key = vibrato; keys labelled with their letters
    float ww = white_w();
    int base = LO_MIDI + kb_oct * 12;
    for (int w = 0; w < 14; w++) {
        int m = LO_MIDI + (w / 7) * 12 + WHITE_SEMI[w % 7], x = KX + (int)(w * ww);
        bool lit = !fromRibbon && intens > 0.01f && (int)(curMidi + 0.5f) == m;
        rectfill(x, KBY, (int)ww - 1, KBH, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
        rect(x, KBY, (int)ww - 1, KBH, CLR_DARK_BROWN);
        int s = m - base;
        if (s >= 0 && s < NKBKEY) print(str("%c", KBMAP[s]), x + (int)ww / 2 - 2, KBY + KBH - 9, CLR_DARK_GREY);
    }
    int bh = (int)(KBH * 0.58f);
    for (int w = 0; w < 13; w++) {
        if (!HAS_BLACK[w % 7]) continue;
        int m = LO_MIDI + (w / 7) * 12 + WHITE_SEMI[w % 7] + 1;
        int bx = KX + (int)((w + 1) * ww - ww * 0.3f), bw = (int)(ww * 0.6f);
        bool lit = !fromRibbon && intens > 0.01f && (int)(curMidi + 0.5f) == m;
        rectfill(bx, KBY, bw, bh, lit ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK);
        rect(bx, KBY, bw, bh, CLR_BLACK);
        int s = m - base;
        if (s >= 0 && s < NKBKEY) print(str("%c", KBMAP[s]), bx + bw / 2 - 1, KBY + bh - 8, CLR_LIGHT_GREY);
    }
}
