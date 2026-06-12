#include "studio.h"
#include "ui.h"

// GROOVEBOX — a tiny house/techno box and a HOME for the summed-bus effects.
//
// The pedalboard cart owns the SERIAL-insert family (one guitar → a chain of
// pedals). This cart owns the other half: the SUMMED-BUS family — many looping
// voices summed into one master, then sculpted as a whole. Those effects can't
// be shown on a single held chord; the loop has to run so you can hear the mix
// breathe. So: a 6-row × 16-step grid plays itself, freeing both hands to ride
// the rack along the bottom.
//
// The headline is the PUMP (sidechain). A real summed-bus sidechain compressor
// isn't in the engine yet — so the pump is FAKED cart-side exactly the way the
// `house` station fakes it: a kick-triggered envelope ducks the held PAD voice
// every time the kick fires (see docs/design/groovebox-blind-brief.md). When a
// real `sidechain()` ships, the PUMP knob stays put and only the wiring under it
// changes — that's why this cart is "a home for upcoming effects." The GLUE slot
// is the next tenant: drawn-but-dormant, waiting for a bus compressor.
//
// Everything else on the rack is REAL and already in the engine, here shown on a
// full mix for the first time: CRUSH (summed bitcrush — the SP-1200 grit that
// only exists on the sum), EQ, TAPE, SPACE (reverb), and the ORDER toggle that
// reorders the master inserts (crush before vs after eq — audibly different).
//
//   TAP / DRAG cells   toggle steps; drag to paint a run (each finger paints)
//   WASD + Z           move cursor / toggle the cell under it
//   the KNOBS          PUMP depth · CRUSH · EQ tilt · TAPE · SPACE (reverb)
//   ORDER button       swap the master CRUSH↔EQ insert order
//   ←/→                tempo down / up

#define ROWS  6
#define STEPS 16

#define GX 46    // grid left edge
#define GY 22    // grid top edge
#define SX 17    // column stride
#define SY 18    // row stride
#define CW 15    // cell width
#define CH 15    // cell height

// instrument slots 5..11 — the kit + the pumped pad
enum { SL_KICK = 5, SL_CLAP, SL_CHAT, SL_OHAT, SL_BASS, SL_STAB, SL_PAD };

static const char *LABEL[ROWS] = { "KICK", "CLAP", "CHAT", "OHAT", "BASS", "STAB" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_PINK, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_BLUE, CLR_GREEN };

// a house groove that sounds good the instant it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor (the pump trigger)
    "....x.......x...",   // clap — backbeat
    "x.x.x.x.x.x.x.x.",   // closed hat — sixteenths
    "..x...x...x...x.",   // open hat — the off-beat lift
    "..x.x...x.x.x...",   // bass — a little bounce
    "x.......x.......",   // stab — chord on the one
};

// a 4-chord minor vamp (A minor: i–VI–III–VII), voiced for smooth pad movement
static const int CHORD[4][3] = {
    { 57, 60, 64 },   // Am  A3 C4 E4
    { 57, 60, 65 },   // F   A3 C4 F4
    { 55, 60, 64 },   // C   G3 C4 E4
    { 55, 59, 62 },   // G   G3 B3 D4
};
static const int   ROOT [4] = { 45, 41, 48, 43 };       // bass roots A2 F2 C3 G2
static const char *CNAME[4] = { "Am", "F", "C", "G" };

static bool grid[ROWS][STEPS];
static int  curR = 0, curC = 0;        // keyboard cursor
static int  cur_step = 0;              // playhead column
static int  last_16  = -1;             // last sixteenth we triggered
static int  flash[ROWS];               // frame() each row last fired
static int  tempo    = 122;
static int  chordIdx = 0, lastChord = -1;

static int   padH[3] = { -1, -1, -1 }; // the held, pumped pad chord
static float pumpEnv = 0;              // 1 the instant the kick fires, decays → 0
static float openness = 1;             // pad "openness" 0..1, for the meter
static int   lastPadCut = -1;          // last cutoff pushed to the pad (skip redundant)

