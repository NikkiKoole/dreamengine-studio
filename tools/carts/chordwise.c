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
    "summary": "Build a chord progression by tapping chords in a key - the strip names each one in roman numerals (I, ii, V...) and a NEXT panel suggests where the harmony wants to go, ranked, each with a one-word reason (cadence, home, tritone sub). Change the key and watch the same chords get re-named - or turn un-nameable (?). SPACE loops your changes on a soft pluck.",
    "detail": "The demand-82 toy: a progression analyzer + next-chord suggester on the shared harmony brain (runtime/harmony.h, lifted byte-identically from the bossa/cocktail radio stations). The chord palette shows the 13-function jazz vocabulary spelled in your key: the 6 diatonic seats on the top row, the borrowed/chromatic shelf below (secondary dominants II7/VI7, the tritone sub bII7, the backdoor iv/bVII7, the minor v and I7). Tap chords into an 8-slot strip; every frame the strip is RE-ANALYZED from the raw chords (root + quality), so the roman numerals are honest lookups, not echoes of what you pressed - move the key under a finished progression and Cmaj7 quietly turns from I into IV, and chords outside the vocabulary show ? instead of a guess (full auto analysis is unsolved; this toy stays in a declared key on purpose). The NEXT panel reads the same Markov table that composes bossa's songs FORWARD: ranked candidates with weight pips and a one-word reason each. Two styles (BOSSA = the 13-function jazz table, LOUNGE = cocktail's 10-function trio tuning) re-rank the suggestions. The whole brain is deterministic and carries a spec() (round-trip: every function's spelling re-analyzes to itself in all 12 keys; doo-wop = I vi IV V; ii-V-I; the borrowed shelf).",
    "controls": "Tap a palette chord to add it (it plays) - or type it: Q W E R T Y = the diatonic row, A S D F G H J = the borrowed row. Tap a NEXT suggestion (or keys 1-4) to follow the brain. SPACE play/stop the loop, U or BACKSPACE undo, X clear. LEFT/RIGHT change the key, B toggle style (BOSSA/LOUNGE)."
  }
}
de:meta */
// chordwise — the progression analyzer + next-chord suggester (demand-82).
//
// The first cart on runtime/harmony.h, exercising all three directions:
//   ANALYZE  — the strip re-derives roman numerals from raw (root, quality)
//              every frame via hb_analyze; the key is an INPUT (declared, not
//              guessed — full auto RN analysis is unsolved, see the brief).
//   SUGGEST  — hb_suggest reads the generation table forward: ranked next
//              chords + a one-word reason (cadence / home / tritone sub...).
//   GENERATE — untouched here; bossa/cocktail keep composing on the same table.
//
// Honesty rules the toy inherits from the research (design/harmony-brain.md):
// a chord outside the 13-function vocab shows "?" (never a guess), and the
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
// chord tones per quality (root + 3rd + 5th + 7th/6th) — SOUND only; the
// naming/choosing lives in harmony.h (spelling stays the vocab's sevenths)
static const int QTONES[HB_NQUAL][4] = {
    { 0, 4, 7, 11 },   // maj7
    { 0, 3, 7, 10 },   // m7
    { 0, 4, 7, 10 },   // 7
    { 0, 3, 6, 10 },   // m7b5
    { 0, 3, 7, 9  },   // m6
};

static int keyPc = 0;
static const HbStyle *STYLES[2]  = { &HB_BOSSA, &HB_COCKTAIL };
static const char    *STYNAME[2] = { "BOSSA", "LOUNGE" };
static int styleSel = 0;

#define MAXP 8
static int prootv[MAXP], pqual[MAXP], plen = 0;   // the strip: raw chords you entered
static int pfn[MAXP];                             // their functions, re-analyzed (-1 = ?)

static HbOpt sugg[4];                             // the NEXT panel
static int   nsugg = 0;

static int playing = 0, playSlot = 0, playT = 0;
#define BAR_FRAMES 66                             // ~1.1s per chord

// palette layout: the diatonic row, then the borrowed/chromatic shelf
static const int ROW1[6] = { HB_I, HB_ii, HB_iii, HB_IV, HB_V, HB_vi };
static const int ROW2[7] = { HB_II7, HB_VI7, HB_bII7, HB_iv, HB_bVII7, HB_v, HB_I7 };

static void chname(char *out, int n, int rootPc, int q) {
    snprintf(out, n, "%s%s", NOTE[rootPc], hb_qname[q]);
}

static void sound_chord(int rootPc, int q) {
    int r = 48 + rootPc;
    note(r - 12, I_BSS, 5);
    for (int i = 0; i < 4; i++) note(r + QTONES[q][i], I_PLK, 5);
}

