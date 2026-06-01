# docs/ — map & organizing rule

These docs are organized by **genre and durability**, not by topic — because the mess
they replaced came from one file trying to be reference *and* status *and* rationale at
once, so part of it was always stale. Each file now answers one kind of question.

> **The rule.** **Status** lives only in `STATUS.md`. **Settled rationale** lives only in
> `decisions/`. **Vision/philosophy** lives only in `VISION.md`. **Exploratory design**
> lives in `design/`. Docs *cross-reference*; they don't *duplicate*. If two docs say the
> same thing, one is wrong — fix it at the source and link.

## Layout

```
docs/
├── README.md          you are here — the map
├── VISION.md          why & what: product idea, audience, philosophy, console spec
├── STATUS.md          state: the one ledger of shipped / open / cut
├── POLISH_TODO.md     work list: the per-game "juice pass" across the carts
├── HANDOFF.md         narrative + environment gotchas (not obvious from code/git)
├── decisions/         frozen, dated decisions (ADR-lite) — the "why we (didn't) do X"
├── design/            exploratory design notes (scratchpads) — rationale + proposals
│   ├── api-notes.md     engine API: classics survey, signatures, naming, cart-patterns
│   ├── audio-notes.md   sound: current engine, chip comparison, expansion roadmap
│   ├── cart-survey-api-priorities.md   cart-evidence-first memo: what real carts prove, filtered through existing decisions
│   ├── baked-rotation-atlas.md   pre-rotated sprite/shape atlas: fast blitted bodies, the centerline model, offscreen canvases
│   ├── font-rendering.md         playful text beyond print: shadow/outline/wave/typewriter, inline codes, PICO-8 comparison
│   ├── rasterization-consistency.md  OPEN: one consistent fill/outline/dither rule so shape edges always agree (seams, off-by-1, outline mismatch)
│   └── headless-autoplay.md  growing the debug harness toward windowless fast runs + agents that play to find bugs (navkit-informed)
├── guides/            how-to
│   ├── cart-authoring.md         the make-cart.js / tools/carts toolchain
│   ├── cart-authoring-prompt.md  reusable AI prompt for designing a new cart
│   ├── cart-specs/               per-game cart specs (paste under the prompt above); README = the index + assembly recipe, AGENT-MESSAGE.md = parallel-build add-on
│   ├── sharing.md                ways to publish finished carts
│   └── debug-harness.md          deterministic record/replay/script + trace (tools/play.js)
└── archive/           superseded / done notes, kept for history
```

## Which file does what

| Genre | File(s) | Changes… | Never holds |
|---|---|---|---|
| **Why/what** | `VISION.md` | rarely (direction shifts) | a roadmap or status (→ STATUS) |
| **State** | `STATUS.md` | constantly | design rationale (→ design/ or decisions/) |
| **Work list** | `POLISH_TODO.md` | as carts get polished | engine/API status (→ STATUS) |
| **Decisions** | `decisions/NNNN-*.md` | append-only (supersede, don't edit) | open questions (those are exploration) |
| **Exploration** | `design/*.md` | freely; it's a scratchpad | whether a thing shipped (→ STATUS) |
| **How-to** | `guides/*.md` | when the workflow changes | — |
| **Narrative** | `HANDOFF.md` | each work session | the roadmap (→ STATUS) |

The **engine API reference itself** is not in `docs/` — it lives in the code:
`runtime/studio.h` (declarations) and `editor/src/studioDocs.js` (the bilingual one-liners
that drive autocomplete/hover/help). Don't maintain a second copy of the function list in
prose — link to those. (This is why the old "~100 functions" counts kept going stale.)

## When you change something

- Shipped / cut / deferred a feature? → update **`STATUS.md`**, then the relevant design note.
- A choice is now *settled* (especially a "no")? → write a **`decisions/`** ADR-lite and link to it.
- Proposing new API? → **`design/api-notes.md`** (sound → **`design/audio-notes.md`**).
- Product direction or a core principle changed? → **`VISION.md`**.
- A doc went stale or started duplicating another? → prune it. This README is the contract.
