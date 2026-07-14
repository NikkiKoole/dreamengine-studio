/* de:meta
{
  "slug": "eno",
  "collection": ["radio"],
  "title": "eno radio",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody"
  ],
  "lineage": "A direct algorithmic recreation of Brian Eno's Ambient 1: Music for Airports (1978), using mutually-coprime loop lengths to produce emergent phase drift — the first radio station with no chord progression and the first to use INSTR_PIANO and a sustained INSTR_VOICE choir.",
  "homage": "Brian Eno - Ambient 1: Music for Airports (1978)",
  "description": "A beatless ambient station after Brian Eno's Music for Airports (1978). Each voice is a LOOP of its own length in seconds, the lengths mutually coprime, so they drift in and out of phase and never line up the same way twice - and there is NO chord progression: each loop just holds one note of a rolled pitch-set, so the harmony is whatever loops happen to coincide (emergent, not authored). The window shows it: one lane per loop, a playhead creeping at the loop's own speed, a bloom where two playheads meet - you watch the chord form. Not-samey by design: the album is four SETUPS and the seed rolls one per song - 1/1 piano (real INSTR_PIANO struck), 2/1 voices (sustained INSTR_VOICE choir), 1/2 duet, 2/2 synth drone - plus key + mode (lush, never dark), register, a fresh coprime period set, per-voice detune (ombak), and timbre macros; each loop also waxes/wanes over ~90s. SPACE/tune next, R same seed, [ ] history, drift/tone/density knobs, B picks the movement, M power, H help. Pin a favourite via ENO_SEED. First radio station to use real INSTR_PIANO and a sustained INSTR_VOICE choir, and the first with no chord brain at all."
}
de:meta */
#include "studio.h"
#include "radio.h"   // shared station chassis — eno takes the PRNG + history + chrome +
                     // the tuner sweep, and (like ambient) skips the step clock: it's beatless
#include <stdio.h>
#include <math.h>

// ── ENO RADIO — "Music for Airports" ───────────────────────────────────────
// A beatless ambient station after Brian Eno's *Ambient 1: Music for Airports*
// (1978). The mechanism is the record's own: each voice is a LOOP of its own
// length in SECONDS, the lengths mutually coprime, so they drift in and out of
// phase and never line up the same way twice. There is NO chord progression —
// each loop just HOLDS one note of a rolled pitch-set, and the harmony is
// whatever loops happen to coincide (emergent, not authored). The window shows
// it: one lane per loop, a playhead creeping across at the loop's own speed,
// and a bloom where two playheads meet — you watch the chord form.
//
// Not-samey by design — the album is four different SETUPS, and the seed rolls
// one per song (it's the big variety lever; the B panel overrides it live):
//   1/1  PIANO  — struck piano (real INSTR_PIANO) over a faint pad bed
//   2/1  VOICES — pure sung-vowel choir (INSTR_VOICE), the purest phase piece
//   1/2  DUET   — voices and piano interleaved
//   2/2  SYNTH  — a slow morphing pad drone only
// On top of that every seed rolls key + mode (lush, never dark), register
// (low-warm vs high-shimmer), a fresh coprime period set, the per-voice detune
// (ombak beating) and the timbre macros — and within a song each loop slowly
// waxes and wanes over ~90s, so it never sits still.
//
//   SPACE / tune  next song    R same seed    [ ] history    M power
//   drift knob  glacial<->flowing    tone knob  mellow..bright    density knob
//   B  the band (movement override)    H or ?  help
//
// First radio station to use real INSTR_PIANO and a sustained INSTR_VOICE
// choir, and the first with no chord brain at all. Blind brief +
// palette shop: docs/design/eno-blind-brief.md.

#define ENO_SEED 0          // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_PIANO 5   // INSTR_PIANO  — struck, rings down on its own (1/1, duet)
#define I_VOICE 6   // INSTR_VOICE  — sung vowel, long swell, holds (2/1, duet)
#define I_PAD   7   // INSTR_SAW    — warm drone bed / synth pad
#define I_PAD2  8   // INSTR_PD     — morphing pad partner (the 2/2 drone)

