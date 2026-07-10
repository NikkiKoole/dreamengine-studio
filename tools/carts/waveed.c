/* de:meta
{
  "slug": "waveed",
  "title": "wave editor",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "additive-synth",
    "wavetable-drawing"
  ],
  "lineage": "A live waveform painter built around the engine's INSTR_USER0-3 slots; mellotron's tapeloop trick (wave_set for timbre construction) is its direct downstream.",
  "description": "Draw the actual waveform. The engine has four custom wave slots (INSTR_USER0-3), each a 64-sample single cycle — paint that cycle with the mouse and hear it instantly. The killer trick: SPACE starts a drone, then redraw the shape while it rings — wave_set() is live, so the timbre morphs under your pen. Seed shapes (sine, square, saw, organ, vocal, wobble) plus SMOOTH and NORM, A-K play notes, P toggles an arp. Press E to export wave_set() code; your drawn wave then works everywhere a wave id does: note(), instruments, the other sound carts."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// WAVE EDITOR — draw the actual waveform. The engine has four custom wave slots
// (INSTR_USER0..3), each one 64-sample single cycle; this cart lets you DRAW that cycle
// with the mouse and hear it instantly. Best trick: SPACE starts a drone, then redraw the
// shape while it rings — wave_set() is live, so the timbre morphs under your pen.
// Seed shapes + SMOOTH/NORM on the right, A-K play notes, E exports wave_set() code.
//
// (Instrument slots 5-8 here hold waves INSTR_USER0-3 — same numbers by coincidence,
// different namespaces: slot = an instrument preset, INSTR_USER* = the waveform id.)

#define WLEN 64
static float w[4][WLEN];
static int   cur = 0;

static bool  drone_on = false;
static int   drone_h = -1;
static bool  arp_on = false;
static float arp_t = 0; static int arp_i = 0;

static int   last_col = -1; static float last_val = 0;
static bool  dirty = false;
static const char *msg = ""; static int msg_t = 0;

// canvas geometry: 64 columns x 3px
#define CX 8
#define CW (WLEN * 3)
#define CY 28
#define CH 128
#define CMID (CY + CH / 2)

static const int TCOL[4] = { CLR_BLUE, CLR_ORANGE, CLR_LIME_GREEN, CLR_PINK };

static int in_box(int x, int y, int bw, int bh) { return mouse_x() >= x && mouse_x() < x + bw && mouse_y() >= y && mouse_y() < y + bh; }

static void push_wave(void) { wave_set(cur, w[cur], WLEN); }

// ---- seed shapes + tools (apply to the current wave) ----
static void normalize(float *t) {
    float m = 0;
    for (int i = 0; i < WLEN; i++) if (fabsf(t[i]) > m) m = fabsf(t[i]);
    if (m > 0.001f) for (int i = 0; i < WLEN; i++) t[i] *= 0.95f / m;
}
static void seed(int kind) {
    float *t = w[cur];
    for (int i = 0; i < WLEN; i++) {
        float ph = i / (float)WLEN;
        switch (kind) {
            case 0: t[i] = sinf(ph * 6.2831853f); break;                                       // sine
            case 1: t[i] = clamp(sinf(ph * 6.2831853f) * 4.0f, -0.85f, 0.85f); break;          // rounded square
            case 2: t[i] = (ph * 2.0f - 1.0f) * 0.85f; break;                                  // saw
            case 3: t[i] = 0.55f * sinf(ph * 6.2831853f) + 0.28f * sinf(ph * 12.566f)          // organ (drawbar sum)
                         + 0.18f * sinf(ph * 18.85f) + 0.12f * sinf(ph * 25.13f); break;
            case 4: t[i] = 0.45f * sinf(ph * 6.2831853f) + 0.30f * sinf(ph * 18.85f)           // vocal-ish (odd partials)
                         + 0.18f * sinf(ph * 31.42f) + 0.07f * sinf(ph * 43.98f); break;
            case 5: t[i] = (i == 0) ? 0 : clamp(t[i - 1] + rnd_float_between(-0.45f, 0.45f), -1, 1); break;  // wobble (random walk)
            case 6: t[i] = (ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f) * 0.85f; break;                 // triangle
        }
    }
    if (kind == 3 || kind == 4 || kind == 5) normalize(t);
    if (kind == 5) { for (int p = 0; p < 2; p++) for (int i = 0; i < WLEN; i++)               // settle the walk
        t[i] = (t[(i + WLEN - 1) % WLEN] + t[i] * 2 + t[(i + 1) % WLEN]) * 0.25f; normalize(t); }
    push_wave();
}
static void smooth_cur(void) {
    float *t = w[cur], o[WLEN];
    for (int i = 0; i < WLEN; i++) o[i] = (t[(i + WLEN - 1) % WLEN] + t[i] * 2 + t[(i + 1) % WLEN]) * 0.25f;
    for (int i = 0; i < WLEN; i++) t[i] = o[i];
    push_wave();
}

// ---- persistence + export ----
typedef struct { int version, cur; float w[4][WLEN]; } SaveData;
static void save_it(void) {
    SaveData d = { 1, cur, {{0}} };
    for (int k = 0; k < 4; k++) for (int i = 0; i < WLEN; i++) d.w[k][i] = w[k][i];
    save_bytes(&d, sizeof d); msg = "SAVED"; msg_t = 80;
}
static void load_it(void) {
    SaveData d;
    if (load_bytes(&d, sizeof d) == (int)sizeof d && d.version == 1) {
        cur = d.cur;
        for (int k = 0; k < 4; k++) for (int i = 0; i < WLEN; i++) w[k][i] = d.w[k][i];
        for (int k = 0; k < 4; k++) wave_set(k, w[k], WLEN);
        msg = "LOADED";
    } else msg = "no save";
    msg_t = 80;
}
static void export_code(void) {
    char b[640]; int n;
    printh("// ---- custom waveform (drawn in the wave editor cart) ----");
    n = snprintf(b, sizeof b, "static const float WAVE_U%d[64] = {", cur + 1);
    for (int i = 0; i < WLEN; i++) n += snprintf(b + n, sizeof b - n, "%.2ff%s", w[cur][i], i < WLEN - 1 ? "," : "");
    snprintf(b + n, sizeof b - n, "};");
    printh("%s", b);
    printh("// in init():  wave_set(%d, WAVE_U%d, 64);", cur, cur + 1);
    printh("// then play it like any wave:  note(60, INSTR_USER%d, 5);", cur);
    msg = "EXPORTED to the log panel"; msg_t = 140;
}

static void drone_toggle(void) {
    if (drone_on) { if (drone_h >= 0) note_off(drone_h); drone_h = -1; drone_on = false; }
    else { drone_h = note_on(57, 5 + cur, 4); drone_on = true; }
}

void init(void) {
    // slots 5-8 wrap the four user waves in a sustained, filter-free instrument: what you
    // draw is exactly what you hear
    for (int i = 0; i < 4; i++) instrument(5 + i, INSTR_USER0 + i, 5, 0, 7, 150);
    int keep = cur;
    cur = 0; seed(0);   // U1 sine
    cur = 1; seed(1);   // U2 rounded square
    cur = 2; seed(3);   // U3 organ
    cur = 3; seed(4);   // U4 vocal
    cur = keep;
}

void update(void) {
    if (keyp(KEY_SPACE)) drone_toggle();
    if (keyp('M')) arp_on = !arp_on;   // M, not P — 'P' belongs to the runtime pause overlay
    if (keyp('E')) export_code();
    if (keyp('1')) save_it();          // S is a piano key here, so save/load live on 1/2
    if (keyp('2')) load_it();
    if (msg_t > 0) msg_t--;

    // piano keys (pentatonic) on the home row
    const char KK[8] = { 'A','S','D','F','G','H','J','K' };
    for (int i = 0; i < 8; i++)
        if (keyp(KK[i])) hit(degree(SCALE_PENTA, 4, i), 5 + cur, 5, 350);

    if (arp_on) {
        arp_t += dt();
        if (arp_t >= 0.22f) { arp_t = 0; hit(degree(SCALE_PENTA, 4, arp_i % 8), 5 + cur, 4, 240); arp_i++; }
    }

    // ---- painting (interpolate between drag points so fast strokes don't gap) ----
    if (mouse_down(MOUSE_LEFT) && in_box(CX, CY, CW, CH)) {
        int col = (int)clamp((mouse_x() - CX) / 3.0f, 0, WLEN - 1);
        float val = clamp((CMID - mouse_y()) / (CH / 2.0f), -1, 1);
        if (last_col >= 0 && col != last_col) {
            int a = last_col, bcol = col, steps = bcol > a ? bcol - a : a - bcol;
            for (int s = 0; s <= steps; s++) {
                int c = a + (bcol > a ? s : -s);
                w[cur][c] = last_val + (val - last_val) * (s / (float)steps);
            }
        } else w[cur][col] = val;
        last_col = col; last_val = val;
        dirty = true;
    } else last_col = -1;
    if (dirty) { push_wave(); dirty = false; }   // live: a ringing drone morphs as you draw
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("WAVE EDITOR", 8, 4, CLR_WHITE);
    font(FONT_SMALL);
    if (msg_t > 0) print(msg, 110, 6, CLR_LIGHT_PEACH);
    else           print_right("SPACE drone + draw = live morph", 312, 6, CLR_MAUVE);
    font(FONT_NORMAL);

    // wave tabs + drone/arp
    for (int t = 0; t < 4; t++) {
        int x = 8 + t * 28;
        rectfill(x, 14, 24, 11, cur == t ? TCOL[t] : CLR_BLACK);
        rect(x, 14, 24, 11, in_box(x, 14, 24, 11) ? CLR_WHITE : CLR_DARK_GREY);
        print(str("U%d", t + 1), x + 5, 17, cur == t ? CLR_BLACK : CLR_MEDIUM_GREY);
        if (in_box(x, 14, 24, 11) && mouse_pressed(MOUSE_LEFT) && cur != t) {
            cur = t;
            if (drone_on) { note_off(drone_h); drone_h = note_on(57, 5 + cur, 4); }   // drone follows the tab
        }
    }
    bool dh = in_box(124, 14, 50, 11);
    rectfill(124, 14, 50, 11, drone_on ? CLR_YELLOW : CLR_BLACK);
    rect(124, 14, 50, 11, dh ? CLR_WHITE : CLR_DARK_GREY);
    print("DRONE", 130, 17, drone_on ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (dh && mouse_pressed(MOUSE_LEFT)) drone_toggle();
    bool ah = in_box(178, 14, 40, 11);
    rectfill(178, 14, 40, 11, arp_on ? CLR_MEDIUM_GREEN : CLR_BLACK);
    rect(178, 14, 40, 11, ah ? CLR_WHITE : CLR_DARK_GREY);
    print("ARP", 188, 17, arp_on ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (ah && mouse_pressed(MOUSE_LEFT)) arp_on = !arp_on;

    // ---- the cycle canvas ----
    rectfill(CX, CY, CW, CH, CLR_BLACK);
    for (int x = CX; x < CX + CW; x += 4) pset(x, CMID, CLR_DARKER_GREY);          // zero line
    for (int q = 1; q < 4; q++) line(CX + q * CW / 4, CY, CX + q * CW / 4, CY + CH, CLR_DARKER_BLUE);
    for (int i = 0; i < WLEN; i++) {                                               // filled columns
        int x = CX + i * 3, y = CMID - (int)(w[cur][i] * (CH / 2 - 2));
        if (y < CMID) rectfill(x, y, 2, CMID - y, TCOL[cur]);
        else          rectfill(x, CMID, 2, y - CMID + 1, TCOL[cur]);
    }
    for (int i = 0; i < WLEN - 1; i++) {                                           // bright outline
        int x0 = CX + i * 3 + 1, x1 = CX + (i + 1) * 3 + 1;
        line(x0, CMID - (int)(w[cur][i] * (CH / 2 - 2)), x1, CMID - (int)(w[cur][i + 1] * (CH / 2 - 2)), CLR_WHITE);
    }
    rect(CX, CY, CW, CH, CLR_DARK_GREY);

    // ---- seed + tool buttons ----
    const char *bn[9]   = { "SINE", "TRIANGLE", "SQUARE", "SAW", "ORGAN", "VOCAL", "WOBBLE", "SMOOTH", "NORM" };
    const int   bseed[7] = { 0, 6, 1, 2, 3, 4, 5 };   // button index -> seed() kind
    font(FONT_SMALL);
    for (int b = 0; b < 9; b++) {
        int y = CY + b * 14;
        bool hot = in_box(208, y, 56, 12);
        rectfill(208, y, 56, 12, CLR_DARKER_BLUE);
        rect(208, y, 56, 12, hot ? CLR_WHITE : CLR_DARK_GREY);
        print(bn[b], 216, y + 3, CLR_LIGHT_GREY);
        if (hot && mouse_pressed(MOUSE_LEFT)) {
            if (b < 7) seed(bseed[b]);
            else if (b == 7) smooth_cur();
            else { normalize(w[cur]); push_wave(); }
            msg = bn[b]; msg_t = 40;
        }
    }
    print("A-K play   SPACE drone   M arp   E export code   1/2 save/load", 8, 162, CLR_MEDIUM_GREY);
    print("draw with the mouse; switch U1-U4 and layer them in your game as INSTR_USER0-3", 8, 172, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print(str("editing U%d -> INSTR_USER%d", cur + 1, cur), 8, 184, TCOL[cur]);
}
