#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// A generative Berlin-school patch, fully editable, laid out as a 4x2 grid of bays.
// Drag an output jack to an input (type-checked: green=gate, blue=cv, yellow=pitch),
// click an occupied input to rewire, right-click a jack to clear. Knobs drag.
// S saves the patch, L loads, R resets. Two empty bays are waiting for new modules.

#define SLOT 5
#define ROOT_OCT 4

// ── grid layout (parametrized: change these to reshape the rack) ──
#define NCOL 4
#define NBAY 8           // 4 cols x 2 rows
#define GX   4           // left margin
#define GW   72          // bay width
#define GSP  78          // column pitch
#define GY   16          // top of row 0
#define GH   84          // bay height
#define GRP  88          // row pitch
#define bayx(i) (GX + ((i) % NCOL) * GSP)
#define bayy(i) (GY + ((i) / NCOL) * GRP)
#define baycx(i) (bayx(i) + GW / 2)

const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
const char *SCALES[6] = { "maj","min","pen","pnm","blu","chr" };

float p_bpm = 112, p_rate = 0.37f, p_scale = SCALE_PENTA, p_root = 0, p_cut = 700;
float p_hits = 4, p_steps = 8;

float lfo_out = 0, sh = 0, cutoff = 700;
int   cur_midi = 60, voice = -1, last_step = -1, tick_flash = 99, eu_counter = 0, eu_flash = 99;
float prev_clk = 0, prev_gate = 0, prev_euclk = 0;

int   held_knob = 0, drag_y = 0, drag_from = -1, msg_flash = 0;
const char *msg = "";

typedef struct { int x, y, type; bool out; float val; } Jack;
enum { J_CLK1, J_CLK2, J_CLK4, J_LFO, J_SH_IN, J_SH_CLK, J_SH_OUT,
       J_QT_IN, J_QT_OUT, J_VO_G, J_VO_P, J_VO_F, J_EU_CLK, J_EU, JACK_N };
Jack jk[JACK_N];

typedef struct { int src, dst; } Cable;
#define MAXCABLE 24
Cable cable[MAXCABLE];
int   ncable = 0;

void setjack(int id, int x, int y, int type, bool out) { jk[id] = (Jack){ x, y, type, out, 0 }; }
int  cable_into(int dst) { for (int c = 0; c < ncable; c++) if (cable[c].dst == dst) return c; return -1; }
void remove_cable(int idx) { for (int k = idx; k < ncable - 1; k++) cable[k] = cable[k + 1]; ncable--; }
void add_cable(int s, int d) {
    int e = cable_into(d); if (e >= 0) remove_cable(e);
    if (ncable < MAXCABLE) cable[ncable++] = (Cable){ s, d };
}
float read_in(int dst) { int c = cable_into(dst); return c >= 0 ? jk[cable[c].src].val : 0.0f; }

void wire_default(void) {
    ncable = 0;
    add_cable(J_CLK1, J_SH_CLK);  add_cable(J_CLK1, J_VO_G);  add_cable(J_CLK1, J_EU_CLK);
    add_cable(J_LFO, J_SH_IN);    add_cable(J_SH_OUT, J_QT_IN); add_cable(J_QT_OUT, J_VO_P);
    add_cable(J_LFO, J_VO_F);
}

typedef struct {
    int   ncable; Cable cable[MAXCABLE];
    float p_bpm, p_rate, p_scale, p_root, p_cut, p_hits, p_steps;
} Patch;

