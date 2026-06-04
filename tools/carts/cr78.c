#include "studio.h"
#include <stdio.h>

// ── CR-78 COMPURHYTHM ─────────────────────────────────────────────────────
// An homage to the Roland CR-78 (1978) — the wood-panelled preset rhythm box
// behind "In the Air Tonight" (Disco-2) and "Heart of Glass" (Mambo+Beguine).
// Every voice on the original is pure analog circuitry, no samples — which
// means every voice can be rebuilt with this synth API. This cart does that,
// one instrument slot per voice, and plays the classic preset patterns.
//
// How the real machine makes each sound, and how we copy it:
//
//   tuned voices (kick, bongos, congas, rim, claves)
//     a "bridged-T" network — a damped sine that rings at a fixed pitch when
//     kicked by a trigger pulse.  →  INSTR_SINE/TRI, fast-decay amp envelope,
//     plus a small downward ENV_PITCH sweep for the membrane "thump".
//
//   noise voices (hats, maracas, tambourine, snare rattle)
//     ONE white-noise source, shaped per voice by a different filter.
//     →  INSTR_NOISE through instrument_filter(); the filter IS the voice:
//        hats = gentle bandpass ~7-8kHz (the famous CR-78 "swish" — softer
//        than an 808), maracas = highpass, tambourine = narrow resonant band.
//
//   metallic voices (cowbell, metallic beat, cymbal)
//     detuned square waves beating against each other, bandpassed. The CR-78
//     metallic beat is confirmed to be THREE filtered squares (the 808 cymbal
//     uses six — that's why the CR-78 clangs simpler and buzzier).
//     →  several hit()s on one square slot at enharmonic pitches.
//
//   guiro — the strangest circuit in the box
//     a noise burst whose loudness is chopped into rapid "teeth" to make the
//     ratcheting scrape.  →  bandpassed noise + LFO_VOLUME at 36Hz, full
//     depth, with a rising ENV_CUTOFF so the scrape sweeps up like a real
//     stick dragged over the ridges.
//
//   accent — not a sound! On the real unit it's a knob that just makes the
//     flagged steps louder. Here: orange ticks above the grid, vol +2.
//
//   Q W E R T Y U I O P A S D F G   play each voice by hand
//   LEFT / RIGHT  preset      UP / DOWN  tempo      SPACE  start / stop

// voices — named indices, never raw numbers (house rule)
enum {
    V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM, V_METAL, V_COW,
    V_TAMB, V_MAR, V_CLV, V_GUIRO, V_BONH, V_BONL, V_CONGA, NV
};

// instrument slots (5..31 are free; 5-8 are the user waves, start at 9)
enum {
    SL_KICK = 9, SL_SNB, SL_SNN, SL_RIM, SL_HATC, SL_HATO, SL_CYMT,
    SL_CYMN, SL_TAMB, SL_COW, SL_CLV, SL_MAR, SL_BON, SL_CON, SL_GUI, SL_MET
};

static const char *VNAME[NV] = {
    "KICK", "SNARE", "RIM", "HAT CL", "HAT OP", "CYMBAL", "METAL", "COWBELL",
    "TAMB", "MARACAS", "CLAVES", "GUIRO", "BONGO H", "BONGO L", "CONGA"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G'
};
static const int VCLR[NV] = {
    CLR_RED, CLR_ORANGE, CLR_PEACH, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_WHITE,
    CLR_BLUE, CLR_GREEN, CLR_PINK, CLR_LIGHT_PEACH, CLR_DARK_ORANGE,
    CLR_LIME_GREEN, CLR_INDIGO, CLR_INDIGO, CLR_DARK_PEACH
};

#define STEPS 16

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
} Pat;

