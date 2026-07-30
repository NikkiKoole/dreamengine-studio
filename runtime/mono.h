// mono.h — NOTE PRIORITY and TRIGGER POLICY for a monophonic keyboard (audit §B3, plan item 2.2).
//
// The thing every monosynth has and none of ours agreed about. Hold three keys on a one-voice synth and
// two questions have to be answered: WHICH held note sounds (priority), and does pressing another key
// restart the envelope (triggering). Synth Secrets Part 18 is dedicated to it, and Reid's claim is that
// this — not the oscillators, not the filter — is what decides whether a synth feels playable:
//
//   "My playing sounded punchier on the Odyssey, and I could play at higher speeds than I could on the
//    Minimoog. The reason for this was nothing to do with my playing … The answer lay in the engineering
//    within the instruments."
//
// He counts four priority schemes crossed with six-plus triggering permutations: "at least 24 keyboard
// characteristics that you might encounter". Before this header, `tb303`, `acidrack`, `moog` and `sh101`
// each hand-rolled one point in that space, so they silently disagreed about the single behaviour that
// most defines how a monosynth answers the fingers.
//
// WHY A CART-LAND HEADER AND NOT `sound.h` (ADR-0006/0016, and the plan's add-vs-change ladder): this is
// pure bookkeeping over a held-key list. It needs no sample-rate work, no voice state, no engine surface —
// so it belongs on the lowest rung that holds the finding. If a built cart later proves it must live in the
// engine (the §L4-vs-§K6 argument below is the likely reason), promote it then, not now.
//
// ── HOW A CART USES IT ────────────────────────────────────────────────────────────────────────────────
//
//   static Mono mono;
//   mono_init(&mono, MONO_LAST, MONO_SINGLE);
//   ...
//   switch (mono_press(&mono, midi, vel)) {          // your key-down
//       case MONO_START:  start_note(mono.sounding); break;   // silence → sounding: a fresh attack
//       case MONO_RETRIG: glide_to(mono.sounding);            // re-attack (may be the SAME pitch) —
//                         note_retrig(h);            break;   //   move the pitch, THEN re-fire the env
//       case MONO_GLIDE:  glide_to(mono.sounding);   break;   // new pitch, envelope untouched = legato
//       case MONO_NONE:   break;                              // nothing audible changed
//       case MONO_STOP:   stop_note();               break;   // (press never returns this)
//   }
//
// The header decides WHAT should happen; the cart owns HOW (whether a glide takes portamento time, how an
// attack is voiced). It never calls into the engine, which is also what makes it fully spec-able.
//
// ⚠ MONO_RETRIG WANTS `note_retrig(handle)`, NOT a fresh `note_on`. Restarting the voice leaves the OLD
// one ringing at the OLD pitch underneath through its release (measured: 27% louder, twice as long to
// settle) and costs a second voice — a monosynth has one. `note_retrig` re-fires the envelope and the
// engine's onset transient on the voice you already hold, click-free, keeping its pitch and glide. That
// also DECOUPLES the two axes properly: the pitch still moves at your portamento time while the envelope
// re-attacks, which the old glide_to-vs-start_note fork could not express (it silently coupled a
// re-attack to a pitch snap). `sh101`'s `articulate()` is the worked pattern.
//
// ── THE TWO AXES ──────────────────────────────────────────────────────────────────────────────────────
//
// PRIORITY — which of the held notes sounds:
//   MONO_LAST    the most recently pressed (what every cart here already did, so it is the default)
//   MONO_LOW     the lowest held note — the Minimoog's answer
//   MONO_HIGH    the highest held note — makes "hold a bass note and solo above it" work, Reid's own A/B
//   MONO_FIRST   the earliest still-held note
//
// TRIGGERING — does the envelope restart:
//   MONO_SINGLE  only when the keyboard was EMPTY before this press. Reid, Figure 8: it "retriggers only
//                when you release all other notes", and "this, by the way, is exactly how a Minimoog
//                works". Playing legato therefore glides without re-attacking.
//   MONO_MULTI   on EVERY press, Figure 9, the ARP behaviour — "retriggers every time that you press a
//                key". Note that means every press, even one that does NOT win priority: you get a fresh
//                attack on the same pitch. That is deliberate and it is what makes fast trills speak.
//   MONO_ANY     Figure 11's third real variant: retriggers "on any transition between notes", so a
//                RELEASE that hands the voice to another held note re-attacks too, where SINGLE and MULTI
//                both glide.
//
// A release never stops the voice while any key is still held; it hands over (glide, or re-attack under
// MONO_ANY). The last release returns MONO_STOP.
//
// ── WHY THIS IS A PROPERTY AN INSTRUMENT WANTS TO DECLARE, EVENTUALLY ─────────────────────────────────
// Two later audit findings collide here, and both are about a defining transient existing at all: §L4 says
// the Hammond's percussion must be SINGLE-trigger (hold a chord, add a note, and the percussion must NOT
// re-strike, or it stops being a Hammond), while §K6 says the flute chiff must be MULTI-trigger (every new
// note needs its own breath onset). So the "right" policy is a fact about an instrument, not a taste
// setting on a panel. That is the argument for promotion later; for now a cart declares it per patch.
//
// Not to be confused with `solo.h` (a scale-locked solo strip over a radio) or `keybed.h` (the polyphonic
// chromatic keybed widget). This header has no UI and makes no sound: it is the decision layer between.
#ifndef MONO_H
#define MONO_H

