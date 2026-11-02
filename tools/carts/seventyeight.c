/* de:meta
{
  "slug": "seventyeight",
  "title": "Seventy-Eight",
  "status": "active",
  "created": "2026-11-02",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "per-instrument-fx-bus",
    "waveguide-synth"
  ],
  "lineage": "Showcase for GitHub #47 (from Playtime's booked score music): instrument_group + reverb_bus_into (a whole band, its room included, on ONE worn record), stereo_width (a 78 is mono), and schedule_note_at (a clarinet that slides note to note, a tuba that leans on the downbeat), all booked ahead on the audio clock.",
  "description": {
    "summary": "A little 1920s dance band on a wind-up gramophone. Drop the needle and the whole band goes onto one worn 78 together: it warbles as one, loses its top and its bass, folds to mono, and the room's echo goes with it. Ring the bell: it stays bright and clean, because it is not on the record.",
    "detail": "Tuba on the one and three, banjo on the two and four, brushes, and a clarinet that slides between its notes (booked ahead with schedule_note_at, never re-tongued inside a phrase). Every band slot plays through ONE group bus (instrument_group); the record is that bus's chain: tape wow + warble, a bass-cut EQ and a lowpass on the whole group (instrument_fx_filter), stereo_width(0). The band's reverb tank folds into the same group (reverb_bus_into), so the hall is recorded onto the disc rather than added afterwards. The bell is a separate slot on the master, which is exactly what a game's sound effects would be. More WEAR = deeper sags and more crackle.",
    "controls": "SPACE or tap the record: needle down / up · UP / DOWN or tap the WEAR bar: wear · X or tap the bell: ring it"
  }
}
de:meta */
// Seventy-Eight — a band on one worn record (issue #47).
//
// THE ONE IDEA: an old record does not wobble each instrument on its own. The whole band was cut into
// one groove, so it sags TOGETHER, and the room it played in is in the groove too. So every band slot
// plays through ONE group bus whose chain IS the record, the reverb tank folds into that group, and
// the needle toggles the chain between "clean" and "worn 78". The bell is the control: a sound that
// was never on the record, left on the master, staying bright while the band goes thin.
#include "studio.h"
#include <math.h>

// ── the band (slot 5 = the LEAD of the group: the record's effects live on its bus) ──
#define TUBA    5
#define BANJO   6
#define CLAR    7
#define BRUSH   8
#define BLOCK   9
#define CRACKLE 10
#define BELL    11      // NOT in the group: the sound effect that stays clean

#define BPM     112
#define STEP    (60.0 / BPM / 2.0)   // an eighth note, in seconds
#define AHEAD   0.25                 // book this far ahead of the audio clock

// eight bars of an original foxtrot tune over a ragtime circle: C C A7 A7 D7 G7 C G7
static const int CHORD[8][4] = {
    { 48, 52, 55, 60 }, { 48, 52, 55, 60 }, { 45, 49, 52, 55 }, { 45, 49, 52, 55 },
    { 50, 54, 57, 60 }, { 43, 47, 50, 53 }, { 48, 52, 55, 60 }, { 43, 47, 50, 53 },
};
// the clarinet: { midi (-1 = rest), length in eighths, slide INTO this note from the last }
typedef struct { int midi, len, slide; } Mel;
static const Mel MEL[] = {
    {67,2,0},{72,2,1},{76,2,1},{79,2,1},          // C   — up the chord, one breath
    {81,3,1},{79,1,1},{76,4,0},                   // C
    {73,2,0},{76,2,1},{81,2,1},{79,2,0},          // A7
    {76,6,0},{-1,2,0},                            // A7
    {78,2,0},{81,2,1},{84,2,0},{81,2,0},          // D7
    {83,3,1},{81,1,1},{79,2,0},{77,2,0},          // G7
    {76,2,0},{74,1,0},{72,1,1},{76,2,0},{79,2,0}, // C
    {77,2,1},{74,2,0},{71,2,0},{67,2,0},          // G7
};
#define MEL_N ((int)(sizeof MEL / sizeof MEL[0]))

static double next_t = -1.0;   // audio time of the next eighth to book
static int    step = 0;        // eighths since the start (64 per chorus)
static int    mel_i = 0, mel_left = 0;

