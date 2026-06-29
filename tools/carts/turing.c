/* de:meta
{
  "title": "Turing",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "shift-register",
    "generative-sequencer",
    "scale-quantize"
  ],
  "homage": "Music Thing Modular Turing Machine",
  "description": "A generative sequencer, after the Music Thing Modular Turing Machine — the whole instrument is ONE looping SHIFT REGISTER and ONE big CHAOS knob. Each clock the register rotates and the recirculated bit is flipped with some probability: knob full RIGHT = 0% flip = the loop is LOCKED and repeats forever; knob CENTRE = 50% = pure RANDOM, a new pattern every bar; knob full LEFT = 100% = FLIP-LOCK, a locked loop of double the length. Park it just shy of locked and the riff slowly mutates — you sculpt a sequence by hand. The register's value is read out as a scale-locked PITCH (so it never sours) and two of its bits trigger a KICK and a HAT, so melody AND groove are the same evolving pattern. The 16 LEDs are the live register — TAP a cell to flip that bit by hand; a SEQUENCE strip graphs the recent notes so you watch the loop lock or drift. The CHAOS knob (or ← / →) rides lock↔random↔flip; LENGTH (↑ / ↓) sets loop length, RANGE the pitch span, TEMPO the clock; 1-4 pick the scale (min/maj pentatonic, dorian, aeolian); SPACE = SCRAMBLE a fresh pattern. A liveset plaything built on a cart-side shift register + an INSTR_SAW Berlin-school voice."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// TURING — a generative sequencer, after the Music Thing Modular Turing Machine.
//
// The whole instrument is ONE looping SHIFT REGISTER and ONE big CHAOS knob. Each clock
// the register rotates and the recirculated bit is flipped with some probability:
//   • knob full RIGHT  → 0% flip  → the loop is LOCKED, repeats forever
//   • knob CENTRE      → 50% flip → pure RANDOM, a new pattern every bar
//   • knob full LEFT   → 100% flip → FLIP-LOCK, a locked loop of double the length
// Park it just shy of locked and the riff slowly mutates — you sculpt a sequence by hand.
//
// The register's value is read out as a PITCH (scale-locked, so it never sours), and two of
// its bits trigger a KICK and a HAT — so melody AND groove are the same evolving pattern.
//
//   • the CHAOS knob (or ← / →) — lock ↔ random ↔ flip   ·  TAP a register cell to flip that bit
//   • LENGTH (or ↑ / ↓) loop length · RANGE pitch span · TEMPO · 1-4 scale · SPACE = SCRAMBLE
//
//   Built on a cart-side shift register + INSTR_SAW sequence voice. A liveset plaything;
//   pairs naturally with grenadier as the voice (cart-library-direction §2c).

enum { SL_LEAD = 5, SL_KICK, SL_HAT };

// ── scales (semitones from root) — all consonant so the random walk never sours ──
static const int SC_MINPENT[] = { 0, 3, 5, 7, 10 };
static const int SC_MAJPENT[] = { 0, 2, 4, 7, 9 };
static const int SC_DORIAN [] = { 0, 2, 3, 5, 7, 9, 10 };
static const int SC_AEOL   [] = { 0, 2, 3, 5, 7, 8, 10 };
typedef struct { const int *s; int n; const char *name; } Scale;
static const Scale SCALES[4] = {
    { SC_MINPENT, 5, "MIN PENT" }, { SC_MAJPENT, 5, "MAJ PENT" },
    { SC_DORIAN,  7, "DORIAN"   }, { SC_AEOL,    7, "AEOLIAN"  },
};
#define BASE 45                                   // A2 — a low, motorik root

static const int LENS[] = { 3, 4, 5, 6, 8, 12, 16 };
#define NLEN 7

static unsigned reg = 0xA6C5;        // seed — a lively starting pattern
static int   scale_idx = 0;
static float k_chaos = 0.84f;        // start just shy of locked (slowly evolving)
static float k_len   = 6.0f / 6.0f;  // → index into LENS (start at 16)
static float k_range = 0.5f;         // → 1..3 octaves
static float k_tempo = (120 - 80) / 80.0f;   // → 80..160 BPM

static int   last_8 = -1, gstep = -1, tempo = 120;
static int   cur_note = BASE, kflash = -100, hflash = -100, lflash = -100;

#define HN 72                        // pitch-history bars
static float hist[HN];
static bool  histlead[HN];
static int   hpos = 0;

// register LED row
#define LX 12
#define LY 22
#define LW 18
#define LH 22

static int active_len(void) { return LENS[(int)(k_len * (NLEN - 0.001f))]; }
static int octaves(void)    { return 1 + (int)(k_range * 2 + 0.5f); }

// CHAOS knob → flip probability. right = lock (0), centre = random (.5), left = flip-lock (1).
static float flip_prob(void) {
    if (k_chaos >= 0.95f) return 0.0f;
    if (k_chaos <= 0.05f) return 1.0f;
    return 1.0f - k_chaos;
}
static const char *chaos_name(float p) {
    if (p <= 0.0f)  return "LOCKED";
    if (p >= 1.0f)  return "FLIP-LOCK";
    if (p < 0.22f)  return "evolving";
    if (p > 0.78f)  return "wild";
    return "RANDOM";
}

static int scale_note(int d) {
    const Scale *S = &SCALES[scale_idx];
    return BASE + 12 * (d / S->n) + S->s[d % S->n];
}

static void clock_step(void) {
    int len = active_len();
    unsigned mask = (1u << len) - 1u;
    int fb = (reg >> (len - 1)) & 1;
    float p = flip_prob();
    if (p >= 1.0f || (p > 0.0f && rnd(1000) < (int)(p * 1000)))   // flip the recirculated bit
        fb ^= 1;
    reg = ((reg << 1) | fb) & mask;

    // read the register as a value → scale degree → pitch
    float v = reg / (float)mask;
    int span = octaves() * SCALES[scale_idx].n;
    int d = (int)(v * span); if (d >= span) d = span - 1;
    cur_note = scale_note(d);
    int leaddur = (int)(60000.0f / tempo / 2 * 0.85f);          // ~ one eighth note
    hit(cur_note, SL_LEAD, 4, leaddur);
    lflash = frame();

    if (reg & 1)        { hit(36, SL_KICK, 6, 220); kflash = frame(); }   // bit 0 → kick
    if ((reg >> 2) & 1) { hit(92, SL_HAT, 3, 28);   hflash = frame(); }   // bit 2 → hat

    hist[hpos] = v; histlead[hpos] = true; hpos = (hpos + 1) % HN;
    gstep++;
}

static void scramble(void) { reg = (unsigned)(rnd(65536)); }

void init(void) {
    instrument(SL_LEAD, INSTR_SAW, 0, 150, 0, 120);
    instrument_filter(SL_LEAD, FILTER_LOW, 1700, 6);            // a resonant Berlin-school bleep
    instrument_drive(SL_LEAD, 0.15f);
    instrument(SL_KICK, INSTR_SINE, 0, 250, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);
    instrument_drive(SL_KICK, 0.26f);
    instrument(SL_HAT, INSTR_NOISE, 0, 28, 0, 16); instrument_filter(SL_HAT, FILTER_HIGH, 7000, 2);
    // pre-fill the SEQUENCE strip with the seed's locked loop, so it shows the riff from frame 0
    unsigned r = reg; int len = active_len(); unsigned mask = (1u << len) - 1u;
    for (int i = 0; i < HN; i++) {
        int fb = (r >> (len - 1)) & 1; r = ((r << 1) | fb) & mask;
        hist[i] = r / (float)mask; histlead[i] = true;
    }
    hpos = 0;
}

void update(void) {
    if (keyp(KEY_SPACE)) scramble();
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) scale_idx = i;
    if (btnp(0, BTN_LEFT))  k_chaos = clamp(k_chaos - 0.05f, 0, 1);
    if (btnp(0, BTN_RIGHT)) k_chaos = clamp(k_chaos + 0.05f, 0, 1);
    if (btnp(0, BTN_UP))    k_len = clamp(k_len + 1.0f / (NLEN - 1), 0, 1);
    if (btnp(0, BTN_DOWN))  k_len = clamp(k_len - 1.0f / (NLEN - 1), 0, 1);

    tempo = 80 + (int)(k_tempo * 80 + 0.5f);
    bpm(tempo);

    // transport: clock the register on every EIGHTH note
    float pos8 = beat() * 2 + beat_pos() * 2.0f;
    int eighth = (int)pos8;
    if (eighth != last_8) { last_8 = eighth; clock_step(); }

#ifdef DE_TRACE
    watch("chaos",  "%s", chaos_name(flip_prob()));
    watch("len",    "%d", active_len());
    watch("reg",    "%d", (int)reg);
    watch("note",   "%d", cur_note);
    watch("tempo",  "%d", tempo);
#endif
}

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int len = active_len();
    float p = flip_prob();

    print("TURING", 2, 4, CLR_LIGHT_YELLOW);
    print(str("%s  %s%d", chaos_name(p), NN[cur_note % 12], cur_note / 12 - 1), 64, 4,
          p <= 0 ? CLR_GREEN : (p >= 1 ? CLR_PINK : CLR_LIGHT_GREY));
    print_right(str("%d BPM", tempo), 318, 4, CLR_LIGHT_GREY);

    // ── the shift-register LED row (the hero) — tap a cell to flip that bit ──
    for (int i = 0; i < 16; i++) {
        int x = LX + i * LW, bit = (reg >> i) & 1;
        bool in_loop = (i < len);
        bool fbk = (i == len - 1);                              // the recirculated feedback bit
        if (in_loop && tapp(x, LY, LW - 2, LH)) reg ^= (1u << i);
        int col = !in_loop ? CLR_DARKER_GREY
                : bit      ? (fbk ? CLR_WHITE : CLR_LIGHT_YELLOW)
                           : CLR_DARK_GREY;
        rectfill(x, LY, LW - 2, LH, col);
        if (fbk) rect(x - 1, LY - 1, LW, LH + 2, CLR_PINK);     // mark the feedback tap
        if (!in_loop) { line(x, LY, x + LW - 2, LY + LH, CLR_DARKER_GREY); }
    }
    font(FONT_SMALL);
    print("SHIFT REGISTER  (tap a bit to flip)", LX, LY + LH + 2, CLR_DARK_GREY);
    print(str("loop %d", len), LX + 240, LY + LH + 2, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // ── pitch-history strip — watch the melody lock or drift ──
    int SY = 58, SH = 60, SW = 296;
    rect(LX, SY, SW, SH, CLR_DARKER_GREY);
    for (int j = 0; j < HN; j++) {
        int idx = (hpos + j) % HN;                              // oldest → newest, left → right
        int bw = SW / HN;
        int x = LX + j * bw;
        int h = (int)(hist[idx] * (SH - 4));
        bool newest = (idx == (hpos - 1 + HN) % HN);
        if (histlead[idx])
            rectfill(x + 1, SY + SH - 2 - h, bw - 1, h + 1, newest ? CLR_WHITE : CLR_BLUE);
    }
    font(FONT_SMALL); print("SEQUENCE", LX + 2, SY + 2, CLR_DARK_GREY); font(FONT_NORMAL);

    // ── beat lamps: KICK / HAT ──
    bool kon = (frame() - kflash) < 6, hon = (frame() - hflash) < 6;
    circfill(LX + 6,  SY + SH + 12, 5, kon ? CLR_RED : CLR_DARK_GREY);
    print("KICK", LX + 14, SY + SH + 8, kon ? CLR_RED : CLR_DARK_GREY);
    circfill(LX + 56, SY + SH + 12, 4, hon ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print("HAT", LX + 64, SY + SH + 8, hon ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);

    // ── the knob rack ──
    ui_begin();
    int ky = 162;
    font(FONT_SMALL);
    ui_knob(&k_chaos, 28,  ky, "CHAOS");
    ui_knob(&k_len,   86,  ky, "LENGTH");
    ui_knob(&k_range, 138, ky, "RANGE");
    ui_knob(&k_tempo, 190, ky, "TEMPO");
    // scale selector
    for (int i = 0; i < 4; i++) {
        int bx = 234, by = 146 + i * 12;
        if (ui_button(bx, by, 82, 11, SCALES[i].name)) scale_idx = i;
    }
    print("<-/-> chaos  up/dn len  SPACE scramble", LX, 189, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
