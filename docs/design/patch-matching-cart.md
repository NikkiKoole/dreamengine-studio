# Taking patch matching into a cart: the fork

> **STATUS: SHIPPED (2026-09-13)** — option A is in the editor, §7's bench holds the SET, and
> **option B is the same cart**: pick a pad, BREED mutates around the keep, UNDO walks back.
> `pmpatch.h` now owns `pm_mutate()` (detents step; there was no operator in the header — #16
> corrected this). Skip C; D waits on the AUv3 refactor. Upstream: [`patch-matching.md`](patch-matching.md).
> No engine change (ADR-0006 — a cart plays patches, it never renders them offline).

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
| **Needs** | The mutation operator (perturb a `PmPatch` in normalized space — written into `pmpatch.h` as `pm_mutate`, because the header shipped the representation and the detents but no neighbour function), the eight-way layout, an audition trigger, and a lineage/undo tree. |
| **Shipped** | 2026-09-13 — on `patchbench`, not a second cart. Hear the pads (already there), the selection **is** the keep, **R / BREED** refills the other seven with children, **U / UNDO** pops the last few litters, SPRD sets how far a litter wanders. Generation 0 still plays the editor's `pb_apply` snippet so an A landing is the printed patch; after the first breed the live `PmPatch` is the source of truth. No scoring, no `record_grab`, no `de_instance_create`. |

**The combination is the good bit.** Seed B's tree with A's match: the CLI finds the ballpark, you
breed away from it by ear. That gives both halves of the wish without waiting for anything, and the
two options are additive rather than alternatives.

### ✅ BUILT 2026-09-13 — on the bench, not a second cart

`patchbench` grew the operator §7 said it was one away from. The selection **is** the keep (yellow
ring + a KEEP label); R / BREED writes seven children onto the other pads and leaves the parent
where your finger was; U / UNDO pops a 4-deep litter stack; SPRD is how far a child wanders.
Generation 0 still plays `pb_apply` so an A landing is the printed snippet. The editor also writes
`PB_SEED` (an exact `// pm:vec` when `pm` is new enough, else an inverse of the printed calls) so
the first breed has a real parent, not a default-plus-macros guess.

`pmpatch.h` did **not** ship a mutation space — #16's 2026-09-13 correction. What it had was the
representation and the detent table. `pm_mutate()` is the operator: continuous dims nudge in 0..1,
snapped harmonics step an adjacent measured centre. `tools/patch-match/mutate-check.c` gates it
without the engine. `runtime/pmbreed.h` is the cart-land include (apply-to-slot; leftovers cleared).

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

### ✅ BUILT 2026-09-12

The `patchbench` cart plus the editor half. Three things worth recording because they were only
found by building it:

- **The A/B was silently useless until the levels matched.** The target plays at whatever level it
  was recorded at and a candidate plays at `hit()` volume 5, and measured on the first real run
  those sat **10.6 dB apart**. In a back-to-back comparison the louder one simply wins, whatever it
  sounds like. The target is normalised up and the candidate trimmed down to meet it (the BAL knob,
  default measured, not guessed) — `instrument_level` only attenuates, 0..1, which is why the trim
  is on the candidate. Both sides now land within 0.25 dB. BAL stays a knob because candidates
  differ in level from *each other* too, and no fixed number matches them all.
- **`pm` puts three calls on one line.** `harmonics(5,..) timbre(5,..) morph(5,..)`. The generator's
  first draft rewrote only the first one, which still worked while the bench happened to use slot 5
  and would have played the wrong slot the day it did not. The re-slotting is global now, and the
  `instrument` prefix is load-bearing: `echo(251,…)` and `reverb(0.96,…)` are MASTER calls whose
  first argument is a time and a size, and must be left alone.
- **The generator REWRITES A SOURCE FILE**, so it refuses rather than guesses: no markers, doubled
  markers, or end-before-begin all throw instead of splicing, because the alternative is eating a
  hand edit. 20 of the module's 52 selfcheck assertions are on these two functions, mutation-tested
  (put the first-match-only bug back and exactly the two guards for it go red).

