/* de:meta
{
  "slug": "glidescale",
  "title": "glide scale",
  "status": "active",
  "created": "2026-07-30",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "Demo for note_glide_scale (synth-secrets audit §B1) — the GLIDE SCALE switch a real synth panel exposes, made audible by a phrase that deliberately alternates small steps with big leaps.",
  "description": "The GLIDE SCALE switch, made audible. One monophonic voice slides between notes, and the phrase deliberately alternates tiny steps with two-octave leaps — because that contrast is the whole point of the switch. CONSTANT (the default) gives every slide the same duration, so a semitone step sounds sluggish while a two-octave leap whooshes past. PER OCTAVE makes the time proportional to the distance, so small moves are crisp and big leaps take a long, deliberate swoop. Tap S (or the button) to flip it mid-phrase and hear the same notes change character. GLIDE sets the time (LEFT/RIGHT, or drag the bar), SPACE stops the auto-phrase so you can play the eight pads yourself with Z X C V B N M comma, and the readout prints what each upcoming slide will actually cost in ms."
}
de:meta */
#include "studio.h"
#include <math.h>   // expf: the viz draws the ENGINE's exact ease curve, not an approximation of it

// GLIDE SCALE — hearing note_glide_scale(). One mono voice, note_pitch to move it, and a phrase
// built so the switch has something to bite on.
//
// WHY THE PHRASE ALTERNATES STEPS AND LEAPS: with GLIDE_CONSTANT every slide takes the same time
// whatever the distance, so a semitone crawls and a two-octave leap is a blur. With GLIDE_PER_OCT
// the time is proportional to the interval, so the small moves get crisp and the leaps get a real
// swoop. Play only steps, or only leaps, and the switch does almost nothing you can hear — the
// contrast IS the demo.
//
//   note_glide      (handle, ms)                  how long a slide takes
//   note_glide_scale(handle, GLIDE_CONSTANT)      ms = the whole slide, any interval (default)
//   note_glide_scale(handle, GLIDE_PER_OCT)       ms = the time per OCTAVE; total = ms x octaves
//
//   S / button   flip the scale        ·   LEFT/RIGHT or drag the bar   glide time
//   SPACE        auto-phrase on/off    ·   Z X C V B N M ,              play the pads yourself

#define SLOT 5
#define NPAD 8

// The phrase: steps and leaps on purpose. 48->50 step, 50->72 nearly two octaves, and so on.
static const int PHRASE[NPAD] = { 48, 50, 72, 50, 48, 47, 36, 48 };
static const char *KEYS = "ZXCVBNM,";

static int   voice = -1;
// GLIDE SCALE is a DIAL, not a switch, and the middle stop is the interesting one: neither extreme is
// what vintage hardware does. Three named stops on it, plus UP/DOWN for anywhere in between.
static const float STOP_AMT[3]  = { GLIDE_CONSTANT, GLIDE_ANALOG, GLIDE_PER_OCT };
static const char *STOP_NAME[3] = { "CONSTANT", "ANALOG", "PER OCT" };
static int   stop = 0;            // which named stop we last landed on (-1 once UP/DOWN moves off one)
static float scale_amt = GLIDE_CONSTANT;
static int   glide_ms = 300;
static bool  autoplay = true;
static int   pos = 0;             // phrase index
static int   cur = 48;            // the note sounding now
static int   prev = 48;           // where the current slide came from
static float slide_t = 1.0f;      // 0..1 progress through the current slide, for the viz
static float glow[NPAD];

// What the NEXT slide from `from` to `to` will actually cost, in ms. Mirrors the engine's law exactly:
// gl_len = ms * |octaves|^amount. Note the pivot that makes the dial coherent — an octave is |oct|=1 and
// 1^anything is 1, so `ms` is the time for a one-octave slide at EVERY setting.
static int slide_cost(int from, int to) {
    int semis = to - from; if (semis < 0) semis = -semis;
    if (semis == 0) return 5;                       // nothing to travel: the declick floor
    if (scale_amt <= 0.0f) return glide_ms;         // constant — distance is ignored
    float oct = (float)semis / 12.0f;
    int ms = (int)((float)glide_ms * powf(oct, scale_amt));
    return ms < 5 ? 5 : ms;                         // the engine floors a scaled glide at the declick length
}

static void apply_scale(void) {
    if (voice >= 0) note_glide_scale(voice, scale_amt);
}

