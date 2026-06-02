# Held notes — sustain, live control, and slew (spec)

> **Status: SHIPPED.** `note_on()→handle`/`note_off()` + `note_pitch`/`note_vol`/
> `note_cutoff`/`note_duty` + `note_off_all` are live (`runtime/sound.h`, request kinds
> 7–13), with `index+generation` handle safety and per-param slew exactly as specced
> below. `note()`/`hit()` keep their fixed-duration behavior. Demo cart: `held notes` (a
> theremin — `tools/carts/heldnotes.c`). The §7 `keyr`/`btnr` release edges shipped too,
> and `moog.c` is retrofitted: keys hold-to-sustain and the held chord follows the filter
> slider live. `note_glide(handle, ms)` (the §9 B portamento opt-in) also shipped — it's
> just the per-voice pitch-slew coefficient turned down, so the default snappy slew and a
> musical glide are the *same knob*. Every §9 item is now settled. The spec text below is
> kept as the rationale.

The missing real-time half of the sound system: a voice you **hold** and **write to
every frame**, instead of firing and forgetting. This is the primitive behind
hold-to-sustain, engine revs, sirens, theremins, charging lasers — and the one that
makes a modular-synth or Moog cart behave like the real thing instead of faking it.

The model is **`note_on` → handle → `note_off`** (the MIDI mental model) — *not* fixed
channels. See §2 for why, and §9 for what's settled vs still open.

Companion to [`audio-notes.md`](audio-notes.md) §6 (the original sketch, which proposed
fixed channels — superseded here) and [`modular-synth.md`](modular-synth.md) (its biggest
consumer). **Genre: design exploration** — this is the build-ready spec; settled choices
graduate to [`../decisions/`](../decisions/README.md), status to [`../STATUS.md`](../STATUS.md).

---

## 1. The gap (why this exists)

Two shipped carts bracket the problem exactly:

- **`omnichord.c`** — plucks, strums, drum hits. Everything one-shot. The current
  fire-and-forget API (`note`/`hit`/`strum`) serves it *perfectly*; it needs nothing
  here.
- **`moog.c` ("DREAM SYNTH")** — wants sustained notes and a filter you sweep live.
  It **can't have either**, so it fakes both:
  - every key is `hit(..., 600)` — a fixed 600 ms one-shot. Hold a key forever → still
    cuts at 600 ms. The gate is divorced from the keypress.
  - its own comment admits the constraint: *"the settings are (re)applied the instant
    you hit a note … read at the next note-on."* Drag the cutoff slider while a note
    rings and **nothing changes** until you re-trigger. A Moog's whole identity —
    sweeping the filter under your fingers — is structurally impossible.

The two constraints from [`audio-notes.md`](audio-notes.md) §2 are the root cause:

1. **Voices snapshot at note-on.** A playing voice holds no reference back to its
   instrument slot, so mutating the slot only affects *future* notes.
2. **Everything is fire-and-forget.** No held voice exists; the default note is 250 ms.

Held notes fix both: `note_on` returns a **handle** to a voice that (a) **sustains** until
you `note_off` it, and (b) accepts **live setters** the audio thread applies to the
*ringing* voice.

---

## 2. The model: handles, not channels

```c
int h = note_on(60, INSTR_SAW, 5);   // start a sustained note, get a handle back
note_cutoff(h, 1800);                 // ... drive it live ...
note_off(h);                          // ... release it
```

A **handle** is an opaque int naming one of the 8 voices. The engine picks the voice and
hands you the address — so **polyphony falls out for free** (each `note_on` mints a new
handle) and there's no fixed channel count to allocate against.

This replaces the fixed-channel sketch in [`audio-notes.md`](audio-notes.md) §6. Channels
(`ch_play(0,…)`) force the *cart* to manage a pool to get polyphony — boilerplate that
exists only to work around a fixed address space. Handles match the MIDI model every
musician already knows (key down = `note_on`, key up = `note_off`) and serve both the
polyphonic keyboard (moog) and the persistent single emitter (engine rev) cleanly.