// ── modes — lush, never dark; the whole song lives inside one ─────────────
static const int MODESC[4][7] = {
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian  — weightless
    { 0, 2, 4, 5, 7, 9, 11 },   // major   — open, bright
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian  — warm, dusky
    { 0, 2, 3, 5, 7, 8, 10 },   // aeolian — far away
};
static const char *MODENAME[4] = { "lydian", "major", "dorian", "aeolian" };
static const char *PCNAME[12]  = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// the setups (the four pieces) — rolled per song, the big variety lever
enum { SET_PIANO, SET_VOICES, SET_DUET, SET_SYNTH };
static const char *SETSHORT[4] = { "1/1", "2/1", "1/2", "2/2" };
static const char *SETLONG[4]  = { "piano", "voices", "duet", "synth" };

#define NLOOP 8
// a stacked ladder of in-mode tones (tonic/3/5/7/9/11/13/octave) — keeps every
// possible coincidence consonant; the variety comes from key/mode/register, not
// from random degrees (random degrees go muddy fast in a beatless wash)
static const int DEGLAD[NLOOP] = { 0, 2, 4, 6, 7, 9, 11, 14 };
// base loop lengths in seconds — all distinct, pairwise non-harmonic (Eno's tape
// loops ran ~17-31s). Per song each is jittered, so phase relations differ.
static const float BASEPER[NLOOP] =
    { 12.4f, 14.9f, 17.1f, 19.8f, 21.7f, 23.9f, 26.3f, 28.7f };
// calm lane colours for the visualiser (no lavender — that constant doesn't exist)
static const int LANECOL[NLOOP] = {
    CLR_BLUE, CLR_INDIGO, CLR_TRUE_BLUE, CLR_MEDIUM_GREEN,
    CLR_PINK, CLR_LIGHT_GREY, CLR_ORANGE, CLR_LIGHT_YELLOW };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int   keyPc, mode, setup;
    int   baseMidi;          // register centre (low-warm .. high-shimmer)
    int   nLoop;             // how many loops this song wants (5..8)
    int   deg[NLOOP];        // scale-degree ladder per loop
    float per[NLOOP];        // base period (seconds) per loop
    float pVoicing;          // piano voicing macro (grand .. celesta)
    float vVowel;            // voice vowel macro (U->O->A->E->I)
    float padMorph;          // pad morph (the PD wowww sweep)
    float detDepth;          // per-voice detune spread (the ombak beating)
    char  title[24];
    float freq;              // FM dial position
    unsigned seed;
} Song;

static Song   song;
static int    toneSel  = 2;      // 0..3 mellow/warm/clear/bright (RAD_TONE*)
static float  drift    = 0.5f;   // glacial(0) .. flowing(1) — scales every period
static float  density  = 0.72f;  // how many loops are live
static bool   radioOn  = true;
static bool   showHelp = false;
static int    songCount = 0;
static RadBand band;
static int     CH_MOVE;          // the band-panel chair index (the movement override)

// composition PRNG + history live in radio.h — same byte-identical contract as
// the other stations; performance colour keeps engine rnd(). Beatless: no clock.
static RadioSeed rs;
#define srnd(n) rad_srnd(&rs, (n))

// per-loop runtime (handles + the wall-clock loop phase) — NOT seeded
static int   lpH[NLOOP];         // live note handle, or -1
static float lpNextOn[NLOOP];    // wall time the loop next (re)sounds
static float lpOffAt[NLOOP];     // wall time to release it
static float lpDet[NLOOP];       // performance detune (semitones)
static float lpPres[NLOOP];      // phase of the slow ~90s presence wax/wane
static float lpPos[NLOOP];       // 0..1 position within the current cycle (viz)
static bool  lpSounding[NLOOP];  // is it ringing right now (viz)

static int scaleAt(int mode, int deg) { return MODESC[mode][deg % 7] + 12 * (deg / 7); }

static int active_loops(void) {
    int a = 3 + (int)(density * (song.nLoop - 3) + 0.5f);
    return a < 3 ? 3 : a > song.nLoop ? song.nLoop : a;
}
static float drift_scale(void) { return 1.70f - drift * 1.15f; }   // 1.70x .. 0.55x

