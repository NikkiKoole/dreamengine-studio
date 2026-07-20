// improv.h — THE IMPROVISER (melody brain #2): phrase-based solo generation.
//
// Born in roadhouse.c (the Doors station, 2026-06-05); graduated to a shared
// header when the cocktail trio became its second customer — the same
// extract-on-second-user rule as radio.h. Future customers: ethio-jazz,
// J-fusion, anything that needs a band member who can actually take a solo.
//
// The model: a solo is PHRASES; a phrase is the MOTIF, developed. At solo
// start a 3-4 note motif is invented; every 2-bar phrase renders it through
// a development op (stated / answered-inverted / sequenced up / answered
// low), with density and register driven by a TENSION ARC over the solo's
// length (rise to a peak at 0.7, then release), double-time bursts at the
// peak, a long resolving tone + a breath at every phrase end.
//
// ⚠ EVERYTHING here runs on engine rnd() — PERFORMANCE, never composition.
// A pinned seed replays the song; the solos are new every time. That also
// means extraction/refactor here can never break a pinned seed.
//
// Usage (see roadhouse.c / cocktail.c):
//   static Improv solo;
//   improv_begin(&solo, 67, 16, 1.0f);            // register, bars, density
//   // per 2-bar boundary inside the solo:
//   if (improv_due(&solo, barInSolo)) improv_render(&solo, barInSolo, mode7);
//   // per step (cs = 0..31 within the 2-bar window):
//   for (int i = 0; i < solo.n; i++)
//       if (solo.onset[i] == cs)
//           play(improv_midi(&solo, i, keyPc, mode7, bluePct), dur=solo.dur[i]);

#ifndef IMPROV_H
#define IMPROV_H

#include "studio.h"

typedef struct {
    int  motif[4], motifN;     // the solo's idea: small degree steps
    int  phraseNo;             // which 2-bar phrase of this solo
    int  onset[16], deg[16], dur[16], n;   // the rendered phrase (32 steps)
    int  regBase;              // register floor (the arc climbs from here)
    int  lenBars;              // solo length in bars (the arc's denominator)
    float dens;                // density multiplier (1 = lead; ~0.6 = a bass solo)
    long renderedAt;           // the 2-bar window this buffer belongs to
} Improv;

// the tension arc 0..1: rise to the peak at 0.7 of the solo, then release
static float improv_arc(const Improv *im, long barInSolo) {
    float t = (float)barInSolo / (float)(im->lenBars > 0 ? im->lenBars : 16);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t < 0.7f ? t / 0.7f : 1.0f - (t - 0.7f) / 0.3f * 0.55f;
}

// a fresh idea — call at the first step of a solo (pure performance rnd)
static void improv_begin(Improv *im, int regBase, int lenBars, float dens) {
    im->motifN = 3 + rnd(2);
    for (int i = 0; i < im->motifN; i++) im->motif[i] = rnd(5) - 2;   // -2..+2 degrees
    if (im->motif[0] == 0) im->motif[0] = 1;        // the idea has to GO somewhere
    im->phraseNo   = 0;
    im->regBase    = regBase;
    im->lenBars    = lenBars;
    im->dens       = dens;
    im->renderedAt = -1;
}

static bool improv_due(const Improv *im, long barInSolo) {
    return im->renderedAt != barInSolo;
}

