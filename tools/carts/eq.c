// eq — a 3-band EQ you play by DRAGGING. A SAW bass + a bright SQUARE lead loop run through the
// new eq() bus; grab the LOW / MID / HIGH nodes in the grid and pull them up to BOOST or down to
// cut (±12 dB each — the library's only boost; the filters only cut). The response curve bends to
// follow your drag. Route over the WHOLE mix or just the BASS, or hit AMP to stack DRIVE_ASYM +
// a mid-forward EQ = a guitar-amp tone. Mouse or touch: drag the nodes, click the buttons.
//   drag a node : boost/cut that band   click MASTER/BASS : route   click AMP : guitar-amp preset
//   keys: LEFT/RIGHT pick a band, UP/DOWN nudge it, A route, B amp
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_BASS 0
#define I_LEAD 1

#define BANDS 3
enum { B_LOW, B_MID, B_HIGH };
static const char  *BAND_NAME[BANDS] = { "LOW", "MID", "HIGH" };
static const float  BAND_FREQ[BANDS] = { 45.0f, 700.0f, 9000.0f };   // node freq on the log axis
static float gain[BANDS] = { 6.0f, -2.0f, 5.0f };   // dB per band; opens on a visible "smile" curve

static int route = 0;     // 0 = master (whole mix), 1 = bass only, 2 = off
static int amp   = 0;     // guitar-amp preset (DRIVE_ASYM + mid-forward EQ)
static int sel   = 0;     // keyboard-selected band
static int drag  = -1;    // band being dragged by a pointer, or -1
static int dirty = 1;

static const char *ROUTE_NAME[3] = { "MASTER (whole mix)", "BASS ONLY (per-instr)", "OFF (flat)" };

// the grid panel: X = frequency (log, 20 Hz..20 kHz), Y = gain (+12 top .. -12 bottom)
#define GX 16
#define GY 36
#define GW (SCREEN_W - 32)
#define GH 96

static int freq_px(float f) { return GX + (int)(log10f(f / 20.0f) / 3.0f * GW); }
static int db_py(float db)  { return GY + (int)((1.0f - (db + 12.0f) / 24.0f) * GH); }
static float py_db(int py)  { float d = (1.0f - (py - GY) / (float)GH) * 24.0f - 12.0f;
                              return d < -12.0f ? -12.0f : (d > 12.0f ? 12.0f : d); }

static void apply(void) {
    float lo = gain[B_LOW], mi = gain[B_MID], hi = gain[B_HIGH];
    instrument_drive_mode(I_LEAD, DRIVE_ASYM);
    instrument_drive(I_LEAD, amp ? 0.55f : 0.0f);   // amp = asymmetric clip + a mid push on the lead
    if (route == 0)      { eq(lo, mi, hi);         instrument_eq(I_BASS, 0, 0, 0); }   // whole mix
    else if (route == 1) { eq(0, 0, 0);            instrument_eq(I_BASS, lo, mi, hi); } // just the bass
    else                 { eq(0, 0, 0);            instrument_eq(I_BASS, 0, 0, 0); }    // off (flat)
}

void init(void) {
    instrument(I_BASS, INSTR_SAW,    4, 200, 6, 240);
    instrument_filter(I_BASS, FILTER_LOW, 1100, 3);
    instrument(I_LEAD, INSTR_SQUARE, 2, 160, 4, 200);
    bpm(120);
    apply();
}

void update(void) {
    // ── pointer: grab the nearest node on press, drag it vertically to set its gain ──
    int mx = -1, my = -1, down = 0;
    if (mouse_down(MOUSE_LEFT)) { mx = mouse_x(); my = mouse_y(); down = 1; }
    if (touch_count() > 0)      { mx = touch_x(0); my = touch_y(0); down = 1; }  // touch wins
    if (down) {
        if (drag < 0 && mx >= GX - 8 && mx < GX + GW + 8 && my >= GY - 8 && my < GY + GH + 8) {
            int best = 0, bestd = 1 << 30;                // pick the node nearest in X
            for (int i = 0; i < BANDS; i++) {
                int dx = freq_px(BAND_FREQ[i]) - mx; if (dx < 0) dx = -dx;
                if (dx < bestd) { bestd = dx; best = i; }
            }
            drag = best; sel = best;
        }
        if (drag >= 0) { gain[drag] = py_db(my); dirty = 1; }
    } else drag = -1;

    // ── buttons (mouse/touch tap) ──
    if (tapp(16, 184, 150, 13))             { route = (route + 1) % 3; dirty = 1; }
    if (tapp(SCREEN_W - 58, 184, 42, 13))   { amp = !amp; dirty = 1; }

    // ── keyboard: pick a band, nudge it, A route, B amp ──
    if (btnp(0, BTN_LEFT))  { sel = (sel + BANDS - 1) % BANDS; }
    if (btnp(0, BTN_RIGHT)) { sel = (sel + 1) % BANDS; }
    if (btn(0, BTN_UP))     { gain[sel] += 0.12f; dirty = 1; }
    if (btn(0, BTN_DOWN))   { gain[sel] -= 0.12f; dirty = 1; }
    if (btnp(0, BTN_A))     { route = (route + 1) % 3; dirty = 1; }
    if (btnp(0, BTN_B))     { amp = !amp; dirty = 1; }
    for (int i = 0; i < BANDS; i++) { if (gain[i] < -12) gain[i] = -12; if (gain[i] > 12) gain[i] = 12; }

    if (dirty) { apply(); dirty = 0; }

    static const int bass[8] = { 36, 36, 43, 36, 41, 36, 48, 43 };
    static const int lead[8] = { 72, 79, 76, 84, 77, 81, 79, 76 };
    if (every(1)) { note(bass[beat() & 7], I_BASS, 6); note(lead[beat() & 7], I_LEAD, 3); }
}

