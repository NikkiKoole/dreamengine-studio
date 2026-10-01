// patgen.h — a LATCHED PATTERN GENERATOR: hold some keys, it plays them back on a clock, with
// orders, rests and octave jumps. Ported from CHOMPI TEMPO's ArpeggiatorSequencer (MIT, CHOMPI Club /
// Chase Bliss; docs/design/chompi-harvest.md §4). Pure logic: no sound, no UI, no clock.
//
// THE HONEST CORE, which is why this is worth a header and not a ten-line arpeggiator:
// a REST consumes a clock step but does NOT advance the note. So the gap DRIFTS through the line
// whenever the number of held notes doesn't divide by the mask's plays-per-period: hold FOUR notes
// against `XXX.` (3 plays per 4 steps) and the gap lands after a different note every pass, and the
// phrase takes 4·4/gcd(4,3) = 16 steps to come round. Hold THREE and it is a plain 4-step phrase with
// the gap at the end. Adding or lifting one key reshapes the whole line, and nobody programmed it.
//
// ── HOW A CART USES IT ────────────────────────────────────────────────────────────────────────────────
//
//   static PatGen pg;
//   pg_init(&pg, 1234);                      // a seed: RANDOM order + octave jumps are deterministic
//   pg_toggle(&pg, 60);                      // latch / unlatch a key (or pg_add / pg_remove)
//   pg.order = PG_UP;  pg.rest = PG_REST_3_1;  pg.octave_chance = 0.2f;
//   ...
//   on every clock edge (the CART owns the clock, e.g. every 1/16 from beat_pos()):
//       int midi = pg_step(&pg);             // the note to play, or PG_REST (rest, or nothing held)
//       if (midi != PG_REST) hit(midi, SL_LEAD, 6, gate_ms);
//   on transport start:  pg_restart(&pg);
//
// The header decides WHICH note; the cart decides how it sounds and how long it is.
//
// ── WHAT TEMPO HAD THAT THE CART KEEPS ────────────────────────────────────────────────────────────────
// Clock divisions (synced 1/2 · 1/4 · 1/8 · 1/16 · 1/32; free ×2 · ×1.5 · ×1 · ×0.66 · ×0.5, i.e. dotted
// and triplet feels), and a division change DEFERRED to the next edge so it never stutters. Both are the
// cart's clock. PG_DIV_BEATS[] below is TEMPO's synced table, in beats, so carts agree on it.
//
// ── CHANGED ON THE WAY IN ─────────────────────────────────────────────────────────────────────────────
// The original's ping-pong direction lived in a function-local `static`, shared by both of TEMPO's
// engines (and a SECOND one in its LED preview, so the preview drifted from what played). RANDOM used
// rand(). Here direction and the generator live in the struct, so two PatGens are strangers.
#ifndef PATGEN_H
#define PATGEN_H

#include <stdint.h>

#define PG_MAX   16    // latched notes per pattern
#define PG_REST  (-1)  // pg_step's "nothing to play this step"

typedef enum { PG_SEQ = 0, PG_UP, PG_DOWN, PG_PINGPONG, PG_RANDOM, PG_NORDER } PgOrder;
// SEQ = the order the keys were pressed in. PINGPONG repeats its end notes (1 2 3 3 2 1 1 2 …), as TEMPO did.

// TEMPO's five rest masks, 20 steps each, bit i = 1 → step i plays. Named by their shape.
typedef enum { PG_REST_NONE = 0, PG_REST_3_1, PG_REST_2_2, PG_REST_1_1, PG_REST_10, PG_NREST } PgRest;
static const char *const PG_REST_NAME[PG_NREST] = { "none", "XXX.", "XX..", "X.", "X.XX.X.XX." };
static const uint32_t PG_REST_MASK[PG_NREST] = {
    0xFFFFFu,                                  // XXXXXXXXXXXXXXXXXXXX
    0x77777u,                                  // XXX.XXX.XXX.XXX.XXX.
    0x33333u,                                  // XX..XX..XX..XX..XX..
    0x55555u,                                  // X.X.X.X.X.X.X.X.X.X.
    0x6B5ADu,                                  // X.XX.X.XX.X.XX.X.XX.  (period 10)
};
#define PG_REST_STEPS 20

