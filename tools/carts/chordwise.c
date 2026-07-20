/* de:meta
{
  "slug": "chordwise",
  "title": "chordwise",
  "status": "active",
  "created": "2026-07-20",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "functional-harmony"
  ],
  "lineage": "The first consumer of runtime/harmony.h — the shared harmony brain extracted byte-identically from bossa/cocktail's Markov chord engines (design/harmony-brain.md). Built for the loudest demand-discovery gap on record: r/musictheory demand-82, 'a harmonic progression analyzer / suggestion tool'. Analysis is generation inverted: the same function vocab + transition table that composes bossa's changes here names YOUR chords and ranks what could come next.",
  "description": {
    "summary": "Build a chord progression by tapping chords in a key - the strip names each one in roman numerals (I, ii, V...) and a NEXT panel suggests where the harmony wants to go, ranked, each with a one-word reason (cadence, home, tritone sub). Change the key and watch the same chords get re-named - or turn un-nameable (?). Five modes span MAJOR (bossa/lounge/pop), MINOR (sad-pop/EDM/rock) and BLUES, and a 7THS/TRIAD toggle reads (and voices) plain triads (Am F C G) for beginners or full sevenths for jazz. SPACE loops your changes on a soft pluck.",
    "detail": "The demand-82 toy: a progression analyzer + next-chord suggester on the shared harmony brain (runtime/harmony.h, lifted byte-identically from the bossa/cocktail radio stations). B cycles five MODES that each swap the whole tonal system - the palette respells, the strip re-analyses, NEXT re-ranks: BOSSA (the 13-function jazz vocab - 6 diatonic seats + the borrowed/chromatic shelf: secondary dominants II7/VI7, tritone sub bII7, backdoor iv/bVII7, minor v, I7), LOUNGE (cocktail's 10-function trio tuning), POP (the same major vocab weighted onto the diatonic loops a stranger hums - the I-V-vi-IV axis, 50s doo-wop), MINOR (the sad-pop/EDM/rock/lofi half of tonal music: i ii° III iv v V VI VII vii°, natural + harmonic-minor V) and BLUES (I7 IV7 V7, the 12-bar's whole world). Tap chords into an 8-slot strip; every frame the strip is RE-ANALYZED from the raw chords (root + quality) against the current mode's vocab, so the roman numerals are honest lookups, not echoes of what you pressed - move the key OR the mode under a finished progression and the same chords re-name (Cmaj7: I in C major, III in A minor), and chords outside the mode's vocabulary show ? instead of a guess. Minor/blues analysis works because the key is DECLARED, never guessed (auto-detecting major-vs-minor from raw chords is the unsolved part, which this toy sidesteps by design). The NEXT panel reads the same Markov table that composes the songs FORWARD: ranked candidates with weight pips and a one-word reason each - the research made audible: genres differ by WEIGHTS over a vocab, not by grammar. Deterministic; carries a spec() (round-trip: every function's spelling re-analyzes to itself in all 12 keys, in major, minor AND blues; doo-wop = I vi IV V; ii-V-I; minor i-VI).",
    "controls": "Tap a palette chord to add it (it plays) - or type it: Q W E R T Y = the top row, A S D F G H J = the second row. Tap a NEXT suggestion (or keys 1-4) to follow the brain. SPACE play/stop the loop, U or BACKSPACE undo, X clear. LEFT/RIGHT change the key, B cycle mode (BOSSA/LOUNGE/POP/MINOR/BLUES). 7 (or the 7THS/TRIAD button) toggles between seventh chords (Am7 Fmaj7) and plain triads (Am F) - triads also VOICE without the 7th, so the tool reads AND sounds basic."
  }
}
de:meta */
// chordwise — the progression analyzer + next-chord suggester (demand-82).
//
// The first cart on runtime/harmony.h, exercising all three directions across
// five MODES (B cycles them) — each swaps the whole vocab, so one toy covers
// MAJOR (bossa/lounge/pop), MINOR (sad-pop/EDM/rock/lofi) and BLUES:
//   ANALYZE  — the strip re-derives roman numerals from raw (root, quality)
//              every frame via hb_vocab_analyze against the current mode's vocab;
//              the key is an INPUT (declared, not guessed). Minor/blues analysis
//              is no harder than major BECAUSE the key is declared — the unsolved
//              part is guessing major-vs-minor, which the toy never does.
//   SUGGEST  — hb_suggest reads the generation table forward: ranked next
//              chords + a one-word reason (cadence / home / tritone sub...).
//   GENERATE — untouched here; bossa/cocktail keep composing on the same table.
//
// Honesty rules the toy inherits from the research (design/harmony-brain.md):
// a chord outside the current mode's vocab shows "?" (never a guess), and the
// suggestion list is short (~2-4) because real harmony is that concentrated
// (93% of next-chords live in the top two options).

