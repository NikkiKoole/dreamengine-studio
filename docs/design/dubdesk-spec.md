# dubdesk — a generative dub groovebox (jam-station spec)

**Status: ✅ BUILT 2026-06-22** (`tools/carts/dubdesk.c`) — shipped per this spec, all three
locked decisions intact (LPG auto + A–K hand-play; siren throw = master echo; shared knob row).
Working name `dubdesk` (alts: `riddim`,
`selector`, `soundclash`). The "jam station" that composes three shipped liveset chassis into
one instrument: the **euclid** rhythm maker, the **lpg** melodic voice, and the **dubsiren**
live lead. Parent design: [`cart-library-direction.md`](cart-library-direction.md) §2c
("a future two-handed jam station composing several of these is the natural next move").

## North star

Not three instruments side by side — **one coherent instrument with three layers and a clear
hand-economy**:

- **Rhythm (euclid)** — the beat *plays itself*. Dial density/rotation, it grooves generatively.
  Mostly autonomous = the backing.
- **LPG voice** — the melodic bed riding the beat; its plonky west-coast timbre is the "tune".
  Auto-sequenced **and** hand-playable.
- **Dub siren** — *your live hands*. The fire-pad lead/FX you throw over everything.

The move that makes it ONE instrument, not three bolted together: **the rhythm and the melody
share one pattern brain.** A single 16th clock drives the drums *and* triggers the LPG notes, so
melody and drums lock together coherently — and your hands stay free for the siren.

## The three layers + the shared pattern brain

One monotonic 16th-note counter `gstep` clocks everything (`bpm()` from a TEMPO knob).

- **Drums (euclid):** 3 rings — KICK / SNARE / HAT — each a Euclidean pattern
  `euclid(hits, steps, (gstep + rot) % steps)`. Per-ring density/rotation. One-shot `hit()` voices
  (recipes lifted from `euclid.c` / `kaoss.c`).
- **Melody (euclid × turing):** a 4th "melody ring" — euclid density decides *when* a note fires;
  an 8-bit **shift register** (turing-lite) read out → scale degree decides *which* pitch
  (scale-locked, never sours). A MORPH knob flips bits with a probability (lock ↔ evolve), so the
  melodic line can lock into a riff or drift — the turing idea, folded in quietly. Each melody note
  triggers an LPG voice.
- **LPG voice (the bed):** a pool of held `INSTR_TRI` voices (≈5, for overlap) on one slot. Each
  fired note runs the **vactrol envelope** from `lpg.c`, driving `note_vol` + `note_cutoff` +
  `note_drive` (DRIVE_FOLD) in lockstep — the low-pass-gate plonk. The melody ring fires these;
  **A–K keys also fire them live** (hand-play over the bed). DECAY / FOLD / COUPLE sculpt the timbre.
- **Dub siren (your hands):** two detuned held voices (SQUARE + SAW) with a `note_lfo(LFO_PITCH)`
  warble (from `dubsiren.c`). The FIRE PAD is the hero live surface: **Y = pitch, X = throw**.

## Audio architecture (slots + the master dub bus)

Slots: `SL_KICK, SL_SNARE, SL_HAT` (drums) · `SL_LPG` (held voice pool, ~5 handles) ·
`SL_SIREN_A, SL_SIREN_B` (siren). ~6 slots, ~10 concurrent voices — well within budget.

**Master dub bus (decision 2 — the throw dubs the WHOLE mix):** one master `echo()` + `reverb()`.
*Every* part sends in (`instrument_echo`: drums ~0.12, LPG ~0.3, siren ~0.7; reverb similar but
lighter). The **siren pad's X axis rides the master `echo()` feedback** — so a throw smears the
entire groove into the distance, and past the red it self-oscillates and HOWLS (the `dubsiren`
behaviour, now applied to the whole desk). The siren pad doubles as the master dub control.

> **Gotcha carried from dubsiren:** `instrument_echo`/`instrument_reverb` sends are sampled at
> `note_on` — set every held voice's sends BEFORE its `note_on`, or it never reaches the bus.
> Ride `echo()` only when feedback/time actually change (set-and-hold); the per-voice
> `note_vol`/`note_cutoff`/`note_drive` are the rideable kind.

## Screen layout (320×200)

```
┌ DUBDESK  124bpm  ●●●●            DUB ▓▓░░ ┐   top: transport + master-throw meter
│  ╭─────────────╮   ╔═══════════════════╗ │
│  │  ring        │   ║                   ║ │   LEFT ~150px: the generative ring cluster
│  │  cluster     │   ║    DUB SIREN      ║ │     4 rings: KICK·SNARE·HAT (drums) +
│  │  drums +      │  ║    FIRE PAD       ║ │     MELODY (inner; lit dots coloured by
│  │  melody ring │   ║   Y=pitch X=throw ║ │     pitch). Playhead sweeps each.
│  ╰─────────────╯   ║                   ║ │   RIGHT ~165px: the siren fire pad (hero,
│                    ╚═══════════════════╝ │     always live; X rides the master throw)
├───────────────────────────────────────── ┤
│ [RHYTHM] [VOICE] [SIREN]      ◯    ◯    ◯ │   bottom: PART selector + shared knob row
└───────────────────────────────────────── ┘     (3 knobs re-target the selected part)
```

