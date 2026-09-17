# 030 — The capability nobody believes exists

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-09-16

**Confidence**
High on the fact; Medium on the explanation

---

## Observation

Asked whether an agent could publish video to YouTube automatically, an outside model (Grok)
answered that it could not — that the platform's posting APIs are approval-gated and closed to this
kind of automation.

This repo has been doing it since **2026-07-20**. `tools/youtube-push.js` (432 lines, node, zero
heavy deps) takes a committed recipe, bakes an mp4, composites a crisp 9:16 Short and uploads it,
returning a `youtube.com/shorts/…` URL. `docs/STATUS.md` records it as **PROVEN live** — first real
upload, the tinyjam reel. The OAuth refresh token has been cached in `~/.youtube/` since the day it
was built; `--check` still passes on this machine two months later, with no re-consent.

So the capability is not merely possible. It is built, exercised, and quietly load-bearing — and the
prevailing belief about it, held confidently enough that a model will assert it unprompted, is that
it does not exist.

---

## Why this matters

The interesting thing is not that an outside model was wrong. It is **why the wrong answer is the
well-supported one**, because that shape will recur for every gated-looking platform this project
ever touches.

**The three venues get collapsed into one belief.** TikTok and Instagram genuinely are gated;
YouTube has had a plain upload endpoint for over a decade. ADR-0033 had to reason about all three
separately to find that out. Compressed to a sentence — "social posting APIs are approval-gated" —
the true part about two platforms swallows the false part about the third.

**First-party and third-party automation are different objects, and almost nobody says which one
they mean.** Posting to *your own* channel under your own OAuth consent is not the same regulatory
animal as posting to *a customer's* channel. Nearly all the written material is authored from the
third-party position, because that is who builds products and writes docs. The solo maker uploading
to their own channel is an unlit corner of the same API.

**The failure stories are louder than the successes.** The 2020 quota-audit change produced a wave
of "you can't upload anymore" posts. That is emotionally loaded, heavily indexed text, and it does
not carry the footnote that the pain lands mostly on apps serving many users. A quiet success writes
nothing at all.

*(This section is reasoning about a single observed instance, not measurement — hence Medium
confidence. What is High is that the belief was asserted and that this repo contradicts it.)*

---

## Evidence

**The decision was made by checking, not by inheriting the consensus.** ADR-0033 (accepted
2026-07-20) evaluated all three venues and picked the one that was actually open. Had it reasoned
from the general belief, the tool would never have been attempted.

**The negative half was re-derived, and it sharpened.** The TikTok re-check on 2026-07-22
(`video-distribution.md` §"The other venues") replaced "approval-gated" with something far more
useful: the self-serve tier is **drafts-only** — after a human scope review it can only park a video
in your inbox, and *direct* publish is a second, heavier audit, with unaudited direct-post
restricted to private-only visibility. The conclusion held; the reason changed from folklore to
mechanism. Only the second version tells you what would have to change for the answer to flip.

**The cost that actually binds is quota, not permission.** ~10,000 units/day, ~1,600 per upload →
about six uploads a day. That is the real ceiling for a solo shelf, and it is nowhere near the
ceiling the consensus warns about. The blocker everyone names is not the blocker that bites.

**Two months of unattended credentials is itself the finding.** `~/.youtube/token.json` dates from
2026-07-20 and still drives non-interactive uploads today. The "one-time cost" claim in the ADR's
Consequences section has now been held for eight weeks without maintenance.

---

## Implications

- **This is publishable, and it is the strongest marketing artifact on the shelf.** Lever #2 of
  `demand-generation.md` is a shareable video; but a *claim that contradicts the consensus and then
  shows working code* travels further than another devlog. The expensive part is already written —
  ADR-0033 has the decision and the honest cost list, `video-distribution.md` has the design. Both
  halves are the story: the YouTube build **and** the TikTok non-build. "I checked and deliberately
  did not build it, here is the mechanism" is rarer and more credible than a success alone.
