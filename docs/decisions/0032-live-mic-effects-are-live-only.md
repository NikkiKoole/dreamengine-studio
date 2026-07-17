# 0032 — Live-mic-through effects are live-only (no deterministic replay)

Date: 2026-07-17 · Status: accepted

## Context
The engine's determinism guarantee — the `.rec` harness, `spec()` gates, seeded stations — rests on
a cart's inputs being reproducible. `docs/design/mic-and-sampling.md` split the mic into two shapes:
**capture-then-freeze** (record a take, then the cart consumes the frozen PCM — deterministic, e.g.
`breakchop`'s beatbox-chop and `mic_record`) and **live throughput** (a signal passes *through* an
effect and you hear the result *now* — a pedal, or the vocoder's live modulator).

Vocoder Phase 2 (`vocoder_mic()`, `docs/design/vocoder.md`) is the first live-throughput feature: the
microphone drives the sound in real time through an audio-thread PCM ring. A live mic signal cannot be
frozen into a cart-owned artifact, so a run that uses it **cannot be reproduced** frame-for-frame from
a `.rec` — the air in the room is not recorded. This is the doctrine call `mic-and-sampling.md` flagged
("either the harness records the live PCM as an input track, or pedal carts are declared live-only").

Recording the live mic PCM into the `.rec` was considered and rejected for v1: it bloats the recording
with audio, still wouldn't reproduce *timing* against a different audio-clock, and isn't needed for the
creative use (you perform a vocoder live, you don't replay it).

## Decision
**Live-mic-through effects (`vocoder_mic()`, and the future pedal/looper tier) are LIVE-ONLY: a cart
that uses them does not carry the deterministic-replay guarantee.** They are the explicit carve-out
from `.rec`/`spec()`. Specifically:

- **Live-only** (no deterministic replay): anything reading the live mic *as a signal* — `vocoder_mic`,
  and future live pedals. The `.rec` still records touches/keys (so the UI replays), but the audio
  output depends on the live input and will differ.
- **Still deterministic** (unchanged): capture-then-freeze — `mic_record()` → `sample_load()`
  (`breakchop`), and the vocoder driven by a **synth** modulator (`vocoder_send`, the `vocode` cart).
  These freeze the input, so they replay exactly and can carry a `spec()`.

Carts that go live-only should say so in their `de:meta` (voxbox does). The mic-analysis surfaces
(`mic_level`/`mic_pitch`) are already the Tier-1 "live by nature" exception the mic doc named; this ADR
generalizes that to live *audio* through-effects.

## Consequences
- The `vocode` cart stays the vocoder's deterministic acceptance test (synth modulator); `voxbox`
  (live mic) is live-only and carries no `spec()`.
- No `.rec` audio-capture machinery to build or maintain; the harness is unchanged.
- The engine core stays deterministic — the non-determinism is confined to carts that opt into a live
  input, and only while the mic is engaged (dormant → byte-identical).
- If a future need arises to *reproduce* a live take (e.g. for a bug report), the path is
  capture-then-freeze: record the take with `mic_record`, then replay it through the effect offline —
  not a `.rec` change. Filed as the fallback, not built.

Related: [`vocoder.md`](../design/vocoder.md), [`mic-and-sampling.md`](../design/mic-and-sampling.md)
(§"the deep constraint" + §"the live-throughput dimension").