void save_patch(void) {
    Patch p = { ncable, {0}, p_bpm, p_rate, p_scale, p_root, p_cut, p_hits, p_steps };
    for (int i = 0; i < ncable; i++) p.cable[i] = cable[i];
    save_bytes(&p, sizeof p);
    msg = "SAVED"; msg_flash = 40;
}
void load_patch(void) {
    Patch p;
    if (load_bytes(&p, sizeof p) == (int)sizeof p) {
        ncable = p.ncable < 0 ? 0 : p.ncable > MAXCABLE ? MAXCABLE : p.ncable;
        for (int i = 0; i < ncable; i++) cable[i] = p.cable[i];
        p_bpm = p.p_bpm; p_rate = p.p_rate; p_scale = p.p_scale; p_root = p.p_root;
        p_cut = p.p_cut; p_hits = p.p_hits; p_steps = p.p_steps;
        msg = "LOADED";
    } else msg = "no save";
    msg_flash = 40;
}

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);
    instrument_filter(SLOT, FILTER_LOW, 600, 10);

    // jacks live at each module's grid origin (inputs near top +16, outputs near bottom +72)
    int o0 = bayx(0), y0 = bayy(0);                       // CLOCK
    setjack(J_CLK1, o0 + 16, y0 + 72, 0, true); setjack(J_CLK2, o0 + 36, y0 + 72, 0, true); setjack(J_CLK4, o0 + 56, y0 + 72, 0, true);
    setjack(J_LFO, baycx(1), bayy(1) + 72, 2, true);      // LFO
    int o2 = bayx(2), y2 = bayy(2);                       // S&H
    setjack(J_SH_IN, o2 + 24, y2 + 16, 2, false); setjack(J_SH_CLK, o2 + 48, y2 + 16, 0, false); setjack(J_SH_OUT, baycx(2), y2 + 72, 2, true);
    setjack(J_QT_IN, baycx(3), bayy(3) + 16, 2, false); setjack(J_QT_OUT, baycx(3), bayy(3) + 72, 1, true);
    int o4 = bayx(4), y4 = bayy(4);                       // VOICE
    setjack(J_VO_G, o4 + 18, y4 + 16, 0, false); setjack(J_VO_P, o4 + 36, y4 + 16, 1, false); setjack(J_VO_F, o4 + 54, y4 + 16, 2, false);
    setjack(J_EU_CLK, baycx(5), bayy(5) + 16, 0, false); setjack(J_EU, baycx(5), bayy(5) + 72, 0, true);  // EUCLID
    wire_default();
}

void update(void) {
    bpm((int)p_bpm);
    if (keyp('S')) save_patch();
    if (keyp('L')) load_patch();
    if (keyp('R')) wire_default();
    if (msg_flash > 0) msg_flash--;

    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);
    bool newstep = step8 != last_step;
    if (newstep) { last_step = step8; tick_flash = 0; } else tick_flash++;
    eu_flash++;
    jk[J_CLK1].val = newstep ? 1 : 0;
    jk[J_CLK2].val = (newstep && step8 % 2 == 0) ? 1 : 0;
    jk[J_CLK4].val = (newstep && step8 % 4 == 0) ? 1 : 0;

    static float lfo_phase = 0;
    lfo_phase += p_rate * dt();
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    jk[J_LFO].val = lfo_out = (sin_deg(lfo_phase * 360.0f) + 1.0f) * 0.5f;

    float clk = read_in(J_SH_CLK);
    if (clk > 0.5f && prev_clk <= 0.5f) jk[J_SH_OUT].val = read_in(J_SH_IN);
    prev_clk = clk;
    sh = jk[J_SH_OUT].val;

    int n = (int)(clamp(read_in(J_QT_IN), 0, 1) * 7.999f);
    cur_midi = degree((int)p_scale, ROOT_OCT, n) + (int)p_root;
    jk[J_QT_OUT].val = cur_midi;

    float gate = read_in(J_VO_G);
    if (gate > 0.5f && prev_gate <= 0.5f) {
        int m = (int)read_in(J_VO_P); if (m < 1) m = 48;
        if (voice >= 0) note_off(voice);
        voice = note_on(m, SLOT, 5);
    }
    prev_gate = gate;
    cutoff = p_cut + clamp(read_in(J_VO_F), 0, 1) * 1800.0f;
    if (voice >= 0) note_cutoff(voice, (int)cutoff);

    float euclk = read_in(J_EU_CLK);
    jk[J_EU].val = 0;
    if (euclk > 0.5f && prev_euclk <= 0.5f) {
        eu_counter++;
        if (euclid((int)p_hits, (int)p_steps, eu_counter)) {
            hit(72, INSTR_NOISE, 3, 18);
            hit(43, INSTR_TRI, 7, 90);
            jk[J_EU].val = 1;
            eu_flash = 0;
        }
    }
    prev_euclk = euclk;

