# `spec()` — a per-cart logic safety net

Status: **design / not built.** This captures the shape we agreed on so building it
later is a transcription job, not a re-litigation.

## The gap

The test surface is lopsided. Audio is covered deeply (`tune-check`, `level-check`,
`fx-check`, `soak-check`, `web-audio-check`). Structure is covered (`build-all`
compile-check, `ui-audit`, `cart-status`). **Gameplay logic is covered by nothing.**
Nothing checks "press jump → player leaves the ground", "collision blocks movement",
"the economy never goes negative", "spawning a car increments the count". For a
draw-only toy that's fine; for the sims that are getting bigger it's the exact class
of bug that rots silently.

`spec()` is the gameplay twin of `tune-check`: a per-cart, deterministic, **headless**
assertion pass.

## The real win — it's not "tests", it's *captured verification*

Today, verifying a sim is ad-hoc and **thrown away**: drive it under `play.js`, fire
the `.bake/*_request` trigger files, read the state JSON, eyeball it — then discard all
of it. Next regression, redo from scratch. `spec()` turns that throwaway driving into a
committed, re-runnable artifact that lives next to the cart. A spec is also executable
documentation of a cart's invariants — the spec *is* the answer to "what is this thing
supposed to guarantee?"

## Decisions (locked)

- **Imperative.** `spec()` drives the loop itself (`step`/`frame`/press), it is not a
  predicate the harness sweeps over a recording. Reads like a test; full control of setup.
- **Logic only, for now.** No pixel asserts, no audio asserts. Those are already served
  by `ui-audit` (layout) and the audio gates (sound). v1 stays headless logic.
- **Opt-in, manual.** A weak stub → carts without a `spec()` cost nothing and are skipped
  (not failed). The runner is a manual/occasional gate like `build-all`, **not** a commit
  hook (parallel agents + compile cost).
- **No `de_state` requirement.** 0/394 carts use it (`cart-analyze.js`); state is plain
  file-scope `static`s. Specs read those directly.

## Two verbs, because carts split in two

`cart-analyze.js` shows two populations that need different specs:

### `step(n)` — for **stateful** carts (~174)

Run `n` frames of `init()`+`update()` with **no `draw()`**. Assert on the persistent
globals that `update()` mutates. This is the common case.

```c
#ifdef DE_SPEC
void spec(void) {
    step(1);                                   // init() + 1 update, headless
    expect(n_cars == 0, "starts empty");

    btn_down(0, BTN_A);  step(1);  btn_up(0, BTN_A);
    expect(n_cars == 1, "A spawns a car");

    step(120);
    expect(budget >= 0, "budget never goes negative over 2s");
}
#endif
```

### `frame(n)` + probe — for **procedural** carts (~38)

The procedural generators (`lotfill`, `interiors`, `coaster`, road sims…) follow
"fixed function, the driver changes": `update()` only nudges camera/seed/UI, and the
real logic — BSP room-splitting, footprint fills, density fields — runs **inside
`draw()`** and is thrown away each frame. `step()` (update-only) can assert almost
nothing on them.

So `frame(n)` runs a full **update + headless draw**, after which the spec asserts via
the cart's own point-query. Many of these carts already expose one (`interiors`'
`plan_probe(x,y)`, `lotfill`'s atom `probe` pointers):

```c
#ifdef DE_SPEC
void spec(void) {
    seed = 1; lf_seed = 1;
    frame(1);                                  // generate + draw the world at seed 1
    expect(plan_probe(BX, BY) == TILE_WALL, "building envelope is sealed");
    expect(plan_probe(DOORX, DOORY) == TILE_DOOR, "entry has a door");
}
#endif
```

### Golden check — orthogonal, **zero per-cart authoring**

For a pure-functional generator, the strongest cheap net needs no `spec()` at all:
render one headless frame at a fixed seed+camera, hash the framebuffer, assert it equals
a committed golden. *Any* drift in the generator changes the hash. Brittle by design (a
deliberate visual tweak invalidates it), so it's an opt-in re-bless like the audio
baselines (`--save`). Exposed as `tools/spec.js --golden`. Best fit for exactly the
"before these sims get bigger" worry.

## API surface (cart-facing, live only under `-DDE_SPEC`)

