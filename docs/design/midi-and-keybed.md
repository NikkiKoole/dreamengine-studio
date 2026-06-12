# MIDI input + `keybed.h` — a shared note-input layer

> **Status: core SHIPPED 2026-06-12 (see "What shipped" at the bottom); `solo.h`/
> martenot/Web-MIDI still open.** Captures the design conversation
> (session, 2026-06-12) sparked by "is there a way to plug in my MIDI keyboard?"
> The honest answer turned into a bigger one: there's no shared note-input layer,
> so every sound cart re-rolls its keybed — and MIDI, done right, is a one-place
> feature that fixes both. Nothing here is built yet; what ships lands in
> [`STATUS.md`](../STATUS.md), not here. See also [`held-notes.md`](held-notes.md),
> [`touch-notes.md`](touch-notes.md), [`ui-widgets-notes.md`](ui-widgets-notes.md),
> and [`cart-library-direction.md`](cart-library-direction.md).

---

## The question, and the bigger answer

"Can I plug my Arturia KeyStep into my sound carts?" Today: **no.** Every `midi` in
the codebase is a *MIDI note number* (the pitch convention); there is no device
input — no CoreMIDI, no Web MIDI, nothing reads a physical keyboard. Carts get notes
from `key()` (QWERTY), touch/mouse, or in-cart sequencers.

Adding MIDI surfaced the real problem: **there is no shared note-input layer.** ~38
carts read a computer-keyboard "piano"; 124 hand-roll some slide/capture/glissando
logic. There is *zero* shared keybed helper. So each cart reinvents the same fiddly
state machine — and the agent has to be re-taught it every time a new sound cart is
built.

Done right, MIDI and the duplication have the **same fix**: a shared input layer that
treats the KeyStep as just another note source. Build it once; every cart that adopts
it gets MIDI (with real velocity) for free.

## What the survey found — four input families, one substrate

We audited `epiano`, `moog`, `touchpiano`, `mellotron`, `martenot`, and `solo.h`.
They are **not** one pattern. They are four, and the differences are load-bearing —
a single "keyboard widget" would have been the wrong abstraction.

| family | carts | voice model | quantization | a MIDI note maps as |
|---|---|---|---|---|
| **Discrete chromatic keybed** | epiano, moog, touchpiano, mellotron (+~35) | polyphonic — one held voice per key | chromatic (white + black) | **direct** `note_on`/`note_off` + velocity |
| **Continuous ribbon** | martenot, heldnotes, otamatone | mono, always-on / glide | none (free pitch) | **set pitch + gate**; pitch-bend → ribbon |
| **Scale-locked strip** | `solo.h` (every radio) | mono glide / struck | scale- or chord-locked cells | **snap to nearest in-scale cell** |
| _(knobs/sliders — future)_ | all | — | — | CC → param (out of scope now) |

Underneath all of them is the **same substrate**: per-finger capture + a
position→note mapping + glide/glissando. **`ui.h` already provides that capture
machine** — and `solo.h` and `mellotron` already build on it. But `epiano`, `moog`,
and `touchpiano` each hand-roll their own `touch_count`/`Ptr` pool. *That* split is
the root of the duplication.

### Why martenot and solo.h matter (the things that nearly broke the plan)

