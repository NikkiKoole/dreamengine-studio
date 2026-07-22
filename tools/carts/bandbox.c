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
    "detail": "The build of the bandbox brief (docs/design/bandbox.md): the draw-only mockup wired for real. The chord lane analyzes in roman numerals via the shared harmony brain (harmony.h); the ^/v spinner steps a chord in-key; the keybed sets the selected chord's root. Playback ports chordwise's subdivided-bar loop (chord comp, walking/idiomatic bass, drumkit groove, blooming melody, held pad) - all reading the declared KEY + MODE, so the band plays the genre's idiom (14 genres: BOSSA/LOUNGE/POP/FOLK/MINOR/CINE/DORIAN/MIXO/PHRYG/LYDIAN/BLUES + four lifted from the radio stations: JANGLE, JINGLE, NAPOLN, CITYPO - the last two with their OWN harmony (HB_JINGLE Mac-DeMarco gravity, HB_CITYPOP Royal Road)). The sequencer difference: p-locks. Each bar carries per-cell overrides that playback reads as p-lock-else-global. The 2026-07-22 radio-idiom sweep (mined from the radio carts' bar-level moves) widened the vocab: chord COMP ANTIC/STAB/HELD/TACET, bass APPR/POP/GHOST/PEDAL, drum HALF/OPEN/HATS/DOUBLE, mel HOLD/DOUBLE/OCT+, pad SWELL/HIGH, and the band-wide FEELs HUSH/STOP/LIFT - enough to lay out bars that read as bossa (ANTIC+APPR), citypop (POP+OPEN+ANTIC+LIFT), napoleon disco (POP+GHOST+OPEN), or a cocktail hush. Deterministic (carries a spec()); no swing-jitter/life yet, same call chordwise made for spec-ability.",
    "controls": "Tap a tracker cell to edit it IN PLACE: that voice's lane rises to the top row (staying visible so you see the change land) and its block editor unfolds in the freed space below; tap another cell in the promoted lane to scrub to that bar, BACK (rail) closes it. CHORDS editor: a global TONE (PLUCK/EPIANO/ORGAN) + ^/v step the chord in-key + STRUM/INV/OCT/7TH/COMP chips (AUTO = follow the global voicing, else a per-cell p-lock; COMP = the bar's comp texture: ANTIC strikes the NEXT bar's chord on the last 8th and ties over the barline (the next downbeat comp is skipped, never doubled) - the bossa/citypop push - STAB chokes the hits short, HELD rings them to the bar line, TACET rests the comp); the keybed sets the chord's root. BASS: global STYLE + TONE (the bass SOUND: SYNTH/SUB/FM/UPRIGHT pizzicato) + per-cell FOLLOW/MUTE/HOLD/WALK/OCT/FILL/APPR/POP/GHOST/PEDAL (drop out, sit on the root, walk, octave-pump, run a fill, lead in chromatic on beat 4, the disco octave-pop cell, soft ghosts in the style's rests, or a key-tonic pedal point). DRUMS: global STYLE + KIT (ELECTRO/ACOUSTIC) + BUSY (SPARSE/NORMAL/BUSY - the Apple-Drummer simple<->complex axis: SPARSE drops the hats, BUSY fills offbeat hats + ghost snares) + per-cell GROOVE/DROP/KICK/FILL/CRASH/BUILD/HALF/OPEN/HATS/DOUBLE (half-time backbeat, offbeat open hats, hats-only breakdown, 16th-hat bar). MEL: global TONE (SINE/SQR/FM/BELL) + per-cell FOLLOW/REST/ACCENT/HOLD/DOUBLE/OCT+ (one bar-long tone, a double-time run, the bloom an octave up). PAD: global TONE (SINE/SAW/STRINGS) + per-cell FOLLOW/OFF/ON/SWELL/HIGH (a slow crescendo across the bar; the bed +12). Every BAR also has a FEEL in the editor header (STRAIGHT/ACCENT/DRAG/HUSH/STOP/LIFT) - a per-bar performance p-lock the whole band shares: ACCENT punches the bar louder, DRAG lays it behind the beat, HUSH pulls everyone down (the room leans in), STOP is the band stop (one accented downbeat hit then air), LIFT is the truck-driver gear change (+2 semitones from that bar to the loop's end; the wrap comes back down). Next to FEEL sits VOLTA (ALL/ODD/EVEN/4TH/VAR2/VAR4) - the 1st/2nd-ending mark over a 4-ROUND cycle: ODD/EVEN/4TH gate WHEN the bar sounds (rest otherwise - the classic volta endings), VAR2/VAR4 always sound but engage the bar's p-locks + FEEL only on even rounds / round 4 (plain groove otherwise - variation-on-the-repeat without silence; song structure lite). The tracker shows it live: resting bars dim, dormant VAR locks grey their pip. All deterministic; PUSH/ahead is still a follow-up - ANTIC covers the anticipation idiom forward-only. Tap a voice rail header to mute/unmute the lane (chords included). Nav: SONG (top-left) opens the SONGS popup - your autosaved song (MINE) + built-in demos (CITY POP = the Royal-Road demo with the full lock set, BOSSA = the early change + chromatic lead-in, NAPOLEON = the disco-funk boogie vamp with a STOP break, ACCENT re-entry and BUILD riser); picking one loads it. The song AUTOSAVES on every edit (a versioned 120-byte blob via save_bytes - the save file format); loading a demo is not an edit, so MINE survives browsing until you edit on top. < KEY > steps the key (STOPPED re-analyzes, PLAYING transposes), MODE cycles the genre, NEW empties to a fresh song (keeping your key/genre/voice setup), the play button loops. Tap the '+' chord cell to open the ADD-CHORD picker: the harmony brain's NEXT suggestions (ranked, what the progression wants) + the full mode palette as roman-numeral chips + the live keybed for any root; each pick appends + auditions and stays open. Keys: SPACE loop, LEFT/RIGHT key, B mode, N new song, BACKSPACE closes the editor/picker. MUSICAL TYPING (GarageBand-style): the QWERTY rows play the keybed - home row A S D F G H J K L (;) = white keys C D E F G A B C D E, top row W E T Y U O P = the black keys; each press adds/edits a chord on that root (opens the picker if idle), so you can type a progression. While the loop plays, the keybed LIGHTS UP (green) the notes the in-view voice is triggering, folded onto the keys - the focused voice when editing, or the chords while picking (the full tracker view stays dark)."
  },
  "todo": [
    "SONG STRUCTURE (the leap from loop to song, the biggest gap for real pop songs): A/B sections (verse/chorus/bridge), each with its OWN progression + arrangement (which voices on, BUSY/feel), plus a SONG ORDER (A A B A...). A pop song IS the contrast between sections; right now bandbox is one 4-8 bar loop forever. Composes with every per-bar param already built (mute/BUSY/feel/p-locks). The VOLTA lock (2026-07-22) delivered a first slice - rounds-aware bars/locks over a 4-round cycle - but real sections still need the data model. Sketch it + how it fits the 160x100 face BEFORE wiring - it's the biggest structural change. (Jump-voltas - the loop actually skipping/shortening - belong to this too.)",
    "A real MELODY / HOOK: the mel voice currently ARPEGGIATES chord tones (tasteful filler, not a topline you'd hum). Let the melody be COMPOSED - a note lane you place, or motif logic that states + repeats a phrase. Pop lives on the hook; a song without one is a backing track.",
    "Cheap pop wins: extend past 8 bars (16) so a full verse fits; add sus/add9 chord qualities (the pop sparkle) - needs a vocab/quality addition in harmony.h.",
    "PUSH feel (ahead of the beat): still needs bar-LOOKAHEAD scheduling (schedule bar N's downbeat during bar N-1) - the forward-only step scheduler can't fire before the beat. The chord lane's ANTIC comp lock now covers the anticipation idiom forward-only (it strikes the next chord late in THIS bar); a whole-band PUSH remains the follow-up.",
    "band.h extraction: the genre-idiom tables + harmony vocab + p-lock vocab + tone-preset machinery is now proven across bandbox - extract a shared band.h so chordwise + the radios draw from ONE source (the copy-first rule is satisfied). The big structural consolidation.",
    "Notation glyphs on a roomier glass (320x200): tracker cells show WHAT each lock is as a real notation mark (ANTIC = a tie arc across the cell boundary, STAB = staccato dot, AUTO = the simile/repeat mark, real sharps/flats + jazz quality shorthand in the chord cells) instead of an orange pip; editor segs keep words (the stranger bar). Symbol inventory + the have-vs-make sourcing plan: docs/design/music-notation-glyphs.md. First probe = a draw-only 320x200 mockup with glyphs faked in.",
    "Play-test + tune pass: several tones/grooves were verified by spec + stem A/B, not full-mix listening - check the mel/pad/chord TONES, the new genre grooves (esp. NAPOLN/CITYPO), the DRAG amount (3 frames), whether the mid-register PAD muds with the comp, and the 2026-07-22 radio-idiom locks (STAB's 60ms choke, POP's velocities, HALF's hat rate, the SWELL curve - linear now, maybe wants a curve)."
  ]
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
// dur > 0 = ring exactly that long (hit()); dur 0 = the instrument's own envelope
// (note()) — the STAB/HELD comp locks + the mel HOLD lock ride the dur channel.
#define NPEND 16
static struct { int midi, instr, vol, at, dur; } pend[NPEND];
static int npend = 0;
static void fire_note(int midi, int instr, int vol, int dur) {
    if (dur > 0) hit(midi, instr, vol, dur); else note(midi, instr, vol);
}
static void queue_dur(int midi, int instr, int vol, int at, int dur) {
    if (at <= 0) { fire_note(midi, instr, vol, dur); return; }
    if (npend >= NPEND) return;
    pend[npend].midi = midi; pend[npend].instr = instr;
    pend[npend].vol = vol;   pend[npend].at = at;   pend[npend].dur = dur;   npend++;
}
static void queue_note(int midi, int instr, int vol, int at) { queue_dur(midi, instr, vol, at, 0); }
static void pump_notes(void) {
    for (int i = 0; i < npend; ) {
        if (--pend[i].at <= 0) { fire_note(pend[i].midi, pend[i].instr, pend[i].vol, pend[i].dur);
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
    { "CITYPO", &HB_CITYPOP,      0 },   // J-pop Royal Road (IVmaj7-V7-iii-vi), smooth funk (citypop.c)
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
    int strum, inv, oct, sev;    // CHORD voicing p-locks (-1 = auto)
    int comp;                    // CHORD comp p-lock (CPL_*): -1 auto · ANTIC/STAB/HELD/TACET
    int bass;                    // BASS p-lock (BPL_*): -1 FOLLOW · MUTE/HOLD/WALK/OCT/FILL/APPR/POP/GHOST/PEDAL
    int fill;                    // DRUM p-lock (DPL_*): GROOVE(0)/DROP/KICK/FILL/CRASH/BUILD/HALF/OPEN/HATS/DOUBLE
    int mel;                     // MEL  p-lock (MPL_*): -1 auto · REST/ACCENT/HOLD/DOUBLE/OCT+
    int pad;                     // PAD  p-lock (PPL_*): -1 auto · OFF/ON/SWELL/HIGH
    int feel;                    // per-BAR FEEL (FEEL_*): the whole band's groove this bar
    int volta;                   // per-BAR VOLTA (VLT_*): which ROUNDS the bar/locks engage
} Bar;
// VOLTA — the 1st/2nd-ending mark, generalized over a 4-ROUND cycle (rounds 1..4,
// then wrap). Two families: GATE THE BAR — ODD/EVEN/4TH sound only those rounds
// (rest otherwise; the classic volta endings) — and GATE THE LOCKS — VAR2/VAR4
// always sound but the bar's p-locks + FEEL engage only on even rounds / round 4
// (plain groove otherwise). VAR is "variation on the repeat" without silence: the
// variation vocabulary is the whole existing lock set, gated by pass.
enum { VLT_ALL, VLT_ODD, VLT_EVEN, VLT_4TH, VLT_VAR2, VLT_VAR4, VLT_N };
static const char *VLT_LAB[VLT_N] = { "ALL", "ODD", "EVEN", "4TH", "VAR2", "VAR4" };
// FEEL — a per-bar performance p-lock the whole band shares (deterministic, spec-safe):
// ACCENT punches the bar louder, DRAG lays it behind the beat, HUSH pulls everyone
// down (cocktail's "the room leans in"), STOP is the band stop (one accented downbeat
// hit, then silence), LIFT is the truck-driver gear change (citypop/italo): from the
// LIFT bar to the loop's end everything SOUNDS +2 semitones; the wrap comes back down.
// (PUSH/ahead needs bar-lookahead in a forward-only scheduler — still a follow-up;
// the chord lane's ANTIC lock covers the anticipation idiom forward-only.)
enum { FEEL_STRAIGHT, FEEL_ACCENT, FEEL_DRAG, FEEL_HUSH, FEEL_STOP, FEEL_LIFT };
static const char *FEEL_LAB[6] = { "STRAIGHT", "ACCENT", "DRAG", "HUSH", "STOP", "LIFT" };
#define NFEEL 6
#define FEEL_DRAG_FR 3           // frames the whole bar lags when DRAGged
static Bar arr[NBARS];
static int nbars = 4;            // loop length (contiguous bars 0..nbars-1)
static int pfn[NBARS];           // each bar's analyzed function (-1 = ? / out of vocab)

// the ROUND counter — which pass of the loop is sounding (0-based, mod 4 → rounds
// 1..4). Wraps with the loop; reset on play start (stopped = round 1, so auditions
// + the idle tracker read as the plain first pass).
static int passRound = 0;
static int cur_round(void) { return (passRound & 3) + 1; }   // 1..4
// does this bar SOUND this round? (the gate-the-bar volta family)
static int bar_sounds_at(int b, int round) {
    switch (arr[b].volta) {
        case VLT_ODD:  return round == 1 || round == 3;
        case VLT_EVEN: return round == 2 || round == 4;
        case VLT_4TH:  return round == 4;
    }
    return 1;                                          // ALL / VAR2 / VAR4 always sound
}
// are this bar's p-locks + FEEL LIVE this round? (the gate-the-locks family)
static int locks_live_at(int b, int round) {
    if (arr[b].volta == VLT_VAR2) return round == 2 || round == 4;
    if (arr[b].volta == VLT_VAR4) return round == 4;
    return 1;
}
static int bar_sounds(int b) { return bar_sounds_at(b, cur_round()); }
static int locks_live(int b) { return locks_live_at(b, cur_round()); }
// a bar's EFFECTIVE feel: STRAIGHT while its locks are gated off this round.
static int bar_feel(int b)  { return locks_live(b) ? arr[b].feel : FEEL_STRAIGHT; }

static int feel_delay_fr(int b) { return bar_feel(b) == FEEL_DRAG ? FEEL_DRAG_FR : 0; }
static int feel_vel(int b, int base) {
    if (bar_feel(b) == FEEL_ACCENT || bar_feel(b) == FEEL_STOP) { base += 2; if (base > 7) base = 7; }
    if (bar_feel(b) == FEEL_HUSH) { base -= 2; if (base < 1) base = 1; }
    return base;
}
// LIFT: playback-only transpose — the chart + analysis stay in the written key.
// (a VAR-gated LIFT transposes only on its live rounds — the every-4th final-chorus.)
static int bar_tp(int b) {
    for (int i = 0; i <= b && i < nbars; i++) if (bar_feel(i) == FEEL_LIFT) return 2;
    return 0;
}
// STOP: everything after the (accented, via feel_vel) downbeat is silenced.
static int feel_stopped(int b, int s) { return bar_feel(b) == FEEL_STOP && s > 0; }

// the band's voices (the rail = the tracker's row headers). von = lane on/off.
static const char *VL[VOICES]  = { "CH", "BA", "ME", "DR", "PA" };
static int von[VOICES] = { 1, 1, 1, 1, 0 };   // pad off by default (chords always on)

// global voice STYLE defaults
enum { B_HOLD, B_ROOT5, B_WALK, B_BAND };
static const char *BASS_LAB[4] = { "HOLD", "R-5", "WALK", "BAND" };
// per-CELL bass p-locks (arr[].bass; -1 = FOLLOW the global style). One-bar
// overrides a bassist would punch in: drop out, sit on the root, walk, pump the
// octave, or run a fill into the next chord. The 2026-07-22 radio-idiom sweep added
// APPR (the style, but beat 4 leads in chromatic — bossa/jingle/cocktail's move),
// POP (the disco octave-pop cell ending in the run — citypop/napoleon), GHOST (the
// style + soft root ghosts in its rests), and PEDAL (insist on the KEY's tonic
// under every chord — the pre-chorus tension device).
enum { BPL_FOLLOW = -1, BPL_MUTE = 0, BPL_HOLD, BPL_WALK, BPL_OCT, BPL_FILL,
       BPL_APPR, BPL_POP, BPL_GHOST, BPL_PEDAL };
static const char *BPL_LAB[10] = { "FOLLOW", "MUTE", "HOLD", "WALK", "OCT", "FILL",
                                   "APPR", "POP", "GHOST", "PEDAL" };
static const int   BPL_VAL[10] = { BPL_FOLLOW, BPL_MUTE, BPL_HOLD, BPL_WALK, BPL_OCT, BPL_FILL,
                                   BPL_APPR, BPL_POP, BPL_GHOST, BPL_PEDAL };
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
// BUSY — the Apple-Drummer "Simple <-> Complex" axis, global to the drums voice and
// deterministic: SPARSE drops the hats (kick+backbeat only), NORMAL is the genre
// groove, BUSY fills offbeat hats + a soft ghost snare. genre=who, KIT=what, BUSY=how much.
static const char *BUSY_LAB[3] = { "SPARSE", "NORMAL", "BUSY" };
static int drumBusy = 1;
// per-CELL drum p-locks (arr[].fill; GROOVE is the default, the rest are one-bar
// moves a drummer punches in): drop out, strip to the kick, a genre fill, a crash
// accent over the groove, or a whole-bar snare build. The radio-idiom sweep added
// HALF (half-time: the backbeat lands once, mid-bar — napoleon FOREVER's gated 8),
// OPEN (offbeat open hats over the groove — the disco "tss"), HATS (breakdown: the
// hat line alone), and DOUBLE (16th hats for one bar — the one-bar BUSY punch).
enum { DPL_GROOVE, DPL_DROP, DPL_KICK, DPL_FILL, DPL_CRASH, DPL_BUILD,
       DPL_HALF, DPL_OPEN, DPL_HATS, DPL_DOUBLE };
static const char *DPL_LAB[10] = { "GROOVE", "DROP", "KICK", "FILL", "CRASH", "BUILD",
                                   "HALF", "OPEN", "HATS", "DOUBLE" };

// per-CELL chord COMP p-locks (arr[].comp; -1 = AUTO comp): ANTIC also strikes the
// NEXT bar's chord on the last 8th (the bossa early change / citypop brass push —
// forward-only: it fires late in THIS bar, no lookahead needed) and TIES over the
// barline (the next bar's downbeat comp is skipped — the anticipation replaces it,
// never doubles it) · STAB chokes every comp hit short (the funk chuck) · HELD lets
// each hit ring to the bar's end (the held-Rhodes texture) · TACET rests the comp.
enum { CPL_ANTIC, CPL_STAB, CPL_HELD, CPL_TACET, CPL_N };
static const char *CPL_LAB[CPL_N] = { "ANTIC", "STAB", "HELD", "TACET" };
// resolve a bar's effective chord-voicing (p-lock else global; a VAR-gated bar's
// locks read as AUTO on its plain rounds)
static int bar_strum(int b) { return locks_live(b) && arr[b].strum >= 0 ? arr[b].strum : strumSel; }
static int bar_inv(int b)   { return locks_live(b) && arr[b].inv   >= 0 ? arr[b].inv   : invSel;   }
static int bar_oct(int b)   { return locks_live(b) && arr[b].oct   >= 0 ? arr[b].oct   : octSel;   }
static int bar_sev(int b)   { return locks_live(b) && arr[b].sev   >= 0 ? arr[b].sev   : seventh;  }

static int playing = 0, playSlot = 0, playT = 0;
static int selVoice = -1;   // which voice's editor is open (-1 = the tracker)
static int selBar   = 0;    // which bar the editor edits
static int helpOn   = 0;    // the legend overlay (? button)
static int picking  = 0;    // the ADD-CHORD picker is open (tap "+")
static int songsOn  = 0;    // the SONGS popup is open (demos + your autosave)
static int haveSave = 0;    // probed when the popup opens: does a MINE blob exist?

// keybed note-lights: per-pitch-class glow that decays each frame. ONLY the voice a
// single instrument is focused on (the edited voice, or CHORDS while picking) lights
// its notes, folded (mod 12) onto the tiny keybed — a live "what's playing" readout.
// In the full tracker (no voice selected) the keybed stays dark (lit_voice = -1).
static float keyLit[12];
static void  light_pc(int pc) { keyLit[((pc % 12) + 12) % 12] = 1.0f; }
static int   lit_voice(void)  { return picking ? V_CH : selVoice; }

// ── chord voicing (chordwise's sound_chord, now p-lock-parameterized) ──────
static void sound_chord(int rootPc, int q, int withBass, int baseDelay,
                        int strum, int inv, int oct, int sev, int vel, int durMs) {
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
        queue_dur(m[i], inst[i], vel, baseDelay + p * gap, durMs);
    }
}
// audition a bar's chord with its resolved voicing (+ its soft root)
static void audition(int b) {
    sound_chord((arr[b].rootPc + bar_tp(b)) % 12, arr[b].qual, 1, 0,
                bar_strum(b), bar_inv(b), bar_oct(b), bar_sev(b), 5, 0);
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
    /* CITYPO */ { BT_ROOT,  BT_FIFTH, BT_SIXTH,  BT_FIFTH },  // smooth melodic root-fifth-six
};
static int bassLast = 36;
// what the GLOBAL style would play this beat (-1 = rest) — factored out so the
// APPR/GHOST p-locks can lean on it (they play AROUND the style, not instead of it).
static int bass_follow_pc(int beat, int r0, int q, int rN, int *oct) {
    if (bassSel == B_HOLD) return beat == 0 ? r0 : -1;
    if (bassSel == B_ROOT5) {
        if (beat == 0) return r0;
        if (beat == 2) return (r0 + 7) % 12;
        return -1;
    }
    if (bassSel == B_WALK) {
        if (beat == 0) return r0;
        if (beat == 1) return (r0 + hb_tones[q][1]) % 12;
        if (beat == 2) return (r0 + hb_tones[q][2]) % 12;
        return (rN + 11) % 12;
    }
    switch (BASS_PAT[modeSel][beat]) {            // B_BAND — the genre idiom
        case BT_ROOT:   return r0;
        case BT_OCTU:   *oct = +1; return r0;
        case BT_FIFTH:  return (r0 + hb_tones[q][2]) % 12;
        case BT_FIFTHD: *oct = -1; return (r0 + hb_tones[q][2]) % 12;
        case BT_THIRD:  return (r0 + hb_tones[q][1]) % 12;
        case BT_SIXTH:  return (r0 + 9) % 12;
        case BT_FLAT2:  return (r0 + 1) % 12;
        case BT_FLAT7:  return (r0 + 10) % 12;
        case BT_APPR:   return (rN + 11) % 12;
    }
    return -1;                                    // BT_REST
}
static void play_bass_beat(int beat, int delay) {
    if (!von[V_BA] || !nbars) return;
    if (!bar_sounds(playSlot)) return;           // VOLTA — the bar rests this round
    int bl = locks_live(playSlot) ? arr[playSlot].bass : BPL_FOLLOW;   // per-cell p-lock, VAR-gated
    if (bl == BPL_MUTE) return;                  // MUTE — drop out this bar
    if (feel_stopped(playSlot, beat)) return;    // STOP — only the downbeat hit

    int nb = (playSlot + 1) % nbars;
    int r0 = (arr[playSlot].rootPc + bar_tp(playSlot)) % 12, q = arr[playSlot].qual;
    int rN = (arr[nb].rootPc + bar_tp(nb)) % 12;
    int base = 36 + (octSel - 1) * 12;
    int lo = base - 4, hi = base + 12;
    int pc = -1, oct = 0, vel = bass_vel();

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
    } else if (bl == BPL_APPR) {                  // APPR — the style, but beat 4 leads in
        if (beat == 3) pc = (rN + 11) % 12;       // chromatic from below into the next root
        else pc = bass_follow_pc(beat, r0, q, rN, &oct);
    } else if (bl == BPL_POP) {                   // POP — the disco/citypop octave-pop cell
        if      (beat == 0) pc = r0;
        else if (beat == 1) { pc = r0; oct = 1; if (vel > 1) vel--; }
        else if (beat == 2) pc = (r0 + hb_tones[q][2]) % 12;
        else                pc = (rN + 11) % 12;  // ... ending in the run into the change
    } else if (bl == BPL_GHOST) {                 // GHOST — the style + soft ghosts in its rests
        pc = bass_follow_pc(beat, r0, q, rN, &oct);
        if (pc < 0) { pc = r0; oct = 0; vel = 2; }
    } else if (bl == BPL_PEDAL) {                 // PEDAL — insist on the KEY's tonic
        pc = (keyPc + bar_tp(playSlot)) % 12;     // under every chord (pre-chorus tension)
    } else {                                      // FOLLOW — the global style
        pc = bass_follow_pc(beat, r0, q, rN, &oct);
    }
    if (pc < 0) return;
    int n = rad_bass_to(pc, bassLast, lo, hi);
    bassLast = n;
    if (lit_voice() == V_BA) light_pc(n + oct * 12);
    queue_note(n + oct * 12, I_BSS, feel_vel(playSlot, vel), delay + feel_delay_fr(playSlot));
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
    /* CITYPO */ { "x..x....", "....x...", "x.x.x.x.", DK_SNARE,  DK_HHC },  // smooth pop-funk backbeat
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
    /* CITYPO */ { "....sshl", 1 },  // a smooth tom fill
};
static const FillPat FILL_PLAIN = { "....sshl", 1 };
static void play_drum_step(int step, int delayMs) {
    if (!von[V_DR] || !nbars) return;
    if (!bar_sounds(playSlot)) return;           // VOLTA — the bar rests this round
    int dp = locks_live(playSlot) ? arr[playSlot].fill : DPL_GROOVE;   // DPL_* per-cell move, VAR-gated
    if (dp == DPL_DROP) return;                  // DROP — silent this bar
    delayMs += feel_delay_fr(playSlot) * 1000 / 60;   // FEEL: the whole bar drags/accents
    const DrumPat *d = (drumSel == 1) ? &DRUM_PAT[modeSel] : &DRUM_PLAIN;

    if (bar_feel(playSlot) == FEEL_STOP) {       // STOP — one accented kick on 1, then air
        if (step == 0) dk_fire_at(delayMs, DK_KICK, 0, feel_vel(playSlot, 5));
        return;
    }
    if (dp == DPL_HALF) {                        // HALF — half-time: the backbeat lands once,
        if (step == 0) dk_fire_at(delayMs, DK_KICK, 0, feel_vel(playSlot, 6));
        if (step == 4) dk_fire_at(delayMs, d->backRole, 0, feel_vel(playSlot, 6));   // mid-bar, BIG
        if (drumBusy >= 1 && (step == 0 || step == 4))
            dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 3));
        return;
    }
    if (dp == DPL_OPEN) {                        // OPEN — offbeat open hats (the disco "tss")
        if (d->kick[step] == 'x') dk_fire_at(delayMs, DK_KICK,     0, feel_vel(playSlot, 6));
        if (d->back[step] == 'x') dk_fire_at(delayMs, d->backRole, 0, feel_vel(playSlot, 5));
        if (drumBusy >= 1 && (step % 2)) dk_fire_at(delayMs, DK_HHO, 0, feel_vel(playSlot, 3));
        return;
    }
    if (dp == DPL_HATS) {                        // HATS — breakdown: the hat line alone
        if (d->hat[step] == 'x') dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 3));
        else if (drumBusy == 2)  dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 2));
        return;
    }
    if (dp == DPL_DOUBLE) {                      // DOUBLE — 16th hats for one bar
        if (d->kick[step] == 'x') dk_fire_at(delayMs, DK_KICK,     0, feel_vel(playSlot, 6));
        if (d->back[step] == 'x') dk_fire_at(delayMs, d->backRole, 0, feel_vel(playSlot, 5));
        dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 3));
        return;
    }
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
    // BUSY: SPARSE(0) drops the hats; NORMAL(1) plays the genre hats; BUSY(2) fills the
    // gaps with soft offbeat hats + a ghost snare on the 'e/a' between backbeats.
    if (drumBusy >= 1 && d->hat[step] == 'x') dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 3));
    if (drumBusy == 2 && bar_feel(playSlot) != FEEL_HUSH) {   // HUSH feathers the extras away
        if (d->hat[step] != 'x') dk_fire_at(delayMs, d->hatRole, 0, feel_vel(playSlot, 2));
        if ((step == 3 || step == 6) && d->back[step] != 'x') dk_fire_at(delayMs, d->backRole, 0, feel_vel(playSlot, 2));
    }
}

