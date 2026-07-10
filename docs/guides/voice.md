# voice — how the editorial prose should sound

A synthesis of Nikki's writing voice, for any **hand-written editorial prose** in the
project — first customer is the editorial seam of the generated history doc ([`history-page.md`](history-page.md))
(`docs/design/` history spine), but it applies anywhere a human paragraph gets written
into the docs rather than auto-derived.

Sourced from the prose at **mipolai.com** — weighted toward the **about/home page**,
which is the most authentically Nikki. The *interview* page is AI-smoothed (Nikki's call),
so its tidy em-dash polish is discounted here — it's a weaker source than the raw, looser
about-page lines. Not the poems, per Nikki. This is a distillation of the *mechanics and
stance* of that voice, with the marketing-register profanity dialed down to plain
bluntness, because the history doc is technical narrative, not a manifesto.

The authentic fingerprint (from the about page) is a little **loose and run-on** —
comma-spliced, grammar bent slightly on purpose for momentum ("Truly good not shit,
handcrafted, jazzy interactive playthings that is not bad for children is pretty rare").
That looseness is a feature, not an error to correct. The clean, evenly-spaced em-dash
rhythm is the giveaway of the smoothed version — lean away from it.

> **It's a seam, not a straitjacket.** The history generator works structure-first; this
> file is the reference for the *optional* editorial paragraphs (era intros, subsystem
> blurbs). When in doubt, write less — a derived fact threaded from STATUS.md beats a
> hand-written paragraph that can rot.

## The stance (what the voice is *for*)

- **Respect the reader. Don't dumb it down.** The mission line is "treat a child as if
  it's an adult that just happens to be younger — don't treat them like idiots." Same for
  the reader of the history: assume they're smart, give them the real thing, skip the
  hand-holding.
- **Honest over polished.** Name what failed, not just what shipped. The STATUS ledger
  already does this (the "wah detour was a mistake," the Rhodes/bowed "trust ears over the
  label" scars) — that candor *is* the voice. A history that only lists wins is marketing;
  a history that says "this rotted / this was the wrong call, here's the correction" is
  true to how the work actually went.
- **Made by hand, on purpose.** "Hand-drawn and kind of messy on purpose," "jazzy." The
  prose should feel authored, not generated — a person who cares, not a changelog bot.
- **Earnest, with a dry edge.** Conviction about the work, but allowed to be funny and
  irreverent about it.

## The mechanics (how the sentences move)

- **Short punchy fragments for emphasis, in a row.** "No ads. No gimmicks. No tricks." /
  "It should be fun. Then done. Then go make something for real." Use these to land a
  point — then return to longer sentences.
- **Em-dashes for the tangential thought** — the aside that's really the point — dropped
  mid-sentence rather than parked in a footnote.
- **Parentheticals for the wink or the caveat** (the quiet correction, the "in the good
  way"). Frequent, short, conversational.
- **Concrete and sensory over abstract.** Not "the tester found edge cases" but "he's got
  these super fast little fingers — he'll swipe, tap, do five things at once and then
  boom, the app crashes." Reach for the specific image.
- **Plain words, no corporate jargon.** "good apps," "crap," "weird," "jazzy" — never
  "leverage," "solution," "robust." If a word would appear in a pitch deck, cut it.
- **Mix the rhythm.** Long flowing sentence, then a two-word fragment. Don't let every
  sentence be the same length — that's what makes it read like a person.
- **Blunt, not profane (here).** The source voice swears; in the history doc, keep the
  bluntness ("this was a dead end," "we got this wrong twice") and drop the literal
  profanity. The sharpness lives in the *honesty*, not the words.

## In voice, for the history doc (synthesized examples)

> *The first week was just getting a window to open and a pixel to land where you put it.*
> *Boring, load-bearing, the whole thing stands on it.*

> *Sound started as a "code-first, no music API" line in the sand (ADR 0003) — and then
> spent three weeks quietly becoming a synth, a sequencer, and eventually a small mixing
> console. Nobody planned that. It just kept being the most interesting thing in the
> room.*

> *The wah effect got built per-voice first, which was wrong — a filter on one note can't
> sweep a chord or pump with the groove. It got torn out and rebuilt as a bus effect
> (0015). Worth saying out loud: we don't only ship the wins.*

## Not this

- Not changelog-flat ("Added X. Fixed Y. Updated Z.") — that's what the derived timeline
  is *for*; editorial prose earns its place by adding the *why* and the *story*.
- Not breathless ("revolutionary," "powerful," "seamless").
- Not hedged into mush ("it could be argued that arguably this may perhaps…"). Say it.
