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
// the plain-TRIAD name per quality — the beginner-facing spelling that drops the
// 7th (C, Am, G, Bdim). Same order/count as hb_qname; a dominant's triad is major.
static const char *hb_qtriad[HB_NQUAL] = { "", "m", "", "dim", "m" };
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

// ── a VOCAB: the tonal SYSTEM a style sits on (major / minor / blues). The major
// vocab (hb_off/hb_qual/hb_fname) is the frozen default; minor & blues are
// ADDITIVE parallels so a beginner's tool covers the basics — sad-pop, EDM, rock,
// lofi (minor) and the 12-bar (blues) — without touching a single pinned seed.
typedef struct {
    const int         *off;    // scale-degree offset per function (from the tonic)
    const int         *qual;   // quality per function (HBQ_*)
    const char *const *fname;  // roman-numeral name per function
    int                n;      // how many functions this vocab has
    const char      *(*reason)(int from, int to);   // the motion explainer
} HbVocab;

// chord tones of function f (vocab v, key keyPc) as pitch classes → out[4]
static int hb_vocab_pcs(const HbVocab *v, int keyPc, int f, int *out) {
    int r = (keyPc + v->off[f]) % 12;
    for (int i = 0; i < 4; i++) out[i] = (r + hb_tones[v->qual[f]][i]) % 12;
    return 4;
}

// ── the MINOR-key vocab — the missing half of tonal music (the sad-pop/EDM/rock/
// lofi tribe that ASKED for this). Offsets from the MINOR tonic; natural minor
// plus the harmonic-minor V (both the soft v and the leading-tone V/vii°). Roman
// numerals follow minor-key convention (III/VI/VII already mean the flat degrees).
enum { HBm_i, HBm_iio, HBm_III, HBm_iv, HBm_v, HBm_V, HBm_VI, HBm_VII, HBm_viio, HBm_NFUNC };
static const int hbm_off[HBm_NFUNC]  = { 0, 2, 3, 5, 7, 7, 8, 10, 11 };
static const int hbm_qual[HBm_NFUNC] = { HBQ_MIN7, HBQ_M7B5, HBQ_MAJ7, HBQ_MIN7,
                                         HBQ_MIN7, HBQ_DOM7, HBQ_MAJ7, HBQ_MAJ7, HBQ_M7B5 };
static const char *hbm_fname[HBm_NFUNC] = { "i","ii\xf8","III","iv","v","V","VI","VII","vii\xf8" };

// ── the BLUES vocab — I7 IV7 V7, all dominant (not major-diatonic, not minor).
// The 12-bar's whole world; the major vocab has no IV7, so blues gets its own.
enum { HBbl_I, HBbl_IV, HBbl_V, HBbl_NFUNC };
static const int hbbl_off[HBbl_NFUNC]  = { 0, 5, 7 };
static const int hbbl_qual[HBbl_NFUNC] = { HBQ_DOM7, HBQ_DOM7, HBQ_DOM7 };
static const char *hbbl_fname[HBbl_NFUNC] = { "I7", "IV7", "V7" };

// ── the DORIAN vocab — minor with a BRIGHT MAJOR IV (the lo-fi / neo-soul / funk
// "So What" sound). Natural minor's flat 3/6/7, but the raised 6th makes iv into
// a MAJOR IV — the one chord that separates dorian from plain minor, and the
// whole point of the i↔IV vamp. Diatonic 7ths off the (minor) tonic.
enum { HBd_i, HBd_ii, HBd_III, HBd_IV, HBd_v, HBd_vio, HBd_VII, HBd_NFUNC };
static const int hbd_off[HBd_NFUNC]  = { 0, 2, 3, 5, 7, 9, 10 };
static const int hbd_qual[HBd_NFUNC] = { HBQ_MIN7, HBQ_MIN7, HBQ_MAJ7, HBQ_DOM7,
                                         HBQ_MIN7, HBQ_M7B5, HBQ_MAJ7 };
static const char *hbd_fname[HBd_NFUNC] = { "i","ii","III","IV","v","vi\xf8","VII" };

