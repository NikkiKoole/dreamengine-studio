/* de:meta
{
  "title": "vapor fm",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "generative-melody"
  ],
  "lineage": "The first EFFECTS-FORWARD station - the FX chain (reverb+chorus+tape+echo+crush) IS the genre, applied to a generative lounge loop; novel in riding varispeed live as the slowed-tape WOBBLE. A slow metered/beatless cousin of citypop/ambient.",
  "homage": "Vaporwave (Macintosh Plus / Floral Shoppe, mallsoft)",
  "description": "Vaporwave - the first EFFECTS-FORWARD station, where THE PROCESSING IS THE GENRE. Vaporwave is 80s/90s lounge / mall-muzak SLOWED DOWN and drowned in reverb + chorus + tape warble + echo, looped hypnotically. With no sampler we GENERATE the lounge source - lush Rhodes + pad + a sax/bell dab + smooth bass over a short, static extended-jazz loop (maj9/m9/9) - and run the whole mix through that chain; the drench and the slow wobble ARE the band. THE DRENCH is a SET-AND-HOLD vaporwave FX chain (big reverb + chorus + heavy tape wow/flutter + echo + a touch of bitcrush), configured once per song/mode. THE SLOW WOBBLE is varispeed ridden LIVE (sweep-safe): a gentle drift so the whole mix wavers like a tape running slow / a dying battery - the slowed/screwed sag, made visible as the scene wobbles. A held vinyl-crackle bed under it all. The seed rolls a SUB-STYLE: classic (Floral-Shoppe lounge + a slow gated beat), mallsoft (beatless dead-mall, max reverb), utopian (brighter eco pads), future funk (a touch faster + funkier). The window is the iconic scene: a sunset gradient, the striped vaporwave sun, a wireframe grid floor receding to the horizon. SPACE next, R replay, [ ] history, LEFT/RIGHT haze (dry..drowned), UP/DOWN tempo, T tone, B band (keys rhodes/wurli, lead sax/soft, beat as-rolled/beatless), M power, H help. Pin via VAPOR_SEED."
}
de:meta */
// ── VAPOR FM — vaporwave ──────────────────────────────────────────────────────
// The first EFFECTS-FORWARD station: THE PROCESSING IS THE GENRE. Vaporwave is
// 80s/90s lounge / mall-muzak SLOWED DOWN and drowned in reverb + chorus + tape
// warble + echo, looped hypnotically. We have no sampler, so we GENERATE the lounge
// source (lush Rhodes + pad + a sax/bell dab over a static extended-jazz loop) and
// run the whole mix through that chain — the drench and the slow wobble ARE the band.
// Blind brief: docs/design/vaporwave-blind-brief.md.
//
// The brains (cart-side over radio.h's clock):
//   • THE DRENCH — a SET-AND-HOLD vaporwave FX chain (reverb + chorus + tape + echo +
//     a touch of bitcrush), configured once per song/mode (never per frame — that
//     churns the bus; copy groovebox's apply_fx gate).
//   • THE SLOW WOBBLE — the cassette wow/flutter in tape() + a draggy tempo: the slowed/
//     whole mix wavers like a tape running slow / a dying battery (the "screwed" sag).
//   • A HAZY-LOOP CHORD BRAIN — a short 2-4 chord lush-extended loop, static, repeated.
//   • Sub-style roll: CLASSIC / MALLSOFT (beatless) / UTOPIAN / FUTURE-FUNK.
//
// Window: the sunset + striped sun + wireframe grid, gently wobbling.
//
//   SPACE next   R replay   [ ] history   LEFT/RIGHT haze   UP/DOWN tempo   T tone   B band   M power

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define VAPOR_SEED 0

// ── slots ───────────────────────────────────────────────────────────────────
#define I_EP   5    // Rhodes lounge keys
#define I_PAD  6    // lush pad
#define I_LEAD 7    // smooth sax / soft lead dab
#define I_BELL 8    // chime sparkle (MALLET)
#define I_BASS 9    // smooth round bass
#define I_KICK 10
#define I_SNR  11
#define I_HAT  12
#define I_VINYL 13  // held vinyl-crackle / tape-hiss bed

