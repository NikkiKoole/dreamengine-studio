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

> **Lost in the instrument docs?** [`instrument-map.md`](instrument-map.md) is the hub over
> all of them — which file answers what, plus the hardware-wings inventory (homages by
> manufacturer) and where the what-next assessments live.

Related design docs (forward-looking, not catalogs): **what to build next** — and which new
instrument adds the most *new sound* — is
[`design/cart-library-direction.md`](../design/cart-library-direction.md) § 2b (the scored
shortlist); the sound API hub is [`design/audio-notes.md`](../design/audio-notes.md); the
engine-port program is [`design/instrument-engines.md`](../design/instrument-engines.md);
per-station timbre swaps are
[`design/radio-instrument-options.md`](../design/radio-instrument-options.md); unbuilt
stations are [`design/future-stations.md`](../design/future-stations.md); the
generative-soundtrack how-to is [`game-music.md`](game-music.md); the cross-shelf audit of
**mono-vs-poly, untapped multitouch, and shared-code/header opportunities** (what plumbing
to factor out next, the `keybed.h` move again) is
[`design/multitouch-and-generalization-audit.md`](../design/multitouch-and-generalization-audit.md).

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
| **An auto-composing station** (plays itself, you tune it) | `radio.h` (+ `solo.h` for a player jam strip, `improv.h` for an auto-soloist) | `bossa` (simplest), `cocktail`/`motorik` (improv.h soloist), `dub`/`citypop` (solo.h jam strip), `addis`/`afrobeat` (real-engine band: MEMBRANE/GUITAR/BRASS/REED + `euclid()` polyrhythm), `mariachi` (real-engine band: BOWED violins/BRASS trumpets/GUITAR rhythm, the sesquialtera time brain), `air`/`polopan`/`napoleon`/`wba`/`thexx` (ARTIST stations: roll SONG ARCHETYPES, not a re-keyed texture — `napoleon` swaps engines per song + a per-archetype jam ribbon; `wba` (Whitest Boy Alive) is the first station on the `ampcab.h` guitar amp/cab + a clean-guitar pedalboard, with a negative-space guitar↔bass duet; `thexx` (The xx) is the two-voice CALL-AND-RESPONSE (two `INSTR_VOICE` singers trading by phrase, unison on the chorus) + space-as-structure — the dark twin of `wba`), `ambient`/`eno` (BEATLESS drone: held voices, no step clock — `eno` = coprime-length loops phasing against each other, no chord brain, emergent harmony, + the seed-rolled four-setup variety lever), `plantasia` (MELODY-FORWARD: the lead is the protagonist — THE SONGWRITER seeded theme-and-variation brain #3 over an A-A'-B-A songform, the Moog voiced four ways, the seed-rolled track-feels, a growing-plant window), `braindance`/`squarepusher`/`plaid` (IDM: `braindance` = rhythm-as-grammar + volatility + `ratchet()` sub-hits; `squarepusher` makes the BASS the lead (Hosono walker + `improv.h` bass solo + slap) over moving jazz-fusion changes with a tender↔manic LURCH; `plaid` = the gentle pole — INTERLOCKING ARPEGGIOS (3–4 cells at different cycle lengths weaving an emergent tune) in odd meters that flow), `vapor` (vaporwave — the first EFFECTS-FORWARD station: the FX chain (reverb+chorus+tape+echo+crush) IS the genre over a lounge loop, + `varispeed` ridden live as the slowed-tape WOBBLE), `lofi` (lo-fi/Nujabes/Dilla jazzy hip-hop: lush Rhodes jazz + THE DRUNK POCKET — a *dialable* off-grid feel (snare-late/lazy-kick/swing), the loose pocket `lowend` undersold — over vapor's reused lo-fi rack) | See [`game-music.md`](game-music.md) for the chord/clock brain. NEVER hand-roll the chassis. |
| **An expressive monophonic instrument** (one voice that bends/slurs as you move) | held notes: `note_on`/`note_off` + live `note_pitch`/`note_vol`/`note_cutoff` + `note_glide` | `heldnotes` (the 100-line theremin — the canonical minimal one), `otamatone`, `glassharmonica`, `hurdygurdy`, `musicalsaw` | The sustain layer is spec'd in [`design/held-notes.md`](../design/held-notes.md). |
| **A patch panel + keyboard** (build a sound with knobs, then play it) | `ui.h` widgets (knob/slider/button) + `instrument_*` setters + `note_on` | `moog` (dream synth), `modrack` (the big modular) | Use `ui.h` — never hand-roll the drag machine. `modrack` is the data-driven-indices archetype (name knobs with an enum). |
| **A polyphonic keyboard you play** (white/black keys, chords, glissando) | **`keybed.h`** — touch + mouse + QWERTY + **MIDI keyboard**, all → one slot, with velocity | `epiano`, `touchpiano`, `mellotron`, `organ`, `solina`, `piano` (`moog` = panel + keys) | **NEVER hand-roll the key tables / finger pool / glissando** — `keybed.h` owns it, and a USB MIDI keyboard plays it for free (native + web). Carts with a special voice model (`mt70`, `sh101`) or a continuous ribbon (`martenot`) instead wire MIDI cart-side via `midi_get()`. See [`../design/midi-and-keybed.md`](../design/midi-and-keybed.md). |
| **A classic-machine homage** (faithful hardware recreation) | `ui.h` faders/knobs + a step sequencer or keybed | drums: `tr808`/`tr909`/`tr606`/`cr78`; mono synth: `tb303`; keyboard synth: `sh101`; poly synth: `juno`; effect showcase: `spacecho` (echo) / `cathedral` (reverb) / `juno` (chorus) / `mistress` (flanger) | These are dense; pick the one whose *form factor* matches (drum grid vs faders vs keybed). |
| **A single-engine showcase** ("here's what INSTR_X sounds like") | one `INSTR_*` engine + a simple keyboard/pad | `pluck`, `mallet`, `organ`, `epiano`, `fm`, `pd`, `tabla`, `handpan` | The tech-demo pattern: one engine, a few live knobs, an autoplay noodle. ~250–390 lines each. |
| **A step sequencer / looper** (a grid or a record-and-layer loop) | the beat clock (`bpm`/`beat`/`beat_pos`) + `schedule_hit` | `drummachine` (16-step grid), `loopstation` (4-track live looper), `mariopaint` (note-placement staff), `tracker` (Song→Chain→Phrase tracker — the song-ARRANGEMENT reference, [`design/tracker-cart.md`](../design/tracker-cart.md)) | |
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

**Computer-keyboard / keybed layout — ALWAYS the GarageBand map (cross-cutting):** any
instrument a player types notes on defaults to the **GarageBand musical-typing** layout, so
the muscle memory carries between carts. Home row `A S D F G H J K L ; '` = the **white** keys
(`A` = C), the QWERTY row above `W E _ T Y U _ O P` = the **blacks** that sit over them — i.e.
the key's index in the string `A W S E D F T G Y H U J K O L P ; '` *is* its semitone (18
semis, 1.5 octaves). **`Z` / `X` shift the octave down / up.** **The easy way to get all of
this (plus touch + MIDI):** use `keybed.h` — it bakes in the GarageBand layout and the octave
keys; you only `#define KEYBED_WHITE_KEYS`/`_BLACK_KEYS` if a cart needs a different reach.
Only hand-roll the `KBMAP[]` + `keyp/keyr` loop (copy it verbatim from `mt70`/`sh101`/
`martenot`) when the cart is a keybed.h exception (special voice model or a ribbon).
And remember **the desktop mouse is already a synthetic touch** (`studio.c` mouse-as-touch),
so a `tap()`/`tapp()` button is clickable for free — do **not** *also* handle it via
`mouse_pressed()`, or every edge double-fires (an even number of toggles reads as a dead
button — it bit `martenot`).

**Held voices + per-slot effects — use the `note_*` live setters (cross-cutting):** a voice
snapshots its slot's reverb/echo send and chorus/flanger *bus* at `note_on` (`sound.h`), so for
a **persistent held voice** (`note_on` once, driven live — theremins, drones, the Ondes) changing
`instrument_reverb()`/`instrument_echo()` afterward does **nothing** to the sounding note. Drive
the send live with **`note_reverb()` / `note_echo()`** instead, and set **`instrument_chorus()`
*before* the `note_on`** (the fx-bus routing is snapshotted, only its params are live). The master
`reverb(size,damping)` is global and always live. (This is why `martenot`'s diffuseur switches were
silent at first — it set the slot send, not the live per-voice send.)

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
| **tr-606 drumatix** (`tr606`) | The TB-303's silver sibling — the tin insect. Double twin-T kick (two resonators beating, TONE crossfade), single-mode paper snare, and the jewel-box hats from the circuit-analysis metal bank (246–627Hz squares, the beating 418+440 pair); open-hat decay is TEMPO-LINKED like the hardware, closed hat fires the shut-off (choke). Stock panel is austere (accent = the only word); the M key reveals the mod-culture TUNE/DECAY/character knobs. Completes the Roland wing. | step grid, chiptune + `NOISE`, `instrument_choke` |
| **tb-303 bass line** (`tb303`) | The acid machine — one mono voice through a resonant lowpass (the `FILTER_DIODE` ladder since 2026-07-02), piano-roll sequenced, with slide/accent/drive. | `note_on`, `INSTR_SAW`/`SQUARE`, live `note_cutoff`/`note_res`/`note_drive` |
| **acid rack** (`acidrack`) | The ReBirth RB-338: 2×tb303 + the full tr909 + the curated tr808 in ONE cart — accordion strips, banks A–D + a 64-bar song chain, per-device FX (dist + one shared delay w/ sends), per-voice TUNE/DEC/CHAR mini-knobs, and a seeded GENERATOR: an 8-hex song code = a whole arranged track, WAV-exportable. The tinyjam-racks pilot (docs/design/rebirth-classic.md). | everything the three machines use + `rad_srnd` seeding, `save_bytes`, `.bake/wav_request` export, master `filter()` PCF lane |
| **session desk** (`yachtrack`) | The yacht radio opened up — tinyjam rack #2, the first CHORD-FIRST rack: an editable chord CHART (the mu vocabulary, MU-IFY/SUS-MELT, form row + gear change) + drum skeleton lanes with three drummer chairs + the sax hook cell, while bass runs/comp anticipation/ghosts/swing stay session PLAYERS behind feel knobs. The generator is yacht's `new_song` verbatim — an 8-hex code from the radio dial reproduces that exact song here, opened up (the first radio→rack seed handoff, spec-gated). docs/design/yacht-rack.md | `radio.h` (seed stream, clock, `rad_lead_to`/`rad_bass_to`, chairs), `save_bytes`, `.bake/wav_request` export, `spec()` golden corpus |
| **sh-101** (`sh101`) | Roland's SH-101 (1982) strap synth, modeled panel section-by-section — 4-source mixer, sequencer, arp, full multitouch keybed. | `ui.h`, `note_on`, multitouch, `INSTR_SAW`/`SQUARE`/`NOISE` |
| **re-201 space echo** (`spacecho`) | Roland's RE-201 (1974) tape echo — the showcase for THE echo bus: sweep the rate and the tails pitch-bend; crank feedback past the red and it self-oscillates. | `INSTR_PLUCK`, `echo()`/`instrument_echo` |
| **juno-6** (`juno`) | Roland's Juno-6 (1982) — a saw+sub+resonant-LP poly synth whose one defining feature is the BBD CHORUS: the showcase for `chorus()`. Dry it's "sh101 with more voices"; the OFF/I/II switch flips it to a lush wide stereo wash. | `ui.h`, `hit`, `INSTR_SAW`, `chorus()`, `instrument_filter` |
| **cathedral** (`cathedral`) | A church organ blooming into a vast stone hall — the showcase for `reverb()`. Strike a chord; the dry attack is brief but the reverb tail swells and hangs for seconds. SIZE/DAMP/WET + chord pads + an AUTO processional. | `ui.h`, `hit`, `INSTR_ORGAN`, `reverb()`/`instrument_reverb` |
| **electric mistress** (`mistress`) | An EHX Electric Mistress flanger pedal — the showcase for `flanger()`. Strum a guitar through the swept comb; the FLANGER footswitch A/Bs it, FEEDBACK takes it from gentle to screaming jet, and the JET button fires a noise swell = the jet overhead. | `ui.h`, `strum`, `INSTR_GUITAR`/`INSTR_NOISE`, `flanger()` |
| **combo amp** (`combo`) | A vintage combo amp you play a guitar through — the showcase for the amp/cab bundle (Increment E): each VOICING is `drive`+`eq`+`glue` baked as one preset (clean→chime→crunch→hi-gain→lo-fi), pinned as the output stage like leslie. GAIN/BASS/MID/TREBLE/MASTER/SAG dial it; the tube glow, VU needle and grille shudder track your playing (a cart-side level proxy). | `keybed.h`, `INSTR_GUITAR`, `instrument_drive`/`_mode` + `instrument_eq` + `glue()` |
| **tapeloop** (`tapeloop`) | A Frippertronics tape loop — the showcase for `tape()`. A pad feeds a long `echo()` loop and the whole circulating wash runs through `tape()`, so every pass returns warmer + more warped (wow/flutter pitch-drift + saturation). WOW/FLUTTER/SAT/FEEDBACK knobs; two reels spin and wobble. | `ui.h`, `hit`, `INSTR_SAW`/drawn waves, `tape()` + `echo()` |
| **mellotron** (`mellotron`) | The 1963 tape-replay keyboard, faked in pure synthesis (the [recorded-timbres](../design/recorded-timbres.md) "Mellotron problem", now built). Drawn STRINGS/CHOIR/FLUTE/BRASS waves + a noise attack CHIFF + `chorus()` ensemble + `tape()`/`reverb()` reconstruct a "recording" with no sampler. Headline: the **~8-second tape limit** — a held note runs out and stops on its own (lift to rewind); each held key drains its tape on-screen. M400 cabinet (wood cheeks, cream panel, TONE/TAPE knobs). **Layering** — tap the voice buttons to stack timbres (the drawn waves are SUMMED into one cycle, additive → still one voice per note) or hit a COMBI preset. Playable three ways: computer keyboard (the GarageBand musical-typing map, Z/X octave, labelled on the keys), mouse, or multitouch (chassis from `touchpiano`). | `ui.h`, `key`/`keyp`/`keyr`, multitouch, `note_on`/`note_off`, drawn waves (`wave_set`/`INSTR_USER0` blend), `INSTR_NOISE`, `chorus()`/`tape()`/`reverb()` |
| **clavinet** (`clavinet`) | A Hohner Clavinet D6 through an auto-wah — the showcase for `wah()`. A percussive clav funk riff run through `instrument_wah()`: the envelope filter opens on each hard stab and quacks shut between — the funk "wakka-wakka." WAH footswitch A/Bs it; SENS/RES/MIX; a mouth opens with the envelope. | `ui.h`, `hit`, `INSTR_EPIANO` (clav), `wah()`/`instrument_wah` |
| **dj filter** (`djfilter`) | The legendary one-knob mixer filter (Allen & Heath XONE:92 / Pioneer DJM) — the showcase for the master `filter()` bus. One BIPOLAR knob over a self-playing bright house loop: center = open, CCW closes a lowpass to a muffled thump, CW opens a highpass to a thin telephone; RES screams it into acid. A live filter-response curve tilts and peaks as you ride it; the BUILD button runs a breakdown→8-bar riser→DROP. BYPASS A/Bs it. | `ui.h`, `filter()`/`FX_FILTER`, `hit`/beat clock, `INSTR_SAW` loop (groovebox voices) |
| **solina** (`solina`) | The ARP/Eminent Solina String Ensemble (1974) — the chorus() companion to juno: not a synth but a divide-down ORGAN, the timbre built from six stacked-saw FOOTAGE tabs (16′/8′/8′/4′ strings + 8′/4′ brass), fully polyphonic. Bare it's a buzzy organ; flip the ENSEMBLE switch and the triple-BBD chorus smears it into THE '70s string wash. HOLD a key → swell in / ring out. | `ui.h`, `note_on`/`note_off`, multitouch, `INSTR_SAW`, `chorus()`, `instrument_filter` |
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
| **monochord** (`monochord`) | One string, no frets — your FINGERS are the movable bridges and segment LENGTH is pitch (a string halved = an octave up). The đàn bầu / diddley bow / slide-guitar / Pythagoras gesture. Drag on the string to place a bridge; two fingers → the gap between them sounds (spread = lower, squeeze = higher); flick across to pluck a segment (pluck several = a chord on one string). PD oscillator so a held note GLIDES as you slide. A `pointer.h` **probe**: does fretting-by-finger feel playable without frets, before a full string fretboard? ([`../design/multitouch-and-generalization-audit.md`](../design/multitouch-and-generalization-audit.md)) | `pointer.h`, `note_on`/`note_pitch`/`note_glide`, `INSTR_PD` |
| **fretboard** (`fretboard`) | The monochord with more strings — six FRETLESS strings, finger position = pitch, slide to glide. Touch several strings at once → a chord by finger GEOMETRY (not a button — the honest version of `pedalboard`'s chord shapes); drag across = a strum; SPACE = a desktop barre-strum at the cursor. The multi-string payoff of the `monochord` probe. One ringing PD voice per string. | `pointer.h`, `note_on`/`note_pitch`/`note_glide`, `INSTR_PD` |
| **upright bass** (`upright`) | The jazz double bass played on its FINGERBOARD — four strings, press to sound, **slide left/right to walk the frets** (each semitone re-articulates — clean both ways), **pull (either way) to bend up** continuously (+2 semis, the string deflects), or start a drag in the gap and **sweep through a string to PICK it** (à la `pluck.c` — press on a string grabs/frets, press in the gap picks), lift to damp; mono / last-note-wins like a walking line. The axis split matches the engine: a waveguide string bends UP but not below its pitch (verified), so down-moves are the fret walk, up-pulls are the smooth bend (and a bent note can be released to fall back to the fret). The right hand is a PIZZ / ARCO / SLAP switch, and pizz + arco are the SAME string (`INSTR_BOWED` + `eng_tune(slot,0,1)` plucks what the bow would draw — the only cart using the engine's built-in pizz mode). And the WOOD around the fingerboard is percussion — slap the belly for a low boom, knock the neck for a drier tick (`INSTR_MEMBRANE`, location-mapped). Mouse/tap or GarageBand keyboard + Z/X octave. | `note_on`/`note_pitch`/`note_glide`, `INSTR_BOWED` (`eng_tune` pizz), `INSTR_MEMBRANE` body, `INSTR_NOISE` slap |
| **pd bass** (`pdbass`) | The `upright`'s fingerboard re-voiced on the Casio CZ phase-distortion engine — the synth sibling. Because `INSTR_PD` is an OSCILLATOR (sustains, glides both ways), it does the **true smooth continuous slide up AND down** the bowed string couldn't (verified: bends a held note down +0.9¢ where the waveguide stuck) — no fret-walk. Same interactions: press=pluck, slide=glide, pull=signed bend (slide freezes while pulling), sweep-the-gap=pick. One pluck mode (RESO TRAP wave); one live **TIMBRE** slider (right edge) sweeps the PD distortion clean↔buzzy; a toggleable backing **DRUM LOOP** (SPACE / button — boom-bap kick/snare/hat at 96bpm) to play over. GarageBand keys + Z/X octave. | `note_on`/`note_pitch`/`note_glide`, `INSTR_PD`, `beat()` loop, hand-rolled vertical slider |
| **ondes martenot** (`martenot`) | The 1928 ribbon synth — a two-hand upgrade to the theremin. ONE persistent voice; the left **touche d'intensité** lever is the gate, driving `note_vol` *and* `note_cutoff` together (no press = silence, harder = louder + brighter). Pitch from a continuous **ruban** or a snapped **clavier**, aligned on one axis. The COMBINABLE timbre stops (O/C/G/N) are summed live into a single drawn cycle via `wave_set` (the drawbar trick — the only held-notes cart that rebuilds its own wave from toggles), + a souffle noise layer; the four **diffuseurs** are reverb/chorus sends (Métallique gong, Palme string-halo). The one cart whose dynamics live entirely in a separate lever, not the pitch hand. | `note_on`/`note_pitch`/`note_vol`/`note_cutoff`/`note_lfo`, `wave_set` (`INSTR_USER0`), `instrument_reverb`/`_chorus` |

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
| **napoleon radio** (`napoleon`) | *Napoleon Dynamite* — **artist station, 5 song archetypes** (genre-clash) | stolen-playbook chord brain (the dance "Canned Heat" / Kip's serenade "Always & Forever" / Swihart's deadpan score / "Forever Young" / "We're Going to Be Friends"); per-archetype voices/groove/tempo/form (engines swap per song); **first SUNG `INSTR_VOICE`** falsetto croon (`voice/croon`); new `square/toy-organ` (detuned-pair Casio), `tri/funk-slap`, auto-wah `epiano/funk-clav`; first-radio `INSTR_MALLET` glock + `INSTR_GUITAR` pizz-mute voicings; `solo.h` jam ribbon **whose voice + behavior change per archetype** |

Many stations also support **THE BAND** panel (press **B**) — a live timbre-audition
overlay to swap a chair's instrument mid-song. Candidate swaps:
[`design/radio-instrument-options.md`](../design/radio-instrument-options.md).

## Sequencers, loopers & note toys

| Cart | What it is | Built from |
|---|---|---|
| **drum machine** (`drummachine`) | A 16-step sequencer — the original sound-synth showcase. | step grid |
| **tracker** (`tracker`) | A real music tracker, after **LSDJ / the Dirtywave M8** — the first cart with song ARRANGEMENT. Phrases (16 steps of note+inst+FX) chain with per-entry TRANSPOSE (one riff serves every chord) into per-track song columns; 4 independent tracks (chains of different lengths = polymeter free); mnemonic FX (`ARP POR VIB VOL CUT PAN RET DEL HOP TPO`) ride the `note_*` API; 8 fixed presets so drums+melody interleave on any track; QWERTY/MIDI note entry via `keybed.h`; the document autosaves (`save_bytes`). Design: [`design/tracker-cart.md`](../design/tracker-cart.md). | `drummachine` beat clock + `keybed.h` entry + `note_on`/`note_glide`/`note_pitch` FX riding |
| **pocket groove box** (`pocketbox`) | The Wee Noise Makers **PGB-1** reimagined ([`../design/pocketbox.md`](../design/pocketbox.md)) — a **buttons-only** groovebox: 16 step keys + 4 modes + hold-combos + ONE touch strip, no knobs anywhere, with a real 128×64 "OLED" (header, dot page-indicator, one param per page). 8 fixed-role tracks (kick/snare/hat/bass/lead/**chords** + 2 drawn-wave tracks), each with a curated engine menu; 16 patterns per track (independent slots = polymeter) with length + link; per-step **p-locks, ratchets, Elektron-style trigger conditions** (25/50/75%/fill/X:Y); a **chord track** that CHD-mode bass/lead steps follow; per-track FX-bus pick (dry/drive/reverb/crush); hold-FX live layer (filter sweeps, tape dive, crush, beat-FREEZE, roll, fill); 12-part song mode. The wave tracks play single-cycle waves you **draw with a finger on the OLED** (the PGB-1's sampler, reimagined as `wave_set`). | `drummachine` beat clock, `tap`/`tapp` hold-combos, engine menus over `INSTR_*` + `instrument_harmonics`/`_timbre` macros, `wave_set`/`INSTR_USER0..3`, per-slot `instrument_drive`/`_reverb`/`_crush` buses, master `filter()`/`varispeed()`/`crush()`/`tremolo()`, `save_bytes` |
| **one note** (`onenote`) | A one-note funk groovebox (after BeatCraft Studio's "magic of one note") — kick/snare + ONE bass row that plays a *single* note; the song comes from RHYTHM, SWING, and TRANSPOSING the whole loop on an in-scale KEYBED (A S D F G H J K L, scale-locked, *queued* to latch on the next bass step). An XY pad shapes the bass tone (Moog ladder), a SUB toggle adds the −1-oct "lower wave". The "no melody, just move the one note" idea. | `drummachine` step-grid + in-scale keybed + XY pad, `INSTR_SAW` bass |
| **grenadier** (`grenadier`) | A triple voltage-controlled FILTERBANK drone/acid synth — a cart-side port of the **Grendel RA-99 Grenadier** (Rare Waves, 2020): three held voices on one root, each through its own resonant filter, swept together in a 2D ALPHA/BETA XY pad (a live frequency-response strip shows the three peaks move). RA-99 (LP+2×BP) / RA-9 (3×BP) layouts; in-scale keybed root; trapezoid VCO (MORPH); per-trigger CMOS RND; a GATE chug. The XY filterbank sweep no other cart does. | three `note_on` voices + live `note_cutoff`/`note_res`, `INSTR_USER0` trapezoid, in-scale keybed + XY pad |
| **kaoss** (`kaoss`) | An XY EFFECTS pad, after the **Korg Kaoss Pad / Kaossilator** — a self-playing techno loop you mangle with a finger on a big XY pad. Pick a PROGRAM (FILTER / ECHO / GATE / TAPE); the two axes drive that effect's two params; touch to engage, HOLD to latch. Only the engine's *ride-safe* effects are on it (filter / varispeed / echo / tremolo) so it never stutters — the performance masher the FX bus was waiting for (onenote/grenadier plug into it). | the master FX bus (`filter`/`varispeed`/`echo`/`tremolo`) + a built-in groovebox loop + XY pad |
| **euclid** (`euclid`) | A Euclidean rhythm box, after **Mutable Instruments Grids / the Euclidean sequencer** — spread k hits evenly across n steps (the `euclid()` Bjorklund helper) and the world's drum patterns fall out for free. Five nested rings (KICK/SNARE/HAT/OHAT/PERC), each with its **own** step count → polymeter (rings drift). No step grid: you DIAL each track's DENSITY + ROTATION (HITS/STEPS/ROT knobs, or ↑↓/←→) and the groove blooms/dissolves; SPACE = MUTATE nudges every density. The player-driven Grids no cart had. The rhythm companion to kaoss/onenote (its loop is what kaoss mangles — kept separate per the one-gesture rule). | `euclid()` helper + onenote/kaoss drum recipes (`INSTR_SINE`/`NOISE`/`TRI`) + `ui.h` knobs + nested-ring sequencer |
| **circle machine** (`circlemachine`) | Raymond Scott's **Circle Machine** (~1959, Manhattan Research Inc.) — a step sequencer a decade before the word; the pre-Roland-wing flagship (STATUS #21). TWO concentric rings of bulbs swept by rotating photocell arms; a bulb's BRIGHTNESS = its note (scale-quantized, brighter = higher), drag it dark for a rest. The rings run different step counts on INDEPENDENT phase — **HUMAN** adds analog timing wobble, **DRIFT** shoves them out of sync (they phase apart and lap back around), **SYNC** snaps them home; the two arms visibly fan apart. 3 voices per ring (RING bleep / GLIDE the Clavivox portamento / BONGO the Bandito auto-drummer); a **DIRT** knob = tape wobble + saturation + valve grime + crush; RINGMOD clang; NUDGE mutates the loop (the Electronium "suggest and steer"). Design: [`../design/scott-blind-brief.md`](../design/scott-blind-brief.md). | ring/`note_glide`/`INSTR_MEMBRANE` voices, per-ring phase accumulators (own arm), `tape`/`chorus`/`amp_noise`/`crush` DIRT bus, `ringmod`, reverb/echo **sends** (`instrument_reverb`/`instrument_echo`), `ui.h` knobs |
| **turing** (`turing`) | A generative sequencer, after the **Music Thing Modular Turing Machine** — one looping 16-bit SHIFT REGISTER + one CHAOS knob: each clock the register rotates and the recirculated bit flips with a probability (right = LOCKED loop, centre = RANDOM, left = FLIP-LOCK / double-length). Read out scale-locked → a pitch; bits 0/2 → kick/hat, so melody AND groove are the same evolving pattern. TAP the LEDs to flip bits by hand; a SEQUENCE strip graphs recent notes so you watch it lock or drift. The "self-evolving riff you sculpt" no cart had. | a cart-side shift register + `INSTR_SAW` (resonant LP) sequence voice + `INSTR_SINE`/`NOISE` kick/hat + `ui.h` knobs + LED row + history strip |
| **dub siren** (`dubsiren`) | A dub siren, after **King Tubby / Scientist** — a GESTURE box, not a melodic instrument. Two detuned held voices warble (`note_lfo(LFO_PITCH)` — the "wheeeoo") and fire into a huge FEEDBACK DELAY: stab, lift, and the `echo()` THROWS the tail into the distance; drag the FIRE PAD's X past the red and feedback crosses 1.0 → the delay self-oscillates and HOWLS. Y = pitch, reverb splash under it. 4 siren shapes; hold/SPACE/LATCH to fire. The hands-on twin of the `dub` auto-radio. | held `note_on` voices + `note_lfo(LFO_PITCH)` warble + `echo()` feedback throw (>1 self-osc) + `reverb()`; fire-pad XY |
| **lpg** (`lpg`) | A low-pass gate — the **west-coast / Buchla** signature voice. A real struck thing decays quieter AND duller at once; an LPG models that by closing a VCA + lowpass together from one springy vactrol envelope. 8 held marimba bars, each a tone gated by a per-bar envelope that drives `note_vol` + `note_cutoff` + `note_drive` (DRIVE_FOLD) in lockstep. COUPLE A/Bs plain-VCA ↔ full-LPG (the teachable bit), FOLD adds harmonics to gate, DECAY = plonk→ring; bars glow white→wood as the gate closes. Tap A-K. The one liveset plaything with a genuinely new timbre. | a pool of held `note_on` `INSTR_TRI` voices + coupled per-voice `note_vol`/`note_cutoff`/`note_drive` from a vactrol env |
| **dubdesk** (`dubdesk`) | A generative **dub groovebox** — the jam station fusing three liveset chassis over one clock. 3 euclid drum rings + a 4th MELODY ring that fires an **LPG voice** at a shift-register pitch (the rhythm + melody share one pattern brain and self-play); a **dub siren** FIRE PAD is your live hands, its X riding a **master echo** so a throw smears the whole mix (self-osc past the red). PART tabs (RHYTHM/VOICE/SIREN) re-target one shared 3-knob row; LPG also hand-played on A–K. The two-handed jam station the one-gesture rule kept out of any single cart. | euclid rings + `euclid()` · lpg held-voice pool + coupled vactrol env · dubsiren fire-pad + master `echo()` throw + `reverb()` · `ui.h` shared knob row. Spec: [`../design/dubdesk-spec.md`](../design/dubdesk-spec.md) |
| **easel** (`easel`) | A complete **west-coast / Buchla Music Easel** voice — the whole instrument the `lpg` gate belongs to. Brightness made ADDITIVELY: a **complex oscillator** (`INSTR_FM` — RATIO/TIMBRE cross-mod, live via `note_harmonics`/`note_timbre`) → **wavefolder** (`DRIVE_FOLD`) → **low-pass gate** (vol+brightness close together). A **function generator** (RISE/FALL) shapes it — PLUCK (tap fires it), PRESS (ribbon pressure rides the gate), CYCLE (free-run LFO → gate tremolo / timbre sweep). Played on a **pressure ribbon** (x=pitch snap/free, y=pressure) + A–K; RUN's pulser self-plays it. Mono v1. | a held `INSTR_FM` voice + coupled `note_vol`/`note_cutoff`/`note_drive` gate + a cart-side function generator + a pressure ribbon (the `ui_captured` guard keeps knob drags off it). Spec: [`../design/easel-spec.md`](../design/easel-spec.md) |
| **groovebox** (`groovebox`) | A self-playing melodic/progressive-house box that is the **home for the summed-bus effects** the pedalboard can't show (a chain runs one voice; this sums six looping parts into one master, then sculpts the whole mix) — also the sidechain acceptance test. A 6×16 grid over a lush sidechained PAD + a cascading arp lead; the rack rides PUMP (a **real summed-bus `sidechain()`** — the kick ducks the whole mix), GLUE (the same engine comp self-keyed, `glue()` — they share the master comp, exclusive in the UI), CRUSH (summed bitcrush), 3-band EQ, TAPE, SPACE (reverb as a real master `reverb_insert`), a bipolar `filter()` DJ-filter knob, + a master `fx_order` reverb↔crush toggle (crush the wet tail vs reverb a gritty mix). The acceptance test for effects-bus Increment D (sidechain/glue). | step grid (`drummachine` chassis) + `ui.h` rack + held pad |
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
3b. **Typing notes? Use the GarageBand keyboard map + `Z`/`X` octave** — never a bespoke
   layout (see the cross-cutting note above; copy `KBMAP[]` from `mt70`/`martenot`). And don't
   double-handle tap buttons with `mouse_pressed()` — the mouse is already a synthetic touch.
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
