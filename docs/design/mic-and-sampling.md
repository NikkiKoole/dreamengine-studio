# Mic input & fantasy-console sampling — the spectrum

STATUS: BUILDING (2026-07-13) — the owner greenlit the **real PCM sampler** after the granular
freeze-and-chop spike (`grainchop`) felt "all over the place." Original framing (2026-07-05): "not
against sampling per se, *if* we can make it fantasy-console-like — but I don't really want to go
there yet." What tipped it: the **internal-synthesis source** angle (below) clears both things this
doc was wary of — no mic (no permission, no live dependency) and replay-deterministic, because the
captured audio derives from the deterministic event stream. And navkit already has the whole thing
built (`soundsystem/engines/sampler.h` + the DAW demo's rolling-capture buffer), so this is a *port*,
not a design-from-scratch.

> **UPDATE (2026-07-16) — the MIC side (not the internal-synthesis sampler) is now warming up.** The
> reddit-gaps drip keeps surfacing the same blocked on-grain wishes (hum→MIDI, pedals, live looping)
> that all need real audio input. A standalone **Tier-1 spike** landed to prove the engine can hear
> before touching the hot files — [`tools/mic-spike/`](../../tools/mic-spike/) (miniaudio capture →
> `mic_level()`/`mic_pitch()`; **CONFIRMED LIVE on Mac 2026-07-16** — an HP webcam mic hit peak
> −17 dBFS, `mic_level()` tracks voice cleanly, `mic_pitch()` is now a YIN detector — octave-safe,
> tracks a hummed voice cleanly, reports 0 when unvoiced). See
> §"Demand check + the live-throughput dimension" + the stem-separation verdict below. **Tier 1 is
> the recommended engine entry point.**

**Shipped (2026-07-16) — the Tier-1 ENGINE SEAM (desktop backend + demo).** The spike graduated into
the engine, built on the [`platform.h`](../../runtime/platform.h) host seam so it reaches all devices
without the raylib/miniaudio symbol clash the spike would have hit. Key decision: the engine core is
**device-free** — the mic is the mirror of the audio-OUTPUT seam, *inverted*. Output is PULLED from the
engine (`de_audio_render`); input is PUSHED into it (`de_audio_input`, platform.h §4). Each host owns its
own capture device + permission flow; the engine only ANALYZES the frames it is fed. So no capture library
lives in the engine (crucially, no second copy of the miniaudio raylib already bundles → no duplicate
symbols, no ABI version-coupling).
- **Analysis core:** [`runtime/mic.h`](../../runtime/mic.h) — pure, device-free: accumulates pushed frames
  into a window → RMS (`mic_level`) + a YIN pitch (`mic_pitch`, octave-safe — see below).
- **Cart API** (studio.h + studio.c, docs + shell): `mic_start()` / `mic_stop()` / `mic_active()` /
  `mic_level()` / `mic_pitch()`. Dormant + byte-identical until a cart calls `mic_start()` (which is what
  pops the OS permission prompt). New `teaches` tag `audio-input`.
- **Desktop host** (the one backend done + tested): [`runtime/mic_desktop.h`](../../runtime/mic_desktop.h)
  — a CoreAudio `AudioQueue` capture polled in studio.c's raylib loop (`-framework AudioToolbox` added to
  the desktop build commands). Apple-only; non-Apple desktop is a no-op (shipping targets are iOS + web).
- **Demo:** [`mictest`](../../tools/carts/mictest.c) — VU meter + pitch note. Gates green: build-all
  (517/517 non-box2d), soundcheck (sound.h untouched), DE_NO_RAYLIB compiles, no-crash scripted `mic_start`.
- **Desktop LIVE-confirmed (2026-07-16):** the maker ran `mictest` with a webcam mic — VU meter moved,
  pitch note appeared. The whole seam (host push → engine analysis → cart API) validated end to end.

**Shipped (2026-07-17) — the other three hosts, same seam (every shipping platform now covered):**
- **Android** — [`android/app/src/main/cpp/main.c`](../../android/app/src/main/cpp/main.c) opens a second
  AAudio stream in the INPUT direction (mono float, lazy on `de_mic_wanted`) pushing to `de_audio_input`;
  `engine.h` carries the seam decls; `RECORD_AUDIO` added to the manifest and requested at runtime via
  JNI (pure NativeActivity has no Java Activity, so it fires `requestPermissions` once and polls
  `checkSelfPermission`). C VERIFIED: compiles clean against the NDK (minSdk 26). Live mic + the JNI
  permission prompt need an on-device APK run (editor Android export → sideload).
- **WEB** — `getUserMedia` → a `ScriptProcessor` on a separate `AudioContext` (zero-gain sink so the
  mic is never echoed to output) → `de_audio_input`, in both web shells
  ([`web_shell.html`](../../runtime/web_shell.html) + `web_shell_worklet.html`). Opens lazily on
  `de_mic_wanted()`. The seam is exported to JS via `EMSCRIPTEN_KEEPALIVE` + a malloc-free scratch
  buffer (`de_mic_scratch`). BUILD-VERIFIED (emcc compiles, both shells carry the glue, exports present);
  live mic needs a browser served over https/localhost (getUserMedia gate).
