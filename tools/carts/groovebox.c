#include "studio.h"
#include "ui.h"
#include <math.h>

// GROOVEBOX — a melodic / progressive-house box and a HOME for the summed-bus effects.
//
// The genre is chosen for the headline effect: melodic house is where SIDECHAIN is the
// soul, not a trick (Lane 8 / Ben Böhmer / Eric Prydz). A LUSH sustained PAD on a slow
// emotional minor progression breathes against a soft four-floor kick — that breathing
// IS the beauty — while a cascading ARP lead carries the tune over the top.
//
// The pedalboard cart owns the SERIAL-insert family (one guitar → a chain of pedals).
// This cart owns the other half: the SUMMED-BUS family — many looping voices summed into
// one master, then sculpted as a whole. Those effects can't be shown on a single held
// chord; the loop has to run so you can hear the mix breathe. So: a 6-row × 16-step grid
// plays itself, freeing both hands to ride the rack along the bottom.
//
// The headline is the PUMP — a REAL summed-bus sidechain (effects-bus Increment D):
// sidechain_key(SL_KICK, 0, 1) routes the kick as the trigger, and sidechain(0, 0,
// amount, …) ducks the WHOLE master mix on every kick — bass, stab, pad all breathe
// together (not the old per-voice filter fake). GLUE is the same engine compressor
// self-keyed (glue(), no trigger) — the mix squashed as one lump. The engine has one
// comp per bus, so PUMP and GLUE share the master comp and the UI keeps them exclusive.
// The PUMP meter mirrors the duck so you can see the mix breathe.
//
// Everything else on the rack is REAL and already in the engine, here shown on a
// full mix: CRUSH (summed bitcrush — the SP-1200 grit that only exists on the sum),
// EQ (3-band), TAPE, SPACE (reverb as a real master INSERT via reverb_insert), the
// bipolar FILTER (the DJ filter — center open, ride it down to a muffle / up to thin,
// resonance rising as you close), and the ORDER toggle that reorders the master inserts:
// CRUSH→VERB (clean space on a gritty mix) vs VERB→CRUSH (the wet tail gets crushed
// — grainy/vaporwave space). Reverb's position only matters because it's an insert.
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

// instrument slots 5..11 — the kit + the lead arp + the pumped pad
enum { SL_KICK = 5, SL_CLAP, SL_CHAT, SL_OHAT, SL_BASS, SL_ARP, SL_PAD };

static const char *LABEL[ROWS] = { "KICK", "CLAP", "CHAT", "OHAT", "BASS", "ARP" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_PINK, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_BLUE, CLR_GREEN };

// a spacious melodic/progressive-house groove — soft four-floor, off-beat sub, a
// cascading arp lead, all breathing under the lush sidechained PAD
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor (the pump trigger)
    "....x.......x...",   // clap — backbeat
    "..x...x...x...x.",   // closed hat — the off-beat tick
    "............x...",   // open hat — a lift before the turn
    "..x...x...x...x.",   // bass — rolling off-beat sub
    "x.x.x.x.x.x.x.x.",   // arp — cascading 8th-note lead over the chord
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
static int  tempo    = 123;
static int  chordIdx = 0, lastChord = -1;
static int  arpIdx = 0;                 // cascading-arp position (advances each ARP step)

static int   padH[3] = { -1, -1, -1 }; // the held pad chord
static float pumpEnv = 0;              // VISUAL only — 1 on each kick, decays (mirrors the engine duck on the meter)
static float openness = 1;             // meter level 0..1

