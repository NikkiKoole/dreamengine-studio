# Portastudio — the bedroom 4-track (a cassette multitracker cart)

> **STATUS: EXPLORING / IDEA (2026-07-10).** A sketch, not a committed plan — we're
> sitting with it. Nothing here is decided; open questions are marked 🤔.
>
> **Genre: design exploration (scratchpad).** The Tascam Portastudio reimagined as a
> cart: a little cassette 4-track you record a bedroom band into, one part at a time.
> Kin: [`input-recording-looper.md`](input-recording-looper.md) (the looper core this
> is built on), [`radiophonic-workshop.md`](radiophonic-workshop.md) (the other
> "perform onto tape" box), [`tinyjam-racks.md`](tinyjam-racks.md) (lane format / song
> handoff). Reference detail for the looper mechanics lives in the first of those —
> this doc is the *Portastudio-specific* thinking (the console, the tape, the band).

---

## The one-line pitch

**loopstation grew up and moved into a cassette 4-track.** Four instruments, four
tracks, a mixer strip per track, a tape stage they all bounce through. You play the
band in one part at a time and it comes back as a warm, wonky lo-fi record.

Why it earns its own slot next to `loopstation`: loopstation is a **4-bar loop ring**;
the Portastudio is **linear tape with a counter** — a *song*, with arrangement,
punch-in, and a mixing console that is itself the thing you play. It's also the cart
that finally showcases the **per-slot mixer family** (`instrument_level` just landed —
the missing leg — alongside `instrument_pan`/`instrument_drive`/`instrument_echo`/
`instrument_reverb`), which no cart currently makes the star.

---

## The honest boundary (name it up front)

A real Portastudio records **audio** — any sound source — onto tape. **This engine has
no PCM capture path** (the [`input-recording-looper.md`](input-recording-looper.md)
"level 3", explicitly deferred; see also [`mic-and-sampling.md`](mic-and-sampling.md)).
So this cart records **control events** — the `note_*` calls the cart itself plays —
exactly like loopstation. Same level, same "songs fall out for free" upside.

What that means in practice:
- ✅ You can record/overdub/bounce/mute/arrange the **four built-in instruments**.
- ❌ You *cannot* record your voice, a mic, or an arbitrary external sound to tape.
- ❌ No true reverse-tape or resampling of rendered audio (that's the PCM-capture
  engine lift — noted as the far end, not this cart).

This is fine. The soul of a Portastudio for our purposes isn't "sample anything" — it's
**the four-track constraint + the tape sound + the console.** All buildable today.

---

## The recording model — takes, not loops (DECIDED 2026-07-10)

This is the heart of it, and it's **not** loopstation's auto-cycling loop ring. You're
the whole band, alone, **overdubbing one honest take at a time**: arm a track, count-in,
play a full pass, *stop*. Listen back. Arm the next instrument and play *along* to what's
already down. Separate in time, stacked in the mix — a solo musician cutting a demo.

Settled calls:

- **First take sets the length.** ✅ You just start playing; when you hit stop, that pass
  defines the song, and every later track records into that fixed window. No "set the
  bars up front," no grid to configure — the most tape-like, zero-setup option.
  - 🤔 Follow-on: redoing track 1 *before* other tracks exist can reset the length;
    once other tracks exist the window is locked (a redo of any track must fit it).
- **Don't quantize — keep the take as-played.** ✅ A take is a *performance*, warts and
  all. loopstation snaps sloppy timing tight (right for a groove, wrong for a take); the
  little rushes and drags are the lo-fi honesty — the whole reason a bedroom recording
  sounds human. Capture as-played; a gentle "feel" nudge at most, never a grid.
- **The core loop is take management:** record a track → hear it back → **keep it or
  punch it again** (re-arm, new pass replaces old). "Nah, one more time" is the gameplay.
- **Monitoring is the crux.** The thing that must feel great is playing along to the
  takes already down while cutting the next. That's the "separate but together."

So the machine's job: **be a patient tape deck you overdub yourself onto, one take at a
time.** (Because there's no loop and no grid, the transport — record / play / stop /
count-in — is the center of the cart, not chrome.)

---

## The band — four instruments, "indie lo-fi real"

The dream lineup first (bedroom-pop / Alex G · Mac DeMarco · Sparklehorse · Elliott
Smith · early Beck — recorded small, close, nothing pristine), then the shelf chassis to
reuse. Every voice already exists — this is casting, not building.

The classic four-piece shape: **kit · bass · chords · texture-lead**, which is also
three discrete-note tracks + one gesture track — the exact split loopstation proved.

| # | Seat | The vibe | Reuse chassis | How you play it |
|---|------|----------|---------------|-----------------|
| 1 | **Chords** | wonky detuned strummer — the harmonic bed | **`omnichord`** (Strumharpy — *the* bedroom-indie instrument) or **`guitar`**/**`mistress`** (janglier electric) | hold a chord, strum a touch strip; the wonky-warble comes from **`tape()` wow + a hair of `instrument_tune` detune** — **NOT `chorus()`** (maker taste: no chorus guitar). Character from the tape stage + grit/`instrument_drive` + a spring-ish reverb / `univibe` instead |
| 2 | **Bass** | soft, round, fingered, a little behind the beat | **`loopstation` saw/bass** block or **`moog`** | monophonic on a pentatonic strip / short keybed |
| 3 | **Kit** | boxy close-mic'd bedroom kit — small, dry, **no reverb** | **`cr78`** (soft drum-machine) or brushed kit + shaker from **`bossa`**/**`cocktail`** | 3 pads (kick / snare-or-rim / shaker); sloppy timing quantized back tight |
| 4 | **Texture-lead** 🌟 | wobbly held smear — the melancholy seat | **`tapeloop`** (drawn Mellotron strings/choir/flute) or a **bowed musical saw** | **gesture track**: drag the pointer (y=pitch, x=swell), recorded as a CC stream, drawn as a ghost hand |

