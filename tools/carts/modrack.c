#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// STEP 4: the patch is still hardcoded, but now the cables are DRAWN — bezier wires
// between jacks with a bright dot pulsing down each one on every clock step, so the
// picture finally matches the sound. Knobs are draggable (step 3). Jacks are now an
// addressable table, which is the groundwork for step 5 (making cables editable).
//
//   CLOCK ──tick──► S&H ◄──samples── LFO
//                    └──value──► QUANT ──pitch──► VOICE
//   LFO ───────────────────────────────────────► VOICE filter cutoff (live, every frame)

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

// ── jacks: an addressable table (step 4). type 0 gate / 1 pitch / 2 cv ──
typedef struct { int x, y, type; bool out; } Jack;
enum { J_CLK1, J_CLK2, J_CLK4, J_LFO, J_SH_IN, J_SH_CLK, J_SH_OUT,
       J_QT_IN, J_QT_OUT, J_VO_G, J_VO_P, J_VO_F, J_EU, JACK_N };
Jack jk[JACK_N];

// ── cables: the fixed default patch (src jack → dst jack) ──
typedef struct { int src, dst; } Cable;
Cable cable[] = {
    { J_CLK1,   J_SH_CLK },   // clock ticks the sample & hold
    { J_CLK1,   J_VO_G   },   // clock gates the voice
    { J_LFO,    J_SH_IN  },   // LFO is what the S&H samples
    { J_SH_OUT, J_QT_IN  },   // held value → quantizer
    { J_QT_OUT, J_VO_P   },   // quantized pitch → voice
    { J_LFO,    J_VO_F   },   // LFO also sweeps the voice filter
};
#define NCABLE 6

void setjack(int id, int x, int y, int type, bool out) { jk[id] = (Jack){ x, y, type, out }; }

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);
    instrument_filter(SLOT, FILTER_LOW, 600, 10);

    // positions match the strips drawn in draw() (strip i at x = 6 + i*52, y = 18)
    setjack(J_CLK1,  17, 150, 0, true);   setjack(J_CLK2,  29, 150, 0, true);  setjack(J_CLK4, 41, 150, 0, true);
    setjack(J_LFO,   81, 150, 2, true);
    setjack(J_SH_IN, 123, 48, 2, false);  setjack(J_SH_CLK, 143, 48, 0, false); setjack(J_SH_OUT, 133, 150, 2, true);
    setjack(J_QT_IN, 185, 42, 2, false);  setjack(J_QT_OUT, 185, 150, 1, true);
    setjack(J_VO_G, 223, 42, 0, false);   setjack(J_VO_P, 237, 42, 1, false);  setjack(J_VO_F, 251, 42, 2, false);
    setjack(J_EU,   289, 150, 0, true);
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
    watch("cut", "%d", (int)cutoff);
#endif
}

// ── drawing helpers ──
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE_GREEN; }

