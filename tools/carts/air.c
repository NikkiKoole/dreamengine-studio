/* de:meta
{
  "slug": "air",
  "collection": ["radio"],
  "title": "air fm",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing"
  ],
  "lineage": "Extends the radio.h station chassis into an artist-station with five song archetypes, each a cited AIR track; novel in using per-archetype template progressions that seed melodic cells, groove, and lead voice together.",
  "homage": "AIR (Moon Safari)",
  "description": "The Moon Safari station - the AIR (Nicolas Godin & J.B. Dunckel) homage, and an ARTIST station, not a genre: the dial plays recognizably DIFFERENT songs OF THEIRS, not one texture re-keyed. SPACE rolls a new tune and you get 'the Sexy Boy one' or 'the Cherry Blossom one' - a different song every time, but always THAT song. Built on the stolen-playbook chord brain: FIVE SONG ARCHETYPES, each a cited AIR track encoded as a template progression that ALSO fixes its groove, tempo, lead voice, mood and form. SEXY BOY (i-bVI-bVII vamp, a fuzzy driven Moog bass riff, laid-back four-on-floor); LA FEMME D'ARGENT (sunny dorian jazz loop, THE rolling melodic bassline, Rhodes comp, Moog lead); PLAYGROUND LOVE (a slow minor torch-song fall, smoky tenor sax, brushed kit); CHERRY BLOSSOM GIRL (bright I-V-vi-IV folk-pop, fingerpicked nylon guitar + breathy flute, airy); KELLY WATCH THE STARS (driving electro four-on-floor, a vocoder robotic lead). The seed varies key / melody cell / patterns / timbre WITHIN an archetype, so two Sexy Boys differ but both are unmistakably Sexy Boy. Reaches engines for AIR's real lineup: INSTR_VOICE as a melodic vocoder lead (a radio first), INSTR_PIPE breathy flute, INSTR_REED tenor sax, the Solina string-machine wash on a detuned-SAW pair, INSTR_EPIANO Rhodes/Wurli, INSTR_GUITAR nylon, and a fuzzy instrument_drive Moog bass - everything sitting in the echo bus's tape wash (AIR is always drenched). The window is a moon over the night highway. SPACE next song, R same tune (a fresh take), [ ] history, LEFT/RIGHT feel (hush/dusk/warm/aglow), UP/DOWN tempo, T tone, B band (strings/e-piano/echo), J jam strip (play along), M power, H help."
}
de:meta */
// ── AIR FM ──────────────────────────────────────────────────────────────────
// The Moon Safari station — AIR (Nicolas Godin & J.B. Dunckel). NOT a genre but
// an ARTIST: the dial plays recognizably DIFFERENT songs *of theirs*, not one
// texture re-keyed. SPACE rolls a new tune and you get "the Sexy Boy one" or
// "the Cherry Blossom one" — a different song every time, but always *that* song.
//
// Designed INTENT-FIRST, band imagined before the palette was shopped
// (radio-station-howto.md firewall; the blind brief: design/air-blind-brief.md).
// Reaches three engines no station had voiced this way for AIR's real lineup:
//   • INSTR_VOICE — the vocoder lead (Kelly Watch the Stars). No station had
//     used the formant engine for a melodic lead before.
//   • INSTR_PIPE  — the breathy flute (Cherry Blossom Girl). Untapped shelf.
//   • INSTR_REED  — the smoky tenor sax (Playground Love).
//   + the Solina string-machine wash on a detuned SAW pair (the AIR signature),
//     INSTR_EPIANO Rhodes/Wurli, INSTR_GUITAR nylon, and a fuzzy driven Moog bass.
//   Everything blooms into a real reverb hall; the Solina pad runs its own per-part BBD
//   ENSEMBLE chorus (the stereo Rhodes too), and the whole record is committed to TAPE
//   (per-archetype wow + saturation), with a touch of echo — AIR is always drenched.
//
// THE BRAIN (stolen-playbook chord brain #4, game-music.md): five SONG ARCHETYPES,
// each a cited AIR track encoded as a template progression that ALSO fixes its
// groove, tempo, lead voice, mood and form. The seed varies key / melody cell /
// patterns / timbre *within* an archetype, so two "Sexy Boy"s differ but both
// are unmistakably Sexy Boy. (radio-station-howto.md Phase 1/4 artist branch.)
//   SEXY       — "Sexy Boy"          fuzz Moog bass riff, laid-back 4-on-floor
//   ARGENT     — "La Femme d'Argent" the rolling melodic bassline, Rhodes, Moog lead
//   PLAYGROUND — "Playground Love"   slow torch ballad, smoky tenor sax, brushes
//   CHERRY     — "Cherry Blossom Girl" nylon arpeggios + flute, airy folk-pop
//   KELLY      — "Kelly Watch the Stars" driving electro, vocoder robotic lead
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density)   UP/DOWN tempo   T tone   B band   J jam   H help

#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "solo.h"    // the jam strip — play along on top of the song
#include <stdio.h>
#include <math.h>

#define AIR_SEED 0   // pin a favourite tune here (0 = free-roaming radio)

