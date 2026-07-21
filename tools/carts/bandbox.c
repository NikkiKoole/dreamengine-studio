/* de:meta
{
  "slug": "bandbox",
  "title": "BandBox",
  "status": "wip",
  "created": "2026-07-21",
  "kind": ["instrument"],
  "genre": null,
  "teaches": ["functional-harmony"],
  "resizable": true,
  "collection": ["device-face"],
  "lineage": "The SEQUENCER sibling of chordwise (the demand-82 harmony analyzer). chordwise proved the chord-chart + genre-band stack on one flat screen and hit a density ceiling; bandbox is the standalone 160x100 device-face sequencer where every VOICE is a lane of lego-block cells and every cell is p-lockable (Elektron-style per-cell overrides on top of the voice's auto 'follows the chord' default). chordwise did per-cell only for chords; bandbox does it for every voice. Reuses the harmony brain (harmony.h) + chordwise's genre-band maps + rad_bass_to + drumkit.h. Design: docs/design/bandbox.md.",
  "description": {
    "summary": "A 160x100 device-face chord SEQUENCER: compose a progression as a 5-lane tracker (CHORDS / BASS / MEL / DRUMS / PAD x 8 bars) and a genre BAND follows the one declared MODE. Every cell defaults to 'follow the chord + genre'; tap one to open its block editor in the glass and P-LOCK it - a per-cell override (a chord's own strum/inversion/octave, a bass MUTE or WALK, a drum FILL bar, a melody REST or ACCENT, a pad ON/OFF). The chassis (voice rail, nav, keybed) never moves; only the glass morphs.",
    "detail": "The build of the bandbox brief (docs/design/bandbox.md): the draw-only mockup wired for real. The chord lane analyzes in roman numerals via the shared harmony brain (harmony.h); the ^/v spinner steps a chord in-key; the keybed sets the selected chord's root. Playback ports chordwise's subdivided-bar loop (chord comp, walking/idiomatic bass, drumkit groove, blooming melody, held pad) - all reading the declared KEY + MODE, so the band plays the genre's idiom (14 genres: BOSSA/LOUNGE/POP/FOLK/MINOR/CINE/DORIAN/MIXO/PHRYG/LYDIAN/BLUES + three lifted from the radio stations: JANGLE, JINGLE, NAPOLN). The sequencer difference: p-locks. Each bar carries per-cell overrides that playback reads as p-lock-else-global. Deterministic (carries a spec()); no swing-jitter/life yet, same call chordwise made for spec-ability.",
    "controls": "Tap a tracker cell to edit it IN PLACE: that voice's lane rises to the top row (staying visible so you see the change land) and its block editor unfolds in the freed space below; tap another cell in the promoted lane to scrub to that bar, BACK (rail) closes it. CHORDS editor: a global TONE (PLUCK/EPIANO/ORGAN) + ^/v step the chord in-key + STRUM/INV/OCT/7TH chips (AUTO = follow the global voicing, else a per-cell p-lock); the keybed sets the chord's root. BASS: global STYLE + TONE (the bass SOUND: SYNTH/SUB/FM/UPRIGHT pizzicato) + per-cell FOLLOW/MUTE/HOLD/WALK/OCT/FILL (drop out, sit on the root, walk, octave-pump, or run a fill into the next chord). DRUMS: global STYLE + KIT (ELECTRO/ACOUSTIC) + per-cell GROOVE/DROP/KICK/FILL/CRASH/BUILD. MEL: global TONE (SINE/SQR/FM/BELL) + per-cell FOLLOW/REST/ACCENT. PAD: global TONE (SINE/SAW/STRINGS) + per-cell FOLLOW/OFF/ON. Every BAR also has a FEEL in the editor header (STRAIGHT/ACCENT/DRAG) - a per-bar performance p-lock the whole band shares: ACCENT punches the bar louder, DRAG lays it behind the beat (deterministic; PUSH/ahead is a follow-up). Tap a voice rail header to mute/unmute the lane (chords included). Nav: < KEY > steps the key (STOPPED re-analyzes, PLAYING transposes), MODE cycles the genre, NEW empties to a fresh song (keeping your key/genre/voice setup), the play button loops. Tap the '+' chord cell to open the ADD-CHORD picker: the harmony brain's NEXT suggestions (ranked, what the progression wants) + the full mode palette as roman-numeral chips + the live keybed for any root; each pick appends + auditions and stays open. Keys: SPACE loop, LEFT/RIGHT key, B mode, N new song, BACKSPACE closes the editor/picker. MUSICAL TYPING (GarageBand-style): the QWERTY rows play the keybed - home row A S D F G H J K L (;) = white keys C D E F G A B C D E, top row W E T Y U O P = the black keys; each press adds/edits a chord on that root (opens the picker if idle), so you can type a progression. While the loop plays, the keybed LIGHTS UP (green) the notes the in-view voice is triggering, folded onto the keys - the focused voice when editing, or the chords while picking (the full tracker view stays dark)."
  }
}
de:meta */
// bandbox — the chord-chart SEQUENCER (a face.h device face), sibling of chordwise.
// The mockup's LOOK is settled (docs/design/bandbox.md); this is the WIRING: a real
// 5-lane tracker where every cell is p-lockable, driven by the shared harmony brain
// + chordwise's genre band. The glass MORPHS (tracker <-> a tapped cell's editor); the
// chassis (rail, nav, keybed) never moves.
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "face.h"
#include "cursor.h"
#include "harmony.h"
#include "radio.h"    // rad_bass_to — nearest-octave voice leading for the bassline
#include "drumkit.h"  // the shared playable kit — the rhythm section's voices
#include <stdio.h>
#include <string.h>

#define I_PLK  5   // the pluck that voices the chord
#define I_BSS  6   // a soft bass under it
#define I_LEAD 7   // the melody voice (a soft lead over the chords)
#define I_PAD  8   // a slow sustained pad

// ─────────────────────────────────────────────────────────────────────────
// THE ENGINE — copied from chordwise.c (the genre band + playback shape). The
// six pattern tables are DATA; the MODES/vocab plumbing is the harmony brain.
// (Per bandbox.md's reuse map: copy first, extract a shared band.h only once the
// per-cell-p-lock shape is known.)
// ─────────────────────────────────────────────────────────────────────────

// STRUM — how the chord's strings are raked. gap = frames between strings; up = 1
// rakes high->low (an upstroke), else low->high.
typedef struct { const char *name; int gap, up; } Strum;
static const Strum STRUMS[] = {
    { "BLOCK", 0, 0 }, { "DOWN", 2, 0 }, { "UP", 2, 1 }, { "ROLL", 6, 0 },
};
#define NSTRUM ((int)(sizeof STRUMS / sizeof STRUMS[0]))
static const char *INV_LAB[4] = { "ROOT", "1ST", "2ND", "3RD" };
static const char *OCT_LAB[3] = { "LOW", "MID", "HIGH" };

// global voice VOICING defaults (a cell inherits these when its p-lock is AUTO)
static int strumSel = 0, invSel = 0, octSel = 1, seventh = 1;

// a tiny pending-note queue so the strum can stagger across frames.
#define NPEND 16
static struct { int midi, instr, vol, at; } pend[NPEND];
static int npend = 0;
static void queue_note(int midi, int instr, int vol, int at) {
    if (at <= 0) { note(midi, instr, vol); return; }
    if (npend >= NPEND) return;
    pend[npend].midi = midi; pend[npend].instr = instr;
    pend[npend].vol = vol;   pend[npend].at = at;   npend++;
}
static void pump_notes(void) {
    for (int i = 0; i < npend; ) {
        if (--pend[i].at <= 0) { note(pend[i].midi, pend[i].instr, pend[i].vol);
                                 pend[i] = pend[--npend]; }
        else i++;
    }
}