Kept small. Prefixed-or-not is the one open naming question (see below) — collisions are
rare since the DSL only exists in the spec build, but the repo has a namespace-trap history.

| call | meaning |
|------|---------|
| `void spec(void)` | weak stub; cart overrides it to define its specs |
| `step(int n)` | advance `n` frames: `init()` (lazily, once) + `update()`, **no draw** |
| `frame(int n)` | advance `n` frames: update **+** headless draw (for probe asserts) |
| `btn_down(int player, int btn)` / `btn_up(...)` | set injected button via the *default* binding |
| `key_down(int k)` / `key_up(int k)` | set injected raw key (full-keyboard carts) |
| `expect(int cond, const char *msg)` | record pass/fail |
| `expect_eq(long got, long want, const char *msg)` | like `expect`, better failure message |

## Engine wiring (`runtime/studio.c`) — small, the plumbing exists

1. `__attribute__((weak)) void spec(void) {}` beside the `update` stub.
2. A `--spec` run path in `main()`: reuse the existing headless init (the `--screenshot`
   path), set `inject_input = true`, then call `spec()` and let it drive via `step`/`frame`.
   No live loop, no window present.
3. `step()`/`frame()` reuse the per-frame body of `loop_step` (minus the present for
   `step`; `frame` keeps the draw so `pget`/probes see a real framebuffer).
4. `btn_down`/`key_down` write the existing `key_inject[]` buffer (already the mechanism
   behind `--replay`/`--script`). `btn_down` maps button→default key via the bind table so
   specs read in button terms and survive a rebind.
5. `expect` prints JSONL (one line per assertion: `{cart,msg,pass}`) for the runner to parse.

Note: the spec DSL is **dev-only test scaffolding**, not part of the cart-authoring API a
player learns — so it deliberately does **not** get the four-place treatment
(`studioDocs.js`/`shell.js` autocomplete + help tab). It's documented here and in the
runner's `--help`.

## The runner — `tools/spec.js`

Sibling of `build-all.js`. For each cart whose source defines a non-empty `spec()`:
compile with `-DDE_SPEC -DDE_TRACE`, run, collect JSONL pass/fail, per-cart report;
`--quiet` exits 1 on any failure (CI). Auto-skips carts with no `spec()` (weak stub).
`--golden` runs/blesses the framebuffer-hash check across all carts; `--save` re-blesses.

## Known boundaries (v1)

- **`update()` that reads pixels** (`pget`/`touching_color`) sees no framebuffer under
  `step()` (update-only). Such a cart uses `frame()` instead, or isn't spec'd. Rare.
- **Audio** isn't asserted — the audio gates own that. Spec mode can skip the audio device.
- The heuristic verdict from `cart-analyze.js` is fuzzy for instrument carts (high
  `update` count = audio-param churn, not game logic). Judge candidates, don't trust the
  label blindly.

## Which carts warrant a spec — `tools/cart-analyze.js`

Run `node tools/cart-analyze.js` for a ranked report (complexity × mutable global state).
Snapshot at time of writing (394 carts):

- **174 stateful** → `step()`+`expect` candidates. Top of the list: `sloop`, `roadnet`/
  `roadnet2`, `dune`, `advancewars`, `soccermgr`, `pizzatycoon`, `tradewinds`, `merchant`,
  `dungeonkeeper`. These are the sims where a logic regression actually hurts.
- **38 procedural** → golden / `frame()`+probe candidates: `coaster`, `lotfill`,
  `interiors`, `flyover`, `outrun`, `air`.
- **78 simple / 34 reactive / 70 mixed** → mostly no spec (draw-only toys, instrument/UI carts).

Don't blanket all 174 — a spec is real maintenance (rename a global, the spec breaks,
which is *good* but is upkeep). Write them on the complex sims worth pinning.

## Build order when we do it

1. Engine: weak stub + `--spec` path + `step`/`frame`/`btn_*`/`key_*`/`expect` + JSONL.
2. `tools/spec.js` runner (skip-if-absent, `--quiet`).
3. A real `spec()` on **one** representative stateful cart (a `roadnet`-class sim) as the
   proven reference — feel it run before rolling out further.
4. (Later) `--golden` for the procedural generators.
