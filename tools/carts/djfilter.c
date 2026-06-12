#define UI_KNOB_R 15            // big knobs — this whole cart is two knobs and a curve
#define UI_KNOB_DRAG_PX 90      // a long, deliberate sweep (you RIDE this, you don't flick it)
#include "studio.h"
#include "ui.h"
#include <math.h>

// ── DJ FILTER ─────────────────────────────────────────────────────────────
// The legendary one-knob mixer filter — Allen & Heath XONE:92, Pioneer DJM —
// and at the screaming end, a Sherman Filterbank. The showcase cart for the
// master filter() bus (FX_FILTER, audio-notes §17 item 15): the effect IS the
// instrument. A bright four-floor house loop plays itself so both hands are
// free to RIDE the filter — exactly like spacecho rides the echo.
//
// THE control is one BIPOLAR knob, center = OPEN:
//   center        FILTER_OFF — flat, full signal
//   turn LEFT     FILTER_LOW,  cutoff 18kHz → 150Hz  (muffled thump)
//   turn RIGHT    FILTER_HIGH, cutoff 20Hz → 6kHz    (thin / telephone)
// That bipolar map is the most satisfying single control in dance music. The
// RES knob screams it into acid territory; the live response curve up top is
// the centerpiece — watch the slope tilt and the resonant peak rise as you ride.
//
//   FILTER knob   the star — drag (or ← → nudge). Center detent = bypass-open.
//   RES knob      resonance — drag (or arrow-key UP/DOWN). Crank it on the build.
//   BUILD button  the money shot: a breakdown → 8-bar filter-sweep riser → DROP
//                 (X also triggers it). The genre's signature gesture.
//   BYPASS button A/B the filter off vs on (Z also toggles). Every fx showcase has it.

// ── voices: cribbed straight from groovebox.c — bright melodic-house canvas ──
enum { SL_KICK = 5, SL_CLAP, SL_CHAT, SL_OHAT, SL_BASS, SL_ARP, SL_PAD };

#define ROWS  6
#define STEPS 16
static const int LIT[ROWS] = { CLR_RED, CLR_PINK, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_BLUE, CLR_GREEN };
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // kick — four on the floor
    "....x.......x...",   // clap — backbeat
    "..x...x...x...x.",   // closed hat — off-beat tick
    "............x...",   // open hat — a lift before the turn
    "..x...x...x...x.",   // bass — rolling off-beat sub
    "x.x.x.x.x.x.x.x.",   // arp — cascading 8th-note saw lead
};
// a 4-chord minor vamp (Am i–VI–III–VII) — same as groovebox
static const int CHORD[4][3] = { {57,60,64}, {57,60,65}, {55,60,64}, {55,59,62} };
static const int ROOT[4]     = { 45, 41, 48, 43 };

static bool grid[ROWS][STEPS];
static int  flash[ROWS];           // frame() each row last fired
static int  cur16 = 0;             // MONOTONIC sixteenth counter (drives transport + performance)
static int  last16 = -1, playstep = 0;
static int  chordIdx = 0, arpIdx = 0;
static int  padH[3] = { -1,-1,-1 };
static int  tempo = 124;

// ── the two knobs ──────────────────────────────────────────────────────────
static float k_filter = 0.5f;      // 0..1, 0.5 = OPEN (bipolar)
static float k_res    = 0.20f;     // 0..1 resonance
static bool  bypass   = false;

// ── the BUILD performance ───────────────────────────────────────────────────
// breakdown → 8-bar filter sweep with rising resonance → DROP (snap open + bang)
#define PERF_LEN16 128             // 8 bars
static int   perfStart = -1;       // cur16 when BUILD pressed; -1 = idle
static int   dropFlash = 0;        // frames left of the DROP flash

// derived-from-the-knob filter target (the bipolar DJ map)
static void filter_target(float k, float res, int *mode, float *cut, float *q) {
    if (bypass || fabsf(k - 0.5f) < 0.02f) { *mode = FILTER_OFF; *cut = 1000.0f; *q = res; return; }
    if (k < 0.5f) { float t = (0.5f - k) * 2.0f;        // LP closing down 18k → 150
        *mode = FILTER_LOW;  *cut = 18000.0f * powf(150.0f / 18000.0f, t); }
    else          { float t = (k - 0.5f) * 2.0f;        // HP opening up 20 → 6k
        *mode = FILTER_HIGH; *cut = 20.0f * powf(6000.0f / 20.0f, t); }
    *q = res;
}