// rack knobs (0..1). EQ is the engine's real 3 bands (0.5 = flat) — a louder EQ is
// what makes the fx_order CRUSH↔EQ toggle audible (gentle = no difference to hear).
static float k_pump = 0.65f, k_crush = 0.0f, k_tape = 0.20f, k_space = 0.30f;
static float k_eqLo = 0.5f, k_eqMid = 0.5f, k_eqHi = 0.5f;
static bool  orderSwapped = false;     // CRUSH after eq (default) vs before

// per-finger grid paint (the drummachine.c sweep pattern)
#define NPTR 10
#define NOID (-999)
typedef struct { int id, paint, lastR, lastC; } Ptr;
static Ptr ptr[NPTR];

static void play_row(int r);

static int cell_rc(int x, int y, int *r, int *c) {
    if (x < GX || y < GY) return 0;
    int cc = (x - GX) / SX, rr = (y - GY) / SY;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    *r = rr; *c = cc; return 1;
}

static void set_cell(int r, int c, bool on) {
    grid[r][c] = on;
    curR = r; curC = c;
    if (on) { play_row(r); flash[r] = frame(); }
}

// fire one row. KICK also arms the pump.
static void play_row(int r) {
    switch (r) {
        case 0: hit(72, INSTR_NOISE, 2, 12);                 // kick click...
                hit(34, SL_KICK, 6, 250);                    // ...over the sine boom
                pumpEnv = 1.0f; break;                       // ← the sidechain trigger
        case 1: hit(64, SL_CLAP, 4, 35);                     // clap — three hands
                schedule_hit(12, 64, SL_CLAP, 3, 30);
                schedule_hit(24, 64, SL_CLAP, 4, 60); break;
        case 2: hit(92, SL_CHAT, 3, 24); break;              // closed hat
        case 3: hit(92, SL_OHAT, 2, 170); break;             // open hat
        case 4: hit(ROOT[chordIdx], SL_BASS, 5, 110); break; // bass root
        case 5: for (int k = 0; k < 3; k++)                  // chord stab
                    hit(CHORD[chordIdx][k], SL_STAB, 4, 150); break;
    }
}

void init() {
    // the kit (slots 5..10) — the drummachine recipes, house-tuned
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);          // kick body
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);        // +30st donk, settles 55ms
    instrument_drive(SL_KICK, 0.28f);                        // run hot — house pump-as-saturation
    instrument(SL_CLAP, INSTR_NOISE, 0, 60, 0, 30);
    instrument_filter(SL_CLAP, FILTER_BAND, 1100, 5);
    instrument(SL_CHAT, INSTR_NOISE, 0, 24, 0, 14);
    instrument_filter(SL_CHAT, FILTER_HIGH, 7000, 2);
    instrument(SL_OHAT, INSTR_NOISE, 0, 180, 0, 90);
    instrument_filter(SL_OHAT, FILTER_HIGH, 6500, 2);
    instrument(SL_BASS, INSTR_SQUARE, 1, 120, 2, 70);
    instrument_filter(SL_BASS, FILTER_LOW, 700, 4);
    instrument(SL_STAB, INSTR_SAW, 2, 150, 1, 120);
    instrument_filter(SL_STAB, FILTER_LOW, 1600, 3);

    // the PAD (slot 11) — a detuned-saw string wash, resonant LP. The pump victim.
    instrument(SL_PAD, INSTR_SAW, 80, 700, 6, 900);
    instrument_filter(SL_PAD, FILTER_LOW, 2200, 4);
    instrument_tune(SL_PAD, 0.07f);                          // unison detune shimmer

    reverb(0.62f, 0.40f);                                    // one shared hall; sends ride the knob

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');

    // start the held pad chord — it breathes from frame one
    for (int k = 0; k < 3; k++) padH[k] = note_on(SL_PAD, CHORD[0][k], 6);

    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
}

