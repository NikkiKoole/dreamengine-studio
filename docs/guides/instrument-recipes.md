# Instrument recipes — the grabbable-patch palette

The supply-side companion to [`instrument-presets.md`](instrument-presets.md). Together
the two files cover both ends of the same trade:

- **[`instrument-presets.md`](instrument-presets.md) + [`radio-voices.md`](radio-voices.md)
  = the demand side** — what the 20 radio stations *use*: the named voices each station
  hand-builds in its `init()`, charted per station, so a shared sound becomes visible on
  one page.
- **`instrument-recipes.md` (this file) = the supply side** — what instrument recipes are
  *available to grab right now*, across the showcase carts, the Roland/classic drum machines,
  the whimsical instruments, and the synth playgrounds. **The showcase carts ARE the de-facto
  preset library** — `mallet.c`'s `PRESET[3]` "vibes" *is* the radio's vibes, `organ.c`'s
  registrations *are* the reggae chop, `moog.c`'s signal path *is* carlos's fat Moog. This
  file indexes every such recipe at its source so the next station builder can shop the
  shelf instead of re-deriving a Wurlitzer from scratch.

A **recipe** here is one cart's complete patch for an `INSTR_*` engine: the `instrument()`
ADSR, any filter/drive, and the `harmonics/timbre/morph` macros (the three universal knobs
on the modeled engines). Unlike the demand side, recipes are *not* deduplicated by sharing
tier — the point is the catalog of what exists, organized by **engine**, so you can see
every way each engine has been voiced.

**Recipe shorthand** is the same legend as
[`instrument-presets.md` → "How to read an entry"](instrument-presets.md): `A/D/R` in ms,
`S` = sustain 0–7; `LP/HP/BP n/q` = filter pass at *n* Hz, resonance *q*; `h/t/m` =
`harmonics/timbre/morph` (0–1); `drive`, `pitch-env`, `cut-env` from `instrument_drive` /
`instrument_env`. Not duplicated here.

> The modeled engines (`PLUCK`, `GUITAR`, `MALLET`, `FM`, `EPIANO`, `ORGAN`, `PD`,
> `MEMBRANE`, `REED`, `PIPE`, `BOWED`, `PIANO`, `VOICE`) each ship one showcase cart whose
> presets are baked **macro positions** on a shared base `instrument()` — the cart is the
> engine's tuning rig. The raw-wave and drawn-wave recipes (last two sections) instead vary
> the *waveform* and full ADSR/filter per voice. Drum-machine voices live under whichever
> engine synthesizes them, **labeled by machine** (808 / 909 / CR-78).

---

## INSTR_PLUCK — Karplus-Strong string

| name | source cart | recipe | character |
|---|---|---|---|
| pluck/default | pluck.c (showcase) | A1 D0 S7 R1200 · h0.5 t0.55 m0.35 | Default ring/brightness/pick-position; the cart is an interactive explorer (no PRESET array), this is the boot knob state. |
| pluck/guitar | spacecho | A1 D0 S7 R150 · h0.72 t0.55 | Bright plucked string with good ring, feeding the RE-201 tape-echo chamber (the effect *is* the instrument). |

> **Cross-ref:** the PLUCK guitars on the radio (`pluck/strat-stab`, `pluck/jangle-guitar`,
> `pluck/nylon-guitar`, `pluck/krieger-guitar`, `pluck/archtop`…) are all hand-rolled
> per-station, not lifted from pluck.c — but pluck.c is the engine's reference rig. The
> macro space pluck.c exposes (ring/brightness/pick-position) is exactly the three knobs
> those station voices bend.

## INSTR_GUITAR — Karplus-Strong string + resonant body

All from **guitar.c** (showcase), same base `A1 D0 S7 R1200`; presets vary only the three
macros. Gate is dynamic: `600ms + (1−morph)² · 14000ms`.

| name | source cart | recipe | character |
|---|---|---|---|
| guitar/harp | guitar.c | h0.05 t0.55 m0.05 · gate~14.6s | Open clear KS, minimal body, long ring — a harp. |
| guitar/nylon | guitar.c | h0.45 t0.20 m0.22 · gate~10.4s | Mid-resonance warm nylon acoustic, dark, gentle mute. |
| guitar/steel | guitar.c | h0.55 t0.70 m0.20 · gate~10.6s | Bright steel-string acoustic, resonant box, medium decay. |
| guitar/banjo | guitar.c | h0.95 t0.85 m0.45 · gate~6.0s | Bright percussive banjo, high resonance, short twangy decay. |
| guitar/koto | guitar.c | h0.80 t0.90 m0.28 · gate~9.5s | Bright Japanese koto, high resonance, long ring, light mute. |
| guitar/uke | guitar.c | h0.60 t0.40 m0.42 · gate~6.8s | Warm small-bodied ukulele, moderate resonance, shorter decay. |
| guitar/pizz | guitar.c | h0.20 t0.60 m0.85 · gate~0.97s | Tight muted pizzicato, minimal body, heavy mute, quick stop. |
| guitar/oud | guitar.c | h0.50 t0.20 m0.55 · gate~4.4s | Dark resonant Middle-Eastern oud, warm timbre, short decay. |
| guitar/mistress | mistress.c | h0.55 t0.70 · A2 R600 · **through `flanger()`** | Bright steel-string strummed into the master flanger — the source isn't the point, the swept comb is. mistress.c exists to show `flanger()`; the guitar is a clean broadband source for it. |
| guitar/combo | combo.c | h0.55 t0.30–0.75 m0.20 · A1 R1200 · **into the amp bundle** | A steel-ish string feeding the combo amp's pinned output stage; `timbre` shifts per voicing (steel 0.75 → nylon 0.30). The character lives in the amp recipe (`drive`+`DRIVE_*`+`eq`+`glue`), not the string — see [`effects-recipes.md` → "amp / cab"](effects-recipes.md). |

> The body-resonance macro (`harmonics`: open/harp→banjo) is what `INSTR_GUITAR` adds over
> `INSTR_PLUCK`. No longer an untapped shelf: `afrobeat` (interlocking rhythm guitars), `air`
> (nylon), and `mariachi` (vihuela / guitarra / guitarrón — one engine, three registers) all
> reach it now. These eight are still the reference rig.

## INSTR_MALLET — modal struck bar

| name | source cart | recipe | character |
|---|---|---|---|
| mallet/marimba | mallet.c (showcase) | A1 D0 S7 R1200 · h0.00 t0.45 m0.35 | Bright wooden struck bar, warm mellow marimba. |
| mallet/xylophone | mallet.c | h0.15 t0.80 m0.15 | Dry percussive click, hard bright xylophone attack. |
| mallet/celesta | mallet.c | h0.50 t0.55 m0.45 | Glass-like shimmer, metallic bar, moderate sustain. |
| mallet/vibraphone | mallet.c | h0.25 t0.50 m0.90 | Spinning vibe motor, long rich sustain with modulation. |
| mallet/glockenspiel | mallet.c | h0.90 t0.85 m0.60 | Bright metallic bell, high partials, steady ring. |
| handpan/ring | handpan.c (whimsical) | A1 D0 S7 R1400 · h0.55 m0.72 | Steel hang-drum field — warm, metallic, long ring; per-strike offset (0.14–0.72) sweeps soft palm thump → bright ping. |
| handpan/tok | handpan.c | A1 D0 S5 R200 · h0.32 t0.85 m0.05 | Dry woody shoulder strike on bare steel — percussive tick, bone-dry. |
| mallet/struck | mt70.c (showcase) | A1 D0 S7 R1200 · no filter | Alternate engine for mt70's struck presets (VIBES/CHIME/BELLS/CELSTA/BANJO) via the SRC control. |

> **Cross-ref (the catalog's clearest copy-propagation case):** mallet.c's `PRESET[3]`
> "vibraphone" (`h0.25 t0.50 m0.90`, key 4) is the source of the radio's shared
> `mallet/vibes` — lowend, motorik, and cocktail all hand-copy these exact macros (see
> [`instrument-presets.md` → mallet/vibes](instrument-presets.md)). `PRESET[2]` "celesta"
> is lifted directly into exotica's bell chair (`mallet/celesta`). The default-MALLET
> `hit()` (no macros) also backs `multitouch`'s touch-paint pings.

## INSTR_FM — 2-op DX-style FM

All from **fm.c** (showcase): a DX-FM rig where each preset is 3 macros (harmonics ratio,
timbre brightness, morph feedback) + a separate ADSR. The 1:1 detent carries a built-in DX
tine harmonic.

| name | source cart | recipe | character |
|---|---|---|---|
| fm/epiano | fm.c | A2 D703 S3 R351 · h0.15 t0.45 m0.10 | DX E-piano, bright strike + mellow tail, 1:1.5 ratio with a singing tine. |
| fm/bell | fm.c | A1 D894 S2 R596 · h0.55 t0.60 m0.15 | Bright bell, long resonant decay, 1:3.5 ratio (inharmonic sidebands). |
| fm/bass | fm.c | A1 D245 S5 R121 · h0.00 t0.75 m0.30 | Punchy sub-ratio carrier, bright mod, feedback growl, no tine. |
| fm/brass | fm.c | A70 D204 S6 R184 · h0.15 t0.90 m0.45 | Slow brass swell (70ms attack lets the brass speak), bright peak then mellow. |
| fm/clang | fm.c | A1 D609 S2 R395 · h0.55 t0.95 m0.95 | Bell detent (1:3.5) cranked to extreme brightness + feedback — metallic clang. |

> **Cross-ref:** the radio's FM voices (`fm/rhodes` yacht, `fm/brass-stab` italo,
> `fm/ostinato-bass` addis, `fm/glass-bell`/`fm/finger-cymbal` exotica) are hand-built but
> map cleanly onto these five archetypes — `fm/epiano` is the Rhodes-detent shelf,
> `fm/bell`/`fm/clang` the bell/cymbal shelf, `fm/bass` the bass shelf. The 909's hats are
> also FM (see INSTR_FM under tr909, below).