static void start_voice(void) {
    voice = note_on(cur, SLOT, 6);
    note_glide(voice, glide_ms);
    apply_scale();
}

static void go_to(int midi) {
    prev = cur; cur = midi;
    slide_t = 0.0f;
    if (voice < 0) { start_voice(); return; }
    note_glide(voice, glide_ms);      // re-assert: the time knob may have moved since the last slide
    apply_scale();
    note_pitch(voice, (float)midi);
}

void init(void) {
    // a saw with a resonant lowpass — a bright, harmonically rich voice so a pitch slide is easy to
    // follow by ear. Long release so a leap's tail rings rather than clipping off.
    instrument(SLOT, INSTR_SAW, 8, 0, 7, 400);
    instrument_filter(SLOT, FILTER_LOW, 2200, 10);
    bpm(120);
    start_voice();
}

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    // S steps through the three named stops; UP/DOWN dials anywhere in between
    if (keyp('S')) { stop = (stop < 0 ? 0 : stop + 1) % 3; scale_amt = STOP_AMT[stop]; apply_scale(); }
    if (key(KEY_UP) || key(KEY_DOWN)) {
        scale_amt = clamp(scale_amt + (key(KEY_UP) ? 0.01f : -0.01f), 0.0f, 1.5f);
        stop = -1;                                  // no longer sitting on a named stop
        for (int i = 0; i < 3; i++) if (fabsf(scale_amt - STOP_AMT[i]) < 0.005f) stop = i;
        apply_scale();
    }
    if (keyp(KEY_SPACE)) autoplay = !autoplay;
    if (key(KEY_LEFT))  glide_ms = glide_ms > 20  ? glide_ms - 4 : 20;
    if (key(KEY_RIGHT)) glide_ms = glide_ms < 900 ? glide_ms + 4 : 900;

    // the SCALE button — also steps the stops
    if (mouse_pressed(MOUSE_LEFT) && mx >= SCREEN_W - 84 && mx < SCREEN_W - 8 && my >= 4 && my < 18) {
        stop = (stop < 0 ? 0 : stop + 1) % 3; scale_amt = STOP_AMT[stop]; apply_scale();
    }
    // drag the glide-time bar
    if (mouse_down(MOUSE_LEFT) && my >= 30 && my < 42 && mx >= 60 && mx < 60 + 200)
        glide_ms = 20 + (mx - 60) * 880 / 200;

    // play the pads yourself
    for (int i = 0; i < NPAD; i++) {
        int px = 12 + i * ((SCREEN_W - 24) / NPAD);
        int pw = (SCREEN_W - 24) / NPAD - 4;
        bool hit = mouse_pressed(MOUSE_LEFT) && mx >= px && mx < px + pw && my >= SCREEN_H - 46 && my < SCREEN_H - 18;
        if (keyp(KEYS[i]) || hit) { autoplay = false; go_to(PHRASE[i]); glow[i] = 1.0f; }
    }

    if (autoplay && every(2)) { pos = (pos + 1) % NPAD; go_to(PHRASE[pos]); glow[pos] = 1.0f; }

    // advance the slide viz at the rate the ENGINE is actually using
    int cost = slide_cost(prev, cur);
    if (cost > 0) slide_t += (1000.0f / 60.0f) / (float)cost;
    if (slide_t > 1.0f) slide_t = 1.0f;
    for (int i = 0; i < NPAD; i++) if (glow[i] > 0) glow[i] -= 0.04f;

