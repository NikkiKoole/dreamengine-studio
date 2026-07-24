# Genre-box rosters — reimagining ReBirth per genre (and per era of gear)

STATUS: EXPLORING (2026-07-17) — a brainstorm with the maker while acidcandy was in flight, on
**what other opinionated "genre in a box" racks to build**, and **which modern/boutique hardware**
to homage. No cart committed; this is the candidate catalogue the next rack picks from. It extends
the rack program ([`tinyjam-racks.md`](tinyjam-racks.md)) and the RB-338 pilot
([`rebirth-classic.md`](rebirth-classic.md), shipped as `acidrack`/`acidcandy`) sideways into new
genres and new gear, filtered for what this engine can honestly synthesize.

> **The one idea.** ReBirth worked because `2×303 + 808 + 909` wasn't a shopping list — it was a
> *genre argument*: "acid techno in a box." So the roster **is** the opinion. A new rack = a new
> curated roster + a genre generator, near-free on the shared chassis (the
> [`tinyjam-racks.md`](tinyjam-racks.md) "a new radio ≈ a new app" thesis). This doc collects the
> rosters worth that box.

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program — lane format, seed-code
handoff, the trademark rule), [`rebirth-classic.md`](rebirth-classic.md) (the pilot + the
modern-feature backlog it already researched), [`yacht-rack.md`](yacht-rack.md) (the chord-first
rack), [`pocketbox.md`](pocketbox.md) (the Elektron p-lock/trig-condition workflow, already ported),
[`cart-library-direction.md`](cart-library-direction.md) §2d (the hardware-wing what-next),
[`../guides/instrument-map.md`](../guides/instrument-map.md) (the museum floor plan — the shelf's
coverage), [`device-face-paradigm.md`](device-face-paradigm.md) + [`candy-style.md`](candy-style.md)
(the two candy/device-face packagings a rack can wear), [`mic-and-sampling.md`](mic-and-sampling.md)
(the frontier that would unlock the sample-based genres).

## 0 · The engine filter (read this before proposing anything)

Two shelf rules decide what's buildable:

- **The sampler doctrine** ([`../guides/instrument-map.md`](../guides/instrument-map.md), the
  curatorial line): the museum takes **analog-circuit / synthesis machines only** — the engine has
  no sample playback, so a sample box (LinnDrum, SP-1200, MPC, Digitakt, Octatrack, Polyend, PO-33,
  EP-133, Volca Sample, TE samplers) would be a **caricature**. `mellotron` is the one licensed
  exception (faked in pure synthesis). The open "unless" clause — mic-equipped iOS sampling — lives
  in [`mic-and-sampling.md`](mic-and-sampling.md); until that lands, sample-core genres are blocked.
- **The Roland wing is complete** ([`../guides/instrument-map.md`](../guides/instrument-map.md)):
  303/606/808/909/SH-101/Juno/RE-201 all ship, plus the whole RB-338 rack. So the fresh ground is
  **modern boutique gear** and **non-Roland lineages** — that's where these rosters point.

So every roster below is scored on: (a) can the engine synthesize its voices? (b) how much is pure
assembly of *shipped* voices vs. one new engine capability? (c) is it a genuinely different
*mechanic* or just a re-skin of the 16-step grid?

## 1 · The three headline reimagines (from the brainstorm)

These are the "ReBirth 2026" candidates — opinionated all-in-one boxes, each a distinct answer.

