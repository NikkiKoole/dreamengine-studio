# loupe — a magnifier for tiny touch targets (design notes)

Status: **scoping** (2026-06-10). POC shipped (`tools/carts/loupe.c`, the "Loupe"
tech-demo cart); the `ui.h` work below is not built yet. **Decision: the loupe is
a `ui.h`-only feature** — a coordinate remap inside `ui.h`, no engine change. The
"engine input-warp" idea was scoped and **rejected** after looking at the carts
(see §1 and §9); don't re-litigate it without re-reading §1.

## 0. The problem

Desktop-designed control panels — a drum machine's step grid, sfxgen's 17
sliders, sh101's 13 faders + keybed — are fine with a mouse and brutal on a phone
in portrait. The layout is fixed by the hardware-replica design; it can't reflow.
The player needs to hit a 14px target with a fat finger without redesigning every
cart.

The POC's answer (validated on a real phone, 2026-06-10): a **loupe**. Drag a
corner handle and a fixed-zoom rectangular lens appears above your finger,
re-rendering whatever it's over, magnified. Lift to park it. A *second* finger
reaches into the lens and edits the fat version; the lens blocks touches
underneath so the real tiny widgets are safe. Grab a control inside the lens and
you can drag right out of it and keep going — the grab already landed.

(Name note: "loupe" is also iOS's native long-press magnifier, which we suppress
shell-wide; this is the in-app, interactive version of the same idea.)

## 1. What the loupe is FOR — and what it is not for

A loupe is for **precision single-target acquisition**: park it, aim at one small
control, hit it bigger. That framing decides everything below.

The first instinct was the opposite — "put the magnify transform as low as
possible so it reaches every input path." `ui-widgets-notes.md §9` settles that
dense touch UIs use **three** tools, all on the same virtual-touch pool:

| Tool | Used by (tiny-target carts) | Loupe fit |
|---|---|---|
| `ui.h` widgets (capture) | sfxgen sliders, synth/sh101 knobs & faders | **YES** — precision targets |
| `tapp()`/`tapr()` (discrete hits) | drummachine/mt70 transport, preset chips | maybe (discrete cells) |
| per-finger pool / `ui_surface` | **sh101 keybed, drummachine paint-grid** | **NO** — see below |

So I argued the warp had to sit *below* `ui.h`, in the engine, to also catch
`tapp` and surfaces. **Looking at the actual carts killed that argument** (§1.1).
The flaw: I assumed "dense surface ⇒ needs a loupe." It doesn't, for two reasons:

