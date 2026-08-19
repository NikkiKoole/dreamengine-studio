# The analog outboard chain: a shared honest output stage

> **STATUS: BUILDING (2026-08-19)** ‑ shipped: [`runtime/outboard.h`](../../runtime/outboard.h) (the
> shared voicing table) · the `outboard` cart (three programmes, five presets, a HEADROOM switch and
> RMS/PEAK/CREST metering, with a 53-assertion `spec()`) · and
> [`tools/bypass-check.js`](../../tools/bypass-check.js), the reconvergence oracle §9's last item asked
> for, which then corrected two of this doc's own claims (§2c-bis, §4b). A second consumer,
> `sideman`, pins EQ+IRON as an organ cabinet. The **stereo plate** is the one piece that needs new
> DSP and is still open (§6).

The target, taken from the marketing copy of a commercial plug-in bundle, because it is a good
statement of what players expect a "mix bus" to be:

> *We meticulously modeled a classic analog outboard signal path: a snappy FET compressor, warm
> British equalizer and lush German plate reverb. The entire output chain is flush with analog vibes.
> (Yes, great effort went into modeling things like field-effect transistor distortion, odd and even
> harmonic coloration, transformer saturation and the non-linearities that generally add personality
> to a song and glue together a mix.)*

This doc records what of that we can back **today**, with what call, and where the copy would be
lying. It is the [ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) ("effects are
recipes, not primitives") treatment of a whole output chain, and its shipped form is the
[§E](effects-bus-architecture.md) move that produced `ampcab.h`: an outboard unit is a **preset
bundle of effects the engine already has**, pinned as the output stage, with ONE table so several
carts and apps cannot drift.

---

## 1. The truth table

| the claim | the shipped primitive | honest? |
|---|---|---|
| **snappy FET compressor** | `glue(bus, amount, atk_ms, rel_ms)` (`runtime/sound.h`, `sc_apply`) | **partly, and not in the way the word "compressor" implies: see §2 and the measurement in §2b.** A peak follower, `gain = 1 - amount*env`, real attack/release in ms, self-keyed. It DUCKS (takes ~4 dB of RMS at the hardest setting) but does **not** control peaks: measured, the crest factor goes UP. No threshold, no ratio, no knee, no makeup, no program-dependent release. |
| **warm British equalizer** | `eq(lo, mid, hi)` ±12 dB, plus `eq_inst(1, …)` + `FX_INST(FX_EQ,1)` for a second EQ in the same chain | **partly.** Bands are FIXED (<80 Hz / 80 Hz‑6 kHz / >6 kHz). No frequency select, no Q, no proportional-Q, no Pultec-style overlapping boost+cut on one band. |
| **lush German plate reverb** | Schroeder core (4 comb + 2 allpass + predelay): `reverb(size, damp)`, `reverb_bus(tank, size, damp)`, `reverb_bus_fx()` to shape the tail | **partly, and this is the weak stage.** The tank is **MONO**. A plate's whole signature is a wide, dense, bright tail with no room geometry; mono removes the "lush". |
| **field-effect transistor distortion** | `drive_insert(amount, DRIVE_*, mix)` | **as a waveshaper, yes. As a device model, no.** No bias point, no hysteresis, no frequency-dependent saturation. |
| **odd and even harmonic coloration** | `DRIVE_SOFT`/`DRIVE_HARD` are symmetric (odd only); `DRIVE_ASYM` is documented in `sound.h` as the "even-harmonic tube" | **yes. This one is fully backed**, and it is the single most defensible sentence in the copy. |
| **transformer saturation** | nothing today. But §6b prototypes a `DRIVE_VOICE_XFMR` that fits an existing extensible slot for ~10 lines and no new state | **no today, cheaply yes tomorrow.** The physics (low frequencies saturate first, the inverse of a Tube Screamer) measured a 49 dB frequency dependence no existing mode has. Still a voicing, never a model: no B-H hysteresis. |
| **non-linearities that glue a mix** | `glue` + the always-on master soft-clip | **yes, as behaviour.** |
| **"meticulously modeled"** | | **no.** Every stage here is a voicing over shared DSP. That is the house rule (ADR-0015), not a defect, but it is not modelling and must not be sold as modelling. |

Roughly 70% of that paragraph is already true if it is phrased as **voicing** rather than
**modelling**. See §7 for copy that stays inside the truth.

---

## 2. The FET insight: "no threshold" is not a gap

The obvious reading of §1's first row is that `glue` is a toy because it has no threshold and no
ratio. For a **FET compressor specifically, that reading is wrong.**

An 1176-class FET unit has **no threshold control at all.** The front panel is INPUT, OUTPUT, ATTACK,
RELEASE and a ratio button bank. You compress harder by **driving the input**, not by lowering a
threshold. So the real control set is:

| the unit's control | our call |
|---|---|
| INPUT (how hard you hit it) | a flat `eq_inst(1, g, g, g)` boost placed just before the comp (the EQ is the only effect that can BOOST) |
| RATIO 4 / 8 / 12 / 20 / all-buttons | a **bundle**: each button position selects an `(amount, attack_ms, release_ms, dirt)` tuple. `outboard.h` owns the table |
| ATTACK / RELEASE | `glue`'s own `atk_ms` / `rel_ms`, which are already real milliseconds |
| the FET's own dirt | `drive_insert` at a small amount, scaled up with the ratio (all-buttons-in is the dirty setting on a real one too) |

This is exactly the `ampcab.h` pattern: **the knob is voiced after the device, not derived from it**,
and saying so out loud is what keeps it honest. What genuinely stays missing is the
**program-dependent release** (a real FET unit's recovery is two-stage and level-dependent; ours is
one exponential coefficient) and the **knee**.

## 2b. What `glue` actually does, measured

The above is the encouraging half. Here is the number, rendered off the `outboard` cart's own loop
(`play.js --wav`, `wav-analyze.js`), all other stages out:

| | peak | RMS | crest |
|---|---|---|---|
| dry (rack out) | -0.06 dBFS | **-15.39 dBFS** | 15.34 dB |
| COMP in, 8:1 | -0.00 dBFS | -15.39 dBFS | 15.38 dB |
| COMP in, ALL | -0.01 dBFS | **-19.19 dBFS** | **19.19 dB** |

Read that carefully, because the naive reading is backwards. At the hardest ratio the comp takes
**3.8 dB of RMS** and the **peak does not move**, so the crest factor **rises by 3.9 dB**. A
compressor is supposed to do the opposite.

So `glue` is not a peak limiter and calling it one would be the lie. What it is: a **fast-attack,
slow-release ducker.** The transient outruns even the 1 ms attack and passes through; the follower
then clamps down across the 55 ms release and squashes the *body*. That is the mix "breathing as one
lump", which is exactly the character the function was built for and exactly what a bus comp is used
for musically. It just is not level control, and it cannot be level-matched (§5.2).

**Consequence for any UI on this stage: meter RMS, not peak.** A peak meter shows the comp doing
nothing, which is how you would wrongly conclude the stage is inert. The `outboard` cart meters RMS
with a peak-hold tick for this reason.

One more thing the "no threshold" design implies, which is easy to read as a bug: **`glue` keys off
ABSOLUTE level**, so a quiet mix compresses less. At the default 8:1 with a modest input boost the
input gain dominates and RMS goes *up* ~3.4 dB while the peaks duck; at the ALL ratio the ducking
dominates and RMS goes *down* ~3.8 dB. Both are the same stage behaving correctly. The INPUT control
is not a convenience, it is how the stage is played.

## 2d. ⚠ The engine moved under this measurement (2026-08-19, same day)

§2b and §2c were measured in the morning. By the afternoon a parallel change had given `glue` **two
things this doc had listed as missing**: automatic makeup gain (a slow running average of the gain the
stage applied, `gavg`, ~1.5 s) and a **program-dependent second release stage** (`env2`/`atk2`/`rel2`,
the deeper of the two recoveries winning). Re-measured on the same loop, same scripts:

| | peak | RMS | crest |
|---|---|---|---|
| rack out | -0.20 dBFS | -21.02 dBFS | 20.82 dB |
| COMP in, 8:1 (default) | | -20.63 dBFS | 20.43 dB |
| COMP in, ALL | -4.20 dBFS | **-18.70 dBFS** | **14.50 dB** |

**That is a compressor.** At the ALL ratio it now takes **6.3 dB off the crest factor** and hands
**+2.3 dB of RMS** back, where the morning's measurement had crest *rising* by 3.9 dB and RMS falling
3.8 dB. So §2b's headline (a ducker, not a limiter) and §5's gap 2 (no makeup) both describe the
engine **before** that change. They are kept above because the before/after IS the evidence that the
change did what it set out to do, and because a UI on this stage should still meter RMS.

**One open regression, found by re-running §4's test and reproducible (with the determinism control
clean: the same script twice is byte-identical).** On the current engine the **IRON** stage no longer
returns to bit-exact when switched out. It reconverges about **3 seconds later** (last differing
sample 4.076 s for a switch at 1.067 s), where in the morning it was exact at the switch. Isolated
with identical plate timing in both renders, so it is not the reverb tail. **Mechanism not pinned
down**, and it is not the obvious suspect: `glue(bus, 0, …)` still returns exactly 1.0 (`g = 1 - 0*e`,
and `gavg` is initialised to 1.0 at `sound.h:6107`, so the makeup is exactly unity), which the COMP
row confirms by still reconverging at the switch. Whoever owns the `glue`/plate change should run
`tools/bypass-check.js` (the oracle built for exactly this, from §9's open item) before calling it
done.

## 2c. Give the rack headroom, or none of it works

The single biggest practical finding from building the bench cart, and it is not a mixing nicety:

**Played at full tilt the demo loop peaked at -0.1 dBFS and the rack barely moved it.** The always-on
master soft-clip was absorbing what the rack did. Pulling the instrument levels down to about -7 dBFS
peak, with no other change, made every stage obvious:

| stage alone, held in | peak | Δ RMS vs dry | biggest per-window Δ |
|---|---|---|---|
| dry (rack out) | -6.30 dBFS | | |
| EQ (WARM) | -6.45 | +0.70 dB | +3.08 dB |
| IRON | -4.55 | +2.77 dB | +3.29 dB |
| COMP (8:1) | -2.57 | +3.41 dB | +4.64 dB |
| PLATE | -6.29 | +0.13 dB | **+2.27 dB** |

Note the PLATE row, which is why an average is the wrong measure for a reverb send: it barely moves
the overall RMS and lifts individual windows by over 2 dB. That is the tail filling the gaps between
hits, which is exactly the right signature. A stage that looks inert in a mean may be working
perfectly; measure where it is *supposed* to act.

**So: a mastering chain demonstrated on an already-clipped mix demonstrates nothing.** Any cart or app
pinning this chain has to leave it room to work.

### 2c-bis. Re-measured, and one claim above was too strong

The `outboard` cart now carries the trap **as a switch** (HEADROOM: ROOM / HOT, key `H`), because
being able to hear the rack stop working teaches more than quietly avoiding the state. HOT is not
"louder": it is **unity on every slot**, which is exactly what a cart sounds like when nobody set
`instrument_level` at all, since 1.0 is the default. That made the effect measurable properly, and it
**corrects the first sentence of §2c above.**

Measured 2026-08-19 (`play.js --wav` over 8 s of the GROOVE programme, `wav-analyze`, dry vs the stage
held in, nothing else changed but the levels):

| | ROOM (peak -6.58 dBFS) | HOT (peak -0.05 dBFS) |
|---|---|---|
| dry RMS | -25.56 dBFS | -15.25 dBFS |
| EQ (WARM) in: Δ RMS | **+0.55 dB** | **+0.27 dB** |
| EQ+IRON+COMP in: Δ RMS | **+10.30 dB** | **+7.53 dB** |
| EQ+IRON+COMP in: peak | -6.58 → -0.58 dBFS | -0.05 → -0.00 dBFS |
| EQ+IRON+COMP in: crest left | 18.98 → **14.68 dB** | 15.20 → **7.72 dB** |

So **"all four stages measured very nearly inert" and "the EQ moved RMS by 0.01 dB" could not be
reproduced** and should not be quoted. What is true, and is enough: the EQ's contribution **halves**,
the three-stage chain loses **2.8 dB** of its lift, and the crest it leaves collapses from 14.7 dB to
7.7 dB. The peak is pinned *before the rack starts*, so what you hear at HOT is mostly the soft-clip
and not the rack. The practical rule is unchanged; only the number was wrong.

---

## 3. The chain-order constraint (read this before writing "signal path")

On the master bus the compressor is **pinned**, not reorderable. `sc_apply` runs on bus 0 **after**
the whole `fx_order` insert chain and **before** the soft-clip. So the achievable order is:

```
instruments ─▶ [ fx_order inserts: EQ, DRIVE, … ] ─▶ glue (pinned) ─▶ soft-clip ─▶ out
                                                       ▲
                          reverb is a parallel SEND bus, not a chain insert
```

You **cannot** put the compressor first on the master. Three honest responses, pick per cart:

1. **Voice it as EQ-then-comp.** This is a real console order (many engineers EQ into the bus comp),
   so nothing is lost except the ability to claim the plug-in's stated order.
2. **Put the comp on an instrument's private FX bus**, where you control what feeds it.
3. Say "output chain", not "signal path in this order".

`outboard.h` takes option 1 and says so in its header.

---

## 4. The baseline: `runtime/outboard.h`

Four stages, all bundles of shipped effects, one table, `SET-AND-HOLD` (the caller fires
`outboard_apply()` only when something changed, never per frame; see the `lint-fx-frame.js` rule).

| stage | = | notes |
|---|---|---|
| **EQ** ("British") | `eq_inst(0, lo, mid, hi)` + `FX_EQ` in `fx_order` | three curve presets (WARM / AIR / SCOOP), scaled by one knob |
| **IRON** (transformer / FET colour) | `drive_insert(amt, DRIVE_ASYM, 1.0)` + `FX_DRIVE` | `DRIVE_ASYM` is the even-harmonic shaper, so this is the stage that legitimately earns the "odd and even harmonics" line |
| **COMP** (FET) | `eq_inst(1, g, g, g)` input gain + `FX_INST(FX_EQ,1)`, then `glue(0, amount, atk, rel)` | ratio positions from the §2 table |
| **PLATE** | `reverb(size, damp)` + `instrument_reverb(slot, send)` per feeding slot | a send, so it is parallel; bypass = every send to 0 |

Every stage bypasses to a null (`eq` 0/0/0, `drive_insert` amount 0, `glue` amount 0, sends 0), which
is what makes the cart's toggles a real A/B rather than an approximate one. **How exact that null is,
and in which direction, is now a committed oracle rather than a claim:
[`tools/bypass-check.js`](../../tools/bypass-check.js)** (`--selfcheck` for the analyser, bare for the
rack, `--quiet` to gate). The whole method below lives in it, so these numbers can be re-derived in
one command instead of re-argued.

### 4a. Why a sha is the wrong tool

Every stage differs *while it is in*: that is its job. So `sha(A) != sha(B)` is true of every working
effect and says nothing about the bypass. The number that carries the meaning is the **last differing
sample**. Two renders that reconverge have one; two renders that do not, do not.

### 4b. OUT and IN are different questions

Switching a stage **out** and switching it back **in** both have to reconverge, but only the OUT
direction can be bit-exact *at the switch*. A re-engaging stage starts from whatever state it was left
holding, while the reference run's state is however full the music has made it, so it converges at the
rate of that stage's **own memory**. That direction went unmeasured until a second consumer of
`outboard.h` (the `sideman` organ cabinet) found a 0.304 s reconvergence on an EQ+IRON pair with the
plate parked out, at which point this table read as covering both directions and covered one.

Method, per stage and per direction, all of it in `bypass-check.js`:

* **OUT**: reference = a run where the stage was **never in**; variant = in at 0.333 s, **out** at
  1.500 s. Measured from 1.500 s.
* **IN**: reference = a run where the stage went in at 0.333 s and **stayed** in; variant = the same,
  then out at 1.500 s and back **in** at 2.250 s. Measured from 2.250 s.
* Both runs in each pair share their whole prelude, so the only thing left that can differ is the
  thing being measured; the tool asserts that (a difference before the window is reported as a harness
  bug, not a finding). The baseline is rendered **twice** as a determinism control.

Measured 2026-08-19 on the `outboard` cart's GROOVE programme at ROOM headroom, 12 s renders:

| stage | OUT: bit-exact after | IN: bit-exact after | IN: residual below -60 dBFS after | what the memory IS |
|---|---|---|---|---|
| **EQ** | **0.0 ms** (0 samples) | **0.0 ms** (0 samples) | n/a | none that matters: see below |
| **IRON** | **0.0 ms** (0 samples) | 300.8 ms | 54.6 ms | the wet path's **DC blocker**, a 7 Hz one-pole |
| **COMP** | **0.0 ms** ‑ see the caveat | >9.7 s (a floor) | 2069 ms | `glue`'s **~1.5 s makeup average** |
| **PLATE** | 3389.6 ms | 3891.8 ms | 528 ms | the **tank**: a reverb tail is real |

Three of those rows overturned an expectation, which is the point of having the oracle.

**EQ, both directions, exact ‑ and the natural hypothesis was wrong.** The console EQ is two cascaded
one-poles and the low one sits at 80 Hz, where a single period is 12.5 ms, so "a re-engaging filter
settles over hundreds of ms" is the obvious guess. It is wrong here: `eq_process`'s **state is driven
by its input**, and the gains only scale the three bands it has already split. `eq_inst(0)` is first
in the chain, so its input is identical in both runs and its state never diverges. A filter's settling
time only shows up when something **upstream** of it changed.

**IRON is the stage with memory, which is the opposite of what "a waveshaper" suggests.**
`drive_process` runs a **DC blocker** on the wet path (asymmetric clipping is one-sided, so it has
to): a one-pole highpass at `R = 0.999`, about 7 Hz. The `dr <= 0.001f` early-out returns *before*
that filter, so its state **freezes** while the stage is out and discharges when the stage comes back.
Identified, not inferred: the residual decays by a factor of **0.6433 per 10 ms**, and `0.999^441` is
0.6433 to four figures. This is also the real explanation for `sideman`'s 0.304 s, which had been
attributed to the EQ. Arguably correct behaviour (a real pedal's coupling capacitor holds charge too);
the only thing that was wrong was the *claim* that the stage has no memory.

**Confirmed across two consumers, which is what makes it a property of the STAGE.** The oracle now
carries a `sideman` rack (`--rack sideman`), and that cart is a genuinely different test: it pins EQ
and IRON together as ONE cabinet switch, leaves COMP out entirely, runs a percussion programme
instead of a groove with a bassline, and sits at `plate_amt` 0.34 rather than 0.55. Measured there:

| | outboard rack | sideman rack | by hand, before the oracle |
|---|---|---|---|
| EQ+IRON out | 0.0 ms (each) | **0.0 ms** | |
| EQ+IRON back in | 300.8 ms (IRON) | **297.5 ms** | 304 ms |
| plate out | 3389.6 ms | 2850.8 ms | |
| plate back in | 3891.8 ms | 3920.8 ms | |

Three independent measurements of the same DC blocker, on two carts with different material, inside
7 ms of each other. The plate rows differ, and *should*: a shorter send is a shorter tail. So the
per-stage tolerances in the table are transferable, and a third consumer gets them for one table
entry rather than an afternoon.

**COMP's OUT row has a caveat, and its IN row cannot be bit-exact by construction.**
On the first programme this was measured on, COMP OUT was **not** bit-exact: 2 samples differing by
exactly 1 LSB (-90.31 dBFS), 17.0 ms after the switch. `glue` itself is exact ‑ amount 0 clears
`sc.used`, so `sc_apply` returns 1.0f ‑ and the residual comes from the stage's other two parts.
`eq_inst(1)` reconstructs its input as `lo + mid + (hi - mid)`, which is an **algebraic** null and not
a float-exact one, and the stage's own `dirt` changes what reaches that EQ, so its retained one-pole
state diverges between the two runs and the rounding differs until the states reconverge. Proven by
setting the ratio's `dirt` to 0: the residual vanishes entirely. The residual sits **at** the 16-bit
quantisation boundary, so whether it produces a differing *sample* depends on the material ‑ the
retuned programme rounds the other way and the row now reads 0.0 ms. The gate keeps a 25 ms window
with a **-85 dBFS ceiling** for that reason: the mechanism has not gone away, and a stage that failed
to null at all would be tens of dB louder and still go red.
Coming back **in**, the comp cannot be bit-exact quickly at all: `glue` learns its makeup from a
~1.5 s one-pole average, so that averager alone needs `ln(2^15) x 1.5 s` ≈ 16 s to converge to within
one LSB (measured: 20.5 s on one programme, >9.7 s on another). Gating that number would be gating an
averager's float precision, not the bypass, so this is the one row held to a **settle** criterion
instead ‑ residual below -60 dBFS within 3.5 s ‑ and the tool prints both numbers side by side so the
weaker claim is never read as the strong one.

**The PLATE's lag is correct behaviour, not a leak**: a reverb tail is real and does not vanish
because you stopped sending to it. Worth stating in any copy that claims a bit-exact bypass, because
the claim is exactly true for two stages, true-to-within-1-LSB for the third and true-after-the-tail
for the fourth. Corollary, and the reason the cart says so on its chain strip: with the PLATE in
circuit *every* stage's A/B is smeared until the tail carries the difference away ‑ measured, the
plate's own residual is below -60 dBFS after **528 ms** and below -80 dBFS after **1011 ms**. To hear
a stage switch cleanly, park the plate out.

### 4c. Two ways this measurement lies, both now guarded

Both were found by the oracle failing to be careful enough on its own first runs, and both are the
same shape: a number that looks like a reconvergence and is really something else.

1. **The render ran out.** A difference still going when the file stops *has* a last differing sample,
   and it sits at the end of the file. The PLATE's first run reported "reconverged at 2997.6 ms" in a
   3000 ms post-switch window. Reporting that is reporting `--frames`, so it is now INCONCLUSIVE.
2. **Byte-identical everywhere.** Two renders that never differ also have "no difference after the
   switch", which any naive reading scores as a perfect bypass. It is the commonest way an A/B harness
   lies (`ab-render.js` exits 2 on exactly this). The gate requires the two runs to differ
   **audibly** across the window first, and reports INCONCLUSIVE otherwise. Inconclusive is not a pass.

A third, specific to this repo: **the engine can move mid-run.** `play.js` recompiles per render and
several agents edit `runtime/` in one shared tree, so a change landing between two renders compares
two different builds. It happened three times while this was being written, twice from a
patch-and-restore probe flipping `sound.h` back and forth. The tool fingerprints the engine sources
after **every** render and aborts with *THE ENGINE MOVED, not the bypass*.

> **The trap this measurement caught.** `fx_order()` must be called **unconditionally**, with every
> stage listed whether it is in or out. The first version of `outboard_apply()` assembled the chain
> from only the ENABLED stages and skipped the call when all were off. That measured a **~2 s
> trailing divergence** after bypass, i.e. the null was no longer exact. Fixed by always
> setting the same three-slot chain and letting the nulled stages sit in it doing nothing. The exact
> mechanism is still not pinned down, but the rule is now **reproducible on demand**: reinstating the
> conditional chain and rerunning `bypass-check.js` goes red on all four stages at once, and (because
> a chain that is not set leaves the master on whatever it last had) it goes red as *the runs are not
> the same run* rather than as a trailing divergence ‑ which is the honest report, and is what a
> conditional chain deserves. The unconditional chain is the right design regardless, because it makes
> the insert order stable. The cost of the rule:
> `outboard_apply()` **owns** the master bus's insert order, so a cart that also wants a chorus on
> the master must place it itself.

---

## 5. The three real gaps

1. **The plate is mono** (`sound.h`: "Mono in v1"), and it does not merely fail to widen, it
   **actively narrows a stereo mix**: the wet is added common-mode (`mixL += wet; mixR += wet;`),
   so hard-panned sources come back centred. Measured, it **halves the width**. But this turned out
   NOT to need new DSP: see §6, which is the one section of this doc whose verdict reversed.
2. ~~**There is no gain above unity anywhere except the EQ.**~~ **CLOSED 2026-08-19** — `glue` now
   carries automatic makeup (§2d), so the comp A/B can be level-matched. The rest of the paragraph is
   kept as the record of why it mattered. `instrument_level` clamps at 1 (unity),
   and there is no master gain call. `glue` has **no makeup**, so switching the comp on makes the mix
   audibly smaller (§2b: 3.8 dB of RMS at the hardest ratio), and a **level-matched A/B of the comp
   is impossible today**. Note the asymmetry:
   `multiband()` DOES carry makeup internally (`sq_mk`, commented "a compressor with none just sounds
   smaller"), so the engine already knows this. Giving `glue` the same makeup leg is a small,
   contained change and is the cheapest honesty win on this list.
3. **The EQ bands are fixed.** No frequency select and no Q means "British console" is a claim about
   the *curve shape we chose*, not about the filter topology. Fine to voice, not fine to model.

---

## 6. Stereo reverb: researched, priced, and NOT worth building

This section originally said a stereo plate was "the one place new DSP is justified". **Measuring it
reversed that.** The recipe already exists and nobody had written it down.

### What the mono tank actually costs

Probe cart `rvbwidth` (two plucks panned hard L/R, one key raising the shared send), read with
`tools/stereo-check.js`, which is the only gate in the repo that does not average L and R at the door:

| routing | width (side/mid) | correlation |
|---|---|---|
| dry, no reverb | **0.9999** | 0.0001 (fully decorrelated) |
| `reverb()` master send, 0.8 | **0.5569** | 0.5268 |
| `reverb_bus(1)` + `reverb_bus_fx(1, FX_CHORUS, 0.45, 0.55, 1.0)` | **0.9710** | 0.0294 |
| the same with a barely-there chorus (0.12 Hz, depth 0.18, mix 0.5) | **0.8356** | 0.1779 |

So the mono send **halves** a stereo image, and **a chorus placed after the reverb on a send-bus
recovers 97% of it with no engine change at all.** Even a chorus subtle enough to be inaudible as an
effect gets 84% back. The mechanism: `chorus_process` reads its bus mono but writes **antiphase**
taps to L and R (`wet1` → L, `wet2` → R), so the tail comes back decorrelated even though the tank
that produced it is mono.

### What building it would have cost

Both cheap, which is why the honest answer is not "too expensive" but "unnecessary":

- **Memory.** Exact, from the struct: one `ReverbTank` is 8786 floats = **34.3 KB** (combs 6100,
  allpasses 780, predelay 882, spring dispersion 1024). Three tanks = 103 KB, plus two more inside
  the shimmer voices = **203.6 KB of reverb-family buffers per engine instance today**. Doubling the
  delay lines for stereo is **+103 KB** (+172 KB if `ReverbTank` itself goes stereo, since shimmer
  embeds one). Against a ~4 MB engine context that is ~2.5%, and `SOUND_CART_CTX` multiplies only the
  config log, not the DSP buffers, so a bundle app does not pay 8×.
- **CPU: nothing.** Not measurable at engine level (dormant vs running tank, 100 s of audio, 3 reps
  each: 3.70 s both ways, identical to inside 0.3%). Priced directly with a micro-benchmark carrying
  the verbatim `rvb_comb`/`rvb_allpass` bodies, warmed up and best-of-7 interleaved: **8.57 ns/sample
  mono (0.038% of one core), 10.92 ns/sample for two tanks (0.048%), so stereo is +0.010% of one
  core.** Only 1.3× rather than 2× because the mono chain is latency-bound on its own delay-line
  reads and a second independent tank fills the idle slots. ⚠ First attempt at this benchmark
  reported stereo as *faster* than mono: the first loop was paying first-touch page faults on 34 KB
  of `.bss`. Same trap `tools/tls-spike` documents.

### The verdict

**Do not build it. Document the recipe instead.** Reasons, in order:

1. It measures 0.97 against a dry 1.00. There is no room left to buy.
2. It costs zero engine surface, so no re-baselining, no new API, no determinism exposure.
3. [stereo.md](stereo.md) already decided this: "**Width** — ping-pong delay, stereo reverb spread"
   is listed explicitly under *what is NOT new API (it's a recipe / bus effect)*. The measurement
   vindicates that call rather than overturning it, which is the tidier outcome.

**What the recipe costs, stated honestly:** it needs a **send-bus**, so it consumes one of the two
available tanks (1..2) and one of the seven aux buses, and it is unavailable on the plain `reverb()`
master send (`reverb_bus_fx` only addresses tanks 1..2). And the character is a **chorused plate**,
not a true stereo plate: the width comes from antiphase modulation, so the tail moves slightly where
a real stereo tank would sit still. That is a famous sound in its own right rather than a compromise.

**If we ever do want static width**, the cheap move is not a second tank: it is two short
decorrelating allpasses on the wet (one per channel, different coefficients, ~1.5 KB total) placed
just before `mixL += wet`. About 1.5% of a second tank's memory.

### ⚠ And that cheap move then landed, the same day, from a parallel build

`reverb_plate(amount)` + `reverb_plate_width(x)` now exist: **two PICKUPS** — extra taps on the
*existing* comb lines at different distances, `mixL += wet + side; mixR += wet - side` — plus 4
input-diffusion allpasses for the plate character (`PLATE_AP_TOTAL` 1074 floats = **4.3 KB/tank**).
So the width itself is essentially free (taps, not buffers), and the whole thing is **4% of the
+103 KB this section priced**. Measured with the same probe:

| routing | width | correlation |
|---|---|---|
| `reverb_plate(1.0)` + `reverb_plate_width(1.0)`, master send | **0.7400** | 0.2935 |

**Both approaches are right, for different reasons, and the width number alone picks the wrong one.**
The chorus recipe wins on raw width (0.97 vs 0.74). The engine plate wins on everything else: it works
on the plain `reverb()` **master send** (consuming no tank and no aux bus), its width is **static**
rather than modulated (no wobble on the tail), and it brings **plate character** — input diffusion, a
low-cut, a top lift — that a chorus cannot supply at all. And 0.74 may be the *physically honest*
answer rather than a shortfall: two pickups on one real steel sheet are partially correlated.

So the verdict this section reached still stands where it was aimed (**do not build a second tank**,
which nobody did) but its framing was too narrow: it treated width as the only axis, and priced the
expensive implementation. Use the plate voicing by default; add the chorus-after-reverb recipe when a
cart has a spare tank and wants more width than one sheet can give.

## 6b. FET distortion and transformer saturation: both addable, one nearly free

### Transformer saturation — YES, and it fits an existing extensible slot

The engine already has the exact mechanism, and it was hiding in plain sight: **`drive_voice(voice,
tone)`** and its `drive_voice_shape()`, the "famous-pedal" layer that shapes *around* the clipper.
Each voice is a band split plus a clip curve plus tone shaping, using two one-pole states
(`drvins_lp1/lp2`) that are **already allocated per bus, per instance, per channel**, with a DC
blocker already on the wet path (which asymmetric curves need). Adding a voice is:

- `#define DRIVE_VOICE_XFMR 4` in `studio.h` (+ the usual `studioDocs.js` / `shell.js` entries),
- one `if` branch in `drive_voice_shape()`,
- widening exactly one bound: `drvins_voice = (r.a >= 0 && r.a <= 3) ? r.a : 0;`.

**Zero new state, zero new buffers, zero new `FX_*` kinds.** This is [0015](../decisions/0015-effects-are-recipes-not-primitives.md)'s
preferred shape ("check if it's a MODE of an effect you already have") rather than a new primitive.

And the physics gives it a genuinely distinct character, which is the real test of whether it earns a
slot. A transformer saturates its **core**, and core flux is the integral of voltage, so at a given
level the flux is inversely proportional to frequency: **low frequencies saturate first.** That is the
exact inverse of the Tube Screamer voice, which keeps the bass clean. Prototyped **offline** (a
standalone harness carrying the verbatim `drive_shape` body, so the shared engine was never patched)
and measured with `harmonic-spec.js`, 0.7-amplitude sine, third harmonic relative to the fundamental:

| voice | at 80 Hz | at 2000 Hz | frequency dependence |
|---|---|---|---|
| `DRIVE_SOFT` | -15.2 dB | -15.2 dB | **none** (memoryless) |
| `DRIVE_ASYM` | -15.9 dB | -15.9 dB | **none** (memoryless) |
| `DRIVE_VOICE_TS` | -41.3 dB | -21.2 dB | distorts the **highs** more (correct for a TS) |
| proposed `XFMR` | **-16.8 dB** | **-66.1 dB** | distorts the **lows** more, by **49 dB** |

At drive 0.35 the prototype saturates an 80 Hz tone as hard as a plain soft-clip and leaves a 2 kHz
tone 49 dB cleaner. Pushed to 0.9 the gap narrows to 10 dB, which is also physically right: enough
level and the highs saturate too. **No existing mode in the engine does this.**

Honest limits: it is a **quasi-static** model (a filter plus a memoryless shaper), so there is no
B-H **hysteresis** and no true core memory. The prototype also came out strongly odd-dominant
(even harmonics ~45 dB down), which is right for a well-made core but means the asymmetry term needs
to be a real parameter if "even-harmonic warmth" is the goal. And it remains a **voicing, not a
model**, same as everything else in this doc.

### FET distortion — the waveshaper exists; the thing that makes it a FET is program dependence

Two separate readings of the claim, with different answers:

1. **As a static curve:** already shipped. `DRIVE_ASYM` is the asymmetric shaper. See the correction
   in §1 about how strong its even content actually is.
2. **As a device:** a FET compressor's distinguishing behaviour is that **THD rises with gain
   reduction** — the dirt arrives when it is working hard. Ours is static.

The good news is that the second one is reachable **from cart-land with no engine change at all.**
`fx_mod(bus, FXMOD_DRIVE, v)` writes `drvins_amt[bus][0]` per sample **with internal slew**, and
`[0]` is exactly the instance `outboard.h`'s IRON stage occupies. So a cart rides the drive from the
level it already measures. This is a sanctioned pattern, not a hack: `mixbooth`'s BREATHE does it
every frame, and `lint-fx-frame` explicitly exempts `FXMOD_*` ("modulation rides, it doesn't enable").
⚠ It **overwrites** the amount rather than offsetting it, so the cart must supply the whole value.

**Verified only as far as the wire:** `ab-render.js` with the ride flipped on and off gives distinct
shas and moves the spectral centroid 7515 → 6500 Hz, so the modulation reaches the DSP. **The
perceptual claim is unverified.** An attempt to show the dirt *tracking* level numerically failed:
correlation between level and a first-difference brightness proxy went *down* (0.665 → 0.489), and
the test was confounded anyway because the ride's mean drive did not match the static baseline. Two
things wrong with it, so it is evidence of nothing either way. This is a **LISTEN item**, which is
what `ab-render` itself says about its own numbers.

The engine-side version — real program-dependent dirt inside the gain stage — is the expensive
option: `sc_apply` returns a *gain scalar*, so distorting there means changing it to process samples,
which touches two call sites in `sound.h`, a hot shared file. Do the cart-land ride first and only
pay that if the ear says it is not enough.

### Ranked recommendation

| # | item | cost | verdict |
|---|---|---|---|
| 1 | document the chorus-after-reverb width recipe | a table in `effects-recipes.md` | **do it now** |
| 2 | `DRIVE_VOICE_XFMR` transformer voice | ~10 lines, one bound widened, no new state | **do it** — measurably distinct, and it is the only honest route to the "transformer" word |
| 3 | FET program dependence, ridden from cart-land | zero engine change | **try it, then listen** |
| 4 | makeup gain on `glue` | small, contained | **do it** — unblocks a level-matched comp A/B (§5.2) |
| 5 | ~~stereo reverb tank~~ | +103 KB, +0.010% of a core | **do NOT build a second tank.** SUPERSEDED: `reverb_plate` landed the two-pickup version at 4.3 KB/tank, width 0.74 static + real plate character |
| 5b | find the IRON bit-exact regression (§2d) | one bisect | **do it** — a measured property of `outboard.h` regressed today |
| 6 | FET dirt inside `sc_apply` | changes a seam in a hot shared file | only if #3 disappoints |
| 7 | B-H hysteresis in the transformer voice | new state + a real model | not now; say "voicing", not "modelled" |

## 7. Copy that stays inside the truth

Not a hypothetical: an app shipping this chain needs a store description, and the version that lies
is the one that gets written by default. What we can say today:

> A three-stage output rack on the master bus: a self-keyed bus compressor that makes the whole mix
> breathe as one lump, with a switchable ratio and real attack and release in milliseconds; a warm
> three-band console EQ that can boost as well as cut; and a plate-voiced reverb send. Between them
> sits an asymmetric saturation stage: odd AND even harmonics, which is what "warmth" actually means.
> Switch the EQ or the saturation out and the mix returns **bit-exact, on the sample the switch
> flips** ‑ we measure it, every build.

What to **cut**: "meticulously modeled", "field-effect transistor", "transformer", "German", any
named device, and the word **limiter** or any promise of peak/loudness control (§2b). What to
**keep**: odd and even harmonics (true), glue as *behaviour* (true, and "breathes as one lump" is
both truer and more evocative than "snappy compressor"), and the bit-exact bypass, which is
verifiable, measured, and a better boast than anything else on the list, because almost no plug-in
can claim it.

**One precision the copy has to keep, and the earlier draft of it did not.** "Switch *any* stage out
and the mix returns bit-exact" is **not** true, and §4b is why: it is exactly true for the EQ and the
saturation stage, true to within one LSB for the compressor, and true only after the tail for the
plate. Naming the two stages it *is* exactly true for is both honest and no weaker as a boast, since
those are the two you would A/B anyway. And nothing in the copy should promise anything about
switching a stage back **in**: a re-engaging stage carries its own memory back with it, which is
correct behaviour and a bad sentence.

---

## 8. Gates

| after touching | run |
|---|---|
| any stage's values | `node tools/fx-check.js`, `node tools/level-check.js` |
| the comp / anything with feedback | `node tools/soak-check.js` |
| a stereo plate (§6) | `node tools/stereo-check.js --check` first, then `--expect wide` |
| the bypass-is-byte-identical claim, in EITHER direction | **`node tools/bypass-check.js`** (`--quiet` to gate, `--direction out\|in\|both`, `--stage NAME`). Do not reach for a sha: every stage differs *while it is in*, so a sha only says "differs". Run `--selfcheck` first ‑ it is the half that runs no cart, and three of its four verdicts are failures (§4) |
| a stage's memory (a filter, a DC blocker, a follower, a learned average) | the same tool's **IN** direction, which is the only thing that sees it. Coming back in is a different question from going out (§4b) |
| `outboard.h` itself | `node tools/build-all.js`, `node tools/lint-fx-frame.js --strict` |

## 9. Open

- [ ] `reverb_plate(amount)`: the stereo/decorrelated plate voicing (§6). The one new-DSP item.
- [ ] makeup gain on `glue` (§5.2), so the comp A/B can be level-matched.
- [ ] a second `IRON` instance (`drive_insert_inst(1, …)`) for input-transformer AND output-transformer
      placement around the EQ, which is the real console topology.
- [ ] program-dependent release on `glue` (§2), the last honest piece of "snappy".
- [ ] a real peak/crest control somewhere in the chain (§2b). `glue` ducks but does not limit, and
      `multiband()` is the only thing here that touches peaks, at the cost of the OTT character.
- [x] **DONE 2026-08-19** ‑ promote the §4 bypass-reconvergence test into a committed oracle:
      [`tools/bypass-check.js`](../../tools/bypass-check.js), 30-answer `--selfcheck` in the
      repo-doctor row set, mutation-tested. It went on to find the three things §4 now records (the
      `eq_inst` algebraic null, the drive stage's DC-blocker memory, and that OUT and IN are different
      questions) and to correct two claims this doc had made.
- [ ] a `dr <= 0.001f` early-out that also **parks the DC blocker** (§4b, IRON's IN row), so a
      re-engaging saturation stage does not discharge a frozen capacitor into the mix. Not obviously a
      bug ‑ a real pedal's coupling cap does exactly this ‑ so it is a decision for whoever owns
      `sound.h`, not a defect to fix quietly. Whatever is decided, the oracle's IRON IN row records it.
- [ ] extend `bypass-check.js` to `sideman` (its cabinet toggle is `C`, its plate `V`) so the second
      consumer of this table is gated too, not just measured once by hand.

## See also

- [effects-bus-architecture.md](effects-bus-architecture.md) ‑ §E is the amp/cab precedent this doc copies, §5 the reverb tank pool
- [../guides/effects-recipes.md](../guides/effects-recipes.md) ‑ the recipe book; the outboard rows live under "combination pedals"
- [../decisions/0015-effects-are-recipes-not-primitives.md](../decisions/0015-effects-are-recipes-not-primitives.md) ‑ why a stage is a bundle
- [sideman.md](sideman.md) ‑ the SECOND consumer of this table (EQ+IRON pinned as an organ cabinet); it is what surfaced the IN direction in §4b
- [../guides/checks-and-oracles.md](../guides/checks-and-oracles.md) ‑ the reverse index; `bypass-check.js`'s row is under the audio gates
- [audio-notes.md](audio-notes.md) ‑ §17 effect ledger
