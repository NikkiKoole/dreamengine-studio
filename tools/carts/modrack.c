#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// STEP 5: the cables are now EDITABLE and the engine is CABLE-DRIVEN. Each module reads
// its input jacks and writes its outputs; signals only flow where a cable carries them.
// Drag from an output jack (hollow) to an input jack (filled) to patch — type-checked
// (gate→gate, pitch→pitch, cv→cv). Click an occupied input to grab its cable and rewire.
// Right-click a jack to clear its cables. Knobs still drag (step 3).
//
// The default patch (wired on load) is the generative Berlin-school chain:
//   CLOCK → S&H clk,  LFO → S&H in,  S&H → QUANT,  QUANT → VOICE pitch,
//   CLOCK → VOICE gate,  LFO → VOICE filter. Repatch it however you like.

#define SLOT 5
#define ROOT_OCT 4

const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
const char *SCALES[6] = { "maj","min","pen","pnm","blu","chr" };

// patch parameters — knob-backed floats (step 3)
float p_bpm = 112, p_rate = 0.37f, p_scale = SCALE_PENTA, p_root = 0, p_cut = 700;

// runtime display mirrors
float lfo_out = 0, sh = 0, cutoff = 700;
int   cur_midi = 60, voice = -1, last_step = -1, tick_flash = 99;
float prev_clk = 0, prev_gate = 0;            // edge detection on patched inputs

// UI state
int   held_knob = 0, drag_y = 0, drag_from = -1;

// ── jacks: addressable table. type 0 gate / 1 pitch / 2 cv. val = signal this frame ──
typedef struct { int x, y, type; bool out; float val; } Jack;
enum { J_CLK1, J_CLK2, J_CLK4, J_LFO, J_SH_IN, J_SH_CLK, J_SH_OUT,
       J_QT_IN, J_QT_OUT, J_VO_G, J_VO_P, J_VO_F, J_EU, JACK_N };
Jack jk[JACK_N];

// ── cables (mutable now): src jack → dst jack ──
typedef struct { int src, dst; } Cable;
#define MAXCABLE 24
Cable cable[MAXCABLE];
int   ncable = 0;

void setjack(int id, int x, int y, int type, bool out) { jk[id] = (Jack){ x, y, type, out, 0 }; }
int  cable_into(int dst) { for (int c = 0; c < ncable; c++) if (cable[c].dst == dst) return c; return -1; }
void remove_cable(int idx) { for (int k = idx; k < ncable - 1; k++) cable[k] = cable[k + 1]; ncable--; }
void add_cable(int s, int d) {                 // an input takes exactly one cable
    int e = cable_into(d); if (e >= 0) remove_cable(e);
    if (ncable < MAXCABLE) cable[ncable++] = (Cable){ s, d };
}
float read_in(int dst) { int c = cable_into(dst); return c >= 0 ? jk[cable[c].src].val : 0.0f; }

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);
    instrument_filter(SLOT, FILTER_LOW, 600, 10);

    setjack(J_CLK1,  17, 150, 0, true);   setjack(J_CLK2,  29, 150, 0, true);  setjack(J_CLK4, 41, 150, 0, true);
    setjack(J_LFO,   81, 150, 2, true);
    setjack(J_SH_IN, 123, 48, 2, false);  setjack(J_SH_CLK, 143, 48, 0, false); setjack(J_SH_OUT, 133, 150, 2, true);
    setjack(J_QT_IN, 185, 42, 2, false);  setjack(J_QT_OUT, 185, 150, 1, true);
    setjack(J_VO_G, 223, 42, 0, false);   setjack(J_VO_P, 237, 42, 1, false);  setjack(J_VO_F, 251, 42, 2, false);
    setjack(J_EU,   289, 150, 0, true);

    add_cable(J_CLK1, J_SH_CLK);  add_cable(J_CLK1, J_VO_G);  add_cable(J_LFO, J_SH_IN);
    add_cable(J_SH_OUT, J_QT_IN); add_cable(J_QT_OUT, J_VO_P); add_cable(J_LFO, J_VO_F);
}

