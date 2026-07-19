// morphdrum.h — the shared MORPHING DRUM VOICE bank. The honest core behind the
// "supergroovebox" idea: an 808 kick and a 909 kick are NOT two engines — they are
// the SAME synthesis structure (sine body + pitch-env punch + lowpass + drive; snare
// = two sine modes + highpassed noise; hat = one bright ringing metal source) at
// DIFFERENT parameter values. So each of three voices is ONE parametric synth with a
// deep panel of knobs. CHARACTER (0=808 … 1=909) morphs the STRUCTURAL voicing (the
// bits without their own knob — click brightness, noise-mode shape, FM timbre floor);
// the rest of the knobs are ABSOLUTE synthesis controls whose ranges reach well PAST
// both machines (deep sub, endless boom, more grit — the electro→rock reach nobody's
// single box had). Both voicings, one header, chosen by data — the acid303.h move for
// drums, but with the whole synth exposed.
//
// NOT drumkit.h (a pad MAP) and NOT tr808.h/tr909.h (two FIXED faithful recipes). This
// is the parameter SPACE between and beyond them, turned into knobs.
//
// A cart owns its PATTERN + UI; this owns the SOUND. Params are floats 0..1 (ui_knob's
// range), indexed by MD_*; each voice reads the subset in MD_PANEL[] (the cart just
// iterates that to lay out its knobs).
//
//   #include "studio.h"
//   #include "morphdrum.h"
//   static MorphKit k;
//   void init(void)   { morph_build(&k, 9); }              // voice slots 9..9+MD_NSLOT-1
//   void update(void) { morph_ride(&k); }                  // rebuild only voices whose knobs moved
//   morph_fire(&k, MD_KICK, 0, 0);                         // (voice, velocity-boost, delay-ms)
//   // bind a knob: ui_knob(..., &k.p[MD_KICK][MD_DECAY]);
//
// SEAM — the hat: an 808 hat is a 6-square metal bank, a 909 hat is FM-clang. Both are
// one bright, highpassed, ringing metal source, so this models it as ONE FM voice and
// gives it timbre/morph knobs (not byte-equal to either machine's hat — the honest
// continuum). The other two voices share their oscillator structure across the pair.

#ifndef MORPHDRUM_H
#define MORPHDRUM_H
#include "studio.h"
#include <math.h>   // powf

// ── the three voices ─────────────────────────────────────────────────────────
enum { MD_KICK, MD_SNARE, MD_HAT, MD_NV };
static const char *MD_NAME[MD_NV] = { "KICK", "SNAR", "HAT" };

// ── the parameter superset (0..1). Each voice reads the subset in MD_PANEL[]. ──
enum { MD_CHAR, MD_TUNE, MD_DECAY, MD_PUNCH, MD_SNAP, MD_CLICK, MD_DRIVE,
       MD_CUT, MD_RES, MD_TONE, MD_ODEC, MD_SUB, MD_LEVEL, MD_NPARAM };

// per-voice knob panel: which params, in order, and the label THIS voice shows for it
// (a param can mean different things per voice — the label reflects that). 10 knobs =
// two rows of five.
typedef struct { unsigned char param; const char *label; } MDKnob;
#define MD_PANEL_N 10
static const MDKnob MD_PANEL[MD_NV][MD_PANEL_N] = {
    { {MD_CHAR,"CHAR"},{MD_TUNE,"TUNE"},{MD_DECAY,"DEC"}, {MD_PUNCH,"PNCH"},{MD_SNAP,"SNAP"},
      {MD_CLICK,"CLIK"},{MD_DRIVE,"DRV"},{MD_CUT,"CUT"},  {MD_SUB,"SUB"},  {MD_LEVEL,"LVL"} },   // KICK
    { {MD_CHAR,"CHAR"},{MD_TUNE,"TUNE"},{MD_DECAY,"DEC"}, {MD_PUNCH,"PNCH"},{MD_TONE,"BODY"},
      {MD_ODEC,"NDEC"},{MD_DRIVE,"DRV"},{MD_CUT,"CUT"},   {MD_SNAP,"SNAP"},{MD_LEVEL,"LVL"} },   // SNARE
    { {MD_CHAR,"CHAR"},{MD_TUNE,"TUNE"},{MD_DECAY,"DEC"}, {MD_ODEC,"OPEN"},{MD_TONE,"TONE"},
      {MD_SUB,"METL"}, {MD_CUT,"CUT"},  {MD_RES,"RES"},   {MD_DRIVE,"DRV"},{MD_LEVEL,"LVL"} },   // HAT
};

