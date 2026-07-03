# 0027 — Cart-facing sound state flows ONLY through the request queue
Date: 2026-07-03 · Status: accepted

## Context
The sound engine has always had a lock-free SPSC request queue (main thread pushes, audio
thread drains — [`audio-threading.md`](../design/audio-threading.md)), originally for thread
safety. With `de_switch_cart()` (the per-cart sound context,
[`share-panel.md`](../design/share-panel.md) §build-ladder rung 1), the queue quietly became
**load-bearing for a second thing**: the cart context is a *log of the config requests that
drained*, so a context restore is only as complete as the queue is universal.

`bpm()` was the one call that bypassed it — a direct write to a global. Nobody noticed for
weeks because it was thread-benign (an aligned int). The FIRST multi-cart bundle exposed it in
minutes, by ear: tabbing racks jumped the tempo, because the incoming cart's main-thread
`bpm()` landed **before** the audio thread snapshotted the outgoing cart's value. Thread-benign
is not context-benign: a direct write is invisible to the log, races the switch, and restores
wrong.

## Decision
**Every cart-facing sound API mutates engine state only by pushing a request** (`sound_push_req`
/ `sound_push_ctrl`); the audio thread applies it in `sound_fire_req()`. No exceptions for
"harmless" globals — `bpm()` itself is now a queued `SR_BPM`.

Three maintenance rules ride on this (each also stated in a comment at the exact edit site in
`sound.h`):

1. **New `SR_*` kind → classify it.** Events (trigger/ride a live voice: notes, sfx,
   handle-payload rides) go in `sound_req_is_event()` so they're never recorded; set-and-hold
   config records automatically via the default. Misclassifying config as an event = silent
   restore gap; the reverse = a stale note replaying on switch.
2. **New config kind → give it a dedup key** in `sound_ctx_key()` (which of a/b/c *name* the
   knob vs carry its value). The fallback is append-only — always correct, just log space; the
   `[sound] WARNING` overflow tripwire names the cost (caught by the soundcheck gate).
3. **New engine state → reset it in `sound_reset_state()`.** The same clean slate serves boot,
   libtcc hot-reload determinism, AND the context switch — one reset, three consumers, so a
   forgotten field breaks `--det` too and gets caught.

## Consequences
- The context log covers every *future* effect automatically — there is no per-effect snapshot
  struct to rot, which is why the log-replay design was chosen over the originally sketched
  field-by-field snapshot ([`share-panel.md`](../design/share-panel.md) §ladder).
- Config takes effect one audio callback (~23ms max) after the call instead of instantly —
  inaudible, and scheduled/note timing already snapped to callback boundaries anyway
  ([`audio-timing.md`](../design/audio-timing.md)).
- The oracle for the whole family: `tools/bundle-spike/proof-sound.sh` (same note, same slot,
  before-switch vs after-round-trip → corr 1.0000; cross-context → 0.004).
- Reviewing a sound.h change? Grep the diff for writes to file-scope state from a public
  (non-`static`) function — that's the violation shape.
