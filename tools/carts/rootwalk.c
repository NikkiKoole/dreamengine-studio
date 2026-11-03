/* de:meta
{
  "slug": "rootwalk",
  "title": "Root Walk",
  "status": "active",
  "created": "2026-11-03",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "song-arrangement",
    "step-sequencer",
    "functional-harmony",
    "swing-timing"
  ],
  "lineage": "The auto-accompaniment jam from Playtime (vector-sketch experiments/playtime/src/music-jam.lua, DRUM-JAM.md J1 + J7), brought home as GitHub #48. The bass figure idea is onenote's (after Aaron Batzdorff's one-note synth bass trick, TikTok / YouTube @aaronbatzdorff), with the root now walking on its own like an organ's one-finger chord. Drums from runtime/drumpat.h (the groovebook cart is its reader) voiced through drumkit.h with groovebook's role map; the upright is the upright cart's pizzicato INSTR_BOWED; the 303 uses instrument_mono. Every note is booked ahead on the audio clock (schedule_note_at).",
  "description": {
    "summary": "A few-note bass figure transposed along a walking root, over a drum machine that arranges itself live. The root moving is the hook.",
    "detail": "The core is small: a 1-3 note bass FIGURE written in scale degrees (R root, 8 octave, 7 flat seventh, A the approach a half step under the next root, N the push: the next root early on the and-of-4), transposed along a WALK of roots (1 4 5 1, 1 5 6 4, doo-wop 1 6 4 5, 2 5 1, round the fourths) that moves every 2 or 4 bars on its own. The track at the top shows the walk with a playhead; the next chord lights up when the bass leans into it. Underneath, a drum machine arranges itself in 8-bar phrases from the ENERGY knob: layers come in one at a time (kick, bass, keys, hats, snare, percussion), every fourth phrase at full tilt is a BREAKDOWN (the kick leaves, the bass never does), and the phrase after it is the DROP, everything back with a crash and the walk home. The KEYS layer plays the chords five ways (a soft Rhodes pad, disco saw stabs, clav funk stabs, a choked reggae skank, a strummed clean electric), each chord voiced close to the last one. Every phrase ends on a fill, the pattern family's own written Break where the library has one. The walks carry their CHORDS (major, minor, dominant seventh), and the riffs follow them: a riff is written in the minor pentatonic and moves onto each chord's own pentatonic, so its minor third becomes a 2 over a major chord. The plain figures stay put. Picks for the walk, figure, pace, key and voice are armed and land at the NEXT ROOT CHANGE, the way an organist changes register between chords; a new genre, family or chord style lands at the next bar. Patterns are the maker's own drum-patterns library, whose provenance is uneven (see drumpat.h): fine to play, not to call the real thing.",
    "controls": "W walk, F figure, P pace, K key, V bass voice: each ARMS a change that lands at the next root change. G genre, R family (rhythm) and C chord style land at the next bar; a picked chord style plays even when the energy is low. L (or tap the layer lamps) = ALL LAYERS from the next bar, whatever the energy. E LEAN and X GAP (AUTO / ALWAYS / NEVER): whether the drums nod at a coming chord change, and whether the bar before a drop goes silent from beat 4. O SPOT: a groove phrase's last 4 bars as a drum break, a bass feature or bass and drums trading (AUTO / NEVER / one of them every phrase). Y VARY: turnaround bars, ghost snares and 16th hats as it gets busy. UP/DOWN or the knob set ENERGY; hold SPACE (or PUSH) to push it to the top, release and it falls back. Tap the buttons on a phone."
  }
}
de:meta */
#include "studio.h"
#include "drumpat.h"
#include "drumkit.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

// ROOT WALK: the auto-accompaniment jam (GitHub #48, from Playtime's music-jam.lua).
//
// The honest core is jam_plan(): one call plans ONE BAR, purely from the Jam state and the
// energy level, and fills a Bar with the root, the drums' arrangement and the bass's 16
// steps. Nothing in it touches audio or the screen, which is what lets spec() check the
// music (the walk, the approach note, a pick landing at the next change) without a sound
// card. update() books each planned bar ahead on the audio clock; draw() reads the bar that
// is SOUNDING, not the one just planned (which is up to a quarter second in the future).
//
// One rule makes the approach note honest: the CHANGE is decided one bar early. When a bar
// is the last on its root, the planner snapshots what the next bar will be (the walk step,
// any armed pick, a drop that sends the walk home) so the A / N notes lean into the root
// that actually comes. A pick armed after that snapshot waits for the change after.

// ── the arrangement constants (Playtime's jam style numbers) ─────────────────
#define PHRASE       8     // bars per phrase: layers move, fills land, on this grid
#define BREAK_EVERY  4     // every 4th phrase at full energy is a breakdown
#define VAR_EVERY    2     // the drum variant moves on every 2nd phrase
#define MINL         2     // the jam never has fewer than kick + bass
#define AHEAD        0.25  // book a bar when it is this close to the audio clock

enum { L_KICK, L_BASS, L_KEYS, L_HATS, L_SNARE, L_PERC, NLAYER };   // the order they come in
static const char *LAYER_NAME[NLAYER] = { "KICK", "BASS", "KEYS", "HATS", "SNARE", "PERC" };
enum { M_GROOVE, M_BREAK, M_DROP };
static const char *MODE_NAME[3] = { "GROOVE", "BREAKDOWN", "DROP" };

// a 14th drum role only the hand-written Brushes use: the brush SWEEP
#define R_SW DP_NROLES
#define NR   (DP_NROLES + 1)
static int layer_of(int r) {
    switch (r) {
    case DP_BD: return L_KICK;
    case DP_CH: case DP_OH: case R_SW: return L_HATS;
    case DP_SD: case DP_CPS: return L_SNARE;
    case DP_AC: return -1;
    default: return L_PERC;     // RS CB TB LT MT HT CY
    }
}

// ── patterns: families of variants, + the family's own fill where it has one ─
typedef struct { unsigned short hit[NR], flam[NR]; } Pat;
#define MAXV 4
typedef struct { const char *name; Pat v[MAXV]; int nv; Pat fill; int has_fill; } Family;

enum { G_ELECTRO, G_HIPHOP, G_BREAKS, G_DISCO, G_FUNK, G_BRUSHES, NGENRE };
#define MAXF 6
typedef struct {
    const char *name; int bpm; float swing; int kit; int vel;   // vel: a genre-wide drum velocity offset
    const char *loop_b;                                         // the genre's second bass figure (AUTO)
    Family fam[MAXF]; int nfam;
} Genre;
static Genre GEN[NGENRE] = {
    { "ELECTRO", 122, 0.50f, 0,  0, "R.............N." },
    { "HIP HOP",  90, 0.56f, 0,  0, "R.............N." },
    { "BREAKS",  102, 0.52f, 1,  0, "R.............N." },
    { "DISCO",   120, 0.50f, 0,  0, "R.....8.R.....8." },
    { "FUNK",    100, 0.54f, 1,  0, "R.............N." },
    { "BRUSHES",  90, 0.62f, 1, -2, "R-----........A." },
};

// Where each family lives in drumpat.h. A NULL group = match the name prefix anywhere (the
// "NAME - A/B/C/D" families); a named group = the book's Measure A / Measure B / Break.
typedef struct { int genre; const char *label, *group, *prefix; } FamSrc;
static const FamSrc FAMSRC[] = {
    { G_ELECTRO, "ELECTRO 1", NULL, "ELECTRO 1 - " }, { G_ELECTRO, "ELECTRO 3", NULL, "ELECTRO 3 - " },
    { G_HIPHOP, "HIP HOP 1", NULL, "HIP HOP 1 - " },  { G_HIPHOP, "HIP HOP 2", NULL, "HIP HOP 2 - " },
    { G_HIPHOP, "HIP HOP 3", NULL, "HIP HOP 3 - " },  { G_HIPHOP, "HIP HOP 4", NULL, "HIP HOP 4 - " },
    { G_BREAKS, "AMEN", NULL, "AMEN BREAK - " },      { G_BREAKS, "CISSY STRUT", NULL, "CISSY STRUT (B-SECTION) - " },
    { G_BREAKS, "HOOK + SLING", NULL, "HOOK AND SLING - " }, { G_BREAKS, "KISSING MY LOVE", NULL, "KISSING MY LOVE - " },
    { G_DISCO, "DISCO 1", "Disco 1", "" }, { G_DISCO, "DISCO 2", "Disco 2", "" }, { G_DISCO, "DISCO 3", "Disco 3", "" },
    { G_DISCO, "DISCO 4", "Disco 4", "" }, { G_DISCO, "DISCO 5", "Disco 5", "" },
    { G_FUNK, "FUNK 1", "Funk 1", "" },  { G_FUNK, "FUNK 3", "Funk 3", "" },  { G_FUNK, "FUNK 6", "Funk 6", "" },
    { G_FUNK, "FUNK 7", "Funk 7", "" },  { G_FUNK, "FUNK 11", "Funk 11", "" }, { G_FUNK, "FUNK 12", "Funk 12", "" },
};

static unsigned short bits(const char *s) {
    unsigned short b = 0;
    for (int i = 0; i < 16 && s[i]; i++) if (s[i] == 'x' || s[i] == 'f') b |= (unsigned short)(1u << i);
    return b;
}
static void from_lib(Pat *p, int i) {
    memset(p, 0, sizeof *p);
    for (int r = 0; r < DP_NROLES; r++) { p->hit[r] = DP_PAT[i].hit[r]; p->flam[r] = DP_PAT[i].flam[r]; }
}
static int same_pat(const Pat *a, const Pat *b) { return memcmp(a->hit, b->hit, sizeof a->hit) == 0; }

static void resolve_patterns(void) {
    for (int k = 0; k < (int)(sizeof FAMSRC / sizeof FAMSRC[0]); k++) {
        const FamSrc *src = &FAMSRC[k];
        Genre *g = &GEN[src->genre];
        if (g->nfam >= MAXF) continue;
        Family *f = &g->fam[g->nfam];
        memset(f, 0, sizeof *f);
        f->name = src->label;
        int lo = 0, hi = DP_NPAT;
        if (src->group) {
            for (int gi = 0; gi < DP_NGROUP; gi++)
                if (strcmp(DP_GROUP[gi].name, src->group) == 0) { lo = DP_GROUP[gi].first; hi = lo + DP_GROUP[gi].count; break; }
        }
        size_t pl = strlen(src->prefix);
        for (int i = lo; i < hi; i++) {
            const char *n = DP_PAT[i].name;
            if (strncmp(n, src->prefix, pl) != 0) continue;
            if (strcmp(n + pl, "Break") == 0) { from_lib(&f->fill, i); f->has_fill = 1; continue; }
            Pat p; from_lib(&p, i);
            int dup = 0;
            for (int v = 0; v < f->nv; v++) if (same_pat(&f->v[v], &p)) dup = 1;   // the library repeats ELECTRO 3 - B
            if (!dup && f->nv < MAXV) f->v[f->nv++] = p;
        }
        if (f->nv > 0) g->nfam++;
    }
    // BRUSHES: the library has no brush kit, so these are hand-written (as in Playtime):
    // a soft kick on 1 and 3, a sweep, the brush's tap and the hi-hat foot on 2 and 4.
    Genre *b = &GEN[G_BRUSHES];
    Family *f = &b->fam[0];
    memset(f, 0, sizeof *f);
    f->name = "BRUSHES";
    f->v[0].hit[DP_BD] = bits("x.......x......."); f->v[0].hit[DP_SD] = bits("....x.......x...");
    f->v[0].hit[R_SW]  = bits("x.......x......."); f->v[0].hit[DP_CH] = bits("....x.......x...");
    f->v[1].hit[DP_BD] = bits("x.......x.....x."); f->v[1].hit[DP_SD] = bits("....x.......x.x.");
    f->v[1].hit[R_SW]  = bits("x..............."); f->v[1].hit[DP_CH] = bits("....x.......x...");
    f->nv = 2;
    b->nfam = 1;
}