**FM percussion kit** — from **fmbox.c** (the Elektron Model:Cycles reimagined): the SAME
`INSTR_FM` engine voiced as six drum "machines" via the three macros + a per-note pitch-env, all
on `instrument(slot, INSTR_FM, 0, 120, 0, R)` with the gate length set per hit. `fm/snare` is the
genuine new recipe (the brief's predicted gap) — an FM body layered with an `INSTR_NOISE` crack; no
other cart pairs FM with a noise layer.

| name | source cart | recipe | character |
|---|---|---|---|
| fm/kick | fmbox.c | R60 · h0.02 t0.15 m0.10 · pitch-env +27st→note, MIDI~32 | Sub-ratio FM kick; the hard downward pitch blip is the donk. |
| fm/snare | fmbox.c | R50 · h0.45 t0.80 m0.55 + INSTR_NOISE (BAND 1900/3) crack | Bright FM body under a band-passed noise snap — the new gap. |
| fm/metal | fmbox.c | R320 · h0.62 t0.85 m0.20 · long gate | Inharmonic ringing accent (off-integer ratio) — the metallic soul. |
| fm/perc | fmbox.c | R80 · h0.40 t0.55 m0.10 · pitch-env, short gate | Tuned glassy blip / tom, dry and short. |
| fm/tone | fmbox.c | R140 · h0.15 t0.45 m0.20 · MIDI~39 | Round low-ratio FM bass/lead — the melodic machine. |
| fm/chord | fmbox.c | R180 · h0.15 t0.50 m0.15 · root+3+7 triad | FM epiano-ish stab, the one polyphonic machine. |

> **Cross-ref:** these overlap the fm.c archetypes on purpose — `fm/metal` is `fm/bell`/`fm/clang`
> voiced short, `fm/kick`/`fm/tone` sit on the `fm/bass` sub-ratio shelf, `fm/chord` on `fm/epiano`.
> The genuinely new one is `fm/snare` (FM + noise). What makes fmbox distinct is per-STEP macro
> moves (p-locks) + conditional trigs, not the base voices — see
> [`../design/fmbox-blind-brief.md`](../design/fmbox-blind-brief.md).

## INSTR_EPIANO — electric-piano (Rhodes/Wurli/Clav models)

All from **epiano** (showcase), same base `A1 D0 S7 R1500`; the harmonics macro picks the
model (Rhodes 0.15 / Wurli 0.50 / Clav 0.85), plus WAH and TREMOLO modulation.

> ⚠ **Temporary — the TREM/WAH columns are provisional.** The tremolo and auto-wah baked
> into these presets are a stopgap living *inside* the instrument. They're slated to move to
> the planned **effects bus**; once that lands they may be **removed** from these recipes (the
> dry patch is just the macros + ADSR — the wobble/vowel-sweep becomes a bus send). Don't
> treat the `TREM`/`WAH` parts as a permanent property of the epiano recipes.

| name | source cart | recipe | character |
|---|---|---|---|
| epiano/rhodes | epiano | h0.15 t0.30 m0.25 · TREM 5Hz/35% | Warm suitcase Rhodes with classic volume wobble, clean fundamental. |
| epiano/rho-brite | epiano | h0.15 t0.78 m0.55 · TREM 5Hz/30% | Bright stage Rhodes with snap and bark, hard hammer. |
| epiano/suitcase | epiano | h0.15 t0.20 m0.12 · TREM 5Hz/45% deep | Mellow clean long-ring suitcase, deep tremolo, vintage. |
| epiano/wurli | epiano | h0.50 t0.35 m0.30 · TREM 5Hz/50% | Soul-ballad Wurlitzer, deeper amp wobble than Rhodes. |
| epiano/wur-buzz | epiano | h0.50 t0.66 m0.82 · WAH (BP 1300Hz, LFO 7.5Hz) · TREM 5Hz/55% | Cranked reed Wurli with auto-wah vowel sweep + heavy tremolo. |
| epiano/clav | epiano | h0.85 t0.75 m0.55 · WAH env (LP 500Hz, A2/R110ms → 2800Hz) · no TREM | Funky bridge-pickup Clavinet with fast env-wah quack. |

> **Cross-ref:** the radio voices its Rhodes/Wurli on `INSTR_EPIANO` too —
> `epiano/rhodes-detent` (italo) and `epiano/wurli-comp` (addis) are the station-side
> equivalents of `epiano/rhodes` and `epiano/wurli`. yacht instead fakes its Rhodes on
> `INSTR_FM` (`fm/rhodes`); lowend fakes one on TRI (`tri/tremolo-rhodes`). epiano's six
> presets are the modeled reference.

## INSTR_ORGAN — tonewheel drawbar

All from **organ.c** (showcase), same base `A1 D0 S7 R200`; eight registrations vary
`h/t/m` + drive, plus three Leslie rotor recipes layered on every note.

| name | source cart | recipe | character |
|---|---|---|---|
| organ/reggae | organ.c | h0.06 t0.55 m0.00 drive0 | Hollow upstroke registration, no motion — thin drawbars, dark. |
| organ/combo | organ.c | h0.19 t0.45 m0.30 drive0 | Soft cocktail combo with light chorus shimmer. |
| organ/bookerT | organ.c | h0.31 t0.45 m0.22 drive0 | 1960s clean registration, light chorus. |
| organ/jimmy | organ.c | h0.44 t0.55 m0.75 drive0 | Fat jazz B3, percussion + C3 chorus (boot default). |
| organ/larry | organ.c | h0.56 t0.60 m0.65 drive0 | Modern jazz, mid-range brightness + animation. |
| organ/ballad | organ.c | h0.69 t0.60 m0.55 drive0 | Sub+fund+sparkle registration, moderate animation. |
| organ/jonlord | organ.c | h0.81 t0.70 m0.40 drive0.30 | Rock growl — all bars bright, overdriven tube grit (combo-organ grind). |
| organ/gospel | organ.c | h0.94 t0.65 m0.88 drive0 | Full registration, all drawbars out, full shimmer + percussion chip. |
| leslie/off | organ.c | depth_vol0 depth_pitch0 rate0 | Leslie bypass — pure organ tone. |
| leslie/slow | organ.c | depth_vol0.45 depth_pitch0.16 → 0.8Hz | Slow chorale rotor — smooth tremolo + gentle doppler, mechanical spin-up. |
| leslie/fast | organ.c | depth_vol0.45 depth_pitch0.16 → 6.6Hz | Fast tremolo rotor — pulsing volume + pitch doppler, rapid spin-up. |
| organ/cathedral | cathedral.c | A30 D0 S6 R600 · h0.62 t0.50 m0.50 · **drenched in `reverb(size,damping)`** (instrument_reverb send ~0.6) | A plain full drawbar registration — the patch is almost beside the point; cathedral.c exists to show `reverb()`, so the organ is a clean source that *blooms* into a vast stone hall. The reverb is the instrument. |

> **Cross-ref:** organ.c is a confirmed showcase-cart lineage. `organ/reggae` (preset 0) is
> lifted verbatim into dub's skank (`organ/reggae-chop`). The radio's other organ voices —
> `organ/combo-drone` (motorik), `organ/addis-comp` (addis) — are hand-built registrations
> on the same engine, but organ.c is the registration shelf they shop. roadhouse instead
> draws its own combo-organ wave on `INSTR_USER0` (`user0/combo-organ`).

## INSTR_PD — Casio CZ phase distortion

All from **pd.c** (showcase); each preset is the wavetype/distortion/DCW-sweep macro triple
plus its own ADSR.

| name | source cart | recipe | character |
|---|---|---|---|
| pd/cz-bass | pd.c | A0 D400 R200 S3 · h0.06 t0.62 m0.40 | Bright saw, punchy decay, light DCW snap — classic CZ bass. |
| pd/reso-lead | pd.c | A20 D320 R300 S4 · h0.94 t0.55 m0.45 | Resonant saw with bright peak, sustained, moderate DCW sweep. |
| pd/synth-brass | pd.c | A420 D300 R320 S5 · h0.56 t0.70 m0.50 | Sawpulse hybrid, slow swell, buzzy bright sustain — CZ brass. |
| pd/sweep-pad | pd.c | A500 D620 R700 S4 · h0.69 t0.38 m0.95 | Resonant triangle, full-sweep DCW — the classic Casio "wowww" pad. |
| pd/cz-pluck | pd.c | A0 D300 R220 S0 · h0.31 t0.72 m0.80 | Tight pulse, hard DCW snap, no sustain — plucky percussive. |
| pd/pdbass | pdbass.c | A2 D150 S4 R220 · h0.20 m0.40 · t = TIMBRE slider 0→1 (clean→buzzy), live | A playable CZ bass on a string fingerboard. Sustains while held so SLIDES ring; INSTR_PD glides both ways (the bowed upright can't) → true smooth up/down slide + signed pull-bend. The one slider sweeps the distortion macro live. |
| pd/monochord | monochord.c | A2 D1500 S0 R300 · h0.50 t0.42 m0.50 | A plucked one-string: instant attack, long decay so it rings, sustain 0. Picked **per pitch from segment length** (no fixed key); INSTR_PD glides both ways so a ringing note bends as the bridge slides (the đàn bầu). |
| pd/fretboard | fretboard.c | A2 D1500 S0 R300 · h0.50 t0.45 m0.50 | The pd/monochord patch across six fretless strings (standard tuning E A D G B e), one ringing voice per string. Pitch from finger position along each string; glides as you slide. |

> **Cross-ref:** italo's `pd/soaring-lead` and addis's `pd/synth-horn` are the station-side
> PD voices — `pd/soaring-lead` mirrors `pd/reso-lead`, `pd/synth-horn` mirrors
> `pd/synth-brass`. pd.c is the engine's reference rig.

## INSTR_MEMBRANE — struck drumhead

All from **tabla.c** (showcase), same base `A1 D0 S7 R250`; macros are
tuned↔inharmonic / strike-position center↔edge / pitch-bend flat↔glissando.

| name | source cart | recipe | character |
|---|---|---|---|
| membrane/tabla | tabla.c | h0.10 t0.45 m0.45 · ring~1.8s | Tuned harmonic tabla, mid-strike + pitch glissando, rings open. |
| membrane/conga | tabla.c | h0.55 t0.35 m0.15 · ring~1.4s | Warm inharmonic conga, near-center, minimal bend. |
| membrane/bongo | tabla.c | h0.72 t0.65 m0.10 · ring~600ms | Bright inharmonic bongo, edge strike, flat pitch, snappy. |
| membrane/djembe | tabla.c | h0.85 t0.55 m0.22 · ring~900ms | Inharmonic djembe, mid-edge slap, moderate bend, medium thud. |
| membrane/tom | tabla.c | h0.62 t0.18 m0.55 · ring~1.5s | Center-strike boom, strong pitch drop, long resonant ring. |
| membrane/tom-kit | fingerdrums.c | h0.28 t0.18 m0.12 · A1 D0 S7 R300 @ MIDI 62/56/45 | Tuned drum-kit tom — tighter ring + far less bend than tabla's tom, played at three fixed pitches for a rack-hi / rack-mid / floor set. |
| membrane/upright-body | upright.c | A0 D170 S0 R60 · h0.45 m0 · LP2200 · belly t0.22 @ MIDI31–45 / neck t0.68 @ MIDI48–60 · + a NOISE knuckle | The bass body as percussion — slap the belly for a low center thump, knock the neck for a drier edge tick. Location maps pitch + strike position; same woody character. |

> **Cross-ref:** addis is the first radio station to use `INSTR_MEMBRANE` for real modeled
> drums — its `membrane/kebero`/`conga`/`bongo` and gamelan's `membrane/kendang` are
> hand-tuned, but tabla.c is the hand-percussion reference rig. (The 808/909/CR-78 drum
> machines use raw waves, not `INSTR_MEMBRANE` — see below.)

## INSTR_REED — blown-reed waveguide (self-oscillating)

All from **reed.c** (showcase), same base `A1 D0 S4 R1200`; macros are bore
cylinder↔cone / reed soft↔stiff / breath still↔growl.

| name | source cart | recipe | character |
|---|---|---|---|
| reed/clarinet | reed.c | h0.00 t0.30 m0.40 | Cylindrical bore, soft cane, dark hollow chalumeau. |
| reed/sop_sax | reed.c | h0.92 t0.70 m0.55 | Strongly conical, stiff reed, bright cutting soprano. |
| reed/alto_sax | reed.c | h0.78 t0.45 m0.55 | Conical, medium reed, warm jazz horn. |
| reed/tenor_sax | reed.c | h0.82 t0.30 m0.62 | Conical, soft big reed, breathy and round. |
| reed/oboe | reed.c | h0.55 t0.92 m0.50 | Narrow conical, very stiff reed, nasal and penetrating. |

> **Cross-ref:** gamelan's `reed/suling` (bamboo flute) is the first charted radio use of
> `INSTR_REED`. reed.c is the engine's reference rig — five orchestral reeds.

## INSTR_PIPE — blown-pipe / flute

All from **pipe.c** (showcase), same base `A1 D0 S4 R1200`; macros shape
embouchure/air/overblow.

| name | source cart | recipe | character | tuning |
|---|---|---|---|---|
| pipe/flute | pipe.c | h0.00 t0.38 m0.70 | Concert flute — pure fundamental, moderate air, focused embouchure. | ✓ in tune C4→~E6 |
| pipe/recorder | pipe.c | h0.00 t0.55 m0.30 | Hollow, breathier, low embouchure — medieval recorder. | ⚠ low embouchure → flat up high; keep below ~C5 |
| pipe/pan-pipe | pipe.c | h0.08 t0.78 m0.50 | Very airy, a shimmer of overblow. | ~ borderline (m0.50); fine mid-register |
| pipe/piccolo | pipe.c | h0.55 t0.28 m0.82 | Overblown to the octave, focused, less air. | ⚠ overblow = deliberate flageolet octave jump, not strict pitch |
| pipe/breathy | pipe.c | h0.00 t0.90 m0.42 | Maximum air, jazzy breathy flute. | ⚠ low-ish embouchure → flat up high |

> **⚠ PIPE TUNING — read before picking a flute voice.** A jet-drive waveguide flute's
> intonation **tracks the morph (embouchure) macro**: it's in tune for a FOCUSED embouchure
> (**morph ≳ 0.5**, like `pipe/flute` m0.70 — verified in tune C4→~E6) but a hollow/low
> embouchure (morph ≲ 0.4) or overblow (harmonics) drifts flat and gets unstable toward the
> top. **For a melodic flute lead, use morph ≳ 0.5 and keep it in a natural register, then
> verify:** `node tools/tune-check.js --engine PIPE --macros h,t,m --range lo-hi` (the default
> `tune-check.js` sweep tests morph 0 — the worst case — so always recipe-check a flute voice).
> The full why: [`design/audio-notes.md`](../design/audio-notes.md) §18, [`STATUS`](../STATUS.md) #31.

> **Cross-ref:** the radio dial now uses `INSTR_PIPE` for real — **air**'s Cherry Blossom flute
> (`pipe/air-flute`, m0.70, register 64–86) and **polopan**'s NANGA flute (m0.68, dropped to
> C4–F5 to sit in the in-tune zone). Earlier breathy flutes faked it on SINE (bossa's
> `sine/breathy-flute`, jingle's `sine/singing-lead`); those stay as cheaper stand-ins.

## INSTR_BOWED — bowed-string waveguide

All from **bowed.c** (showcase), same base `A80 D0 S7 R300` (pizz uses A1); macros map the
Schelleng bowing wedge (position/pressure/speed).

| name | source cart | recipe | character |
|---|---|---|---|
| bowed/violin | bowed.c | h0.45 t0.30 m0.70 | Mid bow, light clean pressure, moderate speed — bright singing violin. |
| bowed/viola | bowed.c | h0.50 t0.38 m0.60 | Slightly over the fingerboard, added weight and fullness. |
| bowed/cello | bowed.c | h0.55 t0.42 m0.55 | Over the fingerboard, fuller pressure, slower bow — warm cello. |
| bowed/ponticello | bowed.c | h0.10 t0.55 m0.80 | Near the bridge, heavy pressure, fast bow — glassy edgy scratch. |
| bowed/tasto | bowed.c | h0.90 t0.18 m0.45 | Far over the fingerboard, feather pressure, slow bow — flute-like whisper. |
| bowed/tremolo | bowed.c | h0.40 t0.35 m0.85 | Fast hard bow speed, moderate position/pressure — driving tremolo. |
| bowed/pizzicato | bowed.c | A1 D0 S7 R300 (pizz, friction off) · h0.30 t0.42 | Plucked string, same waveguide, pluck seeded — warm finger pluck. |
| bowed/upright-pizz | upright.c | A4 D0 S7 R90–790 (RING) · eng_tune(0,1) pizz · h0.30 t0.30 m0.45 · LP 500–2700 (TONE) · low register E1–G3 | Jazz double-bass pizzicato — woody finger pluck that rings and thumps. The RING knob sets the release/damp tail. |
| bowed/upright-arco | upright.c | A110 D0 S7 R260 · bowed (friction on) · h0.40 t0.45 m0.50 · vibrato LFO_PITCH 5/0.12 · LP ~TONE+400 | The same string drawn with the bow — slow speak, sustains, left-hand vibrato. |

> **Cross-ref:** **`mariachi.c` is the FIRST radio station to use `INSTR_BOWED`** — its violin
> section (`bowed/mariachi-violin`, two desks panned wide + a portamento scoop). tango's violin
> section (`saw/violins-arco`/`saw/violins-pizz`) still fakes both arco and pizz on SAW; bowed.c
> models the real thing, including the arco↔pizz technique split (mirrors tango's pair).

## INSTR_BRASS — lip-reed waveguide (self-oscillating)

All from **brass.c** (showcase), held voice `A1 D0 S4 R1200`; macros are
harmonics = instrument/bore (trumpet→tuba), timbre = brassiness (round→blatty), morph =
breath/lip lean-in. The `oct` column is the cart's register shift, not part of the patch.

| name | source cart | recipe | character |
|---|---|---|---|
| brass/trumpet | brass.c | h0.15 t0.60 m0.42 (oct +1) | Tight bright bore, plenty of brassiness — the blatty lead horn. |
| brass/cornet | brass.c | h0.30 t0.44 m0.40 (oct +1) | A touch darker and rounder than the trumpet. |
| brass/flugelhorn | brass.c | h0.46 t0.28 m0.45 | Mellow conical bore, low brassiness — the dark warm one. |
| brass/trombone | brass.c | h0.56 t0.52 m0.46 | Mid bore — the slide horn (drag the slide for a glissando). |
| brass/french-horn | brass.c | h0.70 t0.34 m0.50 | Dark, round, a little breath — the orchestral horn. |
| brass/tuba | brass.c | h0.92 t0.46 m0.48 (oct −1) | Wide dark bore, an octave down — the bass brass. |

> **Cross-ref:** real `INSTR_BRASS` is on the dial now — `afrobeat`'s horn-section trumpet
> (first) and `mariachi`'s two trumpets (`brass/mariachi-trumpet`). The faked brass remains —
> italo's `INSTR_FM` brass stabs and citypop's saw-brass anticipation hits; an A/B retrofit of
> italo's stabs is a tracked follow-up (instrument-engines.md §8.8.10).

## INSTR_PIANO — StifKarp struck stiff string (inharmonic shimmer)

All from **piano.c** (showcase), same base `A1 D0 S7 R2000`; macros are
voicing/stiffness / hammer-softness / pedal-sustain.

| name | source cart | recipe | character |
|---|---|---|---|
| piano/grand | piano.c | h0.08 t0.50 m0.62 | Pure stiff-string tone, minimal stiffness, mellow felt hammer, medium-long decay. |
| piano/bright | piano.c | h0.25 t0.55 m0.55 | Bright metallic shimmer, harder plectrum attack, moderate sustain. |
| piano/harpsichord | piano.c | h0.42 t0.50 m0.20 | Pronounced stiffness + inharmonic stretch, hard plectrum, dry staccato. |
| piano/dulcimer | piano.c | h0.58 t0.50 m0.50 | Heavy metallic shimmer (mid stiffness), balanced hammer + sustain. |
| piano/clavichord | piano.c | h0.75 t0.50 m0.30 | Extreme stiffness, intimate soft hammer, short damped tone. |
| piano/celesta | piano.c | h0.92 t0.50 m0.60 | Maximum metallic shimmer, bright tuned percussion, long sustain. |

> **Cross-ref:** **no charted radio station uses `INSTR_PIANO`** — every piano on the dial
> is faked on TRI/SINE (`tri/felt-grand`, `sine/closed-lid-piano`, satie's `tri/felt-piano`).
> piano.c models the real thing; satie in particular is flagged as an *upgrade candidate*
> (it predates the engine) in [`instrument-presets.md`](instrument-presets.md). The six
> presets are a ready shelf.

## INSTR_VOICE — formant voice

All from the voice-lab carts; same base ADSR (`A45 D60 S7 R160`) plus formant/prosody
params (vowel, size, breath, openq, tilt, vibrato, nasal, F1/F2). Most voice carts are
audition tools that *modulate* one base voice rather than defining fixed patches — see "By
cart" for those. The named layouts:

| name | source cart | recipe | character |
|---|---|---|---|
| voice/gen | voxlab.c (showcase) | vowel0.5 size0.33 breath0.1 openq0.5 tilt0.3 vib0.15 | Neutral vowel foundation, moderate effort — vowel/size/breath axes. |
| voice/mouth | voxlab.c | vowel0.5 size0.33 breath0.04 openq0.45 tilt0.15 vib0 | Clean bright vowel, minimal breath — articulation layout. |
| voice/creat | voxlab.c | vowel0.3 size0.06 breath0.28 openq0.16 tilt0.55 vib0.05 | Tiny, breathy, roughened — experimental sonics, exaggerated tilt. |
| voice/sing | voxlab.c | vowel0.55 size0.42 breath0.12 openq0.58 tilt0.22 vib0.62/0.66 | Full vibrato-rich singing voice, strong vibrato amount + rate. |
| voice/formant | vox.c (voice) | A45 D60 S7 R200 · vib 5.5Hz · voice_consonant/coda | Interactive formant synth — VOWEL/SIZE/EFFORT axes, CV/VC onset/coda timing, pentatonic jam pad. |
| voice/melodyne | voxroll.c (melodyne roll) | A40 D80 S7 R220 · MONOPHONIC 1 voice · `note_pitch` drives the drawn contour · `note_glide` 45ms legato · `note_lfo` vib 5.5Hz · per-note VOWEL (harmonics) + SIZE (timbre) | Melodyne-style vocal piano roll — draggable vowel-blobs, FORMANT (size) decoupled from pitch via the ✱ tab, connect-on-touch glide (touching notes slide, gaps break + re-attack). |

> **Cross-ref:** no radio station uses `INSTR_VOICE` yet. The voice carts (voxlab, vox, say,
> voxab, voxpad) are the engine's reference + audition rigs; `voice/formant` (vox.c) is the
> only one with a single concrete `instrument()` recipe worth naming — the rest live-modulate
> a base voice (see "By cart").

## INSTR_USER0 — drawn / hand-edited single-cycle waves

| name | source cart | recipe | character |
|---|---|---|---|
| drawn/square | stylophone.c (synth) | A6 D130 S6 R110 · LP 2600/3 · square seed, 0–5 smoothing passes (TONE) | Hand-drawn wave, square seed; smoothing rounds raw edits into a 64-sample cycle. |
| drawn/sine | stylophone.c | A6 D130 S6 R110 · LP 2600/3 · sine seed, 0–5 smoothing | Sine seed; smoothing blends edits into a warped sinusoid. |
| drawn/sawtooth | stylophone.c | A6 D130 S6 R110 · LP 2600/3 · sawtooth seed, 0–5 smoothing | Sawtooth seed; smoothing rounds the ramp edits. |
| drawn/organ | stylophone.c | A6 D130 S6 R110 · LP 2600/3 · additive organ seed (f + 2f·0.55 + 3f·0.28 + 4f·0.18 + 5f·0.12), 0–5 smoothing | Additive harmonic organ seed; smoothing tames the multi-tone blend. |
| user0/mellotron-strings | mellotron.c (machine) | A140 D0 S6 R600 · LP 700–4000/2 (TONE) · vibrato LFO_PITCH 5.2/0.10 · drift LFO_CUTOFF 0.30/220 · 7-harmonic bowed stack · + noise CHIFF (INSTR_NOISE A0 D26 R18, LP2600) · `chorus`(0.55/0.50/0.45) · `tape`(0.40/0.25/0.40) · `reverb`(0.55/0.45) | A faked tape "recording" of a string section — the drawn wave supplies the spectrum, the chiff/chorus/tape/reverb supply the recording cues. Held note force-released at the 2–8s TAPE length. |
| user0/mellotron-choir | mellotron.c | same chassis; wave = vocal "aah" (f + 2f·0.62 + 3f·0.89 + 4f·0.93 + 5f·0.49 + 6f·0.22, the formant bump on h3–h4) | Mellotron 8-voice choir. |
| user0/mellotron-flute | mellotron.c | same chassis; wave = near-pure (f + 2f·0.18 + 3f·0.08) | Mellotron flute (the Strawberry Fields timbre); breath is the chiff + lowpass. |
| user0/mellotron-brass | mellotron.c | same chassis; wave = bright 8-harmonic stack (the blare) | Mellotron brass section. |
| user0/mellotron-blend | mellotron.c | LAYERING — the active voices' tables are summed into one cycle and peak-normalized to 0.95, then `wave_set` (additive, still one note-voice). Presets: STR+CH, STR+FL, CHO+FL, FULL | A blended "tape frame" — stack timbres without multiplying polyphony. |

> **Cross-ref:** the radio's drawn-wave voices — roadhouse's `user0/combo-organ` and tango's
> `user0/bandoneon` — pull the same `wave_set` lever, hand-drawing a drawbar/free-reed cycle
> rather than using a modeled engine. stylophone.c is the playground that exposes the
> wave-drawing UI with seedable starting points.

## Raw waves (SQUARE / SAW / TRI / SINE / NOISE)

Recipes built straight on the raw oscillators — full ADSR + filter per voice, no macro
triple. This is where the **drum machines** live (each voice labeled by machine) alongside
the additive synths, whimsical toys, and live synth patches.

### tr808 (machine — all 16 voices, from tr808.c)

Voices use raw waves (SINE/NOISE/TRI/SQUARE), some layered. Per-voice TUNE/DECAY/character
knobs add live variation; 6 pattern presets (PLANET ROCK, HEALING, HOUSE, ELECTRO, BOOM BAP,
COWBELL JAM) are grids, not recipes.

| name | engine | recipe | character |
|---|---|---|---|
| 808/kick | SINE | 0/480/0/60 · LP 250/3 · pitch-env →+26 (0/50) · drive 0.28 | Boomy 808 sub kick, saturated low-end thump, ~50Hz. |
| 808/snare | SINE+NOISE | body 0/100/0/30 LP1200/1 + snappy 0/130/0/40 HP1800/2 | 180+330Hz sine modes + highpassed noise, tunable body↔snap. |
| 808/tom-lo/mid/hi | SINE+NOISE | sine 0/260/0/50 pitch-env →+6 (0/80) + thud 0/28/0/12 BP900/2 | Three toms (midi 40/45/52), sine pitch drop + optional noise thud. |
| 808/conga-lo/mid/hi | SINE | 0/150/0/30 · pitch-env →+4 (0/25) | Three congas (midi 52/57/63), clean sine drop, no noise. |
| 808/rimshot | TRI | 0/45/0/15 · HP500/3 (dual bridged-T 1667+455Hz) | Dual-tone rimshot, bright highpassed triangular punch. |
| 808/claves | TRI | 0/40/0/14 · LP4000/5 (2500Hz) | Sharp wooden clave ping, vocal hollow tone. |
| 808/clap | NOISE | 0/110/0/50 · BP1100/5 · 3× retrigger ~10ms + 140ms tail | Bandpassed noise with retrigger burst + reverb decay. |
| 808/maracas | NOISE | 0/24/0/10 · HP6500/2 | Bright quick highpassed maracas with a tiny body. |
| 808/cowbell | SQUARE | 0/250/0/60 · BP2640/5 (dual 540+800Hz, tone xfade) | The 2.6kHz bandpassed square pair, tunable emphasis. |
| 808/cymbal | SQUARE | 0/850/0/200 · HP3440/3 (3-osc bank 79/72/66Hz) | Bright metallic sustain, shimmering tail. |
| 808/hat-open | SQUARE | 0/340/0/90 · HP7000/3 (2-osc 79/72Hz) | Bright square hats, ~360ms sustain, warm↔bright knob. |
| 808/hat-closed | SQUARE | 0/42/0/16 · HP7000/3 (2-osc 79/72Hz) | Tight ~50ms snap, chokes the open hat. |

### morphdrum (morphing — 4 voices, from morphdrum.h)

The CONTINUUM between the fixed 808/909 recipes: each voice is one parametric model, its
`CHARACTER` knob morphing the 808↔909 *structure* while the rest are absolute knobs whose ranges
reach past both machines. Design: [`design/morphdrum.md`](../design/morphdrum.md); home cart =
`morphbox`. NOT `tr808.h`/`tr909.h` (fixed & faithful) and NOT `drumkit.h` (a pad map).

| name | engine | recipe (knob → range) | character |
|---|---|---|---|
| morph/kick | SINE + NOISE click + SINE sub | TUNE midi 19–52 · DEC 40–1100 · PUNCH pitch-env 0–48st · SNAP env 8–150ms · CUT LP 55–3900 · DRIVE 0–1 · CLICK 0–7 · SUB 0–7; CHAR → click HP 1500→3200 | 808 boom ↔ 909 punch, then sub-deep / endless / distorted beyond. |
| morph/snare | SINE body + NOISE | TUNE 46–74 · DEC 30–320 · PUNCH 0–12st · CUT LP 400–6400 · TONE body↔noise · NDEC noise 30–420; CHAR → noise HP 1800→1400 | 808 crisp ↔ 909 fat; TONE rides tom-thud ↔ white-crack. |
| morph/hat | FM (one metal voice, closed+open) | TUNE 80–110 · DEC closed 10–220 · OPEN 80–800 · CUT HP 3000–12000 · RES 0–12 · TONE=timbre · METL=morph; CHAR → harmonics 0.40→0.70 | One metal source, square-ish tick ↔ full clang; open hat choked by the next closed. |
| morph/pluck | PLUCK (melodic) | TUNE = minor-pentatonic over 3 oct (base 33) · OCT ±2 · DEC 60–1400 (=harmonics ring) · CUT LP 200–6400 · RES 0–10 · ATK 0–25ms · TONE=morph · CHAR → timbre 0.25→0.90 | The bass/lead voice; CHAR = dark round ↔ bright metallic. Pitch snaps to a scale so per-step TUNE p-locks make a musical line. |

### tr909 (machine — 11 voices, from tr909.c)

Mixed: kick/snare/toms are SINE+NOISE, hats/crash/ride are **FM** (`h0.55 t0.90 m0.85` metal
pad), clap/rim raw. Metal XY pad controls the FM slots' highpass + resonance; 6 pattern
presets (GOOD LIFE, THE BELLS, ENERGY FLASH, HARDFLOOR, REVOLUTION 909, GABBER) are grids.

| name | engine | recipe | character |
|---|---|---|---|
| tr909/kick | SINE+NOISE | 0/300/0/50 S0 · LP380/2 · pitch-env →+30 (0/35) · drive0.35 + click 0/9/0/4 HP2500/2 | Fast sine sweep punch, driven warehouse weight, highpass click. |
| tr909/snare | SINE+NOISE | 0/90/0/25 LP1400 pitch-env →4 (0/15) dual 185/330Hz + noise 0/170/0/50 HP1400/2 | Two-mode pitched snare (SNPY knob fades body↔noise). |
| tr909/ltom/mtom/htom | SINE+NOISE | 0/220/0/45 · pitch-env →+7 (0/60) + click 0/12/0/5 BP2000/3 | Three toms (midi ~40/47/54), pitch drop + woody attack click. |
| tr909/rimshot | TRI | 0/28/0/10 · HP400/3 (dual ping 76/64 midi) | Short woody dual ping — the Energy Flash tick. |
| tr909/clap | NOISE | 0/100/0/45 · BP1200/5 · 3× retrigger @9ms + room tail 0/130/0/45 | Triple-attack handclap + soft decay tail. |
| tr909/chh | FM | 0/40/0/16 · FM(h0.55 t0.90 m0.85) · HP8500/2 · cut-env 0/28/−5000 | Tight FM metal clang, fast-closing sizzle, chokes open hat. |
| tr909/ohh | FM | 0/380/0/110 · FM(h0.55 t0.90 m0.85) · HP8000/2 · cut-env 0/220/−4500 | Long-decay FM hat, inharmonic clang + sizzle, choked by closed. |
| tr909/crash | FM+NOISE | 0/1000/0/280 · FM(h0.55 t1.00 m0.90) HP4500/2 + noise 0/1200/0/320 | Long crash, FM metal body + noise wash, tone-blended. |
| tr909/ride | FM | 0/550/0/160 · FM(h0.55 t0.72 m0.58) · HP5000/2 (dual bell 76/83 midi) | Jeff Mills ringing bell, cleaner FM metal than crash. |

### tr606 (machine — 7 voices, from tr606.c)

All raw waves. The metal bank uses the circuit-analysis frequencies (246.4/308/367/418.2/
440.4/627.2 Hz = midi 59/63/66/68/69/75 — the 418+440 pair beats); kick is the schematic's
DOUBLE twin-T (TONE knob crossfades the two resonators). Open-hat decay is tempo-linked;
6 pattern presets (DRUMATIX, ACID PAIR, SKITTER, ELECTRO, TOM DANCE, SPARSE) are grids.

| name | engine | recipe | character |
|---|---|---|---|
| 606/kick | SINE×2 | 65Hz 0/140/0/40 LP300/2 pitch-env →+6 (0/14) + 87Hz 0/100/0/30 LP350/2, TONE xfade | Two twin-T resonators beating — the tight clicky "tock", no 808 boom. |
| 606/snare | SINE+NOISE | body 233Hz 0/85/0/25 LP1400/1 pitch-env →+3 (0/15) + snappy 0/115/0/35 HP2400/2 | ONE body mode (unlike the 808's two) under bright paper noise. |
| 606/tom-lo/hi | SINE+NOISE | 0/230/0/45 pitch-env →+5 (0/60) (midi 45/52) + rumble 0/24/0/10 LP400/2 | Sweet analog toms + the documented LOW-passed rumble burst. |
| 606/cymbal | SQUARE×4 | body midi 63+75 on 0/780/0/200 HP3000/3 + shimmer midi 69+75 on 0/460/0/140 HP7000/3 | The dual-path (3440/7100Hz) cymbal: clangy midband body + fizzy top, ~650ms. |
| 606/hat-open | SQUARE | 0/320/0/80 · HP7000/3 (the beating 418+440Hz pair, midi 68+69) | Jewel-box sizzle; decay TEMPO-LINKED like the hardware (~3 steps). |
| 606/hat-closed | SQUARE | 0/38/0/13 · HP7000/3 (midi 69+75) | ~40ms tick; fires the shut-off circuit (chokes the open hat). |

### cr78 (machine — 16 voices, from cr78.c)

All raw waves; some voices fired multiple times detuned for beating/layering.

| name | engine | recipe | character |
|---|---|---|---|
| cr78/kick | SINE | A0 D170 S0 R40 · LP320/2 · pitch-env →+13 (0/45) | Soft analog sine, gentle pitch thump. |
| cr78/snare-body | SINE | A0 D70 S0 R25 · LP900/1 · pitch-env →+5 (0/25) | Short damped sine, woody pitch blip — snare shell. |
| cr78/snare-rattle | NOISE | A0 D130 S0 R35 · BP1700/3 · cut-env →+900 (0/90) | Bandpassed noise that opens then closes — snare rattle. |
| cr78/rim | TRI | A0 D28 S0 R12 · BP1700/9 | Resonant triangle through tight bandpass — woody tick. |
| cr78/hat-closed | NOISE | A0 D40 S0 R18 · BP7800/4 | Gentle swishy noise — the CR-78 signature. |
| cr78/hat-open | NOISE | A0 D320 S0 R80 · BP7200/4 | Long sustained swishy hat, same filter, longer decay. |
| cr78/cymbal-tone | SQUARE | A0 D450 S0 R120 · HP5200/5 | Highpassed square cluster — detuned cymbal attack. |
| cr78/cymbal-noise | NOISE | A0 D550 S0 R150 · HP6500/2 | Highpassed noise shimmer layered for brightness. |
| cr78/tambourine | NOISE | A0 D90 S0 R30 · BP8400/10 | Narrow resonant noise — tambourine jingle. |
| cr78/cowbell | SQUARE | A0 D220 S0 R60 · BP2600/5 (fired 2× detuned 1.4:1) | Bandpassed square, beating from the detuned pair. |
| cr78/claves | TRI | A0 D36 S0 R14 · LP3600/6 | High triangle ping through lowpass — pure woody click. |
| cr78/maracas | NOISE | A0 D26 S0 R12 · HP6000/2 | Shortest noise burst in the box — shaker texture. |
| cr78/bongo | SINE | A0 D110 S0 R30 · pitch-env →+3 (0/18) | Bare damped sine at membrane pitch, gentle drop. |
| cr78/conga | SINE | A0 D210 S0 R50 · pitch-env →+3 (0/22) | Longer damped sine, subtle pitch drop. |
| cr78/guiro | NOISE | A0 D260 S0 R60 · BP2800/7 · cut-env →+1400 · LFO_VOL 36Hz/1.0 | Ratcheting scrape — bandpassed noise chopped by a 36Hz LFO. |
| cr78/metal-beat | SQUARE | A0 D180 S0 R60 · BP3300/6 (3 detuned squares) | Enharmonic clang — Kraftwerkian metallic beat. |

> **Cross-ref:** cr78.c is a confirmed showcase-cart lineage — ymo lifts the *whole* CR-78
> kit (`kit/cr78`) into its drum slots, the first time a station borrows an entire kit
> rather than one voice (see [`instrument-presets.md` → kit/cr78](instrument-presets.md)).

### drummachine (machine — 6 voices, from drummachine.c)

| name | engine | recipe | character |
|---|---|---|---|
| drummachine/kick | SINE | A0 D280 S0 R60 · pitch-env →+30 (0/55) | 909-style boom, upward pitch punch, long tail. |
| drummachine/snare | NOISE | A0 D130 S0 R50 · BP1400/3 | Noise bark filtered to a tonal snare crack. |
| drummachine/closed_hat | NOISE | A0 D25 S0 R15 · HP6500/2 | Bright crisp highpassed tick. |
| drummachine/open_hat | NOISE | A0 D180 S0 R80 · HP6000/2 | Same sizzle, longer sustain. |
| drummachine/clap | NOISE | A0 D60 S0 R30 · BP1100/5 · 3× @12/24ms | Bursting handclap from three echoed noise bursts. |
| drummachine/bass | SQUARE | A1 D100 S3 R60 · LP900/4 | Rounded square bass, warm lowpass color. |

### tracker (machine — 8 presets, from tracker.c)

The tracker's fixed instrument bank (the Song→Chain→Phrase tracker cart — one preset per
inst-column digit 0–7). Drums are the drummachine kit reused on purpose.

| name | engine | recipe | character |
|---|---|---|---|
| tracker/lead | SQUARE | A2 D120 S5 R90 · duty 0.25 · LP2600/2 | Hollow chip-pulse lead, top tamed by the lowpass. |
| tracker/bass | SQUARE | A1 D100 S4 R60 · LP700/5 | The `drummachine/bass` shape with a darker cutoff — round sub. |
| tracker/bell | FM | A1 D400 S0 R300 | Stock 2-op FM bell, long ring. |
| tracker/pluck | PLUCK | A1 D200 S0 R150 | Dry Karplus-Strong string. |
| tracker/pad | SAW | A180 D300 S5 R400 · LP1200/2 · LFO_CUT 0.7Hz/500 | Slow saw wash, filter breathing. |
| tracker/kick | SINE | — | The `drummachine/kick` recipe verbatim. |
| tracker/snare | NOISE | — | The `drummachine/snare` recipe verbatim. |
| tracker/hat | NOISE | — | The `drummachine/closed_hat` recipe verbatim. |

### onenote (machine — 3 voices, from onenote.c)

| name | engine | recipe | character |
|---|---|---|---|
| onenote/kick | SINE | A0 D280 S0 R60 · pitch-env →+30 (0/55) | The `drummachine/kick` recipe — 909-style boom. |
| onenote/snare | NOISE | A0 D130 S0 R50 · BP1400/3 | The `drummachine/snare` recipe — bandpassed bark. |
| onenote/bass | SAW | A0 D210 S2 R140 · drive0.22 · LADDER (cutoff 120Hz–5kHz / res 0–12 = the XY pad, live) | The one-note funk bass: plucky driven saw through the Moog 4-pole ladder, tone shaped live by the XY pad (right = brighter, up = quackier). Plays a single pitch; the keybed transposes the whole loop. |

### grenadier (machine — 3 filterbank voices, from grenadier.c)

The Grendel RA-99 triple filterbank, cart-side: three held drone voices on one root, each
through its own resonant filter, summed; the cutoffs are ridden live from an Alpha/Beta XY pad.

| name | engine | recipe | character |
|---|---|---|---|
| grenadier/bank | USER0 (trapezoid) | A8 D0 S7 R240 · drive0.35 · ×3 voices, same root, held | three voices summed; the trapezoid VCO morphs triangle→square via `wave_set` (MORPH). The oscillator feeding the bank. |
| grenadier/filter-LP | — | `FILTER_LOW`, cutoff = `base·2^((α−.5)·4)`, res = 1..14 (Q) | voice 1 in RA-99 layout: the lowpass leg of the bank, swept by Alpha. |
| grenadier/filter-BP×2 | — | `FILTER_BAND` at `base·space` and `base·space²`, res = Q | voices 2 & 3: the two bandpass legs; Beta opens the spacing tight→wide. RA-9 layout flips voice 1 to BP too. |

### onenote — bass tone (the v2 XY pad), see the row above

> `onenote/bass` (above) is a `SAW` through `FILTER_LADDER`, cutoff 120 Hz–5 kHz / res 0–12 ridden
> live from the cart's XY pad. The kick/snare reuse the `drummachine` recipes.

### kaoss (machine — reuses the groovebox loop, from kaoss.c)

> Kaoss has **no new voice recipes** — its built-in loop reuses the `groovebox`/`drummachine` kit
> (kick/clap/chat/ohat `INSTR_NOISE`+`INSTR_SINE`, bass + arp `INSTR_SAW`). The cart's whole point
> is the **master effects** the XY pad rides (filter / varispeed / echo / tremolo) — see those rows in
> [`effects-recipes.md`](effects-recipes.md).

### euclid (machine — 5 Euclidean drum rings, from euclid.c)

| recipe | engine | params | notes |
|---|---|---|---|
| euclid/kick | SINE | A0 D280 S0 R60 · pitch-env →+30 (0/55) · drive0.26 | The `drummachine/kick` boom (as `onenote/kick`), a touch of drive. |
| euclid/snare | NOISE | A0 D130 S0 R50 · BP1400/3 | The `onenote/snare` bandpassed bark + an `INSTR_TRI` body blip. |
| euclid/chat | NOISE | A0 D24 S0 R14 · HP7000/2 | Closed-hat tick (the `kaoss`/groovebox closed hat). |
| euclid/ohat | NOISE | A0 D180 S0 R90 · HP6500/2 | Open-hat sizzle (the `kaoss`/groovebox open hat). |
| euclid/perc | TRI | A0 D55 S0 R30 · BP2200/4 @ MIDI79 | A bright wooden clave "tok" — the one new voice; gives the inner 3-in-8 tresillo ring its woodblock. |

> No new *engine* work: the rhythm comes from the `euclid(hits, steps, b)` Bjorklund helper, one
> call per ring per step, with a per-track step count (polymeter). The voices are the standard
> synth-drum kit; only the clave perc is bespoke.

### circlemachine (instrument — 3 voices on a ring, from circlemachine.c)

| recipe | engine | params | notes |
|---|---|---|---|
| circlemachine/ring | SQUARE | A1 D150 S0 R40 · LP3200/2 | The stepped electronic bleep — one clean square per lit bulb, softened by a lowpass. Default voice; bulb brightness = scale degree. |
| circlemachine/glide | SINE | A8 D260 S6 R140 · `note_glide` 90ms | The Clavivox — ONE held sine that `note_pitch`-slides between lit steps (portamento); a rest drops it to vol 0. |
| circlemachine/bongo | MEMBRANE | A0 D90 S0 R40 | The Bandito auto-drummer — a pitched electronic bongo per step. |

> No new *engine* work — the character is the RIG, not the patch: two rings on independent phase
> accumulators (HUMAN wobble + DRIFT de-sync + SYNC re-lock), a DIRT bus (`tape` + `chorus` +
> `amp_noise` + `crush`), `ringmod` clang, and reverb/echo **SENDS** (`instrument_reverb` /
> `instrument_echo` — configuring the tank alone is silent). Design + the send gotcha:
> [`../design/scott-blind-brief.md`](../design/scott-blind-brief.md).

### turing (machine — 1 sequence voice + 2 drums, from turing.c)

| recipe | engine | params | notes |
|---|---|---|---|
| turing/seq | SAW | A0 D150 S0 R~eighth · LP1700/6 · drive0.15 | The Berlin-school sequence bleep — a resonant lowpass saw, staccato; pitch is the shift-register readout (scale-locked, 1–3 octaves via RANGE). |
| turing/kick | SINE | A0 D250 S0 R60 · pitch-env →+30 (0/55) · drive0.26 | The `drummachine/kick` boom, fired when register bit 0 is set. |
| turing/hat | NOISE | A0 D28 S0 R16 · HP7000/2 | Closed-hat tick, fired when register bit 2 is set. |

> No new *engine* work: the rhythm/melody is a cart-side 16-bit shift register (rotate + flip the
> recirculated bit with a probability = the CHAOS knob), read out as a scale degree. The voices are
> the standard sequence/drum kit; the generativity is all in the register, not the patch.

### pocketbox (machine — 8 track-role engine MENUS, from pocketbox.c)

Not fixed patches: each of the 8 track roles carries a small **engine menu** whose 4 macro params
are the player's (and the p-locks') surface, so every entry is a *family* of sounds around the
default below. The drum defaults descend from the `drummachine`/`tr808`/`tr909` recipes above.

| recipe | engine | params (at defaults) | notes |
|---|---|---|---|
| pocketbox/909-kick | SINE+NOISE | 0/290/0/60 · pitch-env →+29 (0/55) · click layer 0/16/0/10 | The `drummachine/kick` boom + a p-lockable click layer. TUNE/DEC/PNCH/CLIK. |
| pocketbox/808-kick | SINE | 0/580/0/120 · LP 1060/0 · pitch-env →+14 (0/80) | Longer, rounder, TONE = lowpass. |
| pocketbox/drumhead | MEMBRANE | 0/300/0/80 · h0.4 t0.5 | The modeled head — TUNE/DEC/MATL/STIK are the engine macros. |
| pocketbox/909-snr | NOISE+TRI | band 1470/3 noise bark + TRI body 0/45/0/30 | BODY mixes the tonal layer; TUNE moves it. |
| pocketbox/clap | NOISE | band 1150/5 · 3 bursts 8–24ms apart | SPRD = burst spacing, TAIL = last-burst ring. |
| pocketbox/808-hat | NOISE | 0/44/0/12 · HP 6800/1 | DEC p-locks make OPEN hats on the same track (the demo groove does this). |
| pocketbox/fm-hat | FM | 0/54/0/16 · h0.75 t0.75 | The metallic 909-style clang from FM's off-integer ratios. |
| pocketbox/acid | SAW | 0/186/2/60 · FILTER_DIODE 1020/8 · cutoff-env →+1440 (0/126) | The `tb303` squelch as a bass-menu entry: CUT/RES/ENV/DEC. |
| pocketbox/wave | USER0..3 | 0/305/2/60 · LP 3800/2 · pitch-env punch | Plays the four **finger-drawn** single-cycle waves; a p-lock can't swap the wave but the engine pick can (WAVE A–D). |

> Lead/chord menus (PD LEAD / CHIP / PLUCK / FM BELL · SAW PAD / E.PIANO / ORGAN) are thin
> macro wrappers over the engine showcases — see those engines' sections above for the timbre space.

### dubsiren (machine — a 2-voice siren, from dubsiren.c)

| recipe | engine | params | notes |
|---|---|---|---|
| dubsiren/siren | SQUARE | A1 D0 S9 R220 · drive0.12 · held, `note_lfo(LFO_PITCH)` 0.5–20Hz × 0–12st | The siren tone — a sustaining square warbled by the pitch LFO (shape = whoop/two-tone/ramp/zigzag); pitch ridden live from the FIRE PAD's Y. |
| dubsiren/body | SAW | A1 D0 S9 R220 · held, detuned +0.09st, same LFO | A detuned saw under the square for beating/fatness. |

> Both voices are `note_on`-held and start at vol 0 (silent until the pad fires). Their character is
> mostly the **effects**, not the patch: `echo()` feedback throw (ride past 1.0 = self-osc howl) +
> `reverb()` splash — see [`effects-recipes.md`](effects-recipes.md). **Set `instrument_echo`/
> `instrument_reverb` BEFORE `note_on`** — the sends are sampled at trigger, so a held voice set after
> never feeds the buses.

### lpg (machine — 8 west-coast marimba bars, from lpg.c)

| recipe | engine | params | notes |
|---|---|---|---|
| lpg/bar | TRI | A4 D0 S9 R90 · FILTER_LOW 300–8000 (rides env) · DRIVE_FOLD 0–`fold`×env · held | The low-pass-gate voice: a sustaining triangle whose VCA *and* lowpass close together from one vactrol envelope (note_vol = env, note_cutoff = 300 + 7700·env^1.6 so highs die first), with a wavefolder (note_drive = fold·env) blooming metallic on the strike and settling to a pure tone. 8 voices = a C-pentatonic, 2-octave marimba. |

> The instrument is the **coupling**, not the patch. COUPLE morphs note_cutoff from "always open"
> (a plain VCA — tone stays bright as it fades) to "tracks the envelope" (the organic plonk). All
> per-voice (`note_vol`/`note_cutoff`/`note_drive` on `note_on` handles), so it's polyphonic on one slot.

### easel (machine — a west-coast voice, from easel.c)

| recipe | engine | params | notes |
|---|---|---|---|
| easel/voice | FM | A2 D0 S9 R120 · FILTER_LOW 300–8000 (gate) · DRIVE_FOLD 0–`fold`×gate · held · `note_harmonics`=RATIO, `note_timbre`=cross-mod | The Buchla complex-osc voice: an FM tone (ratio + index = west-coast cross-mod, ridden live) through a wavefolder, gated by a low-pass gate (note_vol + note_cutoff coupled, `cut = 300 + 7700·g^1.6`). The function generator (rise/fall, one-shot/cycle) or finger pressure drives the gate level `g`. |

> Like `lpg`, the instrument is the **gate coupling** + the additive timbre, not a fixed patch — RATIO/
> TIMBRE/FOLD and the function generator are all played live. Far brighter to gate than lpg's triangle
> because the FM osc + folder make rich harmonics for the lowpass to remove.

### loopstation (synth — 7 fixed tracks, from loopstation.c)

| name | engine | recipe | character |
|---|---|---|---|
| sine/kick | SINE | A0 D130 S0 R60 · pitch-env 0→90→26ms | Punchy kick, pitch sweep sharp→thump. |
| noise/snare | NOISE | A0 D90 S0 R80 · BP1800/4 | Snappy snare, tight bandpass mid-range. |
| noise/hat | NOISE | A0 D25 S0 R30 · HP6500/6 | Crisp closed hat, bright highpass sizzle. |
| saw/bass | SAW | A2 D160 S4 R120 · LP750/7 · cut-env 0→140→1400ms | Fat plucky bass, filter envelope sweeps closed. |
| pulse/lead | SQUARE | A4 D120 S5 R160 · duty0.25 · LFO_PITCH 5.5Hz/0.25 | Thin vibrato lead, tight pulse width. |
| sine/theremin | SINE | A40 D100 S7 R200 · LFO_PITCH 5.5Hz/0.3 · glide30ms | Warm theremin swell, sustained sine, vibrato (live + ghost voices). |
| tri/click | TRI | A0 D30 S0 R20 | Dry triangle metronome click. |

### omnichord (whimsical — harp + 6 drums, from omnichord.c)

The harp is the only explicit `instrument()`; the 6 drum rows use `hit()` convenience calls
(decay-only, no ADSR).

| name | engine | recipe | character |
|---|---|---|---|
| harp/sonic-strings | TRI | A1 D180 S1 R280 · LP2200/4 | Soft plucked bell tone for strumplate glissandos — the melodic voice. |
| drum/kick | TRI | D100ms · pitch 36 (C1) | Deep triangular kick. |
| drum/snare | NOISE | D120ms · pitch 55 | Bright noise snare. |
| drum/hihat | NOISE | D28ms · pitch 84 | Crisp closed hi-hat. |
| drum/ohat | NOISE | D170ms · pitch 84 | Open hi-hat, long ring (same pitch as closed). |
| drum/clap | NOISE | D60ms · pitch 64 | Noise handclap. |
| drum/bass | SQUARE | D110ms · pitch 36+root | Square bass following the selected chord root. |

### fingerdrums (whimsical — play-by-hand kit, FIVE kits, from fingerdrums.c)

Five kits cycled with TAB, each a matched look + sound. A shared 12-slot pool is
re-voiced per kit on switch; voices fire via `schedule_hit` (each role gets its own gate)
and velocity (1..7) comes from vertical hit position. The two ACOUSTIC-style kits share a
perspective layout, the two DIGITAL kits share a pad grid, PIKUNIKU has its own canvas.

**ACOUSTIC** — raw-wave + one modeled `INSTR_MEMBRANE` voice (the toms, `membrane/tom-kit`
above):

| name | engine | recipe | character |
|---|---|---|---|
| acoustic/kick | SINE | A0 D300 S0 R80 · LP220/2 · pitch-env →+28 (0/55) · drive 0.15 | Punchy acoustic kick, short pitch drop, lightly saturated. |
| acoustic/snare-body | SINE | A0 D90 S0 R40 · LP1500/1 · pitch-env →+4 (0/18) | The drum tone under the wires — a tuned thwack. |
| acoustic/snare-snap | NOISE | A0 D120 S0 R60 · BP1900/3 | Bandpassed noise crack — the snare wires (layered over the body). |
| acoustic/hat-closed | NOISE | A0 D45 S0 R25 · HP8500/2 | Tight highpassed tick; chokes the open hat. |
| acoustic/hat-open | NOISE | A0 D60 S2 R220 · HP8200/2 | Sustain>0 so it washes; killed by a closed-hat hit. |
| acoustic/crash | NOISE | A0 D220 S0 R600 · HP5000/1 | Long bright noise wash — the crash cymbal. |
| acoustic/ride | NOISE | A0 D90 S0 R300 · BP7000/3 | Bandpassed noise ping — a ride bell, shorter than the crash. |

**MPC PADS** — 808-style; reuses the **tr808** recipes above (boom kick, sine+noise snare,
retrigger clap, square cowbell). The only divergence is its hats/cymbals, a simpler
NOISE-highpass tick rather than tr808's 3-oscillator square bank.

**PIKUNIKU** — bouncy toy/chiptune voices (genuinely new):

| name | engine | recipe | character |
|---|---|---|---|
| piku/boop-kick | SINE | A0 D190 S0 R70 · LP600/1 · pitch-env →+10 (0/70) | Soft round low "boop" — a cushioned, bouncy kick. |
| piku/bop-snare | TRI | A0 D80 S0 R50 · LP2200/2 · pitch-env →+8 (0/30) | A pitched triangle "bop" — single-layer toy snare. |
| piku/tik-hat | NOISE | A0 D28 S0 R18 · HP7000/1 | A tiny soft tick. |
| piku/boop-tom | TRI | A0 D170 S0 R90 · LP2600/1 · pitch-env →+4 (0/55) | Melodic bouncy boops, played at pitches so you can plink a tune. |
| piku/pop | NOISE | A0 D80 S0 R50 · BP1500/4 | A soft round "pop" clap. |
| piku/ding-sparkle | MALLET | h0.92 t0.6 m0.42 · ring~400ms | An `INSTR_MALLET` glockenspiel chime — the cute melodic accent. |

Master FX: a gentle `chorus(1.3, 0.25, 0.22)` for playful width.

**MAC DEMARCO** (acoustic) — the perspective kit voiced soft & dead (towel kick LP150,
mellow snare, dark HP6500 hats, warm `membrane/tom-kit` at +0.06 semitone detune), then the
whole mix through the woze: `tape(0.45, 0.30, 0.45)` + `chorus(0.8, 0.35, 0.30)` — the
bedroom-tape wow/flutter/warble that IS the sound.

**MAC - TAPE** (digital) — a lo-fi drum-machine version (warm pitched-down SINE kick, soft
synth snare/clap, gentle hats, pitched SINE toms), same woze plus cassette grit:
`tape(0.50, 0.35, 0.50)` + `chorus(0.9, 0.40, 0.35)` + `crush(7.5, 0.6, 0.35)`.

### mariopaint (whimsical — 6 melodic voice stamps, from mariopaint.c)

| name | engine | recipe | character |
|---|---|---|---|
| tri/meow | TRI | 2/120/2/200 · LFO_PITCH 6Hz/0.4 | Vibrato meow with wobble (CAT). |
| sine/bell | SINE | 1/240/0/340 | Bright bell / glock attack (STAR). |
| square/tuba | SQUARE | 2/90/0/90 · duty0.5 · LP700/3 | Warm mellow tuba (MUSH). |
| square/lead | SQUARE | 3/60/5/120 · duty0.15 | Thin bright lead (NOTE). |
| sine/swell | SINE | 70/0/7/380 | Slow-attack soft swell (HEART). |
| saw/harp | SAW | 2/110/1/150 · LP1700/5 | Warm plucky harp (FLWR). |

### mt70 (showcase — Casiotone MT-70, additive SINE stacks, from mt70.c)

Two/three SINE oscillators per key, each with its own decay; some add a MALLET attack or
NOISE click. All use `INSTR_SINE` as the base voice.

| name | engine | recipe | character |
|---|---|---|---|
| sine/piano | SINE | A2 D800 R300 S0 · LP2400 | Plain sine decay — the dullest piano ever. |
| sine/organ | SINE | A10 D100 R80 S6 · LP3400 · o2@19.02st sustains | Fundamental + sustained 3rd harmonic. |
| sine/flute | SINE | A50 D100 R120 S5 · LP2850 · o2@12st decay830ms | Octave overtone fades to a pure sine. |
| sine/vibes | SINE | A2 D1200 R400 S0 · LP4000 · o2@24st decay500ms · mallet(0.25/0.50/0.90) | 4:1 partial decays first — realistic vibraphone. |
| sine/chime | SINE | A1 D1500 R500 S0 · LP5700 · o2@5.01st · mallet(0.80/0.60/0.55) | Two tones a 4th apart, same decay. |
| sine/bells | SINE | A1 D1000 R600 S0 · LP6800 · o2@24st + o3@12st · mallet(0.90/0.70/0.60) | Three sines at +0/+2/+1 octaves — bell timbre. |
| sine/cosmic | SINE | A2 D150 R150 S4 · LP3400 · o2@12st · click(NOISE HP5000) | Click then high octave fades to medium. |
| sine/jzorg | SINE | A2 D100 R60 S5 · LP2850 · o2@19.02st · click(NOISE HP5000) | Hammond-like with percussive key click. |
| sine/celsta | SINE | A1 D600 R300 S0 · LP5700 · o2@19.02st · mallet(0.50/0.55/0.45) | Music box; bass resembles a steel drum. |
| sine/banjo | SINE | A1 D400 R150 S0 · LP3400 · o2@12st + o3@19.02st · mallet(0.10/0.90/0.10) | Hollow and resonant, fast-fading octave + 5th. |

### tb303 (synth — one voice, two wave variants, from tb303.c; ×2 in acidrack.c)

> **The filter is `FILTER_DIODE` since 2026-07-02** (audio-notes §25): a real diode-ladder
> model — ~18dB/oct, bass drains as resonance climbs, saturation INSIDE the loop so it
> growls. Older acid recipes on `FILTER_LOW` (braindance, squarepusher) still work but are
> the clean-SVF approximation — upgrading them is a one-constant swap worth taking when
> touching those carts.

| name | engine | recipe | character |
|---|---|---|---|
| saw/303 | SAW | 2/60/25 S6 · FILTER_DIODE 60–3840Hz/0–15Q · ENV_CUTOFF 0–3000Hz · drive · 0.48 duty · echo | Acid house — saw through the diode ladder, live envelope + drive. The classic TB-303 squelch. |
| square/303 | SQUARE | 2/60/25 S6 · FILTER_DIODE 60–3840Hz/0–15Q · ENV_CUTOFF 0–3000Hz · drive · echo | Same path, square wave — brighter, more digital bite. |

### juno (poly synth — saw+sub+resonant-LP, the chorus IS the patch, from juno.c)

| name | engine | recipe | character |
|---|---|---|---|
| saw/juno | SAW | A24 D0 S6 R460 · FILTER_LOW 200–6000Hz / 0–12Q · sub = root −12 at lower vol · **`chorus(rate,depth,mix)` master BBD** (OFF / I 0.1Hz·0.45 / II 0.22Hz·0.62) | The Roland Juno-6. The dry voice is a plain saw+sub poly synth ("sh101 with more voices"); the **master chorus** is what makes it a Juno — flip I/II and a centered mono chord fans into a lush wide stereo wash (antiphase taps). Chorus is master-wide (not per-slot). |

### solina (string machine — 6 stacked-saw footage tabs, the chorus IS the patch, from solina.c)

The divide-down sibling to juno: where juno's chorus widens *one* voice, solina builds the timbre from **one SAW slot per footage tab** (all firing per held key = the divide-down mix), then the ensemble chorus is its whole identity. Each tab = the same swell envelope at a different octave + cutoff.

| name | engine | recipe | character |
|---|---|---|---|
| saw/solina-tab | SAW | A60–1400 D300 S6 R300–3500 (CRESC/SUST live) · FILTER_LOW (cut×0.5–1.6 TONE)/2 · `instrument_tune` 0.05–0.08 (per-tab detune spread) · LFO_PITCH 0.16Hz/0.04 (tape wow) · brass tabs `instrument_drive` 0.20 | One Solina voice tab. Six slots stacked by footage: Contrabass 16′ (−12, cut 900) · Cello 8′ (0, 1500) · Viola 8′ (0, 2400) · Violin 4′ (+12, 3600) · Trumpet 8′ (brass, 3200) · Horn 4′ (brass, +12, 2600). Bare = a buzzy divide-down organ. |
| (ensemble) | — | **`chorus(rate,depth,mix)` master BBD** — OFF / I 0.9Hz·0.50·0.60 / II 1.6Hz·0.70·0.72 | The triple-BBD string wash. Run lusher/deeper than juno's — it's the entire point of the Solina; off, the stacked saws are static. |

### Whimsical & single-voice synths

| name | source cart | engine | recipe | character |
|---|---|---|---|---|
| pulse/square | stylophone.c | SQUARE | A6 D130 S6 R110 · LP2600/3 · duty0.10–0.50 (TONE) | Buzzy PWM oscillator, nasal→fat sweep. |
| otamatone/reed | otamatone | SQUARE | A8 D90 S6 R130 · duty12% · LP500/9 · LFO_PITCH 6Hz/0.22 | Thin nasal square reed, mouth sweeps the filter live. |
| saw/mouth-harp | mouthharp | SAW | A2 D0 S7 R500 · LP_BAND 700/14 | Buzzy fixed-pitch drone shaped by a dynamic formant (vowel) sweep. |
| noise/tongue-tick | mouthharp | NOISE | A0 D25 S0 R28 | Percussive tongue strike on pluck attack. |
| sine/glass | glassharmonica.c | SINE | A220 D200 S7 R900 · LFO_PITCH 5Hz/0.12 · LFO_VOLUME 0.7Hz/0.18 | Pure sine, faint shimmer + slow breathing tremolo — wet glass. |
| saw/drone-tonic | hurdygurdy | SAW | 140/300/500 S7 · LP600/4 | Warm reedy saw drone on C2 (tonic), crank-controlled. |
| saw/drone-fifth | hurdygurdy | SAW | 140/300/500 S7 · LP600/4 (note_on 43) | Same drone voiced on G2 (fifth). |
| saw/melody-vibrato | hurdygurdy | SAW | 40/200/220 S7 · LP1400/6 · LFO_PITCH 5.5Hz/0.15 | Bright reedy saw with fiddle vibrato — the melody line. |
| sine/bow | musicalsaw | SINE | A120 D200 S7 R600 · LFO_VOLUME 5.2Hz/0.22 | Eerie bowed sine, shimmering vibrato, live wobble-modulated. |
| saw/theremin | heldnotes.c | SAW | A40 D120 S6 R350 · LP1600/11 · LFO_PITCH 5Hz/0.18 | Warm sawtooth pad, resonant lowpass + gentle vibrato. |
| saw/synth-piano | touchpiano | SAW | A8 D140 S6 R260 | Snappy sawtooth synth-piano, quick attack, moderate sustain. |
| voice/formant | vox.c | VOICE | (listed under INSTR_VOICE) | — |

### fartsynth (whimsical — 6 presets, from fartsynth.c)

Synthesis via rapid `note()` hits (every 2 frames) with linear pitch bend + sinusoidal
wobble; no `instrument()` ADSR. Volume fades over the last 25%.

| name | engine | recipe | character |
|---|---|---|---|
| fart/rumbler | SAW | pitch28 len800ms bend−10 wobble3 vol6 | Deep bass fart, downward slide, subtle vibrato. |
| fart/squeaker | SQUARE | pitch60 len250ms bend+6 wobble8 vol5 | High squeaky fart, upward slide, aggressive wobble. |
| fart/trumpet | SAW | pitch44 len500ms bend0 wobble6 vol6 | Mid-range trumpet-ish fart, steady pitch. |
| fart/wetone | NOISE | pitch32 len600ms bend−8 wobble10 vol6 | Noisy wet fart, downward bend, intense wobble. |
| fart/silentbutdeadly | SINE | pitch30 len1000ms bend−4 wobble1 vol2 | Long sine fart, minimal slide, barely audible. |
| fart/machinegun | SQUARE | pitch40 len450ms bend−2 wobble12 vol7 | Rapid-fire square fart, heavy vibrato, max volume. |

> **Cross-ref (raw-wave radio voices):** the dance/groove stations' synth kits and basses
> live on these same raw waves — see [`instrument-presets.md`](instrument-presets.md) for the
> demand side (`drum/french-house-kick`, `tri/disco-bass`, `square/da-funk-lead`, the
> PWM-square lead family, etc.). The drum machines above (808/909/CR-78/drummachine) are the
> *machine* sources those station kits echo.

### modrack (modular patcher — presets are module graphs, from modrack.c)

modrack's presets wire `MOD_*` modules (CLOCK/TURING/GRIDS/MARBLES/TIDES/VOICE/MACRO/SAT…)
into self-running patches, so most are **module wiring, not timbres** and carry no nameable
voice recipe (see "By cart" below). The two **Raymond Scott homages** are the exception —
each builds a distinctive voice worth naming. Both run dirty through `MOD_SAT` (`DRIVE_ASYM`
tube glue, mix 0.8) for the analog character.

| name | engine | recipe | character |
|---|---|---|---|
| modrack/electronium-voice | VOICE (resonant saw) | cut 480 · res 8 · saw · fenv +900 · free-running TIDES (0.13Hz) → cutoff · over a penta-min TURING melody (rnd 0.35, 12-step) · 104 BPM swing 0.10 | The self-composing-machine lead: a resonant saw whose filter slowly morphs under a free-run TIDES LFO so it never quite repeats. MARBLES adds shaped randomness (dens 0.55 / spread 0.6). |
| modrack/bandito-bongos | MEMBRANE (MACRO eng 6) | h0.5 t0.6 m0.1 drive0.2 — struck head between tabla/djembe, edge slap, ~flat | "Bandito the Bongo Artist" — *real* struck-membrane bongos (replacing an earlier MALLET stand-in) fired off the GRIDS snare pattern, over a VOICE pluck bass (cut 350 / res 6 / saw / fenv +1000, an octave down via XPOSE). 100 BPM swing 0.12. |

> **Cross-ref:** the patches themselves (and the rest of the 30+ presets, which are pure
> module wiring) are logged in [`STATUS.md`](../STATUS.md); only these two carry a timbre
> recipe reusable outside modrack.

---

## By cart

The alternate view — each cart and the recipe names it stocks. Carts with no fixed recipes
(pure live/sequenced patching, or audition tools that modulate one base voice) are noted.

**Showcase carts (the modeled-engine reference rigs):**

- **pluck.c** → pluck/default
- **guitar.c** → guitar/harp · /nylon · /steel · /banjo · /koto · /uke · /pizz · /oud
- **mallet.c** → mallet/marimba · /xylophone · /celesta · /vibraphone · /glockenspiel
- **fm.c** → fm/epiano · /bell · /bass · /brass · /clang
- **epiano** → epiano/rhodes · /rho-brite · /suitcase · /wurli · /wur-buzz · /clav
- **organ.c** → organ/reggae · /combo · /bookerT · /jimmy · /larry · /ballad · /jonlord · /gospel · leslie/off · /slow · /fast
- **pd.c** → pd/cz-bass · /reso-lead · /synth-brass · /sweep-pad · /cz-pluck
- **pdbass.c** → pd/pdbass (the upright's fingerboard on INSTR_PD — an oscillator that glides both ways, so smooth continuous up/down slides + a live TIMBRE distortion slider; the synth-voiced sibling of `upright`)
- **tabla.c** → membrane/tabla · /conga · /bongo · /djembe · /tom
- **reed.c** → reed/clarinet · /sop_sax · /alto_sax · /tenor_sax · /oboe
- **pipe.c** → pipe/flute · /recorder · /pan-pipe · /piccolo · /breathy
- **bowed.c** → bowed/violin · /viola · /cello · /ponticello · /tasto · /tremolo · /pizzicato
- **upright.c** → bowed/upright-pizz · bowed/upright-arco · membrane/upright-body (jazz double bass — pizz + arco are the SAME INSTR_BOWED string via eng_tune(0,1/0); left/right walks frets (re-articulate), pull-up = continuous bend — a waveguide bends up but not down; the wood body is MEMBRANE percussion)
- **piano.c** → piano/grand · /bright · /harpsichord · /dulcimer · /clavichord · /celesta
- **voxlab.c** → voice/gen · /mouth · /creat · /sing
- **mt70.c** → sine/piano · /organ · /flute · /vibes · /chime · /bells · /cosmic · /jzorg · /celsta · /banjo · mallet/struck
- **spacecho** → pluck/guitar (the cart is a tape-echo effect showcase — one fixed voice)

**Roland / classic machines:**

- **tr808.c** → 808/kick · /snare · /tom-lo/mid/hi · /conga-lo/mid/hi · /rimshot · /claves · /clap · /maracas · /cowbell · /cymbal · /hat-open · /hat-closed
- **tr909.c** → tr909/kick · /snare · /ltom/mtom/htom · /rimshot · /clap · /chh · /ohh · /crash · /ride
- **cr78.c** → cr78/kick · /snare-body · /snare-rattle · /rim · /hat-closed · /hat-open · /cymbal-tone · /cymbal-noise · /tambourine · /cowbell · /claves · /maracas · /bongo · /conga · /guiro · /metal-beat
- **drummachine.c** → drummachine/kick · /snare · /closed_hat · /open_hat · /clap · /bass

**Synths & sequencers:**

- **tb303.c** → saw/303 · square/303 (note: PRESET[] entries are sequencer patterns, not patches)
- **juno.c** → saw/juno (the chorus showcase — the patch is a plain saw+sub poly synth; the `chorus()` master effect IS the Juno)
- **cathedral.c** → organ/cathedral (the reverb showcase — a clean organ that blooms into a hall; `reverb()` is the instrument)
- **mistress.c** → guitar/mistress (the flanger showcase — a strummed steel string through `flanger()`; the swept comb is the instrument)
- **tapeloop.c** → saw/tapeloop (the tape showcase — a soft saw pad through a long echo loop + `tape()`; the wow/flutter/saturation degradation is the instrument)
- **mellotron.c** → user0/mellotron-strings · -choir · -flute · -brass (drawn-wave fakes of a tape "recording" — wave + noise chiff + `chorus`/`tape`/`reverb` reconstruct the recording cues; the ~8s tape-length force-release is the headline. Cousin of tapeloop)
- **clavinet.c** → epiano/clav-wah (the auto-wah showcase — a percussive INSTR_EPIANO clav (h0.85 t0.75 m0.55) through `instrument_wah()`; the envelope-following quack is the instrument)
- **solina.c** → saw/solina-tab (the chorus companion to juno — a divide-down string machine: 6 stacked-saw footage tabs, the ensemble `chorus()` IS the Solina)
- **stylophone.c** → pulse/square · drawn/square · /sine · /sawtooth · /organ
- **loopstation.c** → sine/kick · noise/snare · noise/hat · saw/bass · pulse/lead · sine/theremin · tri/click
- **touchpiano** → saw/synth-piano
- **heldnotes.c** → saw/theremin
- **sh101.c** → no fixed recipes (one unified 4-source mixer instrument; live faders)
- **moog.c** → no fixed recipes (live patch-building playground; its *signal path* is lifted into carlos's `saw/fat-moog` — see [`instrument-presets.md`](instrument-presets.md))
- **modrack.c** → mostly no fixed recipes (sequencer/patcher; PRESETS are module wiring, not timbres) — except the two Raymond Scott homages `modrack/electronium-voice` · `modrack/bandito-bongos`, which name a reusable voice (see the modrack entry above)

**Whimsical instruments:**

- **handpan.c** → handpan/ring · handpan/tok
- **omnichord.c** → harp/sonic-strings · drum/kick · /snare · /hihat · /ohat · /clap · /bass
- **mariopaint.c** → tri/meow · sine/bell · square/tuba · square/lead · sine/swell · saw/harp
- **otamatone** → otamatone/reed
- **mouthharp** → saw/mouth-harp · noise/tongue-tick
- **glassharmonica.c** → sine/glass
- **hurdygurdy** → saw/drone-tonic · saw/drone-fifth · saw/melody-vibrato
- **musicalsaw** → sine/bow
- **martenot.c** → no fixed recipes (one persistent `INSTR_USER0` voice; the timbre is rebuilt live with `wave_set` by summing the lit O/C/G/N stops — a drawbar, not a patch; the touche lever drives `note_vol`+`note_cutoff`, diffuseurs are `instrument_reverb`/`_chorus` sends. + a souffle `INSTR_NOISE` bed)
- **fartsynth** → fart/rumbler · /squeaker · /trumpet · /wetone · /silentbutdeadly · /machinegun
- **multitouch.c** → no fixed recipes (default-MALLET `hit()` per touch; no `instrument()`)

**Voice carts (INSTR_VOICE):**

- **vox.c** → voice/formant
- **say.c** → no fixed recipes (one base voice; CHARS[] are prosody bundles — NORMAL/ROBOT/KID/GIANT/WHISPER)
- **voxab.c** → no fixed recipes (A/B comparator; pairs toggle single voice_param levers)
- **voxpad.c** → no fixed recipes (one base voice; consonant/coda + SIZE/EFFORT performance sliders)

**Sequencers / visual carts (no instrument recipes):**

- **garden.c** → no fixed recipes (procedural music; transient note()/hit()/strum())
- **wowflutter.c** → no fixed recipes (a sequenced Frippertronics-ish loop: held INSTR_PD pad + INSTR_MALLET pentatonic bells + sub thumps, smeared through `echo()`; the `tape()` master insert + transport warble ARE the degradation — the soft-body / visual companion to tapeloop)
