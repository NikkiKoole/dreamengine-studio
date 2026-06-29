/* de:meta
{
  "title": "Kaoss",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer"
  ],
  "lineage": "After the Korg Kaoss Pad — an XY effects pad wired to a groovebox-derived melodic-techno loop; novel in demonstrating the ride-safe vs set-and-hold effects split in a live-performance context.",
  "homage": "Korg Kaoss Pad",
  "description": "An XY effects pad, after the Korg Kaoss Pad / Kaossilator — a driving techno loop plays itself so both hands are free to MANGLE the whole mix with your finger on a big XY pad. Pick a PROGRAM (1-4) and the X/Y axes drive that effect's two parameters; touch the pad to engage, lift to bypass, or HOLD to latch a mangle. Only the engine's ride-safe effects are on the pad (filter / varispeed / echo / tremolo — the ones built to sweep live), so it never stutters. FILTER: x = lowpass < open > highpass, y = resonance (the DJ filter). ECHO: x = delay time, y = feedback (the dub throw). GATE: x = chop rate, y = depth (square-tremolo stutter). TAPE: x = speed, tape-stop < > spin-up (varispeed dive). The performance masher that onenote and grenadier plug into. Drag the pad (mouse/finger); 1-4 pick program; SPACE = HOLD; left/right = tempo."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// KAOSS — an XY effects pad, after the Korg Kaoss Pad / Kaossilator. A driving techno
// loop plays itself so both hands are free; your finger on the big pad MANGLES the whole
// mix live. Pick a PROGRAM, then the X/Y axes drive that effect's two parameters — touch
// to engage, lift to bypass (or HOLD to latch a mangle). The performance instrument the
// whole effects bus was waiting for; the masher onenote/grenadier plug into.
//
// Only the engine's RIDE-SAFE effects are on the pad (filter/varispeed/echo/tremolo —
// the ones built to be swept every frame; the buffer-rebuilders like crush/tape would
// stutter, so they're left off, per the set-and-hold rule).
//
//   FILTER   x = lowpass < open > highpass   ·  y = resonance   (the DJ filter)
//   ECHO     x = delay time                  ·  y = feedback    (the dub throw)
//   GATE     x = chop rate                    ·  y = depth       (square tremolo stutter)
//   TAPE     x = speed: tape-STOP < > spin-up                    (varispeed dive)
//
//   DRAG the pad (mouse/finger) · 1-4 pick program · SPACE = HOLD latch · ← / → tempo

// ── the loop: a melodic-techno canvas cribbed from groovebox.c ──
enum { SL_KICK = 5, SL_CLAP, SL_CHAT, SL_OHAT, SL_BASS, SL_ARP };
#define ROWS 6
#define STEPS 16
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor
    "....x.......x...",   // clap — backbeat
    "..x...x...x...x.",   // closed hat — off-beat tick
    "............x...",   // open hat — lift before the turn
    "x.x.x.x.x.x.x.x.",   // bass — driving 8ths
    "x.x.x.x.x.x.x.x.",   // arp — cascading saw lead
};
static const int CHORD[4][3] = { {57,60,64},{57,60,65},{55,60,64},{55,59,62} };
static const int ROOT[4]     = { 45, 41, 48, 43 };
static bool grid[ROWS][STEPS];
static int  cur_step = 0, last_16 = -1, tempo = 128;
static int  chordIdx = 0, lastChord = -1, arpIdx = 0;

// ── the pad ──
enum { P_FILTER, P_ECHO, P_GATE, P_TAPE, NPROG };
static const char *PNAME[NPROG] = { "FILTER", "ECHO", "GATE", "TAPE" };
static const char *PX[NPROG] = { "lp < open > hp", "delay time", "chop rate", "speed: stop<>spin" };
static const char *PY[NPROG] = { "resonance", "feedback", "depth", "-" };
static const int   PCOL[NPROG] = { CLR_GREEN, CLR_BLUE, CLR_ORANGE, CLR_PINK };
static int   prog = P_FILTER;
static bool  engaged = false, hold = false;
static float px = 0.5f, py = 0.0f;     // pad position (last touched)

#define PAD_X 8
#define PAD_Y 26
#define PAD_W 188
#define PAD_H 162

static void play_row(int r) {
    switch (r) {
        case 0: hit(72, INSTR_NOISE, 2, 12); hit(34, SL_KICK, 6, 250); break;
        case 1: hit(64, SL_CLAP, 4, 35); schedule_hit(12, 64, SL_CLAP, 3, 30);
                schedule_hit(24, 64, SL_CLAP, 4, 60); break;
        case 2: hit(92, SL_CHAT, 3, 24); break;
        case 3: hit(92, SL_OHAT, 2, 170); break;
        case 4: hit(ROOT[chordIdx], SL_BASS, 5, 150); break;
        case 5: { int i = arpIdx % 6; hit(CHORD[chordIdx][i % 3] + 12 * (i / 3), SL_ARP, 4, 200);
                  arpIdx++; } break;
    }
}

// drive the selected program from the pad. filter()/varispeed()/echo() are ride-safe
// (swept every frame); tremolo() RESETS its LFO phase per call, so it's re-applied only
// on a real change. Only ONE program is active; the rest sit bypassed/transparent.
static void apply_fx(void) {
    // FILTER (bipolar: left = lowpass closing, center = open, right = highpass) — ride-safe
    if (prog == P_FILTER && engaged) {
        if (px < 0.47f)      filter(FILTER_LOW,  18000.0f * powf(150.0f / 18000.0f, (0.47f - px) / 0.47f), 0.1f + 0.85f * py);
        else if (px > 0.53f) filter(FILTER_HIGH, 20.0f    * powf(6000.0f / 20.0f,   (px - 0.53f) / 0.47f), 0.1f + 0.85f * py);
        else                 filter(FILTER_OFF, 0, 0);
    } else filter(FILTER_OFF, 0, 0);

    // TAPE (varispeed: pow(4, ±1) = ±2 octaves; 0.5 on the pad = normal) — ride-safe
    varispeed((prog == P_TAPE && engaged) ? powf(4.0f, (px - 0.5f) * 2.0f) : 1.0f);

    // ECHO — echo() is a SEND, so the loop must FEED it. Open the sends only while ECHO
    // is the selected program (set once on program change); then ride time/feedback.
    static int last_send_prog = -1;
    if (prog != last_send_prog) {
        int slots[ROWS] = { SL_KICK, SL_CLAP, SL_CHAT, SL_OHAT, SL_BASS, SL_ARP };
        float s = (prog == P_ECHO) ? 0.45f : 0.0f;
        for (int i = 0; i < ROWS; i++) instrument_echo(slots[i], s);
        last_send_prog = prog;
    }
    echo(40 + (int)(px * 560), (prog == P_ECHO && engaged) ? py * 0.92f : 0.0f, 0.5f);   // time-slew safe

    // GATE (square tremolo = rhythmic chop). Quantize rate to musical steps + bucket the
    // depth, and re-apply ONLY when one changes — calling tremolo() every frame restarts
    // its LFO (the glitch). Turn it off once when leaving the program.
    static const float GR[6] = { 2, 4, 6, 8, 12, 16 };
    static int lgi = -1, lgd = -1;
    if (prog == P_GATE) {
        int gi = (int)(px * 5.99f); if (gi < 0) gi = 0; if (gi > 5) gi = 5;
        int gd = engaged ? (int)(py * 10 + 0.5f) : 0;
        if (gi != lgi || gd != lgd) { tremolo(GR[gi], gd / 10.0f, LFO_SHAPE_SQUARE); lgi = gi; lgd = gd; }
    } else if (lgd != 0) {
        tremolo(8, 0, LFO_SHAPE_SQUARE); lgi = -1; lgd = 0;
    }
}

void init() {
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);
    instrument_drive(SL_KICK, 0.28f);
    instrument(SL_CLAP, INSTR_NOISE, 0, 60, 0, 30);  instrument_filter(SL_CLAP, FILTER_BAND, 1100, 5);
    instrument(SL_CHAT, INSTR_NOISE, 0, 24, 0, 14);  instrument_filter(SL_CHAT, FILTER_HIGH, 7000, 2);
    instrument(SL_OHAT, INSTR_NOISE, 0, 180, 0, 90); instrument_filter(SL_OHAT, FILTER_HIGH, 6500, 2);
    instrument(SL_BASS, INSTR_SAW, 2, 160, 3, 130);  instrument_filter(SL_BASS, FILTER_LOW, 480, 3);
    instrument(SL_ARP, INSTR_SAW, 1, 90, 0, 220);    instrument_filter(SL_ARP, FILTER_LOW, 3000, 5);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');
}

void update() {
    if (btnp(0, BTN_LEFT))  tempo = max(90,  tempo - 4);
    if (btnp(0, BTN_RIGHT)) tempo = min(170, tempo + 4);
    if (keyp('1')) prog = P_FILTER;
    if (keyp('2')) prog = P_ECHO;
    if (keyp('3')) prog = P_GATE;
    if (keyp('4')) prog = P_TAPE;
    if (keyp(KEY_SPACE)) hold = !hold;
    bpm(tempo);

    // transport (groovebox loop engine)
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int sixteenth = (int)pos16;
    if (sixteenth != last_16) {
        last_16 = sixteenth;
        cur_step = sixteenth % STEPS;
        int ci = (sixteenth / 32) % 4;
        if (ci != lastChord) lastChord = chordIdx = ci;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) play_row(r);
    }

    // pad: any finger/mouse inside it engages + sets X/Y; lift bypasses unless HOLD
    bool touching = false;
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PAD_X && tx < PAD_X + PAD_W && ty >= PAD_Y && ty < PAD_Y + PAD_H) {
            touching = true;
            px = clamp((tx - PAD_X) / (float)PAD_W, 0, 1);
            py = clamp(1.0f - (ty - PAD_Y) / (float)PAD_H, 0, 1);
        }
    }
    engaged = touching || hold;
    apply_fx();

#ifdef DE_TRACE
    watch("prog", "%s", PNAME[prog]);
    watch("eng",  "%d", engaged);
    watch("px",   "%.2f", px);
    watch("py",   "%.2f", py);
#endif
}

void draw() {
    cls(CLR_BROWNISH_BLACK);
    print("KAOSS", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 318, 4, CLR_LIGHT_GREY);

    // ── the XY pad ──
    int col = PCOL[prog];
    rectfill(PAD_X, PAD_Y, PAD_W, PAD_H, engaged ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
    for (int g = 1; g < 8; g++) {
        line(PAD_X + g * PAD_W / 8, PAD_Y, PAD_X + g * PAD_W / 8, PAD_Y + PAD_H, CLR_DARK_GREY);
        line(PAD_X, PAD_Y + g * PAD_H / 8, PAD_X + PAD_W, PAD_Y + g * PAD_H / 8, CLR_DARK_GREY);
    }
    rect(PAD_X, PAD_Y, PAD_W, PAD_H, engaged ? CLR_WHITE : CLR_MEDIUM_GREY);
    int dx = PAD_X + (int)(px * PAD_W), dy = PAD_Y + (int)((1 - py) * PAD_H);
    if (engaged) {
        line(PAD_X, dy, PAD_X + PAD_W, dy, col);
        line(dx, PAD_Y, dx, PAD_Y + PAD_H, col);
        circfill(dx, dy, 6, col); circ(dx, dy, 6, CLR_WHITE);
    } else {
        circ(dx, dy, 5, CLR_DARK_GREY);
    }
    font(FONT_SMALL);
    print(str("%s%s", PNAME[prog], hold ? "  (HOLD)" : ""), PAD_X + 2, PAD_Y + 1, col);
    print(PY[prog], PAD_X + 2, PAD_Y + 9, CLR_DARK_GREY);                       // y axis, top-left
    print(PX[prog], PAD_X + 2, PAD_Y + PAD_H - 8, CLR_DARK_GREY);               // x axis, bottom
    font(FONT_NORMAL);

    // ── program buttons + HOLD + beat dots (drawn manually; tapp() captures clicks) ──
    int bx = 204, bw = 110, by = 26;
    for (int p = 0; p < NPROG; p++) {
        int yy = by + p * 22;
        bool sel = (p == prog);
        if (tapp(bx, yy, bw, 19)) prog = p;
        rectfill(bx, yy, bw, 19, sel ? PCOL[p] : CLR_DARK_GREY);
        rect(bx, yy, bw, 19, CLR_MEDIUM_GREY);
        print(str("%d %s", p + 1, PNAME[p]), bx + 6, yy + 6, sel ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    }
    int hy = by + 4 * 22 + 2;
    if (tapp(bx, hy, bw, 19)) hold = !hold;
    rectfill(bx, hy, bw, 19, hold ? CLR_YELLOW : CLR_DARK_GREY);
    rect(bx, hy, bw, 19, CLR_MEDIUM_GREY);
    print(hold ? "HOLD *" : "HOLD", bx + 6, hy + 6, hold ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);

    // beat indicator + hints
    for (int b = 0; b < 4; b++)
        circfill(bx + 8 + b * 16, by + 5 * 22 + 8, 3, (cur_step / 4) == b ? CLR_WHITE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print("1-4 program  space HOLD", bx, by + 5 * 22 + 18, CLR_DARK_GREY);
    print("drag the pad - L/R tempo", bx, by + 5 * 22 + 26, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
