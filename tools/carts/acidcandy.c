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
    "summary": "A functioning TB-303 acid machine on the device-face skeleton, at half resolution (160x100 x4) in the candy-toy skin — the first machine of a candy acid rack. The acid VOICE is the shared runtime/acid303.h, not a private copy.",
    "detail": "Increment 1 of a candy acidrack, built to device-face-paradigm.md's acidface sketch (five zones) at 160x100. The sound is runtime/acid303.h (the extracted TB-303 voice shared with tb303/acidrack — FILTER_DIODE held voice, non-retriggering slide, accent kick, two-decay, soft attack, tracking, sub-osc), so it can't drift from the others. (1) nav: play/stop, NEW line, bpm, heart. (2) always-live knobs CUT/RES/ENV/DEC/ACC bound straight to the shared voice params. (3) display: the piano-roll of the 16-step line with a running playhead, flanked by soft-keys. (4) the 16-step row: tap toggles a step (accent shows orange). (5) a one-octave keybed: tap a key to set the last-touched step's pitch and audition it. Press NEW for a fresh minor-pentatonic acid walk.",
    "controls": "PLAY runs it. Turn CUT/RES/ENV/DEC/ACC (drag or wheel) — CUT+RES live is the acid squelch. Tap step cells to toggle notes. Tap a keybed key to set the last step's pitch + hear it. NEW = a fresh random line."
  },
  "todo": [
    "SOUL: the display is a functional piano-roll but has no MASCOT — the slime creature that made the tinyface/facemock mockups sing. Put a mascot bopping to the beat on the screen (the paradigm's 'the screen carries character' §1f); make the piano-roll a FLOW you tap to instead of the default.",
    "wire the soft-keys (SEQ/PAT/FX/SCP) — they're decorative; make them switch display flows (mascot / roll / mix / scope), or trim to the ones that do something.",
    "make it a RACK: tab in a DRUM face (drumkit.h) + optional 2nd bass, nav tabs = real FACES you focus between (paradigm-correct: nav=faces, soft-keys=flows), all sharing acid303.h/drumkit.h + one transport.",
    "editable accent/slide: the step row only toggles on/off; NEW randomizes acc/slide but you can't set them per-step (lane-strip or long-press).",
    "optional DEEP page exposing acid303.h's Devil Fish knobs (SLDT/ADEC/ATK/TRK/SUB) — the depth is in the voice, just not surfaced here."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "acid303.h"

// ACID CANDY — a functioning TB-303 on the device-face skeleton, 160x100, candy
// skin. The acid VOICE is the shared runtime/acid303.h; this cart is the FACE.

#define SLOT 6
#define STEPS 16
static Acid  a;                                            // the shared acid voice
static int   on[STEPS], pit[STEPS], acc[STEPS], sld[STEPS];   // the line (pattern lives in the cart)
static int   plen = STEPS, sel = 0;
static int   playing = 1, step = 0, laststep = -1;
static float mbop = 0;

static void gen_line(void) {
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        on[s] = rnd_between(0, 99) < 72;
        int p = pool[rnd_between(0, 7)];
        if (rnd_between(0, 99) < 35) p = prev;
        pit[s] = prev = p;
        acc[s] = rnd_between(0, 99) < 30;
        sld[s] = rnd_between(0, 99) < 22;
    }
}

// ── candy widgets ──────────────────────────────────────────────────────────
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }
static void knob(float *v, int cx, int cy, int r, const char *label) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
             int py = c->released ? c->ry : c->cy; *v = clamp(c->v0 + (c->by - py) / 60.0f, 0, 1); }
    int hot = c != 0 || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.04f, 0, 1);
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    circ(cx, cy, r, hot ? CLR_WHITE : CLR_BROWNISH_BLACK);
    float ang = 150 + *v * 240;
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
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

void init(void) {
    bpm(132);
    acid_init(&a, SLOT, -1);                               // no sub-osc on the tiny candy
    a.p[ACID_CUT] = 0.55f; a.p[ACID_RES] = 0.70f; a.p[ACID_ENV] = 0.55f;
    a.p[ACID_DEC] = 0.45f; a.p[ACID_ACC] = 0.55f;
    acid_define(&a);
    gen_line();
}

void update(void) {
    if (mbop > 0) mbop -= 0.08f;
    acid_ride(&a);                                         // ride cutoff/reso/drive live
    if (playing) {
        float stepf = now() * (132 / 60.0f * 4);           // 16th notes at 132 bpm
        step = ((int)stepf) % plen;
        float frac = stepf - (int)stepf;
        if (step != laststep) {
            laststep = step;
            if (on[step]) { acid_note(&a, a.base + pit[step], acc[step], sld[step]); mbop = 1; }
            else          acid_off(&a);
        } else acid_gate(&a, frac, 0.0f, 0);               // staccato release (straight, no swing)
    } else acid_off(&a);
#ifdef DE_TRACE
    watch("step", "%d", step); watch("cut", "%d", acid_cut_hz(&a)); watch("playing", "%d", playing);
#endif
}

