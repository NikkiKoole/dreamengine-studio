/* de:meta
{
  "slug": "wahhand",
  "title": "wahhand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth"
  ],
  "lineage": "The acceptance spike for NoteOpts.filter (docs/design/humanized-playing.md §8.11: a per-note filter TYPE, not only a cutoff). Built on strumhand's six-mono-string guitar. Asks whether one guitar whose strokes each pick their own filter (thin scratch, quacking stab, round held note) is worth an engine field, against the best a cart could do before it: one slot per string PER filter type, chokes between them.",
  "description": {
    "summary": "A funk guitar on an E9 vamp where every stroke picks its own FILTER: muted scratches high-passed thin, chord stabs band-passed into a quack, the held note at the end low-passed round, and a wah foot rocking over all of it. TAB flips between one slot per string (the per-note filter) and three slots per string (how it had to be built before).",
    "detail": "A real guitar string can only ring once: strike it again and the old note stops. So the six strings are six mono slots, and a scratch on a string really does cut the stab ringing on it. With NoteOpts.filter each stroke carries its own filter type, so the guitar stays six slots. Without it, a string needs one slot per filter type (18 slots), each choking the other two of its own string, and the wah foot has to be booked on all 18. The two versions are meant to sound the same; what is being tested is whether they do, and what the second one costs to write.",
    "controls": "SPACE play/stop · TAB one slot per string / three · W wah foot on/off · LEFT/RIGHT tempo"
  }
}
de:meta */
// wahhand — the acceptance spike for NoteOpts.filter (a booked note's own FILTER_* type).
//
// ONE  (default): six INSTR_GUITAR slots = six mono strings, slot filter FILTER_LOW; each stroke
//                 books .filter = FILTER_HIGH (scratch) / FILTER_BAND (stab) / FILTER_LOW (hold).
// SPLIT (TAB, or DE_DEFINES=SPLIT): the same guitar built WITHOUT a per-note filter type: per string
//                 one slot per type (18), instrument_choke between the three of each string so a stroke
//                 still stops its string's ring, and the wah lane booked on every one of them.
// The strokes, cutoffs, gains and timing are identical in both; only the plumbing differs.
//
// MEASURED (2026-11-04): ONE and SPLIT render BYTE-IDENTICAL audio (a choke stops a voice exactly as a
// mono slot's next note does), so the per-note type buys 6 slots instead of 18 + 36 chokes + 3x the lane
// bookings, at no cost in sound. Control: DE_DEFINES=NOTYPE = ONE without .filter (every stroke through
// the slot's low-pass), which must differ, and does: centroid 1757 Hz against 5629 Hz.
#include "studio.h"
#include <stdio.h>

#define NSTR   6
#define SL_ONE 10                       // 10..15: one slot per string
#define SL_SPL 20                       // 20..37: [type][string], type 0 low, 1 high, 2 band
#define BASE_CUT 1800
#define BASE_RES 8
#define LOOKAHEAD 0.12

enum { K_REST, K_STAB, K_SCRATCH, K_HOLD };
static const int TYPE_FILTER[3] = { FILTER_LOW, FILTER_HIGH, FILTER_BAND };

// two bars of sixteenths: S stab, x muted scratch, H held note, . rest
static const char *GROOVE[2] = { "S.xSx.xSx.S.xSxx", "S.xSx.xS..x.H..." };
// E9, x-7-6-7-7-x; the scratch mutes all six under the same hand shape
static const int CHORD[NSTR] = { 47, 52, 56, 62, 66, 71 };

static bool split = false, foot = true, playing = true;
static int tempo = 100, step = 0;
static double t_next = 0;
static unsigned rng = 12345u;
static float hrand(void) { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) / 16777216.0f; }

static void guitar(int sl, int mode) {
    instrument(sl, INSTR_GUITAR, 1, 0, 7, 260);
    instrument_harmonics(sl, 0.35f);
    instrument_timbre(sl, 0.70f);
    instrument_morph(sl, 0.10f);
    instrument_mode(sl, MODE_STRING_WEIGHT, 0.40f);
    instrument_mode(sl, MODE_STRING_CLICK, 0.45f);
    instrument_mono(sl, 1);
    instrument_filter(sl, mode, BASE_CUT, BASE_RES);
    instrument_reverb(sl, 0.10f);
}

static void setup_sound(void) {
    for (int s = 0; s < NSTR; s++) {
        guitar(SL_ONE + s, FILTER_LOW);
        instrument_pan(SL_ONE + s, (s - 2.5f) * 0.06f);
        for (int ty = 0; ty < 3; ty++) {
            int sl = SL_SPL + ty * NSTR + s;
            guitar(sl, TYPE_FILTER[ty]);
            instrument_pan(sl, (s - 2.5f) * 0.06f);
        }
        // a stroke of one type must stop this string's ring on the other two (mono covers its own slot)
        for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++)
            if (a != b) instrument_choke(SL_SPL + a * NSTR + s, SL_SPL + b * NSTR + s);
    }
}

