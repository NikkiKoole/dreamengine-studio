# Cart OS — carts as tools in a shared environment

> **Status: exploration, fermenting. Not a plan of record.** Captures a design
> conversation (session, 2026-06-25) sparked by *"we have hundreds of carts — could we
> make a sort of OS, terminal-based or an Amiga-Workbench GUI, where carts are tools that
> output data for other carts? Shared filesystem, shared processes…"* Nothing here is
> built. Whether any of it ships lands in [`../STATUS.md`](../STATUS.md), not here.
> Closely related and load-bearing: [`external-data-carts.md`](external-data-carts.md)
> (the `--data` pipe already shipping) and [`cart-as-script.md`](cart-as-script.md) (the
> persistent libtcc host).

---

## The idea

We have **412 carts** ([`editor/public/carts/index.json`](../../editor/public/carts/index.json)),
each tagged by `kind`/`teaches`/`genre`. The dream: stop treating them as 412 isolated
toys and treat them as **programs in an environment** — an OS, first maybe a terminal/shell,
eventually an Amiga-Workbench-style desktop, where:

- a cart is a **tool** that consumes input and produces output;
- you **pipe** one cart into another (`osm-roads delft | roadview`, `synth | scope`,
  `mapgen | platformer`);
- there's a **shared filesystem** the carts read and write (not the per-cart silo we have now);
- carts run as **shared / concurrent processes** on one desktop.

This doc separates the cheap, reachable parts from the expensive research parts, and names
the one decision that determines which it is.

---

## What already exists (the substrate is further along than it looks)

Three pieces of the OS are already built — just not framed as such.

### 1. The kernel is `main.cjs`

[`editor/electron/main.cjs`](../../editor/electron/main.cjs) already **is** a process
manager. It compiles a cart, spawns it as an OS process (`spawn()`, ~`main.cjs:365`,
`stdio: ['ignore','pipe','pipe']`), owns its working directory (`cwd: BUILD_DIR`),
manages its lifecycle (`killLiveHost`), and pipes its stdout/stderr into the editor's log
panel. That is a kernel's job description. **The carts are already "programs"; the
launcher is already a scheduler.** An OS shell is a UI on top of what's there, not a new
system underneath it.

### 2. A real pipe already ships (one stage of it)

The newest work in the repo is the proof of concept, documented in
[`external-data-carts.md`](external-data-carts.md):

- `tools/osm-roads.js --place "Delft"` writes `data/delft.json`;
- `roadview.c` loads it at runtime via `--data <file>` →
  `de_data_path()` ([`runtime/studio.c:4029`](../../runtime/studio.c), decl
  [`runtime/studio.h:793`](../../runtime/studio.h)) → vendored `runtime/json.h`;
- **drag a different `.json` onto the window** and it hot-swaps the input
  (`de_dropped_file()`, `studio.c:4032` / `studio.h:794`).

That is `producer | consumer` with live reload, working today. It is hardcoded to one
pair, but the *mechanism* generalizes: any cart that reads `--data` is already a pipe sink.

### 3. The persistent host (libtcc) is the right runtime shape

[`cart-as-script.md`](cart-as-script.md): the `-DDE_TCC` live backend is one long-lived
native process that JIT-loads a cart as a guest module and hot-reloads it, with
`de_state()` surviving the swap. Today it hosts **one** cart. But "one durable host
process, carts as swappable guest modules, host owns the memory" is exactly the substrate
a multi-cart shell wants — the multi-cart version is "more `TCCState`s in the same host,"
not a new architecture.

### What is genuinely missing

| Missing piece | Today | Notes |
|---|---|---|
| **Inter-cart messaging** | none | `broadcast`/`received` was *designed and cut* — see [ADR-0001](../decisions/0001-cut-coroutine-process-model.md). Zero IPC. |
| **Shared filesystem** | per-cart silos | saves are hard-isolated to `build/saves/<slug>/` (`main.cjs:~205`). Cart A cannot see Cart B's data. No `/shared`. |
| **Output convention** | input only | `--data` reads; there is no blessed `--out <file>` for "emit your result." |
| **Concurrent rendering** | one window/process | Raylib = one window, one GL context, one 8-voice audio device **per process**. The hard one (below). |

---

## The fork: where does the OS live?

This is the decision the whole idea hinges on. It is a weekend vs. a quarter, depending on
the answer.

| Capability | **Node/Electron-side** (extend the kernel we have) | **C / in-engine** (an OS that boots *inside* the console) |
|---|---|---|
| Shell / Workbench | a new IDE panel; spawns carts as it already does | a "shell cart" — but a cart can't enumerate or launch other carts (no API into the registry/launcher) |
| Pipes | trivial — kernel sets `--data`/`--out` and chains processes | needs new C IPC primitives |
| Concurrent windows | free-ish — multiple OS processes already work | needs a multi-`TCCState` host + canvas tiling + multiplexed audio |
| Shared filesystem | trivial — a real directory both processes `cwd` into | needs a sandboxed FS API exposed to C |
| Fits the fantasy-console soul | less — it's IDE chrome, not a cart | more — the OS *is* a cart, deeply on-brand |

**The tension in one sentence:** the cheap, powerful version lives in Node (it's just
orchestration of processes that already run); the charming, on-brand version lives in C
but fights the one-window / one-process / one-audio-device reality of Raylib at every
step.

