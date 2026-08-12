# sync-spike — two MIDI-clock probes, and the end-to-end gate they make possible

The external-clock path (`runtime/sync.h`, see
[`docs/design/external-clock-sync.md`](../../docs/design/external-clock-sync.md)) has one property
that makes it annoying to debug: **when it looks broken, the fault could be on either side of the
wire** and the symptom is identical. A DAW that isn't sending, a bus that isn't online, a byte the
engine mis-parses, or a cart that misreads the API all present as "nothing happens".

These two ~50-line C programs split that in half, and they earned their place by settling the first
real bug in one command each:

| | what it does | what it answers |
|---|---|---|
| **`midimon <secs>`** | listens on every CoreMIDI source and NAMES each transport byte (clock ticks are counted, not printed) | "is anything actually on the wire?" |
| **`midisend <bpm> <secs> [start]`** | generates a clock onto the IAC bus. With `start` it sends START → clock → STOP; **without it, BARE clock and no START** | "does the engine handle what a DAW sends?" |

`midisend` without `start` is the important one: it reproduces **a DAW that was already playing when
your cart booted**, so the cart joins mid-flow and never sees a START. That is the overwhelmingly
common real-world case, and it is the one that shipped broken (the rack became unstartable, which is
why `sync_transport()` exists).

## Use

```bash
zsh tools/sync-spike/run.sh              # build both + run the end-to-end self-test (no DAW needed)
zsh tools/sync-spike/run.sh build        # build only

tools/sync-spike/midimon 20              # listen for 20s — press play in your DAW
tools/sync-spike/midisend 128 6 start    # START, 6s of 128 BPM clock, STOP
tools/sync-spike/midisend 128 6          # bare clock, no START (the mid-flow case)
```

The self-test asserts the whole arc through `synccheck`'s trace: clock arrives → START is seen →
it runs → STOP is followed → beats advance → tempo lands near the sender's → control is handed
back after the clock leaves. **PASS means the real CoreMIDI path works, not just the synthetic
`--midi-clock` one** — which is the gap this fills, since a deterministic harness run deliberately
ignores real MIDI.

## Requirements + traps

- **The IAC bus must be online**: Audio MIDI Setup → Window → MIDI Studio → double-click IAC Driver
  → tick "Device is online". Both probes match the first endpoint whose name *contains* `IAC`, so
  they work on a localised system (it reads `IAC-besturingsbestand` in Dutch).
- **Pre-warm the cart's compile** before sending, which `run.sh` does. Otherwise `play.js` is still
  compiling when the clock starts, the cart joins mid-flow, misses the START, and the run looks like
  a transport bug that isn't one. That cost two confusing rounds.
- **`midisend`'s tempo jitters** (it is `usleep` in a loop, not a real scheduler), typically reading
  a few BPM under nominal. That is the sender, not the engine — do not chase it. Use a real DAW to
  judge tempo accuracy.
- macOS only (CoreMIDI). Not built by default and not part of `build-all`; `run.sh` compiles it.