static int   needle = 0;       // 0 = in the room (clean, wide) · 1 = on the record
static float wear = 0.5f;      // 0..1
static int   applied_needle = -1;
static float applied_wear = -1.0f;

static float spin = 0.0f, spin_wob = 0.0f, arm = 0.0f;   // drawing state
static int   bell_flash = 0;

// THE RECORD — the group's chain. Set-and-hold: called only when the needle or the wear CHANGES.
static void apply_record(void) {
    if (needle) {
        instrument_tape(TUBA, 0.25f + 0.5f * wear, 0.15f + 0.3f * wear, 0.5f);   // wow + flutter + a soft saturation
        instrument_tape_warble(TUBA, 0.2f + 0.8f * wear);                        // the worn groove: random sags, all together
        instrument_eq(TUBA, -12.0f, 3.0f, -12.0f);                               // an acoustic recording: no bass…
        instrument_fx_filter(TUBA, FILTER_LOW, 3400.0f - 1600.0f * wear, 0.15f); // …and no top, the room's echo included
        instrument_stereo_width(TUBA, 0.0f);                                     // a 78 is mono
    } else {
        instrument_tape_warble(TUBA, 0.0f);
        instrument_tape(TUBA, 0.0f, 0.0f, 0.0f);
        instrument_eq(TUBA, 0.0f, 0.0f, 0.0f);
        instrument_fx_filter(TUBA, FILTER_OFF, 20000.0f, 0.0f);
        instrument_stereo_width(TUBA, 1.0f);
    }
    applied_needle = needle; applied_wear = wear;
}

void init(void) {
    instrument(TUBA,  INSTR_BRASS,  20, 0, 6, 120);  instrument_harmonics(TUBA, 0.9f);  instrument_timbre(TUBA, 0.35f);
    instrument(BANJO, INSTR_GUITAR,  0, 0, 7, 120);  instrument_harmonics(BANJO, 0.9f); instrument_timbre(BANJO, 0.75f); instrument_morph(BANJO, 0.6f);
    instrument(CLAR,  INSTR_REED,   25, 0, 6, 90);   instrument_harmonics(CLAR, 0.0f);  instrument_timbre(CLAR, 0.45f);  instrument_morph(CLAR, 0.45f);
    instrument(BRUSH, INSTR_NOISE,   2, 60, 0, 40);  instrument_filter(BRUSH, FILTER_HIGH, 3000, 2);
    instrument(BLOCK, INSTR_MALLET,  0, 0, 7, 80);   instrument_harmonics(BLOCK, 0.0f); instrument_timbre(BLOCK, 0.8f);  instrument_morph(BLOCK, 0.1f);
    instrument(CRACKLE, INSTR_NOISE, 0, 4, 0, 4);    instrument_filter(CRACKLE, FILTER_HIGH, 2500, 1);
    instrument(BELL,  INSTR_MALLET,  0, 0, 7, 900);  instrument_harmonics(BELL, 0.95f); instrument_timbre(BELL, 0.8f);  instrument_morph(BELL, 0.8f);

    // the stage: spread out in the room, so folding to mono is something you can hear
    instrument_pan(BANJO, -0.6f); instrument_pan(CLAR, 0.5f); instrument_pan(BRUSH, -0.3f); instrument_pan(BLOCK, 0.4f);
    instrument_pan(BELL, 0.7f);

    // ONE record for the whole band: every band slot plays through the TUBA's bus
    instrument_group(BANJO, TUBA); instrument_group(CLAR, TUBA); instrument_group(BRUSH, TUBA);
    instrument_group(BLOCK, TUBA); instrument_group(CRACKLE, TUBA);
    // the room: a reverb tank fed by the band, folded back INTO the group, so the hall is on the disc
    reverb_bus(1, 0.55f, 0.5f);
    reverb_bus_into(1, TUBA);
    instrument_reverb_bus(TUBA, 1, 0.15f); instrument_reverb_bus(BANJO, 1, 0.25f);
    instrument_reverb_bus(CLAR, 1, 0.35f); instrument_reverb_bus(BRUSH, 1, 0.2f);
    apply_record();
}