// ── the walks, the figures, the paces ─────────────────────────────────────────
// kinds: the CHORD on each step, one char per step (Playtime's J9): M major, m minor, 7 dominant
typedef struct { const char *name; int n; int deg[12]; const char *kinds; } Walk;
static const Walk WALKS[] = {
    { "1 4 5 1",   4, { 0, 5, 7, 0 },  "MM7M" },
    { "1 5 6 4",   4, { 0, 7, 9, 5 },  "MMmM" },
    { "1 6 4 5",   4, { 0, 9, 5, 7 },  "MmM7" },                  // doo-wop
    { "2 5 1",     4, { 2, 7, 0, 0 },  "m7MM" },                  // the jazz arrival (the 1 holds two steps)
    { "1 4 1 5",   4, { 0, 5, 0, 7 },  "MMM7" },
    { "1 b7 1 4",  4, { 0, 10, 0, 5 }, "MMMM" },
    { "FOURTHS",   8, { 0, 5, 10, 3, 8, 1, 6, 0 }, "77777777" }, // round the fourths
    // WAVE (Playtime's 'wave' style, f1fd11bd, after Peter Martin's Open Studio "Ride the Wave"):
    // the root up a fourth twelve times and home, maj9 (J) and 9sus (s) taking turns, so each 9sus
    // is the next maj9's V. WAVE B swaps them; A then B puts a maj9 on all twelve roots.
    { "WAVE",     12, { 0, 5, 10, 3, 8, 1, 6, 11, 4, 9, 2, 7 }, "JsJsJsJsJsJs" },
    { "WAVE B",   12, { 0, 5, 10, 3, 8, 1, 6, 11, 4, 9, 2, 7 }, "sJsJsJsJsJsJ" },
};
// A riff is written in the MINOR pentatonic; over another chord its letters move by POSITION to
// that chord's pentatonic: a letter on minor-pentatonic slot k (m 4 5 7 = slots 1..4) plays the
// chord's slot k instead. R 8 L A N and every other letter stay put. Over M: R m 4 5 7 -> 0 2 4 7 9.
static const int MINOR_PENTA[5] = { 0, 3, 5, 7, 10 };
static const int *chord_shape(char kind) {
    static const int M[5] = { 0, 2, 4, 7, 9 }, D7[5] = { 0, 2, 4, 7, 10 };
    static const int SUS9[5] = { 0, 2, 5, 7, 10 };   // no third: the 4 in its place, the b7, the 9
    // maj9 (J) borrows M's shape: {0,2,4,7,11} is closer to the chord, but a short bass note on the
    // major 7th sounds sour against the root (9c's call)
    return kind == 'M' || kind == 'J' ? M : kind == '7' ? D7 : kind == 's' ? SUS9 : MINOR_PENTA;
}
static int follow_chord(int semis, char kind) {
    const int *sh = chord_shape(kind);
    for (int k = 1; k < 5; k++) if (semis == MINOR_PENTA[k]) return sh[k];
    return semis;
}
#define NWALK (int)(sizeof WALKS / sizeof WALKS[0])

// a figure is 16ths of degrees over the current root: R root, 2, m minor third, 3, 4, 5, 6,
// 7 flat seventh, 8 octave, L the fifth below, A the approach (a half step under the NEXT
// root), N the push (the next root itself, early); '-' holds, '.' rests. FEW notes on
// purpose: the root moving is the point, a busy riff buries it.
//
// A RIFF (Playtime's src/music-riffs.lua owns this format; this is a port, keep the two in step):
// a phrase + variation overlays + a FORM. Unlike a plain figure it restarts every bar and a '-'
// hold stops at the bar line. A variation is 16 chars laid over the phrase: a space keeps the
// phrase's step, anything else replaces it ('.' included, so a variation can remove a note).
// 'A' is always the plain phrase. The form is lined up with the END of the root's step: with
// barsLeft bars left on this root (1 = the bar before the change), the letter is form[max(0,
// len - barsLeft)], so AAAB plays B into every change and a 2-bar step plays the tail AB.
// Scale: notes on the strong 16ths sit in the minor pentatonic; off-16ths and A/N may leave it.
typedef struct { char letter; const char *overlay; } Var;
typedef struct { const char *name; const char *loop; Var vars[2]; const char *form; } Figure;
static const Figure FIGS[] = {
    { "AUTO", NULL },                                 // the genre's own pair, moving with the drum variant
    { "ROOT 8VE b7", "R..8..7........." },            // the classic funk cell, the favourite
    { "THE PUSH",    "R.............N." },
    { "THE ONE",     "R..............." },
    { "OCTAVE POP",  "R.....8.R.....8." },
    { "APPROACH",    "R-----........A." },
    // the riffs (Playtime's music-riffs.lua, all pentatonic, all following the chord)
    { "POCKET",      "R..R..7.R.....5.", { { 'B', "            8.7A" } }, "AAAB" },
    { "CLIMB",       "R.....m.4.5.7...", { { 'B', "          8.7.5." }, { 'C', "          8.5.mA" } }, "ABAC" },
    { "CALL+ANSWER", "R..8..R.....5.7.", { { 'B', "        7.5.4.mA" } }, "AAAB" },
    { "LOW + HIGH",  "R...L...R..m4...", { { 'B', "        R..m5.N." } }, "AAAB" },
    // eight more (Playtime 22e311da)
    { "DISCO OCTAVES",  "R.8.",             { { 'B', "            7.5A" } }, "AAAB" },
    { "ONE DROP",       "R--.....5-.m....", { { 'B', "        5-.7.8.A" } }, "AAAB" },
    { "MOTOWN",         "R...5...8...5.4.", { { 'B', "        7...5.mA" } }, "AAAB" },
    { "SIXTEENTH FUNK", "R.RR..7.R.m.4.5.", { { 'B', "        8.7.5.4A" } }, "AAAB" },
    { "HIP HOP LOW",    "R-------L-----..", { { 'B', "        L---m-A." } }, "AAAB" },
    { "LADDER",         "R.m.4.5.7.8.7.5.", { { 'B', "        8.7.5.mA" } }, "AAAB" },
    { "PUSH AND POP",   "R..8...R......N.", { { 0 } },                       "A" },
    { "BRUSH WALK",     "R---m---4---5---", { { 'B', "            7--A" } }, "AAAB" },   // the hold ends at the A
};
#define NFIG (int)(sizeof FIGS / sizeof FIGS[0])
static const char *GENRE_A = "R..8..7.........";     // every genre's first figure

static const int PACES[2] = { 2, 4 };

// ── the chord layer (Playtime's J9d: music-jam.lua KEYS_CHORDS + CHORD_FIGURES) ─
// The chord per kind keeps out of the bass's way: the major one has the 9th, not the major 7th
// (the figures play a b7 over every chord); the minor and the dominant their b7.
static const int *keys_chord(char kind, int *n) {
    static const int M[4] = { 0, 4, 7, 14 }, m[4] = { 0, 3, 7, 10 }, D7[4] = { 0, 4, 7, 10 },
                     J[4] = { 0, 4, 11, 14 }, S[4] = { 0, 5, 10, 14 };
    *n = 4;
    return kind == 'm' ? m : kind == '7' ? D7 : kind == 'J' ? J : kind == 's' ? S : M;
}
// how the chords are played: 16 steps, 'x' a hit, '-' holding it, '.' silence; each on its own
// slot, voiced in lo..hi, rolled `roll` seconds a note. AUTO = the genre's (CHORDS_BY_GENRE).
typedef struct { const char *name; const char *steps; int vel, lo, hi; float roll; } ChordFig;
static const ChordFig CHORD_FIGS[] = {
    { "AUTO",  NULL, 0, 0, 0, 0 },
    { "PAD",   "x---------------", 2, 52, 74, 0.012f },   // a soft Rhodes on the one
    { "DISCO", "..x-..x-..x-..x-", 2, 55, 76, 0.0f },     // saw stabs on the offbeat
    { "FUNK",  "....x..x.x....x.", 3, 55, 76, 0.004f },   // clav stabs
    { "SKANK", "....x-......x-..", 1, 60, 79, 0.008f },   // a choked guitar on 2 and 4
    { "STRUM", "x--x--x-x--x--x-", 1, 52, 76, 0.022f },   // a clean electric, strummed
};
#define NCHORDFIG (int)(sizeof CHORD_FIGS / sizeof CHORD_FIGS[0])
enum { CF_AUTO, CF_PAD, CF_DISCO, CF_FUNK, CF_SKANK, CF_STRUM };
// per genre: ELECTRO HIP HOP BREAKS DISCO FUNK BRUSHES
static const int CHORDS_BY_GENRE[6] = { CF_PAD, CF_PAD, CF_FUNK, CF_DISCO, CF_FUNK, CF_PAD };

// Voice a chord close to the last one (Playtime's theory.voiceChord): every rotation of the chord's
// pitch classes, closed and drop-2, at every octave whose bottom sits in lo..lo+12 and top under hi;
// no minor 2nd / b9 anywhere, no close interval down low; the cheapest move from the previous
// voicing wins (a big leap of the top voice costs extra), else the one nearest middle D.
#define MAXVOICE 4
static int voice_chord(int root_pc, const int *iv, int niv, int lo, int hi,
                       const int *prev, int nprev, int *out) {
    int pcs[MAXVOICE], n = 0;
    for (int i = 0; i < niv; i++) {
        int pc = (root_pc + iv[i]) % 12, dup = 0;
        for (int k = 0; k < n; k++) if (pcs[k] == pc) dup = 1;
        if (!dup) pcs[n++] = pc;
    }
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (pcs[b] < pcs[a]) { int t = pcs[a]; pcs[a] = pcs[b]; pcs[b] = t; }
    float best_cost = 1e9f; int found = 0;
    for (int rot = 0; rot < n; rot++) for (int drop2 = 0; drop2 <= (n >= 3 ? 1 : 0); drop2++) {
        int shape[MAXVOICE];
        for (int k = 0; k < n; k++) {
            int note = pcs[(rot + k) % n];
            while (k > 0 && note <= shape[k - 1]) note += 12;
            shape[k] = note;
        }
        if (drop2) {
            shape[n - 2] -= 12;
            for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (shape[b] < shape[a]) { int t = shape[a]; shape[a] = shape[b]; shape[b] = t; }
        }
        for (int oct = 2; oct <= 7; oct++) {
            int v[MAXVOICE];
            for (int k = 0; k < n; k++) v[k] = shape[k] + 12 * oct;
            if (v[0] < lo || v[0] > lo + 12 || v[n - 1] > hi) continue;
            int bad = 0;
            for (int a = 0; a < n; a++) {
                for (int b = a + 1; b < n; b++) { int d = v[b] - v[a]; if (d == 1 || d == 13 || d == 25) bad = 1; }
                if (a > 0 && v[a] - v[a - 1] < 3 && v[a - 1] < 52) bad = 1;
            }
            if (bad) continue;
            float mean = 0; for (int k = 0; k < n; k++) mean += v[k];
            float d = mean / n - 62; float cost = 0.5f * (d < 0 ? -d : d);
            if (nprev > 0) {
                for (int k = 0; k < n; k++) { int m2 = prev[k < nprev ? k : nprev - 1]; int dd = v[k] - m2; cost += dd < 0 ? -dd : dd; }
                int top = v[n - 1] - prev[nprev - 1]; if (top < 0) top = -top;
                if (top > 4) cost += 2 * (top - 4);
            }
            if (cost < best_cost) { best_cost = cost; found = 1; for (int k = 0; k < n; k++) out[k] = v[k]; }
        }
    }
    return found ? n : 0;
}

static int degree_of(char c) {
    switch (c) {
    case 'R': return 0;  case '2': return 2;  case 'm': return 3;  case '3': return 4;
    case '4': return 5;  case '5': return 7;  case '6': return 9;  case '7': return 10;
    case '8': return 12; case 'L': return -5; case 'b': return 6;   // b = the blue b5
    default:  return -99;
    }
}
// a pitch class into A1..G#2 (midi 33..44): low enough to be a bass, high enough to hear
static int in_range(int pc) { return 33 + ((pc - 9) % 12 + 12) % 12; }

// ── the jam state + one planned bar ───────────────────────────────────────────
enum { V_UPRIGHT, V_ACID, V_SINE, NVOICE };
static const char *VOICE_NAME[NVOICE] = { "UPRIGHT", "303 MONO", "SINE" };

