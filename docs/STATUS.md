# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-06-11 (**tape (wow/flutter/saturation) SHIPPED — the third use of the modulated-delay technique; the "wow/flutter buffer" 0015 reserved finally has its namesake.** `tape(wow, flutter, saturation)` + `instrument_tape(slot,…)` — a per-bus INSERT (stereo, chained after chorus→flanger on the 8-bus pool): warm normalized-tanh **saturation** + a slow **WOW** + fast **FLUTTER** pitch warble (a modulated-delay read, ONE shared transport LFO so both channels drift together) + a baked **HF rolloff** (darker as you saturate). Plain-sine LFOs → fully deterministic (`--det`-safe); navkit's noise-LFO + hiss skipped. Reuses the shared `moddel_hermite` read (3rd instance after chorus + flanger). Master + per-instrument like the others. VERIFIED: soundcheck + juno bytes-identical to baselines (tape dormant → bit-exact); tripwire clean; functional — a sustained sine through heavy wow drifts pitch periodically (zero-cross rate cycles 864↔896 Hz on a 2.0s period = the 0.5 Hz wow rate) and saturation lifts RMS −18.8→−10.4 dBFS with crest falling. Showcase: **tapeloop** (Frippertronics — a pad → long echo loop → tape degrades each pass). The mod-delay technique now has three uses (chorus/flanger/tape); the §8.10 effects roster is essentially complete (echo/reverb/chorus/flanger/tape, all per-instrument). Full 4-place wiring + tcc.) Prior: 2026-06-11 (**per-instrument chorus/flanger SHIPPED — the §8.10 aux-routing step (effects on ONE instrument, not the whole mix).** `instrument_chorus(slot,…)` / `instrument_flanger(slot,…)` — matching the per-instrument shape of `instrument_echo`/`instrument_reverb` — so you can "flange the guitar, not the rhythm." The master FX section became an **8-bus pool** (`SOUND_FX_BUSES`: bus 0 = master + 7 aux); each bus owns its own chorus + flanger insert state. Voices accumulate into `busL/busR[v->bus]`; aux buses run their chorus→flanger chain and return to master; the master section (sends + bus-0 inserts + soft-clip) is unchanged after the sum. Each slot grabs a **private aux bus** on first per-instrument call (`instr_bank.fx_bus` + a free-bus cursor; chorus+flanger on one slot share its bus and chain); pool exhausted (>7 inserted slots) → that slot stays dry (never mis-routes to master). `chorus()`/`flanger()` stay the master (bus 0) inserts. **Echo/reverb deliberately stay ONE shared tank each** (sends, already per-instrument) — per-bus reverb/echo would mean 8 separate rooms (~2.7 MB) and break the per-slot send API; only the *inserts* go per-bus. Reuses the shared `moddel_hermite` read per bus. VERIFIED: soundcheck + juno bytes-identical to pre-change baselines (the all-bus-0 path is bit-exact through the hot-path refactor — the dormancy/old-cart guarantee holds); tripwire clean; isolation A/B — a played note reads FLAT with the flanger on another slot, sweeps with it on its own slot. Showcase: **mistress** now flanges its lead while a bass stays dry. Real customer: **air.c** (`instrument_chorus(I_PAD,…)` — Solina pad wet, drums/bass dry), now unblocked. Remaining generalization: explicit named buses to route *several* instruments through one shared insert. Full 4-place wiring + tcc.) Prior: 2026-06-10 (**flanger SHIPPED — the third §8.10 effect, and [decision 0015](decisions/0015-effects-are-recipes-not-primitives.md)'s "one true gap" closed.** 0015 deferred flanger until the master modulated-delay buffer existed ("falls out of it free" once chorus/tape land); chorus landed that technique, so flanger is now a **second instance** of it — its own short buffer (512/~11ms) + the *shared* Hermite read (`cho_hermite` generalized to `moddel_hermite(buf, len, pos)` so chorus + flanger share one read — the literal "write the technique once" discipline; chorus output verified bit-identical after the refactor). `flanger(rate, depth, feedback, mix)`: a short (0.1–10ms) triangle-LFO-swept delay with **feedback** (the flanger's identity — subtle → jet whoosh → metallic resonance; negative = through-zero), navkit `processFlanger` port, mono (the classic flange), a **MASTER INSERT** after chorus in the master FX section. Master-wide, no `instrument_flanger` (per-part waits for aux routing — 0015-faithful); four params (one more than chorus — feedback can't be folded away). Dormant-until-called (`flanger_used`) → byte-identical for old carts: VERIFIED (soundcheck bytes-identical + tripwire clean; juno also bytes-identical — the read refactor doesn't touch chorus). Functional: a dry saw is spectrally flat, a flanged saw's amplitude swings 0.26↔0.94 and brightness 0.004↔0.085 periodically (the comb sweeping the harmonics). Showcase: the **mistress** cart (EHX Electric Mistress — strum a guitar through it; the JET button = a noise swell = the jet overhead). The modulated-delay technique now has two uses (chorus, flanger); **tape** is the natural next (same technique, slow wow/flutter LFO). Full 4-place wiring + tcc; registered in the cart shelf + recipe palette.) Prior: 2026-06-10 (**chorus SHIPPED — the second §8.10 effect, and the first use of the shared modulated-delay buffer.** Per [decision 0015](decisions/0015-effects-are-recipes-not-primitives.md), the real modulated-delay chorus is NOT a third send bus — it's the first use of the ONE master "wow/flutter buffer" 0015 reserves (where flanger + tape-wow will also live). So `chorus(rate, depth, mix)` landed as a **MASTER-STAGE INSERT** in the master FX section's insert chain (after the echo/reverb send-returns, before the soft-clip), the two-send-bus cap intact — no doctrine change, the buffer just landed early (via chorus, not tape). DSP = a line-for-line port of navkit's BBD chorus (the Juno-6 model): one rounded-triangle LFO, **antiphase taps** (tap+mod→L, tap−mod→R) so a centered mono mix fans out to a wide stereo chorus — the Juno's "two amps" — Hermite-interpolated fractional reads + charge-well saturation for analog warmth. One global call, master-wide (no `instrument_chorus` — per-part waits for aux routing, honoring 0015's "no instrument_chorus, ever"); simpler than reverb (no per-voice/per-slot state). Dormant-until-called (`chorus_used`) → byte-identical for old carts: VERIFIED — soundcheck WAV bytes-identical to the pre-change baseline + tripwire clean; functional A/B = a centered mono triad reads L−R rms 0.00000 dry vs 0.123 with chorus on (the antiphase stereo width appears exactly as designed). Full 4-place wiring + tcc symbols. Showcase: the **juno** cart (Roland Juno-6 — the OFF/I/II chorus switch is the whole point; dry it's "sh101 with more voices," on it's unmistakably a Juno). Flanger + tape are now queued uses of the same buffer. Docs: [`design/instrument-engines.md`](design/instrument-engines.md) §8.10 + the 0015 correction.) Prior: 2026-06-10 (**reverb SHIPPED — the first §8.10 effects-bus effect, and the per-sample MASTER FX SECTION formalized.** `reverb(size, damping)` + `instrument_reverb`/`note_reverb`: a shared SEND bus modelled exactly like the echo bus (one processor, per-slot sends), a line-for-line port of navkit's Schroeder reverberator (4 parallel comb + 2 series allpass + 20ms pre-delay, per-comb damping LP), mono in v1. The first bite deliberately did NOT build `bus[NBUS]` + per-slot aux routing — instead it turned the per-sample master section in `sound_callback` into an explicit, ordered, send/insert-aware block: SEND RETURNS (echo, reverb) → INSERT CHAIN (soft-clip limiter last; tape/comp/bitcrush slot in before it later), with a documented-not-built side-chain seam for the future vocoder/sidechain-comp. Reasoning: the future roster is mostly *inserts* (~10) vs *sends* (~3), so the section must take both; per-slot AUX routing stays the deferred next increment (early effects all want master placement). Dormant-until-called (`reverb_used`) → byte-identical for old carts: VERIFIED — soundcheck WAV bytes-identical to the pre-change baseline + sound tripwire clean; functional A/B = a 40ms pluck blooms into a ~1.7s tail that darkens as it decays (damping working). Full 4-place wiring + tcc symbols. Showcase: the **cathedral** cart (a church organ blooming into a vaulted stone hall — reverb as the instrument, the way spacecho is for echo). Placement sanity-check per effect + the send-vs-insert reasoning: [`design/sound-next-steps.md`](design/sound-next-steps.md); design: [`design/instrument-engines.md`](design/instrument-engines.md) §8.10.) Prior: 2026-06-10 (**`INSTR_BRASS` (29) SHIPPED — the lip-reed brass family (trumpet/cornet/flugelhorn/trombone/french horn/tuba), and the LAST engine-blocked instrument in the whole library — every modeled timbre now exists.** STEP-0 was the story again: the §8.9 catalog's literal "lip mass-spring biquad" was tried two ways (an in-loop resonator, then the STK normalized-bandpass + squared valve) and **both decayed to silence after the attack** — measured loop gain ≈0.92 < 1 (the normalized bandpass's broadband gain `(1−r)`≈0.001 starves the lip displacement). The build that reliably self-oscillates: **reuse REED's proven pressure-controlled valve** (the negative-resistance `offset+slope·pdiff` reflection) retuned for loop gain >1 (`endRefl −0.96`, `slope −0.70`; the `tanh` brassiness shaper bounds amplitude even at drive 1), then wear a brass skin — a **dynamics-swept brass-formant bandpass** (centre rising with brassiness = the blatty "blaaat", the one thing no other engine does) + the pressure-driven `tanh` steepening (blow harder → blattier). harmonics = instrument/bore (trumpet→tuba), timbre = brassiness, morph = breath/lip lean-in; bore reuses `ks_buf`. Verified headless: self-oscillates and HOLDS, RMS ≈−21 dBFS (matches reed), crest ~7 dB (steady tone), DC≈0, **0 clipped** at the loud extreme (tuba + brassiness 1.0, peak −12.9 dBFS). Soundcheck slot 22 (tripwire PASS), full 4-place wiring (an engine adds zero functions). Showcase: the **brass** cart, marquee = the draggable trombone SLIDE (live `note_pitch` glissando — pitch tracking made it free), six hardware presets, mono+slide default. Mute deferred (only three macros — bore/brassiness/breath earned them). The lesson mirrors the wah detour + the bowed/Rhodes "trust ears+renders over the navkit/STK label" scar. Design + STEP-0: [`design/instrument-engines.md`](design/instrument-engines.md) §8.8.10.) Prior: 2026-06-09 (**`INSTR_BOWED` (28) SHIPPED — the bowed string, the LAST modeled string engine; the family pluck→guitar→piano→bowed is now complete.** A line-for-line port of navkit's `processBowedOscillator`/`initBowed` Smith/McIntyre stick-slip waveguide: the two nut+bridge delay lines PACKED into one `ks_buf`, self-oscillating and HELD like reed/pipe. **STEP-0 came first and was the whole story** — `tools/navkit-bowsweep.c` swept pressure × velocity × position and proved the original audition's reject (erratic envelope, crest 12.6) was **preset 107's operating point** — corr 0.36, genuine surface sound: one bad preset bowing *outside the Schelleng wedge*, not a bad engine (the Rhodes/Wurli trap again — trust ears+renders over navkit's labels). A large Helmholtz wedge exists (100–150 of 600 cells lock corr→1.0 at crest 1.6–1.8) and holds G2→A4, so **NO hysteresis bow table was needed**. Macros pinned inside the wedge: harmonics = bow position β, timbre = bow pressure (the narrow axis; clean leaning-sawtooth → deliberate scratch, corr 0.96→0.35 confirmed by render), morph = bow speed/swell + the reed/pipe realism layer (humanized vibrato, rosin bow-noise, attack bite). **PIZZICATO is the SAME waveguide, not a guitar preset** — a slot flagged `eng_tune(slot,0,1)` seeds the string with a pluck and bypasses the friction per-sample, so the identical string+body rings down; arco and pizz differ *only* in excitation, exactly as on a real violin. Showcase: the **bowed** cart — played by RUBBING (energy accumulates, so continuous rubbing builds and digs into grit; stop and the bow rests silent), a quick TAP plucks it (pizz). Soundcheck slot 23 (tripwire PASS), full 4-place wiring + the STEP-0 sweep tool committed. `eng_tune` weight/attack/pizz-flag stay EXPERIMENTAL (bake-to-constants later). Design + the STEP-0 wedge map: [`design/instrument-engines.md`](design/instrument-engines.md) §8.5 step 9.) Prior: 2026-06-09 (**the buffered string pair SHIPPED — `INSTR_GUITAR` (26) + `INSTR_PIANO` (27)** — the §8.5 step-9 engines, unblocked by stereo. GUITAR = Karplus-Strong string + body resonator (acoustic / nylon / banjo / koto / harp / uke, + pizzicato as a short-mute preset). PIANO = a **verbatim** navkit StifKarp port: a near-lossless KS string (decay rides the amp envelope, not the loop — that's what keeps the upper harmonics alive), dispersion/inharmonicity, per-voicing soundboard + tone-filter, a detuned 2nd string, and an attack brightness bloom; 6 voicings grand→celesta. **The lesson** (and why two prior attempts read *thin*): param-matching navkit isn't enough, the DSP has to be ported line-for-line — at which point the harpsichord matched navkit's actual render (the ear-confirmed target; the Rhodes/Wurli/bowed mis-tunings taught us to trust ears+renders over navkit's labels). `HARP` folds into `GUITAR` and clavinet stays an EPiano position (preset-not-engine, like Rhodes/Wurli/Clav). Core fix riding along: **quietest-voice stealing** in `sound_find_voice` — steal the most-faded ring-out tail, not a loud note (the "clamped after a few chords" cure, now that the string engines sustain for seconds; helps pluck/guitar too). Showcase `guitar`+`piano` carts (2-row tuning knobs), soundcheck slots 24/25 (tripwire PASS), full 4-place wiring + tcc symbols. **NOT yet locked: the macro mappings + the sound itself isn't fully ear-signed-off** — harmonics/timbre/morph currently map to body/voicing/hammer/pedal, and the `eng_tune()` weight/attack amounts are EXPERIMENTAL scaffolding still being dialed by ear (bake-to-constants + retire later). Design + the full porting journey: [`design/instrument-engines.md`](design/instrument-engines.md) §8.8.9.) Prior: 2026-06-09 (**stereo SHIPPED** — the audio path went 2-channel and gained a pan API in two verified increments. Step 1: stereo stream, every voice centered (`L=R=mix`), proven byte-identical to the pre-stereo mono baseline (a `--det` render's left channel matched on all 220,500 samples) — the `--wav` path + both WAV analyzers (`wav-analyze.js`/`wav-envelope.js`, L+R→mono downmix) went stereo-aware in the same commit so master is never half-broken. Step 2: `instrument_pan`/`note_pan` + `LFO_PAN` (dest 7), slewed like vol/duty, per-slot default inherited at note-on; **linear pan law with center unchanged** means a centered cart is still byte-identical, so **no `--det` baseline regen was needed** (no existing cart pans). Master soft-clip now derives one gain from the channel peak (preserves the pan ratio, bite #5) and the steal-declick tail is per-channel (bite #4) — both byte-identical when centered. Echo stays a mono bus in v1; ping-pong + stereo reverb/width are deferred to the §8.10 effects layer, where the §8.10.1 PARKED items (per-voice wah, follower, epiano tremolo, suitcase auto-pan) fold in on the pan + `LFO_PAN` primitives. Showcase: the `pan` cart. Verified: panned render 99.9% L≠R, sound tripwire clean. 4-place wired + tcc symbols. Full spec: [`design/stereo.md`](design/stereo.md).) Prior: 2026-06-07 eighteenth pass (**`ui.h` widget kit v1 SHIPPED** — Open item 25 built same-day-next-day from its design doc: `runtime/ui.h` (gestures.h-pattern cart-land header) ships **button / slider / knob** for mouse + touch + keyboard at once — per-contact capture (two fingers = two knobs), deferred press resolution (visual hit beats the ≥24px inflated hit, so sfxgen's 17 rows at 9px pitch route right), opt-in marching focus ring on `btn()` (gamepad-ready), `ui_grabbed`/`ui_released` events timed for undo hooks. **Panel + drag-from cut from v1** (user call — the per-widget second-customer rule found them speculative). Second customers shipped: `uikit` probe cart (audible knobs, ball-pit sliders, FOCUS toggle) + the **sfxgen retrofit** (17 sliders + 11 buttons → widgets, drag machine deleted, undo verified by scripted replay — which also forced two API fixes: grab-event-before-value-change, rect-only button identity). mobile-lint now inlines runtime/ headers → uikit/sfxgen rank touch-ready by construction. Open tail: the on-device probe (two-knobs-at-once, fat fingers) + more retrofits. [`design/ui-widgets-notes.md`](design/ui-widgets-notes.md) §7. **Discoverability follow-through (item 28):** the library-header lane got documented (CLAUDE.md runtime/ tree + cart-authoring.md "Cart-land library headers" table) and the **read-only engine-source viewer MOVED out of its code-tab overlay into the docs tab** as a dedicated "engine source" sidebar group (studio.h + the four library headers + sound.h/studio.c); cmd-click an `#include` still jumps there. Deleted the overlay (HTML/CSS/✕/Esc) and outline.js's preview-override machinery — the code tab is now only ever your cart. *(editor change — bundle-verified, not yet eyeballed live.)*) Prior: seventeenth pass (**instrument session** — (1) **`instrument_tune(slot, float semitones)` SHIPPED** — slot-level detune (±24, fractions the point) read LIVE by every sounding voice each sample, **scheduled arp/seq hits included** — the one pitch control that reaches fire-and-forget notes. Born from sh101's TUNE trimmer not reaching the arp; also = unison-detune pairs (audio-notes §12 gap 2 PARTIAL) and gamelan ombak. Full 4-place wiring + tcc symbols, soundcheck tripwire PASS, WAV-diff proof (two identical arp runs, one with a TUNE drag → bytes diverge). (2) **sh101 touch overhaul** — keybed-dominant relayout (keys 19→23px wide / 118→146px tall, full width; faders shortened; VOL/PORTA/porta-mode/TRANSPOSE moved into the mid strip, logo compacted; BENDER now vertical at the keybed's left like the real lever), per-finger pointer table (chord + faders at once; slide-legato key handover so AUTO porta glides), fader grab zones tile each section (near-miss lands on the nearest fader), claim_tap guard for tap targets inside knob grab boxes; mobile-lint verdict tap-as-mouse → **touch-ready**. TUNE made live. (3) **harness fix in `poll_virtual_touches()`**: the synthetic mouse-touch read RAW raylib mouse, so `--script`/`--replay` could never drive any touch-path cart (tap/tapp/touch_*) — it now honors the injected pointer (verified: scripted fader drag moves cutoff in the trace). (4) **handpan cart SHIPPED + published** — INSTR_MALLET as an *instrument*, not a tuning rig: ding + 8 fields zigzagged so neighbors are consonant, strike offset from field center = per-hit timbre macro, shoulder tok, 4 tunings (kurd/celtic/hijaz/pygmy), full multitouch; first build off cart-library-direction §2b. (5) **moog: DRIVE as an LFO target** — cart-side (no engine LFO_DRIVE dest): drive_live() wobbles the slewed note_drive per frame; target button cycles OFF/PITCH/DUTY/VOL/CUT/DRIVE. (6) **tinydaws design doc** (design/tinydaws.md): ReBirth-style genre racks, generate→play→export, the lane format keystone, seed-as-song-code, song.h export tier, 8-rack table + the wider map; cart-library-direction §2b whimsical-instrument list (now/blocked split).) Prior: sixteenth pass (**the fifteenth pass's touch worklist BUILT, device-passed, and rolled out catalog-wide, same day** — (1) the **web phantom-touch fix SHIPPED & DEVICE-PASSED** (Open item 24 closed): `web_shell.html` mirrors the DOM's spec-correct `event.touches` into `Module.deTouches` every touch event, `poll_virtual_touches()` reads it via EM_JS under `PLATFORM_WEB` (raylib fallback when absent; native untouched) — two-finger simultaneous releases now drop `touch_count()` to 0 on device, touchpiano's lift ripples fire on every lift. Bonus device finding #5 (touch-notes): the iPhone's **6th finger mass-cancels ALL touches** — correct OS behavior (UIKit does the same), pre-fix it left five permanent phantoms, post-fix it's a graceful all-release; do not "fix". (2) **`touch_ceiling()` SHIPPED** — first of the mobile-web-notes §5 device-facts trio (5 iPhone / 10 iPad / maxTouchPoints Android / 0 desktop, computed in the shell incl. the iPad-pretends-to-be-a-Mac trap), full 4-place wiring; touchpiano's header prints "this device tracks max N fingers". Also: iOS long-press loupe/tap-highlight suppressed shell-wide (the suppression CSS now covers html/body, not just the canvas). (3) **The WHOLE catalog is live — 52 → 263 carts** at <https://nikkikoole.github.io/dreamengine/>, every cart rebuilt on today's engine, and the gallery grew controls: **sort newest/a–z/mobile-readiness** (newest default), **☀/☾ day-night**, **desc 3-lines/full/off** toggle, per-card "added YYYY-MM-DD" — dates read from each cart's first git commit at build time, so bulk publishes never flatten the timeline. The sweep caught one rotted cart: dwarffort's local `paused` collided with the newer `paused()` API (the CLAUDE.md namespace-trap list strikes again) — renamed, fixed everywhere.) Prior: fifteenth pass (**research + design-doc session, no code** — three worklists written up for pickup. (1) **The web phantom touch point is ROOT-CAUSED**: emscripten#4679 (`wontfix` — touchend conflates remaining+lifted touches, all flagged `isChanged`) × raylib's single-removal-per-touchend (the 5.5 we vendor has PR#4488's blunt `pointCount--`; even master `break`s after removing one point) ⇒ releasing two fingers in one event under-decrements and leaves a stale contact — exactly the on-device symptom, and why gestures.h can't fix it (garbage in). Fix plan (rebuild a JS mirror from spec-correct `event.touches` every event in `web_shell.html`, `poll_virtual_touches()` reads it via EM_JS on web): [`design/touch-notes.md`](design/touch-notes.md) **§7**, + gestures.h follow-ups (2-finger pan vs pinch, id-keyed pinch pair) §8 — Open item 24. (2) **Editor hand-editing gaps** (no save-in-place / the sprite story / gallery metadata unreadable+uneditable) explored with sliced proposals: new [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md) — Open item 26, feeds item 23. (3) **`ui.h` cross-input widget kit** (button/slider/knob/panel/drag-from, cart-land gestures.h-pattern header; capture table, value-address identity, hit-pad ≥24px, opt-in focus ring for keyboard/gamepad; second customers = `uikit` probe cart + the sfx generator's 17 sliders): new [`design/ui-widgets-notes.md`](design/ui-widgets-notes.md) — Open item 25.) Prior: fourteenth pass (**editor: cmd-click include → engine-source preview** — cmd/ctrl-click the filename of a quote-include (`#include "gestures.h"`) and the runtime file opens READ-ONLY inside the code tab, overlaying the editor VS-Code-preview style (no extra tab; the cart underneath is untouched, ▶ run still builds it; ✕/Esc closes). Served by a new vite `/runtime-src/` route (flat `runtime/*.h|c` whitelist — works in Electron and the browser tab; route changes need a dev-server restart). Clicks keep working inside the preview — includes chain to other headers, documented symbols jump to the help tab — and the **outline sidebar follows the previewed file** (headers are declaration-heavy, so a prototype pass joins the definition scan; clicks jump+flash in the preview, the cart outline returns on close). Design journey: started as a persistent viewer tab + topbar back-button/nav-stack, user feedback simplified it to the in-tab preview whose ✕ made the back button redundant — net one new module (`editor/src/navigate.js`), a vite route, an outline override hook. Discovery affordance for the engine-internals headers: only `studio.c` includes `sound.h`, so type `#include "sound.h"` on a scratch line and cmd-click it; header-library carts to click through: touchlab → gestures.h, cocktail/roadhouse → improv.h, the radio carts → radio.h.) Prior: thirteenth pass (**editor "publish to site" button SHIPPED** — settings → "publish to site" toggle (profiler-button pattern, with the danger note: commits + pushes to github.com/NikkiKoole/dreamengine, public ~1 min later) reveals a 🚀 button next to "build for web": compiles the CURRENT editor cart (code + sprite-editor sheet + map + settings) straight into `site/<name>/`, **writes the C source back to `tools/carts/<name>.c`** so repo and site can't drift, then `build-site.js --finish` (manifest/title/thumbnail/gallery; placeholder thumb + bare-name title for unregistered carts) + `publish-cart.sh --no-build` (git leg only; strays guard widened to site/ + the published carts' own `tools/carts/<name>.*`, nothing else). The editor deliberately never touches `editor/public/carts/` or `index.json` (shared-registry hazard) — proper registration stays a CLI step. Spawned open item 23: the **sprite story** (editor pixels vs `.cart.js` generators = two sprite sources of truth; publish ships editor pixels but can't write a generator back). Same entry, prior day's unledgered work (commits b57ffb6/f1a77be/8eb5a40): **touch-release API + touch piano + gestures.h SHIPPED & DEVICE-PASSED** — touch-notes §3 landed verbatim (`touch_ended_count/id/x/y` + `tapr()`, one `vt_prev_pos[]` snapshot + vanished-contact scan), shipped per the second-customer rule with `touchpiano` (two-octave multi-finger piano, per-finger note_on/off keyed by `touch_ended_id`, glissando, lift-ripples; **5-finger chord confirmed on iPhone — Safari's ceiling — each note released by its own finger**); `runtime/gestures.h` (improv.h-pattern) gives per-finger swipes judged at lift (`gest_update()`/`swiped()`/`swiped_in()`/`pinch_scale()`) after the device verdict that rgestures' one-global-gesture model is unusable for games (touchlab test 9 vs test 8: swipe-while-drumming fires vs dies; touch-notes §4 closed: never wrap); plus the **ghost-contact fix** — mouse-as-touch synthesis now latches off after the first real touch (iOS emulated-mouse compatibility events were joining the pool as a stale contact; touch-notes device finding #3).) Prior: twelfth pass (**mobile-readiness sweep** — the gallery meets phones: `tools/mobile-lint.js` (static phone-playability report card, verdicts rank by *best* input path) + **gallery badges** (every card stamped 🟢 Mobile Ready / 🟡 Mostly Playable / 🟠 Rough on Mobile / 🔴 Desktop Only — `build-site.js` requires `lint()` directly; strict tiering, any dead input path demotes; hand-tested `"mobile":` override in index.json beats the lint), plus fullscreen button + per-cart home-screen install (PWA manifests) and the touchlab rgestures device verdict (never wrap — gestures go cart-land). Worklist + device findings: [`design/mobile-web-notes.md`](design/mobile-web-notes.md), Open item 22.) Prior: eleventh pass (**playable web cart gallery SHIPPED** — the sharing guide's "what's missing is a URL" is resolved: <https://nikkikoole.github.io/dreamengine/>. `tools/build-site.js <name>` compiles carts to wasm (emcc + raylib-web, per-cart workdirs in `build/.site/`) into `site/<name>/` + a thumbnail gallery index; `tools/publish-cart.sh <name>` = build → commit → push; `.github/workflows/pages.yml` publishes the committed `site/` (no emscripten in CI). ~283 KB wasm / ~3 s build per cart. First 13: the music room (cr78/tr808/tr909/tb303/sh101/moog/spacecho/otamatone/drummachine) + dinorun/juice/multitouch/touchlab. `web_shell.html` grew a runtime rotate-your-phone hint (hint only — CSS-rotating the canvas would break the GLFW shim's touch mapping). See [`guides/sharing.md`](guides/sharing.md).) Prior: tenth pass (**TR-909 cart SHIPPED** — fifth classic box (`tools/carts/tr909.c`), completing the ReBirth RB-338 rack (303+808+909). Analog kick/snare/toms per the §14 assessment; the ROM-sample hats/cymbals solved better than predicted — `INSTR_FM` on the 3.5 inharmonic clang detent through a closing highpass (negative `ENV_CUTOFF` amount, engine clamps make it safe) instead of the sketched SFX-editor arrays; 909 shuffle period-correct (even 16ths drag). See Open item 21.) Prior: ninth pass (**`INSTR_PLUCK` + three-macro surface SHIPPED** — Karplus-Strong string engine (`INSTR_PLUCK`, wave id 16) + the full §8.1.1 macro surface (`instrument_harmonics/timbre/morph` + `note_*` twins) landed, showcased by the `pluck` cart (eight pentatonic strings, three live knobs, autoplay noodle). The per-voice KS buffer (§8.2) is now concrete — the whole buffered engine family rides this path. The station retrofit (jangle/bossa → INSTR_PLUCK) is the open follow-up. Prior: 2026-06-04 eighth pass (**API usage audit + `music()` cut** — `tools/api-usage.js` cross-checks all 182 API functions against all 233 carts (+ the four-places doc coverage); findings in [`design/api-usage-audit.md`](design/api-usage-audit.md): 11 functions never used, `paused()` doc gap closed. Biggest finding acted on: **`music()` cut** ([decision 0013](decisions/0013-cut-music-api.md)) — zero cart users, patterns were never cart-authorable (only the hardcoded bass+hihat demo), and the project's real music path is the generative beat clock. `Pattern`/`music_bank`/`from_music` machinery removed from `sound.h` (voice stealing simplifies to free → non-held → voice 0); `sfx()` stays — its living role is `sfx(-1)` "silence ringing sounds" (8 of 11 cart call sites) + 6 first-contact demo slots. Soundcheck tripwire: PASS. Same-day follow-up: **three more zero-adoption helpers cut** — `bezier_cubic`/`anim_once`/`bounce_at_edges`, each the unloved sibling of an adopted helper ([decision 0014](decisions/0014-cut-unused-convenience-helpers.md))). Prior: seventh pass (**sound reliability sweep** — (1) the silent-drop bug class is now LOUD: the engine counts dropped requests (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED`; found via the `wave_set` queue flood that made every modrack wav position play square (fix: wave_set packs 4 samples/request, queue 256→512). (2) **`soundcheck` cart** — the sound-engine self-test: worst-case define burst + full-API walk + a 40-shot `schedule_hit` burst; PASS = silent log + 9 audibly distinct waves; run it after touching `sound.h` — see [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire". (3) **modrack: drawn user waves** — wav knob 5→9 positions (`org/vox/bel/fld` baked via `wave_set`); `SOUND_INSTR_SLOTS` 16→32. (4) **modrack knob-index enums** (`VK_*`/`BK_*`) replacing raw param numbers — the rename-by-intent flushed six more preset cross-wires left over from the flt-knob insertion; a reordered knob list now fails at the compiler instead of silently cross-wiring). Prior: sixth pass (**per-cart save folders** — new `--save-dir` runtime flag; the editor (all three spawn paths: run / profile / live host) and `play.js` pass `saves/<cart>`, so `cart.sav`/`cart.kv`/`cart.blob` live under `build/saves/<cart>/` instead of one shared set in `build/` — a fresh cart can no longer inherit another cart's hiscore. Same isolation idea as `build/.bake/<name>`, but for persistence. **monster mix lab cart** — the `sprite-draw.js` `stamp()` showcase: 9 drawn parts composited into all 27 combos at bake time, ×4 `pal()` recolor schemes = 108 monsters from 9 parts; gameplay: assemble the customer's order and piston-stamp it). Prior: fifth pass (**sfx editor: per-step filter CUT lane + RES slider** — a third lane after pitch/volume: per-step lowpass (top = wide open / FILTER_OFF, exponential 150Hz–4.8kHz) for wah-plucks and opening sweeps. Implementation note: scheduled-ahead steps each carry their settings in a rotating instrument slot (10–15), since defines apply immediately but hits fire later; the export emits the slot-rotating player only when the filter is used). Prior: fourth pass (**profiler always-on in native builds** — `PROF()` counters + frame timing now compiled into every native build, not just `-DDE_PROFILE`; `profiler_request` trigger file lets you snapshot `perf.json` from any running cart the same way `screenshot_request`/`state_request` do. **Release build mode** — settings → run mode → "release" compiles with `-O2 -DDE_RELEASE`, stripping `PROF()`, timing, and `harness_inspect` polling). Prior: third pass (**live inspection shipped** — on-demand screenshot + state snapshot via trigger files (`build/.bake/screenshot_request` / `build/.bake/state_request`); handshake: game captures on next frame tick and deletes request. Before/after pairs work across a live run. See [`guides/debug-harness.md`](guides/debug-harness.md)). Prior: second pass (**sound tooling sprint**: `schedule_hit` shipped (delay+duration note — sub-frame sfx steps); `wave_set` + `INSTR_USER0..3` shipped (drawable single-cycle waves, §8.4 partially resolved); three sound-tool carts shipped — `sfx editor`, `sfx generator` (sfxr categories + mutate), `wave editor` (live-morph drone) — all exporting paste-ready C, validating the §5.6 "editor cart, no engine banks" direction). Earlier same day (**sound: mod-envelopes SHIPPED** — `instrument_env`/`note_env` (ENV_CUTOFF/PITCH/DUTY, kinds 18/19) + demo carts `filterenv`/`pitchenv`, wired into modrack (onboard fenv/penv, VCA `a` jack + flat bank, independent clocks, crisp zoom, scaling cables) and the dream synth (AMP/FILTER/PITCH envelope tabs). audio-notes §2 refreshed as the current surface map (32 fns + 32 consts, the 3-way mod matrix); §9 resolved entries struck (handles won, per-instrument filter, duty placement). **SFX authoring direction:** prototype as a PICO-8-style editor cart, zero new engine API (§5.6). Prior 2026-06-03: **modulation envelopes decided as the next feature**, built before the navkit instrument engines — a routable second EG (`instrument_env`, dests `ENV_CUTOFF`/`ENV_PITCH`), the one-shot twin of the LFO; surfaced ear-testing navkit's pluck (filter-env + pitch-env are one primitive). New audio-notes §11; item #5 reordered. Prior same day: Picotron API comparison — added four ideas: `menuitem` folded into the Pause item (#4 — same feature, two ends), frame-spanning **sequence scripts** (#17, kept distinct from the cut DIV process model), **blend tables** (#18 — index-only translucency/fog/additive, the real capability gap), and a **userdata/offscreen-buffer reframe** folded into the rotation-atlas item (#13 — `sset`/canvas/rotation-cache are one general primitive). Sound comparison corrected: Picotron's audio is a deep node-graph synth + tracker, so the real distinction is code-first vs GUI, not depth). Prior: 2026-06-02 (session 14 — `fps()` shipped as the perf read-out; **one-click profiler shipped** (⏱ profile button, see [`guides/profiler.md`](guides/profiler.md)); **off-screen poly bbox clamp shipped** (item 14) — a cliff guard, ~17× on the synthetic stress cart, modest on real carts; `trifill_stress` regression cart added). Prior: session 13 — `fade()` made immediate-mode, fixing a 27-cart stuck-dim bug._