// which engine a given loop plays, given the setup
static int loop_instr(int setup, int i) {
    switch (setup) {
        case SET_PIANO:  return (i < 2) ? I_PAD : I_PIANO;     // 2 quiet bed pads + piano
        case SET_VOICES: return I_VOICE;
        case SET_DUET:   return (i & 1) ? I_PIANO : I_VOICE;
        default:         return (i & 1) ? I_PAD2 : I_PAD;      // SET_SYNTH
    }
}
// fraction of the loop the note actually sounds (the rest is silence — the gap
// that makes the phase audible). Piano is struck: a short gate, the ring is the
// engine's long release.
static float loop_gate(int setup, int instr) {
    if (instr == I_PIANO)    return 0.12f;
    if (setup == SET_SYNTH)  return 0.97f;   // near-continuous -> a drone
    if (instr == I_VOICE)    return 0.62f;   // vowel, then a breath
    return 0.90f;                            // pad bed
}
static int loop_vol(int setup, int instr) {
    if (instr == I_PIANO) return 4;
    if (instr == I_VOICE) return 4;
    if (setup == SET_SYNTH) return 3;
    return 2;                                // the faint bed under the piano
}

// ── instruments (once) ──────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_PIANO, INSTR_PIANO, 4, 2800, 0, 1400);   // struck, rings ~3s
    instrument(I_VOICE, INSTR_VOICE, 2400, 0, 7, 1400);   // long swell, holds, long tail
    instrument(I_PAD,   INSTR_SAW,   1800, 600, 7, 2600); // warm bed / pad
    instrument(I_PAD2,  INSTR_PD,    2200, 600, 7, 2800); // morphing pad
    instrument_reverb(I_PIANO, 0.5f);
    instrument_reverb(I_VOICE, 0.6f);
    instrument_reverb(I_PAD,   0.5f);
    instrument_reverb(I_PAD2,  0.5f);
    reverb(0.92f, 0.35f);                                 // a big, slow bloom
}

// per-song / per-tone timbre macros (shared per slot — all loops on a slot agree)
static void apply_timbres(void) {
    float tb = RAD_TONEMUL[toneSel];                      // 0.55 .. 1.28
    // piano — voicing rolled (grand or glassy celesta), hammer brightens with tone
    instrument_harmonics(I_PIANO, song.pVoicing);
    instrument_timbre(I_PIANO, clamp(0.18f * tb, 0, 1));
    instrument_morph(I_PIANO, 0.85f);                     // open pedal, long sustain
    instrument_filter(I_PIANO, FILTER_LOW, (int)(2600 * tb), 1);
    // voice — vowel rolled, effort (brightness) tracks tone
    instrument_harmonics(I_VOICE, song.vVowel);
    instrument_timbre(I_VOICE, 0.45f);
    instrument_morph(I_VOICE, clamp(0.20f + 0.45f * (tb - 0.55f), 0, 1));
    instrument_filter(I_VOICE, FILTER_LOW, (int)(2400 * tb), 1);
    // pads — saw bed + morphing PD
    instrument_morph(I_PAD2, song.padMorph);
    instrument_filter(I_PAD,  FILTER_LOW, (int)(650 * tb), 2);
    instrument_filter(I_PAD2, FILTER_LOW, (int)(760 * tb), 2);
}

// ── the loops ────────────────────────────────────────────────────────────────
static void all_off(void) {
    for (int i = 0; i < NLOOP; i++)
        if (lpH[i] >= 0) { note_off(lpH[i]); lpH[i] = -1; }
}

// (re)stagger every loop's first entry so the voices arrive over ~20s rather
// than all at once — the "voices fade in" opening. Performance rnd, not seeded.
static void reinit_voices(float T) {
    all_off();
    for (int i = 0; i < NLOOP; i++) {
        lpNextOn[i]  = T + i * 2.4f + rnd_float_between(0, 1.6f);
        lpOffAt[i]   = 0;
        lpDet[i]     = rnd_float_between(-song.detDepth, song.detDepth);
        lpPres[i]    = rnd_float_between(0, 6.2831f);
        lpPos[i]     = 0;
        lpSounding[i] = false;
    }
}