All three layers stay **visible and sounding** at once; the selector only chooses what the
3-knob row edits (decision 3 — shared knobs, nothing hidden):

| PART   | knob 1 | knob 2 | knob 3 |
|--------|--------|--------|--------|
| RHYTHM | density (selected drum ring) | rotation | TEMPO |
| VOICE  | DECAY | FOLD | COUPLE |
| SIREN  | WOBBLE | ECHO time | VERB |

Always-live surfaces regardless of selection: the **siren fire pad**, the **A–K keys** (hand-play
LPG), and **tapping a drum/melody ring** selects it for the RHYTHM knobs. Within RHYTHM, tap a ring
(or `1`–`4`) to pick which ring density/rotation edit.

## Controls summary

- **Fire pad** (mouse/touch, or SPACE/LATCH): hold = siren on; Y = pitch, X = throw (master dub).
- **A S D F G H J K**: play the LPG voice live over the groove.
- **1–4 / tap a ring**: select drum/melody ring (for RHYTHM knobs).
- **PART tabs** (tap, or a key): RHYTHM / VOICE / SIREN → what the knob row edits.
- **SPACE**: siren fire (held); **L**: latch; **arrows**: nudge the selected knobs.

## Reused chassis (provenance)

- **euclid.c** → the ring cluster + `euclid()` density/rotation drum engine, the nested-ring UI.
- **turing.c** → the 8-bit shift register driving melody pitch (lock ↔ MORPH-evolve).
- **lpg.c** → the held-voice pool + coupled vactrol envelope (`note_vol`/`note_cutoff`/`note_drive`).
- **dubsiren.c** → the fire-pad + warbled held voices + feedback-throw (now master) + the
  set-sends-before-`note_on` rule.
- **kaoss.c** → the transport / self-playing-loop pattern and the "ride-safe master FX" discipline.

## Scope

**v1 (build):** shared 16th clock; 3 euclid drum rings + 1 melody ring (euclid density × shift-reg
pitch); LPG voice pool auto-fired + A–K hand-play; dub siren fire pad with the master echo throw +
reverb; PART-selected 3-knob row; tempo; transport + throw meter.

**v2 (shipped 2026-06-22):** **MORPH** knob on the melody ring (the RHYTHM 3rd knob becomes MORPH
when the melody ring is selected) — lock ↔ evolve the shift register, restoring turing's gesture;
**per-part MUTE** (Q/W/E, or re-tap a PART tab) — DRUMS / MELODY / SIREN, where a muted MELODY still
lets A–K hand-play through and a muted SIREN still lets the pad throw the mix; **SWING** (↑/↓) drags
the odd 16ths late.

**Deferred (v3):** kaoss-style multi-FX programs on the pad; pattern/song save; multiple melody
voices or a second siren; tempo-synced echo time; drum accent/velocity; per-mobile tappable LPG
bars (hand-play is A–K keys; auto covers touch-only).

## Risks / open questions

- **Screen density.** Four rings + a big pad + a knob row on 320×200 is tight. Mitigation: the rings
  are compact and mostly visual; the melody ring fuses melody INTO the cluster (no separate marimba).
  If it's too busy, fall back to 3 rings (drop one drum) — decide at first screenshot.
- **Mix muddiness.** ~10 voices + a master echo/reverb can wash out. Mitigation: conservative sends
  (drums dry-ish, melody/siren wetter), the soft-clip master already in the engine, and the throw is
  a *gesture* (you crank feedback briefly, not held).
- **Mobile LPG hand-play.** v1 uses A–K keys; touch-only users get auto-melody but can't finger the
  LPG. Acceptable (the standalone `lpg` cart remains for pure marimba). Revisit with a tappable strip
  if wanted.
- **CPU.** Per-frame rides for ~5 LPG envelopes + 2 siren voices + master FX — comfortable, but
  profile once (`profiler_request`) after first build.

## Build plan

1. Transport + the ring cluster (drums only) on the shared clock — get a euclid groove playing.
2. Add the melody ring + shift-register pitch → LPG voice pool (auto). Tune the coupled envelope.
3. Add A–K hand-play into the same pool.
4. Add the siren fire pad + the master echo/reverb bus; wire X → master throw. (Mind the
   send-before-`note_on` rule.)
5. PART selector + shared knob row; transport/throw meter; hints.
6. ui-audit, bake screenshot, register, docs (shelf row + `// TEACHES:`), park a demo clip.
