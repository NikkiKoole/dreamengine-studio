# Cart-survey API priorities — cart evidence first, docs as constraints

STATUS: EXPLORING (recommendation memo) — changes nothing on its own; accepted items go to STATUS.md or an ADR.

> **Genre: design exploration (scratchpad).** This note is a short recommendation memo
> grounded first in the shipped cart corpus, then filtered through the project's existing
> decisions and status docs. It does **not** change status on its own; if a recommendation
> is accepted, update [`../STATUS.md`](../STATUS.md) and/or write a dated ADR in
> [`../decisions/`](../decisions/README.md).

---

## Why this note exists

Recent API discussion surfaced a useful tension:

- the **carts** show what authors actually keep hand-rolling in real code
- the **docs / decisions** show what the project is intentionally trying to be

Both matter, but they answer different questions.

This memo takes the position that **real cart code is the strongest signal for pain**,
then uses the existing docs only to decide **which fixes are acceptable to ship now**.

---

## Corpus snapshot

Quick survey of the current cart corpus:

- **126 carts**
- roughly **34k lines** of cart C total
- usage scan done against `tools/carts/*.c`

High-level signals from the corpus:

- The immediate-mode drawing core is healthy and heavily used.
- The note-driven sound API is healthy and heavily used.
- The most repeated pain is not "missing rendering power"; it is **repeated glue code**
  around camera state, world-vs-screen coordinates, UI input, and persistence.

Selected usage signals:

- `print` appears in **125 carts**
- `btnp` appears in **103 carts**
- `cls` appears in **121 carts**
- `note` appears in **73 carts**
- `hit` appears in **58 carts**
- `mouse_*` appears in **22 carts**
- map helpers (`map` / `mget` / `mset` / `touching_map` / `tile_at`) appear in **21 carts**
- persistence (`save` / `load` / `save_bytes` / `load_bytes`) appears in **38 carts**

These numbers are not the recommendation by themselves, but they help separate core
behaviour from niche behaviour.

---

## What the carts prove

### 1. World/screen coordinate glue is a real pain

This is the clearest "small helper, real payoff" gap.

Several mouse-heavy, camera-using carts manually convert screen-space mouse coordinates
into world-space:

- [cannonfodder.c](../../tools/carts/cannonfodder.c:420) uses `mouse_x() + cam_x`
- [cannonfodder.c](../../tools/carts/cannonfodder.c:484) does the same again for aiming
- [hotline.c](../../tools/carts/hotline.c:470) computes `wmx` / `wmy`
- [masseffect.c](../../tools/carts/masseffect.c:499) computes world mouse coordinates
- [excitebike.c](../../tools/carts/excitebike.c:641) offsets mouse by a build camera

This is exactly the kind of repetition an engine helper should absorb.

**Conclusion:** world-space mouse helpers are high-confidence additions (shipped as
`mouse_world_x()` / `mouse_world_y()` — see the recommendation section for the naming).

---

### 2. World draw vs HUD draw is a repeated manual pattern

The camera reset pattern is real and common:

- **18 files** explicitly call `camera(0, 0)`
- **6 files** use `follow(...)`
- many carts do "world under camera, then reset for HUD"

Examples:

- [platform.c](../../tools/carts/platform.c:202) draws world with `follow(...)`
- [platform.c](../../tools/carts/platform.c:223) resets with `camera(0, 0)` for HUD
- [classic-starter.c](../../tools/carts/classic-starter.c:53) sets camera for world
- [classic-starter.c](../../tools/carts/classic-starter.c:75) resets for HUD
- [opwolf.c](../../tools/carts/opwolf.c:617) sets camera for map draw
- [opwolf.c](../../tools/carts/opwolf.c:621) resets camera afterward

This is not a theoretical style concern. It is visible repetition across the corpus.

**Conclusion:** a small helper like `hud()` is high-confidence and consistent with the
project's current direction.

---

### 3. Named persistence would remove a real beginner footgun

The slot-based save API is used a lot, but often in its most fragile form:

- **30 files** use `load(0)`
- **30 files** use `save(0, ...)`

Examples:

- [rogue.c](../../tools/carts/rogue.c:129)
- [robotron.c](../../tools/carts/robotron.c:119)
- [pacman.c](../../tools/carts/pacman.c:112)
- [opwolf.c](../../tools/carts/opwolf.c:385)

The pattern works, but `0` only means "hiscore" by convention. That's fine for experts
and poor for learners.

**Conclusion:** named helpers like `save_int("hiscore", v)` / `load_int("hiscore", 0)`
would be a meaningful usability win with little conceptual weight.

---

### 4. UI/input normalization is repeatedly hand-rolled

The carts repeatedly build some version of:

- `clicked(x, y, w, h)`
- `button(...)`
- `confirm_pressed()`
- `cancel_pressed()`

Examples:

- [dpaint.c](../../tools/carts/dpaint.c:395) defines a reusable `button(...)`
- [advancewars.c](../../tools/carts/advancewars.c:437) defines `confirm_pressed()` and
  `cancel_pressed()`