// ── melody (chordwise's bloom; per-cell rest/accent p-lock) ─────────────────
static const char *MEL_PAT[NMODE] = {
    /* BOSSA */ "..x..x.x", /* LOUNGE */ "x...x.x.", /* POP */ "x..x.x..",
    /* FOLK */ "x...x...", /* MINOR */ "x.x..x.x", /* CINE */ "x.......",
    /* DORIAN */ "..x...x.", /* MIXO */ "x..x.x..", /* PHRYG */ "x.x.x...",
    /* LYDIAN */ "x.....x.", /* BLUES */ "x..x..x.",
    /* JANGLE */ "x.x.x.x.", /* JINGLE */ "x...x.x.", /* NAPOLN */ "..x..x.x",
    /* CITYPO */ "x..x.x.x",
};
// per-CELL mel p-locks (arr[].mel; -1 = FOLLOW the bloom): REST · ACCENT (louder) ·
// HOLD (one long tone — improv.h's phrase-end resolve) · DOUBLE (double-time run —
// the solo-heats-up bar) · OCT+ (the bloom an octave up).
enum { MPL_FOLLOW = -1, MPL_REST = 0, MPL_ACCENT, MPL_HOLD, MPL_DOUBLE, MPL_OCT };
static int melLast = 72, melI = 0;
static void play_mel_step(int step, int delay) {
    if (!von[V_ME] || !nbars) return;
    if (!bar_sounds(playSlot)) return;           // VOLTA — the bar rests this round
    int ml = locks_live(playSlot) ? arr[playSlot].mel : MPL_FOLLOW;    // VAR-gated
    if (ml == MPL_REST) return;                  // p-lock REST — silent this bar
    if (feel_stopped(playSlot, step)) return;    // STOP — nothing after the downbeat
    if (ml == MPL_HOLD) { if (step != 0) return; }              // HOLD fires once
    else if (ml != MPL_DOUBLE && MEL_PAT[modeSel][step] != 'x') return;   // DOUBLE fires every step
    int r0 = (arr[playSlot].rootPc + bar_tp(playSlot)) % 12, q = arr[playSlot].qual;
    int base = 72 + (octSel - 1) * 12 + (ml == MPL_OCT ? 12 : 0);
    int lo = base - 5, hi = base + 7;
    int cand[5], nc = 0, voices = bar_sev(playSlot) ? 4 : 3;
    for (int i = 0; i < voices; i++) cand[nc++] = (r0 + hb_tones[q][i]) % 12;
    cand[nc++] = (r0 + 2) % 12;
    int pc = cand[melI % nc]; melI++;
    melLast = rad_bass_to(pc, melLast, lo, hi);
    if (lit_voice() == V_ME) light_pc(melLast);
    int dur = ml == MPL_HOLD ? (int)(bar_frames() * (1000.0f / 60.0f)) : 0;   // one bar-long tone
    queue_dur(melLast, I_LEAD, feel_vel(playSlot, ml == MPL_ACCENT ? 5 : 4),
              delay + feel_delay_fr(playSlot), dur);
}