static const char *NOTE[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
static int keyPc = 0;

// a MODE = a style (carries its VOCAB) + a minor-key readout. (chordwise's palette-
// layout fields are dropped — bandbox enters chords via the spinner/keybed, not a
// palette grid, so it needs only the tonal system + the KEY readout.)
typedef struct { const char *name; const HbStyle *style; int minorKey; } Mode;
static const Mode MODES[] = {
    { "BOSSA",  &HB_BOSSA,        0 },
    { "LOUNGE", &HB_COCKTAIL,     0 },
    { "POP",    &HB_POP,          0 },
    { "FOLK",   &HB_FOLK,         0 },
    { "MINOR",  &HB_MINPOP,       1 },
    { "CINE",   &HB_CINEMATIC,    1 },
    { "DORIAN", &HB_DORIAN_STYLE, 0 },
    { "MIXO",   &HB_MIXO_STYLE,   0 },
    { "PHRYG",  &HB_PHRYG_STYLE,  0 },
    { "LYDIAN", &HB_LYDIAN_STYLE, 0 },
    { "BLUES",  &HB_BLUES,        0 },
    // radio-station ideas as genres — same vocab as a sibling mode, but their OWN groove
    // (the vibe lives in the band tables below): jangle-pop, Mac-DeMarco jingle, ND funk.
    { "JANGLE", &HB_POP,          0 },   // bright 8th-strum indie-pop (jangle.c)
    { "JINGLE", &HB_JINGLE,       0 },   // Mac-DeMarco slacker-jangle: bVII7 home, ii-V, backdoor (jingle.c)
    { "NAPOLN", &HB_MIXO_STYLE,   0 },   // Canned-Heat acid-jazz disco-funk (napoleon.c)
};
#define NMODE ((int)(sizeof MODES / sizeof MODES[0]))
static int modeSel = 0;

static const HbVocab *cur_vocab(void) {
    const HbVocab *v = MODES[modeSel].style->vocab;
    return v ? v : &HB_MAJOR;
}
// tempo: each bar holds for a half-note. BPM is a 0..1 slider.
#define BPM_MIN 60
#define BPM_MAX 180
static float bpmN = 0.408f;
static int cur_bpm(void)    { return BPM_MIN + (int)(bpmN * (BPM_MAX - BPM_MIN) + 0.5f); }
static int bar_frames(void) { return (int)(7200.0f / cur_bpm() + 0.5f); }

static void chname(char *out, int n, int rootPc, int q, int sev) {
    snprintf(out, n, "%s%s", NOTE[rootPc], sev ? hb_qname[q] : hb_qtriad[q]);
}

// ─────────────────────────────────────────────────────────────────────────
// THE DATA MODEL — the chord is the spine; voices follow it; per-cell p-locks
// override. -1 = AUTO (inherit the voice's global default); a concrete value =
// a p-lock. (bandbox.md's proposed model.)
// ─────────────────────────────────────────────────────────────────────────
#define NBARS 8
#define VOICES 5
enum { V_CH, V_BA, V_ME, V_DR, V_PA };
typedef struct {
    int rootPc, qual;            // the CHORD (the harmonic spine of the bar)
    int strum, inv, oct, sev;    // CHORD p-locks (-1 = auto)
    int bass;                    // BASS p-lock (BPL_*): -1 FOLLOW · MUTE/HOLD/WALK/OCT/FILL
    int fill;                    // DRUM p-lock (DPL_*): GROOVE(0)/DROP/KICK/FILL/CRASH/BUILD
    int mel;                     // MEL  p-lock: -1 auto · 0 rest · 1 accent
    int pad;                     // PAD  p-lock: -1 auto · 0 off · 1 on
    int feel;                    // per-BAR FEEL (FEEL_*): the whole band's groove this bar
} Bar;
// FEEL — a per-bar performance p-lock the whole band shares (deterministic, spec-safe):
// ACCENT punches the bar louder, DRAG lays it behind the beat. (PUSH/ahead needs
// bar-lookahead in a forward-only scheduler — a follow-up.)
enum { FEEL_STRAIGHT, FEEL_ACCENT, FEEL_DRAG };
static const char *FEEL_LAB[3] = { "STRAIGHT", "ACCENT", "DRAG" };
#define FEEL_DRAG_FR 3           // frames the whole bar lags when DRAGged
static Bar arr[NBARS];
static int nbars = 4;            // loop length (contiguous bars 0..nbars-1)
static int pfn[NBARS];           // each bar's analyzed function (-1 = ? / out of vocab)
static int feel_delay_fr(int b) { return arr[b].feel == FEEL_DRAG ? FEEL_DRAG_FR : 0; }
static int feel_vel(int b, int base) { if (arr[b].feel == FEEL_ACCENT) { base += 2; if (base > 7) base = 7; } return base; }

// the band's voices (the rail = the tracker's row headers). von = lane on/off.
static const char *VL[VOICES]  = { "CH", "BA", "ME", "DR", "PA" };
static int von[VOICES] = { 1, 1, 1, 1, 0 };   // pad off by default (chords always on)

// global voice STYLE defaults
enum { B_HOLD, B_ROOT5, B_WALK, B_BAND };
static const char *BASS_LAB[4] = { "HOLD", "R-5", "WALK", "BAND" };
// per-CELL bass p-locks (arr[].bass; -1 = FOLLOW the global style). One-bar
// overrides a bassist would punch in: drop out, sit on the root, walk, pump the
// octave, or run a fill into the next chord.
enum { BPL_FOLLOW = -1, BPL_MUTE = 0, BPL_HOLD, BPL_WALK, BPL_OCT, BPL_FILL };
static const char *BPL_LAB[6] = { "FOLLOW", "MUTE", "HOLD", "WALK", "OCT", "FILL" };
static const int   BPL_VAL[6] = { BPL_FOLLOW, BPL_MUTE, BPL_HOLD, BPL_WALK, BPL_OCT, BPL_FILL };
static int bassSel = B_BAND;
// the bass TONE — the SOUND of the bass voice (I_BSS), swapped by re-instrumenting the
// slot (set-and-hold: only on a TONE-chip change). Recipes from instrument-recipes.md.
static const char *BTONE_LAB[4] = { "SYNTH", "SUB", "FM", "UPRIGHT" };
static int bassTone = 0;
static void apply_bass_tone(void) {
    switch (bassTone) {
        case 0:  // round triangle synth (the original)
            instrument(I_BSS, INSTR_TRI, 6, 160, 3, 220);
            instrument_filter(I_BSS, FILTER_OFF, 0, 0); break;
        case 1:  // deep sine sub
            instrument(I_BSS, INSTR_SINE, 8, 200, 5, 260);
            instrument_filter(I_BSS, FILTER_OFF, 0, 0); break;
        case 2:  // DX-style FM bass (fm/bass recipe)
            instrument(I_BSS, INSTR_FM, 1, 245, 5, 121);
            instrument_filter(I_BSS, FILTER_OFF, 0, 0);
            instrument_harmonics(I_BSS, 0.00f); instrument_timbre(I_BSS, 0.75f); instrument_morph(I_BSS, 0.30f); break;
        case 3:  // jazz upright double-bass pizzicato (bowed/upright-pizz recipe)
            instrument(I_BSS, INSTR_BOWED, 4, 0, 7, 360);
            instrument_mode(I_BSS, MODE_BOW_PIZZ, 1.0f);
            instrument_filter(I_BSS, FILTER_LOW, 1400, 4);   // brighter = more presence in the mix
            instrument_harmonics(I_BSS, 0.30f); instrument_timbre(I_BSS, 0.40f); instrument_morph(I_BSS, 0.45f); break;
    }
}
// the modeled/plucked tones read softer (a pluck's energy is transient) — give them
// a touch more velocity so the bass stays present when you switch tone.
static int bass_vel(void) { return bassTone == 3 ? 7 : (bassTone == 2 ? 6 : 5); }

// CHORDS voice TONE (I_PLK) — the comping sound.
static const char *CTONE_LAB[3] = { "PLUCK", "EPIANO", "ORGAN" };
static int chordTone = 0;
static void apply_chord_tone(void) {
    instrument_filter(I_PLK, FILTER_OFF, 0, 0);
    switch (chordTone) {
        case 0: instrument(I_PLK, INSTR_PLUCK, 2, 200, 2, 250); break;                       // plucked string (guitar/harp)
        case 1: instrument(I_PLK, INSTR_EPIANO, 2, 300, 2, 420);                              // Rhodes e-piano
                instrument_harmonics(I_PLK, 0.20f); instrument_timbre(I_PLK, 0.45f); instrument_morph(I_PLK, 0.25f); break;
        case 2: instrument(I_PLK, INSTR_ORGAN, 4, 0, 7, 120);                                 // tonewheel organ (holds)
                instrument_harmonics(I_PLK, 0.45f); instrument_timbre(I_PLK, 0.40f); instrument_morph(I_PLK, 0.20f); break;
    }
}
// MEL voice TONE (I_LEAD) — the lead melody sound.
static const char *MTONE_LAB[4] = { "SINE", "SQR", "FM", "BELL" };
static int melTone = 0;
static void apply_mel_tone(void) {
    instrument_filter(I_LEAD, FILTER_OFF, 0, 0);
    switch (melTone) {
        case 0: instrument(I_LEAD, INSTR_SINE, 4, 180, 3, 200); break;                        // soft sine
        case 1: instrument(I_LEAD, INSTR_SQUARE, 3, 150, 3, 180);
                instrument_filter(I_LEAD, FILTER_LOW, 3000, 2); break;                         // chip-lead square
        case 2: instrument(I_LEAD, INSTR_FM, 2, 220, 3, 240);                                  // round FM lead
                instrument_harmonics(I_LEAD, 0.15f); instrument_timbre(I_LEAD, 0.50f); instrument_morph(I_LEAD, 0.20f); break;
        case 3: instrument(I_LEAD, INSTR_MALLET, 2, 500, 0, 700);                              // bell/celesta
                instrument_harmonics(I_LEAD, 0.70f); instrument_timbre(I_LEAD, 0.50f); instrument_morph(I_LEAD, 0.40f); break;
    }
}
// PAD voice TONE (I_PAD) — the sustained bed.
static const char *PTONE_LAB[3] = { "SINE", "SAW", "STRINGS" };
static int padTone = 0;
static void apply_pad_tone(void) {
    instrument_filter(I_PAD, FILTER_OFF, 0, 0);
    switch (padTone) {
        case 0: instrument(I_PAD, INSTR_SINE, 300, 0, 7, 500); break;                          // warm sine
        case 1: instrument(I_PAD, INSTR_SAW, 300, 0, 6, 500);
                instrument_filter(I_PAD, FILTER_LOW, 1600, 2); break;                          // analog string-machine saw
        case 2: instrument(I_PAD, INSTR_BOWED, 200, 0, 7, 500);                                // bowed strings (arco, holds)
                instrument_mode(I_PAD, MODE_BOW_PIZZ, 0.0f);
                instrument_filter(I_PAD, FILTER_LOW, 2200, 2);
                instrument_harmonics(I_PAD, 0.40f); instrument_timbre(I_PAD, 0.40f); instrument_morph(I_PAD, 0.40f); break;
    }
}
static const char *DRUM_LAB[2] = { "PLAIN", "BAND" };
static int drumSel = 1;   // 0 PLAIN · 1 BAND
// the KIT — the whole rhythm section's SOUND (drumkit.h ships both). A one-line
// dk_use() swap; set-and-hold (re-applied only when the KIT chip changes, never per frame).
static const DrumKitDef *KITS[2]   = { &DK_ELECTRO, &DK_ACOUSTIC };
static const char       *KIT_LAB[2] = { "ELECTRO", "ACOUSTIC" };
static int kitSel = 1;    // default ACOUSTIC
// per-CELL drum p-locks (arr[].fill; GROOVE is the default, the rest are one-bar
// moves a drummer punches in): drop out, strip to the kick, a genre fill, a crash
// accent over the groove, or a whole-bar snare build.
enum { DPL_GROOVE, DPL_DROP, DPL_KICK, DPL_FILL, DPL_CRASH, DPL_BUILD };
static const char *DPL_LAB[6] = { "GROOVE", "DROP", "KICK", "FILL", "CRASH", "BUILD" };

// resolve a bar's effective chord-voicing (p-lock else global)
static int bar_strum(int b) { return arr[b].strum >= 0 ? arr[b].strum : strumSel; }
static int bar_inv(int b)   { return arr[b].inv   >= 0 ? arr[b].inv   : invSel;   }
static int bar_oct(int b)   { return arr[b].oct   >= 0 ? arr[b].oct   : octSel;   }
static int bar_sev(int b)   { return arr[b].sev   >= 0 ? arr[b].sev   : seventh;  }

static int playing = 0, playSlot = 0, playT = 0;
static int selVoice = -1;   // which voice's editor is open (-1 = the tracker)
static int selBar   = 0;    // which bar the editor edits
static int helpOn   = 0;    // the legend overlay (? button)
static int picking  = 0;    // the ADD-CHORD picker is open (tap "+")

// keybed note-lights: per-pitch-class glow that decays each frame. ONLY the voice a
// single instrument is focused on (the edited voice, or CHORDS while picking) lights
// its notes, folded (mod 12) onto the tiny keybed — a live "what's playing" readout.
// In the full tracker (no voice selected) the keybed stays dark (lit_voice = -1).
static float keyLit[12];
static void  light_pc(int pc) { keyLit[((pc % 12) + 12) % 12] = 1.0f; }
static int   lit_voice(void)  { return picking ? V_CH : selVoice; }

// ── chord voicing (chordwise's sound_chord, now p-lock-parameterized) ──────
static void sound_chord(int rootPc, int q, int withBass, int baseDelay,
                        int strum, int inv, int oct, int sev, int vel) {
    int r = 48 + rootPc + (oct - 1) * 12;
    int voices = sev ? 4 : 3;
    if (inv > voices - 1) inv = voices - 1;
    int off[4];
    for (int i = 0; i < voices; i++) off[i] = hb_tones[q][i];
    for (int k = 0; k < inv; k++) off[k] += 12;
    int lo = off[0]; for (int i = 1; i < voices; i++) if (off[i] < lo) lo = off[i];
    int m[5], inst[5], nv = 0;
    if (withBass) { m[nv] = r + lo - 12; inst[nv] = I_BSS; nv++; }
    for (int i = 0; i < voices; i++) { m[nv] = r + off[i]; inst[nv] = I_PLK; nv++; }
    int idx[5]; for (int i = 0; i < nv; i++) idx[i] = i;
    for (int a = 0; a < nv; a++) for (int b = a + 1; b < nv; b++)
        if (m[idx[b]] < m[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    int gap = STRUMS[strum].gap, up = STRUMS[strum].up;
    for (int p = 0; p < nv; p++) {
        int i = idx[up ? (nv - 1 - p) : p];
        queue_note(m[i], inst[i], vel, baseDelay + p * gap);
    }
}
// audition a bar's chord with its resolved voicing (+ its soft root)
static void audition(int b) {
    sound_chord(arr[b].rootPc, arr[b].qual, 1, 0,
                bar_strum(b), bar_inv(b), bar_oct(b), bar_sev(b), 5);
}

// ── bass (chordwise's play_bass_beat, now reading the bar's bass p-lock) ────
enum { BT_ROOT, BT_OCTU, BT_FIFTH, BT_FIFTHD, BT_THIRD, BT_SIXTH, BT_FLAT2, BT_FLAT7, BT_APPR, BT_REST };
static const int BASS_PAT[NMODE][4] = {
    /* BOSSA  */ { BT_ROOT,  BT_REST,  BT_FIFTHD, BT_REST  },
    /* LOUNGE */ { BT_ROOT,  BT_THIRD, BT_FIFTH,  BT_APPR  },
    /* POP    */ { BT_ROOT,  BT_REST,  BT_OCTU,   BT_REST  },
    /* FOLK   */ { BT_ROOT,  BT_REST,  BT_FIFTH,  BT_REST  },
    /* MINOR  */ { BT_ROOT,  BT_OCTU,  BT_ROOT,   BT_OCTU  },
    /* CINE   */ { BT_ROOT,  BT_REST,  BT_REST,   BT_REST  },
    /* DORIAN */ { BT_ROOT,  BT_SIXTH, BT_FIFTH,  BT_SIXTH },
    /* MIXO   */ { BT_ROOT,  BT_FIFTH, BT_FLAT7,  BT_FIFTH },
    /* PHRYG  */ { BT_ROOT,  BT_FLAT2, BT_ROOT,   BT_FLAT2 },
    /* LYDIAN */ { BT_ROOT,  BT_REST,  BT_FIFTH,  BT_REST  },
    /* BLUES  */ { BT_ROOT,  BT_THIRD, BT_FIFTH,  BT_SIXTH },
    /* JANGLE */ { BT_ROOT,  BT_FIFTH, BT_ROOT,   BT_FIFTH },  // driving root-fifth pulse
    /* JINGLE */ { BT_ROOT,  BT_REST,  BT_FIFTH,  BT_REST  },  // gentle boom-chuck
    /* NAPOLN */ { BT_ROOT,  BT_FIFTH, BT_OCTU,   BT_FIFTH },  // disco-funk octave pump
};
static int bassLast = 36;
static void play_bass_beat(int beat, int delay) {
    if (!von[V_BA] || !nbars) return;
    int bl = arr[playSlot].bass;                 // per-cell p-lock (BPL_*), -1 = follow
    if (bl == BPL_MUTE) return;                  // MUTE — drop out this bar

    int r0 = arr[playSlot].rootPc, q = arr[playSlot].qual;
    int rN = arr[(playSlot + 1) % nbars].rootPc;
    int base = 36 + (octSel - 1) * 12;
    int lo = base - 4, hi = base + 12;
    int pc = -1, oct = 0;

    if (bl == BPL_HOLD) {                         // HOLD — one root on the downbeat
        if (beat == 0) pc = r0; else return;
    } else if (bl == BPL_WALK) {                  // WALK — the jazz quarter walk
        if      (beat == 0) pc = r0;
        else if (beat == 1) pc = (r0 + hb_tones[q][1]) % 12;
        else if (beat == 2) pc = (r0 + hb_tones[q][2]) % 12;
        else                pc = (rN + 11) % 12;
    } else if (bl == BPL_OCT) {                   // OCT — root / octave-up pump
        pc = r0; oct = (beat % 2) ? 1 : 0;
    } else if (bl == BPL_FILL) {                  // FILL — a rising run into the next root
        if      (beat == 0) pc = r0;
        else if (beat == 1) pc = (r0 + 2) % 12;
        else if (beat == 2) pc = (r0 + 4) % 12;
        else                pc = (rN + 11) % 12;
    } else {                                      // FOLLOW — the global style
        if (bassSel == B_HOLD) { if (beat == 0) pc = r0; else return; }
        else if (bassSel == B_ROOT5) {
            if (beat == 0)      pc = r0;
            else if (beat == 2) pc = (r0 + 7) % 12;
            else return;
        } else if (bassSel == B_WALK) {
            if      (beat == 0) pc = r0;
            else if (beat == 1) pc = (r0 + hb_tones[q][1]) % 12;
            else if (beat == 2) pc = (r0 + hb_tones[q][2]) % 12;
            else                pc = (rN + 11) % 12;
        } else {                                  // B_BAND — the genre idiom
            switch (BASS_PAT[modeSel][beat]) {
                case BT_REST:                            return;
                case BT_ROOT:   pc = r0;                          break;
                case BT_OCTU:   pc = r0;                 oct = +1; break;
                case BT_FIFTH:  pc = (r0 + hb_tones[q][2]) % 12;  break;
                case BT_FIFTHD: pc = (r0 + hb_tones[q][2]) % 12; oct = -1; break;
                case BT_THIRD:  pc = (r0 + hb_tones[q][1]) % 12;  break;
                case BT_SIXTH:  pc = (r0 + 9) % 12;               break;
                case BT_FLAT2:  pc = (r0 + 1) % 12;               break;
                case BT_FLAT7:  pc = (r0 + 10) % 12;              break;
                case BT_APPR:   pc = (rN + 11) % 12;              break;
            }
        }
    }
    if (pc < 0) return;
    int n = rad_bass_to(pc, bassLast, lo, hi);
    bassLast = n;
    if (lit_voice() == V_BA) light_pc(n + oct * 12);
    queue_note(n + oct * 12, I_BSS, feel_vel(playSlot, bass_vel()), delay + feel_delay_fr(playSlot));
}

// ── drums (chordwise's kit + patterns; FILL is now a per-cell p-lock) ───────
typedef struct { const char *kick, *back, *hat; int backRole, hatRole; } DrumPat;
static const DrumPat DRUM_PAT[NMODE] = {
    /* BOSSA  */ { "x..x....", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },
    /* LOUNGE */ { "x.......", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },
    /* POP    */ { "x.......", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },
    /* FOLK   */ { "x.......", "....x...", "x...x...", DK_SNARE,  DK_HHC },
    /* MINOR  */ { "x...x...", "....x...", ".x.x.x.x", DK_CLAP,   DK_HHO },
    /* CINE   */ { "x...x...", "....x...", "........", DK_TOM_LO, DK_HHC },
    /* DORIAN */ { "x.....x.", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },
    /* MIXO   */ { "x.x.....", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },
    /* PHRYG  */ { "x.......", "x.xx.x..", "........", DK_CLAP,   DK_HHC },
    /* LYDIAN */ { "x.......", "........", "....x...", DK_SNARE,  DK_HHC },
    /* BLUES  */ { "x.......", "....x...", "x..x..x.", DK_SNARE,  DK_HHC },
    /* JANGLE */ { "x...x...", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // driving pop backbeat
    /* JINGLE */ { "x.......", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // soft laid-back backbeat
    /* NAPOLN */ { "x...x...", "....x...", ".x.x.x.x", DK_CLAP,   DK_HHO },  // four-on-floor disco + claps
};
static const DrumPat DRUM_PLAIN = { "x.......", "....x...", "x.x.x.x.", DK_SNARE, DK_HHC };
static int fill_role(char c) {
    switch (c) {
        case 's': return DK_SNARE;  case 'h': return DK_TOM_HI; case 'l': return DK_TOM_LO;
        case 'c': return DK_CLAP;   case 'k': return DK_KICK;   case 'o': return DK_HHO;
        default:  return -1;
    }
}
typedef struct { const char *pat; int crash; } FillPat;
static const FillPat FILL_PAT[NMODE] = {
    /* BOSSA  */ { "....s.hl", 0 }, /* LOUNGE */ { "....ssss", 1 },
    /* POP    */ { "....sshl", 1 }, /* FOLK   */ { "......s.", 0 },
    /* MINOR  */ { "ssssssss", 1 }, /* CINE   */ { "hhllhhll", 1 },
    /* DORIAN */ { "....s.hl", 0 }, /* MIXO   */ { "..sshhll", 1 },
    /* PHRYG  */ { "c.ccc.cc", 0 }, /* LYDIAN */ { "........", 1 },
    /* BLUES  */ { "...ssshl", 1 },
    /* JANGLE */ { "....sshl", 1 },  // a bright tom roll
    /* JINGLE */ { "....s.hl", 0 },  // a gentle tag, no crash
    /* NAPOLN */ { "....ssss", 1 },  // a disco snare build
};
static const FillPat FILL_PLAIN = { "....sshl", 1 };
static void play_drum_step(int step, int delayMs) {
    if (!von[V_DR] || !nbars) return;
    int dp = arr[playSlot].fill;                 // DPL_* per-cell move
    if (dp == DPL_DROP) return;                  // DROP — silent this bar
    delayMs += feel_delay_fr(playSlot) * 1000 / 60;   // FEEL: the whole bar drags/accents
    const DrumPat *d = (drumSel == 1) ? &DRUM_PAT[modeSel] : &DRUM_PLAIN;

    if (dp == DPL_FILL) {                        // FILL — the genre flourish + turnaround crash
        const FillPat *fp = (drumSel == 1) ? &FILL_PAT[modeSel] : &FILL_PLAIN;
        if (step == 0 && fp->crash) dk_fire_at(delayMs, DK_CRASH, 0, feel_vel(playSlot, 5));
        int r = fill_role(fp->pat[step]);
        if (r >= 0) dk_fire_at(delayMs, r, 0, feel_vel(playSlot, 4 + step / 2));
        return;
    }
    if (dp == DPL_BUILD) {                       // BUILD — a whole-bar snare riser
        int vel = 3 + step; if (vel > 7) vel = 7;
        dk_fire_at(delayMs, DK_SNARE, 0, vel);
        if (step == 0) dk_fire_at(delayMs, DK_KICK, 0, 6);
        return;
    }
    if (dp == DPL_KICK) {                         // KICK — strip to the bare pulse
        if (d->kick[step] == 'x') dk_fire_at(delayMs, DK_KICK, 0, feel_vel(playSlot, 6));
        return;
    }
    if (dp == DPL_CRASH && step == 0) dk_fire_at(delayMs, DK_CRASH, 0, feel_vel(playSlot, 5));   // accent, then GROOVE
    if (d->kick[step] == 'x') dk_fire_at(delayMs, DK_KICK,     0, feel_vel(playSlot, 6));
    if (d->back[step] == 'x') dk_fire_at(delayMs, d->backRole, 0, feel_vel(playSlot, 5));
    if (d->hat[step]  == 'x') dk_fire_at(delayMs, d->hatRole,  0, feel_vel(playSlot, 3));
}

// ── melody (chordwise's bloom; per-cell rest/accent p-lock) ─────────────────
static const char *MEL_PAT[NMODE] = {
    /* BOSSA */ "..x..x.x", /* LOUNGE */ "x...x.x.", /* POP */ "x..x.x..",
    /* FOLK */ "x...x...", /* MINOR */ "x.x..x.x", /* CINE */ "x.......",
    /* DORIAN */ "..x...x.", /* MIXO */ "x..x.x..", /* PHRYG */ "x.x.x...",
    /* LYDIAN */ "x.....x.", /* BLUES */ "x..x..x.",
    /* JANGLE */ "x.x.x.x.", /* JINGLE */ "x...x.x.", /* NAPOLN */ "..x..x.x",
};
static int melLast = 72, melI = 0;
static void play_mel_step(int step, int delay) {
    if (!von[V_ME] || !nbars) return;
    int ml = arr[playSlot].mel;
    if (ml == 0) return;                         // p-lock REST — silent this bar
    if (MEL_PAT[modeSel][step] != 'x') return;
    int r0 = arr[playSlot].rootPc, q = arr[playSlot].qual;
    int base = 72 + (octSel - 1) * 12;
    int lo = base - 5, hi = base + 7;
    int cand[5], nc = 0, voices = bar_sev(playSlot) ? 4 : 3;
    for (int i = 0; i < voices; i++) cand[nc++] = (r0 + hb_tones[q][i]) % 12;
    cand[nc++] = (r0 + 2) % 12;
    int pc = cand[melI % nc]; melI++;
    melLast = rad_bass_to(pc, melLast, lo, hi);
    if (lit_voice() == V_ME) light_pc(melLast);
    queue_note(melLast, I_LEAD, feel_vel(playSlot, ml == 1 ? 5 : 4), delay + feel_delay_fr(playSlot));
}

// ── pad (bandbox's own voice — a slow sustained chord under the bar) ────────
static void play_pad(int delay) {
    if (!nbars) return;
    int on = arr[playSlot].pad < 0 ? von[V_PA] : arr[playSlot].pad;
    if (!on) return;
    int r0 = arr[playSlot].rootPc, q = arr[playSlot].qual;
    int base = 36 + (octSel - 1) * 12 + r0;      // low & warm
    int voices = bar_sev(playSlot) ? 4 : 3;
    for (int i = 0; i < voices; i++) {
        if (lit_voice() == V_PA) light_pc(base + hb_tones[q][i]);
        queue_note(base + hb_tones[q][i], I_PAD, feel_vel(playSlot, 3), delay + feel_delay_fr(playSlot));
    }
}

// FEEL — the comp rhythm + per-genre swing (baked, no knob).
static const char *COMP_PAT[NMODE] = {
    /* BOSSA */ "x..x.x..", /* LOUNGE */ "x...x...", /* POP */ "x...x...",
    /* FOLK */ "x..x.x..", /* MINOR */ "x..x..x.", /* CINE */ "x.......",
    /* DORIAN */ "..x..x..", /* MIXO */ "x..x.x..", /* PHRYG */ "x.xx.x..",
    /* LYDIAN */ "x.......", /* BLUES */ "x...x...",
    /* JANGLE */ "x.x.x.x.", /* JINGLE */ "x..x.x..", /* NAPOLN */ "..x.x.x.",
};
static const float SWING[NMODE] = {
    0.12f, 0.60f, 0.00f, 0.10f, 0.00f, 0.00f, 0.38f, 0.08f, 0.00f, 0.00f, 0.66f,
    /* JANGLE */ 0.00f, /* JINGLE */ 0.30f, /* NAPOLN */ 0.14f,
};

// ── analysis + suggestion (harmony brain, over the arrangement) ─────────────
static HbOpt sugg[4];
static int   nsugg = 0;
static void analyze(void) {
    int rp[NBARS], q[NBARS];
    for (int i = 0; i < nbars; i++) { rp[i] = arr[i].rootPc; q[i] = arr[i].qual; }
    hb_vocab_analyze(cur_vocab(), keyPc, rp, q, nbars, pfn);
}
static void resuggest(void) {
    nsugg = 0;
    if (!nbars) return;
    int last = pfn[nbars - 1];
    if (last < 0 || last >= MODES[modeSel].style->nfunc) return;
    nsugg = hb_suggest(MODES[modeSel].style, last, sugg, 4);
}
static void rethink(void) { analyze(); resuggest(); }

// KEY: STOPPED re-analyzes (the same chords re-spell); PLAYING transposes.
static void change_key(int delta) {
    keyPc = (keyPc + delta + 12) % 12;
    if (playing) for (int i = 0; i < nbars; i++) arr[i].rootPc = (arr[i].rootPc + delta + 12) % 12;
    rethink();
}

// LIVE EDIT — step a chord to the next/previous chord in the current mode's palette.
static void nudge_cell(int cell, int dir) {
    if (cell < 0 || cell >= nbars) return;
    const HbVocab *v = cur_vocab();
    int f = pfn[cell];
    if (f < 0) f = (dir > 0) ? -1 : v->n;
    f = (f + dir + v->n) % v->n;
    arr[cell].rootPc = (keyPc + v->off[f]) % 12;
    arr[cell].qual   = v->qual[f];
    rethink();
    audition(cell);
}
// keybed: set the chord's ROOT (keep its quality), re-analyze, audition.
static void set_root(int cell, int pc) {
    if (cell < 0 || cell >= nbars) return;
    arr[cell].rootPc = pc % 12;
    rethink();
    audition(cell);
}

static void bar_defaults(Bar *b) {
    b->strum = b->inv = b->oct = b->sev = -1;
    b->bass = -1; b->fill = 0; b->mel = -1; b->pad = -1; b->feel = FEEL_STRAIGHT;
}
// append a bar with a chosen chord (root + quality) — extends the loop, auditions.
static void append_chord(int rootPc, int qual) {
    if (nbars >= NBARS) return;
    bar_defaults(&arr[nbars]);
    arr[nbars].rootPc = rootPc % 12;
    arr[nbars].qual   = qual;
    nbars++;
    rethink();
    audition(nbars - 1);
}
// append the mode's tonic (the default I) — the spec's builder + the fallback.
static void add_bar(void) {
    const HbVocab *v = cur_vocab();
    append_chord((keyPc + v->off[0]) % 12, v->qual[0]);
}
// the diatonic quality for a root pitch-class (so a keybed-picked root sounds in
// the mode); an out-of-key root falls back to a plain major/dominant seventh.
static int qual_for_root(int pc) {
    const HbVocab *v = cur_vocab();
    for (int f = 0; f < v->n; f++) if ((keyPc + v->off[f]) % 12 == pc % 12) return v->qual[f];
    return HBQ_MAJ7;
}

// NEW SONG — empty the arrangement (nbars=0; build it back up with the "+" slot).
// Clears the chords + every p-lock but KEEPS your key / mode / voice setup — that's
// the session, not the song (like a groovebox: new pattern, same kit).
static void new_song(void) {
    for (int i = 0; i < NBARS; i++) bar_defaults(&arr[i]);
    nbars = 0;
    playing = 0; selVoice = -1; selBar = 0; helpOn = 0; picking = 0;
    rethink();
}

// cold-open with the doo-wop (I vi IV V) so a stranger sees the whole idea.
static void seed_demo(void) {
    static const int DW[4] = { HB_I, HB_vi, HB_IV, HB_V };
    nbars = 4;
    for (int i = 0; i < 4; i++) {
        bar_defaults(&arr[i]);
        arr[i].rootPc = (keyPc + hb_off[DW[i]]) % 12;
        arr[i].qual   = hb_qual[DW[i]];
    }
    rethink();
}

// ─────────────────────────────────────────────────────────────────────────
// THE FACE — the glass morphs (tracker <-> a tapped cell's editor); the chassis
// (rail / nav / keybed) never moves. Layout is computed ONCE per frame in draw();
// all input is ui.h immediate-mode in draw() + keyp() in update(), so taps never
// desync from visuals (bandbox.md's dubjam lesson).
// ─────────────────────────────────────────────────────────────────────────
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.13f, "nav"  },
    { FACE_HERO, 0,           0.00f, "body" },
    { FACE_BAND, EDGE_BOTTOM, 0.19f, "keys" },
};
#define NZ 3

// a draw-nothing tap hit-test (ui.h capture, touch+mouse), so custom cell/key
// visuals show through. Distinct seeds keep widget identities apart.
static int tapped(Box b, unsigned seed) {
    void *wid = ui_wid_hash(seed, (int)b.x, (int)b.y, (int)b.w, (int)b.h);
    int f = 0, p = 0, h = 0;
    return ui_button_core(wid, (int)b.x, (int)b.y, (int)b.w, (int)b.h, &f, &p, &h);
}

// a small tappable chassis chip: label over value.
static int chip(Box b, const char *lab, const char *val, int accent, unsigned seed) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, accent ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
    rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, accent ? CLR_ORANGE : CLR_DARKER_GREY);
    print(lab, (int)b.x + 2, (int)b.y + 2, CLR_INDIGO);
    print(val, (int)b.x + 2, (int)(b.y + b.h - 7), accent ? CLR_WHITE : CLR_LIGHT_GREY);
    return tapped(b, seed);
}
// a segmented option button (highlights when selected).
static int seg(Box b, const char *lab, int active, unsigned seed) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, active ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
    rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, active ? CLR_ORANGE : CLR_DARKER_GREY);
    print(lab, (int)(b.x + b.w / 2 - text_width(lab) / 2), (int)(b.y + b.h / 2 - 2),
          active ? CLR_WHITE : CLR_LIGHT_GREY);
    return tapped(b, seed);
}

