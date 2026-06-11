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
// ── TWO LOCKS: scale-lock (default) vs chord-lock ──────────────────────────
// The strip can fill its cells two ways — pick by the station's harmony, NOT by
// habit (the old default was "always major pentatonic," which was a copy-paste,
// not a decision):
//
//   • SCALE-LOCK (chordLock = 0, the default) — the cells are every note of a
//     FIXED scale; the current chord's tones are merely LIT. Pass the SONG'S OWN
//     scale, not a generic pentatonic: a modal station (jangle = mixolydian, air
//     = its archetype's mode) should hand the player the mode the lead already
//     roams — its idiom note (the b7, the natural 6) is the whole point. Reserve
//     a pentatonic for stations where pentatonic IS the genuine idiom (dub's
//     minor-pentatonic riddim). The scale must fit the WHOLE song — one mode,
//     few/slow changes.
//
// chordLock sets only the STARTING lock; the open strip carries a "ch"/"sc"
// toggle button (and SOLO_LOCK_KEY) so the PLAYER switches live — a chord-locked
// station that feels too constraining is one tap from free scale play, and vice
// versa. solo_locked(cx) reports the current live state.
//
//   • CHORD-LOCK (chordLock = 1) — the cells are ONLY the live chord's tones
//     (chordPcs across octaves), re-shaped every chord change. The "Omnichord"
//     feel: the station presses the chord button for you, you literally can't
//     hit a non-chord-tone, and the harmony slides under your hand. Use it on
//     CHANGES-HEAVY stations where no single scale fits — jazz (bossa), city pop
//     (citypop, the Royal Road + the +2 gear change), the Mac-DeMarco borrowed
//     chords (jingle), borrowed-chord progressions (polopan). A held note does
//     NOT re-pitch on a chord change — it re-maps on your NEXT press, so your
//     finger only glides when YOU move (calm, never a note sliding out from
//     under you). Light the chord ROOT; the rest are its other tones.
//
// ── MAKING ROOM: the station must yield its own lead ───────────────────────
// Most stations ALSO play their own melodic lead (a flute, a whistle, a
// melodica). With the strip live there are now two soloists in the same
// register — and two leads blowing at once is mud, on a real bandstand too.
// So the station owes the player room: gate its OWN lead block on one of the
// two signals this header exposes. (The strip never silences the station — it
// can't know which voice is the lead; only the station does. So this is a
// one-line guard YOU add, per station, on the lead's emission.)
//
// Two signals, three strategies — pick by genre, decide by ear:
//
//   • LAY OUT  — guard the lead on `!solo_open()`. The instant the player
//     opens the strip the station stops launching lead notes; gating note-ons
//     (not the sounding voice) lets the current note ring to its end, so the
//     soloist finishes ON THE BEAT and hands over — no mid-note chop. A held/
//     legato lead (jangle) should route through its existing trail-off so the
//     sustained voice releases instead of cutting. This is the default: clear
//     "I've got it" gesture, zero clash. Cost: a brief hole if the player opens
//     the tab and doesn't play — which reads as an invitation, not a bug.
//     Used by: bossa, citypop, jingle, jangle.
//
//   • TRADE   — guard the lead on `!solo_playing()`. The station lead drops out
//     ONLY while a strip note is actually held, and fills the gaps when the
//     player rests: call-and-response / trading fours. No hole, the busiest and
//     most "alive" feel. Best where a tail bridges the handoff — dub's tape
//     echo makes the melodica trade gorgeous. Used by: dub.
//
//   • DUCK    — keep the lead sounding but quieter while the player plays.
//     Truest to a mixing desk, but our notes are scheduled-ahead at a fixed
//     vol, so it's awkward to retro-quiet them; TRADE (gate the emission) is
//     the practical stand-in here. Documented for completeness, unused so far.
//
// Don't put the strip on stations whose idiom has no soloist to begin with
// (ambient, satie) — there's nothing to make room, and a lead pad just fights
// the texture. The five stations above were chosen because each already has a
// lead voice the player is stepping in for. Full reasoning: game-music.md →
// "the jam layer".
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
#ifndef SOLO_LOCK_KEY
#define SOLO_LOCK_KEY 'L'     // toggles chord-lock ↔ scale-lock live (the "ch"/"sc" button)
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
    bool struck;              // STRUCK voice (mallet/xylophone): a drag RE-STRIKES each new
                              // bar (a run of struck notes, old ones ringing out) instead of
                              // gliding one held voice; release lets the last note ring.
                              // Default 0 = SUSTAINED (held + glide), what every lead jam uses.
    bool chordLock;           // CHORD-LOCK (Omnichord): the strip's cells become the LIVE
                              // chord's tones (chordPcs across octaves), rebuilt every chord
                              // change, instead of a fixed scale — you literally can't play a
                              // non-chord-tone, and the harmony moves under your hand. The
                              // station "presses the chord button" for you. A held note does
                              // NOT re-pitch when the chord changes (it re-maps on the NEXT
                              // press); your finger only glides when YOU move to a new cell.
                              // For changes-heavy stations (jazz/citypop) where no single
                              // scale fits. Default 0 = SCALE-LOCK (the classic jam strip).
                              // This is just the STARTING state — the player flips it live
                              // with the "ch"/"sc" button (or SOLO_LOCK_KEY), so chord-lock is
                              // never a trap: too constraining? tap to free scale play.
} SoloCtx;

