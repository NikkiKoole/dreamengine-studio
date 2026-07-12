# Hearing one voice at a time — stem render + voice-allocation trace

Status: **BUILT (2026-07-12).** Both tools shipped. (1) the voice-allocation TRACE —
`runtime/sound.h` emits `on`/`off`/`reuse`/`steal`/`choke` events (DE_TRACE-only) which
ride the existing `--trace` JSONL automatically; read them with
[`../../tools/voice-trace.js`](../../tools/voice-trace.js). (2) the stem/solo RENDER —
`play.js --solo-slot <n[,n…]>` mutes every voice outside the given instrument slot(s) at
the bus sum. Born from the napoleon-radio "the solo gets cut off by another instrument"
bug, which we parked because our only audio-verification tool (`wav-analyze` on the full
mix) can't answer *"is the solo actually gone, or just masked?"*. Verified: choke events
fire on tr909 (open hat cut by closed hat), on/off on epiano, `--solo-slot 30` (an empty
slot) renders total silence, and — proven byte-transparent — a deterministic render is
identical with and without the trace hooks.

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

Reading `runtime/sound.h`, a held/playing voice can go silent four ways. Only the last
three are "cut off by another instrument" — the trace tags each so you know which fired:

1. **Natural release** (`off`) — `note_off()` (or a fire-and-forget `note()`'s envelope)
   ended it. Not a bug.
2. **Handle-slot reuse** (`reuse`, `sound.h` `SR_NOTE_ON`) — a `note_on` **handle** packs
   a slot (0..31, `SOUND_HANDLE_BITS`) + generation; `held_voice[handle_slot]` tracks the
   one voice serving it. Calling `note_on` on a handle slot that's still live releases the
   old voice (`slot reused → fade the old one`). Note this is the *handle* namespace, **not**
   the instrument slot (5..47) — a cart hits it by reusing/recycling handles, not merely by
   sharing a timbre.
3. **Voice-pool steal** (`steal`, `sound_find_voice`) — with all `SOUND_VOICES` (32) active,
   the allocator steals the **quietest non-held** voice (held notes are protected). A
   fire-and-forget `note()` solo is stealable; a `note_on()` held solo is not.
4. **Choke group** (`choke`, `sound_choke_group`) — a new note whose slot has a `choke_mask`
   silences every active voice in that mask (a closed hat cuts the open hat). Literally "cut
   off by another instrument"; a strong first suspect for a percussive collision.

The fix for a given cart varies — give the solo its own handle/slot, hold it with `note_on`
instead of `note`, raise polyphony, or clear an unintended choke bit — but you can't choose
until you know *which* mechanism fired. Hence the two tools.

> **What actually happened with napoleon:** a 60-second trace showed **275 `on` events, zero
> steal/reuse/choke, peak ~8 concurrent voices** — so the cutoff is *not* engine-level voice
> loss at all. napoleon plays everything fire-and-forget with `note()`; the "solo cut off" is
> therefore cart-level sequencing (the cart itself stops/retriggers the part) or perceptual
> masking under the mix. The trace *ruled out* a whole class of cause in one run — which is
> the point — and hands the next step to Tool 2 (`--solo-slot`).

## Tool 1 — stem / solo render (`--solo-slot`)

**Goal:** render a cart's audio with only one instrument slot audible, so you can hear
(and `wav-analyze`) the solo's tail *without the mix masking it*. The audio twin of
soloing a track on a mixer.

**Shipped:** `sound_solo_active` + a 64-bit `sound_solo_mask` in `sound.h` (both
`#ifdef DE_TRACE`), checked once at the bus sum right after `last_outL/R` are recorded —
a voice whose `instr_slot` isn't in the mask has its contribution (and its echo/reverb/
sidechain sends) zeroed. Because `last_out` is kept, voice **allocation and stealing are
identical** to a normal run: only the audible output becomes a stem. `--solo-slot 5` or
`--solo-slot 5,6` in `tools/play.js` sets the mask; inactive by default (no-op).
**Not** a cart-facing API — harness-only, like `play.js`'s other flags.

**Use:**
```bash
node tools/play.js napoleon run --headless --frames 900 --seed 1 \
     --solo-slot <SLOT_SOLO> --wav build/napoleon-solo.wav
node tools/wav-analyze.js build/napoleon-solo.wav        # is the tail intact here?
```
If the note survives the solo'd render but vanishes in the full mix → it's masking, not a
cut (a mix/level problem). If it's truncated *even solo'd* → the voice is being released
early → Tool 2 says by whom.

**Two caveats** (both fine for the diagnostic; know them before reading absolute numbers):
- The mute is **pre-master-FX**, so the stem still passes through the master limiter/glue —
  which now sees far less input and clamps less. Use the stem to judge *presence / timing /
  truncation*, not absolute loudness (a solo'd part can read *louder* than it is in the mix).
- Byte-for-byte determinism (the golden-WAV guarantee) needs a `script`/`replay`/`--det`
  run — a bare `run` isn't deterministic. For just *listening* to a stem, `run` is fine.

## Tool 2 — voice-allocation trace (`voice-trace`)

**Goal:** a JSONL log of every voice lifecycle event — `on`, `off`, `reuse`, `steal`,
`choke` — each naming the instrument `slot`, MIDI `midi`, `voice` index, and a `victim`
(the instrument slot cut, or the handle slot for reuse/off). Turns "the solo cut off"
into "at frame N, voice V on slot 6 was `choke`d by a note on slot 5."

**Shipped:** an `#ifdef DE_TRACE` lock-free SPSC ring in `sound.h` (`sve_ring`/`sve_push`,
audio thread writes) drained by the game thread in `harness_trace()` (studio.c) into the
`--trace` JSONL — one `{"vev":…}` line per event. Since `play.js` **always** passes
`--trace`, the events are there for free on any audio-cart run; the ring push is
byte-transparent (it never feeds back into the sample path). Events emit at:
- `SR_NOTE_ON` handle-slot reuse → `reuse`; the new held note → `on`.
- `sound_find_voice()` when it steals an active voice → `steal`.
- `sound_choke_group()` → `choke` (victim = the triggering slot).
- `SR_NOTE` fire-and-forget → `on` (victim −1); `SR_NOTE_OFF` → `off`.

Read it with [`../../tools/voice-trace.js`](../../tools/voice-trace.js): a histogram, a
per-slot rollup (notes started + cuts suffered), and every cut in order. When there are
**no** cuts it says so and points you at `--solo-slot` — the "it's masking, not the engine"
verdict.

## How these crack napoleon

The playbook (and what it found): run once, read the trace. napoleon showed **zero cuts**
in 60s (see the callout above) → not the engine → the next step is `--solo-slot <part>` to
hear whether the part is present-but-masked or genuinely stopped by the cart's own
sequencer. (Left there deliberately — the point of this task was the tools; napoleon's
actual fix is a separate sitting.)

## Scope

- Both live entirely in the harness build (`-DDE_TRACE`), **absent** from editor/native/web
  builds — same discipline as `spec()` and the `watch()` trace. Not cart-facing API (no
  `studio.h` entry, no four-places dance) — debug instrumentation like `play.js`'s flags.
- Engine (`sound.h`) changes, made under the hot-shared-file rules
  ([`../../CLAUDE.md`](../../CLAUDE.md)): targeted edits, compile-gated with `soundcheck`.
  The default path is byte-identical (verified: a deterministic render matches with and
  without the trace hooks; `level-check`/`fx-check` drift is pre-existing stale-baseline,
  not from here — the change can't move a float sum).

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
