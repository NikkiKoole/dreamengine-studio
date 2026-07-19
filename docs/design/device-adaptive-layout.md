# Device-adaptive layout — one cart, beautiful on iPhone AND iPad, both orientations

STATUS: BUILDING — **Phase 0 banked; Phase 1 DONE (2026-07-03) + growable framebuffer (2026-07-04); Phase 2 DONE (2026-07-04) — a resizable cart fills any device + dodges the notch + reflows on rotate (verified iPhone SE / 15 / iPad Pro 12.9 + landscape on the sim). Phase 3 RE-PLANNED (2026-07-05) after the maker's device test — the first acidrack pass reflowed but didn't redesign (see §"Phase 3 — revised plan" + field note [018](../field-notes/018-passing-the-gates-felt-like-done.md)). Resizable-app PLUMBING landed 2026-07-06 (K=2 pixel-chunk + safe-area insets + reflow-aware menu; commit be7b2cad — see §"2026-07-06"). Resume at the WIREFRAME CART (extend `acidfit.c` with `lay.h`, chosen over an HTML mock — see §"2026-07-06") → then R5 (acidrack redesign) → Phase 4 (store assets).**
This is the **execution + product** doc that graduates the deferred thinking now that there's a
concrete need (Tinyjam on the App Store).

## Where this stands (scoreboard)

_As of 2026-07-10._ Foundation (Phases 0–2) is DONE; the redesign (Phase 3 = steps R1–R6) is in
progress; store assets (Phase 4) come after. The detail for each lives further down; this is the
at-a-glance.

| Step | What | Status |
|---|---|---|
| Phase 0 | disclosure model proven in mock carts (`respond`/`rackfit`/`acidfit`) | ✅ done |
| Phase 1 | engine reflows live (resizable canvas + growable framebuffer) | ✅ done |
| Phase 2 | iOS fill + safe-area + rotation | ✅ done |
| — | the **`acidwire` wireframe** — interactive, on-glass, all shapes + states | ✅ done · did its job (field note [020](../field-notes/020-the-fit-cart-earns-it-on-glass.md)) |
| R1 | the layout brief — taste calls before code | ✅ layout calls captured in [`acidrack-layout-brief.md`](acidrack-layout-brief.md) §2; **content calls deferred to R5** |
| R2 | `runtime/disclose.h` — shape + finger-budget accordion + stack | 🟡 **core shipped + proven in acidwire** (`27637b26`/`d96c4404`); WIDE tabs + ROOMY grid + tab-chip interaction + `DE_TRACE` self-report deferred |
| R3 | real finger unit — `finger_px()` / `device_class()` | ✅ done (`7102af8b`) — engine primitives from a backing-scale seam (iOS reports pixelChunk); acidwire's `FU` now calls `finger_px()`. Verified on device |
| R4 | judgment oracle — `ui-audit` reads disclose's self-report | ⬜ todo — needs the `DE_TRACE` self-report first |
| R5 | **re-land acidrack on the method** — call `disclose.h`, build the missing iPad arrangement, make the compact/page CONTENT calls *playing it* | ⬜ todo — the big one |
| R6 | `epiano` fresh from a brief — the method's test; keybed reflow → `keybed.h` | ⬜ todo |
| Phase 4 | App Store screenshots/assets from the finished racks | ⬜ later |