static void zone_nav(Box b) {
    // < KEY x >
    Box kdec = lay_split(b, EDGE_LEFT, 9, &b);
    if (seg(kdec, "<", 0, 0x2101)) change_key(-1);
    Box key = lay_split(b, EDGE_LEFT, 32, &b);
    rrectfill((int)key.x, (int)key.y, (int)key.w, (int)key.h, 2, CLR_BROWNISH_BLACK);
    char kb[10]; snprintf(kb, sizeof kb, "KEY %s%s", NOTE[keyPc], MODES[modeSel].minorKey ? "m" : "");
    print(kb, (int)key.x + 2, (int)(key.y + key.h / 2 - 2), CLR_YELLOW);
    Box kinc = lay_split(b, EDGE_LEFT, 9, &b);
    if (seg(kinc, ">", 0, 0x2102)) change_key(+1);
    // play (right) + ? + MODE.  ► glyph is absent in FONT_TINY (ASCII-only), so the
    // play button reads by its GREEN lit state, not a triangle.
    Box play = lay_split(b, EDGE_RIGHT, 16, &b);
    rrectfill((int)play.x, (int)play.y, (int)play.w, (int)play.h, 2, playing ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
    rect((int)play.x, (int)play.y, (int)play.w, (int)play.h, playing ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    print(playing ? "II" : ">", (int)(play.x + play.w / 2 - text_width(playing ? "II" : ">") / 2),
          (int)(play.y + play.h / 2 - 2), playing ? CLR_LIME_GREEN : CLR_WHITE);
    if (tapped(play, 0x2103) && nbars) { playing = !playing; playSlot = 0; playT = 0;
        bassLast = 36 + (octSel - 1) * 12; melLast = 72 + (octSel - 1) * 12; melI = 0; }
    Box help = lay_split(b, EDGE_RIGHT, 10, &b);
    if (seg(help, "?", helpOn, 0x2104)) helpOn = !helpOn;
    Box neu = lay_split(b, EDGE_RIGHT, 20, &b);
    if (seg(neu, "NEW", 0, 0x2107)) new_song();   // empty the arrangement, keep the setup
    // MODE fills the rest
    Box mode = lay_inset(b, 1);
    if (seg(mode, MODES[modeSel].name, 0, 0x2105)) { modeSel = (modeSel + 1) % NMODE; rethink(); }
}

// the legend, drawn ON the glass (what the tracker + p-lock colours mean).
static void glass_help(Box g) {
    Box top = lay_split(g, EDGE_TOP, 11, &g);
    print("LEGEND", (int)top.x + 1, (int)(top.y + 3), CLR_WHITE);
    Box back = lay_split(top, EDGE_RIGHT, 30, &top);
    if (seg(back, "BACK", 0, 0x2106)) helpOn = 0;
    Box p = lay_inset(g, 2);
    int y = (int)p.y, x = (int)p.x;
    print("5 LANES: CH chords BA bass ME mel", x, y, CLR_LIGHT_GREY); y += 7;
    print("DR drums PA pad. tap a cell to edit", x, y, CLR_LIGHT_GREY); y += 7;
    rectfill(x, y + 1, 3, 3, CLR_MEDIUM_GREY); print("follows the chord + genre", x + 6, y, CLR_LIGHT_GREY); y += 7;
    rectfill(x, y + 1, 3, 3, CLR_ORANGE);      print("p-locked (a per-cell override)", x + 6, y, CLR_LIGHT_GREY); y += 7;
    print("tap a rail header to mute a lane", x, y, CLR_INDIGO); y += 7;
    print("+ picks a chord (or type A S D F.. like", x, y, CLR_INDIGO); y += 7;
    print("garageband). KEY: stopped respells, plays move", x, y, CLR_INDIGO);
}

// GLASS material — a recessed dark display panel with a bezel.
static Box glass_panel(Box b) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 3, CLR_BLACK);
    rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, CLR_DARK_GREY);
    return lay_inset(b, 3);
}

