/* de:meta
{
  "slug": "chipjam",
  "collection": ["device-face", "responsive"],
  "title": "Tiny Chip Jam (mockup)",
  "status": "wip",
  "created": "2026-07-19",
  "kind": ["instrument", "generative"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "WIP chiptune RACK on the device-face paradigm — a 'Tiny Chip Jam' sibling. NOW RESPONSIVE via face.h (Layer 3 of responsive-first-device-face.md): the layout is a 4-row FaceZone[] table (nav band · knob band · green SCREEN hero · play-surface lane) instead of hardcoded 160x100 coordinates, so it reflows to any device and the piano-roll + note-bars share ONE full-width register (they used to be hand-aligned at different widths). Five machines (keys 1-5): PU1 is WIRED FOR REAL — a monophonic INSTR_SQUARE pulse voice, retriggered per 16th, with LIVE note_duty and a working ARPEGGIO (draggable DUTY/ATK/DEC/ARP/SWP knobs). PU2/TRI/NSE/MST are still LOOK mockups.",
    "detail": "The face.h conversion (the Layer-3 proving step): draw() became face_resize() -> face_area() -> face_layout() -> draw into f.box[i], and each face draws into the passed zone Boxes with lay_grid + face_col instead of magic numbers. Surfaced by the conversion: chipjam's flanking soft-keys (SEQ/FLAG/FX/GEN left, KEY/PAT right) fought the full-width register, so the grammar moved them to a horizontal nook at the top of the screen and the roll went full-width — the register principle enforced by construction. Audio is unchanged: one held voice (note_on handle) retriggered each step; note_duty ridden live; ARP cycles a major chord; ATK/DEC rebuild the square envelope on change.",
    "controls": "SPACE = play/stop. Keys 1-5 switch face. PU1: drag DUTY (pulse width), ATK/DEC (envelope), ARP (arp speed), SWP (pitch sweep) — vertical drag or wheel. Other faces are draw-only for now. Resize / rotate — face.h reflows it, never scales."
  }
}
de:meta */
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "face.h"

// TINY CHIP JAM — candy chiptune device-face. PU1 is wired for real (pulse +
// duty + arp); the other faces are still look-mockups. Layout is now DECLARED
// through face.h (Layer 3): four zones — nav band · knob band · green screen
// hero · play-surface lane — reflowed, with the roll + bars on one register.

#define BPM     140
#define PU_SLOT 6
#define STEPS   16

// ── THE LAYOUT — one declarative table (visually top→bottom) ──────────────────
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.12f, "nav"    },   // ① transport + machine cartridges
    { FACE_BAND, EDGE_TOP,    0.24f, "knobs"  },   // ② the candy knob row
    { FACE_HERO, 0,           0.00f, "screen" },   // ③ the green glass (soft-keys + roll)
    { FACE_LANE, EDGE_BOTTOM, 0.32f, "play"   },   // ④ the colour play surface (per-step)
};
#define NZ 4

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
#define PITCH_MAX 19

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

// ── PU1 AUDIO (unchanged by the face.h conversion) ───────────────────────────
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