// one string of one stroke
static void pluck(int s, int kind, double t, float gain) {
    int ty = kind == K_SCRATCH ? 1 : kind == K_STAB ? 2 : 0;
    NoteOpts o = { .gain = gain };
    int dur = 0;
    if (kind == K_SCRATCH) { o.morph = 0.70f; o.damp = 0.85f; o.cutoff_oct = -0.5f; dur = 45; }
    if (kind == K_STAB)    { o.damp = 0.30f; o.cutoff_oct = -0.2f; dur = 150; }
    if (kind == K_HOLD)    { o.cutoff_oct = 0.6f; dur = 1300; }
    int sl;
    if (split) sl = SL_SPL + ty * NSTR + s;
    else { sl = SL_ONE + s;
#ifndef NOTYPE
           o.filter = TYPE_FILTER[ty];
#endif
    }
    schedule_note_at(t, CHORD[s], sl, 7, dur, &o);
}

static void stroke(int kind, bool down, double t) {
    float force = 0.75f + 0.20f * hrand();
    int lo = kind == K_SCRATCH ? 0 : 1, hi = kind == K_SCRATCH ? 5 : 4;
    if (!down && kind == K_SCRATCH) lo = 2;            // an upstroke catches the top strings
    for (int i = 0; i <= hi - lo; i++) {
        int s = down ? lo + i : hi - i;
        float g = force * (kind == K_SCRATCH ? 0.55f : 0.90f) * (0.9f + 0.1f * hrand());
        pluck(s, kind, t + i * 0.005 + (hrand() - 0.5f) * 0.003, g);
    }
}

// the wah foot: heel on the beat, toe on the "and", over every guitar slot in use
static void book_foot(double t, bool toe, int ramp_ms) {
    float v = toe ? 0.9f : -0.7f;
    if (split) { for (int i = 0; i < 3 * NSTR; i++) lane_at(t, SL_SPL + i, LANE_CUTOFF, v, ramp_ms); }
    else       { for (int s = 0; s < NSTR; s++)     lane_at(t, SL_ONE + s, LANE_CUTOFF, v, ramp_ms); }
}

static void schedule_ahead(void) {
    double now_a = audio_time(), six = 60.0 / tempo / 4.0;
    while (t_next < now_a + LOOKAHEAD) {
        if (t_next < now_a) t_next = now_a + 0.01;
        int pos = step % 16;
        char c = GROOVE[(step / 16) % 2][pos];
        int kind = c == 'S' ? K_STAB : c == 'x' ? K_SCRATCH : c == 'H' ? K_HOLD : K_REST;
        if (kind != K_REST) stroke(kind, (pos & 1) == 0, t_next + (hrand() - 0.5f) * 0.006);
        if (foot && (pos & 1) == 0) book_foot(t_next, (pos & 3) == 2, (int)(six * 2000 * 0.9));
        t_next += six;
        step++;
    }
}

static void switch_mode(void) {
    split = !split;
    double now_a = audio_time();
    for (int i = 0; i < NSTR; i++) unschedule(SL_ONE + i, now_a, 0);
    for (int i = 0; i < 3 * NSTR; i++) unschedule(SL_SPL + i, now_a, 0);
    t_next = audio_time() + 0.1;
}

void init(void) {
    setup_sound();
#ifdef SPLIT
    split = true;
#endif
#ifdef NOFOOT
    foot = false;
#endif
    t_next = audio_time() + 0.2;
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; t_next = audio_time() + 0.1; }
    if (keyp(KEY_TAB)) switch_mode();
    if (keyp('W')) {
        foot = !foot;
        if (!foot) { for (int i = 0; i < NSTR; i++) lane_at(audio_time(), SL_ONE + i, LANE_CUTOFF, 0, 80);
                     for (int i = 0; i < 3 * NSTR; i++) lane_at(audio_time(), SL_SPL + i, LANE_CUTOFF, 0, 80); }
    }
    if (keyp(KEY_RIGHT)) tempo = min(130, tempo + 4);
    if (keyp(KEY_LEFT))  tempo = max(70, tempo - 4);
    if (playing) schedule_ahead();
#ifdef DE_TRACE
    watch("step", "%d", step);
    watch("split", "%d", split);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("WAHHAND", 8, 8, CLR_WHITE);
    print(split ? "TAB: 3 slots/string, 36 chokes" : "TAB: 1 slot/string, per-note", 8, 24,
          split ? CLR_ORANGE : CLR_GREEN);
    print(foot ? "W: wah foot ON" : "W: wah foot off", 8, 36, foot ? CLR_YELLOW : CLR_DARK_GREY);
    char buf[32]; snprintf(buf, sizeof buf, "%d bpm", tempo); print(buf, 8, 48, CLR_LIGHT_GREY);
    int cur = (step + 31) % 32;
    for (int b = 0; b < 2; b++) for (int i = 0; i < 16; i++) {
        char c = GROOVE[b][i];
        int col = c == 'S' ? CLR_PINK : c == 'x' ? CLR_LIGHT_GREY : c == 'H' ? CLR_BLUE : CLR_DARKER_GREY;
        int x = 8 + i * 18, y = 80 + b * 24;
        rectfill(x, y, 15, 17, col);
        if (b * 16 + i == cur && playing) rect(x - 1, y - 1, 17, 19, CLR_WHITE);
    }
    print("S stab=band x scratch=high H=low", 8, 140, CLR_MEDIUM_GREY);
    print("SPACE play  LEFT/RIGHT tempo", 8, 156, CLR_MEDIUM_GREY);
}
