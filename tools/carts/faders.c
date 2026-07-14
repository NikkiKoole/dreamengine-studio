/* de:meta
{
  "slug": "faders",
  "title": "faders",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "The linear-control family from the control vocabulary, across three skins: a bank of vertical mixer faders (each with an LED reading its level), a horizontal fader, a centre-detented crossfader, a spring-return touch ribbon, and an XY pad.",
    "detail": "The sixth control-vocabulary study cart, the linear twin of rotaries. Row 1 FADERS: six vertical mixer faders — a recessed groove, a chunky grip cap, a coloured level fill, and an LED above that goes green->amber->red with the value (the icon's fader detail). Row 2 HORIZONTAL + CROSSFADER: a plain horizontal fader (fills from the left, unipolar) beside a crossfader (centre-detented, fills OUT from the middle, bipolar). Row 3 RIBBON + XY: a touch ribbon (absolute finger position, springs back to centre on release) and an XY pad (two values at once, a dot you drag on a crosshair field). Every finger is its own pointer (ui.h capture); wheel fine-tunes a hovered fader. Three skins: TACTILE (beveled caps, recessed grooves), FLAT, PURE (hairline tracks + a filled level bar, the native register). SKIN button or B.",
    "controls": "Drag any fader/cap; tap the track to jump there (absolute). Two fingers = two faders. Wheel fine-tunes the hovered one. The ribbon springs back to centre when you let go. Drag the XY dot. SKIN button (top-right) or B cycles TACTILE / FLAT / PURE."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"

// FADERS — the linear-control family (control-vocabulary.md §3 ⇅). Vertical +
// horizontal faders, a bipolar crossfader, a spring-return ribbon, an XY pad.
// Absolute value model (tap-to-jump). Three skins, per §10 (ui_skin).

enum { SK_TACTILE, SK_FLAT, SK_PURE };
static int skin = SK_TACTILE;
static const char *SKIN_NAME[3] = { "SKIN:TACTILE", "SKIN:FLAT", "SKIN:PURE" };
#define bevel (skin == SK_TACTILE)

static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

static void faceplate(int x, int y, int w, int h) {
    if (skin == SK_PURE) { rect(x, y, w, h, CLR_MEDIUM_GREY); return; }
    rrectfill(x, y, w, h, 4, CLR_DARK_BROWN);
    if (skin == SK_TACTILE) {
        line(x + 3, y + 1, x + w - 4, y + 1, CLR_BROWN);
        blend(BLEND_AVG); line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK); blend_reset();
    }
}

static int level_col(float v) { return v > 0.85f ? CLR_RED : v > 0.6f ? CLR_YELLOW : CLR_GREEN; }

// a small LED lamp reading a value (green->amber->red)
static void led(int cx, int cy, int on, int col) {
    if (skin == SK_PURE) { if (on) circfill(cx, cy, 2, col); arc(cx, cy, 2, 0, 360, on ? col : CLR_MEDIUM_GREY); return; }
    if (on) { blend(BLEND_AVG); circfill(cx, cy, 3, col); blend_reset(); }
    circfill(cx, cy, 2, on ? col : CLR_DARKER_GREY);
    if (on && bevel) pset(cx - 1, cy - 1, CLR_WHITE);
    arc(cx, cy, 2, 0, 360, CLR_BLACK);
}

// a beveled grip cap (a fader handle) centred at (cx,cy), size cw x ch, with a grip line.
static void cap(int cx, int cy, int cw, int ch, int hot) {
    int x = cx - cw / 2, y = cy - ch / 2;
    if (skin == SK_PURE) { rectfill(x, y, cw, ch, CLR_LIGHT_GREY); rect(x, y, cw, ch, hot ? CLR_WHITE : CLR_BROWNISH_BLACK); return; }
    rrectfill(x, y, cw, ch, 2, CLR_MEDIUM_GREY);
    if (bevel) {
        line(x + 1, y + 1, x + cw - 2, y + 1, CLR_LIGHT_GREY);           // top sheen
        line(x + 1, y + 1, x + 1, y + ch - 2, CLR_LIGHT_GREY);
        blend(BLEND_AVG); line(x + 1, y + ch - 1, x + cw - 2, y + ch - 1, CLR_BLACK);
        line(x + cw - 2, y + 1, x + cw - 2, y + ch - 2, CLR_BLACK); blend_reset();
    }
    line(x + 3, cy, x + cw - 4, cy, CLR_DARKER_GREY);                    // grip line
    rrect(x, y, cw, ch, 2, hot ? CLR_WHITE : CLR_BLACK);
}

// ── vertical fader ─────────────────────────────────────────────────────────
static void vfader(float *v, int cx, int ty, int th, int col, const char *label) {
    int hw = 20, hx = cx - hw / 2;
    ui_reg(v, hx, ty, hw, th, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { int cy = c->released ? c->ry : c->cy; *v = clamp((float)(ty + th - cy) / th, 0, 1); }
    int hot = c != 0 || ui_hover(hx, ty, hw, th);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.03f, 0, 1);

    int ch = 8, capy = ty + (int)((1 - *v) * (th - ch)) + ch / 2;
    led(cx, ty - 7, *v > 0.02f, level_col(*v));                         // the LED above
    if (skin == SK_PURE) {
        rect(cx - 3, ty, 7, th, hot ? CLR_WHITE : CLR_MEDIUM_GREY);     // hairline track
        rectfill(cx - 1, capy, 3, ty + th - capy, col);                 // filled level bar
        line(cx - 4, capy, cx + 4, capy, CLR_LIGHT_GREY);               // cap line
    } else {
        rrectfill(cx - 2, ty, 5, th, 2, CLR_BROWNISH_BLACK);            // recessed groove
        rectfill(cx - 1, capy, 3, ty + th - capy - 1, col);            // coloured level fill
        cap(cx, capy, 16, ch, hot);
    }
    if (label) plabel(label, cx, ty + th + 3, CLR_LIGHT_GREY);
}

// ── horizontal fader (bipolar = centre-detent crossfader) ───────────────────
static void hfader(float *v, int x, int y, int w, int col, int bipolar, const char *label) {
    int hh = 16;
    ui_reg(v, x, y - 2, w, hh, 0);
    UiCap *c = ui_cap_for(v);
    if (c) { int cx = c->released ? c->rx : c->cx; *v = clamp((float)(cx - x) / w, 0, 1); }
    int hot = c != 0 || ui_hover(x, y - 2, w, hh);
    if (!c && hot && mouse_wheel() != 0) *v = clamp(*v + mouse_wheel() * 0.03f, 0, 1);

    int cw = 10, capx = x + (int)(*v * (w - cw)) + cw / 2, cy = y + 5;
    if (skin == SK_PURE) {
        rect(x, y, w, 11, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
        if (bipolar) { int mid = x + w / 2; rectfill(capx < mid ? capx : mid, y + 2, capx < mid ? mid - capx : capx - mid, 7, col); }
        else rectfill(x + 1, y + 2, capx - x, 7, col);
        line(capx, y, capx, y + 10, CLR_LIGHT_GREY);
    } else {
        rrectfill(x, y, w, 11, 2, CLR_BROWNISH_BLACK);                  // groove
        int mid = x + w / 2;
        if (bipolar) { rectfill(capx < mid ? capx : mid, y + 3, capx < mid ? mid - capx : capx - mid, 5, col);
                       line(mid, y - 1, mid, y + 12, CLR_DARKER_GREY); }   // centre detent tick
        else rectfill(x + 1, y + 3, capx - x, 5, col);
        cap(capx, cy, cw, 12, hot);
    }
    if (label) plabel(label, x + w / 2, y + 14, CLR_LIGHT_GREY);
}

// ── ribbon (absolute touch strip, springs back to centre on release) ────────
static void ribbon(float *v, int *touched, int x, int y, int w, int h) {
    ui_reg(v, x, y, w, h, 0);
    UiCap *c = ui_cap_for(v);
    *touched = c != 0 && !c->released;
    if (*touched) *v = clamp((float)(c->cx - x) / w, 0, 1);
    int hot = c != 0 || ui_hover(x, y, w, h);
    int mx = x + (int)(*v * (w - 1));
    if (skin == SK_PURE) {
        rect(x, y, w, h, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
        rectfill(mx - 1, y + 1, 3, h - 2, *touched ? CLR_LIME_GREEN : CLR_GREEN);
    } else {
        rrectfill(x, y, w, h, 3, CLR_BROWNISH_BLACK);
        for (int i = 1; i < 4; i++) { int gx = x + w * i / 4; blend(BLEND_AVG); line(gx, y + 2, gx, y + h - 3, CLR_DARK_GREEN); blend_reset(); }
        rectfill(mx - 1, y + 2, 3, h - 4, *touched ? CLR_LIME_GREEN : CLR_GREEN);   // position marker
        if (bevel && *touched) { blend(BLEND_AVG); rectfill(mx - 3, y + 2, 7, h - 4, CLR_GREEN); blend_reset(); }
    }
}

// ── XY pad (two values, a dot on a crosshair field) ─────────────────────────
static void xypad(float *vx, float *vy, int x, int y, int w, int h) {
    ui_reg(vx, x, y, w, h, 0);
    UiCap *c = ui_cap_for(vx);
    if (c) { int cx = c->released ? c->rx : c->cx, cy = c->released ? c->ry : c->cy;
             *vx = clamp((float)(cx - x) / w, 0, 1); *vy = clamp((float)(y + h - cy) / h, 0, 1); }
    int hot = c != 0 || ui_hover(x, y, w, h);
    int dx2 = x + (int)(*vx * (w - 1)), dy2 = y + (int)((1 - *vy) * (h - 1));
    if (skin == SK_PURE) {
        rect(x, y, w, h, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
        line(x, dy2, x + w - 1, dy2, CLR_DARK_GREY); line(dx2, y, dx2, y + h - 1, CLR_DARK_GREY);
        circfill(dx2, dy2, 2, CLR_LIME_GREEN);
    } else {
        rrectfill(x, y, w, h, 2, CLR_BROWNISH_BLACK);
        blend(BLEND_AVG); line(x, dy2, x + w - 1, dy2, CLR_DARK_GREEN); line(dx2, y, dx2, y + h - 1, CLR_DARK_GREEN); blend_reset();
        circfill(dx2, dy2, 3, CLR_LIME_GREEN);
        if (bevel) pset(dx2 - 1, dy2 - 1, CLR_WHITE);
        circ(dx2, dy2, 3, CLR_BLACK);
        rrect(x, y, w, h, 2, hot ? CLR_WHITE : CLR_DARKER_GREY);
    }
}

// ── state ────────────────────────────────────────────────────────────────
static float vf[6] = { 0.8f, 0.6f, 0.9f, 0.4f, 0.7f, 0.5f };
static float hf = 0.35f, xf = 0.5f;               // horizontal fader + crossfader
static float rib = 0.5f; static int rib_touch = 0;
static float xyx = 0.35f, xyy = 0.6f;

void init(void) { bpm(120); }

void update(void) {
    if (!rib_touch) rib += (0.5f - rib) * 0.2f;   // ribbon springs back to centre
#ifdef DE_TRACE
    watch("vf0", "%.2f", vf[0]); watch("xf", "%.2f", xf); watch("rib", "%.2f", rib);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("FADERS", 6, 3, CLR_WHITE);
    font(FONT_SMALL);
    if (ui_button(SCREEN_W - 74, 3, 70, 11, SKIN_NAME[skin]) || keyp('B')) skin = (skin + 1) % 3;

    // ── ROW 1 — vertical mixer faders (LED above reads the level) ──────────
    print("1 FADERS  a mixer bank - the LED reads the level (green/amber/red)", 6, 18, CLR_DARK_GREY);
    faceplate(4, 26, 312, 96);
    {
        const char *mn[6] = { "MST", "OSC", "SUB", "NSE", "FX", "VOL" };
        int fc[6] = { CLR_GREEN, CLR_TRUE_BLUE, CLR_MAUVE, CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN };
        for (int i = 0; i < 6; i++) vfader(&vf[i], 28 + i * 48, 44, 62, fc[i], mn[i]);
    }

    // ── ROW 2 — horizontal fader + centre-detented crossfader ──────────────
    print("2 HORIZONTAL + CROSSFADER  unipolar fill / bipolar centre-out", 6, 128, CLR_DARK_GREY);
    faceplate(4, 136, 312, 40);
    {
        hfader(&hf, 16, 150, 130, CLR_GREEN, 0, "LEVEL");
        hfader(&xf, 176, 150, 128, CLR_TRUE_BLUE, 1, "A / B");
    }

    // ── ROW 3 — ribbon + XY pad ────────────────────────────────────────────
    print("3 RIBBON + XY  touch strip (springs to centre) / two values at once", 6, 184, CLR_DARK_GREY);
    faceplate(4, 192, 312, 62);
    {
        ribbon(&rib, &rib_touch, 14, 210, 180, 20);
        plabel("RIBBON", 14 + 90, 234, CLR_LIGHT_GREY);
        xypad(&xyx, &xyy, 216, 200, 50, 44);
        plabel("XY", 216 + 25, 246, CLR_LIGHT_GREY);
    }

    font(FONT_NORMAL);
    ui_end();
}
