# 0019 — Oscillators are naive (non-band-limited); anti-aliasing deferred, not rejected
Date: 2026-06-22 · Status: accepted

## Context

The wavetable oscillators (`sound_osc` in `runtime/sound.h`) are **maximally naive** — a saw
is literally `phase*2-1`, a square is a hard `phase<duty?…`, etc. No band-limiting at all, so
they alias: every harmonic above Nyquist (22.05 kHz at our 44.1 kHz) folds back as inharmonic
content. Adding **hard sync** ([audio-notes §22](../design/audio-notes.md)) raised the question
sharply, because the swept reset discontinuity is the single worst aliasing source in the engine,
and it was audible enough to ask "should we fix this?"

The anti-aliasing options on the table:
- **PolyBLEP** — cheap polynomial smoothing of each discontinuity; the standard real-time VA fix.
  Knocks aliasing down a lot for little CPU; does **not** fully kill it (high notes, sync).
- **minBLEP / band-limited wavetables (mipmaps)** — cleaner, more memory/complexity.
- **Oversampling** (run at 2–8× then downsample) — brute force; the only really effective fix for
  **hard sync and waveshaping**, which BLEP can't fully clean. Costs the multiplier in CPU.

## Decision

**Keep the oscillators naive. Ship no anti-aliasing for now** — but treat it as *deferred*, not
rejected. If we ever add it, it should be **opt-in** (e.g. a per-instrument "hifi/band-limited"
flag), never the default, so the naive character and the existing baselines are preserved.

## Why

1. **The grit is on-brand.** dreamengine is a fantasy console; the lineage is PICO-8 / NES / C64 /
   Amiga, where aliasing *is* part of the loved sound. Clean band-limited oscillators would make a
   lot of carts sound "wrong," not better. Naive is an aesthetic choice, not just an unbuilt feature.
2. **The cost is localized.** Aliasing is only clearly audible in a narrow corner — **high + bright
   + exposed**, and **swept hard sync**. Down low, on dark waves, or in a busy mix it's inaudible,
   and bass/pads (most of the corpus) live there. We'd be paying everywhere to fix a few notes.
3. **Naive is cheap and uniform.** Zero per-voice cost; every voice behaves the same. Band-limiting
   costs cycles on a console that sums many voices.
4. **Blast radius.** Band-limiting the *base* saw/square would change the timbre of **every cart**
   that uses them and break the `level-check` / `web-audio-check` baselines — a large, repo-wide
   change to chase a corner case. Not worth it on spec.
5. **For sync specifically, PolyBLEP isn't even the right tool** — oversampling the synced voice is.
   So the "obvious cheap fix" wouldn't fully solve the case that prompted the question anyway; the
   real fix is a bigger (if contained) job. Better to wait until a concrete need justifies it.

We chose naive sync deliberately and judged it by ear ([the build was A/B'd against the PolyBLEP
option](../design/audio-notes.md) §22) — it sounded good, so it shipped.

## When it's audible (so a future reader can reproduce the judgement)

Rule of thumb: **high + bright + exposed + (sync) = loud; low + dark + busy = inaudible.** A quick
test in the `moog` cart:
1. **SINE, no filter** — the control; zero aliasing at any pitch.
2. **SAW, filter open, walk a note to the top of the keyboard** — past ~2 octaves up it turns
   buzzy/glassy and slightly "sour" (folded harmonics at inharmonic pitches).
3. **SYNC preset, swept, high note** — the gritty/granular "zipper" on the sweep: the worst case.

## Revisit triggers

Pick this back up if any of these actually happen (not on spec):
- A cart genuinely needs **clean, exposed high leads** and the grit is wrong for it.
- We want a **"hifi" instrument mode** as a feature (opt-in flag → PolyBLEP saw/square + oversampled sync).
- The swept-sync aliasing bothers us enough in practice → **oversample just the synced voice** (the
  targeted, contained fix — not a repo-wide oscillator change).

## Consequences

- `audio-notes.md` §22 (hard sync) and §23 (Steiner) already note "naive on purpose / judged by ear";
  this ADR is the home for the *why* so it stops getting relitigated.
- Any future anti-aliasing lands **opt-in**, preserving the naive default and the level/web baselines.
- No action now. The engine stays internally consistent: every oscillator naive, every filter as-is.
