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
   of the `de_safe_top()` nav-bar idea (ios-plan backlog #3).

**Determinism guard (non-negotiable, from responsive-layout.md §4c):** runtime dims must **not** be
forced global. ~45 carts and the `canvas-diff`/`mirror-diff`/`spec` gates lean on fixed dims. A cart
that ignores the new API renders at a fixed size and letterboxes exactly as today — **reflow is
opt-in per rack.** Runtime-dims changes *how studio.c reads the size*, not whether a given cart
varies. Fixed-canvas carts compile and behave unchanged; their runtime value is simply pinned at boot.

## The cart model (graduating `lay.h`)

- The `lay_*` toolkit (flex / fluid / anchor+inset / aspect / wrap) graduates from the `respond.c`
  prototype to `runtime/lay.h` **unchanged** — it's deliberately rect-in/rect-out, so the only
  difference on graduation is passing the real viewport instead of the fake draggable rect.
- **Immediate-mode is what makes rotation free:** every rect is recomputed from the current viewport
  each frame, so a rotation is just "next frame the container is a different shape," and hit-testing
  recomputes with it. Audio/knob **state lives separate from layout**, so it survives relayout
  untouched.
- A rack's `draw`/`update` branch on `device_class()` + orientation into one of its 2–3 arrangements,
  then lay out fluidly within the chosen one.

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

- **Phase 0 — prove the model in `respond.c` (zero engine risk).** It already demos a row→column flip
  + fluid type against a draggable fake screen (~70% there). Extend it to show a **density**
  breakpoint (phone: ~6 fat controls → iPad: the full dense panel, not just a `dir` flip) and treat
  orientation as first-class, laying out **one real rack** (acidrack) so its phone/iPad/rotation
  behavior looks genuinely *beautiful* against the fake screen. If it isn't delightful here, we found
  out for free — before touching `studio.c`.
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
