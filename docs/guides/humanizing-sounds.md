# Humanizing sounds: making an instrument sound PLAYED

*Search words: humanize, humanise, human feel, groove, sounds robotic / mechanical / like a keyboard /
like a sequencer / synthetic, make it alive / realistic / expressive, expression, articulation,
analog warmth, voice cards, drift, ride the knobs, filter sweep, mod wheel, vibrato, p-lock.*

> The how-to. The why, every measurement and the open engine needs live in
> [`design/humanized-playing.md`](../design/humanized-playing.md); this page is what to DO when a cart's
> instrument sounds like a keyboard, a sequencer or a cheap synth. Built from the humanized-playing spike
> carts (2026-11-04); each one is a working template.

## 1. The idea in one paragraph

A sequenced note is pitch, velocity and length, and every note of an instrument sounds the same. A
player never plays the same note twice, and an old analog machine never makes the same sound twice. So
humanizing has two halves: **the player** (a model of the body with slow, correlated state that turns
into per-note values: a hand, lungs, a bow arm, two hands on a tabla, four limbs on a kit) and **the
instrument** (its own imperfections and its real sound). The player model always lives in the CART;
the engine only has to carry what it decided. Never use fresh random per note on its own: drift (a
`noise()` over time) plus a little scatter is what reads as human.

And the part that is easy to forget: **a player's other hand is on the KNOBS.** Moving the sound while
it plays (a filter swept by hand, the mod wheel, a swell pedal, breath pressure) is what turns a
correct part into MUSIC. It has its own section, §4; do not skip it.

## 2. The engine calls made for this