// ── instrument slots (5..14) ──────────────────────────────────────────────
#define I_PAD  5     // Solina string ensemble — detuned SAW pair, slow swell
#define I_EP   6     // Rhodes / Wurlitzer (INSTR_EPIANO)
#define I_GTR  7     // nylon acoustic guitar (INSTR_GUITAR) — Cherry's arpeggios
#define I_BASS 8     // bass — reconfigured per archetype (fuzz Moog vs round)
#define I_LEAD 9     // lead — reconfigured per archetype (Moog/sax/flute/vocoder)
#define I_VIBE 10    // vibraphone twinkle (INSTR_MALLET) — Cherry / sparkle
#define I_KICK 11    // kick
#define I_SNR  12    // snare / clap
#define I_HAT  13    // hat / shaker
#define I_SOLO 14    // the jam-strip lead — sits on top of the song

// ── chord qualities + rootless 3-voice voicings (3rd / 7th / colour) ───────
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_DOM9, Q_MIN, Q_MIN7, Q_MIN9, Q_MIN6, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "9", "m", "m7", "m9", "m6" };
static const int QV[NQ][3] = {
    { 4, 7, 14 },    // maj (add9)
    { 4, 11, 14 },   // maj7/9
    { 4, 10, 14 },   // 7/9
    { 4, 10, 14 },   // 9
    { 3, 7, 14 },    // m (add9)
    { 3, 10, 14 },   // m7/9
    { 3, 10, 14 },   // m9
    { 3, 9, 14 },    // m6/9
};

// scales for the re-pitched melody cell — 0 major, 1 aeolian minor, 2 dorian
static const int SCALES[3][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },    // major  (Cherry)
    { 0, 2, 3, 5, 7, 8, 10 },    // aeolian (Sexy / Kelly)
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian  (Argent jazzy / Playground noir)
};

// ── the five song archetypes — each a cited AIR track ──────────────────────
enum { A_SEXY, A_ARGENT, A_PLAY, A_CHERRY, A_KELLY, NA };
// harmony seat: which instrument carries the chords for this archetype
enum { HM_PAD, HM_RHODES, HM_GUITAR };

// the template progression per archetype (4 chords, { semitone offset, quality })
typedef struct { int off, q; } Ch;
static const Ch PROG[NA][4] = {
    // SEXY        i  – bVI  – bVII – i      (minor, the descending grind)
    { { 0, Q_MIN9 }, { 8, Q_MAJ7 }, { 10, Q_MAJ }, { 0, Q_MIN9 } },
    // ARGENT      Imaj7 – vi7 – ii7 – V9    (major, sunny jazz loop)
    { { 0, Q_MAJ7 }, { 9, Q_MIN7 }, { 2, Q_MIN7 }, { 7, Q_DOM9 } },
    // PLAYGROUND  i  – bVII – bVI  – V      (minor, the torch-song fall)
    { { 0, Q_MIN7 }, { 10, Q_MAJ7 }, { 8, Q_MAJ7 }, { 7, Q_DOM7 } },
    // CHERRY      I  – V    – vi   – IV     (major, gentle folk-pop)
    { { 0, Q_MAJ }, { 7, Q_MAJ }, { 9, Q_MIN7 }, { 5, Q_MAJ7 } },
    // KELLY       i  – bVII – bVI  – bVII   (minor, the insistent vamp)
    { { 0, Q_MIN }, { 10, Q_MAJ }, { 8, Q_MAJ }, { 10, Q_MAJ } },
};

static const struct {
    const char *name;     // the on-display archetype name
    const char *track;    // the cited AIR track (homage line)
    const char *vibe;     // a one-line descriptor
    int  scale;           // index into SCALES
    int  harm;            // HM_* — the harmony seat
    int  tlo, tspan;      // tempo band (coupled to the groove)
    int  leadLo, leadHi;  // the lead's register window
} ARCH[NA] = {
    { "SEXY BOY",   "~ Sexy Boy",            "fuzz-Moog groove", 1, HM_PAD,    106, 10, 64, 80 },
    { "ARGENT",     "~ La Femme d'Argent",   "the rolling bass", 2, HM_RHODES, 90, 10, 64, 81 },
    { "PLAYGROUND", "~ Playground Love",     "tenor-sax ballad", 2, HM_RHODES, 68, 10, 54, 72 },
    { "CHERRY",     "~ Cherry Blossom Girl", "nylon & flute",    0, HM_GUITAR, 94, 10, 64, 86 },
    { "KELLY",      "~ Kelly Watch Stars",   "vocoder electro",  1, HM_PAD,   114, 10, 60, 76 },
};

// ── the form — 8-bar sections; the seed rolls one of three lengths ─────────
enum { S_INTRO, S_A, S_B, S_BREAK, S_OUT };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_A, S_B, S_A, S_OUT }, "single" },
    { 8,  { S_INTRO, S_A, S_B, S_A, S_BREAK, S_B, S_A, S_OUT }, "album" },
    { 11, { S_INTRO, S_A, S_B, S_A, S_B, S_BREAK, S_A, S_B, S_A, S_B, S_OUT }, "extended" },
};
#define NFORMS 3
static const char *SECTNAME[5] = { "intro", "verse", "hook", "break", "outro" };

