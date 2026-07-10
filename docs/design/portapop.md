# portapop — the bedroom 4-track (a cassette multitracker cart)

> **Cart slug: `portapop`.** A `porta-` wink at the Tascam Portastudio (whose name is a
> trademark) that also names the genre — bedroom-pop. "Portastudio" below refers to the
> real Tascam hardware that inspired this; the cart itself is `portapop`.

> **STATUS: READY (2026-07-10).** Fully specced — every design fork below is resolved
> (the ✅ list in "Open questions parked"); no blocking 🤔 remain. Ready to build, nobody
> on it yet. Not started.
>
> **What it is.** The Tascam Portastudio reimagined as a cart (slug `portapop`): a little
> cassette 4-track you record a bedroom band into, one part at a time.
> Kin: [`input-recording-looper.md`](input-recording-looper.md) (the looper core this
> is built on), [`radiophonic-workshop.md`](radiophonic-workshop.md) (the other
> "perform onto tape" box), [`tinyjam-racks.md`](tinyjam-racks.md) (lane format / song
> handoff). Reference detail for the looper mechanics lives in the first of those —
> this doc is the *portapop-specific* thinking (the console, the tape, the band).

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

- **The longest take sets the song; shorter tracks loop to fill.** ✅ (DECIDED 2026-07-10 —
  replaces the earlier "first take sets it," which was too rigid.) Each track keeps *its
  own* length. The **song window = the longest track**, recomputed live; a track shorter
  than the window **repeats to fill it**. So a 4-bar bass cut first, then a 16-bar piano
  over it → the song grows to 16 bars and the bass loops 4× under the piano. Zero setup —
  you still just start playing — but you're no longer locked to whatever you played first.
  - **Why this shape:** it's the natural bedroom layering (a tight bass/drum ostinato
    under a longer evolving part), and it *kills* the old redo-track-1 special-case — there's
    no single locked number, the song is always "as long as the longest track." Shorten the
    piano to 8 bars and the song shrinks to 8 with the bass looping 2×. Predictable.
  - **The loop is *within one pass*, per track — the song does not cycle forever.** A pass
    has a length (= the longest track); each shorter track repeats inside that pass; you
    record/play one window-pass at a time. Not loopstation (one fixed quantized ring) — this
    is variable per-track lengths, un-quantized, performed.
  - **Looping is a *choice*, not imposed.** Want a bass with real feel across all 16 bars?
    Play 16 bars — that take stays a performance. Looping is the convenience for foundational
    ostinato parts; full-length takes stay as-played (see the next bullet).
  - **"Loop to fill" is only the default fill.** Want the bass to *drop out* for the middle
    (silent through the bridge, back in for the last chorus)? That's **arrangement, not the
    recording model** — you carve it in CONSOLE mode (mute a section of the track's timeline /
    punch a hole), the same place you mix and bounce. The take model just decides how a track's
    events map onto the window; *where a track plays or rests* is a console job. Parked as an
    intended console feature, not a recording-model concern.
  - 🤔 **Wrinkle (accepted, not engineered away):** non-integer relationships — a 4-bar bass
    under a *5*-bar part gets chopped mid-phrase at bar 5 (loops 1.25×). Snap track lengths to
    whole bars and let it be; a 5-over-4 is the player's own doing, and "warts and all" covers it.
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

## The band — a curated shelf of 8, cast 4 (DECIDED 2026-07-10)

Not a super-fixed band, not a free-for-all swap — **a curated shelf of eight voices you
cast four of** per song. It's the legible middle between "the Portastudio *is* one
specific band" (too rigid — you play it once) and "pick any instrument per track" (a
formless toy). Eight is enough to make two songs feel like different bands; four tracks
still forces the arrangement discipline that is the whole point. The vibe target is
bedroom-pop / indie-lo-fi *real* (Alex G · Mac DeMarco · Sparklehorse · Elliott Smith ·
early Beck — recorded small, close, nothing pristine). **Every voice already ships — this
is casting, not building.**

| Voice | Reuse chassis (exists) | Surface / how you play it | Seat it fills |
|-------|------------------------|---------------------------|---------------|
| **Upright bass** | **`upright`** (INSTR_BOWED modeled string) | fingerboard: pluck a string, slide to walk frets, pull to bend; pizz · arco · slap | bass (acoustic, woody, behind the beat) |
| **Synth bass** | **`pdbass`** (INSTR_PD, Casio-CZ phase distortion) | the *same* fingerboard, but a true smooth glide both ways + a CLEAN→BUZZY timbre slider | bass (electric, the 303-ish alt) |
| **Guitar** | **`guitar`** (INSTR_GUITAR — bodied plucked string) | grab a string to bend, sweep open space to strum; wonky-warble from **`tape()` wow + a hair of `instrument_tune` detune, NOT `chorus()`** (maker taste) | chords + jangle lead |
| **Piano** | **`piano`** (StifKarp) or **`epiano`** (Rhodes/Wurli/Clav) | one-octave keys, glissando by slide | keys / harmonic bed |
| **Hex scale grid** 🔷 | **`scalegrid`** (isomorphic, scale-locked — *no wrong notes*) | pad grid with a VOICE chip (CZ pad / Rhodes / vibes / organ / pluck); phone-first | melody or chords for non-players |
| **XY smear-lead** 🌟 | **`tapeloop`** (drawn Mellotron strings/choir/flute) or bowed saw, on a Kaoss-style pad (`kaoss` = the XY-surface prior art) | **gesture track**: drag the pad, y = pitch, x = swell → recorded as a CC stream, drawn as a ghost hand | the melancholy held-smear lead |
| **Drum grid** | **`drummachine`** (16-step sequencer) | program a pattern on a step grid, edit live | kit (programmed) |
| **Finger drums** | **`fingerdrums`** (5 kits, velocity-by-position) | tap pads by hand, every finger independent | kit (performed) |

