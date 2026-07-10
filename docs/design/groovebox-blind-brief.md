# Groovebox — the blind band brief (Phase 1)

STATUS: SHIPPED — phase-1 brief for the groovebox; groovebox.c is built and upgraded with real sidechain()/glue().

Written *before* opening any cousin cart (radio-station-howto.md firewall). This is **not**
a self-playing radio station and **not** a single-instrument-with-pedalboard. It's a
**playable multi-track groovebox** — a few looping parts summed into one master bus — and
its whole reason to exist is the **bus / summed-signal family of effects** that a serial
pedalboard structurally *can't* show off:

- **Sidechain pump** — voice A ducking to voice B's trigger. Needs a trigger input + the
  summed victim. The headline.
- **Glue / bus compression** — the band moving as one lump.
- **Summed bitcrush** — the SP-1200 grit that only exists on the *sum* (per-voice crush ≠
  crushing the sum; the inter-voice intermodulation is the sound).
- **Master EQ / tape / reverb / echo** — sculpting and spacing the whole mix at once.

The pedalboard already owns the **serial-insert** family (one instrument → a reorderable
chain). The groovebox owns the **summed-bus** family (many voices → one master → bus FX +
a sidechain trigger). Two genuinely different mental models → two different instruments;
they don't overlap. This doc names the ideal band + the effect-rack, which become the parts
and knobs we build.

## Why this vessel, in one line

You cannot demonstrate a pump with one held chord — the effect is *rhythmic*. A loop is the
only way to hear the mix breathe on every kick. So the machine must loop a few parts
hands-free, leaving both your hands free to ride the rack.

## The world, from the music (not the palette)

The genre that *invented* every effect on the rack is **four-on-the-floor electronic dance
music** — Chicago/Detroit house & techno, with side-trips into the textures that share the
summed-bus production grammar:

- **House / techno** — the four-floor kick is the clock and the sidechain trigger; the pad
  and bass *pump* against it; off-beat open hats carry the groove. This is the pump's home.
- **Acid** — a resonant, driven 303 bassline that screams as you ride the filter. (drive +
  resonant filter, already ours.)
- **Lo-fi / boom-bap** — the *summed* SP-1200 crunch: crush the whole mix, not each part.
- **Dub** — echo/reverb *throws*: send one stab into a long tail on cue.

The machine should let one loop slide between these by what you put on the rack, not by
switching songs. It's an instrument, not a jukebox.

## The ideal band (intent-first), per track

A compact, deliberately *summed-bus-shaped* lineup — every part either triggers the pump,
gets pumped, or feeds the master crunch:

| # | track | role | ideal voice | why it's here |
|---|---|---|---|---|
| 1 | **KICK** | the clock + the **pump trigger** | deep 808/909 four-floor thump, pitch-drop | every other voice ducks to this |
| 2 | **BASS** | the primary pump **victim** | rolling sub / driven acid 303 (resonant + drive) | bass pumping under the kick = the engine of house |
| 3 | **PAD / STAB** | the other pump victim | lush detuned-saw pad *or* a stabby organ/Rhodes chord | a pumping pad is *the* recognizable house gesture |
| 4 | **HATS / PERC** | the groove | crisp closed hat on the offbeat + open hat, a clap on 2&4 | the shuffle that makes a four-floor loop move |
| 5 | **LEAD / ARP** *(optional)* | top-end interest + the dub **throw** source | bright pluck / square arp | the voice you throw into the echo/reverb tail |

## Shopping the palette (grabbable recipes — do this at build, listed here for intent)

- **Kick / hats / clap** → reuse `drum/french-house-kick`, `drum/house-hats`,
  `drum/house-clap` (already cited by napoleon/italo). On-palette, dependable.
- **Acid bass** → `INSTR_SAW` (or square) through a resonant `instrument_filter` +
  `instrument_drive` — the 303 recipe. Ride the cutoff for the scream.
- **Pad** → the `saw/string-machine` / `saw/solina-ensemble` detuned-SAW pair already shared
  across air/italo/house/motorik — **reuse on purpose** (add groovebox to its *used by*).