- **iOS** — [`AudioEngine.swift`](../../ios/Sources/AudioEngine.swift) installs an `inputNode` tap
  (lazy on `de_mic_wanted`, permission-gated via `requestRecordPermission`, session switched to
  `.playAndRecord`/`.defaultToSpeaker` only while listening) pushing to `de_audio_input`;
  `NSMicrophoneUsageDescription` added to `project.yml`; `engine.h` carries the seam decls; `CanvasView`
  polls it per tick. C seam symbols verified present in the `DE_NO_RAYLIB` build; the Swift host is
  written to the existing `de_audio_render` pattern — verified at the next device build (`ios/device.sh`).
**Shipped (2026-07-17) — YIN pitch detector + the first mic instruments.** The zero-crossing estimate
octave-jumped on a real voice (confirmed with the `pitchscope` diagnostic cart). Replaced it in
[`mic.h`](../../runtime/mic.h) with **YIN** (de Cheveigné & Kawahara — autocorrelation family with the
cumulative-mean-normalized difference that rejects octave errors): tracks a hummed voice cleanly, is
octave-safe, and reports 0 rather than guessing when unvoiced. Runs once per ~46ms window on the host
thread (search range ~63–1378 Hz). Confirmed live by the maker — "much better." Two carts now exercise it:
- [`pitchscope`](../../tools/carts/pitchscope.c) — the diagnostic: raw `mic_pitch()` on a log-freq axis
  (the before/after acceptance test for any detector change).
- [`humtheremin`](../../tools/carts/humtheremin.c) — the first voice-played instrument: hum → a
  vibrato'd, reverberant sine follows your pitch (light glide now that YIN is reliable) + volume, with
  a TAB pentatonic snap.

- **Follow-ups:** the iOS session/engine-restart dance may want on-device tuning; more mic instruments
  (beatbox-triggered drums off `mic_level` onsets); YIN runs on the capture thread — fine on desktop, worth
  a glance on the weakest mobile targets.

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

**Shipped (2026-07-14) — piece 5, PERSISTENCE:** the arrangement survives a reload. Needed two new
engine primitives (the sample-I/O the engine lacked — only `record_grab` could fill a slot, and
`sample_peaks` only read a downsampled view): **`sample_read(slot, out, max)`** (raw PCM out) and
**`sample_load(slot, data, n)`** (raw PCM in — the inverse, so a saved buffer restores without
re-recording; also unlocks the future WAV-load path). The sampler serializes the committed chops'
referenced take BUFFERS + the chop table + grid + tempo + grit through `save_bytes` (auto-saved on
any change via a dirty flag; ~230 KB for a 3-chop song), and `song_load()` at init restores it —
`sample_load` back into the slots, rebind the voices, done. A SONG button on the record screen jumps
straight to a restored song. Chose PCM over event-replay because the engine has no offline render
(replaying events would mean re-recording in real time at load) and PCM is immune to source drift +
handles future WAVs. Verified: build a 3-chop CRUSH song, relaunch cold → `song_n=3, grit=3`
restored, plays.

**Shipped (2026-07-14) — a SIBLING cart, [`breakchop`](../../tools/carts/breakchop.c) (the MPC/SP-404
break-chopper):** surfaced repeatedly by the demand tool (r/iosmusicproduction). A distinct honest core
from the sampler — *chop a LOOP into a tempo-locked pad kit and rearrange it* (source = an external
loop; slicing = even tempo-grid), vs the sampler's *sample your OWN output* — so it earned its own cart
rather than sprawling the sampler, reusing `INSTR_SAMPLE` + chop playback + `crush`. The loop comes from
[`data-tools/breaks`](../../data-tools/breaks) (fetch-and-freeze, the audio sibling of `roadview`),
loaded at runtime via the new `sample_load`; a **console-synthesised break** is the fallback so the cart
is self-contained. Slices into 8/16/32 even divisions (tempo DERIVED from loop length — no metadata),
lays them on a pad grid, PLAY reconstructs the loop (the correctness proof — verified: source amen
centroid 4318 Hz vs reconstruction 4057 Hz). The **eventual product surface** is *runtime* user-import
on device (file picker → `sample_load`) — capture-then-freeze keeps it deterministic; a separate piece.
The amen fixture is a **copyrighted dev placeholder** — gitignored, never baked in, swap to CC0 before
release (its `de:meta` todo carries the gate). This is also the [device-face
paradigm](device-face-paradigm.md §2)'s "workstation, not instrument" cousin — a *pipeline* cart, sibling
to the sampler.

