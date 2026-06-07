// ui.h — cross-input widgets (button · slider · knob), cart-land.
//
// Why this exists (ui-widgets-notes.md): six carts hand-roll the same
// claim-on-press / drag-while-held / release state machine (modrack, sh101,
// tb303, sfx generator's 17 sliders, dream synth, wave editor) — mouse-only,
// every time. This header generalizes it for mouse, touch, AND
// keyboard/gamepad at once. Engine deliberately does NOT own widgets
// (decision-0006 lane); carts #include this instead.
//
// Built on the virtual touch pool: the desktop mouse arrives as one
// synthetic finger, so everything below is written against
// touch_count()/touch_id()/touch_ended_*() and is mouse-and-touch capable
// for free. Per-contact capture means two fingers turn two knobs at once.
//
// Usage — widgets are called from draw() (they read input AND put pixels
// down; input is stable across the whole frame):
//
//   #include "ui.h"
//   static float vol = 0.8f, cut = 0.5f;     // widgets edit floats in 0..1
//   void draw(void) {
//       cls(CLR_BLACK);
//       ui_begin();                              // FIRST: contacts, focus keys
//       if (ui_button(10, 180, 52, 16, "play")) toggle_play();
//       if (ui_slider(&vol, 10, 10, 100, "vol")) apply_vol();  // true = changed
//       ui_knob(&cut, 90, 160, "cut");           // x,y = knob CENTER
//       ui_end();                                // LAST: resolve presses
//   }
//
//   ui_grabbed(&vol)  — true the frame a contact takes the widget. Fires
//                       BEFORE the widget applies its first value change
//                       when checked above the widget call — snapshot your
//                       undo state there.
//   ui_released(&vol) — true the frame it lets go (commit/replay here;
//                       check it after the widget call so the final
//                       position has been applied)
//   ui_focus(true)    — opt-in keyboard/gamepad: P0 up/down move a marching
//                       focus ring, left/right adjust the focused value
//                       (hold to accelerate), A activates a focused button.
//                       Off by default so a cart's own d-pad gameplay is
//                       never stolen.
//
// Feel: sliders are absolute (press sets the value at the press point —
// sfxp style); knobs are relative (drag up/down from wherever you grabbed,
// the tb303 fine-control style; hover + wheel = fine-tune when a mouse is
// around — an enhancement, never the only path).
//
// Every widget hit-tests an INFLATED rect (≥ UI_MIN_TARGET px, the
// mobile-lint fat-finger threshold) while drawing the compact visual.
// Presses are resolved at ui_end() against every widget drawn this frame:
// a press inside a widget's visual rect beats inflated-only hits, so dense
// desktop layouts (17 sliders at 9px pitch) still route correctly — the
// inflation only catches presses that would otherwise hit nothing.
// (Consequence: a capture starts on the NEXT frame's widget call —
// invisible at 60fps.)
//
// Screen-space only: call widgets after camera(0,0), like HUDs already do.
// Only the left mouse button participates (it's what the vt pool merges);
// right/middle click stay cart-land enhancements.
//
// Tunables — #define BEFORE the #include to override:
//   UI_MIN_TARGET   (24)   minimum hit-target square, canvas px
//   UI_HIT_PAD      (2)    extra hit padding on every side
//   UI_SLIDER_H     (9)    slider row height
//   UI_KNOB_R       (10)   knob radius
//   UI_KNOB_DRAG_PX (60)   vertical drag distance for a full 0..1 sweep
//   UI_WHEEL_STEP   (0.02) knob fine-tune per wheel notch
//   UI_COL_BG / UI_COL_FILL / UI_COL_FILL_HOT / UI_COL_FRAME /
//   UI_COL_TEXT / UI_COL_TEXT_HOT / UI_COL_FOCUS   palette indices
//
// Everything is static (each cart compiles its own copy — gestures.h /
// improv.h pattern). v1 is deliberately three widgets; panel/drag-from wait
// for a second customer (ui-widgets-notes.md §6).

#ifndef UI_H
#define UI_H

#include "studio.h"
#include <stdint.h>

