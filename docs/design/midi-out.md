# MIDI out — carts as controllers / sequencers

> **The whole outward surface in ONE table** — what ships, what is open, and how it compares to
> ReBirth: [`external-clock-sync.md` → The whole outward surface](external-clock-sync.md#the-whole-outward-surface--what-ships-what-is-open-and-the-rebirth-comparison).
> Read that first; this doc is the OUT half only.

> ⚠ **The virtual source is the STANDALONE path.** A plug-in's MIDI reaches a host through
> `MIDIOutputEventBlock`, and the engine has no seam for that at all, so `acidcandy`'s finished
> sequencer output cannot be recorded by a DAW that loads it. That gap (and the `aumi` MIDI-processor
> product it blocks) is surveyed in [`auv3-plugin-types.md`](auv3-plugin-types.md) §3.2.

> **STATUS: BUILDING (idea 2026-07-11; demand-confirmed 2026-07-19; the SEAM SHIPPED 2026-08-13).**
> `runtime/midi_output.h` exists and is gated: a cart can send notes, CC, bend and clock/transport
> out through a CoreMIDI **virtual source** (the same call on macOS and iOS, so desktop and the
> phone→Ableton case are one implementation). Seven `midi_send_*`/`midi_out_ready` functions are in
> `studio.h`, `midiout` is the demo + gate driver, and `tools/midi-check/run.sh` asserts the
> bytes on real CoreMIDI from a second process, negative control included.
> **MIDI CC *in* shipped the same day** — the one dropped `else if` this doc's sibling kept
> deferring — and it is **channel-aware**, which notes deliberately are not
> ([why](#the-channel-map-and-why-drum-voices-are-notes-not-channels)).
> **Still open:** wiring a rack to it (the target is **`acidcandy`/Tiny Acid Jam**, not `acidrack`
> — that is where the shipping app is), the **slide encoding** decision, an AUv3 MIDI-out path
> (model B), and MPE in either direction. Sibling of the input layer —
> [`midi-and-keybed.md`](midi-and-keybed.md). A 2026-07-19 r/ipadmusic drip put real numbers behind
> it — see [Demand evidence](#demand-evidence-from-the-r-ipadmusic-drip).

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

## The channel map, and why drum voices are NOTES, not channels

Asked by the maker 2026-08-13, in the form "it kinda makes sense if it would send MIDI on various
channels? it has 4 instruments, 2 of those are drum machines with various voices inside too." The
instinct is right and the arithmetic is the interesting part: the answer is **4 channels, not 4+27**.

**The rule.** A *pitched* part gets a channel of its own, because on that channel a note number
means a pitch. A *drum machine* gets **one** channel on which each voice is a fixed **note number** —
not a channel per voice. That is how every drum machine addresses a DAW, and it is why a receiving
drum rack lights kick/snare/hat instead of three arbitrary pitches. Twenty-seven channels would also
overflow the sixteen MIDI has.

**`acidcandy`'s five machines** (`M_303A, M_303B, M_808, M_909, M_MST`) therefore map:

| machine | ch | what goes out |
|---|---|---|
| 303 A | 1 | pitched notes; accent → velocity; slide → **open**, see below |
| 303 B | 2 | same |
| 808 | 10 | its 16 `TR_*` voices as 16 GM notes |
| 909 | 11 | its 11 `TR9_*` voices as GM notes |
| MST | — | **nothing.** It is a master bus, not a sound source |

Channel 10 is GM's reserved drum channel, so the 808 lands where a DAW already looks; two drum
machines cannot both be 10, so the 909 takes 11 (worth making settable).

**Both rosters land on exact GM homes, with nothing left over** — which is not luck: GM's percussion
map was modelled on these machines.

| 808 (`TR_*`) | GM | 808 | GM | 909 (`TR9_*`) | GM |
|---|---|---|---|---|---|
| BD | 36 | CLAV | 75 | BD | 36 |
| SD | 38 | CP | 39 | SD | 38 |
| LT | 41 | MA | 70 | LT / MT / HT | 41 / 45 / 48 |
| MT | 45 | CB | 56 | RS | 37 |
| HT | 48 | CY | 49 | CP | 39 |
| LC / MC / HC | 64 / 63 / 62 | OH | 46 | CH / OH | 42 / 46 |
| RS | 37 | CH | 42 | CC / RC | 49 / 51 |

**The API consequence, caught before it shipped.** This doc originally proposed
`midi_send_note(note, vel, on)` — *no channel argument*. Fine for a single-voice keybed cart, useless
for a rack, and a five-place retrofit once it is in `studio.h`. Every `midi_send_*` therefore takes
`ch` **first**, 1..16 as printed on gear (the wire's 0..15 conversion happens once, inside
`midi_output.h`, so no cart ever writes `ch - 1`).

**And it applies symmetrically to CC *in*.** `midi_input.h` masks the channel nibble off
(`d[k] & 0xF0`) — fine for notes, where a keybed cart has one voice and the channel is noise, but
wrong for knobs: automating 303A's cutoff separately from the 808's is exactly a per-channel
question. So CC carries the channel (`midi_cc(ch, cc)`, `ch 0` = omni) while notes stay omni, and the
shipped `midi_get()` was left alone.

**Still open here:** the **slide** encoding (overlapping note-ons requiring a legato/mono receiver,
versus an explicit portamento CC — target-dependent, so it wants testing against real synths, and it
is the one decision this section does not make), and whether a muted machine keeps sending. Mute is a
*mixer* function, so the defensible default is that MIDI keeps flowing — muting locally while an
external synth plays the part is a real workflow — but it will surprise someone either way.

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

## What it cost us (engine surface — SHIPPED 2026-08-13)

- **`runtime/midi_output.h`**, sibling to `midi_input.h`, compiled inside `studio.c` the same way.
  CoreMIDI **virtual source** (`MIDISourceCreate` + `MIDIReceived`) on macOS *and* iOS — one
  implementation for desktop and the phone→Ableton case. Windows/web = harmless stubs.
  Allocation-free per send (stack packet list), so a cart may send from the audio thread, which is
  where `update()` runs inside the AUv3.
- **Seven `studio.h` functions** in the usual four places: `midi_send_note/cc/bend/clock/start/stop`
  + `midi_out_ready`. All channel-first (see the map above).
- **Two design calls worth not re-deriving:**
  - **Lazy init.** The source is created on the *first send*, never at startup — otherwise every
    one of 578 cart runs would publish a "dreamengine" port into the user's MIDI setup, including
    headless ones.
  - **Automated runs do not send.** `midi_out_automated` mirrors `sync_automated` (headless,
    screenshot, scripted/replayed/ui-audited) and suppresses output unless `--midi-out` is passed.
    Without it a `build-all` sweep or a gif bake would fire notes into whatever DAW the dev has
    open. ⚠ The assignment is kept **outside** any `#ifdef` on purpose — see
    [the inert-guard bug](#the-inert-guard-found-while-building-this) below.
- **Gate:** `tools/midi-check/run.sh` — a real CoreMIDI listener in a second process asserting
  channel/note/velocity/controller, plus a **negative control** (the same run without `--midi-out`
  must publish nothing, which is the only way to tell "correctly gated" from "not gated at all").
  Needs no IAC bus and no DAW, unlike [`sync-spike`](external-clock-sync.md).
- **Still open:** wiring a rack to it, and the **slide encoding** decision above.

### The inert guard, found while building this

Worth recording because the shape recurs. `sync_automated` — the flag that stops an automated run
consulting a real external clock — had its assignment sitting **inside `#ifdef DE_SPEC`**. Only
`spec.js` defines `DE_SPEC`; `play.js` (`--headless`/`--script`/`--replay`) and the screenshot bake
build with `-DDE_TRACE` alone. So in every run the flag was written to protect, it stayed `0` and the
real clock *was* consulted — the exact nondeterminism
[`external-clock-sync.md`](external-clock-sync.md) documents as fixed, still live, with the fix one
`#ifdef` away.

It hid because `--midi-clock` runs were unaffected (`sync_frame`'s synthetic branch wins before the
`automated` branch is reached), and those are the runs the sync gates exercise. **The lesson for any
"is anyone watching this run" guard: the gate must include a case where the guard is the only thing
that can produce the result** — which is why the MIDI-out gate carries a negative control.

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
- [`audio-input-frontier.md`](audio-input-frontier.md) — the wider "engine hears *and speaks*"
  map this output direction lives on.
