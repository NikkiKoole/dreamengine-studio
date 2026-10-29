/* de:meta
{
  "slug": "singsample",
  "title": "singsample",
  "status": "active",
  "created": "2026-10-29",
  "kind": ["probe", "tech-demo"],
  "genre": null,
  "teaches": ["granular-synth"],
  "lineage": "The showcase + acceptance probe for INSTR_SING (GitHub #44's real-time ask): one recorded syllable sings a tune. Built for Playtime's Mipo voices, where each character speaks with short recorded hums. Sibling of voxsize (the offline sample_formant face).",
  "description": {
    "summary": "One recorded 'mi' sings a whole tune, as a big and a small creature.",
    "detail": "The console says 'mi' once (INSTR_VOICE with an m onset, captured off its own output with record_arm/record_grab), and that single take is bound to two INSTR_SING instruments: a BIG one (timbre 0.3, a larger throat) and a SMALL one (timbre 0.72). They trade a little pentatonic tune. Every note is the same sample: the note sets the pitch, the size knob sets who is singing, and the syllable keeps its own length, so it never turns into a chipmunk or a slowed tape. Long notes hold the vowel ('mi-i-i'). Build with DE_DEFINES=SWEEP for the probe: held notes chromatically from an octave below the recording to an octave above, for measuring tuning and artifacts at every interval.",
    "controls": "It plays on its own. SPACE starts the song again. Z / X: sing one note as the big / small one (hold to sustain)."
  }
}
de:meta */
//
// singsample — can ONE recorded syllable sing a tune and stay the same speaker? The probe for INSTR_SING.
//
// The source is the console's own INSTR_VOICE saying "mi", captured with record_arm/record_grab, so there
// is no mic and a play.js --wav render is deterministic. The SING instruments are bound to the sample
// slot BEFORE the capture lands, on purpose: that exercises the re-analysis path (record_grab replaces
// the take under a SING binding), not only the bind-after-load one.
//
// SWEEP (DE_DEFINES=SWEEP) is the measuring mode: note k (0..24) plays MIDI 45+k (A2..A4, the take is
// A3) at the original size, held SWEEP_HOLD frames, starting at frame SWEEP_AT + k*SWEEP_GAP. A note's
// steady part is from +0.25 s (past the "m") to the end of the hold.
// SWEEP_SIZE=<f> runs the sweep at another size; SWEEP_VOX plays the same notes on INSTR_VOICE itself, the
// synth reference (its level curve does NOT match SING's, so SING's loudness is held by a follower instead).

#include "studio.h"
#include "cursor.h"

#define VOX     5          // the source voice
#define BIG     6          // INSTR_SING, a bigger throat
#define SMALL   7          // INSTR_SING, a smaller one
#define SRC_MIDI 57        // A3: the take's pitch

#define CAP_UNTIL 52       // frame the take is grabbed
#define SONG_AT   80

#ifdef SWEEP
#define SWEEP_AT   80
#define SWEEP_GAP  60
#define SWEEP_HOLD 54
#define SWEEP_N    25
#endif

static int tick = 0, grabbed = 0, src_h = -1;
static int song_t = -1, song_i = 0;
static int big_h = -1, small_h = -1;      // what each one is singing (for the mouths)
static int big_on = 0, small_on = 0;      // frames left with the mouth open
#define PEAK_COLS 96
static float peaks_lo[PEAK_COLS], peaks_hi[PEAK_COLS];
static int peaks_n = 0;                   // sample_peaks returns the take's LENGTH (0 = empty), not a column count

// the tune: who sings (0 big, 1 small), MIDI note, length in frames. A minor pentatonic around the take.
typedef struct { int who, note, len; } Step;
static const Step SONG[] = {
    {0, 57, 14}, {0, 60, 14}, {0, 62, 14}, {0, 64, 40},
    {1, 69, 14}, {1, 67, 14}, {1, 64, 14}, {1, 62, 40},
    {0, 60, 14}, {0, 62, 14}, {1, 64, 14}, {1, 67, 14},
    {0, 57, 28}, {1, 69, 28}, {0, 45, 30}, {1, 81, 50},
};
#define SONG_N ((int)(sizeof(SONG) / sizeof(SONG[0])))

static int sing(int who, int note) {
    int h = note_on(note, who ? SMALL : BIG, 6);
    if (who) { small_h = h; small_on = 1; } else { big_h = h; big_on = 1; }
    return h;
}

void init(void) {
    instrument(VOX, INSTR_VOICE, 8, 200, 7, 120);
    instrument_harmonics(VOX, 0.95f);       // an "ee"
    instrument_timbre(VOX, 0.5f);
    instrument_morph(VOX, 0.5f);
    instrument_level(VOX, 0.8f);
    instrument(BIG, INSTR_SING, 4, 10, 7, 90);
    instrument_sample(BIG, 0, SRC_MIDI);
    instrument_timbre(BIG, 0.30f);
    instrument(SMALL, INSTR_SING, 4, 10, 7, 90);
    instrument_sample(SMALL, 0, SRC_MIDI);
    instrument_timbre(SMALL, 0.72f);
#ifdef SWEEP
#ifndef SWEEP_SIZE
#define SWEEP_SIZE 0.5f                     // the probe measures the recording's own size (DE_DEFINES=SWEEP_SIZE=0.3f)
#endif
    instrument_timbre(BIG, SWEEP_SIZE);
#endif
    record_arm();
}