#ifndef UI_MIN_TARGET
#define UI_MIN_TARGET 24
#endif
#ifndef UI_HIT_PAD
#define UI_HIT_PAD 2
#endif
#ifndef UI_SLIDER_H
#define UI_SLIDER_H 9
#endif
#ifndef UI_KNOB_R
#define UI_KNOB_R 10
#endif
#ifndef UI_KNOB_DRAG_PX
#define UI_KNOB_DRAG_PX 60
#endif
#ifndef UI_WHEEL_STEP
#define UI_WHEEL_STEP 0.02f
#endif
#ifndef UI_COL_BG
#define UI_COL_BG CLR_DARKER_BLUE
#endif
#ifndef UI_COL_FILL
#define UI_COL_FILL CLR_DARKER_PURPLE
#endif
#ifndef UI_COL_FILL_HOT
#define UI_COL_FILL_HOT CLR_BLUE
#endif
#ifndef UI_COL_FRAME
#define UI_COL_FRAME CLR_DARK_GREY
#endif
#ifndef UI_COL_TEXT
#define UI_COL_TEXT CLR_LIGHT_GREY
#endif
#ifndef UI_COL_TEXT_HOT
#define UI_COL_TEXT_HOT CLR_WHITE
#endif
#ifndef UI_COL_FOCUS
#define UI_COL_FOCUS CLR_WHITE
#endif

#define UI_MAX_WID   64   // widgets drawn per frame
#define UI_MAX_CAP    8   // simultaneous captured contacts
#define UI_MAX_PRESS  8   // new presses per frame
#define UI_MAX_SEEN  16   // tracked contact ids
#define UI_MAX_EVT    8   // grab/release events per frame

typedef struct {        // a widget drawn this frame
    void *wid;          // identity: the edited value's address (buttons: label+rect hash)
    int x, y, w, h;     // visual rect
    int hx, hy, hw, hh; // inflated hit rect
    int focusable;      // takes part in focus traversal
} UiWid;

typedef struct {        // a contact that owns a widget
    void *wid;
    int   id;           // contact id (mouse = the vt pool's synthetic finger)
    int   cx, cy;       // contact position right now
    int   rx, ry;       // where it lifted (valid when released)
    int   by;           // knob: y where v0 was snapped
    float v0;           // knob: value at grab
    int   has_v0;       // knob baseline snapped
    int   released;     // contact lifted; widget gets one frame to react
} UiCap;

typedef struct { int id, x, y; } UiPress;

static UiWid   ui_wids[UI_MAX_WID];   static int ui_wid_n = 0;
static UiCap   ui_caps[UI_MAX_CAP];   static int ui_cap_n = 0;
static UiPress ui_press[UI_MAX_PRESS]; static int ui_press_n = 0;
static int     ui_seen[UI_MAX_SEEN];  static int ui_seen_n = 0;
static void   *ui_grab_evt[UI_MAX_EVT]; static int ui_grab_n = 0;
static void   *ui_rel_evt[UI_MAX_EVT];  static int ui_rel_n = 0;

static int   ui_frame_ct = 0;
static int   ui_focus_on = 0;
static int   ui_focus_i = 0;
static int   ui_foc_count = 0;   // focusables registered last frame
static int   ui_foc_n = 0;       // focusables registered so far this frame
static float ui_adj = 0;         // focused-value delta this frame (left/right)
static int   ui_activate = 0;    // A pressed this frame (focused button fires)
static int   ui_hold_l = 0, ui_hold_r = 0;

// ── internals ────────────────────────────────────────────────────────────

static int ui_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static int ui_seen_has(int id) {
    for (int i = 0; i < ui_seen_n; i++) if (ui_seen[i] == id) return 1;
    return 0;
}
static void ui_seen_drop(int id) {
    for (int i = 0; i < ui_seen_n; i++)
        if (ui_seen[i] == id) { ui_seen[i] = ui_seen[--ui_seen_n]; return; }
}

static void ui_cap_drop(int i) { ui_caps[i] = ui_caps[--ui_cap_n]; }

// the cap currently driving this widget: prefer a live contact over a
// released one, so a second finger takes over the frame the first lets go
static UiCap *ui_cap_for(void *wid) {
    UiCap *rel = 0;
    for (int i = 0; i < ui_cap_n; i++) {
        if (ui_caps[i].wid != wid) continue;
        if (!ui_caps[i].released) return &ui_caps[i];
        if (!rel) rel = &ui_caps[i];
    }
    return rel;
}

