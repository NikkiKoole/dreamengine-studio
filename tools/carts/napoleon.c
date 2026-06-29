/* de:meta
{
  "title": "napoleon radio",
  "status": "active",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "generative-melody"
  ],
  "lineage": "Artist station for the Napoleon Dynamite (2004) soundtrack; first radio cart to use five hard-switching song archetypes (each fixing its own voice/groove/form) and the first to drive INSTR_VOICE as a sung vibrato croon rather than a robotic vocoder lead.",
  "homage": "Napoleon Dynamite (2004) - soundtrack & John Swihart score",
  "description": "An ARTIST station for the musical world of Napoleon Dynamite (2004) - not one texture re-keyed, but the dial playing five recognizably DIFFERENT songs of theirs, the deliberate genre-clash that IS the film's soundtrack. Each SPACE rolls one of five SONG ARCHETYPES (stolen-playbook chord brain), and each fixes its own lead voice, groove, tempo and form; the seed only varies key/patterns within it. DANCE - Napoleon's class-president dance ('Canned Heat'): ~124bpm acid-funk, four-on-the-floor, popping slap bass + auto-wah clav 16ths, real horn-section stabs, falsetto 'ooh'. SERENADE - Kip's wedding song to LaFawnduh ('Always & Forever'): ~68bpm quiet-storm soul, a SUNG falsetto croon over lush Rhodes + string pad, finger-snaps. SWIHART - John Swihart's deadpan score (the film's actual identity): ~96bpm stiff straight-8s, a cheap detuned toy-organ melody doubled by muted pizzicato guitar + glockenspiel. FOREVER - anthemic 80s synth-pop ('Forever Young'): ~108bpm half-time, fat saw pad, bright arp, big gated snare. FRIENDS - gentle fingerpicked folk ('We're Going to Be Friends'): ~88bpm nylon arpeggios + a single sung voice. The first radio voice to SING on INSTR_VOICE (AIR voices the engine as a vocoder; this one croons). Play along on the jam ribbon (the song's scale, the chord tones lit) - its voice and behavior change per archetype. SPACE next, R replay, [ ] history, LEFT/RIGHT feel, UP/DOWN tempo, T tone, M power, H help."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "solo.h"    // the jam ribbon — a scale-locked lead the player drives over the song
#include <stdio.h>
#include <math.h>

// ── NAPOLEON RADIO ──────────────────────────────────────────────────────────
// An ARTIST station for the musical world of *Napoleon Dynamite* (2004): not one
// texture re-keyed, but the dial playing five recognizably DIFFERENT songs of
// theirs — the deliberate genre-clash that IS the film's soundtrack. Each press
// of SPACE rolls one of five SONG ARCHETYPES (stolen-playbook chord brain), and
// each archetype FIXES its own lead voice, groove, tempo and form; the seed only
// varies key/patterns *within* it, so "the dance one" sounds different every time
// but always like the dance. (Design: docs/design/napoleon-blind-brief.md.)
//
//   DANCE    — "Canned Heat" (Jamiroquai), Napoleon's class-president dance:
//              ~124 BPM acid-funk, four-on-the-floor, popping slap bass + wah
//              clav 16ths, real horn-section stabs, falsetto "ooh".
//   SERENADE — Kip's wedding song to LaFawnduh ("Always & Forever"): ~68 BPM
//              quiet-storm soul ballad, a SUNG falsetto croon over lush Rhodes,
//              round soul bass, string-pad swell, finger-snaps. (the user's
//              "kip waits for lafawnduh" theme — the tender one.)
//   SWIHART  — John Swihart's deadpan original score, the film's actual identity:
//              ~96 BPM stiff straight-8s, a cheap detuned toy-organ melody doubled
//              by a muted pizzicato guitar, glockenspiel twinkle. Childlike,
//              wonky, innocent.
//   FOREVER  — anthemic 80s synth-pop ("Forever Young"): ~108 BPM half-time, fat
//              saw pad, bright arp, big gated snare, an earnest synth lead.
//   FRIENDS  — gentle fingerpicked folk ("We're Going to Be Friends"): ~88 BPM,
//              nylon arpeggios + a single sung voice, sparse brushes & handclaps.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density)   UP/DOWN tempo   T tone   H or ? help
//
// The sung falsetto (Kip's croon + the Jamiroquai "ooh") is the cart's highest-value
// addition — the first radio voice to use INSTR_VOICE as a SUNG, vibrato croon (AIR
// reached the engine first, but as a robotic vocoder lead). Seed pins the
// COMPOSITION (archetype, key, progressions, tempo, title); the PERFORMANCE
// (humanize, kit looseness) re-rolls each playthrough.

#define NAPOLEON_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots (re-voiced per archetype in voice_for_arch) ────────────
#define I_BASS  5    // funk-slap / round-soul / square-root / synth-pulse / folk-root
#define I_COMP  6    // wah-clav / rhodes / detune-twin / saw-pad / —
#define I_LEAD  7    // brass-stab / (croon=VOX) / toy-organ / synth-lead / (gtr)
#define I_GTR   8    // INSTR_GUITAR: muted pizz (SWIHART) / nylon fingerpick (FRIENDS)
#define I_AUX   9    // strings-pad / glockenspiel / bright-arp
#define I_KICK  10
#define I_SNARE 11
#define I_HAT   12
#define I_PERC  13   // clap / open-hat / shaker / finger-snap
#define I_VOX   14   // INSTR_VOICE — the sung falsetto croon (always the voice engine)
#define I_SOLO  15   // the jam-ribbon lead the player drives (solo.h)

// ── archetypes ──────────────────────────────────────────────────────────────
enum { A_DANCE, A_SERENADE, A_SWIHART, A_FOREVER, A_FRIENDS, NARCH };
static const char *ARCHN[NARCH] =
    { "DANCE", "SERENADE", "SWIHART", "FOREVER", "FRIENDS" };
static const char *ARCHDESC[NARCH] = {
    "the dance",          // Canned Heat
    "kip & lafawnduh",    // Always & Forever
    "the score",          // Swihart main theme
    "forever young",      // 80s synth-pop
    "gonna be friends",   // White Stripes folk
};

// the jam-ribbon behavior per archetype (solo.h): register, what vertical drag does,
// whether it quantizes to the 16th, and whether it RE-STRIKES on a drag (struck =
// mallet/plucked voices that ring on their own). Paired with the per-arch I_SOLO voice.
typedef struct { int lo, hi, ymode, quant, struck; float ymin, ymax; } SoloCfg;
static const SoloCfg SOLOCFG[NARCH] = {
    { 60, 84, SOLO_Y_BRIGHT, 1, 0, 1300, 5000 },   // DANCE — funk synth, quantized, filter ride
    { 57, 79, SOLO_Y_VOL,    0, 0, 2,    6    },    // SERENADE — croon, free, swell with the drag
    { 72, 93, SOLO_Y_BRIGHT, 1, 1, 2200, 6500 },   // SWIHART — glock, quantized, struck
    { 67, 90, SOLO_Y_BRIGHT, 0, 0, 1600, 5200 },   // FOREVER — synth lead, free, filter ride
    { 55, 79, SOLO_Y_VOL,    0, 1, 2,    6    },    // FRIENDS — nylon, plucked/struck, swell
};

// ── chords ──────────────────────────────────────────────────────────────────
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_DOM9, Q_MIN, Q_MIN7, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "9", "m", "m7" };
// rootless 3-voice voicings (3rd / 7th-or-5th / 9-color) — the bass owns the root
static const int QV[NQ][3] = {
    { 4, 7, 14 },    // maj (add9)
    { 4, 11, 14 },   // maj7/9
    { 4, 10, 14 },   // 7/9
    { 4, 10, 14 },   // 9
    { 3, 7, 14 },    // m (add9)
    { 3, 10, 14 },   // m7/9
};
static const int QT[NQ][4] = {   // chord tones (root,3,5,7) for the melody walk
    { 0, 4, 7, 7 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 }, { 0, 4, 10, 14 },
    { 0, 3, 7, 7 }, { 0, 3, 7, 10 },
};
static const int MAJSC[7] = { 0, 2, 4, 5, 7, 9, 11 };   // major key
static const int MINSC[7] = { 0, 2, 3, 5, 7, 8, 10 };   // natural minor

// ── progression templates — the stolen playbook, one set per archetype ──────
// { semitone offset from key, quality }. Each archetype rolls a verse + chorus
// progression from its own three; the seed varies which, plus the key.
typedef struct { int off, q; } Ch;
static const Ch ARCH_PROG[NARCH][3][4] = {
    // DANCE — minor acid-funk vamp (i7 driving the groove)
    { { {0,Q_MIN7},{0,Q_MIN7},{10,Q_DOM7},{5,Q_DOM7} },     // i7 i7 bVII7 IV7
      { {0,Q_MIN7},{5,Q_DOM7},{10,Q_DOM7},{5,Q_DOM7} },     // i7 IV7 bVII7 IV7
      { {0,Q_MIN7},{8,Q_MAJ7},{10,Q_DOM7},{0,Q_MIN7} } },   // i7 bVImaj7 bVII7 i7
    // SERENADE — smooth-soul major7 turnarounds
    { { {0,Q_MAJ7},{9,Q_MIN7},{2,Q_MIN7},{7,Q_DOM7} },      // Imaj7 vi7 ii7 V7
      { {0,Q_MAJ7},{4,Q_MIN7},{9,Q_MIN7},{2,Q_MIN7} },      // Imaj7 iii7 vi7 ii7
      { {5,Q_MAJ7},{4,Q_MIN7},{2,Q_MIN7},{7,Q_DOM9} } },    // IVmaj7 iii7 ii7 V9
    // SWIHART — childlike diatonic, no extensions (the point is innocence)
    { { {0,Q_MAJ},{7,Q_MAJ},{9,Q_MIN},{5,Q_MAJ} },          // I V vi IV
      { {0,Q_MAJ},{5,Q_MAJ},{7,Q_MAJ},{0,Q_MAJ} },          // I IV V I
      { {0,Q_MAJ},{9,Q_MIN},{5,Q_MAJ},{7,Q_MAJ} } },        // I vi IV V
    // FOREVER — epic-80s borrowed majors over a minor tonic
    { { {0,Q_MIN},{8,Q_MAJ},{3,Q_MAJ},{10,Q_MAJ} },         // i bVI bIII bVII
      { {0,Q_MIN},{10,Q_MAJ},{8,Q_MAJ},{10,Q_MAJ} },        // i bVII bVI bVII
      { {8,Q_MAJ},{10,Q_MAJ},{0,Q_MIN},{0,Q_MIN} } },       // bVI bVII i i  (lift)
    // FRIENDS — plain folk I-IV-V
    { { {0,Q_MAJ},{5,Q_MAJ},{0,Q_MAJ},{7,Q_MAJ} },          // I IV I V
      { {0,Q_MAJ},{0,Q_MAJ},{5,Q_MAJ},{7,Q_MAJ} },          // I I IV V
      { {0,Q_MAJ},{5,Q_MAJ},{7,Q_MAJ},{5,Q_MAJ} } },        // I IV V IV
};

// the form: 8 sections of 4 bars (one progression pass each) = 32 bars/song
enum { S_INTRO, S_V, S_C, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_C, S_V, S_C, S_C, S_V, S_OUTRO };
#define SECT_BARS 4
#define SONG_BARS 32

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int      arch;
    int      keyPc;
    Ch       verse[4], chorus[4];
    int      voiceRoll;     // small per-song timbre variation within the archetype
    char     title[24];
    float    freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;                        // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 121.0 };    // schedule-ahead step clock (radio.h)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))

static int   tempo     = 110;
static int   intensity = 1;      // feel: shifts the arrangement density curve
static int   toneSel   = 2;      // mellow/warm/clear/bright (radio.h RAD_TONE*)
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   songCount = 0;

static int   cv[3]     = { 60, 64, 67 };   // comp voice-leading state
static bool  cvInit    = false;
static int   bassLast  = 43;
static int   melPitch  = 72;
static int   kitLag    = 4;
static float vu         = 0;
static int   cellOn[6], cellN = 0;         // melody rhythm cell (seeded, 2 bars)
static char  nowChord[4][12];

static int iabs(int v) { return v < 0 ? -v : v; }
static float TM(void) { return RAD_TONEMUL[toneSel]; }

// ── form / harmony lookups ──────────────────────────────────────────────────
static int sect_of(long bar) { long s = bar / SECT_BARS; return (int)(s < 8 ? FORM[s] : S_OUTRO); }
static const Ch *prog_of(long bar) { return sect_of(bar) == S_C ? sng.chorus : sng.verse; }
static Ch  chord_at(long bar) { return prog_of(bar)[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }
static bool arch_minor(void)  { return sng.arch == A_DANCE || sng.arch == A_FOREVER; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]);
}

// the bass is voice-led: each root lands on the octave copy nearest the last one
static int bass_peek(int pc) { return rad_bass_to(pc, bassLast, 33, 50); }
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

static void lead_voices(Ch c, int lo, int hi) {
    rad_lead_to(root_pc(c), QV[c.q], cv, 3, lo, hi, &cvInit);
}

// the melody walk — roam the key's scale (major/minor per archetype), chord
// tones pull, nearest to the last pitch, clamped to the lead's register window.
static int pick_mel(Ch c, int lo, int hi) {
    const int *sc = arch_minor() ? MINSC : MAJSC;
    int bestM = melPitch, bestScore = -999;
    for (int d = 0; d < 7; d++) {
        int pc = (sng.keyPc + sc[d]) % 12;
        for (int oct = 4; oct <= 8; oct++) {
            int m = oct * 12 + pc;
            if (m < lo || m > hi) continue;
            int score = 12 - iabs(m - melPitch) + rnd(4);
            int rel = (pc - root_pc(c) + 12) % 12;
            if (rel == 0 || rel == 3 || rel == 4 || rel == 7) score += 3;
            if (m == melPitch) score -= 4;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── density: arrangement curve × feel knob (the radio.h convention) ─────────
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_V ? 1 : 2);
    return rad_level(base, intensity);
}

// ── per-archetype voicing — configure exactly the slots this archetype plays ──
// Called from new_song so only the rolled archetype's instruments are set; the
// tone knob scales every cutoff by TM(). Re-called on a T press.
static void voice_for_arch(int arch) {
    float m = TM();
    int vr = sng.voiceRoll;

    // INSTR_VOICE croon — the falsetto. vowel(harmonics) ah/ooh, child SIZE(timbre),
    // EFFORT(morph) soft; a singing vibrato. Used by DANCE/SERENADE/FRIENDS.
    instrument(I_VOX, INSTR_VOICE, 60, 80, 7, 220);
    instrument_harmonics(I_VOX, arch == A_DANCE ? 0.18f : 0.42f);  // ooh vs ahh
    instrument_timbre(I_VOX, 0.66f);                               // small tract = falsetto
    instrument_morph(I_VOX, 0.46f);                                // gentle effort
    instrument_lfo(I_VOX, 0, LFO_PITCH, 5.4f, 0.35f);              // croon vibrato
    instrument_filter(I_VOX, FILTER_LOW, (int)(2600 * m), 1);

    // the jam-ribbon lead (solo.h) — voiced to MATCH the archetype the player jams over,
    // so the ribbon hands you a different instrument per song (the SoloCfg table in draw()
    // gives each its own register / quantize / struck / vertical-drag behavior to match).
    switch (arch) {
    case A_DANCE:                              // bright quantized funk synth lead
        instrument(I_SOLO, INSTR_SQUARE, 4, 150, 6, 160);
        instrument_duty(I_SOLO, 0.26f);
        instrument_filter(I_SOLO, FILTER_LOW, (int)(2800 * m), 4);
        instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.4f, 0.10f);
        instrument_drive(I_SOLO, 0.18f);
        break;
    case A_SERENADE:                           // the SUNG croon — you sing the serenade
        instrument(I_SOLO, INSTR_VOICE, 50, 70, 7, 240);
        instrument_harmonics(I_SOLO, 0.42f);   // ahh
        instrument_timbre(I_SOLO, 0.64f);
        instrument_morph(I_SOLO, 0.46f);
        instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.4f, 0.40f);
        instrument_filter(I_SOLO, FILTER_LOW, (int)(2600 * m), 1);
        break;
    case A_SWIHART:                            // a struck glockenspiel — twinkly, stiff
        instrument(I_SOLO, INSTR_MALLET, 1, 0, 7, 900);
        instrument_harmonics(I_SOLO, 0.90f);
        instrument_timbre(I_SOLO, 0.85f);
        instrument_morph(I_SOLO, 0.60f);
        instrument_filter(I_SOLO, FILTER_LOW, (int)(5000 * m), 1);
        break;
    case A_FOREVER:                            // the anthemic 80s synth lead
        instrument(I_SOLO, INSTR_SQUARE, 6, 160, 6, 200);
        instrument_duty(I_SOLO, 0.30f);
        instrument_filter(I_SOLO, FILTER_LOW, (int)(3000 * m), 3);
        instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.6f, 0.12f);
        break;
    case A_FRIENDS:                            // fingerpicked nylon — plucked, rings on its own
        instrument(I_SOLO, INSTR_GUITAR, 1, 0, 7, 900);
        instrument_harmonics(I_SOLO, 0.45f);
        instrument_timbre(I_SOLO, 0.30f);
        instrument_morph(I_SOLO, 0.35f);
        instrument_filter(I_SOLO, FILTER_LOW, (int)(2600 * m), 1);
        break;
    }

    switch (arch) {
    case A_DANCE: {
        // popping funk slap bass (the strut) — SAW with a bright cut-env snap + grit
        instrument(I_BASS, INSTR_SAW, 1, 150, 4, 70);
        instrument_filter(I_BASS, FILTER_LOW, (int)(900 * m), 4);
        instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 70, 1600);   // the pop
        instrument_drive(I_BASS, 0.22f);
        // wah clavinet — EPIANO clav model + a touch-following auto-wah quack
        instrument(I_COMP, INSTR_EPIANO, 1, 220, 3, 160);
        instrument_harmonics(I_COMP, 0.85f);                   // Clav
        instrument_timbre(I_COMP, 0.72f);
        instrument_morph(I_COMP, 0.5f);
        instrument_filter(I_COMP, FILTER_BAND, (int)(1300 * m), 6);
        instrument_follow(I_COMP, LFO_CUTOFF, 4, 120, 1800);   // play harder → quacks open
        // horn-section stab — real lip-reed brass (bright trumpet macros)
        instrument(I_LEAD, INSTR_BRASS, 1, 0, 4, 220);
        instrument_harmonics(I_LEAD, 0.15f);
        instrument_timbre(I_LEAD, 0.66f + 0.06f * vr);          // blattier on some songs
        instrument_morph(I_LEAD, 0.42f);
        instrument_filter(I_LEAD, FILTER_LOW, (int)(3600 * m), 2);
        break;
    }
    case A_SERENADE: {
        // round soul bass — warm even SINE, no snap
        instrument(I_BASS, INSTR_SINE, 4, 320, 5, 130);
        instrument_filter(I_BASS, FILTER_LOW, (int)(480 * (0.7f + 0.3f * m)), 1);
        // Rhodes — EPIANO Rhodes model, warm
        instrument(I_COMP, INSTR_EPIANO, 2, 0, 6, 900);
        instrument_harmonics(I_COMP, 0.15f);                   // Rhodes
        instrument_timbre(I_COMP, 0.32f);
        instrument_morph(I_COMP, 0.25f);
        instrument_filter(I_COMP, FILTER_LOW, (int)(2300 * m), 2);
        // lush string pad — slow-swell saw, a Solina-ish detuned shimmer
        instrument(I_AUX, INSTR_SAW, 360, 400, 6, 1200);
        instrument_filter(I_AUX, FILTER_LOW, (int)(1800 * m), 2);
        instrument_tune(I_AUX, 0.06f);                          // ensemble shimmer
        instrument_lfo(I_AUX, 0, LFO_VOLUME, 4.4f, 0.10f);     // string-machine chorus
        break;
    }
    case A_SWIHART: {
        // simple square root bass
        instrument(I_BASS, INSTR_SQUARE, 1, 150, 4, 70);
        instrument_duty(I_BASS, 0.5f);
        instrument_filter(I_BASS, FILTER_LOW, (int)(700 * m), 2);
        // the cheap detuned toy-organ MELODY — fat square + wobble + a hair of grit
        instrument(I_LEAD, INSTR_SQUARE, 6, 130, 6, 130);
        instrument_duty(I_LEAD, 0.44f);
        instrument_filter(I_LEAD, FILTER_LOW, (int)(2400 * m), 3);
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.2f, 0.10f);     // cheesy vibrato tab
        instrument_drive(I_LEAD, 0.12f);
        // its detune-twin (the cheap "chorus") — same notes, ~8 cents sharp = beating
        instrument(I_COMP, INSTR_SQUARE, 6, 130, 6, 130);
        instrument_duty(I_COMP, 0.46f);
        instrument_filter(I_COMP, FILTER_LOW, (int)(2200 * m), 3);
        instrument_tune(I_COMP, 0.08f);
        // muted pizzicato guitar doubling the melody
        instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 700);
        instrument_harmonics(I_GTR, 0.20f);
        instrument_timbre(I_GTR, 0.60f);
        instrument_morph(I_GTR, 0.85f);                        // heavy mute = tight pizz
        instrument_filter(I_GTR, FILTER_LOW, (int)(2600 * m), 1);
        // glockenspiel twinkle
        instrument(I_AUX, INSTR_MALLET, 1, 0, 7, 900);
        instrument_harmonics(I_AUX, 0.90f);
        instrument_timbre(I_AUX, 0.85f);
        instrument_morph(I_AUX, 0.60f);
        instrument_filter(I_AUX, FILTER_LOW, (int)(5000 * m), 1);
        break;
    }
    case A_FOREVER: {
        // synth pulse bass — octave 8th saw, round
        instrument(I_BASS, INSTR_SAW, 2, 110, 3, 100);
        instrument_filter(I_BASS, FILTER_LOW, (int)(820 * m), 2);
        // fat saw pad (the 80s wash)
        instrument(I_COMP, INSTR_SAW, 220, 400, 6, 900);
        instrument_filter(I_COMP, FILTER_LOW, (int)(1700 * m), 2);
        instrument_tune(I_COMP, 0.05f);
        instrument_lfo(I_COMP, 0, LFO_VOLUME, 4.2f, 0.08f);
        // anthemic synth lead — PWM square, lightly vibrato'd
        instrument(I_LEAD, INSTR_SQUARE, 8, 150, 5, 150);
        instrument_duty(I_LEAD, 0.30f);
        instrument_filter(I_LEAD, FILTER_LOW, (int)(2800 * m), 3);
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.6f, 0.10f);
        // bright synth arpeggio — short KS pluck on the 16ths
        instrument(I_AUX, INSTR_PLUCK, 0, 200, 0, 120);
        instrument_harmonics(I_AUX, 0.45f);
        instrument_timbre(I_AUX, 0.70f);
        instrument_filter(I_AUX, FILTER_LOW, (int)(3600 * m), 2);
        break;
    }
    case A_FRIENDS: {
        // folk root bass — plain, round SINE, no snap
        instrument(I_BASS, INSTR_SINE, 3, 280, 4, 110);
        instrument_filter(I_BASS, FILTER_LOW, (int)(560 * m), 1);
        // fingerpicked nylon (the star)
        instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 900);
        instrument_harmonics(I_GTR, vr ? 0.55f : 0.45f);       // steel vs nylon
        instrument_timbre(I_GTR, vr ? 0.70f : 0.22f);
        instrument_morph(I_GTR, 0.30f);
        instrument_filter(I_GTR, FILTER_LOW, (int)(2400 * m), 1);
        break;
    }
    }

    // the drum kit — re-tuned per archetype (each branch sets the slots it uses).
    switch (arch) {
    case A_DANCE:
        // four-on-the-floor disco box (house kit), open hat off-beats, clap
        instrument(I_KICK, INSTR_SINE, 0, 90, 0, 60);
        instrument_env(I_KICK, 0, ENV_PITCH, 0, 45, 24);
        instrument_drive(I_KICK, 0.28f);
        instrument(I_SNARE, INSTR_NOISE, 0, 95, 0, 45);
        instrument_filter(I_SNARE, FILTER_BAND, 1900, 4);
        instrument(I_HAT, INSTR_SQUARE, 0, 40, 0, 16);
        instrument_filter(I_HAT, FILTER_HIGH, 7000, 3);
        instrument(I_PERC, INSTR_NOISE, 0, 110, 0, 50);        // clap
        instrument_filter(I_PERC, FILTER_BAND, 1100, 5);
        break;
    case A_SERENADE:
        instrument(I_KICK, INSTR_SINE, 0, 150, 0, 60);         // feathered
        instrument_filter(I_KICK, FILTER_LOW, 220, 1);
        instrument_env(I_KICK, 0, ENV_PITCH, 0, 35, 10);
        instrument(I_PERC, INSTR_NOISE, 0, 40, 0, 24);         // finger-snap
        instrument_filter(I_PERC, FILTER_BAND, 2600, 6);
        instrument(I_HAT, INSTR_NOISE, 0, 20, 0, 14);
        instrument_filter(I_HAT, FILTER_HIGH, 7500, 3);
        break;
    case A_SWIHART:
        instrument(I_KICK, INSTR_SINE, 1, 95, 0, 40);          // stiff boom
        instrument_env(I_KICK, 0, ENV_PITCH, 0, 40, 13);
        instrument(I_SNARE, INSTR_NOISE, 0, 70, 0, 35);        // stiff tap
        instrument_filter(I_SNARE, FILTER_BAND, 1500, 5);
        instrument(I_HAT, INSTR_NOISE, 0, 20, 0, 14);
        instrument_filter(I_HAT, FILTER_HIGH, 7000, 3);
        break;
    case A_FOREVER:
        instrument(I_KICK, INSTR_SINE, 0, 110, 0, 50);
        instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 16);
        instrument_drive(I_KICK, 0.18f);
        instrument(I_SNARE, INSTR_NOISE, 0, 150, 2, 220);      // BIG (faked gated reverb)
        instrument_filter(I_SNARE, FILTER_BAND, 1700, 3);
        instrument(I_HAT, INSTR_NOISE, 0, 16, 0, 24);
        instrument_filter(I_HAT, FILTER_HIGH, 8000, 2);
        break;
    case A_FRIENDS:
        instrument(I_PERC, INSTR_NOISE, 0, 110, 0, 50);        // handclap
        instrument_filter(I_PERC, FILTER_BAND, 1100, 5);
        instrument(I_HAT, INSTR_NOISE, 60, 300, 2, 200);       // soft brush sweep
        instrument_filter(I_HAT, FILTER_HIGH, 4800, 1);
        break;
    }
}

// ── song generation ──────────────────────────────────────────────────────────
static const char *TW[NARCH][6] = {
    { "Sweet Moves", "Lucky", "Skills", "Pull It Off", "Sweet Jumps", "Yesss" },
    { "Always", "For LaFawnduh", "Soul Mate", "I Love You", "Forever", "D-Qwon" },
    { "Tetherball", "Liger", "Quesadilla", "Gosh", "Idaho", "Dang" },
    { "Forever Young", "Glamour Shot", "Time Machine", "Boondoggle", "1982", "Wolves" },
    { "Best Friends", "Tina", "Build a Cake", "Drawing", "A Liger", "Numchucks" },
};

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.arch  = srnd(NARCH);
    sng.keyPc = srnd(12);

    int vi = srnd(3), ci;
    do { ci = srnd(3); } while (ci == vi && srnd(2));   // usually a different chorus
    for (int k = 0; k < 4; k++) {
        sng.verse[k]  = ARCH_PROG[sng.arch][vi][k];
        sng.chorus[k] = ARCH_PROG[sng.arch][ci][k];
    }
    sng.voiceRoll = srnd(2);

    // melody/lead rhythm cell — 3..6 onsets across 2 bars (32 16ths)
    cellN = 0;
    int density = (sng.arch == A_FRIENDS || sng.arch == A_SERENADE) ? 22 : 34;
    for (int s = 0; s < 31 && cellN < 6; s += 2)
        if (srnd(100) < (s % 8 == 0 ? density - 8 : density)) cellOn[cellN++] = s;
    if (cellN < 3) { cellN = 3; cellOn[0] = 0; cellOn[1] = 10; cellOn[2] = 20; }

    snprintf(sng.title, sizeof sng.title, "%s", TW[sng.arch][srnd(6)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    // tempo: each archetype owns a range; the seed varies within it
    static const int TLO[NARCH] = { 120, 64,  90, 102, 84 };
    static const int TSPAN[NARCH] = { 10,  9,  12,  12, 10 };
    tempo = TLO[sng.arch] + srnd(TSPAN[sng.arch]);
    bpm(tempo);

    voice_for_arch(sng.arch);

    songBase = (long)pos + 8;
    cvInit   = false;
    melPitch = (sng.arch == A_FOREVER) ? 76 : (sng.arch == A_FRIENDS ? 67 : 72);
    bassLast = 43;
    kitLag   = rnd_between(2, 7);
    songCount++;
}

static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── lead register windows per archetype ──────────────────────────────────────
static void lead_window(int *lo, int *hi) {
    switch (sng.arch) {
    case A_SERENADE: *lo = 62; *hi = 79; break;   // croon
    case A_SWIHART:  *lo = 67; *hi = 86; break;   // toy organ
    case A_FOREVER:  *lo = 72; *hi = 90; break;   // synth lead
    case A_FRIENDS:  *lo = 62; *hi = 79; break;   // sung voice
    default:         *lo = 67; *hi = 84; break;
    }
}

// ── the step players (one per archetype) ──────────────────────────────────────
static void comp_chord(Ch c, int dly, int slot, int lo, int hi, int vol, int durMs, int spread) {
    lead_voices(c, lo, hi);
    for (int k = 0; k < 3; k++)
        schedule_hit(dly + k * spread + rnd(5), cv[k], slot, vol, durMs);
    vu += 1.8f;
}

static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= SONG_BARS) return;                  // song over; update() rolls the next
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  s32  = (int)(s % 32);
    int  rootMidi = bass_near(root_pc(c));
    int  lo, hi; lead_window(&lo, &hi);

    // gentle tape breath at each section
    if (step == 0 && bar % SECT_BARS == 0 && bar > 0) bpm(tempo + rnd(3) - 1);

    switch (sng.arch) {

    // ── DANCE — four-on-the-floor acid-funk ──────────────────────────────────
    case A_DANCE: {
        // KICK every beat
        if (step % 4 == 0) { schedule_hit(dly, 36, I_KICK, 5, 90); vu += 1.6f; }
        // SNARE/clap backbeat on 2 & 4
        if (step == 4 || step == 12) {
            schedule_hit(dly + kitLag, 40, I_SNARE, 4, 90);
            schedule_hit(dly + kitLag + 4, 50, I_PERC, 3, 100);
            vu += 1.6f;
        }
        // HAT: closed 16ths, open on the off-beat "tss"
        if (lvl >= 1 && step % 2 == 0) {
            int open = (step % 4 == 2);
            schedule_hit(dly + 2, 80, I_HAT, open ? 3 : 1, open ? 70 : 16);
        }
        // BASS: popping syncopated funk 16ths (root + octave pops + ghosts)
        {
            bool bhit = (step == 0 || step == 3 || step == 6 || step == 10 || step == 11 || step == 14);
            int n = rootMidi, vol = 4;
            if (step == 6 || step == 11) { n = rootMidi + 12; vol = 3; }   // octave pop
            if (step == 3 || step == 14) vol = 2;                          // ghost
            if (step == 14 && !chance(60)) bhit = false;
            if (bhit) {
                schedule_hit(dly + rnd(4), n, I_BASS, vol, (int)(stepMs * 1.3));
                vu += vol * 0.5f;
            }
        }
        // COMP: wah-clav 16th chord chucks
        if (lvl >= 1 && (step % 2 == 1 || step == 0) && !chance(step == 0 ? 0 : 22)) {
            lead_voices(c, 55, 76);
            int v = cv[(step / 2) % 3];
            schedule_hit(dly + rnd(4), v, I_COMP, step == 0 ? 4 : 2, (int)(stepMs * 0.9));
            vu += 0.7f;
        }
        // LEAD: horn-section stabs on the accents (lvl 2+)
        if (lvl >= 2 && (step == 0 || (step == 6 && chance(60)) || (step == 10 && chance(40)))) {
            lead_voices(c, 64, 84);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + 2, cv[k], I_LEAD, 4, (int)(stepMs * (step == 0 ? 2.2 : 1.1)));
            vu += 2.4f;
        }
        // VOX: falsetto "ooh" on chord changes (lvl 3)
        if (lvl >= 3 && step == 0 && (bar % 2 == 0) && chance(70)) {
            int n = (root_pc(c) + 12 * 6) + QT[c.q][2] % 12;   // a high chord-tone "ooh"
            while (n < 74) n += 12; while (n > 86) n -= 12;
            schedule_hit(dly + 8, n, I_VOX, 3, (int)(stepMs * 6));
            vu += 1.6f;
        }
        break;
    }

    // ── SERENADE — slow quiet-storm soul ballad ──────────────────────────────
    case A_SERENADE: {
        if (step == 0) { schedule_hit(dly, 36, I_KICK, 3, 150); vu += 1.1f; }   // soft pulse
        if (step == 8) schedule_hit(dly + 4, 36, I_KICK, 2, 130);
        if (step == 4 || step == 12) {                          // finger-snaps on 2 & 4
            schedule_hit(dly + kitLag, 60, I_PERC, 3, 30); vu += 1.0f;
        }
        if (lvl >= 2 && step % 4 == 2) schedule_hit(dly + 2, 84, I_HAT, 1, 14);
        // BASS: root on 1, fifth/approach on the & of 2
        if (step == 0) { schedule_hit(dly, rootMidi, I_BASS, 4, (int)(stepMs * 7)); vu += 2.0f; }
        if (step == 10 && chance(60)) {
            int fifth = rootMidi + 7 <= 50 ? rootMidi + 7 : rootMidi - 5;
            schedule_hit(dly, fifth, I_BASS, 3, (int)(stepMs * 5));
        }
        // COMP Rhodes: a lush held chord at the top of each bar
        if (step == 0) comp_chord(c, dly, I_COMP, 52, 74, 3, (int)(stepMs * 14), 26);
        // AUX strings: a slow pad swell, held the whole bar (lvl 1+)
        if (lvl >= 1 && step == 0) comp_chord(c, dly, I_AUX, 55, 76, 2, (int)(stepMs * 15), 0);
        // VOX croon: the SUNG lead — the seeded cell, long sustained notes (the star).
        // Yields to the player when the jam ribbon is open.
        if (lvl >= 2 && !solo_open()) {
            for (int i = 0; i < cellN; i++)
                if (cellOn[i] == s32 && chance(88)) {
                    int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.9); if (dur > 2600) dur = 2600;
                    schedule_hit(dly + 10 + rnd(8), pick_mel(c, lo, hi), I_VOX, 4, dur);
                    vu += 1.8f;
                }
        }
        break;
    }

    // ── SWIHART — stiff straight-8 deadpan score ──────────────────────────────
    case A_SWIHART: {
        if (step == 0 || step == 8) { schedule_hit(dly, 36, I_KICK, 4, 90); vu += 1.4f; } // boom
        if (step == 4 || step == 12) { schedule_hit(dly, 40, I_SNARE, 3, 70); vu += 1.4f; } // tap
        if (lvl >= 1 && step % 2 == 0)                          // straight 8ths, NO swing
            schedule_hit(dly, 84, I_HAT, (step % 8 == 0) ? 2 : 1, 14);
        // BASS: square root on the beat, fifth on 3
        if (step == 0 || step == 8) { schedule_hit(dly, rootMidi, I_BASS, 4, (int)(stepMs * 3.5)); vu += 1.2f; }
        if (step == 12 && chance(50)) {
            int fifth = rootMidi + 7 <= 50 ? rootMidi + 7 : rootMidi - 5;
            schedule_hit(dly, fifth, I_BASS, 3, (int)(stepMs * 3));
        }
        // toy-organ MELODY (+ its detune twin + the pizz double) on the seeded cell
        if (lvl >= 1 && !solo_open()) {
            for (int i = 0; i < cellN; i++)
                if (cellOn[i] == s32) {
                    int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.92); if (dur > 1400) dur = 1400;
                    int n = pick_mel(c, lo, hi);
                    schedule_hit(dly + rnd(4), n, I_LEAD, 4, dur);
                    schedule_hit(dly + rnd(4), n, I_COMP, 3, dur);          // detune twin
                    if (lvl >= 2) schedule_hit(dly + 2 + rnd(4), n, I_GTR, 3, (int)(stepMs * 1.5)); // pizz
                    vu += 2.0f;
                }
        }
        // glockenspiel twinkle — a sparse high chord tone on the off-bars
        if (lvl >= 2 && step == 8 && chance(60)) {
            int g = root_pc(c) + 12 * 7 + 0; while (g > 96) g -= 12;
            schedule_hit(dly + 6, g, I_AUX, 2, 700); vu += 0.9f;
        }
        break;
    }

    // ── FOREVER — anthemic half-time 80s synth-pop ────────────────────────────
    case A_FOREVER: {
        if (step == 0) { schedule_hit(dly, 36, I_KICK, 5, 110); vu += 1.6f; }
        if (step == 8) { schedule_hit(dly + kitLag, 40, I_SNARE, 5, 220); vu += 2.0f; }  // BIG gated
        if (lvl >= 1 && step % 2 == 0) schedule_hit(dly, 82, I_HAT, (step % 8 == 4) ? 2 : 1, 18);
        // BASS: octave 8th pulse
        if (step % 2 == 0) {
            int n = (step % 4 == 0) ? rootMidi : rootMidi + 12;
            schedule_hit(dly + rnd(3), n, I_BASS, step % 4 == 0 ? 4 : 3, (int)(stepMs * 1.6));
            vu += 1.0f;
        }
        // COMP pad: big held chord per bar
        if (step == 0) comp_chord(c, dly, I_COMP, 52, 76, 4, (int)(stepMs * 15), 8);
        // AUX arp: bright 16th arpeggio through the chord tones (lvl 1+)
        if (lvl >= 1 && step % 2 == 0) {
            lead_voices(c, 67, 88);
            int n = cv[(step / 2) % 3] + (step % 8 >= 4 ? 12 : 0);
            while (n > 94) n -= 12;
            schedule_hit(dly + rnd(3), n, I_AUX, 2, (int)(stepMs * 1.0));
            vu += 0.6f;
        }
        // LEAD: the anthemic topline on the seeded cell (lvl 2+) — yields to the jam ribbon
        if (lvl >= 2 && !solo_open()) {
            for (int i = 0; i < cellN; i++)
                if (cellOn[i] == s32 && chance(85)) {
                    int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.9); if (dur > 1600) dur = 1600;
                    schedule_hit(dly + 8 + rnd(6), pick_mel(c, lo, hi), I_LEAD, 4, dur);
                    vu += 1.7f;
                }
        }
        break;
    }

    // ── FRIENDS — sparse fingerpicked folk ────────────────────────────────────
    case A_FRIENDS: {
        // very sparse: brush + handclap, lots of space
        if (lvl >= 1 && (step == 4 || step == 12)) { schedule_hit(dly + kitLag, 50, I_PERC, 2, 90); vu += 0.9f; }
        if (lvl >= 2 && step % 4 == 2) schedule_hit(dly, 80, I_HAT, 1, 120);   // soft brush
        // BASS: folk root on 1, fifth on 3
        if (step == 0) { schedule_hit(dly, rootMidi, I_BASS, 4, (int)(stepMs * 6)); vu += 1.6f; }
        if (step == 8 && chance(70)) {
            int fifth = rootMidi + 7 <= 50 ? rootMidi + 7 : rootMidi - 5;
            schedule_hit(dly, fifth, I_BASS, 3, (int)(stepMs * 5));
        }
        // GTR: fingerpicked nylon arpeggio on the 8ths (the star)
        static const int PICK[8] = { 0, 2, 1, 2, 0, 2, 1, 2 };
        if (step % 2 == 0 && !chance(lvl >= 2 ? 6 : 16)) {
            lead_voices(c, 55, 79);
            int idx = PICK[(step / 2) % 8] % 3;
            schedule_hit(dly + rnd(6), cv[idx], I_GTR, (step == 0) ? 4 : 3, (int)(stepMs * 3));
            vu += 0.9f;
        }
        // VOX: a single sung voice on the seeded cell (lvl 2+) — yields to the jam ribbon
        if (lvl >= 2 && !solo_open()) {
            for (int i = 0; i < cellN; i++)
                if (cellOn[i] == s32 && chance(82)) {
                    int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.85); if (dur > 1800) dur = 1800;
                    schedule_hit(dly + 10, pick_mel(c, lo, hi), I_VOX, 3, dur);
                    vu += 1.4f;
                }
        }
        break;
    }
    }
}

// ── update ─────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        if (NAPOLEON_SEED) { new_song(pos, NAPOLEON_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 60, 140, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) note_off_all(); else scheduled = (long)pos; }
    if (ev & RAD_EV_TONE)   voice_for_arch(sng.arch);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= (long)SONG_BARS * 16) fresh_song(pos);

        long bar = songStep >= 0 ? songStep / 16 : 0;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 12, p[i]);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
    watch("song", "%d", songCount);
    watch("arch", "%s", ARCHN[sng.arch]);
    watch("sect", "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[(int)(bar % 4)]);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — the thrift-store radio on the Preston, Idaho windowsill ───────────
void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  ci  = (int)(bar % 4);

    rad_body(CLR_MEDIUM_GREY, CLR_ORANGE);     // tan thrift plastic, orange accent
    rad_dial(sng.freq, CLR_ORANGE);

    // the window — a flat Idaho field, mountains, a tetherball pole
    rectfill(34, 52, 102, 116, CLR_LIGHT_PEACH);                 // pale dusty sky
    for (int i = 0; i < 4; i++) {                                // mountains
        int mx = 34 + i * 30;
        for (int t = 0; t < 14; t++)
            line(mx + t, 118 - t, mx + 28 - t, 118 - t, CLR_INDIGO);
    }
    rectfill(34, 118, 102, 50, CLR_BROWN);                       // the brown field
    rectfill(34, 118, 102, 4, CLR_DARK_GREEN);                   // a strip of grass
    // tetherball pole + ball, the ball swinging on the beat
    line(70, 92, 70, 140, CLR_LIGHT_GREY);
    {
        float sw = sinf((frame() * 0.06f)) * (radioOn ? 18.0f : 2.0f);
        int bx = 70 + (int)sw, by = 100 + (int)(fabsf(sw) * 0.5f);
        line(70, 94, bx, by, CLR_DARK_GREY);
        circfill(bx, by, 3, CLR_DARK_ORANGE);
    }
    // a tiny "VOTE 4 PEDRO" flyer taped in the corner
    rectfill(40, 150, 30, 14, CLR_WHITE);
    font(FONT_TINY);
    print("VOTE", 42, 152, CLR_DARK_RED);
    print("PEDRO", 42, 158, CLR_DARK_RED);
    font(FONT_NORMAL);
    rect(34, 52, 102, 116, CLR_DARK_BROWN);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_GREY);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        font(FONT_SMALL);
        char l2[40];
        snprintf(l2, 40, "%s - %s", ARCHN[sng.arch], ARCHDESC[sng.arch]);
        print(l2, 154, 70, CLR_LIGHT_YELLOW);
        snprintf(l2, 40, "%.1f FM  key %s  %dbpm", sng.freq, RAD_PCNAME[sng.keyPc], tempo);
        print(l2, 154, 78, CLR_DARK_PEACH);
        snprintf(l2, 40, "#%08X", sng.seed);
        print(l2, 154, 86, CLR_DARK_PEACH);
        font(FONT_NORMAL);
        // the active progression, current chord boxed
        rad_chord_row(nowChord, 4, ci, 152, 100, CLR_ORANGE);
        static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
        font(FONT_SMALL);
        print(SN[sect_of(bar)], 152, 116, CLR_ORANGE);
        font(FONT_NORMAL);
        rad_phrase_dots(206, 119, 8, bar / SECT_BARS, CLR_ORANGE);
        float vt = vu / 12.0f;
        rectfill(154, 64, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // knobs + power LED
    static const char *FEEL[4] = { "quiet", "easy", "lively", "full" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 60, 140, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE))
        voice_for_arch(sng.arch);
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_ORANGE);
    if (showHelp) {
        static const char *HELP[9][2] = {
            { "SPACE",      "next song (rolls a new one)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - density of the arrangement" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
            { "L",          "jam ch/sc: chord-lock / free" },
        };
        static const char *NOTES[3] = {
            "five different songs of theirs - SPACE to roll one.",
            "the dance, kip's serenade, the score, forever, friends.",
            "the #number on the display IS the song.",
        };
        rad_help_panel("NAPOLEON RADIO", HELP, 9, NOTES, 3, CLR_ORANGE);
    }

    // the jam ribbon — play along on the song's scale, the chord tones lit. The voice
    // AND the behavior change per archetype (croon on the serenade, struck glock on the
    // score, quantized funk lead on the dance…). vertical drag = brightness or volume.
    if (radioOn) {
        int chordpcs[4]; {
            Ch c = chord_at(bar);
            for (int k = 0; k < 4; k++) chordpcs[k] = (root_pc(c) + QT[c.q][k]) % 12;
        }
        static const int PENT_MAJ[5] = { 0, 2, 4, 7, 9 };
        static const int PENT_MIN[5] = { 0, 3, 5, 7, 10 };
        const int *pent = arch_minor() ? PENT_MIN : PENT_MAJ;
        const SoloCfg *sc = &SOLOCFG[sng.arch];
        SoloCtx jc = { sng.keyPc, pent, 5, chordpcs, 4, I_SOLO,
                       sc->lo, sc->hi, sc->quant, sc->ymode, sc->ymin, sc->ymax, sc->struck };
        solo_strip(&jc, 28, 170, 250, 18, CLR_ORANGE);
    }

    ui_end();
}