// ── the generated song ─────────────────────────────────────────────────────
typedef struct {
    int   arch, keyPc, form;
    Ch    prog[4];
    int   cellOn[6], cellN;     // melody rhythm cell (2-bar grid)
    int   bassReg;              // rolled bass octave nudge (-12 / 0)
    int   fuzz;                 // 0..100 — rolled fuzz amount on the Moog bass
    int   vibesOn;              // sparkle on/off (rolled)
    char  title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 130.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 106;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 1;        // AIR wants warm, not bright
static int   songCount = 0;
static float vu        = 0;

static int   gvCmp[3]  = { 60, 64, 67 };   // comp (rhodes / pad) voicing state
static bool  cmpInit   = false;
static int   gvGtr[3]  = { 60, 64, 67 };   // nylon-guitar voicing state
static bool  gtrInit   = false;
static int   melPitch  = 74;
static int   bassLast  = 36;
static char  nowChord[4][8];

// per-song instrument base cutoffs (the tone knob multiplies these — gotcha #1)
static int   leadCut = 2400, bassCut = 600;
static bool  padOn   = true;
static float padWet  = 0.28f;       // pad echo (slapback) send
static float padRev  = 0.40f;       // pad reverb send (the Solina drench; lush = wetter)
static float padChorus = 0.58f;     // the Solina ENSEMBLE — per-part chorus on the pad alone
static float spaceW  = 1.0f;        // the reverb chair's master scale: hall 1.0 / room 0.55 / dry 0

// THE BAND (B) — chairs the player can re-voice live
static RadBand band;
static int chStr, chComp, chSpace;

static int iabs(int v) { return v < 0 ? -v : v; }

// ── per-archetype voice setup (reconfigured from the seed each new_song) ───
// I_BASS and I_LEAD change engine entirely between archetypes; everything else
// is configured once in setup_instruments() and only re-filtered by the tone knob.
static void setup_voices(int arch) {
    // BASS — fuzzy driven Moog (Sexy/Kelly) vs round fingered bass (the rest)
    if (arch == A_SEXY || arch == A_KELLY) {
        instrument(I_BASS, INSTR_SAW, 2, 150, 4, 110);
        instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 130, 900);   // a little pluck snap
        bassCut = 720;
        instrument_drive(I_BASS, 0.40f + sng.fuzz * 0.0055f); // 0.40..0.95 — the grind
    } else {
        instrument(I_BASS, INSTR_TRI, 2, 220, 5, 130);        // warm round fingered
        bassCut = arch == A_ARGENT ? 760 : 560;               // Argent's bass sings higher
        instrument_drive(I_BASS, 0.0f);
    }
    instrument_filter(I_BASS, FILTER_LOW, bassCut, arch == A_ARGENT ? 3 : 2);

    // LEAD — the archetype's star voice
    if (arch == A_SEXY || arch == A_ARGENT) {              // resonant Moog (PD reso-lead)
        instrument(I_LEAD, INSTR_PD, 20, 320, 4, 300);
        instrument_harmonics(I_LEAD, 0.94f);               // resonant wavetype
        instrument_timbre(I_LEAD, 0.55f);
        instrument_morph(I_LEAD, 0.45f);
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.0f, 0.10f); // a little vibrato
        leadCut = 2600;
    } else if (arch == A_PLAY) {                           // smoky tenor sax (REED)
        instrument(I_LEAD, INSTR_REED, 1, 0, 4, 1200);
        instrument_harmonics(I_LEAD, 0.82f);               // conical bore
        instrument_timbre(I_LEAD, 0.30f);                  // soft, round
        instrument_morph(I_LEAD, 0.62f);                   // breathy lean
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 4.6f, 0.12f);
        leadCut = 2200;
    } else if (arch == A_CHERRY) {                         // breathy concert flute (PIPE)
        // the showcase pipe/flute recipe, verbatim — overblow at 0 keeps it OUT of the
        // jet-gain "screech at the top" zone the engine warns about. The 2026-06-11 PIPE
        // bore-tuning fix (was octave-low + flat) reopened the register from the old workaround
        // 67–83 to 64–86: at this recipe's embouchure (morph 0.70, short jet) tune-check measures
        // PIPE in tune within ±5¢ from C4 up to ~E6, so the old A5 ceiling is no longer needed.
        // NOTE: the fix also means the flute now sounds at its WRITTEN octave (it played an octave
        // low before), so this register sits an octave above what it used to — by design.
        instrument(I_LEAD, INSTR_PIPE, 1, 0, 4, 1200);
        instrument_harmonics(I_LEAD, 0.00f);               // NO overblow — pure fundamental, in tune
        instrument_timbre(I_LEAD, 0.38f);                  // concert-flute air
        instrument_morph(I_LEAD, 0.70f);                   // focused embouchure
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.0f, 0.08f); // gentle vibrato
        leadCut = 3000;
    } else {                                               // KELLY — vocoder robot (VOICE)
        instrument(I_LEAD, INSTR_VOICE, 8, 90, 6, 220);
        instrument_harmonics(I_LEAD, 0.62f);               // an "O→A" vowel
        instrument_timbre(I_LEAD, 0.40f);                  // tract size
        instrument_morph(I_LEAD, 0.70f);                   // pressed effort = bright robot
        instrument_drive(I_LEAD, 0.25f);                   // a touch of grit
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.6f, 0.06f);
        leadCut = 2400;
    }
    instrument_filter(I_LEAD, FILTER_LOW, leadCut, 2);
    instrument_echo(I_LEAD, arch == A_KELLY ? 0.10f : 0.14f);   // a touch of slapback…
    instrument_reverb(I_LEAD, (arch == A_KELLY ? 0.22f : 0.32f) * spaceW);   // …but the hall carries the space now

    // THE ENSEMBLE now lives PER-PART on the Solina pad (instrument_chorus in
    // setup_instruments / the strings chair), not the master bus — so it can run full
    // solina.c lushness on the pad while Kelly's kit & bass stay bone-dry. No archetype
    // hold-back needed any more; master chorus() stays off.

    // TAPE — the whole record committed to tape: the vintage Moon Safari glue (warmth +
    // a slow wow drift). This one IS master-wide on purpose (it's the tape, not a per-voice
    // FX). A different tape per archetype: the dreamy ones wow & saturate more; Kelly runs
    // tighter & cleaner so the electro stays crisp.
    switch (arch) {
        case A_KELLY: tape(0.10f, 0.08f, 0.24f); break;            // clean, tight
        case A_SEXY:  tape(0.18f, 0.10f, 0.40f); break;            // grittier saturation
        default:      tape(0.22f, 0.11f, 0.34f); break;            // Argent/Playground/Cherry — warm & drifting
    }
}

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "Moon", "Cherry", "Silver", "Sexy", "Highway", "Sunset",
    "Velvet", "Radian", "Soft", "Outer", "Le Soir", "Playground", "Talkie", "Pocket" };