enum { P_AUTO, P_ALWAYS, P_NEVER };            // the LEAN / GAP / VARY picks (Playtime J10a-d)
static const char *PICK3[3] = { "AUTO", "ALWAYS", "NEVER" };
// SPOTLIGHTS (J10c): a groove phrase's last 4 bars hand the stage to one part
enum { SP_NONE, SP_DRUMS, SP_BASS, SP_TRADE, NSPOT };
static const char *SPOT_NAME[NSPOT] = { "", "DRUM BREAK", "BASS FEATURE", "TRADING" };
enum { SPK_AUTO, SPK_NEVER, SPK_DRUMS, SPK_BASS, SPK_TRADE, NSPK };   // the pick
static const char *SPK_NAME[NSPK] = { "AUTO", "NEVER", "DRUMS", "BASS", "TRADE" };
// the ghost snares' three masks (one per song)
static const char *GHOSTS[3] = { "......x....x....", ".......x..x.....", "...x......x....x" };
static unsigned mix32(unsigned x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
#define JAM_SEED 0x5EEDu
typedef struct { int walk, fig, pace, genre, fam, voice, key, chords, all, lean, gap, spot, vary; } Picks;   // all: every layer, whatever the energy   // key: pitch class of the 1

typedef struct {
    Picks cur, want;           // sounding vs armed
    int bar;                   // the index of the next bar to plan
    int phrase, mode, layers, var, on[NLAYER];
    int np_mode, np_layers, np_var, np_ready, np_spot, spot, ghosts;   // the NEXT phrase, decided one bar early
    int wstep, root, left;     // the walk: its step, the root (semitones over the key), bars left on it
    int voicing[MAXVOICE], nvoicing;            // the last chord voiced (the next one moves close to it)
    int ch_pending, ch_wstep; Picks ch;          // the next bar's change, snapshotted one bar early
} Jam;

typedef struct {
    int bar, inph, phrase, mode, layers, on[NLAYER];
    int genre, fam, var, fill, written, voice, fig, walk, wstep, pace, left;
    int root, next_root, moves;
    const Pat *pat;
    const char *loop;
    char letter;                                 // a riff's form letter this bar ('A' for a plain figure)
    int chords, keys_on, voicing[MAXVOICE], nvoicing;
    int cut, lean, run_from, toms, feature_fig;
    int spot, spot_on, ghosts, hats16, turn;     // spotlight + variations this bar               // the drums play the song: see jam_plan
   // the chord figure + this bar's voicing
    int key; char chord;                         // the key's pitch class, this bar's chord kind (M m 7)
    int note[16], len[16]; char kind[16];       // note -1 = nothing starts on this step
} Bar;

static int target_layers(float energy) {
    int n = MINL + (int)(energy * (NLAYER - MINL) + 0.001f);
    return n < MINL ? MINL : n > NLAYER ? NLAYER : n;
}

static void jam_init(Jam *j, Picks p) {
    memset(j, 0, sizeof *j);
    j->cur = j->want = p;
    j->wstep = 0;
    j->root = WALKS[p.walk].deg[0];
    j->left = PACES[p.pace];
    j->np_mode = M_GROOVE; j->np_layers = MINL; j->np_var = 0; j->np_ready = 1;
    j->np_spot = p.spot >= SPK_DRUMS ? p.spot - SPK_DRUMS + SP_DRUMS : SP_NONE;   // a named spotlight from phrase one
    j->phrase = -1;
    j->ghosts = (int)(mix32(JAM_SEED ^ 0xA5A5u) % 3);                  // one ghost mask per song
}

// Playtime's newPhrase, decided for the phrase AFTER the current one.
static void decide_next_phrase(Jam *j, float energy) {
    int want = j->cur.all ? NLAYER : target_layers(energy);
    int phrase = j->phrase + 1;
    int mode = j->mode, layers = j->layers, var = j->var;
    if (mode == M_BREAK && want >= 3)      { mode = M_DROP; if (want > layers) layers = want; }
    else if (mode == M_BREAK)              { mode = M_GROOVE; layers = want; }
    else if (layers >= 3 && want >= 3 && phrase % BREAK_EVERY == 0) mode = M_BREAK;
    else {
        mode = M_GROOVE;
        if (want > layers) layers++; else if (want < layers) layers--;
    }
    if (phrase % VAR_EVERY == 0) var++;
    int spot = SP_NONE;                                            // only a GROOVE phrase gets a spotlight
    if (mode == M_GROOVE && j->cur.spot >= SPK_DRUMS) spot = j->cur.spot - SPK_DRUMS + SP_DRUMS;
    else if (mode == M_GROOVE && j->cur.spot == SPK_AUTO && layers >= 4 && phrase % BREAK_EVERY == BREAK_EVERY / 2) {
        unsigned r = mix32(JAM_SEED + (unsigned)phrase * 104729u);
        if (r % 2 == 0) spot = SP_DRUMS + (int)((r >> 8) % 3);
    }
    j->np_mode = mode; j->np_layers = layers; j->np_var = var; j->np_spot = spot; j->np_ready = 1;
}

static const Genre *genre_of(const Picks *p) { return &GEN[p->genre]; }
static const Family *family_of(const Picks *p) {
    const Genre *g = genre_of(p);
    return &g->fam[p->fam % g->nfam];
}

// Plan ONE bar. Pure: reads energy, writes the Jam forward and the Bar out.
static void jam_plan(Jam *j, float energy, Bar *o) {
    memset(o, 0, sizeof *o);
    int b = j->bar;
    int inph = b % PHRASE;

    // a new phrase: apply what was decided a bar ago
    if (inph == 0) {
        j->phrase++;
        if (!j->np_ready) decide_next_phrase(j, energy);
        j->mode = j->np_mode; j->layers = j->np_layers; j->var = j->np_var; j->spot = j->np_spot; j->np_ready = 0;
        for (int k = 0; k < NLAYER; k++) j->on[k] = k < j->layers;
        if (j->mode == M_BREAK) for (int k = 0; k < NLAYER; k++) j->on[k] = k != L_KICK;  // the floor drops out
    }

    // genre + family land at the next BAR (a new groove shouldn't wait for the chord)
    j->cur.genre = j->want.genre; j->cur.fam = j->want.fam; j->cur.chords = j->want.chords;   // these: next bar
    j->cur.all = j->want.all; j->cur.lean = j->want.lean; j->cur.gap = j->want.gap;
    j->cur.spot = j->want.spot; j->cur.vary = j->want.vary;
    // ALL LAYERS (Playtime's 'jam layers: all'): everything from the next bar; breakdowns still happen
    if (j->cur.all && j->mode != M_BREAK) { j->layers = NLAYER; for (int k = 0; k < NLAYER; k++) j->on[k] = 1; }

    // the root: a change snapshotted last bar lands now, else one bar less on this root
    if (j->ch_pending) {
        int genre = j->cur.genre, fam = j->cur.fam, chords = j->cur.chords, all = j->cur.all, lean = j->cur.lean, gap = j->cur.gap, spot = j->cur.spot, vary = j->cur.vary;
        j->cur = j->ch;
        j->cur.genre = genre; j->cur.fam = fam; j->cur.chords = chords; j->cur.all = all; j->cur.lean = lean; j->cur.gap = gap; j->cur.spot = spot; j->cur.vary = vary;
        j->wstep = j->ch_wstep;
        j->root = WALKS[j->cur.walk].deg[j->wstep];
        j->left = PACES[j->cur.pace];
        j->ch_pending = 0;
    } else if (b > 0) j->left--;

    // decide what the NEXT bar is, now, so this bar's bass can lean into it
    int last_in_phrase = inph == PHRASE - 1;
    if (last_in_phrase) decide_next_phrase(j, energy);
    int drop_next = last_in_phrase && j->np_mode == M_DROP;
    int moves = drop_next || j->left <= 1;
    int next_root = j->root;
    if (moves) {
        Picks c = j->want;                                         // armed picks ride this change
        int nstep;
        if (drop_next || c.walk != j->cur.walk) nstep = 0;         // a new walk / the drop starts from the top
        else nstep = (j->wstep + 1) % WALKS[c.walk].n;
        j->ch = c; j->ch_wstep = nstep; j->ch_pending = 1;
        next_root = WALKS[c.walk].deg[nstep];
    }

    const Genre *g = genre_of(&j->cur);
    const Family *f = family_of(&j->cur);
    o->bar = b; o->inph = inph; o->phrase = j->phrase; o->mode = j->mode; o->layers = j->layers;
    memcpy(o->on, j->on, sizeof o->on);
    o->spot = j->spot; o->spot_on = j->spot && inph >= PHRASE - 4;   // a spotlight acts in the last 4 bars
    if (o->spot_on) {
        if (o->spot == SP_DRUMS) o->on[L_BASS] = o->on[L_KEYS] = 0;
        if (o->spot == SP_BASS)  o->on[L_KEYS] = 0;
        if (o->spot == SP_TRADE) {
            o->on[L_KEYS] = 0;
            if (inph < PHRASE - 2) o->on[L_KICK] = o->on[L_HATS] = o->on[L_SNARE] = o->on[L_PERC] = 0;   // bass alone
            else o->on[L_BASS] = 0;                                                                         // drums alone
        }
    }
    o->genre = j->cur.genre; o->fam = j->cur.fam % g->nfam; o->var = j->var % f->nv;
    o->voice = j->cur.voice; o->fig = j->cur.fig; o->walk = j->cur.walk; o->wstep = j->wstep;
    o->pace = PACES[j->cur.pace]; o->left = j->left;
    o->root = j->root; o->next_root = next_root; o->moves = moves;
    o->fill = last_in_phrase && (o->on[L_SNARE] || j->mode == M_BREAK);

    // THE DRUMS PLAY THE SONG (Playtime J10a-b). One deterministic roll per bar.
    unsigned rs = (unsigned)b * 7919u + 0x9E3779B9u;
    rs ^= rs >> 16; rs *= 0x7feb352du; rs ^= rs >> 15; rs *= 0x846ca68bu; rs ^= rs >> 16;
    #define ROLL(n) ((rs = rs * 1664525u + 1013904223u), (int)((rs >> 8) % (unsigned)(n)))
    // the GAP: the bar before a drop goes silent from step 12, so the one lands in a hole
    int gap = last_in_phrase && j->cur.gap != P_NEVER && (j->cur.gap == P_ALWAYS || drop_next);
    o->cut = gap ? 12 : 16;
    // the written fill plays two phrase ends in three; the build before a drop is always the jam's own
    int build = gap && j->mode == M_BREAK;
    o->written = o->fill && !build && ROLL(3) != 0 && f->has_fill;
    // the LEAN: a bar leading into a real root change (not a phrase end) nods at it
    o->lean = 0;
    if (!last_in_phrase && moves && next_root != j->root && j->cur.lean != P_NEVER) {
        int r10 = ROLL(10);
        if (j->cur.lean == P_ALWAYS || r10 < 6) {
            int kinds[3], nk = 0;
            if (o->on[L_HATS]) kinds[nk++] = 1;    // an open hat on 14, accented
            if (o->on[L_KICK]) kinds[nk++] = 2;    // a kick on 15
            if (o->on[L_SNARE]) kinds[nk++] = 3;   // a snare run 12-15
            if (nk) o->lean = kinds[ROLL(nk)];
        }
    }
    // FILLS (the unwritten snare run): the whole bar when the gap closes a breakdown (the BUILD),
    // from the middle at high energy, else the last beat; toms in Funk / Breaks one fill in three
    int want_now = j->cur.all ? NLAYER : target_layers(energy);
    o->run_from = build ? 0 : want_now >= MINL + 3 ? 8 : 12;
    o->toms = o->fill && !o->written && !gap && (j->cur.genre == G_FUNK || j->cur.genre == G_BREAKS)
              && ROLL(3) == 0;
    #undef ROLL
    // VARIATIONS (J10d), as play gets busy: a turnaround bar, ghost snares, 16th hats
    int busy = want_now >= MINL + 2, always = j->cur.vary == P_ALWAYS, never = j->cur.vary == P_NEVER;
    o->turn = !never && (always || busy) && f->nv > 1 && inph == PHRASE - 2;
    o->ghosts = !never && (always || busy) ? j->ghosts + 1 : 0;     // 1..3 = which mask
    o->hats16 = !never && j->cur.genre != G_BRUSHES && (always || want_now >= MINL + 3);
    o->pat = o->written ? &f->fill : &f->v[o->turn ? (o->var + 1) % f->nv : o->var];

    // the bass: the figure over the root, counted from the song's first bar so a figure
    // that isn't 16 steps long keeps its place across bar lines
    const Figure *fg = &FIGS[j->cur.fig];
    int feature = o->spot_on && o->spot == SP_BASS;
    if (feature && !fg->form) {                                    // the BASS FEATURE plays a riff
        int riffs[32], nr = 0;
        for (int k = 0; k < NFIG && nr < 32; k++) if (FIGS[k].form) riffs[nr++] = k;
        fg = &FIGS[riffs[mix32(JAM_SEED + (unsigned)j->phrase * 7u) % (unsigned)nr]];
    }
    o->feature_fig = feature ? (int)(fg - FIGS) : -1;
    const char *loop = fg->loop;
    if (!loop) loop = (o->var % 2 == 0) ? GENRE_A : g->loop_b;
    o->loop = loop;
    int n = (int)strlen(loop);
    char row[16];                                                  // this bar's 16 steps
    o->letter = 'A';
    if (fg->form) {
        int fl = (int)strlen(fg->form);
        int bars_left = moves ? 1 : j->left;                       // a drop next makes this the last bar too
        int li = fl - bars_left; if (li < 0) li = 0;
        o->letter = fg->form[li];
        if (feature) {                                             // A, v1, A, v2 over the feature's 4 bars
            char v1 = fg->vars[0].letter ? fg->vars[0].letter : 'A';
            char v2 = fg->vars[1].letter ? fg->vars[1].letter : v1;
            if (v1 > v2 && fg->vars[1].letter) { char t = v1; v1 = v2; v2 = t; }   // vars sorted
            int k = inph - (PHRASE - 4);
            o->letter = k == 0 || k == 2 ? 'A' : k == 1 ? v1 : v2;
        }
        const char *ov = NULL;
        for (int v = 0; v < 2; v++) if (fg->vars[v].letter == o->letter) ov = fg->vars[v].overlay;
        for (int s = 0; s < 16; s++) {
            row[s] = loop[s % n];                                  // a riff restarts every bar
            if (ov && s < (int)strlen(ov) && ov[s] != ' ') row[s] = ov[s];
        }
    } else {
        for (int s = 0; s < 16; s++) row[s] = loop[(b * 16 + s) % n];   // a figure keeps its place across bars
    }
    o->key = j->cur.key;
    o->chord = WALKS[j->cur.walk].kinds[j->wstep];
    int home = in_range(j->cur.key + j->root);
    // the next root sounds in the key that will be sounding then (a key pick rides the change)
    int approach = in_range((moves ? j->ch.key : j->cur.key) + next_root) - 1;               // right under the note it leads into
    for (int s = 0; s < 16; s++) {
        o->note[s] = -1;
        char c = row[s];
        o->kind[s] = c;
        if (!o->on[L_BASS]) continue;
        int midi;
        if (c == 'A') midi = approach;
        else if (c == 'N') midi = approach + 1;
        else {
            int d = degree_of(c); if (d == -99) continue;
            if (fg->form) d = follow_chord(d, o->chord);            // riffs follow the chord, figures don't...
            else if (c == '7' && o->chord == 'J') d = 11;             // ...except a figure's 7 over maj9: the major 7th
                                                                      // (Playtime's FIGURE_DEGREES), else it rubs
            midi = home + d;
        }
        int len = 1;
        if (fg->form) { while (s + len < 16 && row[s + len] == '-') len++; }              // stops at the bar line
        else { while (len < n && loop[(b * 16 + s + len) % n] == '-') len++; }
        if (s >= o->cut) continue;                                 // the gap
        if (s + len > o->cut) len = o->cut - s;                    // a hold is clipped at it
        o->note[s] = midi; o->len[s] = len;
    }

    // the chords: a layer of their own, or always when a style is PICKED (else a calm jam never hears it)
    o->chords = j->cur.chords ? j->cur.chords : CHORDS_BY_GENRE[j->cur.genre];
    o->keys_on = (o->on[L_KEYS] || j->cur.chords != CF_AUTO)
              && !(o->spot_on && o->spot != SP_NONE);              // every spotlight sends the keys out
    if (o->keys_on) {
        int niv; const int *iv = keys_chord(o->chord, &niv);
        const ChordFig *cf = &CHORD_FIGS[o->chords];
        int v[MAXVOICE];
        int nv = voice_chord((j->cur.key + j->root) % 12, iv, niv, cf->lo, cf->hi, j->voicing, j->nvoicing, v);
        if (nv) { memcpy(j->voicing, v, sizeof v); j->nvoicing = nv; }
    }
    memcpy(o->voicing, j->voicing, sizeof o->voicing); o->nvoicing = j->nvoicing;
    j->bar++;
}

// ── sound ─────────────────────────────────────────────────────────────────────
#define SL_UPRIGHT 10
#define SL_ACID    11
#define SL_SINE    12
#define SL_KEYS    13                       // 13..17: one slot per chord figure
#define KIT_BASE_E 20                       // ELECTRO kit, 8 slots
#define KIT_BASE_A 28                       // ACOUSTIC kit, 8 slots
#define SL_SWEEP   36
static const int VOICE_SLOT[NVOICE] = { SL_UPRIGHT, SL_ACID, SL_SINE };

// the library's 12 roles onto drumkit.h's 8 (groovebook's map, kept identical so the
// two carts voice a pattern the same way)
typedef struct { int role, midi, vbump; } RoleMap;
static const RoleMap RMAP[DP_NROLES] = {
    [DP_BD]  = { DK_KICK,   36, 3 }, [DP_SD]  = { DK_SNARE,  38, 3 },
    [DP_LT]  = { DK_TOM_LO, 41, 2 }, [DP_MT]  = { DK_TOM_LO, 48, 2 }, [DP_HT] = { DK_TOM_HI, 55, 2 },
    [DP_CH]  = { DK_HHC,    42, 0 }, [DP_OH]  = { DK_HHO,    46, 0 }, [DP_CY] = { DK_CRASH,  49, 0 },
    [DP_RS]  = { DK_SNARE,  50, 3 }, [DP_CB]  = { DK_TOM_HI, 62, 2 },
    [DP_CPS] = { DK_CLAP,   39, 3 }, [DP_TB]  = { DK_HHC,    54, 0 },
    [DP_AC]  = { -1, 0, 0 },
};
// groovebook's measured per-kit trims (a dense pattern hit 0 dBFS untrimmed)
static const float KIT_LVL[2][DK_N] = {
    { [DK_KICK] = 1.00f, [DK_SNARE] = 1.00f, [DK_HHC] = 0.050f, [DK_HHO] = 0.060f,
      [DK_CLAP] = 1.00f, [DK_TOM_LO] = 0.55f, [DK_TOM_HI] = 0.55f, [DK_CRASH] = 0.200f },
    { [DK_KICK] = 1.00f, [DK_SNARE] = 0.575f, [DK_HHC] = 0.023f, [DK_HHO] = 0.028f,
      [DK_CLAP] = 0.50f, [DK_TOM_LO] = 0.70f, [DK_TOM_HI] = 0.70f, [DK_CRASH] = 0.045f },
};
static const int KIT_BASE[2] = { KIT_BASE_E, KIT_BASE_A };

static void setup_sound(void) {
    dk_use(&DK_ELECTRO, KIT_BASE_E);
    dk_use(&DK_ACOUSTIC, KIT_BASE_A);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < DK_N; i++) instrument_level(KIT_BASE[k] + i, KIT_LVL[k][i]);

    // the brush sweep: soft noise, a slow swell, darker than a hat
    instrument(SL_SWEEP, INSTR_NOISE, 180, 400, 0, 300);
    instrument_filter(SL_SWEEP, FILTER_BAND, 3200, 2);
    instrument_level(SL_SWEEP, 0.05f);

    // the upright: the upright cart's pizzicato, the same string model a bow would draw
    instrument(SL_UPRIGHT, INSTR_BOWED, 4, 0, 7, 475);
    instrument_mode(SL_UPRIGHT, MODE_BOW_PIZZ, 1.0f);
    instrument_mode(SL_UPRIGHT, MODE_BOW_BODY, 0.85f);
    instrument_mode(SL_UPRIGHT, MODE_BOW_SIZE, BOW_SIZE_BASS);
    instrument_filter(SL_UPRIGHT, FILTER_LOW, 1500, 3);
    instrument_harmonics(SL_UPRIGHT, 0.30f);
    instrument_timbre(SL_UPRIGHT, 0.30f);
    instrument_morph(SL_UPRIGHT, 0.45f);
    instrument_level(SL_UPRIGHT, 0.45f);   // measured: at unity the pizz sat 7 dB over the acoustic kick

    // the 303: one voice (instrument_mono), a diode ladder that quacks on each note
    instrument(SL_ACID, INSTR_SAW, 0, 220, 3, 40);
    instrument_filter(SL_ACID, FILTER_DIODE, 380, 9);
    instrument_env(SL_ACID, 0, ENV_CUTOFF_OCT, 0, 170, 2.6f);
    instrument_mono(SL_ACID, 1);
    instrument_level(SL_ACID, 0.55f);

    // the round sine bass
    instrument(SL_SINE, INSTR_SINE, 3, 300, 5, 90);
    instrument_level(SL_SINE, 0.2f);

    // the chord instruments (Playtime's synth.lua patches of the same names)
    int k = SL_KEYS + CF_PAD - 1;                                      // rhodes: a soft pad, panned + phased
    instrument(k, INSTR_EPIANO, 1, 0, 7, 450);
    instrument_harmonics(k, 0.15f); instrument_timbre(k, 0.2f); instrument_morph(k, 0.12f);
    instrument_reverb(k, 0.3f); instrument_level(k, 0.35f);
    instrument_autopan(k, 4.0f, 0.5f, 0);
    instrument_phaser(k, 0.8f, 0.65f, 0.5f, 0.5f, 4);
    instrument_tape(k, 0.25f, 0.1f, 0.0f);
    k = SL_KEYS + CF_DISCO - 1;                                        // stab: a filtered saw
    instrument(k, INSTR_SAW, 3, 180, 2, 160);
    instrument_filter(k, FILTER_LOW, 1500, 2);
    instrument_reverb(k, 0.35f); instrument_level(k, 0.38f);
    k = SL_KEYS + CF_FUNK - 1;                                         // clav: a bright, short e-piano
    instrument(k, INSTR_EPIANO, 2, 140, 2, 120);
    instrument_harmonics(k, 0.85f); instrument_timbre(k, 0.75f); instrument_morph(k, 0.55f);
    instrument_reverb(k, 0.2f); instrument_level(k, 1.0f);
    k = SL_KEYS + CF_SKANK - 1;                                        // jab guitar: choked, on a worn tape
    instrument(k, INSTR_GUITAR, 1, 0, 7, 90);
    instrument_harmonics(k, 0.4f); instrument_timbre(k, 0.35f); instrument_morph(k, 0.12f);
    instrument_mode(k, MODE_STRING_WEIGHT, 0.7f); instrument_mode(k, MODE_STRING_CLICK, 0.15f);
    instrument_filter(k, FILTER_LOW, 2800, 1);
    instrument_reverb(k, 0.3f); instrument_level(k, 0.55f);
    instrument_drive(k, 0.15f);
    instrument_tape(k, 0.3f, 0.12f, 0.35f);
    k = SL_KEYS + CF_STRUM - 1;                                        // clean electric, long ring, a flanger
    instrument(k, INSTR_GUITAR, 2, 0, 7, 1600);
    instrument_harmonics(k, 0.4f); instrument_timbre(k, 0.3f); instrument_morph(k, 0.12f);
    instrument_mode(k, MODE_STRING_WEIGHT, 0.7f); instrument_mode(k, MODE_STRING_CLICK, 0.15f);
    instrument_filter(k, FILTER_LOW, 2400, 1);
    instrument_reverb(k, 0.45f); instrument_level(k, 0.45f);
    instrument_drive(k, 0.22f);
    instrument_tape(k, 0.25f, 0.08f, 0.25f);
    instrument_flanger(k, 0.25f, 0.5f, 0.5f, 0.35f);       // measured: a sine carries 12 dB more rms than the pizz at the same level
}

