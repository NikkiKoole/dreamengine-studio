# Export ratios — getting a cart's motion into the frame shape a channel wants

> **STATUS: building** (2026-07-08) — **both stages SHIPPED for resizable carts.** A thread opened
> while building the trailer/reels work ([`trailer-builder.md`](trailer-builder.md),
> [`promote-tab.md`](promote-tab.md)); the approach-(b) path is now wired end-to-end.
>
> **What shipped (2026-07-08).** `make-gif --screen WxH` already renders a resizable cart at any
> canvas (approach b — the "gap" this doc first guessed at was overstated). On top of that:
> **Stage 1** — an output-ratio picker on the trailer builder's Build (`# size` in the `.reel` →
> `compose-clips` renders at that canvas; mismatched clips letterbox). **Stage 2** — per-ratio clip
> **variants** (`editor/public/clips/<cart>/<label>--<W>x<H>.webm`, baked via the Promote "bake at"
> picker) that `compose-clips` prefers at a matching `# size`, so a reel **fills** instead of barring.
> Proven on `onetake` (a keyboard-driven demo): one take → filled reels at 16:9/9:16/1:1 and a filled
> 444×960 iPhone reel. **Gotcha banked:** canvases must be **even** (odd width 443 breaks ffmpeg's
> `pad`); App Store presets are **half sizes** (444×960/960×444/600×800) doubled on delivery — chunky
> pixels double cleanly, and it dodges composing millions of pixels. **Still open:** the **composite
> (a) path for FIXED-layout carts** (letterbox/bg for a non-resizable cart), and delivery-exact upscale.

## The reframe: this is a *ratio/resolution* axis, not "App Store work"

A cart renders at its **native resolution** (say 320×200 — a specific, usually landscape-ish
aspect). But every place you'd *post* the motion wants its own frame shape:

| channel | ratio | note |
|---|---|---|
| **TikTok / Reels / Shorts** | **9:16** vertical (1080×1920) | the common one — a single cart's take, no App Store involved |
| **YouTube / landscape** | 16:9 | |
| **square social** | 1:1 | |
| **App Store app preview** | device portrait (~9:19.5 iPhone, 3:4 iPad) | Apple-spec resolutions + 15–30s + fps/codec rules |

So the axis is **"render this cart's motion at target ratio/resolution R"** — and App Store previews
are *one consumer*, TikTok a *different* one. A take from a single cart for TikTok is the everyday
case; the App Store is the fussy, multi-size case. Same underlying capability.

This is **orthogonal to the scenario** (the `.reel`, [`trailer-builder.md`](trailer-builder.md)): one
scenario can render to many ratios; a single take can render to one. Ratio is a *render target*, not
an arrangement.

## Front and centre: keyboard shortcuts are what make "one take, many ratios" work

> **The enabler, stated plainly: a cart driven by keyboard shortcuts is position-free, so ONE recorded
> take replays truthfully at ANY ratio — and the cart can demo itself.**

Everything hard about multi-ratio video traces back to *how the input was captured*:

- **Keyboard / `btn()` shortcuts have no coordinates.** `down 90 SPACE` means the same thing on iPad,
  iPhone-portrait, and a 9:16 crop — the layout can reflow completely and the take still lands. Bonus:
  the audio is **byte-identical at every viewport** (the beat advances from `dt`, not pixels). So a
  keyboard-driven take = one performance → every ratio, truthfully.
- **A tap is a pixel.** `click 204 135` points at wherever a widget *was* at the recording ratio; reflow
  moves it (or hides it behind a panel) and the tap misses.

So the enabler is really an **authoring property**: *is this cart keyboard-drivable enough to demo
itself?* Racks/toys that bind transport + pattern/preset cycling to keys get multi-ratio store & social
video **for free**; a mouse-only cart doesn't, and needs a re-record per shape or the composite fallback.
Designing a cart with keyboard shortcuts up front is the cheapest thing you can do to make it *filmable*.
(The full input-portability reasoning: [`resolution-portable-input.md`](resolution-portable-input.md);
its Rung-0 bet — "make racks demo themselves from the keyboard" — is exactly this move.)