static const char *TW2[] = { "Safari", "Love", "Girl", "Boy", "Woman", "Light",
    "Star", "Dream", "Dimanche", "Mode", "Drift", "Space", "Walker", "Sun" };

static void roll_cell(int dense) {
    sng.cellN = 0;
    for (int s = 0; s < 31 && sng.cellN < 6; s += 2)
        if (srnd(100) < (s % 8 == 0 ? dense - 8 : dense)) sng.cellOn[sng.cellN++] = s;
    if (sng.cellN < 3) {
        sng.cellN = 3;
        sng.cellOn[0] = 4; sng.cellOn[1] = 14; sng.cellOn[2] = 22;
    }
}

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.arch  = srnd(NA);
    sng.keyPc = srnd(12);
    for (int k = 0; k < 4; k++) sng.prog[k] = PROG[sng.arch][k];

    // flavor rolls — small "two records" variation WITHIN the archetype
    if (sng.arch == A_ARGENT && srnd(100) < 45)            // sometimes a 6 instead of maj7
        sng.prog[0].q = Q_MAJ7;
    if (srnd(100) < 35)                                    // sometimes lift the bVII to a 9
        for (int k = 0; k < 4; k++) if (sng.prog[k].off == 10 && sng.prog[k].q == Q_MAJ)
            sng.prog[k].q = Q_DOM9;

    sng.fuzz    = srnd(100);
    sng.bassReg = srnd(100) < 35 ? -12 : 0;
    sng.vibesOn = (sng.arch == A_CHERRY) || (srnd(100) < 25);
    roll_cell(sng.arch == A_PLAY ? 24 : 34);               // sax leaves more space

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(14)], TW2[srnd(14)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    sng.form = srnd(100) < 32 ? 0 : srnd(100) < 72 ? 1 : 2;

    tempo = ARCH[sng.arch].tlo + srnd(ARCH[sng.arch].tspan);
    bpm(tempo);

    setup_voices(sng.arch);
    songBase = (long)pos + 8;
    cmpInit = gtrInit = false;
    melPitch = (ARCH[sng.arch].leadLo + ARCH[sng.arch].leadHi) / 2;
    bassLast = 36;
    songCount++;
}

static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── form / harmony lookups ─────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[sng.form].n; }
static long song_bars(void)  { return (long)FORMS[sng.form].n * 8; }
static int  sect_of(long bar) {
    int x = (int)(bar / 8), n = FORMS[sng.form].n;
    return x < n ? FORMS[sng.form].s[x] : S_OUT;
}
static Ch  chord_at(long bar) { return sng.prog[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]);
}

// the density curve: section baseline shifted by the feel knob (rad_level)
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUT) ? 0 : (s == S_A ? 1 : 2);
    return rad_level(base, intensity);
}

// nearest-tone voice leading into a 3-voice comp/guitar voicing
static void lead_comp(Ch c, int *gv, bool *init, int lo, int hi) {
    rad_lead_to(root_pc(c), QV[c.q], gv, 3, lo, hi, init);
}

// melody: nearest chord/scale tone to where the line sits, with a little drift
static int pick_mel(Ch c, int lo, int hi) {
    const int *sc = SCALES[ARCH[sng.arch].scale];
    int rt = root_pc(c);
    int best = melPitch, bd = 999;
    for (int t = 0; t < 7; t++) {
        int pc = (sng.keyPc + sc[t]) % 12;
        int rel = (pc - rt + 12) % 12;
        bool chordTone = (rel == 0 || rel == 3 || rel == 4 || rel == 7 || rel == 10);
        for (int o = lo / 12; o <= hi / 12 + 1; o++) {
            int m = o * 12 + pc;
            if (m < lo || m > hi) continue;
            int d = iabs(m - melPitch) + rnd(3) + (chordTone ? 0 : 4);
            if (m == melPitch) d += 3;
            if (d < bd) { bd = d; best = m; }
        }
    }
    melPitch = best;
    return best;
}