// ── lush extended-jazz voicings (rootless 3rd / 7th / 9th-ish) ──────────────
enum { Q_MAJ9, Q_MIN9, Q_DOM9, Q_MAJ7, Q_MIN7, NQ };
static const char *QN[NQ] = { "maj9", "m9", "9", "maj7", "m7" };
static const int QV[NQ][3] = {
    { 4, 11, 14 }, { 3, 10, 14 }, { 4, 10, 14 }, { 4, 11, 7 }, { 3, 10, 7 },
};
static const int SCALES[2][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },    // major / lydian-ish (bright vapor)
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian (the dusky one)
};

// ── the hazy chord loops (off, quality) — static, dreamy, no cadence pull ───
typedef struct { int off, q; } Ch;
#define NLOOP 5
static const struct { int n; Ch c[4]; } LOOPS[NLOOP] = {
    { 2, { { 0, Q_MAJ9 }, { 9, Q_MIN9 } } },                                  // I - vi (dreamy)
    { 2, { { 0, Q_MAJ7 }, { 5, Q_MAJ7 } } },                                  // I - IV (float)
    { 4, { { 2, Q_MIN9 }, { 7, Q_DOM9 }, { 0, Q_MAJ9 }, { 0, Q_MAJ9 } } },    // ii-V-I (slowed)
    { 4, { { 9, Q_MIN9 }, { 5, Q_MAJ7 }, { 0, Q_MAJ9 }, { 7, Q_DOM9 } } },    // vi-IV-I-V (royal-road haze)
    { 4, { { 0, Q_MAJ9 }, { 2, Q_MIN9 }, { 4, Q_MIN7 }, { 5, Q_MAJ7 } } },    // I-ii-iii-IV (rising)
};

// ── sub-styles — each sets tempo, beat, the FX amounts and the wobble ───────
enum { M_CLASSIC, M_MALLSOFT, M_UTOPIAN, M_FUNK, NM };
typedef struct {
    const char *name; int tlo, tspan, beat;        // beat 0 none / 1 slow gated / 2 funk
    float revSize, wow, flutter, sat, chorusMix, echoFb; int echoMs, crushBits; float crushMix;
    float wobBase, wobDepth;
} ModeDef;
static const ModeDef MODE[NM] = {
    // tlo lowered (the "slowed" feel now comes from a genuinely draggy tempo, not held varispeed);
    // wow bumped (the cassette warble now lives in tape()'s wow/flutter). wbase unused; wdep = VISUAL sag only.
    //  name        tlo tsp bt  rev   wow   flut  sat   chor  efb  ems  cb  cmix  wbase  wdep
    { "classic",    70, 12, 1, 0.80f,0.36f,0.20f,0.40f,0.55f,0.30f,380, 12,0.10f, 1.0f, 0.018f },
    { "mallsoft",   58, 10, 0, 0.92f,0.44f,0.24f,0.46f,0.62f,0.44f,540, 11,0.15f, 1.0f, 0.028f },
    { "utopian",    78, 12, 1, 0.72f,0.26f,0.14f,0.28f,0.45f,0.22f,320, 14,0.04f, 1.0f, 0.012f },
    { "future funk",86, 14, 2, 0.60f,0.22f,0.12f,0.34f,0.42f,0.20f,280, 13,0.06f, 1.0f, 0.010f },
};

// ── the generated tune ───────────────────────────────────────────────────────
typedef struct {
    int mode, keyPc, scale, loop;
    int cellOn[5], cellDeg[5], cellN;     // a sparse, hazy melodic dab
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 160.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 78;
static int   hazeSel   = 2;        // 0..3 — the wet/drench dial
static int   toneSel   = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   songCount = 0;
static float vu = 0;
static int   gvEP[3] = { 60, 64, 67 }; static bool epInit = false;
static int   gvPad[3] = { 60, 64, 67 }; static bool padInit = false;
static int   melP = 76, bassLast = 40;
static int   vinylH = -1;
static char  nowChord[4][8];

static RadBand band;
static int chKeys, chLead, chBeat;

static int iabs(int v) { return v < 0 ? -v : v; }
static void apply_fx(void);
static void apply_chair(int idx);

// ── song generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Floral", "Plaza", "Mall", "Neon", "Velvet", "Sunset",
    "Pacific", "Crystal", "Lavish", "Astral", "Boyz", "Eternal" };