#include "studio.h"
#include "harmony.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#define I_PLK 5   // the pluck that voices your taps
#define I_BSS 6   // a soft root under it

static const char *NOTE[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
// chord tones for the tap SOUND come from harmony.h's shared table (hb_tones);
// naming/choosing lives there too (spelling stays the vocab's sevenths).

static int keyPc = 0;

// a MODE = a style (which carries its VOCAB — major/minor/blues) + how to lay its
// palette out + whether the key reads as minor. Cycling MODE swaps the whole tonal
// system: the palette respells, the strip re-analyses, NEXT re-ranks. The three
// major modes share one vocab (byte-exact old behaviour); MINOR and BLUES ride the
// added vocabs — analysis works because the key is DECLARED, never guessed.
// minor palette DISPLAY order: the everyday pop/rock/lofi chords up top
// (i III iv v VI VII), the diminished + harmonic-minor color on the shelf
// (ii° V vii°). Only the palette layout — the vocab + analysis are untouched.
static const int MINOR_ORDER[HBm_NFUNC] = {
    HBm_i, HBm_III, HBm_iv, HBm_v, HBm_VI, HBm_VII,   // row 1: the minor loop
    HBm_iio, HBm_V, HBm_viio                          // row 2: the color
};

typedef struct {
    const char    *name;      // the mode button label
    const HbStyle *style;     // weights + vocab (NULL vocab = the major default)
    int            row1;      // palette functions on the top row (rest go to row 2)
    const char    *lab1, *lab2;
    const int     *order;     // palette display order (NULL = the vocab's own order)
    int            minorKey;  // 1 → the KEY readout shows "<note>m"
} Mode;
static const Mode MODES[] = {
    { "BOSSA",  &HB_BOSSA,    6, "DIATONIC", "BORROWED", NULL,        0 },
    { "LOUNGE", &HB_COCKTAIL, 6, "DIATONIC", "BORROWED", NULL,        0 },
    { "POP",    &HB_POP,      6, "DIATONIC", "BORROWED", NULL,        0 },
    { "MINOR",  &HB_MINPOP,   6, "MINOR",    "HARMONIC", MINOR_ORDER, 1 },
    { "BLUES",  &HB_BLUES,    3, "12-BAR",   "",         NULL,        0 },
};
#define NMODE ((int)(sizeof MODES / sizeof MODES[0]))
static int modeSel = 0;

// the tonal system of the current mode (NULL style-vocab = the frozen major one)
static const HbVocab *cur_vocab(void) {
    const HbVocab *v = MODES[modeSel].style->vocab;
    return v ? v : &HB_MAJOR;
}
// the function at palette position i (mode's display order, or vocab order)
static int fn_at(int i) {
    const int *o = MODES[modeSel].order;
    return o ? o[i] : i;
}

#define MAXP 8
static int prootv[MAXP], pqual[MAXP], plen = 0;   // the strip: raw chords you entered
static int pfn[MAXP];                             // their functions, re-analyzed (-1 = ?)

static HbOpt sugg[4];                             // the NEXT panel
static int   nsugg = 0;

static int playing = 0, playSlot = 0, playT = 0;
#define BAR_FRAMES 66                             // ~1.1s per chord

// 1 = seventh names + voicings (Cmaj7, the jazz reading); 0 = plain triads
// (C Am G) that also VOICE as triads, so a beginner's tool reads AND sounds basic.
static int seventh = 1;

static void chname(char *out, int n, int rootPc, int q) {
    snprintf(out, n, "%s%s", NOTE[rootPc], seventh ? hb_qname[q] : hb_qtriad[q]);
}

static void sound_chord(int rootPc, int q) {
    int r = 48 + rootPc;
    note(r - 12, I_BSS, 5);
    int voices = seventh ? 4 : 3;               // triad mode drops the 7th/6th
    for (int i = 0; i < voices; i++) note(r + hb_tones[q][i], I_PLK, 5);
}

static void analyze(void) { hb_vocab_analyze(cur_vocab(), keyPc, prootv, pqual, plen, pfn); }

static void resuggest(void) {
    nsugg = 0;
    if (!plen) return;
    int last = pfn[plen - 1];
    if (last < 0 || last >= MODES[modeSel].style->nfunc) return;   // ? chord → no advice (honest)
    nsugg = hb_suggest(MODES[modeSel].style, last, sugg, 4);
}

static void rethink(void) { analyze(); resuggest(); }

static void add_chord(int rootPc, int q) {
    if (plen >= MAXP) return;
    prootv[plen] = rootPc; pqual[plen] = q; plen++;
    rethink();
    sound_chord(rootPc, q);
}
static void add_fn(int f) {
    const HbVocab *v = cur_vocab();
    add_chord((keyPc + v->off[f]) % 12, v->qual[f]);
}

// cold-open with the doo-wop (I vi IV V) so a stranger sees the whole idea —
// strip, numerals, NEXT — before touching anything. Silent: no note until a tap.
static void seed_demo(void) {
    static const int DW[4] = { HB_I, HB_vi, HB_IV, HB_V };
    for (int i = 0; i < 4; i++) {
        prootv[plen] = (keyPc + hb_off[DW[i]]) % 12;
        pqual[plen]  = hb_qual[DW[i]];
        plen++;
    }
    rethink();
}

void init(void) {
    instrument(I_PLK, INSTR_PLUCK, 2, 200, 2, 250);
    instrument(I_BSS, INSTR_TRI, 6, 160, 3, 220);
    seed_demo();
}

void update(void) {
    if (keyp(KEY_LEFT))  { keyPc = (keyPc + 11) % 12; rethink(); }
    if (keyp(KEY_RIGHT)) { keyPc = (keyPc + 1)  % 12; rethink(); }
    if (keyp('B'))       { modeSel = (modeSel + 1) % NMODE; rethink(); }  // vocab may change → re-analyse
    if (keyp('7'))       { seventh = !seventh; }                          // 7ths <-> plain triads
    if (keyp('U') || keyp(KEY_BACKSPACE)) { if (plen) { plen--; rethink(); } }
    { const HbVocab *v = cur_vocab();
      int r1 = MODES[modeSel].row1; if (r1 > v->n) r1 = v->n;
      static const char R1K[6] = { 'Q','W','E','R','T','Y' };
      static const char R2K[7] = { 'A','S','D','F','G','H','J' };
      for (int i = 0; i < r1 && i < 6; i++)          if (keyp(R1K[i])) add_fn(fn_at(i));
      for (int i = 0; i + r1 < v->n && i < 7; i++)   if (keyp(R2K[i])) add_fn(fn_at(r1 + i)); }
    if (keyp('X'))       { plen = 0; playing = 0; rethink(); }
    if (keyp(KEY_SPACE) && plen) { playing = !playing; playSlot = 0; playT = 0; }
    for (int i = 0; i < 4; i++)
        if (keyp('1' + i) && i < nsugg) add_fn(sugg[i].f);

    if (playing && plen) {
        if (playT == 0) sound_chord(prootv[playSlot], pqual[playSlot]);
        if (++playT >= BAR_FRAMES) { playT = 0; playSlot = (playSlot + 1) % plen; }
    } else playSlot = 0;
}

void draw(void) {
    char buf[16];
    cls(CLR_DARKER_BLUE);
    ui_begin();

    // ── header: title + key + mode ──
    print("CHORDWISE", 8, 5, CLR_WHITE);
    font(FONT_SMALL);
    print("next-chord brain", 88, 7, CLR_INDIGO);
    font(FONT_NORMAL);
    if (ui_button(168, 2, 14, 14, "<")) { keyPc = (keyPc + 11) % 12; rethink(); }
    rectfill(184, 2, 54, 14, CLR_BROWNISH_BLACK);
    snprintf(buf, sizeof buf, "KEY %s%s", NOTE[keyPc], MODES[modeSel].minorKey ? "m" : "");
    font(FONT_SMALL);
    print(buf, 188, 6, CLR_YELLOW);
    font(FONT_NORMAL);
    if (ui_button(240, 2, 14, 14, ">")) { keyPc = (keyPc + 1) % 12; rethink(); }
    if (ui_button(260, 2, 54, 14, MODES[modeSel].name)) { modeSel = (modeSel + 1) % NMODE; rethink(); }

    // ── the strip: your progression, re-analyzed every frame ──
    for (int i = 0; i < MAXP; i++) {
        int x = 8 + i * 39, y = 22, w = 36, h = 40;
        int on = i < plen, cur = playing && i == playSlot;
        rectfill(x, y, w, h, cur ? CLR_DARK_BLUE : (on ? CLR_BROWNISH_BLACK : CLR_DARKER_PURPLE));
        rect(x, y, w, h, on ? CLR_DARK_GREY : CLR_DARKER_GREY);
        if (!on) continue;
        chname(buf, sizeof buf, prootv[i], pqual[i]);
        font(FONT_SMALL);
        print(buf, x + 3, y + 4, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
        const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
        int len = (int)strlen(rn), small = len > 4;
        if (small) font(FONT_SMALL);
        int rw = len * (small ? 4 : 8);
        print(rn, x + (w - rw) / 2, y + 18, pfn[i] >= 0 ? CLR_YELLOW : CLR_RED);
        if (small) font(FONT_NORMAL);
    }
    font(FONT_SMALL);
    print(plen ? "your changes, named in the key" : "tap chords below to start",
          8, 65, CLR_INDIGO);
    font(FONT_NORMAL);

    // ── NEXT: the table read forward — ranked, explained ──
    print("NEXT", 8, 76, CLR_WHITE);
    if (!plen) { font(FONT_SMALL); print("(waiting for a chord)", 48, 79, CLR_DARK_GREY); font(FONT_NORMAL); }
    else if (!nsugg) { font(FONT_SMALL); print("? chord - the brain has no map here", 48, 79, CLR_RED); font(FONT_NORMAL); }
    for (int i = 0; i < nsugg; i++) {
        int x = 40 + i * 70, y = 74, w = 66;
        int f = sugg[i].f;
        const HbVocab *v = cur_vocab();
        chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
        if (ui_button(x, y, w, 18, v->fname[f])) add_fn(f);
        for (int p = 0; p < sugg[i].w && p < 6; p++)
            rectfill(x + 2 + p * 5, y + 20, 3, 3, CLR_GREEN);
        font(FONT_SMALL);
        char why[24]; snprintf(why, sizeof why, "%s %s", buf, sugg[i].why);
        print(why, x + 2, y + 25, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }

    // ── the palette: the current vocab spelled in your key ──
    { const HbVocab *v = cur_vocab();
      int r1 = MODES[modeSel].row1; if (r1 > v->n) r1 = v->n;
      int r2 = v->n - r1;
      font(FONT_SMALL); print(MODES[modeSel].lab1, 8, 112, CLR_INDIGO); font(FONT_NORMAL);
      for (int i = 0; i < r1; i++) {
          int f = fn_at(i);
          chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
          if (ui_button(8 + i * 52, 119, 50, 20, buf)) add_fn(f);
      }
      if (r2 > 0) {
          font(FONT_SMALL); print(MODES[modeSel].lab2, 8, 143, CLR_INDIGO); font(FONT_NORMAL);
          for (int i = 0; i < r2; i++) {
              int f = fn_at(r1 + i);
              chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
              if (ui_button(8 + i * 44, 150, 42, 20, buf)) add_fn(f);
          }
      } }

    // ── transport ──
    if (ui_button(8, 178, 40, 16, playing ? "STOP" : "PLAY")) {
        if (plen) { playing = !playing; playSlot = 0; playT = 0; }
    }
    if (ui_button(52, 178, 40, 16, "UNDO")) { if (plen) { plen--; rethink(); } }
    if (ui_button(96, 178, 40, 16, "CLR"))  { plen = 0; playing = 0; rethink(); }
    if (ui_button(140, 178, 48, 16, seventh ? "7THS" : "TRIAD")) seventh = !seventh;
    font(FONT_SMALL);
    print("Q-Y/A-J add . B mode . 7 spell", 192, 183, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}

// ── spec — the deterministic oracle the brief demanded (spec-harness.md) ────
#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(1);                                   // init once
    hb_selfcheck();                            // the header's own assertions

    // the cold-open strip is the doo-wop, correctly named
    expect_eq(plen, 4, "cold-open seeds the doo-wop");
    expect(pfn[0]==HB_I && pfn[1]==HB_vi && pfn[2]==HB_IV && pfn[3]==HB_V,
           "cold-open reads I vi IV V");
    plen = 0; rethink();

    // drive the cart's pure core: enter ii-V-I, read the strip's analysis
    add_fn(HB_ii); add_fn(HB_V); add_fn(HB_I);
    expect_eq(plen, 3, "three chords in the strip");
    expect(pfn[0] == HB_ii && pfn[1] == HB_V && pfn[2] == HB_I, "strip reads ii V I");
    expect(nsugg >= 1 && sugg[0].f == HB_vi, "after I the top pull is vi (relative)");

    // move the key under the same chords: numerals re-derive, honestly.
    // Dm7 G7 Cmaj7 heard in G = v, I7, IV — nothing invented, all re-looked-up.
    keyPc = 7; rethink();
    expect(pfn[0] == HB_v && pfn[1] == HB_I7 && pfn[2] == HB_IV,
           "same chords in G re-read as v I7 IV");

    // LOUNGE (10 functions) can't advise from I7 (function 12) — honest silence
    modeSel = 1; resuggest();
    keyPc = 0; rethink();                      // back home: last chord = I again
    expect(nsugg >= 1, "lounge suggests from I");

    // POP re-ranks the same table: from I the diatonic pull is V (axis start)
    modeSel = 2; resuggest();
    expect(nsugg >= 1 && sugg[0].f == HB_V, "pop from I leans V (the I-V-vi-IV axis)");

    // MINOR mode: the same tap chords re-analyse against the minor vocab, and the
    // minor brain suggests over its own world. Am F C (in A minor) = i VI III.
    modeSel = 3; plen = 0; keyPc = 9; rethink();   // A minor
    add_fn(HBm_i); add_fn(HBm_VI); add_fn(HBm_III);
    expect(pfn[0] == HBm_i && pfn[1] == HBm_VI && pfn[2] == HBm_III,
           "minor strip reads i VI III in A minor");
    expect(nsugg >= 1, "minor mode suggests from III");
    // a chord outside the minor vocab is honest: F#m7 has no seat in A minor
    add_chord(6, HBQ_MIN7);
    expect_eq(pfn[3], -1, "F#m7 in A minor -> ? (outside the vocab, not guessed)");

    // BLUES mode: A7 D7 E7 in A = I7 IV7 V7 (all dominant, the 12-bar world)
    modeSel = 4; plen = 0; keyPc = 9; rethink();
    add_fn(HBbl_I); add_fn(HBbl_IV); add_fn(HBbl_V);
    expect(pfn[0] == HBbl_I && pfn[1] == HBbl_IV && pfn[2] == HBbl_V,
           "blues strip reads I7 IV7 V7 in A");

    // TRIAD display: the 7 toggle spells (and voices) plain triads instead of 7ths
    { char a[16], b[16];
      seventh = 1; chname(a, sizeof a, 0, HBQ_MAJ7);   // C
      seventh = 0; chname(b, sizeof b, 0, HBQ_MAJ7);
      expect(!strcmp(a, "Cmaj7") && !strcmp(b, "C"), "spell: Cmaj7 <-> C (triad toggle)");
      chname(a, sizeof a, 9, HBQ_MIN7);                // Am (drop the m7)
      expect(!strcmp(a, "Am"), "spell: A minor7 as a triad = Am");
      chname(a, sizeof a, 11, HBQ_M7B5);               // Bdim (the diminished triad)
      expect(!strcmp(a, "Bdim"), "spell: B m7b5 as a triad = Bdim");
      seventh = 1; }

    // undo + clear keep the machine consistent (back in the major default)
    modeSel = 0; plen = 0; keyPc = 0;
    add_fn(HB_ii); add_fn(HB_V); add_fn(HB_I);
    plen--; rethink();                         // drop the I → last chord = V again
    expect(nsugg >= 1 && sugg[0].f == HB_I, "after V the top pull is I (home)");
    plen = 0; rethink();
    expect_eq(nsugg, 0, "empty strip = no advice");
}
#endif
