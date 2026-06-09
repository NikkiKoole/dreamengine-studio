# Stereo — the design resolution (§9 open question)

> **Genre: design call ahead of build.** Resolves the long-standing §9 "Stereo?" open question
> in [`audio-notes.md`](audio-notes.md) so the answer exists *before* the effects-bus layer
> ([`instrument-engines.md`](instrument-engines.md) §8.10) gets built — stereo is that layer's
> prerequisite. No code yet; this is the API shape, the pan-law decision, and a pre-flight
> checklist of what bites. Fold the "shipped" note back into audio-notes §9 when it lands.
> Written 2026-06-09.

## Why now

Stereo isn't a feature in its own right — it's the **prerequisite for the effects layer**. Reverb,
delay, and Leslie are *where width lives*; the Rhodes suitcase auto-pan (a [§8.10.1](instrument-engines.md)
PARKED item) is meaningless in mono. So the §8.10 sequencing rule — *"resolve stereo before or
alongside this layer"* — makes this the gate to open first. The audience caveat still stands
(`audio-notes.md` §9: *"likely low leverage for this audience"* — beginners on laptop/phone
speakers), which is exactly why the surface stays tiny and the default is "you never touch it."

## The API

Pan is a **position** — a primitive, not an effect — so it gets a real knob (unlike width effects,
which are §8.10 *recipes* per [ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md)).
It resolves at the **same scope the filter/duty questions did** (§9 precedent: filter → per-instrument;
duty → on-instrument + live), so:

```c
void instrument_pan(int slot, float pan);   // -1 left … 0 center … +1 right; default 0 (mirrors instrument_duty/_filter)
void note_pan(int handle, float pan);        // live pan on a held note (mirrors note_duty) — enables auto-pan
```

Plus one LFO destination, since the rack already carries `LFO_VOLUME`/`LFO_CUTOFF`/etc.:

```c
#define LFO_PAN  7   // auto-pan as a declarative LFO target (mirrors the other LFO_* destinations)
```

That's the **entire engine surface**: two functions + one constant, wired the four places
(`studio.h` / `studio.c` / `studioDocs.js` / `shell.js`, per CLAUDE.md).

