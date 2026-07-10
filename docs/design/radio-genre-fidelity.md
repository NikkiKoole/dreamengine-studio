# Radio genre-fidelity — the gap ledger

STATUS: EXPLORING (gap ledger) — genre-true ideals vs what we built, per station and aggregated; a ledger of opportunities.

What would a *genre-true* version of each radio station need, what did we actually
build, and where are the holes? This is a gap analysis across all 20 stations on
[`runtime/radio.h`](../../runtime/radio.h) — design exploration, not a how-to.

> **Genre: design exploration.** A ledger of *opportunities*, not a bug list. The
> "ideals" below are real-genre targets imagined by a music expert blind to our code; a
> fantasy console doesn't owe anyone a real saxophone. Read every hole as "here's a
> direction this station could grow," weighed against the fact that square waves doing
> neon is a feature, not a failure. The companion [`radio-voices.md`](../guides/radio-voices.md)
> is the *what-plays-what* inventory; this is the *what's-missing-vs-an-ideal* overlay.

## How this was made

Two passes, deliberately firewalled:

1. **Blind genre research.** For each station a music expert wrote the ideal generative
   recipe for the genre — instruments, harmony/rhythm/form brains, signature techniques —
   working only from the genre itself (Wikipedia, gearspace, musicology), *never* reading
   our source. These are aspirational: a real Italo record, a real dub plate, a real
   Mulatu Astatke cut.
2. **Cross-ref against the impl.** A second pass diffed each ideal against the actual cart
   (`tools/carts/<name>.c`) and the engine's instrument shelf, tagging every gap as
   **missing** (not there at all), **faked** (the role exists on a cheaper wave than the
   genre wants), or **partial** (there but thinner / approximated than the ideal asks).

The discipline this enforces: the verdicts aren't "is it good music" — they're "how close
is this to *that specific genre*, and is the gap an absence, a substitution, or a thinning."

