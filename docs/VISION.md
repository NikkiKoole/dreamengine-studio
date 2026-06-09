# dreamengine — Vision Notes

> **Status: in active development. Many things are now settled (marked ✓ where so); others are still genuinely open. This document is part roadmap, part scratchpad.**

---

## The Idea

A dream environment for learning programming through making games and interactive things. Inspired by DIV Game Studio, PICO-8, and BlitzMax. The language is C (or close to it), the tools are integrated, and the whole thing feels like a creative toy rather than a professional tool.

The north star: a teenager picks this up, makes something that moves on screen in under an hour, and learns real programming concepts without hitting a wall.

That north star still holds — it's the on-ramp. But a **second pursuit has grown up alongside it**, and it now drives much of the work: dreamengine as a tool for the maker *and Claude, together,* to build **deep, honest simulations hidden behind a humble lo-fi / SNES-ish surface** — one true core per cart, set up so the system tells you the truth (the "legendary" carts: `orbit`, `coaster`, `galerijflat`, `sloop`). It isn't a pivot — the learning-toy constraints are *why* the deep sims read clearly. The fuller reflection (the thesis, why the combination is load-bearing, the honest tensions and forks) is in [`design/second-north-star.md`](design/second-north-star.md).

---

## References

- **PICO-8** — fantasy console constraints, all tools in one window, cartridge format, charming aesthetic
- **DIV Game Studio** — integrated sprite editor, game-object-per-entity feel, 320×200 resolution
- **BlitzMax** — approachable language, game-focused stdlib, not condescending

---

## Target

- **Audience:** teens and hobbyists. Some prior exposure to code helpful but not required.
- **Platform:** native desktop done (Electron + clang). Browser sharing target and iPad runtime are still future work — the editor renders in a browser tab but ▶ run currently needs Electron because it spawns the compiler. Touch input (`stick_x/y`, `tap`, on-screen stick + A/B) is already wired in the runtime to make the iPad path easier later.
- **Constraints:** fantasy console style — fixed resolution, 32-color palette, 8 sound voices. Constraints as a feature, not a limitation. ✓ Settled.

---

## The Language

Real C, or very close to it. Not a toy dialect — skills should transfer. A curated `studio.h` provides the game API. The learner never calls `malloc`. Stack and globals only to start.

The API has grown well past the original "~20 functions" sketch — currently ~120 functions plus ~80 constants, organised into sections (graphics, input, touch, sound, map, utility, palette, screen). Help-tab grouping, hover tooltips, autocomplete, and Cmd-click-to-help compensate for the size, but it's worth pruning later if it starts to feel overwhelming.

### Open questions
- Whether to allow `#include` of other files
- Whether strings need to be simplified somehow
- How much to keep adding to the API vs. teaching learners to write their own helpers

---

## The Learning Model

Two levels of scaffolding, both running on the same runtime underneath:

**Level 1 — Beginner (shielded loop)** ✓ This is what the editor currently produces.
```c
void update() {
    if (btn(RIGHT)) x += 2;
}
void draw() {
    spr(1, x, y);
}
```
No visible loop. Just fill in the blanks. Game objects are plain C — a typed
static array with an `on` flag (`Enemy enemies[64]; bool on;`) and a `for` loop
that skips inactive slots. Across the whole cart corpus this immediate-mode-over-static-pools
style has proven to be enough; the algorithm and the data layout *are* the lesson.

**Level 2 — Advanced (raw loop)** — Not done. Currently `studio.c` owns `main()` and the user fills in `update()` / `draw()`. To unlock this we'd need to make `main()` opt-out (e.g., a `#define STUDIO_NO_MAIN` or a separate header that exposes `studio_init/frame/running`).
```c
int main() {
    studio_init();
    while (studio_running()) {
        studio_frame();
    }
}
```
The curtain drops. Full control.

> **Dropped: the DIV-style process/coroutine model.** Earlier drafts had a middle
> "process" level — each game object as a coroutine with its own `loop … frame;`
> body, needing C-side coroutines (longjmp/ucontext/protothreads) plus a syntax
> transformer. It was once billed as the headline differentiator, but the
> shipped carts are the counter-evidence: they all work cleanly with plain typed
> pools, and `broadcast`/`received`-style events cover the "objects talking to each
> other" need with none of the machinery. Weeks of architectural work for a model
> the cart corpus shows we don't need — cut. (Recorded in [`decisions/0001-cut-coroutine-process-model.md`](decisions/0001-cut-coroutine-process-model.md).)

### Open questions
- Whether Level 2 (raw-loop opt-out) is worth building, or whether the shielded
  loop is the only level this console ever needs.

---

## The IDE

### Overall layout
✓ Settled on **PICO-8 style** — one window, tabs across the top (code · sprites · sound · map · help · settings), game pops out on ▶ run.

A day/night theme toggle was added on top of the original dark-only plan — both themes share the same warm-cream / dark-paper palette family. Sprite and map editors share a common three-row toolbar layout (sliders → tools → status+hints).

### The Text Editor
Using **CodeMirror 6** — not rolling our own. Reasons:
- Rolling your own editor is months of work (cursor, selection, undo, clipboard, wrapping...)
- CodeMirror is lightweight, modular, and built to be embedded
- Fully themeable — can look exactly like we want
- C syntax highlighting, inline error markers, custom keybindings all possible
- Works in browser and Electron

The editor should feel *distinct*, not necessarily *limited*. Custom pixel font, custom color scheme, minimal chrome.

