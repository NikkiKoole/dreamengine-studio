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

1. ~~The mic ring was built for **analysis**, not for a sample-exact insert path. Latency and block
   alignment are unmeasured.~~ **MEASURED 2026-08-17, and it is a non-issue: 0 samples, constant,
   unity gain, nothing dropped** (`node tools/insert-latency.js`, probe cart `inslat`). The ring is
   transparent — it is a rate-adapting FIFO whose reader is aligned to its writer on start
   (`sound_extin_reset`) and then consumes exactly one sample per output sample, so with a producer
   and consumer clocked 1:1 the depth stays at zero. ⚠ **That zero is a property of push-then-render
   at 1:1**, which is precisely what an AU render block does (N in, N out, one callback) — so it is
   the right model, but the wiring must preserve the ordering, and a host whose block size does not
   divide the engine's frame is still unmeasured. Non-44.1k hosts are handled: `mic_input_push`
   linearly resamples to `SOUND_SAMPLE_RATE`, with a stateless fast path at 44.1k.
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

> **STATUS: WIRED on macOS 2026-08-17, and SILENT IN GARAGEBAND — see §4.1b below for the defect.**
>
> **The spike landed.** `apps/pedalboard` declares `"auType": "aumf"` (derived per app by
> `ios/au-identity.sh`, so an effect app and an instrument app share `project.yml`/`project-mac.yml`
> without re-typing each other), `TinyjamAU` declares an input bus when its own
> `componentDescription.componentType` is an effect, and the render block pulls the host's audio and
> pushes it through `de_audio_input` **before** rendering — the ordering the 0-sample latency depends
> on. `aumf tpdl Mpla "Mipolai: Tiny Pedalboard"` registers as an EFFECT and
> **`auval -v aumf tpdl Mpla` → AU VALIDATION SUCCEEDED**, including its effect render tests at
> 512/64/4096 frames and at 11025–192000 Hz, which is real coverage of the block-size and rate
> mismatch §3.1 listed as unmeasured. `Test MIDI` passes too — the `aumf` argument below, confirmed.
>
> ⚠ **ONE INSTANCE.** The extin ring is still process-global (§8 Q2), so a second effect instance is
> two producers on a single-producer ring. Fix that before shipping.
> ⚠ **Not yet judged by eye or by a DAW** — `auval` is not GarageBand, and `au-transport-check`
> structurally cannot host an effect (its rig feeds the input nothing; the transport and `--panel`
> gates now SKIP for an effect type rather than fail misleadingly).
> ⚠ **iOS is untouched** — this is the Mac Catalyst path only.
> One `auval` warning worth a look: *"Can Initialize Unit to un-supported num channels: InputChan:4,
> OutputChan:5"* — we accept a channel layout we do not support, because the AU implements no channel
> capability restriction. It passes, but a host could hand us a 4-in config and be surprised.

If we do the input bus at all, declare `aumf`. Same wiring cost, strictly more useful: you get the
host's audio *and* its notes, which is what makes "the visuals know what the track is playing"
possible rather than only "the visuals react to loudness". `pedalboard` becomes an insert effect,
which matters because it is already **on sale** (`READY_FOR_SALE` since 2026-08-17), so the audience
exists before the plug-in does. Both prerequisites this paragraph once listed have since landed: the
type is derived per app (§6.2) and the spike is built (§4.1b holds the one remaining defect).

### §4.1b OPEN DEFECT: it loads, the panel works, and no audio comes through

**Observed by the maker, 2026-08-17, GarageBand on macOS.** Tiny Pedalboard appears in the track's
Plug-ins list under the E-Piano, the panel draws at a sensible size, `GTR: IN` is lit and `AUTO` is
`off` — and playing the piano produces **silence**.

**What this rules out**, which is most of the surface:

- `aumf` is visible to GarageBand and instantiable by it (the risk we thought was likeliest — dead).
- The view/panel path works in a real host, which no gate here could prove.
- The audio path itself is sound: `auval -v aumf tpdl Mpla` SUCCEEDS including effect render tests at
  512/64/4096 frames and 11025–192000 Hz. **auval feeds real input**, so pull → ring → output demonstrably
  works *somewhere*.

