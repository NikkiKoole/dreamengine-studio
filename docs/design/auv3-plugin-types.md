# AUv3 plug-in types — the five shapes a cart could take in a DAW

> **STATUS: EXPLORING — ★ MAP (2026-08-15).** The engine ships ONE plug-in shape (`aumu`, an
> instrument) and the AUv3 ecosystem has five. This doc is the survey: what each type is, what
> dreamengine would be as each one, and precisely which seam is missing per type. Written after
> discovering that `acidcandy` is already a complete MIDI sequencer whose output cannot reach a
> host, and that `runtime/platform.h` already declares the exact seam an audio-effect plug-in needs.
> Nothing here is committed work. It is the map you read before picking the next AUv3 lane.
>
> See also [`ios-plan.md`](ios-plan.md) (the build ladder that got us here),
> [`host-parameters.md`](host-parameters.md) (the knobs a host can automate),
> [`host-midi-notes.md`](host-midi-notes.md) (what a host's keybed does to the rack),
> [`midi-out.md`](midi-out.md) (the cart-as-sequencer design this doc's §4.2 completes),
> [`audio-input-frontier.md`](audio-input-frontier.md) (the mic seam §4.1 would reuse),
> [`external-clock-sync.md`](external-clock-sync.md) (the transport half, shipped).

## Why this exists

The AUv3 work so far treated "the plug-in" as one thing: get `acidcandy` into GarageBand and make it
behave. It does behave, and that was the right first goal. But `componentType` is a four-character
code with five musically distinct values, and each one puts the cart in a **different slot in the
host's signal chain**, in front of a different set of users, competing with a different shelf.

We picked `aumu` at the start because a groovebox is obviously an instrument. That was correct and
it is also the *only* one we have looked at. Three of the other four are close, and one of them is
close because a cart we already shipped does the work already.

## §1 The five types

The code lives in `AudioComponentDescription.componentType`, and it is the `kAudioUnitType_*`
constant spelled as four ASCII bytes. The mnemonic is `au` plus two letters for the kind, which
is worth spelling out because the names mislead:

| code | constant | expands to | audio in | audio out | MIDI in | MIDI out |
|---|---|---|---|---|---|---|
| `aumu` | `kAudioUnitType_MusicDevice`  | **au** + **mu**sic device | no | yes | yes | via block |
| `augn` | `kAudioUnitType_Generator`    | **au** + **g**e**n**erator | no | yes | no | no |
| `aufx` | `kAudioUnitType_Effect`       | **au** + e**f**fe**x** | yes | yes | not reliably | no |
| `aumf` | `kAudioUnitType_MusicEffect`  | **au** + **m**usic e**f**fect | yes | yes | yes | via block |
| `aumi` | `kAudioUnitType_MIDIProcessor`| **au** + **mi**di processor | no | no | yes | yes |

⚠ **`aumf` is MUSIC EFFECT, not "MIDI something".** The `m` is music, the `f` is effect. It is the
type for an effect that also wants notes: a MIDI-triggered freezer, an arpeggiating filter, a
visualizer that reacts to the chord being played as well as to the sound. It is very often the type
people mean when they say "aufx", because `aufx` historically has no MIDI at all and hosts remain
inconsistent about an `aufx` that declares some.

**The code must agree in three places or the host silently will not list you.** For us those are
`ios/AU/Info.plist` (the `NSExtensionAttributes` `AudioComponents` array, line 45), the component
description in `ios/project.yml` (line 132) and `ios/project-mac.yml` (line 123), and the bus
configuration in `ios/AU/TinyjamAU.swift` (`_inputBusArray` / `_outputBusArray`, ~line 100).
"It does not show up in AUM" is nearly always one of those three disagreeing.

## §2 What we are today, verified

`aumu`. `ios/AU/Info.plist:45`. One output bus, and **an explicitly EMPTY input bus array**
(`TinyjamAU.swift:102`), so the plug-in cannot hear anything the host plays.

What already works, all of it verified on device or through a gate:

- The panel draws and takes touches in GarageBand on macOS and iPadOS.
- Host transport in (`runtime/sync.h`, `de_sync_position`), so the rack follows the DAW.
- Host notes in (`de_midi_event`), mapped through the PITCH lens, gated by `tools/midi-note-check/`.
- Pitch bend and CC in (`de_midi_bend`, `de_midi_cc`).
- Session state, with migration (`de_save_state`, `tools/state-check/`).
- Host parameters (`runtime/param.h`, `param_bind`), 21 bound in `acidcandy`.
- Two instances are two independent racks (the per-instance context work).

That is a well-behaved instrument. Everything below is about the other four slots.

## §3 The seams, per type

| type | what we already have | what is missing | size |
|---|---|---|---|
| `aumu` | everything above | nothing structural | shipped |
| `augn` | same as `aumu` minus the notes | nothing, but also no reason: `aumu` with nobody playing it is the same product | skip |
| `aufx` | `de_audio_input(const float*, int, int)` already declared in `runtime/platform.h:215` and exported in `ios/Sources/engine.h` | the AU declares no input bus; nothing calls `de_audio_input` from the render block | small |
| `aumf` | as `aufx`, plus the note/CC path is already wired | same input bus, plus the `Info.plist` type change | small |
| `aumi` | **`acidcandy` already sends its entire pattern as MIDI**: 303a on ch 1, 303b on ch 2, 808 GM on ch 10, 909 GM on ch 11, with accents, slides, note-offs, gate lengths, clock at 24ppqn and start/stop (`acidcandy.c:2540`+, `runtime/midi_output.h`) | **the engine has no MIDI-OUT seam for a host at all.** `midi_output.h` publishes a CoreMIDI *virtual source* directly, which is the standalone path. `ios/Sources/engine.h` has audio-in, MIDI-in, bend, CC and transport, and no way for the AU to pull events OUT. The AU has no `MIDIOutputEventBlock` and no `midiOutputNames` (grepped: zero hits) | medium |

### §3.1 The audio-input seam is already the right shape

```c
void de_audio_input(const float *mono, int n, int sample_rate);  // host → engine: captured frames
```

The engine deliberately never opens a capture device: each host owns its mic and **pushes** frames
in. An `aufx`/`aumf` render block receives exactly that, a buffer of host audio. So the wiring is
"call the function we already export, from the render block, instead of from a mic callback".

What comes alive for free if that lands:

- `mic_level()` / `mic_pitch()`, so **the visuals react to the host's track**.
- `input_monitor(gain)` routes it through the whole master FX chain, which makes **`pedalboard` a
  real insert effect** rather than a standalone app.
- `vocoder_mic`, `autotune_mic`, `harmonize_mic` become host effects.
- `scope_read()` then sees the processed result, so the visualizer sees what the effect did.

Three honest caveats before anyone budgets this as trivial:

1. The mic ring was built for **analysis**, not for a sample-exact insert path. Latency and block
   alignment are unmeasured. An envelope follower does not care; a guitar pedal does.
2. `input_monitor` is marked **LIVE** (ADR-0032), so it breaks `.rec` replay and no determinism gate
   covers it. An effect plug-in is a live-only product by nature, but that means the gates we lean
   on elsewhere will not be there.
3. `de_audio_input` carries `seam-lint-ignore: one capture device per process`. That reasoning
   (one mic per process, so no instance handle) is **wrong for an effect**: two effect instances on
   two tracks each have their own input. Making this per-instance is part of the job, not a detail.

### §3.2 The MIDI-out seam is the one real gap

`acidcandy` is already a finished sequencer. Read `mo_303`, `mo_drum`, `mo_transport`, `mo_clock`:
slides become overlapping notes, accents become velocity, drum hits get a real gate length, quitting
never strands a held note. It sends to a CoreMIDI virtual source called "dreamengine".

In a DAW that is the wrong destination. A plug-in's MIDI goes to the host through
`MIDIOutputEventBlock`, so the host can record it, route it to another track, and keep it in the
project. Ours goes out a side door to a system-wide virtual port, which in an app extension is at
best awkward (the handoff already flags "K same-named CoreMIDI virtual sources" as uncovered).

So the work is a new engine seam, roughly:

```c
// engine → host: drain the MIDI this instance produced this block, for the AU's output block
int de_midi_out_drain(DeInstance *in, DeMidiOut *dst, int max);
```

plus a ring behind `midi_send_*` that the AU can pull instead of (or as well as) the CoreMIDI push.
`midi_output.h` already tracks held notes per channel for its shutdown flush, so it has most of the
bookkeeping. The cart side is **zero**: `mout_on` already gates it and the pattern already streams.

## §4 The three product shapes, ranked

### §4.1 `aumf`, not `aufx`

If we do the input bus at all, declare `aumf`. Same wiring cost, strictly more useful: you get the
host's audio *and* its notes, which is what makes "the visuals know what the track is playing"
possible rather than only "the visuals react to loudness". `pedalboard` becomes an insert effect,
which matters because it is already an app in review.

### §4.2 The `aumi` sequencer, the sleeper

A MIDI processor that plays the user's *other* plug-ins: their Moog emulation, their sampler, their
expensive orchestral library. Why it fits this repo specifically:

- **Near-zero CPU in the host.** That is the constraint that kills every fancy visual idea, and this
  shape does not have it.
- **The panel becomes the entire product**, and the panel is what we are good at. A MIDI FX slot
  full of grey lists is a low bar to clear with a fantasy console.
- **A much emptier shelf** than the synth slot. Fewer things to be compared against.
- **Both directions are already gated** (`tools/midi-check/`, phase A drives a real CoreMIDI
  listener in a second process).
- It is a second product out of a cart that already shipped.

### §4.3 The visual coat of paint on what we have

Not a new type at all, and the cheapest of the three. `scope_read()` / `scope_read2()` hand cart-land
the real post-FX master, and `wavecandy` already runs a 2048-point FFT, a spectrogram waterfall and a
goniometer on it. That is a visualizer that exists; give the shipping rack a full-panel performance
view and the App Store screenshots change completely.

## §5 Visual ideas that are cheap because of what is already here

These are the "trippy AUv3" ideas filtered through what the engine actually has, rather than what a
GPU shader would do.

- **The framebuffer is the wavetable.** `pget_rgb()` reads the canvas back (the documented
  feedback-shader path) and `wave_set()` is LIVE, so a ringing note morphs as the table is redrawn.
  Read a scanline or a spiral out of the framebuffer, push it into `INSTR_USER0`, and the tunnel you
  are looking at *is* the timbre. Perhaps a hundred lines, and genuinely novel as a plug-in.
- **Video feedback, honestly.** `pget_rgb` + `blend(BLEND_ADD)` + a rotated, scaled re-blit gives the
  infinite-tunnel effect with no shader. The chunky indexed look reads as demoscene rather than as a
  Winamp plugin, so the low-res constraint is an asset.
- **Palette cycling as a modulation bus.** Copper-bar writhing at zero redraw cost. Blocked slightly:
  `palette_hex()` is flagged EXPERIMENTAL and testing-only in `studio.h:1020`, so shipping this means
  promoting it or going through `pset_rgb`.

**Not** `raymarch` / `shadermath.h` per-pixel work inside a host. It already carries a pixel-size
knob because it is heavy, and an AUv3 sharing a CPU with a DAW is the worst place to spend it.
Measure with `profiler_request` before believing otherwise.

## §6 Identity: what a host shows, and what is hardcoded today

Separate topic, same root cause, and it is why a deployed build shows the wrong names.

**`ios/project.yml` is the DEV-LOOP identity, and only some of it gets patched per app.**

| what | where | patched from the manifest? |
|---|---|---|
| app bundle id | `project.yml:48` `com.tinyjam.hello` | ✅ `testflight.sh` seds it to `app.json` `bundleId` |
| marketing version | `project.yml` | ✅ sed |
| app display name | `INFOPLIST_KEY_CFBundleDisplayName` | ✅ `testflight.sh` passes `app.json` `name` |
| app icon | `ios/gen/Assets.xcassets` | ✅ `build-app.js --ios` stages `app.json` `icon` (and runs the mask check) |
| **AU display name** | `project.yml:121` `CFBundleDisplayName: Tinyjam Demo` | ❌ hardcoded. `testflight.sh`'s own header says "the AU keeps its own hand-authored Info.plist" |
| **AU component name** | `project.yml:135` `name: "Tinyjam: Demo"` | ❌ hardcoded. **This is the string a DAW shows in its plug-in list** |
| AU manufacturer / subtype codes | `project.yml` | ❌ hardcoded, and they are the `auval` triple |
| **anything at all on the dev loop** | `device.sh` | ❌ nothing. A cable install is always `TinyjamHello` |

So: over the cable you always see the hello-world identity, and in a DAW you always see
"Tinyjam: Demo" whichever app you built. The icon is the one thing that is already right, provided
you go through `APP=<name>` rather than a bare single-cart `play.js` staging (which stages
`ios/default-icon.png`).

⚠ **And a bigger one found while reading this:** the AU extension is **opt-in via the manifest's
`auCart` key**, and `apps/tinyacidjam/app.json` did not set it. So Tiny Acid Jam was shipping as a
standalone app with **no plug-in at all**. **FIXED 2026-08-15** (`"auCart": "acidcandy"`), which is
necessary and NOT sufficient: see the correction and the collision immediately below.

### §6.1 Correction: macOS is already right, and that is where the collision is

The table above is the **iOS** spec. `ios/project-mac.yml` already carries the correct per-app
identity: `CFBundleDisplayName: Tiny Acid Jam`, subtype `tacj`, manufacturer `Mpla`, component name
`"Mipolai: Tiny Acid Jam"`. So the intended end state exists and is committed; it is `project.yml`
(iOS) that still says `Tinyjam Demo` / `tnyj` / `Tnyj` / `"Tinyjam: Demo"`.

**That makes turning on `auCart` for tinyacidjam a two-step job, not one line.** Those iOS codes are
*tinyjam's*, and `project.yml` is shared by both apps, so with `auCart` set on both manifests the two
apps would ship plug-ins claiming the **same component triple** `aumu tnyj Tnyj`. The mac spec's own
comment names this hazard exactly: *"DISTINCT 4-char codes … the two can be registered on one machine
and a collision would make the host pick whichever it saw first."*

So `project.yml`'s AU identity cannot simply be swapped to `tacj`/`Mpla` (that just moves the wrong
name onto the other app). It has to be **derived from the manifest**, the way the app's own bundle id,
version and display name already are: new manifest keys (`auName`, `auSubtype`, `auManufacturer`) and
a few more `sed` lines in `testflight.sh`. Until that lands, a Tiny Acid Jam TestFlight build with
`auCart` set will list itself in every DAW as **"Tinyjam: Demo"**.

⚠ One more thing the stripped path was hiding: with the AU target present, cloud signing needs the
CHILD App ID `com.mipolai.tinyacidjam.TinyjamAU`, not just the parent. `testflight.sh`'s header calls
this out as the reason single-cart standalones were cheaper to ship. First archive after this change
is where that shows up.

**The fix shape**, if we take it: derive the AU identity from the manifest the same way the app
identity already is (a couple more `sed` lines in `testflight.sh`, plus manifest keys for the AU
name and the four-char subtype), and give `device.sh` the display-name override so the dev loop
stops lying about which app it is. Small, mechanical, and worth doing before the next submission
rather than after.

## §7 Traps

- **Parameter addresses are forever.** A saved project's automation lane stores nothing else. Append,
  never renumber. `runtime/param.h` says it at length and it applies to every type here.
- **Hosts resize the plug-in view to arbitrary shapes**, sometimes a short wide strip. That is what
  `face.h` / `disclose.h` exist for; a new plug-in shape should start from the responsive grammar
  rather than a fixed canvas.
- **Multiple instances.** Extensions get a tighter memory budget than an app, and the host may load
  eight. The per-instance work makes this correct; it does not make it free.
- **Photosensitivity.** If any of §5 ships, ship a reduce-motion path by default and respect
  `UIAccessibility.isReduceMotionEnabled`. Review will ask, and it matters more than review does.

## §8 Open questions

1. Is the mic ring's latency acceptable for an insert effect, or does an `aumf` need a separate,
   sample-exact input path? (Measure before designing.)
2. Does `de_audio_input` become per-instance, and what happens to the mic's "one device per process"
   reasoning when the same seam serves N effect instances?
3. Does the MIDI-out ring **replace** the CoreMIDI virtual source in a plug-in build, or run
   alongside it? (Alongside means an appex publishing a system-wide port, which we have flagged as
   uncovered.)
4. One extension target per type, or one container app shipping several? Several is the common
   pattern and costs little beyond target boilerplate, but it multiplies the identity problem in §6.
5. Does an `aumi` want the *whole* rack panel, or a stripped sequencer-only face? The rack draws five
   machines whose voices would be silent.