#ifdef DE_TRACE
    watch("bpm", "%d", (int)p_bpm);
    watch("cables", "%d", ncable);
#endif
}

// ── drawing helpers ──
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE; }

int near_col(int x, int y) {
    float d = distance(mouse_x(), mouse_y(), x, y);
    return d < 12 ? CLR_WHITE : d < 26 ? CLR_LIGHT_GREY : d < 48 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
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
    print(name, j.x - text_width(name) / 2, j.y + 5, near_col(j.x, j.y));
}

void draw_cable(int sx, int sy, int dx, int dy, int c, bool pulse) {
    int mx = (sx + dx) / 2, my = max(sy, dy) + 14;
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
    if (mouse_pressed(MOUSE_LEFT) && distance(mouse_x(), mouse_y(), cx, cy) < 7) { held_knob = id; drag_y = mouse_y(); }
    if (held_knob == id && mouse_down(MOUSE_LEFT)) {
        v = clamp(v + (drag_y - mouse_y()) * (hi - lo) / 120.0f, lo, hi);
        drag_y = mouse_y();
    }
    bool hot = held_knob == id || distance(mouse_x(), mouse_y(), cx, cy) < 7;
    circfill(cx, cy, 5, CLR_DARKER_GREY);
    circ(cx, cy, 5, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
    float a = 135.0f + clamp((v - lo) / (hi - lo), 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 4), cy + (int)(sin_deg(a) * 4), CLR_WHITE);
    print(name, cx - text_width(name) / 2, cy + 7, near_col(cx, cy));
    if (hot) print(val, cx - text_width(val) / 2, cy + 13, CLR_WHITE);
    return v;
}

