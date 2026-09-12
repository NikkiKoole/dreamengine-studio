# Taking patch matching into a cart: the fork

> **STATUS: BUILDING (2026-09-12)** — option A (editor drop → spawn `pm` → audition → paste) is in
> the editor, and using it turned up the half of A that was never designed: **where the patch lands**
> (§7, the bench). Option B (the ear-judged Synplant cart) is still the next build and the bench is
> most of its shell. Skip C; D waits on the AUv3 refactor. Upstream:
> [`patch-matching.md`](patch-matching.md). No engine change (ADR-0006 — this is a tool spawn, not
> `studio.h`).

The CLI works: hand it a WAV, get eight dreamengine patches back. The obvious next wish is "drop a
sample into a cart and hear the console's own version of it". This doc is what that actually costs,
because the honest answer is that the interesting part is not UI work.

## 1. The reframe that does most of the work

There are **two different products** in this idea, and they have wildly different costs.

**Genopatch** is what the CLI does: automatic matching, thousands of renders, a machine deciding
what counts as close. **Synplant 1** is the older and better-known thing: eight seeds, you listen,
you pick one, it breeds mutations around your pick. That second one **never compares anything**,
because your ear is the loss function.

That distinction is the whole fork. The second needs no engine change, no offline rendering and no
scoring, and it structurally cannot suffer the one limitation the CLI cannot fix (§6 of
[`patch-matching.md`](patch-matching.md): a spectral distance is not perception).

## 2. Getting a sample IN — three seams that already ship

Not a blocker, and worth knowing before anyone designs around it.

- **`de_data_path()`** hands a cart a file named at launch (`--data <file>` / `$DE_DATA`), which is
  how `sloop` and `roadview` load real cities. `tools/patch-match/pmwav.h` is already a dependency-free
  WAV reader that compiles in cart-land, so a cart can read a WAV today.
- **The editor already has drag-and-drop** (`editor/src/shell.js`), so dropping a file is solved
  chrome, not new work.
- **The mic.** `mic_record` + `mic_record_read` + `sample_load` means you can *play the sound at the
  console and let it listen*. This is how an SK-1 worked, it needs nothing new, and it is by far the
  most fantasy-console-native answer to "how does a sample get in".

## 3. Why the offline render is a real wall

The tempting shape is "let a cart render a candidate silently and score it". That is not merely
missing API, it is wrong twice over, and both reasons are structural rather than fixable by effort:

1. **It would race the audio thread.** `sound_callback` runs on the audio thread; a cart's
   `update()` runs on the frame thread. Rendering from the cart means two threads walking the same
   voice and effect state.
2. **It would destroy the sound you are playing.** An offline render consumes the same voices,
   filters, delay lines and reverb tanks as the live output. Scoring a candidate would eat the audio
   the player is currently hearing.

So a correct offline render needs a **second engine instance** to render into. Which is precisely
the multi-instance work already in flight for AUv3
([`tools/instance-check`](../../tools/instance-check/), [`tools/engine-dylib-spike`](../../tools/engine-dylib-spike/),
`tools/ctx-gen.js`). Measured 2026-09-07: `node tools/engine-statics.js` reports **146 mutable
file-scope statics** still process-global. Partway, not nearly there.

The good news in that: option D below is **not a new project**. It is a downstream benefit of work
already being done for a different reason.

## 4. The four options

### A. The editor does it

Drop a WAV on the editor, it spawns `pm` in the background (the way `electron/main.cjs` already
spawns the compiler), shows the eight candidates, click to audition, and a button pastes the chosen
patch into the open cart.

| | |
|---|---|
| **Pros** | Every piece exists: the CLI, the drop handler, the spawn path, the WAVs it already writes. No engine change. Full-quality search, backgrounded, so the minutes do not matter. |
| **Cons** | It is an IDE feature. No player ever experiences it, and it does not answer the question as asked ("a cart where..."). |
| **Effort** | Small. Highest value per hour of anything here. |
| **Needs** | A drop target, a spawn + progress UI, an audition player, a paste-into-buffer action. |
| **Shipped** | 2026-09-11 — drop a `.wav` on the editor window (shift-drop = `--quick`). Electron builds `build/pm` on first use if needed, streams stage progress, then lists the eight candidates `pm` already writes (six on `--quick`, which refines 3 engines × 2 seeds rather than × 3). Click to audition the WAV; **paste into cart** inserts the `instrument()` block at the cursor. Fail / timeout / cancel stay in the panel. Browser tab (no spawn) says so. Indexed in [`guides/editor-features.md`](../guides/editor-features.md). **⚠ The paste is the unfinished half — see §7.** |

