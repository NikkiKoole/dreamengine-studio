# loupe — a magnifier for tiny touch targets (design notes)

Status: **BUILT** (2026-06-10). Two carts: the standalone POC (`tools/carts/loupe.c`,
"Loupe") and the `ui.h`-integrated demo (`tools/carts/uiloupe.c`, "ui.h loupe").

**Shape as built:**
- **Input** is a coordinate remap inside `ui.h` (the four `ui_begin` contact
  read-sites) — so a press/drag/release inside the lens lands on the real tiny
  widget. `ui.h`-only; the "engine *input*-warp" idea (warping the shared touch
  pool so `tapp`/surfaces get it too) was scoped and **rejected** (§1, §9).
- **Rendering** is `zoom_rect()` — a small *engine* primitive (an OUTPUT
  transform, distinct from the rejected input warp) that copies a canvas region
  scaled into a rect. The lens is just a magnified copy of the canvas around the
  sample point, so it shows EVERYTHING the cart drew (widgets, labels, background,
  cart art), nearest-neighbor → crisp doubled pixels. Reusable beyond the loupe
  (picture-in-picture, minimaps). A cart opts the whole loupe in with `ui_loupe(1)`.

Earlier render designs that were tried and dropped: redrawing only the `ui.h`
widgets (didn't show cart-drawn content — the orange step fills, background); and
a `camera_ex` + render-only "redraw the whole scene" callback (worked, but more
machinery in `ui.h` + the cart). `zoom_rect` is simpler than both and universal.

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
possible so it reaches every input path." [`ui-widgets-notes.md`](ui-widgets-notes.md) §9 settles that
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

**(B) Magnified rendering** — `zoom_rect()` (engine, `runtime/studio.c`). The lens
is a scaled COPY of the canvas region around the sample point — so it shows
EVERYTHING the cart drew, no widget-list bookkeeping, no cart cooperation. The
primitive snapshots the canvas to a scratch render-texture (avoiding a
read-while-bound feedback loop), then blits the region with `TEXTURE_FILTER_POINT`
→ crisp doubled pixels. **Call order matters:** `ui_loupe_render` calls `zoom_rect`
*first* (snapshots clean cart content) and draws the panel frame/handle *after* —
drawing any lens chrome before the snapshot would let it pollute the magnified copy
(a drop-shadow rect did exactly that: black lens when the panel overlapped the
sampled region). The render-texture Y-flip means the sub-region source rect is
`{sx, SCREEN_H-sy-sh, sw, -sh}`, matching the engine's own full-canvas `{0,0,W,-H}`
blit.

## 3. The `ui.h` change (where the remap lives)

`ui.h` already reads raw contacts at four sites in `ui_begin`/`ui_end`
(`runtime/ui.h`): updating captured contacts' `cx,cy` (l.237–244), recording new
presses (l.247–253), recording released positions `rx,ry` (l.256–265), and the
insurance-release path (l.268–278). A single `ui_loupe_map(int *x, int *y)`
applied at each turns presses, `UiCap.cx/cy`, and `UiCap.rx/ry` into board space —
after which **every existing widget works unchanged**: deferred press resolution
lands on the right tiny widget, absolute sliders read the mapped x, relative knobs
read the mapped y, button release-hit tests in mapped space.

Cart API (as built):

```c
static void ui_loupe(int on);          // opt the whole loupe in (handle + lens), like ui_focus
static void ui_loupe_at(int sx, int sy); // summon/park programmatically (custom affordance / seed a screenshot)
static int  ui_loupe_has(int id);      // is contact id over the lens panel? (re-anchor helper)
```

`ui.h` owns it all: the corner ⊕ handle + summon/follow/park/dismiss gesture
(reads RAW touch, runs at the top of `ui_begin`, consumes the handle finger), the
input remap above, the knob re-anchor (§4), and the lens render in `ui_end()` via
`zoom_rect`. A cart needs only `ui_loupe(1)`. Engine primitive used:
`void zoom_rect(int sx,int sy,int sw,int sh, int dx,int dy,int dw,int dh)`.

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

## 5. Resolved / open questions

1. **Handle ownership** — RESOLVED: `ui.h`-owned corner handle (zero cart code),
   `UI_LOUPE_HANDLE` tunable.
2. **Mixed-cart rendering** — RESOLVED by `zoom_rect`: the lens copies the canvas,
   so it shows the cart's full content (background, labels, art) automatically —
   the "widgets-only neutral panel" worry is moot.
3. **Summon affordance** — corner handle shipped; held-tap/two-finger still open,
   revisit on device.
4. **`zoom_rect` perf** (open) — the snapshot is a full-canvas GPU copy per loupe
   frame. Fine at 320×200; profile on the larger panels (sh101 460×300) before
   leaning on it in a hot path.

## 6. Second customers (the §5-of-ui-widgets-notes ship rule)

The loupe ships when proven on a real `ui.h` panel, not just the POC. Best target:
**sfxgen** (17 sliders at 9px pitch) or an sh101-style **fader/knob panel** (the
14px faders are the canonical fat-finger pain). If retrofitting fights the API,
the API is wrong (the §5.2 rule).

## 7. Phasing

1. **`zoom_rect` engine primitive** — DONE (`studio.c/.h`, `studioDocs.js`,
   `shell.js`, tcc symbols).
2. **`ui.h` loupe** — DONE: input remap at the four read-sites, `ui.h`-owned
   handle + gesture, knob re-anchor on `ui_loupe_has` toggle, lens render via
   `zoom_rect`. Verified by the input harness (scripted handle-drag onto a knob →
   magnified-knob drag moved the value 0.500→0.683; cross-boundary re-anchor moved
   smoothly, no jump). `uikit`/`sfxgen` recompile unchanged.
3. **`uiloupe` demo** — DONE (cramped synth panel, one line `ui_loupe(1)`).
4. **Still open**: retrofit a *real* cart (sfxgen / sh101 fader panel — §6) +
   device pass; the standalone `loupe` POC stays as the teaching demo.
5. **Docs** (this note done) — still: [`cart-authoring.md`](../guides/cart-authoring.md) library-header note,
   [`mobile-web-notes.md`](mobile-web-notes.md) cross-link, STATUS entry.

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

  Note the distinction that survived: the rejected `touch_lens` was an *input*
  transform (warps where touches land). The `zoom_rect` we DID add is an *output*
  transform (copies pixels) — it touches the engine too, but it's a general
  rendering primitive like `camera_ex`, not a widget, and it's reusable
  (picture-in-picture, minimaps). Adding it does not reopen the input-warp.
- **Render the lens by redrawing widgets / a `scene()` callback** (no engine
  change) — built and working, but it either missed cart-drawn content
  (widgets-only) or needed per-cart `scene()` plumbing + a render-only mode. The
  `zoom_rect` copy is simpler and shows everything; these were dropped for it.
