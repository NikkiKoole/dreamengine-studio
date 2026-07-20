// ui.h — cross-input widgets (button · sprite-button · slider · knob), cart-land.
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
//       if (ui_spr_button(SPIN_ICON, 110, 3, 18, 18, spin)) spin = !spin;  // sprite face + toggle
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
//   ui_loupe(1)       — opt-in MAGNIFIER for fat fingers on a phone: a corner
//                       handle you drag to summon a 3x lens over the tiny
//                       widgets; a 2nd finger edits the magnified copy, grab a
//                       knob and drag out and it keeps turning. THE answer to
//                       "my knobs/sliders are too small to hit on mobile". One
//                       line; shows the whole scene (built on zoom_rect). Tune
//                       with UI_LOUPE_ZOOM / _W / _H / _HANDLE. See
//                       docs/design/loupe-notes.md.
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
#include "lay.h"              // Box + lay_* — ui_knob_cell/ui_button_cell size to a lay.h cell (guarded; safe to also include lay.h in the cart)
#include <stdint.h>
#ifdef DE_TRACE
#include <stdio.h>          // ui_begin()/ui_end() tripwire warning (debug builds only)
#endif

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
#ifndef UI_LOUPE_ZOOM
#define UI_LOUPE_ZOOM 3.0f    // magnification inside the lens
#endif
#ifndef UI_LOUPE_W
#define UI_LOUPE_W 120        // lens panel size, canvas px
#endif
#ifndef UI_LOUPE_H
#define UI_LOUPE_H 84
#endif
#ifndef UI_LOUPE_HANDLE
#define UI_LOUPE_HANDLE 20    // corner summon handle, square px
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
    int   cx, cy;       // contact position right now (loupe-mapped if inside the lens)
    int   rx, ry;       // where it lifted (valid when released; loupe-mapped)
    int   by;           // knob: y where v0 was snapped
    float v0;           // knob: value at grab
    int   has_v0;       // knob baseline snapped
    int   released;     // contact lifted; widget gets one frame to react
    int   in_lens;      // was this contact inside the lens last frame (re-anchor edge)
} UiCap;

typedef struct { int id, x, y, over; } UiPress;  // over = press was on the lens panel

static UiWid   ui_wids[UI_MAX_WID];   static int ui_wid_n = 0;
static UiCap   ui_caps[UI_MAX_CAP];   static int ui_cap_n = 0;
static UiPress ui_press[UI_MAX_PRESS]; static int ui_press_n = 0;
static int     ui_seen[UI_MAX_SEEN];  static int ui_seen_n = 0;
static void   *ui_grab_evt[UI_MAX_EVT]; static int ui_grab_n = 0;
static void   *ui_rel_evt[UI_MAX_EVT];  static int ui_rel_n = 0;

static int   ui_frame_ct = 0;
#ifdef DE_TRACE
static int   ui_ended = 1, ui_warned = 0;   // tripwire: did ui_end() run after the last widget-drawing ui_begin()?
#endif
static int   ui_focus_on = 0;
static int   ui_focus_i = 0;
static int   ui_foc_count = 0;   // focusables registered last frame
static int   ui_foc_n = 0;       // focusables registered so far this frame
static float ui_adj = 0;         // focused-value delta this frame (left/right)
static int   ui_activate = 0;    // A pressed this frame (focused button fires)
static int   ui_hold_l = 0, ui_hold_r = 0;

// loupe — a magnifier for tiny targets (docs/design/loupe-notes.md). Off by
// default; a cart opts in with ui_loupe(1). Touch ids can be negative (desktop
// mouse = -2), so "no finger" is a sentinel no real contact can take.
#define UI_NO_FINGER 0x7fffffff
static int   ui_loupe_on  = 0;             // feature enabled by the cart
static int   ui_loupe_show = 0;            // lens currently visible (parked/dragging)
static int   ui_loupe_pos = UI_NO_FINGER;  // finger positioning the lens
static int   ui_loupe_px = 0, ui_loupe_py = 0;   // panel top-left, canvas space
static float ui_loupe_sx = 0, ui_loupe_sy = 0;   // sampled center, board space

