# Instrument & sound-toy carts — the shelf

A browsable index of every cart whose point is **making sound** — the synths, the
classic-machine homages, the expressive solo instruments, the engine showcases, the
radio stations, and the sound-making toys. Two jobs:

1. **Find one to play** — what each cart is and roughly how you drive it.
2. **Find one to copy** — when you build a *new* instrument, this tells you what
   already exists and which existing cart is its closest relative, so you start from a
   working model instead of a blank file.

Exact controls live in each cart's `editor/public/carts/index.json` entry (the
`description` ends with its key map) — that's the source of truth; this doc stays at
the "what is it / what's it like" altitude so it doesn't drift.

> **Looking for a specific sound/preset to grab?** [`instrument-recipes.md`](instrument-recipes.md)
> is the palette: every named instrument recipe across these carts (the showcase presets,
> the Roland drum voices, the whimsical patches), **organized by engine** — 107 recipes you
> can copy into a new cart. The shelf (this doc) is "which cart"; the palette is "which
> *recipe*." On the radio side, [`instrument-presets.md`](instrument-presets.md) +
> [`radio-voices.md`](radio-voices.md) chart what the 20 stations actually use.

> Scope: this covers carts that **make sound**. Purely-visual toys that happen to be
> tagged `toy` (mandala, traffic jam, the love parade, bloom, dutch sky, oil show/plate,
> line rider, flyover, lil blob, lounge larry, pixel zoo) are not instruments and live
> outside this index.

Related design docs (forward-looking, not catalogs): **what to build next** — and which new
instrument adds the most *new sound* — is
[`design/cart-library-direction.md`](../design/cart-library-direction.md) § 2b (the scored
shortlist); the sound API hub is [`design/audio-notes.md`](../design/audio-notes.md); the
engine-port program is [`design/instrument-engines.md`](../design/instrument-engines.md);
per-station timbre swaps are
[`design/radio-instrument-options.md`](../design/radio-instrument-options.md); unbuilt
stations are [`design/future-stations.md`](../design/future-stations.md); the
generative-soundtrack how-to is [`game-music.md`](game-music.md).

---

> ## 🛑 STOP — if your cart makes *music* (a station / a band / a tune), do this FIRST
>
> **Building a self-playing RADIO STATION?** Don't assemble the path yourself — there's
> one ordered runbook for the whole arc: **[`radio-station-howto.md`](radio-station-howto.md)
> — START THERE.** (It walks firewall → design blind → shop palette → chassis → brains →
> build → register, linking each deep doc at the right step.) The rest of this banner and
> the table below are pieces *it* sequences.
>
> **Before you open a single cousin cart, imagine the band blind.** This is the one
> rule that keeps the library from homogenizing — and it only works if you do it
> *before* reading how other carts are voiced (reading a cousin first anchors you to
> its lineup; that inertia is how the same synth kit ended up in 6 stations and every
> piano got faked on TRI).
>
> 1. **Picture the ideal band from the genre alone** — what does a real record in this
>    style actually use? Write the lineup (instruments + harmony/rhythm/form brains +
>    signature moves) from the *music*, not from our palette. Wikipedia/musicology, not
>    `tools/carts/`.
> 2. **THEN shop the palette** — [`instrument-recipes.md`](instrument-recipes.md) /
>    [`instrument-presets.md`](instrument-presets.md): which imagined voices already
>    exist? Reuse on purpose. Whatever the genre wants that we lack is your new recipe —
>    usually the most valuable thing the cart adds (often an untapped engine:
>    `VOICE`/`PIANO`/`REED`/`BOWED`/`PIPE`/`GUITAR`).
> 3. **THEN copy a cousin's *chassis* only** — the `radio.h`/`solo.h`/`ui.h` wiring from
>    the table below. Take its skeleton; do **not** inherit its instrument lineup.
>
> Full discipline + why: [`cart-authoring-prompt.md`](cart-authoring-prompt.md)
> §"Designing a sound cart's voices — intent-first, then shop". The gap-ledger that
> proves the cost of skipping it: [`../design/radio-genre-fidelity.md`](../design/radio-genre-fidelity.md).
> **The table below is step 3 (chassis) — don't let it be step 1.**

## The building blocks — copy the CHASSIS (not the voices) from your closest cousin

Almost every instrument cart is one of a few shapes. Once you've imagined the band
(banner above), decide which shape your new cart is, then open the **reference cart**
named here and copy its **skeleton/wiring** — the `radio.h`/`ui.h`/held-note plumbing.
Design the *voices* intent-first; never inherit the cousin's instrument lineup.

