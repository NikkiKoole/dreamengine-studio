# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.
>
> **For "what's shipped vs. open vs. cut" see [`STATUS.md`](STATUS.md)** — the single
> status ledger. This file is the running narrative + environment gotchas.

**How this file stays useful (the system).** The `## Where we are right now` section is
**ACTIVE LANES only** — one dated `▶ ACTIVE THREAD` callout per complex in-flight effort, each with
(1) what shipped, (2) a **Resume-at** pointer to the owning doc's pick-up point, (3) any hot files
to avoid colliding on. Rules that keep it honest: **refresh your lane's date whenever you touch it;
prune a lane the moment it ships or goes quiet** (its detail already lives in `STATUS.md` + the
doc's pick-up point — don't duplicate, point). **Write every Resume-at as a real anchor link —
`[text](path#section-slug)` pointing at the target `.md`'s heading, not prose like "→ §3" or "(doc §Foo)"** — so the pointer's
target is machine-checkable: when work ships and that section gets renamed, the anchor breaks and
`--check` catches it. Keep the *status* itself in the doc (point, don't restate) so a shipped slice
can't leave a stale sentence here (the trip-up: a "resume at trim+speed" line survived weeks after
trim+speed shipped, because the status was copied into the pointer instead of pointed-to). A lane
dated **>2 weeks** old is presumed stale — verify or prune. Everything below the lanes is history;
trust `STATUS.md` + the design board over it. **Tooling keeps this honest** (`tools/handoff.js`, the
[driftable two-door pattern](design/driftable-docs.md)): `node tools/handoff.js` lists the active lanes + age (and it's the first
thing `orient` prints — the front door); `node tools/handoff.js --check` flags a lane >2wk old, a
broken doc link, or a **broken `#section` anchor** (surfaced by `cart-status.js` — the back door).
So a forgotten stale lane *surfaces* instead of rotting.

