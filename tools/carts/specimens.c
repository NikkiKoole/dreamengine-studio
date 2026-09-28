/* de:meta
{
  "slug": "specimens",
  "title": "specimens",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "cellular-automata",
    "sonification"
  ],
  "lineage": "Row 6 of docs/design/choochootracker-borrow-list.md: twelve synthesis techniques the repo had no version of, ported to cart-land C from Lyle Mills' Plaits-Alt fork (plaits_alt/dsp/engine2, MIT, 2026, vendored by Choochootracker): phase_flock (Kuramoto-coupled oscillators), rulefield (a 1-D cellular automaton read as a wavecycle), scanned (scanned synthesis: a mass-spring ring read as a wavetable), gendy (Xenakis' dynamic stochastic synthesis), attractor (a Thomas cyclically symmetric chaotic flow), bytebeat (Bees-in-the-Trees' four formulas), pulsar (Roads' pulsar synthesis), wave_terrain (an orbit over a 2-D surface), spectral_spiral (frequency-shift feedback), lockstep (a phase-locked loop), vosim (Kaegi and Tempelaars' VOSIM, via Braids) and glisson (chirping grains). Each note renders in cart-land into a PCM slot (the mme / sintered route) and the render also records the algorithm's STATE sixty times a second, so the picture is the process making the sound you hear, not an animation beside it.",
  "homage": "Lyle Mills' Plaits-Alt (2026) on Emilie Gillet's Plaits; Iannis Xenakis (GENDYN), Yoshiki Kuramoto, Stephen Wolfram's elementary automata, Bill Verplank / Max Mathews / Rob Shaw (scanned synthesis), Rene Thomas, and the bytebeat scene.",
  "description": {
    "summary": "A cabinet of twelve synthesis techniques the rest of the console does not have (oscillators falling into sync, a cellular automaton, a vibrating string network, Xenakis' random breakpoints, a chaotic attractor, bytebeat, pulsars, a wave terrain, a frequency-shift spiral, a phase-locked loop, VOSIM, chirping grains) each drawn live by the same process that makes its sound.",
    "detail": "Pick a specimen, play the keyboard. FLOCK: seven detuned oscillators pull on each other; turn COUPLING up and watch the dots clump as they fall into sync, the arrow in the middle is how synchronised they are. RULES: an elementary cellular automaton evolves once per cycle and its row IS the waveform; the picture is its spacetime diagram. SCANNED: a ring of 32 masses on springs, struck at note-on, read as a wavetable; you see the ring ringing. GENDY: a wave made of random breakpoints that take a random walk every cycle. ATTRACTOR: a three-way chaotic flow, drawn as its orbit. BYTEBEAT: a one-line integer formula as an oscillator, drawn as the byte stream. PULSAR: a burst of formant in each cycle, then silence. TERRAIN: a circular orbit scanning a 2-D surface (drawn as a heat map); the height under the dot is the sample. SPIRAL: a short complex delay loop that shifts the spectrum on every lap, drawn in the I/Q plane. LOCKSTEP: a phase-locked loop, a follower oscillator chasing the note at a ratio, with its phase error. VOSIM: bell-windowed formant pulses. GLISSON: grains that chirp up or down, drawn as pitch over time. Each has four knobs named for what they do in THAT specimen. A note renders its sound (up to 6 s) when you press it; the knobs apply to the next note and fire a short preview while you drag.",
    "controls": "keybed: A-K whites, W-P blacks, Z/X octave, click/touch/MIDI · TAB or the two rows of tabs: pick a specimen · drag the four knobs (wheel = fine) · M: autoplay"
  },
  "todo": [
    "ear pass: which specimens earn an INSTR_* engine (then the knobs ride a held note live, and the 6 s render cap goes away)",
    "still unported, and absent from the repo: undertow (subharmonics), tapfield, phase_weave, loopback (feedback AM), sideband (DSF), question_mark (a Morse transmitter), Braids' digital filters, twin-peaks / clocked / particle noise"
  ]
}
de:meta */
// specimens — twelve synthesis techniques from Plaits-Alt, each drawn by its own state.
//
// THE ROUTE (same as mme / sintered): a key press renders the note in cart-land C into a
// shared buffer, sample_load()s it into one of NV PCM slots (round-robin) and plays it through
// INSTR_SAMPLE at root = that note, so nothing is resampled. The ADSR release gates key-up.
// While rendering, every SNAP_EVERY samples the specimen writes a Snap of its state; draw()
// shows the snapshot at (frames since note-on), so what you see is exactly the state that is
// producing what you hear at that moment.
//
// Ports, from plaits_alt/dsp/engine2/*_engine.cc (Lyle Mills, MIT), in de_* math. Plaits'
// normalised frequency is f/48 kHz; here it is f/44.1 kHz, so the note is the note. Changes:
//   · Random::GetFloat() → a per-note LCG seeded from the pitch, so a render is repeatable
//   · every render ends in a 10 Hz DC blocker, a 3 ms fade-in and a 40 ms fade-out, and is
//     peak-normalised (the specimens differ by >20 dB and a keyboard wants one level)
//   · scanned is always triggered (its "unpatched = driven by noise" mode has no gate here)
//   · terrain runs 1x, not 2x oversampled (22.5 ms per 6 s render at 2x, over one frame), and
//     only its five analytic terrains (the other three read Plaits' wavetable ROM)
//   · Plaits-Alt's "vowel_fof" is NOT here: it is five resonant filters on a saw, the same idea
//     as INSTR_VOICE, so GLISSON took its tab
//
// controls: keybed (A-K, W-P, Z/X) · TAB / tabs pick the specimen · four knobs · M autoplay

#include "studio.h"
#include "ui.h"
#include "keybed.h"
#include <math.h>
#include <string.h>

#define SR          44100
#define NV          6                     // PCM slots / instrument slots 5..10
#define I0          5
#define REN_SEC     6
#define REN_N       (SR * REN_SEC)
#define PREVIEW_N   (SR * 6 / 5)          // 1.2 s: what a knob drag re-renders
#define SNAP_EVERY  (SR / 60)             // one state snapshot per video frame
#define NSNAP       (REN_N / SNAP_EVERY + 2)

enum { SP_FLOCK, SP_RULES, SP_SCAN, SP_GENDY, SP_ATTR, SP_BYTE,
       SP_PULSAR, SP_TERRAIN, SP_SPIRAL, SP_LOCK, SP_VOSIM, SP_GLISSON, NSPEC };
static const char *SPNAME[NSPEC] = { "FLOCK", "RULES", "SCANNED", "GENDY", "ATTRACT", "BYTEBEAT",
                                     "PULSAR", "TERRAIN", "SPIRAL", "LOCKSTEP", "VOSIM", "GLISSON" };
static const char *KLABEL[NSPEC][4] = {
    { "spread",  "couple", "2-flock", "lag"    },
    { "rule",    "edges",  "evolve",  "smooth" },
    { "strike",  "mass",   "damp+fold","physics"},
    { "points",  "amp walk","time walk","smooth"},
    { "lobes",   "chaos",  "x-y-z",   "skew"   },
    { "p0",      "p1",     "formula", "bits"   },
    { "formant", "duty",   "cluster", "skew"   },
    { "terrain", "radius", "x shift", "y shift"},
    { "partials","spacing","feedback","direction"},
    { "ratio",   "bandwidth","saw det","damping"},
    { "formant2","formant1","pulse",  "share"  },
    { "scatter", "grains", "up-down", "length" },
};
static const char *CAPTION[NSPEC] = {
    "seven detuned oscillators pulling each other into sync",
    "an elementary cellular automaton: its row is the wave",
    "a ring of 32 masses on springs, struck, read as a table",
    "Xenakis: random breakpoints taking a random walk each cycle",
    "a Thomas chaotic flow: three states chasing each other",
    "one-line integer formulas as an oscillator",
    "pulsar synthesis: a burst of formant in each cycle, then silence",
    "wave terrain: an orbit scanning a 2-D surface, the height is the sound",
    "frequency-shift feedback: each lap round the loop shifts the spectrum",
    "a phase-locked loop: a follower chasing the note at a ratio",
    "VOSIM: bell-windowed formant pulses, one per cycle",
    "glissons: grains that chirp up or down as they play",
};
static float K[NSPEC][4] = {
    { 0.45f, 0.30f, 0.00f, 0.50f },
    { 0.12f, 0.30f, 0.30f, 0.60f },
    { 0.55f, 0.40f, 0.30f, 0.45f },
    { 0.40f, 0.35f, 0.25f, 0.80f },
    { 0.50f, 0.55f, 0.20f, 0.50f },
    { 0.40f, 0.35f, 0.10f, 0.50f },
    { 0.45f, 0.35f, 0.20f, 0.40f },
    { 0.10f, 0.60f, 0.45f, 0.55f },
    { 0.55f, 0.35f, 0.75f, 0.70f },
    { 0.40f, 0.35f, 0.30f, 0.40f },
    { 0.55f, 0.45f, 0.45f, 0.55f },
    { 0.35f, 0.55f, 0.80f, 0.60f },
};

// ── the snapshot a render leaves behind, once per frame ─────────────────────────
typedef struct {
    float v[64];                          // per-specimen payload (see each render)
    uint32_t row;                         // rules: the CA row · gendy: breakpoint count
    float r;                              // flock: sync order parameter |mean vector|
    float mx, my;                         // flock: the mean vector
} Snap;
static Snap snaps[NSNAP];
static int  nsnap = 0;
static int  snap_spec = SP_FLOCK;         // which specimen the snapshots belong to

static float ren[REN_N];
static int   ren_n = 0;
static int   cur = SP_FLOCK;
static int   rr = 0;
static int   vhandle[NV];
static int   handle[128];
static int   note_frame = 0;              // frame of the last note-on (the picture's clock)
static bool  autoplay = true;
static int   apos = 0, auto_v = -1, auto_left = 0;
static bool  dirty = false;
static int   preview_v = -1, preview_left = 0;

