# Demand generation — how downloads actually happen (and why keywords are the tail)

STATUS: EXPLORING / strategy note (2026-07-03). The honest hierarchy of what drives
downloads + paid conversions for these apps, and where each thing we've built or planned
sits in it. A lens *above* the tactical docs — it doesn't replace them, it orders them:
[`sharing-channels.md`](sharing-channels.md) (the channel map), [`store-agents.md`](store-agents.md)
(the store/ASO layer), and [`../marketing/tinyjam/tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md)
(the full go-to-market).

## The reframe: capture vs. generation

Two different jobs, constantly confused:

- **Demand capture** — get found by people *already looking*. ASO/keywords, App Store
  search, "music sequencer app" queries. A **small, fixed pond** for a niche lo-fi toy.
- **Demand generation** — make people *want it who weren't looking*. Video, community,
  word-of-mouth, a shareable moment. This is where the volume is — and it happens **off**
  the App Store.

Everything in [`store-agents.md`](store-agents.md) (research/suggest/brief/score/lint) is
**capture**. It's cheap, it's done, it's worth having — but it is the *tail*, not the wave.

And you don't grab "the world and all the people in it." Per
[`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §1: **you grab a *tribe*,
one per module** — 303 heads for the acid rack, e-piano players for the e-piano. "Music app
users" isn't a market; "people who'd tattoo a TB-303 on their arm" is.

## The lever hierarchy (most → least leverage)

| # | lever | why it matters | where it lives |
|---|-------|----------------|----------------|
| 1 | **A clippable, delightful product** | a satisfying 8-sec groove is its own ad; nothing below saves a dull app | the [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) "delightful to a stranger" bar — the whole engine ethos |
| 2 | **A shareable video** | the growth engine for a music toy — a loop of a beat coming together travels on Reels/TikTok/Shorts | `make-gif.js` today; the planned 9:16 Game-&-Watch dancer clip (tinyjam-marketing §4.1 "video = the amplifier") |
| 3 | **Showing up in the tribe, gift-first** | the tribe trusts a maker who gives before asking (r/edmproduction, KVR, lines, the PO scene; BoBeats/Andrew Huang/Red Means Recording) | tinyjam-marketing §9 (gift-not-ask) + §3.9 channel directory |
| 4 | **A free playable web demo as funnel** | try-in-browser → buy-on-App-Store; the Minecraft arc | Channel A ([`sharing-channels.md`](sharing-channels.md)) + tinyjam-marketing §9.2, §7.2 per-module pages |
| 5 | **Store-page conversion** (screenshots, preview video, icon, ratings) | moves the needle *more than keywords* once someone lands | the hero-frame director, [`store-agents.md`](store-agents.md) §1; `store-shots.js` |
| 6 | **Keywords / ASO** | captures the handful already searching | the whole ASO toolkit — [`store-agents.md`](store-agents.md) §ASO |

The ordering is the point: we spent a week polishing #6 (cheap, worth it, done), but the
downloads come from #1–#4, which are mostly **execution** (make clips, show up) plus a little
tooling the repo has scoped but not built.

## The honest ceiling

No tool "grabs the world" for a solo dev with a niche music toy. But the
**tribe → video → free-demo → paid-app** funnel is exactly how niche music apps break out,
and occasionally one clip goes wide. That's the realistic shape of "big" here — not a
keyword, a *moment*.

## The highest-leverage thing left to BUILD

Most of #1–#4 is execution, but one tool would feed the top of the funnel directly:

> **A social-clip pipeline** — turn a committed `tools/clips/<cart>/` recipe into a captioned
> **9:16 social video** (the pixel faceplate / Game-&-Watch dancer), ready to post. It reuses
> `make-gif.js` + `store-shots.js`, and it's the concrete form of lever #2. Scoped in
> tinyjam-marketing §4.1 and the share-panel make-clip button
> ([`share-panel.md`](share-panel.md) v1); not built.

Per-module landing pages (#4, tinyjam-marketing §7.2) are the other unbuilt piece — the
funnel *destination*, not the 400-cart gallery.

### App-trailer pipeline — v1 backbone SHIPPED (2026-07-03)

The video unit for a multi-cart IAP app is a **showreel** of its racks, and the "many carts"
part was already solved by `compose-clips.js`. `tools/build-app-reel.js <app>` closes the last
gap: reads the manifest's `carts[]`, bakes a clip per rack (skips racks with no committed clip,
warns), generates a committed+editable `tools/reels/<app>.reel`, and composes →
`editor/public/reels/<app>.webm`. Proven on Tiny Jam (a ~19.5s reel: yachtrack ⤍fade⤍ epiano;
acidrack skipped — no clip yet). The rest is the "make it great" layer, staged:
- **the editor picker** — reorderable clip rows + per-cut transition/seconds (a thin editor over
  the `.reel`, not a keyframe timeline); the "pick which script runs after each other + decide
  the cut/tween" ask. Next.
- **per-clip speed** — an ffmpeg `setpts` in `compose-clips` + a `.reel` field.
- **9:16 social framing** (rung 1) applied to the finished reel, and the **IAP-tease
  choreography** (free rack first → "unlock 3 more") that turns the montage into a funnel.
- text/tweens: the beat-synced, engine-native vs hand-off-to-CapCut fork (a live open decision).

## See also
- [`sharing-channels.md`](sharing-channels.md) — the channel map (A web · B App Store · C files)
- [`store-agents.md`](store-agents.md) — the store/ASO judgment layer (all of lever #5–#6)
- [`../marketing/tinyjam/tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) — the full tribe-by-tribe go-to-market
- [`ADR-0022`](../decisions/0022-collaboration-is-the-north-star.md) — the delightful-to-a-stranger bar (lever #1)
