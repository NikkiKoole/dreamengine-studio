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

_Last updated: 2026-07-30._ **The lane list is NOT maintained here — run `node tools/handoff.js`.**
It prints every lane with its line number and age, read straight from this file, so it cannot drift.

---

## Where we are right now

**Active lanes, newest first.** `node tools/handoff.js` is the index (line · title · age);
`node tools/handoff.js --check` is the back door (>2wk old · a missing or unanchored Resume-at ·
a broken doc link or `#section`).

> Two things lived here until 2026-07-30 and were deleted because both had rotted: a prose
> re-summary of every lane (9.5 KB on a single line) and a hand-numbered index. The index said
> "sixteen lanes" when there were nineteen, and omitted three — including the newest and topmost.
> What a reader needs to *choose* a lane is in the front-door output; what they need to *resume*
> one is in the lane itself. A summary in between is a third copy, and it is the copy nobody
> updates. If you find yourself writing one again, teach `handoff.js` to print it instead.
> **▶ ACTIVE THREAD (2026-07-30) — `pedalboard`: the guitar rig, and an APP IN REVIEW.**
> **This lane did not exist until 2026-07-30, and it should have.** A handoff audit found `pedalboard` was
> the single most active thread in the repo — 18 commits since 07-28 (fret wires warmed into the board, the
> mute check tracking the hand, TRAVIS picking as a second autoplay style, autoplay keeping YOUR chord
> shape, boot on G major, `mouse_wheel_x()`) — while appearing in this file exactly once, as one of 23
> carts that use `INSTR_GUITAR`. A cold agent would not have known the effort existed.
> It is also a PRODUCT: `apps/pedalboard` has its icon, screenshots, listing and review contact pushed,
> and is **in review** (see `STATUS.md`). The engine seam it rides is the `input_monitor(gain)` pedal tier
> that shipped 07-22, so real GUITAR IN → amp → pedals works on desktop.
> **Resume-at:** the cart's own punch list — `node tools/cart-todos.js pedalboard` — plus
> [`design/effects-bus-architecture.md` → Increment E, the output stage](design/effects-bus-architecture.md#increment-e--the-output-stage-4th-zone-cabinets--ampcab--leslie)
> for the amp/cabinet model
> (`runtime/ampcab.h` is the shared voicing table; `fxicons.h` is the shared pedal LOOK).
> Hot files: `tools/carts/pedalboard.c`, `runtime/ampcab.h`, `apps/pedalboard/app.json`.

> **▶ ACTIVE THREAD (2026-07-31) — Synth Secrets: the audit is COMPLETE, the build plan is running (Phase 0 done, **PHASE 1 COMPLETE 7/7**, **PHASE 2: 2.1, 2.2 and 2.3(a) SHIPPED — PIANO now has real stiff-string inharmonicity + a completed Railsback curve; 2.3(b) DROPPED on measurement; 2.4's bowed body now SHARED PER SLOT with a size axis, and defaulting it on is the live item**).**
> The owner supplied Gordon Reid's **Synth Secrets** (Sound On Sound, 63 parts, 1999-2004) and asked for a
> cross-check against `runtime/sound.h`. **All 63 articles are now read**: an architecture pass plus eight
> per-family recipe passes, ~106 sub-findings, every one citing both sides (part + issue on the book side,
> `file:line` on ours). Findings live in [`design/synth-secrets-audit.md`](design/synth-secrets-audit.md)
> §A-§M; **work happens in [`design/synth-secrets-plan.md`](design/synth-secrets-plan.md)**, which is the
> ordered ledger. Do not add work items to the audit.
>
> **The full record is in the plan, not here.** ~400 lines of settled narrative lived in this lane until
> 2026-07-30 and were deleted after a block-by-block check confirmed every one is already in
> [`synth-secrets-plan.md`](design/synth-secrets-plan.md), usually with more tables and more numbers.
> Phase 0 + Phase 1 (7/7), items 2.1 / 2.2 / 2.3(a), the 2.3(b) DROP, the two PIANO defects, the four
> oracles and every measurement table: all there, and ledgered in [`STATUS.md`](STATUS.md). This lane
> now holds only what you need to *resume*.
>
> **Resume-at — the live queue, most-ready first.**
>
> 1. **§M2 / item 2.4 — `BOWED` HAS A BODY, AND IT IS NOW ONE SHARED BOX PER SLOT WITH A SIZE (2026-07-31).**
>    `MODE_BOW_BODY` ships: three parallel feedback combs at 1.3/2.3/3.7 ms (Reid Part 22), opt-in,
>    **default OFF**. Owner's ear: the body WINS (audit §F4's gap is real), and the combs were kept over a
>    bandpass-formant variant on §M2's own argument after the ear could not separate them.
>    **The sizing blocker is GONE.** The owner picked "one body per slot" from the three routes, and it
>    shipped: a pooled `bow_bodies[8]` claimed per slot (like `fx_bus_for`), plus **`MODE_BOW_SIZE`** on
>    `eng_p[2]` — whose `0.5f` bank default maps to exactly 1.0x, the violin box, so no existing cart moved.
>    1.0 = the cello set (3.19/5.65/9.09 ms). Default path verified **byte-identical** (`3de65baf5bd8`).
>    ⚠ **If you touch this: the box is clocked ONCE PER SAMPLE in `bow_body_advance()`, never from the voice
>    loop.** A comb's resonances come from its clock rate, so advancing it per sounding voice would make the
>    body's pitch track the chord size (a triad ringing ~a twelfth high) — silent, and no gate calls it an
>    error. `wet_share` is likewise divided by the reader count so the box radiates once, not N times.
>    **NEXT HERE: defaulting it ON is now an EAR call, not a blocked one.** A cart can declare its
>    instrument's size, so the remaining question is just whether the owner likes the body across the 14
>    BOWED carts, several bass-focused (`walkbox`, `walkroll`, `upright`, `bandbox`). Play `bowed` (baked):
>    presets 1/2/3 are violin/viola/cello boxes, B toggles the body.
>    **Then `guitar`** — still a measurement cross-check ONLY, see the scoping below.
>    Two traps banked: a 1–4 ms body has a MILLISECOND RT60 so there is no audible tail to A/B on (the
>    "reverb" IS the frequency response — Reid's duality); and blend a body ADDITIVELY, never as a crossfade,
>    or you discard the string's own ringdown (a crossfade at 0.8 made a pizzicato note die twice as fast).
>    Ear sets: `build/ab/bowed-body-*.wav` (arco) and `build/ab/bowed-pizz-*.wav`.
> 1b. **`guitar` — the remaining half of §M2, and a measurement cross-check ONLY.**
>    Compare three short delay lines against the four `gt_body` biquads head to head, take the numbers, and
>    **do NOT flip its default.** `GUITAR` is in **23 carts** — essentially the whole amp/pedal shelf
>    (`combo`, `pedalboard`, `tubescreamer`, `mistress`, `springtank`, `wba`, `mixbooth`, `afrobeat`,
>    `thexx`, `mariachi`, `portapop`) — **all voiced by ear against the existing biquad body**, so replacing
>    what they were voiced against is not a neutral act and no oracle would flag the damage. If the delay
>    body also wins there, that is a separate conversation with 23 carts in it.
>    Also mind that §I3's piano tricord already has a weak 0.2% output→string-1 tap, so do not A/B that
>    against an assumed zero.
>    **⚠ `inharm-spec` is the WRONG oracle here** — a body changes partial LEVELS (a frequency response),
>    not partial FREQUENCIES. Use `harmonic-spec` + centroid/brightness from `wav-envelope`. And `BOWED`
>    self-oscillates, so there is no decay to measure: it is a steady-state spectrum, and the `effLen`
>    sustain trap does not apply as it did to PIANO.
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
>
> **Orienting cold:** `node tools/orient.js`, then plan §1 (the four gate kinds, the A/B protocol, and the
> owner's standing constraints), §2 (the add-vs-change ladder), §4 (Phase 1, all seven write-ups), and
> §5 (Phase 2, where the live work is). The audit is reference only — never add work items to it, only
> ✅ verdict banners pointing back at the plan.
> **Two process traps already hit:** `ui-audit` passes **low-contrast** text (it only catches off-screen
> and overlapping), so read the baked PNG; and `--run` bakes only the thumbnail, so **re-embed after every
> source edit** or the pre-commit hook will (correctly) reject a stale `.cart.png`.
>
> **Hot files** (several agents share this tree): `runtime/sound.h` — targeted `Edit`s only, never a
> full-file `Write`, and confirm your change survived the commit (`git show HEAD:runtime/sound.h | grep`).
> Also `runtime/studio.h`, `editor/src/studioDocs.js`, `editor/src/shell.js` (the four-place API
> registration), `tools/tune-check.js` + `tools/carts/tunecheck.c`, and `tools/carts/piano.c`/`bowed.c`.
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

> **▶ ACTIVE THREAD (2026-07-21) — `bandbox`: the chord-chart SEQUENCER — WIRED (spec 131 green); open = `band.h` extraction + richer per-voice editors.**
> **Resume-at:** [`design/bandbox.md` → Build plan](design/bandbox.md#build-plan-ordered) — steps 1–6 are
> done and the cart plays; what is left is the `band.h` extraction and richer per-voice editors.
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
> **acidcandy ROOMY — SHIPPED 2026-07-23, and this block used to say the opposite.** It described a
> COEXISTENCE plan ("app is under App Store review — do NOT delete the old"), a `rack_view==1` path, a NEW
> button, and an INERT soft-key row. All of that is gone: commit `a16b2527` **removed** the old 2×2
> `draw_rack` and promoted ROOMY to THE tablet view (`acidcandy.c:106` says so; `rack_view` is now `0` =
> phone / `2` = ROOMY, auto-selected from `device_class()` at `:3396`, and most of the soft-key row is
> wired — `r2_dpaint`/`r2_303panel`/`r2_drumpanel`/`r2_mstpanel`). **A reader following the old text would
> have gone to protect code that no longer exists**, which is why it is called out rather than quietly
> deleted. Current state and what is genuinely left:
> [`design/acidcandy-ipad-layout.md`](design/acidcandy-ipad-layout.md) (STATUS: shipped — its checklist is
> historical).
> **Resume-at:** [`design/responsive-first-device-face.md`](design/responsive-first-device-face.md#the-layers--cheapest-to-deepest)
> — Layers 1–3 shipped; **Layer 4 (make `face.h` the default) is the open one.** The acidcandy ROOMY proof
> above is done, so that branch of the NEXT above is closed.

> **⏸ PARKED (last touched 2026-07-18) — `walkbox`, a walking-bass step-sequencer.** Core + articulation
> play; what is left (ghost notes, hammer-on/pull-off, presets) is a wish list, not a blocker. Ledgered in
> [`STATUS.md`](STATUS.md). Resume at [`design/walkbox.md`](design/walkbox.md).

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
> **Resume-at: [`design/audio-input-frontier.md` → the frontier, ranked](design/audio-input-frontier.md#what-it-opens-next--the-frontier-ranked-by-juice-per-effort).** Auto-tune
> arc is COMPLETE for the offline feature; the LIVE path is feasible-and-parked (warble). Open frontier, ranked:
> (1) the **live looper** — the *pedal tier* half of this SHIPPED 2026-07-22 (`input_monitor(gain)`, and
>     `pedalboard` became its own app, in review); the looper is the part still open;
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

> **⏸ PARKED (last field note 2026-07-18) — demand discovery, the reddit-gaps drip.** The 6 h LaunchAgent
> keeps mining and the caches keep growing (24 tribes, 1,411 wishes clustered), so this is a STANDING
> PROCESS rather than in-flight work; the tombola toy it produced shipped 2026-07-14. Resume — including
> where the findings live and how to drip the next tribe — at
> [`design/demand-discovery.md`](design/demand-discovery.md#where-the-findings-live-and-grow).

> **⏸ PARKED (last touched 2026-07-07) — responsive instrument UI.** The playbook, ADR-0028, the epianofit
> mock and the `scalegrid` cart (device-tested, spec 71/0) all shipped. The single open step is extracting
> the grid into a `grid.h` library — **`runtime/grid.h` does not exist** and nothing has moved since, so
> this is dormant, not nearly-done. Resume at [`design/scale-grid.md`](design/scale-grid.md).

> **⏸ FOLDED INTO THE `face.h` LANE (2026-07-30) — device-adaptive layout / the acidrack redesign.**
> Phases 0–2 shipped (`runtime/lay.h`, the resizable canvas, iOS fill/safe-area/rotation), R2
> (`runtime/disclose.h`) and R3 (`finger_px()`/`device_class()`) shipped and verified on device, and the
> `acidwire` wireframe did its job (field note 020). **But `acidrack.c` has not moved since 2026-07-14 and
> `disclose.h` since 07-10, while the tablet answer actually shipped through `face.h` + ROOMY in acidcandy
> — so this and the faces lane were two lanes describing one thread.** R5 (port acidrack onto `disclose.h`
> + `finger_px()`) is still open and now rides the faces lane above. Scoreboard:
> [`device-adaptive-layout.md` → Where this stands](design/device-adaptive-layout.md#where-this-stands).

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
> launch) — recorded as `"price"` in `app.json`, BUT the base price is settable via `node tools/asc-push.js <app> --price` (was a manual ASC step) (Pricing &
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

> **⏸ PARKED (tool unchanged since 2026-07-07) — `leads.js`, the local marketeer.** Built and ledgered in
> [`STATUS.md`](STATUS.md); the taxonomy has grown through USE (34 tribes today) but the tool itself has
> not changed. Open: the editor Apps-page surface. Resume at
> [`design/leads-marketeer.md`](design/leads-marketeer.md#open-questions-resume-at).

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
  **the harnesses now do this themselves** — `play.js`/`spec.js`/`make-cart.js` each fire `caffeinate -u -t 1` before the `-dims` wrapper, so you only need it by hand when driving a cart binary directly. Full note in [`guides/debug-harness.md`](guides/debug-harness.md#gotcha-a-sleeping-display-segfaults-every-harness-run). Discovered mid
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