---

## Shipped ✓

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help +
  cmd-click an `#include "x.h"` filename → opens the read-only engine source in the
  docs tab's "engine source" group), sprite editor, map editor — all in one
  PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — 263 registered carts (tutorials, games, toys, instruments, probes);
  all of them also playable on the web gallery (<https://nikkikoole.github.io/dreamengine/>,
  sortable by date-added/title/mobile-readiness, day/night, description toggle).
- **`sprite-draw.js` post-processing batch + `foundry.cart.png`** (2026-06-04) — five new ops for programmatic `.cart.js` sprites: `shade()` (auto light/shadow via the curated `RAMP_DARKER`/`RAMP_LIGHTER` palette LUTs — *the* "one step darker/lighter" table for the fixed palette), `rotate()`/`rotations()` (baked headings, still post-processable unlike runtime `spr_rot`), `scale2x()` (EPX: sketch 16×16, bake 32×32), `replace()`/`clone()` (bake-time variants); `split()` now grid-splits a 32×32 into 4 slots as its comment always claimed. Showcase: **SPRITE FOUNDRY** — "watch the code draw": `foundry.cart.js` snapshots the canvas into the next slot after every drawing step, and the cart plays each subject's time-lapse back with the code line per frame (dragon → `shade()`, ship → `mirror()`+`rotations()`, boss → `noise()`/`replace()`/`scale2x()`). Tutorial 15 (animation phase) also rebuilt on the library: its 6-frame walk cycle is one parametric `walker(t)` sampled over the stride. See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "sprite-draw.js".
- **`ragdoll.cart.png`** — Verlet physics sandbox. Up to 50 stick-figure ragdolls hop autonomously across the screen, bounce off each other and off rolling balls. Grab + throw with mouse (whole-body drag), right-click to spawn balls, C to spawn characters, R to reset. Key techniques: Störmer-Verlet integration, distance constraints (12 sticks/character, 8 iterations), position springs chained bottom-up (feet → knees → hip → chest → head), angular springs per bone with 90° guard (cross-product direction inverts past 90°), per-character standing/ragdoll timer that only recovers when feet are on the floor, hop impulse that immediately releases the foot pin so it isn't erased, broad-phase character collision (hip-to-hip > 40px early-out then 12×12 point pairs with velocity impulse). Debug session used the play.js harness + DE_TRACE watches to trace `rtimer`, `whop`, `hip_y`, `knee_y` and diagnose: rest lengths mismatched standing pose (hip-knee was 9, actual √73 ≈ 8.54 → knee pushed to floor); hop velocity erased by foot pre-pin each frame (fixed by setting `rtimer=0` inside `hop_tick` and re-reading `sk`). See `tools/carts/ragdoll.c`.
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- **Live (libtcc) backend + hot reload** — a "run mode" toggle (settings) switches ▶ run from the clang static build to a persistent `-DDE_TCC` host that JIT-compiles the cart in-process via vendored `runtime/libtcc/`. Editing the code auto-reloads it (debounced, no Run press) without restarting the window; compile errors mark the line and keep the last good cart running. State survives reloads via **`de_state()`** — promoted to a first-class `studio.h` API and fronted by the starter cart's friendly `STATE { ... }; / S->field` sugar (clickable to help). arm64-macOS only; sprite/screen changes relaunch. Full record + rationale: [`design/cart-as-script.md`](design/cart-as-script.md).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).
- **Live inspection** — on-demand screenshot + state snapshot while a cart runs. Write the desired output path into `build/.bake/screenshot_request` or `build/.bake/state_request`; the game captures the current frame on its next tick and deletes the request file as the handshake (gone = fresh, ready to read). Lets you bracket a specific moment: capture before + capture after = instant diff without a filmstrip. Works alongside `play.js run --headless` and any other harness mode. See [`guides/debug-harness.md` → Live inspection](guides/debug-harness.md).
- **Profiler** — one-click `⏱ profile` button (hidden behind a settings toggle). Compiles a profiling build (`-O1 -fno-inline -DDE_PROFILE`), runs the cart ~4s, and reports into the build log: frame CPU budget (ms vs the 16.6ms 60fps target), hottest functions **with the call paths that reach them** (macOS `sample` call-graph attribution, rolled up to `studio.h` primitives), and exact per-frame draw-call counts (in-engine `PROF` counters, re-entrancy-guarded). Behind the scenes — no Instruments GUI. macOS-only (uses the `sample` CLI). **`PROF` counters + frame timing are now always on in native builds** (not just `-DDE_PROFILE`) — `perf.json` is written every 30 frames in any normal run; snapshot it on demand with `profiler_request` (same trigger-file pattern as `screenshot_request`). `-DDE_RELEASE` strips all overhead (new settings toggle, see below). See [`guides/profiler.md`](guides/profiler.md).
- **Release build mode** — settings → run mode → "build mode" select. `normal` (default): profiler counters + `harness_inspect` polling on, `-Os`. `release`: `-O2 -DDE_RELEASE` — strips `PROF()`, timing measurement, and all per-frame trigger-file probes. For when you want to benchmark or ship without any instrumentation overhead.
- **Per-cart save folders** — `save()`/`save_int()`/`save_bytes()` files (`cart.sav`/`cart.kv`/`cart.blob`) live in `build/saves/<cart>/`, not one shared set in `build/`. Runtime takes `--save-dir DIR` (any native build, default cwd); the editor slugs `cartName` and `play.js` uses the cart's file stem, so editor saves and harness saves are separate folders per cart — a scripted test run can't clobber a real hiscore. Web build unchanged (no argv). See [`guides/debug-harness.md`](guides/debug-harness.md) flags table.
- **`monstermix.cart.png`** — the `sprite-draw.js` `stamp()` showcase cart. The `.cart.js` draws 9 parts (3 heads, 3 bodies, 3 legs, `mirror()`ed) and `stamp()`-composites all 27 combos into slots at bake time; magic `pal()` indices 28/29 recolor them into 4 schemes at draw time — 108 monsters from 9 parts. Also exercises `split()` (32-wide machine), concave `polyfill` (star), `noise()` tiles, `outlined()` with a custom outline color. Gameplay: assemble the customer's order, piston-stamp it (squash + dust + shake), combo chords climb with the streak. See `tools/carts/monstermix.c` / `.cart.js`.
- **`tools/font-bake.js` + `fontbake.cart.png`** (2026-06-05) — real-TTF text as sprites, at build time. Parses a TTF (vendored `tools/vendor/opentype.cjs`), flattens the glyph outlines and scanline-fills them (nonzero winding, 3×3 supersampled, optional darker AA-edge color) into sprite-draw 2D canvases — so any Google Font can be a cart's title with zero runtime font code. `measure()` for fitting a slot budget; border/shadow are plain sprite-draw composition (`outlined()`, offset-stamped recolored clone). Fonts live in `tools/fonts/` (Bungee + OFL included; new ones are one curl from github.com/google/fonts). Showcase: **font bake** — words baked centered into fixed slot-rects so the C side `sspr()`s constant sheet regions; title waves in 4px strips, `pal()`-recolors live (fill + AA edge remapped together — swapping only the fill leaves a clashing rim). Same-day follow-up: **`bakeBanner` promoted into the library** (fit + center + outline + shadow → ready tiles; second customer) and **high noon cart** (Smokum) — a quick-draw reaction duel where the baked words ARE the game (DRAW! signal, DEAD/EARLY!/YOU WIN verdicts), five words packed to exactly 64 slots; full championship/death/early paths verified via scripted play.js runs. Hard-won rules now in the guide: `colorkey(0)` in `init()` is mandatory (no default transparent color — banners drag an opaque black slot-rect without it), and every word needs two slot-rows (one row = ~11px glyphs after outline trim, too thin at 2x). See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "font-bake.js".
- **THE BAND panel** (2026-06-05) — every chaired radio station gets a live timbre-audition overlay: press **B**, click a chair row (or press its number) to cycle that chair's instrument candidates mid-song. The G-key A/B pattern generalized: `runtime/radio.h` owns the registry + input + draw (`rad_chair` / `rad_band_input` / `rad_band_panel`), the cart applies each swap in its own `apply_chair()` — the toolkit never calls back in and never touches `rad_srnd`, so pinned seeds stay byte-identical. Picked chairs re-assert after `new_song`'s per-song rolls; chairs left at sel 0 keep the shipped sound and roll. Chaired so far: **cocktail** (solo chair hands improv.h's chorus to piano/vibes/guitar), **tango** (the three orquestas: troilo/d'arienzo/pugliese reed tables, arco/pizzicato violins, felt/dark piano), **yacht** (dx tine/rhodes/clavinet ep, three basses, three leads, pad), **roadhouse** (VOX/Gibson drawbar tables, rhodes/upright piano bass, guitar), **exotica** (vibes/marimba/denny-piano, fm-glass/celesta). Candidates sourced from [`design/radio-instrument-options.md`](design/radio-instrument-options.md) — ten stations there still unchaired.
- **`tools/sprite-draw.js`** — shared programmatic sprite-authoring library for `.cart.js` files. Exports a 2D pixel-canvas API aligned with the C drawing API names: `blank`, `pixel`, `rectfill`, `rrectfill`, `line`, `circlefill`, `ovalfill`, `trifill`, `polyfill`, `ngonfill`, `noise`, `outlined`, `mirror`, `stamp`, `flat`, `split`, `OUT`. All 14 programmatic `.cart.js` files `require('../sprite-draw.js')`. Three `.cart.js` authoring styles documented: (1) settings-only, (2) ASCII art with `DEFAULT_CHAR_MAP` (palette 0–15), (3) programmatic arrays via sprite-draw (palette 0–31, geometry). See `tools/sprite-draw.js` and `CLAUDE.md` → project structure.

