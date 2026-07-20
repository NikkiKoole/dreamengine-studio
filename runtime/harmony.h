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
// the chord TONES of each quality (root, 3rd, 5th, 7th/6th as semitone
// intervals) — what a chord IS, for a melody/solo to target. Five carts each
// hand-rolled this exact table (bossa QTONES, cocktail/squarepusher CT, …);
// it lives here now. Voicing (which tones to omit/extend) stays rad_lead_to.
static const int hb_tones[HB_NQUAL][4] = {
    { 0, 4, 7, 11 },   // maj7
    { 0, 3, 7, 10 },   // m7
    { 0, 4, 7, 10 },   // 7
    { 0, 3, 6, 10 },   // m7b5
    { 0, 3, 7, 9  },   // m6
};

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

// the chord TONES of function f in key keyPc, as pitch classes (0..11) → out[4].
// The shared version of the `chord_pcs()` helper cocktail/squarepusher hand-roll:
// root = keyPc + hb_off[f]; tones = hb_tones[hb_qual[f]]. Returns the count (4).
static int hb_chord_pcs(int keyPc, int f, int *out) {
    int r = (keyPc + hb_off[f]) % 12;
    for (int i = 0; i < 4; i++) out[i] = (r + hb_tones[hb_qual[f]][i]) % 12;
    return 4;
}

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

// a POP style — the same 13-function vocab, weighted for the diatonic loops a
// stranger already hums: the I-V-vi-IV "axis" and the 50s doo-wop (I-vi-IV-V).
// The point of the research made audible: pop is the SAME grammar as bossa with
// the weight piled onto the six diatonic seats — the borrowed shelf still
// resolves home (bVII7/iv->I, the rock cadences) but rarely gets reached. Not
// consumed by any radio cart, so these rows carry no pinned-seed contract.
static const int HB_P_I[8]    = { HB_V, HB_V, HB_V, HB_vi, HB_vi, HB_IV, HB_IV, HB_ii };
static const int HB_P_ii[5]   = { HB_V, HB_V, HB_V, HB_IV, HB_I };
static const int HB_P_iii[4]  = { HB_vi, HB_vi, HB_IV, HB_ii };
static const int HB_P_IV[5]   = { HB_I, HB_I, HB_V, HB_V, HB_vi };
static const int HB_P_V[6]    = { HB_I, HB_I, HB_I, HB_vi, HB_vi, HB_IV };
static const int HB_P_vi[6]   = { HB_IV, HB_IV, HB_IV, HB_V, HB_ii, HB_I };
static const int HB_P_II7[2]  = { HB_V, HB_V };
static const int HB_P_VI7[2]  = { HB_ii, HB_ii };
static const int HB_P_bII7[1] = { HB_I };
static const int HB_P_iv[2]   = { HB_I, HB_I };
static const int HB_P_bVII7[3]= { HB_I, HB_I, HB_I };
static const int HB_P_v[1]    = { HB_I };
static const int HB_P_I7[1]   = { HB_IV };
static const int *const HB_POP_NEXT[HB_NFUNC] = { HB_P_I, HB_P_ii, HB_P_iii,
    HB_P_IV, HB_P_V, HB_P_vi, HB_P_II7, HB_P_VI7, HB_P_bII7, HB_P_iv,
    HB_P_bVII7, HB_P_v, HB_P_I7 };
static const int HB_POP_N[HB_NFUNC] = { 8, 5, 4, 5, 6, 6, 2, 2, 1, 2, 3, 1, 1 };
static const HbStyle HB_POP = { HB_POP_NEXT, HB_POP_N, HB_NFUNC };

// ── generate: the cart keeps its PRNG stream (pinned seeds stay byte-exact) ──
static int hb_nopts(const HbStyle *st, int f) { return st->n[f]; }          // pass to your srnd()
static int hb_pick (const HbStyle *st, int f, int r) { return st->next[f][r]; }

