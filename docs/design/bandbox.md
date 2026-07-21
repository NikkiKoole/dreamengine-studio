# BandBox — the chord-chart SEQUENCER (build brief)

STATUS: BUILDING (2026-07-21) — **wired** (`tools/carts/bandbox.c`): the draw-only mockup is now a
real sequencer. Steps 1–6 of the build plan below are done — the chord lane analyzes via the harmony
brain, the ^/v spinner + keybed edit chords, the ported chordwise band plays every genre, every voice
takes per-cell p-locks (strum/inv/oct/7th · bass mute/walk · drum FILL · mel rest/accent · pad on/off),
and it carries a `spec()` (61 assertions, all green). Demo clip parked at `tools/clips/bandbox/01-doowop.beats`.
Open follow-ups: the shared `band.h` extraction (still copy-first — the p-lock shape is now known, so this
is the moment to consider it), and richer per-voice editors (density/register/kit) if wanted. The design
language stayed settled (see below). Sibling of `chordwise`; part of the chord-bloom thread in
[`bossa-rack.md`](bossa-rack.md).

---

## What it is (the soul)

A **160×100 device-face chord sequencer**: you compose a chord progression as a chart, and a
**genre band follows it** — chords, bass, melody, drums, pad — all driven by the one declared MODE.
It's the standalone *instrument* that `chordwise` (the demand-82 *analyzer*, a legible teaching toy)
pointed at but couldn't hold on one flat screen.