This pairs with the **intent-first voice brief** in
[`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md) (imagine the
band first, then shop the palette) and the **instrument palette** in
[`../guides/instrument-recipes.md`](../guides/instrument-recipes.md) (the recipe shelf the
holes below keep pointing at). The recurring punchline: most "faked"/"missing" instrument
holes have a *real engine sitting unused* — the untapped shelves
(`PIANO`/`PIPE`/`BOWED`/`VOICE`/`GUITAR`/`REED`) that [`future-stations.md`](future-stations.md)
flags as the highest-demand build targets.

---

# Aggregate findings (the payoff)

## The instruments we keep missing or faking

Across the 20 stations the same handful of timbres come up short again and again — and in
almost every case **the engine already models the thing**, it just goes unreached. These
are the untapped shelves from [`instrument-recipes.md`](../guides/instrument-recipes.md),
quantified by how many stations would benefit:

| untapped engine | stations that want it (faked/missing) | the role it would fill |
|---|---|---|
| **`INSTR_VOICE`** (formant/choir) | citypop · house · jangle · yacht · roadhouse · bossa · gamelan · tango · ymo · ambient · motorik · exotica | the single most-absent timbre on the dial — vocal hook (house talkbox, jangle slacker vocal), backing-vocal stacks (citypop/yacht), wordless choir/pad (ambient/ymo/exotica/motorik), the frontman (roadhouse Morrison), sindhèn (gamelan), cantor (tango). **No station uses it.** |
| **`INSTR_PIANO`** (struck stiff string) | satie · cocktail · tango · roadhouse · carlos · ambient · exotica | the genre-defining grand (satie, cocktail) or the missing acoustic body (tango, ambient). Faked on TRI/SINE everywhere; `satie` is flagged the upgrade fossil. **No station uses it.** |
| **`INSTR_EPIANO`** (Rhodes/Wurli) | citypop · house · lowend · yacht · roadhouse · jingle · ambient | THE harmonic bed of city pop / yacht / Rhodes-loop boom-bap. Faked on FM/TRI; a real Rhodes model sits ready. |
| **`INSTR_REED`** (blown waveguide) | citypop · lowend · dub · addis · yacht · carlos | the smoky sax/horn the genre lives on (citypop break, yacht alto cry, addis tenor, dub horns); melodica (dub); inner-voice woodwind (carlos). Faked on SAW/SQUARE/PD. |
| **`INSTR_BOWED`** (bowed/pizz waveguide) | lowend · cocktail · addis · tango · exotica · bossa · gamelan | the woody upright pizz (lowend's #1 hole, cocktail, tango, exotica) and arco strings (tango violins, addis masenqo, gamelan rebab). Faked on SINE/TRI/SAW. |
| **`INSTR_PIPE`** (blown flute) | bossa · exotica · ymo(-adjacent) | the breathy flute/alto-flute lead (bossa, exotica Theme B). Faked on SINE. |
| **`INSTR_GUITAR`** (KS + body) | motorik · jangle · jingle · addis · bossa | the real jangle/drone/krar string — `GUITAR` is more body than `PLUCK` and **no station uses it**; jangle defaults to a TRI fake with the real string one keypress away. |

**The headline:** the dance/synth cluster is timbrally faithful (its oscillators *are* the
genre's instruments), but every **acoustic, jazz, world, and vocal** station fakes its
load-bearing timbre on a cheap wave while the modeled engine waits unused. Closing the
instrument holes is overwhelmingly a *wiring* problem, not an *engine* problem — which is
exactly why [`future-stations.md`](future-stations.md) ranks the voice/piano/reed-forcing
stations (Eno, Plantasia, Reich, Afrobeat) as the highest-value next builds.

## The generative brains we keep missing

Timbre holes dominate, but a smaller, more interesting set of **missing mechanisms** recurs
— these are net-new brains worth building once and reusing:

- **Arrangement-dynamics terracing** — the single most-common *brain* gap. The
  pre-chorus/kick-drop/riser (italo), the stripped breakdown (italo, dub-style for others),
  the quiet spoken-word terrace before the final eruption (roadhouse), the rubato "lagoon"
  drop-out (exotica), the soft ritard/tape-stop ending (jingle, addis dissolve, gamelan
  suwuk). Many stations layer/subtract density but few have a *programmed dramatic
  drop-and-reentry*. A reusable "section terrace" brain (named breakdown / pre-chorus /
  ritard-out events) would close holes in 6+ stations.
- **Tempo as a voice beyond tango** — the drum-led elastic accelerando/ritardando that
  *lands on a downbeat* (gamelan kendang → gong, the Doors jam build). `tango.c` proved
  TEMPO AS A VOICE via live `bpm()`; nothing else borrows it, and gamelan's fixed tempo is
  its biggest brain hole.
- **Incommensurable / prime-length loop phasing** — ambient's defining engine is *missing*:
  it runs a finite synced 16-chord loop on a 60-BPM grid instead of mutually-prime per-voice
  loops in absolute seconds. This is the Eno/Reich phase brain
  ([`future-stations.md`](future-stations.md)) and ambient is the station that most needs it.
- **Per-chord chord-scale matching in solos** — cocktail's improviser runs one global major
  scale regardless of the chord underneath (no altered-dominant over V7, no Lydian over
  maj7). A "pick the scale from the current chord quality" pass would deepen every
  improv.h/solo.h station.
- **Microtonal bend / detune as expression** — addis has zero pitch-bend toward Ethiopian
  intonation (the "crying" inflection); gamelan nails per-degree tuning but pins the octave
  to 1200¢. Adaptive/expressive microtuning (the barbershop/raga/pedal-steel scout note)
  is a brain the world stations want.
- **Real functional harmonic *motion*** (vs a repeated vamp) — yacht and several others cycle
  one 4-bar loop where the genre wants a ii-V-I machine with secondary/backdoor dominants and
  deceptive resolutions. citypop/cocktail/bossa/tango have the richest harmony brains; the
  pop-with-jazz-bridge stations (yacht, jingle) mostly plane fixed loops.
- **Production-as-instrument FX** — tape echo (motorik's biggest hole; a real `echo()` bus
  sits unused), shared global tape-wow pitch drift (jangle/jingle warble per-voice, never
  together), chorus/reverb/spring-crash/Big-Knob-HP-sweep (jingle, dub, yacht). The engine
  lacks a true reverb bus, so some of these are genuine engine gaps, not just wiring.

## The fidelity spread

**Biggest gaps (least faithful — most faked/missing load-bearing identity):**

1. **ambient** — wrong skeleton: a beat-grid finite loop instead of no-pulse prime-phasing,
   *and* every signature timbre (piano, Rhodes, choir) faked on saw/sine/noise. Misses both
   load-bearing pillars (timbre + the phasing engine).
2. **citypop** — best-in-class harmonic/rhythmic *brain*, but the timbres are almost entirely
   cheap raw waves: Rhodes, FM/DX7 sheen, sax, strings, and the trademark backing-vocal
   stacks all missing or faked despite EPIANO/FM/REED/BOWED/VOICE all being available.
3. **yacht** — strong brain (mu chord, Purdie shuffle, gear change) but the two iconic voices
   (Rhodes, sax) faked on FM/square, and organ, mallet color, and stacked vocals absent.
4. **jingle** — composes like Mac DeMarco (real song-template harmony brain) but doesn't yet
   *sound* woozy: no real chorus, no shared tape-wow, no pad/EP dusk-color; cheap-wave voices.
5. **lowend** — strong structural brain but the genre-critical upright bass is a SINE and the
   jazz Rhodes a TRI when real BOWED-pizz and EPIANO exist; horns + DJ scratch absent.

**Most faithful (short entries, holes are color/polish):**

1. **satie** — brain is excellent; the *only* real gap is the felt-piano timbre (TRI faking the
   unused `INSTR_PIANO` it's literally flagged to upgrade to) + pedal-bloom approximation.
2. **carlos** — brilliantly authentic voice-leading-first species-counterpoint brain and the
   core fat-Moog timbres; gaps are wanting 3–4 voices vs the built 2, and real fugue/ritornello
   forms.
3. **motorik** — the unbroken pulse, pedal-point drone, modal planing, and additive process-form
   are exemplary; the gaps are two production signatures (tape echo, drone guitar) with real
   engines unused.
4. **dub** — high fidelity on everything that defines dub (bass-as-song, one-drop, the
   dub-engineer drop-out/echo-throw brain); gaps are FX color (melodica timbre, Big-Knob sweep,
   spring crash) and missing horns/vocal-shards.
5. **tango** — the two make-or-break moves (bellows bandoneón + marcato/síncopa/rubato) and the
   harmony/form/chan-chan brain all land; gaps are faked strings/piano and the three-schools blend.

The pattern in both lists: **fidelity correlates with how much of the genre is a *brain* vs a
*timbre*.** Genres defined by structure (satie, carlos, motorik, dub, tango, cocktail) score
high because our generators are strong; genres defined by *sound* (citypop, yacht, ambient,
jingle) score lower because we fake timbres on cheap waves. This is the inverse of the
[`future-stations.md`](future-stations.md) citypop-conditions finding (condition 4:
"gloss-native timbres" — our oscillators do neon, not smoky/acoustic).

## Top new generative-brain opportunities

1. **A reusable section-terrace brain** (named breakdown / pre-chorus kick-drop / riser /
   ritard-out events) — closes the most *brain* holes across the most stations.
2. **Incommensurable prime-length loop phasing in absolute time** — the Eno/Reich engine;
   ambient is built wrong without it and it's "nearly free" per the parking lot.
3. **Drum/conductor-led elastic tempo that lands on a structural beat** — extend `tango.c`'s
   live-`bpm()` TEMPO-AS-A-VOICE to gamelan's kendang→gong and the Doors jam.

---

# Per-station gap entries

Each entry: genre · ideal-in-a-nutshell · verdict · holes (`kind` · want · status ·
how-to-close). Stations roughly ordered most→least faithful at the top of each cluster.

## italo — Italo disco (Gazebo, Den Harrow)

**Ideal:** machine-tight minor-key disco with a relentless octave-bouncing sequencer bass,
808 kit, and a minor→major chorus lift + semitone gear change.
**Verdict:** high fidelity on the core identity (sequencer bass, harmonic-minor tear, real 808,
Simmons tom fills, gear change, glossy pad/arp/lead); holes are arrangement dynamics and two kit pieces.

- `instrument` · cowbell ride · **missing** · add an `SL_CB` on the unused `808/cowbell` recipe, sparse offbeat ride.
- `instrument` · snare layered with claps on 2 & 4 · **missing** · fire `808/snare` alongside the clap as a layer.
- `instrument` · sampled orchestra-hit · **faked** (FM brass stab stands in; no sampler exists) · push closer with a noise-burst + FM-cluster on the bar-turn.
- `instrument` · big detuned Oberheim brass stabs · **partial** · single FM cluster, no detune-VCO stack.
- `brain` · relative-MAJOR chorus pivot · **partial** · only the semitone gear change lifts; let the chorus start on bIII / recolor tonic to maj7.
- `brain` · pre-chorus kick-drop + riser · **missing** · add a 1-bar pre that kills the kick and ramps a riser into the existing crash.
- `brain` · stripped breakdown (bass + claps only) · **missing** · add an `S_BREAK` gating everything but bass+clap, crash-led re-entry.
- `technique` · fixed recurring lead riff-hook + instrumental break · **partial** · pin a lead motif per seed; give it a feature section.
- `technique` · continuous pitch-bent Simmons rolls · **partial** · stair-step is good; a note_glide bend would match literally.

## house — French house / filter disco (Daft Punk, Stardust, Cassius)

**Ideal:** recycle one disco loop, then *filter it* — slow resonant LP sweep over 8–16 bars +
hard kick-synced sidechain pump.
**Verdict:** high fidelity on the two non-negotiables (the filter ride IS the form; the pump
breathes) and the recycle-one-loop brain; the gaps are timbral and the stutter/swing approximations.

- `instrument` · sampled disco BAND loop (Rhodes/clav/brass/wah-guitar/strings) · **faked** (two saws) · layer `INSTR_EPIANO` Rhodes + a clav/funk-guitar voice; engines all available.
- `instrument` · talkbox/vocoder/chopped vocal hook · **missing** · voice a seeded pentatonic phrase on `INSTR_VOICE`, filter+pump it (lowest priority — "optional" in the ideal).
- `technique` · stutter/beat-repeat chopping the loop at builds · **partial** (drums-only) · machine-gun `I_STAB` on 1/16 in the build's last bars before THE VOID.
- `brain` · swung 16th hats (~54-58%) + ghost-note funk syncopation · **partial** · add swing offset to odd 16ths; ghost-note chucks in the loop voice.

## citypop — Japanese city pop (Tatsuro Yamashita, Mariya Takeuchi)

**Ideal:** jazz-functional extension harmony (royal road / JTTOU) over a tight 16th disco-funk
groove with an athletic octave-pop bass, Rhodes bed, FM sheen, sax, strings, and stacked vocals.
**Verdict:** best-in-class harmonic/rhythmic *brain*, but the timbres are almost entirely cheap
raw waves — the defining Rhodes/FM/sax/strings/backing-vocal stacks are missing or faked.

- `instrument` · Fender Rhodes comp bed ("THE harmonic bed") · **missing** · add an `I_RHODES` on `INSTR_EPIANO` epiano/rhodes — the single biggest win.
- `instrument` · FM/DX7 bell intro + tine sheen · **missing** · FM bell figure on the intro + an FM tine layer.
- `instrument` · real horn section + smooth sax solo · **faked** (SAW brass, no sax) · route the break solo to `INSTR_REED` alto/tenor sax.
- `instrument` · stacked 4-6 part backing-vocal harmonies · **missing** · a 3-voice `INSTR_VOICE` "ooh" stack on the chorus.
- `instrument` · sweeping real string section · **missing** · a saw/string-machine pad swelling on the chorus turnaround.
- `instrument` · FM+Pro-One sub-layer bass (the Yamashita trick) · **missing** · layer a SINE/FM sub under the bass on chorus bars.
- `instrument` · fingerstyle electric-bass tone · **partial** (plays right on TRI; cheap timbre) · `INSTR_PLUCK`/`GUITAR` for pluck character.
- `instrument` · shaker/tambourine/claves + vibraphone color · **missing** · trivial NOISE shaker on off-16ths; mallet vibes.
- `instrument` · clean chorused lead/fill guitar · **partial** · funk-comp covered; no separate lyrical guitar fills.
- `brain` · tritone sub + relative-key modulation · **partial** · only the +2 gear change; add a bII7 sub template + relative-minor path.

## motorik — Krautrock / motorik (Neu!, Stereolab)

**Ideal:** an unbroken fill-free 4/4 over a single held chord; build by layering, not chord
changes; tape echo and a drone guitar as the production signatures.
**Verdict:** high fidelity on the core — the motorik pulse, pedal-point drone, modal planing, and
additive process-form are excellent; missing the two production/texture signatures the recipe insists on.

- `technique` · Conny-Plank tape echo on snare + guitar · **missing** (the single biggest gap) · a real `echo()` bus exists unused — dotted-8th time, slapback send on the snare, deeper send on the drone.
- `instrument` · Rother drone/rhythm guitar (the shimmer) · **missing** · `INSTR_GUITAR` is unused by any station; add a held/strummed steel layer into the echo.
- `instrument` · doubled female-vocal "ooh" pad (Stereolab) · **missing** · low-mixed `INSTR_VOICE` pad tracking the drone (optional in the ideal).
- `instrument` · acoustic-kit timbre + kept micro-timing · **partial** (synth kit, deliberately quantized) · at minimum a slapback echo send for the ghost-8th.
- `technique` · Stereolab 2-4 chord lounge loop variant · **partial** · commits to the Neu! single-drone end; a seed-rolled cycling maj7/add9 loop covers the named variant.

## cocktail — Piano-trio cocktail jazz (Vince Guaraldi, Oscar Peterson)

**Ideal:** a three-voice trio — acoustic grand + walking upright + brushed kit — with rootless
ii-V-I-and-tritone-sub harmony, 60-66% swing, and a jazz-waltz / tag-ending vocabulary.
**Verdict:** strong structural and harmonic fidelity (real walking-bass generator, rootless
harmony, swing, brushes, head/solo/bass-solo/head-out form); the two defining *instruments* are faked.

- `instrument` · acoustic grand (struck stiff string) · **faked** (TRI/SINE) · re-aim onto the unused `INSTR_PIANO` piano/grand — highest-value fix; a piano trio without the piano engine.
- `instrument` · pizzicato upright bass · **faked** (TRI/SINE) · voice on `INSTR_BOWED` bowed/pizzicato.
- `technique` · two-handed block chords / locked-hands · **missing** · render a 4-5 note block voicing tracking the melody on head/solo peaks.
- `brain` · jazz waltz (3/4 lilt) · **missing** · a seed-rolled 3/4 grid (~25% of tunes), bass on 1, rolling-3 ride.
- `brain` · Latin/bossa A-section straight-8ths · **missing** · per-song feel flag zeroing swing on A, clave bass, swing on B.
- `technique` · the tag ending (loop the cadence 2-3×, held 6/9, rubato out) · **partial** · loop the final ii-V in S_OUTRO + a ride swell + stepMs ritard.
- `technique` · altered/Lydian/Dorian chord-scale matching in solos · **partial** (one global Ionian) · pick the scale from the bar's chord quality.

## lowend — Jazz-rap boom bap (A Tribe Called Quest, *The Low End Theory*)

**Ideal:** woody upright bass first and loudest, swung dusty sampled-break boom-bap drums, a
lo-fi chopped jazz Rhodes/horn loop over a static dark-minor vamp.
**Verdict:** strong structural brain (bass-first doctrine, swung boom-bap with a real b-boy break,
rotated modal vamp) but the defining timbres are faked.

- `instrument` · upright double bass, pizz (priority #1) · **faked** (SINE) · voice on `INSTR_BOWED` pizzicato tuned to G1-G2 — highest-value swap.
- `instrument` · chopped Rhodes/jazz-piano loop · **faked** (TRI) · move to `INSTR_EPIANO` rhodes, keep the tremolo.
- `instrument` · jazz horn stabs (muted trumpet / tenor sax) · **missing** · a horn slot on `INSTR_REED`/FM/PD firing on the HOOK section.
- `technique` · DJ scratching on hooks/transitions · **missing** · a fast pitch-bend sweep on a noise voice at section boundaries.
- `brain` · dark after-midnight minor (one Dorian/Aeolian mode) · **partial** (deliberate divergence — the cart models a specific brighter ATCQ record) · weight the pool toward m9/min11 if chasing the generic ideal.
- `technique` · 12-bit SP-1200 lo-fi degradation · **partial** (lowpass + drive, no bit-reduction) · apply a bitcrush/decimate if the runtime exposes one.

## bossa — Bossa nova (João Gilberto, Antônio Carlos Jobim)

**Ideal:** nylon-guitar batida + whispered vocal + soft two-feel upright + jazz extensions with
ii-V-I, tritone subs, and the One-Note-Samba melody trick.
**Verdict:** strong on the brain (the composition is genuinely bossa); the gap is timbral — the
defining nylon guitar defaults to a TRI fake, with flute/bass also faked and vocal/piano/kit absent.

- `instrument` · nylon classical guitar (violão) · **partial** (real `INSTR_PLUCK` nylon exists but ships OFF behind a keypress) · make PLUCK the default or seed-roll it.
- `instrument` · intimate whispered lead vocal · **missing** · an `INSTR_VOICE` "ooh/ah" lead behind the beat.
- `instrument` · breathy real flute · **faked** (SINE) · swap to `INSTR_PIPE` breathy flute.
- `instrument` · soft jazz-piano counter-line (Jobim) · **missing** (optional) · a sparse rootless-voicing piano line.
- `instrument` · brushed kit + soft surdo heartbeat · **partial** (shaker+rim only; no drum) · `INSTR_MEMBRANE` surdo on 1/and-of-2 + brushed ride.
- `instrument` · plucked upright double bass · **faked** (TRI) · `INSTR_PLUCK`/`GUITAR` pizz; double the guitar thumb an octave down.
- `technique` · behind-the-beat near-straight eighths · **partial** (only the lead lays back) · a small global timing offset on the batida + bass.
- `technique` · sustained chromatic descending bassline · **partial** (single approach notes only) · a descending-bass progression template.

## dub — Roots dub reggae (King Tubby, Augustus Pablo)

**Ideal:** bass-as-song over a one-drop riddim with offbeat skank + organ bubble + Far-East
melodica, then a *live dub-engineer mix* — drop-out muting, tempo-synced echo throws, Big-Knob HP sweep.
**Verdict:** high fidelity on everything that defines dub, especially the dub-engineer brain
(drop-out mixing, echo throws with hot feedback, the tape-bend warble); gaps are FX color and two missing techniques.

- `instrument` · Hohner melodica (breathy free-reed) · **faked** (filtered SQUARE) · voice on `INSTR_REED` (clarinet/oboe-leaning).
- `instrument` · horn section (tenor sax + trombone stabs) · **missing** · an `I_HORN` on `INSTR_REED` firing sparse stabs into the echo on transitions.
- `technique` · the "Big Knob" high-pass sweep · **missing** · step `FILTER_HIGH` cutoff up over a build then snap back on re-entry (instrument_filter supports it).
- `technique` · spring-reverb crash punctuation · **missing** (no reverb engine) · a metallic noise burst into the echo bus is the closest approximation.
- `instrument` · repeating vocal shards drowned in delay · **missing** (no sampler) · a short `INSTR_VOICE` blip thrown into the runaway echo.
- `instrument` · tube-amp palm-muted fat bass tone · **partial** (clean SINE, right tone) · a touch of drive/harmonics for body.

## jangle — Jangle / mixolydian slacker pop (Mac DeMarco, One Wayne G)

**Ideal:** wet-only vibrato guitar warble + mixolydian bVII→I harmony, lazy behind-the-beat
phrasing, shared tape-wow haze, and a bedroom FM/analog synth side.
**Verdict:** strong on the harmonic and warble identity; thin on the production-haze half — no
vocal, no bedroom synth (the whole One Wayne G side), no shared tape-drift, default guitar still TRI.

- `instrument` · lazy detuned lead vocal · **missing** · an `INSTR_VOICE` lead doubling the melody, pitched flat.
- `instrument` · vintage analog/FM pad + counter-melody synth · **missing** · a detuned FM/poly pad on the vamp chords — the modern One Wayne G half is unrepresented.
- `instrument` · single-coil electric guitar as a real string · **partial** (TRI default; real `INSTR_GUITAR` unused, PLUCK opt-in) · ship the real string by default.
- `technique` · shared global tape wow & flutter · **partial** (only tempo breathes; per-voice LFOs don't share) · one slow global pitch-drift LFO across all melodic slots.
- `technique` · un-synced free-running delay + reverb · **missing** · a free-rate delay (+ reverb tail) on the guitar/lead.
- `technique` · muffled sustain-killed guitar tone · **partial** · shorter decay + heavier damping + a master low-pass.
- `brain` · lush maj7/maj9/add9 + borrowed iv · **partial** (triads + optional dom7) · extend chord_pcs with 7th/9th tones.
- `brain` · two grooves (boom-bap + bossa shuffle) with swung hats · **partial** (only boom-bap, no swing) · a per-song groove roll + swing offset.

## jingle — Delicate songwriter pop (Mac DeMarco's dusk side)

**Ideal:** the same DeMarco woozy production at ballad tempo — jazz-pop recoloring with a
chromatic walking bass, CE-2 chorus on a tape-warbled single-coil, behind-the-beat phrasing.
**Verdict:** compositionally one of the most faithful stations (real song-template harmony brain,
borrowed-chord/chromatic-bass) but the *production* the genre lives on is largely faked on cheap waves.

- `instrument` · single-coil guitar via CE-2 into a tube amp · **faked** (TRI + LFOs) · move to `INSTR_GUITAR`.
- `technique` · CE-2 chorus layered on the tape wow · **faked** (one volume LFO) · a detuned-doubled voice + `instrument_tune` shimmer.
- `technique` · shared tape wow & flutter (one drift, all voices) · **partial** (independent fast per-voice LFOs; bass/kit don't drift) · a single shared slow LFO across all slots.
- `instrument` · dusk-color pad (Juno/Mellotron/organ) + EP · **missing** · a quiet `INSTR_ORGAN`/`INSTR_EPIANO` layer on chorus + delicate verses.
- `instrument` · second vibrato lead guitar (high-neck hooks) · **partial** (only the SINE lead/vocal stand-in) · a sparse `INSTR_GUITAR` lead with deeper LFO.
- `instrument` · doubled/harmonized intimate vocal + slap/spring · **faked** (mono SINE) · `instrument_tune` doubling + a second offset voice.
- `instrument` · muffled finger-picked P-bass · **partial** (round SINE, right brain) · `INSTR_PLUCK` for the muted pick thud.
- `brain` · soft ending (ritard / tape-stop / ringing final chord) · **missing** · ramp tempo down in S_OUTRO + one held final chord drifting out of tune.

## addis — Ethio-jazz (Mulatu Astatke)

**Ideal:** vibraphone-led modal jazz over a drone vamp in one Ethiopian qñit, with a crying
microtonally-bent tenor sax, hand percussion, and indigenous krar/masenqo accents.
**Verdict:** strong, genuinely modal core (real vibes, qñit-as-data, drone vamp, real membrane
percussion) but it fakes the signature sax and ships no microtonal bend, landing closer to "modal jazz with Ethiopian scales."

- `instrument` · warm breathy tenor sax · **faked** (PD synth-brass) · re-voice the horn chair onto `INSTR_REED` tenor/alto_sax — genre's #2 lead.
- `instrument` · krar (buzzy lyre) / masenqo (bowed fiddle) accents · **missing** · sparse `INSTR_PLUCK`/`GUITAR` krar + `INSTR_BOWED` masenqo in heads/intros.
- `technique` · microtonal vocal-style bends (azmari "crying") · **missing** · slight detune on leads + short pitch-glide/ENV_PITCH slurs into target notes.
- `instrument` · ride-led jazz kit under the hand percussion · **partial** (no kit; congas only — a defensible choice) · a sparse ride/brush layer under the congas.
- `brain` · anchihoye→solemn / ambassel→exotic / tizita default · **partial** (mode rolled uniformly, decoupled from tempo) · bias groove/tempo by qñit.
- `technique` · tracks dissolve on the endless vamp, not cadence · **partial** (outro restates head then hard-cuts) · thin to a bare looping vamp + fade.

## yacht — Yacht rock / AOR (Steely Dan, the Doobie Brothers)

**Ideal:** rootless extended ("mu" major) harmony with a smooth ii-V-I machine, the Purdie
half-time shuffle, melodic bass, and the iconic Rhodes + sax + organ + stacked-vocal sheen.
**Verdict:** strong on the brain (mu chord, Purdie shuffle, gear change, planing, melodic bass)
but thin on timbre fidelity — the two iconic voices are faked and the organ/mallet/vocals are absent.

- `instrument` · Fender Rhodes Mk I (stereo chorus / Leslie) · **faked** (FM tine — defensible DX-era too) · voice on `INSTR_EPIANO` rhodes; no stereo chorus exists.
- `instrument` · soprano/alto sax (the alto cry) · **faked** (PWM square) · voice on `INSTR_REED` alto_sax — the emotional-payoff instrument.
- `instrument` · Hammond B3 + Leslie / Wurlitzer · **missing** · a B3+slow-Leslie pad (`INSTR_ORGAN` has the registrations + rotor recipes) for chorus sustain.
- `instrument` · Latin percussion + mallet sheen · **missing** · a NOISE shaker/conga layer + an `INSTR_MALLET` vibraphone/glock accent on the hook.
- `instrument` · stacked 3-4 part jazz vocal harmonies · **missing** · an `INSTR_VOICE` pad doubling the pad's chord tones — the human center, absent.
- `brain` · the ii-V-I machine (chained ii-V, secondary/backdoor dominants, deceptive res) · **partial** (fixed 4-bar loop, no chaining) · a chord generator that chains ii-V + inserts dominants.
- `technique` · 4-note rootless 3-7-9-13 + common-tone voice-leading · **partial** (3-note voicings) · 4-note voicings + a common-tone pass.
- `technique` · production polish (chorus/plate/compression/Leslie) · **missing** (engine-limit — no reverb/chorus bus) · hardest to close; the EP tremolo is the one touch present.

## roadhouse — Modal psych-rock (The Doors)

**Ideal:** a Vox combo organ + Rhodes piano-bass from one player, modal one/two-chord vamps,
organ↔guitar solo trading, dynamic terracing, a Bach-pastiche frame, and a shaman frontman vocal.
**Verdict:** strong on the instrumental core (combo organ, one-man rig, modal vamps, a real
phrase-developing solo brain) but it omits the frontman — a great band with no shaman.

- `instrument` · Jim Morrison baritone vocal + spoken-word · **missing** (biggest absence) · an `INSTR_VOICE` chair on long held formant notes + a poetic breakdown line.
- `technique` · dynamic terracing with a hushed spoken breakdown before the eruption · **partial** (dynamics within a solo; flat macro-arc) · add a breakdown section dropping to bass+hat (+ spoken VOICE).
- `instrument` · marimba / honky-tonk piano color · **missing** · a cyclical `INSTR_MALLET` marimba (and unused `INSTR_PIANO`) in heads/breakdown.
- `brain` · Baroque circle-of-fifths organ intro/coda · **missing** · a short generated descending-fifths organ line at song start/outro.
- `instrument` · Rhodes piano-bass as a true e-piano voice · **faked** (generic FM) · the dedicated `INSTR_EPIANO` rhodes would be woodier.
- `technique` · Phrygian/Spanish flamenco + bottleneck slide · **partial** (mode reachable; no slide/nylon gesture) · a slide/portamento gesture + brighter nylon when phrygian.

## satie — Solo piano gymnopédies / gnossiennes (Erik Satie)

**Ideal:** solo felt piano only — modal parallel-planing extended chords, a hypnotic LH oom-pah
cell, a sparse breathing stepwise melody, generous pedal, micro-rubato.
**Verdict:** the brain is excellent — the alternating-pair pendulum, modal/parallel harmony, late
sparse melody, 3/4 furniture form, and rubato/ornaments all land. The one real gap is the instrument.

- `instrument` · soft felted salon piano · **faked** (TRI) · the modeled `INSTR_PIANO`/piano-grand sits unused and the docs flag satie as *the* upgrade candidate (it predates the engine).
- `technique` · sustain-pedal bloom (overlapping ringing tails) · **partial** (long single-note durations, no real overlap/resonance) · `INSTR_PIANO`'s longer release + a touch of space would let tails ring into each other.
- `brain` · barless free-rubato gnossiennes over a static drone · **partial** (forced into the same 3/4 oom-pah grid) · hold a drone bass + loosen the grid for gnossiennes.

## gamelan — Indonesian gamelan (Javanese & Balinese)

**Ideal:** the colotomic gong cycle first, per-session non-equal tuning, ombak paired-detune
shimmer, heterophonic irama elaboration + kotekan, all driven by drum-led elastic tempo.
**Verdict:** strong idiomatic core — tuning-as-data, the gong cycle, ombak, kotekan and
inverted-irama heterophony are genuinely nailed — but it's a fixed-tempo, percussion-only realization.

- `instrument` · rebab (bowed spike fiddle) leading the soft style · **missing** · an `INSTR_BOWED` rebab on continuous note_pitch float (like the suling).
- `instrument` · human voice (pesindhèn/gérong) · **missing** · an `INSTR_VOICE` sindhèn line over Javanese pieces — the legato anchor.
- `instrument` · gendér/gambang/slenthem distinct shimmer layers · **partial** (one shared bronze bank, no cengkok) · a second long-ring bronze bank + a wood gambang.
- `instrument` · sustained-metal gong + distinct kettle timbres · **partial** (struck stand-in; kenong/kempul share the melody bank; ketuk/kempyang absent) · distinct kettle voices.
- `technique` · drum-led elastic tempo + Balinese kebyar bursts · **missing** (biggest brain hole — fixed per-song tempo) · a per-step tempo curve slowing into the gong + a kebyar freeze/explode.
- `technique` · suwuk ending (ritard onto a final gong) · **missing** · ramp stepMs up on the last gongan, end on a held gong.
- `brain` · goal-directed cengkok elaboration toward seleh tones · **partial** (random kotekan contour) · steer the figure to the balungan landing tones.
- `brain` · stretched (non-1200¢) octave · **missing** (pinned to 1200¢, flagged v2) · per-octave tune slots or note_pitch float on the bronze.

## tango — Golden-age Argentine tango (D'Arienzo, Pugliese, Troilo)

**Ideal:** the bandoneón section breathing as one + marcato/síncopa interplay against a walking
bass, minor harmonic-minor harmony, arrastre, rubato, and a chan-chan ending — with a knob blending the three schools.
**Verdict:** high fidelity on the two make-or-break moves (bellows bandoneón + marcato/síncopa/rubato)
and the harmony/form/chan-chan brain; main gaps are faked strings/piano and the bolted-on schools.

- `instrument` · violin section on a real bowed engine (arco/pizz) · **faked** (SAW) · retarget to `INSTR_BOWED` violin + bowed/pizzicato.
- `technique` · chicharra/látigo + audible portamento slides · **partial** (chicharra is a noise burst; discrete re-pitch) · pitch-glide between canto notes on BOWED.
- `instrument` · grand piano (marcato LH + chromatic RH runs) · **faked** (TRI/SINE) · move to `INSTR_PIANO`; add RH chromatic fills.
- `instrument` · male cantor (estribillo) · **missing** · route the cancion refrain to an `INSTR_VOICE` cantor.
- `brain` · one knob blending the three schools on distinct axes · **partial** (split across an uncoupled timbre swap + booleans + intensity knob) · one "school" control co-driving timbre + síncopa bias + rubato + tempo + yumba.
- `technique` · arco bowed bass pedal tones (Pugliese) · **missing** · long held bass / a second `INSTR_BOWED` cello under heavy accents.
- `technique` · bandoneón close 3rds/6ths over the melody · **partial** (chord voiced, melody single-line) · double the canto a 3rd/6th below.

## carlos — Switched-On Bach / synthesized baroque (Wendy Carlos)

**Ideal:** Baroque counterpoint voiced one-line-per-timbre on a monophonic Moog — 3-4
independent voices, per-note hand-articulation, real fugue/ritornello/binary forms, terraced dynamics.
**Verdict:** brilliantly authentic on the brain (true voice-leading-first species counterpoint, ground
bass, canon-vs-free, no chords/drums/swing) and the fat-Moog timbres — but it's a TWO-voice cart where the genre wants 3-4.

- `brain` · 3-4 independent contrapuntal voices · **partial** (strictly two + a tonic drone) · a third generated voice with true 3-voice consonance/parallel checks.
- `instrument` · inner woodwind/reed voices (distinct hollow timbre) · **missing** · a narrow-PWM Moog patch (period-correct) or `INSTR_REED` oboe on the inner line.
- `instrument` · plucked/harpsichord patch for fast figuration · **missing** · a 4th slot with a fast-pluck envelope (a SAW pluck env stays Moog-authentic).
- `technique` · Baroque ornaments + leap-filling figuration · **missing** · an ornament pass (trill/mordent/turn) + a figuration pass filling leaps.
- `brain` · real large forms (binary/ritornello/fugue/chorale-prelude) · **partial** (one fixed chaconne+canon every time despite the titles) · a form selector that changes the bar plan + a true subject-answered-at-the-dominant fugue.
- `technique` · terraced thin-solo vs thick-organ-tutti at seams · **partial** (one pedal fade) · a stacked organ voice gated in/out by section.
- `instrument` · stacked-oscillator pipe-organ tutti · **faked** (single SAW tonic drone) · a detuned-stack / `INSTR_ORGAN` tracking the harmony.
- `technique` · slow vibrato LFO on the lead + per-note portamento glide · **missing/partial** · LFO_PITCH on the lead + a small seeded onset glide on leaps.

## exotica — Exotica / tiki lounge (Martin Denny, Les Baxter)

**Ideal:** vibraphone-as-strings lead + diegetic jungle calls + hand-percussion groove + lush
m6/6/9 chromatic-mediant harmony, with a flute foil, the Baxter choir/theremin wing, and a decorative-percussion shelf.
**Verdict:** strong, faithful Denny-combo exotica (vibes lead, aleatoric aviary, hand percussion,
lush harmony all land); the gaps are the missing second melodic voice, the absent Baxter wing, and a thin percussion shelf.

- `instrument` · alto flute / piccolo (trades with the vibes) · **missing** · an `INSTR_PIPE` breathy flute on Theme B / the jungle break.
- `instrument` · grand-piano comp (Denny) · **partial** (piano only replaces the vibes, never alongside) · a piano comp as its own slot (`INSTR_PIANO`).
- `instrument` · wordless female choir pad (Baxter) · **missing** · an optional `INSTR_VOICE` aah pad under the vibe solo.
- `instrument` · decorative percussion (güiro/log drum/marimba/conch/chimes) · **partial** (core only) · add a güiro rasp, log drum, marimba doubling.
- `instrument` · theremin (ghostly portamento swells) · **missing** · a sine/theremin voice swelling in the jungle break.
- `instrument` · plucked upright double bass · **faked** (TRI) · `INSTR_BOWED` pizzicato in bass register.
- `brain` · cha-cha/bolero patterns (tumbao, 2-3 son clave, güiro) · **partial** (simplified) · a real son clave + tumbao + martillo + güiro.
- `brain` · parallel planing of 6/9 voicings · **partial** (chromatic-mediant only) · a "plane" move shifting the held voicing ±2/3 semitones and back.
- `brain` · mysterious-mode coloring (harmonic-minor/Hijaz/whole-tone runs) · **partial** (melody is chord-tones only) · harmonic-minor/Hijaz passing runs over minor; whole-tone over dom9.
- `technique` · rubato percussion-silent "lagoon" passage · **partial** · a section variant dropping kit+bass to rubato vibes + birds + theremin.

## ymo — Techno-kayō / technopop (Yellow Magic Orchestra)

**Ideal:** a pentatonic "Oriental" hook over Western electro-disco — 16th sequencer bass,
lush maj7/9 chords, 808 + Syndrum kit, vocoder, and humanized MC-8 swing.
**Verdict:** strong on the melodic identity (yonanuki lead, Hosono counterpoint, Sakamoto
progression playbook, machine-tight form) but timbrally thin and harmonically too plain.

- `instrument` · CS-80/DX7 lush maj7-9 chordal PAD/comp · **missing** (the biggest timbral hole) · a 4th tonal slot holding maj7/9 voicings on saw/string-machine or FM epiano+bell.
- `instrument` · vocoder robot-choir voice · **missing** · a held `INSTR_VOICE` vowel-pad doubling the pentatonic hook.
- `instrument` · analog string-ensemble swells · **missing** · one held saw/string-machine voice (also serves the chord bed + chorus lift).
- `instrument` · 808 kit (sine kick boom + hand-clap + cowbell) · **partial** (CR-78 substitute — defensible early-YMO) · at least an 808 clap on 2&4.
- `instrument` · Syndrum descending-pitch laser tom fill · **missing** · a SINE with steep downward ENV_PITCH on the last bar of each 8-bar block.
- `brain` · never plain triads — maj7/min7/dom9/add9/maj9 · **partial** (only maj/maj7/min/min7; some plain triads) · add 9th/add9 qualities, swap plain-maj template slots.
- `brain` · relentless 16th-note machine bass · **partial** (masked 8ths) · gate to 16ths at top density.
- `technique` · MC-8 humanized micro-timing + ~54-56% hat swing · **faked** (flat ±2ms by doctrine) · per-step signed offsets + light swing on odd 16th hats.

## ambient — Beatless ambient (Brian Eno, *Music for Airports*)

**Ideal:** static modal pitch-pool harmony with NO pulse, incommensurable per-voice loops in
absolute time, reverb-as-composition, half-speed tape haze, and negative-space placement.
**Verdict:** a genuinely strong modal-drift harmony brain wrapped around the wrong skeleton — it
runs on a 60-BPM beat grid with a finite synced 16-chord loop, and fakes every signature timbre while real PIANO/EPIANO/VOICE engines sit unused.

- `technique` · incommensurable prime-length loop phasing (THE generative move) · **missing** · per-voice independent re-trigger periods of different prime lengths in *seconds*, decoupled from chord changes.
- `brain` · NO pulse/tempo/grid (notes in absolute seconds) · **faked** (metered 60-BPM grid) · drive changes off `timer()`/seconds, drop the bpm clock.
- `instrument` · felt grand piano, tape-slowed (the "1/1" timbre) · **missing** · an `INSTR_PIANO` felt-grand voiced an octave low for the sparse motif.
- `instrument` · Fender Rhodes shimmer · **missing** · an `INSTR_EPIANO` rhodes filling harmonic space.
- `instrument` · treated wordless choir / sustained vowels · **missing** (biggest upgrade) · the held chord on `INSTR_VOICE` with a fixed open vowel — held vowels phase beautifully.
- `instrument` · analog drone bed (string/pad synth) · **partial** (good sine+saw bed; not a dedicated string-machine) · least-urgent.
- `technique` · reverb-as-composition (8-20s tails) · **missing** (no reverb engine) · route bell+pad through `instrument_echo()` with high feedback as a smearing tail.
- `technique` · half-speed octave-down tape haze · **partial** (wow covered; no octave drop) · voice the lead an octave lower with softened attack.

---

*(20 stations, all on `runtime/radio.h`. Ideals are aspirational genre targets, not console
requirements — every hole is an opportunity. Keep this in sync with
[`radio-voices.md`](../guides/radio-voices.md) as stations gain the unused engines.)*
