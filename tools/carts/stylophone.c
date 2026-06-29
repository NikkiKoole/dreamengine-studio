/* de:meta
{
  "title": "stylophone deluxe",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "subtractive-synth",
    "adsr-envelope",
    "wavetable-drawing"
  ],
  "lineage": "Modelled on the Dubreq Stylophone Deluxe; novel for being the first cabinet instrument to expose live pulse-width morphing (note_duty) and a hand-drawn wavetable editor feeding INSTR_USER0.",
  "description": "The pocket stylus-synth, built to reach two corners of the audio engine no other instrument cart touches. LIVE PULSE-WIDTH: the TONE fader morphs the buzzy square's duty by hand (note_duty), thin/nasal to hollow/fat. THE TRANSPORT: the famous REITER (reiteration) switch retriggers the held note in time with the tempo (bpm + beat + beat_pos) — the cabinet's first tempo-synced instrument; crank RATE up and it machine-guns, flip ARP and the retrigger walks a major arpeggio. Play it with the STYLUS (the mouse): touch the metal keyboard and slide for the classic glissando (note_pitch stepping pad-to-pad). VIBRATO and SUB (sub-octave) are switches, like the real deluxe panel. Watch the BEAT lamp pulse with the transport."
}
de:meta */
#include "studio.h"
#include <math.h>

// Stylophone Deluxe — the pocket stylus-synth, built to probe two corners of the
// audio engine the rest of the cabinet barely touches:
//
//   1. LIVE PULSE-WIDTH (note_duty). In PULSE mode the TONE fader morphs a buzzy
//      square, thin/nasal → hollow/fat. No other cart sweeps duty live by hand.
//   2. DRAWN SCW (wave_set). Flip to DRAWN and the same stylus panel grows a tiny
//      64-sample waveform strip with seed buttons. TONE becomes SMOOTH: it rounds
//      your hand-drawn cycle before sending it to INSTR_USER0, live while a note rings.
//
// REITER / RATE are unchanged in both modes — the transport is waveform-agnostic.

#define PULSE_SLOT 5
#define DRAWN_SLOT 6
#define USER_WAVE  0

#define WLEN   64
#define BASE   53            // F3 — lowest pad
#define NPADS  15            // F3 .. G4, chromatic
#define KB_X   8
#define KB_Y   116
#define KB_W   304
#define KB_H   46
#define PAD_W  (KB_W / NPADS)

// fader geometry (shared x range, stacked)
#define FX     46
#define FW     72

// mini wave editor
#define WX     8
#define WY     82
#define WW     (WLEN * 2)
#define WH     28
#define WMID   (WY + WH / 2)

// tiny preset buttons beside the wave strip
#define BX     146
#define BY     84
#define BW     30
#define BH     10

// --- panel state ---
static float tone_knob = 0.25f;  // 0..1. PULSE=width, DRAWN=smoothing amount.
static float tempo = 120;    // bpm
static float rate  = 4;      // reiteration subdivisions per beat
static int   reiter, arp, vibrato, sub, drawn;
static int   ui_active = -1; // which fader is being dragged (-1 = none)

// --- drawn-wave state ---
static float wave_raw[WLEN];
static float wave_play[WLEN];
static float prev_tone = -1;
static int   last_wave_col = -1;
static float last_wave_val = 0;

// --- voices / transport ---
static int   voice = -1, sub_voice = -1;
static int   active_pad = -1;
static int   last_slot = -99999, arp_idx, last_beat;
static int   tick_vis, pulse_vis;
static int   mx, my;

static int inbox(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }
static int current_slot(void) { return drawn ? DRAWN_SLOT : PULSE_SLOT; }
static float pulse_duty(void) { return 0.10f + tone_knob * 0.40f; }

// a horizontal fader: returns the (possibly dragged) value. Drag logic only.
static float fader(int id, int fy, float val, float lo, float hi) {
    if (mouse_pressed(MOUSE_LEFT) && inbox(FX - 3, fy - 4, FW + 6, 12)) ui_active = id;
    if (ui_active == id) val = lo + clamp((float)(mx - FX) / FW, 0, 1) * (hi - lo);
    return val;
}

static void normalize_wave(float *w) {
    float m = 0;
    for (int i = 0; i < WLEN; i++) {
        float a = fabsf(w[i]);
        if (a > m) m = a;
    }
    if (m > 0.001f) for (int i = 0; i < WLEN; i++) w[i] *= 0.95f / m;
}

