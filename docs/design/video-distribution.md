# Video distribution — pushing a clip to YouTube (lever #2's last mile)

STATUS: READY (2026-07-20) — specced, buildable, not built. The platform + build-shape
decision is settled in [ADR-0033](../decisions/0033-youtube-first-video-distribution.md);
this doc is the tool design (contract, auth, metadata, staging).

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

Modelled line-for-line on [`asc-push.js`](store-agents.md) (ADR-0026): node, no heavy deps,
creds outside git, `--dry-run` GETs live state + diffs, `--check` is an offline gate.

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
  `#Shorts` in the description — so the tool bakes/selects the `--ratio 9:16` cut and, if the
  take runs longer than 60s, **refuses with a clear message** (trim first) rather than silently
  uploading a non-Short. `--landscape` opts into the full clip as a normal video.
- **Uploads mp4 (h264/AAC)** — the format YouTube ingests cleanest, at a low CRF source so
  YouTube's re-encode has good material (same reasoning as the Reddit bake).

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

- **v0.1 — single clip upload.** Bake-if-needed → resumable upload → return the URL. Landscape
  and 9:16, `--dry-run`, `--check`, creds in `~/.youtube/`. The whole point in the smallest form.
- **v0.2 — Shorts as the default** + `--reel <app>` to push a composed app trailer. The >60s
  guard + `#Shorts` handling.
- **v0.3 — scheduled drip.** A launchd/cron runner (twin of
  [`reddit-gaps-drip.sh`](demand-discovery.md)) that pushes the newest committed clip on a
  cadence — a steady lever-#2 heartbeat without hand-work.

## Open questions

- **Channel model** — one channel for the whole shelf, or per-app? Start with one; the tool
  takes the target channel from `~/.youtube/config.json`.
- **Privacy default** — upload `unlisted` (review before going public) or straight to `public`?
  Lean `unlisted` for v0.1 so a bad take never auto-publishes; `--public` to commit.
- **Thumbnail** — YouTube auto-picks a frame; `store-shots.js` / `store-contact.js` already
  pick hero frames, so a later `--thumb` could set a crisp custom one.

## See also
- [ADR-0033](../decisions/0033-youtube-first-video-distribution.md) — the platform + in-house
  decision this implements.
- [`demand-generation.md`](demand-generation.md) — lever #2 (shareable video), the strategy
  above this tool.
- [`trailer-builder.md`](trailer-builder.md) — the *authoring* surface (compose a reel);
  distribution picks up where it leaves off.
- [`store-agents.md`](store-agents.md) §"ASC upload tool" — `asc-push.js`, the sibling
  in-house-API tool `youtube-push.js` mirrors.
