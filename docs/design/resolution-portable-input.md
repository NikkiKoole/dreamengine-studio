# Resolution-portable input tracks — one demo, every device shape

STATUS: EXPLORATION / IDEA (2026-07-05). Keyboard-first, deliberately un-designed beyond the
first rung. The point of this doc: name a tension the moment two shipped systems collide, pick
the cheapest thing that could possibly work, and **write down the escape hatches so we reach for
them only when keyboard-first actually fails** — not build a coordinate-portability system upfront.

## The collision

Two things that already work, pulling against each other:

- **The replay system** ([`../guides/debug-harness.md`](../guides/debug-harness.md)) — a committed
  input track (`tools/clips/<cart>/NN-label.{script,beats,rec}`) drives a cart deterministically.
  It powers debugging, the clip/reel video baker (`make-gif.js` → `compose-clips.js`), and the
  proposed [`attract-mode.md`](attract-mode.md) demo loop. One injection engine, three consumers.
- **Device-adaptive layout** ([`device-adaptive-layout.md`](device-adaptive-layout.md), Phases 0–2
  DONE) — a resizable cart *reflows* per device: iPad shows 5 sections inline, iPhone-portrait
  collapses to an accordion, iPhone-landscape to tabs. Layout is a function of the viewport.

The collision: **we want App Store videos of the racks (`acidrack`/`epiano`/`yachtrack`) at every
required device size + orientation** — and those are exactly the carts whose *layout changes most*
across those sizes, and whose input tracks are recorded as *absolute canvas pixels* — the one
encoding that doesn't survive a reflow.

## Why input isn't one thing — the portability table

A track is a stream of injected input, and the three flavours reflow very differently:

| Input kind | Script form | Survives a resolution/layout change? |
|---|---|---|
| **Keyboard** | `down 90 SPACE` | ✅ **Fully portable** — `btn()`/`key()` are position-free. |
| **Mouse / touch** | `click 120 `**`204 135`** | ❌ **Absolute canvas pixels** — reflow moves the widget; the tap lands on empty space or the wrong control. |
| **Audio (as a consequence)** | — | ✅ **Identical at every resolution** — the beat advances from the per-frame `dt`, not the pixel grid, so a keyboard track renders **byte-identical audio** at any viewport (see debug-harness "Why it's possible"). |

That last row is the quiet gift. A keyboard-driven demo gives you **one performance, one soundtrack,
reflowed picture** across iPhone/iPad/portrait/landscape — which is exactly the App Preview video
brief. The `device-adaptive-layout.md` §"store-asset payoff" already assumes this shape (hand the
cart a device viewport → it lays out for that device → `make-gif` captures a real clip); the only
gap it doesn't spell out is that the *input track* must produce the same performance across
viewports. For keyboard tracks, it already does. For touch tracks, it doesn't.

## The plan: keyboard-first, escape hatches parked

**Rung 0 (the bet) — make racks demo themselves from the keyboard.** For the video use-case, drive a
rack the way a self-playing cart runs: keyboard hits **transport** (play/stop) + **pattern / preset
cycling** — all position-free — and the sequencer plays itself. The only thing that varies per
device is the reflowed layout being *captured*; the input track is one file, the audio is identical.
This is `flank`'s "boots into the firefight, walk in with WASD" trick (debug-harness §reel gotchas)
applied to racks, and it matches attract-mode's own note that radios/generative toys are "already
self-playing." **Zero engine work, zero new track format** — it's a per-rack authoring property:
*is this cart keyboard-drivable enough to demo itself?* Where the answer is no, that's a small,
nameable set of shots (the ones that must *show a finger* on a control), not the common case.

If Rung 0 covers the store videos — and the bet is it covers most of them — **we stop here** and
none of the systems below get built.

**A second reason keyboard-first wins:** a self-playing demo never has to *navigate to* a hidden
control (it drives transport and lets the rack play), so the shape-dependent "reveal it first"
problem (hatch B below) never arises. And a touch demo would sometimes legitimately **differ per
shape** anyway — showcasing "open the FX tab, peek inside" is a lovely beat on a phone but
*meaningless* on an iPad where nothing is hidden — which isn't coordinate-remapping at all. One
keyboard track that reflows for free beats N per-shape touch performances.

### Escape hatches (build only when keyboard-first demonstrably can't)

Ranked cheapest → most robust. Each is parked, not scheduled.

- **A — Normalized coordinates (fractional 0..1).** Store `click 0.63 0.67`, multiply by live canvas
  dims on replay. Cheap: a `play.js` script-parser + `--resize` change, no engine work. **Solves
  pure scaling** (a rack that scales rather than deeply rearranges, or a game with a fixed HUD);
  **does not solve reflow** — the fraction lands where the fraction is, not on the control that moved
  to a different arrangement. The right first hatch *if* a track needs to be portable but the cart
  only scales.