// ── instrument-slot layout: 7 slots (kick = body+click+sub, snare + hat = two each) ──
enum { MDS_KICK, MDS_KICKC, MDS_KICKS, MDS_SNB, MDS_SNN, MDS_HC, MDS_HO, MD_NSLOT };

typedef struct {
    int   base;
    float p[MD_NV][MD_NPARAM];          // knobs, 0..1
    int   guard[MD_NV];                 // last-applied hash → morph_ride rebuilds only on change
} MorphKit;

// sensible home defaults per voice (an 808-leaning kit that grooves out of the box)
static const float MD_DEF[MD_NV][MD_NPARAM] = {
    // CHAR TUNE DECAY PUNCH SNAP CLICK DRIVE CUT  RES  TONE ODEC SUB  LEVEL
    {  0.25f,0.36f,0.50f,0.54f,0.30f,0.18f,0.28f,0.34f,0.20f,0.50f,0.50f,0.35f,0.82f },  // KICK
    {  0.30f,0.38f,0.42f,0.30f,0.30f,0.00f,0.05f,0.45f,0.20f,0.55f,0.45f,0.00f,0.70f },  // SNARE
    {  0.55f,0.53f,0.22f,0.50f,0.50f,0.00f,0.00f,0.45f,0.30f,0.80f,0.50f,0.85f,0.65f },  // HAT
};

// ── absolute param → engine-unit mappings ────────────────────────────────────
#define MD_L(a, b, t)  ((a) + ((b) - (a)) * (t))
static int md_lin(float p, int lo, int hi) { int v = lo + (int)(p * (hi - lo) + 0.5f); return v; }
static int md_exp(float p, float lo, float oct) { return (int)(lo * powf(2.0f, p * oct)); }
static int md_vol(float p) { int v = (int)(p * 7.0f + 0.5f); return v < 0 ? 0 : v > 7 ? 7 : v; }

// resolved engine values (filled from the knobs; used by BOTH apply + fire so the
// instrument definition and the trigger can never diverge)
typedef struct {
    int   osc, midi, cut, res, filt, dec;
    int   swm; float swp;               // pitch env
    float drv, tone, timbre, morph, harm;
    int   l1_vol, l1_dec, l1_cut;       // layer 1 — click (kick) / noise (snare)
    int   l2_vol, l2_dec;               // layer 2 — sub (kick) / open hat
    int   vol;                          // main hit velocity from LEVEL
} MDRes;

