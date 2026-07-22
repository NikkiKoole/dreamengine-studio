# Video distribution — pushing a clip to YouTube (lever #2's last mile)

> 🎬 **The video pipeline** — five docs, one topic (this breadcrumb is in all of them, so any one
> leads to the rest): record & bake a clip → assemble ([`promote-tab.md`](promote-tab.md) ·
> [`trailer-builder.md`](trailer-builder.md)) → frame for the channel
> ([`export-ratios.md`](export-ratios.md)) → distribute ([`video-distribution.md`](video-distribution.md)).
> Strategy: [`demand-generation.md`](demand-generation.md) lever #2.
>
> **This doc is the UPLOAD step only.** Authoring the clip (record, bake, stitch, reframe to 9:16)
> is already shipped in the editor's Promote tab / trailer builder — don't rebuild it here.

STATUS: SHIPPED (2026-07-20) — `tools/youtube-push.js` is built and PROVEN live: the OAuth
sign-in (`--auth` loopback consent) + resumable upload work end to end — the first real push
(the tinyjam reel → a 9:16 Short, unlisted) succeeded, returning a `youtube.com/shorts/…` URL.
Every local path is verified too (`--check`, metadata derivation, `--dry-run`, the
bake/transcode → 9:16 composite, the >60s Short guard, `--landscape`/`--public`). The platform
+ build-shape decision is settled in [ADR-0033](../decisions/0033-youtube-first-video-distribution.md).

## The gap this closes

[`demand-generation.md`](demand-generation.md) lever #2 (**a shareable video**) is the growth
engine for a music toy. The pipeline already produces the clip:

```
tools/clips/<cart>/NN-take.rec   →   make-gif.js   →   a webm/mp4 with real audio,
                                                        crisp integer upscale, 9:16 variant
```

…and then a human drags the file into YouTube by hand. `youtube-push.js` closes that last
mile: **recipe → clip → uploaded, with a URL back**, from one command.

This is *distribution*, distinct from *authoring* — [`trailer-builder.md`](trailer-builder.md)
composes a reel; this doc ships one out.

## The tool — `tools/youtube-push.js`

Modelled on [`asc-push.js`](store-agents.md) (ADR-0026): node, no heavy deps, creds outside
git (`~/.youtube/`), `--dry-run` prints the exact upload plan without touching the network,
`--check` is an offline gate. (Unlike asc-push there's no live-state *diff* — a new upload has
nothing to compare against — so `--dry-run` shows the plan rather than a diff.)

```bash
# the common path — bake (if needed) the cart's clip and upload it as a Short
node tools/youtube-push.js acidcandy --recipe 01-take

node tools/youtube-push.js acidcandy --recipe 01-take --landscape   # full 16:10, not a Short
node tools/youtube-push.js --reel tinyacidjam                       # push an app trailer instead
node tools/youtube-push.js acidcandy --recipe 01-take --dry-run     # show what WOULD upload
node tools/youtube-push.js --check                                  # offline: creds + manifest sanity
```

- **Input** — a committed recipe (baked via `make-gif.js` if the clip isn't already in
  `editor/public/clips/<cart>/`), or a baked app reel (`editor/public/reels/<app>.webm`).
- **Default output shape = a Short.** Per ADR-0033 a Short is just a ≤60s, 9:16 upload with
  `#Shorts` in title+description. Two ways to fill the vertical frame, best first:
  1. **Full-bleed (preferred).** If a 9:16 clip variant already exists —
     `editor/public/clips/<cart>/<label>--720x1280.webm`, baked by the editor's **Promote tab
     "bake at 9:16"** picker where a *resizable* cart **reflows to fill** the frame
     ([`export-ratios.md`](export-ratios.md) Stage 2) — the tool uploads **that**. No bars.
     Framing the cart for vertical is the **authoring surface's** job; this tool just consumes its
     output.
  2. **Letterbox fallback.** No variant? The tool integer-upscales the native clip and composites
     it centred on a 1080×1920 canvas (never stretches, same rule as `store-shots.js`) over a
     solid bg (`config.shortBg`), leaving crisp pixels + bars. It **says so** and points you at the
     Promote picker for a full-bleed bake.

  A take longer than 60s is **refused** (trim first, or `--landscape`) rather than uploaded as a
  non-Short. `--landscape` opts into the full native 16:10 clip (no 60s limit).
- **Uploads mp4 (h264/AAC)** — the format YouTube ingests cleanest, at a low CRF source so
  YouTube's re-encode has good material (same reasoning as the Reddit bake). `make-gif.js`'s mp4
  path already emits libx264/yuv420p/AAC/`+faststart`, so a cart clip is upload-ready as baked;
  a `.webm` app reel is transcoded to mp4 first.

