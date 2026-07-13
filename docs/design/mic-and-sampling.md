# Mic input & fantasy-console sampling — the spectrum

STATUS: BUILDING (2026-07-13) — the owner greenlit the **real PCM sampler** after the granular
freeze-and-chop spike (`grainchop`) felt "all over the place." Original framing (2026-07-05): "not
against sampling per se, *if* we can make it fantasy-console-like — but I don't really want to go
there yet." What tipped it: the **internal-synthesis source** angle (below) clears both things this
doc was wary of — no mic (no permission, no live dependency) and replay-deterministic, because the
captured audio derives from the deterministic event stream. And navkit already has the whole thing
built (`soundsystem/engines/sampler.h` + the DAW demo's rolling-capture buffer), so this is a *port*,
not a design-from-scratch.

**Shipped (2026-07-13) — engine piece 1 + `INSTR_SAMPLE` + a movable chop:**
- `record_arm()` — an always-on rolling capture ring of the master mix (8s, `rec_arm`-gated so it's
  byte-identical / zero-cost until a cart records — existing carts + `--det` untouched).
- `record_grab(sample_slot, seconds)` — snapshot the last N seconds into a PCM sample slot,
  **peak-normalized to ~0.95** so playback is as loud as the source (fixed the first-cut softness).
- `INSTR_SAMPLE` + `instrument_sample(slot, sample_slot, root_midi)` — a one-shot forward
  sample-playback voice (linear interp, `speed = played_freq / root_freq`), playable chromatically
  on `keybed.h`.
- `instrument_sample_region(slot, start, end)` — the **CHOP**: play only `[start,end]` of the buffer
  (fractions 0..1), moved live; `sample_peaks(slot, lo, hi, n)` — a min/max waveform readout of a
  grabbed buffer; `record_peaks(seconds, lo, hi, n)` — the LIVE twin that reads the capture ring so
  the take draws itself in as a waveform *while* recording (matching the chop look, not a scope).
- Cart: [`sampler`](../../tools/carts/sampler.c) — clean record flow: **REC** a take (live
  oscilloscope of what's going in) → **STOP** → the recording is drawn as a waveform with two
  draggable handles → carve a **CHOP** → the keys play that slice back, pitched. Verified end-to-end
  (REC live-scope + CHOP waveform/handles screenshots; playback RMS up ~4× after normalization).
  Gates green: soundcheck (900f), build-all (497/497), web-audio-check parity.

**Shipped (2026-07-13) — piece 2:** playback modes (NORMAL / REVERSE / LOOP / PINGPONG,
`instrument_sample_mode`), multi-slice → per-pad KIT mapping, and live chop editing (drag / add /
remove / split markers). The one-cart PCM sampler now makes and plays chops end-to-end.

**Shipped (2026-07-13) — a way to sample DRUMS ([`drumkit.h`](../../runtime/drumkit.h)):** the engine
has no `INSTR_KIT`, so there was no drum source to record into a take. `drumkit.h` is a shared
cart-land header (decision-0006 lane) that owns the drum-kit *skeleton* — a role vocabulary
(KICK/SNARE/HAT/OPEN/CLAP/TOM_LO/TOM_HI/CRASH), a fixed slot layout, and `dk_fire(role, midi, vel)`
with pitch as a first-class param — while each kit's `build()` callback owns the *sound* (so an 808
kick and a 909 kick stay two different voices; nothing is flattened). Ships two built-in kits
(ELECTRO, ACOUSTIC). The sampler adds them as selectable **sources**: pick a kit, the keybed becomes
a drum PAD GRID, you play a beat, and each hit is logged as a chop boundary (same honest note-on
slicing synth takes get). Verified: recorded an ELECTRO beat → sliced into 7 chops, playable
KEYS/KIT; build-all 497/497.

> **Retrofit candidates for `drumkit.h`** (share the trigger, keep their own sound — do this only
> after the header proves itself; retrofitting shipped carts to a v1 abstraction is how you break
> them): the drum carts (`tr808`/`tr909`/`drummachine`/`fingerdrums`), and carts that want a rhythm
> bed beside their melodic core — the radio stations, `notebass`/`onenotebass`, `omnichord`,
> `chordblossom`.

**Shipped (2026-07-13) — piece 3, the SONG arrangement (in-cart):** sample → chop → *arrange*. A
new SONG mode: **+ADD** banks the selected chop into a **fixed 16-pad bank** (the hard limit —
SP-404-style constraint-as-character), a **16-step grid** loops the banked pads off the tempo clock
with a sweeping playhead, and a **seconds-used readout** shows the sample budget — *shown but not yet
capped* (the owner chose fixed-pad-count first; the seconds cap is a later one-line change once the
number feels right). Each take grabs into its OWN sample buffer (up to `MAXBUF`=8), so chops from
different takes — a flute part, an epiano part, drums — coexist in one song (verified end-to-end: a
SAW chop + a drum chop banked from two takes, sequenced together, non-silent). It lives inside the
sampler for now to prove the UX against real chops; lifting it into its own `songbox` cart later is
easy once persistence exists — guessing the arrangement UX in the abstract is not. Nothing persists
across a reload yet.

**Shipped (2026-07-13) — the GRIT mode:** a whole-machine lo-fi character (SP-1200 lineage) — a GRIT
button in the SONG view cycles CLEAN → 12BIT → 8BIT → CRUSH. It's the **master `crush()`**, gated by
state: ON while auditioning/arranging (EDIT/SONG), OFF while recording the source (PLAY/REC) — so the
CAPTURE stays clean and grit is a *playback* mangle you can dial in after the fact (the reason
sampling our own drums isn't redundant — the value is the mangling, not the capture). Not per-slot
`instrument_crush`: there are only 7 aux FX buses (`SOUND_FX_BUSES`), far fewer than the chop voices,
so per-slot crush silently drops (the first cut of this shipped as a no-op — caught by adding a
spectral-centroid readout to `wav-envelope`, see below). Set-and-hold (applied on grit toggle / state
change, never per-frame — `lint-fx-frame` clean). Verified with the new centroid: the SONG-playback
region drops CLEAN 14.8 kHz → CRUSH 4.1 kHz, while a take recorded *with grit set* still captures
clean (15.9 kHz) — the state gate holds.

**Next (piece 5):** **persistence** — `save_bytes()` of the grabbed PCM
+ chop boundaries + the song grid, so a session survives a reload and a song can be lifted into its
own cart. Also: an optional **seconds CAP** (flip the readout into a real budget), touch-drag for the
chop handles (currently mouse), a real choke group for the kit (blocked on a per-slot note-off — the
engine's one-shot notes can't cut each other, tr808's documented infidelity), the **doctrine call**
(an ADR — does a sampler whose source is the console's OWN synthesis clear STATUS #21's
"analog-circuit machines only" line? the argument is yes: an honest closed loop, not a caricature of
a famous PCM box), and the determinism write-up (capture-then-freeze made rigorous for
`.rec`/`spec()`).

## Why this doc exists

Two facts collided (2026-07-05):

1. **The sampler doctrine** ([STATUS #21](../STATUS.md)'s curatorial line): the classic-machine
   museum takes *analog-circuit machines only* — sample-playback boxes (LinnDrum, SP-1200, SK-1,
   PO-33 KO, MPC, SP-404) would be caricatures, since the engine has no sample playback. The
   `mellotron` is the one licensed exception, faked in pure synthesis
   ([`recorded-timbres.md`](recorded-timbres.md)).
2. **Where the carts are going is iOS** — and every device in that store has a **mic** on it. A
   music toy on a phone that ignores the most personal input available is leaving something on
   the table.

This sketch maps the spectrum between those facts: what "sampling, but fantasy-console-like"
could mean without breaking the **one honest core** rule or the engine's determinism. It ranks
the tiers; it does not schedule any of them.

## The deep constraint first: determinism

The real reason the doctrine exists beyond taste: **live mic input breaks replay** — the `.rec`
harness, `spec()` gates, and seeded stations all assume a cart's inputs are reproducible.

The repo already has the answer in `data-tools/`: external data is **acquired at build/edit
time and frozen** (Floorplanner floors → level data, OSM roads → `.rvb`), and the cart consumes
the frozen artifact deterministically. Mic capture fits that shape exactly:

> **Capture-then-freeze.** The mic is an *acquisition device* (editor-side, or a marked
> capture moment in a cart), never a live runtime dependency. Whatever it captures is frozen
> into a small cart-owned artifact (a drawn wave, a parameter set, a PCM chunk) — and from
> that moment the cart is as deterministic as any other. Replay records the *frozen artifact*,
> not the air in the room.

Tier 1 below (mic as controller) is the one exception — it is *live* by nature, and would need
the same treatment touch input already gets in the harness (recorded as an input track).

## The spectrum — four tiers, cheapest and most on-doctrine first

### Tier 1 — mic as CONTROLLER, not source ★ best fit

No storage at all: an envelope follower + a crude pitch tracker driving **existing** voices.

- Beatbox into the phone → onset classification (kick/snare/hat by spectral tilt) fires the
  `tr808` voices.
- Hum → the theremin (`heldnotes`) follows your pitch; the mic becomes another
  `btn()`/touch axis.

The most fantasy-console-like idea here: **the mic is a game controller**, not a recorder.
Zero playback engine, a self-marketing demo on a phone. Engine ask: one small `mic_level()` /
`mic_pitch()` surface (behind a permission prompt on iOS/web).

### Tier 2 — mic → OSCILLATOR, not playback ★ best sound-per-effort

Record a half-second, pitch-detect, extract **one cycle** → `wave_set` / `INSTR_USER0..3`.
Sing "ahh" and your voice becomes a waveform the whole engine already knows how to play —
filters, envelopes, chorus, the lot. You never "play back a recording"; you **harvested a
timbre**. `pocketbox` already fills exactly this slot with a finger-drawn wave; this fills it
with a voice. The synth stays a synth.

### Tier 3 — RESYNTHESIS: the mellotron doctrine, generalized

The console can't store your sound — **only its description**. Record → analyze harmonic
series + amp/brightness envelope → resynthesize on existing engines (additive drawn wave +
noise transient + envelope macros, the [`recorded-timbres.md`](recorded-timbres.md) recipe,
automated). The analysis half already exists as repo tools: `harmonic-spec.js`,
`wav-envelope.js`, `wav-analyze.js`. A "sample" becomes a tiny deterministic parameter set —
text-sized, versionable, embeddable in a cart PNG without bloat. Doctrinally this is the
*pure* fantasy-console sampler: honest, inspectable, and lossy in an interesting way rather
than a caricature way.

### Tier 4 — true PCM, with brutal console specs

If the engine ever grows real sample playback, the constraint must be the aesthetic
(PICO-8-style): e.g. **4 slots × ~1.5 s × ~9 kHz × 4–8 bit**, drawn on screen as physical
tape (the `mellotron`'s 8-second tape limit already proved constraint-as-character reads).
Those are literally the Casio SK-1's real specs, so the lo-fi is period-authentic, not an
effect. Small enough to freeze into a `de:` PNG chunk. This is a real engine commitment
(a PCM voice type + rate/bit reduction in `sound.h`) — hold it until something demands it.

> Note: the [`portapop.md`](portapop.md) cassette-4-track sketch deliberately lives
> *below* this boundary — it records **control events, not PCM** (same level as
> `loopstation`), so it's buildable today without any Tier-4 commitment. The version that
> would record an actual mic/audio take to tape is what waits on this page.

## The hardware touchstones (what each would mean here)

| Machine | The lesson / what an homage needs |
|---|---|
| **Casio SK-1** (1985, 8-bit ~9.4 kHz, ~1.4 s) | The spec sheet for Tier 4's budget — a toy whose *smallness* is the charm. Below Tier 4 it's buildable today as a cheeky fake (Tier 2: "sample" = harvested single-cycle). |
| **Mellotron** (1963) | The licensed exception and the Tier 3 method: attack transient + ensemble + tape ([`recorded-timbres.md`](recorded-timbres.md)), plus the tape-limit-as-character UI. |
| **E-mu SP-1200** (1987, **12-bit** 26.04 kHz, 10 s total) | THE hip-hop crunch — proof that a *bit depth* is a beloved aesthetic, not a defect. Two takes: Tier 4's grown-up preset (12-bit/26 kHz, one shared 10-second budget across all pads — the shared-budget meter is great fantasy-console UI), or today, engine-side: `crush()` already does bit/rate reduction — a **"12-bit bus" preset** (fixed 12-bit + ~26 kHz decimation + the SP's gritty filter) could ship as an effects recipe with no sampler at all. |
| **Roland SP-404** (2005 / MK2 2021) | The modern lo-fi beat-scene box — the culture our `lofi` station already homages (Dilla/Nujabes). Its soul is only half sampling; the other half is **performance FX punched in over a running loop** (the famous vinyl sim, tape, filter, DJFX looper) + pads + resampling. That FX half maps onto *shipped* surface today: `tape()` + `crush()` + `filter()` + `djfilter`/`kaoss`-style punch-in. A "404-spirit" cart = kaoss-meets-lofi (ride the vinyl-sim bus over a generative loop, pads fire stabs) with zero PCM; a *faithful* 404 waits on Tier 4 + resampling, the deepest ask on this page. |
| **TE PO-33 KO** (2018) | The pocket sampler — blocked at Tier 4, but its *form* (16 pads, punch-in FX, LCD character) is already the `pocketbox`/PO lane ([`cart-library-direction.md`](cart-library-direction.md) §2d). |

## What already exists in the repo (the running start)

- **Analysis half of Tier 3**: `tools/harmonic-spec.js`, `tools/wav-envelope.js`, `tools/wav-analyze.js`.
- **Tier 2's landing slot**: `wave_set` / `INSTR_USER0..3` (+ `pocketbox`'s draw-a-wave UI as the pattern).
- **Tier 4's character FX**: `tape()`, `crush()`, `varispeed()`, the master `filter()` — the
  SP-404/SP-1200 *color* without the sampler.
- **The freeze pattern**: `data-tools/` (capture externally, cart consumes frozen data).
- **The doctrine + exception**: [STATUS #21](../STATUS.md), [`recorded-timbres.md`](recorded-timbres.md).

## Verdict (as of 2026-07-05)

Recommended order *if* this ever activates: **Tier 1 → Tier 2** (tiny, on-doctrine, make the
iOS builds feel alive) → **the 12-bit `crush()` preset** (free character, no sampler) →
**Tier 3** (the pure fantasy-console sampler) → only then the Tier 4 / SP-404 conversation.
Nothing here is scheduled; this doc exists so the thinking isn't lost and so the sampler
doctrine has a written "unless it looks like *this*" clause.