### Open questions
- ~~Exact visual style / color palette for the IDE~~ — settled on warm cream + dark paper (day/night). ✓
- ~~Whether to add autocomplete for the studio API~~ — yes, plus hover tooltips and Cmd-click-to-help / go-to-def in the editor. ✓
- Whether to show a character/line counter for the fantasy-console feel — still open.
- ~~Electron vs. pure browser for the desktop version~~ — Electron for native runs (needs to spawn clang); editor itself runs in a browser tab too, but ▶ run is Electron-only. Browser-as-sharing-target is still future work. ✓ for now.

---

## The Tools

### Sprite Editor ✓ Built
- Tile-based pixel editor, 16×16 tiles, 64 sprites (8×8 grid, 128×128 sheet)
- Fixed 32-color PICO-8 palette
- Tools: pixel · fill · select · stamp · line
- Brush slider + size inputs
- Animation frame strip with `1/2/3/4/d` keyboard nav
- Selection ops: `h/v` flip, `r` rotate, arrows shift
- Shift+click eyedropper auto-syncs the swatch palette
- Stamp auto-copies on selection drag (no need for Cmd+C)

#### Open question — what do "frames" actually mean?

The sprite editor lets you flip between N "frames" with `[1]/[2]`, where each frame is a snapshot of *the entire spritesheet*. But the runtime only ever sees one sheet — whichever frame is active at ▶ run becomes `sprites.png`. **That mismatch is the central design tension**, and the system was built before we knew what it should mean.

**What the interaction uniquely buys you.** When you flip between frames, the same canvas position shows different pixels. That is the gold-standard authoring affordance for animation — you draw frame 2 *with frame 1 as visual reference*, pixel by pixel. PICO-8 doesn't have this — you put each animation step in a *different* sprite slot and mentally align them. DIV Game Studio has something like it. We seem to have stumbled into a genuinely good authoring UI.

**Four directions, rough-to-most-promising:**

1. **"Just authoring scratch space."** Frames are an iteration aid; only the active one exports. Rename them "snapshots". Honest — throws away the runtime potential.

2. **"Alternate full sheets."** `sheet(n)` in the cart picks which whole spritesheet is active. Palette swaps, day/night, mode swaps. Coarse — less interesting than per-sprite.

3. **"Sheet-strips."** #2 and #4 combined. Strictly more capability; more concepts to teach.

4. **"Sprite animation strips." ★** Each sprite slot has an N-frame strip. Switching frames in the editor switches *which frame of every sprite* you're seeing. Runtime: `spr(idx, x, y)` auto-cycles at a sprite-defined fps, or `sprf(idx, frame, x, y)` for manual. Walk cycles become *data*, not code. Matches DIV (one of our stated inspirations), matches the terse-API aesthetic, and the authoring UI we already built is exactly the right UI for it.

**Tentative lean: #4.** No rush — the current behavior is harmless as a scratch tool until we commit. The interesting realization is that we've already built the most-novel half of the feature (simultaneous-position drawing) without realizing it; what's left is wiring up the runtime side.

### Map Editor ✓ Built — wasn't in the original vision
- 128×64 grid of sprite indices, dimensions configurable per cart (`-DMAP_W` / `-DMAP_H`)
- Same five tools as the sprite editor + brush + zoom slider
- Selection keyboard ops (arrows / h / v / r) operate on the selected rectangle
- Undo/redo with Cmd-Z, scoped to the map panel
- Stamp hover preview shows where the next paste will land

### Sound Tool — diverged from original vision
The original plan was a 4-channel tracker/sequencer UI. What got built instead is a **code-first sound API** — an 8-voice synth where making music **is** programming, which fits the learn-to-code mission. Strudel-inspired (`every`, `euclid`, `chance`, `degree`), with chords, strum, schedule, and a bpm/beat clock.

The full sound design — current engine, where it sits vs. the SID/NES chips, parameter placement, the gap ledger — lives in **[`design/audio-notes.md`](design/audio-notes.md)** (which also indexes the whole audio doc family); the navkit instrument-port program is **[`design/instrument-engines.md`](design/instrument-engines.md)**. Whether to *also* build a tracker UI later is still genuinely open.

---

## Fantasy Console Spec ✓ Settled

| | |
|---|---|
| Resolution | 320×200 (configurable per cart) |
| Colors | 32-color fixed palette (PICO-8) |
| Sprites | 16×16 tiles, 64 sprite slots (8×8, 128×128 sheet) |
| Map | 128×64 default, configurable up to 512×512 |
| Sound | 8 voices (more than the 4 in the original sketch — needed for chords) |
| Code | C, no heap allocation |

---

## What We're Building First

The first arc — getting a usable PICO-8-style fantasy console with a code editor, sprite editor, map editor, and code-driven sound — is done. A teen can write a cart with movement, sprites, a tile map, and a music loop in one sitting. Cartridge save/load (`.cart.png`), the tutorial gallery (~100 carts), inline error markers, and the emscripten web build all ship.

**Roadmap lives in [`STATUS.md`](STATUS.md)** — the single ledger of what's shipped, open, and cut. This file stays about the *why*, not the *what's-next*. The vision-level questions still genuinely open are below; everything else is tracked there.

### Vision-level open questions
- **Sharing the work** — the web build ships, but the "send your friend a URL" loop needs a host. What's the right shape — itch.io, a one-click publish, or a dreamengine-hosted gallery? (See [`guides/sharing.md`](guides/sharing.md).)
- **Sound: code-only or also a tracker?** The code-as-music primitives work; whether visual editing is ever needed depends on who ends up using this. (Design in [`design/audio-notes.md`](design/audio-notes.md).)
- **The sprite-editor "frames" question** (above) — still the central unresolved design tension in the tools.