// ── internals ────────────────────────────────────────────────────────────

static int ui_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// ── loupe internals ────────────────────────────────────────────────────────
// The engine touch pool is read raw (we do NOT warp studio.c); the loupe maps
// coordinates only inside ui.h, at the few sites that record contacts.
static int ui_loupe_active(void) { return ui_loupe_on && ui_loupe_show; }

// is a RAW canvas point physically over the lens panel?
static int ui_loupe_over(int x, int y) {
    return ui_loupe_active() &&
           ui_in(x, y, ui_loupe_px, ui_loupe_py, UI_LOUPE_W, UI_LOUPE_H);
}
// map a raw canvas point to board space when it's inside the lens (else identity)
static void ui_loupe_map(int *x, int *y) {
    if (!ui_loupe_over(*x, *y)) return;
    float cx = ui_loupe_px + UI_LOUPE_W / 2.0f, cy = ui_loupe_py + UI_LOUPE_H / 2.0f;
    *x = (int)(ui_loupe_sx + (*x - cx) / UI_LOUPE_ZOOM);
    *y = (int)(ui_loupe_sy + (*y - cy) / UI_LOUPE_ZOOM);
}
// raw position of contact id; returns 0 if not currently down
static int ui_touch_raw(int id, int *x, int *y) {
    for (int i = 0; i < touch_count(); i++)
        if (touch_id(i) == id) { *x = touch_x(i); *y = touch_y(i); return 1; }
    return 0;
}
// park the lens: sample under the finger, panel floated just above, clamped
static void ui_loupe_park(int fx, int fy) {
    ui_loupe_sx = (float)fx; ui_loupe_sy = (float)fy;
    ui_loupe_px = (int)clamp(fx - UI_LOUPE_W / 2.0f, 0, SCREEN_W - UI_LOUPE_W);
    ui_loupe_py = (int)clamp(fy - UI_LOUPE_H - 16.0f, 0, SCREEN_H - UI_LOUPE_H);
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

// loupe gesture: follow/park the positioning finger, and claim a new contact on
// the corner handle. Runs at the TOP of ui_begin (before presses are recorded)
// and reads RAW touch; the handle finger is marked seen so it never becomes a
// widget press. (Dismiss-on-tap-empty is resolved in ui_end.)
static void ui_loupe_step(void) {
    if (!ui_loupe_on) { ui_loupe_show = 0; ui_loupe_pos = UI_NO_FINGER; return; }

    if (ui_loupe_pos != UI_NO_FINGER) {
        int fx, fy;
        if (ui_touch_raw(ui_loupe_pos, &fx, &fy)) ui_loupe_park(fx, fy);
        else { ui_loupe_pos = UI_NO_FINGER; ui_loupe_show = 1; }  // lifted → park
    }

    int hx = SCREEN_W - UI_LOUPE_HANDLE - 4, hy = SCREEN_H - UI_LOUPE_HANDLE - 4;
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (ui_seen_has(id)) continue;                       // not a new contact
        int tx = touch_x(i), ty = touch_y(i);
        if (ui_in(tx, ty, hx, hy, UI_LOUPE_HANDLE, UI_LOUPE_HANDLE)) {
            ui_loupe_pos = id; ui_loupe_show = 1; ui_loupe_park(tx, ty);
            if (ui_seen_n < UI_MAX_SEEN) ui_seen[ui_seen_n++] = id;  // consume it
        }
    }
}