static void md__resolve(const MorphKit *k, int v, MDRes *r) {
    const float *p = k->p[v];
    float ch = p[MD_CHAR];              // 0 = 808 … 1 = 909 (structural morph)
    r->osc = INSTR_SINE; r->filt = FILTER_LOW; r->res = 2; r->swm = 0; r->swp = 0;
    r->drv = 0; r->tone = p[MD_TONE]; r->timbre = r->morph = r->harm = 0;
    r->l1_vol = r->l1_dec = r->l1_cut = r->l2_vol = r->l2_dec = 0;
    r->vol = md_lin(p[MD_LEVEL], 2, 7);
    switch (v) {
    case MD_KICK:
        r->midi = md_lin(p[MD_TUNE], 19, 52);
        r->dec  = md_lin(p[MD_DECAY], 40, 1100);
        r->swp  = p[MD_PUNCH] * 48.0f;
        r->swm  = md_lin(p[MD_SNAP], 8, 150);
        r->drv  = p[MD_DRIVE];
        r->cut  = md_exp(p[MD_CUT], 55, 6.2f);              // ~55..3900 Hz
        r->l1_vol = md_vol(p[MD_CLICK]);                     // attack click
        r->l1_dec = 4;
        r->l1_cut = md_lin(ch, 1500, 3200);                 // CHAR: click brightness (dull 808 → sharp 909)
        r->l2_vol = md_vol(p[MD_SUB]);                       // sub, an octave down
        r->l2_dec = r->dec;
        break;
    case MD_SNARE:
        r->midi = md_lin(p[MD_TUNE], 46, 74);
        r->dec  = md_lin(p[MD_DECAY], 30, 320);
        r->swp  = p[MD_PUNCH] * 12.0f;
        r->swm  = md_lin(p[MD_SNAP], 8, 60);
        r->drv  = p[MD_DRIVE];
        r->cut  = md_exp(p[MD_CUT], 400, 4.0f);             // body lowpass 400..6400
        r->l1_dec = md_lin(p[MD_ODEC], 30, 420);            // noise decay
        r->l1_cut = md_lin(ch, 1800, 1400);                 // CHAR: noise mode (crisp 808 → fat 909)
        break;
    case MD_HAT:
        r->osc  = INSTR_FM; r->filt = FILTER_HIGH;
        r->midi = md_lin(p[MD_TUNE], 80, 110);
        r->dec  = md_lin(p[MD_DECAY], 10, 220);             // closed decay
        r->drv  = p[MD_DRIVE];
        r->cut  = md_exp(p[MD_CUT], 3000, 2.0f);            // 3000..12000
        r->res  = md_lin(p[MD_RES], 0, 12);
        r->timbre = p[MD_TONE];
        r->morph  = p[MD_SUB];                               // "METL"
        r->harm   = MD_L(0.40f, 0.70f, ch);                 // CHAR: metal floor (square-ish 808 → clang 909)
        r->l2_dec = md_lin(p[MD_ODEC], 80, 800);            // open decay
        break;
    }
}

// (re)define voice v's instrument slots from its knobs (morph_ride calls this on change)
static void morph_apply(MorphKit *k, int v) {
    int b = k->base; MDRes r; md__resolve(k, v, &r);
    switch (v) {
    case MD_KICK:
        instrument(b + MDS_KICK, INSTR_SINE, 0, r.dec, 0, 60);
        instrument_filter(b + MDS_KICK, FILTER_LOW, r.cut, 3);
        instrument_env(b + MDS_KICK, 0, ENV_PITCH, 0, r.swm, r.swp);
        instrument_drive(b + MDS_KICK, r.drv);
        instrument(b + MDS_KICKC, INSTR_NOISE, 0, r.l1_dec, 0, 4);
        instrument_filter(b + MDS_KICKC, FILTER_HIGH, r.l1_cut, 2);
        instrument(b + MDS_KICKS, INSTR_SINE, 0, r.l2_dec, 0, 60);   // sub, an octave down
        instrument_filter(b + MDS_KICKS, FILTER_LOW, 160, 1);
        break;
    case MD_SNARE:
        instrument(b + MDS_SNB, INSTR_SINE, 0, r.dec, 0, 30);
        instrument_filter(b + MDS_SNB, FILTER_LOW, r.cut, 1);
        instrument_env(b + MDS_SNB, 0, ENV_PITCH, 0, r.swm, r.swp);
        instrument_drive(b + MDS_SNB, r.drv);
        instrument(b + MDS_SNN, INSTR_NOISE, 0, r.l1_dec, 0, 40);
        instrument_filter(b + MDS_SNN, FILTER_HIGH, r.l1_cut, 2);
        break;
    case MD_HAT:
        instrument(b + MDS_HC, INSTR_FM, 0, r.dec, 0, 12);
        instrument_harmonics(b + MDS_HC, r.harm);
        instrument_timbre(b + MDS_HC, r.timbre);
        instrument_morph(b + MDS_HC, r.morph);
        instrument_filter(b + MDS_HC, FILTER_HIGH, r.cut, r.res);
        instrument_drive(b + MDS_HC, r.drv);
        instrument(b + MDS_HO, INSTR_FM, 0, r.l2_dec, 0, 90);
        instrument_harmonics(b + MDS_HO, r.harm);
        instrument_timbre(b + MDS_HO, r.timbre);
        instrument_morph(b + MDS_HO, r.morph);
        instrument_filter(b + MDS_HO, FILTER_HIGH, r.cut, r.res);
        instrument_drive(b + MDS_HO, r.drv);
        instrument_choke(b + MDS_HC, b + MDS_HO);            // closed chokes open
        break;
    }
}

