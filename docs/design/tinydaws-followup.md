# tinydaws — follow-up brainstorm: bias knobs, the agentic RLHF loop, the visualizer

STATUS: IDEA / exploration (2026-06). A **second outside-agent brainstorm** (Gemini, "A Guide
To Pocket Operators"), same arrangement as the first: the outside agent supplied vibes under the
name "Tiny Jam"; mapping to this codebase is ours. Read [`tinydaws.md`](tinydaws.md) **first** —
this is a delta on it, not a replacement. The genre-rack table, the lane format, the
seed-as-song-code handoff, and `rack.h` all live there. Commercial lens (trademark rule, MIDI
tiers, sketch-first) lives in [`product-notes.md`](product-notes.md).

## What it confirmed (no new work — just independent convergence)

The outside agent reinvented, from scratch, the **core move tinydaws.md already names**: a radio
that "freezes" its current generator state into an editable sequencer. Their "Freeze &
Deconstruct" == our *"generation writes lanes, playback reads lanes."* Their pitch:

> The radio is not streaming audio — it is streaming **live data** (notes, knob values, triggers).
> So the current generator state can be captured and made editable at any instant.

That's exactly the lane-format keystone. Worth recording only because two independent passes
landing on the same architecture is a strong signal it's the right spine. Same for the genre-box
philosophy (one focused box per genre, write the engine once, swap assets/rules) — already our
"one cart per rack, shared chassis" plan.

The naming riff (`tj-acid3` / `tj-drums8` / `tj-drums9`, generic-word-plus-single-digit to dodge
Roland's 303/808/909 marks) is a cleaner restatement of the **trademark paywall rule** already in
`product-notes.md`. No change; just a tidy module-naming convention if we ever skin a paid rack.

## What's genuinely new (worth stealing)

### 1. Live "bias" knobs — drive the *generator*, not the sound

The knobs, *before* you freeze, bias how the algorithm behaves on the **next bar** instead of
shaping a static patch:

- **Complexity** — down = simple 4-on-the-floor; up = syncopated polyrhythms.
- **Mood** — left = minor/darker progressions; right = major/brighter.
- **Energy** — one macro over tempo + filter cutoff + hat density together.

Ground-truth fit: this is small. The radios' `new_song()`/`play_step()` already gate groove on
section + density (`tinydaws.md` §"half the groove is NOT data"). A bias knob is just those gates
exposed as a live input to the seeded generator rather than baked. It also **stress-tests the
algorithm** — sweep the knob and hear whether the math holds across its whole range. This is a
genuinely new affordance vs. both the radios (fixed density logic) and the rack (edits *frozen*
lanes); the bias knob edits the *generator* while it runs. Open question it inherits: a biased
generation still needs to stay seed-reproducible (same seed + same knob path → same song), or it
breaks the radio.h seed-compat rule the handoff depends on.

### 2. The agentic loop — JSON diffs as an RLHF signal for the radios

The strongest idea for *us specifically*, because development here is already agentic. The app
emits structured **before/after diffs** — what the engine generated vs. what the human curator
kept — and feeds them to coding agents that tune the generators:

```json
{
  "genre": "house", "seed_id": "...",
  "diff": {
    "drum_groove": { "swing": { "before": 0, "after": 15 },
                     "snare_ghost_notes": { "before": "active", "after": "muted" } },
    "bassline":    { "note_density_per_bar": { "before": 8, "after": 5 } }
  }
}
```

Two ways an agent acts on it:
- **rule/weight generators** (what our stations are): "lower snare-ghost probability, thin the
  bassline" → edit the station's `new_song()`/`play_step()` constants directly.
- **prompt-driven generators**: append a system-prompt rule ("keep this genre's basslines breathing,
  not continuous 16ths").

**The A/B toggle is the purest version of this.** The radio carts already have A/B instrument
compare (see [`radio-instrument-options.md`](radio-instrument-options.md)) — keeping Option B
*is* a rejected-vs-accepted pair, RLHF data for free:

```json
{ "instrument": "bassline",
  "rejected_A": { "note_density": 12, "syncopation": 0.1 },
  "accepted_B": { "note_density": 6,  "syncopation": 0.6 } }
```

This closes the loop the original tinydaws motivation opened ("these boxes would let me improve
the radios — I can't tweak the output today"). The plumbing is mostly here already: a frozen lane
state is just bytes (`save_bytes`), and the export tier already exists. A "developer export"
button that dumps the lane diff as JSON is a near-free addition to the WAV/song-code export tiers
in `tinydaws.md` §Export. **The deeper bet** — app pushes diff → agent rewrites the generator,
runs a check, rebuilds → you hear it smarter — is the same human-in-the-loop curation this repo
already runs by hand; this just gives it a structured payload and a button. Lightweight version
worth doing first: export the diff, paste it to an agent manually. The auto-commit server is a
later, optional escalation.

### 3. Dance / visualizer view + social video export

A little character that moves **to the actual pattern** (event-driven, not a volume bouncer),
plus a clean way to share a song without exposing the rack UI. The PO segmented-LCD / OP-1
"drum monkey" lineage — squarely our lo-fi-surface north star.

- **Event-driven from the sequencer**, not the master volume: kick → stomp, snare → clap/head-snap,
  hats → fast taps, BPM → idle bob speed, filter cutoff → energy / background glow. In our terms:
  the visualizer reads the **lane triggers** as they fire (the same events `schedule_hit()`
  consumes), so it's literally driven by the song data, not an FFT.
- **Aesthetic:** 1-bit segmented-LCD Game & Watch sprites — a few frames, near-zero CPU, on-brand.
  Build it with [`tools/sprite-draw.js`](../../tools/sprite-draw.js) from a `<cart>.cart.js`
  (UI-glyph pattern, like `flank`/`boom`); iterate via `sprite-preview.js`.
- **Social-share video export:** render a clean 9:16 loop of just the dancer + a small watermark,
  no UI. We already have the pipeline — [`tools/make-gif.js`](../../tools/make-gif.js) bakes
  webm/gif/mp4 **with audio** from a committed input track (`tools/clips/<cart>/NN-label.*`). A
  rack "export video" button is mostly arming that, framed 9:16. Doubles as the cheapest showreel
  win (ties to `cart-clips.md` + ADR-0020).

This view is also genre-agnostic chrome — a candidate for the shared `rack.h` chassis, one dancer
skin per faceplate.

### 4. MIDI in — the engine already supports it (committed direction)

The maker confirmed the engine already does MIDI, and wants users to plug controllers in and play.
This lands in the **MIDI parking lot in [`product-notes.md`](product-notes.md)** — note there that
input is no longer hypothetical. Design notes worth carrying:

- **Plug-and-play mapping** so any controller "just works" with zero setup: `tj-drums8/9` →
  General MIDI drum notes (C1 kick, D1 snare, F#1 closed hat) so any 16-pad controller hits the
  right sounds; `tj-acid3` → standard CCs (CC 74 → filter cutoff) so hardware knobs sweep the
  filter live.
- **Live recording into the grid:** hit Record, play a bassline on a keyboard, auto-quantize to
  the 16-step lane — the rack's sequencer doubles as a live recorder (overlaps the looper
  crossover already flagged in `tinydaws.md` §Open questions and `input-recording-looper.md`).
- **Visualizer as live feedback:** since the dancer is trigger-driven, every physical pad tap fires
  its animation frame — the character puppets the player's hands.

## Net effect on the tinydaws plan

Nothing in the build order (`tinydaws.md` §Build order) changes. Three small additions slot in
cheaply when their stage arrives:

- **Bias knobs** → fold into the universal control layout (a third knob row next to tone/tempo),
  once the pilot's generator is lane-native. Carries a seed-reproducibility caveat.
- **JSON diff export** → a fourth export tier alongside WAV / song-code / song.h; A/B-diff is the
  free first cut. The single highest-leverage item here, given agentic dev.
- **Visualizer + 9:16 video export** → shared-chassis chrome; reuses sprite-draw + make-gif, so
  near-free and a showreel multiplier.

Full raw synthesis of the source conversation (external "Tiny Jam" framing, all eight genre-box
sketches, the PO/OP-1/ReBirth/Volca reference survey) is parked outside the repo at
`~/Downloads/tiny-jam-design.md` — most of its substance is already absorbed into the rack table
in `tinydaws.md`; this doc keeps only the deltas.
