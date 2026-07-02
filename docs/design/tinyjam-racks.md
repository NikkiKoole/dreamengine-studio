# Tinyjam Racks — ReBirth-style genre racks: generate → play → export

> A **rack** is one *shape* of Tinyjam — the genre-sequencer shape. The other shapes: **instrument
> modules** (Strumharpy/omnichord, FM, martenot…) and **effects / playables** (kaoss, theremin).
> See [`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §2 + §3.8. (Renamed from "tinydaws" 2026-06-29.)

STATUS: TWO RACKS LIVE (2026-07-02) — `acidrack` (rebirth-classic, the pilot:
[`rebirth-classic.md`](rebirth-classic.md)) **and `yachtrack`** ("session desk", the first
CHORD-FIRST rack: [`yacht-rack.md`](yacht-rack.md)), both design→cart in a day. Generate →
play → export proven end to end, and **the seed-code handoff FROM A RADIO is now proven too**
(yachtrack runs yacht's `new_song` verbatim; a 44-pair golden corpus gates it in `spec()`) —
the milestone rebirth-house was slated for. The per-lane humanize question is answered
at the MODEL level (feel knobs on players — yacht-rack.md), but the maker's first-play
verdict is that the shipped surface is "a tad too indirect" — the knob-heavy UX is under
review before it's copied to rack #3 (yacht-rack.md open questions, top item). Still open: the shared lane format /
`rack.h` (deferred at customer #2 — the two racks share an idiom, not code; the third rack
decides) and the song.h export tier. Rest of this doc = the program. Follow-up brainstorm (bias knobs, the
agentic RLHF loop, the Game&Watch dancer, MIDI-in): [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md).
Companion reading:
[`../guides/game-music.md`](../guides/game-music.md) (the generation recipes this
builds on), [`radio-instrument-options.md`](radio-instrument-options.md) (the band
panel — the first "open the radio up" affordance), [`audio-notes.md`](audio-notes.md)
(engine surface), [`instrument-engines.md`](instrument-engines.md) (the engine
catalog), [`touch-notes.md`](touch-notes.md) (these must be touch-first),
[`product-notes.md`](product-notes.md) (the commercial lens: sketch-first
decision, MIDI/AUv3/latency parking lot, and the **trademark rule** — Roland
names/faceplates in the rack table below are fine free, but nothing
Roland-skinned crosses a paywall; paid racks get original identities).

## The idea in one paragraph

ReBirth RB-338 worked because it was *opinionated*: 2×303 + 808 + 909 wasn't a DAW,
it was "acid techno in a box." A **tinydaw** is that, per genre, built from carts we
already have: the instrument carts supply the voices, the radio carts supply the
genre brains, and a shared rack chassis supplies the sequencer + performance UI.
The arc the user actually wants:

1. **Generate quickly** — what the radios do today (SPACE → a new song).
2. **Play with it musically** — like loopstation + the individual instruments, but
   as a *group*: the generated song lands in a visible, editable sequencer; tap
   cells, mute lanes, smear an XY lead pad over it, swap voices live.
3. **Export** — get the result OUT: audio for a DAW, a code to share with a human,
   or song data another game cart can `#include` as its soundtrack.

The third step is the deep one: the console grows a **built-in soundtrack
workshop**. A kid making their first game gets a real house track in it without
writing a note.

### Scope & direction (confirmed by the maker, 2026-06-29)

One app **per radio** — the app's whole job is to *tweak the radio's output*. That makes the
**radio library the app pipeline**: each radio you already have is a latent rack, turned
inside-out (radio = genre brain; rack = the editable surface). The radios grow the line two ways at
once — **breadth** (a new radio ≈ a new app, near-free on the shared chassis) and **depth** (tweaking
frozen output is also how you debug/improve the generator — the dev loop in
[`tinyjam-racks-followup.md`](tinyjam-racks-followup.md) §2). Two deliberate limits:

- **A curated *few*, not all radios** — pick the ones that earn a rack; chasing every station is the
  completeness trap. Pilot ONE end-to-end first (rebirth-house, per Build order below).
- **The UI adapts to the genre** — not every radio wants a note grid. Editing genres (yacht/drill)
  get the step sequencer; performance genres (dub) get the mixing-desk / FX-ribbon variant. The radio
  decides what kind of app it deserves.

(Distribution note: "one app per radio" is the *web/gallery* shape — each rack is its own cart/URL,
the free funnel. As a paid product the same set bundles into one hub app + an IAP per rack — see
[`product-notes-followup.md`](product-notes-followup.md) §4. Same content, two packaging shapes.)

## Why this is cheap here (ground truth from the code)

- **All instruments are recipes over one engine.** The 808 isn't custom DSP — it's
  `instrument()` slot setups + macro-tuned `schedule_hit()` calls into `sound.h`'s
  engines (`INSTR_FM/PLUCK/MALLET` + waves). A rack cart doesn't *embed* other
  carts; it copies their voice recipes into its own slots. No audio plumbing.
- **The radios already encode genre idiom as code.** bossa, dub, house, exotica,
  citypop… each station's `new_song()` + `play_step()` IS the genre knowledge. A
  tinydaw is a radio turned inside out: same idiom, but the player drives.
- **A song is already a tiny data struct.** house.c's seeded `Song` is ~30 ints
  (chord loop, bass mask + degrees, stab onsets, riff cell). Same seed → same
  struct, forever (the radio.h seed-compatibility rule).
- **But half the groove is NOT data.** Four-on-the-floor, clap on 2&4, the hat
  breathing — those are hardcoded `if (step % 4 == 0)` logic inside `play_step()`,
  gated by section/density. The radio *performs* the groove; it never *writes it
  down*. That gap is exactly what a tinydaw closes.

## The design move: generation writes lanes, playback reads lanes

The keystone is an explicit **lane format** — editable pattern state the whole
pipeline flows through:

```
generator (new_song, same srnd stream)  ──writes──▶  lanes
player taps / XY pad / band panel       ──edits───▶  lanes
save_bytes()                            ◀─persists─  lanes
playback (dumb: read lanes, schedule_hit)◀──reads──  lanes
export (WAV / song code / song.h / MIDI)◀──reads──  lanes
```

Sketch (rebirth-house sized — tune per genre):

- ~8 trigger lanes × 16 steps: kick, clap, hat-c, hat-o, shaker, bass, stab, riff.
  Drum lanes are on/off (+ maybe accent); pitched lanes carry a note per step
  (bass/riff) or read the chord row (stabs).
- the chord loop row: 4 bars × (root pc, quality) — the harmonic spine, voice-led
  at playback exactly like the radios do (`rad_lead_to`).
- per-lane: mute / volume / voice pick (the band panel generalized — see
  `radio-instrument-options.md`; the rack is its natural home).

The generator fills the lanes from the seed — the four-on-floor gets *written into
the kick lane* instead of living as an `if`. Then every cell is a button: tap to
toggle, the groove keeps playing. The seed is the roll; once you edit, *your*
version is the truth (→ `save_bytes()`).

### Sections become pattern banks (the elegant part)

The radios' arrangement (intro/groove/build/drop in house) is currently density
logic. In the rack it becomes **pattern slots A/B/C/D** — the generator fills A
sparse, B groovy, C with the kick-roll build, D full drop. That's literally
ReBirth's pattern banks + song mode, and it makes the radio's hidden arrangement
visible and stealable. A pattern-chain row = song mode. (v1 can ship with just one
pattern — banks are the v2 step.)

### The seed is a song code

The handoff between radio and rack: a u32 seed, shown as 8 hex chars on the radio
dial face. Type it into the matching tinydaw and the exact song you were hearing
lands in the sequencer, opened up. Typeable codes are also *shareable between
humans* — very fantasy-console. Caveat (the one dangerous part, per radio.h): the
rack must copy the station's `new_song()` srnd call sequence **verbatim**, or
codes won't match. Acceptance test = the same trace-diff used for radio.h
migrations.

This implies a small radio-side change: stations grow a "show seed" affordance
(they already keep seed history for [ / ]).

## Export — three tiers, ascending ambition

1. **WAV (for a DAW)** — mostly exists. The harness renders byte-reproducible
   audio (`play.js --wav`) and any running cart honors the `.bake/wav_request`
   live-capture trigger (see [`../guides/debug-harness.md`](../guides/debug-harness.md)).
   A rack "export" button just arms it. **Stems** = solo one lane per pass, render
   again — a loop, not a feature.
2. **Song code (for humans)** — free once seeds are visible. Only describes the
   *unedited* roll; edited songs need tier 3 or a save.
3. **Song data (for other games)** — the deep one. The flattened lane state is
   just bytes; pair it with a tiny read-only player header — **`song.h`, the
   playback half of radio.h** — and any game cart can `#include` a soundtrack the
   author generated in the radio, tweaked in the rack, and exported (as a baked C
   array or a `save_bytes` blob + loader). MIDI for external DAWs is then a
   `tools/` script converting the same blob — it never needs to live in the cart.

The lane format is load-bearing for all of this: design that one struct well and
every stage decouples.

## The genre rack list (curated, mapped to real carts)

From a brainstorm session (an outside agent supplied the vibes; mapping to actual
codebase is ours). Each = one cart, own faceplate identity, same chassis.

| Rack | Vibe | Lead | Chords/comp | Bass | Drums | Whimsy/FX |
|---|---|---|---|---|---|---|
| **rebirth-classic** | acid techno — THE homage: 2×303 + 808 + 909, the RB-338 itself | 303 #1 (slide/accent acid line) | — (acid barely moves: one minor chord for bars) | 303 #2 | 808 + 909 banks side by side | the 303 filter knobs ARE the show |
| **rebirth-house** (pilot) | disco/house | riff (I_STAB-ish) | stabs, voice-led | house bass | 808 bank | filter sweeps |
| **yacht-rack** (→ promoted to [`yacht-rack.md`](yacht-rack.md) 2026-07-02 — the first chord-first rack: chart + interpreters, answers this doc's humanize question) | yacht rock / AOR (Steely Dan) | sax/synth/gtr-solo chair | FM epiano mu-chord comp (the chord ROW is the show) | fingered electric + approach runs | session/Purdie/CR-78 drummer chairs | MU-IFY / SUS-MELT flavor buttons |
| **Spaghetti Western** | desert noir | musicalsaw/otamatone whistle (sine + drift — exists) | INSTR_PLUCK "tic-tac" baritone | sh101 sub | 808 cowbell/woodblock | jew's-harp twang (BPF saw, new ~10-liner) |
| **Broken Fairground** | carnival macabre | stylophone buzz | omnichord, limping strum | 303-as-tuba (short decay, low) | 808 kick/snare + swing | steam calliope (detuned sines + noise puff, new) |
| **Ancient Library** | dungeon synth | hurdygurdy drone+buzz | INSTR_FM "square-lute" | INSTR_MALLET stone marimba | 909 tuned −2 oct (war drums) | ghost choir (low sines, reverb) |
| **Space-Age Bachelor Pad** | exotica | INSTR_FM vibraphone | omnichord sus4 | sh101 walking bass | 808 congas/maracas/rim (or CR-78!) | slide whistle (sine overshoot, new) |
| **Radiophonic Workshop** (→ promoted to [`radiophonic-workshop.md`](radiophonic-workshop.md) 2026-07-02 — engine-unblocked; leads = heldnotes theremin / martenot / musicalsaw) | vintage sci-fi | theremin (saw/otamatone voice) | rapid 8-note arp | 303 heavy slide | procedural noise clicks/pops | wow/flutter on the FM out |
| **Toy-Town Glitch** | circuit-bent nursery | formant voice (genuinely new synthesis) | stylophone + bitcrush | "rubber duck" sh101 snap | 808 pitched way up | music box (tri + clockwork ticks) |
| **Cozy Adventure** (cozy-cluster · parked) | cozy cartoon-adventure / lo-fi exploration (evokes AT/cozy-games, no IP) | PD soft chiptune (pulse+tri), songful | omnichord shimmer + nylon/uke pluck | pdbass round bounce (softened axe-bass) | soft toy/chiptune kit, gentle | mallet/celesta bells; wandering songwriter brain |

Pattern-source pairings: Space-Age steals from `exotica.c`'s pattern vocabulary,
rebirth-house from `house.c`, etc. Western/Fairground/Library need new generators —
but `game-music.md`'s brain catalog (chord brains, groove templates, melody cells)
is exactly the kit for writing them.

The genuinely-new sound work across the whole table: a handful of ~10-line voice
recipes (calliope, slide whistle, jew's harp, music box) + one real project (the
formant voice). Everything else exists.

**Cozy Adventure notes** (added 2026-06-29): a later, marketing-driven addition
rather than one of the original eight — the centrepiece of the "cozy cluster"
(Strumharpy / lofi / plantasia) and mostly a recombination of shipped engines.
Full design seed (band, brain, the original cute face, the IP guardrail,
tribe/channels): [`tinyjam-cozy-adventure.md`](../marketing/tinyjam/tinyjam-cozy-adventure.md);
go-to-market context in [`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §3.10.

**rebirth-classic notes** (added 2026-06-07; **promoted to its own design doc 2026-07-02:
[`rebirth-classic.md`](rebirth-classic.md)** — full research pass, slot math, 320×240 accordion
layout, the seeded generator, the 303-filter fidelity plan, and the case for taking the pilot
slot from rebirth-house): the only rack where every voice is
already a shipped cart (tb303 ×2, tr808, tr909) — zero new sound work, maximum
homage. What it lacks is a generator: there is no acid station, and french house
(house.c) is a *different idiom* — acid's 303 pattern IS the song, slide/accent
placement is the craft, harmony barely moves. So it needs a small new
`new_song()` (seeded 16-step acid lines + slide/accent rolls) — arguably the most
fun generator in the set to write. Pilot trade-off: classic is the purer homage
with simpler harmony; house keeps the pilot slot because its generator already
exists and is battle-tested. Both stay — they are not the same genre.

## The wider map — mining the existing parking lots (2026-06-07)

The repo already holds three relevant wishlists; this section cross-indexes them
through the tinyjam racks lens so nobody re-derives it. Sources:
[`../guides/game-music.md`](../guides/game-music.md) → "Style cheat-sheet" + "The
brain catalog", [`future-stations.md`](future-stations.md) (the station
parking lot, split out of game-music 2026-06-07),
[`instrument-engines.md`](instrument-engines.md) → §8.9 candidate engine catalog,
[`audio-notes.md`](audio-notes.md) → §12 instrument gaps + §14 machine readiness,
[`radio-instrument-options.md`](radio-instrument-options.md).

### Stations already planned but unbuilt → the racks they'd become

| Station (parked where) | Rack configuration (lead / chords / bass / drums / whimsy) | Blocker |
|---|---|---|
| **lofi jazz** (cheat-sheet "idea") | pentatonic noodle / FM rhodes m9 / soft bass / dusty swung kit / vinyl crackle | **none** — it was waiting on a rhodes; `INSTR_FM` epiano shipped since |
| **chiptune action** (cheat-sheet "idea") | square 25% lead / power-chord planing / tri bass / noise kit + `euclid()` fills | none; PD engine would make it sing |
| **gamelan** (brain catalog "future": colotomic cycles + kotekan interlock) | FM bronze metallophones — ombak beating IS FM's inharmonic partials | two new *time brains* (design work, no engine) |
| **Italo disco** (audio-notes §12 "planned batch") | FM epiano / Juno-style pads / octave bass / 909 circuits / CZ reso sweep | pads want second-osc infra (§12 gap 2b) — fakeable via drawn SCW |
| **motorik / krautrock** (brain catalog "future": the process form) | one-chord vamp / phasing arps / driving 8th bass / machine kit | none — a *form* brain to write, the band exists |
| **IDM / Aphex** (brain catalog "future": volatility grammar) | detuned pads / broken drill patterns | hard design, zero engine work |
| **hard-bop combo** (the brain catalog's worked combination) | FM brass (70ms attack) soloing via `improv.h` / walking bass / swing ride | "~150 lines of glue plus a face" — the guide's own words |

The striking row count: **lofi jazz, hard-bop, motorik, gamelan and chiptune are
unblocked today** — they were waiting on engines that have since shipped.

### New instruments ranked by tinydaw leverage

instrument-engines §8.9 re-scored by *how many racks/stations each engine unlocks*:

> **Drift note (2026-07-02): most of this table has since SHIPPED** — voice/formant
> (`INSTR_VOICE`, 2026-06-10, + the `formant()` vowel filter), PD (`INSTR_PD`, 2026-06-08),
> membrane (`INSTR_MEMBRANE`, 2026-06-08), bowed (`INSTR_BOWED`, 2026-06-09), reed
> (`INSTR_REED`, 2026-06-08) and brass (`INSTR_BRASS`, 2026-06-10). AM/ring's *sound* shipped
> as the `ringmod()` bus effect (2026-06-14); an engine version would only add the
> note-tracking modulator ratio. The last two rows are covered *cart-side* but not as engines:
> **Additive**'s small 2–3-partial family shipped as the `mt70` cart (2026-06-07 — stacked note
> slots at exact `note_pitch` ratios, the proof that "two oscillators" is a recipe); the **Juno
> second-osc** stays open as infra (§12 gap 2b, the live saw+pulse blend) but the `juno` cart
> shipped anyway (2026-06-10) on stacked sub-osc hits + the master `chorus()` (BBD, shipped same
> day). Rows kept as written for the unlock rationale; ship state is owned by §8.9 itself.

| Engine | Cost (per §8.9) | Unlocks |
|---|---|---|
| **AM / ring mod** | "~10 lines native" | the Radiophonic Workshop's signature sound (Dalek/Delia), Toy-Town glitch, gamelan shimmer. Cheapest item on the list |
| **Voice / formant** | near-free (rides the §8.3 SVF) | Toy-Town's Speak-and-Spell lead, Radiophonic + Ancient Library choir "aah", the doo-wop direction below |
| **PD (Casio CZ)** | "cheapest in the catalog", buffer-free | chiptune action, Italo basses, the famous reso sweeps; §8.9 already calls it "strong identity fit" |
| **Membrane** (tabla/conga) | mallet-family cost | the whole world-percussion axis: afro-cuban/salsa station, dancehall, fourth-world; upgrades exotica + tango racks. §8.9: "world-music radio fuel" |
| **Bowed string** | buffer packs into existing `ks_buf` | **the Morricone strings the Western rack wants**, tango violin upgrade, dungeon-synth string section |
| **Reed + Brass** | zero new buffer architecture | the horn section: hard-bop sax, a ska/rocksteady station, klezmer — the Broken Fairground's missing voice |
| **Additive** | medium | subsumes the sine/MT70 family with live macros; organ/bell/choir pads |
| **Juno second-osc** (§12 gap 2b) | the expensive one — Voice infra | Italo, a synthwave/outrun station, halves every pad's polyphony cost |

### Sets written down nowhere else (until now)

Each = existing brains + at most one new engine:

- **Surf / spy** — PLUCK tremolo-picked lead + the Western's spring-reverb feel +
  tom-heavy kit. Sits next to Spaghetti Western; could share its rack chassis.
- **Ska / rocksteady** — the offbeat skank is dub's brain at double tempo; organ
  bubble exists; wants the brass engine (shipped 2026-06-10 as `INSTR_BRASS` — unblocked).
- **G-funk** — the whistle lead is *literally* the musicalsaw voice (sine +
  portamento); boom-bap groove already in `lowend.c`.
- **Synthwave / outrun** — gated-reverb snare via SFX-editor sculpting (the §12
  909-hihat technique), Juno pads, arp; the `outrun` cart donates the faceplate
  aesthetic.
- **Doo-wop / street corner** — the formant-engine showcase: four formant voices
  voice-led by `rad_lead_to`, finger snaps. Nothing like it on the dial. (Engine shipped —
  `INSTR_VOICE` even carries the consonant/coda set for "sha-la-la" syllables; buildable today.)

Related: a batch of **whimsical playable-instrument cart ideas** (slide whistle,
melodeon, accordion, harmonica, upright bass) lives in
[`cart-library-direction.md`](cart-library-direction.md) §2b — each doubles as a
future rack voice (the reed family especially: Fairground, a future folk/forró
set), and the upright bass + `walk_note()` is the hard-bop rack's bass lane.

### The market lens — the tinyjam-marketing re-ranking, folded in (2026-07-02)

> **Maker stance, first and load-bearing: the market lens is an INPUT, not the decider.** "I also
> just want to make things I want to see." Build-for-love is a first-class criterion here (it's
> the north star's soul bar, not a consolation tier) — so this section *informs* the pick, it
> doesn't make it. Doo-wop can beat lofi on a given day because the maker wants to hear it.

[`tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §3.6–3.8 re-ranks this whole
space by *tribe* (size × passion × findability) and lands on a structural insight: **the craft
ranking (future-stations.md, this doc's wider map) and the market ranking barely overlap** — the
engine's proudest carts (gamelan/carlos/addis) are the weakest products, and the "dessert" carts
(italo, lofi) sit on the richest tribes. The useful move is hunting the **rare overlaps**: tribe ×
shipped generator × cheap on the chassis. The next-rack shortlist through that triple lens:

1. **rebirth-house** — the structurally correct rack #2 regardless of market:
   [`rebirth-classic.md`](rebirth-classic.md)'s own words, "classic proves the rack chassis; house
   proves the radio handoff" — the seed-code pipeline is still unproven and house is its designed
   test. Also Tier-S tribe (house producers + the ReBirth-mourners community). As `rack.h`'s
   second customer it's maximally *different* from acid, which is what makes extractions good.
2. **lofi** — §3.7's layup: an enormous audience already conditioned to one looping visual
   (Lofi Girl), generator shipped, near-zero new craft. The natural debut for the
   [followup](tinyjam-racks-followup.md) §3 dancer — the rack whose *video export is the product*.
3. **vapor + italo** — the shareability pair, both stations shipped. Vapor is the
   performance-genre showcase (the mixing-desk/FX-ribbon rack variant this doc already plans —
   varispeed/tape/grains ARE the genre, all ride-safe today). Italo brings the outrun faceplate
   + the synthwave tribe; its Juno-pad gap is fakeable with a drawn SCW (audio-notes §12).
4. **braindance / IDM** — the rare craft×tribe overlap §3.7 flags. Where the followup's **bias
   knobs** debut naturally: that tribe wants to steer generators, not tap cells.
5. **dub** — cheapest rack in the space: `dubdesk.c` nearly *is* the mixing-desk variant already.
6. **doo-wop** — unblocked by the 2026-07-02 drift pass (`INSTR_VOICE` consonants can sing actual
   "sha-la-la"). No established tribe — the honest build-for-love pick, and the formant showcase.

Adjacent, from §3.8: **`turing`** (a real Eurorack tribe; rack-adjacent — a Turing-machine *lane
mode* is a cheap generative filler idea) and the **"expressive playables"** cluster (theremin /
martenot / glassharmonica / otamatone…) which is a *different product shape* — gestural, not
sequenced; the capture answer for those is looper-as-notation, recorded in
[`radiophonic-workshop.md`](radiophonic-workshop.md) §4.

### The takeaway for build order

The cheapest *complete* new tinydaw stack was **AM/ring + formant** — and both have since
shipped (see the drift note above: `INSTR_VOICE` is the formant engine, `ringmod()` covers
the ring-mod sound), so the **Radiophonic Workshop** rack is unblocked end-to-end today —
fittingly, the one genre that *celebrates* sounding like a fantasy console. Meanwhile the
zero-engine stations (lofi jazz, hard-bop) would each hand a future rack its
generator for free.

The door-type ranking (formant = biggest single door, reed = biggest cart
family, membrane = the rhythm-section unlock, AM/ring = best per-line) is
recorded in [`instrument-engines.md`](instrument-engines.md) §8.9, right under the
engine catalog it scores — that doc owns the "which engine next" call.

## Universal control layout (touch-first)

Standardize across racks so learning one = learning all:

- **the grid** — 16-step lanes (tap cells; the heart of the tool)
- **the play-pad** — a big XY pad for the lead voice (theremin/saw/stylophone),
  scale-locked
- **the twist** — genre scale lock (Western = phrygian dominant, Space-Age = major
  pentatonic…) so anything smeared on the pad sounds idiomatic
- **transport + pattern bank row** — play/stop, A–D, chain
- radio.h-style extras carried over: tempo, tone knob, band panel, seed display

Must be designed at touch sizes from day one — this rides the current touch pass
on the instrument carts (`touch-notes.md`, `mobile-web-notes.md`).

## Shared chassis: rack.h (the radio.h move, again)

Like radio.h — a toolkit, not a framework: lane state struct + generic fill/edit/
playback helpers + the grid/pad/transport draw-and-input blocks. Cart keeps
update()/draw() and its own faceplate art. Same second-customer rule: extract it
while building rack #2, not before (house pilot first, then Western proves the
chassis).

## Build order

1. **Pilot: rebirth-house.** Hardest handoff constraint (seed-compat with a
   shipped station) + the richest existing generator. Proves lanes, seed code,
   tap-editing, WAV export.
2. **House radio grows the seed display** (tiny, independent — could land first).
3. **Second customer: Spaghetti Western** → extract `rack.h`. First *new*
   generator written lane-native.
4. **song.h + export blob** once the lane format has survived two genres.
5. The rest of the table as appetite dictates; MIDI converter script whenever.

## Open questions

- **Lane count / step resolution per genre** — 16 steps fits house; does the
  fairground's swing or satie's 3/4 need a different grid? (radio.h's clock
  already does 3/4 — satie.) Maybe steps-per-bar is a rack constant, not 16.
- **How much arrangement to expose** — full A–D banks + chain in v1, or one
  pattern and let the radios keep owning arrangement?
- **Edited-song interchange** — can a song edited in the rack go *back* to the
  radio? (Probably no — radios stay generators; the rack is downstream. But the
  seed code travels both ways for unedited songs.)
- **Per-lane humanize** — the radios' performance randomness (`rnd(2)` timing,
  ghost notes) lives outside the seed by design. Does the rack expose it as a
  "humanize" knob per lane, or bake the radios' values?
- **Where does loopstation fit** — overdub-recording the XY pad into a lane is
  the obvious crossover (see [`input-recording-looper.md`](input-recording-looper.md)).
- **Save format vs export blob** — one format for both (`save_bytes` blob = the
  song.h input), or keep save internal and export explicit?