## The device sizes already live in one place

`tools/store-shots.js` holds the device pixel dims (its `DEVICES` map — `iphone69 1290×2796`,
`iphone65 1242×2688`, `ipad13 2048×2732`, `ipad11 1668×2388`). **That is the single source of truth**
— any video sizing should read the same presets, never re-hardcode them (they're Apple's still-shot
sizes; the app-*preview* video spec differs and drifts, so verify video dims/fps/duration against
App Store Connect when we build that path). The stable thing is the **ratios**, not the pixels.

## Two approaches (both viable; they compose)

**(a) Composite** — render the cart at its native res, then place it, crisp integer-upscaled, onto a
target-sized canvas: letterbox/pillarbox bars, or a decorative background (+ optional caption). This
is **exactly what `store-shots.js` already does for a still frame** — the video version is "do that
per frame of a clip." Pros: one capture → any ratio; the cart is never stretched; matches the store
screenshots' look. Cons: bars or bg around a landscape cart in a 9:16 frame; needs a video
compositor step.

**(b) Native capture at the target canvas** — run the cart with `SCREEN_W/H` (or `--resize`) set to
the target ratio so the cart *fills* the frame. Pros: no bars, the whole frame is the game. Cons:
only looks right for a **device-adaptive** cart that reflows to the aspect
([`device-adaptive-layout.md`](device-adaptive-layout.md) + `lay.h`); a fixed-layout cart just crops
or stretches. This is the "record a take at 9:16" idea — best for carts built responsive.