**Sequence — what unblocks what:**
1. ~~R3~~ ✅ done — the finger unit is a real engine primitive (`finger_px()`), so footprints are honest across chunks/HiDPI, not the SCALE=1 coincidence.
2. **R5 next** (the acidrack port) — needs R2-core (done) + R3 (done), both cleared. This is where the deferred CONTENT calls — which compact knobs, what the N/K/F pages hold — finally get made, against the real engine, on glass (field note [020](../field-notes/020-the-fit-cart-earns-it-on-glass.md)'s "stop wireframing when the questions turn into instrument questions").
3. **R4** rides alongside R5 — add disclose's self-report + the `ui-audit` judgment pass once there's a real port to check.
4. **R6** (`epiano`) last — proves the method generalizes; cheap if R1–R4 held.

**Parked (revisit at R5/R6):** whether the WIDE tabs + ROOMY grid graduate into `disclose.h` (they show one state → no disclosure happening; decide when `epiano` actually needs them); `acidwire`'s page-button CONTENT; `acidwire`'s RND/CLR are wireframe-only (acidrack ships its own CPY/CLR/RND).

**Pick-up point (next session):** Phase 0 is complete (model proven across `respond`/`rackfit`/
`acidfit`/`otafit`; `lay_*` shipped as `runtime/lay.h`). **Phase 1 is complete — the engine reflows
live on desktop:**
- **1a** — runtime `de_sw`/`de_sh` globals + all standalone render-extent sites read them.
- **1b** — the per-cart `-DDE_RESIZABLE` opt-in (`de_reflow`): a resizable window whose active dims
  reflow to `window / SCALE` on every resize + at boot. The SW-rasterizer renders into a
  `de_sw×de_sh` region of the framebuffer (flip origin → `de_sh-1`, bounds → `de_sw/de_sh`, stride →
  runtime `fb_w`); the present samples `(0,0,de_sw,-de_sh)`. The cart API **`screen_w()`/`screen_h()`**
  exposes the live size (studio.h/.c + studioDocs + shell).
- **1c** — `respond.c` flipped: `-DDE_RESIZABLE` makes the real window the responsive surface (the
  fake-drag prototype is the `#ifndef` default, unchanged). Verified reflow row→column across sizes.
- **editor** — the ▶-run passes `-DDE_RESIZABLE` for a cart whose `de:meta` says `"resizable": true`
  (`main.cjs`); `play.js` does the same + a `--resize "WxH,…"` sweep (a scripted "resize→look" test).
- **the growable framebuffer ("B", 2026-07-04)** — **the sub-region-vs-realloc fork was re-resolved:
  GROW-ONLY realloc, not a fixed max.** `sw_cbuf`/`sw_world_buf` are heap buffers; `de_ensure_fb()`
  grows them (and, via `de_grow_gpu()`, the GPU `canvas`/`canvas_snap` + the 2× smooth offscreen) to a
  high-water-mark `fb_w×fb_h`, clamped to `DE_MAX_DIM` (4096). `de_set_canvas()` is the single funnel
  (boot/reflow/sweep). So a resizable cart fills **any** window up to 4096/side — no longer capped at
  its compile size. Grow-only = lean when small, big when enlarged, no realloc thrash on a drag.
  This is why the first cut showed **side bands when enlarged** (de clamped to the small buffer) and
  **top-pinned "drift" when narrowed** (clamped `de_sh` → `gr_place` DECK mode): both were the fixed
  ceiling, both gone once the buffer grows to `window/SCALE` so `gr_place` fills.

**Verified byte-identical:** a fixed cart never grows `fb` (stays `SCREEN_W/H`), so `drawall` SW frames
are SHA-identical to HEAD and all 465 carts compile at every stage.

**Phase 2 iOS FILL is DONE (2026-07-04).** New platform seam `de_resize(w,h)` (→ `de_set_canvas`) +
`de_is_resizable()` (→ `de_reflow`) in `platform.h`/`ios/Sources/engine.h`/`studio.c`. `CanvasView.swift`
reads the canvas dims LIVE (was baked once at init) and, for a resizable cart, calls `de_resize(bounds
in points)` from `layoutSubviews` — so the cart fills the device and reflows on rotation for free; the
flip scratch + touch mapping track the new size. `ios/build.sh RESIZABLE=1` builds a single cart with
`-DDE_RESIZABLE`. Verified: `respond` fills iPhone SE / iPhone 15 / iPad Pro 12.9 on the simulator, vs
the 320×200 letterbox baseline; a fixed cart (`de_is_resizable()==0`) stays letterboxed, untouched.
**GOTCHA that bit:** `build-all` only covers the raylib path — the `--resize`/overlay work broke the
DE_NO_RAYLIB (iOS/`build-nr`) build (missing `GetScreenWidth`/`IsWindowState`/`SetWindowSize`/`ImageCrop`
in `raylib_compat`); **run `build-nr.sh` after touching studio.c**, not just `build-all`.

**Safe-area + rotation are DONE too (2026-07-04):** seam `de_set_safe_area(l,t,r,b)` fed from
`UIView.safeAreaInsets` in `layoutSubviews` + cart-facing `safe_rect(&x,&y,&w,&h)` (whole canvas on
desktop, chrome-excluded on device); `respond` lays controls inside it while `cls` bleeds the bg
full — verified the title bar clears the iPhone 15 notch. Rotation: `INFOPLIST_KEY_UISupportedInterfaceOrientations*`
allows landscape, `layoutSubviews`→`de_resize` reflows on rotate (confirmed landscape on the sim).

**Start here to resume: §"Phase 3 — revised plan (2026-07-05)"** below. The first acidrack pass
(commits `839dabed`/`ef2108ae`/`302f3947`) made the cart *reflow* but skipped the model this doc
specs — hand-rolled px thresholds instead of `lay.h` + finger units, no disclosure pass, and **no
iPad/roomy arrangement at all**. The maker's on-device test caught it; the retrospective is field
note [018 — Passing the Gates Felt Like Done](../field-notes/018-passing-the-gates-felt-like-done.md).
The engine foundation (fill, safe-area, rotation, growable fb) is complete and unaffected — what
changed is *how Phase 3 is executed*: brief first, shared `disclose.h`, judgment oracle, and the
three-state strip from the ReBirth study. Nothing in Phase 0/1/2 pending. (The `device_class()` /
finger-unit helper is no longer a deferred nicety — it's step R3 of the revised plan.)

**Where this sits among the three sibling docs — they are NOT duplicates:**
- **This doc** = the *engine change + product plan*: make `SCREEN_W/H` runtime + physically-sized,
  handle rotation/device-class, and the phased ladder to ship it.
- [`responsive-layout.md`](responsive-layout.md) = the *cart-land toolkit* it graduates
  (`lay_*` — flex / fluid / anchor / aspect / wrap), already prototyped in `respond.c`.
- [`window-fill-scaling.md`](window-fill-scaling.md) = the *rejected alternative* (keep the canvas
  fixed, fill the window with a fractional scale). The maker's call (2026-07-02): reflow wins for a
  sellable app — a scaled poster reads as a port, a reflowed UI reads as an app.

> **GOTCHA — scaling the render with `camera()`/`camera_ex()` is NOT a shortcut to fill; it silently
> breaks touch.** The tempting hack for a fixed cart is "zoom the whole draw with `camera_ex` + bleed
> the bg" — DON'T. `ui.h` hit-tests against widget rects in **raw canvas coords** (it reads
> `mouse_x`/`touch_x`, not the camera-inverse `mouse_world_x` — verified 2026-07-19), so a zoom/scale
> camera desyncs every `ui_button`/`ui_knob`/pad: taps land in the wrong place, with **no error**. This
> is a second, concrete reason reflow beats scaling (beyond the "port look" above). The correct fill is
> always: mark the cart `resizable`, read `screen_w()`/`screen_h()` + `safe_rect()`, and lay widgets out
> in that live space — canvas == device px means `ui.h` is 1:1 and input just works. (A `camera_ex` with
> zoom≠1 also forces the cart onto the GPU raster path, `sw_force_gpu` — another reason to avoid it for a
> whole-UI transform.)

Supersedes the "deferred, no concrete need yet" framing in `responsive-layout.md` §gate and
[`share-panel.md`](share-panel.md) next-spike #3, and answers [`ios-plan.md`](ios-plan.md) umbrella
backlog #2 (multi-resolution racks).

**Base resolution is settled at 320×200** ([ADR-0029](../decisions/0029-320x200-is-the-base-resolution.md)):
author carts good at 320×200 (engine default + DOS/DIV heritage, 16:10 Mac-native, the neutral middle
between iPad 4:3 and iPhone 19.5:9), then reflow *outward* to the device — a fixed size can't fit both
iPhone and iPad, so the base is a design canvas, not a device bet.

## Why now — one foundation, three payoffs

The engine still bakes `SCREEN_W/H` as compile-time `#define`s (studio.h), hardcoded in ~76 places
in `studio.c` and shared by the ONE `-D` value the whole binary compiles with. On iOS the fixed
framebuffer is letterboxed onto the device (`CanvasView.swift`: `contentsGravity = .resizeAspect`),
so a 320×240 (4:3) rack shows with big black bars on an iPhone (~9:19.5). `build-app.js` hard-rejects
mixed-dim racks for the same reason. One change — **live-resizable, physically-sized screen dims** —
unblocks all three of these at once:

1. **The product bar.** Tinyjam looks native on iPhone **and** iPad, both orientations.
2. **The bundling limit dies.** Mixed-resolution racks in one app (no more bumping epiano 200→240 to
   join), plus resizable desktop windows fall out for free.
3. **The store-asset blocker dies.** Honest, full-bleed App Store screenshots **and** App Preview
   videos, every required size, rendered straight from one cart (see §"The store-asset payoff").

## The core reframe: it's physical touch ergonomics, not aspect ratio

The naive framing is "iPhone/iPad × portrait/landscape = 4 layouts per rack." That's a maintenance
trap across N racks, and it misses the real driver. A fingertip is **~9mm on every device** — what
changes is how much *canvas* those 9mm cover:

| Device | Physical width | 320px canvas → | A 9mm finger covers |
|---|---|---|---|
| iPhone 15 | ~71mm | 0.22mm/px | **~40 canvas px** — fat; ~5–6 controls fit across |
| iPad 11" | ~178mm | 0.56mm/px | **~16 canvas px** — precise; 12+ controls fit across |

So iPhone and iPad don't want the same layout scaled — they want different **information density**.
iPhone: fewer, bigger, thumb-reachable controls. iPad: the full dense panel, precise, like the
desktop rack. The consequence: a cart must author touch targets in a **physical unit (points), not
framebuffer pixels**, or "finger-sized" is meaningless across devices.

### Two axes, a handful of arrangements — NOT four pixel layouts

A rack is handed a viewport and only two things drive its design:

- **Density** — how many finger-units wide/tall the viewport is (`device_class()`: phone = few,
  tablet = many). Drives *how many controls* and *how big*. **Key: the real signal is the *continuous*
  finger-budget, not the phone/tablet bucket.** "Tablet" spans an **iPad mini** (~744pt wide, ~17
  finger-widths) to an **iPad Pro 12.9"** (~1024pt, ~23) — the Pro fits noticeably more inline, the
  mini is tighter (closer to a big phone). Because the rules key on the finger-budget, the mini
  *defers a section the Pro shows inline* with **no special-casing** — which is exactly why
  `device_class()` is only a convenience that *lies at the boundary* (a mini is "tablet" by name but
  phone-like in density). Branch on the measured ratio; use `device_class()` only as a rough hint.
- **Orientation** — `w > h` vs `h > w`. Drives row↔column arrangement and keybed placement (bottom
  in portrait, spread wide in landscape).

You hand-pick a small number of **arrangements** (topologies — what goes where, how many rows: the
taste part, where phone and iPad genuinely differ), and the `lay_*` fluid units absorb the
*continuous* variation so an iPhone SE vs 15 Pro Max, or an 11" vs 13" iPad, are **not** separate
work. Realistically ~2–3 arrangements per rack, not 4 — iPad portrait+landscape often share one
"roomy" arrangement that just flips a flex direction, while the phone is tight enough to need its two
distinct ones. This is the sweet spot between "one magic fluid layout" (fine everywhere, great
nowhere) and "N hand-tuned pixel layouts" (unmaintainable).

## What the engine must provide

1. **Live-resizable `SCREEN_W/H`.** The ~76 internal uses in `studio.c` (framebuffer, clip bounds,
   blit loops) read the **active render target's** dims at runtime instead of the macro; the
   framebuffer **reallocs when bounds change mid-run** (rotation is a live event, not a per-launch
   one). This is share-panel #3 path B's core AND path C's gate ("a live, queryable size"), unified.
   Exposed to carts as `screen_w()` / `screen_h()`.
2. **A point-based viewport (physical sizing).** The iOS layer already knows the view's size in
   **points** and the backing scale; pass both through so a cart can size a knob at "60pt" and get a
   physically-consistent target on every device. Without this, density adaptation is guesswork.
3. **`device_class()`** — a phone/tablet convenience derived from the point-viewport (mobile-web-notes
   §5), so a cart branches arrangements without hardcoding device names.
4. **Safe-area rect + rotation.** A `safe_rect()` inset (notch / home-bar — folds into `lay_at`
   anchoring per responsive-layout.md primitive #3), and the iOS `Info.plist` must actually *permit*
   rotation (the spike letterboxes / likely locks today). Retires the temporary hold-to-home in favor
   of the `de_safe_top()` nav-bar idea (ios-plan backlog #3). **Rotation is a per-rack *policy*, not
   always-on:** a rack may declare `free` / `lock-landscape` / `lock-portrait` (see "disclosure mode
   is per-shape" below); the shell constrains the active rack's orientation accordingly.
5. **A back-to-root keep-out rect.** The multi-cart shell owns the "back to launcher" affordance
   ([`share-panel.md`](share-panel.md) hold-to-home / `de_switch_cart`), so it must hand the active
   cart a **reserved region** its controls avoid. The space-efficient choice (proven in `rackfit.c`)
   is a **top CORNER**, not a full top edge — reserving the whole edge wastes precious phone height
   for one small button. So the engine gives the cart *both* insets — `safe_rect()` (OS chrome) and a
   corner keep-out (app chrome) — and the shell draws the chip itself. The cart just reads the rect;
   no cart hardcodes the button's position. (This is the responsive successor to the temporary
   shim-drawn hold-to-home pad.)

**Determinism guard (non-negotiable, from responsive-layout.md §4c):** runtime dims must **not** be
forced global. ~45 carts and the `canvas-diff`/`mirror-diff`/`spec` gates lean on fixed dims. A cart
that ignores the new API renders at a fixed size and letterboxes exactly as today — **reflow is
opt-in per rack.** Runtime-dims changes *how studio.c reads the size*, not whether a given cart
varies. Fixed-canvas carts compile and behave unchanged; their runtime value is simply pinned at boot.

## The cart model (graduating `lay.h`)

- The `lay_*` toolkit graduates from the `respond.c` prototype to `runtime/lay.h` **unchanged** —
  it's deliberately rect-in/rect-out, so the only difference on graduation is passing the real
  viewport instead of the fake draggable rect.
- **Immediate-mode is what makes rotation free:** every rect is recomputed from the current viewport
  each frame, so a rotation is just "next frame the container is a different shape," and hit-testing
  recomputes with it. Audio/knob **state lives separate from layout**, so it survives relayout
  untouched.
- A rack's `draw`/`update` branch on `device_class()` + orientation into one of its 2–3 arrangements,
  then lay out fluidly within the chosen one.

### Two layers: geometry vs. behaviour (validated 2026-07-03, [`../../tools/carts/acidfit.c`](../../tools/carts/acidfit.c))

A dense real rack forced a distinction the clean `rackfit` didn't: **where** a panel goes vs.
**whether** it's shown at all.

- **Geometry layer** (`lay_*`) — pure rect-in/rect-out: where a panel goes *if shown*.
- **Behaviour layer** — *whether* a panel is inline, collapsed behind a toggle, tabbed, or scrolled,
  decided by the available finger-budget. This is the same side of the line as scroll/overflow (it's
  interaction + state, not geometry) and is where **progressive disclosure** lives.

**The progressive-disclosure rule (CSS "Priority+"):** each section declares a **priority** + a
**minimum comfortable footprint** (in fingers). One pass per frame inlines sections by priority until
the finger-budget runs out; the overflow collapses into **tab chips** that open as an overlay. No
device is named — the finger-budget decides. `acidfit` proves it on acidrack's real 5 sections:

| preset | inline | tabs |
|---|---|---|
| iPhone portrait | 3 (303-A/B, 909) | 2 (808, master) |
| iPhone landscape | 4 | 1 (master) |
| iPad portrait / landscape | 5 | 0 |

Findings that shaped the model:
- **Orientation drives disclosure as much as device class** — a *short* landscape phone defers more
  than a *tall* portrait one (which stacks). Reinforces "branch on the measured finger-ratio, not the
  device name."
- **The min-footprint must enforce *control* comfort, not just "the box fits."** The first pass used
  boxes small enough that a phone crammed all 5 sections with sub-finger knobs (INLINE 5 / TABS 0 and
  ugly). Sizing each section so its *controls* stay finger-sized is what makes disclosure fire
  correctly — the footprint is a comfort threshold, not a fit threshold.
- **`lay_grid` (fixed-column) earns its slot** — the drum pads read far cleaner as a fixed 4-wide grid
  than `wrap`'s ragged auto-flow (the 14+2 problem). So the added set is now `clamp · fluid ·
  pad/inset · at · split · grid · cell · wrap · aspect`.
- **All of it is pure cart-land** — priority + min-size + fit-check + disclosure state + the toggle
  interaction need *no* engine change beyond the planned viewport/finger inputs. So the whole
  behaviour layer can (and did) get proven before `studio.c` moves.

**Product implication for Tinyjam:** the dense racks want exactly this — phone shows the sequencer +
the sections that fit, the rest one tap away; iPad shows the whole panel. It's the same rack code, and
the section-switcher a phone needs is *emergent*, not hand-authored per device.

### The disclosure MODE is itself per-shape — and orientation may be LOCKED (2026-07-03)

`acidfit` grew a third disclosure mode — an **accordion** (maker's call: the honest contrast is
"expand a few in place" vs. "cram *everything* open at sub-finger size", not vs. hidden chips). It
revealed the mode is not one-size:

- **Accordion shines on a *tall* screen** (portrait phone: 3/5 sections open, the rest listed as
  collapsible headers one tap away, in-context, sequencer pinned).
- **Accordion *degenerates* on a *short* screen** (landscape phone: **0/5 open** — the headers +
  pinned sequencer eat the whole height, so nothing can expand). A short-wide screen wants **tabs /
  a segmented switch** (horizontal), not a vertical accordion.

So the disclosure **mode** is a per-*shape* choice (roomy → inline-all · tall → accordion · short-wide
→ tabs), and separately **some racks are better with rotation LOCKED** — a dense knob rack, or
otafamily's drag-along **ribbons** (which want horizontal pitch-length), may simply be better *always
landscape* than made to reflow both ways. A mediocre both-ways reflow can lose to one committed
orientation.

`otafit` ([`../../tools/carts/otafit.c`](../../tools/carts/otafit.c)) turned that orientation hunch
into a **number** on a third UI shape (four continuous drag *ribbons*, not tap widgets — and `lay.h`
placed them with no new primitive). A fretless ribbon's playability *is* its length (pitch resolution
= px/semitone), so it measures how many semitones a finger covers:

| shape | ribbon | finger covers | verdict |
|---|---|---|---|
| iPhone **portrait** | 110px | **3.1 semitones** | **CRAMPED** (can't hit a note) |
| iPhone **landscape** | 289px | **1.5 semitones** | **PLAYABLE** |
| iPad portrait / landscape | 192 / 301px | 1.8 / 1.0 | playable |

So on a phone this rack is unplayable in portrait, playable in landscape (2.2× the ribbon) — a clean
**`lock-landscape`** recommendation, now expressed as a threshold (a finger covering ≤~2 semitones)
that the `layout-check` oracle can read. That's the difference between "ribbons feel like they want
landscape" and a checkable rule.

**Phase 0's shape space is now covered** — clean widgets (`rackfit`), dense multi-section (`acidfit`),
and continuous drag-surfaces (`otafit`) — plus disclosure, orientation, overrides, and the promoted
`lay.h`. More cart-land mocks would only re-confirm; the next lever is Phase 1 (the engine).

**Both are CHECKABLE, and both can be true for one rack.** The fit decision is deterministic
(finger-budget math), so a **`layout-check.js` oracle** (twin of `mobile-lint` / `ui-audit`) can sweep
a rack across the device×orientation matrix and report, per shape: the mode it would pick, sections
open vs. deferred, min control size (finger-%), wasted space, and **degenerate flags** (0 sections
open, sub-finger controls, a collapsing mode). **When it flags sub-finger controls it should name the
remedy** — reflow, or the standing fat-finger fix `ui_loupe(1)` (the `ui.h` 3× magnifier,
[`loupe-notes.md`](loupe-notes.md); swept grids/keybeds → `gestures.h` pinch-zoom). `mobile-lint.js`
already recommends the loupe *today* for literal tiny `tap()/tapp()` targets (its `tiny-target`
warning); the oracle extends that to the device-shape-dependent case a grep can't see (a control fine
on iPad Pro but sub-finger on an iPad mini or a phone). From that it recommends an **orientation
policy** —
`free` / `lock-landscape` / `lock-portrait`, a per-rack manifest field the shell honours (iOS
orientation is per-view-controller, so a multi-cart app can constrain per *active* rack). The
prototypes (`rackfit` / `acidfit`, and next `otafit` for the ribbon/orientation case) are the *manual*
version of this oracle today; it graduates alongside the engine work. This is the responsive-era twin
of the existing render/audio oracles in [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md).

**REFINED (2026-07-05): not a separate tool — a judgment layer on `ui-audit`'s existing matrix.**
`ui-audit --explore --resize "WxH,…"` already owns the expensive plumbing (the size×state sweep,
per-size findings, waivers, overlays) — but it's a *defect* oracle (clipped/overlapped/dead), blind
to *judgment* (which mode was picked, sub-finger controls, a missing arrangement) and blind to
*absence* (it can't flag what was never drawn — field note 018's core lesson). Rather than inferring
judgment from pixel boxes, **`disclose.h` self-reports its decision** into the same `--uiaudit`
JSONL under `DE_TRACE` — one record per size: mode chosen, sections inline/compact/deferred, min
control size in finger units. The audit then adds a thin pass over those records: flag `inline==0`,
`min_control < 1fu` (naming the remedy — reflow / `ui_loupe` / lock-orientation), a mode override,
and arrangement-vs-brief mismatches. Two matrix gaps to close while there: per-size exploration only
drives *keys* (`ui-audit.js:139`), never widget taps — so tap-only panels (acidrack's strip headers)
are never opened per size; `disclose.h` enumerating its own sections as openable states fixes this
*by name* instead of by guessed coordinates.

### Overrides: emergent defaults, but always an escape hatch (2026-07-03)

The emergent rules produce a *default* — the author must be able to **overwrite a conclusion "just
because"**, both in *positioning* and in the *decision*:

- **Positioning (`position: absolute`) is already in `lay.h`.** `lay_at(container, anchor, w, h,
  inset)` *is* CSS `position:absolute` — pin a box to a container corner/edge/centre, out of the flow
  (the back-to-root chip in `acidfit` is exactly this: pinned top-right no matter what the reflow
  does). And raw `box(x, y, w, h)` is the ultimate hatch — literal coordinates against any reference.
  Because it's immediate-mode, **nothing forces you through `split`/`wrap`**; you can always just
  place a rect.
- **Decision override is plain-C, not a framework fight.** The disclosure/orientation rules are
  ordinary code, so an author override is just writing the exception — `if (device_is(PHONE_L)) mode
  = TABS;`, `expanded |= 1 << FAVOURITE_SECTION;`, `policy = LOCK_LANDSCAPE;`. There's no retained
  layout engine to escape. The model is **emergent defaults + plain-C overrides.**
- **The oracle should *flag* overrides, not forbid them** — "this rack overrode the recommended mode
  on iPhone-L" keeps the override a *declared, visible* choice (a line of code / a manifest flag),
  never a silent hack. Same spirit as a waived lint.

### Is the layout vocabulary complete? (reviewed 2026-07-03)

Empirical test: what did writing a real rack (`rackfit.c`) make us hand-roll? That's the gap. Two
primitives were missing and are now added to the candidate set (in `respond.c` + `rackfit.c`):

- **`lay_split(c, edge, size, &rest)`** — dock a fixed-size band off one edge, return the remainder
  (CSS flex fixed-basis + `flex:1` sibling). The app-chrome workhorse — title/tab/tool bars, keybed.
  `rackfit` hand-computed this before; docking reads far cleaner. **Added.**
- **`lay_pad(c, t,r,b,l)`** — per-side inset (CSS padding). Uniform `lay_inset` can't express an
  **asymmetric** safe-area (a notch tops one edge, the home-bar the opposite). `lay_inset` is now
  just `lay_pad(m,m,m,m)`. **Added.**

The full candidate `lay.h` set is now: `clamp` · `fluid` · `pad`/`inset` · `at` (9-grid anchor) ·
`split` · `cell` (equal flex) · `wrap` (auto-fit grid) · `aspect`.

**Deliberately still out** (composable or out of scope): a fixed-count grid (`lay_grid` — the
hand-picked alternative to auto-`wrap` that the 14+2 step-wrap finding wants; cheap nice-to-have, not
yet added); weighted flex (proportional children — composable from `split`); full CSS Grid with spans
(overkill); **scroll/overflow** (that's *interaction*, not layout — the "too many steps/keys to fit"
case wants a scroll-or-page pattern the cart drives); media/container-query *syntax* (a plain
`if (device_class()==PHONE)` is the query — and the real query variable is the **finger-ratio**, not
raw px). Alignment-within-a-cell is already covered by `lay_at(cell, L_C, …)`; z-order by draw order.

**The back-to-root corner keep-out is covered by the existing vocabulary** — `lay_at` for the chip
rect + `lay_pad` to inset the colliding row off that side (demoed in `rackfit.c`). No new primitive;
the only new thing needed is the engine handing the cart the keep-out rect (plumbing #5 above).

## The store-asset payoff

Today's `store-shots.js` exists *because* the cart can't render at real device aspect ratios: it
**composites, doesn't stretch** — integer-upscales the little frame onto a marketing background with
a caption in the leftover space. Clever, but the screenshot isn't what the app actually looks like.
Once racks render natively at any viewport:

- **Screenshots become truthful, full-bleed.** Hand the cart the exact device viewport
  (e.g. iPhone 6.9" 1320×2868), it lays out *for that device*, screenshot it — no background trick,
  no bars. The store image *is* the app on that device.
- **App Preview videos stop being the hard case.** You can't composite a video the way `store-shots`
  fakes a still, and a letterboxed video reads as broken — but a full-bleed rack →
  `make-gif.js`/`play.js` captures a real clip at exact device resolution + orientation.
- **All required sizes from one cart** (6.9" / 6.5" / iPad 13" / portrait / landscape) — same rack,
  different viewport handed in.

The `store-shots`/`store-contact`/`make-gif` pipeline doesn't disappear — it gets *better inputs*
(real device-resolution frames instead of a 320×240 upscale). The framed-on-a-marketing-bg treatment
stays available as an *optional* style; full-bleed becomes the honest default.

**One gap this doesn't close: the *input track* must also survive the reflow.** A video needs a
demo performance, and a *touch* track is absolute canvas pixels — so a knob tap that's right at
320×240 lands on empty space once the rack rearranges for a phone. The plan is **keyboard-first**
(a rack demos itself via transport/preset keys → one position-free track → identical audio + a
reflowed picture at every device size), with coordinate-portability systems parked as escape
hatches. Full note: [`resolution-portable-input.md`](resolution-portable-input.md).

## The phased plan (riskiest-cheapest first, prototype before engine)

- **Phase 0 — prove the model in cart-land (zero engine risk). ✅ DONE (2026-07-03) —
  [`../../tools/carts/rackfit.c`](../../tools/carts/rackfit.c).** `respond.c` already proved the
  `lay_*` primitives reflow; `rackfit.c` proves the *design claim this doc rests on*: a music rack
  (knob bank + 16-step strip + keybed) laid out against a fake device, with **every control sized to
  a physical 44pt finger** (points→canvas scale, so the finger-to-screen ratio is honest). The
  arrangement is **emergent** — `lay_wrap` flows the knobs/steps into as many finger-sized columns as
  fit, and the keybed fills with as many finger-wide octaves as fit — **no per-device layout branch.**
  Result across the four presets (auto-cycle; keys 1–4 lock): **iPhone portrait** wraps the knobs to
  **2 rows** + **1 octave**; **iPhone landscape / iPad** get **one row of 8** + **3 octaves**.
  Findings: (a) the model holds — density × orientation drives real rearrangement from one code path;
  (b) **orientation matters as much as device class** — iPhone *landscape* behaves tablet-like
  horizontally (finger = 5% of width, like an iPad), so branch on the *measured* finger-ratio, not the
  device name; (c) 16 steps at half-finger wrap awkwardly (14+2) on a narrow phone — real racks want an
  explicit "16 in one row, smaller" vs "2×8" choice, i.e. some topology *is* worth hand-picking, not
  everything should be left to `wrap`. View: `node tools/play.js rackfit run --headless --screen
  360x360 --frames 460 --dump build/.rackfit`. **Dense-rack stress test also DONE** —
  [`../../tools/carts/acidfit.c`](../../tools/carts/acidfit.c) reproduces acidrack's real 5-section
  inventory and adds the progressive-disclosure behaviour layer (see "Two layers" above): a dense real
  panel *does* stay legible on a phone — because sections it can't fit comfortably collapse to tabs
  rather than cramming. Phase 0 is fully de-risked; the layout model + toolkit are validated against
  both a clean and a dense rack. **What's left is genuinely Phase 1+ (engine).**
- **Phase 1 — engine: live-resizable dims. Do it in three sub-steps** — the per-cart opt-in is the
  phasing lever, so the risky refactor never changes observable behaviour and no "flag day" ever
  happens:
  - **1a — plumbing, invisible. ✅ DONE (2026-07-03), verified byte-identical.** Added file-scope
    `de_sw`/`de_sh` (init to `SCREEN_W/H`, unchanged for now) + re-pointed `de_screen_w()`/
    `de_screen_h()` at them (increment 1), then converted every **standalone render-EXTENT site** —
    mouse clamps, camera centers, boot/pause centering, `gr_place`, present source/dest rects, shader
    texel size, `project()`, viewport, camera-follow clamp (increment 2). `drawall` renders pixel-diff
    = 0 across 20 frames vs the pristine baseline; 6 camera/scroll/mouse/3D carts run clean. The two
    compile-time-required sites (static framebuffer sizes, the static `Camera2D` initializer) correctly
    **stay on the macro** — the compiler catches them, and `camera()` recomputes the offset at runtime.
    **What deliberately stays on the macro** (the 47 remaining uses): buffer allocs / `LoadRenderTexture`
    / window init / `SMOOTH_*` / the `tcc_define` cart constant — all **permanent max**; plus the
    **SW-rasterizer stride/flip/clip-bounds** (`(SCREEN_H-1-sy)*SCREEN_W`, `x1>SCREEN_W`, full-buffer
    clears, `pget` snapshot bounds, world-capture) — **not leftover extent sites but coupled to 1b's
    sub-region-layout decision**, so they convert *there*, not blindly here (they'd be byte-identical
    at `de==max` but could bake a wrong layout assumption). No standalone extent site remains.
  - **1b — the opt-in switch + the GROWABLE framebuffer. ✅ DONE (2026-07-03; grow 2026-07-04).**
    Per-cart `-DDE_RESIZABLE` → a runtime `de_reflow` bool (clang compiles `cart.c`+`studio.c`
    together, so the one flag reaches both TUs). OFF (every existing cart) → fixed window, globals
    pinned at boot, identical to today. ON → `FLAG_WINDOW_RESIZABLE` + `de_set_canvas(window/SCALE)` on
    `IsWindowResized()` and at boot. **The sub-region-vs-realloc fork was cut sub-region first, then
    RE-RESOLVED as grow-only realloc** — the fixed max letterboxed when enlarged and top-pinned when
    narrowed. Final: `sw_cbuf`/`sw_world_buf` are heap, grown by `de_ensure_fb` (+ GPU
    `canvas`/`canvas_snap`/smooth via `de_grow_gpu`) to a high-water-mark `fb_w×fb_h` clamped to
    `DE_MAX_DIM` (4096); stride is runtime `fb_w`, flip origin → `de_sh-1`, present samples
    `(0,0,de_sw,-de_sh)`. `pget_snapshot` keeps its own flip; the `cls`/world-clear cover the whole
    (now `fb`-sized) buffer. Cart API `screen_w()/screen_h()` added (4 places). Byte-identical for a
    fixed cart (never grows → `fb_w==SCREEN_W`; SHA-verified on `drawall`). Blast radius = opted-in only.
  - **1c — flip ONE cart. ✅ DONE (2026-07-03).** `respond.c` reads the real `screen_w()/screen_h()`
    under `-DDE_RESIZABLE` (the fake-drag prototype is the `#ifndef` default, unchanged). Verified: a
    140×260 window stacks the button bar to a column, a 480×320 window keeps it a row — one code path,
    the OS window is the drag surface. First consumer is a **resizable desktop window** (no iOS),
    fully under the `canvas-diff`/`build-all` gates.
- **Phase 2 — physical sizing + rotation on iOS.**
  - **FILL ✅ DONE (2026-07-04).** `de_resize`/`de_is_resizable` seam + `CanvasView` hands the engine the
    device point-viewport → a resizable cart fills the device (verified iPhone SE / 15 / iPad Pro 12.9).
    Live realloc on size change falls out of the growable framebuffer. `build.sh RESIZABLE=1`.
  - **safe-area + rotation ✅ DONE (2026-07-04).** `de_set_safe_area` seam (from `safeAreaInsets`) +
    cart `safe_rect()` → controls dodge the notch/home-bar (verified iPhone 15). Landscape allowed in
    Info.plist; `layoutSubviews`→`de_resize` reflows on rotate (confirmed on the sim). (iOS is SCALE=1
    so points==fb px today; a HiDPI/points-vs-px pass + a `device_class()` helper can come later.)
- **Phase 3 — reflow the Tinyjam racks (per-rack density arrangements).** `lay.h` is already shipped;
  what's left is *designing* each rack's arrangements — the media-query-like adaptation keyed on the
  finger-budget. **Genuine per-rack layout work, done one rack at a time — the backlog is below.**
- **Phase 4 — regenerate store assets honestly.** Render each rack at every required device size +
  orientation; feed real frames to `store-shots`/`make-gif`; full-bleed screenshots + videos.

## Phase 3 — revised plan (2026-07-05)

The first acidrack pass proved the failure mode this plan now guards against: the validated model
(finger budget, two layers, per-shape modes) lived in a prototype cart + this doc, and the
production work bypassed both — incremental `screen_w()` arithmetic that *reflowed* without
*redesigning*, judged done because the defect oracle was green. Full retrospective + the
generalisable rules: **field note
[018 — Passing the Gates Felt Like Done](../field-notes/018-passing-the-gates-felt-like-done.md)**.
The structural fixes below make the model the *path of least resistance* instead of a pointer. The
per-cart order-of-operations these fixes imply is distilled as a reusable runbook:
[`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md).

### The ReBirth study — what the original got right (2026-07-05, maker + screenshot session)

acidrack's homage rewards a second look: **ReBirth never shows note data as an XY grid — every
machine edits one lane at a time.** The 303s use a *step programmer* (one octave of piano keys +
DOWN/UP + ACCENT/SLIDE flags + a STEP counter — the current step's pitch, never all 16 at once);
the drum machines show per-voice knob columns + a voice-selector row + **one shared 16-step lane**
(11×16 = 176 cells reduced to 16 on screen). Chrome is rigidly positional (same left pattern/bank
rail + right mix strip on every machine — learn the anatomy once), and nearly text-free: LEDs,
7-seg displays, icon-sized buttons. Caveat: ReBirth also cheats with a **pixel-precise mouse** —
steal the *information architecture*, never the control sizes; the finger budget still governs
sizing. Two transferable moves:

- **The three-state strip.** Between "folded header" and "full-screen editor" sits ReBirth's
  machine module — the **compact** state (~3–4 finger-rows): the 2–3 macro knobs you ride live +
  one finger-sized step lane + mute/fx. *Playable without expanding.* States: **folded** (name +
  mini-pattern + mute) → **compact** (the ReBirth module) → **expanded** (full editor). The
  disclosure pass promotes strips folded→compact→expanded by finger-budget. Payoff: **iPad = all
  strips at compact simultaneously** (the whole ReBirth rack on one screen — the flat/see-and-touch
  arrangement §8.2 of the design system asks for, which the first pass never built); phone = the
  working strip expanded, 1–2 others compact, rest folded.
- **Editor-swap by budget** (the panel-level twin of [design-system](design-system.md) §8.3's widget swap). Same
  `Line` data, two editors: roomy shapes keep our piano roll (a better overview than ReBirth ever
  had); a phone swaps to the ReBirth step programmer — one octave of finger-wide keys + flag
  buttons + step advance *fits a phone width at full finger size*. Same for drums: full grid on
  iPad, voice-selector + single lane on phone.

House rules for every rack (maker, 2026-07-05): **decide what to show — never try to show
everything, but everything stays reachable** (that's what disclosure is); **good icons are smaller
than text** — this is a fantasy console, not an accounting app; **use the fonts and colours we
have** (`FONT_TINY`→`FONT_LARGE`, the pico32 palette, design-system §7 roles); **no shame in
stealing from the homage hardware** — the original solved this screen already.

### The steps (each has a customer the moment it exists)

- **R1 — the layout brief, per rack (write it BEFORE layout code).** A half-page committed spec —
  `docs/design/<cart>-layout-brief.md` or a section in the rack's design doc: the section inventory
  with **priorities + min footprints in fingers** (now ×3: folded/compact/expanded), what's
  **pinned** (the performing surface that never hides), the **arrangement per shape class**
  (roomy / tall / short-wide), the **widget-swap + editor-swap table**, precision hatches (loupe
  where), and the **orientation policy**. This is the `seo-brief` palette/mirror pattern applied to
  layout: the brief is the palette; "done" = *matches the brief on device*, not "no audit findings".
  It's also where the taste calls get made visibly instead of implicitly in C.
- **R2 — graduate the behaviour layer: `runtime/disclose.h`.** The keystone. **STARTED 2026-07-10 —
  `runtime/disclose.h` shipped with the CORE + its proof** (`acidwire` now calls it, commits
  `27637b26`/`d96c4404`): `disclose_shape()` (TALL/WIDE/ROOMY), `disclose_budget()` (the finger-budget
  accordion pass — working EXPANDED, rest FOLDED→COMPACT by priority), `disclose_stack()` (place them).
  **Deferred (not yet in the header):** the WIDE tab bar + ROOMY grid are still cart-side lay.h layouts
  (they show one state, so no *disclosure* — arguably they never need to move in); the tab-chip/promote
  interaction and the `DE_TRACE` self-report are R4. Reassess whether tabs/grid should graduate when the
  second rack (epiano) adopts disclose.h. Original spec below:
  `acidfit`'s priority +
  footprint budget pass becomes a header (design-system §8.5 already admits this gap). Sections
  declare `{name, priority, footprints[3] in fu}`; the header runs the per-frame pass, picks the
  mode per shape (inline-all / accordion / tabs), owns the promote/demote + tab-chip/header
  interaction, hands the cart `(section, state, Box)` tuples. Plain-C overrides stay
  (`mode = TABS;`) per §Overrides. Under `DE_TRACE` it **self-reports its decision** to the
  `--uiaudit` stream (see the REFINED oracle note in §"Both are CHECKABLE") and enumerates its
  sections as openable states so the matrix can drive tap-only panels by name. **Boundary +
  move/stay split** (sharpened on glass in acidwire, 2026-07-10): the one-liner is *disclose decides
  WHERE a panel goes and how big; the cart decides WHAT lives inside it* — content-paging inside a
  section (e.g. acidwire's N/K/F) stays cart-side. Full split in
  [`acidrack-layout-brief.md`](acidrack-layout-brief.md) §2 "Validated on glass".
- **R3 — make the finger unit real. ✅ DONE (`7102af8b`, 2026-07-10).** Engine: `finger_px()` (44pt
  in current canvas px, from a **backing scale** — new `de_set_backing_scale()` seam; iOS `CanvasView`
  reports its `pixelChunk` (2) → 22) + the `device_class()` convenience (TALL/WIDE/ROOMY, the
  `disclose_shape` twin). Both are studio.h cart API (4-places). `acidwire`'s `FU` now calls
  `finger_px()` instead of a hardcoded 22 — behavior-identical today, but follows a changed chunk /
  HiDPI. Verified on device (iPhone SE), `build-all` 481/481, `build-nr`.
- **R4 — the judgment oracle = ui-audit extension**, not a new tool. Spec'd in the REFINED note
  above: a thin pass over `disclose.h`'s self-reported records — degenerate flags, sub-finger
  controls with named remedies, mode overrides, arrangement-vs-brief mismatch.
- **R5 — re-land acidrack on the method.** Its brief is STARTED —
  [`acidrack-layout-brief.md`](acidrack-layout-brief.md) (the R1 worksheet: three-state sketches,
  arrangements, done-bar; **§2's compact-strip taste calls await the maker**). Then replace the hand-rolled
  threshold block (`acidrack.c:459-519`) with `disclose.h` + `lay.h` + finger units, **build the
  missing roomy arrangement**, and swap editors per budget. Verify: R4 matrix + **bake it and put
  it in the maker's hands per arrangement** — the on-device eyeball is part of the loop, not the
  post-mortem.
- **R6 — `epiano` is the test of the method itself.** Done fresh from a brief; if R1–R4 worked it
  costs a fraction of acidrack, and its keybed reflow graduates into `keybed.h` (every keybed cart
  wins). `yachtrack` last, as before.

### 2026-07-06 — resizable-app PLUMBING landed + three findings (before R5's redesign)

Got the multi-cart **Tiny Jam app** to actually reflow to fill the device on the sim (it was locked
320×240 letterboxed — the resizable path only existed for single-cart builds). Committed `be7b2cad`.
This is the plumbing *under* R5; the acidrack redesign (R1→R5, the §2 taste calls) is still the work.
What shipped: `RESIZABLE=1` opt-in on `ios/build.sh`'s `APP=` path; the **launcher menu made
reflow-aware** (`tinyjam-menu.c` lays out inside `safe_rect()`, centered width-capped column); the
**app home-chip** (`build-app.js`) moved inside the safe area (was stuck under the notch → couldn't
get back to the overview); acidrack's **transport + chain row inset by `safe_rect()`** (`saf_t`/`saf_b`
helpers — the notch/home-bar dodge, R5-adjacent but done early since untappable-under-notch is a
correctness bug, not polish). **Device matrix committed** as the design baseline:
[`acidrack-device-matrix.png`](acidrack-device-matrix.png) + regen recipe in
[`acidrack-layout-brief.md`](acidrack-layout-brief.md) §7. **Promoted 2026-07-07 to its own carried
reference — [`device-matrix.md`](device-matrix.md)** (grown to 11 shapes incl. iPhone SE + iPad mini;
§3 adds the App Store asset pixel sizes for the store pipeline). Design every rack against §2 there.

Three findings worth remembering:

1. **Pixel chunk K (the "physically-sized" knob), `CanvasView.swift` `pixelChunk`.** iOS is SCALE=1,
   so reflowing to the full point size (iPhone 16 = 393 pts wide) made each engine pixel = 1 pt →
   **hi-res tiny pixels** (lost the lo-fi look) AND sub-finger controls. Fix: the canvas reflows to
   `points / K` **logical** pixels. **K=1** tiny; **K=3** so chunky the fixed 8px font overflows a
   phone width (only ~16 chars fit — "unlock all – $5.00" ran off both edges); **K=2 is the sweet
   spot** (chunky + text fits). Set to 2. This is the concrete form of R3's finger unit — but note it
   also confirms the brief's "**good icons are smaller than text**": the chunkier you go, the more
   text must become glyphs.
2. **`de_reflow` is BINARY-WIDE (compile-time), not per-cart.** So with `RESIZABLE=1` on, the *whole*
   app is resizable — the fixed menu was fixed by making it reflow-aware, but **yachtrack + epiano are
   NOT reflow-aware** (no `screen_w()`/`safe_rect()` use) so they render in the top-left 320×240 band
   of the filled canvas. The clean fix is **per-cart `de_reflow`** (a runtime flag set on
   `de_switch_cart` from each cart's `resizable` meta, so a fixed cart *centers/letterboxes* instead
   of corner-parking) — a real but separate engine change. Backlog item; acceptable for now while only
   acidrack is being designed.
3. **SEAM — desktop live-resize freezes the music (tried to fix, BACKED OUT; do not re-attempt the
   GLFW-callback route).** On desktop, dragging the cart window's edge runs the macOS **modal event
   loop, which blocks the main thread** ([glfw #2008](https://github.com/glfw/glfw/issues/2008), a
   documented limitation). The transport is main-thread + wall-clock (`beat()`→`GetTime()`), but audio
   synthesis is a separate thread (`SetAudioStreamCallback`), so during the drag the last-scheduled
   voices ring out / drone while the sequencer freezes → "the music hangs." We tried pumping the
   transport from GLFW's **window-refresh** callback and then the **window-size** callback (chaining
   raylib's) — **both fire ZERO times during the live drag on this macOS** (instrumented + confirmed),
   so there's no hook to pump from. The only remaining route is a background timer/display-link thread
   calling `update()`, which reintroduces the cross-thread-race class we'd just fixed on iOS — not
   worth it. **This is desktop-only and cosmetic**: on iOS "resize" is a *rotation* (one discrete
   `de_resize`, not a sustained modal drag), verified glitch-free on the sim. Left unfixed by choice.

**Resume at — the WIREFRAME CART (maker's call, 2026-07-06; chosen over an HTML mock).** Extend
[`tools/carts/acidfit.c`](../../tools/carts/acidfit.c) (the existing disclosure prototype) into the
acidrack layout wireframe: the 5 strips as folded/compact/expanded boxes + the transport + song row,
laid out with `lay.h`, viewed across the 3 shapes via `play.js --resize` (the matrix sizes) and
hot-reloaded in libtcc live mode. It's the vehicle for making §2's compact-strip taste calls — but in
the REAL medium, so the decisions land in code we keep. **Why a cart, not HTML** (weighed the fork):
an HTML mock iterates fast + shows all shapes at once, but a cart has **no translation gap** (a cart
IS the production path — an HTML→C hand-off is the exact field-018 hazard), it **validates
`lay.h`/`disclose.h` can actually express the arrangement** (CSS is a superset — easy to mock
something unbuildable), and it renders **real fonts / finger sizes / safe-area insets on device**. The
cart's supposed downsides mostly evaporate: libtcc live-reload closes the iteration gap, `--resize`
gives the all-shapes filmstrip. **If the wireframe needs a layout primitive `lay.h` lacks, ADD IT TO
`lay.h`** — discovering those gaps is itself R2/R3 groundwork, not a detour. Then R5 proper. Open decisions parked: the **landscape side-notch** inset (acidrack
only insets top/bottom, not `saf_l`/`saf_r` — landscape puts the notch on the side; needs a screenshot
of the bad spot) and **background-audio** policy (keep-playing vs pause-on-background; no
`UIBackgroundModes: audio` today → device suspends on Home, but the sim doesn't, so it *looks* like it
keeps playing).

## Phase 3 backlog — the three Tinyjam racks (`acidrack` · `yachtrack` · `epiano`)

Phase 1–2 make a cart *able* to fill/reflow; Phase 3 is *designing* each rack's arrangements (density
adaptation — CSS "Priority+" keyed on the finger-budget, not the device name). Pick up **one rack at a
time**. Per-rack recipe: build it resizable (`RESIZABLE=1 CART=<rack> ./ios/build.sh` on the sim, or
`DE_DEFINES=DE_RESIZABLE node tools/play.js <rack> --resize "WxH,…" --show-size` on desktop), then give
it **2–3 arrangements + a disclosure mode per shape** (inline / accordion / tabs, per `acidfit`) using
`lay.h`. All three are 320×240 today, packing everything into that fixed frame. Each also carries a
`de:meta.todo` pointing here (surfaced by `cart-todos.js`).

- **`acidrack` — the RB-338 acid rack. START HERE — via the revised plan (R1→R5 above), not by
  patching the first pass.** A first pass landed (2026-07-04/05: fill-width reflow, a 2-row narrow
  transport, a content-aware tabs fallback — `acidrack.c:459-519`) but it's hand-rolled px math with
  **no `lay.h`, no finger unit, no disclosure pass, and no iPad/roomy arrangement**; the maker's
  device test judged it far from done (field note 018). Target (unchanged, now + the three-state
  strip): **iPad** = all strips at **compact** (the ReBirth-module state — the rack on one screen),
  expand-in-place for deep edits; **phone-portrait** = working strip expanded, 1–2 compact, rest
  folded, song row pinned; **phone-landscape** = tabs. Editor-swap: piano roll on roomy shapes, the
  ReBirth step programmer on phones. Sequencer/transport pinned. **Brief started:
  [`acidrack-layout-brief.md`](acidrack-layout-brief.md)** — §2's compact-strip taste calls
  (what earns a slot per machine) await the maker; everything else resumes from there.
- **`epiano` — electromechanical keybed. DO SECOND (broad payoff).** Today: the shared `keybed.h` keybed
  + machine selector (RHODES/WURLI/CLAV) + per-machine stompbox pedals + BARK knob + preset column.
  Target: **keybed width scales to the device** (more octaves on iPad / landscape — the `otafit` finger-
  per-semitone lesson), pedalboard reflows (phone-portrait: compact pedal row; iPad: full board beside
  the keys). **Likely graduates into `keybed.h`** so the reflow benefits *every* keybed cart (moog/
  touchpiano/mellotron) — a shared win, not per-cart. A keybed wants landscape width; consider a
  `lock-landscape` hint on phone-portrait.
- **`yachtrack` — the chord-first yacht rack. DO LAST (spec its sections when you pick it up).** Today:
  chord row (4 slots) + form row (8) + drum skeleton (5×16) + sax hook (32-cell) + band rows + desk/
  transport, also an "accordion is pure view". Target: the **CHORD CHART is the star** → always primary;
  phone shows chart + form inline with drums/hook/band one tap away (accordion/tabs), iPad shows more
  inline. Transport + song-code pinned. Study its sections before speccing the exact arrangements.

## Open questions / risks

- **Pixel-art crispness under point-based scaling.** Lo-fi art wants integer scaling; a point-sized
  viewport at a device backing scale may land on non-integer factors. Ties into
  [`pixel-perfect-scaling.md`](pixel-perfect-scaling.md) (the shipped `scaleFilter` / sharp-bilinear
  shader) — the reflowed layout still renders lo-fi *within* fluid rects, so this is a per-rack render
  choice, not a blocker. Worth an early check in Phase 0/1.
- **The dense racks are real redesign work.** acidrack fills 240px; a beautiful phone-portrait
  arrangement of it is genuine design, not automatic. Budget it per rack; it's the bulk of Phase 3.
- **AUv3 render size is host-driven** and differs from the standalone app — out of scope for this
  pass; noted so it isn't assumed solved (ios-plan spike 7 / 9).
- **Which arrangements per rack** is a per-rack taste call made during Phase 0/3, not specced here.