static void trigger_loop(float T, int i) {
    int instr = loop_instr(song.setup, i);
    int midi  = song.baseMidi + song.keyPc + scaleAt(song.mode, song.deg[i]);
    midi = midi < 28 ? 28 : midi > 96 ? 96 : midi;
    // slow ~90s wax/wane so the texture breathes across minutes (performance)
    float pres = 0.45f + 0.55f * (0.5f + 0.5f * sinf(T * 0.012f + lpPres[i]));
    int vol = (int)(loop_vol(song.setup, instr) * pres + 0.5f);
    if (vol < 1) vol = 1;
    lpH[i] = note_on(midi, instr, vol);
    note_pitch(lpH[i], midi + lpDet[i]);                  // the ombak detune
    note_reverb(lpH[i], instr == I_PIANO ? 0.5f : 0.62f);
    if (instr != I_PIANO) {                               // held voices breathe + wow
        note_glide(lpH[i], 1200);
        note_lfo(lpH[i], 0, LFO_PITCH,  0.05f + 0.03f * i / NLOOP, 0.04f);  // tape wow
        note_lfo(lpH[i], 1, LFO_CUTOFF, 0.03f + 0.02f * i / NLOOP, 280);    // breathing
        if (instr == I_VOICE) voice_nasal(lpH[i], 0.10f);
    }
}

static void service_loops(float T) {
    int active = active_loops();
    float ds = drift_scale();
    for (int i = 0; i < NLOOP; i++) {
        if (i >= active) {                                // density turned this one off
            if (lpH[i] >= 0) { note_off(lpH[i]); lpH[i] = -1; }
            lpSounding[i] = false;
            continue;
        }
        float eff = song.per[i] * ds;
        if (T >= lpNextOn[i]) {
            if (lpH[i] >= 0) note_off(lpH[i]);            // safety: never stack
            trigger_loop(T, i);
            lpOffAt[i]  = T + eff * loop_gate(song.setup, loop_instr(song.setup, i));
            lpNextOn[i] += eff;
            if (lpNextOn[i] <= T) lpNextOn[i] = T + eff;  // drift change can lap it
        }
        if (lpH[i] >= 0 && T >= lpOffAt[i]) { note_off(lpH[i]); lpH[i] = -1; }
        lpPos[i]      = 1.0f - (lpNextOn[i] - T) / eff;   // 0..1 through the cycle
        if (lpPos[i] < 0) lpPos[i] = 0; if (lpPos[i] > 1) lpPos[i] = 1;
        lpSounding[i] = (lpH[i] >= 0);
    }
}

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Airports","Clouds","Glass","Pale","Slow","Apollo",
    "Discreet","Plateaux","Neroli","Becalmed","Snowfield","Lux","Thursday","Vapour",
    "On Land","Halcyon" };
static const char *TW2[] = { "Variation","Reprise","at Dawn","in Blue","Above",
    "Adrift","II","Asleep" };

static void new_song(float T, unsigned seed) {
    song.seed = rad_seed_begin(&rs, seed);

    song.keyPc   = srnd(12);
    song.mode    = srnd(4);
    song.setup   = srnd(4);
    song.baseMidi = 46 + srnd(3) * 5;            // 46 / 51 / 56 — warm .. shimmer
    song.nLoop   = 5 + srnd(4);                  // 5..8
    for (int i = 0; i < NLOOP; i++) {
        song.deg[i] = DEGLAD[i];
        song.per[i] = BASEPER[i] * (0.86f + srnd(36) / 100.0f);   // 0.86..1.21x jitter
    }
    // timbre rolls — grand piano most songs, glassy celesta sometimes
    song.pVoicing = srnd(3) ? 0.04f + srnd(20) / 100.0f : 0.86f + srnd(12) / 100.0f;
    song.vVowel   = 0.25f + srnd(45) / 100.0f;
    song.padMorph = srnd(70) / 100.0f;
    song.detDepth = 0.03f + srnd(9) / 100.0f;

    if (srnd(100) < 35) snprintf(song.title, sizeof song.title, "%s", TW1[srnd(16)]);
    else snprintf(song.title, sizeof song.title, "%s %s", TW1[srnd(16)], TW2[srnd(8)]);
    song.freq = 88.5f + srnd(380) / 20.0f;

    band.c[CH_MOVE].sel = song.setup;            // panel reflects the rolled setup
    songCount++;
    apply_timbres();
    reinit_voices(T);
}