// ── shared helpers ──────────────────────────────────────────────────────────────
static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float softclip(float x) { if (x < -3.0f) return -1.0f; if (x > 3.0f) return 1.0f; return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
static float semis(float s) { return de_powf(2.0f, s / 12.0f); }
static float midi_hz(int m) { return 440.0f * de_powf(2.0f, (float)(m - 69) / 12.0f); }
static float midi_hz_f(float m) { return 440.0f * de_powf(2.0f, (m - 69.0f) / 12.0f); }
typedef struct { uint32_t s; } Rng;
static float rng01(Rng *g) { g->s = g->s * 1664525u + 1013904223u; return (float)(g->s >> 8) * (1.0f / 16777216.0f); }

// ── FLOCK (phase_flock_engine.cc) ───────────────────────────────────────────────
#define NF 7
static const float FL_DETUNE[NF] = { -1.0f, -0.63f, -0.31f, -0.07f, 0.19f, 0.51f, 0.86f };
static const float FL_PHASE0[NF] = { 0.031f, 0.647f, 0.287f, 0.903f, 0.491f, 0.154f, 0.778f };

static int render_flock(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    float sn_[NF], cs_[NF], rs[NF], rc[NF], nat[NF];
    const float turn = 0.083f;                                    // Scatter(), first trigger
    for (int i = 0; i < NF; i++) { float p = FL_PHASE0[i] + turn * (float)(i + 1); p -= floorf(p);
        sn_[i] = de_sin_turns(p); cs_[i] = de_cos_turns(p); rs[i] = 0.0f; rc[i] = 1.0f; }
    const float base = fminf(0.20f, f);
    const float spread = 14.0f * k[0] * k[0];
    for (int i = 0; i < NF; i++) nat[i] = fminf(0.23f, base * semis(spread * FL_DETUNE[i]));
    const float coupling = fminf(0.045f, base * 1.8f * k[1] * k[1]);
    const float cluster = k[2];
    const float lag = (k[3] - 0.5f) * 0.90f, lagn = 1.0f / (1.0f + fabsf(lag));
    const float norm = 1.0f / (float)NF;
    int ns = 0;
    for (int t = 0; t < n; t++) {
        float s2[NF], c2[NF], ms = 0, mc = 0, ms2 = 0, mc2 = 0;
        for (int i = 0; i < NF; i++) {
            s2[i] = 2.0f * sn_[i] * cs_[i]; c2[i] = cs_[i] * cs_[i] - sn_[i] * sn_[i];
            ms += sn_[i]; mc += cs_[i]; ms2 += s2[i]; mc2 += c2[i];
        }
        ms *= norm; mc *= norm; ms2 *= norm; mc2 *= norm;
        float o = ms * 1.55f; o /= 1.0f + 0.30f * fabsf(o);           // LimitAudio
        out[t] = clampf(o, -1.0f, 1.0f);
        if (t % SNAP_EVERY == 0 && ns < NSNAP) {
            Snap *q = &sn[ns++];
            for (int i = 0; i < NF; i++) q->v[i] = de_atan2f(sn_[i], cs_[i]) * 0.15915494f;
            q->mx = mc; q->my = ms; q->r = sqrtf(ms * ms + mc * mc);
        }
        if ((t & 7) == 0) {                                           // coupling at control rate (/8)
            for (int i = 0; i < NF; i++) {
                float a1 = ms * cs_[i] - mc * sn_[i], q1 = mc * cs_[i] + ms * sn_[i];
                float a2 = ms2 * c2[i] - mc2 * s2[i], q2 = mc2 * c2[i] + ms2 * s2[i];
                float att = a1 + (0.5f * a2 - a1) * cluster, quad = q1 + (0.5f * q2 - q1) * cluster;
                float inc = clampf(nat[i] + coupling * (att - lag * quad) * lagn, 0.0f, 0.24f);
                rs[i] = de_sin_turns(inc); rc[i] = de_cos_turns(inc);
                float rn = 1.5f - 0.5f * (rs[i] * rs[i] + rc[i] * rc[i]); rs[i] *= rn; rc[i] *= rn;
            }
        }
        for (int i = 0; i < NF; i++) {
            float s = sn_[i], c = cs_[i];
            sn_[i] = s * rc[i] + c * rs[i];
            cs_[i] = c * rc[i] - s * rs[i];
            if ((t & 7) == 0) { float sn2 = 1.5f - 0.5f * (sn_[i] * sn_[i] + cs_[i] * cs_[i]); sn_[i] *= sn2; cs_[i] *= sn2; }
        }
    }
    *nsn = ns;
    return n;
}

// ── RULES (rulefield_engine.cc) ─────────────────────────────────────────────────
static uint32_t rotl(uint32_t v) { return (v << 1) | (v >> 31); }
static uint32_t rotr(uint32_t v) { return (v >> 1) | (v << 31); }
static float density(uint32_t v) {
    v -= (v >> 1) & 0x55555555u; v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    v = (v + (v >> 4)) & 0x0f0f0f0fu; v += v >> 8; v += v >> 16;
    return (float)(v & 0x3fu) * (1.0f / 32.0f);
}
static uint32_t ca_seed(int rule) {
    uint32_t s = 0xa3619d27u ^ ((uint32_t)rule * 0x45d9f3bu);
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    if (!s || s == 0xffffffffu) s = 0x80000001u;
    return s;
}
static uint32_t ca_step(uint32_t row, int rule, uint32_t gen) {
    uint32_t l = rotl(row), c = row, r = rotr(row), next = 0u;
    for (int nb = 0; nb < 8; nb++) {
        if (!(rule & (1 << nb))) continue;
        next |= (nb & 4 ? l : ~l) & (nb & 2 ? c : ~c) & (nb & 1 ? r : ~r);
    }
    int inj = (int)((gen * 13u + (uint32_t)rule) & 31u);          // keep absorbing rows audible
    if (!next) next = 1u << inj; else if (next == 0xffffffffu) next &= ~(1u << inj);
    return next;
}
static float ca_read(uint32_t row, float dens, float ph, float shape) {
    float addr = ph * 32.0f; int i = (int)addr & 31, j = (i + 1) & 31; float fr = addr - (float)(int)addr;
    float a = (row & (1u << i)) ? 1.0f - dens : -dens, b = (row & (1u << j)) ? 1.0f - dens : -dens;
    float lin = a + (b - a) * fr, sm = a + (b - a) * fr * fr * (3.0f - 2.0f * fr);
    return shape < 0.5f ? a + (lin - a) * (2.0f * shape) : lin + (sm - lin) * (2.0f * shape - 1.0f);
}
static int render_rules(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    int rule = 1 + (int)(253.999f * k[0]);
    uint32_t row = ca_seed(rule), gen = 0;
    float ph = 0.0f, evo = 0.0f, rate = 0.125f + 7.875f * k[2] * k[2];
    uint32_t edge = row ^ rotr(row);
    float dr = density(row), de = density(edge);
    int ns = 0;
    for (int t = 0; t < n; t++) {
        ph += fminf(0.24f, f);
        if (ph >= 1.0f) {
            ph -= 1.0f; evo += rate;
            for (int it = 0; evo >= 1.0f && it < 8; it++) { evo -= 1.0f; row = ca_step(row, rule, ++gen); }
            edge = row ^ rotr(row); dr = density(row); de = density(edge);
        }
        float cells = ca_read(row, dr, ph, k[3]), edges = ca_read(edge, de, ph, k[3]);
        out[t] = 0.9f * (cells + (edges - cells) * k[1]);
        if (t % SNAP_EVERY == 0 && ns < NSNAP) { sn[ns].row = row; sn[ns].r = (float)gen; ns++; }
    }
    *nsn = ns;
    return n;
}

// ── SCANNED (scanned_engine.cc) ─────────────────────────────────────────────────
#define NM 32
static int render_scan(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    float pos[NM], vel[NM], acc[NM];
    memset(pos, 0, sizeof pos); memset(vel, 0, sizeof vel);
    const float structure = k[1] * k[1];
    const float width = 1.25f + 5.0f * (1.0f - k[0]);
    {   // Excite(timbre, width, 0.3 + 0.5 * accent), accent 0.5
        float centre = k[1] * (float)NM, mean = 0.0f, amount = 0.55f;
        for (int i = 0; i < NM; i++) {
            float d = fabsf((float)i - centre); d = fminf(d, (float)NM - d);
            float p = d < width ? 0.5f + 0.5f * de_sin_turns(0.25f + 0.5f * d / width) : 0.0f;
            vel[i] += p * amount; mean += p * amount;
        }
        mean /= (float)NM; for (int i = 0; i < NM; i++) vel[i] -= mean;
    }
    const float prate = 20.0f * semis(k[3] * 72.0f) / (float)SR;
    const float damping = 0.99985f - 0.065f * k[2] * k[2];
    const float nonlin = k[2] * k[2], fold = k[2] * k[2];
    float pph = 0.0f, sph = 0.0f;
    int ns = 0;
    for (int t = 0; t < n; t++) {
        pph += prate;
        while (pph >= 1.0f) {
            pph -= 1.0f;
            float am = 0.0f;
            for (int i = 0; i < NM; i++) {
                int l1 = (i + NM - 1) % NM, r1 = (i + 1) % NM, l2 = (i + NM - 2) % NM, r2 = (i + 2) % NM;
                float lap = pos[l1] + pos[r1] - 2.0f * pos[i];
                float bih = pos[l2] - 4.0f * pos[l1] + 6.0f * pos[i] - 4.0f * pos[r1] + pos[r2];
                float prof = (float)((i * 13) & 31) / 31.0f;
                float mass = 1.0f + k[0] * 0.45f * (float)(i & 1) + structure * (0.25f + 1.35f * prof);
                acc[i] = (0.22f * lap - 0.018f * k[0] * bih) / mass - nonlin * 0.12f * pos[i] * fabsf(pos[i]);
                am += acc[i];
            }
            am /= (float)NM;
            float pm = 0.0f, vm = 0.0f, pk = 0.0f;
            for (int i = 0; i < NM; i++) { vel[i] = (vel[i] + acc[i] - am) * damping; pos[i] += vel[i]; pm += pos[i]; vm += vel[i]; }
            pm /= (float)NM; vm /= (float)NM;
            for (int i = 0; i < NM; i++) { pos[i] -= pm; vel[i] -= vm; pk = fmaxf(pk, fabsf(pos[i])); }
            if (pk > 1.0f) { float g = 1.0f / pk; for (int i = 0; i < NM; i++) { pos[i] *= g; vel[i] *= g; } }
        }
        sph += fminf(0.24f, f); sph -= floorf(sph);
        float wp = sph + 0.22f * structure * de_sin_turns(sph); wp -= floorf(wp);
        float idx = wp * (float)NM; int i0 = (int)idx % NM, i1 = (i0 + 1) % NM; float fr = idx - floorf(idx);
        float smp = pos[i0] + (pos[i1] - pos[i0]) * fr;
        int im = (i0 + NM - 1) % NM, ip = (i0 + 1) % NM, ip2 = (i0 + 2) % NM;
        float left = pos[im] + (pos[i0] - pos[im]) * fr, right = pos[ip] + (pos[ip2] - pos[ip]) * fr;
        float grad = (right - left) * 2.25f;
        float scanned = smp + (grad - smp) * k[1];
        float folded = de_sin_turns(16.0f + scanned * (0.25f + 1.25f * fold));
        out[t] = 0.8f * (scanned + (folded - scanned) * fold);
        if (t % SNAP_EVERY == 0 && ns < NSNAP) { memcpy(sn[ns].v, pos, sizeof pos); sn[ns].r = sph; ns++; }
    }
    *nsn = ns;
    return n;
}

// ── GENDY (gendy_engine.cc) ─────────────────────────────────────────────────────
#define NGB 12
static float g_walk(Rng *g, float v, float amt, float lo, float hi) {
    v += (2.0f * rng01(g) - 1.0f) * amt;
    if (v > hi) v = hi - (v - hi); else if (v < lo) v = lo + (lo - v);
    return clampf(v, lo, hi);
}
static int render_gendy(float *out, int n, float f, const float *k, Snap *sn, int *nsn, uint32_t seed) {
    Rng g = { seed };
    int nb = 3 + (int)(k[0] * 6.999f);
    float amp[NGB], dur[NGB], bnd[NGB];
    for (int i = 0; i < nb; i++) { amp[i] = 2.0f * rng01(&g) - 1.0f; dur[i] = 0.5f + rng01(&g); }
    #define G_BOUNDS() do { float tot = 0; for (int i = 0; i < nb; i++) tot += dur[i]; float cu = 0; \
        for (int i = 0; i < nb; i++) { cu += dur[i] / tot; bnd[i] = cu; } bnd[nb - 1] = 1.0f; } while (0)
    G_BOUNDS();
    const float comp = 1.0f - 0.035f * (float)(nb - 3);
    const float astep = (0.005f + 0.3f * k[1] * k[1]) * comp, dstep = (0.005f + 0.75f * k[2] * k[2]) * comp;
    float ph = 0.0f; int seg = 0, ns = 0;
    for (int t = 0; t < n; t++) {
        ph += fminf(0.24f, f);
        if (ph >= 1.0f) {
            ph -= 1.0f;
            float mean = 0.0f;
            for (int i = 0; i < nb; i++) { amp[i] = g_walk(&g, amp[i], astep, -1.0f, 1.0f); dur[i] = g_walk(&g, dur[i], dstep, 0.2f, 2.5f); mean += amp[i]; }
            mean /= (float)nb;
            for (int i = 0; i < nb; i++) amp[i] = clampf(amp[i] - mean, -1.0f, 1.0f);
            G_BOUNDS();
            seg = 0;
        }
        while (seg < nb - 1 && ph >= bnd[seg]) seg++;
        int nx = seg + 1 == nb ? 0 : seg + 1;
        float st = seg == 0 ? 0.0f : bnd[seg - 1], w = bnd[seg] - st;
        float u = clampf((ph - st) / w, 0.0f, 1.0f);
        float stepped = amp[seg], lin = stepped + (amp[nx] - stepped) * u, sm = stepped + (amp[nx] - stepped) * u * u * (3.0f - 2.0f * u);
        float s = k[3] < 0.5f ? stepped + (lin - stepped) * k[3] * 2.0f : lin + (sm - lin) * (k[3] * 2.0f - 1.0f);
        out[t] = s * 0.8f;
        if (t % SNAP_EVERY == 0 && ns < NSNAP) {
            for (int i = 0; i < nb; i++) { sn[ns].v[i] = amp[i]; sn[ns].v[16 + i] = bnd[i]; }
            sn[ns].row = (uint32_t)nb; sn[ns].r = ph; ns++;
        }
    }
    #undef G_BOUNDS
    *nsn = ns;
    return n;
}