### B. A cart where your EAR is the loss function

Eight sprouts on screen. Tap one to hear it. Keep the one you like and it breeds mutations around
your choice. No target, no scoring, no comparison: it plays patches and mutates them.

| | |
|---|---|
| **Pros** | **Zero engine change, buildable today with shipped API only.** No rendering problem exists because nothing is rendered offline: the cart just plays a patch. Fits the north star (one honest core, legible to a stranger in one sentence, and genuinely delightful). Immune to the loss-is-not-perception flaw by construction. |
| **Cons** | It **explores** rather than matches. On its own it does not do "drop a sample in and get it". |
| **Effort** | Medium, and almost all of it is UI and feel rather than mechanism. |
| **Needs** | The mutation operator (perturb a `PmPatch` in normalized space, which `pmpatch.h` already defines), the eight-way layout, an audition trigger, and a lineage/undo tree. |

**The combination is the good bit.** Seed B's tree with A's match: the CLI finds the ballpark, you
breed away from it by ear. That gives both halves of the wish without waiting for anything, and the
two options are additive rather than alternatives.

### C. A cart that scores in realtime via `record_grab`

Play a candidate, capture the console's own master output (`record_arm` / `record_grab` /
`sample_read`), compare in-cart with `patchmatch.h`'s loss.

| | |
|---|---|
| **Pros** | No engine change, only shipped API. Charming: you *hear* it hunting. |
| **Cons** | **Realtime**, so about one second per candidate against the ~20,000 a cold search uses. Worse, `record_arm` captures the **master bus**, so a candidate must be **audible** to be scored: there is no silent fast path. Realistically 100–200 candidates before anyone loses patience. |
| **Verdict** | Enough for local refinement around a seed, nowhere near a cold match. And B gets the same interaction with a better judge. **I would skip this.** |

### D. The engine seam: a scratch instance a cart can render into

| | |
|---|---|
| **Pros** | Unlocks the whole thing in-cart, live. Not a new project (§3): it rides the AUv3 multi-instance refactor. |
| **Cons** | Gated on those 146 statics. And it asks a real design question rather than just work: `tools/lint-engine-seam.js` deliberately restricts who may call `de_instance_create` (a file must declare `de:engine-owner`), because three components each quietly booting their own engine was a shipped bug. "Any cart, whenever it likes" is a meaningful loosening of that rule. |
| **Verdict** | Let it arrive on its own. Do not pull it forward for this. |

## 5. Recommendation

**A, then B, and let D arrive by itself.** A delivers the actual magic using only what is built. B is
the thing that is genuinely a *cart*, and it is the more interesting object: "drop a seed, hear eight
variations, keep the one you like" needs no engine work and no permission from the search. Seed B
from A and both halves exist without blocking on the refactor. Skip C.

## 6. Questions only the maker can answer

- **Is the deliverable a tool or a toy?** A is a tool that makes patches. B is a toy that happens to
  produce them. They are both good and they are not the same product. (**§7 is the tool answer**,
  written after using A: the tool needs a place to put what it made.)
- **Does B need a target at all?** Pure Synplant 1 has none: you breed toward a sound in your head.
  Adding "and here is the sample it should resemble" changes it from a toy into an exercise.
- **Should a cart ever be allowed to create an engine instance?** That is the D question, and it is
  about the seam's discipline rather than about this feature.
- **Does the loss agree with your ear?** Still open, still unmeasurable by any gate here, and it
  bears directly on the fork: if the answer is often "no", B's human-judged design is not a
  compromise, it is the correct design.

## 7. Where the patch LANDS: the half of A that was never designed

**Added 2026-09-12, from the maker using A on real samples the day it shipped.** This section exists
because everything above got the search right and the ENDING wrong, and it is worth being precise
about how, since the mistake is a generic one.