- **The unlit-corner shape is a reusable research move.** When an outside source says a platform is
  closed, ask *closed to whom* — first-party or third-party — before believing it. That one
  distinction is what separated ADR-0033 from the consensus, and it cost a day.
- **Re-derive a "no" on a mechanism, not on an authority.** The TikTok note is durable precisely
  because it names *what* is gated and what would have to change. A "no" without a mechanism cannot
  tell you when to look again, and rots silently.
- **Outside models are a fair weathervane for public belief, and a poor one for capability.** Note
  014 treats outside agents as a knowledge source; this is the inverse use — Grok's answer was worth
  nothing as a fact and quite a lot as a measurement of what the world currently thinks. That is a
  second, legitimate way to read them.

---

## Open questions

- **Has `--public` ever actually produced a public video?** Every upload so far has been unlisted —
  the tool's default (`youtube-push.js:139`), and the proven tinyjam run. `--public` sets
  `privacyStatus: 'public'` on the request (line 319), but the tool never reads the status back, and
  an unaudited API project can have uploads locked to private regardless of what was asked. A
  throwaway push plus a look in YouTube Studio settles it; until then the hands-off story is proven
  only as far as *unlisted*.
- Should the tool report the *actual* privacy state after upload rather than the requested one? A
  silent downgrade is exactly the failure a `--dry-run`-and-`--check` tool is otherwise built to
  prevent.
- If the write-up lands and the belief is common, does that make the post the demand-generation
  artifact rather than the clips it was built to distribute? The tool would then be marketing twice
  over, which no lever in `demand-generation.md` currently anticipates.

---

## Update — 2026-09-17: `--public` is answered, and it works

The open question above ("has `--public` ever actually produced a public video?") is closed:
**yes**. `chordwise/02-take` pushed with `--public` came back
[`youtube.com/shorts/SYcsvgKTyss`](https://youtube.com/shorts/SYcsvgKTyss) reporting
`privacyStatus=public, uploadStatus=processed`, confirmed by a second read through an independent
script. **There is no private lock on this API project.** The consensus was wrong at the second
level too — not just "you can't automate YouTube", but also "even if you can, an unaudited project
can't publish publicly". Neither held.

Three things the test surfaced that the note above could not have known:

- **The read-back shipped and immediately earned its place — by confirming, not catching.** The
  second open question ("should the tool report the actual privacy state?") is now built:
  `verifyStatus()` reads `videos.list part=status` after the upload and reports what YouTube *did*.
  Its first real run printed `✓ uploaded (public, confirmed by read-back)`. Had it been absent, the
  same upload would have printed the same claim **on no evidence** — the tool could not previously
  tell a honoured request from a silently downgraded one.
- **It cost a scope and a re-consent.** Reading a video back needs more than `youtube.upload`, so
  `OAUTH_SCOPE` grew `youtube.readonly` and the July token had to be re-granted. Worth recording as
  a cost: verification is not free, and a token cached before the scope grew degrades to
  `UNVERIFIED` rather than failing an upload that already succeeded.
- **The ledger undercounts the channel.** `STATUS.md` records "first real upload: the tinyjam reel"
  — the API shows **two** uploads on 2026-07-20 (`Tiny Acid Jam`, `Tiny Jam: Pocket Music Toys`),
  both unlisted. A silent-ledger instance of exactly the shape note 029 describes, found by asking
  an outside authority rather than by any check in this repo. The channel's one other public video
  predates the tool by 13 months and was hand-uploaded, which is why it was *not* evidence either
  way before the test.

What this does **not** settle: quota (~6 uploads/day) is untouched by the result, and the audit is
still what raises it. The lock everyone warns about turned out not to apply here; the ceiling that
does bind was never the one in dispute.

---

## Related notes

- 011-tool-discovery
- 014-outside-agent-brainstorms-as-a-knowledge-source
- 016-knowledge-drift
- 025-demand-discovery-supply-side-showcase
