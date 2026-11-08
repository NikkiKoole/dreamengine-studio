/* de:meta
{
  "slug": "amb1",
  "title": "amb-1",
  "status": "active",
  "created": "2026-11-08",
  "kind": ["instrument", "toy"],
  "teaches": ["polymeter", "gesture-loop", "generative-melody"],
  "lineage": "A dreamengine take on AMB-1, one of the community firmwares for the M-VAVE FM-1 pocket synth (thefirmwarehub.com), itself inspired by Brian Eno's Music for Airports 2/1. Written from the idea, not the firmware's code. Face loosely after the FM-1: colour TFT, four knobs, a silicone keyboard.",
  "description": {
    "summary": "Seven tape loops of different lengths, each holding one note. They never line up the same way twice.",
    "detail": "Eno's trick for Music for Airports 2/1: record each sung note on its own tape loop, make every loop a different length (17.8 s, 20.1 s, 31.8 s...), and let them run. Because the lengths share no common beat, the notes keep drifting into new chords and the piece never exactly repeats. The screen shows each loop as a strip of tape whose length is to scale, with its playhead and the note on it; the bottom line works out when all seven will next be back where they started (usually millions of years). Play a key and the note is spliced onto the next loop at the point its playhead is at right now, so it comes back exactly there every lap.",
    "controls": "Keys (click, touch, or A..L / W..P on QWERTY, Z/X octave) play a note AND splice it onto the next loop. Knobs (drag up/down): VOICE choir > piano > glass, TAPE wow and flutter, SPACE reverb, SPEED how fast the loops turn (pitch stays). BACKSPACE wipes the tapes, R reloads the Eno chord, 1-7 mute a loop."
  }
}
de:meta */
#include "studio.h"
#define KEYBED_OCT_DOWN 'Z'
#define KEYBED_OCT_UP   'X'
#include "keybed.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// AMB-1: seven tape loops, seven lengths, one note each.
//
// The honest core is one line of arithmetic: a note on a loop of length L
// sounds at times p, p+L, p+2L, ... Give seven loops lengths with no shared
// beat and the CHORD you hear at any moment is a new combination. The bottom
// line of the screen proves it: the least common multiple of the lengths, the
// first moment every loop is back at its start together, in years.

#define NT 7
#define SL_CHOIR 5
#define SL_PIANO 6
#define SL_GLASS 7

// loop lengths in TENTHS of a second, so the LCM is exact integer maths.
// Roughly the lengths Eno is reported to have used for 2/1; all different,
// no two sharing a small common factor.
static const int LEN10[NT] = { 178, 201, 236, 259, 293, 318, 373 };
// the Eno-ish starting chord: an Ab-major colour, spread over two octaves
static const int ENO_NOTE[NT] = { 56, 60, 63, 65, 68, 72, 75 };
static const int TCOL[NT] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO, CLR_PINK };

typedef struct {
    int   note;      // midi, -1 = blank tape
    float pos;       // where on the loop the note is spliced, 0..1
    float glow;      // flash when it sounds
    int   voice;     // timbre it was recorded with
    bool  mute;
} Tape;

static Tape  loops[NT];
static double clock_s;        // tape time in seconds (advances by SPEED)
static int   next_tape;       // round-robin: which loop the next key lands on
static float kn[4] = { 0.10f, 0.35f, 0.60f, 0.33f };   // VOICE TAPE SPACE SPEED
static float kn_applied[4] = { -1, -1, -1, -1 };
static int   drag_knob = -1;
static float drag_y0, drag_v0;
static const char *KN_NAME[4] = { "VOICE", "TAPE", "SPACE", "SPEED" };
static const int   KN_COL[4]  = { CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_BLUE };

// layout
#define SCR_X 64
#define SCR_Y 8
#define SCR_W 192
#define SCR_H 100
#define KEY_Y 124

static int knob_x(int i) { return i < 2 ? 30 : 290; }
static int knob_y(int i) { return (i % 2) ? 84 : 36; }

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static int cur_voice(void) { return kn[0] < 0.34f ? 0 : kn[0] < 0.67f ? 1 : 2; }
static float speed(void) { return 0.5f * powf(4.0f, kn[3]); }   // 0.5x .. 2x, 1x at 0.5

static void sound_note(int midi, int voice, int vol) {
    if (voice == 0)      hit(midi, SL_CHOIR, vol, 2600);
    else if (voice == 1) hit(midi, SL_PIANO, vol, 5000);
    else                 hit(midi + 12, SL_GLASS, vol - 1, 3500);
}

static void load_eno(void) {
    for (int i = 0; i < NT; i++) {
        loops[i].note = ENO_NOTE[i];
        loops[i].pos = 0.07f + 0.13f * i;      // stagger so they don't all start at once
        loops[i].voice = 0;
        loops[i].glow = 0;
        loops[i].mute = false;
    }
    next_tape = 0;
}

