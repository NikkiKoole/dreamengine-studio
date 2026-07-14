/* de:meta
{
  "slug": "grooveface",
  "collection": ["device-face"],
  "title": "groove face",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [],
  "description": {
    "summary": "The first real DEVICE FACE built from the control vocabulary: a playable drum-machine laid onto the five-zone skeleton — transport+VU spine, always-live knobs, a flow-switched display, a 16-step grid, and drum pads — cycling all three skins (TACTILE / FLAT / PURE).",
    "detail": "Proves the toolkit in situ (device-face-paradigm.md x control-vocabulary.md). Zone 1 NAV SPINE: play/stop, pattern name, a 7-seg BPM, a VU that bumps on every hit. Zone 2 KNOBS: TUNE + LEVEL retarget to the SELECTED track (parameter-lock idea), SWING + REVERB are global (reverb applied set-and-hold, only on change). Zone 3 DISPLAY: soft-keys pick the flow — GRID (the whole 4x16 pattern with a running playhead + the selected row lit) or MIX (a fader per track). Zone 4 STEP GRID: the 16 cells for the selected track; tap to toggle, the playhead runs. Zone 5 PADS: KICK/SNARE/HAT/CLAP — tap to play live AND select the track (retargeting zones 2+4), and each pad's corner LED lights when the SEQUENCER fires it (external vs finger). Real sound via drumkit.h (DK_ELECTRO). One instrument, one honest core.",
    "controls": "Press PLAY (spine) to run the sequencer. Tap a PAD to play that drum and select its track; the knobs + step row then edit THAT track. Tap step cells to write the pattern. Turn TUNE/LEVEL for the selected track, SWING/REVERB globally. Tap GRID/MIX to switch the display flow. SKIN button (top-right) or B cycles the three looks."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "drumkit.h"

// GROOVE FACE — a drum machine on the five-zone device-face skeleton, drawn with
// the control vocabulary and cycling the three skins. The renderers are copied
// compact from the study carts (rotaries/faders/pads/switches/indicators); this
// cart is what SPECS the eventual shared skin.h / ui_skin() extraction.

#define A0 150.0f
#define SW 240.0f
enum { SK_TACTILE, SK_FLAT, SK_PURE };
static int skin = SK_TACTILE;
static const char *SKIN_NAME[3] = { "TACTILE", "FLAT", "PURE" };
#define bevel (skin == SK_TACTILE)

static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// ── widget renderers (3-skin) ───────────────────────────────────────────────
static void faceplate(int x, int y, int w, int h) {
    if (skin == SK_PURE) { rect(x, y, w, h, CLR_MEDIUM_GREY); return; }
    rrectfill(x, y, w, h, 4, CLR_DARK_BROWN);
    if (skin == SK_TACTILE) { line(x + 3, y + 1, x + w - 4, y + 1, CLR_BROWN);
        blend(BLEND_AVG); line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK); blend_reset(); }
}
static void led(int cx, int cy, int r, int on, int col) {
    if (skin == SK_PURE) { if (on) circfill(cx, cy, r, col); arc(cx, cy, r, 0, 360, on ? col : CLR_MEDIUM_GREY); return; }
    if (on) { blend(BLEND_AVG); circfill(cx, cy, r + 1, col); blend_reset(); }
    circfill(cx, cy, r, on ? col : CLR_DARKER_GREY);
    if (on && bevel) pset(cx - 1, cy - 1, CLR_WHITE);
    arc(cx, cy, r, 0, 360, CLR_BLACK);
}
// a knob: neutral cap, accent value ring, rim-groove pointer
static void pot(float *v, int cx, int cy, int r, int accent, const char *label) {
    ui_reg(v, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
             int py = c->released ? c->ry : c->cy; *v = clamp(c->v0 + (c->by - py) / 60.0f, 0, 1); }
    int hot = c != 0 || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.03f, 0, 1);
    float av = A0 + *v * SW;
    int ri = skin == SK_PURE ? r + 1 : r + 3, ro = skin == SK_PURE ? r + 3 : r + 5;
    ring(cx, cy, ri, ro, A0, A0 + SW, CLR_DARKER_GREY);
    ring(cx, cy, ri, ro, A0, av, accent);
    if (skin == SK_PURE) { circfill(cx, cy, r, CLR_BROWNISH_BLACK); arc(cx, cy, r, 0, 360, hot ? CLR_WHITE : CLR_LIGHT_GREY); }
    else {
        circfill(cx, cy, r, CLR_DARK_GREY);
        if (bevel) { ring(cx, cy, r - 2, r - 1, 165, 285, CLR_LIGHT_GREY); ring(cx, cy, r - 2, r - 1, -15, 105, CLR_BROWNISH_BLACK); }
        arc(cx, cy, r, 0, 360, hot ? CLR_WHITE : CLR_BLACK);
    }
    int inner = (int)(r * 0.45f);
    line(cx + (int)dx(inner, av), cy + (int)dy(inner, av), cx + (int)dx(r - 2, av), cy + (int)dy(r - 2, av), CLR_WHITE);
    if (label) plabel(label, cx, cy + r + (skin == SK_PURE ? 4 : 5), CLR_LIGHT_GREY);
}
// a vertical fader (absolute)
static void vfader(float *v, int cx, int ty, int th, int col, const char *label) {
    int hw = 18; ui_reg(v, cx - hw / 2, ty, hw, th, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { int cy = c->released ? c->ry : c->cy; *v = clamp((float)(ty + th - cy) / th, 0, 1); }
    int hot = c != 0 || ui_hover(cx - hw / 2, ty, hw, th);
    int ch = 7, capy = ty + (int)((1 - *v) * (th - ch)) + ch / 2;
    if (skin == SK_PURE) { rect(cx - 3, ty, 7, th, hot ? CLR_WHITE : CLR_MEDIUM_GREY); rectfill(cx - 1, capy, 3, ty + th - capy, col); }
    else { rrectfill(cx - 2, ty, 5, th, 2, CLR_BROWNISH_BLACK); rectfill(cx - 1, capy, 3, ty + th - capy - 1, col);
           rrectfill(cx - 7, capy - ch / 2, 14, ch, 2, CLR_MEDIUM_GREY);
           if (bevel) { line(cx - 6, capy - ch / 2 + 1, cx + 5, capy - ch / 2 + 1, CLR_LIGHT_GREY); }
           line(cx - 5, capy, cx + 4, capy, CLR_DARKER_GREY);
           rrect(cx - 7, capy - ch / 2, 14, ch, 2, hot ? CLR_WHITE : CLR_BLACK); }
    if (label) plabel(label, cx, ty + th + 2, CLR_LIGHT_GREY);
}
// a drum pad: candy face, velocity flash (env), external-trigger corner LED (trig<0 = none)
static void pad(unsigned seed, int x, int y, int w, int h, int face, float env, float trig,
                const char *label, int sel, int *hit) {
    int pr = 0, hot = 0;
    void *wid = ui_wid_hash(seed, x, y, w, h); int foc = 0;
    if (ui_button_core(wid, x, y, w, h, &foc, &pr, &hot)) *hit = 1;
    float e = pr ? 1.0f : env;
    if (skin == SK_PURE) {
        int lit = e > 0.1f;
        rectfill(x, y, w, h, lit ? face : CLR_BROWNISH_BLACK);
        rect(x, y, w, h, hot || sel ? CLR_WHITE : (lit ? CLR_BROWNISH_BLACK : face));
        if (label) plabel(label, x + w / 2, y + h - 8, lit ? CLR_BROWNISH_BLACK : face);
    } else {
        if (bevel && e > 0.05f) { int g = (int)(e * 3); blend(BLEND_AVG); rrectfill(x - g, y - g, w + 2 * g, h + 2 * g, 4, face); blend_reset(); }
        rrectfill(x, y, w, h, 3, face);
        if (e > 0.4f) { blend(BLEND_AVG); rrectfill(x + 3, y + 3, w - 6, h - 6, 2, CLR_WHITE); blend_reset(); }
        if (bevel) { line(x + 3, y + 1, x + w - 4, y + 1, CLR_WHITE); line(x + 1, y + 3, x + 1, y + h - 4, CLR_LIGHT_PEACH);
            blend(BLEND_AVG); line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK); line(x + w - 2, y + 3, x + w - 2, y + h - 4, CLR_BLACK); blend_reset(); }
        rrect(x, y, w, h, 3, hot || sel ? CLR_WHITE : CLR_BLACK);
        if (sel) rrect(x - 1, y - 1, w + 2, h + 2, 3, CLR_LIGHT_YELLOW);
        if (label) plabel(label, x + w / 2, y + h - 8, CLR_BROWNISH_BLACK);
    }
    if (trig >= 0) { int lx = x + w - 6, ly = y + 4;
        if (skin == SK_PURE) { if (trig > 0.02f) rectfill(lx - 1, ly - 1, 3, 3, CLR_LIGHT_YELLOW); else rect(lx - 1, ly - 1, 3, 3, CLR_DARKER_GREY); }
        else { circfill(lx, ly, 2, trig > 0.02f ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK); circ(lx, ly, 2, CLR_BLACK); } }
}
// a plain button (transport / soft-key), Win95 chisel per skin
static int dbtn(unsigned seed, int x, int y, int w, int h, const char *label, int on) {
    int pr = 0, hot = 0, act = 0; void *wid = ui_wid_hash(seed, x, y, w, h); int foc = 0;
    act = ui_button_core(wid, x, y, w, h, &foc, &pr, &hot);
    int down = pr || on;
    if (skin == SK_PURE) { rectfill(x, y, w, h, down ? CLR_TRUE_BLUE : CLR_BROWNISH_BLACK); rect(x, y, w, h, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
        print(label, x + (w - text_width(label)) / 2, y + (h - 6) / 2, down ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY); }
    else { rectfill(x, y, w, h, down ? CLR_TRUE_BLUE : CLR_DARK_GREY);
        if (bevel) { int a = down ? CLR_BLACK : CLR_WHITE, b = down ? CLR_WHITE : CLR_BLACK;
            line(x, y, x + w - 1, y, a); line(x, y, x, y + h - 1, a); line(x, y + h - 1, x + w - 1, y + h - 1, b); line(x + w - 1, y, x + w - 1, y + h - 1, b); }
        else rect(x, y, w, h, CLR_BLACK);
        print(label, x + (w - text_width(label)) / 2, y + (h - 6) / 2, down ? CLR_WHITE : CLR_LIGHT_GREY); }
    return act;
}
// a step cell
static void stepcell(unsigned seed, int x, int y, int s, int on, int accent, int playing, int *tog) {
    int pr = 0, hot = 0; void *wid = ui_wid_hash(seed, x, y, s, s); int foc = 0;
    if (ui_button_core(wid, x, y, s, s, &foc, &pr, &hot)) *tog = 1;
    int face = on ? (accent ? CLR_ORANGE : CLR_LIME_GREEN) : CLR_DARKER_GREY;
    if (playing) face = CLR_WHITE;
    if (skin == SK_PURE) { rectfill(x, y, s, s, on || playing ? face : CLR_BROWNISH_BLACK); rect(x, y, s, s, hot ? CLR_WHITE : (on ? face : CLR_MEDIUM_GREY)); }
    else { rrectfill(x, y, s, s, 2, face); if (bevel && (on || playing)) { blend(BLEND_AVG); line(x + 1, y + 1, x + s - 2, y + 1, CLR_WHITE); blend_reset(); }
           rrect(x, y, s, s, 2, hot ? CLR_WHITE : CLR_BLACK); }
}
// tiny 7-seg-ish number via the font (crisp, skin-agnostic)
static void readout(const char *s, int x, int y) { font(FONT_NORMAL); print(s, x, y, CLR_LIME_GREEN); font(FONT_SMALL); }

// ── the drum-machine state ───────────────────────────────────────────────
#define NTRK 4
static int trkrole[NTRK] = { DK_KICK, DK_SNARE, DK_HHC, DK_CLAP };
static int pat[NTRK][16] = {
    { 2,0,0,0, 1,0,0,0, 2,0,0,0, 1,0,1,0 },
    { 0,0,0,0, 2,0,0,0, 0,0,0,0, 2,0,0,1 },
    { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,2,0 },
    { 0,0,0,0, 0,0,0,0, 0,0,2,0, 0,0,0,0 },
};
static float tune[NTRK] = { 0.5f, 0.5f, 0.5f, 0.5f };     // per-track pitch (parameter-lock)
static float lvl[NTRK]  = { 0.9f, 0.8f, 0.6f, 0.7f };     // per-track level
static float swing = 0.15f, rev = 0.2f, last_rev = -1;    // global
static float penv[NTRK], pled[NTRK];                      // pad flash + external-trigger LED
static int   sel = 0, playing = 1, step = 0, laststep = -1;
static float vu = 0;
static int   flow = 0;                                    // 0 = GRID, 1 = MIX

static void fire(int trk, int accent, int delay) {
    int midi = DK_ROLE_MIDI[trkrole[trk]] + (int)((tune[trk] - 0.5f) * 24);
    int vel = 1 + (int)(lvl[trk] * (accent ? 6.99f : 4.99f));
    dk_fire_at(delay, trkrole[trk], midi, vel);
    penv[trk] = accent ? 1.0f : 0.7f;
    if (vu < penv[trk]) vu = penv[trk];
}

void init(void) { dk_use(&DK_ELECTRO, 20); bpm(124); }

void update(void) {
    for (int i = 0; i < NTRK; i++) { penv[i] -= 0.06f; if (penv[i] < 0) penv[i] = 0;
                                     pled[i] -= 0.09f; if (pled[i] < 0) pled[i] = 0; }
    vu -= 0.03f; if (vu < 0) vu = 0;
    if (rev != last_rev) { reverb(rev * 0.6f, 0.4f); last_rev = rev; }   // set-and-hold, only on change
    if (playing) {
        step = ((int)(now() * (124 / 60.0f * 4))) % 16;                 // 16th notes at 124bpm
        if (step != laststep) {
            laststep = step;
            int stepms = (int)(60000.0f / 124 / 4);
            for (int p = 0; p < NTRK; p++) if (pat[p][step]) {
                int dly = (step & 1) ? (int)(swing * 0.5f * stepms) : 0; // swing pushes odd 16ths
                fire(p, pat[p][step] == 2, dly);
                pled[p] = 1.0f;                                          // external-trigger LED
            }
        }
    } else laststep = -1;
#ifdef DE_TRACE
    watch("step", "%d", step); watch("sel", "%d", sel); watch("playing", "%d", playing);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    font(FONT_SMALL);

    // ① NAV SPINE — transport · pattern · bpm · VU · skin
    faceplate(4, 3, 312, 22);
    if (dbtn(0x01u, 10, 6, 30, 16, playing ? "STOP" : "PLAY", playing)) playing = !playing;
    print("PATTERN A", 48, 7, CLR_LIGHT_PEACH);
    print("track:", 48, 15, CLR_DARK_GREY); print(dk_role_name(trkrole[sel]), 74, 15, CLR_MAUVE);
    readout("124", 150, 8); print("BPM", 150, 17, CLR_DARK_GREEN);
    for (int i = 0; i < 10; i++) { int on = vu > i / 10.0f;               // VU
        rectfill(186 + i * 5, 9, 4, 10, on ? (i > 7 ? CLR_RED : i > 5 ? CLR_YELLOW : CLR_GREEN) : CLR_DARKER_GREY); }
    if (dbtn(0x02u, 254, 6, 58, 16, SKIN_NAME[skin], 0) || keyp('B')) skin = (skin + 1) % 3;

    // ② KNOBS — TUNE/LEVEL retarget to the selected track; SWING/REVERB global
    faceplate(4, 28, 312, 60);
    pot(&tune[sel], 44, 52, 14, CLR_TRUE_BLUE, "TUNE");
    pot(&lvl[sel], 114, 52, 14, CLR_GREEN, "LEVEL");
    pot(&swing, 200, 52, 14, CLR_MAUVE, "SWING");
    pot(&rev, 272, 52, 14, CLR_ORANGE, "REVERB");

    // ③ DISPLAY — soft-keys pick the flow (GRID / MIX)
    faceplate(4, 92, 312, 108);
    if (dbtn(0x10u, 10, 100, 32, 14, "GRID", flow == 0)) flow = 0;
    if (dbtn(0x11u, 10, 118, 32, 14, "MIX", flow == 1)) flow = 1;
    int sx = 48, sy = 100, sw = 262, sh = 92;
    if (skin == SK_PURE) rect(sx, sy, sw, sh, CLR_MEDIUM_GREY); else { rrectfill(sx, sy, sw, sh, 3, CLR_BROWNISH_BLACK); rrect(sx, sy, sw, sh, 3, CLR_DARKER_GREY); }
    if (flow == 0) {                                                     // GRID: the whole 4x16 pattern
        int gx = sx + 8, gy = sy + 14, cw = 15, cellh = 14;
        for (int t = 0; t < NTRK; t++) {
            print(dk_role_name(trkrole[t]), sx + 4, gy + t * (cellh + 2) - 8 + 4, t == sel ? CLR_WHITE : CLR_DARK_GREY);
            for (int s = 0; s < 16; s++) {
                int cx = gx + s * cw, cy = gy + t * (cellh + 2);
                int on = pat[t][s], here = playing && s == step;
                int col = here ? CLR_WHITE : on ? (on == 2 ? CLR_ORANGE : CLR_MEDIUM_GREEN) : CLR_DARKER_GREY;
                rectfill(cx, cy, cw - 2, cellh - 4, col);
            }
        }
        print(">", gx + step * cw + (cw - 2) / 2 - 1, gy - 6, CLR_LIGHT_YELLOW);   // playhead marker
    } else {                                                            // MIX: a fader per track
        const char *mn[NTRK] = { "KICK", "SNARE", "HAT", "CLAP" };
        int mc[NTRK] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_TRUE_BLUE };
        for (int t = 0; t < NTRK; t++) vfader(&lvl[t], sx + 40 + t * 60, sy + 8, 66, mc[t], mn[t]);
    }

    // ④ STEP GRID — the 16 cells for the SELECTED track (mode = which pad is selected)
    faceplate(4, 204, 312, 40);
    print(dk_role_name(trkrole[sel]), 10, 208, CLR_MAUVE);
    {
        int x0 = 10, cw = 19, y = 220, s = 16;
        for (int i = 0; i < 16; i++) {
            int tog = 0;
            stepcell(0x30u + i, x0 + i * cw, y, s, pat[sel][i] != 0, pat[sel][i] == 2, playing && i == step, &tog);
            if (tog) pat[sel][i] = pat[sel][i] ? 0 : 1;                  // tap toggles; (accent = future)
        }
    }

    // ⑤ PADS — play live + select the track; corner LED = the sequencer fired it
    faceplate(4, 248, 312, 148);
    {
        int pc[NTRK] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_TRUE_BLUE };
        int px[NTRK] = { 12, 164, 12, 164 }, py[NTRK] = { 258, 258, 326, 326 };
        for (int t = 0; t < NTRK; t++) {
            int hit = 0;
            pad(0x40u + t, px[t], py[t], 144, 62, pc[t], penv[t], pled[t], dk_role_name(trkrole[t]), t == sel, &hit);
            if (hit) { sel = t; fire(t, 1, 0); }                        // manual: play + select (no LED)
        }
    }

    font(FONT_NORMAL);
    ui_end();
}