// the bass: nearest octave copy of a pitch class (the line stays smooth + the
// chromatic falls survive any key)
static int bass_peek(int pc, int lo, int hi) { return rad_bass_to(pc, bassLast, lo, hi); }
static int bass_near(int pc, int lo, int hi) { return bassLast = bass_peek(pc, lo, hi); }

// ── the step player ────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= song_bars()) return;
    int  cs   = (int)(s % 32);                 // position in the 2-bar window
    int  bib  = (int)(bar % 8);                // bar-in-section (intro build)
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  arch = sng.arch;
    Ch   c    = chord_at(bar);
    bool brk  = (sect == S_BREAK);
    int  bLo  = 31 + sng.bassReg, bHi = 47 + sng.bassReg;

    // additive intro: the band layers in over the 8-bar intro
    bool inDrums = sect != S_INTRO || bib >= 0;
    bool inBass  = sect != S_INTRO || bib >= 2;
    bool inHarm  = (sect != S_INTRO || bib >= 4) && !brk;
    bool inLead  = (sect != S_INTRO || bib >= 6) && !brk;

    // ── THE KIT — a soft vintage machine, the feel set by the archetype ──
    if (inDrums) {
        bool kick = false, clap = (step == 4 || step == 12);   // claps/snare on 2&4
        if      (arch == A_KELLY)  kick = (step % 4 == 0);                 // four-on-floor drive
        else if (arch == A_SEXY)   kick = (step == 0 || step == 8 || (step == 6 && chance(40)));
        else if (arch == A_ARGENT) kick = (step == 0 || step == 10 || (step == 7 && chance(50)));
        else if (arch == A_PLAY)   kick = (step == 0);                     // a soft heartbeat
        else                       kick = (step == 0 || step == 8);        // Cherry, gentle
        if (kick) { schedule_hit(dly, 36, I_KICK, arch == A_KELLY ? 5 : 4, 110); vu += 1.4f; }

        if (clap) {
            int v = (arch == A_PLAY) ? 2 : (arch == A_KELLY ? 5 : 4);
            schedule_hit(dly + rnd(3), 60, I_SNR, v, arch == A_PLAY ? 60 : 90);
            vu += 1.2f;
        } else if (lvl >= 1 && arch == A_ARGENT && (step % 2) && chance(22))
            schedule_hit(dly + rnd(5), 60, I_SNR, 1, 40);      // a scattered ghost

        // hats / shaker — off-beats; brushes lay back on the ballad
        if (lvl >= 1 && arch != A_PLAY && step % 2 == 0) {
            int v = (step % 8 == 4) ? 2 : 1;
            schedule_hit(dly + rnd(4), 90, I_HAT, v, arch == A_KELLY ? 24 : 30);
        }
        if (arch == A_PLAY && lvl >= 1 && (step == 6 || step == 14))
            schedule_hit(dly + rnd(6), 90, I_HAT, 1, 50);      // brush sweep
    }

    // ── BASS — the per-archetype line (Argent's is the busy, rolling star) ──
    if (inBass) {
        int rt = root_pc(c);
        if (arch == A_KELLY) {                                  // driving 8th-note pulse
            if (step % 2 == 0) {
                int n = bass_near(rt, bLo, bHi);
                schedule_hit(dly + 4, n, I_BASS, step % 4 == 0 ? 5 : 4, (int)(stepMs * 1.6));
                vu += 1.1f;
            }
        } else if (arch == A_SEXY) {                            // the fuzzy syncopated grind
            static const int RIFF[5] = { 0, 6, 8, 11, 14 };
            for (int i = 0; i < 5; i++) if (RIFF[i] == step) {
                int n = bass_near(rt, bLo, bHi);
                if (i == 3) n -= 2;                             // a chromatic lean
                schedule_hit(dly + 6, n, I_BASS, i == 0 ? 5 : 4, (int)(stepMs * 2.2));
                vu += 1.3f;
            }
        } else if (arch == A_ARGENT) {                          // the rolling melodic bassline
            static const int ROLL[8] = { 0, 3, 6, 8, 11, 14, 19, 22 };  // 2-bar grid
            static const int DEG[8]  = { 0, 7, 0, 5, 0, 10, 7, 4 };     // root–5–oct–passing
            for (int i = 0; i < 8; i++) if (ROLL[i] == cs) {
                bassLast = bass_peek((rt + DEG[i]) % 12, bLo, bHi + 7);  // roams a 5th higher
                schedule_hit(dly + 8, bassLast, I_BASS, i == 0 ? 5 : 3, (int)(stepMs * 1.7));
                vu += 1.0f;
            }
        } else {                                                // Playground / Cherry — gentle
            if (step == 0 || step == 8) {
                int n = bass_near(rt, bLo, bHi);
                schedule_hit(dly, n, I_BASS, step == 0 ? 4 : 3, (int)(stepMs * 5.0));
                vu += 0.9f;
            } else if (step == 14 && chance(60)) {              // chromatic approach
                int nb = bass_peek(root_pc(chord_at(bar + 1)), bLo, bHi);
                if (iabs(nb - bassLast) <= 2 && nb != bassLast)
                    schedule_hit(dly, nb > bassLast ? nb - 1 : nb + 1, I_BASS, 2, (int)(stepMs * 1.5));
            }
        }
    }

    // ── HARMONY — Solina pad / Rhodes comp / nylon arpeggio, by archetype ──
    if (inHarm && lvl >= 1) {
        int hm = ARCH[arch].harm;
        if (hm == HM_PAD && padOn) {                            // Solina wash, re-voiced /2 bars
            if (step == 0 && bar % 2 == 0) {
                lead_comp(c, gvCmp, &cmpInit, 50, 72);
                int v = (arch == A_KELLY && lvl >= 2) ? 4 : 3;
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 10, gvCmp[k], I_PAD, v, (int)(stepMs * (arch == A_KELLY ? 12 : 28)));
                vu += 1.0f;
            }
        } else if (hm == HM_RHODES) {                           // Rhodes comp, soft stabs
            bool comp = (arch == A_PLAY) ? (step == 0 || (step == 10 && chance(60)))
                                         : (step == 0 || step == 6 || (step == 11 && chance(50)));
            if (comp) {
                lead_comp(c, gvCmp, &cmpInit, 52, 74);
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 8 + rnd(4), gvCmp[k], I_EP, lvl >= 2 ? 3 : 2,
                                 (int)(stepMs * (arch == A_PLAY ? 8 : 4)));
                vu += 0.9f;
            }
            // Cherry-style soft string bed even under Rhodes archetypes? keep it Rhodes-only.
        } else {                                                // HM_GUITAR — fingerpicked nylon
            static const int PICK[8] = { 0, 2, 1, 2, 0, 2, 1, 2 };
            if (step % 2 == 0 && !chance(lvl >= 2 ? 6 : 18)) {
                lead_comp(c, gvGtr, &gtrInit, 55, 79);
                int v = gvGtr[PICK[(step / 2) % 8]];
                schedule_hit(dly + rnd(6), v, I_GTR, (step == 0 || lvl >= 3) ? 3 : 2, (int)(stepMs * 2.6));
                vu += 0.8f;
            }
            if (padOn && step == 0 && bar % 4 == 0) {           // a quiet string bed under the nylon
                lead_comp(c, gvCmp, &cmpInit, 55, 74);
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 12, gvCmp[k], I_PAD, 2, (int)(stepMs * 56));
            }
        }
    }

    // ── VIBRAPHONE sparkle — a high twinkle on the hook (rolled on) ──
    if (sng.vibesOn && inHarm && lvl >= 2 && sect == S_B && (step == 4 || step == 12) && chance(55)) {
        int pcs[3]; for (int k = 0; k < 3; k++) pcs[k] = (root_pc(c) + QV[c.q][k]) % 12;
        int m = 84 + pcs[rnd(3)] % 12 - 6;
        schedule_hit(dly + 10, m, I_VIBE, 2, (int)(stepMs * 6));
        vu += 0.5f;
    }

    // ── LEAD — the seeded cell, re-pitched. Lays out for the jam strip. ──
    if (inLead && lvl >= 2 && !solo_open()) {
        // the hook section gets the lead; the verse keeps it sparser
        bool active = (sect == S_B) || (arch == A_PLAY) || chance(50);
        if (active)
            for (int i = 0; i < sng.cellN; i++)
                if (sng.cellOn[i] == cs && chance(arch == A_PLAY ? 78 : 88)) {
                    int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - cs : 32 - cs;
                    int dur = (int)(gap * stepMs * (arch == A_PLAY ? 1.0 : 0.85));
                    if (dur > 1600) dur = 1600;
                    int late = (arch == A_PLAY) ? 22 : 12;      // the sax sits late
                    schedule_hit(dly + late + rnd(8),
                                 pick_mel(c, ARCH[arch].leadLo, ARCH[arch].leadHi),
                                 I_LEAD, 3, dur);
                    vu += 1.2f;
                }
    }
}

