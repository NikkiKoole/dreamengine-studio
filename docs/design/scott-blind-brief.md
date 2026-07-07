# Raymond Scott's Circle Machine — the blind band brief

STATUS: BUILT 2026-07-07 — shipped as `tools/carts/circlemachine.c` / `circlemachine.cart.png`. The
rotating-photocell ring, bulb-brightness-as-note, the three voices (RING/GLIDE/BONGO), NUDGE, and the
ringmod/reverb/echo processing all landed. **Two departures from the brief below, both to serve the
record over the guardrail** — see "shipped as" note. This brief remains the design of record; the
cart is the source of truth for what shipped.

> **Shipped as — two changes from the intent-first brief (2026-07-07):**
> 1. **TWO concentric rings, not one.** The focus guardrail below argued for a single ring. Listening
>    back, the records aren't one clean voice — the hypnotic, never-repeating density comes from
>    Scott's *multi-voice* machines (the Electronium's 12 channels, the room-sized Wall of Sound). So
>    the cart runs two rings with **different step counts under one arm** — they drift against each
>    other (polymeter, the `euclid.c` trick), which *is* that texture. This deepens the one idea (a
>    ring you configure and let cycle) rather than bolting on a panel, so it keeps faith with the
>    discipline while being far more Scott.
> 2. **A DIRT layer.** The first pass was too clean (ringmod/reverb/echo only). The records *swim* in
>    tape — so DIRT drives `tape` (wow/flutter/saturation) + `chorus` (wobble) + `amp_noise` (valve
>    grime) + `crush` (cranked), defaulted high. The brief's optional "chorus/tape wobble + amp_noise
>    grime" (§effects) became a first-class control.
>
> **Build gotcha for the next music cart:** `reverb()`/`echo()` are **SENDS**, not inserts — every
> instrument slot defaults to send = 0 (dry), so configuring the tank does nothing audible on its own.
> Route each voice with `instrument_reverb(slot, send)` / `instrument_echo(slot, send)` (the knobs
> drive the *sends*; `reverb()`/`echo()` just set the space once). This bit the first build ("reverb
> and echo aren't doing much"). (`ringmod`/`tape`/`chorus`/`crush` are master-wide and need no send.)

> Intent-first brief written from Scott's machines and his documented technique **before** reading
> any cousin cart (the firewall — [`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md)).
> Like the Joe Meek brief ([`newworld-blind-brief.md`](newworld-blind-brief.md)), this is an unusual
> one: the faithful Scott instrument is **not a keyboard you play** — it's a machine you *configure
> and let cycle*. So the "band" is one generative gesture (a rotating photocell over a ring of tone
> generators) plus the timbre it drives. Chassis to shop in Phase 2: the step-sequencer / groovebox
> family (`drummachine.c`, `groovebox.c`, `modrack.c`, anything using `euclid()`), the radio PRNG /
> step-clock brains (`radio.h`), and — if the glide-lead voice mode is kept — the newworld glide
> ribbon and `note_glide` (the tb303 cart). Sourcing + confidence notes at the foot.

## The music, from the machines

Raymond Scott (1908–1994) — the swing-bandleader-turned-inventor whose **electronic** work
(*Manhattan Research Inc.*, ~1953–69; the *Soothing Sounds for Baby* trilogy, 1962–64) is the
subject here, NOT the 1930s Quintette. The three tracks that seeded this brief:

- **"Bandito the Bongo Artist"** — not merely a title, it *is* one of his machines: an early
  electronic **drum machine**. Scott's own words: *"a device that automatically creates and performs
  bongo-like drum improvisations — an infinite variety of pitches, rhythms and colors."* The track is
  the machine demonstrating itself. Auto-generated, pitched electronic bongo hits in shifting
  rhythms — **generative percussion, never a hand-drummed part.**
- **"Portofino 1" / "Portofino 2"** (both ~1962, ~2:13/2:14) — two variations of the same tune, and
  Scott's most *hummable* electronic piece (it later sustained a whole tribute LP, *The Portofino
  Variations*, and turned up in *Narcos*/Gucci). This is the **tuneful, melodic** end of the palette —
  a clear lead line over a steady cycling pattern — as opposed to the abstract bleep-experiments.

The defining facts (the machines are HIGH-confidence; per-track sonics are not — see sourcing):

- **Scott's whole aesthetic is *configure-and-let-it-cycle*, not note-by-note performance.** His
  instruments generate rhythm by cycling/stepping automatically. Repetition and looping are the
  aesthetic core.
- **The Circle Machine (~1959) is a step sequencer a decade before the word.** A **ring of
  incandescent lamps**, each with its own **brightness rheostat**, swept by a **photocell on a
  rotating spindle**. As the photocell passes a lamp, that lamp's brightness sets the **pitch** of a
  tone; the spindle's **rotation speed** sets the **tempo**. You dial a loop of pitches into the ring
  and it plays them round and round.
- **The Clavivox (1956) is a keyboard theremin** — continuous **portamento glide** between notes,
  glide *rate keyed to key velocity*, plus vibrato; left-hand control keys shape attack/envelope
  while the right hand plays melody. The great-grandfather of the tb303 cart's `note_glide`.
- **The Electronium (1959–70s) is a guided-generative "duet"** — *no keyboard*, ~100 switches / ~150
  knobs; you set parameters and *ask the machine to suggest* a motif, then steer. A 12-channel
  sequencer, each channel its own tone generator with per-voice **accent, tremolo, reverb**.
- **His DSP primitives** (sold through Manhattan Research Inc.): **ring modulators**, envelope
  shapers, filters, variable-waveshape tone generators, chromatic electronic drum generators.
- **Tape:** a 1959 auto-locate **loop** device ("finds a selection and repeats it as long as he
  wants" — proto-sampling); and the "dead-mic" reverb trick (mic switched on *after* the strike, for
  a sense of great space).

## The instrument concept — "a rotating scanner over a ring of tone generators"

Not a keyboard, not a synth workstation. ONE generative gesture, made playable and *visible*:

- A **ring of steps** around a circle, each with a **pitch** (and a per-step level/accent) you set by
  dragging its "bulb" brighter/dimmer.
- A **rotating playhead** (the photocell arm) sweeps the ring; where it passes a bright step, that
  step sounds. **Rotation speed = tempo.** No left-to-right timeline — the loop is a *dial you spin*.
- You perform by **reaching into the running loop**: drag bulbs to reshape the melody live, change
  the spin speed, flip voices — the Scott "man-machine duet" made tactile. It plays itself the moment
  it loads (ADR-0022 delightful-to-a-stranger); first touch reshapes it.

The focus discipline (the maker's guardrail — "everything is not focused enough to be great"): resist
bolting a full keybed + a drum grid + a mixer onto this. The ring IS the sequencer, the melody, and
the arrangement. Extra voices are *modes of the same ring*, not extra panels.

## The play surface (the "special keyboard")

**A circular step ring + a spin control** — the Circle Machine gesture:

- **N steps around a ring** (period-appropriate to keep small — think 8/12/16). Each is a bulb whose
  **brightness = level** and whose **radial/tuned position = pitch** (snap to a scale so the tunes are
  playable — the same scale-snap honesty as the newworld ribbon).
- **Drag a bulb** to set its pitch/level; the running playhead makes the change audible next pass.
- **Spin speed** knob = tempo (the spindle). A second knob for **swing/rotation jitter** gives the
  "improvised" feel of Bandito without true randomness fighting the loop.
- `euclid()` would feel exactly period-appropriate for seeding a starting pattern around the ring.

Rejected on purpose (and why): a **chromatic keybed** (Scott's faithful machine has no keyboard — the
Clavivox is a *separate* archetype, see voice modes); a **left-to-right step grid** (that's every
other sequencer — the *circle* is the whole visual identity and the STATUS #21 reason-to-build).

## The voices (palette mapping — confirm in Phase 2 against `runtime/studio.h`)

A **few** timbres the ring can drive, switchable — plain oscillators are correct (Scott's tone
generators were simple waveshapes; the *pattern* and the *processing* are the character):

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **ring tone** (the stepped bleep — the default) | `INSTR_SINE` / `INSTR_SQUARE` / `INSTR_TRI` | one plain osc per step; clean, so the sequence and ring-mod do the talking |
| **glide lead** (Clavivox mode) | `INSTR_SINE`/narrow `INSTR_PD` + `note_glide` + vibrato LFO | portamento between consecutive lit steps; the *played* Scott, as an alternate ring mode |
| **auto-bongo** (Bandito mode) | `INSTR_MEMBRANE` (pitched) | the ring drives an electronic drum instead of a pitch — per-step pitch/accent = the "improvising" bongo |

Per-step **accent / tremolo / reverb** (the Electronium's per-voice shaping) rather than one global
setting: a bright bulb hits harder, a flagged step gets more wobble or more tail. That per-step
modulation matrix is the single most *Scott* detail after the ring itself.

## The effects (his processing chain — set-and-hold, reconfigure only on change)

Riding these is half the sound (the fx-frame rule — reconfigure only when a value changes):

- **`ringmod`** — the signature inharmonic **clang/bell/bleep**. Scott's ring modulators are the
  source of the "electronic abstraction" timbres; a mix knob takes the ring tone from clean to metallic.
- **`reverb`** — the "great space" (his dead-mic ambience trick); small room → hall on the whole ring.
- **`echo`** — tape delay / the auto-locate loop feel; feedback as a performance move.
- **vibrato / tremolo** — amplitude + pitch LFOs, per-voice (the Clavivox vibrato, the Electronium
  tremolo).
- **`note_glide`** — portamento for the Clavivox voice mode (glide rate as a knob = the velocity-keyed
  glide of the real instrument).
- **`chorus`/`tape`** — gentle wow/wobble for the vintage-valve character (optional grime floor via
  `amp_noise`).

## The generative engine (co-star — the "duet")

The ring is a **generative loop you steer**, not a passive pattern:

- Loads mid-cycle already playing a seeded pattern (`euclid()` or a scale walk).
- A **"nudge"** affordance (Electronium spirit): a button that *mutates* the ring a little — nudges a
  few pitches toward the scale, shifts accents — so the player curates variations rather than
  authoring every step. Faithful to "ask the machine to suggest, then steer."
- Speed, jitter, voice, and the nudge are the whole control set. Deterministic from a seed (so a good
  loop is a committable clip, per the debug-harness clip convention).

## The window / face

The machine **is** the visual — and it's the reason this earns a slot: **a glowing ring of bulbs
with a sweeping photocell arm**, unlike any other music cart in the studio (STATUS #21's exact
argument). Bulbs pulse to their level; the arm sweeps at tempo; the lit step flares as it sounds. A
warm, basement-workshop palette (amber bulbs on dark). Amplitude-reactive glow ties the picture to
the sound. Cold-opens spinning a slow, tuneful self-play loop; first touch hands over.

## What it adds (why it earns the slot)

- **The first *circular* sequencer** — a rotating-playhead ring instead of a left-to-right timeline;
  a genuinely new play surface, reusable by any cyclic-pattern cart, and visually unlike everything we
  have (the STATUS #21 case, made concrete).
- **The pre-Roland wing's flagship** — opens the Raymond Scott / Manhattan Research Inc. lineage
  (Clavivox and Electronium become follow-on voice modes / carts that reuse this chassis), closing the
  "museum of pre-Roland machines" thread in [`cart-library-direction.md`](cart-library-direction.md).
- **A generative-you-steer model** — the "nudge a loop toward better" interaction (Electronium
  spirit) is new to the library and pairs naturally with the existing radio/improv brains.

## Sourcing & confidence (for history)

Grounded in a 2026-07-07 research pass (subagent, ~18 web tool-uses). The **machines and techniques**
are well-corroborated; **per-track sonic detail is not published** — treat it as inference.

- **HIGH:** the Circle Machine mechanism (ring of rheostat-controlled lamps + rotating photocell;
  brightness = pitch, spin = tempo); the Clavivox as a velocity-keyed portamento keyboard-theremin
  with left-hand envelope keys; Bandito as an auto-improvising electronic **drum machine** (Scott's own
  description); the Electronium as a *knob/switch-driven*, no-keyboard, 12-channel generative sequencer
  with per-voice accent/tremolo/reverb; ring modulators / tone generators / filters as his DSP
  primitives; MRI track positions/lengths and the ~1962 Portofino dates.
- **LOW — do NOT encode as fact:** the instrumentation/tempo/structure of Bandito, Portofino 1, or
  Portofino 2 specifically (no citable bar-by-bar source exists — the mappings above are archetype
  inference, not documentation); the Electronium's exact internal generative algorithm (Scott was
  secretive; the one unit is non-functional).
- **⚠️ MYTHS — do not encode:** the Electronium was keyboard-played (FALSE — knobs/switches only; the
  DX-7 MIDI hookup was a one-off 1980s add-on); the Electronium was an autonomous "AI composer" (it
  *suggests*, the human curates — a duet, not autonomy); Bob Moog *built* the Clavivox or Electronium
  (he built a Clavivox **sub-assembly** and later modernized the Electronium's sequencer — not the
  instruments); *Soothing Sounds for Baby* is soothing (many find it eerie/minimal, not lullaby).
  Bandito's date is itself contested (~1959 per Wikipedia's tracklist vs 1963 per Ted Gioia).
- **Best primary sources** if the flagged items need resolving: **Chusid & Winner, "Circle Machines
  and Sequencers: The Untold History of Raymond Scott's Electronica"** (raymondscott.net/em-article-2001)
  — the definitive technical account, quotes Bob Moog directly; the **Basta *Manhattan Research Inc.*
  liner notes** (Chusid & Winner) — primary track documentation, not retrievable online in the pass;
  **raymondscott.net** MRI + Soothing Sounds pages; **120years.net** (Clavivox); **synthforbreakfast.nl**
  (the Electronium 12-channel / per-voice detail).
