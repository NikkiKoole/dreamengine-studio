# Radio instrument options — per-song timbre roll

STATUS: EXPLORING (menu) — per-station timbre swaps ranked per engine; roll-per-song candidates.

Each station currently picks one fixed timbre per slot. This doc maps what we
have, what the real genre uses, and which alternatives are worth rolling per
song so the same radio sounds like a different record each time.

Alternatives are marked: **now** = buildable with current API · **pluck** =
needs INSTR_PLUCK (shipped) · **mallet** = needs INSTR_MALLET (shipped 2026-06-05;
macro taste-tuning still settling in the `mallet` cart) ·
**FM** = needs INSTR_FM (shipped 2026-06-05; taste-tuning settling in the `fm`
cart — note this is the FM *engine*, gap 2a; the Juno second-audible-oscillator
plumbing, gap 2b, is still deferred) · **organ** = needs INSTR_ORGAN (shipped +
published 2026-06-07 — tonewheel/drawbar, usable now; the real Hammond/Vox/Farfisa
that dub, jangle, house, and roadhouse were faking with wave_set or TRI hints) ·
**wave** = a `wave_set()` table recipe (INSTR_USER0 — roadhouse's drawbar organ
and tango's bandoneón are the two station gigs so far; "rolling the wave" means
generating the harmonic mix from the song seed).

⚠ **INSTR_EPIANO is NOT a station lever yet.** The engine shipped + published
2026-06-08 (Rhodes/Wurli/Clav), but it is **PARKED for radio use**: its character
leans on a per-voice wah that's provisional/poopy, and the real plan is **a wah as
a proper bus effect** (instrument-engines.md §8.10). Until that lands, stations
that want electric piano stay on the **FM** detent / **TRI+tremolo** fakes — do
NOT retrofit any station to INSTR_EPIANO, and do NOT adopt the `epiano.c` wah
recipe (game-music.md §"Wah is just the filter swept") into a station.

The engine levers above (pluck/mallet/FM/organ/wave) are live, so for the newer
stations the question is no longer "which engine is missing" but "which
alternatives should the seed roll". Exotica is the model citizen — it rolled
macros per song from day one.

---

## bossa

**Current:** nylon guitar (TRI+filter+env / INSTR_PLUCK A/B), fingered bass
(TRI+LP), flute lead (SINE+vibrato), caxixi shaker (NOISE+HP), cross-stick rim
(NOISE+band).

**Real genre:** nylon-string guitar always; upright or electric bass; flute the
most common horn lead, but also soprano sax, violin on ballads; shaker/pandeiro/
surdo percussion varies widely; piano rare (Jobim studio cuts sometimes);
vibraphone on slower ballads.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| guitar | TRI / INSTR_PLUCK | **pluck** only — already A/B'd. Vary `harmonics`/`morph` per song seed for nylon vs gut-string feel. |
| bass | TRI+LP 700Hz | **now** — SAW+LP is a rounder upright; SINE+LP 500Hz is the fingered electric side. |
| lead | SINE+vibrato = flute | **now** — narrow SQUARE+LP+duty≈0.1 ≈ soprano sax breathiness; SINE no-vibrato+LP = piano ballad RH; **mallet** — vibraphone on slow songs. |
| shaker | NOISE+HP | no variation needed — already disappears into the mix. |
| rim/clave | NOISE+band | **now** — NOISE+LP shorter = woodblock; vary band center 1200–2400 Hz by seed. |

---

## afrobeat

**Current:** two interlocking `INSTR_GUITAR`s (muted tenor + open rhythm, panned wide),
horn section (REED sax + BRASS trumpet), organ/Rhodes comp — all on the `B` "guitars/
horns/comp" chairs.