#define MONO_MAX 16          // held keys tracked at once; a monosynth never needs more
#define MONO_OFF (-1)        // `sounding` when nothing is held

typedef enum { MONO_LAST = 0, MONO_LOW, MONO_HIGH, MONO_FIRST, MONO_NPRIO } MonoPriority;
typedef enum { MONO_SINGLE = 0, MONO_MULTI, MONO_ANY, MONO_NTRIG } MonoTrigger;

// what the cart should DO about it
typedef enum {
    MONO_NONE = 0,   // nothing audible changed
    MONO_START,      // was silent, now sounding — always a fresh attack
    MONO_GLIDE,      // the sounding pitch moved, envelope untouched (legato)
    MONO_RETRIG,     // re-attack; the pitch may or may not have moved
    MONO_STOP        // the last held key went up
} MonoEvent;

typedef struct {
    int note[MONO_MAX];   // held notes in PRESS ORDER, oldest first (so LAST/FIRST are just the ends)
    int vel[MONO_MAX];
    int n;
    int prio;             // MonoPriority
    int trig;             // MonoTrigger
    int sounding;         // the note sounding now, or MONO_OFF
    int sounding_vel;     // its velocity, from its own press
} Mono;

static const char *MONO_PRIO_NAME[MONO_NPRIO] = { "LAST", "LOW", "HIGH", "FIRST" };
static const char *MONO_TRIG_NAME[MONO_NTRIG] = { "SINGLE", "MULTI", "ANY" };

static inline void mono_init(Mono *m, int prio, int trig) {
    m->n = 0; m->prio = prio; m->trig = trig;
    m->sounding = MONO_OFF; m->sounding_vel = 0;
}

static inline int mono_held(const Mono *m, int midi) {
    for (int i = 0; i < m->n; i++) if (m->note[i] == midi) return 1;
    return 0;
}

// index of the winning held note. note[] is in press order, so LAST and FIRST need no comparison at all.
static inline int mono__winner(const Mono *m) {
    int best = 0;
    for (int i = 1; i < m->n; i++) {
        int take = 0;
        if      (m->prio == MONO_LAST)  take = 1;                          // later index always wins
        else if (m->prio == MONO_FIRST) take = 0;                          // index 0 keeps it
        else if (m->prio == MONO_LOW)   take = m->note[i] < m->note[best];
        else if (m->prio == MONO_HIGH)  take = m->note[i] > m->note[best];
        if (take) best = i;
    }
    return best;
}

