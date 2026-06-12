# Build brief — the `filter()` showcase cart (DJ filter)

A handoff spec for a fresh context. The `filter()` master resonant filter shipped 2026-06-12
(`FX_FILTER`, effects-bus insert — see STATUS / audio-notes §17 item 15 / effects-recipes
"filter" section). Per the project flywheel, every effect gets one cart whose whole point is
*that effect as the instrument* (echo→`spacecho`, reverb→`cathedral`, flanger→`mistress`,
wah→`clavinet`, tape→`tapeloop`). `filter()`'s showcase is the one piece still pending. This
is it.

## The concept

**A DJ one-knob filter over a looping groove.** The cart *is* the filter — riding the knob is
the entire gameplay, exactly like `spacecho` is "ride the echo." Homage: the legendary DJ-mixer
filter (Allen & Heath **Xone:92**, Pioneer **DJM**) — and the screaming-resonance end nods to a
**Sherman Filterbank**. Don't overbuild the music; the loop is a *canvas* for the sweep.

## THE key idea — the bipolar one-knob filter

One big FILTER knob, **center = open (bypass)**, and it picks the mode for you:
- **center** → `FILTER_OFF` (flat, full signal)
- **turn left (CCW)** → `FILTER_LOW`, cutoff sweeping ~18 kHz → ~150 Hz (muffled thump)
- **turn right (CW)** → `FILTER_HIGH`, cutoff sweeping ~20 Hz → ~6 kHz (thin/telephone)

That bipolar map *is* the DJ filter, and it's the most satisfying single control in dance music.
Map sketch (ride it from the knob each frame; `filter()` is cheap to re-call — only call when the
value actually moved, per the set-and-hold note):

```c
// k = the big knob 0..1 (0.5 = open); res = the resonance knob 0..1
if (fabsf(k - 0.5f) < 0.02f)      filter(FILTER_OFF, 0.0f, 0.0f);                 // open
else if (k < 0.5f) { float t = (0.5f - k) * 2.0f;                                // LP closing down
    filter(FILTER_LOW,  18000.0f * powf(150.0f/18000.0f, t), res); }
else               { float t = (k - 0.5f) * 2.0f;                                // HP opening up
    filter(FILTER_HIGH,    20.0f * powf(6000.0f/20.0f,    t), res); }
```

Second knob: **RESONANCE** 0..1 (crank it for the acid scream on the build). The mode is implicit
in the bipolar knob — no separate mode control needed (but a small LP/HP/BAND mode chip is a fine
extra if it earns its space).

## The performance — breakdown → build → drop

A **BUILD** button (or auto) is the money-shot: over N bars it ramps the filter from open → closed
(or closed → open) with **rising resonance**, then on the "drop" snaps back to open + full kit.
That breakdown→build→drop IS the genre's signature gesture and the reason the filter exists. Make
this feel great — it's the demo's climax.

## The bed (keep it simple + BRIGHT)

The filter needs a **harmonically rich, full-spectrum** loop to chew on, or the sweep is boring.
A four-on-the-floor house/techno groove with a **bright detuned-saw chord/arp** + a busy hat is
ideal. **Reuse, don't compose:** crib the drum recipes + the saw pad/arp straight from
`groovebox.c` (already melodic-house voiced) — the band is NOT the point, the sweep is. Mono-ish
is fine. A minor vamp (Am–F–C–G is already in groovebox) is plenty.

> **Firewall / 0015:** this is a sound cart, but the **filter is the instrument, not a band** —
> so skip the blind-band ceremony and put the design energy into the KNOB FEEL, the VISUAL, and
> the build-drop. The voices are a deliberately plain, bright canvas.

## Visual (game-feel — tie the effect to a sight)

- A **filter-response curve** drawn live: the LP/HP slope + the resonant peak rising as resonance
  climbs, the cutoff marker sliding with the knob. This is the centerpiece visual.
- The groove visibly **muffling**: a spectrum/brightness bar (or the existing groovebox-style
  meter) that closes down as the filter closes — "see it get dark."
- A big knob graphic for the FILTER (the star), a smaller RESONANCE knob.

## Controls

- **FILTER** big knob — mouse/touch drag (the star); arrows nudge it on keyboard.
- **RESONANCE** knob.
- **BUILD** button — the auto breakdown→build→drop sweep.
- **BYPASS / A-B** — hear it off vs on (every effect showcase has this; cf. `clavinet` WAH switch).
- It self-plays the loop; both hands free to ride the filter (the groovebox model).

## API + chassis to reuse

- **Effect:** `filter(int mode, float cutoff_hz, float resonance)` — `FILTER_LOW`/`HIGH`/`BAND`/
  `NOTCH`/`FILTER_OFF`. Cheap to sweep live (it just stores 3 values — unlike the buffer effects);
  ride it every frame, but gate the call on the value actually changing (set-and-hold note in
  effects-recipes.md intro).
- **Widgets:** `ui.h` (`ui_knob`/`ui_button`) — never hand-roll the drag machine.
- **Clock/loop:** `bpm`/`beat`/`beat_pos` + `hit`/`note_on` (the `drummachine`/`groovebox` pattern).
- **Chassis to copy:** `groovebox.c` (loop + `ui.h` rack + held pad — closest relative, lift its
  voices), and `spacecho.c` / `clavinet.c` (the "effect-as-instrument + footswitch A/B + ride-a-knob"
  showcase shape).

## Ship pipeline (state these steps in the output)

1. `tools/carts/<name>.c` (+ optional `<name>.cart.js` — likely settings-only; it draws with
   primitives). Name idea: `djfilter` / `xone` / `filtersweep` (pick one).
2. `node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png`
3. `node tools/make-cart.js --run editor/public/carts/<name>.cart.png` (bake screenshot)
4. Register in `editor/public/carts/index.json` — `"kind": ["instrument","tech-demo"]`, vivid
   description + controls. `node tools/lint-carts.js`.
5. **Verify it works:** render `--wav` and confirm the sweep audibly transforms the loop (a closed
   LP render is much darker/quieter than open — cf. how the engine was verified: LP-200 on noise
   dropped RMS −20→−40 dBFS).

## On ship — keep the docs current (the rule)

- Add a row to [`instrument-carts.md`](../guides/instrument-carts.md) (classic-machine homage /
  effect showcase table).
- In [`effects-recipes.md`](../guides/effects-recipes.md): add the cart to the **filter** section's
  recipe "used by" column, and add it to the showcase index line (`… · <cart> (filter)`).
- Flip the "Showcase pending" notes: effects-recipes filter section, audio-notes §17 item 15
  ("Showcase + house-radio retrofit pending"), STATUS if desired.

## The sibling task (separate, don't conflate)

The OTHER `filter()` payoff is **retrofitting `house` radio's fake per-frame filter-ride onto the
real master `filter()`** — read `house.c`'s `ride`/`curCut` curve (it re-aims voice filters every
frame) and point it at one `filter(FILTER_LOW, curCut, res)` instead. That's a careful A/B upgrade
of a flagship station, tracked separately from this showcase cart.
```
```
_Engine ref: `runtime/sound.h` `filter_process`/`fx_set_filter` (the TPT SVF), `studio.h` `filter()`
+ `FX_FILTER`. Shipped in commit `2ca2c08`._