int near_col(int x, int y) {   // proximity reveal: label brightens as the cursor nears it
    float d = distance(mouse_x(), mouse_y(), x, y);
    return d < 14 ? CLR_WHITE : d < 30 ? CLR_LIGHT_GREY : d < 54 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

void jack_l(int id, bool lit, const char *name) {
    Jack j = jk[id];
    int c = sig_col(j.type);
    if (j.out) { circ(j.x, j.y, 3, c); if (lit) circfill(j.x, j.y, 1, CLR_WHITE); }
    else         circfill(j.x, j.y, 3, lit ? CLR_WHITE : c);
    print(name, j.x - text_width(name) / 2, j.y + 6, near_col(j.x, j.y));
}

// a drooping bezier cable + a dot that rides it on each clock step
void draw_cable(int sid, int did) {
    Jack s = jk[sid], d = jk[did];
    int mx = (s.x + d.x) / 2, my = (s.y > d.y ? s.y : d.y) + 16;   // control point sags down
    bezier(s.x, s.y, mx, my, d.x, d.y, sig_col(s.type));
    if (tick_flash < 10) {
        float t = tick_flash / 10.0f, u = 1.0f - t;
        int px = (int)(u * u * s.x + 2 * u * t * mx + t * t * d.x);
        int py = (int)(u * u * s.y + 2 * u * t * my + t * t * d.y);
        circfill(px, py, 2, CLR_WHITE);
    }
}

void meter(int x, int y, int w, int h, float v, int col) {
    rectfill(x, y, w, h, CLR_BLACK);
    int fill = (int)(clamp(v, 0, 1) * h);
    rectfill(x, y + h - fill, w, fill, col);
}

float knob_dial(int id, int cx, int cy, float v, float lo, float hi, const char *name, const char *val) {
    if (mouse_pressed(MOUSE_LEFT) && distance(mouse_x(), mouse_y(), cx, cy) < 9) { held_knob = id; drag_y = mouse_y(); }
    if (held_knob == id && mouse_down(MOUSE_LEFT)) {
        v = clamp(v + (drag_y - mouse_y()) * (hi - lo) / 120.0f, lo, hi);
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

void knob_static(int cx, int cy, float v) {
    circfill(cx, cy, 6, CLR_DARKER_GREY);
    circ(cx, cy, 6, CLR_MEDIUM_GREY);
    float a = 135.0f + clamp(v, 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 5), cy + (int)(sin_deg(a) * 5), CLR_DARK_GREY);
}

void draw(void) {
    if (!mouse_down(MOUSE_LEFT)) held_knob = 0;

    cls(CLR_DARKER_BLUE);
    print("MODRACK", 6, 4, CLR_WHITE);
    print("step4: cables", 84, 5, CLR_INDIGO);

    const char *nm[6] = { "CLOCK", "LFO", "S&H", "QUANT", "VOICE", "EUCLID" };
    int col[6]        = { CLR_ORANGE, CLR_PINK, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_DARK_GREY };
    int y = 18, h = 156;
    bool gate_lit = tick_flash < 5;

    for (int i = 0; i < 6; i++) {
        int x = 6 + i * 52, w = 46, cx = x + w / 2;
        rectfill(x, y, w, h, i == 5 ? CLR_BROWNISH_BLACK : CLR_DARKER_PURPLE);
        rect(x, y, w, h, col[i]);
        print(nm[i], x + 3, y + 3, col[i]);

        switch (i) {
            case 0:
                circfill(cx, y + 16, 4, gate_lit ? CLR_WHITE : CLR_DARK_ORANGE);
                p_bpm = knob_dial(1, cx, y + 44, p_bpm, 60, 240, "bpm", str("%d", (int)p_bpm));
                jack_l(J_CLK1, gate_lit, "1"); jack_l(J_CLK2, false, "2"); jack_l(J_CLK4, false, "4");
                break;
            case 1:
                p_rate = knob_dial(2, cx, y + 36, p_rate, 0.1f, 8.0f, "rate", str("%.1f", p_rate));
                meter(x + 8, y + 60, 6, 48, lfo_out, CLR_PINK);
                jack_l(J_LFO, false, "cv");
                break;
            case 2:
                jack_l(J_SH_IN, false, "in"); jack_l(J_SH_CLK, gate_lit, "clk");
                meter(x + 8, y + 56, 6, 52, sh, CLR_YELLOW);
                jack_l(J_SH_OUT, false, "cv");
                break;
            case 3:
                jack_l(J_QT_IN, false, "in");
                p_scale = knob_dial(3, cx, y + 48, p_scale, 0, 5.99f, "scl", SCALES[(int)p_scale]);
                p_root  = knob_dial(4, cx, y + 74, p_root, 0, 11.99f, "root", NOTES[(int)p_root]);
                print(NOTES[cur_midi % 12], cx - text_width(NOTES[cur_midi % 12]) / 2, y + 96, CLR_WHITE);
                jack_l(J_QT_OUT, gate_lit, "pit");
                break;
            case 4:
                jack_l(J_VO_G, gate_lit, "g"); jack_l(J_VO_P, gate_lit, "p"); jack_l(J_VO_F, false, "f");
                p_cut = knob_dial(5, cx, y + 54, p_cut, 200, 2200, "cut", str("%d", (int)p_cut));
                meter(x + 8, y + 74, 6, 40, (cutoff - 300) / 2600.0f, CLR_BLUE);
                if (tick_flash < 8) circ(cx, y + 54, 10 - tick_flash, CLR_LIGHT_PEACH);
                break;
            case 5:
                knob_static(cx, y + 40, 0.4f);
                print("hits", cx - 8, y + 49, CLR_DARK_GREY);
                jack_l(J_EU, false, "g");
                print("step6", x + 6, y + h - 12, CLR_DARK_GREY);
                break;
        }
    }

    for (int c = 0; c < NCABLE; c++) draw_cable(cable[c].src, cable[c].dst);   // patch cables on top

    print("default patch shown - dots ride the cables", 6, 192, CLR_DARKER_GREY);
}
