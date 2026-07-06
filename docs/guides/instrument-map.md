# The instrument museum map — what we have, and which doc says so

*(2026-07-05)* One page answering **"what instruments does dreamengine have, and which file
is the source of truth for each question?"** The instrument knowledge is deliberately spread
across several docs, each owning one altitude (the shelf, the recipes, the presets, the
engines, the backlog) — this map is the hub over all of them. **It points, it never
duplicates**: if a cart list here and in a linked doc ever disagree, the linked doc wins.

## Which doc do I open?

| Question | Open | What it owns |
|---|---|---|
| **"Which cart makes sound? Which cart do I copy?"** | [`instrument-carts.md`](instrument-carts.md) | ★ THE SHELF — every sound cart (synths, machines, stations, showcases, toys), grouped by the chassis it copies. Start here for any "what do we have" question. |
| "Which sound *recipe* do I grab?" | [`instrument-recipes.md`](instrument-recipes.md) | the supply-side palette: every grabbable recipe (showcase presets, Roland drum voices, whimsical patches), organized by engine. |
| "Which *effect* settings?" | [`effects-recipes.md`](effects-recipes.md) | the effects cookbook (echo/reverb/chorus/flanger/tape/wah/drive/crush), with showcase cart + used-by lists. |
| "What patches do the radio stations use?" | [`instrument-presets.md`](instrument-presets.md) + [`radio-voices.md`](radio-voices.md) | the named-patch catalog and the per-station voice charts (slot → role → preset) that read over it. |
| "Which `INSTR_*` engine for which timbre? What engines are unbuilt?" | [`../design/instrument-engines.md`](../design/instrument-engines.md) | the engine port program — §8.9 is the candidate-engine catalog; the shelf has the one-line engine table. |
| "How do I build a new station / music cart?" | [`radio-station-howto.md`](radio-station-howto.md) | ★ the ordered runbook; [`cart-authoring-prompt.md`](cart-authoring-prompt.md) is the imagine-the-band-blind firewall, [`game-music.md`](game-music.md) the chord/clock brains. |
| "What instrument should we build next?" | [`../design/cart-library-direction.md`](../design/cart-library-direction.md) | §2b = scored single instruments, §2c = liveset playthings (complete), §2d = **hardware libraries — which wing next**. Plus [`../design/future-stations.md`](../design/future-stations.md) (unbuilt radios) and [`../STATUS.md`](../STATUS.md) #21 (the parked museum shortlist) and [`../design/sound-next-steps.md`](../design/sound-next-steps.md) (the actionable digest). |
| "Where are we faking a sound a real engine could do?" | [`../design/radio-genre-fidelity.md`](../design/radio-genre-fidelity.md) + [`../design/radio-instrument-options.md`](../design/radio-instrument-options.md) | the gap ledger and the per-station timbre-swap candidates. |
| "The sound API itself?" | [`../design/audio-notes.md`](../design/audio-notes.md) | the sound HUB (it tops with the audio-family index); [`../design/held-notes.md`](../design/held-notes.md) = the sustain layer, [`../design/midi-and-keybed.md`](../design/midi-and-keybed.md) = how notes get played. |
| "The groovebox / rack program?" | [`../design/tinyjam-racks.md`](../design/tinyjam-racks.md) | radios-as-song-generators feeding editable racks; [`../design/rebirth-classic.md`](../design/rebirth-classic.md) (acidrack), [`../design/yacht-rack.md`](../design/yacht-rack.md) (yachtrack), [`../design/pocketbox.md`](../design/pocketbox.md) (PGB-1). |
| "How do I port a voice from navkit?" | [`porting-from-navkit.md`](porting-from-navkit.md) | the verbatim-oscillator discipline + gotchas. |
| "Which carts are mono that should be poly / what plumbing is shared?" | [`../design/multitouch-and-generalization-audit.md`](../design/multitouch-and-generalization-audit.md) | the cross-shelf interaction audit. |

## The hardware wings — homages by lineage

