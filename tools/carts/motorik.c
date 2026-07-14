/* de:meta
{
  "slug": "motorik",
  "collection": ["radio"],
  "title": "motorik radio",
  "status": "active",
  "created": "2026-06-08",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "schedule-driven-agents"
  ],
  "lineage": "Sixteenth radio station built on radio.h + improv.h, proving a 'process form' (no sections, one continuous 0..1 progress value) inspired by Neu! and Stereolab — first station to use INSTR_ORGAN and the first with a single-event key-modulation story beat.",
  "homage": "Neu! / Stereolab (krautrock / motorik)",
  "description": "Endless krautrock - the Neu! x Stereolab homage, station #16, and the first to prove THE PROCESS FORM: a radio with NO sections at all. The relentless motorik 4/4 (a live drummer quantized dead to the grid - kick on every quarter, understated backbeat, straight-8th hat, no fills ever) drives under a thin Farfisa drone (INSTR_ORGAN, the engine that unblocked this station) with maj7 chords PLANING on top (parallel voicings gliding over the drone - the only harmonic motion there is). A single progress value 0..1 runs the WHOLE song: every layer accumulates and then ebbs on its own seeded threshold, never on a bar boundary - so the loop has no seam, which makes it the best game-background music on the dial (nothing ever interrupts). THE MODULATION EVENT: one seeded key lift per song, around two-thirds through, slides the whole drone up under the topline - a story beat, not a grid position. The least performance-random station on the dial; the invariance IS the genre. Seeded per song: key, mode (lydian/major/mixolydian), length, the layer thresholds, the modulation, the pad's planing path, the topline cell, and the band roll (bright Farfisa night vs dark Vox night, round-saw vs buzzy-square Moog bass). The window is an autobahn at night - centerline dashes streaming toward you at the pulse, each layer that enters lighting the scene a little further; the horizon flares when the key lifts. SPACE next drive, R same drive, [ ] history, LEFT/RIGHT feel (idle/cruise/drive/redline - pulls the layers in sooner), UP/DOWN tempo, T tone, M power, H help."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "improv.h"  // THE IMPROVISER — the topline is a vibraphone taking a solo
#include <stdio.h>
#include <math.h>

// ── MOTORIK RADIO ───────────────────────────────────────────────────────────
// The krautrock driver: Neu! × Stereolab. The relentless 4/4 motorik pulse
// (Dinger/Liebezeit — a HUMAN drummer, quantized dead to the grid) under a thin
// Farfisa drone with maj7 chords planing on top. Sixteenth station, and the
// first to prove a new arrangement brain:
//
//   • THE PROCESS FORM — there are NO sections. No FORM[] array, no sect_of().
//     A single prog = songStep / (songLenBars*16) runs 0..1 over the WHOLE song,
//     and every layer accumulates (then ebbs) on a seeded prog threshold —
//     never on a bar boundary. House's "the ride is the form", with the
//     sections deleted. Because there's no seam, it's the best game-loop music
//     on the dial: nothing ever interrupts. (design/motorik.md)
//   • THE MODULATION EVENT — one seeded key lift, once per song, at prog≈0.6.
//     A story beat, not a grid position: the whole drone slides up under the
//     topline. Composition, not performance — a pinned seed lifts at the same
//     place every time.
//   • THE FARFISA DRONE — INSTR_ORGAN (the engine that unblocked this station),
//     a thin combo registration held forever; the maj7 PLANING pad glides
//     parallel maj7 voicings over it (the only harmonic motion there is).
//   • THE VIBRAPHONE SOLOIST — the topline is INSTR_MALLET running improv.h
//     (melody brain #2). The lone PERFORMED voice over the otherwise-invariant
//     machine — man vs. motor, Can soloing over the motorik bed. It's the
//     improviser's easiest gig ever: no chord changes, so it just walks the
//     mode over the drone, transposing with the modulation. New every listen.
//
// Machine-tight, no humanize, NO FILLS — the rhythm-section spine is the genre's
// invariance (the anti-dub: nothing in the BAND re-rolls). The one exception is
// the soloist on top, the deliberate human-over-machine voice. Tempo 132-150.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (pulls the layers in earlier/later)   UP/DOWN tempo
//   T tone   H or ? help
//
// Seed pins the COMPOSITION (key, mode, length, the layer thresholds, the
// modulation, the planing path, the lead cell, the band roll, title); the
// PERFORMANCE barely varies — that's the point.

#define MOTORIK_SEED 0   // pin a favourite drive here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_ORG   5   // Farfisa drone — INSTR_ORGAN, held
#define I_BASS  6   // Moog octave pulse
#define I_PAD   7   // maj7 planing pad — held, 3 voices
#define I_SOLO  8   // the topline — a VIBRAPHONE taking a solo (improv.h)
#define I_KICK  9
#define I_SNR  10
#define I_HAT  11

// ── modes — bright, Stereolab-leaning ─────────────────────────────────────
static const int MODESC[3][7] = {
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian     — the #4 brightness, the home key
    { 0, 2, 4, 5, 7, 9, 11 },   // ionian     — plain major
    { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian — the b7 lilt
};
static const char *MODENAME[3] = { "lydian", "major", "mixolyd" };
static const char *PCNAME[12]  = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// ── the layers — gated on prog, never on a bar boundary ────────────────────
enum { L_KICK, L_ORG, L_BASS, L_SNR, L_PAD, L_LEAD, NLAYER };
static const char *LNAME[NLAYER] = { "kik", "org", "bas", "snr", "pad", "vib" };
// base accumulation thresholds; new_song jitters the enters ±0.05
static const float ENTER0[NLAYER] = { 0.00f, 0.05f, 0.12f, 0.18f, 0.30f, 0.50f };
static const float LEAVE0[NLAYER] = { 1.00f, 0.96f, 0.90f, 0.92f, 0.82f, 0.80f };

#define NPLANE     8    // length of the pad's planing path
#define PLANE_BARS 2    // the pad planes to a new maj7 every 2 bars

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int   keyPc, mode;
    int   songLenBars;            // 48..96 — the form's denominator
    float enter[NLAYER], leave[NLAYER];
    double modAt;                 // 0.55..0.78 — where the key lifts
    int   modIv;                  // +2/+3/+5/+7 semitones
    int   planeDeg[NPLANE];       // the pad's planing path (scale degrees)
    int   leadOn[6], leadDeg[6], leadN;   // (retained: keeps the seed→song map stable — the
                                          //  topline is now the improviser; these srnd draws stay)
    int   bassWave;               // INSTR_SAW vs INSTR_SQUARE (rolled per song)
    float orgTimbre;              // Farfisa bright vs Vox dark (rolled per song)
    char  title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 107.0 };   // schedule-ahead step clock (radio.h)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))

static int    tempo     = 140;
static int    intensity = 1;     // feel: pulls the whole accumulation earlier/later
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;

// ── runtime form state ──────────────────────────────────────────────────────
static double prog     = 0;      // 0..1 over the whole song (display + gating + trace)
static bool   modDone  = false;  // the key lift has fired
static int    curKey   = 0;      // keyPc + the modulation, once it lands
static long   planePhrase = -1;
static int    planeIdx = 0;

// the topline soloist (improv.h) — the lone PERFORMED voice over the machine
static Improv solo;
static long   soloStartBar = -1;            // the bar the lead layer entered (-1 = solo idle)

// held-voice state
static int  orgH[2] = { -1, -1 };          // drone: root + fifth
static int  padH[3] = { -1, -1, -1 };      // maj7 planing
static bool orgLive = false, padLive = false;
static int  gvPad[3];                       // pad voicing (voice-led, glides)
static bool gvPadInit = false;

static int iabs(int v) { return v < 0 ? -v : v; }

// the feel knob shifts prog: a fuller feel pulls every layer in earlier
static float eprog(void) {
    float e = (float)prog + (intensity - 1) * 0.10f;
    return e < 0 ? 0 : e > 1 ? 1 : e;
}
static bool layer_on(int L) { float e = eprog(); return e >= sng.enter[L] && e < sng.leave[L]; }

// ── held voices: the Farfisa drone + the maj7 planing pad ─────────────────
static void repitch_drone(void) {
    if (orgH[0] >= 0) note_pitch(orgH[0], 48 + curKey % 12);
    if (orgH[1] >= 0) note_pitch(orgH[1], 48 + curKey % 12 + 7);
}
static void start_drone(void) {
    orgH[0] = note_on(48 + curKey % 12,     I_ORG, 5);
    orgH[1] = note_on(48 + curKey % 12 + 7, I_ORG, 4);
    note_glide(orgH[0], 220); note_glide(orgH[1], 220);   // the modulation slides
    orgLive = true;
}
static void stop_drone(void) {
    if (orgH[0] >= 0) note_off(orgH[0]);
    if (orgH[1] >= 0) note_off(orgH[1]);
    orgH[0] = orgH[1] = -1; orgLive = false;
}

static int plane_root(void) {
    int deg = sng.planeDeg[planeIdx % NPLANE];
    return (curKey + MODESC[sng.mode][deg % 7]) % 12;
}
// plane to the current degree's parallel maj7; nearest-tone so the voices glide
static void plane_pad(bool starting) {
    static const int MAJ7[3] = { 4, 7, 11 };   // the upper maj7 structure over the drone
    rad_lead_to(plane_root(), MAJ7, gvPad, 3, 55, 76, &gvPadInit);
    if (starting)
        for (int i = 0; i < 3; i++) { padH[i] = note_on(gvPad[i], I_PAD, 3); note_glide(padH[i], 400); }
    else
        for (int i = 0; i < 3; i++) if (padH[i] >= 0) note_pitch(padH[i], gvPad[i]);
}
static void start_pad(void) { gvPadInit = false; plane_pad(true); padLive = true; }
static void stop_pad(void) {
    for (int i = 0; i < 3; i++) if (padH[i] >= 0) note_off(padH[i]);
    padH[0] = padH[1] = padH[2] = -1; padLive = false;
}

// ── the tone knob (T cycles) — master brightness, re-aimed live ────────────
static void apply_voicing(void) {
    float m = RAD_TONEMUL[toneSel];
    instrument_filter(I_ORG,  FILTER_LOW, (int)(2200 * m), 2);
    instrument_filter(I_PAD,  FILTER_LOW, (int)(1800 * m), 2);
    instrument_filter(I_SOLO, FILTER_LOW, (int)(2600 * m), 2);
    instrument_filter(I_BASS, FILTER_LOW, (int)(800 * (0.75f + 0.25f * m)), 1);
}

// the per-song band roll (the rolled timbres) — organ registration + bass wave
static void apply_band(void) {
    instrument(I_ORG, INSTR_ORGAN, 70, 200, 6, 360);     // held combo organ — the drone
    instrument_harmonics(I_ORG, 0.19f);                  // thin combo registration (organ.c #1)
    instrument_timbre(I_ORG, sng.orgTimbre);             // bright Farfisa vs dark Vox (rolled)
    instrument_morph(I_ORG, 0.18f);                      // a touch of scanner chorus
    // THE PHASED FARFISA — the krautrock signature (Neu!/Cluster): a slow, deep phaser swept across
    // the held drone. Extreme in DEPTH/resonance (full depth, fb 0.6, 6 stages) but SLOW in rate
    // (~0.12 Hz = one sweep every ~8s) — the sweep evolves hypnotically over the motorik pulse
    // instead of whooshing against it. Baked into the drone's voice (the machine is invariant — no
    // knob); re-asserted each song after instrument() so the slot stays routed to its phaser bus.
    instrument_phaser(I_ORG, 0.12f, 1.0f, 0.6f, 0.5f, 6);

    instrument(I_BASS, sng.bassWave, 2, 90, 3, 100);     // Moog pulse — round saw vs buzzy square
}

// the static voices (never change per song)
static void setup_static(void) {
    instrument(I_PAD, INSTR_SAW, 360, 600, 5, 1100);     // string-machine canvas for the planing
    instrument_lfo(I_PAD, 0, LFO_PITCH, 0.18f, 0.05f);   // a slow tape wow

    instrument(I_SOLO, INSTR_MALLET, 1, 0, 7, 1100);     // struck bar — vibraphone, rings on its own
    instrument_harmonics(I_SOLO, 0.25f);                 // the vibes preset (lowend.c key 4):
    instrument_timbre(I_SOLO, 0.50f);                    //   bar material + mallet hardness
    instrument_morph(I_SOLO, 0.90f);                     //   morph's top = the motor tremolo

    instrument(I_KICK, INSTR_SINE, 0, 100, 0, 40);       // four-on-the-floor thud
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 12);

    instrument(I_SNR, INSTR_NOISE, 0, 70, 0, 45);        // understated backbeat
    instrument_filter(I_SNR, FILTER_BAND, 1800, 5);

    instrument(I_HAT, INSTR_NOISE, 0, 12, 0, 26);        // straight-8th tick
    instrument_filter(I_HAT, FILTER_HIGH, 8200, 2);
}

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Autobahn", "Neu", "Hallo", "Kosmische", "Motorik",
    "Cosmic", "Dusseldorf", "Chrome", "Velocity", "Solar", "Transient", "Kraftwerk" };
static const char *TW2[] = { "Drive", "Pulse", "Express", "Sequence", "Motorway",
    "Drift", "Engine", "Vector", "Continuum", "Overdrive", "Circuit", "Reprise" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

    sng.keyPc       = srnd(12);
    sng.mode        = srnd(3);
    sng.songLenBars = 48 + srnd(49);        // 48..96 bars — the form's length

    // the accumulation thresholds: base values, enters jittered ±0.05 per song
    for (int L = 0; L < NLAYER; L++) {
        float j = (srnd(11) - 5) * 0.01f;
        sng.enter[L] = ENTER0[L] + (L == L_KICK ? 0.0f : j);
        sng.leave[L] = LEAVE0[L];
    }

    sng.modAt = 0.55 + srnd(24) * 0.01;     // 0.55..0.78
    static const int IV[4] = { 2, 3, 5, 7 };
    sng.modIv = IV[srnd(4)];

    // the planing path: a degree walk, mostly small steps, returns home-ish
    sng.planeDeg[0] = 0;
    for (int i = 1; i < NPLANE; i++) {
        int step = (srnd(2) ? 1 : -1) * (1 + srnd(2));
        sng.planeDeg[i] = (((sng.planeDeg[i - 1] + step) % 7) + 7) % 7;
    }

    // the topline cell: sparse onsets on the 16-step grid
    sng.leadN = 0;
    for (int s = 0; s < 16 && sng.leadN < 6; s++)
        if (srnd(100) < 28) { sng.leadOn[sng.leadN] = s; sng.leadDeg[sng.leadN] = srnd(7); sng.leadN++; }
    if (sng.leadN < 3) {
        sng.leadN = 3;
        sng.leadOn[0] = 0;  sng.leadDeg[0] = 0;
        sng.leadOn[1] = 6;  sng.leadDeg[1] = 2;
        sng.leadOn[2] = 10; sng.leadDeg[2] = 4;
    }

    sng.bassWave  = srnd(2) ? INSTR_SAW : INSTR_SQUARE;
    sng.orgTimbre = srnd(2) ? 0.58f : 0.38f;            // bright Farfisa night vs dark Vox night

    if (srnd(100) < 55) snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    else                snprintf(sng.title, sizeof sng.title, "%s", TW1[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 132 + srnd(19);                             // 132..150
    bpm(tempo);

    songBase    = (long)pos + 8;
    modDone     = false;
    curKey      = sng.keyPc;
    planeIdx    = 0;
    planePhrase = -1;
    soloStartBar = -1;
    gvPadInit   = false;
    stop_drone();                                       // re-init the held voices for the new song
    stop_pad();
    apply_band();                                       // the rolled timbres
    apply_voicing();                                    // re-aim the filters over them
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── the step player — the rhythmic layers (drone + pad are held, above) ────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= sng.songLenBars) return;

    // the pad planes to a new maj7 on the bar grid (held voices, glided)
    if (step == 0 && bar % PLANE_BARS == 0 && bar / PLANE_BARS != planePhrase) {
        planePhrase = bar / PLANE_BARS;
        planeIdx++;
        if (padLive) plane_pad(false);
    }

    // KICK — four on the floor, every quarter. The pulse, always first in.
    if (layer_on(L_KICK) && step % 4 == 0) { schedule_hit(dly, 36, I_KICK, 6, 90); vu += 2; }
    // HAT — straight 8ths, dead even (no swing, no open hat, no fills)
    if (layer_on(L_KICK) && step % 2 == 0)
        schedule_hit(dly, 88, I_HAT, step % 4 == 0 ? 1 : 2, 16);
    // SNARE — understated backbeat on 2 & 4
    if (layer_on(L_SNR) && (step == 4 || step == 12)) { schedule_hit(dly, 60, I_SNR, 4, 55); vu += 1.5f; }

    // BASS — the Moog octave pulse, straight 8ths on the (modulated) root
    if (layer_on(L_BASS) && step % 2 == 0) {
        int n = 36 + curKey % 12 + (step == 8 ? 12 : 0);     // an octave lift mid-bar
        schedule_hit(dly, n, I_BASS, 5, (int)(stepMs * 1.7));
        vu += 1.2f;
    }

    // SOLO — the vibraphone improviser (improv.h). The lone PERFORMED voice over
    // the machine: pure engine rnd(), new every listen, while everything else is
    // seeded. Motorik is its easiest bed — no chord changes, so it just walks the
    // mode over the drone; it transposes with the modulation (curKey). The drone
    // grounds it, so the solo can float freely above.
    if (layer_on(L_LEAD)) {
        if (soloStartBar < 0) {                          // the solo begins as the topline enters
            soloStartBar = bar;
            int lb = (int)((sng.leave[L_LEAD] - sng.enter[L_LEAD]) * sng.songLenBars);
            if (lb < 8) lb = 8;
            improv_begin(&solo, 74, lb, 0.9f);           // vibes register, the soloing span, near-lead density
            improv_render(&solo, 0, MODESC[sng.mode]);   // populate the first 2-bar window now
        }
        long soloBar = bar - soloStartBar;
        int  cs = (int)((soloBar % 2) * 16 + step);      // 0..31 within the solo's 2-bar window
        if (step == 0 && soloBar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, MODESC[sng.mode]);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int m   = improv_midi(&solo, i, soloBar, curKey, MODESC[sng.mode], 0);  // bluePct 0 — clean modal
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 3) * 0.9);
                schedule_hit(dly, m, I_SOLO, 4, gat);
                vu += 1.0f;
            }
    } else {
        soloStartBar = -1;                               // lead layer out — re-arm for next entry
    }
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_static();
        if (MOTORIK_SEED) { new_song(pos, MOTORIK_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 132, 150, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TONE)   apply_voicing();
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); stop_drone(); stop_pad(); }
        else scheduled = (long)pos;
    }

    if (radioOn) {
        long songStep = scheduled - songBase;
        prog = songStep > 0 ? (double)songStep / (sng.songLenBars * 16) : 0;
        if (prog > 1) prog = 1;

        // THE MODULATION EVENT — fires once, the whole drone lifts under the topline
        if (!modDone && eprog() >= sng.modAt) {
            modDone = true;
            curKey  = (curKey + sng.modIv) % 12;
            if (orgLive) repitch_drone();
            if (padLive) plane_pad(false);
        }

        // accumulation/subtraction of the held layers (continuous, no bar seam)
        bool oW = layer_on(L_ORG); if (oW && !orgLive) start_drone(); else if (!oW && orgLive) stop_drone();
        bool pW = layer_on(L_PAD); if (pW && !padLive) start_pad();   else if (!pW && padLive) stop_pad();

        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        if (songStep >= (long)sng.songLenBars * 16) fresh_song(pos);
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    watch("song", "%d", songCount);
    watch("prog", "%.3f", prog);
    watch("layers", "%d%d%d%d%d%d",
          layer_on(L_KICK), layer_on(L_ORG), layer_on(L_BASS),
          layer_on(L_SNR), layer_on(L_PAD), layer_on(L_LEAD));
    watch("key", "%d", curKey);
    watch("modDone", "%d", modDone);
    watch("plane", "%d", sng.planeDeg[planeIdx % NPLANE]);
#endif
}

// ── draw — the autobahn at night (shared chassis from radio.h; the road,
// the streaming centreline, and the layer scenery are motorik's own) ───────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();                                         // knobs are touch-draggable

    rad_body(CLR_DARKER_BLUE, CLR_ORANGE);              // night chrome, sodium-light accent
    rad_dial(sng.freq, CLR_ORANGE);

    // ── the window: the autobahn vanishing to a night horizon ─────────────
    int wx = 34, wy = 52, ww = 102, wh = 116;
    int vx = wx + ww / 2, vy = wy + 8, by = wy + wh;    // vanishing point + road base
    rectfill(wx, wy, ww, wh, CLR_DARKER_PURPLE);        // night sky
    rectfill(wx, vy, ww, by - vy, CLR_BLACK);           // the road's dark ground

    // org in → a sodium glow on the horizon
    if (radioOn && layer_on(L_ORG))
        rectfill(wx, vy - 2, ww, 4, modDone ? CLR_PINK : CLR_DARK_RED);
    // pad in → a skyline silhouette band on the horizon
    if (radioOn && layer_on(L_PAD))
        for (int x = wx; x < wx + ww; x += 6)
            rectfill(x, vy - 5 - ((x * 7) % 5), 4, 6, CLR_DARK_PURPLE);
    // lead in → stars over the road
    if (radioOn && layer_on(L_LEAD))
        for (int k = 0; k < 18; k++) {
            int sx = wx + (k * 37 + 5) % ww, sy = wy + (k * 19 + 3) % 22;
            pset(sx, sy, (k % 3) ? CLR_DARK_GREY : CLR_LIGHT_GREY);
        }

    // road edges converging to the vanishing point
    line(wx + 6, by, vx - 1, vy, CLR_DARK_GREY);
    line(wx + ww - 6, by, vx + 1, vy, CLR_DARK_GREY);

    // the streaming centreline — dashes rush toward the viewer at the pulse rate
    if (radioOn) {
        float flow = fmodf(frame() * tempo / 620.0f, 1.0f);
        for (int i = 0; i < 7; i++) {
            float u  = fmodf(i / 7.0f + flow, 1.0f);    // 0 (far) .. 1 (near)
            int   dy = vy + (int)(u * u * (by - vy));   // perspective accel
            int   dw = 1 + (int)(u * 4);
            int   dh = 2 + (int)(u * 6);
            if (dy + dh <= by) rectfill(vx - dw / 2, dy, dw, dh, CLR_ORANGE);
        }
        // bass in → two taillights pulling away up the road
        if (layer_on(L_BASS)) {
            float tu = 0.45f + 0.10f * sinf(timer() * 0.6f);
            int ty = vy + (int)(tu * tu * (by - vy));
            int sp = 2 + (int)(tu * 6);
            pset(vx - sp, ty, CLR_RED); pset(vx + sp, ty, CLR_RED);
        }
    }
    rect(wx, wy, ww, wh, CLR_DARK_GREY);

    // ── the display ────────────────────────────────────────────────────────
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, MODENAME[sng.mode]);
        print(l2, 154, 70, CLR_YELLOW);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_YELLOW);
        // the process-form progress bar (no sections — one unbroken sweep)
        rectfill(154, 91, 130, 2, CLR_DARKER_GREY);
        rectfill(154, 91, (int)(prog * 130), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the layer stack readout + the modulation flag
    if (radioOn) {
        for (int L = 0; L < NLAYER; L++) {
            int lx = 152 + L * 23;
            bool on = layer_on(L);
            print(LNAME[L], lx, 104, on ? CLR_ORANGE : CLR_DARKER_GREY);
            rectfill(lx + 2, 100, 3, 3, on ? CLR_YELLOW : CLR_DARKER_GREY);
        }
        char kl[20];
        if (modDone) snprintf(kl, 20, "key %s +%d", PCNAME[curKey % 12], sng.modIv);
        else         snprintf(kl, 20, "key %s", PCNAME[curKey % 12]);
        print(kl, 152, 116, modDone ? CLR_PINK : CLR_DARK_GREY);
    }

    // ── knobs + power LED ───────────────────────────────────────────────────
    static const char *FEEL[4] = { "idle", "cruise", "drive", "redline" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 132, 150, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) apply_voicing();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_RED);

    rad_help_button(CLR_ORANGE);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next drive (rolls a new seed)" },
            { "R",          "same drive again" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - pulls the layers in sooner" },
            { "UP/DOWN",    "tempo of the motor" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "no sections - the band assembles itself",
            "across the whole drive; one key lifts once.",
            "pin it: #define MOTORIK_SEED 0x...",
        };
        rad_help_panel("MOTORIK RADIO", HELP, 8, NOTES, 3, CLR_ORANGE);
    }

    ui_end();
}
