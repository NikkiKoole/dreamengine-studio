// harmony.h — the shared HARMONY BRAIN: one function vocabulary + next-chord
// transition model, bidirectional. Extracted from bossa.c / cocktail.c (which had
// each grown their own copy of the same functional-Markov engine — the acid303.h
// move, for harmony). The cart owns its PRNG + form (cadence forcing, section
// walks); this owns the VOCAB (what a "ii" is), the STYLE TABLES (where each
// function likes to go), and the two new directions the carts never had:
// SUGGEST (read the table forward, ranked options for a UI) and ANALYZE (the
// inverse — chords in a KNOWN key → roman-numeral functions).
//
// The model, per the deep-research survey in docs/design/harmony-brain.md:
// 1st-order Markov over key-relative roman-numeral functions, cadences forced by
// the caller (bossa's bars 6-7), genre = different WEIGHTS over this ONE vocab.
// Analysis takes the key as INPUT (full auto roman-numeral analysis is unsolved);
// it is a diatonic + borrowed-chord lookup, deterministic → spec()-able.
//
//   #include "harmony.h"
//   // generate (a radio): the cart's PRNG stays in charge — byte-identical seeds
//   int f = hb_pick(&HB_BOSSA, prev, srnd(hb_nopts(&HB_BOSSA, prev)));
//   // suggest (a toy): ranked next-chord options + a one-word reason each
//   HbOpt o[4]; int n = hb_suggest(&HB_BOSSA, HB_ii, o, 4);   // o[0] = {HB_V, 5, "cadence"}
//   // analyze (the inverse): chords in a given key → functions (-1 = outside the vocab)
//   int f2 = hb_chord_fn(0 /*key C*/, 9 /*A*/, HB_TRIAD_MIN);  // → HB_vi
//
// Voicing is NOT here — rad_lead_to (radio.h) stays the voicing layer; this only
// PICKS/NAMES chords. Pitches: root pc = (keyPc + hb_off[f]) % 12, quality = hb_qual[f].

#ifndef HARMONY_H
#define HARMONY_H

// ── chord qualities (the shared 5-quality vocab both source carts used) ──────
enum { HBQ_MAJ7, HBQ_MIN7, HBQ_DOM7, HBQ_M7B5, HBQ_MIN6, HB_NQUAL };
static const char *hb_qname[HB_NQUAL] = { "maj7", "m7", "7", "m7b5", "m6" };

// plain-triad classes for hb_chord_fn — a toy speaks triads, the vocab thinks
// sevenths; a triad matches its seventh family (maj → maj7/dom7, min → m7/m6)
enum { HB_TRIAD_MAJ = 100, HB_TRIAD_MIN, HB_TRIAD_DIM };

// ── harmonic functions: {scale-degree offset, quality}, key-relative ─────────
// bossa's 13-function vocab; cocktail speaks the first 10. Order is FROZEN —
// both carts' pinned seeds index these values.
enum { HB_I, HB_ii, HB_iii, HB_IV, HB_V, HB_vi, HB_II7, HB_VI7, HB_bII7, HB_iv,
       HB_bVII7, HB_v, HB_I7, HB_NFUNC };
static const int hb_off[HB_NFUNC]  = { 0, 2, 4, 5, 7, 9, 2, 9, 1, 5, 10, 7, 0 };
static const int hb_qual[HB_NFUNC] = { HBQ_MAJ7, HBQ_MIN7, HBQ_MIN7, HBQ_MAJ7,
                                       HBQ_DOM7, HBQ_MIN7, HBQ_DOM7, HBQ_DOM7,
                                       HBQ_DOM7, HBQ_MIN6, HBQ_DOM7, HBQ_MIN7,
                                       HBQ_DOM7 };
static const char *hb_fname[HB_NFUNC] = { "I", "ii", "iii", "IV", "V", "vi",
    "II7", "VI7", "bII7", "iv", "bVII7", "v", "I7" };

// ── styles: where can each function go? repeats = more likely ────────────────
// A style is weights over the ONE vocab (the research finding: genres differ by
// weights, not grammars). Row length is part of a cart's PRNG contract — never
// reorder/resize a shipped row (pinned seeds break silently).
typedef struct {
    const int *const *next;   // per-function candidate list (repeats = weight)
    const int        *n;      // per-function list length (the srnd() argument)
    int               nfunc;  // how much of the vocab this style speaks
} HbStyle;