static const char *TW2[] = { "Shoppe", "Drift", "Dream", "Cafe", "Atrium", "Tears",
    "Lounge", "Memory", "Breeze", "Channel", "リサフランク", "420" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);
    sng.mode  = srnd(NM);
    sng.keyPc = srnd(12);
    sng.scale = (sng.mode == M_UTOPIAN) ? 0 : srnd(2);
    sng.loop  = srnd(NLOOP);
    // a sparse hazy dab — few notes, gentle contour
    sng.cellN = 0; int d = srnd(4);
    for (int s = 0; s < 28 && sng.cellN < 5; s += 4 + srnd(4))
        if (srnd(100) < 55) { sng.cellOn[sng.cellN] = s; sng.cellDeg[sng.cellN] = d;
                              d += srnd(5) - 2; if (d > 8) d -= 7; if (d < 0) d += 7; sng.cellN++; }
    if (sng.cellN < 2) { sng.cellN = 2; sng.cellOn[0] = 4; sng.cellDeg[0] = 0; sng.cellOn[1] = 16; sng.cellDeg[1] = 4; }
    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;
    tempo = MODE[sng.mode].tlo + srnd(MODE[sng.mode].tspan);
    bpm(tempo);
    apply_fx();                          // SET-AND-HOLD — once, on the song change
    songBase = (long)pos + 4;
    epInit = padInit = false; melP = 76; bassLast = 40;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── harmony ───────────────────────────────────────────────────────────────
static int  loop_len(void) { return LOOPS[sng.loop].n; }
static Ch   chord_at(long bar) { return LOOPS[sng.loop].c[(bar / 2) % loop_len()]; }   // 2 bars/chord (slow)
static int  root_pc(Ch c)  { return (sng.keyPc + c.off) % 12; }
static void chord_label(char *out, int n, Ch c) { snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]); }
static int  bass_peek(int pc, int lo, int hi) {
    int dd = ((pc - bassLast) % 12 + 18) % 12 - 6, m = bassLast + dd;
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}
static int deg_midi(int deg, int lo, int hi) {
    const int *sc = SCALES[sng.scale];
    int o = 5; while (deg < 0) { deg += 7; o--; } while (deg >= 7) { deg -= 7; o++; }
    int m = o * 12 + sng.keyPc + sc[deg];
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    int  cs   = (int)(s % 32);
    Ch   c    = chord_at(bar);
    int  rt   = root_pc(c);
    int  beat = (band.c[chBeat].sel == 1) ? 0 : MODE[sng.mode].beat;   // chair: force beatless

    // ── RHODES — lounge comp, soft and chorused; re-voiced on the chord change ──
    if (step == 0 || step == 8) {
        rad_lead_to(rt, QV[c.q], gvEP, 3, 54, 73, &epInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 9 + rnd(4), gvEP[k], I_EP, step == 0 ? 4 : 3, (int)(stepMs * 7));
        vu += 1.0f;
    }
    // ── PAD — a held wash, re-struck every 2 bars (the chord) ──
    if (step == 0 && bar % 2 == 0) {
        rad_lead_to(rt, QV[c.q], gvPad, 3, 50, 69, &padInit);
        for (int k = 0; k < 3; k++) schedule_hit(dly + k * 12, gvPad[k], I_PAD, 3, (int)(stepMs * 30));
        vu += 0.7f;
    }
    // ── BASS — smooth, slow root (+ a fifth mid-bar except mallsoft) ──
    if (step == 0) { int n = bass_peek(rt, 31, 45); bassLast = n;
        schedule_hit(dly + 4, n, I_BASS, 4, (int)(stepMs * 7)); vu += 0.8f; }
    else if (step == 8 && beat && bar % 2 == 0) schedule_hit(dly + 4, bass_peek((rt + 7) % 12, 31, 45), I_BASS, 3, (int)(stepMs * 3));

    // ── THE SLOW BEAT — gated, reverbed; mallsoft is beatless ──
    if (beat) {
        if (beat == 2) {                                       // future-funk: a touch busier
            if (step == 0 || step == 8) schedule_hit(dly, 36, I_KICK, 4, 90);
            if (step == 4 || step == 12) schedule_hit(dly + rnd(3), 60, I_SNR, 3, 70);
            if (step % 2 == 0) schedule_hit(dly + rnd(3), 90, I_HAT, step % 4 == 0 ? 2 : 1, 22);
        } else {                                               // classic/utopian: slow + sparse
            if (step == 0) schedule_hit(dly, 36, I_KICK, 4, 100);
            if (step == 8) schedule_hit(dly + rnd(3), 60, I_SNR, 3, 80);
            if (step == 4 || step == 12) schedule_hit(dly + rnd(3), 90, I_HAT, 1, 26);
        }
        vu += 0.5f;
    }

    // ── THE HAZY DAB — a sparse sax/soft lead line + a bell sparkle ──
    for (int i = 0; i < sng.cellN; i++)
        if (sng.cellOn[i] == cs && chance(70)) {
            int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - cs : 32 - cs;
            int dur = (int)(gap * stepMs * 0.8); if (dur > 2000) dur = 2000;
            schedule_hit(dly + 12 + rnd(8), deg_midi(sng.cellDeg[i], 66, 82), I_LEAD, 3, dur);
            if (chance(45)) schedule_hit(dly + 16, deg_midi(sng.cellDeg[i], 84, 95), I_BELL, 2, (int)(stepMs * 4));
            vu += 0.9f;
        }
}