// ── pad (a real sustained BED, not a per-bar blip) ──────────────────────────
// The fix for "useless pads": HOLD the chord for the whole bar (hit() with a
// bar-length ring, so the tail overlaps the next bar = a legato wash) in a warm MID
// register — under the melody, above the bass — instead of one soft low blip.
// Timing is deliberately loose (a pad doesn't swing), so it ignores the step delay.
// per-CELL pad p-locks (arr[].pad; -1 = FOLLOW the lane switch): OFF · ON · SWELL
// (a slow crescendo across the bar — napoleon SERENADE's string swell, ridden live
// via note_on handles + note_vol, which are built to be ridden) · HIGH (bed +12).
enum { PPL_FOLLOW = -1, PPL_OFF = 0, PPL_ON = 1, PPL_SWELL, PPL_HIGH };
static int   swellH[4], nswellH = 0;             // the live swell's note handles
static float swellTgt = 3;                       // the velocity the swell rises to
static void kill_swell(void) { for (int i = 0; i < nswellH; i++) note_off(swellH[i]); nswellH = 0; }
static void play_pad(int delay) {
    (void)delay;
    kill_swell();                                // last bar's swell releases at the bar line
    if (!nbars) return;
    if (!bar_sounds(playSlot)) return;           // VOLTA — the bar rests this round
    int pl = locks_live(playSlot) ? arr[playSlot].pad : PPL_FOLLOW;    // VAR-gated
    int on = pl < 0 ? von[V_PA] : (pl != PPL_OFF);
    if (!on) return;
    if (bar_feel(playSlot) == FEEL_STOP) return; // a held bed defeats a band stop
    int r0 = (arr[playSlot].rootPc + bar_tp(playSlot)) % 12, q = arr[playSlot].qual;
    int base = 48 + (octSel - 1) * 12 + r0 + (pl == PPL_HIGH ? 12 : 0);   // mid — a chord-register bed
    int voices = bar_sev(playSlot) ? 4 : 3;
    int dur = (int)(bar_frames() * (1000.0f / 60.0f));   // sustain ~one bar; the release tail bleeds into the next
    int vel = feel_vel(playSlot, 3);
    for (int i = 0; i < voices; i++) {
        int m = base + hb_tones[q][i];
        if (lit_voice() == V_PA) light_pc(m);
        if (pl == PPL_SWELL) { if (nswellH < 4) swellH[nswellH++] = note_on(m, I_PAD, 0); }
        else hit(m, I_PAD, vel, dur);            // HELD (vs note()'s quick decay) — a proper pad
    }
    swellTgt = (float)vel;
}