// a key splices its note onto the next loop, at that loop's playhead NOW
static void on_key(int midi, int vel) {
    (void)vel;
    Tape *t = &loops[next_tape];
    double L = LEN10[next_tape] / 10.0;
    t->note = midi;
    t->pos = (float)(fmod(clock_s, L) / L);
    t->voice = cur_voice();
    t->glow = 1.0f;
    t->mute = false;
    next_tape = (next_tape + 1) % NT;
    sound_note(midi, t->voice, 6);
}

static uint64_t gcd64(uint64_t a, uint64_t b) { while (b) { uint64_t r = a % b; a = b; b = r; } return a; }

// first moment every UNMUTED loop with a note is back at its own start: LCM of the lengths
static double repeat_years(void) {
    uint64_t l = 1; int n = 0;
    for (int i = 0; i < NT; i++) {
        if (loops[i].note < 0 || loops[i].mute) continue;
        uint64_t v = (uint64_t)LEN10[i];
        l = l / gcd64(l, v) * v; n++;
    }
    if (n < 2) return -1;
    return (l / 10.0) / (365.25 * 24 * 3600);
}

static void apply_fx(void) {
    if (kn[1] != kn_applied[1]) { tape(0.15f + 0.6f * kn[1], 0.4f * kn[1], 0.3f + 0.3f * kn[1]); kn_applied[1] = kn[1]; }
    if (kn[2] != kn_applied[2]) {
        reverb(0.5f + 0.48f * kn[2], 0.55f);
        float s = 0.15f + 0.75f * kn[2];
        instrument_reverb(SL_CHOIR, s); instrument_reverb(SL_PIANO, s * 0.8f); instrument_reverb(SL_GLASS, s);
        kn_applied[2] = kn[2];
    }
}

void init(void) {
    // choir: a soft "aah" with a slow swell, like a held sung note on tape
    instrument(SL_CHOIR, INSTR_VOICE, 700, 400, 6, 2400);
    instrument_harmonics(SL_CHOIR, 0.50f);   // vowel A
    instrument_timbre(SL_CHOIR, 0.62f);      // a little smaller than a man
    instrument_morph(SL_CHOIR, 0.20f);       // soft effort
    // piano: 1/1 was a piano piece; a felted grand with the pedal down
    instrument(SL_PIANO, INSTR_PIANO, 2, 400, 5, 3000);
    instrument_harmonics(SL_PIANO, 0.08f);   // the grand
    instrument_timbre(SL_PIANO, 0.25f);      // soft felt
    instrument_morph(SL_PIANO, 0.85f);       // pedal
    // glass: an off-integer FM bell
    instrument(SL_GLASS, INSTR_FM, 3, 900, 2, 2600);
    instrument_harmonics(SL_GLASS, 0.62f);
    instrument_timbre(SL_GLASS, 0.30f);
    instrument_morph(SL_GLASS, 0.05f);

    keybed_config(SL_CHOIR, 3, 16);          // 16 whites from C3: 27 keys, like the FM-1
    keybed_layout(8, KEY_Y, 304, 72);
    keybed_manage_voices(false);
    keybed_on_note(on_key);
    load_eno();
}

static void knobs_update(void) {
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT)) {
        for (int i = 0; i < 4; i++) {
            int dx = mx - knob_x(i), dy = my - knob_y(i);
            if (dx * dx + dy * dy <= 16 * 16) { drag_knob = i; drag_y0 = my; drag_v0 = kn[i]; }
        }
    }
    if (drag_knob >= 0) {
        if (mouse_down(MOUSE_LEFT)) {
            float v = drag_v0 + (drag_y0 - my) / 80.0f;
            kn[drag_knob] = v < 0 ? 0 : v > 1 ? 1 : v;
        } else drag_knob = -1;
    }
    float w = mouse_wheel();
    if (w != 0) for (int i = 0; i < 4; i++) {
        int dx = mx - knob_x(i), dy = my - knob_y(i);
        if (dx * dx + dy * dy <= 16 * 16) { kn[i] += w * 0.03f; kn[i] = kn[i] < 0 ? 0 : kn[i] > 1 ? 1 : kn[i]; }
    }
}

void update(void) {
    knobs_update();
    if (drag_knob < 0) keybed_update();
    apply_fx();
    if (keyp(KEY_BACKSPACE)) { for (int i = 0; i < NT; i++) loops[i].note = -1; next_tape = 0; }
    if (keyp('R')) load_eno();
    for (int i = 0; i < NT; i++) if (keyp('1' + i)) loops[i].mute = !loops[i].mute;

    double prev = clock_s;
    clock_s += speed() / 60.0;
    for (int i = 0; i < NT; i++) {
        Tape *t = &loops[i];
        t->glow *= 0.94f;
        if (t->note < 0 || t->mute) continue;
        double L = LEN10[i] / 10.0, at = t->pos * L;
        // did the playhead cross the splice this frame? (count laps, so a wrap is caught)
        double a = floor((prev - at) / L), b = floor((clock_s - at) / L);
        if (b > a) { sound_note(t->note, t->voice, 5); t->glow = 1.0f; }
    }
#ifdef DE_TRACE
    watch("clock", "%.2f", clock_s);
#endif
}

// ── drawing ──────────────────────────────────────────────────────────────