// the VOICE RAIL — row headers aligned to the glass lanes; tap = mute/unmute.
static void zone_rail(Box rail, Box g) {
    // FOCUS mode (editing a cell OR the chord picker): the active voice's lane is
    // promoted to the top row, so the rail shows just that voice's header (aligned to
    // the strip) + a big BACK below. Picking = adding to the CHORDS lane.
    if ((selVoice >= 0 || picking) && !helpOn) {
        int fv = picking ? V_CH : selVoice;
        Box r0 = lay_grid(g, 1, VOICES, 0, 1);
        Box r1 = lay_grid(g, 1, VOICES, 1, 1);
        Box hdr = box(rail.x, r0.y, rail.w, r0.h);
        rrectfill((int)hdr.x, (int)hdr.y, (int)hdr.w, (int)hdr.h, 2, CLR_DARK_BLUE);
        rect((int)hdr.x, (int)hdr.y, (int)hdr.w, (int)hdr.h, CLR_ORANGE);
        print(VL[fv], (int)(hdr.x + hdr.w / 2 - text_width(VL[fv]) / 2),
              (int)(hdr.y + hdr.h / 2 - 2), CLR_WHITE);
        Box back = box(rail.x, r1.y, rail.w, g.y + g.h - r1.y);
        if (seg(back, "BACK", 0, 0x2400)) { picking = 0; selVoice = -1; }
        return;
    }
    for (int v = 0; v < VOICES; v++) {
        Box lane = lay_grid(g, 1, VOICES, v, 1);
        Box row = box(rail.x, lane.y, rail.w, lane.h);
        int lit = von[v];
        rrectfill((int)row.x, (int)row.y, (int)row.w, (int)row.h, 2, CLR_BROWNISH_BLACK);
        rect((int)row.x, (int)row.y, (int)row.w, (int)row.h, lit ? CLR_ORANGE : CLR_DARKER_GREY);
        print(VL[v], (int)(row.x + row.w / 2 - text_width(VL[v]) / 2), (int)(row.y + row.h / 2 - 2),
              lit ? CLR_WHITE : CLR_DARK_GREY);
        rectfill((int)(row.x + row.w - 4), (int)row.y + 2, 2, 2, lit ? CLR_LIME_GREEN : CLR_DARK_GREY);
        if (tapped(row, 0x2200u + v)) von[v] = !von[v];   // any lane mutes/unmutes, chords included
    }
}