**Retrofit shipped (opt-in):** the **guitars** chair gained a third option, **"amp"** — the
clean voicing run through the `ampcab.h` **CHIME** voicing (DRIVE_ASYM presence + airy EQ +
light power-amp glue), for highlife/funk electric-guitar presence and a bit of cohesion
between the two guitars. **Default stays sel 0 (muted), so the shipped sound is untouched;**
A/B confirmed level-safe (peak/RMS unchanged, 0 clipped). The first amp/cab retrofit onto an
older station — the model for any future one (always opt-in via a chair, never forced). The
acoustic-guitar stations (`air` nylon, `mariachi` vihuela) deliberately do **not** get it.

---

## jangle

**Current:** chorus guitar (TRI+pitch-LFO / INSTR_PLUCK A/B), sine bass,
whistle lead (SQUARE+duty), CR-78-ish kick/snare/hat.

**Real genre:** Rickenbacker electric guitar with natural chorus always; bass
guitar (P-bass or J-bass, fairly bright); lead doubles between 12-string guitar,
organ, or a single-coil lead guitar; drums are a real kit (CR-78 is accurate for
DeMarco; R.E.M. uses live kit); tambourine/shaker added on upbeats.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| guitar | TRI+LFO / INSTR_PLUCK+LFO | **pluck** — vary `timbre` (bright pick vs softer thumb) per seed. 12-string doubling: add a second voice an octave up at -2 vol, same slot. |
| bass | SINE+LP 600Hz | **now** — TRI+LP 700Hz is brighter (J-bass); SQUARE+LP+duty≈0.4 ≈ the Smiths' punchy bass. |
| lead | SQUARE+duty 0.22 = whistle | **now** — TRI+LP = mellower Telecaster lead. **organ** — INSTR_ORGAN is the real organ-double / Farfisa now (the SINE+vibrato and SAW+LP+duty hints stay as fallbacks). |
| hat | NOISE+HP | **now** — add tambourine: shorter decay + band at 5kHz sharpens it. |

---

## ambient

**Current:** slow saw pad (4 voices, SAW+LP), sine sub bass (held), sine bell
(long SINE+LP), band-passed noise wind.

**Real genre:** Eno uses piano + synth pads; Budd = acoustic piano constant; TD
= synth pads + sequenced arpeggios; all use very long tails, no percussion, field
recordings or noise textures as atmosphere.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| pad | SAW+LP slow swell | **now** — SQUARE+LP for a warmer pulse-wave body; narrow duty for colder Tangerine Dream tone. |
| sub | SINE held | no variation — it's felt not heard; just vary root pitch. |
| bell | SINE+LP | **mallet** — a real bell/vibraphone ring would transform this slot. For now: vary filter cutoff (2000–5000Hz) by seed for dark vs glassy. |
| wind | NOISE+band 700Hz | **now** — shift band center 400–1200Hz per song for a different weather. |

---

## dub

**Current:** offbeat skank (TRI+env), deep sine bass, melodica (SQUARE+duty+LFO),
organ bubble (TRI+LFO), kick/rim/hat, siren toy.

**Real genre:** skank is electric guitar (or organ), always; bass is always bass
guitar, very dominant; melodica is Augustus Pablo's trademark but NOT every track
— many use trumpet or sax instead; organ (Hammond B3) is constant; melodica,
trumpet, and sax are alternatives for the melody slot.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| skank | TRI+env = choppy guitar | **organ** — INSTR_ORGAN skank (thin combo registration + a fast gate env) is the real Hammond chop now; TRI+LP fake stays as the guitar-skank night. |
| bass | SINE+LP 420Hz | **now** — very faithful; vary `decay_ms` 220–320ms by seed for tighter/looser feel. |
| melodica / lead | SQUARE+duty 0.38 | **now** — SINE+LP+vibrato = trumpet flutter; SQUARE+duty 0.5+HP = harmonica-ish; narrow SQUARE duty 0.12 = flute. |
| organ bubble | TRI+vol-LFO | **organ** — INSTR_ORGAN is the real B3 (roll the registration macro per song: thin combo vs fuller drawbar). The old duty-swept SQUARE / SAW+LP hints stay as cheaper fallbacks. |

---

## ymo

