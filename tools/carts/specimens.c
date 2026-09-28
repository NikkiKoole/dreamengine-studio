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
  "lineage": "Row 6 of docs/design/choochootracker-borrow-list.md: six of the ALGORITHMIC engines in Lyle Mills' Plaits-Alt fork (plaits_alt/dsp/engine2, MIT, 2026, vendored by Choochootracker), ported to cart-land C: phase_flock (Kuramoto-coupled oscillators), rulefield (a 1-D cellular automaton read as a wavecycle), scanned (scanned synthesis: a mass-spring ring read as a wavetable), gendy (Xenakis' dynamic stochastic synthesis), attractor (a Thomas cyclically symmetric chaotic flow) and bytebeat (Bees-in-the-Trees' four formulas, after Tim Churches' Braids port). Each note renders in cart-land into a PCM slot (the mme / sintered route) and the render also records the algorithm's STATE sixty times a second, so the picture is the process making the sound you hear, not an animation beside it.",
  "homage": "Lyle Mills' Plaits-Alt (2026) on Emilie Gillet's Plaits; Iannis Xenakis (GENDYN), Yoshiki Kuramoto, Stephen Wolfram's elementary automata, Bill Verplank / Max Mathews / Rob Shaw (scanned synthesis), Rene Thomas, and the bytebeat scene.",
  "description": {
    "summary": "A cabinet of six algorithmic synthesis specimens (a flock of oscillators falling into sync, a cellular automaton, a vibrating string network, Xenakis' random breakpoints, a chaotic attractor, bytebeat) each drawn live by the same process that makes its sound.",
    "detail": "Pick a specimen, play the keyboard. FLOCK: seven detuned oscillators pull on each other; turn COUPLING up and watch the dots clump as they fall into sync, the arrow in the middle is how synchronised they are. RULES: an elementary cellular automaton evolves once per cycle and its row IS the waveform; the picture is its spacetime diagram. SCANNED: a ring of 32 masses on springs, struck at note-on, read as a wavetable; you see the ring ringing. GENDY: a wave made of random breakpoints that take a random walk every cycle. ATTRACTOR: a three-way chaotic flow, drawn as its orbit. BYTEBEAT: a one-line integer formula as an oscillator, drawn as the byte stream. Each has four knobs named for what they do in THAT specimen. A note renders its sound (up to 6 s) when you press it; the knobs apply to the next note and fire a short preview while you drag.",
    "controls": "keybed: A-K whites, W-P blacks, Z/X octave, click/touch/MIDI · TAB or the tabs: pick a specimen · drag the four knobs (wheel = fine) · M: autoplay"
  },
  "todo": [
    "ear pass: which specimens earn an INSTR_* engine (then the knobs ride a held note live, and the 6 s render cap goes away)",
    "the other Plaits-Alt originals worth a page: spectral_spiral (frequency-shift feedback), undertow (subharmonics), lockstep (a PLL), tapfield, pulsar"
  ]
}
de:meta */
// specimens — six algorithmic synthesis engines from Plaits-Alt, each drawn by its own state.
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

enum { SP_FLOCK, SP_RULES, SP_SCAN, SP_GENDY, SP_ATTR, SP_BYTE, NSPEC };
static const char *SPNAME[NSPEC] = { "FLOCK", "RULES", "SCANNED", "GENDY", "ATTRACT", "BYTEBEAT" };
static const char *KLABEL[NSPEC][4] = {
    { "spread",  "couple", "2-flock", "lag"    },
    { "rule",    "edges",  "evolve",  "smooth" },
    { "strike",  "mass",   "damp+fold","physics"},
    { "points",  "amp walk","time walk","smooth"},
    { "lobes",   "chaos",  "x-y-z",   "skew"   },
    { "p0",      "p1",     "formula", "bits"   },
};
static const char *CAPTION[NSPEC] = {
    "seven detuned oscillators pulling each other into sync",
    "an elementary cellular automaton: its row is the wave",
    "a ring of 32 masses on springs, struck, read as a table",
    "Xenakis: random breakpoints taking a random walk each cycle",
    "a Thomas chaotic flow: three states chasing each other",
    "one-line integer formulas as an oscillator",
};
static float K[NSPEC][4] = {
    { 0.45f, 0.30f, 0.00f, 0.50f },
    { 0.12f, 0.30f, 0.30f, 0.60f },
    { 0.55f, 0.40f, 0.30f, 0.45f },
    { 0.40f, 0.35f, 0.25f, 0.80f },
    { 0.50f, 0.55f, 0.20f, 0.50f },
    { 0.40f, 0.35f, 0.10f, 0.50f },
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

// ── one render, any specimen ────────────────────────────────────────────────────
static float midi_hz(int m) { return 440.0f * de_powf(2.0f, (float)(m - 69) / 12.0f); }

static int render_spec(int sp, float *out, int n, int midi, Snap *sn, int *nsn) {
    float f = midi_hz(midi) / (float)SR;
    const float *k = K[sp];
    switch (sp) {
        case SP_FLOCK: render_flock(out, n, f, k, sn, nsn); break;
        case SP_RULES: render_rules(out, n, f, k, sn, nsn); break;
        case SP_SCAN:  render_scan(out, n, f, k, sn, nsn); break;
        case SP_GENDY: render_gendy(out, n, f, k, sn, nsn, 0x9e3779b9u ^ ((uint32_t)midi * 7919u)); break;
        case SP_ATTR:  render_attr(out, n, f, k, sn, nsn); break;
        default:       render_byte(out, n, f, k, sn, nsn); break;
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

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("SPECIMENS", 6, 3, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print_right(autoplay ? "M auto: on" : "M auto: off", SCREEN_W - 6, 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    for (int s = 0; s < NSPEC; s++) {
        int x = 4 + s * 52, y = 14, w = 50, h = 12;
        if (s == cur) rectfill(x - 1, y - 1, w + 2, h + 2, CLR_ORANGE);
        if (ui_button(x, y, w, h, SPNAME[s])) { set_spec(s); autoplay = false; }
    }

    // the picture, at the playback moment of the last note (or its last frame when it ended)
    const int px = 4, py = 30, pw = 220, ph = 104;
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
            default:       draw_byte(px, py, pw, ph, &snaps[idx]); break;
        }
        clip(0, 0, SCREEN_W, SCREEN_H);
    }
    font(FONT_TINY);
    print(CAPTION[cur], 6, 139, CLR_MEDIUM_GREY);
    if (snap_spec != cur) print("(the picture is the last note's; play one)", 6, 145, CLR_DARK_GREY);

    // four knobs, named for this specimen
    font(FONT_SMALL);
    for (int k = 0; k < 4; k++) {
        int kx = 246 + (k & 1) * 44, ky = 46 + (k >> 1) * 50;
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

    // the panel
    spec_tap(KEY_TAB);
    expect_eq(cur, SP_RULES, "TAB steps to the next specimen");
    spec_tap('M');
    expect(autoplay, "M turns the walk on");
}
#endif