// the 5-LANE TRACKER. Chords = numeral; other voices = a pip (grey follows,
// orange = p-locked). Tap any cell to open its voice's editor.
static int cell_locked(int v, int i) {
    switch (v) {
        case V_CH: return arr[i].strum >= 0 || arr[i].inv >= 0 || arr[i].oct >= 0 || arr[i].sev >= 0;
        case V_BA: return arr[i].bass >= 0;
        case V_ME: return arr[i].mel  >= 0;
        case V_DR: return arr[i].fill != 0;
        case V_PA: return arr[i].pad  >= 0;
    }
    return 0;
}
static void glass_grid(Box g) {
    for (int v = 0; v < VOICES; v++) {
        Box lane = lay_grid(g, 1, VOICES, v, 1);
        int lit = von[v];   // chord numerals always draw (below); lit only gates the pips
        for (int i = 0; i < NBARS; i++) {
            Box c = lay_grid(lane, NBARS, NBARS, i, 1);
            int on = i < nbars, cur = playing && i == playSlot;
            rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1,
                      on ? (cur ? CLR_DARK_BLUE : CLR_DARKER_BLUE) : CLR_BROWNISH_BLACK);
            if (v == V_CH && on) {                 // chord — the numeral
                const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
                print(rn, (int)(c.x + c.w / 2 - text_width(rn) / 2), (int)(c.y + c.h / 2 - 2),
                      pfn[i] >= 0 ? (cur ? CLR_WHITE : CLR_YELLOW) : CLR_RED);
            } else if (v == V_CH && i == nbars) {  // the "add a bar" slot (bright when empty)
                print("+", (int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 2),
                      nbars == 0 ? CLR_LIME_GREEN : CLR_DARK_GREY);
            } else if (lit && on) {                // other voices — a pip
                int lk = cell_locked(v, i);
                rectfill((int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 1), 3, 3,
                         lk ? CLR_ORANGE : CLR_MEDIUM_GREY);
            }
            rect((int)c.x, (int)c.y, (int)c.w, (int)c.h, cur ? CLR_LIME_GREEN : CLR_DARKER_GREY);
            // tap → open editor (or the chord PICKER on the '+' slot)
            if (tapped(c, 0x2300u + v * 16 + i)) {
                if (v == V_CH && i == nbars) picking = 1;
                else if (on) { selVoice = v; selBar = i; }
            }
        }
    }
    if (nbars == 0) {   // an empty song — point at the "+" (in the MEL lane, clear of the grid)
        Box hint = lay_grid(g, 1, VOICES, V_ME, 1);
        const char *h = "NEW SONG - tap + to pick a chord";
        print(h, (int)(hint.x + hint.w / 2 - text_width(h) / 2), (int)(hint.y + hint.h / 2 - 2), CLR_INDIGO);
    }
}