- **martenot** is an Ondes Martenot: one voice rings the whole time (`note_on` at
  startup, *never* `note_off`'d), and the keys call `note_pitch(mainV, …)` — they
  re-pitch a held voice, they don't start/stop discrete ones. The discrete
  `key_down→note_on` core does **not** fit it. It belongs to the continuous family
  with `heldnotes`/`otamatone`.
- **solo.h** is a scale-locked monophonic glide strip (`solo_handle`, cells =
  in-key notes, portamento between cells), wired into `radio.h`'s live key/scale/
  chord. It is *already* a shared header on `ui.h`. It must **not** be swallowed by
  a chromatic keybed — it's harmony-locked on purpose. A MIDI note into solo.h must
  **snap to the nearest in-scale cell**, not play chromatically.
- "ribbon" is **not** a live `radio.h` concept — the only reference is a deprecated
  no-op stub (`radio.h:453`). The radios' playable thing is the solo strip.

## The architecture

The truly universal layer is **not** `keybed.h` — it's a tiny **engine-level MIDI
input API**, because three of the four families need MIDI and only one is a
"keyboard." Everything else is a consumer that interprets MIDI per its own model.

```
                 ┌─────────────────────────────────────┐
   KeyStep ──────▶  engine: midi_get() / midi_held()    │  (CoreMIDI native,
                 │  (raw note-on/off + velocity + bend)  │   Web MIDI in browser)
                 └───────────────┬─────────────────────-┘
                                 │  one event stream
          ┌──────────────────────┼───────────────────────┐
          ▼                      ▼                        ▼
   keybed.h (NEW)           solo.h (EXISTS)         ribbon carts (bespoke)
   discrete, on ui.h        + snap-to-scale         martenot/heldnotes:
   note_on/off + velocity   reader                  note_pitch + gate
```

- **`keybed.h` is built ON `ui.h` capture** (like solo.h/mellotron already are) —
  *not* a fresh `Ptr` pool. This is the agent-facing win: one documented contract,
  and it ends the hand-rolled-touch-pool duplication.
- **`keybed.h` sits beside `solo.h`** as a sibling cart-land header (ADR-0006). It
  does not absorb it. They share only the engine-level MIDI plumbing.

## The engine-level MIDI API (the universal layer)

Minimal, `btn()`/`key()`-shaped, family-agnostic:

```c
int  midi_get(int *note, int *vel);   // drain one event/frame: 1 = note-on, -1 = note-off, 0 = none
bool midi_held(int note);             // is this note currently down? (poll style)
int  midi_bend(void);                 // last pitch-bend, -8192..+8191 (0 = centre) — for ribbons
bool midi_present(void);              // is any MIDI input device connected?
```

**Native backend (macOS / CoreMIDI):** `MIDIClientCreate` → `MIDIInputPortCreate`
→ connect all sources → a read callback parses note-on (`0x90`, vel>0), note-off
(`0x80` / `0x90` vel 0), and pitch-bend (`0xE0`). The callback runs on a CoreMIDI
thread, so it pushes events into a small lock-light ring buffer that the main loop
drains each frame (same discipline as the audio queue in `sound.h`). Link adds
`-framework CoreMIDI -framework CoreFoundation` to `main.cjs` and `play.js`.

**Web backend (Chromium/Firefox; not Safari, not iOS):** JS glue calls
`navigator.requestMIDIAccess()`, listens to `onmidimessage`, and feeds the *same*
ring buffer the wasm cart drains (via an exported function / writes into wasm
memory). The C cart calls the identical `midi_get()` — it never knows which backend
is underneath.

Four-place API wiring per CLAUDE.md: `studio.h` (decl + `//` one-liner), `studio.c`
(impl, behind the CoreMIDI path), `studioDocs.js` (sig + doc + example), `shell.js`
(help-tab keys — likely a new "midi" section).

## `keybed.h` — the discrete-keybed header

Extracts what epiano/moog/touchpiano/mellotron all hand-roll. Read straight off
`epiano.c`, the API is roughly:

```c
// before #include: tunables
#define KEYBED_OCTAVES 1            // how many octaves of keys
// ... key geometry / qwerty map overridable

keybed_config(I_EP, /*base octave*/ 4);   // which instrument slot, starting register
keybed_qwerty(KB_GARAGEBAND);             // QWERTY map preset (or pass your own)
keybed_update();                          // drains touch (via ui.h) + QWERTY + midi_get()
                                          //   → note_on/off + velocity, handles glissando
keybed_draw(x, y, w, h);                  // the rendered manual; or skip & draw your own
int n, vel; bool on;
while (keybed_event(&n, &vel, &on)) { … } // optional: observe for glow/feedback/trace
```

Owns: key layout tables, index↔MIDI (octave-aware), `key_down`/`key_up` (→
`note_on`/`note_off` + handle + glow), hit-test, octave shift + all-notes-off,
glissando (slide across keys), the per-finger pool (**on `ui.h`**), QWERTY binding,
**MIDI in via `midi_get()`** (with real velocity), and the drawn manual.

Must be **parameterized**, because the survey showed real variation:
- **Range:** 1 octave (epiano) vs 2 (mellotron/touchpiano, `NWHITE 14`) vs moog's 11
  whites / `base=48`.
- **Velocity:** moog has it (`vel07`); epiano/touchpiano/mellotron hardcode vol 5–6.
  MIDI brings real velocity — a free upgrade. `keybed.h` carries velocity through;
  carts use or ignore it.
- **QWERTY map differs:** epiano `ASDFGHJK`+`WETYU`; mellotron GarageBand
  `ASDFGHJKL;'`; moog 11-white+`WETYUOP`. → a couple of named presets + override.
- **Geometry differs** (white spacing, black offsets). → `keybed_draw` is
  parameterized; carts with bespoke art skip it and reuse the input core + `key_at`.

The cart keeps what's genuinely its own — voicing, pedals, presets, autoplay.
For epiano that's a ~120-line drop.

## `solo.h` — the scale-locked addition

`solo.h` already owns the strip, the scale/chord lock, glide, and quantize. The
only addition: a MIDI reader that drains `midi_get()` and **snaps the incoming note
to the nearest in-scale cell** before playing it (reusing the existing `notes[]`
cell table). Pitch-bend can map to glide between cells. Net: every radio gains
KeyStep input, always in-key, with no per-station work.

## Pilots — one per family

Prove that **one** engine-level MIDI source feeds genuinely different consumers:

1. **epiano → `keybed.h`** (discrete) — the big line-count win + velocity. The
   strongest proof; a freshly-polished real keybed.
2. **a radio's solo strip → `solo.h` snap** (scale-locked) — proves the harmony path.
3. **martenot → direct `midi_get()`** (ribbon) — proves the continuous path.

If all three play from the KeyStep through the one API, the design is sound and
rollout to the rest is mechanical.

## Build order

1. **Engine MIDI input + CoreMIDI backend** (`midi_get`/`midi_held`/`midi_bend`/
   `midi_present`) + four-place wiring + link flags. **Smallest cart test:** read
   `midi_get()` in a throwaway cart, print incoming notes — confirm the KeyStep
   reaches C. *This alone gets the keyboard making sound through any one cart.*
2. **`keybed.h`** on `ui.h`, modeled on epiano's geometry. Refactor **epiano** onto
   it; verify it plays/sounds identically (debug harness + `tune-check.js`), *then*
   plug in the KeyStep.
3. **`solo.h` MIDI-snap** + pilot on one radio.
4. **martenot** direct `midi_get()` (ribbon pilot).
5. **Web MIDI backend** (Chromium) + incremental rollout to the other keybed carts.

Steps 1–2 deliver "plug in the KeyStep and play epiano." Everything after is
incremental and independently shippable.

## Risks / open questions

- **CoreMIDI threading:** the read callback is off-thread → ring buffer + per-frame
  drain, mirroring `sound.h`'s queue discipline. Run the sound tripwire mindset:
  no dropped/duplicated note events under fast play.
- **libtcc live backend:** CoreMIDI is a system framework, should link into the
  persistent host fine — verify hot-reload doesn't leak the MIDI client/port.
- **Velocity curve:** map MIDI 0–127 → the engine's `vol 0..7`. Pick a curve once,
  in the engine layer, so every cart is consistent.
- **`keybed.h` adoption is per-cart** (~5 lines each after epiano) — not automatic.
  Add `keybed.h` to the cart-land library-header shelf in
  [`../guides/cart-authoring.md`](../guides/cart-authoring.md) and the `runtime/*.h`
  table in `CLAUDE.md` so the agent *finds* it instead of hand-rolling again.
- **Scope discipline:** CC→knob mapping, MPE, and MIDI clock are explicitly **out**
  for now. Notes + velocity + pitch-bend only.
- **`keybed.h` is a singleton (file-global state) — one keybed per cart.** Every cart
  we have is fine with this (and martenot's dual ribbon+manual is a different family),
  but a **split keyboard** (different instrument slots left/right, or stacked manuals)
  can't use two keybeds at once. **We may want to support this in the future** — it
  would mean making keybed.h instance-based (a `Keybed` struct the cart owns, with the
  functions taking a `Keybed*`) instead of the current global. Not needed yet; recorded
  so the next person hitting a split-keyboard cart knows it's a known ceiling, not a bug.

## Weird patterns found during the rollout (the valuable part)

Migrating real carts surfaced where the "one held note per key into one slot" model
bends or breaks. Each drove a capability or is a known limitation:

- **Struck, not held (`piano`).** Uses `hit()` (fixed duration, rings down on its own),
  and its *whole* cart — autoplay, presets, chords — is built on `play_key()`→`hit()`.
  Doesn't want keybed's held `note_on`. → **manual-voice mode** (`keybed_manage_voices(false)`).
- **Multiple voices per key (`solina`).** Each key sounds up to 6 "footages" at once,
  each transposed into a *different* slot. One `note_on` can't express it. → manual-voice mode.
- **Custom note lifecycle (`mellotron`, `mt70`).** mellotron: a chiff transient + an 8s
  tape limit that kills the voice mid-hold (fit via the pre-note hook + `keybed_handle`
  + held-state-as-latch — NO new API). mt70: 3 oscillators per key with independent decay
  + struck/sustained modes (manual-voice mode).
- **No glissando wanted (`organ`).** An organ retriggers; a finger slide shouldn't slur.
  → **`keybed_glissando(false)`**.
- **Non-piano QWERTY (`solina`).** A *tracker* layout (both rows chromatic, `Z`=C `S`=C#…),
  not the GarageBand home-row map — expressible via custom `KEYBED_WHITE_KEYS`/`_BLACK_KEYS`.
- **Split keyboard / two manuals (`sh101`).** Hits the **singleton limitation** above —
  needs instance-based keybed. Deferred.
- **Smart-screen / free-MIDI asymmetry (phase 2, `solo.h` radios).** The on-screen strip
  should stay scale-locked ("can't play wrong" on a phone), but a connected MIDI keyboard —
  a *musician* — should play fully chromatic. So `solo.h`'s MIDI path is *unrestricted*,
  NOT snapped to scale (the opposite of the first instinct). Recorded for the `solo.h` phase.
- **Sibling opportunity:** the 8-pad scale-locked family (`fm`, `filterenv`, `reed`, `pipe`,
  `pluck`) all hand-roll a *diatonic pad row* — a candidate for a *separate* helper, not keybed.h.

## Deferred — exceptional cases (batch them later, on purpose)

The clean chromatic-keybed family is migrated (7 carts). Two keybed carts and a set
of *other* instrument families are **deliberately deferred**: rather than bolt a
feature on per-cart, we collect ALL the exceptional cases first, then design the
advanced capabilities once, informed by the whole set. Come back when the list below
feels complete.

**Both deferred carts now have MIDI** (2026-06-12) — wired straight into their existing
note paths via the engine `midi_get()` layer (sh101: `key_down`/`key_up`; mt70:
`press_semi`/`lift_semi` in its C3..C5 window). So the *goal* (play them from a MIDI
keyboard) is met without migration. What stays deferred is full keybed.h adoption
(unifying touch/QWERTY + the dedup):
- **`mt70`** — the most elaborate voice model: 1–3 stacked oscillator voices per key,
  struck notes that *detach and keep ringing on lift* (bell behaviour), and a
  cart-driven exponential decay via live `note_vol()` per layer. Fits manual-voice
  mode, but intricate. Migrate when we revisit advanced voice lifecycles.
- **`sh101`** — **correction:** it is NOT a split keyboard. It's a *monophonic* synth
  whose two on-screen rows are just two octaves of the *same* keyboard stacked to save
  space (one sound). So it needs neither two instances nor a split. Full keybed.h
  adoption is only blocked on a **multi-row drawn layout** (keybed.h renders one
  horizontal row; sh101 wants two stacked) — a rendering option, not a structural change.

**The split-keyboard feature, reframed (better than instances).** A real split synth is
**1 keybed → 2 sounds**: one keyboard with a split point, low notes → slot A, high notes
→ slot B (bass left hand, lead right hand). That's far lighter than instance-based keybed
— a split point + a second slot, not a whole `Keybed*` refactor. Prefer this when a
genuine split cart appears. (Instance-based keybed is only needed if two keyboards must
be played *and drawn* independently at once — no current cart wants that.)

**Other families that will add exceptional cases (phase 2):**
- **Continuous-pitch ribbons** — one held voice + `note_pitch`; share only `midi_get()`.
  **`martenot` now has MIDI** (cart-side: note→pitch, velocity→swell, bend→ribbon — the
  pattern). Still to do: `upright`, `pdbass`, `spacecho`, `musicalsaw`. May want a small
  *ribbon* helper once a few more exist.
- **Diatonic pad rows** (`fm`, `filterenv`, `reed`, `pipe`, `pluck`, `pan`, `pd`) —
  fixed scale pads, not chromatic keys. Candidate for a *separate* sibling helper.
- **`solo.h` radios** — the smart-screen / free-MIDI asymmetry (touch = scale-locked,
  MIDI = chromatic). See the weird-patterns note above.

The throughline for the revisit: a **richer voice lifecycle** (the manual-voice
callbacks already point the way) covers `mt70`; a **multi-row drawn layout** covers
`sh101`; a **1-keybed→2-sounds split** covers real split synths; and **sibling helpers**
(ribbon, diatonic pad row) cover the other families. None of these needs the heavy
instance-based (`Keybed*`) refactor — that's only for drawing two independent keyboards
at once, which nothing currently wants.

## What shipped (2026-06-12)

Built and committed this session — see `STATUS.md` for the canonical ledger:
- **Engine MIDI input** — `midi_get`/`midi_held`/`midi_bend`/`midi_present`/`midi_name`.
  **Native** = CoreMIDI (hot-plug, device name), hardware-verified on an Arturia KeyStep.
  **Web** = a JS bridge (`runtime/web_midi.js`, emcc `--post-js`) driving
  `navigator.requestMIDIAccess()` into the SAME ring — so a USB keyboard plays carts in
  desktop Chrome/Edge/Firefox (not Safari/iOS), over https/localhost, with a connect toast.
- `keybed.h` — the touch/mouse/QWERTY/MIDI router + discrete voice manager + manual-voice
  mode (`keybed_manage_voices`/`on_note`/`on_off`) + glissando toggle + drawn manual +
  geometry helpers (`keybed_config/layout/velocity/update/draw`, `keybed_held/glow/handle/
  finger/octave`, `keybed_white_rect/black_rect/white_midi/midi_at`, `keybed_octave_shift`).
- **Migrated (7):** epiano, moog, touchpiano, mellotron, organ, solina, piano — unified
  input + velocity, GarageBand keyboard layout everywhere; ~480 lines of duplicated keybed
  code removed net.
- **Exceptions (3, MIDI cart-side via `midi_get()`):** mt70 (multi-voice/detach-ring),
  sh101 (two-row layout), **martenot** (the continuous-pitch ribbon — a note sets the
  ribbon pitch, velocity = swell, and the **pitch-bend wheel bends the ribbon** ±2
  semitones, the first real use of `midi_bend()`). Full keybed.h adoption deferred.

Still open: full keybed.h adoption for mt70/sh101, `solo.h` MIDI-snap (smart-screen /
free-MIDI), the other ribbons (upright/pdbass/spacecho/musicalsaw), velocity on the
keybed exceptions, a native device-name toast.

**Open — MIDI velocity curve (noticed playing epiano, 2026-06-12).** keybed.h maps MIDI
velocity **linearly** (`(vel*7)/127` in `keybed_update`), while the computer keyboard is a
fixed `kb_vel` (6/7). So MIDI and QWERTY *sound different*: normal MIDI playing (~mezzo)
lands around vol 4–5 — quieter/mellower than the constant QWERTY 6, and you have to bash
the keys to reach the full bright tone. That difference is velocity expression working (a
feature), but the linear curve sits too low for comfortable playing. To revisit: replace
the linear map with a gentler curve (typical presses reach ~6–7 without slamming), in
`keybed_update` — one change, applies to every keybed cart at once. Decide then whether
QWERTY should stay fixed-6 or pick up a default that matches the curve's centre.
