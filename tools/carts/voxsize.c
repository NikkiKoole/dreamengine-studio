/* de:meta
{
  "slug": "voxsize",
  "title": "voxsize",
  "status": "active",
  "created": "2026-10-29",
  "kind": ["probe", "tech-demo"],
  "genre": null,
  "teaches": ["granular-synth"],
  "lineage": "The acceptance probe + showcase for sample_formant() (GitHub issue #44), the third face of the PSOLA core after sample_autotune and sample_shift. Same timeline as voxshift, so the same formant-check / psola-check windows land on the same takes.",
  "description": {
    "summary": "One voice, four sizes: pitch and vocal-tract size as two separate knobs.",
    "detail": "A synthesised 'ah' (INSTR_VOICE) is captured off the console's own output with record_arm/record_grab and copied into four slots: RAW; BIGGER (sample_formant -7, a larger throat, same note); SMALLER (sample_formant +7, a smaller throat, same note); and HIGHER, SAME SIZE (sample_shift +7 then sample_formant -7: the note goes up a fifth while the formants are walked back down, so the speaker should not change). Playing a sample faster moves pitch and size together, which is the slowed-tape / chipmunk sound; this cart is the A/B that shows them moving apart.",
    "controls": "It runs on its own: SPACE replays from the top. 1 / 2 / 3 / 4 audition one take."
  }
}
de:meta */
//
// voxsize — the acceptance probe for sample_formant(): did the SPEAKER change while the NOTE held?
//
// Built for Playtime's Mipo voices (issue #44): short recorded hums where each character should sound
// like a different-sized creature, not like the same one played at a different tape speed. The
// source is INSTR_VOICE captured through record_arm/record_grab, like voxshift, so the render is
// deterministic and tools/formant-check.js can be pointed at fixed timestamps of a play.js --wav:
// take k is audible from (PLAY_AT + k*TAKE_GAP)/60 s, the same schedule as voxshift.

#include "studio.h"
#include "cursor.h"

#define VOX    5           // the source voice
#define PLAY   6           // the INSTR_SAMPLE playback slot (rebound per take)

#define SRC_MIDI   57      // A3 — low enough that F1/F2 sit well above f0
#define CAP_SECS   1.20f
#define TAKES      4
#define TAKE_MAX   65536   // float buffer for one take's PCM (CAP_SECS * 44100 ~= 53k)
#ifndef SIZE_SEMIS
#define SIZE_SEMIS 7.0f    // how far the formants move (probe knob: DE_DEFINES=SIZE_SEMIS=12)
#endif

static const char *TAKE_NAME[TAKES] = { "RAW", "BIGGER", "SMALLER", "HIGHER, SAME SIZE" };
static const char *TAKE_NOTE[TAKES] = {
    "the captured take",
    "sample_formant(-7) - a bigger throat, the same note",
    "sample_formant(+7) - a smaller throat, the same note",
    "sample_shift(+7) + sample_formant(-7) - new note, same speaker",
};

static int   tick = 0;
static int   src_h = -1, play_h = -1;
static int   prepared = 0;
static int   now_playing = -1;
static float peaks_lo[TAKES][64], peaks_hi[TAKES][64];
static int   peaks_n[TAKES];

#define REC_UNTIL  95
#define PLAY_AT    110
#define TAKE_GAP   90

static void copy_take(int dst) {
    static float buf[TAKE_MAX];
    int n = sample_read(0, buf, (int)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0) sample_load(dst, buf, n);
}

static void prepare_takes(void) {
    if (!record_grab(0, CAP_SECS)) return;
    for (int k = 1; k < TAKES; k++) copy_take(k);
    sample_formant(1, -SIZE_SEMIS);           // bigger
    sample_formant(2,  SIZE_SEMIS);           // smaller
    sample_shift(3, SIZE_SEMIS);              // up a fifth, formants ride along...
    sample_formant(3, -SIZE_SEMIS);           // ...then walk them back down
    for (int k = 0; k < TAKES; k++) peaks_n[k] = sample_peaks(k, peaks_lo[k], peaks_hi[k], 64);
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
    record_arm();
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
    if (tick == 2) {
        src_h = note_on(SRC_MIDI, VOX, 6);
        note_lfo(src_h, 0, LFO_PITCH, 4.5f, 0.35f);
    }
    if (tick == REC_UNTIL - 8 && src_h >= 0) { note_off(src_h); src_h = -1; }
    if (tick == REC_UNTIL && !prepared) prepare_takes();
    if (prepared)
        for (int k = 0; k < TAKES; k++)
            if (tick == PLAY_AT + k * TAKE_GAP) audition(k);

#ifdef DE_TRACE
    watch("tick", "%d", tick);
    watch("take", "%d", now_playing);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("VOXSIZE", 6, 4, CLR_YELLOW);
    font(FONT_SMALL);
    print("same note, different speaker?", 70, 6, CLR_DARK_GREY);
    font(FONT_NORMAL);

    for (int k = 0; k < TAKES; k++) {
        int y = 20 + k * 44, h = 30;
        int on = (now_playing == k);
        rect(4, y, SCREEN_W - 8, h + 10, on ? CLR_YELLOW : CLR_DARK_GREY);
        print(TAKE_NAME[k], 8, y + 2, on ? CLR_YELLOW : CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print(TAKE_NOTE[k], 8, y + 33, CLR_DARK_GREY);
        font(FONT_NORMAL);
        int wx = 160, ww = SCREEN_W - 168, mid = y + 14;
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