// ── the vaporwave FX chain — SET-AND-HOLD (called from new_song + on knob change) ──
static void apply_fx(void) {
    const ModeDef *m = &MODE[sng.mode];
    float h = hazeSel / 3.0f;                       // 0..1 wet dial
    reverb(m->revSize + h * 0.08f, 0.40f);
    chorus(0.5f, 0.45f, m->chorusMix);
    tape(m->wow, m->flutter, m->sat);
    echo(m->echoMs, m->echoFb + h * 0.10f, 0.42f);
    if (m->crushMix > 0.02f) crush(m->crushBits, 7, m->crushMix);
    else                     crush(12, 7, 0);
}

// ── one-time setup ────────────────────────────────────────────────────────
static void setup_instruments(void) {
    chKeys = rad_chair(&band, "keys", "rhodes", "wurli", NULL, NULL);
    chLead = rad_chair(&band, "lead", "sax", "soft", NULL, NULL);
    chBeat = rad_chair(&band, "beat", "as-rolled", "beatless", NULL, NULL);

    instrument(I_EP, INSTR_EPIANO, 2, 0, 6, 1100);
    instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.32f); instrument_morph(I_EP, 0.25f);
    instrument_filter(I_EP, FILTER_LOW, 2000, 1);
    instrument_chorus(I_EP, 0.6f, 0.30f, 0.30f);
    instrument_pan(I_EP, -0.15f);

    instrument(I_PAD, INSTR_SAW, 400, 600, 6, 1500);
    instrument_filter(I_PAD, FILTER_LOW, 1500, 2);
    instrument_tune(I_PAD, 0.06f);

    instrument(I_LEAD, INSTR_REED, 2, 0, 4, 1200);          // smooth sax (chair can swap to soft sine)
    instrument_harmonics(I_LEAD, 0.80f); instrument_timbre(I_LEAD, 0.30f); instrument_morph(I_LEAD, 0.55f);
    instrument_filter(I_LEAD, FILTER_LOW, 2400, 1);
    instrument_pan(I_LEAD, 0.18f);

    instrument(I_BELL, INSTR_MALLET, 1, 0, 7, 800);
    instrument_harmonics(I_BELL, 0.55f); instrument_timbre(I_BELL, 0.40f); instrument_morph(I_BELL, 0.40f);

    instrument(I_BASS, INSTR_SINE, 4, 280, 5, 280);
    instrument_filter(I_BASS, FILTER_LOW, 600, 1);

    instrument(I_KICK, INSTR_SINE, 0, 130, 0, 70); instrument_filter(I_KICK, FILTER_LOW, 240, 2);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 42, 14);
    instrument(I_SNR, INSTR_NOISE, 1, 0, 0, 110); instrument_filter(I_SNR, FILTER_BAND, 1400, 4);
    instrument(I_HAT, INSTR_NOISE, 0, 22, 0, 16); instrument_filter(I_HAT, FILTER_HIGH, 6800, 3);

    // per-slot reverb sends (the wash); the big master reverb is set in apply_fx
    instrument_reverb(I_EP, 0.34f); instrument_reverb(I_PAD, 0.40f);
    instrument_reverb(I_LEAD, 0.44f); instrument_reverb(I_BELL, 0.48f);
    instrument_reverb(I_SNR, 0.30f); instrument_reverb(I_BASS, 0.08f);
    instrument_echo(I_LEAD, 0.18f); instrument_echo(I_BELL, 0.22f);

    // the vinyl-crackle / tape-hiss bed — a held, faint band-passed noise
    instrument(I_VINYL, INSTR_NOISE, 200, 400, 6, 600);
    instrument_filter(I_VINYL, FILTER_BAND, 3200, 2);

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chKeys) {
        if (sel == 0) { instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.32f); }   // Rhodes
        else          { instrument_harmonics(I_EP, 0.50f); instrument_timbre(I_EP, 0.42f); }   // Wurli
    } else if (idx == chLead) {
        if (sel == 0) { instrument(I_LEAD, INSTR_REED, 2, 0, 4, 1200);                          // sax
                        instrument_harmonics(I_LEAD, 0.80f); instrument_timbre(I_LEAD, 0.30f); instrument_morph(I_LEAD, 0.55f); }
        else          { instrument(I_LEAD, INSTR_SINE, 8, 200, 5, 700);                         // soft sine
                        instrument_lfo(I_LEAD, 0, LFO_PITCH, 4.8f, 0.07f); }
        instrument_filter(I_LEAD, FILTER_LOW, 2400, 1);
        instrument_reverb(I_LEAD, 0.44f); instrument_echo(I_LEAD, 0.18f);
    }
    // chBeat handled at play time (sel 1 = beatless override)
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_EP,  FILTER_LOW, (int)(2000 * tm), 1);
    instrument_filter(I_PAD, FILTER_LOW, (int)(1500 * tm), 2);
    instrument_filter(I_LEAD,FILTER_LOW, (int)(2400 * tm), 1);
}