static void fresh_song(float T) {                // SPACE / tune — a new seed
    new_song(T, 0);
    rad_hist_log(&rs);
}

// ── update ───────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    float T = now();

    if (!booted) {
        bpm(60);                                 // a clock exists (chrome blinks); unused for timing
        setup_instruments();
        CH_MOVE = rad_chair(&band, "movement", "1/1", "2/1", "1/2", "2/2");
        if (ENO_SEED) { new_song(T, ENO_SEED); rad_hist_log(&rs); }
        else fresh_song(T);
        booted = true;
    }

    rad_tune_advance();                          // keep any running tuner sweep moving
    bool tune = keyp(KEY_SPACE);
    if (mouse_pressed(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y();
        if (mx >= RAD_TUNE_BTN_X && mx < RAD_TUNE_BTN_X + RAD_TUNE_BTN_W &&
            my >= RAD_TUNE_BTN_Y && my < RAD_TUNE_BTN_Y + RAD_TUNE_BTN_H) tune = true;
    }
    if (tune && !rad_is_tuning()) { rad_tune_start(); fresh_song(T); }
    if (keyp('R')) new_song(T, song.seed);
    if (keyp('[')) { unsigned s = rad_hist_back(&rs); if (s) new_song(T, s); }
    if (keyp(']')) { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(T, s); }
    if (keyp('M')) { radioOn = !radioOn; if (!radioOn) all_off(); else reinit_voices(T); }
    if (keyp('T')) { toneSel = (toneSel + 1) % 4; apply_timbres(); }
    if (keyp('H')) showHelp = !showHelp;

    int chair = rad_band_input(&band, &showHelp);
    if (chair == CH_MOVE) { song.setup = band.c[CH_MOVE].sel; apply_timbres(); reinit_voices(T); }

    if (radioOn) service_loops(T);

#ifdef DE_TRACE
    int ns = 0; for (int i = 0; i < NLOOP; i++) if (lpSounding[i]) ns++;
    watch("song", "%d", songCount);
    watch("setup", "%s", SETLONG[song.setup]);
    watch("key", "%s", PCNAME[song.keyPc]);
    watch("mode", "%s", MODENAME[song.mode]);
    watch("active", "%d", active_loops());
    watch("sounding", "%d", ns);
#endif
}