static void analyze(void) { hb_analyze(keyPc, prootv, pqual, plen, pfn); }

static void resuggest(void) {
    nsugg = 0;
    if (!plen) return;
    int last = pfn[plen - 1];
    if (last < 0 || last >= STYLES[styleSel]->nfunc) return;   // ? chord → no advice (honest)
    nsugg = hb_suggest(STYLES[styleSel], last, sugg, 4);
}

static void rethink(void) { analyze(); resuggest(); }

static void add_chord(int rootPc, int q) {
    if (plen >= MAXP) return;
    prootv[plen] = rootPc; pqual[plen] = q; plen++;
    rethink();
    sound_chord(rootPc, q);
}
static void add_fn(int f) { add_chord((keyPc + hb_off[f]) % 12, hb_qual[f]); }

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
    if (keyp('B'))       { styleSel ^= 1; resuggest(); }
    if (keyp('U') || keyp(KEY_BACKSPACE)) { if (plen) { plen--; rethink(); } }
    { static const char R1K[6] = { 'Q','W','E','R','T','Y' };
      static const char R2K[7] = { 'A','S','D','F','G','H','J' };
      for (int i = 0; i < 6; i++) if (keyp(R1K[i])) add_fn(ROW1[i]);
      for (int i = 0; i < 7; i++) if (keyp(R2K[i])) add_fn(ROW2[i]); }
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

    // ── header: title + key + style ──
    print("CHORDWISE", 8, 5, CLR_WHITE);
    font(FONT_SMALL);
    print("next-chord brain", 88, 7, CLR_INDIGO);
    font(FONT_NORMAL);
    if (ui_button(168, 2, 14, 14, "<")) { keyPc = (keyPc + 11) % 12; rethink(); }
    snprintf(buf, sizeof buf, "KEY %s", NOTE[keyPc]);
    rectfill(184, 2, 54, 14, CLR_BROWNISH_BLACK);
    print(buf, 188, 5, CLR_YELLOW);
    if (ui_button(240, 2, 14, 14, ">")) { keyPc = (keyPc + 1) % 12; rethink(); }
    if (ui_button(260, 2, 54, 14, STYNAME[styleSel])) { styleSel ^= 1; resuggest(); }

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
        const char *rn = pfn[i] >= 0 ? hb_fname[pfn[i]] : "?";
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
        chname(buf, sizeof buf, (keyPc + hb_off[f]) % 12, hb_qual[f]);
        if (ui_button(x, y, w, 18, hb_fname[f])) add_fn(f);
        for (int p = 0; p < sugg[i].w && p < 6; p++)
            rectfill(x + 2 + p * 5, y + 20, 3, 3, CLR_GREEN);
        font(FONT_SMALL);
        char why[24]; snprintf(why, sizeof why, "%s %s", buf, sugg[i].why);
        print(why, x + 2, y + 25, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }

    // ── the palette: the vocab spelled in your key ──
    font(FONT_SMALL); print("DIATONIC", 8, 112, CLR_INDIGO); font(FONT_NORMAL);
    for (int i = 0; i < 6; i++) {
        int f = ROW1[i];
        chname(buf, sizeof buf, (keyPc + hb_off[f]) % 12, hb_qual[f]);
        if (ui_button(8 + i * 52, 119, 50, 20, buf)) add_fn(f);
    }
    font(FONT_SMALL); print("BORROWED", 8, 143, CLR_INDIGO); font(FONT_NORMAL);
    for (int i = 0; i < 7; i++) {
        int f = ROW2[i];
        chname(buf, sizeof buf, (keyPc + hb_off[f]) % 12, hb_qual[f]);
        if (ui_button(8 + i * 44, 150, 42, 20, buf)) add_fn(f);
    }

    // ── transport ──
    if (ui_button(8, 178, 44, 16, playing ? "STOP" : "PLAY")) {
        if (plen) { playing = !playing; playSlot = 0; playT = 0; }
    }
    if (ui_button(56, 178, 44, 16, "UNDO")) { if (plen) { plen--; rethink(); } }
    if (ui_button(104, 178, 44, 16, "CLR"))  { plen = 0; playing = 0; rethink(); }
    font(FONT_SMALL);
    print("Q-Y A-J chords . 1-4 follow . B style", 156, 183, CLR_DARK_GREY);
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
    styleSel = 1; resuggest();
    keyPc = 0; rethink();                      // back home: last chord = I again
    expect(nsugg >= 1, "lounge suggests from I");
    styleSel = 0;

    // undo + clear keep the machine consistent
    plen--; rethink();
    expect(nsugg >= 1 && sugg[0].f == HB_I, "after V the top pull is I (home)");
    plen = 0; rethink();
    expect_eq(nsugg, 0, "empty strip = no advice");
}
#endif
