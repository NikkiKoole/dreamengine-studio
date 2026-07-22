/* de:meta
{
  "slug": "chordwise",
  "title": "chordwise",
  "status": "active",
  "created": "2026-07-20",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "functional-harmony"
  ],
  "lineage": "The first consumer of runtime/harmony.h — the shared harmony brain extracted byte-identically from bossa/cocktail's Markov chord engines (design/harmony-brain.md). Built for the loudest demand-discovery gap on record: r/musictheory demand-82, 'a harmonic progression analyzer / suggestion tool'. Analysis is generation inverted: the same function vocab + transition table that composes bossa's changes here names YOUR chords and ranks what could come next.",
  "description": {
    "summary": "Build a chord progression by tapping chords in a key - the strip names each one in roman numerals (I, ii, V...) and a NEXT panel suggests where the harmony wants to go, ranked, each with a one-word reason (cadence, home, tritone sub). Change the key and watch the same chords get re-named - or turn un-nameable (?). Eleven modes span MAJOR (bossa/lounge/pop/folk), MINOR (sad-pop + cinematic), all four church MODES (dorian/mixolydian/phrygian/lydian) and BLUES, and a 7THS/TRIAD toggle reads (and voices) plain triads (Am F C G) for beginners or full sevenths for jazz. A STRUM picker rakes the pluck as a block chord, a down/up-stroke or a slow roll, an INV picker voices the chord in root position or an inversion (the bass note changes, so you hear it), an OCT picker drops or lifts the whole voicing a register (the way a pianist plays a chord an octave down), a BASS picker plays a generated bassline under the loop (HOLD root, R-5 root+fifth pump, WALK a quarter-note jazz walk INTO the next chord's root, or BAND - a genre-aware bassist that reads the declared mode and plays its idiom: bossa root-fifth, folk boom-chuck, rock root-fifth+b7, phrygian b2, dorian natural-6, the 12-bar boogie shuffle...), a DRUMS picker adds the rest of the rhythm section on the shared kit (OFF, a PLAIN genre-agnostic backbeat, or BAND - the mode's own groove: bossa surdo+rim, EDM four-on-floor+claps, lofi boom-bap, rock, flamenco palmas, blues shuffle, a big sparse trailer kick...), a MEL toggle adds a lead melody that blooms from the chords (a genre-rhythmed line arpeggiating each chord's tones + the 9th, voice-led so it sings), and a BPM slider sets the loop tempo (60-180). SPACE loops your changes - and the band plays with FEEL, not dead on the grid: the chord COMPS on a per-genre rhythm (bossa clave, jazz 4-to-the-bar, a rock chunk, one held cinematic hit...) and the off-beats SWING per genre (jazz/blues/lofi swing hard, pop/EDM/rock stay straight because straight is their feel). A ? button opens a legend explaining the roman numerals and every NEXT reason-word (home, cadence, walk, borrow, tritone sub...).",
    "detail": "The demand-82 toy: a progression analyzer + next-chord suggester on the shared harmony brain (runtime/harmony.h, lifted byte-identically from the bossa/cocktail radio stations). B cycles eleven MODES that each swap the whole tonal system - the palette respells, the strip re-analyses, NEXT re-ranks. Four ride the 13-function MAJOR vocab, differently weighted (the research thesis - genres differ by WEIGHTS, not grammar): BOSSA (jazz - 6 diatonic seats + the borrowed/chromatic shelf: II7/VI7, tritone sub bII7, backdoor iv/bVII7, v, I7), LOUNGE (cocktail's 10-function trio tuning), POP (the I-V-vi-IV axis + 50s doo-wop), FOLK (IV-forward three-chord-song staples, plainer than pop). Two ride the MINOR vocab (i ii° III iv v V VI VII vii°, natural + harmonic-minor V): MINOR (sad-pop/EDM) and CINE (the epic i-VI-III-VII trailer climb). Then the four CHURCH MODES, each its own vocab with one characteristic chord: DORIAN (minor + a bright MAJOR IV, the lo-fi/neo-soul i<->IV vamp), MIXOLYDIAN (major + a flat 7, the bVII-IV-I rock cadence), PHRYGIAN (minor + a flat 2, the bII-i flamenco/metal half-step), LYDIAN (major + a sharp 4, the floating I<->II film sound). BLUES is I7 IV7 V7, the 12-bar's whole world. Analysis stays honest in every mode because the key is DECLARED, never guessed. Tap chords into an 8-slot strip; every frame the strip is RE-ANALYZED from the raw chords (root + quality) against the current mode's vocab, so the roman numerals are honest lookups, not echoes of what you pressed - move the key OR the mode under a finished progression and the same chords re-name (Cmaj7: I in C major, III in A minor), and chords outside the mode's vocabulary show ? instead of a guess. Minor/blues analysis works because the key is DECLARED, never guessed (auto-detecting major-vs-minor from raw chords is the unsolved part, which this toy sidesteps by design). The NEXT panel reads the same Markov table that composes the songs FORWARD: ranked candidates with weight pips and a one-word reason each - the research made audible: genres differ by WEIGHTS over a vocab, not by grammar. Deterministic; carries a spec() (round-trip: every function's spelling re-analyzes to itself in all 12 keys, in major, minor AND blues; doo-wop = I vi IV V; ii-V-I; minor i-VI).",
    "controls": "Tap a palette chord to add it (it plays) - or type it: Q W E R T Y = the top row, A S D F G H J = the second row. Tap a NEXT suggestion (or keys 1-4) to follow the brain. Tap a chord already IN the strip to EDIT it live: it opens ^/v steppers that walk that chord to the next/previous chord in the key (re-analyzes, and the loop plays the change on its next pass - so you can fix a bar without stopping); tap its name to close. SPACE play/stop the loop, U or BACKSPACE undo, X clear. LEFT/RIGHT (or the < > buttons) change the key: while STOPPED this RE-ANALYZES the same chords in the new key (they re-spell, and may turn ? if they no longer fit); while PLAYING it TRANSPOSES the whole progression to the new key (the chords move, bass + drums follow, the numerals stay put) - so you can lift or drop the loop a key live. B cycle mode (BOSSA/LOUNGE/POP/FOLK/MINOR/CINE/DORIAN/MIXO/PHRYG/LYDIAN/BLUES). 7 (or the 7THS/TRIAD button) toggles between seventh chords (Am7 Fmaj7) and plain triads (Am F) - triads also VOICE without the 7th, so the tool reads AND sounds basic. 8 (or the STRUM button) cycles how the strings are raked: BLOCK (all at once), DOWN (low->high), UP (high->low) and ROLL (a slow arpeggiated rake). 9 (or the INV button) cycles the voicing: ROOT position, 1ST, 2ND (and 3RD for sevenths) inversion - the soft bass follows, so the lowest note actually changes. 0 (or the OCT button) shifts the register LOW / MID / HIGH - the whole chord an octave down or up, MID being the original. The BASS button cycles the accompaniment played UNDER the SPACE loop (silent while tapping): HOLD (the held root), R-5 (root + fifth pump), WALK (a quarter-note jazz walking bass that leads into the next chord's root) and BAND (a genre-aware bassist that plays the DECLARED mode's own idiom - bossa/folk/rock/phrygian/blues-boogie... each different). The DRUMS button adds the rhythm section (also loop-only, on the shared drum kit): OFF, PLAIN (a genre-agnostic backbeat) or BAND (the mode's own groove - four-on-floor for EDM, boom-bap for lofi, palmas for flamenco, a sparse trailer kick for cinematic...). The FILL toggle drops a flourish into the last bar of the loop + a turnaround crash - and the fill is GENRE-AWARE too (like the groove): a rock tom roll, an EDM snare build, a cinematic tom swell, flamenco palmas, a jazz brush flurry... The MEL toggle adds a lead MELODY over the loop - a genre-rhythmed line that arpeggiates the live chord's tones (+ the 9th) in a high register, voice-led so it moves in small steps and re-pitches to each chord (it 'blooms from the chord'). The BPM slider (bottom-right) sets the SPACE-loop tempo, 60-180 BPM. FEEL is baked per genre (no knob): the chord re-strums on a per-mode COMP rhythm (so it grooves instead of hitting once a bar) and the off-beats SWING by a per-mode amount (jazz/blues/lofi lag, pop/EDM/rock stay tight). The ? button (top) opens the legend: what the roman numerals and NEXT reason-words mean."
  },
  "todo": [
    "Consider extracting a shared bass.h once a 2nd/3rd consumer appears (the second-customer rule, like harmony.h/improv.h): a genre-idiom SKELETON (which note on which beat, per genre) that both BAND and the radio stations draw from, each adding its own voice/swing/variation. NOT worth it yet - radio bass is bespoke + already richer (swing, chance-variation, dynamics) than BAND.",
    "BAND could gain LIFE like the radios if wanted: chance()-based note variation + swing/behind-the-beat feel. Deliberately omitted for now - chordwise is a legible, deterministic teaching toy (carries a spec), and predictability is the right call here."
  ]
}
de:meta */
// chordwise — the progression analyzer + next-chord suggester (demand-82).
//
// The first cart on runtime/harmony.h, exercising all three directions across
// five MODES (B cycles them) — each swaps the whole vocab, so one toy covers
// MAJOR (bossa/lounge/pop), MINOR (sad-pop/EDM/rock/lofi) and BLUES:
//   ANALYZE  — the strip re-derives roman numerals from raw (root, quality)
//              every frame via hb_vocab_analyze against the current mode's vocab;
//              the key is an INPUT (declared, not guessed). Minor/blues analysis
//              is no harder than major BECAUSE the key is declared — the unsolved
//              part is guessing major-vs-minor, which the toy never does.
//   SUGGEST  — hb_suggest reads the generation table forward: ranked next
//              chords + a one-word reason (cadence / home / tritone sub...).
//   GENERATE — untouched here; bossa/cocktail keep composing on the same table.
//
// Honesty rules the toy inherits from the research (design/harmony-brain.md):
// a chord outside the current mode's vocab shows "?" (never a guess), and the
// suggestion list is short (~2-4) because real harmony is that concentrated
// (93% of next-chords live in the top two options).