// rack knobs (0..1). EQ is the engine's real 3 bands (0.5 = flat) — a louder EQ is
// what makes the fx_order CRUSH↔EQ toggle audible (gentle = no difference to hear).
static float k_pump = 0.65f, k_glue = 0.0f, k_crush = 0.0f, k_tape = 0.20f, k_space = 0.40f;
static float k_eqLo = 0.5f, k_eqMid = 0.5f, k_eqHi = 0.5f;
static float k_filter = 0.5f;          // the DJ filter — bipolar: 0.5 = open, < = low-pass down, > = high-pass up
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
                pumpEnv = 1.0f; break;                       // ← visual meter only (real trigger = the kick's level via sidechain_key)
        case 1: hit(64, SL_CLAP, 4, 35);                     // clap — three hands
                schedule_hit(12, 64, SL_CLAP, 3, 30);
                schedule_hit(24, 64, SL_CLAP, 4, 60); break;
        case 2: hit(92, SL_CHAT, 3, 24); break;              // closed hat
        case 3: hit(92, SL_OHAT, 2, 170); break;             // open hat
        case 4: hit(ROOT[chordIdx], SL_BASS, 5, 150); break; // rolling sub root
        case 5: {                                            // ARP — a cascading lead over the chord (the topline)
            int i = arpIdx % 6;                              // root·3rd·5th, then the octave up — rising
            hit(CHORD[chordIdx][i % 3] + 12 * (i / 3), SL_ARP, 4, 200);
            arpIdx++;
        } break;
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
    instrument(SL_BASS, INSTR_SAW, 2, 160, 3, 130);          // round rolling sub
    instrument_filter(SL_BASS, FILTER_LOW, 480, 3);
    instrument(SL_ARP, INSTR_SAW, 1, 90, 0, 220);            // a plucky lead — short decay, ringing tail
    instrument_filter(SL_ARP, FILTER_LOW, 3000, 5);
    instrument_echo(SL_ARP, 0.28f);                          // dreamy dotted-8th tail — the prog-house topline
    echo(366, 0.34f, 0.42f);                                 // ~dotted-8th at 123 BPM, dark repeats

    // the PAD (slot 11) — a LUSH wide detuned-saw wash on a slow swell. The sidechain victim
    // (it breathes against the kick), so the longer + more sustained it is, the more beautiful the pump.
    instrument(SL_PAD, INSTR_SAW, 220, 900, 7, 1400);        // slow attack = a swell; long release = it hangs
    instrument_filter(SL_PAD, FILTER_LOW, 2400, 3);
    instrument_tune(SL_PAD, 0.12f);                          // wider unison detune = the analog shimmer

    // master insert chain: tape → eq → crush → reverb → filter. Reverb is a REAL insert
    // now (reverb_insert), so the ORDER toggle can move it before/after crush — audible.
    // FILTER is LAST so the DJ filter sculpts the whole summed mix (incl. the wet tail).
    // NB: fx_order REPLACES the bus chain, so FX_FILTER MUST be listed or the knob is dead
    // (it's the default chain's 10th pedal — omitting it here silently bypasses filter()).
    int order[5] = { FX_TAPE, FX_EQ, FX_CRUSH, FX_REVERB, FX_FILTER };
    fx_order(0, order, 5);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');

    // start the held pad chord — it breathes from frame one
    for (int k = 0; k < 3; k++) padH[k] = note_on(SL_PAD, CHORD[0][k], 6);

    sidechain_key(SL_KICK, 0, 1.0f);   // the kick IS the sidechain trigger (key 0) — drives the real PUMP

    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
}