| If your cart is… | Built from | Reference cart(s) to copy | Notes |
|---|---|---|---|
| **An auto-composing station** (plays itself, you tune it) | `radio.h` (+ `solo.h` for a player jam strip, `improv.h` for an auto-soloist) | `bossa` (simplest), `cocktail`/`motorik` (improv.h soloist), `dub`/`citypop` (solo.h jam strip), `addis`/`afrobeat` (real-engine band: MEMBRANE/GUITAR/BRASS/REED + `euclid()` polyrhythm), `mariachi` (real-engine band: BOWED violins/BRASS trumpets/GUITAR rhythm, the sesquialtera time brain), `air` (ARTIST station: rolls SONG ARCHETYPES, not a re-keyed texture — VOICE/PIPE leads) | See [`game-music.md`](game-music.md) for the chord/clock brain. NEVER hand-roll the chassis. |
| **An expressive monophonic instrument** (one voice that bends/slurs as you move) | held notes: `note_on`/`note_off` + live `note_pitch`/`note_vol`/`note_cutoff` + `note_glide` | `heldnotes` (the 100-line theremin — the canonical minimal one), `otamatone`, `glassharmonica`, `hurdygurdy`, `musicalsaw` | The sustain layer is spec'd in [`design/held-notes.md`](../design/held-notes.md). |
| **A patch panel + keyboard** (build a sound with knobs, then play it) | `ui.h` widgets (knob/slider/button) + `instrument_*` setters + `note_on` | `moog` (dream synth), `modrack` (the big modular) | Use `ui.h` — never hand-roll the drag machine. `modrack` is the data-driven-indices archetype (name knobs with an enum). |
| **A classic-machine homage** (faithful hardware recreation) | `ui.h` faders/knobs + a step sequencer or keybed | drums: `tr808`/`tr909`/`cr78`; mono synth: `tb303`; keyboard synth: `sh101`; poly synth: `juno`; effect showcase: `spacecho` (echo) / `cathedral` (reverb) / `juno` (chorus) | These are dense; pick the one whose *form factor* matches (drum grid vs faders vs keybed). |
| **A single-engine showcase** ("here's what INSTR_X sounds like") | one `INSTR_*` engine + a simple keyboard/pad | `pluck`, `mallet`, `organ`, `epiano`, `fm`, `pd`, `tabla`, `handpan` | The tech-demo pattern: one engine, a few live knobs, an autoplay noodle. ~250–390 lines each. |
| **A step sequencer / looper** (a grid or a record-and-layer loop) | the beat clock (`bpm`/`beat`/`beat_pos`) + `schedule_hit` | `drummachine` (16-step grid), `loopstation` (4-track live looper), `mariopaint` (note-placement staff) | |
| **A multitouch keyboard / pad toy** | the touch API (`touch_*`, `tapr`, `touch_ended_*`) | `touchpiano` (per-finger note_on/off), `multitouch` (paint), `sh101` (chord + fader at once) | Touch model + release API: [`design/touch-notes.md`](../design/touch-notes.md). |

**Which engine for which timbre?** (the `INSTR_*` modeled engines, wave ids 16+):
`PLUCK` = plucked string (Karplus-Strong) · `MALLET` = struck bar/metal · `ORGAN` =
drawbar sines · `EPIANO` = Rhodes/Wurli/Clav · `FM` = 2-op DX bell/bass · `PD` = Casio
CZ phase-distortion · `MEMBRANE` = struck drumhead · `REED`/`PIPE` = blown reed/flute ·
`GUITAR`/`PIANO`/`BOWED` = bodied string / struck stiff string / bowed string · `BRASS` =
lip-reed brass (trumpet→tuba, the slide horn) · `USER0..3` = your own drawn
single-cycle wave (`wave_set`). The raw chiptune waves (`SINE`/`SAW`/`SQUARE`/`TRI`/`NOISE`)
are still there for everything else. Full engine catalog + what's unbuilt:
[`design/instrument-engines.md`](../design/instrument-engines.md) §8.9.