🌟 The gesture voice (track 4) is the one to fight for: it's what makes the cart *sing*,
and it forces the gesture-lane (CC-stream) design so track 4 isn't just three more of
the same. It's the loopstation theremin, recast as the band's lead.

🤔 **Open:** fixed band vs. swappable per track? Fixed is more legible + tinier; a small
per-track instrument picker is more of a toy. Leaning **fixed** for the sketch (the
Portastudio *is* a specific band), but note the picker as the obvious "toy mode."

---

## The tape stage — what ties four voices into one record

The instruments aren't the soul; **the tape they bounce through is.** Four clean voices
→ one warm cassette. All of this exists today:

- **`tape()`** — wow / flutter / saturation on the master (the vintage warble + warmth).
- **`varispeed()`** — tape-speed dips on the transport (the tape-stop dive / chipmunk
  spin-up). Built to be swept live.
- A touch of **hiss** + the transport clunk for the diegetic feel.
- Per-track **VU meters** that actually bounce (the console being the show).

---

## The four-track constraint + bounce (the mechanic with the most soul)

Four tracks, and **bounce / ping-pong**: submix tracks down to one to free the others —
**committed, irreversible.** The limitation that shaped a generation of records;
constraint as creativity, which fits the "emergent from simple rules / honest core"
north star far better than an unlimited DAW.

At the control-event level, a bounce = **flattening several event lanes into one**, with
the "you can't un-mix it now" flavor intact (the bounced track keeps the *sound* but
loses the separate faders). 🤔 Do we let a bounced track keep playing while you overdub
the freed ones (real ping-pong), or is bounce a one-shot "print"? Real ping-pong is the
characterful answer; needs the replay voices to survive the merge.

---

## Layout sketch (very rough)

Cassette-deck chrome, everything on one screen — legible to a stranger on sight (the
ADR-0022 "delightful to a stranger" half, for free: everyone knows a 4-track).

```
┌──────────────────────────────────────────────────────────┐
│  ╔══════════╗   PORTASTUDIO           ⏺ ⏵ ⏸ ⏮ ⏭   [0123] │  ← transport + tape counter
│  ║  ◠◠◠◠◠◠  ║   ○ varispeed   ○ tape                       │
│  ╚══════════╝   (spinning reels)                           │
├────────┬────────┬────────┬────────┬───────────────────────┤
│ CHORDS │  BASS  │  KIT   │ SMEAR  │                        │  ← 4 channel strips
│  ▮ VU  │  ▮ VU  │  ▮ VU  │  ▮ VU  │                        │
│  ═╪═lvl│  ═╪═   │  ═╪═   │  ═╪═   │   ⟳ arm  ✕ clear        │
│  pan   │  pan   │  pan   │  pan   │   M mute  S solo        │
│ [PLAY] │ [PLAY] │ [PLAY] │ [drag] │                        │  ← the instrument itself
└────────┴────────┴────────┴────────┴───────────────────────┘
```

🤔 **Open:** the instrument-playing surface and the mixer strip both want screen room.
One-screen-shows-everything (above), or a mode flip between "mixer view" and "play the
armed track" view? A phone-sized version almost certainly needs the flip.

---

## What's already done vs. what's new

**Reuse wholesale (zero new work):**
- The looper core — `rec_ev` / `fire_replay` / `fire_ev` (~70 lines) from
  `loopstation.c`, plus the quantized-note / gesture-CC split.
- All four instrument voices (see the band table).
- The whole per-slot mixer API + `tape()` + `varispeed()`.
- `save_bytes()` → a serialized track list **is a song** (song-codec's customer).

**Genuinely new (the cart's actual design work):**
- The **mixing console UI** as a playable surface (faders/pan/VU per track).
- **Linear tape** (a counter + arrangement) instead of a fixed loop ring.
- **Bounce / ping-pong** on the event lanes.
- The cassette-deck **chrome + transport** feel.

🤔 Does any of this generate real pressure for engine surface (a bounce/merge helper, a
transport clock distinct from the loop clock)? Probably not — but that's the
[ADR-0006](../decisions/0006-library-carts-not-engine.md) question to answer *after* a
prototype, not before.

---

## Open questions parked (the "sit with it" list)

- ✅ **Loop or song?** → **song / takes** (see the recording-model section). Not a loop.
- ✅ **Take length?** → **first take sets it** (recording-model section).
- 🤔 **Fixed band or per-track picker?** (leaning fixed — see band section)
- 🤔 **Redoing the first take:** resets length while it's alone, locks once others exist —
  or is track 1 special/undeletable? (see recording-model follow-on)
- 🤔 **A click track / count-in on/off?** No grid, but a count-in still helps you come in.
- 🤔 **Bounce: one-shot print vs. real ping-pong?**
- 🤔 **One screen vs. mixer/play mode flip?** (phone forces the flip)
- 🤔 **Where does it sit vs. `tracker` and the racks?** tracker = document-first song
  authoring; racks = generator-first; loopstation = perform-a-loop. Portastudio =
  **perform-a-song-onto-tape** — the missing "record a band in your bedroom" corner.
- 🤔 **Name?** "Portastudio" is a Tascam trademark; the cart slug wants a wink-not-a-copy
  name (cf. the Roland-trademark rule in [`product-notes.md`](product-notes.md)).
