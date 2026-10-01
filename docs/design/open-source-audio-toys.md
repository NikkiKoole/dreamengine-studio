# Open-source audio toys for people who aren't musicians: what to study

> **STATUS: RESEARCH (2026-10-01)** ‑ a reading list, not a plan. Candidates we could learn from, each
> with its license **read from the repo itself** (copy vs study is a license question first) and what is
> worth taking. The first harvest of this kind, CHOMPI, is done: [chompi-harvest.md](chompi-harvest.md).
> O&C's Tonnetz moves (§2.2) are harvested (the `tonnetz` cart). Plinky (§2.1) is parked: too touch-heavy
> for now (the maker wants mouse-friendly experiments first).

The common thread: instruments where **you can't play a wrong note, or the instrument does the musical
part for you**. Chordblossom (our homage to the Telepathic Instruments Orchid: you play chords, not notes)
is the house example. That's the north-star bar applied to music: legible and delightful to a stranger.

## 1 · The rule for using any of these

| license | what we may do |
|---|---|
| **MIT / Apache-2.0** | borrow code, with the copyright notice kept in the comment (as `BOW_BODY_HZ` credits STK and `tape_warble` credits CHOMPI) |
| **GPL / AGPL** | read it, learn the idea, re-derive it. Never paste. Our carts ship inside paid apps |
| **CC BY-NC-SA** (non-commercial) | read only. Nothing from it can reach a paid app, not even a re-typed table |
| **CC BY-SA** (share-alike) | read only for code. Data tables are a judgement call: ask first |
| **names, logos, panel art** | never, whatever the code license says (CHOMPI and Plinky both carve their art out) |

## 2 · Borrowable (MIT / Apache)

### 2.1 Plinky — MIT software, C · **top pick**
[github.com/plinkysynth/plinky_public](https://github.com/plinkysynth/plinky_public) · last push 2025-10.
An 8-voice polysynth played on **8 touch strips**, everything locked to a scale. It also has a latch
arpeggiator with probability, a granular sampler and wavetables. The repo's `LICENSE.md`: "the rest of the
software of plinky is licensed under the MIT License". The logo, panel and graphic elements are CC BY-SA
4.0; third-party parts (STM32 Cube, tinyusb, dear imgui, portaudio) keep their own licenses.
- **Why first:** same language as us, touch-first like our device faces, and made for anyone to play.
- **Study:** how a strip's position and pressure become pitch and expression · the scale lock · the
  arp/sequencer probability (compare with `patgen.h`) · the grain engine (compare with `grains()`).

### 2.2 Ornament & Crime: Harrington 1200 + Automatonnetz — MIT, C++ · **HARVESTED 2026-10-01**

> **Shipped:** [`runtime/tonnetz.h`](../../runtime/tonnetz.h) (the moves, the map geometry, least-motion
> jumps, the vector automaton; O&C's MIT notice kept) and the **`tonnetz` cart** (a mouse-first map you
> click or drag across, the 5×5 automaton, a pad that glides only the voice that moved, voice lanes).
> Checked: `tonnetz_selfcheck()` runs all 24 triads × 6 moves against O&C's own offset table, mutation-tested
> (break one move → 5 red). In a 30 s run of the cart, 32 of 34 chord changes moved exactly one voice (the
> other 2 were two-voice compound moves), never more than 2 semitones, and the register stayed in 54–69.
> **Two findings on the way:** (1) O&C's grid steps 1/6 short (at 24 bits `R/6` rounds down and has no +1
> nudge, unlike 1/7, 1/5 and 1/3), so its first 1/6 crossing comes a tick late. Ours counts a cell as 840
> units, which every fraction divides exactly. (2) Whole-cell vectors don't cover the grid: (1, 2) walks
> one 5-cell diagonal for ever. It's the fraction that sweeps it, e.g. O&C's default (1, 1/5).