// draw the lens + the corner handle. Called LAST in ui_end so it overlays the
// cart. The lens content is just a magnified COPY of the canvas region around the
// sample point (zoom_rect) — so it shows EVERYTHING the cart drew (widgets,
// labels, background, cart art), crisp-pixel-doubled, for free.
static void ui_loupe_render(void) {
    if (!ui_loupe_on) return;
    if (ui_loupe_show) {
        int sw = (int)(UI_LOUPE_W / UI_LOUPE_ZOOM + 0.5f);   // sampled region size
        int sh = (int)(UI_LOUPE_H / UI_LOUPE_ZOOM + 0.5f);
        int sx = (int)(ui_loupe_sx - sw / 2.0f), sy = (int)(ui_loupe_sy - sh / 2.0f);
        // zoom_rect FIRST: it snapshots the frame-so-far, so drawing any lens
        // chrome before it would pollute the magnified copy (a black shadow under
        // the panel showed up in the lens when the panel overlapped the sample).
        zoom_rect(sx, sy, sw, sh, ui_loupe_px, ui_loupe_py, UI_LOUPE_W, UI_LOUPE_H);
        rect(ui_loupe_px, ui_loupe_py, UI_LOUPE_W, UI_LOUPE_H, UI_COL_TEXT_HOT);
        rect(ui_loupe_px - 1, ui_loupe_py - 1, UI_LOUPE_W + 2, UI_LOUPE_H + 2, UI_COL_FRAME);
    }
    int hx = SCREEN_W - UI_LOUPE_HANDLE - 4, hy = SCREEN_H - UI_LOUPE_HANDLE - 4;
    rrectfill(hx, hy, UI_LOUPE_HANDLE, UI_LOUPE_HANDLE, 4,
              ui_loupe_pos != UI_NO_FINGER ? UI_COL_FILL_HOT : UI_COL_FILL);
    rect(hx, hy, UI_LOUPE_HANDLE, UI_LOUPE_HANDLE, UI_COL_TEXT_HOT);
    int mx = hx + UI_LOUPE_HANDLE / 2, my = hy + UI_LOUPE_HANDLE / 2;
    line(mx - 5, my, mx + 5, my, UI_COL_TEXT_HOT);
    line(mx, my - 5, mx, my + 5, UI_COL_TEXT_HOT);
}

// ── frame brackets ───────────────────────────────────────────────────────

