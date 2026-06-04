# Input recording, live-looping & ghosts — what to record, at which level

> **Genre: design exploration (scratchpad).** Can a cart record what the player
> does and play it back in a loop? It is **not** a committed plan:
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - The harness-level record/replay that exists *today* → [`../guides/debug-harness.md`](../guides/debug-harness.md).
>
> Origin: "a couple of instruments on screen, play them separately, and they end
> up looping" — a live-looper (loop pedal / Incredibox energy). Plus: "could also
> be useful for demos where players are moving" (attract modes, racing ghosts).

---

## One idea is actually two

Both wear the costume "record input, replay it," but they want **different
recording levels**, and conflating them produces the wrong design for each:

| | Live-looper (music) | Ghost / demo (gameplay) |
|---|---|---|
| What must be reproduced | the **sound** | the **gameplay** |
| Right level to record | audio **control events** (`note_*` calls) | **input** (packed `btn()` states) |
| Replay collides with live play? | no — N tracks = N streams of calls | no — ghost feeds a *second* entity |
| Quantizable / editable | yes (it's musical data) | no (it's a performance) |
| Doubles as a file format | yes — *this is a song* | yes — *this is a replay* |

The harness's `--record`/`--replay` (`runtime/studio.c:170–260`) is a third thing:
whole-session, file-based, debugging-only. It proves determinism + injection work
in this engine, but it's not cart-facing and shouldn't grow to be — these two are.

---

## The three candidate recording levels (for music)

1. **Raw input** (mouse x/y + buttons + keys, per frame). Tempting because it's
   generic. Wrong for looping, for three reasons:
   - **The mouse can't be in two places.** While track 1 replays a theremin
     gesture, the player is performing track 2 *live with the same pointer*.
     Input-replay means re-running the cart's input→sound mapping against
     injected input — which now fights the live pointer and any UI state the
     mapping reads (selected instrument, mode). Control events have no conflict:
     more tracks are just more streams of `note_*` calls.
   - **Fragile.** The mapping from input to sound depends on screen layout and
     cart state; move a button and every recording breaks.
   - **Opaque.** You can't quantize, edit, or visualize keystrokes musically.

2. **Audio control events** — *what the cart told the sound engine*, timestamped
   in musical time: `note(60, SAW, 6)` at loop-beat 2.25; `note_pitch(v, 63.2)`
   at loop-beat 2.31. **This is MIDI's discovery**: discrete notes are events,
   continuous gestures are controller-change streams, both live in one
   timestamped list. This is the right level for the looper.

3. **Audio samples** (a true loop pedal records the rendered waveform). The
   engine has no PCM capture or playback path at all — biggest lift, least
   editable, and level 2 already yields "save as songs." Not now; noted as the
   far end of the spectrum.

### "But the theremin is all tiny continuous tweaks…"

Right — `musicalsaw.c` is the worked case: `mouse_x/y` → `note_pitch(voice, p)`
**every frame** (`musicalsaw.c:48–63`), and ten carts drive continuous control
(`note_pitch`/`note_cutoff`/`note_vol`/`note_duty`). The CC-stream answer:

- A gesture records as `note_on` + a stream of `(loop_pos, NOTE_PITCH, value)`
  events, **sampled only on frames where the value changed** (delta-recording).
  Worst case ~60 events/sec while the hand moves; a 4-bar swoop is a few hundred
  8-byte events. Fine.
- Notes get quantized; gesture streams replay verbatim (quantizing a swoop is
  nonsense). Two event flavors, one list.
- A replayed gesture can be *drawn* — the recording contains pitch/vol over time,
  so the looper can render a ghost hand/cursor on the instrument. The visual
  feedback raw-input recording would have given "for free" is recoverable, minus
  its problems.

---

## The looper, in cart code today (no engine change)

Per [ADR-0006](../decisions/0006-library-carts-not-engine.md): prototype as a
**library/example cart**. Every needed primitive ships already:

- **Musical clock** — `bpm()` / `beat()` / `beat_pos()` are frame-driven and
  deterministic (`sound.h:143`); continuous time is `beat() + beat_pos()`, loop
  position is `fmodf(t - start, LOOP_BEATS)`.
- **Persistence** — `save_bytes()`/`load_bytes()` hold a whole track list.

Core sketch (~60 lines for the engine of it):

```c
typedef struct { float pos; unsigned char kind, a, b, c; } Ev;   // kind: NOTE / CC_PITCH / CC_CUTOFF / OFF…
typedef struct { Ev ev[256]; int n; bool muted, armed; int voice; } Track;

// record — the cart records what it PLAYS, the moment it plays it:
float lp = fmodf(beat() + beat_pos() - loop_start, LOOP_BEATS);
trk->ev[trk->n++] = (Ev){ quant(lp, 0.25f), EV_NOTE, midi, instr, vol };

// playback — each frame, fire every event the playhead crossed since last frame:
if (crossed(prev_lp, lp, e->pos)) fire(trk, e);    // handles the wrap at the seam
```

Known wrinkles (each has a standard looper answer):