// ── draw — the radio + the loop field (the brain made visible) ───────────────
#define ACCENT CLR_INDIGO
#define FX0 34
#define FX1 286
#define FY0 50
#define FY1 132

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    float t = timer();

    // a few faint stars / dawn dust behind the set
    for (int k = 0; k < 36; k++) {
        int sx = (k * 53 + 7) % 320, sy = (k * 37 + 9) % 16;
        pset(sx, sy + (k % 2 ? 188 : 0), (k % 3) ? CLR_DARKER_GREY : CLR_DARK_GREY);
    }

    rad_body(CLR_DARK_GREY, ACCENT);
    rad_dial(song.freq, ACCENT);

    // the loop-field window
    rectfill(FX0 - 4, FY0 - 2, (FX1 - FX0) + 8, (FY1 - FY0) + 4, CLR_DARKER_BLUE);
    rect(FX0 - 4, FY0 - 2, (FX1 - FX0) + 8, (FY1 - FY0) + 4, CLR_DARK_GREY);

    int active = active_loops();
    int fw = FX1 - FX0, fh = FY1 - FY0;
    int px[NLOOP], cyv[NLOOP];

    if (radioOn) {
        // soft central bloom — brighter the more voices ring at once (the chord)
        int ns = 0; for (int i = 0; i < active; i++) if (lpSounding[i]) ns++;
        if (ns >= 2) {
            int cx = (FX0 + FX1) / 2, cy = (FY0 + FY1) / 2;
            for (int r = ns * 5; r > 0; r -= 5)
                circ(cx, cy, r + (int)(sinf(t * 0.7f) * 2), CLR_DARK_BLUE);
        }

        for (int i = 0; i < active; i++) {
            int cy = FY0 + (i * 2 + 1) * fh / (active * 2);
            cyv[i] = cy;
            int col = LANECOL[i];
            // the track + the sounding window (where in the cycle the note speaks)
            line(FX0, cy, FX1, cy, CLR_DARKER_GREY);
            int instr = loop_instr(song.setup, i);
            int gw = (int)(loop_gate(song.setup, instr) * fw);
            rectfill(FX0, cy - 1, gw, 3, lpSounding[i] ? col : CLR_DARK_GREY);
            // the playhead, creeping at this loop's own speed
            int x = FX0 + (int)(lpPos[i] * fw);
            px[i] = x;
            if (lpSounding[i]) { circfill(x, cy, 3, col); circfill(x, cy, 1, CLR_WHITE); }
            else circfill(x, cy, 1, CLR_DARK_GREY);
            // the loop's note name at the left margin
            int midi = song.baseMidi + song.keyPc + scaleAt(song.mode, song.deg[i]);
            font(FONT_SMALL);
            print(PCNAME[((midi % 12) + 12) % 12], FX0 - 2, cy - 3, CLR_DARK_GREY);
            font(FONT_NORMAL);
        }
        // connectors where two playheads meet AND both ring — the chord forming
        for (int i = 0; i < active; i++)
            for (int j = i + 1; j < active; j++)
                if (lpSounding[i] && lpSounding[j] && rad_iabs(px[i] - px[j]) < 8) {
                    int mx = (px[i] + px[j]) / 2;
                    line(mx, cyv[i], mx, cyv[j], ACCENT);
                    circfill(mx, (cyv[i] + cyv[j]) / 2, 2, CLR_LIGHT_GREY);
                }
    } else {
        print("- radio off -", (FX0 + FX1) / 2 - 36, (FY0 + FY1) / 2 - 3, CLR_DARK_GREY);
    }

    // readout strip
    font(FONT_SMALL);
    if (radioOn) {
        char l[40];
        snprintf(l, sizeof l, "%s  %s %s  %s", SETSHORT[song.setup],
                 PCNAME[song.keyPc], MODENAME[song.mode], SETLONG[song.setup]);
        print(song.title, FX0, FY1 + 6, ACCENT);
        print(l, FX0, FY1 + 14, CLR_LIGHT_GREY);
        snprintf(l, sizeof l, "FM %.1f   #%08X", song.freq, song.seed);
        print(l, 180, FY1 + 14, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // knobs — drift / tone / density (touch-draggable, keys coexist)
    if (rad_knob_float(&drift, 0, 1, 0, 96, 158, 9, "drift", ACCENT)) { /* live next cycle */ }
    if (rad_knob_sel(&toneSel, 4, 160, 158, 9, RAD_TONENAME[toneSel], ACCENT)) apply_timbres();
    rad_knob_float(&density, 0, 1, 0, 224, 158, 9, "density", ACCENT);

    // breathing power LED (no beat to blink to)
    float breathe = 0.5f + 0.5f * sinf(t * 0.7f);
    circfill(292, 36, 2, radioOn && breathe > 0.5f ? ACCENT : CLR_DARKER_GREY);

    rad_band_button(ACCENT);
    rad_help_button(ACCENT);
    rad_band_panel(&band, ACCENT);
    if (showHelp) {
        static const char *HELP[7][2] = {
            { "SPACE/tune", "next song (rolls a new seed)" },
            { "R",          "same seed again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "drift",      "glacial <-> flowing (loop speed)" },
            { "tone",       "mellow .. bright" },
            { "density",    "how many loops are live" },
            { "B",          "the band - pick the movement" },
        };
        static const char *NOTES[3] = {
            "four setups: 1/1 piano - 2/1 voices - 1/2 duet - 2/2 synth",
            "loops of coprime length drift; the chords are emergent",
            "the #number IS the song.  pin: #define ENO_SEED 0x...",
        };
        rad_help_panel("ENO RADIO", HELP, 7, NOTES, 3, ACCENT);
    }

    ui_end();
}
