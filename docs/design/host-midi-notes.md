# Host MIDI notes on a multi-machine rack

> **STATUS: BUILDING (2026-08-15) — the top row is wired and gated; rows 2+ are still open.**
> A host note now transposes the acid lines, or plays the kit on a drum face, with no mode switch.
> Gated by `bash tools/midi-note-check/run.sh` (16 assertions, mutation-tested).
> Came out of the maker loading `acidcandy` as an AUv3 in GarageBand on the iPad: *"there is a
> keybed but that isn't doing anything."* See also
> [`midi-and-keybed.md`](midi-and-keybed.md) (the engine's note-input layer),
> [`midi-out.md`](midi-out.md) (the same rack in the other direction, and where the channel map
> comes from), [`external-clock-sync.md`](external-clock-sync.md) (the sibling host seam, and
> where the surrendered-control rule was learned), [`ios-plan.md`](ios-plan.md) (the AUv3
> extension), [`control-vocabulary.md`](control-vocabulary.md) (rule 6, a gesture nobody can see
> is not a control).

## The symptom, and where it is actually blocked

The AUv3 registers as `aumu`, a music device, so every host draws a keyboard above it
(`ios/AU/Info.plist:45`, `ios/project.yml:132`). Nothing in the plug-in says "I ignore notes",
so the host cannot know not to. Press a key and nothing happens.

This is the mod-wheel bug of 2026-08-14 one hop further along. That one was blocked in the AU;
this one is blocked in the cart. Every hop verified by reading, not remembered:

| hop | state |
|---|---|
| host sends note-on/off | ✅ it must, the component type says instrument |
| AU parses the event list | ✅ `ios/AU/TinyjamAU.swift:467-468` → `de_midi_event` |
| declared to Swift | ✅ `ios/Sources/engine.h:76` |
| engine pushes to the ring | ✅ `runtime/midi_input.h:69`, drained by `midi_get()` at `:82` |
| **the cart reads notes** | ❌ **`acidcandy` calls `midi_get()` / `midi_held()` zero times** |

So host notes arrive, land in the ring, and nobody drains them. **The plumbing work is zero.
All of the work is the musical decision.** (Bend and CC are already wired and mapped, 2026-08-14:
mod wheel to the master DJ filter, bend to the two 303 lines.)

## The one structural constraint: notes are omni, CC is not

`MidiEv` is `{type, note, vel}` (`runtime/midi_ctx.h:40`). There is no channel field.
`de_midi_push` has nowhere to put one, `de_midi_event(in, type, note, vel)` has no parameter for
it, and the AU masks it off. `midi_input.h:44-50` states the reason plainly: *"a keybed cart has
ONE voice, so which channel a note arrived on is noise ... no cart asked otherwise."*

**acidcandy is the first cart that asks.** A five-machine rack wants channel 1 to reach 303a, 2 to
303b, 10 the 808, 11 the 909, which is exactly the map it already **sends** on
(`MO_CH_*`, `acidcandy.c:2350-2353`). Making the input direction symmetric with the output
direction is the only engine-level change anywhere in this design: a `ch` field on `MidiEv`, a
parameter on `de_midi_push` and `de_midi_event`, one line in `engine.h`, one in the AU's
`case 0x90/0x80`. Small, but it touches the shared seam, so it gets its own decision rather than
being smuggled in behind a cart feature.

Not needed for GarageBand, which sends one channel. Needed for AUM, Logic, or any real
controller, and needed for the endgame where all five machines are addressable at once.

## The four behaviours are one axis, not four modes

Four plausible mappings surfaced. They look like a mess until you lay them out:

| | 303 face | drum face |
|---|---|---|
| **the pattern is the instrument** | notes transpose the running line | notes trigger the kit (GM map) |
| **the voice is the instrument** | notes play the acid voice, mono | notes play the selected voice, pitched |

The same distinction on both rows: *am I playing the sequence, or am I playing the sound?* So the
mode question is **one binary that means the same thing on every face**, resolved against the
focus the panel already shows loudly. One control, one meaning, four behaviours. That is a fair
amount to ask a player to hold.

### Row 1, left: transpose (`mroot`)

The ReBirth / TB-3 / Elektron move, and the one that makes this a plug-in rather than a toy
sitting in a plug-in slot: draw a progression in the DAW's piano roll and the acid line follows
the song. The hook is `mroot[i]`, already per-line, already in the note computation at
`acidcandy.c:2589`, already saved in the song blob at `:2245`.

Two things it drags in. `mroot` is also a **UI control** (the KEY panel, `:3162-3173`), so a host
driving it triggers the surrendered-control rule this cart has now learned three times: show it,
and register no widget so a finger cannot silently rotate what the host holds
(`host_knob_ext` is the extracted skin). And it is **inaudible while the transport is stopped**,
which is the multi-precondition trap the SPEAK entry named: test it stopped and it "does nothing"
for a fourth, different reason.

### Row 1, right: the whole kit under one keybed

**Yes, the entire drum machine is playable from one keyboard with no voice selection anywhere.**
`MO_GM808[]` and `MO_GM909[]` (`acidcandy.c:2358, 2363`) are already note-per-voice tables:
C1 kick, D1 snare, F#1 closed hat, all 16 voices live at once. The map is not a convention we
would be imposing. As the cart's own comment at `:2354-2357` says, GM's percussion map was
modelled on these machines, and both rosters land on exact GM homes with nothing left over. Any
drum-pad controller and any host's drum keyboard land on the right voices with no learning.

The bigger prize: route an incoming note through the same path as a voice-pad tap
(`:1622` for the 808, `:1803` for the 909) and **live MIDI step record comes for free**, because
the REC latch grammar already exists (lit + playing = fire and punch onto the current step). That
closes the standing "LIVE RECORD, still open" item in the cart's own todo list, via the MIDI path
rather than new UI.

### Row 2, right: one drum voice played chromatically

A genuinely different feature, not a variant: the tuned-808-kick bassline, cowbell melodies.
Partly built already. `tr808_fire` takes pitch as a per-hit parameter, and `tr808__midi()`
(`tr808.h:210`) resolves it as `base + (tune - 0.5) * 24`, so every hit is already
"this voice at this MIDI note". `TR_CLV` is literally the rimshot circuit retuned to midi 99.

Two honest limits:

- That path is **±12 semitones by design**, one octave either side of the voice's home, because it
  is shaped for a knob. A full keybed needs a wider entry point into `tr808_fire`, or a sibling
  that takes a MIDI note directly. Small, but it is a change to a header `tr808` and `acidcandy`
  both use, so it must stay byte-honest for the existing callers.
- It only means anything on voices that **have** a pitch. Hats, clap and the snare's noise
  component do not, so a third of the roster would retrigger at one sound. That is not a bug to
  fix, it is the shape of the feature: pitched play belongs to kick, toms, congas, cowbell,
  rimshot, clave.

There is a second tune path, `tr808_tune(base, v, semis)`, a live fractional trim through
`instrument_tune` (what the FINE microtune rides). It is continuous, but per-slot state rather
than per-hit, so a keybed on it would smear pitch across a running pattern's hits of the same
voice. The per-hit path is the right one.

### Row 2, left: the 303 as a live mono keyboard

`ac_note()` (`acidcandy.c:406`) is the single funnel every 303 trigger already goes through, and
it records `last_midi[i]` so the pitch bend has a base, so a host note is one call. The hard part
is arbitration with the sequencer, which fires the same line every step. `acid303` holds one
voice, so an overlapping host note reads as a slide, not a second voice. That is exactly the
problem `runtime/mono.h` solves (priority LAST/LOW/HIGH/FIRST, trigger SINGLE/MULTI/ANY);
acidcandy does not use it today, `sh101` drives it. Velocity has an obvious home: high velocity is
an accent, which the 303 pattern language already speaks.

## Where the binary lives, ranked

**1. Let the channel decide.** In a real host this dissolves the question instead of answering it:
one instance, four MIDI tracks, all five machines addressable at once, no switch anywhere. This is
what a groovebox on a DAW track wants to be. Costs the `MidiEv` channel change above, and
GarageBand cannot use it, so it never removes the need for 2.

**2. One rack-wide latch, resolved against the focused face.** The GarageBand answer, and the cart
already owns the grammar: TAP=latch / HOLD=momentary, the same as MUT and REC, so it costs no new
vocabulary. Name the hazard up front: **the sound then changes when you tap a cartridge**, which
is fine while playing with the panel and confusing while recording a take with the panel closed.
Under a host that argues for a rack-wide setting rather than a per-face one.

**3. Split the keyboard, so there is no mode at all. Checked, and it does not work.** The idea is
drums low, 303 high. The 909 alone would split cleanly (its roster spans notes 36 to 51). But the
808's Latin voices sit at their GM homes far up the keyboard: cowbell 56, congas 62-64, maracas
70, claves 75. The 808 kit spans **36 to 75, three octaves**, so the only honest cut leaves the
303 the top of the keyboard and nothing else. Written down because the idea looks like it should
work and will come back otherwise.

Also considered and rejected: **"stopped means play the voice, playing means transpose."** Modeless
and cute, and this cart's own history argues against it. The 4-way pad tool was deleted precisely
because it was a hidden meta-mode the maker forgot existed. A latch you can see beats a rule you
have to remember.

## Build order, and what to build first

**Build the whole top row, modeless, routed by the focused machine.** Not one cell of it. Row 1
is the only self-consistent half: on a 303 face notes transpose, on a drum face notes play the
kit, and **there is no mode switch at all**, because the switch only becomes necessary once row 2
exists. That gets host notes doing something musical on every face for the price of one decision,
and it defers the mode question entirely rather than answering it badly.

Within that, sequence it:

1. ~~**Drums first (row 1 right).**~~ **DONE 2026-08-15.** It was the increment that proved the
   wiring, and the only one of the four needing no mode, no engine change, no panel surrender, and
   audible with the transport stopped. Free rider, as predicted: live MIDI step record via the REC
   path.
2. ~~**Transpose (row 1 left)**~~ **DONE 2026-08-15**, in the same pass once the drum half proved
   the drain. Both open questions were settled as: **both lines** (they are a pair, and transposing
   one leaves the song half in the wrong key) and **absolute pitch class** (a note names the root,
   which is exactly what the KEY panel it overrides already means).
3. **The latch**, only once row 2 is actually wanted.
4. **Row 2** itself: the mono 303 (needs `mono.h` arbitration) and the pitched drum voice (needs
   the wider `tr808_fire` entry point).
5. **The channel field on `MidiEv`**, whenever a real DAW use case shows up, or sooner if it is
   wanted as the primary routing answer, in which case it moves to the front and step 3 may never
   be needed.

## What shipped (2026-08-15) — the top row, and the flag that made it gateable

Built as planned: the whole top row, no mode switch, routed by the focused face
(`host_face()` reads `face` on a phone and `r2_focus` in the roomy rack, since that view draws all
five machines at once).

- **Transpose.** `root_live(i)` replaces `mroot[i]` everywhere a note is computed. Held key wins by
  LAST-note priority over a small stack, so lifting the newer key falls back to the one still down
  rather than to silence. It **overrides rather than writes**, so `mroot` keeps what the player
  dialled and a MIDI take never dirties the saved song. Pitch class only, both lines together, and
  suppressed on a drum face so finger-drumming cannot silently re-key the 303s underneath you.
- **The kit.** `gm_voice()` is `MO_GM808[]`/`MO_GM909[]` read backwards, so input and output agree
  by construction. One deliberate divergence from the pad it otherwise mirrors: a host note **always
  sounds.** A pad tap is a select that sometimes sounds (silent while the transport runs, REC is the
  audible opt-in); a MIDI note is only ever a play, and gating it on REC would kill the keybed
  exactly when someone is jamming. With REC lit it still punches the step, so **live MIDI
  step-record came free**, which closes the cart's own standing "LIVE RECORD, still open" todo.
- **Both KEY panels surrender** while a key is held: no widget registered at all, host key drawn
  white against a dimmed strip. The roomy keyboard also gets the word "MIDI", in the grammar of the
  FLT knob's "MOD" and the tempo knob's "EXT". The phone face gets no word because there is no room
  for one at 160×100.
- **State went in `AcidScratch`, not `CartState`** — live host state is not intent, and the saved
  layout must not move or every existing project stops loading.

**`--midi-note` (`runtime/studio.c` → `midi_sched_add`) is the enabling piece and is worth more than
this cart.** Nothing could put a MIDI note into a headless run before it, so any cart's host-MIDI
path was gateable only through a real DAW on a real device. It pushes into the same ring
`de_midi_event` feeds, and a run that sets one skips `midi_input_init` so a keyboard left plugged
into the dev machine cannot make the gate flaky (sync.h's rule, pointed at notes).

**Two measurement traps, both hit while gating this, both worth more than the feature:**

- **`formant-check.js` reported the pitch going DOWN.** Its autocorrelation returns the ends of its
  own 80–501 Hz search range on a resonant acid saw, so both readings were the analyser failing, not
  a result. A broken oracle and a real answer print the same thing. The gate uses spectral centroid
  across a rising scale instead, **measured on the 303 stem** (`--solo-slot 6`): on the full mix the
  centroid sits at the hats and drifts the wrong way as the bassline rises, because what moves is
  the balance between two sources rather than the pitch of either.
- **The rack AUTOSAVES**, so run N+1 boots from run N's session and two "identical" renders come out
  different. That is not a flaky gate, it is the gate measuring two different racks. Clear
  `build/saves/acidcandy` before every render, or A/B this cart by hand and get two wrong answers.

The strongest assertion in the gate is the quiet one: **holding the panel's own root renders
byte-identical to holding nothing.** It proves the override is exact rather than merely present,
which "the audio changed" never could.

## Hazards to design against

**Timing is frame-quantized, not sample-accurate.** The AU pushes events in the render callback,
and the cart's `update()` drains them once per `de_frame` at 60 Hz (`TinyjamAU.swift`, step 2a).
A live-played note lands up to 16.7 ms late and the event's `AUEventSampleTime` is discarded.
Fine for transpose and for step-record punches (which quantize to the step anyway), audible but
tolerable for finger-drumming, genuinely wrong for anything expressive. Offline bounces run frames
inline, so at least a bounce matches what was heard live.

**MIDI out is on, so watch for a loop.** The rack already sends on channels 1/2/10/11. In a DAW
that echoes MIDI back to the plug-in, host-in plus cart-out is a feedback path. Whatever lands
must not re-emit what it just received.

**Show it on the panel.** Established three times now: the tempo knob under external clock, the
FLT knob under the mod wheel, the transport button while slaved. A control the host drives must
look driven and must register no widget. Host notes should light the note bars or flash the pads
(the `dtrig` ghost-pulse already does the second one), or the first report back is "it still does
nothing" for yet another reason.

## Gates

- `ios/Tests/AUHostTests.swift:83-86` already states this exact gap in its own comment (the
  acidcandy branch does not cover the note path, because a self-running rack sounds the same
  either way) and already drives `scheduleMIDIEventBlock` with a note-on at `:67`. That is the
  natural place to assert a note arrives and changes the mix.
- `zsh tools/midi-check/run.sh` phase C gates the engine-side note path end to end
  (`epianojam` to `epiano` to a WAV that must be loud), a different route to the same code.
- `ios/au-transport-check.swift` is the pattern for gating through a real out-of-process plug-in
  with a no-op control first. `--wheel` is the worked example, and its measured surprise applies
  here too: **peak is the discriminator, not onset count.**
- `ios/build.sh:30-32` already carries the note in a comment: `AU_CART=epiano` still gets you a
  keybed, from when the extension had no panel and could only be played by host MIDI.

## Open questions

Both of the original transpose questions are settled (see the build order). What is left:

- **The MST face transposes**, because it is not a drum face and the rule is "drums only on a drum
  face". That falls out of the design rather than having been chosen, and it is probably right (the
  master face is where a whole-rack gesture belongs), but nobody has played it.
- **Velocity on a drum note is a two-step ladder**: ≥100 accents (+2, the pattern's own accent),
  anything else is a pad tap's 1. A real controller sends a continuous 1–127 and this throws almost
  all of it away. A smooth map wants `tr808_fire`'s boost to mean more than three things.
- **Octave is discarded.** Deliberate (a keyboard has five and a 303 line has two useful ones), but
  it means the bottom and top of the keyboard are the same note, which someone will notice.
- **Nothing is shown on a drum face** when a host note fires beyond the pad flash the pattern
  already uses. Fine for now; it will read as "did that land?" the first time a note is off-map.
- **Rows 2+ and the channel field** are unchanged from the ranking above.