The curated "museum floor plan": which real-hardware families the shelf covers, at a
glance. Cart-by-cart detail (what each is, how it's built) stays on the
[shelf](instrument-carts.md); the what-next assessment lives in
[`cart-library-direction.md`](../design/cart-library-direction.md) §2d.

| Wing | Shipped | Parked / missing |
|---|---|---|
| **Roland** | `cr78` · `tr808` · `tr909` · `tr606` (Drumatix, the 303's silver sibling — shipped 2026-07-06) · `tb303` · `sh101` · `juno` (Juno-6) · `spacecho` (RE-201) — plus `acidrack`, the whole ReBirth RB-338 rack. The wing is **complete**. | — |
| **Korg** | `kaoss` (Kaoss Pad); the MS-10 exists only as a bass preset (`saw/ms10-bass`). | the **Volca line**, MS-20, Monotron, Electribe — assessed in §2d. |
| **Moog** | `moog` (the dream synth); the `carlos` and `plantasia` radios ride its signal path. | — |
| **Buchla / west coast** | `easel` (Music Easel) · `lpg`. | — (the §2c vein is complete). |
| **Eurorack / boutique** | `euclid` (Mutable Instruments Grids) · `turing` (Music Thing Turing Machine) · `grenadier` (Grendel RA-99) · `modrack`; `dubdesk` fuses three of them. | — |
| **Pocket / groovebox class** | `pocketbox` (Wee Noise Makers PGB-1) · `tracker` (LSDJ / Dirtywave M8) · `groovebox` · `drummachine` · `onenote` · `loopstation`. | **Teenage Engineering Pocket Operators** (PO-20 Arcade the best fit) — §2d. |
| **Casio** | `mt70` (Casiotone MT-70); the `INSTR_PD` engine *is* the CZ sound (`pd` showcase). | **VL-1** (parked in [STATUS](../STATUS.md) #21) · a **CZ-101** machine homage — §2d. |
| **Yamaha** | `fm` (the 2-op DX showcase). | **DX7** is engine-blocked (the 6-op/32-algorithm panel is scoped out in [`instrument-engines.md`](../design/instrument-engines.md)); CS-80 is the buildable candidate — [`cart-library-direction.md`](../design/cart-library-direction.md) §2d. |
| **Vintage keys** | `mellotron` · `solina` (ARP/Eminent) · `clavinet` (Hohner D6) · `epiano` (Rhodes/Wurli/Clav) · `organ` · `piano`. | — |
| **Oddities & toys** | `omnichord` (Suzuki) · `stylophone` (Dübreq) · `otamatone` (Maywa Denki) · `martenot` (Ondes Martenot) · `heldnotes` (the theremin) · `fartsynth`. | the **Raymond Scott pre-Roland wing**: Circle Machine / Clavivox / Electronium ([STATUS](../STATUS.md) #21). |
| **Pedals / studio FX** | `mistress` (EHX Electric Mistress) · `tapeloop` (Frippertronics) · `combo` (combo amp) · `djfilter` (XONE:92 / DJM) · `cathedral` (the reverb hall) · the auto-wah on `clavinet`. | [`boutique-pedals-roadmap.md`](../design/boutique-pedals-roadmap.md) owns the pedal backlog. |
| **Dub / sound-system** | `dubsiren` (King Tubby) · `dubdesk` · the `dub` radio. | — |

> **The sampler doctrine** ([STATUS](../STATUS.md) #21's curatorial line): the museum takes
> **analog-circuit machines only** — sample-playback boxes (LinnDrum, SP-1200, SK-1, PO-33 KO,
> MPC) would be caricatures, since the engine has no sample playback. The `mellotron` is the
> one licensed exception: faked in pure synthesis, per
> [`recorded-timbres.md`](../design/recorded-timbres.md). The doctrine's open "unless" clause —
> what fantasy-console-like sampling *could* mean on mic-equipped iOS devices —
> is sketched in [`mic-and-sampling.md`](../design/mic-and-sampling.md).

**Beyond the hardware wings** the shelf also holds the acoustic/expressive instruments
(upright, fretboard, monochord, handpan, tabla, mouthharp, glass harmonica, hurdy-gurdy,
musical saw…), the 25+ radio stations, and the sequencer/note toys — all catalogued in
[`instrument-carts.md`](instrument-carts.md); this map doesn't re-list them.

## Maintenance

Ship a hardware homage → add its shelf row (instrument-carts checklist #6) **and** its wing
row here; new recipes/presets follow the shelf checklist's step 7 as usual. Run
`node tools/lint-docs.js` after.