- **B — Semantic / widget-addressed input.** Record a *target*, not a pixel: "tap CUTOFF, drag to
  0.8." On replay the harness asks the cart *where CUTOFF is now* and taps there — the only thing
  that survives a genuine accordion↔tabs rearrangement. `ui.h` already exposes live widget rects
  (`ui_get_widgets()` / the `de_ui_audit` hook in `ui_end()`), but identity today is a pointer /
  label+rect hash (`UiWid.wid`), **not a stable cross-run name** — so this needs a widget-naming
  layer + a semantic record/replay path. Biggest lift; the correct answer for *interaction* demos
  and for attract-mode replaying a touch demo on a reflowing phone. Natural home for a
  `layout-check`-style oracle: "does this track still hit live widgets at shape X?" becomes checkable
  (twin of `ui-audit` / the planned layout-check in `device-adaptive-layout.md`).

  **B is bigger than a coordinate remap — it's a *navigation-path* remap (2026-07-05).** What
  changes across shapes isn't only *where* a control is, it's whether it's **reachable at all**. On a
  roomy iPad the control is always visible (one tap); on a tight iPhone the same control lives behind
  a **disclosure** — a tab to select, an accordion section to expand, a collapsed panel to open —
  that must be operated *first*. So the recorded intent isn't "tap CUTOFF at X," it's "**operate
  CUTOFF**," and replay must resolve, per shape: *is it reachable → tap; is it hidden → perform the
  reveal action, then tap.* That reveal step is itself resolution-dependent (required on
  iPhone-portrait, a no-op on iPad). This sits directly on top of the **behaviour layer** in
  [`device-adaptive-layout.md`](device-adaptive-layout.md) (progressive disclosure: inline /
  collapsed / tabbed / accordion, chosen by finger-budget) — semantic replay would query it for "the
  path to reach widget X in the current arrangement," possibly multi-step. That's a real jump in cost
  over "re-find the rect," and the reason B stays firmly parked.
- **C — One track per device shape.** Honest, no cleverness, but N× authoring and it kills "author
  once." The thing we're trying *not* to do.

**Sequencing rule:** ship nothing here until Rung 0 leaves a real gap. When it does, reach for the
*narrowest* hatch that closes it (A for a scale-only cart, B only when a shot must show a finger on a
control that reflows). Resist building B speculatively; resist C entirely.

## Two use-cases, deliberately not unified

The tension looks like one problem but it's two, with different urgency:

- **Store videos / clips (near-term, Tinyjam):** Rung 0 is aimed squarely here. Self-playing +
  keyboard-transport gets truthful full-bleed App Preview videos at every device size from one track.
- **Attract mode on reflowing racks (later):** the handoff replaying a *touch* demo on a phone is the
  case that genuinely wants B. But attract-mode's own scope is "prototype native in `sloop` first"
  (a seeded, keyboard/position-simple cart) — so this doesn't force B early either.

Keeping them separate is what lets Rung 0 be enough for the thing we need now.

## Why hatch B eventually earns its keep — the in-app storefront (2026-07-05)

Three separate wants converge on hatch B, which is what turns it from "nice someday" into a real
target once the near-term work lands:

1. **Showing a finger interacting in a demo** ("look how it *feels* to play") — wanted, not now.
2. **Attract mode on the buyer's device** — [`attract-mode.md`](attract-mode.md)'s update (same date)
   reframes attract as the **in-app IAP storefront**: a *locked* rack idles into its `de:demo` loop,
   first touch offers "unlock to play." That demo runs on the buyer's device, so it **reflows** — and
   a finger-on-a-control demo sells better than a hands-off one.
3. So the finger, attract-on-device, and IAP conversion are the *same* need — a touch performance
   that survives reflow — which gives hatch B a **revenue** reason, not just a debug one.

**Rung 0 still ships all three today** (keyboard-transport self-play: reflowed picture, identical
audio, one track). B is the upgrade that makes the demo show hands — reach for it when the storefront
demo measurably converts better with a visible finger, not before.

## The live-looping cousin — a sidestep, not this problem

A separate idea — **record my own inputs and loop them back live** (the
[`input-recording-looper.md`](input-recording-looper.md) direction) — brushes this same coordinate
question. But it's a *creation* tool, not a *distribution* artifact: a loop is captured and played
on the device it was made on. So the pragmatic answer there is just **run separate sessions per
device** rather than port one track across shapes — per-device capture costs nothing there, because
"author once, ship everywhere" isn't the goal the way it is for a store demo.

## Related

- [`../guides/debug-harness.md`](../guides/debug-harness.md) — the replay/script/record engine + the
  track format (`move`/`click` = absolute canvas pixels; keys = position-free) this doc is about.
- [`device-adaptive-layout.md`](device-adaptive-layout.md) — why the racks reflow; §"store-asset
  payoff" is the video goal this serves.
- [`attract-mode.md`](attract-mode.md) — the third replay consumer; the case that eventually wants
  hatch B (a touch demo replayed on a reflowing phone).
- [`input-recording-looper.md`](input-recording-looper.md) / [`headless-autoplay.md`](headless-autoplay.md)
  — the other two corners of the record/replay story.
</content>
</invoke>