// ── ATTRACTOR (attractor_engine.cc) ─────────────────────────────────────────────
static int render_attr(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    float x = 0.17f + 0.21f * 0.5f, y = -0.11f, z = 0.07f - 0.13f * 0.5f;   // Seed(accent 0.5)
    float dc[3] = { 0, 0, 0 };
    const float skew = (k[3] - 0.5f) * 0.78f, damp = 0.355f - 0.185f * k[1];
    const float arg = 0.68f + 0.92f * k[0] * k[0];
    const float base = fminf(0.62f, f * 6.283185307f);
    const float rx = base * (1.0f + skew), ry = base, rz = base * (1.0f - skew);
    int stride = (int)(1.0f / fmaxf(f, 1e-4f) / 32.0f); if (stride < 1) stride = 1;
    if (stride * 32 > SNAP_EVERY) stride = SNAP_EVERY / 32;
    int ns = 0, pts = 0;
    for (int t = 0; t < n; t++) {
        float nx = x + rx * (de_sin_turns(8.0f + y * arg * 0.159154943f) - damp * x);
        float ny = y + ry * (de_sin_turns(8.0f + z * arg * 0.159154943f) - damp * y);
        float nz = z + rz * (de_sin_turns(8.0f + x * arg * 0.159154943f) - damp * z);
        x = clampf(nx, -7.0f, 7.0f); y = clampf(ny, -7.0f, 7.0f); z = clampf(nz, -7.0f, 7.0f);
        float st[3] = { x, y, z }, co[3];
        for (int j = 0; j < 3; j++) { float sh = st[j] / (1.0f + fabsf(st[j])); dc[j] += 0.00022f * (sh - dc[j]); co[j] = sh - dc[j]; }
        float sel = k[2] * 2.0f, m;
        if (sel < 1.0f) m = co[0] + (co[1] - co[0]) * sel; else m = co[1] + (co[2] - co[1]) * (sel - 1.0f);
        out[t] = clampf(0.95f * softclip(m * 3.0f), -1.0f, 1.0f);
        if (t % SNAP_EVERY == 0) pts = 0;
        if (pts < 32 && (t % SNAP_EVERY) % stride == 0 && ns < NSNAP) {
            sn[ns].v[pts] = co[0]; sn[ns].v[32 + pts] = co[2];   // the x-z projection (x and z chase through y)
            pts++;
            if (pts == 32) ns++;
        }
    }
    *nsn = ns;
    return n;
}

// ── BYTEBEAT (bytebeat_engine.cc) ───────────────────────────────────────────────
static uint32_t bb_formula(int fo, uint32_t t, uint32_t p0, uint32_t p1) {
    switch (fo) {
        case 0:  return ((t * 3) & (t >> 10)) | ((t * p0) & (t >> 10)) | ((t * 10) & ((t >> 8) * p1) & 128);
        case 1:  return ((t * p0) & (t >> 4)) | ((t * 5) & (t >> 7)) | ((t * p1) & (t >> 10));
        case 2:  return ((t >> p0) & t) * (t >> p1);
        default: return ((((((t >> p0) | t) | (t >> p0)) * 10) & ((5 * t) | (t >> 10))) | (t ^ (t % p1)));
    }
}
static const char *BB_TEXT[4][2] = {
    { "(t*3 & t>>10) | (t*p0 & t>>10)", "  | (t*10 & (t>>8)*p1 & 128)" },
    { "(t*p0 & t>>4) | (t*5 & t>>7)",   "  | (t*p1 & t>>10)" },
    { "((t>>p0) & t) * (t>>p1)",        "" },
    { "(((t>>p0)|t|(t>>p0))*10 & (5t|t>>10))", "  | (t ^ t%p1)" },
};
static int bb_parts(const float *k, uint32_t *p0, uint32_t *p1, int *width) {
    int fo = (int)(k[2] * 4.0f); if (fo > 3) fo = 3;
    if (fo == 0)      { *p0 = (uint32_t)(k[0] * 63.0f); *p1 = (uint32_t)(k[1] * 15.0f); }
    else if (fo == 3) { *p0 = (uint32_t)(k[0] * 15.0f); *p1 = 1u + (uint32_t)(k[1] * 126.0f); }
    else              { *p0 = (uint32_t)(k[0] * 15.0f); *p1 = (uint32_t)(k[1] * 15.0f); }
    float m = k[3], w = m < 0.5f ? 2.0f + (8.0f - 2.0f) * m * 2.0f : 8.0f + (12.0f - 8.0f) * (m * 2.0f - 1.0f);
    *width = (int)clampf(w, 2.0f, 12.0f);
    return fo;
}
static float bb_sample(uint32_t v, int width) {
    uint32_t span = 1u << width, half = span >> 1, m = v & (span - 1u);
    int32_t c = m >= half ? (int32_t)m - (int32_t)span : (int32_t)m;
    return (float)c / (float)half;
}
static int render_byte(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    uint32_t p0, p1; int width; int fo = bb_parts(k, &p0, &p1, &width);
    float tph = 0.0f; uint32_t t = 1024;                            // kBytebeatRestartTick
    const float inc = f * 256.0f;
    int ns = 0;
    for (int i = 0; i < n; i++) {
        tph += inc;
        while (tph >= 1.0f) { tph -= 1.0f; t++; }
        out[i] = bb_sample(bb_formula(fo, t, p0, p1), width);
        int ph = i % SNAP_EVERY, stride = SNAP_EVERY / 64;            // 64 points across the whole frame
        if (ph % stride == 0 && ph / stride < 64 && ns < NSNAP) {
            sn[ns].v[ph / stride] = out[i];
            if (ph / stride == 63) { sn[ns].row = t; ns++; }
        }
    }
    *nsn = ns;
    return n;
}


// ── PULSAR (pulsar_engine.cc) ───────────────────────────────────────────────────
static float pl_window(float ph, float duty, float peak) {
    if (ph >= duty) return 0.0f;
    float pos = ph / duty, e = pos < peak ? pos / peak : (1.0f - pos) / (1.0f - peak);
    return e * (2.0f - e);
}
static float pl_cluster(float ph, float count, float duty, float peak) {
    int lo = (int)count; float bl = count - (float)lo;
    float a = ph * (float)lo; a -= floorf(a);
    float b = ph * (float)(lo + 1); b -= floorf(b);
    float wa = pl_window(a, duty, peak), wb = pl_window(b, duty, peak);
    return wa + (wb - wa) * bl;
}
// shared: 64 strided samples across one fundamental period, into v[]
static int period_stride(float f) { int st = (int)(1.0f / fmaxf(f, 1e-4f) / 64.0f); if (st < 1) st = 1; if (st * 64 > SNAP_EVERY) st = SNAP_EVERY / 64; return st; }
// cycle-aligned capture: armed once per video frame, it starts at the next cycle start (wrapped
// = the oscillator's phase just wrapped) so the picture holds still instead of sliding
typedef struct { int armed, pts, cnt; } Cap;
static void cap_step(Cap *c, Snap *sn, int *ns, int t, int wrapped, int stride, float val) {
    if (t % SNAP_EVERY == 0 && c->pts < 0) c->armed = 1;
    if (c->armed && wrapped && *ns < NSNAP) { c->armed = 0; c->pts = 0; c->cnt = 0; }
    if (c->pts >= 0) {
        if (c->cnt++ % stride == 0) { sn[*ns].v[c->pts++] = val; if (c->pts == 64) { (*ns)++; c->pts = -1; } }
    }
}

