# The tool you're building: MARIO PAINT — SOUND (note composer)

## What it is
A playful melodic music maker (the Mario Paint "Sound" / Composer mini-tool). Stamp
cute instrument icons onto a multi-octave staff/grid, hit play, and a **playhead loops
left→right** singing your tune. A creative TOOL, not a game — no win/lose; the goal is
that it's instantly fun to noodle on and genuinely makes music with the engine synth.

## Slice (locked) — 🟢
A note grid of **pitch rows × beat columns** (e.g. ~2–3 octaves of a scale × 16–32
steps), a palette of **~6 instrument "stamps"** (each a sprite icon + a synth voice),
click to place/remove a note of the selected instrument, a looping playhead that plays
each column on the beat, a tempo control, clear, and **save/load a song** (`save_bytes`).
Mouse-driven and cute. Differentiates from `drummachine` (percussion grid) by being
**melodic + multi-octave + stamp-based**.

## Core features
- **The staff/grid:** rows = pitches (snap to a scale so it always sounds nice — offer
  major/penta), columns = time steps. Each cell can hold a note; show notes as the
  chosen instrument's little sprite sitting on its row.
- **Instrument stamps:** a row of ~6 icons (e.g. a different critter/shape per timbre),
  each mapped to a synth voice (`INSTR_*` or a custom `instrument` slot with ADSR/
  filter). The selected stamp is what you place.
- **Playback:** a playhead column sweeps in time with `bpm`/`beat`/`every`, playing every
  note in the current column via `note()`/the synth. Loops the bar. Play/Stop toggle,
  tempo up/down.
- **Edit:** left-click place, right-click (or click an existing note) erase; clear-all;
  optional scroll if the song is longer than the screen.
- **Save/load:** persist the grid with `save_bytes` so a tune survives between runs.

## UI & layout
Top/most of the screen = the note grid; a side or bottom toolbar = the instrument-stamp
palette + PLAY/STOP + tempo + clear + save/load. Keep it big, colorful, and obvious —
a kid should get it in five seconds.

## Feel (the "juice")
Notes **bounce/squash and flash** as the playhead strikes them; the playhead is a bright
sweeping bar with a soft glow; placing a note gives an instant little preview blip;
each instrument has a distinct color so the score reads like confetti; a gentle
background groove option. Make playback visually musical — the screen should dance.

## Data model (suggested)
- `int grid[ROWS][COLS]` storing instrument id per cell (0 = empty).
- `instr_def[]` = {sprite, synth voice, color}; `scale`, `bpm`, `playing`, `playcol`.
- map a row index → MIDI via `degree(scale, octave, n)`. `save_bytes` the grid.

## Controls
Mouse-primary: click the staff to place/erase the selected instrument, click a stamp to
select it, PLAY/STOP + tempo + clear + save/load buttons. Keyboard: SPACE play/stop,
1–6 pick instrument, C clear, arrows tempo.

## Lean into / read
`drummachine.c` (sequencer + playhead off `beat()` — the structural cousin), `omnichord.c`
(scale/chord UI + the synth), `20-instruments.c`/`21-lfo.c`/`22-filter.c` (defining
voices), `moog.c` (rich synth), `typesave.c` (`save_bytes`). Skip: sheet-music notation,
multi-track mixing, recording — a joyful single-loop stamp-a-tune toy.