static void book_step(double t, int s) {
    int bar = (s / 8) % 8, e = s % 8;
    const int *ch = CHORD[bar];
    // tuba: root on the one, fifth on the three; the downbeat of each bar LEANS (accent)
    if (e == 0) schedule_note_at(t, ch[0] - 12, TUBA, 6, (int)(STEP * 1800), &(NoteOpts){ .accent = 0.6f });
    if (e == 4) schedule_note_at(t, ch[2] - 12, TUBA, 5, (int)(STEP * 1800), NULL);
    // banjo: the chord on the two and four, strummed down in 12 ms
    if (e == 2 || e == 6)
        for (int k = 1; k < 4; k++) schedule_at(t + 0.012 * (k - 1), ch[k], BANJO, k == 1 ? 7 : 6, 160);
    // brushes on every beat, the woodblock on the "and" of four
    if (e % 2 == 0) schedule_at(t, 60, BRUSH, (e == 2 || e == 6) ? 4 : 2, 70);
    if (e == 7) schedule_at(t, 79, BLOCK, 3, 60);
    // clarinet: a note starts when the last one has run out. A note the NEXT one slides into is held
    // 40 ms past its end, so its key is still down when the slide arrives (legato needs that overlap)
    if (mel_left == 0) {
        const Mel *m = &MEL[mel_i];
        const Mel *nx = &MEL[(mel_i + 1) % MEL_N];
        if (m->midi >= 0) {
            int dur = (int)(m->len * STEP * 1000.0) + (nx->slide ? 40 : -30);
            NoteOpts o = { .glide_ms = m->slide ? 90 : 0, .accent = (e == 0) ? 0.3f : 0.0f };
            schedule_note_at(t, m->midi, CLAR, 4, dur, &o);
        }
        mel_left = m->len;
        mel_i = (mel_i + 1) % MEL_N;
    }
    mel_left--;
    // the groove itself: crackle, only with the needle down, more of it the more worn the record
    if (needle && rnd(100) < (int)(20 + 60 * wear)) schedule_at(t + rnd(250) / 1000.0, 90, CRACKLE, 1 + rnd(2), 4);
}

static void ring_bell(void) {
    hit(88, BELL, 6, 900);
    schedule_hit(140, 84, BELL, 5, 900);
    bell_flash = 20;
}

#define BELL_X 258
#define BELL_Y 118
#define REC_X  100
#define REC_Y  100
#define PX     206   // the side panel

