# Polo & Pan — the effects I wanted (and what's still pending)

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

## The three the music asks for

| want | why | status |
|---|---|---|
| **Reverb — the drench.** Mallets, pizz, flute, glass and the Solina pad all bloom into a hall; the ambience *is* the sound (Nanga and Coeur Croisé especially). | — | ✅ **USED.** `reverb(size,damping)` + per-slot `instrument_reverb()` sends, **coupled to the archetype** — a big hall on Nanga/Coeur (size 0.68–0.70, wet ×1.05–1.10), a small dark room on the driving Tunnel (0.40, damping 0.62, wet ×0.55). Low end + kick + clap kept dry/punchy. |
| **Chorus — the Solina/Juno shimmer.** The detuned string-machine wash is half the P&P signature; the bright arps and pads want that swirl. | — | ✅ **USED.** `chorus(rate,depth,mix)` per archetype — lush on Coeur (mix 0.45) and Nanga (0.35), light on Canopée/Ani Kuni (0.20–0.25), **OFF on Tunnel** (it stays tight). *Caveat (shared with air):* chorus is **master-wide** in v1, so I can't chorus *only* the pad without washing the kit — polopan works around it by re-setting the master chorus **per song** (so each archetype gets the right amount), but within a song the kit shares the pad's chorus. The per-part routing waits for the deferred aux bus (sound-next-steps § 8.10). |
| **Tape delay — the throws.** The flute and the Canopée mallet tails want a tape/ping-pong delay throw — a warbling, filtered, slewed repeat, not clean taps. | The one bus we have (`echo()`) is a clean digital delay; using it as a tape-delay stand-in reads wrong (and the station owner asked to wait for the real one). | ⏳ **PENDING — deliberately NOT wired.** No `echo()` stand-in; the flute/mallet throws wait for the **real tape-delay engine**. polopan is a **demand witness** for it. Marked in `polopan.c` (`voice_song()` FX block comment). |

## Also nice, lower priority

- **Tape saturation** — a touch of warm tape on the master would suit the Caravelle-era warmth
  (kin to air's want). Low priority; the cart sounds right without it.
- **Per-part / aux routing** — would let the Solina pad be chorused while the four-floor kit
  stays dry *within the same song* (today it's per-archetype only). Tracked in § 8.10.

## When the tape delay lands

Wire light, slewed/filtered throws on `I_STAR` (the Nanga flute especially) and the Canopée
`I_MAL` mallet tails — archetype-coupled like the reverb, dialed back on Tunnel. Then strike the
PENDING row above and remove the deferral comment in `voice_song()`.