**Why the symptom is clean rather than muddy:** with `AUTO: off` the cart generates nothing of its own,
so the monitored input is the ONLY possible sound. Silence means the input is not arriving at the
output — it is not masking or mixing confusion.

**Diagnose in this order.** The first two are cheap and would settle it:

1. **Is the input pull succeeding?** The render block skips silently when
   `pull(...) != noErr`, which produces exactly this symptom and says nothing. Add a `deDiag` counter
   (pull failures, and the peak of `inMono`) and read it back with
   `/usr/bin/log show --last 2m --predicate 'eventMessage CONTAINS "[tinyjam]"'` — note `/usr/bin/log`,
   because zsh shadows `log` (CLAUDE.md). **This is the highest-value next step**: it splits "the host
   never gave us audio" from "we got audio and lost it downstream", and those have disjoint fixes.
2. **Did `input_monitor` actually take in THIS instance?** `GTR: IN` lit only proves the cart's own
   `guitar_in` flag flipped. The cart is set-and-hold via an `ap_gtr_in` shadow, so if anything zeroed
   `extin_mon_gain` after the cart applied it (`sound.h:8334` does exactly that in a reset path), the
   cart would never re-apply and the lamp would stay lit over a dead monitor. Trace `extin_mon_on` /
   `extin_mon_gain` / `extin_on` in the plug-in instance.
3. **Rate mismatch.** We pass the host's rate to `de_audio_input`, and `mic_input_push` resamples when
   it differs from 44.1k. Check GarageBand's actual rate; if it is 48k, the resampler path is running
   and `rs_q`/`rs_prev` are in play.
4. **Ordering.** The pull sits before the render deliberately (that is what makes latency 0), but the
   engine renders in whole 735-sample cart frames while GarageBand hands us its own block size. The
   ring should absorb it — verify it is not systematically underrunning and returning `0.0f`.
5. **CHANNEL COUNT — a concrete cause for suspect 1, and the one with real evidence behind it.**
   `auval` warned *"Can Initialize Unit to un-supported num channels: InputChan:4, OutputChan:5"*: we
   ACCEPT layouts we do not support, because the AU implements no channel-capability restriction. The
   render block allocates `AudioBufferList.allocate(maximumBuffers: 2)` and assumes two mono buffers
   (`inABL.count >= 2` → average, else copy channel 0). If GarageBand negotiates anything other than
   2-channel deinterleaved, the pull can fail into a buffer list that cannot hold what the host wants
   to write — **and a failed pull is skipped silently**, which is exactly the observed symptom.
   Fixing it properly means declaring channel capabilities so a host cannot negotiate a layout we
   mishandle, rather than widening the buffer and hoping. Log `inABL.count` and the negotiated input
   bus format alongside suspect 1's counter and this either falls out immediately or is eliminated.

⚠ **Do not conclude anything from `au-transport-check`** here: its rig gives an effect no input, so it
renders silence by construction. Both it and `--panel` now SKIP for effect types for that reason.

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
| **AU display name** | `project.yml:121` `CFBundleDisplayName` | ✅ **since 2026-08-16**: `auDisplayName`, falling back to the app's own `name` |
| **AU component name** | `project.yml:135` `name:` | ✅ **since 2026-08-16**: `auName`. **This is the string a DAW shows in its plug-in list** |
| AU manufacturer / subtype codes | `project.yml` | ✅ **since 2026-08-16**: `auSubtype` / `auManufacturer`, the `auval` triple |
| **anything at all on the dev loop** | `device.sh` | ❌ nothing. A cable install is always `TinyjamHello` |

So: over the cable you always see the hello-world identity, and in a DAW you always see
"Tinyjam: Demo" whichever app you built. The icon is the one thing that is already right, provided
you go through `APP=<name>` rather than a bare single-cart `play.js` staging (which stages
`ios/default-icon.png`).

⚠ **And a bigger one found while reading this:** the AU extension is **opt-in via the manifest's
`auCart` key**, and `apps/tinyacidjam/app.json` did not set it. So Tiny Acid Jam was shipping as a
standalone app with **no plug-in at all**. **FIXED 2026-08-15** (`"auCart": "acidcandy"`), which is
necessary and NOT sufficient: see the correction and the collision immediately below.

