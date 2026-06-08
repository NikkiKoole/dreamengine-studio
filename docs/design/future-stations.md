# Future radio stations — the parking lot

Candidate stations for the radio family, ranked by engine fit, the citypop
conditions, and "what does the engine LEARN from this cart" — plus the IDM and
functional-music wings. **Genre: design exploration** (this is a backlog of
candidates and build-order reasoning, not a how-to).

> **Provenance.** Split out of [`../guides/game-music.md`](../guides/game-music.md)
> on 2026-06-07 — it was that guide's "Future stations — the parking lot" section,
> ~280 lines of exploration living inside a how-to. The *recipes* (step clock, chord
> brain, voice leading, brain catalog, style cheat-sheet, `radio.h`) stay in the
> guide; references to "the brain catalog" / "the style cheat-sheet" / "the
> density curve" below point there. Engine-side companions:
> [`instrument-engines.md`](instrument-engines.md) (what each station would need),
> [`audio-notes.md`](audio-notes.md) §12 (the gap ledger),
> [`tinydaws.md`](tinydaws.md) (stations as genre racks),
> [`radio-instrument-options.md`](radio-instrument-options.md) (retrofitting shipped
> stations).

## Future stations — the parking lot

Candidates ranked by **engine fit** (= the genre's essence maps onto machinery we
have, rather than needing approximation). Context for whoever builds next: the
station owner's taste profile, demonstrated across seven picks, is *warm,
harmonically lush, groove-forward, analog-nostalgic, jazz-adjacent — background
music that rewards attention; crate-digger sensibility*. They name-checked Hosono
and Sakamoto unprompted.

**The strongest candidates:**

1. ~~**Dub reggae**~~ — ✅ shipped as `dub.c`, and the engine-fit prediction held:
   echo via `schedule_hit` and the desk-as-instrument both worked first try.
   Kept here as the reference case for what "best engine fit" means.
2. ~~**YMO / techno-kayō synthpop**~~ — ✅ shipped as `ymo.c`, with the drum
   circuits on loan from `cr78.c` exactly as hoped. The native-timbre prediction
   held too; the Hosono bassline generator (counterpoint with inertia) is the
   reusable new block.
3. ~~**Ethio-jazz (Mulatu Astatke)**~~ — ✅ shipped as `addis.c` (2026-06-08,
   station #17), the deepest crate and the first station to leave 12-TET's
   major/minor world. **SCALES AS DATA landed as designed**: the seed rolls one
   of four qñit (tizita / bati / anchihoye / ambassel) as a 7-entry table, and
   every melodic voice (vibes head, bass ostinato, both solos) walks it — the
   improviser's `mode7` contract ate the pentatonic without a change (each
   entry reduces mod-12 into the five pitch classes, so a solo can only land
   in-scale). **THE MODAL VAMP** is the new chord-brain: a tonic drone (± a
   second i↔iv/i↔bII centre) held eight bars over a locked bass ostinato — no
   functional changes at all. Bonus the plan didn't call: it's the **first
   five-engine band** (MALLET vibes · EPIANO/ORGAN keys · PD horn · FM bass ·
   MEMBRANE congas/bongos/kebero) — MEMBRANE and PD, both same-day 2026-06-08
   engines, earning their first station slot. PD stands in for the horn line
   (no breath engine exists; synth-brass through a lowpass reads as a reedy
   horn doubling the vibes in unison + taking the second solo). The original
   pitch — *minor vamps with horns and vibraphone-ish keys, the accommodation
   rule doing something new* — held; accommodation is automatic here since the
   whole band shares one qñit.