- [advancewars.c](../../tools/carts/advancewars.c:439) defines `clicked(...)`
- [omnichord.c](../../tools/carts/omnichord.c:239) manually checks click zones in the top bar

Also notable:

- **16 carts** use both mouse input and `btn`/`btnp`
- **9 carts** contain explicit "mouse click OR button-A" style combinations

This is a genuine repeated layer, especially in menu-heavy or strategy carts.

**Conclusion:** lightweight UI/input helpers are good candidates, but are slightly more
opinionated than the three items above.

---

### 5. Tile collision helpers should be treated cautiously

The carts do show custom collision code, but it varies by genre and data source.

Examples:

- [platform.c](../../tools/carts/platform.c:33) hand-rolls `solid_tile` / `solid_at`
- [10-world.c](../../tools/carts/10-world.c:16) uses the coarse `touching_map(...)`
- [zelda.c](../../tools/carts/zelda.c:151) uses its own world logic rather than a generic map helper

The reusable signal here is weaker than it first looks. The carts prove "collision is
important", but not that one engine abstraction cleanly fits platformers, top-down games,
and grid tactics games.

**Conclusion:** don't infer a broad tile-semantics layer from the survey alone.

---

### 6. Low `sfx()` usage is not evidence against the sound API

The cart survey shows strong use of the note-driven sound path:

- `note` in **73 carts**
- `hit` in **58 carts**
- `instrument` in **28 carts**

But that should **not** be read as "authored SFX are unimportant." It more likely means
the current authoring path for `sfx` / `music` banks is underpowered relative to the
code-first path.

**Conclusion:** the carts support continued investment in code-first sound; they do not,
by themselves, justify de-emphasising `sfx()` / `music()`.

---

## What the docs constrain

The cart findings above are the primary signal. The existing docs then narrow what should
actually be recommended now.

### Existing project constraints

- [`../STATUS.md`](../STATUS.md) is the source of truth for what is open / shipped / cut.
- [`0005-defer-tile-collision-helper.md`](../decisions/0005-defer-tile-collision-helper.md)
  already says **do not** add a built-in tile push-out resolver yet.
- [`0003-code-first-sound.md`](../decisions/0003-code-first-sound.md) says the current
  sound direction is intentionally **code-first**, and that tracker / SFX tooling is
  **deferred, not rejected**.
- [`../STATUS.md`](../STATUS.md:57) already prioritises **cart-pattern helpers** such as
  `explode()`, `hud()`, and `game_over_screen()`.
- [`../STATUS.md`](../STATUS.md:60) also keeps **events** (`broadcast` / `received`) high
  on the open list.
- [`../../CLAUDE.md`](../../CLAUDE.md:89) documents the real implementation cost of every
  new API symbol: `studio.h`, `studio.c`, `studioDocs.js`, `shell.js`, bilingual docs,
  and usually a demo cart.

### What this means in practice

- Prefer **tiny helpers with obvious cart payoff**
- Avoid broad new conceptual systems unless the cart evidence is overwhelming
- Don't confuse "nobody uses it yet" with "the API is wrong" when tooling is still missing

---

## Recommended shortlist — survives both filters

These are the additions that are both:

1. strongly supported by real cart code, and
2. not contradicted by current project docs / decisions

### Tier 1 — strongest next additions

> **Implemented 2026-05-31** — shipped as `mouse_world_x/y` + `save_int/load_int`.
> `hud()` was prototyped and then **removed** (see the postscript below for why).

#### `mouse_world_x()` / `mouse_world_y()`  *(shipped — renamed from `pointer_world_*`)*

Renamed to match the existing `mouse_x()`/`mouse_y()` surface; there is no `pointer_*`
family yet, so `pointer_world_*` would have introduced a concept with no siblings.

Why:

- directly supported by repeated cart code
- low conceptual weight, low risk
- no conflict with existing decisions

**Caveat learned in implementation:** these return a value derived from the *live*
camera global. If you read them in `update()`, you must also set `camera()` in
`update()` — relying on the camera left over from the previous `draw()` (especially
after a `camera(0,0)` HUD reset) makes clicks land a whole scroll off. The inline
`mouse_x() + cam_x` form does not have this trap because it uses a local variable. The
helper's value went *up* sharply once the camera gained rotate/zoom (shipped as
`camera_ex` — see postscript): the inverse transform is no longer one addition, so the
one-line helper now clearly earns its keep.

#### named persistence helpers

Suggested shape:

```c
void save_int(const char *key, int value);
int  load_int(const char *key, int default_value);
```

Why:

- repeated `save(0)` / `load(0)` is real
- removes a beginner footgun
- does not force removal of the current slot API

---

### Tier 2 — good candidates after the above

#### lightweight UI/input helpers

Examples:

```c
bool clicked(int x, int y, int w, int h);
bool confirm_pressed(void);
bool cancel_pressed(void);
```

Why:

- definitely supported by cart repetition
- especially valuable for menu / editor / tactics carts