// the ADD-CHORD picker (tap "+"). The harmony brain's NEXT suggestions ("what you
// prolly want") on top, the full mode palette below (roman-numeral chips, suggested
// ones flagged green), and the keybed live for ANY root. Each pick appends + auditions
// and stays open, so you build a progression fast.
// the CHORDS lane promoted at the top while picking — same in-place treatment as the
// editor: you watch the progression grow, the green "+" marks where the next lands.
static void pick_strip(Box lane) {
    for (int i = 0; i < NBARS; i++) {
        Box c = lay_grid(lane, NBARS, NBARS, i, 1);
        int on = i < nbars, cur = playing && i == playSlot, last = i == nbars - 1, ins = i == nbars;
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1,
                  on ? (cur ? CLR_DARK_BLUE : CLR_DARKER_BLUE) : CLR_BROWNISH_BLACK);
        if (on) {
            const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
            print(rn, (int)(c.x + c.w / 2 - text_width(rn) / 2), (int)(c.y + c.h / 2 - 2),
                  pfn[i] >= 0 ? (last ? CLR_WHITE : CLR_YELLOW) : CLR_RED);
        } else if (ins) {
            print("+", (int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 2), CLR_LIME_GREEN);
        }
        rect((int)c.x, (int)c.y, (int)c.w, (int)c.h,
             ins ? CLR_LIME_GREEN : (cur ? CLR_LIME_GREEN : CLR_DARKER_GREY));
    }
}
static void glass_pick(Box g) {
    const HbVocab *v = cur_vocab();
    char nm[16];

    // the promoted CHORDS lane (row 0, aligned to the rail's CH header); the picker
    // unfolds in the freed space below — same shape as the in-place voice editors.
    Box r0 = lay_grid(g, 1, VOICES, 0, 1);
    Box r1 = lay_grid(g, 1, VOICES, 1, 1);
    pick_strip(r0);
    Box s = box(g.x, r1.y, g.w, g.y + g.h - r1.y);

    // which functions to recommend: the ranked suggestions from the last chord,
    // or the tonic when the song is empty (the natural place to start).
    int sf[4], ns = 0;
    if (nbars > 0 && nsugg > 0) { for (int i = 0; i < nsugg && i < 4; i++) sf[ns++] = sugg[i].f; }
    else sf[ns++] = 0;

    // NEXT row — the recommended chords, big, with their names.
    Box nx = lay_split(s, EDGE_TOP, 15, &s);
    Box nlab = lay_split(nx, EDGE_LEFT, 18, &nx);
    print("NEXT", (int)nlab.x, (int)(nlab.y + nlab.h / 2 - 2), CLR_INDIGO);
    for (int i = 0; i < ns; i++) {
        Box ch = lay_grid(nx, 4, 4, i, 1);
        int pc = (keyPc + v->off[sf[i]]) % 12;
        chname(nm, sizeof nm, pc, v->qual[sf[i]], seventh);
        rrectfill((int)ch.x, (int)ch.y, (int)ch.w, (int)ch.h, 2, CLR_DARK_BLUE);
        rect((int)ch.x, (int)ch.y, (int)ch.w, (int)ch.h, CLR_LIME_GREEN);
        const char *rn = v->fname[sf[i]];
        print(rn, (int)(ch.x + ch.w / 2 - text_width(rn) / 2), (int)ch.y + 1, CLR_WHITE);
        print(nm, (int)(ch.x + ch.w / 2 - text_width(nm) / 2), (int)ch.y + 8, CLR_LIGHT_GREY);
        if (tapped(ch, 0x2610u + i)) append_chord(pc, v->qual[sf[i]]);
    }

    // PALETTE — every diatonic chord as a roman-numeral chip; recommended ones green.
    int n = v->n, cols = (n + 1) / 2;
    for (int f = 0; f < n; f++) {
        Box ch = lay_grid(s, cols, cols * 2, f, 1);
        int isSug = 0; for (int i = 0; i < ns; i++) if (sf[i] == f) isSug = 1;
        rrectfill((int)ch.x, (int)ch.y, (int)ch.w, (int)ch.h, 1, CLR_BROWNISH_BLACK);
        rect((int)ch.x, (int)ch.y, (int)ch.w, (int)ch.h, isSug ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        const char *rn = v->fname[f];
        print(rn, (int)(ch.x + ch.w / 2 - text_width(rn) / 2), (int)(ch.y + ch.h / 2 - 2),
              isSug ? CLR_WHITE : CLR_LIGHT_GREY);
        if (tapped(ch, 0x2620u + f)) append_chord((keyPc + v->off[f]) % 12, v->qual[f]);
    }
    if (nbars >= NBARS) print("(full)", (int)s.x + 1, (int)(s.y + s.h - 6), CLR_DARK_GREY);
}

// ── the block EDITORS, drawn ON the glass (chassis stays put) ───────────────
// the voice EDITORS below draw only their settings into the box they're given (the
// area under the promoted lane strip); the strip + rail-BACK are the header now.
static void editor_chords(Box g) {
    // global TONE chip on top (the comping sound), then the spinner + p-lock chips
    Box tRow = lay_split(g, EDGE_TOP, 12, &g);
    if (chip(lay_inset(tRow, 1), "TONE", CTONE_LAB[chordTone], 1, 0x2424)) {
        chordTone = (chordTone + 1) % 3; apply_chord_tone();
    }
    // left: the ^ name v spinner
    Box spin = lay_split(g, EDGE_LEFT, g.w * 0.42f, &g);
    Box up = lay_split(spin, EDGE_TOP, 8, &spin);
    Box dn = lay_split(spin, EDGE_BOTTOM, 8, &spin);
    if (seg(up, "^", 0, 0x2410)) nudge_cell(selBar, +1);
    if (seg(dn, "v", 0, 0x2411)) nudge_cell(selBar, -1);
    rrectfill((int)spin.x, (int)spin.y, (int)spin.w, (int)spin.h, 2, CLR_DARKER_BLUE);
    char nm[16]; chname(nm, sizeof nm, arr[selBar].rootPc, arr[selBar].qual, bar_sev(selBar));
    print(nm, (int)(spin.x + spin.w / 2 - text_width(nm) / 2), (int)(spin.y + spin.h / 2 - 5), CLR_WHITE);
    const char *rn = pfn[selBar] >= 0 ? cur_vocab()->fname[pfn[selBar]] : "?";
    print(rn, (int)(spin.x + spin.w / 2 - text_width(rn) / 2), (int)(spin.y + spin.h / 2 + 2),
          pfn[selBar] >= 0 ? CLR_YELLOW : CLR_RED);

    // right: 2x2 grid of per-cell p-lock chips (AUTO = follow the global voicing)
    char v0[8], v1[8], v2[8], v3[8];
    Bar *b = &arr[selBar];
    snprintf(v0, sizeof v0, "%s", b->strum < 0 ? "AUTO" : STRUMS[b->strum].name);
    snprintf(v1, sizeof v1, "%s", b->inv   < 0 ? "AUTO" : INV_LAB[b->inv]);
    snprintf(v2, sizeof v2, "%s", b->oct   < 0 ? "AUTO" : OCT_LAB[b->oct]);
    snprintf(v3, sizeof v3, "%s", b->sev   < 0 ? "AUTO" : (b->sev ? "7TH" : "TRIAD"));
    Box p = lay_inset(g, 1);
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    Box a = lay_grid(r0, 2, 2, 0, 1), bb = lay_grid(r0, 2, 2, 1, 1);
    Box c = lay_grid(r1, 2, 2, 0, 1), d = lay_grid(r1, 2, 2, 1, 1);
    if (chip(a,  "STRUM", v0, b->strum >= 0, 0x2420)) b->strum = (b->strum + 2) % (NSTRUM + 1) - 1;
    if (chip(bb, "INV",   v1, b->inv   >= 0, 0x2421)) b->inv   = (b->inv   + 2) % ((bar_sev(selBar) ? 4 : 3) + 1) - 1;
    if (chip(c,  "OCT",   v2, b->oct   >= 0, 0x2422)) b->oct   = (b->oct   + 2) % 4 - 1;
    if (chip(d,  "7TH",   v3, b->sev   >= 0, 0x2423)) b->sev   = (b->sev   + 2) % 3 - 1;
}

// BASS editor: the global STYLE chip + the per-cell p-lock vocab (2x3):
// FOLLOW the style, or override this one bar — MUTE / HOLD / WALK / OCT / FILL.
static void editor_bass(Box g) {
    Box styRow = lay_split(g, EDGE_TOP, 13, &g);
    Box styB = lay_grid(styRow, 2, 2, 0, 1), tonB = lay_grid(styRow, 2, 2, 1, 1);
    char sv[8]; snprintf(sv, sizeof sv, "%s", BASS_LAB[bassSel]);
    if (chip(lay_inset(styB, 1), "STYLE", sv, 1, 0x2430)) bassSel = (bassSel + 1) % 4;
    if (chip(lay_inset(tonB, 1), "TONE", BTONE_LAB[bassTone], 1, 0x243A)) {
        bassTone = (bassTone + 1) % 4; apply_bass_tone();   // set-and-hold: re-voice only on tap
    }
    Box p = lay_inset(g, 1);
    int bl = arr[selBar].bass;
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    for (int i = 0; i < 3; i++) {
        if (seg(lay_grid(r0, 3, 3, i, 1), BPL_LAB[i],     bl == BPL_VAL[i],     0x2431u + i))
            arr[selBar].bass = BPL_VAL[i];
        if (seg(lay_grid(r1, 3, 3, i, 1), BPL_LAB[i + 3], bl == BPL_VAL[i + 3], 0x2434u + i))
            arr[selBar].bass = BPL_VAL[i + 3];
    }
}
// DRUMS editor: global STYLE + KIT chips, then the per-cell vocab (2x3):
// GROOVE / DROP / KICK / FILL / CRASH / BUILD.
static void editor_drums(Box g) {
    Box styRow = lay_split(g, EDGE_TOP, 13, &g);
    Box styB = lay_grid(styRow, 2, 2, 0, 1), kitB = lay_grid(styRow, 2, 2, 1, 1);
    char sv[8]; snprintf(sv, sizeof sv, "%s", DRUM_LAB[drumSel]);
    if (chip(lay_inset(styB, 1), "STYLE", sv, 1, 0x2440)) drumSel = (drumSel + 1) % 2;
    if (chip(lay_inset(kitB, 1), "KIT", KIT_LAB[kitSel], 1, 0x2443)) {
        kitSel = !kitSel; dk_use(KITS[kitSel], 20);   // set-and-hold: swap only on tap
    }
    Box p = lay_inset(g, 1);
    int dp = arr[selBar].fill;
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    for (int i = 0; i < 3; i++) {
        if (seg(lay_grid(r0, 3, 3, i, 1), DPL_LAB[i],     dp == i,     0x2444u + i)) arr[selBar].fill = i;
        if (seg(lay_grid(r1, 3, 3, i, 1), DPL_LAB[i + 3], dp == i + 3, 0x2447u + i)) arr[selBar].fill = i + 3;
    }
}
static void editor_mel(Box g) {
    Box tRow = lay_split(g, EDGE_TOP, 13, &g);
    if (chip(lay_inset(tRow, 1), "TONE", MTONE_LAB[melTone], 1, 0x2454)) {
        melTone = (melTone + 1) % 4; apply_mel_tone();
    }
    Box p = lay_inset(g, 1);
    int ml = arr[selBar].mel;
    if (seg(lay_grid(p, 3, 3, 0, 1), "FOLLOW", ml < 0, 0x2451)) arr[selBar].mel = -1;
    if (seg(lay_grid(p, 3, 3, 1, 1), "REST",   ml == 0, 0x2452)) arr[selBar].mel = 0;
    if (seg(lay_grid(p, 3, 3, 2, 1), "ACCENT", ml == 1, 0x2453)) arr[selBar].mel = 1;
}
static void editor_pad(Box g) {
    Box tRow = lay_split(g, EDGE_TOP, 13, &g);
    if (chip(lay_inset(tRow, 1), "TONE", PTONE_LAB[padTone], 1, 0x2464)) {
        padTone = (padTone + 1) % 3; apply_pad_tone();
    }
    Box p = lay_inset(g, 1);
    int pl = arr[selBar].pad;
    if (seg(lay_grid(p, 3, 3, 0, 1), "FOLLOW", pl < 0, 0x2461)) arr[selBar].pad = -1;
    if (seg(lay_grid(p, 3, 3, 1, 1), "OFF",    pl == 0, 0x2462)) arr[selBar].pad = 0;
    if (seg(lay_grid(p, 3, 3, 2, 1), "ON",     pl == 1, 0x2463)) arr[selBar].pad = 1;
}
// the promoted lane — the selected voice's 8 cells, kept visible at the top row so
// edits land IN PLACE. The selected bar is yellow; tap another cell to scrub to it.
static void focus_strip(Box lane) {
    for (int i = 0; i < NBARS; i++) {
        Box c = lay_grid(lane, NBARS, NBARS, i, 1);
        int on = i < nbars, cur = playing && i == playSlot, sel = i == selBar;
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1,
                  on ? (cur ? CLR_DARK_BLUE : CLR_DARKER_BLUE) : CLR_BROWNISH_BLACK);
        if (selVoice == V_CH && on) {
            const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
            print(rn, (int)(c.x + c.w / 2 - text_width(rn) / 2), (int)(c.y + c.h / 2 - 2),
                  pfn[i] >= 0 ? (sel ? CLR_WHITE : CLR_YELLOW) : CLR_RED);
        } else if (on) {
            int lk = cell_locked(selVoice, i);
            rectfill((int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 1), 3, 3,
                     lk ? CLR_ORANGE : CLR_MEDIUM_GREY);
        }
        rect((int)c.x, (int)c.y, (int)c.w, (int)c.h,
             sel ? CLR_YELLOW : (cur ? CLR_LIME_GREEN : CLR_DARKER_GREY));
        if (on && tapped(c, 0x2700u + i)) selBar = i;
    }
}
static void glass_editor(Box g) {
    // the promoted lane stays at the top (row 0 of the shared 5-slot register, so it
    // aligns with the rail header); the settings unfold in the freed space below.
    Box r0 = lay_grid(g, 1, VOICES, 0, 1);
    Box r1 = lay_grid(g, 1, VOICES, 1, 1);
    focus_strip(r0);
    Box s = box(g.x, r1.y, g.w, g.y + g.h - r1.y);
    Box hdr = lay_split(s, EDGE_TOP, 9, &s);
    // FEEL is per-BAR (the whole band's groove), so it lives in the bar header — set it
    // from any voice's editor; ACCENT/DRAG show orange.
    Box fbox = lay_split(hdr, EDGE_RIGHT, 46, &hdr);
    if (seg(fbox, FEEL_LAB[arr[selBar].feel], arr[selBar].feel != FEEL_STRAIGHT, 0x2480))
        arr[selBar].feel = (arr[selBar].feel + 1) % 3;
    char nm[16]; chname(nm, sizeof nm, arr[selBar].rootPc, arr[selBar].qual, bar_sev(selBar));
    const char *rn = pfn[selBar] >= 0 ? cur_vocab()->fname[pfn[selBar]] : "?";
    char t[28]; snprintf(t, sizeof t, "BAR %d  %s %s", selBar + 1, nm, rn);
    print(t, (int)hdr.x + 1, (int)hdr.y + 2, CLR_INDIGO);
    switch (selVoice) {
        case V_CH: editor_chords(s); break;
        case V_BA: editor_bass(s);   break;
        case V_ME: editor_mel(s);    break;
        case V_DR: editor_drums(s);  break;
        case V_PA: editor_pad(s);    break;
    }
}

