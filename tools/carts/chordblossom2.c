/* de:meta
{
  "title": "Chord Blossom 2 (flavors)",
  "slug": "chordblossom2",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "step-sequencer"
  ],
  "homage": "Telepathic Instruments Orchid (ORC-1, 2026) — the chord-generating synth co-founded by Tame Impala's Kevin Parker; itself in the Suzuki Omnichord / '80s home-keyboard lineage.",
  "lineage": "A FORK of chordblossom adding genre FLAVORS (docs/design/bossa-rack.md — generalizing flavors to chordblossom): the same play-chords-with-your-hands instrument, but a selectable FLAVOR sets a backing-band personality (comp rhythm + voicing + timbre + groove) so it plays a genre. NEUTRAL = vanilla chordblossom; BOSSA = clave comp + rootless 3-7-9 voicing + nylon + the bossa groove — the band follows your hands. Truthful homage to the Telepathic Instruments Orchid: you play CHORDS, not notes. A one-octave keybed picks the root, four TYPE buttons + four combinable MODIFIER buttons build the quality, and the signature VOICING dial cascades the chord one note at a time through inversions and octave-spreads across the keyboard. The real Orchid is a deliberately minimal SONGWRITING idea-machine + 3-part MIDI brain (it sends chords / lead / bass on separate MIDI channels), with ~50 curated non-editable presets across three engines (virtual-analog / FM / reed-EP), a separate sub-bass engine with Follow/Solo, TWO voicing dials (lead AND bass), and a Key mode for diatonic auto-harmony — its depth is the chord-logic + gorgeous sounds, not synthesis or long sequencing (reviewers dispute the drum machine/looper). Distinct from the repo's existing Suzuki-Omnichord tribute (omnichord/Strumharpy): keybed-root + type/modifier logic + the voicing cascade are the Orchid's own thing.",
  "description": {
    "summary": "EXPERIMENT (fork of chordblossom): a genre-FLAVOR system — pick a flavor (NEUTRAL / BOSSA / YACHT / CITYPOP; V key or the chip) and a backing band follows your hands (auto-colour + comp rhythm + voicing + timbre + groove — all a data row in the Flavor table). Underneath, a chord-generating synth after the Telepathic Instruments Orchid — pick a root on the one-octave keybed, stack chord TYPE + MODIFIER buttons, and turn the VOICING dial to cascade the chord through inversions and octave-spreads. A six-model synth shelf (reed EP / subtractive / FM / pluck / mallet / guitar) with an independent model pick for the CHORD voice (PNO) and the strummable SONIC STRINGS (HRP), a following sub-bass, reverb+chorus, five performance modes (strum/harp/arp/pattern/slop), a drum machine and a real-time chord looper.",
    "detail": "Three tabs across the top bar (or TAB): CHORD, MIX, RHYTHM. CHORD is the instrument: the keybed at the bottom (tap or the GarageBand keys A S D F G H J for the white roots, W E T Y U for the blacks) sets the ROOT and fires the chord. The upper button row picks ONE chord TYPE (dim / min / maj / sus4); the lower row toggles MODIFIERS that stack freely (6th, m7, maj7, 9th). The VOICING strip (arrow keys or drag) is the star: each step shifts the whole chord up or down by one chord-tone, so it cascades through every inversion and octave spread — turn it while a chord rings and hear it re-voice live, exactly the Orchid's patent-pending trick. A rainbow SONIC STRINGS plate above the keybed is always strummable (mouse or multi-finger) as a harp glissando over the current chord. Two selectors in the top row set which synth model voices each part — tap PNO to cycle the chord (piano) model and HRP to cycle the strings model, both drawing from the same six-model shelf. MIX is the Orchid's nine-knob top row (Sound=engine, Perform=mode, FX, Key/transpose, Bass, Loop mix, BPM, Options=strum tightness, Volume). RHYTHM is a 16-step drum machine (kick/snare/hat/ohat/clap + a BASS row that follows the chord root, six preset grooves) plus a real-time CHORD LOOPER: arm REC and every chord you play is captured to a 4-bar loop that plays back so you can jam over yourself. SPACE = transport play/stop.",
    "controls": "keybed roots: A S D F G H J (white C-B) + W E T Y U (black), or play a USB MIDI keyboard (a note picks the root + fires the chord); Z/X = octave down/up; TYPE: tap or 1-4; MODIFIERS: tap or 5-8 (combinable); tap PNO / HRP (top row) to cycle the chord-voice / strings model; LEAD VOICING: LEFT/RIGHT arrows or drag the strip; BASS VOICING walk: UP/DOWN arrows (steps the following bass through the chord tones); strum the rainbow plate with mouse/fingers; TAB = switch tab; SPACE = drum transport play/stop. MIX tab: turn the nine knobs. RHYTHM tab: tap the grid, 1-6 = preset grooves, REC/PLAY/CLEAR the looper."
  },
  "todo": [
    "Use BEAUTIFUL instrument presets, not the bare defaults — voice the three engines from the curated recipes (docs/guides/instrument-recipes.md + instrument-presets.md) so SUB/FM/EP each sound gorgeous, instead of raw INSTR_SAW/FM/EPIANO with basic envelopes.",
    "Fewer knobs, one page: some MIX knobs aren't needed. Put the essentials — chord play + the knobs that matter + a way to trigger the live loop — ALL on a single page, instead of the three CHORD/MIX/RHYTHM tabs. Model the bass knobs/XY on onenote (per more-note-bass's own todo).",
    "Make it responsive / device-adaptive (lay.h + docs/guides/responsive-instrument-ui.md) so the layout fits phone and desktop.",
    "Self-playing BASS: an auto walking-bassline that moves on its own under the groove (a bass sequencer lane à la more-note-bass, or a generative walk), so you don't have to step it by hand — the 'bass playing by itself' feel.",
    "Bass SOLO mode: play a truly independent bassline off the keybed (the real Orchid's long-press-Bass 'take the bassline for a walk'), separate from the chord root. Follow=root + the manual UP/DOWN bass-voicing walk are already in.",
    "Two voicing DIALS on the panel: give LEAD and BASS each a proper on-screen dial (the walk logic for both already exists — this is the UI surface, matching the Orchid's two physical dials).",
    "Key mode: pick a key and map a diatonic, in-key chord to each keybed key (press the white keys, get musically-appropriate chords in that key) — the Orchid's beginner-friendly auto-harmony.",
    "Live re-voice of a HELD chord: turning the lead voicing dial while a chord rings should GLIDE the held notes to the new voicing (note_pitch, no re-attack) — a third option beyond the current RETRIG toggle (silent vs re-strike)."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <stddef.h>

// ============================================================
//  CHORDBLOSSOM — a truthful homage to the Telepathic Instruments Orchid.
//  You play CHORDS, not notes. Keybed picks the ROOT; four TYPE buttons and
//  four combinable MODIFIER buttons build the quality; the VOICING dial cascades
//  the chord one chord-tone at a time through inversions + octave spreads.
//
//  Three tabs share one top bar (TAB switches, the tabs up top are clickable):
//    CHORD  — the instrument: keybed + type/modifier buttons + voicing strip +
//             engine/perform selectors + a strummable sonic-strings plate.
//    MIX    — the Orchid's nine-knob top row.
//    RHYTHM — a 16-step drum machine + a real-time chord looper.
// ============================================================

#define TAB_CHORD  0
#define TAB_MIX    1
#define TAB_RHYTHM 2

// instrument slots (5..31)
#define SL_CHORD 5     // the polyphonic chord voice — engine swapped live
#define SL_BASS  6     // the following mono sub-bass
#define SL_HARP  7     // sonic-strings plate (soft plucked bell)

// ── chord model ────────────────────────────────────────────
// four TYPE triads (semitone intervals from the root), one selected at a time.
#define TY_DIM 0
#define TY_MIN 1
#define TY_MAJ 2
#define TY_SUS 3
static const int TRIAD[4][3] = {
    { 0, 3, 6 },   // dim
    { 0, 3, 7 },   // min
    { 0, 4, 7 },   // maj
    { 0, 5, 7 },   // sus4
};
static const char *TYNAME[4] = { "dim", "min", "MAJ", "sus4" };

// four MODIFIERS, freely combinable — each adds a colour tone (semitones).
#define NMOD 4
static const int   MODIV [NMOD] = { 9, 10, 11, 14 };        // 6th, m7, maj7, 9th
static const char *MODNAME[NMOD] = { "6", "m7", "maj7", "9" };

// diatonic KEY mode — the 7 white keys become the in-key chords (major-key harmony,
// SHARED across flavors; the flavor's colour/voicing/rhythm is what makes them differ).
static const int DEG_OFF [7] = { 0, 2, 4, 5, 7, 9, 11 };    // scale-degree semitone offsets
static const int DEG_TYPE[7] = { TY_MAJ, TY_MIN, TY_MIN, TY_MAJ, TY_MAJ, TY_MIN, TY_DIM };
static const int DEG_MOD [7] = { 2, 1, 1, 2, 1, 1, 1 };     // 7th: maj7(I,IV) · m7(ii,iii,V=dom,vi,vii=m7b5)

// the SPICE STRIP — the genre's out-of-key chords, each a KEY you press (one model, no
// arming). {semitone offset from the key root, triad type, 7th modifier idx}.
typedef struct { int off, ty, sev; } Spice;
#define NSPICE 6
static const Spice SPICE[NSPICE] = {
    { 9, TY_MAJ, 1 },   // V7/ii   (A7 in C) — secondary dominant
    { 2, TY_MAJ, 1 },   // V7/V    (D7)
    { 4, TY_MAJ, 1 },   // V7/vi   (E7)
    { 1, TY_MAJ, 1 },   // subV    (Db7)  — tritone sub of V
    {10, TY_MAJ, 1 },   // bVII    (Bb7)  — backdoor
    { 5, TY_MIN, 1 },   // ivm     (Fm7)  — IV -> IVm
};

// three synth engines, the Orchid's three models.
#define NENG 6            // the curated MODEL shelf — both the PIANO and the HARP part pick one
// NOTE: play through the SL_CHORD / SL_HARP *slots* (configured by applyEngine),
// never a raw INSTR_ id — INSTR_FM/EPIANO are 18/20, which collide with the
// 5..31 user-slot range, so note(midi, INSTR_EPIANO,..) means "slot 20" = silent.
typedef struct { int instr; const char *name; const char *shortName; int a, d, s, r; int cut, res; } Model;
static const Model MODEL[NENG] = {
    { INSTR_EPIANO, "REED EP",  "EP",  1, 300, 3, 320, 3200, 2 },  // warm Rhodes — the default piano
    { INSTR_SAW,    "SUBTRACT", "SUB", 4, 220, 5, 320, 2400, 4 },  // analog pad
    { INSTR_FM,     "FM",       "FM",  2, 240, 4, 300, 4000, 2 },  // DX keys / bells
    { INSTR_PLUCK,  "PLUCK",    "PLK", 1, 260, 0, 300, 3600, 3 },  // koto / harp string
    { INSTR_MALLET, "MALLET",   "MLT", 1, 320, 0, 340, 4200, 1 },  // marimba / celesta bell
    { INSTR_GUITAR, "GUITAR",   "GTR", 1, 300, 0, 300, 3000, 3 },  // nylon guitar / harp
};

// five performance modes.
#define PM_PLAIN   0
#define PM_STRUM   1
#define PM_HARP    2
#define PM_ARP     3
#define PM_PATTERN 4
#define PM_SLOP    5
#define NPERF 6
static const char *PMNAME[NPERF] = { "PLAY", "STRUM", "HARP", "ARP", "PATTERN", "SLOP" };

static const char *NOTE[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

// ── keybed (one octave, GarageBand musical-typing map) ──────
static const int WROOT[7] = { 'A','S','D','F','G','H','J' };
static const int WPC  [7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int BROOT[5] = { 'W','E','T','Y','U' };
static const int BPC  [5] = { 1, 3, 6, 8, 10 };

// ── strum plate ─────────────────────────────────────────────
#define PLATE_X  6
#define PLATE_Y  92
#define PLATE_W  (SCREEN_W - 12)
#define PLATE_H  44
#define STRUM_MS 22
static const int STRCOL[7] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO, CLR_PINK };

// ── drum machine (from omnichord/drummachine chassis) ───────
#define ROWS  6
#define STEPS 16
#define GX 46
#define GY 24
#define SX 17
#define SY 20
#define CW 15
#define CH 17
static const char *DLABEL[ROWS] = { "KICK","SNARE","HIHAT","OHAT","CLAP","BASS" };
static const int   DLIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_PINK, CLR_BLUE };
#define NPRESET 6
static const char *PNAME[NPRESET] = { "ROCK","BOSSA","CHA","SWING","DISCO","MARCH" };
static const char *PATTERN[NPRESET][ROWS] = {
    { "x...x...x..x....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "x.......x......." },
    { "x..x..x...x..x..", "................", "x.x.x.x.x.x.x.x.", "................", "..x.x...x..x.x..", "x..x..x...x..x.." },
    { "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "............x.xx", "x...x...x...x..." },
    { "x.......x.......", "....x.......x...", "x..x..x..x..x..x", "................", "................", "x..x..x..x..x..x" },
    { "x...x...x...x...", "................", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "....x.......x...", "x.x.x.x.x.x.x.x." },
    { "x...x...x...x...", "..x...x...x...x.", "................", "................", "................", "x...x...x...x..." },
};

// ── genre FLAVORS (the fork) — a backing-band personality over the SAME
// instrument: comp rhythm + voicing + timbre + groove. NEUTRAL = vanilla
// chordblossom; BOSSA = clave comp + rootless 3-7-9 + nylon + the bossa groove
// (the band follows your hands). docs/design/bossa-rack.md.
// comp + bass onset patterns over a 2-bar (32 sixteenth) cycle, -1 terminated.
static const int CMP_BOSSA[] = { 0, 6, 12, 20, 26, -1 };              // the João clave
static const int BAS_BOSSA[] = { 0, 8, 16, 24, -1 };                  // surdo
static const int CMP_YACHT[] = { 0, 6, 10, 16, 22, 26, -1 };          // laid-back EP with anticipations
static const int BAS_YACHT[] = { 0, 8, 14, 16, 24, 30, -1 };          // fingered electric, a little walk
static const int CMP_CITY[]  = { 0, 6, 10, 14, 16, 22, 26, 30, -1 };  // bright 16-beat cutting
static const int BAS_CITY[]  = { 0, 6, 8, 14, 16, 22, 24, 30, -1 };   // slap / octave pops

typedef struct {
    const char *name;
    const int  *comp;      // comp onsets over 32 steps (NULL = use the generic perfMode path)
    const int  *bassOn;    // bass onsets over 32 steps
    int  perfMode;         // performance mode (NEUTRAL; flavored carts use comp/bassOn)
    int  rootless;         // drop the root from the voiced chord (the bass owns it)
    int  rich;             // default RICHNESS for this flavor (0 simple .. 3 lush)
    int  eng;              // chord MODEL index (timbre)
    int  drumPreset;       // backing groove
    int  tempo;
    int  strumMs;          // feel (strum spread)
    bool autoRun;          // start the groove so the band plays under you
} Flavor;
#define NFLAV 4
enum { FL_NEUTRAL, FL_BOSSA, FL_YACHT, FL_CITY };
static const Flavor FLAVORS[NFLAV] = {
    //  name       comp        bassOn      perf        rl rich eng drum tempo strum autorun
    { "NEUTRAL",   NULL,       NULL,       PM_STRUM,   0, 0, 0, 0,  96, 22, false },
    { "BOSSA",     CMP_BOSSA,  BAS_BOSSA,  PM_PATTERN, 1, 2, 5, 1, 128, 12, true  },
    { "YACHT",     CMP_YACHT,  BAS_YACHT,  PM_PATTERN, 1, 3, 0, 3,  92, 18, true  },
    { "CITYPOP",   CMP_CITY,   BAS_CITY,   PM_PATTERN, 1, 3, 0, 4, 112,  8, true  },
};

// ── state ───────────────────────────────────────────────────
static int  tab      = TAB_CHORD;
static int  root     = 0;                 // 0..11
static int  chType   = TY_MAJ;
static bool modOn[NMOD] = { false, false, false, false };
static int  voicing  = 0;                 // the cascade offset (signature dial)
static int  octave   = 0;                 // Z/X keybed octave shift (-2..+2), whole chord
static bool retrig   = false;             // RETRIG toggle: re-play the chord on every param change?
static int  bassVoi  = 0;                 // BASS voicing walk: 0=root, 1=3rd, 2=5th, then up an octave… (the Orchid's bass dial)
static int  engine   = 0;                 // PIANO model — index into MODEL[]; start on reed EP (warm)
static int  harpModel = 3;                // HARP (sonic-strings) model — index into MODEL[]; start on PLUCK
static int  perfMode = PM_STRUM;
static int  transpose = 0;                // MIX "Key" knob, -12..+12 semis
static bool armed    = false;             // a chord has been triggered at least once
static int  flavor   = FL_NEUTRAL;        // the active genre flavor (the fork)
static int  rootless = 0;                 // flavor: drop the root from the voiced chord
static int  keyRoot  = 0;                 // KEY mode: the scale root (0..11)
static int  keyMode  = 0;                 // KEY mode: white keys play the diatonic in-key chords
static int  richness = 0;                 // sound: 0 simple(triad) · 1 +7th · 2 +9th · 3 lush(+13/6)
static int  groove   = 1;                 // performance density: 0 sparse · 1 normal · 2 busy (flavor-aware)

// ── the tiny per-part PEDALBOARD (per-slot FX, set-and-hold) ─────────────────
enum { PD_REVERB, PD_DELAY, PD_CHORUS, PD_DRIVE };
typedef struct { const char *lbl; int slot, kind; bool on; float amt, applied; } Pedal;
static Pedal pedals[] = {
    { "RVB", SL_HARP, PD_REVERB, false, 0.40f, -1 },   // HARP row
    { "DLY", SL_HARP, PD_DELAY,  false, 0.30f, -1 },
    { "CHO", SL_HARP, PD_CHORUS, false, 0.50f, -1 },
    { "DRV", SL_BASS, PD_DRIVE,  false, 0.35f, -1 },   // BASS row
    { "CHO", SL_BASS, PD_CHORUS, false, 0.40f, -1 },
};
#define NPED 5

// the built voiced chord (absolute MIDI notes, ascending)
static int  voiced[12], nVoiced = 0;
static int  heldH[12],  nHeld   = 0;      // live handles for PLAY (held-pad) mode

// strum plate strings (chord tones across ~5 octaves)
static int   strNote[40], nStr = 0;
static float litT[40];
#define NFINGER 10
#define NOFINGER (-999)
static int strId[NFINGER], strLast[NFINGER];

// mix knobs (0..1 floats behind ui_knob)
static float kSound = 0.08f, kPerform = 0.2f, kFX = 0.35f, kKey = 0.5f;   // kSound 0.08 -> piano model 0 (EP), matching the default
static float kBass = 0.7f, kLoop = 0.7f, kBPM = 0.25f, kOptions = 0.3f, kVolume = 0.75f;
static float appliedBPM = 0.25f;          // the kBPM value last turned into tempo (change-detect, so the FX tab never clobbers the flavor's tempo)

// derived / applied
static int   tempo    = 96;
static int   masterV  = 5;                // 1..7 note volume
static int   strumMs  = 22;               // OPTIONS knob: strum stagger 8..60ms
static float drumMix  = 0.7f;             // LOOP knob: backing drum level 0..1
static float appRev = -1, appCho = -1;    // fx set-and-hold guards

// drums
static bool grid[ROWS][STEPS];
static int  curPreset = 0, cur_step = 0, last_16 = -1, flash[ROWS];
static bool playing = false;

// arp / pattern
static int  arpIdx = 0;
static const char *PAT_RHYTHM = "x.x.x..xx.x.x.x.";   // pattern-mode chord-stab rhythm

// ── chord looper ────────────────────────────────────────────
#define LOOP_16 64                        // 4 bars of 16ths
typedef struct { int pos, root, ty, mods, voi; } LoopEv;
#define NLOOP 64
static LoopEv loopEv[NLOOP];
static int  nLoop = 0;
static bool loopRec = false, loopPlay = false;

// chord type/modifier bitmask helpers (for the looper)
static int modsMask(void) { int m = 0; for (int i = 0; i < NMOD; i++) if (modOn[i]) m |= (1 << i); return m; }
static void setMods(int m) { for (int i = 0; i < NMOD; i++) modOn[i] = (m >> i) & 1; }

// tiny non-crypto rng for SLOP humanising (no determinism needed here)
static unsigned rngState = 2463534242u;
static int cb_rand(int n) { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return (int)(rngState % (unsigned)n); }

// ── the chord engine ────────────────────────────────────────
// unique pitch classes of the current chord (type triad + active modifiers)
static int cb_pcs(int *out) {
    bool seen[12] = { false };
    for (int i = 0; i < 3; i++)    seen[TRIAD[chType][i] % 12] = true;
    for (int m = 0; m < NMOD; m++) if (modOn[m]) seen[MODIV[m] % 12] = true;
    // RICHNESS adds colour on top of the chord's own 7th (set in modOn): 2 = +9th, 3 = +13/6
    if (richness >= 2) seen[2] = true;               // the 9th
    if (richness >= 3) seen[9] = true;               // the 6th / 13th (lush)
    int n = 0;
    for (int p = 0; p < 12; p++) if (seen[p]) out[n++] = p;
    return n;
}

// build the VOICED chord: stack the chord tones ascending across octaves, then
// take a window of npc consecutive tones offset by `voicing` — so each voicing
// step shifts the whole chord up/down one chord-tone (inversion cascade).
static void cb_build(void) {
    int pc0[12]; int npc0 = cb_pcs(pc0);
    int pc[12], npc = 0;                               // flavor rootless: drop the root pc (the bass owns it)
    for (int i = 0; i < npc0; i++) {
        if (rootless && pc0[i] == 0 && npc0 > 2) continue;
        pc[npc++] = pc0[i];
    }
    int base = 24 + root + transpose + octave * 12;   // low anchor (~C1), shifted by Z/X octave
    int tones[128], nt = 0;
    for (int oct = 0; oct < 9 && nt < 120; oct++)
        for (int i = 0; i < npc && nt < 120; i++)
            tones[nt++] = base + pc[i] + 12 * oct;
    int start = 0;
    while (start < nt && tones[start] < 48 + root + transpose + octave * 12) start++;   // comfortable mid register
    int idx = mid(0, start + voicing, nt - npc);
    nVoiced = npc;
    for (int k = 0; k < npc; k++) voiced[k] = tones[idx + k];

    // strum-plate strings: the same pitch classes across ~5 octaves, low→high
    nStr = 0;
    int slow = 36 + root + transpose + octave * 12;
    for (int oct = 0; oct < 5 && nStr < 40; oct++)
        for (int i = 0; i < npc && nStr < 40; i++)
            strNote[nStr++] = slow + pc[i] + 12 * oct;
}

static void cb_release_held(void) {
    for (int k = 0; k < nHeld; k++) note_off(heldH[k]);
    nHeld = 0;
}

// the FOLLOWING sub-bass: on its own SL_BASS engine, auto-plays the chord ROOT
// (pc[0]) a couple of octaves down + a quieter octave-lower SUB layer for weight
// (the "thundering bottom", à la more-note-bass). Level from the BASS knob. The
// sub stays LOW — it ignores the keybed octave shift on purpose. dur ms lets the
// groove (short, punchy) and a chord-change articulation (longer) differ.
// the bass note = the chord's tones stacked from a LOW anchor, WALKED by bassVoi
// (0 = root, 1 = 3rd, 2 = 5th, then up an octave and around again) — the Orchid's
// bass voicing dial. Changing chords keeps the offset, so the bass voice-leads.
static int cb_bass_note(void) {
    int pc[12]; int npc = cb_pcs(pc);
    if (!npc) return -1;
    int idx = mid(0, bassVoi, npc * 3 - 1);              // ~3 octaves of walk
    return 24 + root + transpose + 12 * (idx / npc) + pc[idx % npc];
}
static void cb_bass_at(int dur, int volBias) {
    if (kBass < 0.02f) return;
    int bn = cb_bass_note();
    if (bn < 0) return;
    int v = mid(1, (int)(kBass * 6) + 1 + volBias, 7);
    hit(bn,      SL_BASS, v,                dur);
    hit(bn - 12, SL_BASS, mid(1, v - 2, 7), dur - 40);   // octave-down SUB layer
}
static void cb_bass(void) { cb_bass_at(420, 0); }        // chord-change articulation

// play the current chord in the current performance mode
static void cb_play(void) {
    cb_build();
    int in = SL_CHORD;                       // the configured slot (applyEngine set its engine+filter+FX)
    switch (perfMode) {
        case PM_PLAIN:
            cb_release_held();
            for (int k = 0; k < nVoiced; k++) heldH[nHeld++] = note_on(voiced[k], in, masterV);
            break;
        case PM_STRUM:
            for (int k = 0; k < nVoiced; k++) schedule_hit(k * strumMs, voiced[k], in, masterV, 900);
            break;
        case PM_SLOP:
            for (int k = 0; k < nVoiced; k++) schedule_hit(k * strumMs + cb_rand(18), voiced[k], in, masterV, 900);
            break;
        case PM_HARP: {
            // a graceful upward gliss, not a machine-gun burst: the OPTIONS knob
            // rolls it (strumMs 8..60 -> ~30..82ms/string, always airier than a
            // strum), with a gentle taper so the low strings ring and the upper
            // ones stay light — like a real harp sweep.
            int harpMs = strumMs + 22;
            for (int k = 0; k < nStr; k++) {
                int v = mid(1, 5 - k / 12, 5);
                schedule_hit(k * harpMs, strNote[k], SL_HARP, v, 260);
            }
            break;
        }
        case PM_ARP:
        case PM_PATTERN:
            arpIdx = 0;      // beat-clock driven in update(); just (re)arm here
            break;
    }
    cb_bass();                                           // Follow: the bass tracks the chord root
    armed = true;
}

// pick a whole new chord (root+type) and fire it; records to the looper if arming
static void cb_trigger(int newRoot, int newType) {
    root = newRoot; chType = newType;
    if (loopRec) {
        int pos = (beat() * 4 + (int)(beat_pos() * 4.0f)) % LOOP_16;
        if (nLoop < NLOOP) { loopEv[nLoop++] = (LoopEv){ pos, root, chType, modsMask(), voicing }; }
    }
    cb_play();
}

// KEY mode: play the diatonic chord for white-key degree d (0..6) in the current key —
// set the degree's type + its diatonic 7th, then fire. The flavor's colour adds the rest.
// the display name for a chord (root pc + triad type + 7th) — used by the keybed + spice strip
static const char *chord_name(int r, int ty, int sev) {
    const char *q = (ty == TY_MIN) ? (sev == 1 ? "m7" : "m")
                  : (ty == TY_DIM) ? "m7b5"
                  : (ty == TY_SUS) ? "sus"
                  : (sev == 2)     ? "maj7"
                  : (sev == 1)     ? "7" : "";
    return str("%s%s", NOTE[r], q);
}
// play an explicit chord: its 7th (unless RICHNESS is "simple") — 9th/13th are added in cb_pcs
static void play_key_chord(int r, int ty, int sev) {
    for (int m = 0; m < NMOD; m++) modOn[m] = false;
    if (sev >= 0 && richness >= 1) modOn[sev] = true;
    cb_trigger(r, ty);
}
// KEY mode: play the diatonic chord for white-key degree d in the current key
static void play_degree(int d) {
    play_key_chord((keyRoot + DEG_OFF[d]) % 12, DEG_TYPE[d], DEG_MOD[d]);
}

// a chord PARAMETER changed (type / modifier / voicing / octave): always
// re-voice the plate + arm the next hit SILENTLY. If RETRIG is on, also re-play
// the chord now (the "hear every tweak" behaviour).
static void cb_param(void) { cb_build(); if (retrig) cb_play(); }

// ── strum plate helpers ─────────────────────────────────────
static int stringAt(int x) {
    if (x < PLATE_X || x > PLATE_X + PLATE_W || nStr == 0) return -1;
    return mid(0, (x - PLATE_X) * nStr / PLATE_W, nStr - 1);
}
static void strike(int k) {
    if (k < 0 || k >= nStr) return;
    note(strNote[k], SL_HARP, 5);
    litT[k] = now();
}

// ── drums ───────────────────────────────────────────────────
static void play_row(int r) {
    int v[7]; for (int i = 0; i < 7; i++) v[i] = mid(0, (int)(i * drumMix + 0.5f), 7);   // LOOP knob scales drum level
    switch (r) {
        case 0: hit(36, INSTR_TRI,   v[6], 100); break;
        case 1: hit(55, INSTR_NOISE, v[5], 120); break;
        case 2: hit(84, INSTR_NOISE, v[3],  28); break;
        case 3: hit(84, INSTR_NOISE, v[2], 170); break;
        case 4: hit(64, INSTR_NOISE, v[4],  60); break;
        case 5: cb_bass_at(180, -1); break;              // BASS row grooves the following sub-bass (own engine, tracks root)
    }
}
static void loadPreset(int p) {
    curPreset = p;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PATTERN[p][r][c] == 'x');
}

// apply a genre flavor: set the backing-band personality (comp/voicing/timbre/groove)
static void apply_flavor(int fl) {
    flavor = ((fl % NFLAV) + NFLAV) % NFLAV;
    const Flavor *F = &FLAVORS[flavor];
    perfMode = F->perfMode; kPerform = (perfMode + 0.5f) / NPERF;
    engine   = F->eng;      kSound   = (engine + 0.5f) / NENG;
    rootless = F->rootless;
    richness = F->rich;
    groove   = 1;                        // start each flavor at its normal density
    tempo    = F->tempo;
    kBPM = appliedBPM = (F->tempo - 60) / 140.0f;   // sync the BPM knob to the flavor's tempo
    strumMs  = F->strumMs;
    loadPreset(F->drumPreset);
    if (F->autoRun) playing = true;
    keyMode = (flavor != FL_NEUTRAL);   // flavors default to diatonic KEY mode; NEUTRAL stays chromatic
    cb_build();
}

// the flavored accompaniment (data-driven): the flavor's comp onsets, STRUMMED (voices
// staggered, not a block stab), + its bass onsets. Called each new 16th while a chord is
// armed; s32 = position in the 2-bar (32-step) cycle. bossa = João clave + surdo, etc.
static void flavor_accomp(int s16) {
    const Flavor *F = &FLAVORS[flavor];
    int  s32  = s16 % 32;                        // position in the 2-bar comp cycle
    int  step = s16 % 16;                        // position in the current bar
    int  pib  = (s16 / 16) % 4;                  // bar within the 4-bar PHRASE (0..3)
    bool fill = (pib == 3);                      // the turnaround / fill bar
    int  swell = fill ? 1 : (pib == 0 ? -1 : 0); // breathe: ease in at the top, lift on the fill

    // COMP — the flavor's clave, strummed; GROOVE sets density (sparse drops hits · busy adds)
    for (int i = 0; F->comp[i] >= 0; i++)
        if (F->comp[i] == s32) {
            if (groove == 0 && (i & 1)) break;                            // sparse: drop odd-index hits
            if (groove == 1 && s32 % 16 != 0 && cb_rand(100) < 15) break; // normal: a little human drop
            int vol = mid(1, ((s32 % 16 == 0) ? masterV : masterV - 1) + swell, 7);
            for (int k = 0; k < nVoiced; k++)
                schedule_hit(k * strumMs / 2 + cb_rand(4), voiced[k], SL_CHORD, vol, 380);
            break;
        }
    // extra anticipation pushes — on the turnaround bar always, and every bar when BUSY
    if ((fill || groove == 2) && (step == 10 || step == 14) && nVoiced) {
        int vol = mid(1, masterV - 1, 7);
        for (int k = 0; k < nVoiced; k++) schedule_hit(k * strumMs / 2, voiced[k], SL_CHORD, vol, 220);
    }

    // BASS — surdo/walk; the fill bar gets a busier pickup; GROOVE sets density
    if (fill && step >= 12) {
        if (step == 12 || step == 14) cb_bass_at(160, -1);
    } else {
        for (int i = 0; F->bassOn[i] >= 0; i++)
            if (F->bassOn[i] == s32) {
                if (groove == 0 && s32 % 16 != 0) break;                  // sparse: bass only on the "1"
                cb_bass_at((s32 % 16 == 0) ? 300 : 240, (s32 % 16 == 0) ? 0 : -1);
                break;
            }
        if (groove == 2 && (step == 6 || step == 14)) cb_bass_at(180, -1);  // busy: extra offbeat bass
    }

    // a soft open-hat lift on the turnaround — the phrase taking a breath
    if (fill && step == 14) hit(84, INSTR_NOISE, mid(1, masterV - 1, 7), 120);
}

// ── engine + fx config (set-and-hold) ───────────────────────
static int appEngine = -1, appHarp = -1;
static void applyEngine(void) {
    if (engine != appEngine) {                       // the PIANO voice — played with hit(), so the model's own sustain stands
        appEngine = engine;
        const Model *m = &MODEL[engine];
        instrument(SL_CHORD, m->instr, m->a, m->d, m->s, m->r);
        instrument_filter(SL_CHORD, FILTER_LOW, m->cut, m->res);
        appRev = appCho = -1;   // re-send fx onto the fresh slot next frame
    }
    if (harpModel != appHarp) {                      // the HARP voice — SAME shelf, but forced plucky (sustain 0) so a strum always decays, never drones
        appHarp = harpModel;
        const Model *m = &MODEL[harpModel];
        instrument(SL_HARP, m->instr, 1, 200, 0, 280);
        instrument_filter(SL_HARP, FILTER_LOW, m->cut, m->res);
    }
}
static void applyFX(void) {
    if (kFX != appRev) { instrument_reverb(SL_CHORD, kFX * 0.8f); appRev = kFX; }
    if (kFX != appCho) {
        if (kFX > 0.05f) instrument_chorus(SL_CHORD, 0.8f, 0.5f, kFX * 0.7f);
        else             instrument_chorus(SL_CHORD, 0.8f, 0.5f, 0);
        appCho = kFX;
    }
}
// pedalboard FX — SET-AND-HOLD: reconfigure a slot only when its pedal value CHANGES
static void apply_pedals(void) {
    for (int i = 0; i < NPED; i++) {
        Pedal *p = &pedals[i];
        float v = p->on ? p->amt : 0.0f;
        if (v == p->applied) continue;
        p->applied = v;
        switch (p->kind) {
            case PD_REVERB: instrument_reverb(p->slot, v); break;
            case PD_DELAY:  instrument_echo(p->slot, v); break;
            case PD_CHORUS: instrument_chorus(p->slot, 0.4f, 0.3f, v); break;
            case PD_DRIVE:  instrument_drive(p->slot, v); break;
        }
    }
}

void init(void) {
    instrument(SL_BASS, INSTR_SAW, 6, 240, 4, 220);      // dedicated sub-bass — own engine, Moog ladder (à la more-note-bass)
    instrument_filter(SL_BASS, FILTER_LADDER, 420, 4);
    reverb(0.6f, 0.4f);
    echo(300, 0.4f, 0.5f);                               // the delay bus (for the harp DLY pedal)
    for (int k = 0; k < NFINGER; k++) strId[k] = NOFINGER;
    for (int k = 0; k < 40; k++) litT[k] = -999;
    applyEngine();
    cb_build();
    loadPreset(0);
}

// ── input: the CHORD tab ────────────────────────────────────
static void update_chord(void) {
    // keybed roots (QWERTY) — in KEY mode a white key plays the diatonic chord of that
    // degree; otherwise a chromatic root (keeping the current type). Black keys stay
    // chromatic "off-road" roots either way.
    for (int i = 0; i < 7; i++) if (keyp(WROOT[i])) { if (keyMode) play_degree(i); else cb_trigger(WPC[i], chType); }
    for (int i = 0; i < 5; i++) if (keyp(BROOT[i])) cb_trigger(BPC[i], chType);

    // on-screen keybed taps (touch / mouse) — check black keys first so their zone wins
    {
        int ww = SCREEN_W / 7, ky = 156, kh = SCREEN_H - 156;
        static const int BXk[5] = { 0, 1, 3, 4, 5 };
        bool blackHit = false;
        for (int b = 0; b < 5; b++) { int x = (BXk[b] + 1) * ww - ww / 4;
            if (tapp(x, ky, ww / 2, kh * 3 / 5)) { cb_trigger(BPC[b], chType); blackHit = true; } }
        if (!blackHit)
            for (int i = 0; i < 7; i++)
                if (tapp(i * ww, ky, ww - 1, kh)) { if (keyMode) play_degree(i); else cb_trigger(WPC[i], chType); }
    }

    // KEY mode: K toggles it; , / . shift the scale root
    if (keyp('K'))  keyMode = !keyMode;
    if (keyp(',')) keyRoot = (keyRoot + 11) % 12;
    if (keyp('.')) keyRoot = (keyRoot + 1)  % 12;

    // MIDI keyboard — a note-on picks the chord ROOT (by pitch class) and fires
    // the current chord, exactly like tapping the keybed (the Orchid plays a
    // whole chord from one key). Octave is the cart's own Z/X shift, so we
    // fold every MIDI octave onto the one-octave root picker.
    int mnote, mvel, mev;
    while ((mev = midi_get(&mnote, &mvel)) != 0)
        if (mev > 0) cb_trigger(mnote % 12, chType);

    // Z / X shift the whole keybed an octave down / up
    if (keyp('Z')) { octave = max(-2, octave - 1); cb_param(); }
    if (keyp('X')) { octave = min( 2, octave + 1); cb_param(); }

    // Changing a chord PARAMETER (type / modifier / voicing / octave) re-voices
    // the plate + arms the next played chord via cb_param(). With RETRIG OFF
    // (default) that is SILENT — you hear a chord only when you play a root or
    // strum. With RETRIG ON, every tweak also re-plays the chord.

    if (keyMode) {
        // KEY mode — two clear zones, one behavior each:
        // SPICE STRIP (row 1): each is a chord you PRESS (like a key), no arming.
        for (int i = 0; i < NSPICE; i++)
            if (tapp(6 + i * 51, 32, 48, 14) || keyp('1' + i))
                play_key_chord((keyRoot + SPICE[i].off) % 12, SPICE[i].ty, SPICE[i].sev);
        // RICHNESS (row 2): one radio-select — simple / 7th / 9th / lush
        for (int r = 0; r < 4; r++) if (tapp(6 + r * 78, 50, 72, 14) || keyp('5' + r)) { richness = r; cb_param(); }  // rebuild → plate strings update live
    } else {
        // chromatic mode — the original chord builders
        for (int t = 0; t < 4; t++) if (keyp('1' + t)) { chType = t; cb_param(); }
        for (int m = 0; m < NMOD; m++) if (keyp('5' + m)) { modOn[m] = !modOn[m]; cb_param(); }
        for (int t = 0; t < 4; t++) if (tapp(6 + t * 78, 32, 72, 14)) { chType = t; cb_param(); }
        for (int m = 0; m < NMOD; m++) if (tapp(6 + m * 78, 50, 72, 14)) { modOn[m] = !modOn[m]; cb_param(); }
    }

    // LEAD VOICING strip — LEFT/RIGHT or drag on the strip
    if (btnp(0, BTN_LEFT))  { voicing--; cb_param(); }
    if (btnp(0, BTN_RIGHT)) { voicing++; cb_param(); }
    if (tapp(6, 68, SCREEN_W - 12, 14)) {
        int nx = mid(-14, (touch_x(0) - SCREEN_W / 2) / 8, 14);
        if (nx != voicing) { voicing = nx; cb_param(); }
    }

    // BASS VOICING walk — UP/DOWN steps the bass note through the chord tones;
    // play it on each step so you HEAR the bass move (the point of walking it).
    if (btnp(0, BTN_UP))   { bassVoi = min(14, bassVoi + 1); cb_bass(); }
    if (btnp(0, BTN_DOWN)) { bassVoi = max(0,  bassVoi - 1); cb_bass(); }

    // FLAVOR chip — tap to cycle the genre (the fork)
    if (tapp(98, 15, 62, 13)) apply_flavor(flavor + 1);
    // PIANO + HARP model selectors — tap to cycle through the MODEL[] shelf
    if (tapp(166, 15, 72, 13)) { engine    = (engine    + 1) % NENG; kSound = (engine + 0.5f) / NENG; }
    if (tapp(242, 15, 72, 13)) { harpModel = (harpModel + 1) % NENG; }
    if (tapp(6,   138, 90,  12)) {
        if (keyMode) groove = (groove + 1) % 3;                       // flavor: sparse / normal / busy GROOVE
        else { perfMode = (perfMode + 1) % NPERF; kPerform = (perfMode + 0.5f) / NPERF; }   // chromatic: perform mode
    }
    if (tapp(100, 138, 100, 12)) retrig   = !retrig;                  // RETRIG toggle
    if (tapp(206, 138, 58,  12)) { if (!keyMode) keyMode = 1; else keyRoot = (keyRoot + 1) % 12; }  // KEY chip

    // sonic-strings plate — every finger strums its own glissando
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), f = -1, freeSlot = -1;
        for (int k = 0; k < NFINGER; k++) {
            if (strId[k] == id) { f = k; break; }
            if (strId[k] == NOFINGER && freeSlot < 0) freeSlot = k;
        }
        if (touch_y(i) < PLATE_Y || touch_y(i) > PLATE_Y + PLATE_H) { if (f >= 0) strId[f] = NOFINGER; continue; }
        int s = stringAt(touch_x(i));
        if (s < 0) continue;
        if (f < 0) { if (freeSlot < 0) continue; f = freeSlot; strId[f] = id; strLast[f] = -1; }
        if (strLast[f] < 0) strike(s);
        else if (s != strLast[f]) { int d = sgn(s - strLast[f]); for (int k = strLast[f] + d; ; k += d) { strike(k); if (k == s) break; } }
        strLast[f] = s;
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int k = 0; k < NFINGER; k++) if (strId[k] == touch_ended_id(i)) strId[k] = NOFINGER;
}

// ── input: the MIX tab (nine knobs) ─────────────────────────
static void update_mix(void) {
    // pedal footswitches — tap to toggle (rects match stomp() in drawMixTab)
    for (int i = 0; i < 3; i++) if (tapp(8 + i * 102 + 8, 28 + 38, 80, 12)) pedals[i].on   = !pedals[i].on;
    for (int i = 0; i < 2; i++) if (tapp(8 + i * 102 + 8, 86 + 38, 80, 12)) pedals[3+i].on = !pedals[3+i].on;
    // the remaining essential knobs (the rest moved to the CHORD face). BPM only applies when
    // the knob actually MOVES — otherwise visiting this tab would overwrite the flavor's tempo.
    if (kBPM != appliedBPM) { tempo = 60 + (int)(kBPM * 140.0f); appliedBPM = kBPM; }
    masterV = mid(1, (int)(kVolume * 7) + 1, 7);
    drumMix = kLoop;
}

// ── input: the RHYTHM tab (drums + looper) ──────────────────
static void update_rhythm(void) {
    // drum grid edit
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            if (tapp(GX + c * SX, GY + r * SY, CW, CH)) {
                grid[r][c] = !grid[r][c];
                if (grid[r][c]) { play_row(r); flash[r] = frame(); }
            }
    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p) || tapp(4 + p * 52, 150, 50, 14)) loadPreset(p);

    // looper transport buttons
    if (tapp(6,   170, 60, 16)) { loopRec = !loopRec; if (loopRec) loopPlay = true; }
    if (tapp(72,  170, 60, 16)) loopPlay = !loopPlay;
    if (tapp(138, 170, 60, 16)) { nLoop = 0; loopRec = false; }
}

void update(void) {
    bpm(tempo);

    // transport tabs + play/stop (clickable; mouse is a synthetic finger)
    if (tapp(96,  1, 40, 12)) tab = TAB_CHORD;
    if (tapp(140, 1, 34, 12)) tab = TAB_MIX;
    if (tapp(178, 1, 50, 12)) tab = TAB_RHYTHM;
    if (tapp(232, 1, 40, 12)) playing = !playing;
    if (keyp(KEY_TAB))   tab = (tab + 1) % 3;
    if (keyp(KEY_SPACE)) playing = !playing;
    if (keyp('V'))       apply_flavor(flavor + 1);   // cycle the genre flavor (the fork)

    // 16th-note tick drives the drums, the arp/pattern, and the looper
    int sixteenth = beat() * 4 + (int)(beat_pos() * 4.0f);
    bool newStep = (sixteenth != last_16);
    if (newStep) last_16 = sixteenth;

    if (playing && newStep) {
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++) {
            if (FLAVORS[flavor].comp && r == 5) continue; // flavored carts own the bass (below)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
        }
    } else if (!playing) {
        cur_step = 0;
    }

    // arp / pattern run off the beat clock whenever a chord is armed
    if (armed && newStep) {
        if (FLAVORS[flavor].comp) {
            flavor_accomp(sixteenth);                     // data-driven comp + bass, phrase-aware (breathes)
        } else {
            int st = sixteenth % STEPS;
            if (perfMode == PM_ARP && nVoiced > 0)
                hit(voiced[arpIdx++ % nVoiced], SL_CHORD, masterV, 130);
            else if (perfMode == PM_PATTERN && PAT_RHYTHM[st] == 'x')
                for (int k = 0; k < nVoiced; k++) schedule_hit(0, voiced[k], SL_CHORD, masterV, 150);
        }
    }

    // chord looper playback
    if (loopPlay && newStep) {
        int pos = sixteenth % LOOP_16;
        for (int i = 0; i < nLoop; i++)
            if (loopEv[i].pos == pos && !loopRec) {   // don't double-fire what you're recording live
                root = loopEv[i].root; chType = loopEv[i].ty; setMods(loopEv[i].mods); voicing = loopEv[i].voi;
                cb_play();
            }
    }

    applyEngine();
    applyFX();
    apply_pedals();

    if      (tab == TAB_CHORD)  update_chord();
    else if (tab == TAB_MIX)    update_mix();
    else                        update_rhythm();

#ifdef DE_TRACE
    watch("flavor",  "%d", flavor);
    watch("tempo",   "%d", tempo);
    watch("root",    "%d", root);
    watch("voicing", "%d", voicing);
    watch("octave",  "%d", octave);
    watch("retrig",  "%d", retrig);
    watch("bassVoi", "%d", bassVoi);
    watch("bassN",   "%d", cb_bass_note());
    watch("nVoiced", "%d", nVoiced);
    watch("lo",      "%d", nVoiced ? voiced[0] : -1);
    watch("hi",      "%d", nVoiced ? voiced[nVoiced - 1] : -1);
#endif
}

// ── drawing ─────────────────────────────────────────────────
static void drawTab(int bx, int bw, const char *label, bool active) {
    rectfill(bx, 1, bw, 12, active ? CLR_ORANGE : CLR_DARKER_PURPLE);
    rect(bx, 1, bw, 12, active ? CLR_WHITE : CLR_MAUVE);
    print(label, bx + (bw - text_width(label)) / 2, 4, active ? CLR_BLACK : CLR_LIGHT_GREY);
}
static void drawTopBar(void) {
    print("BLOSSOM", 6, 4, CLR_LIGHT_PEACH);
    drawTab(96,  40, "CHORD",  tab == TAB_CHORD);
    drawTab(140, 34, "FX",     tab == TAB_MIX);
    drawTab(178, 50, "RHYTHM", tab == TAB_RHYTHM);
    drawTab(232, 40, playing ? "STOP" : "PLAY", playing);
    print_right(str("%d BPM", tempo), SCREEN_W - 4, 4, CLR_DARK_GREY);
}

static const char *chordLabel(void) {
    static char buf[24];
    int n = 0;
    for (const char *p = NOTE[(root + transpose + 120) % 12]; *p; p++) buf[n++] = *p;
    buf[n++] = ' ';
    for (const char *p = TYNAME[chType]; *p; p++) buf[n++] = *p;
    for (int m = 0; m < NMOD; m++) if (modOn[m]) for (const char *p = MODNAME[m]; *p; p++) buf[n++] = *p;
    buf[n] = 0;
    return buf;
}

static void drawKeybed(void) {
    int y = 156, h = SCREEN_H - 156, ww = SCREEN_W / 7;
    for (int i = 0; i < 7; i++) {                        // white keys
        int x = i * ww;
        bool sel = (root == WPC[i]);
        rectfill(x, y, ww - 1, h - 1, sel ? CLR_ORANGE : CLR_WHITE);
        rect(x, y, ww - 1, h - 1, CLR_DARK_GREY);
        if (keyMode) {                                   // show the diatonic chord name per key
            int rp = (keyRoot + DEG_OFF[i]) % 12;
            const char *q = (i == 0 || i == 3) ? "maj7" : (i == 4) ? "7" : (i == 6) ? "m7b5" : "m7";
            const char *l = str("%s%s", NOTE[rp], q);
            font(FONT_SMALL); print(l, x + (ww - text_width(l)) / 2, y + h - 9, CLR_DARK_GREY); font(FONT_NORMAL);
        } else
            print(str("%c", WROOT[i]), x + ww / 2 - 3, y + h - 9, sel ? CLR_WHITE : CLR_MEDIUM_GREY);
    }
    static const int BX[5] = { 0, 1, 3, 4, 5 };          // black keys sit right of white 0,1,3,4,5
    for (int b = 0; b < 5; b++) {
        int x = (BX[b] + 1) * ww - ww / 4;
        bool sel = (root == BPC[b]);
        rectfill(x, y, ww / 2, h * 3 / 5, sel ? CLR_ORANGE : CLR_BLACK);
        rect(x, y, ww / 2, h * 3 / 5, CLR_DARK_GREY);
        print(str("%c", BROOT[b]), x + 2, y + 2, sel ? CLR_BLACK : CLR_LIGHT_GREY);
    }
}

static void drawChordTab(void) {
    // readout + PIANO/HARP model selectors (tap to cycle the MODEL[] shelf)
    print(chordLabel(), 8, 16, CLR_WHITE);
    if (midi_present()) print("MIDI", 162 - text_width("MIDI"), 16, CLR_GREEN);   // a note-on picks the root + fires the chord
    // FLAVOR chip (the fork) — tap to cycle the backing-band personality
    rectfill(98, 15, 62, 13, flavor ? CLR_ORANGE : CLR_DARKER_PURPLE);
    rect(98, 15, 62, 13, flavor ? CLR_WHITE : CLR_MAUVE);
    print(FLAVORS[flavor].name, 101, 17, flavor ? CLR_BLACK : CLR_LIGHT_GREY);

    rectfill(166, 15, 72, 13, CLR_DARKER_BLUE);   rect(166, 15, 72, 13, CLR_INDIGO);
    print(str("PNO %s", MODEL[engine].shortName),    170, 17, CLR_LIGHT_PEACH);
    rectfill(242, 15, 72, 13, CLR_DARKER_PURPLE); rect(242, 15, 72, 13, CLR_MAUVE);
    print(str("HRP %s", MODEL[harpModel].shortName), 246, 17, CLR_LIGHT_PEACH);

    // top two rows: chord builders (chromatic) OR the SPICE + EXTENSION layer (KEY mode).
    // In KEY mode each button's look tells its kind (topbtn): arm / push / toggle.
    if (keyMode) {
        // SPICE STRIP (row 1): the genre's extra chords, each a key (press to play);
        // a chip lights when it's the chord currently sounding.
        print("SPICE", 6, 24, CLR_MEDIUM_GREY);
        for (int i = 0; i < NSPICE; i++) {
            int bx = 6 + i * 51, rp = (keyRoot + SPICE[i].off) % 12;
            bool on = (root == rp && chType == SPICE[i].ty);
            rectfill(bx, 32, 48, 14, on ? CLR_TRUE_BLUE : CLR_DARKER_BLUE);
            rect(bx, 32, 48, 14, on ? CLR_WHITE : CLR_INDIGO);
            const char *l = chord_name(rp, SPICE[i].ty, SPICE[i].sev);
            print(l, bx + (48 - text_width(l)) / 2, 35, on ? CLR_WHITE : CLR_LIGHT_PEACH);
        }
        // RICHNESS (row 2): one radio-select — how jazzy the chords sound
        static const char *RICH[4] = { "simple", "7th", "9th", "lush" };
        print("SOUND", 6, 46, CLR_MEDIUM_GREY);
        for (int r = 0; r < 4; r++) {
            int bx = 6 + r * 78; bool on = (richness == r);
            rectfill(bx, 50, 72, 14, on ? CLR_GREEN : CLR_DARKER_PURPLE);
            rect(bx, 50, 72, 14, on ? CLR_WHITE : CLR_MAUVE);
            print(RICH[r], bx + (72 - text_width(RICH[r])) / 2, 53, on ? CLR_BLACK : CLR_LIGHT_GREY);
        }
    } else {
        for (int t = 0; t < 4; t++) {
            int bx = 6 + t * 78; bool on = (chType == t);
            rectfill(bx, 32, 72, 14, on ? CLR_ORANGE : CLR_DARKER_PURPLE);
            rect(bx, 32, 72, 14, on ? CLR_WHITE : CLR_MAUVE);
            print(TYNAME[t], bx + (72 - text_width(TYNAME[t])) / 2, 35, on ? CLR_BLACK : CLR_LIGHT_GREY);
        }
        for (int m = 0; m < NMOD; m++) {
            int bx = 6 + m * 78; bool on = modOn[m];
            rectfill(bx, 50, 72, 14, on ? CLR_GREEN : CLR_DARKER_PURPLE);
            rect(bx, 50, 72, 14, on ? CLR_WHITE : CLR_MAUVE);
            print(MODNAME[m], bx + (72 - text_width(MODNAME[m])) / 2, 53, on ? CLR_BLACK : CLR_LIGHT_GREY);
        }
    }

    // VOICING strip (the signature) — a slider with the offset marked
    rectfill(6, 68, SCREEN_W - 12, 14, CLR_DARKER_BLUE);
    rect(6, 68, SCREEN_W - 12, 14, CLR_INDIGO);
    int cx = SCREEN_W / 2 + voicing * 8;
    cx = mid(10, cx, SCREEN_W - 10);
    rectfill(cx - 2, 69, 4, 12, CLR_YELLOW);
    print(str("LEAD %+d  <>", voicing), 10, 71, CLR_LIGHT_GREY);
    int bn = cb_bass_note();
    const char *bl = str("BASS %s  ^v", bn < 0 ? "-" : NOTE[bn % 12]);
    print(bl, SCREEN_W - 4 - text_width(bl), 71, CLR_LIGHT_PEACH);

    // sonic-strings plate
    fillp(FILL_CHECKER, CLR_DARKER_PURPLE);
    rectfill(PLATE_X, PLATE_Y, PLATE_W, PLATE_H, CLR_DARK_BLUE);
    fillp_reset();
    rect(PLATE_X, PLATE_Y, PLATE_W, PLATE_H, CLR_INDIGO);
    for (int k = 0; k < nStr; k++) {
        int x = PLATE_X + (k * 2 + 1) * PLATE_W / (nStr * 2);
        bool lit = now() - litT[k] < 0.22f;
        line(x, PLATE_Y + 3, x, PLATE_Y + PLATE_H - 3, lit ? CLR_WHITE : STRCOL[k % 7]);
    }
    for (int i = 0; i < touch_count(); i++)
        if (touch_y(i) >= PLATE_Y && touch_y(i) <= PLATE_Y + PLATE_H)
            circfill(touch_x(i), touch_y(i), 3, CLR_WHITE);
    print("SONIC STRINGS ~ strum", PLATE_X + 4, PLATE_Y + PLATE_H - 10, CLR_INDIGO);

    // control strip: perform-mode cycle · RETRIG toggle · octave (Z/X)
    rectfill(6, 138, 90, 12, CLR_DARKER_PURPLE);
    rect(6, 138, 90, 12, CLR_MAUVE);
    static const char *GRV[3] = { "sparse", "normal", "busy" };
    print(keyMode ? GRV[groove] : PMNAME[perfMode], 10, 140, CLR_LIGHT_GREY);
    rectfill(100, 138, 100, 12, retrig ? CLR_GREEN : CLR_DARKER_PURPLE);
    rect(100, 138, 100, 12, retrig ? CLR_WHITE : CLR_MAUVE);
    print(str("RETRIG %s", retrig ? "ON" : "OFF"), 104, 140, retrig ? CLR_BLACK : CLR_LIGHT_GREY);
    rectfill(206, 138, 58, 12, keyMode ? CLR_INDIGO : CLR_DARKER_PURPLE);
    rect(206, 138, 58, 12, keyMode ? CLR_WHITE : CLR_MAUVE);
    print(str("KEY %s", NOTE[keyRoot]), 210, 140, keyMode ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
    print(str("OCT%+d", octave), 268, 140, CLR_DARK_GREY);
    drawKeybed();
}

static void knob(float *v, int x, int y, const char *label, const char *val) {
    ui_knob(v, x, y, label);                                        // label sits at y+13
    if (val) print(val, x - text_width(val) / 2, y + 23, CLR_LIGHT_PEACH);
}
// a tiny stompbox: body + label + amount knob + footswitch (lit when on)
static void stomp(Pedal *p, int x, int y) {
    rrectfill(x, y, 96, 54, 4, p->on ? CLR_DARK_GREEN : CLR_DARKER_PURPLE);
    rrect(x, y, 96, 54, 4, p->on ? CLR_LIME_GREEN : CLR_MAUVE);
    print(p->lbl, x + 8, y + 5, p->on ? CLR_WHITE : CLR_LIGHT_GREY);
    ui_knob(&p->amt, x + 74, y + 20, "");                      // amount (drag)
    rrectfill(x + 8, y + 38, 80, 12, 2, p->on ? CLR_DARK_RED : CLR_DARKER_PURPLE);
    rrect(x + 8, y + 38, 80, 12, 2, CLR_MAUVE);
    circfill(x + 16, y + 44, 3, p->on ? CLR_RED : CLR_DARKER_GREY);
    print(p->on ? "ON" : "off", x + 26, y + 40, p->on ? CLR_LIGHT_PEACH : CLR_MEDIUM_GREY);
}
static void drawMixTab(void) {
    print("HARP", 8, 20, CLR_MEDIUM_GREY);
    for (int i = 0; i < 3; i++) stomp(&pedals[i], 8 + i * 102, 28);
    print("BASS", 8, 80, CLR_MEDIUM_GREY);
    for (int i = 0; i < 2; i++) stomp(&pedals[3 + i], 8 + i * 102, 86);
    print("RHYTHM  soon", 220, 96, CLR_DARK_GREY);
    // the essential knobs (the rest moved to the CHORD face)
    int ky = 156;
    knob(&kVolume, 44,  ky, "VOL",   str("%d/7", masterV));
    knob(&kBPM,    130, ky, "BPM",   str("%d", tempo));
    knob(&kLoop,   216, ky, "DRUMS", str("%d%%", (int)(kLoop * 100)));
    knob(&kBass,   288, ky, "BASS",  str("%d%%", (int)(kBass * 100)));
}

static void drawRhythmTab(void) {
    print("RHYTHM", 8, 16, CLR_LIGHT_PEACH);
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, playing ? CLR_WHITE : CLR_DARK_GREY);
    for (int r = 0; r < ROWS; r++) {
        bool lit = (frame() - flash[r]) < 5;
        print(DLABEL[r], 2, GY + r * SY + 5, lit ? CLR_WHITE : DLIT[r]);
        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) { rectfill(x, y, CW, CH, DLIT[r]); if (c == cur_step && playing) rect(x, y, CW, CH, CLR_WHITE); }
            else {
                int bg = (c == cur_step && playing) ? CLR_DARK_GREY : (c % 4 == 0) ? CLR_DARKER_BLUE : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }
    for (int p = 0; p < NPRESET; p++) {
        int bx = 4 + p * 52; bool on = (p == curPreset);
        rectfill(bx, 150, 50, 14, on ? CLR_BLUE : CLR_DARKER_GREY);
        rect(bx, 150, 50, 14, on ? CLR_WHITE : CLR_DARK_GREY);
        print(str("%d%s", p + 1, PNAME[p]), bx + 2, 153, on ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    // looper transport
    const char *lb[3] = { loopRec ? "REC*" : "REC", loopPlay ? "STOP" : "PLAY", "CLEAR" };
    int lc[3] = { loopRec ? CLR_RED : CLR_DARKER_PURPLE, loopPlay ? CLR_GREEN : CLR_DARKER_PURPLE, CLR_DARKER_PURPLE };
    for (int i = 0; i < 3; i++) {
        int bx = 6 + i * 66;
        rectfill(bx, 170, 60, 16, lc[i]); rect(bx, 170, 60, 16, CLR_MAUVE);
        print(lb[i], bx + (60 - text_width(lb[i])) / 2, 174, CLR_WHITE);
    }
    print(str("CHORD LOOP  %d evt", nLoop), 210, 174, CLR_LIGHT_GREY);
    print("tap grid  1-6 groove  SPACE play", 2, GY + ROWS * SY + 4, CLR_DARK_GREY);
}

void draw(void) {
    cls(tab == TAB_CHORD ? CLR_DARKER_BLUE : CLR_BROWNISH_BLACK);
    ui_begin();                          // FIRST: ui.h widgets (the MIX knobs) capture here
    drawTopBar();
    if      (tab == TAB_CHORD)  drawChordTab();
    else if (tab == TAB_MIX)    drawMixTab();
    else                        drawRhythmTab();
    ui_end();                            // LAST: resolve knob drags — without this they're dead
}
