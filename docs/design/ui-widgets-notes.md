# ui.h — a small cross-input widget kit (button · slider · knob · panel · drag-from)

> **Genre: design exploration (scratchpad).** Proposal for a cart-land library
> header (`runtime/ui.h`, the `gestures.h`/`improv.h` pattern — all `static`,
> zero engine API, no tcc-symbol regen) giving carts a tiny set of UI elements
> that work for **mouse, touch, and keyboard/keypad/gamepad** at once. Status →
> [`../STATUS.md`](../STATUS.md) (open item 25). Prereq for trusting it on
> phones: the web phantom-touch fix, [`touch-notes.md`](touch-notes.md) §7.

---

## 1. The evidence — carts keep re-rolling the same five things

- **Knob/slider drag**: `modrack` (`held_knob`/`drag_y`), `sh101`
  (`drag_id`/`drag_m0`/`drag_v0`), `tb303` (five knobs), `sfx generator`
  (**17 sliders** — the dream customer), `dream synth`, `wave editor` — each
  duplicates the same claim-on-press / delta-while-held / release state
  machine, mouse-only.
- **Buttons**: `touchlab`'s TAPS button, `multitouch`'s CLEAR, every sound-tool
  cart's preset rows — literal rect consts + `tapp()`/`mouse_pressed()` + a
  hand-drawn rect, no pressed-state visual, no focus.
- **mobile-lint** already flags the failure modes this kit must design against:
  tiny targets, hover-only affordances, wheel-only fine-tuning, mouse-only
  carts ranking 🟠 on phones.
- **Keyboard/gamepad users get nothing**: not one of the knob carts can be
  driven without a pointer.

Per the house instinct (ADR-0006, the rgestures never-wrap verdict): prove the
shape in cart-land first. If half the music carts adopt it, *then* talk about
engine surface.

## 2. The foundation is already laid — one pointer model

The **virtual touch pool** (touch-notes §1) merges mouse and touch: desktop
mouse LMB is a synthetic touch (id `-2`), so a widget written against
`touch_count()/touch_id()/tapp()/tapr()` is mouse-and-touch capable **for
free**. That's the load-bearing design decision and it's already shipped.
What's left on top:

| input | covered by | needs designing |
|---|---|---|
| touch | vt pool | fat-finger hit targets; per-finger capture |
| mouse | vt pool (id −2) | hover + wheel as *enhancements*, never requirements |
| keyboard/keypad/gamepad | `btn()`/`key()` | a **focus model** (ring, traversal, adjust keys) |

## 3. The four real design problems

**3a. Capture.** A control claims a contact (touch id or the mouse) on press
and follows it until release — *even outside its rect* (drag up off a knob for
fine control; the modrack/sh101 pattern, generalized). Per-id capture means two
fingers turn two knobs simultaneously (the touch-piano precedent — this is
where one-global-gesture designs die). One small capture table inside ui.h:
`{contact id → widget id}`.

**3b. Identity.** Immediate-mode needs stable widget identity across frames.
In C the natural id is **the address of the value the widget edits**:
`ui_knob(&cut_v, …)` — unique, zero bookkeeping, survives layout moves.
Buttons (no value) use label pointer + rect, or an explicit id variant if a
cart hits a collision. No string hashing, no id stacks — this is a fantasy
console, not dear imgui.

**3c. Hit target ≠ visual size.** Touch needs ≥ ~24×24 canvas px (the
mobile-lint threshold) but a knob can *look* 12px. Every widget hit-tests an
inflated rect (`UI_HIT_PAD`, default a few px, tunable per the gestures.h
`#define`-before-include convention) while drawing the compact visual. Only
the pointer paths use inflation; focus traversal doesn't care.

**3d. Focus, for the pointer-less.** A focus ring traversed in **registration
order** (= call order, rebuilt every frame — natural for immediate mode):
P0 d-pad / arrows move focus, A (Z) activates a button, left/right (or
up/down) adjust a focused knob/slider with acceleration on hold. Driven by
`btn()`/`btnp()` so gamepad works the day item 7 (gamepad) lands. Focus is
opt-in per cart (`ui_focus(true)`) so a pure-touch cart pays nothing and a
cart's own d-pad gameplay isn't stolen. Focus visual: a 1px `CLR_WHITE`
marching rect — readable on any background, no theme machinery.

