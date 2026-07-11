# Hearing one voice at a time — stem render + voice-allocation trace

Status: **READY TO BUILD (2026-07-11).** Specced below from real seams in
`runtime/sound.h`; not yet started. Born from the napoleon-radio "the solo gets cut
off by another instrument" bug, which we parked because our only audio-verification
tool (`wav-analyze` on the full mix) can't answer *"is the solo actually gone, or just
masked?"*.

Companion to [`audio-notes.md`](audio-notes.md) (the sound hub) and the
[`../guides/debug-harness.md`](../guides/debug-harness.md) (`play.js --wav`, `watch()`,
live inspection). Twin in spirit to the existing audio gates
([`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) → audio): those
assert *"the mix didn't regress"*; these two tools answer *"which voice, and why did it
stop."*

## The problem

Multi-voice carts — radio stations especially — layer a lead/solo over a bed of drums,
bass, and pads. When a note disappears, the full-mix WAV is a blunt instrument: a
truncated solo tail is buried under the drums, so `wav-analyze` shows "sound is
happening" and the bug hides. You end up tweaking **by ear via a proxy** — render,
squint at an envelope plot, guess. We need to (a) *hear one voice in isolation* and
(b) *see the moment a voice is taken away and who took it*.

## Root-cause model (why a voice stops)

Reading `runtime/sound.h`, a held/playing voice can go silent for exactly three reasons.
Only the middle two are "cut off by another instrument":

1. **Natural release** — `note_off()` (or a fire-and-forget `note()`'s envelope) ended
   it. Not a bug.
2. **Shared-slot reuse** (`sound.h:4885`) — `held_voice[slot]` tracks *one* held voice
   per instrument slot. A second `note_on` on the **same slot** calls
   `sound_begin_release()` on the first (`slot reused → fade the old one`). If a solo and
   another part are both authored on the same slot, the other part's next note releases
   the solo. **This is the most likely napoleon cause.**
3. **Voice-pool steal** (`sound.h:4532`) — with all `SOUND_VOICES` (32) voices active,
   the allocator steals the **quietest non-held** voice (held notes are protected). A
   *fire-and-forget* solo played with `note()` is stealable; a `note_on()` held solo is
   not. So a solo that vanishes under a dense bed points at either (2) or a `note()`
   solo hitting the pool ceiling.

The fix for a given cart is usually "give the solo its own slot" or "hold it with
`note_on` instead of `note`" — but you can't choose the fix until you know *which*
mechanism fired. Hence the two tools.

## Tool 1 — stem / solo render (`--solo-slot`)

**Goal:** render a cart's audio with only one instrument slot audible, so you can hear
(and `wav-analyze`) the solo's tail *without the mix masking it*. The audio twin of
soloing a track on a mixer.

**Seam:** a debug slot-mask in `sound.h` — a `static int sound_solo_slot = -1;` (and/or a
mute-mask) checked once where each voice's contribution is summed into the bus (near
`sound.h:5907`, where `last_outL/R` are written). `-1` = normal (byte-identical to
today, so it never affects a real build). Gated behind the harness build only
(`-DDE_TRACE`, matching the rest of `play.js`'s instrumentation) or an env var read at
init — **not** a cart-facing API. Wire a `--solo-slot <n>` flag in `tools/play.js` that
sets it before the run.

**Use:**
```bash
node tools/play.js napoleon script tools/clips/napoleon/… --headless --frames 1800 \
     --solo-slot <SLOT_SOLO> --wav build/napoleon-solo.wav
node tools/wav-analyze.js build/napoleon-solo.wav        # is the tail intact here?
```
If the solo's note survives in the solo'd render but not in the full mix → it's masking,
not a cut (a mix/level problem). If it's truncated *even solo'd* → the voice really is
being released early → Tool 2 tells you by whom.

**Nice-to-have:** `--solo-slots 5,6` (a small list / bitmask) to hear the solo against
just the bass, isolating a two-part collision.

## Tool 2 — voice-allocation trace (`voice-trace`)

**Goal:** a JSONL log of every voice lifecycle event — `on`, `off`, `reuse` (mechanism
2), `steal` (mechanism 3) — each naming the slot, MIDI note, voice index, and (for
reuse/steal) the *victim*. Turns "the solo cut off" into "at frame 512, slot 6's held
voice (MIDI 64) was released by a new note_on on slot 6."

**Seam:** the events already exist as code paths in `sound.h`; they just don't announce
themselves. Under `-DDE_TRACE`, emit a line at:
- `sound_begin_release()` from the slot-reuse site (`sound.h:4885`) → `reuse`.
- the quietest-voice steal (`sound.h:4532`, `sound.h:4863`) → `steal`, with the victim
  slot/note.
- `note_on`/`note_off` allocation → `on`/`off`.

The audio thread can't format JSON safely mid-callback, so push events to a small
lock-free ring buffer (same discipline as the existing sound queue — see
[`audio-threading.md`](audio-threading.md)) and drain them on the game thread into the
`--trace` JSONL that `play.js` already writes, or a dedicated `voice_request` live-
inspection file (twin of the `wav_request` seam in
[`../guides/debug-harness.md`](../guides/debug-harness.md) → Live inspection). A tiny
`tools/voice-trace.js` reader then prints a per-slot timeline / flags every `reuse`+`steal`.

**Use:** run the cart, then read the trace — a `reuse` on the solo's slot is mechanism 2;
a `steal` of the solo's note is mechanism 3; neither means it was a clean `note_off`
(look at the cart's scheduling, not the engine).

## How these crack napoleon

1. `--solo-slot <solo>` render → confirm whether the tail is truncated in isolation.
2. If truncated, `voice-trace` names the event: almost certainly a `reuse` on a slot the
   solo shares with another part. Fix = move the solo to its own slot (or `note_on`-hold
   it). Re-render solo'd → tail intact → ship.

## Build order & scope

- **Tool 2 (trace) first** — cheaper (emit at existing sites; no bus-sum change) and it's
  the *diagnostic*; Tool 1 confirms the fix. Both live entirely in the harness build
  (`-DDE_TRACE`), zero footprint in editor/native/web builds, same as `spec()` and the
  existing `watch()` trace.
- Both are **engine (`sound.h`) changes** — respect the hot-shared-file rules in
  [`../../CLAUDE.md`](../../CLAUDE.md) (targeted `Edit`s only, re-read the region, compile-
  gate with `soundcheck` + confirm the change survived). Run the audio gates after
  (`level-check`, `soak-check`) to prove the `-1`/no-solo default stays byte-identical.
- Not cart-facing API — no `studio.h` entry, no four-places dance. Debug instrumentation,
  like `play.js`'s existing flags.

## Related

- [`audio-notes.md`](audio-notes.md) — the sound hub (add a family-index row pointing here).
- [`../guides/debug-harness.md`](../guides/debug-harness.md) — `--wav`, `watch()`, live
  inspection (`wav_request` is the template for `voice_request`).
- [`audio-threading.md`](audio-threading.md) — the lock-free queue discipline the trace
  ring buffer copies.
- [`held-notes.md`](held-notes.md) — `note_on`/`note_off` handle model (mechanism 3's
  "held voices are protected").
- [`../guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) — the audio gates
  these complement.