**API surface** — ~125 functions + ~90 constants in `runtime/studio.h`.
For the full grouped inventory see [`design/api-notes.md` → "What dreamengine has today"](design/api-notes.md).
Recently landed and worth calling out:
- Juice batch: `pal`/`pal_reset`, `fade`, `shake`, `print_scaled`, `text_width`.
- **Font system:** `font(FONT_NORMAL/FONT_SMALL/FONT_TINY)` state setter; two extra fonts baked (`font4x6.png` ~64 chars/320px, `font3x5.png` ~80 chars/320px); `print_shadow`, `print_outline`; all `print*` functions now return x-after-last-char for chaining and overflow detection. Demo: `fonts.cart.png`. See [`design/font-rendering.md`](design/font-rendering.md).
- Sprite transforms: `spr_rot`, `sspr_ex` (rotation/scale/flip around a pivot).
- **`sget(sx,sy)`** — palette index of a *spritesheet* pixel (companion to `pget`, which reads the canvas). Lets carts treat sprites as data: paint a level in the sprite editor (1 pixel = 1 block, color = type) and read it back at runtime, or build lookup tables. Shipped with two paired platformer tutorial carts — **`platform-rects`** (a pixel-perfect AABB mover: per-axis resolution + sub-pixel position, coyote time, jump buffering, variable jump height, one-way platforms; level as a hard-coded `Box[]`) and **`platform-paint`** (same mover, level read from a painted sprite via `sget`). Same engine, two level sources — the "level as code vs level as data" teaching pair from [`design/tutorial-curriculum.md`](design/tutorial-curriculum.md).
- Fill patterns: `fillp`/`fillp_reset` + `FILL_*` (PICO-8-style fillp).
- Shapes/helpers: `oval`/`ovalfill`, `bar`, `blink`, `fsqrt`.
- Pseudo-3D: `tritex` (affine texture-mapped triangle; used by `mode7`/`raycaster`/`cube3d`).
- 3D leaf-helpers: `V3` + `rot3`/`project3`/`zsort`/`quadfill` — the rotate→project→sort→fill
  pipeline the solid-3D carts re-derived by hand. `cube3d`/`solid3d`/`textured3d`/`flyover`
  refactored onto them. [decision 0009](decisions/0009-small-3d-leaf-helpers.md).