void draw(void) {
    cls(CLR_DARK_PURPLE);
    rrectfill(0, 0, 160, 100, 7, CLR_INDIGO);
    rrectfill(3, 3, 154, 94, 5, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); line(7, 4, 152, 4, CLR_WHITE); blend_reset();
    ui_begin();
    font(FONT_SMALL);

    // ① nav spine
    if (cbtn(0x01, 6, 6, 13, 9, playing ? "STOP" : "PLAY", playing)) { playing = !playing; laststep = -1; }
    font(FONT_TINY); print("ACID", 22, 7, CLR_RED); print("candy", 22, 12, CLR_DARK_BROWN);
    print("132bpm", 118, 12, CLR_DARK_GREEN);
    if (cbtn(0x02, 118, 5, 24, 7, "NEW", 0)) gen_line();
    trifill(147, 8, 153, 8, 150, 12, CLR_RED); circfill(148, 8, 1, CLR_RED); circfill(152, 8, 1, CLR_RED);   // heart

    // ② always-live knobs — bound straight to the shared voice params
    knob(&a.p[ACID_CUT], 20, 26, 6, "CUT"); knob(&a.p[ACID_RES], 48, 26, 6, "RES"); knob(&a.p[ACID_ENV], 76, 26, 6, "ENV");
    knob(&a.p[ACID_DEC], 104, 26, 6, "DEC"); knob(&a.p[ACID_ACC], 132, 26, 6, "ACC");

    // ③ display — the piano-roll of the line + playhead, flanked by soft-keys
    chip(6, 38, "SEQ", 1); chip(6, 47, "PAT", 0);
    chip(138, 38, "FX", 0); chip(138, 47, "SCP", 0);
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    for (int s = 0; s < plen; s++) {
        int cx = 29 + s * 6;
        if (s == step && playing) { blend(BLEND_AVG); rectfill(cx - 1, 40, 5, 18, CLR_MEDIUM_GREEN); blend_reset(); }
        if (!on[s]) continue;
        int y = 56 - pit[s];
        rectfill(cx, y, 4, 3, acc[s] ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
        if (sld[s]) line(cx + 4, y + 1, cx + 6, y + 1, CLR_MEDIUM_GREEN);
    }

    // ④ step row — tap toggles
    for (int s = 0; s < STEPS; s++) {
        int x = 6 + s * 9, pr = 0, hot = 0, foc = 0;
        void *wid = ui_wid_hash(0x30 + s, x, 64, 8, 9);
        if (ui_button_core(wid, x, 64, 8, 9, &foc, &pr, &hot)) { on[s] = !on[s]; sel = s; if (on[s]) mbop = 1; }
        int face = on[s] ? (acc[s] ? CLR_ORANGE : CLR_LIME_GREEN) : CLR_DARK_BROWN;
        if (s == step && playing) face = CLR_WHITE;
        rrectfill(x, 64, 8, 9, 1, face);
        if (s == sel) rrect(x - 1, 63, 10, 11, 1, CLR_LIGHT_YELLOW);
        rrect(x, 64, 8, 9, 1, CLR_BROWNISH_BLACK);
    }

    // ⑤ keybed — tap a key to set the selected step's pitch + audition it
    {
        int kb = 6, ky = 77, kh = 16;
        static const int isblack[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
        int wi = 0;
        for (int n = 0; n < 12; n++) if (!isblack[n]) {
            int x = kb + wi * 21; wi++;
            int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x50 + n, x, ky, 20, kh);
            if (ui_button_core(wid, x, ky, 20, kh, &foc, &pr, &hot)) { pit[sel] = n; on[sel] = 1; mbop = 1; acid_note(&a, a.base + n, 0, 0); }
            int lit = pit[sel] == n && on[sel];
            rrectfill(x, ky, 20, kh, 2, lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
            rrect(x, ky, 20, kh, 2, CLR_BROWNISH_BLACK);
        }
        wi = 0;
        for (int n = 0; n < 12; n++) {
            if (isblack[n]) {
                int x = kb + wi * 21 - 6;
                int pr = 0, hot = 0, foc = 0; void *wid = ui_wid_hash(0x60 + n, x, ky, 12, 9);
                if (ui_button_core(wid, x, ky, 12, 9, &foc, &pr, &hot)) { pit[sel] = n; on[sel] = 1; mbop = 1; acid_note(&a, a.base + n, 0, 0); }
                int lit = pit[sel] == n && on[sel];
                rrectfill(x, ky, 12, 9, 1, lit ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
                rrect(x, ky, 12, 9, 1, CLR_BLACK);
            } else wi++;
        }
    }

    font(FONT_NORMAL);
    ui_end();
}