// a key press (on-screen keybed OR the QWERTY musical typing) → PICKING appends a
// chord on that root (diatonic quality if it fits the key); EDITING a chord cell
// re-roots it; IDLE opens the picker and appends (so you can just start typing chords).
static void keybed_pick(int pc) {
    if (!picking && selVoice == V_CH) { set_root(selBar, pc); return; }
    if (!picking) picking = 1;
    append_chord(pc, qual_for_root(pc));
}
// a tiny keybed (chassis) — live while ADDING (the picker) or EDITING a chord.
static void zone_keybed(Box b) {
    static const int WPC[10] = { 0, 2, 4, 5, 7, 9, 11, 0, 2, 4 };   // C D E F G A B C D E
    int editing = (selVoice == V_CH && !picking);
    int active = picking || editing;
    int hlPc = editing ? arr[selBar].rootPc % 12 : (picking && nbars > 0 ? arr[nbars - 1].rootPc % 12 : -1);
    int rootW = -1;
    if (hlPc >= 0) for (int i = 0; i < 10; i++) if (WPC[i] == hlPc) { rootW = i; break; }
    float ww = b.w / 10.0f;
    for (int i = 0; i < 10; i++) {                  // white keys — the note-lights glow green
        Box k = box(b.x + i * ww, b.y, ww - 1, b.h);
        int held = i == rootW, pc = WPC[i];
        int col = held ? CLR_ORANGE : (active ? CLR_WHITE : CLR_LIGHT_GREY);
        if (!held) { if (keyLit[pc] > 0.55f) col = CLR_LIME_GREEN; else if (keyLit[pc] > 0.18f) col = CLR_GREEN; }
        rrectfill((int)k.x, (int)k.y, (int)k.w, (int)k.h, 1, col);
        if (active && tapped(k, 0x2500u + i)) keybed_pick(pc);
    }
    for (int i = 0; i < 9; i++) {                  // black keys (right of C,D,F,G,A)
        int s = i % 7;
        if (s == 2 || s == 6) continue;
        int pc = (WPC[i] + 1) % 12;
        Box bk = box(b.x + (i + 1) * ww - ww * 0.32f, b.y, ww * 0.64f, b.h * 0.6f);
        int col = CLR_BLACK;
        if (keyLit[pc] > 0.55f) col = CLR_LIME_GREEN; else if (keyLit[pc] > 0.18f) col = CLR_GREEN;
        rectfill((int)bk.x, (int)bk.y, (int)bk.w, (int)bk.h, col);
        if (active && tapped(bk, 0x2510u + i)) keybed_pick(pc);
    }
}

void init(void) {
    apply_chord_tone();                              // each voice's sound = its TONE selector
    apply_bass_tone();
    apply_mel_tone();
    apply_pad_tone();
    dk_use(KITS[kitSel], 20);                        // the rhythm section — slots 20..27
    seed_demo();
}

void update(void) {
    // keyboard convenience (the touch UI is the real interface)
    if (keyp(KEY_LEFT))  change_key(-1);
    if (keyp(KEY_RIGHT)) change_key(+1);
    if (keyp('B'))       { modeSel = (modeSel + 1) % NMODE; rethink(); }
    if (keyp(KEY_BACKSPACE)) { selVoice = -1; picking = 0; }
    if (keyp('N'))           new_song();   // start a fresh empty song

    // GarageBand-style MUSICAL TYPING — the QWERTY rows are a keyboard that drives
    // the keybed (a chord root). Home row A S D F G H J K L ; = the white keys
    // (C D E F G A B C D E), top row W E T Y U O P = the black keys. Each press
    // adds/edits a chord (via keybed_pick), so you can type a progression.
    { static const struct { int k, pc; } MT[] = {
        {'A',0},{'W',1},{'S',2},{'E',3},{'D',4},{'F',5},{'T',6},{'G',7},
        {'Y',8},{'H',9},{'U',10},{'J',11},{'K',0},{'O',1},{'L',2},{'P',3},{';',4} };
      for (int i = 0; i < (int)(sizeof MT / sizeof MT[0]); i++)
          if (keyp(MT[i].k)) keybed_pick(MT[i].pc); }
    if (keyp(KEY_SPACE) && nbars) { playing = !playing; playSlot = 0; playT = 0;
        bassLast = 36 + (octSel - 1) * 12; melLast = 72 + (octSel - 1) * 12; melI = 0; }

    if (playing && nbars) {
        int bf = bar_frames();
        int b0 = playSlot;
        int r0 = arr[b0].rootPc, q = arr[b0].qual;
        int swMax = (int)(SWING[modeSel] * bf / 12.0f);

        int stepLen = bf / 8; if (stepLen < 1) stepLen = 1;
        if (playT % stepLen == 0) {
            int s = playT / stepLen;
            if (s < 8) {
                int sw = (s == 2 || s == 6) ? swMax : 0;
                if (von[V_CH] && COMP_PAT[modeSel][s] == 'x') {   // the chords lane can be muted too
                    sound_chord(r0, q, 0, sw + feel_delay_fr(b0), bar_strum(b0), bar_inv(b0),
                                bar_oct(b0), bar_sev(b0), feel_vel(b0, 5));
                    if (lit_voice() == V_CH) {   // light the chord tones as it comps
                        int nv = bar_sev(b0) ? 4 : 3;
                        for (int i = 0; i < nv; i++) light_pc(r0 + hb_tones[q][i]);
                    }
                }
                play_drum_step(s, sw * 1000 / 60);
                play_mel_step(s, sw);
                if (s == 0) play_pad(sw);          // the pad enters on the downbeat
            }
        }
        int beatLen = bf / 4; if (beatLen < 1) beatLen = 1;
        if (playT % beatLen == 0) {
            int beat = playT / beatLen;
            if (beat < 4) play_bass_beat(beat, (beat == 1 || beat == 3) ? swMax : 0);
        }
        if (++playT >= bf) { playT = 0; playSlot = (playSlot + 1) % nbars; }
    } else playSlot = 0;

    for (int i = 0; i < 12; i++) keyLit[i] *= 0.86f;   // the note-lights fade out
    pump_notes();
}