// Re-apply the master effects, but ONLY when a knob actually moved. Reconfiguring
// crush/tape/eq every frame (60×/s) churns the bus DSP and was the source of the
// stutter — these are set-and-hold, not per-frame controls.
static void apply_fx(void) {
    static float aCrush = -1, aLo = -2, aMid = -2, aHi = -2, aTape = -1, aSpace = -1;
    if (k_crush != aCrush) {
        if (k_crush < 0.02f) crush(8, 6, 0);                 // mix 0 = off
        else                 crush(12.0f - 9.0f * k_crush, 6, k_crush);
        aCrush = k_crush;
    }
    if (k_eqLo != aLo || k_eqMid != aMid || k_eqHi != aHi) {
        eq((k_eqLo - 0.5f) * 24.0f, (k_eqMid - 0.5f) * 24.0f, (k_eqHi - 0.5f) * 24.0f);
        aLo = k_eqLo; aMid = k_eqMid; aHi = k_eqHi;
    }
    if (k_tape != aTape) {
        if (k_tape < 0.02f) tape(0, 0, 0);
        else                tape(k_tape, k_tape * 0.7f, 0.40f + 0.40f * k_tape);
        aTape = k_tape;
    }
    if (k_space != aSpace) {
        instrument_reverb(SL_STAB, k_space * 0.8f);
        for (int k = 0; k < 3; k++)
            if (padH[k] >= 0) note_reverb(padH[k], k_space * 0.7f);  // held → live send
        aSpace = k_space;
    }
}

void update() {
    bpm(tempo);

    // ── transport: advance the playhead off the synth clock ──
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   sixteenth = (int)pos16;
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;

        int ci = (sixteenth / 32) % 4;                       // new chord every 2 bars
        if (ci != lastChord) {
            lastChord = chordIdx = ci;
            for (int k = 0; k < 3; k++)                      // glide the pad to it
                if (padH[k] >= 0) note_pitch(padH[k], CHORD[ci][k]);
        }
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) { play_row(r); flash[r] = frame(); }
    }

    // ── THE PUMP (faked sidechain) — duck the pad on every kick, recover ──
    pumpEnv *= 0.90f;                                        // release (fixed for v1)
    openness = 1.0f - k_pump * pumpEnv;                      // 0 = ducked shut, 1 = open
    int padCut = 350 + (int)(1900 * openness);
    if (padCut != lastPadCut) {                              // only push when it moved
        for (int k = 0; k < 3; k++)
            if (padH[k] >= 0) note_cutoff(padH[k], padCut);
        lastPadCut = padCut;
    }

    // ── the REAL master rack — re-applied ONLY when a knob actually moved ──
    apply_fx();

    // ── edit: WASD cursor, Z toggle ──
    if (btnp(0, BTN_UP))    curR = (curR + ROWS - 1) % ROWS;
    if (btnp(0, BTN_DOWN))  curR = (curR + 1) % ROWS;
    if (btnp(0, BTN_LEFT))  curC = (curC + STEPS - 1) % STEPS;
    if (btnp(0, BTN_RIGHT)) curC = (curC + 1) % STEPS;
    if (btnp(0, BTN_A)) set_cell(curR, curC, !grid[curR][curC]);

    if (btnp(1, BTN_LEFT))  tempo = max(70,  tempo - 4);
    if (btnp(1, BTN_RIGHT)) tempo = min(160, tempo + 4);

    // ── touch/mouse: tap a cell to toggle, drag across to paint ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), r, c;
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP || !cell_rc(tx, ty, &r, &c)) continue; // outside grid → the rack owns it
            p = freeP;
            *p = (Ptr){ id, !grid[r][c], r, c };
            set_cell(r, c, p->paint);
        } else if (cell_rc(tx, ty, &r, &c) && (r != p->lastR || c != p->lastC)) {
            p->lastR = r; p->lastC = c;
            set_cell(r, c, p->paint);
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

#ifdef DE_TRACE
    watch("tempo",    "%d", tempo);
    watch("chord",    "%s", CNAME[chordIdx]);
    watch("openness", "%.2f", openness);
#endif
}

