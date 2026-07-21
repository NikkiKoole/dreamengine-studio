# Responsive-first device faces — building it in from the start

STATUS: BUILDING (2026-07-21) — Layers 1–3 have SHIPPED (the starter · cell widgets + the shared
LayLane register · the declarative `face.h` grammar, proven across three conversions —
chipjam/dubjam/grooveface). The tablet/iPad-Pro spread is MOCKED (`roomyface` B/C/D, arrangement kept
OPEN as a per-cart choice). Only **Layer 4** (make `face.h` the default) is left, gated on the maker's call.
Answers the maker's question "could we make a cart responsive-and-opinionated *from the beginning*,
instead of retrofitting it like we did to acidcandy?" The encouraging finding: the hard, irreversible
half is already done — what's left is cart-land layers (ADR-0006 style), sequenced cheapest → deepest.
Born from the [`acidwide`](../../tools/carts/acidwide.c) mockup session (eight arrangements A–H,
vibe-checked by *looking*). Supersedes nothing; it's the graduation target for
[`canvas-density-spectrum.md`](canvas-density-spectrum.md) + [`responsive-layout.md`](responsive-layout.md).

## The question

Every device face so far — acidcandy, chipjam, dubjam — was authored **fixed-canvas first** (160×100)
and made responsive **after**. That retrofit is the whole acidcandy reflow saga. Could a *new* cart
instead start responsive, with an opinionated layout grammar that gets the arrangement principles
right by default, so nobody re-derives them (or re-makes F's mistake)?

**Yes — and we've already paid for the scary half.**

## What we learned that makes it possible (the constraints, discovered by mocking)

From the `acidwide` A–H study, the reusable principles (this is what an opinionated system would *encode*):

- **Canvas density is one dial** ([canvas-density-spectrum.md](canvas-density-spectrum.md)): a face
  requests a chunky canvas keyed to the device ratio; the lo-fi soul survives because the density
  stays coarse. Reflow, never scale — so `ui.h` touch stays canvas-native (the camera-desync gotcha).
- **The column register.** Per-step things (the sequencer view + the step strip + any accent/slide/tie
  lane) must share one **16-column x-lane**, full-width and stacked, so a step reads as one vertical
  slice. This is the thing E had and F broke.
- **Per-step vs band.** Anything *not* per-step — knobs, mode selectors, PAT/KEY/GEN/PERF — goes in a
  **horizontal band or a nook, never a vertical side-rail** (a rail steals the grid's width and breaks
  the register). This single rule is what would have stopped F automatically.
- **Paging is fine.** You don't need every control visible at once (G proved "everything" is busy);
  a paged screen with a calm always-on aligned strip (H) reads better. The *screen* owns modes; the
  *lane* is permanent.
- **Widgets should size to their cell.** Knob radius/margin/bevel from the cell, not hand-tuned — we
  hit this ~4× in one cart.

## The layers — cheapest to deepest

**Layer 0 — the foundation (DONE).** The irreversible engine work is behind us: runtime
`screen_w()/screen_h()/safe_rect()`, *reflow-not-scale* present, and [`lay.h`](../../runtime/lay.h).
That was the `§4b` wall in [`responsive-layout.md`](responsive-layout.md) / the engine phases in
[`device-adaptive-layout.md`](device-adaptive-layout.md), and it's climbed. Everything below is
cart-land, low-risk, no more engine surgery.

**Layer 1 — a device-face *starter* (cheap, mostly docs + a template). SHIPPED 2026-07-20.**
Today every face re-copies the chunky-canvas `de_resize` block and re-derives its bands by hand.
Promoted into **one template cart + a recipe** so a new face starts responsive *by starting from it* —
[`tools/carts/deviceface.c`](../../tools/carts/deviceface.c): a runnable, heavily-commented skeleton
that ships the route-2 chunky canvas, the five zones stacked as bands, and the shared 16-column
register (display roll ⇄ step grid), with all four constraints baked in and marked seams to drop an
instrument into. It's silent on purpose (teaches the layout, not the sound). Recipe folded into
[`design-language.md`](design-language.md) ("the copy-me starter"). Pure capture of what
acidcandy/acidwide already taught — no engine surgery.

**Layer 2 — widgets + a lane that own the rules. SHIPPED 2026-07-20** (the deeper knob gesture deferred).
- **`ui_knob_cell(Box, *v, label)` / `ui_button_cell(Box, label)`** in [`ui.h`](../../runtime/ui.h) —
  controls that size to a cell (closes the backlogged widget-in-a-cell gap in
  [`responsive-layout.md`](responsive-layout.md); graduated from acidcandy's `_knobx` / deviceface's
  local `knob_cell`). Named `_cell`, not `ui_knob(Box)` — C has no overloading and `ui_knob(float*,…)`
  already exists. **Deferred:** the scale-clean bevel + gear-drag/double-tap gesture (need two extra
  `ui.h` bits per acidcandy's todo); `ui_knob_cell` ships the plain vertical-drag knob.
- **A `LayLane` concept** in [`lay.h`](../../runtime/lay.h) — the shared 16-column register: `lay_lane`
  builds it from an x-span once, `lay_lane_cell` places column *i* into any band, so the sequencer view
  *and* the strip bind to one lane and alignment is *guaranteed*, not hand-matched (acidwide's
  `grid_lane` / deviceface's `g_lane_*` is the hack this replaces). E's insight, made structural. Typed
  `LayLane` not bare `Lane` — carts already use `Lane` for their own structs (acidcandy). `deviceface`
  now drives both; `ui.h` includes `lay.h` (guarded) so the widgets can take a `Box`.

**Layer 3 — a face *grammar* (the real "opinionated" core; a `face.h` cart-land header). FIRST CUT
SHIPPED 2026-07-20 — prototyped, wants proving across more carts before Layer 4.** A small declarative
scaffold over `lay.h`: [`runtime/face.h`](../../runtime/face.h). Declare **zones** in a `FaceZone[]`
table — `FACE_BAND` (docked strip) · `FACE_LANE` (per-step, lane-bound) · `FACE_HERO` (the remainder) —
and `face_layout()` owns the chunky-canvas resize (`face_resize`), the safe-area content box
(`face_area`), carving the zones (declaration order reads visually top→bottom), and the shared
`LayLane` register (`face_col`). It **enforces the principles by construction**: bands take `EDGE_TOP`/
`EDGE_BOTTOM` *only* — there is no way to express a width-stealing side-rail (the rule that would have
stopped "F"); the lane always spans the full width; the hero is always the remainder. The
per-breakpoint arrangement is one branch — pick the `FaceZone[]` table off `device_class()`
(TALL/WIDE/ROOMY). Worked demo: [`facedemo`](../../tools/carts/facedemo.c) — the same five zones as the
`deviceface` starter, but the whole layout is a 5-row table and `draw()` is
`face_resize()`→`face_area()`→`face_layout()`→draw into `f.box[i]`. Scaffold not straitjacket: every
zone is a plain `Box`, and a bespoke face just doesn't call `face_layout()`.

*Proving pass 1 — [`chipjam`](../../tools/carts/chipjam.c) converted (2026-07-20).* Took an existing
fixed-canvas 160×100 face (dense literal coordinates, five machines, live PU1 audio) and reflowed its
`draw()` onto face.h — the exact retrofit case. **The grammar held**: the four zones mapped cleanly, the
knob band cell-sized across 4/5-knob faces, and the register unified chipjam's two per-step strips (the
piano-roll + the note-bars, previously hand-aligned at *different* widths). Audio was untouched (the
conversion is `draw()`-only). Two findings worth keeping:
- **Enforcement changes non-conforming faces (as intended).** chipjam's soft-keys used to *flank* the
  roll (left `SEQ/FLAG/FX/GEN`, right `KEY/PAT`) — mild side-rails that narrowed the per-step roll. The
  grammar has no way to express that, so they moved to a horizontal **nook** at the top of the screen and
  the roll went full-width. A real visual change, and the right one.
- **The register is full-width-anchored → indented per-step content needs the escape hatch.** `face_col`
  takes only a band's y/h, never its x (that's what guarantees alignment). The drums face's grid sits
  *after* a voice-name gutter, so it can't ride the full-width register — it drops to `lay_lane(sub, cols)`
  on the indented sub-box (documented in `face.h` at `face_col`). This is the scaffold-not-straitjacket
  seam working, not a bug — but it's the first thing a converter hits, so it's now called out.

*Proving pass 2 — [`dubjam`](../../tools/carts/dubjam.c) converted (2026-07-20).* The hard case, chosen to
stress the perform zone: four wired machines (grenadier drone, TR-808, dub master, sub) and — unlike
chipjam — **input hit-tested in `update()` at fixed coordinates** (XY sweep pad, root keybed, note-bar
tap/drag, drum grid, dub-throw pad), separate from `draw()`. chipjam converted trivially because all its
input went through `ui.h` capture inside `draw()`; dubjam's coordinate-coupled input is where a naïve
reflow breaks — the visuals move but the touch targets don't. **The grammar still fit, with one pattern
the conversion forced (now the headline caveat in `face.h`):**
- **Input-coupled faces compute the layout in `update()`, shared with `draw()`.** A `relayout()`
  (`face_resize` → `face_area` → `face_layout`, stored in file statics + derived interaction rects) runs
  *first* in `update()`; both the input handlers and `draw()` read those statics, so touch stays glued to
  the pixels under any reflow. This is the general pattern for any device face whose input isn't pure
  `ui.h`-in-`draw()`. Verified live: dragging the XY pad moved the filterbank, and a cartridge *tap*
  (not the key shortcut) switched faces — both through the reflowed rects. Audio DSP was untouched.
- Reconfirmed the chipjam findings: SUB's screen pitch-view + its note-bars now share the full-width
  register (aligned), and the DRM grid (after a name gutter) uses the local-`lay_lane` escape hatch.

*Refinement — the full-width register was too strong (2026-07-20, with the maker).* The vibe-check
landed: forcing chipjam's flanking soft-keys into a top nook (to keep the roll full-width) was **not the
desired look** — flanking the *screen* with chrome is a legitimate, often nicer arrangement. The fix
sharpened the doctrine: the ban is only on **top-level full-height side-rails** that steal the whole
*face's* width (the "F" mistake); **flanking the HERO** with soft-keys is a different, allowed thing (it's
the paradigm's zone 3). Added [`face_sublane(span, cols)`](../../runtime/face.h) — a **bounded** per-step
lane on a sub-region, a *first-class* alternative to the full-width register (not an escape hatch), for two
cases: a flanked hero (roll between soft-key columns) and indented content (a grid after a name-gutter).
chipjam's flanking look is restored on it; the note-bars still ride the full-width register. So the grammar
now offers **both** — full-width-aligned by default, bounded-and-flanked by choice.

That choice is now **one declarative call**: [`face_screen(hero, nL, nR, fracL, fracR, cols)`](../../runtime/face.h)
carves optional soft-key columns off the hero and returns the middle `screen` + the `left`/`right` flank
boxes + a per-step `lane` bounded to the middle. **Side-buttons-or-not and fullscreen-screen are the same
call, a flag apart**: `face_screen(hero, 0, 0, …)` → a fullscreen screen whose lane spans full width (lines
up with a full-width play lane); `face_screen(hero, 4, 2, …)` → soft-keys down both sides with a bounded
middle lane. chipjam drives both shapes (flanked soft-keys on the melodic face, a single name-gutter flank
on the drums face) through the one idiom.

*Proving pass 3 — [`grooveface`](../../tools/carts/grooveface.c) converted (2026-07-20).* The flagship
five-zone face (the control-vocabulary showcase) and the first conversion that is **portrait** and at a
**non-160×100 density** (320×400). It surfaced the one remaining face_resize assumption: the target was
hardcoded to the landscape candy density. Fixed with
[`face_resize_to(dw, dh)`](../../runtime/face.h) — the design density is now a per-face choice; the zones
and register are density-agnostic. Everything else mapped cleanly: all five zones, the GRID/MIX soft-keys
via `face_screen`'s flank, the step row on the full-width register, pads as a 2×2 `lay_grid`. All input is
`ui.h`-in-`draw()`, so it converted like chipjam (no `relayout()` needed). Verified live: skin cycles
(TACTILE/FLAT/PURE render identically through the same zones — **layout is orthogonal to look**), the
MIX/GRID flow switch, and a pad tap re-selecting the track (`sel=3` after a CLAP tap). Audio untouched.

**Three faces converted (landscape input-in-draw, landscape input-coupled, portrait multi-skin), grammar
held every time and got better for the two friction points** (the register opinion → `face_sublane`/
`face_screen`; the density assumption → `face_resize_to`). The grammar now covers: portrait & landscape,
any design density, fullscreen or flanked screens, per-step register or bounded lanes, and input via ui.h
or coordinate-coupled update(). Remaining before Layer 4: the maker's call on whether that's proven enough
to make face.h the default.

**Layer 4 — make it the default (optional, deepest).** A capability so a "device-face" cart gets the
reflowing canvas + scaffold without opting in each time. Only worth it once Layer 3 proves out across
several carts — don't pre-build.

### The tablet / iPad-Pro spread (the arrangement axis) — mocked, pending a pick

A much larger screen is a *different* question from reflow: route-2's chunky canvas just blows ONE phone
face up huge (sparse on a 13"). The good news is face.h needs no engine work for the tablet — `face_layout`
takes any Box (tile faces) and `face_resize_to` is density-free (ask for a finer canvas), so the whole
spread is a `device_class()==ROOMY` branch. What's *not* settled is the arrangement, and it's cart-dependent.
**Decision (2026-07-20, with the maker): keep the arrangement OPEN — it's a per-cart choice menu, not one
default.** Both directions have merit and different carts want different ones. Three are mocked draw-only in
[`roomyface`](../../tools/carts/roomyface.c) (keys 1/2/3), all on the same grammar, on an iPad-Pro 4:3 canvas:
- **B · show more — tile the rack.** Four machine faces at once (2×2), each a compact `face_layout` per
  `lay_grid` cell. Best for a **multi-machine rack**. The paradigm's "show more, not rearrange".
- **C · unhide the depth.** One machine using the whole screen, everything that pages behind soft-keys on a
  phone shown at once (the `acidwide` A–H bet at tablet scale). Best for a **single deep instrument**.
- **D · 2×2 + master (ReBirth RB-338 classic).** B plus a docked master strip (per-machine channel faders +
  master FX + master out) — the whole rack *and* the mix in one view. The maker's reference for "sometimes
  this is the best."
face.h stays **neutral** — it already does all three (B/D = `face_layout` per cell + a master column; C = one
wider zone table), so the cart's `device_class()==ROOMY` branch picks. No default is baked. When acidcandy
(a 4-machine rack) is eventually converted, D/B is the likely pick — but that stays the cart's call.

## The one thing to protect

**Opinionated must stay a scaffold, not a straitjacket.** It has to keep the Fixed-Canvas lo-fi soul
(the chunky-canvas density already ensures that — see [`canvas-density-spectrum.md`](canvas-density-spectrum.md)
+ [ADR-0029](../decisions/0029-320x200-is-the-base-resolution.md)), and any cart
that wants a bespoke layout must still drop to raw `lay.h` underneath. **Opinionated-by-default,
escapable-by-choice.**

## The method is the mockup loop

The way to design Layer 3 is exactly what produced this doc: mock the grammar against throwaway faces
(`acidwide`-style, draw-only), vibe-check by looking, keep the arrangement that survives, then
graduate. The mockup loop isn't a detour from building the system — it *is* how you build it, cheaply
and with the maker's eye in the loop.

## See also

- [`canvas-density-spectrum.md`](canvas-density-spectrum.md) — the density dial (Layer 0/1 mechanism)
- [`responsive-layout.md`](responsive-layout.md) — the `lay_*` toolkit + the widget/gap backlog (Layer 2)
- [`device-adaptive-layout.md`](device-adaptive-layout.md) — the engine foundation that shipped (Layer 0)
- [`design-language.md`](design-language.md) — the paradigm index this would extend (Layer 1)
- [`device-face-paradigm.md`](device-face-paradigm.md) — the device-face family this systematizes
- `tools/carts/acidwide.c` — the A–H arrangement study the constraints came from