[github.com/mxmxmx/O_C](https://github.com/mxmxmx/O_C) (`software/o_c_REV/APP_H1200.ino`, `APP_AUTOMATONNETZ.ino`,
`tonnetz/*.h`). The MIT notice is in each file header ("Copyright (c) 2015, 2016 Patrick Dowling, Tim
Churches"); the repo has no top-level LICENSE file. The Hemisphere Suite fork carries the same `tonnetz/`.
- **What it is:** neo-Riemannian chord moves. Each button moves the current triad to a close neighbour
  (P / L / R: change one note by a step). Any chain of presses sounds smooth, because every move changes
  only one note.
- **Why it matters here:** we have a Tonnetz *layout* (`scalegrid`'s HEX mode) but not the *moves*.
  It's a real harmony gap, small enough to read in an afternoon, and it fits `harmony.h` and chordblossom.

### 2.3 Generative.fm — MIT, JavaScript (Tone.js)
[docs.generative.fm](https://docs.generative.fm/docs/introduction) · pieces in
[github.com/generativefm/generators](https://github.com/generativefm/generators) (MIT).
About 50 endless Eno-style pieces, each a small program.
- **Study:** each piece is a recipe for "music that plays itself and never repeats". Directly relevant
  to the radio stations and to `loficity`'s arranger.

### 2.4 Piano Genie — Apache-2.0, JavaScript
In [github.com/magenta/magenta-js](https://github.com/magenta/magenta-js) (Apache-2.0, last push
2026-06) · [overview](https://www.imaginary.org/node/2342). A whole piano played with **8 buttons**.
- **The trick, without the neural net:** a button means "go up / go down by roughly this much", and the
  instrument picks the in-key note that fits. That's the part worth taking; the model isn't needed.

### 2.5 Chrome Music Lab — Apache-2.0, JavaScript
[github.com/googlecreativelab/chrome-music-lab](https://github.com/googlecreativelab/chrome-music-lab).
Song Maker, Kandinsky (draw → music), Rhythm, Arpeggios, Spectrogram…
- **Study:** UI legibility. These are the reference for "a stranger gets it in five seconds".

### 2.6 Orca — MIT · Sonic Pi — MIT (main source)
[github.com/hundredrabbits/Orca](https://github.com/hundredrabbits/Orca): a grid of letters that IS the
sequencer, hip and esoteric. [Sonic Pi](https://github.com/sonic-pi-net/sonic-pi): music by writing code,
built for schools. Its `app/` is MIT; `app/web/` is licensed separately, check before borrowing from it.
- **Why:** both sit right on our learn-to-code lineage. An Orca-style cart is a natural fit for a
  fantasy console.

## 3 · Study only (don't copy)

| project | license | worth a look for |
|---|---|---|
| [Deluge firmware](https://github.com/SynthstromAudible/DelugeFirmware) | GPL-3.0 | a mature groovebox sequencer; very active community firmware |
| OTTO ([bitfieldaudio/OTTO](https://github.com/bitfieldaudio/OTTO)) | **CC BY-NC-SA 4.0** | an OP-1-style groovebox. Non-commercial: nothing may reach a paid app |
| Bastl Kastle ([bastl-instruments/kastle](https://github.com/bastl-instruments/kastle)) | CC-BY-SA | a tiny drone synth anyone can play |
| Music Thing Workshop Computer ([TomWhitwell/Workshop_Computer](https://github.com/TomWhitwell/Workshop_Computer)) | per program (each `releases/*/LICENSE`) | many small card programs; check each one's license before using it |
| [SuperOS-808](https://synthanatomy.com/2026/08/superos-808-open-source-firmware-for-the-roland-tr-808.html) | not checked | open TR-808 firmware (2026), written with Claude |
| Freetribe (Electribe 2) | not checked | open firmware for the Electribe 2 |

Also found while searching: [Atarity/diy-synths](https://github.com/Atarity/diy-synths), a list of open
synths, for the next round.

## 4 · Already harvested: CHOMPI (MIT)

See [chompi-harvest.md](chompi-harvest.md). Shipped `tape_warble()`, `grains_repeat()`, `patgen.h` and
the `latchbox` cart. Still on the table from it:

### 4.1 The CHOMPI wavetables (MIT): how big, and what's in them
Measured 2026-10-01 from `card-profiles/wave-1.0/wavetable01..07.wav`:

- **On disk:** 7 tables × 33 frames × 2048 samples, mono, 32-bit float, 44.1 kHz. 270,472 bytes each,
  **1.89 MB** total.
- **What's actually in them:** no frame has a harmonic above the **64th** (a spectrum of frames 0/8/16/24/32
  of every table, threshold −60 dB). So 2048 samples per frame is ~16× oversampled; **256 samples per frame
  holds them exactly**.
- **Stored efficiently:** 256 samples/frame in 16-bit = **~118 KB** for all seven; 8-bit = ~59 KB. Small
  enough to bake into a cart or the engine.
- **⚠ Too bright for `wave_set()`:** a 64-sample table holds only 31 harmonics. Five of the seven tables
  reach the 63rd–64th harmonic in some frames, so they'd lose their top half. Doing them justice needs a
  real multi-frame wavetable voice with longer frames.
- **Two kinds of motion:** tables 02 and 05 move smoothly (mean frame-to-frame RMS step 0.013 / 0.060);
  the rest jump (0.15–0.30). A morph knob must crossfade between neighbouring frames, not step through them.

| table | brightness across frames 0 → 32 (highest harmonic) | frame-to-frame step |
|---|---|---|
| 01 | 63 → 63 → 63 → 13 → 5 (bright, darkens at the end) | 0.207 |
| 02 | 1 → 17 → 64 → 64 → 64 (a sine that opens up) | **0.013** (smooth) |
| 03 | 3 → 7 → 13 → 43 → 56 (slow brightening) | 0.238 |
| 04 | 3 → 7 → 11 → 19 → 27 (stays fairly dark) | 0.304 |
| 05 | 64 throughout | **0.060** (smooth) |
| 06 | 64 throughout | 0.150 |
| 07 | 1 → 13 → 19 → 63 → 64 (sine → full) | 0.207 |

### 4.2 The other open CHOMPI items
- **Factory samples** (168 TAPE + 29 TEMPO): MIT on paper; confirm with the authors that the *recordings*
  are meant to be included before any of them ships.
- **A second reverb to A/B ours against:** their `reverb.h` (Mutable Instruments, MIT), for
  [`tools/ref-render/`](../../tools/ref-render/).
- **`tape()` saturation adds up to +9.5 dB** (chompi-harvest §7): needs the maker's call.

## 5 · Suggested order
1. **Plinky:** clone and survey it like CHOMPI (most overlap with what we build).
2. **O&C Tonnetz moves:** small, clearly MIT, fills a real harmony gap → `harmony.h` / chordblossom.
3. **A wavetable voice** using the CHOMPI tables (§4.1), if a cart wants one.
4. **Piano Genie's trick** (relative buttons + scale snap) as a cart for absolute beginners.
5. **Generative.fm** pieces as study material for the next radio station.