**The one cost handles carry — defused.** If all 8 voices are busy and a 9th `note_on`
steals one, the stolen voice's handle is now stale. Left unhandled, a later
`note_off(stale)` could kill the wrong note. Fix: a handle packs **`index + generation`**;
the engine bumps a voice's generation every time it's freed or stolen, and **every setter
and `note_off` silently no-ops if the handle's generation doesn't match the voice's
current one.** Worst case is a note that rings slightly too long — never a crash, never
the wrong note. That single rule makes handles beginner-safe.

A happy consequence: because stale handles are safe, **held voices are still stealable**
(oldest-held first) — there's no "reserved channel starves the event pool" problem. Voice
allocation stays uniform: 8 voices, LRU stealing, handles keep it honest.

The two worlds coexist and share the 8 voices ([`audio-notes.md`](audio-notes.md) §6.3):

- **Events** (`note/hit/chord/sfx`) → blips, hits, music. Unchanged.
- **States** (held notes) → engines, sliders, sirens, sustained keys, CV-modulated voices.

---

## 3. The three jobs

| Job | What it gives | New work |
|---|---|---|
| **Sustain** | a voice that holds at its ADSR sustain level until released | hold-at-sustain + release-then-free; no auto-duration |
| **Live writes** | change pitch/vol/cutoff/duty of the *ringing* voice per-frame | new ring-buffer request kinds + a handle table |
| **Slew** | those writes don't zipper or stairstep | per-param sample-rate smoothing — the only genuinely new DSP |

Slew (§5) is the one that's easy to skip and ruins everything if you do.

---

## 4. API surface (proposal)

Seven calls. A held note plays an **instrument slot** (its timbre/ADSR/LFO/filter, §10 of
audio-notes) — the handle is *how you hold and drive it live*, the slot is *what it sounds
like*.

```c
int  note_on   (int midi, int instr, int vol);   // start a sustained note → handle. vol 0 = start silent
void note_off  (int handle);                       // enter release; voice frees when release completes
void note_pitch (int handle, float midi);          // set pitch (float → between notes); no envelope retrigger
void note_vol   (int handle, int vol);             // 0..7. 0 = silent but the note stays alive
void note_cutoff(int handle, int hz);              // drive the slot's filter cutoff live (no-op if slot filter is OFF)
void note_res   (int handle, int resonance);       // drive the slot's filter resonance 0..15 live (pairs with note_cutoff)
void note_duty  (int handle, float duty);          // drive pulse width live (no-op on non-pulse waves)
void note_lfo   (int handle, int which, int dest, float rate_hz, float depth);  // retune LFO `which` (0..2) live — phase kept, no click
void note_filter(int handle, int mode);            // switch filter mode live (FILTER_OFF/LOW/HIGH/BAND/NOTCH)
void note_glide (int handle, int ms);              // portamento: note_pitch slides over `ms` instead of snapping (0 = snap)
void note_off_all(void);                            // panic: release every held note (recover from a leaked handle)
```

**What can go live vs. what snapshots at note-on.** The rule is mechanical: anything the
mixer reads *per sample* off the voice can be driven live — pitch, volume, filter
cutoff/resonance/mode, pulse width, and all three LFOs (rate/depth/dest). Anything that's
a *one-time shape at attack* cannot: the **waveform** and the **ADSR envelope** snapshot at
`note_on`, because re-attacking a ringing voice is meaningless. That's the whole boundary —
and it's why the live setters cover everything except `wave` + ADSR.

> **Ring-buffer note:** live control pushes many setters per frame (per held voice:
> cutoff + res + duty + mode + 3×LFO). The main→audio request ring was bumped from 32 to
> 256 entries so a fat chord under an actively-dragged knob can't overflow it and drop
> writes. Cheap (a ~9 KB static array), and live control is the thing that needs it.

### 4.1 Decided semantics

- **Each `note_on` is a fresh voice** with its own handle — it triggers the envelope from
  attack. Repeated notes = repeated `note_on` calls (and `note_off` the ones you're done
  with). There is no "retrigger this handle."
- **`note_pitch` never retriggers** — it only moves pitch (glides via slew, §5). The clean
  split: `note_on` = "new note," `note_pitch` = "same note, new pitch."
- **`note_vol(h, 0)` = silent but alive** (the note keeps its slot/pitch, can ramp back up)
  — for an engine that idles at a stop. **`note_off` = release + free** — for a key-up.
  Two distinct gestures, two calls.
