# Export ratios — getting a cart's motion into the frame shape a channel wants

> **STATUS: exploring** (2026-07-08) — a thread opened while building the trailer/reels work
> ([`trailer-builder.md`](trailer-builder.md), [`promote-tab.md`](promote-tab.md)). Not specced to
> build yet; this note names the problem and the two approaches so we pick deliberately later.

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

## The gap + a proposed first step

The missing capability is a **video composite-to-target-ratio** (the `store-shots` treatment applied
per-frame of a clip, not just one frame). `compose-clips` letterboxing is the closest; it does bars,
not the bg+caption composite.

Smallest useful first step (when we build): teach **`make-gif.js` a `--ratio 9:16` / `--size WxH`**
(letterbox for now, the cheap win for TikTok on any cart), then a **Promote-tab ratio picker** that
calls it. Approach (b) for reflowing carts and the full App Store multi-device video pass are later,
larger rungs.

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

- [`trailer-builder.md`](trailer-builder.md) · [`promote-tab.md`](promote-tab.md) · [`cart-clips.md`](cart-clips.md) — the scenarios/takes that render to a ratio.
- [`store-agents.md`](store-agents.md) — `store-shots.js` (the stills solution this mirrors) + the store pipeline.
- [`device-adaptive-layout.md`](device-adaptive-layout.md) — the enabler for approach (b) (carts that reflow to any aspect).
- [`demand-generation.md`](demand-generation.md) — the marketing/video pipeline (#4 app trailer).