// a knob slot drawn but DORMANT — the panel's home for the next bus effect
static void glue_slot(int x, int y) {
    int r = UI_KNOB_R;
    circfill(x, y, r, CLR_DARKER_GREY);
    circ(x, y, r, CLR_DARK_GREY);
    line(x, y, x, y - (r - 3), CLR_DARK_GREY);
    print("GLUE", x - text_width("GLUE") / 2, y + r + 3, CLR_DARK_GREY);
    font(FONT_SMALL);
    print("soon", x - text_width("soon") / 2, y - 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("GROOVEBOX", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 318, 4, CLR_LIGHT_GREY);

    // playhead marker
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);

    for (int r = 0; r < ROWS; r++) {
        bool hot = (frame() - flash[r]) < 5;
        font(FONT_SMALL);
        print(LABEL[r], 4, GY + r * SY + 5, hot ? CLR_WHITE : LIT[r]);
        font(FONT_NORMAL);
        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                rectfill(x, y, CW, CH, LIT[r]);
                if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                int bg = (c == cur_step) ? CLR_DARK_GREY
                       : (c % 4 == 0)    ? CLR_DARKER_BLUE
                                         : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }
    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);

    // ── the PUMP meter — the pad "breathing" made visible (game-feel: tie FX to a sight) ──
    int my = 132, mw = 318 - GX;
    print("PUMP", 4, my - 1, CLR_LIGHT_GREY);
    rectfill(GX, my, mw, 7, CLR_DARKER_GREY);
    int lvl = (int)(openness * mw);
    rectfill(GX, my, lvl, 7, pumpEnv > 0.5f ? CLR_RED : CLR_PINK);
    rect(GX, my, mw, 7, CLR_DARK_GREY);

    // chord + a 4-beat indicator
    print(str("CHORD %s", CNAME[chordIdx]), 4, my + 12, CLR_GREEN);
    for (int b = 0; b < 4; b++) {
        int on = (cur_step / 4) == b;
        circfill(250 + b * 16, my + 15, 3, on ? CLR_WHITE : CLR_DARK_GREY);
    }

    // ── the rack: 5 real knobs + the dormant GLUE slot, an ORDER toggle ──
    ui_begin();
    int ky = 172, kx[8] = { 20, 60, 100, 140, 180, 220, 260, 300 };
    font(FONT_SMALL);
    ui_knob(&k_pump,  kx[0], ky, "PUMP");
    ui_knob(&k_crush, kx[1], ky, "CRUSH");
    ui_knob(&k_eqLo,  kx[2], ky, "LO");
    ui_knob(&k_eqMid, kx[3], ky, "MID");
    ui_knob(&k_eqHi,  kx[4], ky, "HI");
    ui_knob(&k_tape,  kx[5], ky, "TAPE");
    ui_knob(&k_space, kx[6], ky, "SPACE");
    glue_slot(kx[7], ky);
    // bracket the three EQ bands as one 3-band cluster
    line(kx[2] - 9, ky - 13, kx[4] + 9, ky - 13, CLR_DARK_GREY);
    print("EQ", kx[3] - text_width("EQ") / 2, ky - 11, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    if (ui_button(110, 2, 96, 12, orderSwapped ? "CRUSH>EQ" : "EQ>CRUSH")) {
        orderSwapped = !orderSwapped;
        int kinds[3] = { FX_TAPE, FX_EQ, FX_CRUSH };         // default: tape→eq→crush
        if (orderSwapped) { kinds[1] = FX_CRUSH; kinds[2] = FX_EQ; }
        fx_order(0, kinds, 3);                               // master bus insert order
    }
    ui_end();
}
