# tracker — a Song→Chain→Phrase tracker cart (LSDJ/M8 lineage)

STATUS: DESIGNED — ready to build (2026-07-02). Specced from an orientation pass (shelf gap +
ADR-0003 re-read) and a study of what the Dirtywave M8 / LSDJ lineage got right. Shape decided
(single-track phrase view, mnemonic FX, Song→Chain→Phrase); instrument lineup stays intent-first
at build time per the [cart-authoring firewall](../guides/cart-authoring-prompt.md). Nobody's on it.

> Working cart name: **`tracker`** (plain, discoverable). If a better name shows up at build
> time, rename before first commit — the name is not load-bearing.

## Why — the gap on the shelf

No cart has the tracker essence, and **no cart on the whole shelf has song *arrangement***:

- `drummachine` — one 16-step pattern, drums only, no pitch, no song.
- `loopstation` / `otafamily` — live loopers: record gestures, don't edit cells.
- `mariopaint` — note placement, but no channels / effects / structure.
- `groovebox`, `acidrack`, `delia`, the rebirth racks — per-machine pattern lanes, rack-shaped
  (knobs + one pattern per box), not the spreadsheet-of-music.

[ADR-0003](../decisions/0003-code-first-sound.md) deferred a tracker as *editor UI* ("deferred,
not rejected"). A tracker **cart** sidesteps that decision entirely: it's an instrument cart on
the shelf, not editor infrastructure — the same move the whole cart library is built on.

Why it fits unusually well: ProTracker was 320×256, the M8 is 320×240, we are 320×200 with an
8×8 DOS font — the aesthetic is native, not costume. QWERTY note entry (the mandated
GarageBand musical-typing map) *is* the tracker heritage. And deterministic playback makes it
honestly verifiable — the two-part bar's favourite kind of cart.

## What we learn from the M8 (and LSDJ)

The M8 is the modern proof that a tracker works on exactly our canvas. Five lessons, each of
which resolves a design question we'd otherwise re-derive:

1. **Song → Chain → Phrase, not monolithic patterns.** A *phrase* is 16 steps. A *chain* is a
   list of phrases **with per-entry transpose**. The *song* is columns of chains, one per track.
   Massive reuse falls out: one bassline phrase serves every chord of the song via chain
   transpose. FastTracker's 64-row all-tracks patterns don't fit a 40×25-char screen; three
   small nested views do.
2. **The phrase view shows ONE track.** This is the M8's answer to the width crunch and it's
   ours too: 4 tracks × (note+inst+fx+val) is ~47 chars — doesn't fit 40 columns. One track's
   phrase with full columns fits easily; the song view is where you see all tracks side by side
   (chain ids are 2 chars each). Don't fight for a multi-track note matrix in v1.
3. **One screen per view, arranged on a spatial map.** M8 views live on a 2D grid (Song ↔ Chain
   ↔ Phrase ↔ Instrument) navigated by shift+arrows — no menus, no dialogs, no scrolling chrome.
   Perfect for a fantasy console: every view is one fixed 40×25 screen.
4. **FX commands are 3-letter mnemonics, not hex digits.** Even the hardcore modern tracker
   abandoned ProTracker's `A0F` single-hex-digit commands for `ARP`/`VIB`/`RET`/`POR`. That
   settles the hex-vs-gentle fork from the design conversation: named commands + a small value
   are *both* authentic **and** stranger-legible. We keep note names (`C-4`) too, not hex.
5. **Cursor + nudge editing, no modes.** Every cell is edited in place: move cursor, hold edit
   + up/down to nudge the value (or type a note on the keybed). No insert mode, no dialogs.
   LSDJ shipped this on a Game Boy d-pad + 2 buttons — it also gives us the touch/gamepad story
   for free later.

(Also noted for later, deliberately NOT v1: M8 *tables* — per-instrument micro-sequences — and
*live mode* — queueing chains per track for performance, which would rhyme with `loopstation`.)

## The shape (v1)

**4 tracks**, any instrument on any step (no fixed channel roles — the M8/FamiTracker move,
not LSDJ's PU1/PU2/WAV/NOI). The 8-voice engine leaves headroom for FX-spawned extra notes.

### Data model (~6 KB/song, `save_bytes`-sized)

```c
typedef struct { unsigned char note, inst, fx, val; } Step; // note 0=empty, else MIDI
typedef struct { Step s[16]; } Phrase;                      // 16 steps
typedef struct { unsigned char ph[16]; signed char tr[16]; } Chain; // phrase id + transpose
// song: per track, an ordered list of chain ids (0xFF = empty)
static Phrase phrases[64];
static Chain  chains[32];
static unsigned char song[4][32];
```

64 phrases × 16 × 4 B = 4 KB, chains 1 KB, song 128 B. Whole document well under 8 KB —
`save_bytes()` for persistence now, and small enough to be the **lanes layer** of
[song-codec.md](song-codec.md) later (this cart is that doc's first real *author* — the first
cart whose document is edited state, not a seed).

### Views (three screens + instrument peek)

- **SONG** — 4 columns of chain ids, rows = song position. Play cursor sweeps down.
- **CHAIN** — one track's chain: 16 rows of `phrase id · transpose`.
- **PHRASE** — one track, 16 rows: `## · C-4 · in · FXv` (step, note, instrument, fx+value).
- **INST** — read-mostly v1: shows the selected preset's name + a few `ui.h` knobs (level,
  cutoff, pan). NOT a patch editor — the rack carts own knob-land.

Navigation: shift+arrows between views (the M8 spatial map), arrows move the cursor, edit-hold
+ arrows nudge, keybed types notes. Default 8×8 font; if density wants it, `FONT_SMALL` (4×6 →
80 cols) is the escape hatch, but v1 should try to stay chunky-legible at 8×8.

### The FX column — mnemonics over existing API

All of these are already in the engine; the tracker adds no engine work:

| Cmd | Meaning | Maps to |
|---|---|---|
| `ARP` | arpeggio x/y semitones | fast `note_pitch` cycling on the held handle |
| `POR` | portamento to note | `note_glide(h, ms)` + `note_pitch` |
| `VIB` | vibrato rate/depth | `note_pitch` wobble ridden per frame (built to be ridden live) |
| `VOL` | step volume | `note_vol` |
| `CUT` | filter cutoff | `note_cutoff` |
| `PAN` | stereo place | `note_pan` |
| `RET` | retrigger ×n | re-`note_on` subdivisions via `schedule`-style timing |
| `DEL` | delay trigger by ticks | offset the step's trigger inside the row |
| `HOP` | jump to step / end phrase early | sequencer flow control (cart-side) |
| `TPO` | set tempo | `bpm()` |

One fx slot per step in v1 (the LSDJ constraint — it forces musical choices and keeps the row
narrow). The clock is the house beat clock (`bpm`/`beat`/`beat_pos`) with steps scheduled a row
ahead — copy `drummachine`'s wiring, per the chassis rule.

### Instruments — intent-first, ~8 hand-tuned presets

Per the [firewall](../guides/cart-authoring-prompt.md): the lineup below is the *imagined ideal
chip-session band*, written before opening any cousin cart. At build time, shop each voice in
[instrument-recipes.md](../guides/instrument-recipes.md) / [instrument-presets.md](../guides/instrument-presets.md)
and reuse on purpose or build the gap — do **not** inherit a cousin's lineup.

The imagined band: a **bright pulse lead**, a **hollow second lead** (detuned/PWM-ish), a
**fat mono bass**, an **FM bell**, a **plucked string**, a **soft pad**, and a **drum kit**
(kick / snare / hat — membrane + noise territory). No patch editing in v1; presets are the
instrument bank, numbered `00–07` in the inst column.

## Input

- **Notes:** `keybed.h`'s GarageBand map (types into the cursor cell; also previews the sound).
  A USB MIDI keyboard therefore enters notes for free.
- **Cursor/edit:** arrows + edit-hold nudge (LSDJ two-button grammar). Space = play/stop,
  from-here vs from-top on shift.
- **Touch/mobile:** v1 is honest about being keyboard-first (`mobile-lint` will say so). The
  LSDJ grammar means a later touch pass is just a d-pad + 2 buttons via `touch_layout()`, and
  tap-to-place-cursor is cheap — noted, not promised.

## Verifiability (the two-part bar)

- **Verifiable:** a `spec()` (the `streetlab` pattern) — load a baked demo song, run headless,
  assert the scheduled note stream at exact beats; deterministic by construction. A parked
  `tools/clips/tracker/` input track mints the demo gif.
- **Legible-and-delightful:** the stranger test is "type three notes on the home row, press
  space, hear them loop — then discover the song view zooms out." The demo song ships in the
  cart so the first play button press already makes music (the radio-cart lesson: never boot
  to silence).

## Cut from v1 (deliberately)

- Samples / wavetables — the `INSTR_*` engines are the point (LSDJ-not-Amiga).
- Patch editor / instrument tables — racks own knobs; tables are the M8 v2 idea.
- Live performance mode — rhymes with `loopstation`, revisit after v1.
- Multi-song banks, import/export beyond `save_bytes` — the song-codec `?song=` blob lands
  only when that doc's grow-as-we-go trigger fires.
- Raw hex anywhere. Mnemonics + decimal-ish values only.

## Relationship to the racks program

Complementary, not competing: [tinyjam-racks](tinyjam-racks.md) /
[rebirth-classic](rebirth-classic.md) are **generator-first** (seeded machine → tweak);
the tracker is **document-first** (blank page → author). The shared future is the lanes layer:
a radio's frozen output could someday land in tracker phrases. Nothing in v1 depends on that.

## Open questions

- **Rows-per-beat / groove:** fixed 4 steps/beat with a swing knob, or per-phrase speed
  (LSDJ groove tables)? Lean: fixed + swing knob in v1.
- **Track count 4 vs 8:** 4 fits the song view at 8×8 chunky; 8 needs FONT_SMALL. Lean: 4.
- **Where transpose lives:** chain-entry transpose only (M8 style) vs also a song-row
  transpose. Lean: chain only.