static double step_dur(int genre) { return 60.0 / GEN[genre].bpm / 4.0; }
static double step_time(const Bar *o, double t0, int s) {
    double st = step_dur(o->genre);
    if (s & 1) return t0 + (s - 1) * st + 2.0 * st * GEN[o->genre].swing;
    return t0 + s * st;
}

static void drum(int kit, int role, double t, int vel) {
    if (vel < 1) vel = 1; if (vel > 7) vel = 7;
    if (role == R_SW) { schedule_at(t, 70, SL_SWEEP, vel, 500); return; }
    const RoleMap *m = &RMAP[role];
    if (m->role < 0) return;
    schedule_at(t, m->midi, KIT_BASE[kit] + m->role, vel, DK_ROLE_DUR[m->role]);
}

// The chord on step s of this bar, honouring the gap: returns its length in steps, 0 = none.
static int keys_at(const Bar *o, int s) {
    if (!o->keys_on || !o->nvoicing || s >= o->cut) return 0;
    const ChordFig *cf = &CHORD_FIGS[o->chords];
    if (cf->steps[s] != 'x') return 0;
    int len = 1;
    while (s + len < 16 && cf->steps[s + len] == '-') len++;
    if (s + len > o->cut) len = o->cut - s;
    return len;
}

// Every drum hit of a bar, in order: emit(role, step, seconds after the step, velocity). ONE place
// decides the drums, so booking and spec() see the same thing.
typedef void (*DrumEmit)(int role, int s, double dt, int vel, void *ctx);
static void bar_drums(const Bar *o, DrumEmit emit, void *ctx) {
    const Genre *g = &GEN[o->genre];
    const Pat *p = o->pat;
    int unwritten = o->fill && !o->written;
    for (int s = 0; s < o->cut; s++) {
        int acc = (p->hit[DP_AC] >> s) & 1;
        if (o->mode == M_DROP && o->inph == 0 && s == 0) emit(DP_CY, s, 0, 6, ctx);
        for (int r = 0; r < NR; r++) {
            if (r == DP_AC || !((p->hit[r] >> s) & 1)) continue;
            int L = layer_of(r);
            if (L < 0 || !o->on[L]) continue;
            if (unwritten && L == L_HATS && s >= 14) continue;            // let go before the one
            if (o->lean == 1 && r == DP_CH && s >= 14) continue;          // the open hat's room
            int vel = 3 + (r < DP_NROLES ? RMAP[r].vbump : 1) + acc * 2 + g->vel;
            if (o->mode == M_BREAK && L != L_HATS) vel--;
            emit(r, s, 0, vel, ctx);
            if ((p->flam[r] >> s) & 1) emit(r, s, 0.015, vel - 1, ctx);
        }
        // ghost snares where the pattern has none (not under a run-up)
        int running = unwritten && s >= o->run_from;
        if (o->ghosts && o->on[L_SNARE] && !running && GHOSTS[o->ghosts - 1][s] == 'x' && !((p->hit[DP_SD] >> s) & 1))
            emit(DP_SD, s, 0, 3 + RMAP[DP_SD].vbump - 3 + g->vel, ctx);
        // 16th hats: fill the odd step after a closed hat
        if (o->hats16 && o->on[L_HATS] && (s & 1) && ((p->hit[DP_CH] >> (s - 1)) & 1) && !((p->hit[DP_CH] >> s) & 1)
            && !(s >= 14 && (running || o->lean == 1)))
            emit(DP_CH, s, 0, 3 + RMAP[DP_CH].vbump - 1 + g->vel, ctx);
        // the lean
        if (o->lean == 1 && s == 14) emit(DP_OH, s, 0, 3 + RMAP[DP_OH].vbump + 2 + g->vel, ctx);
        if (o->lean == 2 && s == 15) emit(DP_BD, s, 0, 3 + RMAP[DP_BD].vbump - 1 + g->vel, ctx);
        if (o->lean == 3 && s >= 12) emit(DP_SD, s, 0, 6 - (s == 15 ? 1 : 2) + g->vel, ctx);
        // the fill
        if (unwritten && s >= o->run_from) {
            if (o->toms && s >= 12) {
                static const int TOM[4] = { DP_HT, DP_HT, DP_MT, DP_LT };
                emit(TOM[s - 12], s, 0, 3 + RMAP[TOM[s - 12]].vbump + 1 + g->vel, ctx);
            } else {
                int q = (int)((15 - s) * 3.0 / (16 - o->run_from) + 0.5);
                emit(DP_SD, s, 0, 6 - q + g->vel, ctx);
            }
        }
    }
}