void update(void) {
    bool rec_tap  = tapp(REC_X - 70, REC_Y - 70, 140, 140) || (mouse_pressed(0) && abs(mouse_x() - REC_X) < 70 && abs(mouse_y() - REC_Y) < 70);
    bool bell_tap = tapp(BELL_X - 24, BELL_Y - 24, 48, 48) || (mouse_pressed(0) && abs(mouse_x() - BELL_X) < 24 && abs(mouse_y() - BELL_Y) < 24);
    if (keyp(KEY_SPACE) || rec_tap) needle = !needle;
    if (keyp(KEY_UP))   wear = wear + 0.1f > 1.0f ? 1.0f : wear + 0.1f;
    if (keyp(KEY_DOWN)) wear = wear - 0.1f < 0.0f ? 0.0f : wear - 0.1f;
    if (keyp('X') || bell_tap) ring_bell();
    // the wear bar is a slider too: tap or click where you want it (a phone has no UP/DOWN keys)
    for (int i = 0; i < touch_count(); i++)
        if (touch_x(i) >= PX && touch_x(i) <= PX + 100 && touch_y(i) >= 72 && touch_y(i) <= 92) wear = (float)((touch_x(i) - PX) / 5) * 0.05f;   // 5% steps: a drag re-applies the record ~20 times, not every frame
    if (mouse_down(0) && mouse_x() >= PX && mouse_x() <= PX + 100 && mouse_y() >= 72 && mouse_y() <= 92) wear = (float)((mouse_x() - PX) / 5) * 0.05f;
    if (needle != applied_needle || fabsf(wear - applied_wear) > 0.001f) apply_record();

    // book everything up to AHEAD past the audio clock — every note lands on its exact sample
    double now_t = audio_time();
    if (next_t < 0.0) next_t = now_t + 0.1;
    while (next_t < now_t + AHEAD) { book_step(next_t, step); step++; next_t += STEP; }

    // drawing state: 78 rpm, the arm swings over the record, and the spin sags with the wear
    float rps = 78.0f / 60.0f;
    spin_wob = needle ? spin_wob * 0.96f + (rnd(100) < 3 ? -(0.05f + 0.2f * wear) : 0.0f) : 0.0f;
    spin += rps * (1.0f + spin_wob) / 60.0f;
    arm += ((needle ? 1.0f : 0.0f) - arm) * 0.12f;
    if (bell_flash > 0) bell_flash--;
#ifdef DE_TRACE
    watch("needle", "%d", needle); watch("wear", "%.1f", wear); watch("step", "%d", step);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    // the cabinet
    rectfill(10, 18, 186, 168, CLR_DARK_BROWN);
    rect(10, 18, 186, 168, CLR_BROWN);
    // the record: grooves, a label, and a mark on the label so you can SEE it turn
    circfill(REC_X, REC_Y, 66, CLR_BLACK);
    for (int r = 24; r < 66; r += 4) circ(REC_X, REC_Y, r, (r / 4) % 2 ? CLR_DARKER_GREY : CLR_BLACK);
    circfill(REC_X, REC_Y, 20, needle ? CLR_DARK_RED : CLR_RED);
    circfill(REC_X, REC_Y, 2, CLR_BROWNISH_BLACK);
    float a = spin * 6.2831853f;
    for (int k = 0; k < 3; k++) {
        float aa = a + k * 2.0943951f;
        pset(REC_X + (int)(cosf(aa) * 14), REC_Y + (int)(sinf(aa) * 14), CLR_LIGHT_PEACH);
        pset(REC_X + (int)(cosf(aa) * 15), REC_Y + (int)(sinf(aa) * 15), CLR_LIGHT_PEACH);
    }
    print("78", REC_X - 7, REC_Y - 12, CLR_LIGHT_PEACH);
    // the tone arm: pivots from the corner; up = resting OFF the record, down = on the outer groove
    int px = 178, py = 30;
    float ang = 1.5f + 0.35f * arm;
    int nx = px + (int)(cosf(ang) * 96), ny = py + (int)(sinf(ang) * 96);
    thickline(px, py, nx, ny, 3, CLR_LIGHT_GREY);
    circfill(px, py, 6, CLR_MEDIUM_GREY);
    rectfill(nx - 4, ny - 3, 8, 6, needle ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);

    // the panel: what you are hearing, the wear, the bell
    print(needle ? "ON THE RECORD" : "IN THE ROOM", PX, 30, needle ? CLR_ORANGE : CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print(needle ? "one warble, mono," : "the band, live,", PX, 42, CLR_PEACH);
    print(needle ? "the room on the disc" : "spread out wide", PX, 50, CLR_PEACH);
    print("WEAR", PX, 70, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);
    rect(PX, 78, 100, 8, CLR_BROWN);
    rectfill(PX + 1, 79, (int)(98 * wear), 6, needle ? CLR_ORANGE : CLR_DARK_ORANGE);
    // the bell: NOT on the record, so it rings bright either way
    int bc = bell_flash ? CLR_YELLOW : CLR_LIGHT_YELLOW;
    circfill(BELL_X, BELL_Y + 2, 16, bell_flash ? CLR_ORANGE : CLR_DARK_ORANGE);
    circfill(BELL_X, BELL_Y - 4, 12, bc);
    rectfill(BELL_X - 16, BELL_Y + 2, 32, 8, bc);
    circfill(BELL_X, BELL_Y - 18, 3, bc);
    if (bell_flash) { circ(BELL_X, BELL_Y, 22 + (20 - bell_flash) / 2, CLR_YELLOW); }
    font(FONT_SMALL);
    print("the bell is a sound", PX, 146, CLR_PEACH);
    print("effect: never on the", PX, 154, CLR_PEACH);
    print("record, always clean", PX, 162, CLR_PEACH);
    font(FONT_NORMAL);
    hint("SPACE/tap record: needle   UP/DOWN: wear   X/tap: bell");
}
