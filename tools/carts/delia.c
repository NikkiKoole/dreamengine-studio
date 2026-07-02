/* de:meta
{
  "title": "delia",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "drum-synthesis",
    "polymeter",
    "per-instrument-fx-bus"
  ],
  "lineage": "Delia Derbyshire's tape technique at the BBC Radiophonic Workshop (1960s): rhythm from tape loops of UNEQUAL length phasing against each other, found-sound percussion (the struck lampshade), splice timing. Increment 0 of docs/design/radiophonic-workshop.md - the concrete tape-kit proof cart, built standalone first like tb303 was before acidrack.",
  "homage": "Delia Derbyshire / BBC Radiophonic Workshop",
  "todo": [
    "touch pass: cells are 8px - fat-finger pads for phone play",
    "spec(): assert the polymetric schedule (lane phases, lcm cycle)",
    "autosave lanes via save_bytes once the format settles",
    "varispeed ribbon (the tape-slowdown gesture)",
    "candidate: audition clip + park a seed at tools/clips/delia/"
  ],
  "description": {
    "summary": "Five tape loops of DIFFERENT lengths phasing against each other - rhythm the way Delia Derbyshire actually made it. No two bars ever quite repeat.",
    "detail": "A concrete tape-kit: each lane is its own loop with its own length (16-step thud against a 5-step ring-modded lampshade against 7-step BBC pips against 11-step splice clicks against a 32-step reversed swell). All lanes tick at the same sixteenth-note rate but wrap at different points, so the playheads visibly drift apart and realign - the polymetry IS the interface. Voices are musique-concrete recipes: a dead damped thud, a struck bar through a ring modulator (carrier on the RING knob), 1kHz time pips (step 0 of the pips loop is the LONG pip, like the real ones), filtered noise splice-ticks, and a slow-attack noise swell cut dead - the backwards-tape cymbal. Each seed also bakes constant per-step micro-offsets (identical every cycle - a physical splice, not humanize). WOW/FLUT ride the master tape so the whole groove breathes.",
    "controls": "SPACE run/stop . tap cells to toggle (drag paints) . tap a lane name to audition . M box mutes . < > beside a row change its loop length . [ ] previous/next seed . UP/DOWN tempo . knobs: RING (lampshade carrier) WOW FLUT (tape) . keys 1-5 fire the voices"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── DELIA ─────────────────────────────────────────────────────────────────
// Rhythm the Radiophonic way: not a drum kit, tape loops of UNEQUAL length
// phasing against each other (Delia Derbyshire's actual working method).
// Every lane ticks at the same 16th-note rate but wraps at its own length,
// so the composite pattern only repeats at the lcm of all lanes - the
// groove never quite comes back around. Proof cart for the polymetric-lane
// + ring-mod-as-voice + splice-offset ideas in radiophonic-workshop.md.

#define MAXS 32
enum { L_THUD, L_LAMP, L_PIPS, L_TICK, L_SWELL, NL };
enum { SL_THUD = 9, SL_LAMP, SL_PIPS, SL_TICK, SL_SWELL };

typedef struct {
    const char   *name;
    int           len;            // this loop's length in 16th steps (2..MAXS)
    int           mute;
    unsigned char pat[MAXS];      // 1 = hit
    unsigned char splice[MAXS];   // fixed per-step delay ms (the tape splice)
    int           flash;
    int           color;
} Lane;

static Lane     lanes[NL];
static unsigned seed = 0x0DE11A01u;
static unsigned rng;
static int      tempo   = 96;
static bool     running = false;
static int      last16  = -1;
static float    k_ring = 0.42f, k_wow = 0.35f, k_flut = 0.22f;
static bool     paint_val;

// grid geometry
#define GX 56
#define GY 54
#define ROW_H 22
#define STEP_W 7   // 32 cells end at x=280, clear of the < > gutter at 292
#define STEP_H 15

static unsigned xr(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static int rr(int n) { return (int)(xr() % (unsigned)n); }

static float ring_hz(void) { return 40.0f * powf(40.0f, k_ring); }  // 40..1600 Hz

static void apply_ring(void) { instrument_ringmod(SL_LAMP, ring_hz(), 0.6f); }
static void apply_tape(void) { tape(k_wow * 0.8f, k_flut * 0.7f, 0.35f); }

// fire one voice now (audition) or `delay` ms ahead (the scheduler)
static void fire(int l, int delay, int st) {
    switch (l) {
    case L_THUD:  schedule_hit(delay, 36, SL_THUD, 6, 130); break;
    case L_LAMP:  schedule_hit(delay, 79, SL_LAMP, 4, 200); break;
    case L_PIPS:  // step 0 of the loop is the LONG pip, like the real six
        schedule_hit(delay, 84, SL_PIPS, 3, st == 0 ? 300 : 70); break;
    case L_TICK:  schedule_hit(delay, 96, SL_TICK, 3, 25); break;
    case L_SWELL: schedule_hit(delay, 70, SL_SWELL, 5, 620); break;
    }
    lanes[l].flash = 5;
}

static void generate(void) {
    rng = seed ? seed : 1u;
    for (int l = 0; l < NL; l++)
        for (int s = 0; s < MAXS; s++) { lanes[l].pat[s] = 0; lanes[l].splice[s] = (unsigned char)rr(15); }

    for (int s = 0; s < 16; s += 4) lanes[L_THUD].pat[s] = 1;           // the pulse
    if (rr(2)) lanes[L_THUD].pat[7 + rr(2) * 4] = 1;                    // one ghost
    for (int n = 1 + rr(2), t = 0; n > 0 && t < 20; t++) {              // lampshade: 1-2 strikes
        int s = rr(lanes[L_LAMP].len);
        if (!lanes[L_LAMP].pat[s]) { lanes[L_LAMP].pat[s] = 1; n--; }
    }
    lanes[L_PIPS].pat[0] = 1;                                           // the long pip
    lanes[L_PIPS].pat[2 + rr(4)] = 1;
    for (int n = 3 + rr(2), t = 0; n > 0 && t < 30; t++) {              // splice clicks
        int s = rr(lanes[L_TICK].len);
        if (!lanes[L_TICK].pat[s]) { lanes[L_TICK].pat[s] = 1; n--; }
    }
    lanes[L_SWELL].pat[14 + rr(16)] = 1;                                // one swell, back half
}

void init(void) {
    lanes[L_THUD]  = (Lane){ "THUD",  16, 0, {0}, {0}, 0, CLR_ORANGE };
    lanes[L_LAMP]  = (Lane){ "LAMP",   5, 0, {0}, {0}, 0, CLR_YELLOW };
    lanes[L_PIPS]  = (Lane){ "PIPS",   7, 0, {0}, {0}, 0, CLR_GREEN };
    lanes[L_TICK]  = (Lane){ "TICK",  11, 0, {0}, {0}, 0, CLR_LIGHT_GREY };
    lanes[L_SWELL] = (Lane){ "SWELL", 32, 0, {0}, {0}, 0, CLR_INDIGO };

    instrument(SL_THUD, INSTR_SINE, 2, 110, 0, 50);                     // dead mic'd-table thud
    instrument_filter(SL_THUD, FILTER_LOW, 350, 2);

    instrument(SL_LAMP, INSTR_MALLET, 1, 60, 0, 120);                   // the struck lampshade...
    instrument_harmonics(SL_LAMP, 0.85f);                               // ...metal bar
    instrument_timbre(SL_LAMP, 0.7f);
    instrument_morph(SL_LAMP, 0.3f);
    apply_ring();                                                       // ...through the ring mod

    instrument(SL_PIPS, INSTR_SINE, 1, 30, 6, 25);                      // the 1kHz time pip
    instrument(SL_TICK, INSTR_NOISE, 1, 18, 0, 12);                     // tape-splice click
    instrument_filter(SL_TICK, FILTER_HIGH, 3200, 3);

    instrument(SL_SWELL, INSTR_NOISE, 550, 0, 7, 45);                   // rises 550ms, cut dead:
    instrument_filter(SL_SWELL, FILTER_BAND, 1100, 6);                  // the backwards cymbal

    apply_tape();
    bpm(tempo);
    generate();
}

// hit-test the grid: returns lane or -1; *col = step (within that lane's len)
static int grid_lane(int mx, int my, int *col) {
    if (mx < GX || my < GY) return -1;
    int l = (my - GY) / ROW_H;
    if (l >= NL || (my - GY) % ROW_H >= STEP_H) return -1;
    int c = (mx - GX) / STEP_W;
    if (c >= lanes[l].len) return -1;
    *col = c;
    return l;
}

void update(void) {
    for (int l = 0; l < NL; l++)
        if (keyp('1' + l)) fire(l, 0, 1);

    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; }
    if (keyp(KEY_UP))    { tempo += 4; if (tempo > 200) tempo = 200; bpm(tempo); }
    if (keyp(KEY_DOWN))  { tempo -= 4; if (tempo <  50) tempo =  50; bpm(tempo); }
    if (keyp('['))       { seed--; generate(); }
    if (keyp(']'))       { seed++; generate(); }

    // taps: toggle a cell (drag paints), audition a lane label, mute, len < >
    int mc, ml = grid_lane(mouse_x(), mouse_y(), &mc);
    if (mouse_pressed(MOUSE_LEFT)) {
        if (ml >= 0) {
            paint_val = !lanes[ml].pat[mc];
            lanes[ml].pat[mc] = paint_val;
            if (paint_val) fire(ml, 0, mc);
        }
    } else if (mouse_down(MOUSE_LEFT)) {
        if (ml >= 0) lanes[ml].pat[mc] = paint_val;
    }
    for (int l = 0; l < NL; l++) {
        int ry = GY + l * ROW_H;
        if (tapp(2, ry, 36, STEP_H)) fire(l, 0, 1);                     // label = audition
        if (tapp(40, ry, 12, STEP_H)) lanes[l].mute = !lanes[l].mute;
        if (tapp(292, ry, 12, STEP_H) && lanes[l].len > 2)  lanes[l].len--;
        if (tapp(306, ry, 12, STEP_H) && lanes[l].len < MAXS) lanes[l].len++;
    }

#ifdef DE_TRACE
    watch("seed", "%08x", seed);
    watch("s16", "%d", last16);
#endif

    if (!running) return;

    // one absolute 16th-note counter; every lane wraps it at its OWN length,
    // so phases stay tape-locked. Each step scheduled one ahead (bossa/cr78
    // pattern) so hits land sample-accurate + carry their splice offset.
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        bool first = (last16 < 0);
        last16 = s16;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        for (int l = 0; l < NL; l++) {
            int st = (s16 + 1) % lanes[l].len;
            if (lanes[l].pat[st] && !lanes[l].mute)
                fire(l, delay + lanes[l].splice[st], st);
        }
        if (first)
            for (int l = 0; l < NL; l++) {
                int st = s16 % lanes[l].len;
                if (lanes[l].pat[st] && !lanes[l].mute) fire(l, 0, st);
            }
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();

    print("DELIA", 8, 6, CLR_WHITE);
    font(FONT_SMALL);
    print("tape loops of unequal length", 56, 8, CLR_MEDIUM_GREY);
    char buf[48];
    snprintf(buf, sizeof buf, "seed %08X   [ ] reseed", seed);
    print(buf, 8, 22, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "%d bpm", tempo);
    print(buf, 176, 22, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    if (ui_button(232, 16, 60, 16, running ? "STOP" : "RUN"))
        { running = !running; last16 = -1; }

    int cur16 = last16 < 0 ? 0 : last16;
    for (int l = 0; l < NL; l++) {
        Lane *ln = &lanes[l];
        int ry = GY + l * ROW_H;
        int lab = ln->flash > 0 ? CLR_WHITE : ln->color;
        if (ln->flash > 0) ln->flash--;
        font(FONT_SMALL);
        print(ln->name, 4, ry + 5, lab);
        font(FONT_NORMAL);
        rect(40, ry, 12, STEP_H, CLR_DARK_GREY);                        // mute box
        if (ln->mute) rectfill(43, ry + 3, 6, STEP_H - 6, CLR_RED);

        int ph = cur16 % ln->len;
        for (int c = 0; c < ln->len; c++) {
            int cx = GX + c * STEP_W;
            if (ln->pat[c]) rectfill(cx + 1, ry + 1, STEP_W - 2, STEP_H - 2,
                                     ln->mute ? CLR_DARK_GREY : ln->color);
            else            rect(cx + 1, ry + 1, STEP_W - 2, STEP_H - 2, CLR_DARKER_GREY);
            if (running && c == ph)
                rect(cx, ry, STEP_W, STEP_H, CLR_WHITE);                // the playhead
        }
        font(FONT_SMALL);
        print("<", 295, ry + 5, CLR_MEDIUM_GREY);
        print(">", 309, ry + 5, CLR_MEDIUM_GREY);
        snprintf(buf, sizeof buf, "%d", ln->len);
        print(buf, GX + ln->len * STEP_W + 3, ry + 5, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    // the knobs: (re)configure ONLY on change - tape/ringmod are set-and-hold
    if (ui_knob(&k_ring, 70, 176, "RING")) apply_ring();
    if (ui_knob(&k_wow, 150, 176, "WOW"))  apply_tape();
    if (ui_knob(&k_flut, 214, 176, "FLUT")) apply_tape();
    font(FONT_SMALL);
    snprintf(buf, sizeof buf, "%d hz", (int)ring_hz());
    print(buf, 14, 173, CLR_DARK_GREY);
    print("SPACE run", 262, 166, CLR_DARK_GREY);
    print("1-5 fire", 262, 175, CLR_DARK_GREY);
    print("< > length", 262, 184, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}