- **A stale or finished handle makes every setter and `note_off` a silent no-op** (§2).
  This is the safety contract — lean on it; don't make carts check.
- **Live setters compose with the slot's LFOs.** `note_cutoff` sets the *base* the game
  drives; an `LFO_CUTOFF` on the slot still shimmers *around* it — so a sound can be
  modulated by the game **and** an LFO at once (gross sweep + fine shimmer). Same for
  `note_duty` vs `LFO_DUTY`.
- **No auto-release.** A held note rings until `note_off` (or it's stolen). Leak a handle →
  stuck note; `note_off_all()` is the panic. (A drone is a feature, not a bug, so we don't
  auto-kill untouched voices.)

### 4.2 The shape in use

```c
// engine rev — one held note, the game is the modulator
int eng;
void init(void)  { eng = note_on(40, INSTR_SAW, 0); }         // start silent
void update(void){ note_pitch(eng, 36 + speed * 0.6f);        // pitch tracks speed (glides)
                   note_vol  (eng, speed > 0.1f ? 5 : 0); }    // fade to silence at a stop
```

---

## 5. Slew — the only new DSP

Parameters change at 60 fps; audio runs at 44,100. Feed the latest value raw and you get
**60 discrete jumps/sec** → stairstepping on pitch, outright **zipper noise** on
volume/cutoff. "Just change it continuously" sounds *broken*.

Fix: per knob, store `target` (what the setter wrote) and `current` (what the oscillator
reads); each sample glide `current += (target - current) * k`. This generalizes the
existing per-step declick. **`k` is per-parameter** because they want different feels:

| Param | Default slew | Why |
|---|---|---|
| **pitch** | near-instant (~3–5 ms) | stepped sequencer pitch must feel snappy; portamento is opt-in (§9 B) |
| **volume** | moderate (~10 ms) | kills zipper without audible lag on gating |
| **cutoff** | moderate (~15 ms) | the audible one — slow enough to smooth, fast enough to track a sweep |
| **duty** | moderate (~10 ms) | PWM clicks otherwise |

Pitch defaults snappy on purpose: a free *light* glide would make the modular synth's
`QUANT → pitch` feel mushy. Portamento is then an opt-in (see §9 B) — which subsumes the
separately-proposed `slide()` primitive.

---

## 6. Implementation sketch

Rides the existing architecture — **no new shared-state path.** Per
[`audio-notes.md`](audio-notes.md) §2, the 32-entry request ring buffer
(`sound_push_req`) is the one correct place to mutate sound state.

1. **New request kinds** on the ring: `3=note_on, 4=note_off, 5=note_pitch, 6=note_vol,
   7=note_cutoff, 8=note_duty, 9=note_off_all`. Each carries `{handle, value}` (`note_on`
   carries midi/instr/vol). Main thread pushes; audio thread applies. Same discipline as
   `note`/`sfx`/`music` today.
2. **Handles + generations.** A handle packs `index` (which of 8 voices) + `generation`
   (a per-voice counter). The audio thread bumps `voice.generation` whenever it frees or
   steals a voice. Every setter compares the handle's generation to the live one and
   **ignores the request on mismatch** — the §2 safety contract, in one branch.
   - `note_on` allocation: first free voice → else **steal the oldest held/event voice**
     (LRU), bumping its generation. Return `pack(index, generation)`.
3. **Sustain.** The envelope already has attack/release; add a sustain *hold* — after
   decay, stay at the sustain level until a `note_off`/`note_vol(0)` request flips it to
   release. (Fire-and-forget notes keep their fixed-duration auto-release; held notes just
   never schedule one.)
4. **Slew.** Each modulatable field becomes a `{target, current}` pair on the `Voice`;
   the per-sample glide (§5) runs in the mix loop before the oscillator reads `current`.

Cost: a per-voice generation int, a handful of `{target,current}` floats, and ~one lerp
per param per sample. Tiny.

---

## 7. Proof case A — the `moog.c` retrofit

The fix turns the faked synth into a real one, and **polyphony is free** — one handle per
key, no pool to manage:

```c
int held[18];   // handle per playable key, or -1 when up
// (init held[] to -1)

void key_down(int i, int semi) {
    apply_synth();                                       // push the patch to the slot
    held[i] = note_on(base + semi, SLOT, vel * 7 / 127);
}
void key_up(int i) {
    if (held[i] >= 0) { note_off(held[i]); held[i] = -1; }   // release on key-up
}

void update(void) {
    for (int i = 0; i < 11; i++) { if (keyp(wkey[i])) key_down(i, wsemi[i]); if (keyr(wkey[i])) key_up(i); }
    // ... black keys the same ...
    // drag the cutoff slider → sweep every ringing voice LIVE (stale handles safely no-op):
    for (int i = 0; i < 18; i++) note_cutoff(held[i], (int)cutoff);
}
```

Now: hold a key → it sustains; let go → it releases; drag cutoff → the held chord sweeps
under your fingers. The two things a Moog is famous for, finally working.

> **Prerequisite:** `keyr` (key-released-this-frame) **does not exist yet** — today there's
> only `key` (held) and `keyp` (pressed-edge), and likewise `btnp` but no `btnr`. Sustain
> needs a release edge to know when to call `note_off`. Either add `keyr`/`btnr` (clean, the
> natural pair to `keyp`/`btnp`, on-brand) or derive it cart-side by tracking last-frame key
> state. **Adding the pair is the right call** and is a hard dependency of the moog retrofit.

## 8. Proof case B — the modular-synth VOICE module

[`modular-synth.md`](modular-synth.md) step 6 is the one that silently doesn't work today
(filter CV can't touch a ringing note). With held notes it becomes literal — VOICE A keeps
its handle in one int:

```c
int vA = -1;
// gate rising edge:  if (vA >= 0) note_off(vA); vA = note_on(pitch, slotA, voiceVol);  // (re)trigger
// every frame:       note_pitch (vA, pitch);                          // pitch CV (glides)
//                    note_cutoff(vA, base_cutoff + flt_cv * 2000);    // filter CV sweeps the LIVE voice
// gate falling edge: note_off(vA); vA = -1;                           // gate closes
```

The VOICE module's three declared inputs — `gate`, `pitch`, `filter CV` — map onto
`note_on`/`note_off`, `note_pitch`, and `note_cutoff`. The spec was implicitly assuming
this primitive.

---

## 9. Decisions

**Settled (this rewrite):**

- **Handles, not channels** — `note_on`→handle→`note_off`. Polyphony is free; the MIDI
  model is familiar; serves both carts. (§2)
- **Stale-handle safety = silent no-op**, via an `index + generation` handle. The footgun
  is defused, so held voices stay stealable (no reserved pool). (§2, §6)
- **`note_off_all()` is the panic; no auto-release.** A leaked handle is an audible,
  debuggable bug, not a crash. Drones are intentional. (§4.1)
- **B — pitch slew / portamento: SHIPPED snappy-by-default + `note_glide(handle, ms)`
  opt-in.** The key realization: glide *is* the pitch-slew coefficient (default ~4 ms =
  the anti-zipper smoothing, imperceptible as a slide), and `note_glide` just turns that
  same knob down to tens/hundreds of ms so it becomes an audible musical portamento. One
  field (`freq_slew`), two behaviours. Subsumes the proposed `slide()` primitive.
- **C — live setters compose with slot LFOs** (game drives the base, LFO shimmers on top).
- **D — `note_vol` stays integer 0..7** (slew already glides the steps smoothly).
- **E — voice-steal is LRU** (a dead handle silently no-ops, per the contract).

Every §9 item is now settled and shipped.

---

## 10. Surface count & roadmap

Whole feature: **7 calls** (+ maybe `note_glide`) and the `keyr`/`btnr` input pair. No new
constants. Slots, LFOs, filters already exist — held notes are just *how you hold and drive
them*.

Slots into [`audio-notes.md`](audio-notes.md) §7 roadmap as **step 4** (the real-time
foundation; everything dynamic hangs off it). Unblocks: hold-to-sustain (fixes moog), the
modular-synth VOICE module, and engine/siren/theremin sounds in any cart. Pairs naturally
with the §8 navkit organ port (organs *want* to sustain).