static void ui_push_grab(void *v) { if (ui_grab_n < UI_MAX_EVT) ui_grab_evt[ui_grab_n++] = v; }
static void ui_push_rel(void *v)  { if (ui_rel_n  < UI_MAX_EVT) ui_rel_evt[ui_rel_n++]   = v; }

// register a widget for this frame's press resolution; returns its focus index (-1 if not focusable)
static int ui_reg(void *wid, int x, int y, int w, int h, int focusable) {
    if (ui_wid_n >= UI_MAX_WID) return -1;
    UiWid *u = &ui_wids[ui_wid_n++];
    u->wid = wid; u->x = x; u->y = y; u->w = w; u->h = h;
    int padx = UI_HIT_PAD, pady = UI_HIT_PAD;
    if (w + 2 * padx < UI_MIN_TARGET) padx = (UI_MIN_TARGET - w + 1) / 2;
    if (h + 2 * pady < UI_MIN_TARGET) pady = (UI_MIN_TARGET - h + 1) / 2;
    u->hx = x - padx; u->hy = y - pady; u->hw = w + 2 * padx; u->hh = h + 2 * pady;
    u->focusable = focusable;
    return focusable ? ui_foc_n++ : -1;
}

static int ui_hover(int x, int y, int w, int h) {
    return ui_in(mouse_x(), mouse_y(), x, y, w, h);
}

// 1px marching focus ring around a rect (pass the rect already inflated)
static void ui_ring(int x, int y, int w, int h) {
    int per = 2 * w + 2 * h, off = (ui_frame_ct / 3) % 8;
    for (int p = 0; p < per; p++) {
        if (((p + off) % 8) >= 4) continue;
        int px, py;
        if      (p < w)             { px = x + p;                 py = y; }
        else if (p < w + h)         { px = x + w - 1;             py = y + (p - w); }
        else if (p < 2 * w + h)     { px = x + w - 1 - (p - w - h); py = y + h - 1; }
        else                        { px = x;                     py = y + h - 1 - (p - 2 * w - h); }
        pset(px, py, UI_COL_FOCUS);
    }
}

// ── frame brackets ───────────────────────────────────────────────────────

// call FIRST in draw(): tracks contacts, marks releases, reads focus keys.
// (grab/release events are NOT cleared here — grabs were pushed at the end
// of last frame's ui_end, releases get pushed below; the cart reads both
// between ui_begin and ui_end, and ui_end clears them.)
static void ui_begin(void) {
    ui_frame_ct++;
    ui_wid_n = 0; ui_foc_n = 0;
    ui_press_n = 0;

    // update captured contacts with their current position
    for (int c = 0; c < ui_cap_n; c++) {
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == ui_caps[c].id) {
                ui_caps[c].cx = touch_x(i);
                ui_caps[c].cy = touch_y(i);
                break;
            }
    }

    // new contacts → this frame's presses (resolved against widgets in ui_end)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (ui_seen_has(id)) continue;
        if (ui_seen_n < UI_MAX_SEEN) ui_seen[ui_seen_n++] = id;
        if (ui_press_n < UI_MAX_PRESS)
            ui_press[ui_press_n++] = (UiPress){ id, touch_x(i), touch_y(i) };
    }

    // lifted contacts → mark their capture released (widget reacts this frame)
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        ui_seen_drop(id);
        for (int c = 0; c < ui_cap_n; c++)
            if (ui_caps[c].id == id && !ui_caps[c].released) {
                ui_caps[c].released = 1;
                ui_caps[c].rx = touch_ended_x(i);
                ui_caps[c].ry = touch_ended_y(i);
                ui_push_rel(ui_caps[c].wid);
            }
    }
    // insurance: a captured id that's neither active nor in the ended list
    for (int c = 0; c < ui_cap_n; c++) {
        if (ui_caps[c].released) continue;
        int live = 0;
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == ui_caps[c].id) { live = 1; break; }
        if (!live) {
            ui_caps[c].released = 1;
            ui_caps[c].rx = ui_caps[c].cx; ui_caps[c].ry = ui_caps[c].cy;
            ui_push_rel(ui_caps[c].wid);
        }
    }

    // focus keys (P0): up/down traverse, left/right adjust (hold accelerates), A activates
    ui_adj = 0; ui_activate = 0;
    if (ui_focus_on && ui_foc_count > 0) {
        if (btnp(0, BTN_DOWN)) ui_focus_i++;
        if (btnp(0, BTN_UP))   ui_focus_i--;
        ui_focus_i = (ui_focus_i % ui_foc_count + ui_foc_count) % ui_foc_count;
        ui_hold_l = btn(0, BTN_LEFT)  ? ui_hold_l + 1 : 0;
        ui_hold_r = btn(0, BTN_RIGHT) ? ui_hold_r + 1 : 0;
        if (btnp(0, BTN_LEFT)  || ui_hold_l > 18) ui_adj -= ui_hold_l > 18 ? 0.015f : 0.02f;
        if (btnp(0, BTN_RIGHT) || ui_hold_r > 18) ui_adj += ui_hold_r > 18 ? 0.015f : 0.02f;
        ui_activate = btnp(0, BTN_A);
    }
}