// Re-apply the master effects, but ONLY when a knob actually moved. Reconfiguring
// crush/tape/eq every frame (60×/s) churns the bus DSP and was the source of the
// stutter — these are set-and-hold, not per-frame controls.
static void apply_fx(void) {
    static float aCrush = -1, aLo = -2, aMid = -2, aHi = -2, aTape = -1, aSpace = -1, aPump = -1, aGlue = -1, aFilt = -1;
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
        reverb_insert(0.62f, 0.40f, k_space);    // a REAL master insert (mix 0 = bypass), not a send —
        aSpace = k_space;                        // so its chain position (ORDER toggle) is audible
    }
    // PUMP & GLUE share the ONE master compressor (engine: one comp per bus). PUMP = kick-keyed
    // sidechain; GLUE = self-keyed bus comp. The UI keeps them exclusive (raising one zeroes the other).
    if (k_pump != aPump || k_glue != aGlue) {
        if (k_pump > 0.02f)      sidechain(0, 0, k_pump, 1, 140);   // the real summed-bus pump (kick → key 0)
        else if (k_glue > 0.02f) glue(0, k_glue, 8, 150);          // the bus-glue compressor
        else                     sidechain(0, 0, 0.0f, 1, 140);     // both off → master comp dormant
        aPump = k_pump; aGlue = k_glue;
    }
    // the DJ FILTER — one bipolar knob: center = open, turn down = low-pass closing to a muffle,
    // turn up = high-pass thinning out. Resonance rises as you close it (the build-up scream).
    if (k_filter != aFilt) {
        if (fabsf(k_filter - 0.5f) < 0.02f) filter(FILTER_OFF, 0.0f, 0.0f);                          // center = open
        else if (k_filter < 0.5f) { float t = (0.5f - k_filter) * 2.0f;                              // low-pass down
            filter(FILTER_LOW,  18000.0f * powf(150.0f / 18000.0f, t), 0.2f + 0.5f * t); }
        else                      { float t = (k_filter - 0.5f) * 2.0f;                              // high-pass up
            filter(FILTER_HIGH,    20.0f * powf(6000.0f / 20.0f,    t), 0.2f + 0.5f * t); }
        aFilt = k_filter;
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

    // ── PUMP meter (VISUAL) — the real duck now happens in the ENGINE (master sidechain, keyed off
    // the kick via sidechain_key). This kick-triggered envelope just mirrors it on the meter below. ──
    pumpEnv *= 0.90f;                                        // decays after each kick
    openness = 1.0f - (k_pump > 0.02f ? k_pump : k_glue) * pumpEnv;  // shows whichever comp is active

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

    // ── the rack: PUMP · CRUSH · EQ(LO/MID/HI) · TAPE · SPACE · GLUE · FILTER, + the ORDER toggle ──
    ui_begin();
    int ky = 172, kx[9] = { 18, 54, 90, 126, 162, 198, 234, 270, 306 };
    font(FONT_SMALL);
    if (ui_knob(&k_pump, kx[0], ky, "PUMP") && k_pump > 0.02f) k_glue = 0.0f;   // PUMP & GLUE share one master comp —
    ui_knob(&k_crush, kx[1], ky, "CRUSH");
    ui_knob(&k_eqLo,  kx[2], ky, "LO");
    ui_knob(&k_eqMid, kx[3], ky, "MID");
    ui_knob(&k_eqHi,  kx[4], ky, "HI");
    ui_knob(&k_tape,  kx[5], ky, "TAPE");
    ui_knob(&k_space, kx[6], ky, "SPACE");
    if (ui_knob(&k_glue, kx[7], ky, "GLUE") && k_glue > 0.02f) k_pump = 0.0f;   // … so raising one zeroes the other
    ui_knob(&k_filter, kx[8], ky, "FILTER");   // the bipolar DJ filter — center = open
    // bracket the three EQ bands as one 3-band cluster
    line(kx[2] - 9, ky - 13, kx[4] + 9, ky - 13, CLR_DARK_GREY);
    print("EQ", kx[3] - text_width("EQ") / 2, ky - 11, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    if (ui_button(104, 2, 110, 12, orderSwapped ? "VERB>CRUSH" : "CRUSH>VERB")) {
        orderSwapped = !orderSwapped;
        // tape→eq stay first; toggle the CRUSH/REVERB tail. CRUSH→VERB = clean space on a
        // gritty mix; VERB→CRUSH = the wet reverb tail gets crushed (grainy/vaporwave space).
        int kinds[5] = { FX_TAPE, FX_EQ, FX_CRUSH, FX_REVERB, FX_FILTER };
        if (orderSwapped) { kinds[2] = FX_REVERB; kinds[3] = FX_CRUSH; }
        fx_order(0, kinds, 5);                               // master bus insert order (FILTER stays last)
    }
    ui_end();
}