_Last updated: 2026-07-29 (sixteen lanes; newest lane = **the contemporary ReBirth — a post-hardware rack that clones TECHNIQUES, not machines** (`design/contemporary-rebirth.md`): ten of twelve boxes across three candidate racks need zero engine work, and two gap-rungs shipped the same day — Rung A `multiband()`/`instrument_multiband()`/`FX_MULTIBAND` (three-band dynamics with an UPWARD half, the OTT box; `mix` 0 = byte-identical bypass; named `multiband` because five carts already had a local `squash`) showcased by the `hyperbox` hyperpop rack, and half of Rung B `sample_shift()` + `harmonize_mic()` (length-preserving transpose) showcased by the `voxshift` probe. Then the maker LISTENED and reported popping every gate had called clean, which drove the rest of the session: two grain-geometry bugs found and fixed (down-shift zero overlap, 177 hard splices -> 1; and a truncated fractional grain read), and a THIRD defect that resisted three attempts (WSOLA correlation lock raw + normalized, a monotone accumulator, and the lock at a narrow ±8% window) — every one regressing the snap face into PERIOD DOUBLING, all three reverted and now `⚠ DO NOT` comments in `at_psola_slot` with the numbers. Out of that: **`tools/psola-check.js`**, the artifact gate, three detectors because each is blind to what the others catch (a period-doubled take is still perfectly periodic, so a periodicity metric scores that regression as a 2x IMPROVEMENT — only the f0 reading sees it); it killed attempt #3 in one command. Also banked: the amapiano rack audited as a third candidate (zero engine gaps — `INSTR_MEMBRANE`'s morph macro is pitch-bend, so the engine already had a log drum and nobody noticed), and GAP 3's formant dial proven to need a NEW envelope stage rather than the unused `formant` argument, which is a trap (`rubberband-reference.md` §2a-bis: forcing `fstep` to 1 held the formants exactly and moved the pitch not at all). Lesson banked: with a squashed master you mix for the GAPS, not the levels (rms -1.5/crest 1.5 on the first render, a wall with no groove; fixed cart-side to -5.3/5.2). NEXT = the amapiano rack (cheapest, no rung), then Rung C (`beatfx`, a ride-safe beat-synced buffer re-reader). Prior newest = **responsive-first device faces — the `face.h` grammar** (`design/responsive-first-device-face.md`): Layers 1–3 SHIPPED this session — the `deviceface` starter, `ui_*_cell`+`LayLane`, and the declarative `face.h` `FaceZone[]` grammar — proven across three conversions (chipjam/dubjam/grooveface, each driving a refinement: face_sublane/face_screen/face_resize_to); the iPad-Pro spread MOCKED in `roomyface` (B tile / C unhide / D 2×2+master, kept OPEN as a per-cart `device_class()==ROOMY` choice, no engine work); NEXT = prove a ROOMY branch on a real cart (`acidcandy`) or Layer 4 (make face.h the default). Prior newest = **harmony brain — a shared next-chord + analysis engine** (`design/harmony-brain.md`): the demand-discovery drip surfaced r/musictheory demand-82 for a "progression analyzer/suggester", the brief maps bossa's locked functional-Markov engine + folds in a deep-research survey that settles the model (1st-order Markov over roman-numeral functions + forced cadences; ANALYZE takes the key as input), and now SHIPPED v1 the same day: `runtime/harmony.h` (bidirectional — the bossa/cocktail tables extracted with byte-identical pinned-seed WAV proof + hb_suggest/hb_analyze) and the demand-82 toy `chordwise` (24-assert spec); NEXT = the maker's LOOK pass on chordwise + vocab adoption (template stations as weight sets, chord-aware improv.h). Banked en route: spec.js was broken repo-wide (missing AudioToolbox after the mic lane) — fixed. Prior activity = **Tiny Acid Jam shipped to App Store Connect** — `acidcandy` renamed + published as the first $1.99 standalone single, ASC record `6792504925` created + listing pushed live; see the **store/ASO** lane. Newest lane = **walkbox — a walking-bass step-sequencer** (`walkbox`) — a TB-303 workflow on the upright's real `INSTR_BOWED` pizz voice: draw a line on scale-locked note-bars, sculpt a tabbed VEL/LEN lane (velocity → pluck attack; length → staccato gate, top = TIE), flip SLD/OCT rows, dial SWING; the "303 done as a real pluck". Core + articulation ship and play; resume [`design/walkbox.md`](design/walkbox.md). Older: **the chord-bloom rack** (`chordblossom2`) — an instrument you PLAY (chords with your hands) wearing a genre FLAVOR (a backing band that follows you), the winning direction over the radio-rack `bossabloom` (which felt like "radio + UI"); Flavor table NEUTRAL/BOSSA/YACHT/CITYPOP · diatonic KEY mode (keys = the in-key chords) · 3-axis SPICE/RICHNESS/GROOVE · living 4-bar-phrase accompaniment · tiny per-part PEDALBOARD (FX tab) · STOP+REST; NEXT = tune yacht/citypop + per-flavor spice + candy skin, resume `design/bossa-rack.md`. Older: **the Android build target** — the engine runs frameworkless behind `platform.h` (`DE_NO_RAYLIB`), so Android is a host shell + Gradle packaging: spikes 0–3 SHIPPED (NDK compile → GLES2 render → AAudio → touch on an arm64 emulator), the `android/` scaffold + `android/build.sh`, and the editor's **🤖 export .apk** button (sideloadable, no Play account/device); NEXT = save → signed `.aab` → Play Billing → multi-cart app; resume `design/android-plan.md`. Older: **the candy acid RACK** — `acidcandy` packages `acidrack`'s guts (2×303 + 808 + 909 + master) as focusable candy device-faces at 160×100; `tr808.h`/`tr909.h` extracted byte-honest; note-bar free-draw + a flag palette + a drawable PCF + a drag-release-bounce fix; the master FX shelf now lives behind ONE **FX hub** key (chips wearing `fxicons.h` glyphs + a byte-exact **DRY** kill) and a **VOWEL** device whose **SPK** mode makes the 303 notes pronounce a vowel word with glides, rests (**DENS**/**GAP**) and an asymmetric voice gate; NEXT = **SPEAK is hard to hear** (the maker's ear verdict — check the 303s aren't muted, then reroute the formant per-303 via `instrument_formant`; `--solo-slot 6` decides masking-vs-contour in one command) / 808-909 voice completeness / drum depth / arrangement / the mascot — resume the cart's `de:meta.todo[]`. Older: **demand discovery** — `tools/reddit-gaps.js` mines a music tribe's RSS for unmet demand, 2 tribes done (notes 022/023; thesis = a cheap playful lo-fi toy in classic-gear clothes), a 6 h LaunchAgent drip grows caches in `tools/reddit-gaps-cache/`. Older: **editor↔cart workflow** — Gap 1b (`de:meta.slug`) + Gap 1 (code save-back) SHIPPED & committed: slug backfilled across all 455 carts + `lint-carts.js` REQUIRES it; the **"save to source" button** writes the Code-tab buffer back to `tools/carts/<slug>.c` + rebakes in place; and the **PIXEL side (Gap 2) Option D SHIPPED** — reversible slot-level sprite PATCHES for generator carts (`tools/lib/sprite-patch.js` core + `make-cart` composite-on-bake + editor save-to-source diff; `de:spritepatch` chunk), so hand-edits survive the next CLI bake; NEXT = eyeball live + a discard button → resume `design/editor-cart-workflow.md` Gap 2 · worldgen ladder — rungs 0–7 SHIPPED (district fill + Rotterdam calibration via `sndi-check --compare`; `citygen.h` extracted → sloop's M drives a generated city w/ collidable buildings) + the junction grammar `roadkit.h` (B2 pure geometry + B3 field renderer, streetlab byte-identical; citydrive draws curb-return junctions through it, J); NEXT = B4 grade dispatch → roadlab interchanges / the N+M infinite-world reconciliation · multiplayer — rung 5b WebRTC P2P BUILT + published (pong live on github.io; ~12ms direct over wifi, relay = signaling only); step 5 (adaptive NET_DELAY) + step 7 (TURN) PARKED; wire-side diagnostics SHIPPED 07-10 (RTT probe + rx-gap + wall-clock logs + web-tick stalls) → next office visit = the checklist in multiplayer-research.md · device-adaptive-layout — the acidwire WIREFRAME CART is built + INTERACTIVE + runs on the iOS sim (dual-mode desktop/device, 4 states incl. focus, touch+mouse, real 303 piano-roll + drum grids, iPad 2×2 grid both orientations, finger-honest); guide interactive-wireframes.md; next = play on glass → narrow-303 input model → R5 re-land into acidrack.c · store/ASO — the ASC upload tool BUILT (`tools/asc-push.js`, ADR-0026): keywords + screenshots pushed live, all 3 IAPs `READY_TO_SUBMIT`; product ids renamed to `com.mipolai.tinyjam.*` (sim-reverified) · editor media — the per-cart **Promote tab SHIPPED (A–E)** + the **shared-popup pattern** (trailer + keyword-research popups, opened from cart AND app) + reels save/load (subject-scoped strip + cross-subject overview) + multi-resolution export (output-ratio picker on reel-Build + clip-bake, Stage-2 per-ratio variants that FILL, App Store even half-sizes); `export-ratios.md` stages 1+2 SHIPPED + the `onetake` proof cart; keyboard shortcuts = the enabler; NEXT = an EYEBALL PASS (none clicked live) + the fixed-layout composite gap · responsive instrument UI — the playbook + ADR-0028 + the epianofit mock shipped; epiano brief re-scoped to the FAITHFUL piano; the scale-grid SHIPPED as the `scalegrid` cart (device-tested, spec 71/0), open step = extract it into a `grid.h` library then wire epiano's editor-swap · leads — the marketeer tool + ledger BUILT (`tools/leads.js`, 18 tribes): cart→tribe→venues, taxonomy being filled cart-by-cart, editor Apps-page surface next → resume `design/leads-marketeer.md`)_

---

## Where we are right now

**Sixteen lanes are active in parallel right now** (different areas — pick the one you're resuming):
(15) **the contemporary ReBirth — a rack that clones TECHNIQUES, not machines** (`design/contemporary-rebirth.md`; rungs A + B shipped: `multiband()` + `sample_shift()`/`harmonize_mic()`, showcased by `hyperbox` + `voxshift`, plus the `psola-check.js` artifact gate; the epoch-phase glitch and the formant dial are BOTH parked with numbers; NEXT = the amapiano rack, then Rung C),
(14) **responsive-first device faces — the `face.h` grammar** (`design/responsive-first-device-face.md`; Layers 1–3 shipped, proven across chipjam/dubjam/grooveface, tablet spread mocked in `roomyface`; NEXT = a real ROOMY branch or Layer 4),
(13) **harmony brain — a shared next-chord + analysis engine** (`design/harmony-brain.md`; SHIPPED v1: `runtime/harmony.h` + the `chordwise` toy; LOOK pass + adoption open),
(12) **walkbox — a walking-bass step-sequencer** (`walkbox` — a TB-303 workflow on the upright's real pizz voice),
(11) **the chord-bloom rack** (`chordblossom2` — play chords with your hands, a genre FLAVOR band follows),
(10) **Android build target** (host shell + Gradle → Play; spikes 0–3 + editor 🤖 export .apk shipped),
(0) **the candy acid rack** (`acidcandy` — `acidrack`'s guts repackaged as candy device-faces),
(1) **the worldgen ladder** (realistic procedural roadgen), (2) **multiplayer — WebRTC P2P (rung
5b)**, (3) **device-adaptive layout**, (4) **store / ASO + the app-trailer builder**, (5) **editor
media (record/replay + where it lands)**, (6) **responsive instrument UI + the scale-grid**,
(7) **leads — the local marketeer** (find venues to post + track outreach), (8) **editor↔cart
workflow: cart provenance (`de:meta.slug`) + the save-back round-trip**, and (9) **demand discovery
— the reddit-gaps drip** (mine a tribe's RSS for unmet demand; caches grow via a 6 h drip). All
below; none is "the" thread. Shipped/open ledger for all: [`STATUS.md`](STATUS.md) + the design board.

> **▶ ACTIVE THREAD (2026-07-30) — Synth Secrets: the audit is COMPLETE, the build plan is running (Phase 0 done, **PHASE 1 COMPLETE 7/7**, **PHASE 2: 2.1, 2.2 and 2.3(a) SHIPPED — PIANO now has real stiff-string inharmonicity + a completed Railsback curve; 2.3(b) DROPPED on measurement; 2.4 is the live item**).**
> The owner supplied Gordon Reid's **Synth Secrets** (Sound On Sound, 63 parts, 1999-2004) and asked for a
> cross-check against `runtime/sound.h`. **All 63 articles are now read**: an architecture pass plus eight
> per-family recipe passes, ~106 sub-findings, every one citing both sides (part + issue on the book side,
> `file:line` on ours). Findings live in [`design/synth-secrets-audit.md`](design/synth-secrets-audit.md)
> §A-§M; **work happens in [`design/synth-secrets-plan.md`](design/synth-secrets-plan.md)**, which is the
> ordered ledger. Do not add work items to the audit.
>
> **The plan's three rules, which is what makes this resumable:** every item is classified **FACT /
> VERIFY / LISTEN / DESIGN** — the first two are just-do-it, the last two need the owner. A **LISTEN item
> is built opt-in from the start** (a flag, an `eng_p` slot, a second slot) so an A/B is possible and a
> "worse" verdict costs nothing to abandon; an inconclusive A/B resolves to **DROP**, recorded with its
> measurement. And **when to add a new engine vs change one** is answered from this repo's own ADRs
> (0006/0015/0016/0017) as a **7-rung ladder** — lowest rung that holds the finding, escalate only when a
> *built cart* failed the rung below. Verdict: **almost nothing on the list earns a new engine**; §G is a
> cart-land header, and even the electric guitar (§H6) is a second tap on `INSTR_GUITAR`.
>
> **Phase 0 SHIPPED (`f10ea94d`)** — ten FACT fixes and five measurements, four of which changed the plan:
> aliasing on the un-BLEP'd pulse is a **non-issue** (≤ −53 dB even at A5, PWM sweep no worse), so **3.30
> dropped**; `instrument_lfo` **accepts 80 Hz** (sidebands at exactly ±80 Hz, ~30 dB up), so 3.6 is
> unblocked and cheaper; `INSTR_PIPE` is a **closed, odd-harmonic pipe** (evens 50-73 dB down), which
> makes its flute/recorder presets structurally wrong *and* explains why the "octave flageolet" docstring
> was doubly wrong; and **§H8 + §I5 are one cause** (loop coefficients fixed per pass, not per note), so
> two items merged. `sound.h` now carries a **"the `+` is load-bearing"** warning on the piano comb and
> **four "✅ VERIFIED against Part N"** notes on tables that matched the book exactly.
>
> **HOW THE EAR-CALL LOOP ACTUALLY WORKS — read this before starting an item, it is the whole rhythm of
> Phase 1.** Every verdict so far was made from **rendered WAV pairs**, not from playing the cart. So:
> build the change behind a runtime toggle (never a `#define` — the owner cannot A/B a compile-time flag);
> A/B it with [`tools/ab-render.js`](../tools/ab-render.js) (`--file runtime/x.h` to patch a header);
> `--keep` the renders and copy them to `build/ab/<cart>-<STATE>-{stock,recommended}.wav`; hand over the
> `afplay` lines, say what to listen for and **where in the file it happens**, and name anything that could
> bias the ear (above all a level difference — a louder take wins on loudness alone). Bake the cart too, so
> it can be played. When the owner picks: flip the default, keep the loser on the toggle, and **verify the
> shipped default renders byte-identically to the take that was approved** (this caught a near-miss in 1.2
> where a leftover experiment would have shipped a sound 4 dB hotter than the one judged). The full
> protocol with all its traps is [plan §1](design/synth-secrets-plan.md#1-how-we-decide-something-is-done).
>
> **Build the A/B clip so it PROVES its own scope.** 1.3's clips play a preset whose accent row misses the
> snare, then one whose accents hit it, so the first region must come out byte-identical between settings
> and the second must differ — a structural claim checked by sha instead of by argument. Committed clips
> live in `tools/clips/<cart>/`; three exist for this thread and each has a long header comment explaining
> the trap it exists to avoid (e.g. `tr808` **self-plays** on boot, so a render with an empty script is a
> full drum pattern, not silence; and no `tr808` preset has a cymbal row at all, so only the F key strikes it).
>
> **Phase 1 (seven cart-only A/Bs) — 3 of 7 done, ALL THREE confirmed by the owner's ear on 2026-07-28.**
> `solina` (1.1) has a **WOW switch**: BREATHING (unison spread modulated by a random-shape LFO, staggered
> per tab — Reid's Part 46 ladder) is the **default by the owner's ear call**, with CLASSIC kept reachable
> by key **W** or tapping the label. A third state
> was built and **cut**: it measured indistinguishable from CLASSIC and no oracle we have can see a 0.16 Hz
> character change under a chord progression — the cut is documented in the cart with the code to restore it.
>
> **Built a tool on the way: [`tools/ab-render.js`](../tools/ab-render.js).** Flip one file-scope value,
> render each variant, one table, source restored in a `finally`. It exists because hand-sedding a flag
> bit me — the regex stopped matching after the first substitution, so every later variant re-rendered
> state 0 and three byte-identical WAVs were nearly written up as "no audible effect". It **exits 2 and
> shouts on byte-identical variants**. Use it for every LISTEN item; do not hand-roll.
>
> **1.2 (`tr808`) is DONE and the three-band cymbal is the DEFAULT** — same ear call, same day, same shape:
> the stock single-band voice stays reachable on key **C** / tapping `CY 3BAND`, not removed. Stop the
> sequencer (SPACE) and hit **F** — no preset has a cymbal row, so F is the only way to strike it. The
> centroid now walks **14895 → 11844 Hz** over 200ms and then converges *bit-exactly* onto the stock tail,
> where the stock voice was flat for the whole ring. Both ends are provable: the shipped default renders
> byte-identically to the take that was approved (`ff2477695836`) and OFF renders byte-identically to the
> pre-change cart (`90dc75069555`). It IS ~6.8 dB hotter at the strike and can't be level-matched
> (`instrument_level` collides with acidcandy/dubjam's per-slot mixers, velocity clamps to silence after
> 4 steps) — that was flagged for the ear call and accepted, so **don't "fix" it later**. Two findings worth reusing: a
> `FILTER_HIGH` above ~7 kHz on `INSTR_SQUARE` amplifies **aliasing** (that band stem-measured −0.0 dBFS,
> centroid 21942 Hz vs Nyquist 22050 — always stem-check a high band with `--solo-slot`), and the real
> 808's 7100 Hz upper bandpass had been sitting in `tr808.c`'s own docblock, unimplemented, since day one.
>
> **1.3 (`tr808` + `tr909`) is DONE — the velocity-dependent snare is the DEFAULT on both.** Key **N**
> cycles 0/1/2 (state in the 808's `hint()` footer, next to POLY on the 909). `boost` used to be added to
> the body and noise layers *equally*, so an accent was the same snare turned up; now they tip in opposite
> directions and an accent buys **+37% noise share for −1.7 dB of level**. Soft hits are byte-identical to
> the pre-change carts by design, and the shipped defaults match the approved takes (`a6dc6c0a02e7` /
> `b40f2782577b`). `dyn=2` is kept but degenerates near a centred SNPY knob (tilt 4 zeroes the body, so the
> accent loses its pitch) — an effect setting, not a subtler one.
>
> **The most transferable lesson of the thread, from 1.3: print a strength knob's transfer function over its
> REAL input domain, not its endpoints.** The tilt curve was `(boost * dyn + 1) / 2`, which reads like
> harmless half-strength scaling — but `boost` in these carts is only ever 0, 1 or 2, so at `dyn=1` the
> rounding mapped boost 1 *and* boost 2 to the same tilt and the parameter **stopped depending on velocity,
> which was the entire feature**. It measured as a clear effect, A/B'd as a clear effect, and was one message
> from shipping as the default; it was caught by evaluating the curve, not by any oracle. A parameter can be
> audibly doing something and still not be doing the thing you claimed.
>
> **Two mechanical traps that cost real time, worth knowing before you touch a cart's draw():**
> a long **`sprintf` into a cart's shared `char buf[32]`** overflows the stack with no crash and no compiler
> warning — the only symptom in 1.3 was `play.js --dump` writing **zero frames** while audio rendered
> perfectly (use your own buffer + `snprintf`); and **`ui-audit.js` catches off-screen/overlapping text but
> NOT low contrast** — it found a pre-existing 370px footer on `tr909`'s 320px screen (so `POLY:tap=length`
> had never been visible to anyone, now fixed), yet passed `solina`'s invisible grey-on-brown readout. Run
> it, then also read the baked PNG.
>
> **`spec()` now guards the structural claims — 16 assertions, `node tools/spec.js tr808 tr909`.** The
> owner's prompt ("not just out of vanity, there are real things to prove") is exactly right, because the
> 1.3 bug proves the gap: *every audible signal said the feature was fine* while it had stopped depending on
> velocity. So the specs assert what no ear and no audio gate can see — the tilt is strictly increasing in
> velocity; the cymbal's three decays stay unequal and ordered; the top cymbal band stays at/below its
> measured aliasing ceiling; the new cymbal slots stay inside `TR808_NSLOT` (the `D909_BASE 23` collision
> class); and **the preset data the A/B clips depend on** (PLANET ROCK's accents must miss its snare, BOOM
> BAP's must land on it — a one-character preset edit would otherwise leave the clip testing nothing).
> The shared banks carry their own via `tr808_selfcheck()` / `tr909_selfcheck()` (spec.h's "specs on an
> includeable"). **All were verified to FAIL on the original bug before being kept** — re-introduce the old
> curve and 2 go red on the 808, 1 on the 909. An assertion never seen failing is a guess.
>
> **A blocker worth knowing before you plan a spec:** `spec.h` reserves **`step`**, the obvious name in a
> step sequencer. `acidcandy` cannot host a spec at all because of it (it has `static int step` for its
> transport), and the error points at the cart's own pre-existing line, so it reads like the cart broke.
> Written up in [`design/spec-harness.md`](design/spec-harness.md#reserved-names--step-is-the-one-that-bites).
> Consequence here: the 808↔909 tilt-curve **drift** check (they carry duplicate copies on purpose) sits in
> `tr909.h` behind `#ifdef TR808_H` and self-activates the day a both-headers cart can host a spec. It does
> not run today. **`acidcandy.c` also has live foreign WIP right now** (another agent is building an FX hub
> in it) — leave it alone and keep it out of your commits.
>
> **Also corrected an audit error rather than quietly coding around it:** §J9 claimed the snare's tone→noise
> drift *over the note* was missing. It was already there — the noise layer outlives the body in both
> machines (130/100ms, 170/90ms) and a hit's centroid climbs 10890 → 12279 Hz across its own decay. Only the
> velocity half needed building. Expect a few more §-claims to be half-true; check before implementing.
>
> **Fixed a harness trap that will otherwise eat an hour of anyone's day:** `caffeinate -dims` *prevents*
> display sleep but cannot **wake** an already-dark screen, so every headless render segfaulted at
> `signal 11` before frame 1 with a binary that had passed minutes earlier. `play.js`, `spec.js` and
> `make-cart --run` now fire `caffeinate -u -t 1` first. See
> [`guides/debug-harness.md`](guides/debug-harness.md) → "a sleeping display segfaults every harness run".
>
> **1.4 (`brass`) is a recorded DROP, and it is the most instructive item so far.** §E10 lifted three
> envelope numbers from one worked Reid patch; built as toggles, level-matched, A/B'd — **all three lose on
> a waveguide**, for structurally different reasons: the model already does it (the bore supplies a ~40 ms
> onset, so his 100 ms attack double-counts), the parameter isn't what it is on his hardware (with
> `decay_ms` 0, "sustain maximum" is just +4.86 dB = 20·log₁₀(7/4)), and the value destroys the model's own
> behaviour (a short release truncates the bore's ring-down — **the shipped 1200 ms is not a pad envelope by
> mistake, it is roughly this bore's ring-down time**). Envelope left byte-identical (`af3631b9329e`),
> toggle removed rather than left as clutter, measurements + a 4-line restore recipe kept in `brass.c`.
> This is now hard evidence under **§G** (the subtractive-imitation engine): the problem is not that his
> patches *sound* different, it is that his patch *parameters have no faithful translation* into a
> waveguide's controls. **Never port numbers from a Reid patch by editing — always A/B.**
>
> **Two process lessons from it, both of which cost a round trip:**
> - **The short releases were provably CLEAN and still wrong.** Largest sample-to-sample step in the
>   release: 61% of peak, *identical to the shipped voice* — no click, and the 5 ms version is a smooth 6 ms
>   ramp. **No oracle here can tell "a clean short decay" from "the resonator was cut off."** That is the
>   strongest argument in the plan for why LISTEN is a real category.
> - **An absolute judgement flipped when the comparison was restored.** Told "both newer ones get cut off",
>   I swept the release *upward*; the owner then called 120 ms "fine, nothing is cut off" — and a sha check
>   showed that file was **byte-identical** to the one they had just called cut off. Against SHIPPED
>   directly, they picked SHIPPED. **Always hand over the pair, never a single file, and re-confirm against
>   the incumbent before concluding anything.**
>
> **PHASE 1 IS DONE, 7 of 7.** Beyond 1.1-1.3 above: **1.5** (`piano`, key **L**) layers two slots ~7 cents
> apart per Part 45 — liked, but deliberately kept **opt-in**, because this cart's six presets are declared
> acceptance tests and layering would retune the yardstick. **1.4** (`brass`) and **1.6** (`organ`) are
> recorded **DROPs**: Reid loses all three brass envelope numbers (the shipped 1200ms release *is* the
> bore's ring-down), and the Hammond item was mis-priced by the audit — its detent table is in the engine,
> so "two rows" would silently remap 13 carts. **1.7** (`martenot`, key **0**) is **DONE — GATE is the
> default** (owner's ear, 2026-07-29: *"the morph sounds a bit too clean/bright, I like gate"*), with FILTER
> and MORPH still one keypress away. The filter-as-gate measures **30 dB of range with the VCA held
> constant**; the morph was honestly **ear-only** (the HF proxy reads 0.000 on that voice and the centroid is
> fundamental-dominated, so it moves the wrong way) — though here the ear and the table agreed, MORPH's
> centroid being ~2x FILTER's at a light touch.
> **⚠ PROMOTING A TOGGLE TO A DEFAULT IS ITS OWN CHANGE.** GATE holds `note_vol` constant and this cart keeps
> one voice ringing, relying on `note_vol` hitting 0 to be silent at rest — so as a *toggle* nobody ever saw
> it (you always arrived mid-gesture), and as the *default* the cart **droned from boot**: −15.2 dBFS on an
> empty script against FILTER's true silence. Fixed by gating on `intens > 0` (already snapped to exactly 0
> at rest, and the real touche is also the on/off), leaving the VCA untouched inside a gesture. **Whenever
> you flip a default, re-test the states the toggle never let you reach: boot, rest, and release.**
>
> **The recurring lesson of the whole phase, worth carrying into Phase 2:** on these engines **the envelope
> does not own the tail — the model does.** It landed three separate times (the 808 cymbal's decays, the
> brass release, the piano layer's ENV2), and each time a longer release or gate did nothing because the
> physical model's own decay governs the ring-down. Expect it again, and expect Reid's numbers not to
> transfer: **never port values from a subtractive patch by editing — always A/B.**
>
> **Phase 1 also produced one engine FIX** (`41a4c6ea`): `instrument_mode` guarded `idx >= 2` when `eng_p`
> is four wide, so the PIANO's double-decay and hammer-knock params — implemented end to end — were
> unreachable and two `piano` sliders had never worked. A no-op at rest, live once a slider moves.
> `MODE_PIANO_DECAY`/`MODE_PIANO_KNOCK` now exist. Still open: `instrument_mode` does not VALIDATE its
> index, which is how a dead control survives; top of [`STATUS.md`](STATUS.md) → "Open".
>
> **⚠ 1.7's MORPH CRACKLED, and the fix produced a new oracle: [`tools/click-check.js`](../tools/click-check.js).**
> The owner heard it (2026-07-29) before judging the mode. `wave_set` replaces the wavetable under a running
> oscillator, so every quantised dull step jumped the output from `old[phase]` to `new[phase]` — up to 16
> one-sample discontinuities per swell. **Two orthogonal causes, and fixing either alone leaves the
> crackle:** the GRID (a step's jump scales as 1/NDULL; it was 8) and the RATE (`intens` slews up to
> 0.5/frame, so the index could move dozens of steps in ONE frame, and a multi-step jump is as big as a
> coarse-grid one — this is the half that still clicked at every note onset after the grid was fixed).
> NDULL 8 → 64 plus a ±2 steps/frame limit took it from **13 splice-like events (worst 15.4x the local
> step-rms) to 1 (4.1x)**, against a control that peaks at 3.3x. FILTER renders byte-identical; GATE never
> enters that branch. **The lesson is not about wavetables:** the plan's own write-up asserted the stepping
> was "inaudible as stepping on a slow swell", that was the ONE unmeasured claim in an otherwise
> well-measured item, and it was the one that was wrong. An unmeasured sentence in a measured write-up
> reads as evidence. Run `click-check` after ANY mid-note table/shape swap — `wav-envelope`'s amplitude and
> centroid curves look the same whether a transition is a clean ramp or a splice, which is the identical
> blind spot that made 1.4's brass release call so hard.
>
> **PHASE 2 HAS STARTED, AND 2.1 IS DONE IN BOTH HALVES** (`49c398e2` + this one) — the engine can follow
> the keyboard. (a) `instrument_keytrack(slot, amount)`: one multiply at note-on, `amount` 0 = absolute Hz
> (the default and byte-identical), 1 = true 1V/oct so a self-oscillating filter can be *played*, 0.93 =
> Reid's taste value. (b) `ENV_CUTOFF_OCT` / `LFO_CUTOFF_OCT`: a sweep's DEPTH in octaves instead of Hz,
> reaching `instrument_env`, `instrument_lfo` and `instrument_follow` (plus the `note_*` twins). Both halves
> are **VERIFY items** — the acceptance test is a number, not an ear, the first two of those in a while.
> Full write-ups with the tables: plan §2.1(a)/(b).
>
> **Three things from 2.1 worth reusing:**
> - **`DE_RUNTIME_DIR` now exists** (`make-cart.js`, so `play.js` too): point the compile at another engine
>   tree, and a cart renders against the **pre-change** engine with identical harness args. That is how
>   "byte-identical" became a measurement instead of an argument — no destructive `git checkout` on a hot
>   shared header. Copy `runtime/`, `git show HEAD:runtime/sound.h >` the copy, render both, compare shas.
>   The control that proves the rig works: a cart using the new constants must FAIL to compile against it.
> - **Octave modulation multiplies and is applied LAST**, into its own `cutoff_mul` that starts at an exact
>   1.0 — the shape `pitch_mul` already had. Additive Hz first, then the octave scaling, so a patch mixing
>   both units means the same thing regardless of which mod source ran first. Copy that pattern for any
>   future relative-unit destination.
> - **A committed A/B seed must say what it does NOT prove.** `tools/clips/keytrack/0{1,2}-sweep-*.script`
>   pass the ear test but cannot carry the acceptance table: the phrase gates 420 ms notes every 200 ms, so
>   notes overlap and a per-note spectral region is polluted by the two before it (it measured non-monotonic,
>   and both units read the *same* at the first C). The table came from a four-isolated-notes probe instead,
>   whose source is pasted in plan §2.1(b) — a ruler is not a cart, so it is documented rather than committed.
>
> **2.2 IS SHIPPED TOO — [`runtime/mono.h`](../runtime/mono.h), the largest item in the audit, with zero
> engine change.** A `Mono` held-key stack + `mono_press`/`mono_release` returning START/GLIDE/RETRIG/STOP;
> priority LAST/LOW/HIGH/FIRST and triggering SINGLE/MULTI/ANY (Reid's Figures 8/9/11). `sh101` drives it
> from PRIO/TRIG switches under the TUNE knob, defaults byte-identical to the pre-change cart
> (`ddb9d398da39`). Full write-up: plan item 2.2.
>
> **The finding beats the feature: the SH-101's PORTAMENTO switch is secretly a TRIGGER switch.** The cart
> conflated Reid's two axes (gliding the pitch and not retriggering are the same code path), so PORTA OFF
> renders **byte-identically** to his ANY, PORTA AUTO/ON to his SINGLE — and **MULTI is unreachable on the
> real machine's panel**. That is a measured answer to what the conflation costs, which is what §B3 asked.
>
> **`mono.h` carries its own 47-assertion spec** (`node tools/spec.js sh101`) because Part 18 *is* a test
> suite: four priorities, four different pitch sequences from the same played notes. **Every assertion was
> watched failing first** — four mutations (LOW inverted, MULTI not retriggering a losing press, ANY not
> re-attacking on hand-over, FIRST behaving like LAST) turn 4/1/1/3 of them red.
>
> **Three traps from 2.2, all of which produced a confident wrong conclusion before being caught:**
> - **I shipped a decorative switch.** The first cut kept `prio_sel` beside `mono.prio` and synced them only
>   in the tap handler, so `init()` forced LAST back and all four priorities rendered byte-identical.
>   `ab-render`'s byte-identical warning caught it; `mono.prio` IS the switch now, and a spec assertion
>   drives the panel and checks the policy followed. **Two sources of truth for one setting is the bug.**
> - **An ASCENDING test sequence cannot see two of the four schemes** — play 48/50/52 and "last pressed" IS
>   "highest held" (so LAST ≡ HIGH, and FIRST ≡ LOW). Seeds must be non-monotonic AND release middle-first,
>   or the hand-over path never fires. Committed as `tools/clips/sh101/01-overlap.script`.
> - **`seq 1 0` prints "1 0" on macOS**, not nothing, so a `for i in $(seq 1 $n)` loop emitted TWO taps at
>   n=0 and the "baseline" was sitting on HIGH. I nearly wrote up "the priority switch does not reach the
>   audio". A `watch()` trace of `mono.prio` caught it. Sibling of CLAUDE.md's zsh word-splitting note.
>
> **A FOURTH trap, found by the owner's ear minutes after 2.2 landed, and it is the item's own thesis
> biting back.** I split Reid's two axes in the DECISION (`mono.h` owns re-attack-vs-legato) but left the
> glide TIME wired to a switch whose OFF position I then ignored: classic mode never reaches `glide_to` with
> PORTA OFF, but SINGLE/MULTI do, so they glided **125 ms with the switch reading OFF**. Reported as "some
> kind of detuned arpy character"; the pitch track agreed exactly (mid-slide the fundamental sat at 137.7
> then 134.1 Hz between D3 146.8 and C3 130.8 — out of tune for over 100 ms). Fixed: `glide_to` reads
> `(porta_mode == 1) ? 0 : f_porta(porta_v)`. **SEPARATING TWO CONFLATED AXES IS NOT DONE WHEN THE DECISION
> SPLITS — only when every CONSUMER of both axes splits too.** It also invalidated the first version of the
> equivalence table, which had compared trigger policies at *mismatched* glide settings; re-measured at
> matched PORTA, both equivalences hold.
> Related, and NOT a bug: this cart's default voice is SAW 1.0 + SUB (square, exactly −1 oct) at 0.75, with
> no LFO→pitch and TUNE centred — the "thick, two-note" quality is an octave stack, not detuning. The pitch
> tracker locks to 146.6 Hz (saw) at D3 and 98.0 Hz (sub) at G3 on the same take, which is a neat objective
> measure of how present that sub is.
>
> **Budget for this on any instrument cart:** `spec.h` declares `key_down`/`key_up`, which `sh101` had owned
> since it was written, so it could not host a spec until they became `sh_key_down`/`sh_key_up`. Already
> documented next to the `step` trap in
> [`spec-harness.md`](design/spec-harness.md#reserved-names--step-is-the-one-that-bites) — read that BEFORE
> planning a spec, which would have saved the detour. Those are the natural names in every keyboard cart.
>
> **2.3 was attempted and its PREMISE FAILED (2026-07-29), which is now the live question.** The row said
> "prototype on PIANO, which already has the machinery" — the machinery does not work. Built
> [`tools/inharm-spec.js`](../tools/inharm-spec.js) (partial frequencies in cents vs the ideal `n·f0`;
> `--check` green *first*, because a broken tool and a harmonic engine print the same table) and measured
> two defects of the same shape, *a value computed correctly and then never allowed to reach the sound*:
> **§I4b** PIANO's dispersion chain is **inert** (allpass coefficient 0.9999948 = the identity; B ≈ 2e-6 vs
> a real grand's ~1e-4; h16 +0.2¢ where it should be ~+22¢; GUITAR and PLUCK too), and **§I4c** the
> `piano_stretch_freq` seam works in the **treble only** — `v->freq` is written back but `v->freq_target`
> is not, so the glide slew undoes it, *except* that an `effLen > len` clamp ([`:4735`](../runtime/sound.h))
> blocks the undo in the sharp direction. So PIANO ships **half a Railsback curve**: treble stretch tracks
> the design 1:1 (measured at C5), bass stretch is gone (1/23 at C3). One-line fix, `v->freq_target = freq`.
> Plus **§I4d**, smaller: with no stretch at all the loop still runs +1.3→+4.0¢ sharp (its own
> uncompensated delay bookkeeping). §I4b and §I4c hid each other: the stretch exists to reconcile
> inharmonicity, and there was none.
> **`sound.h` was NOT changed** — every number is from the committed engine or a sweep that restored the
> file in a `finally` block.
>
> **§I4c is SHIPPED (2026-07-30): the one-line fix at `K=2`, and a gate that asserts the curve.** Owner
> took the recommendation. `v->freq_target = freq` in `sound_piano_start`; `PIANO_STRETCH_K` untouched at
> 2.0f, because the treble half has been sounding at K=2 all along so the bass now agrees with what already
> ships. Changes nothing above B3, completes the bass (−2.6¢ at A2, ~−8¢ by A1), below the melodic JND.
> **The gate is the durable half.** `tune-check` grew `INTENDED_DETUNE`: an engine that is *supposed* to
> leave ET declares its curve (K parsed from `sound.h`, one source of truth). The reason it matters:
> without the fix A2 measured **+0.0¢ against ET**, so the old check called it perfect — perfectly in tune
> and perfectly wrong. **Then sharpened (also 2026-07-30) into a DIFFERENTIAL**, which is the version to
> know: the stretch is now a runtime parameter, **`MODE_PIANO_STRETCH`** (`instrument_mode` idx 4, default
> byte-identical, and a real feature — set 0 to play in unison with fixed-pitch parts), so the sweep renders
> PIANO twice in one pass, once with it off, and asserts the DIFFERENCE against the intended curve. That
> cancels §I4d and every other constant loop error, so nothing needs blessing: it lands within **0.08¢** at
> every note against a ±0.6¢ tolerance, where the blessed-baseline version needed ±1.5¢ around three
> hand-tuned numbers. Still red on §I4c and now localised — A2 off by +2.93¢ while A3/A4 pass, which reads
> as "the bass half is missing, the treble half is fine". **Trap it cost: the `eng_p` bound exists TWICE**
> (the public setter AND the `SR_ENG_TUNE` handler); widening only the setter is a silent no-op, which is
> the same failure the setter's comment documents for idx 2/3, one layer deeper. A sixth aux param needs
> three edits. Also deleted the `sound.h` comment claiming
> "tune-check flags PIANO by design", which never was true and is what made a green check read as
> confirmation. Ear pair (the `piano` cart is C4–C5 so the fix is inaudible there; this is an A1–A4
> arpeggio + low stack): `build/ab/piano-stretch-{OFF-bass-flat-missing,ON-full-railsback}.wav`.
>
> **§I4b STEP 1 IS DONE (2026-07-30) and it is FEASIBLE — the diagnosis changed.** Built
> [`tools/disp-model.js`](../tools/disp-model.js), which computes what a dispersion allpass cascade does
> to the partials **analytically** (the loop phase condition, so a root-find not a render) and solves the
> coefficient for a target B. Validated against the engine at one point: model B 1.00e-4 / h16 +19.8¢ vs
> measured 1.02e-4 / +19.9¢. Results: the 4-stage cascade already in the engine **can** reach a real
> grand's B = 1e-4 at every pitch C2–C6, costing only 3–7% of the delay line, and no register runs out of
> line. What is actually wrong is the coefficient mapping, in two ways I had not identified: **the SIGN**
> (a positive `c` gives phase delay rising with frequency, which FLATTENS partials; stiffness needs
> `c < 0`, and the `pt ≤ 0.9` clamp makes that unreachable by construction — which is why scaling `pt`
> 3000× moved nothing, it was the wrong half of the space), and **the pitch dependence** (the needed |c|
> FALLS with pitch, −0.72 at C3 to −0.09 at C6; the engine's `pt ∝ freq` moves it the other way).
> **⚠ PROCESS RULE LEARNED THE HARD WAY: do not patch a shared `sound.h` to search a grid.** The first
> attempt left the engine broken twice — a foreground timeout SIGTERMs node so `finally` never runs, and
> signal handlers cannot interrupt a synchronous `execFileSync` either, while `&` made the tool report
> "completed" while it kept holding the engine patched during another agent's render. Model the sweep;
> patch only to confirm ONE point.
>
> **§I4b STEP 2 WAS ATTEMPTED AND THE RECIPE IS WRONG — the compensation and the dispersion are NOT
> separable.** Prototyped "solve `c` for a target B, then subtract the cascade's phase delay from the
> line" and it does not produce a stiff string: measured B overshoots the target 9–14× and the
> `inharm-spec` **fit residual explodes to 48–96¢**, i.e. the partials are scattered rather than
> stretched. Calibrating the compensation empirically brings one note into tune but not others (A2 in
> tune while A3 sits 129¢ sharp). Shortening the line raises the dispersion's effect relative to the loop,
> so `c` and `L` are a coupled system and `solveDesign`'s 3-step fixed point is the wrong formulation.
> **TWO RULES FOR THE NEXT ATTEMPT:** (1) the acceptance criterion MUST include the fit **residual**, not
> just B and pitch — every broken attempt hit a plausible B while sounding like a scattered metallic mess;
> (2) the uncompensated validated point (2 stages, `c = −0.7770`) had a residual of 1.2¢, so the structure
> is right until the compensation touches it.
> **⚠ THE EAR PAIR WAS CONFOUNDED AND THE OWNER'S EAR CAUGHT IT — do not re-run a timbre A/B yet.**
> Reported untrained and without seeing numbers: *"B sounds like the sustain pedal is pressed, it dies out
> earlier"*. `wav-envelope` confirms it hard: the dispersed take is **11 dB down by 0.26 s and 19 dB down
> by 1.3 s**, so the pair's dominant audible difference is DECAY, not inharmonicity. My failure: I checked
> peak and rms, saw rms 4.6 dB low, and flagged it as a level caveat instead of recognising a different
> decay CURVE, which a level caveat does not cover. **Run `wav-envelope` on any A/B pair before handing it
> over, not just the level numbers.** The finding underneath is bigger: a real stiff string rings for
> seconds, so losing sustain means energy is leaving the loop — a first-order allpass is lossless in
> theory, so suspect its interaction with the averaging/brightness filter and `effDamp`. B's centroid also
> RISES as it decays (1026 → 1656 Hz), backwards for a struck string. **So §I4b has a THIRD open question:
> does dispersion in this loop cost sustain, and why.** Answer that before any further ear test.
> The pair itself, for reference: rather than ship a broken compensation it uses the
> *uncompensated* validated config pitch-matched with `instrument_tune`, so it compares TIMBRE at one
> pitch. `build/ab/piano-inharm-{A-OFF-today,B-real-stiff-string}.wav`, both within ~1¢ of pitch, B 1.7e-6
> vs 2.2e-4, h8 +0.3¢ vs +17.1¢, residual 3.8¢. Caveats: B is ~2× a typical grand (deliberately audible),
> and the takes differ 4.6 dB in **rms** (peak within 0.8 dB) because the stiff one decays faster — the
> stiff take is the QUIETER one, so check a verdict against loudness. No musical phrase yet; a phrase
> needs the per-note compensation that does not work.
>
> **✅ RESOLVED (2026-07-30) — step 2 WORKS and the step-1 recipe was right all along.** Both failures
> above were mine, in the test rig, not in the physics. **(a) `instrument_tune` DAMPS a Karplus-Strong
> string** — it shifts pitch through the per-sample `effLen = len/ratio` path, so `ratio ≠ 1` forces
> fractional interpolation every sample, a lowpass inside the feedback loop. Measured at +0.97st on PIANO:
> h2 decay −9.1 → −53.5 dB/s, **6× faster**. Applies to PLUCK/GUITAR/PIANO, was undocumented, and means
> **never pitch-match a KS A/B with `instrument_tune`** — compensate at note-on where `ratio` stays 1.
> Dispersion itself costs NO sustain (it sustains slightly better than today). **(b) The compensation must
> reach EVERY delay line in the voice**: `ideal2` (the grand/bright second string) is computed
> independently at `sound.h:4738`, so compensating only `ideal` left string 2 ~80¢ flat of string 1 —
> *that* was the "scattered partials". Compensate both and it lands: **f0 −0.2¢, B 1.13e-4, residual 1.4¢,
> h4 +2.3 / h8 +8.1 / h16 +22.2¢, decay matched to today.** Note the trap generalises: the voicings that
> expose it (`grand`, `bright`) are the only ones with two strings — harpsi/clavichord/celesta have
> `detune 1.0`, so this bug is INVISIBLE unless you test grand.
> **Clean ear pair, comparable this time** (peak within 0.1 dB, brightness 0.144 vs 0.140, decay curves
> tracking): `build/ab/piano-inharm-{A-OFF-today,B-real-stiff-string}.wav`. **Rule earned twice: run
> `wav-envelope` on both takes before calling anything an A/B** — comparability is a property of the
> envelope, not of peak and rms.
>
> **OWNER'S VERDICT (2026-07-30): B (the stiff string) WINS, but weakly — *"very subtle, hard to explain
> the difference, I feel I like B better."*** Green light to continue, not a mandate to default it. The
> subtlety is expected: a single mid note is the weakest case for inharmonicity, whose payoff is in the
> BASS and in CHORDS/intervals where stretched partials of different notes beat (the whole reason the
> Railsback stretch exists). **A phrase is now renderable at last** — it was not before the compensation
> worked — so the next ear test is a bass passage plus a chord.
> **Also settled by owner request: inharmonicity MUST be swappable, with "perfectly harmonic" reachable.**
> Same idiom as `MODE_PIANO_STRETCH`: `0` = pure harmonic (= today's sound, so backward compat is free),
> `0.5` = the voicing's baked amount, `1` = double; per-voicing amount from the existing
> `PianoVoicing.stiff`. It is a correctness requirement too, not a convenience — a compile-time constant
> cannot be A/B'd by a gate, which is half of why §I4b/§I4c hid. ⚠ `eng_p[]` is now FULL (0–4), so index 5
> means widening the array AND both `idx >= N` bounds AND the four-place registration — **build the bounds
> lint first**.
>
> **✅ SHIPPED 2026-07-30 on the verdict *"B is clearly better in the chords, let's ship it"*:
> `MODE_PIANO_STIFF`** (`instrument_mode` idx 5) — `0` = a perfectly harmonic string, `0.5` = the voicing's
> own amount, `1` = double; target B scales from `PianoVoicing.stiff`, calibrated so the grand at centre
> lands on **1.1e-4**, the value the ear approved. `pn_solve_dispersion()` solves the coefficient from the
> delay DROP between the fundamental and a reference partial — one scalar equation, MONOTONE in c, 28
> bisection steps at note-on. **Do not replace it with a direct fit of B over many partials: not monotone
> at strong coefficients, an earlier attempt overshot 46×.** Compensation goes into `ideal` AND `ideal2`.
> Measured: pitch does NOT move across knob settings (A1 −9.5¢ / C3 −1.0¢ / C4 +1.7¢ at 0, 0.5 and 1.0), B
> 2.4e-6 → 1.1e-4 → 2.5e-4, h8 +0.3 → +8.1 → +17.8¢. Knob 0 is measured-equivalent to the old engine but
> **not byte-identical** (the old near-identity allpasses are now skipped entirely).
> **⚠ TRAP IT EXPOSED: YIN cannot track an inharmonic string.** A stiff string is non-periodic, so
> autocorrelation locks onto a shorter lag pulled by the stretched partials — PIANO read **+26.1¢ sharp at
> A2, confidence 0.65**, while a spectral-peak measurement of the same render was exact. `tunecheck.c` now
> sets `MODE_PIANO_STIFF 0` for both PIANO passes so the tuning sweep and the stretch differential measure
> what they are for; `inharm-spec` (Goertzel) is the oracle for the dispersion itself.
> **New gate: `tools/lint-aux-params.js`** — the aux-param width lives in FIVE places and missing one makes
> a parameter silently inert. Proven red on the real bug. Also: the `piano` cart has a `stiff` slider, its
> knob indices are now a NAMED ENUM (inserting mid-list would have cross-wired decay→knock, the exact
> CLAUDE.md trap), and a pre-existing bug surfaced — the tuning row's bars were drawn dark-grey on
> dark-grey, so decay/knock/velo had ALWAYS been invisible unless selected (`ui-audit` cannot see low
> contrast; read the baked frame).
>
> **Resume-at — the live queue, most-ready first.**
>
> 1. **§M2's A/B (item 2.4).** The live Phase 2 item, premise-checked and needs no ear to start. Three
>    parallel 1–4 ms delay lines with feedback vs `gt_body`-style biquad formants: does a short-delay
>    "room" make a convincing instrument body? Reid gives the range explicitly (Part 22, *"about 1mS to
>    4mS"*) and notes a spring is far too long. Primitives already exist (`moddel_hermite`, the comb
>    helpers, per-instrument aux buses). Targets `BOWED` (no body at all) and `GUITAR` (body with no return
>    path) — both premises **confirmed**. Mind that §I3's piano tricord already has a weak 0.2%
>    output→string-1 tap, so do not A/B it against an assumed zero.
> 2. **Re-voice the other five piano voicings' `B` by ear.** Cheap, no new mechanism. They currently scale
>    from `PianoVoicing.stiff` (celesta 2.4e-4 down to clavichord 4.4e-5 — plausible ordering) but **only
>    the grand was ear-checked**. Use the `stiff` slider in the `piano` cart.
> 3. **§I4d** — with no stretch the loop still runs +1.3→+4.0¢ sharp (its own uncompensated delay
>    bookkeeping). Note the offset is **window-dependent**, because the brightness bloom moves `ksb` and
>    hence the loop delay *within* a note, so any blessed residual is per-measurement-window.
> 4. **Two gates, both cheap and both earned by this thread.** An **A/B comparability gate** (refuse to call
>    two WAVs an A/B when peak, rms *or the decay curve* disagree — peak and rms passed on a pair whose
>    decay differed by 19 dB). And a **"pitch is invariant across `MODE_PIANO_STIFF`"** assertion, which
>    belongs in `inharm-spec` and NOT tune-check, because it needs the spectral method: YIN cannot track an
>    inharmonic string (it read +26¢ sharp at conf 0.65).
> 5. **Also open, lower value:** `B` is constant across the register where a real Railsback curve rises at
>    both ends; and extending the runtime-seam-plus-differential pattern across the `instrument_*`/`MODE_*`
>    surface, since *four* bugs in this thread were the same shape — a value computed correctly that never
>    reaches the sound.
>
> **Three lessons this thread paid for, worth carrying to any engine work:**
> (1) `sound.h` said *"tune-check flags PIANO by design — that IS the stretch, not a bug"* and tune-check
> **passed**. A comment that pre-emptively explains away a gate turns a green check into false confirmation.
> When a comment says a gate should be red, **verify that it is red.**
> (2) The first draft of §I4c said the stretch was cancelled outright — measured at **one note** and
> generalised, when the missing *flat bass* in the curve was the clue on screen the whole time. **Measure
> both halves of a signed curve before describing it.**
> (3) Ranking an item by how many findings it closes is sound **only while those findings are believed
> rather than measured**. 2.3(b) counted five families and had one. 2.4's check cost four greps and one
> render, and re-scoped it honestly. **Check the premise of any finding-count before trusting the rank.**
>
> Full write-ups, every table, and the cold-start reproduction recipes →
> [`design/synth-secrets-plan.md` §2.3(a)](design/synth-secrets-plan.md#the-premise-failed-three-defects-found-by-measuring-first-2026-07-29).
> **Orienting cold on this thread:** `node tools/orient.js` for the repo, then read plan §1 (the gate + the
> A/B protocol), §2 (the add-vs-change ladder) and §4 (Phase 1, incl. the three finished write-ups). The
> audit is reference only — never add work items to it, only ✅ verdict banners pointing back at the plan.
> The owner's standing constraints for this thread: **small steps**, **every engine change must have a cart
> where you can hear it**, and **nothing is skipped** — an item that turns out not to be worth doing gets a
> recorded DROP with its measurement, not silence.
> ⚠ Two process traps already hit: `ui-audit` passes **low-contrast** text (it only catches off-screen and
> overlapping), so read the baked PNG; and `--run` bakes only the thumbnail, so **re-embed after every
> source edit** or the pre-commit hook will (correctly) reject a stale `.cart.png`.

> **▶ ACTIVE THREAD (2026-07-26) — the CONTEMPORARY ReBirth: rungs A+B shipped, the PSOLA artifact hunt PARKED behind a new gate, amapiano next.**
> The post-hardware rack: ReBirth cloned unobtainable *machines*, but modern genres were never made on
> gear, so a contemporary version clones **techniques** (the glide, the ratchet, hard tune, the
> always-on squash). The audit found **ten of twelve boxes across three candidate racks need zero engine
> work**; the gaps became rungs.
>
> **Shipped:** Rung A `multiband()` / `instrument_multiband()` / `FX_MULTIBAND` (three-band dynamics with
> an UPWARD half, the OTT box, `mix` 0 = byte-identical bypass) with `hyperbox` as its showcase rack ·
> half of Rung B, `sample_shift()` + `harmonize_mic()` (length-preserving transpose) with `voxshift` as
> the probe · a **third rack audited** (amapiano §1c/§2c, zero engine gaps — `INSTR_MEMBRANE`'s morph
> macro is pitch-bend, so the engine already had a log drum) · and **`tools/psola-check.js`**, the
> artifact gate.
>
> **Two things are parked, and they are DIFFERENT problems — do not conflate them:**
> 1. **The audible glitch on the snapped and up takes** (an epoch-mapping phase bug). The maker hears it;
>    it is now measured, ~25-29 events/take against the RAW control's 1. **Three fixes tried, all three
>    reverted**, each regressing the snap face into period doubling: the WSOLA correlation lock (raw and
>    normalized), a monotone accumulator, and the lock with a narrow ±8% window. All three are
>    `⚠ DO NOT` comments in `at_psola_slot` with numbers. The pattern is the finding: every attempt broke
>    *snapped* and none broke the shifted takes, because snap's `Tt` is within a few percent of `T`, so
>    epoch jitter is comparable to the correction itself. **The fix must be jitter-FREE, not
>    better-guessed** — a dedicated spike, not a tweak.
> 2. **GAP 3, the chipmunk↔natural formant dial.** Not a clicking problem at all. Needs a **new
>    envelope-rescale stage**; `at_psola_slot`'s unused `formant` argument is a **trap, not a half-built
>    feature** (proven in `rubberband-reference.md` §2a-bis: forcing `fstep` to 1 held the formants
>    exactly and moved the pitch *not at all*). Not an FFT necessarily — LPC or a filter bank also work.
>
> **Before touching either, run `node tools/psola-check.js`** (before AND after). It renders `voxshift`'s
> four takes through three detectors and **no single one is sufficient**: a period-doubled take is still
> perfectly periodic, so a periodicity metric scores that regression as a 2x *improvement*. It killed
> attempt #3 in one command after #1 and #2 each cost several listen-and-report rounds.
>
> **NEXT, in order:** the **amapiano rack** (cheapest of the three — no rung, no external audio; its open
> question is whether the swing is playable, since the genre's identity is a *feel*), then **Rung C:
> `beatfx(mode, bars, mix)`**, a beat-synced buffer re-reader, which must be **ride-safe** (parameters
> read per sample, never re-allocated per call) or it lands on the wrong side of `lint-fx-frame.js` —
> and see `rubberband-reference.md` §2c first for the non-uniform-stretch idea. Resume at
> [Rung C in contemporary-rebirth.md](design/contemporary-rebirth.md#rung-c--gap-1--a-beat-synced-buffer-re-reader-halftime--beat-repeat).
> Smaller open ends: a home cart for `harmonize_mic` (live-only, so ADR-0032 says its clip must be
> captured live, not replayed from a `.rec`), `hyperbox` v2's real voice box (waits on GAP 3), and the
> hip-hop rack, whose hard part is not DSP but **where the loop audio legally comes from**.
>
> Hot files if you take this: `runtime/sound.h` (targeted `Edit`s only, never a full `Write`).
> Adjacent records: [`design/rubberband-reference.md`](design/rubberband-reference.md) (read §2a + §2a-bis
> before GAP 3; the library is **GPL and bars App Store distribution** — reading reference only, never a
> dependency, never a port), [`design/rebirth-classic.md`](design/rebirth-classic.md)
> (the RB-338 pilot whose chassis all three racks reuse), [`design/transparent-autotune.md`](design/transparent-autotune.md)
> (the correction half, which needed two spikes of its own), [`design/tinyjam-racks.md`](design/tinyjam-racks.md)
> (the rack program + the trademark rule: "SB-808" in the sketch is close to a live mark, so the
> hip-hop rack needs its own name before it ships).

> **▶ ACTIVE THREAD (2026-07-21) — `bandbox`: the chord-chart SEQUENCER (mockup done, READY TO WIRE).**
> The standalone device-face instrument the `chordwise` analyzer pointed at: a 160×100 face where you
> compose a chord chart and a genre band (chords/bass/mel/drums/pad) follows it, every voice a lane of
> **lego-block cells** with per-cell **p-locks**. This session settled the whole LOOK (draw-only mockup,
> committed `26e58cb6`): glass 5-lane tracker + chassis (aligned voice rail / nav / keybed), FONT_TINY,
> screen-morphs-chassis-stays. **NEXT = the build phase — wire it.** Full brief (data model, ordered
> build plan, reuse map, gotchas): [`design/bandbox.md`](design/bandbox.md). Reuse the working engine in
> `chordwise.c` (genre band maps + playback) + harmony.h/radio.h/drumkit.h/keybed.h; the shared `band.h`
> extraction is a LATER call (third-customer rule). Chord-bloom context: [`design/bossa-rack.md`](design/bossa-rack.md).

> **▶ ACTIVE THREAD (2026-07-21) — responsive-first device faces: the `face.h` grammar (Layers 1–3 SHIPPED).**
> The "could a cart be responsive-and-opinionated *from the start*?" line
> ([`design/responsive-first-device-face.md`](design/responsive-first-device-face.md)) — a thread DISTINCT
> from the acidrack retrofit (the *device-adaptive layout* lane) and the epiano/scalegrid playbook (the
> *responsive instrument UI* lane). Layer 0 (runtime reflow) was already done; this session shipped the
> whole cart-land stack, all committed:
> **L1** a device-face STARTER, [`deviceface`](../tools/carts/deviceface.c) — the chunky route-2 canvas +
> five stacked bands + the shared 16-column register, with marked seams to drop an instrument in.
> **L2** `ui_button_cell`/`ui_knob_cell` (`runtime/ui.h`) + a `LayLane` register (`runtime/lay.h`) so a
> sequencer view and its step strip bind to ONE lane and align by construction, not by hand.
> **L3** the declarative grammar `runtime/face.h`: a `FaceZone[]` table (`FACE_BAND` top/bottom · `FACE_LANE`
> per-step · `FACE_HERO` = the remainder) that `face_layout()` carves, ENFORCING the principles — a
> width-stealing side-rail is not expressible. Demo = [`facedemo`](../tools/carts/facedemo.c).
> **Proven across three conversions, each driving a real refinement:** `chipjam` (landscape, input-in-draw)
> → the register escape hatch; `dubjam` (input-coupled `update()`) → the `relayout()`-in-`update()` pattern
> + `face_sublane`/`face_screen` (flanking the HERO is allowed — only top-level full-height rails are
> banned); `grooveface` (portrait, multi-skin, 320×400) → `face_resize_to` (design density is per-face;
> layout is orthogonal to skin). The grammar held every time and got better for the friction.
> **Tablet/iPad-Pro spread MOCKED, kept OPEN** ([`roomyface`](../tools/carts/roomyface.c), keys 1/2/3):
> **B** tile-the-rack 2×2 · **C** unhide-one-machine · **D** 2×2 + master (the ReBirth RB-338 classic).
> Decision with the maker: the arrangement stays a per-cart `device_class()==ROOMY` choice, NOT a baked
> default — face.h does all three with zero engine work (B/D = `face_layout` per grid cell + a master
> column; C = one wider zone table).
> **NEXT (the maker's call):** prove a ROOMY branch on a REAL cart — `acidcandy` (a 4-machine rack, D/B the
> likely pick) turns the mockup into a shipped tablet arrangement and hardens the grammar once more; OR
> **Layer 4** (make face.h the default), worth it only once L3 is judged proven enough.
> **acidcandy ROOMY layout — MILESTONE 1 WIRED (2026-07-23) into [`acidcandy.c`](../tools/carts/acidcandy.c) `draw_rack2` (`rack_view==2`).**
> The `acidcandy_ipad` mockup is now a real `draw_rack2()` (helpers `r2_*`) coexisting behind a toggle
> (default UNTOUCHED — reach it via the **NEW** button in the old 2×2; **HOME** → 2×2 → **NEW** on desktop).
> LIVE now: sticky FOCUS, per-machine MUTE, PLAY, MST TEMPO/SWING/GLU/FLT/PUMP + mixer, both 303 knob-columns
> (acid 5 + FX trio + CL/DF + KEY label), the shared screen (303 note-grid tap/drag-draws real notes · drum 2D
> voice-grid tap/drag paints · MST PCF/CRU/GAT lanes), 808/909 pad strips (tap=select+audition-when-stopped) +
> the shared colour-coded context panel. **M2 (next):** the soft-key row is drawn but INERT — wire each key to
> swap the screen (FLAG/PERF/GEN/KIT + DF deep page/WAVE + KEY editing + per-step ACC/PROB/STRK + 909 METAL XY +
> MST RES/FB + DELAY buttons + drum MUT/REC + VOL·PAN·FINE). Gate before flipping the default: the feature-parity
> checklist in [`design/acidcandy-ipad-layout.md`](design/acidcandy-ipad-layout.md). The original draw-only mockup
> stays at [`acidcandy_ipad`](../tools/carts/acidcandy_ipad.c).**
> Maker-driven ground-up iPad layout, all 5 machines at once (NOT the roomyface tiles): narrow 303a/303b +
> MASTER knob-strips bracket a big shared SCREEN; 808(16)/909(11) as pad-bank strips stacked at the bottom.
> **STICKY FOCUS** (tap a nameplate → that machine's DEEP editor fills the screen; play stays live for all).
> Screen per focus: 303 = tb303 note-grid (colour=accent, shape=oct/slide/tie); drum = 2D voice-grid; MST =
> automation lanes. Drums have no column, so the bottom strip = voice PADS + one SHARED colour-coded context
> panel (in the 909's spare room) that follows the last-picked voice on either machine (blue=808 / yellow=909).
> Full model + the OPEN questions (drum VCE home, 303 pitch-gutter, 808 grid cell size) are in the cart's
> `de:meta`; run `node tools/orient.js acidcandy_ipad`.
> **This REPLACES acidcandy's current iPad rack** — `draw_rack` (acidcandy.c ~L2273, the `rack_view==1` path):
> a 2×2 of the full phone device-faces (909|808 / 303a|303b) + a master strip = "four phones taped together",
> four tiny screens, no focal point (the maker isn't happy with it). The mockup's bet = minimal per-machine
> surfaces + ONE big shared screen via sticky focus. TRADEOFF the maker accepted: give up editing all 4
> patterns AT ONCE (2×2) for a big calm one-at-a-time deep editor (play stays live for all).
> **COEXISTENCE (app is under App Store review — do NOT delete the old):** keep `draw_rack` as the shipping
> DEFAULT; wire the new layout as an ALTERNATE behind a toggle (e.g. `rack_view==2` / a dev flag), flip the
> default to it only when the maker is FULLY happy, remove the 2×2 only after that.
> **NEXT = maker locks the screen model, then WIRE** the new `draw_rack` variant — data shapes are concrete
> (per-step degree/acc/slide/oct/tie for 303s, voice×step for drums); the 303/drum grids already exist in
> `acidcandy.c` (seq_grid / draw_808 / draw_909) to lift from. **Wiring GATE = the feature-parity checklist**
> in [`design/acidcandy-ipad-layout.md`](design/acidcandy-ipad-layout.md) (every phone feature must reach the
> new layout before flipping the default off `draw_rack`; several OPEN: DF deep page, FLAG/PERF/GEN/KIT,
> MUT/REC, VOL/PAN/FINE, 909 METAL XY, MST RES/FB, DELAY buttons, per-machine SEND).
> **Resume-at:** [`design/acidcandy-ipad-layout.md`](design/acidcandy-ipad-layout.md) + [`design/responsive-first-device-face.md`](design/responsive-first-device-face.md#the-layers--cheapest-to-deepest).
> Hot files: `runtime/face.h` · `runtime/lay.h` · `runtime/ui.h` (shared — targeted `Edit`s only, the sound.h rule applies).

> **▶ ACTIVE THREAD (2026-07-20, later the same day) — harmony brain: SHIPPED v1 — `runtime/harmony.h` + the `chordwise` toy.**
> The build order below was executed engine-first in one session: **(2) DONE** — `harmony.h` holds the
> 13-function vocab + `HB_BOSSA`/`HB_COCKTAIL` style tables lifted VERBATIM; bossa + cocktail migrated
> with **byte-identical pinned-seed WAV A/B** (`cmp == 0`, seed 1234, 900 frames — the `radio.h`
> seed-compat rule held; the carts keep their walk loops + PRNG, calling `hb_pick`). **(3) DONE** —
> `hb_suggest` (ranked options + one-word reasons) + `hb_analyze`/`hb_chord_fn` (key-in, triads map to
> their seventh family, `-1` = honest out-of-vocab). **(4) DONE** — `chordwise` (the demand-82 toy:
> analyzer strip re-derives numerals every frame, NEXT panel with weight pips + reasons, 13-chord
> palette in the key, QWERTY entry Q-Y/A-J, doo-wop cold-open) carries the spec: **24 assertions**
> incl. the all-12-keys round-trip; `hb_selfcheck()` lives in the header (specs-on-an-includeable).
> **(1) flipped to LAST on purpose:** the LOOK pass is now a steering conversation — the v1 bake is the
> mockup; maker eyeballs `chordwise` in the editor, steers with reference images, then re-skin.
> Banked: **spec.js was broken repo-wide** (every cart's spec died at link — AudioToolbox missing since
> the mic lane) — fixed + streetlab re-verified 104/0.
> **NEXT (adoption, any order):** the template stations as per-genre weight sets over the one vocab ·
> chord-aware `improv.h` (feed it the current function) · `pocketbox`/`chordblossom2` speak the vocab ·
> the chromatic encoding design call (`V/x`).
> **Resume-at:** [`design/harmony-brain.md`](design/harmony-brain.md#whats-still-open-post-v1-2026-07-20).
> Hot files: `runtime/harmony.h` (now shared by 3 carts — targeted edits only, the sound.h rule applies).
>
> *(The original ready-to-build lane, for the record:)*
> **harmony brain: a shared next-chord + analysis engine (research done, ready to build).**
> Grew out of the demand-discovery drip: r/musictheory **demand-82** ("progression analyzer/suggester")
> is the loudest gap any drip has surfaced, and it's on-grain + oracle-verifiable. SHIPPED this session:
> the brief [`design/harmony-brain.md`](design/harmony-brain.md) with a verified three-layer code map —
> `bossa.c`/`cocktail.c` already hold a real functional-harmony **Markov engine** (functions + `TRANS`
> cadence table, `bossa.c:97-118`) but it's **LOCKED in two carts + generate-only**; `rad_lead_to`
> (`radio.h:96-144`) is the shared **VOICING** block (orthogonal); **analysis + suggestion don't exist**.
> Plus a deep-research survey (21 sources, 24 verified claims) folded in that **settles the model**:
> **1st-order Markov over key-relative roman-numeral functions + forced cadences** (= what bossa already
> does; higher order isn't worth it), and **ANALYZE must take the key as INPUT** (full auto RN analysis
> is unsolved — 43–52% SOTA) with a diatonic+borrowed-chord lookup, Krumhansl-Schmuckler scalar-product
> only as a fallback key guess. Genre = **weights over ONE shared vocab**, not different grammars.
> **Build order when picked up:** (1) mockup-first the chord-toy LOOK, like `ribbonpad`/Ribbon; (2) extract
> bossa's `TRANS`/functions into a shared `harmony.h`, proving byte-identical seeds survive (the
> `radio.h` seed-compat rule) — **generate before the inverse**; (3) add `hb_suggest` (TRANS read
> *forward*, ~2–4 ranked options) + `hb_analyze` (key-in + lookup); (4) a `spec()` oracle = known
> progressions → canonical roman numerals. **Still open (design calls, not research):** the minimal
> chromatic encoding (`V/x`, borrowed-degree flags) + the suggest option-count/ranking UX.
> The insight tying it together: **analysis is generation inverted** — one function vocab + transition
> model serves radios (generate), `chordblossom2` flavors (voice), `pocketbox`'s chord track, a
> chord-aware `improv.h`, AND the demand-82 toy. Harmony's `acid303.h`.
> **Resume-at:** [`design/harmony-brain.md`](design/harmony-brain.md#proposed-shape-a-hypothesis-to-test-not-a-spec).
> No hot files yet (a new `runtime/harmony.h` + a new cart; the brief is committed).

> **▶ ACTIVE THREAD (2026-07-18) — walkbox: a walking-bass step-sequencer ("303 done right, as a real pluck").**
> A TB-303 workflow driving the upright's real `INSTR_BOWED` pizz voice — the cell nobody had
> (`acidcandy`/`tb303` = the 303 workflow on a *synth* voice; `upright` = the pluck voice *played
> live*; **walkbox = 303 workflow + real pluck**). SHIPPED this session, all committed, cart plays:
> note-bars (draw the line, scale-locked E min-pent) + a **tabbed VEL | LEN drawable lane** (velocity
> → pluck attack `note_on` vol; length → staccato gate, top of lane = TIE/ring-on) + **SLD/OCT** rows
> + **SWING** (odd-16th shuffle, a port of tb303's clock) + GLIDE/TEMPO/TONE/RING + RND/CLR. Voice =
> mono pizz re-pitched per step; SLIDE = same-voice `note_glide` slur (both directions); TIE detaches
> the voice to ring on. Banked gotcha: `ui.h` `UI_MAX_WID=64` silently drops later widgets → use
> **one widget per row/lane**, compute the column from touch x (per-cell widgets killed the TIE row +
> knobs). **Resume-at:** [`design/walkbox.md`](design/walkbox.md) — the articulation roadmap (open:
> ghost-as-a-muted-timbre, hammer/pull, scoop/fall, presets + save/load) and the **modern-bass
> voice-swap** direction (the lanes are voice-agnostic; swap the pizz for an electric/sub voice).
> Hot file: `tools/carts/walkbox.c` (self-contained — no shared-header edits).

> **▶ ACTIVE THREAD (2026-07-18) — the CHORD-BLOOM rack (`chordblossom2`).**
> The winning answer to "make a radio song playable": NOT a radio turned inside-out (`bossabloom`
> felt like *"radio + a chord-picker"* — the machine still composes) but an **instrument you PLAY
> with your hands, wearing a genre FLAVOR** — a backing band that follows the chords you play. A
> fork of `chordblossom` (never branch — copy-to-new-cart). SHIPPED this session: a data-driven
> **Flavor table** (NEUTRAL / BOSSA / YACHT / CITYPOP; each row = comp+bass onsets · voicing ·
> timbre · groove · tempo · colour); **diatonic KEY mode** (the white keys ARE the in-key chords,
> labelled — a ii–V–I is just S/G/A); the clean **3-axis model** — **SPICE** strip (out-of-key
> chords you press) · **RICHNESS** (simple→lush) · **GROOVE** (sparse→busy), each ONE behaviour
> (killed the arm/toggle murk after a play-test rethink); a **living accompaniment** (4-bar phrase
> that breathes — swell + turnaround fill + human drops); a **tiny per-part PEDALBOARD** on the FX
> tab (HARP RVB/DLY/CHO · BASS DRV/CHO, per-slot FX, set-and-hold, lint-fx-frame clean); and
> **STOP + REST** (REST hushes the chord, the groove keeps going). BOSSA is tuned by ear
> ("sounds right"); YACHT/CITYPOP are plausible-but-untuned.
> **Resume-at: [`bossa-rack.md` → Open questions](design/bossa-rack.md#5--open-questions)** (its top
> carries the ★ BUILD FINDING that flavors-on-chordblossom supersedes the radio-rack plan) +
> [`genre-box-rosters.md`](design/genre-box-rosters.md).
> NEXT = tune YACHT/CITYPOP by ear · per-flavor SPICE sets + fills · the candy skin (per-flavor
> palette + a mascot) · RHYTHM pedals (needs a drum-slot refactor — drums are raw `INSTR_` hits) ·
> label cosmetics (Db7/Bb7, the `C MAjmaj7` readout). Hot files: `tools/carts/chordblossom2.c`,
> `docs/design/bossa-rack.md`, `docs/design/genre-box-rosters.md`. Related: `bossaface.c` (the candy
> vibe mockup) + the superseded `bossabloom.c`.

> **▶ ACTIVE THREAD (2026-07-16) — Android as a Google-Play build target.**
> The engine already runs frameworkless behind `platform.h` (`DE_NO_RAYLIB`), so Android is a **host
> shell + Gradle packaging**, not engine work. SHIPPED this session: the toolchain (NDK/SDK/emulator,
> sudo-free via Homebrew) + **spikes 0–3** — the real engine cross-compiles with the NDK and RENDERS
> (GLES2 fullscreen-quad blit of `de_framebuffer()`, host GPU) + SOUNDS (AAudio) + takes TOUCH on an
> arm64 emulator, all committed under `android/` with `android/build.sh` the one-command loop. Plus the
> editor's **🤖 export .apk** button (share popover, `EDITOR=1` build of the live buffer) — a
> sideloadable debug APK (arm64+arm32), no Play account/device needed.
> Emulator audio was flaky (a virtual-audio-device route conflict — BlackHole / Multi-Output device);
> the working recipe is **device-nudge-after-launch + landscape lock + a deep AAudio buffer**, all only
> needed on the emulator (real hardware is clean; audio survives rotation natively there).
> **Immersive fullscreen added 2026-07-17 (compiles, APK builds; NEEDS ON-DEVICE CHECK).** The
> Fullscreen theme only hid the status bar → the nav bar + edge-swipe system bars showed mid-game.
> `android_immersive()` in `cpp/main.c` now hides the bars via `WindowInsetsController` (API 30+;
> targetSdk 35 ignores the old flags) + `BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`, RE-applied on
> `APP_CMD_GAINED_FOCUS`/rotation (the missing piece), + cutout draw-in via `res/values-v27/styles.xml`.
> The Android twin of iOS's edge-gesture deferral. JNI is exception-guarded (can't crash); verify the
> nav bar truly stays hidden on a real phone — see the immersive gotcha in `design/android-plan.md`.
> **Resume-at: [`design/android-plan.md` → the spike ladder](design/android-plan.md#the-spike-ladder)** —
> NEXT = spike 4 (save → internal storage) → 5 (signed `.aab` for Play) → 6 (Play Billing via the
> existing `Store_*` gate; needs a Play Console account) → 7 (`build-app.js --android` multi-cart app).
> Hot files: `android/app/src/main/cpp/main.c` (the NativeActivity shell), `android/build.sh`,
> `docs/design/android-plan.md`.

> **▶ ACTIVE THREAD (2026-07-18) — the audio-input frontier (the engine HEARS *and SPEAKS*).**
> The reddit-gaps drip kept surfacing the SAME blocked wishes (hum→MIDI, pedals, live looping) — all
> one missing capability: the engine had no ear. Now it does, on every platform, and it vocodes. The
> arc, spike → ship → instruments → vocoder:
> - **The mic seam, all four platforms** — a device-free `platform.h` contract (host owns the device +
>   permission, pushes frames via `de_audio_input`; engine only analyzes): desktop CoreAudio
>   (`1cfe46aa`), WEB `getUserMedia` + iOS `AVAudioSession` (`2689ed68`), ANDROID `AAudio`+JNI
>   (`bc5599d2`). `mic_start/stop/active` + `mic_level` (RMS) + `mic_pitch` + `mic_record`.
> - **YIN pitch** (`2d552d25`) — `pitchscope` diagnosed the zero-crossing estimate octave-jumping; replaced
>   it with a YIN detector (octave-safe). `mic_pitch()` is now a real melody/controller axis.
> - **Carts**: `mictest`, `pitchscope`, `humtheremin` (hum→theremin), `voxbox`; `breakchop` gained mic
>   **beatbox auto-chop** (record → onset-slice onto pads) via `mic_record` (capture-then-freeze). Then two
>   more capture-then-freeze instruments: **`humseq`** (vein 2, hum→MIDI — a hysteresis note-tracker freezes
>   a hummed melody to a scale-locked loop on any INSTR) and **`singsynth`** (vein 3, voice sampler — hold a
>   vowel, loop it into a keybed instrument SK-1-style, draggable loop region). Both use the `ui.h` button bar
>   + `keybed.h`. (Gotcha banked: `rect`/`rectfill` are `(x,y,w,h)`, not corners.) Then **`hardtune`** (vein 3
>   flavour A, robot auto-tune) — a saw carrier locked to `snap_scale(mic_pitch)`, vocoded live, RETUNE slider.
> - **THE VOCODER** (`379fef80` Phase 1 + `46b45c35` Phase 2) — a master-stage 12-band carrier×modulator
>   filterbank in `sound.h` (`vocoder`/`vocoder_send`), fed live by the mic through a **lock-free
>   audio-thread PCM ring** (`sound_extin_*` + `vocoder_mic`). `vocode` = deterministic synth-modulator
>   showcase; `voxbox` upgraded to the REAL vocoder (sing → a chord speaks your words; "sounds like
>   Stevie Wonder"). Determinism carve-out: [ADR-0032](decisions/0032-live-mic-effects-are-live-only.md)
>   (live-mic-through = live-only; capture-then-freeze stays deterministic). All gates green; desktop +
>   web live-verified by the maker.
> - **VOCODER v2 — the unvoiced/sibilance band** (`e7d782fe`): `vocoder_unvoiced(amount)` — a source-agnostic
>   detector (top-band energy fraction) swaps the top bands' excitation from the tonal carrier to band-limited
>   noise, so consonants (s/t/sh/f) cut through instead of turning to mush. `vocode` A/Bs it deterministically
>   (consonant windows: brightness ~0.03→0.2, centroid ~3.8→5.3 kHz, no peak rise); `voxbox` rides it live.
> - **TRANSPARENT auto-tune — OFFLINE primitive SHIPPED + real-voice CONFIRMED** (`b5b26cf6`): `sample_autotune(slot,
>   root, scale, amount)` — formant-preserving pitch correction (whole-buffer TD-PSOLA) on a
>   recorded take, a game-thread sibling of `sample_read/load`. Proven in two offline spikes (v1 known-f0 formant
>   hold, v2 detector-epoch correct-to-scale), byte-faithful cart-C→engine, gated by **`tools/formant-check.js`**
>   (f0 + formant-peak oracle). Showcase = **`mictune`** (rebuilt as a clean app: sing → in-tune, R tunes your
>   voice; the old test-bench version was "confusing"). Maker sang in → "sounds pretty good". *Done.*
> - **LIVE real-time auto-tune — SPIKE BUILT + PARKED** (`d067f257` + phase-lock fix): `autotune_mic(root, scale,
>   amount)` — streaming two-pointer TD-PSOLA on the audio thread off the `sound_extin` ring, monitored into the
>   mix; cart **`livetune`**. Formants preserved + amplitude smooth (phase-lock epochs killed an initial "pulse"),
>   but **audible pitch WARBLE remains**. Diagnosed against a real vocal via the new **`DE_MIC_WAV` harness**
>   (`8c138015` — feed a WAV as the mic headlessly, `tools/testdata/vocal-8s.wav`): a fundamental-band jitter
>   metric shows octave-continuity / retune-glide / hysteresis all MEASURE as no help → the warble is the streaming
>   pitch/epoch **resolution**, needs a YIN-grade real-time tracker or the phase vocoder (a dedicated effort, not
>   spike tweaks). See `design/transparent-autotune.md` §"live real-time path" for the full write-up.
> **Resume-at: [`design/audio-input-frontier.md`](design/audio-input-frontier.md) — the ranked map.** Auto-tune
> arc is COMPLETE for the offline feature; the LIVE path is feasible-and-parked (warble). Open frontier, ranked:
> (1) the **pedal tier / live looper** (the `sound_extin` ring is the built foundation — the loudest unmet wish);
> (2) **vocoder v2 tail** — mic-rate resample (non-44.1k device mics drift the ring) + on-device latency tuning;
> (3) **beatbox→live drum trigger**; (4) **live-autotune warble** if revisited (real-time YIN / phase vocoder).
> `voxroll` decouples formant/pitch but only on synth `INSTR_VOICE`, not a real-mic corrector.
> Hot files: `runtime/sound.h` (vocoder + extin ring + `sample_autotune`/`autotune_mic`), `runtime/mic.h`
> (analysis + record + ring write), `runtime/studio.h`/`.c` (the seam + `DE_MIC_WAV`), the per-host capture
> (`mic_desktop.h`, `ios/Sources/AudioEngine.swift`, `android/…/main.c`). Reference carts: `vocode`/`voxbox`
> (vocoder), `mictune` (offline auto-tune), `livetune` (live spike). Test harness: `DE_MIC_WAV=<wav>` + `tools/testdata/`.

> **▶ ACTIVE THREAD (2026-07-29) — the candy acid RACK (`acidcandy`).**
> `acidcandy` (160×100 ×4) packages `acidrack`'s guts as the **device-face paradigm** instead of the
> accordion: a colour-**cartridge** nav strip of five FOCUSABLE machines on ONE transport — **2×303**
> (`acid303.h`; 303b an octave up = the bass+lead duo), the full **808** (`tr808.h`) and **909**
> (`tr909.h`), and a **MST** master. Every voice is honest — `runtime/tr808.h` + `runtime/tr909.h` were
> EXTRACTED from the tr808/tr909 carts this session (the `acid303.h` move; refactor verified *within the
> run-to-run noise floor*, since the drum voices' noise isn't sample-reproducible), so acidcandy can't
> drift from the reference machines.
> SHIPPED: gear-drag knobs (vertical=value, pull sideways=fine gear, double-tap=reset); the cartridge nav
> (tap body=focus, LED=mute from any face) + a **drag-release-BOUNCE fix** (a stray 2nd click ~8 frames
> after a knob-drag release was landing on a mute LED → silence; now a `TAP_SETTLE` guard ignores nav taps
> for ~200ms after a drag lifts — repro was `build/.rec` replay); the **303 note-bars** (drag = free-draw
> the melody, height=pitch scale-snapped, bottom band=rest; tap=toggle) + a **FLAG palette** in the screen
> (arm ACC/SLD/TIE/OCT+/OCT-/LEN, paint-drag across) that wires per-step tie/octave + per-line
> **LENGTH/polymeter** into playback; the roll draws every flag (glide lines / tie bars / oct ticks); the
> **MST** face (GLU/FLT/RES/FB/PUMP + delay TIME + per-machine SEND, default glue tames the mix) with a
> **drawable PCF** lane.
> **LATEST (2026-07-19) — a LIVE-SET + MST-layout push (all shipped + committed):**
> the 808/909 TOOL-vs-VIEW cleanup + `draw_arms` deletion is long done (both drum faces, the far-right
> stacked-word tool selector). This session added, in order:
> — **master SWING** (one rack-wide `g_swing`, ReBirth model — drums AND both 303s lock; drums drag the
>   odd-16th fire, a 303 delays its step FLIP, same `sw=g_swing*0.60` fraction) + **master TEMPO** (`g_bpm`);
> — **queued bar-quantized bank switch** (tap a bank = arm + blink, lands on the next bar downbeat);
> — **303 PERF lenses** (a PERF soft-key: HALF speed — the 2X/8-12 speeds were later DROPPED · ACC accent-all ·
>   OCT · REV · STAC/GLIDE slide-flip · ROLL),
>   **non-destructive read-lenses**, each **TAP = latch (persists across faces) / HOLD = momentary** (`lcdlatch`);
> — **FLAG-screen NOTE-on** (FL_NOTE, the default armed flag — add notes without leaving for SEQ);
> — **SLIDE-GATE BUGFIX**: acidcandy's staccato `acid_gate` was cutting slid/tied steps at 70% → **manual
>   slides never actually glided**; fixed to tb303's `on && !slide` rule (proven: manual all-SLD == a pure
>   glide, 1.00000; no-slide patterns byte-identical). tb303/acidrack were always fine — this was acidcandy-only;
> — **LCD grown to h30 on ALL FOUR faces** (one consistent size; contains the soft-keys; 1px gap to the content
>   below); **MST redesign** — SWG + TEMPO are a matching **gutter knob pair** (right of the LCD; `gknob` shows
>   the real value), **DELAY division buttons moved UNDER the LCD**, SEND knobs pulled in to free the corner;
> — **three drawable master lanes** on the MST screen (MIX/PCF/CRU/GAT): **PCF** filter/tone (green) +
>   **CRUSH** bitcrush/texture (orange) + **GATE** chop/rhythm (pink), each `m*[16]` applied set-and-hold on step change;
> — ~~a DUB pad in the freed MST corner~~ — **REMOVED 2026-07-21** as gimmicky (its `dub_*` globals are gone;
>   the MST right column is now just SWG + TEMPO). Its old "needs an ear check" item is therefore moot.
>
> **2026-07-29 session — the FX HUB + a formant that SPEAKS.** The phone MST soft-key column was full and one
> key per effect does not scale, so the master DEVICES moved behind a single **FX** key (`mstflow 8`): a MENU of
> chips wearing their `fxicons.h` glyphs, with **SWP retired into it** and the three drawable lanes keeping their
> own keys (they are the rhythmic soul, not devices). Adding the rest of the MORE-MASTER-FX shelf
> (tape/wah/ringmod/drive/spring) is now one `FXK[]` entry + a knob branch each. **DRY** = a tap-latch /
> hold-momentary kill that holds every device dry WITHOUT clearing its arm flag, so lifting it restores the exact
> blend — verified **byte-exact** (two devices armed + DRY renders the everything-off baseline sha).
> New **VOWEL** device (master `formant()`), and **SPK** makes every fresh 303 note-on advance a vowel WORD with
> the vowel gliding to each syllable — the glide is what `formant()`'s ride-safety buys, and no
> buffer-rebuilding effect (crush/flanger/gate) can do it. Pauses came next (the maker: *"sometimes we need a
> little pause"*) and they are what turns a slur into speech: a gapless word moved the spectrum no more than a
> STATIC vowel (centroid std 1270 vs 1281), rests took it to 1964 (+55%). Both pause axes are knobs now —
> **DENS** (how many, via a per-slot threshold array so rests arrive in a fixed musical order rather than
> randomly) and **GAP** (how deep) — five knobs on the page while SPK is lit, which is safe because
> `lcdknob_cell` is height-bound here so 4→5 does not shrink the knob.
> Two bugs worth remembering, both inaudible as bugs and both caught by the `DE_TRACE` watches, not by ear:
> simultaneous notes on the two 303 lines **double-advanced** the word and skipped slots (a skipped rest is a
> pause that never happens), and the gate's **symmetric** close could not finish inside one note, so the pause
> was an inaudible dip (it is asymmetric now — snap shut, ease open, which is what a stop consonant is).
> **▶ TOP OPEN — needs an EAR CHECK (not code), the reason we stopped:**
> **(1) SPEAK IS HARD TO HEAR** — the maker's verdict 2026-07-29. The mechanism is PROVEN (clip 06 covers it) so
> do **not** re-verify it; this is voicing/routing. *Check the dumb thing first:* both 303s boot **muted** and SPEAK
> is driven entirely by 303 note-ons, so with both muted there are zero syllables and you hear only a static vowel
> on the drums. Then the top lead, which the measurement already predicted: speak-on vs speak-off left centroid std
> unchanged because the 808+909 kit dominates the variance — so move the formant OFF the master bus onto the 303s
> only (`instrument_formant` on slots 6/7 + subs 36/37), which also frees MIX to go well above 0.7 without eating
> the hats. `--solo-slot 6` decides it in one command: speech clear in the stem = masking (reroute); weak even solo
> = the vowel contour itself is too subtle. **(2) the GATE lane** — the maker's verdict was *"none is a
> clear win"*: `gate()` is a THRESHOLD/dynamics gate, not a pure per-step volume chop, so the cut is
> level-dependent (mushy). The real fix for a clean trance-gate = wire a **master VOLUME** the GATE lane can
> ride (the long-standing `level[]` master-vol TODO). CRUSH is the keeper of the two new lanes.
> **Resume at the cart's live punch-list — the `de:meta.todo[]` in
> [`tools/carts/acidcandy.c`](../tools/carts/acidcandy.c)** (`node tools/cart-todos.js acidcandy`); the newest
> entries (DUB / GATE / CRUSH / PERF / LCD-GROW) carry the mechanism + every caveat above.
> **303 realism pack — ALL WIRED (2026-07-20):** the `runtime/acid303.h` features are all now surfaced in
> acidcandy — continuous RES, analog DRIFT (FX-panel knob, rides live), per-303 classic⟷DF **voicing** switch
> (with SAW/SQR reachable in vanilla), and the per-303 **CLEAN/RAW** saw toggle (FX panel; opt-in PolyBLEP,
> raw is default). Engine: `instrument_bandlimit` API + `Acid.classic/clean/drift` (all non-destructive struct
> fields). Story + rationale: [`audio-notes.md §26`](design/audio-notes.md#26-303-realism--it-sounds-kinda-digital-2026-07-19).
> Other OPEN there:
> PERF follow-ups (the 2X **funny-accent-order**, octave-shove + reverse lenses, a drums PERF layer), the
> **REC/mode hint-outlines** teaching idea, the **mascot/soul** (deferred), SAVE/LOAD + the SONG layer.
> (PARKED: mute scenes — redundant with the mute LEDs; the LCD-toast feedback idea — maker didn't like it.)
> Hot files: `tools/carts/acidcandy.c`, `runtime/tr808.h`, `runtime/tr909.h`, `runtime/acid303.h`. Design:
> [`device-face-paradigm.md`](design/device-face-paradigm.md) · [`candy-style.md`](design/candy-style.md) ·
> [`control-vocabulary.md`](design/control-vocabulary.md). Cousin lane: the acidrack redesign (R5,
> `disclose.h`) below — acidcandy is the candy device-face *take*; that lane re-lands acidrack itself.

> **▶ ACTIVE THREAD (2026-07-13) — demand discovery: the reddit-gaps drip is running.**
> A new tool (`tools/reddit-gaps.js`) mines a music tribe's public RSS for unmet demand → clusters →
> cross-references our cart shelf → ranks gaps. TWO tribes done
> ([note 022 · ipadmusic](field-notes/022-demand-discovery-ipadmusic.md),
> [023 · synthesizers](field-notes/023-demand-discovery-synthesizers.md)) — convergent thesis: the
> opening is a *cheap, playful, beginner lo-fi toy in classic-gear clothes*, NOT a feature.
> A macOS LaunchAgent **drips one sub every 6 h** (rotation: `tools/reddit-gaps-subs.txt`), so the
> caches KEEP GROWING between sessions.
> - **Where the freshly-downloaded data lands:** `tools/reddit-gaps-cache/<sub>.json` (gitignored —
>   regenerable, never committed per Reddit policy) + the run log `tools/reddit-gaps-cache/drip.log`.
> - **To see what's been collected next time:** `ls -lt tools/reddit-gaps-cache/` (newest first),
>   then `node tools/reddit-gaps.js <sub>` renders the gap report from cache, or `--raw` dumps the
>   mined wish list. `cat tools/reddit-gaps-cache/drip.log` shows the drip's history.
> - **Resume-at:** when a sub's cache looks rich, read `--raw` and write the next numbered field note
>   (the interpretation is a judgment step, not automated) — pick-up point in
>   [`design/demand-discovery.md`](design/demand-discovery.md#where-the-findings-live-and-grow).
> - Hot files: `tools/reddit-gaps.js`, `tools/reddit-gaps-subs.txt`. Drip stop/start/fire-once
>   commands are in that doc's "Continuous fetching — the drip" section.
> - **First build-output of the thread → the tombola cart** ([`design/tombola.md`](design/tombola.md)):
>   the demand's one genuinely-missing on-grain gap (tape=already `loopstation`, chord-toy=already
>   `chordblossom`), designed as a physics sequencer on the [device-face paradigm](design/device-face-paradigm.md)
>   (§1f = the fuller OP-1 device analysis for the paradigm-sharpening pass). **Not built.** When
>   building: reuse the new `runtime/physics.h` (verlet balls) + `circlemachine` (note wiring) — the
>   tombola only adds a rotating drum + trigger-line — resume at [`design/tombola.md`](design/tombola.md#prior-art-in-the-repo-reuse-and-what-s-already-covered).

> **▶ ACTIVE THREAD (2026-07-10) — editor↔cart workflow: CODE round-trip + the PIXEL side (Option D) SHIPPED.**
> Closing the gaps that bite when you hand-edit a cart in the editor instead of going through
> `tools/carts/` + CLI (all in [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md)).
> **Gap 1b (provenance) + Gap 1 (code save-back) are DONE and committed:**
> - **`de:meta.slug`** backfilled across all 455 carts (`tools/backfill-slug.js --write`) and
>   **required** by `lint-carts.js` (missing or `slug !== filename` fails). Slug is the PNG→source
>   anchor. (Bug caught + fixed in the backfill: it built the `.replace()` replacement as a STRING, so
>   `$1`/`$&` in a cart's own de:meta expanded — `mule.c`'s `$100` → invalid JSON; now a replacement
>   FUNCTION. Blast radius was 1 cart, repaired.)
> - **"save to source" button** (editor): writes the Code-tab buffer back to `tools/carts/<slug>.c`
>   and re-bakes the gallery `.cart.png` IN PLACE (keeps thumbnail + sprites/map), regenerates
>   `index.json`. IPC `cart:save-to-source` in `main.cjs`; `saveToSource()` + busy-toast feedback in
>   `shell.js`. Source only; NOT a git commit (stays `node tools/cart-commit.js <slug>`). Key gotcha
>   baked in: it re-embeds like `cart:save` in-place — does NOT shell out to `make-cart.js <src> <png>`,
>   which rebuilds sprites from the `.cart.js` and would BLANK a hand-drawn sheet (make-cart.js ~378).
> - Also shipped: a Settings → "cart panel" toggle to hide the save/load/save-to-source row.
> **PIXEL side (Gap 2, the sprite story = STATUS item 23) — Option D SHIPPED (bake + persist halves).**
> The finding that **all generator carts are `.cart.js`-driven (ZERO hand-drawn)** meant the real need was
> *reversible touch-ups on a generator cart* → straight to D (not A's freeze / B's marker). Committed:
> - **`tools/lib/sprite-patch.js`** — the slot-level overlay CORE (palette-index space, no PNG decode):
>   `fingerprintSlot`/`fingerprintSheet` (over the generator OUTPUT, not the source), `applyPatch`
>   (fast-path → per-slot base-match → pure-addition-over-empty → else STALE: drop loudly + prune, so a
>   wholesale regenerate self-empties), `buildPatch`, stable serialize. Tested 26/26 core + 8/8 bake-int.
> - **`make-cart.js`** — `buildSpriteSheet` split into `genSlots`+`slotsToSheetPng` (byte-identical, verified
>   on 4 real carts); `bakeSprites()` composites a sibling `tools/carts/<name>.sprites.patch.json`; the
>   `<src> <png>` bake mirrors the surviving patch into the `.cart.png` as `de:spritepatch` (`--run` preserves it).
> - **Editor save-to-source** (`main.cjs cart:save-to-source` + `shell.js readSheetSlots`/`saveToSource`) —
>   diffs the sprite canvas vs the RE-RUN generator, writes only changed slots (deletes the file when nothing
>   differs). Stateless (generator re-runnable → no snapshot). Data-layer-validated on 5 real carts (untouched
>   sheet ⇒ empty patch, no spurious diffs); **NOT yet eyeballed live** (needs `make` to restart Electron).
> **Save-path VERIFIED LIVE (2026-07-10, maker)** — hand-edit → save-to-source → patch persists across `--run`.
> **Discard + indicator BUILT (2026-07-10, data-layer tested 5/5, NOT yet eyeballed live):** the pixels tab shows
> a `#sprite-patch-bar` naming the hand-owned slots on load (load handlers thread `de:spritepatch` →
> `applyCart` → `setSpritePatchBar`) + a **"discard hand-edits"** button (`cart:discard-sprite-patch`: delete
> sibling → re-run generator → drop the chunk → reload canvas). **NEXT:** (1) `make` (main.cjs/preload changed),
> open a patched cart → confirm the bar names the slot + discard restores the generator sprites; (2) A's freeze
> stays the future "promote a cart gone hand-drawn" path. **Edge:** a cart whose `.cart.png` already drifted from
> its generator captures the drift as a patch on first save (defensible; `--run` rebake cleans it).
> **Resume-at: [`design/editor-cart-workflow.md` → Gap 2 the sprite story](design/editor-cart-workflow.md#gap-2--the-sprite-story--status-open-item-23)** ("Option D — what shipped").
> Hot files: `editor/electron/main.cjs` (`cart:save-to-source`), `editor/src/shell.js`
> (`saveToSource`/`readSheetSlots`), `editor/src/sprite-editor.js` (the canvas), `tools/make-cart.js`
> (`bakeSprites` / the `<src> <png>` bake), `tools/lib/sprite-patch.js` (the core).

> **▶ ACTIVE THREAD (2026-07-10) — the worldgen ladder + the junction grammar (realistic roadgen).**
> The ladder (rungs 0–3 shipped earlier) is now built end-to-end AND DRIVABLE, and the road-junction
> grammar is extracted + consumed. All gated + committed:
> **Rungs 4–5 (in `citygrow`):** per-district minor-street FILL — the arterial graph's planar faces →
> streetlab-pattern presets (grid/organic/cul-de-sac/superblock) → stitched onto the arterials — then
> CALIBRATED to real Rotterdam via the new **`sndi-check --compare`** gate (five SNDi metrics dead-on;
> T-junction share 1.1%→64.6%). Clip `citygrow/03-districts`. The residual deg-4+/circuity gap is a
> documented STRUCTURAL ceiling (arterial X-crossings + straight minors), not fill tuning.
> **Rung 5.5:** the grammar extracted to **`runtime/citygen.h`** (behaviour-preserving); **sloop's M
> key drives a generated CITY** (`citygen_road_at` behind `road_at` — a 3rd producer beside stub / OSM /
> spine). **Rung 7:** its streets are LINED WITH COLLIDABLE BUILDINGS (`citygen` `cg_lots()` → sloop
> `OB_HOUSE`). Clips `sloop/06-citygen-city` + `07-citygen-buildings`; `spec.js sloop` 25/0.
> **The junction grammar — `runtime/roadkit.h` (Track-B):** **B2** the pure geometry (`curb_return`,
> `edge_corner`, `rk_count_corners`, `rk_cross_hw`) + **B3** the N-arm-native field renderer (`RkField`)
> extracted from streetlab byte-identical (spec 104/0, mirror-diff 68=68, road-check --all all PASS),
> and **citydrive draws curb-return junctions through it** (J key, ground metres, projected; a
> `spec()` 11/0 added first as the render safety net).
> **B4 SHIPPED 2026-07-10 — the interchange grammar.** roadlab's ramp splines + topology (`Port`,
> `rk_make_junction`, POLICY/classify_turn) extracted into `roadkit.h` (pure over `Port[]` + `present[]`,
> byte-identical: roadlab spec 25/0 + render 60/60). **Consumed:** `citygrow` draws citygen's 6 grade-2
> junctions as real cloverleaf/trumpet interchanges (I key hops between them; a clean cloverleaf renders).
> Track B (streetlab → roadkit at-grade field + grade-separated interchanges) is COMPLETE.
> **Resume-at (two open forks, both specced):**
> (1) **the N+M reconciliation** — unify citygen's world model with `worldnet.h`'s β-skeleton lattice
> so the N-spine + M-city are ONE infinite world (two gated spine edits: `get_node`/`get_hub` from
> citygen density; highways lead into citygen cities). See [`worldgen-plan.md`](design/worldgen-plan.md) rung 5.5.
> (2) **polish** — suppress citydrive's round-joint disc at near junctions so the curb-return fully
> replaces the blob (today it layers over it) + per-pixel field-fill for exact N-arm asphalt.
> Hot files: `tools/carts/{citygrow,sloop,citydrive,streetlab,roadlab}.c`, `runtime/{citygen,roadkit,worldnet}.h`
> (shared — targeted edits only). **`roadkit.h` is now the shared dependency of all five carts** — any edit
> to it must re-gate every consumer, not just the one you're touching. Gates to keep green: `spec.js sloop`
> 25/0 · `spec.js streetlab` 104/0 · `spec.js citydrive` 11/0 · **`spec.js roadlab` 25/0** · streetlab
> `mirror-diff` + `road-check --all` · `sndi-check --compare build/citygrow-city.json
> data/rotterdam-netherlands.rvb` PASS. For a roadkit interchange edit, the byte-identical render check is
> the real safety net (topology spec can't see geometry): dump the committed seed
> `tools/clips/roadlab/01-junction-cycle.script` BEFORE and AFTER the edit and diff — must be byte-identical
> (`node tools/play.js roadlab script tools/clips/roadlab/01-junction-cycle.script --frames 60 --dump <dir>`).

> **▶ ACTIVE THREAD (2026-07-07) — responsive instrument UI: playbook, epiano, scale-grid.**
> A research question ("what's the best responsive UI for a music cart?") turned into
> reusable process + two live design docs + a clearly-scoped new feature to build. **What shipped
> (docs/tools, all committed):**
> - [`design/acidrack-ui-research.md`](design/acidrack-ui-research.md) — external survey of the
>   303/909/808 + best clones + the touch/density numbers (48px floor, band table).
> - [`guides/responsive-instrument-ui.md`](guides/responsive-instrument-ui.md) — the reusable
>   **playbook**: sound→inventory→steal-IA→tier→**brief**→prototype→sweep→hands→ship, with the
>   field-note-018 traps baked in as guards.
> - [`decisions/0028-sensible-defaults-optional-tweaks.md`](decisions/0028-sensible-defaults-optional-tweaks.md)
>   — the rule: pick the stranger-legible default, ship it, leave a **seam**; don't agonize, don't
>   over-configure. Wired into [design-system](design/design-system.md) §5 + the playbook.
> - `tools/carts/epianofit.c` — the step-4 layout **MOCK** (no audio): device-fit + finger unit +
>   disclosure across all shapes. Keys: `1-5` lock device / `0` auto / `m` machine / `f` fx / `s`
>   scale / `r` key / `i` iso-layout / `g` force piano-or-grid / `n` native full-bleed.
> - [`design/epiano-layout-brief.md`](design/epiano-layout-brief.md) — **re-scoped** to the FAITHFUL
>   epiano (the classic `keybed.h` piano that scales with width + a disclosing sound panel).
> - [`design/scale-grid.md`](design/scale-grid.md) — the scale-locked isomorphic pad grid **split
>   out** as its own feature (a *general* note surface, not epiano's soul — the maker wants the piano
>   kept AND the grid, eventually).
> - **SHIPPED (2026-07-07): the `scalegrid` cart** (`tools/carts/scalegrid.c`) — the playable,
>   sound-bearing showcase, device-tested on multitouch, pinned by a **71-assertion `spec()`**. 11
>   scales (incl. blues + the SoundForest "FOREST" voicing), ROW = OCT↔4TH toggle, SQR↔HEX packing
>   (equidistant-neighbour Tonnetz grid, nearest-centre hit-test, pixel-correct regular hexagons),
>   fill-both-dims finger-first sizing + a SIZE cycle, and a VOICE cycle (PD/EPIANO/MALLET/ORGAN/PLUCK).
>   No-gap lattice proven across all scales × both modes. The maker's verdict on glass: "a very nice
>   musical toy."
>
> **Resume-at: the scale-grid "where does it live" question ([scale-grid.md §3](design/scale-grid.md#3-where-does-it-live-answered-b-c)) is ANSWERED B→C** —
> built as its own cart first, grid maths kept in self-contained pure fns (`compute_grid`/`pad_midi`/
> `pad_center`/`pad_at`/`hex_verts`). **The one open step: extract those into a `grid.h` library**
> (twin of `keybed.h`, reuse `solo.h`'s scale-lock) so the whole shelf reuses it — then wire epiano's
> optional **editor-swap** to it. Separately, epiano's faithful Phase-3 (piano scales with width) per
> its brief. **Both, eventually — the grid does not replace the beloved piano.**
>
> **Hot files:** `tools/carts/scalegrid.c` (the shipped grid — extract from here), `runtime/solo.h`
> (scale-lock to reuse for grid.h); `tools/carts/epianofit.c` (the earlier silent layout mock, still
> the epiano-brief reference). Gate: `node tools/spec.js scalegrid` (71/0).

> **▶ ACTIVE THREAD (2026-07-10) — multiplayer: WebRTC P2P (rung 5b) — SHIPPED + PUBLISHED; wire-side diagnostics added (follow-ups parked).**
> Steps 1–4 shipped (commit `05a5dc76`): the WebRTC DataChannel is now the WEB game
> transport (`de_rtc_*` EM_JS shim in `runtime/net.h`), the relay reused **unchanged**
> as signaling only. Play-tested Mac↔iPhone over wifi at LAN speed — the rung-5a
> problem (3 fps + freezes from tromboning through the Render relay at ~330 ms) is
> gone. Signaling + the seed handshake ride the channel; everything above the
> `net_transport_*` seam is untouched. The spike's two potholes are baked in
> (joiner-announces-first; binary signaling told from `ROLE` by the `DN` magic).
> **Jitter fix so far = a blunt one:** the phone's ~70 ms wifi radio-sleep spikes
> stalled the old 3-frame/50 ms cushion (a 1-frame hitch every 1–2 s), so `NET_DELAY`
> is bumped to a **fixed 10 frames (~165 ms)** — feels good, at the cost of input lag
> on clean links. **Pairing UI:** a Host/Join split (gallery + in-cart bar); Join via
> native `prompt()` because an inline `<input>` is blocked by the running cart's key
> handlers on iOS. **Resume-at:** [`multiplayer-research.md` → the step table](design/multiplayer-research.md#the-step-table)
> (rung 5b). **Published 2026-07-07** — pong is live on github.io (it's
> the only netplay cart, so that's the whole rollout; the Render relay needed no
> redeploy — it's signaling only now). **PARKED follow-ups** (not being worked):
> **step 5 (adaptive `NET_DELAY`)** — the fixed 10-frame cushion feels good but adds
> lag on clean links; adaptive sizes it to live jitter to claw that back (the
> maker's-call "when we want to sand off the floatiness"). **step 7 (TURN)** — for the
> un-punchable ~10–20% (today they see "connection failed - reload"); needs a free
> Cloudflare/Metered account. **Diagnostics pass shipped 2026-07-10** (`aaca7a36`): in-band
> RTT probe (`NET_PKT_PING`/`PONG`, ~2 Hz — step 5a's adaptive input now exists as
> `net_stat_rtt_ms`), rx inter-arrival gap counter (splits wire-outage from cushion
> starvation per STALL), wall-clock stamps on every net-debug log line (correlate freezes
> with a concurrent ping by time of day), and the web tick books stalls into the same
> counters (the WebRTC path was blind before — F2 now reads the same on both targets).
> **Next time both machines are on the office wifi:** run
> [the office-session checklist](design/multiplayer-research.md#next-office-session--the-checklist)
> (~15 min — seals or refutes the 07-09 shared-AP congestion verdict with direct
> correlation + first measured look at the phone-side stutter). Hot file if resumed:
> `runtime/net.h` (targeted `Edit`s, shared). Gate: `node tools/net-check.js`. Local
> play-test: `node tools/net-relay.js --serve site`.

> **▶ ACTIVE THREAD (2026-07-10) — device-adaptive layout (the acidrack redesign · Phase 3 = R1–R6).**
> Foundation is DONE (Phases 0–2: `runtime/lay.h` + a resizable/growable-framebuffer canvas + iOS
> fill/safe-area/rotation). The **`acidwire` wireframe did its job** — interactive, felt on glass across
> phone portrait/landscape + iPad, all four states; its lessons are field note
> [020](field-notes/020-the-fit-cart-earns-it-on-glass.md). **R1** (brief) captured, **R2**
> (`runtime/disclose.h` — shape + finger-budget accordion + stack) SHIPPED + proven in acidwire
> (`27637b26`/`d96c4404`); **R3** (`finger_px()`/`device_class()` — real backing-scale finger unit)
> SHIPPED + verified on device (`7102af8b`).
> **Status + what's-left + the sequence now live in ONE scoreboard — Resume at**
> [`device-adaptive-layout.md` → Where this stands](design/device-adaptive-layout.md#where-this-stands-scoreboard).
> Short version: **R5 next** (port acidrack onto `disclose.h` + `finger_px()` + make the deferred
> CONTENT calls on glass) → R4 alongside → R6 (`epiano`) last.
> Hot files: `runtime/disclose.h`, `tools/carts/acidwire.c`, `tools/carts/acidrack.c`. Ledger:
> [`STATUS.md`](STATUS.md) #2. Exemplar/guide: [`guides/interactive-wireframes.md`](guides/interactive-wireframes.md).

> **▶ ACTIVE THREAD (2026-07-19) — store / ASO + the app-trailer builder.**
> **LATEST (2026-07-19) — Tiny Acid Jam is LIVE ON ASC as a draft (the FIRST standalone single).**
> Per [`design/launch-sequence.md`](design/launch-sequence.md)'s single-first plan, `acidcandy` ships
> as its own app **Tiny Acid Jam** *before* the Tiny Jam umbrella. Renamed this session from "Acid
> Candy" (the old name collided with candy-match-3 games — poisoning the ASO seed — *and* a real
> sour-candy brand; "Tiny Acid Jam" pre-brands into the umbrella). The **cart slug stays `acidcandy`**
> (no provenance churn) — only the app + the cart's display title changed.
> Done + committed: `apps/acidcandy` → **`apps/tinyacidjam`**; bundle **`com.mipolai.tinyacidjam`**
> (registered in the portal); **ASC app record `6792504925` CREATED** (name reserved globally, private
> "Prepare for Submission" draft, v1.0 en-US); listing PUSHED live + verified in-sync via
> `node tools/asc-push.js tinyacidjam --metadata` — title **"Tiny Acid Jam: 303 Groovebox"** (28/30) /
> subtitle **"808, 909, house in your pocket"** (30/30) / the machine-numbers keyword field; `seo-brief.md`
> regenerated on **genre seeds** (acid house/303/groovebox/techno), dropping the candy vocabulary.
> **Price DECIDED = $1.99** (cheap-paid, one-time; .99 charm point, low friction for a no-reviews first
> launch) — recorded as `"price"` in `app.json`, BUT the **base price is a MANUAL ASC step** (Pricing &
> Availability tab — `asc-push` pushes IAP prices only, and this app has no IAPs).
> OPEN before it can SUBMIT (all deferred by the maker for now): (1) a **description** — `app.json` has no
> `listing.description`; draft against `seo-brief.md` then `asc-push --metadata`; (2) **screenshots** — 0
> today; `store-shots.js` from a `play.js --dump` frame → `asc-push --screenshots`; (3) a **standalone iOS
> build** (`.ipa`) — needs wiring like Tiny Jam's `ios/testflight.sh APP=tinyacidjam`; (4) in ASC: set the
> **$1.99 tier**, age rating, privacy-policy URL. **Resume-at:** `apps/tinyacidjam/app.json` +
> [`design/launch-sequence.md`](design/launch-sequence.md) "For Tiny Acid Jam specifically".
>
> **Buy-screen crash FIXED (2026-07-06, commit `07690c9b`):** the "instant, random" abort on the
> Tiny Jam menu/purchase screen was a **data race** — `Store.unlockedIDs` (a Swift `Set`) read by the
> C entitlement gate every frame while a StoreKit `Task` reassigned it → nano-heap corruption surfacing
> later at an unrelated `malloc`. Never reproduced off-device (desktop stubs `Store_*`). Now
> `NSLock`-guarded. Full lesson in `ios/README.md` §Gotchas — any per-frame `@_cdecl` bridge must be a
> lock-guarded snapshot, never a bare Swift collection.
> **Store-identity day (2026-07-06), all committed:** the App Store name **"Tiny Jam: Pocket
> Music Toys" is RESERVED** on App Store Connect (record created, not public); shipping bundle id
> is **`com.mipolai.tinyjam`** (registered in the dev portal; `apps/tinyjam/app.json` updated —
> the `com.tinyjam.hello` in `ios/project.yml` is dev-loop-only, see the comment there); the
> manifest **`icon` key is live** (`build-app.js --ios` → single-size asset catalog, sim-verified
> in `Assets.car`); **`ios/testflight.sh` RAN TO COMPLETION (2026-07-06)** on the upgraded box
> (macOS 26.5 + Xcode 26.6 at `/Applications/Xcode26_6.app`): **v0.1 build 202607061929 uploaded
> to App Store Connect** (cloud-signed Release, name reservation cemented) — next store step is
> ASC → TestFlight once it clears Processing. Toolchain wobbles found + fixed on the way:
> (1) `open -a Simulator` launches the STALE Xcode 15.1 copy in ~/Downloads and dyld-crashes —
> open Xcode26_6's Simulator.app by path; (2) the **iOS 26 sim runtime killed in-app
> `SKTestSession`** (needs a real XCTest run context now, not just XCTest loaded — dlopen tricks
> don't help; the 17.2 runtime was auto-deleted in the upgrade) — `Store.swift` now skips local
> IAP testing gracefully (gated to iOS 26+); **the sim purchase dev-loop lives on an 18.x
> runtime device** — iOS 18.4 runtime installed + `DEVICE="iPhone 16 (18.4)" ./build.sh`
> VERIFIED purchases working (2026-07-06). Device IAP testing still waits on ASC IAP records
> (Monetization → In-App Purchases; the bundled .storekit only covers the sim).
> (A separate lane from the one above.) A big session. Shipped, all committed to `master`
> (local — **push to sync other machines**):
> - **The free ASO keyword loop** (CLI + Apps tab): `aso-research` (now mines competitor
>   *descriptions*, doc-frequency ranked) · `aso-suggest` (free Google-autocomplete demand proxy) ·
>   `aso-compose` · `aso-lint` · **`aso-brief`** (palette — a committed, drift-tracked
>   `seo-brief.md`) · **`aso-coverage`** (mirror — coverage + stuffing) · **`aso-score`** (terminal
>   scoreboard + A/B tweak, `--deep` = winnability). Loop: research/suggest → brief → *you write* →
>   coverage → compose/lint/score; **no step writes prose**.
> - **Apps-view surface:** the sell row (📝🔎💡🧩🔬📊✅🪞) + IAP copy (char badges) + clickable
>   keyword "keys" + all-keys→research + load-into-all-tools.
> - **Strategy reframe:** [`design/demand-generation.md`](design/demand-generation.md) — capture
>   (ASO, the tail) vs generation (video/tribe, the wave); grab a **tribe**, not the world.
> - **The trailer builder** ([`design/trailer-builder.md`](design/trailer-builder.md)): backbone
>   `tools/build-app-reel.js` (manifest carts → one reel; Tiny Jam = 3-rack) **+ editor v1 (A)** —
>   the Apps-card **🎞 trailer** section, a **non-destructive** click-to-edit timeline (library, ◀▶
>   reorder, transition-at-join, Build → bake+compose → preview; edits only the `.reel`).
>
> **Resume at:** the maker-gated **store submission track** (see the 2026-07-07 update at the foot of
> this lane for the live pick-up — trim + speed already SHIPPED, engine + editor + live preview, per
> [`trailer-builder.md`](design/trailer-builder.md)). **Full snapshot + next:** the pick-up point in
> [`store-agents.md`](design/store-agents.md#pick-up-point-next-session). Orient: `node tools/topic-brief.js "aso"
> "trailer" "demand"`.
> **Editor note:** this lane changed `editor/electron/main.cjs` + `preload.cjs` (new IPCs:
> aso-score, app-clips, build-reel, app-seeds, aso-suggest/brief/coverage) — **restart Electron
> (`make`) to light them up**; `shell.js`/CSS/`index.html` hot-reload. All CLI tools work now.
> **Update 2026-07-07 — the ASC upload tool is BUILT: `tools/asc-push.js`** (the store track's
> "one big unbuilt piece", ADR-0026). In-house against the App Store Connect API, zero deps, proven
> LIVE against Tiny Jam: **keywords + app screenshots pushed**, and **all 3 IAPs created →
> localized → priced → availability → review-shot → `READY_TO_SUBMIT`**. `--metadata`/
> `--screenshots`/`--iap`/`--dry-run`/`--check`. Auth in `~/.appstoreconnect/` (`.p8` + `config.json`,
> never git; `*.p8` gitignored). **Also this session:** the IAP product ids were renamed to the
> bundle-nested scheme **`com.mipolai.tinyjam.{acidrack,epiano,masterpass}`** (was `com.tinyjam.*`;
> rebirth→acidrack) across `app.json` + `Store.swift`/`canvas.c`/`Tinyjam.storekit` + the two iOS
> tests, and the `.storekit` was resynced to the manifest (dropped a phantom "funk", fixed the master
> pass $19.99→$5.00) — **purchase flow re-verified on the iPhone 16 (18.4) sim**. **Resume at:** the
> credentials are set up (Key `Z5DTR9TFW2`); next store moves are per-locale `metadata/<locale>/`
> folders + an editor button for `asc-push`, then the maker-gated submission. Snapshot in
> [`store-agents.md` → Pick-up point](design/store-agents.md#pick-up-point-next-session).
> **Update 2026-07-08 — metadata channel is submission-complete + has an editor button.** Scaffolded
> `apps/tinyjam/metadata/en-US/` (description + promo from press.md, `support_url` →
> https://mipolai.com/tinyjam/support/ live). Built the **☁︎ App Store panel** on the Apps card
> (`asc-push.js` gained `--json`/`--only`; `studio:asc-metadata` IPC): two-click ceremony — dry-run
> diff → per-field checklist → push only ticked fields. **Promoted-purchases channel DONE too**
> (2e35fa03 + f8bd613f + 4697b11c): the ☁︎ panel now has a second section for `asc-push --promote`
> (`--json`/`--only` backend + `studio:asc-promote` IPC), each IAP a row with its promoted state,
> its own ★ Promote button. All 3 tinyjam IAPs already promoted, so it reads "✓ all promotable IAPs
> already promoted". **NEEDS ELECTRON RESTART** (`make`) — the ☁︎ panel (both sections) is verified
> at the data layer but not yet eyeballed live. **Naming stays honest:** this is *promoted purchases*
> (App Store search), NOT the editor [Promote tab](design/promote-tab.md). **Parallel-agent note:**
> the editor JS half was swept into f8bd613f (the Promote-tab agent's commit) — reconciled clean, my
> full two-section panel is in HEAD. **Resume at:** eyeball the ☁︎ panel after `make`; screenshots
> channel is the next unbuilt one (deferred by the maker until there's more screenshot tooling).

> **▶ ACTIVE THREAD (2026-07-08) — the Promote tab + the shared-popup pattern (SHIPPED).**
> (Was "editor media"; shares editor files with the trailer + leads lanes — main/preload need an
> Electron restart.) The per-cart **Promote tab** is BUILT (A–E) — a tab next to code/pixels/apps,
> resolving the media-home seam from
> [`editor-scopes-and-facets.md`](design/editor-scopes-and-facets.md#resolution-2026-07-07-make-promote-ship-promote-is-a-new-per-cart-tab)
> (Dream → Make → Promote → Ship; Make/Ship already existed, Promote was the homeless verb).
> **Sections:** **A** clips & takes — list the cart's takes, click one to WATCH it (`.rec`/`.script`
> native via `studio:replay`, `.beats` via `studio:play-beats`), ● record a take, 🎬 **bake** a take →
> clip (`studio:bake-clip` → `make-gif --recipe`), auto-refresh on record; **B** stills — 📸
> `studio:cart-shot` → `editor/public/shots/<cart>/NN-snap.png` (a NEW sibling of `clips/`); **C**
> trailer; **D** find tribes (cart-scoped `studio:cart-leads`); **E** gallery link. The old toolbar
> `● rec` button + `showRecord` setting are **RETIRED** — recording is Promote-only.
> **The shared-popup pattern (the maker's idea):** a scope-neutral tool gets ONE popup opened from
> BOTH the Promote tab (cart) and the Apps card (app) — `open({kind,name})`, caller supplies scope,
> popup is scope-blind. Two shipped instances: **trailer builder** (lifted out of the Apps panel into
> a top-level `.modal`; `openTrailer({kind,name})` — cart stitches one cart's clips, app stitches
> across an app's) and **keyword research** (`openKeywords` — `aso-research`+`aso-suggest` seeded from
> de:meta/listing). Browse-y glances stay INLINE (`leadsHtml`/`researchHtml`/`suggestHtml` extracted so
> popup + inline render identically).
> **Reels save/load (the .reel *scenario*, not the heavy webm):** a **saved-reels strip** in the
> builder scoped to the subject (`<subject>.reel` + `<subject>--<variant>.reel`) + a **cross-subject
> Reels overview** at the top of the Apps page; click any reel → loads its scenario
> (`studio:list-reels`/`reel-load`; shared `parseReelFile`/`reelClipsFor` helpers). `tlSubject` (list
> scope) vs `tlApp` (build target) are now split → ready for named variants.
> **Multi-resolution export SHIPPED (export-ratios.md, both stages, resizable carts):** an **output-ratio
> picker** on reel-Build (`# size` → compose renders at that canvas) AND a **"bake at" ratio picker** on
> the Promote 🎬 bake. **Stage 2** = per-ratio clip **variants** (`<label>--<W>x<H>.webm`) that compose
> PREFERS at a matching size → the reel **FILLS** (else letterbox). Presets: social (16:9/9:16/1:1) +
> App Store **EVEN half-sizes** (444×960/960×444/600×800, ×2 on delivery — odd widths break ffmpeg pad;
> `# scale 1` so output = the small canvas). Enabler = **keyboard shortcuts** (position-free take → any
> ratio). **`onetake` cart** (committed) is the worked proof: keyboard-driven, %-positioned, resizable.
> **Docs:** [`promote-tab.md`](design/promote-tab.md) (A–E shipped) · [`export-ratios.md`](design/export-ratios.md)
> (BUILDING — stages 1+2 done) ↔ [`resolution-portable-input.md`](design/resolution-portable-input.md)
> (the input half) + a filmability note in [`cart-authoring.md`](guides/cart-authoring.md).
> **Resume at (open builds):** (1) **an EYEBALL PASS** — a big UI stack (Promote tab, popups, reels,
> ratio pickers, Stage 2) verified only at pipeline/logic level, **none clicked live**: restart Electron,
> run record→bake→trailer→reel-with-a-text-card→Build-at-a-ratio; flag breakage (likely spots: the ≥2-clips
> rule, an unbaked clip, or new-IPC wiring). (2) **fixed-layout composite** — the export-ratios gap: a
> non-resizable cart can't reflow, so it needs a dressed bg+caption letterbox (video `store-shots`).
> (3) **named reel variants** (💾 save-as; `tlSubject`/`tlApp` split ready) · delivery-exact upscale ·
> per-cart trailer pre-pop from its reel · delete affordances · published-state dot on E.
> Hot files: `editor/src/shell.js`(+`shell.css`), `editor/index.html`, `editor/electron/main.cjs`+`preload.cjs`,
> `tools/compose-clips.js` (variant preference), `tools/make-gif.js` (`--screen`).

> **▶ ACTIVE THREAD (2026-07-07) — leads: the local marketeer (demand generation).** A new
> tool that answers "where do I post about this cart?" — the generation twin of the `aso-*`
> capture tools. **What shipped (committed):** `tools/leads.js` (7 commands: `match` cart→tribe→
> venues · `discover` venue-hunt links + Google-autocomplete signals · `draft` a gift-first post
> scaffold from the cart's own words · `track` outreach log · `audit` whole-catalogue coverage,
> free/local · `list` · `--check`) + `tools/leads-ledger.json` (committed, hand-editable; **18
> tribes**/9 cross-cutting, seeded from [tinyjam-marketing](marketing/tinyjam/tinyjam-marketing.md) §3.9). The model is **buckets**: a tribe =
> tags + venues + hook; carts auto-match on `de:meta`; a `domain` (music/game/any) pre-filter keeps
> games off music-press venues. Reddit's free API is dead (403) — discover uses free Google
> autocomplete + search-url launchers.
> **Update 2026-07-07 — the MUSIC TAXONOMY is substantially filled: 18 → 32 tribes, music coverage
> 62% → 90%.** The big idea (from a maker insight): **the tribe is the SCENE, not the FORMAT.** The
> genre-radio carts each homage a specific artist (jingle=Mac DeMarco, eno=Brian Eno, afrobeat=Fela,
> house=Daft Punk) → scene tribes keyed on the identity word, with `generative` (r/generative·lines·
> Disquiet) as a cross-domain FORMAT amplifier. Added: ambient/citypop/afrobeat/frenchhouse/
> indie-jangle/microtonal/generative, `piano`+`vintage-poly` (homed the piano/juno misfiles),
> physical-modeling/guitar/world-folk (acoustic cluster), novelty-toy/vocal-synth. Fixed the `moog`
> subtractive over-match (31→14) and the `drone`/`vowel`/`vocoder` generic-adjective noise (tag on
> identity, never technique). Model refinement written into `leads-marketeer.md`.
> **PARKED (maker's call):** the 4 weak-room scenes (satie/bossa/mariachi/tango — stay on the
> generative amplifier) + games buckets (GTM: web-gallery-only; `arcade` is the one game tribe).
> **Update 2026-07-07 — the editor Apps-page surface is BUILT v1:** the Apps card gained a **reach**
> section + **📣 find tribes** button (mirrors the ASO 📊 glance). Per cart of the app it renders
> tribes + matched tags + hook + venues (clickable → browser) + a copyable **post scaffold**, plus
> cross-cutting once — "we prep, you post." New `leads.js match --json` + `studio:leads` IPC
> (app-scoped, loops the app's carts). **NEEDS AN ELECTRON RESTART** (`make`) — main.cjs/preload.cjs
> changed. Verified via CLI (tinyjam 3-cart aggregation), not yet eyeballed in the running UI.
> **Resume-at: [`leads-marketeer.md` → Open questions / resume-at](design/leads-marketeer.md#open-questions-resume-at)** (item #4) — (a)
> maker eyeballs 📣 on the tinyjam card after `make`; (b) v2 = the free-form per-cart **discover box**
> (autocomplete venue hunt) + a cart-scoped entry point (v1 is app-scoped like the ASO tools). Hot
> files: `editor/src/shell.js` (+shell.css), `editor/electron/main.cjs`/`preload.cjs`, `tools/leads.js`,
> `tools/leads-ledger.json` (hand-edit venues). Gate: `node tools/leads.js --check`.

## History & reference (pruned 2026-07-05)

The old session narratives, shipped-feature bullets, and todo list that lived here are ledgered
elsewhere — trust those homes, not a handoff file:

- **Shipped / open / cut** → [`STATUS.md`](STATUS.md) + the design board (`design-board.html`).
  Backlog → the board's READY-TO-BUILD column (`node tools/design-board.js`); the old todo items
  all live on it or in [`design/api-notes.md`](design/api-notes.md).
- **Web build deep reference** (web-specific behaviour, emcc flags + why, how `runtime/raylib-web/`
  was built from source) → moved to [`guides/exporting.md`](guides/exporting.md) §5.
- **Cart format** (`.cart.png` zTXt chunks) → [`design/cart-metadata.md`](design/cart-metadata.md) +
  [`guides/cart-authoring.md`](guides/cart-authoring.md). Editor internals worth remembering: chunk
  helpers are `embedCartChunks`/`extractCartChunks`/`makeZtxtChunk`/`crc32` in
  `editor/electron/main.cjs`, duplicated standalone in `tools/make-cart.js`; the
  `preload.cjs` IPC surface is `studio.saveCart/loadCart/loadCartFile/loadCartBuffer/getFilePath`
  (Electron 32+ for the last); dropping a `.png` on the window loads it as a cart; `--screenshot`
  on a cart binary renders 3 frames and exits.
- **Cart authoring quick reference** → [`guides/cart-authoring.md`](guides/cart-authoring.md)
  (and CLAUDE.md's "Adding a cart" steps).
- Debug-tools design notes archive → [`archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).

---

## Gotchas / environment facts

- **Display asleep = every play.js/make-cart run SEGFAULTS in `rlglInit`** (signal 11, empty
  trace) — Raylib needs a live GL context even `--headless`, so late-night unattended renders
  suddenly "break the engine" when the screen locks. It's not your edit. Fix:
  `caffeinate -du node tools/play.js …` (wakes + holds the display). Discovered mid
  filter-spike 2026-07-02 (audio-notes §25), took out a parallel agent's runs too.

- **`main.cjs` / `preload.cjs` changes need an Electron restart** (`npm start`);
  Vite hot-reloads everything else.
- **`▶ run` only works in Electron** (it spawns clang); the browser tab edits but can't run.
- Use **Node 22** (`nvm use 22`) before any npm command.
- **`--screenshot` mode** opens a real window briefly (Raylib needs a display).
  3 frames is enough for static carts; carts that randomise initial state on frame 1
  will look fine; carts that need user input obviously won't show gameplay.
- **arm64 integer divide-by-zero does NOT trap** (returns 0) — SIGFPE won't fire on
  Apple Silicon. Use a `volatile` null read for reliable test crashes.
- **`save()`/`load()`** write `cart.sav`/`cart.kv`/`cart.blob` into a per-cart folder:
  the editor and `play.js` pass `--save-dir saves/<cart>`, so saves live under
  `build/saves/<cart>/` (2026-06-04; previously all carts shared `build/cart.sav`).
- **`print_centered`/`print_right`** use `strlen(text) * 8` for width — the dos_8x8
  font is exactly 8px per character with 0 spacing (`DrawTextEx` size=8, spacing=0).
- **`rnd_float()`** uses `rand()` from `<stdlib.h>` (added to studio.c includes).
  All carts share the same `rand()` seed — not seeded per-cart; same sequence every run.
- **CLAUDE_CODE_TMPDIR fills up occasionally** — compile/emcc output gets lost when the
  tmp partition fills. Workaround: redirect to a real file and read it back —
  `> build/compile-test.log` for clang, `emcc ... >/tmp/emcc.log 2>&1` for web builds.
- **`trifill()` winding order** — Raylib's `DrawTriangle` needs counter-clockwise winding
  in Y-down screen coords. In Y-down space, cross product > 0 means clockwise visually
  (opposite of math convention), so swap when `cross > 0`.
- **`rrectfill()`/`rrect()` 1px-narrow-on-right-and-bottom bug — FIXED 2026-07-17.** Root cause was
  a stray `-1` in `rrect_inside()` (`runtime/studio.c`): the right/bottom corner-centres were
  `x+w-1-r`/`y+h-1-r` while left/top were `x+r`/`y+r` — asymmetric, so with pixel-centre sampling the
  straight edges landed at `x+w-2`/`y+h-2` instead of `x+w-1`/`y+h-1`. Changed to `x+w-r`/`y+h-r`
  (inscribe in `[x,x+w]×[y,y+h]`), so `rrect`/`rrectfill` straight edges now coincide exactly with
  `rect`/`rectfill` at identical `w,h`. Verified: a probe reads matching right/bottom edges;
  `canvas-diff` PASS (GPU==SW) on `raster_test` + `acidcandy`. **MIGRATION:** any cart that
  compensated with the old `w-1` trick now renders 1px SHORT — remove it. Done: `acidcandy` voice-band
  (line 440, `153`→`154`). If you find another `w-1`-to-align-with-rrectfill comment, undo it.
- **`init()` fires after window + sprites are fully loaded** — safe to call `colorkey()`,
  `mset()`, etc. It does NOT run during `--screenshot` mode's 3-frame early exit, but
  that's fine since screenshot mode still calls it once before the loop.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.
- **Web build server stays alive** — `startWebServer()` in `main.cjs` keeps one
  Node HTTP server on port 8765 alive for the editor session. It reuses the same
  instance on subsequent builds (doesn't restart). If something is already on 8765
  externally, kill it: `kill $(lsof -ti:8765)`. emcc progress goes to the runtime log
  panel; errors to the build log panel.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
