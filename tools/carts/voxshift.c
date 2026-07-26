/* de:meta
{
  "slug": "voxshift",
  "title": "voxshift",
  "status": "active",
  "created": "2026-07-26",
  "kind": ["probe", "tech-demo"],
  "genre": null,
  "teaches": ["scale-quantize"],
  "lineage": "The acceptance probe + showcase for sample_shift() — Rung B of docs/design/contemporary-rebirth.md, the INTERVAL face of the formant-preserving PSOLA that docs/design/transparent-autotune.md shipped as a scale-SNAP (sample_autotune). It answers the question that doc's gate exists for: did the pitch move while the formants stayed? Four takes of ONE captured voice, side by side: RAW, SNAPPED, +12 with formants HELD, +12 with formants FOLLOWING. Source is INSTR_VOICE captured through record_arm/record_grab, so it needs no mic and replays deterministically (unlike mictune/livetune, which are LIVE by nature).",
  "description": {
    "summary": "The chipmunk, on a dial. One captured voice, four ways: raw, auto-tuned to a scale, and shifted up an octave twice — once keeping the singer's voice, once letting it chipmunk. Play them back to back and the difference between moving a pitch and moving a VOICE is obvious in one listen.",
    "detail": "A synthesised 'ah' (INSTR_VOICE with a deliberately wobbly pitch) is captured off the console's own output with record_arm/record_grab, then copied into four sample slots and processed: slot 0 stays RAW; slot 1 gets sample_autotune (snapped to a scale, wobble ironed out); slot 2 gets sample_shift(+12, formant 0) — an octave up with the formants HELD, so it is the same voice singing higher; slot 3 gets sample_shift(+12, formant 1) — an octave up with the formants riding along, which is the chipmunk a plain resample gives you. It auto-plays the four in order, drawing each waveform with the region being heard highlighted, so the numbers from tools/formant-check.js line up with what you hear: in slot 2 f0 doubles and F1/F2/F3 stay put; in slot 3 everything scales together.",
    "controls": "It runs on its own: SPACE replays the sequence from the top. 1 / 2 / 3 / 4 audition one take (raw / auto-tuned / an octave up / an octave down). Watch the waveform lengths: all four are the same length, which is the point."
  },
  "todo": [
    "REBAKE PENDING: the thumbnail is a placeholder — the bake needs a GL context and the machine could not create one when it landed (raylib InitWindow -> rlglInit -> a NULL GL function pointer, with the display asleep). Run: node tools/make-cart.js --run editor/public/carts/voxshift.cart.png",
    "Once harmonize_mic() has a home cart, cross-link it here: this is the offline twin of that live face.",
    "The SNAPPED take reads f0 206Hz with a 110Hz range in formant-check - the analyzer octave-flips on PSOLA output, not necessarily a fault in the take. Worth a look when the transparent-shift spike happens."
  ]
}
de:meta */
//
// voxshift — is it the pitch that moved, or the voice? The acceptance probe for sample_shift().
//
// docs/design/transparent-autotune.md shipped a formant-preserving TD-PSOLA with ONE face: snap a
// take to a scale. Rung B of contemporary-rebirth.md wanted the other face — shift by a fixed
// INTERVAL — plus the axis the snap face never needed: whether the formants come along.
//
//   formant 0  the grains keep their content  → an octave up that is still the same singer
//   formant 1  the grains are resampled too   → the chipmunk (what a plain resample gives you)
//
// The source is the console's OWN INSTR_VOICE captured through record_arm/record_grab, deliberately:
// no mic means no permission prompt and a deterministic render, so tools/formant-check.js can be
// pointed at fixed timestamps in a play.js --wav and gate the claim.
//
// NO_SHIFT builds the cart WITHOUT the sample_shift regions — that is how the A/B against the
// pre-refactor engine was run (DE_DEFINES=NO_SHIFT), to prove sample_autotune stayed bit-identical
// when its PSOLA loop was generalized. Keep it compiling.

#include "studio.h"
#include "cursor.h"

#define VOX    5           // the source voice
#define PLAY   6           // the INSTR_SAMPLE playback slot (rebound per take)

#define SRC_MIDI   57      // A3 — low enough that F1/F2 sit well above f0
#define CAP_SECS   1.20f
#define TAKES      4
#define TAKE_MAX   65536   // float buffer for one take's PCM
#ifndef SHIFT_SEMIS
#define SHIFT_SEMIS 12.0f  // the interval both shift takes use (probe knob: DE_DEFINES=SHIFT_SEMIS=7)
#endif   // float buffer for one take's PCM (CAP_SECS * 44100 ~= 53k)

static const char *TAKE_NAME[TAKES] = { "RAW", "SNAPPED", "UP AN OCTAVE", "DOWN AN OCTAVE" };
static const char *TAKE_NOTE[TAKES] = {
    "the captured take: f0 220Hz, wobbling",
    "sample_autotune - snapped to the scale, wobble ironed out",
    "sample_shift(+12) - f0 440Hz and STILL 1.2s long",
    "sample_shift(-12) - f0 110Hz, same length again",
};

