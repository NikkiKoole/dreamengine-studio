# Live coding — edit your cart while it runs

The **live** run backend lets you change code and see it instantly, without a
compile-and-relaunch step. Edit a color, a number, a shape — the running window
updates in a fraction of a second, and your game state is preserved. It's powered by
[libtcc](https://bellard.org/tcc/) JIT-compiling the cart in-process; the why & how is
in [`../design/cart-as-script.md`](../design/cart-as-script.md). This page is the how-to.

> Desktop app only (it spawns a compiler), and currently **arm64 macOS** only.

## Turn it on

1. Open the **settings** tab → **run mode** → choose **live (libtcc)**.
2. Hit **▶ run**. The first run builds the live host (~1–2s) and opens a window titled
   *dreamengine (live)*.

Switch back to **native (clang)** anytime — that's the optimized build you ship with.

## The loop

Just **type**. ~⅓ second after you stop, the cart recompiles and the window updates
**in place** — no relaunch, no flicker. The build log shows `[tcc] compiled … N ms`.

- **Made a typo?** The offending line is marked red in the editor and the log shows
  `cart.c:LINE: error: …` — but the window keeps running the **last good version**. Fix
  it and it reloads; the marker clears.
- You don't have to press ▶ run after the first launch — saving/typing is enough. (▶ run
  still works and does the same thing.)

## Keeping state across edits

A hot-reload swaps your **code**, so plain globals reset and `init()` does **not** re-run.
To keep state (a position, a score) across edits, put it in the engine-owned block. The
starter cart already sets this up:

```c
#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))
STATE {            // declare your state once, with the fields you need
    float x;
    int   score;
};

void init(void)   { S->x = 10; }              // runs once, at first launch
void update(void) { S->x += 1; S->score++; }  // S survives reloads
void draw(void)   { rectfill(S->x, 90, 16, 16, CLR_RED); }
```

Now change `CLR_RED` to `CLR_GREEN` while it runs — the box keeps sliding from exactly
where it was, score intact. (Cmd/Ctrl-click `STATE`, `S`, or `de_state` for the in-app
help.)

The rule of thumb: **want it to survive an edit? put it in `S`. Fine with it resetting?
a plain global is fine.**

## Hot-reload vs. relaunch

| You change… | What happens |
|---|---|
| Code | **hot-reload** in place — `S` kept, `init()` not called |
| Sprites or screen size | **relaunch** — fresh window (those are baked into the host) |
| The `STATE { … }` field layout | relaunch on purpose (see below) |

When a relaunch happens you'll see `↻ relaunching live window (…)` in the log — that's
expected, not a crash.

## Resetting state on purpose

Want a clean slate (re-run `init()`, zero out `S`)? **Close the window and hit ▶ run** —
a fresh launch starts `S` empty and runs `init()` again.

You must also do this after **reshaping `STATE`** (reordering or retyping existing
fields): a hot-reload reinterprets the old bytes under the new layout and you'd get
garbage. Adding a field at the *end* is safe (it's zero-filled). When in doubt after a
struct change, relaunch.

## Gotchas & limits

- **`#include <stdio.h>`** if you call `snprintf`/`printf` — on arm64 a variadic call
  without its prototype prints garbage.
- **No crash sandbox** — a bad pointer still takes the window down with a
  `*** cart crashed ***` message. Just ▶ run again.
- **`de_state` is RAM, not disk** — it survives reloads *within a session*, not a full
  quit. For save-between-runs (high scores, progress) use `save()` / `save_bytes()`.
- **Ship with native.** A cart written with `STATE`/`S` compiles and runs identically
  under the native (clang) backend — `de_state` is just a normal allocation there — so
  develop live and ship native with no changes.