static bool solo_open_f  = false;
static int  solo_handle  = -1;     // the held mono voice (-1 = silent)
static int  solo_curMidi = -1;     // what's sounding (display / trace)
static int  solo_curCell = -1;     // the held finger's cell INDEX (re-pitch only when this moves —
                                   // so a chord-lock chord change under a still finger doesn't slide)
static float solo_curY   = -1;     // last vertical position 0..1 (-1 = silent)
static bool solo_pending = false;  // a press waiting for its 16th (quantize)
static int  solo_pendMidi = -1;
static long solo_pendStep = 0;
static int  solo_oct      = 0;     // octave shift of the playable window (- / + buttons)
static int  solo_lock     = -1;    // LIVE chord-lock: -1 = uninit (seed from cx->chordLock on
                                   // first draw), 0 = scale-lock, 1 = chord-lock. The "ch"/"sc"
                                   // button (and SOLO_LOCK_KEY) flip it — the player can loosen
                                   // a chord-locked station back to free scale play, or tighten.

static bool  solo_open(void) { return solo_open_f; }
static int   solo_midi(void) { return solo_curMidi; }   // -1 = silent
static bool  solo_playing(void) { return solo_curMidi >= 0; }  // a strip note is sounding NOW
static float solo_y(void)    { return solo_curY; }      // vertical 0..1, -1 = silent

// is the strip chord-locked RIGHT NOW (live toggle, falling back to the cart default)?
// chord-lock needs chord material; a station that passes none is always scale-locked.
static bool solo_locked(const SoloCtx *cx) {
    if (!cx->chordPcs || cx->nchordPcs <= 0) return false;
    return solo_lock < 0 ? (cx->chordLock != 0) : (solo_lock > 0);
}

static void solo_kill(void) {
    if (solo_handle >= 0) note_off(solo_handle);
    solo_handle = -1; solo_curMidi = -1; solo_curCell = -1; solo_curY = -1; solo_pending = false;
}