// bossa.c's TRANS — the jazz cheat-sheet: ii→V→I, secondary dominants resolve
// down a fifth, bII7 = tritone sub of V, iv/bVII7 = backdoor, v→I7 = ii-V of IV.
#define HB_T(name, ...) static const int name[] = { __VA_ARGS__ };
HB_T(HB_B_I,     HB_vi, HB_vi, HB_vi, HB_II7, HB_II7, HB_IV, HB_IV, HB_iii, HB_iii, HB_VI7, HB_VI7, HB_v)
HB_T(HB_B_ii,    HB_V, HB_V, HB_V, HB_V, HB_V, HB_bII7, HB_bII7)
HB_T(HB_B_iii,   HB_VI7, HB_VI7, HB_VI7, HB_vi, HB_vi, HB_ii)
HB_T(HB_B_IV,    HB_iv, HB_iv, HB_iv, HB_ii, HB_ii, HB_I, HB_I, HB_bVII7, HB_bVII7)
HB_T(HB_B_V,     HB_I, HB_I, HB_I, HB_I, HB_I, HB_iii, HB_vi)
HB_T(HB_B_vi,    HB_ii, HB_ii, HB_ii, HB_ii, HB_II7, HB_iv)
HB_T(HB_B_II7,   HB_ii, HB_ii, HB_ii, HB_V, HB_V)
HB_T(HB_B_VI7,   HB_ii, HB_ii, HB_ii, HB_ii, HB_ii)
HB_T(HB_B_bII7,  HB_I, HB_I, HB_I, HB_I, HB_I)
HB_T(HB_B_iv,    HB_bVII7, HB_bVII7, HB_bVII7, HB_I, HB_I)
HB_T(HB_B_bVII7, HB_I, HB_I, HB_I, HB_I)
HB_T(HB_B_v,     HB_I7, HB_I7, HB_I7, HB_I7)
HB_T(HB_B_I7,    HB_IV, HB_IV, HB_IV, HB_IV)
#undef HB_T
static const int *const HB_BOSSA_NEXT[HB_NFUNC] = { HB_B_I, HB_B_ii, HB_B_iii,
    HB_B_IV, HB_B_V, HB_B_vi, HB_B_II7, HB_B_VI7, HB_B_bII7, HB_B_iv,
    HB_B_bVII7, HB_B_v, HB_B_I7 };
static const int HB_BOSSA_N[HB_NFUNC] = { 12, 7, 6, 9, 7, 6, 5, 5, 5, 5, 4, 4, 4 };
static const HbStyle HB_BOSSA = { HB_BOSSA_NEXT, HB_BOSSA_N, HB_NFUNC };

// cocktail.c's FNEXT — the lounge trio's tuning of the same grammar (fixed
// 6-wide rows: its PRNG contract is srnd(6) per step). Speaks 10 functions.
static const int HB_C_I[6]    = { HB_vi, HB_ii, HB_IV, HB_iii, HB_VI7, HB_ii };
static const int HB_C_ii[6]   = { HB_V, HB_V, HB_V, HB_bII7, HB_V, HB_iii };
static const int HB_C_iii[6]  = { HB_vi, HB_VI7, HB_ii, HB_vi, HB_IV, HB_vi };
static const int HB_C_IV[6]   = { HB_iv, HB_ii, HB_I, HB_V, HB_ii, HB_iii };
static const int HB_C_V[6]    = { HB_I, HB_I, HB_I, HB_vi, HB_iii, HB_I };
static const int HB_C_vi[6]   = { HB_ii, HB_II7, HB_ii, HB_iv, HB_ii, HB_VI7 };
static const int HB_C_II7[6]  = { HB_V, HB_V, HB_V, HB_ii, HB_V, HB_V };
static const int HB_C_VI7[6]  = { HB_ii, HB_ii, HB_ii, HB_ii, HB_ii, HB_ii };
static const int HB_C_bII7[6] = { HB_I, HB_I, HB_I, HB_I, HB_iii, HB_I };
static const int HB_C_iv[6]   = { HB_I, HB_I, HB_bII7, HB_I, HB_I, HB_I };
static const int *const HB_COCKTAIL_NEXT[10] = { HB_C_I, HB_C_ii, HB_C_iii,
    HB_C_IV, HB_C_V, HB_C_vi, HB_C_II7, HB_C_VI7, HB_C_bII7, HB_C_iv };
