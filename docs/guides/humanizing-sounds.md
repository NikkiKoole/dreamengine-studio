# Humanizing sounds: making an instrument sound PLAYED

> The how-to. The why, every measurement and the open engine needs live in
> [`design/humanized-playing.md`](../design/humanized-playing.md); this page is what to DO when a cart's
> instrument sounds like a keyboard, a sequencer or a cheap synth. Built from ten spike carts
> (2026-11-04); each one is a working template.

## 1. The idea in one paragraph

A sequenced note is pitch, velocity and length, and every note of an instrument sounds the same. A
player never plays the same note twice, and an old analog machine never makes the same sound twice. So
humanizing has two halves: **the player** (a model of the body with slow, correlated state that turns
into per-note values: a hand, lungs, a bow arm, two hands on a tabla, four limbs on a kit) and **the
instrument** (its own imperfections and its real sound). The player model always lives in the CART;
the engine only has to carry what it decided. Never use fresh random per note on its own: drift (a
`noise()` over time) plus a little scatter is what reads as human.

## 2. The engine calls made for this

| call | what it does | when |
|---|---|---|
| `instrument_vibrato(slot, amount)` | PIPE / REED / BOWED / BRASS give every note a built-in vibrato from the first sample. 0 = off, 1 = as is (default), 2 = deeper. Read live. | Any wind or bowed part you humanize: set 0 and play your own `note_lfo(h, 0, LFO_PITCH, rate, depth)` that blooms late. |
| `instrument_analog(slot, amount)` | Analog warmth: 8 "voice cards" in rotation (own tuning, cutoff, envelope speed, level) + slow drift. 0 = off (default), 1 = a real analog synth, 2 = tired. | Any synth pad, chord or poly part. One call. Nikki: "much more beautiful". |
| `instrument_mode(slot, MODE_BOW_PLUCK, 1)` | INSTR_BOWED pizzicato plucked by a FINGER (strong fundamental) instead of noise (bright, thin). | Upright bass, cello pizz: "a bass instead of a low piano". |

All three default to the old behaviour, bit-identical.

## 3. Recipes by instrument (each is a template cart)

| instrument | cart | what makes it human |
|---|---|---|
| guitar strum | `strumhand` | six mono string slots; per string: timing (the pick accelerates), force, brightness, mute, cents (finger pressure, a weak finger per chord grip); chord-change lift + squeak + damping |
| flute | `breathflute` | held notes ridden every frame from an air supply; scoop into pitch, breath-sharp, late vibrato, messa di voce, slur vs tongue, a deeper breath before a longer phrase |
| tenor sax | `reedhand` | the flute's lungs + a jazz vocabulary: ghost notes, growl (morph up after the attack), scoops, falls, a wandering laid-back swing |
| arco cello | `cellohand` | a bow arm (position, direction, planned speed, mid-note bow changes), quick subdued shifts (no glide within a position), late vibrato |
| walking upright | `basshand` | booked plucks with force / spot / length, open strings ring, slides as booked legato glides, ghosts and rakes, a varying wooden thump, a small room |
| piano | `pianohand` | a pedal the cart computes (legato re-pedalling, half pedal), melody voiced over the accompaniment and a little ahead, chords rolled top note first |
| tabla | `tablahand` (playing), `tablalab` (stroke sound) | bols as data; two hands; the bayan wrist (press / wail / dip / meend) and palm damping ridden live; strokes as measured overtone recipes |
| drum kit | `drumhand` | a drummer with limbs (the hat hand leaves for fills), a technique per stroke (centre / edge / rimshot / cross-stick / ghost), the hi-hat pedal, recorded CC0 cymbals (K) |
| analog synth | `synthhand` | a hand on the cutoff knob (springy), late mod-wheel vibrato, bends, chord spread; voice cards (now `instrument_analog`), drift, power sag |

Shared shape of every spike: keys **1-5** switch the layers, **TAB** flips to the plain version, UP/DOWN
sets how much humanity, R gives a new take. Copy that: it is how the ear decides what matters.

## 4. Techniques that work today