// call LAST in draw(): routes this frame's presses to widgets, prunes captures
static void ui_end(void) {
    ui_grab_n = 0; ui_rel_n = 0;   // cart has read this frame's events by now
    for (int p = 0; p < ui_press_n; p++) {
        int already = 0;          // a contact captures at most one widget
        for (int c = 0; c < ui_cap_n; c++)
            if (ui_caps[c].id == ui_press[p].id) { already = 1; break; }
        if (already || ui_cap_n >= UI_MAX_CAP) continue;

        int best = -1;
        for (int w = 0; w < ui_wid_n; w++)        // visual hit: last drawn wins (it's on top)
            if (ui_in(ui_press[p].x, ui_press[p].y,
                      ui_wids[w].x, ui_wids[w].y, ui_wids[w].w, ui_wids[w].h)) best = w;
        if (best < 0) {                            // inflated hit: nearest center wins
            int bd = 0;
            for (int w = 0; w < ui_wid_n; w++) {
                if (!ui_in(ui_press[p].x, ui_press[p].y,
                           ui_wids[w].hx, ui_wids[w].hy, ui_wids[w].hw, ui_wids[w].hh)) continue;
                int dx = ui_press[p].x - (ui_wids[w].x + ui_wids[w].w / 2);
                int dy = ui_press[p].y - (ui_wids[w].y + ui_wids[w].h / 2);
                int d  = dx * dx + dy * dy;
                if (best < 0 || d < bd) { best = w; bd = d; }
            }
        }
        if (best >= 0) {
            ui_caps[ui_cap_n++] = (UiCap){ ui_wids[best].wid, ui_press[p].id,
                ui_press[p].x, ui_press[p].y, 0, 0, 0, 0, 0, 0 };
            ui_push_grab(ui_wids[best].wid);   // cart sees it NEXT frame, before the value moves
        }
    }

    // prune: released caps had their one frame; orphaned caps lost their widget
    for (int c = ui_cap_n - 1; c >= 0; c--) {
        if (ui_caps[c].released) { ui_cap_drop(c); continue; }
        int found = 0;
        for (int w = 0; w < ui_wid_n; w++)
            if (ui_wids[w].wid == ui_caps[c].wid) { found = 1; break; }
        if (!found) ui_cap_drop(c);
    }

    ui_foc_count = ui_foc_n;     // next frame's traversal range
}

// ── cart-facing state queries ────────────────────────────────────────────

// opt-in keyboard/gamepad focus (sticky; call once or toggle live)
static void ui_focus(int on) { ui_focus_on = on; if (!on) ui_focus_i = 0; }

// true the frame a contact grabbed the widget editing v (push your undo state here)
static int ui_grabbed(void *v) {
    for (int i = 0; i < ui_grab_n; i++) if (ui_grab_evt[i] == v) return 1;
    return 0;
}
// true the frame that contact let go (commit / replay here)
static int ui_released(void *v) {
    for (int i = 0; i < ui_rel_n; i++) if (ui_rel_evt[i] == v) return 1;
    return 0;
}

// ── the widgets ──────────────────────────────────────────────────────────