- **Stab** → `INSTR_ORGAN` thin combo registration *or* a short Rhodes (`INSTR_EPIANO`).
- **Arp/lead** → `INSTR_SQUARE` pluck, short decay.

The point of the cart is **the rack**, not a new voice — so lean hard on reuse here. If a
gap appears, it's a small one.

## The effect rack — THE point of the cart

Live knobs over the **master** bus (and the pump). What exists today vs. what slots in later:

| rack slot | status today | how |
|---|---|---|
| **PUMP (sidechain)** | **faked cart-side** | per-frame duck of the pad+bass amplitude (and/or filter cutoff) keyed to the kick step — exactly house's trick ([`afrobeat-effects-wants.md`](afrobeat-effects-wants.md): "re-aiming filters per frame"). Knobs: **amount** + **trigger source** (default KICK) + **release**. **Swap for real `sidechain()` when it ships** — the knobs stay, only the wiring changes. |
| **CRUSH (SP-1200)** | **real, exists** | `crush(bits, rate, mix)` on the whole mix — the summed crunch. |
| **EQ** | **real, exists** | `eq(low, mid, high)` — sculpt the sum. |
| **TAPE** | **real, exists** | `tape(wow, flutter, sat)` — analog glue/warmth over the mix. |
| **REVERB / ECHO throw** | **real, exists** | per-slot `instrument_reverb`/`instrument_echo` sends — throw the lead/stab into a tail on cue (dub). |
| **CHAIN ORDER** | **real, exists** | `fx_order(0, kinds, n)` — reorder the master inserts (crush before vs after eq — audibly different on a full mix, unlike the pedalboard's single voice). |
| **GLUE (bus comp)** | **parked** | no engine yet; leave a labeled-but-inert slot (or a gentle fake) so the panel already has its home. |

> **This is the "nice home for upcoming effects" the build is for.** Sidechain and glue land
> here first; vocoder/ring-mod are a *different* (two-input) toy and stay out (see below).

## The play model (this replaces a radio's "dial")

- A small **step grid** per track (8 or 16 steps), tap to toggle. The loop runs itself.
- **One row of rack knobs** along the bottom — the hands-free loop frees both hands to ride
  them. The PUMP amount knob is the star: turn it up and *watch + hear* the mix breathe.
- A **visual pump meter** (the master level visibly ducking on each kick) so the invisible
  effect becomes legible — [game-feel](../guides/game-feel.md) rule: tie the effect to a visible event.
- Mobile-friendly per `ui.h` (grid taps + knobs) — it's a touch instrument at heart.

## Explicitly out of scope (a different toy)

**Vocoder** and **ring-mod** are also "doesn't fit a pedalboard" effects, but they're
*carrier × modulator* (two voices multiplied), not summed-bus — the control is "two things
at once," not "ride a master knob." They want their **own** talkbox / robot-voice toy. Don't
cram them into the groovebox.

## Chassis (deferred to Phase 2, per firewall)

Voices designed above *before* reading cousins. For the build, copy the **chassis** from the
nearest step-sequencer / drum-machine cart (`drummachine.c` and a multi-track sequencer
cousin via [`instrument-carts.md`](../guides/instrument-carts.md)), reuse `ui.h` for the grid + knobs, and keep the rack as a
data-driven knob list (named enum, per CLAUDE.md's data-driven rule) so adding `sidechain()`
later is a one-row change.

---

_Built (Phase 2), then UPGRADED through three engine landings: reverb_insert (SPACE became a real
master insert, ORDER toggle → reverb↔crush), and finally **effects-bus Increment D — the real
`sidechain()`/`glue()`** shipped, so the faked cart-side pump was REWIRED onto the engine: PUMP =
`sidechain_key(SL_KICK,0,1)` + `sidechain(0,0,amount,…)` ducking the whole master mix; GLUE =
`glue(0,…)` self-keyed bus comp. They share the one master comp (engine: one per bus) and are
exclusive in the UI. The cart was the acceptance test that surfaced that one-comp-per-bus constraint.
PUMP knob stayed put exactly as the brief predicted — only the wiring under it changed. The faked
duck now lives only in the meter (a visual mirror)._
