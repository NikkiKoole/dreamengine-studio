// solo.h — the radio jam layer: a scale-locked solo strip, cart-land.
//
// The companion to radio.h (game-music.md → "the jam layer"). Every station
// already computes its key, scale, and current chord; this header turns that
// into a playable instrument you CAN'T play wrong: a horizontal strip of only
// the in-key notes, the current chord's tones lit, monophonic with portamento
// so drags slide like a stylophone / Omnichord. The station owns the VOICE (an
// I_SOLO instrument tuned to sit on top of the mix); solo.h owns the
// INTERACTION (capture, scale-lock, glide, optional beat-quantize, the strip).
//
// Built on ui.h's per-finger capture (the mouse is one synthetic finger, so
// it's mouse-and-touch for free) — so it MUST be called between ui_begin() and
// ui_end(), exactly like the rad_knob_* controls. radio.h already pulls ui.h
// in; just #include "solo.h" after it.
//
// Usage — the station fills a SoloCtx from data it already has, then calls the
// strip once in draw() (between ui_begin/ui_end):
//
//   int chord[4]; int nch = current_chord_pcs(chord);   // the cart's own chord
//   SoloCtx jc = { .root = sng.keyPc, .scale = PENT, .nscale = 5,
//                  .chordPcs = chord, .nchordPcs = nch,
//                  .instr = I_SOLO, .loMidi = 72, .hiMidi = 91,
//                  .quantize = false };
//   solo_strip(&jc, 28, 168, 264, 22, CLR_PINK);   // x,y,w,h, accent
//
// Closed, it draws a small "jam" tab (tap it, or press J) — so it works on a
// phone with no keyboard. Open, the strip replaces that corner. solo_midi()
// returns the note sounding right now (-1 = silent) for display or a trace.
//
// Everything static (gestures.h / improv.h / ui.h pattern).

#ifndef SOLO_H
#define SOLO_H

#include "studio.h"
#include "ui.h"
#include <math.h>

#ifndef SOLO_KEY
#define SOLO_KEY 'J'          // toggles the jam strip (override before include)
#endif
#ifndef SOLO_GLIDE_MS
#define SOLO_GLIDE_MS 55      // portamento between cells — the stylophone slide
#endif
#ifndef SOLO_VOL
#define SOLO_VOL 5            // the solo sits ABOVE the backing track
#endif
#ifndef SOLO_OCT_MAX
#define SOLO_OCT_MAX 2        // octave shift range: the - / + buttons span ±this
#endif

// the vertical axis: horizontal picks the note, vertical SHAPES it — and what
// it shapes is the station's call (game-music.md → "the jam layer"). The strip
// applies VOL/BRIGHT to the held voice itself; SOLO_Y_OFF + solo_y() lets a
// cart map the 0..1 height to anything bespoke (echo send, a second LFO, …).
enum { SOLO_Y_OFF, SOLO_Y_VOL, SOLO_Y_BRIGHT, SOLO_Y_SEND };
// SOLO_Y_SEND rides the instrument's echo send (instrument_echo) — the dub move,
// lean up to drench the note in tape echo; only use it where an echo bus runs.

typedef struct {
    int  root;                // key root pitch class (0..11)
    const int *scale;         // in-key semitone offsets (0..11)…
    int  nscale;              // …and how many (5 pentatonic, 7 diatonic)
    const int *chordPcs;      // current chord's pitch classes to LIGHT (or NULL)
    int  nchordPcs;
    int  instr;               // the station's I_SOLO slot
    int  loMidi, hiMidi;      // the strip's playable register, inclusive
    bool quantize;            // snap each note-on to the next 16th
    int  ymode;               // SOLO_Y_* — what vertical drag does (top = more)
    float yMin, yMax;         // that mode's range: VOL = vol levels, BRIGHT = cutoff Hz
} SoloCtx;

static bool solo_open_f  = false;
static int  solo_handle  = -1;     // the held mono voice (-1 = silent)
static int  solo_curMidi = -1;     // what's sounding (display / trace)
static float solo_curY   = -1;     // last vertical position 0..1 (-1 = silent)
static bool solo_pending = false;  // a press waiting for its 16th (quantize)
static int  solo_pendMidi = -1;
static long solo_pendStep = 0;
static int  solo_oct      = 0;     // octave shift of the playable window (- / + buttons)

static bool  solo_open(void) { return solo_open_f; }
static int   solo_midi(void) { return solo_curMidi; }   // -1 = silent
static float solo_y(void)    { return solo_curY; }      // vertical 0..1, -1 = silent

