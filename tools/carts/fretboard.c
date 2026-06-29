/* de:meta
{
  "title": "fretboard",
  "status": "active",
  "created": "2026-06-14",
  "kind": [
    "instrument",
    "toy"
  ],
  "teaches": [
    "chord-voicing"
  ],
  "lineage": "Multi-string extension of the monochord probe — same PD-oscillator/glide/position=pitch feel, scaled to six strings so finger geometry directly voices chords (vertical rake = barre, angled fingers = voicings).",
  "description": "A multitouch fretted string instrument — TAP a string at a fret to pluck it, tap several strings at once and you get a CHORD: the chord is your finger GEOMETRY, not a button you press. The payoff of the monochord experiment, and the honest version of pedalboard's chord buttons (here you actually form the shape). Six strings, standard tuning E A D G B e; frets quantize the position so you land in tune. TAP a string @ a fret = pluck it; DRAG across the strings = a strum (each sounds as you cross it); slide along a string = re-fret. SPACE strums all six at their held frets. DESKTOP crutch (you can't multitouch a mouse): hold 1-6 for an open-chord shape (Em G C D Am A), then SPACE or drag to strum it — on a phone you fret the shape by hand instead. INSTR_GUITAR. A full six-finger chord + strum exceeds the iPhone 5-touch cap, so it leans desktop/iPad."
}
de:meta */
// fretboard — the monochord, with more strings. Six FRETLESS strings stacked into
// a neck: touch a string and your finger's POSITION along it is the pitch (no frets),
// slide and it GLIDES, lift and it rings out. Touch several strings = a chord; the
// chord is your finger GEOMETRY, not a button. A straight vertical rake = the open
// tuning moved up the neck (a moveable barre); angle your fingers for everything else.
//
// The multi-string payoff of the `monochord` probe — same feel (PD oscillator, glide,
// position = pitch), just six of them, so you can voice chords by hand.
//
//   • TOUCH a string        = pluck it at that position; SLIDE along it = glide the pitch.
//   • TOUCH several strings  = a chord (one finger per string).
//   • DRAG across the strings = a strum (each sounds as the finger crosses it).
//   • SPACE (desktop)        = strum all six at the MOUSE's position — a barre chord you
//                              can slide; move the mouse and tap SPACE to hear it move.
//   • the faint ticks         = semitone marks (the bright one = the 12th, an octave up).
//
// Standard tuning E A D G B e. INSTR_GUITAR-on-PD voicing glides both ways. Two-handed
// chording wants a touchscreen; on desktop the mouse is one finger + the SPACE barre.

#include "studio.h"
#include "pointer.h"
#include <math.h>

#define I_STR 5
#define NSTR  6
#define MAXSEMI 15                                  // how many semitones the neck spans

static const int   OPEN[NSTR]  = { 40, 45, 50, 55, 59, 64 };   // E2 A2 D3 G3 B3 E4
static const char *SNAME[NSTR] = { "E", "A", "D", "G", "B", "e" };

// neck geometry — strings run left→right; low E (string 0) at the top
#define NUT_X    34
#define NECK_W   (SCREEN_W - NUT_X - 8)
#define PX_SEMI  (NECK_W / MAXSEMI)                 // pixels per semitone along a string
#define STR_TOP  46
#define STR_GAP  ((SCREEN_H - STR_TOP - 26) / (NSTR - 1))
static int   str_y(int s)   { return STR_TOP + s * STR_GAP; }
static float pitch_at(int tx, int s) {
    float off = clamp((float)(tx - NUT_X) / (float)PX_SEMI, 0.0f, (float)MAXSEMI);
    return (float)OPEN[s] + off;
}
static int str_at(int ty) {
    for (int s = 0; s < NSTR; s++) if (abs(ty - str_y(s)) <= STR_GAP / 2) return s;
    return -1;
}

typedef struct { int id, lastS; } Ptr;              // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

// one ringing voice per string — pluck (re)triggers it, sliding glides it, it decays on its own
static int   vH[NSTR];        // note handle, -1 = silent
static float vVol[NSTR];      // visual/decay level
static float vMidi[NSTR];     // current pitch
static float flash[NSTR];     // pluck-attack glow