// ── update — the live VARISPEED WOBBLE (sweep-safe) is the only per-frame FX ──
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (VAPOR_SEED) { new_song(pos, VAPOR_SEED); rad_hist_log(&rs); } else fresh_song(pos);
        scheduled = (long)pos; apply_tone();
        if (vinylH < 0) vinylH = note_on(60, I_VINYL, 1);     // the crackle bed, on forever
        booted = true;
    }

    int ev = rad_input(&tempo, 60, 110, 2, &hazeSel, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); sfx(-1); vinylH = -1; }
                              else { scheduled = (long)pos; apply_fx(); if (vinylH < 0) vinylH = note_on(60, I_VINYL, 1); } }
    if (ev & RAD_EV_TONE)   apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) apply_chair(chair);

    // HAZE (LEFT/RIGHT or the knob) re-applies the FX chain — GATED on change only
    // (set-and-hold: never reconfigure the bus per frame). The live wobble is separate.
    static int lastHaze = -1;
    if (hazeSel != lastHaze) { apply_fx(); lastHaze = hazeSel; }

    if (radioOn) {
        long st; while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        if (scheduled - songBase >= 64L * 16) fresh_song(pos);   // loop forever (re-roll every 64 bars)
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, LOOPS[sng.loop].c[i % loop_len()]);
    }
    // (the slowed/warbly feel now comes from the draggy tempo + tape()'s wow/flutter — NOT a
    //  held varispeed, which kept the resample ring engaged, drifted the timing, and eventually
    //  lapped its 2s buffer. varispeed is a sweep tool, not a hold.)

    vu *= 0.90f; if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase; long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("mode", "%s", MODE[sng.mode].name);
    watch("key", "%s", RAD_PCNAME[sng.keyPc]);
    watch("chord", "%s", nowChord[(int)((tbar / 2) % loop_len())]);
    watch("tempo", "%d", tempo);
    watch("haze", "%d", hazeSel);
#endif
}