// call FIRST in draw(): tracks contacts, marks releases, reads focus keys.
// (grab/release events are NOT cleared here — grabs were pushed at the end
// of last frame's ui_end, releases get pushed below; the cart reads both
// between ui_begin and ui_end, and ui_end clears them.)
static void ui_begin(void) {
    ui_frame_ct++;
#ifdef DE_TRACE
    if (ui_wid_n > 0 && !ui_ended && !ui_warned) {   // last frame drew widgets but ui_end() never ran
        printf("[ui] WARNING: %d widget(s) drawn but ui_end() was not called — clicks won't "
               "register (only hover shows). Call ui_end() last in draw().\n", ui_wid_n);
        ui_warned = 1;
    }
    ui_ended = 0;
#endif
    ui_wid_n = 0; ui_foc_n = 0;
    ui_press_n = 0;

    ui_loupe_step();   // position the lens + consume the handle finger first

    // update captured contacts with their current position (loupe-mapped when
    // inside the lens; a contact crossing the lens edge re-anchors its knob)
    for (int c = 0; c < ui_cap_n; c++) {
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == ui_caps[c].id) {
                int x = touch_x(i), y = touch_y(i);
                int over = ui_loupe_over(x, y);
                ui_loupe_map(&x, &y);
                ui_caps[c].cx = x; ui_caps[c].cy = y;
                if (over != ui_caps[c].in_lens) {       // crossed the boundary
                    ui_caps[c].in_lens = over;
                    ui_caps[c].has_v0 = 0;              // force knob re-snap → no jump
                }
                break;
            }
    }

    // new contacts → this frame's presses (resolved against widgets in ui_end)
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (ui_seen_has(id)) continue;
        if (ui_seen_n < UI_MAX_SEEN) ui_seen[ui_seen_n++] = id;
        int x = touch_x(i), y = touch_y(i);
        int over = ui_loupe_over(x, y);
        ui_loupe_map(&x, &y);
        if (ui_press_n < UI_MAX_PRESS)
            ui_press[ui_press_n++] = (UiPress){ id, x, y, over };
    }

    // lifted contacts → mark their capture released (widget reacts this frame)
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        ui_seen_drop(id);
        for (int c = 0; c < ui_cap_n; c++)
            if (ui_caps[c].id == id && !ui_caps[c].released) {
                ui_caps[c].released = 1;
                int rx = touch_ended_x(i), ry = touch_ended_y(i);
                ui_loupe_map(&rx, &ry);            // release point in board space
                ui_caps[c].rx = rx; ui_caps[c].ry = ry;
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

#ifdef DE_TRACE
extern void de_ui_audit(int x, int y, int w, int h, int focusable);  // harness sink (studio.c)
#endif

// call LAST in draw(): routes this frame's presses to widgets, prunes captures
static void ui_end(void) {
#ifdef DE_TRACE
    ui_ended = 1;                  // tripwire: ui_end() ran this frame (see ui_begin)
#endif
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
                ui_press[p].x, ui_press[p].y, 0, 0, 0, 0, 0, 0, ui_press[p].over };
            ui_push_grab(ui_wids[best].wid);   // cart sees it NEXT frame, before the value moves
        } else if (ui_loupe_active() && !ui_press[p].over) {
            ui_loupe_show = 0;                 // tap on empty space (not the lens) → dismiss
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
    ui_loupe_render();           // lens + handle overlay, on top of the cart

#ifdef DE_TRACE
    for (int w = 0; w < ui_wid_n; w++)   // hand this frame's widget rects to the auditor
        de_ui_audit(ui_wids[w].x, ui_wids[w].y, ui_wids[w].w, ui_wids[w].h, ui_wids[w].focusable);
#endif
}

// ── cart-facing state queries ────────────────────────────────────────────

// the widgets registered SO FAR this frame: their visual rects + focusable flag,
// valid between ui_begin() and ui_end() (read it right before ui_end). Lets debug
// tooling, layout checks, or a cart's own overlay see the live UI geometry.
// Returns the count; *out points at the internal array (do not free / keep).
static int ui_get_widgets(const UiWid **out) { if (out) *out = ui_wids; return ui_wid_n; }

// opt-in keyboard/gamepad focus (sticky; call once or toggle live)
static void ui_focus(int on) { ui_focus_on = on; if (!on) ui_focus_i = 0; }

// opt-in magnifier loupe (sticky): a corner handle summons a fixed-zoom lens
// that magnifies the widgets under it for fat-finger editing on small screens.
// See docs/design/loupe-notes.md. Drag the handle to position, tap empty space
// to dismiss.
static void ui_loupe(int on) {
    ui_loupe_on = on;
    if (!on) { ui_loupe_show = 0; ui_loupe_pos = UI_NO_FINGER; }
}
// summon + park the lens programmatically over canvas point (sx,sy) — for carts
// that want their own summon affordance, or to seed a screenshot.
static void ui_loupe_at(int sx, int sy) {
    ui_loupe_on = 1; ui_loupe_show = 1; ui_loupe_park(sx, sy);
}
// true while contact `id` is physically over the lens panel (re-anchor helper)
static int ui_loupe_has(int id) {
    int x, y;
    return ui_touch_raw(id, &x, &y) && ui_loupe_over(x, y);
}

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