static void solo_kill(void) {
    if (solo_handle >= 0) note_off(solo_handle);
    solo_handle = -1; solo_curMidi = -1; solo_curY = -1; solo_pending = false;
}

// build the ascending list of in-scale midis across [lo,hi]; returns count
static int solo_notes(const SoloCtx *cx, int *out, int max) {
    int n = 0, sh = solo_oct * 12;                          // the octave shift
    for (int m = cx->loMidi + sh; m <= cx->hiMidi + sh && n < max; m++) {
        if (m < 24 || m > 100) continue;                    // stay in a sane register
        int pc = ((m - cx->root) % 12 + 12) % 12;
        for (int s = 0; s < cx->nscale; s++)
            if (cx->scale[s] == pc) { out[n++] = m; break; }
    }
    return n;
}

static bool solo_is_chord_pc(const SoloCtx *cx, int midi) {
    if (!cx->chordPcs) return false;
    int pc = midi % 12;
    for (int i = 0; i < cx->nchordPcs; i++)
        if (cx->chordPcs[i] % 12 == pc) return true;
    return false;
}

// the whole layer: toggle tab when closed, the playable strip when open.
// call once per frame in draw(), between ui_begin() and ui_end().
static void solo_strip(const SoloCtx *cx, int x, int y, int w, int h, int accent) {
    if (keyp(SOLO_KEY)) { solo_open_f = !solo_open_f; if (!solo_open_f) solo_kill(); }

    // ── the toggle tab (right edge, present in BOTH states) ────────────────
    // tap it to open or close; J does the same. Lit = open.
    int tw = 34, tx = x + w - tw;
    static int tab_id;
    ui_reg(&tab_id, tx, y, tw, h, 0);
    UiCap *tc = ui_cap_for(&tab_id);
    if (tc && tc->released &&
        tc->rx >= tx - UI_HIT_PAD && tc->rx < tx + tw + UI_HIT_PAD &&
        tc->ry >= y - UI_HIT_PAD && tc->ry < y + h + UI_HIT_PAD) {
        solo_open_f = !solo_open_f;
        if (!solo_open_f) solo_kill();
    }
    bool tabHot = tc != 0 || ui_hover(tx, y, tw, h);
    bool tabFill = solo_open_f || tabHot;

    // ── closed: just the tab ───────────────────────────────────────────────
    if (!solo_open_f) {
        rectfill(tx, y, tw, h, tabFill ? accent : CLR_DARKER_GREY);
        rect(tx, y, tw, h, accent);
        print("jam", tx + (tw - text_width("jam")) / 2, y + (h - 6) / 2, tabFill ? CLR_BLACK : accent);
        return;
    }

    // ── open: keybed on the left; octave - / + and the tab on the right ────
    if (keyp(',') && solo_oct > -SOLO_OCT_MAX) solo_oct--;  // octave keys (buttons below)
    if (keyp('.') && solo_oct <  SOLO_OCT_MAX) solo_oct++;
    int octW = 16, octUpX = tx - octW, octDnX = octUpX - octW;
    static int octdn_id, octup_id;
    ui_reg(&octdn_id, octDnX, y, octW, h, 0);
    ui_reg(&octup_id, octUpX, y, octW, h, 0);
    UiCap *odc = ui_cap_for(&octdn_id), *ouc = ui_cap_for(&octup_id);
    if (odc && odc->released &&
        ui_in(odc->rx, odc->ry, octDnX - UI_HIT_PAD, y - UI_HIT_PAD, octW + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD)
        && solo_oct > -SOLO_OCT_MAX) solo_oct--;
    if (ouc && ouc->released &&
        ui_in(ouc->rx, ouc->ry, octUpX - UI_HIT_PAD, y - UI_HIT_PAD, octW + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD)
        && solo_oct <  SOLO_OCT_MAX) solo_oct++;

    int sw = octDnX - x - 2;                                // keybed up to the octave buttons
    int notes[48];
    int nn = solo_notes(cx, notes, 48);
    if (nn == 0 || sw < nn) { rectfill(x, y, sw, h, CLR_BLACK); }
    int cw = nn > 0 ? sw / nn : sw;
    if (cw < 1) cw = 1;

    static int strip_id;
    ui_reg(&strip_id, x, y, sw, h, 0);
    UiCap *c = ui_cap_for(&strip_id);

    int cell = -1, midi = -1;
    if (c && nn > 0) {
        int px = (c->released ? c->rx : c->cx) - x;
        cell = px / cw;
        cell = cell < 0 ? 0 : cell >= nn ? nn - 1 : cell;
        midi = notes[cell];
    }

    float vy = -1;                                          // vertical 0..1 (top = more)
    if (c) {
        int cyp = c->released ? c->ry : c->cy;
        vy = 1.0f - (cyp - y) / (float)h;
        vy = vy < 0 ? 0 : vy > 1 ? 1 : vy;
    }

    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;   // 16th-step clock

    // note lifecycle: grab → on (quantized or now); drag → glide; release → off
    if (ui_grabbed(&strip_id) && midi >= 0) {
        if (cx->quantize) { solo_pending = true; solo_pendMidi = midi; solo_pendStep = (long)pos; }
        else {
            solo_handle = note_on(midi, cx->instr, SOLO_VOL);
            note_glide(solo_handle, SOLO_GLIDE_MS);
            solo_curMidi = midi;
        }
    }
    if (solo_pending && (long)pos > solo_pendStep) {        // the quantized strike lands
        solo_handle = note_on(solo_pendMidi, cx->instr, SOLO_VOL);
        note_glide(solo_handle, SOLO_GLIDE_MS);
        solo_curMidi = solo_pendMidi;
        solo_pending = false;
    }
    if (c && !c->released && solo_handle >= 0 && midi >= 0 && midi != solo_curMidi) {
        note_pitch(solo_handle, midi);                      // slide to the new degree
        solo_curMidi = midi;
    }
    if (ui_released(&strip_id)) solo_kill();

    // the vertical axis — the station decides what it means (top = more)
    if (solo_handle >= 0 && vy >= 0) {
        solo_curY = vy;
        float v = cx->yMin + vy * (cx->yMax - cx->yMin);
        if      (cx->ymode == SOLO_Y_VOL)    note_vol(solo_handle, (int)(v + 0.5f));
        else if (cx->ymode == SOLO_Y_BRIGHT) note_cutoff(solo_handle, (int)v);
        else if (cx->ymode == SOLO_Y_SEND)   instrument_echo(cx->instr, v);
    }

    // ── draw the keybed ────────────────────────────────────────────────────
    rectfill(x, y, sw, h, CLR_BLACK);
    for (int i = 0; i < nn; i++) {
        int col = solo_is_chord_pc(cx, notes[i]) ? accent
                : (notes[i] % 12 == cx->root)     ? CLR_MEDIUM_GREY   // the tonic
                                                  : CLR_DARK_GREY;
        rectfill(x + i * cw + 1, y + 1, cw - 2, h - 2, col);
    }
    if (cell >= 0 && solo_handle >= 0) {                    // the played note + its Y level
        int cx0 = x + cell * cw;
        if (cx->ymode == SOLO_Y_OFF || vy < 0)
            rectfill(cx0 + 1, y + 1, cw - 2, h - 2, CLR_WHITE);
        else {
            int barH = (int)(vy * (h - 2) + 0.5f);          // a level meter = the Y axis
            rectfill(cx0 + 1, y + 1 + (h - 2 - barH), cw - 2, barH, CLR_WHITE);
        }
        rect(cx0, y, cw, h, CLR_WHITE);
    }
    rect(x, y, sw, h, accent);

    // octave - / + buttons (ui.h inflates the hit target so fat fingers land)
    bool dnHot = odc != 0 || ui_hover(octDnX, y, octW, h);
    bool upHot = ouc != 0 || ui_hover(octUpX, y, octW, h);
    rectfill(octDnX, y, octW, h, dnHot ? accent : CLR_DARKER_GREY);
    rect(octDnX, y, octW, h, accent);
    print("-", octDnX + (octW - text_width("-")) / 2, y + (h - 6) / 2, dnHot ? CLR_BLACK : accent);
    rectfill(octUpX, y, octW, h, upHot ? accent : CLR_DARKER_GREY);
    rect(octUpX, y, octW, h, accent);
    print("+", octUpX + (octW - text_width("+")) / 2, y + (h - 6) / 2, upHot ? CLR_BLACK : accent);

    // the tab, lit (tap to close)
    rectfill(tx, y, tw, h, accent);
    rect(tx, y, tw, h, accent);
    print("jam", tx + (tw - text_width("jam")) / 2, y + (h - 6) / 2, CLR_BLACK);
}

#endif // SOLO_H
