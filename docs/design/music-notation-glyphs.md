# Music-notation glyphs — the chart reads as music

STATUS: EXPLORING (2026-07-22) — symbol inventory + sourcing plan; no font/sprite work started.

> Born in a bandbox session: at 160×100 a locked tracker cell can only show an orange pip
> ("something is overridden here"). On a roomier glass (320×200 — the engine's native default;
> bandbox is `resizable: true` and face.h reflows), cells have room to show *what* the lock is —
> and almost every lock in [`bandbox.md`](bandbox.md)'s vocabulary has a **standard notation mark**
> waiting for it. The chart would read as music. This doc is the symbol wishlist + what has to be
> gathered/made, so the font/sprite work can be scoped when the time comes.

## The rule that keeps the stranger bar (ADR-0022)

Notation glyphs are legible to musicians and **opaque to beginners** (`𝄽` says less than "REST"
to a 12-year-old). The split that serves both:

- **Tracker cells get glyphs** — a cell today carries only a pip, so a glyph is strictly *added*
  information; nothing gets less legible.
- **Editor segs keep words** — the editor is where meaning gets learned; it stays verbal
  (FOLLOW / MUTE / HOLD / …). The glyph is decoded by tapping the cell.

## Tier 1 — chord spelling (biggest legibility win, smallest set)

| glyph | for | source |
|---|---|---|
| ♯ ♭ ♮ | real accidentals (`B♭` not "Bb") | **make** — bake into the font atlas |
| Δ | maj7 (`B♭Δ`) | **make** — font atlas (Greek delta, simple bitmap) |
| ° | diminished | **have** — CP437 0xF8 (degree), already printed by the MIXO vocab ("iii\xf8") |
| ø | half-diminished | **make** — a slashed circle; CP437 has no ø (0xF8 is the degree sign standing in today) |
| − / m | minor | **have** — ASCII |
| a drawn key signature | the KEY readout | **later** — composite of ♯/♭ glyphs |

## Tier 2 — the p-locks as notation (maps ~1:1 onto bandbox's vocabulary)

| lock | mark | source |
|---|---|---|
| AUTO / "follows" | 𝄎 simile ("repeat the pattern") — arguably the perfect glyph for an unlocked cell | **make** (font or sprite) |
| ANTIC | a **tie arc crossing the cell's right edge** — the anticipation *is* a tie over the barline (the implemented semantics) | **make** — drawn geometry (sprite/line), NOT a font cell: it spans a cell boundary |
| STAB | · staccato dot | **have** — ASCII |
| HELD | – tenuto (or an in-cell tie) | **have** — ASCII dash |
| TACET / REST / DROP | 𝄽 quarter rest | **make** — font atlas |
| STRUM DOWN/UP | ↓ ↑ | **have** — CP437 0x18/0x19 |
| STRUM ROLL | the vertical arpeggio squiggle | **make** — sprite (tall, cell-height) |
| WALK | ♩♩ walking quarters | **make** — font atlas (♪ ♫ exist in CP437 0x0D/0x0E; ♩ does not) |
| OCT+ / PAD HIGH | 8va | **have** — ASCII text |
| APPR | grace note (slashed ♪) | **make** — sprite or font |
| GHOST | (x) parenthesized notehead — how ghost notes are really written | **have-ish** — ASCII parens + x |
| PEDAL | 𝆮 the piano *Ped.* mark | **make** — font atlas (or just "Ped." text) |
| FILL / BUILD | tremolo slashes + a hairpin | **make** — sprite |
| SWELL | < crescendo hairpin | **have** — ASCII (a drawn hairpin reads better at width) |
| FEEL ACCENT | > | **have** — ASCII |
| FEEL HUSH | 𝆏 dynamic *p* (italic) | **have-ish** — ASCII "p"; italic glyph = **make** |
| FEEL STOP | 𝄓 caesura ("railroad tracks") | **make** — tiny two-slash bitmap |
| FEEL LIFT | no standard mark — "+2" / ↑2 is honest | **have** — ASCII |
| DRAG / future PUSH | no standard mark (real charts write "laid back") | **have** — ASCII arrows/text |

## Tier 3 — structure chrome (pays off when song sections land)

| glyph | for | source |
|---|---|---|
| 𝄆 𝄇 repeat barlines | framing the loop — the tracker *is* a repeated section | **make** — sprite (cell-height) |
| volta brackets (1. / 2. endings) | the VOLTA lock **shipped** (2026-07-22: ALL/ODD/EVEN/4TH + VAR2/VAR4 over a 4-round cycle) — the BRACKET GLYPH over the cells is what remains | **make** — drawn bracket + ASCII digit |
| 𝄋 segno / 𝄌 coda | the eventual SONG ORDER navigation (A A B A = D.S. al coda in costume) | **make** — font atlas |
| 𝄞 𝄢 + percussion clef | lane icons replacing CH/BA/ME/DR/PA rail text | **make** — sprites (the `flank` icon-glyph precedent) |
| ♩= 109 | metronome mark for the BPM readout | **make** — needs ♩ from Tier 2 |
| 𝄁 𝄂 double/final barline | loop-end emphasis | **have-ish** — two drawn lines |

## Sourcing summary — what actually has to be gathered/made

- **Free today (CP437 / ASCII):** ♪ ♫ (0x0D/0x0E) · ° (0xF8) · ↑ ↓ · # b · > < · ( ) x · dots/dashes ·
  8va/Ped./+2 as text. Verify each glyph exists in ALL the faces used (dos_8x8 certainly; FONT_TINY
  3×5 and FONT_SMALL 4×6 carry fewer — audit which codepoints they actually have).
- **Font-atlas bakes (text-flow glyphs, the [`font-rendering.md`](font-rendering.md) bitmap
  pipeline — same road FONT_SMOOTH took):** ♯ ♭ ♮ Δ ♩ 𝄽 𝄓 𝆏 𝆮 𝄋 𝄌 simile 𝄎. Design them at 8×8
  first (320×200 cells are roomy); a 3×5 ♯ is possible, a 3×5 𝄽 is not — small faces keep words.
- **Drawn geometry / sprite-draw.js glyphs (marks that size with the cell or cross cells):**
  the ANTIC tie arc (crosses the cell boundary!), arpeggio squiggle, hairpins at width, tremolo
  slashes, repeat barlines, volta brackets, clef lane icons. `flank` is the worked "icon reads
  where a word can't" precedent; author via `tools/sprite-draw.js`, iterate with `sprite-preview.js`.

## First probe (when picked up)

Per the mockups-first pattern: a **draw-only 320×200 mockup** of the bandbox glass with Tier 1 +
Tier 2 glyphs faked in (rects/lines/hand-placed pixels, no font work) — does the notation actually
*read* at cell size? Only then invest in the atlas bake. The ANTIC tie arc and the simile-mark
default cell are the two to judge first; they carry the "chart reads as music" thesis.

Related: [`bandbox.md`](bandbox.md) (the lock vocabulary this notates) ·
[`font-rendering.md`](font-rendering.md) (the bitmap-font pipeline) ·
[`design-system.md`](design-system.md) (FONT_* family + the two-part bar) ·
[`../decisions/0022-collaboration-is-the-north-star.md`](../decisions/0022-collaboration-is-the-north-star.md)
(the stranger bar the cells/editor split protects).