#ifdef DE_TRACE
    watch("amt", "%.2f", scale_amt);
    watch("semi", "%d", slide_cost(0, 1));      // the three costs the dial spreads apart
    watch("oct",  "%d", slide_cost(0, 12));     // …and the octave, which must stay at glide_ms
    watch("oct3", "%d", slide_cost(0, 36));
    watch("glide_ms", "%d", glide_ms);
    watch("cost", "%d", cost);
    watch("cur", "%d", cur);
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("GLIDE SCALE", 8, 6, CLR_WHITE);

    // the SCALE readout — the dial, its position, and which named stop it is on (if any)
    bool named = stop >= 0;
    int bx = SCREEN_W - 84;
    rectfill(bx, 4, 76, 14, scale_amt > 0.0f ? CLR_ORANGE : CLR_DARKER_GREY);
    rect(bx, 4, 76, 14, CLR_DARK_GREY);
    font(FONT_SMALL);
    print(named ? str("S: %s", STOP_NAME[stop]) : str("S: %.2f", scale_amt),
          bx + 6, 8, scale_amt > 0.0f ? CLR_BLACK : CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // glide time bar
    print("GLIDE", 12, 32, CLR_INDIGO);
    rectfill(60, 30, 200, 12, CLR_DARKER_PURPLE);
    rectfill(60, 30, (glide_ms - 20) * 200 / 880, 12, CLR_PINK);
    rect(60, 30, 200, 12, CLR_DARK_GREY);
    // "/oct" is true at EVERY scale amount — the octave is the pivot. FONT_SMALL so it fits (ui-audit).
    font(FONT_SMALL);
    print(str("%dms/oct", glide_ms), 264, 33, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);

    // THE POINT, in numbers. The SPREAD from a semitone to three octaves is the whole feature, and it is
    // the thing the dial moves: 1x at CONSTANT, ~2x at ANALOG, 36x at PER OCT.
    int semis = cur - prev; if (semis < 0) semis = -semis;
    int lo = slide_cost(0, 1), mid = slide_cost(0, 12), hi = slide_cost(0, 36);
    font(FONT_SMALL);
    if (semis > 0)
        print(str("last move: %d semitones took %dms", semis, slide_cost(prev, cur)), 12, 50, CLR_LIGHT_GREY);
    else
        print("no move yet - the phrase is about to step", 12, 50, CLR_MEDIUM_GREY);
    // Always true, so it reads in a still too. An octave is ALWAYS `ms` — that is the pivot.
    print(str("1 semi %dms  |  1 oct %dms  |  3 oct %dms   spread %.1fx",
              lo, mid, hi, (float)hi / (float)(lo > 0 ? lo : 1)),
          12, 62, scale_amt > 0.0f ? CLR_ORANGE : CLR_INDIGO);
    font(FONT_NORMAL);

    // pitch ribbon: prev -> cur, with a head that moves on the engine's real timing
    int y0 = 76, y1 = 132, x0 = 12, x1 = SCREEN_W - 12;
    line(x0, y1, x1, y1, CLR_DARKER_GREY);
    for (int i = 0; i < NPAD; i++) {
        int yy = y1 - (y1 - y0) * (PHRASE[i] - 36) / 36;
        line(x0, yy, x0 + 3, yy, CLR_DARKER_GREY);
    }
    int ya = y1 - (y1 - y0) * (prev - 36) / 36;
    int yb = y1 - (y1 - y0) * (cur  - 36) / 36;
    line(x0, ya, x1, ya, CLR_DARK_PURPLE);
    line(x0, yb, x1, yb, CLR_DARK_PURPLE);
    // the ENGINE's own ease curve, (1 - e^-5t)/(1 - e^-5) — not ease_out(), so the head on screen
    // lands at the same instant the pitch does instead of merely looking similar
    float e = (1.0f - expf(-5.0f * slide_t)) * 1.0067837f;
    int yh = ya + (int)((yb - ya) * e);
    int xh = x0 + (int)((x1 - x0) * slide_t);
    line(x0, yh, xh, yh, CLR_PINK);
    circfill(xh, yh, 3, CLR_WHITE);
    print(str("note %d", cur), x1 - 48, yb - 10, CLR_LIGHT_YELLOW);

    // pads
    for (int i = 0; i < NPAD; i++) {
        int px = 12 + i * ((SCREEN_W - 24) / NPAD);
        int pw = (SCREEN_W - 24) / NPAD - 4;
        bool lit = glow[i] > 0;
        rectfill(px, SCREEN_H - 46, pw, 28, lit ? CLR_PINK : CLR_DARK_PURPLE);
        rect(px, SCREEN_H - 46, pw, 28, CLR_INDIGO);
        font(FONT_SMALL);
        print(str("%c", KEYS[i]), px + pw / 2 - 2, SCREEN_H - 40, lit ? CLR_BLACK : CLR_LIGHT_GREY);
        print(str("%d", PHRASE[i]), px + 2, SCREEN_H - 28, lit ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    font(FONT_SMALL);
    // one line, not two — the second overlapped the first (ui-audit)
    print(autoplay ? "SPACE: phrase on - steps AND leaps on purpose"
                   : "SPACE: off - play Z X C V B N M , yourself",
          12, SCREEN_H - 12, autoplay ? CLR_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);
}