The shelf **pairs on purpose** — two basses, two kits — so the natural cast is *one bass
+ one kit + two melodic*, but nothing forces it (a two-bass song is allowed, if daft).

🌟 The **XY smear-lead** is the one to fight for: it's what makes the cart *sing*, and it
forces the gesture-lane (CC-stream) design so not every track is three-more-discrete-notes.
It's the loopstation theremin recast as the band's lead — this is also your "played by an
XY pad" seat.

🔷 The **hex scale grid** is the "anyone can play" seat — scale-locked, no wrong notes, and
already phone-native.

✅ **Drum grid vs. take model — keep BOTH, on purpose (DECIDED 2026-07-10).** They're not
redundant, they serve **opposite musical wants**: sometimes you want a dead-tight machine
beat (**`drummachine`** grid — a *quantized loop*, which is exactly the point of it), and
sometimes you want loose, human, behind-the-beat drumming (**`fingerdrums`** — a *loose
as-played take*). So the take model holds both cases: a grid voice lays a **quantized
repeating pattern** into the window (the one deliberate exception to "don't quantize"),
and a played kit records **as-played, warts and all** like every other take. Pick
programmed *or* performed drums per song — that's the shelf pairing doing its job.

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
loses the separate faders). ✅ **DECIDED 2026-07-10: real ping-pong** — the bounced track
keeps playing while you overdub the freed ones, not a one-shot "print." It's the
characterful answer and the whole point of the mechanic (you keep *making the record*
while the bounce plays under you). The cost it accepts: the replay voices must survive
the merge — the flattened lane keeps firing its `note_*` events with the bounced mix
baked in (level/pan/fx printed into the events), while the freed lanes re-arm clean.

---

## Layout — a mode flip: play the take, then mix the tape (DECIDED 2026-07-10)

**Two modes, not one crammed screen.** The instrument surface and the console both want
room, so the cart flips between them following what you're actually doing:

- **TAKE mode (you're playing).** The **armed instrument fills the screen** — the upright
  fingerboard, the scale grid, the finger-drum pads, whatever you cast into that track.
  A thin **Tascam topbar** rides on top: transport (⏺ ⏵ ⏸), the tape counter, which track
  is armed, and its VU. Enough deck to know you're recording onto tape; the rest is the
  instrument. This is where the performing happens, so the instrument gets the pixels.
- **CONSOLE mode (you're done — "full Tascam").** Flip to the whole cassette-deck: the four
  channel strips (fader / pan / VU / arm / mute / solo), the spinning reels, `varispeed` +
  `tape`, and bounce. Where you listen back, mix, arrange, and decide "keep it or punch it
  again." This is the show — the machine as the thing you play.

The flip is automatic on the take boundary (stop a take → land in CONSOLE to hear it back;
arm a track → drop into TAKE for that instrument), with a manual toggle too. This also
solves the phone case for free — a phone was always going to need the flip.

## Console-mode sketch (very rough)

Cassette-deck chrome — legible to a stranger on sight (the ADR-0022 "delightful to a
stranger" half, for free: everyone knows a 4-track).

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

(The play surface isn't sketched here — it's just whichever cast instrument's own cart
surface, full-screen, under the thin Tascam topbar. See TAKE mode above.)

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
- ✅ **Take length?** → **the longest take sets the song window; shorter tracks loop to
  fill it** (recording-model section) — replaces the earlier "first take sets it."
- ✅ **Fixed band or per-track picker?** → **a curated shelf of 8, cast 4** (see band
  section) — the legible middle, not one rigid band and not free swap.
- ✅ **Drum grid vs. the take model** → **keep both** — the grid lays a quantized loop
  (tight machine beat, the one exception to "don't quantize"), `fingerdrums` records a
  loose as-played take. Different musical wants; pick programmed or performed per song.
- ✅ **Redoing the first take** → moot under "longest take sets the song" — the song is
  always as long as the longest track, so a redo just changes that track's length and the
  window recomputes. No special-case, no locked number.
- ✅ **A click track / count-in?** → **yes, a count-in** — no grid, but a count-in gives
  you the "1, 2, 3, 4" to come in on (especially cutting a take against existing tracks).
- ✅ **Bounce: one-shot print vs. real ping-pong?** → **real ping-pong** (bounced track
  plays under you while you overdub the freed lanes; see the four-track/bounce section).
- ✅ **One screen vs. mixer/play mode flip?** → **mode flip** — TAKE mode (armed
  instrument full-screen + thin Tascam topbar) while you play, CONSOLE mode (full Tascam
  deck) when the take's done (see the Layout section). Solves the phone case for free.
- ✅ **Where does it sit vs. `tracker` and the racks?** → confirmed distinct: tracker =
  document-first song authoring; racks = generator-first; loopstation = perform-a-loop.
  Portastudio = **perform-a-song-onto-tape** — the missing "record a band in your
  bedroom" corner none of the others fill.
- ✅ **Name?** → **`portapop`** (DECIDED 2026-07-10). A `porta-` wink at the Tascam
  Portastudio (trademark — cf. the Roland-trademark rule in
  [`product-notes.md`](product-notes.md)) that also names the genre target head-on
  (bedroom-**pop**). Not a copy of the trademark; says what the cart is *for*.