typedef struct { const Bar *o; double t0; } BookCtx;
static void book_drum(int role, int s, double dt, int vel, void *ctx) {
    BookCtx *b = ctx;
    drum(GEN[b->o->genre].kit, role, step_time(b->o, b->t0, s) + dt, vel);
}

static void book_bar(const Bar *o, double t0) {
    double st = step_dur(o->genre);
    int slot = VOICE_SLOT[o->voice];
    for (int s = 0; s < 16; s++) {
        double t = step_time(o, t0, s);
        int klen = keys_at(o, s);
        if (klen) {
            const ChordFig *cf = &CHORD_FIGS[o->chords];
            int ms = (int)(klen * st * 1000.0 * 0.95);
            for (int k = 0; k < o->nvoicing; k++)
                schedule_at(t + k * cf->roll, o->voicing[k], SL_KEYS + o->chords - 1, cf->vel, ms);
        }
        if (o->note[s] >= 0) {
            int ms = (int)(o->len[s] * st * 1000.0 * 0.9);
            if (o->voice == V_UPRIGHT && ms < 160 && s + 3 < o->cut) ms = 160;   // a pizz wants time to ring (not into the gap)
            NoteOpts opt = { .accent = s == 0 ? 0.6f : 0.0f };
            schedule_note_at(t, o->note[s], slot, s == 0 ? 7 : 6, ms, &opt);
        }
    }
    BookCtx bc = { o, t0 };
    bar_drums(o, book_drum, &bc);
}

// ── the cart ──────────────────────────────────────────────────────────────────
static Jam jam;
static float k_energy = 0.5f, push = 0.0f;
static double next_t = -1.0;
#define RING 8
static Bar hist[RING]; static double hist_t[RING]; static int hist_n = 0;
static int lean_flash = 0;   // frames the next chip stays lit after an A / N sounds

#define PUSH_X 268
#define PUSH_Y 146
#define PUSH_W 46
#define PUSH_H 28
static float energy(void) { return k_energy > push ? k_energy : push; }

void init(void) {
    resolve_patterns();
    setup_sound();
    reverb(0.35f, 0.5f);
    instrument_reverb(SL_UPRIGHT, 0.12f);
    jam_init(&jam, (Picks){ .walk = 2, .fig = 1, .pace = 0, .genre = G_FUNK, .fam = 0, .voice = V_UPRIGHT });
}

static void cycle(int *v, int n) { *v = (*v + 1) % n; }

static void arm_genre(void) {
    cycle(&jam.want.genre, NGENRE);
    jam.want.fam = 0;
}

void update(void) {
    if (keyp('W')) cycle(&jam.want.walk, NWALK);
    if (keyp('F')) cycle(&jam.want.fig, NFIG);
    if (keyp('P')) cycle(&jam.want.pace, 2);
    if (keyp('G')) arm_genre();
    if (keyp('R')) cycle(&jam.want.fam, GEN[jam.want.genre].nfam);
    if (keyp('V')) cycle(&jam.want.voice, NVOICE);
    if (keyp('K')) cycle(&jam.want.key, 12);
    if (keyp('C')) cycle(&jam.want.chords, NCHORDFIG);
    // one condition per pick: a mouse click can also arrive as a touch, and two statements cycled twice
    int mrow = mouse_pressed(MOUSE_LEFT) && mouse_y() >= 126 && mouse_y() < 135;
    if (keyp('E') || tapp(6, 126, 72, 9)  || (mrow && mouse_x() >= 6  && mouse_x() < 78))  cycle(&jam.want.lean, 3);
    if (keyp('X') || tapp(82, 126, 64, 9) || (mrow && mouse_x() >= 82 && mouse_x() < 146)) cycle(&jam.want.gap, 3);
    if (keyp('O') || tapp(150, 126, 62, 9) || (mrow && mouse_x() >= 150 && mouse_x() < 212)) cycle(&jam.want.spot, NSPK);
    if (keyp('Y') || tapp(214, 126, 60, 9) || (mrow && mouse_x() >= 214 && mouse_x() < 274)) cycle(&jam.want.vary, 3);
    if (keyp('L') || tapp(6, 112, 228, 12) || (mouse_pressed(MOUSE_LEFT) && mouse_x() >= 6 && mouse_x() < 234
                                               && mouse_y() >= 112 && mouse_y() < 124)) jam.want.all = !jam.want.all;
    if (key(KEY_UP))   { k_energy += 0.01f; if (k_energy > 1) k_energy = 1; }
    if (key(KEY_DOWN)) { k_energy -= 0.01f; if (k_energy < 0) k_energy = 0; }

    // hold to push: climbs to the top in ~1.5 s, falls back over ~4 s
    int held = key(KEY_SPACE) || tap(PUSH_X, PUSH_Y, PUSH_W, PUSH_H)
            || (mouse_down(MOUSE_LEFT) && mouse_x() >= PUSH_X && mouse_x() < PUSH_X + PUSH_W
                && mouse_y() >= PUSH_Y && mouse_y() < PUSH_Y + PUSH_H);
    if (held) push += 1.0f / 90.0f; else push -= 1.0f / 240.0f;
    if (push > 1) push = 1; if (push < 0) push = 0;

    // book every bar that starts within AHEAD of the audio clock
    double now_t = audio_time();
    if (next_t < 0) next_t = now_t + 0.1;
    while (next_t < now_t + AHEAD) {
        Bar o; jam_plan(&jam, energy(), &o);
        book_bar(&o, next_t);
        hist[hist_n % RING] = o; hist_t[hist_n % RING] = next_t; hist_n++;
        next_t += 16 * step_dur(o.genre);
    }
    if (lean_flash > 0) lean_flash--;
}

// the bar sounding now (not the one just planned) + where in it we are, 0..16 steps
static const Bar *sounding(double *pos) {
    double now_t = audio_time();
    const Bar *best = NULL;
    for (int k = hist_n - 1; k >= 0 && k >= hist_n - RING; k--) {
        if (hist_t[k % RING] <= now_t) {
            best = &hist[k % RING];
            *pos = (now_t - hist_t[k % RING]) / step_dur(best->genre);
            if (*pos > 15.999) *pos = 15.999;
            break;
        }
    }
    return best;
}

static const char *NOTE[12] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
static const char *NUMERAL[12] = { "1","b2","2","b3","3","4","b5","5","b6","6","b7","7" };
static void chord_name(int key, int deg, char kind, char *out, int n) {
    snprintf(out, n, "%s%s", NOTE[(key + deg) % 12], kind == 'm' ? "m" : kind == '7' ? "7" : "");   // J / s: tagged below
}