| call | what it does | when |
|---|---|---|
| `instrument_vibrato(slot, amount)` | PIPE / REED / BOWED / BRASS give every note a built-in vibrato from the first sample. 0 = off, 1 = as is (default), 2 = deeper. Read live. | Any wind or bowed part you humanize: set 0 and play your own `note_lfo(h, 0, LFO_PITCH, rate, depth)` that blooms late. |
| `instrument_analog(slot, amount)` | Analog warmth: 8 "voice cards" in rotation (own tuning, cutoff, envelope speed, level) + slow drift. 0 = off (default), 1 = a real analog synth, 2 = tired. | Any synth pad, chord or poly part. One call. Nikki: "much more beautiful". |
| `instrument_release_shape(slot, shape)` | how notes fade after the key is up: `RELEASE_LINEAR` (default) / `RELEASE_EXP` (a natural curve) / `RELEASE_DAMPER` (a piano's felt: a quick drop, a short tail). | Pianos, plucks, anything whose release sounded like it was cut off. |
| `NoteOpts` per-note fields | on a BOOKED note: `.cents` (its pitch, ±1200), `.cutoff_oct` (its filter), `.damp` 0..1 (how fast its ring dies; 1 = a closed stroke), `.release_ms`, `.region_start/.region_end` (its sample slice, fixed at note-on). All 0 = as the slot is. | Per-note micro-variation without one slot per string / per stroke type. |
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
| a mouth (formant) on a synth bass + lead | `mouthhand` | a moving vowel: harder = more open, a "wah" at each attack, drift, a talk-box vowel line (a small ingredient; see §5) |
| analog synth | `synthhand` | a hand on the cutoff knob (springy), late mod-wheel vibrato, bends, chord spread; voice cards (now `instrument_analog`), drift, power sag |

Shared shape of every spike: keys **1-5** switch the layers, **TAB** flips to the plain version, UP/DOWN
sets how much humanity, R gives a new take. Copy that: it is how the ear decides what matters.

## 4. Ride the knobs: the part that makes it music

On every instrument a player is moving the SOUND while playing, not only choosing notes: the synth
player sweeps the cutoff through a phrase and pushes the mod wheel in on a long note, the organist rides
the swell pedal, the guitarist rocks a wah, a wind player's breath IS a continuous control. A part
with perfect notes and a frozen sound still sounds sequenced. In `synthhand` the hand on the cutoff
moved the brightness between ~390 and ~1380 Hz through each phrase while the plain version sat flat
at ~700 Hz, and it is one of the layers that made Nikki call it "much more beautiful".

**What to ride, by instrument:**

| instrument | the knob | ride it with |
|---|---|---|
| synth pad / lead | filter cutoff (+ a little resonance), mod-wheel vibrato, pitch bend | `note_cutoff`, `note_res`, `note_lfo` depth, `note_pitch` on held notes |
| organ | the swell (expression) pedal, Leslie speed | `note_vol` on the held chord, `leslie()` speed on a change |
| guitar | wah pedal, volume swells | a band filter cutoff ridden like a pedal, `note_vol` |
| winds, bowed | breath / bow pressure and speed | `note_vol` + `note_timbre` / `note_morph` (see `breathflute`, `cellohand`) |
| the whole mix | a DJ-style filter sweep, a tape slow-down | `filter()` and `varispeed()` (made to be ridden) |

**Move it like a hand, not like an LFO.** A hand aims at a target that changes with the phrase, gets
there with a bit of momentum and overshoots a little; an LFO repeats. The pattern from `synthhand`:

```c
// target: where the hand wants the knob (phrase shape + slow wandering)
float phase = fmodf(t_now / phrase_len, 1.0f);
knob_target = lo + (hi - lo) * smooth01(sinf(3.14159f * phase)) + wander * (noise(t_now * 0.4f) * 2 - 1);
// the hand: a damped spring, so it lags, swings and settles
knob_vel += (knob_target - knob_cut) * 6.0f * dt() - knob_vel * 3.2f * dt();
knob_cut += knob_vel * dt();
for (each held note h) note_cutoff(h, (int)knob_cut);
```

Shape the target from the MUSIC: open toward the top of a phrase or a build, close for a breakdown,
bring vibrato in only on long notes and only after they have settled (0.3-0.5 s).

**The one trap: effects are set-and-hold.** `note_*`, `filter()`, `varispeed()`, `instrument_level()`
and `instrument_tune()` are built to be ridden every frame. The effect calls (`crush`, `tape`, `eq`,
`chorus`, `reverb`, `echo` and their `instrument_*` twins) rebuild their DSP when called, so calling
them every frame STUTTERS (CLAUDE.md, `tools/lint-fx-frame.js`): change those only when the value
actually changes.

**Booked knobs and pedals: `lane_at`.** A knob or pedal is a curve SHARED by every sounding note; book
it on the audio clock like the notes: `lane_at(t, slot, LANE_CUTOFF / LANE_LEVEL / LANE_VIB /
LANE_DAMPER, value, ramp_ms)`. Ramped in the engine (no frame stepping), followed by held and booked
notes alike. `LANE_DAMPER` is a real sustain pedal: released notes ring on while it is down, a half
pedal slows their release (`pianohand` L).

**Events inside a booked note: `note_event_at`.** `schedule_note_at` returns a booking id; hand it to
`note_event_at(id, t, NEV_REARTIC / NEV_DAMP / NEV_RELEASE, amount)` to make something happen INSIDE the
note later: a bow change on a bowed note (the bow slows, stops and turns; amount = how slowly), the onset
again on the winds and organ, a hand muting it from t, the key coming up at t (`cellohand` B).

## 5. Techniques that work today

- **Booked vs held.** Book notes (`schedule_note_at`, ~0.12 s lookahead on `audio_time()`) when timing
  must be tight (drums, plucks, piano): per-note gain + macro offsets at the onset. Hold notes (`note_on`
  + `note_pitch/vol/timbre/morph/lfo` every frame) when the note changes WHILE it sounds (winds, bow):
  you lose ~17-40 ms of timing precision.
- **A slide INTO a note, booked:** a pluck under the note, then a legato booked note with `glide_ms` on
  a mono slot (`instrument_mono`). A waveguide string bends UP cleanly, never below where it started.
- **Per-note cents on a booked note:** `NoteOpts.cents` (strumhand). The old trick, retuning a slot just
  before booking, needed one slot per string and bent the tail of the note before.
- **Live per-slot rides** (one instrument on its own slot): `instrument_tune` (pitch) and
  `instrument_level` (damping) reach sounding voices; slot macros do NOT (read at note-on only).
- **Open vs closed strokes:** `NoteOpts.damp` (1 = stopped at once) on the same slot; or, for strokes
  that also SOUND different, two slots + `instrument_choke` (tablahand, drumhand).
- **Curves on a booked note (shapes):** define a gesture once with `shape_define` (a scoop, a swell, a
  fade, a puff of breath, a filter closing, a bayan drop), then reference up to three per note through
  `NoteOpts.shape[]`, scaled (how deep) and stretched (how slow) by the player model. Computed in the
  engine, so no per-frame stepping and no detuning from riding macros (`breathflute` B).
- **A late vibrato:** `instrument_vibrato(slot, 0)`, then either on a BOOKED note `NoteOpts.vib`
  (`{ .cents, .rate_hz, .delay_ms, .fade_ms, .wander }`: straight, then blooming; `breathflute` B) or on a
  held note `note_lfo` depth ramped from 0 after 0.15-0.4 s. Only on notes long enough to have one, rate
  wandering 5-6 Hz.
- **Brightness follows force.** The one rule that holds for every instrument: a harder note is a
  BRIGHTER note (strings, hammers, breath, lips all behave this way, and it is exactly what a funk
  touch-wah does). Tie each note's cutoff / timbre offset to its force (booked: `NoteOpts.timbre`; held:
  `note_cutoff` / `note_timbre`), or put `instrument_wah(slot, sensitivity, resonance, mix)` on it for
  the full Mu-Tron "quack" that follows how hard you play.
