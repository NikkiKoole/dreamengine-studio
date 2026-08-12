# External clock sync — playing in time with something outside the cart

> **Status: MIDI clock SHIPPED 2026-08-12** (`runtime/sync.h` + the four `sync_*` API
> functions + `--midi-clock` in the harness + `synccheck` + acidcandy following it).
> **Ableton Link and AUv3 host transport are the two open backends**, both of which push
> into the same seam. Came out of the question "does Tiny Acid Jam do AUv3 or MIDI in?",
> where the honest answer was no to both, and the cheapest useful yes turned out to be the
> one ReBirth for iPad actually shipped. See also
> [`midi-and-keybed.md`](midi-and-keybed.md) (note input, the other direction),
> [`ios-plan.md`](ios-plan.md) (the AUv3 extension), [`product-notes.md`](product-notes.md)
> (Link on the wishlist), [`audio-timing.md`](audio-timing.md) (the engine's own clock).

## The problem

A groovebox cart owns its tempo. Put it next to anything else that owns a tempo, and one of
them has to give. Until now ours never could, so acidcandy in a room with a DAW was a
novelty rather than an instrument: the right sounds at the wrong time.

Three things could hand a cart someone else's tempo, and the trap is that they arrive in
**two different shapes**:

| source | what it gives you | shape |
|---|---|---|
| **MIDI clock** (a DAW's sync out, a drum machine, another app) | 24 ticks per quarter note + START / CONTINUE / STOP / song-position | **incremental** — no absolute position, tempo must be MEASURED from the tick rate |
| **AUv3 host** (`musicalContextBlock`) | `currentTempo` + `currentBeatPosition` | **absolute** — the host says exactly where the playhead is |
| **Ableton Link** | session tempo + beat phase | **absolute**, same shape as the host |

A cart that wired MIDI clock directly into its sequencer would have to be rewritten for the
other two. So the engine owns a seam instead, and every cart reads the same four functions
without learning which clock it is on.

## The seam

`runtime/sync.h`, engine-internal, compiled inside `studio.c` like `sound.h` and
`midi_input.h` (its API is declared in `studio.h`). Producers push, one consumer derives.

```c
bool  sync_active(void);    // is an external clock driving us at all?
bool  sync_playing(void);   // has it pressed PLAY? (start/continue seen, no stop since)
float sync_beats(void);     // beats since it started — 0.25 = one 16th in 4/4
float sync_bpm(void);       // its tempo (0 = none)
```

`sync_beats()` is the **common currency**: an incremental source accumulates into it, an
absolute source overwrites it. Everything else follows from that choice.

**A cart derives its step counter FROM that number rather than accumulating its own.** This
is the whole design, in one line. An accumulator only ever knows *how fast*; it never knows
*where*. So an accumulating cart cannot follow a re-START, a loop wrap, or a playhead scrub,
and it drifts against the clock forever with no way to notice. acidcandy's hookup is
literally `g_phase = sync_beats() * 4.0f` in place of its `g_phase += dt * rate`.

**Opt-in.** This does not hijack the engine's own `bpm()`/`beat()` clock. A cart that never
calls `sync_*` behaves exactly as before, tempo knob and all. That was deliberate: silently
re-clocking 575 carts because a MIDI cable was plugged in is not a feature.

**Control comes back.** `sync_active()` goes false about 2 seconds after the messages stop
(`SYNC_TIMEOUT`), because a stopped-but-connected DAW sends nothing at all, and a cart whose
tempo knob is dead after the DAW went away is broken. The next tick re-slaves instantly.

## Backends

| backend | state | notes |
|---|---|---|
| MIDI clock (CoreMIDI, macOS desktop) | ✅ shipped | `midi_input.h` parses `0xF8` tick / `0xFA` start / `0xFB` continue / `0xFC` stop / `0xF2` song-position. System-realtime bytes are checked BEFORE the channel-message switch, because `& 0xF0` cannot tell them apart (they all read `0xF0`) and they arrive interleaved inside a packet |
| `--midi-clock <bpm>` (harness) | ✅ shipped | a synthetic clock so the gate needs no DAW and no cable. Pushes through the REAL producer API (the `--net-echo` trick: a fake peer down the true path, never a second code path), and on the harness's fixed timestep it is deterministic |
| AUv3 host transport | open | `musicalContextBlock` + `transportStateBlock` read in the render block → `sync_push_pos()`. Gotcha: the host may assign those properties AFTER it fetches `internalRenderBlock`, so capturing them in the closure captures nil — read them via `Unmanaged.passUnretained(self)` or stash them in `allocateRenderResources()` |
| Ableton Link | open | `sync_push_pos()` from the Link session state. The lib is C++ and dual-licensed; a closed-source app needs the proprietary licence from Ableton, so check that before committing to it |
| MIDI clock on iOS | open | CoreMIDI exists on iOS; `midi_input.h` currently gates the backend to macOS desktop. This plus background audio is what ReBirth for iPad actually shipped |
| Web (Web MIDI) | open | `web_midi.js` already feeds notes; clock bytes would push the same way |

## Measuring tempo is the hard part (position is easy)

Position is exact: 24 ppqn is finer than the 1/4 beat a 16th-note step needs, so
`sync_beats()` is just `ticks / 24` and there is nothing to interpolate. **Tempo has to be
inferred from the tick rate, and two separate mistakes were caught by `--midi-clock 120`
reading back wrong:**

1. **Short windows are biased, not noisy.** A frame sees 0, 1 or 2 ticks, so counting 6
   ticks and dividing gave **122 for a true 120**. The fraction of a tick pending at each end
   of the window is worth ±2 BPM when the window is only 8 frames long. The span has to be
   long.
2. **Dead time belongs to no interval.** Counting the time before the first tick (and after
   the last) made a long span converge to 120 only after ten seconds, always from below. So
   the measurement runs between **tick-bearing frames** and divides by `n-1` intervals, not
   `n`.

The estimator accumulates over a long span and **halves both totals** when the span gets
old, which keeps the ratio exactly while forgetting slowly (so it still follows a tempo
change); a jump over 8% resets it outright so a hard tempo edit lands in a beat instead of a
bar. Measured after the fix: within ~1.5 BPM after one second, settling inside ~0.5 BPM.

That residual is frame-quantized arrival — each tick's time is snapped to its frame, worth
±1 frame on each endpoint. Sub-0.1 BPM would want the **CoreMIDI packet timestamps**
(`MIDIPacket.timeStamp`), which is the honest fix and the obvious follow-up. Note the
asymmetry it buys you and the asymmetry it does not: position is already exact, so a
slightly-off tempo readout affects the *display* and a tempo-synced delay time, never the
lock.

## Testing it

**No DAW, deterministic, headless** (this is the gate):

```bash
node tools/play.js synccheck script /dev/null --headless --frames 600 \
     --midi-clock 120 --trace build/sync.jsonl
```

`beats` must reach `frames/60 × bpm/60` exactly (20.0000 at 600 frames of 120 BPM). That is
the assertion that matters: it is the only check that the tick count and the tempo agree,
i.e. that we are not running our own clock while claiming to follow theirs. `synccheck` also
clicks on every 16th, accented on the downbeat, because a playhead that *looks* right and
drifts 30ms is exactly the bug this cart exists to catch.

**Against a real DAW on the Mac**, no plugin work needed:

1. Once, in **Audio MIDI Setup → Window → MIDI Studio**: double-click **IAC Driver**, tick
   "Device is online", ensure it has a bus.
2. In the DAW's MIDI preferences, enable **Sync output** on the IAC bus (in Live: Preferences
   → Link/Tempo/MIDI, the "Sync" toggle on that output).
3. Run `synccheck` (or acidcandy) natively and press play in the DAW. EXT lights, the tempo
   reads back, the clicks land with the DAW's metronome.

**A note on hosts for AU work later:** Live 10 cannot host our extension (AUv3 hosting
arrived in Live 11.2, and Live 10 is x86_64-only so it runs under Rosetta, which can't load
an arm64 plugin). GarageBand for Mac is universal and does host AUv3, and
`ios/Tests/AUHostTests.swift` is already an offline host that needs no DAW at all — and since
the *host* is the side that supplies `musicalContextBlock`, that test file can fake a
transport and gate host sync deterministically.

## Known limits

- ~~**Swing quantization while slaved.**~~ **Checked, and it is fine** (the maker, in Live,
  2026-08-12). The worry was real on paper: acidcandy's 303 swing compares the fractional part
  of the 16th against the swing amount, and derived from ticks that fraction only moves in 1/6
  steps, so shuffle should have felt coarser than on the internal accumulator. It does not.
  Kept here rather than deleted, because the reasoning was sound and the next person to derive
  a *finer* feel from tick phase (a groove template, a swing under 1/6 of a 16th) will hit the
  real version of it.
- **The tempo knob and transport go read-only while slaved.** Deliberate (be a proper slave,
  the ReBirth model), but acidcandy does not yet *show* that on the MST face, so a maker
  pressing play with a DAW attached gets no explanation. UI follow-up.
- **No MIDI clock OUT.** Carts cannot yet drive someone else's tempo. That is the
  output-direction feature discussed in [`midi-and-keybed.md`](midi-and-keybed.md), and it is
  what would make the 303 sequence an Ableton set (the Rozeta pattern).
- **No time signature.** Everything above assumes 4/4 for the bar:beat:16th readout. MIDI
  clock does not carry a signature at all; the AUv3 host block does.