Rule of thumb: **(b) for carts that reflow** (the frame is all game — ideal for TikTok), **(a) as the
universal fallback** (any cart, bars/bg — and the only sane path for the App Store's fixed sizes).

## The ideal — and why it's hard: one take, many ratios

The endgoal is **record a performance once, render it at every ratio**. Approach (b) is what makes
that *truthful* (the cart fills each frame, laid out natively) — but it collides with a second
shipped system, and this is **the hardest case in the whole picture**:

> **A device-adaptive UI changes between ratios** — on iPad five sections sit inline; on
> iPhone-portrait they collapse into an accordion; some controls hide behind a panel at one shape and
> are visible at another. So an input *track* recorded at one ratio does **not** truthfully replay at
> another: a tap recorded as an absolute canvas pixel lands on a widget that moved — or on empty space
> where a now-hidden control used to be.

And that's the whole rub — it's the same coordinate problem the front-and-centre section names, just
seen from the render side. It's fully worked through in
**[`resolution-portable-input.md`](resolution-portable-input.md)** — the *input* half of "one take,
many ratios" (this doc is the *output* half).

So the honest synthesis: **one-take-many-ratios is real, but only for position-free (keyboard /
self-playing) takes on device-adaptive carts** — which is why keyboard shortcuts are the headline, not
a detail. For everything else — touch demos, fixed-layout carts, the App Store's exact sizes —
approach (a) composite is the truthful fallback.

## What already exists to build on

- **`store-shots.js`** — the compositing pattern for **stills** (crisp cart on a device-sized bg +
  caption, `--font` swap). The model for approach (a); the video tool should reuse its `DEVICES` +
  `--bg`/`--caption` conventions. ([`store-agents.md`](store-agents.md) §1.)
- **`compose-clips.js`** — **already letterboxes** clips of mixed sizes onto a target `# size WxH`
  (nearest-neighbour, pixels stay crisp). So a reel can *already* target a canvas — the bones of
  approach (a) for video exist here, just as bars (no decorative bg/caption yet).
- **`make-gif.js`** — renders native + integer `--scale`, muxes audio. Has **no** `--ratio`/`--size`
  today; the natural place to add a target-ratio option for single-take export.
- **`--resize` / `SCREEN_W/H`** (studio.c) — the enabler for approach (b): capture at the target
  canvas size.
- The **reels/trailer builder** ([`trailer-builder.md`](trailer-builder.md)) and per-cart **takes**
  ([`cart-clips.md`](cart-clips.md)) — the *scenarios* that would render to a chosen ratio.

## Where it plugs into what's built

- **Promote tab** ([`promote-tab.md`](promote-tab.md)) — a take/clip export with a **ratio picker**
  ("export for TikTok 9:16 / square / YouTube"). The everyday single-cart case.
- **Trailer builder** — a **render-target** control (native / 9:16 / a device) so ▶ Build renders the
  scenario at that shape; App Store previews = the multi-device pass.
- **App trailer / store previews** ([`demand-generation.md`](demand-generation.md) #4,
  [`store-agents.md`](store-agents.md)) — batch-render the app's reel to each required device size,
  the video sibling of `store-shots`.

## The worked user story that shaped this (2026-07-08)

Walking a real goal caught a trap the feature list hid: *"Nikki has a **game cart** (not an app), records
a cool moment, wants a **9:16 portrait** clip for TikTok."* His cart is **fixed-layout** — and the
"bake at 9:16" picker used `--screen 180x320`, which on a fixed cart just **crops the world and strands
the HUD** (a broken frame, verified on `flank`). It only ever worked for *resizable* carts; the common
case was a silent trap. The story also showed there was **no single-clip "export at ratio"** — you had
to route one gameplay clip through the multi-clip reel builder (+ a text-card to clear the ≥2 rule).

**Both fixed (2026-07-08):**
- **`studio:bake-clip` is now cart-type aware.** Resizable → reflow-fill (`--screen`); **fixed-layout →
  render native, then ffmpeg LETTERBOX into the ratio** (black bars, crisp nearest — the standard TikTok
  gameplay format). No more broken crops.
- **Single-clip export.** A baked variant (`<label>--<W>x<H>.webm`) is a standalone pos(t)able clip; the
  Promote clip row shows it as a **clickable chip** that opens it. So *record → 🎬 bake at 9:16 → click
  the chip* — no reel, no ≥2 hack.

## The remaining gap

**Done:** approach (b) reflow for resizable carts (Stages 1 & 2), **and** approach (a) **plain-bars
letterbox** for fixed carts (single clip + reels). **Still open:** the **DRESSED** composite — bars
replaced by a decorative bg + caption (the video sibling of `store-shots.js`) for a more finished store
look — and **delivery-exact upscale** (double the small canvas to the App Store's exact pixels, a manual
CLI step today).

## Open questions

1. **Composite dressing** — bars, a solid bg, or a decorative bg + caption (store-shots style) for
   the vertical margin? Probably per-channel (bare bars for a quick TikTok, dressed for a store).
2. **Where the size table lives** — reuse `store-shots.js`'s `DEVICES` as the shared source (lean
   yes), and add social ratios (9:16 / 1:1 / 16:9) alongside.
3. **Audio** — social verticals want it; store previews have their own rules. `make-gif` already
   muxes audio, so this mostly carries over.
4. **Which approach the trailer builder defaults to** — (a) universal composite, with (b) offered
   when the cart declares itself device-adaptive.

## Related

- [`resolution-portable-input.md`](resolution-portable-input.md) — **the input half** of one-take-many-ratios (does the *track* survive a reflow: keyboard yes, touch no). Read together with this doc — output + input are the two halves.
- [`device-adaptive-layout.md`](device-adaptive-layout.md) — the enabler for approach (b) (carts that reflow to any aspect) *and* the reason a touch take breaks across ratios.
- [`trailer-builder.md`](trailer-builder.md) · [`promote-tab.md`](promote-tab.md) · [`cart-clips.md`](cart-clips.md) — the scenarios/takes that render to a ratio.
- [`input-recording-looper.md`](input-recording-looper.md) — the take/replay format the ratio render consumes.
- [`store-agents.md`](store-agents.md) — `store-shots.js` (the stills solution this mirrors) + the store pipeline.
- [`demand-generation.md`](demand-generation.md) — the marketing/video pipeline (#4 app trailer).