// render one 2-bar phrase into the buffer. mode7 = the scale as 7 semitone
// offsets (any mode/scale — mixolydian, dorian, a jazz major, tizita...)
static void improv_render(Improv *im, long barInSolo, const int *mode7) {
    (void)mode7;
    float arc = improv_arc(im, barInSolo);
    int   op  = im->phraseNo % 4;                   // statement/answer/sequence/answer-low
    int   want = (int)((3.0f + arc * 7.0f + (op == 2 ? 1 : 0)) * im->dens);
    if (op == 3) want = want * 2 / 3;               // the answer breathes
    if (want > 11) want = 11;
    if (want < 1)  want = 1;

    // onsets: off-beat-leaning candidates; the peak doubles time
    static const int OC[14] = { 0, 2, 3, 5, 6, 8, 10, 11, 13, 16, 18, 21, 24, 26 };
    int used[14] = { 0 };
    im->n = 0;
    for (int k = 0; k < want && im->n < 14; k++) {
        int tries = 6;
        while (tries--) {
            int c = rnd(14);
            if (!used[c]) { used[c] = 1; break; }
        }
    }
    for (int c = 0; c < 14; c++) if (used[c]) im->onset[im->n++] = OC[c];
    if (arc > 0.8f && im->n < 13)                   // the PEAK: double-time bursts
        for (int c = 0; c < 14 && im->n < 13; c++)
            if (used[c] && OC[c] + 1 < 28 && chance(40)) im->onset[im->n++] = OC[c] + 1;
    for (int i = 1; i < im->n; i++)                 // onsets in order for the gap math
        for (int j = i; j > 0 && im->onset[j] < im->onset[j - 1]; j--)
            { int t = im->onset[j]; im->onset[j] = im->onset[j - 1]; im->onset[j - 1] = t; }

    // pitches: walk the mode by the motif; the op transposes/inverts the idea
    int d = (op == 2 ? 2 : 0) + (op == 3 ? -2 : 0) + rnd(3) - 1;
    for (int i = 0; i < im->n; i++) {
        int step = im->motif[i % im->motifN];
        if (op == 1) step = -step;                  // the ANSWER inverts the question
        d += step;
        if (d < -3) d = -3;
        if (d > 10) d = 10;
        im->deg[i] = d;
        im->dur[i] = (i + 1 < im->n ? im->onset[i + 1] - im->onset[i] : 32 - im->onset[i]);
    }
    if (im->n > 0) {                                // resolve: end long, on home
        im->deg[im->n - 1] = (im->deg[im->n - 1] > 4 ? 7 : 0) + (chance(40) ? 4 : 0);
        if (im->dur[im->n - 1] < 6) im->dur[im->n - 1] = 6;
    }
    im->phraseNo++;
    im->renderedAt = barInSolo;
}

// degree index -> midi, walking mode7 up/down from a center
static int improv_deg_to_midi(int degIdx, int center, const int *mode7) {
    int oct = 0;
    while (degIdx < 0)  { degIdx += 7; oct--; }
    while (degIdx >= 7) { degIdx -= 7; oct++; }
    return center + mode7[degIdx] + oct * 12;
}

// note i of the current phrase -> midi: mode walk in a climbing register
// window, with the blue intrusion (b3/b7/b5 over anything, bluePct percent)
static int improv_midi(Improv *im, int i, long barInSolo, int keyPc,
                       const int *mode7, int bluePct) {
    float arc = improv_arc(im, barInSolo);
    int m = improv_deg_to_midi(im->deg[i], 60 + keyPc, mode7);
    m += ((im->regBase - 60) / 12) * 12 + ((int)(arc * 10.0f) / 7) * 12;
    while (m < im->regBase - 5)  m += 12;
    while (m > im->regBase + 17) m -= 12;
    if (chance(bluePct)) {                          // the blue bend
        static const int BLUE[3] = { 3, 10, 6 };
        m = 60 + keyPc + BLUE[rnd(3)];
        while (m < im->regBase - 5) m += 12;
    }
    return m;
}

// ── chord awareness (opt-in, PURE — no rnd(), so it can't shift a seed) ───────
// The improviser walks the KEY's scale; a caller that knows the CURRENT CHORD
// can pull the notes that MATTER (bar downbeats + the phrase's resolving tone)
// onto a chord tone, so the solo lands ON the changes instead of noodling the
// scale — the "chord tone on the strong beat" rule the walking bass already
// obeys. chordPc = the chord's pitch classes 0..11; nPc its count. Give none
// (NULL / 0) and every path below reduces byte-for-byte to improv_midi above.

// nearest midi to m whose pitch-class is one of the chord tones (shortest move)
static int improv_snap(int m, const int *chordPc, int nPc) {
    if (!chordPc || nPc <= 0) return m;
    int pc = ((m % 12) + 12) % 12, best = m, bestd = 99;
    for (int i = 0; i < nPc; i++) {
        int d  = (chordPc[i] - pc + 18) % 12 - 6;   // signed shortest dist -6..+5
        int md = d < 0 ? -d : d;
        if (md < bestd) { bestd = md; best = m + d; }
    }
    return best;
}