// FEEL — the comp rhythm + per-genre swing (baked, no knob).
static const char *COMP_PAT[NMODE] = {
    /* BOSSA */ "x..x.x..", /* LOUNGE */ "x...x...", /* POP */ "x...x...",
    /* FOLK */ "x..x.x..", /* MINOR */ "x..x..x.", /* CINE */ "x.......",
    /* DORIAN */ "..x..x..", /* MIXO */ "x..x.x..", /* PHRYG */ "x.xx.x..",
    /* LYDIAN */ "x.......", /* BLUES */ "x...x...",
    /* JANGLE */ "x.x.x.x.", /* JINGLE */ "x..x.x..", /* NAPOLN */ "..x.x.x.",
    /* CITYPO */ "..x.x.x.",
};
static const float SWING[NMODE] = {
    0.12f, 0.60f, 0.00f, 0.10f, 0.00f, 0.00f, 0.38f, 0.08f, 0.00f, 0.00f, 0.66f,
    /* JANGLE */ 0.00f, /* JINGLE */ 0.30f, /* NAPOLN */ 0.14f, /* CITYPO */ 0.14f,
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
    b->strum = b->inv = b->oct = b->sev = b->comp = -1;
    b->bass = -1; b->fill = 0; b->mel = -1; b->pad = -1; b->feel = FEEL_STRAIGHT;
    b->volta = VLT_ALL;
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
// THE SONG FILE FORMAT (v1) — one serialized shape for a whole song: the header
// (key/mode/tempo/loop length), every voice's global setup, the lane switches,
// and all NBARS bars with their full p-lock set. 120 bytes, versioned. It is
// BOTH the save file (autosaved via save_bytes into build/saves/bandbox/) and
// the shape a built-in DEMO loads through — one format, two producers.
// Harness builds (DE_TRACE/DE_SPEC) never touch the disk copy: play.js/spec runs
// stay deterministic and the parked clips keep replaying from the doo-wop boot.
// ─────────────────────────────────────────────────────────────────────────
// v1 = 12 bytes/bar; v2 adds the VOLTA byte (13/bar). Unpack accepts BOTH — a v1
// blob loads with volta defaulting to ALL (the format-discipline this file exists for).
#define SONG_BYTES    (24 + NBARS * 13)
#define SONG_BYTES_V1 (24 + NBARS * 12)
static void song_pack(signed char *b) {
    b[0] = 'B'; b[1] = 'X'; b[2] = 2;                 // magic + version
    b[3] = (signed char)keyPc;   b[4] = (signed char)modeSel;
    b[5] = (signed char)(bpmN * 100.0f + 0.5f);       // tempo knob, 0..100
    b[6] = (signed char)nbars;
    b[7] = (signed char)strumSel; b[8] = (signed char)invSel;
    b[9] = (signed char)octSel;   b[10] = (signed char)seventh;
    b[11] = (signed char)bassSel; b[12] = (signed char)bassTone;
    b[13] = (signed char)chordTone; b[14] = (signed char)melTone; b[15] = (signed char)padTone;
    b[16] = (signed char)drumSel; b[17] = (signed char)kitSel; b[18] = (signed char)drumBusy;
    for (int v = 0; v < VOICES; v++) b[19 + v] = (signed char)von[v];
    for (int i = 0; i < NBARS; i++) {
        signed char *r = b + 24 + i * 13;
        r[0] = (signed char)arr[i].rootPc; r[1] = (signed char)arr[i].qual;
        r[2] = (signed char)arr[i].strum;  r[3] = (signed char)arr[i].inv;
        r[4] = (signed char)arr[i].oct;    r[5] = (signed char)arr[i].sev;
        r[6] = (signed char)arr[i].comp;   r[7] = (signed char)arr[i].bass;
        r[8] = (signed char)arr[i].fill;   r[9] = (signed char)arr[i].mel;
        r[10] = (signed char)arr[i].pad;   r[11] = (signed char)arr[i].feel;
        r[12] = (signed char)arr[i].volta;
    }
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int song_unpack(const signed char *b, int n) {
    if (b[0] != 'B' || b[1] != 'X') return 0;
    int ver = b[2], stride = ver == 1 ? 12 : 13;
    if (ver != 1 && ver != 2) return 0;
    if (n < (ver == 1 ? SONG_BYTES_V1 : SONG_BYTES)) return 0;
    keyPc = clampi(b[3], 0, 11);  modeSel = clampi(b[4], 0, NMODE - 1);
    bpmN = clampi(b[5], 0, 100) / 100.0f;
    nbars = clampi(b[6], 0, NBARS);
    strumSel = clampi(b[7], 0, NSTRUM - 1); invSel = clampi(b[8], 0, 3);
    octSel = clampi(b[9], 0, 2); seventh = b[10] ? 1 : 0;
    bassSel = clampi(b[11], 0, 3); bassTone = clampi(b[12], 0, 3);
    chordTone = clampi(b[13], 0, 2); melTone = clampi(b[14], 0, 3); padTone = clampi(b[15], 0, 2);
    drumSel = clampi(b[16], 0, 1); kitSel = clampi(b[17], 0, 1); drumBusy = clampi(b[18], 0, 2);
    for (int v = 0; v < VOICES; v++) von[v] = b[19 + v] ? 1 : 0;
    for (int i = 0; i < NBARS; i++) {
        const signed char *r = b + 24 + i * stride;
        arr[i].rootPc = clampi(r[0], 0, 11);
        arr[i].qual   = clampi(r[1], 0, HB_NQUAL - 1);
        arr[i].strum  = clampi(r[2], -1, NSTRUM - 1);
        arr[i].inv    = clampi(r[3], -1, 3);
        arr[i].oct    = clampi(r[4], -1, 2);
        arr[i].sev    = clampi(r[5], -1, 1);
        arr[i].comp   = clampi(r[6], -1, CPL_N - 1);
        arr[i].bass   = clampi(r[7], -1, BPL_PEDAL);
        arr[i].fill   = clampi(r[8], 0, DPL_DOUBLE);
        arr[i].mel    = clampi(r[9], -1, MPL_OCT);
        arr[i].pad    = clampi(r[10], -1, PPL_HIGH);
        arr[i].feel   = clampi(r[11], 0, NFEEL - 1);
        arr[i].volta  = ver >= 2 ? clampi(r[12], 0, VLT_N - 1) : VLT_ALL;
    }
    apply_chord_tone(); apply_bass_tone(); apply_mel_tone(); apply_pad_tone();
    dk_use(KITS[kitSel], 20);                         // set-and-hold: re-voice on load only
    playing = 0; selVoice = -1; picking = 0;
    rethink();
    return 1;
}
// AUTOSAVE — a checksum of the packed song, taken every frame; when it moves, the
// blob is written. Loads (boot / MINE / a demo) re-arm the checksum WITHOUT saving,
// so browsing demos never clobbers your song until you actually edit on top of one.
static unsigned songClean = 0;
static unsigned song_sum(void) {
    signed char b[SONG_BYTES]; song_pack(b);
    unsigned h = 2166136261u;
    for (int i = 0; i < SONG_BYTES; i++) { h ^= (unsigned char)b[i]; h *= 16777619u; }
    return h;
}
static void song_mark_clean(void) { songClean = song_sum(); }

// ── the built-in DEMOS — songs as DATA in the song format's shape. The first is
// the parked 02-citypop-royalroad clip transcribed (the citypop.c checklist:
// Royal Road x2, POP bass + disco hats into the turn, ANTIC pushes, +2 LIFT).
typedef struct {
    const char *name, *blurb;
    int mode, key, nbars, padOn;
    signed char root[NBARS], qual[NBARS], comp[NBARS], bass[NBARS],
                fill[NBARS], mel[NBARS], pad[NBARS], feel[NBARS], volta[NBARS];
} Demo;
static const Demo DEMOS[] = {
    { "CITY POP", "royal road x2 - the +2 LIFT",
      14, 0, 8, 1,
      { 5, 7, 4, 9, 5, 7, 4, 9 },
      { HBQ_MAJ7, HBQ_DOM7, HBQ_MIN7, HBQ_MIN7, HBQ_MAJ7, HBQ_DOM7, HBQ_MIN7, HBQ_MIN7 },
      { -1, -1, -1, CPL_ANTIC, -1, -1, -1, CPL_ANTIC },
      { -1, -1, -1, BPL_POP,   -1, -1, -1, BPL_POP },
      { 0, 0, 0, DPL_FILL, DPL_OPEN, DPL_OPEN, DPL_DOUBLE, DPL_FILL },
      { -1, -1, -1, -1, -1, -1, -1, MPL_OCT },
      { -1, -1, -1, PPL_SWELL, -1, -1, -1, -1 },
      { 0, 0, 0, 0, FEEL_LIFT, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { "BOSSA", "the early change + lead-in",
      0, 0, 4, 0,
      { 0, 9, 5, 7, 0, 0, 0, 0 },
      { HBQ_MAJ7, HBQ_MIN7, HBQ_MAJ7, HBQ_DOM7, 0, 0, 0, 0 },
      { -1, -1, -1, CPL_ANTIC, -1, -1, -1, -1 },
      { -1, BPL_APPR, -1, BPL_APPR, -1, -1, -1, -1 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { -1, -1, -1, -1, -1, -1, -1, -1 },
      { -1, -1, -1, -1, -1, -1, -1, -1 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0 } },
    // the disco-funk archetype (napoleon.c DANCE): a mixolydian boogie vamp
    // (I7 I7 bVII IV — the Sweet Home cadence as the loop wraps), STAB clav chucks
    // on the vamp, POP runs on the changes, DOUBLE heat into a BUILD riser, ANTIC
    // pushing the restart. VOLTA makes it a 4-round FORM: the STOP break bar is
    // VAR2 (plays plain on odd rounds, breaks on even) and the BUILD turn is VAR4
    // (plain groove three rounds, the big turn every 4th).
    { "NAPOLEON", "boogie vamp - break + BUILD",
      13, 0, 8, 0,
      { 0, 0, 10, 5, 0, 0, 10, 5 },
      { HBQ_DOM7, HBQ_DOM7, HBQ_MAJ7, HBQ_MAJ7, HBQ_DOM7, HBQ_DOM7, HBQ_MAJ7, HBQ_MAJ7 },
      { CPL_STAB, CPL_STAB, -1, -1, CPL_STAB, CPL_STAB, -1, CPL_ANTIC },
      { -1, -1, BPL_POP, -1, -1, -1, BPL_POP, BPL_FILL },
      { DPL_CRASH, 0, 0, 0, DPL_CRASH, 0, DPL_DOUBLE, DPL_BUILD },
      { MPL_REST, MPL_REST, MPL_ACCENT, -1, -1, -1, MPL_OCT, -1 },
      { -1, -1, -1, -1, -1, -1, -1, -1 },
      { 0, 0, 0, FEEL_STOP, FEEL_ACCENT, 0, 0, 0 },
      { 0, 0, 0, VLT_VAR2, 0, 0, 0, VLT_VAR4 } },
};
#define NDEMO ((int)(sizeof DEMOS / sizeof DEMOS[0]))
static void demo_load(const Demo *d) {
    playing = 0; selVoice = -1; picking = 0;
    keyPc = d->key; modeSel = d->mode; nbars = d->nbars;
    von[V_PA] = d->padOn;
    for (int i = 0; i < NBARS; i++) {
        bar_defaults(&arr[i]);
        if (i >= d->nbars) continue;
        arr[i].rootPc = d->root[i]; arr[i].qual = d->qual[i];
        arr[i].comp = d->comp[i];   arr[i].bass = d->bass[i];
        arr[i].fill = d->fill[i];   arr[i].mel  = d->mel[i];
        arr[i].pad  = d->pad[i];    arr[i].feel = d->feel[i];
        arr[i].volta = d->volta[i];
    }
    rethink();
    song_mark_clean();                                // a demo is not an edit
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
    // SONG (top-left) — the songs popup: built-in demos + your autosaved song
    Box sng = lay_split(b, EDGE_LEFT, 21, &b);
    if (seg(sng, "SONG", songsOn, 0x2108)) {
        songsOn = !songsOn; selVoice = -1; picking = 0; helpOn = 0;
        if (songsOn) {                                // probe MINE once, on open
#if !defined(DE_TRACE) && !defined(DE_SPEC)           // harness runs never see the disk
            signed char probe[SONG_BYTES];            // copy — clips stay deterministic
            int pn = load_bytes(probe, SONG_BYTES);
            haveSave = pn >= SONG_BYTES_V1
                       && probe[0] == 'B' && probe[1] == 'X' && (probe[2] == 1 || probe[2] == 2);
#else
            haveSave = 0;
#endif
        }
    }
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

// the SONGS popup, drawn ON the glass (the same morph as help/picker): your
// autosaved song (MINE) + the built-in demos. Picking one loads it and closes.
// Loads are not edits — MINE survives demo browsing until you edit on top.
static void glass_songs(Box g) {
    Box top = lay_split(g, EDGE_TOP, 9, &g);
    print("SONGS", (int)top.x + 1, (int)(top.y + 2), CLR_WHITE);
    Box back = lay_split(top, EDGE_RIGHT, 30, &top);
    if (seg(back, "BACK", 0, 0x2900)) songsOn = 0;
    Box p = lay_inset(g, 1);
    for (int i = 0; i < NDEMO + 1; i++) {             // row 0 = MINE, then the demos
        Box row = lay_split(p, EDGE_TOP, 12, &p);     // fixed-height rows (4 fit; a
        int mine = i == 0, on = !mine || haveSave;    // longer shelf needs paging later)
        const char *nm = mine ? "MINE" : DEMOS[i - 1].name;
        const char *bl = mine ? (haveSave ? "your autosaved song" : "nothing saved yet")
                              : DEMOS[i - 1].blurb;
        Box r = lay_inset(row, 1);
        rrectfill((int)r.x, (int)r.y, (int)r.w, (int)r.h, 2, on ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
        print(nm, (int)r.x + 3, (int)r.y,       on ? CLR_ORANGE : CLR_DARK_GREY);
        print(bl, (int)r.x + 3, (int)(r.y + 5), on ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        if (on && tapped(r, 0x2910u + i)) {
            if (mine) {
#if !defined(DE_TRACE) && !defined(DE_SPEC)
                signed char b[SONG_BYTES];
                int ln = load_bytes(b, SONG_BYTES);
                if (ln >= SONG_BYTES_V1 && song_unpack(b, ln)) song_mark_clean();
#endif
            } else demo_load(&DEMOS[i - 1]);
            songsOn = 0;
        }
    }
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
        case V_CH: return arr[i].strum >= 0 || arr[i].inv >= 0 || arr[i].oct >= 0 || arr[i].sev >= 0 || arr[i].comp >= 0;
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
            // VOLTA, live: a bar resting THIS round dims; a VAR bar on a plain
            // round greys its lock pips (the locks aren't live right now).
            int resting = playing && on && !bar_sounds(i);
            int varOff  = playing && on && !locks_live(i);
            rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1,
                      on ? (resting ? CLR_BROWNISH_BLACK : (cur ? CLR_DARK_BLUE : CLR_DARKER_BLUE))
                         : CLR_BROWNISH_BLACK);
            if (v == V_CH && on) {                 // chord — the numeral
                const char *rn = pfn[i] >= 0 ? cur_vocab()->fname[pfn[i]] : "?";
                print(rn, (int)(c.x + c.w / 2 - text_width(rn) / 2), (int)(c.y + c.h / 2 - 2),
                      pfn[i] < 0 ? CLR_RED : (resting ? CLR_DARK_GREY : (cur ? CLR_WHITE : CLR_YELLOW)));
            } else if (v == V_CH && i == nbars) {  // the "add a bar" slot (bright when empty)
                print("+", (int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 2),
                      nbars == 0 ? CLR_LIME_GREEN : CLR_DARK_GREY);
            } else if (lit && on) {                // other voices — a pip
                int lk = cell_locked(v, i);
                rectfill((int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 1), 3, 3,
                         resting ? CLR_DARKER_GREY
                                 : (lk ? (varOff ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_MEDIUM_GREY));
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

    // right: 2x3 grid of per-cell p-lock chips (AUTO = follow the global voicing).
    // COMP is the bar's comp texture (ANTIC/STAB/HELD/TACET — the radio-idiom locks).
    char v0[8], v1[8], v2[8], v3[8], v4[8];
    Bar *b = &arr[selBar];
    snprintf(v0, sizeof v0, "%s", b->strum < 0 ? "AUTO" : STRUMS[b->strum].name);
    snprintf(v1, sizeof v1, "%s", b->inv   < 0 ? "AUTO" : INV_LAB[b->inv]);
    snprintf(v2, sizeof v2, "%s", b->oct   < 0 ? "AUTO" : OCT_LAB[b->oct]);
    snprintf(v3, sizeof v3, "%s", b->sev   < 0 ? "AUTO" : (b->sev ? "7TH" : "TRIAD"));
    snprintf(v4, sizeof v4, "%s", b->comp  < 0 ? "AUTO" : CPL_LAB[b->comp]);
    Box p = lay_inset(g, 1);
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    Box a = lay_grid(r0, 3, 3, 0, 1), bb = lay_grid(r0, 3, 3, 1, 1), c = lay_grid(r0, 3, 3, 2, 1);
    Box d = lay_grid(r1, 3, 3, 0, 1), e  = lay_grid(r1, 3, 3, 1, 1);
    if (chip(a,  "STRUM", v0, b->strum >= 0, 0x2420)) b->strum = (b->strum + 2) % (NSTRUM + 1) - 1;
    if (chip(bb, "INV",   v1, b->inv   >= 0, 0x2421)) b->inv   = (b->inv   + 2) % ((bar_sev(selBar) ? 4 : 3) + 1) - 1;
    if (chip(c,  "OCT",   v2, b->oct   >= 0, 0x2422)) b->oct   = (b->oct   + 2) % 4 - 1;
    if (chip(d,  "7TH",   v3, b->sev   >= 0, 0x2423)) b->sev   = (b->sev   + 2) % 3 - 1;
    if (chip(e,  "COMP",  v4, b->comp  >= 0, 0x2425)) b->comp  = (b->comp  + 2) % (CPL_N + 1) - 1;
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
    for (int i = 0; i < 5; i++) {
        if (seg(lay_grid(r0, 5, 5, i, 1), BPL_LAB[i],     bl == BPL_VAL[i],     0x2431u + i))
            arr[selBar].bass = BPL_VAL[i];
        if (seg(lay_grid(r1, 5, 5, i, 1), BPL_LAB[i + 5], bl == BPL_VAL[i + 5], 0x24B0u + i))
            arr[selBar].bass = BPL_VAL[i + 5];
    }
}
// DRUMS editor: global STYLE + KIT + BUSY chips, then the per-cell vocab (2x3):
// GROOVE / DROP / KICK / FILL / CRASH / BUILD.
static void editor_drums(Box g) {
    Box styRow = lay_split(g, EDGE_TOP, 13, &g);
    Box styB = lay_grid(styRow, 3, 3, 0, 1), kitB = lay_grid(styRow, 3, 3, 1, 1), busB = lay_grid(styRow, 3, 3, 2, 1);
    if (chip(lay_inset(styB, 1), "STYLE", DRUM_LAB[drumSel], 1, 0x2440)) drumSel = (drumSel + 1) % 2;
    if (chip(lay_inset(kitB, 1), "KIT", KIT_LAB[kitSel], 1, 0x2443)) {
        kitSel = !kitSel; dk_use(KITS[kitSel], 20);   // set-and-hold: swap only on tap
    }
    if (chip(lay_inset(busB, 1), "BUSY", BUSY_LAB[drumBusy], 1, 0x2450)) drumBusy = (drumBusy + 1) % 3;
    Box p = lay_inset(g, 1);
    int dp = arr[selBar].fill;
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    for (int i = 0; i < 5; i++) {
        if (seg(lay_grid(r0, 5, 5, i, 1), DPL_LAB[i],     dp == i,     0x2444u + i)) arr[selBar].fill = i;
        if (seg(lay_grid(r1, 5, 5, i, 1), DPL_LAB[i + 5], dp == i + 5, 0x24C0u + i)) arr[selBar].fill = i + 5;
    }
}
static void editor_mel(Box g) {
    Box tRow = lay_split(g, EDGE_TOP, 13, &g);
    if (chip(lay_inset(tRow, 1), "TONE", MTONE_LAB[melTone], 1, 0x2454)) {
        melTone = (melTone + 1) % 4; apply_mel_tone();
    }
    Box p = lay_inset(g, 1);
    int ml = arr[selBar].mel;
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    if (seg(lay_grid(r0, 3, 3, 0, 1), "FOLLOW", ml < 0,             0x2451)) arr[selBar].mel = MPL_FOLLOW;
    if (seg(lay_grid(r0, 3, 3, 1, 1), "REST",   ml == MPL_REST,     0x2452)) arr[selBar].mel = MPL_REST;
    if (seg(lay_grid(r0, 3, 3, 2, 1), "ACCENT", ml == MPL_ACCENT,   0x2453)) arr[selBar].mel = MPL_ACCENT;
    if (seg(lay_grid(r1, 3, 3, 0, 1), "HOLD",   ml == MPL_HOLD,     0x2455)) arr[selBar].mel = MPL_HOLD;
    if (seg(lay_grid(r1, 3, 3, 1, 1), "DOUBLE", ml == MPL_DOUBLE,   0x2456)) arr[selBar].mel = MPL_DOUBLE;
    if (seg(lay_grid(r1, 3, 3, 2, 1), "OCT+",   ml == MPL_OCT,      0x2457)) arr[selBar].mel = MPL_OCT;
}
static void editor_pad(Box g) {
    Box tRow = lay_split(g, EDGE_TOP, 13, &g);
    if (chip(lay_inset(tRow, 1), "TONE", PTONE_LAB[padTone], 1, 0x2464)) {
        padTone = (padTone + 1) % 3; apply_pad_tone();
    }
    Box p = lay_inset(g, 1);
    int pl = arr[selBar].pad;
    Box r0 = lay_grid(p, 1, 2, 0, 1), r1 = lay_grid(p, 1, 2, 1, 1);
    if (seg(lay_grid(r0, 3, 3, 0, 1), "FOLLOW", pl < 0,            0x2461)) arr[selBar].pad = PPL_FOLLOW;
    if (seg(lay_grid(r0, 3, 3, 1, 1), "OFF",    pl == PPL_OFF,     0x2462)) arr[selBar].pad = PPL_OFF;
    if (seg(lay_grid(r0, 3, 3, 2, 1), "ON",     pl == PPL_ON,      0x2463)) arr[selBar].pad = PPL_ON;
    if (seg(lay_grid(r1, 3, 3, 0, 1), "SWELL",  pl == PPL_SWELL,   0x2465)) arr[selBar].pad = PPL_SWELL;
    if (seg(lay_grid(r1, 3, 3, 1, 1), "HIGH",   pl == PPL_HIGH,    0x2466)) arr[selBar].pad = PPL_HIGH;
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
    // FEEL + VOLTA are per-BAR (the whole band's), so they live in the bar header —
    // set from any voice's editor; anything non-default shows orange.
    Box fbox = lay_split(hdr, EDGE_RIGHT, 40, &hdr);
    if (seg(fbox, FEEL_LAB[arr[selBar].feel], arr[selBar].feel != FEEL_STRAIGHT, 0x2480))
        arr[selBar].feel = (arr[selBar].feel + 1) % NFEEL;
    Box vbox = lay_split(hdr, EDGE_RIGHT, 26, &hdr);
    if (seg(vbox, VLT_LAB[arr[selBar].volta], arr[selBar].volta != VLT_ALL, 0x2481))
        arr[selBar].volta = (arr[selBar].volta + 1) % VLT_N;
    char nm[16]; chname(nm, sizeof nm, arr[selBar].rootPc, arr[selBar].qual, bar_sev(selBar));
    const char *rn = pfn[selBar] >= 0 ? cur_vocab()->fname[pfn[selBar]] : "?";
    char t[28]; snprintf(t, sizeof t, "B%d %s %s", selBar + 1, nm, rn);
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
#if !defined(DE_TRACE) && !defined(DE_SPEC)
    // resume your autosaved song if one exists; else the doo-wop cold open. Harness
    // builds always cold-open — the parked clips + spec stay deterministic.
    { signed char b[SONG_BYTES];
      int ln = load_bytes(b, SONG_BYTES);
      if (!(ln >= SONG_BYTES_V1 && song_unpack(b, ln))) seed_demo(); }
#else
    seed_demo();
#endif
    song_mark_clean();                               // booting is not an edit
}

void update(void) {
    // keyboard convenience (the touch UI is the real interface)
    if (keyp(KEY_LEFT))  change_key(-1);
    if (keyp(KEY_RIGHT)) change_key(+1);
    if (keyp('B'))       { modeSel = (modeSel + 1) % NMODE; rethink(); }
    if (keyp(KEY_BACKSPACE)) { selVoice = -1; picking = 0; songsOn = 0; }
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
                int cpl = locks_live(b0) ? arr[b0].comp : -1;   // VAR-gated
                int tp0 = bar_tp(b0);
                // an ANTIC bar TIES over the barline: it already stated this bar's
                // chord on its last 8th, so the downbeat comp hit is skipped — else
                // two full strikes land a half-beat apart (the bossa.c rule; only
                // audible in genres whose comp pattern hits the downbeat, e.g. BOSSA).
                // The prev bar played in the PREVIOUS round when b0 == 0 — its ANTIC
                // only fired if it sounded + its locks were live back THEN.
                int pb = (b0 + nbars - 1) % nbars;
                int pr = b0 == 0 ? ((passRound + 3) & 3) + 1 : cur_round();
                int prevAntic = nbars > 0
                    && arr[pb].comp == CPL_ANTIC
                    && bar_sounds_at(pb, pr) && locks_live_at(pb, pr)
                    && arr[pb].feel != FEEL_STOP;                 // a STOPped ANTIC never fired
                if (von[V_CH] && bar_sounds(b0) && cpl != CPL_TACET && !feel_stopped(b0, s)
                    && !(s == 0 && prevAntic)
                    && COMP_PAT[modeSel][s] == 'x') {   // the chords lane can be muted too
                    // STAB chokes the hit short; HELD rings it to the bar line; else
                    // the instrument's own envelope (dur 0).
                    int dur = cpl == CPL_STAB ? 60
                            : cpl == CPL_HELD ? (int)((8 - s) * stepLen * (1000.0f / 60.0f)) : 0;
                    sound_chord((r0 + tp0) % 12, q, 0, sw + feel_delay_fr(b0), bar_strum(b0), bar_inv(b0),
                                bar_oct(b0), bar_sev(b0), feel_vel(b0, 5), dur);
                    if (lit_voice() == V_CH) {   // light the chord tones as it comps
                        int nv = bar_sev(b0) ? 4 : 3;
                        for (int i = 0; i < nv; i++) light_pc(r0 + tp0 + hb_tones[q][i]);
                    }
                }
                // ANTIC — the anticipation: the last 8th of THIS bar already strikes the
                // NEXT bar's chord (its own resolved voicing). Forward-only, no lookahead.
                if (von[V_CH] && bar_sounds(b0) && cpl == CPL_ANTIC && s == 7 && !feel_stopped(b0, s)) {
                    int nb = (b0 + 1) % nbars;
                    int rA = (arr[nb].rootPc + bar_tp(nb)) % 12, qA = arr[nb].qual;
                    sound_chord(rA, qA, 0, feel_delay_fr(b0), bar_strum(nb), bar_inv(nb),
                                bar_oct(nb), bar_sev(nb), feel_vel(b0, 5), 0);
                    if (lit_voice() == V_CH) {
                        int nv = bar_sev(nb) ? 4 : 3;
                        for (int i = 0; i < nv; i++) light_pc(rA + hb_tones[qA][i]);
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
        if (++playT >= bf) { playT = 0; playSlot = (playSlot + 1) % nbars;
                             if (playSlot == 0) passRound++; }   // a new ROUND at each wrap
        // PAD SWELL — ride the held notes' volume up across the bar (note_vol is
        // built to be ridden live; the handles die at the next bar line). Skip the
        // wrap frame (playT 0) so the peak holds until note_off's release tail —
        // the swell bleeds into the next bar like the normal pad does.
        if (nswellH && playT > 0) {
            float v = swellTgt * (float)playT / (float)bar_frames();
            for (int i = 0; i < nswellH; i++) note_vol(swellH[i], v);
        }
    } else { playSlot = 0; passRound = 0; if (nswellH) kill_swell(); }

    for (int i = 0; i < 12; i++) keyLit[i] *= 0.86f;   // the note-lights fade out
    pump_notes();

#if !defined(DE_TRACE) && !defined(DE_SPEC)
    // AUTOSAVE — any edit moves the song checksum; write the v1 blob when it does.
    // Loads re-arm the checksum without writing, so demo browsing never clobbers MINE.
    { unsigned s = song_sum();
      if (s != songClean) { signed char b[SONG_BYTES]; song_pack(b); save_bytes(b, SONG_BYTES); songClean = s; } }
#endif
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
    else if (songsOn)        glass_songs(g);
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
    expect_eq(drumBusy, 1, "drums default to NORMAL busy (the Apple-Drummer complexity axis)");

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

    // the radio-idiom FEELs (2026-07-22): HUSH pulls down, STOP accents the lone hit
    // then silences, LIFT transposes from its bar to the loop's end (and only there)
    arr[1].feel = FEEL_HUSH;  expect_eq(feel_vel(1, 5), 3, "HUSH pulls the band down two");
    expect_eq(feel_vel(1, 2), 1, "HUSH clamps at 1 (never silent)");
    arr[1].feel = FEEL_STOP;  expect_eq(feel_vel(1, 5), 7, "STOP's downbeat hit is accented");
    expect(!feel_stopped(1, 0) && feel_stopped(1, 3), "STOP silences after the downbeat only");
    arr[1].feel = FEEL_STRAIGHT;
    expect_eq(bar_tp(nbars - 1), 0, "no LIFT = no transpose");
    arr[2].feel = FEEL_LIFT;
    expect(bar_tp(1) == 0 && bar_tp(2) == 2 && bar_tp(3) == 2,
           "LIFT transposes from its bar to the loop end (the gear change), not before");
    arr[2].feel = FEEL_STRAIGHT;

    // the radio-idiom per-cell locks read as p-locks (orange pip) and reset clean
    expect(!cell_locked(V_CH, 0), "chord cell starts unlocked");
    arr[0].comp = CPL_ANTIC; expect(cell_locked(V_CH, 0), "COMP ANTIC reads as a p-lock");
    arr[0].comp = CPL_TACET; expect_eq(arr[0].comp, CPL_TACET, "COMP TACET sets");
    arr[0].comp = -1;        expect(!cell_locked(V_CH, 0), "COMP back to AUTO unlocks");
    arr[0].bass = BPL_APPR;  expect(cell_locked(V_BA, 0), "bass APPR reads as a p-lock");
    arr[0].bass = BPL_PEDAL; expect_eq(arr[0].bass, BPL_PEDAL, "bass PEDAL sets");
    arr[0].bass = BPL_FOLLOW;
    arr[3].fill = DPL_HALF;  expect(cell_locked(V_DR, 3), "drum HALF reads as a p-lock");
    arr[3].fill = DPL_DOUBLE; expect_eq(arr[3].fill, DPL_DOUBLE, "drum DOUBLE sets");
    arr[3].fill = DPL_GROOVE;
    arr[0].mel = MPL_HOLD;   expect(cell_locked(V_ME, 0), "mel HOLD reads as a p-lock");
    arr[0].mel = MPL_FOLLOW;
    arr[0].pad = PPL_SWELL;  expect(cell_locked(V_PA, 0), "pad SWELL reads as a p-lock");
    arr[0].pad = PPL_FOLLOW;

    // P-LOCK ISOLATION — every per-cell p-lock set on bar 0 must leave bar 1 untouched
    // (the guard against a p-lock leaking to the whole instrument).
    seed_demo();   // 4 fresh bars, all AUTO/default
    arr[0].strum = 0; arr[0].inv = 1; arr[0].oct = 0; arr[0].sev = 0;
    expect(arr[1].strum < 0 && arr[1].inv < 0 && arr[1].oct < 0 && arr[1].sev < 0,
           "chord voicing p-locks are bar-local (bar 1 still AUTO)");
    arr[0].bass = BPL_MUTE;   expect(arr[1].bass < 0,            "bass p-lock is bar-local");
    arr[0].fill = DPL_FILL;   expect_eq(arr[1].fill, DPL_GROOVE, "drum p-lock is bar-local");
    arr[0].mel  = 0;          expect(arr[1].mel  < 0,            "mel p-lock is bar-local");
    arr[0].pad  = 1;          expect(arr[1].pad  < 0,            "pad p-lock is bar-local");
    arr[0].feel = FEEL_ACCENT; expect_eq(arr[1].feel, FEEL_STRAIGHT, "feel p-lock is bar-local");
    seed_demo();   // clear the probes

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

    // the radio-idea genres (jangle/jingle/napoleon/citypop) — appended, analyze on their vocab
    expect_eq(NMODE, 15, "four radio-idea genres added");
    modeSel = 11; keyPc = 0; seed_demo();   // JANGLE rides the POP vocab
    expect(pfn[0] == HB_I && pfn[3] == HB_V, "jangle reads the doo-wop I .. V (pop vocab)");

    // JINGLE has its OWN harmony now (Mac DeMarco's gravity): from I it leans bVII7
    modeSel = 12; keyPc = 0; nbars = 1; bar_defaults(&arr[0]);
    arr[0].rootPc = 0; arr[0].qual = HBQ_MAJ7; rethink();
    expect(pfn[0] == HB_I, "jingle: Cmaj7 = I");
    expect(nsugg >= 1 && sugg[0].f == HB_bVII7, "jingle from I leans bVII7 (Mac's flat-7 home)");

    // CITYPOP has its own gravity: the Royal-Road tell is V -> iii (not V -> I)
    modeSel = 14; keyPc = 0; nbars = 1; bar_defaults(&arr[0]);
    arr[0].rootPc = 7; arr[0].qual = HBQ_DOM7; rethink();   // G7 = V in C
    expect(pfn[0] == HB_V, "citypop: G7 = V");
    expect(nsugg >= 1 && sugg[0].f == HB_iii, "citypop from V leans iii (the Royal Road tell)");

    // VOLTA — the round gates are pure functions of (volta, round)
    seed_demo();
    arr[1].volta = VLT_EVEN; arr[2].volta = VLT_4TH; arr[3].volta = VLT_VAR2;
    expect(bar_sounds_at(1, 1) == 0 && bar_sounds_at(1, 2) == 1 && bar_sounds_at(1, 4) == 1,
           "EVEN sounds rounds 2+4 only");
    expect(bar_sounds_at(2, 3) == 0 && bar_sounds_at(2, 4) == 1, "4TH sounds round 4 only");
    expect(bar_sounds_at(3, 1) == 1 && locks_live_at(3, 1) == 0 && locks_live_at(3, 2) == 1,
           "VAR2 always sounds; its locks live on even rounds only");
    // a VAR bar's locks read as AUTO on its plain rounds (strum + feel checked)
    strumSel = 2; arr[3].strum = 0; arr[3].feel = FEEL_ACCENT;
    passRound = 0;   // round 1 — plain
    expect(bar_strum(3) == 2 && feel_vel(3, 5) == 5, "VAR2 round 1: lock + feel dormant (AUTO)");
    passRound = 1;   // round 2 — live
    expect(bar_strum(3) == 0 && feel_vel(3, 5) == 7, "VAR2 round 2: lock + feel engage");
    passRound = 0; strumSel = 0;

    // SONG format v2 (the save file + the demo shape): pack -> wreck -> unpack restores
    modeSel = 14; keyPc = 3; seed_demo();
    arr[1].bass = BPL_POP; arr[2].feel = FEEL_LIFT; arr[0].comp = CPL_STAB;
    arr[1].volta = VLT_VAR4; von[V_PA] = 1;
    { signed char sb[SONG_BYTES], sb2[SONG_BYTES], v1[SONG_BYTES_V1];
      song_pack(sb);
      modeSel = 0; keyPc = 0; new_song(); von[V_PA] = 0;
      expect(song_unpack(sb, SONG_BYTES), "a v2 blob unpacks (magic + version accepted)");
      expect(modeSel == 14 && keyPc == 3 && nbars == 4, "unpack restores key/mode/loop length");
      expect(arr[1].bass == BPL_POP && arr[2].feel == FEEL_LIFT && arr[0].comp == CPL_STAB
             && arr[1].volta == VLT_VAR4 && von[V_PA] == 1,
             "unpack restores p-locks + volta + lane switches");
      song_pack(sb2);
      expect(memcmp(sb, sb2, SONG_BYTES) == 0, "pack(unpack(b)) == b (the format is stable)");
      // a v1 blob (12-byte bars, no volta) still loads — volta defaults to ALL
      memcpy(v1, sb, 24); v1[2] = 1;
      for (int i = 0; i < NBARS; i++) memcpy(v1 + 24 + i * 12, sb + 24 + i * 13, 12);
      modeSel = 0; keyPc = 0; new_song();
      expect(song_unpack(v1, SONG_BYTES_V1), "a v1 blob (pre-volta) still loads");
      expect(arr[1].bass == BPL_POP && arr[1].volta == VLT_ALL,
             "v1 load keeps the locks, defaults volta to ALL");
      sb[2] = 99;
      expect(!song_unpack(sb, SONG_BYTES), "an unknown version is refused, not misread"); }

    // the built-in demos load through the same shape
    demo_load(&DEMOS[0]);
    expect(modeSel == 14 && nbars == 8 && von[V_PA] == 1, "CITY POP demo: CITYPO, 8 bars, pad on");
    expect(arr[3].bass == BPL_POP && arr[3].comp == CPL_ANTIC && arr[4].feel == FEEL_LIFT
           && arr[6].fill == DPL_DOUBLE && arr[7].mel == MPL_OCT && arr[3].pad == PPL_SWELL,
           "CITY POP demo carries the parked clip's locks (POP/ANTIC/LIFT/DOUBLE/OCT+/SWELL)");
    expect(pfn[0] == HB_IV && pfn[1] == HB_V && pfn[2] == HB_iii && pfn[3] == HB_vi,
           "CITY POP demo analyzes as the Royal Road (IV V iii vi)");
    demo_load(&DEMOS[1]);
    expect(modeSel == 0 && nbars == 4 && arr[1].bass == BPL_APPR && arr[3].comp == CPL_ANTIC,
           "BOSSA demo: the early change + the chromatic lead-in");
    demo_load(&DEMOS[2]);
    expect(modeSel == 13 && nbars == 8 && arr[3].feel == FEEL_STOP && arr[4].feel == FEEL_ACCENT
           && arr[0].comp == CPL_STAB && arr[7].fill == DPL_BUILD && arr[2].bass == BPL_POP,
           "NAPOLEON demo: the break + accented re-entry + the boogie locks");
    expect(arr[3].volta == VLT_VAR2 && arr[7].volta == VLT_VAR4,
           "NAPOLEON demo is a 4-round form (break every 2nd, the big turn every 4th)");
    expect(pfn[0] == HBmx_I && pfn[2] == HBmx_bVII && pfn[3] == HBmx_IV,
           "NAPOLEON demo analyzes as I / bVII / IV (the mixolydian cadence)");
    von[V_PA] = 0;

    // back to a clean state
    modeSel = 0; keyPc = 0; seed_demo();
    expect_eq(nbars, 4, "reset to the cold-open");
}
#endif
