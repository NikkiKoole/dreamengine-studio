# Cart OS — carts as tools in a shared environment

> **TL;DR (where this landed):** not an OS — **a launcher + piper + viewer that *feels* like
> one but is mostly chrome over `spawn()` + the filesystem you already have.** Launch a cart
> (the `index.json` catalog + `main.cjs`'s spawner, already here), optionally pipe one cart's
> output into another (one `--out` flag mirroring the shipped `--data`), and show the results
> (the shared FS / a cart's output in a second pane — Norton-Commander style). The only
> genuinely-new primitive in the pile is the music **shared clock** (a live `{bpm, beat}`
> file); everything else is presentation. A weekend-ish, on-brand showcase toy — deliberately
> *not* a pivot of the "legendary carts, one true core" north star.

> **Status: exploration, fermenting. Not a plan of record.** Captures a design
> conversation (session, 2026-06-25) sparked by *"we have hundreds of carts — could we
> make a sort of OS, terminal-based or an Amiga-Workbench GUI, where carts are tools that
> output data for other carts? Shared filesystem, shared processes…"* Nothing here is
> built. Whether any of it ships lands in [`../STATUS.md`](../STATUS.md), not here.
> Closely related and load-bearing: [`external-data-carts.md`](external-data-carts.md)
> (the `--data` pipe already shipping) and [`cart-as-script.md`](cart-as-script.md) (the
> persistent libtcc host).
>
> **Update — the idea deflated to its honest size (same session, later).** The grand
> framing collapses to *save a file, load it elsewhere* — which the **real OS already
> does**, audio-mixing-across-processes included. There is no operating system to build;
> what remains is a convention, one flag, and a couple of use cases. See **"The honest
> size of it"** immediately below — read it before the tiers, which describe the full
> design space but overstate the need.
>
> **Update — un-hedged by the north-star decision ([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md), 2026-06-26).**
> Three caveats in this doc were beginner-shaped and now relax: (1) the **"NO new public API"**
> contortion below ("The face") was justified by *"every beginner would see kernel calls that
> are a category error next to `circfill`"* — with the beginner retired as an audience, the
> filesystem bridge stays only if it's the cleaner design *on taste*, not as a mandate; a plain
> `de_out_path()` / `fs_*` accessor is now allowed. (2) The **north-star-fit anxiety** ("an OS
> is meta-infrastructure, a sideways move… most defensible as showcase/play") softens: composition
> plumbing is now *core-adjacent* — the same category as the oracle layer (tooling that helps the
> maker+Claude build well). (3) The **focused-cart reframe strengthens this doc's own demotion of
> the live Workbench** — "one cart = one honest core, small enough to verify" is the opposite of a
> sprawling concurrent desktop, so Tier 3 stays demoted *on principle*, not just pragmatically.
> Nothing here is invalidated; the beginner-shaped caveats that were distorting the conclusions are.

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

## The honest size of it (settled 2026-06-25)

Take the costume off and the whole idea reduces to one sentence: **carts as tools for other
carts = save a file here, load it there.** And *the real OS already does that* — the
filesystem is the shared filesystem, and macOS already mixes audio from multiple processes
at the system level for free. **There is no operating system to build.** Every grand word
below — kernel, shell, Workbench, IPC — is either something the real OS hands you for
nothing, or chrome on top of "two carts agree on a file path."

What actually remains, after you delete the OS framing, is three modest things, none of them
a system:

1. **A convention for where the files land.** Tool outputs are scattered today (`build/.bake/…`,
   `--dump`, `--trace`, the `*_request` mailbox) with no naming discipline. That's
   [STATUS open item 44](../STATUS.md), and it's pure tidying — not part of any OS.
2. **One blessed `--out <file>` flag**, mirroring the `--data` that already ships. A cart
   writes its result to a path; another reads it. That's the entire "pipe." One runtime flag.
3. **Use cases actually worth the flag** — chiefly `bones → game` (below).

### Authoring carts vs consumer carts (the "tool for other carts" category)

The framing surfaces a cart taxonomy the repo already has but never named:

- **Authoring carts** produce reusable assets — `bones` (skeletal rigs), the sprite editor,
  the map editor.
- **Consumer carts** eat them — a game that plays a rig, draws the sprites, walks the map.

The diagnostic that decides whether the flag *helps* a given tool: **is it stranded or
unwanted?**

- **`bones` is stranded** — a genuinely useful rigger whose output has *nowhere to go*. You
  rig once; no channel carries the skeleton + keyframes to a game cart. That's not a flaw in
  bones, it's the missing `--out`. Fix the channel and a real workflow exists that doesn't
  today. **This is the use case that justifies the flag, and a better Tier-1 proof pair than
  `osm-roads → roadview`** — it's the full loop (a human authors in one cart, a game consumes
  it), not just data processing.
- **The sprite editor is unwanted** — not unused for lack of a pipe, but because
  *code-authoring won on the merits* (`sprite-draw.js` / `.cart.js`; CLAUDE.md steers everyone
  there). A pipe won't revive a tool that lost to a better workflow. The OS framing is a lever
  for the stranded, a distraction for the unwanted — don't let it talk you into plumbing the
  sprite editor.

### Authoring-time pipes ≠ runtime pipes

`osm-roads → roadview` is a **runtime** pipe: data fetched, consumed live, hot-swappable
while the window's open — it wants a live shared directory. `bones → game` is an
**authoring-time** pipe: you rig once and the game ships with the asset **baked in**. That
case probably doesn't want a shared dir at all — it wants to **bake into the consumer cart's
`.cart.png` chunks**, which *already exists* (sprites/map/settings are embedded zTXt chunks
today). So an authoring cart's natural `--out` target may be "write target-cart's `de:rig`
chunk," reusing the bake mechanism — not `build/fs/foo.json`. The shared-FS framing below
quietly assumes the runtime case for everything; the authoring case is a different, cheaper,
more on-brand channel.

### Music sync — the one primitive the real OS doesn't hand you

The single thread with any real meat. Multiple musical carts playing *in time* (MIDI-clock /
Ableton-Link style) is the one thing "save a file, load it" doesn't trivially cover — but it's
close. The realization: **musical sync needs a shared *clock*, not a shared audio device.**
MIDI clock transmits tempo + transport position + start/stop; it does *not* route audio. Each
cart makes its own sound on its own process; **the OS mixes the resulting streams for free.**
So the only new primitive is a tiny **shared transport** — a live file/socket holding
`{bpm, beat, playing}` that every musical cart reads each frame instead of running its own free
clock. The substrate is shockingly close: `beat`/`bpos` already exist as trace auto-fields, and
`play.js` already has `--bpm`.

Caveats that keep this leashed: it won't be **sample-accurate** — separate processes on separate
audio devices drift between sync points, exactly like a rack of real MIDI gear (which also
re-aligns on each pulse; for a jam that's the texture, not a bug). The real engineering is
drift/phase compensation (Link's actually-hard part), not the plumbing — a naive first cut
(everyone snaps to the shared beat, accept the jitter) is enough to learn whether it's good
enough. **Only sample-locked sync needs a single host (Tier 3);** loose musical sync is a
live file. With a corpus of musical carts already here (radio stations, keybeds, groovebox,
`improv.h`/`solo.h`/`radio.h`), "what if they all played in time" is a real payoff — note this
pulls the *most exciting* musical use case down from Tier 3 to roughly **Tier 1.5**, correcting
the tier map below, which files all live concurrent carts under the hard tier.

### Across devices — the Ableton Link case (where the plumbing stops being free)

Everything above assumes carts on **one machine**, and that's what makes it cheap: the OS mixes
the audio streams for free, and the shared clock is a local file every process reads off the same
wall clock, so "drift" is negligible. Now ask the demand-driven question — **can separate
*devices* (phones on a wifi) play in time?** ([field-note 024](../field-notes/024-demand-discovery-four-tribes.md)
mined this as a recurring wish: *"get several phones to all play the same click at once."*) Both
freebies vanish: there's no shared audio device (fine — each phone plays its own out) and **no
shared wall clock** (not fine — each device's clock drifts, and the transport is now a jittery
network, not a file read).

That regime has a name: **Ableton Link** — LAN, peer-to-peer, no master, agreeing on tempo *and*
beat-phase with per-device latency/drift compensation. So "carts in sync across phones" is
literally **re-deriving Link's network layer on top of our shipped WebRTC transport**
(multiplayer rung 5b — [`multiplayer-research.md`](multiplayer-research.md)). What it would mean,
concretely:

- **Transport:** broadcast the same `{bpm, t0, playing}` triple over the WebRTC DataChannel instead
  of writing it to a file. It changes only on play/stop/tempo, so it's tiny and **jitter-tolerant
  by design** — you replicate *state*, not a 60 Hz pulse, so a dropped/late packet costs nothing
  (the receiver keeps deriving off the last-known triple). This is the same insight that makes the
  single-machine version a file and not a stream, paying off harder over a lossy link.
- **No-master still holds** — the clock is *data* (the triple), now replicated over the wire; a
  transport peer writes it, everyone else derives `beat = (now − t0)·bpm/60` off their **own**
  clock. Same decoupling as the N=1 case; nothing emits clock.
- **The hard part becomes real** — this is where the doc's own "Link's actually-hard part"
  (drift/phase compensation) stops being hypothetical. On one machine it's free; across physical
  devices you get real clock-domain drift plus a one-way network delay that offsets `t0`. The naive
  first cut (derive locally, re-agree `t0` periodically) gives **loose jam sync** — the same
  "re-aligns each pulse, texture-not-bug" tolerance the single-machine caveat already accepts, just
  with bigger error bars. Tight sync would need round-trip delay estimation to correct `t0` (what
  Link does); worth measuring the naive version first.

**Why this is suddenly worth a line and not just a daydream:** all three pieces are now in hand —
the **demand** is proven (024), the **transport** is shipped (WebRTC P2P), and the **clock design**
already lives in this doc. And it lands dead-centre on the note-024 thesis: a humble *"several
phones, one groove"* toy is the **beginner-simple, zero-setup version of Ableton Link** — Link for
people who'd never open a DAW. Still exploration, not a plan; but it's the cheapest bridge from a
mined wish to a shippable cart the demand work has surfaced.

### So the Workbench and our own terminal…

…stay in this doc as the **someday-maybe cosmetic skin we'd genuinely like** — not as the
substrate. They're chrome over "agree on a file path," not an operating system, and composition
*works* without them. Demoted from "the dream" to "the fun version." (We see you, TempleOS.)
Read Tiers 2–3 below knowing **Tier 1 is the whole functional need**; the rest is for the love
of the thing, which is a fine reason, just an honest one.

### The face — amber, MDA font, CRT fuzz (and NO new public API)

The orchestration (launch + pipe) is invisible JS plumbing. The part actually worth fussing over
is the *look*: a warm **amber-phosphor** monochrome window, a **CGA/DOS bitmap font**, a little
CRT fuzz — Norton-Commander-in-a-tube. And that look is **native to the engine and foreign to
HTML**, which is what argues for an *engine-rendered* viewer (a cart) over a plain Electron panel:

- **The fonts are already in the runtime, and they're period-correct.** `dos_8x8.png` is a DOS
  8×8 ROM font (the in-game default); `FONT_LARGE` is **MDA 9×14** — MDA being the *Monochrome
  Display Adapter*, the literal text of amber/green IBM monitors; `FONT_BOOT` is VGA 9×16.
  `gen-rom-font.js` already bakes ROM-dump fonts. You'd be using the era's actual glyphs, not
  evoking them.
- **Amber is one palette / a tint** (`pal()` or a shader uniform over a near-1-bit image).
- **The CRT pass has a natural home** — the engine renders to a `RenderTexture` then scales to
  the window; that scale step is exactly where scanlines / bloom / phosphor-smear / barrel-curve
  go. A CRT+amber post pass is the one **genuinely-new cosmetic bit** (no such shader in the repo
  yet); everything else is already here.

Faking this in an HTML panel (CSS filters + a web font) would be Electron cosplaying as a console,
reimplementing badly what the engine nails. The feeling is the whole point, and the feeling is a
render — so the viewer wants to be a cart.

**The API worry, answered.** Making the viewer a cart sounds like it forces *privileged kernel
verbs* into the public API — `cart_list()` / `cart_launch()` / `fs_write()` — which would be a
real `studio.h` pollution (the four-places rule pushes them into autocomplete/help/docs, and every
beginner would see kernel calls that are a category error next to `circfill`). **It doesn't — the
clean design routes the bridge through the filesystem, adding ZERO new public API:**

- **Catalog in** via the existing `--data` pipe — hand the amber pane `index.json` exactly as
  `roadview` is handed roads.
- **Launch intent out** as a tiny dropped file (`save_bytes()` — already exists) that JS watches
  and acts on — the same `build/.bake/*_request` mailbox pattern already used for live inspection,
  just reversed. The cart expresses *intent*; the kernel (`main.cjs`) keeps the privileged power.

So the viewer cart needs only what every cart already has: render, read `--data`, write a save
blob. The privilege stays in JS, `studio.h` stays clean, and it's *more* faithful to the
"save-a-file, load-a-file" thesis than a C bridge would be. That dissolves the "needless API"
cost — the bridge is a **convention**, not a function.

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

- `data-tools/roadview/osm-roads.js --place "Delft"` writes `data/delft.json`;
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

### Why you can't open two carts today (and why it's mundane)

It's tempting to read the one-cart-at-a-time reality as the deep Raylib wall above. It mostly
isn't — today's blocker is three **single-document assumptions** in the editor, not the runtime:

1. **The editor is a single-document IDE.** One source buffer, one Run button, one build
   target. Loading a cart *replaces* the source — there's no gesture for "two carts open," so
   the second load overwrites the first.
2. **The live (libtcc) backend is one cart by design.** Every Run calls `killLiveHost()`
   (`main.cjs:355`), and a native run kills the host too (`main.cjs:703`). One persistent host,
   one hot-swapped guest, always. On the live backend that's the direct cause.
3. **Everything shares one `build/` dir.** Each run compiles to the same `build/cart` and
   overwrites `build/sprites.png`/font/`cart.c`, so even two runs that *could* coexist fight
   over on-disk assets.

The tell that this is mundane: **the native run path never kills the previous native process**
— `proc` at `main.cjs:729` is a local `const`, never tracked, never killed. The OS-level
machinery for two side-by-side carts already works (two `spawn()`ed binaries; the OS schedules
both and mixes their audio for free — see the music-sync note). What's missing is just
**per-run build dirs** (`build/run-<slug>/` instead of one shared `build/`) and **a launcher
that doesn't assume one loaded document** — which is exactly what a Norton-Commander-style pane
*is*. So the multi-cart story's first real blocker is a shared build folder and a single-doc
editor, not the runtime or any kernel/IPC work.

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

## Kernel + sibling shells (the Win 3.1 / DOS model)

The natural way to picture the layering is **Windows 3.1 on DOS** — a text substrate
underneath, a GUI shell on top. But the precise mapping matters, because the obvious
reading ("the Workbench runs on top of the terminal") is the wrong one and leads to a
backwards build.

| Win 3.1 / DOS | Here |
|---|---|
| **DOS** (the kernel: FS, loads & runs programs) | `main.cjs` + the shared FS + the pipe-compose primitive |
| **COMMAND.COM** (the prompt) | the terminal |
| **Windows 3.1** (the GUI) | the Workbench |

The load-bearing detail: **Win 3.1 did not run on top of COMMAND.COM — it ran on top of
DOS and bypassed the prompt entirely.** You could *launch* it from the prompt, but once
up it talked to the kernel directly. The terminal and Windows were **siblings**, both
sitting on DOS. Same here: the Workbench sits on the **kernel** (shared FS + pipe
substrate), exactly as the terminal does — not on the terminal.

Why this earns its keep:

1. **It prevents a backwards build.** "Workbench on top of terminal" tempts you into having
   the GUI shell out to the text shell and scrape its stdout — fragile, like a GUI
   screen-scraping a CLI. The right move: define one small **kernel API** — `list carts`,
   `run cart`, `read/write FS`, `compose pipe` — and make *both* the terminal and the
   Workbench thin clients of it.
2. **So "terminal-first" really means "kernel-first."** The terminal isn't the
   foundation; it's the *cheapest client that forces the foundation to exist*. Build the
   Tier-1 substrate, and a text prompt is the smallest UI that proves it. The Workbench
   becomes a second skin over the same API later, paying nothing the terminal didn't
   already pay. That — not any dependency of the GUI on the terminal — is why terminal-first
   is the sensible order.
3. **The analogy even predicts where the pain lands.** Win 3.1's fragile part was
   **cooperative multitasking** — one misbehaving app could hang the whole desktop. That is
   exactly the **Tier 3** risk: a multi-cart libtcc host where one cart's segfault takes the
   host down ([`cart-as-script.md`](cart-as-script.md) Step 4). Tier 1 dodges it (separate
   processes, file handoff); the Workbench is where you'd have to solve it — the same wall
   Microsoft only really cleared with the NT kernel.

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

## What would prove the magic cheapest — the first-step ladder

**The key realization: only one half is unproven.** A cart *consuming* another's output
already works — `roadview` reads `--data` today. So the test must exercise the missing half,
**a cart *emitting* output** (no `--out` flag, no emitting cart exists yet). That makes the
first test a **cart, not a JS pane** — the launcher/viewer is chrome over a pipe that doesn't
exist yet, exactly like the amber CRT is chrome over a viewer that doesn't exist yet. Each
rung below is worthless until the one before it works, and the first two are an afternoon:

1. **`de_out_path()`** — a literal mirror of the existing `de_data_path()`/`--data` in
   `studio.c`. One accessor giving a cart a path to write to. The cheapest possible runtime
   change; *this* is the whole "one flag."
2. **Hello-world pair (proves bytes flow).** A producer cart writes `{"n":42}` to
   `de_out_path()` on frame 1; a consumer reads it via `--data` and `print`s 42. Run
   back-to-back by hand. If 42 appears, the entire "save-a-file, load-it-there" substrate is
   proven in two toy carts + one function.
3. **Tilemap pair (proves it generalizes).** A trivial generator → a trivial renderer over a
   **tilemap** payload (first-class engine data — `map()`, `MAP_W/H`, `charMap` — not arbitrary
   JSON): `gen-maze --out grid.json` → `tilewalk --data grid.json`. The point is they're **two
   unrelated carts** — clearing the real bar (a second pair that pipes cleanly, *not* a
   roadview special case). A tilemap is natural at both ends; making some cart emit *roads*
   would be contrived.
4. **The JS pane** — only now, as one-button sugar that runs the two commands and shows
   `grid.json` in the right panel. Not before: a launcher for an unproven pipe is wasted work.
5. **The amber cart** — last, and for love (see "The face").

If the tilemap pair pipes cleanly, the launcher/piper/viewer stops being a "can we" question
and becomes pure UI. If it's awkward, we learned that for the price of one accessor and two
throwaway carts. **Note `bones → game` and the rotation-atlas baker are *later instances* of
this same proven pipe, not prerequisites** — don't gate the first test on either; prove the
mechanism with throwaways first.

## First time-synced multicart — the coprime ensemble (and how you launch it)

The music half's equivalent first build. It rides entirely on substrate already named here:
the clock is the `--data` pipe with a tiny payload, the audio mixing is free from the real OS,
and every cart already has a `beat` notion (`play.js` already takes `--bpm`).

**The transport is barely a clock.** Don't stream `beat` into a file 60×/s. Carts don't need the
beat *handed* to them — they need to *agree on tempo + a phase reference* and derive it locally
(the Ableton-Link insight, in miniature). So the shared transport is three numbers that change
only on play/stop/tempo:

```
{ "bpm": 120, "t0": <start timestamp>, "playing": true }
```

Each cart reads it (once, plus on change) and computes `beat = (now - t0) * bpm/60` from its own
clock. They stay locked because they share `bpm` + `t0`; drift is only per-process clock wander,
small over a jam and re-derivable every note. **No new public API** — clock in via `--data`,
nothing out (same filesystem bridge as everything else here).

**What they play — emergence, not a sequence.** Each cart runs one dead-simple rule keyed to the
shared beat; the *ensemble* emerges. The first demo: three windows on **coprime** periods —
A fires every **4** beats, B every **3**, C every **5** — over one clock. Nobody composed the
groove; it's an interlocking, slowly-phasing polyrhythm (Reich / Euclidean territory) that
re-emerges into a new texture the moment you change one period. The demo is **its own oracle**:
coprime rhythms make sync *audible* — locked → the pattern repeats cleanly every LCM (60 beats);
drifting → you hear the smear. Same trick as the data-pipe ladder (the test is the demo). Voices
are configured **once** and only *triggered* on the beat — never reconfigure FX per frame
(`lint-fx-frame.js`). Want melody later? Same shape: each cart picks a scale degree from a rule
over the shared beat; harmony emerges from the rules + a shared scale, nobody writes a chart.

### Who owns the clock (and the N=1 case)

**Nobody among the carts — because the clock is *data, not a process*.** MIDI's master-election
mess exists because the master is a *device that emits clock*. Here nothing emits: the clock is
three numbers in a file, and every cart *derives* its beat locally. So the only question is *who
may write the numbers*, and the answer is **whoever owns the session UI** — `jam.js`, later the
pane, later a transport-control cart. Voice carts only ever *read*. "Boss" isn't a role a voice
holds; it's write-access to a file, held by the launcher. Controlling tempo and being a voice are
**decoupled**: a future transport cart with play/stop/tempo doesn't become master — it just writes
the same file (via the mailbox bridge), and the voices re-read three numbers without caring who
changed them. No election, ever.

**The transport is *optional input*, which kills the solo/ensemble special case.** A musical cart
needs no "solo mode" vs "ensemble mode" — one rule covers both:

- **No `--data` clock handed in** → run your own internal beat (`--bpm`/default). A normal cart, as today.
- **Clock handed in** → derive beat from `{bpm, t0}`.

So a solo cart is just an ensemble of one; launch three and they share `t0` and interlock — same
code, no branch. Presence of the file *is* the "join the band" signal. Join-in-progress works for
free: because the launcher owns `t0`, a late joiner reads the *same* origin and phase-locks
immediately — beat is derived from a shared origin, not from "go now."

**The one real piece of transport math** lives in the single writer, not the voices: changing bpm
live would make `beat = (now − t0)·bpm/60` jump, so on a tempo change rebase the origin to preserve
the current beat — `t0' = now − beat·60/bpm_new`. Stop/start is just the `playing` flag. The voice
carts stay dumb (read three numbers, derive); all the subtlety sits in whoever writes the file.

### How you actually start them — a CLI spawner, not the editor

The launch question collides head-on with "you can't open two carts today" (single-doc editor +
shared `build/`). **Don't make the editor multi-launch — that *is* building the launcher pane,
the premature-chrome rung.** And the real blocker isn't UI at all: it's **per-run build
isolation** — three carts can't share one `build/cart` + one `build/sprites.png`; each needs its
own `build/run-<slug>/` (own binary, sprites, cwd). Solve that and spawning N is trivial.

So the first "UI" is a **command**, not a GUI:

```
node tools/jam.js kick3 snare4 hat5 --bpm 120
```

A ~30-line node spawner that (1) writes the transport `{bpm, t0:<launch time>, playing:true}`,
(2) compiles each named cart into its own `build/run-<slug>/` (reusing the compile path
`make-cart.js`/`play.js` already share), (3) spawns each with `--data <transport>` + its own
`--save-dir`, (4) kills them all on Ctrl-C. Optional nicety: pass window positions so they
**tile** rather than stack — the difference between "three windows" and "a console."

**Key unification: the launcher *is* the transport master.** `t0` = the moment `jam.js` launches;
every cart derives `beat` from that *same* `t0`, so they're phase-locked from birth even if they
start a few ms apart — no separate "clock cart" needed. The general multi-cart launcher and the
music-sync starter are the **same tool**; music just adds the clock file to the spawn. You're not
building a launcher *and* a music thing — one spawner that optionally writes a clock.

The launch-UI ladder mirrors the data-pipe one (build bottom-up; each rung wraps the one below):

1. **`jam.js` CLI** — spawns the synced ensemble. No GUI. (Forces per-run build dirs = the real unlock.)
2. **JS launcher pane** — multi-select carts, "Launch synced," shows bpm/playing + stop/tempo. Calls `jam.js`.
3. **Amber cart launcher** — renders the list, drops `launch:` + transport-control intent files the
   kernel watches (the no-new-API bridge from "The face"); CRT tiles of the running windows.

Drift is the only real engineering, and it's deferred: v1 accepts it (re-derive from `t0` each
note, like MIDI gear re-aligning on each pulse — the looseness is the texture). Reach for a socket,
a kernel-ticked clock, or single-host sample-lock (Tier 3) *only* if a real jam proves the
file-derived clock too loose.

## Open questions to sit with

- Is the shared FS a flat blob store, or does it carry the typed "vector features" /
  tilemap / audio-buffer schemas so pipes are *typed* (a cart advertises what it eats and
  emits)? The `index.json` metadata could grow `consumes`/`produces` tags. (The
  authoring-cart taxonomy above — `bones`'s `de:rig`, etc. — is one concrete instance of
  this typing question.)
- ~~Terminal-first or Workbench-first as the visible surface?~~ **Settled (this session):
  kernel-first — and then deflated further: the kernel is the real OS.** Both shells are
  chrome over "agree on a file path"; build neither to make composition *work* (a flag does
  that). See "The honest size of it" and "Kernel + sibling shells" above.
- ~~Does "shared processes" ever mean genuinely concurrent live carts (Tier 3)?~~ **Mostly
  answered:** sequential file-handoff (Tier 1) is the whole need for data; *loose* musical
  sync is a shared-clock file (Tier 1.5, no concurrency). The *only* thing that genuinely
  needs one live multi-cart host is **sample-accurate** musical sync — reach for Tier 3 only
  if that specific tightness is ever wanted.
- **The visible surface, when we eventually want one for the love of it:** a file-manager
  shell to *launch and chain* carts — a **Norton Commander / Midnight Commander clone**
  (twin-pane: carts on the left, the shared FS / their outputs on the right; Enter runs,
  a keystroke pipes A→B) is the most on-brand and pleasant fit, and it's a single cart-styled
  panel, not a windowing system. Strictly Tier-2 chrome over the Tier-1 substrate — wanted,
  not needed. Is it a cart (on-brand, fights the runtime) or an IDE panel (cheap, off to the
  side of the console feel)? Same fork as the Workbench.