// build the ascending list of playable midis across [lo,hi]; returns count.
// SCALE-lock: every in-scale note. CHORD-lock: only the live chord's tones
// (chordPcs), so the cell layout re-shapes itself each time the chord changes.
static int solo_notes(const SoloCtx *cx, int *out, int max) {
    int n = 0, sh = solo_oct * 12;                          // the octave shift
    bool clock = solo_locked(cx);
    for (int m = cx->loMidi + sh; m <= cx->hiMidi + sh && n < max; m++) {
        if (m < 24 || m > 100) continue;                    // stay in a sane register
        if (clock) {
            int pc = m % 12;
            for (int i = 0; i < cx->nchordPcs; i++)
                if (cx->chordPcs[i] % 12 == pc) { out[n++] = m; break; }
        } else {
            int pc = ((m - cx->root) % 12 + 12) % 12;
            for (int s = 0; s < cx->nscale; s++)
                if (cx->scale[s] == pc) { out[n++] = m; break; }
        }
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
    if (solo_lock < 0) solo_lock = cx->chordLock ? 1 : 0;   // seed the live flag from the cart default
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

    // ── chord-lock ↔ scale-lock toggle (left of the octave pair) ───────────
    // only where there's chord material to lock onto; SOLO_LOCK_KEY flips it too.
    // Lets the player loosen a chord-locked station to free scale play, or tighten.
    bool hasChord = cx->chordPcs && cx->nchordPcs > 0;
    int modeW = hasChord ? 22 : 0;
    int modeX = octDnX - modeW;                             // 0-width (== octDnX) when hidden
    static int mode_id;
    UiCap *mc = 0;
    if (hasChord) {
        if (keyp(SOLO_LOCK_KEY)) solo_lock = solo_lock ? 0 : 1;
        ui_reg(&mode_id, modeX, y, modeW, h, 0);
        mc = ui_cap_for(&mode_id);
        if (mc && mc->released &&
            ui_in(mc->rx, mc->ry, modeX - UI_HIT_PAD, y - UI_HIT_PAD, modeW + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD))
            solo_lock = solo_lock ? 0 : 1;
    }

    int sw = modeX - x - 2;                                 // keybed up to the mode/octave buttons
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

    // note lifecycle: grab → on (quantized or now); drag → glide (sustained) or RE-STRIKE
    // (struck); release → off (sustained) or let the last note ring out (struck).
    // STRUCK voices skip the glide (you can't slur a struck bar) and never note_off on drag —
    // each new bar is a fresh note_on, the previous one decaying on its own (the xylophone run).
    if (ui_grabbed(&strip_id) && midi >= 0) {
        if (cx->quantize) { solo_pending = true; solo_pendMidi = midi; solo_pendStep = (long)pos; solo_curCell = cell; }
        else {
            solo_handle = note_on(midi, cx->instr, SOLO_VOL);
            if (!cx->struck) note_glide(solo_handle, SOLO_GLIDE_MS);
            solo_curMidi = midi; solo_curCell = cell;
        }
    }
    if (solo_pending && (long)pos > solo_pendStep) {        // the quantized strike lands
        solo_handle = note_on(solo_pendMidi, cx->instr, SOLO_VOL);
        if (!cx->struck) note_glide(solo_handle, SOLO_GLIDE_MS);
        solo_curMidi = solo_pendMidi;
        solo_pending = false;
    }
    // re-pitch only when the FINGER moves to a new cell — not when notes[cell] shifts
    // under a still finger (chord-lock chord change → the held note rides on, re-maps next press).
    if (c && !c->released && solo_handle >= 0 && midi >= 0 && cell != solo_curCell) {
        if (cx->struck) solo_handle = note_on(midi, cx->instr, SOLO_VOL);  // RE-STRIKE; old note rings out
        else            note_pitch(solo_handle, midi);                     // SUSTAINED: slide to the new degree
        solo_curMidi = midi; solo_curCell = cell;
    }
    if (ui_released(&strip_id)) {
        if (cx->struck) { solo_curMidi = -1; solo_curY = -1; solo_pending = false; }  // let the last bar ring
        else solo_kill();
    }

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
    bool clock = solo_locked(cx);
    for (int i = 0; i < nn; i++) {
        int col;
        if (clock)                                            // every cell is a chord tone:
            col = (notes[i] % 12 == cx->chordPcs[0] % 12)     // light the chord's ROOT…
                ? accent : CLR_MEDIUM_GREY;                   // …the rest are its other tones
        else
            col = solo_is_chord_pc(cx, notes[i]) ? accent
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

    // the chord-lock toggle: lit accent + "ch" = chord-locked; dim + "sc" = free scale play
    if (hasChord) {
        bool modeHot = mc != 0 || ui_hover(modeX, y, modeW, h);
        bool lk = solo_lock > 0;
        int bg = lk ? accent : (modeHot ? CLR_DARK_GREY : CLR_DARKER_GREY);
        const char *lbl = lk ? "ch" : "sc";
        rectfill(modeX, y, modeW, h, bg);
        rect(modeX, y, modeW, h, accent);
        print(lbl, modeX + (modeW - text_width(lbl)) / 2, y + (h - 6) / 2, lk ? CLR_BLACK : accent);
    }

    // the tab, lit (tap to close)
    rectfill(tx, y, tw, h, accent);
    rect(tx, y, tw, h, accent);
    print("jam", tx + (tw - text_width("jam")) / 2, y + (h - 6) / 2, CLR_BLACK);
}

#endif // SOLO_H
