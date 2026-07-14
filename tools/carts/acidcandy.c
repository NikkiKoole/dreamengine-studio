/* de:meta
{
  "slug": "acidcandy",
  "collection": ["device-face"],
  "title": "acid candy (160x100)",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "instrument",
    "generative"
  ],
  "genre": null,
  "teaches": [],
  "description": {
    "summary": "A candy-toy acid RACK on the device-face skeleton at half resolution (160x100 x4): a colour-cartridge nav strip you focus machines through, all on one transport. Two live TB-303 lines share the extracted runtime/acid303.h voice; drum + master faces are stubbed in.",
    "detail": "The nav backbone is a strip of candy 'cartridges' — 303a / 303b / 808 / 909 / MST — each a compound control: tap the body to FOCUS that machine's face, tap its corner LED to MUTE it from anywhere (nav=faces, per the device-face paradigm). One shared transport clocks every machine. 303a + 303b are full TB-303 faces on the shared runtime/acid303.h voice (303b sits an octave up = the bass+acid-lead duo): (2) always-live gear-drag knobs CUT/RES/ENV/DEC/ACC, (3) an LCD piano-roll of the 16-step line with NEW living in the screen, (4) the 16-step row, (5) a one-octave keybed. 808/909 (drumkit.h) + MST (mixer) are styled placeholders pending their faces. Knobs: vertical drag = value, pull sideways for a fine gear, double-tap resets.",
    "controls": "Tap a cartridge to focus its machine; tap the cartridge LED to mute/unmute. PLAY runs the shared transport. On a 303 face: turn CUT/RES/ENV/DEC/ACC (drag vertical, pull sideways for fine, double-tap resets), tap step cells to toggle notes, tap a keybed key to set the last step's pitch, NEW = a fresh line."
  },
  "todo": [
    "SOUL: the LCD is a functional piano-roll but has no MASCOT — the slime creature that made the tinyface/facemock mockups sing. Put a mascot bopping to the beat on the screen (the paradigm's 'the screen carries character' §1f); make the piano-roll a FLOW you tap to instead of the default.",
    "wire the soft-keys (SEQ/PAT/FX/SCP) — they're decorative; make them switch display flows (mascot / roll / mix / scope), or trim to the ones that do something.",
    "build the DRUM faces (808/909 via drumkit.h) + the MST mixer face — currently styled placeholders behind their cartridges.",
    "editable accent/slide: the step row only toggles on/off; NEW randomizes acc/slide but you can't set them per-step (lane-strip or long-press).",
    "graduate the gear-drag knob (vertical=value, sideways=fine gear, double-tap=reset) into ui.h's ui_knob so every cart gets it.",
    "optional DEEP page exposing acid303.h's Devil Fish knobs (SLDT/ADEC/ATK/TRK/SUB) — the depth is in the voice, just not surfaced here."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "acid303.h"

// ACID CANDY — a candy-toy acid RACK on the device-face skeleton, 160x100. The
// nav spine is a strip of colour CARTRIDGES you focus machines through (nav=faces);
// the acid VOICE is the shared runtime/acid303.h. This cart is the FACE + the rack.

#define STEPS 16
// knob-feel tunables (dial these while play-testing)
#define KNOB_SWEEP   24.0f   // canvas px for a full 0..1 on a straight vertical drag (smaller = snappier)
#define KNOB_GEAR    22.0f   // sideways px per +1x of fine gear
#define KNOB_GEARMAX 2.0f    // cap so FINE still covers real ground (max sweep = SWEEP*this)

// ── the rack: one cartridge per machine, all on one transport ────────────────
enum { M_303A, M_303B, M_808, M_909, M_MST, M_N };
enum { MK_303, MK_DRUM, MK_MST };
typedef struct { const char *name; int kind; int col, lo; int mute; } Machine;
static Machine mac[M_N] = {
    { "303", MK_303,  CLR_PINK,      CLR_DARK_PURPLE, 0 },   // pink 303 (bass)
    { "303", MK_303,  CLR_ORANGE,    CLR_DARK_ORANGE, 0 },   // orange 303 (lead) — told apart by colour
    { "808",  MK_DRUM, CLR_TRUE_BLUE, CLR_DARK_BLUE,   0 },
    { "909",  MK_DRUM, CLR_YELLOW,    CLR_DARK_ORANGE, 0 },
    { "MST",  MK_MST,  CLR_GREEN,     CLR_DARK_GREEN,  0 },
};
static int face = M_303A;

// the two TB-303 lines (index 0/1 == machine M_303A/M_303B). Pattern lives here.
static Acid ac[2];
static int  on[2][STEPS], pit[2][STEPS], acc[2][STEPS], sld[2][STEPS];
static int  sel[2] = { 0, 0 };

// transport (shared across the rack)
static int   playing = 1, step = 0, laststep = -1;
static float mbop = 0;

static void gen_line(int i) {
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        on[i][s] = rnd_between(0, 99) < 72;
        int p = pool[rnd_between(0, 7)];
        if (rnd_between(0, 99) < 35) p = prev;
        pit[i][s] = prev = p;
        acc[i][s] = rnd_between(0, 99) < 30;
        sld[i][s] = rnd_between(0, 99) < 22;
    }
}

// ── candy widgets ──────────────────────────────────────────────────────────
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// per-knob interaction memory (for double-tap-to-reset), keyed by the value pointer
static struct { void *v; float gval, gt, ltt; } kmeta[8];
static int kmeta_i(void *v) {
    for (int i = 0; i < 8; i++) if (kmeta[i].v == v) return i;
    for (int i = 0; i < 8; i++) if (!kmeta[i].v) { kmeta[i].v = v; kmeta[i].ltt = -9; return i; }
    return 0;
}

// candy rotary. VERTICAL drag = value; PULL SIDEWAYS to shift into a finer gear
// (further out = finer) — one gesture gives quick AND precise. DOUBLE-TAP = reset
// to `def`. While turning, the label shows the live value + a bright value band
// rides the rim; the pointer goes amber in fine gear. Wheel = fine (desktop).
static void knob(float *v, int cx, int cy, int r, const char *label, float def) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    int mi = kmeta_i(v), fine = 0, held = c != 0;
    if (ui_grabbed(v)) { kmeta[mi].gval = *v; kmeta[mi].gt = now(); }
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }   // (re)snap on grab / lens-cross
        int py = c->released ? c->ry : c->cy;
        int px = c->released ? c->rx : c->cx;
        int ox = px - cx; if (ox < 0) ox = -ox;                         // horizontal offset from the knob
        float gear = 1.0f + ox / KNOB_GEAR;                            // 1x over the knob → finer as you pull out
        if (gear > KNOB_GEARMAX) gear = KNOB_GEARMAX;                  // capped so FINE still covers ground
        fine = gear > 1.5f;
        float sweep = KNOB_SWEEP * gear;
        *v = clamp(c->v0 + (c->by - py) / sweep, 0, 1);
        c->v0 = *v; c->by = py;                                        // re-anchor each frame → a gear change never JUMPS the value
    }
    if (ui_released(v)) {                                              // a tap (barely moved, quick) can reset
        float rt = now(), dv = *v - kmeta[mi].gval; if (dv < 0) dv = -dv;
        if (dv < 0.02f && rt - kmeta[mi].gt < 0.25f) {
            if (rt - kmeta[mi].ltt < 0.35f) { *v = def; kmeta[mi].ltt = -9; }   // double-tap → default
            else kmeta[mi].ltt = rt;
        }
    }
    int hot = held || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    if (held) { blend(BLEND_AVG); circfill(cx, cy, r + 1, CLR_WHITE); blend_reset(); }   // grab-glow halo
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    float ang = 150 + *v * 240;
    if (held) ring(cx, cy, r - 3, r, 150, ang, CLR_LIGHT_YELLOW);      // FAT value band — fills as you turn
    circ(cx, cy, r, held ? CLR_WHITE : hot ? CLR_LIGHT_PEACH : CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang),
         fine ? CLR_ORANGE : CLR_WHITE);                              // pointer goes amber in fine gear
    font(FONT_TINY);
    if (held) { int p = (int)(*v * 99 + 0.5f); char b[3] = { (char)('0' + p / 10), (char)('0' + p % 10), 0 };
                plabel(b, cx, cy + r + 1, CLR_DARK_GREEN); }
    else plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}

static int cbtn(unsigned seed, int x, int y, int w, int hh, const char *s, int on2) {
    int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(seed, x, y, w, hh);
    int act = ui_button_core(wid, x, y, w, hh, &foc, &pr, &hot);
    int down = pr || on2;
    rrectfill(x, y, w, hh, 2, down ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(x, y, w, hh, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, down ? CLR_WHITE : CLR_LIGHT_PEACH);
    return act;
}
static void chip(int x, int y, const char *s, int sel2) {
    rrectfill(x, y, 16, 8, 2, sel2 ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    font(FONT_TINY); print(s, x + (16 - text_width(s)) / 2, y + 1, sel2 ? CLR_WHITE : CLR_LIGHT_PEACH);
}

// ── the cartridge nav strip (zone 1) ─────────────────────────────────────────
// each cartridge is a COMPOUND control: left body taps to FOCUS the face, the
// right LED taps to MUTE the machine (from any face). Two non-overlapping
// sub-buttons, so ui.h's visual-hit-wins routes touch cleanly.
static void cartridge(int m) {
    int x = 19 + m * 25, y = 3, foc = (m == face), live = !mac[m].mute;  // pitch 25; y3 = same row as play/home
    int prf = 0, hotf = 0, fof = 0, prm = 0, hotm = 0, fom = 0;
    void *wf = ui_wid_hash(0x70u + m, x, y, 16, 10);
    void *wm = ui_wid_hash(0x80u + m, x + 16, y, 8, 10);
    if (ui_button_core(wf, x, y, 16, 10, &fof, &prf, &hotf)) face = m;
    if (ui_button_core(wm, x + 16, y, 8, 10, &fom, &prm, &hotm)) mac[m].mute = !mac[m].mute;

    rrectfill(x, y, 24, 10, 2, foc ? mac[m].col : mac[m].lo);
    if (foc) { blend(BLEND_AVG); line(x + 2, y + 1, x + 19, y + 1, CLR_WHITE); blend_reset(); }   // top sheen
    rrect(x, y, 24, 10, 2, (foc || hotf) ? CLR_WHITE : CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(mac[m].name, x + (16 - text_width(mac[m].name)) / 2, y + 2, foc ? CLR_BROWNISH_BLACK : mac[m].col);
    int lx = x + 20, ly = y + 5;                                      // mute LED
    circfill(lx, ly, 2, live ? (foc ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_PURPLE);
    circ(lx, ly, 2, CLR_BROWNISH_BLACK);
    if (!live) line(lx - 2, ly - 2, lx + 2, ly + 2, CLR_RED);         // muted = red slash
}

static void navspine(void) {
    // transport (shared)
    int px = 5, py = 3, pw = 14, ph = 10, pr = 0, hot = 0, fo = 0;    // play, in from the left bezel
    void *w = ui_wid_hash(0x01u, px, py, pw, ph);
    if (ui_button_core(w, px, py, pw, ph, &fo, &pr, &hot)) { playing = !playing; laststep = -1; }
    rrectfill(px, py, pw, ph, 2, playing ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect(px, py, pw, ph, 2, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (playing) { rectfill(px + 4, py + 3, 2, 4, CLR_WHITE); rectfill(px + 8, py + 3, 2, 4, CLR_WHITE); }
    else trifill(px + 5, py + 3, px + 5, py + 7, px + 10, py + 5, CLR_WHITE);
    for (int m = 0; m < M_N; m++) cartridge(m);

    // HOME (meta) — reserved space only; the app shell owns the real leave-cart gesture
    int hx = 143, hy = 3, hw = 12, hh = 10, hpr = 0, hhot = 0, hfo = 0;   // home, in from the right bezel
    void *wh = ui_wid_hash(0x03u, hx, hy, hw, hh);
    ui_button_core(wh, hx, hy, hw, hh, &hfo, &hpr, &hhot);            // registered but unwired
    rrectfill(hx, hy, hw, hh, 2, CLR_DARK_BROWN);
    rrect(hx, hy, hw, hh, 2, hhot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    int cxh = hx + hw / 2;                                            // little house glyph
    trifill(cxh - 3, hy + 5, cxh + 3, hy + 5, cxh, hy + 2, CLR_LIGHT_PEACH);
    rectfill(cxh - 2, hy + 5, 5, 3, CLR_LIGHT_PEACH);
}

// ── a 303 face (zones 2–5) ───────────────────────────────────────────────────
static void draw_303(int i) {
    Acid *a = &ac[i];
    // ② always-live gear-drag knobs, bound to the shared voice params
    knob(&a->p[ACID_CUT], 20, 26, 6, "CUT", 0.55f); knob(&a->p[ACID_RES], 48, 26, 6, "RES", 0.70f);
    knob(&a->p[ACID_ENV], 76, 26, 6, "ENV", 0.55f); knob(&a->p[ACID_DEC], 104, 26, 6, "DEC", 0.45f);
    knob(&a->p[ACID_ACC], 132, 26, 6, "ACC", 0.55f);

    // ③ display — the piano-roll + playhead, flanked by (still-decorative) soft-keys
    chip(6, 38, "SEQ", 1); chip(6, 47, "PAT", 0);
    chip(138, 38, "FX", 0); chip(138, 47, "SCP", 0);
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    font(FONT_TINY); print("132", 29, 40, CLR_MEDIUM_GREEN);          // bpm lives in the screen
    if (cbtn(0x02u, 113, 39, 18, 7, "NEW", 0)) gen_line(i);           // ...and so does NEW
    for (int s = 0; s < STEPS; s++) {
        int cx = 29 + s * 6;
        if (s == step && playing) { blend(BLEND_AVG); rectfill(cx - 1, 40, 5, 18, CLR_MEDIUM_GREEN); blend_reset(); }
        if (!on[i][s]) continue;
        int y = 56 - pit[i][s];
        rectfill(cx, y, 4, 3, acc[i][s] ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
        if (sld[i][s]) line(cx + 4, y + 1, cx + 6, y + 1, CLR_MEDIUM_GREEN);
    }

    // ④ step row — tap toggles
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, pr = 0, hot = 0, foc = 0;
        void *wid = ui_wid_hash(0x30u + s, x, 64, 8, 9);
        if (ui_button_core(wid, x, 64, 8, 9, &foc, &pr, &hot)) { on[i][s] = !on[i][s]; sel[i] = s; if (on[i][s]) mbop = 1; }
        int fc = on[i][s] ? (acc[i][s] ? CLR_ORANGE : CLR_LIME_GREEN) : CLR_DARK_BROWN;
        if (s == step && playing) fc = CLR_WHITE;
        rrectfill(x, 64, 8, 9, 1, fc);
        if (s == sel[i]) rrect(x - 1, 63, 10, 11, 1, CLR_LIGHT_YELLOW);
        rrect(x, 64, 8, 9, 1, CLR_BROWNISH_BLACK);
    }

    // ⑤ keybed — tap a key to set the selected step's pitch + audition it
    int kb = 6, ky = 77, kh = 16;
    static const int isblack[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
    int wi = 0;
    for (int n = 0; n < 12; n++) if (!isblack[n]) {
        int x = kb + wi * 21; wi++;
        int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x50u + n, x, ky, 20, kh);
        if (ui_button_core(wid, x, ky, 20, kh, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + n, 0, 0); }
        int lit = pit[i][sel[i]] == n && on[i][sel[i]];
        rrectfill(x, ky, 20, kh, 2, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
        rrect(x, ky, 20, kh, 2, CLR_BROWNISH_BLACK);
    }
    wi = 0;
    for (int n = 0; n < 12; n++) {
        if (isblack[n]) {
            int x = kb + wi * 21 - 6;
            int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x60u + n, x, ky, 12, 9);
            if (ui_button_core(wid, x, ky, 12, 9, &foc, &pr, &hot)) { pit[i][sel[i]] = n; on[i][sel[i]] = 1; mbop = 1; acid_note(a, a->base + n, 0, 0); }
            int lit = pit[i][sel[i]] == n && on[i][sel[i]];
            rrectfill(x, ky, 12, 9, 1, lit ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
            rrect(x, ky, 12, 9, 1, CLR_BLACK);
        } else wi++;
    }
}

// ── placeholder faces (drum kits + master) — styled, pending their builds ─────
static void placeholder(int m) {
    rrectfill(6, 20, 148, 71, 4, CLR_BROWNISH_BLACK);
    rrectfill(9, 23, 142, 65, 3, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 24; y < 87; y += 2) line(9, y, 150, y, CLR_BROWNISH_BLACK); blend_reset();
    const char *sub = mac[m].kind == MK_DRUM ? "DRUM FACE" : "MIX / MASTER";
    font(FONT_NORMAL); plabel(mac[m].name, 80, 42, mac[m].col);
    font(FONT_SMALL); plabel(sub, 80, 58, CLR_MEDIUM_GREEN);
    font(FONT_TINY);  plabel("coming soon", 80, 70, CLR_DARK_GREEN);
}

void init(void) {
    bpm(132);
    acid_init(&ac[0], 6, -1);                                          // 303a — the bass line
    acid_init(&ac[1], 7, -1);                                          // 303b — an octave up = the acid lead
    ac[1].base = 48;
    for (int i = 0; i < 2; i++) {
        ac[i].p[ACID_CUT] = 0.55f; ac[i].p[ACID_RES] = 0.70f; ac[i].p[ACID_ENV] = 0.55f;
        ac[i].p[ACID_DEC] = 0.45f; ac[i].p[ACID_ACC] = 0.55f;
        acid_define(&ac[i]);
        gen_line(i);
    }
}

void update(void) {
    if (mbop > 0) mbop -= 0.08f;
    for (int i = 0; i < 2; i++) acid_ride(&ac[i]);                     // ride cutoff/reso live on both lines
    if (playing) {
        float stepf = now() * (132 / 60.0f * 4);                       // 16th notes at 132 bpm
        step = ((int)stepf) % STEPS;
        float frac = stepf - (int)stepf;
        if (step != laststep) {
            laststep = step;
            for (int i = 0; i < 2; i++) {
                if (!mac[i].mute && on[i][step]) { acid_note(&ac[i], ac[i].base + pit[i][step], acc[i][step], sld[i][step]); mbop = 1; }
                else acid_off(&ac[i]);
            }
        } else for (int i = 0; i < 2; i++) acid_gate(&ac[i], frac, 0.0f, 0);   // staccato release
    } else for (int i = 0; i < 2; i++) acid_off(&ac[i]);
#ifdef DE_TRACE
    watch("face", "%d", face); watch("step", "%d", step); watch("cut", "%d", acid_cut_hz(&ac[0]));
    watch("mute0", "%d", mac[0].mute); watch("mute1", "%d", mac[1].mute);
#endif
}

void draw(void) {
    cls(CLR_DARK_PURPLE);
    rrectfill(0, 0, 160, 100, 7, CLR_INDIGO);
    rrectfill(3, 2, 154, 96, 5, CLR_LIGHT_PEACH);                     // 2px purple bezel top & bottom
    blend(BLEND_AVG); line(7, 2, 152, 2, CLR_WHITE); blend_reset();
    ui_begin();
    font(FONT_SMALL);

    navspine();
    if (mac[face].kind == MK_303) draw_303(face);
    else                          placeholder(face);

    font(FONT_NORMAL);
    ui_end();
}
