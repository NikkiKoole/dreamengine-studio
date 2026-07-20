# Harmony brain — a shared, bidirectional next-chord + analysis engine

> **STATUS: SHIPPED v1 (2026-07-20, same day as the research — brief → header → toy in one session).**
> `runtime/harmony.h` exists and is **bidirectional**: the 13-function vocab + `HB_BOSSA`/`HB_COCKTAIL`
> style tables extracted from bossa/cocktail with **byte-identical pinned-seed WAV A/B proof** (the
> `radio.h` seed rule held: `cmp == 0` on both carts), plus the two missing directions — `hb_suggest`
> (ranked options + a one-word reason) and `hb_analyze`/`hb_chord_fn` (key-in roman numerals, `-1` =
> honest out-of-vocab). The demand-82 toy shipped as **`chordwise`** (analyzer strip + NEXT panel +
> the vocab palette, QWERTY entry, doo-wop cold-open) carrying the spec() oracle: **24 assertions**
> incl. the all-keys round-trip (analysis inverts generation exactly). Still open, in "What's still
> open" below: the chromatic encoding (`V/x`), re-expressing the template stations, chord-aware
> `improv.h`, the toy's LOOK pass (maker steering), and per-genre weight sets beyond the two lifted
> tables.
>
> *(Original brief, kept for the research record:)* A research topic, not a build ticket.
> Sparked by a demand-discovery drip (r/musictheory, demand **82** — the loudest single topic
> across every tribe sampled) whose recurring, specific wish is a *"harmonic progression
> analyzer / suggestion plugin."* We have lots of carts that *play* chords and one cart that
> *generates* progressions — but nothing that **suggests a next chord or analyzes a progression
> you give it**. The interesting part (the maker's framing): can one "next chord" model both
> **generalize the radio carts' harmony brains** *and* serve a new interactive chord toy — i.e.
> graduate harmony to a shared engine the way `acid303.h` / `drumkit.h` did for voices? **Deep-research
> survey done (2026-07-20) — see [Research findings](#research-findings-deep-research-survey-2026-07-20):
> the evidence backs the simplest model (1st-order Markov over functions + forced cadences), and analysis
> must take the key as input.**

## Why this is a real topic (and on-grain)

- **Demand is loud and specific.** r/musictheory's *"is there a progression analyzer/suggester"*
  recurs across dozens of threads (demand 82). It also sits on the cross-tribe backlog already —
  [`field-notes/026-demand-discovery-cross-tribe.md`](../field-notes/026-demand-discovery-cross-tribe.md)
  ranks *"chord identify/progression"* among the top on-grain builds from 1,411 wishes / 24 tribes.
- **On-grain for the console.** A humble lo-fi theory *tool/toy* — not a notation app (that grain
  is explicitly off, per [`demand-discovery.md`](demand-discovery.md)).
- **Oracle-verifiable AND delightful — the [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) two-part bar.**
  Roman-numeral analysis of a progression in a key is *deterministic*, so a chord toy built on this
  can carry a real `spec()`. And it's legible to a stranger: type `C Am F G`, watch `I vi IV V —
  "the doo-wop / 50s progression"` light up.

## What already exists (verified source map, 2026-07-20)

Three distinct layers — one is a real "next chord" engine we can build on, one is a shared voicing
block, and the third layer (analysis/suggestion) is entirely missing.

### Layer 1 — functional next-chord grammar: EXISTS, but locked in two carts, generate-only

`bossa.c` and `cocktail.c` already implement real functional harmony as a **weighted Markov
transition table over roman-numeral *functions*** (not absolute chords), with ii–V–I cadences
hard-forced into the last two bars of each section.

- Functions are `{scale-degree offset, quality}` pairs — `bossa.c:91-95`
  (`F_I…F_I7`, `F_OFF[]`, `F_QUAL[]`).
- The grammar is the transition table `TRANS[]`, weighted by repeated entries and commented as a
  jazz cheat-sheet ("ii→V→I, secondary dominants resolve down a fifth, bII7 = tritone sub of V,
  backdoor cadence") — `bossa.c:97-118`.
- The picker + section generator — `bossa.c:177-184`:
  `markov_next(f) = TRANS[f][srnd(TRANSN[f])]`; bars 0–5 free walk, bars 6–7 forced `ii → V/bII7`.
- `cocktail.c:81-92` + `:160-168` — the same design, table `FNEXT[NF][6]`, cadence forced at
  `bar%8==6/7`.

**The limitation:** it is *one-directional* (seed → progression) and buried as `static const`
tables inside two carts. It is never queried "given function X, list weighted candidates for a UI,"
and there is no inverse (progression → functions).

### Layer 2 — voicing: ALREADY a shared, graduated block

`rad_lead_to` / `rad_bass_to` in `runtime/radio.h:96-144` — nearest-tone voice-leading. Given a
chord *already chosen* (rootpc + interval set), it moves the held voices to the nearest tones. This
answers **"how do I VOICE this chord,"** which is *orthogonal* to "which chord next." The header
calls it "the single biggest 'sounds composed' trick." `chordblossom2`'s flavor table
(`chordblossom2.c:172-193`) and `pocketbox`'s chord track (`pocketbox.c:489-497`) are **consumers of
voicing/comping** — they re-voice or replay chords, they don't *choose* them. Leave this layer as-is;
the harmony brain sits *beside* it and feeds it.

### Layer 3 — analysis + interactive suggestion: MISSING ENTIRELY

- The engine has only *sound-producing* chord primitives (`studio.h:749-783`: `CHORD_*`, `chord()`,
  `strum()`, `degree()`, scales). A recursive grep for `roman|next_chord|suggest|functional|analyze`
  across `runtime/` returns **nothing** — no function type, no key context, no analyzer.
- The "stolen playbook" stations (`citypop`/`house`/`yacht`/`air`/`jingle`/`polopan`) are **hard-coded
  cited-track templates** — a `{off,q}` `Ch` struct + a static 4-chord table (e.g.
  `citypop.c:91-105`, `house.c:111-134`). No generation of the changes; the seed only *selects* a
  template. Useful as a corpus, not a model.
- `improv.h` is **chord-blind** — scale/key-locked only; `improv_render` even does `(void)mode7`
  (`improv.h:71`). Nothing feeds it the current chord. A shared harmony brain is the missing bridge
  that could finally make solos chord-aware.

## The core insight — analysis is the inverse of generation

bossa's engine maps **function → pitches**. A roman-numeral analyzer maps **pitches → function**.
Interactive suggestion is just reading the *same transition table forward* and surfacing the weighted
options instead of sampling one. If the radios (generate), `chordblossom`'s flavors (voice/colour),
`pocketbox`'s chord track (store/replay), a chord-aware `improv.h` (solo over changes), and the new
toy (suggest/analyze) all speak **one shared function vocabulary + transition model**, they generalize
together. That's the graduation move — harmony's `acid303.h`.

## The research questions (the actual work)

This is what a focused research pass — internal spike + external survey — should resolve. Each is a
genuine open question, not a foregone conclusion.

1. **Representation.** Functions/roman-numerals (key-relative, like bossa) vs absolute chords vs
   both? A key context is required for analysis — where does key come from (declared? detected from
   the progression)? How are borrowed chords / modal interchange / secondary dominants represented
   in one vocabulary (bossa already needs `bII7`, `VI7`, backdoor `bVII7`)?
2. **The model.** Keep bossa's *hand-authored weighted Markov* (interpretable, tiny, already tuned),
   derive one from a corpus, or use a functional-harmony *grammar* (rule-based cadence pulls)? What
   does each cost in code size and "sounds right"? Is a first-order Markov enough, or does good
   suggestion need context (the last 2–3 chords, position in the phrase — bossa's cadence-forcing is
   already a crude positional rule)?
3. **Genre generalization — the maker's central question.** bossa's `TRANS[]` is jazz-specific; the
   stolen-playbook brains are hard templates. Is there **one transition model parameterized by
   genre** (a small set of weights/allowed moves per style), or a *family* of tables? Can the six
   cited-track template stations be re-expressed as (or validated against) the same function grammar,
   or are they genuinely un-modelable one-offs? What generalizes cleanly, and what must stay bespoke?
4. **Analysis (the inverse).** pitches → function given a key: the ambiguity problem (the same four
   pitches are different functions in different keys/contexts). How much context is needed to name a
   chord's function reliably? Is naive "closest diatonic function + a borrowed-chord table" good
   enough for a toy, or does it need real key-finding (Krumhansl-style)?
5. **Suggestion UX.** How many next options to surface, how to *rank* them (raw transition weight?
   cadence pull toward home?), and how to *explain* one ("V → I: resolve home"). This is where the
   toy earns the "delightful to a stranger" half.
6. **The oracle.** Define the deterministic acceptance test: a set of known progressions → their
   canonical roman-numeral analysis, so the analyzer carries a `spec()`
   ([`spec-harness.md`](spec-harness.md)). This is the gate that keeps it honest.

### External survey worth running (deep-research)

How do existing tools model this, and what can we steal for a lo-fi version? Hooktheory's *Trends*
(a real chord-transition frequency corpus from analyzed songs), ChordChord / Captain Chords / Scaler
(commercial suggesters), and the academic lineage (Markov/HMM chord models, functional-harmony
grammars, Riemannian/neo-Riemannian voice-leading, Krumhansl key-finding). Goal: pick the *simplest*
model that gives musical suggestions and honest analysis — not the most sophisticated.

## Research findings (deep-research survey, 2026-07-20)

A cited deep-research pass (21 sources, 24 claims adversarially verified) ran the survey above. The
headline: **the evidence validates bossa's existing approach — simplest wins.** Verdict per operation:

- **GENERATE + SUGGEST → a first-order Markov table over key-relative Roman-numeral functions**
  (HIGH-confidence, corpus-grounded). Harmonic motion is *highly concentrated*: five roots
  (I/IV/V/♭VII/vi) = **87% of rock chords**; **93% of chords after Em-in-C go to just Am or F**
  (de Clercq & Temperley's 100-song rock corpus; Hooktheory's 1,300-song pop corpus — both hand you
  priors + transition weights to seed the table). **Higher Markov order isn't worth it** — 2nd-order
  beats 1st by ~0.03; chord LMs gain tenths of a percent 2→5-gram. The one exception is **cadences**,
  which carry trigram info (pre-tonic V is overwhelmingly preceded by IV) — fixed by a few
  forced-cadence special cases, *exactly* bossa's forced ii-V-I. Caveat: naive long autoregressive
  runs compound errors → favor **short, cadence-anchored/reseeded phrases** (bossa's 8-bar sections
  already do this).
- **ANALYZE → "closest diatonic function + a borrowed-chord lookup table," operating in a GIVEN key.**
  The honesty check that reshapes the feature: **full automatic Roman-numeral analysis is UNSOLVED**
  — SOTA deep systems reach only **43–52%** complete-RN accuracy — so the toy must *take the key as
  input* (the constrained one-chord-in-a-known-key problem is far easier: chord-quality ~78%,
  local-key ~81%). For a *fallback* key guess use **Krumhansl-Schmuckler**, and Temperley's
  simplification — a plain **scalar product** (∑ profile×pitch-class-duration) replaces the
  correlation for ~the same result, far cheaper. Short context-free segments are unreliable (K-S fell
  to ~46% on Chopin snippets), so only key-find over a window and always prefer a declared key.
- **REPRESENTATION + GENRE → key-relative functions; genre = different *weights* over ONE shared
  vocabulary**, not different grammars (answers RQ #1 + #3). Rock over-weights ♭VII; pop often omits
  the tonic. One function table, per-genre weight sets. So the cited-track "stolen playbook" stations
  are weight/where-tonic-sits variations on the same model, not un-modelable one-offs.

**Still genuinely open (the research couldn't settle):**
- **Chromatic encoding (RQ #1 tail)** — the *minimal* way to carry secondary dominants (`V/x`) and
  modal interchange in a tiny vocab without exploding the state space. No source prescribed it.
- **Jazz quantitatively** — the one-model-per-weights finding is measured for pop/rock; jazz's
  extended/secondary-dominant-heavy language is only asserted-by-disclaimer, not measured.
- **Suggest UX (RQ #5)** — corpus data (93% → 2 chords) *implies* **~2–4 ranked options** is well
  motivated, but no source studied option-count/ranking directly. A design call, not a research one.

Source-strength caveat: the "1st-order is enough" evidence leans partly on a weak melody study + an
audio chord-*recognition* paper (LM auxiliary), so it's directionally strong but not proven for pure
chord *generation*; the 43–52% RN figures are for the FULL task, a floor-of-difficulty caution, not a
ceiling on the constrained toy. Full report: workflow run `wf_a3e8a018-597`.

## Proposed shape (a hypothesis to test, not a spec)

A header-only, static, bidirectional `harmony.h` sitting beside `rad_lead_to`, exporting roughly:

- `hb_generate(...)` — the bossa/cocktail walk, lifted and genre-parameterized (radios adopt it,
  proving byte-identical seeds survive — the `radio.h` seed-compatibility rule).
- `hb_suggest(context, key, out[]) → n` — weighted next-chord candidates + a one-word reason each
  (powers the toy's "suggest" mode; the transition table read *forward*).
- `hb_analyze(chords, n, key) → functions[]` — roman-numeral analysis (the toy's "analyze" mode +
  the `spec()` oracle; the *inverse*).

Consumers: the radio chord brains (generate), `chordblossom`/flavors (feed voicing), `pocketbox`
chord track (store), `improv.h` (finally chord-aware), and a **new chord toy** (the demand-82 build).

## What's still open (post-v1, 2026-07-20)

What shipped vs the proposed shape: the vocab, both style tables, `hb_pick`/`hb_suggest`/
`hb_analyze` + `hb_selfcheck`, and the `chordwise` toy. Deliberately NOT shipped: a monolithic
`hb_generate` — the carts keep their own walk loops (cadence forcing is *form*, cart-owned) and
call `hb_pick` per step; that's what made the byte-identical proof trivial.

- **The toy's LOOK** — `chordwise` v1 is a functional layout, baked for the maker's eyeball pass
  (mockups-first: steer with reference images, then re-skin).
- **Chromatic encoding (`V/x`, borrowed-degree flags)** — the RQ #1 tail; unresolved by research,
  a design call for when a progression needs naming beyond the 13 seats.
- **The template stations** (`citypop`/`house`/`yacht`/`air`/`jingle`/`polopan`) — re-express or
  validate their hard-coded 4-chord tables against the vocab (per-genre *weight sets*, the research
  says); would prove "one vocab, many weights" beyond jazz.
- **Chord-aware `improv.h`** — ✅ the bridge landed (2026-07-20): pure `improv_snap` /
  `improv_is_target` / `improv_midi_chord` pull a solo's target notes onto the current chord's tones
  — no `rnd()`, so a performance/seed stream can't shift, and `chordPc=NULL` is byte-identical to the
  old key-relative path. A **per-station scope knob** picks how much lands: `IMPROV_SNAP_STRONG` (bar
  downbeats + the resolving note, locked-on) vs `IMPROV_SNAP_RESOLVE` (only the phrase's final note —
  downbeats stay loose; the more human feel). Carries `improv_selfcheck()` (DE_SPEC, both scopes).
  `cocktail` is the first customer (its `chord_pcs()` feeds the bar's tones; chose RESOLVE after an
  A/B). **Open: roll it out** to the other 5 improv stations (`addis`/`afrobeat`/`motorik`/`roadhouse`/
  `squarepusher`) — each needs a `chord_pcs`-style feed + a scope choice at its solo call site.
- **`pocketbox`'s chord track / `chordblossom2` flavors** — adopt the vocab as their chord type.
- **Suggest UX depth** — reasons are one word; the "explain the pull" layer (e.g. tension arrows
  toward home) is untouched.

## Non-goals / constraints

- **Not a notation app, not a DAW plugin.** A humble lo-fi theory tool/toy on the console.
- **Don't reinvent voicing** — `rad_lead_to` stays the voicing layer; the harmony brain only *picks*.
- **Simplest model that sounds right** wins over theoretical completeness (the console's whole thesis).
- **Seed-compatibility is sacred** — if radios adopt `hb_generate`, migration must not add/remove/
  reorder a single PRNG call, or every pinned `*_SEED` breaks silently (`radio.h` rule, in
  [`game-music.md`](../guides/game-music.md)).

## Prior art to read (verified anchors)

- **The engine to lift:** `bossa.c:91-118` (functions + `TRANS`), `bossa.c:177-184` (picker +
  forced cadence); `cocktail.c:81-92,160-168` (same, `FNEXT`).
- **The voicing to reuse:** `runtime/radio.h:96-144` (`rad_lead_to` / `rad_bass_to`).
- **The corpus (templates):** `citypop.c:91-105`, `house.c:111-134`, `air.c:100-113`,
  `jingle.c:91-109`, `polopan.c:107-124`.
- **The chord-blind soloist to bridge:** `runtime/improv.h` (`:71`, `:127-140`).
- **The voicing consumers:** `chordblossom2.c:172-193` (flavor table), `pocketbox.c:489-497,610-618`
  (chord track + boot progressions).
- **The (empty) engine surface:** `studio.h:749-783`.

## See also

- [`game-music.md`](../guides/game-music.md) — the "chord brains" catalog (7 brains) this generalizes
  from; bossa is its worked example (chord brain #1 + the voice-leading recipe).
- [`bossa-rack.md`](bossa-rack.md) — the chord-bloom rack: "play the changes, the band follows" (the
  play-first surface a suggester complements) + its `bossa.c` harmony ground-truth pass.
- [`demand-discovery.md`](demand-discovery.md) / [`field-notes/026-demand-discovery-cross-tribe.md`](../field-notes/026-demand-discovery-cross-tribe.md)
  — where the demand-82 signal and the "chord identify/progression" backlog line come from.
- [`spec-harness.md`](spec-harness.md) — the deterministic oracle the analyzer must carry.