### 1a · The modern-acid reimagine — closest to the original
**Roster:** Behringer **TD-3-MO** (Devil Fish 303) + **RD-9** (909) + a Perkons/DrumBrute-style
analog drum-synth. **The move:** everything the originals lacked, all of which
[`rebirth-classic.md`](rebirth-classic.md) already researched into `acidrack`'s `de:meta.todo` —
step probability, per-track length/**polymeter**, **motion/p-lock lanes**, sidechain **pump**,
separate **accent decay** + **sub-osc** + variable slide (the Devil Fish knobs).
**Status here:** half-built already — `acidcandy` shipped probability, the data-driven per-voice
**LANE** system, and the Devil Fish page. This reimagine is really "finish acidcandy's arrangement +
Devil Fish depth and call it its own thing." *Fit: trivial — it's the backlog realized.*

### 1b · The semi-modular Moog stack — freshest mechanics ★
**Roster:** Moog **DFAM** + **Subharmonicon** + **Mother-32** (the modern live-techno trinity),
patched together. **Why it's the exciting one:** it breaks the "everything is a 16-step grid" mold.
- **DFAM** ("Drummer From Another Mother") — a percussive semi-modular synth with an **8-step
  pitch+velocity sequencer** per row (pitch row + velocity row), not a trigger grid. You *tune* a
  drum part.
- **Subharmonicon** — two VCOs each with two **subharmonic** oscillators + **four polyrhythm
  generators** that clock the two sequencers independently. Drones and evolving polymeter, not
  bars. A genuinely novel mechanic for the shelf.
- **Mother-32** — the mono synth + a classic step sequencer + a patchbay glue.
- **Fit:** the Moog signal path already exists (`moog`, the `carlos`/`plantasia` radios ride it);
  the two *new* things are the **subharmonic-oscillator** voicing and the **polyrhythm clock** — both
  cart-side sequencer logic, likely no new DSP. Pairs beautifully with the **Grenadier** interest
  (`grenadier.c`, the Grendel RA-99 filterbank) — semi-modular, filterbank-adjacent, **performed on
  knobs/XY, not step-sequenced**. A "west-coast/east-coast techno patch rack."

### 1c · The Volca candy rack — most on-brand for where we are ★
**Roster:** Korg **Volca Bass** (303-ish 3-osc analog) + **Volca Beats** (808-ish analog drums) +
**Volca Keys** (3-voice analog poly) / **Volca FM** / **NuBass** (tube 303). **Why:** the Volca line
is the modern *budget, tiny, playful* acid-house trinity — literally toys — which is **exactly** what
the 160×100 candy device-face ([`candy-style.md`](candy-style.md),
[`device-face-paradigm.md`](device-face-paradigm.md)) was built for. It's the natural *sibling* to
`acidcandy`, not a competitor to `acidrack`. **Fit: excellent** — Volca Bass/Beats/Keys are all
analog and close cousins of voices we already have (303/808/Juno); it reuses everything acidcandy
just built (LANES, probability, the cartridge nav, the candy chrome). The Korg wing is currently
near-empty on the shelf (only `kaoss`), so this also *opens the Korg floor*. **Doctrine note:** skip
**Volca Sample** (it's a sampler).

## 2 · The genre-box rosters (roster = the opinion)

Each is a curated 3–4 machine box + its signature FX. "Fit" = how much is assembling *shipped*
voices vs. one new capability. Trademark: homage names free, original faceplate for anything paid
([`tinyjam-racks.md`](tinyjam-racks.md) §trademark; precedent `moog` ships as "dream synth").

| Genre box | Roster (real gear) | Signature FX | Fit / gap |
|---|---|---|---|
| **★ Detroit techno** *(Belleville / Model 500)* | SH-101 bass · TR-909 · DX100/DX7 (FM bass + "Strings of Life" bells) · Juno-106 pads | hall reverb, subtle chorus | **Pure assembly** — SH-101/909/FM/Juno all shipped. Highest fidelity-per-effort |
| **★ Electro** *(Drexciya / Cybotron)* | TR-808 or 606 · SH-101/Pro-One cold bass · **vocoder** (VP-330/SVC-350) · brittle FM lead | vocoder, short slap delay | **Strong + timely** — uses the new mic **vocoder** (`vocode`/`voxbox`); 808/606/Juno/FM shipped |
| **★ Dub techno** *(Basic Channel / R&S)* | one filtered **chord-stab** voice + a dubbed kick — an **FX/mixing-DESK box**, not a note box | **Space Echo** (`spacecho`) + spring/plate (`cathedral`) + **DJ filter** (`djfilter`) | **Strong** — the "performance genre gets the FX-ribbon variant" case; `dubdesk` is the seed. Architecturally different |
| **Trance / prog** *(classic Euro)* | JP-8000 **supersaw** lead + hoover · 303/Juno bass · 909 | big hall, gated verb | Needs **one engine add: the supersaw** (7 detuned saws) — but it's the whole genre, and unlocks hardcore too |
| **Hardcore / gabber** *(Rotterdam)* | distorted **TR-909 kick** · **Alpha Juno hoover** · screamer 303 | heavy **drive** (shipped) | Cheap *once the supersaw/hoover exists* — 909 + drive shipped; shares trance's one new voice |
| **Ambient / drone** *(Eno / BC's quiet cousin)* | Buchla **Music Easel** (`easel`) + **Subharmonicon** + **tape loops** (`tapeloop`) | **cathedral** reverb, tape | **Strong** — west-coast voices shipped; Subharmonicon the one new mechanic. The no-grid, no-drums pole |
| **Synthwave / Italo** | Juno-106 · DX7 bells · fat Moog bass · Linn-feel (fake on 808) | chorus, gated reverb | **Good, near-shipped** — retro-pop more than club, but a very sellable candy face |

**Doctrine-blocked for now** (sample-core — wait on [`mic-and-sampling.md`](mic-and-sampling.md)):
**jungle / DnB** (the Amen break), **UK garage / 2-step**, **big beat / breakbeat**, **trip-hop /
downtempo**, and **classic Chicago house's TR-707/727** (PCM drums). Enormous genres, but a synth
caricature of a chopped break would embarrass them. (Chicago house *can* be done with 808/909 +
303 + a DX/organ stab — it just isn't the 707 faithfully.)

## 3 · The wider hardware landscape (reference — what's in use, what fits)

Grounding for future rosters — contemporary house/techno gear, grouped, with the engine verdict.
Cross-referenced against the shelf's wings ([`../guides/instrument-map.md`](../guides/instrument-map.md)).

- **Moog (semi-modular)** — **DFAM · Subharmonicon · Mother-32 · Matriarch · Grandmother**. *All
  analog, all buildable* on the Moog path. The §1b stack. **Biggest fresh-mechanic opportunity.**
- **Korg** — **Volca line** (Bass/Beats/Keys/FM/NuBass = §1c; Sample/Drum blocked) · **Minilogue /
  Monologue / Prologue** · **MS-20 mini** · **opsix / drumlogue** (FM) · **Electribe**. Wing nearly
  empty (only `kaoss`) — lots of open floor.
- **Behringer clones** — **TD-3-MO** (Devil Fish 303) · **RD-8 / RD-9** (808/909) · semi-modular
  **Crave / Neutron** · the **2600 / Model D / Pro-1 / Wasp** clones. The affordable roster most
  bedrooms actually own; §1a rides these.
- **Arturia** — **DrumBrute (Impact)** (the modern *analog* drum machine; Steiner filter = our
  `FILTER_STEINER`) · **MicroFreak** (digital osc + analog Steiner filter, the weird-house darling) ·
  **MiniBrute / MatrixBrute**. Analog-friendly, on-trend.
- **Erica Synths** — **Perkons HD-01** (the current-darling analog drum-synth + performance
  sequencer) · **LXR-02** · **Syntrx**. Perkons is a live-techno star and a clean gap.
- **Moog/Make Noise/Buchla west-coast** — **0-Coast · Strega · Easel** (`easel`/`lpg` shipped). The
  ambient/drone vein.
- **Elektron** — **Digitone** (4-op FM — engine exists) · **Syntakt** (analog+digital) are
  buildable; **Digitakt / Octatrack** are sample-blocked. The killer **p-lock + trig-condition +
  probability** workflow is *already ported* in `pocketbox` — the sequencer brain is on the shelf.
- **Roland modern** — TR-8S · SH-4d · **Aira Compact** (T-8/J-6/E-4) · MC-707. *Wing complete* — only
  build these if a specific box adds a mechanic (e.g. the 8S's per-voice motion, already in the
  acidrack backlog).
- **Novation / UDO / Dreadbox / Jomox / Vermona** — Bass Station II · Peak · Super 6 · Nymphes ·
  Jomox **AlphaBase** (analog 909-ish) · Vermona **DRM1** (analog drum synth). Analog drum-synths
  (Jomox/Vermona/DRM1) are a recurring gap the DrumBrute/Perkons rosters also want.
- **Sample-based (blocked until the mic frontier)** — TE **EP-133 / PO-33** · **Polyend Tracker /
  Play** · **SP-404** · Digitakt/Octatrack · Volca Sample. Huge in house *right now*; parked on
  [`mic-and-sampling.md`](mic-and-sampling.md).

## 4 · Reading the field (recommendations)

Two patterns decide priority:

- **Almost-free rosters** (pure assembly of shipped voices — proves "a new rack ≈ a new app"):
  **Detroit techno**, **synthwave**, **ambient drone**, and the **Volca candy rack** (§1c). Cheapest
  wins; the work is the *box + generator*, not the sound.
- **One capability unlocks a wing:** **electro** rides the new **vocoder**; **trance + hardcore**
  both fall out of a single **supersaw** engine add; the **Moog stack** (§1b) needs only a
  **subharmonic osc + polyrhythm clock** (both cart-side) to open a whole fresh-mechanic lineage.

**Pointing, if forced:**
- **Cheapest, most on-brand:** the **Volca candy rack** (§1c) — reuses all of acidcandy, opens the
  Korg wing, made for the candy face.
- **Freshest / most exciting:** the **Moog semi-modular stack** (§1b) — genuinely new mechanics
  (subharmonic + polyrhythm) and the natural home for the **Grenadier** XY/filterbank performance
  idea.
- **Highest fidelity-per-effort:** **Detroit techno** (§2) — assemble the legends, near-zero new
  sound work.
- **Uses the new mic work:** **Electro** (§2) — robotic vocals over an 808.

## 5 · Open questions

- **Candy face vs. accordion vs. FX-desk per roster.** Grid genres (Detroit, electro) fit the
  [device-face](device-face-paradigm.md)/candy shape or the [`acidrack`](rebirth-classic.md)
  accordion; **dub techno** wants the **mixing-desk / FX-ribbon** variant (no note grid — the
  instrument is the send knobs); the **Moog stack** and **ambient** want a **patch/knob** surface
  closer to `grenadier`/`easel` than to a step sequencer. The roster should pick its own body-plan.
- **Which get a seeded generator?** ReBirth's magic was the box; the tinyjam magic is *generate →
  play → export*. Ambient/drone and the Moog polyrhythm box are the hardest to "generate" musically
  — do they get a generator, or are they pure performance instruments?
- **`rack.h` extraction.** Still deferred at customer #2 ([`tinyjam-racks.md`](tinyjam-racks.md));
  a third-and-fourth *different-shaped* rack (an FX-desk, a patch box) is exactly the pressure that
  decides whether there's shared code or just a shared idiom.
- **Supersaw is the one recurring engine ask** — it gates trance, hardcore, and a lot of rave/
  progressive. Worth its own small showcase cart first (the "prove the voice as its own cart" rule
  from [`radiophonic-workshop.md`](radiophonic-workshop.md)) before a rack leans on it.
- **Trademark / faceplate naming** per roster — original names for anything paid; homage in
  `lineage`/`homage` metadata ([`tinyjam-racks.md`](tinyjam-racks.md) §trademark).
- **A worked roster already designed:** [`bossa-rack.md`](bossa-rack.md) — the chord-bloom rack
  is a concrete instance of this "genre box per style" thinking taken to a full design.