// ── one-time instrument setup (fixed slots; bass/lead done in setup_voices) ─
static void apply_chair(int idx);

static void setup_instruments(void) {
    chStr  = rad_chair(&band, "strings", "solina", "lush", "off", NULL);
    chComp = rad_chair(&band, "e-piano", "rhodes", "wurli", NULL, NULL);
    chSpace = rad_chair(&band, "reverb",  "hall", "room", "dry", NULL);

    chorus(1.0f, 0.4f, 0.0f);                       // master chorus OFF — the Solina ENSEMBLE is
                                                    // now per-part (below), so the kit never gets washed

    // Solina string ensemble — a saw slot whose ENSEMBLE is a real per-part BBD chorus
    // (the solina.c recipe, now that instrument_chorus exists). The detune is just the
    // divide-down beating under it; the chorus does the swirl.
    instrument(I_PAD, INSTR_SAW, 600, 400, 6, 1600);
    instrument_filter(I_PAD, FILTER_LOW, 2000, 2);
    instrument_tune(I_PAD, 0.05f);                  // subtle divide-down beating (solina.c value)
    instrument_lfo(I_PAD, 0, LFO_PITCH, 0.18f, 0.05f);   // slow tape wow under the ensemble
    instrument_chorus(I_PAD, 0.9f, 0.50f, padChorus);    // THE ENSEMBLE — solina.c "I", on the pad alone
    instrument_echo(I_PAD, padWet);
    instrument_pan(I_PAD, -0.15f);

    // Rhodes / Wurlitzer (the chair swaps the model) — a touch of chorus = the stereo Rhodes
    instrument(I_EP, INSTR_EPIANO, 2, 0, 6, 900);
    instrument_harmonics(I_EP, 0.15f);              // Rhodes
    instrument_timbre(I_EP, 0.35f);
    instrument_morph(I_EP, 0.20f);
    instrument_filter(I_EP, FILTER_LOW, 2200, 2);
    instrument_chorus(I_EP, 0.7f, 0.30f, 0.28f);    // gentle stereo-Rhodes width
    instrument_echo(I_EP, 0.14f);

    // nylon acoustic guitar (Cherry's arpeggios)
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1100);
    instrument_harmonics(I_GTR, 0.45f);             // warm nylon body
    instrument_timbre(I_GTR, 0.22f);
    instrument_morph(I_GTR, 0.24f);
    instrument_filter(I_GTR, FILTER_LOW, 2600, 2);
    instrument_echo(I_GTR, 0.16f);

    // vibraphone twinkle
    instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1400);
    instrument_harmonics(I_VIBE, 0.25f);
    instrument_timbre(I_VIBE, 0.45f);
    instrument_morph(I_VIBE, 0.85f);                // long ring, motor on
    instrument_echo(I_VIBE, 0.30f);
    instrument_pan(I_VIBE, 0.25f);

    // KICK — soft sine thump
    instrument(I_KICK, INSTR_SINE, 1, 80, 0, 70);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 42, 16);
    // SNARE / CLAP — band-shaped noise
    instrument(I_SNR, INSTR_NOISE, 1, 0, 0, 90);
    instrument_filter(I_SNR, FILTER_BAND, 1500, 4);
    instrument_echo(I_SNR, 0.10f);
    // HAT / brush — high filtered noise
    instrument(I_HAT, INSTR_NOISE, 0, 22, 0, 16);
    instrument_filter(I_HAT, FILTER_HIGH, 6800, 3);

    // the jam-strip lead — soft but present, sits above the mix
    instrument(I_SOLO, INSTR_SINE, 12, 200, 6, 240);
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.2f, 0.16f);
    instrument_filter(I_SOLO, FILTER_LOW, 3000, 2);
    instrument_echo(I_SOLO, 0.22f);

    // a real reverb hall — the AIR drench (per-slot sends; the low end stays dry & tight)
    reverb(0.62f, 0.38f);                  // lush but not endless; a little dark in the tail
    instrument_reverb(I_PAD,  padRev);     // the Solina blooms into the room
    instrument_reverb(I_EP,   0.28f);
    instrument_reverb(I_GTR,  0.26f);
    instrument_reverb(I_VIBE, 0.42f);      // twinkle hangs in the hall
    instrument_reverb(I_SNR,  0.18f);
    instrument_reverb(I_SOLO, 0.30f);
    // I_LEAD's send is set per archetype in setup_voices; I_KICK/I_HAT/I_BASS stay dry

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);  // base = sel 0
}

