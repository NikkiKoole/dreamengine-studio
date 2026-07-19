# MIDI out — carts as controllers / sequencers

> **STATUS: EXPLORING (idea, 2026-07-11; demand-confirmed 2026-07-19).** Captured from a
> session sparked by "does MIDI out always make sense — e.g. in the 303?" Nothing built yet;
> there is **no MIDI-output path in the engine today** (`runtime/midi_input.h` is input-only
> and its v1 scope note explicitly defers CC/out). Current focus is the **303 (`acidrack`)**;
> the maker notes many other carts could want this. Sibling of the shipped input layer —
> [`midi-and-keybed.md`](midi-and-keybed.md). Deeper research deferred. A 2026-07-19 r/ipadmusic
> drip put real numbers behind it — see [Demand evidence](#demand-evidence-from-the-r-ipadmusic-drip).

---

## The question

We support MIDI *in* (a physical keyboard plays a keybed cart). Does it make sense for a
cart to send MIDI *out* — emit note/CC events that drive another instrument, in another
app or on the desktop?

**Not universally.** It depends on what the cart's *value* is:

- **Sound-source carts** (keybed instruments — epiano, moog, touchpiano, mellotron, combo):
  their value is the **timbre**, and they are *driven by* MIDI in. They have nothing
  meaningful to send out — the notes came from you, and the sound is the point. MIDI out
  here would just echo the input. **Skip.**
- **Sequence / gesture generators** (a 303, drum machines, arps, the radio-station improv):
  these **originate** note events. That pattern/gesture is genuinely worth sending
  elsewhere. **MIDI out makes real sense.**

So MIDI out is a **per-cart** capability, added to the carts that generate — not a global
engine default.

## Demand evidence from the r-ipadmusic drip

A [`reddit-gaps.js`](demand-discovery.md) drip of **r/ipadmusic** (2026-07-19; two
runs, the second widened with complaint-shaped probes: 267 threads, 155 wishes) ranked **MIDI
routing / control** as the tribe's single **loudest topic — demand 70** — while our shelf is
thinnest there (only ~18 carts touch it). It reads as a `GAP` by raw demand, but reading the
actual threads splits it cleanly along *this doc's* seam, which is why it isn't one we can ship
as a cart today:

**On-grain (already possible with MIDI *in* + a voice):** "iPad as a keybed / drum-pad I *play*"
— a physical keyboard drives a cart's timbre. `keybed.h`, `acidcandy`, `tb303` already do this.

**Off-grain (needs the MIDI-*out* seam this doc proposes — not built):** almost all the loud
asks. Verbatim from the cache:
- *Best ipad app as a midi controller* · *Midi controller app that uses TRS midi* · *make an
  ipad a drum pad for Logic* · *behaves like Novation's Launchpad with Midi Sync* — **model A**
  (iPad-as-controller-for-a-DAW).
- *Recommend a MIDI Sequencer to control my hardware synths and drum machines* · *iOS/iPadOS
  MIDI sequencing app* · *midi layered/zones live performance* — **sequence external gear**.
- *transpose incoming MIDI before it hits a synth* · *send MIDI CC while playing back audio* ·
  *MIDI Learn for changing presets* — **routing / transform utilities** (furthest off-grain;
  these are MIDI-processor apps, not instruments).
- ***A TB-303 app that can map knobs to MIDI hardware*** — the one thread that lands exactly on
  this doc's ✅ case (303 sequencer-out alongside the voice + CC from the knobs).

**Reading:** the demand is real and loud, but it is demand for a **capability, not a cart** —
it validates building the `runtime/midi_output.h` seam below (model A especially), and it does
**not** contradict the "pure keybed instruments stay input-only" rule: not one wish asked a
sound-source cart to *emit* MIDI. Until the seam exists, this topic will keep surfacing as a
phantom `GAP` in every r/ipadmusic drip — that's expected; the answer lives here, not in a new
cart.

**Corroborated (r/edmproduction, 2026-07-19):** a second tribe's drip ranked MIDI
routing/control among its loudest topics (demand 20, ours ~18), with the example thread
*"Is there any app that can turn my ipod touch into a midi controller"* — **model A** almost
verbatim. So the MIDI-out pull isn't a one-sub artifact; it recurs across music communities.

### MPE as *input* — the on-grain half we CAN ship (and did)

The one part of the "MIDI controller" demand that needs **no MIDI at all** is the *expressive
surface itself*. MPE (per-note pitch-bend + pressure + timbre) is only a **wire format** for
carrying per-note expression *between apps*. If a cart drives its **own** voices, there's no wire
to cross — and the engine already has per-note expression natively: `note_on()` returns a handle,
and `note_pitch()`/`note_cutoff()`/`note_res()` ride that one voice live. That **is** MPE's data
model, addressed by a voice handle instead of a channel. The touchscreen supplies the gesture
(`pointer.h` folds the desktop mouse in as one finger, so it's mouse- *and* multitouch-playable):
each finger → one handle, horizontal drag → `note_pitch`, vertical drag → `note_cutoff`/`note_res`.

- **Shipped:** [`ribbonpad`](../../tools/carts/ribbonpad.c) ("Ribbon") — a polyphonic MPE-style
  pad over a `FILTER_DIODE` acid voice, scale-snapped pitch with glide, Y = the filter opening.
- **The one honest gap:** iOS finger touch exposes **no per-finger force** (iPhone dropped 3D
  Touch; iPad never had it — only the Pencil). So MPE's *pressure* dimension can't be read; the Y
  axis carries timbre instead (the standard proxy). Pitch + timbre are real; pressure is faked.
- **Still a genuine seam (not built):** receiving MPE *from* an external controller (Roli/
  Linnstrument) *into* a cart — `midi_bend()` is a single global bend, not per-channel, so the
  input layer is MPE-*unaware*. That, and MIDI-*out*, remain the actual engine work.

## The 303 is the interesting case — it's *both* a sequencer and a voice

A TB-303 is a step sequencer **and** a synth voice, so there are two different "MIDI out"
ideas and only one is right:

- ❌ **MIDI out *instead of* the voice** — pointless. The whole reason anyone wants a 303 is
  the squelchy diode-ladder + accent/decay envelope. Send the notes out and mute the voice
  and you've thrown the instrument away. Nobody wants "a 303 that doesn't sound like a 303."
- ✅ **MIDI out *of the pattern*, running alongside the voice** — idiomatic and desirable.
  The 303's magic is as much its **sequencer** (the slide/accent pattern language) as its
  voice. Using the 303's weird step sequencer to drive a *different* synth while the 303
  also plays is a workflow people specifically buy hardware/clones for.

### The catch — encoding slide + accent

Plain note-on/off can't carry the 303's expression:
- **Accent** → maps cleanly to **velocity** (accented step = high velocity). Easy.
- **Slide** → is portamento/legato between overlapping steps. In MIDI that's either
  **overlapping note-ons** (receiver must be in legato/mono mode to glide) or an explicit
  **pitch-bend / CC5 portamento** ramp. There is no universal encoding — it depends on the
  target synth. **This is the real design decision**, not "should we add MIDI out."

## Two delivery models (they are genuinely different — don't conflate)

Both are valid; a cart like the 303 could do either. They share the same net-new engine
piece but reach the outside world differently.

### A. Standalone app → desktop DAW (the "hook the phone to Ableton" case)

The cart runs as a plain app on the phone (or desktop) and **streams MIDI to a desktop DAW**
(Ableton / Logic) over a transport the OS provides:
- **USB cable** — phone as a class-compliant USB-MIDI device. Lowest latency, most reliable.
- **Network MIDI (RTP-MIDI)** — both on the same wifi; a MIDI Network Session (macOS *Audio
  MIDI Setup → Network*, iOS via CoreMIDI). No cables, a few ms latency.
- **Bluetooth MIDI (BLE-MIDI)** — wireless, least setup, more jitter.

App-side: create a **CoreMIDI virtual source** (`MIDISourceCreate`) and push packets; the OS
+ transport make it visible, and the DAW selects it as a MIDI **input**. This is exactly how
iOS sequencer/controller apps work. **Common and fully possible.** Does **not** involve AUv3.

### B. AUv3 instrument-with-MIDI-out (on-device, inside a host)

The cart runs as an **AUv3 hosted on the device** (AUM/GarageBand/Cubasis — the path proven
by [`ios-plan.md`](ios-plan.md) spike 7). An instrument that *also emits MIDI* is a
first-class AUv3 shape: the host routes the pattern to another plugin. This reuses the
render-block MIDI plumbing already built — just the output direction. Note: desktop Ableton
**cannot** load an iOS AUv3 (hosting is same-device only), so model B stays on the phone;
reaching desktop Ableton is model A's job.

## What it would cost us (engine surface — not built)

- A new **`runtime/midi_output.h`** sibling to `midi_input.h`, compiled inside `studio.c` the
  same way. **Native/iOS = CoreMIDI** — `MIDISourceCreate` + `MIDISend` (the virtual-source
  API is the **same call on macOS and iOS**, so one implementation covers desktop *and* the
  phone→Ableton case). **Windows/web = harmless stubs** (web could later use Web-MIDI
  *output*, mirroring the input bridge in `runtime/web_midi.js`).
- A few `studio.h` functions — e.g. `midi_send_note(note, vel, on)`, `midi_send_bend(v)`,
  later `midi_send_cc(cc, v)` — added in the usual four places (studio.h/studio.c/studioDocs/
  shell) per CLAUDE.md.
- A cart-side convention for **which** events a generator emits, plus the **slide encoding**
  decision above (per-cart, since it's target-dependent).

## Where this could go (later)

- **303 (`acidrack`) first** — sequencer-out alongside the voice; accent→velocity; pick a
  slide encoding.
- **Other generators** — drum machines (per-voice note-out on a GM-ish map), arps, radio
  `improv.h` auto-solos. Pure keybed instruments stay input-only.
- Research to do when we pick this up: confirm the iOS virtual-source + network/USB paths on
  a real device against Ableton/Logic; settle the slide encoding by testing against a couple
  of target synths; decide whether CC-out (knob automation) rides along.

## See also

- [`midi-and-keybed.md`](midi-and-keybed.md) — the shipped MIDI **input** layer + `keybed.h`
  (this doc is its output-direction sibling).
- [`ios-plan.md`](ios-plan.md) — the AUv3 spike (model B's host).
- [`held-notes.md`](held-notes.md) — `note_on`/`note_off`/`note_pitch`, the voice lifecycle a
  slide would mirror when *emitting* MIDI.
