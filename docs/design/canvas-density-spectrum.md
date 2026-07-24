# The canvas-density knob — one dial from "scale it up" to "full reflow"

STATUS: PROTOTYPED (2026-07-19) — the middle route (chunky scale + spread) is proven live in
[`acidcandy`](../../tools/carts/acidcandy.c)'s 303 face (landscape, iPhone + iPad shapes; WIP — the
other faces + a commit still pending). The insight below is the reusable part.

> **Why this doc exists.** "Responsive" felt like a binary — either *scale the fixed 160×100 up and
> letterbox it* (looks like a port, dead bars) **or** *do a full device-adaptive reflow* (real work,
> and one face spread across a tablet reads sparse). Retrofitting `acidcandy` for landscape surfaced
> that it's **not** a binary: scale and reflow are two ends of **one dial**, and the dial is a single
> number — **the pixel density of the canvas you ask the engine for.** Same `lay.h` layout code the
> whole way; only the requested canvas size changes. This doc names the three settings so the next
> cart picks one on purpose instead of re-deriving it. It's the *mechanism* under Fixed-Canvas
> "expand outward" ([`design-language.md`](design-language.md)).

## The one idea

A resizable cart requests its own logical canvas via `de_resize(w, h)` (cart-drivable — see
`studio.c`; `acidwire`/`acidface`/`acidcandy` do it). The engine then blit-scales that canvas to the
device, nearest-neighbour, and maps input back through it (so touch stays 1:1 — **never** scale with a
camera, which desyncs `ui.h`; see [CLAUDE.md] / [`device-adaptive-layout.md`](device-adaptive-layout.md)).

So the cart controls **how many logical pixels** it lays out in. That single choice is the whole
spectrum:

| route | canvas you request | feel | control sizing | cost | when |
|---|---|---|---|---|---|
| **1 · scale + bars** | the fixed design size (160×100) | the design, blown up, **dead margins** (black/candy bars) | n/a (fixed) | ~zero | never great; the `window-fill-scaling` reject |
| **2 · chunky scale + spread** | device **ratio** at the **design's density** (~217×100 phone, ~160×120 iPad) | the design's exact character, scaled up, the **leftover ratio-offset spread in** — no bars | by **design proportions** (fractions of the canvas) | ~cheap retrofit | a **Fixed-Canvas cart** that should keep its 160×100 soul |
| **3 · full reflow** | device size at **fine K=2 density** (426×196, 688×516) | controls re-spaced for the device; can **rearrange / show more** on a tablet | by **`finger_px()`** | per-device-class design work | a cart that genuinely wants to *use* the extra room (more faces, denser info) |

The knob is literally the `de_resize` target. Ask for 160×100 → route 1. Ask for device-ratio ×
design-density → route 2. Ask for device-ratio × fine-density → route 3. **Same reflow code, three
feels.**

## The middle route (2), in detail — the one acidcandy uses

Goal: "scale the 160×100 up as much as it fits, then adjust a little" — keep the density, spread only
the leftover offset from the aspect mismatch (a phone is wider than 16:10, a tablet is less wide).

**Pick the canvas** — match the device ratio while pinning the *fitting* axis to the design, so the
design sits at 1:1 there and the *other* axis gains a little slack to spread into:

```c
extern void de_resize(int w, int h);   // engine seam (platform.h); cart-drivable
// in draw(), before laying out — the ratio is invariant under our own resize, so this is a fixed point:
int cw = screen_w(), ch = screen_h();
float r = (float)cw / (float)ch;
int tw, th;
if (r >= 1.6f) { th = 100; tw = (int)(100.0f * r + 0.5f); }   // wider than 16:10 → keep height, extend width
else           { tw = 160; th = (int)(160.0f / r + 0.5f); }   // taller → keep width, extend height
if (tw != cw || th != ch) de_resize(tw, th);                  // no-op once at the fixed point
```

**Lay out by DESIGN PROPORTIONS, not `finger_px()`.** This is the key difference from route 3. At the
chunky density the canvas is ~design-sized, so a `finger_px`-sized band is way too tall and eats the
hero (learned the hard way — the LCD vanished). Size bands as fractions of the canvas that match the
original layout, and let the leftover fall into the hero + the widths:

```c
float H = stage.h, W = stage.w;
Box krow  = lay_split(body, EDGE_TOP,    H * 0.24f, &body);   // design ≈24/100
Box notes = lay_split(body, EDGE_BOTTOM, H * 0.28f, &body);   // design ≈26/100
Box skcL  = lay_split(body, EDGE_LEFT,   W * 0.11f, &body);   // design ≈16/160
/* ... */  Box lcd = body;                                    // the hero = the remainder (absorbs extra height)
// widths spread via lay_grid(row, N, N, i, gap) → knobs/steps fan out into the extra width
```