- **A moving formant is a small ingredient, not a headline.** `instrument_formant` ridden slowly (smooth
  the vowel yourself, re-send only on a change, trim the level: open vowels add gain) gives a part a
  gentle "mouth" (`mouthhand`); it sounds better, it does not wow on its own.
- **For an instrument that TALKS** (talk box, robot voice), do not fake it with the formant: the wow is
  WORDS, and consonants make them. Use the vocoder with the voice engine speaking syllables as its
  modulator (`vocode`; `INSTR_VOICE` with `voice_consonant` / `voice_coda`, see `say`, `vox`), a bright
  open saw as the carrier, all of it through the vocoder, exaggerated vowels and pitch bends into the
  words. How real players do it: Roger Troutman on a Minimoog through an Electro-Harmonix Golden Throat
  ([EHX](https://www.ehx.com/blog/ehx-vault-golden-throat-roger-troutman/)); talk box technique, vowels
  first then consonants ([Disc Makers](https://blog.discmakers.com/2017/09/meet-the-talk-box/),
  [Dunlop](https://www.jimdunlop.com/content/manuals/MRT01.pdf)); the sawtooth source
  ([gfworks](https://gfworks.jp/talkbox/how_to_play.html)); Bootsy's Mu-Tron III envelope filter
  ([Guitar World](https://www.guitarworld.com/features/the-mu-tron-iii-without-it-there-aint-no-bootsy)).
- **Riding a waveguide's macros can detune it.** On `INSTR_PIPE` harmonics 0.15 pulls a sounding note
  ~10-13 cents flat (breath air a few cents): a held flute ridden for brightness drifted out of tune
  where the booked one stayed within ~2 cents. Prefer booked notes; when riding live, keep harmonics
  still or correct with `note_pitch`.
- **One physical object = one voice.** A cymbal, a string, a drum head rings ONCE: a new stroke
  replaces the old ring, it does not stack on it. Give it a mono slot (`instrument_mono`). A ride whose
  strokes each rang 1.3 s on its own voice turned eighth notes into a low drone ("a train locomotive").
- **A wooden attack:** a short low damped `INSTR_MEMBRANE` hit under each pluck, its gain following force
  squared and its spot / length varying (basshand). A bright "snap" layer went the wrong way.
- **The master EQ as a volume knob:** there is no master volume; `eq(db, db, db)` works when the cart
  is quiet (set once, never per frame).

## 6. Making the SOUND right: measure against a recording

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

Tools for it (dreamengine-f4, 2026-11-04): `node tools/fetch-refs.js` rebuilds a reference set from a
committed manifest (`tools/refs/*.txt`, e.g. the drum kit's), so `build/refs/` is reproducible on any
machine; `node tools/oneshot-compare.js` measures one-shot hits against their references from a spec
(`tools/oneshot-specs/*.json`), e.g. `--render drumhand --define PROBE_TECH,SYNTH_KIT --spec
tools/oneshot-specs/drumhand.json --rel kick` (each hit vs the real kit's balance to the kick, ~10 s).

Pitfalls that cost time: `play.js --wav` renders are STEREO (downmix before analysis, or every
frequency reads an octave low); very low notes fool simple pitch trackers (treat noisy tracks as
unknown, not as a finding); `click-check` flags every attack that starts from silence, so compare
against the plain version before blaming a change; match loudness before an A/B, quieter always loses.

Known engine sound gaps (the full checklist with status: [`design/humanized-playing.md`](../design/humanized-playing.md) §7.2): `INSTR_MEMBRANE` tilts all modes together and its
2nd mode is too weak for kit drums; the ADSR decay and release are LINEAR (real decays are curves);
the piano's morph "pedal" is latched at note-on; `note_retrig` does nothing on BOWED; bowed pressure
barely changes the colour; the piano treble decays too fast; `INSTR_METAL` cymbals want a LOWPASS.

## 7. Recorded one-shots, when synthesis is not the point

Realistic cymbals are the hardest thing to synthesize. `drumhand` loads CC0 one-shots at runtime with
`fopen` + `sample_load` into `INSTR_SAMPLE` slots (the `breakchop` route), with a synth fallback when
the files are missing. The binary runs with cwd = `build/`, so the files live in `build/refs/...`; a web
build falls back to synthesis. For a shipped product (Playtime) ship them as assets.

## 8. What the engine still cannot do

A BOOKED note can only be shaped at its onset: no pitch curve, no late vibrato, no swell, no bow change,
no shared pedal curve, no per-note release, filter or damping. The full needs list (with every
instrument's rows and the voice from Playtime's choir) is
[`design/humanized-playing.md`](../design/humanized-playing.md) §6; building it is on hold until one
shape fits all. Until then: hold the note and ride it, or ride the slot.
