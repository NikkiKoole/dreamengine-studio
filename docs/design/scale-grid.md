# scale-grid — a scale-locked isomorphic pad grid

STATUS: PROTOTYPED (2026-07-07) — **the playable showcase shipped** as the `scalegrid` cart
(`tools/carts/scalegrid.c`), device-tested on multitouch and pinned by a 71-assertion `spec()`. The
"where does it live" question (§3) is answered **B-then-C**: built as its own cart first, with the
grid maths kept self-contained so it lifts cleanly into a `grid.h` library — **the `grid.h` extraction
is the one open step**. Born from the epiano responsive spike but it is **not** epiano-specific — it's
a general note-input surface, split out of [`epiano-layout-brief.md`](epiano-layout-brief.md) so the
piano redesign stays faithful and this good-but-separate idea got its own home.

> **Why this doc exists (the drift check, 2026-07-07).** Designing epiano's responsive layout, we
> co-designed a *new* input surface — a scale-locked pad grid — onto the mock. It's a genuinely nice
> idea, but the real `epiano` is a **chromatic piano keybed you slide across** (`keybed.h`); a grid
> is a *different* gesture (Push/Launchpad-like). Bolting it onto the Rhodes would blur the
> instrument's soul. So: **epiano keeps its classic piano keybed** (its faithful Phase-3 redesign);
> the grid becomes an **optional, separately-homed feature** — and the maker wants *both, eventually*.
> This doc owns the grid; the brief owns the piano.

## 1 · What it is

A note-input surface where the player picks a **SCALE** and a **KEY** (root, C…B). Instead of a
piano's white/black keys you get a grid of finger pads:

- **Scale-locked** — non-chromatic scales show *only in-scale notes*, so there are **no wrong notes**;
  roots are tinted orange. Chromatic shows all 12 (sharps tinted darker). Shipped set = **11 scales**:
  chromatic, major, natural minor, minor + major pentatonic, dorian, whole-tone, harmonic minor,
  hirajoshi, blues, and **FOREST** (the open/spread SoundForest voicing `{0,2,5,9,11,16}`, from
  melodypaint's research — its top note reaches a maj3 past the octave; no canonical scale name).
  Our own naming/order, deliberately **not** a clone of Koala's list.
- **Isomorphic** — each row is offset by a fixed musical interval, so a chord/scale *shape* is
  identical in every key and octave **and survives a reflow**. **ROW toggles the only two layouts that
  are musical** (per [ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md)): **OCT** (row
  up = +1 octave — the clean scale map, roots recur in columns; the default) and **4TH** (row up =
  +a fourth — compact Linnstrument-style chord shapes). LINEAR and the arbitrary +N offsets were
  **dropped** (reflow-defective / not musical).
- **Fill both dimensions, finger-first** — the hard rule (maker's): a pad is **never below one finger**,
  and within that the grid packs as many pads as fit **across AND down**. A **SIZE** cycle (`PAD:S/M/L`)
  scales the pad unit so you can trade density for chunkier pads. Where the surface is wider than the
  scale, notes simply **repeat** (a shape is playable in several places — the Linnstrument feel), never
  leaving a gap. *(This supersedes the earlier "cap-and-grow to `GRID_MAX_OCT`" model.)*
- **No gaps, ever** — the isomorphic offset is clamped to `≤ columns`, so the lattice's degree coverage
  is contiguous. (This is the fix for the two bugs the mock's model would have had: chromatic dropping
  A#/B into the row gap, and pentatonic doubling columns.)
- **Optional HEX packing** — a `SQR ↔ HEX` toggle repacks the pads into interlocking **regular
  pointy-top hexagons** (a Tonnetz / harmonic-table layout, √3⁄2 row pitch) so all six neighbours are
  **equidistant** — a diagonal chord reach is the same finger stretch as a sideways one. Hit-testing is
  **nearest-centre** (a hexagon *is* the Voronoi cell of its centre — exact at the slanted corners).
- **Voiced by a cycle of engines** — a **VOICE** cycle plays the pads through `INSTR_*` engines: PD
  sweep pad (default) / EPIANO / MALLET / ORGAN / PLUCK, so the same grid can be heard several ways.
- **Delightful cold-open** — it self-plays a walking arpeggio on load; the first pad touch hands over.

## 2 · What shipped — the `scalegrid` cart

**`tools/carts/scalegrid.c`** — the playable, sound-bearing showcase (the `epianofit.c` mock proved
the *layout* silently; this is the real instrument). Fully polyphonic multitouch; on-screen chip bar
so it's playable on a phone with no keyboard:

`SCALE · KEY(+octave) · ROW(OCT/4TH) · OCT-/OCT+ · VOICE · SIZE · HEX · AUTO`
(keyboard mirrors: `S R I Z/X V P G M`, `H` help).

**Pinned by `spec()` — 71 assertions** (`node tools/spec.js scalegrid`): the no-gap lattice across all
11 scales × both row modes, the finger floor at every SIZE preset, the hit-test round-trip (square +
hex) *and* hex corner points, and hexagon regularity (all six edges equal, apex centred). So the
correctness we'd otherwise eyeball in pixel zooms is proven, not guessed. Clip:
`tools/clips/scalegrid/01-selfplay-then-hands`. **Confirmed on glass** (multitouch, device build).

## 3 · Where does it live? — ANSWERED: B → C

The original three homes:

- **A · a mode inside `keybed.h`** — rejected: `keybed.h`'s job is a *piano manual* (white/black,
  glissando); a grid is a different topology — a muddy two-headed header.
- **B · its own cart** — where it is now. Gave it a clean identity and a place to settle every design
  question by ear on a real device.
- **C · a new `grid.h` library** (twin of `keybed.h`, reusing `solo.h`'s scale-lock maths) — **still
  the destination.** The whole keybed shelf reuses it, and `epiano` composes it via the editor-swap.

**Resolution:** built as **B first** (the proving ground), but the grid maths are kept in **self-
contained pure functions** — `compute_grid` / `pad_midi` / `pad_center` / `pad_at` / `hex_verts` — so
the lift into **`grid.h` (C)** is a mechanical extraction, not a rewrite. That extraction is the open
next step; reuse `solo.h` for the scale→semitone maths rather than re-deriving it.

## 4 · Open sub-questions — resolved

- **Confirm the layout WITH SOUND** — ✅ done on device. OCT reads as the clean scale map; 4TH gives
  the compact chord shapes. Both earned their place, so ROW ships as an OCT↔4TH toggle (not a default
  with a hidden seam).
- **Isomorphic layout** — ✅ fixed degree offset, OCT default, 4TH toggle, LINEAR dropped.
- **`GRID_MAX_OCT` / range** — superseded. The grid fills both dimensions with finger pads; the **SIZE**
  cycle (`PAD:S/M/L`) sets density instead of a fixed octave cap.
- **HEX vs square** — added as a live toggle (was not in the original scope); equidistant-neighbour
  ergonomics vs. the tidy square map — the player picks.
- **Chord affordances** (chord pads / strum) — still **out of scope**; the isomorphic + hex layouts
  already make chord *shapes* ergonomic. A future note if it's wanted.

## 5 · Relationship to epiano (both, eventually)

`epiano`'s redesign ([`epiano-layout-brief.md`](epiano-layout-brief.md)) stays **faithful**: the
classic chromatic piano keybed that scales with device width + the reflowing pedalboard. The
**editor-swap** is the seam: once **`grid.h`** exists (the §3 open step), epiano can offer the
scale-grid as an *optional* alternate editor — the piano is the soul, the grid is a bonus. The
showcase now proves the grid is worth wiring in. Neither blocks the other.

### The merged face — a KEYS ↔ HEX toggle (the device-face design)

The concrete shape of "both, eventually": one [device-face](device-face-paradigm.md) cart — the warm
epiano machines (Rhodes / Wurli / Clav + their pedals) as the SOUND, with a **toggle in the nav spine
that flips the performance surface between the piano keybed and this grid.** Same instrument, two ways
to play it: real keys when you know them, no-wrong-notes hexes when you don't — dead-on for the "for
someone who knows nothing" demand. It's a **keybed-family** face (no step sequencer → zone 4 dropped),
and the *freed* zone-4 band is exactly where the grid's chips go when you flip to HEX.

**KEYS mode — the electromechanical piano:**

```
┌──────────────────────────────────────────────────────────┐
│ ▶  RHODES · WURLI · CLAV     [ KEYS | hex ]   C maj  -1oct │ ① SPINE — machine · KEYS↔HEX toggle · key/scale
├──────────────────────────────────────────────────────────┤
│  (BARK) (BRITE) (PHASE) (VOL)                              │ ② FOUR ENCODERS — knob 3 = active machine's pedal
├──────┬─────────────────────────────────────┬─────────────┤
│PEDALS│    ~ the Rhodes tines glinting ~     │ SCOPE       │ ③ DISPLAY flows: PEDALS (stompboxes, tap + knobs
│ TONE │     VIBE ◉   DYNO ○   PHASE ◉        │ HELP        │   inside) · TONE · SCOPE — the screen's soul
├──────┴─────────────────────────────────────┴─────────────┤
│              (zone 4 — none: keybed family, no steps)      │ ④ DROPPED
├──────────────────────────────────────────────────────────┤
│    W E     T Y U                                           │ ⑤ PERFORMANCE = the PIANO KEYBED
│  A S D F G H J K         slide = glissando                 │    (epiano's chromatic slide-across manual)
└──────────────────────────────────────────────────────────┘
```

**HEX mode — the same sound, flipped to the scale-grid:**

```
┌──────────────────────────────────────────────────────────┐
│ ▶  RHODES · WURLI · CLAV     [ keys | HEX ]   C maj  -1oct │ ① SPINE — unchanged; toggle now on HEX
├──────────────────────────────────────────────────────────┤
│  (BARK) (BRITE) (PHASE) (VOL)                              │ ② same encoders — the SOUND is identical
├──────┬─────────────────────────────────────┬─────────────┤
│PEDALS│    ~ animated machine screen ~       │ SCOPE       │ ③ same display + flows
├──────┴─────────────────────────────────────┴─────────────┤
│  SCALE: maj   KEY: C   ROW: OCT   PAD: M                   │ ④ the FREED band hosts the grid's chips (HEX only)
├──────────────────────────────────────────────────────────┤
│    ⬡ ⬡ ⬡ ⬡ ⬡ ⬡ ⬡                                          │ ⑤ PERFORMANCE = the HEX SCALE-GRID
│   ⬡ ⬡ ⬡ ⬡ ⬡ ⬡ ⬡    isomorphic · roots orange · no wrong  │    (Tonnetz packing; slide to glide a run)
│    ⬡ ⬡ ⬡ ⬡ ⬡ ⬡ ⬡    notes                                 │
└──────────────────────────────────────────────────────────┘
```

- **Sound is shared, input toggles.** Zones 1–3 don't change between modes — same machine, encoders,
  pedals, screen. Only zone 5 flips, and the KEYS↔HEX toggle is a persistent selector in the spine.
- **The freed zone 4 earns its keep.** A keybed-family face drops zone 4 — but HEX mode borrows that
  freed band for SCALE / KEY / ROW / PAD (the grid's selectors). KEYS mode leaves it empty. The
  alternate surface brings its own controls into the slot its body-plan freed up.
- **This is the editor-swap seam (§3), made a live toggle.** §3 wants epiano to compose `grid.h` as an
  optional alternate editor; this face is what that looks like to a *player* — not a settings choice
  but a first-class flip on the instrument. (So it needs the `grid.h` extraction first — the §3 open step.)
- The general rule this merge motivated: **the performance surface itself can be mode-toggled between two
  note-entry grammars** — folded into [`device-face-paradigm.md`](device-face-paradigm.md) §2.

Related: [`epiano-layout-brief.md`](epiano-layout-brief.md) (the piano half) ·
[`acidrack-ui-research.md`](acidrack-ui-research.md) (touch/density numbers) ·
[`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the playbook) ·
[ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md) (defaults + seams) ·
`tools/carts/scalegrid.c` (the SHIPPED showcase) · `tools/carts/epianofit.c` (the earlier silent
mock) · `runtime/keybed.h` · `runtime/solo.h` (scale-lock to reuse for `grid.h`) ·
[`device-adaptive-layout.md`](device-adaptive-layout.md).