Result (acidcandy 303, verified): iPhone-landscape → ~**217×100** (knob row + note-bars widen); iPad
→ **160×120** (the extra 20px flows into the LCD hero). Both read exactly like the 160×100 scaled up,
no bars. Its render at 160×100 is pixel-faithful to the original design (same layout), so the base is
untouched and it only *spreads* off-ratio.

## Choosing a route

- **Fixed-Canvas instrument that should keep its identity** (the candy device-faces, most single-idea
  carts) → **route 2**. Cheap, keeps the soul, no per-device design.
- **A cart that wants to exploit the room** — a tablet showing *more* (several device-faces at once,
  more of a list, a bigger playfield) → **route 3** (device-adaptive-layout.md's model; `acidwire` is
  the reference). This is the "show more on tablet" direction — a deliberate, bigger commitment.
- **Route 1** is only worth it as a stopgap or when the design truly can't spread; prefer 2.

Nothing stops a cart from using route 2 in one orientation and route 3 in another — it's a per-frame
choice of the `de_resize` target.

## The second axis — ARRANGEMENT (a breakpoint), orthogonal to density

The density knob changes *how big/crisp* a layout is — it keeps the **same arrangement**. A big screen
often wants a **different arrangement**: not "the phone face, bigger", but a bespoke layout that *uses*
the room (acidcandy's four machines in rows at once, instead of one focused face). That's a **second,
orthogonal axis**, chosen at a breakpoint via `device_class()` (0 TALL / 1 WIDE / 2 ROOMY; the twin of
`disclose_shape()` → `DISC_TALL/WIDE/ROOMY` in [`disclose.h`](../../runtime/disclose.h)).

| axis | question | lever | acidcandy |
|---|---|---|---|
| **density** | how big / crisp? | the `de_resize` canvas size (routes 1–3) | phone: route 2 — scale the face up + spread |
| **arrangement** | which layout? | a `device_class()` branch in the cart | tablet: a bespoke "all four machines in rows" |

They **compose**: phone-landscape = *route-2 density × one focused face*; tablet = *fine density ×
show-more arrangement*. The tablet layout is a genuinely **separate draw path** — the device-face
paradigm's "scale phone→tablet by showing MORE, not rearranging one face" (Pure Acid's iPad shows every
rack; [`acidwire`](../../tools/carts/acidwire.c)'s ROOMY 2×2 is the built reference). It is **not** free
like route 2 — it's real layout design (what's compact vs expanded per machine; does the nav *focus*
one or *highlight* among all).

**So a `responsive.h` helper must stay arrangement-AGNOSTIC** — that's what keeps it tiny and universal.
It owns axis 1 (hand back the density-right canvas + safe stage) and *reports* axis 2 (the class); the
**cart owns the branch**:

```c
Box stage = respond(160, 100);                       // axis 1: the canvas + safe rect
if (device_class() == 2 /* ROOMY */) draw_all_rows(stage);      // axis 2: the cart's own arrangements
else                                 draw_focused_face(stage, face);
```

One tiny include then serves both "just scale my 160×100 up" carts *and* "reorder everything on a
tablet" carts. The **nav is class-aware too**: on a phone the cartridge strip *focuses* one face; on a
tablet it can *highlight* while all stay visible ("focus, don't swap" — [device-face-paradigm.md](device-face-paradigm.md) §2).

**acidcandy's plan (both, per the maker 2026-07-19):** WIDE / phone = the route-2 scaled-up focused
face we have; ROOMY / iPad = a **bespoke four-machines-in-rows** arrangement designed *for* the tablet
(`acidwire` ROOMY is the reference sketch). Two arrangements, one cart, chosen by class — the tablet one
is deliberate design work, not a knob.

## Toward a "for everyone" feature

Today each route-2 cart hand-rolls the `de_resize` snippet above. The general version (parked, per the
maker 2026-07-19): a cart declares a **target density** (e.g. a `de:meta` `"density"` or a
`de_request_density()` seam) and the host sizes the canvas to *device-ratio × that density*
automatically — so a fixed cart opts into route 2 with one line, no draw-loop code. That's the
rule-of-three extraction candidate once a second and third cart want it (see *Generalize after
repetition* in [`design-language.md`](design-language.md)).

## Related

[`design-language.md`](design-language.md) (Fixed Canvas + author-reflow-ready — this is the *how* of
"expand outward") · [`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine plan; route 3
+ the camera-desyncs-`ui.h` gotcha) · [`responsive-layout.md`](responsive-layout.md) (`lay_*`, the
layout code all three routes share) · [`window-fill-scaling.md`](window-fill-scaling.md) (route 1, the
rejected pure-scale) · [`device-matrix.md`](device-matrix.md) (the device shapes/ratios) ·
[`acidcandy`](../../tools/carts/acidcandy.c) (the worked example).
