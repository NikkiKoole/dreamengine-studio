# Polo & Pan — the effects the music asked for (all shipped, all wired)

The blind brief for the **polopan** station ([`polopan-blind-brief.md`](polopan-blind-brief.md))
named the band *from the music* (intent-first,
[`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md)). Shopping it went
*well on timbre* — Polo & Pan's mallet/pizz/flute/vocoder lineup maps onto modeled engines, and
the cart voices `INSTR_MALLET` (marimba/balafon/glock/vibe), `INSTR_BOWED` pizzicato,
`INSTR_PIPE` flute, `INSTR_VOICE` chant, and a resonant MS-10 `saw/ms10-bass`. What the *music*
also needs is the wash: Polo & Pan is escapist dream-pop **drenched in reverb, lush chorus, and
tape echo**. Sibling notes: [`air-effects-wants.md`](air-effects-wants.md),
[`afrobeat-effects-wants.md`](afrobeat-effects-wants.md). Roster + build order:
[`sound-next-steps.md`](sound-next-steps.md) § 8.10.

> **Genre: design exploration**, not a bug list — a wishlist tied to one station's brief.

## The wash the music asks for

| want | why | status |
|---|---|---|
| **Reverb — the drench.** Mallets, pizz, flute, glass and the Solina pad all bloom into a hall; the ambience *is* the sound (Nanga and Coeur Croisé especially). | — | ✅ **USED.** `reverb(size,damping)` + per-slot `instrument_reverb()` sends, **coupled to the archetype** — a big hall on Nanga/Coeur (size 0.68–0.70, wet ×1.05–1.10), a small dark room on the driving Tunnel (0.40, damping 0.62, wet ×0.55). Low end + kick + clap kept dry/punchy. |
| **Chorus — the Solina/Juno shimmer.** The detuned string-machine wash is half the P&P signature; the bright arps and pads want that swirl. | — | ✅ **USED, per-instrument.** `instrument_chorus()` on the lush voices (pad most, then mallets/pizz/lead/arp/solo), archetype-coupled — lush on Coeur/Nanga, light on Canopée/Ani Kuni, **OFF on Tunnel**. Because it's per-slot (not master), the four-on-the-floor **kit + bass stay bone dry** while the pad swirls. |
| **Tape saturation — the Caravelle warmth.** The whole mix wants warm analog tape (soft-clip + a whisper of wow/flutter), the hi-fi-but-warm sound of their records. | — | ✅ **USED.** `tape(wow,flutter,saturation)` master, archetype-coupled — warm on the vintage/dreamy archetypes (Nanga sat 0.30, Coeur 0.28), nearly off on the modern tight Tunnel (0.12). The soft-clip also tames peaks. |
| **Tape echo — the dub throws.** The flute and the Canopée mallet tails want a *dub* tape-echo throw — a warbling, filtered, self-feeding repeat. | — | ✅ **AVAILABLE today — not a gap.** The tape echo already exists: `echo()` (the RE-201 bus, see `spacecho`) for the repeats, with master `tape()` stacked on it for the wow/flutter wobble. Currently **not wired** in polopan — a taste call, not a missing engine. To add: see "the dub throws" below. |

## Also nice, lower priority

- **Per-part routing — ✅ done** (it's how the chorus is wired, above). Tape stays **master** on
  purpose (the whole record to tape is the Caravelle sound); reverb is per-slot.

> **Engine note (resolved):** the original out-of-tune flute complaint was an `INSTR_PIPE` bug
> (octave-low + flat) — **fixed in `sound.h`**, and the Nanga flute was also **dropped an octave**
> into the in-tune/natural flute range (`241a199`). The `pipe`/`sine` flute A/B stays in the band
> panel as an option.

## The dub throws (optional — available now, just unwired)

If we want the flute / Canopée-mallet throws, no new engine is needed — wire `instrument_echo()`
sends on `I_STAR` (the Nanga flute especially) and the Canopée `I_MAL` tails, configure the
`echo()` bus (the RE-201) for a dub-length feedback, and the master `tape()` already supplies the
tape wobble on the repeats. Archetype-coupled like the reverb, dialed back on Tunnel. Left out for
now as a taste call.