// SET-AND-HOLD: re-aim filter() ONLY when a value actually moved (the intro note
// in effects-recipes.md — reconfiguring the bus DSP 60×/s stutters the audio).
static void apply_filter(void) {
    static int   aMode = -2;
    static float aCut = -1, aRes = -1;
    int mode; float cut, res;
    filter_target(k_filter, k_res, &mode, &cut, &res);
    if (mode != aMode || fabsf(cut - aCut) > 0.5f || fabsf(res - aRes) > 0.001f) {
        filter(mode, cut, res);
        aMode = mode; aCut = cut; aRes = res;
    }
}

void init(void) {
    bpm(tempo);
    // the kit (groovebox recipes, house-tuned)
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);
    instrument_drive(SL_KICK, 0.28f);
    instrument(SL_CLAP, INSTR_NOISE, 0, 60, 0, 30);
    instrument_filter(SL_CLAP, FILTER_BAND, 1100, 5);
    instrument(SL_CHAT, INSTR_NOISE, 0, 24, 0, 14);
    instrument_filter(SL_CHAT, FILTER_HIGH, 7000, 2);
    instrument(SL_OHAT, INSTR_NOISE, 0, 180, 0, 90);
    instrument_filter(SL_OHAT, FILTER_HIGH, 6500, 2);
    instrument(SL_BASS, INSTR_SAW, 2, 160, 3, 130);
    instrument_filter(SL_BASS, FILTER_LOW, 480, 3);
    instrument(SL_ARP, INSTR_SAW, 1, 90, 0, 220);            // plucky saw lead
    instrument_tune(SL_ARP, 0.10f);                          // detuned = bright + full (more for the filter to chew)
    // a LUSH wide detuned-saw pad — the harmonically rich bed the sweep lives on
    instrument(SL_PAD, INSTR_SAW, 220, 900, 7, 1400);
    instrument_tune(SL_PAD, 0.14f);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++) grid[r][c] = (PRESET[r][c] == 'x');
    for (int k = 0; k < 3; k++) padH[k] = note_on(SL_PAD, CHORD[0][k], 6);

    apply_filter();   // dormant FILTER_OFF until you ride it (byte-identical to bypass)
}

static void play_row(int r) {
    switch (r) {
        case 0: hit(72, INSTR_NOISE, 2, 12); hit(34, SL_KICK, 6, 250); break;
        case 1: hit(64, SL_CLAP, 4, 35);
                schedule_hit(12, 64, SL_CLAP, 3, 30);
                schedule_hit(24, 64, SL_CLAP, 4, 60); break;
        case 2: hit(92, SL_CHAT, 3, 24); break;
        case 3: hit(92, SL_OHAT, 2, 170); break;
        case 4: hit(ROOT[chordIdx], SL_BASS, 5, 150); break;
        case 5: { int i = arpIdx % 6;
                  hit(CHORD[chordIdx][i % 3] + 12 * (i / 3), SL_ARP, 4, 200);
                  arpIdx++; } break;
    }
    flash[r] = frame();
}