**Stereo (cross-cutting — any model above can use it):** place a voice in the field with
`instrument_pan(slot, -1..+1)` (per slot), `note_pan(handle, -1..+1)` (live — e.g. follow a
sprite's screen-x for positional audio), or `LFO_PAN` (auto-pan). Default is center, and the
linear pan law leaves a centered voice bit-identical to mono, so it's opt-in and free. Reference:
the **`pan`** cart. Stereo *width* effects (ping-pong, reverb spread) are the deferred §8.10 layer,
not per-voice. Full spec: [`design/stereo.md`](../design/stereo.md).

**Tight web timing — automatic (cross-cutting):** every `kind: "instrument"` cart (these,
the radios, all of them) is **auto-built with the AudioWorklet backend** for the web — audio
runs on a dedicated thread, so scheduled notes land sample-tight instead of wobbling on the
main thread (the ScriptProcessor ceiling). It's free and needs no per-cart code: `build-site`
ships both a worklet build and a ScriptProcessor fallback + a loader that auto-picks per
browser. Opt a *non*-instrument cart in with `worklet: true` in its `.cart.js`, or opt out with
`worklet: false`. Full rationale + the build: [`design/audio-threading.md`](../design/audio-threading.md).

---

## Playable synths

The "build a sound, then play it" carts — the deepest, most general instruments.

| Cart | What it is | Built from |
|---|---|---|
| **dream synth** (`moog`) | A playable Moog-style patch panel + keyboard that ties the whole engine together — pick a wave, drag the ADSR, route 3 LFOs, dial a resonant filter with DRIVE, play on-screen or with the A-row keys. | `ui.h`, `note_on`, `INSTR_SAW`, all `instrument_*` |
| **modrack** (`modrack`) | A tiny modular synth built in patchable steps — drawn user waves, mod-envelopes, the whole `INSTR_*` family on tap. The data-driven-indices archetype (knob enums). | `gestures.h`, `note_on`, every engine |

## Classic-machine homages

Faithful recreations of real hardware. Drum machines, the acid box, the strap synth,
the tape echo.

| Cart | What it is | Built from |
|---|---|---|
| **cr-78 compurhythm** (`cr78`) | All 15 voices of the Roland CR-78 (1978) rebuilt from their analog circuits. | step grid, chiptune waves + `NOISE` |
| **tr-808 rhythm composer** (`tr808`) | The big one — the 808 kit, step-sequenced. | step grid, chiptune + `NOISE` |
| **tr-909 rhythm composer** (`tr909`) | The house/techno 909 — analog kick/snare/toms + FM-clang hats. Completes the ReBirth rack. | step grid, `INSTR_FM` |
| **tb-303 bass line** (`tb303`) | The acid machine — one mono voice through a resonant lowpass, piano-roll sequenced, with slide/accent/drive. | `note_on`, `INSTR_SAW`/`SQUARE`, live `note_cutoff`/`note_res`/`note_drive` |
| **sh-101** (`sh101`) | Roland's SH-101 (1982) strap synth, modeled panel section-by-section — 4-source mixer, sequencer, arp, full multitouch keybed. | `ui.h`, `note_on`, multitouch, `INSTR_SAW`/`SQUARE`/`NOISE` |
| **re-201 space echo** (`spacecho`) | Roland's RE-201 (1974) tape echo — the showcase for THE echo bus: sweep the rate and the tails pitch-bend; crank feedback past the red and it self-oscillates. | `INSTR_PLUCK`, `echo()`/`instrument_echo` |
| **juno-6** (`juno`) | Roland's Juno-6 (1982) — a saw+sub+resonant-LP poly synth whose one defining feature is the BBD CHORUS: the showcase for `chorus()`. Dry it's "sh101 with more voices"; the OFF/I/II switch flips it to a lush wide stereo wash. | `ui.h`, `hit`, `INSTR_SAW`, `chorus()`, `instrument_filter` |
| **cathedral** (`cathedral`) | A church organ blooming into a vast stone hall — the showcase for `reverb()`. Strike a chord; the dry attack is brief but the reverb tail swells and hangs for seconds. SIZE/DAMP/WET + chord pads + an AUTO processional. | `ui.h`, `hit`, `INSTR_ORGAN`, `reverb()`/`instrument_reverb` |
| **omnichord** (`omnichord`) | A tribute to the Suzuki Omnichord (1981) — chord buttons + a strum plate. | chiptune + `NOISE` |
| **stylophone deluxe** (`stylophone`) | The pocket stylus-synth — live pulse-width + a tempo-synced reiteration switch. | `note_on`, `INSTR_USER0`/`SQUARE`, beat clock |
| **mt-70** (`mt70`) | A 1982 Casiotone home keyboard rebuilt with ZERO new engine code — the "prove the engine's range" probe. | `note_on`, `INSTR_MALLET`/`SINE`/`NOISE` |
| **otamatone** (`otamatone`) | The face-on-a-stem toy — drag the stem ribbon, squeeze the mouth to open a lowpass. (Also a held-notes instrument.) | `note_on`, `INSTR_SQUARE` |

## Expressive solo instruments (held-notes)

One sustained voice that bends and slurs as you move. The minimal teaching examples for
the held-note API.

| Cart | What it is | Built from |
|---|---|---|
| **held notes** (`heldnotes`) | The 100-line theremin — THE canonical minimal held-note cart. Start here. | `note_on`, `INSTR_SAW` |
| **mouth harp** (`mouthharp`) | A jaw harp: pitch is *frozen*, and a high-Q `FILTER_BAND` formant sweep (`note_cutoff`+`note_res`) is the whole melody — "resonance-as-melody," the bandpass-vowel corner no other cart centers on. TAB toggles pluck (tempo-locked re-twang) vs continuous drone. | `note_on`, `INSTR_SAW`, `FILTER_BAND`, beat clock |
| **glass harmonica** (`glassharmonica`) | Ben Franklin's rubbed-wine-glass contraption. | `note_on`, `INSTR_SINE` |
| **hurdy-gurdy** (`hurdygurdy`) | The cranked drone-fiddle — built around a crank mechanic nothing else uses. | `note_on`, `INSTR_SAW`/`NOISE` |
| **musical saw** (`musicalsaw`) | The theremin's spooky cousin — the one cart that drives `note_lfo` LIVE. | `note_on`, `INSTR_SINE` |

## Single-engine showcases

One modeled `INSTR_*` engine each, with a small keyboard and an autoplay noodle. The
cleanest examples of "here's what this engine sounds like."

| Cart | Engine | What it is |
|---|---|---|
| **pluck** (`pluck`) | `INSTR_PLUCK` | Karplus-Strong string — eight pentatonic strings, three live knobs. |
| **guitar** (`guitar`) | `INSTR_GUITAR` | The bodied pluck — PLUCK's string + a resonant body (4 formant biquads). Eight strings, three macros (body/brightness/mute), eight hardware presets incl. pizzicato. Built on `pluck`. |
| **piano** (`piano`) | `INSTR_PIANO` | The struck stiff string (StifKarp) — KS string + a dispersion allpass chain (the metallic shimmer) + a baked grand-piano soundboard. A one-octave keyboard, three macros (stiffness/hammer/pedal), six presets (grand→celesta). Single-string v1. |
| **bowed** (`bowed`) | `INSTR_BOWED` (arco + pizz) | The bowed string (Smith/McIntyre stick-slip waveguide) — self-oscillating/held, played by RUBBING: drag a string and the energy ACCUMULATES (rub more → builds & digs in), stop and it rests; a quick TAP plays PIZZICATO — the **same** waveguide flagged `eng_tune(slot,0,1)`, seeded with a pluck and the friction bypassed, so the identical string+body rings down (arco/pizz differ only in excitation). Three macros (position/pressure/speed), six presets (violin→tremolo). The interaction-driven showcase, not just a keyboard. |
| **brass** (`brass`) | `INSTR_BRASS` | The lip-reed family (trumpet→tuba), the LAST engine-blocked instrument. A bore + a self-oscillating pressure valve (reed's core) under a dynamics-swept brass formant — push *brassiness* and the tone opens up blatty. Marquee: **drag the trombone SLIDE** for a live glissando (`note_pitch`). Six hardware presets, mono+slide by default. |
| **mallet** (`mallet`) | `INSTR_MALLET` | Struck bar simulation. |
| **organ** (`organ`) | `INSTR_ORGAN` | Nine drawbar sines per key. |
| **epiano** (`epiano`) | `INSTR_EPIANO` | Rhodes / Wurlitzer / Clavinet in one. |
| **fm** (`fm`) | `INSTR_FM` | Two-operator FM — the DX recipe. |
| **pd** (`pd`) | `INSTR_PD` | Phase distortion — the Casio CZ sound. |
| **tabla** (`tabla`) | `INSTR_MEMBRANE` | A struck drumhead. |
| **handpan** (`handpan`) | `INSTR_MALLET` | A steel hang drum — one ding, eight tone fields, every strike a gesture. |
| **voxlab** (`voxlab`) | `INSTR_VOICE` *(experimental)* | The formant VOICE — a navkit VoicForm port (glottal pulse → 4 vowel formants). **A probe, not a finished showcase**: 7 raw params on sliders via the throwaway `voice_param()` poke, auditioning which 3 become the public macros. Verdict pending in [`probe-carts.md`](../design/probe-carts.md); don't build a voice cart on the API until it's locked. |

## Radio stations — auto-composing genre machines

Endless generative stations built on `radio.h`. You don't play notes; you tune the
station and (where present) jam over it. The biggest family on the shelf — copy the
closest genre cousin, never start a station from scratch.

> **Which instruments does each station use, and what's shared?** Two companion docs map
> every station's voices: [`radio-voices.md`](radio-voices.md) — a voice chart per station
> (slot → role → named preset) + a findings summary; and
> [`instrument-presets.md`](instrument-presets.md) — the named-patch catalog those charts
> reference, showing where the same sound recipe is reused across stations. **Ship a new
> station → update both** (see the checklist below).

| Cart | Homage / genre | Extras |
|---|---|---|
| **bossa radio** (`bossa`) | bossa nova | `solo.h` jam strip |
| **ambient radio** (`ambient`) | beatless ambient drift | — |
| **jangle radio** (`jangle`) | mixolydian slacker pop | `solo.h` |
| **jingle radio** (`jingle`) | delicate dusk songwriter | `solo.h` |
| **low end radio** (`lowend`) | jazz-rap boom bap (Tribe homage) | `INSTR_MALLET` vibraphone |
| **city pop radio** (`citypop`) | Japanese city pop | `solo.h` |
| **dub radio** (`dub`) | roots dub (King Tubby / Augustus Pablo) | `solo.h`, melodica call-and-response |
| **ymo radio** (`ymo`) | techno-kayo (Yellow Magic Orchestra) | — |
| **satie radio** (`satie`) | solo-piano gymnopédies (Erik Satie) | soloist-less |
| **house radio** (`house`) | French house / filter disco (Daft Punk) | `note_on` filter sweeps |
| **exotica radio** (`exotica`) | exotica (Martin Denny / Les Baxter) | vibraphone over many engines |
| **yacht radio** (`yacht`) | yacht rock / AOR (Steely Dan / Doobies) | DX EPs, multi-engine |
| **roadhouse radio** (`roadhouse`) | modal psych-rock (The Doors) | `improv.h` organ soloist |
| **cocktail radio** (`cocktail`) | piano-trio lounge (Vince Guaraldi) | `improv.h` soloist |
| **tango radio** (`tango`) | Golden-Age orquesta típica | three orquesta voicings |
| **motorik radio** (`motorik`) | krautrock (Neu! × Stereolab) | `improv.h` vibraphone soloist |
| **addis radio** (`addis`) | ethio-jazz (Mulatu Astatke) — scales-as-data | five engines, `improv.h` |
| **carlos radio** (`carlos`) | Switched-On Bach (Wendy Carlos) — two-voice species counterpoint | `moog.c`'s fat-Moog signal path; no chord table |
| **italo disco radio** (`italo`) | Italo disco (Gazebo / Den Harrow) — minor-key spaghetti disco | octave-arp `INSTR_SAW` bass, `INSTR_FM` brass stabs, `INSTR_EPIANO` keys, `INSTR_PD` lead; the truck-driver gear change; 808 + pump on loan from `house` |
| **afrobeat fm** (`afrobeat`) | Afrobeat (Fela Kuti / Tony Allen) — held modal vamp | first-radio `INSTR_GUITAR`/`INSTR_BRASS`, REED horn section, `euclid()` polyrhythm, `improv.h` sax solo |
| **air fm** (`air`) | AIR / Moon Safari — **artist station, 5 song archetypes** | stolen-playbook chord brain (Sexy Boy / La Femme d'Argent / Playground Love / Cherry Blossom Girl / Kelly Watch the Stars); per-archetype lead/groove/tempo; first-radio `INSTR_VOICE` vocoder lead + `INSTR_PIPE` flute; Solina detuned-SAW pad; `solo.h` |
| **mariachi fm** (`mariachi`) | Mariachi / son jalisciense — the SESQUIALTERA (6/8-vs-3/4) | **first-radio `INSTR_BOWED`** violin section (+ scoop), `INSTR_BRASS` trumpet call-and-response, `INSTR_GUITAR` ×3 (vihuela/guitarra/guitarrón); functional I-IV-V; son/vals/huapango groove; `sinte` SAW-string fallback in the band panel |
| **polo & pan radio** (`polopan`) | Polo & Pan — **artist station, 5 song archetypes** | stolen-playbook chord brain (Canopée / Ani Kuni / Nanga / Tunnel / Coeur Croisé); per-archetype lead/groove/tempo/form; mallet chair (marimba/balafon/glock/vibe), `INSTR_BOWED` **pizzicato** bounce, `INSTR_PIPE`/`INSTR_VOICE` topline (engine swapped per song), resonant MS-10 `saw/ms10-bass`; `pipe`/`sine` flute A/B in the band panel; `solo.h` |

Many stations also support **THE BAND** panel (press **B**) — a live timbre-audition
overlay to swap a chair's instrument mid-song. Candidate swaps:
[`design/radio-instrument-options.md`](../design/radio-instrument-options.md).

## Sequencers, loopers & note toys

| Cart | What it is | Built from |
|---|---|---|
| **drum machine** (`drummachine`) | A 16-step sequencer — the original sound-synth showcase. | step grid |
| **loopstation** (`loopstation`) | A live-looper pedal — arm a track, play, layer; the first cart that records itself. | beat clock, `note_on`, all chiptune waves |
| **composer (mario paint sound)** (`mariopaint`) | A melodic music toy in the spirit of Mario Paint's Sound mode — place notes on a staff. | chiptune waves |
| **touch piano** (`touchpiano`) | A two-octave multi-finger piano — the touch-release API's first customer. | touch API, `note_on`, `INSTR_SAW` |
| **multitouch paint** (`multitouch`) | Finger-paint playground for the touch API (makes mallet tones). | touch API, `INSTR_MALLET` |
| **le petomane** (`fartsynth`) | A fart synthesizer that is secretly a sound-design lesson. | all chiptune waves |
| **music garden** (`garden`) | A screensaver that plays itself — ambient generative bells. | `INSTR_TRI`/`SINE` |
| **drift** (`drift`) | A timing **diagnostic** you can hear: the same 16th pulse on three clocks at once — `schedule_hit` (tight, center) vs `hit()`-on-frame (jittery, left) vs every-N-frames (drifts, right) — with per-voice error scopes and a GLITCH button. The A/B rig for the web `frame_dt` clamp; see [`../design/audio-timing.md`](../design/audio-timing.md). | beat clock, `schedule_hit`, `instrument_pan` |

---

## Adding a new instrument cart — checklist

0. **Need an idea, or want the highest-impact one?**
   [`design/cart-library-direction.md`](../design/cart-library-direction.md) § 2b is the
   scored shortlist of buildable instruments, ranked by *new sound* (hand-drum / bellows /
   violin top it — engines that are showcase-only today). Skip if you already know what you're building.
1. **Decide the shape** from the building-blocks table above and open the named
   reference cart. Copy its skeleton.
2. **Don't hand-roll plumbing that already exists** — `radio.h` for stations, `ui.h`
   for knobs/sliders/buttons, `solo.h`/`improv.h` for jam/solo layers, the held-notes
   API for sustained voices. Check `runtime/*.h` first (CLAUDE.md → runtime tree).
3. **Pick the right engine** (`INSTR_*` table above) rather than approximating a timbre
   with raw waves if a modeled engine fits.
4. **If it's a station**, read [`game-music.md`](game-music.md) for the chord/clock
   brain and pin the seed so THE BAND swaps stay byte-identical.
5. **Follow the standard cart-add flow** (build → bake screenshot → register in
   `index.json` with `kind:["instrument"]` (+ `"toy"` for stations) → `lint-carts.js`):
   [`cart-authoring.md`](cart-authoring.md).
6. **Add a row to the right table in this doc** so the shelf stays complete.
7. **Keep the recipe docs current** (a rule — they only stay useful if maintained; run
   `node tools/lint-docs.js` after):
   - **A radio station?** Add its voice chart to [`radio-voices.md`](radio-voices.md), and in
     [`instrument-presets.md`](instrument-presets.md) name any new patch recipes — or, for a
     recipe you reused, add your cart to that preset's **used by** line (don't duplicate).
   - **A showcase / machine / whimsical instrument with presets?** Add its recipes (by engine)
     to the palette, [`instrument-recipes.md`](instrument-recipes.md), so the shelf's recipe
     supply stays complete.