static void pluck_str(int s, int tx) {
    if (s < 0 || s >= NSTR) return;
    float m = pitch_at(tx, s);
    if (vH[s] >= 0) note_off(vH[s]);
    vH[s] = note_on((int)(m + 0.5f), I_STR, 6);
    note_glide(vH[s], 24);
    note_pitch(vH[s], m);
    vVol[s] = 1.0f; vMidi[s] = m; flash[s] = 1.0f;
}

void init(void) {
    // a plucked string that rings and can be slid: instant attack, long decay, sustain 0
    instrument(I_STR, INSTR_PD, 2, 1500, 0, 300);
    instrument_harmonics(I_STR, 0.5f);
    instrument_timbre(I_STR, 0.45f);
    instrument_morph(I_STR, 0.5f);
    PTR_CLEAR(ptr);
    for (int s = 0; s < NSTR; s++) { vH[s] = -1; vVol[s] = 0; flash[s] = 0; }
    for (int s = 0; s < NSTR; s++) pluck_str(s, NUT_X + 3 * PX_SEMI);   // a lively first-frame chord
}

void update(void) {
    if (keyp(KEY_SPACE)) for (int s = 0; s < NSTR; s++) pluck_str(s, mouse_x());   // desktop barre at the cursor

    // each finger plays the string under it, re-plucking when it crosses to a new string
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) p->lastS = -1;
        int s = str_at(ty);
        if (s >= 0) {
            if (s != p->lastS) pluck_str(s, tx);                 // arrived on a new string → pluck
            vMidi[s] = pitch_at(tx, s);                          // and ride the pitch with x (glide)
            if (vH[s] >= 0) note_pitch(vH[s], vMidi[s]);
        }
        p->lastS = s;
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // voices ring down on their own (the pluck decays even while a finger rests on the string)
    for (int s = 0; s < NSTR; s++) if (vH[s] >= 0) {
        vVol[s] *= 0.965f;
        if (vVol[s] < 0.03f) { note_off(vH[s]); vH[s] = -1; }
    }

#ifdef DE_TRACE
    int nf = 0; for (int j = 0; j < PTR_MAX; j++) if (ptr[j].id != PTR_NONE && ptr[j].lastS >= 0) nf++;
    watch("fingers", "%d", nf);
    int nv = 0; for (int s = 0; s < NSTR; s++) if (vH[s] >= 0) nv++;
    watch("ringing", "%d", nv);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("FRETBOARD", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("six fretless strings - position is pitch", 80, 8, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    int y0 = str_y(0), y1 = str_y(NSTR - 1);

    // the neck + faint semitone marks (the octave brighter)
    rectfill(NUT_X, y0 - 12, NECK_W, y1 - y0 + 24, CLR_DARK_BROWN);
    for (int k = 1; k <= MAXSEMI; k++) {
        int x = NUT_X + k * PX_SEMI;
        int c = (k == 12) ? CLR_PEACH : CLR_BROWN;
        for (int y = y0 - 10; y <= y1 + 10; y += 4) pset(x, y, c);
    }
    rectfill(NUT_X - 3, y0 - 12, 3, y1 - y0 + 24, CLR_LIGHT_PEACH);   // the nut

    // strings — low E (thick) at top; glow as they ring, brightest on the pluck attack
    for (int s = 0; s < NSTR; s++) {
        int y = str_y(s);
        flash[s] *= 0.85f;
        float lvl = vVol[s] > flash[s] ? vVol[s] : flash[s];
        int col = flash[s] > 0.4f ? CLR_LIGHT_YELLOW : lvl > 0.12f ? CLR_PEACH : CLR_LIGHT_GREY;
        int th = (NSTR - s) / 2;
        for (int t = -th; t <= th; t++) line(NUT_X, y + t, NUT_X + NECK_W, y + t, col);
        print(SNAME[s], 14, y - 3, lvl > 0.12f ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    // live finger positions
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].lastS >= 0) {
            int s = ptr[j].lastS;
            int x = clamp(NUT_X + (int)((vMidi[s] - OPEN[s]) * PX_SEMI), NUT_X, NUT_X + NECK_W);
            circfill(x, str_y(s), 4, CLR_LIGHT_YELLOW);
        }

    font(FONT_TINY);
    print("TOUCH a string = pluck + SLIDE to glide   DRAG across = strum   SPACE = barre-strum at the cursor", 8, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