**Next (piece 6):** an optional **seconds CAP** (flip the readout into a real budget), touch-drag for
the chop handles (currently mouse), a real choke group for the kit (`drumkit.h` doesn't declare one
yet — the engine's `instrument_choke` already does it, as tr808/tr909 do for the hats), lifting the SONG
into its own `songbox` cart (persistence is the bridge — now built), embeddable lo-fi songs in the
cart PNG (the save is disk-only today), the **doctrine call** (an ADR — does a sampler whose source
is the console's OWN synthesis clear STATUS #21's "analog-circuit machines only" line? the argument
is yes: an honest closed loop, not a caricature of a famous PCM box), and the determinism write-up
(capture-then-freeze made rigorous for `.rec`/`spec()`).

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

## Demand check + the live-throughput dimension (2026-07-16)

The [`reddit-gaps`](demand-discovery.md) drip keeps surfacing audio-input demand across every
music tribe it rotates through (r/synthesizers, r/ipadmusic, r/edmproduction, r/modular,
r/musicproduction). It's the one recurring cluster of on-grain wishes that the current shelf
**structurally cannot** answer — every music-cart run comes back "no gap, well-served," and the
genuinely-blocked wishes almost all trace back to "the engine has no ear." Two example shapes seen
repeatedly: *"convert humming into MIDI"* (r/musicproduction, twice) and the whole live-guitar-fx
cluster (*"live loop guitar", "live granular fx", "app like the Hologram Microcosm", polyphonic
live pitch-shift*). So the case for the seam isn't a hunch — it's the standing demand signal.

But "audio input unblocks a lot" splits into **two problems that are not the same**, and this doc so
far only frames one of them:

1. **Capture-then-freeze** (everything above — Tiers 1–4). The mic is an *acquisition device*; the
   captured audio is frozen into a cart-owned artifact and replay stays deterministic. Hum-to-MIDI is
   basically Tier 1 (`mic_pitch()` → notes); harvest-a-timbre is Tier 2. **This half is already
   designed** — it just isn't scheduled.

2. **Live throughput — the PEDAL / stompbox** (the most-demanded shape, *not* covered above). Play a
   signal *through* a self-contained effect and hear the result **now**: a fuzz, a granular cloud, a
   looper you overdub into. This is the opposite of capture-then-freeze — a live pedal *is* the live
   runtime dependency the doctrine forbids, so it can't be frozen into an artifact. It forces a
   **doctrine call**: either the harness records the live input as an input track (the Tier-1 "live by
   nature" exception, generalized) and accepts PCM in the `.rec`, or pedal carts are declared
   *live-only, no deterministic replay* — a real carve-out from the `.rec`/`spec()` guarantee.

**The cost, either way.** Audio input is a substantial multi-platform lift, not an API afternoon:
each backend needs its own mic plumbing + permission flow — Raylib (mac), WASM (`getUserMedia`), iOS
(mic-permission prompt), and the `DE_NO_RAYLIB` software engine. And the pedal use-case lives or dies
on **round-trip latency** — a laggy pedal is unusable, so problem #2 is a real-time-audio engineering
commitment, well above problem #1's tracker/analyzer work.

**Net:** the demand data makes audio input the most compelling *engine* frontier the discovery tool
has pointed at — but it opens **one on-grain slice at real cost**, not "a lot for free." The
sampling/analysis slice (Tiers 1–2) is the cheap, on-doctrine, already-designed entry; the pedal
slice is genuinely new work gated behind a determinism decision *and* a latency bar. What it does
**not** unblock is the biggest *raw* demand numbers (DAW / MIDI-routing / notation) — those stay
off-grain with or without a mic.

### Stem separation — asked for, but a hard NO at quality (2026-07-16)

Surfaces repeatedly in the sampling/hiphop tribes (*"isolate the acapella from a track"*,
r/makinghiphop). Verdict: **we do not build this.** The quality people mean (clean vocal/drum/bass
isolation from any mix) is **neural** — Spleeter (~tens of MB), Demucs htdemucs_ft (~300 MB of
weights) *plus* an ONNX/PyTorch runtime. Call it ~500 MB and a model dependency: the antithesis of a
humble lo-fi C cart. **No 500 MB carts here.** We would also lose the comparison every time.

The only buildable tier is classic no-model DSP — **karaoke center-cancel** (subtract L−R to null a
center-panned vocal) and **HPSS** (median-filter a spectrogram → rough harmonic/percussive split).
Both are tiny and deterministic, but they are *effects that occasionally sound like separation*, not
separation — a world below Demucs. Two gates even for that: it needs the **audio-input seam** above
(a track to load — though it's capture-then-freeze compatible, so unlike the pedal it doesn't break
`.rec`), and HPSS needs a **runtime STFT `sound.h` doesn't have** (we have offline analysis *tools*
like `harmonic-spec.js`, not an in-cart FFT). So: not the thing the wish asks for; at most a
deliberately-crude lo-fi *splitter toy* (constraint-as-character, like the SP-1200's bit-crush is a
feature) far down the road, gated behind audio input. Filed, not scheduled.