void draw(void) {
    face_resize();
    Box area = face_area(3);
    Face f = face_layout(area, ZONES, NZ, NBARS);

    cls(CLR_DARKER_BLUE);
    rrectfill(0, 0, screen_w(), screen_h(), 6, CLR_INDIGO);   // the molded device body

    if (nbars && selBar >= nbars) selBar = nbars - 1;         // stay valid if the loop shrank

    font(FONT_TINY);
    ui_begin();
    zone_nav(f.box[0]);
    Box body = f.box[1];
    Box rail = lay_split(body, EDGE_LEFT, 18, &body);
    Box g = glass_panel(body);
    zone_rail(rail, g);
    if (picking)             glass_pick(g);
    else if (helpOn)         glass_help(g);
    else if (selVoice >= 0)  glass_editor(g);
    else                     glass_grid(g);
    zone_keybed(f.box[2]);
    font(FONT_NORMAL);
    ui_end();
    cursor_draw(CUR_HAND);
}

// ── spec — the deterministic oracle (bandbox.md step 6) ─────────────────────
#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(1);                                   // init once
    hb_selfcheck();                            // the harmony brain's own assertions

    // cold-open = the doo-wop, analyzed
    expect_eq(nbars, 4, "cold-open seeds four bars");
    expect(pfn[0]==HB_I && pfn[1]==HB_vi && pfn[2]==HB_IV && pfn[3]==HB_V,
           "cold-open reads I vi IV V");

    // the arrangement re-analyzes under a new key (chords re-spell, honestly).
    // C Am7 Fmaj7 G7 (I vi IV V in C) heard in G: the G7 is the dominant-quality
    // tonic = I7 (the borrowed dominant), not the maj7 I.
    keyPc = 7; rethink();
    expect(pfn[3] == HB_I7, "G7 heard in G = I7 (dominant tonic, not maj7 I)");
    keyPc = 0; rethink();
    expect(pfn[0] == HB_I, "back in C, C major = I");

    // the spinner steps a chord in-key (I -> ii) and re-analyzes
    nudge_cell(0, +1);
    expect(pfn[0] == HB_ii, "nudge I -> ii (next in key)");
    nudge_cell(0, -1);
    expect(pfn[0] == HB_I, "nudge back ii -> I");

    // the keybed sets a chord's root (A over the maj7 quality = Amaj7 = ? in C)
    set_root(0, 9);
    expect_eq(arr[0].rootPc, 9, "keybed sets bar 0 root to A");
    set_root(0, 0);   // put it back to C
    expect(pfn[0] == HB_I, "root back to C = I");

    // p-locks are per-cell and default to AUTO (inherit) — and RESOLVE correctly
    expect(arr[0].strum < 0 && arr[0].bass < 0 && arr[0].mel < 0 && arr[0].pad < 0,
           "fresh bars are all AUTO");
    strumSel = 2;                                     // global UP-stroke
    expect_eq(bar_strum(0), 2, "AUTO bar follows the global strum");
    arr[0].strum = 0;                                 // p-lock bar 0 to BLOCK
    expect_eq(bar_strum(0), 0, "p-locked bar uses its own strum, not the global");
    expect_eq(bar_strum(1), 2, "a neighbour still follows the global");
    arr[0].strum = -1; strumSel = 0;                  // reset

    // the drum per-cell vocab (GROOVE is the default; the rest are one-bar moves)
    expect_eq(arr[3].fill, DPL_GROOVE, "bars default to GROOVE");
    expect(!cell_locked(V_DR, 3), "a GROOVE drum cell reads as unlocked");
    arr[3].fill = DPL_FILL;  expect(cell_locked(V_DR, 3), "FILL reads as a p-lock");
    arr[3].fill = DPL_CRASH; expect_eq(arr[3].fill, DPL_CRASH, "drum CRASH p-lock sets");
    arr[3].fill = DPL_BUILD; expect_eq(arr[3].fill, DPL_BUILD, "drum BUILD p-lock sets");
    arr[3].fill = DPL_GROOVE;

    // the bass per-cell vocab: FOLLOW is the AUTO default, the rest are one-bar overrides
    expect_eq(arr[0].bass, BPL_FOLLOW, "bars default to FOLLOW the bass style");
    expect(!cell_locked(V_BA, 0), "a FOLLOW bass cell reads as unlocked");
    arr[0].bass = BPL_HOLD; expect(cell_locked(V_BA, 0), "HOLD reads as a p-lock");
    arr[0].bass = BPL_OCT;  expect_eq(arr[0].bass, BPL_OCT,  "bass OCT p-lock sets");
    arr[0].bass = BPL_FILL; expect_eq(arr[0].bass, BPL_FILL, "bass FILL p-lock sets");
    arr[0].bass = BPL_FOLLOW;

    // every voice TONE preset is a valid engine setup (applies without fault)
    for (bassTone = 0; bassTone < 4; bassTone++) apply_bass_tone();   bassTone = 0;  apply_bass_tone();
    for (chordTone = 0; chordTone < 3; chordTone++) apply_chord_tone(); chordTone = 0; apply_chord_tone();
    for (melTone = 0; melTone < 4; melTone++) apply_mel_tone();       melTone = 0;   apply_mel_tone();
    for (padTone = 0; padTone < 3; padTone++) apply_pad_tone();       padTone = 0;   apply_pad_tone();
    expect(1, "all voice TONE presets apply (bass/chord/mel/pad)");

    // every lane can be muted — the chords included (no longer forced on)
    expect_eq(von[V_CH], 1, "chords lane starts on");
    von[V_CH] = 0; expect_eq(von[V_CH], 0, "chords lane can be muted too");
    von[V_CH] = 1;

    // FEEL p-lock (per-bar): ACCENT lifts velocity, DRAG lags the whole bar, deterministically
    expect_eq(arr[0].feel, FEEL_STRAIGHT, "bars default to STRAIGHT feel");
    arr[0].feel = FEEL_ACCENT; expect_eq(feel_vel(0, 5), 7, "ACCENT lifts velocity (+2, clamped)");
    expect_eq(feel_delay_fr(0), 0, "ACCENT does not drag");
    arr[0].feel = FEEL_DRAG;   expect_eq(feel_delay_fr(0), FEEL_DRAG_FR, "DRAG lags the bar");
    expect_eq(feel_vel(0, 5), 5, "DRAG does not lift velocity");
    arr[0].feel = FEEL_STRAIGHT;

    // add_bar extends the loop with a default I chord
    add_bar();
    expect_eq(nbars, 5, "add_bar extends the loop");
    expect(pfn[4] == HB_I, "the added bar defaults to I in the key");

    // KEY while PLAYING transposes (chords move, numerals stay)
    nbars = 4; playing = 1; keyPc = 0; seed_demo(); playing = 1;
    change_key(+2);                                   // up a whole step to D
    expect(arr[0].rootPc == 2 && pfn[0] == HB_I, "playing key +2: chord moved to D, still I");
    playing = 0; keyPc = 0; seed_demo();

    // MODE cycles the whole tonal system: minor re-analyzes over its own vocab
    modeSel = 4; keyPc = 9; nbars = 3;                // A minor
    bar_defaults(&arr[0]); arr[0].rootPc = 9; arr[0].qual = HBQ_MIN7;   // Am7 = i
    bar_defaults(&arr[1]); arr[1].rootPc = 5; arr[1].qual = HBQ_MAJ7;   // Fmaj7 = VI
    bar_defaults(&arr[2]); arr[2].rootPc = 0; arr[2].qual = HBQ_MAJ7;   // Cmaj7 = III
    rethink();
    expect(pfn[0]==HBm_i && pfn[1]==HBm_VI && pfn[2]==HBm_III,
           "minor strip reads i VI III in A minor");

    // a chord outside the vocab is honest (?), never guessed
    add_bar();                                        // adds i (A minor's function 0)
    arr[3].rootPc = 6; arr[3].qual = HBQ_MIN7;        // F#m7 — no seat in A minor
    rethink();
    expect_eq(pfn[3], -1, "F#m7 in A minor -> ? (outside the vocab)");

    // BLUES: A7 D7 E7 in A = I7 IV7 V7
    modeSel = 10; keyPc = 9; nbars = 3;
    bar_defaults(&arr[0]); arr[0].rootPc = 9; arr[0].qual = HBQ_DOM7;
    bar_defaults(&arr[1]); arr[1].rootPc = 2; arr[1].qual = HBQ_DOM7;
    bar_defaults(&arr[2]); arr[2].rootPc = 4; arr[2].qual = HBQ_DOM7;
    rethink();
    expect(pfn[0]==HBbl_I && pfn[1]==HBbl_IV && pfn[2]==HBbl_V,
           "blues reads I7 IV7 V7 in A");

    // NEW SONG empties the arrangement; the "+" slot builds it back
    modeSel = 0; keyPc = 0;
    new_song();
    expect_eq(nbars, 0, "new_song empties the arrangement");
    expect(!playing && selVoice < 0 && !picking, "new_song stops + closes editor/picker");
    add_bar();
    expect_eq(nbars, 1, "the + slot adds the first bar back (a I in the key)");

    // the chord picker: append_chord + the keybed's diatonic-quality helper
    new_song();
    expect_eq(qual_for_root(7), HBQ_DOM7, "qual_for_root: G in C major = V's dominant 7");
    append_chord((keyPc + hb_off[HB_V]) % 12, hb_qual[HB_V]);
    expect(nbars == 1 && pfn[0] == HB_V, "picker append: a V lands as V");

    // musical typing (idle keybed_pick) opens the picker + appends that root
    new_song();
    keybed_pick(7);
    expect(picking && nbars == 1 && arr[0].rootPc == 7,
           "idle keybed_pick opens the picker + appends (G)");
    picking = 0;

    // the radio-idea genres (jangle/jingle/napoleon) — appended, analyze on their vocab
    expect_eq(NMODE, 14, "three radio-idea genres added");
    modeSel = 11; keyPc = 0; seed_demo();   // JANGLE rides the POP vocab
    expect(pfn[0] == HB_I && pfn[3] == HB_V, "jangle reads the doo-wop I .. V (pop vocab)");

    // JINGLE has its OWN harmony now (Mac DeMarco's gravity): from I it leans bVII7
    modeSel = 12; keyPc = 0; nbars = 1; bar_defaults(&arr[0]);
    arr[0].rootPc = 0; arr[0].qual = HBQ_MAJ7; rethink();
    expect(pfn[0] == HB_I, "jingle: Cmaj7 = I");
    expect(nsugg >= 1 && sugg[0].f == HB_bVII7, "jingle from I leans bVII7 (Mac's flat-7 home)");

    // back to a clean state
    modeSel = 0; keyPc = 0; seed_demo();
    expect_eq(nbars, 4, "reset to the cold-open");
}
#endif