static int render_pulsar(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    f = fminf(0.24f, f);
    float maxf = fmaxf(1.0f, 0.225f / f);
    float formant = fminf(maxf, 1.0f + 31.0f * k[0] * k[0]);
    float duty = 0.055f + 0.89f * k[1] * k[1], cluster = 1.0f + 3.0f * k[2], skew = 0.12f + 0.76f * k[3];
    float ph = 0.0f; int ns = 0;
    Cap cap = { 0, -1, 0 };
    // zoom into the sounding part of the cycle (the pulsaret), the rest is silence by design
    int st = (int)(duty / fmaxf(f, 1e-4f) / 64.0f); if (st < 1) st = 1;
    for (int t = 0; t < n; t++) {
        ph += f; int wrapped = 0; if (ph >= 1.0f) { ph -= 1.0f; wrapped = 1; }
        float w = pl_cluster(ph, cluster, duty, skew);
        float cp = ph * formant; cp -= floorf(cp);
        out[t] = 0.78f * w * de_sin_turns(cp);
        if (ns < NSNAP) { sn[ns].r = duty; sn[ns].mx = formant; sn[ns].my = cluster; }
        cap_step(&cap, sn, &ns, t, wrapped, st, out[t]);
    }
    *nsn = ns;
    return n;
}

// ── TERRAIN (wave_terrain_engine.cc, the five analytic terrains) ────────────────
static float squash(float x, float a) { x *= a; return x / (1.0f + fabsf(x)); }
static float terrain_z(float x, float y, int ti) {
    const float kk = 4.0f;
    switch (ti) {
        case 0:  return (squash(de_sin_turns(kk + x * 1.273f), 2.0f) - de_sin_turns(kk + y * (x + 1.571f) * 0.637f)) * 0.57f;
        case 1:  { float xy = x * y; return de_sin_turns(kk + de_sin_turns(kk + (x + y) * 0.637f) / (0.2f + xy * xy) * 0.159f); }
        case 2:  { float xy = x * y; return de_sin_turns(kk + de_sin_turns(kk + 2.387f * xy) / (0.350f + xy * xy) * 0.159f); }
        case 3:  { float xy = x * y, xys = (x - 0.25f) * (y + 0.25f); return de_sin_turns(kk + xy / (2.0f + fabsf(5.0f * xys)) * 6.366f); }
        default: return de_sin_turns(0.159f / (0.170f + fabsf(y - 0.25f)) + 0.477f / (0.350f + fabsf((x + 0.5f) * (y + 1.5f))) + kk);
    }
}
static float terrain_at(float x, float y, float zsel) {           // interpolate between adjacent terrains
    int zi = (int)zsel; if (zi > 3) zi = 3; float zf = zsel - (float)zi;
    float a = terrain_z(x, y, zi), b = terrain_z(x, y, zi + 1);
    return a + (b - a) * zf;
}
static float terrain_sel(const float *k) { return fminf(k[0] * 1.05f, 1.0f) * 3.999f; }
static float terrain_radius(float f, const float *k) { float att = fmaxf(1.0f - 8.0f * f, 0.0f); return 0.1f + 0.9f * k[1] * att * (2.0f - att); }
static int render_terrain(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    const float rad = terrain_radius(f, k), xo = 1.9f * k[2] - 1.0f, yo = 1.9f * k[3] - 0.95f, zs = terrain_sel(k);
    float ph = 0.0f; int ns = 0;
    for (int t = 0; t < n; t++) {
        // upstream oversamples 2x; this runs 1x, which halves the cost (a 6 s render measured 22.5 ms
        // at 2x, over one frame) and costs some aliasing on high notes with a wide orbit
        ph += f; ph -= floorf(ph);
        float px = de_cos_turns(ph) * rad, py = de_sin_turns(ph) * rad;
        float x = px * (1.0f - fabsf(xo)) + xo, y = py * (1.0f - fabsf(yo)) + yo;
        out[t] = terrain_at(x, y, zs);
        if (t % SNAP_EVERY == 0 && ns < NSNAP) { sn[ns].mx = x; sn[ns].my = y; sn[ns].r = rad; ns++; }
    }
    *nsn = ns;
    return n;
}

// ── SPIRAL (spectral_spiral_engine.cc): frequency-shift feedback ────────────────
#define SPD 32
static float sp_att(float fr) { return clampf((0.24f - fr) * 12.5f, 0.0f, 1.0f); }
static int render_spiral(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    float di[SPD], dq[SPD]; memset(di, 0, sizeof di); memset(dq, 0, sizeof dq);
    int wi = 0;
    const float sf = fminf(0.20f, f), rich = k[0];
    float w[4] = { sp_att(sf), 0.58f * rich * sp_att(sf * 2.0f), 0.37f * rich * rich * sp_att(sf * 3.0f), 0.24f * rich * rich * rich * sp_att(sf * 5.0f) };
    float wsum = w[0] + w[1] + w[2] + w[3], wn = wsum > 0.0f ? 1.0f / wsum : 0.0f;
    const float mag = 0.000015f + sf * (0.035f + 0.43f * k[1] * k[1]);
    const float shf = mag * (k[3] * 2.0f - 1.0f);
    const float fbk = 0.94f * k[2] * k[2], inj = 0.76f * (1.0f - fbk);
    static const int H[4] = { 1, 2, 3, 5 };
    float sph = 0.0f, hph = 0.0f; int ns = 0;
    for (int t = 0; t < n; t++) {
        sph += sf; sph -= floorf(sph);
        float si = 0.0f, sq = 0.0f;
        for (int p = 0; p < 4; p++) { float a = sph * (float)H[p]; a -= floorf(a); si += w[p] * de_sin_turns(a); sq += w[p] * de_sin_turns(a + 0.25f); }
        si *= wn; sq *= wn;
        hph += shf; hph -= floorf(hph);
        float hs = de_sin_turns(hph), hc = de_sin_turns(hph + 0.25f);
        float xi = di[wi], xq = dq[wi];
        float yi = xi * hc - xq * hs, yq = xi * hs + xq * hc;
        float oi = clampf(inj * si + fbk * yi, -1.2f, 1.2f), oq = clampf(inj * sq + fbk * yq, -1.2f, 1.2f);
        di[wi] = oi; dq[wi] = oq;
        if (++wi >= SPD) wi = 0;
        out[t] = clampf(oi * 1.05f, -1.0f, 1.0f);
        if (t % SNAP_EVERY == 0 && ns < NSNAP) {
            for (int j = 0; j < SPD; j++) { int m = (wi + j) % SPD; sn[ns].v[j] = di[m]; sn[ns].v[32 + j] = dq[m]; }
            sn[ns].r = hph; ns++;
        }
    }
    *nsn = ns;
    return n;
}

// ── LOCKSTEP (lockstep_engine.cc): a phase-locked loop ──────────────────────────
static const int LK_NUM[15] = { 1, 1, 1, 2, 3, 1, 4, 3, 2, 5, 3, 4, 5, 6, 8 };
static const int LK_DEN[15] = { 4, 3, 2, 3, 4, 1, 3, 2, 1, 2, 1, 1, 1, 1, 1 };
static float wrap01(float x) { x -= floorf(x); return x; }
static int lock_index(const float *k) { int i = (int)(k[0] * 15.0f); return i > 14 ? 14 : i; }
static int render_lock(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    int ri = lock_index(k);
    const float num = (float)LK_NUM[ri], den = (float)LK_DEN[ri], ratio = num / den;
    const float ceil_ = 0.22f / fmaxf(1.0f, ratio), rf = fminf(f, ceil_), tf = rf * ratio;
    float ffreq = tf * 0.82f, rph = 0.0f, fph = 0.0f, dlp = 0.0f;
    fph = wrap01(fph + 0.29f + 0.17f * 0.5f);                         // the note-on trigger (accent 0.5)
    ffreq *= 0.70f + 0.16f * k[2];
    const float bw = 0.00012f + 0.0075f * k[1] * k[1], damp = 0.35f + 1.45f * k[3];
    const float pg = fminf(0.08f, 2.0f * damp * bw), ig = bw * bw, feed = 0.35f * k[3];
    const float cap = 0.10f + 1.85f * k[3], cmin = tf * semis(-12.0f * cap), cmax = tf * semis(12.0f * cap);
    int ns = 0;
    for (int t = 0; t < n; t++) {
        rph = wrap01(rph + rf);
        float rc = wrap01(rph * num), fc = wrap01(fph * den);
        float pe = wrap01(rc - fc + 0.5f) - 0.5f;
        float det = de_sin_turns(8.0f + pe);
        det = det + (pe * 2.0f - det) * k[2];
        ffreq += ig * det;
        ffreq += bw * 0.0015f * (tf - ffreq);
        ffreq = clampf(ffreq, cmin, cmax);
        fph = wrap01(fph + ffreq + pg * det);
        float ripple = de_sin_turns(rc), car = de_sin_turns(wrap01(fph + feed * ripple));
        float sq = car / (0.22f + 0.78f * fabsf(car)), es = fminf(1.0f, k[2] + 4.0f * fabsf(pe));
        out[t] = (car + (sq - car) * es) * 0.78f;
        if (t % SNAP_EVERY == 0 && ns < NSNAP) { sn[ns].mx = rc; sn[ns].my = fc; sn[ns].r = pe; sn[ns].v[0] = ffreq / fmaxf(tf, 1e-9f); ns++; }
    }
    (void)dlp;
    *nsn = ns;
    return n;
}