Caution:

- these are more opinionated than `hud()` or world-pointer helpers
- choose names and semantics carefully so they teach well

#### `explode()`

Why:

- already a top item in [`../STATUS.md`](../STATUS.md:57)
- repeated enough in carts to justify a shared helper

This one is slightly different from the others: it is a **cart-pattern helper**, not an
engine plumbing helper. Still, the docs and the corpus both point at it.

---

## Explicit non-recommendations for now

These came up during review but should **not** be recommended as near-term API work.

### Not now: built-in tile collision framework

Reason:

- cart evidence is mixed
- existing ADR already says defer

### Not now: "soft deprecate `sfx()` / `music()`"

Reason:

- the docs show authored-bank tooling is still incomplete
- low current usage is not enough evidence

### Not now: large `v2` namespace or broad API re-organisation

Reason:

- the pain is in repeated glue, not in the low-level drawing core
- each new symbol has a high per-function cost in this repo

---

## Bottom line

If we stay honest to the carts while respecting the project's current decisions, the
clearest next API additions are:

1. `mouse_world_x()` / `mouse_world_y()`  *(shipped)*
2. named persistence — `save_int()` / `load_int()`  *(shipped)*
3. ~~`hud()`~~  *(prototyped, then dropped — see postscript)*
4. then, maybe, lightweight UI/input helpers

That is a much smaller recommendation than a hypothetical "studio v2", but it is better
supported by both the **real cart corpus** and the **existing docs contract**.

---

## Postscript — what building Tier 1 actually revealed

Implementing the shortlist (rather than just proposing it) changed the grading:

- **`save_int` / `load_int` — clean win, no caveats.** A pure naming improvement over the
  magic slot numbers; no hidden state, nothing to remember.

- **`hud()` — dropped.** It was only sugar for `camera(0, 0)`, which was already clear and
  familiar to PICO-8 users. Worse, as a flat setter called at the *end* of `draw()` it
  left the camera global at 0, which is what made `mouse_world_*` read in `update()` go
  wrong. It added a footgun for ~zero expressive gain, so it was removed.

- **`mouse_world_*` — kept, but it taught the real lesson.** A helper that *returns a value
  derived from mutable global render state* is categorically riskier than the inline math
  it replaces, because it hides a temporal dependency (which camera is active, set when?).
  The verbose `mouse_x() + cam_x` was buying locality and explicitness — that wasn't pure
  waste.

**The general principle for future helpers:** absorbing repeated *glue* is not automatically
a win. Helpers that only *write* state or compute from their arguments (`save_int`, a future
`clicked(x,y,w,h)`) are safe. Helpers that *read* hidden global state to hand back a value
need a documented convention or they trade visible boilerplate for invisible bugs.

**Wider corollary — sticky global render state leaks, in both directions.** The same
root cause surfaced a second time while juicing the `hotline` cart with `camera_ex`. The
engine's drawing-scope state is a set of **sticky setters** — `camera`, `pal`, `fillp` —
whose values persist across frames *and across game states* until something changes them.
Two symmetric failure modes:
- **Read-side (stale):** you read the leftover value and get something from another scope —
  `mouse_world_*` in `update()` inverting the camera that `draw()`'s HUD reset left at the
  origin, so aim drifts as the world scrolls. Fix: set the camera in `update()` before
  reading it.
- **Write-side (leak):** you set it and never reset, so it bleeds into a scope you didn't
  mean — `panel()` calls `fade(0.5)` on the title screen, and with nothing clearing it the
  50% dim stayed overlaid on the *entire* game session.

> **Update — `fade` is no longer in this set.** The write-side example above was `fade`'s
> bug, and it recurred in **27 carts** — so `fade` was made **immediate-mode**: the runtime
> zeroes it every frame, a cart re-asserts `fade()` only on the frames it wants the dim, and
> it clears itself on state exit. No `fade(0)` reset needed. The "reset what you own" habit
> below now covers `camera`/`pal`/`fillp` only. See [decision 0010](../decisions/0010-fade-is-immediate-mode.md).

Defensive habit for cart authors: **reset the sticky globals you touch at the top of each
`draw()`** (`camera(0,0); pal_reset(); fillp_reset();`) and re-apply them per scope, rather
than trusting whatever the previous frame or game state left behind. This is the single most
common class of "works at first, breaks after some play" bug in the corpus.

**This also reframes `mouse_world_*`'s value — and that reframing has now shipped.** While
`camera()` was translate-only, the helper barely beat one addition. Adding **rotation and
zoom** (`camera_ex(x, y, zoom, angle)`, backed by a raylib `Camera2D` matrix) made
world↔screen a full inverse transform no cart author should hand-roll — so a one-line
`mouse_world_x()` (now `GetScreenToWorld2D` under the hood) clearly earns its keep. The
`worldpointer` cart demonstrates clicks landing correctly under scroll + zoom + rotation.
Documented limitations: `clip()` stays screen-space, and `pget`/`pset` are exact under
translate but approximate under zoom/rotation.