1. **A loupe shows a *slice*; a surface is *swept*.** You play a keybed by sliding
   across its full width (glissando); you fill a drum pattern by dragging across
   cells (paint). A lens showing one zoomed slice can't express either — you can't
   glissando an octave or paint a run inside a small window. **The loupe is the
   wrong tool for a surface**, independent of whether the surface *could* be
   `ui.h` (it can't — capture is the opposite of hand-over, `ui-widgets-notes.md
   §9`).
2. **Surfaces aren't even the acute tiny-target pain.** Keybed keys are *narrow
   but tall* (62–146px) — a tall strip is easy to hit; you just need the right x.
   The genuinely fiddly targets are the things you dial *precisely*: sh101's 14px
   faders and 28px knobs. Those are **value editors — `ui.h`-shaped already.**

Conclusion: the loupe's niche is precision targeting of small, scattered value
editors on a fixed panel. That is `ui.h` territory. The input paths it would
"miss" (`tapp` grids, surfaces) either don't want a loupe at all (surfaces) or are
the soft, discrete-tap edge. **No engine warp is justified.**

### 1.1 The cart evidence (2026-06-10)

Read `drummachine.c`, `mt70.c`, `sh101.c` to ground the decision in fact, not the
§9 taxonomy:

| Cart | Smallest target | Tool | True hand-over surface? |
|---|---|---|---|
| **drummachine** | grid cell 15×18 (17×21 stride) | `touch_*` | **Yes** — drag paints each cell, per-finger paint state (`drummachine.c:173`) |
| **mt70** | black key 16×62, white 19–21×112 | `touch_*` | **Yes** — slide = glissando, note hands over (`mt70.c:328`) |
| **sh101** | black key 13×90 (surface); **MOD fader 14×74 (value editor)** | `touch_*` | keybed **yes** (`sh101.c:842`); faders **no** — capture (`sh101.c:463`) |

Both halves verified: the surfaces are *genuine* hand-over (not discrete taps
mislabeled), so they can't be `ui.h`; **and** they're swept, so they don't want a
loupe. The acute precision pain (sh101's 14px faders/knobs) is `ui.h`-shaped.

### 1.2 Surfaces want pinch-zoom, not a loupe (out of scope here)

The right mobile aid for a swept surface is **whole-view zoom** — enlarge the
whole grid/keybed, then sweep it at size. The engine already has the parts:
`gestures.h` `pinch_scale()` (two-finger pinch → per-frame scale factor) feeding
`camera_ex(x,y,zoom,angle)`. **But adoption is thin: only `modrack` uses it
today** (`modrack.c:1398` — accumulate, step at ±30%, anchor at the pinch
midpoint); `touchlab` only tests the gesture. So retrofitting pinch-zoom onto
`drummachine`/`mt70`/`sh101` is *available and proven*, not done — and it's a
**separate, optional** effort, not part of the loupe. Recorded here only so the
loupe doesn't grow to cover surfaces.

## 2. The two concerns (both stay in/around `ui.h`)

**(A) Input mapping** — the core. A contact physically inside the lens panel
reports the *sampled canvas coordinate* instead of its raw position. Inverse of
the render transform: a raw canvas point `(rx,ry)` inside the panel maps to
`bx = sampleX + (rx - panelCx)/zoom`, `by = sampleY + (ry - panelCy)/zoom` (the
POC's `board_of()`). In `ui.h` this is applied at the handful of sites where it
reads a raw contact coordinate (§3).

**(B) Magnified rendering** — cart-side, but `ui.h` can largely own it for
`ui.h`-only carts: it knows every widget drawn this frame (`ui_wids[]`), so it can
re-draw those widgets scaled into the panel. A mixed cart (own background + a few
`ui.h` widgets) gets the widgets magnified on a neutral panel; cart-drawn
decoration outside the widgets won't appear in the lens. Acceptable for v1 — the
loupe's customers are *widget panels* (§1).

## 3. The `ui.h` change (where the remap lives)

`ui.h` already reads raw contacts at four sites in `ui_begin`/`ui_end`
(`runtime/ui.h`): updating captured contacts' `cx,cy` (l.237–244), recording new
presses (l.247–253), recording released positions `rx,ry` (l.256–265), and the
insurance-release path (l.268–278). A single `ui_loupe_map(int *x, int *y)`
applied at each turns presses, `UiCap.cx/cy`, and `UiCap.rx/ry` into board space —
after which **every existing widget works unchanged**: deferred press resolution
lands on the right tiny widget, absolute sliders read the mapped x, relative knobs
read the mapped y, button release-hit tests in mapped space.

Cart API (provisional): the cart drives the lens each frame —

```c
ui_loupe(panelX, panelY, panelW, panelH, sampleX, sampleY, zoom); // active lens
ui_loupe_off();                                                   // parked/hidden
int ui_loupe_has(int contact_id);   // is this contact inside the lens right now?
```

`ui.h` draws the lens chrome + the magnified widget pass in `ui_end()` (after all
widgets are registered). The handle + summon/park/dismiss gesture can live in
`ui.h` too (it owns contact tracking) or stay cart-side; **open** (§5).

## 4. Subtleties the POC already solved — `ui.h` inherits them

- **Knob re-anchor at the lens edge.** Relative drags (knobs) read an absolute
  baseline; when a captured contact crosses *out* of the lens the mapped→raw
  coordinate flips and the value jumps. The POC re-baselines on the crossing
  (`wasIn` compare → reset `v0`/`by`). In `ui.h`: when a captured contact's
  in-lens state (`ui_loupe_has`) toggles, re-snap the knob's `by`/`v0`. Absolute
  sliders don't need it (dragging an absolute control out of the lens is
  meaningless with or without a loupe).
- **Chrome vs mapped coords.** The handle/panel hit-test in *raw* space while
  everything else maps; the lens lifecycle must run (and set the lens) *before*
  the frame's widget reads.
- **`NO_FINGER` sentinel, not `-1`.** Touch ids can be negative — the desktop
  mouse is id `-2` (`MOUSE_TOUCH_ID`). The POC's first bug was a `posFinger >= 0`
  guard silently disabling drag-tracking for the mouse.

## 5. Open questions

1. **Handle ownership** — `ui.h`-owned corner handle (zero cart code) vs.
   cart-drawn handle calling `ui_loupe(...)`. Leaning `ui.h`-owned with a
   tunable corner.
2. **Mixed-cart rendering** (§2B) — is "widgets-only, neutral panel bg" enough, or
   do some target carts need their own background in the lens? Decide against the
   real §6 customer.
3. **Summon affordance** — corner handle (POC, most discoverable) vs. held-tap.
   Revisit on device.

## 6. Second customers (the §5-of-ui-widgets-notes ship rule)

The loupe ships when proven on a real `ui.h` panel, not just the POC. Best target:
**sfxgen** (17 sliders at 9px pitch) or an sh101-style **fader/knob panel** (the
14px faders are the canonical fat-finger pain). If retrofitting fights the API,
the API is wrong (the §5.2 rule).

## 7. Phasing

1. **`ui.h` input remap** — `ui_loupe`/`_off`/`_has` + `ui_loupe_map` at the four
   read-sites. Verify with the input harness: a scripted drag inside a scripted
   lens lands on the mapped widget (`--trace` assert, like ui.h's §7).
2. **Lens chrome + magnified widget pass** in `ui_end`; handle + gesture.
3. **Knob re-anchor** on `ui_loupe_has` toggle.
4. **Second customer** retrofit (§6) + device pass. Re-point the POC cart's
   hand-rolled lens at the `ui.h` version (or keep the POC standalone as the
   teaching demo — decide then).
5. **Docs** — `cart-authoring.md` library-header table row, `mobile-web-notes.md`
   cross-link, STATUS open-item.

## 8. Tunables

`#define`-before-include, matching `ui.h` style: `UI_LOUPE_ZOOM` (3),
`UI_LOUPE_W`/`_H`, handle corner.

## 9. Why not the alternatives (recorded for the next agent)

- **Engine input-warp primitive** (`touch_lens` warping the shared vt-pool, so
  `tapp`/surfaces get it too) — the original recommendation, **rejected**. The
  cart evidence (§1.1) showed the only input paths it would reach beyond `ui.h`
  are *surfaces*, and surfaces are swept, not precision-targeted, so they don't
  want a loupe at all (§1, §1.2). The warp would have been universal reach built
  for customers that don't want the feature, at the cost of touching the engine
  and crossing toward the ADR-0006 line. If a future cart genuinely needs to
  *precision-target* a non-`ui.h` control through a lens, reopen this — but the
  three densest carts today don't.
- **`loupe.h` opt-in `loupe_map(&x,&y)`** (cart-land, every consumer wires it by
  hand) — not transparent, and made moot once the reach shrank to `ui.h` only.