// ── candy widgets (now positioned from zone cells, not magic numbers) ────────
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
// a knob centred in a cell (radius from the cell — the widgets-size-to-cell rule)
static void knob_in(Box c, float *v, const char *lab, int live) {
    float rh = c.h * 0.34f, rw = c.w * 0.40f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 5, 10);
    int cx = (int)(c.x + c.w / 2), cy = (int)(c.y + r + 1);
    if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    if (live) cknob(v, cx, cy, r, lab); else dknob(cx, cy, r, lab, *v);
}
static void cbtn(Box b, const char *s, int on) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, (int)(b.x + (b.w - text_width(s)) / 2), (int)(b.y + 1), on ? CLR_WHITE : CLR_LIGHT_PEACH);
}
static void lcdbtn(Box b, const char *s, int on) {
    if (on) rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, CLR_MEDIUM_GREEN);
    rrect((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
    font(FONT_TINY); print(s, (int)(b.x + (b.w - text_width(s)) / 2), (int)(b.y + 1), on ? CLR_DARK_GREEN : CLR_LIME_GREEN);
}
static void cartridge(Box b, int m, int foc) {
    int x = (int)b.x, y = (int)b.y, w = (int)b.w - 5, h = (int)b.h;   // leave room for the LED
    rrectfill(x, y, w, h, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + w - 3, y + 1, CLR_WHITE); blend_reset(); }
    rrect(x, y, w, h, 2, foc ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (w - text_width(mac[m].name)) / 2, y + (h - 5) / 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    int lx = x + w + 2, ly = y + h / 2;
    circfill(lx, ly, 2, foc ? CLR_LIME_GREEN : CLR_DARK_GREEN);
    circ(lx, ly, 2, CLR_BROWNISH_BLACK);
}
static void glass(Box g) {
    rrectfill((int)g.x, (int)g.y, (int)g.w, (int)g.h, 3, CLR_DARK_GREEN);
    rrect((int)g.x, (int)g.y, (int)g.w, (int)g.h, 3, CLR_MEDIUM_GREEN);
}
static int roll_y(Box rb, int pit) { return (int)(rb.y + rb.h - 3 - (pit / (float)(PITCH_MAX + 1)) * (rb.h - 6)); }

// ── ① nav spine + cartridges ─────────────────────────────────────────────────
static void draw_nav(Box nav, int lo) {
    rrectfill((int)nav.x, (int)nav.y, (int)nav.w, (int)nav.h, 4, lo);
    Box tp = lay_split(nav, EDGE_LEFT,  lay_clamp(nav.h + 2, 12, 20), &nav);
    Box pw = lay_split(nav, EDGE_RIGHT, lay_clamp(nav.h,     10, 16), &nav);
    // transport
    tp = lay_inset(tp, 1);
    rrectfill((int)tp.x, (int)tp.y, (int)tp.w, (int)tp.h, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect((int)tp.x, (int)tp.y, (int)tp.w, (int)tp.h, 2, CLR_BROWNISH_BLACK);
    int tcx = (int)(tp.x + tp.w / 2), tcy = (int)(tp.y + tp.h / 2);
    if (playing) { rectfill(tcx - 2, tcy - 2, 2, 4, CLR_WHITE); rectfill(tcx + 1, tcy - 2, 2, 4, CLR_WHITE); }
    else trifill(tcx - 2, tcy - 2, tcx - 2, tcy + 2, tcx + 2, tcy, CLR_WHITE);
    // 5 cartridges
    for (int m = 0; m < 5; m++) cartridge(lay_grid(nav, 5, 5, m, 1), m, m == face);
    // power / eject nook
    pw = lay_inset(pw, 1);
    rrectfill((int)pw.x, (int)pw.y, (int)pw.w, (int)pw.h, 2, CLR_DARK_BROWN);
    rrect((int)pw.x, (int)pw.y, (int)pw.w, (int)pw.h, 2, CLR_BROWNISH_BLACK);
    int ex = (int)(pw.x + pw.w / 2), ey = (int)(pw.y + pw.h / 2);
    trifill(ex - 2, ey, ex + 2, ey, ex, ey - 2, CLR_LIGHT_PEACH);
    rectfill(ex - 1, ey, 3, 2, CLR_LIGHT_PEACH);
}

// soft-key nook (horizontal, top of the screen) — was a flanking rail; the grammar
// moved it here so the roll can use the full-width register.
static void softkeys(Box sk, const char **labels, int n, int active) {
    for (int i = 0; i < n; i++) lcdbtn(lay_inset(lay_grid(sk, n, n, i, 2), 0), labels[i], i == active);
}

// ── MELODIC face (PU1 live; PU2/TRI mock) ────────────────────────────────────
static void face_melodic(Box kb, Box scr, Box play, Face *f, int step, int col, int live) {
    static const char *kn[5] = { "DUTY", "ATK", "DEC", "ARP", "SWP" };
    float *kp[5] = { &k_duty, &k_atk, &k_dec, &k_arp, &k_swp };
    for (int i = 0; i < 5; i++) knob_in(lay_grid(kb, 6, 6, i, 1), kp[i], kn[i], live);
    cbtn(lay_inset(lay_grid(kb, 6, 6, 5, 1), 3), "DF", 0);

    glass(scr);
    // a screen WITH side-buttons (paradigm zone 3): soft-keys flank the HERO, the roll rides
    // the bounded middle. One call — pass 0,0 for a fullscreen screen instead (see face.h).
    static const char *skL[4] = { "SEQ", "FLAG", "FX", "GEN" };
    static const char *skR[2] = { "KEY", "PAT" };
    FaceScreen sc = face_screen(lay_inset(scr, 3), 4, 2, 0.16f, 0.14f, STEPS);
    for (int i = 0; i < 4; i++) lcdbtn(face_key(sc.left,  4, i), skL[i], i == 0);
    for (int i = 0; i < 2; i++) lcdbtn(face_key(sc.right, 2, i), skR[i], 0);
    Box si = sc.screen;
    // piano-roll — the BOUNDED lane in the flanked middle
    for (int s = 0; s < STEPS; s++) {
        Box c = lay_lane_cell(sc.lane, si, s, 0);
        if (s == step) { blend(BLEND_AVG); rectfill((int)c.x, (int)si.y, (int)c.w, (int)si.h, CLR_MEDIUM_GREEN); blend_reset(); }
        if (!p_on[s]) continue;
        int y = roll_y(si, p_pit[s]);
        rectfill((int)c.x + 1, y, (int)c.w - 2, 3, p_acc[s] ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
        if (p_arp[s]) { pset((int)c.x + 2, y - 2, CLR_LIME_GREEN); pset((int)c.x + 3, y - 4, CLR_LIME_GREEN); }
    }
    // note-bars — SAME register, so they line up under the roll
    for (int s = 0; s < STEPS; s++) {
        Box c = face_col(f, play, s, 1);
        if (s == step) rrect((int)c.x - 1, (int)play.y, (int)c.w + 2, (int)play.h, 1, CLR_WHITE);
        if (!p_on[s]) { rectfill((int)c.x, (int)(play.y + play.h - 2), (int)c.w, 2, CLR_DARK_BROWN); continue; }
        float hf = 0.25f + 0.75f * p_pit[s] / (float)PITCH_MAX;
        int h = (int)(play.h * hf), ty = (int)(play.y + play.h - h);
        rrectfill((int)c.x, ty, (int)c.w, h, 1, col);
        rrect((int)c.x, ty, (int)c.w, h, 1, CLR_BROWNISH_BLACK);
        if (p_acc[s]) rectfill((int)c.x, ty - 3, (int)c.w, 2, CLR_ORANGE);
        if (p_arp[s]) { for (int a = 0; a < 3; a++) pset((int)(c.x + c.w / 2), ty - 5 - a * 2, CLR_LIGHT_YELLOW); }
    }
}

// ── NSE noise-DRUM face (mock) ───────────────────────────────────────────────
static void face_drums(Box kb, Box scr, Box play, int step, int col) {
    static const char *kn[4] = { "TUNE", "DEC", "TONE", "DIST" };
    static float kv[4] = { 0.50f, 0.45f, 0.60f, 0.30f };
    for (int i = 0; i < 4; i++) knob_in(lay_grid(kb, 4, 4, i, 1), &kv[i], kn[i], 0);

    glass(scr);
    // same idiom as the melodic face — here the left flank is a voice-name gutter (not buttons),
    // and the grid rides the bounded middle lane.
    FaceScreen sc = face_screen(lay_inset(scr, 3), 1, 0, 0.14f, 0, STEPS);
    Box si = sc.screen;
    for (int v = 0; v < 6; v++) {
        Box rb = face_key(sc.left, 6, v);
        font(FONT_TINY); print(dvn[v], (int)rb.x, (int)(rb.y + rb.h / 2 - 2), CLR_MEDIUM_GREEN);
    }
    for (int s = 0; s < STEPS; s++) {
        Box cs = lay_lane_cell(sc.lane, si, s, 0);
        if (s == step) { blend(BLEND_AVG); rectfill((int)cs.x, (int)si.y, (int)cs.w, (int)si.h, CLR_MEDIUM_GREEN); blend_reset(); }
        for (int v = 0; v < 6; v++) {
            Box cell = lay_grid(si, 1, 6, v, 1);
            int cx = (int)cs.x, cy = (int)cell.y;
            if (dgrid[v][s]) rrectfill(cx, cy, (int)cs.w - 1, (int)cell.h - 1, 1, dacc[v][s] ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
            else pset(cx + (int)cs.w / 2, cy + (int)cell.h / 2, (s % 4 == 0) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN);
        }
    }
    // 6 pads (NOT per-step → a plain 6-grid in the play zone)
    for (int v = 0; v < 6; v++) {
        Box p = lay_inset(lay_grid(play, 6, 6, v, 2), 1);
        int hit = dgrid[v][step];
        rrectfill((int)p.x, (int)p.y, (int)p.w, (int)p.h, 3, hit ? col : CLR_DARK_BROWN);
        rrect((int)p.x, (int)p.y, (int)p.w, (int)p.h, 3, CLR_BROWNISH_BLACK);
        circfill((int)(p.x + p.w - 5), (int)(p.y + 4), 2, hit ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        font(FONT_TINY); plabel(dvn[v], (int)(p.x + p.w / 2), (int)(p.y + p.h / 2 - 2), hit ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
    }
}

// ── MST master face (mock) ───────────────────────────────────────────────────
static void face_master(Box kb, Box scr, Box play, int step) {
    static const char *kn[5] = { "GLU", "FILT", "SWG", "TEMP", "VOL" };
    static float kv[5] = { 0.60f, 0.75f, 0.30f, 0.51f, 0.80f };
    for (int i = 0; i < 5; i++) knob_in(lay_grid(kb, 5, 5, i, 1), &kv[i], kn[i], 0);

    glass(scr);
    Box si = lay_inset(scr, 3);
    static const char *sk[4] = { "MIX", "PCF", "CRU", "GAT" };
    Box skrow = lay_split(si, EDGE_TOP, lay_clamp(si.h * 0.26f, 6, 10), &si);
    softkeys(skrow, sk, 4, 0);
    for (int x = 0; x < (int)si.w; x++)   // a little scope
        pset((int)si.x + x, (int)(si.y + si.h / 2 + dy(si.h * 0.28f, x * 9 + step * 20)), CLR_LIME_GREEN);
    // 5 mixer bars in the play zone (per-machine, not per-step)
    for (int m = 0; m < 5; m++) {
        Box c = lay_inset(lay_grid(play, 5, 5, m, 2), 1);
        int h = (int)(c.h * (0.3f + ((m * 7 + step) % 22) / 30.0f));
        rrect((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1, CLR_BROWNISH_BLACK);
        rectfill((int)c.x + 1, (int)(c.y + c.h - h), (int)c.w - 2, h, mac[m].col);
        font(FONT_TINY); plabel(mac[m].name, (int)(c.x + c.w / 2), (int)(c.y + c.h - 6), CLR_BROWNISH_BLACK);
    }
}

void draw(void) {
    face_resize();                                   // ① chunky canvas (route 2)
    Box area = face_area(1);                          // ② content = screen inset 1, ∩ safe-rect
    Face f = face_layout(area, ZONES, NZ, STEPS);     // ③ carve + enforce + build the register
    int step = g_step, lo = mac[face].lo, col = mac[face].col;

    cls(CLR_BROWNISH_BLACK);                          // chassis bleeds to every edge
    Box full = box(0, 0, screen_w(), screen_h());
    rrectfill((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_DARK_BROWN);
    rrect((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_BROWNISH_BLACK);

    ui_begin();
    draw_nav(f.box[0], lo);
    if (face == 3)      face_drums(f.box[1], f.box[2], f.box[3], step, col);
    else if (face == 4) face_master(f.box[1], f.box[2], f.box[3], step);
    else                face_melodic(f.box[1], f.box[2], f.box[3], &f, step, col, face == 0);   // PU1 live, PU2/TRI mock
    ui_end();
}
