# braindance radio — the blind band brief (Phase 1)

Written *before* opening any cousin cart (radio-station-howto.md firewall). Working name:
**`braindance`** (Rephlex's term for the aesthetic). This is **Path B** of the Aphex problem
(see [`future-stations.md`](future-stations.md) → "Aphex Twin, decomposed"): Aphex is a *range*,
not a genre, so this station builds **one mode only — drill'n'bass / braindance** — and leaves
the others to carts that already cover them (ambient → `ambient.c`, solo piano → `satie.c`).

The thing to hold onto: **braindance is beauty over brutality.** It is NOT just fast drums —
it's a sweet, childlike, slightly-wrong melody floating *over* impossible drum chaos. Strip
the melody and it's jungle; strip the chaos and it's a music box. The station is the *tension
between the two layers*. That's why the core brain is "two crossed density curves," not "fast
drums."

## The band, from the music (not the palette)

Aphex Twin's braindance/drill'n'bass (*…Richard D. James Album*, *Come to Daddy*, *Windowlicker*,
the *Drukqs* drill tracks; kin: Squarepusher, µ-Ziq): chopped **breakbeats** shredded into
gravity-defying rolls, a deep simple **sub**, the occasional **acid** squelch, and over it all a
**lush, nostalgic, faintly eerie** melodic top — detuned strings, a music-box/bell motif, wordless
choir "aahs." Everything is **a few cents out of tune** (tape/varispeed warmth — the machine is
alive and slightly broken), and it **lurches**: sudden drops to near-silence, sections that mutate
rather than repeat.

The ideal lineup (intent-first):

| role | ideal voice | the Aphex reference |
|---|---|---|
| the break kit | chopped Amen/Think — shredded, pitched, gated, ratcheted into impossible rolls | every drill track; the whole point |
| sub bass | deep, simple, round (sine/saw), sits under the chaos | "4," "Cock/Ver10" |
| acid line | 303 squelch, occasional — the rubbery counter-melody | "Come to Daddy," Windowlicker funk |
| sweet pad | detuned warm string-machine wash, nostalgic, slow | RDJ album beds |
| music-box / bell melody | celesta / glock / detuned bell, childlike, *slightly wrong* | "Girl/Boy Song," "Flim" |
| choir "aahs" | wordless formant voice, eerie-sweet | RDJ album, "Windowlicker" |
| (timbre over everything) | systematic MISTUNING — per-voice detune + slow tape-wow | the signature character |

## The dial — track feels (rolled per song, the "modes within the mode")

Not full song-archetypes like AIR (this is one genre-mode); these are **poles the seed rolls**,
mostly by re-weighting the two density curves and the tempo:

- **sweet** ("Flim" / "Girl/Boy") — melody-forward, the music box sings; drums present but the
  beauty wins. Crossed curves at their widest.
- **drill** (*Drukqs* / "Bucephalus") — drum-forward, melody sparse and stabby, **max volatility**,
  ratchet storms; the brutality wins.
- **funk** ("Windowlicker") — slinky half-time feel, the acid line up front, a vocal-chop hook.
- **eerie** — sparse, near-ambient, the choir/pad dominate, drums intermittent — the bridge
  toward `ambient.c` but with grit and mistuning.

## Phase 2 — palette shop (gaps = new recipes; SAVE THE GAPS)

| imagined voice | what we have | note |
|---|---|---|
| sweet pad | `saw/solina-ensemble` / `saw/string-machine` (air, italo, house, motorik) — **reuse** | detuned-SAW pair |
| music-box / bell | `mallet/celesta` / `mallet/glockenspiel` (mallet.c) — **reuse**, detune it | the childlike top |
| choir "aahs" | `INSTR_VOICE` (air's vocoder lead is the first melodic use) — **reuse/extend** | wordless formant |
| acid line | `saw/303` (tb303.c) — **reuse**, the fast per-step squelch is unclaimed | `note_cutoff`/`note_res` per step |
| sub bass | SINE/SAW + pitch-env — trivial | — |
| mistuning glue | `instrument_tune` detune pairs + slow `note_lfo(LFO_PITCH)` (the BoC "tape memory") | a shared *timbre brain* |
| **the chopped break kit** | **GAP — no sampler / granular / time-stretch.** Fake it: synth+noise break voices (808/909/cr78 circuits on loan) driven by the volatility grammar + ratchets + per-hit pitch/decay drift (Autechre move) | the genre's defining production tool; **log it** |

**Effects/instrument gaps to record** ([`sound-next-steps.md`](sound-next-steps.md), per the
runbook save-the-gap rule): (1) a **breakbeat sampler / granular time-stretch** — the single
biggest honest gap; we approximate the chopped-Amen character with ratcheted+pitch-drifted synth
hits, which is *evocative, not faithful* — don't pretend otherwise; (2) **reverb** (the space the
beauty needs); (3) **tape varispeed / wow** (the global mistuning is faked per-voice instead).

## Brain picks (Phase 4)

The **core engine** is the three rhythm brains the IDM wing was scouted for:

1. **Rhythm-as-grammar + a volatility knob** — anchors persist (kick near 1, snare backbeat-ish),
   everything else re-rolls per bar; *fills are the norm.* `volatility` 0 = a stable break loop →
   1 = never the same bar twice. **The first brain for rhythm, not harmony.**
2. **Ratchets** — replace a hit with 3–8 sub-hits at 1/32–1/64 with vol/pitch ramps (the
   impossible rolls). ~10 lines, like `echo_hit`; viable only since the sample-exact timing fix.
3. **Two crossed density curves** — the serene melodic top and the violent drums move on
   *independent* curves (the "Flim" move). This is the station's identity; it spans melody AND
   rhythm, so it's the brain the feel-roll actually bends.

The four layers the rhythm brains DON'T cover, decided here:

- **Harmony — static, not functional.** A still pool, not a progression: 1–2 chords held (modal
  drift / the vamp brain), minor or lydian-nostalgic. The motion is *rhythmic*; the harmony is a
  beautiful frozen bed. (Chord brain #2 or #3.)
- **Melody — the re-pitched cell, sweet & sparse**, music-box voiced, *slightly detuned*; the
  human/childlike counterweight to the drums. Occasional `improv.h` run at a peak.
- **Bass — deep sub on the root**, simple; the acid line (`saw/303`) as an occasional second bass.
- **Form — THE LURCH.** Not 8×8 sections: a process-form with **sudden-event drops** (strip to the
  melody for 2 bars, then slam back) and mutate-don't-repeat. (motorik's unbuilt process-form + a
  drop-event channel.)
- **Timbre — systematic mistuning** as a 4th brain (detune pairs + slow pitch LFO = tape memory),
  applied to every voice so the drums aren't sterile.

**New brains this cart delivers:** rhythm-grammar+volatility · ratchets · 2-D crossed density ·
mistuning-as-timbre · sudden-drop process-form — **4–5 new brains, the top of the board.**

**Chassis (Phase 3, when building):** `radio.h` clock + the dance stations for circuits on loan
(`house`'s 808/pump, `cr78` kit, `dub`'s `echo_hit` — which doubles as the "Avril 14th" one-bar
canon generator). The rhythm grammar, ratchets, and crossed curves are new layer-3 logic.

**Seeds:** composition (the track feel, the harmony bed, the melody cell, the break anchors, the
volatility baseline) on `rad_srnd`; performance (the per-bar re-rolls, ratchet placement, the
mistuning jitter) on engine `rnd()` — so R replays the *track* but the drums shred differently
every time. That split is especially apt here: a braindance track that's never bar-identical on
replay is exactly right.