void update(void) {
    bpm(tempo);

    // ── BUILD performance: drives the knobs itself, mutes the kit, then DROPS ──
    bool performing = perfStart >= 0;
    if (performing) {
        float p = (cur16 - perfStart) / (float)PERF_LEN16;   // 0..1 across 8 bars
        if (p >= 1.0f) {                                     // THE DROP
            perfStart = -1; performing = false;
            k_filter = 0.5f; k_res = 0.0f;                   // snap wide open
            dropFlash = 14;
            hit(72, INSTR_NOISE, 3, 14); hit(34, SL_KICK, 7, 300);  // the bang
            hit(64, SL_CLAP, 5, 60);
        } else if (p < 0.12f) {                              // breakdown: ease shut, muffled
            k_filter = 0.5f - 0.38f * (p / 0.12f);           // → 0.12 (LP, fairly closed)
            k_res    = 0.35f;
        } else {                                             // build: sweep the LP back open, screaming
            float b = (p - 0.12f) / 0.88f;
            k_filter = 0.12f + (0.5f - 0.12f) * b;           // 0.12 → 0.50 (LP opens to full)
            k_res    = 0.35f + 0.58f * b;                    // 0.35 → 0.93 (resonant peak rises)
        }
    }

    // ── nudge the knobs by hand (held arrow keys = a smooth ride) ──
    if (!performing) {
        if (btn(0, BTN_LEFT))  k_filter = fmaxf(0.0f, k_filter - 0.012f);
        if (btn(0, BTN_RIGHT)) k_filter = fminf(1.0f, k_filter + 0.012f);
        if (btn(0, BTN_DOWN))  k_res    = fmaxf(0.0f, k_res - 0.015f);
        if (btn(0, BTN_UP))    k_res    = fminf(1.0f, k_res + 0.015f);
    }
    if (btnp(0, BTN_A)) bypass = !bypass;                    // Z
    if (btnp(0, BTN_B) && !performing) perfStart = cur16;    // X = BUILD

    // ── transport: a MONOTONIC sixteenth counter ──
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        if (last16 >= 0) cur16 += ((s16 - last16) % STEPS + STEPS) % STEPS;  // monotonic advance
        last16 = s16;
        playstep = cur16 % STEPS;

        int ci = (cur16 / 32) % 4;
        if (ci != chordIdx) {
            chordIdx = ci;
            for (int k = 0; k < 3; k++) if (padH[k] >= 0) note_pitch(padH[k], CHORD[ci][k]);
        }
        // a clap roll on the last bar of the build — the riser tension
        bool buildTail = performing && (cur16 - perfStart) > (int)(PERF_LEN16 * 0.78f);
        for (int r = 0; r < ROWS; r++) {
            if (!grid[r][playstep]) continue;
            if (performing && (r == 0 || r == 1)) continue;  // mute kick + clap until the drop
            play_row(r);
        }
        if (buildTail && (cur16 & 1)) hit(64, SL_CLAP, 3 + (cur16 - perfStart - 100) / 6, 30);
    }

    apply_filter();   // the live ride — gated on actual change (set-and-hold)
    if (dropFlash > 0) dropFlash--;

#ifdef DE_TRACE
    int mode; float cut, q; filter_target(k_filter, k_res, &mode, &cut, &q);
    watch("filter", "%.2f", k_filter);
    watch("res",    "%.2f", k_res);
    watch("mode",   "%d", bypass ? -1 : mode);
    watch("cut_hz", "%.0f", cut);
#endif
}