#include "studio.h"
#include "harmony.h"
#include "ui.h"
#include "radio.h"    // rad_bass_to — nearest-octave voice leading for the bassline
#include "drumkit.h"  // the shared playable kit — the rhythm section's voices
#include <stdio.h>
#include <string.h>

#define I_PLK  5   // the pluck that voices your taps
#define I_BSS  6   // a soft root under it
#define I_LEAD 7   // the melody voice (a soft lead over the chords)

// STRUM — how the chord's strings are raked. BLOCK fires every voice on one
// frame (a piano-style block); the others stagger the onsets a few frames apart
// so it reads as a strummed guitar. gap = frames between strings; up = 1 rakes
// high->low (an upstroke), else low->high (a downstroke).
typedef struct { const char *name; int gap, up; } Strum;
static const Strum STRUMS[] = {
    { "BLOCK", 0, 0 },   // all at once — the original block chord
    { "DOWN",  2, 0 },   // low string first — a downstroke
    { "UP",    2, 1 },   // high string first — an upstroke
    { "ROLL",  6, 0 },   // slow low->high rake, near-arpeggiated
};
#define NSTRUM ((int)(sizeof STRUMS / sizeof STRUMS[0]))
static int strumSel = 0;

// INVERSION — which chord tone sits in the bass. ROOT = root position; 1ST/2ND
// raise the lowest 1/2 tones an octave (3RD needs a 7th). The soft root follows,
// so the LOWEST note actually changes — you hear the inversion, not just a spread.
static const char *INV_LAB[4] = { "ROOT", "1ST", "2ND", "3RD" };
static int invSel = 0;

// OCTAVE — the whole voicing shifted a register, the way a pianist drops a chord
// an octave for a darker/fuller sound or lifts it for a brighter one. MID = the
// original register (0 shift), so it stays byte-identical unless you touch it.
static const char *OCT_LAB[3] = { "LOW", "MID", "HIGH" };
static int octSel = 1;

// a tiny pending-note queue so sound_chord can stagger a strum across frames.
#define NPEND 12
static struct { int midi, instr, vol, at; } pend[NPEND];
static int npend = 0;
static void queue_note(int midi, int instr, int vol, int at) {
    if (at <= 0) { note(midi, instr, vol); return; }   // fire now
    if (npend >= NPEND) return;                         // full — drop (won't happen)
    pend[npend].midi = midi; pend[npend].instr = instr;
    pend[npend].vol = vol;   pend[npend].at = at;       npend++;
}
static void pump_notes(void) {                          // call once per frame
    for (int i = 0; i < npend; ) {
        if (--pend[i].at <= 0) { note(pend[i].midi, pend[i].instr, pend[i].vol);
                                 pend[i] = pend[--npend]; }
        else i++;
    }
}