static int md__hash(const float *p) {
    int h = 0; for (int i = 0; i < MD_NPARAM; i++) h = h * 131 + (int)(p[i] * 1000.0f + 0.5f);
    return h;
}

// build the whole kit at `base` (init): home defaults, define every voice
static void morph_build(MorphKit *k, int base) {
    k->base = base;
    for (int v = 0; v < MD_NV; v++) {
        for (int i = 0; i < MD_NPARAM; i++) k->p[v][i] = MD_DEF[v][i];
        morph_apply(k, v);
        k->guard[v] = md__hash(k->p[v]);
    }
}

// each frame: rebuild any voice whose knobs changed (set-and-hold — never every frame)
static void morph_ride(MorphKit *k) {
    for (int v = 0; v < MD_NV; v++) {
        int h = md__hash(k->p[v]);
        if (h != k->guard[v]) { morph_apply(k, v); k->guard[v] = h; }
    }
}

// fire voice v, `delay` ms from now, at extra velocity-boost `boost` (0 = the LEVEL knob's vol)
static void morph_fire(MorphKit *k, int v, int boost, int delay) {
    int b = k->base; MDRes r; md__resolve(k, v, &r);
    int vol = r.vol + boost; if (vol < 0) vol = 0; if (vol > 7) vol = 7;
    switch (v) {
    case MD_KICK:
        schedule_hit(delay, r.midi, b + MDS_KICK, vol, r.dec);
        if (r.l1_vol > 0) schedule_hit(delay, 60, b + MDS_KICKC, r.l1_vol, r.l1_dec);
        if (r.l2_vol > 0) schedule_hit(delay, r.midi - 12, b + MDS_KICKS, r.l2_vol, r.l2_dec);
        break;
    case MD_SNARE: {
        int body = (int)((1.0f - r.tone) * vol + 0.5f), snpy = (int)(r.tone * 7.0f + 0.5f);
        schedule_hit(delay, r.midi,      b + MDS_SNB, body, r.dec);
        schedule_hit(delay, r.midi + 10, b + MDS_SNB, body, r.dec);
        schedule_hit(delay, 60,          b + MDS_SNN, snpy, r.l1_dec);
        break;
    }
    case MD_HAT:   // the CLOSED hat; open hat = morph_fire_open()
        schedule_hit(delay, r.midi, b + MDS_HC, vol, r.dec);
        break;
    }
}

// fire the OPEN hat (the long variant — choked by the next closed hat)
static void morph_fire_open(MorphKit *k, int boost, int delay) {
    int b = k->base; MDRes r; md__resolve(k, MD_HAT, &r);
    int vol = r.vol + boost; if (vol < 0) vol = 0; if (vol > 7) vol = 7;
    schedule_hit(delay, r.midi, b + MDS_HO, vol, r.l2_dec);
}

#endif // MORPHDRUM_H
