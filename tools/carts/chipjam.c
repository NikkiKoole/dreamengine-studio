/* de:meta
{
  "slug": "chipjam",
  "collection": ["device-face"],
  "title": "Tiny Chip Jam (mockup)",
  "status": "wip",
  "created": "2026-07-19",
  "kind": ["instrument", "generative"],
  "genre": null,
  "teaches": [],
  "description": {
    "summary": "WIP chiptune RACK on the acidcandy device-face — a 'Tiny Chip Jam' sibling at 160x100 x4. THREE-PART layout: candy KNOBS on top, a green SCREEN in the middle, a colour PLAY surface below. Five machines (keys 1-5): PU1 is WIRED FOR REAL — a monophonic INSTR_SQUARE pulse voice, retriggered per 16th, with LIVE note_duty and a working ARPEGGIO on the arp steps (draggable DUTY/ATK/DEC/ARP/SWP knobs). PU2/TRI/NSE/MST are still LOOK mockups.",
    "detail": "PU1 audio: one held voice (note_on handle), retriggered each step; note_duty ridden live from the DUTY knob (maps to pulse width); ARP steps cycle a major chord {0,4,7,12} on the held voice via note_pitch at ARP-knob rate; SWP glides the pitch into the note; ATK/DEC rebuild the square instrument's envelope on change. The bet: a chiptune pulse channel IS a 303 line in UX, so PU1/PU2/TRI share the melodic face; NSE is the drum face; MST is master. Auto-plays on load; SPACE toggles transport.",
    "controls": "SPACE = play/stop. Keys 1-5 switch face. PU1: drag DUTY (pulse width), ATK/DEC (envelope), ARP (arp speed), SWP (pitch sweep) — vertical drag or wheel. Other faces are draw-only for now."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"

// TINY CHIP JAM — candy chiptune device-face. PU1 is wired for real (pulse +
// duty + arp); the other faces are still look-mockups. Three zones:
// candy knobs (top) · green screen (middle) · colour play surface (below).

#define BPM     140
#define PU_SLOT 6

typedef struct { const char *name; int col, lo; } Machine;
static Machine mac[5] = {
    { "PU1", CLR_TRUE_BLUE,  CLR_DARK_BLUE   },   // pulse 1 (lead) — WIRED
    { "PU2", CLR_PINK,       CLR_DARK_PURPLE },   // pulse 2 (harmony) — mock
    { "TRI", CLR_ORANGE,     CLR_DARK_ORANGE },   // triangle (bass) — mock
    { "NSE", CLR_LIGHT_GREY, CLR_DARKER_GREY },   // noise (drum kit) — mock
    { "MST", CLR_YELLOW,     CLR_DARK_GREEN  },   // master — mock
};
static int face = 0;   // start on PU1

// PU1 pattern (16 steps)
static const int p_on[16]  = { 1,1,1,1, 1,0,1,1, 1,1,1,0, 1,1,1,1 };
static const int p_pit[16] = { 0,12,7,12, 0,0,7,15, 5,17,12,0, 3,15,10,19 };  // semitones above root
static const int p_acc[16] = { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,1 };
static const int p_arp[16] = { 0,1,1,0, 0,0,1,1, 0,1,0,0, 0,1,0,1 };          // an arp (fake-chord) step
#define ROOT 52

// live PU1 params (0..1), all draggable
static float k_duty = 0.25f, k_atk = 0.10f, k_dec = 0.55f, k_arp = 0.55f, k_swp = 0.0f;
static int   playing  = 1;
static int   ph       = -1;    // the held pulse voice handle
static int   cur_base = ROOT;  // the sounding step's root midi (arp cycles around it)
static int   g_step   = 0;     // current 16th (set in update, drawn in draw)
static int   last_step = -1;
static int   last_atk = -99, last_dec = -99;

// NSE mock kit
static const char *dvn[6] = { "KICK", "SNR", "HAT", "OHAT", "CLAP", "ZAP" };
static const int dgrid[6][16] = {
    { 1,0,0,0, 1,0,0,1, 1,0,0,0, 1,0,1,0 },
    { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
    { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 },
    { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 },
    { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
    { 0,0,0,0, 0,0,0,0, 0,0,0,1, 0,0,0,1 },
};
static const int dacc[6][16] = {
    { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 },
    { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
    {0}, {0}, {0}, {0},
};

static float dutypw(void) { return 0.06f + k_duty * 0.44f; }   // knob -> pulse width 0.06..0.50
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// ── PU1 AUDIO ────────────────────────────────────────────────────────────────
static void trig(int step) {
    int base = ROOT + p_pit[step];
    cur_base = base;
    if (!p_on[step]) { if (ph >= 0) { note_off(ph); ph = -1; } return; }
    int vol = p_acc[step] ? 7 : 5;
    if (ph >= 0) note_off(ph);
    ph = note_on(base, PU_SLOT, vol);
    note_glide(ph, 0);
    note_duty(ph, dutypw());
    if (!p_arp[step] && k_swp > 0.03f) {          // sweep the pitch into the note
        note_pitch(ph, base + (int)(k_swp * 12));
        note_glide(ph, 90);
        note_pitch(ph, base);
    }
}

void init(void) {
    instrument(PU_SLOT, INSTR_SQUARE, 2, 140, 5, 60);
    instrument_duty(PU_SLOT, dutypw());
}

void update(void) {
    for (int i = 0; i < 5; i++) if (keyp('1' + i)) face = i;
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing && ph >= 0) { note_off(ph); ph = -1; } last_step = -1; }

    // ATK/DEC rebuild the envelope only when they change (set-and-hold)
    int atk = (int)(k_atk * 30), dec = 20 + (int)(k_dec * 280);
    if (atk != last_atk || dec != last_dec) { instrument(PU_SLOT, INSTR_SQUARE, atk, dec, 5, 60); last_atk = atk; last_dec = dec; }

    if (!playing) return;
    float s16 = 60.0f / BPM / 4.0f;               // seconds per 16th
    int step = (int)(now() / s16) % 16;
    g_step = step;
    if (step != last_step) { last_step = step; trig(step); }

    // ARP: cycle a chord on the held voice at the ARP-knob rate
    if (ph >= 0 && p_on[step] && p_arp[step]) {
        static const int chord[4] = { 0, 4, 7, 12 };
        int rate = 8 + (int)(k_arp * 28);         // 8..36 Hz
        int ai = ((int)(now() * rate)) % 4;
        note_glide(ph, 0);
        note_pitch(ph, cur_base + chord[ai]);
    }
    if (ph >= 0) note_duty(ph, dutypw());          // live duty ride
}

// ── widgets ──
// draggable candy rotary (ui.h capture, candy look) — PU1's live knobs
static void cknob(float *v, int cx, int cy, int r, const char *label) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = clamp(c->v0 + (c->by - py) / 24.0f, 0, 1);
    }
    int hot = c != 0 || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    float ang = 150 + *v * 240;
    if (hot) { blend(BLEND_AVG); circfill(cx, cy, r + 1, CLR_WHITE); blend_reset(); }
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    if (c) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);
    circ(cx, cy, r, c ? CLR_WHITE : hot ? CLR_LIGHT_PEACH : CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang),
         cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), c ? CLR_ORANGE : CLR_WHITE);
    font(FONT_TINY);
    if (c) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 }; plabel(b, cx, cy + r + 1, CLR_DARK_BROWN); }
    else plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}
// static (non-interactive) candy knob — the mock faces
static void dknob(int cx, int cy, int r, const char *label, float v) {
    float ang = 150 + v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}
static void cbtn(int x, int y, int w, int h, const char *s, int on) {
    rrectfill(x, y, w, h, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(x, y, w, h, 2, CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, on ? CLR_WHITE : CLR_LIGHT_PEACH);
}
static void lcdbtn(int x, int y, int w, int h, const char *s, int on) {
    if (on) rrectfill(x, y, w, h, 2, CLR_MEDIUM_GREEN);
    rrect(x, y, w, h, 2, on ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, on ? CLR_DARK_GREEN : CLR_LIME_GREEN);
}
static void cartridge(int m, int foc) {
    int x = 19 + m * 25, y = 0;
    rrectfill(x, y, 24, 10, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + 19, y + 1, CLR_WHITE); blend_reset(); }
    rrect(x, y, 24, 10, 2, foc ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (16 - text_width(mac[m].name)) / 2, y + 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    int lx = x + 20, ly = y + 5;
    circfill(lx, ly, 2, foc ? CLR_LIME_GREEN : CLR_DARK_GREEN);
    circ(lx, ly, 2, CLR_BROWNISH_BLACK);
}
static void glass(void) {
    rrectfill(4, 37, 152, 30, 3, CLR_DARK_GREEN);
    rrect(4, 37, 152, 30, 3, CLR_MEDIUM_GREEN);
}

// ── MELODIC face (PU1 live; PU2/TRI mock) ──
static void face_melodic(int step, int col, int live) {
    static const char *kn[5] = { "DUTY", "ATK", "DEC", "ARP", "SWP" };
    float *kp[5] = { &k_duty, &k_atk, &k_dec, &k_arp, &k_swp };
    for (int i = 0; i < 5; i++) {
        int cx = 22 + i * 28;
        if (live) cknob(kp[i], cx, 23, 8, kn[i]);
        else dknob(cx, 23, 8, kn[i], *kp[i]);
    }
    cbtn(144, 17, 13, 9, "DF", 0);

    glass();
    static const char *sk[4] = { "SEQ", "FLAG", "FX", "GEN" };
    for (int i = 0; i < 4; i++) lcdbtn(6, 38 + i * 7, 20, 6, sk[i], i == 0);
    lcdbtn(136, 38, 18, 6, "KEY", 0);
    lcdbtn(136, 45, 18, 6, "PAT", 0);
    int rx = 30, rw = 102, base = 63;
    for (int s = 0; s < 16; s++) {
        int cx = rx + (s * rw) / 16;
        if (s == step) { blend(BLEND_AVG); rectfill(cx, 39, 6, 26, CLR_MEDIUM_GREEN); blend_reset(); }
        if (!p_on[s]) continue;
        int y = base - p_pit[s];
        rectfill(cx + 1, y, 4, 3, p_acc[s] ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
        if (p_arp[s]) { pset(cx + 2, y - 2, CLR_LIME_GREEN); pset(cx + 3, y - 4, CLR_LIME_GREEN); }
    }
    int by = 95;
    for (int s = 0; s < 16; s++) {
        int bx = 8 + s * 9, bw = 8;
        if (s == step) rrect(bx - 1, 68, bw + 2, 29, 1, CLR_WHITE);
        if (!p_on[s]) { rectfill(bx, by - 1, bw, 2, CLR_DARK_BROWN); continue; }
        int h = 5 + p_pit[s], ty = by - h;
        rrectfill(bx, ty, bw, h, 1, col);
        rrect(bx, ty, bw, h, 1, CLR_BROWNISH_BLACK);
        if (p_acc[s]) rectfill(bx, ty - 3, bw, 2, CLR_ORANGE);
        if (p_arp[s]) { for (int a = 0; a < 3; a++) pset(bx + bw / 2, ty - 5 - a * 2, CLR_LIGHT_YELLOW); }
    }
}

// ── NSE noise-DRUM face (mock) ──
static void face_drums(int step, int col) {
    static const char *kn[4] = { "TUNE", "DEC", "TONE", "DIST" };
    static const float kv[4] = { 0.50f, 0.45f, 0.60f, 0.30f };
    for (int i = 0; i < 4; i++) dknob(28 + i * 34, 23, 8, kn[i], kv[i]);
    glass();
    int gx0 = 26, gy0 = 40, cw = 8, rh = 4;
    { int cx = gx0 + step * cw; blend(BLEND_AVG); rectfill(cx, gy0 - 1, cw, 6 * rh + 1, CLR_MEDIUM_GREEN); blend_reset(); }
    for (int v = 0; v < 6; v++) {
        int ry = gy0 + v * rh;
        font(FONT_TINY); print(dvn[v], 6, ry - 1, CLR_MEDIUM_GREEN);
        for (int s = 0; s < 16; s++) {
            int cx = gx0 + s * cw;
            if (dgrid[v][s]) rrectfill(cx, ry, cw - 1, rh - 1, 1, dacc[v][s] ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
            else pset(cx + (cw - 1) / 2, ry + 1, (s % 4 == 0) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN);
        }
    }
    for (int v = 0; v < 6; v++) {
        int px = 6 + v * 25, pw = 23, py = 70, ph2 = 25, hit = dgrid[v][step];
        rrectfill(px, py, pw, ph2, 3, hit ? col : CLR_DARK_BROWN);
        rrect(px, py, pw, ph2, 3, CLR_BROWNISH_BLACK);
        circfill(px + pw - 5, py + 4, 2, hit ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        font(FONT_TINY); plabel(dvn[v], px + pw / 2, py + ph2 / 2, hit ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
    }
}

// ── MST master face (mock) ──
static void face_master(int step, int col) {
    static const char *kn[5] = { "GLU", "FILT", "SWG", "TEMP", "VOL" };
    static const float kv[5] = { 0.60f, 0.75f, 0.30f, 0.51f, 0.80f };
    for (int i = 0; i < 5; i++) dknob(22 + i * 28, 23, 8, kn[i], kv[i]);
    glass();
    static const char *sk[4] = { "MIX", "PCF", "CRU", "GAT" };
    for (int i = 0; i < 4; i++) lcdbtn(8 + i * 37, 39, 34, 7, sk[i], i == 0);
    int py = 56;
    for (int x = 8; x < 152; x++) pset(x, py + (int)(dy(6, x * 9 + step * 20)), CLR_LIME_GREEN);
    for (int m = 0; m < 5; m++) {
        int bx = 14 + m * 28, w = 18, h = 8 + ((m * 7 + step) % 22);
        rrect(bx, 70, w, 26, 1, CLR_BROWNISH_BLACK);
        rectfill(bx + 1, 95 - h, w - 2, h, mac[m].col);
        font(FONT_TINY); plabel(mac[m].name, bx + w / 2, 89, CLR_BROWNISH_BLACK);
    }
}

void draw(void) {
    ui_begin();
    int step = g_step, lo = mac[face].lo, col = mac[face].col;

    cls(CLR_BROWNISH_BLACK);
    rrectfill(1, 1, 158, 98, 6, CLR_DARK_BROWN);
    rrect(1, 1, 158, 98, 6, CLR_BROWNISH_BLACK);

    // nav spine + cartridges
    rrectfill(3, 2, 154, 10, 5, lo);
    rectfill(3, 7, 154, 5, lo);
    rrectfill(5, 0, 14, 10, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(5, 0, 14, 10, 2, CLR_BROWNISH_BLACK);
    if (playing) { rectfill(9, 3, 2, 4, CLR_WHITE); rectfill(13, 3, 2, 4, CLR_WHITE); }
    else trifill(9, 3, 9, 7, 14, 5, CLR_WHITE);
    for (int m = 0; m < 5; m++) cartridge(m, m == face);
    rrectfill(143, 0, 12, 10, 2, CLR_DARK_BROWN);
    rrect(143, 0, 12, 10, 2, CLR_BROWNISH_BLACK);
    trifill(146, 5, 152, 5, 149, 2, CLR_LIGHT_PEACH);
    rectfill(147, 5, 5, 3, CLR_LIGHT_PEACH);

    if (face == 3)      face_drums(step, col);
    else if (face == 4) face_master(step, col);
    else                face_melodic(step, col, face == 0);   // PU1 live, PU2/TRI mock

    ui_end();
}