- **Quantize-backwards.** Rounding a late press to the nearest 1/8 can place the
  event *behind* the playhead. Standard fix: sound it live un-quantized on the
  record pass; store the quantized position for every later loop.
- **Held notes across the seam.** `note_on`/`note_off` pairs must record as
  paired events; an `on` without its `off` by loop-end gets an implicit `off` at
  the seam (or the pair wraps). The looper owns its replay voices' handles, so a
  muted/cleared track can `note_off` everything it started.
- **Don't `schedule()` everything.** The delayed-request pen on the audio thread
  is only 16 entries ([audio-notes §2](audio-notes.md)). Fire events at frame
  granularity — 16 ms jitter at 60 fps is within fantasy-console fidelity. If a
  frame straddles two events, `schedule()` the second with its ms offset; that's
  the pen's intended dose.

### Songs fall out

A serialized track list **is a song format** — events are engine vocabulary
(midi/instr/vol/cutoff), not cart-specific. `save_bytes()` keeps your jam between
runs today. This is also a live-performance answer to the oldest open audio gap
(STATUS: SFX/music banks hardcoded, **no cart-side authoring**): you don't author
patterns in a tracker, you *play* them in and the looper captures them.

---

## The ghost (demos, racing, time-clones) — input level, deliberately

Here input recording is *right*: the point is reproducing gameplay, and the
replayed entity is a **second body**, so there's no live/replay input conflict.
The standard pattern (Super Meat Boy replays, racing ghosts, Braid time-clones):

```c
// pack the 6 buttons into a byte per frame; ring-buffer it
ghost_tape[f % TAPE_LEN] = pack_btns(0);
// replay: feed unpacked bits into THE SAME movement function the player uses
move_body(&ghost, unpack(ghost_tape[g]));
```

Requirements this puts on the *cart* (the teachable part):

- Movement must be a function of `(state, input-snapshot)` — not sprinkled
  `btn()` calls. That's a structure lesson worth a tutorial on its own.
- Replay only reproduces a run if the simulation is deterministic given input —
  keep `rnd()` out of the movement path (or record positions instead of inputs
  for attract-mode purposes, which tolerates drift anyway).
- `save_bytes()` ⇒ a **persistent ghost**: race yesterday's run. At one byte per
  frame, a 60 s run is 3.6 KB.

---

## Engine promotion path (if cart-land shows friction)

Every sound mutation already flows through one place: the main→audio-thread
**request ring buffer** (kinds 0–19; "the one correct place to mutate sound
state", [audio-notes §2](audio-notes.md)). The ring *is* this engine's MIDI
stream. So an engine-level recorder, if ever justified, is a **tee on the ring**:
record = capture requests + timestamps; replay = re-inject them. No new
vocabulary, no second event model. Likewise the harness's injection engine is the
seed for any engine-level ghost support. Neither is proposed now — per ADR-0006,
the cart prototypes come first and generate the evidence.

---

## Staged plan

1. **`loopstation.c`** — the headline example cart. 3–4 instruments on screen
   (drums / bass / lead / one gesture instrument to force the CC-stream design),
   fixed 4-bar loop, arm-record per track, overdub, mute/clear, loop-ring
   visualization with event dots. The looper core written as a clean liftable
   block (a future library cart per ADR-0006). **✅ Shipped** —
   `tools/carts/loopstation.c`: the looper core is `rec_ev`/`fire_replay`/`fire_ev`
   (~70 lines); harness-verified (events quantize to the 16th grid and replay at
   identical loop positions across passes, no double-fire on the recording pass).
   Findings: the persistent-silent-voice trick (a dedicated `note_on(…, 0)` voice
   per gesture track, driven by `note_pitch`/`note_vol`) dissolves the held-note
   seam problem entirely — EV_OFF is just `note_vol(v, 0)`, so a gesture wrapping
   the loop seam needs no on/off pairing. No engine friction found.
2. **Ghost cart** — retrofit a racer or platformer with record-your-run /
   race-your-ghost, demonstrating the input-snapshot structure + `save_bytes()`
   persistence. **✅ Shipped** — retrofitted into `tools/carts/marble.c`: a run
   that sets a new best time becomes a see-through ghost marble racing you,
   persisted via `save_bytes()` with a magic header so a course change
   invalidates stale ghosts. Findings:
   - It records a **position stream** (6 bytes/frame), *not* input — marble's
     sim integrates with `dt()` (variable timestep), so input-replay would
     diverge exactly as §"the ghost" predicted. Rule of thumb confirmed: input
     ghosts need a frame-stepped sim; dt-integrated sims record positions.
   - The retrofit forced a real **gameplay fix pass** first (the marble could
     be fully hidden behind tall iso walls, ledges teleported, ramps connected
     nothing) — a ghost is only worth racing on a course with honest physics.
     Harness filmstrips (`--dump`) were how each fix was verified.
   - Ghost visibility debugging gotcha: replaying a ghost against the *same*
     deterministic script hides it pixel-perfectly under the player — test
     with an offset run.
3. **Revisit**: did either generate evidence for engine surface (ring-buffer
   tee, packed-input helper)? Update STATUS / write the ADR then, not before.