// ── VOSIM (vosim_engine.cc) ─────────────────────────────────────────────────────
static const uint16_t VS_BELL[257] = {
    0, 670, 2655, 5873, 10191, 15434, 21387, 27805, 34427, 40980, 47198, 52824, 57630, 61417, 64032, 65366,
    65534, 65528, 65517, 65500, 65477, 65449, 65415, 65376, 65331, 65280, 65224, 65162, 65095, 65022, 64944, 64860,
    64770, 64675, 64574, 64468, 64357, 64240, 64118, 63990, 63857, 63718, 63575, 63426, 63271, 63112, 62947, 62777,
    62602, 62421, 62236, 62046, 61850, 61650, 61444, 61234, 61018, 60798, 60573, 60343, 60109, 59870, 59626, 59377,
    59124, 58866, 58604, 58338, 58067, 57791, 57512, 57228, 56940, 56648, 56351, 56051, 55746, 55438, 55126, 54810,
    54490, 54166, 53839, 53508, 53173, 52835, 52494, 52149, 51801, 51449, 51094, 50736, 50375, 50011, 49645, 49275,
    48902, 48526, 48148, 47767, 47384, 46998, 46610, 46219, 45826, 45431, 45033, 44633, 44232, 43828, 43423, 43015,
    42606, 42195, 41783, 41369, 40953, 40537, 40118, 39699, 39278, 38856, 38433, 38010, 37585, 37159, 36733, 36306,
    35879, 35450, 35022, 34593, 34163, 33734, 33304, 32874, 32445, 32015, 31585, 31156, 30727, 30298, 29869, 29442,
    29014, 28588, 28162, 27737, 27312, 26889, 26467, 26045, 25625, 25206, 24789, 24373, 23958, 23545, 23133, 22723,
    22315, 21908, 21504, 21101, 20700, 20302, 19905, 19511, 19119, 18730, 18343, 17958, 17576, 17196, 16819, 16445,
    16074, 15706, 15340, 14978, 14618, 14262, 13909, 13559, 13212, 12869, 12529, 12193, 11860, 11531, 11206, 10884,
    10566, 10252, 9941, 9635, 9332, 9034, 8740, 8450, 8164, 7882, 7604, 7331, 7062, 6798, 6538, 6283,
    6032, 5786, 5544, 5307, 5075, 4848, 4625, 4407, 4195, 3987, 3784, 3586, 3393, 3205, 3022, 2844,
    2671, 2504, 2342, 2185, 2033, 1887, 1746, 1610, 1479, 1354, 1235, 1121, 1012, 909, 811, 719,
    632, 550, 475, 405, 340, 281, 228, 180, 138, 101, 70, 45, 25, 11, 2, 0,
    0
};
static float vs_bell(float ph) { float ix = clampf(ph * 256.0f, 0.0f, 255.999f); int i = (int)ix; float fr = ix - (float)i;
    return ((float)VS_BELL[i] + ((float)VS_BELL[i + 1] - (float)VS_BELL[i]) * fr) * (1.0f / 65536.0f); }
static float vs_formant_hz(float knob) { int p = (int)clampf(knob * 32767.0f, 0.0f, 32767.0f); return midi_hz_f((float)(p >> 1) * (1.0f / 128.0f)); }
static float apply_macro(float stock, float lo, float hi, float m) { float a = m * 2.0f; return m < 0.5f ? lo + (stock - lo) * a : stock + (hi - stock) * (a - 1.0f); }
static int render_vosim(float *out, int n, float f, const float *k, Snap *sn, int *nsn) {
    const float PED = 24576.0f / 32768.0f, STOCK_SHARE = 16384.0f / 24576.0f, SOFF = 127.0f / 32768.0f, SAMP = 32639.0f / 32768.0f;
    const float SBP = 16.0f / 256.0f;
    static const float HB[5] = { 0.500547367f, 0.310005574f, -0.082226011f, 0.029547365f, -0.007600612f };
    const float inc = fminf(f * 0.5f, 0.249999f);
    const float fi1 = vs_formant_hz(k[1]) / (float)SR * 0.5f, fi2 = vs_formant_hz(k[0]) / (float)SR * 0.5f;
    const float bp = apply_macro(SBP, 1.0f / 256.0f, 0.5f, k[2]);
    const float as = SBP / bp, ds = (1.0f - SBP) / (1.0f - bp);
    const float share = apply_macro(STOCK_SHARE, 0.0f, 1.0f, k[3]);
    const float ped = PED * (1.0f + SOFF), a1 = SAMP * PED * share, a2 = SAMP * PED - a1;
    float ph = 0.0f, fp1 = 0.75f, fp2 = 0.75f, hist[16], dc = 0.0f; int hp = 0;
    memset(hist, 0, sizeof hist);
    int ns = 0, st = period_stride(f);
    Cap cap = { 0, -1, 0 };
    for (int t = 0; t < n; t++) {
        int cyc = 0;
        for (int s2 = 0; s2 < 2; s2++) {
            bool wrapped = false;
            ph += inc; if (ph >= 1.0f) { ph -= 1.0f; wrapped = true; }
            fp1 += fi1; if (fp1 >= 1.0f) fp1 -= 1.0f;
            fp2 += fi2; if (fp2 >= 1.0f) fp2 -= 1.0f;
            float wp = ph < bp ? ph * as : SBP + (ph - bp) * ds;
            float smp = (ped + a1 * de_sin_turns(fp1) + a2 * de_sin_turns(fp2)) * vs_bell(wp);
            if (wrapped) { fp1 = fp2 = 0.75f; smp = 0.0f; cyc = 1; }
            hp = (hp + 1) & 15; hist[hp] = smp - PED;
        }
        float raw = HB[0] * hist[(hp - 7) & 15] + HB[1] * (hist[(hp - 6) & 15] + hist[(hp - 8) & 15])
                  + HB[2] * (hist[(hp - 4) & 15] + hist[(hp - 10) & 15]) + HB[3] * (hist[(hp - 2) & 15] + hist[(hp - 12) & 15])
                  + HB[4] * (hist[hp] + hist[(hp - 14) & 15]);
        dc += 0.001f * (raw - dc);
        out[t] = raw - dc;
        cap_step(&cap, sn, &ns, t, cyc, st, out[t]);
    }
    *nsn = ns;
    return n;
}

// ── GLISSON (glisson_engine.cc): chirping grains ────────────────────────────────
#define NGR 5
typedef struct { float ph, env, einc, r0, r1; } Grain;
static void gl_start(Grain *g, Rng *rn, float scatter, float dir, float dur, float delay) {
    float centre = (2.0f * rng01(rn) - 1.0f) * scatter, exc = dir * (3.0f + 0.65f * scatter);
    g->r0 = semis(centre - 0.5f * exc); g->r1 = semis(centre + 0.5f * exc);
    g->env = -delay; g->einc = 1.0f / dur;
}
static int render_glisson(float *out, int n, float f, const float *k, Snap *sn, int *nsn, uint32_t seed) {
    Rng rn = { seed };
    Grain gr[NGR];
    const float scatter = 2.0f + 34.0f * k[0] * k[0], dir = 2.0f * k[2] - 1.0f;
    const float dur = 0.012f * semis(k[3] * 60.0f) * (float)SR;
    const int ng = 1 + (int)(k[1] * (float)(NGR - 1) + 0.5f);
    for (int i = 0; i < NGR; i++) { gr[i].ph = 0.0f; gl_start(&gr[i], &rn, scatter, dir, dur, (float)i / (float)ng); }
    const float gain = 0.72f / (float)ng;
    int ns = 0;
    for (int t = 0; t < n; t++) {
        float acc = 0.0f;
        for (int i = 0; i < ng; i++) {
            Grain *g = &gr[i];
            g->env += g->einc;
            if (g->env >= 1.0f) gl_start(g, &rn, scatter, dir, dur, 0.0f);
            if (g->env < 0.0f) continue;
            float u = g->env, cu = u * u * (3.0f - 2.0f * u); u += (cu - u) * k[3];
            float ratio = g->r0 + (g->r1 - g->r0) * u;
            g->ph += fminf(0.22f, f * ratio); g->ph -= floorf(g->ph);
            acc += de_sin_turns(g->ph) * 4.0f * g->env * (1.0f - g->env) * gain;
        }
        out[t] = acc;
        if (t % SNAP_EVERY == 0 && ns < NSNAP) {
            for (int i = 0; i < NGR; i++) {
                float u = gr[i].env, cu = u * u * (3.0f - 2.0f * u); u += (cu - u) * k[3];
                sn[ns].v[i] = i < ng && gr[i].env >= 0.0f ? gr[i].env : -1.0f;
                sn[ns].v[8 + i] = de_log2f(gr[i].r0 + (gr[i].r1 - gr[i].r0) * clampf(u, 0.0f, 1.0f)) * 12.0f;
            }
            sn[ns].row = (uint32_t)ng; ns++;
        }
    }
    *nsn = ns;
    return n;
}

// ── one render, any specimen ────────────────────────────────────────────────────

static int render_spec(int sp, float *out, int n, int midi, Snap *sn, int *nsn) {
    float f = midi_hz(midi) / (float)SR;
    const float *k = K[sp];
    switch (sp) {
        case SP_FLOCK: render_flock(out, n, f, k, sn, nsn); break;
        case SP_RULES: render_rules(out, n, f, k, sn, nsn); break;
        case SP_SCAN:  render_scan(out, n, f, k, sn, nsn); break;
        case SP_GENDY: render_gendy(out, n, f, k, sn, nsn, 0x9e3779b9u ^ ((uint32_t)midi * 7919u)); break;
        case SP_ATTR:  render_attr(out, n, f, k, sn, nsn); break;
        case SP_BYTE:  render_byte(out, n, f, k, sn, nsn); break;
        case SP_PULSAR:  render_pulsar(out, n, f, k, sn, nsn); break;
        case SP_TERRAIN: render_terrain(out, n, f, k, sn, nsn); break;
        case SP_SPIRAL:  render_spiral(out, n, f, k, sn, nsn); break;
        case SP_LOCK:    render_lock(out, n, f, k, sn, nsn); break;
        case SP_VOSIM:   render_vosim(out, n, f, k, sn, nsn); break;
        default:         render_glisson(out, n, f, k, sn, nsn, 0x7f4a7c15u ^ ((uint32_t)midi * 104729u)); break;
    }
    // DC blocker, fades, peak-normalise
    float hx = 0.0f, hy = 0.0f, pk = 0.0f;
    const int fin = SR * 3 / 1000, fout = SR * 40 / 1000;
    for (int i = 0; i < n; i++) {
        hy = out[i] - hx + 0.99857f * hy; hx = out[i];
        float e = 1.0f;
        if (i < fin) e = (float)i / (float)fin;
        if (i > n - fout) e *= (float)(n - i) / (float)fout;
        out[i] = hy * e;
        float a = fabsf(out[i]); if (a > pk) pk = a;
    }
    if (pk > 1e-4f) { float g = 0.9f / pk; for (int i = 0; i < n; i++) out[i] *= g; }
    return n;
}