static const int HB_COCKTAIL_N[10] = { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 };
static const HbStyle HB_COCKTAIL = { HB_COCKTAIL_NEXT, HB_COCKTAIL_N, 10 };

// ── generate: the cart keeps its PRNG stream (pinned seeds stay byte-exact) ──
static int hb_nopts(const HbStyle *st, int f) { return st->n[f]; }          // pass to your srnd()
static int hb_pick (const HbStyle *st, int f, int r) { return st->next[f][r]; }

// ── suggest: the same table read FORWARD — ranked options for a UI ──────────
// why one chord follows another, in a word or two (the toy's explainer)
static const char *hb_reason(int from, int to) {
    if (to == HB_I  && from == HB_V)                       return "home";
    if (to == HB_I  && from == HB_bII7)                    return "sub home";
    if (to == HB_I  && (from == HB_bVII7 || from == HB_iv)) return "backdoor";
    if (to == HB_V  && from == HB_ii)                      return "cadence";
    if (to == HB_ii && (from == HB_VI7 || from == HB_II7)) return "5th down";
    if (to == HB_V  && from == HB_II7)                     return "5th down";
    if (to == HB_I7 && from == HB_v)                       return "to IV";
    if (to == HB_IV && from == HB_I7)                      return "resolve";
    if (to == HB_ii)                                       return "pre-V";
    if (to == HB_iv || to == HB_bVII7)                     return "borrow";
    if (to == HB_vi && from == HB_I)                       return "relative";
    if (to == HB_bII7)                                     return "tritone sub";
    if (to == HB_II7 || to == HB_VI7)                      return "sec dom";
    return "walk";
}

typedef struct { int f, w; const char *why; } HbOpt;   // function, weight, reason

// weighted next-chord candidates from `from`, best first (weight = repeat count
// in the style row). Returns how many were written (≤ max). ~2-4 is the sweet
// spot per the research (93% of real next-chords live in the top 2).
static int hb_suggest(const HbStyle *st, int from, HbOpt *out, int max) {
    if (from < 0 || from >= st->nfunc || max <= 0) return 0;
    int w[HB_NFUNC] = { 0 };
    for (int i = 0; i < st->n[from]; i++) w[st->next[from][i]]++;
    int cnt = 0;
    for (;;) {
        int best = -1;
        for (int f = 0; f < HB_NFUNC; f++)
            if (w[f] > 0 && (best < 0 || w[f] > w[best])) best = f;
        if (best < 0 || cnt >= max) break;
        out[cnt].f = best; out[cnt].w = w[best]; out[cnt].why = hb_reason(from, best);
        cnt++; w[best] = 0;
    }
    return cnt;
}

// ── analyze: the INVERSE — chords in a KNOWN key → functions ─────────────────
// qual = an HBQ_* (exact seventh) or an HB_TRIAD_* class. Diatonic functions win
// ties (enum order: the 6 diatonic seats come first). Returns -1 = outside the
// 13-function vocab (a genuinely chromatic chord — honest, not guessed).
static int hb_quality_matches(int fq, int qual) {
    if (qual < HB_TRIAD_MAJ) return fq == qual;
    if (qual == HB_TRIAD_MAJ) return fq == HBQ_MAJ7 || fq == HBQ_DOM7;
    if (qual == HB_TRIAD_MIN) return fq == HBQ_MIN7 || fq == HBQ_MIN6;
    return fq == HBQ_M7B5;                              // HB_TRIAD_DIM
}
static int hb_chord_fn(int keyPc, int rootPc, int qual) {
    int off = (rootPc - keyPc + 120) % 12;
    for (int f = 0; f < HB_NFUNC; f++)
        if (hb_off[f] == off && hb_quality_matches(hb_qual[f], qual)) return f;
    return -1;
}
// whole progression at once → outF[i] = function or -1; returns how many matched
static int hb_analyze(int keyPc, const int *rootPc, const int *qual, int n, int *outF) {
    int hit = 0;
    for (int i = 0; i < n; i++) {
        outF[i] = hb_chord_fn(keyPc, rootPc[i], qual[i]);
        if (outF[i] >= 0) hit++;
    }
    return hit;
}

#endif // HARMONY_H