// ── THE BAND chairs — live re-voicing (never touches the seed) ──────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chStr) {
        padOn     = (sel != 2);
        padWet    = (sel == 1) ? 0.45f : 0.28f;
        padRev    = (sel == 1) ? 0.55f : 0.40f;             // lush = deeper into the hall
        padChorus = (sel == 1) ? 0.70f : 0.58f;             // lush = ENSEMBLE II territory
        instrument_tune(I_PAD, sel == 1 ? 0.07f : 0.05f);   // lush = wider divide-down beating
        instrument_chorus(I_PAD, 0.9f, 0.50f, padOn ? padChorus : 0);
        instrument_echo(I_PAD, padWet);
        instrument_reverb(I_PAD, padOn ? padRev * spaceW : 0);
    } else if (idx == chComp) {
        if (sel == 0) {                                     // Rhodes
            instrument_harmonics(I_EP, 0.15f);
            instrument_timbre(I_EP, 0.35f); instrument_morph(I_EP, 0.20f);
        } else {                                            // Wurlitzer — barkier
            instrument_harmonics(I_EP, 0.50f);
            instrument_timbre(I_EP, 0.42f); instrument_morph(I_EP, 0.35f);
        }
    } else if (idx == chSpace) {
        spaceW = (sel == 0) ? 1.0f : (sel == 1) ? 0.55f : 0.0f;   // hall / room / dry
        instrument_reverb(I_PAD,  (padOn ? padRev : 0) * spaceW);
        instrument_reverb(I_EP,   0.28f * spaceW);
        instrument_reverb(I_GTR,  0.26f * spaceW);
        instrument_reverb(I_VIBE, 0.42f * spaceW);
        instrument_reverb(I_LEAD, 0.32f * spaceW);
        instrument_reverb(I_SNR,  0.18f * spaceW);
        instrument_reverb(I_SOLO, 0.30f * spaceW);
    }
}