static const Pat PRESET[] = {
    { "ROCK-1", 118, {
        [V_KICK]  = "x.......x.x.....",
        [V_SNARE] = "....x.......x...",
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
      }, "x.......x......." },
    { "DISCO-2", 100, {            // the "In the Air Tonight" preset
        [V_KICK]  = "x...x...x...x...",
        [V_HATC]  = "xx.xxx.xxx.xxx.x",
        [V_HATO]  = "..x...x...x...x.",
        [V_TAMB]  = "....x.......x...",
        [V_BONH]  = ".........x.....x",
        [V_GUIRO] = "............x...",
      }, "x.......x......." },
    { "BOSSA NOVA", 132, {
        [V_KICK]  = "x.....x.x.....x.",
        [V_RIM]   = "x..x..x...x..x..",   // bossa clave on the rim
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
        [V_MAR]   = "xxxxxxxxxxxxxxxx",
      }, "x.......x......." },
    { "MAMBO", 176, {              // half of the "Heart of Glass" blend
        [V_KICK]  = "x.......x.......",
        [V_COW]   = "x..x..x.x..x..x.",
        [V_CONGA] = "..x...x...x...x.",
        [V_BONH]  = ".x......x.x.....",
        [V_CLV]   = "x..x..x...x.x...",   // son clave 3-2
      }, "x.......x......." },
    { "CHA-CHA", 118, {
        [V_GUIRO] = "x...x...x...x...",
        [V_COW]   = "x...x...x...x...",
        [V_KICK]  = "....x.......x...",
        [V_CONGA] = "......x.......xx",
        [V_BONL]  = "..x.......x.....",
      }, "x.......x......." },
    { "BEGUINE", 104, {            // the other half of "Heart of Glass"
        [V_KICK]  = "x.....x...x.....",
        [V_METAL] = "x...x...x...x...",   // the Kraftwerkian clang
        [V_RIM]   = "...x...x.....x..",
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
        [V_BONL]  = "..............x.",
      }, "x.........x....." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 1;      // start on DISCO-2, obviously
static int  tempo    = 100;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice, sample-accurately `delay` ms from now. Layered voices are
// several hit()s — the detuned pairs/triples ARE the metallic timbres.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_KICK:  schedule_hit(delay, 33, SL_KICK, vv(5, boost), 220); break;
    case V_SNARE: // tonal shell + bandpassed rattle, the two-layer recipe
        schedule_hit(delay, 53, SL_SNB, vv(4, boost), 100);
        schedule_hit(delay, 60, SL_SNN, vv(4, boost), 170);
        break;
    case V_RIM:   schedule_hit(delay, 94, SL_RIM, vv(4, boost), 45); break;
    case V_HATC:  schedule_hit(delay, 90, SL_HATC, vv(3, boost), 60); break;
    case V_HATO:  schedule_hit(delay, 90, SL_HATO, vv(3, boost), 360); break;
    case V_CYM:   // two detuned squares + a long noise wash
        schedule_hit(delay, 90, SL_CYMT, vv(3, boost), 500);
        schedule_hit(delay, 95, SL_CYMT, vv(2, boost), 500);
        schedule_hit(delay, 70, SL_CYMN, vv(3, boost), 600);
        break;
    case V_METAL: // confirmed CR-78 recipe: THREE filtered squares
        schedule_hit(delay, 88, SL_MET, vv(3, boost), 210);
        schedule_hit(delay, 93, SL_MET, vv(3, boost), 210);
        schedule_hit(delay, 97, SL_MET, vv(2, boost), 210);
        break;
    case V_COW:   // detuned pair beating ~1.4:1 — the beating IS the cowbell
        schedule_hit(delay, 74, SL_COW, vv(4, boost), 250);
        schedule_hit(delay, 80, SL_COW, vv(3, boost), 250);
        break;
    case V_TAMB:  schedule_hit(delay, 80, SL_TAMB, vv(3, boost), 120); break;
    case V_MAR:   schedule_hit(delay, 90, SL_MAR, vv(3, boost), 40); break;
    case V_CLV:   schedule_hit(delay, 97, SL_CLV, vv(4, boost), 55); break;
    case V_GUIRO: schedule_hit(delay, 70, SL_GUI, vv(4, boost), 300); break;
    case V_BONH:  schedule_hit(delay, 64, SL_BON, vv(4, boost), 140); break;
    case V_BONL:  schedule_hit(delay, 59, SL_BON, vv(4, boost), 150); break;
    case V_CONGA: schedule_hit(delay, 52, SL_CON, vv(4, boost), 250); break;
    }
}

