# Napoleon radio — the voices I wanted but can't have yet (blocked on the effects bus)

The blind-band brief for **Napoleon radio** ([`napoleon-blind-brief.md`](napoleon-blind-brief.md))
named an ideal lineup *from the music* — the film's soundtrack + John Swihart's score — before
reading any cousin cart (the [radio-station-howto](../guides/radio-station-howto.md) firewall).
Shopping it against the [instrument palette](../guides/instrument-recipes.md) went well: the
*timbres* mostly map onto modeled engines, and the cart's headline addition — a **sung falsetto
on `INSTR_VOICE`** (Kip's croon + the Jamiroquai "ooh" — the first radio voice to *sing* on
that engine, where AIR uses it as a vocoder) — needed no effect at all. But this is an *artist* station spanning five genres, and several of
those genres are *defined by an effect*, not just instruments. Those are short for the same
reason every station is: **there is no effects bus yet.**

> **Genre: design exploration.** A wishlist tied to one station's blind brief, not a bug list.
> The shipped `napoleon.c` works around every hole below (notes in each row); this records what
> the cart *would* be with the bus, and it's a fresh **demand witness**. Companion docs: the bus
> is **designed, not built** in [`instrument-engines.md`](instrument-engines.md) §8.10, disciplined
> by [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md); the build-list
> (effect → showcase cart → stations rescued) is in [`sound-next-steps.md`](sound-next-steps.md).
> Pattern sibling: [`afrobeat-effects-wants.md`](afrobeat-effects-wants.md).

---

## The wants, each mapped to the effect that would unblock it

| want (the voice the archetype asks for) | why it's short today | unblocked by |
|---|---|---|
| **Gated reverb** — FOREVER ("Forever Young") is *built on* the giant 80s gated snare/tom: a huge bright tail slammed shut. It's the single most identifying sound of the genre. | No reverb, and no gate. Faked in-cart with a long-release (R220, S2) band-passed noise snare — that's a longer crack, not a cavern that suddenly stops. | **reverb** (§8.10 #1, the biggest) **+ a gate** (a noise-gate / envelope on the send — not yet a distinct roster entry; flag below) |
| **Plate / spring reverb** — SERENADE (Kip's quiet-storm serenade) is a ballad *in a hall*: the croon, the Rhodes and the string pad all want to be drenched and blooming. | Dry. The lush wash is half the genre; the voices read as close-mic'd and bare. | **reverb** (§8.10 #1) — second vote, the "ballad" use-case |
| **Tape wow / flutter + lo-fi** — SWIHART's deadpan score is partly *cheap recording*: a Casio through a tape 4-track, slightly unstable pitch, rolled-off highs. | No tape stage. Faked with a slow `LFO_PITCH` wobble on the toy organ + a detune-twin square for "chorus," and dark filtering. That's a hint of the cheapness, not the medium. | **tape (wow/flutter/sat)** (build-list → Frippertronics showcase) |
| **Chorus / ensemble** — the 80s saw pad (FOREVER) and the toy organ (SWIHART) both want a real chorus to widen and shimmer. | No chorus. Faked with `instrument_tune` detuned pairs (the ombak trick) — gives beating, not the swept multi-voice ensemble. | **chorus / unison width** (§8.10 #2; build-list → Juno) |
| **Auto-wah pedal** — DANCE ("Canned Heat") is funk: the clavinet wants a real envelope-following wah pedal (the quack). | `INSTR_EPIANO` clav + `instrument_follow(LFO_CUTOFF)` on a `FILTER_BAND` gives a hint of the quack, but it's one swept peak, not the pedal. | **wah / auto-wah** (build-list, already rescuing citypop/afrobeat guitars — a third vote) |

## The gap in the gap: a *gate* isn't on the roster yet

FOREVER's defining sound isn't reverb alone — it's reverb **cut short by a noise gate** (the
"gated reverb" Hugh Padgham invention). Reverb is rostered (§8.10 #1); the *gate* that chops its
tail is not a distinct entry. Like compression (flagged in afrobeat's wants), it's a **dynamics**
processor the bus design hasn't named. **Flagging it as a roster candidate**: an 80s-synth-pop
station is the natural demand, and its showcase cart writes itself (a reverb pedal with a
gate-threshold knob → the snare goes from cavern to *thwack-silence*).

## Not-effects: the cart's own generative business

- **The five-genre clash itself** is a **form/arrangement brain**, not an effect — solved in
  `napoleon.c` by the archetype roll (each press of SPACE bundles its own groove/tempo/voices).
  This is layer-3 cart logic, the artist-station pattern (jingle/citypop/yacht).
- **A real horn section** (DANCE) wants 3–5 players slightly apart — same chorus/width want as
  afrobeat's horns; faked here by voicing the brass stab as a tight `rad_lead_to` cluster.

---

## What this says about the effects bus

Napoleon's five archetypes vote for **reverb (×2), tape, chorus, wah** — four rostered bus
effects — and surface **a gate** as a missing roster entry (the gated-reverb snare). Because it's
an *artist* station each effect maps to a *specific recognizable record*, so when the bus ships,
`napoleon.c` is a sharp acceptance test: FOREVER literally cannot sound like Forever Young
without the gate, and the serenade cannot bloom without the plate. The instruments are there
(including the new `INSTR_VOICE` croon); the *room and the medium* are what's missing.

*(Pairs with `tools/carts/napoleon.c` and its `radio-voices.md` chart.)*