These are not mutually exclusive over time — the recommended path builds the Node-side
substrate first (so composition *works*), and leaves an in-engine Workbench as an optional
visual skin over it later.

---

## Three tiers (rough → ambitious)

### Tier 1 — Pipes (the MVP; mostly already here)

Turn the hardcoded osm-roads→roadview pipeline into a general capability.

1. **A shared filesystem namespace.** A real directory — `build/fs/` (or reuse `data/`) —
   that any cart can read/write, sitting *beside* the isolated per-cart saves (which stay
   as they are). This is the "shared filesystem" ask, in its smallest honest form.
2. **An `--out <file>` flag** mirroring `--data`: a cart writes its result (JSON via the
   `json.h` schema in [`external-data-carts.md`](external-data-carts.md), or a `save_bytes`
   blob) to a path the kernel chose.
3. **A composition primitive in the kernel.** `run A | B` = run A with `--out tmp.json`,
   then run B with `--data tmp.json`. Every spawn primitive for this already exists in
   `main.cjs`. The UI can be as small as one text command line in an IDE panel.

This delivers **carts-as-tools + a shared filesystem + composition** for roughly one IDE
panel and one runtime flag. It needs *no* new concurrency, *no* IPC, *no* in-engine work,
and it generalizes work already in the repo. **Recommended first step.**

### Tier 2 — A shell

A persistent terminal: list carts (the `index.json` catalog is the `ls`), run them, chain
them, inspect the shared FS, keep a history. Still Node-side; still spawning separate
processes. The carts gain a notion of "stdin/stdout" only in the file-handoff sense of
Tier 1 — no live streaming between two running carts yet. The shell could itself be
rendered as a cart-styled panel for the aesthetic without being a C program.

### Tier 3 — The Workbench (the research project)

The Amiga dream: several **live** cart windows on one desktop, composited, maybe streaming
data to each other in real time. This is where the one-window/one-process wall is load-bearing:

- **Compositing N separate OS processes** into one desktop is very hard (you can't blit
  another process's Raylib window into yours).
- **The tractable route** is the multi-cart libtcc host: N carts as guest modules in one
  `-DDE_TCC` process, each drawing into its own `RenderTexture`, tiled into a desktop the
  host composites — with a multiplexed frame budget and a shared/mixed audio device. This
  is real work (a frame scheduler, audio mixing across carts, per-cart input routing,
  crash isolation that today takes the whole host down — see `cart-as-script.md` Step 4),
  but it reuses the host model rather than inventing one.
- **Live streaming between carts** (true pipes, not file handoff) needs the IPC that
  ADR-0001 cut. A ring-buffer in host-owned memory (the `de_state` neighbourhood) is the
  natural shape if/when it's wanted.

---

## Honest tensions (the fermenting part)

- **North-star fit.** [`VISION.md`](../VISION.md) and
  [`second-north-star.md`](second-north-star.md) frame dreamengine around *deep, honest
  simulations behind a humble surface* — "legendary carts, one true core each," explicitly
  *not* a pile of toys. An OS is meta-infrastructure: a frame around the carts, not a cart.
  It's genuinely cool and very Amiga, but it's a sideways move from the main pursuit. The
  most defensible framing is **showcase / play**, and it dovetails with roadmap work
  already blessed — an OS desktop is a natural home for the curated showcase and
  [attract-mode](attract-mode.md) embeds ([ADR-0020](../decisions/0020-in-house-tool-curated-showcase.md)).
  Worth being deliberate that this is that, not a pivot of the core.

- **The DIV ghost.** We already cut the coroutine/process model once
  ([ADR-0001](../decisions/0001-cut-coroutine-process-model.md)) because the cart corpus
  didn't need it. A multi-process OS with live IPC reintroduces much of that machinery.
  The Tier-1 pipe MVP deliberately sidesteps it: it composes *separate* runs through the
  filesystem — no shared address space, no scheduler, no message bus. Reach for IPC only
  when a concrete cart pair demands live streaming that file-handoff can't serve.

- **No sandbox.** A cart is unsandboxed native code (`cart-as-script.md` caveats). A
  shared filesystem the carts write to is fine for a local in-house tool; it is not a
  security boundary, and shouldn't pretend to be.

---

## What would prove the magic cheapest

Build Tier 1 against the pair that already half-exists. Concretely: add `build/fs/` + an
`--out` flag, then make `osm-roads | roadview` run as a *composed pipeline the kernel
drives* rather than two manual commands — and confirm a second, unrelated pair works too
(e.g. a generator cart emitting a tilemap that a game cart loads via `--data`). If two
unrelated carts pipe cleanly, the abstraction is real and the shell/Workbench become
optional skins over a working substrate. If it's awkward, we learned that for the price of
one flag.

## Open questions to sit with

- Is the shared FS a flat blob store, or does it carry the typed "vector features" /
  tilemap / audio-buffer schemas so pipes are *typed* (a cart advertises what it eats and
  emits)? The `index.json` metadata could grow `consumes`/`produces` tags.
- Terminal-first or Workbench-first as the *visible* surface, given the substrate is the
  same either way?
- Does "shared processes" ever mean genuinely concurrent live carts (Tier 3), or is
  sequential file-handoff (Tier 1) actually the whole need — the same question ADR-0001
  answered "no" to for coroutines?
- If a Workbench is built, is it a cart (on-brand, fights the runtime) or an IDE panel
  (cheap, off to the side of the console feel)?