### What happened

Six real searches (`ambitone`, `analok`, `cassette-v2`, `cassette-square-wave`, `sklocken`,
`toytone`), then three symptoms in a row:

1. **It pasted into an unrelated cart.** "Paste into cart" inserts at the cursor in whatever buffer
   happens to be open, and the buffer that is open is the cart you were working on, not a cart that
   has anything to do with the sample you just dropped.
2. **The cart then would not compile.** The block `pm` emits is `instrument(...)` plus a `hit(...)`,
   which are statements. Dropped at the cursor they land at file scope, or mid-declaration, and the
   run button reports an error about a cart the maker never meant to touch.
3. **The earlier runs were unfindable.** Each search writes `build/patch-match/<stem>/`, which is a
   perfectly good archive that nothing in the editor lists, so a seven-and-a-half-minute result is
   one navigation away from being lost.

### Where it hid in this doc

Option A's **Needs** row reads: *a drop target, a spawn + progress UI, an audition player, **a
paste-into-buffer action***. Three of those four were designed. The fourth is four untroubled words
standing in for the only step where the tool touches the maker's own source, and it turned out to be
the only step that can do damage. A spec that says what a feature needs, in a list, will hide the one
item nobody thought about, because a list makes every entry look the same size.

### Why "paste at the cursor" is wrong three times over

Not a bug to patch, and this is the point: it is wrong about the CART (the result has nothing to do
with the open buffer), wrong about the PLACE (a statement at file scope cannot compile), and wrong
about the JUDGEMENT (a candidate is only meaningful next to the target, and the cursor's cart has
never heard the target). Fixing the insertion POINT fixes one of the three.

The real diagnosis: **`pm` produces a SET, and this repo has no object that holds a set.** Eight
candidates, a target to compare them against, and a decision to make. That is a thing, and it has
been going into whatever text file was open.

### The bench

A `patchbench` cart whose one honest core (ADR-0022) is *the place a matched patch lands and gets
judged*. It carries a region the editor owns:

```c
// de:patch-slots begin      ← the editor replaces everything between these
    instrument(5, INSTR_PIPE, 123, 723, 4, 3480);
    ...
// de:patch-slots end
```

"Paste into cart" becomes **"open in bench"**: write ALL the candidates into that region, open the
bench, run it. Then they are auditioned in the real engine rather than as WAV files, played from a
keybed, A/B'd against the target, and the macros move while you listen. The paste has exactly one
legal home, so it cannot break a cart you did not mean to open. Alongside it, a **run browser** over
`build/patch-match/*` listing each run and its best loss, so a finished search is reopenable instead
of re-runnable. `paste into cart` demotes to **copy block** (clipboard, never edits a file), which
keeps the escape hatch without the footgun.

### It answers §6's first question, and it is B's shell

§6 asks whether the deliverable is a tool or a toy. **The bench is the tool answer**, and this doc
never wrote it down because it treated the tool half as finished the moment the CLI had a UI.

It is also, structurally, most of option B. §4's own note says *"seed B's tree with A's match: the
CLI finds the ballpark, you breed away from it by ear"* — which describes the bench from B's side
without noticing it needs somewhere to stand. A bench that holds N candidates, plays them against a
target and lets you tweak is **one mutation operator away** from being the sprout tree. So the
sequencing question is real: the minimum bench (a slot list and play buttons) works in an afternoon
and risks being built twice; the fuller one is a day and is the shell [#16](https://github.com/NikkiKoole/dreamengine-studio/issues/16)
plugs into.

### What it still does NOT need

No engine change. No `studio.h` growth. **No cart-created engine instance**: the bench PLAYS
candidates, it never renders them offline to score them, so §3's wall and §4's option-D trap both
stay where they are. The search stays in the CLI where it belongs.

## See also

- [`patch-matching.md`](patch-matching.md) — the shipped CLI, the measurements, and §6's honest limits.
- [`mic-and-sampling.md`](mic-and-sampling.md) — the mic seam §2 leans on, and the sampler doctrine.
- [`engine-context.md`](engine-context.md) — the per-instance refactor option D waits on.
