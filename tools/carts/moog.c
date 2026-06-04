#include "studio.h"

// A little Moog-style synth playground. Build a sound with the mouse — waveform,
// the famous ADSR envelope, three LFOs, and a resonant filter — then play it on
// the on-screen keyboard with the mouse or your computer keys (A row = white,
// W E T Y U O P = black). Z/X shift octave, C/V change velocity.
//
// Real hold-to-sustain: each key starts a HELD note (note_on) that rings as long as
// you hold it and releases on key-up (note_off) — the ADSR sustain stage finally does
// something. While a chord rings the FILTER (mode + cutoff + resonance), PW, and all
// three LFOs follow their sliders LIVE under your fingers — the classic Moog filter
// sweep and more, the thing a fire-and-forget note() can't do. Only WAVE and the ADSR
// shape wait for the next note (you can't re-attack a ringing voice).

#define SLOT 5
#define NONE -999
#define CLI(v,a,b) ((int)clamp((float)(v),(float)(a),(float)(b)))

// ---- synth state (what the UI edits) ----
int   wave    = 1;                 // INSTR_SAW
float attack  = 6,  decay = 140, sustain = 5, release = 320;   // ms, sustain 0..7
float duty    = 0.5f;
int   fmode   = 1;                 // FILTER_LOW
float cutoff  = 700, res = 7;      // lower base so the FILTER CONTOUR has room to open it (the Moog "wow")
typedef struct { int target; float rate; float depth; } Lfo;   // target 0=OFF,1=PIT,2=DUT,3=VOL,4=CUT
Lfo   lfos[3] = { {1, 5.0f, 0.25f}, {0, 3.0f, 0.3f}, {0, 0.4f, 0.6f} };

// mod-envelopes — a filter contour (ENV_CUTOFF) and a pitch envelope (ENV_PITCH), each a
// one-shot AD spike with a bipolar amount. The ENVELOPE panel toggles which one you edit.
int   env_view = 0;                // 0 = AMP ADSR, 1 = FILTER contour, 2 = PITCH env
float fenv_amt = 1500, fenv_atk = 4, fenv_dec = 220;   // filter contour: Hz (bipolar), ms, ms
float penv_amt = 0,    penv_atk = 0, penv_dec = 120;   // pitch env: semitones (bipolar), ms, ms

int   base = 48;                   // MIDI of the leftmost white key (C3)
int   vel  = 98;                   // 0..127, like a real synth

// ---- mouse / immediate-mode UI state ----
int mx, my, ui_held = 0, mouse_semi = NONE;

// ---- held-note handles, one per playable key (-1 = key is up) ----
int wh[11], bh[7], mouse_h = -1;

// ---- keyboard layout ----
const char wkey[]  = "ASDFGHJKL;'";
int   wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
char  bkey[7]   = { 'W','E','T','Y','U','O','P' };
int   bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
int   bafter[7] = { 0, 1, 3, 4, 5, 7, 8 };

int white_x(int i) { return 10 + i * 40; }
int black_x(int j) { return white_x(bafter[j]) + 28; }

