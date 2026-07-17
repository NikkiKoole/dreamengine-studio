# walkbox — a walking-bass step-sequencer ("303 done as a real pluck")

> **STATUS (2026-07-18):** BUILDING — the core sequencer + articulation lanes ship and
> play; open work is lower-priority garnish + a possible second (electric/sub) voice.

A TB-303-style **bass-line sequencer** whose voice is not an acid synth but the **upright's
real `INSTR_BOWED` pizzicato** — the cell nobody had: `acidcandy`/`tb303` are the 303 workflow on a
*synth* voice; `upright` is the pluck voice *played live*; **walkbox = 303 workflow + real pluck**.
The framing that stuck (the maker's): *"bassline303 but done right, as a real pluck instrument."*

Cart: [`tools/carts/walkbox.c`](../../tools/carts/walkbox.c). Lineage: `upright` (voice + tone/ring),
`tb303` (the per-step row idiom + clock swing), `acidcandy` (the note-bar free-draw + the drawable
p-lock lane it's modelled on). See also the bass-landscape survey below.

## The surface (what ships)

- **Note-bars** (pitch): 16 chunky bars, height = pitch, scale-locked to **E minor pentatonic** (2
  octaves) so any line walks. Tap = on/off, drag up/down = pitch, drag sideways = draw the line,
  drag to the floor = rest. While stopped, drawing a note auditions it.
- **A tabbed `VEL | LEN` lane** (one drawable lane, mode-switched — keeps the bars chunky instead of
  stacking two lanes):
  - **VEL** = per-step velocity → drives the pluck attack via `note_on` vol (1..7). Loud = accent,
    near-silent = ghost.
  - **LEN** = per-step note length → gates the note off partway through its step (staccato→legato).
    The **top of the lane = TIE** (`>= TIE_LEN`): the note detaches from the mono voice and rings on
    while the next note plucks (open-string overlap). A **slide** step holds full length so it can slur.
- **Thin binary rows** `SLD` (glide into the next note — a same-voice slur via `note_glide` +
  `note_pitch`, both directions) and `OCT` (tap-cycle +1 / -1 / off).
- **Knobs** GLIDE (slur time) · SWING (shuffle — delays the odd 16ths up to 0.6 of a step, LEN-gate
  is swing-aware) · TEMPO · TONE (pickup darkness) · RING (pluck ring-out). Plus PLAY, RND, CLR.

**Voice model:** monophonic held pizz voice, re-pitched per step. A fresh step plucks (`note_off` the
old, `note_on` the new); a slide re-pitches the *same* voice with glide (no re-attack); a TIE detaches
the voice (`voice = -1`) so it rings out untracked and the next note plucks fresh.

**Gotcha banked:** `ui.h` caps at **`UI_MAX_WID = 64` widgets/frame** and silently drops the rest —
per-cell row widgets (16×N) blow it and kill later widgets (the "TIE row + knobs dead" bug). Fix:
**one widget per row/lane**, compute the column from the touch x.

## Articulation roadmap

Done: ✅ dynamics (VEL) · ✅ note length + tie (LEN) · ✅ slide · ✅ octave · ✅ ghost (draw VEL low)
· ✅ swing.

Open (lower-priority garnish):
- **Ghost as a distinct muted timbre** — low VEL is only *quiet* now; a real dead-note thunk would
  drop harmonics/cutoff on very-low-velocity steps.
- **Hammer/pull** — a second "connected" flavour: legato with no pitch glide (vs SLIDE's glide).
- **Scoop / fall** — bend into a note from below / let it drop at the release (`note_pitch` ramp).
- **Presets + `< >`** and **save/load** the pattern (acidrack has the recipe).

## The bigger direction — a modern-bass box

There is **no modern/electric bass instrument** in the shelf: everything is acoustic double bass
(`upright`) or acid synth (the 303 family); the radio stations only use a bass *voice* in a mix.
walkbox's sequencer + lanes are **voice-agnostic** — swapping the `INSTR_BOWED` pizz for a
fingered-electric or synth-sub voice (+ maybe a per-step slap/mute articulation from `upright`'s
SLAP) turns the same box into a modern-bass sequencer, or a voice-select knob → "pick your bass,
walk a line." This is the natural expansion once the articulation set feels right.