The editor also got the **run browser** (`build/patch-match/*` with each run's best loss, so a
seven-minute search is reopenable) and **"copy block"** in place of "paste into cart".

### What it still does NOT need

No engine change. No `studio.h` growth. **No cart-created engine instance**: the bench PLAYS
candidates, it never renders them offline to score them, so §3's wall and §4's option-D trap both
stay where they are. The search stays in the CLI where it belongs.

## 8. PARKED: a CHORD in, the same sound out, in that shape

> **STATUS: PARKED (2026-09-14)** — not now, no work planned. Written down so the next person asking
> this question starts from the decomposition instead of from "we need polyphonic pitch detection".

The wish: drop a WAV of a CHORD, have the tool work out (a) the notes, (b) the timbre, (c) hand back
that one sound played in that shape. The three parts cost wildly different amounts, and separating
them is most of the answer.

**(c) is free today.** Once a patch exists, a chord is three `hit()` calls on one slot. The engine is
polyphonic. Nothing to build.

**(b) is what `pm` already does**, well, given a clean single note.

**(a) is the only genuinely missing piece**, and the repo has nothing for it. `pmw_pitch` is
normalized autocorrelation over one window: one f0, and it returns 0 rather than guess.
`mic_pitch` is YIN. Both are monophonic BY CONSTRUCTION, not by accident. `hb_analyze` does not
help: it takes symbolic roots and qualities as ints, so it NAMES a chord, it does not hear one.

### The move that dodges (a) entirely

Do not transcribe: **race the chord the way stage 1 races the engine**. "Which chord" is a discrete
decision, the same kind as "which engine", and `pm` already resolves those by rendering every option
and ranking. Take the vocabulary from `harmony.h`, pin the octave from the target's lowest strong
partial, render, rank. Cost: ~200 candidates on a short window, against a measured ~26ms render per
second of audio, already forked across 4 workers. **Seconds, inside a search that takes 7.5 minutes.**
The expensive part is the part we already own.

Three things would bite, all with known shapes:

- **Chord and timbre are entangled** — a wrong chord is papered over by a wrong timbre. That is
  exactly why stage 3 freezes the voice before fitting fx; the same discipline applies (race chords
  against a fixed probe, then the engine race, then re-race the chord with the winner).
- **A strum is not a block chord.** One cheap continuous dim: milliseconds between notes.
- **An ambiguity that does not go away** — a rich single note imitates a triad's lower partials, and
  voicings an octave apart are near-identical to a log-mel loss. Same class as PD never ranking first
  because SAW reaches its tones (`patch-matching.md` §on the selftest). Expect a RANKING, not a fact.

### What it would take, if it is ever wanted

1. **A render that strikes more than one note.** `pmcart.c` fires `hit(pm_midi, …)` and the host seam
   is three ints; it needs a note LIST. Small.
2. **A chord beside `PmPatch`, not inside it.** A chord is not timbre. The printed snippet ends in one
   `hit()` and the bench plays one note with up/down; both would carry a shape.
3. **A `--selftest` for it**, which is the part that decides whether any of it works: render a known
   chord with a known patch, throw both away, find them again. Without it you cannot separate "the
   console cannot make that chord" from "the chord search is broken" — the exact distinction
   `--selftest` already exists to settle for engines.

### Build the ear version first

None of the above is needed to answer the question that matters. **Put a chord picker on the bench**
and let the ear do it, exactly like BREED: the patch is already there, so play a triad, a seventh, an
inversion, keep what matches. Zero multi-f0, zero new search, same thesis that just worked in option
B. An afternoon. It would also reveal whether picking the chord was ever the hard part, which decides
whether the automatic version is worth wanting at all.

## See also

- [`patch-matching.md`](patch-matching.md) — the shipped CLI, the measurements, and §6's honest limits.
- [`mic-and-sampling.md`](mic-and-sampling.md) — the mic seam §2 leans on, and the sampler doctrine.
- [`engine-context.md`](engine-context.md) — the per-instance refactor option D waits on.
