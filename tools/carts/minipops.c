/* de:meta
{
  "slug": "minipops",
  "title": "mini pops mp-7",
  "status": "active",
  "created": "2026-11-04",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "drum-synthesis",
    "analog-voice-modeling",
    "swing-timing"
  ],
  "lineage": "Korg Mini Pops MP-7 (1966) homage on the cr78 chassis (the same editable 16-step grid, accent strip and swing knob); every voice synthesized and measured against the real MP-7's one-shots (tools/oneshot-compare.js), the guira and quijada on INSTR_SCRAPE.",
  "homage": "Korg Mini Pops MP-7 (1966)",
  "description": "The Korg Mini Pops MP-7 (1966), the little preset rhythm box Jean-Michel Jarre ran through Oxygène, rebuilt from synth voices and tuned against the real machine's one-shots: a short round 54 Hz kick, two snares that are pure filtered noise (no drum body at all, the sound of the era), three bongos as bare sine pings, a pure 851 Hz cowbell, a two-tone clave, maracas, a narrow ringing tambourine, a short and a long cymbal, and the two scrapers the box was famous for, a metal guira and a rattling quijada (jawbone). Ten rhythms in the styles 1960s rhythm boxes offered (bossa nova, samba, cha-cha, mambo, rumba, beguine, tango, slow rock, swing, march): our own arrangements, not the machine's preset data. The grid is editable: click/drag cells, click the orange strip to move accents, click a label to audition. Q-G play voices by hand, LEFT/RIGHT preset, UP/DOWN tempo, Z/X swing, SPACE start/stop."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// ── KORG MINI POPS MP-7 ──────────────────────────────────────────────────
// A 1966 home rhythm box (Jarre's Oxygène). Built on cr78.c's chassis (grid,
// accents, swing, keys); this file owns the VOICES and the RHYTHMS.
//
// Every voice was measured against the real MP-7's one-shots (a vintage
// drum-machine sample collection, used locally to tune by ear and by number,
// never shipped) with tools/oneshot-compare.js: pitch, energy band, time to
// -20 dB. What they say about the machine:
//   kick, bongos, cowbell, clave — almost PURE sine pings (kick 54 Hz,
//     bongos 592 / 398 / 183 Hz, cowbell 851, clave 409 + a 2175 Hz partial)
//   snares — pure filtered NOISE, 2.7-12 kHz, no tonal body at all
//   tambourine — a narrow ring at 4.6 kHz (the CR-78's is the same idea)
//   cymbals — a noise band around 6.4 kHz, a short one and a long one
//   guira — a metal scraper, 66 teeth/s, body 3.4 / 4.5 / 9 kHz -> INSTR_SCRAPE
//   quijada — the jawbone, 22 clacks/s at 2.6 kHz; INSTR_SCRAPE's quijada
//     recipe was fitted to THIS machine's quijada (Playtime's mp7-quijada)
// The RHYTHMS are our own arrangements in the styles such boxes offered;
// there is no sourced MP-7 preset data here, so none is claimed.
//
//   Q W E R T Y U I O P A S D F G   play each voice by hand
//   LEFT / RIGHT preset · UP / DOWN tempo · Z / X swing · SPACE start/stop
//   MOUSE  click/drag cells, click the orange strip for accents, a label to audition

// voices — named indices, never raw numbers (house rule)
enum {
    V_KICK, V_SN1, V_SN2, V_RIM, V_BONH, V_BONM, V_BONL, V_COW,
    V_CLV, V_MAR, V_TAMB, V_CYMS, V_CYML, V_GUIRA, V_QUIJ, NV
};

// instrument slots (5..31 are free; 5-8 are the user waves, start at 9)
enum {
    SL_KICK = 9, SL_SN1, SL_SN2, SL_RIM, SL_BON, SL_COW, SL_CLV, SL_MAR,
    SL_TAMB, SL_CYM, SL_GUIRA, SL_QUIJ
};

static const char *VNAME[NV] = {
    "KICK", "SNARE 1", "SNARE 2", "RIM", "BONGO H", "BONGO M", "BONGO L", "COWBELL",
    "CLAVE", "MARACAS", "TAMB", "CYMBAL S", "CYMBAL L", "GUIRA", "QUIJADA"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G'
};
static const int VCLR[NV] = {
    CLR_RED, CLR_ORANGE, CLR_PEACH, CLR_LIGHT_PEACH, CLR_INDIGO, CLR_INDIGO, CLR_INDIGO,
    CLR_GREEN, CLR_DARK_ORANGE, CLR_YELLOW, CLR_PINK, CLR_WHITE, CLR_LIGHT_GREY,
    CLR_LIME_GREEN, CLR_DARK_PEACH
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   44    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight, 66 = triplet feel (offbeat 8ths late)
} Pat;

// the RHYTHMS: our own arrangements in the styles a 1960s rhythm box offered (not MP-7 preset data)
static const Pat PRESET[] = {
    { "BOSSA NOVA", 128, {
        [V_KICK]  = "x..x..x.x..x..x.",
        [V_RIM]   = "x..x..x...x..x..",   // the bossa clave
        [V_MAR]   = "xxxxxxxxxxxxxxxx",
        [V_CYMS]  = "x.x.x.x.x.x.x.x.",
      }, "x.......x......." },
    { "SAMBA", 104, {
        [V_KICK]  = "x..xx..xx..xx..x",
        [V_SN2]   = "..x...x...x...x.",
        [V_TAMB]  = "xxxxxxxxxxxxxxxx",
        [V_BONH]  = ".x.x.x...x.x.x..",
        [V_COW]   = "x.....x...x.....",
      }, "x...x...x...x..." },
    { "CHA-CHA", 118, {
        [V_KICK]  = "x.......x.......",
        [V_COW]   = "x...x...x...x...",
        [V_GUIRA] = "x...x...x...x.x.",   // the guira on the beat, a short extra before the bar
        [V_BONL]  = "......x.......xx",
        [V_CLV]   = "....x.......x...",
      }, "x.......x......." },
    { "MAMBO", 132, {
        [V_KICK]  = "x.......x.......",
        [V_COW]   = "x.x.x.x.x.x.x.x.",
        [V_BONH]  = ".x..x..x.x..x...",
        [V_BONM]  = "...x.......x....",
        [V_CLV]   = "x..x..x...x.x...",   // son clave 3-2
      }, "x...x...x...x..." },
    { "RUMBA", 112, {
        [V_KICK]  = "x.....x.x.......",
        [V_CLV]   = "x..x...x..x.x...",   // rumba clave
        [V_MAR]   = "x.x.x.x.x.x.x.x.",
        [V_BONL]  = "...x.......x..x.",
        [V_BONH]  = ".x...x.x.x...x..",
      }, "x.......x......." },
    { "BEGUINE", 104, {
        [V_KICK]  = "x.....x...x.....",
        [V_RIM]   = "...x...x.....x..",
        [V_CYMS]  = "x.x.x.x.x.x.x.x.",
        [V_BONL]  = "..............x.",
        [V_QUIJ]  = "x...............",   // the jawbone rattles in the bar
      }, "x.........x....." },
    { "TANGO", 120, {
        [V_KICK]  = "x...x...x...x...",
        [V_SN1]   = "......x.....x.x.",
        [V_CLV]   = "x.......x.......",
        [V_CYMS]  = "x...x...x...x...",
      }, "x...x...x...x..." },
    { "SLOW ROCK", 72, {   // the triplet feel comes from the swing knob, set to 66
        [V_KICK]  = "x.......x.x.....",
        [V_SN1]   = "....x.......x...",
        [V_CYMS]  = "x.x.x.x.x.x.x.x.",
        [V_CYML]  = "x...............",
      }, "x.......x.......", 66 },
    { "SWING", 138, {
        [V_KICK]  = "x.......x.......",
        [V_CYMS]  = "x...x.x.x...x.x.",
        [V_SN2]   = "....x.......x...",
      }, "x.......x.......", 66 },
    { "MARCH", 116, {
        [V_KICK]  = "x.......x.......",
        [V_SN1]   = "x.xxx.x.x.xxx.x.",
        [V_CYML]  = "........x.......",
      }, "x...x...x...x..." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;      // start on BOSSA NOVA
static int  tempo    = 128;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static int  nextz_x = -1;      // the preset '>' tap zone, recorded by draw() ('<' is fixed)
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  swing = 50;        // 50 = straight .. 66 = full triplet

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    for (int v = 0; v < NV; v++)
        for (int s = 0; s < STEPS; s++)
            grid[v][s] = p->row[v] && p->row[v][s] == 'x';
    for (int s = 0; s < STEPS; s++)
        gacc[s] = p->accent && p->accent[s] == 'x';
    tempo = p->tempo;
    swing = p->swing ? p->swing : 50;
    bpm(tempo);
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice, sample-accurately `delay` ms from now
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_KICK:  schedule_hit(delay, 33, SL_KICK, vv(5, boost), 90); break;    // 54 Hz
    case V_SN1:   schedule_hit(delay, 60, SL_SN1,  vv(4, boost), 180); break;
    case V_SN2:   schedule_hit(delay, 60, SL_SN2,  vv(4, boost), 110); break;
    case V_RIM:   schedule_hit(delay, 91, SL_RIM,  vv(5, boost), 12); break;   // a ~1.6 kHz click
    case V_BONH:  schedule_hit(delay, 74, SL_BON,  vv(4, boost), 18); break;   // 587 Hz (real 592), -20 dB ~30 ms
    case V_BONM:  schedule_hit(delay, 67, SL_BON,  vv(4, boost), 28); break;   // 392 (398), ~40 ms
    case V_BONL:  schedule_hit(delay, 54, SL_BON,  vv(4, boost), 60); break;   // 185 (183), ~75 ms
    case V_COW:   schedule_hit(delay, 80, SL_COW,  vv(4, boost), 55); break;   // 851 Hz with the tune
    case V_CLV:   // 409 Hz + its 2175 Hz partial ~8 dB under
        schedule_hit(delay, 68, SL_CLV, vv(4, boost), 35);
        schedule_hit(delay, 97, SL_CLV, vv(1, boost), 30);
        break;
    case V_MAR:   schedule_hit(delay, 90, SL_MAR,  vv(2, boost), 35); break;
    case V_TAMB:  schedule_hit(delay, 90, SL_TAMB, vv(1, boost), 60); break;   // the narrow ring is hot
    case V_CYMS:  schedule_hit(delay, 90, SL_CYM,  vv(3, boost), 130); break;
    case V_CYML:  schedule_hit(delay, 90, SL_CYM,  vv(3, boost), 340); break;
    case V_GUIRA: schedule_hit(delay, 50, SL_GUIRA, vv(4, boost), 940); break;  // one long stroke, the body a fifth down
    case V_QUIJ:  schedule_hit(delay, 60, SL_QUIJ,  vv(4, boost), 500); break;
    }
}

void init(void) {
    // ── the voice circuits, from the measurements in the header ─────────
    instrument(SL_KICK, INSTR_SINE, 0, 75, 0, 40);          // a short round thump, no pitch drop
    instrument_filter(SL_KICK, FILTER_LOW, 400, 1);
    instrument(SL_SN1, INSTR_NOISE, 0, 160, 0, 40);         // pure filtered noise, 2.7-12 kHz
    instrument_filter(SL_SN1, FILTER_BAND, 3700, 11);       // resonant: a wide band's skirt ran to 20 kHz
    instrument(SL_SN2, INSTR_NOISE, 0, 100, 0, 30);
    instrument_filter(SL_SN2, FILTER_BAND, 3700, 11);
    instrument(SL_RIM, INSTR_TRI, 0, 8, 0, 5);
    instrument_filter(SL_RIM, FILTER_BAND, 1600, 6);
    instrument(SL_BON, INSTR_SINE, 0, 120, 0, 20);          // the gate sets each bongo's length
    instrument(SL_COW, INSTR_SINE, 0, 60, 0, 20);
    instrument_tune(SL_COW, 0.4f);                          // 831 -> 851 Hz
    instrument(SL_CLV, INSTR_SINE, 0, 40, 0, 12);
    instrument(SL_MAR, INSTR_NOISE, 0, 35, 0, 10);
    instrument_filter(SL_MAR, FILTER_BAND, 7200, 12);
    instrument(SL_TAMB, INSTR_NOISE, 0, 45, 0, 30);
    instrument_filter(SL_TAMB, FILTER_BAND, 4600, 15);
    instrument(SL_CYM, INSTR_NOISE, 0, 400, 0, 60);         // the gate makes it short or long
    instrument_filter(SL_CYM, FILTER_BAND, 4800, 12);
    instrument(SL_GUIRA, INSTR_SCRAPE, 0, 0, 7, 30);        // a metal scraper, 66 teeth/s
    instrument_harmonics(SL_GUIRA, 0.92f);                  // BODY: nearly metal (a metal scraper: noisy, wide)
    instrument_timbre(SL_GUIRA, 0.82f);                     // RATE: 10 * 10^0.82 = 66 /s
    instrument_morph(SL_GUIRA, 0.0f);                       // a steady scrape
    instrument(SL_QUIJ, INSTR_SCRAPE, 0, 0, 7, 30);         // = scrape/quijada, fitted to THIS machine's
    instrument_harmonics(SL_QUIJ, 0.50f);
    instrument_timbre(SL_QUIJ, 0.35f);
    instrument_morph(SL_QUIJ, 1.0f);

    load_preset();
}

// hit-test the step grid: row -1 = accent strip, -2 = miss; col via *col
static int grid_row(int mx, int my, int *col) {
    int s = (mx - GX) / SX;
    if (mx < GX || s < 0 || s >= STEPS) return -2;
    if (my >= GY - 8 && my < GY - 1) { *col = s; return -1; }   // accent strip
    int v = (my - GY) / SY;
    if (my < GY || v < 0 || v >= NV) return -2;
    *col = s;
    return v;
}

void update(void) {
    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE) || tapp(244, 24, 64, 11)) { running = !running; last16 = -1; }   // tap PLAYING/STOPPED
    // < > switch presets — keys, or tap the label's arrows
    if (keyp(KEY_LEFT)  || tapp(10, 24, 20, 11))
        { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT) || (nextz_x >= 0 && tapp(nextz_x, 24, 20, 11)))
        { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP)   || tapp(274, 12, 32, 12)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }   // BPM halves
    if (keyp(KEY_DOWN) || tapp(244, 12, 30, 12)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z') || tapp(200, 12, 18, 12)) { swing -= 2; if (swing < 50) swing = 50; }                      // SW halves
    if (keyp('X') || tapp(218, 12, 20, 12)) { swing += 2; if (swing > 66) swing = 66; }

    // mouse: press toggles a cell (and remembers the value so a drag PAINTS
    // it across cells); a click on a row's label auditions the voice
    int mc, mr = grid_row(mouse_x(), mouse_y(), &mc);
    if (mouse_pressed(MOUSE_LEFT)) {
        if (mr >= 0) {
            paint_val = !grid[mr][mc];
            grid[mr][mc] = paint_val;
            if (paint_val) { fire(mr, 0, 0); flash[mr] = 5; }
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mouse_x() < GX && mouse_y() >= GY && mouse_y() < GY + NV * SY) {
            int v = (mouse_y() - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT)) {
        if (mr >= 0)       grid[mr][mc] = paint_val;
        else if (mr == -1) gacc[mc]     = paint_val;
    }

    if (!running) return;

    // sixteenth-note clock off the synth's own beat counter; each step is
    // scheduled ONE STEP AHEAD with schedule_hit() so hits land sample-
    // accurate, free of frame jitter (the bossa.c pattern)
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        bool first = (last16 < 0);
        last16  = s16;
        playhead = s16 & 15;

        for (int v = 0; v < NV; v++)
            if (grid[v][playhead]) flash[v] = 5;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        // swing: push each offbeat 8th (steps 2,6,10,14) late. At 66 the
        // offbeat lands 2/3 into the quarter — the triplet shuffle a 1966 box
        // could only have as its own SWING / SLOW ROCK presets
        if ((nx & 3) == 2) delay += (swing - 50) * 4 * step_ms / 100;
        int boost   = gacc[nx] ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (grid[v][nx]) fire(v, boost, delay);

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = gacc[playhead] ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (grid[v][playhead]) fire(v, b0, 0);
        }
    }
}

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // walnut cabinet + black panel
    cls(CLR_DARK_BROWN);
    for (int y = 3; y < 200; y += 7) line(0, y, 319, y, CLR_BROWNISH_BLACK);
    rectfill(8, 10, 304, 186, CLR_BLACK);
    rect(8, 10, 304, 186, CLR_DARK_GREY);

    print("MINI POPS MP-7", 16, 16, CLR_LIGHT_GREY);
    sprintf(buf, "SW%2d", swing);
    print(buf, 204, 16, swing > 50 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 16, CLR_LIGHT_PEACH);
    int nx = print("< ", 16, 28, CLR_ORANGE);      // the arrows are tap targets
    nx = print(p->name, nx, 28, CLR_ORANGE);
    print(" >", nx, 28, CLR_ORANGE);
    nextz_x = nx + 2;
    print(running ? "PLAYING" : "STOPPED", 248, 28, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 8, CLR_DARKER_GREY);

    // accent ticks above the grid (the real unit's accent = louder steps);
    // off slots stay faintly visible so you can see they're clickable
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_ORANGE : CLR_DARKER_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_YELLOW);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int s = 0; s < STEPS; s++) {
            int x   = GX + s * SX;
            bool on = grid[v][s];
            int  c  = on ? ((flash[v] > 0 && s == playhead && running) ? CLR_WHITE : VCLR[v])
                         : ((s & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (on) rectfill(x, y, SX - 4, 7, c);
            else    rect(x, y, SX - 4, 7, c);
        }
        if (flash[v] > 0) flash[v]--;
    }

    print("CLICK TO EDIT  <> PRESET  ^v BPM  SPC", 14, 187, CLR_MEDIUM_GREY);
}