int in_box(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

// UI depth (0..1) → engine units, per LFO destination
float lfo_scaled(int dest, float d) {
    return dest == LFO_PITCH  ? d * 5.0f
         : dest == LFO_DUTY   ? d * 0.45f
         : dest == LFO_VOLUME ? d
                              : d * 1800.0f;   // LFO_CUTOFF (Hz)
}

// push the current patch into the instrument bank (read at the next note-on:
// this is where the snapshot-only knobs — wave + ADSR — take hold)
void apply_synth(void) {
    instrument(SLOT, wave, (int)attack, (int)decay, CLI(sustain + 0.5f, 0, 7), (int)release);
    instrument_duty(SLOT, duty);
    for (int L = 0; L < 3; L++) {
        if (lfos[L].target == 0) { instrument_lfo(SLOT, L, LFO_PITCH, 0, 0); continue; }
        int dest = lfos[L].target - 1;                 // 1..4 -> LFO_PITCH..LFO_CUTOFF
        instrument_lfo(SLOT, L, dest, lfos[L].rate, lfo_scaled(dest, lfos[L].depth));
    }
    instrument_filter(SLOT, fmode, (int)cutoff, CLI(res + 0.5f, 0, 15));
    instrument_env(SLOT, 0, ENV_CUTOFF, (int)fenv_atk, (int)fenv_dec, fenv_amt);   // filter contour
    instrument_env(SLOT, 1, ENV_PITCH,  (int)penv_atk, (int)penv_dec, penv_amt);   // pitch envelope
}

// push every LIVE-controllable knob to one ringing note this frame: filter (mode +
// cutoff + resonance), pulse width, and all three LFOs. So dragging any of these while
// a chord rings sweeps it under your fingers — only wave + ADSR wait for the next note.
void drive_live(int h) {
    if (h < 0) return;
    note_filter(h, fmode);
    note_cutoff(h, (int)cutoff);
    note_res(h, CLI(res + 0.5f, 0, 15));
    note_duty(h, duty);
    for (int L = 0; L < 3; L++) {
        if (lfos[L].target == 0) { note_lfo(h, L, LFO_PITCH, 0, 0); continue; }
        int dest = lfos[L].target - 1;
        note_lfo(h, L, dest, lfos[L].rate, lfo_scaled(dest, lfos[L].depth));
    }
}

int  vel07(void) { return CLI(vel * 7 / 127, 1, 7); }

// key down → start a sustained note (apply the live patch first); key up → release it
void voice_on(int *h, int semi) { apply_synth(); *h = note_on(base + semi, SLOT, vel07()); }
void voice_off(int *h)          { if (*h >= 0) note_off(*h); *h = -1; }

int key_at_mouse(void) {
    if (my < 200) return NONE;
    for (int j = 0; j < 7;  j++) if (in_box(black_x(j), 200, 24, 62))  return bsemi[j];
    for (int i = 0; i < 11; i++) if (in_box(white_x(i), 200, 39, 100)) return wsemi[i];
    return NONE;
}

// ---- widgets ----
int ui_btn(int x, int y, int w, int h, const char *label, int on, int col) {
    int hot = in_box(x, y, w, h);
    rectfill(x, y, w, h, on ? col : CLR_BLACK);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    print(label, x + 3, y + (h - 5) / 2, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    return hot && mouse_pressed(MOUSE_LEFT);
}

float ui_slider(int id, int x, int y, int w, float val, float lo, float hi, int col) {
    if (mouse_pressed(MOUSE_LEFT) && in_box(x - 2, y - 3, w + 4, 11)) ui_held = id;
    if (ui_held == id) val = lo + clamp((float)(mx - x) / w, 0, 1) * (hi - lo);
    rectfill(x, y + 1, w, 2, CLR_DARK_GREY);
    int kx = x + (int)((val - lo) / (hi - lo) * w);
    rectfill(kx - 2, y - 2, 4, 8, col);
    return val;
}

// XY pad: drag to set two params at once — *vx on the horizontal (left→right = xlo→xhi),
// *vy on the vertical (bottom→top = ylo→yhi). One control for cutoff × resonance.
void ui_xy(int id, int x, int y, int w, int h, float *vx, float xlo, float xhi,
           float *vy, float ylo, float yhi, int col) {
    if (mouse_pressed(MOUSE_LEFT) && in_box(x, y, w, h)) ui_held = id;
    if (ui_held == id) {
        *vx = xlo + clamp((float)(mx - x) / w, 0, 1) * (xhi - xlo);
        *vy = yhi - clamp((float)(my - y) / h, 0, 1) * (yhi - ylo);   // top = more
    }
    rectfill(x, y, w, h, CLR_BLACK);
    for (int g = 1; g < 4; g++) line(x + g * w / 4, y, x + g * w / 4, y + h, CLR_DARKER_GREY);
    for (int g = 1; g < 3; g++) line(x, y + g * h / 3, x + w, y + g * h / 3, CLR_DARKER_GREY);
    rect(x, y, w, h, CLR_DARK_GREY);
    int kx = clamp(x + (int)((*vx - xlo) / (xhi - xlo) * w), x, x + w);
    int ky = clamp(y + (int)((yhi - *vy) / (yhi - ylo) * h), y, y + h);
    line(kx, y, kx, y + h, col);
    line(x, ky, x + w, ky, col);
    circfill(kx, ky, 3, CLR_WHITE);
}

void panel(int x, int y, int w, int h, const char *title, int col) {
    rect(x, y, w, h, CLR_DARK_GREY);
    print(title, x + 4, y + 3, col);
}

void init() {
    for (int i = 0; i < 11; i++) wh[i] = -1;
    for (int j = 0; j < 7;  j++) bh[j] = -1;
}

void update() {
    for (int i = 0; i < 11; i++) {                       // computer-key white notes
        if (keyp(wkey[i])) voice_on(&wh[i], wsemi[i]);
        if (keyr(wkey[i])) voice_off(&wh[i]);
    }
    for (int j = 0; j < 7; j++) {                        // black notes
        if (keyp(bkey[j])) voice_on(&bh[j], bsemi[j]);
        if (keyr(bkey[j])) voice_off(&bh[j]);
    }

    if (keyp('Z')) base = CLI(base - 12, 12, 96);
    if (keyp('X')) base = CLI(base + 12, 12, 96);
    if (keyp('C')) vel  = CLI(vel - 10, 1, 127);
    if (keyp('V')) vel  = CLI(vel + 10, 1, 127);

    if (mouse_pressed(MOUSE_LEFT)) {                     // mouse on the on-screen keys
        int s = key_at_mouse();
        if (s != NONE) { mouse_semi = s; apply_synth(); mouse_h = note_on(base + s, SLOT, vel07()); }
    }
    if (mouse_released(MOUSE_LEFT)) { voice_off(&mouse_h); mouse_semi = NONE; }
}

// the famous ADSR envelope editor, with three drag handles
void draw_adsr(int gx, int gy, int gw, int gh) {
    float aMax = gw * 0.28f, dMax = gw * 0.28f, hold = gw * 0.16f, rMax = gw * 0.28f;
    int ax  = gx + (int)(attack  / 1200.0f * aMax);
    int dsx = ax + (int)(decay   / 1200.0f * dMax);
    int sy  = gy + (int)((1 - sustain / 7.0f) * gh);
    int hx  = dsx + (int)hold;
    int rx  = hx + (int)(release / 1800.0f * rMax);
    int by  = gy + gh;

    // grab handles
    if (mouse_pressed(MOUSE_LEFT)) {
        if      (in_box(ax - 5, gy - 5, 10, 10))  ui_held = 11;
        else if (in_box(dsx - 5, sy - 5, 10, 10)) ui_held = 12;
        else if (in_box(rx - 5, by - 5, 10, 10))  ui_held = 13;
    }
    if (ui_held == 11) attack  = clamp((float)(mx - gx) / aMax * 1200, 0, 1200);
    if (ui_held == 12) { decay = clamp((float)(mx - ax) / dMax * 1200, 0, 1200);
                         sustain = clamp((1 - (float)(my - gy) / gh) * 7, 0, 7); }
    if (ui_held == 13) release = clamp((float)(mx - hx) / rMax * 1800, 0, 1800);

    // recompute after drag so the picture tracks the mouse the same frame
    ax  = gx + (int)(attack / 1200.0f * aMax);
    dsx = ax + (int)(decay / 1200.0f * dMax);
    sy  = gy + (int)((1 - sustain / 7.0f) * gh);
    hx  = dsx + (int)hold;
    rx  = hx + (int)(release / 1800.0f * rMax);

    rectfill(gx, gy, gw, gh, CLR_BLACK);
    line(gx, by, ax, gy, CLR_GREEN);     // attack
    line(ax, gy, dsx, sy, CLR_GREEN);    // decay
    line(dsx, sy, hx, sy, CLR_GREEN);    // sustain
    line(hx, sy, rx, by, CLR_GREEN);     // release
    rectfill(ax - 2, gy - 2, 5, 5, CLR_WHITE);
    rectfill(dsx - 2, sy - 2, 5, 5, CLR_WHITE);
    rectfill(rx - 2, by - 2, 5, 5, CLR_WHITE);
    print(str("A%d D%d S%d R%d", (int)attack, (int)decay, (int)(sustain + 0.5f), (int)release),
          gx, by + 4, CLR_MEDIUM_GREY);
}

// AD-with-amount editor for the mod-envelopes: a spike from a center zero-line up (or DOWN,
// for a negative amount) to `amount` over attack, back to zero over decay. Two drag dots:
// the peak (X = attack, Y = amount ± about the center) and the decay end (X = decay).
void draw_adenv(int gx, int gy, int gw, int gh, float *atk, float atkmax,
                float *dec, float decmax, float *amt, float amtmax, int col) {
    int zy = gy + gh / 2;
    float aMax = gw * 0.40f, dMax = gw * 0.45f, half = gh / 2.0f;
    int px = gx + (int)(*atk / atkmax * aMax);
    int py = (int)clamp(zy - *amt / amtmax * half, gy, gy + gh);
    int ex = px + (int)(*dec / decmax * dMax);

    if (mouse_pressed(MOUSE_LEFT)) {
        if      (in_box(px - 5, py - 5, 10, 10)) ui_held = 14;
        else if (in_box(ex - 5, zy - 5, 10, 10)) ui_held = 15;
    }
    if (ui_held == 14) { *atk = clamp((float)(mx - gx) / aMax * atkmax, 0, atkmax);
                         *amt = clamp((zy - my) / half * amtmax, -amtmax, amtmax); }
    if (ui_held == 15)   *dec = clamp((float)(mx - px) / dMax * decmax, 1, decmax);

    px = gx + (int)(*atk / atkmax * aMax);
    py = (int)clamp(zy - *amt / amtmax * half, gy, gy + gh);
    ex = px + (int)(*dec / decmax * dMax);

    rectfill(gx, gy, gw, gh, CLR_BLACK);
    for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, zy, CLR_DARKER_GREY);   // zero line
    line(gx, zy, px, py, col);
    line(px, py, ex, zy, col);
    line(ex, zy, gx + gw, zy, col);
    rectfill(px - 2, py - 2, 5, 5, CLR_WHITE);
    rectfill(ex - 2, zy - 2, 5, 5, CLR_WHITE);
    print(str("amt %+d  a%d d%d", (int)*amt, (int)*atk, (int)*dec), gx, gy + gh + 4, CLR_MEDIUM_GREY);
}

void draw() {
    mx = mouse_x(); my = mouse_y();
    if (!mouse_down(MOUSE_LEFT)) ui_held = 0;

    cls(CLR_DARKER_GREY);
    print("DREAM SYNTH", 8, 4, CLR_WHITE);

    // ---- TIMBRE ----
    const char *wn[5] = { "SQUARE", "SAW", "TRIANGLE", "NOISE", "SINE" };
    panel(6, 16, 92, 100, "WAVE", CLR_BLUE);
    for (int i = 0; i < 5; i++)
        if (ui_btn(12, 30 + i * 15, 80, 13, wn[i], wave == i, CLR_BLUE)) wave = i;
    print("PW", 12, 107, CLR_MEDIUM_GREY);
    duty = ui_slider(1, 34, 107, 58, duty, 0.05f, 0.95f, CLR_BLUE);

    // ---- ENVELOPE (toggle: AMP ADSR / FILTER contour / PITCH env — drag the dots) ----
    const char *evn[3]  = { "AMP", "FILTER", "PITCH" };
    int         evcol[3] = { CLR_GREEN, CLR_DARK_PEACH, CLR_MAUVE };
    panel(104, 16, 198, 100, "", evcol[env_view]);
    for (int i = 0; i < 3; i++)
        if (ui_btn(108 + i * 50, 19, 46, 12, evn[i], env_view == i, evcol[i])) env_view = i;
    if      (env_view == 0) draw_adsr(112, 42, 182, 56);
    else if (env_view == 1) draw_adenv(112, 42, 182, 56, &fenv_atk, 600, &fenv_dec, 800, &fenv_amt, 3000, evcol[1]);
    else                    draw_adenv(112, 42, 182, 56, &penv_atk, 400, &penv_dec, 600, &penv_amt, 36,   evcol[2]);

    // ---- FILTER ----
    const char *fn[5] = { "OFF", "LP", "HP", "BP", "NF" };
    panel(308, 16, 146, 100, "FILTER", CLR_ORANGE);
    for (int i = 0; i < 5; i++)
        if (ui_btn(312 + i * 28, 30, 26, 13, fn[i], fmode == i, CLR_ORANGE)) fmode = i;
    ui_xy(2, 314, 48, 134, 44, &cutoff, 80, 4000, &res, 0, 15, CLR_ORANGE);
    print("RES", 316, 50, CLR_DARK_GREY);                       // up  = more resonance
    print("CUT", 426, 84, CLR_DARK_GREY);                       // right = more cutoff
    print(str("cut %dhz  res %d", (int)cutoff, (int)(res + 0.5f)), 314, 96, CLR_MEDIUM_GREY);

    // ---- 3 LFOs ----
    const char *tn[5] = { "OFF", "PITCH", "DUTY", "VOL", "CUT" };
    int lcol[3] = { CLR_PINK, CLR_YELLOW, CLR_INDIGO };
    for (int L = 0; L < 3; L++) {
        int x = 6 + L * 151, y = 122, w = 146;
        panel(x, y, w, 62, str("LFO %d", L + 1), lcol[L]);
        if (ui_btn(x + 6, y + 14, 64, 12, tn[lfos[L].target], lfos[L].target != 0, lcol[L]))
            lfos[L].target = (lfos[L].target + 1) % 5;
        // live dot so you can see it tick
        if (lfos[L].target != 0) {
            int dotx = x + w - 16 + (int)(sin_deg(now() * lfos[L].rate * 360.0f) * 7);
            circfill(dotx, y + 20, 3, lcol[L]);
        }
        print("RATE", x + 6, y + 32, CLR_MEDIUM_GREY);
        lfos[L].rate = ui_slider(20 + L, x + 38, y + 32, w - 46, lfos[L].rate, 0.1f, 12, lcol[L]);
        print("DEPTH", x + 6, y + 50, CLR_MEDIUM_GREY);
        lfos[L].depth = ui_slider(30 + L, x + 44, y + 50, w - 52, lfos[L].depth, 0, 1, lcol[L]);
    }

    // ---- info line ----
    print(str("OCT C%d   [Z -] [X +]", base / 12 - 1), 8, 190, CLR_LIGHT_GREY);
    print(str("VELOCITY %d   [C -] [V +]", vel), 250, 190, CLR_LIGHT_GREY);

    // ---- keyboard ----
    for (int i = 0; i < 11; i++) {
        int x = white_x(i);
        int lit = key(wkey[i]) || (mouse_down(MOUSE_LEFT) && mouse_semi == wsemi[i]);
        rectfill(x, 200, 39, 100, lit ? CLR_YELLOW : CLR_WHITE);
        rect(x, 200, 39, 100, CLR_DARK_GREY);
        print(str("%c", wkey[i]), x + 16, 288, CLR_DARK_GREY);
    }
    for (int j = 0; j < 7; j++) {
        int x = black_x(j);
        int lit = key(bkey[j]) || (mouse_down(MOUSE_LEFT) && mouse_semi == bsemi[j]);
        rectfill(x, 200, 24, 62, lit ? CLR_ORANGE : CLR_BROWNISH_BLACK);
        print(str("%c", bkey[j]), x + 8, 248, CLR_WHITE);
    }

    // LIVE: every held note follows the filter (mode+cutoff+res), PW, and LFO knobs this
    // frame — tweak them while a chord rings and you hear it change under your fingers.
    for (int i = 0; i < 11; i++) drive_live(wh[i]);
    for (int j = 0; j < 7;  j++) drive_live(bh[j]);
    drive_live(mouse_h);
}
