# 0033 — Video distribution: YouTube first, via an in-house push tool
Date: 2026-07-20 · Status: accepted

## Context
[`demand-generation.md`](../design/demand-generation.md) ranks **a shareable video** as
lever #2 — the growth engine for a music toy ("a loop of a beat coming together travels on
Reels/TikTok/Shorts"). But the pipeline stops one step short: `make-gif.js` mints the clip
(real cart audio, integer nearest-neighbour upscale, per-ratio variants including 9:16), and
then a human hand-uploads it. The clip is a build artifact; its *distribution* is manual.

The question raised: should dreamengine be able to **push a clip to a video platform** from
the repo — and if so, which platform, and built how? Three algorithmic short-video venues
matter for this tribe (YouTube Shorts, TikTok, Instagram Reels). Only one of them is
automatable.

## Decision
**Add a push endpoint to lever #2, and make YouTube the first (and, for now, only) target.**
A hand-rolled repo tool — `tools/youtube-push.js` — uploads a baked clip (or an app reel) to
YouTube via the **YouTube Data API v3**, same in-house shape as
[`asc-push.js`](../design/store-agents.md) / [ADR-0026](0026-store-pipeline-in-house-not-fastlane.md):
node, zero heavy deps, creds outside git, `--dry-run` / `--check`, metadata derived from cart
data.

Two sub-decisions fall out:

- **A "Short" is not a separate integration** — it's just a **≤60s, 9:16** upload with
  `#Shorts` in the title/description. `youtube-push` reuses the existing `--ratio 9:16` cut and
  **defaults to the vertical short** (the format that actually travels for a music toy), with a
  landscape flag for the full trailer.
- **TikTok and Instagram Reels stay MANUAL.** Their posting APIs are approval-gated (TikTok
  Content Posting API) / business-account-gated (Instagram Graph API) — hostile to a solo
  dev's automation. Revisit only if that changes.

## Why
- **YouTube has the only usable official upload API of the three.** This is the decisive
  point: "push to YouTube" is the pragmatic wedge into lever #2's last mile; the others can't
  be automated cheaply today, so hand-upload remains their path.
- **In-house is the standing ethos** — same reasoning as ADR-0026 (App Store Connect API, not
  Fastlane) and share-panel rule 2 (every action is a CLI tool first; the button streams its
  log). A hosted uploader (Zapier/Buffer/Make) is off-ethos, recurring cost, and opaque.
- **The metadata is a derivable artifact of the cart**, exactly like the store copy in
  ADR-0026: title/description/tags draft from the cart's `de:meta` + the app's `listing` block,
  and the `aso-*` tools already produce good tag vocabulary. No fifth hand-maintained copy of
  the truth.
- **Own generated audio = no Content ID / copyright exposure.** Unlike reposting someone
  else's track, every clip's soundtrack is the cart's own synthesis — safe to upload at scale.
- **The one structural difference from asc-push is auth, and it's a one-time cost.** YouTube
  needs OAuth2 user consent (not a JWT/`.p8`): a first-run browser sign-in, then a refresh
  token cached in `~/.youtube/` (never in git). After that, uploads are non-interactive —
  scriptable, cron-able.

## Consequences / costs
- **One-time setup:** a Google Cloud project with the Data API enabled + an OAuth consent
  screen. Uploading to your *own* channel works under test-user consent without full app
  verification.
- **Quota:** the default free quota is ~10,000 units/day and an upload costs ~1,600 units →
  ~6 uploads/day. Ample for a solo shelf; documented so it isn't a surprise.
- **Scope discipline:** resist mission-creep into a "post everywhere" tool. YouTube only;
  others manual until their APIs are viable. The tool distributes *one clip well*, it is not a
  social-media suite.

## Alternatives considered
- **Fastlane / third-party CLIs** — rejected for the same reasons as ADR-0026 (Ruby DSL
  runtime, opaque, and it can't treat the clip+copy as cart-derived artifacts).
- **A hosted scheduler (Buffer/Zapier/Make)** — recurring cost, off-ethos, and it still needs
  the file produced here first.
- **TikTok/Reels first** — blocked by gated APIs; not automatable for a solo dev now.
- **Stay fully manual** — fine *today*, but lever #2 is the growth engine and the artifacts
  (clip + derivable copy) already exist, so closing the last mile is cheap and high-leverage.

## See also
- [`video-distribution.md`](../design/video-distribution.md) — the tool contract, OAuth model,
  metadata derivation, and staging (this ADR is the *decision*; that doc is the *design*).
- [`demand-generation.md`](../design/demand-generation.md) — lever #2, whose push endpoint this
  is.
- [ADR-0026](0026-store-pipeline-in-house-not-fastlane.md) — the sibling in-house-API decision
  (App Store Connect) `youtube-push.js` is modelled on.
