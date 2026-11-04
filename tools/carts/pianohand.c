/* de:meta
{
  "slug": "pianohand",
  "title": "pianohand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "adsr-envelope",
    "chord-voicing"
  ],
  "lineage": "The humanized-playing piano spike (docs/design/humanized-playing.md section 7.1), after strumhand, breathflute, tablahand, reedhand and basshand. The new kind of rule it tests: ONE curve shared by every sounding note (the sustain pedal), and how a note ENDS. INSTR_PIANO notes are all BOOKED (schedule_note_at); the pedal is planned ahead by the player and turned into each note's gate and release, because a booked note cannot be reached after its onset.",
  "description": {
    "summary": "A short original piece for two hands on INSTR_PIANO, played by a model of a pianist: a sustain pedal that changes just AFTER each new chord (legato pedalling), a dry passage, a half-pedal passage, a final chord let go by a slow lift. Melody voiced over the accompaniment, chords slightly apart, a phrase that breathes. TAB flips to the sequenced version.",
    "detail": "The left hand plays broken chords, the right hand a melody. The pianist plans each phrase: where it swells, how much the melody sings over the accompaniment, how the tempo pushes and broadens. The pedal is a curve: pressed after the first note, lifted a moment after each new bass note so the old harmony clears without a gap, held at half depth in a misty passage, and raised slowly over the last chord. Every note ends when its damper falls: when the key comes up with the pedal up, or later when the pedal lifts. Staccato keys let go fast, bass dampers are slower, the top octave has no dampers at all. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB pianist / sequenced · L pedal as a lane (one slot) / computed gates · 1 time 2 dynamics 3 pedal 4 endings 5 pedal noise+halo · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// pianohand — a spike: a piano played by a model of a pianist, and above all its SUSTAIN PEDAL.
//
// What the engine offers (sound.h, read-only, 2026-11-04):
//   - INSTR_PIANO's morph ("pedal") sets the string's LOOP LOSS once, at note-on
//     (sound_piano_start: `pn_dampg = pn_damps = v->mor`). The per-sample loop reads pn_dampg only, so
//     note_morph / NoteOpts.morph after the onset never reach it: the pedal macro is a per-note,
//     onset-only "how long does this string ring", not a pedal. (Measured: PROBE=1.)
//   - A note ENDS through the slot's ADSR release, copied into the voice at note-on (v->r_samp), and the
//     release is a LINEAR amplitude ramp.
//   - A booked note has no handle, so nothing touches it after its onset.
// So this cart treats morph as "the undamped string" (constant), and builds the pedal itself: the
// pianist PLANS the pedal curve ahead, and each note's gate = the moment its damper falls (key up with
// the pedal up, or the later pedal lift), its release = how the damper falls (key let go fast, a slow
// pedal lift, half pedal, a heavy bass damper). Release speed is per SLOT, so five identical piano slots
// differ only in release time and the model picks one per note.
// Half pedal (dampers brushing the strings) cannot be a decay RATE on a booked note: here it becomes a
// slow release starting when the pedal drops into the half zone. PROBE=5 measures the live alternative
// (held note_on notes with note_vol ridden per frame), which can do a real shared curve but loses the
// booked onsets.

#include "studio.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define NREL 5
#define SL_P0  10                 // piano slots 10..14: same voice, release 70/180/450/1100/2600 ms
#define SL_HALO 15                // sympathetic halo: a soft, slow-attack piano under the pedalled bass
#define SL_PNOISE 16              // pedal / damper-rail noise
static const int REL_MS[NREL] = { 70, 180, 450, 1100, 2600 };
#define LOOKAHEAD 0.15

#ifndef PNO_MORPH
#define PNO_MORPH 0.85f           // the UNDAMPED string (the dampers are the cart's job)
#endif

// ---------------------------------------------------------------- the score (original)
// beats in 4/4. Sections: A pedalled [0,16) · B dry [16,24) · C half pedal [24,32) · D pedalled [32,53)
typedef struct { float b, len; int midi; int u[3]; char art; } Mel;   // art: l legato, s staccato, e phrase end
static const Mel MEL[] = {
    {0,1.5f,69,{0},'l'},{1.5f,.5f,71,{0},'l'},{2,1,74,{0},'l'},{3,1,78,{0},'l'},
    {4,1.5f,76,{0},'l'},{5.5f,.5f,74,{0},'l'},{6,1,73,{0},'l'},{7,1,71,{0},'l'},
    {8,1,71,{0},'l'},{9,1,74,{0},'l'},{10,1.5f,79,{0},'l'},{11.5f,.5f,78,{0},'l'},
    {12,3,76,{73,69},'e'},{15,1,69,{0},'l'},
    {16,.5f,74,{0},'s'},{16.5f,.5f,78,{0},'s'},{17,.5f,81,{0},'s'},{17.5f,.5f,78,{0},'s'},{18,1,79,{0},'s'},{19,1,76,{0},'s'},
    {20,.5f,73,{0},'s'},{20.5f,.5f,76,{0},'s'},{21,.5f,79,{0},'s'},{21.5f,.5f,76,{0},'s'},{22,1,78,{0},'s'},{23,1,74,{0},'e'},
    {24,1,71,{0},'l'},{25,1,76,{0},'l'},{26,1,79,{0},'l'},{27,.5f,78,{0},'l'},{27.5f,.5f,76,{0},'l'},
    {28,1,73,{0},'l'},{29,1,76,{0},'l'},{30,2,81,{76,73},'e'},
    {32,1.5f,83,{0},'l'},{33.5f,.5f,81,{0},'l'},{34,1,78,{0},'l'},{35,1,74,{0},'l'},
    {36,1,79,{0},'l'},{37,1,83,{0},'l'},{38,1.5f,86,{82,79},'l'},{39.5f,.5f,82,{0},'l'},
    {40,2,81,{78,74},'l'},{42,1,79,{0},'l'},{43,1,76,{0},'l'},
    {44,5,86,{81,78,74},'e'},
};
#define NMEL ((int)(sizeof MEL / sizeof MEL[0]))
// left hand, one chord per half bar: bass + three upper tones
static const int LHC[24][4] = {
    {38,45,50,54},{38,45,50,54}, {35,42,47,50},{35,42,47,50}, {31,38,43,47},{31,38,43,47}, {33,40,45,49},{33,40,43,49},
    {38,45,50,54},{33,45,50,54}, {33,43,49,52},{40,43,49,52}, {40,47,52,55},{40,47,52,55}, {33,40,45,49},{33,40,43,49},
    {35,42,47,50},{35,42,47,50}, {31,38,43,47},{31,38,43,46}, {33,42,45,50},{33,40,43,49}, {38,45,50,54},{38,45,50,54},
};
#define PIECE_BEATS 53.0f
static int section_of(float b) { return b < 16 ? 0 : b < 24 ? 1 : b < 32 ? 2 : 3; }
static const char *SEC_NAME[4] = { "A pedal", "B dry", "C half pedal", "D pedal" };
// the phrases (for dynamics + rubato)
static const float PHR[5] = { 0, 16, 24, 32, 53 };

// ---------------------------------------------------------------- state
enum { L_TIME, L_DYN, L_PEDAL, L_END, L_EXTRA, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "dyn", "pedal", "ends", "extra" };
static bool  playing = true;
static bool human = true;
static bool lane_mode = false;   // L: the pedal as LANE_DAMPER on ONE piano slot (NoteOpts step 4) instead of computed gates
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 72;
static int   seed = 1;

// tempo map: seconds at beat i/16
#define TMN (64 * 16 + 1)
static double TM[TMN];
static double time_of(float b) {
    if (b <= 0) return b * 60.0 / tempo;
    float x = b * 16.0f; int i = (int)x; if (i >= TMN - 1) return TM[TMN - 1] + (b - (TMN - 1) / 16.0f) * 60.0 / tempo;
    return TM[i] + (TM[i + 1] - TM[i]) * (x - i);
}
static float beat_of(double t) {
    if (t <= 0) return 0;
    int lo = 0, hi = TMN - 1;
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (TM[m] <= t) lo = m; else hi = m; }
    double f = (TM[hi] > TM[lo]) ? (t - TM[lo]) / (TM[hi] - TM[lo]) : 0;
    return (lo + (float)f) / 16.0f;
}

// pedal curve: breakpoints (beat, depth 0 up .. 1 down)
#define NPED 96
static float ped_b[NPED], ped_d[NPED];
static int   nped;
static float pedal_at(float b) {
    if (nped == 0) return 0;
    if (b <= ped_b[0]) return ped_d[0];
    for (int i = 1; i < nped; i++)
        if (b < ped_b[i]) { float k = (b - ped_b[i-1]) / fmaxf(1e-4f, ped_b[i] - ped_b[i-1]); return ped_d[i-1] + (ped_d[i] - ped_d[i-1]) * k; }
    return ped_d[nped - 1];
}
static void ped_add(float b, float d) {
    if (nped >= NPED) return;
    if (nped && b <= ped_b[nped - 1]) b = ped_b[nped - 1] + 0.001f;
    ped_b[nped] = b; ped_d[nped] = d; nped++;
}

// the planned notes
typedef struct {
    double t;            // onset, piece seconds
    double t_up;         // key up, piece seconds (display)
    float  key_rel;      // the key's own release, ms (lane mode: the pedal lane does the rest)
    double t_damp;       // damper starts (gate end), piece seconds
    int    midi, slot, gate_ms, hand;   // hand 0 LH 1 RH 2 halo 3 pedal noise
    float  gain, timb;
} Ev;
#define NEV 400
static Ev ev[NEV];
static int nev;
static int ev_next;              // next event to book
static double piece_t0;          // audio time of beat 0
static int ped_next;              // lane mode: the next pedal breakpoint to book
static double piece_len;
static int max_voices;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float drift(float x, float k, float off) { return noise(x * k + seed * 13.1f + off) * 2.0f - 1.0f; }

// ---------------------------------------------------------------- sound
static void setup_sound(void) {
    for (int i = 0; i < NREL; i++) {
        int sl = SL_P0 + i;
        instrument(sl, INSTR_PIANO, 1, 0, 7, REL_MS[i]);
        instrument_harmonics(sl, 0.08f);       // piano/grand (instrument-recipes.md)
        instrument_timbre(sl, 0.48f);
        instrument_morph(sl, PNO_MORPH);
        instrument_reverb(sl, 0.22f);
    }
    // the halo: the open strings answering a pedalled bass note (sympathetic resonance), a slow swell
    instrument(SL_HALO, INSTR_PIANO, 140, 0, 7, 900);
    instrument_harmonics(SL_HALO, 0.08f);
    instrument_timbre(SL_HALO, 0.15f);
    instrument_morph(SL_HALO, 0.9f);
    instrument_mode(SL_HALO, MODE_PIANO_KNOCK, 0.0f);
    instrument_reverb(SL_HALO, 0.35f);
    // the damper rail: felt on wood, a low soft thump
    instrument(SL_PNOISE, INSTR_NOISE, 2, 70, 0, 40);
    instrument_filter(SL_PNOISE, FILTER_LOW, 380, 2);
    instrument_reverb(SL_PNOISE, 0.15f);
    reverb(0.55f, 0.5f);
    eq(8.0f, 8.0f, 7.0f);   // master level: the piano alone peaked at -15 dBFS (no master volume; the EQ in dB does it)
#ifdef PROBE
    for (int i = 0; i < NREL; i++) instrument_reverb(SL_P0 + i, 0.0f);   // probes measure the dry string
#endif
}

// ---------------------------------------------------------------- the pianist's plan
static void build_tempo(bool H_on) {
    float H = humanity;
    bool T = H_on && layer[L_TIME];
    double sec = 0;
    for (int i = 0; i < TMN; i++) {
        TM[i] = sec;
        float b = i / 16.0f;
        float f = 1.0f;
        if (T) {
            int p = 0; while (p < 3 && b >= PHR[p + 1]) p++;
            float b0 = PHR[p], b1 = PHR[p + 1], x = (b - b0) / (b1 - b0);
            f += 0.045f * sinf(3.14159f * fminf(x, 1)) * H;                       // push through the middle
            float endw = (p == 3) ? 9.0f : 1.5f;                                  // broaden into the cadence
            float tail = b - (b1 - endw);
            if (p == 3) tail = b - 40.0f, endw = 9.0f;
            if (tail > 0 && b < b1) f *= 1.0f - ((p == 3) ? 0.32f : 0.14f) * powf(fminf(tail / endw, 1), 2) * H;
            if (b >= b0 && b < b0 + 0.25f && p > 0 && p != 1) f *= 1.0f - 0.25f * H;   // a breath into the phrase
            f += drift(b, 0.12f, 0) * 0.03f * H;                                  // slow wander
        }
        sec += (60.0 / tempo) / f / 16.0;
    }
}

static void build_pedal(bool H_on) {
    float H = humanity;
    nped = 0;
    if (!H_on || !layer[L_PEDAL]) {
        // the sequencer pedal: down for the pedalled sections, changed exactly ON the barline (a gap or a blur)
        ped_add(0, 1);
        for (int bar = 1; bar < 4; bar++) { ped_add(bar * 4 - 0.02f, 1); ped_add(bar * 4 - 0.01f, 0); ped_add(bar * 4, 1); }
        ped_add(15.99f, 1); ped_add(16, 0);
        ped_add(31.99f, 0); ped_add(32, 1);
        for (int bar = 9; bar < 12; bar++) { ped_add(bar * 4 - 0.02f, 1); ped_add(bar * 4 - 0.01f, 0); ped_add(bar * 4, 1); }
        ped_add(50.99f, 1); ped_add(51, 0);
        return;
    }
    // the pianist: press after the first note sounds; at each change lift a moment AFTER the new bass
    // note (so its hammer is already on the string) and press again (legato / syncopated pedalling)
    ped_add(0, 0);
    float pd = 0.2f + 0.1f * rnd_float();
    ped_add(pd, 0); ped_add(pd + 0.10f, 1);
    static const float CH_A[3] = { 4, 8, 12 };
    static const float CH_D[5] = { 36, 38, 40, 42, 44 };
    for (int i = 0; i < 3; i++) {
        float c = CH_A[i], ld = (0.10f + 0.10f * rnd_float()) * fmaxf(0.4f, H), up = 0.08f + 0.07f * rnd_float();
        ped_add(c + ld, 1); ped_add(c + ld + 0.06f, 0); ped_add(c + ld + 0.06f + up, 0); ped_add(c + ld + 0.14f + up, 1);
    }
    // into the dry passage: clear the pedal just after the first dry note, stay up
    ped_add(16.12f, 1); ped_add(16.20f, 0);
    // half pedal: the dampers brush the strings (a mist that clears itself), with a quick change at the bar
    ped_add(24.15f, 0); ped_add(24.30f, 0.5f);
    ped_add(28.12f, 0.5f); ped_add(28.18f, 0.05f); ped_add(28.30f, 0.05f); ped_add(28.40f, 0.5f);
    // full pedal again: change after the first note of D
    ped_add(32.12f, 0.5f); ped_add(32.18f, 0); ped_add(32.28f, 0); ped_add(32.36f, 1);
    for (int i = 0; i < 5; i++) {
        float c = CH_D[i], ld = (0.10f + 0.10f * rnd_float()) * fmaxf(0.4f, H), up = 0.08f + 0.07f * rnd_float();
        ped_add(c + ld, 1); ped_add(c + ld + 0.06f, 0); ped_add(c + ld + 0.06f + up, 0); ped_add(c + ld + 0.14f + up, 1);
    }
    // the last chord rings, then a SLOW lift over ~3 beats (the dampers settle gradually)
    float lift0 = 49.2f + 0.4f * r1() * H;
    ped_add(lift0, 1); ped_add(lift0 + 1.0f, 0.75f); ped_add(lift0 + 2.2f, 0.35f); ped_add(lift0 + 3.2f, 0);
}

// release time -> nearest slot (log distance)
static int slot_for_rel(float ms) {
    int best = 0; float bd = 1e9f;
    for (int i = 0; i < NREL; i++) { float d = fabsf(logf(ms / REL_MS[i])); if (d < bd) { bd = d; best = i; } }
    return SL_P0 + best;
}

static Ev *add_ev(void) { if (nev >= NEV) return NULL; Ev *e = &ev[nev++]; memset(e, 0, sizeof *e); return e; }

// the damper: when does it fall on a note whose key comes up at beat k1, and how fast?
// Returns the gate end (piece seconds) and writes the release in ms.
static double damper(float k1, int midi, float key_rel_ms, bool H_on, float *rel_ms) {
    bool E = H_on && layer[L_END];
    if (E && midi >= 89) { *rel_ms = 1100; return time_of(k1) + 7.0; }      // the top octave has no dampers
    float reg = E ? 1.0f + clamp((60 - midi) / 24.0f, 0, 1.2f) : 1.0f;     // heavier bass strings stop slower
    if (pedal_at(k1) < 0.7f && pedal_at(k1) < 0.15f) { *rel_ms = key_rel_ms * reg; return time_of(k1); }
    // scan forward: first damper contact (< 0.7), then full contact (< 0.15)
    float b = k1, bc = -1, bf = -1;
    for (; b < PIECE_BEATS + 4; b += 1.0f / 64) { if (pedal_at(b) < 0.7f) { bc = b; break; } }
    if (bc < 0) { *rel_ms = 2600; return time_of(PIECE_BEATS + 4); }
    for (float c = bc; c < bc + 6.0f; c += 1.0f / 64) { if (pedal_at(c) < 0.15f) { bf = c; break; } }
    double tc = time_of(bc);
    float base = (bc <= k1 + 1e-3f) ? key_rel_ms : 140.0f;                  // a pedal lift drops them like a key
    // HALF PEDAL: the depth RESTS in the contact zone (the dampers brush the strings): a slow fade from
    // contact. A booked note cannot change its decay RATE, so the fade is a long release instead.
    bool resting = pedal_at(bc + 0.5f) > 0.3f && pedal_at(bc + 0.5f) >= pedal_at(bc) - 0.1f;
    if (bf < 0 || resting) {
        float r = 900.0f * reg;
        if (bf >= 0) r = fminf(r, base + (float)(time_of(bf) - tc) * 1000.0f);
        *rel_ms = r; return tc;
    }
    float closing = (float)(time_of(bf) - tc) * 1000.0f;                    // a slow LIFT: the dampers settle as it rises
    *rel_ms = (base + closing) * reg;
    return tc;
}

static void book_note(float k0, float k1, int midi, int hand, float gain, float timb, double toff,
                      float key_rel_ms, bool H_on) {
    Ev *e = add_ev(); if (!e) return;
    float rel;
    double tdamp = damper(k1, midi, key_rel_ms, H_on, &rel);
    if (!H_on) rel = 180;                                   // the sequencer: one release for everything
    e->t = time_of(k0) + toff;
    e->t_up = time_of(k1) + toff;
    e->t_damp = (fabs(tdamp - time_of(k1)) < 1e-9) ? e->t_up : tdamp;   // at key up: the key's own timing
    if (e->t_damp < e->t_up) e->t_damp = e->t_up;
    e->midi = midi; e->hand = hand; e->slot = slot_for_rel(rel);
    {   // the key's own release, as damper() would give it at key-up with the pedal up (lane mode)
        bool E = H_on && layer[L_END];
        float reg = E ? 1.0f + clamp((60 - midi) / 24.0f, 0, 1.2f) : 1.0f;
        e->key_rel = (E && midi >= 89) ? 1100.0f : key_rel_ms * reg;      // the top octave has no dampers
    }
    e->gain = clamp(gain, 0.04f, 1.0f);                     // NoteOpts gain 0 means FULL
    e->timb = timb;
}

// dynamics: the phrase plan
static float dyn_at(float b, bool H_on) {
    if (!H_on || !layer[L_DYN]) return 0.65f;
    float H = humanity;
    int s = section_of(b);
    float base = s == 0 ? 0.55f : s == 1 ? 0.62f : s == 2 ? 0.40f : 0.55f;
    if (s == 3) {   // D builds to its peak at the Gm, then falls away into the last chord
        if (b < 39) base = 0.55f + 0.30f * (b - 32) / 7.0f;
        else if (b < 44) base = 0.85f - 0.30f * (b - 39) / 5.0f;
        else base = 0.52f;
    }
    int p = 0; while (p < 3 && b >= PHR[p + 1]) p++;
    float x = (b - PHR[p]) / (PHR[p + 1] - PHR[p]);
    float arc = 0.10f * sinf(3.14159f * x);
    float d = base + (arc + drift(b, 0.25f, 50) * 0.05f) * H;
    return 0.65f + (d - 0.65f) * fminf(H, 1.5f);
}

static void build(void) {
    bool H_on = human;
    float H = humanity;
    rnd_seed(seed * 7919 + (H_on ? 1 : 0));
    build_tempo(H_on);
    build_pedal(H_on);
    nev = 0;
    bool T = H_on && layer[L_TIME], D = H_on && layer[L_DYN], E = H_on && layer[L_END], X = H_on && layer[L_EXTRA];

    // ---- right hand: the melody, voiced over its own chord notes
    for (int i = 0; i < NMEL; i++) {
        const Mel *m = &MEL[i];
        float k0 = m->b, len = m->len, k1 = k0 + len;
        float next_b = (i + 1 < NMEL) ? MEL[i + 1].b : 1e9f;
        float rel_key = 180;
        if (E) {
            if (m->art == 's')      { k1 = k0 + len * (0.32f + 0.15f * rnd_float()); rel_key = 70; }     // let go fast
            else if (m->art == 'e') { k1 = k0 + len * (0.86f + 0.08f * rnd_float()); rel_key = 450; }    // a gentle lift
            else if (fabsf(next_b - (k0 + len)) < 1e-3f) {
                float ov = (0.02f + 0.03f * rnd_float()) * tempo / 60.0f;                              // finger legato: 20-50 ms overlap
                k1 = next_b + ov; rel_key = 180;
            }
            if (i == NMEL - 1) k1 = 49.0f;
        } else if (!H_on && m->art == 's') { k1 = k0 + len * 0.5f; }
        float dyn = dyn_at(k0, H_on);
        float g = D ? dyn * (1.0f + (m->midi - 76) * 0.008f) + r1() * 0.03f * H : 0.65f;
        // the melody leads: louder notes land a little earlier (the "melody lead")
        double lead = T ? -(0.006 + 0.014 * dyn) * H + r1() * 0.003 * H : 0;
        float tb = D ? r1() * 0.02f * H : 0;
        book_note(k0, k1, m->midi, 1, g, tb, lead, rel_key, H_on);
        for (int u = 0; u < 3 && m->u[u]; u++) {
            double spread = T ? lead + (0.003 + 0.004 * u + 0.003 * rnd_float()) * H : 0;   // top voice first
            float gu = D ? g * (0.58f + 0.06f * r1() * H) : 0.65f;
            book_note(k0, k1, m->u[u], 1, gu, tb - 0.03f, spread, rel_key, H_on);
        }
    }

    // ---- left hand
    for (int hb = 0; hb < 24; hb++) {
        float b0 = hb * 2.0f;
        int s = section_of(b0);
        const int *c = LHC[hb];
        double hand_off = T ? drift(b0, 0.3f, 90) * 0.008 * H : 0;                    // the hands drift apart
        if (s == 1) {
            // dry: oom-pah, the bass held a little, the chord detached
            for (int q = 0; q < 2; q++) {
                float kb = b0 + q;
                if (q == 0) {
                    float k1 = kb + (E ? 0.55f + 0.2f * rnd_float() : 0.5f);
                    float g = D ? dyn_at(kb, H_on) * 0.72f + r1() * 0.03f * H : 0.65f;
                    book_note(kb, k1, c[0], 0, g, 0, hand_off + (T ? r1() * 0.003 * H : 0), E ? 120 : 180, H_on);
                } else {
                    float k1 = kb + (E ? 0.30f + 0.12f * rnd_float() : 0.5f);
                    for (int u = 1; u < 4; u++) {
                        float g = D ? dyn_at(kb, H_on) * (0.46f + (u == 3 ? 0.04f : 0)) + r1() * 0.03f * H : 0.65f;
                        double sp = T ? (u - 1) * 0.002 * H + r1() * 0.002 * H : 0;
                        book_note(kb, k1, c[u], 0, g, 0, hand_off + sp, E ? 70 : 180, H_on);
                    }
                }
            }
            continue;
        }
        if (hb == 22 || hb == 23) {
            if (hb == 23) continue;
            // the final chord: rolled from the bass up, held, then released into the pedal
            for (int u = 0; u < 4; u++) {
                float kb = 44.0f;
                double roll = T ? (0.030 * u) * H + r1() * 0.004 * H : 0;
                float g = D ? 0.52f * (u == 0 ? 0.78f : 0.52f) : 0.65f;
                book_note(kb, E ? 49.0f + 0.1f * u : 50.0f, c[u], 0, g, 0, hand_off + roll, 450, H_on);
                if (X && u == 0 && c[u] < 55) {
                    int src = nev - 1;
                    for (int k = 0; k < 2; k++) {
                        Ev *e = add_ev(); if (!e) break;
                        *e = ev[src];
                        e->midi = c[u] + (k ? 19 : 12); e->hand = 2; e->slot = SL_HALO;
                        e->gain = 0.10f; e->t += 0.03;
                    }
                }
            }
            continue;
        }
        // broken chord: bass, then the three upper tones; the bass held by the finger for the half bar
        for (int k = 0; k < 4; k++) {
            float kb = b0 + k * 0.5f;
            int midi = c[k];
            float k1 = kb + 0.5f;
            if (E) k1 = (k == 0) ? b0 + 1.8f + 0.15f * rnd_float() : kb + 0.5f * (1.0f + 0.15f * rnd_float());
            float dyn = dyn_at(kb, H_on);
            float g = 0.65f;
            if (D) g = dyn * (k == 0 ? 0.70f : k == 2 ? 0.52f : 0.46f) + r1() * 0.035f * H;
            double jit = T ? r1() * 0.003 * H : 0;
            book_note(kb, k1, midi, 0, g, 0, hand_off + jit, 180, H_on);
            // the halo: a pedalled bass note sets the open strings above it ringing (sympathetic)
            if (X && k == 0 && pedal_at(kb + 0.3f) > 0.4f && midi < 55) {
                Ev *src = &ev[nev - 1];
                for (int o = 0; o < 2; o++) {
                    Ev *e = add_ev(); if (!e) break;
                    *e = *src;
                    e->midi = midi + (o ? 19 : 12); e->hand = 2; e->slot = SL_HALO;
                    e->gain = (0.06f + 0.03f * rnd_float()) * pedal_at(kb + 0.3f); e->t += 0.03;
                }
            }
        }
    }

    // ---- pedal noise: the damper rail lifting (down) and the felt landing (up)
    if (X && layer[L_PEDAL]) {
        for (int i = 1; i < nped; i++) {
            bool down = ped_d[i] > 0.6f && ped_d[i - 1] < 0.4f, up = ped_d[i] < 0.4f && ped_d[i - 1] > 0.6f;
            if (!down && !up) continue;
            Ev *e = add_ev(); if (!e) break;
            e->t = time_of(ped_b[i]); e->t_up = e->t_damp = e->t + 0.05;
            e->midi = 60; e->hand = 3; e->slot = SL_PNOISE;
            e->gain = (up ? 0.20f : 0.14f) * (0.8f + 0.4f * rnd_float()) * fminf(1.2f, humanity + 0.2f);
        }
    }

    // ---- one string per pitch: a re-struck note's earlier voice stops when the hammer hits again
    for (int i = 0; i < nev; i++) {
        if (ev[i].hand >= 2) continue;
        for (int j = 0; j < nev; j++) {
            if (j == i || ev[j].hand >= 2 || ev[j].midi != ev[i].midi) continue;
            if (ev[j].t > ev[i].t && ev[j].t < ev[i].t_damp + REL_MS[ev[i].slot - SL_P0] / 1000.0) {
                if (ev[j].t < ev[i].t_damp) ev[i].t_damp = ev[j].t;
                if (ev[i].t_up > ev[i].t_damp) ev[i].t_up = ev[i].t_damp;
                ev[i].slot = SL_P0;                       // stopped by the hammer: quick
            }
        }
    }
    // gates + sort by onset
    piece_len = 0;
    for (int i = 0; i < nev; i++) {
        double g = ev[i].t_damp - ev[i].t; if (g < 0.03) g = 0.03;
        ev[i].gate_ms = (int)(g * 1000.0);
        double end = ev[i].t_damp + (ev[i].hand == 2 ? 0.9 : ev[i].hand == 3 ? 0.1 : REL_MS[ev[i].slot - SL_P0] / 1000.0);
        if (end > piece_len) piece_len = end;
    }
    for (int i = 1; i < nev; i++) { Ev k = ev[i]; int j = i - 1; while (j >= 0 && ev[j].t > k.t) { ev[j + 1] = ev[j]; j--; } ev[j + 1] = k; }
    // voice budget: how many notes sound at once at worst (onset .. end of release)
    max_voices = 0;
    for (int i = 0; i < nev; i++) {
        int n = 0;
        for (int j = 0; j < nev; j++) {
            if (ev[j].hand == 3) continue;
            double end = ev[j].t_damp + (ev[j].hand == 2 ? 0.9 : REL_MS[ev[j].slot - SL_P0] / 1000.0);
            if (ev[j].t <= ev[i].t && end > ev[i].t) n++;
        }
        if (n > max_voices) max_voices = n;
    }
}

// rebuild in place (TAB, layers, humanity, take): keep the score position, rebook from now on
static void rebuild(void) {
    double now_a = audio_time();
    float b_now = beat_of(now_a - piece_t0);
    if (now_a - piece_t0 < 0) b_now = 0;
    for (int s = SL_P0; s <= SL_PNOISE; s++) unschedule(s, now_a, 0);   // notes AND booked lane points
    lane_at(now_a, SL_P0, LANE_DAMPER, 0, 0);
    build();
    if (b_now > 0 && b_now < PIECE_BEATS) piece_t0 = now_a + 0.05 - time_of(b_now);
    ev_next = 0;
    while (ev_next < nev && piece_t0 + ev[ev_next].t < now_a + 0.02) ev_next++;
    ped_next = 0;
    while (ped_next < nped && piece_t0 + time_of(ped_b[ped_next]) < now_a) ped_next++;
    if (ped_next > 0) ped_next--;   // re-book the segment we are inside
}

// lane mode: the pedal curve itself, booked as LANE_DAMPER ramps on the piano slot. Pedal DEPTH -> damper
// lift: full contact below 0.15, free above 0.7, the contact zone between = half pedal (a slower release)
static float damper_of(float depth) { return clamp((depth - 0.15f) / 0.55f, 0, 1); }

static void book_ahead(void) {
    double now_a = audio_time();
    if (now_a > piece_t0 + piece_len + 1.5) { piece_t0 = now_a + 0.4; ev_next = 0; ped_next = 0; }   // loop the piece
    if (lane_mode) {
        while (ped_next < nped && piece_t0 + time_of(ped_b[ped_next]) < now_a + LOOKAHEAD) {
            int i = ped_next++;
            double t = piece_t0 + time_of(ped_b[i]);
            if (t < now_a) t = now_a;
            if (i + 1 < nped) {   // ramp from this breakpoint's depth to the next one's
                int ramp = (int)((time_of(ped_b[i + 1]) - time_of(ped_b[i])) * 1000.0);
                lane_at(t, SL_P0, LANE_DAMPER, damper_of(ped_d[i]), 0);
                lane_at(t, SL_P0, LANE_DAMPER, damper_of(ped_d[i + 1]), ramp);
            } else lane_at(t, SL_P0, LANE_DAMPER, damper_of(ped_d[i]), 0);
        }
    }
    while (ev_next < nev && piece_t0 + ev[ev_next].t < now_a + LOOKAHEAD) {
        Ev *e = &ev[ev_next++];
        double t = piece_t0 + e->t;
        if (t < now_a) continue;
        NoteOpts o = { .gain = e->gain, .timbre = e->timb };
        if (lane_mode && (e->hand == 0 || e->hand == 1)) {
            // ONE piano slot: the note lasts as long as its KEY, with the key's own release; the pedal lane holds it
            o.release_ms = (int)e->key_rel;
            int key_ms = (int)((e->t_up - e->t) * 1000.0); if (key_ms < 20) key_ms = 20;
            schedule_note_at(t, e->midi, SL_P0, 7, key_ms, &o);
            continue;
        }
        schedule_note_at(t, e->midi, e->slot, 7, e->gate_ms, &o);
    }
}

// ---------------------------------------------------------------- probes (DE_DEFINES=PROBE=n)
#ifdef PROBE
static int ph = 0;
static void probe_init(void) {
    playing = false;
    double t0 = audio_time() + 0.2;
#if PROBE == 1   // is the pedal macro live on a held note? note_on C4 at PNO_MORPH, PROBE_MORPH_TO at 1.2 s
    (void)t0;
#elif PROBE == 2 // the undamped string: one note PROBE_NOTE held 8 s (decay vs morph via PNO_MORPH)
#ifndef PROBE_NOTE
#define PROBE_NOTE 60
#endif
    NoteOpts o = { .gain = 0.7f };
    schedule_note_at(t0, PROBE_NOTE, SL_P0, 7, 8000, &o);
#elif PROBE == 3 // a chord: key up at 0.5 s; PROBE_PED=1 the pedal holds it until 2.5 s, then the damper (180 ms slot)
    int ch[4] = { 48, 52, 55, 60 };
    for (int i = 0; i < 4; i++) {
        NoteOpts o = { .gain = 0.7f };
        schedule_note_at(t0, ch[i], SL_P0 + 1, 7, PROBE_PED ? 2500 : 500, &o);
    }
#elif PROBE == 4 // release shape: C4, gate 1.0 s, on release slot PROBE_SLOT (0..4)
    NoteOpts o = { .gain = 0.7f };
    schedule_note_at(t0, 60, SL_P0 + PROBE_SLOT, 7, 1000, &o);
#elif PROBE == 5 // the LIVE alternative: a held C4, from 1.2 s note_vol decays at half-pedal rate, 3.2 s pedal up
    (void)t0;
#endif
}
static void probe_update(void) {
    double t = audio_time();
#if PROBE == 1
    if (!ph && t > 0.2) { ph = 1; note_off_all(); ph = note_on(60, SL_P0 + 4, 5); }
#ifdef PROBE_MORPH_TO
    static bool done = false;
    if (ph && !done && t > 1.2) { note_morph(ph, PROBE_MORPH_TO); done = true; }
#endif
#elif PROBE == 5
    static float v = 5.0f; static bool off = false;
    if (!ph && t > 0.2) ph = note_on(60, SL_P0 + 1, 5);
    if (ph && t > 1.2 && t < 3.2) {          // half pedal: an extra -12 dB/s, frame by frame
        v *= powf(10.0f, -12.0f / 20.0f / 60.0f);
        note_vol(ph, v);
    }
    if (ph && t >= 3.2 && !off) { note_off(ph); off = true; }
#endif
    (void)t;
}
#endif

// ---------------------------------------------------------------- loop
void init(void) {
    setup_sound();
    build();
    piece_t0 = audio_time() + 0.3;
    ev_next = 0;
#ifdef LANE_MODE
    lane_mode = true;
#endif
#ifdef PROBE
    probe_init();
#endif
#ifdef DUMP_PLAN   // DE_DEFINES=DUMP_PLAN[,START_PLAIN]: print the planned notes (for measuring the plan)
#ifdef START_PLAIN
    human = false; build();
#endif
    for (int i = 0; i < nev; i++)
        printf("EV %.4f %.4f %.4f %d %d %d %d %.3f %.4f\n", ev[i].t, ev[i].t_up, ev[i].t_damp, ev[i].midi,
               ev[i].hand, ev[i].slot, ev[i].gate_ms, ev[i].gain, beat_of(ev[i].t));
    for (int i = 0; i < nped; i++) printf("PED %.3f %.3f %.4f\n", ped_b[i], ped_d[i], time_of(ped_b[i]));
#endif
}

void update(void) {
    bool rb = false;
    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) { piece_t0 = audio_time() + 0.2; ev_next = 0; } else note_off_all(); }
    if (keyp(KEY_TAB)) { human = !human; rb = true; }
    if (keyp('L')) { lane_mode = !lane_mode; rb = true; }
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) { layer[i] = !layer[i]; rb = true; }
    if (keyp(KEY_UP))    { humanity = fminf(1.5f, humanity + 0.25f); rb = true; }
    if (keyp(KEY_DOWN))  { humanity = fmaxf(0.0f, humanity - 0.25f); rb = true; }
    if (keyp(KEY_RIGHT)) { tempo = min(120, tempo + 4); rb = true; }
    if (keyp(KEY_LEFT))  { tempo = max(48, tempo - 4); rb = true; }
    if (keyp('R')) { seed++; rb = true; }
    if (rb) rebuild();
#ifdef PROBE
    probe_update();
#else
    if (playing) book_ahead();
#endif
#ifdef DE_TRACE
    watch("human", "%d", human);
    watch("beat_pos", "%.2f", beat_of(audio_time() - piece_t0));
    watch("pedal", "%.2f", pedal_at(beat_of(audio_time() - piece_t0)));
    watch("max_voices", "%d", max_voices);
    watch("nev", "%d", nev);
    watch("piece_len", "%.2f", piece_len);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    double at = audio_time() - piece_t0;
    float bnow = beat_of(at);
    int sec = section_of(bnow);
    print(human ? "PIANIST" : "SEQUENCED", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(lane_mode ? "L: pedal LANE, 1 slot" : "L: computed gates, 5 slots", 180, 4, lane_mode ? CLR_YELLOW : CLR_DARK_GREY);
    print(str("%s  bar %d  %d  #%d", SEC_NAME[sec], (int)(bnow / 4) + 1, tempo, seed), 84, 4, CLR_WHITE);

    // piano roll: 7 s window, now at x = 70; thick = key down, thin = held by the pedal, dots = release
    int x0 = 4, x1 = 316, xn = 70, y0 = 20, y1 = 150;
    float pps = 44.0f;                                   // pixels per second
    rectfill(x0, y0, x1 - x0, y1 - y0, CLR_BLACK);
    for (int i = 0; i < nev; i++) {
        Ev *e = &ev[i];
        if (e->hand == 3) continue;
        float xa = xn + (float)(e->t - at) * pps, xu = xn + (float)(e->t_up - at) * pps, xd = xn + (float)(e->t_damp - at) * pps;
        float xr = xd + REL_MS[(e->slot >= SL_P0 && e->slot < SL_P0 + NREL) ? e->slot - SL_P0 : 2] / 1000.0f * pps;
        if (xr < x0 || xa > x1) continue;
        int y = y1 - 4 - (e->midi - 28) * 2;
        if (y < y0 + 2 || y > y1 - 2) continue;
        int col = e->hand == 1 ? (e->gain > 0.55f ? CLR_YELLOW : CLR_ORANGE) : e->hand == 0 ? (e->gain > 0.4f ? CLR_BLUE : CLR_DARK_PURPLE) : CLR_DARK_GREEN;
        int a = (int)fmaxf(x0, xa), u = (int)fminf(x1, xu), d = (int)fminf(x1, xd), r = (int)fminf(x1, xr);
        if (u > a) rectfill(a, y - 1, u - a, 2, col);
        if (d > u) line((int)fmaxf(x0, xu), y, d, y, CLR_LIGHT_GREY);
        for (int x = (int)fmaxf(x0, xd); x < r; x += 3) pset(x, y, CLR_MEDIUM_GREY);
        if (xa >= x0 && xa <= x1) pset((int)xa, y - 2, CLR_WHITE);
    }
    line(xn, y0, xn, y1 - 1, CLR_RED);
    // the pedal lane
    int py0 = 154, ph_ = 14;
    rectfill(x0, py0, x1 - x0, ph_, CLR_DARKER_GREY);
    for (int x = x0; x < x1; x++) {
        double t = at + (x - xn) / pps;
        float d = pedal_at(beat_of(t));
        int hgt = (int)(d * ph_);
        if (hgt > 0) line(x, py0 + ph_ - hgt, x, py0 + ph_ - 1, d > 0.7f ? CLR_GREEN : d > 0.15f ? CLR_DARK_GREEN : CLR_DARK_GREY);
    }
    line(xn, py0, xn, py0 + ph_ - 1, CLR_RED);
    print("pedal", x1 - 42, py0 + 3, CLR_WHITE);
    print(str("max voices %d", max_voices), 150, py0 + 3, CLR_LIGHT_GREY);

    int ly = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, ly, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, ly + 12, CLR_LIGHT_GREY);
}