// how much of the solo lands on the chord — the per-station taste knob:
//   STRONG  = each bar's downbeat (step 0/16) AND the resolving note (locked-on,
//             most "correct" — good for a busy bop line)
//   RESOLVE = ONLY the phrase's final note (downbeats stay loose/scale-y, but
//             every phrase still lands home — the more human lounge feel)
enum { IMPROV_SNAP_STRONG, IMPROV_SNAP_RESOLVE };

// the notes worth landing on a chord tone, per the scope above (the resolving
// last note is always a target; STRONG adds the two bar downbeats)
static bool improv_is_target(const Improv *im, int i, int scope) {
    if (i == im->n - 1) return true;
    if (scope == IMPROV_SNAP_RESOLVE) return false;
    return im->onset[i] == 0 || im->onset[i] == 16;
}

// chord-aware pitch: the target notes (per `scope`) snap to a chord tone, the
// rest walk the scale as before. chordPc == NULL is EXACTLY improv_midi.
static int improv_midi_chord(Improv *im, int i, long barInSolo, int keyPc,
                             const int *mode7, int bluePct,
                             const int *chordPc, int nPc, int scope) {
    int m = improv_midi(im, i, barInSolo, keyPc, mode7, bluePct);
    if (chordPc && improv_is_target(im, i, scope)) m = improv_snap(m, chordPc, nPc);
    return m;
}

// ── self-check — the spec.h pattern (a shared header can't own spec(), but it
// can carry its own PURE assertions; a customer's spec() just calls this) ─────
#ifdef DE_SPEC
#include "spec.h"
static inline void improv_selfcheck(void) {
    // no chord given → identity (the old key-relative path is untouched)
    expect_eq(improv_snap(60, 0, 0),    60, "improv snap: no chord = identity");

    // Cmaj7 tones {C E G B}: an already-chord-tone stays put; a non-tone moves
    static const int CMAJ7[4] = { 0, 4, 7, 11 };
    expect_eq(improv_snap(64, CMAJ7, 4), 64, "improv snap: E over Cmaj7 is a tone, held");
    expect_eq(improv_snap(62, CMAJ7, 4), 60, "improv snap: D pulls to nearest tone C (tie -> down)");
    expect_eq(improv_snap(65, CMAJ7, 4), 64, "improv snap: F pulls down to E (the 3rd)");

    // shortest move wraps the octave: C with only B in the chord goes DOWN a
    // semitone (59), never up eleven
    static const int BONLY[1] = { 11 };
    expect_eq(improv_snap(60, BONLY, 1), 59, "improv snap: shortest path, not nearest-up");

    // STRONG scope: bar downbeats (0,16) + the last note are targets; off-beats free
    Improv im; im.n = 4;
    im.onset[0] = 0; im.onset[1] = 6; im.onset[2] = 16; im.onset[3] = 21;
    expect(improv_is_target(&im, 0, IMPROV_SNAP_STRONG), "improv STRONG: bar-1 downbeat");
    expect(!improv_is_target(&im, 1, IMPROV_SNAP_STRONG), "improv STRONG: an off-beat walks free");
    expect(improv_is_target(&im, 2, IMPROV_SNAP_STRONG), "improv STRONG: bar-2 downbeat");
    expect(improv_is_target(&im, 3, IMPROV_SNAP_STRONG), "improv STRONG: the resolving last note");

    // RESOLVE scope: ONLY the last note is a target — even the downbeats walk free
    expect(!improv_is_target(&im, 0, IMPROV_SNAP_RESOLVE), "improv RESOLVE: downbeat walks free");
    expect(!improv_is_target(&im, 2, IMPROV_SNAP_RESOLVE), "improv RESOLVE: bar-2 downbeat free too");
    expect(improv_is_target(&im, 3, IMPROV_SNAP_RESOLVE), "improv RESOLVE: only the resolving note lands");
}
#endif // DE_SPEC

#endif // IMPROV_H