void update(void) {
    bpm((int)p_bpm);

    // ── CLOCK: gate outs pulse high on a new step (/1 every step, /2 every 2, /4 every 4)
    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);
    bool newstep = step8 != last_step;
    if (newstep) { last_step = step8; tick_flash = 0; } else tick_flash++;
    jk[J_CLK1].val = newstep ? 1 : 0;
    jk[J_CLK2].val = (newstep && step8 % 2 == 0) ? 1 : 0;
    jk[J_CLK4].val = (newstep && step8 % 4 == 0) ? 1 : 0;

    // ── LFO: slow wander 0..1
    static float lfo_phase = 0;
    lfo_phase += p_rate * dt();
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    jk[J_LFO].val = lfo_out = (sin_deg(lfo_phase * 360.0f) + 1.0f) * 0.5f;

    // ── S&H: on a rising clock edge at its trigger input, latch its CV input
    float clk = read_in(J_SH_CLK);
    if (clk > 0.5f && prev_clk <= 0.5f) jk[J_SH_OUT].val = read_in(J_SH_IN);
    prev_clk = clk;
    sh = jk[J_SH_OUT].val;

    // ── QUANT: snap the held CV (0..1) to a scale degree → MIDI
    int n = (int)(clamp(read_in(J_QT_IN), 0, 1) * 7.999f);
    cur_midi = degree((int)p_scale, ROOT_OCT, n) + (int)p_root;
    jk[J_QT_OUT].val = cur_midi;

    // ── VOICE: gate retriggers a held note at the pitch input; filter CV sweeps it live
    float gate = read_in(J_VO_G);
    if (gate > 0.5f && prev_gate <= 0.5f) {
        int m = (int)read_in(J_VO_P); if (m < 1) m = 48;     // unpatched pitch → a default note
        if (voice >= 0) note_off(voice);
        voice = note_on(m, SLOT, 5);
    }
    prev_gate = gate;
    cutoff = p_cut + clamp(read_in(J_VO_F), 0, 1) * 1800.0f;
    if (voice >= 0) note_cutoff(voice, (int)cutoff);

#ifdef DE_TRACE
    watch("bpm", "%d", (int)p_bpm);
    watch("cables", "%d", ncable);
#endif
}

// ── drawing helpers ──
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE_GREEN; }

int near_col(int x, int y) {
    float d = distance(mouse_x(), mouse_y(), x, y);
    return d < 14 ? CLR_WHITE : d < 30 ? CLR_LIGHT_GREY : d < 54 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

int jack_at(int mx, int my) {
    for (int i = 0; i < JACK_N; i++) if (distance(mx, my, jk[i].x, jk[i].y) < 6) return i;
    return -1;
}

void jack_l(int id, bool lit, const char *name) {
    Jack j = jk[id];
    int c = sig_col(j.type);
    if (j.out) { circ(j.x, j.y, 3, c); if (lit) circfill(j.x, j.y, 1, CLR_WHITE); }
    else         circfill(j.x, j.y, 3, lit ? CLR_WHITE : c);
    print(name, j.x - text_width(name) / 2, j.y + 6, near_col(j.x, j.y));
}

void draw_cable(int sx, int sy, int dx, int dy, int c, bool pulse) {
    int mx = (sx + dx) / 2, my = max(sy, dy) + 16;
    bezier(sx, sy, mx, my, dx, dy, c);
    if (pulse && tick_flash < 10) {
        float t = tick_flash / 10.0f, u = 1.0f - t;
        circfill((int)(u * u * sx + 2 * u * t * mx + t * t * dx),
                 (int)(u * u * sy + 2 * u * t * my + t * t * dy), 2, CLR_WHITE);
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

// patch-cable editing: drag out→in to connect, click an occupied input to grab, right-click to clear
void edit_cables(int mx, int my) {
    if (mouse_pressed(MOUSE_LEFT)) {
        int j = jack_at(mx, my);
        if (j >= 0 && jk[j].out)  drag_from = j;                           // start from an output
        else if (j >= 0) { int c = cable_into(j); if (c >= 0) { drag_from = cable[c].src; remove_cable(c); } }  // grab existing
    }
    if (mouse_released(MOUSE_LEFT)) {
        if (drag_from >= 0) {
            int j = jack_at(mx, my);
            if (j >= 0 && !jk[j].out && jk[j].type == jk[drag_from].type) add_cable(drag_from, j);
        }
        drag_from = -1;
    }
    if (mouse_pressed(MOUSE_RIGHT)) {
        int j = jack_at(mx, my);
        if (j >= 0) for (int c = ncable - 1; c >= 0; c--) if (cable[c].src == j || cable[c].dst == j) remove_cable(c);
    }
}

void draw(void) {
    int mx = mouse_x(), my = mouse_y();
    if (!mouse_down(MOUSE_LEFT)) held_knob = 0;
    edit_cables(mx, my);

    cls(CLR_DARKER_BLUE);
    print("MODRACK", 6, 4, CLR_WHITE);
    print("step5: patch it yourself", 84, 5, CLR_INDIGO);

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
                jack_l(J_CLK1, gate_lit, "1"); jack_l(J_CLK2, jk[J_CLK2].val > 0.5f, "2"); jack_l(J_CLK4, jk[J_CLK4].val > 0.5f, "4");
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

    // patched cables
    for (int c = 0; c < ncable; c++)
        draw_cable(jk[cable[c].src].x, jk[cable[c].src].y, jk[cable[c].dst].x, jk[cable[c].dst].y, sig_col(jk[cable[c].src].type), true);

    // a cable being dragged: rubber-band to the cursor + highlight compatible input jacks
    if (drag_from >= 0) {
        int c = sig_col(jk[drag_from].type);
        draw_cable(jk[drag_from].x, jk[drag_from].y, mx, my, c, false);
        for (int i = 0; i < JACK_N; i++) if (!jk[i].out && jk[i].type == jk[drag_from].type) circ(jk[i].x, jk[i].y, 5, CLR_WHITE);
    }

    print("drag out->in to patch  click input to grab  rclick clears", 6, 192, CLR_DARKER_GREY);
}