static void push_drawn_wave(void) {
    float work[WLEN], tmp[WLEN];
    int passes = 1 + (int)(tone_knob * 5.0f);
    for (int i = 0; i < WLEN; i++) work[i] = wave_raw[i];
    for (int p = 0; p < passes; p++) {
        for (int i = 0; i < WLEN; i++)
            tmp[i] = (work[(i + WLEN - 1) % WLEN] + work[i] * 2.0f + work[(i + 1) % WLEN]) * 0.25f;
        for (int i = 0; i < WLEN; i++) work[i] = tmp[i];
    }
    for (int i = 0; i < WLEN; i++) wave_play[i] = lerp(wave_raw[i], work[i], tone_knob);
    normalize_wave(wave_play);
    wave_set(USER_WAVE, wave_play, WLEN);
}

static void seed_wave(int kind) {
    for (int i = 0; i < WLEN; i++) {
        float ph = i / (float)WLEN;
        switch (kind) {
            case 0: wave_raw[i] = ph < 0.5f ? 0.85f : -0.85f; break;                          // square
            case 1: wave_raw[i] = sinf(ph * 6.2831853f) * 0.90f; break;                       // sine
            case 2: wave_raw[i] = (ph * 2.0f - 1.0f) * 0.90f; break;                          // saw
            default: wave_raw[i] = 0.55f * sinf(ph * 6.2831853f) + 0.28f * sinf(ph * 12.566f)
                                 + 0.18f * sinf(ph * 18.85f) + 0.12f * sinf(ph * 25.13f); break;  // organ
        }
    }
    if (kind == 3) normalize_wave(wave_raw);
    push_drawn_wave();
}

static void revoice(void) {
    if (voice >= 0) note_off(voice);
    if (sub_voice >= 0) note_off(sub_voice);
    voice     = note_on(60, current_slot(), 0);
    sub_voice = note_on(48, current_slot(), 0);
}

static void switch_btn(int x, int y, const char *label, int on, int col) {
    rectfill(x, y, 72, 16, on ? col : CLR_DARKER_BLUE);
    rect(x, y, 72, 16, inbox(x, y, 72, 16) ? CLR_WHITE : CLR_DARK_BLUE);
    print(label, x + 6, y + 5, on ? CLR_BLACK : CLR_MEDIUM_GREY);
}

static void mode_btn(int x, int y, const char *label, int on) {
    rectfill(x, y, 72, 12, on ? CLR_LIGHT_PEACH : CLR_DARKER_BLUE);
    rect(x, y, 72, 12, inbox(x, y, 72, 12) ? CLR_WHITE : CLR_DARK_BLUE);
    print(label, x + 10, y + 3, on ? CLR_BLACK : CLR_MEDIUM_GREY);
}

static void draw_fader(int fy, const char *label, float val, float lo, float hi, const char *readout) {
    print(label, 8, fy + 1, CLR_LIGHT_PEACH);
    rectfill(FX, fy + 2, FW, 2, CLR_DARKER_GREY);
    int kx = FX + (int)((val - lo) / (hi - lo) * FW);
    rectfill(kx - 2, fy - 2, 4, 9, ui_active >= 0 ? CLR_WHITE : CLR_LIGHT_GREY);
    print(readout, FX + FW + 6, fy + 1, CLR_MEDIUM_GREY);
}

void init(void) {
    instrument(PULSE_SLOT, INSTR_SQUARE, 6, 130, 6, 110);
    instrument_duty(PULSE_SLOT, pulse_duty());
    instrument_filter(PULSE_SLOT, FILTER_LOW, 2600, 3);

    instrument(DRAWN_SLOT, INSTR_USER0, 6, 130, 6, 110);
    instrument_filter(DRAWN_SLOT, FILTER_LOW, 2600, 3);

    seed_wave(0);  // square first — useful A/B against the pulse path
    revoice();
}