- **Booked vs held.** Book notes (`schedule_note_at`, ~0.12 s lookahead on `audio_time()`) when timing
  must be tight (drums, plucks, piano): per-note gain + macro offsets at the onset. Hold notes (`note_on`
  + `note_pitch/vol/timbre/morph/lfo` every frame) when the note changes WHILE it sounds (winds, bow):
  you lose ~17-40 ms of timing precision.
- **A slide INTO a note, booked:** a pluck under the note, then a legato booked note with `glide_ms` on
  a mono slot (`instrument_mono`). A waveguide string bends UP cleanly, never below where it started.
- **Per-note cents on a booked note:** give each string / voice its own slot and set `instrument_tune`
  just before booking (strumhand).
- **Live per-slot rides** (one instrument on its own slot): `instrument_tune` (pitch) and
  `instrument_level` (damping) reach sounding voices; slot macros do NOT (read at note-on only).
- **Open vs closed strokes:** two slots per drum (long and short release) + `instrument_choke`.
- **A late vibrato:** `instrument_vibrato(slot, 0)`, then `note_lfo` depth ramped from 0 after
  0.15-0.4 s, only on notes long enough to have one, rate wandering 5-6 Hz.
- **A wooden attack:** a short low damped `INSTR_MEMBRANE` hit under each pluck, its gain following force
  squared and its spot / length varying (basshand). A bright "snap" layer went the wrong way.
- **The master EQ as a volume knob:** there is no master volume; `eq(db, db, db)` works when the cart
  is quiet (set once, never per frame).

## 5. Making the SOUND right: measure against a recording

Humanized playing on a wrong sound still sounds wrong (the tabla and the upright both proved it).
The method that worked:

1. **Find CC0 single notes on Freesound.** Search with the license filter, then CHECK EACH SOUND PAGE
   (packs mix CC0, CC BY, Sampling+). The public HQ preview MP3 URL is in the page HTML, no login.
   Convert to WAV with ffmpeg and keep them in the gitignored `build/refs/<name>/` with a
   `SOURCES.txt` (id, author, license, URL). Measurement only unless CC0 and you mean to ship.
2. **Measure the same things on the recording and on a one-note probe of ours** (a `DE_DEFINES=PROBE_*`
   block in the cart): harmonic levels at the attack and later, per-harmonic decay (t60), pitch over
   the first 300 ms, energy between the harmonics, spectral centroid, the level envelope.
3. **Fix the biggest difference first,** measure again, then render for the ear.

Pitfalls that cost time: `play.js --wav` renders are STEREO (downmix before analysis, or every
frequency reads an octave low); very low notes fool simple pitch trackers (treat noisy tracks as
unknown, not as a finding); `click-check` flags every attack that starts from silence, so compare
against the plain version before blaming a change; match loudness before an A/B, quieter always loses.

Known engine sound gaps (open, see the design doc): `INSTR_MEMBRANE` tilts all modes together and its
2nd mode is too weak for kit drums; the ADSR decay and release are LINEAR (real decays are curves);
the piano's morph "pedal" is latched at note-on; `note_retrig` does nothing on BOWED; bowed pressure
barely changes the colour; the piano treble decays too fast; `INSTR_METAL` cymbals want a LOWPASS.

## 6. Recorded one-shots, when synthesis is not the point

Realistic cymbals are the hardest thing to synthesize. `drumhand` loads CC0 one-shots at runtime with
`fopen` + `sample_load` into `INSTR_SAMPLE` slots (the `breakchop` route), with a synth fallback when
the files are missing. The binary runs with cwd = `build/`, so the files live in `build/refs/...`; a web
build falls back to synthesis. For a shipped product (Playtime) ship them as assets.

## 7. What the engine still cannot do

A BOOKED note can only be shaped at its onset: no pitch curve, no late vibrato, no swell, no bow change,
no shared pedal curve, no per-note release, filter or damping. The full needs list (with every
instrument's rows and the voice from Playtime's choir) is
[`design/humanized-playing.md`](../design/humanized-playing.md) §6; building it is on hold until one
shape fits all. Until then: hold the note and ride it, or ride the slot.