**Current:** square synth bass (SQUARE+duty+env), TRI arp conveyor belt,
square lead (SQUARE+duty+vibrato), CR-78 extended kit (kick/snare-shell/
snare-rattle/hat-c/hat-o/metallic/claves).

**Real genre:** Moog/synth bass always; ARP/Moog sequenced arpeggios (often
no vibrato, very machine-like); lead is bright synth — can be Moog, can also be
treated saxophone or vocoder texture; 808 from "Technodelic" onward; vibraphone/
marimba appears as textural counterpoint on some Hosono/Sakamoto tracks.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| bass | SQUARE+duty 0.35 | **now** — vary duty 0.3–0.5 + env amount per song for fatter/thinner synth bass. SAW+LP = warmer Moog bass variant. |
| arp | TRI+LP, staccato | **now** — SQUARE+duty = colder sequencer tone; SINE+LP = rounder, more Hosono. |
| lead | SQUARE+vibrato | **now** — SINE+LP+vibrato = Sakamoto piano-synth flavor; SAW+LP+slow attack = string machine lead. **mallet** — marimba/vibraphone counterpoint (Hosono's *Watering a Flower* territory). |

---

## citypop

**Current:** clean funk guitar (TRI+env), octave-pop bass (TRI+pitch-env),
bright chorus synth lead (SQUARE+LFO), saw brass section (SAW+fall-env), kit.

**Real genre:** Fender Rhodes (electric piano) near-constant and most distinctive
sound; clean electric guitar (funk chunks); electric bass tight and funky; tenor or
soprano saxophone for solos; string section on big arrangements; chorus synth lead
comes later (mid-80s); flute on EPO/Anri arrangements.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| guitar | TRI+env = funk chunk | **now** — narrow duty SQUARE+HP = single-coil brightness; vary env decay 40–80ms per seed. |
| bass | TRI+pitch-env | **now** — SINE+LP = rounder; add SAW+LP variant for slap-bass brightness. |
| lead synth | SQUARE+LFO chorus | **now** — SINE+vibrato+LP = Rhodes-hint; **FM** = real Rhodes bell overtone (the interim epiano). INSTR_EPIANO would close this — the genre's most distinctive sound — but it's **parked for stations** (see the intro note); stay on FM until the bus wah lands. |
| brass | SAW+fall-env | **now** — narrow SQUARE+fall-env = tighter horn stab; two-voice spread (SAW +4 semitones) = bigger section. |

---

## house

**Current:** saw stab (SAW+LP = the sampled chop), TRI disco bass+pluck-snap,
square mono lead (Da Funk), saw string machine (held), 808-circuit kit
(kick/clap/hat-c/hat-o/cymbal/maracas).

**Real genre:** French house chops soul/disco samples — the stab IS a sampled
Rhodes, organ, or guitar lick put through the filter ride. String machine (Solina
or Mellotron strings) constant. Bass is disco bass (Fender, very funky). Lead
varies: vocoder, talkbox, mono synth, or filtered sample hit. Drum machine always
(Roland TR-707/909 or sampled).

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| stab | SAW+LP = chopped chord | **now** — SQUARE+LP = organ stab; TRI+LP+attack = Rhodes-ish pluck. **organ** — INSTR_ORGAN chord through the filter ride = a real organ-chop sample night. Filter cutoff start varies widely per seed (800–1800Hz) before the ride opens it. |
| bass | TRI+snap-env | **now** — SAW+LP = rounder disco bass; SINE+LP = very round sub-bass style. |
| lead | SQUARE+duty 0.3 = Da Funk | **now** — SAW+LP = talkbox hint (more harmonics); SQUARE+duty 0.5 = fatter; **FM** = real talkbox formant. |
| strings | SAW+slow-attack+LP | **now** — vary filter cutoff 700–1200Hz by seed for dark/mid strings; short attack = pizzicato stab. |
| hats/shimmer | offbeat open hat + 16th maracas, always on | **owner feedback (2026-06-05):** *"the tingling rhythm sound — sounds very disco, but we shouldn't have that in every song."* Roll it from the composition seed: some songs get the full disco shimmer (open-hat offbeat + maracas), some swap to closed-hat-only or ride-style, some drop the maracas entirely. Part of the per-song timbre roll (kit dressing), not a fixed layer. |

---

## jingle

**Current:** fingerpicked guitar (TRI+LFO warble), sine bass, sine singing lead
(SINE+vibrato), kick/rim/hat.

**Real genre (acoustic indie/folk):** fingerpicked acoustic guitar always; bass
guitar or upright; cello or violin on strings; piano or organ underneath; flute or
oboe as lead doubles; electric guitar very rare in this zone.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| guitar | TRI+gentle-LFO | **pluck** — INSTR_PLUCK with low timbre, medium morph = acoustic fingerpick. LFO warble stays. |
| bass | SINE+LP 520Hz | **now** — TRI+LP = upright bass warmth. |
| lead | SINE+vibrato = singing voice | **now** — SINE+LP no-vibrato = cello; narrow SQUARE+LP = oboe-ish; **mallet** = celesta for intimate songs. |

---

## lowend (lofi jazz)

**Current:** TRI Rhodes with tremolo + bark-env, sine upright bass+thump-env,
TRI sparse lead+vibrato, kick/snare/hat, vinyl noise.

**Real genre (lofi jazz/hip-hop):** Fender Rhodes near-constant; upright bass
(very common) or electric bass; saxophone or trumpet for melody; vibraphone appears
frequently (Nujabes signature); vinyl crackle always in lofi aesthetic; brushed
drums or sampled breaks.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| Rhodes | TRI+tremolo+bark-env | **now** — already good; stay here. Vary tremolo rate 3.5–5Hz + depth 0.08–0.14 by seed. (INSTR_EPIANO is the eventual upgrade but is **parked** — see intro note.) |
| bass | SINE+LP+thump-env | **now** — TRI+LP = slightly brighter electric-bass feel. |
| lead | TRI+vibrato = sparse hook | **now** — SINE+vibrato = trumpet flutter; SQUARE+LP = sax hint; **mallet** = vibraphone (the Nujabes sound — highest-value swap). |
| vinyl | NOISE+HP | **now** — vary HP cutoff 3000–6000Hz by seed for dustier/cleaner pressing. |

---

## satie

**Current:** TRI right-hand piano (fast decay, filter env hammer), TRI left-hand
(slower, rounder).

**Real genre:** solo piano is the original — no alternatives in the classic
recording. Arrangements exist: strings (cello + violin), flute+piano duet,
harpsichord, small chamber ensemble. The looped generative version is already
an arrangement.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| right hand | TRI+filter-env | **now** — vary env amount (400–900Hz) and decay (150–250ms) by song for a different piano registration. SINE+LP = softer felt hammer. |
| left hand | TRI+LP rounder | **now** — SINE+LP = very dark left-hand voicing; vary cutoff 900–1600Hz. |
| add: cello line | — | **now** — SINE+LP+slow-vibrato at -12 semitones under RH melody; sparse, 1 voice. Only on the Gnossienne songs. |

---

## exotica

**Current:** vibraphone lead (INSTR_MALLET, motor on), nylon comp guitar
(INSTR_PLUCK), glass-bell sparkle (INSTR_FM bell detent), upright bass
(TRI+pitch-thump), latin kit (clave/shaker/conga/finger cymbal), the aviary
(aleatoric bird/frog calls, never seeded).

**Real genre:** the genre has two poles — Arthur Lyman (vibraphone lead) and
Martin Denny (he was a *pianist*; vibes were his sideman's). Marimba is the
other mallet color; celesta appears on Baxter arrangements; percussion is
latin + "bamboo" novelty instruments; bird calls were the band improvising.

**Already rolls per song:** vibes macros, guitar pick, bird species pair, kit
dressing — the reference implementation for this whole doc.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| vibe lead | INSTR_MALLET, motor on | **now** — roll `harmonics` toward 0 = marimba night (Lyman's other color); roll `morph` down = drier 50s mallet sound. TRI+felt-env = the *Denny piano* night — the genre's other pole, worth a real A/B. |
| comp guitar | INSTR_PLUCK nylon | already rolled — leave it. |
| bell | INSTR_FM bell detent | **now** — vary ratio/brightness slightly per seed; **mallet** celesta (harmonics ≈ 0.5) as an alternate sparkle. |
| aviary | sine swoops + square croak | leave unseeded — that's the point. Maybe roll the *species pool* (jungle vs shore night). |

---

## yacht

**Current:** FM electric piano (1:1 tine detent + tremolo), round fingered
bass (TRI+pitch-thump), clean strat stabs (INSTR_PLUCK bright), breathy sax
(narrow SQUARE+LP+vibrato), soft string pad (SAW+LP), ride + session kit OR
CR-78 circuits (already rolled per song).

**Real genre:** Rhodes vs Wurlitzer vs DX-tine vs clavinet — the epiano *kind*
varies per record; bass is P-bass (Chuck Rainey school), tight and round;
lead chair alternates sax, guitar solo (Carlton), and synth; strings on lush
cuts, synth brass stabs on funkier ones.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| epiano | INSTR_FM 1:1 detent | **FM** — roll `timbre` (tine brightness) + tremolo depth per song: DX glass vs softer Rhodes night. Clav night: INSTR_PLUCK, narrow + bright, comping the same voicings. (INSTR_EPIANO's Rhodes/Wurli/Clav macro is the natural future chair here, but it's **parked** — see intro note; keep the FM detent for now.) |
| bass | TRI+thump | **now** — SINE+LP = rounder ballad bass; SAW+LP brighter = the slap-adjacent cuts. |
| chorus lead | SQUARE duty 0.12 = sax | **now** — SQUARE duty 0.3 = synth lead night; **pluck** — guitar solo lead (bright timbre, high register). Roll which session player got the solo. |
| pad | SAW+LP strings | **now** — SAW+fall-env stabs = synth brass night (citypop's loan repaid). |
| kit | session vs CR-78 | already rolled (incl. Purdie vs straight) — leave it. |

---

## roadhouse

**Current:** combo organ comp + solo stop (INSTR_USER0 drawbar wave_set +
vibrato LFO), Rhodes piano bass (INSTR_FM epiano detent, low), Krieger guitar
+ open-string drone (INSTR_PLUCK), trio kit (shuffle/latin/straight already
rolled per song).

**Real genre:** Manzarek switched organs — Vox Continental early, Gibson G-101
later (brighter, reedier); piano bass constant until *L.A. Woman* brought in a
real session bassist (Scheff); Krieger moves between fingerpicked, flamenco
runs, and fuzz; harpsichord and marimba cameo on *Strange Days*.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| organ | USER0 drawbar recipe | **wave** (shipped) — roll the footage mix per song: current recipe = Vox night; more 2nd/4th harmonic + brighter filter = Gibson night. Roll vibrato-tab depth with it. **organ** — INSTR_ORGAN is now an alternative to the wave_set approach (registration + scanner-chorus macros, key click built in); the shipped wave_set chairs stay — A/B them if revisiting. |
| piano bass | INSTR_FM epiano, low | **now** — TRI+LP+thump = the session-bassist night (*L.A. Woman*); keep FM as the default — it's the signature. |
| guitar | INSTR_PLUCK | **now** — roll `timbre` (fingerpicked vs flatpick) and add `instrument_drive` = the fuzz night. |
| kit | shuffle/latin/straight | already rolled — leave it. |

---

## cocktail

**Current:** felt piano, comp + solo stop (TRI+cutoff-env ×2, satie's recipe),
upright bass (TRI+pitch-thumb), brushed ride (SQUARE+HP), hat, brush sweep
(NOISE slow), feathered kick.

**Real genre:** the piano trio's third chair is the historical variable —
Oscar Peterson's first trio had *guitar and no drums* (the Nat Cole format);
Guaraldi used drums; add vibes and it's the MJQ. Brushes vs sticks splits the
repertoire; the bass solo chorus is constant either way.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| piano | TRI+felt env | **now** — registration roll (env amount 400–900, decay 150–250) like satie; SINE+LP = the closed-lid ballad set. |
| the guest chair | — (trio is fixed) | **mallet** — vibes guest on some tunes = the MJQ night (improv.h already solos; give it the vibes); **pluck** — guitar trio night (Herb Ellis), comping where the piano LH was. Highest-value addition on this station. |
| bass | TRI+thumb | **now** — SINE+LP darker; solo chorus already exists. |
| brushes | NOISE kit, soft | **now** — the sticks night: ride HP up, hat tighter, kick less feathered, sweep dropped. |

---

## tango

**Current:** bandoneón chords + right hand (INSTR_USER0 free-reed wave_set,
bellows LFO on the RH), violins one desk (SAW, vibrato + scoop already rolled
per song), felt piano (satie's recipe), upright bass (TRI+thumb), chicharra +
golpe (no kit — the band is the percussion).

**Real genre:** orquesta típica = bandoneón *section* (up to four), violin
section, piano, contrabass. The cantabile lead alternates bandoneón and
violin per tune; Pugliese's sound is darker/heavier than D'Arienzo's bright
drive; early guardia vieja ensembles used flute and guitar instead.

| Slot | Current fake | Alternatives to roll |
|---|---|---|
| bandoneón | USER0 free-reed recipe | **wave** — roll the reed harmonic mix per song: brighter odd-harmonics = D'Arienzo drive night, darker fundamentals = Pugliese yumba night. Roll bellows-tremble depth with it. |
| violins | SAW one desk | **now** — second desk (same line, slight detune, -2 vol) on the big songs; pizzicato variant (short env, no scoop) for síncopa passages. |
| B-section lead | violin takes it | **now** — roll WHO sings the cantabile: violin vs bandoneón RH vs **mallet**-celesta-tinted piano. The orquesta's real per-tune variable. |
| piano | TRI+felt env | **now** — satie registration roll. |

---

## Summary — highest-value swaps by engine tier

**Now (current API):**
- Bossa lead: flute → soprano sax (SQUARE+LP) → piano ballad (SINE no-vibrato)
- Dub melodica: → trumpet (SINE+vibrato) → harmonica (SQUARE+HP)
- Jangle bass: → P-bass (TRI+LP) → Smiths bass (SQUARE+duty)
- Lowend lead: → trumpet (SINE+vibrato) → sax (SQUARE+LP)
- Any brass slot: vary env fall amount ±4 semitones per seed

**INSTR_PLUCK (shipped):**
- Bossa guitar, jangle guitar: already A/B'd — vary macro values per song seed
- Jingle guitar: swap in INSTR_PLUCK (not yet done)

**INSTR_MALLET (shipped 2026-06-05 — retrofits now unblocked):**
- ~~Lowend lead: vibraphone = the Nujabes sound — single highest-value engine swap, do first~~
  **DONE 2026-06-05** — G-key A/B in `lowend.c` (TRI hook vs vibes preset from the mallet cart)
- Cocktail guest chair: vibes = the MJQ night (improv.h takes the solo) — new highest-value mallet item
- Ambient bell: real bell/vibraphone ring
- YMO lead: marimba/vibraphone counterpoint (Hosono territory)
- Bossa lead on ballads: vibraphone
- (each retrofit: live A/B toggle against the old fake — the G-key pattern from jangle/bossa/lowend)

**INSTR_FM (shipped 2026-06-05 — retrofits now unblocked):**
- Citypop lead: real Rhodes bell overtone (the FM epiano detent, ratio 1 + decaying brightness) — the interim, until INSTR_EPIANO is unparked
- Lowend Rhodes: candidate A/B against the TRI+tremolo fake
- House lead: talkbox-adjacent growl via the feedback macro (true formant still needs §8.3)
- (each retrofit: live A/B toggle against the old fake — the G-key pattern)

**INSTR_ORGAN (shipped + published 2026-06-07 — retrofits now unblocked):**
- Dub skank + organ bubble: the real Hammond B3 (roll the registration per song; the genre's constant) — highest-value organ item
- Jangle lead: organ-double / Farfisa night, a real engine instead of the SINE/SAW hints
- House stab: INSTR_ORGAN chord through the filter ride = an organ-chop sample night
- Roadhouse: an A/B candidate against the shipped USER0 wave_set drawbar chairs
- (each retrofit: live A/B toggle against the old fake — the G-key / BAND-panel pattern)
- New stations it unblocks: motorik/Stereolab (Farfisa drones, native now), a gospel/soul-jazz organ trio (Jimmy Smith)

**INSTR_EPIANO (shipped + published 2026-06-08 — ⚠ PARKED for stations):**
- The dedicated Rhodes/Wurli/Clav engine. It would close the electric-piano gap
  on citypop, lowend, yacht, cocktail, roadhouse — but **do not retrofit any
  station to it yet.** Its character relies on a provisional per-voice wah; the
  real plan is a wah-on-a-bus effect (instrument-engines.md §8.10). Until that
  ships, electric-piano slots stay on the FM detent / TRI+tremolo fakes, and the
  `epiano.c` wah recipe is **not** to be adopted into a station.

**wave_set recipes (USER0 — roll the table itself from the seed):**
- Roadhouse organ: footage mix = Vox night vs Gibson night
- Tango bandoneón: reed mix = D'Arienzo drive vs Pugliese yumba

**Newer-station "now" rolls:**
- Yacht lead chair: sax vs synth vs guitar solo — roll which session player got the call
- Cocktail brushes vs sticks night; tango second violin desk + pizzicato síncopa
- Exotica vibes→marimba night; the Denny-piano pole as a real A/B

## Where the live toggles stand

The evidence-gathering step before a chair's roll goes into the seed: a live
toggle, mid-song, against the shipped fake.

**THE BAND panel — SHIPPED 2026-06-05.** The design idea below became
`runtime/radio.h`'s band registry (`rad_chair` / `rad_band_input` /
`rad_band_panel`): press **B** for a second overlay besides help where every
chair lists its candidates — click a row or press its number to cycle, live,
mid-song. The cart applies each swap in its own `apply_chair()` recipes; the
toolkit never calls `rad_srnd`, so pinned seeds stay byte-identical. Picked
chairs re-assert after `new_song`'s per-song rolls; sel 0 keeps the shipped
sound. It doubles as the audition UI for tuning each station's roll tables.

| Station | Chairs | Status |
|---|---|---|
| jangle | guitar (TRI chorus vs PLUCK) | shipped (G key — pre-panel) |
| bossa | guitar (TRI vs PLUCK) | shipped (G key — pre-panel) |
| lowend | lead (TRI hook vs MALLET vibes) | shipped 2026-06-05 (G key — pre-panel) |
| cocktail | piano · solo chair (piano/vibes MJQ/guitar Herb Ellis) · bass · drums (brushes/sticks) | band panel, 2026-06-05 |
| tango | bandoneón reed (troilo/d'arienzo/pugliese) · violins (arco/pizzicato) · piano (felt/dark) | band panel, 2026-06-05 |
| yacht | ep (dx tine/soft rhodes/clavinet) · bass (round/sine/saw) · lead (pulse/synth/guitar) · pad | band panel, 2026-06-05 |
| roadhouse | organ wave (VOX/Gibson G-101) · piano bass (rhodes/upright) · guitar | band panel, 2026-06-05 |
| exotica | vibes (vibes/marimba/denny piano) · sparkle (fm glass/celesta) | band panel, 2026-06-05 |
| bossa, jangle, ambient, dub, ymo, citypop, house, jingle, lowend, satie | — | open — chair candidates in the per-station tables above |

The G-key carts (jangle/bossa/lowend) still work; migrating them to the panel
is the natural move in the same visit as any other touch.