**What is NOT new API** (it's a recipe / bus effect, §8.10):
- **Width** — ping-pong delay, stereo reverb spread.
- **Auto-pan** as an *effect* — built from `note_pan` + `LFO_PAN`; the parked suitcase auto-pan and
  the parked epiano tremolo finally get their real home here (one shared, phase-coherent bus LFO,
  not a per-voice one — see [§8.10.1](instrument-engines.md) for why per-voice phase-resets drift).
- **Positional audio** (pan follows a sprite's screen-x) — stays **cart-land** (ADR-0006): the cart
  computes pan from the entity and calls `note_pan`. The engine owns no camera→pan mapping.

## The pan-law decision — LINEAR, center unchanged

Two laws, one real trade:

| Law | center gain | mono carts | loudness across the arc |
|---|---|---|---|
| **Linear (chosen)** | `L = R = mix` | **byte-identical** | center is +3 dB vs hard-panned (center buildup) |
| Constant-power | `L = R = mix·0.707` | every level shifts | consistent (−3 dB center is the point) |

**Decision: linear, center = `L = R = mix`.** For a learning console where ~all carts are centered
and never pan, the constant-power law would shift every existing cart's level and invalidate every
committed `--det` WAV baseline to fix a loudness inconsistency that only manifests *while actively
panning*. Don't regress the 99% case. Cache `panL`/`panR` on the pan *write* (not per sample):
`panL = (pan <= 0) ? 1 : 1-pan`, `panR = (pan >= 0) ? 1 : 1+pan` — a centered voice keeps full gain
on both, a hard pan zeroes the far side. Revisit constant-power only if a cart that pans heavily
sounds wrong — and if so, gate it behind the law, don't flip the default.

## What bites — pre-flight checklist

Grounded in the current mono mix path (`sound.h` `sound_callback`, the single `float mix` →
`out[i]`). Order is rough effort, cheap → expensive.

1. **Interleaved-buffer frame/sample trap.** Today `LoadAudioStream(SR, 32, 1)` + `out[i] = mix`.
   Stereo = `…, 32, 2)`; the callback's `frames` still counts **frame pairs**, so write `out[2*i]`
   / `out[2*i+1]`. Off-by-one here = half-speed/chipmunk audio or a buffer overrun. Verify first
   with the sound tripwire.
2. **Pan wants slew.** A hard `note_pan` jump zippers exactly like a raw vol/duty jump — give it the
   same anti-zipper slew the mix loop already applies to `vol`/`duty`/`cutoff`. Forgetting it = an
   audible click on every pan move.
3. **Synthesis does NOT double — only the accumulate does.** `api-notes.md` claims stereo "doubles
   every voice's per-sample math"; that's pessimistic. `s = sound_engine_sample(...)` is computed
   **once**; only the final `mix += contrib` splits into `mixL/mixR` via the cached gains. Cost is
   ~one extra multiply-add per voice, not 2× synthesis. (Correct the api-notes line when this ships.)
4. **`steal_tail` is a single mono float.** The steal-declick tail would tick in the **center** even
   when the stolen voice was hard-panned. Fix: pan the tail with the stolen voice's last `panL/panR`.
   Trivial, but audible if skipped on a panned pluck.
5. **Per-channel soft-clip shifts the image.** The master soft-clip runs on one `mix` today. Clip L
   and R *independently* and a hot hard-panned signal clips asymmetrically → the apparent pan
   **wanders under load**. Derive the clip gain from a mono sum (or peak of the two) and apply it to
   both channels; don't clip each in isolation.
6. **Mono-fold cancellation — and we target phones.** Any *width* trick using inter-channel
   delay/phase (Haas, ping-pong tails) comb-filters when summed to mono, and a lot of mobile/laptop
   playback is effectively mono — a voice that's wide on headphones can partially *vanish* on a
   phone speaker. **Rule: pan by amplitude only, never delay/phase, unless mono-fold is explicitly
   tested.** (Pairs with the mobile-web target — see [`mobile-web-notes.md`](mobile-web-notes.md).)
7. **The echo bus stays mono in v1 (decision).** It's a single mono delay line today. Ping-pong
   (the headline stereo-delay feature) doubles `SOUND_ECHO_MAX` memory **and** reintroduces the
   mono-fold risk (#6) — so it's a deferred §8.10 *bus recipe*, not part of stereo enablement. In v1
   the echo wet sums into both channels equally (mono-sent-to-both). Open ping-pong deliberately
   later, with a mono-fold test.
8. **The `--wav` / `wav-analyze.js` / `wav-envelope.js` harness is mono-assuming — the biggest
   surface-area job.** `wavcap_buf[pos++] = mix` is mono; the hand-written WAV header is `ch = 1`
   (the `block/ch` fields near the top of `sound.h`); both analyzers read mono frames. Stereo means:
   interleaved capture, `ch=2` + doubled byte-rate/block-align in the header, and de-interleaving in
   both tools (the two-file **bytes-identical** compare in `wav-analyze.js` silently reads garbage
   otherwise). And every committed `--det` baseline regenerates **once** at the flip — a known,
   acceptable one-time reset, but flag it so a diff across the boundary isn't read as a regression.
   This (not the DSP) is the long tail: the DSP is ~a day; re-validating baselines + teaching the
   analyzers to de-interleave is the rest.

## Build order when this opens

1. ✅ **DONE 2026-06-09.** Stream → 2ch + the `out[2*i]`/`out[2*i+1]` split, centered (`L=R=mix`) —
   verified byte-identical: a `--det` render's left channel matched the pre-change mono baseline on
   all 220,500 samples, L==R everywhere, frame count exact (no chipmunk). Soft-clip applied to the
   shared mix pre-split (bite #5 pre-empted). The `--wav` path (`wav_stream_pump` + header) and both
   analyzers (`wav-analyze.js` / `wav-envelope.js`, downmix L+R→mono) went stereo-aware in the same
   commit so master is never left half-broken; the live-capture `wavcap` tap stays mono (it reads the
   internal `mix`, not the interleaved `out`). **Not yet done from #8:** regenerating committed `--det`
   baselines (deferred until the pan knob actually changes output — centered output is still identical).
2. ✅ **DONE 2026-06-09.** `instrument_pan(slot,pan)` / `note_pan(handle,pan)` + `LFO_PAN` (dest 7),
   pan slewed like vol/duty (anti-zipper), per-instrument default inherited at note-on. 4-place wired
   (studio.h / sound.h impl / studioDocs.js / shell.js) + tcc symbols regenerated. Showcase: the
   **pan** cart (ball-follows note_pan + hard-L/R instrument_pan + an LFO_PAN drone). Verified a
   panned render is genuinely stereo: 99.9% of frames L≠R, max |L−R| 6860, L/R balanced over a full
   sweep. **Pan computed inline** from the slewed base + LFO offset (not cached) — slew changes it
   most samples during a sweep anyway, so the per-sample 2-ternary cost is below caching's bookkeeping.
3. ✅ **DONE 2026-06-09 (folded into step 2 — adding pan made both bites live at once).** Soft-clip
   now derives ONE gain from the peak of the two channels and applies it to both, preserving the pan
   ratio (#5); the steal-declick tail is per-channel `steal_tailL/R`, paid the stolen voice's panned
   `last_outL/R` (#4). Both are **byte-identical when centered** (verified), so no regression.
4. ✅ **Mostly done (step 1).** WAV capture interleaved + both analyzers de-interleave. **`--det`
   baseline regen NOT needed** after all: with the linear/center-unchanged law, every existing cart
   (none of which pan) is byte-identical — only a cart that actually calls pan changes output.
5. ✅ Tripwire clean in stereo; centered `--det` render byte-identical to the pre-stereo mono baseline.
6. **Next: the effects layer (§8.10)** — master reverb first, then delay, then ping-pong/auto-pan as
   the stereo-aware bus recipes the §8.10.1 parked items (per-voice wah, follower, tremolo, suitcase
   auto-pan) fold into. Pan + `LFO_PAN` are the primitives those recipes will build on.