// ── draw — the vaporwave scene: sunset, striped sun, wireframe grid, wobble ──
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  ACC = CLR_PINK;
    float t = timer();

    rad_body(CLR_DARK_PURPLE, ACC);
    rad_dial(sng.freq, ACC);

    // the window — a sunset scene that wobbles with the varispeed drift
    int wx = 34, wy = 52, ww = 102, wh = 116;
    int wob = radioOn ? (int)(MODE[sng.mode].wobDepth * 120.0f * sinf(t * 0.55f)) : 0;  // the tape sag, made visible
    int hz = wy + 74;                                                    // horizon
    // gradient sky (pink -> purple -> indigo), bands
    for (int y = wy; y < hz; y++) {
        int c = (y < wy + 24) ? CLR_INDIGO : (y < wy + 46) ? CLR_DARK_PURPLE : CLR_MAUVE;
        line(wx, y, wx + ww, y, c);
    }
    // the striped vaporwave sun
    int sx = wx + ww / 2 + wob, sy = hz - 4;
    for (int r = 26; r > 0; r--) {
        int yy = sy - r;
        if (yy < wy) continue;
        int band = (sy - yy);
        if (band > 8 && (band % 5 < 2)) continue;                        // horizontal slits
        int half = (int)sqrtf((float)(26 * 26 - r * r));
        line(sx - half, yy, sx + half, yy, (yy < sy - 14) ? CLR_PEACH : CLR_PINK);
    }
    // the wireframe grid floor (perspective: verticals to a vanishing point + scrolling rungs)
    rectfill(wx, hz, ww, wh - (hz - wy), CLR_DARKER_PURPLE);
    int vp = wx + ww / 2 + wob;
    for (int gx = -5; gx <= 5; gx++) {
        int bxp = wx + ww / 2 + gx * 12 + wob;
        line(vp, hz, bxp, wy + wh, CLR_MAUVE);
    }
    for (int i = 0; i < 7; i++) {                                        // rungs receding, scrolling up
        int yy = wy + wh - ((i * 9 + (frame() / 2) % 9));
        if (yy > hz) line(wx, yy, wx + ww, yy, CLR_MAUVE);
    }
    rect(wx, wy, ww, wh, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, ACC);
    if (radioOn) {
        print(sng.title, 154, 58, ACC);
        char l2[32]; snprintf(l2, 32, "%s  %s", MODE[sng.mode].name, RAD_PCNAME[sng.keyPc]);
        font(FONT_SMALL); print(l2, 154, 70, CLR_LIGHT_GREY); font(FONT_NORMAL);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 81, CLR_MAUVE);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, ACC);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ci = (int)((bar / 2) % loop_len()), x = 152;
        for (int i = 0; i < loop_len(); i++) {
            int cw = text_width(nowChord[i]); if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 104, cw + 4, 12, ACC); print(nowChord[i], x, 106, CLR_BLACK); }
            else print(nowChord[i], x, 106, CLR_MAUVE);
            x += cw + 8;
        }
        rad_phrase_dots(232, 124, 8, (bar / 8) % 8, ACC);
    }

    static const char *HAZE[4] = { "dry", "soft", "hazy", "drowned" };
    rad_knob_sel(&hazeSel, 4, 168, 148, 9, HAZE[hazeSel], ACC);
    if (rad_knob_int(&tempo, 60, 110, 2, 218, 148, 9, "tempo", ACC)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], ACC)) apply_tone();
    rad_power_led(radioOn, ACC, CLR_DARK_PURPLE);

    rad_help_button(ACC);
    rad_band_button(ACC);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "replay this one" },
            { "[ / ]",      "back / forward history" },
            { "LEFT/RIGHT", "haze - dry..drowned" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "band - keys/lead/beat" },
        };
        static const char *NOTES[3] = {
            "a lush lounge loop, DROWNED: reverb + chorus +",
            "tape + echo + crush, and a tape WOBBLE (wow + flutter)",
            "(the slowed-tape sag). classic/mallsoft/utopian/funk.",
        };
        rad_help_panel("VAPOR FM", HELP, 8, NOTES, 3, ACC);
    }
    rad_band_panel(&band, ACC);
    ui_end();
}
