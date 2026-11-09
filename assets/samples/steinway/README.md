# Steinway D — 13 notes

A Steinway D from the **bitKlavier Grand Sample Library** (Piano Bar mic image) by **Daniel Trueman,
Princeton University**, recorded January 2021 in Taplin Auditorium.
<https://doi.org/10.34770/r75s-9j74> — licence **CC BY 4.0** (`LICENSE-CC-BY-4.0.txt`).

**Changes:** 13 notes (C1, F#1, C2 … F#6, C7, one every 6 semitones) of velocity layer 12 of 16,
from the 44.1 kHz / 16-bit release, each mixed to mono and cut to its first 3.2 s. That cut was
made by Flyfish (delirioux, <https://github.com/delirioux/flyfish>), from whose
`assets/samples-steinway/` these files were taken unchanged apart from the names
(`Fs` = F#). Only the recordings are used here: no Flyfish or Felucca code (GPL-3.0).

Anything that ships these sounds (a cart, an app, the web gallery) must carry the credit line:

> Piano: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0)

Used by: `runtime/steinway.h` (GENERATED from these files by `tools/gen-steinway.js`: 4-bit IMA ADPCM,
896 KB, so web/app builds play them; rerun the tool if a file here changes) and through it `grandkeys`
(a playable keybed, with Pleyel voicing + soft pedal) and `openstrings`. `pianoref` (the measuring probe)
still reads these WAVs directly, for the 16-bit reference. How they are used: the hybrid grand,
[docs/design/piano-engine.md](../../../docs/design/piano-engine.md#the-steinway-match-and-the-hybrid-2026-11-09).