void edit_cables(int mx, int my) {
    if (mouse_pressed(MOUSE_LEFT)) {
        int j = jack_at(mx, my);
        if (j >= 0 && jk[j].out)  drag_from = j;
        else if (j >= 0) { int c = cable_into(j); if (c >= 0) { drag_from = cable[c].src; remove_cable(c); } }
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
    font(FONT_NORMAL);
    print("MODRACK", 4, 3, CLR_WHITE);
    font(FONT_SMALL);
    if (msg_flash > 0) print(msg, 80, 5, CLR_LIGHT_PEACH);
    else               print("drag out->in to patch", 80, 5, CLR_INDIGO);

    const char *nm[6] = { "CLOCK", "LFO", "S&H", "QUANT", "VOICE", "EUCLID" };
    int col[6]        = { CLR_ORANGE, CLR_PINK, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_RED };
    bool gate_lit = tick_flash < 5;

    for (int i = 0; i < NBAY; i++) {
        int x = bayx(i), y = bayy(i), cx = baycx(i);

        if (i >= 6) {   // empty bay — room for a new module
            rect(x, y, GW, GH, CLR_DARKER_GREY);
            print("+ add", cx - 10, y + GH / 2 - 3, CLR_DARK_GREY);
            continue;
        }

        rectfill(x, y, GW, GH, CLR_DARKER_PURPLE);
        rect(x, y, GW, GH, col[i]);
        print(nm[i], x + 3, y + 2, col[i]);

        switch (i) {
            case 0:  // CLOCK
                circfill(x + 12, y + 14, 3, gate_lit ? CLR_WHITE : CLR_DARK_ORANGE);
                p_bpm = knob_dial(1, x + 40, y + 34, p_bpm, 60, 240, "bpm", str("%d", (int)p_bpm));
                jack_l(J_CLK1, gate_lit, "1"); jack_l(J_CLK2, jk[J_CLK2].val > 0.5f, "2"); jack_l(J_CLK4, jk[J_CLK4].val > 0.5f, "4");
                break;
            case 1:  // LFO
                p_rate = knob_dial(2, x + 24, y + 36, p_rate, 0.1f, 8.0f, "rate", str("%.1f", p_rate));
                meter(x + 48, y + 18, 6, 46, lfo_out, CLR_PINK);
                jack_l(J_LFO, false, "cv");
                break;
            case 2:  // S&H
                jack_l(J_SH_IN, false, "in"); jack_l(J_SH_CLK, gate_lit, "clk");
                meter(x + 10, y + 30, 6, 36, sh, CLR_YELLOW);
                print(str("%d%%", (int)(sh * 99)), x + 26, y + 40, CLR_DARK_GREY);
                jack_l(J_SH_OUT, false, "cv");
                break;
            case 3:  // QUANT — scale + root
                jack_l(J_QT_IN, false, "in");
                p_scale = knob_dial(3, x + 22, y + 40, p_scale, 0, 5.99f, "scl", SCALES[(int)p_scale]);
                p_root  = knob_dial(4, x + 50, y + 40, p_root, 0, 11.99f, "root", NOTES[(int)p_root]);
                print(NOTES[cur_midi % 12], cx - text_width(NOTES[cur_midi % 12]) / 2, y + 54, CLR_WHITE);
                jack_l(J_QT_OUT, gate_lit, "pit");
                break;
            case 4:  // VOICE
                jack_l(J_VO_G, gate_lit, "g"); jack_l(J_VO_P, gate_lit, "p"); jack_l(J_VO_F, false, "f");
                p_cut = knob_dial(5, x + 24, y + 44, p_cut, 200, 2200, "cut", str("%d", (int)p_cut));
                meter(x + 48, y + 28, 6, 40, (cutoff - 300) / 2600.0f, CLR_BLUE);
                if (tick_flash < 8) circ(x + 24, y + 44, 9 - tick_flash, CLR_LIGHT_PEACH);
                break;
            case 5: {  // EUCLID
                jack_l(J_EU_CLK, gate_lit, "clk");
                int hits = (int)p_hits, steps = (int)p_steps;
                p_hits  = knob_dial(6, x + 22, y + 40, p_hits, 1, 8.99f, "h", str("%d", hits));
                p_steps = knob_dial(7, x + 50, y + 40, p_steps, 2, 16.99f, "s", str("%d", steps));
                for (int s = 0; s < steps; s++) {
                    int dx = x + 8 + (int)(s * (56.0f / steps));
                    bool on = euclid(hits, steps, s);
                    circfill(dx, y + 56, on ? 2 : 1, on ? CLR_RED : CLR_DARK_GREY);
                    if (s == ((eu_counter % steps) + steps) % steps && eu_flash < 6) circ(dx, y + 56, 3, CLR_WHITE);
                }
                jack_l(J_EU, jk[J_EU].val > 0.5f, "g");
                break;
            }
        }
    }

    for (int c = 0; c < ncable; c++)
        draw_cable(jk[cable[c].src].x, jk[cable[c].src].y, jk[cable[c].dst].x, jk[cable[c].dst].y, sig_col(jk[cable[c].src].type), true);

    if (drag_from >= 0) {
        int c = sig_col(jk[drag_from].type);
        draw_cable(jk[drag_from].x, jk[drag_from].y, mx, my, c, false);
        for (int i = 0; i < JACK_N; i++) if (!jk[i].out && jk[i].type == jk[drag_from].type) circ(jk[i].x, jk[i].y, 5, CLR_WHITE);
    }

    print("drag patch   rclick clear   S/L save   R reset", 4, 192, CLR_DARKER_GREY);
    font(FONT_NORMAL);
}
