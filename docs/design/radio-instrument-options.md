# Radio instrument options — per-song timbre roll

Each station currently picks one fixed timbre per slot. This doc maps what we
have, what the real genre uses, and which alternatives are worth rolling per
song so the same radio sounds like a different record each time.

Alternatives are marked: **now** = buildable with current API · **pluck** =
needs INSTR_PLUCK (shipped) · **mallet** = needs INSTR_MALLET (shipped 2026-06-05;
macro taste-tuning still settling in the `mallet` cart) ·
**FM** = needs INSTR_FM (shipped 2026-06-05; taste-tuning settling in the `fm`
cart — note this is the FM *engine*, gap 2a; the Juno second-audible-oscillator
plumbing, gap 2b, is still deferred).

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
| lead | SQUARE+duty 0.22 = whistle | **now** — TRI+LP = mellower Telecaster lead; SINE+vibrato+LP = organ hint; SAW+LP+duty = Farfisa. |
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
| skank | TRI+env = choppy guitar | **now** — organ skank: TRI+LP lower cutoff + slower env. Electric piano skank: add pitch env blip. |
| bass | SINE+LP 420Hz | **now** — very faithful; vary `decay_ms` 220–320ms by seed for tighter/looser feel. |
| melodica / lead | SQUARE+duty 0.38 | **now** — SINE+LP+vibrato = trumpet flutter; SQUARE+duty 0.5+HP = harmonica-ish; narrow SQUARE duty 0.12 = flute. |
| organ bubble | TRI+vol-LFO | **now** — duty-swept SQUARE = Hammond drawbar hint; SAW+LP = thicker chord stab. |

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
| lead synth | SQUARE+LFO chorus | **now** — SINE+vibrato+LP = Rhodes-hint; **FM** = real Rhodes bell overtone. The electric piano gap is the big one here. |
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
| stab | SAW+LP = chopped chord | **now** — SQUARE+LP = organ stab; TRI+LP+attack = Rhodes-ish pluck. Filter cutoff start varies widely per seed (800–1800Hz) before the ride opens it. |
| bass | TRI+snap-env | **now** — SAW+LP = rounder disco bass; SINE+LP = very round sub-bass style. |
| lead | SQUARE+duty 0.3 = Da Funk | **now** — SAW+LP = talkbox hint (more harmonics); SQUARE+duty 0.5 = fatter; **FM** = real talkbox formant. |
| strings | SAW+slow-attack+LP | **now** — vary filter cutoff 700–1200Hz by seed for dark/mid strings; short attack = pizzicato stab. |

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
| Rhodes | TRI+tremolo+bark-env | **now** — already good. Vary tremolo rate 3.5–5Hz + depth 0.08–0.14 by seed. |
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
- Lowend lead: vibraphone = the Nujabes sound — single highest-value engine swap, do first
- Ambient bell: real bell/vibraphone ring
- YMO lead: marimba/vibraphone counterpoint (Hosono territory)
- Bossa lead on ballads: vibraphone
- (each retrofit: live A/B toggle against the old fake — the G-key pattern from jangle/bossa)

**INSTR_FM (shipped 2026-06-05 — retrofits now unblocked):**
- Citypop lead: real Rhodes bell overtone (the epiano preset, ratio 1 + decaying brightness)
- Lowend Rhodes: candidate A/B against the TRI+tremolo fake
- House lead: talkbox-adjacent growl via the feedback macro (true formant still needs §8.3)
- (each retrofit: live A/B toggle against the old fake — the G-key pattern)
