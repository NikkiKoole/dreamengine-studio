# The xx — the blind band brief

> Phase-1 intent-first brief for an ARTIST radio station (`thexx.c`), written from the
> music before reading any cousin cart (the firewall). Companion to the runbook
> [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md). An **artist
> station** (stolen-playbook archetypes, kin to `air`/`wba`/`napoleon`) — but its headline
> is a genuinely **new brain**. Sibling brief: [`wba-blind-brief.md`](wba-blind-brief.md).

## The one thing that must be true

The xx is **intimacy + negative space**. The gaps *are* the song — they leave more out
than any band, drench the few events in reverb, and let silence carry the weight. Two
things define them and nothing on the dial does either:

1. **THE BOY/GIRL VOCAL CALL-AND-RESPONSE.** Romy (close, breathy, higher) and Oliver
   (lower, conversational) **trade lines** — she states, he answers, sometimes they meet
   in close unison. A *conversation*, intimate and unhurried. (air used one vocoder lead,
   napoleon one falsetto, eno a choir — none is *two singers talking to each other*.)
2. **SPACE AS STRUCTURE.** Extreme restraint, nocturnal, dark-minor; the arrangement is
   mostly rests. The cousin of the parked Japanese *ma* idea, made the whole identity.

## The record, from the music

The xx (Romy Madley Croft — guitar+voice; Oliver Sim — bass+voice; Jamie xx — production):
*xx* (2009), *Coexist* (2012), *I See You* (2016). The band:

- **Two voices, trading** — close, breathy, almost whispered, lots of air around each line.
- **Romy's guitar** — clean, single-note, *icy*, heavily reverbed/delayed; a sparse 2–3 note
  motif, not chords. Telecaster-clean into a huge hall.
- **Oliver's bass** — deep, round, **dub-influenced sub**, prominent and slow; often the most
  present thing in the mix, melodic and spacious.
- **Jamie's percussion** — minimal, clicky, electronic; finger-snaps, claps, a sparse kick,
  and **steel-pan / marimba melodic dabs** (Coexist / I See You), all drenched in reverb. Not
  a driving beat — pointillistic, with rests between every hit.
- **Reverb + silence** — the production *is* the band; the huge reverb and the gaps are voices.

Harmony: slow, simple, **dark minor**, melancholic. Tempo ~85–115, spacious; sometimes a
UK-garage swing (*I See You*), often nearly beatless (*Angels*).

## The brains

- **THE VOCAL CALL-AND-RESPONSE — the headline (a NEW brain).** Two `INSTR_VOICE` voices, a
  **low male** (large tract, dark vowel, soft effort) and a **higher female** (smaller tract,
  brighter vowel, breathy), each singing a seeded melodic cell, that **TRADE by phrase** —
  A states, B answers — and occasionally meet in **close unison/harmony**. A *duet
  conversation* brain: who-sings-when is the structure, and the silence between turns is part
  of it. Reusable by any call-and-response act. (Built on the VOICE work air/napoleon/eno did,
  but as two conversing singers — new.)
- **SPACE AS STRUCTURE (the form/arrangement brain).** Density stays *low* by design — the
  arrangement is gaps. Few events, each into a big reverb, with rests carrying weight. The
  anti-busy station; *ma* as the form. (Lineage: the parked Japanese-music *ma* note.)
- **SLOW DARK-MINOR HARMONY** — a simple, slow minor progression (i–VI–III–VII / i–v–VI), held;
  the chords barely move so the voices and the space lead.
- **DUB SUB BASS** — deep round sub as a co-lead (Oliver), prominent and spacious (dub's
  bass-forward idea, but nocturnal-minimal).
- **POINTILLISTIC PERCUSSION** — Jamie's clicky minimal kit + steel-pan/marimba dabs, reverb-
  drenched, lots of rests.

## The voices (palette mapping — to confirm in Phase 2)

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **voice A — Oliver (low male)** | `INSTR_VOICE` (large tract, dark vowel U/O, soft effort), low register | one half of the trade |
| **voice B — Romy (higher female)** | `INSTR_VOICE` (smaller tract, brighter A/E, breathy), higher register | the other half; meet in unison sometimes |
| **icy guitar** | `INSTR_GUITAR` clean / `INSTR_PLUCK`, single-note + heavy echo + huge reverb | the 2–3 note motif |
| **dub sub bass** | `INSTR_SINE`/`INSTR_SAW`+LP, deep round, prominent | Oliver's bass, the anchor |
| **steel-pan / marimba dabs** | `INSTR_MALLET` (steel-pan/marimba material) + reverb | Jamie's melodic percussion |
| **clicky kit** | `INSTR_SINE` kick · `INSTR_NOISE` snap/clap, sparse | pointillistic, drenched |
| **the space** | a big `reverb()` + sends, maybe a touch of `tape()` | the production as a voice |

Reuses the VOICE engine and our mallet/guitar/sub timbres — the new work is the brains
(the vocal trade + space-as-structure). Likely **no new engine API**.

## NOT SAMEY — the archetype roll

The xx has a recognizable catalogue, so roll **song archetypes** (cited tracks), each fixing
who leads, the groove, tempo and palette:
- **CRYSTALISED** — the guitar+bass interlock, the two voices trading, the icon.
- **ISLANDS** — a gentle UK-garage click, romantic, both voices.
- **VCR / REUNION** — sweet, steel-pan/glockenspiel-forward (Coexist).
- **ANGELS** — Romy near-solo, almost beatless, gorgeous and bare.
- **ON HOLD** — *I See You*: brighter, more produced, a steel-pan hook.

Plus per-song key/mode-minor, reverb size, which voice opens, and the percussion palette
(snaps vs steel-pan).

## Player knobs (radio.h chrome)

- **space** — the restraint/reverb dial (barer ↔ fuller).
- **tone**, **tempo**; SPACE re-rolls the archetype; **B** the band (voices, percussion, guitar).

## Window / face — the X, and two trading glows

The iconic stark white **X** on black (their logo / album art). Propose: the X in a field of
black, with the **two voices as two soft glows** (one each side) that **alternate left↔right
as they trade lines** — the call-and-response made visible — brightening on each sung phrase,
the reverb tail fading them into the dark. Minimal, monochrome, nocturnal.

## What this station adds (why it earns the slot)

- **First two-voice call-and-response brain** — two singers *conversing*, the structure being
  who-sings-when + the silence between (new; reusable).
- **Space-as-structure** — the most minimal, most restrained station; *ma* as form.
- A **dark, intimate, nocturnal** mood the dial doesn't have.
- An **artist station** (archetypes), kin to `air`/`wba` — pairs as the dark twin of WBA's warm restraint.

> **Phase-3 chassis hint** (not yet read): an artist station on the `radio.h` clock — copy the
> *wiring* from `air`/`wba` (the archetype roll), build the two-VOICE trade brain + the low-
> density space form, and voice the band from this brief.