static void restart_song(void) { song_t = 0; song_i = 0; }

void update(void) {
    tick++;
    if (tick == 2) {                        // say "mi" once
        src_h = note_on(SRC_MIDI, VOX, 6);
        voice_consonant(src_h, 3);          // m
        note_lfo(src_h, 0, LFO_PITCH, 5.0f, 0.25f);
    }
    if (tick == 32 && src_h >= 0) { note_off(src_h); src_h = -1; }
    if (tick == CAP_UNTIL && !grabbed) {
        grabbed = record_grab(0, 0.85f) > 0;
        if (grabbed) peaks_n = sample_peaks(0, peaks_lo, peaks_hi, PEAK_COLS);
    }
    if (!grabbed) return;

#ifdef SWEEP
    for (int k = 0; k < SWEEP_N; k++) {
#ifdef SWEEP_VOX
        if (tick == SWEEP_AT + k * SWEEP_GAP) { big_h = note_on(45 + k, VOX, 6); note_lfo(big_h, 0, LFO_PITCH, 5.0f, 0.25f); }   // the REFERENCE: the synth itself on the same notes
#else
        if (tick == SWEEP_AT + k * SWEEP_GAP) big_h = sing(0, 45 + k);
#endif
        if (tick == SWEEP_AT + k * SWEEP_GAP + SWEEP_HOLD && big_h >= 0) { note_off(big_h); big_h = -1; }
    }
#else
    if (tick == SONG_AT) restart_song();
    if (keyp(KEY_SPACE)) restart_song();
    if (song_t >= 0 && song_i < SONG_N) {
        if (song_t == 0) sing(SONG[song_i].who, SONG[song_i].note);
        if (++song_t >= SONG[song_i].len) {
            int *h = SONG[song_i].who ? &small_h : &big_h;
            if (*h >= 0) { note_off(*h); *h = -1; }
            song_i++; song_t = 0;
        }
    }
    if (keyp('Z')) sing(0, 57);
    if (keyr('Z') && big_h >= 0)   { note_off(big_h); big_h = -1; }
    if (keyp('X')) sing(1, 69);
    if (keyr('X') && small_h >= 0) { note_off(small_h); small_h = -1; }
#endif
    big_on = big_h >= 0; small_on = small_h >= 0;

#ifdef DE_TRACE
    watch("tick", "%d", tick);
    watch("song_i", "%d", song_i);
#endif
}

// a Mipo: a round blob with eyes, its mouth open while it sings
static void mipo(int cx, int cy, int r, int col, int singing) {
    int bob = singing ? (int)(2.0f * de_sinf((float)tick * 0.3f)) : 0;
    cy -= bob;
    circfill(cx, cy, r, col);
    circfill(cx - r / 3, cy - r / 4, r / 6 + 1, CLR_WHITE);
    circfill(cx + r / 3, cy - r / 4, r / 6 + 1, CLR_WHITE);
    circfill(cx - r / 3, cy - r / 4, r / 12 + 1, CLR_BLACK);
    circfill(cx + r / 3, cy - r / 4, r / 12 + 1, CLR_BLACK);
    if (singing) circfill(cx, cy + r / 3, r / 4, CLR_BLACK);
    else line(cx - r / 5, cy + r / 3, cx + r / 5, cy + r / 3, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("SINGSAMPLE", 6, 4, CLR_YELLOW);
    font(FONT_SMALL);
    print("one recorded 'mi', a whole tune", 92, 6, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // the one take, with each singer's playhead over it
    int wx = 20, ww = SCREEN_W - 40, mid = 40;
    rect(wx - 2, mid - 16, ww + 4, 33, CLR_DARK_GREY);
    if (peaks_n > 0) {
        for (int i = 0; i < PEAK_COLS; i++) {
            int x = wx + i * ww / PEAK_COLS;
            line(x, mid - (int)(peaks_hi[i] * 14.0f), x, mid - (int)(peaks_lo[i] * 14.0f), CLR_INDIGO);
        }
        float pb = instrument_playhead(BIG), ps = instrument_playhead(SMALL);
        if (pb >= 0.0f) line(wx + (int)(pb * ww), mid - 15, wx + (int)(pb * ww), mid + 15, CLR_PINK);
        if (ps >= 0.0f) line(wx + (int)(ps * ww), mid - 15, wx + (int)(ps * ww), mid + 15, CLR_GREEN);
    } else {
        font(FONT_SMALL); print("listening to the console say 'mi'...", wx + 4, mid - 2, CLR_LIGHT_GREY); font(FONT_NORMAL);
    }
    font(FONT_SMALL);
    print("THE ONLY SAMPLE", wx, mid + 20, CLR_DARK_GREY);
    font(FONT_NORMAL);

    mipo(100, 130, 34, CLR_PINK, big_on);
    mipo(230, 140, 18, CLR_GREEN, small_on);
    font(FONT_SMALL);
    print("BIG  (timbre 0.30)", 64, 172, CLR_PINK);
    print("SMALL  (timbre 0.72)", 192, 172, CLR_GREEN);
    print("Z / X: sing   SPACE: again", 8, 190, CLR_DARK_GREY);
    font(FONT_NORMAL);
    cursor_draw(CUR_ARROW);
}