void update(void) {
    mx = mouse_x(); my = mouse_y();
    int down = mouse_down(MOUSE_LEFT), pressed = mouse_pressed(MOUSE_LEFT);
    if (mouse_released(MOUSE_LEFT)) { ui_active = -1; last_wave_col = -1; }

    // ---- mode / switches ----
    if (pressed) {
        if (inbox(158, 68, 72, 12) && !drawn) { drawn = 1; revoice(); }
        else if (inbox(236, 68, 72, 12) && drawn) { drawn = 0; revoice(); }
        else if      (inbox(158, 28, 72, 16)) reiter  = !reiter;
        else if (inbox(236, 28, 72, 16)) arp     = !arp;
        else if (inbox(158, 48, 72, 16)) vibrato = !vibrato;
        else if (inbox(236, 48, 72, 16)) sub     = !sub;
    }

    // ---- faders ----
    tone_knob  = fader(0, 30, tone_knob,  0, 1);
    tempo = fader(1, 46, tempo, 70, 180);
    rate  = fader(2, 62, rate,  1, 8);

    // ---- tiny preset buttons for the drawn wave ----
    if (drawn && pressed) {
        if      (inbox(BX +  0, BY +  0, BW, BH)) seed_wave(0);  // SQR
        else if (inbox(BX + 34, BY +  0, BW, BH)) seed_wave(1);  // SIN
        else if (inbox(BX + 68, BY +  0, BW, BH)) seed_wave(2);  // SAW
        else if (inbox(BX +  0, BY + 14, BW, BH)) seed_wave(3);  // ORG
    }

    // ---- paint the 64-sample strip in DRAWN mode ----
    int painting_wave = 0;
    if (drawn && down && ui_active < 0 && inbox(WX, WY, WW, WH)) {
        int col = (int)clamp((mx - WX) / 2.0f, 0, WLEN - 1);
        float val = clamp((WMID - my) / (WH / 2.0f), -1, 1);
        if (last_wave_col >= 0 && col != last_wave_col) {
            int a = last_wave_col, b = col, steps = b > a ? b - a : a - b;
            for (int s = 0; s <= steps; s++) {
                int c = a + (b > a ? s : -s);
                wave_raw[c] = last_wave_val + (val - last_wave_val) * (s / (float)steps);
            }
        } else wave_raw[col] = val;
        last_wave_col = col;
        last_wave_val = val;
        painting_wave = 1;
    } else if (!down) last_wave_col = -1;
    if (painting_wave || fabsf(tone_knob - prev_tone) > 0.001f) {
        push_drawn_wave();
        prev_tone = tone_knob;
    }

    // ---- stylus on the keyboard (only if not dragging the wave/faders) ----
    active_pad = -1;
    if (down && ui_active < 0 && !painting_wave && inbox(KB_X, KB_Y, NPADS * PAD_W, KB_H)) {
        active_pad = (mx - KB_X) / PAD_W;
        if (active_pad >= NPADS) active_pad = NPADS - 1;
    }
    int playing = active_pad >= 0;
    int m = BASE + (playing ? active_pad : 0);

    // ---- audio ----
    bpm((int)tempo);
    float duty = pulse_duty();
    float vdep = vibrato ? 0.6f : 0;
    if (!drawn) instrument_duty(PULSE_SLOT, duty);    // reiter hits snapshot the slot
    instrument_lfo(current_slot(), 0, LFO_PITCH, 6.0f, vdep);

    if (reiter) {
        note_vol(voice, 0);
        note_vol(sub_voice, 0);
        int subdiv = (int)(rate + 0.5f); if (subdiv < 1) subdiv = 1;
        int slot = (int)((beat() + beat_pos()) * subdiv);
        if (playing && slot != last_slot) {
            int step = 0;
            if (arp) { int pat[4] = { 0, 4, 7, 12 }; step = pat[arp_idx & 3]; arp_idx++; }
            hit(m + step, current_slot(), 7, 90);
            if (sub) hit(m + step - 12, current_slot(), 5, 90);
            tick_vis = 4;
        }
        last_slot = slot;
        if (!playing) arp_idx = 0;
    } else {
        arp_idx = 0;
        note_pitch(voice, m);
        note_lfo(voice, 0, LFO_PITCH, 6.0f, vdep);
        if (!drawn) note_duty(voice, duty);
        note_vol(voice, playing ? 7 : 0);

        note_pitch(sub_voice, m - 12);
        if (!drawn) note_duty(sub_voice, duty);
        note_vol(sub_voice, (playing && sub) ? 6 : 0);
    }

    if (beat() != last_beat) { last_beat = beat(); pulse_vis = 6; }
    if (pulse_vis > 0) pulse_vis--;
    if (tick_vis  > 0) tick_vis--;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(4, 20, 312, 156, CLR_DARKER_BLUE);
    rect(4, 20, 312, 156, CLR_TRUE_BLUE);

    // ---- panel: faders + switches ----
    font(FONT_SMALL);
    char buf[16];
    int pct = (int)(tone_knob * 100);
    if (drawn) {
        buf[0]='S'; buf[1]='M'; buf[2]=' '; buf[3]='0'+pct/10; buf[4]='0'+pct%10; buf[5]='%'; buf[6]=0;
    } else {
        int dp = (int)(pulse_duty() * 100);
        buf[0]='P'; buf[1]='W'; buf[2]='M'; buf[3]=' '; buf[4]='0'+dp/10; buf[5]='0'+dp%10; buf[6]='%'; buf[7]=0;
    }
    draw_fader(30, "TONE",  tone_knob,  0, 1, buf);
    int bp = (int)tempo; char tb[5] = { (char)('0'+bp/100), (char)('0'+(bp/10)%10), (char)('0'+bp%10), 0, 0 };
    draw_fader(46, "TEMPO", tempo, 70, 180, tb);
    char rb[4] = { 'x', (char)('0' + (int)(rate + 0.5f)), 0, 0 };
    draw_fader(62, "RATE",  rate,  1, 8, rb);

    switch_btn(158, 28, "REITER",  reiter,  CLR_ORANGE);
    switch_btn(236, 28, "ARP",     arp,     CLR_YELLOW);
    switch_btn(158, 48, "VIBRATO", vibrato, CLR_GREEN);
    switch_btn(236, 48, "SUB",     sub,     CLR_PINK);
    mode_btn(158, 68, "PULSE", !drawn);
    mode_btn(236, 68, "DRAWN", drawn);

    // transport lamp — blinks on each beat, brighter on a reiteration tick
    int lampcol = tick_vis > 0 ? CLR_WHITE : pulse_vis > 0 ? CLR_ORANGE : CLR_DARKER_GREY;
    circfill(300, 92, 4, lampcol);
    print("BEAT", 266, 90, CLR_INDIGO);

    // ---- tiny wave strip + presets ----
    rectfill(WX, WY, WW, WH, drawn ? CLR_BLACK : CLR_DARKER_GREY);
    rect(WX, WY, WW, WH, drawn ? CLR_DARK_GREY : CLR_DARK_BLUE);
    line(WX, WMID, WX + WW, WMID, CLR_DARKER_BLUE);
    for (int i = 0; i < WLEN; i++) {
        int x = WX + i * 2;
        int y0 = WMID - (int)(wave_raw[i] * (WH / 2 - 2));
        int y1 = WMID - (int)(wave_play[i] * (WH / 2 - 2));
        int fill = drawn ? CLR_TRUE_BLUE : CLR_DARK_BLUE;
        line(x, WMID, x, y0, fill);
        if (i < WLEN - 1) {
            int nx = WX + (i + 1) * 2;
            int ny = WMID - (int)(wave_play[i + 1] * (WH / 2 - 2));
            line(x, y1, nx, ny, CLR_WHITE);
        }
    }
    print("WAVE", WX + 2, WY - 9, drawn ? CLR_LIGHT_PEACH : CLR_INDIGO);
    if (!drawn) print("flip to DRAWN to sketch a 64-sample cycle", 144, 104, CLR_INDIGO);

    const char *btxt[4] = { "SQR", "SIN", "SAW", "ORG" };
    for (int i = 0; i < 4; i++) {
        int bx = BX + (i % 3) * 34;
        int by = BY + (i / 3) * 14;
        int hot = drawn && inbox(bx, by, BW, BH);
        rectfill(bx, by, BW, BH, drawn ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
        rect(bx, by, BW, BH, hot ? CLR_WHITE : CLR_DARK_BLUE);
        print(btxt[i], bx + 5, by + 2, drawn ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
    }

    font(FONT_NORMAL);

    // ---- keyboard ----
    rect(KB_X - 2, KB_Y - 2, NPADS * PAD_W + 4, KB_H + 4, CLR_LIGHT_GREY);
    for (int i = 0; i < NPADS; i++) {
        int pc = (BASE + i) % 12;
        int sharp = (pc==1||pc==3||pc==6||pc==8||pc==10);
        int px = KB_X + i * PAD_W;
        int on = (i == active_pad);
        int col = on ? (tick_vis > 0 || !reiter ? CLR_YELLOW : CLR_ORANGE)
                     : sharp ? CLR_DARKER_GREY : CLR_MEDIUM_GREY;
        rectfill(px + 1, KB_Y, PAD_W - 2, KB_H, col);
        line(px, KB_Y, px, KB_Y + KB_H, CLR_DARK_BLUE);
    }

    // ---- the stylus (drawn over the keyboard while the pointer is on it) ----
    if (my >= KB_Y - 10 && my <= KB_Y + KB_H + 4 && mx >= KB_X && mx < KB_X + NPADS * PAD_W) {
        int tipy = my < KB_Y ? KB_Y : my;
        line(mx + 12, tipy - 20, mx, tipy, CLR_LIGHT_GREY);
        line(mx + 13, tipy - 20, mx + 1, tipy, CLR_DARK_GREY);
        circfill(mx, tipy, 2, active_pad >= 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    // ---- HUD ----
    print("STYLOPHONE DELUXE", 6, 6, CLR_LIGHT_PEACH);
    print("touch + slide the stylus to play", 6, 178, CLR_INDIGO);
    print(drawn ? "TONE=smoothing   DRAWN=live SCW   REITER still clocks the note"
                : "TONE=pulse width   PULSE=classic buzz   REITER+RATE=rhythm",
          6, 188, CLR_DARK_BLUE);
}