// horizontal slider editing *v in 0..1; absolute (press sets the value).
// Returns 1 if *v changed this frame. Visual: a filled row, label inside.
static int ui_slider(float *v, int x, int y, int w, const char *label) {
    int h = UI_SLIDER_H;
    int fi = ui_reg(v, x, y, w, h, 1);
    int focused = ui_focus_on && fi >= 0 && fi == ui_focus_i;
    float old = *v;

    UiCap *c = ui_cap_for(v);
    if (c) {
        int px = c->released ? c->rx : c->cx;
        *v = clamp((px - x) / (float)w, 0, 1);
    }
    if (focused && ui_adj != 0) *v = clamp(*v + ui_adj, 0, 1);

    int hot = c != 0 || ui_hover(x, y, w, h);
    rectfill(x, y + 1, w, h - 3, UI_COL_BG);
    rectfill(x, y + 1, (int)(*v * w), h - 3, hot ? UI_COL_FILL_HOT : UI_COL_FILL);
    if (label) print(label, x + 3, y + 2, hot ? UI_COL_TEXT_HOT : UI_COL_TEXT);
    if (focused) ui_ring(x - 2, y - 1, w + 4, h + 1);
    return *v != old;
}

// rotary knob editing *v in 0..1; x,y = CENTER. Relative vertical drag
// (UI_KNOB_DRAG_PX for a full sweep); hover + wheel = fine-tune.
// Returns 1 if *v changed this frame.
static int ui_knob(float *v, int x, int y, const char *label) {
    int r = UI_KNOB_R;
    int fi = ui_reg(v, x - r, y - r, 2 * r + 1, 2 * r + 1, 1);
    int focused = ui_focus_on && fi >= 0 && fi == ui_focus_i;
    float old = *v;

    UiCap *c = ui_cap_for(v);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = clamp(c->v0 + (c->by - py) / (float)UI_KNOB_DRAG_PX, 0, 1);
    }
    int hot = c != 0 || ui_hover(x - r, y - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0)
        *v = clamp(*v + mouse_wheel() * UI_WHEEL_STEP, 0, 1);
    if (focused && ui_adj != 0) *v = clamp(*v + ui_adj, 0, 1);

    circfill(x, y, r, UI_COL_BG);
    circ(x, y, r, hot ? UI_COL_TEXT_HOT : UI_COL_FRAME);
    float a = 225.0f - *v * 270.0f;            // min 7 o'clock → max 5 o'clock
    line(x, y, x + (int)(cos_deg(a) * (r - 2)), y - (int)(sin_deg(a) * (r - 2)),
         hot ? UI_COL_FILL_HOT : UI_COL_TEXT);
    if (label) print(label, x - text_width(label) / 2, y + r + 3,
                     hot ? UI_COL_TEXT_HOT : UI_COL_TEXT);
    if (focused) ui_ring(x - r - 2, y - r - 2, 2 * r + 5, 2 * r + 5);
    return *v != old;
}

// momentary button; returns 1 the frame it activates (contact lifted inside,
// or A while focused). Pressed visual while a contact holds it.
// Identity is the RECT, not the label — labels may be dynamic (str(),
// toggle text); whatever is drawn at the pressed rect is what you pressed.
static int ui_button(int x, int y, int w, int h, const char *label) {
    void *wid = (void *)(uintptr_t)(0x10000001u + x * 131071 + y * 257 + w * 31 + h);
    int fi = ui_reg(wid, x, y, w, h, 1);
    int focused = ui_focus_on && fi >= 0 && fi == ui_focus_i;
    int activated = 0, pressed = 0;

    UiCap *c = ui_cap_for(wid);
    if (c) {
        if (c->released)
            activated = ui_in(c->rx, c->ry, x - UI_HIT_PAD, y - UI_HIT_PAD,
                              w + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD);
        else
            pressed = ui_in(c->cx, c->cy, x - UI_HIT_PAD, y - UI_HIT_PAD,
                            w + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD);
    }
    if (focused && ui_activate) { activated = 1; pressed = 1; }

    int hot = c != 0 || ui_hover(x, y, w, h);
    rectfill(x, y, w, h, pressed ? UI_COL_FILL_HOT : UI_COL_BG);
    rect(x, y, w, h, hot ? UI_COL_TEXT_HOT : UI_COL_FRAME);
    if (label) print(label, x + (w - text_width(label)) / 2 + (pressed ? 1 : 0),
                     y + (h - 6) / 2 + 1 + (pressed ? 1 : 0),
                     pressed ? UI_COL_TEXT_HOT : UI_COL_TEXT);
    if (focused) ui_ring(x - 2, y - 2, w + 4, h + 4);
    return activated;
}

#endif // UI_H