// ── the MIXOLYDIAN vocab — MAJOR with a flat 7 (rock/funk/folk). The tonic is a
// dominant (I7) and the tell is the major bVII: the bVII-IV-I "mixolydian
// cadence" (Sweet Home Alabama, most of rock). Diatonic 7ths off the tonic.
enum { HBmx_I, HBmx_ii, HBmx_iiio, HBmx_IV, HBmx_v, HBmx_vi, HBmx_bVII, HBmx_NFUNC };
static const int hbmx_off[HBmx_NFUNC]  = { 0, 2, 4, 5, 7, 9, 10 };
static const int hbmx_qual[HBmx_NFUNC] = { HBQ_DOM7, HBQ_MIN7, HBQ_M7B5, HBQ_MAJ7,
                                           HBQ_MIN7, HBQ_MIN7, HBQ_MAJ7 };
static const char *hbmx_fname[HBmx_NFUNC] = { "I","ii","iii\xf8","IV","v","vi","bVII" };

// ── the PHRYGIAN vocab — MINOR with a flat 2 (metal / flamenco / "Spanish"). The
// tell is the major bII a half-step above the tonic: the bII-i "Phrygian cadence"
// (the Andalusian pull). Natural minor's flat 3/6/7 plus that flat 2.
enum { HBph_i, HBph_bII, HBph_III, HBph_iv, HBph_vo, HBph_VI, HBph_vii, HBph_NFUNC };
static const int hbph_off[HBph_NFUNC]  = { 0, 1, 3, 5, 7, 8, 10 };
static const int hbph_qual[HBph_NFUNC] = { HBQ_MIN7, HBQ_MAJ7, HBQ_DOM7, HBQ_MIN7,
                                           HBQ_M7B5, HBQ_MAJ7, HBQ_MIN7 };
static const char *hbph_fname[HBph_NFUNC] = { "i","bII","III","iv","v\xf8","VI","vii" };

// ── the LYDIAN vocab — MAJOR with a sharp 4 (dreamy / film / "The Simpsons"). The
// tell is the major II a whole-step up: the I-II vamp (the floating lydian sound).
// Diatonic 7ths off the tonic; the #iv° is the raised-4th diminished seat.
enum { HBly_I, HBly_II, HBly_iii, HBly_ivo, HBly_V, HBly_vi, HBly_vii, HBly_NFUNC };
static const int hbly_off[HBly_NFUNC]  = { 0, 2, 4, 6, 7, 9, 11 };
static const int hbly_qual[HBly_NFUNC] = { HBQ_MAJ7, HBQ_DOM7, HBQ_MIN7, HBQ_M7B5,
                                           HBQ_MAJ7, HBQ_MIN7, HBQ_MIN7 };
static const char *hbly_fname[HBly_NFUNC] = { "I","II","iii","#iv\xf8","V","vi","vii" };

