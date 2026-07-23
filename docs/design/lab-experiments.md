# The LAB pattern — try-before-commit feature experiments

STATUS: shipped (first use: `chordblossom2`, 2026-07-23)

A way to add a feature as a **live, switchable experiment** so the maker can play with the
variants and *decide by feel*, instead of the maker (or Claude) committing to one design in code up
front. Born from the `chordblossom2` Orchid work: two Orchid-faithful behaviours (diatonic Key mode
vs chromatic; the Simple/Advanced/Free Play Style) were both worth trying, and neither was obviously
right without hearing them.

The pattern has two halves: **SEAMs** (the droppable code structure) and the **LAB** (the dashboard
that flips them at runtime).

## 1. SEAM — a labelled, droppable fork

An experimental behaviour is gated by **one state var**, funnelled through **one dispatch point**,
and every use site is tagged with a grep-able marker:

```c
// SEAM:trigger — the Orchid Play Style. Drop-to-one: keep the winning branch, delete the rest.
enum { PS_SIMPLE, PS_ADVANCED, PS_FREE, NPLAYSTYLE };
static int playStyle = PS_SIMPLE;
```

Rules that make a seam actually droppable later:

- **One var, one dispatch.** All the variant-specific logic lives behind a single function or
  `switch`, never smeared across the file. (`chordblossom2` routes every param change through
  `cb_param_kind(kind)`, which is the only place the Play Style is read.)
- **Tag every site** `// SEAM:<name>` so `grep 'SEAM:<name>'` finds the whole blast radius.
- **The default variant reproduces the old behaviour exactly** — so turning the seam *on* is a
  no-op until you flip it, and there's zero regression risk while it's still a question.

When the maker picks a winner: keep that branch, inline it, delete the enum + the other branches +
the LAB row. The seam disappears and the choice becomes just how the cart works.

## 2. LAB — the live dashboard

An optional overlay (toggle: the `` ` `` key or an on-screen LAB chip) that gathers **every** live
seam for a cart into one panel, each on a row of option chips showing the current pick. Flip a chip
and hear the difference immediately, without a recompile.

```c
typedef struct { const char *name; int *val; int n; const char *const *opts; } Experiment;
static const Experiment LAB[NLAB] = {
    { "harmony",   &keyMode,   2,          OPT_HARMONY },  // SEAM:harmony
    { "playstyle", &playStyle, NPLAYSTYLE, PSNAME },       // SEAM:trigger
};
```

`lab_update()` (hit-test) and `lab_draw()` (render) **share one layout** so chips never drift from
their tap targets. Adding an experiment is **one row** in `LAB[]`.

Non-negotiables:

- **It pokes only the seam vars, never the audio/sim path.** The LAB is a control surface for the
  experiments, not a feature itself.
- **Keep playing while it's open.** Gate only the *on-screen control taps the panel covers* (via a
  `bool ctl = !labOpen;` guard); leave the keybed keys / strum / play inputs live, so you A/B a
  seam *while holding a chord*. On other tabs whose controls the panel would overlap, freeze that
  tab's update while the LAB is open (tap-clash avoidance).
- **It's scaffolding, not product.** The LAB ships only while the feature is undecided. Once the
  seams are resolved, the LAB block comes out with the losing branches.

## When to reach for it

- Two-plus plausible designs for one behaviour and the choice is a matter of *feel* (sound, timing,
  interaction) that you can't settle by reasoning — you have to try it.
- NOT for a behaviour with an obvious right answer (just build it), or one an oracle can decide
  (`canvas-diff`, `spec`, the audio gates) — use the gate.

## Verifying a LAB headless

`play.js` `click <frame> <x> <y>` / `release …` drive the chips (coords are **canvas px** for a
fixed-canvas cart; `--dump` a frame to eyeball the overlay; `watch()` a seam var to confirm a flip).
See [debug-harness.md](../guides/debug-harness.md).