// ── voicing ─────────────────────────────────────────────────────────────────────
static int play(int midi, int vel, int n) {
    if (midi < 0 || midi > 127) return -1;
    int v = rr; rr = (rr + 1) % NV;
    if (vhandle[v] >= 0) { note_off(vhandle[v]); vhandle[v] = -1; }
    ren_n = render_spec(cur, ren, n, midi, snaps, &nsnap);
    snap_spec = cur;
    sample_load(v, ren, ren_n);
    instrument_sample(I0 + v, v, midi);
    vhandle[v] = note_on(midi, I0 + v, vel);
    note_frame = frame();
    kb_glow[midi] = 1.0f;
    return v;
}
static void release_slot(int v) { if (v >= 0 && v < NV && vhandle[v] >= 0) { note_off(vhandle[v]); vhandle[v] = -1; } }
static void on_note(int midi, int vel) { autoplay = false; if (handle[midi] >= 0) release_slot(handle[midi]); handle[midi] = play(midi, vel, REN_N); }
static void on_off(int midi) { if (midi >= 0 && midi < 128 && handle[midi] >= 0) { release_slot(handle[midi]); handle[midi] = -1; } }

static void set_spec(int s) { cur = ((s % NSPEC) + NSPEC) % NSPEC; dirty = true; }

void init(void) {
    for (int v = 0; v < NV; v++) { instrument(I0 + v, INSTR_SAMPLE, 2, 0, 7, 300); vhandle[v] = -1; }
    for (int m = 0; m < 128; m++) handle[m] = -1;
    keybed_config(I0, 3, 14);
    keybed_layout(0, 152, SCREEN_W, SCREEN_H - 152);
    keybed_manage_voices(false);
    keybed_on_note(on_note);
    keybed_on_off(on_off);
    bpm(84);
    ren_n = render_spec(cur, ren, PREVIEW_N, 57, snaps, &nsnap);    // a picture before the first note
    snap_spec = cur;
}

// autoplay: a slow walk, two notes per specimen, then the next specimen
static void walk_step(void) {
    static const int walk[4] = { 45, 52, 48, 55 };
    if ((apos & 1) == 0 && apos > 0) cur = (cur + ((apos & 3) == 0 ? 1 : 0)) % NSPEC;
    if (auto_v >= 0) release_slot(auto_v);
    auto_v = play(walk[apos & 3], 6, SR * 3);
    auto_left = 150;
    apos++;
}

void update(void) {
    if (keyp(KEY_TAB)) set_spec(cur + 1);
    if (keyp('M')) autoplay = !autoplay;
    keybed_update();
    if (dirty && !autoplay && frame() % 10 == 0) {
        dirty = false;
        if (preview_v >= 0) release_slot(preview_v);
        preview_v = play(57, 5, PREVIEW_N);
        preview_left = 60;
    }
    if (preview_left > 0 && --preview_left == 0) { release_slot(preview_v); preview_v = -1; }
    if (auto_left > 0 && --auto_left == 0) { release_slot(auto_v); auto_v = -1; }
    if (autoplay && every(2)) walk_step();
#ifdef DE_TRACE
    watch("spec", "%d", cur);
    watch("k0", "%.2f", K[cur][0]);
    watch("nsnap", "%d", nsnap);
    watch("auto", "%d", autoplay ? 1 : 0);
#endif
}

