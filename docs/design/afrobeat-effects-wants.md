# Afrobeat — the voices I wanted but can't have yet (blocked on the effects bus)

The blind-band brief for the **Afrobeat** station (Fela Kuti / Tony Allen) named an ideal
lineup *from the genre*, before reading any cousin cart — the intent-first discipline in
[`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md). Shopping that
brief against the [instrument palette](../guides/instrument-recipes.md) had a happy result —
the genre's signature **timbres** map almost perfectly onto modeled engines we already have
(`INSTR_GUITAR`, `INSTR_REED`, `INSTR_BRASS`, `INSTR_ORGAN`, `INSTR_MEMBRANE`). But several
voices came up short, and they're short for the *same single reason*: **we have no effects
bus yet.** The 70s Afrobeat sound isn't just instruments — it's instruments *in a room,
through an amp, onto tape, with the rhythm guitar through a wah pedal.* That layer is missing.

> **Genre: design exploration.** A wishlist tied to one station's blind brief, not a bug
> list. The shipped `afrobeat.c` works around every hole below (notes in each row); this doc
> records what the cart *would* be if the effects bus existed — and, more usefully, it's a
> fresh **demand witness** for that bus: Afrobeat wants nearly the entire rostered chain.
> Companion docs: the bus is **designed, not built** in
> [`instrument-engines.md`](instrument-engines.md) § 8.10, disciplined by
> [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md); the sequenced
> build-list (effect → showcase cart → stations it rescues) is in
> [`sound-next-steps.md`](sound-next-steps.md) § "Each effect → its showcase cart". The
> shipped-station gap ledger is [`radio-genre-fidelity.md`](radio-genre-fidelity.md); this is
> its forward-looking sibling for a station being built.
>
> **UPDATE 2026-06-10 — reverb + chorus SHIPPED.** Two §8.10 effects landed:
> `reverb(size,damping)` (Afrobeat's "band-in-a-room glue" want, met — send the whole band into one
> room) and `chorus(rate,depth,mix)` (the BBD modulated-delay; meets the horn-section-width vote at
> the master level — chorus is master-wide in v1, so it widens the whole mix, not just the horns,
> until per-part aux routing lands). Still open: wah, tape, leslie, drive, and the not-yet-rostered
> compression.

---

## The wants, each mapped to the effect that would unblock it

The right column is the **§8.10 / build-list effect** that delivers it — so this table is
also "which bus effect does Afrobeat vote for, and why." The build-list already names some of
these rescues for *other* stations; Afrobeat is a second (or third) vote for the same boxes.

| want (the voice the genre asks for) | why it's short today | unblocked by |
|---|---|---|
| **Wah / envelope-filter rhythm guitar** — the *talking*, quacking funk guitar (the "Gentleman" chop). The single most identifiable Afrobeat guitar move. | `INSTR_GUITAR` gives the string + body, but the **pedal** is an effect we can't send to. Faked in-cart with one swept `FILTER_BAND` peak (the mouthharp / epiano-clav stopgap) — a hint of quack, not the pedal. | **wah / auto-wah** (build-list already lists this rescuing *citypop's* funk guitar — Afrobeat is the second vote) |
| **Band-in-a-room glue (reverb)** — every 70s Afrobeat side is a live band in one room; the ambience *is* the sound. | No reverb at all. Nothing fakes a room convincingly; the cart plays dry. | **reverb** (§8.10 #1, bus-only — "the biggest") |
| **Tape saturation / analog warmth** — Afrika 70 on tape through a console: soft compression, harmonic warmth, the glue that makes the horns and guitars one band. | No tape/saturation stage. The mix is clean and digital where it wants to be warm and slightly squashed. | **tape (wow/flutter/sat)** (build-list → Frippertronics showcase) |
| **Horn-section width** — a real horn line is 3–5 players slightly apart in tune and time. We stack 2 modeled horns (`reed/tenor_sax` + `brass/trumpet`); they sound like 2 players, not a section. | No chorus / unison-width. Partly faked in-cart by **detuned + slightly-delayed layering**, but that's a stand-in for the real ensemble spread. | **chorus / unison width** (§8.10 #2; build-list → Juno) |
| **Organ that's actually there** — the combo organ/B3 comp is thin without its **Leslie**, tube grit, and room. *(Raised in review: "organ is not really there yet without Leslie.")* | The Leslie *rotor* is modeled (organ.c `leslie/slow`·`leslie/fast`) but only as a per-note layered recipe; the radio chassis voices a clean organ. Without the rotary swirl + amp drive + room it reads as a static drawbar, not a Hammond. | **leslie (rotary)** (build-list → "Hammond B3 + Leslie" showcase, also rescues roadhouse/yacht) **+ drive + reverb** |
| **Electric bass that sits in the pocket** — fingered electric through an amp, *compressed*, round and even. *(Raised in review: "electric bass ... is not really there yet.")* | A clean modeled/synth bass is thin and dynamically uneven; the genre's bass is an *amped, compressed* instrument. We have neither amp character nor compression. | **drive (amp)** + **compression** (compression is **not yet rostered** — see below) |

## The gap in the gap: compression isn't even on the bus roster yet

Two of the wants above (the even funk-guitar dynamics, the bass sitting in the pocket) and the
genre's overall *pumping glue* need **compression / sidechain** — and unlike wah/reverb/leslie,
compression is **not** in the §8.10 effect list or the build-list. The dance stations fake a
sidechain pump by re-aiming filters per frame (house's filter ride), but that's a per-voice
trick, not a dynamics processor. **Flagging it here as a roster candidate**: an Afrobeat (or any
live-band) station is the natural demand for it, and its showcase cart writes itself (a
**dbx/1176-style compressor pedal** with a visible gain-reduction meter — the same effect →
showcase-cart flywheel). Worth a line in `sound-next-steps.md`'s roster if it earns a vote
beyond this one.

## Not-effects: two holes that are the cart's own generative business

Two more holes the brief found are **not** the effects bus's job — and not the engine's
either. They're **layer-3 cart logic**: the generative C a station writes on top of the shared
chassis (`radio.h`'s clock, `improv.h`), classified in the
[`game-music.md`](../guides/game-music.md) **brain catalog** (chord / time / melody / bass /
form brains). A good cart-local brain *graduates* into a shared header once a second cart wants
it (as `improv.h` did, and as `cocktail`'s walking bass is queued to). Recorded here so a future
reader doesn't expect the bus — or a new engine — to close them:

- **Tony Allen's broken-funk feel** — the human drummer's off-grid ghost notes and elastic
  accenting. This is a **time brain / feel** (a sibling of `yacht`'s "Purdie half-time shuffle
  + ghosts", `lowend`'s push/drag groove templates, `cocktail`'s 55–62% swing). It's pure cart
  C — *when* and *how hard* notes fire — riding **on top of** `radio.h`'s schedule-ahead clock,
  which the cart must not reimplement. Graduates to a header if a second Afro/funk station wants
  it. Brain backlog: [`future-stations.md`](future-stations.md).

- **Call-and-response vocals** — Fela's lead call + the chorus answer. This is really *three*
  layers, only one of which is a real hole:
  - **Timbre** → `INSTR_VOICE` (the `vox` carts), an **engine** capability that exists and is
    maturing. A given, not a hole.
  - **The call-and-response phrasing** (lead states → chorus answers) → an ordinary **form /
    melody brain**, the cart's business — and *already half-built*: `roadhouse`'s improviser
    does "motif → stated / answered / sequenced / doubled", and a horn answer is a form pattern.
    Buildable today.
  - **Intelligible *words*** → the only genuinely hard part — and for a *generative radio* it
    **dissolves**: radio is wordless/instrumental, so recognizable lyrics aren't a brain we'd
    ever build here. The realistic version is a **wordless** vocal call-and-response (a vowel
    "hey!" shout, a hummed answer) — engine + cart brain, doable now.

  So the vocal "hole" is mostly a *buildable* cart brain plus an existing engine; the cart can
  leave the chair empty for v1 and add a wordless `INSTR_VOICE` answer later without waiting on
  anything.

---

## What this says about the effects bus

One station's blind brief just voted for **wah, reverb, tape, chorus, leslie, drive** — six of
the rostered bus effects — *plus* surfaced **compression** as a missing roster entry. That's the
strongest single-station demand signal for the bus so far, and it lands exactly where § 8.10
predicted the value is: the bus doesn't just add polish, it's the difference between "a stack of
modeled instruments" and "a band." When the bus ships, `afrobeat.c` is a ready acceptance test —
nearly every effect has a job to do in it.

*(Pairs with the cart `tools/carts/afrobeat.c` and its `radio-voices.md` chart once shipped.)*