static int   tick = 0;
static int   src_h = -1, play_h = -1;
static int   prepared = 0;          // takes processed?
static int   now_playing = -1;      // which take is sounding
static float peaks_lo[TAKES][64], peaks_hi[TAKES][64];
static int   peaks_n[TAKES];

// the schedule, in frames at 60fps (play.js --wav renders at a fixed step, so these are timestamps
// formant-check can be pointed at: take k is audible from (PLAY_AT + k*TAKE_GAP)/60 seconds)
#define REC_UNTIL  95
#define PLAY_AT    110
#define TAKE_GAP   90

static void grab_peaks(int take) {
    peaks_n[take] = sample_peaks(take, peaks_lo[take], peaks_hi[take], 64);
}

// copy slot 0 into slot `dst` (sample_read + sample_load — the documented way to duplicate a take)
static void copy_take(int dst) {
    static float buf[TAKE_MAX];   // one take (CAP_SECS at 44.1k fits with room to spare)
    int n = sample_read(0, buf, (int)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0) sample_load(dst, buf, n);
}

static void prepare_takes(void) {
    if (!record_grab(0, CAP_SECS)) return;      // nothing captured yet
    for (int k = 1; k < TAKES; k++) copy_take(k);
    sample_autotune(1, 9, SCALE_PENTA_MIN, 1.0f);            // root A, minor pentatonic
#ifndef NO_SHIFT
    sample_shift(2, SHIFT_SEMIS);        // up, same length
    sample_shift(3, -SHIFT_SEMIS);       // down, same length
#endif
    for (int k = 0; k < TAKES; k++) grab_peaks(k);
    prepared = 1;
}

static void audition(int take) {
    if (play_h >= 0) { note_off(play_h); play_h = -1; }
    instrument_sample(PLAY, take, 60);
    play_h = note_on(60, PLAY, 6);
    now_playing = take;
}

void init(void) {
    instrument(VOX, INSTR_VOICE, 40, 200, 7, 200);
    instrument_harmonics(VOX, 0.55f);       // an "ah"
    instrument_timbre(VOX, 0.45f);          // a medium tract
    instrument_morph(VOX, 0.55f);
    instrument_level(VOX, 0.8f);
    instrument(PLAY, INSTR_SAMPLE, 1, 10, 7, 40);
    record_arm();                           // capture the console's own output
}

static void restart(void) {
    tick = 0; prepared = 0; now_playing = -1;
    if (play_h >= 0) { note_off(play_h); play_h = -1; }
    if (src_h  >= 0) { note_off(src_h);  src_h  = -1; }
}

void update(void) {
    if (keyp(KEY_SPACE)) { restart(); return; }
    for (int k = 0; k < TAKES; k++) if (keyp('1' + k) && prepared) audition(k);

    tick++;

    if (tick == 2) {                       // sing the source: a wobbly held vowel
        src_h = note_on(SRC_MIDI, VOX, 6);
        note_lfo(src_h, 0, LFO_PITCH, 4.5f, 0.35f);   // the wobble sample_autotune should iron out
    }
    if (tick == REC_UNTIL - 8 && src_h >= 0) { note_off(src_h); src_h = -1; }
    if (tick == REC_UNTIL && !prepared) prepare_takes();

    if (prepared)                           // then walk the four takes
        for (int k = 0; k < TAKES; k++)
            if (tick == PLAY_AT + k * TAKE_GAP) audition(k);

#ifdef DE_TRACE
    watch("tick", "%d", tick);
    watch("take",  "%d", now_playing);
    watch("prepared", "%d", prepared);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("VOXSHIFT", 6, 4, CLR_YELLOW);
    font(FONT_SMALL);
    print("did the PITCH move, or the VOICE?", 78, 6, CLR_DARK_GREY);
    font(FONT_NORMAL);

    for (int k = 0; k < TAKES; k++) {
        int y = 20 + k * 44, h = 30;
        int on = (now_playing == k);
        rect(4, y, SCREEN_W - 8, h + 10, on ? CLR_YELLOW : CLR_DARK_GREY);
        print(TAKE_NAME[k], 8, y + 2, on ? CLR_YELLOW : CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print(TAKE_NOTE[k], 8, y + 33, CLR_DARK_GREY);
        font(FONT_NORMAL);
        // the waveform
        int wx = 128, ww = SCREEN_W - 136, mid = y + 14;
        line(wx, mid, wx + ww, mid, CLR_DARKER_GREY);
        if (peaks_n[k] > 0)
            for (int i = 0; i < 64; i++) {
                int x = wx + i * ww / 64;
                int a = mid - (int)(peaks_hi[k][i] * 12.0f), b = mid - (int)(peaks_lo[k][i] * 12.0f);
                line(x, a, x, b, on ? CLR_YELLOW : CLR_INDIGO);
            }
        else { font(FONT_SMALL); print("waiting for the capture...", wx, mid - 2, CLR_DARKER_GREY); font(FONT_NORMAL); }
    }

    font(FONT_SMALL);
    print("1-4: audition a take   SPACE: run it again from the top", 8, 192, CLR_DARK_GREY);
    font(FONT_NORMAL);
    cursor_draw(CUR_ARROW);
}
