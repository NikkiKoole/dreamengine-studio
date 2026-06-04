# Cart-as-script — running cart C without an external compiler

> **Status: exploration, not a plan of record.** Captures a design conversation
> (session, 2026-06-02) sparked by "could clang-in-browser help our web editor?"
> Nothing here is built. State of any of it lands in [`STATUS.md`](../STATUS.md),
> not here. See also [`packaging-distribution.md`](packaging-distribution.md) and
> [`../guides/sharing.md`](../guides/sharing.md).

---

## The question

Today ▶ run shells out to a developer's `clang` + Homebrew `libraylib.a` to build a
**native** binary, and "Build for web" shells out to `emcc` to build
`cart.html/js/wasm`. Both spawn an external compiler — which is why ▶ run only works
inside Electron, never in a plain browser tab. The trigger was the Wasmer
["clang in the browser"](https://wasmer.io/posts/clang-in-browser) post: could we
compile carts client-side?

Two separable goals got tangled together; keep them apart:

| Goal | Best answer | Notes |
|---|---|---|
| **A.** Make native dev feel like scripting + instant hot-reload | **TCC / libtcc** | easy, near-instant, drops the `clang` spawn — *this doc* |
| **B.** Run carts in a plain browser tab | emscripten (server-side or in-browser) | heavier; its own project |

The Wasmer route specifically does **not** solve B: its clang emits **WASIX** (headless
POSIX) wasm, not a graphics-capable target. dreamengine needs raylib → WebGL, whose
only supported web backend is **emscripten** — a different toolchain that doesn't
compose with WASIX clang. We already have that emscripten path working locally; what's
missing for B is hosting (see `sharing.md`) and/or moving `emcc` itself into the
browser (much bigger).

So this doc is about **goal A**: treat *the cart code* as a script.

## What "cart-as-script" means

The fantasy-console field splits into two camps, and dreamengine is an awkward third:

- **Camp 1 — interpret a scripting language at runtime** (PICO-8, TIC-80). The console
  itself is compiled to wasm once; carts are Lua/JS, *interpreted*, never compiled. No
  per-cart build step → web "just works".
- **Camp 2 — the cart *is* precompiled wasm** (WASM-4, MicroW8, TIC-80 wasm mode). The
  runtime hosts a wasm VM; the user brings a `.wasm`. Toolchain runs on the user's box.
- **dreamengine (third case):** users write **C against a native raylib runtime**, and
  we want to compile *that*. Uncommon — both camps above exist precisely to avoid it.

"Cart-as-script" = borrow Camp 1's architecture with **C as the guest language**: fix
`studio.c` + raylib as a **native host compiled once**, and make the cart a
**hot-swappable guest module** loaded at runtime. Crucially, **the cart already looks
like a script** — it only uses the `studio.h` API plus plain C, defines only
`draw()`/`update()`/`init()`, owns no `main()`, never touches raylib directly. The
*only* thing turning that "script" into "part of the program" today is the static link
step.

## The tool: TCC / libtcc

[TinyCC](https://bellard.org/tcc/) compiles C **in memory and executes it in-process**,
no external toolchain. The `libtcc` API is the whole story:

```c
TCCState *s = tcc_new();
tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
tcc_add_symbol(s, "spr",   spr);      // expose the studio.h API to the cart
tcc_add_symbol(s, "print", print);    // ... the cart's C calls these directly
tcc_compile_string(s, cart_source);
tcc_relocate(s, TCC_RELOCATE_AUTO);
void (*draw)(void)   = tcc_get_symbol(s, "draw");
void (*update)(void) = tcc_get_symbol(s, "update");
// the native loop now calls these function pointers each frame
```

What it buys:

- **No more spawning `clang`** — the compiler becomes a library inside the runtime.
  That's the entire reason ▶ run is Electron-only today.
- **Near-instant compiles** (~MB/s; carts are a few KB → sub-10ms) → recompile on every
  keystroke.
- **Live coding / hot reload** — the real prize: the native `main()` + raylib loop stay
  persistent, and on each edit you recompile the cart in memory and swap the
  `draw`/`update` pointers. PICO-8's "edit and it's instantly live" feel, in C. Arguably
  a bigger UX win than the browser story.

## How close the current wiring is (it's *very* close)

Traced `runtime/studio.c`. The cart is referenced at **exactly three symbols** —
everything else in the file is the host:

| Cart symbol | Linkage | Called from |
|---|---|---|
| `init()`   | weak stub (`studio.c:481`) | once — native `:942`, web `:630` (after click-to-start) |
| `update()` | weak stub (`studio.c:481`) | every frame — `:692` |
| `draw()`   | strong (cart **must** define) | every frame — `:701`, inside `BeginTextureMode(canvas)` → `BeginMode2D(cam)` |

The loop:

- **`loop_step()`** (`studio.c:621`) is the whole per-frame body (input snapshot →
  `update()` → render `draw()` into the low-res `canvas` → scale to window). One
  function, shared by native and web.
- **`main()`** (`studio.c:847`) does setup, calls `init()`, then dispatches:
  web → `emscripten_set_main_loop(loop_step, 0, 1)` (`:948`); native →
  `while (!WindowShouldClose()) loop_step()` (`:950`).

> Note: the `emscripten_set_main_loop` fork is **already in place** — a real
> `PLATFORM_WEB` build ships. Nothing to add there.

**The change for cart-as-script:**

1. Replace the 3 direct calls with function pointers `cart_init` / `cart_update` /
   `cart_draw`. After `tcc_relocate`, fill them via `tcc_get_symbol`; NULL for
   init/update → keep a no-op stub (exactly today's weak-stub semantics), `draw`
   required.
2. **Register the studio API surface** (~100 functions the cart can call) with
   `tcc_add_symbol`. This is the bulk of the work — best driven by a generated export
   table (an X-macro list, or parse the `studio.h` declarations) so it can't drift.
3. Constants (`CLR_*`, `BTN_*`, `FILL_*`, `KEY_*`) are `#define`s in `studio.h` →
   compile straight into the cart, no registration. Only `SCREEN_W`/`SCREEN_H`/`SCALE`
   (today `-D` flags) need `#define`-ing into the cart compile.
4. Architecturally this flips "link `cart.c` + `studio.c`" into "`studio.c` is the host;
   it loads `cart.c` via `libtcc`." `studio.h` becomes the formal **host↔guest FFI
   boundary** — which is the right mental model for a fantasy console anyway.

## One cart, many backends — libtcc and clang coexist (they're not either/or)

libtcc does **not** replace clang; it's additive. The load-bearing realization: **the
cart source is identical regardless of how it's compiled** — only the build *backend*
changes. So the architecture is one cart, N backends:

| Backend | Target | Role |
|---|---|---|
| **libtcc** (JIT) | this desktop, in-process | the **live edit loop** — instant compile, hot-reload |
| **clang** (AOT, `-O2`) | native binary | optimized "release"/ship build, good diagnostics |
| **emcc** (AOT) | wasm + WebGL | browser (already shipping) |
| **clang / Xcode** (AOT) | iOS/iPad app | mobile |

The dev loop defaults to libtcc for speed + hot-reload; an "Export" action picks an AOT
backend per target. The cart author writes the same C throughout.

**libtcc is a *development* tool, not a *distribution* tool.** Every route that *ships*
something — standalone binary, web build, iPad app — needs an AOT compiler. You'd never
want libtcc-only: you'd lose all distribution targets plus codegen quality and full
diagnostics. So clang/emcc/Xcode are needed *anyway*; libtcc just sits alongside for
iteration.

### iOS/iPad forces AOT — the clearest proof clang stays

**iOS forbids JIT entirely.** Third-party apps may not generate and execute code at
runtime (no executable memory for app code; the JIT exception is reserved for Apple's
own JS engine). So **libtcc literally cannot run on a non-jailbroken iPad** — its whole
mechanism (compile to memory → mark executable → jump to it) is exactly what's banned.
For iPad there's no choice: AOT-compile the cart + `studio.c` + raylib into the app
binary with clang/Xcode ahead of time. Far from replacing clang, the iPad route is one
where libtcc is *impossible* and clang is *required*.

### What makes coexistence clean (one design commitment)

The two paths use different linking models:
- **clang today:** cart + `studio.c` compiled and linked into one program.
- **libtcc:** `studio.c` is a persistent host; the cart is a guest module loaded at
  runtime, with the API injected via `tcc_add_symbol`.

Design the cart contract around the **stricter** backend (libtcc/JIT + iOS-friendly),
because anything that satisfies the strict path also satisfies the loose one:
- API declared as host functions → resolves via `tcc_add_symbol` (libtcc) *and* via the
  linker (clang). Same cart.
- Persistent state via the `de_state` block → survives hot-reload under libtcc, and is
  just a normal allocation under clang. **This is the unifying abstraction** — the
  "dedicated way to declare cart variables" isn't a libtcc wart, it's the thing that
  makes a cart behave identically across backends.

### Costs of supporting both (honest)

1. **A symbol-export table to maintain** — every API function must be registered for
   libtcc (`tcc_add_symbol`). Effectively a *fifth* place alongside the four-places rule
   in `CLAUDE.md`. Mitigation: generate it from `studio.h` so it can't drift.
2. **Two linking models in the build code** — `main.cjs`/`studio.c` grow a "host loads
   guest" path next to the existing "link them together" path.
3. **Parity risk** — a cart can behave subtly differently under `tcc -O0` (dev) vs
   `clang -O2` (ship): UB, float rounding, timing. Validate a cart under its *AOT*
   backend before shipping; "works in hot-reload" ≠ "ships correctly."
4. **An extra vendored dependency** — the source-built libtcc (see the spike's
   packaging caveat).

## Caveats (decide up front)

- **Cart globals reset on recompile.** A cart's `static`/global state lives in the
  TCC module's memory, so a hot-reload wipes it. To survive reloads, park persistent
  state in **host-owned memory** the cart indexes into (the Handmade Hero hot-reload
  pattern). Otherwise accept reset-on-reload.
- **No sandbox.** Interpreting/JITing arbitrary C in-process means a buggy cart can
  segfault or scribble host memory — `libtcc` gives zero isolation. For a *local
  learning tool* that's arguably fine (and educational; the existing `crash_handler`
  signal traps already catch SIGSEGV/SIGFPE/etc. at `studio.c:875`). Name it as a
  deliberate choice. wasm-based options give the sandbox back.
- **TCC ≠ clang.** C99-ish, good coverage, occasional feature/optimization gaps. For
  cart-sized code this almost never bites.
- **TCC is native-only.** It emits x86/ARM machine code, not wasm — so this serves
  goal A, **not** the browser (goal B). TCC-to-wasm exists only as experiments.

## Spike result (2026-06-02) — it works on arm64 macOS

A throwaway libtcc spike (`build/tcc-spike/`, gitignored) verified every core
assumption on this machine:

- **libtcc JIT works on arm64 macOS.** The make-or-break unknown passed. Caveat: the
  *stable* TCC (Homebrew 0.9.27) is **x86-only and deprecated** (disabled 2026-09-16) —
  we built **TCC mainline (mob branch) from source**, which builds cleanly and
  self-hosts as `0.9.28rc (AArch64 Darwin)`. A real integration must **vendor a
  source-built libtcc**, not depend on a package.
- **Compile-in-memory + call works** (`tcc_set_output_type(TCC_OUTPUT_MEMORY)` →
  `tcc_compile_string` → `tcc_relocate` → `tcc_get_symbol`).
- **Host API resolution works** — a cart's calls resolved to the host's compiled
  functions via `tcc_add_symbol`. This is the mechanism for the real ~100-fn surface.
- **Hot reload works** — loading a modified cart into a fresh `TCCState` and swapping
  the `draw` pointer changed behaviour mid-run with no process/window restart.
- **Persistent-state mechanism works** — a host-owned `de_state(size)` block survived
  the reload (continuous `x`/`frame` across the swap). This validates the "dedicated way
  to declare cart variables" approach: carts grab a state struct from a host-owned
  block, so it outlives recompiles. (Note: `static` cart globals are **not** reachable
  via `tcc_get_symbol` — only non-static globals — so route persistence through
  `de_state`, not by poking globals by name.)

**Verdict:** native cart-as-script + hot-reload is viable here. Note this de-risks
*goal A only* — TCC is native codegen, still no help for the browser (goal B).

### Step 2a (does TCC parse `studio.h` + what's the symbol surface)

Compiled the whole cart corpus to objects with TCC (`build/tcc-spike/objs/`):

- **TCC parses the real `studio.h` and 191/193 carts** clean. The 2 failures (`elite`,
  `mode7`) are **pre-existing breakage, not TCC**: `static` arrays named `star`/`ring`
  collide with `studio.h`'s later-added `star()`/`ring()` functions — **clang rejects
  them identically** (`redefinition … as different kind of symbol`). The exact
  name-a-var-after-a-builtin trap `CLAUDE.md` warns about; TCC just surfaced it (handy
  free corpus-lint).
- **Symbol surface is small and known.** Across the corpus: **155** studio API
  functions actually called (of 175 declared) → the `tcc_add_symbol` table needs
  ~155–175 entries, confirming it must be **generated from `studio.h`**, not
  hand-kept. Plus only **16** libc/intrinsic symbols total: `cosf fabsf floorf memcpy
  memmove memset powf roundf sinf snprintf sqrtf strcmp strcpy strlen strncmp strncpy`
  (resolve via TCC's `libtcc1.a` runtime + host symbols — verify in 2b). No
  `malloc`/file-I/O/`printf` — carts are pure compute + the studio API.

### Step 2b (real integration: studio.c + raylib loading a cart → a window) — WORKS

Flagged a `-DDE_TCC` host mode into `runtime/studio.c` (the normal build is untouched —
all changes are behind `#ifdef DE_TCC`):

- **`main()` loads the cart via libtcc** instead of static-linking it: registers the
  full studio.h API symbol table (generated by `tools/gen-tcc-symbols.js` →
  `runtime/studio_tcc_symbols.h`), forwards `SCREEN_W/H/SCALE` via
  `tcc_define_symbol`, resolves `init/update/draw` with `tcc_get_symbol`, and the
  existing `loop_step` calls them through pointers (3 guarded call sites).
- **Verified:** the `02-shapes` cart, compiled at runtime by libtcc, drove the real
  `studio.c` + raylib host and rendered a correct frame (rects/circles/lines/tris, the
  32-colour strip, `pset`, text) headless via `--screenshot`. The central unknown —
  *can JIT'd cart code call studio funcs that call raylib?* — is retired. The 16 libc
  symbols resolved automatically (no manual registration needed).

**Build gotchas worth carrying forward:**
- **Symbol clash:** libtcc's codegen leaks a global `load` (`ST_FUNC void load()` in
  `arm64-gen.c`) that collides with studio's `int load(int slot)`. `-load_hidden` did
  **not** fix it on this `ld`. Fix that worked: **build libtcc as a `.dylib`** so its
  internals live in a separate image (two-level namespace) — no static-link collision.
  A real integration must either vendor libtcc as a dylib or rename/​localize its
  leaked globals.
- Host build = `clang runtime/studio.c -DDE_TCC -DDE_TCC_INCDIR=… -DDE_TCC_LIBDIR=…`
  + `libtcc.dylib` (with `-rpath`) + the usual raylib/frameworks. Cart path via `--cart`.

Regression check: the normal static build (`cart.c + studio.c`, no `DE_TCC`) still
compiles — the `#ifdef DE_TCC` guards don't touch the static path.

### Step 3 (live hot-reload with persistent state) — WORKS

Added to the `DE_TCC` host: a host-owned `de_state(bytes)` block (zero-init, grows on
demand, never freed on reload) and a per-frame file-watch (`cart_reload_if_changed()`
stats the cart's mtime; on change it re-runs `cart_load` and hot-swaps the entry-point
pointers **without** calling `init()`, so state carries over). A compile error leaves
the running cart live.

**Verified** with a file-watch run (headless, frame dumps): edited the cart from a red
box (`V1`) to a green circle (`V2`) *while running*; the engine logged
`[tcc] hot-reloaded` and the next frames drew V2 — **and a frame counter stored in
`de_state` ran straight through the swap (61 → 595, no reset to 0)**. That's the payoff:
code hot-swaps, game state persists. Cart globals would have reset — which is exactly
why state must route through `de_state`.

**arm64 gotcha found:** a cart calling a **variadic** function (e.g. `snprintf`)
**without a prototype** silently prints garbage — on AArch64, varargs use a different
calling convention than the register-passed args TCC assumes for an implicitly-declared
function. Fix in the cart: `#include <stdio.h>`. Design implication: the cart template
should include `<stdio.h>` (or `studio.h` should, or we provide a non-variadic
number-print helper) so beginners don't hit this.

### Step 4 (robustness probes) — all green

- **Compile latency — fast enough for per-keystroke.** Across carts from 8 to 704
  lines: **0.77–6.5 ms** (most < 2 ms; `chess` at 704 lines = 6.5 ms; the lone 3.4 ms
  on tiny `01-hello` is first-call warmup). All comfortably under one 60 fps frame
  (16.7 ms), so recompiling on **every keystroke** is viable, not just on save.
- **Heavy cart works end-to-end.** `juice` (particles, shake, `fillp`, sound, shape
  rendering) compiled in 3 ms and rendered correctly via the host.
- **Compile errors are editor-ready.** TCC's error callback yields
  `broken_cart.c:2: error: ';' expected (got 'print')` — a clean `file:line: message`
  that maps straight onto inline markers. A failed compile aborts load cleanly (no
  crash); under hot-reload the previous cart stays live.
- **Crash isolation — bad cart takes the host down (with a diagnostic).** A null-deref
  cart hits the existing `crash_handler`, which prints `*** cart crashed: signal 11 ***`
  + last `watch()` values, then re-raises and dies. So you get a clear message, but the
  process ends — the "no sandbox" reality. **Opportunity the persistent-host model
  unlocks:** the static build *must* die, but a libtcc host could `sigsetjmp` before
  calling the cart and `siglongjmp` out of the crash handler back to a "cart crashed —
  waiting for a fixed reload" idle state, surviving the crash so the next hot-reload
  recovers. (Recovering from SIGSEGV mid-GL-call is genuinely unsafe in general, so
  treat as a design idea to validate, not a free win.)

## Productizing — backend toggle in settings — DONE (v1)

Shipped a **run-mode toggle** rather than replacing ▶ run. `▶ run` now obeys a setting:

- **Native (clang)** — default; today's AOT optimised build, full diagnostics, + the
  background Windows cross-build.
- **Live (libtcc)** — spawn the persistent `-DDE_TCC` host once; thereafter a Run/save
  rewrites `cart.c` and the host's file-watch hot-reloads it in place, with `de_state`
  continuity. The "liveloopy" dev mode.

(**Web (emcc)** stays its own "Build for web" button — it builds+serves rather than
runs locally, so folding it into the run toggle would be a category error. Native vs.
live is the run-button choice.)

What landed:
- **`de_state` is now a first-class `studio.h` API** (the four-places: declared in
  `studio.h`, implemented unconditionally in `studio.c`, documented in `studioDocs.js`,
  listed in `shell.js`). So a cart written for `live` compiles unchanged under `native`
  too (it's just a process-lifetime allocation there). The generator picks it up
  automatically (173 symbols now); the `DE_TCC` host no longer registers it by hand.
- **Vendored libtcc** at `runtime/libtcc/` (`libtcc.dylib` + `libtcc.h` + `tcclib/`
  = `libtcc1.a` + freestanding headers; ~0.5 MB, arm64-macOS, built from mainline). See
  its README. The packaged Homebrew TCC is x86-only, so vendoring the source-built dylib
  is the only option.
- **`editor/src/settings.js`** — `backend` setting (`native` | `live`), persisted, with
  a "run mode" select in the settings tab. The run button already passes `{...settings}`,
  so `backend` flows through with no `shell.js` change.
- **`editor/electron/main.cjs`** — `macTccHostArgs()` builds the host; `runLive()` (re)builds
  it when its signature changes (studio sources / symbol table / dims / sprites / map)
  and otherwise just reuses the running host so a code edit hot-reloads. `studio:run`
  branches on `cfg.backend`; a native run kills any live host first.

Verified at the runtime/build layer: the vendored host builds via the exact
shell-escaped command `main.cjs` emits, loads a `de_state` cart, and renders. The
Electron UI wiring itself is unrun here (no Electron in this environment) — syntax-checked
only; first manual `npm start` should exercise the toggle.

Known v1 limitations (documented for the next pass):
- **Sprites / screen dims are baked into the host**, so changing them triggers a host
  rebuild + relaunch (the live window restarts); only *code* edits hot-reload in place.
- Reload is **on Run/save**, not per-keystroke (latency says keystroke is feasible —
  a later refinement).
- **No crash sandbox** — a cart segfault still takes the host down (Step 4); the
  `sigsetjmp`/`siglongjmp` survive-and-wait recovery is not yet wired.
- libtcc is **arm64-macOS only**; other platforms need their own vendored build.

## Resolved by the spike (were open questions)

- **Symbol table:** generated from `studio.h` (`tools/gen-tcc-symbols.js`) — ~180 entries
  is far too many to hand-keep. The editor's live-host build regenerates it
  automatically before every host compile (`buildTccHost` calls `generate()`), so it
  can't silently drift; still run it manually after editing `studio.h` so the
  regenerated file lands in the same commit.
- **clang vs. libtcc:** not either/or — libtcc for the live loop, clang/emcc/Xcode for
  ship/web/iPad. See "One cart, many backends".
- **Hot-reload state model:** host-owned `de_state` block (PICO-8-like continuity) —
  proven in Step 3.

## Web editor: the cart contract is portable; *running in the browser tab* is the gap

Where a cart can run today, by editor:

| | Electron (desktop) | Browser tab |
|---|---|---|
| edit code/sprites/map | ✓ | ✓ |
| ▶ run — native (clang) | ✓ | ✗ (can't spawn a compiler) |
| ▶ run — live (libtcc) | ✓ | ✗ (native code can't run in a browser) |
| Build for web (emcc → wasm) | ✓ (spawns emcc) | ✗ (also needs to spawn emcc) |

So the live/native backends are **desktop-only by construction** — they're native machine
code, which a browser sandbox can't execute. None of the libtcc work moves the
browser-run needle. The browser tab is **edit-only** today.

What *does* carry over is the **cart source**: `de_state` is a normal `studio.h` function
(not `DE_TCC`-gated), so a `STATE { … } / S->field` cart compiles unchanged under
emscripten. **Verified 2026-06-02:** the starter cart built to `cart.html/js/wasm` via
the existing web path with no changes. So games written live on desktop are web-portable.

The remaining gap is **goal B from the top of this doc** — letting the *browser tab
itself* build+run a cart — still unbuilt. Two paths (see also `../guides/sharing.md`):
a **build server** (browser → emcc on a backend → wasm → `<canvas>`; needs a backend), or
**emscripten-in-the-browser** (no backend, much heavier). Nice structural note: a browser
hot-reload would recompile to wasm and hot-swap the module, and to keep state across that
swap you'd want the host (JS) to own the block — which is exactly the `de_state` shape, so
the *state model* already ports even though the *compiler* doesn't.

## de_state ergonomics — DECIDED: `STATE { … }; / S->field`, baked into the starter

Chosen: option 4 (the keyword pair) with `S` as the accessor, **defined in the starter
cart, not in `studio.h`**:

```c
#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))
STATE { int x; };          // declare once
void update(){ S->x++; }   // use everywhere — survives a live reload
```

**Why cart-local, not a `studio.h` built-in:** `S` is used as an identifier in ~45
existing carts (`static float S[4][4]`, …), so a global `#define S` would break them.
Putting the macros at the top of the starter (`EMPTY_TEMPLATE` in `shell.js`) gives every
new cart `S` for free while leaving `studio.h` and all existing carts untouched.
`de_state()` remains the real, documented API; `STATE`/`S` are friendly sugar over it.
Works identically under native clang (pure macros over the existing fn). Struct-reshape
gotcha unchanged: change fields → relaunch for a fresh zeroed block.

### Options considered (the chosen design combines #1 + #4)

`de_state` works, but the cart-author surface is a bit cryptic for beginners:

```c
typedef struct { float x; int score; } State;
#define ST ((State*)de_state(sizeof(State)))   // cast + sizeof + macro = scary line
// then ST->x, ST->score
```

The function itself is fine; it's the *spelling* that's unfriendly (the cast, the
`sizeof`, the macro, and the techy name). Options to weigh (not yet chosen):

1. **Zero-API change — bake it into the starter template.** Ship the welcome/empty cart
   with the `State` struct + `#define ST …` already written, plus an empty `State {}`.
   Beginners just add fields and use `ST->…`; they never type the scary line. Cheapest;
   normalises the pattern without touching the engine.
2. **Generic accessor macro in `studio.h`** — `#define de_state_of(T) ((T*)de_state(sizeof(T)))`
   → `de_state_of(State)->x`. Hides the cast/sizeof but repeats the type each use. Meh.
3. **Friendlier name** — `remember()` / `keep()` / `persist()` instead of `de_state`
   (keep the same mechanism). Low effort, warmer to read.
4. **A `STATE { … }` keyword pair** (strongest candidate for "looks friendly"):
   ```c
   // in studio.h:
   #define STATE  typedef struct DeState DeState; struct DeState
   #define S      ((DeState *)de_state(sizeof(DeState)))
   // in the cart — no typedef, no cast, no sizeof:
   STATE { float x; int score; };
   void update(void){ S->x += 1; }
   ```
   One word to declare (`STATE { … };`), one letter to use (`S`). Fixed names are fine
   for a fantasy console (one state blob per cart). Reads almost like a language feature.

Tensions to keep in mind while deciding: it must still compile cleanly under **native
clang** (it does — pure macros over the existing fn); keep the **struct-reshape gotcha**
teachable (changing fields → relaunch for a fresh zeroed block); and don't collide with
common cart identifiers (`S`, `ST`, `STATE` — check against the namespace trap in
`CLAUDE.md`). Decision deferred — the user wants to sit with the feel of it.