**The one thing that makes it a sequencer, not chordwise-with-a-skin:** every voice is a lane of
**lego-block cells**, and each cell is **p-lockable** (Elektron-style per-cell parameter overrides
on top of the voice's auto "follows the chord" default). chordwise did per-cell only for chords;
bandbox does it for every voice.

Two-part bar (ADR-0022) to protect: it must stay **verifiable** (deterministic; carry a `spec()`)
**AND legible/delightful on glass** — a device you *play*, not a config screen.

---

## Current state (what the commit contains)

`tools/carts/bandbox.c` is a **draw-only mockup**. Working:
- The full face layout at 160×100 via `face.h` (`face_resize()` → the candy-family canvas).
- **Glass 5-lane tracker** (CHORDS/BASS/MEL/DRUMS/PAD × 8 blocks), rail headers aligned to lanes,
  playhead column glow, p-locks shown orange, tap a chord cell → editor morph.
- **Chassis** chrome: voice rail (row headers), nav (`KEY`/`MODE`/▶/`?`), keybed.
- FONT_TINY throughout; glass-vs-chassis materials; per-cell editor drawn *in the glass*.

**Not** real: all state is hardcoded (`CH_NAME`/`CH_RN`/`LOCK`/`VON`); no audio, no harmony, no
keybed input, no playback. Only `selCell` (tap→editor) and BACK are wired.

---

## The DECIDED design language — do NOT relitigate

These were settled with the maker over a long iteration; build ON them:
1. **Materials.** Dynamic → **glass** (recessed, dark, lit, bezel): the tracker + the editor. Tactile
   → **chassis** (chunky, FONT_TINY, on the device body): rail, nav, keybed. Never mix.
2. **No right-column buttons.** Voices are chassis soft-keys flanking the glass on the **left**;
   everything to the right is glass. (Carved off the HERO — face.h forbids side-rail *bands*, but
   flanking the hero is sanctioned.)
3. **The screen MORPHS, the chassis stays.** The glass shows the tracker OR a tapped cell's block
   editor; the rail/nav/keybed never move. (This replaced an earlier full-screen overlay.)
4. **5-lane tracker** (not lane-at-a-time). Whole arrangement visible; cells only need a glyph
   (numeral / pip); real editing is the tap-to-morph editor.
5. **Lego blocks + p-locks** are the grammar for the whole device. Cells default to "follows the
   chord + genre"; p-locks are per-cell overrides that read as accented (orange).
6. **FONT_TINY** everywhere; compact chunky chassis (studied against `acidcandy`).

---

## Proposed data model (start here)

The chord is the spine; voices follow it; per-cell p-locks override. Something like:

```c
#define NBARS 8
typedef struct {
    int on;                     // is this bar active (part of the loop)?
    int rootPc, qual;           // the CHORD (the harmonic spine of the bar)
    // per-cell CHORD p-locks (-1 = inherit the global voice setting):
    int strum, inv, oct, voicing;
    // per-cell p-locks for the following voices (a sentinel = "just follow"):
    int bassLock;               // e.g. run / octave / mute
    int drumFill;               // 0 = groove, 1 = fill this bar
    int melLock;                // accent / rest / chosen note
    int padOn;
} Bar;
static Bar arr[NBARS];

// global voice defaults (what a cell inherits when not p-locked):
static int bassStyle, drumStyle, melOn, padOn, seventh, ...;   // mirror chordwise's controls
static int keyPc, modeSel;      // the declared key + genre (the band follows these)
```

Keep it deterministic (no `Math.random`-style life yet — chordwise deliberately omitted swing-jitter
for spec-ability; same call here).

---

## Build plan (ordered)

1. **Chord lane real.** Wire `keyPc`/`modeSel`; the chart shows real numerals via
   `hb_vocab_analyze` (harmony.h). Tap a chord cell → editor; `^`/`v` step it in-key (lift
   `nudge_cell` from chordwise). Keybed (keybed.h) sets the selected/next chord's root.
2. **Playback loop.** Port chordwise's loop: subdivide the bar (4 for bass, 8 for drums/mel/comp),
   the chord comps (`COMP_PAT`), bass walks (`play_bass_beat` + `rad_bass_to`), drums groove
   (`play_drum_step` + drumkit.h), melody blooms (`play_mel_step`), pad holds. All follow the
   current bar's chord + the declared mode.
3. **Genre band = chordwise's maps.** Copy `BASS_PAT` / `DRUM_PAT` / `FILL_PAT` / `MEL_PAT` /
   `COMP_PAT` / `SWING` from `chordwise.c` verbatim (they're data). See "reuse" below.
4. **Per-cell p-locks.** Each voice's block editor (tap a cell in that lane) sets its p-locks;
   playback reads p-lock-else-global. Drums: the FILL p-lock (the maker's canonical example).
5. **The voice block editors.** CHORDS = chord spinner + strum/inv/voice/oct (mostly built). BASS =
   style + octave (+ per-cell run/mute). DRUMS = style/kit/fill/intensity (+ per-cell fill). MEL =
   density/register. PAD = on/level. All render in the glass, chassis stays.
6. **spec()** — deterministic gate over the arrangement (chords analyze correctly; p-locks apply;
   the loop is stable). Model on chordwise's spec.
7. **Bake + commit per step** (`cart-commit.js bandbox`); park a demo clip once it plays.

---

## Reuse map (what's free / copy / later)

- **FREE (shared engine headers, already used by the mock or trivially added):** `harmony.h`
  (`hb_vocab_analyze`, `hb_suggest`, `hb_tones`, `HbVocab`), `radio.h` (`rad_bass_to`), `drumkit.h`
  (`dk_use`/`dk_fire`/`dk_fire_at`, DK_* roles), `keybed.h`, `face.h`/`lay.h`/`ui.h`.
- **COPY from `chordwise.c`** (data + the playback shape): the six genre tables above, `sound_chord`
  (strum/inv/oct + baseDelay for swing), `play_bass_beat`/`play_mel_step`/`play_drum_step`, the loop,
  `change_key` (transpose), `nudge_cell` (step-in-key). It's fine for these to diverge as bandbox
  grows per-cell p-locks — that's expected.
- **LATER — the shared `band.h`.** Once bandbox's needs settle, extract the genre band (idiom maps +
  voice-leading + swing) into `band.h` so chordwise, bandbox, and the radios all draw from it
  (third-customer rule; see the `chordwise` `de:meta.todo` + `bossa-rack.md`). **Do NOT extract
  prematurely** — copy first, extract when the shape is known.

---

## Gotchas / rules (learned the hard way this session)

- **Input-coupled face (the dubjam lesson, face.h header).** bandbox hit-tests raw touch (chart
  cells, keybed). Compute the layout ONCE at the top of `update()` into file statics (a `Face` + your
  derived rects) and read those from BOTH input and `draw()`, or taps desync from visuals on reflow.
- **`lay_cell(c, dir, n, i, gap)`: `dir==0` is COLUMNS, non-zero is ROWS** — and `EDGE_TOP==0`. For a
  vertical stack use `lay_grid(c, 1, n, i, gap)` (bit us on the voice rail).
- **Align derived chrome to its glass counterpart** by deriving both from the same split (the rail
  headers use the glass's `lay_grid(g, 1, VOICES, v, 1)` slots — see `zone_rail`).
- **REFLOW, never camera-scale** (ui.h hit-tests in canvas coords; a zoom desyncs every widget).
- **Music-cart prep:** this is a music instrument — skim `docs/guides/instrument-carts.md` +
  `radio-station-howto.md` conventions; keep the recipe docs current when voices ship.
- Effects are set-and-hold; `watch()`'s 2nd arg is a format string; British greys (`CLR_*_GREY`);
  don't shadow built-ins (`map`/`line`/…). (See CLAUDE.md "Key things to know".)

---

## Pointers

- Cart + mock: `tools/carts/bandbox.c` (commit `26e58cb6`).
- Engine to lift from: `tools/carts/chordwise.c` (the genre band + playback, fully working).
- The thread: [`bossa-rack.md`](bossa-rack.md) (chord-bloom convergence, the `band.h` idea),
  [`harmony-brain.md`](harmony-brain.md) (the harmony engine + roadmap).
- Face grammar: [`responsive-first-device-face.md`](responsive-first-device-face.md); worked faces
  `facedemo` (grammar), `acidcandy` (160×100 candy, the tiny-text/chassis reference).
