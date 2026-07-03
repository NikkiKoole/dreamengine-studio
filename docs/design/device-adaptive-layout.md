# Device-adaptive layout — one cart, beautiful on iPhone AND iPad, both orientations

STATUS: READY TO BUILD (2026-07-03) — prototype-first. The model and the phased plan
are settled; nothing engine-wide moves until the model is proven against the fake screen in
[`../../tools/carts/respond.c`](../../tools/carts/respond.c). This is the **execution + product** doc
that graduates the deferred thinking now that there's a concrete need (Tinyjam on the App Store).

**Where this sits among the three sibling docs — they are NOT duplicates:**
- **This doc** = the *engine change + product plan*: make `SCREEN_W/H` runtime + physically-sized,
  handle rotation/device-class, and the phased ladder to ship it.
- [`responsive-layout.md`](responsive-layout.md) = the *cart-land toolkit* it graduates
  (`lay_*` — flex / fluid / anchor / aspect / wrap), already prototyped in `respond.c`.
- [`window-fill-scaling.md`](window-fill-scaling.md) = the *rejected alternative* (keep the canvas
  fixed, fill the window with a fractional scale). The maker's call (2026-07-02): reflow wins for a
  sellable app — a scaled poster reads as a port, a reflowed UI reads as an app.

Supersedes the "deferred, no concrete need yet" framing in `responsive-layout.md` §gate and
[`share-panel.md`](share-panel.md) next-spike #3, and answers [`ios-plan.md`](ios-plan.md) umbrella
backlog #2 (multi-resolution racks).

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
  tablet = many). Drives *how many controls* and *how big*.
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

**Both are CHECKABLE, and both can be true for one rack.** The fit decision is deterministic
(finger-budget math), so a **`layout-check.js` oracle** (twin of `mobile-lint` / `ui-audit`) can sweep
a rack across the device×orientation matrix and report, per shape: the mode it would pick, sections
open vs. deferred, min control size (finger-%), wasted space, and **degenerate flags** (0 sections
open, sub-finger controls, a collapsing mode). From that it recommends an **orientation policy** —
`free` / `lock-landscape` / `lock-portrait`, a per-rack manifest field the shell honours (iOS
orientation is per-view-controller, so a multi-cart app can constrain per *active* rack). The
prototypes (`rackfit` / `acidfit`, and next `otafit` for the ribbon/orientation case) are the *manual*
version of this oracle today; it graduates alongside the engine work. This is the responsive-era twin
of the existing render/audio oracles in [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md).

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
- **Phase 1 — engine: live-resizable dims.** Make `studio.c`'s ~76 sites read the active target;
  realloc the framebuffer on resize; add `screen_w()`/`screen_h()`. First consumer is a **resizable
  desktop window** (testable with no iOS at all), fully under the `canvas-diff`/`mirror-diff` gates.
- **Phase 2 — physical sizing + rotation on iOS.** Pass the point-viewport + backing scale through
  the iOS layer; add `device_class()` + `safe_rect()`; enable rotation in `Info.plist`; live
  framebuffer realloc on orientation change.
- **Phase 3 — graduate `lay.h` + reflow the Tinyjam racks.** Move the toolkit to `runtime/lay.h`;
  give each rack its 2–3 arrangements. **This is genuine per-rack layout work** — acidrack fills all
  240px densely today, so it's a real redesign, done rack by rack (see risks).
- **Phase 4 — regenerate store assets honestly.** Render each rack at every required device size +
  orientation; feed real frames to `store-shots`/`make-gif`; full-bleed screenshots + videos.

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
