# I Hear a New World — the blind band brief

STATUS: READY TO BUILD — blind brief for the `newworld` instrument cart (a Joe Meek "Outer
Space Music Fantasy" machine). Design agreed 2026-07-07; cart not yet built. Play surface,
voices, effects and the loop-and-mangle method are locked; engine palette mapping is
Phase-2-confirm (verify each `INSTR_*`/effect against `runtime/studio.h` before wiring).

> Intent-first brief written from the record and its production techniques **before** reading
> any cousin cart (the firewall — [`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md)).
> Unusually for our briefs this is an **instrument you play**, not a self-playing radio station,
> so the "band" is really *one expressive gesture + a tape method*. Chassis to shop in Phase 2:
> the dub/tape family (`dub.c`, `dubdesk.c`, `tapeloop.c`, `loopstation.c`, `spacecho.c`) and the
> keybed/ribbon libraries (`keybed.h`, `solo.h`, `pointer.h`, `gestures.h`). Sibling tape/found-sound
> lineage. Sourcing + confidence notes at the foot of this doc.

## The record, from the music

*I Hear a New World: An Outer Space Music Fantasy* (Joe Meek & the Blue Men, recorded 1959–60
at Meek's flat, 20 Arundel Gardens, Holland Park; released as EPs in 1960, full LP 1991 on RPM).
A concept record about jolly moon-creatures — Globbots, Dribcots, Saroos — each track a little
scene Meek narrated in the sleeve notes. It is **not** an avant-garde abstraction: Meek said he
kept "the construction of the music down to earth" so it had "entertainment value." So the bones
are a **skiffle/rock band** (guitar, steel guitar, sax, bass, drums) playing simple, tuneful,
often childlike/hypnotic melodies — and the *magic* is entirely in the studio manipulation layered
on top. **The real instrument on this record is the tape machine.**

The defining facts:

- **The lead is a swoop, and it's a SLIDE — not a keyboard.** The eerie melodic-space voice is a
  **steel / Hawaiian (lap-slide) guitar glissando**, theremin-like. (⚠️ The clavioline so often
  attributed here is a *Telstar*-1962 back-projection — no keyboard is credited on this album. See
  sourcing note.) This is the single fact that shapes the whole play surface: the hero gesture is a
  continuous-pitch *slide*, not discrete keys.
- **Sped-up "little creature" voices** — sung/spoken vocals recorded then replayed faster (Pinky &
  Perky / Chipmunks style), and crucially **layered at *several different* speeds** ("various
  speeds," per the sources — not one fixed ratio; do not hardcode "+1 octave").
- **Everything is drenched** in Meek's **homemade spring reverb** (his guarded "black box," twangy)
  and **self-oscillating tape echo** (echo pushed into regeneration, used as an instrument).
- **Burbling water & found-object percussion** — bubbles blown through a straw into water, milk
  bottles struck with spoons (pitched, distorted), a sink draining, an electrical circuit shorted,
  and **reverse-tape swells** as seasoning. The kitchen-sink musique-concrète layer.
- **Extreme, pumping compression + deliberate preamp distortion** — limiters pinned into the red for
  breathing/pumping, preamps overdriven for "smooth, musical distortion." A hot, slightly-broken
  lo-fi floor under everything.
- **Mood arc:** calm/hopeful → whimsical/playful (the creature dances) → genuinely sinister ("Glob
  Waterfall"). Novelty brightness with an uncanny undertow. Tempos span slow trance to jaunty march.

## The instrument concept — "a lap-steel wired into a tape machine"

Not a synth workstation, not a general keyboard. ONE expressive gesture + ONE method:

- **Play a slow, eerie, swooping melodic line** on a scale-snapping glide surface (the slide).
- **Loop it and mangle it with tape** — build the hypnotic bed the way Meek built it: by layering
  tape, not by playing chords. The looper *is* the polyphony and the arrangement.

The focus discipline (the maker's own guardrail — "all/everything is not focused enough to be
great"): resist the keybed + ribbon + chord-buttons kitchen sink. Fuse them into one surface and
let the looper absorb the rest.

## The play surface (the "special keyboard")

A **scale-snapping glide ribbon + a swell control** — the lap-steel gesture, made playable:

- **Slide** along the ribbon for the continuous glissando swoop (the signature).
- **Snaps toward in-scale notes** so the simple tunes are playable in tune (this is what a naive
  freehand ribbon can't do, and it's how we honour the "down to earth, tuneful" melodies while
  keeping the swoop). The snap strength is a knob — full = keyed feel, zero = pure theremin.
- **Swell / intensity** (a second axis — vertical position, or a dedicated fader) for the ghostly
  fade-ins: the steel-guitar volume swell and the reverse-tape swell character.
- Monophonic (one slide hand), which is authentic — layering comes from the looper, not polyphony.

Rejected on purpose (and why): a separate chromatic **keybed** (pulls toward "general instrument,"
and the record has no keyboard); separate **chord buttons** (fight the mono-swoop soul — the looped
drone bed is the chords, done Meek's way). If real keys are ever wanted, make the ribbon *toggle*
between glide-mode and keyed-mode — same one surface, two feels — never both on screen at once.

## The voices (palette mapping — confirm in Phase 2 against `runtime/studio.h`)

A **few** voices on that one ribbon, switchable:

| imagined voice | likely engine (confirm Phase 2) | notes |
|---|---|---|
| **slide lead** (the steel-guitar swoop) | `INSTR_GUITAR` steel voicing, or `INSTR_REED`/buzzy `INSTR_SAW` for the reedy alt | the hero; mono, glide-native, drenched in reverb+echo |
| **creature voice** | `INSTR_VOICE` (formant) + `varispeed`/per-note pitch-up | with a **pitch-up amount** control so you can layer takes at *different* speeds (the "various speeds" trick) |
| **bubble blip** | short `INSTR_SINE`/noise burst → `grains` scatter → echo | the burbling-water melodic-ish texture |

And **found-sound one-shots** — the secret sauce that makes it Meek, not generic — as a small row
of tap-pads you can also loop in:

| one-shot | likely engine | source it evokes |
|---|---|---|
| **straw-bubble** | pitched noise burst + `grains` | bubbles through a straw into water |
| **milk-bottle clonk** | `INSTR_MEMBRANE`/`INSTR_MALLET`, detuned + distorted | bottles struck with spoons |
| **electric zap** | noise burst + `crush` + ring/`ringmod` | a shorted electrical circuit |
| **reverse swell** | `grains_pitch(reverse=1)` on the loop buffer | reverse-tape swells |

## The effects ARE the instrument (the tape machine)

Riding these is half the performance — set-and-hold, reconfigure only on change (the fx-frame rule):

- **`echo`** — self-oscillating tape echo; feedback pushed near/over 1.0 as a performance move.
- **`reverb`** — the twangy homemade spring; small→hall, on everything.
- **`varispeed`** — the tape wobble / speed-dive (sweep it; it glides — built for sweeps, not a held
  detune).
- **`grains` / `grains_freeze` / `grains_pitch`** — freeze a captured phrase, reverse it, pitch it:
  this is the **live self-resampling** that gives us a "tape/sampling" aesthetic with **no sampler**
  (external-sample playback stays out of scope — we mangle our *own* output). The key insight of the
  design.
- **`glue`** — the extreme pumping bus compression (crank the amount for the Meek squash/breathe).
- **`crush` / `tape(…, saturation)`** — the deliberate preamp overdrive / lo-fi grit.
- **`amp_noise(hiss, hum, mains_hz)`** — the valve-bedroom-studio grime floor (Arundel Gardens hum).

## The looper (co-star — the layering method)

A simple live looper is the arrangement engine, not a bonus:

- **Record** a phrase → it loops → **overdub** more takes on top (build the bed, then the lead).
- Loops feed the granular buffer, so **freeze / reverse / varispeed apply to the loop**, not just
  the live voice — that's how one player builds the whole layered soundscape.
- Authentic: Meek kept a library of tape loops "hanging from nails." The looper is that, playable.

## The window / face

A little **generative moon-scape**: a low lunar horizon under a black star-field, with a few
floating **Globbot/Dribcot creatures** that bob and blink to the music (amplitude-reactive), and
things floating "three feet above the crust" (his "Magnetic Field" note — a low-gravity drift on
sprites). The loop/tape state visualised as **spinning reels** or a scrolling tape. The record's
whimsy-with-an-uncanny-undertow made visible. Cold-opens playing a slow swooping self-play line
(ADR-0022 delightful-to-a-stranger); first touch hands over.

## What it adds (why it earns the slot)

- **The first "tape machine as instrument" cart** — proves the live-self-resampling seam
  (`varispeed` + `grains` + looper) is a whole aesthetic, dissolving the "we can't do sampling"
  bogeyman for a class of future carts (external-sample playback stays out; self-resampling is in).
- **A new play surface** — the scale-snapping glide ribbon + swell (the lap-steel gesture), reusable
  by any theremin/Ondes/slide-lead cart; candidate to graduate into a shared header.
- **A genuinely under-served sound-world** — musique-concrète pop / electronic exotica, unlike
  anything in the library, and one of the highest engine-fit records imaginable *because* its magic
  is processing, not sample playback.

## Sourcing & confidence (for history)

Grounded in a 2026-07-07 research pass (background agent, ~7 web tool-uses). The **technique
inventory** is well-corroborated; **per-track sound-to-object mappings** are mostly reviewer
interpretation. Flags worth carrying into the build:

- **HIGH:** varispeed layered vocals; the core found-sound set (milk-bottles+spoons, straw-bubbles,
  sink drain, electrical short, reversed tape); homemade spring reverb + self-oscillating tape echo +
  extreme compression + preamp distortion; the Blue Men skiffle lineup with prominent steel guitar;
  Arundel Gardens (not 304 Holloway Road — that's the Telstar era); Meek's "kept it down to earth"
  quote.
- **⚠️ MYTHS — do not encode as fact:** **clavioline on this album** (back-projected from *Telstar*;
  no keyboard credited — use steel-guitar-swoop, not clavioline); any **specific vocal speed ratio**
  ("half-speed / +1 octave" is the *Chipmunks'* documented figure, not Meek's — sources only ever say
  "various speeds"); Dave Adams singing the aliens (his roles are Telstar-era); wet-finger-on-glass-rims,
  radiator-banging, comb-and-paper (Telstar lore / not found for this record).
- **Best un-mined primary sources** if the flagged items ever need resolving: **Barry Cleveland,
  *Creative Music Production: Joe Meek's Bold Techniques*** (the definitive technical reference, with a
  track-by-track IHANW chapter — not retrievable online in the research pass); the **RPM/Cherry Red
  (1991) reissue liner notes**; and **joemeekpage.info** (Dave Adams's account).
