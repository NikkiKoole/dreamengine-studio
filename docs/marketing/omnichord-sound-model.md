# Omnichord — the sound model (what to mimic for Strumharpy)

> Serves the **authentic-musicality** build-task ([`tinyjam-launch-strumharp.md`](tinyjam-launch-strumharp.md)
> + [`research-insights.md`](research-insights.md)): the honest, product-side differentiator. Two layers —
> the **signal chain** (electrical) and the **musical structure** (voicing). Both are achievable on
> dreamengine's *existing* engines; the win is *accuracy*, not exotic DSP. Web-researched 2026-06-29 from
> repair/teardown docs + history articles (links at the bottom; treat chip-level claims as best-available,
> not gospel).

## 1. The signal chain (electrical)

The early Omnichords (OM-27 1981 → OM-36/OM-84 1984; analog/hybrid until PCM arrived with the 200M in
1989) all work the same way:

> **All sounds start as square waves from a digital timer chip, are filtered, and pass through a VCA** —
> that filtered-square-through-a-VCA is the "warm" Omnichord tone.

OM-84 specifics (from repair docs):
- The "voice chip" **M5M82C54P-6** is a **programmable interval timer** (an 82C54/8254-class 3-channel
  PIT) — i.e. it literally clocks out square-wave *frequencies* for the chord tones.
- A Mitsubishi **M50740** microcontroller drives the **drums + bassline** and feeds the timer.

**To mimic (cheap on our engine):** square / pulse oscillators (our SQUARE/SAW/PD waves) → a gentle
**lowpass** to round the buzz into "warm" → simple VCA-style amp envelopes. That's the whole core voice.

## 2. The chorus / vibrato (the signature shimmer)

The Omnichord's lush shimmer is essentially **chorus from natural detuning**. Ballpark recipe: a slow
**~0.5–1 Hz**, moderate-depth chorus. The Sonic Strings **vibrato** button adds a shimmering pitch
wobble on top.

**To mimic:** our `chorus()` (slow rate, moderate depth) on the strum/chord bus, + an optional pitch-LFO
vibrato on the strum voice. This is most of what makes it *sound* like an Omnichord rather than a bare
square wave — don't skip it.

## 3. The strum plate ("Sonic Strings")

A touch strip that **strums an arpeggio of the held chord across ~4 octaves**, always in tune with the
selected chord; one or several fingers → a glissando. Later models gave the strings selectable voices
(vibes / brass / organ / guitar / banjo).

**To mimic:** map touch position → which chord-tone in the 4-octave span; sound each as a short-envelope
square-through-lowpass pluck; multi-touch = the glissando. (dreamengine's multitouch + the strum/solo
headers already do the gesture side.)

## 4. The musical structure — the part "no other app includes"

From the OH-84 thread ([`raw/omnichord-heaven-oh84-thread.md`](raw/omnichord-heaven-oh84-thread.md)), the
*voicing* quirks omnichord players actually check for:

- **Fudged triads:** the chord section isn't true chords — it **drops 3rds/5ths to make room for 7ths**
  so a 3-note voice still reads as the full chord. (Note the lovely fit: the OM-84's PIT has **3 timer
  channels** → 3 simultaneous tones, which is *why* the triads are fudged.)
- **The F# note-wrap:** an F#-to-F# range limit means the strum root isn't always the lowest pitch — the
  top note drops an octave, so strumming bottom→top **isn't always ascending** (a B chord strums
  B1 D#1 F#0, B2 D#2 F#1, B3 D#3 F#2 …).

This voicing logic — not the raw timbre — is the authenticity that earns omnichord-players' respect.

## 5. The rhythm box

Built-in electronic **drum patterns + a bassline** (the M50740 on the OM-84). Simple synthesized
percussion. (This is also the honest basis for the "Strumdrum"-style read — the real Omnichord *does*
have a rhythm section.) **Mimic** with our 808-ish / simple kit.

## Net

`square/pulse → lowpass → VCA` + a slow **chorus** (+ vibrato) + a touch-mapped 4-octave arpeggiator that
honours the **fudged-triad + F#-wrap** voicing + a simple **rhythm box**. **No new DSP** — it's a recipe
over engines we have. The differentiator is the *voicing accuracy* and the *chorus character*, not novel
synthesis.

## Sources (verify; chip-level = best-available)
- [Omnichord — Wikipedia](https://en.wikipedia.org/wiki/Omnichord)
- [Strumplates & Circuit Bending: History of the Omnichord — Perfect Circuit](https://www.perfectcircuit.com/signal/suzuki-omnichord-history)
- [Suzuki Omnichord — diyAudio thread](https://www.diyaudio.com/community/threads/suzuki-omnichord.231603/)
- [Suzuki Omnichord — Circuit Bending Wiki](https://circuitbending.miraheze.org/wiki/Suzuki_Omnichord)
- [OM-84 repair — Erich Izdepski](https://erichizdepski.wordpress.com/2017/11/11/suzuki-omnichord-om-84-repair/)
- [Suzuki Omnichord 101 — The Pro Audio Files](https://theproaudiofiles.com/suzuki-omnichord/)