// ── the response curve: |H(f)| of a 2nd-order SVF, the centerpiece visual ──
#define CVX 6
#define CVY 16
#define CVW 308
#define CVH 86
static float resp_db(int mode, float fc, float Q, float f) {
    if (mode == FILTER_OFF) return 0.0f;
    float w = f / fc, w2 = w * w;
    float d = sqrtf((1.0f - w2) * (1.0f - w2) + (w / Q) * (w / Q)) + 1e-6f;
    float mag = (mode == FILTER_LOW)  ? 1.0f / d
              : (mode == FILTER_HIGH) ? w2 / d
              : (mode == FILTER_BAND) ? (w / Q) / d
                                      : fabsf(1.0f - w2) / d;   // notch
    return 20.0f * log10f(mag + 1e-6f);
}
static int db_to_y(float db) {                                 // +18dB top .. -48dB bottom
    float t = (18.0f - db) / 66.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return CVY + (int)(t * (CVH - 1));
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    if (dropFlash > 0) cls((dropFlash & 1) ? CLR_DARKER_PURPLE : CLR_BROWNISH_BLACK);

    print("DJ FILTER", 6, 4, CLR_LIGHT_PEACH);
    print_right(bypass ? "BYPASS" : (k_filter < 0.48f ? "LOW-PASS" : k_filter > 0.52f ? "HIGH-PASS" : "OPEN"),
                314, 4, bypass ? CLR_DARK_GREY : CLR_YELLOW);

    int mode; float cut, q;
    filter_target(k_filter, k_res, &mode, &cut, &q);
    float Q = 0.7f + k_res * 13.0f;

    // ── response-curve panel ──
    rectfill(CVX, CVY, CVW, CVH, CLR_BLACK);
    int y0 = db_to_y(0);
    line(CVX, y0, CVX + CVW - 1, y0, CLR_DARKER_GREY);          // 0 dB reference
    // decade gridlines + labels (100 / 1k / 10k Hz, log axis 20..20000)
    float lf0 = log10f(20.0f), lf1 = log10f(20000.0f);
    int decX[3]; const char *decL[3] = { "100", "1K", "10K" };
    float decF[3] = { 100.0f, 1000.0f, 10000.0f };
    for (int i = 0; i < 3; i++) {
        decX[i] = CVX + (int)((log10f(decF[i]) - lf0) / (lf1 - lf0) * (CVW - 1));
        line(decX[i], CVY, decX[i], CVY + CVH - 1, CLR_DARKER_BLUE);
    }

    // the magnitude curve — slope tilts with the knob, peak rises with RES
    int colCurve = bypass ? CLR_DARK_GREY : (mode == FILTER_HIGH ? CLR_BLUE : CLR_PINK);
    int prevY = -1;
    for (int px = 0; px < CVW; px++) {
        float lf = lf0 + (px / (float)(CVW - 1)) * (lf1 - lf0);
        float f  = powf(10.0f, lf);
        int yy = db_to_y(resp_db(bypass ? FILTER_OFF : mode, cut, Q, f));
        int x = CVX + px;
        if (prevY < 0) prevY = yy;
        line(x - 1, prevY, x, yy, colCurve);
        for (int fy = yy; fy < CVY + CVH; fy += 3) pset(x, fy, CLR_DARKER_GREY);  // faint fill = "what passes"
        prevY = yy;
    }
    // cutoff marker — slides with the knob
    if (mode != FILTER_OFF && !bypass) {
        int cx = CVX + (int)((log10f(cut) - lf0) / (lf1 - lf0) * (CVW - 1));
        if (cx < CVX) cx = CVX; if (cx > CVX + CVW - 1) cx = CVX + CVW - 1;
        line(cx, CVY, cx, CVY + CVH - 1, CLR_YELLOW);
        print(str("%dHZ", (int)cut), cx + 3 > CVX + CVW - 40 ? cx - 40 : cx + 3, CVY + 2, CLR_LIGHT_YELLOW);
    }
    rect(CVX, CVY, CVW, CVH, CLR_DARK_GREY);
    font(FONT_SMALL);
    for (int i = 0; i < 3; i++) print(decL[i], decX[i] - 4, CVY + CVH - 7, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── BRIGHTNESS bar — "see it get dark" as the filter closes ──
    float bright = bypass ? 1.0f : 1.0f - fabsf(k_filter - 0.5f) * 2.0f;   // 1 open → 0 at the extremes
    int bx = CVX, bw = CVW, by = CVY + CVH + 4;
    print("FULLNESS", bx, by - 1, CLR_DARK_GREY);
    int blx = bx + 60, blw = (CVX + CVW) - blx;
    rectfill(blx, by, blw, 6, CLR_DARKER_GREY);
    rectfill(blx, by, (int)(bright * blw), 6, bright > 0.6f ? CLR_GREEN : bright > 0.3f ? CLR_ORANGE : CLR_RED);
    rect(blx, by, blw, 6, CLR_DARK_GREY);

    // ── kit activity dots + beat ──
    for (int r = 0; r < ROWS; r++) {
        bool hot = (frame() - flash[r]) < 6;
        circfill(8 + r * 12, by + 16, 4, hot ? LIT[r] : CLR_DARKER_GREY);
    }
    for (int b = 0; b < 4; b++)
        circfill(250 + b * 16, by + 16, 3, (playstep / 4) == b ? CLR_WHITE : CLR_DARK_GREY);

    // ── the rack: the BIG bipolar FILTER knob (the star) + RES, BUILD, BYPASS ──
    ui_begin();
    int fkx = 150, fky = 168, rkx = 250;

    // decorative bipolar dial behind the FILTER knob: LP arc (red, left) / HP arc (blue, right)
    for (int a = -135; a <= 135; a += 5) {
        float rad = a * 0.0174533f;
        int col = a < -8 ? CLR_DARK_RED : a > 8 ? CLR_TRUE_BLUE : CLR_DARK_GREY;
        pset(fkx + (int)(sinf(rad) * 22), fky - (int)(cosf(rad) * 22), col);
    }
    print("LP", fkx - 30, fky + 10, CLR_DARK_RED);
    print("HP", fkx + 22, fky + 10, CLR_TRUE_BLUE);

    ui_knob(&k_filter, fkx, fky, "FILTER");
    ui_knob(&k_res,    rkx, fky, "RES");

    if (ui_button(8, 150, 64, 18, perfStart >= 0 ? "..BUILD.." : "BUILD") && perfStart < 0)
        perfStart = cur16;
    if (ui_button(8, 174, 64, 18, bypass ? "FILTER:OFF" : "FILTER:ON"))
        bypass = !bypass;

    // RES-scream warning lamp
    if (!bypass && mode != FILTER_OFF && k_res > 0.75f)
        print("SCREAM", rkx - 22, fky - 26, (frame() & 4) ? CLR_RED : CLR_ORANGE);
    if (perfStart >= 0)
        print_right("RISER", 314, 150, (frame() & 4) ? CLR_YELLOW : CLR_ORANGE);

    ui_end();
}