// ── suggest: the same table read FORWARD — ranked options for a UI ──────────
// why one chord follows another, in a word or two (the toy's explainer)
static const char *hb_reason(int from, int to) {
    if (to == HB_I  && from == HB_V)                       return "home";
    if (to == HB_I  && from == HB_bII7)                    return "sub home";
    if (to == HB_I  && (from == HB_bVII7 || from == HB_iv)) return "backdoor";
    if (to == HB_I  && from == HB_IV)                      return "plagal";
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

// ── self-check — the spec.h "specs on an includeable" pattern ────────────────
// A shared header can't define spec() (one per cart), but it can carry its own
// assertions: a cart's spec() just calls hb_selfcheck(). Lives only under
// -DDE_SPEC (the `node tools/spec.js` build); a normal build pays nothing.
#ifdef DE_SPEC
#include "spec.h"
static inline void hb_selfcheck(void) {
    // (off, quality) is unique across the 13 functions → analysis inverts
    // generation exactly: every function's own spelling re-analyzes to itself
    int ok = 1;
    for (int k = 0; k < 12; k++)
        for (int f = 0; f < HB_NFUNC; f++)
            ok &= hb_chord_fn(k, (k + hb_off[f]) % 12, hb_qual[f]) == f;
    expect(ok, "hb round-trip: function -> chord -> same function, all 12 keys");

    // the doo-wop triads in C: C Am F G -> I vi IV V
    { int rp[4] = { 0, 9, 5, 7 };
      int q[4]  = { HB_TRIAD_MAJ, HB_TRIAD_MIN, HB_TRIAD_MAJ, HB_TRIAD_MAJ };
      int f[4];
      expect_eq(hb_analyze(0, rp, q, 4, f), 4, "hb doo-wop: all four in vocab");
      expect(f[0]==HB_I && f[1]==HB_vi && f[2]==HB_IV && f[3]==HB_V,
             "hb doo-wop = I vi IV V"); }

    // ii-V-I in sevenths: Dm7 G7 Cmaj7
    { int rp[3] = { 2, 7, 0 }, q[3] = { HBQ_MIN7, HBQ_DOM7, HBQ_MAJ7 }, f[3];
      hb_analyze(0, rp, q, 3, f);
      expect(f[0]==HB_ii && f[1]==HB_V && f[2]==HB_I, "hb ii-V-I named"); }

    // the borrowed shelf in C: Db7=tritone sub, Bb7=backdoor, Fm=iv, C7=I7, Gm7=v
    expect_eq(hb_chord_fn(0, 1,  HBQ_DOM7),    HB_bII7,  "hb Db7 -> bII7");
    expect_eq(hb_chord_fn(0, 10, HBQ_DOM7),    HB_bVII7, "hb Bb7 -> bVII7");
    expect_eq(hb_chord_fn(0, 5,  HB_TRIAD_MIN), HB_iv,   "hb Fm -> iv");
    expect_eq(hb_chord_fn(0, 0,  HBQ_DOM7),    HB_I7,    "hb C7 -> I7");
    expect_eq(hb_chord_fn(0, 7,  HBQ_MIN7),    HB_v,     "hb Gm7 -> v");
    // honesty: a chord outside the vocab says so instead of guessing
    expect_eq(hb_chord_fn(0, 6,  HBQ_MIN7),    -1,       "hb F#m7 in C -> outside vocab");

    // suggest = the table read forward, ranked: bossa's ii goes V (w5) then bII7 (w2)
    { HbOpt o[4]; int n = hb_suggest(&HB_BOSSA, HB_ii, o, 4);
      expect_eq(n, 2, "hb bossa ii: two candidates");
      expect(o[0].f==HB_V    && o[0].w==5, "hb ii -> V first, weight 5");
      expect(o[1].f==HB_bII7 && o[1].w==2, "hb ii -> bII7 second, weight 2"); }

    // pick is a pure table read (the carts' PRNG stays outside)
    expect_eq(hb_pick(&HB_BOSSA, HB_ii, 0),    HB_V,  "hb pick = table read");
    expect_eq(hb_pick(&HB_COCKTAIL, HB_VI7, 3), HB_ii, "hb cocktail VI7 row -> ii");

    // chord tones: ii in C = Dm7 (D F A C); V = G7 (G B D F) — the shared table
    { int t[4];
      hb_chord_pcs(0, HB_ii, t);
      expect(t[0]==2 && t[1]==5 && t[2]==9 && t[3]==0, "hb chord_pcs: ii in C = Dm7 (D F A C)");
      hb_chord_pcs(0, HB_V, t);
      expect(t[0]==7 && t[1]==11 && t[2]==2 && t[3]==5, "hb chord_pcs: V in C = G7 (G B D F)");
      // spelling round-trips with analysis: each function's tones re-analyze to it
      expect_eq(hb_chord_fn(0, (0+hb_off[HB_vi])%12, HB_TRIAD_MIN), HB_vi,
                "hb chord_pcs root matches hb_chord_fn"); }
}
#endif // DE_SPEC

#endif // HARMONY_H