// the engine's shape: 3 bands split at ~80 Hz and ~6 kHz, each scaled by its gain. Approximate the
// combined magnitude per frequency so the drawn curve matches what you hear.
static float resp_db(float f) {
    float lg = powf(10.0f, gain[B_LOW]  / 20.0f);
    float mg = powf(10.0f, gain[B_MID]  / 20.0f);
    float hg = powf(10.0f, gain[B_HIGH] / 20.0f);
    if (route == 2 && !amp) { lg = mg = hg = 1.0f; }
    float wl = 1.0f / (1.0f + (f / 80.0f) * (f / 80.0f));
    float r  = (f / 6000.0f) * (f / 6000.0f);
    float wh = r / (1.0f + r);
    float wm = 1.0f - wl - wh; if (wm < 0) wm = 0;
    float lin = wl * lg + wm * mg + wh * hg;
    if (lin < 0.0001f) lin = 0.0001f;
    return 20.0f * log10f(lin);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print_centered("EQ", SCREEN_W / 2, 6, CLR_WHITE);
    print_centered("3-band - drag a node to boost or cut", SCREEN_W / 2, 20, CLR_MEDIUM_GREY);

    // grid frame + 0 dB line + ±12 labels + the two crossover guides (80 Hz / 6 kHz)
    rect(GX - 1, GY - 1, GW + 2, GH + 2, CLR_DARKER_GREY);
    int midY = db_py(0.0f);
    line(GX, midY, GX + GW, midY, CLR_DARK_GREY);
    print("+12", GX + 1, GY + 1, CLR_DARKER_GREY);
    print("-12", GX + 1, GY + GH - 8, CLR_DARKER_GREY);
    line(freq_px(80.0f),   GY, freq_px(80.0f),   GY + GH, CLR_DARK_GREY);
    line(freq_px(6000.0f), GY, freq_px(6000.0f), GY + GH, CLR_DARK_GREY);

    // response curve
    int prev = midY;
    for (int px = 0; px < GW; px++) {
        float f = 20.0f * powf(10.0f, (px / (float)GW) * 3.0f);
        int y = db_py(resp_db(f));
        if (y < GY) y = GY; if (y > GY + GH) y = GY + GH;
        if (px > 0) line(GX + px - 1, prev, GX + px, y, CLR_LIME_GREEN);
        prev = y;
    }

    // the three draggable nodes
    for (int i = 0; i < BANDS; i++) {
        int nx = freq_px(BAND_FREQ[i]);
        int ny = db_py((route == 2 && !amp) ? 0.0f : gain[i]);
        int on = (drag == i) || (sel == i);
        int col = drag == i ? CLR_ORANGE : (sel == i ? CLR_YELLOW : CLR_WHITE);
        circfill(nx, ny, on ? 5 : 4, col);
        circ(nx, ny, on ? 5 : 4, CLR_BLACK);
        char b[16]; snprintf(b, sizeof b, "%s %+.0f", BAND_NAME[i], gain[i]);
        print_centered(b, nx, GY + GH + 4, on ? col : CLR_MEDIUM_GREY);
    }

    // route banner (click / A) + amp toggle (click / B)
    int rc = route == 0 ? CLR_ORANGE : (route == 1 ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rectfill(16, 184, 150, 13, rc);
    print(ROUTE_NAME[route], 20, 186, CLR_BLACK);
    int ac = amp ? CLR_RED : CLR_DARKER_GREY;
    rectfill(SCREEN_W - 58, 184, 42, 13, ac);
    print_centered("AMP", SCREEN_W - 37, 186, amp ? CLR_WHITE : CLR_MEDIUM_GREY);
}