### §6.1 Correction: macOS was already right, and that is where the collision was — DERIVED 2026-08-16

The table above is the **iOS** spec. `ios/project-mac.yml` already carries the correct per-app
identity: `CFBundleDisplayName: Tiny Acid Jam`, subtype `tacj`, manufacturer `Mpla`, component name
`"Mipolai: Tiny Acid Jam"`. So the intended end state exists and is committed; it is `project.yml`
(iOS) that still says `Tinyjam Demo` / `tnyj` / `Tnyj` / `"Tinyjam: Demo"`.

**That makes turning on `auCart` for tinyacidjam a two-step job, not one line.** Those iOS codes are
*tinyjam's*, and `project.yml` is shared by both apps, so with `auCart` set on both manifests the two
apps would ship plug-ins claiming the **same component triple** `aumu tnyj Tnyj`. The mac spec's own
comment names this hazard exactly: *"DISTINCT 4-char codes … the two can be registered on one machine
and a collision would make the host pick whichever it saw first."*

So `project.yml`'s AU identity could not simply be swapped to `tacj`/`Mpla` (that just moves the wrong
name onto the other app). It had to be **derived from the manifest**, the way the app's own bundle id,
version and display name already are. **Done 2026-08-16** in `testflight.sh`: four manifest keys
(`auName`, `auSubtype`, `auManufacturer`, and `auDisplayName` which falls back to the app's `name`),
validated before any staging runs, then substituted into the derived spec.

```
▸ AU identity: "Mipolai: Tiny Acid Jam"  ·  aumu tacj Mpla  ·  shown as "Tiny Acid Jam"
```

**Tiny Jam is a deliberate no-op**: its manifest repeats `tnyj`/`Tnyj`/`"Tinyjam: Demo"` verbatim, so
its derived component triple is byte-identical to what it had before. That is the FOREVER rule in
practice, and it is why the keys are per-app data rather than something generated from the app name:
a DAW stores the triple in the saved project to re-instantiate the plug-in, so changing a shipped
subtype makes every project that used it come back with a missing plug-in and no way to reconnect it.
The name strings are display only and safe to improve whenever. (Tiny Jam's AU display name did move,
from `Tinyjam Demo` to `Tiny Jam`, which is the safe half.)

**What the derivation refuses**, each checked before the expensive staging step so a bad manifest
fails in a second rather than after a compile: a missing key when `auCart` is set · a code that is not
exactly 4 alphanumerics · an all-lowercase manufacturer (Apple reserves those) · a name containing
`\ & | "`, which would corrupt either the `sed` replacement or the YAML scalar it lands in. Then, after
substituting, it asserts the four **expected values are present** in the derived spec rather than that
the dev-loop strings are absent: for Tiny Jam those two are the same text, so an absence check would
either fail on a correct build or be skipped for it, and "skipped" is how a guard goes quietly blind.

`DERIVE_ONLY=1 APP=<name> ./testflight.sh` runs the whole derivation and stops before xcodegen,
printing the AU block it produced. No Xcode, no archive, about a second. That is what makes the
validators testable.

⚠ **A guard here was green and blind, and only mutation-testing found it.** The all-lowercase
manufacturer check was first written `case "$AU_MANUF" in *[A-Z]*)`, which passed a plain `mpla`:
a shell bracket **range** is collation-based, and under a UTF-8 locale `[A-Z]` contains the lowercase
letters too. Reading it will not show you this. Feeding it a deliberately bad value will. The fix is
POSIX classes (`[[:upper:]]`, `[[:alnum:]]`), and the same trap applies to every `[A-Z]`/`[a-z]` range
in a shell script in this repo.

✅ **The child App ID was a non-issue, and only a real archive could say so.** With the AU target
present, cloud signing needs `com.mipolai.tinyacidjam.TinyjamAU` as well as the parent, and
`testflight.sh`'s header names that as the reason single-cart standalones were cheaper to ship. Ran
it (`SKIP_UPLOAD=1 APP=tinyacidjam ./testflight.sh`, 2026-08-16): `-allowProvisioningUpdates`
registered the child and minted its profile with no manual portal work. The archive verifies:

| | value |
|---|---|
| app | `com.mipolai.tinyacidjam` · "Tiny Acid Jam" · v1.0 |
| extension | `com.mipolai.tinyacidjam.TinyjamAU` · "Tiny Acid Jam" |
| component | `aumu` · `tacj` · `Mpla` · "Mipolai: Tiny Acid Jam" |
| view | `NSExtensionPointIdentifier: com.apple.AudioUnit-UI` (so the panel, not host sliders) |

✅ **`device.sh` derives the same names now** (2026-08-16), into a COPY of the spec
(`project-dev.yml`, gitignored) so `project.yml` is never rewritten and a bare `xcodegen generate`
still yields the plain dev-loop project. The dev bundle id stays the throwaway `com.tinyjam.hello`.
⚠ The AU subtype/manufacturer follow the manifest there too, so a cable build registers the SAME
component triple the store build will. Deliberate (you are testing the real plug-in identity), and it
means a dev install can shadow a store install of the same app in a host's list. Install one, not both.

⚠ **What actually blocked the first archive was neither of those.** `build-app.js` staged nothing at
all: `SOUND_CART_CTX` had moved out of `sound.h` into the generated `sound_ctx.h` during the
per-instance context work, and the parser still grepped `sound.h`, so **every app build** died at
`could not parse SOUND_CART_CTX from runtime/sound.h`. The message names the symbol and blames the
wrong file, which reads like engine corruption rather than a stale path. It searches both homes now.
The lesson for this doc: the store path had a fatal break that no gate in the repo could see, because
nothing runs `build-app.js --ios` except a human about to ship. Reasoning about the archive would
never have found it; running it found it in four seconds.

⚠ **One real thing left, and it is about the icon rather than the plug-in.** Staging warns that the
mask is cutting real detail: *"worst: bottom-right, 45.4% of its cut region is not flat background"*.
iOS shaves ~6% off a square icon and Apple's corner curve is continuous where a hand-drawn rounded
rect is circular, which is the trap `docs/design/app-icon-mask.md` exists for and which has already
bitten two apps in review. See it with `node tools/icon-mask.js preview apps/tinyacidjam/icon.png`.

~~**The fix shape**, if we take it: derive the AU identity from the manifest…~~ **Done, above.** This
paragraph proposed the work that the rest of §6.1 records as shipped on 2026-08-16, and survived the
edit that shipped it. Struck rather than deleted because a proposal left standing next to its own
completion is the exact drift this doc keeps getting caught by, and it is worth being able to see one.

### §6.2 The TYPE was hardcoded, and that blocked an effect build — SOLVED 2026-08-17

✅ **Fixed the same day this was written, by `f62d4e64`**, in the shape proposed at the foot of
this section: `apps/<app>/app.json` carries an `auType` key, derived and asserted like the other
four, defaulting to `aumu` so every app shipped before it is a no-op. `apps/pedalboard/app.json`
now reads `"auType": "aumf"`. The record of the defect follows, because it is the sixth instance
of one shape and that shape is the point.

Verified 2026-08-17 by reading the specs, not assumed. The identity work of §6.1 derived the
*subtype*, *manufacturer* and *names* per app. It did **not** touch `componentType`, which is
still the literal `aumu` in five places:

| where | line |
|---|---|
| `ios/project.yml` · `project-store.yml` · `project-dev.yml` · `project-mac.yml` | `- type: aumu` |
| `ios/AU/Info.plist` | `<string>aumu</string>` |
| `ios/au-identity.sh` | the echoed summary, `aumu $AU_SUBTYPE $AU_MANUF` |
| `ios/mac.sh` | `auval -v aumu tacj Mpla` |

So **the shape of the plug-in is a property of the shared spec, not of the app**, which is the sixth
instance of the one shape behind every store-path failure: *a per-app value hardcoded in a spec every
app shares*. It went unnoticed because every app we have shipped so far wanted the same value.

It stops mattering the moment two apps want different shapes, and that is now: an effect build has to
declare `aumf` (or `aufx`), so `apps/<app>/app.json` needs an `auType` key defaulting to `aumu`,
derived and asserted exactly like the other four. That is a prerequisite for §4.1, not a follow-up —
and note `mac.sh`'s `auval` line has to follow, or the validator will look up a component that is no
longer there and report a plug-in that builds fine as missing.

### §6.3 `pedalboard` ships no plug-in at all — OPEN

`apps/pedalboard/app.json` sets no `auCart`, so `testflight.sh` strips the AU target (*"no AU
extension (manifest sets no auCart) — single-cart standalone build"*). **Tiny Pedalboard went on sale
on 2026-08-17 as a standalone app with no AUv3 in it**, which is the same ship-blocker caught for Tiny
Acid Jam on 08-15, in the same week, on the app nobody re-checked because it was already through
review.

**Still true as of 2026-08-17, and now on purpose rather than by oversight.** The manifest carries
`auType: aumf`, `auSubtype: tpdl`, `auManufacturer: Mpla` and `auName`, and no `auCart`. So the
plug-in is fully specified and deliberately unshipped.

That hold is worth keeping until §4.1b closes. A component triple is FOREVER: a DAW stores it to
re-instantiate the plug-in from a saved project. Nothing has shipped `aumf tpdl Mpla` yet, so it is
still free to change. Turn `auCart` on and the first version a buyer installs is silent on an insert,
which is the one thing this plug-in exists to do.

⚠ **A local macOS build does not need `auCart`.** `ios/mac.sh` registers the AU on this machine
directly, which is how the plug-in can be loaded in GarageBand today while the App Store version
contains no extension at all. Those two facts look contradictory and are not.

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

1. ~~Is the mic ring's latency acceptable for an insert effect?~~ **ANSWERED 2026-08-17 — yes, 0
   samples and constant** (§3.1 caveat 1; `tools/insert-latency.js`). No separate input path is
   needed. What the measurement *reframed*: the blocker for an `aumf` was never latency, it is
   question 2 below, which the measurement work made concrete.
2. **THE ACTUAL BLOCKER** (promoted 2026-08-17, once latency turned out to be free). Does
   `de_audio_input` become per-instance, and what happens to the mic's "one device per process"
   reasoning when the same seam serves N effect instances? The per-instance work deliberately left
   this whole path SHARED, and `tools/ctx-classification.json` wrote down why in terms that name
   their own expiry: `mic_h.shared` says *"ONE CAPTURE DEVICE per process… **Revisit if an instance
   ever needs its own mic routing**"*, and the extin group is filed as *"the plug-in has no mic path
   at all… never exercised multi-instance today"*. An `aumf` insert is exactly the condition both
   sentences were waiting for — each instance's input is its own track. Concretely, `mic_input_push`
   holds `rs_q`/`rs_prev` as **function-local statics** (`runtime/mic.h:116`), the class a `#define`
   cannot fix — the declarations have to move.
   ⚠ **Correction (same day, found while wiring §4.1):** an earlier draft of this entry said "at
   44.1k the fast path is stateless, so two instances are safe today". **That is wrong**, and the
   reason is worth keeping because it is the more general shape of the bug. `rs_q`/`rs_prev` are only
   the resampler's *private* state; the RING ITSELF is shared — `extin_mon_on`/`extin_mon_gain` moved
   into the per-instance context but `sound_extin[]`, `extin_w`, `extin_r` and `extin_on` did not. It
   is a SINGLE-producer/SINGLE-consumer ring by construction, so two effect instances are two
   producers racing on `extin_w` **and** two consumers each eating samples the other needed —
   garbled at **any** sample rate. `extin_on` compounds it: one instance switching its monitor off
   clears the shared flag and stops the other's writes. So the unit of work is the whole extin group,
   not two stray locals. Reading half a moved group and concluding the rest was fine is exactly the
   half-moved-group hazard this refactor warns about elsewhere.
3. Does the MIDI-out ring **replace** the CoreMIDI virtual source in a plug-in build, or run
   alongside it? (Alongside means an appex publishing a system-wide port, which we have flagged as
   uncovered.)
4. One extension target per type, or one container app shipping several? Several is the common
   pattern and costs little beyond target boilerplate, but it multiplies the identity problem in §6.
5. Does an `aumi` want the *whole* rack panel, or a stripped sequencer-only face? The rack draws five
   machines whose voices would be silent.