4. ~~**Swing / cocktail jazz trio**~~ — ✅ shipped as `cocktail.c` (2026-06-05,
   station #14). The walking-bass prediction held: root → chord/scale motion →
   chromatic approach, ~20 lines as promised. **And it triggered THE IMPROVISER's
   graduation**: second customer → extracted to `runtime/improv.h` (the radio.h
   rule applied to melody); roadhouse.c migrated same-day. Bonus the plan didn't
   call: the BASS takes a solo too (same brain, register 45, density 0.62 — the
   brushes drop to a whisper and the piano just nods).
5. **Gamelan (Java/Bali)** — planned 2026-06-05; the first station to leave
   12-TET entirely. Four new tricks in one cart:
   - **the tuning system as data** — sléndro/pélog as cent tables fed through
     `note_pitch` floats; and since *no two real gamelans are tuned alike*
     (every village ensemble is its own instrument), **the seed rolls the
     tuning** — each song is played on slightly different bronze. Also the
     natural home of the per-song timbre-roll idea (game-music's "same band
     every night" section).
   - **colotomic time — a new TIME brain** — form as *nested gong cycles*
     (gong ageng marks the full gongan, kempul halves, kenong quarters,
     kethuk between), not bar→phrase→section. The arrangement curve becomes
     concentric circles.
   - **kotekan — melody as interlock** — two voices (polos/sangsih) on
     *complementary* onset masks summing to an unbroken 16th stream.
     Trace-verifiable by definition: assert the union fills the grid.
   - **ombak** — paired voices a few cents apart so they BEAT; ambient's
     detune promoted to the genre's whole timbral identity. Plus **irama**
     as the feel knob: density doubles as tempo halves.
   Open questions: tuning per song vs per session; Bali (fast, bright,
   kotekan-forward) vs Java (slow, deep) — possibly the two ends of the feel
   knob; face = gong rack vs a wayang shadow puppet dancing the cycle.
   Taste-fit: the Hosono thread again (*Cochin Moon*), Nonesuch Explorer
   crate-digging.
6. ~~**The Doors / modal psych-rock**~~ — ✅ shipped as `roadhouse.c`
   (2026-06-05, station #13). **THE IMPROVISER exists**: phrase-based solos
   (motif stated / answered-inverted / sequenced / doubled at the peak), a
   16-bar tension arc, breath rests, pure performance rnd — R replays the
   song, the solos are always new. Also the first station to pull the
   `wave_set` lever (the Vox drawbar cycle). The improviser lives in the
   cart for now — it graduates to a shared header when ethio-jazz or the
   cocktail trio (its other customers) confirm the boundary, same rule as
   `radio.h`. Original design notes kept below for the next customer:
   (was: owner-requested 2026-06-05, with the
   theory homework already done. The vamp side we mostly own (mixolydian
   one-/two-chord drones, "L.A. Woman" A–G over an A center that never
   resolves — jangle's vamp brain, slowed and darkened; modal-mixture
   excursions for the bridges, "Crystal Ship" color). What earns the slot is
   **THE IMPROVISER — melody brain #2**: every station so far repeats a cell
   or a fixed riff; nobody can take a SOLO. Manzarek demands a phrase-based
   improvisation generator — question/answer phrases with breath rests, a
   3–4 note motif *developed* (sequenced up/down the mode, rhythm varied),
   and a tension arc across 16–32 bars (register climbs, density rises,
   peak, release). Built once, it's reusable for ethio-jazz (#3) and the
   cocktail trio (#4). Solos are pure PERFORMANCE (engine rnd) — the band
   never played it the same twice, and neither should the radio.
   The rest of the kit maps clean:
   - **major/minor blues duality** — the soloist draws from minor pentatonic
     OVER the major vamp (a seeded "how blue" knob biasing b3/b7 vs major
     tones) — deliberate clash, the opposite of jingle's accommodation rule.
   - **Rhodes piano-bass ostinato** — a seeded 1-bar left-hand riff (dub's
     riddim, swung); combo-organ held chords above it: two keyboards, one
     player. Kit rolls shuffle / latin (Densmore's "Break on Through" is
     nearly a bossa) / straight rock.
   - **Krieger drone guitar** — open-string pedal tone held UNDER a moving
     modal line (two-voice guitar), flamenco/phrygian color for "The End"-
     style excursions — connects to the gamelan/drone interest.
   - **form** — the Doors essence is head → LONG instrumental middle (organ
     solo → guitar solo, each with its own arc) → head out. The arrangement
     curve finally gets to model a JAM.

**Also very doable:**

- **Motorik / Stereolab** — one-chord pulse, strict-8ths motorik kit, Farfisa-ish
  drones on held voices, maj7 planing on top. The experimental axis. **→ now
  fully spec'd: [`motorik.md`](motorik.md)** (promoted once `INSTR_ORGAN`
  unblocked the Farfisa drone; proves THE PROCESS FORM — the first sectionless
  station).
- **Chicha / Peruvian cumbia** — wobbly surf guitar (jangle's warble at higher
  depth), minor vamps, güiro 16ths. Crate-digger royalty.
- **Afrobeat** — 2–3 interlocking single-note guitar ostinatos (`euclid()` was
  made for this), one chord for eleven minutes, horn riffs as call-and-response.
- ~~**French house / disco edits**~~ — ✅ shipped as `house.c`, and the
  prediction held to the letter: the filter ride IS the song (`note_cutoff`/
  `note_res` on held strings from an arrangement-level curve). Bonus finds: the
  sidechain pump as a *filter* gesture (cutoff ducks at each kick), and THE
  VOID — one `return` statement of silence before every drop.
- ~~**Exotica (Martin Denny / Les Baxter)**~~ — ✅ shipped as `exotica.c`
  (2026-06-05): the first station born on the `radio.h` scaffold, the first to
  play **all three modeled engines in one band** (MALLET vibes + PLUCK nylon +
  FM bells — same-day engines, immediately exercised), and the aviary
  prediction held: unseeded `chance()`-per-bar bird/frog calls read exactly as
  the band improvising the jungle. The per-song timbre roll is built in from
  day one (vibes macros, guitar pick, kit dressing, bird species — all seeded).
- **Wendy Carlos / baroque counterpoint** — chord brain #7 that isn't chords
  at all: **rule-based two-voice species counterpoint** over a ground bass
  (a chaconne loop = the vamp brain's classical ancestor). Contrary motion
  preferred, no parallel fifths, dissonance only passing on weak beats —
  ~40 lines of rules generating real polyphony, played on bright Moog timbres
  (square + saw, generous portamento). *Switched-On Bach* as a station.
- **Steve Reich minimalism** — two voices, same pattern, slightly different step
  lengths → phasing. The most experimental station possible, nearly free to build.
- **Lofi hip-hop jazz** — still in game-music's style cheat-sheet; most of its parts
  (swing, rhodes, crackle, groove template) now exist across other carts.

Build order advice: pick by which *new engine trick* the station would prove,
not just by genre appeal — that's what kept the first seven carts each earning
their place (held voices, groove template, playbook encoding, loop rotation,
scheduled echo, the desk…).

### Why citypop lands — the four conditions (the second build-order axis)

Owner observation (2026-06-05): citypop is the station that works best. The
diagnosis generalizes — it scores 4/4 on conditions the others meet only
partly, and candidate genres can be scored against them:

1. **A named formula to steal.** The royal road (王道進行) and JTTOU changes
   are published, named progressions — encoding two templates captures the
   genre's identity. (Genres that are "a vibe" — ambient, jangle — can't be
   captured this way; they work differently, by texture.)
2. **Directional gestures per layer, not static masks.** The bass runs INTO
   each change, the brass anticipates the and-of-4 BEFORE the bar, the chucks
   lay out AT bar turns, the gear change lifts the LAST chorus — every layer
   points at what's coming. Forward pull beats pattern density.
3. **A time-feel that wants the engine's native precision.** Session-tight or
   machine-tight genres (citypop, ymo, house) get authenticity for free;
   loose genres need artificial sloppiness that's hard to dose (lowend
   shipped at half groove strength).
4. **Gloss-native timbres.** Our oscillators do neon (saws, bright squares,
   clean tris) natively; they can't do smoky (breath, real horns).

Genres scoring 4/4, in taste order:

- ~~**Yacht rock / Steely Dan & AOR**~~ — ✅ shipped as `yacht.c` (2026-06-05),
  second scaffold-born station. The mu chord landed as a first-class chord
  QUALITY in the template tables (+ a "mu-ify" flavor roll), the FM epiano
  got its first station workout (the tine pings every comp hit), and the kit
  rolls between session 16ths / Purdie half-time shuffle / CR-78 circuits on
  loan (the Hall & Oates machine-soul move).
- **Italo disco** — citypop's machine cousin: sequenced octave-arpeggio
  bass, dramatic minor formulas (i–bVI–bVII–V), bright stabs, drum-machine
  time, and the truck driver's gear change lives here too. Reuses house.c's
  808 + pump. The easy-win pick. (Its descendant **eurobeat** is the same
  recipe at 155 bpm if maximum formula is ever wanted.)
- **J-fusion (Casiopea / T-Square)** — citypop's instrumental twin, same
  Tokyo session scene — but it needs real solos, so it should wait for the
  Doors station's improviser (melody brain #2).

### New brains per cart — the third build-order axis (2026-06-05)

The first axis was engine fit, the second the citypop conditions; this one asks
**what does the engine LEARN from this cart?** — counting genuinely new brains
(see game-music's brain catalog), not genre appeal. The session that shipped
stations 11–14 ranked the remaining candidates:

| candidate | new brains | what they are |
|---|---|---|
| **gamelan** | **4** | seed-rolled TUNING (first exit from 12-TET) · colotomic time (form as nested gong cycles) · kotekan (melody as two interlocking voices, trace-verifiable) · ombak (paired-voice beating as the timbral identity) |
| **Aphex / IDM wing** | 3 | RHYTHM BRAIN #1: drums as grammar with a volatility knob (0 = loop, 1 = never the same bar) · ratchets (sample-exact sub-hits) · two deliberately CROSSED density curves |
| **KPM library** | 2 | MOOD METADATA AS THE FRONT DOOR ("give me tension" — the API games actually want; could retrofit the whole family as a mood-keyed library) · the drums-only BREAK as an arrangement feature |
| **motorik / Stereolab** | 1–2 | THE PROCESS FORM: no sections at all — one unbroken line, accumulation/subtraction as a continuous function of song-time (house's ride, committed to per-song) · the modulation as a once-per-song EVENT (a story beat, not a position). Sneaky-high taste fit (Farfisa+Moog+lounge = exotica∩Plantasia∩ymo) and the best *game-loop* music on the list — no boundaries to interrupt |
| **Plantasia** | 1 | the MELODY-FORWARD arrangement — first station where the lead is the protagonist (could promote the improviser from soloist to frontman) + the growing-houseplant face |
| **Eno / Reich phasing** | 1 | prime-length cells drifting against each other; nearly free |
| **Boards of Canada** | 1 | systematic MISTUNING as warmth (inconsistent per-voice detune + slow pitch LFOs as tape memory) |
| **Italo disco** | **0** | pure recombination: ymo's arp + house's pump + citypop's gear change + FM keys + new minor templates. High gloss, an easy afternoon, teaches nothing — *that's fine, but schedule it as dessert* |

(Ethio-jazz and J-fusion fell off the "blocked" list when `improv.h` shipped —
they're now ordinary stations: scales-as-data + the improviser.)

**Scouted BY the axis (2026-06-05)** — traditions found by hunting for
mechanisms we lack, rather than genres we know. None needs new engine API:

| candidate | new brains | the mechanisms |
|---|---|---|
| **Indian classical / raga** | **3–4** | raga as melody GRAMMAR (ascent ≠ descent, direction-gated tones, the pakad signature phrase — melody brain #3, beyond the improviser's mode walk) · tala + THE TIHAI (a cadence you COMPUTE: a phrase ×3 calculated to land exactly on sam) · alap (meterless form — the raga's notes revealed one at a time, pitch-set as dramaturgy) · the tanpura drone; raga-by-time-of-day is mood-metadata's cousin |
| ~~**tango / Romantic rubato**~~ | 1 big | ✅ shipped as `tango.c` (2026-06-05, station #15) — TEMPO AS A VOICE landed as designed: phrase-level accel/rit via live `bpm()`, and the clock genuinely survived untouched. Trace-verified: a rit dip at every 8-bar boundary, final rallentando to −22%, then the next song snaps a tempo. Bonus blocks the scout note didn't call: THE ARRASTRE, the chan-chan ending (the first station whose songs END rather than loop out), and the breathing-bellows window art — the new brain made visible |
| **barbershop / close harmony** | 1–2 | ADAPTIVE JUST INTONATION — retune each held chord to pure ratios with `note_pitch` floats (the "ring"); gamelan's tuning idea per-CHORD instead of per-song — the two reinforce |
| **West African / Afro-Cuban** | 1–2 | true POLYMETER (12/8 bell over 4/4, the timeline as the lock) · batucada's CONDUCTOR — a performance channel that *signals* the band (calls, breaks, re-entries; the desk mixes, the conductor commands) |
| **Balkan / klezmer aksak** | 1 | additive meter as data (2+2+3 limping 7/8) — satie proved odd bars |
| **pedal steel / country** | 1 | bending INDIVIDUAL VOICES inside a held chord (per-handle `note_pitch` — chord-internal glides nobody has used) |
| **the fill grammar** | 1 | cross-genre: phrase-end fills/turnarounds as a rule system — none of 14 stations does fills |

### Batch two: the IDM / electronic wing

The first nine stations were about **harmony brains**; this batch is about
**rhythm brains and timbre brains** — volatility, ratchets, mutation,
mistuning. The other half of the engine's expressiveness, mostly unexplored.

**Aphex Twin, decomposed** (the anchor of the batch):

1. **No repeating patterns = drums as composition.** The kit is a GRAMMAR, not
   a pattern: anchors persist (kick near 1, snare backbeat-ish), everything
   else re-rolls per bar — fills are the norm. One knob: *volatility*
   (0 = loop, 1 = never the same bar twice). The first brain for rhythm
   rather than harmony.
2. **Ratchets** — replace a hit with 3–8 sub-hits at 1/32–1/64 with vol/pitch
   ramps. Only viable since the sample-exact fix (cf68813): ~23ms spacing is
   exactly what block-quantization used to destroy. ~10 lines, like echo_hit.
3. **Soft vs hard = TWO independent density curves**, deliberately crossed
   ("Flim": angelic music-box at volatility 0 over drums at volatility 1).
   Extends the arrangement-curve model to two dimensions.
4. **Canon via echo_hit** — dub's echo with a ONE-BAR delay at +12, quieter,
   is a two-voice canon generator ("Avril 14th"). Structural reuse for free.
5. **Microtonal drift** (`note_pitch` floats — detuned wrong on purpose), odd
   meters (satie.c proved non-16 bars; 7/8 = 14 steps), and acid lines —
   `note_cutoff`/`note_res` squelch. house.c now rides those two calls as a
   slow arrangement gesture; the fast per-step acid squelch is still unclaimed.

**The rest of the wing, by engine fit:**

- **Boards of Canada** — likely the best taste-fit: systematic MISTUNING as
  warmth (inconsistent per-voice detune + slow pitch LFOs — tape memory),
  lydian/major nostalgic 2-chord vamps, slow hip-hop kits, heavy rolloff.
- **Burial** — 2-step garage swing (new groove template: shuffled kicks,
  off-grid woodblock snares), vinyl + rain, "vocal" sighs = filtered saw with
  heavy note_glide on wordless pentatonics. Sits next to lowend and ambient.
- **Squarepusher** — the ratchet engine at maximum + the Hosono bassline
  generator (ymo.c) turned virtuoso slap. Two existing blocks, pushed to 11.
- **Autechre (Tri Repetae era)** — pattern mutation applied to TIMBRE:
  percussion params (pitch, cutoff, decay) drift per hit; the drum machine as
  a slowly-decaying organism. Needs the rotating-slot trick (audio-notes §2.2).
- **Plaid / B12** — melodic IDM, arps in odd meters, bells. Gentlest entry.
- **Eno "Music for Airports"** — melody cells of different PRIME lengths
  (7/11/13 bars) phasing against each other. Nearly free; ties to the Reich
  idea above.

### Batch three: the functional-music wing

Music engineered for a PURPOSE — plants, shops, television programs that
didn't exist yet. The meta-joke is the point: these radios already ARE library
music — generative tracks with mood metadata waiting for a game to license
them. This wing makes that explicit.

- **Plantasia (Mort Garson, 1976)** — "warm earth music for plants." Early
  Moog, and the first MELODY-FORWARD station: every cart so far buries the
  lead in the arrangement; here the mono Moog lead is the protagonist — a held
  voice with note_glide portamento + vibrato (jangle's lead, promoted),
  bouncy staccato bass, celesta bells, innocent major harmony with vaudeville
  chromatic passing chords. Face gimmick: a generative houseplant that GROWS
  as the piece plays — fresh plant per seed, fully grown by the outro.
- **Muzak / sounds of the supermarket** — the Muzak Corporation's "Stimulus
  Progression" (1948): 15-minute blocks of ASCENDING INTENSITY engineered to
  pace workers — literally game-music's density curve, invented as a corporate
  product 75 years early (the cart should cite it). Vibraphone-led easy
  listening, muted brass, everything consonant and dynamically flattened, plus
  the foley layer: PA bing-bong chime, register beeps as percussion. Optional
  modern lens: mallsoft (drown it in echo_hit, pitch it down). Face:
  fluorescent aisle, PA speaker, the seed on a price display.
- **KPM library music (the green sleeves)** — Mansfield/Hawkshaw/Dale/Tew:
  big-band breakbeat funk made for unknown television, the most-sampled
  catalogue in hip hop. Two engine ideas, one big:
  - **the break** — an arrangement section that is deliberately DRUMS-ONLY as
    a feature (the bars that made these records sampleable; connects to
    lowend.c).
  - **MOOD METADATA AS THE FRONT DOOR** — KPM sold tracks by descriptor
    ("bright, confident movement", "tension: industrial"). The cart picks the
    mood FIRST and derives everything (tempo, key, template, kit) from a mood
    table, displayed on a generated green sleeve with a seeded catalogue
    number. This prototypes the API every game actually wants from generative
    music — "give me tension", "give me victory lap" — and could retrofit the
    whole radio family as a mood-keyed library.

