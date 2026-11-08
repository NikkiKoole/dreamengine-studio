/* de:meta
{
  "slug": "chordband",
  "title": "chord band",
  "status": "active",
  "created": "2026-11-08",
  "kind": ["instrument", "toy"],
  "teaches": ["chord-voicing", "functional-harmony", "step-sequencer"],
  "lineage": "After Polly (charlesvestal/fm1-polly), a QY70-style pocket arranger firmware for the M-VAVE FM-1. The idea it rests on, every part written ONCE over C major 7 and moved to the chord you hold by one of five note-transposition rules, is Yamaha's QY70 design (owner's manual p.147). Written here from that description; no Polly code. FM-1 face as in amb1.",
  "description": {
    "summary": "Hold a chord, and an eight-piece band plays it. Every part is written once, over C major 7.",
    "detail": "The trick of the 90s arranger keyboards (the Yamaha QY70 is the one Polly copies): a style's eight parts are each written a single time, over C major 7, and every part carries a RULE that says how it follows the chord you hold. BYPASS (drums) never moves. PARALLEL (a riff) shifts by the root, so the lick keeps its shape. CHORD 1 (the pad) maps each degree to the same degree of your chord: the written E is the 3rd, so over A minor it becomes C. CHORD 2 (keys, guitar) does the same and then picks the octave closest to its last note, so the voices move as little as they can. BASS does CHORD 1 but lands its first note after a change on the root, or on the slash note in F/A. Every chord type also implies a scale (major ionian, 7 mixolydian, minor dorian, m7b5 locrian), so a written 9th or 13th lands right over anything. The screen shows each part, its rule, and its pattern; the big name is the chord the band thinks you are holding.",
    "controls": "Hold keys (click, touch, or A..L / W..P on QWERTY, Z/X octave) to set the chord; it stays until you play the next one. Lowest key not the root = a slash chord. Knobs (drag): STYLE pop / bossa / funk, TEMPO, BAND how many parts play (drums first), SPACE reverb. SPACE starts and stops."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "drumkit.h"
#include <stdio.h>
#include <string.h>

// ── chords ───────────────────────────────────────────────────────────────
// A chord = root pitch class + type + (optional) bass pitch class.
// Each type lists its tones (for recognising it from held keys) and the
// semitones of its seven DEGREES R 9 3 11 5 13 7 (for moving notes onto it):
// its own tones where it has them, the implied scale between them. A triad
// has no 7th, so its 7 degree folds onto the octave.
enum { DG_R, DG_9, DG_3, DG_11, DG_5, DG_13, DG_7, NDG };
typedef struct { const char *name; uint16_t tones; int8_t deg[NDG]; } ChordType;
#define T(a) (1u << (a))
static const ChordType CT[] = {
    { "",     T(0)|T(4)|T(7),           { 0, 2, 4, 5, 7, 9, 12 } },   // major (ionian)
    { "m",    T(0)|T(3)|T(7),           { 0, 2, 3, 5, 7, 9, 12 } },   // minor (dorian)
    { "7",    T(0)|T(4)|T(7)|T(10),     { 0, 2, 4, 5, 7, 9, 10 } },   // dominant (mixolydian)
    { "M7",   T(0)|T(4)|T(7)|T(11),     { 0, 2, 4, 5, 7, 9, 11 } },
    { "m7",   T(0)|T(3)|T(7)|T(10),     { 0, 2, 3, 5, 7, 9, 10 } },
    { "6",    T(0)|T(4)|T(7)|T(9),      { 0, 2, 4, 5, 7, 9, 9 } },
    { "m6",   T(0)|T(3)|T(7)|T(9),      { 0, 2, 3, 5, 7, 9, 9 } },
    { "sus4", T(0)|T(5)|T(7),           { 0, 2, 5, 5, 7, 9, 12 } },
    { "7sus4",T(0)|T(5)|T(7)|T(10),     { 0, 2, 5, 5, 7, 9, 10 } },
    { "add9", T(0)|T(2)|T(4)|T(7),      { 0, 2, 4, 5, 7, 9, 12 } },
    { "9",    T(0)|T(2)|T(4)|T(7)|T(10),{ 0, 2, 4, 5, 7, 9, 10 } },
    { "mM7",  T(0)|T(3)|T(7)|T(11),     { 0, 2, 3, 5, 7, 9, 11 } },
    { "m7b5", T(0)|T(3)|T(6)|T(10),     { 0, 1, 3, 5, 6, 8, 10 } },   // locrian
    { "dim",  T(0)|T(3)|T(6),           { 0, 2, 3, 5, 6, 8, 9 } },    // diminished: the 7 is the dim 7th
    { "aug",  T(0)|T(4)|T(8),           { 0, 2, 4, 6, 8, 9, 12 } },
};
#define NCT ((int)(sizeof CT / sizeof CT[0]))
typedef struct { int root, type, bass; } Chord;     // bass -1 = none
static const char *PCN[12] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };

static int popc(unsigned x) { int n = 0; while (x) { n += x & 1; x >>= 1; } return n; }
static unsigned rot(unsigned set, int r) { return ((set >> r) | (set << (12 - r))) & 0xFFFu; }

// which chord are these keys? Try every held pitch class as the root; an exact
// match wins (lowest key first), else the type sharing the most tones.
static bool recognise(const int *notes, int n, Chord *out) {
    if (n <= 0) return false;
    unsigned set = 0; int low = 128;
    for (int i = 0; i < n; i++) { set |= 1u << (notes[i] % 12); if (notes[i] < low) low = notes[i]; }
    int lowpc = low % 12;
    if (popc(set) == 1) { *out = (Chord){ lowpc, 0, -1 }; return true; }
    int best = -999, br = lowpc, bt = 0;
    for (int k = 0; k < 12; k++) {
        int r = (lowpc + k) % 12;                    // lowest key tried first
        if (!(set & (1u << r))) continue;
        unsigned rel = rot(set, r);
        for (int t = 0; t < NCT; t++) {
            unsigned tt = CT[t].tones;
            int score = (rel == tt) ? 100 - k
                      : 2 * popc(rel & tt) - 2 * popc(rel & ~tt) - popc(tt & ~rel) - k / 4;
            if (score > best) { best = score; br = r; bt = t; }
        }
    }
    *out = (Chord){ br, bt, lowpc != br ? lowpc : -1 };
    return true;
}

static void chord_name(Chord c, char *b, int n) {
    if (c.bass >= 0) snprintf(b, n, "%s%s/%s", PCN[c.root], CT[c.type].name, PCN[c.bass]);
    else snprintf(b, n, "%s%s", PCN[c.root], CT[c.type].name);
}

// ── the transposer ───────────────────────────────────────────────────────
// A written note's pitch class read as a degree of C major 7, plus how far
// it sits ABOVE that degree (C# is "the root, raised").
static const int8_t SRC_DG[12]  = { DG_R, DG_R, DG_9, DG_9, DG_3, DG_11, DG_11, DG_5, DG_5, DG_13, DG_13, DG_7 };
static const int8_t SRC_OFF[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
enum { R_BYPASS, R_PARALLEL, R_CHORD1, R_CHORD2, R_BASS };
static const char *RULE_TAG[5] = { "BYP", "PAR", "CH1", "CH2", "BAS" };

static int fold(int n, int lo, int hi) {
    while (n > hi && n - 12 >= lo) n -= 12;
    while (n < lo && n + 12 <= hi) n += 12;
    return n;
}
static int nearest(int n, int to) {          // n moved by octaves as close to `to` as it gets
    while (n - to > 6) n -= 12;
    while (to - n > 6) n += 12;
    return n;
}
// the root as a shift that keeps the part near where it was written: -5..+6
static int root_shift(int root) { return root > 6 ? root - 12 : root; }

static int degree_map(int note, Chord c) {
    int pc = note % 12, d = SRC_DG[pc];
    return note - pc + root_shift(c.root) + CT[c.type].deg[d] + SRC_OFF[pc];
}

// ── the band ─────────────────────────────────────────────────────────────
enum { TR_D1, TR_D2, TR_PC, TR_BA, TR_C1, TR_C2, TR_C3, TR_C4, NTR };
static const char *TR_NAME[NTR] = { "D1", "D2", "PC", "BA", "C1", "C2", "C3", "C4" };
static const char *TR_WHAT[NTR] = { "kit", "hats", "conga", "bass", "pad", "epiano", "guitar", "brass" };
static const int   TR_RULE[NTR] = { R_BYPASS, R_BYPASS, R_BYPASS, R_BASS, R_CHORD1, R_CHORD2, R_CHORD2, R_PARALLEL };
static const int   TR_LO[NTR]   = { 0, 0, 0, 28, 52, 55, 50, 60 };
static const int   TR_HI[NTR]   = { 127, 127, 127, 52, 77, 81, 76, 86 };
static const int   TR_COL[NTR]  = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO, CLR_PINK, CLR_PEACH };
// BAND knob: the order parts join in
static const int ENTRY[NTR] = { TR_D1, TR_BA, TR_D2, TR_C1, TR_C2, TR_C3, TR_PC, TR_C4 };

typedef struct { int last; bool fresh; } XState;
static XState xs[NTR];

static int xpose(int tr, int note, Chord c) {
    int r = TR_RULE[tr], out;
    switch (r) {
    case R_BYPASS:   return note;
    case R_PARALLEL: out = note + root_shift(c.root); break;
    case R_CHORD2:
        out = degree_map(note, c);
        if (xs[tr].last >= 0) out = nearest(out, xs[tr].last);
        break;
    case R_BASS:
        out = degree_map(note, c);
        if (xs[tr].fresh) {                 // land on the root (or slash note) at or BELOW the written note
            int r = out - out % 12 + (c.bass >= 0 ? c.bass : c.root);
            out = r > out ? r - 12 : r;
        }
        break;
    default: out = degree_map(note, c); break;
    }
    out = fold(out, TR_LO[tr], TR_HI[tr]);
    xs[tr].last = out; xs[tr].fresh = false;
    return out;
}

// ── styles: every note written over C major 7 ────────────────────────────
// 32 steps = two bars of 16ths. Drum notes are GM numbers.
typedef struct { uint8_t tr, step, note, len, vel; } Ev;
typedef struct { const char *name; int bpm; const Ev *ev; int n; } Style;

static const Ev POP[] = {
    // D1: kick + snare
    {0,0,36,1,7},{0,6,36,1,5},{0,8,36,1,6},{0,4,38,1,6},{0,12,38,1,6},
    {0,16,36,1,7},{0,22,36,1,5},{0,24,36,1,6},{0,26,36,1,4},{0,20,38,1,6},{0,28,38,1,6},{0,31,38,1,3},
    // D2: 8th hats, an open one to close each bar
    {1,0,42,1,4},{1,2,42,1,3},{1,4,42,1,4},{1,6,42,1,3},{1,8,42,1,4},{1,10,42,1,3},{1,12,42,1,4},{1,14,46,2,3},
    {1,16,42,1,4},{1,18,42,1,3},{1,20,42,1,4},{1,22,42,1,3},{1,24,42,1,4},{1,26,42,1,3},{1,28,42,1,4},{1,30,46,2,3},
    // PC: a soft conga answer
    {2,14,62,1,3},{2,15,64,1,2},{2,30,62,1,3},{2,31,60,1,3},
    // BA: root, push, fifth, root; second bar walks up through the 3rd
    {3,0,36,3,6},{3,3,36,1,4},{3,6,36,2,5},{3,8,43,2,5},{3,10,36,2,5},{3,14,43,2,4},
    {3,16,36,3,6},{3,19,36,1,4},{3,22,36,2,5},{3,24,43,2,5},{3,26,40,2,5},{3,28,41,2,4},{3,30,43,2,4},
    // C1: pad, 5 R 3 7, a bar each
    {4,0,55,16,3},{4,0,60,16,3},{4,0,64,16,3},{4,0,71,16,2},
    {4,16,55,16,3},{4,16,60,16,3},{4,16,64,16,3},{4,16,71,16,2},
    // C2: e.piano on the off-beats
    {5,2,64,2,4},{5,2,67,2,4},{5,2,72,2,3},{5,6,64,1,3},{5,6,67,1,3},{5,10,64,2,4},{5,10,67,2,4},{5,10,72,2,3},
    {5,18,64,2,4},{5,18,67,2,4},{5,18,72,2,3},{5,22,64,1,3},{5,22,67,1,3},{5,26,64,2,4},{5,26,67,2,4},{5,26,74,2,3},
    // C3: guitar arpeggio in 8ths
    {6,0,60,2,4},{6,2,64,2,3},{6,4,67,2,3},{6,6,72,2,3},{6,8,74,2,3},{6,10,72,2,3},{6,12,67,2,3},{6,14,64,2,3},
    {6,16,60,2,4},{6,18,64,2,3},{6,20,67,2,3},{6,22,71,2,3},{6,24,72,2,3},{6,26,71,2,3},{6,28,67,2,3},{6,30,64,2,3},
    // C4: a brass lick into the next bar (moves with the root, keeps its shape)
    {7,27,67,1,4},{7,28,69,1,4},{7,29,72,3,5},
};
static const Ev BOSSA[] = {
    // D1: kick on 1 and the "and of 2", side-stick clave (37)
    {0,0,36,1,5},{0,6,36,1,4},{0,8,36,1,5},{0,14,36,1,4},{0,16,36,1,5},{0,22,36,1,4},{0,24,36,1,5},{0,30,36,1,4},
    {0,0,37,1,5},{0,3,37,1,4},{0,6,37,1,5},{0,10,37,1,4},{0,12,37,1,5},
    {0,19,37,1,4},{0,22,37,1,5},{0,26,37,1,4},{0,28,37,1,5},
    // D2: 16th shaker on the closed hat, soft
    {1,0,42,1,3},{1,1,42,1,1},{1,2,42,1,2},{1,3,42,1,1},{1,4,42,1,3},{1,5,42,1,1},{1,6,42,1,2},{1,7,42,1,1},
    {1,8,42,1,3},{1,9,42,1,1},{1,10,42,1,2},{1,11,42,1,1},{1,12,42,1,3},{1,13,42,1,1},{1,14,42,1,2},{1,15,42,1,1},
    {1,16,42,1,3},{1,17,42,1,1},{1,18,42,1,2},{1,19,42,1,1},{1,20,42,1,3},{1,21,42,1,1},{1,22,42,1,2},{1,23,42,1,1},
    {1,24,42,1,3},{1,25,42,1,1},{1,26,42,1,2},{1,27,42,1,1},{1,28,42,1,3},{1,29,42,1,1},{1,30,42,1,2},{1,31,42,1,1},
    // PC: congas, quiet
    {2,7,64,1,2},{2,15,62,1,2},{2,23,64,1,2},{2,31,60,1,2},
    // BA: the bossa bass, root then fifth below
    {3,0,36,5,6},{3,6,31,2,5},{3,8,31,5,5},{3,14,36,2,5},
    {3,16,36,5,6},{3,22,31,2,5},{3,24,31,5,5},{3,30,36,2,5},
    // C1: a quiet pad under it
    {4,0,64,16,2},{4,0,71,16,2},{4,16,64,16,2},{4,16,71,16,2},
    // C3: nylon guitar on the comp rhythm, voiced 3 5 7 9
    {6,0,52,2,4},{6,0,55,2,3},{6,0,59,2,3},{6,0,62,2,3},
    {6,3,52,2,3},{6,3,55,2,3},{6,3,59,2,3},{6,3,62,2,3},
    {6,6,52,2,4},{6,6,55,2,3},{6,6,59,2,3},{6,6,62,2,3},
    {6,10,52,2,3},{6,10,55,2,3},{6,10,59,2,3},{6,10,62,2,3},
    {6,12,52,2,4},{6,12,55,2,3},{6,12,59,2,3},{6,12,62,2,3},
    {6,19,52,2,3},{6,19,55,2,3},{6,19,59,2,3},{6,19,62,2,3},
    {6,22,52,2,4},{6,22,55,2,3},{6,22,59,2,3},{6,22,62,2,3},
    {6,26,52,2,3},{6,26,55,2,3},{6,26,59,2,3},{6,26,62,2,3},
    {6,28,52,2,4},{6,28,55,2,3},{6,28,59,2,3},{6,28,62,2,3},
    // C2: e.piano, one soft 9th-13th answer per two bars
    {5,13,74,3,3},{5,13,69,3,3},{5,29,72,3,3},{5,29,76,3,3},
};
static const Ev FUNK[] = {
    // D1
    {0,0,36,1,7},{0,3,36,1,5},{0,10,36,1,6},{0,4,38,1,6},{0,12,38,1,6},{0,7,38,1,2},{0,15,38,1,2},
    {0,16,36,1,7},{0,19,36,1,5},{0,26,36,1,6},{0,30,36,1,4},{0,20,38,1,6},{0,28,38,1,6},{0,23,38,1,2},
    // D2: 16th hats with accents, open on the last 8th
    {1,0,42,1,4},{1,1,42,1,2},{1,2,42,1,4},{1,3,42,1,2},{1,4,42,1,4},{1,5,42,1,2},{1,6,42,1,4},{1,7,42,1,2},
    {1,8,42,1,4},{1,9,42,1,2},{1,10,42,1,4},{1,11,42,1,2},{1,12,42,1,4},{1,13,42,1,2},{1,14,46,2,3},
    {1,16,42,1,4},{1,17,42,1,2},{1,18,42,1,4},{1,19,42,1,2},{1,20,42,1,4},{1,21,42,1,2},{1,22,42,1,4},{1,23,42,1,2},
    {1,24,42,1,4},{1,25,42,1,2},{1,26,42,1,4},{1,27,42,1,2},{1,28,42,1,4},{1,29,42,1,2},{1,30,46,2,3},
    // PC
    {2,6,64,1,3},{2,11,62,1,3},{2,22,64,1,3},{2,27,62,1,3},{2,29,60,1,3},
    // BA: octave pops and the flat 7 (written Bb = "13, raised")
    {3,0,36,2,6},{3,3,48,1,5},{3,6,36,1,5},{3,7,46,1,4},{3,8,43,2,5},{3,10,36,1,5},{3,11,48,1,4},{3,14,46,2,4},
    {3,16,36,2,6},{3,19,48,1,5},{3,22,36,1,5},{3,23,46,1,4},{3,24,43,1,5},{3,26,40,1,4},{3,27,41,1,4},{3,28,43,2,5},
    // C2: clav-ish e.piano stabs, voiced 3 7 9 (the funk "9" chord)
    {5,1,64,1,4},{5,1,71,1,4},{5,1,74,1,3},{5,6,64,1,4},{5,6,71,1,4},{5,6,74,1,3},
    {5,9,64,1,3},{5,9,71,1,3},{5,9,74,1,3},{5,14,64,1,4},{5,14,71,1,4},{5,14,74,1,3},
    {5,17,64,1,4},{5,17,71,1,4},{5,17,74,1,3},{5,22,64,1,4},{5,22,71,1,4},{5,22,74,1,3},
    {5,25,64,1,3},{5,25,71,1,3},{5,25,74,1,3},
    // C3: muted chicken-scratch guitar on one note
    {6,2,67,1,3},{6,6,67,1,3},{6,10,67,1,3},{6,13,67,1,2},{6,18,67,1,3},{6,22,67,1,3},{6,26,67,1,3},{6,29,67,1,2},
    // C4: horn hit
    {7,28,67,1,5},{7,29,70,1,5},{7,30,72,2,6},
};
static const Style STYLES[] = {
    { "POP",   100, POP,   (int)(sizeof POP   / sizeof POP[0]) },
    { "BOSSA", 132, BOSSA, (int)(sizeof BOSSA / sizeof BOSSA[0]) },
    { "FUNK",  104, FUNK,  (int)(sizeof FUNK  / sizeof FUNK[0]) },
};
#define NSTYLE ((int)(sizeof STYLES / sizeof STYLES[0]))

// ── sounds ───────────────────────────────────────────────────────────────
#define SL_KIT   20     // drumkit.h: 20..28
#define SL_BASS  5
#define SL_PAD   6
#define SL_EP    7
#define SL_GTR   8
#define SL_BRASS 9
#define SL_CONGA 10
#define SL_RIM   11
static const int TR_SLOT[NTR] = { -1, -1, SL_CONGA, SL_BASS, SL_PAD, SL_EP, SL_GTR, SL_BRASS };

static void drum(double t, int gm, int vel) {
    int role = -1;
    switch (gm) {
    case 36: role = DK_KICK; break;
    case 38: role = DK_SNARE; break;
    case 42: role = DK_HHC; break;
    case 46: role = DK_HHO; break;
    case 39: role = DK_CLAP; break;
    case 49: role = DK_CRASH; break;
    case 37: schedule_at(t, 79, SL_RIM, vel, 60); return;   // side stick: a short wood click
    }
    if (role >= 0) dk_play_at(&DK_ACOUSTIC, SL_KIT, t, role, 0, vel);
}

// ── state ────────────────────────────────────────────────────────────────
static Chord cur = { 0, 3, -1 };       // C major 7: the patterns as written
static int   held_prev;
static bool  playing = true;
static double t0;                         // audio time of step 0
static long  next_step;
static float kn[4] = { 0.0f, 0.5f, 1.0f, 0.45f };   // STYLE TEMPO BAND SPACE
static float kn_applied[4] = { -1, -1, -1, -1 };
static int   drag_knob = -1;
static float drag_y0, drag_v0;
static const char *KN_NAME[4] = { "STYLE", "TEMPO", "BAND", "SPACE" };
static const int   KN_COL[4]  = { CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_BLUE };
static double flash_t[NTR][8]; static int flash_w[NTR];   // booked hit times, to light the lanes on time
static float glow[NTR];
static double chord_flash;

// the pad HOLDS its notes (note_on), so a chord change can restrike them
typedef struct { int src, handle, vel; double start, end; bool on; } PadV;
#define NPAD 8
static PadV pad[NPAD];

static int style_i(void) { int s = (int)(kn[0] * NSTYLE); return s >= NSTYLE ? NSTYLE - 1 : s; }
static int tempo(void) { return (int)(STYLES[style_i()].bpm * (0.7f + 0.6f * kn[1])); }
static int band_n(void) { int n = 1 + (int)(kn[2] * 7.999f); return n; }
static bool track_on(int tr) { for (int i = 0; i < band_n(); i++) if (ENTRY[i] == tr) return true; return false; }
static double step_dur(void) { return 60.0 / tempo() / 4.0; }

#define KEY_Y 124
#define SCR_X 64
#define SCR_Y 8
#define SCR_W 192
#define SCR_H 100
static int knob_x(int i) { return i < 2 ? 30 : 290; }
static int knob_y(int i) { return (i % 2) ? 88 : 34; }

static void pad_all_off(void) {
    for (int i = 0; i < NPAD; i++) { if (pad[i].on) note_off(pad[i].handle); pad[i].on = false; pad[i].src = -1; }
}

static void set_chord(Chord c) {
    if (c.root == cur.root && c.type == cur.type && c.bass == cur.bass) return;
    cur = c;
    chord_flash = 1.0;
    for (int i = 0; i < NTR; i++) xs[i].fresh = true;
    // restrike: a held pad note with more than an 8th left moves to the new chord tnow
    double tnow = audio_time();
    for (int i = 0; i < NPAD; i++) {
        if (!pad[i].on) continue;
        note_off(pad[i].handle);
        if (pad[i].end - tnow > 2 * step_dur()) pad[i].handle = note_on(xpose(TR_C1, pad[i].src, cur), SL_PAD, pad[i].vel);
        else { pad[i].on = false; pad[i].src = -1; }
    }
}

static void book_step(long s) {
    const Style *st = &STYLES[style_i()];
    double sd = step_dur(), t = t0 + s * sd;
    int ps = (int)(s % 32);
    for (int i = 0; i < st->n; i++) {
        const Ev *e = &st->ev[i];
        if (e->step != ps || !track_on(e->tr)) continue;
        int dur = (int)(e->len * sd * 1000.0 * 0.92);
        if (e->tr == TR_D1 || e->tr == TR_D2) drum(t, e->note, e->vel);
        else if (e->tr == TR_C1) {
            for (int k = 0; k < NPAD; k++) if (pad[k].src < 0) {
                pad[k] = (PadV){ e->note, -1, e->vel, t, t + e->len * sd, false };
                break;
            }
        } else {
            int n = xpose(e->tr, e->note, cur);
            schedule_at(t, n, TR_SLOT[e->tr], e->vel, dur);
        }
        flash_t[e->tr][flash_w[e->tr]++ & 7] = t;
    }
}

static void apply_fx(void) {
    if (kn[3] != kn_applied[3]) {
        reverb(0.45f + 0.4f * kn[3], 0.5f);
        float s = kn[3];
        instrument_reverb(SL_PAD, 0.2f + 0.5f * s); instrument_reverb(SL_EP, 0.1f + 0.4f * s);
        instrument_reverb(SL_GTR, 0.1f + 0.4f * s); instrument_reverb(SL_BRASS, 0.1f + 0.4f * s);
        instrument_reverb(SL_RIM, 0.1f + 0.3f * s); instrument_reverb(SL_CONGA, 0.1f + 0.3f * s);
        kn_applied[3] = kn[3];
    }
}

void init(void) {
    dk_use(&DK_ACOUSTIC, SL_KIT);
    instrument(SL_BASS, INSTR_PLUCK, 2, 300, 5, 120);
    instrument_harmonics(SL_BASS, 0.55f); instrument_timbre(SL_BASS, 0.25f); instrument_morph(SL_BASS, 0.2f);
    instrument(SL_PAD, INSTR_SAW, 350, 400, 5, 700);
    instrument_filter(SL_PAD, FILTER_LOW, 1400, 2);
    instrument(SL_EP, INSTR_EPIANO, 1, 500, 3, 300);
    instrument_harmonics(SL_EP, 0.1f); instrument_timbre(SL_EP, 0.45f);
    instrument(SL_GTR, INSTR_GUITAR, 1, 400, 2, 200);
    instrument_harmonics(SL_GTR, 0.35f); instrument_timbre(SL_GTR, 0.3f); instrument_morph(SL_GTR, 0.45f);
    instrument(SL_BRASS, INSTR_BRASS, 20, 200, 6, 150);
    instrument_harmonics(SL_BRASS, 0.2f); instrument_timbre(SL_BRASS, 0.55f);
    instrument(SL_CONGA, INSTR_MEMBRANE, 1, 300, 0, 200);
    instrument_harmonics(SL_CONGA, 0.6f); instrument_timbre(SL_CONGA, 0.4f);
    instrument(SL_RIM, INSTR_MALLET, 1, 60, 0, 40);
    instrument_harmonics(SL_RIM, 0.0f); instrument_timbre(SL_RIM, 0.9f); instrument_morph(SL_RIM, 0.0f);

    keybed_config(SL_EP, 3, 16);
    keybed_layout(8, KEY_Y, 304, 72);
    keybed_manage_voices(false);
    for (int i = 0; i < NPAD; i++) pad[i].src = -1;
    for (int i = 0; i < NTR; i++) { xs[i].last = -1; xs[i].fresh = true; }
    t0 = audio_time() + 0.15; next_step = 0;
}

static void knobs_update(void) {
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT))
        for (int i = 0; i < 4; i++) {
            int dx = mx - knob_x(i), dy = my - knob_y(i);
            if (dx * dx + dy * dy <= 16 * 16) { drag_knob = i; drag_y0 = my; drag_v0 = kn[i]; }
        }
    if (drag_knob >= 0) {
        if (mouse_down(MOUSE_LEFT)) { float v = drag_v0 + (drag_y0 - my) / 80.0f; kn[drag_knob] = v < 0 ? 0 : v > 1 ? 1 : v; }
        else drag_knob = -1;
    }
    float w = mouse_wheel();
    if (w != 0) for (int i = 0; i < 4; i++) {
        int dx = mx - knob_x(i), dy = my - knob_y(i);
        if (dx * dx + dy * dy <= 16 * 16) { kn[i] += w * 0.03f; kn[i] = kn[i] < 0 ? 0 : kn[i] > 1 ? 1 : kn[i]; }
    }
}

void update(void) {
    static int style_prev = -1;
    knobs_update();
    if (drag_knob < 0) keybed_update();
    apply_fx();

    // the chord: whatever is held, latched until the next keys go down
    int notes[32], n = 0;
    for (int m = 0; m < 128 && n < 32; m++) if (keybed_held(m)) notes[n++] = m;
    if (n > 0) { Chord c; if (recognise(notes, n, &c)) set_chord(c); }
    held_prev = n;

    if (keyp(KEY_SPACE)) {
        playing = !playing;
        if (playing) { t0 = audio_time() + 0.1; next_step = 0; }
        else pad_all_off();
    }
    if (style_i() != style_prev) { style_prev = style_i(); pad_all_off(); }

    double tnow = audio_time();
    if (playing) {
        // book every step that starts within the next ~70 ms, on the sound clock
        while (t0 + next_step * step_dur() < tnow + 0.07) {
            // a tempo change keeps the step we're on: re-anchor t0 to it
            book_step(next_step++);
        }
    }
    // pad voices start and end on the frame that passes their time
    for (int i = 0; i < NPAD; i++) {
        PadV *p = &pad[i];
        if (p->src < 0) continue;
        if (!p->on && tnow >= p->start) { p->handle = note_on(xpose(TR_C1, p->src, cur), SL_PAD, p->vel); p->on = true; }
        if (p->on && tnow >= p->end) { note_off(p->handle); p->on = false; p->src = -1; }
    }
    for (int tr = 0; tr < NTR; tr++) {
        glow[tr] *= 0.85f;
        for (int k = 0; k < 8; k++) if (flash_t[tr][k] > 0 && tnow >= flash_t[tr][k]) { glow[tr] = 1.0f; flash_t[tr][k] = 0; }
    }
    chord_flash *= 0.9;
#ifdef DE_TRACE
    char cn[16]; chord_name(cur, cn, sizeof cn);
    watch("chord", "%s", cn);
    watch("step", "%ld", next_step);
#endif
}

// a tempo knob turn would jump the playhead if t0 stayed put; keep the step we're on
static void retime(void) {
    static int tp = -1;
    int t = tempo();
    if (tp > 0 && t != tp && playing) {
        double tnow = audio_time(), old = 60.0 / tp / 4.0;
        double pos = (tnow - t0) / old;                 // steps elapsed at the old tempo
        t0 = tnow - pos * step_dur();
    }
    tp = t;
}

// ── drawing ──────────────────────────────────────────────────────────────
static void draw_knob(int i) {
    int x = knob_x(i), y = knob_y(i);
    circfill(x, y + 2, 15, CLR_BLACK);
    circfill(x, y, 15, CLR_DARKER_GREY);
    circfill(x, y, 11, KN_COL[i]);
    circ(x, y, 15, drag_knob == i ? CLR_WHITE : CLR_DARK_GREY);
    float a = (0.75f + kn[i] * 1.5f) * 3.14159f;
    line(x + (int)(cosf(a) * 4), y + (int)(sinf(a) * 4), x + (int)(cosf(a) * 10), y + (int)(sinf(a) * 10), CLR_WHITE);
    font(FONT_SMALL);
    char buf[16];
    if (i == 0) snprintf(buf, sizeof buf, "%s", STYLES[style_i()].name);
    else if (i == 1) snprintf(buf, sizeof buf, "%d", tempo());
    else if (i == 2) snprintf(buf, sizeof buf, "%d/8", band_n());
    else snprintf(buf, sizeof buf, "%s", KN_NAME[i]);
    print_centered(i == 3 ? KN_NAME[3] : buf, x, y + 18, CLR_LIGHT_GREY);
    if (i != 3) print_centered(KN_NAME[i], x, y - 24, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

static void draw_screen(void) {
    rectfill(SCR_X - 3, SCR_Y - 3, SCR_W + 6, SCR_H + 6, CLR_BLACK);
    rectfill(SCR_X, SCR_Y, SCR_W, SCR_H, CLR_DARKER_BLUE);
    char buf[48];
    chord_name(cur, buf, sizeof buf);
    font(FONT_COMIC);
    print(buf, SCR_X + 4, SCR_Y + 2, chord_flash > 0.3 ? CLR_WHITE : CLR_PEACH);
    font(FONT_SMALL);
    snprintf(buf, sizeof buf, "%s %d", STYLES[style_i()].name, tempo());
    print(buf, SCR_X + SCR_W - text_width(buf) - 4, SCR_Y + 3, CLR_LIGHT_GREY);
    print(playing ? "PLAY" : "STOP", SCR_X + SCR_W - 22, SCR_Y + 11, playing ? CLR_GREEN : CLR_RED);

    // eight lanes: part, rule, its two bars, playhead
    double tnow = audio_time();
    long cur = playing ? (long)floor((tnow - t0) / step_dur()) : -1;
    int ps = cur >= 0 ? (int)(cur % 32) : -1;
    const Style *st = &STYLES[style_i()];
    for (int tr = 0; tr < NTR; tr++) {
        int y = SCR_Y + 25 + tr * 9;
        bool on = track_on(tr);
        int col = on ? TR_COL[tr] : CLR_DARK_GREY;
        print(TR_NAME[tr], SCR_X + 3, y, on ? (glow[tr] > 0.4f ? CLR_WHITE : col) : CLR_DARK_GREY);
        print(RULE_TAG[TR_RULE[tr]], SCR_X + 14, y, on ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        int gx = SCR_X + 30;
        for (int s = 0; s < 32; s++) {
            bool hit = false;
            for (int i = 0; i < st->n; i++) if (st->ev[i].tr == tr && st->ev[i].step == s) { hit = true; break; }
            int c = hit ? col : (s % 4 == 0 ? CLR_DARKER_GREY : CLR_BLACK);
            if (s == ps && on) c = hit ? CLR_WHITE : CLR_DARK_GREY;
            rectfill(gx + s * 4 + (s >= 16 ? 2 : 0), y + 1, 3, 4, c);
        }
        print(TR_WHAT[tr], gx + 132, y, on ? col : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
}

static void draw_keys(void) {
    int x, y, w, h, m;
    rectfill(4, KEY_Y - 4, 312, 80, CLR_BLACK);
    for (int i = 0; i < keybed_white_count(); i++) {
        keybed_white_rect(i, &x, &y, &w, &h);
        int midi = keybed_white_midi(i);
        bool in_chord = (CT[cur.type].tones >> ((midi % 12 - cur.root + 12) % 12)) & 1;
        rectfill(x + 1, y, w - 2, h, keybed_held(midi) ? CLR_PEACH : CLR_LIGHT_GREY);
        if (in_chord) rectfill(x + 3, y + h - 7, w - 6, 3, CLR_BLUE);
        rectfill(x + 1, y + h - 2, w - 2, 2, CLR_MEDIUM_GREY);
    }
    for (int i = 0; i < keybed_white_count(); i++) {
        if (!keybed_black_rect(i, &x, &y, &w, &h, &m)) continue;
        bool in_chord = (CT[cur.type].tones >> ((m % 12 - cur.root + 12) % 12)) & 1;
        rectfill(x, y, w, h, keybed_held(m) ? CLR_PINK : CLR_DARKER_GREY);
        if (in_chord) rectfill(x + 2, y + h - 6, w - 4, 2, CLR_BLUE);
    }
}

void draw(void) {
    retime();
    cls(CLR_DARKER_GREY);
    rectfill(2, 2, SCREEN_W - 4, SCREEN_H - 4, CLR_DARK_GREY);
    rect(2, 2, SCREEN_W - 4, SCREEN_H - 4, CLR_BLACK);
    draw_screen();
    for (int i = 0; i < 4; i++) draw_knob(i);
    font(FONT_SMALL);
    print("hold keys = chord  SPACE play/stop  Z/X oct", 64, SCR_Y + SCR_H + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    draw_keys();
}

// ── spec: the transposer's rules, as known answers ───────────────────────
#ifdef DE_SPEC
#include "spec.h"
static Chord ch(int r, const char *t, int b) {
    for (int i = 0; i < NCT; i++) if (!strcmp(CT[i].name, t)) return (Chord){ r, i, b };
    return (Chord){ r, 0, b };
}
void spec(void) {
    for (int i = 0; i < NTR; i++) { xs[i].last = -1; xs[i].fresh = true; }
    // CHORD 1: written E4 (the 3rd) over A minor -> C (A's minor 3rd), folded into the pad range
    expect_eq(xpose(TR_C1, 64, ch(9, "m", -1)) % 12, 0, "ch1 E->C over Am");
    // CHORD 1: written B (the 7th) over C major triad folds to the octave root
    expect_eq(xpose(TR_C1, 71, ch(0, "", -1)) % 12, 0, "ch1 7 over triad = root");
    // CHORD 1: written B over G7 -> F (the b7)
    expect_eq(xpose(TR_C1, 71, ch(7, "7", -1)) % 12, 5, "ch1 7 over G7 = F");
    // a written Bb ("13, raised") over C7 -> Bb, the bluesy b7 the funk bass wants
    expect_eq(xpose(TR_C1, 70, ch(0, "7", -1)) % 12, 10, "raised 13 over C7");
    // PARALLEL keeps the interval shape: G->A over D is up a 2nd still
    int a = xpose(TR_C4, 67, ch(2, "m", -1)), b = xpose(TR_C4, 69, ch(2, "m", -1));
    expect_eq(b - a, 2, "parallel keeps shape");
    // BASS: first note after a change is the slash note
    xs[TR_BA].fresh = true;
    expect_eq(xpose(TR_BA, 43, ch(5, "", 9)) % 12, 9, "bass lands on slash");
    // CHORD 2: never jumps more than a tritone from its last note
    xs[TR_C2].last = 64;
    int c2 = xpose(TR_C2, 76, ch(5, "", -1));
    expect(c2 - 64 <= 6 && 64 - c2 <= 6, "chord2 voice-leads");
    // recognition
    Chord r; int k1[] = { 57, 60, 64 }; recognise(k1, 3, &r);
    expect(r.root == 9 && !strcmp(CT[r.type].name, "m") && r.bass < 0, "A C E = Am");
    int k2[] = { 57, 60, 65 }; recognise(k2, 3, &r);
    expect(r.root == 5 && r.type == 0 && r.bass == 9, "A C F = F/A");
    int k3[] = { 55, 59, 62, 65 }; recognise(k3, 4, &r);
    expect(r.root == 7 && !strcmp(CT[r.type].name, "7"), "G B D F = G7");
    int k4[] = { 62 }; recognise(k4, 1, &r);
    expect(r.root == 2 && r.type == 0, "one key = major");
}
#endif
