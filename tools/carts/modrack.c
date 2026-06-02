#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// STEP 3: the patch is still hardcoded (no cables yet), but the KNOBS are now live —
// click-drag any knob up/down to change it, and the running chain responds. Hover a knob
// to read its value (proximity reveal). The generative Berlin-school patch:
//
//   CLOCK ──tick──► S&H ◄──samples── LFO
//                    └──value──► QUANT ──pitch──► VOICE
//   LFO ───────────────────────────────────────► VOICE filter cutoff (live, every frame)
//
// A slow LFO wanders; S&H freezes it each clock step; QUANT snaps it to a scale (always in
// key); VOICE plays it as a held note whose filter the LFO sweeps live. Next steps draw
// the cables and make them editable.

#define SLOT 5
#define ROOT_OCT 4

const char *NOTES[12]  = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
const char *SCALES[6]  = { "maj","min","pen","pnm","blu","chr" };

// patch parameters — knob-backed floats, draggable (step 3)
float p_bpm = 112, p_rate = 0.37f, p_scale = SCALE_PENTA, p_root = 0, p_cut = 700;

// runtime signal state
float lfo_phase = 0, lfo_out = 0, sh = 0, cutoff = 700;
int   cur_midi = 60, voice = -1, last_step = -1, tick_flash = 99;

// knob-drag UI state
int   held_knob = 0, drag_y = 0;

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);
    instrument_filter(SLOT, FILTER_LOW, 600, 10);
}

void update(void) {
    bpm((int)p_bpm);

    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);     // CLOCK — 8th-note steps
    bool tick = step8 != last_step;
    if (tick) { last_step = step8; tick_flash = 0; } else tick_flash++;

    lfo_phase += p_rate * dt();                            // LFO — slow wander 0..1
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    lfo_out = (sin_deg(lfo_phase * 360.0f) + 1.0f) * 0.5f;

    if (tick) {                                            // S&H → QUANT → VOICE retrigger
        sh = lfo_out;
        int n = (int)(sh * 7.999f);
        cur_midi = degree((int)p_scale, ROOT_OCT, n) + (int)p_root;
        if (voice >= 0) note_off(voice);
        voice = note_on(cur_midi, SLOT, 5);
    }

    cutoff = p_cut + lfo_out * 1800.0f;                    // VOICE filter CV — base knob + LFO
    if (voice >= 0) note_cutoff(voice, (int)cutoff);

#ifdef DE_TRACE
    watch("bpm", "%d", (int)p_bpm);
    watch("cut", "%d", (int)p_cut);
#endif
}

// ── drawing helpers ──
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE_GREEN; }

int near_col(int x, int y) {   // proximity reveal: label brightens as the cursor nears it
    float d = distance(mouse_x(), mouse_y(), x, y);
    return d < 14 ? CLR_WHITE : d < 30 ? CLR_LIGHT_GREY : d < 54 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

void jack_l(int cx, int cy, int type, bool out, bool lit, const char *name) {
    int c = sig_col(type);
    if (out) { circ(cx, cy, 3, c); if (lit) circfill(cx, cy, 1, CLR_WHITE); }
    else       circfill(cx, cy, 3, lit ? CLR_WHITE : c);
    print(name, cx - text_width(name) / 2, cy + 6, near_col(cx, cy));
}

void meter(int x, int y, int w, int h, float v, int col) {
    rectfill(x, y, w, h, CLR_BLACK);
    int fill = (int)(clamp(v, 0, 1) * h);
    rectfill(x, y + h - fill, w, fill, col);
}

// a draggable knob: click+drag up/down to change. returns the new value; hover reveals it.
float knob_dial(int id, int cx, int cy, float v, float lo, float hi, const char *name, const char *val) {
    if (mouse_pressed(MOUSE_LEFT) && distance(mouse_x(), mouse_y(), cx, cy) < 9) { held_knob = id; drag_y = mouse_y(); }
    if (held_knob == id && mouse_down(MOUSE_LEFT)) {
        v = clamp(v + (drag_y - mouse_y()) * (hi - lo) / 120.0f, lo, hi);   // drag up = increase
        drag_y = mouse_y();
    }
    bool hot = held_knob == id || distance(mouse_x(), mouse_y(), cx, cy) < 9;
    circfill(cx, cy, 6, CLR_DARKER_GREY);
    circ(cx, cy, 6, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
    float a = 135.0f + clamp((v - lo) / (hi - lo), 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 5), cy + (int)(sin_deg(a) * 5), CLR_WHITE);
    print(name, cx - text_width(name) / 2, cy + 9, near_col(cx, cy));
    if (hot) print(val, cx - text_width(val) / 2, cy + 17, CLR_WHITE);
    return v;
}

// a static (non-interactive) knob, for placeholder modules
void knob_static(int cx, int cy, float v) {
    circfill(cx, cy, 6, CLR_DARKER_GREY);
    circ(cx, cy, 6, CLR_MEDIUM_GREY);
    float a = 135.0f + clamp(v, 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 5), cy + (int)(sin_deg(a) * 5), CLR_DARK_GREY);
}