static inline void mono__refresh(Mono *m) {
    if (m->n == 0) { m->sounding = MONO_OFF; m->sounding_vel = 0; return; }
    int w = mono__winner(m);
    m->sounding = m->note[w]; m->sounding_vel = m->vel[w];
}

static inline MonoEvent mono_press(Mono *m, int midi, int vel) {
    if (mono_held(m, midi)) return MONO_NONE;        // already down (key repeat) — not a new press
    if (m->n >= MONO_MAX)   return MONO_NONE;        // full: ignore rather than evict, so the held set
                                                     // stays exactly what the player's fingers say
    int was = m->sounding;
    m->note[m->n] = midi; m->vel[m->n] = vel; m->n++;
    mono__refresh(m);

    if (was == MONO_OFF) return MONO_START;          // from silence, every policy attacks
    if (m->trig == MONO_SINGLE)                      // Fig 8: only an empty keyboard retriggers, and the
        return (m->sounding != was) ? MONO_GLIDE     // line above already handled that case
                                    : MONO_NONE;
    return MONO_RETRIG;                              // Fig 9/11: every press re-attacks, pitch moved or not
}

static inline MonoEvent mono_release(Mono *m, int midi) {
    int at = -1;
    for (int i = 0; i < m->n; i++) if (m->note[i] == midi) { at = i; break; }
    if (at < 0) return MONO_NONE;                    // wasn't held (a latched/injected release)
    for (int j = at; j < m->n - 1; j++) { m->note[j] = m->note[j + 1]; m->vel[j] = m->vel[j + 1]; }
    m->n--;

    int was = m->sounding;
    mono__refresh(m);
    if (m->n == 0)             return MONO_STOP;
    if (m->sounding == was)    return MONO_NONE;     // a note that wasn't sounding went up
    return (m->trig == MONO_ANY) ? MONO_RETRIG       // Fig 11: ANY transition re-attacks…
                                 : MONO_GLIDE;       // …otherwise handing over is legato
}

static inline void mono_clear(Mono *m) { m->n = 0; m->sounding = MONO_OFF; m->sounding_vel = 0; }

// ── specs on an includeable (spec.h's pattern; the including cart's spec() calls this) ────────────────
//
// This header is pure logic, which makes it the one part of a monosynth that an oracle CAN judge — and
// Reid's Part 18 is already a test suite: the same played sequence through four priorities gives four
// different pitch sequences (his Figures 2a-4d), and the trigger switch changes how many attacks you hear.
// That is exactly the class of claim 1.3 taught us not to trust to the ear: the snare tilt measured as a
// clear effect, A/B'd as a clear effect, and had stopped depending on velocity — the thing it claimed to do.
#ifdef DE_SPEC
#include "spec.h"

// One sequence that separates all four priorities. Press 64, then 60 (below it), then 67 (above both),
// then release them middle-first. Chosen because every scheme disagrees with every other one somewhere:
//   press:  +64      +60      +67      -60      -67      -64
//   LAST     64       60       67       67       64       off
//   LOW      64       60       60       64       64       off
//   HIGH     64       64       67       67       64       off
//   FIRST    64       64       64       64       64       off
static inline void mono__spec_priority(void) {
    static const int WANT[MONO_NPRIO][6] = {
        { 64, 60, 67, 67, 64, MONO_OFF },   // LAST
        { 64, 60, 60, 64, 64, MONO_OFF },   // LOW
        { 64, 64, 67, 67, 64, MONO_OFF },   // HIGH
        { 64, 64, 64, 64, 64, MONO_OFF },   // FIRST
    };
    for (int p = 0; p < MONO_NPRIO; p++) {
        Mono m; mono_init(&m, p, MONO_SINGLE);
        int got[6];
        mono_press(&m, 64, 100); got[0] = m.sounding;
        mono_press(&m, 60, 100); got[1] = m.sounding;
        mono_press(&m, 67, 100); got[2] = m.sounding;
        mono_release(&m, 60);    got[3] = m.sounding;
        mono_release(&m, 67);    got[4] = m.sounding;
        mono_release(&m, 64);    got[5] = m.sounding;
        for (int i = 0; i < 6; i++)
            expect_eq(got[i], WANT[p][i], MONO_PRIO_NAME[p]);
    }
}