static const char *NOTE[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
// chord tones for the tap SOUND come from harmony.h's shared table (hb_tones);
// naming/choosing lives there too (spelling stays the vocab's sevenths).

static int keyPc = 0;

// a MODE = a style (which carries its VOCAB — major/minor/blues) + how to lay its
// palette out + whether the key reads as minor. Cycling MODE swaps the whole tonal
// system: the palette respells, the strip re-analyses, NEXT re-ranks. The three
// major modes share one vocab (byte-exact old behaviour); MINOR and BLUES ride the
// added vocabs — analysis works because the key is DECLARED, never guessed.
// minor palette DISPLAY order: the everyday pop/rock/lofi chords up top
// (i III iv v VI VII), the diminished + harmonic-minor color on the shelf
// (ii° V vii°). Only the palette layout — the vocab + analysis are untouched.
static const int MINOR_ORDER[HBm_NFUNC] = {
    HBm_i, HBm_III, HBm_iv, HBm_v, HBm_VI, HBm_VII,   // row 1: the minor loop
    HBm_iio, HBm_V, HBm_viio                          // row 2: the color
};
// dorian palette order: the i<->IV vamp + cadence up top, the color on the shelf
static const int DORIAN_ORDER[HBd_NFUNC] = {
    HBd_i, HBd_IV, HBd_VII, HBd_v,                    // row 1: the vamp
    HBd_ii, HBd_III, HBd_vio                          // row 2: the color
};
// the other three modes: the characteristic chord second in row 1, color below
static const int MIXO_ORDER[HBmx_NFUNC] = {
    HBmx_I, HBmx_bVII, HBmx_IV, HBmx_v, HBmx_vi,      // row 1: I-bVII-IV rock vamp
    HBmx_ii, HBmx_iiio                                // row 2: the color
};
static const int PHRYG_ORDER[HBph_NFUNC] = {
    HBph_i, HBph_bII, HBph_III, HBph_iv, HBph_VI,     // row 1: i-bII flamenco vamp
    HBph_vo, HBph_vii                                 // row 2: the color
};
static const int LYDIAN_ORDER[HBly_NFUNC] = {
    HBly_I, HBly_II, HBly_V, HBly_vi, HBly_iii,       // row 1: I-II floating vamp
    HBly_ivo, HBly_vii                                // row 2: the color
};

typedef struct {
    const char    *name;      // the mode button label
    const HbStyle *style;     // weights + vocab (NULL vocab = the major default)
    int            row1;      // palette functions on the top row (rest go to row 2)
    const char    *lab1, *lab2;
    const int     *order;     // palette display order (NULL = the vocab's own order)
    int            minorKey;  // 1 → the KEY readout shows "<note>m"
} Mode;
static const Mode MODES[] = {
    { "BOSSA",  &HB_BOSSA,       6, "DIATONIC", "BORROWED", NULL,         0 },
    { "LOUNGE", &HB_COCKTAIL,    6, "DIATONIC", "BORROWED", NULL,         0 },
    { "POP",    &HB_POP,         6, "DIATONIC", "BORROWED", NULL,         0 },
    { "FOLK",   &HB_FOLK,        6, "DIATONIC", "BORROWED", NULL,         0 },
    { "MINOR",  &HB_MINPOP,      6, "MINOR",    "HARMONIC", MINOR_ORDER,  1 },
    { "CINE",   &HB_CINEMATIC,   6, "MINOR",    "HARMONIC", MINOR_ORDER,  1 },
    { "DORIAN", &HB_DORIAN_STYLE,4, "DORIAN",   "COLOR",    DORIAN_ORDER, 0 },
    { "MIXO",   &HB_MIXO_STYLE,  5, "MIXO",     "COLOR",    MIXO_ORDER,   0 },
    { "PHRYG",  &HB_PHRYG_STYLE, 5, "PHRYG",    "COLOR",    PHRYG_ORDER,  0 },
    { "LYDIAN", &HB_LYDIAN_STYLE,5, "LYDIAN",   "COLOR",    LYDIAN_ORDER, 0 },
    { "BLUES",  &HB_BLUES,       3, "12-BAR",   "",         NULL,         0 },
};
#define NMODE ((int)(sizeof MODES / sizeof MODES[0]))
static int modeSel = 0;

// the tonal system of the current mode (NULL style-vocab = the frozen major one)
static const HbVocab *cur_vocab(void) {
    const HbVocab *v = MODES[modeSel].style->vocab;
    return v ? v : &HB_MAJOR;
}
// the function at palette position i (mode's display order, or vocab order)
static int fn_at(int i) {
    const int *o = MODES[modeSel].order;
    return o ? o[i] : i;
}

#define MAXP 8
static int prootv[MAXP], pqual[MAXP], plen = 0;   // the strip: raw chords you entered
static int pfn[MAXP];                             // their functions, re-analyzed (-1 = ?)

static HbOpt sugg[4];                             // the NEXT panel
static int   nsugg = 0;

static int playing = 0, playSlot = 0, playT = 0;
static int helpOn = 0;                            // the legend overlay (? button)
// tempo: the loop holds each chord for a half-note. BPM is a 0..1 slider mapped
// to BPM_MIN..BPM_MAX; bar_frames() is that in 60fps frames (default ~109 BPM =
// the old 66-frame, ~1.1s-per-chord feel).
#define BPM_MIN 60
#define BPM_MAX 180
static float bpmN = 0.408f;                       // (109-60)/(180-60)
static int cur_bpm(void)    { return BPM_MIN + (int)(bpmN * (BPM_MAX - BPM_MIN) + 0.5f); }
static int bar_frames(void) { return (int)(7200.0f / cur_bpm() + 0.5f); }  // half-note @ 60fps

// 1 = seventh names + voicings (Cmaj7, the jazz reading); 0 = plain triads
// (C Am G) that also VOICE as triads, so a beginner's tool reads AND sounds basic.
static int seventh = 1;

static void chname(char *out, int n, int rootPc, int q) {
    snprintf(out, n, "%s%s", NOTE[rootPc], seventh ? hb_qname[q] : hb_qtriad[q]);
}

static void sound_chord(int rootPc, int q, int withBass, int baseDelay) {
    int r = 48 + rootPc + (octSel - 1) * 12;    // OCT: shift the whole voicing a register
    int voices = seventh ? 4 : 3;               // triad mode drops the 7th/6th
    int inv = invSel; if (inv > voices - 1) inv = voices - 1;   // 3RD needs a 7th

    // chord-tone offsets; an inversion raises the lowest `inv` of them an octave.
    int off[4];
    for (int i = 0; i < voices; i++) off[i] = hb_tones[q][i];
    for (int k = 0; k < inv; k++) off[k] += 12;
    int lo = off[0]; for (int i = 1; i < voices; i++) if (off[i] < lo) lo = off[i];

    // the soft root follows the inversion's bass, an octave under it. A moving
    // bassline (BASS != HOLD) drives its own root, so skip this one to avoid doubling.
    int m[5], inst[5], nv = 0;
    if (withBass) { m[nv] = r + lo - 12; inst[nv] = I_BSS; nv++; }
    for (int i = 0; i < voices; i++) { m[nv] = r + off[i]; inst[nv] = I_PLK; nv++; }

    // rake by actual PITCH so a strum stays low->high even after inverting.
    int idx[5]; for (int i = 0; i < nv; i++) idx[i] = i;
    for (int a = 0; a < nv; a++) for (int b = a + 1; b < nv; b++)
        if (m[idx[b]] < m[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    int gap = STRUMS[strumSel].gap, up = STRUMS[strumSel].up;
    for (int p = 0; p < nv; p++) {
        int i = idx[up ? (nv - 1 - p) : p];     // which string is struck when
        queue_note(m[i], inst[i], 5, baseDelay + p * gap);   // baseDelay = the swing push
    }
}

// BASS — a generated accompaniment under the SPACE loop (silent while tapping).
// HOLD = the held root sound_chord already plays. R-5 = root/fifth pump. WALK =
// a jazz quarter-note walk (root, 3rd, 5th, chromatic approach into the next
// root). BAND = a genre-aware bassist: it reads the DECLARED mode and plays that
// genre's idiom — the same "genres differ by character over one vocab" thesis the
// chords already make audible, now in the bass. Reuses improv.h's "chord tone on
// the strong beat" rule; the mode is an input, so this stays honest, never guessed.
// Every style is VOICE-LED through radio.h's rad_bass_to: each note steps to the
// NEAREST instance of its pitch class and folds into a register window, so the
// line walks smoothly instead of leaping (octave bounces are layered on top).
enum { B_HOLD, B_ROOT5, B_WALK, B_BAND };
static const char *BASS_LAB[4] = { "HOLD", "R-5", "WALK", "BAND" };
static int bassSel = 0;

// beat tokens the BAND patterns are written in — resolved against the live chord
// (quality-correct 3rd/5th) and the next root, so one table serves every key.
enum { BT_ROOT, BT_OCTU, BT_FIFTH, BT_FIFTHD, BT_THIRD, BT_SIXTH, BT_FLAT2, BT_FLAT7, BT_APPR, BT_REST };

// the genre map: one 4-beat bass idiom per MODE (index matches MODES[] above).
static const int BASS_PAT[NMODE][4] = {
    /* BOSSA  */ { BT_ROOT,  BT_REST,  BT_FIFTHD, BT_REST  },  // sparse latin root / fifth-below
    /* LOUNGE */ { BT_ROOT,  BT_THIRD, BT_FIFTH,  BT_APPR  },  // the jazz quarter walk (its true home)
    /* POP    */ { BT_ROOT,  BT_REST,  BT_OCTU,   BT_REST  },  // root / root-octave pulse
    /* FOLK   */ { BT_ROOT,  BT_REST,  BT_FIFTH,  BT_REST  },  // boom-chuck: root then fifth
    /* MINOR  */ { BT_ROOT,  BT_OCTU,  BT_ROOT,   BT_OCTU  },  // driving root/octave eighths
    /* CINE   */ { BT_ROOT,  BT_REST,  BT_REST,   BT_REST  },  // one sustained pedal root a bar
    /* DORIAN */ { BT_ROOT,  BT_SIXTH, BT_FIFTH,  BT_SIXTH },  // the bright natural-6 dorian color
    /* MIXO   */ { BT_ROOT,  BT_FIFTH, BT_FLAT7,  BT_FIFTH },  // rock: root, fifth, the flat-7
    /* PHRYG  */ { BT_ROOT,  BT_FLAT2, BT_ROOT,   BT_FLAT2 },  // the flamenco b2 half-step
    /* LYDIAN */ { BT_ROOT,  BT_REST,  BT_FIFTH,  BT_REST  },  // sparse floating root/fifth
    /* BLUES  */ { BT_ROOT,  BT_THIRD, BT_FIFTH,  BT_SIXTH },  // the boogie shuffle 1-3-5-6
};

static int bassLast = 36;   // last bass note played — the voice-leading anchor

static void play_bass_beat(int beat, int delay) {   // beat 0..3 within the current chord's bar
    if (!plen) return;
    int r0 = prootv[playSlot], q = pqual[playSlot];
    int rN = prootv[(playSlot + 1) % plen];      // the chord we're walking toward
    int base = 36 + (octSel - 1) * 12;           // bass register, follows OCT
    int lo = base - 4, hi = base + 12;           // the register window the line lives in
    if (bassSel == B_HOLD) {                     // just a soft root, held, on the downbeat
        if (beat == 0) queue_note(base + r0, I_BSS, 5, delay);
        return;
    }

    // pick a target pitch CLASS for this beat; oct = a deliberate octave bounce
    // (POP/MINOR root-octave, BOSSA fifth-below) layered ON TOP of the voice leading.
    int pc = -1, oct = 0;
    if (bassSel == B_ROOT5) {
        if (beat == 0)      pc = r0;
        else if (beat == 2) pc = (r0 + 7) % 12;      // the fifth, on beat 3
        else return;                                 // beats 2 & 4 rest
    } else if (bassSel == B_WALK) {
        if      (beat == 0) pc = r0;                          // root, the strong beat
        else if (beat == 1) pc = (r0 + hb_tones[q][1]) % 12;  // the 3rd
        else if (beat == 2) pc = (r0 + hb_tones[q][2]) % 12;  // the 5th
        else                pc = (rN + 11) % 12;              // approach the next root
    } else {                                         // B_BAND — the genre idiom
        switch (BASS_PAT[modeSel][beat]) {
            case BT_REST:                            return;
            case BT_ROOT:   pc = r0;                          break;
            case BT_OCTU:   pc = r0;                 oct = +1; break;  // root, an octave up
            case BT_FIFTH:  pc = (r0 + hb_tones[q][2]) % 12;  break;
            case BT_FIFTHD: pc = (r0 + hb_tones[q][2]) % 12; oct = -1; break;  // fifth, an octave down
            case BT_THIRD:  pc = (r0 + hb_tones[q][1]) % 12;  break;
            case BT_SIXTH:  pc = (r0 + 9) % 12;               break;
            case BT_FLAT2:  pc = (r0 + 1) % 12;               break;
            case BT_FLAT7:  pc = (r0 + 10) % 12;              break;
            case BT_APPR:   pc = (rN + 11) % 12;              break;
        }
    }
    if (pc < 0) return;
    int n = rad_bass_to(pc, bassLast, lo, hi);   // step to the NEAREST instance of pc
    bassLast = n;                                // anchor the line on the in-register note...
    queue_note(n + oct * 12, I_BSS, 5, delay);   // ...but sound the octave bounce, if any
}

// DRUMS — the rest of the rhythm section, on drumkit.h (the shared kit), the same
// methodical shape as BASS: OFF, a PLAIN backbeat (genre-agnostic), or BAND
// (the DECLARED mode's own groove). Each pattern is 8 sixteenth-steps spanning one
// chord's two beats, so it tiles into a normal 4-beat feel across two chords
// (kick on 1 & 3, backbeat on 2 & 4). Three lanes: kick, a backbeat voice
// (snare/clap/tom), and a hat/ride — written as 'x'=hit step-strings.
enum { D_OFF, D_PLAIN, D_BAND };
static const char *DRUM_LAB[3] = { "DRUMS", "PLAIN", "BAND" };   // OFF shows the control's name; PLAIN = a genre-agnostic backbeat
static int drumSel = 0;
static int fillOn  = 0;   // FILL: a tom roll on the last bar of the loop + a crash on the turnaround

typedef struct { const char *kick, *back, *hat; int backRole, hatRole; } DrumPat;
//                        step:  0123 4567  (16ths; beat1=0, beat2=4)
static const DrumPat DRUM_PAT[NMODE] = {
    /* BOSSA  */ { "x..x....", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // gentle surdo + rim + soft ride
    /* LOUNGE */ { "x.......", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // feathered kick, brush 2&4, ride
    /* POP    */ { "x.......", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // the classic backbeat
    /* FOLK   */ { "x.......", "....x...", "x...x...", DK_SNARE,  DK_HHC },  // light: kick 1, brush 2, quarter hats
    /* MINOR  */ { "x...x...", "....x...", ".x.x.x.x", DK_CLAP,   DK_HHO },  // four-on-floor + clap + offbeat opens
    /* CINE   */ { "x...x...", "....x...", "........", DK_TOM_LO, DK_HHC },  // big sparse kick + tom, no hats
    /* DORIAN */ { "x.....x.", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // laid-back boom-bap
    /* MIXO   */ { "x.x.....", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // driving rock kick push
    /* PHRYG  */ { "x.......", "x.xx.x..", "........", DK_CLAP,   DK_HHC },  // flamenco palmas (handclaps)
    /* LYDIAN */ { "x.......", "........", "....x...", DK_SNARE,  DK_HHC },  // barely-there floating pulse
    /* BLUES  */ { "x.......", "....x...", "x..x..x.", DK_SNARE,  DK_HHC },  // the shuffle ride feel
};
static const DrumPat DRUM_PLAIN = { "x.......", "....x...", "x.x.x.x.", DK_SNARE, DK_HHC };

// FILL is genre-aware too — the flourish on the loop's LAST bar reads the mode,
// like the groove does. 8 step-slots over the last bar (a '.' step lets the normal
// groove through; a letter takes it over): s=snare h=tomHi l=tomLo c=clap k=kick
// o=openhat. `crash` lands a cymbal on the turnaround. A jazz brush flurry, an EDM
// snare BUILD, a cinematic tom swell and flamenco palmas are all different fills.
static int fill_role(char c) {
    switch (c) {
        case 's': return DK_SNARE;  case 'h': return DK_TOM_HI; case 'l': return DK_TOM_LO;
        case 'c': return DK_CLAP;   case 'k': return DK_KICK;   case 'o': return DK_HHO;
        default:  return -1;   // '.' = no fill hit (normal groove plays this step)
    }
}
typedef struct { const char *pat; int crash; } FillPat;
static const FillPat FILL_PAT[NMODE] = {
    /* BOSSA  */ { "....s.hl", 0 },  // gentle tom tag, no crash (stays subtle)
    /* LOUNGE */ { "....ssss", 1 },  // a brushy snare flurry
    /* POP    */ { "....sshl", 1 },  // the classic last-beat tom roll
    /* FOLK   */ { "......s.", 0 },  // just a snare pickup
    /* MINOR  */ { "ssssssss", 1 },  // the EDM snare BUILD (a whole-bar riser)
    /* CINE   */ { "hhllhhll", 1 },  // a big descending tom swell
    /* DORIAN */ { "....s.hl", 0 },  // laid-back, no crash
    /* MIXO   */ { "..sshhll", 1 },  // the rock descending tom roll
    /* PHRYG  */ { "c.ccc.cc", 0 },  // a flamenco palmas flourish
    /* LYDIAN */ { "........", 1 },  // just a floating crash swell, no roll
    /* BLUES  */ { "...ssshl", 1 },  // a shuffly snare-into-tom fill
};
static const FillPat FILL_PLAIN = { "....sshl", 1 };   // the generic fill for the PLAIN groove

static void play_drum_step(int step, int delayMs) {   // step 0..7, sixteenths within the chord's two beats
    if (drumSel == D_OFF || !plen) return;
    const DrumPat *d = (drumSel == D_BAND) ? &DRUM_PAT[modeSel] : &DRUM_PLAIN;

    // FILL: the genre's own flourish on the loop's LAST bar, and its turnaround crash.
    if (fillOn && plen > 1) {
        const FillPat *fp = (drumSel == D_BAND) ? &FILL_PAT[modeSel] : &FILL_PLAIN;
        if (playSlot == 0 && step == 0 && fp->crash) dk_fire_at(delayMs, DK_CRASH, 0, 4);   // base 4 — the crash is loud enough (bandbox's trim, mirrored)
        if (playSlot == plen - 1) {
            int r = fill_role(fp->pat[step]);
            if (r >= 0) { dk_fire_at(delayMs, r, 0, 4 + step / 2); return; }   // crescendo into the turnaround
        }
    }
    if (d->kick[step] == 'x') dk_fire_at(delayMs, DK_KICK,     0, 6);
    if (d->back[step] == 'x') dk_fire_at(delayMs, d->backRole, 0, 5);
    // an OPEN hat (MINOR's offbeat opens) carries far more noise-energy than a
    // closed one — one velocity notch softer keeps it from washing out the mix.
    if (d->hat[step]  == 'x') dk_fire_at(delayMs, d->hatRole,  0, d->hatRole == DK_HHO ? 2 : 3);
}

// MELODY — a lead that BLOOMS from the chords: on a genre rhythm it arpeggiates the
// live chord's tones + the 9th, VOICE-LED through rad_bass_to (in a high register)
// so it walks smoothly and re-pitches to each chord — the "melody re-resolves to the
// chord" idea from bossa.c's pick_mel. On/off toggle; genre-aware when on (MEL_PAT).
// 8-step rhythm per mode ('x' = a note); kept sparse so it sings, not chatters.
static const char *MEL_PAT[NMODE] = {
    /* BOSSA  */ "..x..x.x",   // bossa syncopation
    /* LOUNGE */ "x...x.x.",   // laid-back jazz phrase
    /* POP    */ "x..x.x..",   // a pop hook rhythm
    /* FOLK   */ "x...x...",   // plain, on the beats
    /* MINOR  */ "x.x..x.x",   // driving
    /* CINE   */ "x.......",   // one long held note a bar
    /* DORIAN */ "..x...x.",   // laid-back offbeats
    /* MIXO   */ "x..x.x..",   // a rock riff
    /* PHRYG  */ "x.x.x...",   // spanish descending feel
    /* LYDIAN */ "x.....x.",   // sparse, floating
    /* BLUES  */ "x..x..x.",   // bluesy phrasing
};
static int melOn   = 0;
static int melLast = 72;   // the melodic walker's last note, for voice leading
static int melI    = 0;    // which chord tone to reach for next (arpeggiates)

static void play_mel_step(int step, int delay) {   // step 0..7, on the drum grid
    if (!melOn || !plen) return;
    if (MEL_PAT[modeSel][step] != 'x') return;
    int r0 = prootv[playSlot], q = pqual[playSlot];
    int base = 72 + (octSel - 1) * 12;           // a high, singable register (follows OCT)
    int lo = base - 5, hi = base + 7;
    // candidates: the chord tones + the 9th (the colour note)
    int cand[5], nc = 0, voices = seventh ? 4 : 3;
    for (int i = 0; i < voices; i++) cand[nc++] = (r0 + hb_tones[q][i]) % 12;
    cand[nc++] = (r0 + 2) % 12;                  // the 9th
    int pc = cand[melI % nc]; melI++;            // walk through them, chord to chord
    melLast = rad_bass_to(pc, melLast, lo, hi);  // nearest instance = a smooth line
    queue_note(melLast, I_LEAD, 4, delay);
}

// FEEL — the two things that lift the loop off the dead grid, baked per genre (no knob):
//  COMP_PAT: WHEN the chord re-strums within its bar (8 sixteenth-steps, 'x' = a strum).
//            So the harmony has a pulse — a bossa clave, a jazz 4-to-the-bar, a rock chunk —
//            instead of one dead hit on beat 1. Sparse genres (cine/lydian) keep one held hit.
//  SWING:    how far the OFF-BEATS lag (0 = dead straight, 1 = full triplet). Jazz/blues/lofi
//            swing; pop/EDM/rock stay tight because straight IS their feel.
static const char *COMP_PAT[NMODE] = {
    /* BOSSA  */ "x..x.x..",   // syncopated bossa comp
    /* LOUNGE */ "x...x...",   // 4-to-the-bar jazz comp (swings)
    /* POP    */ "x...x...",   // steady on the beats
    /* FOLK   */ "x..x.x..",   // a gentle strum pattern
    /* MINOR  */ "x..x..x.",   // syncopated stabs
    /* CINE   */ "x.......",   // one big held chord a bar
    /* DORIAN */ "..x..x..",   // off-beat lofi comp (swings)
    /* MIXO   */ "x..x.x..",   // a rock chunk rhythm
    /* PHRYG  */ "x.xx.x..",   // flamenco rasgueado feel
    /* LYDIAN */ "x.......",   // let it float, one hit
    /* BLUES  */ "x...x...",   // quarters (heavy shuffle swing does the rest)
};
static const float SWING[NMODE] = {
    /* BOSSA  */ 0.12f, /* LOUNGE */ 0.60f, /* POP    */ 0.00f, /* FOLK   */ 0.10f,
    /* MINOR  */ 0.00f, /* CINE   */ 0.00f, /* DORIAN */ 0.38f, /* MIXO   */ 0.08f,
    /* PHRYG  */ 0.00f, /* LYDIAN */ 0.00f, /* BLUES  */ 0.66f,
};

static void analyze(void) { hb_vocab_analyze(cur_vocab(), keyPc, prootv, pqual, plen, pfn); }

static void resuggest(void) {
    nsugg = 0;
    if (!plen) return;
    int last = pfn[plen - 1];
    if (last < 0 || last >= MODES[modeSel].style->nfunc) return;   // ? chord → no advice (honest)
    nsugg = hb_suggest(MODES[modeSel].style, last, sugg, 4);
}

static void rethink(void) { analyze(); resuggest(); }

// the KEY control does two jobs. STOPPED: just move the key and re-analyze — the
// SAME chords re-spell under the new key (the cart's honesty demo). PLAYING: you're
// performing, so TRANSPOSE — drag every chord along with the key (bass + drums
// follow), so the progression lifts/drops a key live and the numerals stay put.
static void change_key(int delta) {
    keyPc = (keyPc + delta + 12) % 12;
    if (playing)
        for (int i = 0; i < plen; i++) prootv[i] = (prootv[i] + delta + 12) % 12;
    rethink();
}

// LIVE EDIT — tap a strip cell to select it, then ▲/▼ step THAT chord to the
// next/previous chord in the current mode's palette (stays in-key, re-analyzes,
// the loop plays the edit next pass). pfn[] already holds each cell's function.
static int selCell = -1;   // the strip cell being edited, -1 = none
static void nudge_cell(int cell, int dir) {
    if (cell < 0 || cell >= plen) return;
    const HbVocab *v = cur_vocab();
    int f = pfn[cell];                        // current function (-1 = out of vocab)
    if (f < 0) f = (dir > 0) ? -1 : v->n;     // a ? chord snaps onto the palette
    f = (f + dir + v->n) % v->n;
    prootv[cell] = (keyPc + v->off[f]) % 12;
    pqual[cell]  = v->qual[f];
    rethink();
    sound_chord(prootv[cell], pqual[cell], 1, 0);   // audition the edit
}

static void add_chord(int rootPc, int q) {
    if (plen >= MAXP) return;
    prootv[plen] = rootPc; pqual[plen] = q; plen++;
    rethink();
    sound_chord(rootPc, q, 1, 0);   // tap audition: full chord + its root, no delay
}
static void add_fn(int f) {
    const HbVocab *v = cur_vocab();
    add_chord((keyPc + v->off[f]) % 12, v->qual[f]);
}

// cold-open with the doo-wop (I vi IV V) so a stranger sees the whole idea —
// strip, numerals, NEXT — before touching anything. Silent: no note until a tap.
static void seed_demo(void) {
    static const int DW[4] = { HB_I, HB_vi, HB_IV, HB_V };
    for (int i = 0; i < 4; i++) {
        prootv[plen] = (keyPc + hb_off[DW[i]]) % 12;
        pqual[plen]  = hb_qual[DW[i]];
        plen++;
    }
    rethink();
}

void init(void) {
    instrument(I_PLK, INSTR_PLUCK, 2, 200, 2, 250);
    instrument(I_BSS, INSTR_TRI, 6, 160, 3, 220);
    instrument(I_LEAD, INSTR_SINE, 4, 180, 3, 200);   // a soft lead for the melody
    dk_use(&DK_ACOUSTIC, 20);   // the rhythm section — voice slots 20..27
    // hat chokes (drumkit.h's documented candidate default, wired cart-side): the
    // open hat is mono + choked by the closed hat AND the kick (the foot-close) —
    // stacked open-hat rings are the "washy drums" (MINOR's offbeat opens).
    instrument_choke(20 + DK_HHO,  20 + DK_HHO);
    instrument_choke(20 + DK_HHC,  20 + DK_HHO);
    instrument_choke(20 + DK_KICK, 20 + DK_HHO);
    instrument_level(20 + DK_CRASH, 0.6f);   // the crash slot is hot — duck ~-4.4 dB (bandbox's trim, mirrored)
    seed_demo();
}

void update(void) {
    if (keyp(KEY_LEFT))  change_key(-1);
    if (keyp(KEY_RIGHT)) change_key(+1);
    if (keyp('B'))       { modeSel = (modeSel + 1) % NMODE; rethink(); }  // vocab may change → re-analyse
    if (keyp('7'))       { seventh = !seventh; if (!seventh && invSel > 2) invSel = 2; }  // 7ths <-> triads
    if (keyp('8'))       { strumSel = (strumSel + 1) % NSTRUM; }           // block <-> strum styles
    if (keyp('9'))       { invSel = (invSel + 1) % (seventh ? 4 : 3); }    // root <-> inversions
    if (keyp('0'))       { octSel = (octSel + 1) % 3; }                    // low / mid / high register
    if (keyp('U') || keyp(KEY_BACKSPACE)) { if (plen) { plen--; rethink(); } }
    { const HbVocab *v = cur_vocab();
      int r1 = MODES[modeSel].row1; if (r1 > v->n) r1 = v->n;
      static const char R1K[6] = { 'Q','W','E','R','T','Y' };
      static const char R2K[7] = { 'A','S','D','F','G','H','J' };
      for (int i = 0; i < r1 && i < 6; i++)          if (keyp(R1K[i])) add_fn(fn_at(i));
      for (int i = 0; i + r1 < v->n && i < 7; i++)   if (keyp(R2K[i])) add_fn(fn_at(r1 + i)); }
    if (keyp('X'))       { plen = 0; playing = 0; rethink(); }
    if (keyp(KEY_SPACE) && plen) { playing = !playing; playSlot = 0; playT = 0; bassLast = 36 + (octSel - 1) * 12; melLast = 72 + (octSel - 1) * 12; melI = 0; }
    for (int i = 0; i < 4; i++)
        if (keyp('1' + i) && i < nsugg) add_fn(sugg[i].f);

    if (playing && plen) {
        int bf = bar_frames();
        int r0 = prootv[playSlot], q = pqual[playSlot];
        int swMax = (int)(SWING[modeSel] * bf / 12.0f);   // full-triplet push at SWING=1

        // the 16th grid (8 steps): the CHORD comp, the drums and the melody.
        // off-8th steps (2 & 6 = the "ands") lag by the swing push.
        int stepLen = bf / 8; if (stepLen < 1) stepLen = 1;
        if (playT % stepLen == 0) {
            int s = playT / stepLen;
            if (s < 8) {
                int sw = (s == 2 || s == 6) ? swMax : 0;   // frames
                if (COMP_PAT[modeSel][s] == 'x') sound_chord(r0, q, 0, sw);  // chord re-strum (no bass)
                play_drum_step(s, sw * 1000 / 60);         // drums take ms
                play_mel_step(s, sw);
            }
        }
        // the 8th grid (4 beats): the bassline. Its off-8ths (beats 1 & 3) swing too.
        int beatLen = bf / 4; if (beatLen < 1) beatLen = 1;
        if (playT % beatLen == 0) {
            int beat = playT / beatLen;
            if (beat < 4) play_bass_beat(beat, (beat == 1 || beat == 3) ? swMax : 0);
        }
        if (++playT >= bf) { playT = 0; playSlot = (playSlot + 1) % plen; }
    } else playSlot = 0;

    pump_notes();   // fire any staggered strum voices whose delay has elapsed
}

void draw(void) {
    char buf[16];
    cls(CLR_DARKER_BLUE);
    ui_begin();

    // ── help overlay: what the roman numerals + NEXT reason-words mean ──
    if (helpOn) {
        print("WHAT THE NAMES MEAN", 8, 5, CLR_WHITE);
        if (ui_button(SCREEN_W - 46, 3, 42, 14, "CLOSE")) helpOn = 0;
        font(FONT_SMALL);
        int y = 24;
        print("ROMAN NUMERALS - each chord's JOB in the key", 8, y, CLR_YELLOW); y += 10;
        print("UPPER = major (I IV V)   lower = minor (ii vi)", 12, y, CLR_LIGHT_GREY); y += 8;
        print("dot=diminished   7=seventh   b=borrowed / flat", 12, y, CLR_LIGHT_GREY); y += 8;
        print("I is HOME.  V pulls to I.   ? = outside the key", 12, y, CLR_LIGHT_GREY); y += 12;

        print("WHY 'NEXT' SUGGESTS EACH - the word beneath it", 8, y, CLR_YELLOW); y += 10;
        static const char *L[6] = {
            "home     V->I, arrived",
            "cadence  ii-V-I move",
            "walk     a plain step",
            "resolve  tension eases",
            "borrow   parallel key",
            "plagal   IV->I, 'amen'",
        };
        static const char *R[6] = {
            "tritone  bII replaces V",
            "turnrnd  loops to top",
            "relative maj/min, same notes",
            "leadtone vii pulls to i",
            "pre-cad  sets up cadence",
            "subtonic bVII, flat-7 step",
        };
        for (int i = 0; i < 6; i++) {
            print(L[i], 8,   y + i * 9, CLR_LIGHT_GREY);
            print(R[i], 150, y + i * 9, CLR_LIGHT_GREY);
        }
        y += 6 * 9 + 4;
        print("THE BAND (loop): BASS=HOLD/R-5/WALK/BAND  DRUMS=OFF/PLAIN/BAND",
              8, y, CLR_LIGHT_GREY); y += 9;
        print("BAND = the mode's genre; MEL = a lead blooming from your chords",
              8, y, CLR_LIGHT_GREY); y += 9;
        print("FILL = the mode's own last-bar flourish + a turnaround crash",
              8, y, CLR_LIGHT_GREY); y += 9;
        print("FEEL (auto): the chord COMPS + the off-beats SWING, per genre",
              8, y, CLR_LIGHT_GREY); y += 9;
        print("CONTROLS  B=mode  7=spell  8=strum  9=inv  0=octave",
              8, y, CLR_INDIGO); y += 8;
        print("tap a strip chord to EDIT it live (^/v step it in-key, even while playing)",
              8, y, CLR_INDIGO); y += 8;
        print("Q-Y/A-J add chords  SPACE=loop  U=undo  X=clear",
              8, y, CLR_INDIGO); y += 8;
        print("arrows/< > = key: STOPPED re-analyzes, PLAYING transposes the loop",
              8, y, CLR_INDIGO);
        font(FONT_NORMAL);
        ui_end();
        return;
    }

    // ── header: title + key + mode ──
    print("CHORDWISE", 8, 5, CLR_WHITE);
    font(FONT_SMALL);
    print("next-chord", 88, 7, CLR_INDIGO);
    font(FONT_NORMAL);
    if (ui_button(168, 2, 14, 14, "<")) change_key(-1);
    rectfill(184, 2, 54, 14, CLR_BROWNISH_BLACK);
    snprintf(buf, sizeof buf, "KEY %s%s", NOTE[keyPc], MODES[modeSel].minorKey ? "m" : "");
    font(FONT_SMALL);
    print(buf, 188, 6, CLR_YELLOW);
    font(FONT_NORMAL);
    if (ui_button(240, 2, 14, 14, ">")) change_key(+1);
    if (ui_button(260, 2, 54, 14, MODES[modeSel].name)) { modeSel = (modeSel + 1) % NMODE; rethink(); }
    if (ui_button(146, 2, 16, 14, "?")) helpOn = !helpOn;   // the legend overlay

    // ── the strip: your progression — tap a chord to edit it live (▲/▼ step in-key) ──
    if (selCell >= plen) selCell = -1;   // stay valid if the strip shrank
    for (int i = 0; i < MAXP; i++) {
        int x = 8 + i * 39, y = 22, w = 36, h = 40;
        int on = i < plen, cur = playing && i == playSlot;
        if (!on) {   // empty slot — a plain placeholder, nothing to edit
            rectfill(x, y, w, h, CLR_DARKER_PURPLE);
            rect(x, y, w, h, CLR_DARKER_GREY);
            continue;
        }
        chname(buf, sizeof buf, prootv[i], pqual[i]);
        if (i == selCell) {   // the cell being edited: name/deselect row + ▲/▼ steppers
            rectfill(x, y, w, h, CLR_BROWNISH_BLACK);
            font(FONT_SMALL);
            if (ui_button(x, y, w, 11, buf)) selCell = -1;        // tap the name to close
            font(FONT_NORMAL);
            if (ui_button(x, y + 12, w, 13, "^")) nudge_cell(i, +1);   // next chord in key
            if (ui_button(x, y + 26, w, 14, "v")) nudge_cell(i, -1);   // previous chord in key
            rect(x, y, w, h, CLR_YELLOW);                          // "editing" border
            continue;
        }
        if (ui_button(x, y, w, h, "")) selCell = i;   // tap to select this chord
        if (cur) rectfill(x + 2, y + 2, w - 4, h - 4, CLR_DARK_BLUE);   // playing tint
        font(FONT_SMALL);
        print(buf, x + 3, y + 4, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
        const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
        int len = (int)strlen(rn), small = len > 4;
        if (small) font(FONT_SMALL);
        int rw = len * (small ? 4 : 8);
        print(rn, x + (w - rw) / 2, y + 18, pfn[i] >= 0 ? CLR_YELLOW : CLR_RED);
        if (small) font(FONT_NORMAL);
    }
    font(FONT_SMALL);
    print(plen ? "your changes, named in the key" : "tap chords below to start",
          8, 65, CLR_INDIGO);
    font(FONT_NORMAL);

    // ── NEXT: the table read forward — ranked, explained ──
    print("NEXT", 8, 76, CLR_WHITE);
    if (!plen) { font(FONT_SMALL); print("(waiting for a chord)", 48, 79, CLR_DARK_GREY); font(FONT_NORMAL); }
    else if (!nsugg) { font(FONT_SMALL); print("? chord - the brain has no map here", 48, 79, CLR_RED); font(FONT_NORMAL); }
    for (int i = 0; i < nsugg; i++) {
        int x = 40 + i * 70, y = 74, w = 66;
        int f = sugg[i].f;
        const HbVocab *v = cur_vocab();
        chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
        if (ui_button(x, y, w, 18, v->fname[f])) add_fn(f);
        for (int p = 0; p < sugg[i].w && p < 6; p++)
            rectfill(x + 2 + p * 5, y + 20, 3, 3, CLR_GREEN);
        font(FONT_SMALL);
        char why[24]; snprintf(why, sizeof why, "%s %s", buf, sugg[i].why);
        print(why, x + 2, y + 25, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }

    // ── the palette: the current vocab spelled in your key ──
    { const HbVocab *v = cur_vocab();
      int r1 = MODES[modeSel].row1; if (r1 > v->n) r1 = v->n;
      int r2 = v->n - r1;
      font(FONT_SMALL); print(MODES[modeSel].lab1, 8, 108, CLR_INDIGO); font(FONT_NORMAL);
      for (int i = 0; i < r1; i++) {
          int f = fn_at(i);
          chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
          if (ui_button(8 + i * 52, 114, 50, 18, buf)) add_fn(f);
      }
      if (r2 > 0) {
          font(FONT_SMALL); print(MODES[modeSel].lab2, 8, 134, CLR_INDIGO); font(FONT_NORMAL);
          for (int i = 0; i < r2; i++) {
              int f = fn_at(r1 + i);
              chname(buf, sizeof buf, (keyPc + v->off[f]) % 12, v->qual[f]);
              if (ui_button(8 + i * 44, 140, 42, 18, buf)) add_fn(f);
          }
      } }

    // ── voicing + accompaniment row (voicing | the band) ──
    if (ui_button(2, 161, 38, 15, seventh ? "7THS" : "TRIAD")) { seventh = !seventh; if (!seventh && invSel > 2) invSel = 2; }
    if (ui_button(42, 161, 38, 15, STRUMS[strumSel].name)) strumSel = (strumSel + 1) % NSTRUM;
    if (ui_button(82, 161, 30, 15, INV_LAB[invSel])) invSel = (invSel + 1) % (seventh ? 4 : 3);
    if (ui_button(114, 161, 28, 15, OCT_LAB[octSel])) octSel = (octSel + 1) % 3;
    if (ui_button(144, 161, 34, 15, BASS_LAB[bassSel])) bassSel = (bassSel + 1) % 4;
    if (ui_button(180, 161, 38, 15, DRUM_LAB[drumSel])) drumSel = (drumSel + 1) % 3;
    if (ui_button(220, 161, 36, 15, fillOn ? "FILL*" : "FILL")) fillOn = !fillOn;
    if (ui_button(258, 161, 36, 15, melOn ? "MEL*" : "MEL")) melOn = !melOn;

    // ── transport + tempo row ──
    if (ui_button(4, 180, 40, 15, playing ? "STOP" : "PLAY")) {
        if (plen) { playing = !playing; playSlot = 0; playT = 0; bassLast = 36 + (octSel - 1) * 12; melLast = 72 + (octSel - 1) * 12; melI = 0; }
    }
    if (ui_button(48, 180, 40, 15, "UNDO")) { if (plen) { plen--; rethink(); } }
    if (ui_button(92, 180, 32, 15, "CLR"))  { plen = 0; playing = 0; rethink(); }
    font(FONT_SMALL);
    snprintf(buf, sizeof buf, "%d BPM", cur_bpm());
    print(buf, 150, 181, CLR_INDIGO);
    ui_slider(&bpmN, 186, 183, 128, NULL);        // tempo of the SPACE loop
    font(FONT_NORMAL);

    ui_end();
}

// ── spec — the deterministic oracle the brief demanded (spec-harness.md) ────
#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(1);                                   // init once
    hb_selfcheck();                            // the header's own assertions

    // the cold-open strip is the doo-wop, correctly named
    expect_eq(plen, 4, "cold-open seeds the doo-wop");
    expect(pfn[0]==HB_I && pfn[1]==HB_vi && pfn[2]==HB_IV && pfn[3]==HB_V,
           "cold-open reads I vi IV V");
    plen = 0; rethink();

    // drive the cart's pure core: enter ii-V-I, read the strip's analysis
    add_fn(HB_ii); add_fn(HB_V); add_fn(HB_I);
    expect_eq(plen, 3, "three chords in the strip");
    expect(pfn[0] == HB_ii && pfn[1] == HB_V && pfn[2] == HB_I, "strip reads ii V I");
    expect(nsugg >= 1 && sugg[0].f == HB_vi, "after I the top pull is vi (relative)");

    // move the key under the same chords: numerals re-derive, honestly.
    // Dm7 G7 Cmaj7 heard in G = v, I7, IV — nothing invented, all re-looked-up.
    keyPc = 7; rethink();
    expect(pfn[0] == HB_v && pfn[1] == HB_I7 && pfn[2] == HB_IV,
           "same chords in G re-read as v I7 IV");

    // LOUNGE (10 functions) can't advise from I7 (function 12) — honest silence
    modeSel = 1; resuggest();
    keyPc = 0; rethink();                      // back home: last chord = I again
    expect(nsugg >= 1, "lounge suggests from I");

    // POP re-ranks the same table: from I the diatonic pull is V (axis start)
    modeSel = 2; resuggest();
    expect(nsugg >= 1 && sugg[0].f == HB_V, "pop from I leans V (the I-V-vi-IV axis)");

    // MINOR mode (index 4): the same tap chords re-analyse against the minor
    // vocab, and the minor brain suggests over its own world. i VI III in A minor.
    modeSel = 4; plen = 0; keyPc = 9; rethink();   // A minor
    add_fn(HBm_i); add_fn(HBm_VI); add_fn(HBm_III);
    expect(pfn[0] == HBm_i && pfn[1] == HBm_VI && pfn[2] == HBm_III,
           "minor strip reads i VI III in A minor");
    expect(nsugg >= 1, "minor mode suggests from III");
    // a chord outside the minor vocab is honest: F#m7 has no seat in A minor
    add_chord(6, HBQ_MIN7);
    expect_eq(pfn[3], -1, "F#m7 in A minor -> ? (outside the vocab, not guessed)");

    // DORIAN mode (index 6): C dorian's tell is the major IV. i IV VII named, and
    // the vamp pulls back to i. F major here is IV (dorian), not "?" as in C minor.
    modeSel = 6; plen = 0; keyPc = 0; rethink();
    add_fn(HBd_i); add_fn(HBd_IV); add_fn(HBd_VII);
    expect(pfn[0] == HBd_i && pfn[1] == HBd_IV && pfn[2] == HBd_VII,
           "dorian strip reads i IV VII in C dorian");
    add_chord(5, HB_TRIAD_MAJ);                      // an F MAJOR triad = IV in C dorian
    expect_eq(pfn[3], HBd_IV, "F major in C dorian -> IV (the bright IV, named)");

    // MIXOLYDIAN (index 7): the major bVII is diatonic here (Bb in C = bVII, the
    // rock cadence), where in C MAJOR that same chord is the borrowed bVII7.
    modeSel = 7; plen = 0; keyPc = 0; rethink();
    add_fn(HBmx_I); add_fn(HBmx_bVII); add_fn(HBmx_IV);
    expect(pfn[0] == HBmx_I && pfn[1] == HBmx_bVII && pfn[2] == HBmx_IV,
           "mixo strip reads I bVII IV in C");
    add_chord(10, HB_TRIAD_MAJ);                     // Bb major triad = bVII
    expect_eq(pfn[3], HBmx_bVII, "Bb major in C mixo -> bVII (named diatonic)");

    // PHRYGIAN (index 8): the tell is the major bII a half-step up (Db in C).
    modeSel = 8; plen = 0; keyPc = 0; rethink();
    add_fn(HBph_i); add_fn(HBph_bII);
    expect(pfn[0] == HBph_i && pfn[1] == HBph_bII, "phryg strip reads i bII in C");
    add_chord(1, HB_TRIAD_MAJ);                      // Db major triad = bII
    expect_eq(pfn[2], HBph_bII, "Db major in C phrygian -> bII (the tell)");

    // LYDIAN (index 9): the tell is the major II a whole-step up (D in C).
    modeSel = 9; plen = 0; keyPc = 0; rethink();
    add_fn(HBly_I); add_fn(HBly_II);
    expect(pfn[0] == HBly_I && pfn[1] == HBly_II, "lydian strip reads I II in C");
    add_chord(2, HB_TRIAD_MAJ);                      // D major triad = II
    expect_eq(pfn[2], HBly_II, "D major in C lydian -> II (the bright II)");

    // BLUES mode (index 10): A7 D7 E7 in A = I7 IV7 V7 (all dominant, the 12-bar)
    modeSel = 10; plen = 0; keyPc = 9; rethink();
    add_fn(HBbl_I); add_fn(HBbl_IV); add_fn(HBbl_V);
    expect(pfn[0] == HBbl_I && pfn[1] == HBbl_IV && pfn[2] == HBbl_V,
           "blues strip reads I7 IV7 V7 in A");

    // TRIAD display: the 7 toggle spells (and voices) plain triads instead of 7ths
    { char a[16], b[16];
      seventh = 1; chname(a, sizeof a, 0, HBQ_MAJ7);   // C
      seventh = 0; chname(b, sizeof b, 0, HBQ_MAJ7);
      expect(!strcmp(a, "Cmaj7") && !strcmp(b, "C"), "spell: Cmaj7 <-> C (triad toggle)");
      chname(a, sizeof a, 9, HBQ_MIN7);                // Am (drop the m7)
      expect(!strcmp(a, "Am"), "spell: A minor7 as a triad = Am");
      chname(a, sizeof a, 11, HBQ_M7B5);               // Bdim (the diminished triad)
      expect(!strcmp(a, "Bdim"), "spell: B m7b5 as a triad = Bdim");
      seventh = 1; }

    // undo + clear keep the machine consistent (back in the major default)
    modeSel = 0; plen = 0; keyPc = 0;
    add_fn(HB_ii); add_fn(HB_V); add_fn(HB_I);
    plen--; rethink();                         // drop the I → last chord = V again
    expect(nsugg >= 1 && sugg[0].f == HB_I, "after V the top pull is I (home)");
    plen = 0; rethink();
    expect_eq(nsugg, 0, "empty strip = no advice");
}
#endif
