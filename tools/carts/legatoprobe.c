/* de:meta
{
  "slug": "legatoprobe",
  "title": "booked legato + accent probe",
  "status": "hidden",
  "created": "2026-11-02",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Measurement probe for schedule_note_at() (issue #47, from Playtime's booked music): legato glides, accents and per-note macro offsets on notes booked ahead with audio_time(). Rendered headless and read back by pitch, peak and centroid; not played by hand.",
  "description": "Measurement probe, not a playable cart (status hidden). Books five tests at fixed audio times in the first frame: T1 a four-note flute phrase that should SLIDE note to note on one voice; T2 an eight-step 303 line with accents on steps 2 and 5; T3 the same line with a 303 SLIDE from step 3 into step 4; T4 a mallet and T5 a flute alternating plain and accented notes; T6 a mallet with a per-note timbre offset. VARIANT 0 uses the new extras, 1 books every note through schedule_note_at with an all-zero NoteOpts, 2 uses plain schedule_at — 1 and 2 must render byte-identical. Render it with `node tools/play.js legatoprobe script /dev/null --headless --frames 780 --wav out.wav`."
}
de:meta */
// legatoprobe — the schedule_note_at() measurement (issue #47).
//
// THE CONTROLS ARE THE POINT. VARIANT 1 sends every note down the NEW path with nothing set, VARIANT 2
// down the old one; they must be byte-identical, or "the extras changed the sound" below is measuring
// the path rather than the extras. And within VARIANT 0, every UNACCENTED 303 step must match VARIANT 2's
// sample for sample — the accent may only touch the notes it is on.
#include "studio.h"

static int VARIANT = 0;   // 0 = extras on · 1 = schedule_note_at with a zeroed NoteOpts · 2 = plain schedule_at

#define FLUTE  5
#define ACID   6
#define MAL    7
#define FLUTE2 8

static int booked = 0;

void init(void) {
    instrument(FLUTE, INSTR_PIPE, 30, 0, 6, 140);
    instrument_harmonics(FLUTE, 0.0f); instrument_timbre(FLUTE, 0.38f); instrument_morph(FLUTE, 0.7f);
    instrument(FLUTE2, INSTR_PIPE, 30, 0, 6, 140);
    instrument_harmonics(FLUTE2, 0.0f); instrument_timbre(FLUTE2, 0.38f); instrument_morph(FLUTE2, 0.7f);
    instrument(ACID, INSTR_SAW, 2, 60, 6, 25);                 // Playtime's tb303 patch, minus the drift LFOs
    instrument_filter(ACID, FILTER_DIODE, 390, 10);
    instrument_env(ACID, 0, ENV_CUTOFF, 0, 230, 2990);
    instrument(MAL, INSTR_MALLET, 0, 0, 7, 600);
    instrument_harmonics(MAL, 0.2f); instrument_timbre(MAL, 0.35f); instrument_morph(MAL, 0.4f);
}

// one booked note, through whichever path the variant asks for
static void book(double t, int midi, int slot, int vol, int dur, int glide, float accent, float dtimb) {
    if (VARIANT == 2) { schedule_at(t, midi, slot, vol, dur); return; }
    NoteOpts o = { 0 };
    if (VARIANT == 0) { o.glide_ms = glide; o.accent = accent; o.timbre = dtimb; }
    schedule_note_at(t, midi, slot, vol, dur, &o);
}

static void book_all(double t0) {
    // T1 0.5 s: a flute phrase, each note held 30 ms into the next (Playtime's legato length)
    static const int ph[4] = { 62, 64, 67, 69 };
    for (int i = 0; i < 4; i++) book(t0 + 0.5 + 0.4 * i, ph[i], FLUTE, 5, i < 3 ? 430 : 400, 80, 0, 0);
    // T2 3.0 s: 303 line, accents on steps 2 and 5, staccato
    static const int ac[8] = { 45, 45, 57, 45, 48, 45, 52, 45 };
    for (int i = 0; i < 8; i++) book(t0 + 3.0 + 0.25 * i, ac[i], ACID, 5, 150, 0, (i == 2 || i == 5) ? 1.0f : 0.0f, 0);
    // T3 5.5 s: the same line, step 3 held into step 4 and SLID (a 303 slide = legato + 60 ms glide)
    for (int i = 0; i < 8; i++) {
        int dur = (i == 3) ? 280 : 150;
        book(t0 + 5.5 + 0.25 * i, ac[i], ACID, 5, dur, i == 4 ? 60 : 0, (i == 2 || i == 5) ? 1.0f : 0.0f, 0);
    }
    // T4 8.0 s: mallet, plain / accented alternating
    for (int i = 0; i < 4; i++) book(t0 + 8.0 + 0.5 * i, 72, MAL, 5, 200, 0, (i & 1) ? 1.0f : 0.0f, 0);
    // T5 10.5 s: flute, plain / accented alternating, separate notes
    for (int i = 0; i < 4; i++) book(t0 + 10.5 + 0.5 * i, 74, FLUTE2, 5, 300, 0, (i & 1) ? 1.0f : 0.0f, 0);
    // T6 12.5 s: mallet with a per-note timbre OFFSET (softer, then harder)
    book(t0 + 12.5, 72, MAL, 5, 200, 0, 0, -0.3f);
    book(t0 + 13.0, 72, MAL, 5, 200, 0, 0,  0.0f);
    book(t0 + 13.5, 72, MAL, 5, 200, 0, 0, +0.5f);
}

void update(void) {
    if (!booked) { book_all(audio_time()); booked = 1; }
#ifdef DE_TRACE
    watch("variant", "%d", VARIANT);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("legatoprobe", 4, 4, CLR_WHITE);
    print(VARIANT == 0 ? "extras on" : VARIANT == 1 ? "zeroed NoteOpts" : "plain schedule_at", 4, 16, CLR_LIGHT_GREY);
}