> **Authoring note — the target is 9:16, and full-bleed beats letterbox.** For YouTube reach the
> shape that travels is the vertical **9:16 Short**, not the engine's native 16:10. Full-bleed
> (fills the whole phone screen) beats letterbox bars — and getting there is **already shipped**,
> just upstream of this tool: bake a `--720x1280` variant in the editor's **Promote tab** (a
> `resizable` cart reflows to fill; [`export-ratios.md`](export-ratios.md)). This tool prefers that
> variant automatically and only letterboxes when none exists. So the workflow is: **author a
> vertical-friendly cart → bake its 9:16 variant in Promote → `youtube-push` uploads it full-bleed.**
> (Designing a genuinely vertical *face* for a cart — not just a reflow — is the per-cart polish
> that makes a Short shine; that part is authoring, not this tool.)

## Auth — OAuth2, the one difference from asc-push

asc-push signs a JWT from a `.p8`; YouTube needs **OAuth2 user consent**. One-time cost:

1. A Google Cloud project with the **YouTube Data API v3** enabled + an OAuth consent screen
   (uploading to your *own* channel works under test-user consent — no full app verification).
2. First run opens a browser to sign in; the **refresh token is cached in `~/.youtube/`**
   (client secret + token), **never in git** — same convention as `~/.appstoreconnect/`.
3. Every subsequent run is **non-interactive** — scriptable, cron-able (see the drip below).

Quota is documented in the header: ~10,000 units/day default, ~1,600 per upload → ~6/day.

## Metadata — derived, not hand-typed

Like the store copy in ADR-0026, the video's title/description/tags are **artifacts of the
cart**, not a new copy of the truth:

| field | source |
|-------|--------|
| title | app `listing.title` / cart `de:meta.title` (+ ` #Shorts` when vertical) |
| description | cart `de:meta.description.summary` + a store/gallery link |
| tags | app `listing.keywords` + the `aso-*` tag vocabulary already generated |

The agent owns the *taste* (final title punch-up); the tool owns the deterministic packing —
same script/agent split as the ASO composer.

Own generated audio → **no Content ID / copyright risk**, so uploads are safe at scale.

## Staging

- **v0.1 — single clip upload. BUILT (2026-07-20).** Bake-if-needed → resumable upload → return
  the URL. Landscape and 9:16, `--dry-run`, `--check`, creds in `~/.youtube/`. The whole point
  in the smallest form.
- **v0.2 — Shorts as the default** + `--reel <app>` to push a composed app trailer. The >60s
  guard + `#Shorts` handling. **SHIPPED alongside v0.1 (2026-07-20)** — it fell out of the same
  code. The live path is proven: the OAuth client is set up (creds in `~/.youtube/`) and the
  first real upload (tinyjam reel → an unlisted Short) returned a live URL.
- **v0.3 — scheduled drip.** NOT built. A launchd/cron runner (twin of
  [`reddit-gaps-drip.sh`](demand-discovery.md)) that pushes the newest committed clip on a
  cadence — a steady lever-#2 heartbeat without hand-work.

## Open questions

- **Channel model** — one channel for the whole shelf, or per-app? v0.1 uploads to whatever
  channel the cached OAuth token authorizes (one account = one signed-in channel); per-app would
  need separate tokens or a brand-account selector. Start with one.
- **Privacy default** — upload `unlisted` (review before going public) or straight to `public`?
  Lean `unlisted` for v0.1 so a bad take never auto-publishes; `--public` to commit.
- **Thumbnail** — YouTube auto-picks a frame; `store-shots.js` / `store-contact.js` already
  pick hero frames, so a later `--thumb` could set a crisp custom one.

## The other venues — TikTok (revisited 2026-07-22, still manual)

ADR-0033's "TikTok stays manual" was re-checked against TikTok's
[Content Posting API](https://developers.tiktok.com/doc/content-posting-api-get-started-upload-content)
— the conclusion HOLDS, but the reason is now sharper than "approval-gated":

- **The self-serve tier is drafts-only.** After registering a developer app AND passing a human
  scope review (`video.upload`, days-to-weeks), the API can only park a video in your TikTok
  **inbox as a draft** — you still open the phone app to caption + publish. Direct publish is a
  second, heavier audit (and unaudited direct-post is restricted to private-only visibility).
- **The manual path is one drag-and-drop.** tiktok.com/upload (TikTok Studio, in a browser)
  takes the baked mp4 straight from `editor/public/clips/` / `build/` — caption, schedule,
  publish, no phone. At this repo's volume (a few clips a week) the API tier would save
  *only the drag*, at the cost of the registration + review + a new `tiktok-push.js`.
- **Revisit trigger:** wanting a fully hands-off drip (v0.3-style cron) to TikTok — and even
  then the drafts-only tier doesn't deliver it; it'd need the direct-post audit.

So: the pipeline ends at the baked mp4; TikTok's upload page is the last step, by hand, on purpose.

## See also
- [ADR-0033](../decisions/0033-youtube-first-video-distribution.md) — the platform + in-house
  decision this implements.
- [`demand-generation.md`](demand-generation.md) — lever #2 (shareable video), the strategy
  above this tool.
- [`trailer-builder.md`](trailer-builder.md) — the *authoring* surface (compose a reel);
  distribution picks up where it leaves off.
- [`store-agents.md`](store-agents.md) §"ASC upload tool" — `asc-push.js`, the sibling
  in-house-API tool `youtube-push.js` mirrors.