// ── the pictures: each draws the snapshot at the playback moment ───────────────
static void draw_flock(int x0, int y0, int w, int h, const Snap *s) {
    int cx = x0 + w / 2, cy = y0 + h / 2, rad = h / 2 - 6;
    circ(cx, cy, rad, CLR_DARKER_GREY);
    for (int i = 0; i < NF; i++) {
        float a = s->v[i];
        int px = cx + (int)(de_cos_turns(a) * (float)rad), py = cy - (int)(de_sin_turns(a) * (float)rad);
        int col = i == 3 ? CLR_LIGHT_YELLOW : (i < 3 ? CLR_ORANGE : CLR_PEACH);
        circfill(px, py, 3, col);
    }
    line(cx, cy, cx + (int)(s->mx * (float)rad), cy - (int)(s->my * (float)rad), CLR_LIME_GREEN);
    circfill(cx, cy, 1, CLR_LIME_GREEN);
    font(FONT_TINY);
    print(str("sync %.2f", s->r), x0 + 4, y0 + h - 8, CLR_LIME_GREEN);
    font(FONT_NORMAL);
}
static void draw_rules(int x0, int y0, int w, int h, int idx) {
    // the spacetime diagram: one row per snapshot, newest at the bottom
    int rows = (h - 10) / 3, cw = w / 32;
    for (int r = 0; r < rows; r++) {
        int si = idx - (rows - 1 - r) * 2; if (si < 0) continue;
        uint32_t row = snaps[si].row;
        for (int c = 0; c < 32; c++) if (row & (1u << c)) rectfill(x0 + c * cw, y0 + r * 3, cw - 1, 2, r == rows - 1 ? CLR_LIGHT_YELLOW : CLR_ORANGE);
    }
    font(FONT_TINY);
    print(str("rule %d  gen %.0f", 1 + (int)(253.999f * K[SP_RULES][0]), snaps[idx].r), x0 + 4, y0 + h - 7, CLR_WHITE);
    font(FONT_NORMAL);
}
static void draw_scan(int x0, int y0, int w, int h, const Snap *s) {
    int cx = x0 + w / 2, cy = y0 + h / 2, rad = h / 2 - 16;
    circ(cx, cy, rad, CLR_DARKER_GREY);
    int px0 = 0, py0 = 0, fx = 0, fy = 0;
    float pk = 0.02f; for (int i = 0; i < NM; i++) pk = fmaxf(pk, fabsf(s->v[i]));
    for (int i = 0; i <= NM; i++) {
        int m = i % NM; float a = (float)m / (float)NM, rr2 = (float)rad + s->v[m] / pk * 12.0f;
        int px = cx + (int)(de_cos_turns(a) * rr2), py = cy - (int)(de_sin_turns(a) * rr2);
        if (i > 0) line(px0, py0, px, py, CLR_PEACH); else { fx = px; fy = py; }
        if (i < NM) circfill(px, py, 1, CLR_ORANGE);
        px0 = px; py0 = py;
    }
    (void)fx; (void)fy;
    float a = s->r;                                                  // the read head
    line(cx, cy, cx + (int)(de_cos_turns(a) * (float)(rad + 14)), cy - (int)(de_sin_turns(a) * (float)(rad + 14)), CLR_LIME_GREEN);
}
static void draw_gendy(int x0, int y0, int w, int h, int idx) {
    for (int back = 6; back >= 0; back--) {
        int si = idx - back * 3; if (si < 0) continue;
        const Snap *s = &snaps[si];
        int nb = (int)s->row; if (nb < 3 || nb > NGB) continue;
        int col = back == 0 ? CLR_LIGHT_YELLOW : back < 3 ? CLR_BROWN : CLR_DARK_BROWN;
        int px0 = x0, py0 = y0 + h / 2 - (int)(s->v[0] * (float)(h / 2 - 6));
        for (int i = 1; i <= nb; i++) {
            float bx = s->v[16 + i - 1];
            int px = x0 + (int)(bx * (float)w), py = y0 + h / 2 - (int)(s->v[i % nb] * (float)(h / 2 - 6));
            line(px0, py0, px, py, col);
            if (back == 0) circfill(px0, py0, 2, CLR_ORANGE);
            px0 = px; py0 = py;
        }
    }
    font(FONT_TINY);
    print(str("%d breakpoints", (int)snaps[idx].row), x0 + 4, y0 + h - 7, CLR_WHITE);
    font(FONT_NORMAL);
}
static void draw_attr(int x0, int y0, int w, int h, int idx) {
    int cx = x0 + w / 2, cy = y0 + h / 2;
    float pk = 1e-3f;                                                 // autoscale to the orbit on screen
    for (int back = 12; back >= 0; back--) { int si = idx - back; if (si < 0) continue;
        for (int p = 0; p < 64; p++) pk = fmaxf(pk, fabsf(snaps[si].v[p])); }
    float sc = (float)(h / 2 - 6) / pk;
    for (int back = 12; back >= 0; back--) {
        int si = idx - back; if (si < 0) continue;
        int col = back == 0 ? CLR_LIGHT_YELLOW : back < 5 ? CLR_ORANGE : CLR_BROWN;
        for (int p = 0; p < 32; p++) {
            int px = cx + (int)(snaps[si].v[p] * sc), py = cy - (int)(snaps[si].v[32 + p] * sc);
            if (px > x0 && px < x0 + w && py > y0 && py < y0 + h) pset(px, py, col);
        }
    }
}
static void draw_byte(int x0, int y0, int w, int h, const Snap *s) {
    uint32_t p0, p1; int width; int fo = bb_parts(K[SP_BYTE], &p0, &p1, &width);
    font(FONT_TINY);
    print(BB_TEXT[fo][0], x0 + 4, y0 + 4, CLR_PEACH);
    print(BB_TEXT[fo][1], x0 + 4, y0 + 11, CLR_PEACH);
    print(str("p0=%u  p1=%u  %d bits  t=%u", p0, p1, width, s->row), x0 + 4, y0 + 19, CLR_MEDIUM_GREY);
    for (int b = 0; b < 32; b++)                                      // t in binary, the counter itself
        rectfill(x0 + 4 + (31 - b) * 6, y0 + 28, 5, 5, (s->row >> b) & 1 ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
    int mid = y0 + 38 + (h - 42) / 2, amp = (h - 46) / 2, cw = w / 64;
    for (int i = 0; i < 64; i++) {                                     // the stepped byte stream
        int yy = mid - (int)(s->v[i] * (float)amp);
        rectfill(x0 + i * cw, yy < mid ? yy : mid, cw, abs(yy - mid) + 1, CLR_ORANGE);
    }
    font(FONT_NORMAL);
}


static void draw_period(int x0, int y0, int w, int h, const Snap *s, int col, const char *label) {
    int mid = y0 + h / 2, amp = h / 2 - 8, cw = w / 64;
    float pk = 1e-3f; for (int i = 0; i < 64; i++) pk = fmaxf(pk, fabsf(s->v[i]));
    line(x0, mid, x0 + w, mid, CLR_DARKER_GREY);
    for (int i = 1; i < 64; i++)
        line(x0 + (i - 1) * cw, mid - (int)(s->v[i - 1] / pk * (float)amp), x0 + i * cw, mid - (int)(s->v[i] / pk * (float)amp), col);
    font(FONT_TINY); print(label, x0 + 4, y0 + h - 7, CLR_WHITE); font(FONT_NORMAL);
}
static void draw_terrain(int x0, int y0, int w, int h, int idx) {
    const float *k = K[SP_TERRAIN];
    const float xo = 1.9f * k[2] - 1.0f, yo = 1.9f * k[3] - 0.95f, zs = terrain_sel(k);
    const int cs = 4;                                               // the surface, as a heat map
    static const int RAMP[6] = { CLR_BLACK, CLR_DARK_BLUE, CLR_DARK_PURPLE, CLR_BROWN, CLR_ORANGE, CLR_PEACH };
    for (int cy = 0; cy < h / cs; cy++)
        for (int cx = 0; cx < w / cs; cx++) {
            float x = -1.0f + 2.0f * ((float)cx + 0.5f) / (float)(w / cs), y = 1.0f - 2.0f * ((float)cy + 0.5f) / (float)(h / cs);
            int band = (int)((terrain_at(x, y, zs) * 0.5f + 0.5f) * 5.999f); if (band < 0) band = 0; if (band > 5) band = 5;
            rectfill(x0 + cx * cs, y0 + cy * cs, cs, cs, RAMP[band]);
        }
    const Snap *s = &snaps[idx];
    float rad = s->r;                                               // the orbit, and where it is now
    for (int a = 0; a < 48; a++) {
        float t = (float)a / 48.0f;
        float x = de_cos_turns(t) * rad * (1.0f - fabsf(xo)) + xo, y = de_sin_turns(t) * rad * (1.0f - fabsf(yo)) + yo;
        pset(x0 + (int)((x + 1.0f) * 0.5f * (float)w), y0 + (int)((1.0f - y) * 0.5f * (float)h), CLR_WHITE);
    }
    circfill(x0 + (int)((s->mx + 1.0f) * 0.5f * (float)w), y0 + (int)((1.0f - s->my) * 0.5f * (float)h), 2, CLR_LIME_GREEN);
}
static void draw_spiral(int x0, int y0, int w, int h, int idx) {
    int cx = x0 + w / 2, cy = y0 + h / 2;
    float pk = 1e-3f;
    for (int back = 0; back < 14; back++) { int si = idx - back; if (si < 0) continue; for (int j = 0; j < 64; j++) pk = fmaxf(pk, fabsf(snaps[si].v[j])); }
    float sc = (float)(h / 2 - 6) / pk;
    for (int back = 13; back >= 0; back--) {
        int si = idx - back; if (si < 0) continue;
        int col = back == 0 ? CLR_LIGHT_YELLOW : back < 4 ? CLR_ORANGE : back < 9 ? CLR_BROWN : CLR_DARK_BROWN;
        for (int j = 1; j < 32; j++)
            line(cx + (int)(snaps[si].v[j - 1] * sc), cy - (int)(snaps[si].v[31 + j] * sc), cx + (int)(snaps[si].v[j] * sc), cy - (int)(snaps[si].v[32 + j] * sc), col);
    }
    font(FONT_TINY); print("the 32-sample loop in the I/Q plane", x0 + 4, y0 + h - 7, CLR_WHITE); font(FONT_NORMAL);
}
static void draw_lock(int x0, int y0, int w, int h, int idx) {
    const Snap *s = &snaps[idx];
    int cx = x0 + 52, cy = y0 + h / 2, rad = h / 2 - 8;
    circ(cx, cy, rad, CLR_DARKER_GREY);
    line(cx, cy, cx + (int)(de_sin_turns(s->mx) * (float)rad), cy - (int)(de_cos_turns(s->mx) * (float)rad), CLR_PEACH);
    line(cx, cy, cx + (int)(de_sin_turns(s->my) * (float)(rad - 6)), cy - (int)(de_cos_turns(s->my) * (float)(rad - 6)), CLR_LIME_GREEN);
    int gx = x0 + 110, gw = w - 116, gm = cy;                       // the phase error over time
    line(gx, gm, gx + gw, gm, CLR_DARKER_GREY);
    for (int i = 0; i < gw; i++) { int si = idx - (gw - 1 - i); if (si < 0) continue;
        pset(gx + i, gm - (int)(snaps[si].r * 2.0f * (float)(h / 2 - 8)), CLR_ORANGE); }
    font(FONT_TINY);
    int ri = lock_index(K[SP_LOCK]);
    print(str("ratio %d/%d", LK_NUM[ri], LK_DEN[ri]), x0 + 4, y0 + 4, CLR_WHITE);
    print("note", x0 + 4, y0 + h - 14, CLR_PEACH); print("follower", x0 + 4, y0 + h - 7, CLR_LIME_GREEN);
    print("phase error", gx, y0 + 4, CLR_MEDIUM_GREY);
    print(str("follower %.3fx target", s->v[0]), gx, y0 + h - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
static void draw_glisson(int x0, int y0, int w, int h, int idx) {
    int mid = y0 + h / 2;
    line(x0, mid, x0 + w, mid, CLR_DARKER_GREY);                    // the played pitch
    float span = 2.0f + 34.0f * K[SP_GLISSON][0] * K[SP_GLISSON][0] + 26.0f;
    for (int i = 0; i < w / 2; i++) {                                 // 2 px per snapshot, newest at the right
        int si = idx - (w / 2 - 1 - i); if (si < 0) continue;
        for (int g = 0; g < NGR; g++) {
            float e = snaps[si].v[g]; if (e < 0.0f) continue;
            int py = mid - (int)(snaps[si].v[8 + g] / span * (float)(h / 2 - 4));
            int col = e < 0.2f || e > 0.8f ? CLR_BROWN : CLR_ORANGE;
            if (py > y0 && py < y0 + h) rectfill(x0 + i * 2, py, 2, 1, col);
        }
    }
    font(FONT_TINY); print(str("%d grains, pitch over time", (int)snaps[idx].row), x0 + 4, y0 + h - 7, CLR_WHITE); font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("SPECIMENS", 6, 3, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print_right(autoplay ? "M auto: on" : "M auto: off", SCREEN_W - 6, 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    for (int s = 0; s < NSPEC; s++) {
        int x = 4 + (s % 6) * 52, y = 13 + (s / 6) * 13, w = 50, h = 11;
        if (s == cur) rectfill(x - 1, y - 1, w + 2, h + 2, CLR_ORANGE);
        if (ui_button(x, y, w, h, SPNAME[s])) { set_spec(s); autoplay = false; }
    }

    // the picture, at the playback moment of the last note (or its last frame when it ended)
    const int px = 4, py = 41, pw = 220, ph = 94;
    rectfill(px, py, pw, ph, CLR_BLACK);
    rect(px - 1, py - 1, pw + 2, ph + 2, CLR_DARKER_GREY);
    if (nsnap > 0) {
        int idx = frame() - note_frame; if (idx < 0) idx = 0; if (idx >= nsnap) idx = nsnap - 1;
        clip(px, py, pw, ph);
        switch (snap_spec) {
            case SP_FLOCK: draw_flock(px, py, pw, ph, &snaps[idx]); break;
            case SP_RULES: draw_rules(px, py, pw, ph, idx); break;
            case SP_SCAN:  draw_scan(px, py, pw, ph, &snaps[idx]); break;
            case SP_GENDY: draw_gendy(px, py, pw, ph, idx); break;
            case SP_ATTR:  draw_attr(px, py, pw, ph, idx); break;
            case SP_BYTE:  draw_byte(px, py, pw, ph, &snaps[idx]); break;
            case SP_PULSAR:  draw_period(px, py, pw, ph, &snaps[idx], CLR_ORANGE, str("pulsaret = the sounding %.0f%%, formant x%.1f", snaps[idx].r * 100.0f, snaps[idx].mx)); break;
            case SP_TERRAIN: draw_terrain(px, py, pw, ph, idx); break;
            case SP_SPIRAL:  draw_spiral(px, py, pw, ph, idx); break;
            case SP_LOCK:    draw_lock(px, py, pw, ph, idx); break;
            case SP_VOSIM:   draw_period(px, py, pw, ph, &snaps[idx], CLR_PEACH, "one cycle: the windowed formant pulse"); break;
            default:         draw_glisson(px, py, pw, ph, idx); break;
        }
        clip(0, 0, SCREEN_W, SCREEN_H);
    }
    font(FONT_TINY);
    print(CAPTION[cur], 6, 139, CLR_MEDIUM_GREY);
    if (snap_spec != cur) print("(the picture is the last note's; play one)", 6, 145, CLR_DARK_GREY);

    // four knobs, named for this specimen
    font(FONT_SMALL);
    for (int k = 0; k < 4; k++) {
        int kx = 246 + (k & 1) * 44, ky = 56 + (k >> 1) * 46;
        float before = K[cur][k];
        if (ui_knob(&K[cur][k], kx, ky, KLABEL[cur][k]) && K[cur][k] != before) dirty = true;
    }
    font(FONT_NORMAL);
    keybed_draw();
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
#define SPN (SR / 2)
static float sb[NSPEC][SPN];
static Snap  ss[SPN / SNAP_EVERY + 2];
static float s_peak(const float *b, int n) { float p = 0; for (int i = 0; i < n; i++) { float a = fabsf(b[i]); if (a > p) p = a; } return p; }
static int   s_finite(const float *b, int n) { for (int i = 0; i < n; i++) if (!(b[i] == b[i]) || fabsf(b[i]) > 1.0f) return 0; return 1; }
static float s_diff(const float *a, const float *b, int n) { double d = 0; for (int i = 0; i < n; i++) d += fabsf(a[i] - b[i]); return (float)(d / n); }

void spec(void) {
    autoplay = false;
    step(1);
    expect(nsnap > 0, "init leaves a picture");

    // every specimen renders finite, normalised, leaves a snapshot per frame, and differs from the rest
    int nsn = 0;
    for (int s = 0; s < NSPEC; s++) {
        int n = render_spec(s, sb[s], SPN, 57, ss, &nsn);
        expect(s_finite(sb[s], n), str("%s renders finite, in range", SPNAME[s]));
        expect(s_peak(sb[s], n) > 0.85f, str("%s is audible (peak-normalised)", SPNAME[s]));
        expect(nsn >= SPN / SNAP_EVERY - 1, str("%s leaves one snapshot per frame (%d)", SPNAME[s], nsn));
    }
    for (int a = 0; a < NSPEC; a++)
        for (int b = a + 1; b < NSPEC; b++)
            expect(s_diff(sb[a], sb[b], SPN) > 0.02f, str("%s and %s are different sounds", SPNAME[a], SPNAME[b]));

    // every knob of every specimen reaches its DSP
    for (int s = 0; s < NSPEC; s++)
        for (int k = 0; k < 4; k++) {
            float keep = K[s][k];
            render_spec(s, sb[0], SPN, 57, ss, &nsn);
            static float alt[SPN];
            K[s][k] = keep > 0.5f ? 0.05f : 0.95f;
            render_spec(s, alt, SPN, 57, ss, &nsn);
            K[s][k] = keep;
            expect(s_diff(sb[0], alt, SPN) > 0.002f, str("%s: the %s knob reaches the sound", SPNAME[s], KLABEL[s][k]));
        }

    // deterministic: gendy is stochastic but seeded from the note, so a note renders the same twice
    {
        static float g1[SPN], g2[SPN];
        render_spec(SP_GENDY, g1, SPN, 57, ss, &nsn);
        render_spec(SP_GENDY, g2, SPN, 57, ss, &nsn);
        expect(memcmp(g1, g2, sizeof g1) == 0, "gendy renders the same note byte-identical twice");
        render_spec(SP_GENDY, g2, SPN, 58, ss, &nsn);
        expect(memcmp(g1, g2, sizeof g1) != 0, "and a different note walks differently");
    }

    // the physics each picture claims
    {   // FLOCK: coupling pulls the flock into sync (the Kuramoto order parameter rises)
        float kf[4]; memcpy(kf, K[SP_FLOCK], sizeof kf);
        K[SP_FLOCK][0] = 0.30f; K[SP_FLOCK][2] = 0.0f; K[SP_FLOCK][3] = 0.5f;
        K[SP_FLOCK][1] = 0.0f;  render_spec(SP_FLOCK, sb[0], SPN, 57, ss, &nsn); float r_free = ss[nsn - 1].r;
        K[SP_FLOCK][1] = 1.0f;  render_spec(SP_FLOCK, sb[0], SPN, 57, ss, &nsn); float r_lock = ss[nsn - 1].r;
        memcpy(K[SP_FLOCK], kf, sizeof kf);
        expect(r_lock > 0.9f, str("FLOCK: full coupling synchronises (r = %.2f)", r_lock));
        expect(r_lock > r_free + 0.3f, str("FLOCK: and uncoupled it does not (r = %.2f)", r_free));
    }
    {   // RULES: the automaton is Wolfram's: rule 90 on one cell = its two neighbours (XOR)
        expect(ca_step(1u << 10, 90, 1) == ((1u << 9) | (1u << 11)), "RULES: rule 90 is the XOR of the neighbours");
        expect(ca_step(1u << 10, 204, 1) == (1u << 10), "RULES: rule 204 is the identity");
        expect(fabsf(density(0xffff0000u) - 0.5f) < 1e-6f, "RULES: density is the popcount / 32");
    }
    {   // SCANNED: the knob is damping AND a wavefolder (upstream), and the fold raises the OUTPUT, so
        // judge the string itself: the ring's energy (from the snapshots) decays faster when damped
        float kf[4]; memcpy(kf, K[SP_SCAN], sizeof kf);
        float ratio[2];
        for (int j = 0; j < 2; j++) {
            K[SP_SCAN][2] = j ? 0.95f : 0.05f;
            render_scan(sb[0], SPN, 220.0f / SR, K[SP_SCAN], ss, &nsn);
            float e0 = 0, e1 = 0;
            for (int i = 0; i < NM; i++) { e0 += ss[2].v[i] * ss[2].v[i]; e1 += ss[nsn - 1].v[i] * ss[nsn - 1].v[i]; }
            ratio[j] = e1 / fmaxf(e0, 1e-9f);
        }
        memcpy(K[SP_SCAN], kf, sizeof kf);
        expect(ratio[1] < ratio[0] * 0.5f, str("SCANNED: a damped ring loses its energy faster (%.3f vs %.3f kept)", ratio[1], ratio[0]));
    }
    {   // ATTRACTOR: bounded, and more chaos (less damping) visits more of the space
        float kf[4]; memcpy(kf, K[SP_ATTR], sizeof kf);
        K[SP_ATTR][1] = 0.0f; render_attr(sb[0], SPN, 110.0f / SR, K[SP_ATTR], ss, &nsn);
        float span_lo = 0; for (int p = 0; p < 32; p++) span_lo = fmaxf(span_lo, fabsf(ss[nsn - 1].v[p]));
        K[SP_ATTR][1] = 1.0f; render_attr(sb[0], SPN, 110.0f / SR, K[SP_ATTR], ss, &nsn);
        float span_hi = 0; for (int p = 0; p < 32; p++) span_hi = fmaxf(span_hi, fabsf(ss[nsn - 1].v[p]));
        memcpy(K[SP_ATTR], kf, sizeof kf);
        expect(span_hi <= 1.0f && span_lo <= 1.0f, "ATTRACTOR: the shaped state stays bounded");
        expect(span_hi > span_lo, str("ATTRACTOR: chaos widens the orbit (%.2f vs %.2f)", span_hi, span_lo));
    }
    {   // BYTEBEAT: width 8 reads the low byte as signed 8-bit, as Braids did
        expect(spec_close(bb_sample(0x7f, 8), 127.0f / 128.0f, 1e-6f) && spec_close(bb_sample(0x80, 8), -1.0f, 1e-6f), "BYTEBEAT: 8-bit two's complement read");
        uint32_t p0, p1; int w; float kk[4] = { 0.5f, 0.0f, 0.99f, 0.5f };
        bb_parts(kk, &p0, &p1, &w);
        expect(p1 >= 1u, "BYTEBEAT: formula 3's modulo divisor never reaches zero");
        expect_eq(w, 8, "BYTEBEAT: bits knob centre = 8 bits");
    }

    {   // PULSAR: duty is the share of each cycle that sounds; the rest is silence
        float kp[4] = { 0.45f, 0.0f, 0.0f, 0.4f };
        render_pulsar(sb[0], SPN, 110.0f / SR, kp, ss, &nsn);
        int q0 = 0; for (int i = 0; i < SPN; i++) if (fabsf(sb[0][i]) < 1e-4f) q0++;
        kp[1] = 1.0f; render_pulsar(sb[0], SPN, 110.0f / SR, kp, ss, &nsn);
        int q1 = 0; for (int i = 0; i < SPN; i++) if (fabsf(sb[0][i]) < 1e-4f) q1++;
        expect(q0 > SPN * 0.8f && q1 < SPN * 0.2f, str("PULSAR: short duty is mostly silence (%d%% vs %d%%)", q0 * 100 / SPN, q1 * 100 / SPN));
    }
    {   // TERRAIN: the surface is bounded, and the orbit's radius follows the knob
        int ok = 1; for (int i = 0; i < 200; i++) { float x = -1.0f + 0.01f * i, y = 0.3f; for (int z = 0; z < 4; z++) if (fabsf(terrain_at(x, y, (float)z + 0.5f)) > 1.2f) ok = 0; }
        expect(ok, "TERRAIN: every terrain stays bounded");
        float a[4] = { 0, 0.1f, 0.5f, 0.5f }, b[4] = { 0, 0.9f, 0.5f, 0.5f };
        expect(terrain_radius(110.0f / SR, b) > terrain_radius(110.0f / SR, a) * 3.0f, "TERRAIN: radius widens the orbit");
    }
    {   // SPIRAL: with no shift and no feedback the loop just passes the source (periodic); shift + feedback smear it
        float ks[4] = { 0.5f, 0.5f, 0.0f, 0.5f };
        const float f = 441.0f / SR;                                  // a period of exactly 100 samples
        render_spiral(sb[0], SPN, f, ks, ss, &nsn);
        double d0 = 0; for (int i = SPN / 2; i < SPN - 100; i++) d0 += fabsf(sb[0][i] - sb[0][i + 100]);
        ks[2] = 0.9f; ks[3] = 1.0f; render_spiral(sb[0], SPN, f, ks, ss, &nsn);
        double d1 = 0; for (int i = SPN / 2; i < SPN - 100; i++) d1 += fabsf(sb[0][i] - sb[0][i + 100]);
        expect(d0 < 1.0 && d1 > d0 * 50.0, str("SPIRAL: stationary loop is periodic, a shifting loop is not (%.3g vs %.3g)", d0, d1));
    }
    {   // LOCKSTEP: a wide loop locks: small phase error, follower on its target
        float kl[4] = { 5.0f / 15.0f + 0.01f, 1.0f, 0.0f, 0.5f };    // ratio 1/1, full bandwidth
        render_lock(sb[0], SPN, 110.0f / SR, kl, ss, &nsn);
        expect(fabsf(ss[nsn - 1].r) < 0.05f, str("LOCKSTEP: the loop locks (phase error %.3f)", ss[nsn - 1].r));
        expect(fabsf(ss[nsn - 1].v[0] - 1.0f) < 0.01f, str("LOCKSTEP: the follower sits on its target (%.4f x)", ss[nsn - 1].v[0]));
        kl[1] = 0.0f; render_lock(sb[0], SPN, 110.0f / SR, kl, ss, &nsn);
        expect(fabsf(ss[nsn - 1].v[0] - 1.0f) > 0.01f, str("LOCKSTEP: a narrow loop has not caught up yet (%.4f x)", ss[nsn - 1].v[0]));
    }
    {   // VOSIM: the pedestal is removed: no DC
        render_vosim(sb[0], SPN, 110.0f / SR, K[SP_VOSIM], ss, &nsn);
        double m = 0; for (int i = SPN / 2; i < SPN; i++) m += sb[0][i]; m /= SPN / 2;
        expect(fabs(m) < 0.02, str("VOSIM: the pedestal is taken back out (mean %.4f)", m));
    }
    {   // GLISSON: the up-down knob sets which way a grain chirps
        Rng rn = { 1u }; Grain g;
        gl_start(&g, &rn, 10.0f, 1.0f, 500.0f, 0.0f);
        int up = g.r1 > g.r0;
        gl_start(&g, &rn, 10.0f, -1.0f, 500.0f, 0.0f);
        expect(up && g.r1 < g.r0, "GLISSON: up-down picks the chirp's direction");
    }

    // the panel
    spec_tap(KEY_TAB);
    expect_eq(cur, SP_RULES, "TAB steps to the next specimen");
    spec_tap('M');
    expect(autoplay, "M turns the walk on");
}
#endif
