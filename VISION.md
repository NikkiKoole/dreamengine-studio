# Eventually — Vision Notes

> **Status: early exploration. Nothing here is decided. This document captures thinking-in-progress, not a spec.**

---

## The Idea

A dream environment for learning programming through making games and interactive things. Inspired by DIV Game Studio, PICO-8, and BlitzMax. The language is C (or close to it), the tools are integrated, and the whole thing feels like a creative toy rather than a professional tool.

The north star: a teenager picks this up, makes something that moves on screen in under an hour, and learns real programming concepts without hitting a wall.

---

## References

- **PICO-8** — fantasy console constraints, all tools in one window, cartridge format, charming aesthetic
- **DIV Game Studio** — integrated sprite editor, the process/coroutine model for game objects, 320×200 feel
- **BlitzMax** — approachable language, game-focused stdlib, not condescending

---

## Target

- **Audience:** teens and hobbyists. Some prior exposure to code helpful but not required.
- **Platform:** both native (desktop app) and browser (zero-install, shareable). Probably native-first, browser as export/share target.
- **Constraints:** fantasy console style — fixed resolution, limited palette, limited sound channels. Constraints as a feature, not a limitation.

---

## The Language

Real C, or very close to it. Not a toy dialect — skills should transfer. A curated `studio.h` provides the game API (~20 functions). The learner never calls `malloc`. Stack and globals only to start.

### Open questions
- Exact API surface (not designed yet)
- Whether to allow `#include` of other files
- Whether strings need to be simplified somehow

---

## The Learning Model

Three levels of scaffolding, all running on the same runtime underneath:

**Level 1 — Beginner (shielded loop)**
```c
void update() {
    if (btn(RIGHT)) x += 2;
}
void draw() {
    spr(1, x, y);
}
```
No visible loop. Just fill in the blanks.

**Level 2 — Intermediate (process model, DIV-style)**
```c
process player() {
    x = 100; y = 100;
    loop {
        if (btn(RIGHT)) x += 2;
        spr(1, x, y);
        frame;
    }
}
```
Each game object is a coroutine. `frame;` yields to the next tick. Write each thing's behavior as its own story.

**Level 3 — Advanced (raw loop)**
```c
int main() {
    studio_init();
    while (studio_running()) {
        studio_frame();
    }
}
```
The curtain drops. Full control.

### Open questions
- Can a single cartridge mix levels 1 and 2, or do you pick one per project?
- Exact shape of the process model — how do processes communicate?
- Whether level 2 requires actual coroutine/fiber support in C, and how to implement that

---

## The IDE

### Overall layout
Undecided between:
- **PICO-8 style** — one fixed-size window, tabbed tools (code / sprites / sound / docs), game pops out on run. Most charming.
- **Split panel** — editor left, game preview right, tool panels below. More familiar.

Leaning toward PICO-8 style for the aesthetic, but nothing is fixed.

### The Text Editor
Using **CodeMirror 6** — not rolling our own. Reasons:
- Rolling your own editor is months of work (cursor, selection, undo, clipboard, wrapping...)
- CodeMirror is lightweight, modular, and built to be embedded
- Fully themeable — can look exactly like we want
- C syntax highlighting, inline error markers, custom keybindings all possible
- Works in browser and Electron

The editor should feel *distinct*, not necessarily *limited*. Custom pixel font, custom color scheme, minimal chrome.

### Open questions
- Exact visual style / color palette for the IDE
- Whether to add autocomplete for the studio API
- Whether to show a character/line counter for the fantasy-console feel
- Electron vs. pure browser for the desktop version

---

## The Tools

### Sprite Editor
- Tile-based pixel editor
- 8×8 or 16×16 tiles, up to 256 sprites
- Fixed palette
- Not designed yet

### Sound Tool
- 4-channel tracker/sequencer
- Square, sawtooth, noise waveforms
- Not designed yet

---

## Fantasy Console Spec (strawman, subject to change)

| | |
|---|---|
| Resolution | 320×200 |
| Colors | 32-color fixed palette |
| Sprites | 8×8 or 16×16 tiles, up to 256 |
| Sound | 4 channels |
| Code | C, no heap allocation |

---

## What We're Building First

Right now: getting the **code editor** into a state where we can see and tweak it. CodeMirror 6 with C syntax highlighting, dark theme, custom font. The rest of the IDE shell comes later.