static void draw_knob(int i) {
    int x = knob_x(i), y = knob_y(i);
    circfill(x, y + 2, 15, CLR_BLACK);
    circfill(x, y, 15, CLR_DARKER_GREY);
    circfill(x, y, 11, KN_COL[i]);
    circ(x, y, 15, drag_knob == i ? CLR_WHITE : CLR_DARK_GREY);
    float a = (0.75f + kn[i] * 1.5f) * 3.14159f;   // 7 o'clock .. 5 o'clock
    line(x + (int)(cosf(a) * 4), y + (int)(sinf(a) * 4), x + (int)(cosf(a) * 10), y + (int)(sinf(a) * 10), CLR_WHITE);
    font(FONT_SMALL);
    print_centered(KN_NAME[i], x, y + 18, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}

static void draw_screen(void) {
    rectfill(SCR_X - 3, SCR_Y - 3, SCR_W + 6, SCR_H + 6, CLR_BLACK);
    rectfill(SCR_X, SCR_Y, SCR_W, SCR_H, CLR_DARKER_BLUE);
    font(FONT_SMALL);
    print("AMB-1", SCR_X + 3, SCR_Y + 2, CLR_WHITE);
    char buf[48];
    int secs = (int)clock_s;
    snprintf(buf, sizeof buf, "%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
    print(buf, SCR_X + SCR_W - text_width(buf) - 3, SCR_Y + 2, CLR_LIGHT_GREY);

    // each loop is a strip of tape, drawn to scale: the longest fills the row
    int maxw = SCR_W - 34;
    for (int i = 0; i < NT; i++) {
        Tape *t = &loops[i];
        int y = SCR_Y + 11 + i * 11;
        int w = maxw * LEN10[i] / LEN10[NT - 1];
        int x = SCR_X + 22;
        int col = t->mute ? CLR_DARK_GREY : TCOL[i];
        snprintf(buf, sizeof buf, "%d", i + 1);
        print(buf, SCR_X + 3, y + 1, i == next_tape ? CLR_WHITE : CLR_DARK_GREY);
        if (i == next_tape) print(">", SCR_X + 9, y + 1, CLR_WHITE);
        rectfill(x, y + 2, w, 4, CLR_DARKER_GREY);
        double L = LEN10[i] / 10.0;
        int ph = x + (int)(w * fmod(clock_s, L) / L);
        if (t->note >= 0) {
            int nx = x + (int)(w * t->pos);
            int r = 2 + (int)(t->glow * 3);
            if (t->glow > 0.05f) circfill(nx, y + 4, r, col);
            rectfill(nx - 1, y + 1, 3, 6, col);
            snprintf(buf, sizeof buf, "%s%d", NN[t->note % 12], t->note / 12 - 1);
            print(buf, x + w + 3, y + 1, t->glow > 0.3f ? CLR_WHITE : col);
        }
        line(ph, y, ph, y + 8, CLR_WHITE);
    }
    double yrs = repeat_years();
    if (yrs < 0) snprintf(buf, sizeof buf, "play keys to splice notes on");
    else if (yrs >= 1e6) snprintf(buf, sizeof buf, "exact repeat in %.1f million years", yrs / 1e6);
    else if (yrs >= 1) snprintf(buf, sizeof buf, "exact repeat in %.0f years", yrs);
    else snprintf(buf, sizeof buf, "exact repeat in %.0f days", yrs * 365.25);
    print_centered(buf, SCR_X + SCR_W / 2, SCR_Y + SCR_H - 8, CLR_PEACH);
    font(FONT_NORMAL);
}

static void draw_keys(void) {
    int x, y, w, h, m;
    rectfill(4, KEY_Y - 4, 312, 80, CLR_BLACK);
    for (int i = 0; i < keybed_white_count(); i++) {
        keybed_white_rect(i, &x, &y, &w, &h);
        int midi = keybed_white_midi(i);
        float g = keybed_glow(midi);
        rectfill(x + 1, y, w - 2, h, keybed_held(midi) ? CLR_PEACH : CLR_LIGHT_GREY);
        if (g > 0.05f) rectfill(x + 1, y + h - 6, w - 2, 6, CLR_PINK);
        rectfill(x + 1, y + h - 2, w - 2, 2, CLR_MEDIUM_GREY);
    }
    for (int i = 0; i < keybed_white_count(); i++) {
        if (!keybed_black_rect(i, &x, &y, &w, &h, &m)) continue;
        rectfill(x, y, w, h, keybed_held(m) ? CLR_PINK : CLR_DARKER_GREY);
        rectfill(x + 1, y + h - 3, w - 2, 2, CLR_DARK_GREY);
    }
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rectfill(2, 2, SCREEN_W - 4, SCREEN_H - 4, CLR_DARK_GREY);
    rect(2, 2, SCREEN_W - 4, SCREEN_H - 4, CLR_BLACK);
    draw_screen();
    for (int i = 0; i < 4; i++) draw_knob(i);
    font(FONT_SMALL);
    print("BKSP wipe  R eno  1-7 mute  Z/X oct", 64, SCR_Y + SCR_H + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    draw_keys();
}