// a cycling picker: label on top, the value below, a dot when armed but not yet sounding
static int picker(int x, int y, int w, const char *label, const char *val, int armed) {
    int hit = ui_button(x, y, w, 20, "");
    font(FONT_TINY);
    print(label, x + 3, y + 3, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    print(val, x + 3, y + 11, armed ? CLR_YELLOW : CLR_WHITE);
    if (armed && (frame() / 15) % 2) circfill(x + w - 5, y + 5, 2, CLR_YELLOW);
    font(FONT_NORMAL);
    return hit;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    double pos = 0;
    const Bar *o = sounding(&pos);
    int st = (int)pos;

    font(FONT_NORMAL);
    print("ROOT WALK", 6, 5, CLR_WHITE);
    font(FONT_SMALL);
    if (o) {
        char hdr[64];
        snprintf(hdr, sizeof hdr, "%s / %s  %d BPM", GEN[o->genre].name,
                 GEN[o->genre].fam[o->fam].name, GEN[o->genre].bpm);
        print_right(hdr, 314, 6, CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);
    if (!o) return;

    // ── the walk: one chip per step, as wide as its bars, a playhead through it ──
    const Walk *w = &WALKS[o->walk];
    int total = w->n * o->pace;
    int tx = 8, tw = 304, ty = 22, th = 40;
    int bars_in = o->pace - o->left;                       // bars already played on this root
    int x = tx;
    for (int k = 0; k < w->n; k++) {
        int cw = tw * o->pace / total;
        int cur = k == o->wstep;
        int nxt = o->moves && k == (o->wstep + 1) % w->n && WALKS[jam.ch.walk].deg[jam.ch_wstep] == w->deg[k]
                  && jam.ch.walk == o->walk;
        int lit = nxt && (lean_flash > 0 || st >= 12);
        rectfill(x + 1, ty, cw - 2, th, cur ? CLR_BLUE : lit ? CLR_DARK_PURPLE : CLR_DARK_BLUE);
        if (lit) rect(x + 1, ty, cw - 2, th, CLR_YELLOW);
        char nm[8]; chord_name(o->key, w->deg[k], w->kinds[k], nm, sizeof nm);
        if (cw >= 30) print_scaled(nm, x + 5, ty + 6, cur ? CLR_WHITE : CLR_LIGHT_GREY, cw >= 60 ? 2 : 1);
        else print(nm, x + 3, ty + 6, cur ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_TINY);
        print(NUMERAL[w->deg[k] % 12], x + 3, ty + th - 8, cur ? CLR_LIGHT_PEACH : CLR_MEDIUM_GREY);
        if (w->kinds[k] == 'J' || w->kinds[k] == 's')
            print(w->kinds[k] == 'J' ? "MAJ9" : "9SUS", x + 3, ty + th - 15, cur ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_NORMAL);
        if (cur) {
            int ph = x + 1 + (int)((cw - 2) * (bars_in + pos / 16.0) / o->pace);
            line(ph, ty - 2, ph, ty + th + 1, CLR_YELLOW);
        }
        x += cw;
    }
    // the change that is coming, when it differs from the plain next step
    font(FONT_SMALL);
    if (o->moves && jam.ch.walk != o->walk)
        print("NEW WALK FROM THE NEXT BAR", tx, ty + th + 4, CLR_YELLOW);
    else if (o->mode == M_BREAK && o->inph == PHRASE - 1 && o->moves)
        print("THE DROP BRINGS THE WALK HOME", tx, ty + th + 4, CLR_YELLOW);
    else {
        char nb[48]; snprintf(nb, sizeof nb, "WALK %s  EVERY %d BARS", w->name, o->pace);
        print(nb, tx, ty + th + 4, CLR_MEDIUM_GREY);
    }

    // ── the bass figure: 16 steps of degrees, the sounding step lit ──
    int fy = 76, fx = 40, cs = 17;
    font(FONT_SMALL);
    print("BASS", 6, fy + 4, o->on[L_BASS] ? CLR_WHITE : CLR_DARK_GREY);
    for (int s = 0; s < 16; s++) {
        char c = o->kind[s];
        int note = o->note[s] >= 0;
        int lean = c == 'A' || c == 'N';
        int col = !note ? (c == '-' ? CLR_DARK_BLUE : CLR_DARKER_GREY)
                : lean ? CLR_ORANGE : CLR_GREEN;
        int cx = fx + s * cs;
        rectfill(cx, fy, cs - 2, 14, col);
        if (s == st) rect(cx - 1, fy - 1, cs, 16, CLR_WHITE);
        if (c != '.' && c != '-') { char t[2] = { c, 0 }; print_centered(t, cx + (cs - 2) / 2, fy + 4, CLR_BLACK); }
        if (note && lean && s == st) lean_flash = 20;
    }
    {
        char nm[24];
        int cur_note = -1;
        for (int s = 0; s <= st; s++) if (o->note[s] >= 0) cur_note = o->note[s];
        if (cur_note >= 0) snprintf(nm, sizeof nm, "%s%d", NOTE[cur_note % 12], cur_note / 12 - 1);
        else snprintf(nm, sizeof nm, "-");
        print(nm, 6, fy + 18, CLR_GREEN);
        print(FIGS[o->fig].name, fx, fy + 18, CLR_MEDIUM_GREY);
        if (FIGS[o->fig].form) {
            const char *fm = FIGS[o->fig].form;
            int fx2 = fx + 70;
            for (int i = 0; fm[i]; i++) {
                char t[2] = { fm[i], 0 };
                int lit = (int)strlen(fm) - (o->moves ? 1 : o->left) == i
                       || (i == 0 && (int)strlen(fm) - (o->moves ? 1 : o->left) < 0);
                print(t, fx2 + i * 7, fy + 18, lit ? CLR_YELLOW : CLR_DARK_GREY);
            }
        }
    }

    // ── the arrangement: layers, mode, phrase position ──
    // the chords: the figure's 16 steps + the voicing that is sounding
    {
        int ky = 102;
        const ChordFig *cf = &CHORD_FIGS[o->chords];
        print("KEYS", 6, ky, o->keys_on ? CLR_WHITE : CLR_DARK_GREY);
        for (int s = 0; s < 16; s++) {
            char c = cf->steps[s];
            int col = !o->keys_on ? CLR_DARKER_GREY : c == 'x' ? CLR_PINK : c == '-' ? CLR_DARK_PURPLE : CLR_DARKER_GREY;
            rectfill(fx + s * cs, ky + 1, cs - 2, 4, col);
        }
        if (o->keys_on && o->nvoicing) {
            char vb[32] = ""; int at = 0;
            for (int k = 0; k < o->nvoicing && at < 28; k++)
                at += snprintf(vb + at, sizeof vb - at, "%s%s", k ? " " : "", NOTE[o->voicing[k] % 12]);
            print_right(vb, 314, ky - 8, CLR_PINK);
        }
    }
    int ly = 112;
    for (int k = 0; k < NLAYER; k++) {
        int lx = 6 + k * 38;
        rectfill(lx, ly, 35, 12, o->on[k] ? (k == L_BASS ? CLR_GREEN : k == L_KEYS ? CLR_PINK : CLR_RED) : CLR_DARKER_GREY);
        print_centered(LAYER_NAME[k], lx + 17, ly + 3, o->on[k] ? CLR_WHITE : CLR_DARK_GREY);
    }
    if (jam.want.all || jam.cur.all) {
        rect(4, ly - 2, 230, 16, jam.want.all != jam.cur.all && (frame() / 15) % 2 ? CLR_DARK_GREY : CLR_YELLOW);
        font(FONT_TINY); print(jam.want.all ? "ALL" : "AUTO", 296, ly + 4, CLR_YELLOW); font(FONT_SMALL);
    }
    // LEAN + GAP: how the drums play the song (tap to cycle AUTO / ALWAYS / NEVER)
    {
        char lb[24];
        const Picks *a = &jam.want, *c = &jam.cur;
        font(FONT_TINY);
        snprintf(lb, sizeof lb, "E LEAN %s", PICK3[a->lean]);
        print(lb, 8, ly + 16, a->lean != c->lean ? CLR_YELLOW : CLR_MEDIUM_GREY);
        snprintf(lb, sizeof lb, "X GAP %s", PICK3[a->gap]);
        print(lb, 84, ly + 16, a->gap != c->gap ? CLR_YELLOW : CLR_MEDIUM_GREY);
        snprintf(lb, sizeof lb, "O SPOT %s", SPK_NAME[a->spot]);
        print(lb, 152, ly + 16, a->spot != c->spot ? CLR_YELLOW : CLR_MEDIUM_GREY);
        snprintf(lb, sizeof lb, "Y VARY %s", PICK3[a->vary]);
        print(lb, 216, ly + 16, a->vary != c->vary ? CLR_YELLOW : CLR_MEDIUM_GREY);
        // what the drums are doing right now, next to the figure name
        const char *now = o->spot_on ? SPOT_NAME[o->spot]
                        : o->lean ? (o->lean == 1 ? "OPEN HAT LEANS IN" : o->lean == 2 ? "KICK LEANS IN" : "SNARE LEANS IN")
                        : o->cut < 16 ? "THE GAP" : o->turn ? "TURNAROUND" : NULL;
        if (now) print(now, 160, 95, CLR_ORANGE);
        font(FONT_SMALL);
    }
    print(MODE_NAME[o->mode], 240, ly + 3, o->mode == M_GROOVE ? CLR_LIGHT_GREY : CLR_YELLOW);
    for (int i = 0; i < PHRASE; i++)
        rectfill(240 + i * 9, ly + 12, 7, 3, i == o->inph ? (o->fill && i == PHRASE - 1 ? CLR_ORANGE : CLR_WHITE)
                                                           : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── the pickers (arm a change for the next root change) + energy ──
    ui_begin();
    const Picks *c = &jam.cur, *a = &jam.want;
    int py = 140;
    char pbuf[8]; snprintf(pbuf, sizeof pbuf, "%d BARS", PACES[a->pace]);
    if (picker(6,   py, 62, "W  WALK",   WALKS[a->walk].name, a->walk != c->walk)) cycle(&jam.want.walk, NWALK);
    if (picker(70,  py, 66, "F  FIGURE", FIGS[a->fig].name,   a->fig != c->fig))   cycle(&jam.want.fig, NFIG);
    if (picker(138, py, 38, "P  PACE",   pbuf,                a->pace != c->pace)) cycle(&jam.want.pace, 2);
    if (picker(178, py, 36, "K  KEY",    NOTE[a->key],        a->key != c->key))   cycle(&jam.want.key, 12);
    py += 24;
    if (picker(6,   py, 62, "G  GENRE",  GEN[a->genre].name,  a->genre != c->genre)) arm_genre();
    if (picker(70,  py, 66, "R  FAMILY", GEN[a->genre].fam[a->fam % GEN[a->genre].nfam].name,
               a->genre != c->genre || a->fam != c->fam)) cycle(&jam.want.fam, GEN[a->genre].nfam);
    if (picker(138, py, 38, "V  VOICE",  VOICE_NAME[a->voice], a->voice != c->voice)) cycle(&jam.want.voice, NVOICE);
    if (picker(178, py, 36, "C  CHORDS", CHORD_FIGS[a->chords].name, a->chords != c->chords)) cycle(&jam.want.chords, NCHORDFIG);

    ui_knob(&k_energy, 240, 160, "ENERGY");
    ui_end();
    // PUSH is held, not clicked (update() reads the finger / mouse / SPACE), so it is drawn here
    {
        int down = push > k_energy && push > 0.02f;
        rectfill(PUSH_X, PUSH_Y, PUSH_W, PUSH_H, down ? CLR_YELLOW : CLR_DARK_BLUE);
        rect(PUSH_X, PUSH_Y, PUSH_W, PUSH_H, CLR_LIGHT_GREY);
        print_centered("PUSH", PUSH_X + PUSH_W / 2, PUSH_Y + 10, down ? CLR_BLACK : CLR_WHITE);
    }
    // energy meter under the knob: knob value + the push on top
    rectfill(222, 192, 36, 3, CLR_DARK_GREY);
    rectfill(222, 192, (int)(36 * energy()), 3, push > k_energy ? CLR_YELLOW : CLR_GREEN);
}

#ifdef DE_SPEC
#include "spec.h"
// Pins the core: the root per bar follows the walk and pace, A lands a half step under the
// next bar's root and N on it, a walk armed mid-song starts at the next change from its first
// step, and with energy at zero the layers never grow (and a breakdown never drops the bass).
static void spec_late_probe(int role, int st, double dt, int vel, void *c) { (void)role; (void)dt; (void)vel; if (st >= 12) *(int *)c = 1; }
static int root_home(int deg) { return in_range(deg); }   // the spec runs in C
void spec(void) {
    resolve_patterns();
    for (int gi = 0; gi < NGENRE; gi++) {
        expect(GEN[gi].nfam > 0, "every genre resolved at least one family");
        for (int f = 0; f < GEN[gi].nfam; f++) expect(GEN[gi].fam[f].nv >= 2, "every family has variants");
    }
    expect(GEN[G_DISCO].fam[0].has_fill, "the book's Disco 1 brings its written Break");

    // 1. the root per bar follows the walk at its pace
    Jam j; Bar o;
    for (int wi = 0; wi < NWALK; wi++) for (int pc = 0; pc < 2; pc++) {
        jam_init(&j, (Picks){ .walk = wi, .fig = 1, .pace = pc, .genre = G_FUNK });
        int ok = 1;
        for (int b = 0; b < 32; b++) {          // energy 0: no breakdown, so no drop resets the walk
            jam_plan(&j, 0.0f, &o);
            int want = WALKS[wi].deg[(b / PACES[pc]) % WALKS[wi].n];
            if (o.root != want) ok = 0;
        }
        expect(ok, "root per bar follows the walk at its pace");
    }

    // 2. A sits a half step under the next bar's root; N is the next bar's root
    const int figs[2] = { 5 /* APPROACH */, 2 /* THE PUSH */ };
    for (int fi = 0; fi < 4; fi++) {            // each figure at energy 0, then at 1 (drops reset the walk)
        float e = fi < 2 ? 0.0f : 1.0f;
        jam_init(&j, (Picks){ .walk = 1, .fig = figs[fi % 2], .pace = 0, .genre = G_FUNK });
        Bar prev; int checked = 0, ok = 1;
        jam_plan(&j, e, &prev);
        for (int b = 1; b < 8 * 12; b++) {
            jam_plan(&j, e, &o);
            for (int s = 0; s < 16; s++) {
                if (prev.kind[s] == 'A' && prev.note[s] >= 0) { checked++; if (prev.note[s] != root_home(o.root) - 1) ok = 0; }
                if (prev.kind[s] == 'N' && prev.note[s] >= 0) { checked++; if (prev.note[s] != root_home(o.root)) ok = 0; }
            }
            prev = o;
        }
        expect(checked > 0, "the figure leaned into a change at least once");
        expect(ok, fi % 2 == 0 ? "A lands a half step under the next root" : "N lands on the next root");
    }

    // 3. a walk armed mid-song starts at the NEXT change, from its first step
    jam_init(&j, (Picks){ .walk = 0, .fig = 1, .pace = 1, .genre = G_FUNK });   // 1 4 5 1, 4 bars
    for (int b = 0; b < 5; b++) jam_plan(&j, 0.0f, &o);                        // bar 4 = step 1, first bar
    expect_eq(o.root, 5, "on the 4 after the first change");
    j.want.walk = 3;                                                           // arm 2 5 1
    int changed_at = -1;
    for (int b = 5; b < 12; b++) {
        jam_plan(&j, 0.0f, &o);
        if (o.walk == 3 && changed_at < 0) { changed_at = b; expect_eq(o.wstep, 0, "the new walk starts at its first step"); expect_eq(o.root, WALKS[3].deg[0], "on its first root"); }
        if (o.walk != 3) expect_eq(o.root, 5, "the old root holds until the change");
    }
    expect_eq(changed_at, 8, "the armed walk lands exactly at the next change");

    // 4. energy at zero: the layers never grow; at full: breakdowns come, the bass stays
    jam_init(&j, (Picks){ .walk = 0, .fig = 1, .pace = 0, .genre = G_FUNK });
    int grew = 0;
    for (int b = 0; b < 8 * 12; b++) { jam_plan(&j, 0.0f, &o); if (o.layers > MINL) grew = 1; if (!o.on[L_BASS]) grew = 1; }
    expect(!grew, "energy 0: kick + bass only, for twelve phrases");
    jam_init(&j, (Picks){ .walk = 0, .fig = 1, .pace = 0, .genre = G_FUNK });
    int breaks = 0, drops = 0, bass_lost = 0, drop_home = 1;
    for (int b = 0; b < 8 * 16; b++) {
        jam_plan(&j, 1.0f, &o);
        if (o.inph == 0 && o.mode == M_BREAK) breaks++;
        if (o.inph == 0 && o.mode == M_DROP) { drops++; if (o.wstep != 0) drop_home = 0; }
        if (o.mode == M_BREAK && o.on[L_KICK]) bass_lost = 1;    // the kick must leave...
        if (!o.on[L_BASS] && !o.spot_on) bass_lost = 1;          // ...the bass never does (a spotlight may)
    }
    expect(breaks > 0 && drops > 0, "full energy: breakdowns and drops happen");
    expect(!bass_lost, "a breakdown drops the kick and keeps the bass");
    expect(drop_home, "the drop lands the walk back home");

    // 6. riffs follow the chord; plain figures do not; the key moves everything; genre lands next bar
    {
        int climb = -1, rob7 = 1;
        for (int fi = 0; fi < NFIG; fi++) if (FIGS[fi].form && strcmp(FIGS[fi].name, "CLIMB") == 0) climb = fi;
        expect(climb > 0, "CLIMB is on the shelf");
        jam_init(&j, (Picks){ .walk = 0, .fig = climb, .pace = 0, .genre = G_FUNK });   // 1 4 5 1 = M M 7 M
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.chord, 'M', "1 4 5 1 starts on a major chord");
        expect_eq(o.note[6] - root_home(0), 2, "over M, the riff's m plays a 2");
        expect_eq(o.note[8] - root_home(0), 4, "over M, the riff's 4 plays a major third");
        jam_init(&j, (Picks){ .walk = 3, .fig = climb, .pace = 0, .genre = G_FUNK });   // 2 5 1 = m 7 M M
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.chord, 'm', "2 5 1 starts on a minor chord");
        expect_eq(o.note[6] - root_home(2), 3, "over m, the riff keeps its minor third");
        jam_init(&j, (Picks){ .walk = 0, .fig = rob7, .pace = 0, .genre = G_FUNK });
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.note[6] - root_home(0), 10, "a plain figure's b7 stays a b7 over a major (not maj9) chord");
        jam_init(&j, (Picks){ .walk = 0, .fig = rob7, .pace = 0, .genre = G_FUNK, .key = 7 });
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.note[0], 43, "in G the root is G2");
        jam_init(&j, (Picks){ .walk = 0, .fig = rob7, .pace = 1, .genre = G_FUNK });
        jam_plan(&j, 0.0f, &o);
        j.want.genre = G_DISCO; j.want.fam = 0; j.want.key = 2;
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.genre, G_DISCO, "a genre pick lands at the next bar");
        expect_eq(o.key, 0, "a key pick waits for the next change");
        jam_plan(&j, 0.0f, &o); jam_plan(&j, 0.0f, &o); jam_plan(&j, 0.0f, &o);
        expect_eq(o.key, 2, "...and lands on it");
    }
    // the approach also holds across a KEY change: A under the next bar's root in the NEW key
    {
        jam_init(&j, (Picks){ .walk = 1, .fig = 5, .pace = 0, .genre = G_FUNK });
        Bar prev; int ok = 1, seen = 0;
        jam_plan(&j, 0.0f, &prev);
        for (int b = 1; b < 24; b++) {
            if (b == 3) j.want.key = 5;
            jam_plan(&j, 0.0f, &o);
            for (int s = 0; s < 16; s++)
                if (prev.kind[s] == 'A' && prev.note[s] >= 0) { seen++; if (prev.note[s] != in_range(o.key + o.root) - 1) ok = 0; }
            prev = o;
        }
        expect(seen > 0 && ok, "A leans into the next root across a key change");
    }
    // the wave: 9sus moves a riff's m to a 2 and keeps its 4 (no third); maj9 reads as major
    {
        int climb = -1, wave = -1;
        for (int fi = 0; fi < NFIG; fi++) if (strcmp(FIGS[fi].name, "CLIMB") == 0) climb = fi;
        for (int wi = 0; wi < NWALK; wi++) if (strcmp(WALKS[wi].name, "WAVE") == 0) wave = wi;
        jam_init(&j, (Picks){ .walk = wave, .fig = climb, .pace = 0, .genre = G_FUNK });
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.chord, 'J', "the wave starts on a maj9");
        expect_eq(o.note[6] - root_home(0), 2, "over maj9, the riff's m plays a 2");
        expect_eq(o.note[8] - root_home(0), 4, "over maj9, the riff's 4 plays the major third");
        jam_plan(&j, 0.0f, &o); jam_plan(&j, 0.0f, &o);            // bar 2 = step 1, a 9sus on the 4
        expect_eq(o.chord, 's', "then a 9sus");
        expect_eq(o.note[6] - root_home(5), 2, "over 9sus, the riff's m plays a 2");
        expect_eq(o.note[8] - root_home(5), 5, "over 9sus, the riff's 4 stays a 4 (no third)");
        int home = 1;
        jam_init(&j, (Picks){ .walk = wave, .fig = 1, .pace = 0, .genre = G_FUNK });
        for (int b = 0; b < 26; b++) { jam_plan(&j, 0.0f, &o); if (b == 24 && o.root != 0) home = 0; }
        expect(home, "twelve steps up in fourths and the wave is home");
    }
    // the chord layer: keys come in third; a voicing holds the chord's notes, in range, no b9, and
    // moves close to the last one; a picked chord style plays even in a calm jam
    {
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK });
        int keys_early = 0, keys_seen = 0, voiced_ok = 1, close = 1, prev[MAXVOICE], np = 0;
        for (int b = 0; b < 8 * 6; b++) {
            jam_plan(&j, 0.5f, &o);                                  // target = 3 layers: kick bass keys
            if (o.on[L_KEYS] && o.layers < 3) keys_early = 1;
            if (!o.keys_on) continue;
            keys_seen++;
            int niv; const int *iv = keys_chord(o.chord, &niv);
            const ChordFig *cf = &CHORD_FIGS[o.chords];
            for (int k = 0; k < o.nvoicing; k++) {
                int pc = (o.voicing[k] - (o.key + o.root)) % 12, ok = 0; pc = (pc + 12) % 12;
                for (int i = 0; i < niv; i++) if (iv[i] % 12 == pc) ok = 1;
                if (!ok || o.voicing[k] > cf->hi) voiced_ok = 0;
                for (int m2 = k + 1; m2 < o.nvoicing; m2++) { int d = o.voicing[m2] - o.voicing[k]; if (d == 1 || d == 13 || d == 25) voiced_ok = 0; }
            }
            if (o.voicing[0] < cf->lo || o.voicing[0] > cf->lo + 12) voiced_ok = 0;
            if (np) { int top = o.voicing[o.nvoicing - 1] - prev[np - 1]; if (top > 7 || top < -7) close = 0; }
            memcpy(prev, o.voicing, sizeof prev); np = o.nvoicing;
        }
        expect(!keys_early, "the keys are the third layer in");
        expect(keys_seen > 0, "the keys come in at middling energy");
        expect(voiced_ok, "every voicing: the chord's notes, in range, no b9");
        expect(close, "the top voice moves by a fifth at most, chord to chord");
        jam_init(&j, (Picks){ .walk = 6, .fig = 1, .pace = 0, .genre = G_FUNK });
        jam_plan(&j, 0.0f, &o);
        expect(!o.keys_on, "a calm jam on AUTO has no keys");
        j.want.chords = CF_STRUM;
        jam_plan(&j, 0.0f, &o);
        expect(o.keys_on && o.chords == CF_STRUM, "a picked chord style plays from the next bar, calm or not");
    }
    // ALL LAYERS: everything from the next bar at zero energy, breakdowns still come
    {
        jam_init(&j, (Picks){ .walk = 0, .fig = 1, .pace = 0, .genre = G_FUNK });
        jam_plan(&j, 0.0f, &o);
        expect_eq(o.layers, MINL, "calm: kick + bass");
        j.want.all = 1;
        jam_plan(&j, 0.0f, &o);
        int all_on = 1; for (int k = 0; k < NLAYER; k++) if (!o.on[k]) all_on = 0;
        expect(all_on, "ALL: every layer from the next bar, at zero energy");
        int breaks = 0, kick_in_break = 0;
        for (int b = 0; b < 8 * 10; b++) { jam_plan(&j, 0.0f, &o); if (o.mode == M_BREAK) { breaks++; if (o.on[L_KICK]) kick_in_break = 1; } }
        expect(breaks > 0 && !kick_in_break, "ALL: breakdowns still drop the kick");
    }
    // over maj9 a plain figure's 7 is the MAJOR 7th; over everything else it stays a b7
    // (playtime-71's known answers: root-octave-b7 in F)
    {
        int wave = -1;
        for (int wi = 0; wi < NWALK; wi++) if (strcmp(WALKS[wi].name, "WAVE") == 0) wave = wi;
        jam_init(&j, (Picks){ .walk = wave, .fig = 1, .pace = 0, .genre = G_FUNK, .key = 5 });
        jam_plan(&j, 0.0f, &o);
        expect(o.note[0] == 41 && o.note[3] == 53 && o.note[6] == 52, "Fmaj9: F2 F3 E3");
        jam_plan(&j, 0.0f, &o); jam_plan(&j, 0.0f, &o);
        expect(o.note[0] == 34 && o.note[3] == 46 && o.note[6] == 44, "Bb9sus: A#1 A#2 G#2");
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .key = 5 });
        jam_plan(&j, 0.0f, &o);
        expect(o.note[0] == 41 && o.note[3] == 53 && o.note[6] == 51, "1 5 6 4 in F: F2 F3 D#3");
    }
    // THE DRUMS PLAY THE SONG: before every drop no part plays at step 12 or later (Playtime's own
    // spec), a lean only on a bar into a real change, longer fills when it's busy, toms in Funk
    {
        jam_init(&j, (Picks){ .walk = 1, .fig = 2 /* THE PUSH: a note on 14 */, .pace = 0, .genre = G_FUNK,
                              .chords = CF_STRUM });
        int gaps = 0, clean = 1, leans = 0, lean_wrong = 0, run8 = 0, toms = 0;
        Bar prev = { 0 }; int have = 0;
        for (int b = 0; b < 8 * 16; b++) {
            jam_plan(&j, 1.0f, &o);
            if (have && prev.cut == 12) {
                gaps++;
                if (o.mode != M_DROP || o.inph != 0) clean = 0;            // AUTO: a gap only before a drop
            }
            if (have && o.mode == M_DROP && o.inph == 0) {
                if (prev.cut != 12) clean = 0;
                for (int s2 = 12; s2 < 16; s2++) {
                    if (prev.note[s2] >= 0 || keys_at(&prev, s2)) clean = 0;
                }
                for (int s2 = 0; s2 < 12; s2++) if (prev.note[s2] >= 0 && s2 + prev.len[s2] > 12) clean = 0;
                // and the drums: walk the events
                int late = 0;
                bar_drums(&prev, spec_late_probe, &late);
                if (late) clean = 0;
            }
            if (o.lean) { leans++; if (o.inph == PHRASE - 1 || !o.moves || o.next_root == o.root) lean_wrong = 1; }
            if (o.fill && !o.written && o.run_from == 8) run8++;
            if (o.toms) toms++;
            prev = o; have = 1;
        }
        expect(gaps > 0, "there were drops to gap before");
        expect(clean, "before every drop: nothing from step 12 (bass, keys, drums, holds)");
        expect(leans > 0 && !lean_wrong, "the drums lean only into a real root change");
        (void)run8; (void)toms;     // Funk's families all bring a WRITTEN break: the run-up is for Breaks
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_BREAKS });
        run8 = toms = 0; int calm12 = 1;
        for (int b = 0; b < 8 * 16; b++) {
            jam_plan(&j, 1.0f, &o);
            if (o.fill && !o.written && o.cut == 16 && o.run_from == 8) run8++;
            if (o.toms) toms++;
        }
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_BREAKS, .chords = 0 });
        for (int b = 0; b < 8 * 6; b++) { jam_plan(&j, 0.5f, &o); if (o.fill && !o.written && o.cut == 16 && o.run_from != 12) calm12 = 0; }
        expect(run8 > 0, "a busy jam's fills run from the middle of the bar");
        expect(calm12, "a middling jam's fills run the last beat only");
        expect(toms > 0, "Breaks gets a tom fill now and then");
        // the BUILD: a breakdown's last bar (the gap) runs the snare from the top of the bar
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_BREAKS });
        int builds = 0, build_ok = 1;
        for (int b = 0; b < 8 * 16; b++) {
            jam_plan(&j, 1.0f, &o);
            if (o.mode == M_BREAK && o.cut == 12 && o.fill && !o.written) { builds++; if (o.run_from != 0) build_ok = 0; }
        }
        expect(builds > 0 && build_ok, "a breakdown's gap bar is the BUILD: a snare run from step 0");
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .lean = P_NEVER, .gap = P_NEVER });
        int any = 0;
        for (int b = 0; b < 8 * 16; b++) { jam_plan(&j, 1.0f, &o); if (o.lean || o.cut < 16) any = 1; }
        expect(!any, "NEVER means never, for lean and gap");
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .lean = P_ALWAYS, .gap = P_ALWAYS });
        int lean_all = 1, gap_all = 1;
        for (int b = 0; b < 8 * 4; b++) {
            jam_plan(&j, 1.0f, &o);
            if (o.inph == PHRASE - 1 && o.cut != 12) gap_all = 0;
            if (o.inph != PHRASE - 1 && o.moves && o.next_root != o.root && !o.lean) lean_all = 0;
        }
        expect(gap_all, "GAP ALWAYS: every phrase ends in the gap");
        expect(lean_all, "LEAN ALWAYS: every real change gets a lean");
    }
    // SPOTLIGHTS: only in a groove phrase's last 4 bars; drum break = no bass/keys; trading = bass
    // alone then drums alone; bass feature = a riff A v1 A v2, keys out
    {
        int seen[NSPOT] = { 0 }, ok = 1, early = 0, not_groove = 0;
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK });
        for (int b = 0; b < 8 * 40; b++) {
            jam_plan(&j, 1.0f, &o);
            if (o.spot_on && o.inph < PHRASE - 4) early = 1;
            if (o.spot && o.mode != M_GROOVE) not_groove = 1;
            if (!o.spot_on) continue;
            seen[o.spot]++;
            if (o.keys_on) ok = 0;
            if (o.spot == SP_DRUMS && (o.on[L_BASS] || !o.on[L_KICK])) ok = 0;
            if (o.spot == SP_TRADE && o.inph < PHRASE - 2 && (o.on[L_KICK] || !o.on[L_BASS])) ok = 0;
            if (o.spot == SP_TRADE && o.inph >= PHRASE - 2 && (o.on[L_BASS] || !o.on[L_KICK])) ok = 0;
            if (o.spot == SP_BASS) {
                if (o.feature_fig < 0 || !FIGS[o.feature_fig].form) ok = 0;
                int k = o.inph - (PHRASE - 4);
                if ((k == 0 || k == 2) && o.letter != 'A') ok = 0;
                if ((k == 1 || k == 3) && o.letter == 0) ok = 0;
            }
        }
        expect(!early && !not_groove, "a spotlight is a groove phrase's last 4 bars, nothing else");
        expect(seen[SP_DRUMS] + seen[SP_BASS] + seen[SP_TRADE] > 0, "AUTO: a busy jam gets spotlights");
        expect(ok, "each spotlight sends out the right parts (and the keys)");
        for (int sp = SPK_DRUMS; sp <= SPK_TRADE; sp++) {
            jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .spot = sp });
            int every = 1;
            for (int b = 0; b < 8 * 8; b++) { jam_plan(&j, 0.0f, &o); if (o.mode == M_GROOVE && o.inph >= 4 && !o.spot_on) every = 0; }
            expect(every, "a named spotlight plays in every groove phrase, calm or not");
        }
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .spot = SPK_NEVER });
        int none = 1; for (int b = 0; b < 8 * 40; b++) { jam_plan(&j, 1.0f, &o); if (o.spot) none = 0; }
        expect(none, "spotlight NEVER");
    }
    // VARIATIONS: a turnaround bar 7 on the family's next pattern; ghosts only where the pattern
    // has no snare; NEVER means none
    {
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .vary = P_ALWAYS, .spot = SPK_NEVER });
        int turns = 0, turn_ok = 1, ghost_ok = 1, ghosts = 0;
        const char *mask = GHOSTS[j.ghosts];
        for (int b = 0; b < 8 * 8; b++) {
            jam_plan(&j, 1.0f, &o);
            const Family *f = &GEN[o.genre].fam[o.fam];
            if (o.turn) { turns++; if (o.inph != PHRASE - 2 || o.pat != &f->v[(o.var + 1) % f->nv]) turn_ok = 0; }
            if (o.ghosts && o.on[L_SNARE]) for (int s2 = 0; s2 < 16; s2++)
                if (mask[s2] == 'x' && !((o.pat->hit[DP_SD] >> s2) & 1) && !(o.fill && !o.written)) ghosts++;
            if (o.ghosts && o.ghosts - 1 != j.ghosts) ghost_ok = 0;
        }
        expect(turns > 0 && turn_ok, "the turnaround plays the next pattern on bar 7");
        expect(ghosts > 0 && ghost_ok, "ghost snares fill the song's mask where the pattern is empty");
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK, .vary = P_NEVER });
        int any = 0; for (int b = 0; b < 8 * 8; b++) { jam_plan(&j, 1.0f, &o); if (o.turn || o.ghosts || o.hats16) any = 1; }
        expect(!any, "variations NEVER");
        // the written fill now plays two phrase ends in three: Funk gets its toms
        jam_init(&j, (Picks){ .walk = 1, .fig = 1, .pace = 0, .genre = G_FUNK });
        int written = 0, fills = 0, toms = 0;
        for (int b = 0; b < 8 * 60; b++) { jam_plan(&j, 1.0f, &o); if (o.fill) { fills++; if (o.written) written++; } if (o.toms) toms++; }
        expect(written > 0 && written < fills, "Funk's written break plays some phrase ends, not all");
        expect(toms > 0, "so Funk rolls its toms now and then");
    }
    // brush walk's B: '7--A' holds the 7 for three steps and stops AT the A, which still sounds
    {
        int bw = -1;
        for (int fi = 0; fi < NFIG; fi++) if (strcmp(FIGS[fi].name, "BRUSH WALK") == 0) bw = fi;
        jam_init(&j, (Picks){ .walk = 0, .fig = bw, .pace = 0, .genre = G_BRUSHES });
        jam_plan(&j, 0.0f, &o); jam_plan(&j, 0.0f, &o);            // bar 1 = the last before the change: B
        expect_eq(o.letter, 'B', "brush walk plays B into the change");
        expect_eq(o.len[12], 3, "the 7 holds up to the A");
        expect(o.note[15] >= 0, "the A after the hold still sounds");
    }

    // 5. riffs: the form lines up with the END of the root's step; the strong 16ths stay in the
    //    pentatonic; and A/N in a riff's variation still lean into the root that arrives
    static const int PENTA[5] = { 0, 3, 5, 7, 10 };
    for (int fi = 0; fi < NFIG; fi++) {
        const Figure *fg = &FIGS[fi];
        if (!fg->form) continue;
        int in_scale = 1;
        for (int letter = 0; letter < 3; letter++) {
            const char *ov = letter == 0 ? NULL : fg->vars[letter - 1].overlay;
            if (letter > 0 && !ov) continue;
            int n = (int)strlen(fg->loop);
            for (int s = 0; s < 16; s += 2) {
                char c = ov && ov[s] != ' ' ? ov[s] : fg->loop[s % n];
                int d = degree_of(c);
                if (c == 'A' || c == 'N' || d == -99) continue;
                int pc = ((d % 12) + 12) % 12, ok = 0;
                for (int k = 0; k < 5; k++) if (PENTA[k] == pc) ok = 1;
                if (!ok) in_scale = 0;
            }
        }
        expect(in_scale, "a riff's strong 16ths stay in the minor pentatonic");
        for (int pc = 0; pc < 2; pc++) {
            jam_init(&j, (Picks){ .walk = 1, .fig = fi, .pace = pc, .genre = G_FUNK });
            int fl = (int)strlen(fg->form), ok = 1;
            for (int b = 0; b < 16; b++) {
                jam_plan(&j, 0.0f, &o);
                int pace = PACES[pc], bars_left = pace - (b % pace);
                int li = fl - bars_left; if (li < 0) li = 0;
                if (o.letter != fg->form[li]) ok = 0;
            }
            expect(ok, pc ? "4-bar step plays the whole form" : "2-bar step plays the form's tail");
        }
        for (int e = 0; e < 2; e++) {
            jam_init(&j, (Picks){ .walk = 6, .fig = fi, .pace = 0, .genre = G_FUNK });
            Bar prev; int ok = 1;
            jam_plan(&j, (float)e, &prev);
            for (int b = 1; b < 8 * 12; b++) {
                jam_plan(&j, (float)e, &o);
                for (int s = 0; s < 16; s++) {
                    if (prev.note[s] < 0) continue;
                    if (prev.kind[s] == 'A' && prev.note[s] != root_home(o.root) - 1) ok = 0;
                    if (prev.kind[s] == 'N' && prev.note[s] != root_home(o.root)) ok = 0;
                }
                prev = o;
            }
            expect(ok, "a riff's A / N lean into the root that arrives");
        }
    }
}
#endif