// the tone knob — re-aim every filtered slot, keeping each song's base cutoff
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_PAD,  FILTER_LOW, (int)(2000 * tm), 2);
    instrument_filter(I_EP,   FILTER_LOW, (int)(2200 * tm), 2);
    instrument_filter(I_GTR,  FILTER_LOW, (int)(2600 * tm), 2);
    instrument_filter(I_LEAD, FILTER_LOW, (int)(leadCut * tm), 2);
    instrument_filter(I_BASS, FILTER_LOW, (int)(bassCut * (0.8f + 0.2f * tm)),
                      sng.arch == A_ARGENT ? 3 : 2);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (AIR_SEED) { new_song(pos, AIR_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 60, 130, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); sfx(-1); } else scheduled = (long)pos; }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD))
        apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) { apply_chair(chair); apply_tone(); }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        long songStep = scheduled - songBase;
        if (songStep >= song_bars() * 16) fresh_song(pos);
        long bar = songStep >= 0 ? songStep / 16 : 0;
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, sng.prog[i]);
        (void)bar;
    }

    vu *= 0.87f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("arch", "%s", ARCH[sng.arch].name);
    watch("form", "%s", FORMS[sng.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("key", "%s", RAD_PCNAME[sng.keyPc]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — the chassis; window art = a moon over the night highway ──────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);
    int  ACC  = CLR_BLUE;

    rad_body(CLR_INDIGO, ACC);                       // lavender-grey vintage shell
    rad_dial(sng.freq, ACC);

    // the window — a Moon Safari: gradient dusk sky, a big moon, stars, a road
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);
    rectfill(34, 52, 102, 24, CLR_DARK_BLUE);        // deeper at the top
    rectfill(34, 118, 102, 18, CLR_DARK_PURPLE);     // warm band at the horizon
    rectfill(34, 130, 102, 38, CLR_DARKER_PURPLE);   // the dark land
    // stars (twinkle)
    for (int k = 0; k < 30; k++) {
        int sx = 36 + (k * 47 + 9) % 98, sy = 54 + (k * 29 + 3) % 60;
        if ((k + frame() / 40) % 5) pset(sx, sy, k % 4 ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    }
    // the moon, with a soft halo + a couple of craters
    int mx = 110, my = 74;
    circfill(mx, my, 13, CLR_DARKER_GREY);           // halo
    circfill(mx, my, 11, CLR_LIGHT_PEACH);
    circfill(mx - 4, my - 3, 2, CLR_MEDIUM_GREY);
    circfill(mx + 3, my + 4, 2, CLR_MEDIUM_GREY);
    circfill(mx + 2, my - 5, 1, CLR_MEDIUM_GREY);
    // the highway — its edges recede to a vanishing point, dashed centre line, vu-lit
    int vp = 85, glow = (int)vu;
    line(38, 168, vp, 131, CLR_DARK_GREY);           // left verge
    line(131, 168, vp, 131, CLR_DARK_GREY);          // right verge
    for (int i = 0; i < 6; i++) {                    // dashes scroll toward the horizon
        int yy = 167 - i * 6 - (frame() / 3) % 6;
        if (yy > 132) rectfill(vp, yy, (yy - 131) / 12 + 1, 2,
                               glow > 5 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, ACC);
    if (radioOn) {
        print(sng.title, 154, 58, ACC);
        char l2[32];
        snprintf(l2, 32, "%s  %s", ARCH[sng.arch].name, ARCH[sng.arch].vibe);
        font(FONT_SMALL); print(l2, 154, 70, CLR_LIGHT_PEACH); font(FONT_NORMAL);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 81, CLR_LIGHT_PEACH);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, ACC);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ci = (int)(bar % 4), x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]);
            if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 102, cw + 4, 12, ACC); print(nowChord[i], x, 104, CLR_BLACK); }
            else print(nowChord[i], x, 104, CLR_LIGHT_GREY);
            x += cw + 8;
        }
        print(SECTNAME[sect], 152, 120, ACC);
        print(ARCH[sng.arch].track, 196, 120, CLR_INDIGO);
        rad_phrase_dots(232, 124, form_sects(), bar / 8, ACC);
    }

    static const char *FEEL[4] = { "hush", "dusk", "warm", "aglow" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], ACC);
    if (rad_knob_int(&tempo, 60, 130, 2, 218, 148, 9, "tempo", ACC)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], ACC)) apply_tone();
    rad_power_led(radioOn, ACC, CLR_DARK_BLUE);

    rad_help_button(ACC);
    rad_band_button(ACC);

    if (showHelp) {
        static const char *HELP[9][2] = {
            { "SPACE",      "next tune (rolls a new song)" },
            { "R",          "same tune - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - hush/dusk/warm/aglow" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "B",          "the band - strings/e-piano/reverb" },
            { "J / tap",    "the jam strip - play along" },
            { "L",          "jam ch/sc: chord-lock / free" },
        };
        static const char *NOTES[3] = {
            "the dial plays different AIR songs: Sexy Boy,",
            "La Femme d'Argent, Playground Love, Cherry",
            "Blossom Girl, Kelly Watch the Stars.",
        };
        rad_help_panel("AIR FM", HELP, 9, NOTES, 3, ACC);
    }
    rad_band_panel(&band, ACC);

    // the jam strip — play along on the song's scale, the chord's tones lit
    if (radioOn && !showHelp && !band.show) {
        Ch c = chord_at(bar);
        int chord[4]; for (int k = 0; k < 4; k++) chord[k] = (root_pc(c) + (k ? QV[c.q][k - 1] : 0)) % 12;
        const int *sc = SCALES[ARCH[sng.arch].scale];
        SoloCtx jc = { sng.keyPc, sc, 7, chord, 4, I_SOLO, 67, 88, false, SOLO_Y_BRIGHT, 1400, 5200 };
        solo_strip(&jc, 28, 170, 250, 16, ACC);
    }

    ui_end();
}