// true while touch-contact `id` is captured by a ui.h widget (a knob/slider/button it
// grabbed and still holds). Lets a cart's OWN pad/ribbon IGNORE fingers that started on a
// widget, so a knob drag that wanders over the pad never triggers it. Capture is seen from
// the frame after the press (ui runs at ui_end), so it covers the whole ongoing drag:
//     for (i ...touches...) { if (ui_captured(touch_id(i))) continue; ...pad reads...; }
static int ui_captured(int id) {
    for (int i = 0; i < ui_cap_n; i++)
        if (ui_caps[i].id == id && !ui_caps[i].released) return 1;
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

// rotary knob core: edit *v in 0..1, x,y = CENTER, radius r. Relative vertical drag
// (UI_KNOB_DRAG_PX for a full sweep); hover + wheel = fine-tune. Returns 1 if *v
// changed this frame. The shared body of ui_knob (fixed radius) and ui_knob_cell
// (radius from a cell); call those, not this, unless you're picking your own radius.
static int ui_knob_at(float *v, int x, int y, int r, const char *label) {
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

// rotary knob editing *v in 0..1; x,y = CENTER, fixed UI_KNOB_R radius.
static int ui_knob(float *v, int x, int y, const char *label) {
    return ui_knob_at(v, x, y, UI_KNOB_R, label);
}

// ui_knob_cell — the Box/responsive twin of ui_knob: a knob sized to its CELL. The
// radius comes from the cell HEIGHT (so it keeps a constant margin above/below as a
// chunky canvas scales the whole face up — a width-scaled knob would swell to fill a
// wide cell and eat the row); the width only CAPS it so it can't overflow a narrow
// cell. Centered horizontally, label below. Bind it to a lay_grid() / lay_cell() cell
// and the layout owns the sizing — no hand-tuned radius. Returns 1 if *v changed.
static int ui_knob_cell(Box c, float *v, const char *label) {
    float rh = c.h * 0.30f, rw = c.w * 0.42f;
    int r = (int)(rh < rw ? rh : rw);
    if (r < 4) r = 4; if (r > 14) r = 14;
    int cx = (int)(c.x + c.w / 2), cy = (int)(c.y + r + 1);
    if (label && cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    return ui_knob_at(v, cx, cy, r, label);
}

// widget identity from its RECT (labels can be dynamic; the rect is stable).
// `seed` separates widget *kinds* that might share a rect (button vs spr-button).
static void *ui_wid_hash(unsigned seed, int x, int y, int w, int h) {
    return (void *)(uintptr_t)(seed + x * 131071 + y * 257 + w * 31 + h);
}

// shared button input core: register the rect, resolve capture → press/activate,
// and focus/A. Returns 1 the frame it activates (contact lifted inside, or A while
// focused); writes the draw-state flags (any pointer may be NULL). ui_button and
// ui_spr_button_styled run this — only the drawn face after it differs.
static int ui_button_core(void *wid, int x, int y, int w, int h,
                          int *focused, int *pressed, int *hot) {
    int fi = ui_reg(wid, x, y, w, h, 1);
    int foc = ui_focus_on && fi >= 0 && fi == ui_focus_i;
    int activated = 0, prs = 0;

    UiCap *c = ui_cap_for(wid);
    if (c) {
        if (c->released)
            activated = ui_in(c->rx, c->ry, x - UI_HIT_PAD, y - UI_HIT_PAD,
                              w + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD);
        else
            prs = ui_in(c->cx, c->cy, x - UI_HIT_PAD, y - UI_HIT_PAD,
                        w + 2 * UI_HIT_PAD, h + 2 * UI_HIT_PAD);
    }
    if (foc && ui_activate) { activated = 1; prs = 1; }

    if (focused) *focused = foc;
    if (pressed) *pressed = prs;
    if (hot)     *hot = c != 0 || ui_hover(x, y, w, h);
    return activated;
}

// momentary button; returns 1 the frame it activates (contact lifted inside,
// or A while focused). Pressed visual while a contact holds it.
// Identity is the RECT, not the label — labels may be dynamic (str(),
// toggle text); whatever is drawn at the pressed rect is what you pressed.
static int ui_button(int x, int y, int w, int h, const char *label) {
    void *wid = ui_wid_hash(0x10000001u, x, y, w, h);
    int focused = 0, pressed = 0, hot = 0;
    int activated = ui_button_core(wid, x, y, w, h, &focused, &pressed, &hot);
    rectfill(x, y, w, h, pressed ? UI_COL_FILL_HOT : UI_COL_BG);
    rect(x, y, w, h, hot ? UI_COL_TEXT_HOT : UI_COL_FRAME);
    if (label) print(label, x + (w - text_width(label)) / 2 + (pressed ? 1 : 0),
                     y + (h - 6) / 2 + 1 + (pressed ? 1 : 0),
                     pressed ? UI_COL_TEXT_HOT : UI_COL_TEXT);
    if (focused) ui_ring(x - 2, y - 2, w + 4, h + 4);
    return activated;
}

// ui_button_cell — the Box twin of ui_button: fill a whole cell, no px maths. Bind
// it to a lay_grid()/lay_cell()/lay_split() Box and the layout owns the sizing.
static int ui_button_cell(Box c, const char *label) {
    return ui_button((int)c.x, (int)c.y, (int)c.w, (int)c.h, label);
}

// Sprite-button style — per-state colours so a cart can keep its own toolbar look
// while sharing the widget's input handling. Any colour -1 = "skip that draw":
//   bg/bg_sel  panel fill (normal / when selected; bg_sel -1 → reuse bg)
//   frame/frame_hot/frame_sel  1px border (normal / hovered-or-pressed / selected; -1 = no border)
//   halo_sel   an extra 1px ring just OUTSIDE the rect when selected (-1 = none; boom's yellow halo)
typedef struct { int bg, bg_sel, frame, frame_hot, frame_sel, halo_sel; } UiSprStyle;

// ui_spr_button_styled — the full sprite-faced button: ui_button's press/capture/hit-pad/
// focus machinery, a 16×16 sprite `slot` centred in (x,y,w,h), coloured by `st` keyed off
// selected > hover/pressed > normal. `selected` is the caller's sticky toggle/active state.
// Returns 1 the frame it's activated (tap/click/focused-A). Set colorkey() for the sprite's
// transparency as usual; pal() before the call to recolour the glyph itself per state.
static int ui_spr_button_styled(int slot, int x, int y, int w, int h, int selected, UiSprStyle st) {
    void *wid = ui_wid_hash(0x10000002u + slot * 7, x, y, w, h);
    int focused = 0, pressed = 0, hot = 0;
    int activated = ui_button_core(wid, x, y, w, h, &focused, &pressed, &hot);
    int bg = (selected && st.bg_sel >= 0) ? st.bg_sel : st.bg;
    if (bg >= 0) rectfill(x, y, w, h, bg);
    spr(slot, x + (w - 16) / 2, y + (h - 16) / 2);   // 16 = sprite slot size (spr is 16×16)
    int fr = selected ? st.frame_sel : ((hot || pressed) ? st.frame_hot : st.frame);
    if (fr >= 0) rect(x, y, w, h, fr);
    if (selected && st.halo_sel >= 0) rect(x - 1, y - 1, w + 2, h + 2, st.halo_sel);
    if (focused) ui_ring(x - 2, y - 2, w + 4, h + 4);
    return activated;
}

// ui_spr_button — the convenience default: a sprite-faced toggle button in the shared UI
// palette (selected = lit "hot" fill). For a custom toolbar look pass your own colours via
// ui_spr_button_styled. Returns 1 the frame it's activated — toggle your flag on that:
//   if (ui_spr_button(SPIN_ICON, 110, 3, 18, 18, spin_on)) spin_on = !spin_on;
static int ui_spr_button(int slot, int x, int y, int w, int h, int selected) {
    UiSprStyle st = { UI_COL_BG, UI_COL_FILL_HOT, UI_COL_FRAME, UI_COL_TEXT_HOT, UI_COL_TEXT_HOT, -1 };
    return ui_spr_button_styled(slot, x, y, w, h, selected, st);
}

#endif // UI_H