// ── styles: where can each function go? repeats = more likely ────────────────
// A style is weights over ONE vocab (the research finding: genres differ by
// weights, not grammars). Row length is part of a cart's PRNG contract — never
// reorder/resize a shipped row (pinned seeds break silently). A style names its
// vocab; NULL = the major default, so every pre-existing style stays byte-exact.
typedef struct {
    const int *const *next;   // per-function candidate list (repeats = weight)
    const int        *n;      // per-function list length (the srnd() argument)
    int               nfunc;  // how much of the vocab this style speaks
    const HbVocab    *vocab;  // the tonal system (NULL = HB_MAJOR, the frozen default)
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
// minor-key motion (the loops the tribe hums)
static const char *hbm_reason(int from, int to) {
    if (to == HBm_i && (from == HBm_V || from == HBm_v)) return "home";
    if (to == HBm_i && from == HBm_VII)                  return "rock cadence";
    if (to == HBm_i && from == HBm_iv)                   return "plagal";
    if (to == HBm_i && from == HBm_viio)                 return "leading tone";
    if (to == HBm_V || to == HBm_v)                      return "cadence";
    if (to == HBm_VII)                                   return "subtonic";
    if (to == HBm_VI)                                    return "the VI";
    if (to == HBm_III)                                   return "relative";
    if (to == HBm_iio)                                   return "pre-V";
    if (to == HBm_iv)                                    return "minor sub";
    return "walk";
}
// blues motion (the 12-bar's pull)
static const char *hbbl_reason(int from, int to) {
    (void)from;
    if (to == HBbl_I)  return "home";
    if (to == HBbl_IV) return "the IV";
    if (to == HBbl_V)  return "turnaround";
    return "walk";
}
// dorian motion — the i<->IV vamp IS the mode (the bright IV is the whole tell)
static const char *hbd_reason(int from, int to) {
    if (to == HBd_i  && from == HBd_IV)  return "dorian vamp";
    if (to == HBd_i  && from == HBd_VII) return "subtonic";
    if (to == HBd_i  && from == HBd_v)   return "minor cadence";
    if (to == HBd_IV)                    return "bright IV";
    if (to == HBd_VII)                   return "subtonic";
    if (to == HBd_ii)                    return "pre-cadence";
    if (to == HBd_III)                   return "relative";
    return "walk";
}
// mixolydian motion — the bVII-IV-I cadence is the rock tell
static const char *hbmx_reason(int from, int to) {
    if (to == HBmx_I && from == HBmx_bVII) return "mixo cadence";
    if (to == HBmx_I && from == HBmx_IV)   return "plagal";
    if (to == HBmx_I)                      return "home";
    if (to == HBmx_bVII)                   return "the bVII";
    if (to == HBmx_IV)                     return "the IV";
    if (to == HBmx_v || to == HBmx_vi)     return "relative";
    if (to == HBmx_ii)                     return "pre-cadence";
    return "walk";
}
// phrygian motion — the bII-i half-step is the flamenco/metal tell
static const char *hbph_reason(int from, int to) {
    if (to == HBph_i && from == HBph_bII)  return "phryg cadence";
    if (to == HBph_i && from == HBph_vii)  return "subtonic";
    if (to == HBph_i)                      return "home";
    if (to == HBph_bII)                    return "the bII";
    if (to == HBph_III)                    return "relative";
    if (to == HBph_iv)                     return "minor sub";
    if (to == HBph_VI)                     return "the VI";
    return "walk";
}
// lydian motion — the I<->II vamp is the floating tell
static const char *hbly_reason(int from, int to) {
    if (to == HBly_I && from == HBly_II)   return "lydian vamp";
    if (to == HBly_I)                      return "home";
    if (to == HBly_II)                     return "bright II";
    if (to == HBly_V)                      return "cadence";
    if (to == HBly_vi)                     return "relative";
    if (to == HBly_iii)                    return "pre-cadence";
    return "walk";
}
// the vocab instances — the major one wraps the frozen arrays; a NULL style vocab
// resolves to this, so nothing pre-existing changes.
static const HbVocab HB_MAJOR       = { hb_off,   hb_qual,   hb_fname,   HB_NFUNC,   hb_reason };
static const HbVocab HB_MINOR       = { hbm_off,  hbm_qual,  hbm_fname,  HBm_NFUNC,  hbm_reason };
static const HbVocab HB_BLUES_VOCAB = { hbbl_off, hbbl_qual, hbbl_fname, HBbl_NFUNC, hbbl_reason };
static const HbVocab HB_DORIAN      = { hbd_off,  hbd_qual,  hbd_fname,  HBd_NFUNC,  hbd_reason };
static const HbVocab HB_MIXO        = { hbmx_off, hbmx_qual, hbmx_fname, HBmx_NFUNC, hbmx_reason };
static const HbVocab HB_PHRYG       = { hbph_off, hbph_qual, hbph_fname, HBph_NFUNC, hbph_reason };
static const HbVocab HB_LYDIAN      = { hbly_off, hbly_qual, hbly_fname, HBly_NFUNC, hbly_reason };

// a MINOR-POP / EDM style — the loops the tribe hums: i-VI-III-VII, i-iv-V, the
// climb i-VI-VII-i. Weighted over the minor vocab; not consumed by any radio.
static const int HB_M_i[8]    = { HBm_VI, HBm_VI, HBm_VII, HBm_VII, HBm_iv, HBm_III, HBm_v, HBm_V };
static const int HB_M_iio[4]  = { HBm_V, HBm_V, HBm_v, HBm_i };
static const int HB_M_III[4]  = { HBm_VII, HBm_VII, HBm_VI, HBm_iv };
static const int HB_M_iv[5]   = { HBm_V, HBm_V, HBm_i, HBm_i, HBm_iio };
static const int HB_M_v[4]    = { HBm_i, HBm_i, HBm_VI, HBm_iv };
static const int HB_M_V[4]    = { HBm_i, HBm_i, HBm_i, HBm_VI };
static const int HB_M_VI[5]   = { HBm_VII, HBm_VII, HBm_iv, HBm_III, HBm_i };
static const int HB_M_VII[4]  = { HBm_i, HBm_i, HBm_III, HBm_VI };
static const int HB_M_viio[2] = { HBm_i, HBm_i };
static const int *const HB_MINPOP_NEXT[HBm_NFUNC] = { HB_M_i, HB_M_iio, HB_M_III,
    HB_M_iv, HB_M_v, HB_M_V, HB_M_VI, HB_M_VII, HB_M_viio };
static const int HB_MINPOP_N[HBm_NFUNC] = { 8, 4, 4, 5, 4, 4, 5, 4, 2 };
static const HbStyle HB_MINPOP = { HB_MINPOP_NEXT, HB_MINPOP_N, HBm_NFUNC, &HB_MINOR };

// a BLUES style — I7 mostly home or to IV; IV7 back to I (or up to V); V7 the
// turnaround home (sometimes the quick IV). The 12-bar's tendency, not its bars.
static const int HB_BL_I[5]  = { HBbl_I, HBbl_I, HBbl_IV, HBbl_IV, HBbl_V };
static const int HB_BL_IV[4] = { HBbl_I, HBbl_I, HBbl_V, HBbl_IV };
static const int HB_BL_V[4]  = { HBbl_I, HBbl_I, HBbl_IV, HBbl_V };
static const int *const HB_BLUES_NEXT[HBbl_NFUNC] = { HB_BL_I, HB_BL_IV, HB_BL_V };
static const int HB_BLUES_N[HBbl_NFUNC] = { 5, 4, 4 };
static const HbStyle HB_BLUES = { HB_BLUES_NEXT, HB_BLUES_N, HBbl_NFUNC, &HB_BLUES_VOCAB };

// a DORIAN style — the i<->IV vamp is home base; VII and v are the other pulls.
// The bright IV keeps coming back (that's the mode's whole identity). Over the
// dorian vocab; no radio consumes it, so the rows carry no pinned-seed contract.
static const int HB_D_i[7]    = { HBd_IV, HBd_IV, HBd_IV, HBd_VII, HBd_VII, HBd_v, HBd_ii };
static const int HB_D_ii[4]   = { HBd_v, HBd_v, HBd_IV, HBd_i };
static const int HB_D_III[3]  = { HBd_VII, HBd_IV, HBd_i };
static const int HB_D_IV[5]   = { HBd_i, HBd_i, HBd_i, HBd_VII, HBd_v };
static const int HB_D_v[4]    = { HBd_i, HBd_i, HBd_IV, HBd_VII };
static const int HB_D_vio[2]  = { HBd_i, HBd_v };
static const int HB_D_VII[4]  = { HBd_i, HBd_i, HBd_IV, HBd_III };
static const int *const HB_DORIAN_NEXT[HBd_NFUNC] = { HB_D_i, HB_D_ii, HB_D_III,
    HB_D_IV, HB_D_v, HB_D_vio, HB_D_VII };
static const int HB_DORIAN_N[HBd_NFUNC] = { 7, 4, 3, 5, 4, 2, 4 };
static const HbStyle HB_DORIAN_STYLE = { HB_DORIAN_NEXT, HB_DORIAN_N, HBd_NFUNC, &HB_DORIAN };

// a FOLK style — the three-chord-song staples on the MAJOR vocab, plainer than
// POP: IV-forward (I-IV-V, plagal), the borrowed shelf barely reached. Same
// grammar as bossa/pop, weight piled on I IV V vi. NULL vocab = major default.
static const int HB_F_I[7]    = { HB_IV, HB_IV, HB_V, HB_V, HB_vi, HB_vi, HB_ii };
static const int HB_F_ii[3]   = { HB_V, HB_V, HB_IV };
static const int HB_F_iii[3]  = { HB_vi, HB_IV, HB_ii };
static const int HB_F_IV[5]   = { HB_I, HB_I, HB_V, HB_V, HB_vi };
static const int HB_F_V[5]    = { HB_I, HB_I, HB_I, HB_vi, HB_IV };
static const int HB_F_vi[5]   = { HB_IV, HB_IV, HB_V, HB_ii, HB_I };
static const int HB_F_II7[1]  = { HB_V };
static const int HB_F_VI7[1]  = { HB_ii };
static const int HB_F_bII7[1] = { HB_I };
static const int HB_F_iv[2]   = { HB_I, HB_I };
static const int HB_F_bVII7[2]= { HB_I, HB_IV };
static const int HB_F_v[1]    = { HB_I };
static const int HB_F_I7[1]   = { HB_IV };
static const int *const HB_FOLK_NEXT[HB_NFUNC] = { HB_F_I, HB_F_ii, HB_F_iii,
    HB_F_IV, HB_F_V, HB_F_vi, HB_F_II7, HB_F_VI7, HB_F_bII7, HB_F_iv,
    HB_F_bVII7, HB_F_v, HB_F_I7 };
static const int HB_FOLK_N[HB_NFUNC] = { 7, 3, 3, 5, 5, 5, 1, 1, 1, 2, 2, 1, 1 };
static const HbStyle HB_FOLK = { HB_FOLK_NEXT, HB_FOLK_N, HB_NFUNC };

// a CINEMATIC style — epic/trailer minor: the i-VI-III-VII climb, weight piled on
// the bright major chords (III VI VII) rather than the ii°/v color. Same minor
// vocab as MINPOP, different gravity (the "two steps from hell" pull).
static const int HB_CIN_i[6]    = { HBm_VI, HBm_VI, HBm_VI, HBm_VII, HBm_III, HBm_iv };
static const int HB_CIN_iio[2]  = { HBm_i, HBm_v };
static const int HB_CIN_III[4]  = { HBm_VII, HBm_VII, HBm_VI, HBm_i };
static const int HB_CIN_iv[3]   = { HBm_i, HBm_VII, HBm_v };
static const int HB_CIN_v[3]    = { HBm_i, HBm_VI, HBm_iv };
static const int HB_CIN_V[2]    = { HBm_i, HBm_VI };
static const int HB_CIN_VI[5]   = { HBm_VII, HBm_VII, HBm_III, HBm_III, HBm_iv };
static const int HB_CIN_VII[5]  = { HBm_i, HBm_i, HBm_III, HBm_VI, HBm_iv };
static const int HB_CIN_viio[2] = { HBm_i, HBm_i };
static const int *const HB_CIN_NEXT[HBm_NFUNC] = { HB_CIN_i, HB_CIN_iio, HB_CIN_III,
    HB_CIN_iv, HB_CIN_v, HB_CIN_V, HB_CIN_VI, HB_CIN_VII, HB_CIN_viio };
static const int HB_CIN_N[HBm_NFUNC] = { 6, 2, 4, 3, 3, 2, 5, 5, 2 };
static const HbStyle HB_CINEMATIC = { HB_CIN_NEXT, HB_CIN_N, HBm_NFUNC, &HB_MINOR };

// a MIXOLYDIAN style — the I-bVII-IV rock vamp is home base; bVII keeps returning.
static const int HB_MX_I[7]    = { HBmx_bVII, HBmx_bVII, HBmx_bVII, HBmx_IV, HBmx_IV, HBmx_v, HBmx_vi };
static const int HB_MX_ii[3]   = { HBmx_v, HBmx_IV, HBmx_I };
static const int HB_MX_iiio[2] = { HBmx_IV, HBmx_vi };
static const int HB_MX_IV[5]   = { HBmx_I, HBmx_I, HBmx_bVII, HBmx_bVII, HBmx_v };
static const int HB_MX_v[3]    = { HBmx_I, HBmx_IV, HBmx_bVII };
static const int HB_MX_vi[4]   = { HBmx_IV, HBmx_IV, HBmx_bVII, HBmx_ii };
static const int HB_MX_bVII[5] = { HBmx_IV, HBmx_IV, HBmx_I, HBmx_I, HBmx_v };
static const int *const HB_MIXO_NEXT[HBmx_NFUNC] = { HB_MX_I, HB_MX_ii, HB_MX_iiio,
    HB_MX_IV, HB_MX_v, HB_MX_vi, HB_MX_bVII };
static const int HB_MIXO_N[HBmx_NFUNC] = { 7, 3, 2, 5, 3, 4, 5 };
static const HbStyle HB_MIXO_STYLE = { HB_MIXO_NEXT, HB_MIXO_N, HBmx_NFUNC, &HB_MIXO };

// a PHRYGIAN style — the i<->bII half-step vamp (flamenco / metal); bII dominates.
static const int HB_PH_i[6]   = { HBph_bII, HBph_bII, HBph_bII, HBph_vii, HBph_III, HBph_iv };
static const int HB_PH_bII[4] = { HBph_i, HBph_i, HBph_III, HBph_vii };
static const int HB_PH_III[3] = { HBph_iv, HBph_bII, HBph_i };
static const int HB_PH_iv[3]  = { HBph_i, HBph_III, HBph_bII };
static const int HB_PH_vo[2]  = { HBph_i, HBph_iv };
static const int HB_PH_VI[3]  = { HBph_bII, HBph_III, HBph_i };
static const int HB_PH_vii[3] = { HBph_i, HBph_i, HBph_VI };
static const int *const HB_PHRYG_NEXT[HBph_NFUNC] = { HB_PH_i, HB_PH_bII, HB_PH_III,
    HB_PH_iv, HB_PH_vo, HB_PH_VI, HB_PH_vii };
static const int HB_PHRYG_N[HBph_NFUNC] = { 6, 4, 3, 3, 2, 3, 3 };
static const HbStyle HB_PHRYG_STYLE = { HB_PHRYG_NEXT, HB_PHRYG_N, HBph_NFUNC, &HB_PHRYG };

// a LYDIAN style — the I<->II vamp is home base (the floating, film-score sound).
static const int HB_LY_I[6]   = { HBly_II, HBly_II, HBly_II, HBly_V, HBly_vi, HBly_iii };
static const int HB_LY_II[4]  = { HBly_I, HBly_I, HBly_V, HBly_vi };
static const int HB_LY_iii[3] = { HBly_vi, HBly_II, HBly_I };
static const int HB_LY_ivo[2] = { HBly_V, HBly_I };
static const int HB_LY_V[4]   = { HBly_I, HBly_I, HBly_vi, HBly_II };
static const int HB_LY_vi[3]  = { HBly_II, HBly_V, HBly_iii };
static const int HB_LY_vii[2] = { HBly_I, HBly_iii };
static const int *const HB_LYDIAN_NEXT[HBly_NFUNC] = { HB_LY_I, HB_LY_II, HB_LY_iii,
    HB_LY_ivo, HB_LY_V, HB_LY_vi, HB_LY_vii };
static const int HB_LYDIAN_N[HBly_NFUNC] = { 6, 4, 3, 2, 4, 3, 2 };
static const HbStyle HB_LYDIAN_STYLE = { HB_LYDIAN_NEXT, HB_LYDIAN_N, HBly_NFUNC, &HB_LYDIAN };

typedef struct { int f, w; const char *why; } HbOpt;   // function, weight, reason

// weighted next-chord candidates from `from`, best first (weight = repeat count
// in the style row). Returns how many were written (≤ max). ~2-4 is the sweet
// spot per the research (93% of real next-chords live in the top 2).
static int hb_suggest(const HbStyle *st, int from, HbOpt *out, int max) {
    if (from < 0 || from >= st->nfunc || max <= 0) return 0;
    const HbVocab *v = st->vocab ? st->vocab : &HB_MAJOR;   // NULL = major default
    int w[HB_NFUNC] = { 0 };
    for (int i = 0; i < st->n[from]; i++) w[st->next[from][i]]++;
    int cnt = 0;
    for (;;) {
        int best = -1;
        for (int f = 0; f < HB_NFUNC; f++)
            if (w[f] > 0 && (best < 0 || w[f] > w[best])) best = f;
        if (best < 0 || cnt >= max) break;
        out[cnt].f = best; out[cnt].w = w[best]; out[cnt].why = v->reason(from, best);
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

// the same analysis over ANY vocab (minor/blues), in a DECLARED key — a chord's
// function is a deterministic lookup once the key is known, so a minor tool is no
// harder than a major one (the unsolved part is GUESSING the key, which chordwise
// never does — you declare it). hb_chord_fn/hb_analyze above are the major case,
// left byte-exact; these are the general form the added vocabs ride.
static int hb_vocab_fn(const HbVocab *v, int keyPc, int rootPc, int qual) {
    int off = (rootPc - keyPc + 120) % 12;
    for (int f = 0; f < v->n; f++)
        if (v->off[f] == off && hb_quality_matches(v->qual[f], qual)) return f;
    return -1;
}
static int hb_vocab_analyze(const HbVocab *v, int keyPc, const int *rootPc,
                            const int *qual, int n, int *outF) {
    int hit = 0;
    for (int i = 0; i < n; i++) {
        outF[i] = hb_vocab_fn(v, keyPc, rootPc[i], qual[i]);
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

    // ── the added vocabs (minor + blues) ──
    { int t[4];
      // A minor (keyPc 9): i = A C E, VI = F, VII = G — spelled off the minor tonic
      hb_vocab_pcs(&HB_MINOR, 9, HBm_i, t);
      expect(t[0]==9 && t[1]==0 && t[2]==4, "hb minor: i in Am = A C E");
      hb_vocab_pcs(&HB_MINOR, 9, HBm_VII, t);
      expect(t[0]==7 && t[1]==11 && t[2]==2, "hb minor: VII in Am = G major (G B D)");
      // blues: IV7 in C = F7
      hb_vocab_pcs(&HB_BLUES_VOCAB, 0, HBbl_IV, t);
      expect(t[0]==5 && t[1]==9 && t[2]==0 && t[3]==3, "hb blues: IV7 in C = F7 (F A C Eb)"); }
    // minor style suggests over the minor vocab, with minor reasons
    { HbOpt o[4]; int n = hb_suggest(&HB_MINPOP, HBm_i, o, 4);
      expect(n >= 1 && o[0].f == HBm_VI, "hb minor-pop: i leans VI (the loop)");
      expect_eq((int)(o[0].why[0]), (int)'t', "hb minor reason table is used ('the VI')"); }
    // blues style stays in its 3-function world
    { HbOpt o[4]; int n = hb_suggest(&HB_BLUES, HBbl_I, o, 4);
      expect(n >= 1 && o[0].f == HBbl_I && o[0].why[0]=='h', "hb blues: I7 -> home, top"); }
    // minor, blues & dorian ANALYSIS inverts generation too (declared key, all 12
    // roots) — the guarantee that lets chordwise NAME these chords, not just make them
    { int okm = 1, okb = 1, okd = 1;
      for (int k = 0; k < 12; k++) {
          for (int f = 0; f < HBm_NFUNC; f++)
              okm &= hb_vocab_fn(&HB_MINOR, k, (k + hbm_off[f]) % 12, hbm_qual[f]) == f;
          for (int f = 0; f < HBbl_NFUNC; f++)
              okb &= hb_vocab_fn(&HB_BLUES_VOCAB, k, (k + hbbl_off[f]) % 12, hbbl_qual[f]) == f;
          for (int f = 0; f < HBd_NFUNC; f++)
              okd &= hb_vocab_fn(&HB_DORIAN, k, (k + hbd_off[f]) % 12, hbd_qual[f]) == f;
      }
      expect(okm, "hb minor round-trip: function -> chord -> same function, all 12 keys");
      expect(okb, "hb blues round-trip: function -> chord -> same function, all 12 keys");
      expect(okd, "hb dorian round-trip: function -> chord -> same function, all 12 keys"); }

    // the other three church modes round-trip too (each a distinct declared key)
    { int okx = 1, okp = 1, okl = 1;
      for (int k = 0; k < 12; k++) {
          for (int f = 0; f < HBmx_NFUNC; f++)
              okx &= hb_vocab_fn(&HB_MIXO, k, (k + hbmx_off[f]) % 12, hbmx_qual[f]) == f;
          for (int f = 0; f < HBph_NFUNC; f++)
              okp &= hb_vocab_fn(&HB_PHRYG, k, (k + hbph_off[f]) % 12, hbph_qual[f]) == f;
          for (int f = 0; f < HBly_NFUNC; f++)
              okl &= hb_vocab_fn(&HB_LYDIAN, k, (k + hbly_off[f]) % 12, hbly_qual[f]) == f;
      }
      expect(okx, "hb mixolydian round-trip: all 12 keys");
      expect(okp, "hb phrygian round-trip: all 12 keys");
      expect(okl, "hb lydian round-trip: all 12 keys"); }

    // dorian's tell: the major IV, and the i<->IV vamp is the top suggestion
    { int t[4];
      hb_vocab_pcs(&HB_DORIAN, 0, HBd_IV, t);     // IV in C dorian = F major (F A C)
      expect(t[0]==5 && t[1]==9 && t[2]==0, "hb dorian: IV in C = F MAJOR (the bright IV)");
      HbOpt o[4]; int n = hb_suggest(&HB_DORIAN_STYLE, HBd_i, o, 4);
      expect(n >= 1 && o[0].f == HBd_IV, "hb dorian: i leans IV (the vamp)"); }

    // the free style tables ride existing vocabs, differently weighted
    { HbOpt o[4];
      int nf = hb_suggest(&HB_FOLK, HB_I, o, 4);         // folk from I: IV first (plagal)
      expect(nf >= 1 && o[0].f == HB_IV, "hb folk: I leans IV (plagal, IV-forward)");
      int nc = hb_suggest(&HB_CINEMATIC, HBm_i, o, 4);   // cinematic from i: the VI climb
      expect(nc >= 1 && o[0].f == HBm_VI, "hb cinematic: i leans VI (the epic climb)"); }

    // each mode's characteristic chord is its top pull, and it's the right quality
    { int t[4]; HbOpt o[4]; int n;
      n = hb_suggest(&HB_MIXO_STYLE, HBmx_I, o, 4);
      expect(n >= 1 && o[0].f == HBmx_bVII, "hb mixo: I leans bVII (the rock cadence)");
      hb_vocab_pcs(&HB_MIXO, 0, HBmx_bVII, t);           // bVII in C = Bb major
      expect(t[0]==10 && t[1]==2 && t[2]==5, "hb mixo: bVII in C = Bb MAJOR");
      n = hb_suggest(&HB_PHRYG_STYLE, HBph_i, o, 4);
      expect(n >= 1 && o[0].f == HBph_bII, "hb phryg: i leans bII (the half-step)");
      hb_vocab_pcs(&HB_PHRYG, 0, HBph_bII, t);           // bII in C = Db major
      expect(t[0]==1 && t[1]==5 && t[2]==8, "hb phryg: bII in C = Db MAJOR");
      n = hb_suggest(&HB_LYDIAN_STYLE, HBly_I, o, 4);
      expect(n >= 1 && o[0].f == HBly_II, "hb lydian: I leans II (the bright II)");
      hb_vocab_pcs(&HB_LYDIAN, 0, HBly_II, t);           // II in C = D major
      expect(t[0]==2 && t[1]==6 && t[2]==9, "hb lydian: II in C = D MAJOR"); }
}
#endif // DE_SPEC

#endif // HARMONY_H