static const float PG_DIV_BEATS[5] = { 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };   // 1/2 · 1/4 · 1/8 · 1/16 · 1/32
static const char *const PG_DIV_NAME[5] = { "1/2", "1/4", "1/8", "1/16", "1/32" };

typedef struct {
    int      notes[PG_MAX];   // latched MIDI notes, in PRESS order
    int      n;
    int      order;           // PgOrder
    int      rest;            // PgRest
    float    octave_chance;   // 0..1: chance a played note jumps an octave (up or down, 50/50)
    int      idx;             // position in the current order's view
    int      dir;             // ping-pong direction, +1 / -1
    int      rest_idx;        // 0..PG_REST_STEPS-1
    int      started;         // has the first note of this run been played?
    uint32_t seed;
    int      last;            // the last note pg_step returned (incl. octave), PG_REST if it rested
} PatGen;

static inline uint32_t pg__rand(PatGen *p) {   // LCG, the same constants as TEMPO's warble / navkit
    p->seed = (1103515245u * p->seed + 12345u) & 0x7FFFFFFFu;
    return p->seed;
}
static inline float pg__unit(PatGen *p) { return (float)pg__rand(p) * 4.656612873e-10f; }   // [0,1)
// ⚠ never use an LCG's LOW bits: bit 0 alternates 0,1,0,1 and `% 4` cycles with period 4. The first
// version did `rand & 1` for the octave direction (two draws a note → always the same parity → every
// jump went the same way) and `rand % n` for RANDOM order (a fixed loop). The selfcheck caught it.
static inline uint32_t pg__pick(PatGen *p, uint32_t n) { return (pg__rand(p) >> 15) % n; }

static inline void pg_init(PatGen *p, uint32_t seed) {
    p->n = 0; p->order = PG_SEQ; p->rest = PG_REST_NONE; p->octave_chance = 0.0f;
    p->idx = 0; p->dir = 1; p->rest_idx = 0; p->started = 0; p->seed = seed ? seed : 1u; p->last = PG_REST;
}
static inline void pg_restart(PatGen *p) { p->idx = 0; p->dir = 1; p->rest_idx = 0; p->started = 0; }
static inline void pg_clear(PatGen *p)   { p->n = 0; pg_restart(p); }

