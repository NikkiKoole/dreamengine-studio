/* de:meta
{
  "slug": "otamatone",
  "title": "Singing Stick",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Built as the teaching cart for the held-note API (note_pitch/note_cutoff live steering); models the Otamatone toy's reed-buzz + resonant-mouth character via a single sustained voice shaped every frame.",
  "description": "The little face-on-a-stem toy, built entirely from the HELD NOTE api. DRAG the vertical stem to play — one sustained voice (note_on) whose pitch chases your finger up and down the fretless ribbon (note_pitch + note_glide for the signature slurp). Hold SPACE (or right-click) to squeeze the mouth open: that sweeps a resonant lowpass from a muffled 'mm' to a bright 'aah' (note_cutoff), and the pixel mouth opens to match. Wiggle while you slide for hand vibrato. Q toggles fretless ↔ snap-to-pentatonic.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// Otamatone — the little face-on-a-stem toy, built entirely out of the new
// held-note API. The whole point: sound here is NOT fire-and-forget. You start
// ONE sustained voice with note_on(), then steer it live every frame —
//
//   * the vertical stem is a fretless pitch ribbon: drag your finger and
//     note_pitch() chases it (with note_glide for that signature slurp);
//   * the mouth is a lowpass filter: squeeze it open (SPACE / right-click) and
//     note_cutoff() sweeps up — closed = muffled "mm", open = bright "aah".
//
// Wiggle while you slide and the ribbon's glide turns it into hand vibrato. A
// fire-and-forget note() could never do any of this — that's the unlock.
//
// CONTROLS: drag the stem to play · hold SPACE (or right-click) to open the
// mouth · Q toggles fretless ↔ snap-to-pentatonic.

#define SLOT     5
#define STEM_X   160          // ribbon centre x
#define STEM_TOP 96           // y of the highest note
#define STEM_BOT 188          // y of the lowest note
#define MIDI_HI  72           // pitch at the top of the stem  (C5)
#define MIDI_LO  36           // pitch at the bottom           (C2)

static int   voice  = -1;     // held-note handle, -1 = silent
static float pitch  = 54;     // current target MIDI (float = between notes)
static float mouth  = 0;      // 0 closed .. 1 open, smoothed
static float squash = 0;      // head wobble on note-on
static int   snap   = 0;      // 0 = fretless, 1 = snap to pentatonic

static const char *NOTE_NAMES[12] =
    { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

// stem y → MIDI pitch (top = high). clamped so the finger can't slide off.
static float ribbon_to_midi(int y) {
    float t = clamp((float)(y - STEM_TOP) / (STEM_BOT - STEM_TOP), 0, 1);
    return lerp(MIDI_HI, MIDI_LO, t);     // top of stem is the high note
}

// nearest major-pentatonic pitch (C D E G A), searched ±6 semitones
static float snap_penta(float m) {
    int around = (int)floorf(m + 0.5f), best = around;
    float bd = 99;
    for (int c = around - 6; c <= around + 6; c++) {
        int pc = ((c % 12) + 12) % 12;
        if (pc==0 || pc==2 || pc==4 || pc==7 || pc==9) {
            float d = fabsf((float)c - m);
            if (d < bd) { bd = d; best = c; }
        }
    }
    return (float)best;
}

static int over_stem(void) {
    return mouse_x() >= STEM_X - 14 && mouse_x() <= STEM_X + 14 &&
           mouse_y() >= STEM_TOP - 8 && mouse_y() <= STEM_BOT + 8;
}

void init(void) {
    // a thin, nasal pulse through a resonant lowpass — the otamatone reed buzz.
    instrument(SLOT, INSTR_SQUARE, 8, 90, 6, 130);   // soft attack, real sustain
    instrument_duty(SLOT, 0.12f);                    // thin = nasal
    instrument_filter(SLOT, FILTER_LOW, 500, 9);     // start muffled, resonant peak
    instrument_lfo(SLOT, 0, LFO_PITCH, 6.0f, 0.22f); // gentle ever-present warble
}

void update(void) {
    if (keyp('Q')) snap = !snap;

    // ---- start / stop the single held voice ----
    if (mouse_pressed(MOUSE_LEFT) && over_stem() && voice < 0) {
        voice = note_on((int)ribbon_to_midi(mouse_y()), SLOT, 6);
        note_glide(voice, 45);          // slide between notes, don't retrigger
        squash = 1.0f;                  // little "boing" on touch
    }
    if (mouse_released(MOUSE_LEFT) && voice >= 0) {
        note_off(voice);
        voice = -1;
    }

    // mouth opens with SPACE or right-click; smoothed so it sweeps, not snaps
    int squeeze = key(KEY_SPACE) || mouse_down(MOUSE_RIGHT);
    mouth = lerp(mouth, squeeze ? 1.0f : 0.0f, 0.25f);

    // ---- drive the live voice every frame ----
    if (voice >= 0) {
        pitch = ribbon_to_midi(mouse_y());      // finger stays down even off-stem-x
        if (snap) pitch = snap_penta(pitch);
        note_pitch(voice, pitch);
        note_cutoff(voice, (int)lerp(450, 4200, mouth));   // mouth → brightness
    }

    squash = lerp(squash, 0.0f, 0.18f);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    int playing = voice >= 0;

    // ---- the stem / pitch ribbon ----
    rectfill(STEM_X - 9, STEM_TOP, 18, STEM_BOT - STEM_TOP, CLR_DARKER_GREY);
    rect(STEM_X - 9, STEM_TOP, 18, STEM_BOT - STEM_TOP, CLR_DARK_GREY);
    // octave ticks down the side
    for (int m = MIDI_LO; m <= MIDI_HI; m += 12) {
        float t = (float)(MIDI_HI - m) / (MIDI_HI - MIDI_LO);
        int y = (int)lerp(STEM_TOP, STEM_BOT, t);
        line(STEM_X + 10, y, STEM_X + 14, y, CLR_DARK_GREY);
    }
    // pentatonic frets when snapping
    if (snap) {
        for (int m = MIDI_LO; m <= MIDI_HI; m++) {
            int pc = m % 12;
            if (pc==0||pc==2||pc==4||pc==7||pc==9) {
                float t = (float)(MIDI_HI - m) / (MIDI_HI - MIDI_LO);
                int y = (int)lerp(STEM_TOP, STEM_BOT, t);
                line(STEM_X - 8, y, STEM_X + 8, y, CLR_INDIGO);
            }
        }
    }
    // the glowing finger marker
    if (playing) {
        float t = clamp((float)(MIDI_HI - pitch) / (MIDI_HI - MIDI_LO), 0, 1);
        int y = (int)lerp(STEM_TOP, STEM_BOT, t);
        circfill(STEM_X, y, 5, CLR_YELLOW);
        circfill(STEM_X, y, 2, CLR_WHITE);
    }

    // ---- the head ----
    int sq = (int)(squash * 4);                 // squash: wider+shorter on touch
    int cx = STEM_X, cy = 56 + sq / 2;
    int rx = 34 + sq, ry = 34 - sq;
    ovalfill(cx, cy, rx, ry, playing ? CLR_WHITE : CLR_LIGHT_GREY);
    // cheeks blush a little when the mouth is open
    if (mouth > 0.15f) {
        ovalfill(cx - 22, cy + 6, 5, 4, CLR_PEACH);
        ovalfill(cx + 22, cy + 6, 5, 4, CLR_PEACH);
    }

    // eyes — pupils glance upward as the pitch climbs
    float pt = clamp((pitch - MIDI_LO) / (MIDI_HI - MIDI_LO), 0, 1);
    int gaze = (int)lerp(4, -4, pt);            // high note → look up
    ovalfill(cx - 13, cy - 8, 8, 9, CLR_WHITE);
    ovalfill(cx + 13, cy - 8, 8, 9, CLR_WHITE);
    circfill(cx - 13, cy - 8 + gaze, 3, CLR_BLACK);
    circfill(cx + 13, cy - 8 + gaze, 3, CLR_BLACK);

    // mouth — closed = a slit, open = a singing oval; height tracks the filter
    int my0 = cy + 16;
    int mh = 2 + (int)(mouth * 13);
    ovalfill(cx, my0, 9, mh, CLR_DARK_RED);
    if (mouth > 0.45f) ovalfill(cx, my0 + mh / 3, 4, mh / 3, CLR_PINK);  // tongue
    line(cx - 9, my0, cx + 9, my0, CLR_BROWNISH_BLACK);

    // vowel label that tracks the mouth-as-filter
    const char *vowel = mouth < 0.2f ? "mm" : mouth < 0.55f ? "ooh" : "aah";
    if (playing) print_centered(vowel, cx, my0 + 22, CLR_LIGHT_PEACH);

    // ---- HUD ----
    print_centered("OTAMATONE", STEM_X, 6, CLR_PEACH);
    if (playing) {
        int m = (int)(pitch + 0.5f);
        char buf[8]; const char *nm = NOTE_NAMES[((m % 12) + 12) % 12];
        // tiny "C4"-style readout
        buf[0] = nm[0];
        int i = 1; if (nm[1]) buf[i++] = nm[1];
        buf[i++] = '0' + (m / 12 - 1); buf[i] = 0;
        print_centered(buf, STEM_X, 84, CLR_YELLOW);
    }
    print("drag stem: play", 4, 178, CLR_MEDIUM_GREY);
    print(snap ? "Q: pentatonic" : "Q: fretless", 4, 188, CLR_DARK_GREY);
    print_centered("SPACE = open mouth", 250, 188, CLR_MEDIUM_GREY);
}
