# ui.h — a small cross-input widget kit (button · slider · knob · panel · drag-from)

> **Genre: design exploration → build record.** Proposal for a cart-land library
> header (`runtime/ui.h`, the `gestures.h`/`improv.h` pattern — all `static`,
> zero engine API, no tcc-symbol regen) giving carts a tiny set of UI elements
> that work for **mouse, touch, and keyboard/keypad/gamepad** at once. Status →
> [`../STATUS.md`](../STATUS.md) (open item 25). The phone prereq (web
> phantom-touch, [`touch-notes.md`](touch-notes.md) §7) cleared 2026-06-06.
>
> **v1 SHIPPED 2026-06-07** — `runtime/ui.h`: `ui_begin/ui_end` + **button /
> slider / knob** + per-contact capture + hit-pad inflation + opt-in focus
> ring + `ui_grabbed`/`ui_released` events. **Panel and drag-from were cut
> from v1** (user call, the second-customer rule applied per-widget — §4's
> dragfrom customers were speculative; they wait for a real retrofit that
> wants them). Shipped per §5: `uikit` showcase/probe cart + the `sfxgen`
> retrofit. Build learnings → §7 below; device-probe half still open
> ([`probe-carts.md`](probe-carts.md)).

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

- **Panel and drag-from** *(moved here from §4 at ship time)* — the §4 sketch
  had five widgets, but judged by our own per-widget second-customer standard,
  `ui_dragfrom`'s named customers were speculative (modrack jack-patching is
  not a planned retrofit; monstermix composites at *bake* time) and `ui_panel`
  had none. They ship when a real cart demands them.
- Windowing (drag/resize/z-order panels), modal dialogs, scroll regions.
- Text fields — mobile web can't do native text input anyway
  ([`mobile-web-notes.md`](mobile-web-notes.md) §6c); a tappable `ui_keyboard`
  could be a v2 widget *built from* `ui_button`, which is the right test of
  composability.
- Toggle/checkbox/radio — `ui_button` + cart-side bool covers it; add only if
  carts keep writing the same three lines.
- Layout engines, anchoring, autosizing. Coordinates are literals; this is the
  PICO-8 lane.

## 7. Build learnings (v1, 2026-06-07) — what the retrofit taught the API

The `sfxgen` retrofit (17 sliders + 11 buttons) was run as the §5.2 test —
"if the retrofit fights the API, the API is wrong" — and it found two fights,
both resolved API-side:

- **Event timing: grab must fire *before* the first value change.** sfxgen's
  undo (`remember()`) needs the *pre-drag* param set, but sliders are
  absolute — the press itself jumps the value. Fix: presses are resolved at
  `ui_end()` and the grab event is pushed *there*, one frame before the
  widget applies the jump. A cart that checks `ui_grabbed(&v)` **above** the
  widget call snapshots clean pre-drag state; `ui_released` is symmetric
  (pushed in `ui_begin`, checked after the call so the final position has
  landed). Verified: scripted drag → undo restores the exact pre-press value.
- **Button identity is the RECT, not the label pointer.** sfxgen's SPD button
  labels with `str("SPD %dms", …)` — a rotating buffer whose pointer changes
  every frame, which would orphan the capture mid-press under §3b's
  label-pointer scheme. Rect-only identity is also just *truer* for immediate
  mode: whatever is drawn at the pressed rect is what you pressed. (Value
  widgets keep the value-address identity exactly as §3b designed.)
- **Deferred press resolution doubles as the dense-layout answer.** Presses
  collected in `ui_begin`, resolved against *all* widgets at `ui_end`
  (visual-rect hit beats inflated hit; nearest center among inflated
  candidates): sfxgen's 17 rows at 9px pitch route correctly even though
  every row's inflated target overlaps its neighbours. Cost: captures start
  one frame late — invisible at 60fps, and a sub-frame tap still lands
  (press resolves at N, release marked at N+1, button activates at N+1).
- **mobile-lint §5.4 contract enforced from the lint side:** the lint now
  inlines quote-included `runtime/` headers before scanning (skipping
  `studio.h` — declarations would match every regex), so `uikit`/`sfxgen`
  rank **touch-ready by construction** instead of keyboard-only/tap-as-mouse.
- Verified by harness, not by hand: handcrafted `.rec` replays drive the vt
  pool (the injected pointer becomes the synthetic touch), so slider
  press-jump/drag/release, knob relative drag + clamp, button
  release-activation, capture counts, focus traversal (F → arrows →
  LEFT/RIGHT adjust) are all asserted from `--trace` output. The remaining
  unverifiable-by-replay piece is real multi-touch (two knobs, two fingers) —
  that's the device probe.

## 8. v2 candidates — what the sh101 retrofit taught (2026-06-07, same day, parallel)

Context: hours before v1 shipped, sh101 grew the most advanced *hand-rolled*
version of this exact layer (per-finger pointer table, grab zones tiling each
panel section, a `claim_tap` ordering guard, slide-legato key handover —
commit 60b1f02). Convergent evolution, and a free design review in both
directions. What v1 already does better: deferred `ui_end()` press resolution
**obsoletes sh101's `claim_tap`** (tap targets inside a knob's inflated zone
route correctly because visual-rect hits beat inflated hits — no call-order
discipline needed). What sh101 exposes as v2 gaps, each with named customers:

### 8a. Vertical fader — the synth-panel staple

sh101 has 13 of them; modrack, tb303, the dream synth and every future
tinyjam rack lane want the same shape. Not just `ui_slider` rotated — it
drags in y, reads bottom-up, and carries the feel question below.

### 8b. The slider-feel question (OPEN — decide before the fader ships)

v1 sliders are **absolute** (press sets the value at the press point, sfxp
style); sh101's faders are **relative** (drag from wherever you grabbed —
the tb303 fine-control style v1's *knob* already uses). The tension:

- **Absolute** is instantly legible and great for coarse "set it about here"
  sliders (sfxgen's 17 rows). But on a dense hardware-replica panel a sloppy
  grab **teleports the value** — and the inflated hit target makes this
  *worse*: a fat-finger press that lands in the inflation zone *off* the
  track end jumps the value to an extreme. The very feature that makes dense
  layouts grabbable makes absolute jumps more violent.
- **Relative** never teleports, feels like hardware (the cap is a physical
  thing you push), and fine control falls out of drag distance. Costs:
  coarse moves take a longer drag, and a tap-without-drag does nothing
  (which a player may read as broken).
- **Middle paths to weigh:**
  1. the DAW convention — *absolute when the press lands on the track,
     relative when it lands on the cap* (grab the handle = push it; tap the
     scale = jump there);
  2. a per-widget flag (`ui_slider_rel()` / `UI_SLIDER_RELATIVE`) and let
     the cart pick its feel — replica panels relative, utility panels
     absolute;
  3. absolute with slew (the value glides to the press point — softens the
     teleport but adds hidden state and lag).
- Whatever wins, the **vertical fader and the horizontal slider should
  answer it the same way** — one feel rule, not per-orientation surprise.

### 8c. `ui_surface` — the per-finger zone primitive

sh101's keybed, handpan's drumhead, touchpiano, drum pads, and **the XY pad
in every tinyjam rack** ([`tinyjam-racks.md`](tinyjam-racks.md) "universal layout")
are not widgets — they're *surfaces*: per-finger enter/leave/move over a
zone map, where a moving finger **hands over between zones**
(sh101's slide-legato: key_down(new) then key_up(old), so AUTO portamento
glides). Note this is the deliberate *opposite* of widget capture (a
captured contact never migrates widgets — right for knobs, wrong for
keybeds). A v2 `ui_surface` would hand the cart per-finger events
(enter/move/leave + which zone) and keep the policy cart-side. Until it
exists, sh101's pointer table is the reference recipe — don't migrate sh101
onto ui.h before this and 8a land; its hand-rolled layer is currently *more*
capable than the kit.

### 8d. Already banked

The injected-pointer→synthetic-touch harness fix (same commit) makes every
ui.h widget script/replay-drivable — §7's verification leans on it. And the
tinyjam rack chassis is the kit's next big customer: step-grid = buttons,
lane levels = vertical faders (8a), the play-pad = a surface (8c) — the
rack work order and this v2 list are the same list.

## 9. Scope boundary — `ui.h` is NOT the answer to "my cart has buttons" (settled 2026-06-07)

Two carts (`mt70`, `drummachine`) were retrofitted for touch *without* `ui.h`,
on purpose, and the reasoning needs to be written down so it's a consistent
path and not a per-agent coin-flip. The trap is reading "carts keep
re-rolling widgets → use `ui.h`" (§1) as "all on-screen controls → `ui.h`."
They don't. **The deciding fact: `ui.h`, `tapp()`, and the finger pool are
ALL built on the same virtual-touch pool, so all three are mouse + touch
capable for free.** Touch coverage is never the reason to pick one — the
*kind of control* is. Three kinds, three tools:

| Control kind | Tool | Why | Example |
|---|---|---|---|
| **Value editor** — a continuous number a finger drags, needs *capture* (keeps tracking after sliding off the widget) | **`ui.h`** (`ui_slider`/`ui_knob`) | capture + slew + the keyboard/gamepad focus ring is the machinery you must not hand-roll | sfxgen's 17 sliders, a synth's cutoff knob |
| **Discrete hit-target** — a thing you tap once; drawn in the cart's own style | **`tapp()`/`tapr()`** | no value, no capture; wrapping it in `ui_button` only forces `ui.h`'s visual + focus model onto a surface you're already drawing | a sequencer cell, a preset chip, a transport button, a piano key |
| **Surface** — a zone map where a finger *hands over between zones* as it moves (the deliberate opposite of capture, §8c) | **the per-finger pool** (`touch_id`/`touch_ended_*`); future `ui_surface` | a captured contact never migrates widgets — wrong for keybeds, drum grids, glissando, paint-drag | sh101 keybed, mt70/drummachine grids, mallet sweep |

The fuzzy edge is **buttons**: `ui_button` exists and is right *inside a `ui.h`
panel* (so the button joins the focus ring with neighbouring sliders). A
**standalone** button, or one on a custom-drawn panel, is a `tapp()` — that's
what mt70's preset/octave/transport row and drummachine's transport use, and it
keeps those carts off an include they'd otherwise touch for nothing.

One-line rule (mirrored in
[`../guides/cart-authoring.md`](../guides/cart-authoring.md) → "Touch input"):
**value → `ui.h`; hit-target → `tapp()`; drag-across → the finger pool.**