void init(void) {
    // ── the voice circuits ──────────────────────────────────────────────
    // kick — soft and round (NOT an 808 punch): low sine, lowpassed, with a
    // quick pitch drop from ~+13 semitones for the gentle "thump"
    instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 40);
    instrument_filter(SL_KICK, FILTER_LOW, 320, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 45, 13.0f);

    // snare shell — short damped sine with a tiny pitch blip
    instrument(SL_SNB, INSTR_SINE, 0, 70, 0, 25);
    instrument_filter(SL_SNB, FILTER_LOW, 900, 1);
    instrument_env(SL_SNB, 0, ENV_PITCH, 0, 25, 5.0f);
    // snare rattle — bandpassed noise that opens then closes
    instrument(SL_SNN, INSTR_NOISE, 0, 130, 0, 35);
    instrument_filter(SL_SNN, FILTER_BAND, 1700, 3);
    instrument_env(SL_SNN, 0, ENV_CUTOFF, 0, 90, 900.0f);

    // rim — very short triangle ring through a resonant band: woody "tick"
    instrument(SL_RIM, INSTR_TRI, 0, 28, 0, 12);
    instrument_filter(SL_RIM, FILTER_BAND, 1700, 9);

    // hats — the famous swishy CR-78 hat: GENTLE bandpass, breathy not sizzly.
    // closed and open are the same filter, only the decay differs
    instrument(SL_HATC, INSTR_NOISE, 0, 40, 0, 18);
    instrument_filter(SL_HATC, FILTER_BAND, 7800, 4);
    instrument(SL_HATO, INSTR_NOISE, 0, 320, 0, 80);
    instrument_filter(SL_HATO, FILTER_BAND, 7200, 4);

    // cymbal — highpassed square cluster + noise shimmer on top
    instrument(SL_CYMT, INSTR_SQUARE, 0, 450, 0, 120);
    instrument_filter(SL_CYMT, FILTER_HIGH, 5200, 5);
    instrument(SL_CYMN, INSTR_NOISE, 0, 550, 0, 150);
    instrument_filter(SL_CYMN, FILTER_HIGH, 6500, 2);

    // tambourine — noise through a NARROW resonant band: the jingle ring
    instrument(SL_TAMB, INSTR_NOISE, 0, 90, 0, 30);
    instrument_filter(SL_TAMB, FILTER_BAND, 8400, 10);

    // cowbell — square slot, fired twice detuned (see fire())
    instrument(SL_COW, INSTR_SQUARE, 0, 220, 0, 60);
    instrument_filter(SL_COW, FILTER_BAND, 2600, 5);

    // claves — single high triangle ping, lowpassed: pure woody click
    instrument(SL_CLV, INSTR_TRI, 0, 36, 0, 14);
    instrument_filter(SL_CLV, FILTER_LOW, 3600, 6);

    // maracas — the shortest noise burst in the box, highpassed
    instrument(SL_MAR, INSTR_NOISE, 0, 26, 0, 12);
    instrument_filter(SL_MAR, FILTER_HIGH, 6000, 2);

    // bongos / conga — bare damped sines at membrane pitches, small pitch drop
    instrument(SL_BON, INSTR_SINE, 0, 110, 0, 30);
    instrument_env(SL_BON, 0, ENV_PITCH, 0, 18, 3.0f);
    instrument(SL_CON, INSTR_SINE, 0, 210, 0, 50);
    instrument_env(SL_CON, 0, ENV_PITCH, 0, 22, 3.0f);

    // guiro — the ratchet: bandpassed noise CHOPPED by a 36Hz volume LFO
    // (~11 teeth per scrape) while the cutoff sweeps upward like the stick
    // running along the ridges
    instrument(SL_GUI, INSTR_NOISE, 0, 260, 0, 60);
    instrument_filter(SL_GUI, FILTER_BAND, 2800, 7);
    instrument_env(SL_GUI, 0, ENV_CUTOFF, 200, 80, 1400.0f);
    instrument_lfo(SL_GUI, 0, LFO_VOLUME, 36.0f, 1.0f);

    // metallic beat — one square slot, fired three times enharmonically
    instrument(SL_MET, INSTR_SQUARE, 0, 180, 0, 60);
    instrument_filter(SL_MET, FILTER_BAND, 3300, 6);

    tempo = PRESET[pre].tempo;
    bpm(tempo);
}

void update(void) {
    const Pat *p = &PRESET[pre];

    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; }
    if (keyp(KEY_LEFT))  { pre = (pre + NP - 1) % NP; tempo = PRESET[pre].tempo; bpm(tempo); last16 = -1; }
    if (keyp(KEY_RIGHT)) { pre = (pre + 1) % NP;      tempo = PRESET[pre].tempo; bpm(tempo); last16 = -1; }
    if (keyp(KEY_UP))   { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }
    if (keyp(KEY_DOWN)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }

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
            if (p->row[v] && p->row[v][playhead] == 'x') flash[v] = 5;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        int boost   = (p->accent && p->accent[nx] == 'x') ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (p->row[v] && p->row[v][nx] == 'x') fire(v, boost, delay);

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = (p->accent && p->accent[playhead] == 'x') ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (p->row[v] && p->row[v][playhead] == 'x') fire(v, b0, 0);
        }
    }
}

#define GX   88    // grid left edge
#define GY   44    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // walnut cabinet + black panel
    cls(CLR_DARK_BROWN);
    for (int y = 3; y < 200; y += 7) line(0, y, 319, y, CLR_BROWNISH_BLACK);
    rectfill(8, 10, 304, 182, CLR_BLACK);
    rect(8, 10, 304, 182, CLR_DARK_GREY);

    print("COMPURHYTHM CR-78", 16, 16, CLR_LIGHT_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 16, CLR_LIGHT_PEACH);
    sprintf(buf, "< %s >", p->name);
    print(buf, 16, 28, CLR_ORANGE);
    print(running ? "PLAYING" : "STOPPED", 248, 28, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 8, CLR_DARKER_GREY);

    // accent ticks above the grid (the real unit's accent = louder steps)
    for (int s = 0; s < STEPS; s++)
        if (p->accent && p->accent[s] == 'x')
            rectfill(GX + s * SX, GY - 6, SX - 4, 3, CLR_ORANGE);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_YELLOW);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int s = 0; s < STEPS; s++) {
            int x   = GX + s * SX;
            bool on = p->row[v] && p->row[v][s] == 'x';
            int  c  = on ? ((flash[v] > 0 && s == playhead && running) ? CLR_WHITE : VCLR[v])
                         : ((s & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (on) rectfill(x, y, SX - 4, 7, c);
            else    rect(x, y, SX - 4, 7, c);
        }
        if (flash[v] > 0) flash[v]--;
    }

    print("Q-G PLAY  <> PRESET  ^v BPM  SPC RUN", 16, 184, CLR_MEDIUM_GREY);
}