- **`fade()` is now immediate-mode** — the runtime zeroes it each frame, so a cart re-asserts
  `fade()` only on the frames it wants the screen dimmed and never calls `fade(0)` by hand. Fixed
  the same stuck-dim bug in **27 carts** at once (conditional overlay fade that never cleared on
  exit). `camera`/`pal`/`fillp` remain sticky setters. [decision 0010](decisions/0010-fade-is-immediate-mode.md).
- **Removed:** turtle graphics (`turtle_*`/`pen_*`) — one user, trivially a cart; see Cut
  below + [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
- Input: full mouse (`mouse_x/y/down/pressed/released/wheel`), keyboard
  (`key`/`keyp`/`text_input`).
- Time/persistence: `dt`, `epoch`, `save_bytes`/`load_bytes`.
- **`fps()`** — measured frames-per-second right now, `int`, smoothed over the last second
  (wraps Raylib `GetFPS()`). 60 = smooth; lower = the cart is too slow this frame. Independent
  of `dt()`/`det_mode`: even when the sim's `frame_dt` is pinned for deterministic replay,
  `fps()` still reports *real* render throughput, so scripted/headless runs stay reproducible
  while showing true speed. Intended as the read-out for the triangle-perf work (open item 14):
  `watch("fps", "%d", fps())` on a haze-heavy `podracer` frame, before/after a change. *(A
  dedicated profiling setup is in progress separately.)*

**Code-first sound** — 8-voice synth; `note`/`hit`/`chord`/`strum`/`tone`/`degree`,
`bpm`/`beat`, `every`/`euclid`/`chance`, `schedule`, `schedule_hit` (delay **+** duration —
sample-accurate sub-frame sfx/arp steps). (The `sfx` bank plays built-in demo data only —
see "Open" below. **`music()` is CUT** as of 2026-06-04 — zero cart users, patterns were
never cart-authorable, the generative beat-clock path won;
[decision 0013](decisions/0013-cut-music-api.md).)
- **Modulation envelopes** — `instrument_env()`/`note_env()`: 2 routable one-shot AD
  envelopes per slot (`ENV_CUTOFF` = the pluck "pew", `ENV_PITCH` = drum punch/zap,
  `ENV_DUTY`), bipolar amount, exp decay — the second EG (audio-notes §11). Demo carts:
  `filter env`, `pitch env`; wired into `modrack` (onboard fenv/penv + a VCA `a` jack) and
  `dream synth` (AMP/FILTER/PITCH envelope tabs).
- **Drawable waveforms** — `wave_set()` + `INSTR_USER0..3`: four 64-sample single-cycle
  tables you can draw and play like any wave; live-morphable (a ringing note changes as the
  table is rewritten). Demo cart: `wave editor` (draw the cycle, SPACE-drone + live morph,
  seed shapes, exports `wave_set()` code). The cart-authorable half of
  [`design/instrument-engines.md`](design/instrument-engines.md) §8.4.
- **Sound-tool carts (draw → export-as-code)** — `sfx editor` (paint 32 steps),
  `sfx generator` (sfxp/bfxr-style: 17 sliders + RANDOM/MUTATE + sfxr category buttons),
  `wave editor`. All export paste-ready C (a data array + a tiny player) — the
  decision-0003 answer to sfx authoring, zero engine banks needed.
- **Sound tripwire + `soundcheck` self-test** — the engine counts dropped requests
  (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED` when sound
  calls are lost (the silent class: defines that never land, notes that never play).
  `soundcheck` cart = worst-case burst + full-API walk; run after touching `sound.h`:
  `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  — silence = PASS. See [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire".
- **Instrument synth** — `instr` is now an instrument slot (0–4 = the raw waves,
  unchanged; 5–15 cart-defined). Four expressive axes bundled per slot, all on the raw
  waveforms: `instrument()` (per-voice **ADSR**), `instrument_duty()` (pulse width),
  `instrument_lfo()` (**3 LFOs**/slot → vibrato/PWM/tremolo/wah), `instrument_filter()`
  (resonant SVF: low/high/band/notch). Demo carts: `instruments`, `lfo`, `filter`, and
  `dream synth` (a playable Moog-style patch panel + keyboard). See
  [`design/audio-notes.md`](design/audio-notes.md) §10.
- **Held notes** — `note_on()→handle`/`note_off()` plus live setters
  `note_pitch`/`note_vol`/`note_cutoff`/`note_res`/`note_duty`/`note_lfo`/`note_filter`/`note_glide`
  (+ `note_off_all`). A sustained voice
  you drive frame-by-frame, the opposite of fire-and-forget `note()`: hold-to-sustain,
  engine revs, sirens, theremins. Handles are `index + generation` so a stale handle
  safely no-ops; per-param slew smooths live writes (no zipper). `note()`/`hit()` keep
  their fixed-duration behavior. Demo cart: `held notes` (a theremin); `dream synth`
  retrofitted onto them (real hold-to-sustain + live filter sweep). See
  [`design/held-notes.md`](design/held-notes.md).
- **Stereo + panning** — the audio path is stereo. `instrument_pan(slot, pan)` (per-slot
  position, inherited at note-on), `note_pan(handle, pan)` (live, slewed — positional audio:
  map a sound to where it is on screen), and `LFO_PAN` (auto-pan as an LFO destination).
  `pan` is -1 left .. 0 center .. +1 right. **Linear pan law, center unchanged**: a centered
  voice is byte-for-byte the old mono mix, so existing carts are unaffected. Master soft-clip
  and the steal-declick tail are stereo-correct (peak-gain clip preserves the pan ratio).
  Echo stays a mono bus in v1; ping-pong + the effects layer's stereo width are next (§8.10).
  Demo cart: `pan`. Full design + the build-order + bite checklist: [`design/stereo.md`](design/stereo.md).
- **Input release edges** — `keyr(k)` / `btnr(player, button)`, the falling-edge partners
  to `keyp`/`btnp` (true on the frame a key/button is released). Needed for hold gestures
  (note_on on press, note_off on release).

---

## Open — prioritized

Ordered by leverage. Section refs point at the design doc that specs each item.
Which *carts* are probing these questions (and every verdict so far) →
[`design/probe-carts.md`](design/probe-carts.md); probe carts carry `"probe"` in
their `kind[]` tags.

1. **Cart-pattern helpers** — `hud()` and `game_over_screen()` cut (see Decided-against).
   - **AABB collision already SHIPPED as `boxes_touch()`** (+ `point_in_box`, `circles_touch`,
     `near`, `touching_map`, `tile_at`, `touching_color` — the full collision section;
     `bounce_at_edges` later cut for zero adoption, [decision 0014](decisions/0014-cut-unused-convenience-helpers.md)).
     *Not* a missing primitive. Open question is **discoverability, not API**: a rough
     grep finds ~30 carts still hand-rolling inline rectangle overlap even though `boxes_touch`
     exists and 15 carts use it — so the lever is teaching (a collision tutorial / converting
     example carts), and *maybe* a more-guessable alias name, not a new function. Adding a bare
     `overlap()` synonym would just grow the already-large surface (see VISION's prune note).
   - `explode()` / particle system is a **research topic** before any build: a no-param
     `explode()` risks making all carts look identical (same concern that killed `hud()`).
     Needs design work on color, shape, lifetime, and movement params first.
     See particle survey + open questions in [`design/api-notes.md`](design/api-notes.md) §C.
2. ~~**2D geometry helpers**~~ — **SHIPPED** as `ngon`/`ngonfill`, `star`/`starfill`, `poly`/`polyfill`, `thickline`, `rrect`/`rrectfill`, `vgradient`/`hgradient` (+ outline siblings `arcoutline`/`ringoutline`/`thicklineoutline` so every filled shape has a matching boundary-ring outline). Demo: `shapes.cart.png`. See [`design/geometry-helpers.md`](design/geometry-helpers.md).
   - *Parked thought (not a build item):* true smooth color interpolation (`lerp_color`/`rgb`) — splits the color model; needs its own ADR. Gradients are dithered.
   - ~~**BUG: `vgradient` renders INVERTED.**~~ **FIXED 2026-06-04** — swapped color roles in `gradient_band()` (`fillp(pat, cb); rectfill(..., ca)`). Thumbnails re-baked for trafficjam, loveparade, loopstation, shapes.
3. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
4. **Pause** — runtime-owned pause overlay. (`fps()` SHIPPED — see Shipped above.)
   [`design/api-notes.md`](design/api-notes.md) §16.

   **First pass — SHIPPED** (P/ENTER opens, ESC resumes, freezes `update()`,
   mutes sound, Continue/Restart via `execv`, `paused()` API).
   **2026-06-05 hardening** (sh101's full keyboard collided with P):
   - **Key claiming** — a key the cart reads via `key()/keyp()/keyr()` is
     claimed and skipped by the pause hotkey; claims reset on libtcc hot-reload.
     A full-keyboard cart keeps P; ENTER still pauses everything.
   - `-DPAUSE_KEY` (settings → controls) is honored now — the check was
     hardcoded `KEY_P`, so the rebind never worked.
   - ENTER can actually open the overlay — the menu used to consume the same
     press as Continue in the same frame, net no-op.

   **Deferred — Options submenu (document now, build later):**
   Matches PICO-8's pause → Options screen:
   - Sound: ON/OFF
   - Volume: slider/bar
   - Fullscreen: OFF/ON (one `ToggleFullscreen()` call)
   - Controls: read-only display of current P1/P2 key bindings (rebinding stays in
     the editor settings tab)
   - Back

   **Further deferred:**
   - **Per-player pause key** — currently one shared `PAUSE_KEY` for both players.
     When gamepad support lands, each player should have their own pause button
     (P0_PAUSE_KEY / P1_PAUSE_KEY, same `-D` flag pattern as the other bindings).
     The architecture already supports it — just not exposed in the UI yet.
   - **`menuitem(index, label, callback)`** — let carts add custom rows ("restart
     level", "toggle music", "easy mode") to the pause menu. Zero layout work for
     the cart; ~30 carts currently hand-roll their own options screens.
5. **Sound expansion** — _instrument bank (ADSR/duty/LFO/filter) and **held notes**
   (`note_on`/`note_off` + live setters + slew) now SHIPPED, see above._
   **NEXT (decided 2026-06-03, built BEFORE the engines): modulation envelopes** — a second/third
   envelope generator routed to a destination, the one-shot twin of the routable LFO. One call
   `instrument_env(slot, which, dest, attack_ms, decay_ms, amount)` (+ live `note_env`), **2 dests
   to ship** — `ENV_CUTOFF` (the pluck "pew/dwow") and `ENV_PITCH` (drum punch / attack snap / zap;
   `ENV_DUTY` optional) — **2 env slots** so both run at once, **AD shape, exp decay**. Deliberately
   no `ENV_VOLUME` (= the amp ADSR). This was the missing second EG; doing it first makes every
   engine + raw wave better and frees the pluck `morph` knob to be *inharmonicity* instead of
   filter-decay. Demo carts: `pitchenv`, `filterenv`. Spec: [`design/audio-notes.md`](design/audio-notes.md) **§11**.
   Then, behind it: the **navkit rich-instrument port** (rich non-chiptune voices as `INSTR_*`
   engines, each played behind a tiny fixed 3-macro API: harmonics/timbre/morph, §8.1.1 — all
   §8.x refs here = [`design/instrument-engines.md`](design/instrument-engines.md), split out of
   audio-notes 2026-06-07, numbering preserved).
   The bite order (instrument-engines §8 status note + §8.8 port sketch):
     1. ~~**`INSTR_PLUCK`** (Karplus-Strong)~~ **SHIPPED 2026-06-05** — per-voice KS buffer (§8.2)
        made concrete, full macro surface, `pluck` showcase cart. Station retrofit (jangle/bossa)
        is the open follow-up.
     2. ~~**`INSTR_MALLET`** — buffer-free celesta / music-box voice.~~ **SHIPPED 2026-06-05** —
        4 modal sines + strike click, no buffer, `mallet` showcase cart (5 hardware presets =
        baked macro positions). First full walk of the §8.8.2 engine-shipping playbook. Open
        tail: macro taste-tuning + the lowend vibraphone retrofit.
     3. ~~**`INSTR_FM`** — 2-op + feedback, buffer-free (§12 gap **2a** only — the self-contained
        engine, NOT the deferred Juno second-osc plumbing, gap 2b). Promoted ahead of the organ
        2026-06-05: epiano/bell demand is already on the dial.~~ **SHIPPED 2026-06-05** —
        snapped ratio table, in-note brightness decay (the DX strike), feedback morph; `fm`
        showcase cart (epiano/bell/bass/brass/clang presets + a live formula scope). Design:
        instrument-engines §8.8.3. The two named risks both resolved same-day (§8.8.3
        post-ship findings): epiano tine added → nameplate test PASSED; brass fixed by
        making brightness follow the amp attack. Open tail: plain taste-tuning + the
        citypop/lowend epiano retrofits.
     4. ~~**`INSTR_ORGAN`**~~ **SHIPPED + PUBLISHED 2026-06-07** — Hammond tonewheel, 9 drawbar
        additive, `organ` showcase cart (full design instrument-engines §8.8.4). Post-ship: a
        drive-fizz fix (pre-drive HF rolloff) and the shared per-voice **DC blocker** on the drive
        path both landed. Leslie stays a per-voice recipe / future bus item (0015, §8.10).
     5. ~~**`INSTR_EPIANO`**~~ **SHIPPED + PUBLISHED 2026-06-08** — Rhodes/Wurli/Clav, 12-mode modal
        bank + pickup nonlinearity, `epiano` showcase (§8.8.5). Post-ship tuning (by ear + the
        navkit-render A/B, tools/navkit-render.c): **timbre** given a hammer-hardness tilt so it bites
        on all 3, **bark** folds in drive (clean→growl), clav has a fast filter-env quack. **Rhodes
        rebuilt from MEASURED spectra (2026-06-08, second pass)** — the by-ear body/bell split (old
        `RHO_*` consts) still left a loud sustained inharmonic 4.2× partial that read as an "untuned
        bell"; FFT of our own render confirmed it. Replaced `RAT[0]` with the real structure
        (harmonic body 1-6 + sparse fast inharmonic tine bells 6.27×/17.55×/34.4×) + per-mode `DEC_R`
        + a 2nd-harmonic "voicing" crossfade (Shear 2011 / Münster & Pfeifle 2020), FFT-verified.
        Added a **tremolo** slider (suitcase/Wurli amp wobble). **Wurli** then got its own A/B pass
        (2026-06-09): boosted the OCTAVE partials (2,4,8,16) over the reedy 3rd — the 200A's
        fuller/punchier tone (Reed200 spectral-model crib), FFT-checked + ear-confirmed. **Clav** is
        still the navkit crib (a near-harmonic struck string — plausible, but not reference-validated
        like Rhodes/Wurli; candidate for a future pass). The per-voice **wah is provisional** (flagged TEMP! in-cart)
        — the real auto-wah is a bus effect (§8.10). 🅿️ see PARKED below.
     7. ~~**`INSTR_MEMBRANE`**~~ **SHIPPED 2026-06-08** — struck drumhead, 6 modal sines, buffer-free,
        `tabla` showcase (5 kit presets + drumhead viz). Ported from navkit's `processMembraneOscillator`
        with its live circular-membrane strike-position weighting (timbre) and monotonic pitch-bend chirp
        (morph); the one deviation — harmonics also crossfades the *ratios* tuned-harmonic↔Bessel (tabla
        really is harmonic), not just the amps. Hand percussion the 808/909 sine+pitch-env can't reach
        (instrument-engines §8.5 step 8). (PD shipped separately, item 6.)
     ~~Next: reed → pipe → buffered piano/guitar pair → bowed~~ **ALL SHIPPED 2026-06-09** — the full
     wind + string families are done (`INSTR_BOWED` = violin/cello, arco *and* pizzicato from one
     waveguide; the modeled string family pluck→guitar→piano→bowed is complete). **And `INSTR_BRASS`
     (29) SHIPPED 2026-06-10** — the lip-reed brass family (trumpet→tuba), the LAST engine-blocked
     instrument; STEP-0 found the literal lip mass-spring won't self-oscillate, so it's reed's
     pressure valve + a dynamics-swept brass formant (the blatty "blaaat"); showcase = the `brass`
     cart's trombone slide (§8.8.10). Now **no instrument is engine-blocked.** Next:
     **formant + effects layer.** Additive stays deferred
     (`INSTR_SINE` + FM + MALLET cover its near corners; the MT70 family is its first named customer).
   The **MT70 finding, corrected 2026-06-07** (the 2026-06-03 "all one pure sine" verdict was a
   verification artifact — the songs' `osc2*` fields sit ~50 lines into each block and are
   non-zero): the presets are **2–3 mixed sines with per-partial decay + click** — small
   additive. Conclusion unchanged, better reasons: *no engine port* — the struck half is
   `INSTR_MALLET` territory, exact = 2-slot voice stacking, single-voice ≈ SCW + closing
   `ENV_CUTOFF`; ships as demo/preset carts (instrument-engines §8.9 corrected note).
   Also still open: zero-setup **preset instruments** (`INSTR_PLUCK`/`PAD`/…) and cart-side
   **SFX authoring** (the sfx bank is hardcoded today; *pattern* authoring is settled-no —
   `music()` cut, [decision 0013](decisions/0013-cut-music-api.md)) — *direction 2026-06-04: prototype
   as a PICO-8-style editor CART first (draw the pitch contour, toggle per step), zero new
   engine API — hit()/schedule() + beat clock for playback, save_bytes for persistence,
   export-as-C-code into games; `sfx_def()` only if the prototype proves the engine should
   own banks. See audio-notes §5.6 direction note.*
   ⚠️ The port touches `runtime/sound.h`/`studio.c` — shared with the live/libtcc runtime work;
   sync before starting. [`design/audio-notes.md`](design/audio-notes.md) §5–8, [`design/held-notes.md`](design/held-notes.md).
   🅿️ **PARKED — revisit when the effects-bus layer (instrument-engines §8.10) lands:** the
   per-voice wah (epiano AUTO/TOUCH flavours) and the **envelope follower** (`instrument_follow`/
   `note_follow`) are *interim* — the realistic "woah woah" auto-wah is a BUS effect and will
   likely replace them; the follower's real home is bus-level. Kept (may be handy) but flagged so
   we don't build more on them. When §8.10 is built: reassess whether to fold these into the bus
   wah or remove. Full context: [`design/instrument-engines.md`](design/instrument-engines.md) → §8.10.1 PARKED.
6. **Sprite flags** — `fget`/`fset` (per-sprite 8-bit flags; 256 bytes). Pairs with an
   8-checkbox row in the sprite editor. [`design/api-notes.md`](design/api-notes.md) 2026-05-30 review.
7. **Gamepad** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal `btn()` augment.
   [`design/api-notes.md`](design/api-notes.md) §15.
8. **Strudel extras / Dilla groove timing** — `pitch`, `sometimes`/`often`/`rarely`,
   `arp`, `stutter`, `palindrome`, `off_beat`; `groove` + `groove_swing/jitter/push`,
   `tick`/`PPQ`. [`design/api-notes.md`](design/api-notes.md) §13–14.
9. **Per-game polish pass** — title/game-over screens, hi-scores, sound on every event,
   juice, difficulty curves, readable HUDs. (Reframed as a reference idea bank, not an
   active backlog — see [`POLISH_TODO.md`](POLISH_TODO.md).)
10. ~~**Browser URL-sharing**~~ — **SHIPPED** (2026-06-05, eleventh pass): the whole catalog
   is playable at <https://nikkikoole.github.io/dreamengine/> (263 carts), publish is one
   command (`tools/publish-cart.sh`) or the editor's 🚀 button. The guide's "what's missing
   is a URL" is resolved — [`guides/sharing.md`](guides/sharing.md) carries the SHIPPED
   banner. *(Struck 2026-06-07 — the item lagged the eleventh-pass ship by two days.)*
11. **iPad runtime** — touch is wired in the runtime; needs a build path. [`VISION.md`](VISION.md).
    *Product lens (2026-06-07):* if the tinydaws racks become a paid product, this item is
    the cash register — iOS is where music-app buyers live. Deliberately **waits for
    evidence** (a rack holding people's attention on the free web gallery) per the
    sketch-first decision in [`design/product-notes.md`](design/product-notes.md).
12. **Sound tracker UI** — open question; depends on whether code-first sound proves
    sufficient. *Direction 2026-06-04: leaning PICO-8-style, prototyped as a CART with
    zero new engine API (see #5 + audio-notes §5.6) — the cheap way to find out if the
    editor earns a place before any engine-side bank API exists.*
    [`VISION.md`](VISION.md), [`design/audio-notes.md`](design/audio-notes.md) §5.6, §9.

13. **Baked rotation atlas** *(exploratory — full design note, not yet started)* — an
    offscreen-canvas primitive (`make_canvas`/`target`/`blit`) plus pre-rotated sprite/shape
    "stamps" so bodies are fast translate-blits instead of per-frame rasterization (for
    crowds, rich shapes, low-end). Centerline/pivot model, `pal()` recolor for free color
    variety, parts capped at 16px (native slot size). The path to scaling the `bones`
    animator past realtime drawing. [`design/baked-rotation-atlas.md`](design/baked-rotation-atlas.md).
    - *Reframe (from the Picotron manual):* Picotron collapses "sprite sheet," "offscreen
      canvas," and "raw memory" into ONE primitive — `userdata(type,w,h)`, a typed 2D buffer
      that sprites/map/screen all are. Suggests the primitive here should be **general**, not
      rotation-specific: a draw target you can render into, read/write per-pixel (`sset`/`sget`),
      and stamp — the offscreen canvas, the rotation cache, and runtime sprite editing are then
      the *same feature*, not three. Worth designing the buffer primitive first; the rotation
      atlas becomes one use of it. (Still index-only — no RGB/alpha, unlike Picotron's userdata.)
14. **Rasterization consistency** *(SHIPPED — every filled primitive on one coverage path)* —
    every filled primitive now shares one pixel-center coverage definition, so the outline is
    exactly the boundary of the fill (no rasterizer drift, dither = solid path):
    `rect`, `circ`/`oval`/`rrect` via `disc_inside`/`ellipse_inside`/`rrect_inside`;
    `tri`/`trifill`, `quadfill`, `ngon`/`star`/`poly` via even-odd `poly_inside` (concave-safe,
    winding-independent; `trifill_pat` deleted); `arc`/`arcfill`/`ring` via `sector_fill` (same
    pixel-centre disc); `thickline` via a capsule coverage (was `quadfill`+caps — the test found
    a 1px seam crack from a `w*0.5` body vs `w/2` cap mismatch). `trifill` is now CPU per-pixel —
    3D carts (`solid3d`/`cube3d`) smoke-tested OK; solid3d's face hairlines gone.
    Detector rewritten to a global invariant (outline == boundary of `fill ∪ outline`): catches
    a 1px offset at any angle, never false-flags sharp tips; verified to have teeth (GPU tris →
    282). Open strokes verified by a 4th-page equivalence self-test (ring==annulus, sector-tiling
    ==disc, thickline solid). Verified: all 11 marker states (3 pages × 4) + 3 equivalence checks
    = 0.
    **Still open (verification, not design):** ~~perf of CPU `trifill` vs old GPU~~ **measured
    (2026-06-01, `podracer`): the cost is real.** CPU `trifill` froze podracer when its haze
    spammed ~190 large software-filled tris/frame; fixed cart-side by moving bulk fills to GPU
    (`rectfill`/`line`). **Off-screen bbox clamp — SHIPPED (2026-06-02).** `poly_fill_cov`/
    `poly_stroke_cov` now intersect their scan box with the on-screen region before scanning,
    mapped through the camera (`GetScreenToWorld2D` on the four screen corners → conservative
    AABB, so translate/zoom/rotation are all correct and the image is byte-identical). Huge
    off-screen tris no longer iterate their full bbox doing point-in-poly tests on cells that
    plot nothing. Verified output-identical: `raster_test` reports `mismatches:"0"` on all 46
    analyse frames + `eq total=0`.
    **It's a performance-cliff guard, not a general speedup** — the cost of a software polygon
    now scales with its *visible* area, not its *total* area. The effect tracks how far a
    poly spills off-screen, so it ranges from huge to nil (all measured with the profiler):
    - `trifill_stress` (synthetic: 12 thin spokes reaching ~1100px off-screen) — **46.7 →
      2.7ms/frame (~17×), ~21fps → 60fps.** This is the worst-case win, not a typical one.
    - `solid3d` (real 3D, model fits the screen so faces only spill a little) — 3.18 → 3.02ms
      avg (~5%), 4.6 → 3.9ms **peak (~15%)**. Modest; the gain is on the frames a face is
      largest.
    - `podracer` — **no effect** (0.81 → 0.75ms, noise): it draws zero software polys (haze
      already on GPU `tritex`/`line`/`rectfill`), so there's nothing on this path to clamp.
    Takeaway: existing well-behaved carts don't get visibly faster; the value is that a future
    cart flying the camera into a `quadfill`/`trifill` surface degrades gracefully instead of
    freezing (the cliff `podracer`'s author had to hand-work-around). *Per-call overhead* is 4
    `GetScreenToWorld2D` (a matrix inverse) — negligible at observed call counts (solid3d got
    faster, not slower); if a cart ever draws thousands of tiny on-screen polys/frame, cache
    the clamp box once per frame (invalidate in `camera()`/`camera_ex()`) instead of per-call.
    Still open: web GL ES confirmation (`pget` disabled on web); an ADR for the GPU→CPU
    `tri`/`trifill` + `thickline` behaviour change.
    **Regression test:** `tools/carts/raster_test.c` + `tools/raster_test.script` —
    drag `editor/public/carts/raster_test.cart.png` into the editor (Z outline, X dither,
    C cycle 4 pages, SPACE analyse / run equiv), or run headless:
    `node tools/play.js raster_test script tools/raster_test.script --headless --trace build/raster_trace.jsonl --frames 60`
    then check every `fs=2` frame reports `mismatches:"0"` and the `eq` line shows `total=0`.
    **Perf-regression test** (the bbox clamp): `tools/carts/trifill_stress.c` — a pinwheel of
    thin off-screen tris. In the editor it should hold 60fps even with reach cranked to max; if
    the clamp regresses, pushing reach tanks the fps. It runs `raster_test` for correctness and
    the ⏱ profiler for the budget (was ~46.7ms unclamped, ~2.7ms clamped at the defaults).
    [`design/rasterization-consistency.md`](design/rasterization-consistency.md).
15. ~~**Tiny fonts**~~ — **SHIPPED** as `font(FONT_SMALL/FONT_TINY)`. See Shipped above.
16. **Packaging & public distribution** *(not started)* — dreamengine only runs as a dev
    build today. A dev-only icon + app name stopgap landed this session (the running app was
    a generic "Electron"); real packaging (bundler, `.icns`, code-signing/notarization, load
    the built renderer instead of `localhost:5173`) is unaddressed. The actual blocker isn't
    Electron — it's that the ▶ run model shells out to a developer's `clang` + Homebrew raylib,
    which a consumer machine doesn't have; web/wasm is the likely public path. Full breakdown:
    [`design/packaging-distribution.md`](design/packaging-distribution.md). Related: browser
    URL-sharing (item 10), iPad runtime (item 11).

17. **Frame-spanning sequence scripts** *(idea — from the Picotron API comparison; needs design)* —
    the *useful half* of Lua's `yield`/coroutines: write time-based logic (cutscenes, scripted
    AI, juice sequences, dialog) as **linear top-to-bottom code** — `walk_to(100); wait(30);
    say("hi"); wait(60); …` — instead of a `switch(state)` shredded across `update()` calls.
    C has no native coroutines, but the pattern is emulable with **protothreads** (Dunkels'
    switch/case macro): stackless, so locals that must survive a `wait` go in `de_state()`.
    **Distinct from the cut "process / coroutine model"** below — that was a full DIV-style
    Level-2 *execution model* (every entity its own process); this is a *small optional helper*
    for sequencing, not a new way to structure carts. Open question is whether a macro trick fits
    dreamengine's "readable C" ethos, or whether it's better shipped as a documented example cart
    than as core API. Worth prototyping one `sequence`/`wait` helper to feel the ergonomics.

18. **Blend tables** *(idea — from the Picotron manual; the substantive capability gap)* —
    index-only translucency/fog/tinting/additive via a precomputed lookup `result = blend[src][dst]`,
    staying entirely in the 32-color palette (**no RGB, no real alpha** — just a table that says
    "drawing color `a` over existing color `b` resolves to `c`"). Unlocks the things carts fake
    with `fillp` dither today: translucent water/glass, fog, additive glows, drop shadows. This is
    a real *capability* dreamengine lacks — `pal()` swaps and `fillp` are the closest, neither
    blends with what's already on screen. Deliberately framed as a lookup table so it does **not**
    trip the "splits the color model" concern flagged on the `lerp_color`/`rgb` parked thought
    (item 2) — the output is always a palette index. Picotron pairs this with stencil clipping;
    that's a separate, lower-value follow-on. **Design note now exists →
    [`design/blend-tables.md`](design/blend-tables.md)**, and the concept is **validated in
    cart-space**: the `blend lab` tech-demo (`tools/carts/blendlab.c`, 2026-06-04) builds
    AVG/ADD/MUL tables and blends per-pixel against a cart-owned scene fn, zero engine API.
    Verdict: the look works (additive glow / glass / fog all read correctly, in-palette), and
    the engine crux is identified — dst must be read from the *in-progress* frame (a `pget`
    last-frame read feeds back and blooms; demonstrated by the cart's `P` mode). Candidate
    implementation: shader + per-scope canvas snapshot, the decision-0007 lane. Next step: ADR —
    **after the palette decision**: blend tables are computed *from* the palette, and the default
    palette (lifted verbatim from PICO-8) should become our own / settable first, or #18 bakes the
    borrowed palette one layer deeper. See [`design/palette-and-color.md`](design/palette-and-color.md)
    (Picotron findings + three-layer plan: new default → `palette_set` + `de:palette` chunk →
    tables-from-active-palette).

19. **Per-cell parameter locks in the cr-78 cart** *(cart-space idea, zero engine API — parked
    2026-06-05)* — Elektron-style p-locks for `tools/carts/cr78.c`: each lit step optionally
    carries a pitch offset (melodic bongos/congas/metal riffs, TR-727 style) and a cutoff
    override (hats opening across the bar). Historically cheeky on purpose: CR-78 voices (1978)
    + the cart's swing knob (LM-1, 1980) + p-locks (Machinedrum, 2001) in one box. Pitch is
    trivial (`fire()` already passes midi). **The known crux is the filter**: one-shot cutoff
    lives on the instrument slot and scheduled notes snapshot their slot at fire time
    ([`design/audio-notes.md`](design/audio-notes.md) §2.2) — per-step cutoff therefore needs
    the sfx-editor **rotating scratch-slot pattern** (cr78 uses slots 9–24; 25–31 free, pool of
    2-3 suffices at one-step lookahead). UI sketch in the cart header: hover+wheel = pitch,
    C+wheel = cutoff, notch markers on the 9×7px cells. Full design notes at the top of
    `tools/carts/cr78.c`.

20. ~~**TB-303 bassline cart**~~ — **SHIPPED same-day 2026-06-05** as `tools/carts/tb303.c` /
    `tb303.cart.png` ("parked for another time" lasted about an hour — user asked for it).
    Exactly as sketched: one `note_on()` voice, `note_glide(60)` slides that don't refire the
    filter envelope (authentic to the circuit), accent = vol 7 + env amount × ACC knob,
    staccato gate at 70% of step, five mouse-draggable knobs with CUT/RES applied live to the
    ringing voice (`note_cutoff`/`note_res`), saw/square switch, mouse piano roll with
    OCT/ACC/SLD flag rows, and an N-key random acid-line generator (root-heavy minor
    pentatonic). The classic-machine family is now cr78 + tr808 + tb303.

21. **More classic boxes — the museum shortlist** *(cart-space, zero engine API — parked
    2026-06-05; the family so far is cr78 + tr808 + tb303 + sh101 + tr909, all in
    `tools/carts/`)*. Curated by
    API fit; the curatorial line is **analog-circuit machines only** — sample/tape boxes
    (LinnDrum, DMX, SP-1200, Mellotron) would be caricatures since the engine has no sample
    playback. Ranked:
    - **TR-606 Drumatix (1981)** — first pick: the TB-303's actual companion (sold as a silver
      pair, sync'd together); all-analog, same six-oscillator metal-bank trick as the 808, both
      cart templates already exist. An afternoon.
    - ~~**TR-909 (1983)**~~ — **SHIPPED 2026-06-05** as `tools/carts/tr909.c` / `tr909.cart.png`.
      Analog kick/snare/toms/rim/clap as assessed (house kick = +30st/35ms swept sine + a click
      layer on the ATTACK knob). The ROM-sample hats/cymbals got a BETTER workaround than the
      predicted SFX-editor one: **`INSTR_FM` on the 3.5 inharmonic clang detent** (harmonics
      0.55, feedback cranked) through a highpass whose cutoff starts ~5kHz low and rises via a
      **negative `ENV_CUTOFF` amount** — the fast-closing sizzle of a sampled hat, synthesized.
      (Same-day instrument-engines §8.8 insight applied: inharmonicity reads as metal; the engine clamps cutoff
      after env addition, so negative amounts are safe.) Swing-knob saga complete: the 909
      shuffle is period-correct at last (even 16ths drag — audio-notes §14 still carries the
      pre-build assessment). Six presets (Good Life, The Bells, Energy Flash, Hardfloor,
      Revolution 909, Gabber); closed hat chokes open via `instrument_choke`. **The
      classic-machine family (cr78 + tr808 + tb303 + sh101 + tr909) now covers the full
      ReBirth RB-338 rack** (303 + 808 + 909). Same-day follow-up: **FLAM shipped** after
      all ("the one panel feature not modeled" lasted one message) — and grew into a
      **stroke cycle**: right-click cycles plain → flam (1 grace, 22ms early) → drag
      (2 graces) → ratchet (4 even hits chopped across the step — post-909, but the
      techno fill); `'f'`/`'d'`/`'r'` in preset rows, cells draw their strokes as ticks,
      Hardfloor flams its claps + ends the hat row on a ratchet, Gabber drags its snare. Plus a deliberate impurity by ear-feedback: a
      **metal-filter XY pad** (cut × resonance, `instrument_filter` re-aimed live across
      all five FM/noise metal slots) because the FM hats landed bright and hissy —
      defaults nudged fuller than the first build. Sound tripwire: PASS (700 headless
      frames on Hardfloor incl. flams + pad clicks, no drops).
    - **EKO ComputeRhythm (1972)** — Jarre's punch-card programmable box; UI gimmick = draw the
      punch card. Pre-dates the CR-78's "first programmable" claim by six years.
    - **Wurlitzer Sideman (1959)** — the FIRST rhythm machine: tubes + rotating contact wheel;
      UI = a circular playhead instead of left-to-right. Oldest piece by two decades.
    - **Casio VL-1 (1979)** — "Da Da Da"; calculator keys + the 8-digit ADSR number code you
      literally type to design a sound.
    - **Maestro Rhythm King (1970)** — Sly Stone's funk preset box; simpler/weirder than CR-78.
    - **Stylophone (1968)** — mouse-as-stylus, ~20 lines, instant Bowie.

    **Pre-Roland wing — Raymond Scott** (Manhattan Research Inc., the basement where all of
    this started; Bob Moog sold him circuits in the '50s):
    - **Circle Machine (~1959)** — strongest candidate of the whole list: a RING of bulbs, each
      with a brightness knob, swept by a rotating photocell arm — a step sequencer a decade
      before the word. Cart: circular sequencer, drag bulb brightness (= pitch/volume per
      step), rotating playhead instead of left-to-right, rotation speed = tempo. `euclid()`
      would feel period-appropriate. Visually unlike every other music cart in the studio.
    - **Clavivox (1956)** — keyboard theremin with portamento; the great-grandfather of the
      tb303 cart's `note_glide`. Could be a small mouse-played instrument cart.
    - **Electronium (1959-70s)** — not an instrument, a collaborative composition machine you
      NUDGE ("faster", "more like that"); Motown bought one and hired Scott as electronic R&D
      director. Already has descendants here: the bossa/ambient/jangle radio carts. A cart
      that adds the nudge-interface to a generative engine would close the circle.

> `tritex` (affine textured triangle) shipped in session 8 — it was Open here; now in the API.

**Smaller open items (no design doc yet):** looping ambience (`drone`)/`volume`/mute. Noted
in [`POLISH_TODO.md`](POLISH_TODO.md).

**Noise is value-noise + the seeding idiom** *(new 2026-06-09, surfaced building the
`procplaces` procgen testbed)*. The engine's `noise`/`noise2`/`noise3` (`studio.c:2911`) is
**value noise, not Perlin** — it hashes lattice *values* and smoothstep-interpolates them
(Perlin interpolates *gradients*), so it has faint axis-aligned grid artifacts at low
frequency. Fine for terrain/density/fog; worth knowing before leaning on it for anything
needing isotropy. Crucially it **is seedable today**, even though `noise`/`noise2` take no
seed: `noise2(x,y)` == `noise3(x,y,0)`, so **`noise3(x, y, (float)seed)` with an INTEGER seed
is a fully-independent, repeatable field** (the z-slice folds through `noise_hash`
independently; an integer z = no interpolation bleed). The `procplaces` cart uses exactly
this (both its generators) and verifies byte-identical re-renders. **Open question:** add a named seeded
helper (`noise2_seeded(x,y,seed)`?) and/or document the `noise3`-as-seed idiom + the
value-vs-Perlin caveat in `studioDocs.js`, so the next author doesn't conclude "can't seed it."

**`sprite-draw.js` next steps:**
- ~~Gap audit~~ **DONE** — `ovalfill`, `rrectfill`, `ngonfill`, `polyfill`, `noise` added (2026-06-04).
- **Remaining Tier 2** — `starfill`, `arcfill`, `thickline`, `vgradient`/`hgradient`, `bezier`. Lower priority; current set covers most sprites.
- **JS-only extras** — `hflip`/`vflip`, `rotate90` (reuse one sprite in 4 orientations). Also: migrate terrain-tile carts (cannonfodder, druglord, heroes, hotline, etc.) to use `noise()` instead of their copy-pasted `(x*a + y*b) % m` patterns.
- **Stress-test cart** — a cart exercising all primitives as a visual reference + regression guard.

23. **The sprite story — two sprite sources of truth** *(new 2026-06-06, surfaced by the
    editor publish button but it already bites without it)*. A cart's sprites can come from
    (a) the **sprite editor's canvas** (exported as `build/sprites.png` on every run — what
    you see is what ships) or (b) a **`.cart.js` generator** (ASCII art / sprite-draw.js
    programs, rebuilt by `make-cart.js`/`build-site.js` at bake time). They do not know
    about each other: repaint a generator-cart's sprites in the editor and your pixels ship
    in that build but the generator still owns the repo truth — the next CLI bake silently
    reverts them. Same applies to plain sprite touch-ups: there is no path from editor
    pixels back to `tools/carts/<name>.cart.js`. Options to explore: a pixels→`.cart.js`
    exporter (slot arrays, lossless), an explicit per-cart marker for which source owns
    sprites, or a publish-time conflict warning (the editor's publish log already warns
    when a `.cart.js` exists). Until then: generator carts should get sprite changes in the
    generator, hand-drawn carts in the editor — never both.
22. **Mobile-web readiness** *(new 2026-06-05, born from the live gallery + first
    real-device session)* — desktop-authored carts meet phones now. Shipped from
    the worklist (both 2026-06-05): ~~`tools/mobile-lint.js`~~ static checker
    (verdicts rank by *best* phone-usable input path; first `--site` run over 50
    carts: 3 touch-ready, 34 tap-as-mouse, 5 fixable, 2 keyboard-only, 6 no-input)
    and ~~gallery input badges~~ (`build-site.js` requires `lint()` and stamps a
    colored chip per card — 🟢 Mobile Ready / 🟡 Mostly Playable / 🟠 Rough on
    Mobile / 🔴 Desktop Only; strict: any dead input path demotes, hover/wheel
    cores rank rough, and a hand-tested `"mobile":` field in index.json
    overrides the lint; `fixable` shows as desktop-only until touchControls
    lands). Still
    open: a per-cart `fit` setting (letterbox / stretch / integer-scale) flowing
    `.cart.js` → `de:settings` → shell (radios are the first `"stretch"`
    customers), the **device-facts trio** (`touch_available()` grown into
    `touch_available`/`device_class`/`touch_ceiling` — researched 2026-06-06
    incl. the iPad-pretends-to-be-a-Mac detection traps and the
    ceiling-safeguard pattern against the iPhone 6th-finger mass-cancel;
    **`touch_ceiling()` SHIPPED same day** — shell computes `Module.deTouchCeiling`,
    EM_JS read, 4-place wiring, touchpiano header prints "max N fingers";
    `touch_available`/`device_class` still open, design in mobile-web-notes §5),
    and the formal touchlab-on-iPhone
    checklist run (>5-touch assumptions — iPhone Safari caps at ~5 simultaneous
    touches, found on-device; tiny tap targets). Full design + device findings:
    [`design/mobile-web-notes.md`](design/mobile-web-notes.md).

24. ~~**Web phantom touch point**~~ — **CLOSED 2026-06-06, same day as filed**: root-caused,
    fix BUILT & DEVICE-PASSED (touchlab/multitouch/touchpiano via the live gallery), and the
    rebuild tail cleared by the whole-catalog publish (all 263 carts carry the fixed engine).
    **Same-day sequel (iPad play session): tap-as-mouse death** — taps stop registering in
    mouse-driven carts until reload; emscripten GLFW's `primaryTouchId` latch sticks when iOS
    drops a `touchcancel` (WebKit 153064). Fixed with the same medicine: on web, once a real
    touch is seen the mouse is synthesized from the touch mirror (`web_tm_*` in `studio.c`),
    GLFW's emulated mouse never read again. Touch-notes **device finding #6**.
    Spawned **open item 27** (web debug overlay) — three on-device mysteries in one iPad
    afternoon with zero cable-free visibility.
    Kept for the record:
    on web builds a lifted finger's contact can stay in the touch list (most reliably when
    two fingers release at once); native is clean. Cause: emscripten#4679 (`wontfix`,
    touchend conflates remaining+lifted touches) stacked on raylib's
    one-removal-per-touchend (5.5 *and* master). Fix: own the touch truth on web — a JS
    mirror rebuilt from spec-correct `event.touches` in `web_shell.html`, read by
    `poll_virtual_touches()` via EM_JS; rebuild-don't-decrement is immune by construction.
    Acceptance: touchlab two-finger simultaneous-release drumming → `touch_count()` hits 0
    every time. Then the gestures.h follow-ups (two-finger **pan vs pinch** classifier +
    id-keyed pinch pair — also why rgestures reads any 2-finger drag as pinch/zoom).
    Root-cause chain, issue links, full plan: [`design/touch-notes.md`](design/touch-notes.md)
    **§7–8**. Blocks the capture model of item 25 on web.

25. **`ui.h` — cross-input widget kit** — **v1 SHIPPED 2026-06-07** *(designed 2026-06-06;
    the item-24 web blocker closed in between)*. `runtime/ui.h` (gestures.h pattern, zero
    engine API): **button / slider / knob** for mouse + touch + keyboard/gamepad at once —
    per-contact capture table (two fingers = two knobs), value-address identity (buttons:
    rect-only — sfxgen's dynamic `str()` label broke pointer identity), hit-pad inflation
    to mobile-lint's ≥24px with **deferred press resolution** (presses collected in
    `ui_begin`, routed at `ui_end`; a visual-rect hit beats an inflated one, so 17 sliders
    at 9px pitch still route correctly), opt-in marching **focus ring** (arrows traverse,
    LEFT/RIGHT adjust with hold-accel, A activates — written on `btn()` so it inherits
    gamepad the day item 7 lands), and `ui_grabbed`/`ui_released` events timed so a cart
    can snapshot undo state before the press-jump lands. Shipped per the second-customer
    rule: **`uikit`** showcase/probe cart (knobs play a synth voice, sliders drive a ball
    pit, FOCUS toggles keyboard driving) + the **`sfxgen` retrofit** (17 sliders + 11
    buttons → widgets, hand-rolled drag machine deleted; behavior-faithful incl. undo —
    scripted-replay verified). mobile-lint now **inlines runtime/ library headers** before
    scanning, so all-ui.h carts rank touch-ready by construction (the notes' §5.4
    contract). **Cut from v1: panel + drag-from** — the per-widget second-customer rule
    found their named customers speculative; they wait for a real cart that wants them.
    **Still open:** the on-device probe run (two-knobs-at-once, fat fingers, 5-touch
    ceiling) and further retrofits (modrack knob rows, sfxed, wave editor). Design + §7
    build learnings: [`design/ui-widgets-notes.md`](design/ui-widgets-notes.md).

27. **Web debug overlay** *(designed 2026-06-06; **v1 SHIPPED 2026-06-07**)* — cable-free
    on-device visibility for wasm carts: `?debug=1` or **triple-tap the top-left corner**
    overlays live touch rings straight from `Module.deTouches` (a ring that stays after
    lifting = phantom; rings the game ignores = bug is past the touch layer), a
    printh/console mirror, `window.onerror` red lines, fps + the device's touch ceiling.
    Built per the §6d architecture rule (shell tweaks cost a 263-cart rebuild — learned
    twice on 2026-06-06): the shell bakes only a ~25-line loader; ALL overlay logic lives
    in one site-root `debug-overlay.js` (source `runtime/debug-overlay.js`, copied by
    `build-site.js`) — future overlay iteration is a one-file republish, zero rebuilds.
    **Still open (v2):** the cart's `watch()` values pushed per frame via EM_JS so the
    native watch-workflow works on a phone; the `web_tm_*` mouse-synth state readout.
    Zero-code alternative for deep dives: iPad + cable + Mac Safari remote Web Inspector.
    Design: [`design/mobile-web-notes.md`](design/mobile-web-notes.md) §6d.

26. **Editor hand-editing workflow** *(new 2026-06-06 — explored, sliced)* — three gaps when
    a human edits carts in the editor instead of via `tools/carts/` + CLI: (a) **no
    save-in-place** — every save is a Save-As dialog (`currentCartFile` only feeds the
    publish slug); fix = track the loaded cart's origin path, Cmd-S writes back (gallery
    carts excluded — shared registry + build products), keep the existing thumbnail. (b) the
    **sprite story** = item 23; lean recorded: ownership marker (`spriteSource:
    "editor"|"generator"`) now, lossless pixels→`.cart.js` exporter for hand-drawn carts
    later. (c) **gallery metadata**: descriptions CSS-clamped to 3 lines with no way to read
    the rest, and no UI to author `index.json` entries — fix slices: full-description detail
    view (no hazard), then a metadata form that emits a paste-ready index.json entry
    (registration deliberately stays a CLI act — the shared-registry rule holds); the
    self-describing `de:meta` chunk + generated index is parked as a direction. Proposals +
    priority table: [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md).

28. **Library headers hard to find inside the editor** *(new 2026-06-07, surfaced by the
    ui.h ship)* — **slice (a) SHIPPED same day, and the move it implied got made too.**
    The fix turned out cleaner than the original sketch: rather than bolt a "library
    headers" list onto the API reference, the **read-only engine-source viewer moved out
    of its code-tab overlay and into the docs tab** as a dedicated **"engine source"
    sidebar group** (`studio.h` · `ui.h` · `gestures.h` · `improv.h` · `radio.h` ·
    `sound.h` · `studio.c`) — one viewer, two entry points: browse the group, or
    cmd-click an `#include "x.h"` in your cart and it switches to the docs tab and opens
    the same view. This **deleted** the code-tab overlay (`#viewer-overlay` HTML/CSS, the
    ✕/Esc close path, and `outline.js`'s `setOutlineOverride` prototype-scan machinery —
    the cart outline no longer has to yield to a previewed file). `editor/src/navigate.js`
    rewritten to a `showEngineFileIn(container, file)` mounter the docs tab calls; the
    code tab is now always, only your cart. (Reasoning for the move: the headers
    *belong* with the docs, not floating over your code; the docs tab already had the
    sidebar + chrome, so the viewer cost ~5 lines of new wiring and removed an overlay.)
    **Still open:** (b) autocomplete offering `ui.h`/`gestures.h`/… inside an
    `#include "` quote; (c) *maybe* the starter cart mentioning the lane in a comment.
    Function-level autocomplete/hover for header symbols stays deliberately out — keyed
    to `studioDocs.js` and the four-places contract, which is engine-API surface only;
    the header's top-comment manual is the doc, by the lane's contract.
    *(Editor change — needs a dev-server restart + a human visual pass to confirm the
    viewer renders and the day/night theme follows; built and bundle-verified, not yet
    eyeballed live.)*

29. **Sub-pixel camera — thin features shimmer when panning at fractional zoom** *(new
    2026-06-09, surfaced by procplaces' zoomable world)* — `camera_ex(int x, int y, float
    zoom, float angle)` takes **integer** world coordinates. At a fractional zoom (e.g.
    0.55×) a smoothly-moving camera can only snap to whole world units, and 1px features
    (road curbs, dashes, grid lines) land on different fractional screen pixels each frame
    as the world scrolls — so they crawl/shimmer while panning. Static, they're fine. Same
    *class* of problem as sloop's bright-curb jitter (commit fe5553f), but that was a
    velocity-lead wobble (fixed cart-side by low-passing the lead); this is the deeper
    integer-camera limitation underneath. **Cart-side workaround in place** (procplaces):
    suppress thin markings below ~0.55× zoom — wide solid fills are pixel-stable, and the
    detail is invisible zoomed out anyway. **A real fix is engine-level:** give the camera
    a sub-pixel float position and snap world→screen to the zoom pixel grid (so `zoom *
    cam` stays integer), or offer a `camera_exf(float x, float y, …)`. Until then any
    cart drawing thin lines in a zoomable/pannable world will shimmer at non-integer zoom.
    Probably belongs with the rasterization work — [`design/rasterization-consistency.md`](design/rasterization-consistency.md).

30. **Docs/context hygiene — shrink what every agent loads** *(new 2026-06-10, surfaced
    while the per-agent context cost started "being felt in more agents")*. The docs are
    tight and high-value; the problem is *load shape*, not volume. Diagnosis + the one move
    already taken:
    - ✅ **DONE — CLAUDE.md is now a router, not a reference.** The 197-line "Game feel"
      essay (~31% of the file) was always-loaded into *every* agent every session despite
      being task-specific. Extracted to [`guides/game-feel.md`](guides/game-feel.md), left a
      ~10-line pointer. CLAUDE.md 628 → 443 lines. **Next candidate:** the live-inspection
      recipe in CLAUDE.md's "Debugging carts" section largely duplicates
      [`guides/debug-harness.md`](guides/debug-harness.md) — could become a pointer too.
    - **Add a `stereo.md`-style "STATE + next bite" header to the big design docs.**
      `instrument-engines.md` (1,455 lines) and `audio-notes.md` (1,238) force a large read
      to orient; a 20–30 line top block (current state, ✅ shipped, next concrete step) lets
      an agent orient cheaply — the pattern that let stereo.md convey the whole stereo+effects
      gate in one small read. Split past ~1,500 lines, keeping stable §-anchors (lint-docs
      already guards refs).
    - **Consolidate layered corrections.** ✅ **DONE for the effects decision (2026-06-10).**
      It was the worst offender — 4 reads for 1 truth (0015 + its appended Correction +
      `instrument-engines.md` §8.10 + its banner). Folded the wah correction into 0015's "Why"
      lead (now correct in place; Correction shrunk to a dated tail) and trimmed the §8.10
      banner's redundant preamble. **General pattern stays open** but explicitly *not a project*:
      fold corrections into the primary text + push superseded rationale to a clearly-marked
      tail, opportunistically, never as a 154-file sweep. Append-only is honest but taxes every
      future reader — fix a doc's lead when it's outright *wrong*, not just append-decorated.
    - **Keep favoring queryable state over prose** — `cart-status.js` / `lint-docs.js` /
      `lint-carts.js` let an agent *ask* "what's stale / do refs resolve" instead of reading
      everything. That's the right direction; more of it beats more prose.

31. **Engine tuning — some modeled engines play out of tune** *(new 2026-06-10, found by
    the new `tune-check.js`)*. Run `node tools/tune-check.js` for the live per-engine cents
    report (SINE is the 0¢ control); full first-run audit + the *why* in
    [`design/audio-notes.md`](design/audio-notes.md) §18. **The to-do list, worst first:**
    - ~~**`INSTR_PIPE` (flute) — the bad one.**~~ **MOSTLY FIXED 2026-06-11.** Was an octave low
      and progressively flat (A2 −13¢ → A5 −159¢): the bore was sized a full wavelength but the
      inverting open-end reflection resonates at SR/(2·delay), so it rang an octave down, and the
      uncompensated jet+filter loop delay added the flatness. Fix in `sound_pipe_start`: half-
      wavelength bore minus a (mildly pitch-sloped) loop-delay term, sized with the bowed-string
      fractional-read trick to kill integer quantization. **Now: A2–A4 within ~±2¢** (the core
      range), octave correct everywhere. **Residual: A5 (880 Hz) ≈ −65¢** — at a ~20-sample bore
      the fixed-length jet ≈ the bore, so the voice sits on the overblow edge and pitch is
      hyper-sensitive (a 0.3-sample bore change swings it ~140¢ or flips an octave); a loop-delay
      trim can't reach 0¢ there. Closing it needs a **jet-length re-voicing** (jetLen proportional
      to the bore so the jet:bore ratio is pitch-invariant) — a timbre change, hence still open.
    - **`INSTR_PLUCK` / `INSTR_REED` / `INSTR_BRASS` — flatten at the top** (A5 −17 to −25¢),
      ~0¢ low down. Classic integer-sample delay-length quantization in the waveguides; the
      fix is a fractional (interpolated) delay read tap, or a per-note tuning correction.
    - **In tune, no action:** SINE/MALLET/EPIANO/PD/PIANO/GUITAR/FM, and **BOWED** (≤ +3¢ —
      whatever's off about the bowed voice, it is *not* pitch). **`INSTR_ORGAN`** reads an
      octave low but is in tune (+3–7¢) — that's the 16′ sub-octave drawbar, expected.

---

## Decided-against / deferred ✗

These were considered and **cut** — kept here so the decision isn't relitigated.
Rationale lives in [`design/api-notes.md`](design/api-notes.md)'s "What to defer or skip" and the 2026-05-30 review.

- **Process / coroutine model (DIV-style)** — the would-be "Level-2" learning model.
  Every shipped cart works cleanly with plain typed static pools, so it's weeks
  of coroutine/transformer machinery for a model we don't need. [`VISION.md`](VISION.md).
- **Engine-owned entity system** (God-struct / `SELF` / `val[16]` / ECS) — per-game
  typed pools with *named* fields beat all of them for a learn-C console.
- **MMF movement/qualifier engines** (`move_platform`, `move_8dir`) — removes the lesson.
- **`move_and_collide`** (resolved tile push-out) — low demand; only `platform.c` does
  the full pattern, and `zelda`/`gta` test against their own data, not `mget`.
- **DS structures** (lists/maps/grids), **memory arenas**, **PS1 z-sort/ordering table**,
  **tools-as-carts / VFS / fantasy-OS / peek-poke**, a **3D *engine*** (scene graph / mat4
  stack / z-buffer / per-pixel depth) — out of scope. *(The small 3D leaf-helpers
  `rot3`/`project3`/`zsort`/`quadfill` + `V3` ARE shipped — see below and [decision 0009].)*
- **Particle systems** and **pathfinding** — ship as *library carts* (seeds: `astar.c`,
  `boids.c`), not engine surface.
- **Pixel-perfect sprite collision** — eventually maybe; AABB covers 95% first.
- **Turtle graphics API** (`turtle_*`/`pen_*`) — shipped, then **removed 2026-06-01**: only
  `16-spirograph.c` used it, and a turtle is just `dx`/`dy` + `line()`, so it lives in the
  cart now. [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