void draw(void) {
    if (!mouse_down(MOUSE_LEFT)) held_knob = 0;            // release any drag

    cls(CLR_DARKER_BLUE);
    print("MODRACK", 6, 4, CLR_WHITE);
    print("step3: live knobs", 84, 5, CLR_INDIGO);

    const char *nm[6] = { "CLOCK", "LFO", "S&H", "QUANT", "VOICE", "EUCLID" };
    int col[6]        = { CLR_ORANGE, CLR_PINK, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_DARK_GREY };
    int y = 18, h = 156;

    for (int i = 0; i < 6; i++) {
        int x = 6 + i * 52, w = 46, cx = x + w / 2;
        bool gate_lit = tick_flash < 5;

        rectfill(x, y, w, h, i == 5 ? CLR_BROWNISH_BLACK : CLR_DARKER_PURPLE);
        rect(x, y, w, h, col[i]);
        print(nm[i], x + 3, y + 3, col[i]);

        switch (i) {
            case 0:  // CLOCK
                circfill(cx, y + 16, 4, gate_lit ? CLR_WHITE : CLR_DARK_ORANGE);
                p_bpm = knob_dial(1, cx, y + 44, p_bpm, 60, 240, "bpm", str("%d", (int)p_bpm));
                jack_l(x + 11, y + 132, 0, true, gate_lit, "1");
                jack_l(cx,     y + 132, 0, true, false,    "2");
                jack_l(x + 35, y + 132, 0, true, false,    "4");
                break;
            case 1:  // LFO
                p_rate = knob_dial(2, cx, y + 36, p_rate, 0.1f, 8.0f, "rate", str("%.1f", p_rate));
                meter(x + 8, y + 60, 6, 48, lfo_out, CLR_PINK);
                jack_l(cx, y + 132, 2, true, false, "cv");
                break;
            case 2:  // S&H
                jack_l(x + 13, y + 30, 2, false, false,    "in");
                jack_l(x + 33, y + 30, 0, false, gate_lit, "clk");
                meter(x + 8, y + 56, 6, 52, sh, CLR_YELLOW);
                jack_l(cx, y + 132, 2, true, false, "cv");
                break;
            case 3:  // QUANT — scale + root knobs
                jack_l(cx, y + 24, 2, false, false, "in");
                p_scale = knob_dial(3, cx, y + 48, p_scale, 0, 5.99f, "scl", SCALES[(int)p_scale]);
                p_root  = knob_dial(4, cx, y + 74, p_root, 0, 11.99f, "root", NOTES[(int)p_root]);
                print(NOTES[cur_midi % 12], cx - text_width(NOTES[cur_midi % 12]) / 2, y + 96, CLR_WHITE);
                jack_l(cx, y + 132, 1, true, gate_lit, "pit");
                break;
            case 4:  // VOICE — cutoff base knob (LFO sweeps on top)
                jack_l(x + 9,  y + 24, 0, false, gate_lit, "g");
                jack_l(cx,     y + 24, 1, false, gate_lit, "p");
                jack_l(x + 37, y + 24, 2, false, false,    "f");
                p_cut = knob_dial(5, cx, y + 54, p_cut, 200, 2200, "cut", str("%d", (int)p_cut));
                meter(x + 8, y + 74, 6, 40, (cutoff - 300) / 2600.0f, CLR_BLUE);
                if (tick_flash < 8) circ(cx, y + 54, 10 - tick_flash, CLR_LIGHT_PEACH);
                break;
            case 5:  // EUCLID — placeholder until step 6
                knob_static(cx, y + 40, 0.4f);
                print("hits", cx - 8, y + 49, CLR_DARK_GREY);
                jack_l(cx, y + 132, 0, true, false, "g");
                print("step6", x + 6, y + h - 12, CLR_DARK_GREY);
                break;
        }
    }

    print("drag a knob up/down to change it - hover to read", 6, 180, CLR_DARKER_GREY);
    print(str("%d bpm  %s  root %s  cut %dhz", (int)p_bpm, SCALES[(int)p_scale], NOTES[(int)p_root], (int)cutoff), 6, 190, CLR_BLUE_GREEN);
}
