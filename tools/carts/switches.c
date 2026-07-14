/* de:meta
{
  "slug": "switches",
  "title": "switches",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "The press + flip families from the control vocabulary, in beveled lo-fi: momentary and latching buttons, the illuminated light vocabulary (off/dim/lit/blink), a radio group, and the flip family — bat toggles, a slide switch, a rocker.",
    "detail": "The second study cart for docs/design/control-vocabulary.md (the sibling of rotaries). Row 1 PUSH: MOMENTARY (on only while held), LATCH (a press flips a sticky state), ILLUM (a lit latching button), BLINK (the pending/recording state). Row 2 LIGHT STATES + RADIO: the one light vocabulary every illuminated control shares — OFF / DIM (armed) / LIT (active) / BLINK (pending) — plus a mutually-exclusive RADIO group (one lit). Row 3 SWITCHES (flip): a 2-position and a 3-position BAT toggle whose lever leans, a SLIDE switch whose nub sits at a labelled stop, and a ROCKER see-saw. Buttons are beveled caps that depress + darken on press; input rides ui.h's ui_button_core so every one is mouse + touch + focus at once.",
    "controls": "Tap/click any button. MOMENTARY lights only while held. LATCH/ILLUM toggle and stay. Tap a RADIO button to select it (only one lit). Tap a BAT toggle or SLIDE to step it to the next position; tap the ROCKER to flip it. Tap BEVEL (top-right) or press B to toggle the Win95 chisel — flat vs beveled (the ui_skin prototype)."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"

// SWITCHES — the press + flip families (control-vocabulary.md §3 ⊙ / ⇄) in
// beveled lo-fi. A button is a rounded cap that depresses + darkens; an
// illuminated one carries the shared light vocabulary (off/dim/lit/blink); a
// switch LEANS or SLIDES to a discrete position. Input rides ui.h's
// ui_button_core, so every control is mouse + touch + focus at once.

// ── shared chrome ────────────────────────────────────────────────────────
static int bevel = 1;   // Win95 chisel on/off — the ui_skin prototype

static void faceplate(int x, int y, int w, int h) {
    rrectfill(x, y, w, h, 4, CLR_DARK_BROWN);
    line(x + 3, y + 1, x + w - 4, y + 1, CLR_BROWN);           // top sheen
    blend(BLEND_AVG);
    line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK);   // bottom shade
    blend_reset();
}

static void plabel(const char *s, int cx, int y, int col) {
    print(s, cx - text_width(s) / 2, y, col);
}

// the Win95 double-bevel chisel: highlight on TOP+LEFT, shadow on BOTTOM+RIGHT,
// two tones deep, black on the outer bottom-right. Draw highlight first so the
// shadow wraps the shared corners (the recognizable look). tl/br = the two edge
// colours (outer, inner); inverting them sinks the control.
static void chisel(int x, int y, int w, int h, int tl_out, int tl_in, int br_in, int br_out) {
    line(x, y, x + w - 1, y, tl_out); line(x, y, x, y + h - 1, tl_out);              // outer TL
    line(x, y + h - 1, x + w - 1, y + h - 1, br_out); line(x + w - 1, y, x + w - 1, y + h - 1, br_out); // outer BR
    line(x + 1, y + 1, x + w - 2, y + 1, tl_in); line(x + 1, y + 1, x + 1, y + h - 2, tl_in);           // inner TL
    line(x + 1, y + h - 2, x + w - 2, y + h - 2, br_in); line(x + w - 2, y + 1, x + w - 2, y + h - 2, br_in); // inner BR
}

// a Win95-style pushbutton. RAISED normally, SUNKEN (inverted bevel + content
// nudged down-right) when pressed/engaged; `face` is the cap colour (caller picks
// it per state); `glow` adds a soft lit halo for LED-style buttons; `lab` = label.
static void draw_btn(int x, int y, int w, int h, const char *label,
                     int pressed, int face, int lab, int hot, int glow) {
    if (glow) { blend(BLEND_AVG); rectfill(x - 2, y - 2, w + 4, h + 4, face); blend_reset(); }
    rectfill(x, y, w, h, face);                                // square face
    if (!bevel) rect(x, y, w, h, CLR_BLACK);                   // flat: a plain 1px border
    else if (pressed) chisel(x, y, w, h, CLR_BLACK, CLR_MEDIUM_GREY, CLR_LIGHT_GREY, CLR_WHITE);  // sunken
    else              chisel(x, y, w, h, CLR_WHITE, CLR_LIGHT_GREY, CLR_MEDIUM_GREY, CLR_BLACK);  // raised
    if (hot && !pressed) rect(x + 2, y + 2, w - 4, h - 4, CLR_LIGHT_YELLOW);  // hover cue (mouse)
    int o = pressed ? 1 : 0;                                   // content nudges into the press
    print(label, x + (w - text_width(label)) / 2 + o, y + (h - 6) / 2 + o, lab);
}

// input for a button-shaped control: register + resolve press/activate/hot.
static int btn_input(unsigned seed, int x, int y, int w, int h, int *pr, int *hot) {
    void *wid = ui_wid_hash(seed, x, y, w, h);
    int foc = 0;
    return ui_button_core(wid, x, y, w, h, &foc, pr, hot);
}

// a bat toggle: a base + a lever that LEANS to position `pos` of `n` (a selector).
static void bat_toggle(int cx, int cy, int pos, int n, int hot) {
    rrectfill(cx - 12, cy - 2, 24, 16, 3, CLR_DARKER_GREY);    // base plate
    rrect(cx - 12, cy - 2, 24, 16, 3, hot ? CLR_WHITE : CLR_BLACK);
    blend(BLEND_AVG);
    ovalfill(cx, cy + 6, 6, 3, CLR_BLACK);                     // recessed slot shadow
    blend_reset();
    float ang = 270 + (pos - (n - 1) / 2.0f) * 42;             // 270 = straight up
    int px = cx, py = cy + 8;                                  // pivot
    int tx = px + (int)dx(13, ang), ty = py + (int)dy(13, ang);
    line(px, py, tx, ty, CLR_MEDIUM_GREY);                     // lever shaft
    line(px + 1, py, tx + 1, ty, CLR_LIGHT_GREY);
    circfill(tx, ty, 4, CLR_LIGHT_GREY);                       // beveled ball tip
    arc(tx, ty, 4, 205, 335, CLR_WHITE);
    arc(tx, ty, 4, 0, 360, CLR_BLACK);
}

// a slide switch: a recessed track with a nub sitting at stop `pos` of `n`.
static void slide_switch(int x, int y, int w, int pos, int n, int hot) {
    rrectfill(x, y, w, 12, 3, CLR_BROWNISH_BLACK);             // recessed track
    rrect(x, y, w, 12, 3, hot ? CLR_WHITE : CLR_DARKER_GREY);
    int seg = w / n, nx = x + seg * pos;
    rrectfill(nx + 2, y + 1, seg - 4, 10, 2, CLR_LIGHT_GREY);  // the nub
    line(nx + 4, y + 2, nx + seg - 6, y + 2, CLR_WHITE);
    blend(BLEND_AVG);
    line(nx + 4, y + 10, nx + seg - 6, y + 10, CLR_BLACK);
    blend_reset();
}

// a rocker see-saw: two halves, the active one raised + lit, the other pressed in.
static void rocker(int x, int y, int w, int h, int on, int hot) {
    rrectfill(x, y, w, h, 3, CLR_DARKER_GREY);
    rrect(x, y, w, h, 3, hot ? CLR_WHITE : CLR_BLACK);
    int mid = y + h / 2;
    // top half = "I" (on), bottom half = "O" (off); whichever is active is raised+lit
    blend(BLEND_AVG);
    rectfill(x + 1, on ? mid : y + 1, w - 2, h / 2 - 1, CLR_BLACK);   // pressed half in shadow
    blend_reset();
    print("I", x + w / 2 - 1, y + 3, on ? CLR_GREEN : CLR_DARK_GREY);
    print("O", x + w / 2 - 1, mid + 2, on ? CLR_DARK_GREY : CLR_LIGHT_GREY);
}

// ── state ────────────────────────────────────────────────────────────────
static int latch_on = 0, illum_on = 1, rec_on = 1;
static int radio_sel = 1;
static int bat2 = 1, bat3 = 1, slide_p = 0, rock_on = 1;
static int step[8] = { 1, 0, 0, 1, 0, 1, 0, 0 };       // COMPACT: a finger-sized step row
static int cbat = 1, crock = 1;

void init(void) { bpm(120); }

void update(void) {
#ifdef DE_TRACE
    watch("latch", "%d", latch_on); watch("radio", "%d", radio_sel);
    watch("bat3", "%d", bat3); watch("slide", "%d", slide_p);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("SWITCHES", 6, 4, CLR_WHITE);
    font(FONT_SMALL);
    if (ui_button(SCREEN_W - 64, 3, 60, 11, bevel ? "BEVEL:ON" : "BEVEL:OFF") || keyp('B')) bevel = !bevel;

    int col[4] = { 12, 88, 164, 240 }, bw = 64, bh = 24;
    int blink = ((int)(now() * 2)) & 1;                        // 2 Hz pending flash

    // ── ROW 1 — PUSH: momentary / latching / illuminated / blink ───────────
    print("1 PUSH  momentary hold / latching toggle / illuminated / pending", 6, 20, CLR_DARK_GREY);
    faceplate(4, 28, 312, 44);
    int y1 = 40;
    {
        // MOMENTARY — lit only while held
        int pr = 0, hot = 0; btn_input(0x51u, col[0], y1, bw, bh, &pr, &hot);
        draw_btn(col[0], y1, bw, bh, "PUSH", pr, pr ? CLR_LIGHT_YELLOW : CLR_DARK_GREY,
                 pr ? CLR_BLACK : CLR_LIGHT_GREY, hot, pr);
        // LATCH — flips a sticky state; ENGAGED = sunken (the Win95 toggle look)
        int pr2 = 0, hot2 = 0; if (btn_input(0x52u, col[1], y1, bw, bh, &pr2, &hot2)) latch_on = !latch_on;
        draw_btn(col[1], y1, bw, bh, latch_on ? "ON" : "OFF", pr2 || latch_on,
                 latch_on ? CLR_GREEN : CLR_DARK_GREY, latch_on ? CLR_BLACK : CLR_LIGHT_GREY, hot2, 0);
        // ILLUM — a lit latching button
        int pr3 = 0, hot3 = 0; if (btn_input(0x53u, col[2], y1, bw, bh, &pr3, &hot3)) illum_on = !illum_on;
        draw_btn(col[2], y1, bw, bh, "LAMP", pr3, illum_on ? CLR_TRUE_BLUE : CLR_DARK_BLUE,
                 illum_on ? CLR_WHITE : CLR_MEDIUM_GREY, hot3, illum_on);
        // BLINK — the pending / recording state
        int pr4 = 0, hot4 = 0; if (btn_input(0x54u, col[3], y1, bw, bh, &pr4, &hot4)) rec_on = !rec_on;
        int reclit = rec_on && blink;
        draw_btn(col[3], y1, bw, bh, "REC", pr4, reclit ? CLR_RED : CLR_DARK_RED,
                 reclit ? CLR_WHITE : CLR_LIGHT_PEACH, hot4, reclit);
    }

    // ── ROW 2 — the light vocabulary + a radio group ───────────────────────
    print("2 LIGHT STATES  off / dim / lit / blink        + RADIO (one lit)", 6, 78, CLR_DARK_GREY);
    faceplate(4, 86, 312, 44);
    int y2 = 98;
    {
        // the four light states (display-only samples)
        const char *sn[4] = { "OFF", "DIM", "LIT", "BLINK" };
        int sf[4] = { CLR_DARKER_GREY, CLR_DARK_RED, CLR_RED, blink ? CLR_RED : CLR_DARK_RED };
        int sg[4] = { 0, 0, 1, blink };
        for (int i = 0; i < 4; i++)
            draw_btn(col[i], y2, bw, bh, sn[i], 0, sf[i],
                     i >= 2 ? CLR_WHITE : CLR_LIGHT_PEACH, 0, sg[i]);
    }

    // ── ROW 2b — RADIO group (its own faceplate) ───────────────────────────
    print("   RADIO  mutually exclusive - tap one", 6, 136, CLR_DARK_GREY);
    faceplate(4, 144, 312, 44);
    int y2b = 156;
    {
        const char *wn[4] = { "SIN", "SQR", "SAW", "TRI" };
        for (int i = 0; i < 4; i++) {
            int pr = 0, hot = 0;
            if (btn_input(0x60u + i, col[i], y2b, bw, bh, &pr, &hot)) radio_sel = i;
            int on = radio_sel == i;                             // selected = sunken (Win95 toggle)
            draw_btn(col[i], y2b, bw, bh, wn[i], pr || on, on ? CLR_LIME_GREEN : CLR_DARK_GREEN,
                     on ? CLR_BLACK : CLR_LIGHT_GREY, hot, 0);
        }
    }

    // ── ROW 3 — SWITCHES (flip): bat / slide / rocker ──────────────────────
    print("3 SWITCHES  bat toggle (2 & 3 pos) / slide / rocker", 6, 194, CLR_DARK_GREY);
    faceplate(4, 202, 312, 62);
    int y3 = 224;
    {
        // BAT 2-position
        int pr = 0, hot = 0;
        if (btn_input(0x71u, col[0] + 16, y3 - 12, 32, 32, &pr, &hot)) bat2 = !bat2;
        bat_toggle(col[0] + 32, y3, bat2, 2, hot);
        plabel("BAT 2", col[0] + 32, y3 + 22, CLR_LIGHT_GREY);
        // BAT 3-position
        int pr2 = 0, hot2 = 0;
        if (btn_input(0x72u, col[1] + 16, y3 - 12, 32, 32, &pr2, &hot2)) bat3 = (bat3 + 1) % 3;
        bat_toggle(col[1] + 32, y3, bat3, 3, hot2);
        plabel("BAT 3", col[1] + 32, y3 + 22, CLR_LIGHT_GREY);
        // SLIDE 3-position
        int pr3 = 0, hot3 = 0;
        if (btn_input(0x73u, col[2] + 4, y3 - 4, 68, 16, &pr3, &hot3)) slide_p = (slide_p + 1) % 3;
        slide_switch(col[2] + 6, y3 - 2, 60, slide_p, 3, hot3);
        plabel("SLIDE", col[2] + 36, y3 + 22, CLR_LIGHT_GREY);
        // ROCKER
        int pr4 = 0, hot4 = 0;
        if (btn_input(0x74u, col[3] + 16, y3 - 10, 32, 28, &pr4, &hot4)) rock_on = !rock_on;
        rocker(col[3] + 20, y3 - 8, 24, 26, rock_on, hot4);
        plabel("ROCKER", col[3] + 32, y3 + 22, CLR_LIGHT_GREY);
    }

    // ── ROW 4 — COMPACT: everything shrunk to a finger minimum ─────────────
    int fp = finger_px(), s = fp < 16 ? 16 : (fp > 26 ? 26 : fp);   // finger target, clamped for layout
    print(str("4 COMPACT  sized to finger_px()=%d - hit area still auto-pads to %d", fp, UI_MIN_TARGET),
          6, 270, CLR_DARK_GREY);
    faceplate(4, 278, 312, 52);
    int sy = 290;
    {
        // a mini STEP ROW — the zone-4 use case: 8 finger-sized toggles
        for (int i = 0; i < 8; i++) {
            int x = 10 + i * (s + 3), pr = 0, hot = 0;
            if (btn_input(0x80u + i, x, sy, s, s, &pr, &hot)) step[i] = !step[i];
            draw_btn(x, sy, s, s, "", pr, step[i] ? CLR_LIME_GREEN : CLR_DARK_GREY,
                     CLR_BLACK, hot, step[i]);
        }
        plabel("STEP ROW", 10 + 4 * (s + 3) - 2, sy + s + 3, CLR_LIGHT_GREY);

        // the flip family shrinks too — a compact bat + rocker
        int rx = 10 + 8 * (s + 3) + 20;
        int pr = 0, hot = 0;
        if (btn_input(0x88u, rx - 12, sy, 26, s + 2, &pr, &hot)) cbat = !cbat;
        bat_toggle(rx, sy + 6, cbat, 2, hot);
        plabel("BAT", rx, sy + s + 3, CLR_LIGHT_GREY);

        int cx2 = rx + 42, pr2 = 0, hot2 = 0;
        if (btn_input(0x89u, cx2 - 2, sy - 2, s + 4, s + 4, &pr2, &hot2)) crock = !crock;
        rocker(cx2, sy, s, s, crock, hot2);
        plabel("ROCK", cx2 + s / 2, sy + s + 3, CLR_LIGHT_GREY);
    }

    font(FONT_NORMAL);
    ui_end();
}