## 4. API sketch (v1: button, slider, knob, panel, drag-from)

Widgets are called from **`draw()`** — they need to both read input and put
pixels down, and input state is stable across the whole frame. (Carts that
must react in `update()` read the value the widget edited last frame; for
knobs/sliders that's already the natural data flow.)

```c
#include "ui.h"

static float cut = 0.5f, vol = 0.8f;   // widgets edit floats in 0..1

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();                                   // frame setup, focus input

    if (ui_button(10, 180, 52, 16, "play")) toggle_play();

    ui_slider(&vol, 10, 10, 100, "vol");          // horizontal track, w=100
    ui_knob(&cut, 90, 170, "cut");                // vertical drag; wheel = fine

    ui_panel(120, 8, 80, 60, "mixer");            // titled frame + layout cursor
    // (panel = visual frame + an origin the next widgets can be placed
    //  relative to — NOT windowing/z-order/drag-to-move; that's v2 at most)

    if (ui_dragfrom(200, 10, 16, 16, SPR_CRATE)) {        // true while dragging
        // ghost sprite follows the contact automatically
        if (ui_dropped()) place_crate(ui_drop_x(), ui_drop_y());
    }

    ui_end();                                     // focus ring, capture cleanup
}
```

- Values are normalized `0..1` floats; mapping to Hz/semitones/etc. stays at
  the use site (the sh101 convention — keeps the widget dumb).
- Return of `ui_button` = activated this frame (tap released inside, or A on
  focus). Press-visual handled internally.
- `ui_knob`: vertical drag relative to grab point; `mouse_wheel()` fine-tune
  *when a mouse is around* (enhancement, never the only path); focused
  left/right steps.
- `ui_dragfrom` is the "drag-from thing": a source rect that spawns a payload
  you drag and drop — the modrack jack-patching / monstermix part-assembly
  interaction, generalized.
- Screen-space only: widgets assume no `camera()` offset (document: draw UI
  after `camera(0,0)`, like HUDs already do).
- Theme = a handful of `#define UI_COL_*` palette indices, overridable before
  the include. No style structs.

## 5. Hygiene — how this ships (our own rules, applied)

1. **Cart-land header first** (`runtime/ui.h`), engine API never-or-later.
   Same contract as gestures.h: everything `static`, include-able by any cart,
   tunables via `#define` before the include, no `studio_tcc_symbols` impact.
2. **Second-customer rule.** Ships only with (a) a **`uikit` showcase cart** —
   every widget live, all three input modes demoed on one screen, doubling as
   the device probe (`kind: ["probe", "tech-demo"]`) — and (b) **one real
   retrofit**: `sfx generator` (17 hand-rolled sliders → `ui_slider`, biggest
   deletion-to-value ratio) or one modrack knob row. If the retrofit fights
   the API, the API is wrong — fix before shipping.
3. **Probe before polish.** uikit goes through the touchlab treatment on a real
   phone (fat fingers, 5-touch ceiling, two-knobs-at-once) before any widget
   beyond the v1 five is added. Verdicts → [`probe-carts.md`](probe-carts.md).
4. **mobile-lint should love the result**: a cart whose input is all ui.h
   widgets should lint 🟢 touch-ready by construction. If it doesn't, either
   the lint or the kit is lying — reconcile.
5. **Blocked-by**: web builds need the phantom-touch fix first
   ([`touch-notes.md`](touch-notes.md) §7) — a capture model is exactly where a
   stuck contact hurts most (a knob grabbed by a ghost finger never releases).

## 6. Deliberately NOT in v1

- Windowing (drag/resize/z-order panels), modal dialogs, scroll regions.
- Text fields — mobile web can't do native text input anyway
  ([`mobile-web-notes.md`](mobile-web-notes.md) §6c); a tappable `ui_keyboard`
  could be a v2 widget *built from* `ui_button`, which is the right test of
  composability.
- Toggle/checkbox/radio — `ui_button` + cart-side bool covers it; add only if
  carts keep writing the same three lines.
- Layout engines, anchoring, autosizing. Coordinates are literals; this is the
  PICO-8 lane.