// The same sequence, counting ATTACKS (START or RETRIG) per trigger policy, at LAST priority:
//   SINGLE 1  — only the first press, onto an empty keyboard (Fig 8)
//   MULTI  3  — every press (Fig 9), including +67 which does move the pitch here
//   ANY    4  — those three plus the -67 release that hands the voice back to 64 (Fig 11)
// If two of these ever come out equal, the trigger switch has stopped meaning anything.
static inline void mono__spec_trigger(void) {
    static const int WANT_ATTACKS[MONO_NTRIG] = { 1, 3, 4 };
    for (int t = 0; t < MONO_NTRIG; t++) {
        Mono m; mono_init(&m, MONO_LAST, t);
        int attacks = 0;
        MonoEvent e[6];
        e[0] = mono_press(&m, 64, 100);
        e[1] = mono_press(&m, 60, 100);
        e[2] = mono_press(&m, 67, 100);
        e[3] = mono_release(&m, 60);
        e[4] = mono_release(&m, 67);
        e[5] = mono_release(&m, 64);
        for (int i = 0; i < 6; i++) if (e[i] == MONO_START || e[i] == MONO_RETRIG) attacks++;
        expect_eq(attacks, WANT_ATTACKS[t], MONO_TRIG_NAME[t]);
        expect(e[5] == MONO_STOP, "the last release stops the voice");
    }

    // SINGLE's rule stated positively: release everything, press again, and you DO get a fresh attack.
    Mono s; mono_init(&s, MONO_LAST, MONO_SINGLE);
    mono_press(&s, 60, 100);
    expect(mono_press(&s, 62, 100) == MONO_GLIDE, "SINGLE: legato press glides, no attack");
    mono_release(&s, 62);
    mono_release(&s, 60);
    expect(mono_press(&s, 62, 100) == MONO_START, "SINGLE: after releasing all, a press attacks again");

    // MULTI re-attacks even when the pressed key does NOT win priority — the claim is about the PRESS,
    // not about the pitch. Under LOW priority, pressing above the held note keeps the pitch and still
    // re-articulates it.
    Mono u; mono_init(&u, MONO_LOW, MONO_MULTI);
    mono_press(&u, 60, 100);
    expect(mono_press(&u, 72, 100) == MONO_RETRIG, "MULTI: a losing press still re-attacks");
    expect_eq(u.sounding, 60, "MULTI: …and the pitch did not move");
}

static inline void mono__spec_bookkeeping(void) {
    Mono m; mono_init(&m, MONO_LAST, MONO_MULTI);
    expect_eq(m.sounding, MONO_OFF, "a fresh Mono sounds nothing");
    expect(mono_release(&m, 60) == MONO_NONE, "releasing an unheld note is a no-op");
    mono_press(&m, 60, 99);
    expect(mono_press(&m, 60, 99) == MONO_NONE, "a repeated press of a held key is not a new press");
    expect_eq(m.n, 1, "…and does not grow the held stack");
    expect_eq(m.sounding_vel, 99, "velocity comes from the winning note's own press");

    for (int i = 0; i < MONO_MAX + 4; i++) mono_press(&m, 40 + i, 100);   // overflow must not corrupt
    expect_eq(m.n, MONO_MAX, "the held stack clamps at MONO_MAX");
    expect(mono_held(&m, 60), "the first held key survives an overflow");

    mono_clear(&m);
    expect_eq(m.n, 0, "mono_clear empties the stack");
    expect_eq(m.sounding, MONO_OFF, "…and silences it");
}

static inline void mono_selfcheck(void) {
    mono__spec_priority();
    mono__spec_trigger();
    mono__spec_bookkeeping();
}
#endif  // DE_SPEC

#endif  // MONO_H