static inline int pg_has(const PatGen *p, int midi) {
    for (int i = 0; i < p->n; i++) if (p->notes[i] == midi) return 1;
    return 0;
}
static inline void pg_add(PatGen *p, int midi) {
    if (pg_has(p, midi) || p->n >= PG_MAX) return;
    p->notes[p->n++] = midi;
}
// the view an order plays through: press order, or sorted (insertion sort; n ≤ 16)
static inline void pg__view(const PatGen *p, int *v) {
    for (int i = 0; i < p->n; i++) v[i] = p->notes[i];
    if (p->order == PG_SEQ || p->order == PG_RANDOM) return;
    for (int i = 1; i < p->n; i++) {
        int x = v[i], j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    if (p->order == PG_DOWN)
        for (int i = 0, j = p->n - 1; i < j; i++, j--) { int t = v[i]; v[i] = v[j]; v[j] = t; }
}
static inline void pg_remove(PatGen *p, int midi) {
    int v[PG_MAX]; pg__view(p, v);
    int at = -1;                                   // where it sits in the VIEW (that is what idx indexes)
    for (int i = 0; i < p->n; i++) if (v[i] == midi) { at = i; break; }
    if (at < 0) return;
    for (int i = 0, k = 0; i < p->n; i++) if (p->notes[i] != midi) p->notes[k++] = p->notes[i];
    p->n--;
    if (at < p->idx) p->idx--;                     // keep pointing at the same next note (TEMPO's removeKey)
    if (p->idx >= p->n) p->idx = p->n > 0 ? p->n - 1 : 0;
}
static inline void pg_toggle(PatGen *p, int midi) { if (pg_has(p, midi)) pg_remove(p, midi); else pg_add(p, midi); }

static inline void pg__advance(PatGen *p) {
    int n = p->n;
    switch (p->order) {
        case PG_SEQ: case PG_UP: case PG_DOWN: p->idx = (p->idx + 1) % n; break;
        case PG_PINGPONG:                         // TEMPO's: step, and at an end step back + turn (ends repeat)
            p->idx += p->dir;
            if (p->idx > n - 1) { p->idx = n - 1; p->dir = -1; }
            if (p->idx < 0)     { p->idx = 0;     p->dir =  1; }
            break;
        case PG_RANDOM: p->idx = (int)pg__pick(p, (uint32_t)n); break;
    }
}

// one clock edge → the note to play, or PG_REST. A rest consumes the step and NOT the note.
static inline int pg_step(PatGen *p) {
    if (p->n == 0) { p->last = PG_REST; return PG_REST; }
    int plays = (PG_REST_MASK[p->rest] >> p->rest_idx) & 1u;
    p->rest_idx = (p->rest_idx + 1) % PG_REST_STEPS;
    if (!plays) { p->last = PG_REST; return PG_REST; }
    if (!p->started) {                            // the first note of a run: where each order starts
        p->started = 1; p->dir = 1;
        p->idx = (p->order == PG_RANDOM) ? (int)pg__pick(p, (uint32_t)p->n) : 0;
    } else {
        pg__advance(p);
    }
    if (p->idx >= p->n) p->idx = 0;
    int v[PG_MAX]; pg__view(p, v);
    int midi = v[p->idx];
    if (p->octave_chance > 0.0f && pg__unit(p) < p->octave_chance)
        midi += pg__pick(p, 2u) ? 12 : -12;
    p->last = midi;
    return midi;
}

// ── SELF-CHECK (spec.h's "specs on an includeable": the cart's spec() calls patgen_selfcheck()) ──
#ifdef DE_SPEC
#include "spec.h"
static inline void pg__run(PatGen *p, int steps, int *out) { for (int i = 0; i < steps; i++) out[i] = pg_step(p); }
static inline int pg__same(const int *a, const int *b, int n) { for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0; return 1; }

static inline void patgen_selfcheck(void) {
    PatGen p; int o[24];
    pg_init(&p, 7);
    pg_add(&p, 64); pg_add(&p, 60); pg_add(&p, 67);         // pressed out of pitch order on purpose
    pg__run(&p, 6, o);
    { int want[6] = { 64, 60, 67, 64, 60, 67 }; expect(pg__same(o, want, 6), "SEQ plays the keys in PRESS order"); }

    pg_restart(&p); p.order = PG_UP;   pg__run(&p, 4, o);
    { int want[4] = { 60, 64, 67, 60 }; expect(pg__same(o, want, 4), "UP plays low to high and wraps"); }
    pg_restart(&p); p.order = PG_DOWN; pg__run(&p, 4, o);
    { int want[4] = { 67, 64, 60, 67 }; expect(pg__same(o, want, 4), "DOWN plays high to low and wraps"); }
    pg_restart(&p); p.order = PG_PINGPONG; pg__run(&p, 8, o);
    { int want[8] = { 60, 64, 67, 67, 64, 60, 60, 64 }; expect(pg__same(o, want, 8), "PINGPONG repeats its end notes, as TEMPO's did"); }

    // the core: a rest takes the STEP, not the note. 3 notes against XXX. (3 plays / 4 steps) = a fixed
    // 4-step phrase; 4 notes against it = the gap drifts, 4·4/gcd(4,3) = 16 steps to come round.
    pg_restart(&p); p.order = PG_SEQ; p.rest = PG_REST_3_1; pg__run(&p, 8, o);
    { int want[8] = { 64, 60, 67, PG_REST, 64, 60, 67, PG_REST };
      expect(pg__same(o, want, 8), "XXX. against 3 notes: the rest does not eat a note, so the gap stays at the end"); }
    pg_add(&p, 69); pg_restart(&p);
    int o2[32]; pg__run(&p, 32, o2);
    { int want[16] = { 64, 60, 67, PG_REST, 69, 64, 60, PG_REST, 67, 69, 64, PG_REST, 60, 67, 69, PG_REST };
      expect(pg__same(o2, want, 16), "XXX. against 4 notes: the gap lands after a different note every pass"); }
    { int period = 0;
      for (int k = 1; k <= 16 && !period; k++) { int ok = 1; for (int i = 0; i + k < 32; i++) if (o2[i] != o2[i + k]) { ok = 0; break; } if (ok) period = k; }
      expect_eq(period, 16, "…so that phrase takes 16 steps to come round"); }
    pg_remove(&p, 69); p.rest = PG_REST_NONE;

    int plays = 0; for (int i = 0; i < PG_REST_STEPS; i++) plays += (PG_REST_MASK[PG_REST_10] >> i) & 1u;
    expect_eq(plays, 12, "the X.XX.X.XX. mask plays 12 of its 20 steps");
    expect_eq((long)(PG_REST_MASK[PG_REST_10] & 0x3FFu), (long)(PG_REST_MASK[PG_REST_10] >> 10), "…and has period 10");

    // RANDOM: deterministic per seed, and always one of the held notes
    PatGen a, b; pg_init(&a, 99); pg_init(&b, 99);
    for (int i = 0; i < 4; i++) { pg_add(&a, 60 + i); pg_add(&b, 60 + i); }
    a.order = b.order = PG_RANDOM;
    int ra[24], rb[24]; pg__run(&a, 24, ra); pg__run(&b, 24, rb);
    expect(pg__same(ra, rb, 24), "RANDOM is deterministic for a given seed");
    { int ok = 1; for (int i = 0; i < 24; i++) if (ra[i] < 60 || ra[i] > 63) ok = 0; expect(ok, "RANDOM only ever plays a held note"); }
    { int periodic = 0;                       // an LCG's low bits would make "random" a fixed short loop
      for (int k = 1; k <= 8 && !periodic; k++) { int ok = 1; for (int i = 0; i + k < 24; i++) if (ra[i] != ra[i + k]) { ok = 0; break; } periodic = ok; }
      expect(!periodic, "RANDOM is not a short repeating loop (no LCG low-bit cycle)"); }

    // octave chance: 0 never moves a note, 1 always moves it by exactly an octave
    pg_init(&p, 5); pg_add(&p, 60); p.octave_chance = 0.0f; pg__run(&p, 24, o);
    { int ok = 1; for (int i = 0; i < 24; i++) if (o[i] != 60) ok = 0; expect(ok, "octave_chance 0 never jumps"); }
    p.octave_chance = 1.0f; pg__run(&p, 24, o);
    { int up = 0, dn = 0, bad = 0; for (int i = 0; i < 24; i++) { if (o[i] == 72) up++; else if (o[i] == 48) dn++; else bad++; }
      expect(bad == 0 && up > 0 && dn > 0, "octave_chance 1 always jumps an octave, both ways"); }

    // unlatching a note that already played keeps the line pointing at the same NEXT note
    pg_init(&p, 1); pg_add(&p, 60); pg_add(&p, 62); pg_add(&p, 64); pg_add(&p, 65);
    pg_step(&p); pg_step(&p); pg_step(&p);                   // played 60 62 64; next would be 65
    pg_remove(&p, 60);
    expect_eq(pg_step(&p), 65, "removing an already-played note does not skip the next one");
    expect_eq(p.n, 3, "…and the note is gone");
    pg_clear(&p);
    expect_eq(pg_step(&p), PG_REST, "nothing held = a rest");
    for (int i = 0; i < PG_MAX + 4; i++) pg_add(&p, 30 + i);
    expect_eq(p.n, PG_MAX, "the latch clamps at PG_MAX");
}
#endif  // DE_SPEC

#endif  // PATGEN_H
