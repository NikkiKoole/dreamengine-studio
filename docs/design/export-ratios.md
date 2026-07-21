# Export ratios — getting a cart's motion into the frame shape a channel wants

> 🎬 **The video pipeline** — five docs, one topic (this breadcrumb is in all of them, so any one
> leads to the rest): record & bake a clip → assemble ([`promote-tab.md`](promote-tab.md) ·
> [`trailer-builder.md`](trailer-builder.md)) → frame for the channel
> ([`export-ratios.md`](export-ratios.md)) → distribute ([`video-distribution.md`](video-distribution.md)).
> Strategy: [`demand-generation.md`](demand-generation.md) lever #2.

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
> Proven on `onetake` (keyboard-driven demo) + `flank` (a fixed 320×200 game → a crisp 720×1280 9:16).
> **Presets live in ONE place — `editor/src/ratios.js`** (2026-07-21): the bake picker, the trailer
> output picker, and the dress-modal ratio picker all build from `RATIO_PRESETS` (they used to be three
> hand-copied lists that drifted). Social presets are standard 1080-class even frames — **16:9 1920×1080 ·
> 9:16 1080×1920 · 1:1 1080×1080 · 4:5 1080×1350** — so 9:16 is a single 1080×1920 everywhere (matching
> `dress-clip.js` + `youtube-push`). App Store: iPhone 6.9 886×1920 / 1920×886 · iPad 1200×1600 / 1600×1200
> (these are app-PREVIEW-VIDEO aspect sizes — deliberately smaller than the SCREENSHOT pixels in
> `store-shots.js`'s `DEVICES`; the two specs differ, verify vs ASC). Output at **`# scale 1`** (exactly
> that size). **Two gotchas
> banked:** (1) canvases must be **even** (odd widths break ffmpeg's `pad`); (2) never let a small
> frame **downscale** the source — that was the blur (a 320-wide game squeezed into a 180-wide 9:16).
> The fixed-cart letterbox now **integer nearest-upscales** the native to FIT the frame (320×200 → ×2 =
> 640×400 centred with bars), never down. **Still open:** the **dressed** composite (bg+caption instead
> of plain bars) and delivery-exact upscale (a cart whose art is bigger than an integer fit of the frame).

## Dressed composite — SHIPPED as `tools/dress-clip.js` (2026-07-20)

**Built (CLI):** `node tools/dress-clip.js <clip> --title … --sub … --hook … --cta … --footer …`
takes a baked clip and composites it into a 9:16 Short with your **hand-typed** text in the bars
(title card + accent rule above, hook/CTA below, framed console in the middle), `--bg`/`--accent`/
`--ink` colours, `--mp4` or webm out. Verified on the real acidcandy clip. Completes the user story
**record → bake → dress with text → export**. **Now fully driveable from the editor with the REAL
engine font + kinetic text** (2026-07-21):

- **Engine bitmap font + boil + tween-in** (the old "needs a pixel TTF" follow-up — SOLVED without a
  TTF). The on-screen text is no longer ffmpeg `drawtext` with a smooth TTF; it's drawn by the
  **`titlecard` cart** (the trailer builder's renderer) in the real `dos_8x8` pixel font with a drop
  shadow, per-letter **boil**, a **breathe** pulse, and a **tween-in** entrance (fade / slide from an
  edge). dress-clip bakes two titlecards (title/sub up top, hook/cta/footer at the bottom) onto the
  magic-green key, then `colorkey`+`overlay`s them into the bars over the console — the SAME
  pixel-perfect path `compose-clips.js` uses for reel overlays. Zero engine/cart changes.
- **Editor UI** — a **✨ dress** button on each baked clip in the Promote tab opens a modal with:
  an instant `drawtext` **layout preview** (`dress-clip.js --preview <png>`, one frame, smooth-face
  approximation of placement, re-rendered debounced as you type); free-text fields; bg/frame colour
  pickers; **boil/breathe sliders** + an **entrance** dropdown; a **▶ preview motion** button that
  bakes a short real engine clip (`--secs 3`) so you see the actual pixel font + boil before
  committing; and **✨ Dress** → `<label>-dressed.webm` into the clip folder. (Two preview tiers
  because engine text can't render per-keystroke — each is a cart compile+bake.)

The CLI stays the single source of truth for the composite + kinetic look — the editor just drives
it. Long takes are capped to a 60s Short. Open follow-up: `youtube-push --dress`.

**Unified with the trailer builder (2026-07-21).** The same dressed look is now a **frame style** in
the trailer builder ([`trailer-builder.md`](trailer-builder.md) §"Frame styles"): `# frame letterbox`
renders every reel clip console-centred with a device frame + bars, and the trailer builder's existing
**multiple timed text overlays** (drag each one's time window on the overlay lane — same-time or
staggered; `pos top/center/bottom` = top-bar / over-console / bottom-bar under the letterbox frame)
give the multi-overlay, staggered version. So: the **`dress-clip.js` modal** is the quick single-clip
dresser (title/sub/hook/cta/footer, always-on); a **1-clip letterbox reel** is the same look with an
arbitrary number of timed overlays. The direction it grew from:

Turn the letterbox bars from dead space into a **framed treatment** — a title card above the cart,
a hook/CTA below (the [`store-shots.js`](store-agents.md) "console as a framed
artifact" look, but for video). Two mockups (`tinyacidjam`, 9:16) compared a bare **subtitle** vs a
**grown title-card**; the grown version clearly wins — the bars earn their space. Decisions taken:

- **Text is ARBITRARY / hand-typed**, NOT derived from `de:meta`. The maker types whatever they want
  per overlay region (title / subtitle / hook / CTA). (Still on-ethos — a human writes the words, no
  auto-generated prose.) So the core primitive is **dedicated overlay regions that take free text**.
- **Engine bitmap font**, not a smooth display face (Bungee etc.) — `FONT_BOOT`/`dos_8x8` so the
  frame reads unmistakably dreamengine (the lo-fi-type edge from [`trailer-builder.md`](trailer-builder.md)
  §kinetic-text). NB: ffmpeg `drawtext` needs a TTF/OTF, so this wants either a TTF of the ROM font or
  compositing the text through the engine itself — a real choice to make at build time.
- **Static first, kinetic later.** A static dressed frame (drawtext on the letterbox path, shareable
  by [`video-distribution.md`](video-distribution.md)'s `youtube-push` + the bake) is v1; text that
  animates / pulses on the beat is the next rung (the `trailer-builder.md` §kinetic-text idea).

Only applies to the **letterbox** path (there ARE bars to dress); the full-bleed reflow has none.

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

## The sizes live in one place per medium (two specs, don't conflate them)

There are **two** size vocabularies, because Apple's app-preview **video** sizes differ from its
**screenshot** sizes (and the video spec drifts — verify vs App Store Connect):

- **Screenshots (stills):** `tools/store-shots.js`'s `DEVICES` map — the real device pixels
  (`iphone69 1290×2796`, `iphone65 1242×2688`, `ipad13 2048×2732`, `ipad11 1668×2388`). The single
  source of truth for STILL screenshots; `store-shots.js` reads it, nothing else should re-hardcode it.
- **Video exports (clips / reels / dressed Shorts):** `editor/src/ratios.js`'s `RATIO_PRESETS` — the
  single source for the editor's ratio pickers (social 1080-class + app-preview-video aspect sizes).
  The bake picker, trailer output picker, and dress-modal picker all build from it.

The stable thing is the **ratios**, not the exact pixels. (Earlier this doc named `DEVICES` the source
of truth for "any video sizing" — that was conflating the two; video sizing now has its own module.)

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

**Done:** approach (b) reflow for resizable carts, **and** approach (a) **crisp plain-bars letterbox**
for fixed carts (single clip + reels) — full-size, integer nearest-upscaled. **Still open:** the
**DRESSED** composite — bars replaced by a decorative bg + caption (the video sibling of
`store-shots.js`) for a more finished store look; and the rare case where a cart's native is *larger*
than an integer fit of the frame (would need a fractional/down-scale — currently clamps to ×1).

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
